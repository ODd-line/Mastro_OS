#!/bin/sh
set -eu

FLASH_SIZE_BYTES=16777216
BAUD_RATE=921600
BACKUP=${1:-}
PORT=${2:-}
CONFIRM=${3:-}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

if ! command -v esptool.py >/dev/null 2>&1 && [ -f "$HOME/esp/esp-idf-v5.5.5/export.sh" ]; then
    . "$HOME/esp/esp-idf-v5.5.5/export.sh" >/dev/null
fi

[ -n "$BACKUP" ] || fail "usage: $0 BACKUP.bin PORT --yes"
[ -f "$BACKUP" ] || fail "backup does not exist: $BACKUP"
[ -c "$PORT" ] || fail "serial port is not a character device: $PORT"
[ "$CONFIRM" = "--yes" ] || fail "restore requires the final --yes argument"
command -v esptool.py >/dev/null 2>&1 || fail "esptool.py is unavailable; source the ESP-IDF export script first"

actual_size=$(wc -c < "$BACKUP" | tr -d ' ')
[ "$actual_size" -eq "$FLASH_SIZE_BYTES" ] || fail "backup is $actual_size bytes; expected $FLASH_SIZE_BYTES"

chip_output=$(esptool.py --port "$PORT" chip_id 2>&1) || fail "could not communicate with $PORT"
printf '%s\n' "$chip_output" | grep -q 'ESP32-S3' || fail "connected target is not an ESP32-S3"

printf 'Restoring complete flash image to %s...\n' "$PORT"
esptool.py --chip esp32s3 --port "$PORT" --baud "$BAUD_RATE" write_flash 0 "$BACKUP"
printf 'Factory image restored.\n'