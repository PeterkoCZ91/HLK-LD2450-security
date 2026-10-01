#include "ld2450/services/MQTTOfflineBuffer.h"
#include "ld2450/utils/crc32.h"
#include <time.h>

// Integrity metadata for the buffer file — shared IEEE CRC-32 (host-tested).
static inline uint32_t ofbCrc32(const uint8_t* data, size_t len) {
    return ld2450_crc::crc32(data, len);
}

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

bool MQTTOfflineBuffer::push(const char* topic, const char* payload, bool retained) {
    // Never store truncated records (could be invalid JSON / wrong topic).
    if (!ld2450_mqttbuf::fits(topic, sizeof(_buf[0].topic)) ||
        !ld2450_mqttbuf::fits(payload, sizeof(_buf[0].payload))) {
        Serial.println("[MQTTOfB] Skip buffering: topic/payload too long");
        return false;
    }
    MQTTBufferedMsg msg;
    memset(&msg, 0, sizeof(msg));
    time_t epoch = time(nullptr);
    msg.timestamp = (epoch > 1700000000) ? (uint32_t)epoch : millis() / 1000;
    strncpy(msg.topic,   topic,   sizeof(msg.topic)   - 1);  msg.topic[sizeof(msg.topic)   - 1] = '\0';
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);  msg.payload[sizeof(msg.payload) - 1] = '\0';
    msg.retained = retained ? 1 : 0;
    msg.qos = 0;

    // Retained = state: keep only the latest per topic so replay can't
    // overwrite newer state with a stale one.
    if (retained) {
        int dup = ld2450_mqttbuf::findRetained(_buf, MQTT_OFB_CAPACITY, _head, _count, topic);
        if (dup >= 0) {
            _buf[dup] = msg;
            _dirty = true;
            return true;
        }
    }

    size_t idx = (_head + _count) % MQTT_OFB_CAPACITY;
    if (_count < MQTT_OFB_CAPACITY) {
        _buf[idx] = msg;
        _count++;
    } else {
        _buf[_head] = msg;
        _head = (_head + 1) % MQTT_OFB_CAPACITY;
    }

    _dirty = true;  // saveToDisk() runs from update() at most once per 30 s
    Serial.printf("[MQTTOfB] Buffered [%u/%u]: %s%s\n", _count, MQTT_OFB_CAPACITY,
                  topic, retained ? " (retained)" : "");
    return true;
}

bool MQTTOfflineBuffer::peek(char* topic, size_t topicLen, char* payload, size_t payloadLen,
                             bool* retained) const {
    if (_count == 0) return false;
    const MQTTBufferedMsg& msg = _buf[_head % MQTT_OFB_CAPACITY];
    strncpy(topic,   msg.topic,   topicLen   - 1);  topic[topicLen   - 1] = '\0';
    strncpy(payload, msg.payload, payloadLen - 1);  payload[payloadLen - 1] = '\0';
    if (retained) *retained = (msg.retained != 0);
    return true;
}

void MQTTOfflineBuffer::consume() {
    if (_count == 0) return;
    _head = (_head + 1) % MQTT_OFB_CAPACITY;
    _count--;
    _dirty = true;
}

void MQTTOfflineBuffer::update() {
    if (!_dirty) return;
    if (millis() - _lastSave < SAVE_INTERVAL_MS) return;
    // MQTT-01: keep _dirty set if the write failed so the next tick retries
    // instead of silently dropping buffered records.
    bool ok = saveToDisk();
    _lastSave = millis();
    if (ok) _dirty = false;
}

void MQTTOfflineBuffer::flushNow() {
    if (!_dirty) return;
    if (saveToDisk()) _dirty = false;
    _lastSave = millis();
}

void MQTTOfflineBuffer::loadFromDisk() {
    // Crash recovery: power loss between remove and rename leaves only .tmp
    // (fully written; CRC below still validates it).
    String tmpName = String(_filename) + ".tmp";
    if (!LittleFS.exists(_filename) && LittleFS.exists(tmpName.c_str()))
        LittleFS.rename(tmpName.c_str(), _filename);
    if (!LittleFS.exists(_filename)) return;
    File f = LittleFS.open(_filename, "r");
    if (!f) return;

    uint32_t magic = 0; uint8_t version = 0; uint32_t storedCount = 0;
    bool hdrOk = (f.read((uint8_t*)&magic, sizeof(magic)) == sizeof(magic)) &&
                 (f.read(&version, 1) == 1) &&
                 (f.read((uint8_t*)&storedCount, sizeof(storedCount)) == sizeof(storedCount));
    if (!hdrOk || magic != OFB_MAGIC || version != OFB_VERSION) {
        Serial.println("[MQTTOfB] Buffer file bad/foreign header — ignoring");
        f.close();
        return;
    }
    if (storedCount > MQTT_OFB_CAPACITY) { f.close(); return; }

    size_t bytes = storedCount * sizeof(MQTTBufferedMsg);
    static MQTTBufferedMsg tmp[MQTT_OFB_CAPACITY];
    uint32_t storedCrc = 0;
    bool bodyOk = (f.read((uint8_t*)tmp, bytes) == (int)bytes) &&
                  (f.read((uint8_t*)&storedCrc, sizeof(storedCrc)) == sizeof(storedCrc));
    f.close();
    if (!bodyOk) { Serial.println("[MQTTOfB] Buffer file truncated — ignoring"); return; }

    uint32_t calc = ofbCrc32((uint8_t*)tmp, bytes);
    if (calc != storedCrc) {
        Serial.println("[MQTTOfB] Buffer checksum mismatch — discarding (corrupt)");
        return;
    }

    memcpy(_buf, tmp, bytes);
    _count = storedCount;
    _head  = 0;
}

bool MQTTOfflineBuffer::saveToDisk() {
    if (!_fsAvailable) return false;

    // MQTT-01: write to a temp file then atomically rename, so a power loss
    // mid-write leaves either the previous valid file or the complete new one.
    String tmp = String(_filename) + ".tmp";
    File f = LittleFS.open(tmp.c_str(), "w");
    if (!f) return false;

    // Serialize the records contiguously to compute a single CRC over them.
    static MQTTBufferedMsg lin[MQTT_OFB_CAPACITY];
    for (uint32_t i = 0; i < _count; i++)
        lin[i] = _buf[(_head + i) % MQTT_OFB_CAPACITY];
    size_t bytes = _count * sizeof(MQTTBufferedMsg);
    uint32_t crc = ofbCrc32((uint8_t*)lin, bytes);

    uint32_t magic = OFB_MAGIC; uint8_t version = OFB_VERSION;
    bool ok = (f.write((uint8_t*)&magic, sizeof(magic)) == sizeof(magic)) &&
              (f.write(&version, 1) == 1) &&
              (f.write((uint8_t*)&_count, sizeof(_count)) == sizeof(_count)) &&
              (bytes == 0 || f.write((uint8_t*)lin, bytes) == bytes) &&
              (f.write((uint8_t*)&crc, sizeof(crc)) == sizeof(crc));
    f.close();

    if (!ok) {
        LittleFS.remove(tmp.c_str());
        Serial.println("[MQTTOfB] Write error — keeping previous buffer");
        return false;
    }

    // POSIX/lfs rename replaces the target atomically. Fallback to remove+rename
    // only if the FS refuses to overwrite (boot recovery in loadFromDisk covers it).
    if (!LittleFS.rename(tmp.c_str(), _filename)) {
        LittleFS.remove(_filename);
        if (!LittleFS.rename(tmp.c_str(), _filename)) {
            Serial.println("[MQTTOfB] Rename failed — buffer not updated");
            return false;
        }
    }
    return true;
}
