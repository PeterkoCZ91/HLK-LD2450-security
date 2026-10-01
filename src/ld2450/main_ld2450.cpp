/**
 * main_ld2450.cpp
 * ESP32 Security Node - LD2450 Edition
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
// SEC-02: ArduinoOTA is disabled by default. It only compiles in when the build
// explicitly opts in with -D ENABLE_ARDUINO_OTA, which in turn requires a unique
// ARDUINO_OTA_PASSWORD supplied outside source control (see platformio.ini).
#ifdef ENABLE_ARDUINO_OTA
  #include <ArduinoOTA.h>
  #ifndef ARDUINO_OTA_PASSWORD
    #error "ENABLE_ARDUINO_OTA requires a unique ARDUINO_OTA_PASSWORD (e.g. from an env var); refusing to build with a default/blank OTA password"
  #endif
  // Fail closed: an empty password (e.g. an unset env var expanding to "") must not
  // ship an unauthenticated OTA listener. sizeof("") == 1, so require length >= 1.
  static_assert(sizeof(ARDUINO_OTA_PASSWORD) > 1,
                "ARDUINO_OTA_PASSWORD is empty — set the env var before building (export ARDUINO_OTA_PASSWORD=...)");
#endif
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "ld2450/types.h"
#include "ld2450/constants.h"
#include "ld2450/services/ConfigManager.h"
#include "ld2450/utils/schedule_time.h"
#include "ld2450/utils/fs_policy.h"
#include "ld2450/utils/zone_persist.h"
#include "ld2450/utils/gpio_allowlist.h"
#include "ld2450/utils/wifi_retry.h"
#include "ld2450/utils/sched_edge.h"
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
// OTA-01: the rollback-validation window is measured from the END of setup(), not
// from reset — a slow captive portal / provisioning must not consume the stability
// window before the app has actually run.
static unsigned long _setupCompleteMs = 0;

// --- STATE ---
NetworkQuality netQuality;
TamperState tamperState;
ZoneConfig zoneConfig;
PolygonZone detectionPolygons[MAX_POLYGONS];
uint8_t polyCount = 0;
BlackoutZone blackoutZones[MAX_BLACKOUT_ZONES];
uint8_t blackoutZoneCount = 0;
// Day/Night profile masks (parallel arrays). bit0=day, bit1=night. Default 0x03 = both.
uint8_t polygonMasks[MAX_POLYGONS] = {0x03, 0x03, 0x03, 0x03, 0x03};
uint8_t blackoutMasks[MAX_BLACKOUT_ZONES] = {0x03, 0x03, 0x03, 0x03, 0x03};
uint8_t currentProfile = 0x01;  // Default day until first scheduler tick
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
    DeserializationError derr = deserializeJson(doc, history);
    // If NVS content is corrupt or not an array, start fresh - otherwise arr.add() would be UB.
    if (derr != DeserializationError::Ok || !doc.is<JsonArray>()) {
        doc.clear();
        doc.to<JsonArray>();
    }
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

    eventLog.flushNow();  // dirty events (max 60 s) by jinak restartem zanikly
    delay(100);
    ESP.restart();
}

// CFG-01: use a short-lived LOCAL Preferences handle for the "ld2450-zones"
// namespace. The global `preferences` handle stays open on "ld2450_config" for the
// whole app; calling begin()/end() on it here would repoint or close that shared
// handle and break every later _ctx->preferences->put* (alarm, schedule, MQTT, …).
void saveBlackoutZonesFn() {
    Preferences p;
    if (!p.begin("ld2450-zones", false)) {
        Serial.println("[Zones] ERROR: blackout save failed to open NVS namespace");
        return;
    }
    p.putBytes("blackout", blackoutZones, sizeof(blackoutZones));
    p.putUChar("bz_count", blackoutZoneCount);
    p.putBytes("bz_masks", blackoutMasks, sizeof(blackoutMasks));
    p.end();
}

void savePolygonsFn() {
    Preferences p;
    if (!p.begin("ld2450-zones", false)) {
        Serial.println("[Zones] ERROR: polygon save failed to open NVS namespace");
        return;
    }
    p.putBytes("polygons", detectionPolygons, sizeof(detectionPolygons));
    p.putUChar("poly_count", polyCount);
    p.putBytes("poly_masks", polygonMasks, sizeof(polygonMasks));
    p.end();
}

static void loadPolygonsFn() {
    Preferences p;
    p.begin("ld2450-zones", true);
    polyCount = p.getUChar("poly_count", 0);
    if (polyCount > MAX_POLYGONS) polyCount = 0;
    if (polyCount > 0) {
        p.getBytes("polygons", detectionPolygons, sizeof(detectionPolygons));
    }
    if (p.isKey("poly_masks")) {
        p.getBytes("poly_masks", polygonMasks, sizeof(polygonMasks));
    }
    if (p.isKey("bz_masks")) {
        p.getBytes("bz_masks", blackoutMasks, sizeof(blackoutMasks));
    }
    p.end();
}

void saveNoiseMapFn() {
    if (!noiseMap) return;
    File f = LittleFS.open("/noisemap.bin", FILE_WRITE);
    if (f) {
        f.write((uint8_t*)noiseMap->energy, sizeof(noiseMap->energy));
        f.close();
        Serial.println("[NoiseMap] Saved (callback)");
    }
}

// --- MQTT CALLBACK (alarm commands from HA) ---
// HA MQTT Alarm Panel protocol: the payload is either a plain command such as "DISARM",
// or JSON `{"action":"DISARM","code":"1234"}` (when HA has `code_disarm_required=true`).
// SEC-08: DISARM over MQTT ALWAYS requires a configured `sec_code`. If no code is
// configured, remote DISARM is rejected (fail closed) - broker access alone
// must not be enough to disarm the alarm. ARM commands need no code (not sensitive).
void mqttCallback(char* topic, byte* payload, unsigned int length) {
#ifndef ENABLE_MQTT_ALARM_COMMANDS
    // Remote MQTT alarm control (ARM/DISARM) is deactivated. We do not subscribe to
    // the command topic either, so this should never fire — ignore defensively.
    (void)topic; (void)payload; (void)length;
    return;
#else
    if (length == 0 || length > 256) return;
    char buf[257];
    memcpy(buf, payload, length);
    buf[length] = '\0';

    if (strcmp(topic, mqttService.getTopics().alarm_command) != 0) return;

    // Detekce JSON tvaru
    String action;
    String code;
    if (buf[0] == '{') {
        JsonDocument doc;
        if (deserializeJson(doc, buf, length) == DeserializationError::Ok) {
            if (doc["action"].is<const char*>()) action = doc["action"].as<const char*>();
            if (doc["code"].is<const char*>())   code   = doc["code"].as<const char*>();
        }
    } else {
        action = buf;
    }

    if (action == "ARM_AWAY") {
        securityMonitor.setArmed(true, false, false);
    } else if (action == "ARM_HOME") {
        securityMonitor.setArmed(true, false, true);
    } else if (action == "DISARM") {
        Preferences p;
        p.begin("ld2450_config", true);
        String requiredCode = p.getString("sec_code", "");
        p.end();
        if (requiredCode.length() == 0) {
            // SEC-08: fail closed — no command secret configured, so refuse remote
            // DISARM entirely. Broker access alone must not control the alarm.
            Serial.println("[MQTT] DISARM rejected: no sec_code configured (set one to allow remote disarm)");
            if (mqttService.connected()) {
                mqttService.publish(mqttService.getTopics().notification,
                    "DISARM rejected: no security code configured", false);
            }
        } else if (requiredCode == code) {
            securityMonitor.setArmed(false);
        } else {
            Serial.println("[MQTT] DISARM rejected: invalid/missing code");
            if (mqttService.connected()) {
                mqttService.publish(mqttService.getTopics().notification, "DISARM rejected: invalid code", false);
            }
        }
    }
#endif // ENABLE_MQTT_ALARM_COMMANDS
}

// --- WIFI SETUP ---
bool shouldSaveConfig = false;
void saveConfigCallback() { shouldSaveConfig = true; }

void setupWiFi() {
  AsyncWiFiManager* wm = new AsyncWiFiManager(&server, &dns);
  Serial.println("[WiFi] Captive portal mode");

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

  // PROD MODE: captive portal for WiFi + MQTT parameters
  SystemConfig& cfg = configManager.getConfig();

  wm->setConfigPortalTimeout(300);
  wm->setConnectTimeout(20);
  wm->setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  AsyncWiFiManagerParameter* p_server = new AsyncWiFiManagerParameter("mqtt_server", "MQTT Server", cfg.mqtt_server, sizeof(cfg.mqtt_server));
  AsyncWiFiManagerParameter* p_port = new AsyncWiFiManagerParameter("mqtt_port", "MQTT Port", cfg.mqtt_port, sizeof(cfg.mqtt_port));
  AsyncWiFiManagerParameter* p_user = new AsyncWiFiManagerParameter("mqtt_user", "MQTT User", cfg.mqtt_user, sizeof(cfg.mqtt_user));
  AsyncWiFiManagerParameter* p_pass = new AsyncWiFiManagerParameter("mqtt_pass", "MQTT Password", cfg.mqtt_pass, sizeof(cfg.mqtt_pass), "type='password'");

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
      cfg.mqtt_server[sizeof(cfg.mqtt_server) - 1] = '\0';
      cfg.mqtt_port[sizeof(cfg.mqtt_port) - 1] = '\0';
      cfg.mqtt_user[sizeof(cfg.mqtt_user) - 1] = '\0';
      cfg.mqtt_pass[sizeof(cfg.mqtt_pass) - 1] = '\0';
      configManager.save();
  }

  delete p_server; delete p_port; delete p_user; delete p_pass;
  delete wm;
}

#ifdef ENABLE_ARDUINO_OTA
// ArduinoOTA handler setup — extracted so it can be (re)installed on a WiFi
// connect edge, not only at boot (NET-01). Disabled by default (SEC-02).
static void setupArduinoOTA() {
    ArduinoOTA.setHostname(device_hostname);
    ArduinoOTA.setPassword(ARDUINO_OTA_PASSWORD);
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
    Serial.println("[OTA] ArduinoOTA ready - Hostname: " + String(device_hostname));
}
#endif

// NET-01: idempotent "WiFi is up" lifecycle handler. Runs on the first-boot
// connect and again on every later reconnect edge, so a node that booted
// offline still gets mDNS, NTP, MQTT and OTA once WiFi appears. The one-shot
// bindings (MQTT client, ArduinoOTA listener) are guarded so a reconnect does
// not re-allocate them; mDNS/NTP are cheap and simply refreshed.
static void onWifiConnected(bool firstBoot) {
    static bool s_mqttBegun = false;
    static bool s_otaBegun  = false;

    Serial.println("[NET] Bringing up network services");

    // mDNS — refresh (safe to end+begin after an IP change).
    MDNS.end();
    if (MDNS.begin(device_hostname)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mDNS] http://%s.local\n", device_hostname);
    }

    // NTP with configurable POSIX timezone (TIME-01).
    {
        const char* tz = configManager.getConfig().timezone;
        if (!tz || !tz[0]) tz = "CET-1CEST,M3.5.0,M10.5.0/3";
        configTzTime(tz, "pool.ntp.org", "time.google.com");
        Serial.printf("[NTP] Time sync configured (TZ=%s)\n", tz);
    }

    // MQTT — begin once; it self-reconnects afterwards via update().
    if (!s_mqttBegun) {
        mqttService.begin(&preferences, device_id, &netQuality);
        mqttService.setCallback(mqttCallback);
        s_mqttBegun = true;
    }

#ifdef ENABLE_ARDUINO_OTA
    if (!s_otaBegun) {
        setupArduinoOTA();
        s_otaBegun = true;
    }
#endif

    // Telegram needs WiFi + heap; if it was not started at boot (e.g. booted
    // offline), request the deferred start path used by the main loop.
    if (!firstBoot && !telegramService.isEnabled()) {
        telegramDeferred = true;
    }
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
  // Never format blindly: a format erases logs, noise map and offline MQTT data.
  {
    Preferences fsPrefs;
    fsPrefs.begin("fsstate", false);
    bool everMounted = fsPrefs.getBool("ok", false);
    uint8_t failedBoots = fsPrefs.getUChar("fails", 0);
    bool mounted = LittleFS.begin(false);
    if (!mounted) {
      delay(200);
      mounted = LittleFS.begin(false);  // one retry for a transient failure
    }
    if (!mounted) {
      Serial.println("[LittleFS] Mount Failed");
      if (ld2450_fs::mountFailAction(everMounted, failedBoots) == ld2450_fs::Format) {
        Serial.println("[LittleFS] Formatting (fresh flash or repeated mount failures)");
        mounted = LittleFS.begin(true);
      } else {
        if (failedBoots < 255) fsPrefs.putUChar("fails", failedBoots + 1);
        Serial.println("[LittleFS] Running without filesystem; data kept for the next boot");
      }
    }
    if (mounted) {
      Serial.println("[LittleFS] Mounted");
      if (!everMounted) fsPrefs.putBool("ok", true);
      if (failedBoots) fsPrefs.putUChar("fails", 0);
    }
    fsPrefs.end();
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

  // CFG-02: load the complete persisted zone model into the runtime ZoneConfig
  // (previously never loaded — zones reset to defaults on every reboot), with
  // one-time migration from the legacy zone_* short keys.
  ld2450_zones::load(preferences, zoneConfig);

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
    onWifiConnected(true);  // NET-01: mDNS + NTP + MQTT (+ OTA below)
  } else {
    Serial.println("[WiFi] No connection - services start on reconnect (NET-01)");
  }

  // Radar
  if (!radar.begin(Serial2)) {
    Serial.println("[RADAR] Failed to init serial!");
  } else {
    // Dedicated UART task on Core 1 (same core as loop), priority 2 (above loop=1).
    // Prevents UART overflow during long blocking operations in the main loop
    // (MQTT TLS handshake, NVS write, mbedtls cert parse).
    radar.startTask(4096, 2, 1);

    // Push the native region filter (cmd 0xC2) from NVS if active.
    // The filter is applied in hardware inside the module before the UART, so targets are
    // filtered out before they reach the parser buffer.
    {
        const SystemConfig& rfCfg = configManager.getConfig();
        if (rfCfg.region_filter_mode != 0) {
            LD2450Service::RegionFilter rf{};
            rf.mode = rfCfg.region_filter_mode;
            for (uint8_t z = 0; z < 3; z++) {
                rf.x1[z] = rfCfg.region_filter_zones[z * 4 + 0];
                rf.y1[z] = rfCfg.region_filter_zones[z * 4 + 1];
                rf.x2[z] = rfCfg.region_filter_zones[z * 4 + 2];
                rf.y2[z] = rfCfg.region_filter_zones[z * 4 + 3];
            }
            radar.setRegionFilter(rf);
        }
    }
  }
  Serial.printf("[HEAP] After Radar: %u\n", ESP.getFreeHeap());

  // EventLog
  eventLog.begin();

  // SecurityMonitor
  securityMonitor.begin(&mqttService, &eventLog, &preferences);
  // DOC-03: siren GPIO is NVS-configurable but validated against a safe
  // allowlist; an out-of-range/unsafe stored value falls back to disabled (-1).
  {
    int sirenPin = preferences.getInt("siren_pin", SIREN_PIN_DEFAULT);
    if (!ld2450_gpio::sirenPinAllowed(sirenPin)) {
      Serial.printf("[SIREN] Stored GPIO %d not in safe allowlist — disabling\n", sirenPin);
      sirenPin = -1;
    }
    securityMonitor.setSirenPin((int8_t)sirenPin);
  }

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
  appContext.polygonMasks = polygonMasks;
  appContext.blackoutMasks = blackoutMasks;
  appContext.currentProfile = &currentProfile;
  appContext.deviceId = device_id;
  appContext.deviceHostname = device_hostname;
  appContext.authUser = cfg.auth_user;
  appContext.authPass = cfg.auth_pass;
  // SEC-03: warn (do not block) while web credentials are still the shipped default.
  if (ld2450::isDefaultCreds(cfg.auth_user, cfg.auth_pass, WEB_ADMIN_USER_DEFAULT, WEB_ADMIN_PASS_DEFAULT)) {
      Serial.println("[SECURITY] WARNING: default web credentials (admin/admin) in use — change them in the Network section.");
  }
  appContext.useNoiseFilter = &useNoiseFilter;
  appContext.saveBlackoutZones = saveBlackoutZonesFn;
  appContext.savePolygons = savePolygonsFn;
  appContext.saveNoiseMap = saveNoiseMapFn;
  appContext.shouldReboot = &shouldReboot;

  // Load polygons from NVS (same as blackout zones)
  loadPolygonsFn();
  appContext.telegram = &telegramService;
  appContext.bluetooth = &bluetoothService;
  appContext.dataMutex = xSemaphoreCreateMutex();
  if (!appContext.dataMutex) {
      Serial.println("[SETUP] FATAL: dataMutex create failed");
      delay(2000);
      safeRestart("mutex_create_failed");
  }

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

#ifdef ENABLE_ARDUINO_OTA
  // OTA listener is installed by onWifiConnected() (NET-01) — at boot if WiFi is
  // already up, otherwise on the first reconnect. Disabled by default (SEC-02).
  if (!wifiConnected) Serial.println("[OTA] Deferred - starts on WiFi connect");
#else
  Serial.println("[OTA] ArduinoOTA disabled in this build (use Web OTA)");
#endif

  // Presence Service (main processing loop)
  presenceService.begin(&appContext);

  Serial.printf("[SETUP] Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println("=== SETUP COMPLETE ===\n");
  _setupCompleteMs = millis(); // OTA-01: start the rollback-validation window here
}

// --- LOOP ---
void loop() {
  esp_task_wdt_reset();
#ifdef ENABLE_ARDUINO_OTA
  ArduinoOTA.handle();
#endif
  presenceService.update();
  securityMonitor.update();
  bluetoothService.update();

  unsigned long now = millis();

  // NET-01: edge-triggered WiFi lifecycle. Detect connect/disconnect edges and
  // (re)initialize dependent services on connect; nudge a reconnect when down.
  {
    static bool wifiWas = (WiFi.status() == WL_CONNECTED);
    static unsigned long lastReconnectTry = 0;
    bool wifiNow = (WiFi.status() == WL_CONNECTED);
    if (wifiNow && !wifiWas) {
      Serial.println("[NET] WiFi reconnected");
      onWifiConnected(false);
    } else if (!wifiNow && wifiWas) {
      Serial.println("[NET] WiFi lost — services will re-init on reconnect");
    }
    if (!wifiNow && (now - lastReconnectTry > 20000)) {
      lastReconnectTry = now;
      // WiFi.reconnect() pouziva jen POSLEDNI begin() (po selhani = zaloha) -> primarni
      // sit by se po vypadku routeru nikdy neobnovila. Se zalohou strida obe site.
      static bool s_tryBackup = false;
      const SystemConfig& wc = configManager.getConfig();
      const bool hasPrimary = true;  // primary credentials are stored by the captive portal
      s_tryBackup = ld2450_wifi::nextIsBackup(s_tryBackup, hasPrimary, wc.backup_ssid[0] != '\0');
      if (s_tryBackup) WiFi.begin(wc.backup_ssid, wc.backup_pass);
      else if (wc.backup_ssid[0] && hasPrimary) {
          WiFi.begin();  // re-use the portal-stored credentials
      } else WiFi.reconnect();
    }
    wifiWas = wifiNow;
  }

  // MQTT reconnect: force state re-publish
  if (mqttService.consumeReconnect()) {
      securityMonitor.forceRepublish();
  }

  // OTA-01: delayed rollback validation. The stability window is measured from the
  // end of setup(). Health signal is intentionally NOT tied to WiFi — the node is
  // designed to run offline (radar works without network), so requiring WiFi would
  // roll back a perfectly healthy offline image. Surviving the window without a crash
  // plus a non-starved heap is the health proof. Only mark validated on ESP_OK — if
  // the NVS write fails we keep _otaValidated false and retry on a later loop.
  if (!_otaValidated && _setupCompleteMs != 0 &&
      (millis() - _setupCompleteMs) > TIMEOUT_OTA_VALIDATION_MS) {
    bool healthy = (ESP.getFreeHeap() > HEAP_MIN_FOR_PUBLISH);
    if (healthy) {
      const esp_partition_t *running = esp_ota_get_running_partition();
      esp_ota_img_states_t ota_state;
      if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
          esp_err_t markErr = esp_ota_mark_app_valid_cancel_rollback();
          if (markErr == ESP_OK) {
            Serial.println("[OTA] Firmware validated after stable operation");
            _otaValidated = true;
          } else {
            Serial.printf("[OTA] mark_app_valid failed (%d) — will retry\n", markErr);
          }
        } else {
          // Not pending verify (normal boot / already validated) — nothing to do.
          _otaValidated = true;
        }
      }
    }
    // If not healthy yet, leave _otaValidated false and re-check next loop.
  }

  // P0-4: Uptime persistence — save every hour
  if (now - _lastUptimeSave > INTERVAL_UPTIME_SAVE_MS) {
    _lastUptimeSave = now;
    Preferences p;
    p.begin("ld2450_sys", false);
    p.putULong("uptime", now / 1000);
    p.end();
  }

  // Deferred Telegram init: start after BLE frees heap; retried every 60 s until started
  if (telegramDeferred && !bluetoothService.isRunning()) {
    static uint32_t lastTgTry = 0;
    if (ld2450_sched::retryDue((uint32_t)now, lastTgTry, 60000)) {
      lastTgTry = (uint32_t)now ? (uint32_t)now : 1;
      if (WiFi.status() == WL_CONNECTED && ESP.getFreeHeap() > 100000) {
        telegramDeferred = false;
        telegramService.begin(&preferences);
        telegramService.setRadarService(&radar);
        telegramService.setSecurityMonitor(&securityMonitor);
        telegramService.setRebootFlag(&shouldReboot);
        Serial.printf("[Telegram] Deferred init OK - heap: %u\n", ESP.getFreeHeap());
      } else {
        Serial.printf("[Telegram] Deferred init postponed - heap: %u\n", ESP.getFreeHeap());
      }
    }
  }

  // Scheduled arm/disarm (check every 30s)
  {
    static unsigned long lastSchedCheck = 0;
    static int32_t lastArmMin = -1, lastDisMin = -1;  // last handled schedule minute
    if (now - lastSchedCheck > 30000) {
      lastSchedCheck = now;
      time_t epoch = time(nullptr);
      if (epoch > 1700000000) {
        struct tm ti;
        localtime_r(&epoch, &ti);
        int cur = ti.tm_hour * 60 + ti.tm_min;
        const char* armT = configManager.getConfig().sched_arm_time;
        const char* disT = configManager.getConfig().sched_disarm_time;
        int32_t minId = (int32_t)(epoch / 60);
        // Act once per schedule edge: a manual override afterwards sticks until the next edge.
        if (ld2450_sched::edgeOnce(ld2450_time::scheduleDue(cur, armT), minId, lastArmMin) && !securityMonitor.isArmed()) {
          securityMonitor.setArmed(true, false);
          Serial.printf("[SCHED] Auto-armed at %s\n", armT);
          if (telegramService.isEnabled()) telegramService.sendMessage("🔒 Scheduled arm (" + String(armT) + ")");
        }
        if (ld2450_sched::edgeOnce(ld2450_time::scheduleDue(cur, disT), minId, lastDisMin) && securityMonitor.isArmed()) {
          securityMonitor.setArmed(false);
          Serial.printf("[SCHED] Auto-disarmed at %s\n", disT);
          if (telegramService.isEnabled()) telegramService.sendMessage("🔓 Scheduled disarm (" + String(disT) + ")");
        }
      }
    }
  }

  // Day/Night zone profile selector (check every 30s, shares time-since check with scheduled arm)
  // If night_start_time is empty, the profile stays day. Otherwise compare HH:MM with the current time.
  // If start > end (e.g. 22:00 -> 06:00) the night interval wraps over midnight.
  {
    static unsigned long lastProfCheck = 0;
    if (now - lastProfCheck > 30000) {
      lastProfCheck = now;
      const char* nsT = configManager.getConfig().night_start_time;
      const char* neT = configManager.getConfig().night_end_time;
      uint8_t newProfile = 0x01; // default day
      time_t epoch = time(nullptr);
      if (epoch > 1700000000) {
        struct tm ti; localtime_r(&epoch, &ti);
        int cur = ti.tm_hour * 60 + ti.tm_min;
        bool isNight = false;
        if (ld2450_time::isNightNow(cur, nsT, neT, isNight)) {
          newProfile = isNight ? 0x02 : 0x01;
        }
      }
      if (newProfile != currentProfile) {
        currentProfile = newProfile;
        Serial.printf("[PROFILE] Switched to %s\n", currentProfile == 0x02 ? "NIGHT" : "DAY");
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
