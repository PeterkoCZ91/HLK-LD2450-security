#ifndef BLUETOOTH_SERVICE_H
#define BLUETOOTH_SERVICE_H

#include <NimBLEDevice.h>
#include <Arduino.h>
#include "services/ConfigManager.h"

class BluetoothService {
public:
    BluetoothService();

    void begin(const char* deviceName, ConfigManager* config);
    void update();
    void stop();
    void startEmergency();  // Re-activate BLE for WiFi recovery

    void setTimeout(uint32_t seconds) { _timeoutSeconds = seconds; }
    bool isRunning() const { return _isRunning; }

private:
    ConfigManager* _config = nullptr;
    bool _isRunning = false;
    uint32_t _timeoutSeconds = 300; // 5 minutes (frees heap for Telegram SSL)
    unsigned long _startTime = 0;

    NimBLEServer* _server = nullptr;
    NimBLEService* _configService = nullptr;
    NimBLEService* _statusService = nullptr;

    static const char* SERVICE_CONFIG_UUID;
    static const char* CHAR_WIFI_UUID;
    static const char* CHAR_RESTART_UUID;
    static const char* SERVICE_STATUS_UUID;
    static const char* CHAR_INFO_UUID;

    class WiFiCallbacks : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    };

    class ActionCallbacks : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    };

    WiFiCallbacks _wifiCb;
    ActionCallbacks _actionCb;
};

#endif
