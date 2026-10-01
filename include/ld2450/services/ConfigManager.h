#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "secrets.h"

struct SystemConfig {
    char mqtt_server[60] = MQTT_SERVER_DEFAULT;
    char mqtt_port[6] = "1883";
    char mqtt_user[40] = MQTT_USER_DEFAULT;
    char mqtt_pass[40] = MQTT_PASS_DEFAULT;
    char mqtt_id[40] = "ld2450_device";
    char hostname[33] = "ld2450-node";
    char auth_user[20] = "admin";
    char auth_pass[20] = "admin";
    char backup_ssid[33] = "";
    char backup_pass[65] = "";
    bool mqtt_enabled = true;
    bool mqtt_tls = false;
    bool led_enabled = true;
    uint16_t startup_led_sec = 120;
    // Zone bounds/thresholds live in the unified z_* NVS schema owned by
    // ld2450_zones (CFG-02), not here; the old zone_x* duplicates were removed.
    // Schedule
    char sched_arm_time[6] = "";
    char sched_disarm_time[6] = "";
    uint16_t auto_arm_minutes = 0;
    // Day/Night zone profile schedule (HH:MM). Empty = always day profile.
    char night_start_time[6] = "";
    char night_end_time[6] = "";
    // POSIX TZ string for local time / DST (TIME-01). Default = Central European
    // Time with EU DST rules. configTzTime() applies this to NTP-synced time.
    char timezone[40] = "CET-1CEST,M3.5.0,M10.5.0/3";

    // Native LD2450 region filter (cmd 0xC2). Mode 0=disabled, 1=detect-only, 2=exclude.
    // Array holds 3 zones x {x1,y1,x2,y2} = 12 int16. Stored in NVS as blob "rf_zones".
    uint8_t region_filter_mode = 0;
    int16_t region_filter_zones[12] = {0};
};

class ConfigManager {
public:
    ConfigManager();
    void begin();
    void load();
    void save();

    SystemConfig& getConfig() { return _config; }

    bool isDefaultAuth() const {
        return (strcmp(_config.auth_user, "admin") == 0 && strcmp(_config.auth_pass, "admin") == 0);
    }

private:
    Preferences _prefs;
    SystemConfig _config;

    void loadPref(const char* key, char* target, size_t maxLen);
};

#endif
