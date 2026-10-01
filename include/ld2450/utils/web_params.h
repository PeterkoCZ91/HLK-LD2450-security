#pragma once
// Pure, host-testable range/length checks for web request parameters.
// Arduino String::toInt() returns long; negative or huge values used to be cast
// straight into unsigned/uint8_t/int16_t targets (wraparound, silent truncation).

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

namespace ld2450 {

// seconds -> milliseconds, only when 0 <= s <= maxS (no unsigned wraparound).
inline bool secondsToMs(long s, long maxS, unsigned long& ms) {
    if (s < 0 || s > maxS) return false;
    ms = (unsigned long)s * 1000UL;
    return true;
}

// Index must be 0 <= v < n (a bare (uint8_t) cast maps 256 -> 0).
inline bool idxInRange(long v, long n) { return v >= 0 && v < n; }

// Radar coordinate in mm (same +-10000 bound as the region filter).
inline bool coordInRange(long v) { return v >= -10000 && v <= 10000; }

// String fits a char[cap] buffer including the terminator (no silent truncation).
inline bool fitsBuf(const char* s, size_t cap, bool allowEmpty) {
    if (!s) return false;
    size_t n = strlen(s);
    return (allowEmpty || n > 0) && n < cap;
}

// TCP port given as text: empty (= default) or all digits in 1..65535.
inline bool portTextValid(const char* s) {
    if (!s || !*s) return true;
    long v = 0;
    for (const char* p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (*p - '0');
        if (v > 65535) return false;
    }
    return v >= 1;
}

}  // namespace ld2450
