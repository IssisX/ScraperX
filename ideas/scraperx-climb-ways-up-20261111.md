# Ways up between the machines, 340 m to 750 m (design only)

For J-082 (the canonical climb above 221 m), J-085 (her ways up) and J-102 (#421). No game code is in this
change. Everything below is world geometry, so each build is a protected change
(`<agent>/integrate-protected-<job>`).

## What this replaces

Twelve ladders stand between TP-340 and the 750 deck. Eleven ways up replace them (the leg spiral replaces two).
Every machine and stage stays as built and stays the route, in order. Nothing here is a ladder, a lift, a
handle or a puzzle.

| Ladder today | Built by | Replaced by |
|---|---|---|
| 340.25 to 352.25, TP-340 to the 352 ring | `band_plate_shop.cpp` `build_climbing_route` | 5 plate stacks |
| 440 to 462, boards and ladder | same, the loop's default body | 10 scaffold lifts |
| 462 to 484, boards and ladder | same | 1 leg spiral (shared) |
| 462.25 to 484.25, from I's cage (the machine route) | `build_stage_i`, line 405 | 1 leg spiral |
| 484 to 506, boards and ladder | `band_facade_crane.cpp` `build_climbing_route` | 4 outside flights |
| 594 to 616, boards and ladder | same | 7 hanging shelves |
| 616.25 to 640.25, through TP-640's hatch | same | 3 the lip |
| 640.25 to 662.1, beside M | `band_midstack_service.cpp` `build_climbing_route` | 6 hatch chimney |
| 662.1 to 684.1, beside C4 | same | 11 three pipes |
| 684.1 to 706.1, beside N | same | 2 the hoist gallery |
| 706.1 to 728.1, beside C5 | same | 8 duct flights |
| 728.1 to 750.1, beside O | same | 9 north stair |

## Three facts that shape every entry

1. **She cannot ride a machine.** Her moves are walk, mantle, step down, pole, riser, jump to a bar, jump to a
   lip, a short gap jump down, and a drop (`MoveKind`, `matron_traversal.hpp`). Once he rides, the cage is gone.
   So the no-lift route is her route, and it has to be whole from 340 to 750.
2. **Her scan only sees level, square-on tops.** A walkable patch is the top of an axis-aligned box at least
   0.18 m across; tops join when they are a step (0.36 m) apart and touch in plan
   (`matron_traversal.cpp`, the patch test and `build_regions`). A sloped plank is nothing to her. So every
   slope here is treads, never a ramp.
3. **A stair is quicker than a lift.** The lifts raise him about 1 m a second (#402's times: M 22 m in 21.7 s,
   N 19.8 s, O 24.1 s, G 34 m in 31.7 s). On a stair he gains about 2 m a second, and she about the same. So a
   stair backup is quicker than the machine beside it, and she would be at the top before the cage. The
   mantle, hang and pipe ways run at the ladders' pace (20 to 40 s). **This is the one call for Cory**: quick
   stair backups (she keeps up, the machine becomes a choice) or slow ones (the machine stays the quick way,
   she falls 10 to 20 s behind a stage). The entries say which each is. A stair is slowed by length only,
   never by touching a machine.

## Numbers used, each read from the code

| | Him | Her | Where |
|---|---|---|---|
| Mass, height, width | 85 kg, 1.8 m, 0.7 m | 220 kg, 2.1 m, 0.6 m | `physics_world_tuning.hpp`; `Limits` in `matron_traversal.hpp` |
| Step up | 0.35 m | 0.36 m | `kStepMaximumHeight`; `step_max_m` |
| Mantle | 0.9 to 1.85 m | up to 2.30 m, at 1.4 m/s | C4's note in `band_midstack_service.cpp`; `mantle_max_m`, `mantle_mps` |
| Hang caught | 3.2 m up (C4's, reach 3.76 m), on a face 1.2 m deep | a lip up to 3.64 m | C4's note; `jump_hold_max_m` |
| Pipe | C4's standpipe, 0.16 m square | a pole up to 0.16 m, 0.5 m/s, grip 4600 N x 0.6 = 2760 N against her 2158 N | `build_c4`; `pole_up_mps`, `pole_squeeze_n`, `pole_mu_static` |
| On foot | 5.5 m/s, sprint 8, accel 22 | hunt 6.8 m/s, stride 4.2, accel 9 | tuning; `matron.hpp` `kHuntMps`, `kStrideMps`, `kFootAccel` |
| Ladder | 0.9 m/s | 0.6 m/s, if she fitted | `kClimbUpSpeed`; `climb_mps` |
| Drop | survives under 20.4 m | 4.6 m | C5's note; `drop_max_m` |
| Budget | | 8 kN, 44 kW; her weight is 2.2 kN; a stair at hunt pace is about 6 kW | `total_force_n`, `power_w` |

The tread used throughout: rise 0.275 m, going 0.6 to 0.7 m, 1.0 to 1.3 m wide. Eighty of them make 22.0 m.
The lower stack already has stairs of 0.27 m rise and 0.5 m going (`band_stack.cpp`, the two access stairs),
and her walk lifts her onto "a stair or plate edge within her stride" (`matron_traversal.cpp`, line 857). A
going of 0.6 m or more keeps each tread out of her "narrow" class (under 0.6 m), so she walks it without
balancing. Rails are C4's parapet: 0.8 m high, 0.25 m thick, below the 0.9 m his ledge probe starts at.

The frame, for placing things: ring at height h has inner half-size 14.72 - 0.91 (h - 330) / 22 about
(0, -150) and is 4 m wide (`ring_inner`); so 462: 9.26, 484: 8.35, 506: 7.44, 594: 3.80, 616: 2.89. The legs
are rust boxes in 11 m lifts on the rings' outer corners (`build_upper_frame`, `build_frame`).

**Times are sums from these speeds. Nothing here has been run.** Each says "estimate".

---

## 1. The leg spiral (best) — I's cage to J, 462.25 to 484.25

1. **Where.** From the 462 ring to the 484 ring at the north-west leg, between where I's cage parks (west band)
   and J's traveler (north face, 12 m east of the leg). It is on the machine route: this is the only ladder a
   player riding every machine still climbs. The backup's own ladder on the east band (462 to 484) is simply
   deleted: the rings are whole bands, so the backup walks round to this spiral.
2. **What.** A square stair wrapped four times round the leg, hanging out past the frame's corner. The leg is
   `build_upper_frame`'s corner column: 0.86 m square from 462 to 473, 0.84 m from 473 to 484, the upper lift
   standing 0.45 m further in.
3. **Him.** Walks or runs it; sixteen quarter-turns, each four treads and a landing. **Her.** Walks it: treads
   0.675 x 1.3 m, corner landings 1.3 m square, 5.5 m between one turn and the next overhead (she needs 2.1).
   No hands, no jump, no squeeze.
4. **Numbers.** Rise 22.0 m in 80 steps of 0.275 m: 4 turns of 20. Outer square 4.0 m, walkway 1.3 m, slope
   22 degrees, path 43 m. The gap to the leg is 0.05 to 0.5 m, too small for either body. It overhangs the 462
   ring's edge by 1.5 m on two sides; the innermost landing stands on the 462 ring at the foot and on the 484
   ring at the head (both rings cover it). Estimate: him 12 s (the ladder is 24.4 s), her 18 s (she brakes
   into every corner).
5. **What could go wrong.** (a) She is 220 kg and her middle is 1.05 m up: at a corner she could tip over a
   0.8 m rail. Fix: full-height plate on the two outside corners of each turn. (b) The last flight's top tread
   meets the 484 ring's slab edge: a near miss to settle by a few centimetres of geometry. (c) Camera inside a
   1.3 m walkway with a column beside it: check by eye before it lands. (d) Her on a long stair is unproven:
   build one flight first and run her on it.

## 2. The hoist gallery — beside N, 684.1 to 706.1

1. **Where.** From the 684 deck to the 706 deck, round the outside of N's headframe.
2. **What.** A walkway that climbs round the six yellow headframe posts (`build_stage_n_frame`: 0.3 m square,
   at x -11.4, -8.6, -4.9 and z -157.9, -152.1), so whoever is on it circles the cage and hopper as they move.
3. **Him.** Walks it, 1.75 turns. **Her.** Walks it; while he rides N she is in his sight the whole way, and
   he in hers. Treads 0.63 x 1.3 m.
4. **Numbers.** 74 treads and 6 landings, 80 steps, 22.0 m. Long sides 11 treads, short sides 10. Path 56 m,
   slope 24 degrees. It starts at the north-east corner going west, so its east side passes 9.6 m over the
   deck where he boards the cage. It ends on the south side, level with the 706 deck's west edge. West side
   hangs 0.95 m outside the tower. Estimate: him 11 s, her 13 s; N's ride is about 20 s.
5. **What could go wrong.** (a) She reaches the 706 deck before the cage does. That is the call in fact 3;
   if unwanted, add a second lap (lower rise per step). (b) The flap arm, its chain and the pour must stay
   inside the posts: not checked part by part. (c) It meets the 706 deck's west parapet (0.8 m): he vaults,
   she mantles and steps down; or open one bay.

## 3. The lip — 616.25 to 640.25, onto TP-640

1. **Where.** From the 616 ring up to TP-640, replacing the ladder through the plate's hatch.
2. **What.** Long flights round the inside of the four legs, then a gangway out under the plate and a last
   flight round the outside of its east edge. The legs are `build_frame`'s columns (inner faces 6.37 m out,
   then 5.91 m); the plate is 24 m square and 1.5 m thick, so it overhangs the legs by 5.8 m.
3. **Him.** Walks 4.6 sides of 10.5 m, out along the gangway, up the edge and onto the plate. **Her.** The
   same, walking. A stair cannot pass the hatch for her: the hatch is 2.15 m long, and clearing a 1.5 m slab
   with a 2.1 m body on this slope needs about 9 m. Going round the edge needs no hole.
4. **Numbers.** 88 steps of 0.273 m, 24.0 m. Inside: 73 steps to 636.16, which leaves 2.59 m under the plate.
   Gangway 6.2 m. Edge flight 14 treads, 9.8 m, outside x = 12. Treads 0.7 x 1.3 m, path about 64 m.
   Estimate: him 14 s (the ladder is 26.7 s), her 15 s.
5. **What could go wrong.** (a) L's winch drums and ropes inside the legs at this height were not traced:
   place the gangway on the north half, away from the cart (z -159.75) and its weight. (b) The edge flight is
   over the full drop: plate wall on the outside. (c) Gangway headroom is hers plus 0.49 m; do not raise it.

## 4. The outside flights — 484 to 506

1. **Where.** From the 484 ring's north-west corner (beside the spiral's head) to the 506 ring's east band,
   where the springboard starts.
2. **What.** Three long straight flights round the outside of the west, south and east faces, leaving the
   north face to J. Hung from the legs (`build_frame`: 0.82 m square here).
3. **Him.** Sprints three 18 m flights. **Her.** Runs them. This is the one place it is a plain foot race.
4. **Numbers.** 78 treads and 2 corner landings, 22.0 m; treads 0.7 x 1.3 m, 1.3 m outside the ring edge. The
   506 ring stands 0.91 m further in, so the head is a 2.5 m bridge. Estimate: him 8 s sprinting or 11 s
   walking, her 10 s.
5. **What could go wrong.** (a) His sprint on treads is unmeasured. (b) Both arrive at a corner fast: plate
   walls on the two corners. (c) Quicker than J: see fact 3.

## 5. The plate stacks — TP-340 to the 352 ring, 340.25 to 352.25

1. **Where.** On TP-340 at z = -158, up to the 352 ring's east band, where the wall stack starts.
2. **What.** Seven stacks of steel plate, each 1.5 m higher than the last, stepping up to the ring's inner
   face (`ring_inner(352)` = 13.81 m).
3. **Him.** Eight mantles of 1.5 m (his range is 0.9 to 1.85). **Her.** Eight mantles (her limit is 2.30),
   each top 1.5 m deep and 2.4 m wide, open above.
4. **Numbers.** 8 x 1.5 = 12.0 m; run 10.5 m. Estimate: him 9 s (the ladder is 13.3 s), her 15 s.
5. **What could go wrong.** (a) TP-340's reach east of x = 3 at this z was not read. (b) His ledge probe needs
   each stack's face and top in one body, as C4 does.

## 6. The hatch chimney — beside M, 640.25 to 662.1

1. **Where.** Under the 662 deck's existing hatch (x 8.1 to 9.7, z -147.4 to -145.25).
2. **What.** A timber tower the size of the hatch, with half-floors left and right alternately, so it comes up
   through the hatch with the deck untouched.
3. **Him.** Fourteen mantles of 1.56 m, turning each time. **Her.** The same; each half-floor is 1.6 x 1.07 m,
   and the one above her is 3.1 m up.
4. **Numbers.** 14 x 1.561 = 21.85 m. Estimate: him 20 s, her 30 s; M's ride is 21.7 s.
5. **What could go wrong.** (a) Posts over 5 m tall and 0.16 to 1.5 m square read as risers to her: break
   them at each floor. (b) Fits the hatch with 0.5 m to spare each way for her; no rail inside.

## 7. The hanging shelves — 594 to 616

1. **Where.** On the 594 ring's north-east corner (the band's corner is 4 m square), up to the 616 ring.
2. **What.** Six deep shelves, left and right alternately, 3.14 m apart; a seventh level with the 616 ring.
3. **Him.** Seven jumps to a hang and a pull-up, C4's move (3.2 m there). **Her.** Seven jumps to a lip
   (`LedgeJump`; her limit is 3.64 m, so 0.5 m to spare).
4. **Numbers.** 7 x 3.143 = 22.0 m. Each shelf face 1.2 m deep (his hang needs it), top 1.6 x 3 m.
   Estimate: him 21 s, her 40 s. He gains here.
5. **What could go wrong.** (a) Her lip jump has no proof on record at this height. (b) A miss falls at most
   one shelf for her if the shelves overlap by 0.5 m; without the overlap it is over her 4.6 m.

## 8. The duct flights — beside C5, 706.1 to 728.1

1. **Where.** Over the north half of the 706 deck, up to the 728 deck's west parapet. She cannot do C5 (a
   crawl under 1.45 m, a 6.5 m gap), so this is her only way here.
2. **What.** Three runs of the cooling plant's ducts laid as steps, joined by plenum boxes to mantle.
3. **Him.** Walks 21 treads, mantles 1.55 m, three times. **Her.** The same. Duct tops 1.0 m wide, no rail.
4. **Numbers.** 3 x 21 x 0.275 + 3 x 1.55 = 22.0 m; flights 12.6 m. Estimate: him 11 s, her 14 s; C5 takes
   him about 29 s.
5. **What could go wrong.** (a) Much quicker than C5 for him: fact 3. (b) Keep the first flight on the deck's
   west side so the walk from N to C5 stays open. (c) N's posts at x = -4.9 are 0.95 m south of it.

## 9. The north stair — beside O, 728.1 to 750.1

1. **Where.** In the open strip north of the 728 and 750 decks (4.2 m wide, above C5's top).
2. **What.** Eight short flights back and forth, hung from O's north columns (`build_stage_o_frame`).
3. **Him and her.** Walk it. Its foot is west of where C5's last mantle comes up; its head is at the 750
   deck's north parapet (he vaults 0.8 m, she mantles and steps down).
4. **Numbers.** 72 treads and 8 landings; flights 5.4 m; footprint 7.8 x 2.6 m. Estimate: him 12 s, her 16 s.
5. **What could go wrong.** Eight U-turns: plate walls at the ends.

## 10. The scaffold lifts — 440 to 462

1. **Where.** Against the 462 ring's east inner face, after the run.
2. **What.** G's scaffold again, standing: four board floors, each reached by 3.95 m of bare tube and a
   1.55 m mantle.
3. **Him.** Climbs the tube as C4's standpipe. **Her.** Pole climb (tube 0.16 m), then the mantle.
4. **Numbers.** 4 x (3.95 + 1.55) = 22.0 m. Estimate: him 23 s, her 40 s. A slow backup: I stays quicker.
5. **What could go wrong.** Her step from a pole's head onto a floor: end each tube 0.2 m under the floor's
   top and 0.14 m off its face, as C4's.

## 11. The three pipes (weakest) — beside C4, 662.1 to 684.1

1. **Where.** East of the 684 deck's middle east column; the 684 deck's east edge has no parapet.
2. **What.** Three 6.3 m standpipes with two 1.55 m mantles between, on platforms off the column.
3. **Him and her.** As entry 10. She cannot do C4 itself (a shimmy and a 3 m level gap).
4. **Numbers.** 3 x 6.3 + 2 x 1.55 = 22.0 m. Estimate: him 23 s, her 41 s; C4 takes him 36 s.
5. **What could go wrong.** It is the nearest thing here to a ladder, and C4 and C5 already use the pipe.
   If one entry is dropped, drop this and give C4's backup a second lap of entry 2's kind.

---

## Other places the same ideas would help

By the numbers only; her scan has not been run on any of these.

- **352 to 374, the wall stack:** five faces of 4.4 m (`kStackRise`). Her lip limit is 3.64 m. A plate-stack
  step of 2.2 m at each face would make it hers and change nothing for him.
- **418 to 440 (the run) and 572 to 594 (the slide jump):** each has a crawl beam 1.45 m over the lane. She is
  2.1 m tall and has no crawl. She needs a way round each beam.
- **C4 and C5:** covered by entries 11 and 8.
- **The lower stack (#421):** the missing ways 11 to 22, 22 to 33, 55 to 66, 88 to 110 and 110 to 132. The
  hatch chimney fits any existing hatch without changing a deck; the leg spiral fits any corner column.
- **Free for her already, perhaps:** N's headframe posts (0.3 m square, 28 m tall) and O's columns (0.4 m
  square, 21.5 m) are inside her riser range (0.16 to 1.5 m square, 5 m or taller), as are the frame's legs.
  They end under decks. A small landing at a head would give her a way he cannot take, at 0.6 m/s.

## Build order

1. One flight of entry 1 and her run on it. Everything walked depends on that proof.
2. Entry 1 whole: it removes the last ladder from the machine route in this band.
3. Cory's call on fact 3, then the rest in rank order.
