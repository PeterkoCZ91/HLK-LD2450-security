#include "services/EventLog.h"
#include <new>

EventLog::EventLog(size_t capacity) : _capacity(capacity), _head(0), _count(0), _dirty(false), _lastFlush(0) {
    _buffer = new (std::nothrow) LogEvent[_capacity]; if (!_buffer) _capacity = 0;
}

EventLog::~EventLog() {
    delete[] _buffer;
}

void EventLog::begin() {
    loadFromDisk();
}

void EventLog::addEvent(uint8_t type, uint16_t dist, uint8_t energy, const char* msg) {
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
    unsigned long now = millis();
    if (now - _lastFlush < 60000) return;
    if (!_dirty) return;

    saveToDisk();
    _lastFlush = now;
    _dirty = false;
}

void EventLog::flushNow() {
    if (!_dirty) return;
    saveToDisk();
    _lastFlush = millis();
    _dirty = false;
}

void EventLog::clear() {
    _head = 0;
    _count = 0;
    _dirty = true;
    LittleFS.remove(_filename);
    Serial.println("[EventLog] Cleared");
}

void EventLog::getEventsJSON(JsonDocument& doc) {
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

void EventLog::saveToDisk() {
    File f = LittleFS.open(_filename, "w");
    if (!f) {
        Serial.println("[EventLog] Failed to open file for writing");
        return;
    }

    f.write((uint8_t*)&_count, sizeof(_count));

    for (size_t i = 0; i < _count; i++) {
        size_t idx = (_head + i) % _capacity;
        f.write((uint8_t*)&_buffer[idx], sizeof(LogEvent));
    }

    f.close();
    Serial.println("[EventLog] Saved to disk");
}
