#pragma once
// Which network to try on the next reconnect attempt (host-testable).
namespace ld2450_wifi {
// Alternates primary/backup when both exist; otherwise always the one that exists.
inline bool nextIsBackup(bool lastWasBackup, bool hasPrimary, bool hasBackup) {
    if (!hasBackup) return false;
    if (!hasPrimary) return true;
    return !lastWasBackup;
}
}  // namespace ld2450_wifi
