#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "ld2450/types.h"
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

private:
    AppContext* _ctx;
    AsyncWebServer* _server;
    AsyncEventSource* _events = nullptr;

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
