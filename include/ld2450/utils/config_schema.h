#pragma once
#include <cstdint>
#include <cstring>

/**
 * config_schema - versioned backup/restore contract + pure range validation
 * (CFG-03, and the bounds part of SEC-07).
 *
 * The importer must validate every field against these ranges BEFORE applying
 * anything, so a malformed backup cannot partially overwrite live config. These
 * helpers are pure (no ArduinoJson / NVS) so the range logic is unit tested on
 * the host; the route layer maps JSON fields onto them.
 *
 * Secret policy: export intentionally omits secrets (MQTT/Telegram passwords,
 * WiFi credentials). Import MAY set them but never requires them. The backup is
 * therefore a non-secret core subset — documented as such.
 */

namespace ld2450_cfg {

// Current backup schema version. Bump on incompatible field changes and add a
// migration branch in the importer.
static constexpr int SCHEMA_VERSION = 1;

// A backup with a version <= ours is accepted (older = migrate up); a newer
// version is rejected because we cannot know its field semantics.
inline bool versionSupported(int v) {
    return v >= 1 && v <= SCHEMA_VERSION;
}

inline bool inRange(long v, long lo, long hi) { return v >= lo && v <= hi; }

// --- Field ranges (single source of truth, shared by route + tests) ---
struct Range { long lo, hi; };

// Zone bounds/thresholds.
static constexpr Range R_ZONE_X        = {-8000, 8000};
static constexpr Range R_ZONE_Y        = {-8000, 8000};
static constexpr Range R_MIN_RES       = {0, 2000};
static constexpr Range R_GHOST_TIMEOUT = {0, 600000};
static constexpr Range R_MOVE_THRESH   = {0, 2000};
static constexpr Range R_POS_THRESH    = {0, 2000};
static constexpr Range R_STATIC_RES    = {0, 2000};
static constexpr Range R_PERSIST_MS    = {0, 30000};
// Security timings.
static constexpr Range R_ANTIMASK_TIME = {0, 86400};
static constexpr Range R_LOITER_MS     = {0, 3600000};
static constexpr Range R_HEARTBEAT_MS  = {0, 86400000};
static constexpr Range R_ENTRY_DELAY   = {0, 600000};
static constexpr Range R_EXIT_DELAY    = {0, 600000};

// map_rotation is discrete.
inline bool rotationValid(long v) {
    return v == 0 || v == 90 || v == 180 || v == 270;
}

} // namespace ld2450_cfg
