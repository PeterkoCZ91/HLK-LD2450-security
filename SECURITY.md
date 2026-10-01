# Security Policy

## Supported Versions

Only the latest release on `main` receives security fixes.

| Version | Supported |
|---------|-----------|
| v5.8.x (latest) | Yes |
| < v5.8 | No |

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Report security issues by email to the maintainer. Include:

- A description of the vulnerability and its potential impact
- Steps to reproduce (proof of concept if available)
- Affected version(s)
- Any suggested fix or mitigation

You will receive a response within 7 days. If the issue is confirmed, a fix will be released and you will be credited in the changelog unless you prefer to remain anonymous.

## Scope

This project runs on a local ESP32 and requires no cloud service for its core operation. The optional Telegram bot is an outbound integration with Telegram's cloud (`api.telegram.org`); enable it only if you want that. Relevant attack surfaces:

- **Web UI (port 80)** — HTTP Basic Auth over **unencrypted HTTP** (credentials are only base64-encoded on the wire); default credentials (`admin/admin`) trigger a warning banner
- **MQTT** — optional MQTTS (TLS on port 8883) with CA cert validation
- **BLE pairing** — NimBLE peripheral, 6-digit passkey required
- **OTA updates** — password-protected; optional MD5 hash verification
- **Telegram bot** — command whitelist, chat ID restriction

Out of scope: physical access to the ESP32, LD2450 radar module firmware (maintained by Hi-Link), issues in third-party Arduino libraries.

## Security Hardening Notes

- Change default web credentials immediately after first boot
- **Keep the device on a trusted/isolated LAN.** The web UI is plain HTTP (port 80), so Basic Auth credentials and session traffic are not encrypted — do not expose it to the internet directly; put it behind a reverse proxy with TLS, a VPN, or a dedicated VLAN
- Use MQTTS (port 8883) in production deployments
- Keep `secrets.h` and `known_devices.h` out of version control (both are in `.gitignore`)
- Consider disabling the LD2450 BLE radio via the System tab after initial setup
