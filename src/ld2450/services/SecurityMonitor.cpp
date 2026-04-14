#include "services/SecurityMonitor.h"
#include "services/MQTTService.h"
#include "services/EventLog.h"

SecurityMonitor::SecurityMonitor() {}

void SecurityMonitor::begin(MQTTService* mqttService, EventLog* eventLog, Preferences* prefs) {
    _mqttService = mqttService;
    _eventLog = eventLog;
    _prefs = prefs;
    _lastRSSI = WiFi.RSSI();
    _baselineRSSI = _lastRSSI;
    _startTime = millis();
    Serial.printf("[SecMon] Initialized. Baseline RSSI: %ld dBm\n", _baselineRSSI);
}

void SecurityMonitor::update() {
    unsigned long now = millis();

    // Health check every minute
    if (now - _lastHealthCheck > INTERVAL_HEALTH_CHECK_MS) {
        _lastHealthCheck = now;
        checkSystemHealth();
    }

    // Exit delay: ARMING -> ARMED
    if (_alarmState == SecurityState::ARMING && now - _exitDelayStart >= _exitDelay) {
        _alarmState = SecurityState::ARMED;
        Serial.println("[SecMon] ARMED (exit delay expired)");
        triggerAlert(EVT_SECURITY, "ARMED - exit delay completed");
    }

    // TRIGGERED timeout -> auto-silence
    if (_alarmState == SecurityState::TRIGGERED && _triggerTimeout > 0 && now - _triggerStartTime >= _triggerTimeout) {
        deactivateSiren();
        if (_autoRearm) {
            _alarmState = SecurityState::ARMED;
            triggerAlert(EVT_SECURITY, "Alarm auto-silenced, re-armed");
        } else {
            _alarmState = SecurityState::DISARMED;
            triggerAlert(EVT_SECURITY, "Alarm auto-silenced, disarmed");
        }
    }

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
            _mqttService->publish(_mqttService->getTopics().alarm_state, getAlarmStateStr(), true);
            lastPublished = _alarmState;
            _forceRepublish = false;
        }
    }
}

void SecurityMonitor::setArmed(bool armed, bool immediate, bool homeMode) {
    unsigned long now = millis();
    if (armed) {
        // Re-arm guard: reject if alarm is in PENDING or TRIGGERED state
        if (_alarmState == SecurityState::PENDING || _alarmState == SecurityState::TRIGGERED) {
            Serial.println("[SecMon] Re-arm rejected: alarm active (PENDING/TRIGGERED)");
            return;
        }
        _homeMode = homeMode;
        if (immediate) {
            _alarmState = SecurityState::ARMED;
            triggerAlert(EVT_SECURITY, homeMode ? "ARMED HOME (immediate)" : "ARMED AWAY (immediate)");
        } else {
            _alarmState = SecurityState::ARMING;
            _exitDelayStart = now;
            triggerAlert(EVT_SECURITY, homeMode ? "ARMING HOME - exit delay started" : "ARMING AWAY - exit delay started");
        }
        _lastPresenceWhileDisarmed = 0;
        _presenceWhileDisarmedStart = 0;
        _lastDisarmReminder = 0;
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
            triggerAlert(EVT_SECURITY, "DISARMED");
        }
    }
    if (_prefs) {
        _prefs->putBool("sec_armed", armed);
        _prefs->putBool("sec_home", homeMode && armed);
    }
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
    if (_alarmState == SecurityState::ARMED && targetCount > 0 && !_homeMode) {
        _alarmState = SecurityState::PENDING;
        _entryDelayStart = now;
        triggerAlert(EVT_SECURITY, "ENTRY DETECTED - entry delay started");
    }
    else if (_alarmState == SecurityState::PENDING && now - _entryDelayStart >= _entryDelay) {
        _alarmState = SecurityState::TRIGGERED;
        _triggerStartTime = now;

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
        activateSiren();
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
        if (now - _lastWiFiAnomalyAlert > COOLDOWN_WIFI_ANOMALY_MS) {
            triggerAlert(EVT_WIFI, "RSSI drop detected");
            _lastWiFiAnomalyAlert = now;
            _lastEvent.wifi_jamming_detected = true;
            _lastEvent.last_event_time = now;
        }
    }

    if (currentRSSI < _rssiThreshold) {
        if (_lowRssiStartTime == 0) _lowRssiStartTime = now;
        if (now - _lowRssiStartTime > TIMEOUT_LOW_RSSI_SUSTAINED_MS) {
            if (!_lastEvent.low_rssi && now - _lastWiFiAnomalyAlert > COOLDOWN_WIFI_ANOMALY_MS) {
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
        if (now - _lastTamperAlert > COOLDOWN_TAMPER_ALERT_MS) {
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
        if (now - _lastRadarAlert > COOLDOWN_RADAR_ALERT_MS) {
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

    // Certificate check (once per day)
    static unsigned long lastCertCheck = 0;
    unsigned long now = millis();
    if (_mqttService && _mqttService->connected() && (now - lastCertCheck > INTERVAL_CERT_CHECK_MS)) {
        _mqttService->checkCertificateExpiry();
        lastCertCheck = now;
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
    e.timestamp = millis() / 1000;
    e.x = t.x;
    e.y = t.y;
    e.speed = t.speed;
    e.targetIdx = targetIdx;
    e.targetCount = totalCount;

    struct tm ti;
    if (getLocalTime(&ti, 0)) {
        strftime(e.isoTime, sizeof(e.isoTime), "%Y-%m-%dT%H:%M:%S", &ti);
    } else {
        e.isoTime[0] = '\0';
    }

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
