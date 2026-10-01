#include "ld2450/services/MQTTService.h"
#include "ld2450/constants.h"

// CON-01: RAII lock for the recursive client mutex. No-op if the mutex was not
// created (extreme low-heap fallback) so behaviour degrades to the old racy
// path rather than crashing.
namespace {
struct ClientLock {
    SemaphoreHandle_t m;
    bool held;
    // wait: bounded for callers that must not stall behind a blocking connect().
    explicit ClientLock(SemaphoreHandle_t mtx, TickType_t wait = portMAX_DELAY)
        : m(mtx), held(false) {
        if (m) held = (xSemaphoreTakeRecursive(m, wait) == pdTRUE);
    }
    bool ok() const { return !m || held; }
    ~ClientLock() { if (m && held) xSemaphoreGiveRecursive(m); }
};
}

// Forward extern for CA cert if it's not in secrets.h (it IS in secrets.h usually)
// In main it was used as `mqtt_server_ca`. It comes from secrets.h.

MQTTService::MQTTService() : _mqttClient(_plainClient) {
    // Constructor
}

void MQTTService::begin(Preferences* prefs, const char* deviceId, NetworkQuality* netQuality) {
    if (!_clientMutex) _clientMutex = xSemaphoreCreateRecursiveMutex();
    _prefs = prefs;
    _netQuality = netQuality;
    strncpy(_deviceId, deviceId, 31);
    _deviceId[31] = '\0';
    
    // Load Config
    String s_server = _prefs->getString("mqtt_server", MQTT_SERVER_DEFAULT);
    String s_port = _prefs->getString("mqtt_port", "");
    String s_user = _prefs->getString("mqtt_user", MQTT_USER_DEFAULT);
    String s_pass = _prefs->getString("mqtt_pass", MQTT_PASS_DEFAULT);
    _enabled = _prefs->getBool("mqtt_en", true);
#ifdef MQTTS_ENABLED
    if (_prefs->isKey("mqtt_tls")) {
        _tlsEnabled = _prefs->getBool("mqtt_tls", true);
    } else {
        _tlsEnabled = s_port.toInt() == MQTTS_PORT;
    }
#else
    _tlsEnabled = false;
#endif

    s_server.toCharArray(_server, sizeof(_server));
    s_port.toCharArray(_port, 6);
    s_user.toCharArray(_user, sizeof(_user));
    s_pass.toCharArray(_pass, sizeof(_pass));
    
    generateTopics();
    setupClient();
    _offlineBuffer.begin();
}

void MQTTService::generateTopics() {
    // Generate Topics (UNIFIED naming) — use snprintf to prevent overflow
    snprintf(_topics.availability, sizeof(_topics.availability), "security/%s/availability", _deviceId);
    snprintf(_topics.boot, sizeof(_topics.boot), "security/%s/boot", _deviceId);
    snprintf(_topics.rssi, sizeof(_topics.rssi), "security/%s/rssi", _deviceId);
    snprintf(_topics.uptime, sizeof(_topics.uptime), "security/%s/uptime", _deviceId);
    snprintf(_topics.heap, sizeof(_topics.heap), "security/%s/heap", _deviceId);
    snprintf(_topics.max_alloc, sizeof(_topics.max_alloc), "security/%s/heap_max", _deviceId);
    snprintf(_topics.ip, sizeof(_topics.ip), "security/%s/ip", _deviceId);
    snprintf(_topics.reason, sizeof(_topics.reason), "security/%s/restart_reason", _deviceId);
    snprintf(_topics.health, sizeof(_topics.health), "security/%s/health", _deviceId);
    snprintf(_topics.tamper, sizeof(_topics.tamper), "security/%s/tamper", _deviceId);
    snprintf(_topics.netquality, sizeof(_topics.netquality), "security/%s/net_quality", _deviceId);

    // Application Topics (Device Specific)
    snprintf(_topics.presence_state, sizeof(_topics.presence_state), "security/%s/presence/state", _deviceId);
    snprintf(_topics.tracking_count, sizeof(_topics.tracking_count), "security/%s/tracking/count", _deviceId);
    snprintf(_topics.notification, sizeof(_topics.notification), "security/%s/presence/notification", _deviceId);

    for(int i=0; i<3; i++) {
        snprintf(_topics.target_x[i], sizeof(_topics.target_x[i]), "security/%s/tracking/target%d/x", _deviceId, i+1);
        snprintf(_topics.target_y[i], sizeof(_topics.target_y[i]), "security/%s/tracking/target%d/y", _deviceId, i+1);
    }

    // Security Topics
    snprintf(_topics.alarm_state, sizeof(_topics.alarm_state), "security/%s/alarm/state", _deviceId);
    snprintf(_topics.alarm_command, sizeof(_topics.alarm_command), "security/%s/alarm/command", _deviceId);

    // Analytics
    snprintf(_topics.entry_count, sizeof(_topics.entry_count), "security/%s/entry_count", _deviceId);
    snprintf(_topics.exit_count, sizeof(_topics.exit_count), "security/%s/exit_count", _deviceId);

    // Identity
    snprintf(_topics.radar_type, sizeof(_topics.radar_type), "security/%s/radar_type", _deviceId);
}


void MQTTService::setupClient() {
    if (!_enabled || strlen(_server) == 0) return;

    int portInt;
    if (strlen(_port) > 0) {
        portInt = atoi(_port);
    } else {
#ifdef MQTTS_ENABLED
        portInt = _tlsEnabled ? MQTTS_PORT : MQTT_PORT_DEFAULT;
#else
        portInt = MQTT_PORT_DEFAULT;
#endif
    }

    _mqttClient.setBufferSize(512); // Required for HA discovery payloads

#ifdef MQTTS_ENABLED
    if (_tlsEnabled) {
        // Validate the CA cert content: a placeholder/empty cert must not be accepted.
        bool caValid = (mqtt_server_ca != nullptr) &&
                       (strstr(mqtt_server_ca, "BEGIN CERTIFICATE") != nullptr) &&
                       (strlen(mqtt_server_ca) > 200);
        if (caValid) {
            _secureClient.setCACert(mqtt_server_ca);
            _mqttClient.setClient(_secureClient);
        } else {
            // SEC-05: fail closed. TLS was requested but no valid CA is configured.
            // Silently downgrading to plaintext would expose MQTT credentials and
            // remote alarm commands, so refuse to connect instead. connect() checks
            // _tlsMisconfigured and never opens a socket. The client pointer is still
            // set (to the secure client) so downstream connected()/publish() are safe.
            Serial.println("[MQTT] ERROR: TLS requested but CA missing/placeholder - refusing to connect (fail closed)");
            _tlsMisconfigured = true;
            _mqttClient.setClient(_secureClient);
        }
    } else {
        _mqttClient.setClient(_plainClient);
    }
#else
    _mqttClient.setClient(_plainClient);
#endif
    _mqttClient.setServer(_server, portInt);
}

void MQTTService::setCallback(MQTT_CALLBACK_SIGNATURE) {
    _mqttClient.setCallback(callback);
}

void MQTTService::update() {
    if (!_enabled || strlen(_server) == 0) return;

    ClientLock lock(_clientMutex);
    if (!_mqttClient.connected()) {
        connect();
    } else {
        _mqttClient.loop();
    }

    // Lazy persistence of the offline buffer (protects flash from wear when MQTT flaps)
    _offlineBuffer.update();
}

bool MQTTService::connected() {
    if (!_enabled) return false;
    ClientLock lock(_clientMutex, pdMS_TO_TICKS(200));
    if (!lock.ok()) return false;  // connect() in progress
    return _mqttClient.connected();
}

bool MQTTService::publish(const char* topic, const char* payload, bool retained) {
    if (!_enabled) {
        return false;
    }
    // Bounded wait: connect() holds the lock during a blocking (TLS) connect.
    // On timeout the message is dropped (buffer is protected by the same lock).
    ClientLock lock(_clientMutex, pdMS_TO_TICKS(200));
    if (!lock.ok()) return false;
    if (!_mqttClient.connected()) {
        _offlineBuffer.push(topic, payload, retained);  // MQTT-01: keep retained
        return false;
    }

    // Heap guard: skip publish if memory critically low
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < HEAP_MIN_FOR_PUBLISH) {
        Serial.printf("[MQTT] Heap guard: skip publish (%u < %u)\n", freeHeap, HEAP_MIN_FOR_PUBLISH);
        // Not sent: keep it (static buffer, no heap) so retained state gets replayed.
        _offlineBuffer.push(topic, payload, retained);
        return false;
    }
    if (freeHeap < HEAP_LOW_WARNING) {
        static unsigned long lastWarn = 0;
        if (millis() - lastWarn > 60000) {
            Serial.printf("[MQTT] Low heap warning: %u bytes\n", freeHeap);
            lastWarn = millis();
        }
    }

    bool ok = _mqttClient.publish(topic, payload, retained);
    if (ok) {
        _lastSuccessfulPublish = millis();
    } else {
        // MQTT-01: a transient connected-publish failure must not be lost;
        // buffer it (with retained) so it replays on the next reconnect.
        _offlineBuffer.push(topic, payload, retained);
    }
    return ok;
}

void MQTTService::connect() {
    if (!_enabled) return;

    // SEC-05: never open a socket when TLS is requested but misconfigured, as that
    // would leak credentials over plaintext. Rate-limited diagnostic only.
    if (_tlsMisconfigured) {
        static unsigned long lastMisconfigLog = 0;
        if (millis() - lastMisconfigLog > 60000) {
            Serial.println("[MQTT] TLS misconfigured (no valid CA) - connection refused. Fix mqtt_server_ca.");
            lastMisconfigLog = millis();
        }
        return;
    }

    // Exponential backoff
    unsigned long now = millis();
    if (now - _lastReconnectAttempt < _reconnectDelay) return;

    // Don't attempt if WiFi is down
    if (WiFi.status() != WL_CONNECTED) return;

    _lastReconnectAttempt = now;

    if (_netQuality) {
        _netQuality->mqttReconnects++;
        _netQuality->lastDisconnect = now;
    }

    String clientId = "ESP32-LD2450-" + String(random(0xffff), HEX);

    // Connect with LWT
    if (_mqttClient.connect(clientId.c_str(), _user, _pass, _topics.availability, 1, true, "offline")) {
        // Connected - reset backoff
        _reconnectDelay = 5000;
        _lastSuccessfulPublish = millis();
        _justReconnected = true;

        _mqttClient.publish(_topics.availability, "online", true);
        _mqttClient.publish(_topics.radar_type, "ld2450", true);
        _mqttClient.publish(_topics.tamper, "OK", true);

#ifdef ENABLE_MQTT_ALARM_COMMANDS
        // Subscribe to alarm command topic (remote ARM/DISARM). Disabled by default —
        // remote alarm control via MQTT is off until the HA integration is reworked.
        _mqttClient.subscribe(_topics.alarm_command);
#endif

        publishDiscovery();

        if (!_bootMsgSent) {
            _mqttClient.publish(_topics.boot, "REBOOT", false);
            _mqttClient.publish(_topics.reason, getResetReason().c_str(), true);
            _bootMsgSent = true;
        }

        _mqttClient.publish(_topics.ip, WiFi.localIP().toString().c_str(), true);

        // Replay offline buffer, preserving each record's retained flag (MQTT-01).
        char t[MQTT_OFB_TOPIC_LEN], p[MQTT_OFB_PAYLOAD_LEN];
        bool rtn = false;
        while (_offlineBuffer.hasMessages()) {
            if (_offlineBuffer.peek(t, sizeof(t), p, sizeof(p), &rtn)) {
                if (_mqttClient.publish(t, p, rtn)) {
                    _offlineBuffer.consume();
                } else break;
            } else break;
        }
        if (_offlineBuffer.count() == 0 && _offlineBuffer.hasMessages() == false) {
            Serial.println("[MQTT] Offline buffer replayed");
        }
    } else {
        // Failed - increase backoff (exponential, max 2 min)
        _reconnectDelay = min(_reconnectDelay * 2, MAX_RECONNECT_DELAY);
        Serial.printf("[MQTT] Connection failed. Next attempt in %lu s\n", _reconnectDelay / 1000);
    }
}

String MQTTService::getResetReason() {
    // Simplified version
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON: return "Power On";
        case ESP_RST_EXT: return "External Pin";
        case ESP_RST_SW: return "Software Reset";
        case ESP_RST_PANIC: return "Crash/Panic";
        case ESP_RST_INT_WDT: return "WDT Reset";
        case ESP_RST_TASK_WDT: return "Task WDT";
        case ESP_RST_WDT: return "Other WDT";
        case ESP_RST_DEEPSLEEP: return "Deep Sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO Reset";
        default: return "Unknown";
    }
}

void MQTTService::publishDiscovery() {
    ClientLock lock(_clientMutex);
    String baseSensor = "homeassistant/sensor/" + String(_deviceId);
    String baseBinary = "homeassistant/binary_sensor/" + String(_deviceId);

    // Helper: add device info to a JsonDocument
    auto addDev = [&](JsonDocument& doc) {
        JsonObject dev = doc["dev"].to<JsonObject>();
        dev["ids"] = _deviceId;
        dev["name"] = "LD2450 Security Node";
        dev["mdl"] = "LD2450";
        dev["mf"] = "DIY";
        dev["sw"] = FW_VERSION;
    };

    // Safe publish: if the payload does not fit the buffer, temporarily enlarge it.
    // PubSubClient::setBufferSize reallocates but does not keep the buffer during a
    // publish, so the enlargement must be done BEFOREHAND. After each publish the
    // buffer is restored to 512.
    auto safePublish = [&](const char* topic, const char* payload, bool retained) -> bool {
        size_t needed = strlen(topic) + strlen(payload) + 8; // 5B fixed header + slack
        uint16_t cur = _mqttClient.getBufferSize();
        bool resized = false;
        if (needed > cur) {
            if (!_mqttClient.setBufferSize((uint16_t)((needed + 63) & ~63u))) {
                Serial.printf("[MQTT] HA discovery: buffer resize to %u failed\n", (unsigned)needed);
                return false;
            }
            resized = true;
        }
        bool ok = _mqttClient.publish(topic, payload, retained);
        if (!ok) Serial.printf("[MQTT] HA discovery publish failed: %s (len=%u)\n", topic, (unsigned)strlen(payload));
        if (resized) _mqttClient.setBufferSize(512);
        return ok;
    };

    auto pub = [&](const char* name, const char* uid, const char* state_topic,
                    const char* unit, const char* dev_class, const char* icon,
                    bool isDiag, const String& base_topic,
                    const char* pl_on = nullptr, const char* pl_off = nullptr) {
        JsonDocument doc;
        doc["name"] = name;
        doc["uniq_id"] = String(_deviceId) + "_" + uid;
        doc["stat_t"] = state_topic;
        if(unit[0]) doc["unit_of_meas"] = unit;
        if(dev_class[0]) doc["dev_cla"] = dev_class;
        if(icon[0]) doc["ic"] = icon;
        if(isDiag) doc["ent_cat"] = "diagnostic";
        if(pl_on) doc["pl_on"] = pl_on;
        if(pl_off) doc["pl_off"] = pl_off;
        doc["avty_t"] = _topics.availability;
        addDev(doc);

        String p; serializeJson(doc, p);
        String configTopic = base_topic + "_" + uid + "/config";
        safePublish(configTopic.c_str(), p.c_str(), true);
    };

    // --- SENSORS ---
    pub("Presence State", "state", _topics.presence_state, "", "", "mdi:motion-sensor", false, baseSensor);
    pub("Tracking Count", "count", _topics.tracking_count, "", "", "mdi:counter", false, baseSensor);

    pub("WiFi Signal", "rssi", _topics.rssi, "dBm", "signal_strength", "", true, baseSensor);
    pub("Uptime", "uptime", _topics.uptime, "s", "duration", "", true, baseSensor);
    pub("Free Heap", "free_heap", _topics.heap, "B", "data_size", "", true, baseSensor);
    pub("IP Address", "ip", _topics.ip, "", "", "mdi:ip-network", true, baseSensor);
    pub("Tamper Status", "tamper", _topics.tamper, "", "", "mdi:shield-alert", false, baseSensor);

#ifdef ENABLE_MQTT_ALARM_COMMANDS
    // --- HA ALARM CONTROL PANEL ---
    // Only advertised when remote MQTT alarm control is enabled — otherwise HA would
    // show a control-panel entity whose ARM/DISARM commands are ignored.
    {
        JsonDocument doc;
        doc["name"] = "Security Alarm";
        doc["uniq_id"] = String(_deviceId) + "_alarm";
        doc["stat_t"] = _topics.alarm_state;
        doc["cmd_t"] = _topics.alarm_command;
        doc["avty_t"] = _topics.availability;
        // Supported features
        doc["sup_feat"][0] = "arm_home";
        doc["sup_feat"][1] = "arm_away";
        // Require code for disarm to prevent unauthorized MQTT disarm
        doc["code_arm_required"] = false;
        doc["code_disarm_required"] = true;
        addDev(doc);

        String p; serializeJson(doc, p);
        String configTopic = "homeassistant/alarm_control_panel/" + String(_deviceId) + "_alarm/config";
        safePublish(configTopic.c_str(), p.c_str(), true);
    }
#endif

    // --- BINARY SENSORS ---
    pub("Zone Occupancy", "occupancy", _topics.presence_state, "", "occupancy", "", false, baseBinary, "Detected", "Clear");

    // Targets X/Y (1-3)
    for(int i=0; i<3; i++) {
        char nameX[20], nameY[20], uidX[8], uidY[8];
        snprintf(nameX, sizeof(nameX), "Target %d X", i+1);
        snprintf(nameY, sizeof(nameY), "Target %d Y", i+1);
        snprintf(uidX, sizeof(uidX), "t%d_x", i+1);
        snprintf(uidY, sizeof(uidY), "t%d_y", i+1);
        pub(nameX, uidX, _topics.target_x[i], "mm", "distance", "", false, baseSensor);
        pub(nameY, uidY, _topics.target_y[i], "mm", "distance", "", false, baseSensor);
    }
}

void MQTTService::publishHealth(const char* healthStatus, bool radarOk, bool wifiOk, bool heapOk, bool mqttOk) {
    ClientLock lock(_clientMutex);
    if (!_mqttClient.connected()) return;

    JsonDocument doc;
    doc["status"] = healthStatus;
    doc["radar_ok"] = radarOk;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ok"] = wifiOk;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["heap_ok"] = heapOk;
    doc["mqtt_ok"] = _mqttClient.connected(); // Use actual state
    if (_netQuality) {
        doc["mqtt_reconnects"] = _netQuality->mqttReconnects;
        doc["wifi_reconnects"] = _netQuality->wifiReconnects;
    }

    String payload;
    serializeJson(doc, payload);
    _mqttClient.publish(_topics.health, payload.c_str(), false);
}

void MQTTService::checkCertificateExpiry() {
    #ifndef MQTTS_ENABLED
    return;
    #else
    
    // Logic from main.cpp
    // NTP already configured in setup(), no need to reconfigure here
    
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        Serial.println("[Cert] ❌ Time not synced yet. Skipping check.");
        return;
    }
    
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    
    // mqtt_server_ca is extern from secrets.h
    int ret = mbedtls_x509_crt_parse(&cert, (const unsigned char*)mqtt_server_ca, strlen(mqtt_server_ca) + 1);
    if(ret != 0) {
        Serial.printf("[Cert] ❌ Parse failed: -0x%x\n", -ret);
        mbedtls_x509_crt_free(&cert);
        return;
    }
    
    struct tm exp_tm = {0};
    exp_tm.tm_year = cert.valid_to.year - 1900;
    exp_tm.tm_mon  = cert.valid_to.mon - 1;
    exp_tm.tm_mday = cert.valid_to.day;
    exp_tm.tm_hour = cert.valid_to.hour;
    exp_tm.tm_min  = cert.valid_to.min;
    exp_tm.tm_sec  = cert.valid_to.sec;
    
    time_t exp_time = mktime(&exp_tm);
    time_t now = time(nullptr);
    
    double seconds_left = difftime(exp_time, now);
    double days_left = seconds_left / 86400.0;
    
    Serial.printf("[Cert] Validity: %04d-%02d-%02d. Days left: %.1f\n", 
                  cert.valid_to.year, cert.valid_to.mon, cert.valid_to.day, days_left);
                  
    if(days_left < 30) {
        Serial.println("[Cert] ⚠️ WARNING: Certificate expires soon!");
        ClientLock lock(_clientMutex);
        if(_mqttClient.connected()) {
            _mqttClient.publish(_topics.notification, "WARNING: SSL certificate expiring soon!", true);
        }
    } else {
        Serial.println("[Cert] ✅ Certificate is valid.");
    }
    
    mbedtls_x509_crt_free(&cert);
    #endif
}
