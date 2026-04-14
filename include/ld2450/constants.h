#ifndef CONSTANTS_H
#define CONSTANTS_H

// =============================================================================
// Time Constants (milliseconds)
// =============================================================================

// Intervals
constexpr unsigned long INTERVAL_SSE_UPDATE_MS       = 250;       // SSE telemetry update
constexpr unsigned long INTERVAL_TELEMETRY_ACTIVE_MS = 2000;      // Telemetry when active
constexpr unsigned long INTERVAL_TELEMETRY_IDLE_MS   = 3600000;   // Telemetry when idle (1h)
constexpr unsigned long INTERVAL_CONNECTIVITY_MS     = 60000;     // Connectivity check (1 min)
constexpr unsigned long INTERVAL_UPTIME_SAVE_MS      = 3600000;   // Uptime save to NVS (1h)
constexpr unsigned long INTERVAL_CERT_CHECK_MS       = 86400000;  // Certificate check (24h)

// Timeouts
constexpr unsigned long TIMEOUT_WIFI_FAILOVER_MS     = 45000;     // WiFi failover trigger
constexpr unsigned long TIMEOUT_OTA_VALIDATION_MS    = 60000;     // OTA boot validation
constexpr unsigned long TIMEOUT_DMS_NO_PUBLISH_MS    = 600000;    // Dead Man's Switch (10 min)
constexpr unsigned long TIMEOUT_DMS_STARTUP_MS       = 300000;    // DMS startup grace (5 min)

// Security Defaults
constexpr unsigned long DEFAULT_ENTRY_DELAY_MS       = 30000;     // Entry delay (30 sec)
constexpr unsigned long DEFAULT_EXIT_DELAY_MS        = 30000;     // Exit delay (30 sec)
constexpr unsigned long DEFAULT_ANTI_MASK_MS         = 300000;    // Anti-masking threshold (5 min)
constexpr unsigned long DEFAULT_LOITER_MS            = 15000;     // Loitering threshold (15 sec)
constexpr unsigned long DEFAULT_HEARTBEAT_MS         = 14400000;  // Heartbeat interval (4h)
constexpr unsigned long DEFAULT_TRIGGER_TIMEOUT_MS   = 900000;    // Alarm auto-silence (15 min)

// =============================================================================
// Memory Thresholds
// =============================================================================
constexpr uint32_t HEAP_MIN_FOR_PUBLISH              = 20000;     // Min heap for MQTT publish
constexpr uint32_t HEAP_LOW_WARNING                  = 30000;     // Low memory warning

// =============================================================================
// Hardware Configuration
// =============================================================================
constexpr uint8_t LED_PIN_DEFAULT                    = 2;         // Built-in LED
constexpr int8_t SIREN_PIN_DEFAULT                   = -1;        // Siren/strobe GPIO (-1 = disabled)
constexpr uint16_t DEFAULT_STARTUP_LED_SEC           = 120;       // LED blink after boot

// =============================================================================
// LD2450 Radar Configuration
// =============================================================================
constexpr uint8_t RADAR_MAX_TARGETS                  = 3;         // Max tracked targets
constexpr uint32_t RADAR_BAUD_RATE                   = 256000;    // LD2450 UART baud
constexpr uint8_t RADAR_RX_PIN_DEFAULT               = 18;
constexpr uint8_t RADAR_TX_PIN_DEFAULT               = 19;
constexpr int16_t RADAR_X_MIN                        = -4000;     // mm
constexpr int16_t RADAR_X_MAX                        = 4000;      // mm
constexpr int16_t RADAR_Y_MAX                        = 8000;      // mm (distance)
constexpr uint16_t RADAR_TIMEOUT_MS                  = 2000;      // No-data timeout

// Noise & Ghost Detection
constexpr uint8_t GRID_SIZE_CONST                    = 80;        // 80x80 noise grid
constexpr int16_t GRID_CELL_SIZE_CONST               = 100;       // mm per cell
constexpr int16_t GRID_OFFSET_CONST                  = 4000;      // offset for negative coords
constexpr uint16_t NOISE_MARGIN_MM_CONST             = 50;
constexpr uint32_t GHOST_TIMEOUT_MS                  = 120000;    // 2 min static = ghost
constexpr uint16_t HOLD_TIMEOUT_MS                   = 3000;      // Presence hold after lost

// =============================================================================
// Network Configuration
// =============================================================================
constexpr uint8_t DMS_MAX_RESTARTS                   = 3;
constexpr uint8_t WIFI_FAILOVER_MAX_ATTEMPTS         = 3;

// =============================================================================
// Security Monitor Intervals
// =============================================================================
constexpr unsigned long INTERVAL_HEALTH_CHECK_MS     = 60000;
constexpr unsigned long INTERVAL_RSSI_BASELINE_MS    = 30000;
constexpr unsigned long COOLDOWN_WIFI_ANOMALY_MS     = 300000;
constexpr unsigned long COOLDOWN_TAMPER_ALERT_MS     = 60000;
constexpr unsigned long COOLDOWN_RADAR_ALERT_MS      = 300000;
constexpr unsigned long TIMEOUT_LOW_RSSI_SUSTAINED_MS = 120000;
constexpr unsigned long TIMEOUT_RADAR_DISCONNECT_MS  = 30000;

// =============================================================================
// Tamper Detection (LD2450-specific)
// =============================================================================
constexpr uint16_t TAMPER_DISTANCE_THRESHOLD_MM      = 500;       // Close-range tamper
constexpr uint16_t MERGED_RESOLUTION_THRESHOLD       = 350;       // Merged target detection
constexpr float SMOOTHING_ALPHA                      = 0.4f;      // EMA smoothing factor
constexpr int16_t MQTT_MOVE_THRESHOLD                = 50;        // Min move to publish (mm)

// =============================================================================
// Time Conversion
// =============================================================================
constexpr unsigned long MS_PER_MINUTE                = 60000;
constexpr unsigned long MS_PER_HOUR                  = 3600000;

#endif // CONSTANTS_H
