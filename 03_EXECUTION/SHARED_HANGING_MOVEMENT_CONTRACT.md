# Shared hanging movement — module3

Owner intent: expert parkour fluency, extraordinary climbing, precise footwork,
confident catches and smooth transitions that preserve momentum. Every appropriate
hanging attachment must use the shared native mechanics. The physical state and
ordered player inputs determine the outcome. The hanging bar is an encounter proof,
not the only permitted support. Hook-launcher and vehicle ideas are future work
after the movement system.

## First bounded integration: deliberate posture

Native `PhysicalHandClimb` owns the real attachments, forces and actuator targets.
`Simulation::PhysicsWorld` combines posture with its existing climb/hang/swing command.
Godot delivers ordinary Crouch intent and shows TUCK/EXTEND from native eligibility
and request readback. Jump, Action, Drop and Back retain their contextual meaning;
native attachment ownership ending clears a posture-origin toggle while preserving
held keyboard intent and ordinary ground crouch. No presentation state supplies physical truth.

Eligibility requires two actual compatible hands, airborne Climbing or Hanging,
overhead grip geometry and human reach. Ground footing, lowering, transfer, push
stroke and slingshot ownership exclude it. There is no encounter-ID whitelist.
Rigid and material-coupled soft hands retain their existing reaction paths.

| Quantity | Contract / evidence class |
| --- | --- |
| Player mass, tick, gravity | Existing native85kg,90Hz,9.81m/s²; retained source parameters |
| Posture | CHOSEN0.25m world-up COM rest-target translation, explicit Crouch input |
| Eligibility | CHOSEN overhead separation>0.35m with delivered-offset compensation; existing reach≤1.75m |
| Force | Existing≤1500N resultant per hand |
| Active posture stroke | CHOSEN≤0.6m/s combined movement/posture target change |
| Active posture work | CHOSEN≤500W aggregate positive spring-rest-target work across both hands |

Positive and absorbed target work are separate receipts. Absorbed work cannot fund
another hand. Representable actuator displacement tracks the delivered posture;
held input must not repeatedly add0.25m to a moving climb baseline. Regrips preserve
spring extension; ownership changes clear posture bookkeeping. Untucked ordinary
climb and transfer retain their existing limits. The dynamic body, gravity, standing
capsule, all contacts and support reactions remain authoritative.

Native Snapshot and `get_landing_state` expose `swing_tuck_available`,
`swing_tuck_requested`, `swing_body_below_grip_m` and
`swing_commanded_body_below_grip_m`. The distances use actual grip points and actual
COM/bounded actuator target, respectively; all four clear outside eligibility.

## Rejecting acceptance and limits

Paired identical states must show>0.10m actual grip-relative COM response and a
0.25±0.01m commanded change, with held targets stable within0.01m. Manual pulses at
different times must change actual bar motion at a common final native tick. Static
and moving ordinary ledges must exercise the same mechanic, preserve mantle and
momentum-bearing Drop, and reject no-hand availability. One complete ordinary touch
encounter must use deliberate tuck/extension, timed release, actual receiver/onward
footing and existing feasible recovery/retry. `parkour_tuck` is registered as
`scraperx_sim.shared_hanging_posture`.

First bounded posture integration is verified by native and Godot headless touch
execution. [Source-bound evidence](../evidence/shared-hanging-posture-2026-10-11/README.md)
records actual0.25m response, static/moving ledges, ordinary incremental Climbing
with real regrips and restored ascent, manual timing, full receiver/recovery/retry
and onward footing. Baseline behavioral RED measured0m rise with Crouch held
against a same-tick neutral control. When extension causes a real regrip, measure
COM/target displacement relative to stationary support rather than assuming the
hand midpoint remains fixed.

This first model translates COM through finite hands; it does not simulate articulated
limbs or passive free arcs around arbitrary grips. Existing named machine tangential
lean/pumping predates this work. General passive hanging must free tangential motion
through the same hand owner, retain reciprocal finite loads, meter intentional
radius/posture work and preserve real climb/regrip arbitration. Existing catch mass
filters also need condition-based treatment; removing a posture whitelist does not
prove universal catches. Full system energy closure, phone feel and a new fullGREEN
exact-source APK remain unverified.
