#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

// EventType enum is in types.h

struct LogEvent {
    uint32_t timestamp;      // uptime seconds
    uint8_t type;            // EventType
    uint16_t distance;       // mm (LD2450 uses mm, not cm)
    uint8_t energy;          // resolution/confidence
    char message[48];
    char isoTime[24];        // ISO 8601 timestamp (if NTP synced)
};

class EventLog {
public:
    EventLog(size_t capacity = 20);
    ~EventLog();

    void begin();
    void addEvent(uint8_t type, uint16_t dist, uint8_t energy, const char* msg);
    void flush();
    void flushNow();
    void clear();

    void getEventsJSON(JsonDocument& doc);

private:
    void loadFromDisk();
    void saveToDisk();

    LogEvent* _buffer;
    size_t _capacity;
    size_t _head;
    size_t _count;

    bool _dirty;
    unsigned long _lastFlush;
    const char* _filename = "/events.bin";
};

#endif
