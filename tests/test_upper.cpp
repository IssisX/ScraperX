#include "sim_checks.hpp"
#include "sim_routes.hpp"

// The pitman, the wheel, the brace, and the climb that runs through all of them.

double pitman_hinge_angle(const scraperx::sim::Simulation &simulation) {
    const auto index = simulation.kit_body_index(scraperx::sim::Simulation::kPitmanEntityId);
    const auto rotation = simulation.kit_body_rotation(index);
    return 2.0 * std::atan2(rotation.x, rotation.w);
}


// North of the casing gallery. Catch the bar, pull with its swing, let go
// onto the next deck. A let-go over the gap lands on the shelf, and the
// ladder west of the swing brings you back for another try.
bool climb_pitman(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalState;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto say = [&](const char *what) {
        const auto p = floor.snapshot();
        std::cout << what << " " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " v=" << p.player_linear_velocity.x << "," << p.player_linear_velocity.y << ","
                  << p.player_linear_velocity.z << " grounded=" << p.player_grounded
                  << " support=" << p.support_entity_id << " grip=" << p.grip_available
                  << " trav=" << static_cast<int>(p.traversal_state)
                  << " hold=" << p.traversal_support_entity_id << " deaths=" << p.death_count
                  << " hinge=" << pitman_hinge_angle(floor) << "\n";
    };
    auto watch = [&](const double seconds) {
        using scraperx::sim::Simulation;
        const auto ticks =
            static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    if (!walk_to(floor, 9.40, -150.55, 8.0, 0.12)) {
        say("pitman approach");
        return false;
    }
    (void)floor.set_facing(0.0, 1.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.3);
    if (!floor.snapshot().grip_available) {
        say("pitman no bar in reach");
        return false;
    }
    const auto before = floor.snapshot();
    (void)floor.request_traversal();
    watch(scraperx::sim::Simulation::kFixedStepSeconds);
    const auto caught = floor.snapshot();
    if (caught.traversal_state != TraversalState::Hanging ||
        caught.traversal_support_entity_id != Simulation::kPitmanEntityId) {
        say("pitman catch");
        return false;
    }
    if (std::hypot(caught.player_position.x - before.player_position.x,
                   caught.player_position.z - before.player_position.z) > 0.15) {
        say("pitman catch yanked");
        return false;
    }

    // Over the gap, still short of the next deck: let go, land on the shelf.
    if (!hold_stick(floor, 0.0, 1.0, 0.0, 1.0, 8.0, [](const scraperx::sim::Snapshot &state) {
            return state.player_position.z > -150.25 && state.player_position.z < -150.05;
        })) {
        say("pitman to the gap");
        return false;
    }
    (void)floor.request_release();
    watch(scraperx::sim::Simulation::kFixedStepSeconds);
    if (floor.snapshot().traversal_state != TraversalState::None) {
        say("pitman release");
        return false;
    }
    if (!wait_for(floor, 4.0, [](const scraperx::sim::Snapshot &state) {
            return state.player_grounded && state.traversal_state == TraversalState::None &&
                   state.death_count == 0 && state.player_position.y > 757.2 && state.player_position.y < 758.2 &&
                   state.player_position.z < -148.40 && state.player_position.z > -150.20;
        })) {
        say("pitman shelf");
        return false;
    }
    const auto missed = floor.snapshot();
    const double speed_at_miss = std::hypot(missed.player_linear_velocity.x, missed.player_linear_velocity.z);

    // The bar swings back on its own. A free swing has to die down to a hang
    // before the ladder brings you back to it. One sample at the bottom is
    // not that: the peak over two seconds has to be small.
    bool settled = false;
    double window_peak = 0.0;
    for (int tick = 0; tick < 22 * 90 && !settled; ++tick) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        window_peak = std::max(window_peak, std::abs(pitman_hinge_angle(floor)));
        if ((tick + 1) % 180 == 0) {
            settled = window_peak < 0.10;
            window_peak = 0.0;
        }
    }
    if (!settled) {
        say("pitman settle");
        return false;
    }
    if (!walk_to(floor, 8.10, -149.30, 8.0, 0.20)) {
        say("pitman ladder foot");
        return false;
    }
    if (!climb_wet_hold(floor, 8.10, -149.30, 0.0, -1.0, false, 759.4)) {
        say("pitman back up");
        return false;
    }
    if (!walk_to(floor, 9.40, -150.55, 8.0, 0.12)) {
        say("pitman second approach");
        return false;
    }
    (void)floor.set_facing(0.0, 1.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.3);
    if (!floor.snapshot().grip_available) {
        say("pitman second grab missing");
        return false;
    }
    const auto before_second = floor.snapshot();
    (void)floor.request_traversal();
    watch(scraperx::sim::Simulation::kFixedStepSeconds);
    const auto second = floor.snapshot();
    if (second.traversal_state != TraversalState::Hanging ||
        second.traversal_support_entity_id != Simulation::kPitmanEntityId) {
        say("pitman second catch");
        return false;
    }
    if (std::hypot(second.player_position.x - before_second.player_position.x,
                   second.player_position.z - before_second.player_position.z) > 0.15) {
        say("pitman second catch yanked");
        return false;
    }
    // Jump still mantles a real ledge. On this bar it must not.
    (void)floor.request_jump();
    watch(scraperx::sim::Simulation::kFixedStepSeconds);
    if (floor.snapshot().traversal_state != TraversalState::Hanging) {
        say("pitman jump stole the hang");
        return false;
    }
    double pump_max_z = -1.0e9;
    double pump_min_z = 1.0e9;
    double pump_max_hinge = 0.0;
    double pump_at_z = 0.0;
    {
        using scraperx::sim::Simulation;
        const auto ticks = static_cast<std::uint32_t>(16.0 * Simulation::kTickRateHz);
        bool reached = false;
        for (std::uint32_t tick = 0; tick < ticks && !reached; ++tick) {
            (void)floor.set_move_input(0.0, 1.0);
            (void)floor.set_facing(0.0, 1.0);
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
            const auto state = floor.snapshot();
            const double hinge = pitman_hinge_angle(floor);
            if (state.player_position.z > pump_max_z) {
                pump_max_z = state.player_position.z;
                pump_at_z = hinge;
            }
            pump_min_z = std::min(pump_min_z, state.player_position.z);
            pump_max_hinge = std::max(pump_max_hinge, std::abs(hinge));
            reached = state.traversal_state == TraversalState::Hanging &&
                      state.player_position.z > -148.60 && state.player_linear_velocity.z < 1.2 &&
                      state.player_linear_velocity.z > -0.5;
        }
        (void)floor.set_move_input(0.0, 0.0);
        if (!reached) {
            std::cout << "pump envelope max_z=" << pump_max_z << " at_hinge=" << pump_at_z
                      << " min_z=" << pump_min_z << " max_abs_hinge=" << pump_max_hinge << "\n";
            say("pitman pump");
            return false;
        }
    }
    const auto flying = floor.snapshot();
    (void)floor.request_release();
    watch(scraperx::sim::Simulation::kFixedStepSeconds);
    if (floor.snapshot().traversal_state != TraversalState::None) {
        say("pitman let go");
        return false;
    }
    if (!wait_for(floor, 4.0, [](const scraperx::sim::Snapshot &state) {
            return state.player_grounded && state.death_count == 0 && state.player_position.y > 759.4 &&
                   state.player_position.z > -148.70 && state.player_position.z < -146.00 &&
                   state.support_entity_id == Simulation::kCraneFrameEntityId;
        })) {
        say("pitman deck");
        return false;
    }
    if (!walk_to(floor, 9.40, -146.10, 6.0, 0.25)) {
        say("pitman walk off");
        return false;
    }
    const auto landed = floor.snapshot();
    if (!(landed.player_grounded && landed.death_count == 0 && landed.player_position.y > 759.4 &&
          landed.player_position.z > -146.50 && landed.support_entity_id == Simulation::kCraneFrameEntityId)) {
        say("pitman onward");
        return false;
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "pitman snap " << g_path_watch.worst << " at " << g_path_watch.worst_at.x << " "
                  << g_path_watch.worst_at.y << " " << g_path_watch.worst_at.z << " from "
                  << g_path_watch.worst_from_x << " " << g_path_watch.worst_from_z
                  << " trav=" << g_path_watch.worst_traversal << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the pitman must not snap the body more than 0.15 m in one tick");
    std::cout << "PASS scraperx_sim pitman: floor_y=" << landed.player_position.y
              << " release_z=" << flying.player_position.z << " release_v=" << flying.player_linear_velocity.z
              << " miss_y=" << missed.player_position.y << " miss_speed=" << speed_at_miss
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    (void)speed_at_miss;
    return true;
}


double wheel_hinge_angle(const scraperx::sim::Simulation &simulation) {
    const auto index = simulation.kit_body_index(scraperx::sim::Simulation::kWheelEntityId);
    const auto rotation = simulation.kit_body_rotation(index);
    return 2.0 * std::atan2(rotation.x, rotation.w);
}


// North of the pitman's landing. Step into the hanging car, haul the lever
// on its west side, and ride the bucket down onto the next deck. The lever
// is thrown from the car, so the walk that releases the catch stays on the
// floor.
bool climb_wheel(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto say = [&](const char *what) {
        const auto p = floor.snapshot();
        const auto car = floor.kit_body_rotation(floor.kit_body_index(Simulation::kWheelCarEntityId));
        std::cout << what << " " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " v=" << p.player_linear_velocity.x << "," << p.player_linear_velocity.y << ","
                  << p.player_linear_velocity.z << " grounded=" << p.player_grounded
                  << " support=" << p.support_entity_id << " carry=" << p.carrying_entity_id
                  << " target=" << p.carry_target_entity_id << " trav=" << static_cast<int>(p.traversal_state)
                  << " deaths=" << p.death_count << " wheel=" << wheel_hinge_angle(floor) << " car_q=" << car.x
                  << "," << car.y << "," << car.z << "," << car.w << "\n";
    };
    auto watch = [&](const double seconds) {
        const auto ticks =
            static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    if (!walk_to(floor, 9.40, -145.55, 6.0, 0.18)) {
        say("wheel onto the car");
        return false;
    }
    // The car is pinned, so the step onto it is a real 0.30 m rise. A full
    // walk clears that rise in the air and the check sees a body that has
    // not landed. Creep until the feet are on the floor.
    bool boarded_ok = false;
    for (int step = 0; step < 6 * 90 && !boarded_ok; ++step) {
        (void)floor.set_move_input(0.0, 0.22);
        (void)floor.set_facing(0.0, 1.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto step_state = floor.snapshot();
        boarded_ok = step_state.player_grounded && step_state.player_position.y > 759.5 &&
                     step_state.support_entity_id == Simulation::kWheelCarEntityId &&
                     step_state.player_position.z > -144.85;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (!boarded_ok) {
        say("wheel not on the car");
        return false;
    }
    // Under the hanger, then west along that line to the lever. South of
    // the hanger the floor pitches and the feet lose it.
    if (!walk_to(floor, 9.40, -144.20, 4.0, 0.08)) {
        say("wheel to the middle");
        return false;
    }
    (void)floor.set_move_input(0.0, 0.0);
    watch(1.0);
    const auto settled = floor.snapshot();
    if (!settled.player_grounded || settled.player_position.y < 759.5 ||
        settled.support_entity_id != Simulation::kWheelCarEntityId) {
        say("wheel left the middle");
        return false;
    }
    if (!walk_to(floor, 8.74, -144.20, 5.0, 0.08)) {
        say("wheel to the lever");
        return false;
    }
    const auto at_lever = floor.snapshot();
    if (!at_lever.player_grounded || at_lever.player_position.y < 759.5 ||
        at_lever.support_entity_id != Simulation::kWheelCarEntityId) {
        say("wheel left the car");
        return false;
    }
    (void)floor.set_facing(-1.0, 0.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.45);
    if (floor.snapshot().carry_target_entity_id != Simulation::kWheelLeverEntityId) {
        say("wheel lever not in reach");
        return false;
    }
    (void)floor.request_pick_up();
    watch(0.3);
    if (floor.snapshot().carrying_entity_id != Simulation::kWheelLeverEntityId) {
        say("wheel lever not in the hands");
        return false;
    }
    bool released = false;
    for (int step = 0; step < 6 * 90 && !released; ++step) {
        const auto here = floor.snapshot().player_position;
        if (here.x < 9.15) {
            (void)floor.set_move_input(0.35, 0.0);
        } else {
            (void)floor.set_move_input(0.0, 0.0);
        }
        (void)floor.set_facing(-1.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        released = wheel_hinge_angle(floor) < -0.08;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (floor.snapshot().carrying_entity_id != 0) {
        (void)floor.request_set_down();
    }
    watch(0.2);
    if (!released) {
        say("wheel did not let go");
        return false;
    }
    bool riding = false;
    for (int step = 0; step < 20 * 90 && !riding; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        riding = state.player_grounded && state.death_count == 0 &&
                 state.support_entity_id == Simulation::kWheelCarEntityId && state.player_position.y > 764.2 &&
                 state.player_position.y < 766.2 && state.player_position.z > -141.8 &&
                 state.player_position.z < -140.1 && wheel_hinge_angle(floor) < -1.65;
    }
    if (!riding) {
        say("wheel ride");
        return false;
    }
    const auto at_stop = floor.snapshot();
    if (!walk_to(floor, 9.40, -138.70, 8.0, 0.30)) {
        say("wheel step off");
        return false;
    }
    const auto landed = floor.snapshot();
    if (!(landed.player_grounded && landed.death_count == 0 && landed.player_position.y > 764.4 &&
          landed.player_position.z > -139.90 && landed.player_position.z < -138.25 &&
          landed.support_entity_id == Simulation::kCraneFrameEntityId)) {
        say("wheel deck");
        return false;
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "wheel snap " << g_path_watch.worst << " at " << g_path_watch.worst_at.x << " "
                  << g_path_watch.worst_at.y << " " << g_path_watch.worst_at.z << " from "
                  << g_path_watch.worst_from_x << " " << g_path_watch.worst_from_z
                  << " trav=" << g_path_watch.worst_traversal << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the wheel must not snap the body more than 0.15 m in one tick");
    std::cout << "PASS scraperx_sim wheel: floor_y=" << landed.player_position.y
              << " ride_y=" << at_stop.player_position.y << " ride_z=" << at_stop.player_position.z
              << " wheel=" << wheel_hinge_angle(floor) << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The ladder west of the high deck, while the wheel is still held. Up, then
// back down onto the lane, so the same body can still take the car.
bool climb_wheel_ladder(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto say = [&](const char *what) {
        const auto p = floor.snapshot();
        std::cout << what << " " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " grip=" << p.grip_available << " trav=" << static_cast<int>(p.traversal_state)
                  << " deaths=" << p.death_count << "\n";
    };
    if (!climb_wet_hold(floor, 7.05, -138.50, 1.0, 0.0, false, 764.5)) {
        say("wheel ladder up");
        return false;
    }
    if (!walk_to(floor, 8.45, -139.55, 6.0, 0.20)) {
        say("wheel ladder deck");
        return false;
    }
    for (int step = 0; step < 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
    }
    bool left = false;
    for (int step = 0; step < 4 * 90 && !left; ++step) {
        (void)floor.set_move_input(-0.22, 0.0);
        (void)floor.set_facing(-1.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        left = !state.player_grounded && state.player_position.y < 763.5;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (!left) {
        say("wheel ladder leave");
        return false;
    }
    if (!wait_for(floor, 5.0, [](const scraperx::sim::Snapshot &state) {
            return state.player_grounded && state.death_count == 0 && state.player_position.y > 759.2 &&
                   state.player_position.y < 760.6 && state.player_position.x < 8.6 &&
                   state.player_position.x > 6.7;
        })) {
        say("wheel ladder down");
        return false;
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "wheel ladder snap " << g_path_watch.worst << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the wheel ladder must not snap the body more than 0.15 m in one tick");
    const auto down = floor.snapshot();
    std::cout << "PASS scraperx_sim wheel ladder: floor_y=" << down.player_position.y << "\n";
    if (!walk_to(floor, 7.05, -139.70, 6.0, 0.25)) {
        say("wheel lane after the drop");
        return false;
    }
    if (!walk_to(floor, 7.05, -145.80, 8.0, 0.22)) {
        say("wheel back south");
        return false;
    }
    if (!walk_to(floor, 9.40, -146.10, 6.0, 0.20)) {
        say("wheel back to the landing");
        return false;
    }
    return true;
}


// Thrown from the pad, not the car. The wheel still turns. The ladder is
// how you reach the deck you did not ride to.
bool miss_wheel(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto say = [&](const char *what) {
        const auto p = floor.snapshot();
        std::cout << what << " " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " carry=" << p.carrying_entity_id << " target=" << p.carry_target_entity_id
                  << " deaths=" << p.death_count << " wheel=" << wheel_hinge_angle(floor) << "\n";
    };
    auto watch = [&](const double seconds) {
        const auto ticks =
            static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    if (!walk_to(floor, 8.30, -145.80, 6.0, 0.16)) {
        say("wheel miss pad");
        return false;
    }
    if (!walk_to(floor, 7.40, -145.70, 5.0, 0.16)) {
        say("wheel miss pad");
        return false;
    }
    if (!walk_to(floor, 7.40, -144.20, 6.0, 0.12)) {
        say("wheel miss pad");
        return false;
    }
    (void)floor.set_facing(1.0, 0.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.45);
    if (floor.snapshot().carry_target_entity_id != Simulation::kWheelLeverEntityId) {
        say("wheel miss lever");
        return false;
    }
    (void)floor.request_pick_up();
    watch(0.25);
    if (floor.snapshot().carrying_entity_id != Simulation::kWheelLeverEntityId) {
        say("wheel miss hands");
        return false;
    }
    bool gone = false;
    for (int step = 0; step < 5 * 90 && !gone; ++step) {
        const auto here = floor.snapshot().player_position;
        if (here.x < 7.80) {
            (void)floor.set_move_input(0.35, 0.0);
        } else {
            (void)floor.set_move_input(0.0, 0.0);
        }
        (void)floor.set_facing(1.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        gone = wheel_hinge_angle(floor) < -0.40;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (floor.snapshot().carrying_entity_id != 0) {
        (void)floor.request_set_down();
    }
    watch(0.4);
    const auto stayed = floor.snapshot();
    if (!(gone && stayed.death_count == 0 && stayed.player_grounded && stayed.player_position.y < 761.0 &&
          stayed.player_position.y > 759.2)) {
        say("wheel miss still on the landing");
        return false;
    }
    if (!climb_wet_hold(floor, 7.05, -138.50, 1.0, 0.0, false, 764.5)) {
        say("wheel miss ladder");
        return false;
    }
    const auto up = floor.snapshot();
    if (!(up.death_count == 0 && up.player_grounded && up.player_position.y > 764.5 &&
          up.support_entity_id == Simulation::kCraneFrameEntityId)) {
        say("wheel miss deck");
        return false;
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "wheel miss snap " << g_path_watch.worst << "\n";
    }
    require(g_path_watch.worst <= 0.15, "missing the wheel must not snap the body more than 0.15 m in one tick");
    std::cout << "PASS scraperx_sim wheel miss: floor_y=" << up.player_position.y
              << " stayed_y=" << stayed.player_position.y << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// Same body, from standing on the 220 m ring, up through the rest of the
// machines that are already in the building, to the floor the ladder reaches.
bool climb_above_ring220(scraperx::sim::Simulation &run, const double start_seconds,
                         const std::uint64_t deaths_at_start, const char *pass_name) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    require_leg(run, walk_to(run, 2.5, -129.6, 8.0) && walk_to(run, 4.0, -129.2, 6.0) && seat_wet_spool(run) &&
                         throw_wet_fill(run),
                "along the 220 ring, the spool seated and the fill thrown");
    require_leg(run,
                wait_for(run, 120.0, [&](const Snapshot &) { return run.wet_state().d_platform_travel >= 35.95; }),
                "the first wet platform to 256 m");
    require_leg(run,
                walk_to(run, 13.2, -134.5, 6.0) && walk_to(run, 13.2, -135.8, 6.0) && shut_wet_door(run) &&
                    pull_wet_trip(run),
                "into the cab, the door shut and the chiller tripped");
    require_leg(run, wait_for(run, 90.0, [&](const Snapshot &) { return run.wet_state().e_cab_travel >= 41.95; }),
                "the cab to 298 m");
    require_leg(run,
                walk_to(run, 13.4, -137.2, 4.0) && walk_to(run, 13.4, -139.9, 6.0) && couple_wet_hose(run) &&
                    pull_wet_stop_valve(run),
                "onto the next platform, the hose coupled and the valve thrown");
    require_leg(run,
                wait_for(run, 90.0, [&](const Snapshot &) { return run.wet_state().f_platform_travel >= 41.95; }),
                "that platform to the 340 m floor");
    require_leg(run, walk_to(run, 13.0, -137.2, 6.0), "off that platform onto the 340 m floor");
    const double at_340 = run.snapshot().simulation_time_seconds - start_seconds;

    require_leg(run, walk_to_shop_cleat(run) && rig_shop_g(run) && pull_shop_g(run),
                "the shop lift rigged and its pin pulled");
    require_leg(run, wait_for(run, 60.0, [&](const Snapshot &) { return run.shop_state().g_platform_travel >= 33.7; }),
                "the shop lift to the 374 m ring");
    (void)run.advance_frame(1.0);
    require_leg(run, walk_to(run, -14.5, -149.3, 8.0) && pull_shop_girder_pin(run) && board_shop_h(run) &&
                         pull_shop_chock(run),
                "the shop beam's pin out, across the gangway, and its chock pulled");
    require_leg(run,
                wait_for(run, 90.0, [&](const Snapshot &) { return run.shop_state().h_platform_travel >= 43.9; }),
                "that ride to the 418 m ring");
    (void)run.advance_frame(1.0);
    require_leg(run, rig_shop_i(run) && pull_shop_domino(run), "onto the cage, hooked on, the pin pulled");
    require_leg(run, wait_for(run, 90.0, [&](const Snapshot &) { return run.shop_state().i_cage_travel >= 43.9; }),
                "the cage to 462 m");
    (void)run.advance_frame(1.0);
    require_leg(run, climb_wet_hold(run, -7.6, -145.6, -1.0, 0.0, false, 484.5), "up the ladder onto the 484 m ring");
    const double at_484 = run.snapshot().simulation_time_seconds - start_seconds;

    require_leg(run, climb_crane_band(run), "the three cranes, then standing on the 640 m floor");
    const auto top = run.snapshot();
    require(top.death_count == deaths_at_start && standing_above(top, 640.2),
            "the same climb must be standing on the 640 m floor, and must not have died");
    require_leg(run, climb_girder(run), "up the girder onto the next floor");
    require_leg(run, climb_shutter(run), "the sheet aside, then up the ladder");
    require_leg(run, climb_ramp(run), "the pin out, then up the ramp");
    require_leg(run, climb_gate(run), "the gate aside, then up the ladder");
    require_leg(run, climb_cart(run), "onto the cart, then up the rail");
    require_leg(run, climb_slats(run), "up the cooling-tower slats");
    require_leg(run, climb_duct(run), "the duct down, then up it");
    require_leg(run, climb_casing(run), "up the outside of the casing");
    require_leg(run, climb_pitman(run), "the pitman over the gap north of the gallery");
    require_leg(run, climb_wheel(run), "onto the wheel, then up to the next deck");
    const auto landed = run.snapshot();
    std::cout << "PASS scraperx_sim " << pass_name << ": seconds=" << landed.simulation_time_seconds - start_seconds
              << " at_340=" << at_340 << " at_484=" << at_484 << " floor_y=" << landed.player_position.y << '\n';
    return true;
}


double brace_hinge_angle(const scraperx::sim::Simulation &simulation) {
    const auto index = simulation.kit_body_index(scraperx::sim::Simulation::kBraceEntityId);
    const auto rotation = simulation.kit_body_rotation(index);
    return 2.0 * std::atan2(rotation.z, rotation.w);
}


// West of the wheel deck. Up the cabinet, along the beam, up the ladder,
// haul the brace down, and walk it onto the next deck.
bool climb_brace(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto say = [&](const char *what) {
        const auto p = floor.snapshot();
        const auto brace = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId));
        std::cout << what << " " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " trav=" << static_cast<int>(p.traversal_state) << " ledge=" << p.ledge_available
                  << " grip=" << p.grip_available << " carry=" << p.carrying_entity_id
                  << " target=" << p.carry_target_entity_id << " deaths=" << p.death_count
                  << " brace=" << brace.x << "," << brace.y << "," << brace.z
                  << " hinge=" << brace_hinge_angle(floor) << "\n";
    };
    auto watch = [&](const double seconds) {
        const auto ticks =
            static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    if (!walk_to(floor, 9.20, -138.15, 5.0, 0.14)) {
        say("brace north on the deck");
        return false;
    }
    if (!walk_to(floor, 7.40, -137.95, 6.0, 0.14)) {
        say("brace to the cabinet");
        return false;
    }
    const auto at_cabinet = floor.snapshot();
    if (!at_cabinet.player_grounded || at_cabinet.player_position.y < 764.4) {
        say("brace not on the catwalk");
        return false;
    }
    (void)floor.set_facing(-1.0, 0.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.4);
    if (!floor.snapshot().ledge_available) {
        say("brace cabinet not offered");
        return false;
    }
    (void)floor.request_traversal();
    if (!wait_for(floor, 2.5, [](const Snapshot &state) { return standing_above(state, 766.2); })) {
        say("brace cabinet mantle");
        return false;
    }
    if (!walk_to(floor, 6.40, -139.60, 6.0, 0.12)) {
        say("brace to the plate");
        return false;
    }
    (void)floor.set_facing(-1.0, 0.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.4);
    if (!floor.snapshot().ledge_available) {
        say("brace plate not offered");
        return false;
    }
    (void)floor.request_traversal();
    if (!wait_for(floor, 2.5, [](const Snapshot &state) { return standing_above(state, 767.7); })) {
        say("brace plate mantle");
        return false;
    }
    if (!walk_to(floor, 5.50, -142.20, 8.0, 0.18)) {
        say("brace along the beam");
        return false;
    }
    if (!walk_to(floor, 5.50, -142.80, 6.0, 0.16)) {
        say("brace to the pad");
        return false;
    }
    if (!walk_to(floor, 4.55, -142.80, 5.0, 0.16)) {
        say("brace off the beam");
        return false;
    }
    if (!walk_to(floor, 4.55, -144.55, 6.0, 0.16)) {
        say("brace beside the ladder");
        return false;
    }
    if (!walk_to(floor, 5.50, -144.55, 5.0, 0.14)) {
        say("brace to the ladder");
        return false;
    }
    const auto at_ladder = floor.snapshot();
    if (!at_ladder.player_grounded || at_ladder.player_position.y < 767.4) {
        say("brace left the pad");
        return false;
    }
    if (!climb_wet_hold(floor, 5.50, -144.55, 0.0, 1.0, false, 778.4)) {
        say("brace ladder");
        return false;
    }
    if (floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId)).x < 2.5) {
        say("brace already down");
        return false;
    }
    // South of the lever post, then east of it, then up to the bar. A
    // straight line walks the capsule into the post and knocks the pin out.
    if (!walk_to(floor, 5.50, -143.18, 4.0, 0.14)) {
        say("brace back from the post");
        return false;
    }
    if (!walk_to(floor, 6.85, -143.18, 5.0, 0.14)) {
        say("brace east of the post");
        return false;
    }
    if (!walk_to(floor, 6.85, -142.55, 4.0, 0.12)) {
        say("brace to the lever");
        return false;
    }
    const auto at_lever = floor.snapshot();
    if (!at_lever.player_grounded || at_lever.player_position.y < 778.4) {
        say("brace left the perch");
        return false;
    }
    (void)floor.set_facing(-1.0, 0.0);
    (void)floor.set_move_input(0.0, 0.0);
    watch(0.45);
    if (floor.snapshot().carry_target_entity_id != Simulation::kBraceLeverEntityId) {
        say("brace lever not in reach");
        return false;
    }
    (void)floor.request_pick_up();
    watch(0.3);
    if (floor.snapshot().carrying_entity_id != Simulation::kBraceLeverEntityId) {
        say("brace lever not in the hands");
        return false;
    }
    const auto brace_x0 =
        floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId)).x;
    bool released = false;
    for (int step = 0; step < 6 * 90 && !released; ++step) {
        const auto here = floor.snapshot().player_position;
        if (here.x < 7.30) {
            (void)floor.set_move_input(0.28, 0.0);
        } else {
            (void)floor.set_move_input(0.0, 0.0);
        }
        (void)floor.set_facing(-1.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        released = brace_hinge_angle(floor) > 0.45 ||
                   floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId)).x <
                       brace_x0 - 0.4;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (floor.snapshot().carrying_entity_id != 0) {
        (void)floor.request_set_down();
    }
    watch(0.3);
    if (!released || floor.snapshot().player_position.y < 777.5) {
        say("brace did not let go");
        return false;
    }
    bool settled = false;
    for (int step = 0; step < 12 * 90 && !settled; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto brace = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId));
        const auto vel = floor.kit_body_velocity(floor.kit_body_index(Simulation::kBraceEntityId));
        settled = brace.x < 1.2 && std::hypot(vel.x, vel.y) < 0.35;
    }
    if (!settled) {
        say("brace did not settle");
        return false;
    }
    auto walk_slow = [&](const double x, const double z, const double seconds, const double tolerance,
                          const double min_y) {
        const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto state = floor.snapshot();
            if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance &&
                state.player_grounded && state.player_position.y > min_y) {
                (void)floor.set_move_input(0.0, 0.0);
                return true;
            }
            steer_toward(floor, x, z, 0.34);
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
        (void)floor.set_move_input(0.0, 0.0);
        return false;
    };
    if (!walk_slow(3.6, -142.90, 8.0, 0.40, 778.2)) {
        say("brace not on the foot");
        return false;
    }
    if (!walk_slow(-1.2, -142.90, 16.0, 0.50, 780.0)) {
        say("brace walk");
        return false;
    }
    if (!walk_slow(-4.0, -142.90, 10.0, 0.45, 783.4)) {
        say("brace deck");
        return false;
    }
    const auto landed = floor.snapshot();
    if (!(landed.player_grounded && landed.death_count == 0 && landed.player_position.y > 783.4 &&
          landed.player_position.x < -2.8 && landed.support_entity_id == Simulation::kCraneFrameEntityId)) {
        say("brace high deck");
        return false;
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "brace snap " << g_path_watch.worst << " at " << g_path_watch.worst_at.x << " "
                  << g_path_watch.worst_at.y << " " << g_path_watch.worst_at.z << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the brace bay must not snap the body more than 0.15 m in one tick");
    std::cout << "PASS scraperx_sim brace: floor_y=" << landed.player_position.y
              << " hinge=" << brace_hinge_angle(floor) << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// Off the west edge before the brace has fallen. The shelf catches you,
// and the ladder on it comes back to the perch.
bool miss_brace(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto say = [&](const char *what) {
        const auto p = floor.snapshot();
        const auto brace = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId));
        std::cout << what << " " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " grounded=" << p.player_grounded << " deaths=" << p.death_count << " brace_x=" << brace.x
                  << "\n";
    };
    const auto held = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId));
    if (held.x < 2.5) {
        say("brace miss already down");
        return false;
    }
    // The caught brace stands in the west edge. Go round it to the north,
    // then off. The target sits just past the edge so the step off is slow.
    if (!walk_to(floor, 5.40, -140.95, 5.0, 0.16)) {
        say("brace miss around");
        return false;
    }
    if (!walk_to(floor, 4.05, -140.95, 4.0, 0.20)) {
        say("brace miss off the perch");
        return false;
    }
    bool down = false;
    for (int step = 0; step < 6 * 90 && !down; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        down = state.player_grounded && state.death_count == 0 && state.player_position.y > 774.1 &&
               state.player_position.y < 775.2 && state.player_position.x < 4.2;
    }
    if (!down) {
        say("brace miss did not land on the shelf");
        return false;
    }
    if (!climb_wet_hold(floor, 3.20, -141.70, 1.0, 0.0, false, 778.4)) {
        say("brace miss ladder");
        return false;
    }
    const auto back = floor.snapshot();
    if (!(back.death_count == 0 && back.player_grounded && back.player_position.y > 778.4 &&
          floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kBraceEntityId)).x > 2.5)) {
        say("brace miss not back on the perch");
        return false;
    }
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15, "missing the brace must not snap the body more than 0.15 m in one tick");
    std::cout << "PASS scraperx_sim brace miss: floor_y=" << back.player_position.y
              << " shelf_ok=1 worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


void run_girder() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation floor(InitialSpawn::AfterCrane);
    require(floor.advance_frame(0.6).accepted, "girder settle must be accepted");
    const auto stood = floor.snapshot();
    require(stood.player_grounded && stood.player_position.y > 640.8 && stood.player_position.y < 642.0 &&
                stood.death_count == 0 && stood.support_entity_id == Simulation::kCraneFrameEntityId,
            "the player must start standing on the 640 m floor");
    require(kit_y(floor, Simulation::kGirderEntityId) > 643.4, "the girder must start standing, not already fallen");
    require(climb_girder(floor), "up the girder onto the next floor");
    require(climb_shutter(floor), "the sheet aside, then up the ladder");
    require(climb_ramp(floor), "the pin out, then up the ramp");
    require(climb_gate(floor), "the gate aside, then up the ladder");
    require(climb_cart(floor), "onto the cart, then up the rail");
    require(climb_slats(floor), "up the cooling-tower slats");
    require(climb_duct(floor), "the duct down, then up it");
    require(climb_casing(floor), "up the outside of the casing");
    require(climb_pitman(floor), "the pitman over the gap north of the gallery");
    require(climb_wheel(floor), "onto the wheel, then up to the next deck");
    require(climb_brace(floor), "up the brace bay onto the next deck");
}


