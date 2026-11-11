#include "sim_tests.hpp"
#include "sim_registry.hpp"

// J-071 step 2: the default sequence split across two CTest slices so each
// native-tests shard stays under about 900 s. Groups keep their order; the
// cut is after zipline, from the expand-tests TIME lines (run 42600534526:
// athletic-traversal-1 705.1 s, athletic-traversal-2 705.7 s).

void run_athletic_traversal_1() {
    run_pulley_static_end();
    run_matron_exam();
    run_core_traversal();
    run_fall_checkpoint();
    run_world_solids();
    run_twin_geometry();
    run_twin();
    run_twin_balconies();
    run_well_stage_a();
    run_well_stage_b();
    run_well_stage_c();
    run_well_band();
    run_step2_movement();
    run_stack();
    run_wet_isolation();
    run_wet_e();
    run_wet_handling();
    run_wet_band();
    run_wet_route();
    run_water();
    run_plate_shop();
    run_plate_band();
    run_plate_route();
    run_plate_wreckage();
    run_facade_crane();
    run_crane_band();
    run_crane_route();
    run_route_pockets();
    run_crane_wreckage();
    run_midstack_service();
    run_service_c4();
    run_service_n();
    run_service_c5();
    run_service_o();
    run_wreck_way_on();
    run_service_band();
    run_service_route();
    run_determinism();
    run_slingshot();
    run_swing();
    run_c6();
    run_mast();
    run_pitman();
    run_helix();
    run_high();
    run_incline();
    run_wheel();
    run_save();
    run_landing();
    run_jump_buffer();
    run_parkour_seams();
    run_teeter();
    run_edge_stand();
    run_trapeze();
    run_tram();
    run_derrick();
    run_brachiation();
    run_pipe_gallery();
    run_slide();
    run_speed_vault();
    run_wall_run();
    run_wall_climb();
    run_tuck();
    run_flow();
    run_springboard();
    run_swing_launch();
    run_slide_jump();
    run_climb_moves();
    run_wall_chain();
    run_up_controls();
    run_zipline();
}

void run_athletic_traversal_2() {
    run_corpse();
    run_arm_break();
    run_hook();
    run_workers();
    run_c7();
    run_gust_front_upper();
    run_summit();
    run_summit_arm();
    run_summit_ending();
    run_matron_player_independence();
    run_matron_routecraft();
    run_matron_exam_routecraft();
    run_matron_exam_climber();
    run_matron_exam_descent();
    run_matron_found_climb();
    run_matron();
    run_matron_pace();
    run_matron_unstick();
    run_matron_traverse();
    run_matron_pursuit();
    run_matron_ring_descent();
    run_matron_ways();
    run_matron_climb();
    run_strike();
    run_one_arm_route();
    run_bay();
    run_loose_stock();
    run_look_target();
    run_body_order();
    run_fabricated();
    run_forklift();
    run_forklift_strain();
    run_yard_crane();
    run_yard_crane_magnet();
    run_yard_crane_spreader();
    run_machine_hearing();
    run_machine_draw();
    run_ascent();
}

SCRAPERX_SLICE("athletic-traversal-1", run_athletic_traversal_1);
SCRAPERX_SLICE("athletic-traversal-2", run_athletic_traversal_2);
