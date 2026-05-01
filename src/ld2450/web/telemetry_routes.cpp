// Telemetry / diagnostics / radar debug routes (split from WebService.cpp)
//   GET /api/telemetry      — main snapshot pro UI: targets, zone config, polygons, blackout, tripwire, noise
//   GET /api/diagnostics    — heap, RSSI, uptime, mqtt status, radar health/fps
//   GET /api/debug/radar    — raw radar slots + history (debug only)
//
// /api/telemetry pracuje s mutex-chráněným snapshotem TargetHistory/NoiseMap/etc.,
// JSON build je mimo lock (dataMutex držet co nejkratší dobu).

#include "ld2450/services/WebService.h"

void WebService::setupTelemetry() {
    _server->on("/api/telemetry", HTTP_GET, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;

      // Safety: Don't try to build JSON if memory is critical
      if (ESP.getMaxAllocHeap() < 4096) {
           r->send(503, "text/plain", "Low Memory");
           return;
      }

      // Local snapshot buffers
      struct {
          uint8_t targetCount;
          LD2450Target targets[3];
          TargetHistory history;
          bool isGhost[3];
          bool noiseLearning;
          bool noisePending;
          uint32_t noiseSamples;
          unsigned long noiseStart;
          unsigned long noiseLearnStart;
          bool tamper;
          char tamperReason[64];
          bool useNoiseFilter;
          bool adaptiveEnabled;
      } snapshot;

      // Try to take lock with short timeout (e.g., 50ms)
      if (!_ctx->dataMutex || xSemaphoreTake(_ctx->dataMutex, 50 / portTICK_PERIOD_MS) != pdTRUE) {
           r->send(503, "text/plain", "System Busy (Lock)");
           return;
      }

      // COPY DATA (Fast, under lock)
      snapshot.targetCount = _ctx->radar->getTargetCount();
      for(int i=0; i<3; i++) {
          snapshot.targets[i] = _ctx->radar->getTarget(i);
          snapshot.history.smoothX[i] = _ctx->targetHistory->smoothX[i];
          snapshot.history.smoothY[i] = _ctx->targetHistory->smoothY[i];
          snapshot.history.wasValid[i] = _ctx->targetHistory->wasValid[i];
          snapshot.history.variance[i] = _ctx->targetHistory->variance[i];
          snapshot.isGhost[i] = _ctx->ghostTracker->isGhost[i];
      }
      snapshot.noiseLearning = _ctx->noiseMap->learning;
      snapshot.noisePending = _ctx->noiseMap->pending;
      snapshot.noiseSamples = _ctx->noiseMap->samples;
      snapshot.noiseStart = _ctx->noiseMap->startTime;
      snapshot.noiseLearnStart = _ctx->noiseMap->learningStartTs;
      snapshot.tamper = _ctx->tamperState->tamperDetected;
      if(snapshot.tamper) { strncpy(snapshot.tamperReason, _ctx->tamperState->tamperReason, 63); snapshot.tamperReason[63] = '\0'; }
      snapshot.useNoiseFilter = *(_ctx->useNoiseFilter);
      snapshot.adaptiveEnabled = _ctx->adaptiveConfig->enabled;

      // Release Lock ASAP
      xSemaphoreGive(_ctx->dataMutex);

      // BUILD JSON (Slow, unlocked)
      JsonDocument doc;
      doc["rssi"] = WiFi.RSSI();
      doc["uptime"] = millis() / 1000;
      doc["hostname"] = _ctx->deviceHostname;

      uint8_t validCount = 0;
      for(int i=0; i<3; i++) {
          if(snapshot.history.wasValid[i]) validCount++;
      }

      doc["count"] = validCount;
      doc["tamper"] = snapshot.tamper ? snapshot.tamperReason : "OK";
      doc["raw_count"] = snapshot.targetCount;
      doc["noise_filter"] = snapshot.useNoiseFilter;
      doc["noise_learning"] = snapshot.noiseLearning;
      doc["noise_pending"] = snapshot.noisePending;
      doc["adaptive_enabled"] = snapshot.adaptiveEnabled;

      if (snapshot.noisePending) {
          long remaining = (long)(snapshot.noiseLearnStart - millis()) / 1000;
          doc["noise_countdown"] = (remaining > 0) ? remaining : 0;
      }
      if (snapshot.noiseLearning) {
          doc["noise_samples"] = snapshot.noiseSamples;
          doc["noise_elapsed"] = (millis() - snapshot.noiseStart) / 1000;
      }

      JsonArray targets = doc["targets"].to<JsonArray>();
      for(int i=0; i<3; i++) {
          bool exists = (snapshot.targets[i].x != 0 || snapshot.targets[i].y != 0);

          if(exists) {
              JsonObject tObj = targets.add<JsonObject>();
              tObj["x"] = (int)snapshot.history.smoothX[i];
              tObj["y"] = (int)snapshot.history.smoothY[i];
              tObj["spd"] = snapshot.targets[i].speed;
              tObj["res"] = snapshot.targets[i].resolution;
              tObj["variance"] = snapshot.history.variance[i];

              if (snapshot.history.wasValid[i]) {
                  tObj["type"] = "human";
              } else {
                  tObj["type"] = "ghost";
              }
              // Analytics per target
              if (_ctx->analytics) {
                  tObj["move"] = moveClassStr(_ctx->analytics->moveClass[i]);
                  if (_ctx->analytics->dwellMs[i] > 0)
                      tObj["dwell"] = _ctx->analytics->dwellMs[i] / 1000;
              }
          }
      }

      // Entry/Exit counter
      if (_ctx->tripwire) {
          doc["tripwire_y"] = _ctx->tripwire->y;
          doc["tripwire_en"] = _ctx->tripwire->enabled;
          doc["entry_count"] = _ctx->tripwire->entryCount;
          doc["exit_count"] = _ctx->tripwire->exitCount;
      }

      // Static Config (Read directly, assume infrequent changes)
      JsonObject zone = doc["zone"].to<JsonObject>();
      zone["xmin"] = _ctx->zoneConfig->xMin;
      zone["xmax"] = _ctx->zoneConfig->xMax;
      zone["ymin"] = _ctx->zoneConfig->yMin;
      zone["ymax"] = _ctx->zoneConfig->yMax;
      zone["min_res"] = _ctx->zoneConfig->minRes;
      zone["ghost_timeout"] = _ctx->zoneConfig->ghostTimeout;
      zone["move_threshold"] = _ctx->zoneConfig->moveThreshold;
      zone["pos_threshold"] = _ctx->zoneConfig->posThreshold;
      zone["map_rotation"] = _ctx->zoneConfig->mapRotation;

      JsonArray bzones = doc["blackout_zones"].to<JsonArray>();
      for(uint8_t i=0; i<*(_ctx->blackoutZoneCount); i++) {
          JsonObject bz = bzones.add<JsonObject>();
          bz["id"] = i;
          bz["xmin"] = _ctx->blackoutZones[i].xMin;
          bz["xmax"] = _ctx->blackoutZones[i].xMax;
          bz["ymin"] = _ctx->blackoutZones[i].yMin;
          bz["ymax"] = _ctx->blackoutZones[i].yMax;
          bz["label"] = _ctx->blackoutZones[i].label;
          bz["enabled"] = _ctx->blackoutZones[i].enabled;
          bz["mask"] = _ctx->blackoutMasks ? _ctx->blackoutMasks[i] : 0x03;
      }

      JsonArray polyZones = doc["polygons"].to<JsonArray>();
      for(uint8_t i=0; i<*(_ctx->polyCount); i++) {
          if(_ctx->polygons[i].enabled) {
              JsonObject p = polyZones.add<JsonObject>();
              p["id"] = i;
              p["label"] = _ctx->polygons[i].label;
              p["mask"] = _ctx->polygonMasks ? _ctx->polygonMasks[i] : 0x03;
              JsonArray pts = p["points"].to<JsonArray>();
              for(uint8_t j=0; j<_ctx->polygons[i].pointCount; j++) {
                  JsonObject pt = pts.add<JsonObject>();
                  pt["x"] = _ctx->polygons[i].points[j].x;
                  pt["y"] = _ctx->polygons[i].points[j].y;
              }
          }
      }

      // Active day/night profile state
      doc["profile"] = (_ctx->currentProfile && *(_ctx->currentProfile) == 0x02) ? "night" : "day";

      AsyncResponseStream *response = r->beginResponseStream("application/json");
      serializeJson(doc, *response);
      r->send(response);
  });

  // Diagnostics
  _server->on("/api/diagnostics", HTTP_GET, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      JsonDocument doc;
      doc["free_heap"] = ESP.getFreeHeap();
      doc["wifi_rssi"] = WiFi.RSSI();
      doc["uptime"] = millis() / 1000;
      doc["mqtt_connected"] = _ctx->mqtt->connected();
      doc["mqtt_server"] = _ctx->mqtt->getServer();
      doc["mqtt_port"] = _ctx->mqtt->getPort();
      doc["mqtt_enabled"] = _ctx->mqtt->isEnabled();
      doc["mqtt_tls"] = _ctx->mqtt->isTlsEnabled();

      doc["ip_address"] = WiFi.localIP().toString();
      doc["mac_address"] = WiFi.macAddress();
      doc["device_id"] = _ctx->deviceId;
      doc["is_default_pass"] = (strcmp(_ctx->authUser, WEB_ADMIN_USER_DEFAULT) == 0 &&
                                strcmp(_ctx->authPass, WEB_ADMIN_PASS_DEFAULT) == 0);
      doc["auth_user"] = _ctx->authUser;

      if (_ctx->radar) {
          RadarHealth h = _ctx->radar->getHealth();
          if (h.framesTotal > 0) {
              doc["health_score"] = (int)(100.0f * h.framesGood / h.framesTotal);
          }
          doc["frame_rate"] = h.fps;
          doc["error_count"] = h.framesCorrupt + h.bufferOverflows;
          String rmac = _ctx->radar->getRadarMAC();
          if (rmac.length()) doc["radar_mac"] = rmac;
      }

      String res; serializeJson(doc, res);
      r->send(200, "application/json", res);
  });

    // Debug endpoint
  _server->on("/api/debug/radar", HTTP_GET, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;

      if (!_ctx->dataMutex || xSemaphoreTake(_ctx->dataMutex, 50 / portTICK_PERIOD_MS) != pdTRUE) {
           r->send(503, "text/plain", "System Busy (Lock)");
           return;
      }

      JsonDocument doc;
      doc["raw_target_count"] = _ctx->radar->getTargetCount();

      JsonArray rawTargets = doc["raw_targets"].to<JsonArray>();
      for(int i=0; i<3; i++) {
          LD2450Target t = _ctx->radar->getTarget(i);
          JsonObject tObj = rawTargets.add<JsonObject>();
          tObj["slot"] = i;
          tObj["valid"] = t.valid;
          tObj["x"] = t.x;
          tObj["y"] = t.y;
          tObj["speed"] = t.speed;
          tObj["resolution"] = t.resolution;
      }

      JsonArray history = doc["target_history"].to<JsonArray>();
      for(int i=0; i<3; i++) {
          JsonObject h = history.add<JsonObject>();
          h["slot"] = i;
          h["smoothX"] = (int)_ctx->targetHistory->smoothX[i];
          h["smoothY"] = (int)_ctx->targetHistory->smoothY[i];
          h["wasValid"] = _ctx->targetHistory->wasValid[i];
          h["lastSeen"] = _ctx->targetHistory->lastSeen[i];
          h["isGhost"] = _ctx->ghostTracker->isGhost[i];
      }

      xSemaphoreGive(_ctx->dataMutex);

      String res; serializeJson(doc, res);
      r->send(200, "application/json", res);
  });
}
