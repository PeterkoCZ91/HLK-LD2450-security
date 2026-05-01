#include "services/BluetoothService.h"
#include "secrets.h"
#include <WiFi.h>

extern void safeRestart(const char* reason);

const char* BluetoothService::SERVICE_CONFIG_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
const char* BluetoothService::CHAR_WIFI_UUID      = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
const char* BluetoothService::CHAR_RESTART_UUID   = "d949c585-1d6f-4d9f-a83a-48f86f8a845a";
const char* BluetoothService::SERVICE_STATUS_UUID = "3e766e4a-4363-45f8-8f8d-4e2e288e2a3c";
const char* BluetoothService::CHAR_INFO_UUID      = "c0ffee01-4363-45f8-8f8d-4e2e288e2a3c";

BluetoothService::BluetoothService() {}

void BluetoothService::begin(const char* deviceName, ConfigManager* config) {
    _config = config;

    uint32_t freeHeap = ESP.getFreeHeap();
    Serial.printf("[BLE] Free heap before init: %u\n", freeHeap);
    if (freeHeap < 50000) {
        Serial.println("[BLE] Not enough memory, skipping BLE");
        _isRunning = false;
        return;
    }

    _startTime = millis();
    _isRunning = true;

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // BLE Security: passkey pairing
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(BLE_PASSKEY_DEFAULT);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    Serial.println("[BT] Security enabled, passkey required for pairing");

    _server = NimBLEDevice::createServer();

    // Config Service (Write, encrypted)
    _configService = _server->createService(SERVICE_CONFIG_UUID);

    NimBLECharacteristic* pWiFiChar = _configService->createCharacteristic(
        CHAR_WIFI_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC
    );
    pWiFiChar->setCallbacks(&_wifiCb);

    NimBLECharacteristic* pActionChar = _configService->createCharacteristic(
        CHAR_RESTART_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC
    );
    pActionChar->setCallbacks(&_actionCb);

    _configService->start();

    // Status Service (Read, encrypted)
    _statusService = _server->createService(SERVICE_STATUS_UUID);
    NimBLECharacteristic* pInfoChar = _statusService->createCharacteristic(
        CHAR_INFO_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC
    );

    String info = "IP: " + WiFi.localIP().toString() + " | RSSI: " + String(WiFi.RSSI());
    pInfoChar->setValue(info.c_str());

    _statusService->start();

    // Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_CONFIG_UUID);
    pAdvertising->start();

    Serial.println("[BT] BLE Active and Advertising");
}

void BluetoothService::update() {
    if (!_isRunning) return;

    if (_timeoutSeconds > 0 && (millis() - _startTime) > (_timeoutSeconds * 1000)) {
        Serial.println("[BT] Timeout reached, stopping BLE");
        stop();
    }
}

void BluetoothService::stop() {
    if (!_isRunning) return;
    NimBLEDevice::deinit(true);
    _isRunning = false;
    Serial.println("[BT] BLE Stack stopped");
}

void BluetoothService::startEmergency() {
    if (_isRunning) return;

    // Guard 1: heap budget — NimBLE init si bere 25-30 KB. Pod 50K bychom skončili crashem.
    uint32_t heap = ESP.getFreeHeap();
    if (heap < 50000) {
        Serial.printf("[BT] Emergency aborted: heap %u < 50000\n", heap);
        return;
    }

    // Guard 2: pokud byl už NimBLE inicializován dříve a stop() volal deinit(true),
    // re-init by měl být bezpečný; ale pokud je stack ještě "v deinit transit", pomůže delay.
    Serial.println("[BT] Emergency BLE re-activation (WiFi lost)");
    _timeoutSeconds = 600; // 10 min emergency window
    _startTime = millis();
    _isRunning = true;

    if (!NimBLEDevice::isInitialized()) {
        NimBLEDevice::init("LD2450-EMERGENCY");
    }
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    // Emergency mode still requires passkey authentication
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(BLE_PASSKEY_DEFAULT);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    _server = NimBLEDevice::createServer();
    if (!_server) {
        Serial.println("[BT] Emergency: createServer failed");
        _isRunning = false;
        return;
    }

    _configService = _server->createService(SERVICE_CONFIG_UUID);
    NimBLECharacteristic* pWiFiChar = _configService->createCharacteristic(
        CHAR_WIFI_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC);
    pWiFiChar->setCallbacks(&_wifiCb);
    NimBLECharacteristic* pActionChar = _configService->createCharacteristic(
        CHAR_RESTART_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC);
    pActionChar->setCallbacks(&_actionCb);
    _configService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_CONFIG_UUID);
    pAdvertising->start();
    Serial.println("[BT] Emergency BLE advertising started");
}

// --- Callbacks ---

void BluetoothService::WiFiCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        size_t comma = value.find(',');
        if (comma != std::string::npos) {
            String ssid = String(value.substr(0, comma).c_str());
            String pass = String(value.substr(comma + 1).c_str());

            Preferences prefs;
            prefs.begin("ld2450_config", false);
            prefs.putString("bk_ssid", ssid);
            prefs.putString("bk_pass", pass);
            prefs.end();

            Serial.println("[BT] WiFi credentials saved. Restarting...");
            delay(1000);
            safeRestart("ble_wifi_config");
        }
    }
}

void BluetoothService::ActionCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    std::string value = pCharacteristic->getValue();
    if (value == "restart") {
        Serial.println("[BT] Restart command received via BLE");
        delay(500);
        safeRestart("ble_command");
    }
}
