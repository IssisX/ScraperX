# Test-reach map: which tests a change can reach

Look up the file (or function, with its line number) you changed; the row names the native groups and touch scenarios that run it. This serves `CLAUDE.md` "Test only what a change reaches": run those, cheapest first, once, and say in the report which and why. It is a lookup, not a verdict: read "How far to trust it" first.

Built 2026-10-29 on trunk `ScraperX-Claude` at commit b551b24. Regenerate when the test lists, the kit's body order or the call structure change a lot (the method is below); a stale map is worse than none.

## How to use it (30 seconds)

1. Changed a `src/sim/*.cpp` or `.hpp`? Find the file in section 2. Changed a function in a big file? Section 3 has it with its line range. Changed Matron behavior? Start from the subject index (section 4).
2. Take the tiers left to right: **D** (a test calls this code straight from its own body), **T1+O**, **O**, **T1** (weaker, see below). Run the first group of the strongest tier present; add a second only if the change spans subjects. Groups inside a tier are listed narrowest first (fewest `src/sim` functions reached without the per-tick step), so the first is the cheapest.
3. Run it as `SCRAPERX_ONLY=<group> ./build/host/scraperx_sim_tests` (the 13 named in `CMakeLists.txt` are also ctest names, `ctest -R scraperx_sim.<group>`). Under 3 groups should be enough for one change.
4. Changed `godot/presentation/**/*.gd`? Section 7 (files) and section 8 (functions in big files) name the touch scenarios (`--uitest=<scenario>`); a `touch_*` scenario also runs as `<name>_buttons` on the BUTTONS scheme.
5. Row says `tick only`, `no scenario or test reaches it` or `**no test reaches**`: nothing gives direct evidence; say so in the report instead of picking a random group.

## How far to trust it

Built from the code, not from names: every `src/sim`, `src/bridge` and `tests/*.cpp` file was parsed with libclang (the build's include paths and defines) into a call graph; each group's seed functions are the `run_*`/`test_*` calls in its block of `tests/simulation_tests.cpp` main(); the closure of those through the graph is what the group "reaches". GDScript was parsed by a small script: each scenario's `match` arm in `ui_test_driver.gd` is the seed, calls are resolved by function name (and by receiver name for `obj.f()` calls), and `_native.f()` calls are resolved to the bridge method bound under that name (`src/bridge/scraperx_simulation.cpp`) and from there to `src/sim`.

What it cannot see, and what I did about it:

- **Every group ticks the world.** `Simulation::advance_frame` reaches 89 of the `src` files from any group, so plain reachability says "all tests reach everything". The per-tick step is therefore cut out: **T1** is reach without passing through it (setup calls, debug hooks, requests, accessors). Code that only runs in the tick when the right state holds is not separated by T1.
- **O (observes).** A group counts as observing a function if it reaches the function (tick included) and its test code reads a struct field (`MatronSnapshot.grabs`, `Snapshot.zipline_riding`, ...) that the function also touches. Fields read by more than 35% of groups are ignored as noise. This is evidence a test watches the function's effect, not proof it runs the branch you changed.
- **D** is the strongest: some test function calls the function directly. It is rarely complete (a group that reaches the code through the tick is not a D).
- **Virtual calls** are followed to every override; Jolt callbacks (contact listener and layer filters) are tied to every `PhysicsSystem::Update`; `make_unique`/`emplace` construct edges go to the class's constructors; a destructor counts with its constructor. Function pointers stored in tables, `std::function`, and GDScript signals/`call_deferred` by string are followed only when the name is a literal in a `connect`/`call_deferred`/`Callable` call; anything else is missed (GD side: **may run via signals/scene wiring**).
- **Nothing was run.** This map is static, per the job. The ctest names and run command are from `CMakeLists.txt`.

## 1. Native test groups

103 entries: the 100 `SCRAPERX_ONLY` groups of `tests/simulation_tests.cpp` (the second `swing` block is shadowed by the first and never runs: `run_swing` runs only in the full run), the full run (`scraperx_sim.athletic_traversal`: every `run_*` in main(); the groups `matron-chase` and `matron-grab` run only in their own slice, as the comments in main() say), and the two other test executables.

"Src functions reached" is the group's size without the per-tick step: the smaller, the cheaper and more focused. The last column lists the `src/sim` and bridge `.cpp` files that no more than 30% of groups reach: what makes the group different.

| Group (`SCRAPERX_ONLY=`) | ctest name | Runs | Src functions reached without the per-tick step | Files only this kind of group reaches |
|---|---|---|---|---|
| `jumpbuffer` | - | run_jump_buffer | 563 | - |
| `edge-stand` | - | run_edge_stand | 564 | - |
| `twin-geometry` | - | run_twin_geometry | 564 | - |
| `workers` | - | run_workers | 565 | - |
| `C6` | - | run_c6 | 566 | - |
| `landing` | - | run_landing | 566 | - |
| `parkour_flow (own exe)` | scraperx_sim.parkour_flow | main | 566 | - |
| `AS-010-c4` | - | run_service_c4 | 567 | - |
| `AS-010-c5` | - | run_service_c5 | 568 | - |
| `parkour` | - | run_parkour_seams | 568 | - |
| `AS-008-route` | - | run_plate_route | 569 | - |
| `handling` | - | run_wet_handling | 569 | - |
| `slingshot` | - | run_slingshot | 570 | - |
| `summit` | - | run_summit, run_summit_arm | 570 | - |
| `AS-008-wreck` | - | run_plate_wreckage | 571 | - |
| `AS-009-route` | - | run_crane_route | 571 | - |
| `trapeze` | - | run_trapeze | 571 | - |
| `AS-008-band` | - | run_plate_band | 572 | - |
| `AS-009-band` | - | run_crane_band | 572 | - |
| `AS-010-route` | - | run_service_route | 572 | - |
| `AS-009-wreck` | - | run_crane_wreckage | 573 | - |
| `look` | - | run_look_target | 573 | - |
| `AS-008` | - | run_plate_shop | 574 | - |
| `matron-ladder` | scraperx_sim.matron-ladder | run_matron_ladder | 575 | - |
| `zipline` | - | run_zipline | 575 | - |
| `AS-009` | - | run_facade_crane | 576 | - |
| `AS-010-n` | - | run_service_n | 576 | - |
| `matron-room` | - | run_matron_room | 576 | - |
| `AS-010` | - | run_midstack_service | 577 | - |
| `matron-collision` | - | run_matron_collision | 577 | - |
| `AS-010-o` | - | run_service_o | 578 | - |
| `climb` | - | run_wall_climb, run_tuck, run_flow, run_springboard, run_swing_launch, run_slide_jump +3 | 580 | - |
| `AS-007-band` | - | run_wet_band | 581 | - |
| `AS-007-route` | - | run_wet_route | 581 | - |
| `stock` | - | run_loose_stock | 581 | - |
| `AS-010-band` | - | run_service_band | 582 | - |
| `matron-crash` | scraperx_sim.matron-crash | run_matron_crash | 582 | - |
| `matron-mischief` | - | run_matron_mischief | 583 | - |
| `tram` | - | run_tram | 583 | - |
| `pitman` | - | run_pitman | 584 | - |
| `water` | - | run_water | 584 | matron_fatality.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `helix` | - | run_helix | 585 | - |
| `incline` | - | run_incline | 586 | - |
| `up-throw` | - | run_up_throw | 587 | - |
| `matron-traverse` | - | run_matron_traverse | 588 | - |
| `one-arm` | - | run_one_arm_route | 588 | matron_fatality.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `forklift` | - | run_forklift, run_forklift_strain | 589 | - |
| `high` | - | run_high | 590 | - |
| `well` | - | run_well_stage_a, run_well_stage_b, run_well_stage_c, run_well_band | 590 | - |
| `ascent` | - | run_ascent | 591 | - |
| `derrick` | - | run_derrick | 592 | - |
| `matron-stand` | scraperx_sim.matron-stand | run_matron_stand | 592 | matron_routecraft.cpp |
| `swing (shadowed second block: never runs)` | - | run_swing | 592 | - |
| `wheel` | - | run_wheel | 592 | - |
| `matron-pipe` | - | run_matron_pipe | 594 | - |
| `spreader` | - | run_yard_crane_spreader | 594 | - |
| `stack` | - | run_stack | 594 | - |
| `matron-balance` | - | run_matron_balance | 596 | - |
| `body-order` | - | run_body_order | 600 | - |
| `determinism` | - | run_determinism | 600 | - |
| `mast` | - | run_mast | 604 | - |
| `matron-remaining` | scraperx_sim.matron-remaining | run_matron_remaining | 605 | - |
| `matron-arm` | - | run_matron_arm | 606 | matron_fatality.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `crane` | - | run_yard_crane | 609 | - |
| `matron-moves` | - | run_matron_pace, run_matron_unstick | 612 | matron_fatality.cpp, matron_moves.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-pursuit` | - | run_matron_pursuit | 612 | matron_fatality.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-player-independence` | - | run_matron_player_independence | 619 | matron_fatality.cpp, matron_moves.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-ring-descent` | - | run_matron_ring_descent | 624 | matron_fatality.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-found-climb` | - | run_matron_found_climb | 626 | matron_fatality.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-ladders` | scraperx_sim.matron-ladders | run_matron_ladders | 627 | matron_moves.cpp |
| `matron-tower` | scraperx_sim.matron-tower | run_matron_tower_proof | 628 | matron_moves.cpp |
| `matron-ways` | - | run_matron_ways | 629 | matron_moves.cpp |
| `matron` | - | run_matron | 633 | matron_fatality.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-chase` | scraperx_sim.matron-chase | run_matron_chase | 633 | matron_fatality.cpp, physics_world_arm.cpp, physics_world_grab.cpp |
| `matron-climb-scan` | - | run_matron_climb_scan | 633 | matron_moves.cpp |
| `up` | - | run_up_controls | 639 | - |
| `teeter` | - | run_teeter | 665 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `save` | - | run_save | 670 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `twin` | - | run_twin | 678 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `bay` | - | run_bay | 679 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `corpse` | - | run_corpse, run_arm_break | 685 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `magnet` | - | run_yard_crane_magnet | 695 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `swing` | - | run_brachiation, run_pipe_gallery, run_slide, run_speed_vault, run_wall_run, run_wall_climb +11 | 703 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `AS-007-E` | - | run_wet_e | 705 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `strike` | - | run_strike | 706 | fabricated.cpp, matron_fatality.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `AS-007` | - | run_wet_isolation, run_wet_e, run_wet_handling, run_wet_band, run_wet_route | 708 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `fabricated` | - | run_fabricated | 724 | fabricated.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `matron-fatality` | - | run_matron_fatality | 725 | fabricated.cpp, matron_fatality.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `matron-grab` | scraperx_sim.matron-grab | run_matron_grab, run_matron_arm, run_matron_fatality | 751 | fabricated.cpp, matron_fatality.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |
| `matron-exam` | - | run_matron_exam | 843 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-chain` | scraperx_sim.matron-chain | run_matron_chain | 844 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-narrow` | - | run_matron_narrow | 844 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-towers` | scraperx_sim.matron-towers | run_matron_towers | 844 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-exam-climber` | - | run_matron_exam_climber | 845 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-climb-board` | - | run_matron_climb_board | 859 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-climb` | - | run_matron_climb, run_matron_climb_board, run_matron_climb_scan | 872 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-hearing` | scraperx_sim.matron-hearing | test_matron_hearing | 939 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-exam-routecraft` | - | run_matron_exam_routecraft | 940 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `shared_survival (own exe)` | scraperx_sim.shared_survival | main | 940 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-board-walls` | scraperx_sim.matron-board-walls | run_matron_board_walls | 941 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-climb-support` | scraperx_sim.matron-climb-support | test_matron_climb_support | 941 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-exam-descent` | - | run_matron_exam_descent | 941 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `matron-routecraft` | - | run_matron_routecraft | 956 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_fabricated.cpp |
| `(all: scraperx_sim.athletic_traversal)` | scraperx_sim.athletic_traversal | run_pulley_static_end, run_matron_exam, run_core_traversal, run_fall_checkpoint, run_world_solids, run_twin_geometry +96 | 1291 | bridge/scraperx_simulation.cpp, fabricated.cpp, matron_fatality.cpp, matron_moves.cpp, matron_routecraft.cpp, physics_world_arm.cpp, physics_world_fabricated.cpp, physics_world_fatality.cpp, physics_world_grab.cpp |

Not groups but also tests: `tests/twin_geometry_test.py <pre-relocation commit>` (compares `src/sim/world_solids.inc` boxes against a base commit; no `src/sim/*.cpp` is run); CI runs `ctest` plus a bare `./build/host/scraperx_sim_tests` (every group in the default order).

## 2. Every `src/sim` file and the native groups that reach it

`Run these` lists tiers D, T1+O, O, T1 for the union of the file's functions (a group appears in its strongest tier only; `~all` means 80% or more of groups). Columns 4 and 5 are counts of groups that reach the file without / only through the per-tick step. `core: ~every group` means a function in the file is reached by 80% or more of groups without the tick (usually constructors, accessors or world building): use section 3 for the function you changed. `src/bridge` is not in any native group (the native tests do not link the bridge); section 9 covers it.

| Source file | Lines | Run these (tier, narrowest first) | Groups reaching it without the tick | Groups reaching it only via the tick | Verdict |
|---|---|---|---|---|---|
| `src/bridge/scraperx_simulation.cpp` | 2116 | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14 | 88 | specific |
| `src/bridge/scraperx_simulation.hpp` | 297 | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14 | 88 | specific |
| `src/sim/band_counterweight_well.cpp` | 1089 | **D** `matron-routecraft` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/band_facade_crane.cpp` | 549 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/band_midstack_service.cpp` | 1226 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/band_plate_shop.cpp` | 507 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/band_stack.cpp` | 1708 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/band_wet_isolation.cpp` | 799 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/bay.cpp` | 328 | **T1+O** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/bay.hpp` | 185 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/climb_c6.cpp` | 146 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/climb_c7.cpp` | 187 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/corpse.cpp` | 222 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/corpse.hpp` | 113 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/crew.cpp` | 357 | **T1+O** `matron-climb-scan`, `matron-exam`, `matron-chain`, `matron-narrow` +8 more · **T1** ~all (91) | 103 | 0 | core: ~every group |
| `src/sim/crew.hpp` | 141 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/fabricated.cpp` | 210 | **D** `fabricated` · **T1** `teeter`, `save`, `twin`, `bay` +22 more | 27 | 75 | specific |
| `src/sim/forklift.cpp` | 481 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/forklift.hpp` | 176 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/impacts.cpp` | 112 | **D** `stock`, `matron-crash`, `matron` · **T1+O** `up-throw`, `forklift`, `crane`, `up` +1 more · **T1** ~all (95) | 103 | 0 | core: ~every group |
| `src/sim/impacts.hpp` | 88 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/loose_stock.cpp` | 871 | **D** `stock`, `forklift`, `spreader`, `crane` +1 more · **T1** ~all (98) | 103 | 0 | core: ~every group |
| `src/sim/loose_stock.hpp` | 152 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/matron.cpp` | 4456 | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +98 more · **T1** `twin-geometry` | 103 | 0 | core: ~every group |
| `src/sim/matron.hpp` | 2461 | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +98 more · **T1** `twin-geometry` | 103 | 0 | core: ~every group |
| `src/sim/matron_climb.cpp` | 258 | **T1+O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +8 more · **T1** ~all (91) | 103 | 0 | core: ~every group |
| `src/sim/matron_contact.cpp` | 112 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/matron_fatality.cpp` | 339 | **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +16 more | 20 | 77 | specific |
| `src/sim/matron_found_climb.cpp` | 774 | **T1+O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +12 more · **T1** ~all (87) | 103 | 0 | core: ~every group |
| `src/sim/matron_grip.hpp` | 79 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/matron_hold.cpp` | 1533 | **D** `matron-routecraft` · **T1+O** `matron-traverse`, `matron-pursuit`, `matron-ring-descent`, `matron-tower` +4 more · **T1** ~all (94) | 103 | 0 | core: ~every group |
| `src/sim/matron_journey.cpp` | 528 | **D** `matron-board-walls`, `matron-climb-support`, `matron-exam-descent` · **T1+O** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +8 more · **T1** ~all (88) | 103 | 0 | core: ~every group |
| `src/sim/matron_line.cpp` | 387 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/matron_mover.hpp` | 114 | - | 0 | 0 | NO TEST REACHES IT |
| `src/sim/matron_moves.cpp` | 368 | **T1** `matron-moves`, `matron-player-independence`, `matron-ladders`, `matron-tower` +16 more | 20 | 82 | specific |
| `src/sim/matron_pipe.cpp` | 730 | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-towers` +4 more · **T1** ~all (95) | 103 | 0 | core: ~every group |
| `src/sim/matron_routecraft.cpp` | 603 | **D** `matron-routecraft` · **T1+O** `fabricated`, `matron-exam`, `matron-climb`, `matron-exam-routecraft` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +1 more · **T1** `matron-stand`, `matron-pursuit`, `matron-found-climb`, `teeter` +21 more | 30 | 72 | specific |
| `src/sim/matron_routecraft.hpp` | 145 | **D** `matron-routecraft` · **T1+O** `matron-exam-routecraft` · **T1** ~all (101) | 103 | 0 | core: ~every group |
| `src/sim/matron_travel.cpp` | 87 | **T1+O** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +11 more · **T1** ~all (88) | 103 | 0 | core: ~every group |
| `src/sim/matron_traversal.cpp` | 3381 | **D** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-chase` +10 more · **T1+O** `matron-traverse`, `matron-pursuit`, `matron-ring-descent`, `fabricated` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` · **T1** `matron-stand`, `matron-found-climb`, `teeter`, `save` +15 more | 37 | 65 | specific |
| `src/sim/matron_traversal.hpp` | 422 | **D** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +11 more · **T1** ~all (88) | 103 | 0 | core: ~every group |
| `src/sim/mechanism_kit.cpp` | 2295 | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-towers` +4 more · **T1** ~all (95) | 103 | 0 | core: ~every group |
| `src/sim/mechanism_kit.hpp` | 940 | **D** `magnet` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/physics_world.cpp` | 1431 | **T1+O** `stock`, `matron-crash`, `up-throw`, `forklift` +15 more · **T1** ~all (84) | 103 | 0 | core: ~every group |
| `src/sim/physics_world.hpp` | 1243 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_arm.cpp` | 259 | **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb` · **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +19 more | 23 | 68 | specific |
| `src/sim/physics_world_climb.cpp` | 796 | **T1+O** `matron-routecraft` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_fabricated.cpp` | 243 | **T1** `teeter`, `save`, `twin`, `bay` +23 more | 27 | 75 | specific |
| `src/sim/physics_world_fatality.cpp` | 415 | **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb` · **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13 | 78 | specific |
| `src/sim/physics_world_grab.cpp` | 556 | **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb`, `matron-routecraft` · **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +19 more | 23 | 68 | specific |
| `src/sim/physics_world_movement.cpp` | 1259 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_one_arm.cpp` | 162 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_probes.cpp` | 1212 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_state.cpp` | 888 | **T1+O** `edge-stand`, `twin-geometry`, `workers`, `C6` +69 more · **T1** `jumpbuffer`, `slingshot`, `trapeze`, `look` +26 more | 103 | 0 | core: ~every group |
| `src/sim/physics_world_strike.cpp` | 290 | **T1+O** `strike` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_support.hpp` | 860 | **D** `matron-exam` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_traversal.cpp` | 790 | **T1+O** `matron-climb`, `matron-routecraft` · **T1** ~all (101) | 103 | 0 | core: ~every group |
| `src/sim/physics_world_water.cpp` | 101 | - | 0 | 91 | tick-only (gated by runtime state) |
| `src/sim/pipe_gallery.cpp` | 92 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/rope_constraint.cpp` | 244 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/rope_constraint.hpp` | 122 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/shared_survival.cpp` | 207 | **D** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/shared_survival.hpp` | 100 | **D** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/simulation.cpp` | 1692 | **D** `jumpbuffer`, `edge-stand`, `twin-geometry`, `workers` +88 more · **T1+O** `matron-hearing`, `shared_survival (own exe)`, `matron-climb-support` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +4 more | 103 | 0 | core: ~every group |
| `src/sim/simulation.hpp` | 1924 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/slingshot.cpp` | 1073 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/slingshot.hpp` | 214 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/slingshot_model.cpp` | 76 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/summit.cpp` | 323 | **T1+O** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/summit.hpp` | 106 | **T1** `summit` | 1 | 0 | specific |
| `src/sim/swing.cpp` | 595 | **T1+O** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/swing.hpp` | 163 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/trapeze.cpp` | 366 | **T1+O** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/trapeze.hpp` | 152 | **T1** `trapeze`, `spreader`, `crane`, `up` +1 more | 5 | 86 | specific |
| `src/sim/twin_tower.cpp` | 194 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/barrel_helix.cpp` | 44 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/cascade_mast.cpp` | 78 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/crown_gondola.cpp` | 34 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/gravity_balance.cpp` | 34 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/luffing_derrick.cpp` | 145 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/pitman_lift.cpp` | 121 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/rack_climber.cpp` | 35 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/slab_incline.cpp` | 179 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/stone_wheel.cpp` | 264 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/traction_tram.cpp` | 49 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/vertical_machine.cpp` | 124 | **T1+O** `shared_survival (own exe)` · **T1** ~all (102) | 103 | 0 | core: ~every group |
| `src/sim/vertical/vertical_route.cpp` | 523 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/vertical/vertical_route.hpp` | 214 | **T1** `tram`, `pitman`, `helix`, `incline` +5 more | 9 | 0 | specific |
| `src/sim/yard_crane.cpp` | 898 | **T1+O** `crane`, `shared_survival (own exe)` · **T1** ~all (101) | 103 | 0 | core: ~every group |
| `src/sim/yard_crane.hpp` | 312 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/zipline.cpp` | 375 | **T1** ~all (103) | 103 | 0 | core: ~every group |
| `src/sim/zipline.hpp` | 215 | **T1** ~all (103) | 103 | 0 | core: ~every group |

Header-only files have inline functions only; `matron_mover.hpp` is a pure interface (no function bodies), so no test "reaches" it by itself: its `Mover::step` is reached through the ways that implement it (`TravelWay`, `PipeWay`, the climb and line ways).

## 3. Big files, function by function

Every function of 25 or more lines (40 in headers) in a file of 500 or more lines. `Reached w/o tick / only via tick` counts groups. A function with no reach has the note `**no test reaches**`: the native tests never run it (compare section 9 for the bridge).

### `src/bridge/scraperx_simulation.cpp` (2116 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 47-382 | `ScraperXSimulation::_bind_methods` | - | 0/0 | **no test reaches** |
| 1051-1081 | `ScraperXSimulation::get_landing_state` | - | 0/0 | **no test reaches** |
| 1100-1132 | `ScraperXSimulation::get_wet_state` | - | 0/0 | **no test reaches** |
| 1174-1203 | `ScraperXSimulation::get_service_state` | - | 0/0 | **no test reaches** |
| 1217-1250 | `ScraperXSimulation::get_slingshot_state` | - | 0/0 | **no test reaches** |
| 1391-1415 | `ScraperXSimulation::get_trapeze_state` | - | 0/0 | **no test reaches** |
| 1429-1479 | `forklift_dictionary` | - | 0/0 | **no test reaches** |
| 1520-1558 | `ScraperXSimulation::get_yard_crane_state` | - | 0/0 | **no test reaches** |
| 1678-1807 | `ScraperXSimulation::get_matron_state` | - | 0/0 | **no test reaches** |
| 1815-1925 | `ScraperXSimulation::get_matron_hold_state` | - | 0/0 | **no test reaches** |
| 1967-1996 | `ScraperXSimulation::get_swing_state` | - | 0/0 | **no test reaches** |
| 2016-2041 | `ScraperXSimulation::get_lift_state` | - | 0/0 | **no test reaches** |

### `src/sim/band_counterweight_well.cpp` (1089 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 104-138 | `cage_parts` | **T1** ~all (103) | 103/0 |  |
| 140-319 | `build_stage_a` | **T1** ~all (103) | 103/0 |  |
| 384-428 | `boom_parts` | **T1** ~all (103) | 103/0 |  |
| 430-590 | `build_stage_b` | **T1** ~all (103) | 103/0 |  |
| 726-998 | `build_stage_c` | **T1** ~all (103) | 103/0 |  |
| 1032-1078 | `build_climbing_route` | **T1** ~all (103) | 103/0 |  |

### `src/sim/band_facade_crane.cpp` (549 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 167-207 | `build_frame` | **T1** ~all (103) | 103/0 |  |
| 237-308 | `build_stage_j` | **T1** ~all (103) | 103/0 |  |
| 335-413 | `build_stage_k` | **T1** ~all (103) | 103/0 |  |
| 438-515 | `build_stage_l` | **T1** ~all (103) | 103/0 |  |

### `src/sim/band_midstack_service.cpp` (1226 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 158-184 | `build_frame` | **T1** ~all (103) | 103/0 |  |
| 242-321 | `build_stage_m` | **T1** ~all (103) | 103/0 |  |
| 358-423 | `build_c4` | **T1** ~all (103) | 103/0 |  |
| 504-574 | `build_stage_n_frame` | **T1** ~all (103) | 103/0 |  |
| 576-659 | `build_stage_n` | **T1** ~all (103) | 103/0 |  |
| 838-872 | `build_c5_frame` | **T1** ~all (103) | 103/0 |  |
| 874-959 | `build_c5` | **T1** ~all (103) | 103/0 |  |
| 961-1039 | `build_stage_o_frame` | **T1** ~all (103) | 103/0 |  |
| 1041-1191 | `build_stage_o` | **T1** ~all (103) | 103/0 |  |

### `src/sim/band_plate_shop.cpp` (507 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 153-226 | `build_stage_g` | **T1** ~all (103) | 103/0 |  |
| 268-358 | `build_stage_h` | **T1** ~all (103) | 103/0 |  |
| 385-465 | `build_stage_i` | **T1** ~all (103) | 103/0 |  |

### `src/sim/band_stack.cpp` (1708 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 158-183 | `cage_parts` | **T1** ~all (103) | 103/0 |  |
| 217-378 | `build_s1_headframe` | **T1** ~all (103) | 103/0 |  |
| 380-454 | `build_yard_traversal` | **T1** ~all (103) | 103/0 |  |
| 456-542 | `build_s1` | **T1** ~all (103) | 103/0 |  |
| 612-649 | `build_deck2_traversal` | **T1** ~all (103) | 103/0 |  |
| 651-723 | `build_c1_traversal_enhancements` | **T1** ~all (103) | 103/0 |  |
| 725-825 | `build_c1` | **T1** ~all (103) | 103/0 |  |
| 895-920 | `s2_cage_parts` | **T1** ~all (103) | 103/0 |  |
| 922-955 | `s2_beam_parts` | **T1** ~all (103) | 103/0 |  |
| 957-989 | `build_crossover_gangway` | **T1** ~all (103) | 103/0 |  |
| 991-1018 | `build_deck4_traversal` | **T1** ~all (103) | 103/0 |  |
| 1033-1093 | `build_s2_frame` | **T1** ~all (103) | 103/0 |  |
| 1095-1148 | `build_s2` | **T1** ~all (103) | 103/0 |  |
| 1162-1196 | `build_c2_traversal_enhancements` | **T1** ~all (103) | 103/0 |  |
| 1198-1307 | `build_c2` | **T1** ~all (103) | 103/0 |  |
| 1363-1394 | `build_north_crossover_gangway` | **T1** ~all (103) | 103/0 |  |
| 1396-1421 | `s3_cage_parts` | **T1** ~all (103) | 103/0 |  |
| 1464-1499 | `build_s3_frame` | **T1** ~all (103) | 103/0 |  |
| 1501-1558 | `build_s3` | **T1** ~all (103) | 103/0 |  |
| 1571-1679 | `build_c3` | **T1** ~all (103) | 103/0 |  |

### `src/sim/band_wet_isolation.cpp` (799 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 285-390 | `build_stage_d` | **T1** ~all (103) | 103/0 |  |
| 412-602 | `build_stage_e` | **T1** ~all (103) | 103/0 |  |
| 604-647 | `build_stage_f` | **T1** ~all (103) | 103/0 |  |
| 649-690 | `build_plate_and_header` | **T1** ~all (103) | 103/0 |  |
| 737-775 | `build_climbing_route` | **T1** ~all (103) | 103/0 |  |

### `src/sim/loose_stock.cpp` (871 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 74-372 | `make_spec` | **T1** ~all (103) | 103/0 |  |
| 376-444 | `add_stock_yard` | **T1** ~all (103) | 103/0 |  |
| 472-499 | `LooseStock::placements` | **D** `stock`, `forklift`, `spreader`, `crane` +1 more · **T1** ~all (98) | 103/0 |  |
| 507-538 | `LooseStock::sound` | **T1** ~all (103) | 103/0 |  |
| 591-616 | `LooseStock::add_piece` | **T1+O** `stock`, `forklift`, `spreader`, `crane` +1 more · **T1** ~all (98) | 103/0 |  |
| 637-664 | `spot_templates` | **T1** ~all (103) | 103/0 |  |
| 713-869 | `LooseStock::dress_tower` | **T1** ~all (103) | 103/0 |  |

### `src/sim/matron.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **senses and hunt, balance, pace, cable** row of section 4, then confirm it with this row.
 (4456 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 74-238 | `Matron::Matron` | **D** `matron-hearing`, `shared_survival (own exe)`, `matron-board-walls`, `matron-climb-support` · **T1+O** `matron-climb-scan`, `matron-exam`, `matron-chain`, `matron-narrow` +4 more · **T1** ~all (91) | 103/0 |  |
| 283-307 | `Matron::set_home` | **T1** ~all (103) | 103/0 |  |
| 309-336 | `Matron::place_for_test` | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +98 more | 23/79 |  |
| 412-491 | `Matron::muffled_distance` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 518-542 | `Matron::onto_floor_lines` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 561-736 | `Matron::route_toward` | **T1** `matron-moves`, `matron-player-independence`, `matron-ladders`, `matron-tower` +16 more | 20/82 |  |
| 738-767 | `Matron::way_clear` | **T1** `matron-moves`, `matron-player-independence`, `matron-ladders`, `matron-tower` +16 more | 20/82 |  |
| 838-884 | `Matron::feel_floor` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 886-949 | `Matron::settle_level` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 951-976 | `Matron::off_its_steel` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 978-1748 | `Matron::walk_toward` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 1780-1813 | `Matron::step_rage` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 1827-1851 | `Matron::step_planks` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 1853-1881 | `Matron::restore_planks` | **T1** `matron-stand`, `matron-found-climb`, `teeter`, `save` +25 more | 29/73 |  |
| 1911-2060 | `Matron::trap_step` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 2112-2141 | `Matron::take_way` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 2157-3120 | `Matron::pre_step` | **D** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more · **O** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +4 more | 7/90 |  |
| 3122-3178 | `Matron::post_step` | **D** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 3259-3311 | `Matron::fetch_stock` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 3313-3339 | `Matron::take_stock` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 3361-3410 | `Matron::throw_stock` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 3456-3635 | `Matron::restore` | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +98 more | 29/73 |  |
| 3675-3742 | `Matron::sense_near` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 3747-3952 | `Matron::take_blow` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 3976-4001 | `Matron::shape_fits` | **T1+O** `matron-routecraft` · **T1** `matron-traverse`, `matron-exam`, `matron-chain`, `matron-narrow` +10 more | 15/87 |  |
| 4010-4034 | `Matron::lie_down` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 4056-4092 | `Matron::sole_contact` | **T1** ~all (103) | 103/0 |  |
| 4119-4160 | `Matron::brace_at_edges` | **T1+O** `matron-exam-descent` · **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb` · **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +2 more | 7/90 |  |
| 4163-4213 | `Matron::step_knocked` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 4215-4454 | `Matron::refresh_view` | **T1+O** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-climb-support` +2 more · **T1** ~all (97) | 103/0 |  |

### `src/sim/matron.hpp` (2461 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/matron_found_climb.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (774 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 91-122 | `Matron::found_floor` | **T1+O** `matron-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 124-265 | `Matron::find_climb` | **T1+O** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +5 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-climb-board`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +1 more | 14/88 |  |
| 311-380 | `Matron::follow_found_climb` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 387-420 | `RoutecraftWay::step` | **T1+O** `matron-exam-routecraft`, `matron-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +8 more | 14/88 |  |
| 434-463 | `RoutecraftWay::view` | **T1+O** `matron-exam-routecraft`, `matron-routecraft` · **T1** ~all (101) | 103/0 |  |
| 494-558 | `Matron::start_journey` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-climb-board`, `matron-hearing`, `matron-exam-routecraft` +3 more | 14/88 |  |
| 560-616 | `Matron::end_journey` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-stand`, `matron-pursuit`, `matron-found-climb`, `teeter` +19 more | 30/72 |  |
| 618-705 | `Matron::step_journey` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +4 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +2 more | 14/88 |  |
| 730-763 | `JourneyWay::view` | **T1+O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +8 more · **T1** ~all (91) | 103/0 |  |

### `src/sim/matron_hold.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **hold** row of section 4, then confirm it with this row.
 (1533 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 47-85 | `Matron::begin_grab` | **D** `matron-routecraft` | 1/90 |  |
| 104-168 | `Matron::grip_lost` | **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +9 more | 13/78 |  |
| 224-337 | `Matron::choose_rim` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 364-448 | `Matron::choose_fatal_edge` | **O** `matron-traverse`, `matron-pursuit`, `matron-ring-descent`, `matron-tower` +4 more · **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 450-475 | `Matron::hang_room` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 508-551 | `Matron::fall_impact_mps` | **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +16 more | 20/77 |  |
| 615-1054 | `Matron::step_hold` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 1097-1199 | `Matron::step_armplay` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 1242-1281 | `Matron::arm_repose` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 1288-1319 | `Matron::spare_side` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |
| 1419-1444 | `Matron::over_her_deck` | **T1** `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)`, `matron-board-walls` +3 more | 7/90 |  |

### `src/sim/matron_journey.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (528 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 61-127 | `Matron::region_under` | **T1+O** `matron-tower`, `matron-ways`, `matron-climb-scan`, `matron-climb` · **O** `matron-ring-descent`, `matron-chase` · **T1** `matron-ladders`, `matron-exam`, `matron-chain`, `matron-narrow` +10 more | 18/84 |  |
| 141-194 | `Matron::travel_to` | **D** `matron-board-walls`, `matron-climb-support`, `matron-exam-descent` · **T1+O** `matron-exam-climber`, `matron-climb` · **T1** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +9 more | 18/84 |  |
| 196-483 | `Matron::step_travel` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +5 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +1 more | 14/88 |  |

### `src/sim/matron_pipe.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **corner pipe** row of section 4, then confirm it with this row.
 (730 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 71-106 | `Matron::pipe_rail_hold` | **T1** `matron-traverse`, `matron-exam`, `matron-chain`, `matron-narrow` +11 more | 15/87 |  |
| 111-138 | `Matron::pipe_face_hold` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 142-167 | `Matron::get_on_pipe` | **T1** `matron-traverse`, `matron-exam`, `matron-chain`, `matron-narrow` +11 more | 15/87 |  |
| 169-204 | `Matron::get_off_pipe` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 223-553 | `Matron::ride_pipe` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 563-600 | `Matron::follow_pipe` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |

### `src/sim/matron_routecraft.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (603 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 118-144 | `MatronRoutecraft::resume_transition` | **T1+O** `matron-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 162-246 | `MatronRoutecraft::plan` | **T1+O** `matron-exam`, `matron-climb`, `matron-exam-routecraft`, `matron-routecraft` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +2 more · **T1** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +6 more | 14/88 |  |
| 335-487 | `MatronRoutecraft::advance_plan` | **T1+O** `matron-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 488-545 | `MatronRoutecraft::hold` | **T1+O** `matron-exam-routecraft`, `matron-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +8 more | 14/88 |  |
| 563-602 | `MatronRoutecraft::step` | **D** `matron-routecraft` · **T1+O** `matron-exam-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +8 more | 14/88 |  |

### `src/sim/matron_traversal.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (3381 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 142-178 | `gap_flight` | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-towers` +4 more · **T1** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +10 more | 22/80 |  |
| 248-274 | `Budget::take` | **D** `matron-climb` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 334-365 | `Climber::revalidate` | **T1+O** `matron-climb` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 367-402 | `Climber::take_hold` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 404-434 | `Climber::find_rung` | **T1+O** `matron-narrow`, `matron-climb` · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan` · **T1** `matron-exam`, `matron-chain`, `matron-towers`, `matron-exam-climber` +8 more | 14/88 |  |
| 436-479 | `Climber::find_lip` | **T1+O** `matron-climb` · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 492-533 | `Climber::plant_leg` | **T1+O** `matron-climb`, `matron-exam-descent` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +8 more | 14/88 |  |
| 535-569 | `Climber::planted_leg_contact` | **T1+O** `matron-climb` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 576-644 | `Climber::drive_leg` | **T1+O** `matron-climb`, `matron-exam-descent` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +8 more | 14/88 |  |
| 699-742 | `Climber::stand` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-climb` · **T1+O** `matron-exam-climber`, `matron-climb-board`, `matron-exam-descent` · **O** `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 744-847 | `Climber::walk` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 860-887 | `Climber::begin` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +2 more · **T1+O** `matron-climb-support`, `matron-exam-descent` · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-traverse`, `matron-exam`, `matron-hearing`, `matron-exam-routecraft` +3 more | 15/87 |  |
| 924-959 | `Climber::step` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +2 more · **T1+O** `matron-exam-descent` · **O** `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 988-1076 | `Climber::step_rung_up` | **T1+O** `matron-narrow`, `matron-climb-board`, `matron-climb` · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan` · **T1** `matron-exam`, `matron-chain`, `matron-towers`, `matron-exam-climber` +7 more | 14/88 |  |
| 1080-1109 | `Climber::begin_mantle_from_hang` | **T1+O** `matron-narrow`, `matron-exam-climber`, `matron-climb-board`, `matron-climb` · **O** `matron-tower`, `matron-climb-scan` · **T1** `matron-exam`, `matron-chain`, `matron-towers`, `matron-hearing` +6 more | 14/88 |  |
| 1114-1251 | `Climber::step_mantle` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 1259-1365 | `Climber::step_dyno` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 1370-1492 | `Climber::step_rung_down` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 1499-1654 | `Climber::step_pole` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +4 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +2 more | 14/88 |  |
| 1660-1772 | `Climber::step_gap` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +4 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +2 more | 14/88 |  |
| 1777-1844 | `Climber::step_jump` | **T1+O** `matron-narrow`, `matron-climb-board`, `matron-climb`, `matron-exam-descent` · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan` · **T1** `matron-exam`, `matron-chain`, `matron-towers`, `matron-exam-climber` +6 more | 14/88 |  |
| 1849-1970 | `Climber::step_drop` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-exam`, `matron-hearing`, `matron-exam-routecraft`, `shared_survival (own exe)` +3 more | 14/88 |  |
| 2038-2241 | `AffordanceMap::scan` | **D** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-towers` +2 more · **T1+O** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +2 more · **O** `matron-traverse`, `matron-pursuit` · **T1** `matron-fatality`, `matron-grab`, `matron-exam`, `matron-climb-board` +6 more | 22/80 |  |
| 2248-2350 | `AffordanceMap::split_patches` | **T1+O** `matron-ring-descent`, `matron-tower`, `matron-ways`, `matron-chase` +2 more · **O** `matron-traverse`, `matron-pursuit` · **T1** `matron-ladders`, `matron-fatality`, `matron-grab`, `matron-exam` +12 more | 22/80 |  |
| 2352-2435 | `AffordanceMap::build_regions` | **T1+O** `matron-ring-descent`, `matron-tower`, `matron-ways`, `matron-chase` +2 more · **O** `AS-010-c4`, `matron-traverse`, `stack`, `body-order` +3 more · **T1** `matron-ladders`, `matron-fatality`, `matron-grab`, `matron-exam` +12 more | 22/80 |  |
| 2490-2982 | `AffordanceMap::build_edges` | **T1+O** `matron-ring-descent`, `matron-ladders`, `matron-tower`, `matron-ways` +11 more · **O** `AS-010-c4`, `matron-traverse`, `stack`, `body-order` +3 more · **T1** `matron-fatality`, `matron-grab`, `matron-exam`, `matron-hearing` +3 more | 22/80 |  |
| 2984-3064 | `AffordanceMap::plan` | **D** `matron-tower`, `matron-chase`, `matron-chain`, `matron-narrow` +5 more · **T1+O** `matron-ladders`, `matron-ways`, `matron-climb-scan`, `matron-exam-descent` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +1 more · **T1** `matron-exam`, `matron-climb-board`, `matron-hearing`, `matron-exam-routecraft` +2 more | 19/83 |  |
| 3066-3125 | `AffordanceMap::walk_path` | **T1+O** `matron-climb` · **O** `matron-traverse`, `matron-pursuit`, `matron-ring-descent`, `matron-tower` +3 more · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more | 14/88 |  |
| 3150-3379 | `traversal::grid_walk` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +2 more · **T1+O** `matron-climb-scan` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +1 more · **T1** `matron-exam`, `matron-climb-board`, `matron-hearing`, `matron-exam-routecraft` +4 more | 15/87 |  |

### `src/sim/mechanism_kit.cpp` (2295 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 98-138 | `make_shape` | **T1** ~all (103) | 103/0 |  |
| 147-187 | `Kit::~Kit` | **T1** ~all (103) | 103/0 |  |
| 191-223 | `Kit::add_body` | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-towers` +4 more · **T1** ~all (95) | 103/0 |  |
| 225-250 | `Kit::add_shape_body` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/0 |  |
| 296-324 | `Kit::add_guide` | **T1** ~all (103) | 103/0 |  |
| 748-777 | `Kit::govern` | - | 0/91 | per-tick only |
| 779-869 | `Kit::pre_step` | - | 0/91 | per-tick only |
| 883-915 | `Kit::land_stream` | - | 0/91 | per-tick only |
| 917-953 | `Kit::flow_bins` | - | 0/91 | per-tick only |
| 993-1029 | `Kit::post_step` | - | 0/91 | per-tick only |
| 1209-1248 | `Kit::press` | - | 0/91 | per-tick only |
| 1250-1334 | `Kit::flow_water` | - | 0/91 | per-tick only |
| 1336-1398 | `Kit::flow_air` | - | 0/91 | per-tick only |
| 1449-1516 | `Kit::travel_envelopes` | **T1** ~all (103) | 103/0 |  |
| 1553-1588 | `Kit::connect_rope` | **T1** ~all (103) | 103/0 |  |
| 1670-1707 | `Kit::hook_target` | - | 0/91 | per-tick only |
| 1795-1853 | `Kit::capture` | **T1** ~all (103) | 103/0 |  |
| 1855-1960 | `Kit::restore` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/78 |  |
| 2058-2084 | `Kit::rope_polyline` | **T1** `swing (shadowed second block: never runs)`, `body-order`, `determinism`, `mast` +2 more | 6/0 |  |
| 2086-2124 | `Kit::cable_polyline` | **T1** `swing (shadowed second block: never runs)`, `body-order`, `determinism`, `mast` +2 more | 6/0 |  |

### `src/sim/mechanism_kit.hpp` (940 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/physics_world.cpp` (1431 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 34-410 | `PhysicsWorld::PhysicsWorld` | **T1+O** `stock`, `forklift`, `spreader`, `crane` +10 more · **T1** ~all (89) | 103/0 |  |
| 412-454 | `PhysicsWorld::~PhysicsWorld` | **T1** ~all (103) | 103/0 |  |
| 500-920 | `PhysicsWorld::step` | **O** `stock`, `matron-crash`, `up-throw`, `forklift` +4 more | 0/91 | per-tick only |
| 939-974 | `PhysicsWorld::slingshot_prediction` | - | 0/0 | **no test reaches** |
| 1006-1030 | `PhysicsWorld::add_shape_body` | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-towers` +4 more · **T1** ~all (95) | 103/0 |  |
| 1061-1094 | `PhysicsWorld::body_id_for_entity` | - | 0/91 | per-tick only |
| 1102-1148 | `PhysicsWorld::build_world_solids` | **T1** ~all (103) | 103/0 |  |
| 1197-1269 | `PhysicsWorld::build_stack` | **T1** ~all (103) | 103/0 |  |
| 1278-1305 | `PhysicsWorld::create_constraint` | **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +19 more | 23/68 |  |
| 1320-1349 | `PhysicsWorld::update_support_motion` | - | 0/91 | per-tick only |

### `src/sim/physics_world.hpp` (1243 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/physics_world_climb.cpp` (796 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 27-83 | `PhysicsWorld::find_grip` | **O** `matron-routecraft` · **T1** `up` | 1/90 |  |
| 159-227 | `PhysicsWorld::update_swing` | **O** `matron-routecraft` | 0/91 | per-tick only |
| 249-295 | `PhysicsWorld::route_ahead` | **T1** `up` | 1/0 |  |
| 306-378 | `PhysicsWorld::update_climb` | **O** `matron-routecraft` | 0/91 | per-tick only |
| 382-439 | `PhysicsWorld::move_hands` | - | 0/91 | per-tick only |
| 522-552 | `PhysicsWorld::slide_down_climb` | **O** `matron-routecraft` | 0/91 | per-tick only |
| 557-588 | `PhysicsWorld::probe_lip` | - | 0/91 | per-tick only |
| 593-624 | `PhysicsWorld::update_shimmy` | - | 0/91 | per-tick only |
| 629-653 | `PhysicsWorld::probe_edge_drop` | - | 0/91 | per-tick only |
| 691-767 | `PhysicsWorld::beam_underfoot` | - | 0/91 | per-tick only |
| 769-794 | `PhysicsWorld::clear_traversal` | **T1** `water`, `one-arm`, `matron-arm`, `matron-moves` +19 more | 23/68 |  |

### `src/sim/physics_world_grab.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **hold** row of section 4, then confirm it with this row.
 (556 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 75-106 | `PhysicsWorld::set_player_free_rotation` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/78 |  |
| 195-285 | `PhysicsWorld::grab_pre_step` | - | 0/91 | per-tick only |
| 287-446 | `PhysicsWorld::grab_drive` | **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb` | 0/91 | per-tick only |
| 455-554 | `PhysicsWorld::grab_post_step` | - | 0/91 | per-tick only |

### `src/sim/physics_world_movement.cpp` (1259 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 10-73 | `PhysicsWorld::update_wall_run` | - | 0/91 | per-tick only |
| 77-137 | `PhysicsWorld::try_wall_climb` | - | 0/91 | per-tick only |
| 211-251 | `PhysicsWorld::resolve_up` | - | 0/91 | per-tick only |
| 253-344 | `PhysicsWorld::apply_traversal_commands` | - | 0/91 | per-tick only |
| 480-564 | `PhysicsWorld::record_landing` | - | 0/91 | per-tick only |
| 569-596 | `PhysicsWorld::roll_locomotion` | - | 0/91 | per-tick only |
| 621-650 | `PhysicsWorld::track_support_jolt` | - | 0/91 | per-tick only |
| 654-686 | `PhysicsWorld::apply_wind` | - | 0/91 | per-tick only |
| 750-1107 | `PhysicsWorld::apply_locomotion` | - | 0/91 | per-tick only |
| 1132-1166 | `PhysicsWorld::try_step_up` | - | 0/91 | per-tick only |
| 1218-1257 | `PhysicsWorld::matron_loudness` | - | 0/91 | per-tick only |

### `src/sim/physics_world_probes.cpp` (1212 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 69-96 | `PhysicsWorld::cast_wall` | **T1** `up` | 1/90 |  |
| 156-245 | `PhysicsWorld::probe_ledge` | **T1** `up` | 1/90 |  |
| 250-289 | `PhysicsWorld::probe_vault_landing` | - | 0/91 | per-tick only |
| 337-374 | `PhysicsWorld::carry_candidate` | - | 0/91 | per-tick only |
| 492-562 | `PhysicsWorld::update_carry` | - | 0/91 | per-tick only |
| 600-636 | `PhysicsWorld::throw_velocity` | **T1** `up-throw`, `up`, `fabricated` | 3/88 |  |
| 664-692 | `PhysicsWorld::throw_prediction` | **T1** `up-throw`, `up`, `fabricated` | 3/0 |  |
| 710-737 | `PhysicsWorld::find_rig_action` | - | 0/91 | per-tick only |
| 769-809 | `PhysicsWorld::update_crouch` | - | 0/91 | per-tick only |
| 872-913 | `PhysicsWorld::hook_pre_step` | - | 0/91 | per-tick only |
| 965-1038 | `PhysicsWorld::hook_post_step` | - | 0/91 | per-tick only |
| 1044-1069 | `PhysicsWorld::tear_hook` | - | 0/91 | per-tick only |
| 1115-1142 | `PhysicsWorld::hook_line_blocked` | - | 0/91 | per-tick only |
| 1147-1210 | `PhysicsWorld::bend_hook_line` | - | 0/91 | per-tick only |

### `src/sim/physics_world_state.cpp` (888 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 12-43 | `PhysicsWorld::update_precision_sight` | - | 0/91 | per-tick only |
| 45-92 | `PhysicsWorld::update_affordance` | - | 0/91 | per-tick only |
| 103-187 | `PhysicsWorld::update_up_move` | - | 0/91 | per-tick only |
| 220-309 | `PhysicsWorld::restore_from_checkpoint` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/78 |  |
| 322-363 | `PhysicsWorld::write_save` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/0 |  |
| 368-490 | `PhysicsWorld::read_save` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/0 |  |
| 492-534 | `PhysicsWorld::read_machine_state` | **T1+O** `handling`, `AS-008-wreck`, `AS-008-band`, `AS-009-band` +29 more · **T1** `jumpbuffer`, `edge-stand`, `twin-geometry`, `workers` +66 more | 103/0 |  |
| 536-886 | `PhysicsWorld::read_state` | **T1+O** `edge-stand`, `twin-geometry`, `workers`, `C6` +57 more · **T1** `jumpbuffer`, `handling`, `slingshot`, `trapeze` +38 more | 103/0 |  |

### `src/sim/physics_world_support.hpp` (860 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 360-433 | `PlayerContactListener::observe_support` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 484-612 | `detail::spawn_position` | **T1** ~all (103) | 103/0 |  |

### `src/sim/physics_world_traversal.cpp` (790 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 8-71 | `PhysicsWorld::try_begin_hang` | **O** `matron-routecraft` | 0/91 | per-tick only |
| 107-133 | `PhysicsWorld::try_begin_ground_traversal` | - | 0/91 | per-tick only |
| 153-199 | `PhysicsWorld::mantle_approach_end` | - | 0/91 | per-tick only |
| 201-243 | `PhysicsWorld::begin_mantle` | - | 0/91 | per-tick only |
| 267-381 | `PhysicsWorld::begin_vault` | - | 0/91 | per-tick only |
| 383-479 | `PhysicsWorld::traversal_point` | - | 0/91 | per-tick only |
| 481-512 | `PhysicsWorld::drive_traversal` | - | 0/91 | per-tick only |
| 514-545 | `PhysicsWorld::resolve_traversal_outcome` | - | 0/91 | per-tick only |
| 557-595 | `PhysicsWorld::complete_traversal` | - | 0/91 | per-tick only |

### `src/sim/simulation.cpp` (1692 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 367-402 | `Simulation::wet_state` | **D** `AS-007-band`, `AS-007-route`, `ascent`, `stack` +2 more | 6/0 |  |
| 446-481 | `Simulation::service_state` | **D** `AS-010-route`, `AS-010-n`, `AS-010`, `AS-010-o` +2 more | 6/0 |  |
| 483-513 | `Simulation::stack_state` | **D** `stack` | 1/0 |  |
| 515-631 | `Simulation::step_fixed` | **O** `edge-stand`, `C6`, `landing`, `AS-010-c4` +43 more | 0/91 | per-tick only |
| 633-665 | `Simulation::advance_frame` | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +87 more | 91/0 |  |
| 765-794 | `Simulation::look_target` | **D** `twin-geometry`, `look` | 2/0 |  |
| 818-855 | `Simulation::slingshot_state` | **D** `slingshot` | 1/0 |  |
| 932-973 | `Simulation::forklift_state` | **D** `forklift` · **T1+O** `body-order`, `crane`, `up`, `fabricated` | 5/0 |  |
| 1010-1051 | `Simulation::yard_crane_state` | **D** `spreader`, `crane`, `magnet` | 3/0 |  |
| 1081-1107 | `Simulation::trapeze_state` | **D** `trapeze`, `up` | 2/0 |  |
| 1172-1201 | `Simulation::matron_journey` | **D** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-chase` +3 more | 7/0 |  |
| 1258-1301 | `Simulation::debug_matron_fixture` | **D** `matron-player-independence` | 1/0 |  |
| 1321-1491 | `Simulation::matron_state` | **D** `matron-ladder`, `matron-room`, `matron-collision`, `matron-crash` +25 more | 29/0 |  |
| 1529-1563 | `Simulation::swing_state` | **D** `swing (shadowed second block: never runs)` | 1/0 |  |
| 1662-1690 | `Simulation::lift_state` | **D** `tram`, `pitman`, `helix`, `incline` +5 more | 9/0 |  |

### `src/sim/simulation.hpp` (1924 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/slingshot.cpp` (1073 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 64-191 | `Slingshot::Slingshot` | **T1** ~all (103) | 103/0 |  |
| 217-249 | `Slingshot::create_aim_gimbal` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/78 |  |
| 421-469 | `Slingshot::attach` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/78 |  |
| 536-576 | `Slingshot::refresh_state` | **T1** ~all (103) | 103/0 |  |
| 578-639 | `Slingshot::finish_previous_step` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 641-713 | `Slingshot::pre_step` | - | 0/91 | per-tick only |
| 715-744 | `Slingshot::drive_ratchet` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 746-820 | `Slingshot::OnStep` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 862-887 | `Slingshot::finish_recovery` | - | 0/91 | per-tick only |
| 912-967 | `Slingshot::restore` | **T1** `teeter`, `save`, `twin`, `bay` +9 more | 13/78 |  |
| 969-1005 | `Slingshot::reset` | - | 0/0 | **no test reaches** |
| 1007-1071 | `Slingshot::prediction` | - | 0/0 | **no test reaches** |

### `src/sim/swing.cpp` (595 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 61-150 | `frame_parts` | **T1** ~all (103) | 103/0 |  |
| 268-342 | `Swing::Swing` | **T1** ~all (103) | 103/0 |  |
| 431-459 | `Swing::machine_energy` | **T1** ~all (103) | 103/0 |  |
| 484-531 | `Swing::OnStep` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +10 more | 14/88 |  |
| 533-558 | `Swing::post_step` | - | 0/91 | per-tick only |

### `src/sim/vertical/vertical_route.cpp` (523 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 22-125 | `Route::Route` | **T1** ~all (103) | 103/0 |  |
| 156-186 | `Route::at` | **T1** `tram`, `pitman`, `helix`, `incline` +5 more | 9/82 |  |
| 224-257 | `Route::build_late` | **T1** ~all (103) | 103/0 |  |
| 259-328 | `Route::build_wheel_reload` | **T1** ~all (103) | 103/0 |  |
| 341-461 | `Route::pre_step` | - | 0/91 | per-tick only |
| 463-502 | `Route::pitman_step` | - | 0/91 | per-tick only |

### `src/sim/yard_crane.cpp` (898 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 53-84 | `mast_parts` | **T1** ~all (103) | 103/0 |  |
| 97-165 | `jib_parts` | **T1** ~all (103) | 103/0 |  |
| 302-346 | `dock_parts` | **T1+O** `crane` · **T1** ~all (102) | 103/0 |  |
| 361-451 | `YardCrane::YardCrane` | **T1+O** `crane`, `shared_survival (own exe)` · **T1** ~all (101) | 103/0 |  |
| 484-556 | `YardCrane::magnet_contact` | - | 0/91 | per-tick only |
| 558-584 | `YardCrane::work_magnet` | - | 0/91 | per-tick only |
| 586-640 | `YardCrane::spreader_seat` | - | 0/91 | per-tick only |
| 699-811 | `YardCrane::pre_step` | - | 0/91 | per-tick only |
| 813-858 | `YardCrane::post_step` | **T1** ~all (103) | 103/0 |  |


## 4. Matron subject index (what her tests watch)

Her code is one state machine, so reach alone cannot tell the holds tests from the climbing tests. This table groups her `MatronSnapshot` fields (and the hold/fatality test result structs) into subjects, and lists the groups whose test code reads any field of the subject beyond the ubiquitous ones, most fields first. The source-file column is my reading of each file's header comment, not computed. For a change in her hold code, run the first group here that also shows up in the function's row in section 3.

| Subject | Source files (my reading of their header comments) | Groups whose test code reads a field of that subject (count = distinct fields of that subject read; most first) |
|---|---|---|
| Her hold, arm tears, fatality (she grabs, carries, tears, kills) | matron_hold.cpp, matron_fatality.cpp, physics_world_grab.cpp, physics_world_arm.cpp, physics_world_fatality.cpp, physics_world_one_arm.cpp, matron_grip.hpp | `matron-grab` (92), `matron-fatality` (59), `matron-arm` (37), `matron` (12), `matron-remaining` (10), `matron-mischief` (1), `one-arm` (1), `matron-chase` (1), `strike` (1), `matron-climb-board` (1), `matron-climb` (1), `matron-routecraft` (1) |
| Her climbing (ladders, mantles, found climbs, journeys) | matron_climb.cpp, matron_traversal.cpp, matron_routecraft.cpp, matron_found_climb.cpp, matron_journey.cpp, matron_travel.cpp, matron_contact.cpp | `matron-climb` (44), `matron-climb-scan` (43), `matron` (37), `matron-remaining` (34), `matron-found-climb` (33), `matron-pipe` (32), `matron-tower` (11), `matron-ways` (7), `matron-ladders` (6), `matron-chase` (6), `matron-moves` (5), `matron-ladder` (4), `matron-traverse` (3), `bay` (3), `matron-pursuit` (2), `matron-player-independence` (2), `matron-routecraft` (2), `matron-ring-descent` (1), `matron-climb-board` (1) |
| Her senses and hunt (hearing, sight, lead, fooled, wary) | matron.cpp (state machine, hunt), matron_moves.cpp | `matron` (9), `matron-hearing` (9), `matron-crash` (8), `matron-remaining` (6), `matron-balance` (5), `matron-chase` (5), `matron-room` (3), `matron-moves` (2), `matron-pipe` (1), `matron-arm` (1), `matron-grab` (1) |
| Her balance and the player's blows (strike, stagger, knockdown, rage) | physics_world_strike.cpp, matron.cpp | `strike` (11), `matron-remaining` (8), `matron` (8), `matron-balance` (7), `matron-collision` (4), `matron-chase` (4), `matron-mischief` (2), `matron-pipe` (2), `matron-grab` (2), `matron-stand` (1), `matron-moves` (1), `matron-pursuit` (1), `matron-player-independence` (1) |
| Her corner pipe | matron_pipe.cpp | `matron-pipe` (11), `matron-remaining` (11), `matron` (11), `matron-found-climb` (4), `matron-climb-scan` (4), `matron-climb` (4), `matron-pursuit` (2), `matron-chase` (2), `matron-traverse` (1), `matron-moves` (1), `matron-player-independence` (1), `matron-ring-descent` (1), `matron-ways` (1) |
| Her cable and catwalk (lines, rotten planks) | matron_line.cpp, matron.cpp | `matron` (8), `matron-remaining` (4), `matron-moves` (4), `matron-chase` (2), `matron-player-independence` (1) |
| Her pace and getting unstuck | matron_moves.cpp, matron.cpp | `matron-moves` (10), `matron-chase` (5), `matron-remaining` (2), `matron` (2), `matron-mischief` (1), `matron-balance` (1) |

## 5. Touch scenarios

`godot/presentation/ui/ui_test_driver.gd` has 130 scenarios (`SCENARIOS`), each a `match _scenario` arm calling a driver function in `ui_test_base.gd`, `_moves`, `_rides`, `_chase`, `_act`, `_bands`, `_service`, `_stack` or `_uncanny` (all one script by `extends`). Run one as `godot --headless --fixed-fps 60 --path godot -- --uitest=<scenario>` (CI runs the set in `wo000-delivery-spine.yml`; `look_*` and `perf_*` render only and are not in CI). The `touch_*` ones also run as `<name>_buttons`; `pad_*` and `keyboard_*` inject device events.

Columns: spawn is the id passed to `configure_initial_spawn`; "game functions" are `presentation` functions the scenario's own code calls (file:function, first five); "bridge methods" are native methods it calls (first five); the last column lists the `src/sim` files its bridge calls run without the per-tick step, ignoring files that more than 35% of scenarios reach.

| Scenario | Spawn | Driver | Input | Game functions it calls straight from the scenario (file:function) | Bridge methods it calls (native) | `src/sim` files those run (no per-tick step) |
|---|---|---|---|---|---|---|
| `audio_mix` | 8 | `_audio_mix` | none injected (renders/audio/perf) | audio_director:quiesce | - | - |
| `audio_fall` | 9 | `_audio_fall` | none injected (renders/audio/perf) | audio_director:quiesce | get_death_count | - |
| `look_night_lamps` | 8 | `_look_night_lamps` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour | - | - |
| `perf_grade` | 8 | `_perf_probe` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_shadow_level | - | - |
| `perf_top` | 37 | `_perf_probe` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_shadow_level | - | - |
| `touch_clouds` | 8 | `_clouds` | touch (gestures; `_buttons` too) | cloud_layer:nearest, cloud_layer:puff_count, crew_view:_pose, matron_view:_pose | - | - |
| `pad_core` | 8 | `_pad_core` | gamepad events | - | get_player_linear_velocity, get_player_position, get_tick_index, is_player_crouched, is_player_grounded | - |
| `look_finishes` | 8 | `_look_finishes` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour | get_kit_body_index, get_kit_body_transform, get_loose_stock | - |
| `touch_quickturn` | 8 | `_touch_quickturn` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:is_button_shown, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, is_player_crouched | - |
| `touch_works` | 27 | `_touch_works` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, works:beacon_count, works:lit_count, works:steaming_at +1 | - | - |
| `keyboard_core` | 8 | `_keyboard_core` | keyboard/mouse events | crew_view:_pose, matron_view:_pose | get_player_linear_velocity, get_player_position, get_rejected_traversal_count, is_player_crouched, is_player_grounded | - |
| `look_matron_gallery` | 11 | `_look_matron_gallery` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, matron_view:preview, sky_cycle:set_hour | get_matron_state, get_player_position | matron.cpp, matron_traversal.cpp |
| `touch_body` | 0 | `_touch_body` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_body:nearest_to, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +1 | is_player_crouched | - |
| `touch_move_look` | 8 | `_touch_move_look` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, get_player_position, is_player_grounded | - |
| `touch_workers` | 8 | `_touch_workers` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:draw_bounds, crew_view:figures, matron_view:_pose, touch_controls:stick_home | get_player_position, get_worker_counts | - |
| `touch_flow` | 8 | `_touch_flow` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, is_player_crouched, is_player_grounded | - |
| `touch_gyro_aim` | 8 | `_touch_gyro_aim` | touch (gestures; `_buttons` too) | crew_view:_pose, main:_apply_settings, main:uncanny_profile_for_start, matron_view:_pose, touch_controls:stick_home | configure_initial_spawn, get_player_position, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +45 |
| `touch_lethal_feedback` | 9 | `_touch_lethal_feedback` | touch (gestures; `_buttons` too) | corpse_view:splashes_shown, crew_view:_pose, hud:gauge_word, matron_view:_pose | get_corpses, get_death_count, get_lethal_impact_speed_mps, get_player_position | corpse.cpp |
| `touch_room_hearing` | 11 | `_touch_room_hearing` | touch (gestures; `_buttons` too) | main:_apply_settings, main:uncanny_profile_for_start, touch_controls:stick_home, voice_keeper:feed_samples, voice_keeper:room_loudness | configure_initial_spawn, get_matron_state, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +46 |
| `touch_title` | 8 | `_title` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, title_screen:button_center, title_screen:button_text, title_screen:has_continue +1 | get_player_position, get_tick_index | - |
| `fall_voice` | 10 | `_fall_voice` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_death_count, get_landing_state, get_last_impact_speed_mps, get_player_linear_velocity, get_support_point_linear_velocity +1 | - |
| `touch_birds` | 8 | `_touch_birds` | touch (gestures; `_buttons` too) | birds:flying_count, birds:highest_above, birds:nearest_perched_flock, birds:perched_count, crew_view:_pose +2 | get_player_position, get_surface_below | physics_world.cpp, physics_world_probes.cpp |
| `touch_double_tap_vault` | 3 | `_touch_double_tap_vault` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:gesture_hint, touch_controls:stick_home, touch_controls:swipe_units | get_accepted_traversal_count, get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_jump` | 8 | `_touch_jump` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:is_button_shown, touch_controls:stick_home +1 | get_player_linear_velocity, is_player_crouched, is_player_grounded | - |
| `touch_radio` | 8 | `_touch_radio` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `touch_slide` | 0 | `_touch_slide` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, get_player_position, is_player_crouched, is_player_sliding | - |
| `touch_speed_vault` | 3 | `_touch_speed_vault` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, get_player_position, get_traversal_state, is_player_crouched | - |
| `touch_teeter` | 26 | `_teeter` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `touch_vault` | 3 | `_touch_vault` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_accepted_traversal_count, get_player_position, get_traversal_state, is_player_crouched | - |
| `look_her_chart` | 11 | `_look_her_chart` | none injected (renders/audio/perf) | chart_pages:lay_page, chart_pages:page_sheet, crew_view:_pose, her_chart:date_line, matron_view:_pose +3 | get_player_position, get_surface_below | physics_world.cpp, physics_world_probes.cpp |
| `look_stock_yard` | 8 | `_look_stock_yard` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `look_yard_crane` | 8 | `_look_yard_crane` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `touch_crouch` | 0 | `_touch_crouch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +3 | get_player_position, is_player_crouched | - |
| `touch_route_hint` | 11 | `_touch_route_hint` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_route_ahead +2 | physics_world_climb.cpp, physics_world_probes.cpp |
| `touch_swing_launch` | 39 | `_touch_swing_launch` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, get_player_position, get_swing_launch_count, get_traversal_left_hand, get_traversal_state +2 | - |
| `touch_chute` | 9 | `_touch_chute` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:risers_visible, matron_view:_pose, touch_controls:button_center +1 | get_death_count, get_last_impact_speed_mps, get_player_linear_velocity, is_parachute_deployed, is_player_grounded | - |
| `touch_wall_run` | 41 | `_touch_wall_run` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_position, get_player_wall_normal, get_wall_step_count, is_player_crouched, is_player_grounded +1 | - |
| `keyboard_crane_magnet` | 8 | `_crane_magnet` | keyboard/mouse events | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_player_linear_velocity +4 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `look_bay` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_catwalk` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_hunt` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_locker` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_stock` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_tower_stock_176` | 14 | `_look_tower_stock` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_player_linear_velocity +3 | - |
| `look_tower_stock_deck2` | 25 | `_look_tower_stock` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_player_linear_velocity +3 | - |
| `pad_crane_magnet` | 8 | `_crane_magnet` | gamepad events | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_player_linear_velocity +4 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_crane_magnet` | 8 | `_crane_magnet` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_player_linear_velocity +4 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_ladder` | 11 | `_touch_ladder` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +4 | - |
| `touch_pause` | 8 | `_touch_pause` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, pause_menu:current_page, pause_menu:page_first_center, pause_menu:setting_center +5 | configure_initial_spawn, get_tick_index | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +45 |
| `touch_precision` | 3 | `_touch_precision` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, get_player_position, get_precision_jump_count, get_precision_landing_count, get_support_point_linear_velocity +2 | - |
| `touch_voice_echo` | 11 | `_touch_voice_echo` | touch (gestures; `_buttons` too) | main:_apply_settings, main:uncanny_profile_for_start, touch_controls:stick_home, uncanny:advance, voice_keeper:_enforce_caps +4 | configure_initial_spawn, get_player_position, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +45 |
| `touch_climb` | 4 | `_touch_climb` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:planted_count, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_hook` | 39 | `_touch_hook` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_hook_state, get_player_position, is_player_crouched, is_player_grounded | - |
| `touch_springboard` | 8 | `_touch_springboard` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_springboard_count +3 | - |
| `touch_stock` | 8 | `_stock_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_player_linear_velocity +3 | - |
| `touch_tuck` | 0 | `_touch_tuck` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +3 | - |
| `look_summit` | 42 | `_look_summit` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour, summit_view:beam_world_direction, summit_view:lamp_world_position +1 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_summit_state +3 | - |
| `look_summit_ground` | 8 | `_look_summit` | none injected (renders/audio/perf) | crew_view:_pose, matron_view:_pose, sky_cycle:set_hour, summit_view:beam_world_direction, summit_view:lamp_world_position +1 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_summit_state +3 | - |
| `touch_arm_break` | 11 | `_touch_arm_break` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_death_state, get_landing_state, get_player_linear_velocity +4 | - |
| `touch_checkpoint` | 26 | `_checkpoint_continuation` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_carrying_entity_id, get_checkpoint_position, get_death_count, get_landing_state, get_player_linear_velocity +6 | - |
| `touch_her_chart` | 11 | `_touch_her_chart` | touch (gestures; `_buttons` too) | chart_pages:lay_page, chart_pages:page_sheet, chart_pages:page_text, her_chart:date_line, her_chart:hhmm +4 | get_kit_body_entity_id, get_kit_body_index, get_kit_body_transform, get_loose_stock, get_player_position | - |
| `touch_hook_rope` | 16 | `_touch_hook_rope` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_hook_state, get_landing_state, get_player_linear_velocity +4 | - |
| `touch_matron` | 11 | `_matron_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +4 | matron.cpp, matron_traversal.cpp |
| `touch_matron_climb` | 11 | `_matron_climb_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +4 | matron.cpp, matron_traversal.cpp |
| `touch_trapeze` | 8 | `_trapeze` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +4 | trapeze.cpp |
| `touch_bay` | 11 | `_bay_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_bay_state, get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state +5 | bay.cpp, matron.cpp, matron_traversal.cpp |
| `touch_crane_spreader` | 8 | `_crane_spreader` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_loose_stock +5 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_her_chart_fast` | 11 | `_touch_her_chart_fast` | touch (gestures; `_buttons` too) | chart_pages:page_text, consent_screen:button_center, her_chart:compose, main:uncanny_profile_for_start, story_radio_player:configure +5 | get_kit_body_entity_id, get_kit_body_index, get_kit_body_transform, get_loose_stock, get_player_position | - |
| `touch_matron_heave` | 11 | `_matron_heave_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:gesture_hint, touch_controls:stick_home +1 | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +4 | matron.cpp, matron_traversal.cpp |
| `touch_mic_consent` | 8 | `_touch_mic_consent` | touch (gestures; `_buttons` too) | consent_screen:button_center, crew_view:_pose, main:_apply_settings, main:_show_consent, main:uncanny_profile_for_start +7 | configure_initial_spawn, get_tick_index, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +45 |
| `touch_pipe_gallery` | 40 | `_touch_pipe_gallery` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:view_fault, matron_view:_pose +5 | get_player_position, get_traversal_left_hand, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_swing` | 16 | `_swing` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +5 | swing.cpp |
| `touch_wall_climb` | 0 | `_touch_wall_climb` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +3 | - |
| `touch_wall_kick` | 41 | `_touch_wall_kick` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:gesture_hint, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +6 | - |
| `touch_yard_crane` | 8 | `_yard_crane_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +3 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +2 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_forklift` | 8 | `_forklift_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:stick_home +1 | get_carrying_entity_id, get_forklift_state, get_kit_body_index, get_kit_body_transform, get_landing_state +5 | forklift.cpp, physics_world.cpp |
| `touch_forklift_strain` | 8 | `_forklift_strain_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:stick_home +1 | get_carrying_entity_id, get_forklift_state, get_kit_body_index, get_kit_body_transform, get_landing_state +5 | forklift.cpp, physics_world.cpp |
| `touch_matron_ride` | 11 | `_matron_ride_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +6 | matron.cpp, matron_traversal.cpp |
| `touch_hang_climb` | 5 | `_touch_hang` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:planted_count, first_person_arms:view_fault +7 | get_player_linear_velocity, get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_hang_drop` | 5 | `_touch_hang` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:planted_count, first_person_arms:view_fault +7 | get_player_linear_velocity, get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_matron_line` | 11 | `_matron_line_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_death_state, get_landing_state, get_matron_state +7 | matron.cpp, matron_traversal.cpp, zipline.cpp |
| `touch_monkey_bars` | 39 | `_touch_monkey_bars` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:view_fault, matron_view:_pose +6 | get_player_linear_velocity, get_player_position, get_traversal_left_hand, get_traversal_state, is_player_crouched +1 | - |
| `touch_roll` | 8 | `_roll` | touch (gestures; `_buttons` too) | crew_view:_pose, main:_apply_settings, main:uncanny_profile_for_start, matron_view:_pose, touch_controls:button_center +2 | configure_initial_spawn, get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity +5 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +45 |
| `touch_roll_reduced` | 8 | `_roll` | touch (gestures; `_buttons` too) | crew_view:_pose, main:_apply_settings, main:uncanny_profile_for_start, matron_view:_pose, touch_controls:button_center +2 | configure_initial_spawn, get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity +5 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +45 |
| `touch_tram` | 27 | `_tram` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +9 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_twin` | 16 | `_twin` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_hook_state, get_kit_body_index, get_kit_body_transform +7 | - |
| `touch_zipline_slam` | 25 | `_touch_zipline_slam` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_corpses, get_death_count, get_death_state, get_landing_state +7 | corpse.cpp, zipline.cpp |
| `keyboard_boom` | 12 | `_boom` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `keyboard_rig` | 11 | `_rig` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `pad_boom` | 12 | `_boom` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `pad_rig` | 11 | `_rig` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `touch_boom` | 12 | `_boom` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `touch_incline` | 8 | `_incline` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:stick_home | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +10 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_rig` | 11 | `_rig` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `touch_save` | 26 | `_save` | touch (gestures; `_buttons` too) | consent_screen:close, consent_screen:open, main:_save_climb, main:discard_saved_climb, main:load_saved_climb +5 | get_carrying_entity_id, get_checkpoint_position, get_death_count, get_landing_state, get_player_linear_velocity +3 | - |
| `touch_wheel` | 25 | `_wheel` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +8 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `keyboard_debris` | 13 | `_debris` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +7 | - |
| `pad_debris` | 13 | `_debris` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +7 | - |
| `touch_debris` | 13 | `_debris` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +7 | - |
| `touch_machines` | 36 | `_machines` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +1 | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +8 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_slingshot` | 8 | `_slingshot` | touch (gestures; `_buttons` too) | canopy_view:open_fraction, crew_view:_pose, launch_shot:active, launch_shot:look_weight, launch_shot:time_scale +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +5 | - |
| `touch_summit` | 42 | `_summit` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +3 | get_carrying_entity_id, get_death_count, get_landing_state, get_last_impact_speed_mps, get_player_linear_velocity +6 | - |
| `touch_c5` | 33 | `_c5` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `touch_c6` | 35 | `_c6` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `keyboard_service` | 30 | `_service` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `pad_service` | 30 | `_service` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_c4` | 31 | `_c4` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +3 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `touch_c7` | 36 | `_c7` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +9 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_high` | 37 | `_high` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +1 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +10 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_n` | 32 | `_n` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_o` | 34 | `_o` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_service` | 30 | `_service` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `keyboard_shop` | 19 | `_shop` | keyboard/mouse events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose +3 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `pad_shop` | 19 | `_shop` | gamepad events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose +3 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_shop` | 19 | `_shop` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose +3 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_zipline` | 25 | `_touch_zipline` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center +5 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +7 | zipline.cpp |
| `keyboard_crane` | 22 | `_crane` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +11 | - |
| `keyboard_wet` | 16 | `_wet` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +11 | - |
| `pad_crane` | 22 | `_crane` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +11 | - |
| `pad_wet` | 16 | `_wet` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +11 | - |
| `touch_crane` | 22 | `_crane` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +11 | - |
| `touch_wet` | 16 | `_wet` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, matron_view:_pose, touch_controls:button_center +2 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +11 | - |
| `touch_matron_pipe` | 11 | `_matron_pipe_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, matron_view:head_world_position, touch_controls:button_center, touch_controls:gesture_hint +2 | get_carry_target_entity_id, get_carrying_entity_id, get_death_count, get_death_state, get_kit_body_index +12 | matron.cpp, matron_traversal.cpp, zipline.cpp |
| `keyboard_stack_upper` | 26 | `_stack_upper` | keyboard/mouse events | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_loose_stock, get_player_linear_velocity +13 | - |
| `pad_stack_upper` | 26 | `_stack_upper` | gamepad events | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_loose_stock, get_player_linear_velocity +13 | - |
| `touch_stack_upper` | 26 | `_stack_upper` | touch (gestures; `_buttons` too) | crew_view:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label, touch_controls:gesture_hint +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_loose_stock, get_player_linear_velocity +13 | - |
| `touch_matron_act` | 11 | `_matron_act_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, gore_victim:held_ankle_drawn, her_takes:in_ring, matron_hold_camera:black, matron_hold_camera:since_let_go +9 | get_carrying_entity_id, get_death_count, get_death_state, get_kit_body_index, get_kit_body_transform +12 | matron.cpp, matron_traversal.cpp, zipline.cpp |
| `keyboard_stack` | 8 | `_stack` | keyboard/mouse events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center +5 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +35 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `pad_stack` | 8 | `_stack` | gamepad events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center +5 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +35 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_stack` | 8 | `_stack` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, matron_view:_pose, touch_controls:button_center +5 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +35 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |

## 6. Which `src/sim` files the touch scenarios run

**N**: the scenario calls a bridge method whose body runs the file (cut at the per-tick step). **O**: the scenario reads a bridge value the file's code writes, so the file runs in its ticks. Every scenario that advances the world also ticks everything; files with neither column set are reached by ticks and world building alone (`tick only`).

| `src/sim` file | **N**: scenarios whose bridge calls run it | **O**: scenarios that read a bridge value this file writes (it runs in their ticks) | N/O counts |
|---|---|---|---|
| `src/sim/band_counterweight_well.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/band_facade_crane.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +7 more | 7/13 |
| `src/sim/band_midstack_service.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +9 more | 7/15 |
| `src/sim/band_plate_shop.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +7 more | 7/13 |
| `src/sim/band_stack.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_checkpoint`, `touch_mic_consent` +8 more | 7/14 |
| `src/sim/band_wet_isolation.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +7 more | 7/13 |
| `src/sim/bay.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_bay`, `touch_mic_consent` +2 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `look_bay`, `look_matron_catwalk`, `look_matron_hunt` +15 more | 8/21 |
| `src/sim/climb_c6.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/climb_c7.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/corpse.cpp` | `touch_gyro_aim`, `touch_lethal_feedback`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent` +3 more | `touch_gyro_aim`, `touch_lethal_feedback`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent` +3 more | 9/9 |
| `src/sim/crew.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `look_bay`, `look_matron_catwalk`, `look_matron_hunt` +15 more | 7/21 |
| `src/sim/fabricated.cpp` | - | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 0/7 |
| `src/sim/forklift.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_pause` +8 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `keyboard_crane_magnet` +11 more | 14/17 |
| `src/sim/impacts.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/loose_stock.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `look_finishes`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +19 more | 7/25 |
| `src/sim/matron.cpp` | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `look_bay`, `look_matron_catwalk`, `look_matron_hunt` +15 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +20 more | 21/26 |
| `src/sim/matron_climb.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/matron_contact.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +18 more | 7/24 |
| `src/sim/matron_fatality.cpp` | - | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 0/7 |
| `src/sim/matron_found_climb.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | 7/10 |
| `src/sim/matron_hold.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | 7/10 |
| `src/sim/matron_journey.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_route_hint`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent` +2 more | 7/8 |
| `src/sim/matron_line.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +22 more | 7/28 |
| `src/sim/matron_moves.cpp` | - | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | 0/10 |
| `src/sim/matron_pipe.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +18 more | 7/24 |
| `src/sim/matron_routecraft.cpp` | - | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | 0/10 |
| `src/sim/matron_travel.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/matron_traversal.cpp` | `look_matron_gallery`, `touch_room_hearing`, `look_bay`, `look_matron_catwalk`, `look_matron_hunt`, `look_matron_locker` +9 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | 15/10 |
| `src/sim/mechanism_kit.cpp` | `look_finishes`, `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `look_tower_stock_176`, `look_tower_stock_deck2` +46 more | `look_finishes`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +49 more | 52/55 |
| `src/sim/physics_world.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `keyboard_crane_magnet`, `pad_crane_magnet` +10 more | `look_finishes`, `look_matron_gallery`, `touch_gyro_aim`, `touch_lethal_feedback`, `touch_room_hearing`, `touch_birds` +68 more | 16/74 |
| `src/sim/physics_world_arm.cpp` | - | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +25 more | 0/31 |
| `src/sim/physics_world_climb.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_route_hint`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent` +2 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `keyboard_crane_magnet` +25 more | 8/31 |
| `src/sim/physics_world_fabricated.cpp` | - | `look_finishes`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +43 more | 0/49 |
| `src/sim/physics_world_fatality.cpp` | - | `look_finishes`, `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart` +59 more | 0/65 |
| `src/sim/physics_world_grab.cpp` | - | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +34 more | 0/40 |
| `src/sim/physics_world_movement.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `keyboard_crane_magnet` +20 more | 7/26 |
| `src/sim/physics_world_one_arm.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `keyboard_crane_magnet` +20 more | 7/26 |
| `src/sim/physics_world_probes.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | `look_finishes`, `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart` +59 more | 10/65 |
| `src/sim/physics_world_state.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | most scenarios (104) | 7/104 |
| `src/sim/physics_world_strike.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `look_matron_gallery`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +37 more | 7/43 |
| `src/sim/physics_world_traversal.cpp` | - | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `keyboard_crane_magnet` +25 more | 0/31 |
| `src/sim/physics_world_water.cpp` | - | `look_finishes`, `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint` +47 more | 0/53 |
| `src/sim/pipe_gallery.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/rope_constraint.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/shared_survival.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_pause` +4 more | 7/10 |
| `src/sim/simulation.cpp` | every scenario that reads state (124 of 130) | `pad_core`, `look_finishes`, `look_matron_gallery`, `touch_gyro_aim`, `touch_lethal_feedback`, `touch_room_hearing` +84 more | 124/90 |
| `src/sim/slingshot.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_pause` +9 more | 7/15 |
| `src/sim/slingshot_model.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/summit.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `look_summit`, `look_summit_ground` +4 more | 7/10 |
| `src/sim/swing.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_swing` +2 more | `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_pause` +9 more | 8/15 |
| `src/sim/trapeze.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_trapeze`, `touch_mic_consent` +2 more | `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_pause` +8 more | 8/14 |
| `src/sim/twin_tower.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/vertical/barrel_helix.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/cascade_mast.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/crown_gondola.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/gravity_balance.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/luffing_derrick.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/pitman_lift.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/vertical/rack_climber.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/vertical/slab_incline.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/stone_wheel.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 7/16 |
| `src/sim/vertical/traction_tram.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +1 more | 7/7 |
| `src/sim/vertical/vertical_machine.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 16/16 |
| `src/sim/vertical/vertical_route.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll` +10 more | 16/16 |
| `src/sim/yard_crane.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_pause` +6 more | `touch_gyro_aim`, `touch_room_hearing`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_pause` +6 more | 12/12 |
| `src/sim/zipline.cpp` | `touch_gyro_aim`, `touch_room_hearing`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent`, `touch_matron_line` +6 more | `touch_gyro_aim`, `touch_room_hearing`, `touch_route_hint`, `touch_pause`, `touch_voice_echo`, `touch_mic_consent` +9 more | 12/15 |

## 7. Presentation files

`Per-frame core` counts the file's functions reachable from engine callbacks (`_ready`, `_process`, `_input`, ...) of the game scripts: they run in every scenario. `Scenarios that call into it` are the scenarios whose own code (or the chain of game functions it starts) calls a function in the file that is not in the per-frame core. `no scenario or test reaches it` means no static call path from a scenario or an engine callback; signals and scene wiring are invisible to this, so check `grep -rn <file stem> godot/` before treating it as dead.

| File | Lines | Functions run by a scenario | Per-frame core | Scenarios that call into it (narrowest first) | Headless tests | Verdict |
|---|---|---|---|---|---|---|
| `audio/audio_director.gd` | 1817 | 66 | 62/66 | `audio_mix`, `audio_fall` | footstep_contact_test.gd | scenario-specific |
| `audio/her_takes.gd` | 185 | 12 | 12/12 | `touch_matron_act` | her_takes_test.gd | scenario-specific |
| `audio/matron_sound.gd` | 483 | 9 | 9/9 | - | - | per-frame core only (every scenario) |
| `audio/sound_bank.gd` | 1659 | 91 | 1/91 | - | - | per-frame core only (every scenario) |
| `audio/story_radio_player.gd` | 275 | 22 | 9/22 | `touch_her_chart_fast` | - | scenario-specific |
| `audio/voice_lines.gd` | 137 | 0 | 0/0 | - | - | no scenario or test reaches it |
| `canopy_view.gd` | 154 | 6 | 5/6 | `touch_slingshot` | - | scenario-specific |
| `cloud_layer.gd` | 169 | 6 | 4/6 | `touch_clouds` | - | scenario-specific |
| `corpse_view.gd` | 224 | 8 | 7/8 | `touch_lethal_feedback` | - | scenario-specific |
| `crane_view.gd` | 108 | 4 | 4/4 | - | - | per-frame core only (every scenario) |
| `fabricated_view.gd` | 118 | 8 | 3/8 | - | - | per-frame core only (every scenario) |
| `first_person_arms.gd` | 951 | 34 | 27/34 | `touch_chute`, `touch_climb`, `touch_hook`, `touch_pipe_gallery`, `touch_yard_crane`, `touch_hang_climb` +31 more | - | scenario-specific |
| `first_person_body.gd` | 372 | 8 | 7/8 | `touch_body` | - | scenario-specific |
| `floating_audit.gd` | 622 | 31 | 31/31 | - | - | per-frame core only (every scenario) |
| `forklift_view.gd` | 169 | 5 | 5/5 | - | - | per-frame core only (every scenario) |
| `gait.gd` | 40 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `identity/photo_id_intake.gd` | 62 | 5 | 5/5 | - | - | per-frame core only (every scenario) |
| `identity/player_identity_store.gd` | 320 | 22 | 19/22 | - | - | per-frame core only (every scenario) |
| `identity/portrait_calibration.gd` | 92 | 8 | 5/8 | - | - | per-frame core only (every scenario) |
| `identity/portrait_face_fit.gd` | 188 | 10 | 0/10 | - | - | only called from code no scenario reaches |
| `identity/work_id_page.gd` | 111 | 10 | 9/10 | `touch_her_chart_fast` | - | scenario-specific |
| `launch_shot.gd` | 77 | 5 | 5/5 | `touch_slingshot` | - | scenario-specific |
| `life/birds.gd` | 523 | 23 | 19/23 | `touch_birds` | - | scenario-specific |
| `life/climber/climber_body_rig.gd` | 18 | 0 | 0/0 | - | - | no scenario or test reaches it |
| `life/crew_view.gd` | 501 | 22 | 19/22 | `look_night_lamps`, `perf_grade`, `perf_top`, `touch_clouds`, `look_finishes`, `touch_works` +108 more | - | core |
| `life/foreman_radio.gd` | 91 | 4 | 4/4 | - | - | per-frame core only (every scenario) |
| `life/gore/blood_fx.gd` | 263 | 10 | 10/10 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_body.gd` | 112 | 8 | 8/8 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_kit.gd` | 749 | 13 | 13/13 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_limb.gd` | 70 | 3 | 2/3 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_victim.gd` | 305 | 15 | 14/15 | `touch_matron_act` | - | scenario-specific |
| `life/gore/gore_view.gd` | 364 | 12 | 11/12 | - | gore_shots_test.gd | per-frame core only (every scenario) |
| `life/gore/gore_wound.gd` | 206 | 7 | 5/7 | - | - | per-frame core only (every scenario) |
| `life/matron_hold_camera.gd` | 469 | 14 | 10/14 | `touch_matron_act` | - | scenario-specific |
| `life/matron_look.gd` | 246 | 8 | 8/8 | - | matron_figure_test.gd | per-frame core only (every scenario) |
| `life/matron_rig.gd` | 803 | 31 | 28/31 | `touch_matron_act` | - | scenario-specific |
| `life/matron_view.gd` | 2320 | 53 | 50/53 | `look_night_lamps`, `perf_grade`, `perf_top`, `touch_clouds`, `look_finishes`, `touch_works` +108 more | gore_shots_test.gd, matron_figure_test.gd, matron_perception_climb_test.gd, matron_presentation_test.gd | core |
| `life/strike_feel.gd` | 101 | 4 | 4/4 | - | - | per-frame core only (every scenario) |
| `life/works.gd` | 272 | 14 | 10/14 | `touch_works` | - | scenario-specific |
| `main.gd` | 1214 | 36 | 35/36 | `touch_gyro_aim`, `touch_room_hearing`, `touch_voice_echo`, `touch_her_chart_fast`, `touch_mic_consent`, `touch_roll` +2 more | - | scenario-specific |
| `main_context.gd` | 623 | 9 | 9/9 | - | - | per-frame core only (every scenario) |
| `main_feel.gd` | 1504 | 31 | 30/31 | - | - | per-frame core only (every scenario) |
| `main_geometry.gd` | 725 | 27 | 27/27 | - | - | per-frame core only (every scenario) |
| `main_machines.gd` | 1134 | 30 | 30/30 | - | - | per-frame core only (every scenario) |
| `main_world.gd` | 908 | 21 | 21/21 | - | - | per-frame core only (every scenario) |
| `mountain_mesh.gd` | 198 | 5 | 5/5 | - | - | per-frame core only (every scenario) |
| `sky_cycle.gd` | 159 | 7 | 7/7 | `look_night_lamps`, `perf_grade`, `perf_top`, `look_finishes`, `look_matron_gallery`, `look_stock_yard` +10 more | - | scenario-specific |
| `slingshot_view.gd` | 227 | 14 | 14/14 | - | - | per-frame core only (every scenario) |
| `solid_export.gd` | 297 | 11 | 11/11 | - | - | per-frame core only (every scenario) |
| `stock_view.gd` | 115 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `summit_view.gd` | 282 | 9 | 7/9 | `look_summit`, `look_summit_ground` | - | scenario-specific |
| `swing_view.gd` | 144 | 6 | 6/6 | - | - | per-frame core only (every scenario) |
| `throw_arc_view.gd` | 86 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `ui/consent_screen.gd` | 119 | 7 | 6/7 | `touch_her_chart_fast`, `touch_mic_consent`, `touch_save` | - | scenario-specific |
| `ui/hud.gd` | 563 | 22 | 22/22 | `touch_lethal_feedback` | - | scenario-specific |
| `ui/input_router.gd` | 368 | 17 | 17/17 | - | - | per-frame core only (every scenario) |
| `ui/licences.gd` | 161 | 1 | 1/1 | - | - | per-frame core only (every scenario) |
| `ui/pause_menu.gd` | 735 | 38 | 31/38 | `touch_pause`, `touch_mic_consent`, `touch_save` | - | scenario-specific |
| `ui/settings_store.gd` | 211 | 6 | 6/6 | `touch_pause` | - | scenario-specific |
| `ui/title_screen.gd` | 168 | 10 | 7/10 | `touch_title`, `touch_save` | - | scenario-specific |
| `ui/touch_controls.gd` | 793 | 32 | 26/32 | `touch_quickturn`, `touch_body`, `touch_move_look`, `touch_workers`, `touch_flow`, `touch_gyro_aim` +109 more | - | core |
| `ui/ui_style.gd` | 481 | 25 | 24/25 | - | - | per-frame core only (every scenario) |
| `uncanny/attention.gd` | 184 | 6 | 5/6 | - | - | per-frame core only (every scenario) |
| `uncanny/chart_pages.gd` | 376 | 18 | 18/18 | `look_her_chart`, `touch_her_chart`, `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/her_chart.gd` | 455 | 27 | 18/27 | `look_her_chart`, `touch_her_chart`, `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/page_reader.gd` | 99 | 6 | 5/6 | `look_her_chart`, `touch_her_chart` | - | scenario-specific |
| `uncanny/tower_clocks.gd` | 186 | 8 | 7/8 | `touch_her_chart` | - | scenario-specific |
| `uncanny/uncanny.gd` | 307 | 14 | 13/14 | `look_her_chart`, `touch_voice_echo`, `touch_her_chart`, `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/uncanny_profile.gd` | 52 | 1 | 1/1 | `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/voice_keeper.gd` | 685 | 31 | 26/31 | `touch_room_hearing`, `touch_voice_echo`, `touch_mic_consent` | - | scenario-specific |
| `water_feel.gd` | 123 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `yard_ground.gd` | 107 | 3 | 1/3 | - | - | per-frame core only (every scenario) |
| `zipline_view.gd` | 184 | 7 | 6/7 | - | - | per-frame core only (every scenario) |

## 8. Big presentation files, function by function

### `audio/audio_director.gd` (1817 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 225-311 | `_ready` | - | yes |
| 345-371 | `_on_bank_ready` | - | no |
| 427-489 | `update` | - | yes |
| 490-519 | `_update_breath` | - | yes |
| 520-562 | `update_machines` | - | yes |
| 582-617 | `update_impacts` | - | yes |
| 624-654 | `_update_air` | - | yes |
| 655-702 | `_update_voice` | - | yes |
| 711-743 | `_voice_say` | - | yes |
| 856-890 | `worker_weld` | - | yes |
| 909-995 | `update_forklift` | - | yes |
| 1004-1055 | `update_crane` | - | yes |
| 1056-1083 | `crane_cue` | - | yes |
| 1249-1317 | `_her_cue` | - | yes |
| 1389-1459 | `matron_layers` | - | yes |
| 1469-1505 | `_her_level` | - | yes |
| 1679-1728 | `update_bay` | - | yes |
| 1743-1778 | `update_wet_e` | - | yes |

### `audio/sound_bank.gd` (1659 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 33-131 | `build` | - | no |
| 143-173 | `build_her` | - | no |
| 371-396 | `_drip` | - | no |
| 413-438 | `_creak` | - | no |
| 782-846 | `_matron_wake` | - | no |
| 878-907 | `_her_bed` | - | no |
| 1357-1382 | `_resonate` | - | no |
| 1387-1414 | `_wav` | - | no |
| 1434-1473 | `_glottal` | - | no |
| 1474-1515 | `_formants` | - | no |
| 1571-1598 | `_add_glide` | - | no |

### `first_person_arms.gd` (951 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 219-278 | `_build_hand` | - | yes |
| 302-338 | `_build_remote` | - | yes |
| 370-552 | `update_arms` | - | yes |
| 591-725 | `_pose_target` | - | yes |
| 764-789 | `_solve_arm` | - | yes |
| 921-952 | `_strike_target` | - | yes |

### `floating_audit.gd` (622 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 106-160 | `run` | - | yes |
| 161-186 | `_text_off_boards` | - | yes |
| 187-216 | `_board_of` | - | yes |
| 264-291 | `_gather_kit` | - | yes |
| 292-325 | `_gather_views` | - | yes |
| 351-377 | `_add` | - | yes |
| 447-489 | `_overlap` | - | yes |
| 527-554 | `_settle` | - | yes |
| 587-618 | `_report` | - | yes |

### `life/gore/gore_kit.gd` (749 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 409-447 | `material` | - | yes |
| 513-549 | `_skin_rim` | - | yes |
| 550-625 | `_meat` | - | yes |
| 626-679 | `_bone` | - | yes |
| 680-736 | `ribcage` | - | yes |

### `life/matron_rig.gd` (803 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 59-85 | `setup` | - | yes |
| 103-141 | `_place_pivots` | - | yes |
| 155-203 | `apply` | - | yes |
| 335-387 | `_reach_chain` | - | yes |
| 412-484 | `_plant_limbs` | - | yes |
| 485-510 | `hold_reach` | - | yes |
| 520-560 | `govern` | - | yes |
| 561-704 | `_contact` | - | yes |
| 740-797 | `step_hair` | - | yes |

### `life/matron_view.gd` (2320 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 273-302 | `_build_sparks` | - | yes |
| 363-459 | `_build_locker` | - | yes |
| 470-527 | `_update_locker` | - | yes |
| 650-704 | `_build_pivots` | - | yes |
| 705-730 | `_load_body` | - | yes |
| 755-786 | `_fours_gate` | - | yes |
| 787-836 | `_hold_gate` | - | yes |
| 858-899 | `_turn_yaw` | - | yes |
| 960-1011 | `_ease_pose` | - | yes |
| 1035-1061 | `_ease_hands` | - | yes |
| 1062-1087 | `_step_bursts` | - | yes |
| 1117-2054 | `_pose` | `look_night_lamps`, `perf_grade`, `perf_top`, `touch_clouds`, `look_finishes`, `touch_works` +108 more | yes |
| 2055-2083 | `_topple` | - | yes |
| 2093-2191 | `_update_figure` | - | yes |

### `main.gd` (1214 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 126-311 | `_ready` | - | yes |
| 337-536 | `_process` | - | yes |
| 550-636 | `_build_interface` | - | yes |
| 637-673 | `_apply_settings` | `touch_gyro_aim`, `touch_room_hearing`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll`, `touch_roll_reduced` | yes |
| 900-1007 | `_dispatch` | - | yes |
| 1099-1145 | `_perform_action` | - | yes |

### `main_context.gd` (623 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 124-456 | `_read_context` | - | yes |
| 489-552 | `_carry_name` | - | yes |
| 582-624 | `_kit_anchor_name` | - | yes |

### `main_feel.gd` (1504 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 271-306 | `_haptic` | - | yes |
| 318-413 | `_update_body_feedback` | - | yes |
| 414-494 | `_update_feedback` | - | yes |
| 536-568 | `_build_arms` | - | yes |
| 569-629 | `_arms_state` | - | yes |
| 630-676 | `_layout_hud` | - | yes |
| 677-892 | `_apply_camera_feel` | - | yes |
| 936-964 | `_feel_hold` | - | yes |
| 995-1056 | `_render_snapshot` | - | yes |
| 1095-1141 | `_feel_death` | - | yes |
| 1142-1170 | `_ride_death` | - | yes |
| 1177-1210 | `_feel_zipline` | - | yes |
| 1230-1283 | `_feel_crane` | - | yes |
| 1284-1341 | `_feel_forklift` | - | yes |
| 1387-1441 | `_write_telemetry` | - | yes |

### `main_geometry.gd` (725 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 51-77 | `_add_tree` | - | yes |
| 296-322 | `_make_streak_texture` | - | yes |
| 344-387 | `_material` | - | yes |
| 621-657 | `_batch_static_dressing` | - | yes |

### `main_machines.gd` (1134 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 109-344 | `_build_kit` | - | yes |
| 410-437 | `_build_hook_gun` | - | yes |
| 438-467 | `_build_hook_view` | - | yes |
| 481-517 | `_render_hook` | - | yes |
| 542-616 | `_render_rubble` | - | yes |
| 642-669 | `_rock_mesh` | - | yes |
| 726-758 | `_stone_dust` | - | yes |
| 759-790 | `_tumble_stones` | - | yes |
| 813-839 | `_glass_material` | - | yes |
| 840-894 | `_build_wet_e` | - | yes |
| 895-947 | `_build_vents` | - | yes |
| 960-992 | `_spray` | - | yes |
| 993-1020 | `_grit` | - | yes |
| 1021-1049 | `_mist` | - | yes |
| 1104-1135 | `_render_vents` | - | yes |

### `main_world.gd` (908 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 40-87 | `_build_world` | - | yes |
| 102-233 | `_build_stack` | - | yes |
| 272-312 | `_build_stack_halls` | - | yes |
| 313-361 | `_build_stack_signage` | - | yes |
| 362-426 | `_build_stack_bridges` | - | yes |
| 427-461 | `_build_stack_accents` | - | yes |
| 464-519 | `_build_stack_dressing` | - | yes |
| 520-553 | `_build_tower_skin` | - | yes |
| 554-593 | `_build_stack_continuation` | - | yes |
| 640-670 | `_build_lighting` | - | yes |
| 681-730 | `_build_mountains_and_waterfall` | - | yes |
| 738-796 | `_build_basin` | - | yes |
| 842-879 | `_build_waterfall` | - | yes |
| 880-909 | `_build_kellerworks_signage` | - | yes |

### `ui/pause_menu.gd` (735 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 175-294 | `_build` | - | yes |
| 295-352 | `menu_theme` | - | yes |
| 406-432 | `_build_controls_page` | - | yes |
| 456-493 | `_controls_rows` | - | yes |
| 494-524 | `_draw_controls` | - | yes |
| 636-663 | `_slider_row` | - | yes |

### `ui/touch_controls.gd` (793 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 236-265 | `_layout` | - | yes |
| 341-374 | `_touch_down` | - | yes |
| 375-429 | `_touch_drag` | - | yes |
| 430-469 | `_touch_up` | - | yes |
| 496-533 | `_update_stick_vector` | - | yes |
| 534-649 | `update_context` | - | yes |
| 657-701 | `_draw_button` | - | yes |
| 702-731 | `_draw_stick` | - | yes |

### `uncanny/voice_keeper.gd` (685 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 155-200 | `_make_buses` | - | yes |
| 382-409 | `_window_done` | - | yes |
| 445-491 | `voiced_share` | - | no |
| 594-643 | `update` | - | yes |
| 660-686 | `echo_source` | - | yes |


## 9. Headless Godot tests (`godot/tests/*.gd`)

Run with `tools/godot/godot --headless --path godot --script res://tests/<name>.gd`. They load the scripts named; the scripts' own functions are what they run.

| Test | Loads | Lines |
|---|---|---|
| `tests/footstep_contact_test.gd` | `audio/audio_director.gd` | 31 |
| `tests/gore_shots_test.gd` | `life/gore/gore_view.gd`, `life/matron_view.gd` | 269 |
| `tests/her_takes_test.gd` | `audio/her_takes.gd` | 154 |
| `tests/matron_figure_test.gd` | `life/matron_look.gd`, `life/matron_view.gd` | 254 |
| `tests/matron_perception_climb_test.gd` | `life/matron_view.gd` | 37 |
| `tests/matron_presentation_test.gd` | `life/matron_view.gd` | 245 |
| `tests/script_load_test.gd` | - | 32 |

## 10. Not reached by any test

- Native (`src/sim`): `PhysicsWorld::slingshot_prediction` (`physics_world.cpp` 939-974), `Slingshot::reset` (`slingshot.cpp` 969-1005), `Slingshot::prediction` (`slingshot.cpp` 1007-1071): no native group calls them. The bridge binds `get_slingshot_prediction`, which `touch_slingshot` can reach (section 6).
- `src/sim/matron_mover.hpp`: interface only, no code.
- Bridge (`src/bridge/scraperx_simulation.cpp`): the native groups do not link it; every `get_*_state` and `_bind_methods` is reached only by the touch scenarios and the game.
- Presentation: `audio/voice_lines.gd`, `identity/portrait_face_fit.gd`, `life/climber/climber_body_rig.gd`. See section 7 for what that means.
- `SCRAPERX_ONLY=swing` runs the parkour list (`run_brachiation` ... `run_c7`), not `run_swing` (the AS-012 swing): the second `swing` block in main() is never reached. `run_swing` runs only in the full run.

## Method (to regenerate)

A throwaway Python script with libclang (`pip install libclang`, 18.x) parsed every `src/**/*.cpp` and `tests/*.cpp` with the flags of `build/host` (`-std=c++17`, Jolt include path and `JPH_*` defines, `-DSCRAPERX_HAS_JOLT=1`), keyed functions by canonical declaration location, and recorded call edges (`CALL_EXPR` references), override links, constructor edges for `make_unique`/`emplace*`, and struct-field references. A second script did the GDScript. Neither is in the repo; if the map should stay current they belong under `tools/` with a CI check.
