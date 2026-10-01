#pragma once
#include <stdint.h>

// Decide what to do when LittleFS fails to mount at boot.
// A blind format erases logs, noise map and offline MQTT data, so it is only done
// when the filesystem was never mounted successfully on this device (fresh flash),
// or after several consecutive failed boots (unattended last-resort recovery).
namespace ld2450_fs {

enum Action { Skip = 0, Format = 1 };

// everMounted: NVS flag set after any successful mount.
// failedBoots: consecutive failed boots BEFORE this one.
inline Action mountFailAction(bool everMounted, uint8_t failedBoots, uint8_t maxFailedBoots = 3) {
    if (!everMounted) return Format;
    return (unsigned)failedBoots + 1 >= maxFailedBoots ? Format : Skip;
}

}  // namespace ld2450_fs
