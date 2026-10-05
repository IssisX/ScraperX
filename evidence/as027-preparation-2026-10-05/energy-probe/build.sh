#!/bin/bash
set -euo pipefail
cd /workspace/module-import
c++ -std=c++17 -O2 -pthread -mavx2 -mbmi -mpopcnt -mlzcnt -mf16c -mfma -mfpmath=sse -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -DJPH_USE_AVX -DJPH_USE_AVX2 -DJPH_USE_F16C -DJPH_USE_FMADD -DJPH_USE_LZCNT -DJPH_USE_SSE4_1 -DJPH_USE_SSE4_2 -DJPH_USE_TZCNT -DNDEBUG -Iinclude -I/workspace/ScraperX/src -I/tmp/scraperx-build/_deps/jolt-src energy-probe/lift.cpp src/*.cpp /tmp/scraperx-build/libscraperx_sim.a /tmp/scraperx-build/_deps/jolt-build/libJolt.a -o energy-probe/lift
