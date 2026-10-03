#!/bin/sh
set -eu
BINDING_DIR=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/design/rope-production-binding
for phase in quiet rider; do
 proot-distro login ubuntu --shared-tmp -- /usr/bin/c++ -O2 -std=c++17 -pthread -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -DNDEBUG -I/data/data/com.termux/files/usr/tmp/scraperx-restore-build/_deps/jolt-src "$BINDING_DIR/rope_binding_$phase.cpp" /data/data/com.termux/files/usr/tmp/scraperx-restore-linux-build/_deps/jolt-build/libJolt.a -o "$BINDING_DIR/rope-binding-$phase"
done
