#include "sim_checks.hpp"
#include "sim_routes.hpp"

// The service skin and the floors from the girder up to the casing.

// ---- B06 west service skin (TP-640 -> 672.25) --------------------------------

void report_skin(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "skin " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " grounded=" << state.player_grounded
              << " crouched=" << state.player_crouched << " balancing=" << state.player_balancing
              << " traversal=" << static_cast<int>(state.traversal_state)
              << " support=" << state.support_entity_id << " ledge=" << state.ledge_available
              << " rise=" << state.ledge_rise_meters << " grip=" << state.grip_available
              << " deaths=" << state.death_count << "\n";
}


// The authored route, on player inputs. Each leg is one verb the geometry
// offers, and the next leg is not available until that verb has been used.
bool climb_service_skin(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    auto fail = [&](const char *leg) {
        report_skin(simulation, leg);
        return false;
    };

    // South of the manifold, facing it. The bar offers a vault, not a hold.
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    const auto at_bar = simulation.snapshot();
    if (!at_bar.ledge_available || at_bar.ledge_rise_meters < 0.85 || at_bar.ledge_rise_meters > 1.05) {
        return fail("manifold vault not offered");
    }
    if (at_bar.grip_available) {
        return fail("a hold was offered at the manifold; the vault is the move");
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) {
            return state.traversal_state == TraversalState::Vaulting ||
                   (state.player_grounded && state.player_position.z > -145.55 &&
                    state.traversal_state == TraversalState::None);
        })) {
        return fail("vault the manifold");
    }
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) {
            return state.player_grounded && state.traversal_state == TraversalState::None &&
                   state.player_position.z > -145.55 && state.player_position.y < 642.2;
        })) {
        return fail("land north of the manifold");
    }

    // Turn west. The duct's east face is a mantle; it is not in reach from
    // the spawn, which is what the vault was for.
    if (!(walk_to(simulation, -11.50, -144.40, 8.0, 0.12))) {
        return fail("to the duct");
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    const auto at_duct = simulation.snapshot();
    if (!at_duct.ledge_available || at_duct.ledge_rise_meters < 1.30 || at_duct.ledge_rise_meters > 1.60) {
        return fail("duct mantle not offered");
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 3.0, [](const Snapshot &state) { return standing_above(state, 642.40); })) {
        return fail("mantle onto the duct");
    }

    // Out along the inspection beam. It must read as a balance, and the
    // lattice must not be a hold while still standing on the beam.
    bool balanced = false;
    {
        const auto ticks = static_cast<std::uint32_t>(12.0 * Simulation::kTickRateHz);
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto state = simulation.snapshot();
            if (state.player_balancing) {
                balanced = true;
            }
            if (std::hypot(-15.35 - state.player_position.x, -142.35 - state.player_position.z) <= 0.18) {
                break;
            }
            steer_toward(simulation, -15.35, -142.35, 0.55);
            (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)simulation.set_move_input(0.0, 0.0);
    }
    if (!balanced) {
        return fail("the inspection beam was not a balance");
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.35);
    if (simulation.snapshot().grip_available) {
        return fail("the lattice was in reach from the beam; the jump is the move");
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, 0.85, 0.0, 1.0, 2.2,
                    [](const Snapshot &state) { return is_climbing(state); })) {
        return fail("catch the lattice");
    }
    // Up to the traverse holds. A top-out here is the canopy failing.
    if (!hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 14.0, [](const Snapshot &state) {
            return is_climbing(state) && state.player_position.y >= 649.50;
        })) {
        return fail("climb the lattice");
    }
    if (!is_climbing(simulation.snapshot())) {
        return fail("the canopy let a top-out off the lattice");
    }
    // East along the holds, then face the balcony fascia and mantle.
    if (!hold_stick(simulation, 1.0, 0.15, 0.0, 1.0, 10.0, [](const Snapshot &state) {
            return is_climbing(state) && state.player_position.x >= -13.75;
        })) {
        return fail("traverse to the balcony");
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 3.0, [](const Snapshot &state) { return standing_above(state, 650.90); })) {
        return fail("mantle onto the balcony");
    }

    // Under the tray. Standing is refused once the body is in the lane;
    // crouched, the lane is open. Crouch on the apron, then step in.
    if (!walk_to(simulation, -12.10, -140.55, 8.0, 0.18)) {
        return fail("to the tray");
    }
    (void)simulation.set_crouch_input(true);
    (void)simulation.advance_frame(0.3);
    if (!simulation.snapshot().player_crouched) {
        return fail("did not crouch under the tray");
    }
    if (!walk_to(simulation, -12.10, -141.50, 8.0, 0.18)) {
        return fail("into the tray");
    }
    (void)simulation.set_crouch_input(false);
    (void)simulation.advance_frame(0.3);
    if (!simulation.snapshot().player_crouched) {
        return fail("the tray let a standing body through");
    }
    (void)simulation.set_crouch_input(true);
    if (!walk_to(simulation, -12.10, -144.75, 12.0, 0.12)) {
        return fail("through the tray");
    }
    (void)simulation.set_crouch_input(false);
    if (!wait_for(simulation, 1.5, [](const Snapshot &state) {
            return !state.player_crouched && state.player_grounded;
        })) {
        return fail("could not stand past the tray");
    }

    if (!walk_to(simulation, -12.35, -144.85, 6.0, 0.15)) {
        return fail("to the handler");
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.35);
    const auto at_handler = simulation.snapshot();
    if (!at_handler.ledge_available || at_handler.ledge_rise_meters < 1.40 ||
        at_handler.ledge_rise_meters > 1.70) {
        return fail("handler mantle not offered");
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 3.0, [](const Snapshot &state) { return standing_above(state, 652.45); })) {
        return fail("mantle onto the handler");
    }

    if (!walk_to(simulation, -13.90, -145.20, 6.0, 0.15)) {
        return fail("to the handler's south edge");
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().grip_available) {
        return fail("the louver was in reach from the handler");
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.85, 0.0, -1.0, 2.2,
                    [](const Snapshot &state) { return is_climbing(state); })) {
        return fail("catch the louver");
    }
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 12.0, [](const Snapshot &state) {
            return is_climbing(state) && state.player_position.y >= 657.15;
        })) {
        return fail("climb the louver");
    }
    if (!is_climbing(simulation.snapshot())) {
        return fail("the louver canopy let a top-out");
    }
    // Jump back. A neutral stick bleeds the 2.5 m/s throw off in a fifth of
    // a second. Holding forward — full throw, which is all a key can do —
    // keeps the throw and air-controls it out to about 4 m, still on the pad.
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, 1.0, 0.0, -1.0, 1.6, [](const Snapshot &state) {
            return standing_above(state, 656.70);
        })) {
        return fail("jump back onto the platform");
    }

    if (!walk_to(simulation, -12.70, -141.20, 6.0, 0.16)) {
        return fail("to the brace ladder");
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.35);
    if (!simulation.snapshot().grip_available) {
        return fail("the brace ladder was not a hold");
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 22.0,
                    [](const Snapshot &state) { return standing_above(state, 672.85); })) {
        return fail("climb the brace onto the service deck");
    }
    const auto top = simulation.snapshot();
    if (top.death_count != 0 || !top.player_grounded || top.player_position.y < 672.90 ||
        top.support_entity_id != Simulation::kSkinWestEntityId) {
        return fail("not standing on the 672.25 deck");
    }
    return true;
}


void run_service_skin() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation skin(InitialSpawn::ServiceSkinWest);
    require(skin.advance_frame(0.5).accepted, "service skin settle must be accepted");
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const double start = skin.snapshot().simulation_time_seconds;
    const bool climbed = climb_service_skin(skin);
    g_path_watch.armed = false;
    require(climbed, "the west service skin must be climbed on player inputs");
    require(g_path_watch.worst <= 0.15, "the service skin must not snap the body more than 0.15 m in one tick");
    const auto top = skin.snapshot();
    std::cout << "PASS scraperx_sim service skin: seconds=" << top.simulation_time_seconds - start
              << " deck_y=" << top.player_position.y << " deaths=" << top.death_count
              << " worst_tick_m=" << g_path_watch.worst << "\n";
}


// The girder on the 640 m floor. The pin stays put until it is carried off,
// then the girder falls north onto the next bit of floor and the same player
// walks up it. The player starts where the crane car lets you off.
bool climb_girder(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    const double girder_y0 = kit_y(floor, Simulation::kGirderEntityId);
    require(walk_to(floor, -1.70, -154.90, 8.0, 0.25), "the player must reach the girder pin");
    (void)floor.set_facing(1.0, 0.0);
    require(floor.advance_frame(0.3).accepted, "facing the pin must be accepted");
    require(floor.snapshot().carry_target_entity_id == Simulation::kGirderPinEntityId,
            "the pin must be the thing in reach");
    require(floor.request_pick_up(), "picking up the pin must be accepted");
    require(floor.advance_frame(0.3).accepted, "the pick-up step must be accepted");
    require(floor.snapshot().carrying_entity_id == Simulation::kGirderPinEntityId, "the pin must be in the hands");
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(walk_to(floor, -2.40, -157.20, 8.0, 0.30), "the player must carry the pin clear of the girder");
    require(floor.request_set_down(), "setting the pin down must be accepted");
    require(floor.advance_frame(0.4).accepted, "the pin must leave the hands");
    require(floor.snapshot().carrying_entity_id == 0, "the hands must be empty before the climb");
    observe_path(floor, false);
    bool fell = false;
    for (int step = 0; step < 12 * 90; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        if (kit_y(floor, Simulation::kGirderEntityId) < girder_y0 - 1.2) {
            fell = true;
            break;
        }
    }
    if (!fell) {
        const auto g = floor.kit_body_position(floor.kit_body_index(Simulation::kGirderEntityId));
        const auto pin = floor.kit_body_position(floor.kit_body_index(Simulation::kGirderPinEntityId));
        const auto p = floor.snapshot().player_position;
        std::cout << "no fall y0=" << girder_y0 << " girder " << g.x << " " << g.y << " " << g.z << " pin " << pin.x
                  << " " << pin.y << " " << pin.z << " player " << p.x << " " << p.y << " " << p.z << "\n";
    }
    require(fell, "the girder must fall once the pin is out");
    for (int step = 0; step < 2 * 90; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
    }
    require(floor.snapshot().death_count == 0, "the fall must not kill the player still on the floor");
    auto walk_slow = [&](const double x, const double z, const double seconds, const double tolerance) {
        const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto state = floor.snapshot();
            if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance &&
                state.player_grounded) {
                (void)floor.set_move_input(0.0, 0.0);
                return true;
            }
            steer_toward(floor, x, z, 0.40);
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
        (void)floor.set_move_input(0.0, 0.0);
        return false;
    };
    const bool on_girder = walk_slow(0.0, -154.4, 16.0, 0.45);
    if (!on_girder) {
        const auto p = floor.snapshot();
        std::cout << "onto girder at " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << "\n";
    }
    require(on_girder, "the player must step onto the fallen girder");
    require(walk_slow(0.0, -148.2, 24.0, 0.40), "the player must walk up the girder onto the next floor");
    bool up = false;
    for (int step = 0; step < 3 * 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        if (state.death_count == 0 && state.player_grounded && state.player_position.y > 644.3 &&
            state.player_position.z > -149.6 && state.player_position.z < -147.1 &&
            state.support_entity_id == Simulation::kCraneFrameEntityId) {
            up = true;
            break;
        }
    }
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15, "the girder must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the next floor, still inside the building");
    const auto landed = floor.snapshot();
    std::cout << "PASS scraperx_sim girder: girder_y0=" << girder_y0
              << " girder_y=" << kit_y(floor, Simulation::kGirderEntityId)
              << " floor_y=" << landed.player_position.y << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The sheet in front of the ladder. Lift the pin, the weight drops, the sheet
// slides off the ladder, and the same player climbs.
bool climb_shutter(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const auto shutter_x = [&]() {
        return floor.kit_body_position(floor.kit_body_index(Simulation::kShutterEntityId)).x;
    };
    const double x0 = shutter_x();
    auto watch = [&](const double seconds) {
        const int ticks = static_cast<int>(seconds * 90.0 + 0.5);
        for (int step = 0; step < ticks; ++step) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    // The sheet has to be in the way. Standing against it, facing the ladder,
    // must not offer a hold. If it does, the pin does nothing the climb needs.
    (void)walk_to(floor, 2.15, -148.00, 8.0, 0.12);
    (void)floor.set_facing(0.0, 1.0);
    watch(0.4);
    if (floor.snapshot().grip_available) {
        const auto blocked = floor.snapshot();
        const auto w = floor.kit_body_position(floor.kit_body_index(Simulation::kShutterWeightEntityId));
        const auto pin = floor.kit_body_position(floor.kit_body_index(Simulation::kShutterPinEntityId));
        std::cout << "ladder still open " << blocked.player_position.x << " " << blocked.player_position.y
                  << " " << blocked.player_position.z << " sheet=" << shutter_x() << " weight " << w.y
                  << " pin " << pin.x << " " << pin.y << " " << pin.z << "\n";
    }
    require(!floor.snapshot().grip_available, "the sheet must block the ladder until the pin is lifted");
    const bool at_pin = walk_to(floor, -1.15, -148.10, 8.0, 0.12);
    (void)floor.set_facing(-1.0, 0.0);
    watch(0.4);
    bool took = at_pin && floor.snapshot().carry_target_entity_id == Simulation::kShutterPinEntityId &&
                floor.request_pick_up();
    watch(0.4);
    took = took && floor.snapshot().carrying_entity_id == Simulation::kShutterPinEntityId;
    bool slid = false;
    if (took) {
        for (int step = 0; step < 6 * 90 && !slid; ++step) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
            slid = shutter_x() > x0 + 1.30;
        }
    }
    if (!slid) {
        const auto p = floor.snapshot();
        const auto w = floor.kit_body_position(floor.kit_body_index(Simulation::kShutterWeightEntityId));
        const auto pin = floor.kit_body_position(floor.kit_body_index(Simulation::kShutterPinEntityId));
        std::cout << "sheet stayed x0=" << x0 << " x=" << shutter_x() << " weight " << w.x << " " << w.y << " "
                  << w.z << " pin " << pin.x << " " << pin.y << " " << pin.z << " player " << p.player_position.x
                  << " " << p.player_position.y << " " << p.player_position.z << " target=" << p.carry_target_entity_id
                  << " carrying=" << p.carrying_entity_id << "\n";
    }
    require(slid, "the sheet must slide off the ladder once the pin is lifted");
    (void)floor.request_set_down();
    watch(0.3);
    for (int step = 0; step < 90; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
    }
    if (!walk_to(floor, 2.15, -148.00, 8.0, 0.12)) {
        const auto p = floor.snapshot();
        std::cout << "no ladder " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " grounded=" << p.player_grounded << " deaths=" << p.death_count << "\n";
    }
    require(walk_to(floor, 2.15, -148.00, 2.0, 0.12), "the player must reach the ladder");
    (void)floor.set_facing(0.0, 1.0);
    watch(0.35);
    if (!floor.snapshot().grip_available) {
        const auto p = floor.snapshot();
        std::cout << "no hold " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " sheet_x=" << shutter_x() << "\n";
    }
    require(floor.snapshot().grip_available, "the ladder must be a hold once the sheet is aside");
    require(floor.request_traversal(), "taking the ladder must be accepted");
    watch(0.2);
    require(is_climbing(floor.snapshot()), "the player must be on the ladder");
    require(hold_stick(floor, 0.0, 1.0, 0.0, 1.0, 30.0,
                       [](const scraperx::sim::Snapshot &state) { return standing_above(state, 652.40); }),
            "the player must climb onto the next floor");
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        const auto top = floor.snapshot();
        std::cout << "snap " << g_path_watch.worst << " at " << g_path_watch.worst_at.x << " "
                  << g_path_watch.worst_at.y << " " << g_path_watch.worst_at.z
                  << " trav=" << g_path_watch.worst_traversal << " now " << top.player_position.x << " "
                  << top.player_position.y << " " << top.player_position.z << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the sheet and the ladder must not snap the body more than 0.15 m in one tick");
    const auto top = floor.snapshot();
    require(top.death_count == 0 && top.player_grounded && top.player_position.y > 652.50 &&
                top.player_position.x > 1.1 && top.player_position.x < 3.4 && top.player_position.z > -147.3 &&
                top.player_position.z < -145.3 && top.support_entity_id == Simulation::kCraneFrameEntityId,
            "the player must be standing on the next floor, still inside the building");
    std::cout << "PASS scraperx_sim shutter: floor_y=" << top.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The ramp above the ladder floor. It must stay put until its pin is lifted.
// Then it slides down, and the same player walks it.
bool climb_ramp(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const auto ramp_x = [&]() {
        return floor.kit_body_position(floor.kit_body_index(Simulation::kRampEntityId)).x;
    };
    const double x0 = ramp_x();
    auto watch = [&](const double seconds) {
        const int ticks = static_cast<int>(seconds * 90.0 + 0.5);
        for (int step = 0; step < ticks; ++step) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    watch(1.2);
    require(std::abs(ramp_x() - x0) < 0.08, "the ramp must stay put until the pin is lifted");
    require(walk_to(floor, 2.15, -146.40, 8.0, 0.16), "the player must reach the ramp pin");
    (void)floor.set_facing(-1.0, 0.0);
    watch(0.4);
    require(floor.snapshot().carry_target_entity_id == Simulation::kRampPinEntityId,
            "the pin must be the thing in reach");
    require(floor.request_pick_up(), "picking up the pin must be accepted");
    watch(0.35);
    require(floor.snapshot().carrying_entity_id == Simulation::kRampPinEntityId, "the pin must be in the hands");
    require(walk_to(floor, 2.40, -146.40, 6.0, 0.20), "the player must carry the pin clear");
    require(floor.request_set_down(), "setting the pin down must be accepted");
    watch(0.4);
    bool slid = false;
    for (int step = 0; step < 10 * 90 && !slid; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        slid = ramp_x() < x0 - 1.40;
    }
    if (!slid) {
        const auto ramp = floor.kit_body_position(floor.kit_body_index(Simulation::kRampEntityId));
        const auto pin = floor.kit_body_position(floor.kit_body_index(Simulation::kRampPinEntityId));
        const auto p = floor.snapshot().player_position;
        std::cout << "ramp stayed x0=" << x0 << " x=" << ramp.x << " y=" << ramp.y << " pin " << pin.x << " " << pin.y
                  << " " << pin.z << " player " << p.x << " " << p.y << " " << p.z << "\n";
    }
    require(slid, "the ramp must slide down once the pin is lifted");
    watch(1.2);
    auto walk_slow = [&](const double x, const double z, const double seconds, const double tolerance) {
        const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto state = floor.snapshot();
            if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance &&
                state.player_grounded) {
                (void)floor.set_move_input(0.0, 0.0);
                return true;
            }
            steer_toward(floor, x, z, 0.38);
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
        (void)floor.set_move_input(0.0, 0.0);
        return false;
    };
    require(walk_slow(4.6, -146.20, 14.0, 0.40), "the player must step onto the ramp");
    require(walk_slow(8.4, -146.20, 18.0, 0.40), "the player must walk up the ramp");
    require(walk_slow(10.4, -146.20, 12.0, 0.45), "the player must step off onto the next floor");
    bool up = false;
    for (int step = 0; step < 3 * 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        if (state.death_count == 0 && state.player_grounded && state.player_position.y > 655.3 &&
            state.player_position.x > 9.3 && state.player_position.x < 11.4 &&
            state.player_position.z > -147.2 && state.player_position.z < -145.2 &&
            state.support_entity_id == Simulation::kCraneFrameEntityId) {
            up = true;
            break;
        }
    }
    if (!up) {
        const auto p = floor.snapshot();
        std::cout << "off the ramp at " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << "\n";
    }
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15, "the ramp must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the next floor, still inside the building");
    const auto landed = floor.snapshot();
    std::cout << "PASS scraperx_sim ramp: floor_y=" << landed.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The gate in front of the ladder on the ramp's floor. A pin holds it shut.
// Lift the pin, pull the gate off the ladder, and the same player climbs.
bool climb_gate(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    auto watch = [&](const double seconds) {
        const int ticks = static_cast<int>(seconds * 90.0 + 0.5);
        for (int step = 0; step < ticks; ++step) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    require(walk_to(floor, 10.20, -146.55, 6.0, 0.16), "the player must reach the gate");
    (void)floor.set_facing(0.0, 1.0);
    watch(0.4);
    require(!floor.snapshot().grip_available, "the gate must block the ladder until it is pulled aside");
    require(walk_to(floor, 10.40, -146.90, 6.0, 0.16), "the player must reach the gate pin");
    (void)floor.set_facing(1.0, 0.0);
    watch(0.4);
    require(floor.snapshot().carry_target_entity_id == Simulation::kGateLatchEntityId,
            "the pin must be the thing in reach");
    require(floor.request_pick_up(), "picking up the pin must be accepted");
    watch(0.35);
    require(floor.snapshot().carrying_entity_id == Simulation::kGateLatchEntityId, "the pin must be in the hands");
    require(walk_to(floor, 9.20, -146.90, 6.0, 0.18), "the player must carry the pin clear");
    require(floor.request_set_down(), "setting the pin down must be accepted");
    watch(0.5);
    const double gate_z0 = floor.kit_body_position(floor.kit_body_index(Simulation::kGateEntityId)).z;
    require(walk_to(floor, 10.40, -146.45, 6.0, 0.08), "the player must reach the gate handle");
    (void)floor.set_facing(0.0, 1.0);
    for (int step = 0; step < 45; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
    }
    require(floor.snapshot().carry_target_entity_id == Simulation::kGateEntityId,
            "the gate must be the thing in reach");
    require(floor.request_pick_up(), "taking the gate must be accepted");
    for (int step = 0; step < 27; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
    }
    require(floor.snapshot().carrying_entity_id == Simulation::kGateEntityId, "the gate must be in the hands");
    bool swung = false;
    for (int step = 0; step < 4 * 90 && !swung; ++step) {
        (void)floor.set_move_input(0.0, -0.42);
        (void)floor.set_facing(0.0, 1.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        swung = floor.kit_body_position(floor.kit_body_index(Simulation::kGateEntityId)).z < gate_z0 - 0.35;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (floor.snapshot().carrying_entity_id != 0) {
        (void)floor.request_set_down();
    }
    for (int step = 0; step < 27; ++step) {
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
    }
    if (!swung) {
        const auto gate = floor.kit_body_position(floor.kit_body_index(Simulation::kGateEntityId));
        const auto p = floor.snapshot().player_position;
        std::cout << "gate stayed z0=" << gate_z0 << " z=" << gate.z << " x=" << gate.x << " player " << p.x << " "
                  << p.y << " " << p.z << " target=" << floor.snapshot().carry_target_entity_id << "\n";
    }
    require(swung, "the gate must swing off the ladder once the latch is open");
    watch(0.4);
    if (floor.snapshot().player_position.y < 655.5) {
        const auto p = floor.snapshot().player_position;
        const auto gate = floor.kit_body_position(floor.kit_body_index(Simulation::kGateEntityId));
        std::cout << "fell pulling the gate player " << p.x << " " << p.y << " " << p.z << " gate " << gate.x << " "
                  << gate.y << " " << gate.z << " deaths=" << floor.snapshot().death_count << "\n";
    }
    require(floor.snapshot().player_position.y > 655.5, "pulling the gate must not knock the player off the floor");
    require(walk_to(floor, 10.20, -146.15, 6.0, 0.16), "the player must get back to the ladder");
    (void)floor.set_facing(0.0, 1.0);
    watch(0.4);
    if (!floor.snapshot().grip_available) {
        const auto p = floor.snapshot();
        const auto gate = floor.kit_body_position(floor.kit_body_index(Simulation::kGateEntityId));
        std::cout << "no ladder hold " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " gate " << gate.x << " " << gate.y << " " << gate.z << "\n";
    }
    require(floor.snapshot().grip_available, "the ladder must be a hold once the gate is aside");
    require(floor.request_traversal(), "taking the ladder must be accepted");
    watch(0.2);
    require(is_climbing(floor.snapshot()), "the player must be on the ladder");
    const bool climbed = hold_stick(floor, 0.0, 1.0, 0.0, 1.0, 40.0, [](const scraperx::sim::Snapshot &state) {
        return standing_above(state, 672.40);
    });
    if (!climbed) {
        const auto p = floor.snapshot();
        std::cout << "stopped on the ladder " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded
                  << " trav=" << static_cast<int>(p.traversal_state) << " deaths=" << p.death_count << "\n";
    }
    require(climbed, "the player must climb onto the next floor");
    bool up = false;
    for (int step = 0; step < 3 * 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        if (state.death_count == 0 && state.player_grounded && state.player_position.y > 672.40 &&
            state.player_position.x > 9.2 && state.player_position.x < 11.4 && state.player_position.z > -146.3 &&
            state.player_position.z < -143.0 && state.support_entity_id == Simulation::kCraneFrameEntityId) {
            up = true;
            break;
        }
    }
    if (!up) {
        const auto p = floor.snapshot();
        std::cout << "off the high ladder at " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << "\n";
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        const auto top = floor.snapshot();
        std::cout << "snap " << g_path_watch.worst << " at " << g_path_watch.worst_at.x << " "
                  << g_path_watch.worst_at.y << " " << g_path_watch.worst_at.z
                  << " trav=" << g_path_watch.worst_traversal << " now " << top.player_position.x << " "
                  << top.player_position.y << " " << top.player_position.z << " grounded=" << top.player_grounded
                  << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the gate and the ladder must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the next floor, still inside the building");
    const auto landed = floor.snapshot();
    std::cout << "PASS scraperx_sim gate: floor_y=" << landed.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The cart on the 45° rail above the ladder floor. It stays put until its
// pin is lifted. The same player is on it when the weight drops, and the
// rope drags the cart up the rail.
bool climb_cart(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const auto cart_x = [&]() {
        return floor.kit_body_position(floor.kit_body_index(Simulation::kCartHaulEntityId)).x;
    };
    const double x0 = cart_x();
    auto watch = [&](const double seconds) {
        const int ticks = static_cast<int>(seconds * 90.0 + 0.5);
        for (int step = 0; step < ticks; ++step) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    watch(1.0);
    require(std::abs(cart_x() - x0) < 0.08, "the cart must stay put until the pin is lifted");
    require(walk_to(floor, 9.05, -144.55, 8.0, 0.20), "the player must leave the ladder floor");
    require(walk_to(floor, 7.55, -149.20, 8.0, 0.25), "the player must reach the cart");
    require(walk_to(floor, 7.30, -150.00, 6.0, 0.30), "the player must step onto the cart");
    (void)floor.set_facing(1.0, 0.2);
    watch(0.4);
    if (floor.snapshot().carry_target_entity_id != Simulation::kCartHaulPinEntityId) {
        const auto p = floor.snapshot();
        std::cout << "no cart pin " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " target=" << p.carry_target_entity_id
                  << " grounded=" << p.player_grounded << " y0cart=" << x0 << "\n";
    }
    require(floor.snapshot().carry_target_entity_id == Simulation::kCartHaulPinEntityId,
            "the pin must be the thing in reach from the cart");
    require(floor.request_pick_up(), "picking up the pin must be accepted");
    watch(0.3);
    require(floor.snapshot().carrying_entity_id == Simulation::kCartHaulPinEntityId, "the pin must be in the hands");
    bool rode = false;
    double peak_y = floor.snapshot().player_position.y;
    for (int step = 0; step < 20 * 90 && !rode; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        peak_y = std::max(peak_y, floor.snapshot().player_position.y);
        rode = cart_x() < x0 - 9.0;
    }
    if (!rode || peak_y < 680.0) {
        const auto cart = floor.kit_body_position(floor.kit_body_index(Simulation::kCartHaulEntityId));
        const auto weight = floor.kit_body_position(floor.kit_body_index(Simulation::kCartHaulWeightEntityId));
        const auto p = floor.snapshot();
        std::cout << "cart ride x0=" << x0 << " x=" << cart.x << " y=" << cart.y << " weight_y=" << weight.y
                  << " player " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " peak_y=" << peak_y << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << "\n";
    }
    require(rode && peak_y > 680.0, "the cart must be dragged up the rail once the pin is lifted");
    watch(1.5);
    require(floor.request_set_down(), "setting the pin down must be accepted");
    watch(0.4);
    auto walk_slow = [&](const double x, const double z, const double seconds, const double tolerance) {
        const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto state = floor.snapshot();
            if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance &&
                state.player_grounded) {
                (void)floor.set_move_input(0.0, 0.0);
                return true;
            }
            steer_toward(floor, x, z, 0.35);
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
        (void)floor.set_move_input(0.0, 0.0);
        return false;
    };
    const bool off = walk_slow(-6.4, -150.0, 10.0, 0.45);
    if (!off) {
        const auto p = floor.snapshot();
        std::cout << "still on the cart " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << "\n";
    }
    require(off, "the player must step off the cart onto the upper floor");
    bool up = false;
    for (int step = 0; step < 3 * 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        if (state.death_count == 0 && state.player_grounded && state.player_position.y > 683.2 &&
            state.player_position.x < -4.5 && state.player_position.x > -9.0 &&
            state.player_position.z > -151.6 && state.player_position.z < -148.3 &&
            state.support_entity_id == Simulation::kCraneFrameEntityId) {
            up = true;
            break;
        }
    }
    if (!up) {
        const auto p = floor.snapshot();
        std::cout << "off the cart at " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << "\n";
    }
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15, "the cart must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the floor the cart climbs to, still inside the building");
    const auto landed = floor.snapshot();
    std::cout << "PASS scraperx_sim cart: floor_y=" << landed.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The cooling-tower louvers north of the cart's landing. Same body, up the
// slats, onto the shell's top. No pin and no ride.
bool climb_slats(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool up = climb_wet_hold(floor, -6.20, -148.15, 0.0, 1.0, false, 708.2);
    if (!up) {
        const auto p = floor.snapshot();
        std::cout << "off the slats at " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " grip=" << p.grip_available << " trav=" << static_cast<int>(p.traversal_state)
                  << " deaths=" << p.death_count << "\n";
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "slat snap " << g_path_watch.worst << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the slats must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the cooling tower, still inside the building");
    const auto landed = floor.snapshot();
    require(landed.player_position.x > -9.0 && landed.player_position.x < -4.0 &&
                landed.player_position.z > -147.3 && landed.player_position.z < -144.5 &&
                landed.support_entity_id == Simulation::kCraneFrameEntityId && landed.death_count == 0,
            "the cooling tower floor must be the one under the player");
    std::cout << "PASS scraperx_sim slats: floor_y=" << landed.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The duct folded up east of the cooling-tower floor. The handle on this
// floor turns a light lever. The duct then drops onto the floor, and the
// same player walks up it. No pin.
bool climb_duct(scraperx::sim::Simulation &floor) {
    using scraperx::sim::Simulation;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const auto duct_y = [&]() {
        return floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kDuctEntityId)).y;
    };
    auto watch = [&](const double seconds) {
        const int ticks = static_cast<int>(seconds * 90.0 + 0.5);
        for (int step = 0; step < ticks; ++step) {
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
    };
    const double y0 = duct_y();
    if (y0 < 718.0) {
        const auto duct = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kDuctEntityId));
        std::cout << "duct already down y=" << duct.y << " x=" << duct.x << " z=" << duct.z << "\n";
    }
    require(y0 > 718.0, "the duct must still be folded up on the cooling tower");
    require(walk_to(floor, -6.55, -145.35, 8.0, 0.12), "the player must reach the duct's lever");
    (void)floor.set_facing(-1.0, 0.0);
    watch(0.45);
    if (floor.snapshot().carry_target_entity_id != Simulation::kDuctLeverEntityId) {
        const auto p = floor.snapshot();
        const auto lever = floor.kit_body_position(floor.kit_body_index(Simulation::kDuctLeverEntityId));
        std::cout << "no duct lever player " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " lever " << lever.x << " " << lever.y << " " << lever.z
                  << " target=" << p.carry_target_entity_id << " grounded=" << p.player_grounded << "\n";
    }
    require(floor.snapshot().carry_target_entity_id == Simulation::kDuctLeverEntityId,
            "the duct lever must be the thing in reach");
    require(floor.request_pick_up(), "taking the duct lever must be accepted");
    watch(0.3);
    require(floor.snapshot().carrying_entity_id == Simulation::kDuctLeverEntityId,
            "the duct lever must be in the hands");
    bool dropped = false;
    for (int step = 0; step < 5 * 90 && !dropped; ++step) {
        const auto here = floor.snapshot().player_position;
        if (here.x > -5.70) {
            (void)floor.set_move_input(0.0, 0.0);
        } else {
            (void)floor.set_move_input(0.22, 0.0);
        }
        (void)floor.set_facing(-1.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        // The catch lets go as soon as the bar is over. Stop there. Waiting
        // for the duct to finish its swing walks the player off the floor.
        dropped = duct_y() < 718.0;
    }
    (void)floor.set_move_input(0.0, 0.0);
    if (floor.snapshot().carrying_entity_id != 0) {
        (void)floor.request_set_down();
    }
    watch(0.4);
    if (!dropped || floor.snapshot().player_position.y < 707.4) {
        const auto duct = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kDuctEntityId));
        const auto lever = floor.kit_body_position(floor.kit_body_index(Simulation::kDuctLeverEntityId));
        const auto q = floor.kit_body_rotation(floor.kit_body_index(Simulation::kDuctLeverEntityId));
        const auto p = floor.snapshot();
        std::cout << "duct pull y0=" << y0 << " y=" << duct.y << " x=" << duct.x << " lever " << lever.x << " "
                  << lever.y << " " << lever.z << " quat " << q.w << " " << q.x << " " << q.y << " " << q.z
                  << " player " << p.player_position.x << " " << p.player_position.y << " " << p.player_position.z
                  << " carrying=" << p.carrying_entity_id << " grounded=" << p.player_grounded
                  << " deaths=" << p.death_count << "\n";
    }
    require(floor.snapshot().death_count == 0, "pulling the duct lever must not kill the player");
    require(floor.snapshot().player_position.y > 707.4, "pulling the duct lever must not knock the player off");
    require(dropped, "the duct must swing down once the lever is pulled");
    bool settled = false;
    for (int step = 0; step < 8 * 90 && !settled; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto vel = floor.kit_body_velocity(floor.kit_body_index(Simulation::kDuctEntityId));
        settled = duct_y() < 712.6 && duct_y() > 709.0 && std::hypot(vel.x, vel.y) < 0.25;
    }
    if (!settled) {
        const auto duct = floor.kit_body_center_of_mass(floor.kit_body_index(Simulation::kDuctEntityId));
        const auto vel = floor.kit_body_velocity(floor.kit_body_index(Simulation::kDuctEntityId));
        std::cout << "duct did not settle y=" << duct.y << " x=" << duct.x << " z=" << duct.z << " v=" << vel.x << " "
                  << vel.y << " " << vel.z << "\n";
    }
    require(settled, "the duct must come to rest on the floor");
    auto walk_slow = [&](const double x, const double z, const double seconds, const double tolerance) {
        const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto state = floor.snapshot();
            if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance &&
                state.player_grounded && state.player_position.y > 707.4) {
                (void)floor.set_move_input(0.0, 0.0);
                return true;
            }
            steer_toward(floor, x, z, 0.38);
            (void)floor.advance_frame(Simulation::kFixedStepSeconds);
            observe_path(floor, true);
        }
        (void)floor.set_move_input(0.0, 0.0);
        return false;
    };
    const bool on = walk_slow(-3.6, -146.00, 10.0, 0.40);
    if (!on) {
        const auto p = floor.snapshot();
        std::cout << "not on the duct " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << "\n";
    }
    require(on, "the player must step onto the lowered duct");
    const bool mid = walk_slow(2.2, -146.00, 16.0, 0.45);
    if (!mid) {
        const auto p = floor.snapshot();
        std::cout << "stuck on the duct " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " yduct=" << duct_y() << "\n";
    }
    require(mid, "the player must walk up the duct");
    const bool off = walk_slow(7.50, -146.00, 12.0, 0.45);
    if (!off) {
        const auto p = floor.snapshot();
        std::cout << "not on the duct deck " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << "\n";
    }
    require(off, "the player must step off the duct onto the next floor");
    bool up = false;
    for (int step = 0; step < 3 * 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        if (state.death_count == 0 && state.player_grounded && state.player_position.y > 714.6 &&
            state.player_position.x > 6.7 && state.player_position.x < 8.7 &&
            state.player_position.z > -147.1 && state.player_position.z < -144.9 &&
            state.support_entity_id == Simulation::kCraneFrameEntityId) {
            up = true;
            break;
        }
    }
    if (!up) {
        const auto p = floor.snapshot();
        std::cout << "off the duct at " << p.player_position.x << " " << p.player_position.y << " "
                  << p.player_position.z << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << " yduct=" << duct_y() << "\n";
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "duct snap " << g_path_watch.worst << " at " << g_path_watch.worst_at.x << " "
                  << g_path_watch.worst_at.y << " " << g_path_watch.worst_at.z << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the duct must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the floor the duct climbs to, still inside the building");
    const auto landed = floor.snapshot();
    std::cout << "PASS scraperx_sim duct: floor_y=" << landed.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


// The fan casing south of the duct deck. Up the outside, south along the
// gallery, up again, back north, up onto the top gallery. No pin.
bool climb_casing(scraperx::sim::Simulation &floor) {
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
    if (!(walk_to(floor, 10.10, -149.10, 8.0, 0.20) && walk_to(floor, 10.10, -151.30, 6.0, 0.20))) {
        say("casing bridge");
        return false;
    }
    if (!climb_wet_hold(floor, 8.70, -150.85, 0.0, 1.0, false, 729.4)) {
        say("casing first ladder");
        return false;
    }
    if (!(walk_to(floor, 10.10, -148.70, 6.0, 0.20) && walk_to(floor, 10.10, -154.60, 8.0, 0.20) &&
          walk_to(floor, 8.70, -154.70, 6.0, 0.15))) {
        say("casing south gallery");
        return false;
    }
    if (!climb_wet_hold(floor, 8.70, -154.85, 0.0, -1.0, false, 744.4)) {
        say("casing second ladder");
        return false;
    }
    if (!(walk_to(floor, 10.10, -156.60, 6.0, 0.20) && walk_to(floor, 10.10, -153.10, 8.0, 0.20) &&
          walk_to(floor, 8.70, -153.10, 6.0, 0.15))) {
        say("casing north gallery");
        return false;
    }
    if (!climb_wet_hold(floor, 8.70, -153.05, 0.0, 1.0, false, 759.4)) {
        say("casing third ladder");
        return false;
    }
    bool up = false;
    for (int step = 0; step < 3 * 90; ++step) {
        (void)floor.set_move_input(0.0, 0.0);
        (void)floor.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(floor, true);
        const auto state = floor.snapshot();
        if (state.death_count == 0 && state.player_grounded && state.player_position.y > 759.6 &&
            state.player_position.x > 8.2 && state.player_position.x < 10.6 &&
            state.player_position.z > -151.9 && state.player_position.z < -150.4 &&
            state.support_entity_id == Simulation::kCraneFrameEntityId) {
            up = true;
            break;
        }
    }
    if (!up) {
        say("casing deck");
    }
    g_path_watch.armed = false;
    if (g_path_watch.worst > 0.15) {
        std::cout << "casing snap " << g_path_watch.worst << "\n";
    }
    require(g_path_watch.worst <= 0.15, "the casing must not snap the body more than 0.15 m in one tick");
    require(up, "the player must be standing on the casing gallery, still inside the building");
    const auto landed = floor.snapshot();
    std::cout << "PASS scraperx_sim casing: floor_y=" << landed.player_position.y
              << " worst_tick_m=" << g_path_watch.worst << "\n";
    return true;
}


