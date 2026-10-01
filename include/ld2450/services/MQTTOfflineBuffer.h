#ifndef MQTT_OFFLINE_BUFFER_H
#define MQTT_OFFLINE_BUFFER_H

#include <Arduino.h>
#include <LittleFS.h>
#include "ld2450/utils/mqtt_buffer_logic.h"

#define MQTT_OFB_CAPACITY   30
#define MQTT_OFB_TOPIC_LEN  64
#define MQTT_OFB_PAYLOAD_LEN 200

struct MQTTBufferedMsg {
    uint32_t timestamp;
    char topic[MQTT_OFB_TOPIC_LEN];
    char payload[MQTT_OFB_PAYLOAD_LEN];
    uint8_t retained;  // MQTT-01: preserve retained flag across buffering/replay
    uint8_t qos;       // reserved (QoS 0 today) — stored for forward-compatibility
    uint8_t _pad[2];
};

class MQTTOfflineBuffer {
public:
    void begin();
    // MQTT-01: retained flag is preserved so replayed records keep their
    // original delivery semantics.
    bool push(const char* topic, const char* payload, bool retained = false);
    bool hasMessages() const { return _count > 0; }
    bool peek(char* topic, size_t topicLen, char* payload, size_t payloadLen,
              bool* retained = nullptr) const;
    void consume();
    uint32_t count() const { return _count; }
    // Lazy persist: call from the main loop / MQTT update; saves at most once per SAVE_INTERVAL_MS
    void update();
    // Force an immediate write (e.g. before a restart)
    void flushNow();

private:
    void loadFromDisk();
    bool saveToDisk();  // MQTT-01: returns success so callers keep _dirty on failure

    // On-disk format (MQTT-01): [magic][version][count][records...][crc32].
    static constexpr uint32_t OFB_MAGIC = 0x4F464232;   // "OFB2"
    static constexpr uint8_t  OFB_VERSION = 1;

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
