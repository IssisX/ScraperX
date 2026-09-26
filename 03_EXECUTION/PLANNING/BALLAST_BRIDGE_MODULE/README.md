# Ballast bridge - reusable prototype module

**Inactive source archive.** This package preserves the separate ballast-powered bridge built locally on 2026-09-26. It is excluded from the CMake build and Godot project and does not change normal gameplay. The owner authorized preserving it for another model after stopping concurrent branch work.

The current pipe-loaded balance bridge remains the active `AS-016` task. This archive neither replaces that task nor changes the Atlas. The original prototype's comments, tests and optional workflow patch also say `AS-016`; that is historical provenance, **not a second active ticket**. Use the descriptive identity `ballast-bridge-prototype-v1` when discussing this archive.

## What can be reused

One normal-input route: reachable T-grip -> tension-only control cable -> three-metre overhead brake arm -> finite passive brake releases -> ten-tonne ballast descends -> main cable raises a hinged bridge -> nose contacts visible caps -> player crosses onto the existing +11 m tower deck. No timed stage trigger, scripted bridge pose, ascent motor or completion flag supplies the lift.

| Item | Archived construction and observation |
|---|---|
| Source | 10,000 kg guided ballast, initially centred `(22,13.8,-118)`; observed descent 9.178 m |
| Bridge | 12,000 kg effective compound-body mass; 32 m long, 4 m wide; hinge `(14,0.16,-92)` about +X |
| Main cable | 1:1 tension-only pulley constraint; fixed bearings `(14,19,-118)` and `(22,19,-118)`; **corrected 1 MN breaking limit** |
| Control | Free 3 kg T-grip near `(7,1.10,-84)`; actual trip cable to 35 kg / 3 m arm hinged at `(22,20.15,-118)`; 45 Nm passive hinge friction |
| Brake | Maximum 150 kN resistance; arm angle continuously opens it. Governor limited to 150 kN, 1.3 m/s, nominal 0.6 m/s² approach deceleration |
| Arrival | Nose about 10.762 m, slope about 19.24 degrees; native fixed receiver top 11 m; normal player crosses onward to existing tower support at `(23,-126)` |
| Stops and hold | Nose lugs contact caps before the emergency hinge limit. Hanging ballast maintains about 98.1 kN cable tension after arrival |
| Bodies and IDs | Frame/receiver 1016; bridge 2016; ballast 2017; T-grip 2018; brake arm 2019. Four dynamic kit bodies, one static kit body; the player is additional |

These coordinates, IDs and geometry describe the archived prototype, not a placement contract for the active pipe bridge. The two routes occupy overlapping space. Check current allocations and physical clearances before any adoption; do not instantiate both by copying a second builder call.

## Files and ownership

| File | Purpose |
|---|---|
| `source/src/sim/macro_bridge.cpp` and `.hpp` | Complete final native assembly and continuous brake law. These are the canonical new source files in this package |
| `source/tests/macro_bridge_tests.cpp` | Complete native route and causal failure tests |
| `integration.patch` | Exact changes to the eleven existing runtime/build/test files needed by those three new files; based on commit `360cffbe2374a37cd6e7b606f72443f26c0d8f93` |
| `ci-reference.patch` | Separate, **unexecuted** delivery-workflow changes for later adaptation; never applied merely by storing this archive |
| `manifest.json` | Baseline, dependencies, final source hashes, source identity and evidence scopes |
| `runtime.SHA256SUMS` | Checks every reconstructed changed runtime file against the actual local source |
| `SHA256SUMS` | Integrity of the archive's other files, including raw evidence |
| `evidence/` | Original logs, explicitly separated into corrected-source results, interrupted work and earlier-source results |

`scraperx_sim` and Jolt remain the only physics owners. Godot forwards existing grab/movement input and reads native body parts, transforms and telemetry. The C++ `Bridge::build(kit)` constructs the assembly once; `Bridge::pre_step(kit)` updates passive brake resistance before `Kit::pre_step` and the native 90 Hz physics step.

The integration patch includes these actual seams:

- `mechanism_kit.*`: finite guide brake and hinge friction; signed brake work and rigid-body kinetic energy; checkpoint preservation of guide work/peak speed. It reuses the existing rope/guide/hinge solver.
- `simulation.*`: default-world construction, normal-input grip reach, read-only snapshot values, and two C++ proof interventions for source mass and cable removal. Those interventions are **not** bound to Godot player controls.
- `scraperx_simulation.*`: `get_macro_bridge_state()` exposes observed state. It does not command movement.
- `main.gd`, `audio_director.gd`, `ui_test_driver.gd`: native-derived geometry/cables, reachable grip label, motion-driven sound cues, and ordinary-spawn touch/keyboard/gamepad routes.
- `CMakeLists.txt`, `simulation_tests.cpp`: compile the new module/test and update the default foundation's explicit body-count assertions while preserving the legacy fixture distinction.

Do not copy only the three new files and call that integration complete. Conversely, do not overwrite current `simulation.cpp`, the Kit or Godot files with a whole earlier version. Review the patch against their present owners and bring across only the necessary compatible changes.

## Physical model boundaries

The head brake is a reduced, finite passive-force law reflected to the ballast's guide coordinate. Its resistance depends continuously on the actual arm angle. It does not simulate individual pads/cams or rotating-sheave inertia. Cable geometry uses native ideal tension-only constraints; the visible sheave housings are static. Cable rupture uses the existing Kit rule of exceeding its limit on two consecutive native steps, not a fatigue/material model. Effective body masses and the chosen cable limit are simulation parameters, not structural certification.

The prototype's route and energy tests do not establish every conceivable interference case, cross-platform deterministic replay, Android execution or complete structural fidelity. Checkpoint restore is the game's existing save-state mechanism. There is no player-facing instant recharge of the ballast; an incomplete pull can be released, approached and completed with the same fallen grip. Existing ordinary stairs and parkour remain available.

## What the evidence establishes

| Evidence file | Observation | Claim boundary |
|---|---|---|
| `native-current-pass.log` | Final standalone native suite passes: ordinary route to original +11 m support, 45 s idle, 300 kg insufficient source, disconnected main cable, work/energy bounds, real fatal-fall checkpoint restore, partial pull/re-grab, passive passenger ride, 30 t overload slipping the finite brake, three pull speeds at both 60 and 30 Hz frame partitions | Final corrected module; native simulation, not device observation |
| `touch-30fps-current-headless-pass.log` | Normal touch events from unchanged spawn release the bridge and reach the original tower deck, alive, with native-driven audio cue counters | Final corrected module; no renderer or audible output in this run |
| `current-host-build.log` | Native library, Godot extension and test executables link | Compilation, not an APK |
| `current-full-suite-interrupted.log` | CTest started and was stopped at the owner's request | **No final full-suite verdict** for the corrected snapshot |
| `prior-250kn-render-failure.log` and `prior-250kn-diagnostic-failure.log` | Rendered touch route failed at stopping impact; main cable exceeded 250 kN for two steps, measured about 304/345 kN | Earlier, undersized cable. This is rejected evidence, retained to expose the real failure |
| `prior-250kn-keyboard-pass.log` and `prior-250kn-gamepad-pass.log` | These ordinary-input routes passed before the cable correction | Earlier snapshot only; they did not discriminate the failing 30 FPS touch timing |

The archived code contains the correction: main cable limit **1,000,000 N**, plus the expanded native timing cases. The headless 30 FPS touch route and the native suite then passed. Do not regress to the earlier 250 kN source from the local ancestor commit. No corrected rendered touch run, completed corrected full regression suite, GitHub gameplay-candidate run, APK or device execution was produced before the owner stopped work.

One recorded no-passenger run released 900,351 J of ballast potential energy, gained 620,243 J in the bridge's actual compound centre of mass, and measured 277,228 J brake dissipation. About 2,880 J remains for contact/damping/numerical losses (0.32%). These are observed values in the native log, not inputs chosen to force the test to pass.

## Reconstruct without touching the other model's checkout

From the current repository root, use a **new detached local worktree** at the recorded baseline. These commands do not move any branch or start CI. The worktree command refuses an already occupied destination.

```bash
bridge_module="$(pwd)/03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE"
bridge_repro="$(dirname "$(pwd)")/ScraperX-ballast-repro"
(cd "$bridge_module" && sha256sum --check SHA256SUMS)
git worktree add --detach "$bridge_repro" 360cffbe2374a37cd6e7b606f72443f26c0d8f93
git -C "$bridge_repro" apply --check "$bridge_module/integration.patch"
git -C "$bridge_repro" apply "$bridge_module/integration.patch"
cp "$bridge_module/source/src/sim/macro_bridge.cpp" "$bridge_repro/src/sim/macro_bridge.cpp"
cp "$bridge_module/source/src/sim/macro_bridge.hpp" "$bridge_repro/src/sim/macro_bridge.hpp"
cp "$bridge_module/source/tests/macro_bridge_tests.cpp" "$bridge_repro/tests/macro_bridge_tests.cpp"
(cd "$bridge_repro" && sha256sum --check "$bridge_module/runtime.SHA256SUMS")
```

Stop on any failed command; do not force an application or use `--reject`. A shallow clone may need the baseline commit fetched first. Do not apply this historical patch directly to the active working tree. Current Atlas, pipe-bridge ticket, planning changes and prototype evidence are intentionally absent from the patch.

To rebuild and exercise the reconstructed module using the pinned dependencies and Godot 4.7:

```bash
cd "$bridge_repro"
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel 2
./build/host/scraperx_macro_tests
ctest --test-dir build/host --output-on-failure
godot --headless --path godot --audio-driver Dummy --fixed-fps 30 -- --uitest=touch_macro
godot --headless --path godot --audio-driver Dummy --fixed-fps 60 -- --uitest=keyboard_macro
godot --headless --path godot --audio-driver Dummy --fixed-fps 60 -- --uitest=pad_macro
mkdir -p build/bridge-captures
xvfb-run -a -s '-screen 0 1296x1114x24' godot --path godot --rendering-method gl_compatibility --audio-driver Dummy --fixed-fps 30 -- --uitest=touch_macro "--capture=$(pwd)/build/bridge-captures/bridge"
```

`godot` above means the repository-pinned 4.7 binary available on PATH; substitute its actual executable path if necessary. `xvfb-run` requires Xvfb/xauth or an equivalent real display. The rendered route should capture `macro_ready`, `macro_moving`, `macro_seated`, `macro_climb` and `macro_exit`; inspect those images and require the actual PASS result. The final rendered rerun is a pending check, not a promise of success.

## How the current implementation owner should use it

Choose the smallest justified reuse: the full alternative route, or specific Kit/carry/telemetry/input-proof changes useful to the pipe bridge. The owner's request to save this archive is not an instruction to replace the active pipe-loaded design or publish another gameplay candidate.

Before adoption, resolve source/ID/geometry conflicts against the current branch, retain one solver and one mechanism owner, and preserve all parkour and fallback routes. Relocating or changing the mechanism invalidates its old route/clearance evidence and requires renewed native and rendered checks. Keep source-mass/cable-removal proof controls outside the player API.

Before any gameplay delivery, complete the corrected rendered path, full retained regressions, normal and regression collision-export comparisons, and relevant input routes. Adapt the optional CI patch rather than replacing the active workflow. Only the designated branch writer should publish one candidate and follow its exact SHA through proof and APK verification. Current integrated delivery status remains owned by the root `00_START_HERE.md`.
