#ifndef SECURITY_MONITOR_H
#define SECURITY_MONITOR_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "ld2450/types.h"
#include "ld2450/constants.h"

// Forward declarations
class MQTTService;
class EventLog;

// Reuse SecurityState from types.h (DISARMED, ARMING, ARMED, PENDING, TRIGGERED)

struct SecurityEvent {
    bool tamper_detected = false;
    bool wifi_jamming_detected = false;
    bool radar_disconnected = false;
    bool low_rssi = false;
    unsigned long last_event_time = 0;
};

// Approach forensics: circular buffer of detections while armed
static const uint8_t APPROACH_LOG_SIZE = 16;
struct ApproachEntry {
    uint32_t timestamp;    // millis()/1000
    char isoTime[24];      // ISO 8601 if NTP synced
    int16_t x;             // mm
    int16_t y;             // mm
    int16_t speed;         // cm/s
    uint8_t targetIdx;     // which target slot (0-2)
    uint8_t targetCount;   // how many valid at that moment
};

class SecurityMonitor {
public:
    SecurityMonitor();

    void begin(MQTTService* mqttService, EventLog* eventLog, Preferences* prefs);
    void update();

    // LD2450-specific: process multi-target data
    void processTargets(uint8_t targetCount, const LD2450Target targets[3]);

    // Monitoring
    void checkRSSIAnomaly(long currentRSSI);
    void checkTamperState(bool isTamper);
    void checkRadarHealth(bool isConnected);
    void checkSystemHealth();

    // Armed/Disarmed
    void setArmed(bool armed, bool immediate = false, bool homeMode = false);
    bool isArmed() const;
    bool isHomeMode() const { return _homeMode; }
    SecurityState getAlarmState() const { return _alarmState; }
    const char* getAlarmStateStr() const;
    void setEntryDelay(unsigned long ms) { _entryDelay = ms; }
    void setExitDelay(unsigned long ms) { _exitDelay = ms; }
    unsigned long getEntryDelay() const { return _entryDelay; }
    unsigned long getExitDelay() const { return _exitDelay; }

    // Trigger timeout & auto-rearm
    void setTriggerTimeout(unsigned long ms) { _triggerTimeout = ms; }
    void setAutoRearm(bool en) { _autoRearm = en; }
    bool isAutoRearm() const { return _autoRearm; }

    // Siren GPIO
    void setSirenPin(int8_t pin);

    // Configuration
    void setLoiterTime(unsigned long ms) { _loiterThreshold = ms; }
    void setLoiterAlertEnabled(bool en) { _loiterAlertEnabled = en; }
    void setHeartbeatInterval(unsigned long ms) { _heartbeatInterval = ms; }
    void setRSSIThreshold(int threshold) { _rssiThreshold = threshold; }

    // Approach forensics
    const ApproachEntry* getApproachLog() const { return _approachLog; }
    uint8_t getApproachLogCount() const { return _approachCount < APPROACH_LOG_SIZE ? _approachCount : APPROACH_LOG_SIZE; }
    uint8_t getApproachLogHead() const { return _approachHead; }

    // Getters
    const SecurityEvent& getLastEvent() const { return _lastEvent; }
    bool isSystemHealthy() const { return _systemHealthy; }
    bool isLoitering() const { return _isLoitering; }
    unsigned long getLoiterTime() const { return _loiterThreshold; }
    bool isLoiterAlertEnabled() const { return _loiterAlertEnabled; }
    unsigned long getHeartbeatInterval() const { return _heartbeatInterval; }
    int getRSSIThreshold() const { return _rssiThreshold; }
    int getRSSIDropThreshold() const { return _rssiDropThreshold; }
    bool isDisarmReminderEnabled() const { return _disarmReminderEnabled; }
    void setRSSIDropThreshold(int t) { _rssiDropThreshold = t; }
    void setDisarmReminderEnabled(bool en) { _disarmReminderEnabled = en; }
    void forceRepublish() { _forceRepublish = true; }

private:
    void triggerAlert(uint8_t eventType, const String& message);
    void activateSiren();
    void deactivateSiren();

    MQTTService* _mqttService = nullptr;
    EventLog* _eventLog = nullptr;
    Preferences* _prefs = nullptr;

    // Mutex chrání alarm state mutace mezi loop tasem (Core 1) a Telegram taskem (Core 0).
    // Bez něj může setArmed() z Telegramu kolidovat s update() / mqttCallback z hlavní smyčky.
    SemaphoreHandle_t _stateMutex = nullptr;

    // Alarm state
    SecurityState _alarmState = SecurityState::DISARMED;
    bool _homeMode = false; // armed_home vs armed_away
    unsigned long _entryDelay = DEFAULT_ENTRY_DELAY_MS;
    unsigned long _exitDelay = DEFAULT_EXIT_DELAY_MS;
    unsigned long _exitDelayStart = 0;
    unsigned long _entryDelayStart = 0;
    unsigned long _triggerStartTime = 0;
    unsigned long _triggerTimeout = DEFAULT_TRIGGER_TIMEOUT_MS;
    bool _autoRearm = true;

    SecurityEvent _lastEvent;

    // RSSI monitoring
    long _lastRSSI = 0;
    long _baselineRSSI = 0;
    int _rssiThreshold = -80;
    int _rssiDropThreshold = 20;
    unsigned long _lowRssiStartTime = 0;
    unsigned long _startTime = 0;
    bool _rssiBaselineEstablished = false;

    // Tamper
    bool _lastTamperState = false;
    unsigned long _tamperStartTime = 0;

    // Radar health
    bool _lastRadarConnected = true;
    unsigned long _radarDisconnectedTime = 0;

    // System health
    bool _systemHealthy = true;
    unsigned long _lastHealthCheck = 0;

    // Alert cooldowns
    unsigned long _lastWiFiAnomalyAlert = 0;
    unsigned long _lastTamperAlert = 0;
    unsigned long _lastRadarAlert = 0;

    // Loitering (LD2450: target close and stationary)
    bool _isLoitering = false;
    bool _loiterAlertEnabled = false;
    unsigned long _loiterStart = 0;
    unsigned long _loiterThreshold = DEFAULT_LOITER_MS;

    // Heartbeat
    unsigned long _heartbeatInterval = DEFAULT_HEARTBEAT_MS;
    unsigned long _lastHeartbeat = 0;

    // Presence while disarmed
    unsigned long _lastPresenceWhileDisarmed = 0;
    unsigned long _presenceWhileDisarmedStart = 0;
    unsigned long _lastDisarmReminder = 0;
    bool _disarmReminderEnabled = true;
    bool _forceRepublish = false;

    // Siren
    int8_t _sirenPin = SIREN_PIN_DEFAULT;
    bool _sirenActive = false;

    // Approach forensics log
    ApproachEntry _approachLog[APPROACH_LOG_SIZE];
    uint8_t _approachHead = 0;
    uint16_t _approachCount = 0;
    void recordApproach(uint8_t targetIdx, const LD2450Target& t, uint8_t totalCount);
};

#endif
