# Sol ascent implementation package

> **For agentic workers:** Open the installed Superpowers root `SKILL.md`, then `skills/executing-plans/MODULE.md`. Apply the installed Causal Mechanism Compiler and `$threespine` to the relevant physical interfaces. The owner selected a model handoff: execute this package after they switch models; do not ask for permission at every local construction or test step.

**Goal:** Deliver the existing +77→88 m braced bay and implement a substantial new +88→110 m north service-frame climb, preserving continuous ascent from grade and all earlier working machinery.

**Architecture:** Native Kit parts own new collision and provide the same geometry to Godot rendering. Existing native contact, climbing, hanging, balance, jump, mantle and checkpoint code owns traversal. Godot exercises that implementation through ordinary viewport input and renders the result.

**Tech stack:** C++17, pinned Jolt, Godot 4.7/GDExtension, native 90 Hz simulation, GitHub Actions Android delivery.

**Spec:** [Atlas §10](../../01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md#10-upper-tower) owns AS-021 dimensions and AS-022 framing. [AS-021](../ASCENT/AS-021_BRACED_BAY.md) records calculations, observed tests and the exact touch-port sequence. [Start Here](../../00_START_HERE.md) §2 owns delivery status. Keep these names/headings/IDs.

## Scope and decision boundary

This is one multi-milestone coding assignment: the braced bay gameplay finish, a two-level climbing section, recovery tests, presentation and complete delivery. Do not stop after adding a screenshot, one ledge or one test. Continue through +110 m unless a concrete blocker requires a new topology or kernel change.

The difficult owner decisions already resolved are the active/retired split, one native/render geometry source, capsule contact versus step geometry, inherited teeter top-out clearance, checkpoint edge-perch failure, movement limits and proof boundaries. AS-022 is authorized framing, **not a fully compiled or runtime-proven layout**. Sol owns bounded placement and validation inside that framing. There is no evidence-based way to promise that one model can solve every remaining issue another cannot; escalate a demonstrated unresolved physical/kernel defect, not routine level geometry or a failed first attempt.

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

- [ ] Add the new scenario/dispatch and port the five-step sequence in AS-021. First establish failure at the unimplemented continuation; then add the inputs. Keep every old `touch_teeter` assertion and earlier capture.
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

- [ ] Survey actual north-ring solid geometry at +88/+99/+110, including outer edge beams, posts and diagonals. Use `godot/presentation/main.gd` `_build_stack`, the corresponding native tower construction and `src/sim/world_solids.inc`. Record usable entry/receiver poses and clear capsule volumes before placing holds.
- [ ] Write native route assertions for entry support, the +99 intermediate receiver and +110 final receiver. Run and observe the missing-route failure. Advance through `Simulation::kFixedStepSeconds`; input APIs are `set_move_input`, `set_facing`, `request_traversal`, `request_jump` and `request_release`. Read their existing route-test uses before assigning an action. Never teleport between route beats.
- [ ] Implement the lower subsection inside the Atlas envelope: short climb, lateral transfer, rest, change of approach and next climb to +99. Test the full lower subsection before building above it. Use actual native affordances and capsule sweeps to select final hold/landing positions; record dimensions in Atlas and the ticket as source geometry once they exist.
- [ ] Implement the upper subsection with a short hanging traverse to a clear top-out pocket and a distinct final mantle sequence to +110. A canopy may obstruct top-out at the entry portion but must leave a reachable clear pocket; test standing body and hand clearance, not only the camera. Legal alternatives remain legal.
- [ ] Provide visible frame/tiebacks and useful rest/recovery surfaces. Each rest must permit turning, checkpointing and an ordinary next action. Avoid a broad continuous fallback stair, an automatic lift, a full-height pole or excessive repeated vertical input.
- [ ] Verify ordinary movement from AS-021’s actual +88 arrival along the tower to the AS-022 entry. Then verify the connected native +77→110 route; focused section spawns alone cannot establish continuity.
- [ ] Add the new executable and CTest to CMake and the workflow build target list. Count assertions should reflect the actual added body; do not simply remove the checks. Commit the complete native section with its evidence.

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

- [ ] For each gap/transfer, demonstrate an ordinary successful attempt and a deliberately missed/released attempt. Observe real support ID, position, grounded/hang/climb state and death count. Falling onto a catch deck is success only if the player can leave it and retry through ordinary input.
- [ ] Repeat critical transfers with at least two additional takeoff positions or input timings and a non-90 Hz caller frame partition while retaining the native 90 Hz fixed step. Record tested values rather than claiming a continuous robustness envelope from three examples.
- [ ] After earning each tower receiver, exercise one real fatal fall, restore, then wait 20 simulated seconds with no input. Require one death, stable footing and normal movement away. Do not loosen `footing_is_firm`, force velocity to zero or save an airborne point to make recovery pass.
- [ ] Use the AS-021 negative cases as regression coverage: no-jump gap, standing obstruction, side departure and the existing teeter grip top-out. If a kernel issue emerges, reduce it to a discriminating failing case before editing the shared owner.

## Task 4 — full touch, visual readability and exact-source delivery to +110 m

**Files:** `godot/presentation/ui/ui_test_driver.gd`; `.github/workflows/wo000-delivery-spine.yml`; AS-022 evidence, Atlas status, Start Here §2 and the running plan.

- [ ] Add `_touch_north_service_frame() -> bool` calling `_touch_braced_bay()` first. Register a normal-spawn scenario; port the proven native inputs through viewport events. Require supported +99 and +110 arrivals and zero deaths in the continuous grade route. Keep recovery demonstrations as separate cases so the zero-death claim remains meaningful.
- [ ] Explicitly capture the north entry, lower transfer, +99 rest, hanging/top-out pocket, upper transfer and supported +110 arrival. Show holds and landing depth from ordinary first-person distance. Inspect images; repair misleading framing, intersecting geometry and obscured exits at their owner.
- [ ] Preserve native machine physics tests, prior normal route, regression inputs/render/audio, both collision-table comparisons, fixed-tick/replay checks and author APK identity. Do not edit generated world-solids tables for Kit-owned geometry; only regenerate them if their actual Godot-owned source changed.
- [ ] Publish to `ChatGPT`, identify the exact SHA’s workflow and follow it to terminal status. Build/export the APK in Actions, download it into agent temporary storage, verify its source metadata/signature/checksum and report those actual results. A successful artifact upload alone does not prove every required gate ran.
- [ ] Update the running to-do and current evidence without changing historical run claims. Separate desktop input/render, Android build, installed execution and sustained Fold performance. Stop at the supported +110 handoff or report the exact blocker; do not invent an upper-machine design to fill a progress report.

## Verification at this handoff

The final local source, including the corrected inner post, rebuilt successfully. `ctest --test-dir build-make --output-on-failure -E '^scraperx_sim.athletic_traversal$'` passed **20/20**, 30.23 s. This includes AS-021, prior normal routes and teeter physics. Independent reviewer also reproduced native +88 arrival and the unattended checkpoint retry before the small post correction.

The broad ARM executable was separately run and still fails the already reproduced retired AS-002 mid-landing case; it is not included in the 20/20 claim. The full x86 CI gate remains mandatory. Current-source Godot parse/render, continuous touch +88, AS-022 construction, +110 route and current-source APK are not established by this local result. The last fully green desktop/APK delivery remains `dfcfa53` through +77 m until a newer exact-source run proves otherwise.

## Running to-do

- [x] Native AS-021 geometry, source-derived movement closure and negative cases.
- [x] Shared checkpoint root cause and focused fix; 20-second retry.
- [x] Independent review, support-post correction and final 20-test rerun.
- [x] Substantial Sol package, bounded section framing, file ownership and acceptance.
- [ ] Follow the pending handoff candidate’s exact-source Actions result, if present.
- [ ] Task 1: AS-021 full rendered touch and visual inspection.
- [ ] Task 2: AS-022 complete lower and upper native section through +110 m.
- [ ] Task 3: misses, release, retries and input variation.
- [ ] Task 4: continuous rendered grade→+110 m, retained gates and exact-source APK.
- [ ] Device execution and sustained Fold performance when the device path actually permits observation.
