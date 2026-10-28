#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Native unit tests for the song model and player (app/song), built with the
# host cc (Apple clang on macOS). Usage: tests/run_host.sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/build/host-tests"
CC="${CC:-cc}"
mkdir -p "$OUT"
# Everything in app/song except song_port.c, which needs the instrument.
SOURCES=$(ls "$ROOT"/app/song/*.c | grep -v /song_port.c)
FLAGS="-std=c11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -fwrapv -fsanitize=address,undefined -fno-sanitize-recover=all"
status=0
for test in "$ROOT"/tests/test_*.c; do
    name=$(basename "$test" .c)
    # shellcheck disable=SC2086
    $CC $FLAGS -I"$ROOT/app/song" "$test" $SOURCES -o "$OUT/$name"
    "$OUT/$name" || status=1
done
[ "$status" -eq 0 ] && echo "run_host: all tests passed" || echo "run_host: FAILED"
exit "$status"
