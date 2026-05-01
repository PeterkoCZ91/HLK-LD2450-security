#include "ld2450/services/WebService.h"
#include "ld2450/services/TelegramService.h"
#include "ld2450/services/ConfigManager.h"
#include <time.h>
#include "ld2450/web_interface.h"

extern void safeRestart(const char* reason);
extern ConfigManager configManager;

WebService::WebService() {
    // Constructor
}

void WebService::begin(AppContext* ctx, AsyncWebServer* server) {
    _ctx = ctx;
    _server = server;

    setupRoot();
    setupSSE();
    setupTelemetry();
    setupSecurity();
    setupNoiseMap();
    setupConfig();
    setupSystem();
    setupBlackoutZones();
    setupPolygons();
    setupNetwork();
    setupSchedule();
    setupMisc();

    _server->begin();
}

void WebService::setupSSE() {
    _events = new AsyncEventSource("/events");
    _events->onConnect([](AsyncEventSourceClient *client) {
        client->send("connected", "status", millis());
    });
    // Vyžaduj basic-auth (LAB_MODE skipne, viz níže)
#if LAB_MODE != 1
    if (strlen(_ctx->authUser) > 0 && strlen(_ctx->authPass) > 0) {
        _events->setAuthentication(_ctx->authUser, _ctx->authPass, AsyncAuthType::AUTH_BASIC);
    } else {
        _events->setAuthentication("admin", "admin", AsyncAuthType::AUTH_BASIC);
    }
#endif
    _server->addHandler(_events);
}

void WebService::sendSSE(const String& event, const String& data) {
    if (_events) {
        _events->send(data.c_str(), event.c_str(), millis());
    }
}


bool WebService::requireAuth(AsyncWebServerRequest *r) {
    #if LAB_MODE == 1
      return true; // Skip in LAB mode
    #endif
    
    if (strlen(_ctx->authUser) == 0 || strlen(_ctx->authPass) == 0) {
      Serial.println("[Web] WARNING: Empty credentials — using default auth");
      if (!r->authenticate("admin", "admin")) {
        r->requestAuthentication();
        return false;
      }
      return true;
    }
    
    if (!r->authenticate(_ctx->authUser, _ctx->authPass)) {
      r->requestAuthentication();
      return false;
    }
    return true;
}

bool WebService::validateInt(AsyncWebServerRequest *r, const char* param, int min, int max, int &out) {
    if (r->hasParam(param, true)) {
        int val = r->getParam(param, true)->value().toInt();
        if (val >= min && val <= max) {
            out = val;
            return true;
        } else {
            Serial.printf("[Config] ⚠️ Invalid value for %s: %d (Range: %d to %d)\n", param, val, min, max);
        }
    }
    return false;
}

void WebService::setupRoot() {
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *r){ if (!requireAuth(r)) return;
        AsyncWebServerResponse *response = r->beginResponse(200, "text/html", (const uint8_t*)index_html, sizeof(index_html)-1);
        r->send(response);
    });
    _server->on("/api/version", HTTP_GET, [this](AsyncWebServerRequest *r){ if (!requireAuth(r)) return; r->send(200, "text/plain", FW_VERSION); });
}



