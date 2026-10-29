# Operations notes: how to build, prove and playtest

Five facts that were only in the archived integrator handoff of 2026-10-28
(`git show 5a663eb2:03_EXECUTION/handoffs/INTEGRATOR_HANDOFF_2026-10-28.md`, "CI facts" and "Local
tooling"). Each is checked against the workflow where it can be. Times are Central.

1. **A head commit with `[skip ci]` is skipped on push.** To prove such a branch, dispatch
   `wo000-delivery-spine.yml` on the `<agent>/integrate-<job>` branch instead of pushing.
   Source: the handoff, "CI facts". Off trunk, step 20 ("Preserve Android updates") still stops
   with the expected KeyError (`CLAUDE.md`, "Test only what a change reaches", item 4).
2. **Trunk runs publish `ScraperX-build-<sha>`** (the `artifacts/` folder: logs, evidence and the
   signed APK; kept 90 days). **`ScraperX-APK-<sha>` is no longer a trunk output:** the
   `playtest-apk` job runs only on a manual dispatch (workflow comment, owner 2026-10-28; the
   handoff's evening update says the same). Source: the handoff; checked against
   `.github/workflows/wo000-delivery-spine.yml` (artifact names at "Publish immutable checkpoint
   build and evidence" and "Publish the playtest APK").
3. **`touch_matron_pipe` fails in the cloud container, even on trunk.** The handoff's first note
   says "CI passes it"; its evening update says "CI does not run it". The workflow agrees with the
   update: `touch_matron_pipe` is not in the scenario list of the touch step. Do not read its
   failure here as a regression, and do not count it as proved by CI. Source: the handoff, "CI
   facts" and the 6:30 PM update; checked against the workflow's scenario loop.
4. **In an agent worktree, use the main checkout's Godot:** `tools/godot/godot` of the main
   checkout with `--path <worktree>/godot`. Source: the handoff, "CI facts".
5. **The Android NDK is lost with the container.** It was `29.0.14206865` at
   `/root/android/ndk/29.0.14206865` (gone in a new container; checked: the folder does not exist
   now). The playtest zip: build `scraperx_native` for arm64 with CI's flags (NDK `29.0.14206865`
   clang, the CMake flags in `wo000-delivery-spine.yml`), put the library in
   `godot/addons/scraperx_native/bin/android/arm64/`, and zip `godot/` without `.godot/` and the
   linux library. The owner opens it in the Godot 4.7 editor on his phone (`00_START_HERE.md`
   rule 48). Source: the handoff, "Fast playtests" and "Local tooling".

## The proof's layout (parallel jobs, 2026-10-29)

`wo000-delivery-spine.yml` runs as parallel jobs: `host-build` (host native build, shared as an
artifact) and `android-build` (arm64 cross-compile) start at once; then `native-tests` (4 shards:
the unsliced athletic_traversal run with shared_survival and parkour_flow, whose log carries every
falsifier PASS line; and three groups of the Matron ctest slices, the last of which takes every slice
not named in the others), `godot-touch` (2 shards, the same scenario list as before, split at
`touch_o`), `godot-audits-and-mix` (Fold runtime proof, the mix, solids, floating audit and the GDScript
tests) and `android-apk`; last `publish-checkpoint` (evidence, signing, the trunk build artifact).
Green means every job green but `publish-checkpoint`, which off trunk stops with the expected
`sign-apks.py` KeyError. The old single job ran the full native binary twice (once under ctest, once
directly for its PASS lines); it now runs once, under `ctest -V`.
