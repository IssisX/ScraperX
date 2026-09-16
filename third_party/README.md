# Dependency authority

WO-000 uses CMake `FetchContent` so CI can reproduce the exact dependency graph without committing generated third-party source.

| Dependency | Exact commit | Role |
|---|---|---|
| godot-cpp | `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` | Godot 4.7 GDExtension ABI boundary |
| Jolt Physics | `e77f175595e64cb44218cc9d9d56fc365ad0e36a` | Native rigid/contact substrate reserved by the TDD |

The bridge never transfers consequential-state ownership to Godot. Jolt is compiled in the native graph but WO-000 deliberately creates no physics world or gameplay behavior.

