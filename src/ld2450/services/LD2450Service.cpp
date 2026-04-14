#include "services/LD2450Service.h"
#include <cstring>

// LD2450 Data Frame: 30 bytes
// Header: AA FF 03 00 (4 bytes)
// Target 1-3: 8 bytes each (24 bytes total)
//   - X: 2 bytes (sign-magnitude, bit15=1 means positive)
//   - Y: 2 bytes (sign-magnitude)
//   - Speed: 2 bytes (sign-magnitude)
//   - Resolution: 2 bytes (unsigned)
// Footer: 55 CC (2 bytes)

static const size_t FRAME_SIZE = 30;

LD2450Service::LD2450Service(int8_t rxPin, int8_t txPin)
    : _rxPin(rxPin), _txPin(txPin), _serial(nullptr),
      _targetCount(0), _mutex(nullptr) {
    for (auto &t : _targets) {
        t = {0, 0, 0, 0, false};
    }
}

bool LD2450Service::begin(HardwareSerial &serial) {
    _serial = &serial;
    _serial->setRxBufferSize(2048);
    _serial->begin(256000, SERIAL_8N1, _rxPin, _txPin);

    _mutex = xSemaphoreCreateMutex();
    _health.lastFrameTime = millis();
    _health.connected = false;

    Serial.println("[LD2450] UART initialized at 256000 baud");
    Serial.printf("[LD2450] RX=%d, TX=%d\n", _rxPin, _txPin);

    delay(500);
    enableMultiTargetTracking(true);
    return true;
}

// --- Ring Buffer Helpers ---

size_t LD2450Service::ringAvailable() const {
    return (_ringHead >= _ringTail) ?
           (_ringHead - _ringTail) :
           (RING_BUF_SIZE - _ringTail + _ringHead);
}

uint8_t LD2450Service::ringPeek(size_t offset) const {
    return _ringBuf[(_ringTail + offset) % RING_BUF_SIZE];
}

void LD2450Service::ringAdvance(size_t count) {
    _ringTail = (_ringTail + count) % RING_BUF_SIZE;
}

void LD2450Service::readIntoRing() {
    int avail = _serial->available();
    if (avail <= 0) return;

    // Read in chunks, wrap around ring buffer
    while (avail > 0) {
        size_t nextHead = (_ringHead + 1) % RING_BUF_SIZE;
        if (nextHead == _ringTail) {
            // Ring buffer full - discard oldest data
            _ringTail = (_ringTail + 1) % RING_BUF_SIZE;
            _health.bufferOverflows++;
        }
        _ringBuf[_ringHead] = _serial->read();
        _ringHead = nextHead;
        avail--;
    }
}

bool LD2450Service::parseFrame() {
    // Need at least 30 bytes
    if (ringAvailable() < FRAME_SIZE) return false;

    // Search for header: AA FF 03 00
    while (ringAvailable() >= FRAME_SIZE) {
        if (ringPeek(0) == 0xAA && ringPeek(1) == 0xFF &&
            ringPeek(2) == 0x03 && ringPeek(3) == 0x00) {

            // Check footer at offset 28-29
            if (ringPeek(28) == 0x55 && ringPeek(29) == 0xCC) {
                // Valid frame! Parse targets
                uint8_t count = 0;
                LD2450Target tmp[3];

                for (int i = 0; i < 3; i++) {
                    size_t base = 4 + (i * 8);
                    uint16_t raw_x = ringPeek(base) | (ringPeek(base + 1) << 8);
                    uint16_t raw_y = ringPeek(base + 2) | (ringPeek(base + 3) << 8);
                    uint16_t raw_speed = ringPeek(base + 4) | (ringPeek(base + 5) << 8);
                    uint16_t resolution = ringPeek(base + 6) | (ringPeek(base + 7) << 8);

                    // Sign-magnitude: bit 15 set = positive, clear = negative
                    int16_t x = (raw_x & 0x8000) ? (int16_t)(raw_x - 0x8000) : -(int16_t)raw_x;
                    int16_t y = (raw_y & 0x8000) ? (int16_t)(raw_y - 0x8000) : -(int16_t)raw_y;
                    int16_t speed = (raw_speed & 0x8000) ? (int16_t)(raw_speed - 0x8000) : -(int16_t)raw_speed;

                    tmp[i].x = x;
                    tmp[i].y = y;
                    tmp[i].speed = speed;
                    tmp[i].resolution = resolution;
                    tmp[i].valid = (resolution != 0);
                    if (tmp[i].valid) count++;
                }

                // Update shared state with non-blocking mutex
                // timeout=0: if mutex busy, skip this frame (cross-core safe)
                if (xSemaphoreTake(_mutex, 0) == pdTRUE) {
                    memcpy(_targets, tmp, sizeof(tmp));
                    _targetCount = count;
                    _health.framesGood++;
                    _health._fpsCounter++;
                    _health.lastFrameTime = millis();
                    _health.connected = true;
                    xSemaphoreGive(_mutex);
                }

                _health.framesTotal++;
                ringAdvance(FRAME_SIZE);
                return true;
            } else {
                // Header ok but footer bad - corrupt frame
                _health.framesCorrupt++;
                _health.framesTotal++;
                ringAdvance(1); // Skip 1 byte, keep searching
            }
        } else {
            ringAdvance(1); // Not a header, advance
        }
    }
    return false;
}

void LD2450Service::update() {
    if (!_serial) return;

    readIntoRing();

    // Parse all complete frames
    while (parseFrame()) { /* keep parsing */ }

    // FPS calculation (once per second)
    unsigned long now = millis();
    if (now - _health._fpsLastCalc >= 1000) {
        _health.fps = _health._fpsCounter * 1000.0f / (now - _health._fpsLastCalc);
        _health._fpsCounter = 0;
        _health._fpsLastCalc = now;
    }

    // Timeout check + UART debug
    if (now - _health.lastFrameTime > 2000) {
        if (_health.connected) {
            Serial.println("[LD2450] Connection Lost (Timeout)");
        }

        if (xSemaphoreTake(_mutex, 0) == pdTRUE) {
            _health.connected = false;
            _targetCount = 0;
            for (auto &t : _targets) t.valid = false;
            xSemaphoreGive(_mutex);
        }
    }
}

// --- Thread-safe Accessors ---

uint8_t LD2450Service::getTargetCount() const {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        uint8_t count = _targetCount;
        xSemaphoreGive(_mutex);
        return count;
    }
    return _targetCount; // fallback: stale read better than blocking
}

LD2450Target LD2450Service::getTarget(uint8_t index) const {
    if (index >= 3) return {0, 0, 0, 0, false};
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        LD2450Target t = _targets[index];
        xSemaphoreGive(_mutex);
        return t;
    }
    return _targets[index]; // fallback
}

bool LD2450Service::isConnected() const {
    return _health.connected;
}

RadarHealth LD2450Service::getHealth() const {
    return _health;
}

// --- Configuration ---

void LD2450Service::setBaudRate(uint32_t baud) {
    if (_serial) _serial->updateBaudRate(baud);
}

bool LD2450Service::enableMultiTargetTracking(bool /* enable */) {
    if (!_serial) return false;
    // Enter config mode (multi-target tracking is always-on for LD2450)

    // Enable Config Mode
    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    _serial->write(enableConfig, sizeof(enableConfig));
    delay(50);

    // End Config Mode
    uint8_t endConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    _serial->write(endConfig, sizeof(endConfig));

    return true;
}

bool LD2450Service::setZoneConfig(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY) {
    (void)minX; (void)maxX; (void)minY; (void)maxY;
    return false; // Stub - zone filtering is done in software
}

// --- API Compatibility ---

String LD2450Service::getTelemetryJson() const {
    String json = "{\"type\":\"ld2450\",\"count\":";
    json += _targetCount;
    json += ",\"targets\":[";

    bool first = true;
    for (int i = 0; i < 3; i++) {
        if (_targets[i].valid) {
            if (!first) json += ",";
            json += "{\"x\":";
            json += _targets[i].x;
            json += ",\"y\":";
            json += _targets[i].y;
            json += ",\"spd\":";
            json += _targets[i].speed;
            json += ",\"res\":";
            json += _targets[i].resolution;
            json += "}";
            first = false;
        }
    }
    json += "]}";
    return json;
}

void LD2450Service::factoryReset() {
    if (_serial) {
        uint8_t cmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA2, 0x00, 0x04, 0x03, 0x02, 0x01};
        _serial->write(cmd, sizeof(cmd));
    }
}

bool LD2450Service::startAutoCalibration() { return false; }
bool LD2450Service::isCalibrating() const { return false; }
uint8_t LD2450Service::getCalibrationProgress() const { return 0; }
void LD2450Service::enableEngineeringMode(bool enable) { (void)enable; }
bool LD2450Service::isEngineeringMode() const { return false; }
void LD2450Service::setTestMode(bool enable) { (void)enable; }
void LD2450Service::getEngineeringData(uint8_t* mov, uint8_t* stat) {
    if (mov) memset(mov, 0, 9);
    if (stat) memset(stat, 0, 9);
}
