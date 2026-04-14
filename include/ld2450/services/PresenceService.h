#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "ld2450/types.h"
#include "services/MQTTService.h"
#include "services/LD2450Service.h"
#include "services/SecurityMonitor.h"
#include "services/EventLog.h"
#include "services/BluetoothService.h"

class PresenceService {
public:
    PresenceService();
    void begin(AppContext* ctx);
    void update();

private:
    AppContext* _ctx;

    // State Variables
    unsigned long _lastTelemetryTime = 0;
    unsigned long _lastWiFiCheckTime = 0;
    unsigned long _lastMqttSuccess = 0;
    unsigned long _bootTime = 0;
    bool _mqttEverConnected = false;
    uint8_t _dmsRestartCount = 0;
    unsigned long _lastRSSICheck = 0;
    unsigned long _holdStartTime = 0;
    unsigned long _wifiLostSince = 0;
    
    PresenceState _currentState = PresenceState::IDLE;
    TelemetryState _tCache;

    // Constants
    const unsigned long WIFI_RECONNECT_INTERVAL = 10000;
    const unsigned long HEARTBEAT_INTERVAL = 60000;
    const unsigned long HEALTH_CHECK_INTERVAL = 3600000; // 1 hour
    const uint16_t MERGED_RESOLUTION_THRESHOLD = 350;
    const float SMOOTHING_ALPHA = 0.4;
    const int16_t MQTT_MOVE_THRESHOLD = 50;
    const uint16_t TAMPER_DISTANCE_THRESHOLD_MM = 500;
    const uint16_t NOISE_MARGIN_MM = 50;
    const int16_t ZONE_HYSTERESIS_MM = 200;  // Target stays "in zone" until 200mm outside
    const int GRID_OFFSET = 4000;
    const int GRID_CELL_SIZE = 100;
    const int PANIC_LED_PIN = 2;
    const uint16_t HOLD_TIMEOUT_MS = 3000;
    
    // Helpers
    int toGridX(int16_t x);
    int toGridY(int16_t y);
    bool isPointInPolygon(int16_t x, int16_t y, const PolygonZone& poly);
    bool isInBlackoutZone(int16_t x, int16_t y);
    void applyRotation(int16_t& x, int16_t& y);
    
    // Logic Blocks
    void processNoiseLearning(unsigned long now);
    void checkWiFi(unsigned long now);
    void performHealthCheck(unsigned long now);
    void checkMqttWatchdog(unsigned long now);
    void checkRSSIAnomaly(unsigned long now);
    void processRadarData(unsigned long now);
    void updateAdaptiveFilter(unsigned long now);

    
    // Sub-logic for Radar Data
    void handleTamperDetection(uint8_t validCount, bool* currentTargetValid, unsigned long now);
    void updateStateMachine(uint8_t validCount, unsigned long now);
    void publishTelemetry(uint8_t validCount, bool* currentTargetValid, bool stateChanged, unsigned long now);
    void updateVariance(int targetIdx, int16_t x, int16_t y);
    
    // Persistence
    void loadNoiseMap();
    void saveNoiseMap();
    void loadBlackoutZones();
    // saveBlackoutZones is handled via callback in AppContext
};
