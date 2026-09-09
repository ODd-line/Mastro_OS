#!/bin/sh
set -eu
umask 077

FLASH_SIZE_BYTES=16777216
BAUD_RATE=${BAUD_RATE:-921600}
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BACKUP_DIR="$PROJECT_DIR/backups"
PORT=${1:-}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

if ! command -v idf.py >/dev/null 2>&1 && [ -f "$HOME/esp/esp-idf-v5.5.5/export.sh" ]; then
    . "$HOME/esp/esp-idf-v5.5.5/export.sh" >/dev/null
fi

command -v idf.py >/dev/null 2>&1 || fail "idf.py is unavailable; source the ESP-IDF export script first"
command -v esptool.py >/dev/null 2>&1 || fail "esptool.py is unavailable; source the ESP-IDF export script first"

if [ -z "$PORT" ]; then
    set -- /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial*
    candidates=""
    for candidate in "$@"; do
        [ -e "$candidate" ] && candidates="${candidates}${candidate}\n"
    done
    count=$(printf '%b' "$candidates" | sed '/^$/d' | wc -l | tr -d ' ')
    [ "$count" -eq 1 ] || fail "expected exactly one serial device; pass its path explicitly"
    PORT=$(printf '%b' "$candidates" | sed '/^$/d')
fi

[ -c "$PORT" ] || fail "serial port is not a character device: $PORT"
chip_output=$(esptool.py --port "$PORT" chip_id 2>&1) || fail "could not communicate with $PORT"
printf '%s\n' "$chip_output" | grep -q 'ESP32-S3' || fail "connected target is not an ESP32-S3"

cd "$PROJECT_DIR"
idf.py set-target esp32s3
idf.py build

mkdir -p "$BACKUP_DIR"
timestamp=$(date -u '+%Y%m%dT%H%M%SZ')
backup="$BACKUP_DIR/factory-${timestamp}.bin"
metadata="$backup.sha256"

printf 'Reading complete 16 MB factory flash from %s...\n' "$PORT"
printf 'This private backup may contain Wi-Fi credentials; do not upload or share it.\n'
esptool.py --chip esp32s3 --port "$PORT" --baud "$BAUD_RATE" read_flash 0 "$FLASH_SIZE_BYTES" "$backup"

actual_size=$(wc -c < "$backup" | tr -d ' ')
[ "$actual_size" -eq "$FLASH_SIZE_BYTES" ] || fail "backup is $actual_size bytes; expected $FLASH_SIZE_BYTES"
shasum -a 256 "$backup" > "$metadata"
shasum -a 256 -c "$metadata" >/dev/null || fail "backup SHA-256 verification failed"

printf 'Verified backup: %s\n' "$backup"
printf 'Flashing watch OS...\n'
esptool.py --chip esp32s3 --port "$PORT" --baud 115200 \
    --before default_reset --after no_reset chip_id >/dev/null
(
    cd "$PROJECT_DIR/build"
    esptool.py --chip esp32s3 --port "$PORT" --baud "$BAUD_RATE" \
        --before no_reset --after watchdog_reset write_flash @flash_args
)
printf 'Flash complete. Factory restore image: %s\n' "$backup"