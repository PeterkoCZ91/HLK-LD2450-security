#pragma once
#include <cstdint>
#include <cstddef>

/**
 * crc32 - IEEE CRC-32 for on-disk integrity metadata (MQTT-01 offline buffer,
 * EventLog, backups). Pure and host-testable.
 */
namespace ld2450_crc {

inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return crc;
}

} // namespace ld2450_crc
