// Schedule + Misc routes (split from WebService.cpp)
//   POST /api/schedule          — denní arm/disarm + auto-arm minuty
//   GET  /api/schedule
//   POST /api/tripwire          — virtuální čára pro entry/exit count
//   POST /api/blackout/update   — toggle blackout zone enabled
//   POST /api/polygon/add_current — přidat aktuální target jako bod polygonu
//   GET  /api/config/export
//   POST /api/config/import     — multipart upload, race-free per-request buffer

#include "ld2450/services/WebService.h"
#include "ld2450/services/ConfigManager.h"

extern ConfigManager configManager;

void WebService::setupSchedule() {
    // GET /api/schedule
    _server->on("/api/schedule", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        JsonDocument doc;
        doc["arm_time"] = _ctx->preferences->getString("sched_arm", "");
        doc["disarm_time"] = _ctx->preferences->getString("sched_disarm", "");
        doc["auto_arm_minutes"] = _ctx->preferences->getUShort("auto_arm_min", 0);
        doc["night_start"] = _ctx->preferences->getString("night_start", "");
        doc["night_end"] = _ctx->preferences->getString("night_end", "");
        doc["current_profile"] = (_ctx->currentProfile && *(_ctx->currentProfile) == 0x02) ? "night" : "day";
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    // POST /api/schedule
    _server->on("/api/schedule", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        bool changed = false;
        if (r->hasParam("arm_time", true)) {
            _ctx->preferences->putString("sched_arm", r->getParam("arm_time", true)->value());
            strncpy(configManager.getConfig().sched_arm_time, r->getParam("arm_time", true)->value().c_str(), 5);
            configManager.getConfig().sched_arm_time[5] = '\0';
            changed = true;
        }
        if (r->hasParam("disarm_time", true)) {
            _ctx->preferences->putString("sched_disarm", r->getParam("disarm_time", true)->value());
            strncpy(configManager.getConfig().sched_disarm_time, r->getParam("disarm_time", true)->value().c_str(), 5);
            configManager.getConfig().sched_disarm_time[5] = '\0';
            changed = true;
        }
        if (r->hasParam("auto_arm_minutes", true)) {
            uint16_t val = r->getParam("auto_arm_minutes", true)->value().toInt();
            _ctx->preferences->putUShort("auto_arm_min", val);
            configManager.getConfig().auto_arm_minutes = val;
            changed = true;
        }
        if (r->hasParam("night_start", true)) {
            String v = r->getParam("night_start", true)->value();
            _ctx->preferences->putString("night_start", v);
            strncpy(configManager.getConfig().night_start_time, v.c_str(), 5);
            configManager.getConfig().night_start_time[5] = '\0';
            changed = true;
        }
        if (r->hasParam("night_end", true)) {
            String v = r->getParam("night_end", true)->value();
            _ctx->preferences->putString("night_end", v);
            strncpy(configManager.getConfig().night_end_time, v.c_str(), 5);
            configManager.getConfig().night_end_time[5] = '\0';
            changed = true;
        }
        r->send(200, "text/plain", changed ? "Schedule saved." : "No params");
    });

    // POST /api/tripwire
    _server->on("/api/tripwire", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!_ctx->tripwire) { r->send(503); return; }
        if (r->hasParam("enabled", true))
            _ctx->tripwire->enabled = r->getParam("enabled", true)->value().toInt() != 0;
        if (r->hasParam("y", true))
            _ctx->tripwire->y = r->getParam("y", true)->value().toInt();
        if (r->hasParam("reset", true)) {
            _ctx->tripwire->entryCount = 0;
            _ctx->tripwire->exitCount = 0;
        }
        r->send(200, "text/plain", "OK");
    });
}

void WebService::setupMisc() {
    // POST /api/blackout/update  (toggle enabled and/or set day/night mask for a zone)
    _server->on("/api/blackout/update", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!r->hasParam("id", true)) { r->send(400, "text/plain", "Missing id"); return; }
        uint8_t id = (uint8_t)r->getParam("id", true)->value().toInt();
        if (id >= *_ctx->blackoutZoneCount) { r->send(404, "text/plain", "Zone not found"); return; }
        bool toggled = false;
        const char* msg = "OK";
        if (r->hasParam("enabled", true)) {
            bool en = (r->getParam("enabled", true)->value() == "true");
            _ctx->blackoutZones[id].enabled = en;
            msg = en ? "Enabled" : "Disabled";
            toggled = true;
        }
        if (r->hasParam("mask", true) && _ctx->blackoutMasks) {
            uint8_t m = (uint8_t)(r->getParam("mask", true)->value().toInt() & 0x03);
            if (m == 0) m = 0x03;
            _ctx->blackoutMasks[id] = m;
            msg = "Mask saved";
            toggled = true;
        }
        if (!toggled) {
            // Legacy behavior: bare /api/blackout/update with only id flips enabled
            bool en = !_ctx->blackoutZones[id].enabled;
            _ctx->blackoutZones[id].enabled = en;
            msg = en ? "Enabled" : "Disabled";
        }
        if (_ctx->saveBlackoutZones) _ctx->saveBlackoutZones();
        r->send(200, "text/plain", msg);
    });

    // POST /api/polygon/mask  (set day/night mask without touching points)
    _server->on("/api/polygon/mask", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!r->hasParam("id", true) || !r->hasParam("mask", true)) {
            r->send(400, "text/plain", "Missing id/mask"); return;
        }
        uint8_t id = (uint8_t)r->getParam("id", true)->value().toInt();
        if (id >= MAX_POLYGONS) { r->send(400, "text/plain", "Invalid id"); return; }
        if (_ctx->polygonMasks) {
            uint8_t m = (uint8_t)(r->getParam("mask", true)->value().toInt() & 0x03);
            if (m == 0) m = 0x03;
            _ctx->polygonMasks[id] = m;
        }
        if (_ctx->savePolygons) _ctx->savePolygons();
        r->send(200, "text/plain", "Mask saved");
    });

    // POST /api/polygon/add_current  (add current target position as polygon point)
    _server->on("/api/polygon/add_current", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!r->hasParam("id", true)) { r->send(400, "text/plain", "Missing id"); return; }
        uint8_t polyId = (uint8_t)r->getParam("id", true)->value().toInt();
        if (polyId >= MAX_POLYGONS) { r->send(400, "text/plain", "Invalid polygon id"); return; }

        int16_t px = 0, py = 0;
        bool found = false;
        if (_ctx->dataMutex && xSemaphoreTake(_ctx->dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            for (int i = 0; i < 3 && !found; i++) {
                if (_ctx->targetHistory->wasValid[i]) {
                    px = (int16_t)_ctx->targetHistory->smoothX[i];
                    py = (int16_t)_ctx->targetHistory->smoothY[i];
                    found = true;
                }
            }
            xSemaphoreGive(_ctx->dataMutex);
        }
        if (!found) { r->send(400, "text/plain", "No active target"); return; }

        PolygonZone& poly = _ctx->polygons[polyId];
        if (poly.pointCount >= MAX_POLY_POINTS) { r->send(400, "text/plain", "Polygon full"); return; }
        poly.points[poly.pointCount++] = {px, py};
        poly.enabled = (poly.pointCount >= 3);

        uint8_t maxUsed = 0;
        for (uint8_t i = 0; i < MAX_POLYGONS; i++) {
            if (_ctx->polygons[i].pointCount >= 3) maxUsed = i + 1;
        }
        *(_ctx->polyCount) = maxUsed;
        if (_ctx->savePolygons) _ctx->savePolygons();
        r->send(200, "text/plain", (String("Point added: ") + px + "," + py).c_str());
    });

    // GET /api/config/export
    _server->on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        JsonDocument doc;

        JsonObject zone = doc["zone"].to<JsonObject>();
        zone["xmin"]          = _ctx->preferences->getInt("z_x_min",       _ctx->zoneConfig->xMin);
        zone["xmax"]          = _ctx->preferences->getInt("z_x_max",       _ctx->zoneConfig->xMax);
        zone["ymin"]          = _ctx->preferences->getInt("z_y_min",       _ctx->zoneConfig->yMin);
        zone["ymax"]          = _ctx->preferences->getInt("z_y_max",       _ctx->zoneConfig->yMax);
        zone["min_res"]       = _ctx->preferences->getInt("min_res",        _ctx->zoneConfig->minRes);
        zone["ghost_timeout"] = _ctx->preferences->getInt("ghost_timeout",  _ctx->zoneConfig->ghostTimeout);
        zone["move_threshold"]= _ctx->preferences->getInt("move_threshold", _ctx->zoneConfig->moveThreshold);
        zone["pos_threshold"] = _ctx->preferences->getInt("pos_threshold",  _ctx->zoneConfig->posThreshold);
        zone["map_rotation"]  = _ctx->preferences->getInt("map_rotation",   _ctx->zoneConfig->mapRotation);

        JsonObject sec = doc["security"].to<JsonObject>();
        sec["antimask_time"]    = _ctx->preferences->getInt("antimask_time",    300);
        sec["antimask_enabled"] = _ctx->preferences->getBool("antimask_en",     false);
        sec["loiter_ms"]        = _ctx->preferences->getULong("loiter_ms",      DEFAULT_LOITER_MS);
        sec["loiter_alert"]     = _ctx->preferences->getBool("loiter_alert",    false);
        sec["heartbeat_ms"]     = _ctx->preferences->getULong("heartbeat_ms",   DEFAULT_HEARTBEAT_MS);
        sec["entry_delay_ms"]   = _ctx->preferences->getULong("entry_delay_ms", DEFAULT_ENTRY_DELAY_MS);
        sec["exit_delay_ms"]    = _ctx->preferences->getULong("exit_delay_ms",  DEFAULT_EXIT_DELAY_MS);

        JsonObject mqtt = doc["mqtt"].to<JsonObject>();
        String mqttPort = _ctx->preferences->getString("mqtt_port", "1883");
        mqtt["server"] = _ctx->preferences->getString("mqtt_server", "");
        mqtt["port"]   = mqttPort;
        mqtt["user"]   = _ctx->preferences->getString("mqtt_user",   "");
        mqtt["enabled"] = _ctx->preferences->getBool("mqtt_en", true);
#ifdef MQTTS_ENABLED
        mqtt["tls"] = _ctx->preferences->isKey("mqtt_tls")
            ? _ctx->preferences->getBool("mqtt_tls", true)
            : (mqttPort.toInt() == MQTTS_PORT);
#else
        mqtt["tls"] = false;
#endif

        JsonObject tg = doc["telegram"].to<JsonObject>();
        tg["chat_id"] = _ctx->preferences->getString("tg_chat", "");

        AsyncResponseStream *resp = r->beginResponseStream("application/json");
        resp->addHeader("Content-Disposition", "attachment; filename=ld2450_config.json");
        serializeJson(doc, *resp);
        r->send(resp);
    });

    // POST /api/config/import  (multipart file upload)
    // Per-request buffer (r->_tempObject) — předtím static String == race condition mezi
    // souběžnými uploady (AsyncWebServer hostí víc requestů paralelně).
    _server->on("/api/config/import", HTTP_POST,
        [this](AsyncWebServerRequest *r) {
            if (!requireAuth(r)) return;
            String* importBuf = static_cast<String*>(r->_tempObject);
            if (!importBuf || importBuf->length() == 0) {
                if (importBuf) { delete importBuf; r->_tempObject = nullptr; }
                r->send(400, "text/plain", "No data uploaded");
                return;
            }
            JsonDocument doc;
            DeserializationError derr = deserializeJson(doc, *importBuf);
            delete importBuf;
            r->_tempObject = nullptr;
            if (derr != DeserializationError::Ok) {
                r->send(400, "text/plain", "Error: invalid JSON");
                return;
            }
            if (doc["zone"].is<JsonObject>()) {
                JsonObject z = doc["zone"];
                if (z["xmin"].is<int>())           _ctx->preferences->putInt("z_x_min",       z["xmin"].as<int>());
                if (z["xmax"].is<int>())           _ctx->preferences->putInt("z_x_max",       z["xmax"].as<int>());
                if (z["ymin"].is<int>())           _ctx->preferences->putInt("z_y_min",       z["ymin"].as<int>());
                if (z["ymax"].is<int>())           _ctx->preferences->putInt("z_y_max",       z["ymax"].as<int>());
                if (z["min_res"].is<int>())        _ctx->preferences->putInt("min_res",        z["min_res"].as<int>());
                if (z["ghost_timeout"].is<int>())  _ctx->preferences->putInt("ghost_timeout",  z["ghost_timeout"].as<int>());
                if (z["move_threshold"].is<int>()) _ctx->preferences->putInt("move_threshold", z["move_threshold"].as<int>());
                if (z["pos_threshold"].is<int>())  _ctx->preferences->putInt("pos_threshold",  z["pos_threshold"].as<int>());
                if (z["map_rotation"].is<int>())   _ctx->preferences->putInt("map_rotation",   z["map_rotation"].as<int>());
            }
            if (doc["security"].is<JsonObject>()) {
                JsonObject s = doc["security"];
                if (s["antimask_time"].is<int>())      _ctx->preferences->putInt("antimask_time",    s["antimask_time"].as<int>());
                if (s["antimask_enabled"].is<bool>())  _ctx->preferences->putBool("antimask_en",     s["antimask_enabled"].as<bool>());
                if (s["loiter_ms"].is<long>())         _ctx->preferences->putULong("loiter_ms",      s["loiter_ms"].as<unsigned long>());
                if (s["loiter_alert"].is<bool>())      _ctx->preferences->putBool("loiter_alert",    s["loiter_alert"].as<bool>());
                if (s["heartbeat_ms"].is<long>())      _ctx->preferences->putULong("heartbeat_ms",   s["heartbeat_ms"].as<unsigned long>());
                if (s["entry_delay_ms"].is<long>())    _ctx->preferences->putULong("entry_delay_ms", s["entry_delay_ms"].as<unsigned long>());
                if (s["exit_delay_ms"].is<long>())     _ctx->preferences->putULong("exit_delay_ms",  s["exit_delay_ms"].as<unsigned long>());
            }
            if (doc["mqtt"].is<JsonObject>()) {
                JsonObject m = doc["mqtt"];
                if (m["server"].is<const char*>()) _ctx->preferences->putString("mqtt_server", m["server"].as<const char*>());
                if (m["port"].is<const char*>())   _ctx->preferences->putString("mqtt_port",   m["port"].as<const char*>());
                if (m["user"].is<const char*>())   _ctx->preferences->putString("mqtt_user",   m["user"].as<const char*>());
                if (m["pass"].is<const char*>())   _ctx->preferences->putString("mqtt_pass",   m["pass"].as<const char*>());
                if (m["enabled"].is<bool>())        _ctx->preferences->putBool("mqtt_en",      m["enabled"].as<bool>());
                if (m["tls"].is<bool>())            _ctx->preferences->putBool("mqtt_tls",     m["tls"].as<bool>());
            }
            if (doc["telegram"].is<JsonObject>()) {
                JsonObject t = doc["telegram"];
                if (t["chat_id"].is<const char*>()) _ctx->preferences->putString("tg_chat", t["chat_id"].as<const char*>());
            }
            r->send(200, "text/plain", "Config imported. Restart device to apply.");
        },
        [](AsyncWebServerRequest *r, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (index == 0) {
                // Nový upload — uvolni případný předchozí buffer (request reuse)
                if (r->_tempObject) {
                    delete static_cast<String*>(r->_tempObject);
                    r->_tempObject = nullptr;
                }
                r->_tempObject = new (std::nothrow) String();
                if (!r->_tempObject) return;
                static_cast<String*>(r->_tempObject)->reserve(2048);
            }
            String* buf = static_cast<String*>(r->_tempObject);
            if (!buf) return;
            if (buf->length() + len > 4096) return; // DoS prevention
            buf->concat((const char*)data, len);
        }
    );
}
