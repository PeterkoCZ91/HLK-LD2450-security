# Contributing to LD2450 Security

Bug reports, feature requests, and pull requests are welcome.

---

## Development Setup

**Requirements:** [PlatformIO](https://platformio.org/) (VS Code extension or CLI), Python 3.8+.

```bash
git clone https://github.com/PeterkoCZ91/HLK-LD2450-security.git
cd HLK-LD2450-security

# Config files (never commit these)
cp include/secrets.h.example include/secrets.h
cp include/ld2450/known_devices.h.example include/ld2450/known_devices.h
# Edit secrets.h with your WiFi, MQTT, Telegram credentials
```

**Build environments:**

| Command | Description |
|---------|-------------|
| `pio run -e ld2450_release` | Build the firmware |
| `pio run -e ld2450_release --target upload` | Flash via USB |
| `pio run -e ld2450_ota --target upload` | Flash via OTA (set IP in platformio.ini and export `ARDUINO_OTA_PASSWORD` first) |
| `pio test -e native` | Run parser regression tests (no hardware needed) |

---

## Running Tests

```bash
pio test -e native
```

The native environment compiles the radar frame parser against Unity and runs 16 host-side regression tests. No ESP32 needed. All tests must pass before submitting a PR.

---

## Branch Naming

| Type | Pattern | Example |
|------|---------|---------|
| New feature | `feature/<short-description>` | `feature/tripwire-reset-api` |
| Bug fix | `fix/<short-description>` | `fix/parser-trailing-garbage` |
| Documentation | `docs/<short-description>` | `docs/api-reference-update` |
| Refactor | `refactor/<short-description>` | `refactor/zone-classifier` |

Work from `main`. PRs merge back to `main`.

---

## What to Test Before Opening a PR

- `pio test -e native` passes (16/16)
- `pio run -e ld2450_release` builds cleanly (zero errors, ideally zero warnings)
- If you touched the web UI (`web_interface.h`): verify in a browser, both CS and EN language modes
- If you touched MQTT topics or HA auto-discovery: verify entities appear correctly in Home Assistant
- If you touched the alarm state machine: manually exercise DISARMED → ARMING → ARMED → PENDING → TRIGGERED

---

## What We're Looking For

- Bug fixes with a reproducer or unit test
- New features that fit the embedded constraints (heap budget ~60 KB free, PROGMEM for web UI)
- Parser improvements backed by a new test case in `test/test_parser/`
- Documentation fixes and clearer error messages

## What We're Not Looking For

- Cloud integrations or third-party service dependencies
- Features that require SD card or external storage (project uses LittleFS on internal flash)
- Rewrites of core data structures without prior discussion in an issue
- Changes that break the `pio test -e native` suite

---

## Code Style

This is an embedded C++ project targeting ESP32 Arduino. Follow the conventions already in `src/` and `include/`:

- Indent with **4 spaces**, no tabs
- One class per `.cpp` / `.h` pair; keep headers lean (declarations only)
- `UPPER_SNAKE_CASE` for constants and `#define`s, `PascalCase` for classes, `camelCase` for functions and variables
- No dynamic allocation in hot paths (`new`/`delete` only at init time)
- Keep ISR and UART task callbacks short; defer work to the main loop via queues
- `Serial.print`/`printf` is used for boot and runtime **diagnostics**; persistent **security events** (alarms, arm/disarm, tamper) must go through the `EventLog` service so they survive reboots. Keep diagnostic logging out of hot paths (ISR/UART callbacks)
- `secrets.h` and `known_devices.h` must never appear in a commit (they are in `.gitignore`)

---

## Submitting a Pull Request

1. Fork and create a branch from `main`
2. Make your changes, run `pio test -e native` and `pio run -e ld2450_release`
3. Open a PR against `main` using the PR template
4. Describe **what** changed and **why** — hardware constraints or protocol quirks that aren't obvious from the code belong in the PR description, not inline comments

Questions or design discussions → open an Issue first.
