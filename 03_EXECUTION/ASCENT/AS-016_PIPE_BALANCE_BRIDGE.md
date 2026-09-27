# AS-016 — Pipe-loaded balance bridge

**Ascent Slice:** `AS-016`
**Lifecycle:** DELIVERED — native default and presentation integrated; exact-commit desktop proof and Android artifact verified. Device execution remains unproven.
**Provenance:** design probes began at `360cffb`; implementation continues from `e8d9ca8`
**Implementation gate:** satisfied. Loaded releases, pipe capture, arrest and supported crossing are integrated in normal gameplay and covered by the delivered exact-source workflow.
**Evidence:** current delivery status remains in `00_START_HERE.md`; [integrated observations](../PLANNING/AS-016_PROTOTYPES/integrated/README.md) distinguish final production tests from the earlier copied prototype

## Preserved prototype checkpoint — 2026-09-26

[Local source and observations](../PLANNING/AS-016_PROTOTYPES/README.md) now cover
the separate swing/arrest, static native walking route and loaded releases.
The selected upper cheek offset is 0.25 m. The independent swing evaluator
passes all 27 cases against 54 native runs and rejects incomplete evidence.
The final production assembly now closes actual moving-pan delivery, physical loaded release, arrest and native player crossing. The earlier isolated files remain design provenance.

[Exact delivery evidence](AS-016_DELIVERY_EVIDENCE.md): gameplay `254eb28`, successful run `36269138275`, verified ARM64 APK.

## Production observations — 2026-09-26

Profile: **MACRO-TRAVERSAL-STRICT**. Native source is the only physics owner; Godot reads its body/part state and forwards input. The ordinary constructor builds the pipe bridge, without a candidate flag.

- INTEGRATED native route: actual player walks from grade, grabs rack grip 2530, delivers 20 independent pipes, grabs bridge grip 2505, crosses beam 2500 and the +8 m receiver, and arrives supported on the original +11 m tower.
- Native controls cover idle, rack-only and empty-pan release; partial pull/regrab; boarding while the beam rises and being passively carried; and a real fatal fall restoring the deployed bridge, retained pipes and spent crusher.
- Actual viewport inputs cover gamepad/keyboard at 60 FPS and touch at 30 FPS. Separate first-person rendered captures show the rack, loaded pan, raised route and tower arrival.
- Kit adaptations from the inactive ballast archive: handle-only reach, grip classification before normal-world return, principal-inertia energy readback and guide peak-speed persistence. Actual-contact control grips and active material checkpoints remain authoritative. No alternate ballast assembly is instantiated.
- The material boundary follows plastic compression; elastic indentation is at most 6 mm before yield. Other-body contacts with this boundary do not add to the pan-only constitutive law. The sloped pan can meet the ultimate floor near 0.58 m compression, so the nominal 0.65 m parameter is not a claim of that much unobstructed travel.
- Human recorded fall reactions trigger only during dangerous falls, with bounded variation, escalation, wind ducking, and cancellation on recovery/pause/death. Sources and CC0 provenance accompany the assets.

No APK installation, audible Android test or sustained Fold performance is established by desktop evidence.

## Objective

From ordinary grade play, release a rack of twenty heavy pipes into a balance pan, release the loaded beam, and use the resulting inclined bridge to reach a fixed +8 m landing and its supported connection to the existing +11 m deck.

## Authority and state owner

The Atlas §6 owns the layout and dimensions. The macro authoring contract owns physical closure and evidence requirements. Governing Laws 12–17, 22–26 and 29 apply to causality, authoritative state and completion claims. Native Jolt bodies, contacts and constraints own the mechanism; a finite material law owns the crush receiver's deformation and energy loss. Godot presents that state and forwards reachable player input.

## Required causal path

1. The player physically moves the rack restraint through a reachable control.
2. Gravity rolls the separate hollow-pipe bodies down four contained lanes into the pan.
3. A separate visible prop holds the beam during loading. The player releases it through a finite-force linkage.
4. Captured pipe weight rotates the beam. Two large external arms form a parallelogram that keeps the pan level.
5. The descending pan compresses a visible receiver. The receiver dissipates finite work and supports the residual load.
6. The player enters the bridge from the side at grade, climbs it, exits sideways onto the receiver, and continues onto the +11 m tower deck.

The two releases do not depend on an inventory counter, elapsed time or a success flag. Early release, missing pipes, boarding during motion and physical interference retain their actual consequences.

## Backward design findings

- A landing placed directly beyond the rising tip intersects its sweep. Use an adjacent side receiver with an inclined entry cheek.
- Ballast keeps applying raising torque at the upper pose. An underside seat beneath the long arm cannot oppose that torque. Support beneath the descending pan has the required reaction direction.
- The initial 4 t deck leaves excessive kinetic energy. The current prototype uses an 8.5 t deck, a 1 t pan and a 1 t combined arm budget; these remain chosen design masses.
- Raise the short-arm attachment on a diagonal arm to keep the pan above grade. A horizontal low arm with a vertical bracket would still intersect grade.
- Route the linkage arms outside the pan. A centreline arm intersects the pipe-loading volume.
- A single pan hinge does not enforce a level pan. The prototype uses a real four-bar linkage.
- Model the deck's centre below its walking surface when deriving work and inertia. Do not confuse the walking datum with the body centre.

## Prototype closure record

The sequence below records how the delivered slice was closed. It is historical design rationale, not an open implementation checklist. Current source and delivery evidence are owned by `00_START_HERE.md` §2 and the linked evidence files above.

### 1. Receiver and route geometry

The final sweep and player corridors were resolved against source geometry; the receiver accepts a range of stopping angles. The native player route and supporting geometry are recorded in the integrated observations linked above.

### 2. Loaded swing and arrest

The four-bar and loaded swing were evaluated in isolation and then in the integrated native assembly at production and refined timesteps. The integrated observations report drift, pan tilt, work, stroke, rebound, support and post-arrival state.

The crush material must track consumed stroke and elastic energy. It cannot regain plastic travel after rebound, reload or checkpoint restore. Choose a visible force-bearing volume and preserve a real ultimate support below it. Do not remove kinetic energy by setting velocity to zero.

### 3. Physical releases and pipe loading

The accepted route uses reachable physical restraints and twenty separate hollow pipe bodies. Native cases cover release states, loading, early boarding, passive ride and restoration; see the integrated evidence for measured bounds.

### 4. Integrated mechanism and player crossing

The final normal-input sequence starts from grade and reaches the supported +11 m deck. Rendered captures use the same native mechanism. The stable receiver and onward route are the completion condition met by this delivered slice.

## Implemented source ownership

- The dedicated pipe-bridge native module owns construction, material state and telemetry.
- Kit owns bodies, hinges, mass properties and contacts; no alternate ballast assembly is constructed.
- `simulation.*` builds the mechanism in the normal world, forwards control input and captures/restores its state.
- `scraperx_simulation.*` exports observed state and geometry to Godot.
- `main.gd` draws native geometry, labels reachable controls and presents mechanism feedback.
- Native tests, the Godot input scenario and the delivery workflow exercise and package this sequence; exact current delivery evidence is linked above.

This pipe bridge is the active normal-scene mechanism. Keep isolated engineering fixtures and the overlapping ballast alternative out of normal play.

## Recovery and persistence

Spent pipes and crushed material remain physical state. A successful crossing leaves a reusable route. A spill or incomplete lift must leave an accessible return or alternative route. Checkpoint restore includes body poses and velocities, control/link states, pipe inventory and the crush material's plastic front and stored energy. Resetting a local progress flag cannot rearm it. Any physical rearm needs its own source of work; none is claimed by this ticket yet.

## Accepted criteria

These were the delivery conditions for AS-016 and are recorded as passed in the exact-source evidence above. They are not pending work for this slice.

- Ordinary input completes rack release, pan loading, beam release, bridge crossing, fixed receiver arrival and onward exit.
- Removing the source or a necessary load path prevents the claimed ascent through a physical cause.
- Placement, contact resistance, timing and early boarding variations have explicit bounded results.
- Consequential rendered surfaces agree with their native geometry and material state.
- The opening motion is visible and useful within the Atlas pacing target.
- Failure and checkpoint continuation preserve the complete physical state.
- The exact code candidate passes the required workflow and produces its Android artifact. Installation and device observation remain separately recorded facts.

This ticket does not authorize constructing the later Colossus chain.
