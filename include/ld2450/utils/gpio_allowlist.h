#pragma once
#include <cstdint>

/**
 * gpio_allowlist - safe GPIO validation for the user-configurable siren pin
 * (DOC-03). Previously the siren was documented as "any GPIO, web configurable"
 * while the release shipped pin -1 with no UI and no validation. Wiring an
 * arbitrary pin is unsafe (flash pins, input-only pins, UART0, strapping pins).
 *
 * This is the single source of truth for which ESP32-WROOM GPIOs may drive a
 * siren/strobe: output-capable, not used by flash, UART0, the radar UART, or the
 * on-board LED, and avoiding boot-strapping pins. Pure and host-testable.
 */

namespace ld2450_gpio {

// Pins reserved by this firmware / hardware and never selectable:
//   0        strapping/boot
//   1, 3     UART0 serial console
//   2        on-board LED (LED_PIN_DEFAULT)
//   6..11    SPI flash
//   12,15    strapping pins (boot voltage / silence)
//   18,19    radar UART (RADAR_RX/TX)
//   34..39   input-only (cannot drive an output)
inline bool sirenPinAllowed(int pin) {
    if (pin == -1) return true;              // -1 = disabled (valid)
    switch (pin) {
        case 4:  case 5:
        case 13: case 14: case 16: case 17:
        case 21: case 22: case 23:
        case 25: case 26: case 27:
        case 32: case 33:
            return true;
        default:
            return false;
    }
}

} // namespace ld2450_gpio
