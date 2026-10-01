#include "services/EventLog.h"
#include <new>

// CON-01: RAII lock for the EventLog mutex (no-op if creation failed).
namespace {
struct LogLock {
    SemaphoreHandle_t m; bool held;
    explicit LogLock(SemaphoreHandle_t mtx) : m(mtx), held(false) {
        if (m) held = (xSemaphoreTake(m, portMAX_DELAY) == pdTRUE);
    }
    ~LogLock() { if (m && held) xSemaphoreGive(m); }
};
}

EventLog::EventLog(size_t capacity) : _capacity(capacity), _head(0), _count(0), _dirty(false), _lastFlush(0) {
    _buffer = new (std::nothrow) LogEvent[_capacity]; if (!_buffer) _capacity = 0;
    _mutex = xSemaphoreCreateMutex();
}

EventLog::~EventLog() {
    delete[] _buffer;
    if (_mutex) vSemaphoreDelete(_mutex);
}

void EventLog::begin() {
    LogLock lock(_mutex);
    loadFromDisk();
}

void EventLog::addEvent(uint8_t type, uint16_t dist, uint8_t energy, const char* msg) {
    LogLock lock(_mutex);
    // LOG-01: buffer allocation may have failed in the constructor (_capacity forced
    // to 0). The ring-buffer modulo below would divide by zero, so fail safe instead.
    if (_capacity == 0 || _buffer == nullptr) return;

    LogEvent evt;
    evt.timestamp = millis() / 1000;
    evt.type = type;
    evt.distance = dist;
    evt.energy = energy;
    strncpy(evt.message, msg, sizeof(evt.message) - 1);
    evt.message[sizeof(evt.message) - 1] = '\0';

    // ISO timestamp if NTP synced
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
        strftime(evt.isoTime, sizeof(evt.isoTime), "%Y-%m-%dT%H:%M:%S", &ti);
    } else {
        evt.isoTime[0] = '\0';
    }

    // Ring buffer logic
    size_t index = (_head + _count) % _capacity;

    if (_count < _capacity) {
        _buffer[index] = evt;
        _count++;
    } else {
        _buffer[_head] = evt;
        _head = (_head + 1) % _capacity;
    }

    _dirty = true;
    Serial.printf("[EventLog] Added: %s\n", msg);
}

void EventLog::flush() {
    LogLock lock(_mutex);
    unsigned long now = millis();
    if (now - _lastFlush < 60000) return;
    if (!_dirty) return;

    // LOG-01: keep the dirty flag set if the write failed, so the next flush retries
    // instead of silently dropping events. _lastFlush is advanced either way to
    // rate-limit retries to once per minute.
    bool ok = saveToDisk();
    _lastFlush = now;
    if (ok) _dirty = false;
}

void EventLog::flushNow() {
    LogLock lock(_mutex);
    if (!_dirty) return;
    if (saveToDisk()) {
        _dirty = false;
    }
    _lastFlush = millis();
}

void EventLog::clear() {
    LogLock lock(_mutex);
    _head = 0;
    _count = 0;
    _dirty = true;
    LittleFS.remove(_filename);
    Serial.println("[EventLog] Cleared");
}

void EventLog::getEventsJSON(JsonDocument& doc) {
    LogLock lock(_mutex);
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < _count; i++) {
        size_t logical_idx = _count - 1 - i;
        size_t physical_idx = (_head + logical_idx) % _capacity;

        LogEvent& e = _buffer[physical_idx];

        JsonObject obj = arr.add<JsonObject>();
        obj["ts"] = e.timestamp;
        if (e.isoTime[0] != '\0') obj["time"] = e.isoTime;
        obj["type"] = e.type;
        obj["dist"] = e.distance;
        obj["en"] = e.energy;
        obj["msg"] = e.message;
    }
}

void EventLog::loadFromDisk() {
    // Crash recovery: promote an orphaned .tmp (short/partial reads are rejected below).
    String tmpName = String(_filename) + ".tmp";
    if (!LittleFS.exists(_filename) && LittleFS.exists(tmpName.c_str()))
        LittleFS.rename(tmpName.c_str(), _filename);
    if (!LittleFS.exists(_filename)) return;

    File f = LittleFS.open(_filename, "r");
    if (!f) return;

    size_t storedCount = 0;
    if (f.read((uint8_t*)&storedCount, sizeof(storedCount)) == sizeof(storedCount)) {
        if (storedCount > _capacity) storedCount = _capacity;

        size_t bytesToRead = storedCount * sizeof(LogEvent);
        if (f.read((uint8_t*)_buffer, bytesToRead) == bytesToRead) {
            _count = storedCount;
            _head = 0;
            Serial.printf("[EventLog] Loaded %d events from disk\n", _count);
        }
    }
    f.close();
}

bool EventLog::saveToDisk() {
    if (_capacity == 0 || _buffer == nullptr) return true; // nothing to persist

    // LOG-01: write to a temp file and atomically rename, so a power loss mid-write
    // cannot corrupt the existing log — either the old or the complete new file wins.
    String tmp = String(_filename) + ".tmp";
    File f = LittleFS.open(tmp.c_str(), "w");
    if (!f) {
        Serial.println("[EventLog] Failed to open temp file for writing");
        return false;
    }

    bool ok = (f.write((uint8_t*)&_count, sizeof(_count)) == sizeof(_count));
    for (size_t i = 0; ok && i < _count; i++) {
        size_t idx = (_head + i) % _capacity;
        ok = (f.write((uint8_t*)&_buffer[idx], sizeof(LogEvent)) == sizeof(LogEvent));
    }
    f.close();

    if (!ok) {
        Serial.println("[EventLog] Write error — keeping previous log");
        LittleFS.remove(tmp.c_str());
        return false;
    }

    // rename replaces the target atomically; remove+rename only as fallback
    // (boot recovery in load promotes an orphaned .tmp).
    if (!LittleFS.rename(tmp.c_str(), _filename)) {
        LittleFS.remove(_filename);
        if (!LittleFS.rename(tmp.c_str(), _filename)) {
            Serial.println("[EventLog] Rename failed — log not updated");
            return false;
        }
    }
    Serial.println("[EventLog] Saved to disk");
    return true;
}
