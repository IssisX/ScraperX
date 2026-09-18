# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Current work order

**WO-005 — First freight mechanism** is implemented in source on `ScraperX-Grok`.

`KX-JIB` is a finite 5 t-class machine. `KX-CRATE` is a Jolt body on a real hook constraint. Local pendant verbs are Raise / Lower / Slew / Brake. Action enters or exits the station; it does not hoist.

Native tests cover in-SWL lift, overweight stall, locked-brake hold, travel-limit stop, remote-command rejection, and riding the crate as moving support. Machine pose is stored in the WO-004 commit blob.

Do not start WO-006 in this change. Do not assemble B01–B11.

## Claim boundary

| Class | This change |
|---|---|
| Implemented | Yes — native freight + Godot pendant presentation |
| Built | Host compile unverified in this environment |
| APK produced | Not claimed |
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
