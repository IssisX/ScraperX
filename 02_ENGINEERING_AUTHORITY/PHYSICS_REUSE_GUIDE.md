# Reusing ScraperX physics

Read this when authoring or repairing consequential physical behavior. This is a compact implementation guide, not a new physics authority or a required whole-game audit. Native C++17/Jolt at 90 Hz remains the owner; Godot consumes snapshots and supplies input. The complete phased scope remains unchanged.

## Start with a prediction, then reuse the owner

For the current encounter, write six short statements in its existing work record:

1. **Outcome:** the actual approach, operation, receiving footing and recovery the player must achieve.
2. **Cause:** initial configuration, masses/inertia, geometry, materials and ordered inputs; what supplies motion and how much work is available.
3. **Reaction:** the bodies and contacts carrying each load, including the player; identify static world reactions explicitly.
4. **Storage/loss:** gravitational, kinetic and elastic energy; physical friction, material loss or impacts. Distinguish actuator command bounds from measured work and solver residual.
5. **Operating range:** justified load, approach speed, timing and geometry variation. State units and provenance of parameters. Do not call a single passing point a range.
6. **Failure/exit:** what physically stops, slips, breaks, misses or falls; how the player recovers and what state remains afterward.

Use an estimate sufficient to reject impossible force/travel/work before coding. It need not become a large simulation report. Then implement the smallest connected encounter using the existing owners below. New parts remain allowed; this is not a parts whitelist.

## Existing owners and boundaries

| Need | Authoritative implementation | Reuse contract and limit |
|---|---|---|
| Bodies, mass/inertia, hinges, guides, ropes and physical catches | `src/sim/mechanism_kit.{hpp,cpp}` | Use actual body/constraint identities and visible collision geometry. `point_inverse_mass` accounts for the implemented fixed-joint cluster. `Material` is a presentation palette, not an automatic physical material law. Explicitly author friction/mass. |
| Grip, loaded suspension, powered regrips and release | `src/sim/physical_hand_climb.{hpp,cpp}` | Reuse independent finite hands and their actual support reaction. Command work bounds and spring-target supplied/absorbed work are separate readbacks, not complete energy closure. Do not add a second rider-weight force or reset departure velocity. |
| Finite push-off on light movable supports | `src/sim/physical_foot_push.{hpp,cpp}` | `begin` retains an actual material point; the existing row limits force, stroke, duration and command budget. Losing face, traction or reach terminates the push. Respect owner-thread creation/destruction and collision-step readback. Its nominal spring diagnostic is not a full solver-energy ledger. |
| Flexible cloth, material anchors, clips and reciprocal loading | `src/sim/cargo_net.{hpp,cpp}` and pinned Jolt patch | Use `CargoNetConfig` and actual material coordinates. Clip stiffness is N/m, clip damping is N·s/m; axial damping ratio is dimensionless. Do not interchange body drag, edge strain loss and clip damping as equivalent tuning knobs. Catch-net settling/cost remain unresolved until demonstrated. |
| Elastic/breakable timber | `src/sim/deformable_plank.{hpp,cpp}` | Reuse segment bodies, joint ownership, strain/strength and fracture state. Current timber values are explicit reduced-order authoring assumptions; they are not certified wood properties or a universal beam preset. Preserve actual broken-body collision and load release. |
| Ground support, takeoff, landing and recovery | `src/sim/simulation.cpp`: `support_point_velocity`, `record_landing`, `apply_landing_recovery` | Use actual contact-point velocity, including rotation, and relative impact. Preserve support momentum, physical contact requirements and the 20 m/s lethal-impact contract. Existing recovery is not proof of a physical roll. |

Solver iterations and collision-step count are numerical settings, not material properties or free fixes for missing dissipation. Keep the existing 90 Hz integration contract; justify any numerical change with a specific solver defect and its cost.

Headers carry lifecycle, lock and measurement contracts. Read the selected owner's implementation and a relevant existing caller; do not copy an entire encounter or create a parallel controller. Native force duration and collision-step impulse are not interchangeable with render frames or outer-tick totals.

## Decide what to change

**Encounter engineering:** observed interaction obeys its contract, but the machine is too heavy, its ratio/travel is wrong, the receiver is unreachable, or a legitimate impact exceeds capacity. Change the justified geometry, material, mass, ratio or player decision. Predict the result before adjusting it.

**Foundation defect:** contact pulls when it should only push; passive release deletes velocity; reactions are missing/duplicated; internal losses create energy; or the same interaction fails across otherwise valid encounters. Capture the smallest actual native failure and correct the existing shared owner. Add a regression that would reject that defect on the old implementation. Reuse the correction rather than adding encounter-specific compensation.

Before extending a stubborn encounter investigation, compare two or three concrete physical alternatives: altered load path, distributed attachment, passive damper/tether, or counterweight. Predict each force/reaction/energy path and player consequence. Choose the simplest objective-preserving design; use a bounded independent mechanism review when uncertainty warrants it. A proven shared solver defect still belongs at its owner.

After a hypothesis is rejected, write the result and remaining uncertainty. Choose one observation that distinguishes the remaining causes before another edit. Do not continue searching damping, gains, iterations or geometry merely to get a pass. Do not assume that fixing one measured primitive also fixes every encounter symptom.

## Economical execution for GPT-6.1 Sol and other contributors

- Read the live checkpoint, this guide and only the selected owner/evidence. Do not replay historical investigations or load the full skill catalog.
- Root makes the physical/integration decision. Delegate independent ready work with one writer per file, concise evidence and a concrete acceptance behavior. Use the configured model and actual task-appropriate effort; escalate only for unresolved consequential uncertainty.
- Implement one correction or connected encounter at a time. Reuse valid evidence; invalidate it only when its source, configuration or relevant dependency changed.
- Prove the changed primitive and its affected gameplay connection, then return to construction. Expand checks only for a new risk or observed failure. Never soften acceptance to hide a failure.
- Finish with a small reusable record: owner/API; guarantee; measured operating cases; limits; regression command; affected gameplay outcome. Keep raw logs outside the startup path.
- Distinguish source review, native execution, rendered input, GREEN Android build and phone observations. A primitive test is not an APK or an in-game exit.

## Current bounded foundation repair

Pinned Jolt 5.6 soft-contact response can apply a tensile normal impulse to an overlapping vertex already separating from a static or dynamic receiver. The correction belongs in `cmake/patches/jolt-passive-soft-attachments.patch`, protected by its source-hash manifest and the existing `scraperx_sim.catch_net_contact_friction` regression group.

Reject the complete tensile normal response, including restitution history; do not simply reject every positive current normal speed, because a valid repulsive bounce may still be owed. Preserve geometric depenetration, legitimate closing response and reciprocal dynamic reactions. This correction does not by itself establish full energy closure, kinematic-contact correctness, catch-net settling, mobile performance or GREEN delivery. The live checkpoint and [scoped correction evidence](../evidence/shared-soft-contact-2026-10-13/README.md) own its current verification status.

## Reusable membrane contact and passive tether

`CargoNetSurface::ContinuousMembrane` uses actual indexed native faces and three unique barycentric masses for position, velocity, impulse, inverse mass and continuous force. Default cargo remains openweave. Jolt discrete soft/rigid contact samples vertices; visible filled triangles alone do not establish persistent collision coverage. Choose resolution from the smallest interacting contact geometry and require loaded/cell-offset evidence. The catch's.3m pitch follows the.35m capsule/.035m vertex and.55 support-normal bound; it is not a universal cloth setting.

An opt-in `CargoNet` tether reuses the native iteration owner: actual material-point weights → finite unilateral spring/dashpot → reciprocal actual frame-anchor impulse/torque. Anchor config is body-origin-local, converted to actualCOM-local; force uses real point inertia. Parameters have unitsN/m, Ns/m andm. Slack carries no force. Native render endpoints drive the cord casing. Capture/restore clears transient tether diagnostics without inventing external motion.

Ground tether is encounter engineering, not proof of a missing solver loss: dense36m caught/settled/exited before it, while45m decayed too slowly for its20s gate. Current600N/m,300Ns/m,2.5m rest candidate passed the unchanged45m gate and native off-centre unequalmass slack/taut force/torque/energy regression. Radial predictor loss includes implicit numerical loss and does not prove complete scene energy closure. Remaining actual input/render/Android evidence lives in the [checkpoint](../CONTINUE_HERE_CHATGPT_CODEX.md) and [scoped results](../evidence/canvas-contact-resolution-2026-10-13/README.md).
