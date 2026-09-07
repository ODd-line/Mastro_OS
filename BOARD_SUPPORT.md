# Board Support

Mastro OS separates shared application code from hardware-specific drivers through
`hal/watch_board.h`. Every supported watch must have a board adapter and must pass
build and physical-device tests. The build rejects unknown board names instead of
guessing pin assignments.

## Support matrix

| Platform | Build profile | Status |
| --- | --- | --- |
| Waveshare ESP32-S3-Touch-AMOLED-1.8 V2 | `waveshare_amoled_1_8_v2` | Built and exercised on hardware |
| LilyGo T-Watch S3 | Not assigned | Planned; hardware and driver port required |
| LilyGo T-Watch 2020 | Not assigned | Planned; hardware and driver port required |
| Other documented ESP32 watches | Not assigned | Possible after board-specific development and testing |
| Wear OS / Android watches | Separate application | ESP32 firmware is not compatible |
| Apple Watch | None | Locked platform; unsupported |

## Selecting a board

The current verified profile is explicit even though it remains the default:

```sh
idf.py -D WATCH_BOARD=waveshare_amoled_1_8_v2 reconfigure build
```

An unknown profile fails during CMake configuration. Do not rename an existing
profile to flash another watch.

## Adding an ESP32 watch

A real port must provide an implementation of `hal/watch_board.h` for:

- peripheral power and reset sequencing
- display creation, brightness and bus cleanup
- touch-controller creation
- shared I2C access and clock rate
- minimum flash and PSRAM requirements

The port must also define its display geometry, ESP-IDF target, flash and PSRAM
configuration, partition table, dependencies, release artifact name, backup size,
and chip verification rules. Add it to the root `WATCH_BOARD` allowlist only after
the adapter compiles. Mark it verified only after display, touch, RTC, brightness,
sleep, reboot, backup, flash and restore are tested on the exact hardware revision.

Security validation for each profile must also document whether secure boot and
flash encryption are enabled, where credentials are stored, how the exact model is
identified before flashing, which debug interfaces remain accessible, and whether
factory backups contain unique device secrets. Flash and restore scripts must use
that profile's actual chip, flash size, reset behavior and partition layout.

## Android and Wear OS

Wear OS watches need a separate Android application written against Wear OS APIs.
They cannot execute this ESP32 image. Shared branding, service protocols and some
business logic may be reused, but the boot process, drivers, UI framework, package
format and installation workflow are different.

Unknown consumer watches should be treated as unsupported until their processor,
bootloader, display, touch, power controller and recovery method are documented.