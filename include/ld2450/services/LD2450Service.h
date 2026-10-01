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

    // Starts a dedicated FreeRTOS task for UART read+parse. Calling it after begin()
    // removes the dependency of UART parsing on the main loop, so there is no UART
    // overflow during long blocking operations (MQTT TLS handshake, NVS write, ...).
    // No-op if the task is already running.
    bool startTask(uint32_t stackSize = 4096, UBaseType_t priority = 2, BaseType_t coreId = 1);
    void stopTask();
    bool isTaskRunning() const { return _taskRunning; }

    // Thread-safe data access (non-blocking mutex, safe from any core)
    uint8_t getTargetCount() const;
    LD2450Target getTarget(uint8_t index) const;
    // RAD-02: read all three targets + count as one consistent snapshot under a
    // single mutex hold, so callers never mix fields from two different frames.
    void getSnapshot(LD2450Target out[3], uint8_t& count) const;
    bool isConnected() const;
    RadarHealth getHealth() const;
    String getRadarMAC() const { return _radarMAC; }

    // Configuration
    void setBaudRate(uint32_t baud);
    bool enableMultiTargetTracking(bool enable);
    bool setZoneConfig(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY);

    // Enables/disables the BLE radio inside the LD2450 module itself (cmd 0xA4 + restart 0xA3).
    // Recommended: disable for security; it reduces the attack surface (the module
    // otherwise exposes an unauthenticated GATT configuration interface). The change
    // persists in the module's NVS.
    bool setBluetoothEnabled(bool enable);

    // Native region filter (cmd 0xC2). The module stores 3 rectangular zones in NVM
    // and, depending on the mode, either reports only targets inside (detect-only)
    // or filters targets inside out (exclude). Hardware-side filtering: targets are
    // dropped before being sent over UART. Complementary to the SW polygons.
    //
    // Wire format (per ESPHome ld2450 production code):
    //   header  FD FC FB FA
    //   length  1C 00          (28 = 2 cmd + 26 payload)
    //   cmd     C2 00          (little-endian)
    //   payload [mode_lo, mode_hi=0x00] + 3x zone (x1_lo,x1_hi,y1_lo,y1_hi,
    //                                              x2_lo,x2_hi,y2_lo,y2_hi)
    //   footer  04 03 02 01
    // Coordinates: int16 little-endian (val & 0xFFFF). The change persists in the module NVM.
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

    // RAD-01: serialize configuration transactions (Enable/Cmd/End sequences)
    // against each other and pause the RX parser while a transaction owns the
    // UART, so command ACKs are never consumed as telemetry and two callers
    // (web region filter, BLE bluetooth toggle, ...) never interleave frames.
    SemaphoreHandle_t _cmdMutex = nullptr;
    volatile bool _configActive = false;
    bool beginConfigTxn(uint32_t waitMs = 500);
    void endConfigTxn();
    // Send one wrapped config command and verify the module ACK status.
    // cmdWord is the little-endian command byte (e.g. 0x90). Returns true only
    // when an ACK frame for that command reports success (status word == 0).
    bool sendConfigCommand(const uint8_t* cmd, size_t cmdLen, uint8_t cmdWord,
                           uint32_t ackTimeoutMs = 250);
    bool readAck(uint8_t cmdWord, uint32_t timeoutMs);

    // Dedicated UART task
    TaskHandle_t _taskHandle = nullptr;
    volatile bool _taskRunning = false;
    static void radarTaskFn(void* arg);
    void serviceOnce();  // radar task body: read+parse+timeout check

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
