#include "sim_checks.hpp"
#include "sim_routes.hpp"

// The lower stack, and the one run from the 154 m deck up to the next floor.

// ---- The mechanism ascent in one run ---------------------------------------------

// Fails the run with where it stopped: the leg, the height and the time.

// The girder on the 640 m floor. Defined with the other floor checks below.
bool climb_girder(scraperx::sim::Simulation &floor);
bool climb_shutter(scraperx::sim::Simulation &floor);
bool climb_above_ring220(scraperx::sim::Simulation &run, double start_seconds, std::uint64_t deaths_at_start,
                         const char *pass_name);

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
    require(climb_above_ring220(run, start, deaths, "ascent 154 to the next floor"),
            "from the 220 ring up through the sheet and the ladder");
}


// ---- Band 0, the Stack: S1, the water-balance hoist -------------------------

constexpr double kS1CageMassKg = 300.0;
constexpr double kS1BucketMassKg = 150.0;
constexpr double kS1Travel = 21.8;
constexpr double kS1FloorTopUp = 22.05;
constexpr double kDeck2Top = 22.0;
constexpr double kDeck4Top = 44.0;

// Where S1's valve chain hangs at rest, inside the cage at grade: its
// handle's centre.
constexpr double kS1ChainX = 9.50;
constexpr double kS1ChainY = 2.16;
constexpr double kS1ChainZ = -120.10;
// Water that balances the cage and an 85 kg rider against the empty bucket.
constexpr double kS1BalanceKg = kS1CageMassKg + 85.0 - kS1BucketMassKg;

// From wherever the player stands in the yard, in through S1's open south
// side to stand beside the valve chain hanging inside the cage: face it and
// take hold. Hanging above the hands, it comes down to them, and that pulls
// the valve's lever down. True once the chain is in the hands.
bool take_s1_chain(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, 10.0, -117.3, 30.0) && walk_to(simulation, 10.0, kS1ChainZ, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kStackS1ChainEntityId || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kStackS1ChainEntityId;
}


// Whether S1's chain hangs where a rider at grade can take it again.
bool s1_chain_at_rest(const scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto at = simulation.kit_body_position(simulation.kit_body_index(Simulation::kStackS1ChainEntityId));
    return std::abs(at.x - kS1ChainX) <= 0.10 && std::abs(at.y - kS1ChainY) <= 0.06 &&
           std::abs(at.z - kS1ChainZ) <= 0.10 && simulation.snapshot().carrying_entity_id == 0;
}


// What one ride of S1 saw, from taking hold of the chain at grade to the top.
struct S1Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;          // from taking hold to the top
    double lift_water_kg = 0.0;    // in the bucket as the cage left the yard
    double shut_travel = -1.0;     // the cage's travel when the valve shut
    double shut_water_kg = 0.0;
    double shut_tank_kg = 0.0;
    double top_water_kg = 0.0;
    double top_tank_kg = 0.0;
    double worst_rise = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

// Holding the chain, with no other input, until the cage stops at the top of
// its travel (or `seconds` pass); then let go. Every tick, the energy the
// bucket and the water in it have released falling is compared with what the
// cage and the rider have gained rising.
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
            ride.rode_on_cage = ride.rode_on_cage && state.player_grounded &&
                                state.support_entity_id == Simulation::kStackS1CageEntityId;
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
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
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


// From deck 2's south band up C1 to deck 4, on player inputs: out onto the
// landing, up onto the cabinet, a jump to hang from the duct's lip, up onto
// the duct and along it, up the vent stack over deck 3's edge; along deck 3,
// out along the monorail under the davit's ladder, a turn and a leap for it,
// up it onto the davit's arm, and back along the arm onto deck 4.
// What a climb of C1 saw on the way, for its falsifiers.
struct C1Notes final {
    bool ladder_in_reach_standing = true;
};

bool climb_c1(scraperx::sim::Simulation &simulation, C1Notes *notes = nullptr) {
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


// ---- Band 0, the Stack: S2, the walking beam hoist with fixed ballast cart --

constexpr double kDeck6Top = 66.0;
constexpr double kS2CageMassKg = 300.0;
constexpr double kS2CartMassKg = 1500.0; // West arm pig-iron ballast cart (beam total 2400 kg incl 900 kg frame)
constexpr double kS2Travel = 22.0;
constexpr double kS2FloorTopUp = 66.05;

bool take_s2_handle(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, 10.0, -130.5, 30.0) && walk_to(simulation, 10.0, -137.6, 20.0, 0.10))) {
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kStackS2HandleEntityId || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kStackS2HandleEntityId;
}


bool s2_handle_at_rest(const scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto at = simulation.kit_body_position(simulation.kit_body_index(Simulation::kStackS2HandleEntityId));
    return std::abs(at.x - 10.00) <= 0.15 && std::abs(at.y - (44.05 + 1.95)) <= 0.10 &&
           std::abs(at.z - (-138.00)) <= 0.15 && simulation.snapshot().carrying_entity_id == 0;
}


struct S2Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

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

struct C2Notes final {
    bool ladder_in_reach_standing = true;
};

void report_c2(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "C2 " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " grounded=" << state.player_grounded
              << " traversal=" << int(state.traversal_state) << " support=" << state.support_entity_id
              << " ledge=" << state.ledge_available << " grip=" << state.grip_available << "\n";
}


bool climb_c2(scraperx::sim::Simulation &simulation, C2Notes *notes = nullptr) {
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


// ---- S3: The brake-override counterweight hoist (88 -> 132 m) ----------------

constexpr double kDeck12Top = 132.0;
constexpr double kS3CageMassKg = 400.0;
constexpr double kS3CarMassKg = 3500.0;
constexpr double kS3Travel = 44.0;
constexpr double kS3FloorTopUp = 132.05;

bool take_s3_handle(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -8.0, -170.0, 20.0)) {
        const auto p = simulation.snapshot().player_position;
        std::cout << "DEBUG take_s3_handle failed walk to (-8, -170), pos=(" << p.x << "," << p.y << "," << p.z << ")\n";
        return false;
    }
    if (!walk_to(simulation, -8.0, -164.0, 10.0)) {
        const auto p = simulation.snapshot().player_position;
        std::cout << "DEBUG take_s3_handle failed walk to (-8, -164), pos=(" << p.x << "," << p.y << "," << p.z << ")\n";
        return false;
    }
    if (!walk_to(simulation, -8.0, -157.6, 10.0, 0.08)) {
        const auto p = simulation.snapshot().player_position;
        std::cout << "DEBUG take_s3_handle failed walk to (-8, -157.6), pos=(" << p.x << "," << p.y << "," << p.z << ")\n";
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kStackS3HandleEntityId || facing.carry_target_kind != 2) {
        std::cout << "DEBUG take_s3_handle target_id=" << facing.carry_target_entity_id
                  << " kind=" << facing.carry_target_kind << "\n";
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kStackS3HandleEntityId;
}


bool s3_handle_at_rest(const scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto at = simulation.kit_body_position(simulation.kit_body_index(Simulation::kStackS3HandleEntityId));
    return std::abs(at.x - (-8.00)) <= 0.15 && std::abs(at.y - (88.05 + 1.95)) <= 0.10 &&
           std::abs(at.z - (-158.00)) <= 0.15 && simulation.snapshot().carrying_entity_id == 0;
}


struct S3Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

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
constexpr double kDeck14Top = 154.0;

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


// Off S1 onto deck 2. The north door is a plate. East: vault the sill onto
// the grate, balance the stringer, mantle the corbel, step down onto the
// tower. Every move is a player input against the real controller.
void report_s1_exit(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "S1 exit " << leg << ": at=" << state.player_position.x << "," << state.player_position.y
              << "," << state.player_position.z << " grounded=" << state.player_grounded
              << " traversal=" << int(state.traversal_state) << " support=" << state.support_entity_id
              << " ledge=" << state.ledge_available << " rise=" << state.ledge_rise_meters
              << " balance=" << state.player_balancing << "\n";
}


bool exit_s1(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    // The well's north side is a plate. Walking at the deck must not reach it,
    // and must not knock the rider off the cage.
    const double pushed = walk_toward(simulation, 10.0, -128.0, 2.0);
    const auto blocked = simulation.snapshot();
    if (pushed < -122.6 || !blocked.player_grounded || blocked.player_position.y < 21.5) {
        report_s1_exit(simulation, "north door");
        std::cout << "S1 north push z=" << pushed << "\n";
        return false;
    }
    if (!walk_to(simulation, 10.50, -120.80, 5.0, 0.12)) {
        report_s1_exit(simulation, "to the sill");
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.35);
    const auto at_sill = simulation.snapshot();
    if (!at_sill.ledge_available || at_sill.ledge_rise_meters < 0.50 || at_sill.ledge_rise_meters > 1.15) {
        report_s1_exit(simulation, "sill not a vault");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) {
            return state.traversal_state == TraversalState::None && state.player_grounded &&
                   state.player_position.x > 12.05 && state.player_position.x < 13.30 &&
                   state.player_position.y > 22.2 && state.player_position.y < 23.3;
        })) {
        report_s1_exit(simulation, "vault onto the grate");
        return false;
    }
    if (!walk_to(simulation, 15.75, -120.80, 8.0, 0.14)) {
        report_s1_exit(simulation, "along the stringer");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.35);
    const auto at_corbel = simulation.snapshot();
    if (!at_corbel.ledge_available || at_corbel.ledge_rise_meters < 1.20) {
        report_s1_exit(simulation, "corbel not a mantle");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) {
            return state.traversal_state == TraversalState::None && state.player_grounded &&
                   state.player_position.y > 24.0;
        })) {
        report_s1_exit(simulation, "mantle onto the corbel");
        return false;
    }
    if (!walk_to(simulation, 15.75, -126.2, 6.0, 0.35)) {
        report_s1_exit(simulation, "off the corbel");
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto on_deck = simulation.snapshot();
    if (!(on_deck.player_grounded && on_deck.player_position.y > kDeck2Top + 0.5 &&
          on_deck.player_position.z < -124.0 && on_deck.support_entity_id == Simulation::kTowerEntityId)) {
        report_s1_exit(simulation, "not on deck 2");
        return false;
    }
    return true;
}


void run_stack() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;

    // The yard's kerb is 0.5 m wide and 86 m long, but a step high: it is
    // walked on and off, not balanced along. Held on its line as a beam, a
    // body on it could not step down off it with a gentle stick.
    Simulation kerb(InitialSpawn::ExteriorGrade);
    require(kerb.advance_frame(0.5).accepted, "kerb settle interval must be accepted");
    require(walk_to(kerb, 20.0, -117.0, 40.0) && walk_to(kerb, 20.0, -118.0, 6.0, 0.08),
            "the walk must reach the top of the yard's kerb");
    const auto on_kerb = kerb.snapshot();
    for (std::uint32_t tick = 0; tick < 90; ++tick) {
        (void)kerb.set_move_input(0.0, 0.35);
        (void)kerb.set_facing(0.0, -1.0);
        (void)kerb.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto off_kerb = kerb.snapshot();
    require(on_kerb.player_position.y > 1.1 && !on_kerb.player_balancing,
            "standing on the yard's kerb must not be balancing");
    require(off_kerb.player_position.z > on_kerb.player_position.z + 0.4 &&
                off_kerb.player_position.y < 0.95,
            "a gentle stick must step down off the yard's kerb");
    std::cout << "PASS scraperx_sim kerb: on_y=" << on_kerb.player_position.y
              << " stepped_z=" << off_kerb.player_position.z - on_kerb.player_position.z << "\n";

    // (a) A tug is not a ride: taken and let go at once, the chain opens the
    // valve for a moment and lets the catch go, but the water that runs in is
    // far short of what outweighs the cage and its rider, so nothing moves,
    // and let go, the catch seats the bucket again. The water stays in the
    // bucket: tugs add up, and held again the chain finishes the job.
    // Left alone, S1 waits as found: the lever on its stop under the chain's
    // weight, the valve shut, the bucket dry on its catch, the cage at grade.
    Simulation idle(InitialSpawn::ExteriorGrade);
    require(idle.advance_frame(30.0).accepted, "S1 idle interval must be accepted");
    const auto idle_state = idle.stack_state();
    require(idle_state.s1_valve_angle < 0.005 && idle_state.s1_bucket_water_kg < 0.01 &&
                idle_state.s1_catch_latched && std::abs(idle_state.s1_cage_travel) <= 0.01 &&
                s1_chain_at_rest(idle),
            "left alone for 30 s, S1 must wait as found");

    Simulation tug(InitialSpawn::ExteriorGrade);
    require(tug.advance_frame(0.5).accepted, "S1 settle interval must be accepted");
    const double tug_tank0 = tug.stack_state().s1_tank_water_kg;
    require(take_s1_chain(tug), "the player must walk from the yard into S1's cage and take hold of its chain");
    (void)tug.advance_frame(0.3);
    const auto tugged = tug.stack_state();
    require(tugged.s1_valve_angle >= 0.24 && !tugged.s1_catch_latched,
            "held, S1's chain must turn its lever past full open and let the catch go");
    (void)tug.request_set_down();
    double tug_cage_worst = 0.0;
    double tug_bucket_worst = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)tug.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = tug.stack_state();
        tug_cage_worst = std::max(tug_cage_worst, std::abs(state.s1_cage_travel));
        tug_bucket_worst = std::max(tug_bucket_worst, std::abs(state.s1_bucket_travel));
    }
    const auto after_tug = tug.stack_state();
    require(after_tug.s1_bucket_water_kg > 10.0 && after_tug.s1_bucket_water_kg < kS1BalanceKg - 20.0,
            "a tug on S1's chain must let some water in, short of what lifts the cage");
    require(std::abs(tug_tank0 - after_tug.s1_tank_water_kg - after_tug.s1_bucket_water_kg) < 2.0,
            "the bucket's water must have come out of S1's tank");
    require(tug_cage_worst <= 0.05 && tug_bucket_worst <= 0.05,
            "after a tug, S1's cage and bucket must not move");
    require(after_tug.s1_catch_latched && after_tug.s1_valve_angle < 0.03,
            "let go, S1's lever must come back up, shut the valve and seat the catch");
    require(s1_chain_at_rest(tug), "let go, S1's chain must hang back where it was");
    const double tug_water = after_tug.s1_bucket_water_kg;
    require(take_s1_chain(tug), "S1's chain must be taken again after a tug");
    const auto tug_ride = ride_s1(tug, 20.0);
    require(tug_ride.reached_top && tug_ride.lift_water_kg > tug_water,
            "held again after a tug, S1's chain must fill the bucket the rest of the way and lift the rider");
    std::cout << "PASS scraperx_sim S1 tug: water_kg=" << tug_water << " cage_worst=" << tug_cage_worst
              << " bucket_worst=" << tug_bucket_worst << " then_lift_kg=" << tug_ride.lift_water_kg << "\n";

    // (b) The ride, from the game's spawn on player inputs: into the cage, take
    // hold of the chain and hold it. Water runs into the bucket until it
    // outweighs the cage and the rider (235 kg); the bucket falls 21.8 m and the
    // cage carries the rider 21.8 m under a governor that can only brake. Its
    // first half metre lets the chain go slack and the valve shut. At no tick
    // has the payload gained more energy than the bucket and its water
    // released.
    Simulation ride(InitialSpawn::ExteriorGrade);
    require(ride.advance_frame(0.5).accepted, "S1 ride settle interval must be accepted");
    const double tank0 = ride.stack_state().s1_tank_water_kg;
    require(take_s1_chain(ride), "the rider must walk from the yard into S1's cage and take hold of its chain");
    const auto rode = ride_s1(ride, 20.0);
    const auto top_state = ride.stack_state();
    const double floor_y = kit_y(ride, Simulation::kStackS1CageEntityId) + 0.10;
    require(rode.reached_top, "holding S1's chain must carry the rider to the top of the cage's travel");
    require(rode.lift_water_kg >= kS1BalanceKg - 5.0 && rode.lift_water_kg <= kS1BalanceKg + 60.0,
            "S1's cage must leave the yard only once the bucket's water outweighs the cage and rider");
    require(rode.shut_travel > 0.0 && rode.shut_travel < 1.0,
            "rising, the rider must let S1's chain go slack and its valve shut within the first metre");
    require(std::abs(tank0 - rode.shut_tank_kg - rode.shut_water_kg) < 2.0,
            "every kilogram in S1's bucket must have come out of its tank");
    require(rode.shut_tank_kg - rode.top_tank_kg < 1.0 && rode.top_water_kg - rode.shut_water_kg < 1.0,
            "with the valve shut, no more water may leave S1's tank on the way up");
    require(rode.rode_on_cage, "the rider must stand on S1's cage for the whole ride");
    require(std::abs(floor_y - kS1FloorTopUp) <= 0.05, "S1's floor must stop at 22.05 m");
    require(top_state.s1_cage_peak_speed <= 2.6, "S1's governor must hold the cage to 2.5 m/s");
    require(rode.worst_margin_j >= -kStanceJitterJ,
            "at no tick may S1's payload have gained more energy than the bucket released");

    // (d) The receiver: vault the east sill, balance the stringer, mantle the
    // corbel, and stand on deck 2's south band.
    require(exit_s1(ride), "the rider must vault, balance and mantle off S1 onto deck 2");
    (void)ride.advance_frame(0.5);
    const auto on_deck = ride.snapshot();
    require(on_deck.player_grounded && on_deck.player_position.y > kDeck2Top + 0.5 &&
                on_deck.player_position.z < -124.0 &&
                on_deck.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 2's south band");

    // Recovery: at the foot of its guide the bucket sits on the striker and
    // drains; lighter than the cage again, it rises, the cage comes back down
    // and the catch seats the bucket at the top. S1 is as it was found, its
    // chain hanging in the cage where the next rider takes it.
    bool returned = false;
    double descent_water = -1.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !returned; ++tick) {
        (void)ride.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = ride.stack_state();
        if (descent_water < 0.0 && state.s1_cage_travel < kS1Travel - 0.10) {
            descent_water = state.s1_bucket_water_kg;
        }
        returned = std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched &&
                   std::abs(state.s1_bucket_travel) <= 0.05;
    }
    (void)ride.advance_frame(2.0);
    const auto back = ride.stack_state();
    require(returned, "drained, S1's bucket must rise and bring the cage back down to the yard");
    require(descent_water > 0.0 && descent_water < kS1CageMassKg - kS1BucketMassKg,
            "S1's empty cage must start down only once the bucket is lighter than it");
    require(s1_chain_at_rest(ride), "S1's chain must hang back in the cage at grade, in a rider's reach");
    std::cout << "PASS scraperx_sim S1 ride: ride_s=" << rode.seconds << " lift_water_kg=" << rode.lift_water_kg
              << " valve_shut_at_m=" << rode.shut_travel << " top_water_kg=" << rode.top_water_kg
              << " floor_y=" << floor_y << " peak_speed=" << top_state.s1_cage_peak_speed
              << " energy_margin_J=" << rode.worst_margin_j << " deck2_y=" << on_deck.player_position.y
              << " descent_water_kg=" << descent_water << " return_water_kg=" << back.s1_bucket_water_kg << "\n";

    // (c) The rider who stays aboard: let go at the top and stand still, the
    // bucket drains until it is lighter than the cage and rider, and the cage
    // brings them back down to the yard. There the chain hangs in reach again
    // and the machine carries them up a second time: nobody is stranded, and
    // the tank holds water for many rides.
    Simulation stay(InitialSpawn::ExteriorGrade);
    require(stay.advance_frame(0.5).accepted, "S1 stay-aboard settle interval must be accepted");
    require(take_s1_chain(stay) && ride_s1(stay, 20.0).reached_top, "S1 must carry the rider up");
    bool brought_down = false;
    bool aboard = true;
    double stay_descent_water = -1.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !brought_down; ++tick) {
        (void)stay.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = stay.stack_state();
        if (stay_descent_water < 0.0 && state.s1_cage_travel < kS1Travel - 0.10) {
            stay_descent_water = state.s1_bucket_water_kg;
        }
        if (state.s1_cage_travel > 0.05 && state.s1_cage_travel < kS1Travel - 0.05) {
            const auto snap = stay.snapshot();
            aboard = aboard && snap.player_grounded && snap.support_entity_id == Simulation::kStackS1CageEntityId;
        }
        brought_down = std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched;
    }
    (void)stay.advance_frame(2.0);
    require(brought_down && aboard, "a rider who stays aboard must be brought back down to the yard on S1's cage");
    require(stay_descent_water > 0.0 && stay_descent_water < kS1BalanceKg,
            "S1's cage must start down with its rider only once the bucket is lighter than both");
    require(s1_chain_at_rest(stay), "back at grade, S1's chain must hang in reach again");
    const double stay_tank = stay.stack_state().s1_tank_water_kg;
    require(take_s1_chain(stay) && ride_s1(stay, 20.0).reached_top, "S1 must carry the rider up a second time");
    const double per_ride = stay_tank - stay.stack_state().s1_tank_water_kg;
    require(per_ride > 0.0 && stay.stack_state().s1_tank_water_kg > 10.0 * per_ride,
            "S1's tank must hold water for many more rides");
    std::cout << "PASS scraperx_sim S1 stay aboard: descent_water_kg=" << stay_descent_water
              << " per_ride_kg=" << per_ride << " tank_kg=" << stay.stack_state().s1_tank_water_kg << "\n";

    // C1: from deck 2's south band, where S1 leaves its rider, up the facade
    // to deck 4 on player inputs.
    Simulation facade(InitialSpawn::Deck2South);
    require(facade.advance_frame(1.0).accepted, "C1 settle interval must be accepted");
    const double facade_start = facade.snapshot().simulation_time_seconds;
    C1Notes notes;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool climbed = climb_c1(facade, &notes);
    g_path_watch.armed = false;
    require(climbed, "C1 must carry a climber from deck 2 to deck 4");
    if (g_path_watch.worst > 0.15) {
        std::cout << "C1 jump: step=" << g_path_watch.worst << " at=" << g_path_watch.worst_at.x << ","
                  << g_path_watch.worst_at.y << "," << g_path_watch.worst_at.z
                  << " traversal=" << g_path_watch.worst_traversal << "\n";
    }
    require(g_path_watch.worst <= 0.15,
            "climbing C1, the body must never move more than 0.15 m sideways in one tick");
    require(!notes.ladder_in_reach_standing,
            "the davit's ladder must be out of reach from the monorail without the leap");
    const auto facade_top = facade.snapshot();
    std::cout << "PASS scraperx_sim C1 climb: seconds=" << facade_top.simulation_time_seconds - facade_start
              << " deck4_y=" << facade_top.player_position.y << " ladder_needs_leap=1"
              << " worst_tick_step_m=" << g_path_watch.worst << "\n";

    // The Stack so far in one run from the game's spawn, on player inputs:
    // hold S1's chain, ride it to deck 2, climb C1 to deck 4.
    Simulation band(InitialSpawn::ExteriorGrade);
    require(band.advance_frame(0.5).accepted, "the Stack's settle interval must be accepted");
    const double band_start = band.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(take_s1_chain(band), "the Stack: into S1's cage and take hold of its chain");
    require(ride_s1(band, 20.0).reached_top, "the Stack: S1 carries the rider to deck 2");
    require(exit_s1(band), "the Stack: vault, balance and mantle off S1 onto deck 2");
    const double at_deck2 = band.snapshot().simulation_time_seconds - band_start;
    require(climb_c1(band), "the Stack: up C1 to deck 4");
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack: from the yard to deck 4 the body must never move more than 0.15 m sideways in one tick");
    const auto band_top = band.snapshot();
    require(band_top.death_count == 0, "the Stack: from the yard to deck 4 without dying");
    std::cout << "PASS scraperx_sim Stack to deck 4: seconds=" << band_top.simulation_time_seconds - band_start
              << " at_deck2=" << at_deck2 << " deck4_y=" << band_top.player_position.y
              << " worst_tick_step_m=" << g_path_watch.worst << "\n";

    // S2 at rest: left alone for 10 s, the walking beam and cage wait as found.
    Simulation s2_idle(InitialSpawn::Deck4South);
    require(s2_idle.advance_frame(10.0).accepted, "S2 idle interval must be accepted");
    const auto s2_idle_state = s2_idle.stack_state();
    require(std::abs(s2_idle_state.s2_cage_travel) <= 0.03 && s2_idle_state.s2_chock_latched &&
                s2_handle_at_rest(s2_idle),
            "left alone, S2 must wait as found with chock latched");
    std::cout << "PASS scraperx_sim S2 at rest: cage_travel=" << s2_idle_state.s2_cage_travel
              << " chock_latched=" << s2_idle_state.s2_chock_latched << "\n";

    // S2 ride: board cage at deck 4, pull chock lanyard, ride walking beam to deck 6.
    Simulation s2_sim(InitialSpawn::Deck4South);
    require(s2_sim.advance_frame(0.5).accepted, "S2 settle interval must be accepted");
    require(take_s2_handle(s2_sim), "the player must walk from deck 4 into S2's cage and take hold of its lanyard");
    const auto s2_rode = ride_s2(s2_sim, 20.0);
    require(s2_rode.reached_top, "pulling S2's lanyard must carry the rider to deck 6");
    require(s2_rode.rode_on_cage, "the rider must stand on S2's cage for the whole ride");
    const auto s2_top_state = s2_sim.stack_state();
    require(s2_top_state.s2_cage_peak_speed <= 2.6, "S2's governor must hold the cage to 2.5 m/s");
    require(s2_rode.worst_margin_j >= 0.0, "S2 payload energy gain must not exceed source energy released");

    // Walk off onto deck 6's south band
    require(walk_to(s2_sim, 10.0, -133.0, 6.0) && walk_to(s2_sim, 10.0, -126.0, 6.0),
            "the rider must walk off S2's cage over the upper gangway onto deck 6");
    (void)s2_sim.advance_frame(0.5);
    const auto on_deck6 = s2_sim.snapshot();
    require(on_deck6.player_grounded && on_deck6.player_position.y > kDeck6Top + 0.5 &&
                on_deck6.player_position.z < -124.0 &&
                on_deck6.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 6's south band");
    const double s2_floor_y = kit_y(s2_sim, Simulation::kStackS2CageEntityId) + 0.10;
    require(std::abs(s2_floor_y - kS2FloorTopUp) <= 0.05, "S2 floor must stop at 66.05 m");
    std::cout << "PASS scraperx_sim S2 ride: ride_s=" << s2_rode.seconds
              << " floor_y=" << s2_floor_y
              << " peak_speed=" << s2_top_state.s2_cage_peak_speed
              << " deck6_y=" << on_deck6.player_position.y << "\n";

    // S2 checkpoint continuation: commit checkpoint on deck 4, fatal fall, restore, then take S2 handle and ride.
    Simulation s2_cp(InitialSpawn::Deck4South);
    require(s2_cp.advance_frame(0.5).accepted, "S2 CP settle interval must be accepted");
    require(walk_to(s2_cp, 10.0, -125.5, 4.0, 0.10), "walk to checkpoint location on deck 4");
    (void)s2_cp.advance_frame(0.5);
    const auto s2_cp_pos = s2_cp.snapshot().checkpoint_position;
    require(s2_cp_pos.y > 44.0, "Deck 4 checkpoint committed above 44m");
    (void)s2_cp.set_facing(0.0, 1.0);
    (void)s2_cp.set_move_input(0.0, 1.0);
    const auto initial_deaths = s2_cp.snapshot().death_count;
    for (int t = 0; t < 400 && s2_cp.snapshot().death_count == initial_deaths; ++t) {
        (void)s2_cp.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s2_cp.set_move_input(0.0, 0.0);
    require(s2_cp.snapshot().death_count > initial_deaths, "fall off deck 4 must be fatal");
    (void)s2_cp.advance_frame(0.5);
    require(s2_cp.snapshot().player_grounded, "player must be grounded after checkpoint restore");
    require(std::abs(s2_cp.snapshot().player_position.y - s2_cp_pos.y) <= 0.5, "player must restore to deck 4 altitude");
    require(take_s2_handle(s2_cp), "player must be able to take S2 handle after checkpoint restore");
    const auto s2_cp_rode = ride_s2(s2_cp, 20.0);
    require(s2_cp_rode.reached_top, "S2 must carry rider to deck 6 after checkpoint restore");
    std::cout << "PASS scraperx_sim S2 checkpoint continuation: deaths=" << s2_cp.snapshot().death_count << "\n";

    // Full Stack ascent in one run from grade: S1 (0->22m) -> C1 (22->44m) -> S2 (44->66m)
    require(take_s2_handle(band), "the Stack: from deck 4 into S2's cage and take hold of its lanyard");
    const auto band_s2_rode = ride_s2(band, 20.0);
    require(band_s2_rode.reached_top, "the Stack: S2 carries the rider to deck 6");
    require(walk_to(band, 10.0, -133.0, 6.0) && walk_to(band, 10.0, -126.0, 6.0),
            "the Stack: off S2 onto deck 6");
    (void)band.advance_frame(0.5);
    const auto band_deck6_top = band.snapshot();
    require(band_deck6_top.death_count == 0, "the Stack: from the yard to deck 6 without dying");
    require(band_deck6_top.player_grounded && band_deck6_top.player_position.y > kDeck6Top + 0.5,
            "the Stack: standing on deck 6");
    std::cout << "PASS scraperx_sim Stack to deck 6: seconds="
              << band_deck6_top.simulation_time_seconds - band_start
              << " deck6_y=" << band_deck6_top.player_position.y << "\n";

    // C2 climb: from Deck 6 South band up to Deck 8 North band
    Simulation c2_sim(InitialSpawn::Deck6South);
    require(c2_sim.advance_frame(1.0).accepted, "C2 settle interval must be accepted");
    const double c2_start = c2_sim.snapshot().simulation_time_seconds;
    C2Notes c2_notes;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool c2_climbed = climb_c2(c2_sim, &c2_notes);
    g_path_watch.armed = false;
    require(c2_climbed, "C2 must carry a climber from deck 6 to deck 8");
    require(g_path_watch.worst <= 0.15,
            "climbing C2, the body must never move more than 0.15 m sideways in one tick");
    require(!c2_notes.ladder_in_reach_standing,
            "the davit ladder on C2 must be out of reach standing on the monorail");
    const auto c2_top = c2_sim.snapshot();
    std::cout << "PASS scraperx_sim C2 climb: seconds=" << c2_top.simulation_time_seconds - c2_start
              << " deck8_y=" << c2_top.player_position.y << " ladder_needs_leap=1\n";

    // S3 at rest: left alone for 10 s, S3 waits as found
    Simulation s3_idle(InitialSpawn::Deck8North);
    require(s3_idle.advance_frame(10.0).accepted, "S3 idle interval must be accepted");
    const auto s3_idle_state = s3_idle.stack_state();
    std::cout << "S3 idle: travel=" << s3_idle_state.s3_cage_travel
              << " latched=" << s3_idle_state.s3_brake_latched
              << " handle=" << s3_handle_at_rest(s3_idle) << "\n";
    require(std::abs(s3_idle_state.s3_cage_travel) <= 0.08 && s3_idle_state.s3_brake_latched &&
                s3_handle_at_rest(s3_idle),
            "left alone, S3 must wait as found with brake latched");
    std::cout << "PASS scraperx_sim S3 at rest: cage_travel=" << s3_idle_state.s3_cage_travel
              << " brake_latched=" << s3_idle_state.s3_brake_latched << "\n";

    // S3 ride: board cage at deck 8, pull brake handle, ride 44 m to deck 12
    Simulation s3_sim(InitialSpawn::Deck8North);
    require(s3_sim.advance_frame(0.5).accepted, "S3 settle interval must be accepted");
    require(take_s3_handle(s3_sim), "the player must walk from deck 8 into S3's cage and take hold of its trip handle");
    const auto s3_rode = ride_s3(s3_sim, 25.0);
    require(s3_rode.reached_top, "pulling S3's handle must carry the rider to deck 12");
    require(s3_rode.rode_on_cage, "the rider must stand on S3's cage for the whole ride");
    const auto s3_top_state = s3_sim.stack_state();
    require(s3_top_state.s3_cage_peak_speed <= 2.6, "S3's governor must hold the cage to 2.5 m/s");
    require(s3_rode.worst_margin_j >= 0.0, "S3 payload energy gain must not exceed source energy released");

    // Walk off onto deck 12 North band
    require(walk_to(s3_sim, -8.0, -164.0, 6.0) && walk_to(s3_sim, -8.0, -171.2, 6.0),
            "the rider must walk off S3's cage over the upper gangway onto deck 12");
    (void)s3_sim.advance_frame(0.5);
    const auto on_deck12 = s3_sim.snapshot();
    require(on_deck12.player_grounded && on_deck12.player_position.y > kDeck12Top + 0.5 &&
                on_deck12.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 12's north band");
    const double s3_floor_y = kit_y(s3_sim, Simulation::kStackS3CageEntityId) + 0.10;
    require(std::abs(s3_floor_y - kS3FloorTopUp) <= 0.05, "S3 floor must stop at 132.05 m");
    std::cout << "PASS scraperx_sim S3 ride: ride_s=" << s3_rode.seconds
              << " floor_y=" << s3_floor_y
              << " peak_speed=" << s3_top_state.s3_cage_peak_speed
              << " deck12_y=" << on_deck12.player_position.y << "\n";

    // C3 climb: from Deck 12 to Deck 14 (154 m)
    Simulation c3_sim(InitialSpawn::Deck12North);
    require(c3_sim.advance_frame(1.0).accepted, "C3 settle interval must be accepted");
    const double c3_start = c3_sim.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool c3_climbed = climb_c3(c3_sim);
    g_path_watch.armed = false;
    require(c3_climbed, "C3 must carry a climber from deck 12 to deck 14");
    require(g_path_watch.worst <= 0.15,
            "climbing C3, the body must never move more than 0.15 m sideways in one tick");
    const auto c3_top = c3_sim.snapshot();
    std::cout << "PASS scraperx_sim C3 climb: seconds=" << c3_top.simulation_time_seconds - c3_start
              << " deck14_y=" << c3_top.player_position.y << "\n";

    // Complete continuous Stack ascent in one unbroken run from Grade (0.0 m) to Deck 14 (+154.0 m)
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(climb_c2(band), "the Stack: up C2 to deck 8");
    require(take_s3_handle(band), "the Stack: from deck 8 into S3's cage and take hold of its trip handle");
    const auto band_s3_rode = ride_s3(band, 25.0);
    require(band_s3_rode.reached_top, "the Stack: S3 carries the rider to deck 12");
    require(walk_to(band, -8.0, -164.0, 6.0) && walk_to(band, -8.0, -171.2, 6.0),
            "the Stack: off S3 onto deck 12 landing");
    require(climb_c3(band), "the Stack: up C3 to deck 14 (Deck154)");
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack: from yard to deck 14 the body must never move more than 0.15 m sideways in one tick");
    const auto band_deck14_top = band.snapshot();
    require(band_deck14_top.death_count == 0, "the Stack: from yard to deck 14 without dying");
    require(band_deck14_top.player_grounded && band_deck14_top.player_position.y > kDeck14Top + 0.5,
            "the Stack: standing on deck 14");
    std::cout << "PASS scraperx_sim Stack to deck 14: seconds="
              << band_deck14_top.simulation_time_seconds - band_start
              << " deck14_y=" << band_deck14_top.player_position.y
              << " pos=(" << band_deck14_top.player_position.x << ", "
              << band_deck14_top.player_position.y << ", "
              << band_deck14_top.player_position.z << ")\n";

    // ---- Mega-Ascent Continuation: Stack -> Counterweight Well (0 to 220 m) ----
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(board_well_a(band), "the Stack to A: board Stage A cage from Deck 14");
    require(rig_well_a(band), "the Stack to A: rig Stage A shackle to cage eye");
    require(pull_well_a(band, 0.5, 3.0), "the Stack to A: pull Stage A trip handle");
    require(wait_for(band, 16.0, [](const Snapshot &state) {
        return state.well_a_cage_travel >= kWellATravel - 0.01;
    }), "the Stack to A: Stage A carries rider to Ring 176 (176.25 m)");
    (void)band.advance_frame(1.0);
    const auto band_ring176 = band.snapshot();
    require(band_ring176.death_count == 0, "the Stack to Ring 176 without dying");
    std::cout << "PASS scraperx_sim Stack to Ring 176: seconds="
              << band_ring176.simulation_time_seconds - band_start
              << " ring176_y=" << band_ring176.player_position.y << "\n";

    // Step across into B's cage
    require(walk_to(band, -9.4, -131.2, 4.0) && walk_to(band, -7.6, -131.2, 4.0),
            "from A's parked cage the rider must step across into B's");
    require(rig_well_b(band) && pull_well_b(band), "the rider must rig and trip Stage B");
    require(wait_for(band, 16.0, [](const Snapshot &state) {
        return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
    }), "B must carry the rider to the 198 ring");
    (void)band.advance_frame(3.0);
    require(band.snapshot().player_grounded &&
            band.snapshot().support_entity_id == Simulation::kWellBCageEntityId,
            "the rider must stand in B's parked cage");

    // Step across onto C's platform
    require(walk_to(band, -6.2, -131.9, 4.0) && walk_to(band, -4.4, -131.9, 4.0),
            "from B's parked cage the rider must step across onto C's platform");
    require(clear_well_c_chute(band) && fill_well_c(band, 10.0) && pull_well_c_latch(band, 3.0),
            "the rider must clear C's chute, let the dumpster fill and pull the latch");
    require(wait_for(band, 16.0, [](const Snapshot &state) {
        return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
    }), "C must carry the rider to the 220 ring");
    (void)band.advance_frame(2.0);

    // Step off onto Ring 220
    require(walk_to(band, -3.0, -129.0, 4.0), "from C's platform the rider must step onto the 220 ring");
    (void)band.advance_frame(1.0);
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack to 220m: body must never move more than 0.15 m sideways in one tick");
    const auto band_ring220 = band.snapshot();
    require(band_ring220.death_count == 0, "Grade to Ring 220 without dying");
    require(band_ring220.player_grounded && band_ring220.player_position.y > 221.0 &&
            band_ring220.player_position.y < 221.3 && band_ring220.player_position.z > -130.73 &&
            band_ring220.support_entity_id != Simulation::kWellCPlatformEntityId,
            "continuous ascent from Grade must end standing on Ring 220");
    std::cout << "PASS scraperx_sim Stack to Ring 220: seconds="
              << band_ring220.simulation_time_seconds - band_start
              << " ring220_y=" << band_ring220.player_position.y << "\n";

    // Verify cascade re-arms Stage A:
    require(wait_for(band, 25.0, [](const Snapshot &state) {
        return state.well_a_catch_latched && state.well_a_cage_travel <= 0.01;
    }), "C's spent dumpster empties into A's cage, re-arming A");
    const auto rearmed = band.snapshot();
    require(rearmed.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0 &&
            rearmed.well_c_dumpster_kg <= 1.0 && rearmed.well_a_skip_travel >= -0.01,
            "re-armed, A's skip must be up in its catch, rubble in its cage");
    std::cout << "PASS scraperx_sim Mega-Ascent Grade to 220m: seconds="
              << band_ring220.simulation_time_seconds - band_start
              << " deck14_y=" << band_deck14_top.player_position.y
              << " ring176_y=" << band_ring176.player_position.y
              << " ring220_y=" << band_ring220.player_position.y
              << " rearmed=1\n";
    require(climb_above_ring220(band, band_start, 0, "grade to the floor above the ladder"),
            "the same climb from the ground must continue past the 220 m ring");
}


