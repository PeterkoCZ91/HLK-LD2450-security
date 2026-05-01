#ifndef LD2450_SERVICE_H
#define LD2450_SERVICE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "ld2450/types.h"

// Health stats for monitoring
struct RadarHealth {
    uint32_t framesTotal = 0;
    uint32_t framesGood = 0;
    uint32_t framesCorrupt = 0;
    uint32_t bufferOverflows = 0;
    unsigned long lastFrameTime = 0;
    bool connected = false;
    float fps = 0;
    uint32_t _fpsCounter = 0;
    unsigned long _fpsLastCalc = 0;
};

class LD2450Service {
public:
    LD2450Service(int8_t rxPin, int8_t txPin);

    bool begin(HardwareSerial& serial);
    void update();

    // Spustí dedikovaný FreeRTOS task pro UART read+parse. Voláním po begin()
    // se odstraní závislost UART parsingu na hlavní smyčce — žádný UART overflow
    // při dlouhých blokujících operacích (MQTT TLS handshake, NVS write, …).
    // Pokud task už běží, no-op.
    bool startTask(uint32_t stackSize = 4096, UBaseType_t priority = 2, BaseType_t coreId = 1);
    void stopTask();
    bool isTaskRunning() const { return _taskRunning; }

    // Thread-safe data access (non-blocking mutex, safe from any core)
    uint8_t getTargetCount() const;
    LD2450Target getTarget(uint8_t index) const;
    bool isConnected() const;
    RadarHealth getHealth() const;
    String getRadarMAC() const { return _radarMAC; }

    // Configuration
    void setBaudRate(uint32_t baud);
    bool enableMultiTargetTracking(bool enable);
    bool setZoneConfig(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY);

    // Vypne/zapne BLE rádia v samotném modulu LD2450 (cmd 0xA4 + restart 0xA3).
    // Doporučeno: vypnout pro security — sníží attack-surface (modul jinak vysílá
    // unauth GATT konfigurační rozhraní). Změna persistuje v NVS modulu.
    bool setBluetoothEnabled(bool enable);

    // Native region filter (cmd 0xC2). Modul si v NVM uloží 3 obdélníkové zóny
    // a podle zvoleného módu buď reportuje pouze cíle uvnitř (detect-only)
    // nebo cíle uvnitř filtruje (exclude). Hardware-side filtrování → cíle jsou
    // odfiltrované ještě před odesláním přes UART. Komplementární k SW polygonům.
    //
    // Wire format (per ESPHome ld2450 production code):
    //   header  FD FC FB FA
    //   length  1C 00          (28 = 2 cmd + 26 payload)
    //   cmd     C2 00          (little-endian)
    //   payload [mode_lo, mode_hi=0x00] + 3× zone (x1_lo,x1_hi,y1_lo,y1_hi,
    //                                              x2_lo,x2_hi,y2_lo,y2_hi)
    //   footer  04 03 02 01
    // Coordinates: int16 little-endian (val & 0xFFFF). Změna persistuje v NVM modulu.
    struct RegionFilter {
        uint8_t mode;        // 0=disabled, 1=detect-only, 2=exclude
        int16_t x1[3], y1[3], x2[3], y2[3];   // 3 rectangular zones (mm)
    };
    bool setRegionFilter(const RegionFilter& cfg);

    // API Compatibility
    String getTelemetryJson() const;
    void factoryReset();
    bool startAutoCalibration();
    bool isCalibrating() const;
    uint8_t getCalibrationProgress() const;
    void enableEngineeringMode(bool enable);
    bool isEngineeringMode() const;
    void setTestMode(bool enable);
    void getEngineeringData(uint8_t* mov, uint8_t* stat);

private:
    int8_t _rxPin;
    int8_t _txPin;
    HardwareSerial* _serial;

    // Ring buffer for UART data
    static const size_t RING_BUF_SIZE = 2048;
    uint8_t _ringBuf[RING_BUF_SIZE];
    size_t _ringHead = 0;  // Write position
    size_t _ringTail = 0;  // Read position

    // Parsed data (protected by mutex)
    LD2450Target _targets[3];
    uint8_t _targetCount;
    RadarHealth _health;
    SemaphoreHandle_t _mutex;

    // Dedikovaný UART task
    TaskHandle_t _taskHandle = nullptr;
    volatile bool _taskRunning = false;
    static void radarTaskFn(void* arg);
    void serviceOnce();  // body radar tasku — read+parse+timeout check

    // Internal parsing
    void readIntoRing();
    bool parseFrame();
    size_t ringAvailable() const;
    uint8_t ringPeek(size_t offset) const;
    void ringAdvance(size_t count);

    // Radar BLE MAC (queried during begin via cmd 0xA5; cached, read-only after init)
    String _radarMAC;
    bool queryMAC();
};

#endif // LD2450_SERVICE_H
