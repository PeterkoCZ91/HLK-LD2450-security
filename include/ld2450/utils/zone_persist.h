#pragma once
#include <Preferences.h>
#include "ld2450/types.h"

/**
 * zone_persist - single owner of the runtime ZoneConfig <-> NVS mapping (CFG-02).
 *
 * Historically two incompatible schemas coexisted: ConfigManager wrote
 * zone_xmin/zone_xmax/zone_ymax as int16 (putShort) while the web /api/config
 * route wrote z_x_min/z_x_max/z_y_max/z_y_min as int32 (putInt), and boot never
 * copied any of it back into the runtime ZoneConfig — so every reboot reset the
 * zone bounds and thresholds to compile-time defaults. static_res_threshold and
 * persistenceMs were not persisted at all.
 *
 * This defines one versioned schema (the z_* int32 keys), loads the complete
 * model into runtime at boot, and performs a one-time migration from the legacy
 * zone_* short keys.
 */

namespace ld2450_zones {

static constexpr uint8_t ZONE_SCHEMA_VERSION = 1;

// Canonical NVS keys (int32 unless noted).
// z_x_min z_x_max z_y_min z_y_max min_res static_res ghost_timeout
// move_threshold pos_threshold persist_ms map_rotation

// One-time migration of the legacy int16 zone_* keys into the z_* schema.
inline void migrateLegacy(Preferences& p) {
    if (p.isKey("zone_xmin") && !p.isKey("z_x_min"))
        p.putInt("z_x_min", p.getShort("zone_xmin", -4000));
    if (p.isKey("zone_xmax") && !p.isKey("z_x_max"))
        p.putInt("z_x_max", p.getShort("zone_xmax", 4000));
    if (p.isKey("zone_ymax") && !p.isKey("z_y_max"))
        p.putInt("z_y_max", p.getShort("zone_ymax", 8000));
    // Drop the obsolete duplicates so they cannot diverge again.
    p.remove("zone_xmin");
    p.remove("zone_xmax");
    p.remove("zone_ymax");
}

// Load the complete persisted zone model into `zc`, using the struct defaults
// already in `zc` as fallbacks. Runs migration first. Safe to call once at boot.
inline void load(Preferences& p, ZoneConfig& zc) {
    uint8_t ver = p.getUChar("zver", 0);
    if (ver < ZONE_SCHEMA_VERSION) {
        migrateLegacy(p);
        p.putUChar("zver", ZONE_SCHEMA_VERSION);
    }

    zc.xMin              = (int16_t)p.getInt("z_x_min",        zc.xMin);
    zc.xMax              = (int16_t)p.getInt("z_x_max",        zc.xMax);
    zc.yMin              = (int16_t)p.getInt("z_y_min",        zc.yMin);
    zc.yMax              = (int16_t)p.getInt("z_y_max",        zc.yMax);
    zc.minRes            = (uint16_t)p.getInt("min_res",       zc.minRes);
    zc.staticResThreshold= (uint16_t)p.getInt("static_res",   zc.staticResThreshold);
    zc.ghostTimeout      = (uint32_t)p.getInt("ghost_timeout", zc.ghostTimeout);
    zc.moveThreshold     = (uint16_t)p.getInt("move_threshold",zc.moveThreshold);
    zc.posThreshold      = (uint16_t)p.getInt("pos_threshold", zc.posThreshold);
    zc.persistenceMs     = (uint16_t)p.getInt("persist_ms",    zc.persistenceMs);
    zc.mapRotation       = (int16_t)p.getInt("map_rotation",   zc.mapRotation);
}

} // namespace ld2450_zones
