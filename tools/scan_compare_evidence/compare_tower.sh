#!/bin/bash
# Throwaway: compares tower run <r> of trunk and final (timing stripped).
C=/tmp/claude-0/-home-user-ScraperX/5aee6a5c-426f-51cc-9ebb-389fd4033a74/scratchpad/cmp
cd "$C"
r="$1"
for v in trunk final; do
  sed -E 's/wall_s=[0-9.]+ stats_s=[0-9.]+ //' -- "tower_${v}_r$r.log" > "tower_${v}_r$r.norm"
done
echo "== trunk vs final, run $r (summary lines)"
if diff -q -- "tower_trunk_r$r.norm" "tower_final_r$r.norm" > /dev/null; then echo IDENTICAL; else diff -- "tower_trunk_r$r.norm" "tower_final_r$r.norm" | cut -c1-200; fi
echo "== trunk vs final, run $r (full dumps)"
if diff -rq -- "out_tower_trunk_r$r" "out_tower_final_r$r" > /dev/null; then echo IDENTICAL; else diff -r -- "out_tower_trunk_r$r" "out_tower_final_r$r" | head -12 | cut -c1-200; fi
cut -c1-330 -- "tower_final_r$r.norm"
wc -l -- out_tower_final_r$r/*.txt | tail -1
