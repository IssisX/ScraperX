# J-008 work in progress (stopped 11-15 for quota)

Base: trunk `ScraperX-Claude` @ ecee5a35. No code changed yet; this file is the whole commit.

## What is already on trunk
- Ladder-bar sort is a strict total order (`bar_before`, #314 merged 11-08).
- The 258-down traversal fix (a walk to a move's start may cross regions unless it is a patch walk, plus the
  landing replan) is in #377, merged 11-11.
- The determinism flags (`-ffp-contract=off`, Jolt `USE_FMADD OFF`, `CROSS_PLATFORM_DETERMINISTIC ON`) are NOT on
  trunk. They belong to J-009 (Muse, #116) and cause drift reds across the whole suite. J-008 does not land them.

## What is still missing for J-008
- The edge tie order: `AffordanceMap::build_edges` ends with `std::sort` on (from, to, kind) at
  `src/sim/matron_traversal.cpp:3638`. Edges with the same key but different contents keep a stdlib-specific order
  (libstdc++ on host vs libc++ on the NDK). This is #97's measured host/arm64 swap of edges 564/565 (340->341, kind 2).
- Builder's `builder/integrate-j008` @ cb8bd5cd has `stable_sort` there, and its device-determinism job (per-edge
  print, host vs arm64 under qemu) passed on proof 43027088683. But do not build on cb8bd5cd: its diff against
  trunk reverses later trunk work (the `floors` scan parameter, stock_damage/slab_cell/objectives/flex_plank).

## Measured
- Nothing run yet. Two local build dirs were configured in the session scratchpad (lost after reset):
  base = CI flags; det = CI flags + `-DCMAKE_CXX_FLAGS=-ffp-contract=off -DCMAKE_C_FLAGS=-ffp-contract=off
  -DUSE_FMADD=OFF -DCROSS_PLATFORM_DETERMINISTIC=ON`. Checked in det/build.ninja: no `-mfma`,
  `-ffp-contract=off` and `-DJPH_CROSS_PLATFORM_DETERMINISTIC` on scraperx_sim objects. A full build of
  `scraperx_sim_tests` takes over an hour on 4 cores with no ccache.

## Exact next step
1. Build trunk twice (base and det, as above; reuse `_deps` sources via `FETCHCONTENT_SOURCE_DIR_GODOT_CPP` /
   `_MONOCYPHER` from the main checkout's build/host). Run `SCRAPERX_ONLY=matron-tower` and `ladder-bar-sort` on
   both; keep the leg tables. Other reds under det are J-009's drift: do not chase.
2. If 258 down arrives under det: add `traversal::sort_edges(std::vector<Edge>&)` (declared after `struct Edge` in
   matron_traversal.hpp), called by `build_edges`, using `std::stable_sort` on (from, to, kind). New slice
   `edge-tie-order` in tests/sim_tests_matron_climb.cpp: more than 16 edges, tie groups interleaved with other keys,
   tagged by `cost` = insertion index; require sorted by key and ties in insertion order. Fail-first: run it with
   `std::sort` inside `sort_edges`, see FAIL, then switch to `stable_sort`, see PASS.
3. Diff the base tower leg table before/after; if host order moved, run matron-tower, matron-ladders and both
   athletic-traversal shards once. Edge indices are not saved (`travel_avoid_` is runtime only), so not protected.
4. If 258 down fails under det: the fix must be general traversal code with a CI test that shows it without the
   flags; if it can only be shown with flags on, stop and report.
5. rule_check, push `claude/integrate-j008`, PR via REST (`POST repos/IssisX/ScraperX/pulls`, draft; GraphQL is
   403 here), body starts `Job: J-008`. Open after landing: CI cannot prove 258 down under the flags until J-009
   lands them.
