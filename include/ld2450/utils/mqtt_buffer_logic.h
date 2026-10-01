#ifndef LD2450_MQTT_BUFFER_LOGIC_H
#define LD2450_MQTT_BUFFER_LOGIC_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Pure helpers for the MQTT offline ring buffer (host-tested).
namespace ld2450_mqttbuf {

// A string fits a fixed char[cap] field without truncation (room for NUL).
inline bool fits(const char* s, size_t cap) {
    return s != nullptr && strlen(s) < cap;
}

// Index of the ring slot holding a retained message for `topic`, or -1.
// Msg needs: char topic[]; uint8_t retained.
template <class Msg>
inline int findRetained(const Msg* buf, uint32_t cap, uint32_t head, uint32_t count,
                        const char* topic) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (head + i) % cap;
        if (buf[idx].retained && strcmp(buf[idx].topic, topic) == 0) return (int)idx;
    }
    return -1;
}

}  // namespace ld2450_mqttbuf

#endif
