#pragma once
#include <cstring>
#include <cstdint>

/**
 * schedule_time - pure, host-testable helpers for HH:MM schedules (TIME-01).
 *
 * The firmware previously matched schedule times with a bare
 * `sscanf("%d:%d")`, which happily accepts "99:99", "7:5", leading/trailing
 * junk, and negative values, and never rejected an out-of-range minute. These
 * helpers validate strictly and centralise the "is this schedule due now?"
 * and midnight-crossing night-window logic so they can be unit tested without
 * hardware or a real clock.
 *
 * Timezone handling itself lives in setup (configTzTime with a configurable
 * POSIX TZ); these helpers operate purely on an already-resolved local
 * minute-of-day, so DST correctness is a property of the TZ string, tested
 * separately at integration time.
 */

namespace ld2450_time {

// Parse "HH:MM" strictly. Accepts exactly two fields separated by a single
// ':'; hours 00-23, minutes 00-59; 1- or 2-digit fields; no extra chars.
// Returns true and sets `outMinutes` (0..1439) on success.
inline bool parseHHMM(const char* s, int& outMinutes) {
    if (!s) return false;
    size_t len = strlen(s);
    if (len < 3 || len > 5) return false; // "H:MM"=4? allow "0:00"(4) "00:00"(5) "9:00"(4) "9:0"?(3->"9:0")
    int h = 0, m = 0, hDigits = 0, mDigits = 0;
    size_t i = 0;
    for (; i < len && s[i] != ':'; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        h = h * 10 + (s[i] - '0');
        if (++hDigits > 2) return false;
    }
    if (i >= len || s[i] != ':') return false; // no colon
    i++; // skip ':'
    if (i >= len) return false;                // nothing after colon
    for (; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        m = m * 10 + (s[i] - '0');
        if (++mDigits > 2) return false;
    }
    if (hDigits == 0 || mDigits == 0) return false;
    if (h > 23 || m > 59) return false;
    outMinutes = h * 60 + m;
    return true;
}

inline bool isValidHHMM(const char* s) {
    int dummy;
    return parseHHMM(s, dummy);
}

// True when `hhmm` is a valid time equal to the current local minute-of-day.
// Invalid/empty schedule strings are never "due" (fail closed for scheduling).
inline bool scheduleDue(int curMinuteOfDay, const char* hhmm) {
    int target;
    if (!parseHHMM(hhmm, target)) return false;
    return curMinuteOfDay == target;
}

// Determine whether `cur` falls inside the night window [start, end).
// Handles the normal case (start < end) and the midnight-crossing case
// (start > end, e.g. 22:00 -> 06:00). Sets `outNight` and returns true only
// when both bounds are valid; returns false (leaving outNight untouched) if
// either bound is missing/invalid so the caller can keep the day profile.
inline bool isNightNow(int cur, const char* start, const char* end, bool& outNight) {
    int s, e;
    if (!parseHHMM(start, s) || !parseHHMM(end, e)) return false;
    if (s == e) { outNight = false; return true; } // empty window
    outNight = (s < e) ? (cur >= s && cur < e)
                       : (cur >= s || cur < e);
    return true;
}

} // namespace ld2450_time
