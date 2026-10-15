# The parkour move set

The spec for ScraperX-Claude's climbing and parkour, 2026-10-15. What elite movement is,
what each move needs from the native body, and what is built against it. The native
(`src/sim/simulation.cpp`) owns every move: a move is a change to the forces, constraints and
contacts of an 85 kg body in Jolt at 90 Hz, never a placed animation. Every number here is a
project parameter (CHOSEN unless marked DERIVED or MEASURED), and each move names the tests
that prove it.

## 1. What the research says

How expert traceurs move, reduced to what a physics body can do.

1. **Momentum is the currency.** A traceur never stops at an obstacle: every move converts the
   run's speed into height, distance or a new direction, and gives it back on the far side. The
   amateur's tell is the full stop before each obstacle; the expert's is a steady rhythm through a
   whole line. Mechanically: the entry speed into a vault, mantle or wall-run must leave with the
   body, less only what the move physically costs.
2. **Four contact points, used as needed.** Hands are feet when the terrain asks: a kong vault
   loads the arms like a stride; a cat leap catches with hands and feet on the wall at once; a
   climb-up is a mantle done from the hands. Legs are springs, not stilts: the knees bend to take
   a landing and to pre-load the next push.
3. **The eyes commit the body.** A precision jump is decided before takeoff: the eyes fix the
   landing, the body takes off at the one speed that lands there. A cat leap is sighted on the
   lip. The hands reach for the edge that the eyes picked. Late decisions, in the air, are the
   amateur's.
4. **The catch happens on the way up.** A cat leap's hands close on the lip near the apex of the
   jump, usually still rising, so the body arrives soft and the arms take little. Waiting to be
   falling before grabbing is what makes a catch hurt.
5. **Flow is chains without hard stops.** Vault, two strides, precision, cat leap, climb-up, roll
   out of the drop: each move's exit is the next move's entry, with no neutral stance between.
   Rhythm (step count between obstacles) is kept by adjusting stride, not speed.
6. **Reading the terrain is continuous.** The expert's eyes are 2-3 obstacles ahead; the current
   one is already decided. Affordances (what can be caught, vaulted, stepped) are read as the
   body moves, not at a stop.
7. **Landings are shaped, not suffered.** A precision landing takes the drop in the legs, on
   the balls of the feet, and stays on the lip; a roll converts a drop with forward speed into
   travel; a stumble is a failure to shape it.
8. **Primate traits that read as "expert" in motion:** the committed, un-hedged leap; the arms
   as a second pair of legs; the body folding (tuck) and opening (extension) to steer momentum;
   the wall used as a floor turned sideways (wall-run, tic-tac); the hand-over-hand swing.

What this means for a first-person physics body: the moves need no new body parts, they need
the seams between moves to carry momentum, the catch to fire while rising, the wall to be a
contact the legs can push off at an angle of the player's choosing, and a way to commit a
jump to a sighted landing.

## 2. What the native already does (audit, 2026-10-15)

| Move | Built | Where | Proof |
|---|---|---|---|
| Step-up (≤ 0.35 m) | yes | `try_step_up` | `athletic traversal` |
| Vault (0.35–1.15 m, landing beyond) | yes | `begin_vault`, `traversal_point` | `athletic`, `touch_vault` |
| Speed vault (≥ 6 m/s, run's own pace, takes off up to 2 m early) | yes | `vault_reach`, speed-vault branch | `speed vault`, `touch_speed_vault` |
| Double-tap jump → vault (0.30 s window) | yes | `try_begin_jump_vault` | `touch_jump_vault` |
| Mantle (0.35–1.85 m), approach step keeps run speed into it | yes | `begin_mantle` | `athletic`, `touch_mantle` |
| Ledge hang (compliant arms, finite grip, tear) | yes | `begin_hang`, `attach_hang_arms` | `step2 last hold`, `touch_c*` |
| Climb-up from hang, blocked climb returns to hang | yes | `begin_mantle_from_hang` | `step2` |
| Climb holds (ladders, pipes, lattice), shimmy, lower | yes | `begin_climb`, `Lowering` | `step2` |
| Swing from bars, pump, hand over hand | yes | `begin_swing`, kip | `swing`, `touch_monkey_bars` |
| Wall-run as steps (3 plants), kick-off | yes | `update_wall_run` | `wall run`, `touch_wall_run` |
| Slide (≥ 5 m/s, friction-braked) | yes | `begin_slide_if_running` | `slide`, `touch_slide` |
| Roll on landing (crouch held, ≥ 2.5 m/s fwd, clear lane) | yes | `record_landing` | `landing`, `touch_roll` |
| Landing debt, stumble, balance | yes | `record_landing`, `recovery_balance` | `landing` |
| Jump buffer (0.10 s before landing) | yes | `kJumpBufferTicks` | `jump buffer` |
| Coyote time | **decided against** (Governing Law 40: no free-air jump) | task #24 | — |
| Sprint-jump momentum carry; air keeps excess speed | yes | `air_full_speed_` | `landing` air case |

The gaps, found by playing the seams rather than the moves:

- **G1. A mantle throws the run away.** `complete_traversal` exits a mantle at the support's
  velocity only. Running at a 1 m wall, the body climbs it, then stands still on top: the
  amateur's full stop, done for them.
- **G2. The catch waits to fall.** `try_begin_hang` refuses while the body rises faster than
  0.2 m/s, and needs the stick held into the wall at that tick. A jump at a lip catches only
  after the apex, so every catch is a falling catch, harder on the grip than it should be; and a
  jump made while steering is missed.
- **G3. A wall-run needs a jump first and ends where it started.** `update_wall_run` fires
  only in `flight_from_jump_`, and a kick-off goes straight out along the wall's normal. There
  is no tic-tac (kick off one wall toward a ledge or the next wall), the signature wall move.
- **G4. No precision jump.** The jump is one speed (5.5 m/s up, the run's horizontal). A gap
  that needs less than a full jump is overshot; one that needs the jump aimed at a sighted lip
  is luck.
- **G5. The eyes do not read ahead.** The HUD offers one affordance at arm's length. The vault
  reads ahead at a run (2 m); nothing else does.

## 3. The move set

Each move: trigger, input, physics, feedback, transitions. "Rel." is relative to the support
under the feet (every move resolves against the support's contact-point velocity).

### M1 Run and sprint (built)
- Input: stick; SPRINT held for 8.0 m/s, else 5.5 m/s.
- Physics: 22 m/s² ground traction scaled by slope and balance; inertia kept, braking is
  friction (no input = 22 m/s² decel to rest).
- Transitions: → jump (M2), vault (M4), mantle (M5), slide (M9), step-up, wall-run (M7) by
  jumping along a wall.

### M2 Jump and sprint-jump (built)
- Trigger: JUMP with real support; buffered 0.10 s before landing.
- Physics: 5.5 m/s up rel. to support, equal and opposite impulse to a dynamic support;
  horizontal speed carried (a sprint jump is 8 m/s out, 1.51 m up, 1.11 s).
- Air: 14 m/s² control, weaker than ground; a released stick never brakes a committed arc;
  excess world speed (platform, swing, launcher) kept.

### M3 Precision jump (new, this pass)
- What it is: the sighted, committed jump onto a lip or ledge short of a full jump's range.
- Trigger: grounded, the eyes (facing, pitch) on a landing surface 0.8–4.5 m ahead whose top
  is within −2.0..+1.2 m of the feet, with a clear stand on it (the same probe as a mantle's
  landing). JUMP while it is sighted.
- Physics: the takeoff keeps the jump's 5.5 m/s vertical (the legs' push is what it is) and
  sets the horizontal speed to what lands the feet on the sighted point: v_h = d / t_flight,
  t_flight from the vertical arc and the drop/rise (DERIVED per jump, bounded to ≤ the sprint's
  8 m/s; past that the body is told the jump is short and takes off at full speed, no magic).
  The horizontal change is capped at the legs' push, 3.0 m/s (CHOSEN) over the run's speed, so
  a standing precision reaches ~1.5 m and a running one reaches the far side of a 4 m gap only
  from a run. No air steering is added: a committed arc.
- Feedback: the landing point shown as a small ring on the surface while sighted (HUD
  affordance `precision`), the arms' pre-jump load, a soft knees-bend on landing with the
  prepared free speed (crouch not needed: the precision landing is itself the shaped landing,
  so the free speed is 8 m/s when the feet land with the point under the sole: within the
  capsule's radius plus 0.15 m).
- Transitions: → landing (M10) at the lip; → cat leap (M6) if the lip is above the feet and
  the body falls short (the hands close on it); → roll (M11) with crouch held.

### M4 Vault and speed vault (built; momentum fix in this pass)
- As built. The exit keeps the carried speed. No change except G1's fix does not apply (vaults
  already carry).

### M5 Mantle and climb-up (built; momentum fix, this pass)
- Trigger: as built (0.35–1.85 m lip ahead within reach, clear stand on top).
- Fix for G1: the exit velocity keeps the horizontal speed the body had when the mantle began,
  projected onto the facing, less a cost of 1.5 m/s (CHOSEN: the push from the hands and the
  knee onto the top), floored at 0. A walking mantle (3 m/s) steps off at 1.5 m/s; a sprint
  mantle (8 m/s) runs off at 6.5 m/s; a standing one still stands. Climb-up from a hang keeps
  its 0 exit (nothing was carried).
- Transitions: → run; → precision (M3) or jump within the buffer; → vault at the next rail.

### M6 Ledge catch and cat leap (built; rising catch and hands-reach fix, this pass)
- Trigger: airborne, the stick held toward the wall (intent ≥ 0.3, as built), a lip 0.45–1.35 m
  above the body's centre within reach ahead, or a hold at hand height.
  Fix for G2: the catch is taken while rising up to 2.5 m/s (CHOSEN: hands close near the
  apex; above that the lip is going past too fast to close on), in flight from a jump only: a
  step's lift pops the body up at the same 2 m/s, and a walk over a kerb beside a rail must
  not swing from it (seen on deck 8, walking into S3's cage). Rising faster than the old
  0.2 m/s, a hold is taken only where the first pull up from it is clear (`climb_move_clear`
  over 0.5 m): a body that has just struck the rung above it keeps its flight and catches on
  the way down, clear, as it always did (seen on C2's davit ladder: the jump goes up into the
  bottom rung, and a catch pinned under it never climbs).
  Tried and withdrawn: a momentum catch with the stick neutral (closing ≥ 1 m/s, intent ≥ −0.3).
  It caught holds the thumb never pointed at: a walk-off the 220 ring south grabbed AS-007's
  ladder below the lip instead of falling; a run up the yard curb hung from the compressor's
  top; a slingshot flight would snag the first hold in front of the eyes. Hands reach where the
  thumb points.
- Physics: as built (arms on a 2 Hz spring, 4.5 kN grip, 350 N·s tear). A rising catch loads
  the arms less (MEASURED in the native test: a catch at +1.5 m/s rising against the same lip
  caught falling at −1.5 m/s, grip peak lower).
- Feedback: the hands close (arms pose), a thud on the wall for the feet, haptic grab.
- Transitions: → climb-up (stick forward), → shimmy, → let go, → swing on a bar instead.

### M7 Wall-run and tic-tac (built; tic-tac added this pass)
- Trigger: airborne from a jump, running along a wall within 40°, stick not away from it.
  Fix for G3, the tic-tac: JUMP while a wall step is planted kicks off along the **stick's**
  direction blended with the wall's normal: the push is 3.0 m/s out (as built) plus up to
  2.0 m/s (CHOSEN) along the wall in the stick's direction across it, so the body can be sent
  toward a ledge or the opposite wall. The kick resets the step count, so a second wall can be
  run (a wall-to-wall tic-tac), at most 2 kicks per flight (CHOSEN: the legs' push is finite).
- Physics: each plant 1.2 m/s out, friction 0.8 holds the fall; as built.
- Feedback: foot plants heard and felt (as built), the view banks 4° toward the wall (new).
- Transitions: → cat leap on a lip, → landing, → second wall.

### M8 Wall-jump (built as the kick-off; direction added by M7)

### M9 Slide (built)
- Crouch at ≥ 5 m/s within 45° of facing; friction 0.35 g; steer 2 m/s²; ends under 2.5 m/s.
- Transitions: → run; → jump out of the slide (JUMP is accepted during a slide, the body stands
  as it leaves).

### M10 Landing (built)
- Free to 6 m/s (1.8 m), 8 m/s prepared (crouch, or a precision landing per M3); above, a
  balance debt paid back through traction, never a lockout; across the facing → stumble.

### M11 Roll (built)
- Crouch at contact, ≥ 2.5 m/s forward, ≥ 5 m/s down, clear lane: keeps the speed, quarter
  debt, 0.6 s low.

### M12 Climb, shimmy, lower, swing (built)
- As built; no change this pass.

### M13 Reading ahead (G5, partial this pass)
- The precision ring (M3) is the first read-ahead affordance beyond the vault's. The others
  (a cat-leap lip sighted before the jump) are the next pass.

## 4. Timing windows (forgiving, skill-rewarding)

| Window | Value | Why |
|---|---|---|
| Jump buffer | 0.10 s | a frame early is kept |
| Double-tap vault | 0.30 s | a late press is a vault |
| Rising catch | ≤ 2.5 m/s up | near the apex, not at a lip flashing past |
| Tic-tac kick | while a step is planted (0.22 s per step, 3 steps) | the foot has to be on the wall |
| Roll | crouch held at contact | a decision, not a reflex |
| Precision sighting | the lip under the reticle at takeoff | the eyes commit |

No coyote time (Governing Law 40), no auto-grab of a released hold (27-tick lockout).

## 5. Camera and hands

- Hands: every state drives the first-person arms (`first_person_arms.gd`); the new precision
  shows the arms loading before takeoff (the run pose with a dip), the tic-tac shows the
  planted foot's side arm braced.
- Camera: bank 4° into a wall-run, the landing dip as built, speed FOV as built, REDUCED MOTION
  removes the bank.

## 6. Tuning values

All in `src/sim/simulation.cpp`, named `k…`, one line each, with the reasoning beside them:
`kMantleExitCostMps` (1.5), `kHangMaximumClimbSpeed` (2.5, was 0.2), `kHangApexSpeed` (0.2, the
old limit: above it a hold needs a clear first pull), `kHangIntentDotThreshold` (0.3, unchanged),
`kWallKickAlongMps` (2.0), `kWallKicksMax` (2), `kPrecisionMinM` (0.8),
`kPrecisionMaxM` (4.5), `kPrecisionRiseMaxM` (1.2), `kPrecisionDropMaxM` (2.0),
`kPrecisionPushMps` (3.0), `kPrecisionLandingSlack` (0.15, past the foot's radius). The feel is in these; the
proofs bound them.
