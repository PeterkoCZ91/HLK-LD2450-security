#pragma once
#include <stdint.h>

// Pure helpers extracted from PresenceService (no Arduino deps, unit-testable).
namespace ld2450_presence {

// True when `now` has reached `deadline`, wraparound-safe for 32-bit millis().
inline bool timeReached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

// Tamper auto-clear: tamper is cleared after targets were CONTINUOUSLY visible
// for clearVisibleMs, or after no targets for clearAbsentMs.
// `visibleSince` is caller state (0 = not counting; caller stores now|1 semantics
// are avoided by using a separate `counting` flag).
struct TamperClear {
    bool     counting = false;
    uint32_t visibleSince = 0;

    // Returns true if tamper should be cleared now.
    bool update(bool tamperActive, uint8_t validCount, uint32_t now,
                uint32_t lastTargetSeen, uint32_t clearVisibleMs, uint32_t clearAbsentMs) {
        if (!tamperActive) { counting = false; return false; }
        if (validCount > 0) {
            if (!counting) { counting = true; visibleSince = now; return false; }
            if ((uint32_t)(now - visibleSince) > clearVisibleMs) { counting = false; return true; }
            return false;
        }
        counting = false;
        return (uint32_t)(now - lastTargetSeen) > clearAbsentMs;
    }
};

} // namespace ld2450_presence
