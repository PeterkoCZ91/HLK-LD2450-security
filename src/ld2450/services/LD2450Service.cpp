#include "services/LD2450Service.h"
#include "ld2450/utils/ld2450_frame.h"
#include <cstring>

// Pure parsing helpers live in ld2450_frame.h (shared with native unit tests).
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
    _cmdMutex = xSemaphoreCreateMutex();
    if (!_cmdMutex) {
        Serial.println("[LD2450] FATAL: cmd mutex create failed (low heap)");
        return false;
    }
    _health.lastFrameTime = millis();
    _health.connected = false;

    Serial.println("[LD2450] UART initialized at 256000 baud");
    Serial.printf("[LD2450] RX=%d, TX=%d\n", _rxPin, _txPin);

    delay(500);
    queryMAC();  // best-effort; failure leaves _radarMAC empty
    enableMultiTargetTracking(true);
    return true;
}

bool LD2450Service::queryMAC() {
    if (!_serial) return false;
    // LD2450 protocol: Get MAC: cmd=0xA5 + payload 0x0001, the ACK returns a 6-byte MAC.
    // Called only from begin() before the task starts, so no mutex/contention.
    while (_serial->available()) _serial->read();  // drain stale data

    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t getMacCmd[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA5, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t endConfig[]    = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};

    _serial->write(enableConfig, sizeof(enableConfig));
    delay(60);
    _serial->write(getMacCmd, sizeof(getMacCmd));

    // Collect the ACK into a buffer for ~250 ms (ACK frame is 20 B, but the module may send extra tail bytes).
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

    // ACK format: FD FC FB FA | 0A 00 | A5 01 | status(2) | MAC(6) | 04 03 02 01  = 20 B
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

// --- Config transaction helpers (RAD-01) ---
//
// A configuration transaction takes _cmdMutex (serializing all callers), then
// raises _configActive so the RX task's serviceOnce() stops touching the UART.
// The transaction owns the port for the Enable/Cmd/End sequence and reads the
// module ACKs itself; telemetry parsing resumes cleanly on endConfigTxn().

static const uint8_t LD_ENABLE_CFG[] = {0xFD,0xFC,0xFB,0xFA,0x04,0x00,0xFF,0x00,0x01,0x00,0x04,0x03,0x02,0x01};
static const uint8_t LD_END_CFG[]    = {0xFD,0xFC,0xFB,0xFA,0x02,0x00,0xFE,0x00,0x04,0x03,0x02,0x01};

bool LD2450Service::readAck(uint8_t cmdWord, uint32_t timeoutMs) {
    if (!_serial) return false;
    uint8_t buf[128];
    size_t pos = 0;
    unsigned long start = millis();
    while (millis() - start < timeoutMs && pos < sizeof(buf)) {
        while (_serial->available() && pos < sizeof(buf)) buf[pos++] = _serial->read();
        delay(5);
    }
    // ACK: FD FC FB FA | len(2) | cmd 0x01 | status(2) | ... | 04 03 02 01
    for (size_t i = 0; i + 10 <= pos; i++) {
        if (buf[i]==0xFD && buf[i+1]==0xFC && buf[i+2]==0xFB && buf[i+3]==0xFA &&
            buf[i+6]==cmdWord && buf[i+7]==0x01) {
            // status word (little-endian) at [8],[9]; 0 == success
            bool ok = (buf[i+8]==0x00 && buf[i+9]==0x00);
            if (!ok) Serial.printf("[LD2450] cmd 0x%02X negative ACK (status %02X%02X)\n",
                                   cmdWord, buf[i+9], buf[i+8]);
            return ok;
        }
    }
    Serial.printf("[LD2450] cmd 0x%02X: no ACK within %ums\n", cmdWord, (unsigned)timeoutMs);
    return false;
}

bool LD2450Service::beginConfigTxn(uint32_t waitMs) {
    if (!_serial || !_cmdMutex) return false;
    if (xSemaphoreTake(_cmdMutex, pdMS_TO_TICKS(waitMs)) != pdTRUE) {
        Serial.println("[LD2450] config busy: another transaction in progress");
        return false;
    }
    _configActive = true;
    delay(10);  // let any in-flight serviceOnce() finish before we own the UART
    while (_serial->available()) _serial->read();  // drain stale telemetry
    _serial->write(LD_ENABLE_CFG, sizeof(LD_ENABLE_CFG));
    bool ok = readAck(0xFF, 200);
    if (!ok) {
        // Module did not enter config mode; abort the transaction cleanly.
        _configActive = false;
        xSemaphoreGive(_cmdMutex);
    }
    return ok;
}

void LD2450Service::endConfigTxn() {
    if (_serial) {
        _serial->write(LD_END_CFG, sizeof(LD_END_CFG));
        delay(30);
        while (_serial->available()) _serial->read();  // drain End-Config ACK + tail
    }
    _configActive = false;
    if (_cmdMutex) xSemaphoreGive(_cmdMutex);
}

bool LD2450Service::sendConfigCommand(const uint8_t* cmd, size_t cmdLen,
                                      uint8_t cmdWord, uint32_t ackTimeoutMs) {
    if (!_serial) return false;
    _serial->write(cmd, cmdLen);
    return readAck(cmdWord, ackTimeoutMs);
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
                // Valid frame! Copy 30 B from the ring (may wrap around) and parse.
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

                // Update shared state: short timeout (5 ms) instead of non-blocking,
                // otherwise a web/diag query would silently drop frames and cause a phantom 2 s timeout.
                if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    memcpy(_targets, tmp, sizeof(tmp));
                    _targetCount = count;
                    _health.framesGood++;
                    _health._fpsCounter++;
                    _health.lastFrameTime = millis();
                    _health.connected = true;
                    xSemaphoreGive(_mutex);
                } else {
                    // Reader held the mutex for >5 ms (likely a long HTTP serialization).
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
    // RAD-01: a config transaction owns the UART — do not read/parse, or we
    // would swallow command ACKs and desync the ring buffer.
    if (_configActive) return;

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
    // If the dedicated task is running, the main loop has nothing to do
    if (_taskRunning) return;
    serviceOnce();
}

void LD2450Service::radarTaskFn(void* arg) {
    auto* self = static_cast<LD2450Service*>(arg);
    Serial.printf("[LD2450] Radar task started on core %d\n", xPortGetCoreID());
    while (self->_taskRunning) {
        self->serviceOnce();
        // 200 Hz polling: radar sends ~10-30 fps, a 30 B frame @ 256 kBaud takes ~1.2 ms
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
    // the task finishes on its own at vTaskDelay, then vTaskDelete
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

void LD2450Service::getSnapshot(LD2450Target out[3], uint8_t& count) const {
    // RAD-02: copy the whole frame under one lock so position/speed/resolution
    // of every slot come from the same radar frame.
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (int i = 0; i < 3; i++) out[i] = _targets[i];
        count = _targetCount;
        xSemaphoreGive(_mutex);
        return;
    }
    // Fallback: best-effort stale copy (still one pass, never blocks the caller).
    for (int i = 0; i < 3; i++) out[i] = _targets[i];
    count = _targetCount;
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
    // LD2450 protocol: 0x90 = single-target mode, 0x91 = multi-target mode (up to 3 targets)
    // Serialized config transaction with ACK verification (RAD-01).
    uint8_t modeCmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x90, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t modeWord = 0x90;
    if (enable) { modeCmd[6] = 0x91; modeWord = 0x91; }

    if (!beginConfigTxn()) return false;
    bool ok = sendConfigCommand(modeCmd, sizeof(modeCmd), modeWord);
    endConfigTxn();
    Serial.printf("[LD2450] Tracking mode set: %s (%s)\n",
                  enable ? "MULTI" : "SINGLE", ok ? "ACK" : "no/neg ACK");
    return ok;
}

bool LD2450Service::setZoneConfig(int16_t minX, int16_t maxX, int16_t minY, int16_t maxY) {
    (void)minX; (void)maxX; (void)minY; (void)maxY;
    return false; // Stub - zone filtering is done in software
}

bool LD2450Service::setBluetoothEnabled(bool enable) {
    // LD2450 protocol: Set BT: cmd 0xA4 + payload (0x0001 = on, 0x0000 = off).
    // The change takes effect after a module restart (cmd 0xA3). Serialized txn (RAD-01).
    uint8_t btCmd[]      = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
    uint8_t restartCmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA3, 0x00, 0x04, 0x03, 0x02, 0x01};
    if (enable) btCmd[8] = 0x01;  // payload byte 0 = 0x01

    if (!beginConfigTxn()) return false;
    bool ok = sendConfigCommand(btCmd, sizeof(btCmd), 0xA4);
    // Restart module so the BT change takes effect. The module reboots, so the
    // restart ACK is best-effort; endConfigTxn drains the boot tail.
    _serial->write(restartCmd, sizeof(restartCmd));
    delay(60);
    endConfigTxn();
    Serial.printf("[LD2450] Bluetooth radaru: %s (%s, restart)\n",
                  enable ? "ON" : "OFF", ok ? "ACK" : "no/neg ACK");
    return ok;
}

bool LD2450Service::setRegionFilter(const RegionFilter& cfg) {
    if (!_serial) return false;
    // LD2450 protocol: Set Zone Filter (cmd 0xC2). Wire format verified against
    // ESPHome production kódu (esphome/components/ld2450/ld2450.cpp,
    // send_set_zone_command_ + convert_int_values_to_hex).
    //
    // Frame layout (38 B celkem):
    //   FD FC FB FA | 1C 00 | C2 00 | mode_lo mode_hi(=00) | zone0(8B) | zone1(8B) | zone2(8B) | 04 03 02 01
    //   payload length = 0x1C = 28 (2 cmd + 26 data)
    //   coordinates: int16 little-endian, ESPHome writes (val & 0xFFFF) directly.
    //
    // Mode 0=disabled (zones ignored), 1=detection (inside only), 2=filter/exclude.

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
    // 3 zones x 4 coordinates (int16 little-endian)
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

    // Debug log: print all 38 bytes as hex
    Serial.printf("[Radar] Region filter: mode=%u, sending %u-byte frame: ",
                  cfg.mode, (unsigned)sizeof(frame));
    for (size_t i = 0; i < sizeof(frame); i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();

    // Wrap: Enable Config, 0xC2, End Config (serialized txn, ACK-checked).
    if (!beginConfigTxn()) return false;
    bool ok = sendConfigCommand(frame, sizeof(frame), 0xC2);
    endConfigTxn();
    Serial.printf("[Radar] Region filter applied (mode=%u, %s)\n",
                  cfg.mode, ok ? "ACK" : "no/neg ACK");
    return ok;
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
    // LD2450 factory reset (cmd 0xA2), serialized config transaction (RAD-01).
    uint8_t factoryCmd[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0xA2, 0x00, 0x04, 0x03, 0x02, 0x01};
    if (!beginConfigTxn()) { Serial.println("[LD2450] Factory reset: config busy"); return; }
    bool ok = sendConfigCommand(factoryCmd, sizeof(factoryCmd), 0xA2);
    endConfigTxn();
    Serial.printf("[LD2450] Factory reset command sent (%s)\n", ok ? "ACK" : "no/neg ACK");
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
