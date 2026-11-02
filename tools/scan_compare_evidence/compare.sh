#!/bin/bash
# Throwaway: compares a run's logs (timing stripped) and dumps across builds.
# usage: compare.sh worlds
C=/tmp/claude-0/-home-user-ScraperX/5aee6a5c-426f-51cc-9ebb-389fd4033a74/scratchpad/cmp
cd "$C"
p="$1"
for v in trunk j3 final; do
  [ -f "${p}_$v.log" ] && sed -E 's/wall_s=[0-9.]+ stats_s=[0-9.]+ //' -- "${p}_$v.log" > "${p}_$v.norm"
done
echo "== trunk vs final (summary lines)"
if diff -q -- "${p}_trunk.norm" "${p}_final.norm" > /dev/null; then echo IDENTICAL; else diff -- "${p}_trunk.norm" "${p}_final.norm" | cut -c1-250; fi
echo "== trunk vs final (full dumps)"
if diff -rq -- "out_${p}_trunk" "out_${p}_final" > /dev/null; then echo IDENTICAL; else diff -r -- "out_${p}_trunk" "out_${p}_final" | head -20 | cut -c1-250; fi
echo "== trunk vs J3 before these fixes (summary lines)"
if diff -q -- "${p}_trunk.norm" "${p}_j3.norm" > /dev/null; then echo IDENTICAL; else diff -- "${p}_trunk.norm" "${p}_j3.norm" | cut -c1-250; fi
