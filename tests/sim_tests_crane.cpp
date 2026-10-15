// AS-009, the Facade Crane Stack (484 -> 640 m), and AS-010 stage M.

#include "sim_tests.hpp"

// ---- AS-009, the Facade Crane Stack (484 -> 640 m) ------------------------------

// From the 484 ring's north band, the rail joint off the ring and laid in its
// cradle from J's traveler.
bool lay_crane_joint(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 2.8, -139.0, 12.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kCraneJJointEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kCraneJJointEntityId ||
        !(walk_to(simulation, -0.3, -138.3, 8.0) && walk_to(simulation, -0.3, -136.2, 6.0, 0.08) &&
          walk_to(simulation, 0.9, -136.2, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.5);
    return simulation.crane_state().j_rail_whole;
}

// On J's traveler, the wagon's chock lever pulled over by its lanyard's
// handle beyond the traveler's north edge.
bool pull_crane_chock(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, 0.0, -135.5, 0.0, 1.0, Simulation::kCraneJHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.crane_state().j_wagon_latched;
                       });
}

// On K's cage: round its hanging shackle, take it, and hook it on the eye.
bool rig_crane_k(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    // Straight to the cage's north side, along it clear of the shackle on its
    // long rope, and round behind it.
    const double x0 = simulation.snapshot().player_position.x;
    if (!(walk_to(simulation, x0, -154.2, 6.0) && walk_to(simulation, -13.3, -154.2, 10.0) &&
          walk_to(simulation, -13.3, -155.0, 4.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kCraneKShackleEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kCraneKShackleEntityId ||
        !walk_to(simulation, -12.3, -155.0, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kCraneKCageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.crane_state().k_rope_on_eye;
}

// The jib's pendant pin, drawn by its lanyard's handle beyond the cage's
// north edge.
bool pull_crane_pendant(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -12.6, -154.3, 0.0, 1.0, Simulation::kCraneKHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.crane_state().k_jib_latched;
                       });
}

// On L's cab: the winch's clutch lever thrown in by its handle beyond the
// cab's north edge.
bool throw_crane_clutch(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -3.7, -159.4, 0.0, 1.0, Simulation::kCraneLClutchHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) { return simulation.crane_state().l_clutch_in; });
}

// The drop weight's pin, drawn by its lanyard's handle beyond the cab's south
// edge.
bool pull_crane_drop(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -3.0, -160.8, 0.0, -1.0, Simulation::kCraneLPinHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.crane_state().l_weight_latched;
                       });
}

void run_facade_crane() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- J: the joint out, the traveler cannot pass the gap ----------------------
    Simulation j_gap(InitialSpawn::Ring484North);
    (void)j_gap.advance_frame(1.0);
    require(!j_gap.crane_state().j_rail_whole && j_gap.crane_state().j_wagon_latched,
            "as found, J's rail is missing its joint and the wagon stands chocked");
    require(walk_to(j_gap, 0.0, -138.3, 8.0) && walk_to(j_gap, 0.0, -136.2, 6.0) && pull_crane_chock(j_gap),
            "the rider must pull the wagon's chock from the traveler");
    (void)j_gap.advance_frame(10.0);
    require(j_gap.crane_state().j_traveler_travel < 0.05 && j_gap.crane_state().j_wagon_travel < 0.3,
            "with the rail's joint out the traveler holds at the gap and the wagon on its rope");

    // ---- J: the joint in, the ride -----------------------------------------------
    Simulation j_ride(InitialSpawn::Ring484North);
    (void)j_ride.advance_frame(1.0);
    require(lay_crane_joint(j_ride), "the rider must lay the rail's joint in its cradle");
    const double j_rider_y0 = j_ride.snapshot().player_position.y;
    const double j_wagon_y0 = kit_com_y(j_ride, Simulation::kCraneJWagonEntityId);
    require(pull_crane_chock(j_ride), "the rider must pull the wagon's chock, the rail whole");
    const bool j_arrived = wait_for(j_ride, 60.0, [&](const scraperx::sim::Snapshot &) {
        return j_ride.crane_state().j_traveler_travel >= 43.9;
    });
    (void)j_ride.advance_frame(1.0);
    const auto j_top = j_ride.snapshot();
    require(j_arrived, "the runaway wagon must drag J's traveler to the 528 ring");
    require(on_support(j_top, Simulation::kCraneJTravelerEntityId), "the rider must ride J's traveler all the way");
    const double j_gain = kRiderMassKg * kG * (j_top.player_position.y - j_rider_y0);
    const double j_released = 4000.0 * kG * (j_wagon_y0 - kit_com_y(j_ride, Simulation::kCraneJWagonEntityId));
    require(j_gain > 0.0 && j_gain <= j_released, "J's rider must never gain more than the wagon released");
    std::cout << "PASS scraperx_sim AS-009 J: rider_y=" << j_top.player_position.y << " gain_J=" << j_gain
              << " released_J=" << j_released << '\n';

    // ---- K: the shackle free, the jib swings for nothing ----------------------------
    Simulation k_free(InitialSpawn::CraneKCage);
    (void)k_free.advance_frame(1.0);
    require(!k_free.crane_state().k_rope_on_eye && k_free.crane_state().k_jib_latched,
            "as found, K's shackle hangs free and the jib stands pinned level");
    require(pull_crane_pendant(k_free), "the rider must pull the pendant's pin from the cage");
    (void)k_free.advance_frame(20.0);
    require(k_free.crane_state().k_jib_angle > 1.3 && k_free.crane_state().k_cage_travel < 0.05,
            "with the shackle free the jib swings down and K's cage stays");

    // ---- K: hooked on, the ride ----------------------------------------------------
    Simulation k_ride(InitialSpawn::CraneKCage);
    (void)k_ride.advance_frame(1.0);
    require(rig_crane_k(k_ride), "the rider must hook K's shackle on the cage's eye");
    const double k_rider_y0 = k_ride.snapshot().player_position.y;
    const double k_jib_y0 = kit_com_y(k_ride, Simulation::kCraneKJibEntityId);
    require(pull_crane_pendant(k_ride), "the rider must pull the pendant's pin, hooked on");
    const bool k_arrived = wait_for(k_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return k_ride.crane_state().k_cage_travel >= 43.9;
    });
    (void)k_ride.advance_frame(1.0);
    const auto k_top = k_ride.snapshot();
    require(k_arrived, "the jib's swing must haul K's cage to the 572 ring");
    require(on_support(k_top, Simulation::kCraneKCageEntityId), "the rider must ride K's cage all the way");
    const double k_gain = kRiderMassKg * kG * (k_top.player_position.y - k_rider_y0);
    const double k_released = 8000.0 * kG * (k_jib_y0 - kit_com_y(k_ride, Simulation::kCraneKJibEntityId));
    require(k_gain > 0.0 && k_gain <= k_released, "K's rider must never gain more than the jib released");
    std::cout << "PASS scraperx_sim AS-009 K: rider_y=" << k_top.player_position.y
              << " jib_angle=" << k_ride.crane_state().k_jib_angle << " gain_J=" << k_gain
              << " released_J=" << k_released << '\n';

    // ---- L: the clutch out, the cart runs for nothing ---------------------------------
    Simulation l_free(InitialSpawn::CraneLCab);
    (void)l_free.advance_frame(1.0);
    require(!l_free.crane_state().l_clutch_in && l_free.crane_state().l_cart_latched &&
                l_free.crane_state().l_weight_latched,
            "as found, L's clutch is out, the cart caught and the drop weight pinned");
    require(pull_crane_drop(l_free), "the rider must pull the drop weight's pin from the cab");
    (void)l_free.advance_frame(40.0);
    require(!l_free.crane_state().l_cart_latched && l_free.crane_state().l_cart_travel > 40.0 &&
                l_free.crane_state().l_cab_travel < 0.05,
            "with the clutch out the weight trips the cart, it runs down, and L's cab stays");

    // ---- L: the clutch in, the cascade and the ride -------------------------------------
    Simulation l_ride(InitialSpawn::CraneLCab);
    (void)l_ride.advance_frame(1.0);
    require(throw_crane_clutch(l_ride), "the rider must throw the winch's clutch in");
    const double l_rider_y0 = l_ride.snapshot().player_position.y;
    const double l_cart_y0 = kit_com_y(l_ride, Simulation::kCraneLCartEntityId);
    const double l_weight_y0 = kit_com_y(l_ride, Simulation::kCraneLWeightEntityId);
    require(pull_crane_drop(l_ride), "the rider must pull the drop weight's pin, the clutch in");
    const bool l_arrived = wait_for(l_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return l_ride.crane_state().l_cab_travel >= 67.9;
    });
    (void)l_ride.advance_frame(1.0);
    const auto l_top = l_ride.snapshot();
    require(l_arrived, "the cart's run must wind L's cab up into TP-640");
    require(on_support(l_top, Simulation::kCraneLCabEntityId), "the rider must ride L's cab all the way");
    const double l_gain = kRiderMassKg * kG * (l_top.player_position.y - l_rider_y0);
    const double l_released =
        4000.0 * kG * (l_cart_y0 - kit_com_y(l_ride, Simulation::kCraneLCartEntityId)) +
        200.0 * kG * (l_weight_y0 - kit_com_y(l_ride, Simulation::kCraneLWeightEntityId));
    require(l_gain > 0.0 && l_gain <= l_released, "L's rider must never gain more than the cart and weight released");
    std::cout << "PASS scraperx_sim AS-009 L: rider_y=" << l_top.player_position.y << " gain_J=" << l_gain
              << " released_J=" << l_released << '\n';
}

// AS-009's band from wherever on the 484 ring's west or north band: round to
// J's joint and traveler, the ride to 528; round the 528 ring and over the
// board to K's cage, hooked on, the pendant's pin, the ride to 572; over the
// board and round the 572 ring to L's cab, the clutch, the drop weight, the
// ride into TP-640; off onto the plate.
bool climb_crane_band(scraperx::sim::Simulation &band) {
    using scraperx::sim::Simulation;
    const auto here = band.snapshot().player_position;
    if (here.x < -8.0 && !walk_to(band, here.x, -140.3, 12.0)) {
        std::cout << "band: round to the 484 ring's north band\n";
        return false;
    }
    if (!(walk_to(band, -3.0, -140.3, 12.0) && lay_crane_joint(band) && pull_crane_chock(band))) {
        std::cout << "band: J rigged and set off\n";
        return false;
    }
    if (!wait_for(band, 60.0, [&](const scraperx::sim::Snapshot &) {
            return band.crane_state().j_traveler_travel >= 43.9;
        })) {
        std::cout << "band: J's traveler to 528\n";
        return false;
    }
    (void)band.advance_frame(1.0);
    if (!(walk_to(band, 0.0, -138.6, 8.0) && walk_to(band, 0.0, -141.5, 8.0) && walk_to(band, -8.5, -141.5, 12.0) &&
          walk_to(band, -8.5, -155.0, 16.0) && walk_to(band, -11.6, -155.0, 8.0))) {
        const auto p = band.snapshot().player_position;
        std::cout << "band: round the 528 ring to K's board, at " << p.x << ' ' << p.y << ' ' << p.z << '\n';
        return false;
    }
    if (!(rig_crane_k(band) && pull_crane_pendant(band))) {
        const auto p = band.snapshot().player_position;
        std::cout << "band: K rigged and set off, at " << p.x << ' ' << p.y << ' ' << p.z << '\n';
        return false;
    }
    if (!wait_for(band, 90.0, [&](const scraperx::sim::Snapshot &) {
            return band.crane_state().k_cage_travel >= 43.9;
        })) {
        std::cout << "band: K's cage to 572\n";
        return false;
    }
    (void)band.advance_frame(1.0);
    if (!(walk_to(band, -11.9, -155.0, 6.0) && walk_to(band, -6.5, -155.0, 10.0) &&
          walk_to(band, -6.5, -156.7, 6.0) && walk_to(band, -3.0, -156.7, 8.0) &&
          walk_to(band, -3.0, -159.6, 6.0) && throw_crane_clutch(band) && pull_crane_drop(band))) {
        std::cout << "band: over the board and round the 572 ring to L, clutch in and set off\n";
        return false;
    }
    if (!wait_for(band, 90.0, [&](const scraperx::sim::Snapshot &) {
            return band.crane_state().l_cab_travel >= 67.9;
        })) {
        std::cout << "band: L's cab to TP-640\n";
        return false;
    }
    (void)band.advance_frame(1.0);
    if (!walk_to(band, -3.0, -156.5, 8.0)) {
        std::cout << "band: off L's cab onto TP-640\n";
        return false;
    }
    (void)band.advance_frame(0.5);
    return standing_above(band.snapshot(), 640.2);
}

void run_crane_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::Ring484North);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(climb_crane_band(band), "band: the 484 ring to standing on TP-640 through J, K and L");
    const auto top = band.snapshot();
    std::cout << "PASS scraperx_sim AS-009 band: to_640_s=" << top.simulation_time_seconds - start
              << " plate_y=" << top.player_position.y << '\n';
}

// AS-009's climbing route, on player inputs: from the 484 ring's north band to
// the east band, one L of boards and a ladder per ring gap to 616, then the
// ladder up through TP-640's hatch; every lift in the band where it was found.
void run_crane_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation route(InitialSpawn::Ring484North);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    require(walk_to(route, -3.0, -140.8, 6.0) && walk_to(route, 10.3, -140.8, 16.0) &&
                walk_to(route, 10.3, -149.0, 12.0),
            "route: along the 484 ring to its east band");
    for (double h = 484.0; h < 615.9; h += 22.0) {
        const double s = 14.72 - 0.91 * (h - 330.0) / 22.0;
        // Stopped at the boards' end before turning onto them: the walk off
        // the ladder carries its speed across their line.
        const bool leg = walk_to(route, s + 0.25, -149.0, 12.0, 0.1) && route.advance_frame(0.4).accepted &&
                         walk_to(route, s - 1.67, -149.0, 8.0, 0.1) &&
                         climb_wet_hold(route, s - 1.67, -150.0, 1.0, 0.0, false, h + 22.5);
        if (!leg) {
            std::cout << "route: stuck above " << h << " at y=" << route.snapshot().player_position.y << '\n';
        }
        require(leg, "route: boards and a ladder to the next ring");
    }
    require(climb_wet_hold(route, 4.9, -151.02, 0.0, 1.0, false, 640.5),
            "route: the ladder up through TP-640's hatch");
    const auto top = route.snapshot();
    const auto crane = route.crane_state();
    require(crane.j_traveler_travel < 0.02 && crane.k_cage_travel < 0.02 && crane.l_cab_travel < 0.02 &&
                crane.j_wagon_latched && crane.k_jib_latched && crane.l_cart_latched,
            "route: no lift in the band moved");
    std::cout << "PASS scraperx_sim AS-009 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lifts_untouched=1\n";
}

// AS-009's one-shot wreckage, on player inputs: each spent machine reached and
// climbed.
void run_crane_wreckage() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    // ---- K, its pendant pin pulled with the shackle free ------------------------------
    Simulation k(InitialSpawn::CraneKCage);
    (void)k.advance_frame(1.0);
    require(pull_crane_pendant(k) &&
                wait_for(k, 20.0, [&](const scraperx::sim::Snapshot &) { return k.crane_state().k_jib_angle > 1.56; }) &&
                k.crane_state().k_cage_travel < 0.05,
            "K wreck: the jib swings down to hang plumb, the cage stays");
    require(walk_to(k, -12.6, -155.0, 4.0) && walk_to(k, -9.5, -155.0, 8.0),
            "K wreck: off the cage over its board onto the 528 ring");
    require(walk_to(k, -10.25, -151.0, 8.0, 0.08), "K wreck: to the ring's edge beside the hanging jib");
    (void)k.set_facing(-1.0, 0.0);
    (void)k.advance_frame(0.4);
    (void)k.request_traversal();
    (void)k.advance_frame(0.2);
    const auto k_hold = k.snapshot();
    require(is_climbing(k_hold) && k_hold.traversal_support_entity_id == Simulation::kCraneKJibEntityId,
            "K wreck: a hold on the hanging jib");
    require(hold_stick(k, -1.0, 0.0, -1.0, 0.0, 40.0,
                       [](const scraperx::sim::Snapshot &state) { return state.player_position.y >= 551.3; }),
            "K wreck: up the jib to its heel");
    (void)k.request_jump();
    require(hold_stick(k, 1.0, 0.0, 1.0, 0.0, 4.0,
                       [](const scraperx::sim::Snapshot &state) { return standing_above(state, 550.2); }),
            "K wreck: from the jib's heel back onto the 550 ring");
    std::cout << "PASS scraperx_sim AS-009 wreckage K: jib_angle=" << k.crane_state().k_jib_angle
              << " ring_y=" << k.snapshot().player_position.y << '\n';

    // ---- J, ridden to the 528 ring, where its wagon's run ends beside the ring ----------
    Simulation j(InitialSpawn::Ring484North);
    (void)j.advance_frame(1.0);
    require(lay_crane_joint(j) && pull_crane_chock(j) &&
                wait_for(j, 60.0,
                         [&](const scraperx::sim::Snapshot &) { return j.crane_state().j_traveler_travel >= 43.9; }),
            "J wreck: J ridden to the 528 ring");
    (void)j.advance_frame(1.0);
    require(walk_to(j, 0.0, -138.6, 8.0) && walk_to(j, 0.0, -141.5, 8.0) && walk_to(j, 9.8, -141.5, 12.0),
            "J wreck: off the traveler and along the 528 ring to its east band");
    require(climb_wreck(j, 10.3, -142.0, 1.0, 0.0, Simulation::kCraneJWagonEntityId, 531.9) &&
                j.snapshot().support_entity_id == Simulation::kCraneJWagonEntityId,
            "J wreck: the spent wagon climbed from the ring onto its deck");
    std::cout << "PASS scraperx_sim AS-009 wreckage J: wagon_travel=" << j.crane_state().j_wagon_travel
              << " deck_y=" << j.snapshot().player_position.y << '\n';

    // ---- L, its drop weight's pin pulled with the clutch out ----------------------------
    Simulation l(InitialSpawn::CraneLCab);
    (void)l.advance_frame(1.0);
    require(pull_crane_drop(l) &&
                wait_for(l, 40.0, [&](const scraperx::sim::Snapshot &) { return l.crane_state().l_cart_travel > 51.9; }) &&
                l.crane_state().l_cab_travel < 0.05,
            "L wreck: the cart runs down beside the 572 ring, the cab stays");
    require(walk_to(l, -3.0, -158.3, 6.0) && walk_to(l, 0.1, -158.3, 8.0), "L wreck: off the cab onto the 572 ring");
    require(climb_wreck(l, 0.1, -158.45, 0.0, -1.0, Simulation::kCraneLCartEntityId, 575.9) &&
                l.snapshot().support_entity_id == Simulation::kCraneLCartEntityId,
            "L wreck: the spent cart climbed from the ring onto its deck");
    std::cout << "PASS scraperx_sim AS-009 wreckage L: cart_travel=" << l.crane_state().l_cart_travel
              << " deck_y=" << l.snapshot().player_position.y << '\n';
}

// ---- AS-010, Midstack Service (640 -> 780 m): stage M ---------------------------

// The service lift's reel and cage, as its contract gives them: the reel's
// drum and yoke, its cable's density and coil, and the cage.
constexpr double kReelMassKg = 1339.5;
constexpr double kCableKgPerM = 22.0;
constexpr double kCableCoil = 23.0;
constexpr double kServiceCageKg = 1500.0;

// From L's hole on TP-640 north and west into the service cage, round the
// shackle hanging over its middle, take it, and hook it on the cage's eye.
bool rig_service_m(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, -3.0, -144.3, 12.0) && walk_to(simulation, -8.7, -144.3, 8.0) &&
          walk_to(simulation, -8.7, -143.5, 4.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kServiceMShackleEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kServiceMShackleEntityId ||
        !walk_to(simulation, -7.7, -143.5, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kServiceMCageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.service_state().m_rope_on_eye;
}

// The reel's chock, pulled over by its lanyard's handle beyond the cage's
// north rail.
bool pull_service_chock(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -8.0, -142.8, 0.0, 1.0, Simulation::kServiceMHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) { return !simulation.service_state().m_reel_latched; });
}

// What the reel released falling `drop` metres: its drum and yoke all the
// way, the cable still wound all the way, each paid-out metre as far as it
// fell before it paid out.
double reel_released(const double drop) {
    const double paid = std::min(drop, kCableCoil);
    return 9.81 * (kReelMassKg * drop + kCableKgPerM * ((kCableCoil - paid) * drop + 0.5 * paid * paid));
}

ServiceRide ride_service_m(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    ServiceRide ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    double last = simulation.service_state().m_cage_travel;
    int still = 0;
    const auto ticks = static_cast<std::uint32_t>(40.0 * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const double travel = simulation.service_state().m_cage_travel;
        const double speed = std::abs(travel - last) / Simulation::kFixedStepSeconds;
        last = travel;
        ride.apex = std::max(ride.apex, travel);
        ride.peak_speed = std::max(ride.peak_speed, speed);
        still = travel > 1.0 && speed < 0.005 ? still + 1 : 0;
        if (still >= static_cast<int>(Simulation::kTickRateHz)) {
            ride.settled = true;
            break;
        }
    }
    ride.seconds = simulation.snapshot().simulation_time_seconds - start;
    return ride;
}

void run_midstack_service() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- M: the shackle free, the reel falls for nothing ---------------------------
    Simulation m_free(InitialSpawn::PlateTop640);
    (void)m_free.advance_frame(1.0);
    const auto found = m_free.service_state();
    require(!found.m_rope_on_eye && found.m_reel_latched && found.m_cage_travel < 0.01 &&
                std::abs(found.m_reel_kg - (kReelMassKg + kCableKgPerM * kCableCoil)) < 0.5,
            "as found, M's shackle hangs free, its reel stands chocked at the top, full of cable");
    require(walk_to(m_free, -3.0, -144.3, 12.0) && pull_service_chock(m_free),
            "the rider must pull the reel's chock from the cage");
    (void)m_free.advance_frame(15.0);
    const auto spent = m_free.service_state();
    require(spent.m_reel_travel > 22.0 && spent.m_cage_travel < 0.05 && spent.m_cable_paid > 22.0 &&
                spent.m_reel_kg < kReelMassKg + kCableKgPerM * (kCableCoil - 22.0) + 0.5,
            "with the shackle free the reel falls its shaft, paying out its cable, and M's cage stays");

    // ---- M: hooked on, the ride ---------------------------------------------------
    Simulation m_ride(InitialSpawn::PlateTop640);
    (void)m_ride.advance_frame(1.0);
    require(rig_service_m(m_ride), "the rider must hook M's shackle on the cage's eye");
    const double rider_y0 = m_ride.snapshot().player_position.y;
    const double cage_y0 = kit_com_y(m_ride, Simulation::kServiceMCageEntityId);
    const double reel_y0 = kit_com_y(m_ride, Simulation::kServiceMReelEntityId);
    require(pull_service_chock(m_ride), "the rider must pull the reel's chock, hooked on");
    const ServiceRide ride = ride_service_m(m_ride);
    const auto top = m_ride.snapshot();
    const auto held = m_ride.service_state();
    require(ride.settled, "M's cage must come to rest on its dogs");
    require(held.m_cage_travel >= 21.05 && held.m_cage_travel < 22.9 && ride.apex < 22.9,
            "the reel's fall must lift M's cage to the 662 deck and its dogs hold it there, clear of its stop");
    require(ride.peak_speed <= 3.5, "M's cage must never pass 3.5 m/s: the reel's drive fades as it pays out");
    require(on_support(top, Simulation::kServiceMCageEntityId), "the rider must ride M's cage all the way");
    const double drop = reel_y0 - kit_com_y(m_ride, Simulation::kServiceMReelEntityId);
    const double gain = kRiderMassKg * kG * (top.player_position.y - rider_y0) +
                        kServiceCageKg * kG * (kit_com_y(m_ride, Simulation::kServiceMCageEntityId) - cage_y0);
    const double released = reel_released(drop);
    require(gain > 0.0 && gain <= released, "M's cage and rider must never gain more than the reel released");
    std::cout << "PASS scraperx_sim AS-010 M: apex_m=" << ride.apex << " held_m=" << held.m_cage_travel
              << " peak_mps=" << ride.peak_speed << " seconds=" << ride.seconds
              << " rider_y=" << top.player_position.y << " reel_kg=" << held.m_reel_kg << " gain_J=" << gain
              << " released_J=" << released << '\n';

    // ---- M: the rider off at the top, the empty cage stays on its dogs --------------
    require(walk_to(m_ride, -5.8, -143.5, 6.0), "off M's cage east onto the 662 deck");
    (void)m_ride.advance_frame(1.0);
    const auto off = m_ride.snapshot();
    require(on_support(off, Simulation::kServiceFrameEntityId) && off.player_position.y > 662.5,
            "the rider must stand on the 662 deck");
    (void)m_ride.advance_frame(10.0);
    require(std::abs(m_ride.service_state().m_cage_travel - held.m_cage_travel) < 0.01,
            "empty, M's cage must stay where its dogs hold it");
}
