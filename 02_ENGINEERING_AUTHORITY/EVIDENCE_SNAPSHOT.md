# ScraperX TDD — External Evidence Snapshot

This file is evidence/provenance, not product or architecture authority.

## Godot 4.7
- Official Godot 4.7 Android export documentation confirms Android export and Gradle/custom build paths.
- Official Godot 4.7 GDExtension documentation confirms native shared-library integration.
- Official Godot 4.7 `godot-cpp` documentation identifies `godot-cpp` as the official C++ GDExtension binding.
- Godot 4.6 made Jolt the default 3D physics engine after removing its experimental label; 4.7 documentation describes Jolt as built in and the default for new projects.

## Jolt
- Jolt's official repository lists Android ARM32/ARM64 support and C++17.
- Jolt documents multithreaded simulation, deterministic same-binary simulation, and an optional cross-platform deterministic build mode with performance cost.
- Jolt documents CharacterVirtual moving-platform support, but current/open issue history also contains moving-platform/dynamic-body instability reports. Therefore ScraperX does not delegate its constitutional moving-support behavior without its own regression suite.

## godot-cpp build
- Official Godot documentation states SCons is the main godot-cpp build system and CMake is an actively supported secondary build system.
- Current godot-cpp CMake configuration exposes Godot API 4.7 targeting.

## XPBD
- Macklin, Müller, and Chentanez (2016) introduced XPBD to reduce timestep/iteration dependence of constraint stiffness and provide force estimates. It is retained only as a rigging candidate pending ScraperX-specific benchmarks.

Sources were checked 2026-09-16 against official Godot/Jolt repositories/docs and the XPBD publication.
