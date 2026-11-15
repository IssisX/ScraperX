# J-082: the climb above 221 m, audit and stage designs (#400)

Design only: nothing here is built. Trunk `4fce440a`, read 11-15. Files: WI `band_wet_isolation.cpp`,
PS `band_plate_shop.cpp`, FC `band_facade_crane.cpp`, MS `band_midstack_service.cpp`, VR `vertical/vertical_route.cpp`.
Numbers marked *est.* are sums, not runs.

## 1. Audit

**The played route** (`tests/sim_tests_canonical_ascent.cpp`), 19 legs:

| m | What he does | Kind | Code |
|---|---|---|---|
| 220-242 | AS-012 swing, kicks the trip | machine | `swing.cpp:268` |
| 242-264 | C6 west band | climb | `climb_c6.cpp:50` |
| 264-291 | cascade mast, Action (or C7 climb, `climb_c7.cpp:89`) | machine | VR:29 |
| 291-315 | pitman lift, HAUL | machine, by hand | VR:38 |
| 315-340 | barrel helix, Action, mantle onto TP-340 | machine | VR:50 |
| 340-374 | G: prop-pin lanyard | pulled handle | PS:243 |
| 374-418 | H: girder pin, chock lanyard | pulled handle | PS:358 |
| 418-462 | I: domino-pin lanyard | pulled handle | PS:475 |
| 462-484 | I's way out | **ladder** (the only one on the played route above 340) | PS:494 |
| 484-528 | J: rail block laid, chock lanyard | pulled handle | FC:237 |
| 528-572 | K: pendant | pulled handle | FC:335 |
| 572-640 | L: clutch thrown, drop pulled | pulled handle | FC:438 |
| 640-662 | M: reel chock | pulled handle | MS:242 |
| 662-684 | C4 service gantry | climb | MS:358 |
| 684-706 | N: GRAB the flap chain | pulled handle | MS:627 |
| 706-728 | C5 cooling plant (or the bait lift, her weight, MS:1320) | climb | MS:927 |
| 728-750 | O: chock lanyard | pulled handle | MS:1129 |
| 750-778 | crown gondola, Action | machine | VR:62 |
| 778-803.5 | luffing derrick, pigs thrown | machine | VR:70 |

**The no-lift routes** (`build_climbing_route`), 25 segments:

| m | What | Code |
|---|---|---|
| 220-242 | boards + **ladder** | WI:739-742 |
| 242-264 | beam, panel caught by a jump | WI:744-745 |
| 264-286 | shimmy under a bulkhead, boards, standpipe | WI:747-751 |
| 286-308 | boards + **ladder** | WI:753-755 |
| 308-330 | catwalk, panel caught by a jump | WI:757-758 |
| 330-340 | **ladder** | WI:760 |
| 340-352 | **ladder** | PS:825 |
| 352-374 | wall-climb stack | PS:583 |
| 374-396 | gantry: poles, monkey bars, hang-ups | PS:657 |
| 396-418 | gorge: poles, wall-run (pit ladder for a miss, PS:755) | PS:722 |
| 418-440 | run: poles, slide, vault | PS:783 |
| 440-462, 462-484 | boards + **ladder** (each); J-105's spiral flight 462-467.75 beside it (PS:99) | PS:837-842 |
| 484-506 | boards + **ladder** | FC:776-781 |
| 506-528 | springboard, poles | FC:541 |
| 528-550 | stepping stones, poles | FC:601 |
| 550-572 | tuck wall, poles | FC:661 |
| 572-594 | slide-jump, poles | FC:713 |
| 594-616 | boards + **ladder** | FC:776-781 |
| 616-640 | hatch **ladder** | FC:783 |
| 640-662, 662-684, 684-706, 706-728, 728-750 | hatch **ladder** (each) | MS:1286, 1287, 1290, 1291, 1292 |

From 340 up: **11 ladder stretches** on the no-lift route (4 boards-and-ladder: 440, 462, 484, 594; 7 ladder only:
340, 616 and the five hatches), plus I's ladder on the played route. Every lift on the played route from 340
to 750 starts with a pull.

## 2. Designs (pick by number)

Rules held: each a new action for him; no copies (no throw, no landing start, no pig in a pan, no her-weight
start, no ram, no pendant); no machine touched (Cory 11-11, #424); **every ladder stays**, as her way and
the easy backup, until J-105 replaces it. At most two rides in the set (1 and 5). His body gives 3.2-3.76 m a
hang, 1.85 m a mantle, so the rest of a 22 m rise is poles, as in J-017's eight stages.

1. **340-352, the uphill zip** (ride). He jumps from a 2.2 m stand on TP-340 to a parked trolley's bar; his
   weight frees it; a counterweight over a sheave on the 352 ring hauls him up a cable over the plate, braked
   by his hand, and he drops onto the ring. Real: 93 kg on a 22 degree cable pulls 342 N down it against a
   50 kg weight's 490 N; about 1 m/s2, about 7 m/s over 27 m unbraked, held under 5.4 m/s by the 900 N hand
   brake (*est.*). Reuses the zipline's trolley, brake and buffer (`zipline.hpp`), kit rope. **New code:** a
   park catch his weight frees on an uphill line. Medium.
2. **440-462, the trapeze over the well.** Pole up a pier to a launch stand; catch a trapeze bar hung on 7 m
   rods under the 462 ring; pump it (Crouch at the bottom, the arms' 1500 N) and let go at the top of the swing
   onto a far stand 2.8 m higher (the yard's 2.2 to 5.0 m, out of a plain catch's reach); second pole to the
   ring. The well is 20 m wide here: room for the arc. Reuses `trapeze.hpp`. **New to the climb** (a yard toy
   today); **new code:** a second bar (the trapeze is one instance). Medium.
3. **462-484: covered.** J-105's leg spiral (step 1 built) is planned to replace this ladder and I's; its next step is
   Cory's call on #424. No J-082 design.
4. **484-506, the swing-pole launch.** Pole up to a lane; sprint off its end, catch a single bar over a 6 m
   gap, and Jump at the front of the swing: the arms add 2 m/s at 45 degrees to the swing's speed and he takes a
   lip about 1.5 m higher (*est.*); second pole to the ring. Reuses `launch_from_swing` (tested in
   `run_swing_launch`, on no stage). **New to the climb**; no new code. Small to medium.
5. **594-616, the gravity balance** (ride). The well is 7.6 m wide, so it stands outside the east face, where
   the slide-jump's pole already tops out. He steps onto its deck and presses Action (the vertical machines' start, not yet checked for this
   module): 2500 kg of ballast lifts
   the 1000 kg deck and him, the motor only brakes (`gravity_balance.cpp`, -80 kN, ballast does the work).
   **A parked machine put back** (rule 54: reuse first); travel cut from 25 to 22 m. Medium. It is a lift:
   pick it only if you want a ride here.
6. **616-640, the falling catch.** Pole up the 616 ring's inner face to a ledge; jump out across the 5.8 m
   well, fall past a rail under TP-640 and catch it at the chest (J-043's catch, `kBarSearchAimRise`), hand
   over hand to the hatch, hang up through it onto the plate. **New to the climb** (deck 154 only); no new
   code. Small to medium.
7. **640-662, the flow jump** (beside M, a choice to the ride). A long run on TP-640, sprint held to flow's
   9 m/s (`kFlowSpeedMps`) and carried through the air over a gap no sprint jump clears (over 8.98 m), the
   hands on the lip of a raised deck, then a pole to the 662 deck. **New to the climb.** Room on TP-640 for the
   run is not yet measured. Small to medium.
8. **662-684: covered** by C4. 9. **684-706: covered** by N for him, J-105's hoist gallery for her.
   10. **706-728: covered** by C5 and the bait lift. 11. **728-750: covered** by O for him, J-105's north stair
   for her.
12. **484-640 in one: Cory's glass air tube** (his own idea, 10-20, waiting on him under rule 64). It would
    take the place of 4, 5 and 6 at once.

**Push back.** (a) 2, 4, 6 and 7 still climb most of their height on poles, J-017's pattern; if that reads
as repetitive, only rides or 12 fix it. (b) The hook is found on the 220 ring, keeps once taken, reels 30 m
and bites rungs and rails: it may already skip ring gaps (not checked). No design uses it. (c) The pulled
starts of G to O stay, by Cory's 11-11 ruling; rule 53 calls that pull spent. That is his call, not J-082's.
