#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Native unit tests for the song model and player (app/song), built with the
# host cc (Apple clang on macOS), then the workbench's Node tests.
# Usage: tests/run_host.sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/build/host-tests"
CC="${CC:-cc}"
mkdir -p "$OUT"
# The model and player. The glue, delivery and diagnostics need the instrument.
SOURCES=$(ls "$ROOT"/app/song/*.c | grep -vE '/(song_port|song_deliver|song_knobs|song_store|song_transfer|song_perf|song_chord|diag)\.c$')
FLAGS="-DSONG_UI=1 -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -fwrapv -fsanitize=address,undefined -fno-sanitize-recover=all"
status=0
for test in "$ROOT"/tests/test_*.c; do
    name=$(basename "$test" .c)
    [ "$name" = test_flash_map ] && continue # full layout only, below
    # shellcheck disable=SC2086
    $CC $FLAGS -I"$ROOT/app/song" "$test" $SOURCES -o "$OUT/$name"
    "$OUT/$name" || status=1
done
# The full device build's layout: its flash map (song_flash.h), and the
# storage tests again with the pool on logical sectors 25 to 125 through it
# (tests/ram_flash.h), every file built with the device's configuration.
FULL="-DSONG_DEVICE_FULL=1 -DTEST_FULL_LAYOUT -I$ROOT/sdk/include"
for name in test_flash_map test_store test_corrupt test_corrupt_kits test_kit_copies; do
    # shellcheck disable=SC2086
    $CC $FLAGS $FULL -I"$ROOT/app/song" "$ROOT/tests/$name.c" $SOURCES -o "$OUT/${name}_full"
    "$OUT/${name}_full" || status=1
done
# The workbench's file codecs and MIDI (web/workbench), in Node.
if command -v node >/dev/null 2>&1; then node "$ROOT/tests/workbench_test.mjs" || status=1
else echo "run_host: node not found, workbench tests skipped"; fi
[ "$status" -eq 0 ] && echo "run_host: all tests passed" || echo "run_host: FAILED"
exit "$status"
