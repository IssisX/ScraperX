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
| Jolt Physics | `jolt_patch.cmake`, applied by FetchContent's `PATCH_COMMAND` | `IndependentAxisConstraintPart` (the pulley constraint's solver part) never wrote a static body's lever arms and read them every velocity solve, times zero. Allocation garbage holding a NaN there made a rope made fast to a static body tear its load to NaN the first time it went taut, depending on what the allocator's reused memory held (the full native suite's intermittent NaN in AS-008 G). The patch zero-initializes the four lever-arm members. Upstream master still has the pinned code. | `scraperx_sim pulley seam`: the constraint is allocated from NaN-filled memory; it fails on the pinned code and passes patched |

| Jolt Physics | `jolt_patch.cmake`, the same script | `ContactConstraintManager` sorts an island's contact constraints for the solver by a byte hash of (body 1 id, sub-shape 1, body 2 id, sub-shape 2). A body created anywhere in the world shifts the id of every body created after it, so the contacts of islands the new body never touches are solved in another order and every moving thing drifts in its last bits (the rail split at 143 m that drove the yard crane's forklift into the dock's rail, 2026-10-20). The patch keys the sort on what the bodies are (their user data, the game's entity ids) and the sub-shapes, with the body ids breaking ties, whose relative order a new body never changes. `03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md` | `scraperx_sim body order`: the world built with a box in the yard's band, the 143 m band, the crane band and after every body; a drive at grade and a walk on the 706 m deck end bit for bit as built. Fails on the pinned code (the drive ends 11 cm off), passes patched |

Each patch is idempotent, and a checkout that holds neither the pinned nor the patched text stops the configure, so a Jolt bump re-checks it rather than dropping it.

## Bundled assets

Files shipped in the APK as they are (a `keep` import), read at runtime. Each one's license travels with it and is printed on the pause menu's LICENCES page (`godot/presentation/ui/licences.gd`).

| Asset | File | Source | License | Role |
|---|---|---|---|---|
| Cedarville Cursive, by Kimberly Geswein | `godot/presentation/uncanny/Cedarville-Cursive.ttf` (69,436 bytes, sha256 `5f86d71d08a68605d5912b9bcb639a060ce9c0debd7e0b715f5e268d4a4bc317`), unmodified | Google Fonts, `ofl/cedarvillecursive/Cedarville-Cursive.ttf` in `github.com/google/fonts` (branch `main`, fetched 2026-10-20; upstream `github.com/librefonts/cedarvillecursive` at `cd212b0e`) | SIL Open Font License 1.1: `godot/presentation/uncanny/Cedarville-Cursive-OFL.txt` (sha256 `0e441ae9f18ad0c294cc5b6ae3974c2db1614cab220598578b1b58c4cc334485`), copied from the same directory; no Reserved Font Name | her chart's handwriting on the pages in the tower (`godot/presentation/uncanny/chart_pages.gd`) |
