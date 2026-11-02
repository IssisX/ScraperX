#!/bin/bash
# Throwaway: builds the scan comparison three ways (trunk 864a804b, J3 merge
# d90cb815 before the key fix, and the final tree's own library object).
set -euo pipefail
W=/home/user/ScraperX/.claude/worktrees/agent-aaa32d00ab8c71cad
B=$W/build/host
C=/tmp/claude-0/-home-user-ScraperX/5aee6a5c-426f-51cc-9ebb-389fd4033a74/scratchpad/cmp
J=$W/build/jolt-src/Build/..
DEFS="-DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -DJPH_USE_AVX -DJPH_USE_AVX2 -DJPH_USE_F16C -DJPH_USE_FMADD -DJPH_USE_LZCNT -DJPH_USE_SSE4_1 -DJPH_USE_SSE4_2 -DJPH_USE_TZCNT -DNDEBUG -DSCRAPERX_HAS_JOLT=1"
OPT="-O3 -DNDEBUG -std=c++17 -fPIC -mavx2 -mbmi -mpopcnt -mlzcnt -mf16c -mfma -mfpmath=sse -pthread"
SYM=_ZN8scraperx3sim9traversal13AffordanceMap4scanERKN3JPH13PhysicsSystemERKNS3_5AABoxERKNS1_6IgnoreERKNS1_6LimitsE
c++ $DEFS -I$C/trunk_src -I$W/src -I$J $OPT -Wall -Wextra -Wpedantic -Werror -c $C/trunk_src/sim/matron_traversal.cpp -o $C/trunk_mt.o &
c++ $DEFS -I$C/j3_src -I$W/src -I$J $OPT -Wall -Wextra -Wpedantic -Werror -c $C/j3_src/sim/matron_traversal.cpp -o $C/j3_mt.o &
c++ $DEFS -I$W/src -I$J $OPT -c $C/scan_compare.cpp -o $C/scan_compare.o &
wait
LIBS="$B/libscraperx_sim.a $B/libscraperx_delivery.a $B/_deps/jolt-build/libJolt.a"
c++ -O3 -pthread -Wl,--wrap=$SYM $C/scan_compare.o $C/trunk_mt.o $LIBS -o $C/cmp_trunk
c++ -O3 -pthread -Wl,--wrap=$SYM $C/scan_compare.o $C/j3_mt.o $LIBS -o $C/cmp_j3
c++ -O3 -pthread -Wl,--wrap=$SYM $C/scan_compare.o $LIBS -o $C/cmp_final
for v in trunk j3 final; do
  printf '%s scan_steel symbols: %s\n' "$v" "$(nm -C $C/cmp_$v | grep -c 'AffordanceMap::scan_steel' || true)"
done
