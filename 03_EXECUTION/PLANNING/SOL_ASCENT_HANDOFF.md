# Sol ascent implementation package

> **For agentic workers:** Open the installed Superpowers root `SKILL.md`, then `skills/executing-plans/MODULE.md`. Apply the installed Causal Mechanism Compiler and `$threespine` to the relevant physical interfaces. The owner selected a model handoff: execute this package after they switch models; do not ask for permission at every local construction or test step.

**Goal:** Deliver the existing +77→88 m braced bay and implement a substantial new +88→110 m north service-frame climb, preserving continuous ascent from grade and all earlier working machinery.

**Architecture:** Native Kit parts own new collision and provide the same geometry to Godot rendering. Existing native contact, climbing, hanging, balance, jump, mantle and checkpoint code owns traversal. Godot exercises that implementation through ordinary viewport input and renders the result.

**Tech stack:** C++17, pinned Jolt, Godot 4.7/GDExtension, native 90 Hz simulation, GitHub Actions Android delivery.

**Spec:** [Atlas §10](../../01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md#10-upper-tower) owns AS-021 dimensions and AS-022 source geometry. [AS-021](../ASCENT/AS-021_BRACED_BAY.md) and [AS-022](../ASCENT/AS-022_NORTH_SERVICE_FRAME.md) record calculations and observed native tests. [Start Here](../../00_START_HERE.md) §2 owns delivery status. Keep these names/headings/IDs.

## Scope and decision boundary

This is one multi-milestone coding assignment: the braced bay gameplay finish, a two-level climbing section, recovery tests, presentation and complete delivery. Do not stop after adding a screenshot, one ledge or one test. Continue through +110 m unless a concrete blocker requires a new topology or kernel change.

The difficult owner decisions already resolved are the active/retired split, one native/render geometry source, capsule contact versus step geometry, inherited teeter top-out clearance, checkpoint edge-perch failure, movement limits and proof boundaries. AS-022 now has a locally compiled/native-tested layout and a bounded hanging-clearance owner repair; rendered play and exact-source delivery are still gates. The handoff instructions below remain the acceptance checklist, with the actual observations in the AS-022 ticket.

A new gravity machine above +110 m is a later design boundary, not an excuse to replace this section with repeated lifts or poles. The larger +300 m/1,600 m goals remain unchanged.

## Workspace and publication

- Repository `IssisX/ScraperX`, write branch `ChatGPT` only. Recover local changes and remote head before editing. Never force-push or discard other agents’ work.
- Current agent clone: `/data/data/com.termux/files/usr/tmp/scraperx-agent-dlm6eNWc/repo`. It is outside personal Downloads. Reuse this agent clone if present; otherwise create an independent clone in agent temporary storage. Do not use the owner’s original files.
- `build-make/` is an untracked local build directory, not source to stage. Its presence is not a fresh-clone dependency. APK builds use GitHub Actions only.
- Preserve stable author APK filename `ScraperX-ChatGPT.apk`, label `ScraperX-ChatGPT`, package `com.cory.scraperx.chatgpt` and branch signer checks.
- One candidate workflow at a time. Identify any pending handoff run by its exact head SHA. While it runs, implement the next local milestone and update the running to-do; publish the next candidate after the previous run ends.

## Task 1 — complete AS-021 normal rendered gameplay

**Files:** `godot/presentation/ui/ui_test_driver.gd`; `.github/workflows/wo000-delivery-spine.yml`; AS-021 evidence and Start Here status. Native reference: `tests/braced_bay_tests.cpp`.

**Interface:** `_touch_braced_bay() -> bool` consumes a successful `await _touch_teeter()` and produces grounded tower support 11 near capsule Y=88.9 with zero deaths. Register `touch_braced_bay` at normal grade spawn 8 and include it in the existing normal-world scenario exclusion list; never configure a regression spawn for this route.

- [x] Add the new scenario/dispatch and port the five-step sequence in AS-021. First establish failure at the unimplemented continuation; then add the inputs. Keep every old `touch_teeter` assertion and earlier capture. Current-source Godot execution is still pending.
- [ ] During the gap jump, translate the native world-space steering command into view-local stick input exactly as `_walk_to` already does:

```gdscript
var to := target - Vector2(_position().x, _position().z)
var world_command := (to / 1.4).limit_length(1.0)
var yaw := float(_main._yaw)
var forward := Vector2(-sin(yaw), -cos(yaw))
var right := Vector2(cos(yaw), -sin(yaw))
_move_dir(device, Vector2(world_command.dot(right), world_command.dot(forward)))
```

Use the existing touch jump/crouch button events, `_act`, `_face`, `_wait_until`, `_pose` and `process_frame`; no direct position, velocity or native movement setters. `_walk_to` does not turn the camera: face along each narrow beam before entering it. Stop and settle on the flat approaches before changing axes.

- [ ] Capture `braced_bay_entry`, `braced_bay_transfer`, `braced_bay_low_member`, `braced_bay_upper_junction`, `braced_bay_arrival`. Assert each PNG exists in CI and inspect actual images; a file-existence check is not visual verification. Final log must distinguish walking-surface +88 from capsule-centre +88.9.
- [ ] Extend the existing rendered continuous route invocation to the new scenario and retain previous pose requirements. Run headless input first if available, then the actual renderer at the existing tested settings. Preserve shipping viewport settings after CI’s temporary capture settings.
- [ ] Record a bounded AS-021 delivery result and commit only the relevant source/evidence. An upstream CI failure remains a failure even when this local test passes.

## Task 2 — implement both levels of AS-022

**Create:** `src/sim/north_service_frame.hpp`, `.cpp`; `tests/north_service_frame_tests.cpp`; `03_EXECUTION/ASCENT/AS-022_NORTH_SERVICE_FRAME.md`.

**Modify:** `CMakeLists.txt`; `src/sim/simulation.cpp`, `.hpp`; Atlas §10; existing active-world count assertions in `tests/simulation_tests.cpp` and workflow; later the touch driver. Do not rename existing IDs. Proposed static entity 1901 and next appended test spawn need a fresh availability check at the implementation head.

**Interface:**

```cpp
// north_service_frame.hpp, includes sim/mechanism_kit.hpp
namespace scraperx::sim {
void build_north_service_frame(kit::Kit &kit);
}
```

Call once immediately after `build_braced_bay(*kit_)` in the active `WorldContent::PipeBridge` branch. Follow `braced_bay.cpp` for explicit box spans and a static compound. Add exactly one authoritative part list; no matching handwritten Godot colliders. Test-only entry on earned +88 m support does not alter the normal campaign spawn.

- [x] Survey actual north-ring geometry and select entry/receiver poses, with native collision observations recorded in AS-022.
- [x] Write and run native route assertions for entry, both tower receivers and a missing-route failure before construction. Use ordinary movement and traversal inputs between route beats.
- [x] Implement and test the lower subsection, lateral turn and +99 receiver; record source dimensions in the Atlas and ticket.
- [x] Implement and test the upper subsection, blocked first top-out, shimmy pocket and +110 mantle.
- [x] Provide frame/tiebacks, rests and real release/retry contact. First ladder pacing and visual readability remain subject to screenshot/play inspection.
- [ ] Verify ordinary movement from AS-021’s actual +88 arrival along the tower to the AS-022 entry. Then verify the connected native +77→110 route; focused section spawns alone cannot establish continuity.
- [x] Add the executable/CTest and workflow target. Update the active-world count for exactly one added Kit body.

### Source-derived geometric constraints

All source constants below were read at the handoff baseline; recheck if another agent changed the controller. These are feasibility bounds, not permission to author at every maximum.

| Quantity | Constraint and consequence |
|---|---|
| Standing capsule | Height 1.80 m, radius 0.35 m; crouched height 1.20 m. Sweep the whole capsule, including top-out, around adjacent members. |
| Step / mantle | Step ≤0.35 m; mantle rise 0.35–1.85 m. Target 1.2–1.6 m for isolated authored mantle rises [CHOSEN margin]. A tall wall cannot become a mantle by changing its label. |
| Hold classifier | Two thin dimensions ≤0.18 m, length ≥0.25 m. Use 0.12–0.16 m hand members [CHOSEN], supported by visibly substantial frame members. Do not treat a thick column as a grip. |
| Climbing | Up speed 0.9 m/s; hand search half extents (0.30,0.30,0.35) m and 0.18 m maximum hold section. Start with 0.30 m rung pitch [CHOSEN] and prove continuous acquisition/sweeps. Gap transitions need an actual new grip or a jump. |
| Hanging | Ledge 0.45–1.35 m above capsule centre; shimmy 0.6 m/s. Keep forced traverse ≤2 m [CHOSEN], with a visible open top-out pocket. |
| Jump | Initial vertical speed 5.5 m/s, g=9.81 m/s²; ideal apex 1.542 m and same-height duration 1.121 s [DERIVED]. Use actual takeoff speed and collision tests; a 6.167 m ideal maximum range is not a safe authored gap. |
| Rest / recovery | Choose clear rest footprint at least 1.4×1.4 m [CHOSEN]; check side clearance independently. Bound exposed transfer drops to intended real receiving geometry; no inferred survival from height alone. |
| Work / fixed frame | 22 m player elevation adds 85×9.81×22 = 18,344.7 J potential energy [DERIVED], supplied by locomotion. Static supports perform zero work. Structural deformation/load rating is not simulated. |

CMC profile is `MACRO-TRAVERSAL-STRICT`; use LINK for these static support interfaces. There is no dynamic machine coordinate for `stage1dof.py` to integrate. ThreeSpine applies to capsule/ledge geometry, contact, trajectory and movement state transitions. Do not write fictional force/energy sweep results for static level geometry.

## Task 3 — failure, recovery and input variation for the complete section

**Files:** `tests/north_service_frame_tests.cpp`; AS-022 ticket; narrowly scoped native owner only if evidence identifies a defect.

- [x] Demonstrate the upper jump/hang, a deliberate release onto the +103.5 rest and a normal-input retry. AS-021 gap/recovery remains a retained separate test.
- [x] Repeat the raised hanging transfer from two additional takeoffs, (15.0,−180.8) and (15.2,−180.5), and run the complete focused route with 60 and 360 Hz callers feeding the native 90 Hz fixed step. These are sampled cases, not a continuous robustness envelope.
- [x] Exercise real fatal falls after +99 and +110, with 20 seconds no input, stable tower footing and continued movement after +99. Death counts are cumulative 1 and 2.
- [ ] Use the AS-021 negative cases as regression coverage: no-jump gap, standing obstruction, side departure and the existing teeter grip top-out. If a kernel issue emerges, reduce it to a discriminating failing case before editing the shared owner.

## Task 4 — full touch, visual readability and exact-source delivery to +110 m

**Files:** `godot/presentation/ui/ui_test_driver.gd`; `.github/workflows/wo000-delivery-spine.yml`; AS-022 evidence, Atlas status, Start Here §2 and the running plan.

- [x] Add `_touch_north_service_frame() -> bool` calling `_touch_braced_bay()` first, with a normal-spawn scenario and viewport events. Its runtime pass and zero-death claim await CI.
- [ ] Explicitly capture the north entry, lower transfer, +99 rest, hanging/top-out pocket, upper transfer and supported +110 arrival. Show holds and landing depth from ordinary first-person distance. Inspect images; repair misleading framing, intersecting geometry and obscured exits at their owner.
- [ ] Preserve native machine physics tests, prior normal route, regression inputs/render/audio, both collision-table comparisons, fixed-tick/replay checks and author APK identity. Do not edit generated world-solids tables for Kit-owned geometry; only regenerate them if their actual Godot-owned source changed.
- [ ] Publish to `ChatGPT`, identify the exact SHA’s workflow and follow it to terminal status. Build/export the APK in Actions, download it into agent temporary storage, verify its source metadata/signature/checksum and report those actual results. A successful artifact upload alone does not prove every required gate ran.
- [ ] Update the running to-do and current evidence without changing historical run claims. Separate desktop input/render, Android build, installed execution and sustained Fold performance. Stop at the supported +110 handoff or report the exact blocker; do not invent an upper-machine design to fill a progress report.

## Verification at this handoff

The earlier AS-021 source, including its corrected inner post, passed 20/20 selected local tests. The new AS-022 source rebuilt and its native test passed through both tower receivers, release/retry, three catch starts and two stable fatal-fall checkpoints at 90, 60 and 360 Hz caller partitions. A selected CTest run passed **21/21** after the support-member addition and before the final +99 checkpoint/partition assertions; the final three native partitions were rerun separately and passed. The earlier teeter and AS-021 tests are among those 21. Independent review reproduced AS-021 +88 arrival and its unattended checkpoint retry before the small post correction.

The broad ARM executable was separately run and still fails the already reproduced retired AS-002 mid-landing case; it is not included in the 21/21 claim. The full x86 CI gate remains mandatory. Current-source Godot parse/render, continuous touch +88/+110 and current-source APK are not established by the local native result. The preceding source `2e622f7` passed [Actions run 36781607829](https://github.com/IssisX/ScraperX/actions/runs/36781607829), still proving rendered play only through +77 m.

## Running to-do

- [x] Native AS-021 geometry, source-derived movement closure and negative cases.
- [x] Shared checkpoint root cause and focused fix; 20-second retry.
- [x] Independent review, support-post correction and final 20-test rerun.
- [x] Substantial Sol package, bounded section framing, file ownership and acceptance.
- [x] Follow the prior `2e622f7` candidate to successful exact-source Actions completion; it proves the previous +77 m rendered boundary.
- [ ] Task 1: AS-021 full rendered touch and visual inspection.
- [x] Task 2: AS-022 lower and upper native section through +110 m, with one Kit/render part list.
- [x] Task 3: release, retry, varied catch starts, 90/60/360 Hz caller partitions and both checkpoint recoveries pass native.
- [ ] Task 4: continuous rendered grade→+110 m, retained gates and exact-source APK.
- [ ] Device execution and sustained Fold performance when the device path actually permits observation.
