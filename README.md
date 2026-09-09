# Mastro OS

Mastro OS is an Apple Watch-inspired smartwatch platform built with ESP-IDF and LVGL. Its shared UI and services are separated from board hardware through selectable adapters. The Waveshare ESP32-S3-Touch-AMOLED-1.8 V2 is currently the only verified profile.

## Hardware target

- Waveshare ESP32-S3-Touch-AMOLED-1.8 V2
- ESP32-S3 with 16 MB flash and 8 MB octal PSRAM
- 368 x 448 CO5300 QSPI AMOLED
- CST816S/CST820-compatible capacitive touch controller
- PCF85063 real-time clock
- TCA9554 peripheral reset and power control

The firmware performs profile-specific flash and PSRAM preflight checks before initializing board peripherals. See [BOARD_SUPPORT.md](BOARD_SUPPORT.md) for verified and planned targets. Do not flash another model under the Waveshare profile.

## Implemented features

- Live RTC-backed watch face with date, weekday, weather and Solar Dial data
- Touch gestures between the watch face, launcher and Control Center
- Registry-driven app grid with 19 app entries
- Hardware brightness control with idle dimming and display sleep
- Functional Wi-Fi, time-format, synchronization, torch and theater controls
- On-watch Wi-Fi credential entry and persistent settings
- SNTP synchronization with RTC persistence
- Hong Kong time and location defaults
- Network-backed weather refresh
- GUI task watchdog and hardware startup checks
- Factory flash backup and verified restore tooling
- Reproducible release packaging with SHA-256 checksums

Most launcher entries currently open reusable app-shell views rather than complete phone, health, navigation or messaging services. Settings, watch-face data, navigation and Control Center hardware actions are the primary implemented workflows.

## Project layout

- `main.c` - hardware preflight and firmware startup
- `hal/` - AMOLED, touch and RTC hardware abstraction
- `hal/watch_board.h` - board adapter contract for additional ESP32 watches
- `ui/` - LVGL screens, gestures and display lifecycle
- `apps/` - launcher application registry
- `utils/` - Wi-Fi, time, weather and settings services
- `astronomy/` - Solar Dial calculations and host tests
- `scripts/` - tests, safe flashing, restore and release packaging
- `HARDWARE.md` - board-specific build and flashing requirements
- `BOARD_SUPPORT.md` - verified targets and port acceptance requirements
- `SECURITY.md` - security posture, limitations and secret handling

## Build and test

The scripts expect ESP-IDF 5.5.5 at `~/esp/esp-idf-v5.5.5` by default.

```bash
./scripts/test.sh
```

This runs the host Solar Dial tests and a complete ESP-IDF firmware build. To build manually:

```bash
source "$HOME/esp/esp-idf-v5.5.5/export.sh"
idf.py -D WATCH_BOARD=waveshare_amoled_1_8_v2 reconfigure build
```

ESP-IDF compiles the C sources with warnings enabled and is the project's type
and link validation. This repository has no JavaScript or TypeScript sources,
`package.json`, ESLint configuration or `tsconfig.json`; ESLint and `tsc` are
therefore not project checks and should not be installed solely for this firmware.

## Safe flashing

Read [HARDWARE.md](HARDWARE.md) before connecting the board. The recommended first-flash command is:

```bash
./scripts/flash_safe.sh /dev/cu.usbmodemXXXX
```

The script verifies the ESP32-S3, backs up the complete 16 MB factory flash, verifies that backup and flashes using ESP-IDF's generated arguments. This board requires a DIO ROM image header and a watchdog reset after flashing; forcing QIO in a merged image can prevent boot.

To restore a verified factory image:

```bash
./scripts/restore_factory.sh backups/factory-TIMESTAMP.bin /dev/cu.usbmodemXXXX --yes
```

## Release package

```bash
./scripts/package_release.sh
```

Generated binaries and checksums are written to `release/`. Publish those files through GitHub Releases rather than committing them to the source tree.

## Privacy and security

Read [SECURITY.md](SECURITY.md) before entering real network credentials or
sharing a flash backup. The current development configuration stores the Wi-Fi
SSID and password in NVS without flash encryption. Anyone with physical access
to the watch or a complete flash backup may be able to recover them.

Wi-Fi driver persistence is RAM-only, HTTPS redirects are rejected, and Settings
provides a long-press **Hold Erase** action for decommissioning. The firmware does
not accept compile-time Wi-Fi credentials because those remain recoverable from
distributed binaries. Logical deletion cannot defeat forensic recovery from
unencrypted flash.

Weather requests use HTTPS with ESP-IDF's trusted certificate bundle. SNTP time
sync is not cryptographically authenticated and must not be treated as a trusted
time source for security decisions. Secure boot, flash encryption, signed OTA,
Bluetooth pairing, remote APIs and account authentication are not implemented.
Release SHA-256 files detect accidental corruption but do not prove who created
an image.

Do not commit credentials, distribute files from `backups/`, or present launcher
app shells as connected phone, health or emergency services.

## Project status

Mastro OS is an experimental hardware project, not a commercial wearable, medical device or safety-critical system. Display, touch, RTC, Wi-Fi, brightness, navigation and release workflows have been exercised on the target board, but physical hardware revisions and third-party services can change behavior.
