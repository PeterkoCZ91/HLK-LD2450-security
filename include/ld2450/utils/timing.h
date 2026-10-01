#pragma once
// Wraparound-safe millis() helpers (no Arduino deps, host-testable).
#include <stdint.h>

namespace ld2450_timing {

// True when `dur` ms elapsed since `start`. If `now` was sampled BEFORE another
// task set `start` (start is "in the future" by a small amount), returns false
// instead of letting the unsigned difference wrap to a huge value.
inline bool elapsedAtLeast(uint32_t now, uint32_t start, uint32_t dur) {
    int32_t d = (int32_t)(now - start);
    if (d < 0) return false;
    return (uint32_t)d >= dur;
}

// Cooldown gate where last == 0 means "never fired": first event right after
// boot must not be suppressed by now - 0 < cooldown.
inline bool cooldownElapsed(uint32_t now, uint32_t last, uint32_t cooldown) {
    if (last == 0) return true;
    return (uint32_t)(now - last) > cooldown;
}

}  // namespace ld2450_timing
