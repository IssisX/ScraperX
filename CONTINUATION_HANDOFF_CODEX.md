## 1. CURRENT OBJECTIVE

Finish one bounded presentation batch for **IssisX/ScraperX**, exclusively on **`ChatGPT`**:

1. Extract mechanism rendering from `godot/presentation/main.gd` into `kit_view.gd`, preserving existing behavior during extraction.
2. Improve the cargo net with substantial, rounded strands and knots that visibly follow its actual native deformation.
3. Replace the rejected synthetic footsteps with licensed recordings and verify their shipping mix.

**Observable acceptance:**

- Moving mechanism bodies, cables, visibility and bucket-water geometry follow native snapshots correctly.
- The rounded cargo net visibly deforms during ordinary player loading. Normal touch gameplay climbs from grade, tops out and reaches the existing first tower ring with zero deaths and no launcher work.
- Proven hand placement, rigid geometry/materials, interpolation and generated collision geometry remain correct.
- Recorded footsteps pass resource/contact checks and the actual Master audio-mix gate.
- Final combined source passes its exact GitHub Actions run and exports the verified APK.

The batch is unfinished. Do not expand into `simulation.cpp`, world builders, camera, telemetry, broader movement development or test-suite restructuring.

## 2. LIVE REPOSITORY STATE

Verified again **2026-10-04 at 20:01 UTC**, before adding this documentation-only file:

- **Repository path:** `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`
- **Remote:** `https://github.com/IssisX/ScraperX.git`
- **Exact branch:** `ChatGPT`
- **Executable source / HEAD at inspection:** `7844b5c96ae9b66dbf22e4c18f0409e27d33ba56`
- **Remote `ChatGPT` at inspection:** same HEAD, verified through `git ls-remote`.
- **Modified files before this document:** none.
- **Untracked files before this document:** none.
- **Worktrees:** one, at the path above.
- No matching local Godot, Xvfb or timeout processes were running during the preceding repository inspection.

Preserve these committed changes:

- `b140e05`: KitView extraction.
- `188f90e3774d356ae9c2c8255212fa7cc9097d65`: recorded footsteps.
- `7844b5c`: rounded net implementation, updated tests, unverified mechanism-test draft and restart evidence.

Current pickup documents:

- This file: `CONTINUATION_HANDOFF_CODEX.md`.
- `RESTART_HERE_CODEX.md` for detailed saved tooling/pickup information.
- `CONTINUE_HERE_CHATGPT_CODEX.md` §7 for the running to-do.
- `evidence/kit-net-audio-2026-10-04/README.md` for scoped receipts.

**The older documents' saved CI status is stale:** live inspection found the preceding run failed at the audio-mix gate. Use the live results below. Adding this handoff changes documentation only; inspect the actual resulting HEAD on pickup.

## 3. CURRENT IMPLEMENTATION STATE

### Completed implementation

**Mechanism extraction**

`kit_view.gd` owns `_kit_tube`, `_build_kit`, `_update_cargo_net`, `_render_kit`, `_lay_segment` and their body/cable/net/bucket display state.

Its interface is:

- `setup(native, palette, cable_material, cargo_net_material, create_sign)`
- `render_view()`

The module remains the direct **`KitPresentation`** child of main. Main retains startup, frame coordination, shared material preparation and sign creation.

**Rounded cargo net**

`NativeSoftCargoNet` now contains two shared MultiMesh batches:

- **207 sphere knot proxies**
- **382 cylinder strand proxies**

Connectivity comes from native indices. Centres and frames derive from native interpolated vertices. Unloaded strands are nominally **80 mm wide and 70 mm deep**. Final source includes collapsed-frame guards, transformed geometry bounds and hiding when fixture selection removes the native net.

**Recorded footsteps**

`sound_bank.gd` preloads four variants each for concrete, metal and earth. Twelve WAVs, PCM import settings, licensing and provenance are committed under `godot/assets/audio/footsteps/`.

Legacy step-generator RNG draws are consumed to preserve later procedural cues. Contact timing, cadence, pitch and mixer behavior were otherwise retained.

### Incomplete

- Fresh runtime verification of the final net guards, bounds and strengthened assertions.
- Normal/fixture rigid rendering parity against final source.
- Meaningful moving-mechanism, cable, shackle-visibility and bucket fill/drain proof.
- Repair and verification of recorded-footstep Master prominence.
- CI wiring for the four new standalone tests.
- Final green combined-source APK delivery.

`godot/tests/kit_mechanism_render_test.gd` is a saved **unverified draft**, not accepted coverage.

## 4. VERIFIED EVIDENCE

Existing receipts were inspected in `evidence/kit-net-audio-2026-10-04/`.

| Check actually performed | Result and boundary |
|---|---|
| Extraction, actual renderer, normal world | PASS: 29 rigid bodies, 11,186 checks; original mesh-channel/material/sign parity plus poses, cables and original flat net. |
| Extraction, actual renderer, fixture world | PASS: 15 bodies, 1,298 checks. These were extraction-baseline checks, not final rounded-net verification. |
| Rounded-net ordinary-touch rendered route | PASS: grade → loaded climb → top-out → first ring; Y=11.9, zero deaths, no launcher work; maximum observed native displacement **0.28759 m**. |
| Final four scripts, Godot `--check-only` | PASS: `kit_view.gd`, `kit_view_test.gd`, `kit_route_render_test.gd`, `kit_mechanism_render_test.gd`. Parsing alone does not establish runtime behavior. |
| Recorded-resource test | PASS: three surfaces, twelve distinct loaded 16-bit PCM variants. |
| Footstep contact test | PASS: five cases covering carried support, walking on carrier, rotating contact, fixed floor and airborne state. |
| Generated solids, extraction source | Both comparisons passed byte for byte: normal 1,316 boxes/666 hulls; fixture 1,149 boxes/660 hulls. |

**Important route-proof boundary:** the successful rounded-net run loaded an earlier implementation/test snapshot. Final guards/bounds and stronger wrist/profile assertions were edited during that run. A worker also updated files in its shared runtime. Therefore, this receipt does **not** establish final-HEAD runtime acceptance.

The mechanism draft passed parsing, but its rendered run was stopped before any accepted pose. The local audio Movie Maker capture was stopped before measurements; it is not a mix pass.

### Live GitHub Actions evidence

**Preceding source `609c857`:** [run 37226116280](https://github.com/IssisX/ScraperX/actions/runs/37226116280) completed **failure**.

Its native suite, imports, camera/input checks, uninterrupted normal ascent to 121 m, historical rendered route and fixture input checks succeeded. Step 16, **“Measure the mix a player hears,” failed**:

- Footstep metric: **−18.9 dBFS**
- Ambience metric: **−22.6 dBFS**
- Separation: approximately **3.7 dB**, below the required **4 dB**
- Exit code: **31**

The audio bank, director and UI test driver are unchanged between that failed source and executable source `7844b5c`.

**Executable source `7844b5c`:** [run 37228283068](https://github.com/IssisX/ScraperX/actions/runs/37228283068) was still **in progress** at 20:01 UTC, in the historical rendered upper-route step. No terminal result or accepted APK yet.

Last confirmed green delivery remains source `b1da5f3d240c9c7834b759a1e0aabee7288e6603`, run `37214576377`, artifact `11309435499`. It does not prove this batch.

## 5. SCRAPERX CONTRACTS THAT MATTER RIGHT NOW

- Native **C++/Jolt** owns consequential physics, contacts, deformation and traversal. Godot renders snapshots and handles input/audio.
- Preserve existing `KitPresentation` rigid-body paths, especially `KitBody1950/1951/2900`, used by slingshot presentation.
- Cargo net **entity 1953** is a native Jolt soft body: 9×23 four-corner knots, open ribbon triangles and pinned end rows.
- Rounded display geometry is a **proxy**, not newly matching cylindrical collision. Jolt’s `.035 m` vertex radius is particle standoff. Native ray/grip queries still use triangles.
- Knot ellipsoids and uniform cylinders approximate warp, taper and twist. Do not describe them as exact native collision geometry.
- Existing hand placement is owner-confirmed accurate. Preserve native grip anchors, wrist placement and shared interpolation.
- Keep the proven normal ascent route. Retired fixtures provide fixture evidence, not normal-world traversal proof.
- APK builds happen in GitHub Actions. Acceptance requires a **green exact-source run**, not merely an exported APK.
- Screenshots are optional diagnostics, not mandatory acceptance gates.

## 6. SETTLED DECISIONS

- Geometry preservation applied to the extraction. The subsequent cargo-net task deliberately changes display geometry.
- Keep the rounded native-driven MultiMesh design; independent source review found no material implementation defect.
- Use actual recordings for footsteps. Do not return to tweaking the rejected synthetic step recipes.
- Do not weaken the existing audio prominence or clipping gates to obtain green.
- Touch-driven render tests use **432×371**; passive parity tests may use the smaller viewport.
- A bounded mechanism-display test may use legitimate public machine commands in a retired fixture, with real native evolution. Assigning volumes or poses is not acceptable.
- Stop after this batch; broader refactoring and content development remain separate.

Local tools remain available:

- Godot executable: `/data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64`
- Ubuntu wrapper: `proot-distro login ubuntu --shared-tmp`
- Local renderer: Xvfb, Compatibility, `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=softpipe`
- Task root: `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao`, containing `runtime-kit`, `runtime-green`, `runtime-audio-recorded` and scoped proof directories.
- Native library in `runtime-kit`: SHA-256 `9ff20444d0733bdcf6ff2353ee4d80aa2025f8c39d9b3e324d1d08a75f615198`.

Use separate runtime copies for concurrent workers. Do not overwrite scripts in another worker’s running project.

## 7. BLOCKERS / KNOWN FAILURES

- **Actual delivery failure:** recorded footsteps fail the shipping Master prominence gate. The precise repair has not been established.
- Current executable-source Actions run has no terminal result yet.
- Final rounded-net and mechanism runtime coverage remains incomplete.
- None of `kit_view_test`, `kit_route_render_test`, `kit_mechanism_render_test` or `recorded_footsteps_test` is wired into the workflow.
- Remaining review gaps: cargo poses can skip net assertions when native vertices are empty; wrist checks do not require exactly two wrists; knot orientation/bounds lack direct checks.
- Subjective sound quality and Android device appearance/performance remain unverified.

## 8. EXACT NEXT ACTION

**Resolve the shipping recorded-footstep mix failure first.** Inspect the newest result of run `37228283068`, then reproduce and diagnose the existing Movie Maker `audio_mix` probe at the current executable source.

The relevant owners are `sound_bank.gd`, `audio_director.gd` and `_audio_mix()` in `presentation/ui/ui_test_driver.gd`; the workflow command is under **“Measure the mix a player hears.”** Preserve its 4 dB prominence and −0.5 dBFS peak requirements. Establish the cause before choosing a gain, sample or mixing repair.

HANDOFF COMPLETE
