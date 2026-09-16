# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Current work order

**WO-000 — Delivery Spine**

```text
Godot 4.7 app
  -> ScraperX GDExtension
  -> native 90 Hz fixed-step scraperx_sim
  -> immutable diagnostic snapshot
  -> Android arm64 APK
```

No gameplay systems belong in this work order.

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

The GitHub Actions workflow performs the complete proof path, including Linux GDExtension loading, one rendered diagnostic capture, Android arm64 cross-compilation, Godot export, APK inspection, checksums, and artifact publication.

## Claim boundary

An APK artifact proves production by the current source. It does not prove installation, execution on Android, or Fold 6 behavior. Those remain separate evidence states.

