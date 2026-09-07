#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf-v5.5.5}"
TEST_BINARY="${TMPDIR:-/tmp}/watch_os_solar_test"

cd "$PROJECT_DIR"

cc -std=c11 -Wall -Wextra -Werror -I. \
    astronomy/solar_calc.c astronomy/solar_calc_test.c \
    -lm -o "$TEST_BINARY"
"$TEST_BINARY"

. "$IDF_PATH/export.sh"
idf.py build
