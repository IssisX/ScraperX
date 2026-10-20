// The mechanism ascent in one run, and Band 0 the Stack's stage helpers (S1 .. C3).

#include "sim_tests.hpp"

#include "sim/loose_stock.hpp"

// ---- The mechanism ascent in one run ---------------------------------------------

// Fails the run with where it stopped: the leg, the height and the time.
void require_leg(const scraperx::sim::Simulation &simulation, const bool ok, const char *leg) {
    if (!ok) {
        const auto state = simulation.snapshot();
        std::cout << "ascent: stopped at " << leg << ", y=" << state.player_position.y
                  << " t=" << state.simulation_time_seconds << '\n';
    }
    require(ok, leg);
}

// The goal's test (MECHANISM_ASCENT_PLAN.md, AS-006 to AS-009): one run on
// player inputs from the 154 m deck to standing on TP-640,
// riding and climbing through all four bands' linked stages -- the
// Counterweight Well, Wet Isolation, the Plate Shop and the Facade Crane
// Stack -- with no spawn, placement or teleport between them.
void run_ascent() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    Simulation run(InitialSpawn::Deck154);
    (void)run.advance_frame(1.0);
    const double start = run.snapshot().simulation_time_seconds;
    const auto deaths = run.snapshot().death_count;

    // B02, the Counterweight Well: A, B, C to the 220 ring.
    require_leg(run, board_well_a(run) && rig_well_a(run) && pull_well_a(run, 0.5, 3.0), "A rigged and tripped");
    require_leg(run,
                wait_for(run, 16.0,
                         [](const Snapshot &state) { return state.well_a_cage_travel >= kWellATravel - 0.01; }),
                "A to the 176 ring");
    (void)run.advance_frame(1.0);
    require_leg(run,
                walk_to(run, -9.4, -131.2, 4.0) && walk_to(run, -7.6, -131.2, 4.0) && rig_well_b(run) &&
                    pull_well_b(run),
                "across into B, rigged and tripped");
    require_leg(run, wait_for(run, 16.0, [](const Snapshot &state) {
                    return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
                }),
                "B to the 198 ring");
    (void)run.advance_frame(3.0);
    require_leg(run,
                walk_to(run, -6.2, -131.9, 4.0) && walk_to(run, -4.4, -131.9, 4.0) && clear_well_c_chute(run) &&
                    fill_well_c(run, 10.0) && pull_well_c_latch(run, 3.0),
                "across onto C, its chute cleared, filled and let go");
    require_leg(run, wait_for(run, 16.0, [](const Snapshot &state) {
                    return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
                }),
                "C to the 220 ring");
    (void)run.advance_frame(2.0);
    require_leg(run, walk_to(run, -3.0, -129.0, 4.0), "off C onto the 220 ring");
    const double at_220 = run.snapshot().simulation_time_seconds - start;

    // B03, Wet Isolation: D, E, F to TP-340.
    require_leg(run, walk_to(run, 2.5, -129.6, 8.0) && walk_to(run, 4.0, -129.2, 6.0) && seat_wet_spool(run),
                "along the 220 ring, D's spool carried aboard and seated");
    require_leg(run,
                wait_for(run, 120.0,
                         [&](const Snapshot &) { return run.wet_state().d_platform_travel >= 35.95; }),
                "D to 256.25");
    require_leg(run, drop_into_wet_e(run) && wait_wet_e_top(run, 60.0),
                "through E's doors onto its deck, blown up to 298.25");
    require_leg(run,
                walk_to(run, 13.4, -137.2, 4.0) && walk_to(run, 13.4, -139.9, 6.0) && couple_wet_hose(run) &&
                    pull_wet_stop_valve(run),
                "onto F, its hose coupled and its valve thrown");
    require_leg(run,
                wait_for(run, 90.0,
                         [&](const Snapshot &) { return run.wet_state().f_platform_travel >= 41.95; }),
                "F to TP-340");
    require_leg(run, walk_to(run, 13.0, -137.2, 6.0), "off F onto TP-340");
    const double at_340 = run.snapshot().simulation_time_seconds - start;

    // B04, the Plate Shop: G, H, I and the ladder to the 484 ring.
    require_leg(run, walk_to_shop_cleat(run) && rig_shop_g(run) && pull_shop_g(run), "G rigged and its pin pulled");
    require_leg(run,
                wait_for(run, 60.0,
                         [&](const Snapshot &) { return run.shop_state().g_platform_travel >= 33.7; }),
                "G to the 374 ring");
    (void)run.advance_frame(1.0);
    require_leg(run, walk_to(run, -14.5, -149.3, 8.0) && pull_shop_girder_pin(run) && board_shop_h(run) &&
                         pull_shop_chock(run),
                "the girder's tail pin out, over the gangway onto H, its chock pulled");
    require_leg(run,
                wait_for(run, 90.0,
                         [&](const Snapshot &) { return run.shop_state().h_platform_travel >= 43.9; }),
                "H to the 418 ring");
    (void)run.advance_frame(1.0);
    require_leg(run, rig_shop_i(run) && pull_shop_domino(run), "onto I's cage, hooked on, the domino's pin pulled");
    require_leg(run,
                wait_for(run, 90.0, [&](const Snapshot &) { return run.shop_state().i_cage_travel >= 43.9; }),
                "I to 462.25");
    (void)run.advance_frame(1.0);
    require_leg(run, climb_wet_hold(run, -7.6, -145.6, -1.0, 0.0, false, 484.5), "up the ladder onto the 484 ring");
    const double at_484 = run.snapshot().simulation_time_seconds - start;

    // B05, the Facade Crane Stack: J, K, L into TP-640.
    require_leg(run, climb_crane_band(run), "J, K and L to standing on TP-640");
    const auto top = run.snapshot();
    require(top.death_count == deaths && standing_above(top, 640.2),
            "ascent: one run from the 154 m deck must end standing on TP-640, never having died");
    std::cout << "PASS scraperx_sim ascent 154 to TP-640: seconds=" << top.simulation_time_seconds - start
              << " at_220=" << at_220 << " at_340=" << at_340 << " at_484=" << at_484
              << " plate_y=" << top.player_position.y << '\n';
}
constexpr double kDeck4Top = 44.0;

// S1's scale plate, in the middle of the cage's floor.
constexpr double kS1PlateX = 10.00;
constexpr double kS1PlateZ = -120.80;

// From wherever the player stands in the yard, in through S1's open south
// side and onto the scale plate in the middle of the cage's floor. Nothing is
// pressed: the rider's weight takes the plate down onto the floor, and its
// line turns the valve's lever until the pawl drops behind it. True once the
// pawl holds the valve open.
bool board_s1(scraperx::sim::Simulation &simulation) {
    if (!(walk_to(simulation, 10.0, -117.3, 30.0) && walk_to(simulation, kS1PlateX, kS1PlateZ, 6.0, 0.08))) {
        return false;
    }
    return advance_until(
        simulation, [&](const scraperx::sim::Snapshot &) { return simulation.stack_state().s1_valve_pawled; }, 1.0);
}

// Whether S1 waits as a rider finds it: the plate hanging proud of the floor,
// the pawl out and the valve shut.
bool s1_plate_at_rest(const scraperx::sim::Simulation &simulation) {
    const auto stack = simulation.stack_state();
    return std::abs(stack.s1_plate_travel) <= 0.01 && !stack.s1_valve_pawled && stack.s1_valve_angle < 0.03;
}

// Standing on the plate, with no input, until the cage stops at the top of
// its travel (or `seconds` pass). Every tick, the energy the bucket and the
// water in it have released falling is compared with what the cage and the
// rider have gained rising.
S1Ride ride_s1(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    S1Ride ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cage_y0 = kit_y(simulation, Simulation::kStackS1CageEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    double bucket_y = kit_y(simulation, Simulation::kStackS1BucketEntityId);
    double released = 0.0;
    const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !ride.reached_top; ++tick) {
        const double water = simulation.stack_state().s1_bucket_water_kg;
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.set_facing(-1.0, 0.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = simulation.snapshot();
        const auto stack = simulation.stack_state();
        const double bucket_now = kit_y(simulation, Simulation::kStackS1BucketEntityId);
        released += (kS1BucketMassKg + water) * kGravity * (bucket_y - bucket_now);
        bucket_y = bucket_now;
        const double rise = kit_y(simulation, Simulation::kStackS1CageEntityId) - cage_y0;
        if (ride.lift_water_kg == 0.0 && rise > 0.01) {
            ride.lift_water_kg = stack.s1_bucket_water_kg;
        }
        if (ride.shut_travel < 0.0 && rise > 0.01 && stack.s1_valve_angle < 0.06) {
            ride.shut_travel = rise;
            ride.shut_water_kg = stack.s1_bucket_water_kg;
            ride.shut_tank_kg = stack.s1_tank_water_kg;
        }
        if (rise > 0.05 && rise < kS1Travel - 0.05) {
            // On the plate, which rides on the cage's floor, or the floor itself.
            ride.rode_on_cage = ride.rode_on_cage && state.player_grounded &&
                                (state.support_entity_id == Simulation::kStackS1CageEntityId ||
                                 state.support_entity_id == Simulation::kStackS1PlateEntityId);
        }
        const double gained =
            kS1CageMassKg * kGravity * rise + kRiderMassKg * kGravity * (state.player_position.y - rider_y0);
        if (released - gained < ride.worst_margin_j) {
            ride.worst_margin_j = released - gained;
            ride.worst_rise = rise;
        }
        if (stack.s1_cage_travel >= kS1Travel - 0.01) {
            ride.reached_top = true;
            ride.seconds = state.simulation_time_seconds - start;
            ride.top_water_kg = stack.s1_bucket_water_kg;
            ride.top_tank_kg = stack.s1_tank_water_kg;
        }
    }
    (void)simulation.advance_frame(0.5);
    return ride;
}

// ---- Band 0, the Stack: C1, the facade -----------------------------------------

// Where a leg of C1 left the body, for a failure's message.
void report_c1(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "C1 " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " grounded=" << state.player_grounded
              << " traversal=" << int(state.traversal_state) << " support=" << state.support_entity_id
              << " ledge=" << state.ledge_available << " grip=" << state.grip_available << "\n";
}

bool climb_c1(scraperx::sim::Simulation &simulation, C1Notes *notes) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    // Out onto the landing, round the cabinet to its south side.
    if (!(walk_to(simulation, 21.2, -124.4, 20.0) && walk_to(simulation, 21.2, -121.3, 6.0) &&
          walk_to(simulation, 20.0, -121.3, 6.0, 0.08))) {
        report_c1(simulation, "to the cabinet");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        report_c1(simulation, "cabinet offered");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 24.5); })) {
        report_c1(simulation, "mantle onto the cabinet");
        return false;
    }
    // On the cabinet, clear of the duct's underside: jump, stick to the duct.
    (void)walk_to(simulation, 20.0, -122.10, 2.0, 0.05);
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.4, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        report_c1(simulation, "hang on the duct's lip");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 28.0); })) {
        report_c1(simulation, "up onto the duct");
        return false;
    }
    // Along the duct to the vent stack, and up it over deck 3's edge.
    if (!(walk_to(simulation, 22.5, -123.05, 6.0, 0.1) && walk_to(simulation, 24.0, -123.00, 6.0, 0.06))) {
        report_c1(simulation, "along the duct");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        report_c1(simulation, "vent offered");
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 33.5); })) {
        report_c1(simulation, "up the vent onto deck 3");
        return false;
    }
    // Along deck 3 to the monorail, out along it under the ladder to its end.
    if (!(walk_to(simulation, 22.0, -125.2, 6.0) && walk_to(simulation, 12.5, -125.2, 12.0, 0.08) &&
          walk_to(simulation, 12.5, -119.65, 12.0, 0.06))) {
        report_c1(simulation, "out along the monorail");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    const auto at_end = simulation.snapshot();
    if (!at_end.player_grounded || at_end.player_position.y < 34.0) {
        report_c1(simulation, "standing at the monorail's end");
        return false;
    }
    // Turn to the ladder and leap for it: standing, it is out of reach.
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    if (notes != nullptr) {
        notes->ladder_in_reach_standing = simulation.snapshot().grip_available;
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.3, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return is_climbing(state); })) {
        report_c1(simulation, "catch the ladder");
        return false;
    }
    // Up the ladder onto the davit's arm, and back along it onto deck 4.
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 44.5); })) {
        report_c1(simulation, "up the ladder onto the arm");
        return false;
    }
    if (!walk_to(simulation, 12.5, -125.0, 10.0, 0.1)) {
        report_c1(simulation, "back along the arm");
        return false;
    }
    if (!walk_to(simulation, 11.2, -125.3, 4.0, 0.1)) {
        report_c1(simulation, "off the arm onto deck 4");
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto on_deck4 = simulation.snapshot();
    return on_deck4.player_grounded && on_deck4.player_position.y > kDeck4Top + 0.5 &&
           on_deck4.support_entity_id == Simulation::kTowerEntityId;
}

bool climb_c1_to_monorail(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    if (!(walk_to(simulation, 21.2, -124.4, 20.0) && walk_to(simulation, 21.2, -121.3, 6.0) &&
          walk_to(simulation, 20.0, -121.3, 6.0, 0.08))) {
        report_c1(simulation, "to the cabinet");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        report_c1(simulation, "cabinet offered");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 24.5); })) {
        report_c1(simulation, "mantle onto the cabinet");
        return false;
    }
    (void)walk_to(simulation, 20.0, -122.10, 2.0, 0.05);
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.4, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        report_c1(simulation, "hang on the duct's lip");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 28.0); })) {
        report_c1(simulation, "up onto the duct");
        return false;
    }
    if (!(walk_to(simulation, 22.5, -123.05, 6.0, 0.1) && walk_to(simulation, 24.0, -123.00, 6.0, 0.06))) {
        report_c1(simulation, "along the duct");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        report_c1(simulation, "vent offered");
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 33.5); })) {
        report_c1(simulation, "up the vent onto deck 3");
        return false;
    }
    if (!(walk_to(simulation, 22.0, -125.2, 6.0) && walk_to(simulation, 12.5, -125.2, 12.0, 0.08) &&
          walk_to(simulation, 12.5, -119.65, 12.0, 0.06))) {
        report_c1(simulation, "out along the monorail");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    return simulation.snapshot().player_grounded && simulation.snapshot().player_position.y >= 34.0;
}
constexpr double kS2CageMassKg = 300.0;
constexpr double kS2CartMassKg = 1500.0; // West arm pig-iron ballast cart (beam total 2400 kg incl 900 kg frame)
constexpr double kS2Travel = 22.0;

// Into S2's cage from deck 4, round to the shoot bolt's push plate, and
// walking into it until the bolt is out and the cage is free.
bool shove_s2_bolt(scraperx::sim::Simulation &simulation) {
    if (!(walk_to(simulation, 10.0, -130.5, 30.0) && walk_to(simulation, 10.0, -137.6, 20.0, 0.10) &&
          walk_to(simulation, 9.1, -138.3, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    for (int tick = 0; tick < 6 * 90; ++tick) {
        (void)simulation.set_move_input(0.0, 0.6);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
        if (!simulation.stack_state().s2_bolt_shot) {
            (void)simulation.set_move_input(0.0, 0.0);
            return true;
        }
    }
    (void)simulation.set_move_input(0.0, 0.0);
    return false;
}

bool s2_bolt_at_rest(const scraperx::sim::Simulation &simulation) {
    const auto stack = simulation.stack_state();
    return stack.s2_bolt_shot && std::abs(stack.s2_bolt_travel) <= 0.02;
}

S2Ride ride_s2(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    S2Ride ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cage_y0 = kit_y(simulation, Simulation::kStackS2CageEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !ride.reached_top; ++tick) {
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.set_facing(0.0, -1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = simulation.snapshot();
        const auto stack = simulation.stack_state();
        const double rise = kit_y(simulation, Simulation::kStackS2CageEntityId) - cage_y0;
        const double rider_rise = state.player_position.y - rider_y0;
        const double payload_gained = (kS2CageMassKg + 80.0) * kGravity * rider_rise;
        const double released = kS2CartMassKg * kGravity * rise;
        if (rise > 0.05) {
            ride.worst_margin_j = std::min(ride.worst_margin_j, released - payload_gained);
            if (state.support_entity_id != Simulation::kStackS2CageEntityId) {
                ride.rode_on_cage = false;
            }
        }
        if (stack.s2_cage_travel >= kS2Travel - 0.10) {
            ride.reached_top = true;
            ride.seconds = state.simulation_time_seconds - start;
            (void)simulation.request_set_down();
            (void)simulation.advance_frame(0.2);
            break;
        }
    }
    return ride;
}

// ---- C2: The East Machinery Hall & Pipe Rack (66 -> 88 m) -------------------

[[maybe_unused]] constexpr double kDeck7Top = 77.0;
constexpr double kDeck8Top = 88.0;

void report_c2(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "C2 " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " grounded=" << state.player_grounded
              << " traversal=" << int(state.traversal_state) << " support=" << state.support_entity_id
              << " ledge=" << state.ledge_available << " grip=" << state.grip_available << "\n";
}

bool climb_c2(scraperx::sim::Simulation &simulation, C2Notes *notes) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    if (!walk_to(simulation, 22.0, -125.5, 20.0)) {
        report_c2(simulation, "walk along south band to x=22");
        return false;
    }
    if (!walk_to(simulation, 22.0, -135.7, 15.0, 0.08)) {
        report_c2(simulation, "walk north to cabinet");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        report_c2(simulation, "cabinet ledge offered");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 68.5); })) {
        report_c2(simulation, "mantle onto cabinet");
        return false;
    }
    (void)walk_to(simulation, 22.0, -138.6, 2.0, 0.05);
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.4, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        report_c2(simulation, "hang on duct lip");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 72.0); })) {
        report_c2(simulation, "up onto duct");
        return false;
    }
    if (!(walk_to(simulation, 22.0, -142.0, 6.0, 0.1) && walk_to(simulation, 22.0, -148.05, 6.0, 0.06))) {
        report_c2(simulation, "along duct to ladder");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        report_c2(simulation, "wall ladder grip offered");
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 77.5); })) {
        report_c2(simulation, "up wall ladder onto deck 7");
        return false;
    }
    if (!(walk_to(simulation, 22.0, -155.0, 10.0) && walk_to(simulation, 20.0, -160.20, 10.0, 0.08))) {
        report_c2(simulation, "on deck 7 to pipe rack");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        report_c2(simulation, "pipe rack ledge offered");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 79.5); })) {
        report_c2(simulation, "mantle onto pipe rack");
        return false;
    }
    if (!(walk_to(simulation, 18.0, -162.0, 6.0, 0.1) && walk_to(simulation, 10.0, -162.0, 15.0, 0.08))) {
        report_c2(simulation, "along monorail beam");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    const auto at_monorail_end = simulation.snapshot();
    if (!at_monorail_end.player_grounded || at_monorail_end.player_position.y < 82.5) {
        report_c2(simulation, "standing at monorail end");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    if (notes != nullptr) {
        notes->ladder_in_reach_standing = simulation.snapshot().grip_available;
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.3, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return is_climbing(state); })) {
        report_c2(simulation, "catch davit ladder");
        return false;
    }
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 88.5); })) {
        report_c2(simulation, "up davit ladder onto deck 8");
        return false;
    }
    if (!(walk_to(simulation, 10.0, -165.0, 6.0, 0.1) && walk_to(simulation, 10.0, -171.2, 6.0, 0.1))) {
        report_c2(simulation, "walk off davit arm onto deck 8");
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto on_deck8 = simulation.snapshot();
    return on_deck8.player_grounded && on_deck8.player_position.y > kDeck8Top + 0.5 &&
           on_deck8.support_entity_id == Simulation::kTowerEntityId;
}
constexpr double kS3CageMassKg = 400.0;
constexpr double kS3CarMassKg = 3500.0;
constexpr double kS3Travel = 44.0;

// From deck 8's gangway into S3's cage with the pig of iron lying beside the
// way in (LooseStock::lay_machine_weights), set in the pan its brake's trip
// line ends in: true once the brake has tripped.
bool load_s3_pan(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto at = [&simulation](const char *where) {
        const auto p = simulation.snapshot().player_position;
        std::cout << "DEBUG load_s3_pan short of " << where << ", pos=(" << p.x << "," << p.y << "," << p.z << ")\n";
        return false;
    };
    if (!walk_to(simulation, -8.0, -170.0, 20.0)) {
        return at("(-8, -170)");
    }
    if (!walk_to(simulation, -8.0, -164.0, 10.0)) {
        return at("(-8, -164)");
    }
    if (!walk_to(simulation, -8.35, -161.8, 6.0, 0.06)) {
        return at("the pig");
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    // The machines' weights end where the Matron's throwables begin.
    const std::uint64_t pig = scraperx::sim::LooseStock::kFirstEntity + simulation.loose_stock_count() - 1 -
                              scraperx::sim::LooseStock::kMatronThrowables;
    if (simulation.snapshot().carry_target_entity_id != pig) {
        std::cout << "DEBUG load_s3_pan target_id=" << simulation.snapshot().carry_target_entity_id << " not the pig "
                  << pig << "\n";
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != pig) {
        return at("the pig taken");
    }
    // In, short of the pan in the cage's middle, facing it; still a moment
    // for the pig to stop swinging in the hands; and set down into it.
    if (!walk_to(simulation, -8.0, -158.75, 12.0, 0.06)) {
        return at("the pan");
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(1.0);
    (void)simulation.request_set_down();
    for (int tick = 0; tick < 2 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        if (!simulation.stack_state().s3_brake_latched) {
            return true;
        }
    }
    const auto in = simulation.kit_body_position(simulation.kit_body_index(pig));
    std::cout << "DEBUG load_s3_pan the brake held, the pig at (" << in.x << "," << in.y << "," << in.z << ")\n";
    return false;
}

// S3's pan hanging empty and level in the cage's middle, where it was built.
bool s3_pan_at_rest(const scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto body = simulation.kit_body_index(Simulation::kStackS3HandleEntityId);
    const auto at = simulation.kit_body_position(body);
    const auto turn = simulation.kit_body_rotation(body);
    return std::abs(at.x - (-8.00)) <= 0.05 && std::abs(at.y - (88.05 + 0.75 - 0.012)) <= 0.05 &&
           std::abs(at.z - (-158.00)) <= 0.05 && std::abs(turn.w) > std::cos(0.5 * 0.05);
}

S3Ride ride_s3(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    S3Ride ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cage_y0 = kit_y(simulation, Simulation::kStackS3CageEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !ride.reached_top; ++tick) {
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.set_facing(0.0, -1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = simulation.snapshot();
        const auto stack = simulation.stack_state();
        const double rise = kit_y(simulation, Simulation::kStackS3CageEntityId) - cage_y0;
        const double rider_rise = state.player_position.y - rider_y0;
        const double payload_gained = (kS3CageMassKg + 80.0) * kGravity * rider_rise;
        const double released = kS3CarMassKg * kGravity * rise;
        if (rise > 0.05) {
            ride.worst_margin_j = std::min(ride.worst_margin_j, released - payload_gained);
            if (state.support_entity_id != Simulation::kStackS3CageEntityId) {
                ride.rode_on_cage = false;
            }
        }
        if (stack.s3_cage_travel >= kS3Travel - 0.10) {
            ride.reached_top = true;
            ride.seconds = state.simulation_time_seconds - start;
            (void)simulation.request_set_down();
            (void)simulation.advance_frame(0.2);
            break;
        }
    }
    return ride;
}

// ---- C3: The Crown Trusses & High Riser Ladder (132 -> 154 m) ---------------

[[maybe_unused]] constexpr double kDeck13Top = 143.0;

bool climb_c3(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;

    if (!walk_to(simulation, -10.5, -171.0, 10.0, 0.1)) {
        std::cout << "DEBUG climb_c3 failed walk to (-10.5, -171), pos=("
                  << simulation.snapshot().player_position.x << ","
                  << simulation.snapshot().player_position.y << ","
                  << simulation.snapshot().player_position.z << ")\n";
        return false;
    }
    if (!walk_to(simulation, -10.5, -145.0, 20.0, 0.1)) {
        std::cout << "DEBUG climb_c3 failed walk to (-10.5, -145), pos=("
                  << simulation.snapshot().player_position.x << ","
                  << simulation.snapshot().player_position.y << ","
                  << simulation.snapshot().player_position.z << ")\n";
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        std::cout << "DEBUG climb_c3 ledge not available at (-10.5, -145), pos=("
                  << simulation.snapshot().player_position.x << ","
                  << simulation.snapshot().player_position.y << ","
                  << simulation.snapshot().player_position.z << ")\n";
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 138.0); })) {
        std::cout << "DEBUG climb_c3 wait_for standing_above 138.0 failed\n";
        return false;
    }
    if (!walk_to(simulation, -10.5, -135.85, 6.0, 0.08)) {
        std::cout << "DEBUG climb_c3 failed walk to (-10.5, -135.85), pos=("
                  << simulation.snapshot().player_position.x << ","
                  << simulation.snapshot().player_position.y << ","
                  << simulation.snapshot().player_position.z << ")\n";
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        std::cout << "DEBUG climb_c3 grip not available at (-10.5, -135.85), pos=("
                  << simulation.snapshot().player_position.x << ","
                  << simulation.snapshot().player_position.y << ","
                  << simulation.snapshot().player_position.z << ")\n";
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 143.5); })) {
        std::cout << "DEBUG climb_c3 ladder to deck 13 failed, pos=("
                  << simulation.snapshot().player_position.x << ","
                  << simulation.snapshot().player_position.y << ","
                  << simulation.snapshot().player_position.z << ")\n";
        return false;
    }
    if (!walk_to(simulation, -6.0, -133.85, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 30.0,
                    [](const Snapshot &state) { return standing_above(state, 154.5); })) {
        return false;
    }
    if (!(walk_to(simulation, -6.0, -130.0, 4.0, 0.1) &&
          walk_to(simulation, -6.0, -128.2, 4.0, 0.1) &&
          walk_to(simulation, -10.5, -128.2, 6.0, 0.1))) {
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto on_deck14 = simulation.snapshot();
    return on_deck14.player_grounded && on_deck14.player_position.y > kDeck14Top + 0.5 &&
           on_deck14.support_entity_id == Simulation::kTowerEntityId;
}
