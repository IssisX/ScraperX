# Kit rendering / recorded footsteps checkpoint — 2026-10-04

Owner requested a Codex restart before net geometry implementation. This ledger records a partial batch, not completed net refinement or final APK delivery.

## Source

- Base: `03d50dcd2cdce65c6037ad964950ea075759826f`, exact `ChatGPT`.
- Extraction: `b140e05`, mechanism state/routines moved into `godot/presentation/kit_view.gd` with explicit native/material/sign dependencies.
- Recorded bank: `188f90e3774d356ae9c2c8255212fa7cc9097d65`, twelve small CC0 boot recordings and committed PCM import settings. Source/edits/hashes: `godot/assets/audio/footsteps/provenance.json`.
- Native C++/Jolt, world builders, camera, player/hand placement and physics tuning are unchanged.

## Evidence actually obtained

Godot 4.7 official ARM64, native library built from the preceding green executable, Ubuntu/proot on the local Android host. Rendering used X11/Mesa softpipe, Compatibility renderer, 192×164, LOW quality. These are bounded geometry checks, not target-device quality or performance acceptance.

- `kit-red.log`, `kit-fixture-red.log`: pre-extraction ownership assertion fails; remaining native pose/cable/flat-net checks pass.
- `kit-normal-green.log`: actual renderer, 29 bodies, 11,186 checks, exact original rigid mesh-channel/material/sign baseline parity plus native interpolated poses/cable endpoints/net vertices/indices/normals.
- `kit-fixture-green.log`: actual renderer, 15 bodies, 1,298 checks. Golden records are committed under `godot/tests/fixtures/`; final test defaults to those records. The saved runs used the same records through the explicit baseline argument before that default was added.
- Independent read-only review found no material introduced extraction defect. Twelve idle frames do not establish loaded transfer, bucket filling/draining, shackle disabling or restart; records omit static body transform, child visibility and material override.
- Footstep RED was executed against the original generated bank: exit 1, twelve missing recorded-resource failures. Its console output remains in the session tool history; it was not saved as a full local log.
- `recorded-green.log` is an **intermediate failure**, not a passing result: Godot defaulted to QOA (`compress/mode=2`) and all twelve PCM-format assertions failed. Committed WAV import files set `compress/mode=0`.
- `recorded-green-pcm.log`: exit 0, three surfaces, twelve distinct loaded 16-bit PCM resources. Independent Python checks also matched all twelve recorded hashes and WAV headers (mono, 22,050 Hz, 7,056 frames / 0.32 s).
- `footstep-contact.log`: five cases pass (carried, walking on carrier, rotating support, fixed floor, airborne); audio timing/controller code unchanged.
- Normal and fixture solids exports exit 0; `diff -q` against both committed collision tables exits 0. Normal: 1,316 boxes/666 hulls; fixture: 1,149 boxes/660 hulls.
- `net-red.log`: real ordinary-touch approach reaches entry (tick 1,507, position `(19.99276, 0.9, -109.9546)`), draws through X11, then exits 1 for the correctly missing rounded strands/knots. This is a proposed net-refinement test, **not implemented net behavior** and not a route pass.

## Remaining boundaries

Rounded net geometry is not implemented. Loaded net deformation/top-out in the new geometry, moving-body/cable and filled-water runtime proof, final combined-source rendering regressions, new recording subjective listening, shipping mix, exact-source Actions/APK, installation and Fold performance remain unverified. New standalone tests are not yet wired into Actions. The prior green run `37214576377` proves `b1da5f3`, not this source.

Read `RESTART_HERE_CODEX.md`, then the existing running to-do in `CONTINUE_HERE_CHATGPT_CODEX.md` §7. Continue the bounded batch after restart; do not expand into simulation/camera/world-builder refactors.

## Second quick restart checkpoint

This subsection supersedes the first checkpoint's statement that net geometry is not implemented. Rounded native-driven net geometry and two shared MultiMesh batches are saved now. Final code and three render harnesses pass Godot4.7 `--check-only` (`checkpoint-parse.log`). No native/hand/physics tuning changed.

`net-green-first.log` is a failed early test: 192×164 minimum UI scaling placed the CLIMB action over stick home. `net-green-touch.log` uses separated 432×371 controls and exits0 with ordinary grade→loaded climb→top-out→supported first ring, zero deaths, no launcher work and maximum observed native displacement0.28759m. It loaded the initial rounded implementation/test; bounds/degeneracy/empty-net guards and stronger profile/wrist assertions were subsequently edited during that run. Those final changes have parse/source-review proof only until a fresh runtime run. This limitation matters because a worker also copied scripts into the shared runtime during the parent's run; future workers must use isolated runtime projects.

Source reviews found no material introduced defect. Remaining review recommendations: require native net presence at cargo poses, exactly two wrists and knot orientation/bounds checks. Rounded cylinders/ellipsoids are presentation proxies for native triangle ribbons, not exact collision surfaces or complete warped-quad representations. Jolt's .035m particle standoff is not continuous cylindrical collision.

Saved `kit_mechanism_render_test.gd` parses but remains an unverified touch pump/fill/lift/drain draft: owner restart stopped rendering before any accepted pose. Its earlier attempt exposed empty-net indexing; final code hides absent net before reading vertices. Audio import succeeded; partial MovieMaker capture was intentionally stopped (exit130) before Master measurements. No new workflow wiring was made. Run37226116280 for609c857 remained in the historical rendered upper-route step at pause. Fresh final source, mechanics/water, mix, exact-source green APK and device/listening checks remain open. All workers and captures are stopped. See the updated restart guide for the shortest next steps.
