#!/bin/sh
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR="$PROJECT_DIR/build"
RELEASE_DIR="$PROJECT_DIR/release"
FLASH_ARGS="$BUILD_DIR/flash_args"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

if ! command -v idf.py >/dev/null 2>&1 && [ -f "$HOME/esp/esp-idf-v5.5.5/export.sh" ]; then
    . "$HOME/esp/esp-idf-v5.5.5/export.sh" >/dev/null
fi

command -v idf.py >/dev/null 2>&1 || fail "idf.py is unavailable; source the ESP-IDF export script first"
command -v esptool.py >/dev/null 2>&1 || fail "esptool.py is unavailable; source the ESP-IDF export script first"

cd "$PROJECT_DIR"
idf.py build
[ -f "$FLASH_ARGS" ] || fail "ESP-IDF did not generate build/flash_args"

set -- $(sed -n '1p' "$FLASH_ARGS")
[ "$#" -eq 6 ] || fail "unexpected flash configuration in build/flash_args"

mkdir -p "$RELEASE_DIR"
cp "$BUILD_DIR/bootloader/bootloader.bin" "$RELEASE_DIR/bootloader.bin"
cp "$BUILD_DIR/partition_table/partition-table.bin" "$RELEASE_DIR/partition-table.bin"
cp "$BUILD_DIR/watch_os.bin" "$RELEASE_DIR/watch-os-app.bin"

esptool.py --chip esp32s3 merge_bin "$@" \
    -o "$RELEASE_DIR/watch-os-waveshare-v2.bin" \
    0x0 "$RELEASE_DIR/bootloader.bin" \
    0x8000 "$RELEASE_DIR/partition-table.bin" \
    0x10000 "$RELEASE_DIR/watch-os-app.bin"

(
    cd "$RELEASE_DIR"
    shasum -a 256 bootloader.bin partition-table.bin watch-os-app.bin watch-os-waveshare-v2.bin > SHA256SUMS
    shasum -a 256 -c SHA256SUMS
)
