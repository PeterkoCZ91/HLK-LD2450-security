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
    bool led_enabled = true;
    uint16_t startup_led_sec = 120;
    // LD2450-specific (no gate config, but zone config)
    int16_t zone_x_min = -4000;
    int16_t zone_x_max = 4000;
    int16_t zone_y_max = 8000;
    // Schedule
    char sched_arm_time[6] = "";
    char sched_disarm_time[6] = "";
    uint16_t auto_arm_minutes = 0;
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
