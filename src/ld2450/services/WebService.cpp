#include "ld2450/services/WebService.h"
#include "ld2450/services/TelegramService.h"
#include "ld2450/services/ConfigManager.h"
#include "ld2450/utils/csrf_check.h"
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
    // Require basic-auth on the SSE stream
    if (strlen(_ctx->authUser) > 0 && strlen(_ctx->authPass) > 0) {
        _events->setAuthentication(_ctx->authUser, _ctx->authPass, AsyncAuthType::AUTH_BASIC);
    } else {
        _events->setAuthentication("admin", "admin", AsyncAuthType::AUTH_BASIC);
    }
    _server->addHandler(_events);
}

void WebService::sendSSE(const String& event, const String& data) {
    if (_events) {
        _events->send(data.c_str(), event.c_str(), millis());
    }
}


// No-challenge credential check. Safe to call inside upload/body callbacks where
// sending an HTTP challenge is not possible — returns the decision without side
// effects so the caller can silently reject unauthorized chunks.
bool WebService::checkAuth(AsyncWebServerRequest *r) {
    if (strlen(_ctx->authUser) == 0 || strlen(_ctx->authPass) == 0) {
      return r->authenticate("admin", "admin");
    }
    return r->authenticate(_ctx->authUser, _ctx->authPass);
}

// SEC-06: reject cross-origin browser requests on state-changing methods. The
// decision itself lives in ld2450::csrfAllowed (host-testable); here we just pull
// the relevant headers out of the request.
bool WebService::checkCsrf(AsyncWebServerRequest *r) {
    WebRequestMethodComposite m = r->method();
    bool mutating = !(m == HTTP_GET || m == HTTP_HEAD || m == HTTP_OPTIONS);

    const AsyncWebHeader* hostHdr = r->getHeader("Host");
    const AsyncWebHeader* origin  = r->getHeader("Origin");
    const AsyncWebHeader* referer = r->getHeader("Referer");

    return ld2450::csrfAllowed(
        mutating,
        hostHdr ? hostHdr->value().c_str() : nullptr,
        origin  ? origin->value().c_str()  : nullptr,
        referer ? referer->value().c_str() : nullptr);
}

bool WebService::requireAuth(AsyncWebServerRequest *r) {
    if (!checkAuth(r)) {
        r->requestAuthentication();
        return false;
    }
    if (!checkCsrf(r)) {
        r->send(403, "text/plain", "CSRF check failed: cross-origin request blocked");
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



