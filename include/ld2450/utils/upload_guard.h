#pragma once
// SEC-07 - host-testable guards for body/upload callbacks (config import,
// region-filter JSON body).
//
// Rules enforced here (pure C++, no Arduino/AsyncWebServer dependency):
//   1. authenticate BEFORE any allocation (uploadAdmit),
//   2. declared Content-Length must be present and within a hard cap,
//   3. only one concurrent upload session (UploadSlot, with stale reclaim),
//   4. per-request buffer is a single malloc block (UploadBuf) so that
//      AsyncWebServer's free(_tempObject) on disconnect/abort releases it
//      completely - no leaked String internals,
//   5. overflow / truncation is detected and rejected, never silently clipped.
//
// Plus typed "temp models" (ConfigImportModel, RegionFilterModel) that are filled
// from parsed JSON, validated as a whole, and only then applied by the route.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "ld2450/utils/config_schema.h"

namespace ld2450 {

// ---------------------------------------------------------------- admission

enum class UploadVerdict : uint8_t {
    Ok = 0,
    Unauthorized,   // -> 401, nothing allocated
    Busy,           // -> 503, another upload is in progress
    NoLength,       // -> 411, Content-Length missing/zero (chunked not accepted)
    TooLarge,       // -> 413
};

// Decision order matters: authentication first so unauthenticated clients learn
// nothing about busy state or limits and cause no allocation.
inline UploadVerdict uploadAdmit(bool authenticated, bool busy,
                                 size_t contentLength, size_t maxContentLength) {
    if (!authenticated) return UploadVerdict::Unauthorized;
    if (busy) return UploadVerdict::Busy;
    if (contentLength == 0) return UploadVerdict::NoLength;
    if (contentLength > maxContentLength) return UploadVerdict::TooLarge;
    return UploadVerdict::Ok;
}

inline int uploadHttpStatus(UploadVerdict v) {
    switch (v) {
        case UploadVerdict::Ok:           return 200;
        case UploadVerdict::Unauthorized: return 401;
        case UploadVerdict::Busy:         return 503;
        case UploadVerdict::NoLength:     return 411;
        case UploadVerdict::TooLarge:     return 413;
    }
    return 400;
}

// ------------------------------------------------------- single-session slot

// One concurrent upload across all upload routes. A dropped connection that never
// released the slot becomes reclaimable after staleMs (wrap-safe on millis()).
struct UploadSlot {
    bool active = false;
    uint32_t lastMs = 0;
    const void* owner = nullptr;   // identity of the request holding the slot

    bool busy(uint32_t nowMs, uint32_t staleMs) const {
        return active && (uint32_t)(nowMs - lastMs) < staleMs;
    }
    // True when the caller now owns the slot.
    bool tryAcquire(uint32_t nowMs, uint32_t staleMs, const void* who) {
        if (busy(nowMs, staleMs)) return false;
        active = true;
        lastMs = nowMs;
        owner = who;
        return true;
    }
    void touch(uint32_t nowMs, const void* who) { if (active && owner == who) lastMs = nowMs; }
    // Only the current owner may release (a reclaimed stale session must not free
    // the slot of the request that took it over).
    void release(const void* who) { if (active && owner == who) { active = false; owner = nullptr; } }
};

// ------------------------------------------------------------- body buffer

// Fixed-capacity buffer in ONE malloc block (header + data + NUL). Safe to store
// in AsyncWebServerRequest::_tempObject, which the library free()s on destruction.
struct UploadBuf {
    size_t cap;        // max payload bytes
    size_t len;        // bytes stored
    size_t expected;   // declared total (0 = unknown)
    bool   overflow;   // an append exceeded cap -> whole upload is invalid

    static UploadBuf* create(size_t cap, size_t expected = 0) {
        void* p = std::malloc(sizeof(UploadBuf) + cap + 1);
        if (!p) return nullptr;
        UploadBuf* b = static_cast<UploadBuf*>(p);
        b->cap = cap; b->len = 0; b->expected = expected; b->overflow = false;
        b->data()[0] = '\0';
        return b;
    }
    char* data() { return reinterpret_cast<char*>(this + 1); }
    const char* c_str() const { return reinterpret_cast<const char*>(this + 1); }

    bool append(const uint8_t* src, size_t n) {
        if (overflow) return false;
        if (n > cap || len > cap - n) { overflow = true; return false; }
        if (n) std::memcpy(data() + len, src, n);
        len += n;
        data()[len] = '\0';
        return true;
    }
    // Complete = no overflow, non-empty and (when a total was declared) all bytes arrived.
    bool complete() const {
        return !overflow && len > 0 && (expected == 0 || len == expected);
    }
};

// ----------------------------------------------------- string field limits

static constexpr size_t MQTT_SERVER_MAX = 64;
static constexpr size_t MQTT_USER_MAX   = 64;
static constexpr size_t MQTT_PASS_MAX   = 128;
static constexpr size_t MQTT_PORT_MAX   = 5;
static constexpr size_t TG_CHAT_MAX     = 32;

// Port as stored (string): 1..5 digits, value 1..65535.
inline bool portStringValid(const char* s) {
    if (!s) return false;
    size_t n = std::strlen(s);
    if (n == 0 || n > MQTT_PORT_MAX) return false;
    long v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    return v >= 1 && v <= 65535;
}

// Reject control characters (incl. NUL-adjacent junk) in imported strings.
inline bool printableNoCtl(const char* s) {
    for (; *s; ++s) if ((unsigned char)*s < 0x20 || (unsigned char)*s == 0x7F) return false;
    return true;
}

// ------------------------------------------------------------- temp models

template <typename T> struct Opt { bool has = false; T v{}; void set(T x) { has = true; v = x; } };

template <size_t N> struct StrOpt {
    bool has = false;
    bool tooLong = false;
    char v[N] = {0};
    void set(const char* s) {
        has = true;
        size_t n = std::strlen(s);
        if (n >= N) { tooLong = true; n = N - 1; }
        std::memcpy(v, s, n);
        v[n] = '\0';
    }
};

// Typed staging model for /api/config/import. The route fills it (a wrongly typed
// known field sets `typeError`), validate() checks the WHOLE model, and only a
// fully valid model is written to NVS.
struct ConfigImportModel {
    const char* typeError = nullptr;   // first key with a wrong JSON type

    Opt<int> xmin, xmax, ymin, ymax, minRes, staticRes, ghostTimeout,
             moveThreshold, posThreshold, persistenceMs, mapRotation;
    Opt<int>  antimaskTime;   Opt<bool> antimaskEnabled;
    Opt<long> loiterMs;       Opt<bool> loiterAlert;
    Opt<long> heartbeatMs, entryDelayMs, exitDelayMs;
    StrOpt<MQTT_SERVER_MAX + 1> mqttServer;
    StrOpt<MQTT_PORT_MAX + 1>   mqttPort;
    StrOpt<MQTT_USER_MAX + 1>   mqttUser;
    StrOpt<MQTT_PASS_MAX + 1>   mqttPass;
    Opt<bool> mqttEnabled, mqttTls;
    StrOpt<TG_CHAT_MAX + 1>     tgChat;

    // Returns nullptr when valid, otherwise the offending key.
    const char* validate() const {
        using namespace ld2450_cfg;
        if (typeError) return typeError;
        auto bad = [](const Opt<int>& o, Range r) { return o.has && !inRange(o.v, r.lo, r.hi); };
        auto badL = [](const Opt<long>& o, Range r) { return o.has && !inRange(o.v, r.lo, r.hi); };
        if (bad(xmin, R_ZONE_X)) return "xmin";
        if (bad(xmax, R_ZONE_X)) return "xmax";
        if (bad(ymin, R_ZONE_Y)) return "ymin";
        if (bad(ymax, R_ZONE_Y)) return "ymax";
        if (bad(minRes, R_MIN_RES)) return "min_res";
        if (bad(staticRes, R_STATIC_RES)) return "static_res";
        if (bad(ghostTimeout, R_GHOST_TIMEOUT)) return "ghost_timeout";
        if (bad(moveThreshold, R_MOVE_THRESH)) return "move_threshold";
        if (bad(posThreshold, R_POS_THRESH)) return "pos_threshold";
        if (bad(persistenceMs, R_PERSIST_MS)) return "persistence_ms";
        if (mapRotation.has && !rotationValid(mapRotation.v)) return "map_rotation";
        if (bad(antimaskTime, R_ANTIMASK_TIME)) return "antimask_time";
        if (badL(loiterMs, R_LOITER_MS)) return "loiter_ms";
        if (badL(heartbeatMs, R_HEARTBEAT_MS)) return "heartbeat_ms";
        if (badL(entryDelayMs, R_ENTRY_DELAY)) return "entry_delay_ms";
        if (badL(exitDelayMs, R_EXIT_DELAY)) return "exit_delay_ms";
        if (mqttServer.tooLong || (mqttServer.has && !printableNoCtl(mqttServer.v))) return "mqtt.server";
        if (mqttPort.has && !portStringValid(mqttPort.v)) return "mqtt.port";
        if (mqttUser.tooLong || (mqttUser.has && !printableNoCtl(mqttUser.v))) return "mqtt.user";
        if (mqttPass.tooLong || (mqttPass.has && !printableNoCtl(mqttPass.v))) return "mqtt.pass";
        if (tgChat.tooLong || (tgChat.has && !printableNoCtl(tgChat.v))) return "telegram.chat_id";
        return nullptr;
    }
};

// Region filter: raw (wide) values so an out-of-range number is rejected instead
// of silently wrapping when narrowed to int16.
struct RegionFilterModel {
    static constexpr long COORD_LIM = 10000;
    const char* typeError = nullptr;
    long mode = -1;
    long coords[12] = {0};
    int  zoneCount = 0;
    bool tooManyZones = false;

    const char* validate() const {
        if (typeError) return typeError;
        if (tooManyZones) return "zones";
        if (mode < 0 || mode > 2) return "mode";
        for (int i = 0; i < 12; i++)
            if (coords[i] < -COORD_LIM || coords[i] > COORD_LIM) return "coord";
        return nullptr;
    }
};

}  // namespace ld2450
