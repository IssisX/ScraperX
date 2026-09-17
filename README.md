# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Current work order

**WO-003 — Athletic Traversal**

```text
touch / WASD movement, look facing, contextual action
  -> ScraperX GDExtension command seam
  -> native 90 Hz Jolt player body
  -> native geometry probe against the real collision world
  -> validated mantle / vault / ledge-hang in the support body's own frame
  -> inherited support motion on completion
  -> immutable snapshot
  -> Godot first-person tower-deck presentation
```

Completed before it: WO-000 delivery spine, WO-001 embodied authority, WO-002 moving-support truth.

This is the third embodied slice of the full game: one persistent 1.6 km skyscraper climbed through athletic industrial traversal and large-scale Rube-Goldberg physics mechanisms. WO-003 deliberately stops before the fall/parachute/checkpoint loop, freight machinery, structural coupling, and process systems.

Traversal assistance never fabricates geometry. Each mantle, vault, and ledge grab requires a real wall hit, a real top surface on the same body, a rise inside the reach band, a supported landing, and a capsule clearance query at the actual landing pose. A committed traversal is driven through Jolt every tick and is aborted — not forced through — when real geometry blocks it.

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

The GitHub Actions workflow performs the bounded WO-003 proof path, including the native traversal test suite, Linux GDExtension loading, a runtime that walks to a real ledge and mantles it, one rendered first-person capture, Android arm64 cross-compilation, Godot export, APK inspection, checksums, and artifact publication.

## Claim boundary

An APK artifact proves production by the current source. It does not prove installation, execution on Android, or Fold 6 behavior. Those remain separate evidence states.
