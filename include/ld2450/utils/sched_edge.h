#pragma once
// Host-testable decision helpers (no Arduino deps).
#include <stdint.h>
#include "ld2450/utils/timing.h"

namespace ld2450_sched {

// One-shot edge for a schedule minute. `minuteId` = epoch/60 (unique per real
// minute, so the same HH:MM on the next day is a new edge). Returns true only
// the first time `due` is seen in a given minute; later polls in the same
// minute return false, so a manual override made after the edge sticks.
inline bool edgeOnce(bool due, int32_t minuteId, int32_t& lastHandled) {
    if (!due || minuteId == lastHandled) return false;
    lastHandled = minuteId;
    return true;
}

// Certificate check gate: first run as soon as time is valid, then every interval.
inline bool certCheckDue(uint32_t now, uint32_t last, bool everChecked,
                         uint32_t interval, bool timeValid) {
    if (!timeValid) return false;
    if (!everChecked) return true;
    return ld2450_timing::elapsedAtLeast(now, last, interval);
}

// Retry gate: last == 0 means "not tried yet".
inline bool retryDue(uint32_t now, uint32_t last, uint32_t interval) {
    return last == 0 || ld2450_timing::elapsedAtLeast(now, last, interval);
}

}  // namespace ld2450_sched
