// Security/alarm REST routes (split from WebService.cpp)
//   GET  /api/alarm/state
//   POST /api/alarm/arm
//   POST /api/alarm/disarm
//   GET  /api/events
//   POST /api/events/clear
//   GET  /api/security/config
//   POST /api/security/config
//   POST /api/alarm/config       (entry/exit delay, disarm reminder, sec_code)
//   GET  /api/alarm/code_status  (zda je nastaven PIN, neexponuje samotný kód)

#include "ld2450/services/WebService.h"

void WebService::setupSecurity() {
    // Get alarm state
    _server->on("/api/alarm/state", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!_ctx->security) { r->send(503, "text/plain", "SecurityMonitor not initialized"); return; }

        JsonDocument doc;
        doc["state"] = _ctx->security->getAlarmStateStr();
        doc["armed"] = _ctx->security->isArmed();
        doc["healthy"] = _ctx->security->isSystemHealthy();
        doc["loitering"] = _ctx->security->isLoitering();
        doc["entry_delay"] = _ctx->security->getEntryDelay() / 1000;
        doc["exit_delay"] = _ctx->security->getExitDelay() / 1000;
        doc["auto_rearm"] = _ctx->security->isAutoRearm();

        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    // Arm/Disarm
    _server->on("/api/alarm/arm", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!_ctx->security) { r->send(503, "text/plain", "N/A"); return; }
        bool immediate = r->hasParam("immediate", true);
        _ctx->security->setArmed(true, immediate);
        r->send(200, "text/plain", immediate ? "Armed (immediate)" : "Arming (exit delay)");
    });

    _server->on("/api/alarm/disarm", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!_ctx->security) { r->send(503, "text/plain", "N/A"); return; }
        _ctx->security->setArmed(false);
        r->send(200, "text/plain", "Disarmed");
    });

    // Event Log
    _server->on("/api/events", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (!_ctx->eventLog) { r->send(503, "text/plain", "EventLog not initialized"); return; }

        JsonDocument doc;
        _ctx->eventLog->getEventsJSON(doc);
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    _server->on("/api/events/clear", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (_ctx->eventLog) _ctx->eventLog->clear();
        r->send(200, "text/plain", "Events cleared");
    });

    // GET /api/security/config
    _server->on("/api/security/config", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        JsonDocument doc;
        doc["antimask_time"]    = _ctx->preferences->getInt("antimask_time", 300);
        doc["antimask_enabled"] = _ctx->preferences->getBool("antimask_en", false);
        if (_ctx->security) {
            doc["loiter_time"]        = _ctx->security->getLoiterTime() / 1000;
            doc["loiter_alert"]       = _ctx->security->isLoiterAlertEnabled();
            doc["heartbeat"]          = _ctx->security->getHeartbeatInterval() / 1000;
            doc["rssi_threshold"]     = _ctx->security->getRSSIThreshold();
            doc["rssi_drop_threshold"]= _ctx->security->getRSSIDropThreshold();
            doc["disarm_reminder"]    = _ctx->security->isDisarmReminderEnabled();
        }
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    // POST /api/security/config
    _server->on("/api/security/config", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (r->hasParam("antimask", true))
            _ctx->preferences->putInt("antimask_time", r->getParam("antimask", true)->value().toInt());
        if (r->hasParam("antimask_en", true))
            _ctx->preferences->putBool("antimask_en", r->getParam("antimask_en", true)->value().toInt() != 0);
        if (_ctx->security) {
            if (r->hasParam("loiter", true)) {
                unsigned long ms = (unsigned long)r->getParam("loiter", true)->value().toInt() * 1000UL;
                _ctx->security->setLoiterTime(ms);
                _ctx->preferences->putULong("loiter_ms", ms);
            }
            if (r->hasParam("loiter_alert", true)) {
                bool en = r->getParam("loiter_alert", true)->value().toInt() != 0;
                _ctx->security->setLoiterAlertEnabled(en);
                _ctx->preferences->putBool("loiter_alert", en);
            }
            if (r->hasParam("heartbeat", true)) {
                unsigned long ms = (unsigned long)r->getParam("heartbeat", true)->value().toInt() * 1000UL;
                _ctx->security->setHeartbeatInterval(ms);
                _ctx->preferences->putULong("heartbeat_ms", ms);
            }
        }
        r->send(200, "text/plain", "Security config saved");
    });

    // POST /api/alarm/config
    _server->on("/api/alarm/config", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (_ctx->security) {
            if (r->hasParam("entry_delay", true)) {
                unsigned long ms = (unsigned long)r->getParam("entry_delay", true)->value().toInt() * 1000UL;
                _ctx->security->setEntryDelay(ms);
                _ctx->preferences->putULong("entry_delay_ms", ms);
            }
            if (r->hasParam("exit_delay", true)) {
                unsigned long ms = (unsigned long)r->getParam("exit_delay", true)->value().toInt() * 1000UL;
                _ctx->security->setExitDelay(ms);
                _ctx->preferences->putULong("exit_delay_ms", ms);
            }
            if (r->hasParam("disarm_reminder", true)) {
                bool en = r->getParam("disarm_reminder", true)->value().toInt() != 0;
                _ctx->security->setDisarmReminderEnabled(en);
                _ctx->preferences->putBool("disarm_rem", en);
            }
        }
        // Volitelný PIN pro MQTT DISARM (empty = vypnout)
        if (r->hasParam("sec_code", true)) {
            String code = r->getParam("sec_code", true)->value();
            _ctx->preferences->putString("sec_code", code);
        }
        r->send(200, "text/plain", "Alarm config saved");
    });

    // GET /api/alarm/code_status — neexponovat samotný kód, jen jeho délku
    _server->on("/api/alarm/code_status", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        String code = _ctx->preferences->getString("sec_code", "");
        JsonDocument doc;
        doc["set"] = code.length() > 0;
        doc["len"] = (uint8_t)code.length();
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });
}
