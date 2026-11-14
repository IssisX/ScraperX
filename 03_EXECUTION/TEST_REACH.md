# Test-reach map: which tests a change can reach

Look up the file (or function, with its line number) you changed; the row names the native groups and touch scenarios that run it. This serves `CLAUDE.md` "Test only what a change reaches": run those, cheapest first, once, and say in the report which and why. It is a lookup, not a verdict: read "How far to trust it" first.

Built 2026-11-14 on fb-bot/integrate-j141 at commit d8a2d71f. Regenerate when the test lists, the kit's body order or the call structure change a lot (the method is below); a stale map is worse than none.

## How to use it (30 seconds)

1. Changed a `src/sim/*.cpp` or `.hpp`? Find the file in section 2. Changed a function in a big file? Section 3 has it with its line range. Changed Matron behavior? Start from the subject index (section 4).
2. Take the tiers left to right: **D** (a test calls this code straight from its own body), **T1+O**, **O**, **T1** (weaker, see below). Run the first group of the strongest tier present; add a second only if the change spans subjects. Groups inside a tier are listed narrowest first (fewest `src/sim` functions reached without the per-tick step), so the first is the cheapest.
3. Run it as `SCRAPERX_ONLY=<group> ./build/host/scraperx_sim_tests` (the 13 named in `CMakeLists.txt` are also ctest names, `ctest -R scraperx_sim.<group>`). Under 3 groups should be enough for one change.
4. Changed `godot/presentation/**/*.gd`? Section 7 (files) and section 8 (functions in big files) name the touch scenarios (`--uitest=<scenario>`); a `touch_*` scenario also runs as `<name>_buttons` on the BUTTONS scheme.
5. Row says `tick only`, `no scenario or test reaches it` or `**no test reaches**`: nothing gives direct evidence; say so in the report instead of picking a random group.

## How far to trust it

Built from the code, not from names: every `src/sim`, `src/bridge` and `tests/*.cpp` file was parsed with libclang (the build's include paths and defines) into a call graph; each group's seed functions are the `run_*`/`test_*` calls in its block of `tests/simulation_tests.cpp` main(); the closure of those through the graph is what the group "reaches". GDScript was parsed by a small script: each scenario's `match` arm in `ui_test_driver.gd` is the seed, calls are resolved by function name (and by receiver name for `obj.f()` calls), and `_native.f()` calls are resolved to the bridge method bound under that name (`src/bridge/scraperx_simulation.cpp`) and from there to `src/sim`.

What it cannot see, and what I did about it:

- **Every group ticks the world.** `Simulation::advance_frame` reaches 100 of the `src` files from any group, so plain reachability says "all tests reach everything". The per-tick step is therefore cut out: **T1** is reach without passing through it (setup calls, debug hooks, requests, accessors). Code that only runs in the tick when the right state holds is not separated by T1.
- **O (observes).** A group counts as observing a function if it reaches the function (tick included) and its test code reads a struct field (`MatronSnapshot.grabs`, `Snapshot.zipline_riding`, ...) that the function also touches. Fields read by more than 35% of groups are ignored as noise. This is evidence a test watches the function's effect, not proof it runs the branch you changed.
- **D** is the strongest: some test function calls the function directly. It is rarely complete (a group that reaches the code through the tick is not a D).
- **Virtual calls** are followed to every override; Jolt callbacks (contact listener and layer filters) are tied to every `PhysicsSystem::Update`; `make_unique`/`emplace` construct edges go to the class's constructors; a destructor counts with its constructor. Function pointers stored in tables, `std::function`, and GDScript signals/`call_deferred` by string are followed only when the name is a literal in a `connect`/`call_deferred`/`Callable` call; anything else is missed (GD side: **may run via signals/scene wiring**).
- **Nothing was run.** This map is static, per the job. The ctest names and run command are from `CMakeLists.txt`.

## 1. Native test groups

154 entries: the 151 `SCRAPERX_ONLY` groups of `tests/simulation_tests.cpp`, the full run (`scraperx_sim.athletic_traversal`: every `run_*` in main(); the groups `matron-chase` and `matron-grab` run only in their own slice, as the comments in main() say), and the two other test executables.

"Src functions reached" is the group's size without the per-tick step: the smaller, the cheaper and more focused. The last column lists the `src/sim` and bridge `.cpp` files that no more than 30% of groups reach: what makes the group different.

| Group (`SCRAPERX_ONLY=`) | ctest name | Runs | Src functions reached without the per-tick step | Files only this kind of group reaches |
|---|---|---|---|---|
| `ladder-bar-sort` | scraperx_sim.ladder-bar-sort | run_ladder_bar_sort | 1 | - |
| `matron-budget` | scraperx_sim.matron-budget | run_matron_budget | 6 | - |
| `jumpbuffer` | - | run_jump_buffer | 603 | - |
| `edge-stand` | - | run_edge_stand | 604 | - |
| `twin-geometry` | - | run_twin_geometry | 604 | - |
| `workers` | - | run_workers | 605 | - |
| `C6` | - | run_c6 | 606 | - |
| `landing` | - | run_landing | 606 | - |
| `parkour_flow (own exe)` | scraperx_sim.parkour_flow | main | 606 | - |
| `AS-010-c4` | - | run_service_c4 | 607 | - |
| `AS-010-c5` | - | run_service_c5 | 608 | - |
| `parkour` | - | run_parkour_seams | 608 | - |
| `handling` | - | run_wet_handling | 609 | - |
| `slingshot` | - | run_slingshot | 610 | - |
| `AS-008-wreck` | - | run_plate_wreckage | 611 | - |
| `trapeze` | - | run_trapeze | 611 | - |
| `AS-008-band` | - | run_plate_band | 612 | - |
| `AS-008-route` | - | run_plate_route | 612 | - |
| `AS-009-band` | - | run_crane_band | 612 | - |
| `AS-010-route` | - | run_service_route | 612 | - |
| `summit` | - | run_summit, run_summit_arm | 612 | - |
| `AS-009-wreck` | - | run_crane_wreckage | 613 | - |
| `look` | - | run_look_target | 613 | - |
| `AS-008` | - | run_plate_shop | 614 | - |
| `AS-009-route` | - | run_crane_route | 614 | - |
| `zipline` | - | run_zipline | 615 | - |
| `AS-009` | - | run_facade_crane | 616 | - |
| `AS-010-n` | - | run_service_n | 616 | - |
| `AS-010` | - | run_midstack_service | 617 | - |
| `matron-room` | - | run_matron_room | 617 | - |
| `AS-010-o` | - | run_service_o | 618 | - |
| `matron-collision` | - | run_matron_collision | 618 | - |
| `matron-ladder` | scraperx_sim.matron-ladder | run_matron_ladder | 618 | - |
| `AS-007-band` | - | run_wet_band | 621 | - |
| `AS-007-route` | - | run_wet_route | 621 | - |
| `AS-010-band` | - | run_service_band | 622 | - |
| `tram` | - | run_tram | 623 | - |
| `matron-load` | scraperx_sim.matron-load | run_matron_load | 624 | - |
| `matron-mischief` | - | run_matron_mischief | 624 | - |
| `pitman` | - | run_pitman | 624 | - |
| `helix` | - | run_helix | 625 | - |
| `incline` | - | run_incline | 626 | - |
| `matron-ladder-pull` | scraperx_sim.matron-ladder-pull | run_matron_ladder_pull | 627 | - |
| `matron-crash` | scraperx_sim.matron-crash | run_matron_crash | 628 | - |
| `forklift` | - | run_forklift, run_forklift_strain | 629 | - |
| `up-throw` | - | run_up_throw | 629 | - |
| `high` | - | run_high | 630 | - |
| `well` | - | run_well_stage_a, run_well_stage_b, run_well_stage_c, run_well_band | 630 | - |
| `ascent` | - | run_ascent | 631 | - |
| `objectives` | scraperx_sim.objectives | run_objectives | 631 | objectives.cpp |
| `derrick` | - | run_derrick | 632 | - |
| `swing` | - | run_swing | 632 | - |
| `wheel` | - | run_wheel | 632 | - |
| `spreader` | - | run_yard_crane_spreader | 634 | - |
| `machine-hearing` | scraperx_sim.machine-hearing | run_machine_hearing | 635 | stock_damage.cpp |
| `matron-traverse` | - | run_matron_traverse | 635 | - |
| `stack` | - | run_stack | 635 | - |
| `water` | - | run_water | 636 | - |
| `matron-rage-blow` | - | run_matron_rage_blow | 638 | stock_damage.cpp |
| `matron-balance` | - | run_matron_balance | 639 | stock_damage.cpp |
| `body-order` | - | run_body_order | 640 | - |
| `determinism` | - | run_determinism | 640 | - |
| `one-arm` | - | run_one_arm_route | 640 | - |
| `mast` | - | run_mast | 644 | - |
| `matron-home-chase` | scraperx_sim.matron-home-chase | run_matron_home_chase | 646 | - |
| `stock` | - | run_loose_stock | 648 | stock_damage.cpp |
| `crane` | - | run_yard_crane | 649 | - |
| `matron-stand` | scraperx_sim.matron-stand | run_matron_stand | 653 | - |
| `matron-chase-bands` | scraperx_sim.matron-chase-bands | run_matron_chase_bands | 657 | - |
| `matron-sweep` | scraperx_sim.matron-sweep | run_matron_sweep | 658 | - |
| `matron-arm` | - | run_matron_arm | 659 | - |
| `matron-moves` | scraperx_sim.matron-moves | run_matron_pace, run_matron_unstick | 659 | stock_damage.cpp |
| `matron-ammo` | scraperx_sim.matron-ammo | run_ammo | 661 | stock_damage.cpp |
| `matron-jab` | scraperx_sim.matron-jab | run_matron_jab | 661 | - |
| `matron-kick` | scraperx_sim.matron-kick | run_matron_kick | 661 | - |
| `matron-player-independence` | - | run_matron_player_independence | 663 | - |
| `probe-plate` | scraperx_sim.probe-plate | run_probe_plate | 663 | stock_damage.cpp |
| `wreck-way-on` | - | run_wreck_way_on | 664 | stock_damage.cpp |
| `matron-pursuit` | - | run_matron_pursuit | 665 | - |
| `matron-wreck` | - | run_matron_wreck | 665 | stock_damage.cpp |
| `matron-pipe` | - | run_matron_pipe | 673 | stock_damage.cpp |
| `route-pockets` | - | run_route_pockets | 676 | matron_hoist.cpp |
| `perf-step` | scraperx_sim.perf-step | run_perf_step | 677 | - |
| `up` | - | run_up_controls | 681 | - |
| `save-probe` | scraperx_sim.save-probe | run_save_roundtrip_probe | 690 | stock_damage.cpp |
| `matron-slab-trap` | scraperx_sim.matron-slab-trap | run_matron_slab_trap | 693 | - |
| `hard-blow` | scraperx_sim.hard-blow | run_hard_blow | 695 | stock_damage.cpp |
| `forklift-matron` | scraperx_sim.forklift-matron | run_forklift_matron | 696 | - |
| `matron-slab-drop` | scraperx_sim.matron-slab-drop | run_matron_slab_drop | 698 | stock_damage.cpp |
| `climb` | - | run_wall_climb, run_tuck, run_flow, run_springboard, run_swing_launch, run_slide_jump +4 | 707 | matron_hoist.cpp |
| `matron-ladders` | scraperx_sim.matron-ladders | run_matron_ladders | 717 | matron_hoist.cpp |
| `matron-tower` | scraperx_sim.matron-tower | run_matron_tower_proof | 717 | matron_hoist.cpp |
| `matron-way-up-684` | scraperx_sim.matron-way-up-684 | run_matron_way_up_684 | 717 | matron_hoist.cpp |
| `J-105` | - | run_j105_leg_spiral | 720 | matron_hoist.cpp, matron_perception.cpp |
| `j105-leg-spiral` | scraperx_sim.j105-leg-spiral | run_j105_leg_spiral | 720 | matron_hoist.cpp, matron_perception.cpp |
| `matron-ways` | - | run_matron_ways | 720 | matron_hoist.cpp |
| `matron-ring-descent` | - | run_matron_ring_descent | 721 | matron_hoist.cpp |
| `matron-same-region` | - | run_matron_same_region | 724 | matron_hoist.cpp |
| `matron-heard-lead` | - | run_matron_heard_lead | 725 | matron_hoist.cpp |
| `matron-climb-scan` | - | run_matron_climb_scan | 726 | matron_hoist.cpp, matron_perception.cpp |
| `matron-chase` | scraperx_sim.matron-chase | run_matron_chase | 730 | matron_hoist.cpp |
| `checkpoint-moving-floor` | scraperx_sim.checkpoint-moving-floor | run_checkpoint_moving_floor | 731 | - |
| `matron-home-pursuit` | scraperx_sim.matron-home-pursuit | run_matron_home_pursuit_slice | 733 | matron_hoist.cpp |
| `teeter` | - | run_teeter | 742 | physics_world_fatality.cpp, stock_damage.cpp |
| `save-matron-state` | scraperx_sim.save-matron-state | run_save_matron_state | 747 | physics_world_fatality.cpp, stock_damage.cpp |
| `save` | - | run_save | 749 | physics_world_fatality.cpp, stock_damage.cpp |
| `twin-pitch` | - | run_twin_pitch | 750 | physics_world_fatality.cpp, stock_damage.cpp |
| `twin-pitch-probe` | - | run_twin_pitch_probe | 750 | physics_world_fatality.cpp, stock_damage.cpp |
| `twin-pitch-probe-pump` | - | run_twin_pitch_probe_pump | 750 | physics_world_fatality.cpp, stock_damage.cpp |
| `bay` | - | run_bay | 757 | physics_world_fatality.cpp, stock_damage.cpp |
| `save-plain-probe` | scraperx_sim.save-plain-probe | run_save_plain_probe | 758 | physics_world_fatality.cpp, stock_damage.cpp |
| `twin` | - | run_twin, run_twin_balconies, run_twin_pitch | 762 | physics_world_fatality.cpp, stock_damage.cpp |
| `matron-damage` | scraperx_sim.matron-damage | run_matron_damage | 767 | physics_world_fatality.cpp, stock_damage.cpp |
| `matron-injury` | scraperx_sim.matron-injury | run_matron_injury | 770 | physics_world_fatality.cpp, stock_damage.cpp |
| `matron-found-climb` | - | run_matron_found_climb | 771 | matron_hoist.cpp |
| `summit-ending` | - | run_summit_ending | 771 | physics_world_fatality.cpp, stock_damage.cpp |
| `magnet` | - | run_yard_crane_magnet | 772 | physics_world_fatality.cpp, stock_damage.cpp |
| `matron-hoist` | scraperx_sim.matron-hoist | run_matron_hoist | 775 | matron_hoist.cpp |
| `corpse` | - | run_corpse, run_arm_break, run_wind_own_drive | 779 | physics_world_fatality.cpp, stock_damage.cpp |
| `parkour-moves` | - | run_brachiation, run_pipe_gallery, run_slide, run_speed_vault, run_wall_run, run_wall_climb +12 | 780 | physics_world_fatality.cpp, stock_damage.cpp |
| `AS-007-E` | - | run_wet_e | 782 | physics_world_fatality.cpp, stock_damage.cpp |
| `AS-007` | - | run_wet_isolation, run_wet_e, run_wet_handling, run_wet_band, run_wet_route | 785 | physics_world_fatality.cpp, stock_damage.cpp |
| `strike` | - | run_strike | 786 | physics_world_fatality.cpp, stock_damage.cpp |
| `fabricated` | - | run_fabricated | 802 | physics_world_fatality.cpp, stock_damage.cpp |
| `stock-damage` | - | run_stock_damage | 828 | physics_world_fatality.cpp, stock_damage.cpp |
| `save-roundtrip` | scraperx_sim.save-roundtrip | run_save_roundtrip | 832 | physics_world_fatality.cpp, stock_damage.cpp |
| `matron-fatality` | - | run_matron_fatality | 848 | matron_hoist.cpp, physics_world_fatality.cpp, stock_damage.cpp |
| `service-bait-lift` | scraperx_sim.service-bait-lift | run_service_bait_lift | 868 | matron_hoist.cpp, physics_world_fatality.cpp, stock_damage.cpp |
| `matron-grab` | scraperx_sim.matron-grab | run_matron_grab, run_matron_arm, run_matron_fatality | 880 | matron_hoist.cpp, physics_world_fatality.cpp, stock_damage.cpp |
| `matron-exam` | - | run_matron_exam | 938 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-chain` | scraperx_sim.matron-chain | run_matron_chain | 939 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-narrow` | - | run_matron_narrow | 939 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-shape` | scraperx_sim.matron-shape | run_matron_shape | 939 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-towers` | scraperx_sim.matron-towers | run_matron_towers | 939 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-capacity` | scraperx_sim.matron-capacity | run_matron_capacity | 940 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-exam-climber` | - | run_matron_exam_climber | 940 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-climb-board` | - | run_matron_climb_board | 988 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-climb` | scraperx_sim.matron-climb | run_matron_climb, run_matron_climb_board, run_matron_climb_scan | 996 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-remaining` | scraperx_sim.matron-remaining | run_matron_remaining | 1015 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp, stock_damage.cpp |
| `matron` | - | run_matron | 1034 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp, stock_damage.cpp |
| `matron-hearing` | scraperx_sim.matron-hearing | test_matron_hearing | 1065 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-sight` | scraperx_sim.matron-sight | test_matron_sight | 1065 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-exam-routecraft` | - | run_matron_exam_routecraft | 1066 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-riser` | scraperx_sim.matron-riser | test_matron_riser | 1066 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-state-exits` | scraperx_sim.matron-state-exits | run_matron_state_exits | 1066 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-board-walls` | scraperx_sim.matron-board-walls | run_matron_board_walls | 1067 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-honest` | scraperx_sim.matron-honest | test_matron_honest | 1067 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `shared_survival (own exe)` | scraperx_sim.shared_survival | main | 1067 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-exam-descent` | scraperx_sim.matron-exam-descent | run_matron_exam_descent | 1068 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-climb-support` | scraperx_sim.matron-climb-support | test_matron_climb_support | 1069 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-stalk` | scraperx_sim.matron-stalk | test_matron_stalk, test_matron_ladder_head | 1073 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `matron-routecraft` | - | run_matron_routecraft | 1084 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp |
| `athletic-traversal-1` | scraperx_sim.athletic-traversal-1 | run_athletic_traversal_1 | 1229 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp, physics_world_fatality.cpp, stock_damage.cpp |
| `athletic-traversal-2` | scraperx_sim.athletic-traversal-2 | run_athletic_traversal_2 | 1440 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp, physics_world_fatality.cpp, stock_damage.cpp |
| `(all: scraperx_sim.athletic_traversal)` | scraperx_sim.athletic_traversal | run_pulley_static_end, run_matron_exam, run_core_traversal, run_fall_checkpoint, run_world_solids, run_twin_geometry +116 | 1501 | bridge/scraperx_simulation.cpp, matron_hoist.cpp, matron_perception.cpp, objectives.cpp, physics_world_fatality.cpp, stock_damage.cpp |

Not groups but also tests: `tests/twin_geometry_test.py <pre-relocation commit>` (compares `src/sim/world_solids.inc` boxes against a base commit; no `src/sim/*.cpp` is run); CI runs `ctest` plus a bare `./build/host/scraperx_sim_tests` (every group in the default order).

## 2. Every `src/sim` file and the native groups that reach it

`Run these` lists tiers D, T1+O, O, T1 for the union of the file's functions (a group appears in its strongest tier only; `~all` means 80% or more of groups). Columns 4 and 5 are counts of groups that reach the file without / only through the per-tick step. `core: ~every group` means a function in the file is reached by 80% or more of groups without the tick (usually constructors, accessors or world building): use section 3 for the function you changed. `src/bridge` is not in any native group (the native tests do not link the bridge); section 9 covers it.

| Source file | Lines | Run these (tier, narrowest first) | Groups reaching it without the tick | Groups reaching it only via the tick | Verdict |
|---|---|---|---|---|---|
| `src/bridge/scraperx_simulation.cpp` | 2352 | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25 | 126 | specific |
| `src/bridge/scraperx_simulation.hpp` | 370 | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25 | 126 | specific |
| `src/sim/band_counterweight_well.cpp` | 1089 | **D** `matron-routecraft`, `athletic-traversal-2` · **T1** ~all (150) | 152 | 0 | core: ~every group |
| `src/sim/band_facade_crane.cpp` | 799 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/band_midstack_service.cpp` | 1365 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/band_plate_shop.cpp` | 867 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/band_stack.cpp` | 1709 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/band_wet_isolation.cpp` | 799 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/bay.cpp` | 331 | **T1+O** `save-plain-probe`, `save-roundtrip`, `shared_survival (own exe)` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/bay.hpp` | 185 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/climb_c6.cpp` | 146 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/climb_c7.cpp` | 187 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/corpse.cpp` | 222 | **T1+O** `matron-remaining`, `matron`, `athletic-traversal-2` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/corpse.hpp` | 113 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/crew.cpp` | 357 | **T1+O** `matron-climb-scan`, `matron-exam`, `matron-chain`, `matron-narrow` +17 more · **T1** ~all (131) | 152 | 0 | core: ~every group |
| `src/sim/crew.hpp` | 141 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/fabricated.cpp` | 210 | **D** `fabricated`, `athletic-traversal-2` · **T1** `save-probe`, `forklift-matron`, `teeter`, `save-matron-state` +45 more | 51 | 100 | specific |
| `src/sim/forklift.cpp` | 481 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/forklift.hpp` | 186 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/impacts.cpp` | 155 | **D** `matron-crash`, `machine-hearing`, `stock`, `matron-ammo` +2 more · **T1+O** `forklift`, `up-throw`, `crane`, `up` +3 more · **T1** ~all (139) | 152 | 0 | core: ~every group |
| `src/sim/impacts.hpp` | 117 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/loose_stock.cpp` | 1132 | **D** `forklift`, `spreader`, `stock`, `crane` +4 more · **T1** ~all (144) | 152 | 0 | core: ~every group |
| `src/sim/loose_stock.hpp` | 247 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/matron.cpp` | 5705 | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +147 more · **T1** `twin-geometry` | 152 | 0 | core: ~every group |
| `src/sim/matron.hpp` | 3177 | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +147 more · **T1** `twin-geometry` | 152 | 0 | core: ~every group |
| `src/sim/matron_capacity.hpp` | 113 | **D** `matron-injury`, `matron-capacity` | 2 | 0 | specific |
| `src/sim/matron_climb.cpp` | 286 | **T1+O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +13 more · **T1** ~all (135) | 152 | 0 | core: ~every group |
| `src/sim/matron_contact.cpp` | 140 | **T1+O** `matron-climb`, `matron-state-exits`, `matron-climb-support`, `athletic-traversal-2` · **T1** ~all (148) | 152 | 0 | core: ~every group |
| `src/sim/matron_effort.hpp` | 222 | **D** `matron-budget`, `matron-ammo` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 27 | 125 | specific |
| `src/sim/matron_fatality.cpp` | 342 | **D** `matron-state-exits`, `matron-climb-support` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-stand` +73 more | 79 | 72 | specific |
| `src/sim/matron_found_climb.cpp` | 871 | **D** `matron-state-exits`, `matron-climb-support` · **T1+O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +15 more · **T1** ~all (131) | 152 | 0 | core: ~every group |
| `src/sim/matron_grip.hpp` | 79 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/matron_hoist.cpp` | 404 | **T1+O** `climb`, `matron-tower`, `matron-way-up-684`, `matron-ways` +20 more · **O** `matron-traverse`, `matron-pursuit`, `matron-wreck`, `forklift-matron` · **T1** `route-pockets`, `matron-ladders`, `J-105`, `j105-leg-spiral` +16 more | 44 | 107 | specific |
| `src/sim/matron_hold.cpp` | 1703 | **D** `matron-state-exits`, `matron-climb-support`, `matron-routecraft`, `athletic-traversal-2` · **T1+O** `matron-traverse`, `matron-pursuit`, `climb`, `matron-tower` +7 more · **T1** ~all (137) | 152 | 0 | core: ~every group |
| `src/sim/matron_injury.hpp` | 177 | **D** `matron-injury` · **T1** ~all (151) | 152 | 0 | core: ~every group |
| `src/sim/matron_journey.cpp` | 678 | **D** `matron-board-walls`, `matron-exam-descent`, `matron-climb-support`, `athletic-traversal-2` · **T1+O** `route-pockets`, `matron-ladders`, `matron-tower`, `matron-way-up-684` +18 more · **T1** ~all (126) | 152 | 0 | core: ~every group |
| `src/sim/matron_line.cpp` | 399 | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** ~all (150) | 152 | 0 | core: ~every group |
| `src/sim/matron_mover.hpp` | 114 | - | 0 | 0 | NO TEST REACHES IT |
| `src/sim/matron_moves.cpp` | 368 | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-stand` +78 more | 84 | 67 | specific |
| `src/sim/matron_perception.cpp` | 200 | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `J-105`, `j105-leg-spiral`, `matron-climb-scan`, `matron-exam` +22 more | 28 | 123 | specific |
| `src/sim/matron_perception.hpp` | 135 | **D** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more · **T1** `J-105`, `j105-leg-spiral`, `matron-climb-scan`, `matron-exam` +11 more | 28 | 123 | specific |
| `src/sim/matron_pipe.cpp` | 1298 | **D** `matron-state-exits`, `matron-climb-support` · **T1+O** `matron-wreck`, `route-pockets`, `forklift-matron`, `matron-ladders` +24 more · **T1** `jumpbuffer`, `edge-stand`, `twin-geometry`, `workers` +118 more | 152 | 0 | core: ~every group |
| `src/sim/matron_routecraft.cpp` | 606 | **D** `matron-routecraft`, `athletic-traversal-2` · **T1+O** `matron-climb-scan`, `matron-damage`, `matron-found-climb`, `fabricated` +4 more · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` · **T1** `matron-traverse`, `matron-stand`, `matron-sweep`, `matron-jab` +62 more | 76 | 75 | specific |
| `src/sim/matron_routecraft.hpp` | 145 | **D** `matron-routecraft`, `athletic-traversal-2` · **T1+O** `matron-exam-routecraft` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/matron_shape.cpp` | 462 | **D** `matron-budget`, `matron-shape`, `matron-remaining`, `matron` +1 more · **T1+O** `matron-exam-routecraft`, `matron-state-exits`, `matron-board-walls`, `matron-exam-descent` +2 more · **T1** ~all (142) | 153 | 0 | core: ~every group |
| `src/sim/matron_stalk.cpp` | 203 | **T1+O** `matron-found-climb`, `matron-climb-board`, `matron-climb`, `matron-board-walls` +4 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-stand`, `matron-sweep`, `matron-jab`, `matron-kick` +48 more | 60 | 91 | specific |
| `src/sim/matron_travel.cpp` | 87 | **T1+O** `route-pockets`, `matron-ladders`, `matron-tower`, `matron-way-up-684` +20 more · **T1** ~all (128) | 152 | 0 | core: ~every group |
| `src/sim/matron_traversal.cpp` | 4090 | **D** `ladder-bar-sort`, `route-pockets`, `matron-ladders`, `matron-tower` +25 more · **T1+O** `matron-traverse`, `matron-pursuit`, `forklift-matron`, `climb` +5 more · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +1 more · **T1** `matron-stand`, `matron-sweep`, `matron-jab`, `matron-kick` +35 more | 77 | 75 | specific |
| `src/sim/matron_traversal.hpp` | 493 | **D** `route-pockets`, `climb`, `matron-ladders`, `matron-tower` +25 more · **T1** `jumpbuffer`, `edge-stand`, `twin-geometry`, `workers` +119 more | 152 | 0 | core: ~every group |
| `src/sim/mechanism_kit.cpp` | 2360 | **T1+O** `matron-climb-scan`, `save-plain-probe`, `save-roundtrip`, `matron-chain` +12 more · **T1** ~all (136) | 152 | 0 | core: ~every group |
| `src/sim/mechanism_kit.hpp` | 984 | **D** `checkpoint-moving-floor`, `save`, `magnet`, `athletic-traversal-1` +1 more · **T1** ~all (147) | 152 | 0 | core: ~every group |
| `src/sim/objectives.cpp` | 124 | **D** `objectives` | 1 | 0 | specific |
| `src/sim/objectives.hpp` | 57 | **D** `objectives` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 26 | 125 | specific |
| `src/sim/physics_world.cpp` | 1923 | **T1+O** `matron-crash`, `forklift`, `up-throw`, `spreader` +23 more · **T1** ~all (125) | 152 | 0 | core: ~every group |
| `src/sim/physics_world.hpp` | 1385 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_arm.cpp` | 263 | **T1+O** `matron-climb-board`, `matron-climb`, `matron-remaining`, `matron` +2 more · **O** `matron-climb-scan` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-arm` +43 more | 53 | 80 | specific |
| `src/sim/physics_world_climb.cpp` | 819 | **T1+O** `matron-routecraft`, `athletic-traversal-1`, `athletic-traversal-2` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_fabricated.cpp` | 262 | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** `route-pockets`, `save-probe`, `forklift-matron`, `climb` +61 more | 67 | 84 | specific |
| `src/sim/physics_world_fatality.cpp` | 419 | **T1+O** `athletic-traversal-1`, `athletic-traversal-2` · **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb`, `matron-remaining` +1 more · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +20 more | 26 | 107 | specific |
| `src/sim/physics_world_grab.cpp` | 656 | **T1+O** `matron-climb-board`, `matron-climb`, `matron-remaining`, `matron` +2 more · **O** `matron-ammo`, `matron-climb-scan`, `matron-routecraft` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-arm` +43 more | 53 | 80 | specific |
| `src/sim/physics_world_movement.cpp` | 1312 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_one_arm.cpp` | 162 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_probes.cpp` | 1214 | **T1+O** `athletic-traversal-1` · **T1** ~all (151) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_state.cpp` | 1454 | **T1+O** `edge-stand`, `twin-geometry`, `workers`, `C6` +102 more · **T1** `jumpbuffer`, `slingshot`, `trapeze`, `look` +42 more | 152 | 0 | core: ~every group |
| `src/sim/physics_world_strike.cpp` | 515 | **T1+O** `matron-sweep`, `matron-jab`, `matron-kick`, `perf-step` +5 more · **T1** ~all (143) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_support.hpp` | 956 | **D** `save-plain-probe`, `save-roundtrip`, `matron-exam`, `matron-shape` +5 more · **T1** ~all (143) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_traversal.cpp` | 917 | **T1+O** `matron-climb`, `matron-routecraft`, `athletic-traversal-2` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/physics_world_water.cpp` | 101 | - | 0 | 133 | tick-only (gated by runtime state) |
| `src/sim/pipe_gallery.cpp` | 92 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/rope_constraint.cpp` | 244 | **T1+O** `athletic-traversal-1` · **T1** ~all (151) | 152 | 0 | core: ~every group |
| `src/sim/rope_constraint.hpp` | 122 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/shared_survival.cpp` | 207 | **D** `shared_survival (own exe)` · **T1** ~all (151) | 152 | 0 | core: ~every group |
| `src/sim/shared_survival.hpp` | 100 | **D** `shared_survival (own exe)` · **T1** ~all (151) | 152 | 0 | core: ~every group |
| `src/sim/simulation.cpp` | 1974 | **D** `jumpbuffer`, `edge-stand`, `twin-geometry`, `workers` +130 more · **T1+O** `matron-hearing`, `matron-honest`, `shared_survival (own exe)`, `matron-climb-support` +1 more · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +9 more | 152 | 0 | core: ~every group |
| `src/sim/simulation.hpp` | 2184 | **D** `save-roundtrip` · **T1** ~all (151) | 152 | 0 | core: ~every group |
| `src/sim/slab_cell.cpp` | 78 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/slingshot.cpp` | 1073 | **T1+O** `save-plain-probe`, `save-roundtrip`, `matron-honest` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/slingshot.hpp` | 214 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/slingshot_model.cpp` | 76 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/stock_damage.cpp` | 468 | **D** `stock-damage`, `athletic-traversal-2` · **T1+O** `matron-damage`, `athletic-traversal-1` · **O** `AS-010-c4`, `stack`, `matron-climb-scan`, `matron-found-climb` +1 more · **T1** `machine-hearing`, `matron-rage-blow`, `matron-balance`, `stock` +33 more | 41 | 92 | specific |
| `src/sim/stock_damage.hpp` | 245 | **D** `stock-damage`, `athletic-traversal-2` · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +20 more | 26 | 107 | specific |
| `src/sim/summit.cpp` | 513 | **T1+O** `shared_survival (own exe)`, `athletic-traversal-1` · **T1** ~all (150) | 152 | 0 | core: ~every group |
| `src/sim/summit.hpp` | 150 | **T1** `summit`, `summit-ending`, `athletic-traversal-2` | 3 | 0 | specific |
| `src/sim/swing.cpp` | 595 | **T1+O** `save-plain-probe`, `save-roundtrip`, `shared_survival (own exe)` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/swing.hpp` | 163 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/trapeze.cpp` | 366 | **T1+O** `save-plain-probe`, `save-roundtrip`, `shared_survival (own exe)` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/trapeze.hpp` | 152 | **T1** `trapeze`, `spreader`, `crane`, `up` +4 more | 8 | 125 | specific |
| `src/sim/twin_tower.cpp` | 237 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/barrel_helix.cpp` | 44 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/cascade_mast.cpp` | 81 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/crown_gondola.cpp` | 34 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/gravity_balance.cpp` | 34 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/luffing_derrick.cpp` | 145 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/pitman_lift.cpp` | 121 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/rack_climber.cpp` | 35 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/slab_incline.cpp` | 179 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/stone_wheel.cpp` | 264 | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** ~all (150) | 152 | 0 | core: ~every group |
| `src/sim/vertical/traction_tram.cpp` | 49 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/vertical_machine.cpp` | 124 | **T1+O** `save-plain-probe`, `save-roundtrip`, `shared_survival (own exe)` · **T1** ~all (149) | 152 | 0 | core: ~every group |
| `src/sim/vertical/vertical_route.cpp` | 523 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/vertical/vertical_route.hpp` | 214 | **T1** `tram`, `pitman`, `helix`, `incline` +11 more | 15 | 0 | specific |
| `src/sim/yard_crane.cpp` | 898 | **T1+O** `crane`, `shared_survival (own exe)`, `athletic-traversal-1`, `athletic-traversal-2` · **T1** ~all (148) | 152 | 0 | core: ~every group |
| `src/sim/yard_crane.hpp` | 312 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/zipline.cpp` | 375 | **T1** ~all (152) | 152 | 0 | core: ~every group |
| `src/sim/zipline.hpp` | 215 | **T1** ~all (152) | 152 | 0 | core: ~every group |

Header-only files have inline functions only; `matron_mover.hpp` is a pure interface (no function bodies), so no test "reaches" it by itself: its `Mover::step` is reached through the ways that implement it (`TravelWay`, `PipeWay`, the climb and line ways).

## 3. Big files, function by function

Every function of 25 or more lines (40 in headers) in a file of 500 or more lines. `Reached w/o tick / only via tick` counts groups. A function with no reach has the note `**no test reaches**`: the native tests never run it (compare section 9 for the bridge).

### `src/bridge/scraperx_simulation.cpp` (2352 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 48-405 | `ScraperXSimulation::_bind_methods` | - | 0/0 | **no test reaches** |
| 856-905 | `ScraperXSimulation::get_kit_body_poses` | - | 0/0 | **no test reaches** |
| 1126-1156 | `ScraperXSimulation::get_landing_state` | - | 0/0 | **no test reaches** |
| 1181-1213 | `ScraperXSimulation::get_wet_state` | - | 0/0 | **no test reaches** |
| 1255-1284 | `ScraperXSimulation::get_service_state` | - | 0/0 | **no test reaches** |
| 1298-1331 | `ScraperXSimulation::get_slingshot_state` | - | 0/0 | **no test reaches** |
| 1472-1496 | `ScraperXSimulation::get_trapeze_state` | - | 0/0 | **no test reaches** |
| 1510-1560 | `forklift_dictionary` | - | 0/0 | **no test reaches** |
| 1601-1639 | `ScraperXSimulation::get_yard_crane_state` | - | 0/0 | **no test reaches** |
| 1765-1950 | `ScraperXSimulation::fill_matron_state` | - | 0/0 | **no test reaches** |
| 1976-2087 | `ScraperXSimulation::fill_matron_hold` | - | 0/0 | **no test reaches** |
| 2199-2228 | `ScraperXSimulation::get_swing_state` | - | 0/0 | **no test reaches** |
| 2252-2277 | `ScraperXSimulation::get_lift_state` | - | 0/0 | **no test reaches** |

### `src/sim/band_counterweight_well.cpp` (1089 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 104-138 | `cage_parts` | **T1** ~all (152) | 152/0 |  |
| 140-319 | `build_stage_a` | **T1** ~all (152) | 152/0 |  |
| 384-428 | `boom_parts` | **T1** ~all (152) | 152/0 |  |
| 430-590 | `build_stage_b` | **T1** ~all (152) | 152/0 |  |
| 726-998 | `build_stage_c` | **T1** ~all (152) | 152/0 |  |
| 1032-1078 | `build_climbing_route` | **T1** ~all (152) | 152/0 |  |

### `src/sim/band_facade_crane.cpp` (799 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 167-207 | `build_frame` | **T1** ~all (152) | 152/0 |  |
| 237-308 | `build_stage_j` | **T1** ~all (152) | 152/0 |  |
| 335-413 | `build_stage_k` | **T1** ~all (152) | 152/0 |  |
| 438-515 | `build_stage_l` | **T1** ~all (152) | 152/0 |  |
| 541-577 | `build_springboard` | **T1** ~all (152) | 152/0 |  |
| 601-642 | `build_stones` | **T1** ~all (152) | 152/0 |  |
| 661-693 | `build_tuck` | **T1** ~all (152) | 152/0 |  |
| 713-755 | `build_slide_jump` | **T1** ~all (152) | 152/0 |  |
| 757-785 | `build_climbing_route` | **T1** ~all (152) | 152/0 |  |

### `src/sim/band_midstack_service.cpp` (1365 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 158-184 | `build_frame` | **T1** ~all (152) | 152/0 |  |
| 242-321 | `build_stage_m` | **T1** ~all (152) | 152/0 |  |
| 358-423 | `build_c4` | **T1** ~all (152) | 152/0 |  |
| 553-625 | `build_stage_n_frame` | **T1** ~all (152) | 152/0 |  |
| 627-710 | `build_stage_n` | **T1** ~all (152) | 152/0 |  |
| 889-925 | `build_c5_frame` | **T1** ~all (152) | 152/0 |  |
| 927-1012 | `build_c5` | **T1** ~all (152) | 152/0 |  |
| 1016-1047 | `build_bait_lift_frame` | **T1** ~all (152) | 152/0 |  |
| 1049-1127 | `build_stage_o_frame` | **T1** ~all (152) | 152/0 |  |
| 1129-1279 | `build_stage_o` | **T1** ~all (152) | 152/0 |  |
| 1320-1363 | `bands::build_bait_lift` | **T1** ~all (152) | 152/0 |  |

### `src/sim/band_plate_shop.cpp` (867 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 99-171 | `build_leg_spiral` | **T1** ~all (152) | 152/0 |  |
| 243-316 | `build_stage_g` | **T1** ~all (152) | 152/0 |  |
| 358-448 | `build_stage_h` | **T1** ~all (152) | 152/0 |  |
| 475-555 | `build_stage_i` | **T1** ~all (152) | 152/0 |  |
| 583-625 | `build_wall_stack` | **T1** ~all (152) | 152/0 |  |
| 657-705 | `build_gantry` | **T1** ~all (152) | 152/0 |  |
| 722-765 | `build_gorge` | **T1** ~all (152) | 152/0 |  |
| 783-821 | `build_run` | **T1** ~all (152) | 152/0 |  |

### `src/sim/band_stack.cpp` (1709 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 158-183 | `cage_parts` | **T1** ~all (152) | 152/0 |  |
| 217-378 | `build_s1_headframe` | **T1** ~all (152) | 152/0 |  |
| 380-454 | `build_yard_traversal` | **T1** ~all (152) | 152/0 |  |
| 456-542 | `build_s1` | **T1** ~all (152) | 152/0 |  |
| 612-649 | `build_deck2_traversal` | **T1** ~all (152) | 152/0 |  |
| 651-723 | `build_c1_traversal_enhancements` | **T1** ~all (152) | 152/0 |  |
| 725-825 | `build_c1` | **T1** ~all (152) | 152/0 |  |
| 895-920 | `s2_cage_parts` | **T1** ~all (152) | 152/0 |  |
| 922-955 | `s2_beam_parts` | **T1** ~all (152) | 152/0 |  |
| 957-989 | `build_crossover_gangway` | **T1** ~all (152) | 152/0 |  |
| 991-1018 | `build_deck4_traversal` | **T1** ~all (152) | 152/0 |  |
| 1033-1093 | `build_s2_frame` | **T1** ~all (152) | 152/0 |  |
| 1095-1148 | `build_s2` | **T1** ~all (152) | 152/0 |  |
| 1162-1196 | `build_c2_traversal_enhancements` | **T1** ~all (152) | 152/0 |  |
| 1198-1308 | `build_c2` | **T1** ~all (152) | 152/0 |  |
| 1364-1395 | `build_north_crossover_gangway` | **T1** ~all (152) | 152/0 |  |
| 1397-1422 | `s3_cage_parts` | **T1** ~all (152) | 152/0 |  |
| 1465-1500 | `build_s3_frame` | **T1** ~all (152) | 152/0 |  |
| 1502-1559 | `build_s3` | **T1** ~all (152) | 152/0 |  |
| 1572-1680 | `build_c3` | **T1** ~all (152) | 152/0 |  |

### `src/sim/band_wet_isolation.cpp` (799 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 285-390 | `build_stage_d` | **T1** ~all (152) | 152/0 |  |
| 412-602 | `build_stage_e` | **T1** ~all (152) | 152/0 |  |
| 604-647 | `build_stage_f` | **T1** ~all (152) | 152/0 |  |
| 649-690 | `build_plate_and_header` | **T1** ~all (152) | 152/0 |  |
| 737-775 | `build_climbing_route` | **T1** ~all (152) | 152/0 |  |

### `src/sim/loose_stock.cpp` (1132 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 75-385 | `make_spec` | **T1** ~all (152) | 152/0 |  |
| 389-468 | `add_stock_yard` | **T1** ~all (152) | 152/0 |  |
| 499-526 | `LooseStock::placements` | **D** `forklift`, `spreader`, `stock`, `crane` +2 more · **T1** ~all (146) | 152/0 |  |
| 534-566 | `LooseStock::sound` | **D** `matron-ammo` · **T1** ~all (151) | 152/0 |  |
| 656-681 | `LooseStock::create_body` | **T1+O** `forklift`, `spreader`, `stock`, `crane` +2 more · **T1** ~all (146) | 152/0 |  |
| 734-761 | `spot_templates` | **T1** ~all (152) | 152/0 |  |
| 816-1130 | `LooseStock::dress_tower` | **T1** ~all (152) | 152/0 |  |

### `src/sim/matron.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **senses and hunt, balance, pace, cable** row of section 4, then confirm it with this row.
 (5705 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 74-240 | `Matron::Matron` | **D** `matron-hearing`, `matron-sight`, `matron-riser`, `matron-state-exits` +5 more · **T1+O** `matron-climb-scan`, `matron-exam`, `matron-chain`, `matron-narrow` +10 more · **T1** ~all (129) | 152/0 |  |
| 281-305 | `Matron::set_home` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** ~all (150) | 152/0 |  |
| 307-343 | `Matron::place_for_test` | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +147 more | 48/103 |  |
| 436-465 | `Matron::look_at` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 467-546 | `Matron::muffled_distance` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 573-597 | `Matron::onto_floor_lines` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 650-680 | `Matron::floor_at` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 686-861 | `Matron::route_toward` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-stand` +78 more | 84/67 |  |
| 886-915 | `Matron::way_clear` | **T1** `water`, `one-arm`, `matron-home-chase`, `matron-stand` +80 more | 84/67 |  |
| 991-1070 | `Matron::feel_floor` | **D** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 1072-1135 | `Matron::settle_level` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 1137-1162 | `Matron::off_its_steel` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 1164-1945 | `Matron::walk_toward` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-capacity` +9 more · **O** `matron-wreck`, `forklift-matron`, `matron-way-up-684`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-hearing`, `matron-sight` +8 more | 25/126 |  |
| 1949-1974 | `Matron::step_turn` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 1981-2014 | `Matron::step_rage` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 2028-2052 | `Matron::step_planks` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 2054-2082 | `Matron::restore_planks` | **T1+O** `matron-honest` · **T1** `matron-stand`, `matron-sweep`, `matron-jab`, `matron-kick` +55 more | 60/91 |  |
| 2112-2278 | `Matron::trap_step` | **D** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 2331-2360 | `Matron::take_way` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 2376-3534 | `Matron::pre_step` | **D** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more · **O** `matron-traverse`, `matron-pursuit`, `matron-wreck`, `route-pockets` +18 more | 13/131 |  |
| 3536-3611 | `Matron::post_step` | **D** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 3613-3640 | `Matron::catch_sense` | **T1** `shared_survival (own exe)`, `matron-stalk`, `matron-routecraft`, `athletic-traversal-2` | 4/131 |  |
| 3653-3693 | `Matron::caught` | **D** `shared_survival (own exe)`, `matron-stalk` | 2/133 |  |
| 3695-3719 | `Matron::shove_direction` | - | 0/133 | per-tick only |
| 3754-3792 | `Matron::notice_moved_stock` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 3837-3894 | `Matron::fetch_stock` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 3909-3935 | `Matron::take_stock` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 3937-3983 | `Matron::hold_stock` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 3985-4050 | `Matron::throw_stock` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 4077-4156 | `Matron::pick_shove_edge` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 4158-4299 | `Matron::shove_stock` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 4321-4360 | `Matron::end_thrown_claim` | - | 0/133 | per-tick only |
| 4362-4395 | `Matron::throw_stroke` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 4421-4657 | `Matron::restore` | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +147 more | 60/91 |  |
| 4697-4774 | `Matron::sense_near` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 4779-5078 | `Matron::take_blow` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 5080-5116 | `Matron::clear_interrupted_actions` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-stand`, `matron-sweep`, `matron-jab`, `matron-kick` +54 more | 60/91 |  |
| 5136-5161 | `Matron::shape_fits` | **T1+O** `matron-exam-routecraft`, `matron-routecraft`, `athletic-traversal-2` · **T1** `matron-traverse`, `matron-exam`, `matron-chain`, `matron-narrow` +19 more | 26/125 |  |
| 5170-5205 | `Matron::lie_down` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 5231-5267 | `Matron::sole_contact` | **T1** ~all (152) | 152/0 |  |
| 5294-5335 | `Matron::brace_at_edges` | **T1+O** `matron-exam-descent`, `athletic-traversal-2` · **O** `matron-wreck`, `forklift-matron`, `matron-way-up-684`, `matron-climb-scan` +5 more · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 5338-5387 | `Matron::step_knocked` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 5389-5703 | `Matron::refresh_view` | **T1+O** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-state-exits` +7 more · **T1** ~all (141) | 152/0 |  |

### `src/sim/matron.hpp` (3177 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/matron_found_climb.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (871 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 91-122 | `Matron::found_floor` | **T1+O** `matron-exam-routecraft`, `matron-routecraft`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 124-262 | `Matron::find_climb` | **T1+O** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-towers` +9 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-shape`, `matron-capacity`, `matron-climb-board`, `matron-remaining` +8 more | 25/126 |  |
| 312-384 | `Matron::follow_found_climb` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 391-446 | `RoutecraftWay::step` | **T1+O** `matron-exam-routecraft`, `matron-routecraft`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 475-504 | `RoutecraftWay::view` | **T1+O** `matron-exam-routecraft`, `matron-state-exits`, `matron-climb-support`, `matron-routecraft` +1 more · **T1** ~all (147) | 152/0 |  |
| 537-609 | `Matron::start_journey_to` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +7 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-climb-board` +10 more | 25/126 |  |
| 611-667 | `Matron::end_journey` | **T1+O** `matron-found-climb`, `matron-chain`, `matron-narrow`, `matron-towers` +8 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` · **T1** `matron-stand`, `matron-sweep`, `matron-jab`, `matron-kick` +45 more | 61/90 |  |
| 669-802 | `Matron::step_journey` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +7 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +10 more | 25/126 |  |
| 827-860 | `JourneyWay::view` | **T1+O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +12 more · **T1** ~all (136) | 152/0 |  |

### `src/sim/matron_hold.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **hold** row of section 4, then confirm it with this row.
 (1703 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 47-89 | `Matron::begin_grab` | **D** `matron-routecraft`, `athletic-traversal-2` | 2/131 |  |
| 108-142 | `Matron::report_held_load` | - | 0/133 | per-tick only |
| 144-212 | `Matron::grip_lost` | **D** `matron-state-exits`, `matron-climb-support` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-arm` +34 more | 40/95 |  |
| 268-381 | `Matron::choose_rim` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 408-492 | `Matron::choose_fatal_edge` | **T1+O** `matron-state-exits`, `matron-climb-support`, `athletic-traversal-2` · **O** `matron-traverse`, `matron-pursuit`, `climb`, `matron-tower` +7 more · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +6 more | 13/131 |  |
| 494-519 | `Matron::hang_room` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 552-595 | `Matron::fall_impact_mps` | **T1** `water`, `one-arm`, `matron-home-chase`, `matron-arm` +46 more | 50/94 |  |
| 637-689 | `Matron::end_hold` | **D** `matron-state-exits`, `matron-climb-support` · **T1** `water`, `one-arm`, `matron-home-chase`, `matron-stand` +73 more | 79/72 |  |
| 691-1216 | `Matron::step_hold` | **D** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 1259-1361 | `Matron::step_armplay` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 1404-1443 | `Matron::arm_repose` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 1450-1481 | `Matron::spare_side` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |
| 1553-1583 | `Matron::his_blow` | **D** `matron-state-exits`, `matron-climb-support` · **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +7 more | 13/131 |  |
| 1589-1614 | `Matron::over_her_deck` | **T1** `matron-hearing`, `matron-sight`, `matron-exam-routecraft`, `matron-riser` +9 more | 13/131 |  |

### `src/sim/matron_journey.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (678 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 61-127 | `Matron::region_under` | **T1+O** `matron-tower`, `matron-way-up-684`, `matron-ways`, `matron-climb-scan` +5 more · **O** `route-pockets`, `matron-ring-descent`, `matron-same-region`, `matron-heard-lead` +2 more · **T1** `matron-ladders`, `matron-exam`, `matron-chain`, `matron-narrow` +19 more | 32/119 |  |
| 145-216 | `Matron::travel_to` | **T1+O** `matron-exam-climber`, `matron-climb`, `matron-state-exits`, `matron-board-walls` +3 more · **T1** `matron-ladders`, `matron-tower`, `matron-way-up-684`, `matron-ways` +21 more | 32/119 |  |
| 218-583 | `Matron::step_travel` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +8 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +9 more | 25/126 |  |
| 588-621 | `TravelWay::begin` | **T1+O** `matron-chain`, `matron-towers`, `matron-exam-climber`, `matron-climb` +5 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-narrow`, `matron-shape`, `matron-capacity` +12 more | 25/126 |  |
| 642-676 | `TravelWay::view` | **T1+O** `route-pockets`, `matron-tower`, `matron-way-up-684`, `matron-ways` +19 more · **T1** ~all (129) | 152/0 |  |

### `src/sim/matron_pipe.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **corner pipe** row of section 4, then confirm it with this row.
 (1298 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 77-106 | `Matron::pipe_top_under` | **T1** `matron-traverse`, `matron-exam`, `matron-chain`, `matron-narrow` +22 more | 26/125 |  |
| 214-240 | `Matron::pipe_grip_in_reach` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 272-317 | `Matron::get_on_pipe` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-traverse`, `matron-exam`, `matron-chain`, `matron-narrow` +20 more | 26/125 |  |
| 319-362 | `Matron::get_off_pipe` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 388-959 | `Matron::ride_pipe` | **D** `matron-state-exits`, `matron-climb-support` · **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-capacity` +7 more · **O** `matron-wreck`, `forklift-matron`, `matron-way-up-684`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-hearing`, `matron-sight` +8 more | 25/126 |  |
| 1022-1095 | `Matron::own_way_sooner` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +7 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +10 more | 25/126 |  |
| 1097-1154 | `Matron::follow_pipe` | **T1+O** `matron-state-exits`, `matron-climb-support` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |

### `src/sim/matron_routecraft.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (606 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 119-145 | `MatronRoutecraft::resume_transition` | **T1+O** `matron-routecraft`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 163-247 | `MatronRoutecraft::plan` | **T1+O** `matron-exam`, `matron-climb`, `matron-exam-routecraft`, `matron-routecraft` +2 more · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +4 more · **T1** `matron-chain`, `matron-narrow`, `matron-shape`, `matron-towers` +15 more | 25/126 |  |
| 336-488 | `MatronRoutecraft::advance_plan` | **T1+O** `matron-routecraft`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 489-547 | `MatronRoutecraft::hold` | **T1+O** `matron-exam-routecraft`, `matron-routecraft`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 566-605 | `MatronRoutecraft::step` | **D** `matron-routecraft`, `athletic-traversal-2` · **T1+O** `matron-exam-routecraft` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |

### `src/sim/matron_traversal.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **climbing** row of section 4, then confirm it with this row.
 (4090 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 160-196 | `gap_flight` | **T1+O** `matron-way-up-684`, `matron-climb-scan`, `service-bait-lift`, `matron-chain` +9 more · **O** `matron-wreck`, `forklift-matron` · **T1** `route-pockets`, `climb`, `matron-ladders`, `matron-tower` +27 more | 44/107 |  |
| 266-294 | `Budget::take` | **D** `matron-capacity`, `matron-climb`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 357-388 | `Climber::revalidate` | **T1+O** `matron-climb`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 390-425 | `Climber::take_hold` | **T1+O** `matron-climb`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 427-457 | `Climber::find_rung` | **T1+O** `matron-narrow`, `matron-climb`, `matron-stalk`, `athletic-traversal-2` · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan`, `matron-found-climb` · **T1** `matron-exam`, `matron-chain`, `matron-shape`, `matron-towers` +17 more | 25/126 |  |
| 459-502 | `Climber::find_lip` | **T1+O** `matron-climb`, `matron-stalk`, `athletic-traversal-2` · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan`, `matron-found-climb` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 515-556 | `Climber::plant_leg` | **T1+O** `matron-climb`, `matron-exam-descent`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 558-592 | `Climber::planted_leg_contact` | **T1+O** `matron-climb`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +19 more | 25/126 |  |
| 599-667 | `Climber::drive_leg` | **T1+O** `matron-capacity`, `matron-climb`, `matron-exam-descent`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +17 more | 25/126 |  |
| 669-703 | `Climber::drive` | **T1+O** `matron-climb`, `matron-exam-descent`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 737-780 | `Climber::stand` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-climb` +1 more · **T1+O** `matron-capacity`, `matron-exam-climber`, `matron-climb-board`, `matron-remaining` +2 more · **O** `matron-wreck`, `forklift-matron`, `matron-way-up-684`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-hearing`, `matron-sight` +10 more | 25/126 |  |
| 782-885 | `Climber::walk` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +6 more · **O** `matron-wreck`, `forklift-matron`, `matron-way-up-684`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-hearing` +11 more | 25/126 |  |
| 898-927 | `Climber::begin` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **T1+O** `matron-exam-descent`, `matron-climb-support`, `matron-stalk` · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-traverse`, `matron-exam`, `matron-shape`, `matron-capacity` +12 more | 26/125 |  |
| 985-1022 | `Climber::maintain_second_hold` | **T1+O** `matron-narrow`, `matron-climb`, `matron-stalk`, `athletic-traversal-2` · **T1** `matron-exam`, `matron-chain`, `matron-shape`, `matron-towers` +17 more | 25/126 |  |
| 1024-1071 | `Climber::step` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **T1+O** `matron-capacity`, `matron-remaining`, `matron`, `matron-exam-descent` · **O** `matron-wreck`, `forklift-matron`, `matron-way-up-684`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-hearing`, `matron-sight` +10 more | 25/126 |  |
| 1100-1188 | `Climber::step_rung_up` | **T1+O** `matron-narrow`, `matron-climb-board`, `matron-climb`, `matron-exam-descent` +2 more · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan`, `matron-found-climb` · **T1** `matron-exam`, `matron-chain`, `matron-shape`, `matron-towers` +15 more | 25/126 |  |
| 1192-1221 | `Climber::begin_mantle_from_hang` | **T1+O** `matron-narrow`, `matron-exam-climber`, `matron-climb-board`, `matron-climb` +2 more · **O** `matron-tower`, `matron-climb-scan`, `matron-found-climb` · **T1** `matron-exam`, `matron-chain`, `matron-shape`, `matron-towers` +15 more | 25/126 |  |
| 1226-1363 | `Climber::step_mantle` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +6 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +11 more | 25/126 |  |
| 1371-1477 | `Climber::step_dyno` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +7 more · **O** `matron-wreck`, `forklift-matron`, `matron-ladders`, `matron-tower` +4 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-hearing` +10 more | 25/126 |  |
| 1482-1648 | `Climber::step_rung_down` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +6 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +11 more | 25/126 |  |
| 1655-1814 | `Climber::step_pole` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-capacity` +7 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-remaining`, `matron` +10 more | 25/126 |  |
| 1820-1932 | `Climber::step_gap` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +8 more · **O** `matron-wreck`, `forklift-matron`, `matron-ladders`, `matron-tower` +5 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-hearing` +9 more | 25/126 |  |
| 1937-2004 | `Climber::step_jump` | **T1+O** `matron-narrow`, `matron-climb-board`, `matron-climb`, `matron-exam-descent` +2 more · **O** `matron-ladders`, `matron-tower`, `matron-climb-scan`, `matron-found-climb` · **T1** `matron-exam`, `matron-chain`, `matron-shape`, `matron-towers` +15 more | 25/126 |  |
| 2016-2105 | `Climber::step_ledge_jump` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +6 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +11 more | 25/126 |  |
| 2110-2248 | `Climber::step_drop` | **T1+O** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +6 more · **O** `matron-ladders`, `matron-tower`, `matron-ways`, `matron-climb-scan` +1 more · **T1** `matron-exam`, `matron-shape`, `matron-capacity`, `matron-remaining` +11 more | 25/126 |  |
| 2338-2385 | `steel_key` | **T1** `route-pockets`, `climb`, `matron-ladders`, `matron-tower` +40 more | 44/107 |  |
| 2483-2703 | `AffordanceMap::scan_steel` | **T1+O** `climb`, `matron-ladders`, `matron-tower`, `matron-ways` +10 more · **O** `matron-traverse`, `matron-pursuit` · **T1** `route-pockets`, `matron-way-up-684`, `J-105`, `j105-leg-spiral` +26 more | 44/107 |  |
| 2710-2838 | `AffordanceMap::split_patches` | **T1+O** `climb`, `matron-tower`, `matron-ways`, `matron-ring-descent` +6 more · **O** `matron-traverse`, `matron-pursuit` · **T1** `route-pockets`, `matron-ladders`, `matron-way-up-684`, `J-105` +30 more | 44/107 |  |
| 2840-2923 | `AffordanceMap::build_regions` | **T1+O** `route-pockets`, `climb`, `matron-tower`, `matron-way-up-684` +12 more · **O** `AS-010-c4`, `matron-traverse`, `stack`, `body-order` +4 more · **T1** `matron-ladders`, `J-105`, `j105-leg-spiral`, `matron-home-pursuit` +24 more | 44/107 |  |
| 2978-3643 | `AffordanceMap::build_edges` | **T1+O** `climb`, `matron-ladders`, `matron-tower`, `matron-way-up-684` +20 more · **O** `AS-010-c4`, `matron-traverse`, `stack`, `body-order` +6 more · **T1** `route-pockets`, `J-105`, `j105-leg-spiral`, `matron-same-region` +16 more | 44/107 |  |
| 3645-3745 | `AffordanceMap::plan` | **D** `route-pockets`, `matron-tower`, `matron-chase`, `matron-found-climb` +12 more · **T1+O** `matron-ladders`, `matron-ways`, `matron-climb-scan`, `matron-stalk` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +2 more · **T1** `matron-way-up-684`, `service-bait-lift`, `matron-exam`, `matron-shape` +11 more | 35/116 |  |
| 3747-3806 | `AffordanceMap::walk_path` | **T1+O** `matron-climb`, `athletic-traversal-1`, `athletic-traversal-2` · **O** `matron-traverse`, `matron-pursuit`, `climb`, `matron-tower` +5 more · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +18 more | 25/126 |  |
| 3831-4088 | `traversal::grid_walk` | **D** `matron-chain`, `matron-narrow`, `matron-towers`, `matron-exam-climber` +3 more · **T1+O** `matron-climb-scan`, `athletic-traversal-1` · **O** `AS-010-c4`, `stack`, `body-order`, `determinism` +3 more · **T1** `J-105`, `j105-leg-spiral`, `matron-exam`, `matron-shape` +15 more | 28/123 |  |

### `src/sim/mechanism_kit.cpp` (2360 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 98-138 | `make_shape` | **T1** ~all (152) | 152/0 |  |
| 147-187 | `Kit::~Kit` | **T1** ~all (152) | 152/0 |  |
| 191-223 | `Kit::add_body` | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-shape` +8 more · **T1** ~all (140) | 152/0 |  |
| 225-250 | `Kit::add_shape_body` | **T1+O** `athletic-traversal-2` · **T1** `save-probe`, `forklift-matron`, `teeter`, `save-matron-state` +23 more | 28/0 |  |
| 296-324 | `Kit::add_guide` | **T1** ~all (152) | 152/0 |  |
| 753-782 | `Kit::govern` | - | 0/133 | per-tick only |
| 784-877 | `Kit::pre_step` | - | 0/133 | per-tick only |
| 891-923 | `Kit::land_stream` | - | 0/133 | per-tick only |
| 925-961 | `Kit::flow_bins` | - | 0/133 | per-tick only |
| 1001-1037 | `Kit::post_step` | - | 0/133 | per-tick only |
| 1217-1256 | `Kit::press` | - | 0/133 | per-tick only |
| 1258-1342 | `Kit::flow_water` | - | 0/133 | per-tick only |
| 1344-1406 | `Kit::flow_air` | - | 0/133 | per-tick only |
| 1457-1524 | `Kit::travel_envelopes` | **T1** ~all (152) | 152/0 |  |
| 1561-1596 | `Kit::connect_rope` | **T1+O** `athletic-traversal-1` · **T1** ~all (151) | 152/0 |  |
| 1678-1715 | `Kit::hook_target` | - | 0/133 | per-tick only |
| 1803-1861 | `Kit::capture` | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** ~all (150) | 152/0 |  |
| 1863-1982 | `Kit::restore` | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +20 more | 26/107 |  |
| 2080-2106 | `Kit::rope_polyline` | **T1** `swing`, `body-order`, `determinism`, `mast` +6 more | 10/0 |  |
| 2108-2146 | `Kit::cable_polyline` | **T1** `swing`, `body-order`, `determinism`, `mast` +6 more | 10/0 |  |
| 2312-2353 | `Kit::hoists` | **T1** `route-pockets`, `climb`, `matron-ladders`, `matron-tower` +20 more | 24/109 |  |

### `src/sim/mechanism_kit.hpp` (984 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/physics_world.cpp` (1923 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 36-543 | `PhysicsWorld::PhysicsWorld` | **T1+O** `forklift`, `spreader`, `stock`, `crane` +16 more · **T1** ~all (132) | 152/0 |  |
| 554-580 | `PhysicsWorld::give_matron_stock` | **T1+O** `forklift`, `spreader`, `stock`, `crane` +2 more · **T1** ~all (146) | 152/0 |  |
| 582-624 | `PhysicsWorld::~PhysicsWorld` | **T1** ~all (152) | 152/0 |  |
| 670-773 | `PhysicsWorld::spawn_break_pieces` | **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +22 more | 26/107 |  |
| 775-1337 | `PhysicsWorld::step` | **O** `matron-crash`, `forklift`, `up-throw`, `spreader` +11 more | 0/133 | per-tick only |
| 1356-1391 | `PhysicsWorld::slingshot_prediction` | - | 0/0 | **no test reaches** |
| 1450-1474 | `PhysicsWorld::add_shape_body` | **T1+O** `matron-climb-scan`, `matron-chain`, `matron-narrow`, `matron-shape` +8 more · **T1** ~all (140) | 152/0 |  |
| 1505-1538 | `PhysicsWorld::body_id_for_entity` | - | 0/133 | per-tick only |
| 1546-1592 | `PhysicsWorld::build_world_solids` | **T1** ~all (152) | 152/0 |  |
| 1641-1723 | `PhysicsWorld::build_stack` | **T1** ~all (152) | 152/0 |  |
| 1732-1759 | `PhysicsWorld::create_constraint` | **T1** `water`, `one-arm`, `matron-home-chase`, `matron-arm` +49 more | 53/80 |  |
| 1774-1803 | `PhysicsWorld::update_support_motion` | - | 0/133 | per-tick only |
| 1893-1918 | `PhysicsWorld::matron_grid_walk` | **T1+O** `matron-climb-scan`, `matron-climb` · **T1** `J-105`, `j105-leg-spiral` | 4/0 |  |

### `src/sim/physics_world.hpp` (1385 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/physics_world_climb.cpp` (819 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 28-87 | `PhysicsWorld::find_grip` | **T1+O** `athletic-traversal-1` · **O** `matron-routecraft`, `athletic-traversal-2` · **T1** `up` | 2/131 |  |
| 163-231 | `PhysicsWorld::update_swing` | **O** `matron-routecraft`, `athletic-traversal-2` | 0/133 | per-tick only |
| 254-300 | `PhysicsWorld::route_ahead` | **T1** `up`, `athletic-traversal-1` | 2/0 |  |
| 311-385 | `PhysicsWorld::update_climb` | **O** `matron-routecraft`, `athletic-traversal-2` | 0/133 | per-tick only |
| 389-461 | `PhysicsWorld::move_hands` | - | 0/133 | per-tick only |
| 544-574 | `PhysicsWorld::slide_down_climb` | **O** `matron-routecraft`, `athletic-traversal-2` | 0/133 | per-tick only |
| 579-610 | `PhysicsWorld::probe_lip` | - | 0/133 | per-tick only |
| 615-646 | `PhysicsWorld::update_shimmy` | - | 0/133 | per-tick only |
| 651-675 | `PhysicsWorld::probe_edge_drop` | - | 0/133 | per-tick only |
| 713-789 | `PhysicsWorld::beam_underfoot` | **O** `athletic-traversal-1` | 0/133 | per-tick only |
| 791-817 | `PhysicsWorld::clear_traversal` | **T1** `water`, `one-arm`, `matron-home-chase`, `matron-arm` +49 more | 53/80 |  |

### `src/sim/physics_world_grab.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **hold** row of section 4, then confirm it with this row.
 (656 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 76-107 | `PhysicsWorld::set_player_free_rotation` | **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +22 more | 26/107 |  |
| 196-288 | `PhysicsWorld::grab_pre_step` | - | 0/133 | per-tick only |
| 290-475 | `PhysicsWorld::grab_drive` | **O** `matron-climb-scan`, `matron-climb-board`, `matron-climb`, `matron-remaining` +3 more | 0/133 | per-tick only |
| 484-583 | `PhysicsWorld::grab_post_step` | - | 0/133 | per-tick only |
| 608-635 | `effort_constraint` | - | 0/133 | per-tick only |

### `src/sim/physics_world_movement.cpp` (1312 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 10-73 | `PhysicsWorld::update_wall_run` | - | 0/133 | per-tick only |
| 77-137 | `PhysicsWorld::try_wall_climb` | - | 0/133 | per-tick only |
| 211-251 | `PhysicsWorld::resolve_up` | - | 0/133 | per-tick only |
| 253-344 | `PhysicsWorld::apply_traversal_commands` | - | 0/133 | per-tick only |
| 480-564 | `PhysicsWorld::record_landing` | - | 0/133 | per-tick only |
| 569-596 | `PhysicsWorld::roll_locomotion` | - | 0/133 | per-tick only |
| 621-650 | `PhysicsWorld::track_support_jolt` | - | 0/133 | per-tick only |
| 654-702 | `PhysicsWorld::apply_wind` | - | 0/133 | per-tick only |
| 766-1160 | `PhysicsWorld::apply_locomotion` | - | 0/133 | per-tick only |
| 1185-1219 | `PhysicsWorld::try_step_up` | - | 0/133 | per-tick only |
| 1271-1310 | `PhysicsWorld::matron_loudness` | - | 0/133 | per-tick only |

### `src/sim/physics_world_probes.cpp` (1214 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 69-96 | `PhysicsWorld::cast_wall` | **T1** `up`, `athletic-traversal-1` | 2/131 |  |
| 156-245 | `PhysicsWorld::probe_ledge` | **T1** `up`, `athletic-traversal-1` | 2/131 |  |
| 250-289 | `PhysicsWorld::probe_vault_landing` | - | 0/133 | per-tick only |
| 337-374 | `PhysicsWorld::carry_candidate` | - | 0/133 | per-tick only |
| 492-562 | `PhysicsWorld::update_carry` | - | 0/133 | per-tick only |
| 602-638 | `PhysicsWorld::throw_velocity` | **T1** `up-throw`, `matron-ammo`, `up`, `hard-blow` +3 more | 7/126 |  |
| 666-694 | `PhysicsWorld::throw_prediction` | **T1** `up-throw`, `matron-ammo`, `up`, `hard-blow` +3 more | 7/0 |  |
| 712-739 | `PhysicsWorld::find_rig_action` | - | 0/133 | per-tick only |
| 771-811 | `PhysicsWorld::update_crouch` | - | 0/133 | per-tick only |
| 874-915 | `PhysicsWorld::hook_pre_step` | - | 0/133 | per-tick only |
| 967-1040 | `PhysicsWorld::hook_post_step` | - | 0/133 | per-tick only |
| 1046-1071 | `PhysicsWorld::tear_hook` | - | 0/133 | per-tick only |
| 1117-1144 | `PhysicsWorld::hook_line_blocked` | - | 0/133 | per-tick only |
| 1149-1212 | `PhysicsWorld::bend_hook_line` | - | 0/133 | per-tick only |

### `src/sim/physics_world_state.cpp` (1454 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 12-43 | `PhysicsWorld::update_precision_sight` | - | 0/133 | per-tick only |
| 45-92 | `PhysicsWorld::update_affordance` | - | 0/133 | per-tick only |
| 103-187 | `PhysicsWorld::update_up_move` | - | 0/133 | per-tick only |
| 220-348 | `PhysicsWorld::restore_from_checkpoint` | **T1+O** `magnet`, `stock-damage`, `athletic-traversal-2` · **O** `forklift`, `spreader`, `stock`, `crane` · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +19 more | 26/107 |  |
| 394-510 | `PhysicsWorld::write_save` | **T1** `save-probe`, `teeter`, `save-matron-state`, `save` +23 more | 27/0 |  |
| 516-1048 | `PhysicsWorld::read_save` | **T1+O** `save-plain-probe`, `magnet`, `stock-damage`, `save-roundtrip` +1 more · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +17 more | 26/0 |  |
| 1050-1092 | `PhysicsWorld::read_machine_state` | **T1+O** `handling`, `AS-008-wreck`, `AS-008-band`, `AS-009-band` +47 more · **T1** `jumpbuffer`, `edge-stand`, `twin-geometry`, `workers` +97 more | 152/0 |  |
| 1094-1452 | `PhysicsWorld::read_state` | **T1+O** `edge-stand`, `twin-geometry`, `workers`, `C6` +85 more · **T1** `jumpbuffer`, `handling`, `slingshot`, `trapeze` +59 more | 152/0 |  |

### `src/sim/physics_world_strike.cpp`

> State-gated code: T1 and O are weak here. Take the group from the **balance and blows** row of section 4, then confirm it with this row.
 (515 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 161-195 | `PhysicsWorld::sweep_land_point` | - | 0/133 | per-tick only |
| 282-480 | `PhysicsWorld::strike_step` | - | 0/133 | per-tick only |

### `src/sim/physics_world_support.hpp` (956 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 385-426 | `PlayerContactListener::observe_hit` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 435-508 | `PlayerContactListener::observe_support` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 560-688 | `detail::spawn_position` | **T1** ~all (152) | 152/0 |  |

### `src/sim/physics_world_traversal.cpp` (917 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 39-117 | `PhysicsWorld::try_begin_hang` | **O** `matron-routecraft`, `athletic-traversal-2` | 0/133 | per-tick only |
| 153-179 | `PhysicsWorld::try_begin_ground_traversal` | - | 0/133 | per-tick only |
| 199-245 | `PhysicsWorld::mantle_approach_end` | - | 0/133 | per-tick only |
| 247-289 | `PhysicsWorld::begin_mantle` | - | 0/133 | per-tick only |
| 313-427 | `PhysicsWorld::begin_vault` | - | 0/133 | per-tick only |
| 429-525 | `PhysicsWorld::traversal_point` | - | 0/133 | per-tick only |
| 527-622 | `PhysicsWorld::drive_traversal` | - | 0/133 | per-tick only |
| 624-672 | `PhysicsWorld::resolve_traversal_outcome` | - | 0/133 | per-tick only |
| 684-722 | `PhysicsWorld::complete_traversal` | - | 0/133 | per-tick only |

### `src/sim/simulation.cpp` (1974 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 410-445 | `Simulation::wet_state` | **D** `AS-007-band`, `AS-007-route`, `ascent`, `objectives` +7 more | 11/0 |  |
| 489-530 | `Simulation::service_state` | **D** `AS-010-route`, `AS-010-n`, `AS-010`, `AS-010-o` +5 more | 9/0 |  |
| 532-562 | `Simulation::stack_state` | **D** `objectives`, `stack`, `matron-chase-bands`, `wreck-way-on` +2 more | 6/0 |  |
| 564-692 | `Simulation::step_fixed` | **O** `edge-stand`, `C6`, `landing`, `AS-010-c4` +56 more | 0/133 | per-tick only |
| 694-729 | `Simulation::advance_frame` | **D** `jumpbuffer`, `edge-stand`, `workers`, `C6` +129 more | 133/0 |  |
| 829-858 | `Simulation::look_target` | **D** `twin-geometry`, `look`, `twin`, `athletic-traversal-1` +1 more | 5/0 |  |
| 882-919 | `Simulation::slingshot_state` | **D** `slingshot`, `save-probe`, `save-roundtrip`, `athletic-traversal-1` | 4/0 |  |
| 1023-1064 | `Simulation::forklift_state` | **D** `forklift`, `forklift-matron`, `athletic-traversal-2` · **T1+O** `body-order`, `stock`, `crane`, `up` +4 more | 11/0 |  |
| 1101-1142 | `Simulation::yard_crane_state` | **D** `spreader`, `crane`, `checkpoint-moving-floor`, `magnet` +1 more | 5/0 |  |
| 1172-1198 | `Simulation::trapeze_state` | **D** `trapeze`, `up`, `athletic-traversal-1` | 3/0 |  |
| 1271-1301 | `Simulation::matron_journey` | **D** `matron-chase-bands`, `matron-ladders`, `matron-tower`, `matron-way-up-684` +9 more | 13/0 |  |
| 1383-1426 | `Simulation::debug_matron_fixture` | **D** `matron-player-independence`, `athletic-traversal-2` | 2/0 |  |
| 1428-1489 | `Simulation::debug_matron_strikers` | **D** `matron-slab-trap`, `forklift-matron`, `matron-slab-drop` | 3/0 |  |
| 1515-1741 | `Simulation::matron_state` | **D** `matron-room`, `matron-collision`, `matron-ladder`, `matron-load` +57 more | 61/0 |  |
| 1801-1835 | `Simulation::swing_state` | **D** `swing`, `matron-chase-bands`, `save-probe`, `save-roundtrip` +1 more | 5/0 |  |
| 1847-1876 | `Simulation::load_game` | **D** `teeter`, `save-matron-state`, `save`, `twin-pitch` +22 more | 26/0 |  |
| 1944-1972 | `Simulation::lift_state` | **D** `tram`, `pitman`, `helix`, `incline` +11 more | 15/0 |  |

### `src/sim/simulation.hpp` (2184 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|


### `src/sim/slingshot.cpp` (1073 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 64-191 | `Slingshot::Slingshot` | **T1** ~all (152) | 152/0 |  |
| 217-249 | `Slingshot::create_aim_gimbal` | **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +22 more | 26/107 |  |
| 421-469 | `Slingshot::attach` | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +20 more | 26/107 |  |
| 536-576 | `Slingshot::refresh_state` | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** ~all (150) | 152/0 |  |
| 578-639 | `Slingshot::finish_previous_step` | **O** `save-plain-probe`, `save-roundtrip` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 641-713 | `Slingshot::pre_step` | **O** `save-plain-probe`, `save-roundtrip` | 0/133 | per-tick only |
| 715-744 | `Slingshot::drive_ratchet` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 746-820 | `Slingshot::OnStep` | **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 862-887 | `Slingshot::finish_recovery` | **O** `save-plain-probe`, `save-roundtrip` | 0/133 | per-tick only |
| 912-967 | `Slingshot::restore` | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** `teeter`, `save-matron-state`, `save`, `twin-pitch` +20 more | 26/107 |  |
| 969-1005 | `Slingshot::reset` | - | 0/0 | **no test reaches** |
| 1007-1071 | `Slingshot::prediction` | - | 0/0 | **no test reaches** |

### `src/sim/summit.cpp` (513 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 85-203 | `frame_parts` | **T1** ~all (152) | 152/0 |  |
| 219-252 | `lamp_parts` | **T1** ~all (152) | 152/0 |  |
| 255-300 | `Summit::Summit` | **T1+O** `shared_survival (own exe)` · **T1** ~all (151) | 152/0 |  |
| 310-357 | `Summit::build_ending` | **T1** ~all (152) | 152/0 |  |
| 434-474 | `Summit::read_ending` | - | 0/133 | per-tick only |

### `src/sim/swing.cpp` (595 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 61-150 | `frame_parts` | **T1** ~all (152) | 152/0 |  |
| 268-342 | `Swing::Swing` | **T1+O** `save-plain-probe`, `save-roundtrip` · **T1** ~all (150) | 152/0 |  |
| 431-459 | `Swing::machine_energy` | **T1** ~all (152) | 152/0 |  |
| 484-531 | `Swing::OnStep` | **O** `save-plain-probe`, `save-roundtrip` · **T1** `matron-exam`, `matron-chain`, `matron-narrow`, `matron-shape` +21 more | 25/126 |  |
| 533-558 | `Swing::post_step` | **O** `save-plain-probe`, `save-roundtrip` | 0/133 | per-tick only |

### `src/sim/vertical/vertical_route.cpp` (523 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 22-125 | `Route::Route` | **T1** ~all (152) | 152/0 |  |
| 156-186 | `Route::at` | **T1** `tram`, `pitman`, `helix`, `incline` +11 more | 15/118 |  |
| 224-257 | `Route::build_late` | **T1** ~all (152) | 152/0 |  |
| 259-328 | `Route::build_wheel_reload` | **T1** ~all (152) | 152/0 |  |
| 341-461 | `Route::pre_step` | - | 0/133 | per-tick only |
| 463-502 | `Route::pitman_step` | - | 0/133 | per-tick only |

### `src/sim/yard_crane.cpp` (898 lines)

| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |
|---|---|---|---|---|
| 53-84 | `mast_parts` | **T1** ~all (152) | 152/0 |  |
| 97-165 | `jib_parts` | **T1** ~all (152) | 152/0 |  |
| 302-346 | `dock_parts` | **T1+O** `crane`, `athletic-traversal-2` · **T1** ~all (150) | 152/0 |  |
| 361-451 | `YardCrane::YardCrane` | **T1+O** `crane`, `shared_survival (own exe)`, `athletic-traversal-1`, `athletic-traversal-2` · **T1** ~all (148) | 152/0 |  |
| 484-556 | `YardCrane::magnet_contact` | - | 0/133 | per-tick only |
| 558-584 | `YardCrane::work_magnet` | - | 0/133 | per-tick only |
| 586-640 | `YardCrane::spreader_seat` | - | 0/133 | per-tick only |
| 699-811 | `YardCrane::pre_step` | - | 0/133 | per-tick only |
| 813-858 | `YardCrane::post_step` | **T1** ~all (152) | 152/0 |  |


## 4. Matron subject index (what her tests watch)

Her code is one state machine, so reach alone cannot tell the holds tests from the climbing tests. This table groups her `MatronSnapshot` fields (and the hold/fatality test result structs) into subjects, and lists the groups whose test code reads any field of the subject beyond the ubiquitous ones, most fields first. The source-file column is my reading of each file's header comment, not computed. For a change in her hold code, run the first group here that also shows up in the function's row in section 3.

| Subject | Source files (my reading of their header comments) | Groups whose test code reads a field of that subject (count = distinct fields of that subject read; most first) |
|---|---|---|
| Her hold, arm tears, fatality (she grabs, carries, tears, kills) | matron_hold.cpp, matron_fatality.cpp, physics_world_grab.cpp, physics_world_arm.cpp, physics_world_fatality.cpp, physics_world_one_arm.cpp, matron_grip.hpp | `matron-grab` (92), `matron-fatality` (59), `matron-arm` (37), `athletic-traversal-2` (13), `matron` (12), `matron-remaining` (10), `matron-ladder-pull` (3), `matron-load` (2), `hard-blow` (2), `matron-mischief` (1), `one-arm` (1), `matron-jab` (1), `perf-step` (1), `matron-chase` (1), `strike` (1), `matron-climb-board` (1), `matron-climb` (1), `matron-routecraft` (1) |
| Her climbing (ladders, mantles, found climbs, journeys) | matron_climb.cpp, matron_traversal.cpp, matron_routecraft.cpp, matron_found_climb.cpp, matron_journey.cpp, matron_travel.cpp, matron_contact.cpp | `athletic-traversal-2` (54), `matron-climb` (50), `matron-climb-scan` (49), `matron` (44), `matron-remaining` (41), `matron-found-climb` (39), `matron-pipe` (38), `matron-tower` (11), `matron-way-up-684` (10), `matron-ways` (7), `service-bait-lift` (7), `matron-ladders` (6), `matron-chase` (6), `matron-moves` (5), `matron-ladder` (4), `matron-hoist` (4), `matron-traverse` (3), `matron-chase-bands` (3), `matron-home-pursuit` (3), `bay` (3), `matron-climb-board` (3), `matron-player-independence` (2), `matron-pursuit` (2), `matron-wreck` (2), `matron-routecraft` (2), `forklift-matron` (1), `matron-ring-descent` (1), `matron-same-region` (1), `matron-heard-lead` (1) |
| Her senses and hunt (hearing, sight, lead, fooled, wary) | matron.cpp (state machine, hunt), matron_moves.cpp | `athletic-traversal-2` (12), `matron` (10), `matron-hearing` (9), `matron-crash` (8), `matron-remaining` (7), `matron-sight` (7), `matron-honest` (7), `matron-rage-blow` (5), `matron-balance` (5), `matron-ammo` (5), `matron-chase` (5), `matron-room` (3), `forklift-matron` (3), `matron-home-chase` (2), `matron-moves` (2), `climb` (2), `matron-heard-lead` (2), `service-bait-lift` (2), `matron-grab` (2), `athletic-traversal-1` (2), `matron-load` (1), `machine-hearing` (1), `matron-arm` (1), `matron-pipe` (1), `save-probe` (1), `save-roundtrip` (1) |
| Her balance and the player's blows (strike, stagger, knockdown, rage) | physics_world_strike.cpp, matron.cpp | `athletic-traversal-2` (16), `matron-jab` (11), `strike` (11), `matron-sweep` (10), `matron-kick` (10), `hard-blow` (10), `matron-remaining` (8), `matron` (8), `matron-balance` (7), `matron-collision` (4), `matron-rage-blow` (4), `forklift-matron` (4), `matron-chase` (4), `matron-damage` (4), `matron-ammo` (3), `matron-injury` (3), `matron-grab` (3), `matron-mischief` (2), `matron-pipe` (2), `perf-step` (2), `save-probe` (2), `save-matron-state` (2), `save-roundtrip` (2), `matron-stand` (1), `matron-moves` (1), `matron-player-independence` (1), `matron-pursuit` (1), `service-bait-lift` (1) |
| Her corner pipe | matron_pipe.cpp | `matron-pipe` (11), `matron-remaining` (11), `matron` (11), `athletic-traversal-2` (11), `matron-home-pursuit` (5), `matron-climb-scan` (4), `matron-found-climb` (4), `matron-climb` (4), `matron-pursuit` (2), `matron-chase` (2), `matron-traverse` (1), `matron-moves` (1), `matron-player-independence` (1), `matron-ways` (1), `matron-ring-descent` (1) |
| Her cable and catwalk (lines, rotten planks) | matron_line.cpp, matron.cpp | `athletic-traversal-2` (10), `matron` (8), `matron-moves` (4), `matron-remaining` (4), `matron-chase` (2), `matron-player-independence` (1) |
| Her pace and getting unstuck | matron_moves.cpp, matron.cpp | `matron-moves` (10), `athletic-traversal-2` (10), `matron-chase` (5), `hard-blow` (2), `matron-remaining` (2), `matron` (2), `matron-mischief` (1), `matron-balance` (1), `matron-sweep` (1), `matron-jab` (1), `matron-kick` (1), `matron-slab-trap` (1), `matron-slab-drop` (1) |

## 5. Touch scenarios

`godot/presentation/ui/ui_test_driver.gd` has 162 scenarios (`SCENARIOS`), each a `match _scenario` arm calling a driver function in `ui_test_base.gd`, `_moves`, `_rides`, `_chase`, `_act`, `_bands`, `_service`, `_stack` or `_uncanny` (all one script by `extends`). Run one as `godot --headless --fixed-fps 60 --path godot -- --uitest=<scenario>` (CI runs the set in `wo000-delivery-spine.yml`; `look_*` and `perf_*` render only and are not in CI). The `touch_*` ones also run as `<name>_buttons`; `pad_*` and `keyboard_*` inject device events.

Columns: spawn is the id passed to `configure_initial_spawn`; "game functions" are `presentation` functions the scenario's own code calls (file:function, first five); "bridge methods" are native methods it calls (first five); the last column lists the `src/sim` files its bridge calls run without the per-tick step, ignoring files that more than 35% of scenarios reach.

| Scenario | Spawn | Driver | Input | Game functions it calls straight from the scenario (file:function) | Bridge methods it calls (native) | `src/sim` files those run (no per-tick step) |
|---|---|---|---|---|---|---|
| `audio_mix` | 8 | `_audio_mix` | none injected (renders/audio/perf) | audio_director:quiesce | - | - |
| `audio_fall` | 9 | `_audio_fall` | none injected (renders/audio/perf) | audio_director:quiesce | get_death_count | - |
| `perf_night` | 8 | `_perf_night` | none injected (renders/audio/perf) | sky_cycle:set_hour, sky_cycle:set_passed_height | - | - |
| `touch_app_leave_title` | 8 | `_touch_app_leave_title` | touch (gestures; `_buttons` too) | main:_save_climb, title_screen:button_center | get_save_game, get_tick_index | physics_world_fabricated.cpp, physics_world_state.cpp, stock_damage.cpp |
| `pad_core` | 8 | `_pad_core` | gamepad events | - | get_player_linear_velocity, get_player_position, get_tick_index, is_player_crouched, is_player_grounded | - |
| `perf_grade` | 8 | `_perf_probe` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_shadow_level, zipline_view:_pose | - | - |
| `perf_top` | 37 | `_perf_probe` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_shadow_level, zipline_view:_pose | - | - |
| `look_night_climb` | 8 | `_look_night_climb` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, sky_cycle:set_passed_height +1 | - | - |
| `look_night_lamps` | 8 | `_look_night_lamps` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_helmet_lamp_enabled, sky_cycle:set_hour +1 | - | - |
| `touch_clouds` | 8 | `_clouds` | touch (gestures; `_buttons` too) | cloud_layer:nearest, cloud_layer:puff_count, crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose +1 | - | - |
| `touch_quickturn` | 8 | `_touch_quickturn` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:is_button_shown, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, is_player_crouched | - |
| `perf_view_bottom` | 8 | `_perf_view_bottom` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, zipline_view:_pose | get_matron_state, get_player_position, get_step_timing | matron.cpp, matron_traversal.cpp |
| `perf_view_top` | 42 | `_perf_view_top` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, zipline_view:_pose | get_matron_state, get_player_position, get_step_timing | matron.cpp, matron_traversal.cpp |
| `look_finishes` | 8 | `_look_finishes` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, zipline_view:_pose | get_kit_body_index, get_kit_body_transform, get_loose_stock | - |
| `perf_fight` | 11 | `_perf_fight` | none injected (renders/audio/perf) | touch_controls:button_center | debug_hold_matron_hands, debug_place_matron, debug_set_matron_stage, get_matron_state, get_player_position +2 | matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_found_climb.cpp, matron_hold.cpp, matron_journey.cpp, matron_line.cpp, matron_pipe.cpp +2 |
| `touch_room_hearing` | 11 | `_touch_room_hearing` | touch (gestures; `_buttons` too) | main:_apply_settings, main:uncanny_profile_for_start, touch_controls:stick_home, voice_keeper:feed_samples, voice_keeper:room_loudness | configure_initial_spawn, get_matron_state, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +48 |
| `touch_works` | 27 | `_touch_works` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, works:beacon_count, works:lit_count +3 | - | - |
| `keyboard_core` | 8 | `_keyboard_core` | keyboard/mouse events | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, zipline_view:_pose | get_player_linear_velocity, get_player_position, get_rejected_traversal_count, is_player_crouched, is_player_grounded | - |
| `look_matron_gallery` | 11 | `_look_matron_gallery` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, matron_view:preview +2 | get_matron_state, get_player_position | matron.cpp, matron_traversal.cpp |
| `perf_walk` | 8 | `_perf_walk` | none injected (renders/audio/perf) | touch_controls:stick_home | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +3 | matron.cpp, matron_traversal.cpp |
| `touch_body` | 0 | `_touch_body` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_body:nearest_to, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center +3 | is_player_crouched | - |
| `touch_double_tap_vault` | 3 | `_touch_double_tap_vault` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:gesture_hint, touch_controls:stick_home, touch_controls:swipe_units | get_accepted_traversal_count, get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_move_look` | 8 | `_touch_move_look` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, touch_controls:swipe_units +1 | get_player_linear_velocity, get_player_position, is_player_grounded | - |
| `touch_workers` | 8 | `_touch_workers` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:draw_bounds, crew_view:figures, hair_pose_probe:_pose, matron_view:_pose +2 | get_player_position, get_worker_counts | - |
| `look_mist_streak` | 21 | `_look_mist_streak` | none injected (renders/audio/perf) | consent_screen:close, consent_screen:open, mist_streak:front_x, pause_menu:close, pause_menu:open +3 | get_simulation_time_seconds, get_wind_front_x_m | - |
| `touch_flow` | 8 | `_touch_flow` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_player_linear_velocity, is_player_crouched, is_player_grounded | - |
| `touch_gyro_aim` | 8 | `_touch_gyro_aim` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, main:_apply_settings, main:uncanny_profile_for_start, matron_view:_pose +2 | configure_initial_spawn, get_player_position, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +47 |
| `touch_lethal_feedback` | 9 | `_touch_lethal_feedback` | touch (gestures; `_buttons` too) | corpse_view:splashes_shown, crew_view:_pose, hair_pose_probe:_pose, hud:gauge_word, matron_view:_pose +1 | get_corpses, get_death_count, get_lethal_impact_speed_mps, get_player_position | corpse.cpp |
| `touch_swing_launch` | 39 | `_touch_swing_launch` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_player_linear_velocity, get_player_position, get_swing_launch_count, get_traversal_left_hand, get_traversal_state +2 | - |
| `touch_title` | 8 | `_title` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, title_screen:button_center, title_screen:button_text +3 | get_player_position, get_tick_index | - |
| `fall_voice` | 10 | `_fall_voice` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_death_count, get_landing_state, get_last_impact_speed_mps, get_player_linear_velocity, get_support_point_linear_velocity +1 | - |
| `perf_locker` | 11 | `_perf_locker` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, main:_prepare_her, matron_view:_pose, matron_view:warm_down +3 | get_matron_state, get_player_position, get_step_timing | matron.cpp, matron_traversal.cpp |
| `touch_birds` | 8 | `_touch_birds` | touch (gestures; `_buttons` too) | birds:flying_count, birds:highest_above, birds:nearest_perched_flock, birds:perched_count, crew_view:_pose +4 | get_player_position, get_surface_below | physics_world.cpp, physics_world_probes.cpp |
| `touch_jump` | 8 | `_touch_jump` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:is_button_shown +3 | get_player_linear_velocity, is_player_crouched, is_player_grounded | - |
| `touch_matron_sweep` | 11 | `_matron_sweep_touch` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:button_label, touch_controls:stick_home, touch_controls:swipe_units | debug_hold_matron_hands, debug_place_matron, debug_set_matron_stage, get_matron_state, get_player_position +2 | matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_found_climb.cpp, matron_hold.cpp, matron_journey.cpp, matron_line.cpp, matron_pipe.cpp +2 |
| `touch_radio` | 8 | `_touch_radio` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `touch_slide` | 0 | `_touch_slide` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_player_linear_velocity, get_player_position, is_player_crouched, is_player_sliding | - |
| `touch_speed_vault` | 3 | `_touch_speed_vault` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_player_linear_velocity, get_player_position, get_traversal_state, is_player_crouched | - |
| `touch_teeter` | 26 | `_teeter` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `touch_vault` | 3 | `_touch_vault` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_accepted_traversal_count, get_player_position, get_traversal_state, is_player_crouched | - |
| `keyboard_crane_magnet` | 8 | `_crane_magnet` | keyboard/mouse events | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_player_linear_velocity +4 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `look_her_chart` | 11 | `_look_her_chart` | none injected (renders/audio/perf) | chart_pages:lay_page, chart_pages:page_sheet, crew_view:_pose, hair_pose_probe:_pose, her_chart:date_line +5 | get_player_position, get_surface_below | physics_world.cpp, physics_world_probes.cpp |
| `look_stock_yard` | 8 | `_look_stock_yard` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home +1 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `look_yard_crane` | 8 | `_look_yard_crane` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home +1 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `pad_crane_magnet` | 8 | `_crane_magnet` | gamepad events | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_player_linear_velocity +4 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_crane_magnet` | 8 | `_crane_magnet` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_player_linear_velocity +4 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_crouch` | 0 | `_touch_crouch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +5 | get_player_position, is_player_crouched | - |
| `touch_ladder` | 11 | `_touch_ladder` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +4 | - |
| `touch_route_hint` | 11 | `_touch_route_hint` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_route_ahead +2 | physics_world_climb.cpp, physics_world_probes.cpp |
| `touch_voice_echo` | 11 | `_touch_voice_echo` | touch (gestures; `_buttons` too) | main:_apply_settings, main:uncanny_profile_for_start, touch_controls:stick_home, uncanny:advance, voice_keeper:_enforce_caps +4 | configure_initial_spawn, get_player_position, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +47 |
| `touch_chute` | 9 | `_touch_chute` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:risers_visible, hair_pose_probe:_pose, matron_view:_pose +3 | get_death_count, get_last_impact_speed_mps, get_player_linear_velocity, is_parachute_deployed, is_player_grounded | - |
| `touch_wall_run` | 41 | `_touch_wall_run` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_player_position, get_player_wall_normal, get_wall_step_count, is_player_crouched, is_player_grounded +1 | - |
| `look_bay` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour +2 | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_catwalk` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour +2 | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_hunt` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour +2 | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_locker` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour +2 | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_matron_stock` | 11 | `_look_matron` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, sky_cycle:set_hour +2 | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +2 | matron.cpp, matron_traversal.cpp |
| `look_tower_stock_176` | 14 | `_look_tower_stock` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home +1 | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_player_linear_velocity +3 | - |
| `look_tower_stock_deck2` | 25 | `_look_tower_stock` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, touch_controls:stick_home +1 | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_player_linear_velocity +3 | - |
| `perf_attrib` | 8 | `_perf_attrib` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, sky_cycle:set_shadow_level +2 | get_carrying_entity_id, get_forklift_state_at, get_landing_state, get_player_linear_velocity, get_player_position +2 | forklift.cpp, physics_world.cpp |
| `perf_attrib_locker` | 11 | `_perf_attrib` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, sky_cycle:set_shadow_level +2 | get_carrying_entity_id, get_forklift_state_at, get_landing_state, get_player_linear_velocity, get_player_position +2 | forklift.cpp, physics_world.cpp |
| `perf_attrib_top` | 42 | `_perf_attrib` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, sky_cycle:set_shadow_level +2 | get_carrying_entity_id, get_forklift_state_at, get_landing_state, get_player_linear_velocity, get_player_position +2 | forklift.cpp, physics_world.cpp |
| `touch_her_chart` | 11 | `_touch_her_chart` | touch (gestures; `_buttons` too) | chart_pages:lay_page, chart_pages:page_sheet, chart_pages:page_text, her_chart:date_line, her_chart:hhmm +4 | get_kit_body_entity_id, get_kit_body_index, get_kit_body_transform, get_loose_stock, get_player_position | - |
| `touch_pause` | 8 | `_touch_pause` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, pause_menu:current_page, pause_menu:page_first_center +7 | configure_initial_spawn, get_tick_index | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +47 |
| `touch_precision` | 3 | `_touch_precision` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_player_linear_velocity, get_player_position, get_precision_jump_count, get_precision_landing_count, get_support_point_linear_velocity +2 | - |
| `touch_climb` | 4 | `_touch_climb` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:planted_count, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center +6 | get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_her_chart_fast` | 11 | `_touch_her_chart_fast` | touch (gestures; `_buttons` too) | chart_pages:page_text, consent_screen:button_center, her_chart:compose, main:uncanny_profile_for_start, story_radio_player:configure +5 | get_kit_body_entity_id, get_kit_body_index, get_kit_body_transform, get_loose_stock, get_player_position | - |
| `touch_hook` | 39 | `_touch_hook` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center +6 | get_hook_state, get_player_position, is_player_crouched, is_player_grounded | - |
| `touch_screens` | 4 | `_touch_screens` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:is_held +5 | get_player_linear_velocity, get_support_point_linear_velocity, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_springboard` | 8 | `_touch_springboard` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_springboard_count +3 | - |
| `touch_stock` | 8 | `_stock_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_player_linear_velocity +3 | - |
| `touch_tuck` | 0 | `_touch_tuck` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +3 | - |
| `touch_wall_kick` | 41 | `_touch_wall_kick` | touch (gestures; `_buttons` too) | touch_controls:button_center, touch_controls:gesture_hint, touch_controls:stick_home, touch_controls:swipe_units | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +6 | - |
| `look_summit` | 42 | `_look_summit` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, summit_view:beam_world_direction +3 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_summit_state +3 | summit.cpp |
| `look_summit_ground` | 8 | `_look_summit` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, sky_cycle:set_hour, summit_view:beam_world_direction +3 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_summit_state +3 | summit.cpp |
| `perf_view_sling` | 8 | `_perf_view_sling` | none injected (renders/audio/perf) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_landing_state, get_matron_state, get_player_linear_velocity, get_player_position +4 | matron.cpp, matron_traversal.cpp |
| `touch_checkpoint` | 26 | `_checkpoint_continuation` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_checkpoint_position, get_death_count, get_landing_state, get_player_linear_velocity +6 | - |
| `touch_hook_rope` | 16 | `_touch_hook_rope` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_hook_state, get_landing_state, get_player_linear_velocity +4 | - |
| `touch_matron` | 11 | `_matron_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +4 | matron.cpp, matron_traversal.cpp |
| `touch_matron_climb` | 11 | `_matron_climb_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +4 | matron.cpp, matron_traversal.cpp |
| `touch_slab_trap` | 11 | `_slab_trap_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_kit_body_transform, get_landing_state, get_loose_stock, get_matron_state +4 | matron.cpp, matron_traversal.cpp |
| `touch_trapeze` | 8 | `_trapeze` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +4 | trapeze.cpp |
| `touch_arm_break` | 11 | `_touch_arm_break` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | debug_launch_player, get_carrying_entity_id, get_death_count, get_death_state, get_landing_state +5 | matron.cpp, matron_fatality.cpp, matron_hold.cpp, matron_moves.cpp, physics_world.cpp, physics_world_arm.cpp, physics_world_climb.cpp, physics_world_grab.cpp +3 |
| `touch_bay` | 11 | `_bay_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_bay_state, get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state +5 | bay.cpp, matron.cpp, matron_traversal.cpp |
| `touch_crane_spreader` | 8 | `_crane_spreader` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_kit_body_index, get_kit_body_transform, get_landing_state, get_loose_stock +5 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_mic_consent` | 8 | `_touch_mic_consent` | touch (gestures; `_buttons` too) | consent_screen:button_center, crew_view:_pose, hair_pose_probe:_pose, main:_apply_settings, main:_show_consent +9 | configure_initial_spawn, get_tick_index, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +47 |
| `touch_photo_id_radio` | 8 | `_touch_photo_id_radio` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, pause_menu:current_page, pause_menu:resume_button_center +6 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +1 | - |
| `touch_pipe_gallery` | 40 | `_touch_pipe_gallery` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:view_fault, hair_pose_probe:_pose +7 | get_player_position, get_traversal_left_hand, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_swing` | 16 | `_swing` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +5 | swing.cpp |
| `touch_wall_climb` | 0 | `_touch_wall_climb` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +3 | - |
| `touch_yard_crane` | 8 | `_yard_crane_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center +5 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_point_linear_velocity +2 | forklift.cpp, physics_world.cpp, yard_crane.cpp |
| `touch_forklift` | 8 | `_forklift_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +3 | get_carrying_entity_id, get_forklift_state, get_kit_body_index, get_kit_body_transform, get_landing_state +5 | forklift.cpp, physics_world.cpp |
| `touch_forklift_ram` | 11 | `_forklift_ram_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_forklift_state, get_kit_body_index, get_kit_body_transform, get_landing_state +6 | forklift.cpp, matron.cpp, matron_traversal.cpp, physics_world.cpp |
| `touch_forklift_strain` | 8 | `_forklift_strain_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +3 | get_carrying_entity_id, get_forklift_state, get_kit_body_index, get_kit_body_transform, get_landing_state +5 | forklift.cpp, physics_world.cpp |
| `touch_matron_ride` | 11 | `_matron_ride_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_matron_state, get_player_linear_velocity +6 | matron.cpp, matron_traversal.cpp |
| `touch_mission_1` | 8 | `_mission_1` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `touch_mission_7` | 16 | `_mission_7` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +6 | - |
| `touch_summit_ending` | 42 | `_summit_ending` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:is_button_shown +3 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +5 | summit.cpp |
| `touch_hang_climb` | 5 | `_touch_hang` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:planted_count, first_person_arms:view_fault +9 | get_player_linear_velocity, get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_hang_drop` | 5 | `_touch_hang` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:planted_count, first_person_arms:view_fault +9 | get_player_linear_velocity, get_player_position, get_traversal_state, is_player_crouched, is_player_grounded | - |
| `touch_matron_heave` | 11 | `_matron_heave_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:gesture_hint +3 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +6 | matron.cpp, matron_traversal.cpp |
| `touch_matron_line` | 11 | `_matron_line_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_death_state, get_landing_state, get_matron_state +7 | matron.cpp, matron_traversal.cpp, zipline.cpp |
| `touch_monkey_bars` | 39 | `_touch_monkey_bars` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, first_person_arms:view_fault, hair_pose_probe:_pose +8 | get_player_linear_velocity, get_player_position, get_traversal_left_hand, get_traversal_state, is_player_crouched +1 | - |
| `touch_roll` | 8 | `_roll` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, main:_apply_settings, main:uncanny_profile_for_start, matron_view:_pose +4 | configure_initial_spawn, get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity +5 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +47 |
| `touch_roll_reduced` | 8 | `_roll` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, main:_apply_settings, main:uncanny_profile_for_start, matron_view:_pose +4 | configure_initial_spawn, get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity +5 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +47 |
| `touch_tram` | 27 | `_tram` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +9 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_twin` | 16 | `_twin` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_hook_state, get_kit_body_index, get_kit_body_transform +7 | - |
| `touch_zipline_slam` | 25 | `_touch_zipline_slam` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_corpses, get_death_count, get_death_state, get_landing_state +7 | corpse.cpp, zipline.cpp |
| `keyboard_boom` | 12 | `_boom` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `keyboard_rig` | 11 | `_rig` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `pad_boom` | 12 | `_boom` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `pad_rig` | 11 | `_rig` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `touch_app_leave_fall` | 9 | `_touch_app_leave_fall` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, launch_shot:time_scale, main:_apply_settings, main:_process +10 | configure_initial_spawn, get_save_game, get_tick_index, is_parachute_deployed, place_matron_home | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +49 |
| `touch_boom` | 12 | `_boom` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `touch_incline` | 8 | `_incline` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:stick_home, zipline_view:_pose | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +10 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_rig` | 11 | `_rig` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +6 | - |
| `touch_wheel` | 25 | `_wheel` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:stick_home +2 | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +8 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `keyboard_debris` | 13 | `_debris` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +7 | - |
| `pad_debris` | 13 | `_debris` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +7 | - |
| `touch_debris` | 13 | `_debris` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position, get_support_entity_id +7 | - |
| `touch_machines` | 36 | `_machines` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center +3 | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +8 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_matron_ammo` | 11 | `_matron_ammo_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:gesture_hint +3 | get_carrying_entity_id, get_death_count, get_death_state, get_kit_body_index, get_kit_body_transform +8 | matron.cpp, matron_traversal.cpp |
| `touch_slingshot` | 8 | `_slingshot` | touch (gestures; `_buttons` too) | canopy_view:open_fraction, crew_view:_pose, hair_pose_probe:_pose, launch_shot:active, launch_shot:look_weight +6 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +5 | - |
| `touch_summit` | 42 | `_summit` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +5 | get_carrying_entity_id, get_death_count, get_landing_state, get_last_impact_speed_mps, get_player_linear_velocity +6 | summit.cpp |
| `touch_c5` | 33 | `_c5` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `touch_c6` | 35 | `_c6` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `keyboard_service` | 30 | `_service` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `pad_service` | 30 | `_service` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_c4` | 31 | `_c4` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +5 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +8 | - |
| `touch_c7` | 36 | `_c7` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_lift_state, get_player_linear_velocity +9 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_high` | 37 | `_high` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center +3 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +10 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_n` | 32 | `_n` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_o` | 34 | `_o` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_service` | 30 | `_service` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `keyboard_shop` | 19 | `_shop` | keyboard/mouse events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose +5 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `pad_shop` | 19 | `_shop` | gamepad events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose +5 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_shop` | 19 | `_shop` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose +5 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +9 | - |
| `touch_zipline` | 25 | `_touch_zipline` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:anchored_error, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose +7 | get_carrying_entity_id, get_death_count, get_landing_state, get_player_linear_velocity, get_player_position +7 | zipline.cpp |
| `keyboard_crane` | 22 | `_crane` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +11 | - |
| `keyboard_wet` | 16 | `_wet` | keyboard/mouse events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +11 | - |
| `pad_crane` | 22 | `_crane` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +11 | - |
| `pad_wet` | 16 | `_wet` | gamepad events | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +11 | - |
| `touch_crane` | 22 | `_crane` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +11 | - |
| `touch_wet` | 16 | `_wet` | touch (gestures; `_buttons` too) | crew_view:_pose, first_person_arms:hand_poses, first_person_arms:wrist_positions, hair_pose_probe:_pose, matron_view:_pose +4 | get_carrying_entity_id, get_death_count, get_kit_body_index, get_kit_body_transform, get_landing_state +11 | - |
| `look_matron_fatal` | 11 | `_look_set` | none injected (renders/audio/perf) | consent_screen:close, consent_screen:open, matron_hold_camera:cone_cast, matron_rig:bone_world, matron_view:head_world_position +6 | debug_hold_matron_hands, debug_launch_player, debug_lose_arm, debug_matron_climb_way, debug_matron_hear +10 | loose_stock.cpp, matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_fatality.cpp, matron_found_climb.cpp, matron_hoist.cpp, matron_hold.cpp +16 |
| `look_matron_fight` | 11 | `_look_set` | none injected (renders/audio/perf) | consent_screen:close, consent_screen:open, matron_hold_camera:cone_cast, matron_rig:bone_world, matron_view:head_world_position +6 | debug_hold_matron_hands, debug_launch_player, debug_lose_arm, debug_matron_climb_way, debug_matron_hear +10 | loose_stock.cpp, matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_fatality.cpp, matron_found_climb.cpp, matron_hoist.cpp, matron_hold.cpp +16 |
| `look_matron_grade_ladder` | 11 | `_look_set` | none injected (renders/audio/perf) | consent_screen:close, consent_screen:open, matron_hold_camera:cone_cast, matron_rig:bone_world, matron_view:head_world_position +6 | debug_hold_matron_hands, debug_launch_player, debug_lose_arm, debug_matron_climb_way, debug_matron_hear +10 | loose_stock.cpp, matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_fatality.cpp, matron_found_climb.cpp, matron_hoist.cpp, matron_hold.cpp +16 |
| `look_matron_hold` | 11 | `_look_set` | none injected (renders/audio/perf) | consent_screen:close, consent_screen:open, matron_hold_camera:cone_cast, matron_rig:bone_world, matron_view:head_world_position +6 | debug_hold_matron_hands, debug_launch_player, debug_lose_arm, debug_matron_climb_way, debug_matron_hear +10 | loose_stock.cpp, matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_fatality.cpp, matron_found_climb.cpp, matron_hoist.cpp, matron_hold.cpp +16 |
| `look_matron_moves` | 11 | `_look_set` | none injected (renders/audio/perf) | consent_screen:close, consent_screen:open, matron_hold_camera:cone_cast, matron_rig:bone_world, matron_view:head_world_position +6 | debug_hold_matron_hands, debug_launch_player, debug_lose_arm, debug_matron_climb_way, debug_matron_hear +10 | loose_stock.cpp, matron.cpp, matron_climb.cpp, matron_contact.cpp, matron_fatality.cpp, matron_found_climb.cpp, matron_hoist.cpp, matron_hold.cpp +16 |
| `touch_matron_pipe` | 11 | `_matron_pipe_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, matron_view:head_world_position, touch_controls:button_center +4 | get_carry_target_entity_id, get_carrying_entity_id, get_death_count, get_death_state, get_kit_body_index +12 | matron.cpp, matron_traversal.cpp, zipline.cpp |
| `keyboard_stack_upper` | 26 | `_stack_upper` | keyboard/mouse events | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_loose_stock, get_player_linear_velocity +13 | - |
| `pad_stack_upper` | 26 | `_stack_upper` | gamepad events | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_loose_stock, get_player_linear_velocity +13 | - |
| `touch_app_leave_slingshot` | 8 | `_touch_app_leave_slingshot` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, launch_shot:time_scale, main:_apply_settings, main:_process +11 | configure_initial_spawn, get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position +6 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +49 |
| `touch_stack_upper` | 26 | `_stack_upper` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, matron_view:_pose, touch_controls:button_center, touch_controls:button_label +4 | get_carrying_entity_id, get_death_count, get_landing_state, get_loose_stock, get_player_linear_velocity +13 | - |
| `touch_app_leave_ride` | 25 | `_touch_app_leave_ride` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, launch_shot:time_scale, main:_apply_settings, main:_process +11 | configure_initial_spawn, get_carrying_entity_id, get_landing_state, get_player_linear_velocity, get_player_position +7 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +49 |
| `touch_save` | 26 | `_save` | touch (gestures; `_buttons` too) | consent_screen:close, consent_screen:open, crew_view:_pose, hair_pose_probe:_pose, main:_save_climb +13 | get_carrying_entity_id, get_checkpoint_position, get_death_count, get_landing_state, get_player_linear_velocity +7 | - |
| `touch_matron_act` | 11 | `_matron_act_touch` | touch (gestures; `_buttons` too) | crew_view:_pose, gore_victim:held_ankle_drawn, hair_pose_probe:_pose, her_takes:in_ring, matron_hold_camera:black +12 | get_carrying_entity_id, get_death_count, get_death_state, get_kit_body_index, get_kit_body_transform +12 | matron.cpp, matron_traversal.cpp, zipline.cpp |
| `touch_app_leave_hold` | 11 | `_touch_app_leave_hold` | touch (gestures; `_buttons` too) | crew_view:_pose, hair_pose_probe:_pose, launch_shot:time_scale, main:_apply_settings, main:_process +14 | configure_initial_spawn, get_carrying_entity_id, get_death_count, get_death_state, get_kit_body_index +16 | band_counterweight_well.cpp, band_facade_crane.cpp, band_midstack_service.cpp, band_plate_shop.cpp, band_stack.cpp, band_wet_isolation.cpp, bay.cpp, climb_c6.cpp +50 |
| `keyboard_stack` | 8 | `_stack` | keyboard/mouse events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose +7 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +35 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `pad_stack` | 8 | `_stack` | gamepad events | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose +7 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +35 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_stack` | 8 | `_stack` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose +7 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +35 | vertical/vertical_machine.cpp, vertical/vertical_route.cpp |
| `touch_stack_ending` | 8 | `_stack_ending` | touch (gestures; `_buttons` too) | crew_view:_pose, crew_view:burning_count, first_person_arms:hand_poses, hair_pose_probe:_pose, matron_view:_pose +7 | get_carrying_entity_id, get_crane_state, get_death_count, get_kit_body_index, get_kit_body_transform +36 | summit.cpp, vertical/vertical_machine.cpp, vertical/vertical_route.cpp |

## 6. Which `src/sim` files the touch scenarios run

**N**: the scenario calls a bridge method whose body runs the file (cut at the per-tick step). **O**: the scenario reads a bridge value the file's code writes, so the file runs in its ticks. Every scenario that advances the world also ticks everything; files with neither column set are reached by ticks and world building alone (`tick only`).

| `src/sim` file | **N**: scenarios whose bridge calls run it | **O**: scenarios that read a bridge value this file writes (it runs in their ticks) | N/O counts |
|---|---|---|---|
| `src/sim/band_counterweight_well.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/band_facade_crane.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +12 more | 11/18 |
| `src/sim/band_midstack_service.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +14 more | 11/20 |
| `src/sim/band_plate_shop.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +12 more | 11/18 |
| `src/sim/band_stack.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_checkpoint`, `touch_mic_consent` +15 more | 11/21 |
| `src/sim/band_wet_isolation.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_mission_7` +13 more | 11/19 |
| `src/sim/bay.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_bay`, `touch_mic_consent` +6 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +34 more | 12/40 |
| `src/sim/climb_c6.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/climb_c7.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/corpse.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_lethal_feedback`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent` +7 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_lethal_feedback`, `touch_voice_echo`, `touch_pause`, `touch_arm_break` +13 more | 13/19 |
| `src/sim/crew.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +35 more | 11/41 |
| `src/sim/fabricated.cpp` | - | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_arm_break`, `touch_mic_consent` +11 more | 0/17 |
| `src/sim/forklift.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_voice_echo` +16 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `keyboard_crane_magnet` +27 more | 22/33 |
| `src/sim/impacts.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/loose_stock.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +10 more | `look_finishes`, `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep` +32 more | 16/38 |
| `src/sim/matron.cpp` | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +35 more | `touch_app_leave_title`, `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery` +41 more | 41/47 |
| `src/sim/matron_climb.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +13 more | 18/19 |
| `src/sim/matron_contact.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +38 more | 18/44 |
| `src/sim/matron_fatality.cpp` | `touch_arm_break`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +13 more | 6/19 |
| `src/sim/matron_found_climb.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `look_her_chart` +16 more | 18/22 |
| `src/sim/matron_hoist.cpp` | `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `look_her_chart` +16 more | 5/22 |
| `src/sim/matron_hold.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +13 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `look_her_chart` +16 more | 19/22 |
| `src/sim/matron_journey.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_route_hint`, `touch_voice_echo` +14 more | 18/20 |
| `src/sim/matron_line.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +45 more | 18/51 |
| `src/sim/matron_moves.cpp` | `touch_arm_break`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `look_her_chart` +16 more | 6/22 |
| `src/sim/matron_perception.cpp` | - | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +13 more | 0/19 |
| `src/sim/matron_pipe.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +38 more | 18/44 |
| `src/sim/matron_routecraft.cpp` | `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +38 more | 5/44 |
| `src/sim/matron_shape.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +36 more | 11/42 |
| `src/sim/matron_stalk.cpp` | - | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +13 more | 0/19 |
| `src/sim/matron_travel.cpp` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +12 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_voice_echo`, `touch_pause` +13 more | 18/19 |
| `src/sim/matron_traversal.cpp` | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +25 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +38 more | 31/44 |
| `src/sim/mechanism_kit.cpp` | `touch_app_leave_title`, `look_finishes`, `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet` +68 more | `touch_app_leave_title`, `look_finishes`, `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds` +69 more | 74/75 |
| `src/sim/objectives.cpp` | - | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_checkpoint`, `touch_mic_consent` +45 more | 0/51 |
| `src/sim/physics_world.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `keyboard_crane_magnet`, `look_her_chart`, `pad_crane_magnet` +24 more | `touch_app_leave_title`, `perf_view_bottom`, `perf_view_top`, `look_finishes`, `perf_fight`, `touch_room_hearing` +96 more | 30/102 |
| `src/sim/physics_world_arm.cpp` | `touch_arm_break`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +48 more | 6/54 |
| `src/sim/physics_world_climb.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_route_hint`, `touch_voice_echo`, `touch_pause`, `touch_arm_break` +12 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `keyboard_crane_magnet` +42 more | 18/48 |
| `src/sim/physics_world_fabricated.cpp` | `touch_app_leave_title`, `touch_app_leave_fall`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold` +4 more | `touch_app_leave_title`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `keyboard_crane_magnet`, `look_her_chart` +26 more | 10/32 |
| `src/sim/physics_world_fatality.cpp` | - | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +58 more | 0/64 |
| `src/sim/physics_world_grab.cpp` | `touch_arm_break`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +58 more | 6/64 |
| `src/sim/physics_world_movement.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `keyboard_crane_magnet` +37 more | 11/43 |
| `src/sim/physics_world_one_arm.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `keyboard_crane_magnet`, `look_her_chart`, `pad_crane_magnet` +35 more | 11/41 |
| `src/sim/physics_world_probes.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `look_her_chart`, `touch_route_hint`, `touch_voice_echo` +14 more | `perf_view_bottom`, `perf_view_top`, `look_finishes`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery` +67 more | 20/73 |
| `src/sim/physics_world_state.cpp` | `touch_app_leave_title`, `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent` +6 more | most scenarios (134) | 12/134 |
| `src/sim/physics_world_strike.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery`, `perf_walk` +61 more | 11/67 |
| `src/sim/physics_world_traversal.cpp` | `touch_arm_break`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `keyboard_crane_magnet` +42 more | 6/48 |
| `src/sim/physics_world_water.cpp` | - | `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `keyboard_crane_magnet`, `look_her_chart`, `pad_crane_magnet` +35 more | 0/41 |
| `src/sim/pipe_gallery.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/rope_constraint.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/shared_survival.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `look_her_chart` +15 more | 11/21 |
| `src/sim/simulation.cpp` | every scenario that reads state (154 of 162) | `touch_app_leave_title`, `pad_core`, `perf_view_bottom`, `perf_view_top`, `look_finishes`, `perf_fight` +114 more | 154/120 |
| `src/sim/slab_cell.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/slingshot.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_voice_echo` +20 more | 11/26 |
| `src/sim/slingshot_model.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/stock_damage.cpp` | `touch_app_leave_title`, `touch_app_leave_fall`, `touch_app_leave_slingshot`, `touch_app_leave_ride`, `touch_app_leave_hold` | `touch_app_leave_title`, `perf_view_bottom`, `perf_view_top`, `perf_fight`, `touch_room_hearing`, `look_matron_gallery` +29 more | 5/35 |
| `src/sim/summit.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `look_summit`, `look_summit_ground` +15 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_birds`, `touch_matron_sweep`, `look_her_chart` +20 more | 21/26 |
| `src/sim/swing.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_swing` +6 more | `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_voice_echo` +20 more | 12/26 |
| `src/sim/trapeze.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_trapeze`, `touch_mic_consent` +6 more | `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_voice_echo` +12 more | 12/18 |
| `src/sim/twin_tower.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/vertical/barrel_helix.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/cascade_mast.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/crown_gondola.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/gravity_balance.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/luffing_derrick.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/pitman_lift.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_arm_break`, `touch_mic_consent` +11 more | 11/17 |
| `src/sim/vertical/rack_climber.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/vertical/slab_incline.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/stone_wheel.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | 11/21 |
| `src/sim/vertical/traction_tram.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +5 more | 11/11 |
| `src/sim/vertical/vertical_machine.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | `touch_app_leave_title`, `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent` +16 more | 21/22 |
| `src/sim/vertical/vertical_route.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent`, `touch_roll` +15 more | `touch_app_leave_title`, `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_mic_consent` +16 more | 21/22 |
| `src/sim/yard_crane.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_voice_echo` +10 more | `touch_room_hearing`, `touch_gyro_aim`, `keyboard_crane_magnet`, `pad_crane_magnet`, `touch_crane_magnet`, `touch_voice_echo` +10 more | 16/16 |
| `src/sim/zipline.cpp` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_pause`, `touch_arm_break`, `touch_mic_consent` +16 more | `perf_fight`, `touch_room_hearing`, `touch_gyro_aim`, `touch_matron_sweep`, `touch_route_hint`, `touch_voice_echo` +25 more | 22/31 |

## 7. Presentation files

`Per-frame core` counts the file's functions reachable from engine callbacks (`_ready`, `_process`, `_input`, ...) of the game scripts: they run in every scenario. `Scenarios that call into it` are the scenarios whose own code (or the chain of game functions it starts) calls a function in the file that is not in the per-frame core. `no scenario or test reaches it` means no static call path from a scenario or an engine callback; signals and scene wiring are invisible to this, so check `grep -rn <file stem> godot/` before treating it as dead.

| File | Lines | Functions run by a scenario | Per-frame core | Scenarios that call into it (narrowest first) | Headless tests | Verdict |
|---|---|---|---|---|---|---|
| `audio/audio_director.gd` | 1986 | 72 | 68/72 | `audio_mix`, `audio_fall` | footstep_contact_test.gd, hold_mix_leak_probe.gd, matron_speech_test.gd, sound_bank_test.gd | scenario-specific |
| `audio/her_takes.gd` | 184 | 12 | 12/12 | `touch_matron_act` | her_takes_test.gd, sound_bank_test.gd | scenario-specific |
| `audio/matron_sound.gd` | 606 | 12 | 12/12 | - | fight_s1_sound_dedupe_test.gd, fight_s1_test.gd, matron_speech_test.gd | per-frame core only (every scenario) |
| `audio/matron_speech.gd` | 107 | 6 | 6/6 | - | matron_speech_test.gd | per-frame core only (every scenario) |
| `audio/matron_voice_lines.gd` | 113 | 0 | 0/0 | - | matron_speech_test.gd | per-frame core only (every scenario) |
| `audio/sound_bank.gd` | 1529 | 86 | 1/86 | - | sound_bank_test.gd | per-frame core only (every scenario) |
| `audio/story_radio_player.gd` | 285 | 23 | 9/23 | `touch_her_chart_fast`, `touch_photo_id_radio` | - | scenario-specific |
| `audio/voice_lines.gd` | 137 | 0 | 0/0 | - | - | preloaded by audio_director.gd, ui_test_moves.gd |
| `canopy_view.gd` | 154 | 6 | 5/6 | `touch_slingshot` | - | scenario-specific |
| `cloud_layer.gd` | 169 | 6 | 4/6 | `touch_clouds` | - | scenario-specific |
| `corpse_view.gd` | 224 | 8 | 7/8 | `touch_lethal_feedback` | - | scenario-specific |
| `crane_view.gd` | 110 | 4 | 4/4 | - | - | per-frame core only (every scenario) |
| `detail_kit.gd` | 559 | 18 | 5/18 | - | - | per-frame core only (every scenario) |
| `error_reporter.gd` | 234 | 13 | 12/13 | - | error_reporter_test.gd | per-frame core only (every scenario) |
| `fabricated_view.gd` | 118 | 8 | 3/8 | - | - | per-frame core only (every scenario) |
| `far_world.gd` | 399 | 6 | 6/6 | - | far_world_test.gd | per-frame core only (every scenario) |
| `first_person_arms.gd` | 1012 | 35 | 28/35 | `touch_chute`, `touch_climb`, `touch_hook`, `touch_pipe_gallery`, `touch_yard_crane`, `touch_hang_climb` +32 more | - | scenario-specific |
| `first_person_body.gd` | 441 | 8 | 7/8 | `touch_body` | - | scenario-specific |
| `floating_audit.gd` | 636 | 31 | 31/31 | - | - | per-frame core only (every scenario) |
| `forklift_view.gd` | 229 | 9 | 9/9 | - | - | per-frame core only (every scenario) |
| `gait.gd` | 40 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `identity/photo_id_intake.gd` | 62 | 5 | 5/5 | - | - | per-frame core only (every scenario) |
| `identity/player_identity_store.gd` | 320 | 22 | 19/22 | - | - | per-frame core only (every scenario) |
| `identity/portrait_calibration.gd` | 92 | 8 | 5/8 | - | - | per-frame core only (every scenario) |
| `identity/portrait_face_fit.gd` | 188 | 10 | 0/10 | - | - | only called from code no scenario reaches |
| `identity/work_id_page.gd` | 111 | 10 | 10/10 | `touch_her_chart_fast` | - | scenario-specific |
| `lamp_glow.gd` | 193 | 5 | 5/5 | - | - | per-frame core only (every scenario) |
| `launch_shot.gd` | 77 | 5 | 5/5 | `touch_app_leave_fall`, `touch_slingshot`, `touch_app_leave_slingshot`, `touch_app_leave_ride`, `touch_app_leave_hold` | - | scenario-specific |
| `life/birds.gd` | 523 | 23 | 19/23 | `touch_birds` | - | scenario-specific |
| `life/climber/climber_body_rig.gd` | 18 | 0 | 0/0 | - | - | preloaded by first_person_body.gd |
| `life/crew_view.gd` | 501 | 22 | 19/22 | `perf_grade`, `perf_top`, `look_night_climb`, `look_night_lamps`, `touch_clouds`, `perf_view_bottom` +130 more | - | core |
| `life/foreman_radio.gd` | 147 | 6 | 6/6 | - | foreman_radio_test.gd | per-frame core only (every scenario) |
| `life/gore/blood_fx.gd` | 263 | 10 | 10/10 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_body.gd` | 112 | 8 | 8/8 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_kit.gd` | 749 | 13 | 13/13 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_limb.gd` | 70 | 3 | 2/3 | - | - | per-frame core only (every scenario) |
| `life/gore/gore_victim.gd` | 305 | 15 | 14/15 | `touch_matron_act` | - | scenario-specific |
| `life/gore/gore_view.gd` | 425 | 14 | 13/14 | - | gore_shots_test.gd | per-frame core only (every scenario) |
| `life/gore/gore_wound.gd` | 206 | 7 | 5/7 | - | - | per-frame core only (every scenario) |
| `life/matron/matron_injury.gd` | 732 | 34 | 28/34 | - | - | per-frame core only (every scenario) |
| `life/matron_hold_camera.gd` | 698 | 18 | 15/18 | `touch_app_leave_fall`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves` +4 more | hold_beats_probe.gd, rip_shot_probe.gd | scenario-specific |
| `life/matron_look.gd` | 257 | 8 | 8/8 | - | matron_figure_test.gd | per-frame core only (every scenario) |
| `life/matron_rig.gd` | 1914 | 55 | 53/55 | `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder`, `look_matron_hold`, `look_matron_moves`, `touch_matron_act` +1 more | - | scenario-specific |
| `life/matron_skirt.gd` | 529 | 9 | 8/9 | - | - | per-frame core only (every scenario) |
| `life/matron_view.gd` | 2668 | 61 | 56/61 | `perf_grade`, `perf_top`, `look_night_climb`, `look_night_lamps`, `touch_clouds`, `perf_view_bottom` +135 more | fight_s1_test.gd, gore_shots_test.gd, hair_pose_probe.gd, injury_shots_test.gd, matron_figure_test.gd, matron_foot_plant_test.gd, matron_perception_climb_test.gd, matron_presentation_test.gd | core |
| `life/strike_feel.gd` | 104 | 4 | 4/4 | - | - | per-frame core only (every scenario) |
| `life/works.gd` | 272 | 14 | 10/14 | `touch_works` | - | scenario-specific |
| `main.gd` | 1321 | 36 | 35/36 | `touch_app_leave_title`, `touch_room_hearing`, `touch_gyro_aim`, `perf_locker`, `touch_voice_echo`, `touch_her_chart_fast` +8 more | - | scenario-specific |
| `main_context.gd` | 662 | 10 | 10/10 | - | - | per-frame core only (every scenario) |
| `main_feel.gd` | 1594 | 33 | 32/33 | - | fight_s1_test.gd, matron_hold_haptics_test.gd | per-frame core only (every scenario) |
| `main_geometry.gd` | 1179 | 27 | 27/27 | - | - | per-frame core only (every scenario) |
| `main_machines.gd` | 1461 | 34 | 34/34 | - | - | per-frame core only (every scenario) |
| `main_world.gd` | 1087 | 23 | 23/23 | - | far_world_test.gd | per-frame core only (every scenario) |
| `mist_streak.gd` | 125 | 6 | 5/6 | `look_mist_streak` | - | scenario-specific |
| `mountain_mesh.gd` | 232 | 5 | 5/5 | - | far_world_test.gd | per-frame core only (every scenario) |
| `plants.gd` | 305 | 12 | 12/12 | - | - | per-frame core only (every scenario) |
| `sky_cycle.gd` | 312 | 13 | 11/13 | `perf_night`, `perf_grade`, `perf_top`, `look_night_climb`, `look_night_lamps`, `look_finishes` +21 more | - | scenario-specific |
| `slingshot_view.gd` | 288 | 15 | 15/15 | - | - | per-frame core only (every scenario) |
| `solid_export.gd` | 297 | 11 | 11/11 | - | far_world_test.gd | per-frame core only (every scenario) |
| `stock_view.gd` | 122 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `summit_view.gd` | 308 | 9 | 7/9 | `look_summit`, `look_summit_ground` | - | scenario-specific |
| `swing_view.gd` | 164 | 7 | 7/7 | - | - | per-frame core only (every scenario) |
| `throw_arc_view.gd` | 86 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `ui/consent_screen.gd` | 119 | 7 | 6/7 | `look_mist_streak`, `touch_her_chart_fast`, `touch_mic_consent`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder` +3 more | - | scenario-specific |
| `ui/hud.gd` | 631 | 25 | 25/25 | `touch_lethal_feedback` | - | scenario-specific |
| `ui/input_router.gd` | 368 | 17 | 17/17 | - | - | per-frame core only (every scenario) |
| `ui/licences.gd` | 182 | 1 | 1/1 | - | - | per-frame core only (every scenario) |
| `ui/pause_menu.gd` | 817 | 40 | 33/40 | `look_mist_streak`, `touch_pause`, `touch_mic_consent`, `touch_photo_id_radio`, `touch_app_leave_fall`, `look_matron_fatal` +8 more | - | scenario-specific |
| `ui/punch_list.gd` | 36 | 1 | 1/1 | - | punch_list_test.gd | per-frame core only (every scenario) |
| `ui/settings_store.gd` | 234 | 8 | 8/8 | `touch_pause` | fight_s1_test.gd, matron_hold_haptics_test.gd | scenario-specific |
| `ui/title_screen.gd` | 168 | 10 | 7/10 | `touch_app_leave_title`, `look_mist_streak`, `touch_title`, `look_matron_fatal`, `look_matron_fight`, `look_matron_grade_ladder` +3 more | - | scenario-specific |
| `ui/touch_controls.gd` | 841 | 34 | 28/34 | `touch_quickturn`, `perf_fight`, `touch_room_hearing`, `perf_walk`, `touch_body`, `touch_double_tap_vault` +135 more | - | core |
| `ui/ui_style.gd` | 484 | 25 | 24/25 | `touch_screens` | foreman_radio_test.gd | scenario-specific |
| `uncanny/attention.gd` | 184 | 6 | 5/6 | - | - | per-frame core only (every scenario) |
| `uncanny/chart_pages.gd` | 376 | 18 | 18/18 | `look_her_chart`, `touch_her_chart`, `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/her_chart.gd` | 455 | 27 | 18/27 | `look_her_chart`, `touch_her_chart`, `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/page_reader.gd` | 99 | 6 | 5/6 | `look_her_chart`, `touch_her_chart` | - | scenario-specific |
| `uncanny/tower_clocks.gd` | 186 | 8 | 7/8 | `touch_her_chart` | - | scenario-specific |
| `uncanny/uncanny.gd` | 313 | 14 | 13/14 | `look_her_chart`, `touch_voice_echo`, `touch_her_chart`, `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/uncanny_profile.gd` | 52 | 1 | 1/1 | `touch_her_chart_fast` | - | scenario-specific |
| `uncanny/voice_keeper.gd` | 685 | 31 | 26/31 | `touch_room_hearing`, `touch_voice_echo`, `touch_mic_consent`, `touch_app_leave_fall`, `touch_app_leave_slingshot`, `touch_app_leave_ride` +1 more | - | scenario-specific |
| `water_feel.gd` | 135 | 3 | 3/3 | - | - | per-frame core only (every scenario) |
| `yard_ground.gd` | 109 | 3 | 1/3 | - | - | per-frame core only (every scenario) |
| `zipline_view.gd` | 211 | 8 | 7/8 | `perf_grade`, `perf_top`, `look_night_climb`, `look_night_lamps`, `touch_clouds`, `perf_view_bottom` +130 more | - | core |

## 8. Big presentation files, function by function

### `audio/audio_director.gd` (1986 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 241-328 | `_ready` | - | yes |
| 382-408 | `_on_bank_ready` | - | no |
| 464-526 | `update` | - | yes |
| 527-556 | `_update_breath` | - | yes |
| 557-599 | `update_machines` | - | yes |
| 619-654 | `update_impacts` | - | yes |
| 661-691 | `_update_air` | - | yes |
| 692-739 | `_update_voice` | - | yes |
| 748-780 | `_voice_say` | - | yes |
| 893-927 | `worker_weld` | - | yes |
| 946-1032 | `update_forklift` | - | yes |
| 1041-1092 | `update_crane` | - | yes |
| 1093-1120 | `crane_cue` | - | yes |
| 1301-1339 | `matron_say` | - | yes |
| 1340-1416 | `_her_cue` | - | yes |
| 1524-1607 | `matron_layers` | - | yes |
| 1617-1653 | `_her_level` | - | yes |
| 1848-1897 | `update_bay` | - | yes |
| 1912-1947 | `update_wet_e` | - | yes |

### `audio/matron_sound.gd` (606 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 177-389 | `update` | - | yes |
| 390-522 | `hear_hold` | - | yes |

### `audio/sound_bank.gd` (1529 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 33-135 | `build` | - | no |
| 151-178 | `build_her` | - | no |
| 376-401 | `_drip` | - | no |
| 418-443 | `_creak` | - | no |
| 794-823 | `_her_bed` | - | no |
| 1227-1252 | `_resonate` | - | no |
| 1257-1284 | `_wav` | - | no |
| 1304-1343 | `_glottal` | - | no |
| 1344-1385 | `_formants` | - | no |
| 1441-1468 | `_add_glide` | - | no |

### `first_person_arms.gd` (1012 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 226-285 | `_build_hand` | - | yes |
| 309-345 | `_build_remote` | - | yes |
| 377-569 | `update_arms` | - | yes |
| 608-744 | `_pose_target` | - | yes |
| 783-808 | `_solve_arm` | - | yes |
| 940-986 | `_strike_target` | - | yes |
| 987-1013 | `_sweep_target` | - | yes |

### `floating_audit.gd` (636 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 108-162 | `run` | - | yes |
| 163-188 | `_text_off_boards` | - | yes |
| 189-218 | `_board_of` | - | yes |
| 266-293 | `_gather_kit` | - | yes |
| 294-327 | `_gather_views` | - | yes |
| 335-364 | `_gather_lights_and_labels` | - | yes |
| 365-391 | `_add` | - | yes |
| 461-503 | `_overlap` | - | yes |
| 541-568 | `_settle` | - | yes |
| 601-632 | `_report` | - | yes |

### `life/gore/gore_kit.gd` (749 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 409-447 | `material` | - | yes |
| 513-549 | `_skin_rim` | - | yes |
| 550-625 | `_meat` | - | yes |
| 626-679 | `_bone` | - | yes |
| 680-736 | `ribcage` | - | yes |

### `life/matron/matron_injury.gd` (732 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 161-201 | `_cheek_texture` | - | yes |
| 202-238 | `_ring_texture` | - | yes |
| 392-515 | `_skin_mesh_for` | - | yes |
| 574-632 | `update` | - | yes |
| 633-680 | `_update_eye` | - | yes |
| 681-733 | `_update_cheek` | - | yes |

### `life/matron_hold_camera.gd` (698 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 225-565 | `frame` | - | yes |

### `life/matron_rig.gd` (1914 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 97-131 | `setup` | - | yes |
| 149-186 | `_place_pivots` | - | yes |
| 200-257 | `apply` | - | yes |
| 447-503 | `_reach_chain` | - | yes |
| 508-560 | `_follow_hand` | - | yes |
| 604-684 | `_plant_limbs` | - | yes |
| 768-1136 | `_plant_upright_feet` | - | yes |
| 1137-1165 | `hold_reach` | - | yes |
| 1193-1251 | `contact` | - | yes |
| 1252-1281 | `_finish_prop_wrist` | - | yes |
| 1282-1341 | `govern` | - | yes |
| 1356-1579 | `_contact` | - | yes |
| 1615-1694 | `step_hair` | - | yes |
| 1710-1741 | `_segment_outside` | - | yes |
| 1757-1789 | `_clear_ring` | - | yes |
| 1873-1915 | `_clear_front` | - | yes |

### `life/matron_view.gd` (2668 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 297-326 | `_build_sparks` | - | yes |
| 413-440 | `_merge_leaves` | - | yes |
| 448-552 | `_build_locker` | - | yes |
| 563-622 | `_update_locker` | - | yes |
| 661-696 | `_build_planks` | - | yes |
| 730-756 | `_update_planks` | - | yes |
| 788-845 | `_build_pivots` | - | yes |
| 846-905 | `_seat_hair` | - | yes |
| 906-963 | `_hair_onto_chains` | - | yes |
| 964-992 | `_load_body` | - | yes |
| 1017-1048 | `_fours_gate` | - | yes |
| 1049-1098 | `_hold_gate` | - | yes |
| 1120-1161 | `_turn_yaw` | - | yes |
| 1205-1242 | `_drive_body` | - | yes |
| 1243-1294 | `_ease_pose` | - | yes |
| 1295-1320 | `_ease_figure` | - | yes |
| 1341-1366 | `_step_bursts` | - | yes |
| 1396-2346 | `_pose` | `perf_grade`, `perf_top`, `look_night_climb`, `look_night_lamps`, `touch_clouds`, `perf_view_bottom` +130 more | yes |
| 2347-2375 | `_topple` | - | yes |
| 2385-2501 | `_update_figure` | - | yes |
| 2640-2669 | `_apply` | - | yes |

### `main.gd` (1321 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 138-373 | `_ready` | - | yes |
| 376-408 | `_prepare_her` | `perf_locker` | no |
| 409-628 | `_process` | `touch_app_leave_fall`, `touch_app_leave_slingshot`, `touch_app_leave_ride`, `touch_app_leave_hold` | yes |
| 642-729 | `_build_interface` | - | yes |
| 730-766 | `_apply_settings` | `touch_room_hearing`, `touch_gyro_aim`, `touch_voice_echo`, `touch_mic_consent`, `touch_roll`, `touch_roll_reduced` +4 more | yes |
| 780-806 | `_notification` | - | yes |
| 1003-1110 | `_dispatch` | - | yes |
| 1202-1252 | `_perform_action` | - | yes |

### `main_context.gd` (662 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 141-491 | `_read_context` | - | yes |
| 524-589 | `_carry_name` | - | yes |
| 621-663 | `_kit_anchor_name` | - | yes |

### `main_feel.gd` (1594 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 281-316 | `_haptic` | - | yes |
| 328-423 | `_update_body_feedback` | - | yes |
| 424-504 | `_update_feedback` | - | yes |
| 546-578 | `_build_arms` | - | yes |
| 579-639 | `_arms_state` | - | yes |
| 640-691 | `_layout_hud` | - | yes |
| 709-943 | `_apply_camera_feel` | - | yes |
| 946-981 | `_feel_strike` | - | yes |
| 1019-1048 | `_feel_hold` | - | yes |
| 1080-1146 | `_render_snapshot` | - | yes |
| 1185-1231 | `_feel_death` | - | yes |
| 1232-1260 | `_ride_death` | - | yes |
| 1267-1300 | `_feel_zipline` | - | yes |
| 1320-1373 | `_feel_crane` | - | yes |
| 1374-1431 | `_feel_forklift` | - | yes |
| 1477-1531 | `_write_telemetry` | - | yes |

### `main_geometry.gd` (1179 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 217-245 | `_build_bump_textures` | - | yes |
| 311-338 | `_make_streak_texture` | - | yes |
| 339-366 | `_make_bump_texture` | - | yes |
| 370-405 | `_material` | - | yes |
| 406-448 | `_surfaced` | - | yes |
| 1067-1108 | `_batch_static_dressing` | - | yes |

### `main_machines.gd` (1461 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 130-399 | `_build_kit` | - | yes |
| 465-492 | `_build_hook_gun` | - | yes |
| 493-522 | `_build_hook_view` | - | yes |
| 536-582 | `_render_hook` | - | yes |
| 583-652 | `_add_kit_body` | - | yes |
| 653-734 | `_render_kit` | - | yes |
| 735-809 | `_render_rubble` | - | yes |
| 835-862 | `_rock_mesh` | - | yes |
| 919-951 | `_stone_dust` | - | yes |
| 952-983 | `_tumble_stones` | - | yes |
| 1006-1032 | `_glass_material` | - | yes |
| 1033-1087 | `_build_wet_e` | - | yes |
| 1088-1140 | `_build_vents` | - | yes |
| 1153-1185 | `_spray` | - | yes |
| 1186-1213 | `_grit` | - | yes |
| 1214-1242 | `_mist` | - | yes |
| 1297-1337 | `_render_vents` | - | yes |
| 1361-1437 | `_instance_kit_bodies` | - | yes |

### `main_world.gd` (1087 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 46-110 | `_build_world` | - | yes |
| 135-275 | `_build_stack` | - | yes |
| 328-368 | `_build_stack_halls` | - | yes |
| 369-417 | `_build_stack_signage` | - | yes |
| 418-482 | `_build_stack_bridges` | - | yes |
| 483-517 | `_build_stack_accents` | - | yes |
| 520-584 | `_build_stack_dressing` | - | yes |
| 585-642 | `_build_tower_skin` | - | yes |
| 643-682 | `_build_stack_continuation` | - | yes |
| 732-774 | `_build_lighting` | - | yes |
| 810-859 | `_build_mountains_and_waterfall` | - | yes |
| 867-932 | `_build_basin` | - | yes |
| 1059-1088 | `_build_kellerworks_signage` | - | yes |

### `ui/hud.gd` (631 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 89-128 | `update_hud` | - | yes |
| 137-189 | `_build_prompts` | - | yes |
| 190-224 | `_draw` | - | yes |
| 225-273 | `_draw_lens_blood` | - | yes |
| 299-341 | `_draw_gauge` | - | yes |
| 355-394 | `_draw_prompts` | - | yes |
| 395-444 | `_draw_altimeter` | - | yes |
| 445-471 | `_draw_toasts` | - | yes |

### `ui/pause_menu.gd` (817 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 193-220 | `_show_page` | - | yes |
| 221-352 | `_build` | - | yes |
| 377-434 | `menu_theme` | - | yes |
| 488-514 | `_build_controls_page` | - | yes |
| 538-575 | `_controls_rows` | - | yes |
| 576-606 | `_draw_controls` | - | yes |
| 718-745 | `_slider_row` | - | yes |

### `ui/touch_controls.gd` (841 lines)

| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |
|---|---|---|---|
| 241-277 | `_layout` | - | yes |
| 360-393 | `_touch_down` | - | yes |
| 394-448 | `_touch_drag` | - | yes |
| 449-488 | `_touch_up` | - | yes |
| 515-552 | `_update_stick_vector` | - | yes |
| 553-675 | `update_context` | - | yes |
| 683-737 | `_draw_button` | - | yes |
| 750-779 | `_draw_stick` | - | yes |

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
| `tests/error_reporter_test.gd` | `error_reporter.gd` | 109 |
| `tests/far_world_test.gd` | `far_world.gd`, `main_world.gd`, `mountain_mesh.gd`, `solid_export.gd` | 200 |
| `tests/fight_s1_sound_dedupe_test.gd` | `audio/matron_sound.gd` | 187 |
| `tests/fight_s1_test.gd` | `audio/matron_sound.gd`, `life/matron_view.gd`, `main_feel.gd`, `ui/settings_store.gd` | 205 |
| `tests/footstep_contact_test.gd` | `audio/audio_director.gd` | 31 |
| `tests/foreman_radio_test.gd` | `life/foreman_radio.gd`, `ui/ui_style.gd` | 173 |
| `tests/gore_shots_test.gd` | `life/gore/gore_view.gd`, `life/matron_view.gd` | 269 |
| `tests/hair_pose_probe.gd` | `life/matron_view.gd` | 225 |
| `tests/her_takes_test.gd` | `audio/her_takes.gd` | 159 |
| `tests/hold_beats_probe.gd` | `life/matron_hold_camera.gd` | 108 |
| `tests/hold_mix_leak_probe.gd` | `audio/audio_director.gd` | 89 |
| `tests/injury_shots_test.gd` | `life/matron_view.gd` | 591 |
| `tests/matron_figure_test.gd` | `life/matron_look.gd`, `life/matron_view.gd` | 359 |
| `tests/matron_foot_plant_test.gd` | `life/matron_view.gd` | 267 |
| `tests/matron_hold_haptics_test.gd` | `main_feel.gd`, `ui/settings_store.gd` | 39 |
| `tests/matron_perception_climb_test.gd` | `life/matron_view.gd` | 37 |
| `tests/matron_presentation_test.gd` | `life/matron_view.gd` | 357 |
| `tests/matron_speech_test.gd` | `audio/audio_director.gd`, `audio/matron_sound.gd`, `audio/matron_speech.gd`, `audio/matron_voice_lines.gd` | 488 |
| `tests/punch_list_test.gd` | `ui/punch_list.gd` | 77 |
| `tests/rip_shot_probe.gd` | `life/matron_hold_camera.gd` | 299 |
| `tests/script_load_test.gd` | - | 32 |
| `tests/sound_bank_test.gd` | `audio/audio_director.gd`, `audio/her_takes.gd`, `audio/sound_bank.gd` | 98 |

## 10. Not reached by any test

- Native (`src/sim`): `PhysicsWorld::slingshot_prediction` (`physics_world.cpp` 939-974), `Slingshot::reset` (`slingshot.cpp` 969-1005), `Slingshot::prediction` (`slingshot.cpp` 1007-1071): no native group calls them. The bridge binds `get_slingshot_prediction`, which `touch_slingshot` can reach (section 6).
- `src/sim/matron_mover.hpp`: interface only, no code.
- Bridge (`src/bridge/scraperx_simulation.cpp`): the native groups do not link it; every `get_*_state` and `_bind_methods` is reached only by the touch scenarios and the game.
- Presentation: `identity/portrait_face_fit.gd`. See section 7 for what that means.

## Method (to regenerate)

`tools/test_reach/` (Python with libclang, `pip install libclang`) parsed every `src/**/*.cpp` and `tests/*.cpp` with the flags of the build's `compile_commands.json` (or, without one, the flags of `build/host`) (`-std=c++17`, Jolt include path and `JPH_*` defines, `-DSCRAPERX_HAS_JOLT=1`), keyed functions by canonical declaration location, and recorded call edges (`CALL_EXPR` references), override links, constructor edges for `make_unique`/`emplace*`, and struct-field references. A second script did the GDScript. The scripts are in `tools/test_reach/` (README.md there); they need libclang and a configured build for the Jolt and godot-cpp headers.
