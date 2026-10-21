# Physical capture implementation handoff — 2026-10-21

Side branch: `dot/matron-physical-capture-20261021`, based on remote
`ScraperX-Claude` at `82cb589`. No merge into Claude. Claude advanced independently
to `b9f7d14` during implementation; that work was not overwritten or rebased.
Water and paused load-bearing implementation are untouched.

## Delivered

Changed-stage encounters now require real player/Matron Jolt contact instead of
immediate death at hand-reach distance. The chase approaches that contact. The
same player capsule keeps its shape, position and velocity, unlocks rotation,
and attaches at its local ankle through a bounded spring/damper actuator.
A finite-speed reachable hand target lifts it; gravity produces inversion.
A ragdoll would add collider, injury and controller handoffs unnecessary for
this slice, so it was deliberately not introduced.

The native world remains authoritative at 90 Hz. During capture/recovery Jolt
uses two collision substeps to refresh the rotating capsule's contact manifold;
extra position iterations alone did not fix measured floor penetration.

- Grip and escape together are capped at **3800 N**, a tuning value, not measured
  anatomy. The Matron receives the opposite force; her existing upright capsule
  retains translation response and braces through real floor friction.
- Movement input accumulates at most 1/2.5 escape effort per second. Normal
  jump/crouch/carry/hook/traversal/machine commands cannot acquire the body.
- Release occurs on escape, overload, excessive separation, interruption, or a
  nine-second hold limit. No release pose/velocity write. No guaranteed landing.
- Recovery uses at most **650 Nm**, including damping, to return the entire frame
  (yaw included). No upright snap or discarded angular velocity. The bounded
  servo remains active after recovery instead of silently relocking rotation.
- Death and checkpoint load clear ownership before the existing reset restores
  the standing checkpoint. Existing impact/injury paths remain active.
- Godot receives the physical quaternion, effort and ankle attachment. Camera and
  first-person body follow that quaternion; the Matron's hand mirrors the ankle.
  Posture-correct camera offset is preserved. Touch hints say MOVE TO STRUGGLE.

## Verification

Build: CMake Release, GCC 14, pinned Jolt and godot-cpp; native tests and Linux
GDExtension built successfully. No new dependencies or dependency-pin changes.

Commands (build directory used here: `/workspace/matron-build`):

```
SCRAPERX_ONLY=capture ./scraperx_sim_tests
SCRAPERX_ONLY=matron ./scraperx_sim_tests
SCRAPERX_ONLY=matron-collision ./scraperx_sim_tests
SCRAPERX_ONLY=matron-balance ./scraperx_sim_tests
SCRAPERX_ONLY=bay ./scraperx_sim_tests
ctest -R 'matron_capture|parkour_flow|shared_survival' --output-on-failure
```

Capture exercises actual encounter callers for five scenarios: timeout/recovery,
standing escape with competing jump/crouch input, stage interruption plus restore
during recovery, restore during hold, and lethal stock collision during hold.
Assertions inspect the physical quaternion's inverted up vector, finite force
and torque, per-tick pose continuity, rotated capsule floor envelope (4 cm maximum
solver tolerance), successful normal movement after recovery, and cleared grip
following death/restore. Original instant-death implementation failed the new
contact test before implementation. Obsolete instant-contact-death expectations
in the old encounter tests now use explicit checkpoint restore.

Godot 4.7 official binary checked against CI SHA-256
`0b1a6c54c2c619c12e169fe9241edda4b81080b519451cec2984bf0d2c6cb73c`.
Headless checks: `capture_pose_test.gd` (quaternion inversion, continuity, crouch,
normal camera), game `--ci` boot/approach proof, and `--uitest=touch_matron_heave`.
The new camera test is also wired into the existing CI workflow.

Independent review found residual-yaw input mismatch and crouched-eye offset
mismatch; both corrected. Testing additionally caught premature regrab after a
long recovery and floor penetration; both corrected rather than relaxing the
physical requirements.

## Limits

This is an unlocked-capsule encounter, not articulated anatomical simulation.
The Matron retains her pre-existing upright body architecture. No full-suite,
Android build, device installation, Fold performance or motion-comfort claim.
Real-world tuning of grip, recovery torque and escape duration remains necessary.
The workflow normally triggers only on `ScraperX-Claude`; pushing this side
branch alone does not constitute CI or APK proof. The full capture encounter's
rendered hand/body quality still needs visual/device playtesting; headless tests
prove contracts and execution, not visual comfort.

No severing, voice generation, general climbing overhaul, injury redesign,
one-arm route redesign, or guaranteed safe landing geometry was added.

## Integrated on ScraperX-Claude (2026-10-21)

Taken as delivered (`c090a9d`) on top of the broken bridge and the rigging
reach. Two defects found against the rest of the game, fixed on top:

- **The rotation stayed free after recovery** (`a3ed75c`). Every other system
  acts off the body's centre on a body that cannot turn (a hang's arms at the
  shoulders, a carry at the hands, a harness). Once recovered (within 1.4°,
  under 0.08 rad/s, grounded) the body is set upright and its rotation locked
  again. The capture test throws a drum into the recovered body: tipped 0.024
  before, 0 now.
- **Getting up could lock the controls indefinitely** (`d976a46`). Let go with
  her leaning on the climber, the body stood 12–21° over, never within 1.4°,
  every control locked (traced in `touch_matron_act`). While getting up the
  stick now walks (and, let go, stands) the body; everything else still waits
  for upright. Dropping the soles' friction instead was tried and refused: the
  lethal-drum scenario then spun the body 12° in a tick.

`touch_matron_act` met the old rule (her hands a death): it now waits for her
grip, holds the stick until it breaks, waits for the climber to get up, then
CONTINUEs from the climb's save to the checkpoint below her deck.
