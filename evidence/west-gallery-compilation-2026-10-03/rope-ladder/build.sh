#!/bin/sh
# Private probe only. Reads pinned existing Jolt; never writes the shared build.
set -eu
probe_root=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/design/rope-ladder
proot-distro login ubuntu --shared-tmp -- /usr/bin/c++ \
  -O2 -std=c++17 -pthread -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM \
  -DJPH_PROFILE_ENABLED -DNDEBUG \
  -I/data/data/com.termux/files/usr/tmp/scraperx-restore-build/_deps/jolt-src \
  "$probe_root/rope_ladder_probe.cpp" \
  /data/data/com.termux/files/usr/tmp/scraperx-restore-linux-build/_deps/jolt-build/libJolt.a \
  -o "$probe_root/rope-ladder-probe"
