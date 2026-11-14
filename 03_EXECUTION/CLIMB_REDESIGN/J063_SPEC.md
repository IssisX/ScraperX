# J-063 — her climbing on real support

Job: J-063, issue #321. Cory's playtest, 2026-11-08: from her locker down the
ladder to the ground, she floats instead of climbing. The fix is general: the
climbing system, not one ladder. Status: spec + failing test first; the fix
follows on `fb-bot/integrate-j063`.

## What is wrong (measured, not guessed)

In `src/sim/matron_traversal.cpp`, `step_rung_up` and `step_rung_down` hang her
from one hand hold (`hold_`, plus the second hand on the same member, M5) and
drive her body up or down at `rate_mps()` (0.6 m/s) with a spring-damper. Her
feet never take a rung: the only foot hold in the Climber, `foot_`, is set by
`plant_leg`, which the rung moves never call — its callers are the dyno
(1413), gap (1900), jump (1955) and ledge-jump (2033) paths only. So on every
ladder, up or down, her feet dangle at her centre minus 1.05 m, 0.42 m out from
the ladder line, at whatever height the hand drive leaves them.

Claude's repro from source (#321, 2026-11-08 07:44), same finding:

- `step_rung_down` (1478) drives her body by force (`drive`, PD stiffness 30)
  along a target height that falls at a scripted `climb_mps`. Her grips do not
  carry her.
- The hand hold jumps to the next lower rung in one tick. `maintain_second_hold`
  (983) keeps both hands on the same rung, so there is no hand-over-hand.
- Feet are never placed: `plant_leg` and `foot_` go unused in `rung_down`.
- At the bottom she lets go of every hold and free-falls 1.45 m to grade.
- `step_rung_up` (1096) has the same pattern. The view's `rung_down` pose in
  `matron_view.gd` is a procedural stride.

The grade ladder (x=12, z=-123.6): 27 rungs at y = 1.45 + 0.36k, the lowest
1.45 m above grade (`main_world.gd:250-259`).

The view has nothing better to draw. `Matron::refresh_view` (`matron.cpp`)
probes for feet support straight down from her centre, 1.35 m at most — during
a hang that ray hits nothing, so `climb_feet_support_known` is false (or the
climb's top deck, which on a descent is above her). Her second hand is posed
geometrically (`climb_hand + left * 0.45`) and only counts as known when a
probe proves it rests on the same member the first hand holds; journey climbs
carry no support handle, so it stays unknown there too. The rig gets a real
hand, an unknown second hand, and no feet: that is the float Cory saw.

ChatGPT's research (#321, 2026-11-08 07:18) — physics-driven ladder climbing
(Jakobsen, GDC 2001; Muller et al., Position Based Dynamics, 2007): support
constraints transmit load; maintain at least three contacts during transfer;
create a fourth hold before releasing the trailing one; body support is the
sum of solved support forces, not a scripted vertical displacement. For her
220 kg, weight is ~2,158 N; two evenly loaded supports need ~1,079 N each at
rest. A no-support tick drops ~0.000606 m from rest, speed up ~0.109 m/s per
tick. At the bottom, transfer weight to actual ground contact before releasing
the final rung. Never snap the root to a marker.

## The rule (Claude, #321 07:44, verbatim)

"She descends by alternating real grips, with one hand or foot moving at a
time, each to a rung it reaches. Her weight is carried by those holds' forces
inside the budget, with no scripted height target. Her feet are on rungs, and
she steps off supported at the bottom."

Integrator ruling, #321 07:58: section 4 of Argon's trace (via Claude) is the
design — a four-limb contact state machine, at most one limb in swing, centre
of mass bounded by the attached limbs' reach, contact forces split per limb
inside the force budget, and no `target_y_` ramp. Section 5 is the proof
test. The per-limb numbers in 4.A and 4.C are Argon's proposals, not settled
values — see "Numbers" below.

On every ladder, up or down:

1. **Three points of contact.** At every tick on the rungs (phase 2), at
   least three of her four limbs hold rungs. One limb moves at a time, each
   to a rung it reaches; a fourth hold is made before the trailing one is
   released. Hands and feet alternate in a four-beat diagonal gait
   (HandL, FootR, HandR, FootL). Each hold is revalidated every tick by the
   same `revalidate()` the first hold uses, and each hand is within arm reach
   of her shoulder: shoulder 1.80 m over her feet (`Matron::kShoulderUpM`),
   arm reach 1.4 m (`Matron::kArmReachM`).
2. **Feet on rungs.** Each foot holds a rung: a `Hold` (body, sub-shape,
   anchor in the body's frame) taken through the general `take_hold` path, on
   a rung member (short in Y, under 0.2 m, the same bar test `find_rung`
   uses). The foot point stays within her leg's stroke of its rest position:
   leg travel 0.85 m (`leg_travel_m`) from straight below her centre.
3. **The holds carry her; no scripted height target.** Her body moves only
   as the holds carry it: the drive's target is derived from the holds she
   has (her hang under the hand holds, her stand on the foot holds), never
   from a clock. No tick advances a target height at `climb_mps`. Testable
   form: in phase 2, her body does not move down on any tick where no limb
   moved and no hold changed — the body follows the grips, it does not creep
   on a schedule. Her vertical speed stays inside what the holds' forces
   allow: no free-fall speed while three limbs hold.
4. **A supported step-off at the bottom.** Getting on from the floor and off
   at the bottom are real steps onto support, with no snap and no free-fall.
   At the bottom she transfers her weight to actual ground contact before
   releasing the final rung: feet on the ground while a hand still holds, then
   the hands let go. The old phase-3 let-go-and-fall (1.45 m on the grade
   ladder) is gone. Where the floor is past the drop the scanned edge allows,
   the move fails the way a missing rung fails — it does not drop her.
5. **The sim exposes each hold.** `ClimbView` reports, per limb: the hold
   point, whether it is held, and the load — the drive force reacted through
   that limb's anchors this tick (`hand_load_n`, `foot_load_n`, split the way
   M5c splits the two hand anchors). The view draws only from these fields:
   a limb is never drawn on a rung the sim is not using.

## Numbers (checked against trunk, 2026-11-08)

Argon's per-limb proposals in #321 section 4 are NOT adopted as settled —
checked against the J-044a spec (`03_EXECUTION/FIGHT/J044A_SPEC.md`) and trunk:

- **Reach 0.72 m (hands) / 0.98 m (feet): NOT SETTLED.** Argon's joint
  offsets mix frames: his shoulder at COM + 0.396 m is the rig's unscaled
  shoulder (1.446 m over feet) against the trunk COM (1.05 m over feet);
  at trunk scale the shoulder is COM + 0.75 m (`kShoulderUpM` 1.80 m).
  His hand reach (0.72 m) exceeds the J-044a anatomical arm (0.65844 m
  extended, upper 0.291 + forearm 0.278 + hand 0.089). The fix uses trunk's
  established envelope: `kArmReachM` 1.4 m (soft, with posture) and the
  Climber's revalidate bounds (`hand_above_center` 1.40, `reach_out` 0.95,
  `reach_low` 0.80, `leg_travel_m` 0.85).
- **1760 N per hand / 3520 N per foot: NOT SETTLED.** J-044a gives no
  per-limb climbing caps; its ledger is one total, `F_used <= 8000 N`
  across feet, hands and grip. Trunk's caps: climb grip `kClimbGripMaxN`
  5500 N, feet `foot_force_n` 1980 N. Argon's foot cap (3520 N) exceeds
  trunk's foot cap (1980 N) — it does not govern unless a protected change
  re-derives it. The qualitative point stands: one hand alone cannot carry
  her weight (2158 N), which is why the three-contact rule holds.
- **Friction mu 0.85: NOT SETTLED.** Trunk's established friction is the
  pole's mu_static 0.60 / kinetic 0.45; J-044a gives no rung friction.
  0.85 governs nothing until measured. Feet on rungs are compression first;
  the friction cone bounds only the shear.
- **Settled and used:** weight 2158 N (220 kg), budget 8 kN / 44 kW, leg
  travel 0.85 m, launch 6.30 m/s, rung pitch on the grade ladder 0.36 m,
  bottom rung 1.45 m above grade.

## The mechanism (the fix — section 4 of #321, ruled design)

- **Four-limb contact state machine.** `hold_ / hold2_ / foot_ / foot2_`
  become `limbs_[4]` (HandL, HandR, FootL, FootR), each with a phase:
  Attached, Unloading, Swing, Loading. Only attached limbs inside their
  reach transmit force; at most one limb is in Swing at any tick. A limb
  starts Unloading only when the other three are Attached at full load (or,
  during mount/dismount, while the sole has real deck/ground support).
  Unloading takes 3 ticks (load shifting to the other three while the limb
  is still on the rung); Swing carries 0 N along a smoothstep path to the
  next rung (multi-tick, bounded limb speed — a hold point never teleports);
  Loading takes 3 ticks after `take_hold` verifies the rung in Jolt. The
  view reads the swing path straight from `ClimbView`.
- **Four-beat diagonal gait.** Order HandL, FootR, HandR, FootL. Each hand
  targets one pitch beyond the other hand's rung inside its reach band; each
  foot likewise. Top mount: feet on the deck carry her while both hands take
  the top rungs, then the feet step up rung by rung, always three points.
  Bottom dismount: the hands keep stepping down to the bottom rung while the
  feet lower; her soles touch grade and carry her before the hands unload and
  release. The M5 same-member second-hand contract is superseded by this
  gait (hands are on neighbouring rungs while stepping); `test_second_hold`
  is updated as a protected change with a plain before/after when the fix
  lands — it asserted the old contract, not the ruled one.
- **COM bounded by the attached limbs' reach, no `target_y_` ramp.** The
  desired height is clamped to the intersection of the attached limbs'
  reach bands (plus a small margin); the desired velocity is the climb rate
  inside the clamp, else zero. Her COM cannot pass the reach of the holds
  she has. `rate_mps()` stays only as the limb-move speed. With no attached
  limbs the climb force is zero and she falls.
- **Force and power split per limb inside the budget.** The required force
  is the PD on the clamped COM target plus weight, charged through the one
  `Budget::take` (8 kN, 44 kW); each limb's share is clamped to its trunk
  capacity (hands within the 5500 N climb grip, feet within the 1980 N foot
  force, shear within the trunk friction 0.60) and the shares are scaled to
  the budget. Equal and opposite force goes on any dynamic hold body. Loss
  of hold: the remaining limbs either carry her or she slips/falls — a
  missing rung fails the move, it never continues the animation.
- `step()` writes the limb phases, points and per-limb loads into
  `ClimbView` the way it writes the hand fields today. `Matron::View` then
  reads the Climber's limb holds for `climb_feet_support` and the second
  hand instead of the centre raycast and the geometric pose (follow-up
  commit; the drawn limbs are Claude's side).

## What stays

The force budget (8 kN, 44 kW), leg travel (0.85 m) and launch (6.30 m/s)
limits, determinism, and the 300-line diff cap. Ladder geometry is a
protected change — this job moves no steel. No per-route, per-deck or
per-transition code: the foot stepping is one general mechanism for every
ladder. Pole, jump, dyno, gap and ledge-jump paths are untouched.

## Tests (failing on trunk first)

- `test_rung_support` (`tests/sim_tests_matron_climb.cpp`, `matron-climb`
  slice): the synthetic ladder world from `test_rungs`, up then down, with a
  per-tick audit in phase 2 — at least three limbs on rungs at every tick,
  one limb moving at a time, hands in reach, feet on rung bodies within leg
  stroke, no body creep on ticks away from a grip change, no tick over
  0.20 m, a supported step-off at the bottom (feet on the goal floor while a
  hand still holds, then Done standing). Fails on trunk: the Climber takes no
  foot hold on the rungs, both hands jump together, and the body creeps at
  0.6 m/s on a clock. Section 5's falsifiers join it: a loaded limb's hold
  point never jumps in one tick (the swing carries it, multi-tick); the
  velocity change per tick stays inside the budget-plus-gravity bound;
  wrists and feet move at bounded limb speed.
- World audit (measurement first, then the fix): `test_grade_ladder_descent`
  — her grade-ladder descent at x=12, z=-123.6 on the real tower, per-tick
  hand/foot distance to its rung, vertical speed, ticks with no limb on a
  rung, force per hold. The BEFORE table on current code, then the AFTER
  table after the fix; both tables go on #321 (main posts them). The AFTER
  table is the proof for the fix. The same audit then extends to the
  `run_matron_ladders` enumeration, one ladder at a time.

## What this spec does not do

It does not change the mantle, the pole grip, or the view drawing. It does
not add a per-ladder fix. It does change `test_second_hold`: the M5
same-member contract is superseded by the ruled diagonal gait, so that
test's expectation changes as a protected change (own protected branch,
plain before/after) when the fix lands. If the foot stepping cannot hold the
rate on some ladder, the move fails there the way a missing rung fails —
that failure is reported, not papered over.
