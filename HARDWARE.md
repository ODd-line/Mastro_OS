# Waveshare V2 Hardware Build

This document applies only to the `waveshare_amoled_1_8_v2` profile. See
[BOARD_SUPPORT.md](BOARD_SUPPORT.md) before building for any other watch.

This firmware targets the **Waveshare ESP32-S3-Touch-AMOLED-1.8 V2** with
16 MB flash, 8 MB octal PSRAM, a 368 x 448 CO5300 QSPI AMOLED, and CST820
touch. The managed Waveshare BSP performs panel initialization, board-revision
detection, touch setup, and command-based brightness control.

## Verified Hardware Map

| Function | Device or connection |
| --- | --- |
| MCU | ESP32-S3, QFN56 revision v0.2 tested |
| Flash | 16 MB, DIO image header; second-stage bootloader enables QIO |
| PSRAM | 8 MB octal PSRAM at 80 MHz |
| Display | 368 x 448 CO5300 QSPI AMOLED |
| Display QSPI | CS GPIO 12, clock GPIO 11, data GPIOs 4, 5, 6 and 7 |
| Touch | CST816S/CST820-compatible controller at I2C address `0x15` |
| Touch interrupt | GPIO 21 |
| RTC | PCF85063 at I2C address `0x51` |
| Shared I2C | SDA GPIO 15, SCL GPIO 14 |
| Reset and power | TCA9554 outputs 0, 1 and 2 |

The startup sequence drives TCA9554 outputs 0-2 low for 20 ms, drives them high,
then waits about 200 ms before initializing touch and display. A board with the
same screen dimensions but different pins, power polarity, controller or reset
timing is not compatible.

Do not connect peripherals to these GPIOs without checking the board schematic.
Incorrect power-control or reset levels can electrically conflict with onboard
devices. The firmware has only been exercised on the V2 board revision.

## Toolchain

The guarded scripts automatically activate the local ESP-IDF 5.5.5 install at
`~/esp/esp-idf-v5.5.5`. To build manually:

```sh
idf.py --version
idf.py build
```

The first build downloads the pinned managed components declared in
`main/idf_component.yml`.

Select the hardware profile explicitly when configuring a fresh build:

```sh
idf.py -D WATCH_BOARD=waveshare_amoled_1_8_v2 reconfigure build
```

## Safe First Flash

Connect only one ESP board, then run:

```sh
./scripts/flash_safe.sh
```

If more than one serial device is present, pass the board explicitly:

```sh
./scripts/flash_safe.sh /dev/cu.usbmodemXXXX
```

The script refuses to flash unless it identifies an ESP32-S3 and successfully
reads and verifies a complete 16 MB factory backup. Backups are written under
`backups/` with a SHA-256 sidecar. It flashes from ESP-IDF's generated
`build/flash_args`; do not override the ROM boot header to QIO when creating a
merged image. This board boots with a DIO image header, then the second-stage
bootloader enables QIO safely. Leave the board with an esptool watchdog reset;
the native USB hard reset can leave the shared display/touch reset domain in an
unresponsive state.

Chip-family detection does not prove that the connected ESP32-S3 is this watch.
Confirm the exact product and hardware revision yourself before flashing. The
script is intentionally profile-specific and must not be reused for another
board by changing only its name.

The backup contains every readable flash partition, including NVS and therefore
stored Wi-Fi credentials. Keep both the `.bin` file and its checksum private and
delete them securely when no longer required. A SHA-256 sidecar verifies file
integrity; it is not a digital signature and provides no publisher identity.

## Restore

```sh
./scripts/restore_factory.sh backups/factory-TIMESTAMP.bin /dev/cu.usbmodemXXXX --yes
```

Restore accepts only an exact 16 MB image and also verifies the connected chip.
Restoring overwrites the complete flash, including application data, settings and
credentials. Never restore an image from another physical watch.

## Expected Boot Log

The firmware prints detected flash and PSRAM sizes before touching NVS or
starting board peripherals. It stops startup if flash is smaller than 16 MB or
PSRAM is missing. Touch starts before the display because the BSP uses touch
controller detection to apply the V2 panel's 16-pixel X offset.

## Production Security

The checked-in configuration is intended for development and recovery. ESP32-S3
secure boot and flash encryption are disabled. Enabling either feature changes
provisioning, flashing and recovery behavior and can permanently restrict a
device when eFuses are burned. Do not enable production security settings until
you have a separate signing-key process, encrypted key backups, recovery plan and
dedicated test hardware. See [SECURITY.md](SECURITY.md) for the current threat
model.