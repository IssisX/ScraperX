# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Where the project is

Two classes of work, defined once in [`00_START_HERE.md`](00_START_HERE.md) §2 and never sharing
an identifier:

- **Kernel Work Orders** `WO-000`–`WO-013` in `03_KERNEL/` — foundational engineering that proves
  one tool type each on the `KX-*` substrate. A **closed set**. All fourteen are in source.
- **Ascent Slices** `ASC-01`–`ASC-15` in `04_ASCENT/` — the actual 1.6 km climb, one Atlas band
  at a time. This is where new work happens.

`B00`–`B11` are Atlas band IDs — floors of the tower. They are never filenames and never tickets.

**Proven** (CI run `35651140478`, all steps green): 10 native falsifiers at exit 0; Godot 4.7 at
Fold inner-panel aspect `2160x1856`; Android arm64 APK carrying `libscraperx_native.so`.

The kernel proves: a native 90 Hz Jolt player; moving-support point velocity; vault / mantle /
ledge / hang; a coupled steam plant whose every link reads the previous link's real body state;
fall, parachute and automatic commit; freight on a finite jib; a seated beam that changes
traversal; a process volume that decides whether a grate is walkable.

`ASC-01` builds the first real campaign ground: an apron, an 18 m intake belt, a 12 m / 5 t yard
jib, a 4 t pack physically pinning a landing dog, a switchback stair and an exposed facade ladder
to +24 m. The stair is shut because freight is standing in the dog's swing, and it opens because
the jib moved that freight.

**Next:** `ASC-02` — first legal stand at +40 m. Planned, not coded.

Still absent: bands B01–B11 above the hall deck, NPCs, missions, and the summit.

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

The GitHub Actions workflow performs the full proof path: the full native suite, Linux GDExtension loading, a runtime rendered at the Galaxy Z Fold 6 inner-panel aspect (2160x1856) that walks the tower approach and observes the coupled machine work, one rendered first-person capture, Android arm64 cross-compilation, Godot export, APK inspection, checksums, and artifact publication.

## Claim boundary

An APK artifact proves production by the current source. It does not prove installation, execution on Android, or Fold 6 behavior. Those remain separate evidence states.
