# Forklift operator compartment

Owner priority: finish the forklift first, then the crane. The truck is a real stand-on three-wheel counterbalance design. The85kg dynamic player stands on its native rear floor; entry and exit use actual contacts. There is no seated pose or camera relocation.

The overhead guard, dashboard, steering column, lifting eyes and side treads are real native chassis geometry. Every added steel wall/plate contributes its volume-derived mass and inertia through Kit/Jolt. Current chassis3040.956477kg; complete truck3372.956477kg. Cargo remains separate dynamic material resting on actual fork blades.

| Control | Keyboard/mouse | Gamepad | Touch |
| --- | --- | --- | --- |
| Accelerator | W | RT | Stick up |
| Service brake | S or Space | LT | Stick down |
| Rear-wheel steering | A/D | Left-stick X | Stick X |
| Forward / neutral / reverse | F / N / R | RB / Y / LB | F / N / R buttons |
| Parking | P | Left-stick click | PARK |
| Lift up/down | Up/down arrows | D-pad up/down | LIFT+/− |
| Tilt back/forward | Left/right arrows | D-pad left/right | TILT BACK/FWD |
| Fine inching | Hold Shift | Hold right-stick click | Hold INCH |
| Horn | Hold H | Hold A | Hold HORN |
| Look around | Mouse | Right stick | Right-thumb drag |
| Leave controls | E or Q | X or B | DONE |

Entry starts neutral and parked. Direction selection requires released accelerator; an opposite-direction selection waits for low actual native speed. Accepted F/R selection releases parking. Accelerator release coasts; the service brake remains independent. Parking, lost native footing, leaving controls, pause/focus loss and energy cutoff neutralize demand and request the finite native brake. Pause resumes neutral and parked. Leaving controls does not move the player out of the compartment; ordinary walking exits the open side.

The camera uses the actual native rider eye. Native chassis heading carries the view through turns while preserving free head yaw/pitch. Native wheel steering and hydraulic target receipts drive the visible wheel/levers. Arms use those rendered world anchors and the cab frame, including elbow poles during look-back. The radio remote stays hidden in forklift mode.

Four cached spatial sound voices follow actual drive motion, hydraulic commands/load/work, reverse demand/motion and the held horn. They reset on pause/unavailability. The source/PCM/player tests used Dummy audio; heard sound and phone feel remain device acceptance.

The finite actuator/traction laws remain authoritative. No cargo parenting, adhesion, upright lock, artificial balance correction, scripted tipping or exit-velocity assignment is added. Loaded stability is demonstrated with the same1700kg steel block at two material centers: near lifts with rear support; farther out unloads the rear wheel, pitches the truck, sheds onto actual ground and recovers rear support. This is a pair of operating cases, not a rated-capacity certification or a full handling envelope.

The old full-jump boarding fixture legitimately caught the new guard. Real side treads plus the existing finite reciprocal leg-step owner permit ordinary walking entry. Both-side entry/90tickhold/gradeexit acceptance requires zero legacy step pose writes. `BOARDING_STEP.md`, `LOAD_STABILITY.md`, the persistent mechanic windows and final operator evidence retain exact sources and failures.

Current source-bound native and Godot receipts live in `/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015`. Final connected cargo trial14 passed ordinary boarding, both600kg pickups, stacking, withdrawal, actual player gallery support, onward walk and grade retry. The actual Main/native rendered run passed36 checks; root inspected all five forward/fork-view/touch/hydraulic/rearward captures. Final Q-exit and pad-inch checks are recorded separately because those input-only changes followed that render. GitHub publication and matching APK follow the accepted selected source; phone feel remains unobserved.

Tuning remains owned by `WarehouseForklift::Config`: finite drive torque/gearing, steering angle/rate, wheel traction/brake, lift speed/stroke/force, tilt speed/limits/torque, battery and hydraulic power. UI inching changes bounded requested drive/brake only. The coordinator does not scale force/stiffness laws or assign vehicle/cargo poses.
