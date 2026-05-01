#include "services/LD2450Service.h"
#include "ld2450/utils/ld2450_frame.h"
#include <cstring>

// Pure parsing helpers v ld2450_frame.h — sdílené s native unit testy.
static const size_t FRAME_SIZE = LD2450Frame::FRAME_SIZE;

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
    if (!_mutex) {
        Serial.println("[LD2450] FATAL: mutex create failed (low heap)");
        return false;
    }
    _health.lastFrameTime = millis();
    _health.connected = false;

    Serial.println("[LD2450] UART initialized at 256000 baud");
    Serial.printf("[LD2450] RX=%d, TX=%d\n", _rxPin, _txPin);

    delay(500);
    queryMAC();  // best-effort; failure → _radarMAC zůstane prázdný
    enableMultiTargetTracking(true);
    return true;
}

bool LD2450Service::queryMAC() {
    if (!_serial) return false;
    // LD2450 protokol — Get MAC: cmd=0xA5 + payload 0x0001, ACK vrátí 6B MAC.
    // Volá se pouze v begin() před spuštěním tasku → bez mutex/contention.
    while (_serial->available()) _serial->read();  // drain stale data

    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t getMacCmd[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA5, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t endConfig[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};

    _serial->write(enableConfig, sizeof(enableConfig));
    delay(60);
    _serial->write(getMacCmd, sizeof(getMacCmd));

    // Sběr ACK do bufferu po ~250ms (ACK rámec je 20B, ale modul může poslat víc tail bytů).
    uint8_t buf[128];
    size_t pos = 0;
    unsigned long start = millis();
    while (millis() - start < 250 && pos < sizeof(buf)) {
        while (_serial->available() && pos < sizeof(buf)) buf[pos++] = _serial->read();
        delay(5);
    }

    _serial->write(endConfig, sizeof(endConfig));
    delay(60);
    while (_serial->available()) _serial->read();  // drain End-Config ACK + tail

    // ACK formát: FD FC FB FA | 0A 00 | A5 01 | status(2) | MAC(6) | 04 03 02 01  →  20B
    for (size_t i = 0; i + 20 <= pos; i++) {
        if (buf[i] == 0xFD && buf[i+1] == 0xFC && buf[i+2] == 0xFB && buf[i+3] == 0xFA &&
            buf[i+6] == 0xA5 && buf[i+7] == 0x01 &&
            buf[i+16] == 0x04 && buf[i+17] == 0x03 && buf[i+18] == 0x02 && buf[i+19] == 0x01) {
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     buf[i+10], buf[i+11], buf[i+12], buf[i+13], buf[i+14], buf[i+15]);
            _radarMAC = mac;
            Serial.printf("[LD2450] Radar MAC: %s\n", mac);
            return true;
        }
    }
    Serial.println("[LD2450] MAC query: no valid ACK received");
    return false;
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
        uint8_t hdr[4] = { ringPeek(0), ringPeek(1), ringPeek(2), ringPeek(3) };
        if (LD2450Frame::hasHeader(hdr)) {

            // Check footer at offset 28-29
            uint8_t ftr[2] = { ringPeek(LD2450Frame::FOOTER_OFFSET), ringPeek(LD2450Frame::FOOTER_OFFSET + 1) };
            if (LD2450Frame::hasFooter(ftr)) {
                // Valid frame! Skopíruj 30B z ringu (potenciálně přes hranici) a parsuj.
                uint8_t frame[LD2450Frame::FRAME_SIZE];
                for (size_t k = 0; k < LD2450Frame::FRAME_SIZE; k++) frame[k] = ringPeek(k);

                LD2450Frame::ParsedTarget pt[3];
                uint8_t count = LD2450Frame::parseTargets(frame, pt);

                LD2450Target tmp[3];
                for (int i = 0; i < 3; i++) {
                    tmp[i].x = pt[i].x;
                    tmp[i].y = pt[i].y;
                    tmp[i].speed = pt[i].speed;
                    tmp[i].resolution = pt[i].resolution;
                    tmp[i].valid = pt[i].valid;
                }

                // Update shared state — krátký timeout (5ms) místo non-blocking,
                // jinak bychom při web/diag dotazu tiše zahazovali framy → phantom timeout 2s.
                if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    memcpy(_targets, tmp, sizeof(tmp));
                    _targetCount = count;
                    _health.framesGood++;
                    _health._fpsCounter++;
                    _health.lastFrameTime = millis();
                    _health.connected = true;
                    xSemaphoreGive(_mutex);
                } else {
                    // Reader držel mutex >5ms — pravděpodobné dlouhé HTTP serializace.
                    _health.bufferOverflows++;
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

void LD2450Service::serviceOnce() {
    if (!_serial) return;

    readIntoRing();
    while (parseFrame()) { /* keep parsing */ }

    // FPS calculation (once per second)
    unsigned long now = millis();
    if (now - _health._fpsLastCalc >= 1000) {
        _health.fps = _health._fpsCounter * 1000.0f / (now - _health._fpsLastCalc);
        _health._fpsCounter = 0;
        _health._fpsLastCalc = now;
    }

    // Timeout check
    if (now - _health.lastFrameTime > 2000) {
        if (_health.connected) {
            Serial.println("[LD2450] Connection Lost (Timeout)");
        }
        if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            _health.connected = false;
            _targetCount = 0;
            for (auto &t : _targets) t.valid = false;
            xSemaphoreGive(_mutex);
        }
    }
}

void LD2450Service::update() {
    // Pokud běží dedikovaný task, hlavní smyčka už nemá co dělat
    if (_taskRunning) return;
    serviceOnce();
}

void LD2450Service::radarTaskFn(void* arg) {
    auto* self = static_cast<LD2450Service*>(arg);
    Serial.printf("[LD2450] Radar task started on core %d\n", xPortGetCoreID());
    while (self->_taskRunning) {
        self->serviceOnce();
        // 200 Hz polling — radar posílá ~10-30 fps, frame je 30 B @ 256 kBaud ≈ 1.2 ms
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    Serial.println("[LD2450] Radar task stopped");
    vTaskDelete(nullptr);
}

bool LD2450Service::startTask(uint32_t stackSize, UBaseType_t priority, BaseType_t coreId) {
    if (_taskRunning) return true;
    _taskRunning = true;
    BaseType_t res = xTaskCreatePinnedToCore(radarTaskFn, "ld2450_rx", stackSize, this, priority, &_taskHandle, coreId);
    if (res != pdPASS) {
        _taskRunning = false;
        Serial.println("[LD2450] FATAL: radar task create failed");
        return false;
    }
    return true;
}

void LD2450Service::stopTask() {
    _taskRunning = false;
    // task se sám dokončí na vTaskDelay → vTaskDelete
    _taskHandle = nullptr;
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

bool LD2450Service::enableMultiTargetTracking(bool enable) {
    if (!_serial) return false;
    // LD2450 protokol: 0x90 = single-target mode, 0x91 = multi-target mode (až 3 cíle)
    // Vše musí být zabaleno v Enable/End Config rámcích.
    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t modeCmd[]      = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x90, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t endConfig[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    if (enable) modeCmd[6] = 0x91;  // multi-target

    _serial->write(enableConfig, sizeof(enableConfig));
    delay(50);
    _serial->write(modeCmd, sizeof(modeCmd));
    delay(50);
    _serial->write(endConfig, sizeof(endConfig));
    Serial.printf("[LD2450] Tracking mode set: %s\n", enable ? "MULTI" : "SINGLE");
    return true;
}

bool LD2450Service::setZoneConfig(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY) {
    (void)minX; (void)maxX; (void)minY; (void)maxY;
    return false; // Stub - zone filtering is done in software
}

bool LD2450Service::setBluetoothEnabled(bool enable) {
    if (!_serial) return false;
    // LD2450 protokol — Set BT: cmd 0xA4 + payload (0x0001 = on, 0x0000 = off).
    // Změna se projeví po restartu modulu (cmd 0xA3). Persistuje v NVS modulu.
    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t btCmd[]        = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t restartCmd[]   = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA3, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t endConfig[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    if (enable) btCmd[8] = 0x01;  // payload byte 0 → 0x01

    _serial->write(enableConfig, sizeof(enableConfig));
    delay(60);
    _serial->write(btCmd, sizeof(btCmd));
    delay(60);
    _serial->write(endConfig, sizeof(endConfig));
    delay(80);
    // Restart radaru aby se BT změna projevila
    _serial->write(enableConfig, sizeof(enableConfig));
    delay(60);
    _serial->write(restartCmd, sizeof(restartCmd));
    delay(60);
    while (_serial->available()) _serial->read();  // drain ACK + boot tail
    Serial.printf("[LD2450] Bluetooth radaru: %s (radar restartován)\n", enable ? "ON" : "OFF");
    return true;
}

bool LD2450Service::setRegionFilter(const RegionFilter& cfg) {
    if (!_serial) return false;
    // LD2450 protokol — Set Zone Filter (cmd 0xC2). Wire format ověřený proti
    // ESPHome production kódu (esphome/components/ld2450/ld2450.cpp,
    // send_set_zone_command_ + convert_int_values_to_hex).
    //
    // Frame layout (38 B celkem):
    //   FD FC FB FA | 1C 00 | C2 00 | mode_lo mode_hi(=00) | zone0(8B) | zone1(8B) | zone2(8B) | 04 03 02 01
    //   payload length = 0x1C = 28 (2 cmd + 26 data)
    //   souřadnice: int16 little-endian, ESPHome zapisuje (val & 0xFFFF) přímo.
    //
    // Mode 0=disabled (zóny ignorované), 1=detection (jen uvnitř), 2=filter/exclude.

    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t endConfig[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};

    // Build the 0xC2 frame
    uint8_t frame[38];
    size_t p = 0;
    // Header
    frame[p++] = 0xFD; frame[p++] = 0xFC; frame[p++] = 0xFB; frame[p++] = 0xFA;
    // Length (little-endian) = 28
    frame[p++] = 0x1C; frame[p++] = 0x00;
    // Command word (little-endian) = 0xC2
    frame[p++] = 0xC2; frame[p++] = 0x00;
    // Payload: mode (2B, low byte = mode, high byte = 0x00)
    frame[p++] = (uint8_t)(cfg.mode & 0xFF);
    frame[p++] = 0x00;
    // 3 zóny × 4 souřadnice (int16 little-endian)
    for (uint8_t z = 0; z < 3; z++) {
        int16_t coords[4] = { cfg.x1[z], cfg.y1[z], cfg.x2[z], cfg.y2[z] };
        for (uint8_t i = 0; i < 4; i++) {
            uint16_t v = (uint16_t)(coords[i]) & 0xFFFF;  // ESPHome: raw int16 LE
            frame[p++] = (uint8_t)(v & 0xFF);
            frame[p++] = (uint8_t)((v >> 8) & 0xFF);
        }
    }
    // Footer
    frame[p++] = 0x04; frame[p++] = 0x03; frame[p++] = 0x02; frame[p++] = 0x01;

    // Sanity check (compile-time-ish)
    if (p != sizeof(frame)) {
        Serial.printf("[Radar] Region filter: frame size mismatch %u vs %u\n",
                      (unsigned)p, (unsigned)sizeof(frame));
        return false;
    }

    // Debug log — vytiskne všech 38 bajtů hex
    Serial.printf("[Radar] Region filter: mode=%u, sending %u-byte frame: ",
                  cfg.mode, (unsigned)sizeof(frame));
    for (size_t i = 0; i < sizeof(frame); i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();

    // Wrap: Enable Config → 0xC2 → End Config (žádný restart, není destruktivní)
    _serial->write(enableConfig, sizeof(enableConfig));
    delay(50);
    _serial->write(frame, sizeof(frame));
    delay(50);
    _serial->write(endConfig, sizeof(endConfig));
    delay(20);
    Serial.printf("[Radar] Region filter applied (mode=%u)\n", cfg.mode);
    return true;
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
    if (!_serial) return;
    // LD2450 protocol: konfigurace musí být zabalená v Enable Config / End Config rámcích,
    // jinak modul command zahodí.
    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t factoryCmd[]   = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA2, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t endConfig[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
    _serial->write(enableConfig, sizeof(enableConfig));
    delay(50);
    _serial->write(factoryCmd, sizeof(factoryCmd));
    delay(50);
    _serial->write(endConfig, sizeof(endConfig));
    Serial.println("[LD2450] Factory reset command sent");
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
