# J-017 handoff, worker 2, 2026-11-03 (session_01W3SSN2utVR8V9YiKAfLJee), written at ~565k

Job: #124, the route above 221 m. Reports to the integrator session_01W3SSN2Y8JGNMvmxc394dkf. Takes no new job.

## Head (state at 6:00 PM Central)
- Landed: #180 (352->374 wall-climb), #182 (374->396 bar traverse, gantry turned onto the 374 ring's north band, c6ecb7f), #186 (crane 506->528 springboard), #189 (528->550 precision stones), #190 (550->572 tuck wall).
- In CI/review: #183 (396->418 wall-run gorge) at 36b1ca7: trunk merged in, E-guard re-pinned 13642769397912826673 -> 17708029113149545001, run_gorge now requires native wall_kick_count delta >= 3 (measured 4). #191 (crane 572->594 slide jump) at 02ed086 (round-the-572-ring walk kept, plus a walk back to the west band). #192 (route-pockets test, in main()'s default sequence) at b4f4edf. #185 (418->440 slide + speed vault) at 2f69c54, from old trunk.
- Branches: `claude/integrate-protected-j017-{wallrun,slidevault,crane-slidejump,scan-pockets}`; `claude/integrate-protected-j017-crane-precision` and `-tuck` already merged.

## Left to do
1. When #183 lands: merge trunk into #185's branch, run `SCRAPERX_ONLY=AS-007 timeout 150 ./build/host/scraperx_sim_tests | grep "E guard"` (the stock_hash on line ~11; the require fails first, that is fine), set the pin in tests/sim_tests_wet.cpp:279 with old/new and the reason in the comment and in #185's body, rerun AS-007 and `SCRAPERX_ONLY=AS-008-route` (about 220 s). #185 edits only the h == 418 pass of the plate route loop; it uses its own `stage_pole` helper in sim_tests_plate.cpp: after the merge reuse trunk's `gantry_pole` instead (same signature) so there is no duplicate.
2. #191: when its CI is green the integrator asks Codex and merges. If athletic-traversal goes red there, the 572 west band is narrower where the pier stands (x -8.2..-6.2): 1.49 m free past it.
3. #192: merge on green; nothing else.
4. Final line on #124 when the last one lands, then stop.

## Measured and ruled out (do not repeat)
- Precision jump is not a gate: a walking jump with the stick held crosses a 2.5 m stone gap (reach ~6 m > 4.5 m). Tuck and slide jump are likewise comfortable-line verbs, not gates; springboard is a real height gate (hands to 3.76 m standing, ~4.7 m springboarded). Only the grappling hook is a strict 11 m gate: parked for Cory with the 66 m of ladders (440 -> 506).
- A vault is refused when its landing is on the rail's own body (`probe_vault_landing`): the slide stage's rail is in the shop-frame body, the deck in the route body.
- A pole's top under a ring slab blocks the top-out (440 ring south band inner edge z -160.17; 572 ring north band inner edge z -145.29): keep poles inside the inner edges.
- The 374 east band's outer side must stay walkable (run_workers' welder test, tests/sim_tests_movement.cpp:618, welder lane x 13.6, z -156..-145).
- Her scan: `route-pockets` (#192) passes on every branch; she cannot follow the wall-climb or bars.

## Method that works
Branch from trunk; put the stage's run in the route test (`run_plate_route` / `run_crane_route`); `SCRAPERX_ONLY=AS-008-route` or `AS-009-route` once (220-240 s); `SCRAPERX_ONLY=AS-007` (E-guard pin) and `rule_check`; look at it with the Godot runtime (`native` build, `run_project`, a second `Camera3D` made current, since the main camera is rewritten every frame).
