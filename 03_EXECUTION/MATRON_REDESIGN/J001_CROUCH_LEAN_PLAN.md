# J-001: her crouch and lean in the hold (design, 11-14 6:16 PM CST)

A proposal for Cory's OK; no code or test changed, none run. Built on the J-001 branch at
e6f45b42 (trunk has no five beats). `HOLD_WITH_REAL_ARMS.md` already has her "bent at the hips and
knees": the bend is no new move. Section 3's steps 6-7 are the one item for his OK.

## What the code has (differs from the brief)

- The branch has a trunk: `Trunk{hip_up, pitch}`, `bend_to_hands`, `shoulder_of`. Hips 0.99 m
  down to 0.55 m, pitch to 80 deg, trunk 0.45 m, shoulders 0.16 m aside, arm 0.50 m. It only
  clamps hand poses, costs nothing, and the view adds 20 deg on top.
- Her physical arm ignores it: `hand_arm_` is tied to `shoulder_local()` (1.80 m), limit
  1.95 m. The "1.5-1.6 m to his belt" figure is from that point.

## 1. Least crouch and lean per beat

Frame: `out` from her axis to the drop, `up` over her feet. Shoulder =
(0.45 sin p, 0.99 - crouch + 0.45 cos p), 0.16 m aside; in reach = within 0.50 m of it.
Her spot is 0.42 m in from the rail; rail top 1.07-1.09 m. Him: 0.35 m round, ankle 0.82 m
and collar 0.55 m from his belt; lying, his axis is 0.35 m up and 0.65 m or more from hers.
Her swat lands within 0.15 m, her fingers close within 0.14 m. Rule: least crouch; lean at
most 60 deg on deck, and her chest clear of a rail.

| Beat | Hand point (out, up) | Crouch | Lean |
|---|---|---|---|
| 1 swat his chest | 0.65-0.85, 1.35 | 0 | 0-27 deg |
| 1 his ankle on the deck | 0.60-0.75, 0.35 | 0.27-0.34 m | 60 |
| 2 drag | -0.10, 0.80, 0.30 aside | 0.18 m | 0 |
| deck switch, two hands | 0.65, 0.35, hands 0.55-0.82 apart | 0.30-0.34 m | 60 |
| 3 pin, fatality | collar 0.62, 1.37; belt 0.42, 1.42 | 0 | 16 |
| 3 pin, regular (collar pose clamped) | collar 0.85, 1.37; belt 0.57, 1.42 | 0 | 54 |
| 4 tear, wrist outside | 0.72, 0.90 | 0 | 55 |
| 4 tip | belt 0.57, 1.52; ankle 0.30, 1.77 | 0 | 27 |
| 5 hang, regular | 0.50, 1.15 | 0 | 15 |
| 5 hang, his window | 0.77, 1.37 | 0 | 42 |
| 5 let-go | his middle 0.92 out (bar: 0.5 past the lip) | 0 | cap |

- At a rail she never crouches: her hips (0.99) are under the top rail, so bent knees put her
  chest into it. With 0.08 m of chest over a 1.09 m top the cap is atan(0.40 / 0.18) = 66 deg.
- At 66 deg her hand reaches 0.88 m out (0.91 in line with her shoulder). The let-go needs 0.92:
  0.01-0.04 m short on a plumb hang. Near-miss fix: her hold stand up to 0.05 m nearer the
  rail (0.08-0.12 m of air is there).
- Beat 4: one hand on his belt at 1.42 m, her other reaches down to about 0.95 m: she takes
  his ankle only once it is that high.

## 2. The model: keep the kinematic trunk, tie the arm to it, charge it

Pick: the kinematic shoulder frame, not a real joint. A hinged trunk splits her 220 kg capsule
(protected mass), adds a body that hits the rail and him, and moves every matron group. The
hold needs only the shoulder in the right place and the bend paid for.

1. Both arm limits anchor at `shoulder_of(side)`, limit 0.50 m (Carbon's sphere). Jolt's
   DistanceConstraint has no anchor setter: add one in `jolt_patch.cmake` patch 5, or re-make
   the limit as the anchor moves.
2. The trunk moves at finite speed and acceleration (now 2.5 rad/s, 0.8 m/s, no acceleration
   limit). No velocity writes: its reaction goes on her capsule as a force.
3. One new take per tick, after her hands, before her feet: `take_limb(m * a, Path::Trunk)`.
   Force is mass times acceleration (her weight stays with the floor, as in her walk). `grant`
   counts only force times speed, so the m g rise power is taken from `power_left` and booked
   beside it. Lowering is free, as in `grant`. A short grant slows the bend.
4. Cost: lean (110 kg, 0.25 m lever) about 340 N and 580 W; knees (190 kg) about 760 N and
   1500 W: under 10% of 8 kN and 4% of 44 kW.
5. Moment: not charged. Her capsule cannot tip, and the valve on `report_held_load`'s shift
   was removed 10-31. The shift (0.28 m at his window) stays a logged number.
6. Time: 60 deg or a deck crouch takes 0.42 s, already in the branch's passing 5-7 s runs.

## 3. Deck switch, one hand on him every tick

R is her first hand, L her second. Ankle to collar (1.37 m) is past her 0.86 m span: via the belt.
1. R keeps his ankle at 900 N, its pose following the ankle (2600 N for 0.4 s tore it).
2. She steps to his hip, 0.65 m off his line, facing across him, and crouches (0.34 m, 60 deg).
3. L takes his belt (0.82 m from the ankle; 0.04 m of margin). Gate: L closed.
4. Only then R opens. L holds the belt where it closed (no push; the 490 N fix).
5. She side-steps to his chest, still crouched; R takes his collar (0.55 m). Gate: R closed.
6. She rises in place, knees first (0.42 s), to the fatality haul poses (collar 1.30, belt 0.95,
   0.45 out): he comes up in front of her. Gate: his belt at 0.90 m or more.
7. She walks him to her spot; hands go to the pin poses only there.
Steps 6-7 wait on build step 1; they are beat 3's "hauls him up by the collar and belt".

Why 17.9 s (my read, unproven): lying, he is 0.70 m high and the rail is one bar at 1.07 m
(rays at 0.5 and 0.99 m hit nothing). In the old gap he got up, met the rail upright and bent
over it (up.y 0.25). Held all the way he arrives low with her belt hand already on, so the haul
starts at once, hands posed past the rail line: he goes under the bar. When her belt hand leaves
for his wrist he hangs by his collar outside the rail (up.y 0.87), the bar between collar and
belt. Each re-pose hauls him against it; all 4 go. His get-up acts only when he is free,
so that switch changes nothing.
Test, old against WIP: his middle's height on the tick it crosses the rail line (`PROBE haul`
prints both), and on the first ArmPlay tick his up.y and his ankle past the line. Right if the
WIP crosses under 1.07 m with his ankle outside. Wrong if he crosses over the bar, or a run
starting with up.y near 0.25 still takes over 10 s.

## 4. Falsifiers and build order

One per beat:
1. Grab: an ankle take in matron-grab needs hips under 0.55 m or lean over 60 deg.
2. Drag: hips under 0.80 m, or grip load over 2000 N.
3. Pin: hand error over 0.03 m at her spot with lean at the rail cap.
4. Tear: wrist over 0.50 m from her left `shoulder_of`. Tip: ankle not at 0.95 m in 3 s.
5. Hang: let-go under 0.5 m past, window reach over 0.75 m, or hang start after 4.45 s.

Build order (each alone; protected steps on their own branch):
0. Probe her drawn shoulders against `shoulder_of`; fit hips, trunk, half-width. Pass: within
   0.03 m on held frames.
1. Scratch probe of section 3's test. Pass: read kept or dropped.
2. Deck switch steps 1-7. Pass: matron-grab 36 PASS (arm snagged under 10 s), unheld Dangle
   frames 0 (now 44 and 61); then 631833c6 lands.
3. Rail lean cap from `rail_up_`. Pass: lean within it every tick; fatality bars PASS.
4. Arm limits on `shoulder_of`, 0.50 m, in the five beats from her Grab reach on (the floor
   fatality and the ladder pull keep today's limits). Pass: hand to shoulder 0.51 m or less on
   every tick of them; matron-budget's 12 at or under 8000 N.
5. Charge the trunk (new ledger row: protected). Pass: the row is non-zero, every TOTAL at or
   under 8000 N and 44 kW, fatality 5-7 s PASS.
6. View: no extra 20 deg, no 0.08 m shoulder slide. Pass: `stretch_arm_hold` nothing over
   1.05, `grip_hold` worst 0.05 m, by eye too. Then CI's proof.

## 5. Estimates (not measured by me)

- Her shoulder height: branch 1.44 m (0.99 + 0.45), brief 1.32 m; half-width 0.16 or 0.19. At
  1.32 the regular pin and the tip no longer fit; the drag crouch is 0.06 m. Step 0 settles it.
- Trunk masses (110, 190 kg), the 0.25 m lever, 0.2 s ramps, 0.08 m of chest.
- His points: lying 0.65 m off her, ankle 0.60-0.75 m off, wrist at 0.90 m.
- A one-bar rail at the arm-snagged spot (measured at fatality rails only).
- The let-go shortfall (today's pass may lean to 80 deg: read, not run), the 0.04 m
  ankle-to-belt margin, the times.
- The 17.9 s read, until build step 1 runs.
