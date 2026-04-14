#include "ld2450/services/MQTTOfflineBuffer.h"
#include <time.h>

void MQTTOfflineBuffer::begin() {
    if (!LittleFS.begin(false)) {
        Serial.println("[MQTTOfB] LittleFS not available");
        _fsAvailable = false;
        return;
    }
    _fsAvailable = true;
    loadFromDisk();
    if (_count > 0) {
        Serial.printf("[MQTTOfB] Loaded %u buffered message(s) from disk\n", _count);
    }
}

void MQTTOfflineBuffer::push(const char* topic, const char* payload) {
    MQTTBufferedMsg msg;
    time_t epoch = time(nullptr);
    msg.timestamp = (epoch > 1700000000) ? (uint32_t)epoch : millis() / 1000;
    strncpy(msg.topic,   topic,   sizeof(msg.topic)   - 1);  msg.topic[sizeof(msg.topic)   - 1] = '\0';
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);  msg.payload[sizeof(msg.payload) - 1] = '\0';

    size_t idx = (_head + _count) % MQTT_OFB_CAPACITY;
    if (_count < MQTT_OFB_CAPACITY) {
        _buf[idx] = msg;
        _count++;
    } else {
        _buf[_head] = msg;
        _head = (_head + 1) % MQTT_OFB_CAPACITY;
    }

    saveToDisk();
    Serial.printf("[MQTTOfB] Buffered [%u/%u]: %s\n", _count, MQTT_OFB_CAPACITY, topic);
}

bool MQTTOfflineBuffer::peek(char* topic, size_t topicLen, char* payload, size_t payloadLen) const {
    if (_count == 0) return false;
    const MQTTBufferedMsg& msg = _buf[_head % MQTT_OFB_CAPACITY];
    strncpy(topic,   msg.topic,   topicLen   - 1);  topic[topicLen   - 1] = '\0';
    strncpy(payload, msg.payload, payloadLen - 1);  payload[payloadLen - 1] = '\0';
    return true;
}

void MQTTOfflineBuffer::consume() {
    if (_count == 0) return;
    _head = (_head + 1) % MQTT_OFB_CAPACITY;
    _count--;
    saveToDisk();
}

void MQTTOfflineBuffer::loadFromDisk() {
    if (!LittleFS.exists(_filename)) return;
    File f = LittleFS.open(_filename, "r");
    if (!f) return;

    uint32_t storedCount = 0;
    if (f.read((uint8_t*)&storedCount, sizeof(storedCount)) != sizeof(storedCount)) { f.close(); return; }
    if (storedCount > MQTT_OFB_CAPACITY) storedCount = MQTT_OFB_CAPACITY;

    size_t bytes = storedCount * sizeof(MQTTBufferedMsg);
    if (f.read((uint8_t*)_buf, bytes) == bytes) {
        _count = storedCount;
        _head  = 0;
    }
    f.close();
}

void MQTTOfflineBuffer::saveToDisk() {
    if (!_fsAvailable) return;
    File f = LittleFS.open(_filename, "w");
    if (!f) return;

    f.write((uint8_t*)&_count, sizeof(_count));
    for (uint32_t i = 0; i < _count; i++) {
        size_t idx = (_head + i) % MQTT_OFB_CAPACITY;
        f.write((uint8_t*)&_buf[idx], sizeof(MQTTBufferedMsg));
    }
    f.close();
}
