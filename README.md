# ScraperX

Godot 4.7 / C++17 / Jolt industrial ascent. Read [`00_START_HERE.md`](00_START_HERE.md) before changes. Writes go to `ChatGPT` only.

## Current game

The owner-directed ground reset removes the legacy campaign, including the water screw/lift, intake machinery, upper lifts and machine-linked stair assemblies, from normal play. The tower structure, alpine setting, full parkour/controller and ordinary stairs/ramps remain. Those stairs are the optional fallback for a player who wants a straightforward route. No replacement macro mechanism is implemented yet.

The revised [Atlas](01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md) proposes a pipe-loaded balance bridge from grade to a +8 m receiver. It includes preliminary geometry/work estimates and explicitly unresolved release, capture, stopping and recovery requirements. The [macro plan](03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md) defines delivery order; the [corrected catalogue](Mechanism-Ideas-and-archetypes.md) adapts the supplied Colossus ideas.

Legacy machinery is retained only as an explicitly selected desktop/native regression fixture. Its test success is not campaign progress. AS-001–015 are retired; do not resume their queue.

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


Default Godot launch loads the cleared foundation. `--uitest=ground_foundation` proves default input and removal. Legacy UI scenarios explicitly load regression fixtures. To regenerate collision tables, use normal `--export-solids=<path>` for `src/sim/world_solids.inc`, and add `--regression-fixtures` for `tests/fixtures/world_solids.inc`.

CI runs the default-world proof and rendered capture, retained physics/controller regressions, both collision drift checks, Android arm64 build and APK export. Its artifact contains exact-commit checksums and evidence boundaries.

## Claim boundary

An exported APK proves packaging. Installation, Android execution, device readability, touch ergonomics and sustained Fold 6 performance remain separate evidence states. No new mechanism is established by this cleanup or by a passing legacy fixture.
