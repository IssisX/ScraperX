# Dependency authority

ScraperX uses CMake `FetchContent` so CI can reproduce the exact dependency graph without committing generated third-party source.

| Dependency | Exact commit | Role |
|---|---|---|
| godot-cpp | `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` | Godot 4.7 GDExtension ABI boundary |
| Jolt Physics | `e77f175595e64cb44218cc9d9d56fc365ad0e36a` | Native rigid/contact substrate reserved by the TDD |

The bridge never transfers consequential-state ownership to Godot. Beginning with WO-001, the pinned Jolt graph owns the player rigid body, static-world collision, and support contacts; Godot receives only commands and immutable presentation snapshots.

## Patches on the pinned graph

| Dependency | Patch | Why | Proof |
|---|---|---|---|
| Jolt Physics | `jolt_patch.cmake`, applied by FetchContent's `PATCH_COMMAND` | `IndependentAxisConstraintPart` (the pulley constraint's solver part) never wrote a static body's lever arms and read them every velocity solve, times zero. Allocation garbage holding a NaN there made a rope made fast to a static body tear its load to NaN the first time it went taut, depending on what the allocator's reused memory held (the full native suite's intermittent NaN in AS-008 G). The patch zero-initialises the four lever-arm members. Upstream master still has the pinned code. | `scraperx_sim pulley seam`: the constraint is allocated from NaN-filled memory; it fails on the pinned code and passes patched |

The patch is idempotent, and a checkout that holds neither the pinned nor the patched text stops the configure, so a Jolt bump re-checks it rather than dropping it.
