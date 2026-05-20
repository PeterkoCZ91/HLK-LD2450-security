# Flashing Guide

Flash pre-built firmware onto your ESP32 without installing PlatformIO.  
Total time: ~5 minutes.

---

## What You Need

- ESP32 DevKit (ESP32-WROOM-32 recommended)
- HLK-LD2450 radar module
- USB-A to Micro-USB cable (or USB-C, depending on your board)
- Python 3.8+ installed on your PC

---

## 1. Download Firmware

Go to the [**Releases page**](https://github.com/PeterkoCZ91/HLK-LD2450-security/releases/latest) and download:

```
firmware-ld2450-vX.Y.Z.bin
```

---

## 2. Install esptool

```bash
pip install esptool
```

Verify:

```bash
esptool.py version
```

---

## 3. Wire the Sensor

Connect the LD2450 to the ESP32 **before** powering on.

```
ESP32 DevKit                HLK-LD2450
─────────────────────       ─────────────
5V  ──────────────────────► VCC
GND ──────────────────────► GND
GPIO 18 (RX2) ───────────► TX
GPIO 19 (TX2) ◄─────────── RX
```

> **Important — TX/RX cross-over:** GPIO 18 is UART2 RX on the ESP32, so it connects to the TX pin of the radar (radar transmits → ESP32 receives). GPIO 19 is UART2 TX → radar RX.

### Photo reference

| Signal | ESP32 pin | LD2450 pin |
|--------|-----------|------------|
| Power | 5V | VCC |
| Ground | GND | GND |
| Data in (radar → ESP32) | GPIO 18 | TX |
| Data out (ESP32 → radar) | GPIO 19 | RX |

The radar requires a stable 5 V supply. Power it from the ESP32 DevKit's 5V pin (fed from USB) or an external 5 V adapter — not 3.3 V.

Optional: connect a passive piezo buzzer or relay between any free GPIO and GND for siren/strobe output (configure the pin in the web UI).

---

## 4. Flash

Connect the ESP32 to your PC via USB, then run:

```bash
esptool.py --chip esp32 --port PORT --baud 460800 write_flash 0x0 firmware-ld2450-vX.Y.Z.bin
```

Replace `PORT` with your serial port:

| OS | Example port |
|----|-------------|
| Linux | `/dev/ttyUSB0` or `/dev/ttyACM0` |
| macOS | `/dev/cu.usbserial-0001` |
| Windows | `COM3` (check Device Manager → Ports) |

**If flashing fails:** hold the **BOOT** button on the ESP32 while running the command, release after "Connecting…" appears.

---

## 5. First Boot

1. The ESP32 starts a WiFi access point: **`esp32-ld2450-XXXX`**  
   Default AP password: `changeme`

2. Connect to it from your phone or PC and open **`http://192.168.4.1`**  
   (a captive portal opens automatically on most devices)

3. Enter your home WiFi SSID and password → Save → ESP32 reboots and joins your network

4. Find the device IP in your router's DHCP table, or use mDNS:  
   `http://ld2450.local/`

5. Open the web dashboard. Default credentials: **admin / admin**  
   > Change them immediately in **Network → Web credentials**

---

## 6. Basic Setup

After first boot:

1. **Alarm tab** — set entry/exit delays, configure scheduled arm/disarm
2. **Zones tab** — draw detection polygon zones on the live radar map; add blackout zones for furniture/HVAC
3. **System tab** — run background calibration (~1 h in empty room) to build the noise map
4. **Network tab** — configure MQTT broker and optional Telegram bot

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| `No serial data received` | Wrong port, or cable is charge-only (no data lines) |
| `Failed to connect to ESP32` | Hold BOOT button while flash starts |
| `A fatal error occurred: MD5` | Binary mismatch — re-download the .bin file |
| Radar not detected at boot | Check TX/RX cross-over wiring; verify 5 V on VCC |
| AP not appearing | Power-cycle the ESP32 after flashing |
| Web UI unreachable | Use the IP from router DHCP, mDNS may take 30 s |

---

## Building from Source

If you want to customise the firmware, see [CONTRIBUTING.md](../CONTRIBUTING.md) for the PlatformIO build setup.
