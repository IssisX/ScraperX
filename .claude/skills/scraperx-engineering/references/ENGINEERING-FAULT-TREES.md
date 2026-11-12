# Precomputed ScraperX causal fault trees

**Use only when relevant.** These are engineering hypotheses, ranked discriminator designs and rejection rules; **not** verified current defects. Before applying a concrete function name or threshold, read it at the current `ScraperX-Claude` SHA. The current product/architecture rules govern any conflict.

## Universal causal-path diagnosis

The practical end-to-end chain is:

`human touch/action -> Godot input router -> native request and StepCommands -> fixed native simulation / contact / world update -> authoritative state and event -> bridge snapshot -> Godot pose/effect/audio/haptics -> seen player consequence -> save/load or Android, if claimed`.

For every failure, locate the **earliest boundary whose input is correct but output is wrong**. Place timestamps/tick IDs, entity IDs and measured positions on both sides of that boundary. A later visual discrepancy can originate much earlier; a failed native test cannot prove a presentation error. The best repair is at the owner of the earliest wrong transition, not where the symptom happens to be visible.

**Hypothesis-selection rule:** keep at least two candidate causes until one measurement discriminates them. A probe is worthwhile only if its possible outputs lead to different next actions; otherwise use the already-supported choice. Prefer the one-step probe that partitions candidates most strongly, not the longest log or broadest suite.

## A. Fight contact: one event must explain six consequences

Player expectation: the fist or foot touches the **drawn body part**; the contact point and direction are physically believable; guard intercepts; reciprocal impulses and balance response arise from the same contact; injury, sound and haptic effects belong to that event. **Never solve this by adding cosmetic knockback after a capsule proximity check.**

Possible causes, in order of discrimination:

1. **False physical hit** — the native test is deciding contact from a broad upright capsule or from a prospective reach/offer rather than the limb surface actually reached; guard and anatomical subregions may be missing from contact selection.
2. **Correct contact, wrong reaction** — contact is real but impulse, torque (`r × F`), strike direction, available support, guard absorption or equal-and-opposite force fails at the physical owner.
3. **Correct physical exchange, wrong pose** — native hit tick and the rendered hand/head/bones disagree spatially or temporally; interpolation, freezes/hit-stop, body offset or bone rig owns the mismatch.
4. **Correct state and pose, wrong effects** — audio/haptics/injury trigger from an attack *count*, animation phase or anticipated hit instead of the specific committed contact event.

**Discriminator:** one controlled strike with a stable target and one with a moving/guarding target; capture at the **same tick**: real striker surface/trajectory, target subshape or skeletal hit surface, first contact point and normal, native hit flag, impulse, force application point, head/torso angular response, snapshot tick, displayed limb position, impact audio/haptic edge. Repeat with a deliberate near miss and a blocked strike. If native hit occurs before surfaces touch, fix contact authority; if native contact is correct but image is displaced, fix pose/tick synchronization; if hit/pose agree but effect occurs early, fix event gating. A guard must change which physical surface received momentum, not only play a block animation.

**Check:** real hit, near miss, guard, inverse-side hit, her attack on the player, target motion/reach, interruption/cancel, repeated strike accumulation, and save/reload if injury persisted. Only probe reached paths; do not build a universal tracing framework unless simpler instrumentation cannot discriminate. Relevant starting points: `src/sim/physics_world_strike.cpp`, `src/sim/matron.cpp` and injury paths, `godot/presentation/life/strike_feel.gd`, `matron_view.gd` / `matron_rig.gd`, native `strike` and `matron-*` tests, touch `touch_matron_act` / `touch_matron_heave` where applicable.

**Physics detail:** a contact-derived force at position `p` should yield impulse `J=∫F dt` and torque about the body's center `τ=(p-COM)×F`; check actual Jolt body/constraint semantics before interpreting rotation. Equal opposite **force** is not by itself proof of correctly placed opposite **torque**. Never invent recoil, balance, guard or injuries directly in the renderer.

## B. The Matron's knowledge: distinguish detection from physics

Her AI may use *known* information from sight/hearing/touch, time since evidence, or a physically felt hold. The game simulation may use the player's true body position to resolve collision, constraints and contact; **that is not automatically a perception leak**. The leak occurs when an *information decision* (route/heading/search/rage/target selection) uses true state without sensed evidence.

Classify each true-position read by **consumer and consequence**:

- **Collision/safety/support mechanics:** full body truth permitted for physically necessary contacts, but must not incidentally export a secret destination to her cognition.
- **Sight, hearing, touching, holding:** allowed only through an actual sense/contact event, with location, confidence and age.
- **Chase, searching, travel, facing, rage:** may use only evidence-derived lead plus memory, not current unseen coordinates.
- **Yielding to an occupied ladder:** may be an immediate physical accommodation rather than omniscience. Probe whether she can legitimately see/touch that occupancy before classifying it.

**Decisive paired counterfactual:** two runs identical up to the loss of sensory contact, then secretly move the player between two unseen positions that produce no new sound/contact. Hold the Matron's sensed-history input identical. Her information-dependent decisions and route choice must remain identical until a new valid observation; physical collisions may differ if the bodies actually interact. Third run introduces one controlled audible event: her search should change toward the *heard location*, not automatically the new true player position. Add same-input digest repetition for determinism. Read `Matron::pre_step`, `Matron::step_travel` and `Matron::step_hold` at current head before proposing edits.

## C. Matron movement/force: local success can hide invalid mechanics

Separate five rival failures: **(1) false support**, **(2) invalid affordance/route**, **(3) incapable force/power**, **(4) failed contact transfer**, **(5) stale state after a fall/landing**. A fall without a path may be legitimate; arrival on a region when airborne is not.

**Discriminator:** at the first bad tick capture real contact bodies and normals, relative vertical velocity to support, scan/region and planned edge, moving support point velocity, all per-path forces and power consumed from the common budget, body change in velocity, and step resolution. Verify generality against two materially different structures, not just the problem ladder/deck. A genuine <0.2 m near miss does not authorize an invisible catch: use the current protected geometry process for a tiny authentic world correction if warranted.

Do not substitute stronger force, copied velocity, position snap, immunity to falls, or place-name special cases. The ordinary shared budget and separately limited lunge must remain distinct. Check live limit declarations rather than copying stale numbers from notes.

## D. Determinism: distinguish ordering from numeric drift

If two runs on the *same binary* and same seed/input already differ, first check mutable/static state leakage, uninitialized values, unseeded randomness, nonstable keys and order-dependent reductions. If identical on each individual platform but diverging between host and ARM64, then inspect compiler flags, contraction/FMA, fast math, precision widths and Jolt cross-platform options. Do not blame floating-point architecture until input and event order are shown equal.

A broad-phase/capped-contact query is not ordered merely because it returns a vector. Sort **all** candidates on grounded distance/score with NaN-safe strict comparison and a stable entity/body tie-breaker, *then* take the nearest N; do not truncate the first N seen. A sort comparator is strict (`a < b`, not `a <= b`) and total enough for equal scores to avoid unspecified outcomes. Record digest at the earliest divergent tick and compare the first state value that differs, not only final positions.

Relevant starts: `src/sim/matron_traversal.hpp`, `src/sim/matron_journey.cpp`, `src/sim/simulation.cpp`, relevant deterministic native test groups, CI host/ARM64 digest jobs.

## E. Persistence: restored continuation, not just equal serialized bytes

A passing round-trip serializer can still omit an important consequence or restore an invalid contact relation. To verify an injury/machine/topology change, use **three timelines**:

- T0: checkpoint commits state A.
- T1: meaningful physics/world change B after checkpoint.
- T2: death/restore should return to A, including injuries/death as they were *at commit*, then produce a coherent next contact/simulation tick.

Also commit **after** B and verify restore returns B, not A. Compare entity identity and relevant relationships, support/moving state, wounds and reactivation behavior, not only byte length. Check whether save-layout changes are protected; do not reset the whole world to make a restore test green. Read `Simulation::PhysicsWorld::commit_checkpoint`, `restore_from_checkpoint`, `Matron::save/restore`, `Simulation::save_game/load_game`, and `save`/`matron` test groups.

## F. Native passes, rendered game fails

Likely alternatives: wrong bridge type/value, frame interpolation based on an outdated snapshot, pose rig space mismatch, stale node/resource, touch router not issuing the intended command, presentation freeze decoupled from still-running simulation, or main-thread budget/performance.

**Discriminator:** one trace with source native tick/state, bridge value, Godot read value, pose result, and actual visible location. Only after identifying which boundary diverges should it be repaired. Tests that inspect a dictionary value do not substitute for an eye-checked scene. Use live editor input/screenshot tools where available; stop the launched project and report missing visual proof if they are not available. Do not modify consequential physics in a GDScript presentation fix.

## G. Tests that drift after legitimate world changes

A scripted test may drive a fixed duration and arrive slightly short because physics changed, or the gameplay capability may truly be broken. **Distinguish them by human equivalence**: can a normal player complete the same action using the actual offer/contact and same legitimate route? If yes, adapt *only the test's approach inputs* to close the loop on sensed state, with a bounded failure. Preserve the test's asserted outcome, tolerance, time contract, destination and physically meaningful scenario; check protected classification before modifying it. If not, repair game mechanics. Consult the existing `.claude/skills/test-drift/SKILL.md`, which owns this specialized procedure; do **not** create a duplicate rule in code.

## One-line conclusion to send to the implementer

`Earliest wrong boundary: <A->B at tick>. Competing causes: <X/Y>. Probe: <action and disjoint expected results>. Owner/seam: <current function>. Do not change: <specific law/test/API>. Acceptance: <one observable player outcome and real proof>.`
