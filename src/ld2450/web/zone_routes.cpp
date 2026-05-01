// Zone / noise / blackout / polygon routes (split from WebService.cpp)
//   POST /api/config           — zone bounds, ghost timeout, thresholds, map_rotation, hostname
//   POST /api/mqtt_config      — legacy MQTT param names (z UI v Network tab)
//   GET  /api/noisemap         — binary download učenné noise mapy
//   POST /api/noise/{toggle,start,stop}
//   POST /api/blackout/add, /api/blackout/delete
//   POST /api/polygon/set, /api/polygon/delete

#include "ld2450/services/WebService.h"
#include "ld2450/services/LD2450Service.h"
#include "ld2450/services/ConfigManager.h"

extern ConfigManager configManager;

void WebService::setupConfig() {
    _server->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      bool configChanged = false;
      int val = 0;

      // Use helper to validate integers SAFELY (no casting of 16-bit vars to int&)
      if (validateInt(r, "z_x_min", -4000, 4000, val)) {
          _ctx->zoneConfig->xMin = (int16_t)val;
          _ctx->preferences->putInt("z_x_min", _ctx->zoneConfig->xMin);
          configChanged = true;
      }
      if (validateInt(r, "z_x_max", -4000, 4000, val)) {
          _ctx->zoneConfig->xMax = (int16_t)val;
          _ctx->preferences->putInt("z_x_max", _ctx->zoneConfig->xMax);
          configChanged = true;
      }
      if (validateInt(r, "z_y_max", 0, 8000, val)) {
          _ctx->zoneConfig->yMax = (int16_t)val;
          _ctx->preferences->putInt("z_y_max", _ctx->zoneConfig->yMax);
          configChanged = true;
      }
      if (validateInt(r, "min_res", 0, 1000, val)) {
          _ctx->zoneConfig->minRes = (uint16_t)val;
          _ctx->preferences->putInt("min_res", _ctx->zoneConfig->minRes);
          configChanged = true;
      }
      if (validateInt(r, "ghost_timeout", 0, 300000, val)) {
          _ctx->zoneConfig->ghostTimeout = (uint32_t)val;
          _ctx->preferences->putInt("ghost_timeout", _ctx->zoneConfig->ghostTimeout);
          configChanged = true;
      }
      if (validateInt(r, "move_threshold", 0, 1000, val)) {
          _ctx->zoneConfig->moveThreshold = (uint16_t)val;
          _ctx->preferences->putInt("move_threshold", _ctx->zoneConfig->moveThreshold);
          configChanged = true;
      }
      if (validateInt(r, "pos_threshold", 0, 2000, val)) {
          _ctx->zoneConfig->posThreshold = (uint16_t)val;
          _ctx->preferences->putInt("pos_threshold", _ctx->zoneConfig->posThreshold);
          configChanged = true;
      }

      if (r->hasParam("map_rotation", true)) {
          val = r->getParam("map_rotation", true)->value().toInt();
          if (val == 0 || val == 90 || val == 180 || val == 270) {
               _ctx->zoneConfig->mapRotation = (int16_t)val;
               _ctx->preferences->putInt("map_rotation", _ctx->zoneConfig->mapRotation);
               configChanged = true;
          }
      }
      if (validateInt(r, "z_y_min", -8000, 8000, val)) {
          _ctx->zoneConfig->yMin = (int16_t)val;
          _ctx->preferences->putInt("z_y_min", _ctx->zoneConfig->yMin);
          configChanged = true;
      }

      if (r->hasParam("hostname", true)) {
          String host = r->getParam("hostname", true)->value();
          if (host.length() > 0 && host.length() < 32) {
            _ctx->preferences->putString("hostname", host);
          }
      }

      r->send(200, "text/plain", configChanged ? "OK - Config updated" : "OK");
  });

  // MQTT Config (legacy param names — POST /api/mqtt_config)
  _server->on("/api/mqtt_config", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      bool changed = false;

      if (r->hasParam("mqtt_server", true)) {
          _ctx->preferences->putString("mqtt_server", r->getParam("mqtt_server", true)->value());
          changed = true;
      }
      if (r->hasParam("mqtt_port", true)) {
          _ctx->preferences->putString("mqtt_port", r->getParam("mqtt_port", true)->value());
          changed = true;
      }
      if (r->hasParam("mqtt_user", true)) {
          _ctx->preferences->putString("mqtt_user", r->getParam("mqtt_user", true)->value());
          changed = true;
      }
      if (r->hasParam("mqtt_pass", true)) {
          _ctx->preferences->putString("mqtt_pass", r->getParam("mqtt_pass", true)->value());
          changed = true;
      }
      if (r->hasParam("mqtt_tls", true)) {
          _ctx->preferences->putBool("mqtt_tls", r->getParam("mqtt_tls", true)->value().toInt() != 0);
          changed = true;
      }

      if (changed) {
          r->send(200, "text/plain", "MQTT config saved. Restart device to apply.");
      } else {
          r->send(400, "text/plain", "No parameters provided");
      }
  });
}

void WebService::setupNoiseMap() {
   _server->on("/api/noisemap", HTTP_GET, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      if (!_ctx->noiseMap->active && !_ctx->noiseMap->learning) {
          r->send(404, "text/plain", "No noise map found");
          return;
      }

      // Copy under mutex to avoid torn reads
      static uint8_t nmBuf[sizeof(_ctx->noiseMap->energy)];
      if (_ctx->dataMutex && xSemaphoreTake(_ctx->dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          memcpy(nmBuf, _ctx->noiseMap->energy, sizeof(nmBuf));
          xSemaphoreGive(_ctx->dataMutex);
      }
      AsyncWebServerResponse *response = r->beginResponse(200, "application/octet-stream", nmBuf, sizeof(nmBuf));
      response->addHeader("Content-Disposition", "attachment; filename=noisemap.bin");
      r->send(response);
  });

  _server->on("/api/noise/toggle", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      *(_ctx->useNoiseFilter) = !*(_ctx->useNoiseFilter);
      r->send(200, "text/plain", *(_ctx->useNoiseFilter) ? "Filter ON" : "Filter OFF");
  });

  // Trigger learning (s 30s odložením, ať uživatel uteče ze zóny)
  _server->on("/api/noise/start", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      _ctx->noiseMap->pending = true;
      _ctx->noiseMap->learningStartTs = millis() + 30000; // 30s delay
      r->send(200, "text/plain", "Learning started");
  });

  _server->on("/api/noise/stop", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      if (_ctx->noiseMap->learning) {
          _ctx->noiseMap->learning = false;
          _ctx->noiseMap->active = true;
          *(_ctx->useNoiseFilter) = true;
          if (_ctx->saveNoiseMap) _ctx->saveNoiseMap();
          r->send(200, "text/plain", "Learning finished and saved");
      } else {
          r->send(400, "text/plain", "Learning not active");
      }
  });
}

void WebService::setupBlackoutZones() {
  _server->on("/api/blackout/add", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      if (*(_ctx->blackoutZoneCount) >= MAX_BLACKOUT_ZONES) {
          r->send(400, "text/plain", "Max blackout zones reached");
          return;
      }

      uint8_t idx = *(_ctx->blackoutZoneCount);
      BlackoutZone* zones = _ctx->blackoutZones;

      zones[idx].xMin = r->hasParam("xmin", true) ? r->getParam("xmin", true)->value().toInt() : 0;
      zones[idx].xMax = r->hasParam("xmax", true) ? r->getParam("xmax", true)->value().toInt() : 0;
      zones[idx].yMin = r->hasParam("ymin", true) ? r->getParam("ymin", true)->value().toInt() : 0;
      zones[idx].yMax = r->hasParam("ymax", true) ? r->getParam("ymax", true)->value().toInt() : 0;
      zones[idx].enabled = r->hasParam("enabled", true) ? (r->getParam("enabled", true)->value() == "true") : true;

      if (r->hasParam("label", true)) {
          strncpy(zones[idx].label, r->getParam("label", true)->value().c_str(), 31);
      } else {
          strncpy(zones[idx].label, "Blackout Zone", 31);
      }

      // Day/night profile mask (bit0=day, bit1=night). Default 0x03 = both.
      if (_ctx->blackoutMasks) {
          uint8_t m = 0x03;
          if (r->hasParam("mask", true)) {
              m = (uint8_t)(r->getParam("mask", true)->value().toInt() & 0x03);
              if (m == 0) m = 0x03;
          }
          _ctx->blackoutMasks[idx] = m;
      }

      (*_ctx->blackoutZoneCount)++;
      if(_ctx->saveBlackoutZones) _ctx->saveBlackoutZones();

      r->send(200, "text/plain", "Blackout zone added");
  });

  _server->on("/api/blackout/delete", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      if (!r->hasParam("id", true)) {
          r->send(400, "text/plain", "Missing zone id");
          return;
      }

      uint8_t id = r->getParam("id", true)->value().toInt();
      if (id >= *(_ctx->blackoutZoneCount)) {
          r->send(404, "text/plain", "Zone not found");
          return;
      }

      // Shift
      for (uint8_t i = id; i < *(_ctx->blackoutZoneCount) - 1; i++) {
          _ctx->blackoutZones[i] = _ctx->blackoutZones[i + 1];
          if (_ctx->blackoutMasks) _ctx->blackoutMasks[i] = _ctx->blackoutMasks[i + 1];
      }
      (*_ctx->blackoutZoneCount)--;

       if(_ctx->saveBlackoutZones) _ctx->saveBlackoutZones();

      r->send(200, "text/plain", "Blackout zone deleted");
  });
}

void WebService::setupPolygons() {
    // Set polygon (nahradí celý obsah jednoho slotu novými body)
    // Body: id=N, points="x1,y1;x2,y2;..." (max MAX_POLY_POINTS), label="...", enabled=0/1
    _server->on("/api/polygon/set", HTTP_POST, [this](AsyncWebServerRequest *r){
        if (!requireAuth(r)) return;
        if (!r->hasParam("id", true)) { r->send(400, "text/plain", "Missing id"); return; }
        uint8_t id = (uint8_t)r->getParam("id", true)->value().toInt();
        if (id >= MAX_POLYGONS) { r->send(400, "text/plain", "Invalid id"); return; }

        PolygonZone& poly = _ctx->polygons[id];
        poly.pointCount = 0;

        if (r->hasParam("points", true)) {
            String pts = r->getParam("points", true)->value();
            int from = 0;
            while (from < (int)pts.length() && poly.pointCount < MAX_POLY_POINTS) {
                int sep = pts.indexOf(';', from);
                String pair = (sep < 0) ? pts.substring(from) : pts.substring(from, sep);
                int comma = pair.indexOf(',');
                if (comma > 0) {
                    int16_t px = (int16_t)pair.substring(0, comma).toInt();
                    int16_t py = (int16_t)pair.substring(comma + 1).toInt();
                    poly.points[poly.pointCount++] = {px, py};
                }
                if (sep < 0) break;
                from = sep + 1;
            }
        }

        if (r->hasParam("label", true)) {
            strncpy(poly.label, r->getParam("label", true)->value().c_str(), sizeof(poly.label) - 1);
            poly.label[sizeof(poly.label) - 1] = '\0';
        }
        poly.enabled = r->hasParam("enabled", true)
                            ? (r->getParam("enabled", true)->value().toInt() != 0)
                            : (poly.pointCount >= 3);

        // Day/night profile mask (bit0=day, bit1=night). Default 0x03 = both.
        if (_ctx->polygonMasks) {
            uint8_t m = 0x03;
            if (r->hasParam("mask", true)) {
                m = (uint8_t)(r->getParam("mask", true)->value().toInt() & 0x03);
                if (m == 0) m = 0x03;
            }
            _ctx->polygonMasks[id] = m;
        }

        // Recalc polyCount jako horní mez slotů s ≥3 body
        uint8_t maxUsed = 0;
        for (uint8_t i = 0; i < MAX_POLYGONS; i++) {
            if (_ctx->polygons[i].pointCount >= 3) maxUsed = i + 1;
        }
        *(_ctx->polyCount) = maxUsed;

        if (_ctx->savePolygons) _ctx->savePolygons();
        r->send(200, "text/plain", "Polygon saved");
    });

    // Smazat polygon (vyprázdnit slot)
    _server->on("/api/polygon/delete", HTTP_POST, [this](AsyncWebServerRequest *r){
        if (!requireAuth(r)) return;
        if (!r->hasParam("id", true)) { r->send(400, "text/plain", "Missing id"); return; }
        uint8_t id = (uint8_t)r->getParam("id", true)->value().toInt();
        if (id >= MAX_POLYGONS) { r->send(400, "text/plain", "Invalid id"); return; }
        _ctx->polygons[id].pointCount = 0;
        _ctx->polygons[id].enabled = false;
        _ctx->polygons[id].label[0] = '\0';

        uint8_t maxUsed = 0;
        for (uint8_t i = 0; i < MAX_POLYGONS; i++) {
            if (_ctx->polygons[i].pointCount >= 3) maxUsed = i + 1;
        }
        *(_ctx->polyCount) = maxUsed;

        if (_ctx->savePolygons) _ctx->savePolygons();
        r->send(200, "text/plain", "Polygon deleted");
    });

    // -----------------------------------------------------------------------
    // Native LD2450 region filter (radar cmd 0xC1 read / 0xC2 write).
    // Hardware-side filter — cíle jsou odfiltrované přímo v modulu, ještě
    // než dorazí přes UART. Komplementární k SW polygonům (rectangulární jen).
    //
    //   GET  /api/zones/region_filter        — vrátí aktuální config (NVS)
    //   POST /api/zones/region_filter        — uloží + pushne do radaru
    //     body (JSON): {"mode":1,"zones":[{"x1":...,"y1":...,"x2":...,"y2":...}, ...]}
    //     nebo form-encoded:
    //       mode=0|1|2
    //       z0_x1, z0_y1, z0_x2, z0_y2,  z1_x1, ..., z2_y2   (int16 mm, ±10000)
    // -----------------------------------------------------------------------
    _server->on("/api/zones/region_filter", HTTP_GET, [this](AsyncWebServerRequest *r){
        if (!requireAuth(r)) return;
        const SystemConfig& cfg = configManager.getConfig();
        String json = "{\"mode\":";
        json += (int)cfg.region_filter_mode;
        json += ",\"zones\":[";
        for (uint8_t z = 0; z < 3; z++) {
            if (z) json += ',';
            json += "{\"x1\":";
            json += (int)cfg.region_filter_zones[z * 4 + 0];
            json += ",\"y1\":";
            json += (int)cfg.region_filter_zones[z * 4 + 1];
            json += ",\"x2\":";
            json += (int)cfg.region_filter_zones[z * 4 + 2];
            json += ",\"y2\":";
            json += (int)cfg.region_filter_zones[z * 4 + 3];
            json += '}';
        }
        json += "]}";
        r->send(200, "application/json", json);
    });

    // POST handler — primární cesta používá form params (konzistentní
    // s ostatními endpointy v tomto souboru). Volitelně přijímá i JSON body
    // (handler níž v `onBody` lambdě), který se zpracuje rovnou inline.
    auto applyRegionFilter = [this](AsyncWebServerRequest *r,
                                    int mode,
                                    const int16_t coords[12],
                                    bool* allValid) {
        if (mode < 0 || mode > 2) {
            if (allValid) *allValid = false;
            r->send(400, "text/plain", "Invalid mode (0-2)");
            return;
        }
        for (int i = 0; i < 12; i++) {
            if (coords[i] < -10000 || coords[i] > 10000) {
                if (allValid) *allValid = false;
                r->send(400, "text/plain", "Coord out of range (+/-10000mm)");
                return;
            }
        }

        SystemConfig& cfg = configManager.getConfig();
        cfg.region_filter_mode = (uint8_t)mode;
        memcpy(cfg.region_filter_zones, coords, sizeof(int16_t) * 12);
        configManager.save();

        if (_ctx && _ctx->radar) {
            LD2450Service::RegionFilter rf{};
            rf.mode = (uint8_t)mode;
            for (uint8_t z = 0; z < 3; z++) {
                rf.x1[z] = coords[z * 4 + 0];
                rf.y1[z] = coords[z * 4 + 1];
                rf.x2[z] = coords[z * 4 + 2];
                rf.y2[z] = coords[z * 4 + 3];
            }
            _ctx->radar->setRegionFilter(rf);
        }

        String resp = "{\"ok\":true,\"mode\":";
        resp += mode;
        resp += "}";
        r->send(200, "application/json", resp);
    };

    _server->on("/api/zones/region_filter", HTTP_POST,
        [this, applyRegionFilter](AsyncWebServerRequest *r){
            if (!requireAuth(r)) return;

            // 1) Pokud máme JSON body (přes onBody handler), zpracuj ho.
            String* bodyBuf = static_cast<String*>(r->_tempObject);
            if (bodyBuf && bodyBuf->length() > 0) {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, *bodyBuf);
                delete bodyBuf;
                r->_tempObject = nullptr;
                if (err != DeserializationError::Ok) {
                    r->send(400, "text/plain", "Invalid JSON");
                    return;
                }
                int mode = doc["mode"] | -1;
                int16_t coords[12] = {0};
                if (doc["zones"].is<JsonArray>()) {
                    JsonArray zones = doc["zones"].as<JsonArray>();
                    uint8_t z = 0;
                    for (JsonObject zone : zones) {
                        if (z >= 3) break;
                        coords[z * 4 + 0] = (int16_t)(zone["x1"] | 0);
                        coords[z * 4 + 1] = (int16_t)(zone["y1"] | 0);
                        coords[z * 4 + 2] = (int16_t)(zone["x2"] | 0);
                        coords[z * 4 + 3] = (int16_t)(zone["y2"] | 0);
                        z++;
                    }
                }
                applyRegionFilter(r, mode, coords, nullptr);
                return;
            }

            // 2) Fallback — form-encoded params.
            if (!r->hasParam("mode", true)) {
                r->send(400, "text/plain", "Missing mode");
                return;
            }
            int mode = r->getParam("mode", true)->value().toInt();
            int16_t coords[12] = {0};
            const char* keys[12] = {
                "z0_x1","z0_y1","z0_x2","z0_y2",
                "z1_x1","z1_y1","z1_x2","z1_y2",
                "z2_x1","z2_y1","z2_x2","z2_y2"
            };
            for (int i = 0; i < 12; i++) {
                if (r->hasParam(keys[i], true)) {
                    coords[i] = (int16_t)r->getParam(keys[i], true)->value().toInt();
                }
            }
            applyRegionFilter(r, mode, coords, nullptr);
        },
        nullptr,  // upload handler — není potřeba
        [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
            // onBody handler — agreguje JSON body do per-request bufferu.
            if (index == 0) {
                if (r->_tempObject) {
                    delete static_cast<String*>(r->_tempObject);
                    r->_tempObject = nullptr;
                }
                r->_tempObject = new (std::nothrow) String();
                if (!r->_tempObject) return;
                static_cast<String*>(r->_tempObject)->reserve(total > 0 && total < 1024 ? total : 256);
            }
            String* buf = static_cast<String*>(r->_tempObject);
            if (!buf) return;
            if (buf->length() + len > 2048) return;  // DoS prevention
            buf->concat((const char*)data, len);
        }
    );
}
