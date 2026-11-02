#!/bin/bash
# Throwaway: fused multiply-adds in split_patches, per object.
W=/home/user/ScraperX/.claude/worktrees/agent-aaa32d00ab8c71cad
C=/tmp/claude-0/-home-user-ScraperX/5aee6a5c-426f-51cc-9ebb-389fd4033a74/scratchpad/cmp
SYM=_ZN8scraperx3sim9traversal13AffordanceMap13split_patchesERKN3JPH13PhysicsSystemERKNS1_6IgnoreERKNS1_6LimitsE
for o in "$C/trunk_mt.o" "$C/j3_mt.o" "$W/build/host/CMakeFiles/scraperx_sim.dir/src/sim/matron_traversal.cpp.o"; do
  objdump -d --no-show-raw-insn --disassemble="$SYM" -- "$o" > "$C/split.asm"
  printf '%s: %s instructions, fma: %s (%s)\n' "$(basename "$(dirname "$o")")/$(basename "$o")" \
    "$(grep -c '^ ' "$C/split.asm")" \
    "$(grep -cE 'vf(n)?m(add|sub)' "$C/split.asm")" \
    "$(grep -oE 'vf(n)?m(add|sub)[0-9a-z]+' "$C/split.asm" | sort | uniq -c | tr '\n' ' ')"
done
