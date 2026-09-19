# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Current work order

**WO-007 — First process coupling** is implemented in source on `ScraperX-Grok`.

`KX-SUMP` is a lumped isolation volume with a local valve and a drain cock. `KX-GRATE` is a hole while the volume is wet and ordinary support only after isolate+drain empties inventory. Dumping the isolation line refills the volume and restores the fall. The grate sits on the +22 m cage-house landing, after `KX-NEEDLE` and `KX-CAGE`.

Do not start WO-008 in this change. Do not assemble B01–B11.

## Claim boundary

| Class | This change |
|---|---|
| Implemented | Yes — native sump/grate + Godot process station presentation |
| Built | Host compile unverified in this environment until Actions |
| APK produced | Not claimed until the Android Actions job publishes an artifact |
| Installed | Not claimed |
| Executed / observed | Not claimed |
| Verified on Fold | Not claimed |

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

An APK artifact proves production by the current source. It does not prove installation, execution on Android, or Fold 6 behavior. Those remain separate evidence states.
