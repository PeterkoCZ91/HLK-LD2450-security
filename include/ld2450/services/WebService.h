#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "ld2450/types.h"
#include "ld2450/utils/credential_check.h"
#include "ld2450/utils/upload_guard.h"
#include "ld2450/services/MQTTService.h"
#include "ld2450/services/LD2450Service.h"
#include "ld2450/services/SecurityMonitor.h"
#include "ld2450/services/EventLog.h"

class WebService {
public:
    WebService();

    void begin(AppContext* ctx, AsyncWebServer* server);

    // SSE: send event to all connected clients
    void sendSSE(const String& event, const String& data);

    bool requireAuth(AsyncWebServerRequest *r);
    // No-challenge credential check for use inside upload/body callbacks (SEC-01).
    bool checkAuth(AsyncWebServerRequest *r);
    // SEC-06: CSRF defense — for state-changing methods, reject cross-origin browser
    // requests by validating Origin/Referer against Host. No side effects.
    bool checkCsrf(AsyncWebServerRequest *r);

private:
    AppContext* _ctx;
    AsyncWebServer* _server;
    AsyncEventSource* _events = nullptr;
    // True while a Web OTA upload is being written; enforces one active OTA at a time.
    bool _otaActive = false;
    // millis() of the last written OTA chunk; a stale (aborted-connection) session
    // is reclaimable after OTA_STALE_MS so a dropped upload cannot lock out OTA.
    unsigned long _otaLastActivityMs = 0;
    static constexpr unsigned long OTA_STALE_MS = 30000UL;
    // SEC-07: one concurrent config-import / region-filter body upload at a time.
    ld2450::UploadSlot _uploadSlot;
    static constexpr uint32_t UPLOAD_STALE_MS = 15000UL;
    static constexpr size_t IMPORT_MAX_BODY    = 4096;              // JSON payload cap
    static constexpr size_t IMPORT_MAX_CONTENT = IMPORT_MAX_BODY + 1024;  // + multipart framing
    static constexpr size_t REGION_MAX_BODY    = 2048;

    bool validateInt(AsyncWebServerRequest *r, const char* param, int min, int max, int &out);

    void setupRoot();
    void setupTelemetry();
    void setupSSE();
    void setupSecurity();
    void setupConfig();
    void setupNoiseMap();
    void setupBlackoutZones();
    void setupPolygons();
    void setupSystem();
    void setupNetwork();
    void setupSchedule();
    void setupMisc();
};
