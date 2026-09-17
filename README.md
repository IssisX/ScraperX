# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Current work orders

**WO-004 Fold Stage · WO-005 Exterior Grade and Approach · WO-006 First Coupled Machine**

```text
device bounds -> Fold inner-panel viewport, frustum and HUD
grade spawn   -> walk the approach -> tower's lower third fills the frame,
                 crown sheared off by haze and stack plume
hoist -> ballast -> hinged tipper -> tension-only rope -> valve lever
      -> real orifice -> finite pressure vessel -> actuator cylinder
      -> piston -> counterweighted platform the player rides
```

Completed before them: WO-000 delivery spine, WO-001 embodied authority, WO-002 moving-support truth, WO-003 athletic traversal.

The machine is not scripted. Each link reads the previous link's actual body state, so it can be entered mid-cycle, walked around, looked up into, blocked, and started early by standing on the tipper. The plant is a reduced-order pressure vessel with finite stored energy: cut the boiler feed and the lift fades and stops. The steam plume is a one-way consumer of the native orifice mass flow — shut the valve and it dies.

Still deliberately absent: the fall/parachute/checkpoint loop, load-bearing structural coupling, process/isolation networks, persistence, NPCs, missions, and interior routes into the tower.

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

The GitHub Actions workflow performs the bounded WO-004..006 proof path: the full native suite, Linux GDExtension loading, a runtime rendered at the Galaxy Z Fold 6 inner-panel aspect (2160x1856) that walks the tower approach and observes the coupled machine work, one rendered first-person capture, Android arm64 cross-compilation, Godot export, APK inspection, checksums, and artifact publication.

## Claim boundary

An APK artifact proves production by the current source. It does not prove installation, execution on Android, or Fold 6 behavior. Those remain separate evidence states.
