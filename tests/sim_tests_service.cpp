// AS-010 C4, stage N and C5: the service gantry, the gravel hoist, the cooling plant.

#include "sim_tests.hpp"

// ---- AS-010 C4, the service gantry (662 -> 684 m) --------------------------------

// Where a leg of C4 left the body, for a failure's message.
bool report_c4(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "C4 " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " traversal=" << static_cast<int>(state.traversal_state)
              << " grounded=" << state.player_grounded << " support=" << state.support_entity_id
              << " ledge=" << state.ledge_available << " grip=" << state.grip_available
              << " edge_drop=" << state.edge_drop_available << '\n';
    return false;
}

// A jump from where the body stands to a lip 3.2 m up ahead, caught, and the
// pull up onto it.
bool c4_hang_up(scraperx::sim::Simulation &simulation, const double fx, const double fz, const double top,
                const char *leg) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.4 * fx, 0.4 * fz, fx, fz, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        return report_c4(simulation, leg);
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    return wait_for(simulation, 2.5, [top](const Snapshot &state) { return standing_above(state, top); }) ||
           report_c4(simulation, leg);
}

// A mantle onto the ledge ahead.
bool c4_mantle(scraperx::sim::Simulation &simulation, const double fx, const double fz, const double top,
               const char *leg) {
    using scraperx::sim::Snapshot;
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        return report_c4(simulation, leg);
    }
    (void)simulation.request_traversal();
    return wait_for(simulation, 2.0, [top](const Snapshot &state) { return standing_above(state, top); }) ||
           report_c4(simulation, leg);
}

// From the 662 deck by C4's cabinet onto its duct, on player inputs: the
// cabinet, then the duct from the cabinet's back edge.
bool c4_to_duct(scraperx::sim::Simulation &simulation) {
    // The cabinet, from the deck.
    if (!walk_to(simulation, 6.2, -149.1, 20.0, 0.08)) {
        return report_c4(simulation, "to the cabinet");
    }
    if (!c4_mantle(simulation, 0.0, -1.0, 664.3, "onto the cabinet")) {
        return false;
    }
    // The duct, from the cabinet's back edge.
    (void)walk_to(simulation, 6.2, -150.9, 2.0, 0.05);
    return c4_hang_up(simulation, 0.0, -1.0, 667.5, "up onto the duct");
}

// East along C4's duct and over the beam to the pump deck.
bool c4_duct_to_pump_deck(scraperx::sim::Simulation &simulation, C4Notes &seen) {
    if (!(walk_to(simulation, 10.0, -152.3, 6.0, 0.1) && walk_to(simulation, 10.0, -153.9, 3.0, 0.1))) {
        return report_c4(simulation, "onto the beam");
    }
    seen.balanced = simulation.snapshot().player_balancing;
    if (!walk_to(simulation, 10.0, -158.0, 6.0, 0.1) || !standing_above(simulation.snapshot(), 667.5)) {
        return report_c4(simulation, "over the beam");
    }
    return true;
}

// Up C4's standpipe from the pump deck onto the runway.
bool c4_pump_deck_to_runway(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Snapshot;
    if (!walk_to(simulation, 9.5, -158.7, 4.0, 0.08)) {
        return report_c4(simulation, "to the standpipe");
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return report_c4(simulation, "standpipe offered");
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 675.3); })) {
        return report_c4(simulation, "up the standpipe");
    }
    return true;
}

// From the 662 deck by C4's cabinet up onto its hoist runway, on player
// inputs: the cabinet, the duct, the beam, the standpipe.
bool climb_c4_to_runway(scraperx::sim::Simulation &simulation, C4Notes &seen) {
    return c4_to_duct(simulation) && c4_duct_to_pump_deck(simulation, seen) && c4_pump_deck_to_runway(simulation);
}

// On one of C4's platforms, whose top is `top`, from (x, z), the stick held
// toward the tower's face for 2 s: whether the body stays up on it.
bool c4_stays_up(scraperx::sim::Simulation &simulation, const double x, const double z, const double stick_x,
                 const double stick_z, const double top) {
    using scraperx::sim::Snapshot;
    if (!walk_to(simulation, x, z, 4.0, 0.1)) {
        return report_c4(simulation, "to a parapet");
    }
    bool off = false;
    (void)hold_stick(simulation, stick_x, stick_z, stick_x, stick_z, 2.0, [&off, top](const Snapshot &state) {
        off = off || state.player_position.y < top + 0.5;
        return off;
    });
    return (!off && standing_above(simulation.snapshot(), top + 0.5)) || report_c4(simulation, "at a parapet");
}

// Walked into the parapets where C4's duct, pump deck and runway come within
// 1 m of the tower's faces: whether the body stays up on each platform.
bool c4_parapets_hold() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kDuctTop = 666.85;     // the duct's and the pump deck's
    constexpr double kRunwayTop = 674.6;
    Simulation walker(InitialSpawn::Deck662);
    (void)walker.advance_frame(1.0);
    C4Notes seen;
    return c4_to_duct(walker) && c4_stays_up(walker, 10.0, -152.3, 1.0, 0.0, kDuctTop) &&
           c4_duct_to_pump_deck(walker, seen) && walk_to(walker, 8.8, -158.3, 3.0, 0.1) &&
           c4_stays_up(walker, 8.8, -160.3, 0.0, -1.0, kDuctTop) && walk_to(walker, 8.8, -158.3, 3.0, 0.1) &&
           c4_stays_up(walker, 10.5, -158.3, 1.0, 0.0, kDuctTop) && c4_pump_deck_to_runway(walker) &&
           c4_stays_up(walker, 9.5, -160.6, 0.0, -1.0, kRunwayTop) &&
           c4_stays_up(walker, 10.0, -160.6, 1.0, 0.0, kRunwayTop);
}

// From the 662 deck by C4's cabinet up the service gantry to the 684 deck, on
// player inputs, the way a player who has worked it out climbs it.
bool climb_c4(scraperx::sim::Simulation &simulation, C4Notes *notes) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    C4Notes seen;
    if (!climb_c4_to_runway(simulation, seen)) {
        return false;
    }
    // The winch house: nothing to climb on it, from the runway.
    if (!walk_to(simulation, 7.6, -160.8, 4.0, 0.08)) {
        return report_c4(simulation, "to the winch house");
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    seen.house_offered = simulation.snapshot().ledge_available || simulation.snapshot().grip_available;
    // Jumped at, pushing in, it is out of any jump's reach and catches nothing.
    (void)simulation.request_jump();
    seen.house_caught = hold_stick(simulation, -0.4, 0.0, -1.0, 0.0, 1.5, [](const Snapshot &state) {
        return state.traversal_state == TraversalState::Hanging || state.traversal_state == TraversalState::Climbing;
    });
    if (seen.house_caught || !wait_for(simulation, 1.0, [](const Snapshot &state) { return standing_above(state, 675.3); })) {
        return report_c4(simulation, "back on the runway after the jump at the house");
    }
    // Over the runway's +z edge into a hang, west along its lip past the
    // house, and up.
    if (!walk_to(simulation, 7.8, -159.9, 4.0, 0.08)) {
        return report_c4(simulation, "to the runway's edge");
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().edge_drop_available) {
        return report_c4(simulation, "edge drop offered");
    }
    (void)simulation.request_release();
    if (!wait_for(simulation, 1.5,
                  [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        return report_c4(simulation, "lowered into a hang");
    }
    // Hanging all the way: a body that loses the lip fails here, not later.
    const double hung_x = simulation.snapshot().player_position.x;
    bool let_go = false;
    (void)hold_stick(simulation, -1.0, 0.0, 0.0, -1.0, 9.0, [&let_go](const Snapshot &state) {
        let_go = state.traversal_state != TraversalState::Hanging;
        return let_go || state.player_position.x <= 4.4;
    });
    if (let_go || simulation.snapshot().player_position.x > 4.4) {
        return report_c4(simulation, "hanging along the lip past the house");
    }
    seen.shimmied = hung_x - simulation.snapshot().player_position.x;
    (void)simulation.advance_frame(0.2);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 675.3); })) {
        return report_c4(simulation, "up west of the house");
    }
    // Across the gap: a run along +z off the runway's west end.
    if (!walk_to(simulation, 2.5, -161.0, 6.0, 0.1)) {
        return report_c4(simulation, "to the run-up");
    }
    if (!hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 2.0,
                    [](const Snapshot &state) { return state.player_position.z >= -159.9; })) {
        return report_c4(simulation, "the run-up");
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 2.0, [](const Snapshot &state) {
            return state.player_grounded && state.player_position.y > 675.0 && state.player_position.z > -156.4;
        })) {
        return report_c4(simulation, "across the gap");
    }
    // The gallery, from the landing's +z edge.
    if (!walk_to(simulation, 2.5, -153.25, 4.0, 0.05)) {
        return report_c4(simulation, "to the landing's edge");
    }
    if (!c4_hang_up(simulation, 0.0, 1.0, 678.5, "up onto the gallery")) {
        return false;
    }
    // The riser, from the gallery's west end.
    if (!walk_to(simulation, 0.9, -151.7, 6.0, 0.08)) {
        return report_c4(simulation, "to the riser");
    }
    if (!c4_mantle(simulation, -1.0, 0.0, 680.0, "onto the riser")) {
        return false;
    }
    // The hoist platform, from the riser's -z edge.
    (void)walk_to(simulation, -0.8, -152.2, 3.0, 0.05);
    if (!c4_hang_up(simulation, 0.0, -1.0, 683.2, "up onto the hoist platform")) {
        return false;
    }
    // The 684 deck, from the hoist platform's west end.
    if (!walk_to(simulation, -1.6, -153.7, 4.0, 0.08)) {
        return report_c4(simulation, "to the 684 deck's edge");
    }
    if (!c4_mantle(simulation, -1.0, 0.0, 684.8, "onto the 684 deck")) {
        return false;
    }
    if (!walk_to(simulation, -4.0, -153.7, 4.0, 0.1)) {
        return report_c4(simulation, "onto the 684 deck");
    }
    (void)simulation.advance_frame(0.5);
    if (notes != nullptr) {
        *notes = seen;
    }
    const auto top = simulation.snapshot();
    return (on_support(top, Simulation::kServiceFrameEntityId) && top.player_position.y > 684.8) ||
           report_c4(simulation, "standing on the 684 deck");
}

// On foot from the runway's lip east of C4's winch house, the stick held
// west (and a little into the lip, or into the house): whether the body
// ever stands on the runway west of the house.
bool c4_walks_past_house(const double stick_x, const double stick_z) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    Simulation walker(InitialSpawn::Deck662);
    (void)walker.advance_frame(1.0);
    C4Notes seen;
    require(climb_c4_to_runway(walker, seen) && walk_to(walker, 7.6, -159.6, 4.0, 0.05),
            "C4: up onto the runway, by the winch house's lip");
    bool past = false;
    (void)hold_stick(walker, stick_x, stick_z, -1.0, 0.0, 3.0, [&past](const Snapshot &state) {
        past = past || (state.player_grounded && state.player_position.y > 675.0 && state.player_position.x < 5.0);
        return past;
    });
    return past;
}

void run_service_c4() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    for (const std::pair<double, double> &stick : {std::pair{-1.0, 0.0}, std::pair{-1.0, 0.1}, std::pair{-1.0, -0.2}}) {
        require(!c4_walks_past_house(stick.first, stick.second),
                "on foot along the runway, nothing gets past C4's winch house");
    }
    require(c4_parapets_hold(), "walked into C4's parapets by the tower's faces, a body must stay up on its platform");
    Simulation c4(InitialSpawn::Deck662);
    require(c4.advance_frame(1.0).accepted, "C4 settle interval must be accepted");
    const double start = c4.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    C4Notes notes;
    const bool climbed = climb_c4(c4, &notes);
    g_path_watch.armed = false;
    require(climbed, "C4 must carry a climber from the 662 deck to the 684 deck");
    require(notes.balanced, "C4's beam must hold its walker on its line");
    require(!notes.house_offered, "C4's winch house must offer nothing to climb: the way past it is the hang");
    require(!notes.house_caught, "jumped at from the runway, C4's winch house must catch nothing");
    require(notes.shimmied >= 3.0, "C4's hang must carry the body along the runway's lip past the winch house");
    require(g_path_watch.worst <= 0.15, "climbing C4, the body must never move more than 0.15 m sideways in one tick");
    const auto top = c4.snapshot();
    require(top.death_count == 0, "C4 must be climbed without a death");
    std::cout << "PASS scraperx_sim AS-010 C4: seconds=" << top.simulation_time_seconds - start
              << " deck684_y=" << top.player_position.y << " shimmied_m=" << notes.shimmied
              << " worst_step_m=" << g_path_watch.worst << '\n';
}

// ---- AS-010 stage N, the granular discharge hoist (684 -> 706 m) -------------

constexpr double kNCageKg = 2000.0;
constexpr double kNGravelKg = 1213.0;
constexpr double kNChainFullKg = 33.0 * 23.0;
constexpr double kNDeck = 684.3;            // the cage's deck at rest
constexpr double kN706Top = 706.1;
constexpr double kNFlapOpen = 0.3;          // the flap past its dead point: the bin pours
// Where N's flap chain hangs, in the cage.
constexpr double kNChainX = -7.1;
constexpr double kNChainZ = -155.35;

// From the 684 deck into N's cage beside its chain: face it and take hold.
// Hanging over the hands, it comes down to them, which draws the flap's arm
// toward its dead point. True once the chain is in the hands.
bool take_n_chain(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, -5.6, -155.35, 20.0) && walk_to(simulation, kNChainX + 0.5, kNChainZ, 6.0, 0.06))) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kServiceNHandleEntityId || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kServiceNHandleEntityId;
}

// Holding N's chain until the flap lies open past its dead point, then let
// go: true once it is open.
bool open_n_flap(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!take_n_chain(simulation)) {
        return false;
    }
    bool open = false;
    observe_path(simulation, false);
    for (int tick = 0; tick < 4 * 90 && !open; ++tick) {
        (void)simulation.set_facing(-1.0, 0.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(simulation, true);
        open = simulation.service_state().n_gate_angle >= kNFlapOpen;
    }
    (void)simulation.request_set_down();
    return open;
}

NRide ride_service_n(scraperx::sim::Simulation &simulation, const bool rider_aboard) {
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;
    NRide ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cage_y0 = kit_com_y(simulation, Simulation::kServiceNCageEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    double hopper_y = kit_com_y(simulation, Simulation::kServiceNHopperEntityId);
    double last = simulation.service_state().n_cage_travel;
    int still = 0;
    observe_path(simulation, false);
    for (int tick = 0; tick < 45 * 90; ++tick) {
        const double hopper_kg = simulation.service_state().n_hopper_mass;
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(simulation, true);
        const double now_y = kit_com_y(simulation, Simulation::kServiceNHopperEntityId);
        ride.released += hopper_kg * kG * (hopper_y - now_y);
        hopper_y = now_y;
        const double travel = simulation.service_state().n_cage_travel;
        const double speed = std::abs(travel - last) / Simulation::kFixedStepSeconds;
        last = travel;
        ride.apex = std::max(ride.apex, travel);
        ride.peak_speed = std::max(ride.peak_speed, speed);
        if (rider_aboard && travel > 0.05 && speed > 0.05) {
            ride.rode_on_cage = ride.rode_on_cage && on_support(simulation.snapshot(), Simulation::kServiceNCageEntityId);
        }
        still = travel > 1.0 && speed < 0.005 ? still + 1 : 0;
        if (still >= static_cast<int>(Simulation::kTickRateHz)) {
            ride.settled = true;
            break;
        }
    }
    const auto end = simulation.snapshot();
    ride.seconds = end.simulation_time_seconds - start;
    ride.gain = kNCageKg * kG * (kit_com_y(simulation, Simulation::kServiceNCageEntityId) - cage_y0);
    if (rider_aboard) {
        ride.gain += kRiderMassKg * kG * (end.player_position.y - rider_y0);
    }
    return ride;
}

// Off N's cage at the top, east onto the 706 deck.
bool step_off_n(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -3.8, -155.0, 6.0)) {
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto off = simulation.snapshot();
    return on_support(off, Simulation::kServiceFrameEntityId) && off.player_position.y > kN706Top + 0.5;
}

void run_service_n() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    // ---- As found, nothing moves; a tug short of the dead point lets the flap
    // fall shut again and no gravel moves ------------------------------------------
    Simulation found(InitialSpawn::Deck684);
    (void)found.advance_frame(1.0);
    const auto as_found = found.service_state();
    require(std::abs(as_found.n_gate_angle) < 0.01 && std::abs(as_found.n_silo_kg - kNGravelKg) < 0.5 &&
                as_found.n_hopper_kg < 0.5 && std::abs(as_found.n_chain_kg - kNChainFullKg) < 0.5 &&
                as_found.n_cage_travel < 0.01 && as_found.n_hopper_travel < 0.01,
            "as found, N's flap is shut on a full bin, its hopper hangs empty at the top on its whole chain, the cage down");
    (void)found.advance_frame(10.0);
    const auto untouched = found.service_state();
    require(untouched.n_cage_travel < 0.01 && untouched.n_hopper_travel < 0.01 &&
                std::abs(untouched.n_silo_kg - kNGravelKg) < 0.5,
            "untouched, N stays as found: the empty hopper is lighter than the cage");
    // Beside the cage on the deck, east of it and then west, round the chain,
    // facing it: out of reach.
    Simulation reach(InitialSpawn::Deck684);
    (void)reach.advance_frame(1.0);
    require(walk_to(reach, -4.8, kNChainZ, 12.0, 0.06), "to beside N's cage on the 684 deck, east");
    (void)reach.set_facing(-1.0, 0.0);
    (void)reach.advance_frame(0.5);
    const bool east_reach = reach.snapshot().carry_target_entity_id == Simulation::kServiceNHandleEntityId;
    require(walk_to(reach, -4.8, -154.3, 6.0, 0.06) && walk_to(reach, -8.3, -154.3, 12.0, 0.06) &&
                walk_to(reach, -8.3, kNChainZ, 6.0, 0.06),
            "across N's cage clear of its chain, to beside it on the 684 deck, west");
    (void)reach.set_facing(1.0, 0.0);
    (void)reach.advance_frame(0.5);
    const bool west_reach = reach.snapshot().carry_target_entity_id == Simulation::kServiceNHandleEntityId;
    require(!east_reach && !west_reach, "N's flap chain must be in reach only from inside the cage");
    require(take_n_chain(found), "the rider must take hold of N's chain from inside the cage");
    (void)found.advance_frame(0.3);
    (void)found.request_set_down();
    (void)found.advance_frame(10.0);
    const auto tugged = found.service_state();
    require(tugged.n_gate_angle < 0.02 && std::abs(tugged.n_silo_kg - kNGravelKg) < 0.5 && tugged.n_cage_travel < 0.01,
            "a tug on N's chain short of its dead point lets the flap fall shut again, and no gravel moves");

    // ---- The ride ---------------------------------------------------------------
    Simulation n(InitialSpawn::Deck684);
    require(n.advance_frame(1.0).accepted, "N settle interval must be accepted");
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(open_n_flap(n), "the rider's pull must draw N's flap past its dead point and it must fall open");
    const NRide ride = ride_service_n(n, true);
    g_path_watch.armed = false;
    const auto top = n.snapshot();
    const auto held = n.service_state();
    require(held.n_silo_kg < 0.5 && std::abs(held.n_hopper_kg - kNGravelKg) < 0.5,
            "N's bin must pour its whole charge into the hopper");
    require(ride.settled, "N's cage must come to rest on its dogs");
    require(held.n_cage_travel >= kN706Top - 0.35 - kNDeck && held.n_cage_travel <= kN706Top + 0.65 - kNDeck &&
                ride.apex < 22.9,
            "the gravel must lift N's cage to the 706 deck, a step from it either way, clear of its stop");
    require(ride.peak_speed <= 3.5, "N's cage must never pass 3.5 m/s: the chain sets its drive down as it goes");
    require(ride.rode_on_cage && on_support(top, Simulation::kServiceNCageEntityId),
            "the rider must ride N's cage all the way");
    require(ride.gain > 0.0 && ride.gain <= ride.released,
            "N's cage and rider must never gain more than the hopper, its gravel and its chain released");
    require(g_path_watch.worst <= 0.15, "riding N, the body must never move more than 0.15 m in one tick");
    (void)n.advance_frame(10.0);
    require(std::abs(n.service_state().n_cage_travel - held.n_cage_travel) < 0.01,
            "with its rider aboard, N's cage must stay on its dogs");
    require(step_off_n(n), "the rider must step off N's cage onto the 706 deck");
    (void)n.advance_frame(10.0);
    require(std::abs(n.service_state().n_cage_travel - held.n_cage_travel) < 0.01,
            "empty, N's cage must stay on its dogs: the hopper, its chain set down, is lighter than the cage");
    require(n.snapshot().death_count == 0, "N must be ridden without a death");

    // ---- The flap opened, the rider steps out: the cage goes up alone ------------
    Simulation empty(InitialSpawn::Deck684);
    (void)empty.advance_frame(1.0);
    require(open_n_flap(empty) && walk_to(empty, -3.2, -155.0, 3.0) && empty.service_state().n_cage_travel < 0.01,
            "the rider must be able to open N's flap and step out onto the 684 deck before the cage moves");
    const NRide alone = ride_service_n(empty, false);
    const auto alone_held = empty.service_state();
    // The top of the guide is a dog's tooth: an empty cage that reaches its
    // head is held there.
    require(alone.settled && alone_held.n_cage_travel >= 21.1 && alone_held.n_cage_travel <= 22.91 &&
                empty.snapshot().player_position.y < 685.5 && empty.snapshot().death_count == 0,
            "sent up empty, N's cage must run to its head and come to rest on its dogs, the rider left on the 684 deck");

    std::cout << "PASS scraperx_sim AS-010 N: apex_m=" << ride.apex << " held_m=" << held.n_cage_travel
              << " peak_mps=" << ride.peak_speed << " seconds=" << ride.seconds
              << " rider_y=" << top.player_position.y << " gain_J=" << ride.gain << " released_J=" << ride.released
              << " empty_apex_m=" << alone.apex << " empty_peak_mps=" << alone.peak_speed
              << " worst_step_m=" << g_path_watch.worst << '\n';
}

// ---- AS-010 C5, the cooling plant (706 -> 728 m) --------------------------------

constexpr double kC5TankTop = 707.7;
constexpr double kC5DuctTop = 708.6;
constexpr double kC5FirstTop = 717.2;       // the first platform hung from, over the void
constexpr double kC5SecondTop = 720.4;      // the second, the standpipe's foot

// Where a leg of C5 left the body, for a failure's message.
bool report_c5(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "C5 " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " traversal=" << static_cast<int>(state.traversal_state)
              << " grounded=" << state.player_grounded << " crouched=" << state.player_crouched
              << " support=" << state.support_entity_id << " ledge=" << state.ledge_available
              << " grip=" << state.grip_available << '\n';
    return false;
}

// Up C4's standpipe-style hold at (x, z) from where the body stands, facing
// (fx, fz), until it stands on top above `top`.
bool c5_pipe(scraperx::sim::Simulation &simulation, const double x, const double z, const double fx,
             const double fz, const double top, const char *leg) {
    using scraperx::sim::Snapshot;
    if (!walk_to(simulation, x, z, 6.0, 0.06)) {
        return report_c5(simulation, leg);
    }
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return report_c5(simulation, leg);
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    return (is_climbing(simulation.snapshot()) &&
            hold_stick(simulation, fx, fz, fx, fz, 20.0, [top](const Snapshot &state) { return standing_above(state, top); })) ||
           report_c5(simulation, leg);
}

// From the 706 deck over the manifold, under the duct bank, onto the tank, up
// its standpipe and over the gap onto the landing, on player inputs.
bool c5_to_landing(scraperx::sim::Simulation &simulation, C5Notes &seen) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    // 1. Onto the plant floor and over the manifold in a vault.
    if (!(walk_to(simulation, -3.3, -146.0, 12.0) && walk_to(simulation, -0.85, -145.5, 12.0, 0.06))) {
        return report_c5(simulation, "to the manifold");
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_traversal();
    bool vaulting = false;
    if (!hold_stick(simulation, 0.0, 0.0, 1.0, 0.0, 2.0, [&vaulting](const Snapshot &state) {
            vaulting = vaulting || state.traversal_state == TraversalState::Vaulting;
            return state.traversal_state == TraversalState::None && state.player_grounded &&
                   state.player_position.x > 0.9;
        })) {
        return report_c5(simulation, "over the manifold");
    }
    seen.vaulted = vaulting;
    // 2. Under the duct bank, crouched.
    (void)simulation.set_crouch_input(true);
    bool crawled = true;
    if (!walk_to(simulation, 5.6, -145.5, 8.0, 0.08)) {
        (void)simulation.set_crouch_input(false);
        return report_c5(simulation, "under the ducts");
    }
    (void)simulation.set_crouch_input(false);
    (void)simulation.advance_frame(0.3);
    crawled = crawled && !simulation.snapshot().player_crouched;
    seen.crawled = crawled;
    // 3. Onto the tank.
    if (!(walk_to(simulation, 8.5, -146.5, 4.0, 0.06) && c4_mantle(simulation, 1.0, 0.0, 708.2, "C5 onto the tank"))) {
        return false;
    }
    // 4. Up its standpipe onto the platform over it.
    if (!c5_pipe(simulation, 10.25, -144.8, 0.0, 1.0, 714.5, "up the tank's standpipe")) {
        return false;
    }
    // 5. West along the platform at a sprint and over the gap.
    if (!walk_to(simulation, 10.85, -142.6, 4.0, 0.08)) {
        return report_c5(simulation, "to the run-up");
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.2);
    (void)simulation.set_sprint_input(true);
    if (!hold_stick(simulation, -1.0, 0.0, -1.0, 0.0, 2.0,
                    [](const Snapshot &state) { return state.player_position.x <= 8.45; })) {
        (void)simulation.set_sprint_input(false);
        return report_c5(simulation, "the run-up");
    }
    seen.gap_speed = std::abs(simulation.snapshot().player_linear_velocity.x);
    (void)simulation.request_jump();
    const bool across = hold_stick(simulation, -1.0, 0.0, -1.0, 0.0, 3.0, [](const Snapshot &state) {
        return state.player_grounded && state.player_position.y > 714.5 && state.player_position.x < 1.5;
    });
    (void)simulation.set_sprint_input(false);
    (void)simulation.advance_frame(0.5);
    if (!across) {
        return report_c5(simulation, "across the gap");
    }
    return true;
}

// From the 706 deck up the cooling plant to the 728 deck, on player inputs,
// the way a player who has worked it out climbs it.
bool climb_c5(scraperx::sim::Simulation &simulation, C5Notes *notes) {
    using scraperx::sim::Simulation;
    C5Notes seen;
    if (!c5_to_landing(simulation, seen)) {
        return false;
    }
    // 6. Up onto the platform north of the landing, and the one east of that.
    if (!(walk_to(simulation, -0.5, -141.6, 6.0, 0.05) && c4_hang_up(simulation, 0.0, 1.0, 717.7, "C5 the first hang"))) {
        return false;
    }
    if (!(walk_to(simulation, 2.55, -139.6, 6.0, 0.05) && c4_hang_up(simulation, 1.0, 0.0, 720.9, "C5 the second hang"))) {
        return false;
    }
    // 7. Up the second standpipe onto the top platform.
    if (!c5_pipe(simulation, 6.35, -139.63, 1.0, 0.0, 727.0, "up the second standpipe")) {
        return false;
    }
    // 8. Over the 728 deck's north girder.
    if (!(walk_to(simulation, 9.0, -141.75, 6.0, 0.05) && c4_mantle(simulation, 0.0, -1.0, 728.6, "C5 onto the 728 deck"))) {
        return false;
    }
    (void)simulation.advance_frame(0.5);
    if (notes != nullptr) {
        *notes = seen;
    }
    const auto top = simulation.snapshot();
    return (on_support(top, Simulation::kServiceFrameEntityId) && top.player_position.y > 728.8) ||
           report_c5(simulation, "standing on the 728 deck");
}

// From the 706 deck over the manifold onto the plant floor: the start of
// every C5 check.
bool c5_over_manifold(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    if (!(walk_to(simulation, -3.3, -146.0, 12.0) && walk_to(simulation, -0.85, -145.5, 12.0, 0.06))) {
        return report_c5(simulation, "to the manifold");
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_traversal();
    return hold_stick(simulation, 0.0, 0.0, 1.0, 0.0, 2.0, [](const Snapshot &state) {
               return state.traversal_state == TraversalState::None && state.player_grounded &&
                      state.player_position.x > 0.9;
           }) ||
           report_c5(simulation, "over the manifold");
}

void run_service_c5() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;

    // ---- Standing, nothing passes under the duct bank --------------------------------
    Simulation upright(InitialSpawn::Deck706);
    (void)upright.advance_frame(1.0);
    require(c5_over_manifold(upright), "C5: over the manifold onto the plant floor");
    (void)walk_to(upright, 5.6, -145.5, 4.0, 0.08);
    require(upright.snapshot().player_position.x < 3.0,
            "standing, a body must not get under C5's duct bank: the way on is crouched");

    // ---- A walking jump falls short of the gap, onto the plant floor ------------------
    Simulation walker(InitialSpawn::Deck706);
    (void)walker.advance_frame(1.0);
    C5Notes ignored;
    require(c5_over_manifold(walker), "C5: over the manifold, for the walking jump");
    (void)walker.set_crouch_input(true);
    require(walk_to(walker, 5.6, -145.5, 8.0, 0.08), "C5: under the ducts, for the walking jump");
    (void)walker.set_crouch_input(false);
    (void)walker.advance_frame(0.3);
    require(walk_to(walker, 8.5, -146.5, 4.0, 0.06) && c4_mantle(walker, 1.0, 0.0, 708.2, "C5 onto the tank") &&
                c5_pipe(walker, 10.25, -144.8, 0.0, 1.0, 714.5, "up the tank's standpipe") &&
                walk_to(walker, 10.85, -142.6, 4.0, 0.08),
            "C5: up onto the platform over the tank, for the walking jump");
    (void)walker.set_facing(-1.0, 0.0);
    (void)walker.advance_frame(0.2);
    (void)hold_stick(walker, -1.0, 0.0, -1.0, 0.0, 2.0, [](const Snapshot &state) { return state.player_position.x <= 8.45; });
    (void)walker.request_jump();
    (void)hold_stick(walker, -1.0, 0.0, -1.0, 0.0, 4.0,
                     [](const Snapshot &state) { return state.player_grounded && state.player_position.y < 709.0; });
    (void)walker.advance_frame(1.0);
    const auto short_of = walker.snapshot();
    require(short_of.player_position.y < 708.0 && short_of.death_count == 0,
            "a walking jump must fall short of C5's gap and land on the plant floor, alive");

    // ---- Walked into the tank top's rails, a body stays up -----------------------------
    Simulation railed(InitialSpawn::Deck706);
    (void)railed.advance_frame(1.0);
    require(c5_over_manifold(railed), "C5: over the manifold, for the tank's rails");
    (void)railed.set_crouch_input(true);
    require(walk_to(railed, 5.6, -145.5, 8.0, 0.08), "C5: under the ducts, for the tank's rails");
    (void)railed.set_crouch_input(false);
    (void)railed.advance_frame(0.3);
    require(walk_to(railed, 8.5, -146.5, 4.0, 0.06) && c4_mantle(railed, 1.0, 0.0, 708.2, "C5 onto the tank"),
            "C5: onto the tank, for its rails");
    require(c4_stays_up(railed, 10.3, -147.5, 0.0, -1.0, kC5TankTop) && c4_stays_up(railed, 10.3, -147.5, 1.0, 0.0, kC5TankTop) &&
                c4_stays_up(railed, 10.3, -142.0, 0.0, 1.0, kC5TankTop),
            "walked into the rails round C5's tank top, a body must stay up on it");

    // ---- North of the landing, over the void: edges that hold and edges that drop ----
    // The first and second platforms stand over nothing but the 662 deck, 55 m
    // down. Walked into the first's west parapet (a step off it would land by
    // the 706 deck's hatch and slide in) and into the faces over the first's
    // and the second's east edges, a body stays up; stepped slowly off the top
    // platform's west edge where it juts past the second's hang face, it lands
    // on the second; stepped off the second's south edge, it lands on the duct
    // bank; walked into the bank's ends, it stays up.
    Simulation edges(InitialSpawn::Deck706);
    (void)edges.advance_frame(1.0);
    C5Notes unused;
    require(c5_to_landing(edges, unused) && walk_to(edges, -0.5, -141.6, 6.0, 0.05) &&
                c4_hang_up(edges, 0.0, 1.0, 717.7, "C5 the first hang"),
            "C5: up onto the first platform, for its edges");
    require(c4_stays_up(edges, -1.2, -140.3, -1.0, 0.0, kC5FirstTop) &&
                c4_stays_up(edges, 2.3, -139.6, 1.0, 0.0, kC5FirstTop),
            "walked into the first platform's west parapet and the face over its east edge, a body must stay up");
    require(walk_to(edges, 2.55, -139.6, 6.0, 0.05) && c4_hang_up(edges, 1.0, 0.0, 720.9, "C5 the second hang"),
            "C5: up onto the second platform, for its edges");
    require(c4_stays_up(edges, 6.4, -140.0, 1.0, 0.0, kC5SecondTop),
            "walked into the face over the second platform's east edge, a body must stay up");
    require(c5_pipe(edges, 6.35, -139.63, 1.0, 0.0, 727.0, "up the second standpipe") &&
                walk_to(edges, 7.6, -141.85, 6.0, 0.05),
            "C5: onto the top platform where it juts, for its west edge");
    (void)hold_stick(edges, -0.15, 0.0, -1.0, 0.0, 6.0, [](const Snapshot &state) { return state.player_position.y < 725.0; });
    (void)wait_for(edges, 4.0, [](const Snapshot &state) { return state.player_grounded; });
    require(standing_above(edges.snapshot(), kC5SecondTop + 0.5) && edges.snapshot().death_count == 0,
            "stepped slowly off the top platform's west edge where it juts, a body must land on the second platform, alive");
    require(walk_to(edges, 4.0, -141.6, 6.0, 0.05), "C5: to the second platform's south edge, over the duct bank");
    (void)hold_stick(edges, 0.0, -0.15, 0.0, -1.0, 6.0, [](const Snapshot &state) { return state.player_position.y < 719.0; });
    (void)wait_for(edges, 4.0, [](const Snapshot &state) { return state.player_grounded; });
    require(standing_above(edges.snapshot(), kC5DuctTop + 0.5) && edges.snapshot().death_count == 0,
            "stepped off the second platform's south edge, a body must land on the duct bank, alive");
    require(c4_stays_up(edges, 4.0, -141.6, 0.0, 1.0, kC5DuctTop) &&
                c4_stays_up(edges, 4.0, -148.3, 0.0, -1.0, kC5DuctTop),
            "walked into the duct bank's ends, a body must stay up on it");

    // ---- The climb -------------------------------------------------------------------
    Simulation c5(InitialSpawn::Deck706);
    require(c5.advance_frame(1.0).accepted, "C5 settle interval must be accepted");
    const double start = c5.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    C5Notes notes;
    const bool climbed = climb_c5(c5, &notes);
    g_path_watch.armed = false;
    require(climbed, "C5 must carry a climber from the 706 deck to the 728 deck");
    require(notes.vaulted, "C5's manifold must go by in a vault");
    require(notes.crawled, "C5's duct bank must be passed crouched, and the body stand again past it");
    require(notes.gap_speed > 7.5, "C5's gap must be taken at a sprint");
    require(g_path_watch.worst <= 0.15, "climbing C5, the body must never move more than 0.15 m sideways in one tick");
    const auto top = c5.snapshot();
    require(top.death_count == 0, "C5 must be climbed without a death");
    std::cout << "PASS scraperx_sim AS-010 C5: seconds=" << top.simulation_time_seconds - start
              << " deck728_y=" << top.player_position.y << " gap_speed_mps=" << notes.gap_speed
              << " walk_jump_landed_y=" << short_of.player_position.y << " worst_step_m=" << g_path_watch.worst << '\n';
}
