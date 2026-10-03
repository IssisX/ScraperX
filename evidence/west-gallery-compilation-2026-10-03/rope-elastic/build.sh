#!/bin/sh
set -eu
PROBE_DIR=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/design/rope-elastic
JOLT_HEADERS=/data/data/com.termux/files/usr/tmp/scraperx-restore-build/_deps/jolt-src
JOLT_LIB=/data/data/com.termux/files/usr/tmp/scraperx-restore-linux-build/_deps/jolt-build/libJolt.a
for candidate in quiet rider; do
  proot-distro login ubuntu --shared-tmp -- /usr/bin/c++ -O2 -std=c++17 -pthread -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -DNDEBUG -I"$JOLT_HEADERS" "$PROBE_DIR/elastic_ladder_$candidate.cpp" "$JOLT_LIB" -o "$PROBE_DIR/elastic-ladder-$candidate"
done
