#ifndef SECRETS_H
#define SECRETS_H

// =============================================================================
// COPY THIS FILE as secrets.h and fill in your values:
//   cp include/secrets_example.h include/secrets.h
// =============================================================================

// WiFi Configuration (fallback — primary WiFi is set via captive portal)
#define WIFI_SSID_DEFAULT "YourSSID"
#define WIFI_PASS_DEFAULT "YourPassword"

// MQTT Configuration
#define MQTT_SERVER_DEFAULT "192.168.1.100"
#define MQTT_PORT_DEFAULT 1883
#define MQTT_USER_DEFAULT "mqtt_user"
#define MQTT_PASS_DEFAULT "mqtt_pass"

// MQTTS (TLS) Configuration
// Enabled at build time via -D MQTTS_ENABLED in platformio.ini (prod builds only)
#define MQTTS_PORT 8883

// MQTT Server CA Certificate (only needed if MQTTS_ENABLED)
static const char* mqtt_server_ca = R"EOF(
-----BEGIN CERTIFICATE-----
...INSERT YOUR CA CERT HERE...
-----END CERTIFICATE-----
)EOF";

// Web Admin (change these! Default triggers a warning banner in the UI)
#define WEB_ADMIN_USER_DEFAULT "admin"
#define WEB_ADMIN_PASS_DEFAULT "admin"

// Telegram Bot (optional — get token from @BotFather, chat ID from @userinfobot)
#define TELEGRAM_TOKEN_DEFAULT ""
#define TELEGRAM_CHAT_ID_DEFAULT ""

// BLE Security (6-digit passkey for NimBLE pairing)
#define BLE_PASSKEY_DEFAULT 123456

#endif
