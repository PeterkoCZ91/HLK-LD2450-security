#include "services/ConfigManager.h"

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
    _prefs.begin("ld2450_config", false);
    load();
}

void ConfigManager::load() {
    loadPref("mqtt_server", _config.mqtt_server, sizeof(_config.mqtt_server));
    loadPref("mqtt_port", _config.mqtt_port, sizeof(_config.mqtt_port));
    loadPref("mqtt_user", _config.mqtt_user, sizeof(_config.mqtt_user));
    loadPref("mqtt_pass", _config.mqtt_pass, sizeof(_config.mqtt_pass));
    loadPref("mqtt_id", _config.mqtt_id, sizeof(_config.mqtt_id));
    loadPref("hostname", _config.hostname, sizeof(_config.hostname));
    loadPref("auth_user", _config.auth_user, sizeof(_config.auth_user));
    loadPref("auth_pass", _config.auth_pass, sizeof(_config.auth_pass));
    loadPref("bk_ssid", _config.backup_ssid, sizeof(_config.backup_ssid));
    loadPref("bk_pass", _config.backup_pass, sizeof(_config.backup_pass));

    _config.mqtt_enabled = _prefs.getBool("mqtt_en", true);
#ifdef MQTTS_ENABLED
    if (_prefs.isKey("mqtt_tls")) {
        _config.mqtt_tls = _prefs.getBool("mqtt_tls", true);
    } else {
        _config.mqtt_tls = String(_config.mqtt_port).toInt() == MQTTS_PORT;
    }
#else
    _config.mqtt_tls = false;
#endif
    _config.led_enabled = _prefs.getBool("led_en", true);
    _config.startup_led_sec = _prefs.getUInt("led_start", 120);

    // LD2450-specific zone config
    _config.zone_x_min = _prefs.getShort("zone_xmin", -4000);
    _config.zone_x_max = _prefs.getShort("zone_xmax", 4000);
    _config.zone_y_max = _prefs.getShort("zone_ymax", 8000);

    // Schedule
    loadPref("sched_arm", _config.sched_arm_time, sizeof(_config.sched_arm_time));
    loadPref("sched_disarm", _config.sched_disarm_time, sizeof(_config.sched_disarm_time));
    _config.auto_arm_minutes = _prefs.getUShort("auto_arm_min", 0);
    loadPref("night_start", _config.night_start_time, sizeof(_config.night_start_time));
    loadPref("night_end", _config.night_end_time, sizeof(_config.night_end_time));

    // Native LD2450 region filter (cmd 0xC2)
    _config.region_filter_mode = (uint8_t)_prefs.getUChar("rf_mode", 0);
    if (_prefs.isKey("rf_zones")) {
        size_t got = _prefs.getBytes("rf_zones",
                                     _config.region_filter_zones,
                                     sizeof(_config.region_filter_zones));
        if (got != sizeof(_config.region_filter_zones)) {
            // Korupce / neúplný blob — vynuluj
            memset(_config.region_filter_zones, 0, sizeof(_config.region_filter_zones));
        }
    }

    // Persist defaults to NVS on first boot (prevents NOT_FOUND errors)
    if (!_prefs.isKey("mqtt_server")) {
        save();
        Serial.println("[CONFIG] First boot - defaults written to NVS");
    }

    Serial.println("[CONFIG] Configuration loaded from NVS");
    if (isDefaultAuth()) {
        Serial.println("[SECURITY] WARNING: Default credentials (admin/admin) are in use!");
    }
}

void ConfigManager::save() {
    _prefs.putString("mqtt_server", _config.mqtt_server);
    _prefs.putString("mqtt_port", _config.mqtt_port);
    _prefs.putString("mqtt_user", _config.mqtt_user);
    _prefs.putString("mqtt_pass", _config.mqtt_pass);
    _prefs.putString("mqtt_id", _config.mqtt_id);
    _prefs.putString("hostname", _config.hostname);
    _prefs.putString("auth_user", _config.auth_user);
    _prefs.putString("auth_pass", _config.auth_pass);
    _prefs.putString("bk_ssid", _config.backup_ssid);
    _prefs.putString("bk_pass", _config.backup_pass);

    _prefs.putBool("mqtt_en", _config.mqtt_enabled);
    _prefs.putBool("mqtt_tls", _config.mqtt_tls);
    _prefs.putBool("led_en", _config.led_enabled);
    _prefs.putUInt("led_start", _config.startup_led_sec);

    _prefs.putShort("zone_xmin", _config.zone_x_min);
    _prefs.putShort("zone_xmax", _config.zone_x_max);
    _prefs.putShort("zone_ymax", _config.zone_y_max);

    _prefs.putString("sched_arm", _config.sched_arm_time);
    _prefs.putString("sched_disarm", _config.sched_disarm_time);
    _prefs.putUShort("auto_arm_min", _config.auto_arm_minutes);
    _prefs.putString("night_start", _config.night_start_time);
    _prefs.putString("night_end", _config.night_end_time);

    _prefs.putUChar("rf_mode", _config.region_filter_mode);
    _prefs.putBytes("rf_zones",
                    _config.region_filter_zones,
                    sizeof(_config.region_filter_zones));

    Serial.println("[CONFIG] Configuration saved to NVS");
}

void ConfigManager::loadPref(const char* key, char* target, size_t maxLen) {
    String val = _prefs.getString(key, "");
    if (val.length() > 0) {
        val.toCharArray(target, maxLen);
    }
}
