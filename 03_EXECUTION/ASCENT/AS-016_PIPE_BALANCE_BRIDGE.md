# AS-016 — Pipe-loaded balance bridge

**Ascent Slice:** `AS-016`
**Lifecycle:** PLANNED — design and isolated native probes in progress; no gameplay mechanism added
**Provenance:** re-derived against `360cffb` and the current native geometry
**Implementation gate:** loaded releases, pipe capture, arrest and the supported crossing must close in the bounded prototypes below before promotion to normal gameplay
**Evidence:** current delivery status remains in `00_START_HERE.md`; isolated prototype results must not be reported as a completed ascent

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

## Bounded prototype order

### 1. Receiver and route geometry

Check the complete bridge sweep and player approach/exit corridors against the current source collision geometry. Check the full stopping-angle band, not only the nominal +8 m tip. Produce a labelled plan/side drawing and reproducible geometry record. Then exercise the supported route with the native player before accepting it.

### 2. Loaded swing and arrest

First isolate the four-bar with a declared preloaded pan. Compare the reduced calculation with native Jolt at the production 90 Hz and a refined step. Measure joint drift, pan tilt, work, plastic stroke, rebound, final support reaction and the full post-arrival motion envelope. A preloaded pan proves only this isolated stage.

The crush material must track consumed stroke and elastic energy. It cannot regain plastic travel after rebound, reload or checkpoint restore. Choose a visible force-bearing volume and preserve a real ultimate support below it. Do not remove kinetic energy by setting velocity to zero.

### 3. Physical releases and pipe loading

Test each loaded restraint with actual contact, finite control force and travel. An angle-triggered deletion of a fixed constraint does not establish loaded release effort. Use separate pipe bodies with the declared hollow-cylinder mass and inertia. Measure every pipe's actual entry, retained state, impact losses and containment; a seeded mass or a count-based force does not establish this stage.

### 4. Integrated mechanism and player crossing

Join the proven pieces in one normal-input sequence from grade, without seeded inventory or player relocation. Render from the same native geometry and inspect the first-person release, loading, arrest and crossing. The stable destination and route onward are the completion condition.

## Source seams for implementation

- A dedicated pipe-bridge native module owns construction, material state and telemetry.
- `mechanism_kit.*` may gain the specific body-to-body hinge, mass-property or shape support this module needs. Existing primitive semantics remain explicit.
- `simulation.*` constructs the new mechanism, forwards its control inputs, and captures/restores all mechanism state.
- `scraperx_simulation.*` exports observed state and geometry to Godot.
- `main.gd` draws native geometry, labels reachable controls and presents relevant mechanism sound/feedback.
- Native tests and a dedicated Godot input scenario exercise the real sequence. The delivery workflow checks those exact proofs and packages the candidate APK.

Do not activate this mechanism in the normal scene while a critical prototype still fails. Keep isolated engineering fixtures out of normal play.

## Recovery and persistence

Spent pipes and crushed material remain physical state. A successful crossing leaves a reusable route. A spill or incomplete lift must leave an accessible return or alternative route. Checkpoint restore includes body poses and velocities, control/link states, pipe inventory and the crush material's plastic front and stored energy. Resetting a local progress flag cannot rearm it. Any physical rearm needs its own source of work; none is claimed by this ticket yet.

## Acceptance

- Ordinary input completes rack release, pan loading, beam release, bridge crossing, fixed receiver arrival and onward exit.
- Removing the source or a necessary load path prevents the claimed ascent through a physical cause.
- Placement, contact resistance, timing and early boarding variations have explicit bounded results.
- Consequential rendered surfaces agree with their native geometry and material state.
- The opening motion is visible and useful within the Atlas pacing target.
- Failure and checkpoint continuation preserve the complete physical state.
- The exact code candidate passes the required workflow and produces its Android artifact. Installation and device observation remain separately recorded facts.

This ticket does not authorize constructing the later Colossus chain.
