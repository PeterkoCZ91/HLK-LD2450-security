# Changelog

All notable changes to the LD2450 Security project.

## [5.5.2] - 2026-03-30

### Added
- ESP32-C6 support with dedicated build environments (`ld2450_c6_lab`, `ld2450_c6_prod`)
- Native USB CDC configuration for C6 (`ARDUINO_USB_CDC_ON_BOOT`)

## [5.5.1] - 2026-03-23

### Fixed
- Security audit fixes ported from LD2412 v3.9.3

## [5.5.0] - 2026-03-22

### Added
- Reliability and security improvements ported from LD2412 security audit
- Certificate expiry monitoring for MQTTS connections
- Dead Man's Switch with configurable timeout

### Fixed
- WiFi failover timing and reconnection logic

## [5.4.1] - 2026-02-09

### Added
- Security config REST API with getter methods
- Distance-scaled target weighting

## [5.4.0] - 2026-02-06

### Added
- Auto-identify ESP by MAC address via `known_devices.h`
- Extended Kalman Filter (EKF2D) per-target tracking
- Target association algorithm for cross-frame matching

## [5.3.0] - 2026-01

### Added
- Blackout zone drawing on radar map (click and drag)
- Manual blackout zone entry dialog
- Blackout zone enable/disable toggle

## [5.2.0] - 2026-01

### Added
- AI noise learning mode (80x80 grid, 1h calibration)
- Noise filter toggle in web UI
- Ghost detection and debug overlay on radar map

## [5.1.x] - 2025-12

### Added
- Telegram bot integration (arm, disarm, status, mute)
- BLE configuration via NimBLE

### Changed
- Telegram commands moved to external fusion service
- Loitering alert disabled on ESP (handled by fusion)

## [5.0.0] - 2025-11

### Added
- Full alarm state machine (5 states) ported from LD2412
- Polygon detection zones (2 zones, 8 vertices each)
- Live radar map with target trails
- Dark/light theme toggle
- Config export/import (JSON)
- Web OTA firmware upload

## [4.x] - 2025-10

### Added
- Multi-target tracking with Kalman filtering
- Real-time 2D radar visualization
- MQTT Home Assistant auto-discovery
- SSE real-time telemetry stream

## [3.x] - 2025-09

### Added
- Basic presence detection from LD2450 UART frames
- MQTT publishing
- Web dashboard with radar data display

## [1.x--2.x] - 2025

### Added
- Initial LD2450 UART parser
- WiFi + MQTT connectivity
- Basic web interface
