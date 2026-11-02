#!/bin/bash
# Throwaway: scans the small worlds with each build.
C=/tmp/claude-0/-home-user-ScraperX/5aee6a5c-426f-51cc-9ebb-389fd4033a74/scratchpad/cmp
cd "$C"
for v in trunk j3 final; do
  rm -rf "out_worlds_$v"; mkdir -p "out_worlds_$v"
  "$C/cmp_$v" worlds "out_worlds_$v" > "worlds_$v.log" 2>&1
  echo "$v exit $?"
done
