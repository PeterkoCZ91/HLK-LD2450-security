# Changelog

All notable changes to LD2450 Security are documented here.

---

## [v5.7.6] — 2026-07-15

### Fixed
- GitHub Release binary creation with the esptool 5.x command-line interface
- Pre-built firmware now uses the captive-portal release environment instead of development mode

### Changed
- GitHub Actions upgraded to Node.js 24-based releases
- PlatformIO and esptool versions pinned in CI and release jobs for reproducible builds
- SHA-256 checksum published alongside each firmware binary
- Flashing documentation updated to the current esptool command syntax

### Documentation
- Corrected the MQTT topic reference table to match the firmware (`tracking/count`, `presence/notification`, `rssi`, `health`, `heap`, …) and added previously undocumented topics
- Fixed the Dead Man's Switch description (10-minute timeout, max 3 restarts, then degraded local-only mode — was incorrectly "60 minutes")
- Renamed the tracking filter from "Extended Kalman Filter (EKF2D)" to the accurate "constant-velocity Kalman filter"; described ghost suppression as adaptive background/noise-map filtering rather than "AI / noise learning"
- Corrected over-claims: config backup/restore covers core settings (not "all settings"); siren output is compile-time optional with no dashboard control
- SECURITY.md: clarified that core operation needs no cloud (optional Telegram uses Telegram's cloud); noted the web UI is plain HTTP and should stay on a trusted LAN
- CONTRIBUTING.md: reconciled the logging guideline with actual practice

> Firmware functionality is unchanged from v5.7.5.

## [v5.7.5] — 2026-05

### Added
- CI workflow (build firmware + run parser tests on every push/PR)
- CONTRIBUTING.md, issue templates, PR template, SECURITY.md, `.editorconfig`

## [v5.7] — 2026-05

### Added
- **Hardware region filter** — native LD2450 cmd 0xC2: 3 rectangular zones in include/exclude mode, filtering done radar-side before UART
- **Day / night zone profiles** — per-zone HH:MM switch times; each polygon and blackout zone carries a day-only / night-only / both mask
- **Bilingual web UI** — built-in Czech / English toggle, persisted in `localStorage`
- **Parser regression tests** — 16 host-side Unity tests covering valid frames, multi-target, origin-target with non-zero resolution (HLK firmware v2.14), trailing garbage, and back-to-back parsing (`pio test -e native`)

### Changed
- Route layer refactored: REST endpoints split into `network_routes`, `security_routes`, `schedule_routes`, `system_routes`, `telemetry_routes`, `zone_routes`

## [v5.5] — 2026-03

### Added
- MQTT offline buffer — LittleFS-backed ring buffer (30 messages), survives reboot, auto-replay on reconnect
- Entry / exit counter — virtual tripwire line with directional in/out counting
- Movement classification — standing / walking / running per target
- Zone dwell time — per-target time spent inside polygon zones
- Scheduled arm / disarm — daily HH:MM auto-arm/disarm + auto-arm after inactivity timeout
- BLE configuration — NimBLE peripheral for mobile setup (passkey-protected)
- 13 security fixes ported from LD2412 security audit

## [v5.4] — 2026-03

### Added
- Extended Kalman Filter (EKF2D) per target — smooth trajectory estimation for [x, y, vx, vy]
- Hungarian-algorithm-inspired target association across frames

## [v5.3] — 2026-02

### Added
- Blackout zone drawing — draw rectangular exclusion areas directly on the radar map
- Anti-masking detection — alert when sensor is obstructed while armed

## [v5.2] — 2026-02

### Added
- Background calibration — 80×80 grid noise map, learns static reflectors over ~1 h in an empty room
- Noise map overlay on live radar display

## [v5.0–v5.1] — 2026-02

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
