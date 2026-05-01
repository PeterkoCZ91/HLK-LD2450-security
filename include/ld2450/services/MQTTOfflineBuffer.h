#ifndef MQTT_OFFLINE_BUFFER_H
#define MQTT_OFFLINE_BUFFER_H

#include <Arduino.h>
#include <LittleFS.h>

#define MQTT_OFB_CAPACITY   30
#define MQTT_OFB_TOPIC_LEN  64
#define MQTT_OFB_PAYLOAD_LEN 200

struct MQTTBufferedMsg {
    uint32_t timestamp;
    char topic[MQTT_OFB_TOPIC_LEN];
    char payload[MQTT_OFB_PAYLOAD_LEN];
};

class MQTTOfflineBuffer {
public:
    void begin();
    void push(const char* topic, const char* payload);
    bool hasMessages() const { return _count > 0; }
    bool peek(char* topic, size_t topicLen, char* payload, size_t payloadLen) const;
    void consume();
    uint32_t count() const { return _count; }
    // Lazy persist: zavolat z hlavního loop / MQTT update, save proběhne max 1x za SAVE_INTERVAL_MS
    void update();
    // Vynutit okamžitý zápis (např. před restartem)
    void flushNow();

private:
    void loadFromDisk();
    void saveToDisk();

    MQTTBufferedMsg _buf[MQTT_OFB_CAPACITY];
    uint32_t _head  = 0;
    uint32_t _count = 0;
    bool _fsAvailable = false;
    bool _dirty = false;
    unsigned long _lastSave = 0;
    static constexpr unsigned long SAVE_INTERVAL_MS = 30000;
    const char* _filename = "/mqtt_ofb.bin";
};

#endif
