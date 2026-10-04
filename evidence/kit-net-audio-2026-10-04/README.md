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

## Verified baseline delivery `7844b5c`

Independent verification on 2026-10-04 found exact-source [run 37228283068](https://github.com/IssisX/ScraperX/actions/runs/37228283068) **successful**, attempt 1, job `111512396228`, for `7844b5c96ae9b66dbf22e4c18f0409e27d33ba56` on `ChatGPT`. This supersedes the saved in-progress status for that source. Documentation-only successor `ac45fa0` preserves its executable source. Selected verification metadata is retained in [ci-7844b5c/receipt.json](ci-7844b5c/receipt.json).

- [Artifact 11314460651](https://github.com/IssisX/ScraperX/actions/runs/37228283068/artifacts/11314460651), `ScraperX-build-7844b5c96ae9b66dbf22e4c18f0409e27d33ba56`: independently downloaded ZIP is 55,658,866 bytes; SHA-256 `34d5578287f9e42f4d87ebaa0eceb421146349d4872479fe8f958bba49b5ac10` matches GitHub's artifact digest.
- `apk/ScraperX-ChatGPT.apk`: 33,561,468 bytes; independently recomputed SHA-256 `54c0407348789ad6365eb581dc9694d09e1efd94ebaa0d12dc3eac8e0c43d844` matches `apk/SHA256SUMS.txt`. `apk/CHECKPOINT.txt` names the exact source above, package `com.cory.scraperx.chatgpt` and label `ScraperX-ChatGPT`; actual CI badging confirms that identity and `arm64-v8a`.
- Actual CI signing logs report `Verifies`, verified v2/v3 schemes and one signer with certificate SHA-256 `9a06889e7614d140f6cd1fc45634bb1e2391968a9fcc60f1608c9e50e15a1ba4`, matching the checkpoint. Local APK cryptographic signature verification was not run.
- CI passes **33/33 native tests**, **23/23 fixture input scenarios**, ordinary grade-to-121 m with zero deaths and launcher work, historical rendered grade-to-110 m, normal/fixture solids comparisons, Android ARM64 compilation and APK export. The actual Master mix passes with bed **−22.6 dBFS**, steps **−18.3 dBFS** (approximately **4.3 dB** separation), peak **−0.8 dBFS** and ten cues; all five fall recordings report zero failures.

The full batch remains unfinished: this baseline workflow omits `kit_view_test`, `kit_route_render_test`, `kit_mechanism_render_test` and `recorded_footsteps_test`. Its green result does not establish those dedicated checks or a later combined candidate's acceptance. The preceding [run 37226116280](https://github.com/IssisX/ScraperX/actions/runs/37226116280) failed the recorded-footstep prominence gate at −18.9 dBFS steps versus −22.6 dBFS ambience, exit 31; relevant audio-owner files are unchanged between that source and this successful baseline, so the differing outcome remains **unresolved**. No subjective listening, APK installation, Android execution or sustained device performance is established by this receipt.

## Current continuation audio repair

The original recorded bank fails all 36 new production-controller material/variant/phase cases. Isolated original cues gain little post-limiter body when their level increases: high crest and weak energy above 300 Hz make peak gain ineffective. Prior CI outcomes near the 4 dB gate varied with cue pitch/window alignment. The replacement uses recorded Fantozzi hard-surface Foley, Eelke/congusbongus metal and Ali_6868 gravel; twelve full short clips receive resampling and −1.5 dBFS peak normalization, with no new EQ/compression. An earlier compressed alternative was evaluated privately and not adopted. Source credits and license differences are preserved in the exported in-game notice and provenance.

[The actual 36-case log](audio-2026-10-04/recorded-mix-36.log) and [receipt](audio-2026-10-04/receipt.json) prove all cases pass at unchanged requirements: minimum prominence concrete **7.03 dB**, metal **6.95 dB**, earth **7.53 dB**; every raw captured peak **−0.80 dBFS**, 4–5 cues per case. It uses the actual production AudioDirector/buses/pool/limiter with controlled component inputs. Master capture is post-limiter **before** the Master volume fader; at default 0.8 the final output is another 1.9382 dB lower, preserving separation. Full-scene native-input mixing, final render/CI/APK delivery and subjective listening are separate boundaries. The preceding sections retain historical results and do not describe current completion.

## Delivery frontier after owner scope clarification

The water screw/lift is constructed only in explicit historical regression mode (`simulation.cpp` fixture guards; `main.gd` regression-only builder). The owner clarified that those stages were retired. The extra new fill/lift/drain renderer gate is removed; existing native/input historical regressions remain unchanged. Partial lift-harness attempts are not acceptance evidence and required no production physics change. The retained AS-006 shackle display follows real native hook/unhook visibility in three actual-renderer poses (99 checks).

The final integrated native-input audio probe passes: bed **−23.2 dBFS**, recorded footsteps **−13.0 dBFS**, prominence **10.2 dB**, peak **−0.8 dBFS**, ten cues. It executes unchanged Main/native/input/AudioDirector/UiTestDriver with real X11 MovieMaker audio; a documented local adapter disables only 3D drawing after the first frame. A fully drawn local attempt timed out at 300 wall seconds/142 frames due software rendering cost and returned no audio verdict. Exact-source Actions retains the standard rendered mix gate. Automated evidence supports this delivery; device listening and performance remain separate. The owner requested proportional checks, so no further optional local test runs are planned.

## Root-owned repair of first final CI blocker

Exact-source candidate `11e99fb` failed [run 37236422395](https://github.com/IssisX/ScraperX/actions/runs/37236422395) at normal KitView; all following new tests/audio/export gates were skipped and no APK was published. [The immutable failure receipt](ci-11e99fb/receipt.json) and focused actual CI log record both assertions.

The visible-body guard incorrectly required the hidden rigid proxy of native pouch2900 to draw. Existing SlingshotView intentionally replaces it with its native leather surface. The corrected check requires that actual owner to have a visible complete mesh and the native render position. All other enabled-body drawable guards remain. This introduced harness error is reproduced locally and repaired; production/native/slingshot code required no change.

The raw serialized mesh-byte oracle matched current ARM output against the stored pre-extraction record but failed on x86 CI. The original CI log lacks its first divergent field, so its precise byte-level cause remains unresolved. The new oracle preserves complete original mesh channels/materials/signs as compressed numeric records captured from exact pre-extraction Main `03d50dcd2cdce65c6037ad964950ea075759826f`, with native/SlingshotView source unchanged. A `1e-4` tolerance handles tiny floating-point differences while channel types/counts/topology/text remain enforced; any larger mismatch reports its exact path.

[The root repair receipt](kit-ci-repair/receipt.json) and actual logs establish normal **32,811 checks/29 bodies** and fixture **1,695 checks/15 bodies**, both exit0 on Godot4.7 ARM64 / X11 / softpipe. One combined negative control changes a vertex by10mm and hides the actual leather surface; both assertions reject it, exit1. These are bounded geometry checks with Dummy audio. The corrected combined candidate still requires its x86 exact-source Actions and verified APK; completed audio and route work is not rerun locally. The owner reinforced root ownership of difficult repairs and proportional checking; after delivery wait for the owner's new goal.
