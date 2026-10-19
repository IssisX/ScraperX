// The native tests: one executable, every test in order. SCRAPERX_ONLY=<name>
// runs one group. The tests themselves live in sim_tests_*.cpp.

#include "sim_tests.hpp"

int main() {
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "parkour") {
        run_parkour_seams();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "jumpbuffer") {
        run_jump_buffer();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "workers") {
        run_workers();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "swing") {
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
        run_zipline();
        run_hook();
        run_workers();
        run_c7();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "handling") {
        run_wet_handling();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "stock") {
        run_loose_stock();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "climb") {
        run_wall_climb();
        run_tuck();
        run_flow();
        run_springboard();
        run_swing_launch();
        run_slide_jump();
        run_climb_moves();
        run_wall_chain();
        run_zipline();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "up") {
        run_up_controls();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "zipline") {
        run_zipline();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "corpse") {
        run_corpse();
        run_arm_break();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "well") {
        run_well_stage_a();
        run_well_stage_b();
        run_well_stage_c();
        run_well_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "forklift") {
        run_forklift();
        run_forklift_strain();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "crane") {
        run_yard_crane();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "magnet") {
        run_yard_crane_magnet();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "spreader") {
        run_yard_crane_spreader();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "matron") {
        run_matron();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "matron-collision") {
        run_matron_collision();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "matron-mischief") {
        run_matron_mischief();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY"); only != nullptr && std::string(only) == "summit") {
        run_summit();
        run_summit_arm();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007") {
        run_wet_isolation();
        run_wet_handling();
        run_wet_band();
        run_wet_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007-route") {
        run_wet_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007-band") {
        run_wet_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008") {
        run_plate_shop();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-band") {
        run_plate_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-route") {
        run_plate_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-wreck") {
        run_plate_wreckage();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009") {
        run_facade_crane();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-band") {
        run_crane_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-route") {
        run_crane_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-wreck") {
        run_crane_wreckage();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010") {
        run_midstack_service();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010-c5") {
        run_service_c5();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010-n") {
        run_service_n();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010-o") {
        run_service_o();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "slingshot") {
        run_slingshot();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "C6") {
        run_c6();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "mast") {
        run_mast();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "pitman") {
        run_pitman();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "helix") {
        run_helix();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "high") {
        run_high();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "incline") {
        run_incline();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "wheel") {
        run_wheel();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "teeter") {
        run_teeter();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "trapeze") {
        run_trapeze();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "landing") {
        run_landing();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "save") {
        run_save();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "tram") {
        run_tram();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "derrick") {
        run_derrick();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "swing") {
        run_swing();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "determinism") {
        run_determinism();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010-band") {
        run_service_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010-route") {
        run_service_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-010-c4") {
        run_service_c4();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "stack") {
        run_stack();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "ascent") {
        run_ascent();
        return EXIT_SUCCESS;
    }

    run_pulley_static_end();
    run_core_traversal();
    run_fall_checkpoint();
    run_world_solids();
    run_well_stage_a();
    run_well_stage_b();
    run_well_stage_c();
    run_well_band();
    run_step2_movement();
    run_stack();
    run_wet_isolation();
    run_wet_handling();
    run_wet_band();
    run_wet_route();
    run_plate_shop();
    run_plate_band();
    run_plate_route();
    run_plate_wreckage();
    run_facade_crane();
    run_crane_band();
    run_crane_route();
    run_crane_wreckage();
    run_midstack_service();
    run_service_c4();
    run_service_n();
    run_service_c5();
    run_service_o();
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
    run_corpse();
    run_arm_break();
    run_hook();
    run_workers();
    run_c7();
    run_summit();
    run_summit_arm();
    run_matron();
    run_loose_stock();
    run_forklift();
    run_forklift_strain();
    run_yard_crane();
    run_yard_crane_magnet();
    run_yard_crane_spreader();
    run_ascent();

    return EXIT_SUCCESS;
}
