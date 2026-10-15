# ScraperX parkour and climbing — phased execution plan

**Goal:** Deliver the complete movement system requested2026-10-15 onChatGPT, from research through actual connected gameplay and an APK.
**Architecture:** One native contact planner coordinates existing finite hand/foot owners and the missing compression-only surface contacts. Jolt owns motion; Godot consumes receipts. Preserve all unrelated work.
**Stack:** C++17, pinned Jolt, native90Hz,85kg dynamic capsule, Godot4.7, Android.
**Spec:** [Complete researched move-set](PARKOUR_MOVEMENT_SPEC_20261015.md).

## Set → implement → accept → close → set next

Only ONE active simple goal. Acceptance checks belong to that implementation goal; they are never a new verification-only goal. A phase can contain several short goals where ownership demands it. Complete means its promised usable behavior and meaningful negatives work, not merely that code compiles. Record source/evidence once and reuse until affected source changes. On success close the active tool goal, update this ledger and immediately create the next authorized implementation goal. Preserve all unfinished scope. Two failed uncertain-mechanic trials/time expiry trigger a supported causal reassessment through existing mechanics_trial.py, not an endless parameter loop.

| Phase | Simple goal to set | Includes | Completion/next goal |
|---|---|---|---|
| 0 — Research/spec | Define the entire move set and physical integration contract | Original biomechanics/coaching; human vs primate limits; every trigger/input/physics/timing/feedback/transition; audit targets and tuning policy | Written spec and this plan saved; then setPhase1 |
| 1 — Responsive takeoff and intent | Make run→jump and late/early input flow through real foot contact | Audit existing owners first; sprint-jump carry, native intent buffering, contact-valid trailing-plant coyote grace, cancellation, no repeated held actions; reuse accepted existing behavior | Real early/late positive +empty-air/stale/reset negatives, frame-delivery equivalence; then setPhase2 |
| 2 — Connected climbing | Make jump→one/two-hand catch→climb/shimmy→physical top-out→onward run work | Existing affordances/independent hand generations; predictive catch ranking; moving/material support; legal shared hand/foot transition; blocked/degraded recovery; preserve physical production mantle routing and keep legacy drivers fixture-only | Public connected positive and blocked/one-hand/release/reset cases; then setPhase3 |
| 3 — Physical vaults | Carry running momentum over obstacles through actual palm/foot support | Compression-only palm/forearm owner, step/safety and speed/kong-style topologies, finite work/traction/reach; remove production kinematic vault; real far-side footing | Public low-obstacle vault/running exit and blocked/no-contact/dynamic-support negatives; then setPhase4 |
| 4 — Wall movement | Turn real wall plants into brief wall runs and wall jumps | Ground/vertical/lateral contact planning, finite friction-limited wall-foot actuation, tic-tac, generation lockout, ledge-catch transition, separation/failure | Public vertical/lateral wallrun→jump/catch plus no-contact/low-traction/work/reach/reset negatives; then setPhase5 |
| 5 — Land and keep moving | Land into a roll or slide and continue without a forced stop | Actual impact/prepared compression and directional recovery; grounded crouch slide with passive dissipation; momentum/clearance/stand-up; buffered onward Jump | Actual landing receipts/runout/blocked/edge/restart and onward move; then setPhase6 |
| 6 — Unified presentation and controls | Make every move readable and responsive on touch, pad and keyboard | Snapshot/bridge phases and actual loads; planted hands/feet; bounded camera blending/free-look/reduced motion; material/contact audio/visuals; contextual prompts without losing tool/machine controls | Meaningful native→Godot transactions and additive return-to-neutral; then setPhase7 |
| 7 — Playgrounds throughout the place | Make the move set fun to chain through real environments | Connected ground loading/utility/duct loops, alternate precision/fast routes, service-deck and rooftop opportunities, visible goals and recoverable failures; use Mirror’s Edge’s consistent movement metrics and route-network principles within ScraperX industrial identity | Actual public-input chains through several connected locations, clear receiving surfaces and retry paths, tangible exported collision; then setPhase8 |
| 8 — Connected release | Deliver the full move set as committed playable code and a matching APK | Connected chains and recovery, relevant existing route regressions, justified integrated native/Godot/Android checks; fix introduced defects; source commit/push toChatGPT and actual artifact | Every requested move production-reachable, no stubs/canned motion; publish commit/APK/spec/tuning notes, accurately separate full proof from device feel |

Parallel work is elastic: root codes the integration path; independent workers own bounded actuator, presentation or input modules only after interfaces/acceptance are fixed. One writer per file. Use appropriate skills at each task transition, low/medium for routine work and high for consequential contact design; xhigh only when high leaves material uncertainty. No fixed staffing quota, model/tier substitution or AI build watchers.

## Initial source ownership map (audit resolves exact seams)

- Root: `src/sim/simulation/{tick,locomotion,legacy_traversal,hand_transfer}.cpp`, `world.hpp`, publicSimulation/Snapshot integration, CMake and final delivery. Split new owner-correct files where useful, not another monolithic controller.
- Finite actuators: `physical_hand_climb.*`, `physical_foot_push.*`, later compression-only palm/wall contacts. Do not change accepted constitutive laws through a coordinator.
- Planning/sensing: `traversal_affordance.*`, `traversal_plan.*`, `traversal_receiver.*`, native listener contact identity.
- Presentation: `src/bridge/scraperx_simulation.*`, `godot/presentation/main.gd`, `first_person_arms.gd`, `landing_camera_response.gd`, `audio/*`, `ui/input_router.gd`, existing touch/HUD contextual seams.
- Meaningful native/public tests alongside the existing movement owners; Godot input/presentation checks alongside existing tests. Avoid broad unrelated test campaigns.

## Current goal ledger

- Phase0: COMPLETE — research-backed spec and full phased plan written2026-10-15.
- Phase1: NEXT — audit live movement owners; implement actual missing responsive takeoff/intent behavior.
- Phases2–8: PENDING; all listed scope retained. Existing capabilities earn credit only after audit identifies their actual public behavior/evidence; do not rewrite working mechanics for activity.

## Tuning ownership

Existing constants/actuator APIs own physics. New intent/selection/recovery bounds live in one native tuning surface with units, defaults, owners and finite ranges. Presentation settings remain in Godot and cannot change support/forces. Exact final knobs and measured outcomes are delivered withPhase8. Spec numeric hypotheses are not claims of an implemented or proven operating range.


## Added owner scope — usable parkour terrain

Owner2026-10-15 requests objects and scenarios throughout the place, inspired by Mirror’s Edge. DICE’s own movement designer describes contextual intent and learnable combinations ([EA movement basics](https://www.ea.com/amp/news/movement-in-mirrors-edge-catalyst-the-basics)); its design director describes a rough whole-world route network, consistent geometry metrics, multiple paths, embodied first-person readability and avoiding dead zones ([developer interview](https://www.gamedeveloper.com/design/how-i-battlefield-i-devs-learned-to-build-an-open-world-for-i-mirror-s-edge-catalyst-i-), [EA first-person design](https://www.ea.com/en-au/amp/news/the-world-from-faiths-perspective)). Apply these principles to functional industrial spaces: clear run-ups, linked changes in height/direction, alternate risky/precise lines and physical recovery. No blind clutter scattering, borrowed assets, automatic safety nudges or additional wheel encounters. New movement needs actual destinations and return routes, not just a flat mechanics fixture. A bounded independent west-yard blockout can progress while native mechanics are integrated; it earns no gameplay acceptance until root integrates tangible native collision and plays the public paths.


### Ring utilization owner steering

Every ring offers a large9m-wide service band; use selected sectors to create reasons to cross or revisit the deck. Scope includes route braids, layered maintenance galleries, confined workrooms, cross-ring machine load paths, suspended transfers, moving material/receivers, precision versus momentum lines and recoverable failure loops. All kinds of physically causal industrial experiences are allowed; no restrictive machine whitelist. Existing large wheels are accepted and finished. Mere dressing on a continuous ring remains optional: claiming a sector is needed requires a real progression/operation/recovery dependency. Preserve current earned arrivals and do not install a generic bypass ladder or repeated lift. Exact occupied regions and proposed meaningful height handoffs are being mapped before authoring consequential geometry.
