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

    // Thread-safe data access (non-blocking mutex, safe from any core)
    uint8_t getTargetCount() const;
    LD2450Target getTarget(uint8_t index) const;
    bool isConnected() const;
    RadarHealth getHealth() const;

    // Configuration
    void setBaudRate(uint32_t baud);
    bool enableMultiTargetTracking(bool enable);
    bool setZoneConfig(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY);

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

    // Internal parsing
    void readIntoRing();
    bool parseFrame();
    size_t ringAvailable() const;
    uint8_t ringPeek(size_t offset) const;
    void ringAdvance(size_t count);
};

#endif // LD2450_SERVICE_H
