# ScraperX

Godot 4.7 / C++17 / Jolt industrial ascent. Read [`00_START_HERE.md`](00_START_HERE.md) before changes. Writes go to `ChatGPT` only.

## Current game state — 2026-09-27

The owner-directed reset retired the old campaign machinery, including the water screw/lift, intake sequence, upper lifts and machine-linked stair assemblies. The normal game keeps the tower, alpine setting, full parkour/controller and optional ordinary stairs to +154 m. The ambient yard crane and gear motifs remain presentation; the crane sway is not simulated rigging and does not operate or lift anything.

Current gameplay source contains the native pipe-loaded bridge from grade through +8 m to the +11 m tower ring, followed by AS-017's static façade route to +33 m. See [`00_START_HERE.md`](00_START_HERE.md) for the single current record of source, CI, artifact and device evidence.

The +33→44 m swinging-stair adaptation is local staging, not active gameplay. Its current state is recorded in [`00_START_HERE.md`](00_START_HERE.md). The +66 m objective is paused. The [Atlas](01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md) owns space; the [ascent plan](03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md) and [development proposal](03_EXECUTION/PLANNING/NEXT_ASCENT_PROPOSAL.md) hold future targets, not proof of playable height.

Old machinery and traversal remain available only through explicitly selected regression fixtures. Fixture results are not campaign progress. AS-001–015 are retired; do not resume that queue.

## Build

Dependencies are fetched at exact commits by CMake:

- `godot-cpp` `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` (10.0.0 stable, API 4.7)
- Jolt Physics `e77f175595e64cb44218cc9d9d56fc365ad0e36a` (5.6.0)

Host build:

```bash
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```


Normal Godot launch loads the current game world, including the pipe bridge and the AS-017 façade route. The explicit `--uitest=ground_foundation` scenario exercises the cleared-foundation regression case; it is not the normal game. Legacy UI scenarios explicitly load regression fixtures. To regenerate collision tables, use normal `--export-solids=<path>` for `src/sim/world_solids.inc`, and add `--regression-fixtures` for `tests/fixtures/world_solids.inc`.

CI runs the default-world proof and rendered capture, retained physics/controller regressions, both collision drift checks, Android arm64 build and APK export. Its artifact contains exact-commit checksums and evidence boundaries.

## Claim boundary

An exported APK proves packaging. Installation, Android execution, device readability, touch ergonomics and sustained Fold 6 performance remain separate evidence states. See [`00_START_HERE.md`](00_START_HERE.md) for the current candidate's exact evidence boundary.
