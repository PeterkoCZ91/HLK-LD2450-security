/**
 * main_ld2450.cpp
 * ESP32 Security Node - LD2450 Edition v5.4.0
 * Multi-Target Tracking + Security System
 */

#include <Arduino.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "ld2450/types.h"
#include "ld2450/constants.h"
#include "ld2450/services/ConfigManager.h"
#include "ld2450/services/MQTTService.h"
#include "ld2450/services/LD2450Service.h"
#include "ld2450/services/WebService.h"
#include "ld2450/services/PresenceService.h"
#include "ld2450/services/SecurityMonitor.h"
#include "ld2450/services/EventLog.h"
#include "ld2450/services/TelegramService.h"
#include "ld2450/services/BluetoothService.h"
#include "secrets.h"
#include "ld2450/known_devices.h"

// --- STATIC ALLOCATION ---
static NoiseMap staticNoiseMap;
NoiseMap* noiseMap = &staticNoiseMap;
static AdaptiveConfig staticAdaptiveConfig;
bool useNoiseFilter = false;

// --- DEFINITIONS ---
#define WDT_TIMEOUT_SECONDS 360
#define RADAR_RX_PIN 18
#define RADAR_TX_PIN 19
#define RESET_BUTTON_PIN 0

// --- GLOBAL OBJECTS ---
Preferences preferences;
AsyncWebServer server(80);
DNSServer dns;
LD2450Service radar(RADAR_RX_PIN, RADAR_TX_PIN);

ConfigManager configManager;
MQTTService mqttService;
WebService webService;
PresenceService presenceService;
SecurityMonitor securityMonitor;
EventLog eventLog;
TelegramService telegramService;
BluetoothService bluetoothService;
AppContext appContext;

// --- VARIABLES ---
char device_hostname[32] = "";
char device_id[32] = "";
volatile bool shouldReboot = false;
bool telegramDeferred = false;
static unsigned long _lastUptimeSave = 0;
static bool _otaValidated = false;

// --- STATE ---
NetworkQuality netQuality;
TamperState tamperState;
ZoneConfig zoneConfig;
PolygonZone detectionPolygons[MAX_POLYGONS];
uint8_t polyCount = 0;
BlackoutZone blackoutZones[MAX_BLACKOUT_ZONES];
uint8_t blackoutZoneCount = 0;
GhostTracker ghostTracker;
TargetHistory targetHistory;
Tripwire tripwire;
TargetAnalytics targetAnalytics;

// --- HELPERS ---

String getResetReason() {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON: return "Power On";
        case ESP_RST_SW: return "Software Reset";
        case ESP_RST_PANIC: return "Crash/Panic";
        case ESP_RST_INT_WDT: return "Watchdog (Interrupt)";
        case ESP_RST_TASK_WDT: return "Watchdog (Task)";
        case ESP_RST_WDT: return "Watchdog (Other)";
        case ESP_RST_DEEPSLEEP: return "Deep Sleep";
        case ESP_RST_BROWNOUT: return "Brownout";
        default: return "Unknown";
    }
}

void safeRestart(const char* reason) {
    Serial.printf("[RESTART] Reason: %s\n", reason);
    Preferences p;
    p.begin("ld2450_sys", false);
    p.putString("rst_reason", reason);
    p.putULong("rst_uptime", millis() / 1000);

    // Restart history (last 5 entries as JSON)
    String history = p.getString("rst_history", "[]");
    JsonDocument doc;
    deserializeJson(doc, history);
    JsonArray arr = doc.as<JsonArray>();
    // Keep max 5 entries
    while (arr.size() >= 5) arr.remove(0);
    JsonObject entry = arr.add<JsonObject>();
    entry["reason"] = reason;
    entry["uptime"] = millis() / 1000;
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
        char ts[24];
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &ti);
        entry["time"] = ts;
    }
    String out;
    serializeJson(doc, out);
    p.putString("rst_history", out);
    p.end();

    delay(100);
    ESP.restart();
}

void saveBlackoutZonesFn() {
    preferences.begin("ld2450-zones", false);
    preferences.putBytes("blackout", blackoutZones, sizeof(blackoutZones));
    preferences.putUChar("bz_count", blackoutZoneCount);
    preferences.end();
}

// --- MQTT CALLBACK (alarm commands from HA) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

    // Check alarm command topic (HA MQTT Alarm Panel protocol)
    if (String(topic) == mqttService.getTopics().alarm_command) {
        if (msg == "ARM_AWAY") {
            securityMonitor.setArmed(true, false, false);
        } else if (msg == "ARM_HOME") {
            securityMonitor.setArmed(true, false, true);
        } else if (msg == "DISARM") {
            securityMonitor.setArmed(false);
        }
    }
}

// --- WIFI SETUP ---
bool shouldSaveConfig = false;
void saveConfigCallback() { shouldSaveConfig = true; }

void setupWiFi() {
  #if LAB_MODE == 1
    Serial.println("[WiFi] LAB MODE - Direct connection");
    // Factory reset - require 3-second sustained press on BOOT button
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    delay(200);
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
        Serial.println("[RESET] Button detected - hold 3s for factory reset...");
        unsigned long pressStart = millis();
        while (digitalRead(RESET_BUTTON_PIN) == LOW) {
            if (millis() - pressStart >= 3000) {
                Serial.println("[RESET] Factory reset triggered!");
                preferences.clear();
                delay(500);
                safeRestart("factory_reset");
            }
            delay(50);
        }
        Serial.println("[RESET] Button released - continuing normal boot");
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID_DEFAULT, WIFI_PASS_DEFAULT);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500); Serial.print("."); attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("\n[WiFi] FAILED - continuing without network");
        // Don't restart! Radar will work locally without WiFi.
    }
    return;
  #endif

  // PROD MODE
  AsyncWiFiManager* wm = new AsyncWiFiManager(&server, &dns);
  // Factory reset - require 3-second sustained press on BOOT button
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  delay(200);
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      Serial.println("[RESET] Button detected - hold 3s for factory reset...");
      unsigned long pressStart = millis();
      while (digitalRead(RESET_BUTTON_PIN) == LOW) {
          if (millis() - pressStart >= 3000) {
              Serial.println("[RESET] Factory reset triggered!");
              wm->resetSettings();
              preferences.clear();
              delay(500);
              safeRestart("factory_reset");
          }
          delay(50);
      }
      Serial.println("[RESET] Button released - continuing normal boot");
  }

  SystemConfig& cfg = configManager.getConfig();

  wm->setConfigPortalTimeout(300);
  wm->setConnectTimeout(20);
  wm->setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  AsyncWiFiManagerParameter* p_server = new AsyncWiFiManagerParameter("mqtt_server", "MQTT Server", cfg.mqtt_server, 40);
  AsyncWiFiManagerParameter* p_port = new AsyncWiFiManagerParameter("mqtt_port", "MQTT Port", cfg.mqtt_port, 6);
  AsyncWiFiManagerParameter* p_user = new AsyncWiFiManagerParameter("mqtt_user", "MQTT User", cfg.mqtt_user, 32);
  AsyncWiFiManagerParameter* p_pass = new AsyncWiFiManagerParameter("mqtt_pass", "MQTT Password", cfg.mqtt_pass, 32, "type='password'");

  wm->addParameter(p_server);
  wm->addParameter(p_port);
  wm->addParameter(p_user);
  wm->addParameter(p_pass);
  wm->setSaveConfigCallback(saveConfigCallback);

  if (!wm->autoConnect(device_hostname, AP_PASS)) {
      safeRestart("wifi_portal_timeout");
  }

  if (shouldSaveConfig) {
      strncpy(cfg.mqtt_server, p_server->getValue(), sizeof(cfg.mqtt_server) - 1);
      strncpy(cfg.mqtt_port, p_port->getValue(), sizeof(cfg.mqtt_port) - 1);
      strncpy(cfg.mqtt_user, p_user->getValue(), sizeof(cfg.mqtt_user) - 1);
      strncpy(cfg.mqtt_pass, p_pass->getValue(), sizeof(cfg.mqtt_pass) - 1);
      configManager.save();
  }

  delete p_server; delete p_port; delete p_user; delete p_pass;
  delete wm;
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== ESP32 LD2450 SECURITY NODE " FW_VERSION " ===");
  Serial.print("Reset Reason: ");
  Serial.println(getResetReason());

  // Show previous session info
  {
    Preferences p;
    p.begin("ld2450_sys", true);
    String prevReason = p.getString("rst_reason", "");
    unsigned long prevUptime = p.getULong("rst_uptime", 0);
    p.end();
    if (prevReason.length() > 0) {
      Serial.printf("[SYS] Previous restart: %s (uptime %lu s)\n", prevReason.c_str(), prevUptime);
    }
  }

  // OTA Rollback — delayed validation in loop() after 60s stable operation
  {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        Serial.println("[OTA] New firmware pending validation (60s grace period)...");
      }
    }
  }

  // LittleFS
  if (!LittleFS.begin(true)) {
      Serial.println("[LittleFS] Mount Failed");
  } else {
      Serial.println("[LittleFS] Mounted");
  }

  pinMode(LED_PIN_DEFAULT, OUTPUT);
  digitalWrite(LED_PIN_DEFAULT, LOW);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  {
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    // ESP-IDF 5.x initializes TWDT automatically — reconfigure it
    if (esp_task_wdt_reconfigure(&wdt_config) != ESP_OK) {
        esp_task_wdt_init(&wdt_config);
    }
  }
#else
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
#endif
  esp_task_wdt_add(NULL);

  preferences.begin("ld2450_config", false);

  // ConfigManager
  configManager.begin();
  SystemConfig& cfg = configManager.getConfig();

  // Generate Device ID from MAC
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

  // Defaults based on MAC
  snprintf(device_id, sizeof(device_id), "mw1_%02X%02X", macAddr[4], macAddr[5]);
  snprintf(device_hostname, sizeof(device_hostname), "esp32-ld2450-%02X%02X", macAddr[4], macAddr[5]);

  // NVS hostname (only if no known_devices match)
  bool knownDevice = false;
  for (int i = 0; i < KNOWN_DEVICE_COUNT; i++) {
      if (strcasecmp(KNOWN_DEVICES[i].mac, macStr) == 0) {
          strncpy(device_id, KNOWN_DEVICES[i].id, sizeof(device_id) - 1);
          strncpy(device_hostname, KNOWN_DEVICES[i].hostname, sizeof(device_hostname) - 1);
          Serial.printf("[SETUP] Known device: %s\n", KNOWN_DEVICES[i].id);
          knownDevice = true;
          break;
      }
  }

  if (!knownDevice) {
      String savedHostname = preferences.getString("hostname", "");
      if (savedHostname.length() > 0) {
          strncpy(device_hostname, savedHostname.c_str(), sizeof(device_hostname) - 1);
      }
  }

  Serial.printf("[SETUP] Device ID: %s, Hostname: %s\n", device_id, device_hostname);

  // WiFi
  setupWiFi();
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  Serial.printf("[HEAP] After WiFi: %u\n", ESP.getFreeHeap());

  if (wifiConnected) {
    Serial.println("[WiFi] Connected");
    MDNS.begin(device_hostname);
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] http://%s.local\n", device_hostname);

    // NTP Time Sync
    configTime(3600, 3600, "pool.ntp.org", "time.google.com");
    Serial.println("[NTP] Time sync configured (GMT+1, DST)");

    // MQTT
    mqttService.begin(&preferences, device_id, &netQuality);
    mqttService.setCallback(mqttCallback);
  } else {
    Serial.println("[WiFi] No connection - skipping mDNS, MQTT");
  }

  // Radar
  if (!radar.begin(Serial2)) {
    Serial.println("[RADAR] Failed to init serial!");
  }
  Serial.printf("[HEAP] After Radar: %u\n", ESP.getFreeHeap());

  // EventLog
  eventLog.begin();

  // SecurityMonitor
  securityMonitor.begin(&mqttService, &eventLog, &preferences);
  securityMonitor.setSirenPin(SIREN_PIN_DEFAULT);

  // Restore armed state from NVS (with exit delay to prevent false trigger on boot)
  if (preferences.getBool("sec_armed", false)) {
      bool homeMode = preferences.getBool("sec_home", false);
      securityMonitor.setArmed(true, false, homeMode);
      Serial.printf("[SecMon] Armed state restored from NVS (%s, with exit delay)\n",
                    homeMode ? "HOME" : "AWAY");
  }
  Serial.printf("[HEAP] After Security: %u\n", ESP.getFreeHeap());

  // AppContext (DI Container)
  appContext.preferences = &preferences;
  appContext.config = &configManager;
  appContext.mqtt = &mqttService;
  appContext.radar = &radar;
  appContext.security = &securityMonitor;
  appContext.eventLog = &eventLog;
  appContext.zoneConfig = &zoneConfig;
  appContext.adaptiveConfig = &staticAdaptiveConfig;
  appContext.noiseMap = noiseMap;
  appContext.targetHistory = &targetHistory;
  appContext.ghostTracker = &ghostTracker;
  appContext.tamperState = &tamperState;
  appContext.tripwire = &tripwire;
  appContext.analytics = &targetAnalytics;
  appContext.netQuality = &netQuality;
  appContext.blackoutZones = blackoutZones;
  appContext.blackoutZoneCount = &blackoutZoneCount;
  appContext.polygons = detectionPolygons;
  appContext.polyCount = &polyCount;
  appContext.deviceId = device_id;
  appContext.deviceHostname = device_hostname;
  appContext.authUser = cfg.auth_user;
  appContext.authPass = cfg.auth_pass;
  appContext.useNoiseFilter = &useNoiseFilter;
  appContext.saveBlackoutZones = saveBlackoutZonesFn;
  appContext.shouldReboot = &shouldReboot;
  appContext.telegram = &telegramService;
  appContext.bluetooth = &bluetoothService;
  appContext.dataMutex = xSemaphoreCreateMutex();

  // WebService
  webService.begin(&appContext, &server);
  Serial.printf("[HEAP] After WebService: %u\n", ESP.getFreeHeap());

  // BluetoothService
  bluetoothService.begin(device_hostname, &configManager);
  Serial.printf("[HEAP] After BLE: %u\n", ESP.getFreeHeap());

  // TelegramService (requires WiFi + enough heap for SSL)
  // SSL/TLS handshake needs ~40KB; with BLE active heap may be too tight
  // If heap is sufficient now, start immediately; otherwise defer until BLE stops
  if (wifiConnected && ESP.getFreeHeap() > 100000) {
    telegramService.begin(&preferences);
    telegramService.setRadarService(&radar);
    telegramService.setSecurityMonitor(&securityMonitor);
    telegramService.setRebootFlag(&shouldReboot);
    Serial.printf("[HEAP] After Telegram: %u\n", ESP.getFreeHeap());
  } else if (wifiConnected) {
    Serial.printf("[Telegram] Deferred - heap %u, will start after BLE stops\n", ESP.getFreeHeap());
    telegramDeferred = true;
  }

  // OTA Configuration (requires WiFi)
  if (wifiConnected) {
    ArduinoOTA.setHostname(device_hostname);
    ArduinoOTA.setPassword("admin");
    ArduinoOTA.setPort(3232);

    ArduinoOTA.onStart([]() {
      String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
      if (type == "filesystem") LittleFS.end();
      Serial.println("[OTA] Update Start: " + type);
      if (mqttService.connected()) {
        mqttService.publish(mqttService.getTopics().notification, ("OTA Update: " + type).c_str(), false);
        mqttService.publish(mqttService.getTopics().availability, "updating", true);
        mqttService.update();
      }
      digitalWrite(LED_PIN_DEFAULT, HIGH);
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("\n[OTA] Update Complete!");
      if (mqttService.connected()) {
        mqttService.publish(mqttService.getTopics().notification, "OTA Update complete. Rebooting...", false);
        mqttService.update();
        delay(100);
      }
      digitalWrite(LED_PIN_DEFAULT, LOW);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      static unsigned long lastPrint = 0;
      unsigned long now = millis();
      if (now - lastPrint > 1000) {
        Serial.printf("[OTA] Progress: %u%%\r", total > 0 ? (progress * 100) / total : 0);
        lastPrint = now;
      }
    });

    ArduinoOTA.onError([](ota_error_t error) {
      const char* msg = "Unknown";
      if (error == OTA_AUTH_ERROR) msg = "Auth Failed";
      else if (error == OTA_BEGIN_ERROR) msg = "Begin Failed";
      else if (error == OTA_CONNECT_ERROR) msg = "Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) msg = "Receive Failed";
      else if (error == OTA_END_ERROR) msg = "End Failed";
      Serial.printf("[OTA] Error: %s\n", msg);
      if (mqttService.connected()) {
        mqttService.publish(mqttService.getTopics().availability, "online", true);
      }
      digitalWrite(LED_PIN_DEFAULT, LOW);
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready - Hostname: " + String(device_hostname));
  } else {
    Serial.println("[OTA] Skipped - no WiFi");
  }

  // Presence Service (main processing loop)
  presenceService.begin(&appContext);

  Serial.printf("[SETUP] Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println("=== SETUP COMPLETE ===\n");
}

// --- LOOP ---
void loop() {
  esp_task_wdt_reset();
  ArduinoOTA.handle();
  presenceService.update();
  securityMonitor.update();
  bluetoothService.update();

  unsigned long now = millis();

  // MQTT reconnect: force state re-publish
  if (mqttService.consumeReconnect()) {
      securityMonitor.forceRepublish();
  }

  // P0-5: OTA delayed validation — wait 60s before marking firmware valid
  if (!_otaValidated && now > TIMEOUT_OTA_VALIDATION_MS) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.println("[OTA] Firmware validated after 60s stable operation");
      }
    }
    _otaValidated = true;
  }

  // P0-4: Uptime persistence — save every hour
  if (now - _lastUptimeSave > INTERVAL_UPTIME_SAVE_MS) {
    _lastUptimeSave = now;
    Preferences p;
    p.begin("ld2450_sys", false);
    p.putULong("uptime", now / 1000);
    p.end();
  }

  // Deferred Telegram init: start after BLE frees heap
  if (telegramDeferred && !bluetoothService.isRunning()) {
    telegramDeferred = false;
    if (WiFi.status() == WL_CONNECTED && ESP.getFreeHeap() > 100000) {
      telegramService.begin(&preferences);
      telegramService.setRadarService(&radar);
      telegramService.setSecurityMonitor(&securityMonitor);
      telegramService.setRebootFlag(&shouldReboot);
      Serial.printf("[Telegram] Deferred init OK - heap: %u\n", ESP.getFreeHeap());
    } else {
      Serial.printf("[Telegram] Deferred init failed - heap: %u\n", ESP.getFreeHeap());
    }
  }

  // Scheduled arm/disarm (check every 30s)
  {
    static unsigned long lastSchedCheck = 0;
    if (now - lastSchedCheck > 30000) {
      lastSchedCheck = now;
      time_t epoch = time(nullptr);
      if (epoch > 1700000000) {
        struct tm ti;
        localtime_r(&epoch, &ti);
        int cur = ti.tm_hour * 60 + ti.tm_min;
        const char* armT = configManager.getConfig().sched_arm_time;
        const char* disT = configManager.getConfig().sched_disarm_time;
        int h, m;
        if (strlen(armT) >= 4 && sscanf(armT, "%d:%d", &h, &m) == 2 && cur == h*60+m && !securityMonitor.isArmed()) {
          securityMonitor.setArmed(true, false);
          Serial.printf("[SCHED] Auto-armed at %s\n", armT);
          if (telegramService.isEnabled()) telegramService.sendMessage("🔒 Scheduled arm (" + String(armT) + ")");
        }
        if (strlen(disT) >= 4 && sscanf(disT, "%d:%d", &h, &m) == 2 && cur == h*60+m && securityMonitor.isArmed()) {
          securityMonitor.setArmed(false);
          Serial.printf("[SCHED] Auto-disarmed at %s\n", disT);
          if (telegramService.isEnabled()) telegramService.sendMessage("🔓 Scheduled disarm (" + String(disT) + ")");
        }
      }
    }
  }

  // Auto-arm after N minutes of no presence
  {
    static unsigned long lastPresenceTime = now;
    static bool wasArmed = false;
    if (radar.getTargetCount() > 0) lastPresenceTime = now;
    bool armed = securityMonitor.isArmed();
    if (wasArmed && !armed) lastPresenceTime = now;
    wasArmed = armed;
    uint16_t autoMin = configManager.getConfig().auto_arm_minutes;
    if (autoMin > 0 && !armed && (now - lastPresenceTime) / 60000 >= autoMin) {
      securityMonitor.setArmed(true, false);
      wasArmed = true;
      lastPresenceTime = now;
      Serial.printf("[AUTO-ARM] No presence for %u min\n", autoMin);
      if (telegramService.isEnabled()) telegramService.sendMessage("🔒 Auto-arm: no presence " + String(autoMin) + " min");
    }
  }

  // Reboot flag (from Telegram /restart command)
  if (shouldReboot) {
    delay(500);
    safeRestart("telegram_command");
  }
}
