// Network/auth/integrations routes (split from WebService.cpp)
//   GET/POST /api/wifi/config        — backup SSID/password
//   GET/POST /api/telegram/config    — token (masked GET), chat_id, enabled
//   POST     /api/telegram/test      — send test message
//   POST     /api/auth/config        — basic auth credentials
//   POST     /api/mqtt/config        — moderní param names (server/port/user/pass/tls)

#include "ld2450/services/WebService.h"
#include "ld2450/services/TelegramService.h"
#include "ld2450/services/ConfigManager.h"

void WebService::setupNetwork() {
    // GET /api/wifi/config
    _server->on("/api/wifi/config", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        JsonDocument doc;
        doc["bk_ssid"] = _ctx->config->getConfig().backup_ssid;
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    // POST /api/wifi/config
    _server->on("/api/wifi/config", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        SystemConfig& cfg = _ctx->config->getConfig();
        if (r->hasParam("bk_ssid", true))
            strncpy(cfg.backup_ssid, r->getParam("bk_ssid", true)->value().c_str(), sizeof(cfg.backup_ssid) - 1);
        if (r->hasParam("bk_pass", true)) {
            String pass = r->getParam("bk_pass", true)->value();
            if (pass != "***")
                strncpy(cfg.backup_pass, pass.c_str(), sizeof(cfg.backup_pass) - 1);
        }
        _ctx->config->save();
        r->send(200, "text/plain", "WiFi backup config saved");
    });

    // GET /api/telegram/config — token je maskovaný (jen prefix)
    _server->on("/api/telegram/config", HTTP_GET, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        JsonDocument doc;
        String token = _ctx->preferences->getString("tg_token", "");
        String masked = (token.length() > 4) ? token.substring(0, 4) + "****" : token;
        doc["enabled"] = _ctx->preferences->getBool("tg_direct_en", false);
        doc["token"]   = masked;
        doc["chat_id"] = _ctx->preferences->getString("tg_chat", "");
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    // POST /api/telegram/config
    _server->on("/api/telegram/config", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        if (r->hasParam("enabled", true))
            _ctx->preferences->putBool("tg_direct_en", r->getParam("enabled", true)->value().toInt() != 0);
        if (r->hasParam("token", true)) {
            String tok = r->getParam("token", true)->value();
            if (!tok.endsWith("****"))  // skip masked placeholder
                _ctx->preferences->putString("tg_token", tok);
        }
        if (r->hasParam("chat_id", true))
            _ctx->preferences->putString("tg_chat", r->getParam("chat_id", true)->value());
        r->send(200, "text/plain", "Telegram config saved");
    });

    // POST /api/telegram/test
    _server->on("/api/telegram/test", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        JsonDocument doc;
        if (_ctx->telegram && _ctx->telegram->sendMessage("Test message from LD2450")) {
            doc["success"] = true;
        } else {
            doc["success"] = false;
            doc["error"]   = "Telegram not enabled or send failed";
        }
        String res; serializeJson(doc, res);
        r->send(200, "application/json", res);
    });

    // POST /api/auth/config — uloží user/pass do NVS, pak restartuje aby nový auth platil
    _server->on("/api/auth/config", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        SystemConfig& cfg = _ctx->config->getConfig();
        bool changed = false;
        if (r->hasParam("user", true)) {
            strncpy(cfg.auth_user, r->getParam("user", true)->value().c_str(), sizeof(cfg.auth_user) - 1);
            changed = true;
        }
        if (r->hasParam("pass", true)) {
            String pass = r->getParam("pass", true)->value();
            if (pass != "***") {
                strncpy(cfg.auth_pass, pass.c_str(), sizeof(cfg.auth_pass) - 1);
                changed = true;
            }
        }
        if (changed) {
            _ctx->config->save();
            r->send(200, "text/plain", "Auth config saved. Restarting...");
            if (_ctx->shouldReboot) *_ctx->shouldReboot = true;
        } else {
            r->send(400, "text/plain", "No parameters");
        }
    });

    // POST /api/mqtt/config  (alias s GUI param names: server/port/user/pass)
    _server->on("/api/mqtt/config", HTTP_POST, [this](AsyncWebServerRequest *r) {
        if (!requireAuth(r)) return;
        bool changed = false;
        if (r->hasParam("server", true)) { _ctx->preferences->putString("mqtt_server", r->getParam("server", true)->value()); changed = true; }
        if (r->hasParam("port",   true)) { _ctx->preferences->putString("mqtt_port",   r->getParam("port",   true)->value()); changed = true; }
        if (r->hasParam("user",   true)) { _ctx->preferences->putString("mqtt_user",   r->getParam("user",   true)->value()); changed = true; }
        if (r->hasParam("pass",   true)) {
            String pass = r->getParam("pass", true)->value();
            if (pass != "***") { _ctx->preferences->putString("mqtt_pass", pass); changed = true; }
        }
        if (r->hasParam("enabled",true)) { _ctx->preferences->putBool("mqtt_en", r->getParam("enabled", true)->value().toInt() != 0); changed = true; }
        if (r->hasParam("tls", true)) { _ctx->preferences->putBool("mqtt_tls", r->getParam("tls", true)->value().toInt() != 0); changed = true; }
        r->send(200, "text/plain", changed ? "MQTT config saved. Restart to apply." : "No params");
    });
}
