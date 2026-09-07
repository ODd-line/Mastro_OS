# Waveshare V2 Hardware Build

This firmware targets the **Waveshare ESP32-S3-Touch-AMOLED-1.8 V2** with
16 MB flash, 8 MB octal PSRAM, a 368 x 448 CO5300 QSPI AMOLED, and CST820
touch. The managed Waveshare BSP performs panel initialization, board-revision
detection, touch setup, and command-based brightness control.

## Toolchain

The guarded scripts automatically activate the local ESP-IDF 5.5.5 install at
`~/esp/esp-idf-v5.5.5`. To build manually:

```sh
idf.py --version
idf.py build
```

The first build downloads the pinned managed components declared in
`main/idf_component.yml`.

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

## Restore

```sh
./scripts/restore_factory.sh backups/factory-TIMESTAMP.bin /dev/cu.usbmodemXXXX --yes
```

Restore accepts only an exact 16 MB image and also verifies the connected chip.

## Expected Boot Log

The firmware prints detected flash and PSRAM sizes before touching NVS or
starting board peripherals. It stops startup if flash is smaller than 16 MB or
PSRAM is missing. Touch starts before the display because the BSP uses touch
controller detection to apply the V2 panel's 16-pixel X offset.