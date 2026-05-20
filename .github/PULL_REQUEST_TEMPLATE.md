## Description

<!-- What does this PR do and why? Include relevant context (hardware constraints, protocol quirks, issue link). -->

Fixes # <!-- issue number, if applicable -->

## Type of Change

- [ ] Bug fix
- [ ] New feature
- [ ] Refactor (no functional change)
- [ ] Documentation
- [ ] Build / CI

## Testing

- [ ] `pio test -e native` passes (16/16 parser tests)
- [ ] `pio run -e ld2450_lab` builds without errors
- [ ] Tested on hardware (if applicable — note board and firmware version)
- [ ] Web UI verified in browser (if UI changes — both CS and EN modes)
- [ ] MQTT / Home Assistant entities verified (if connectivity changes)
- [ ] Alarm state machine exercised manually (if alarm logic changes)

## Checklist

- [ ] `secrets.h` and `known_devices.h` are **not** included in this PR
- [ ] No hardcoded IPs, passwords, or credentials
- [ ] Backwards-compatible with existing NVS config (or migration documented)
- [ ] CHANGELOG.md updated (if user-visible change)
