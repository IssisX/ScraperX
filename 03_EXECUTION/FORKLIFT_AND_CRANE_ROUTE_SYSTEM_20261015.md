# Forklift and crane route system — authorized implementation design

Status: native mechanics and actual public station/lift/energy/leaving flow PASS; Godot controls/view implemented. Cargo pickup/stack/withdraw/player-ascent, matching native bridge/UI and APK remain pending. Public source-bound evidence: `evidence/forklift-2026-10-15/world-window.json`, trial2. This is not a complete route PASS. Same-truck crane transfer and later attachments remain pending. Detailed next action is in the live checkpoint. Powered actuation is expressly authorized; use honest finite native Jolt owners.

## First complete player interaction

Use the new warehouse bay west of the west-apron practice area, without moving accepted routes. Truck authored chassis2800kg, mast220kg, carriage40kg, forks60kg; geometry-derived COM/inertia and real wheel contact. Two600kg hollow forkable crates provide actual entry tunnels below a.22m plate ceiling. Pick, carry, place and stack them into real upward footholds toward a5.9m receiving gallery. Withdraw the forks, leave the real rear operator platform and climb onto the actually supported cargo toward the gallery. Ground perimeter provides retry. This is the current authored route proposal, **not a demonstrated public completion**.

The separate600kg,6m beam on dunnage remains a useful handling prop. The earlier beam-only gap proposal could be cleared by sprint-jump and did not establish a necessary machine interaction; it is superseded as the first acceptance route. No placement flag opens a route. Missed/off-center cargo can slip/tip/fall; no cargo parenting or universal weld. Actual pickup, stacking, withdrawal and player onward footing are still required before calling the interaction complete.

## Native ownership and mechanics

`mechanism_kit.hpp/.cpp` already provides compound Part geometry/mass, shape-derived COM/inertia, dynamic BodyIDs/CCD/pair filtering, checkpoints and mesh receipts. Reuse it for chassis/counterweight, mast, carriage, forks, cargo, dunnage and receivers. Existing cart/slab and split-inspection constructions are patterns, not bodies to repurpose.

New `WarehouseForklift` module owns one pinned Jolt VehicleConstraint, mast tilt hinge, carriage lift slider, and modular attachment coupler. New yard-builder module owns encounter construction. Keep this outside giant Simulation.cpp. World construction/lifetime in `simulation/world.cpp`; pre→Jolt→post seams in `tick.cpp`; snapshots/ordered public input/bridge/Godot controls at existing owners. Register/remove the vehicle step listener independently of the conditional launcher listener. Teardown before Kit destruction.

Pinned Jolt e77f1755 provides VehicleConstraint, WheeledVehicleController, wheel cast-cylinder contact tester and actual wheel transform/contact/lambda readbacks, already in the Jolt library. Vehicle sample source exists; sample target OFF, no sample runtime success claimed. Use the real cylinder contact tester, not sample fake-wheel/rubble workarounds. Set vehicle maximum pitch/roll angle to pi so no artificial anti-tip row rescues the truck.

Draft truck: three-wheel layout, two driven front wheels and a central steering rear wheel. Its real contacts form a stability triangle. Chassis/material2800kg plus mast220kg/carriage+attachment100kg are CHOSEN hypotheses, not OEM numbers. Counterweight/battery mass is physically authored in rear compound parts; derive COM/inertia rather than prescribe a hidden low COM. Rate-limited wheel orientation is a reduced steering model, not a claimed fully articulated steering linkage. Wheel poses come from native wheel receipts.

Drive torque/tire friction/braking torque/steering rate are bounded. Lift draft stroke2.4m; tilt draft−8° forward/+12° back. Force/speed/power/torque limits and a finite battery/work allowance must be derived and exercised under load. Use ServiceLift's reserve-before-solve/observe-work-after pattern. Vehicle engine minimum RPM must not regenerate energy through an idle floor; distinguish conservative energy debit from measured drivetrain work. Braking dissipates; zero battery removes powered commands but preserves passive joints/braking.

Player walks onto a real stand-up rear platform and reaches the control station. Actual support+reach gates control authority; no seat teleport, body parenting or second player controller. Leaving/pausing neutralizes demand and applies finite braking. Existing support-relative locomotion carries the dynamic capsule.

## Modular tools

Fork frame is a separate physical attachment with sequenced BodyID/local mating frames/generation. Couple only at aligned close/slow/unobstructed supported hardware; create the joint at current pose without snapping. Detach preserves velocities and rejects unsafe suspended-load removal. Cargo is not part of this attachment joint. Later hanging claw requires real suspension/gripper mechanism; clamp requires closing jaws, finite force and contact/friction/slip.

## Truck transfer between heights

Owner explicitly authorizes a large working yard crane. Provide truck lifting eyes, real sling/hook joints and a mass-aware rig; preserve the same truck BodyIDs, battery, tool and payload. Powered hoist/slew/trolley use finite forces/torques/speeds/power and reciprocal mast/base/counterweight reactions. Respect the load moment at actual radius, free load swing, cable clearance and actual receiving-bay footing. Release rigging only from a supported truck; no cosmetic truck copy, elevator teleport or static parenting.

First connected freight handoff is ground→11/22m receiving bay. Higher freight stages need genuine crane reach/structure/bay planning; one yard crane cannot be assumed to serve the whole1600m tower. Design/load envelope and actual native proof remain pending.

Primary-source references for sizing, not copied game constants: [Toyota core-electric specifications](https://www.toyotaforklift.com/content/dam/tmh/marketing/en/pdf/product-spec-brochures/2024_Core%20Electric_Spec%20Sheet_Digital.pdf), [Liebherr crane finder distinguishing maximum capacity and capacity at maximum radius](https://www.liebherr.com/en-us/tower-cranes/crane-finder-8471546). Owner's5000–12000lb range converts to approximately2268–5443kg; exact authored truck/rigging mass governs the simulation.

## Required falsifiers and delivery

One affected acceptance set must reject no-contact lifting, airborne traction, unlimited lift/stall power, hidden upright rescue, unsafe off-center/high-load cornering, false receiver completion, snapping couplers and repeated-reset body/constraint duplication. Demonstrate actual dynamic crate support after fork withdrawal, player cargo contact and onward gallery footing. Retry restores Kit state plus vehicle spin/transmission/suspension, owner battery/coupler, joint warm starts and render history; Kit transforms alone are insufficient.

Native/public/bridge input checks and exact-source APK build are distinct from device operation. No placeholder vehicle, shell animation or delivery claim based on the design.

Further authored scenarios: angle a long beam through a tight rack; stack offset blocks into a scramble; rotate a pipe into real saddle bearings with a suspended tool; clamp/pull a jammed container to expose an inspection path; move actual ballast to alter another machine's behavior. Build the first connected path before multiplying attachments/scenarios.
