#include "services/SecurityMonitor.h"
#include "services/MQTTService.h"
#include "services/EventLog.h"
#include "ld2450/utils/timing.h"
#include "ld2450/utils/sched_edge.h"

SecurityMonitor::SecurityMonitor() {}

void SecurityMonitor::begin(MQTTService* mqttService, EventLog* eventLog, Preferences* prefs) {
    _mqttService = mqttService;
    _eventLog = eventLog;
    _prefs = prefs;
    _lastRSSI = WiFi.RSSI();
    _baselineRSSI = _lastRSSI;
    _startTime = millis();
    if (!_stateMutex) {
        _stateMutex = xSemaphoreCreateMutex();
        if (!_stateMutex) {
            Serial.println("[SecMon] FATAL: state mutex create failed");
        }
    }
    Serial.printf("[SecMon] Initialized. Baseline RSSI: %ld dBm\n", _baselineRSSI);
}

void SecurityMonitor::update() {
    unsigned long now = millis();

    // Health check every minute
    if (now - _lastHealthCheck > INTERVAL_HEALTH_CHECK_MS) {
        _lastHealthCheck = now;
        checkSystemHealth();
    }

    // Exit delay: ARMING -> ARMED  (protected by mutex against Telegram setArmed)
    const char* transitionMsg = nullptr;
    if (_stateMutex) xSemaphoreTake(_stateMutex, portMAX_DELAY);
    if (_alarmState == SecurityState::ARMING && ld2450_timing::elapsedAtLeast(now, _exitDelayStart, _exitDelay)) {
        _alarmState = SecurityState::ARMED;
        Serial.println("[SecMon] ARMED (exit delay expired)");
        transitionMsg = "ARMED - exit delay completed";
    }
    // TRIGGERED timeout -> auto-silence
    bool fireSilenceMsg = false;
    const char* silenceMsg = nullptr;
    if (_alarmState == SecurityState::TRIGGERED && _triggerTimeout > 0 && ld2450_timing::elapsedAtLeast(now, _triggerStartTime, _triggerTimeout)) {
        deactivateSiren();
        if (_autoRearm) {
            _alarmState = SecurityState::ARMED;
            silenceMsg = "Alarm auto-silenced, re-armed";
        } else {
            _alarmState = SecurityState::DISARMED;
            silenceMsg = "Alarm auto-silenced, disarmed";
        }
        fireSilenceMsg = true;
    }
    if (_stateMutex) xSemaphoreGive(_stateMutex);

    if (transitionMsg) triggerAlert(EVT_SECURITY, transitionMsg);
    if (fireSilenceMsg && silenceMsg) triggerAlert(EVT_SECURITY, silenceMsg);

    // Disarm reminder
    if (_alarmState == SecurityState::DISARMED && _disarmReminderEnabled && _lastPresenceWhileDisarmed > 0) {
        if (now - _lastDisarmReminder > 1800000) { // 30 min
            _lastDisarmReminder = now;
            triggerAlert(EVT_HEARTBEAT, "System DISARMED - presence detected");
        }
    }

    // Publish alarm state via MQTT
    if (_mqttService && _mqttService->connected()) {
        static SecurityState lastPublished = SecurityState::DISARMED;
        if (_alarmState != lastPublished || _forceRepublish) {
            // Mark as sent only on success so a dropped publish is retried.
            if (_mqttService->publish(_mqttService->getTopics().alarm_state, getAlarmStateStr(), true)) {
                lastPublished = _alarmState;
                _forceRepublish = false;
            }
        }
    }
}

void SecurityMonitor::setArmed(bool armed, bool immediate, bool homeMode) {
    unsigned long now = millis();

    // Mutex protects against cross-core races (Telegram task vs loop task).
    if (_stateMutex) xSemaphoreTake(_stateMutex, portMAX_DELAY);

    bool stateChanged = false;
    const char* alertMsg = nullptr;

    if (armed) {
        // Re-arm guard: reject if alarm is in PENDING or TRIGGERED state
        if (_alarmState == SecurityState::PENDING || _alarmState == SecurityState::TRIGGERED) {
            Serial.println("[SecMon] Re-arm rejected: alarm active (PENDING/TRIGGERED)");
            if (_stateMutex) xSemaphoreGive(_stateMutex);
            return;
        }
        _homeMode = homeMode;
        if (immediate) {
            _alarmState = SecurityState::ARMED;
            alertMsg = homeMode ? "ARMED HOME (immediate)" : "ARMED AWAY (immediate)";
        } else {
            _alarmState = SecurityState::ARMING;
            _exitDelayStart = now;
            alertMsg = homeMode ? "ARMING HOME - exit delay started" : "ARMING AWAY - exit delay started";
        }
        _lastPresenceWhileDisarmed = 0;
        _presenceWhileDisarmedStart = 0;
        _lastDisarmReminder = 0;
        stateChanged = true;
    } else {
        SecurityState prev = _alarmState;
        if (prev == SecurityState::TRIGGERED) deactivateSiren();
        _alarmState = SecurityState::DISARMED;
        _homeMode = false;
        _entryDelayStart = 0;
        _exitDelayStart = 0;
        _lastPresenceWhileDisarmed = 0;
        _presenceWhileDisarmedStart = 0;
        _lastDisarmReminder = 0;
        if (prev != SecurityState::DISARMED) {
            alertMsg = "DISARMED";
            stateChanged = true;
        }
    }

    // SECSTATE-01: persist INSIDE the state lock so the NVS write order can
    // never diverge from the in-RAM state mutation order. Persist the
    // resulting state (derived from this call), not a stale read.
    if (stateChanged && _prefs) {
        _prefs->putBool("sec_armed", armed);
        _prefs->putBool("sec_home", homeMode && armed);
    }

    if (_stateMutex) xSemaphoreGive(_stateMutex);

    // Network/notification work happens OUTSIDE the state lock (lock ordering:
    // never call into Telegram/MQTT while holding _stateMutex).
    if (alertMsg) triggerAlert(EVT_SECURITY, alertMsg);
}

bool SecurityMonitor::isArmed() const {
    return _alarmState == SecurityState::ARMED ||
           _alarmState == SecurityState::ARMING ||
           _alarmState == SecurityState::PENDING ||
           _alarmState == SecurityState::TRIGGERED;
}

const char* SecurityMonitor::getAlarmStateStr() const {
    switch (_alarmState) {
        case SecurityState::DISARMED:  return "disarmed";
        case SecurityState::ARMING:    return "arming";
        case SecurityState::ARMED:     return _homeMode ? "armed_home" : "armed_away";
        case SecurityState::PENDING:   return "pending";
        case SecurityState::TRIGGERED: return "triggered";
        default: return "disarmed";
    }
}

// LD2450-specific: process multi-target data
void SecurityMonitor::processTargets(uint8_t targetCount, const LD2450Target targets[3]) {
    unsigned long now = millis();

    // Loitering: any target within 2000mm (2m) and slow/stationary
    bool closeTarget = false;
    for (int i = 0; i < 3; i++) {
        if (targets[i].valid && targets[i].y < 2000 && targets[i].y > 0) {
            closeTarget = true;
            break;
        }
    }

    if (closeTarget) {
        if (_loiterStart == 0) _loiterStart = now;
        if (now - _loiterStart > _loiterThreshold && !_isLoitering) {
            _isLoitering = true;
            if (_loiterAlertEnabled) {
                triggerAlert(EVT_PRESENCE, "LOITERING: target <2m");
            }
        }
    } else {
        _loiterStart = 0;
        _isLoitering = false;
    }

    // Heartbeat
    if (_lastHeartbeat == 0) _lastHeartbeat = now;
    if (_heartbeatInterval > 0 && now - _lastHeartbeat > _heartbeatInterval) {
        _lastHeartbeat = now;
        unsigned long hours = millis() / MS_PER_HOUR;
        char buf[64];
        snprintf(buf, sizeof(buf), "Heartbeat: uptime %luh, RSSI %ld", hours, WiFi.RSSI());
        triggerAlert(EVT_HEARTBEAT, buf);
    }

    // Record approach log while armed
    if ((_alarmState == SecurityState::ARMED || _alarmState == SecurityState::PENDING) && targetCount > 0) {
        for (int i = 0; i < 3; i++) {
            if (targets[i].valid) {
                recordApproach(i, targets[i], targetCount);
            }
        }
    }

    // Armed logic (home mode ignores normal presence, only tamper triggers)
    if (_stateMutex) xSemaphoreTake(_stateMutex, portMAX_DELAY);
    bool fireEntry = false;
    bool fireTrigger = false;
    if (_alarmState == SecurityState::ARMED && targetCount > 0 && !_homeMode) {
        _alarmState = SecurityState::PENDING;
        _entryDelayStart = now;
        fireEntry = true;
    }
    else if (_alarmState == SecurityState::PENDING && ld2450_timing::elapsedAtLeast(now, _entryDelayStart, _entryDelay)) {
        _alarmState = SecurityState::TRIGGERED;
        _triggerStartTime = now;
        fireTrigger = true;
        // Siren must switch on in the same critical section as the state change,
        // otherwise a concurrent setArmed(false) could leave it stuck ON.
        activateSiren();
    }
    if (_stateMutex) xSemaphoreGive(_stateMutex);

    if (fireEntry) triggerAlert(EVT_SECURITY, "ENTRY DETECTED - entry delay started");
    if (fireTrigger) {

        // Build approach forensics summary
        String alertMsg = "ALARM TRIGGERED! Approach log:";
        uint8_t count = getApproachLogCount();
        uint8_t start = (_approachHead + APPROACH_LOG_SIZE - count) % APPROACH_LOG_SIZE;
        for (uint8_t i = 0; i < min((uint8_t)4, count); i++) {
            ApproachEntry& e = _approachLog[(start + count - 1 - i) % APPROACH_LOG_SIZE];
            char buf[80];
            snprintf(buf, sizeof(buf), " [T%d x=%d y=%d spd=%d @%lus]",
                     e.targetIdx, e.x, e.y, e.speed, (unsigned long)e.timestamp);
            alertMsg += buf;
        }
        triggerAlert(EVT_SECURITY, alertMsg.c_str());
    }

    // Track presence while disarmed (for reminder)
    if (_alarmState == SecurityState::DISARMED && targetCount > 0) {
        if (_presenceWhileDisarmedStart == 0) {
            _presenceWhileDisarmedStart = now;
        } else if (now - _presenceWhileDisarmedStart > 10000) {
            _lastPresenceWhileDisarmed = now;
        }
    } else if (targetCount == 0) {
        _presenceWhileDisarmedStart = 0;
    }
}

void SecurityMonitor::checkRSSIAnomaly(long currentRSSI) {
    unsigned long now = millis();

    if (!_rssiBaselineEstablished && (now - _startTime) > INTERVAL_RSSI_BASELINE_MS) {
        _baselineRSSI = currentRSSI;
        _rssiBaselineEstablished = true;
    }

    long rssiDelta = _lastRSSI - currentRSSI;
    if (rssiDelta > _rssiDropThreshold && _rssiBaselineEstablished) {
        if (ld2450_timing::cooldownElapsed(now, _lastWiFiAnomalyAlert, COOLDOWN_WIFI_ANOMALY_MS)) {
            triggerAlert(EVT_WIFI, "RSSI drop detected");
            _lastWiFiAnomalyAlert = now;
            _lastEvent.wifi_jamming_detected = true;
            _lastEvent.last_event_time = now;
        }
    }

    if (currentRSSI < _rssiThreshold) {
        if (_lowRssiStartTime == 0) _lowRssiStartTime = now;
        if (now - _lowRssiStartTime > TIMEOUT_LOW_RSSI_SUSTAINED_MS) {
            if (!_lastEvent.low_rssi && ld2450_timing::cooldownElapsed(now, _lastWiFiAnomalyAlert, COOLDOWN_WIFI_ANOMALY_MS)) {
                triggerAlert(EVT_WIFI, "WiFi signal unstable (sustained)");
                _lastWiFiAnomalyAlert = now;
                _lastEvent.low_rssi = true;
            }
        }
    } else {
        _lowRssiStartTime = 0;
        _lastEvent.low_rssi = false;
    }

    _lastRSSI = currentRSSI;
}

void SecurityMonitor::checkTamperState(bool isTamper) {
    unsigned long now = millis();

    if (isTamper && !_lastTamperState) {
        _tamperStartTime = now;
        if (ld2450_timing::cooldownElapsed(now, _lastTamperAlert, COOLDOWN_TAMPER_ALERT_MS)) {
            triggerAlert(EVT_TAMPER, "TAMPER detected!");
            _lastTamperAlert = now;
            _lastEvent.tamper_detected = true;
            _lastEvent.last_event_time = now;
        }
    }

    if (!isTamper && _lastTamperState) {
        _lastEvent.tamper_detected = false;
    }

    _lastTamperState = isTamper;
}

void SecurityMonitor::checkRadarHealth(bool isConnected) {
    unsigned long now = millis();

    if (!isConnected && _lastRadarConnected) {
        _radarDisconnectedTime = now;
    }

    if (!isConnected && (now - _radarDisconnectedTime > TIMEOUT_RADAR_DISCONNECT_MS)) {
        if (ld2450_timing::cooldownElapsed(now, _lastRadarAlert, COOLDOWN_RADAR_ALERT_MS)) {
            triggerAlert(EVT_SYSTEM, "Radar connection lost");
            _lastRadarAlert = now;
            _lastEvent.radar_disconnected = true;
        }
    }

    if (isConnected && !_lastRadarConnected) {
        _lastEvent.radar_disconnected = false;
    }

    _lastRadarConnected = isConnected;
}

void SecurityMonitor::checkSystemHealth() {
    bool healthy = true;

    if (WiFi.status() != WL_CONNECTED) healthy = false;
    if (WiFi.RSSI() < _rssiThreshold) healthy = false;

    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < HEAP_LOW_WARNING) {
        healthy = false;
        if (!_systemHealthy) {
            triggerAlert(EVT_SYSTEM, "Low memory warning");
        }
    }

    // Certificate check: first run as soon as time is valid (non-blocking test,
    // so getLocalTime inside the check returns immediately), then once per day.
    static uint32_t lastCertCheck = 0;
    static bool certEverChecked = false;
    uint32_t now = millis();
    bool timeValid = time(nullptr) > 1700000000;
    if (_mqttService && _mqttService->connected() &&
        ld2450_sched::certCheckDue(now, lastCertCheck, certEverChecked, INTERVAL_CERT_CHECK_MS, timeValid)) {
        _mqttService->checkCertificateExpiry();
        lastCertCheck = now;
        certEverChecked = true;
    }

    _systemHealthy = healthy;
}

void SecurityMonitor::triggerAlert(uint8_t eventType, const String& message) {
    // Log to EventLog
    if (_eventLog) {
        _eventLog->addEvent(eventType, 0, 0, message.c_str());
    }

    // Publish to MQTT notification topic
    if (_mqttService && _mqttService->connected()) {
        _mqttService->publish(_mqttService->getTopics().notification, message.c_str(), false);
    }

    Serial.printf("[SecMon] Alert: %s\n", message.c_str());
}

// --- Approach Forensics ---

void SecurityMonitor::recordApproach(uint8_t targetIdx, const LD2450Target& t, uint8_t totalCount) {
    ApproachEntry& e = _approachLog[_approachHead];
    // If NTP is synchronized store epoch unix time, otherwise fall back to uptime seconds.
    // Same meaning as isoTime (both wall time, or both uptime).
    time_t epoch = time(nullptr);
    if (epoch > 1700000000) {
        e.timestamp = (uint32_t)epoch;
        struct tm ti;
        localtime_r(&epoch, &ti);
        strftime(e.isoTime, sizeof(e.isoTime), "%Y-%m-%dT%H:%M:%S", &ti);
    } else {
        e.timestamp = millis() / 1000;
        e.isoTime[0] = '\0';
    }
    e.x = t.x;
    e.y = t.y;
    e.speed = t.speed;
    e.targetIdx = targetIdx;
    e.targetCount = totalCount;

    _approachHead = (_approachHead + 1) % APPROACH_LOG_SIZE;
    if (_approachCount < APPROACH_LOG_SIZE) _approachCount++;
}

// --- Siren GPIO ---

void SecurityMonitor::setSirenPin(int8_t pin) {
    _sirenPin = pin;
    if (_sirenPin >= 0) {
        pinMode(_sirenPin, OUTPUT);
        digitalWrite(_sirenPin, LOW);
    }
}

void SecurityMonitor::activateSiren() {
    if (_sirenPin >= 0 && !_sirenActive) {
        digitalWrite(_sirenPin, HIGH);
        _sirenActive = true;
        Serial.printf("[SecMon] SIREN ON (GPIO %d)\n", _sirenPin);
    }
}

void SecurityMonitor::deactivateSiren() {
    if (_sirenPin >= 0 && _sirenActive) {
        digitalWrite(_sirenPin, LOW);
        _sirenActive = false;
        Serial.printf("[SecMon] SIREN OFF (GPIO %d)\n", _sirenPin);
    }
}
