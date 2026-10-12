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


## Shared passive arcs (native and headless touch verified)

Neutral stick with appropriate actual two-hand airborne attachment selects passive
radial hands through the same owner. Entry uses overhead reach geometry; active
arcs retain attachment across changing world-Y projection while actual reach,
footing and competing ownership remain authoritative. Explicit movement returns
to the existing finite climb/shimmy/manual machine-lean actuator. Neutral uses
the actual traversal-axis deadzones; mounted-bar lean retains every nonzero
world-X command from its existing actuator. The two modes
must never act together. Crouch shortens the captured radial rest spans through
the existing0.25m allowance; EXTEND restores them. Neutral spans remain constant.

The native tension-only spring has5000N/m stiffness,1500N/hand cap and actual
relative grip-point damping. Each collision substep applies equal opposite
impulses at player COM and actual support grip; moving rigid supports receive
angular reaction, and soft grips use the real material adapter. A scalar implicit
radial solve uses actual effective point inverse mass for stability with lighter
supports. Saturated elastic potential is quadratic below0.30m extension and linear
above it. Positive representable rest-span work is metered per hand within500W
aggregate, with0.6m/s stroke; absorbed work supplies no credit.

Mode handoffs preserve actual positions, velocities and attachment anchors while
explicitly discarding old controller preload. Return creates fresh zero-error
motor constraints; no stale lambda, work refund or renewed posture allowance.
This chosen dissipative transition does not claim force continuity or articulated
limb inertia. Native readback distinguishes radial actual/rest spans from literal
world-Y projection. The current bounded native goal requires an unobstructed
fixed-grip passive arc, moving support recoil, mode handoffs, paid shortening,
momentum-bearing release and actual ordinary climbing resume. The next bounded
goal requires the complete actual touch encounter. Both must pass before this
integration receives completion credit.

The contact-free native coupling fixtures pass their focused force/work/arc
checks. Actual ordinary climbing, static/moving ledges, momentum-bearing release,
the complete touch encounter and affected regressions also pass.
[Source-bound evidence](../evidence/shared-passive-hanging-2026-10-11/README.md)
records their separate proof boundaries. The dynamic support fixture enables gyroscopic integration so an
anisotropic rotating box can independently test integrated angular conservation;
the actual hand impulse pair also has its own instantaneous momentum oracle.
This fixture setting changes no shipping body. A framed gameplay catch is a
separate proof of acquisition, posture and ownership; its late lateral movement
does not establish a contact-free passive arc.

Physics-tuning skill is applied at the native fixed-step owner: preserve90Hz and
existing collision substeps, reciprocal loads and interpolated presentation;
change damping/integration only against measured rejecting cases. No new phone
feel, frame-rate convergence or full system energy closure is claimed yet.


## Reachable thin-edge discovery (native and affected touch verified)

Airborne ledge discovery sweeps continuously through the existing0.45–1.35m
COM-relative hand-height band. A swept contact only seeds validation: actual
side/top/receiver and both lips must belong to the same physical part, each
grip must be within1.75m reach, hand approaches must be unobstructed, and the
existing hold capsule must be clear. Acquisition uses existing finite hands;
actual pose, velocity, gravity and support reactions remain authoritative.
Mantle/vault continue to use their original query and validation flags.

[Source-bound thin-edge evidence](../evidence/shared-thin-edge-catch-2026-10-11/README.md)
contains a real missed-shelf RED baseline and passing catch/load/Drop plus three
refusal/fall cases. Existing static/moving ledges, the simulation suite and the
full exact-bridge headless touch encounter also pass. Discovery currently uses
a narrow centreline sweep, rejects initial overlaps and caps expensive candidate
validation at16. This bounded repair does not establish universal shape/mass
catchability; rising-jump ledge acquisition remains the next bounded goal.


## Ordinary rising-jump ledge acquisition (verified)

A reachable ledge can acquire real hands while the player is still rising.
Acquisition no longer waits for vertical speed to fall below0.2m/s: original
reach/clearance/approach and ownership conditions remain, and existing finite
hands absorb the catch without resetting body pose or velocity. Ordinary Jump
on actual ground reaches ledge7 with first catch at35ticks and incoming/outgoing
upward speed1.794/1.60065m/s; the old source delayed to50ticks/.159/.060.
Actual paired lips remain within1.75m reach and native force/work limits.

[Source-bound evidence](../evidence/shared-rising-ledge-catch-2026-10-11/README.md)
includes the rejecting baseline, thin-edge/ledge checks, exact-bridge headless
touch, an exposed carry-handoff regression and its maintained full-suite repair.
The carry test driver holds one normal facing input per leg so braking does not
continually turn the physical load. Its actual receiving support/contact/held
load, settled position/speed and12s budget remain unchanged. Shipping carry
physics still responds to deliberate heading changes. General mass/shape
acquisition policy and phone feel remain open; no fullGREEN APK claim is made.


## Lightweight dynamic rigid grips (verified bounded integration)

Rigid grip discovery uses actual hold geometry and unobstructed approach rather
than a400kg mass threshold or encounter identity exceptions. Real mass, inertia,
joints and contacts govern the support response. Each actual rigid hand,
including a paired fallback, must have its own clear approach from the actual
player COM plus hand offset to the target body's leaf shape. Sensors/player are
ignored; real soft bodies and carried objects still occlude rigid approaches.
The existing paired controller rejects incomplete initial hand pairs.

[Source-bound evidence](../evidence/shared-dynamic-grip-2026-10-11/README.md)
records old-source missed40kg hinged-arm acquisition, obstructed-fallback RED,
actual paired loading/support response/Drop GREEN, maintained simulation and
exact-bridge headless touch passes. Generalized one-hand control, material-net
approach visibility and universal shape eligibility remain open. The existing
plank passive hang fails identically on saved and new source; repairing that
coupling is the next bounded goal, with no plank completion claim here.
