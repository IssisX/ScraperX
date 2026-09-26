# AS-016 integration evidence

Profile: **MACRO-TRAVERSAL-STRICT**. Local ARM64 desktop observations, 2026-09-26. Exact-SHA CI/APK status belongs to `00_START_HERE.md`; no Android installation/performance claim follows from this directory.

## Final production construction

`src/sim/pipe_bridge.cpp` and Kit build the same assembly used by ordinary `Simulation` and the normal Godot scene. There is no candidate flag or presentation controller supplying motion. The final arms are at X=3.2/8.8, prop at X=2.5; pan/rack sides are open retaining rails; native grip contact and visible hands agree. IDs 1500..1535 and 2500..2531 are unique. Archived ballast IDs are absent.

`tests/pipe_bridge_tests.cpp` exercises public native movement/hand input from ordinary spawn. Six cases pass: full route and fatal-fall material restore; no input; rack only; empty-pan release; partial pull/regrab; early boarding/passive ride. The final route reaches original tower support at player centre Y=11.9. The optional stairs and unoperated default checkpoint also pass. Current full legacy-suite ARM64 failure is reproduced on untouched `4cdb2bc`, at the old AS-002 mid-landing; it remains a required CI gate.

`tests/pipe_bridge_refinement.cpp` constructs production Kit/PipeBridge directly at 90 and 360 Hz. Finite 150 N pulls act at 3–5 s (rack) and 14–16 s (bridge). No body/velocity setters or duplicated mechanism geometry. Tolerances are the 7.53–8.20 m receiving corridor, <50 mm final-tip difference, <10 mm plastic-front difference, and no ultimate-floor contact.

| INTEGRATED quantity | 90 Hz | 360 Hz |
|---|---:|---:|
| Retained native pipes | 20 | 20 |
| Final tip | 8.02333 m | 8.00219 m |
| Post-turn tip envelope | 7.98728–8.03119 m | 7.98890–8.01427 m |
| Plastic front | 0.457362 m | 0.452477 m |
| Irreversible crush work | 41.1626 kJ | 40.7230 kJ |
| Total control work | 83.5208 J | 83.5202 J |
| Remaining energy loss after crush | 120.368 kJ | 120.986 kJ |
| Peak kinetic energy, complete loading/release sequence | 54.9080 kJ | 59.4832 kJ |
| Peak pan pitch | 0.0127242 rad | 0.00182617 rad |
| Minimum sloped-floor/ultimate-floor clearance | 0.118215 m | 0.123636 m |

Energy observation includes all dynamic bodies' gravity and principal-inertia kinetic energy, material elastic energy, actual hand work and permanent crush work. Remaining loss includes pipe/rack impacts and friction; it is not separately measured per-contact dissipation. The nominal 0.65 m constitutive stroke is geometrically limited to about 0.58 m by the sloped floor's eventual contact. Current run leaves >0.11 m actual floor clearance. The kinematic material collider follows plastic compression and omits up to 6 mm elastic indentation; only the pan striker drives that material law.

Reproduce with the pinned CMake build, then `ctest --test-dir build/host -R pipe_bridge --output-on-failure`. Run `scraperx_pipe_bridge_refinement` directly for quantitative output. The standard workflow builds both test executables and gates all seven cases.

## Input, presentation and sound

The existing `ui_test_driver.gd` now includes `pipe_bridge`, `keyboard_pipe_bridge` (60 FPS) and `touch_pipe_bridge` (30 FPS). Events go through ordinary viewport input; the tests require offered controls, actual held IDs, twenty captured pipes, compression, beam support and original tower support with no deaths. They use normal startup, without a special scene override.

`pipe_bridge_capture.gd` separately captures four real player positions reached through native input. Images were inspected locally: rack ready, loaded pan, raised bridge and tower arrival. Local software-rendered 640×480 inspection is not a Fold performance measurement. PRoot emits host `getcwd` errors during capture/shutdown; the route itself passes. Exact-source CI renders the touch route separately.

Human fall audio uses five CC0 recordings with asset-local provenance. Local Movie Maker decoded the audio during an actual native high fall: two reactions, measured mix peak 0.361820, no test failures. Ordinary jumps, variation, per-fall limits, recovery/pause/death and parachute cancellation are checked. This verifies signal output, not subjective listening or Android audibility.

## Earlier copied prototype — historical only

`full_mechanism.cpp`, `guided-*`, control logs/CSVs, `control-results.json` and the older `production-route.log` preserve the preceding complete-chain prototype. That source has X=3.4/8.6 arms and opaque retaining walls and precedes final grip/clearance/collider changes. Its 90/360 Hz observations remain useful provenance; they are **not** the final production refinement above. `SHA256.json` fingerprints this historical set. `production/` fingerprints final source and current logs separately.
