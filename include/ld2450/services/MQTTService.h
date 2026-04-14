#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ld2450/types.h"
#include "ld2450/services/MQTTOfflineBuffer.h"
#include "secrets.h"

#ifdef MQTTS_ENABLED
#include <WiFiClientSecure.h>
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#endif

class MQTTService {
public:
    MQTTService();
    
    // Initialize with preferences and device info
    void begin(Preferences* prefs, const char* deviceId, NetworkQuality* netQuality);
    
    // Main loop handler
    void update();
    
    // Check if connected
    bool connected();
    
    // Generic publish
    bool publish(const char* topic, const char* payload, bool retained = false);
    
    // Specific logic methods
    void publishDiscovery();
    void publishHealth(const char* healthStatus, bool radarOk, bool wifiOk, bool heapOk, bool mqttOk);
    
    // Check certificate expiry (MQTTS only)
    void checkCertificateExpiry();

    // Accessors
    const Topics& getTopics() const { return _topics; }
    PubSubClient& getClient() { return _mqttClient; }
    const char* getServer() const { return _server; }
    const char* getPort() const { return _port; }
    
    // Callback setter wrapper
    void setCallback(MQTT_CALLBACK_SIGNATURE);

private:
    void setupClient();
    void connect();
    void generateTopics();
    String getResetReason();

    // Internal objects
    #ifdef MQTTS_ENABLED
    WiFiClientSecure _espClient;
    #else
    WiFiClient _espClient;
    #endif
    
    PubSubClient _mqttClient;
    Topics _topics;
    NetworkQuality* _netQuality;
    Preferences* _prefs;
    
    // Configuration
    char _server[40];
    char _port[6];
    char _user[32];
    char _pass[32];
    char _deviceId[32]; // Increased size just in case
    
    // State
    bool _bootMsgSent = false;
    unsigned long _lastReconnectAttempt = 0;
    unsigned long _lastSuccessfulPublish = 0;

    // Exponential backoff
    uint32_t _reconnectDelay = 5000;       // Start at 5s
    static const uint32_t MAX_RECONNECT_DELAY = 120000; // Max 2 min

    bool _justReconnected = false;
    MQTTOfflineBuffer _offlineBuffer;

public:
    unsigned long getLastSuccessfulPublish() const { return _lastSuccessfulPublish; }
    bool consumeReconnect() { bool r = _justReconnected; _justReconnected = false; return r; }
    MQTTOfflineBuffer& getOfflineBuffer() { return _offlineBuffer; }
};
