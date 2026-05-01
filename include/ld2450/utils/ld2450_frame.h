#pragma once
#include <stdint.h>
#include <stddef.h>

// Pure helpers pro LD2450 frame parsing — bez závislosti na Arduino API.
// Použití: LD2450Service v běhu na ESP32, native unit testy v PIO native env.
//
// LD2450 Data Frame: 30 bytes
//   Header  : AA FF 03 00 (4 bytes)
//   Target 1: 8 bytes
//   Target 2: 8 bytes
//   Target 3: 8 bytes
//   Footer  : 55 CC (2 bytes)
// Per-target layout (8 B):
//   X (2B, sign-magnitude — bit15=1 means positive)
//   Y (2B, sign-magnitude)
//   Speed (2B, sign-magnitude, cm/s)
//   Resolution (2B, unsigned)

namespace LD2450Frame {

constexpr size_t FRAME_SIZE     = 30;
constexpr size_t HEADER_LEN     = 4;
constexpr size_t FOOTER_LEN     = 2;
constexpr size_t FOOTER_OFFSET  = 28;
constexpr size_t TARGET_OFFSET  = 4;
constexpr size_t TARGET_SIZE    = 8;

// Sanity bounds — LD2450 reálný rozsah ~±6m laterálně, 8m hloubkově, max ~10 m/s.
// Hodnoty mimo => zahodit jako noise.
constexpr int16_t SANE_X_MIN = -6500;
constexpr int16_t SANE_X_MAX =  6500;
constexpr int16_t SANE_Y_MIN =     0;
constexpr int16_t SANE_Y_MAX =  8500;
constexpr int16_t SANE_SPEED_MIN = -1000;
constexpr int16_t SANE_SPEED_MAX =  1000;

struct ParsedTarget {
    int16_t  x;
    int16_t  y;
    int16_t  speed;
    uint16_t resolution;
    bool     valid;
};

// Sign-magnitude decode: bit 15 = sign (1=pozitivní, 0=negativní), spodních 15 bitů = magnituda.
// Příklady: 0x8064 -> +100, 0x0064 -> -100, 0x8000 -> +0, 0x0000 -> 0.
static inline int16_t decodeSignMag(uint16_t raw) {
    return (raw & 0x8000) ? (int16_t)(raw - 0x8000) : -(int16_t)raw;
}

static inline bool hasHeader(const uint8_t* p) {
    return p[0] == 0xAA && p[1] == 0xFF && p[2] == 0x03 && p[3] == 0x00;
}

static inline bool hasFooter(const uint8_t* p) {
    return p[0] == 0x55 && p[1] == 0xCC;
}

// Parsuj 30B frame na 3 cíle. Vstup musí ukazovat na začátek validního framu.
// Vrátí počet cílů s `valid=true` (po sanity-checku).
static inline uint8_t parseTargets(const uint8_t* frame30, ParsedTarget out[3]) {
    uint8_t count = 0;
    for (int i = 0; i < 3; i++) {
        const uint8_t* p = frame30 + TARGET_OFFSET + i * TARGET_SIZE;
        uint16_t raw_x = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        uint16_t raw_y = (uint16_t)p[2] | ((uint16_t)p[3] << 8);
        uint16_t raw_s = (uint16_t)p[4] | ((uint16_t)p[5] << 8);
        uint16_t res   = (uint16_t)p[6] | ((uint16_t)p[7] << 8);
        int16_t x = decodeSignMag(raw_x);
        int16_t y = decodeSignMag(raw_y);
        int16_t s = decodeSignMag(raw_s);
        bool sane = (res != 0)
                 && (x >= SANE_X_MIN && x <= SANE_X_MAX)
                 && (y >= SANE_Y_MIN && y <= SANE_Y_MAX)
                 && (s >= SANE_SPEED_MIN && s <= SANE_SPEED_MAX);
        out[i].x = x;
        out[i].y = y;
        out[i].speed = s;
        out[i].resolution = res;
        out[i].valid = sane;
        if (sane) count++;
    }
    return count;
}

} // namespace LD2450Frame
