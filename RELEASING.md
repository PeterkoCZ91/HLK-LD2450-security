# Releasing

1. Bump `FW_VERSION` in `platformio.ini` and commit.
2. Configure commit/tag signing once (SSH example):
   ```
   git config gpg.format ssh
   git config user.signingkey ~/.ssh/<signing-key>.pub
   ```
3. Run `tools/release.sh vX.Y.Z`. It checks a clean tree and a matching
   `FW_VERSION`, runs the native tests and a compile check, creates a signed tag
   and writes a signed `SHA256SUMS` of the source archive to `dist/`.
4. Push the tag: `git push origin vX.Y.Z`.

## Protecting tags (repository settings)

On GitHub: Settings → Rules → Rulesets → New tag ruleset, target pattern `v*`,
enable "Restrict deletions", "Restrict updates" and "Require signed commits".
Release tags then cannot be moved or deleted.

## Notes

- Release binaries are built from the `ld2450_release` environment (no ArduinoOTA,
  no embedded password). The OTA-enabled `ld2450_ota` build embeds a build-time
  `ARDUINO_OTA_PASSWORD` and must never be published.
- Verify a release: `git tag -v vX.Y.Z` and `sha256sum -c SHA256SUMS`.
