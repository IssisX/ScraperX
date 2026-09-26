#!/bin/sh
# Invoke inside the host/toolchain environment that built the Jolt archive.
set -eu
if [ "$#" -lt 2 ]; then
    echo 'usage: build_probe.sh JOLT_SOURCE_DIR JOLT_ARCHIVE [OUTPUT_BINARY]' >&2
    exit 2
fi
PROBE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
JOLT_SOURCE_DIR=$1
JOLT_ARCHIVE_FILE=$2
PROBE_BINARY=${3:-"$PROBE_DIR/native_probe"}
"${CXX:-c++}" -std=c++17 -O2 -fno-rtti -fno-exceptions -ffp-contract=on -pthread \
    -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -DNDEBUG \
    -I"$JOLT_SOURCE_DIR" "$PROBE_DIR/native_probe.cpp" "$JOLT_ARCHIVE_FILE" \
    -o "$PROBE_BINARY"
