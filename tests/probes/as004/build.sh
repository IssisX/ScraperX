#!/bin/bash
# White-box reach probes behind AS-004's Revalidation section. Each probe
# compiles src/sim/simulation.cpp into itself with `private` opened, so it
# can read the Jolt world and move bodies (the cut audit moves the tower's
# 33->44 m flight out of the world). Probes, not falsifiers: nothing here
# is built by CMake or run by CI.
#
# Needs the host build configured first (build/host, as CI does), then:
#   tests/probes/as004/build.sh tests/probes/as004/audit44.cpp /tmp/audit44
#   /tmp/audit44
set -euo pipefail
if [ "$#" -ne 2 ]; then
    echo "usage: $0 <probe.cpp> <output>" >&2
    exit 2
fi
root="$(cd "$(dirname "$0")/../../.." && pwd)"
host="${root}/build/host"
c++ -w -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -DJPH_USE_AVX -DJPH_USE_AVX2 \
    -DJPH_USE_F16C -DJPH_USE_FMADD -DJPH_USE_LZCNT -DJPH_USE_SSE4_1 -DJPH_USE_SSE4_2 -DJPH_USE_TZCNT \
    -DNDEBUG -DSCRAPERX_HAS_JOLT=1 -I"${root}/src" -I"${host}/_deps/jolt-src/Build/.." -O2 -std=c++17 \
    -mavx2 -mbmi -mpopcnt -mlzcnt -mf16c -mfma -mfpmath=sse -pthread \
    "$1" "${root}/src/sim/steam_plant.cpp" "${host}/_deps/jolt-build/libJolt.a" -o "$2"
