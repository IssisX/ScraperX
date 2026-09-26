# Dependency authority

ScraperX uses CMake `FetchContent` so CI can reproduce the exact dependency graph without committing generated third-party source.

| Dependency | Exact commit | Role |
|---|---|---|
| godot-cpp | `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` | Godot 4.7 GDExtension ABI boundary |
| Jolt Physics | `e77f175595e64cb44218cc9d9d56fc365ad0e36a` | Native rigid/contact substrate reserved by the TDD |

The bridge never transfers consequential-state ownership to Godot. Beginning with WO-001, the pinned Jolt graph owns the player rigid body, static-world collision, and support contacts; Godot receives only commands and immutable presentation snapshots.
