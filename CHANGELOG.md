# Changelog

All notable changes to LD2450 Security are documented here.

---

## [v5.7.5] — 2026-05

### Added
- CI workflow (build firmware + run parser tests on every push/PR)
- CONTRIBUTING.md, issue templates, PR template, SECURITY.md, `.editorconfig`

## [v5.7] — 2025-05

### Added
- **Hardware region filter** — native LD2450 cmd 0xC2: 3 rectangular zones in include/exclude mode, filtering done radar-side before UART
- **Day / night zone profiles** — per-zone HH:MM switch times; each polygon and blackout zone carries a day-only / night-only / both mask
- **Bilingual web UI** — built-in Czech / English toggle, persisted in `localStorage`
- **Parser regression tests** — 16 host-side Unity tests covering valid frames, multi-target, origin-target with non-zero resolution (HLK firmware v2.14), trailing garbage, and back-to-back parsing (`pio test -e native`)

### Changed
- Route layer refactored: REST endpoints split into `network_routes`, `security_routes`, `schedule_routes`, `system_routes`, `telemetry_routes`, `zone_routes`

## [v5.5] — 2025-03

### Added
- MQTT offline buffer — LittleFS-backed ring buffer (30 messages), survives reboot, auto-replay on reconnect
- Entry / exit counter — virtual tripwire line with directional in/out counting
- Movement classification — standing / walking / running per target
- Zone dwell time — per-target time spent inside polygon zones
- Scheduled arm / disarm — daily HH:MM auto-arm/disarm + auto-arm after inactivity timeout
- BLE configuration — NimBLE peripheral for mobile setup (passkey-protected)
- 13 security fixes ported from LD2412 security audit

## [v5.4] — 2025-02

### Added
- Extended Kalman Filter (EKF2D) per target — smooth trajectory estimation for [x, y, vx, vy]
- Hungarian-algorithm-inspired target association across frames

## [v5.3] — 2025-01

### Added
- Blackout zone drawing — draw rectangular exclusion areas directly on the radar map
- Anti-masking detection — alert when sensor is obstructed while armed

## [v5.2] — 2024-12

### Added
- Background calibration — 80×80 grid noise map, learns static reflectors over ~1 h in an empty room
- Noise map overlay on live radar display

## [v5.0–v5.1] — 2024-11

### Added
- Alarm state machine — 5 states (DISARMED / ARMING / ARMED / PENDING / TRIGGERED) with configurable entry/exit delays
- Auto-rearm after trigger timeout
- Siren / strobe GPIO output
- Disarm reminder notification

## [v4.x] — 2024-09 to 2024-10

### Added
- Multi-target tracking (up to 3 simultaneous targets with X/Y/speed)
- Polygon detection zones — up to 5 user-defined polygon zones with up to 8 vertices
- Ghost detection — static targets exceeding timeout classified as ghosts
- Live 2D radar map with trails and zone overlay
- Target association across frames

## [v3.x] — 2024-07 to 2024-08

### Added
- MQTTS (TLS) support with CA certificate validation and expiry monitoring
- Telegram bot — arm, disarm, status, mute, restart commands
- OTA firmware update (web-based + ArduinoOTA)
- WiFi backup SSID with automatic failover

## [v2.x] — 2024-05 to 2024-06

### Added
- Web dashboard — responsive dark-mode UI with live target display
- REST API — config, telemetry, alarm, OTA endpoints
- MQTT + Home Assistant auto-discovery
- Config backup / restore (JSON export/import)
- mDNS (`http://hostname.local/`)

## [v1.x] — 2024-03 to 2024-04

### Added
- Initial LD2450 UART frame parser (256000 baud, 8N1, Serial2)
- Basic presence detection via MQTT
- WiFi provisioning captive portal (ESPAsyncWiFiManager)
- NVS persistence layer (ConfigManager)
