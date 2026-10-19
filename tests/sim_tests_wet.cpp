// AS-007, Wet Isolation (220 -> 340 m), and its handling.

#include "sim_tests.hpp"

// D's spool, from where it lies on the 220 ring, carried north and set down
// between the guides across its gap.
bool seat_wet_spool(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 9.5, -130.6, 8.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kWetDSpoolEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWetDSpoolEntityId) {
        return false;
    }
    for (std::uint32_t tick = 0; tick < 90 * 6; ++tick) {
        if (simulation.snapshot().player_position.z >= -128.25) {
            break;
        }
        (void)simulation.set_move_input(0.0, 0.35);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    (void)simulation.advance_frame(0.6);
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.5);
    return simulation.wet_state().d_pipe_whole;
}

// From the 220 ring to D's platform by its walkway, east of the fill bar's
// handle hanging over it, then throw the bar.
bool throw_wet_fill(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 13.4, -129.6, 8.0) || !walk_to(simulation, 13.4, -132.7, 4.0)) {
        return false;
    }
    return pull_facing(simulation, 12.3, -132.7, 0.0, 1.0, Simulation::kWetDValveHandleEntityId,
                       0.5, 4.0, [&](const scraperx::sim::Snapshot &) {
                           return simulation.wet_state().d_valve_angle > 1.8;
                       });
}

// Inside E's cab, east of its door (it opens into the cab): wait for the
// leaf to stop swinging, take its handle and draw it east until the latch
// drops.
bool shut_wet_door(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 13.05, -136.4, 6.0, 0.08)) {
        return false;
    }
    const auto door_index = simulation.kit_body_index(Simulation::kWetEDoorEntityId);
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        const auto v = simulation.kit_body_velocity(door_index);
        if (std::hypot(v.x, v.z) < 0.2) {
            break;
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto door = simulation.kit_body_center_of_mass(door_index);
    const auto here = simulation.snapshot().player_position;
    const double dx = door.x - here.x;
    const double dz = door.z - here.z;
    const double length = std::hypot(dx, dz);
    (void)simulation.set_facing(dx / length, dz / length);
    (void)simulation.advance_frame(0.6);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kWetEDoorEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    bool latched = false;
    for (std::uint32_t tick = 0; tick < 90 * 6 && !latched; ++tick) {
        (void)simulation.set_move_input(0.4, 0.0);
        (void)simulation.set_facing(dx / length, dz / length);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        latched = simulation.wet_state().e_door_latched;
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    (void)simulation.advance_frame(0.3);
    return latched;
}

// The chiller's trip handle hangs inside the cab's east wall.
bool pull_wet_trip(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, 13.4, -136.65, 1.0, 0.0, Simulation::kWetEHandleEntityId, 0.5,
                       4.0, [&](const scraperx::sim::Snapshot &) {
                           return !simulation.wet_state().e_catch_latched;
                       });
}

// On F's platform: the hose off the deck and onto the ram's inlet on the
// platform's west rail.
bool couple_wet_hose(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 12.6, -139.95, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-0.3, -0.95);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kWetFHoseEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWetFHoseEntityId ||
        !walk_to(simulation, 12.4, -139.55, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-0.6, 0.8);
    (void)simulation.advance_frame(1.2);
    const auto at_inlet = simulation.snapshot();
    if (at_inlet.rig_action != 1 ||
        at_inlet.rig_target_entity_id != Simulation::kWetFPlatformEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.wet_state().f_hose_coupled;
}

// F's stop valve's handle hangs off the platform's south rail.
bool pull_wet_stop_valve(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, 12.25, -140.7, 0.0, -1.0, Simulation::kWetFHandleEntityId, 0.5,
                       4.0, [&](const scraperx::sim::Snapshot &) {
                           return !simulation.wet_state().f_catch_latched;
                       });
}

bool on_support(const scraperx::sim::Snapshot &state, const std::uint64_t entity) {
    return state.player_grounded && state.support_entity_id == entity;
}

void run_wet_isolation() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- D: no spool, no water -----------------------------------------------
    Simulation d_dry(InitialSpawn::Ring220North);
    (void)d_dry.advance_frame(1.0);
    require(!d_dry.wet_state().d_pipe_whole, "as found, D's spool lies off its gap");
    require(throw_wet_fill(d_dry), "the rider must throw D's fill bar from the platform");
    (void)d_dry.advance_frame(10.0);
    require(d_dry.wet_state().d_platform_travel < 0.02 && d_dry.wet_state().d_tube_kg <= 0.0,
            "with the spool on the deck no water runs and D's platform stays down");

    // ---- D: the spool seated, the ride -----------------------------------------
    Simulation d_ride(InitialSpawn::Ring220North);
    (void)d_ride.advance_frame(1.0);
    require(seat_wet_spool(d_ride), "the rider must carry D's spool into its gap");
    const auto d_before = d_ride.wet_state();
    require(throw_wet_fill(d_ride), "the rider must throw D's fill bar with the spool seated");
    const double d_rider_y0 = d_ride.snapshot().player_position.y;
    const bool d_arrived = wait_for(d_ride, 120.0, [&](const scraperx::sim::Snapshot &) {
        return d_ride.wet_state().d_platform_travel >= 35.95;
    });
    (void)d_ride.advance_frame(1.0);
    const auto d_after = d_ride.wet_state();
    const auto d_top = d_ride.snapshot();
    require(d_arrived, "D's float must carry the platform to its stop at 256.25");
    require(on_support(d_top, Simulation::kWetDPlatformEntityId),
            "the rider must ride D's platform all the way");
    // Released: the water's fall, tank to tube, from each pool's centre of
    // mass (the float's displacement moves the tube's by centimetres).
    const auto pool_pe = [&](const double kg, const double floor, const double level) {
        return kg * kG * 0.5 * (floor + level);
    };
    const double d_gain = kRiderMassKg * kG * (d_top.player_position.y - d_rider_y0);
    const double d_released =
        pool_pe(d_before.d_tank_kg, 227.0, d_before.d_tank_level) +
        pool_pe(d_before.d_tube_kg, 181.0, d_before.d_tube_level) -
        pool_pe(d_after.d_tank_kg, 227.0, d_after.d_tank_level) -
        pool_pe(d_after.d_tube_kg, 181.0, d_after.d_tube_level) -
        (d_after.drained_kg - d_before.drained_kg) * kG * 219.0;
    require(d_gain > 0.0 && d_gain <= d_released,
            "D's rider must never gain more than the water released");
    std::cout << "PASS scraperx_sim AS-007 D: rider_y=" << d_top.player_position.y
              << " tank_level=" << d_after.d_tank_level << " tube_level=" << d_after.d_tube_level
              << " gain_J=" << d_gain << " released_J=" << d_released << '\n';

    // ---- E: door open, the air leaves by it ---------------------------------------
    Simulation e_open(InitialSpawn::WetECab);
    (void)e_open.advance_frame(1.0);
    require(!e_open.wet_state().e_door_latched && e_open.wet_state().e_catch_latched,
            "as found, E's door stands open and the chiller hangs in its catch");
    require(pull_wet_trip(e_open), "the rider must trip the chiller from the cab");
    (void)e_open.advance_frame(15.0);
    require(e_open.wet_state().e_cab_travel < 0.05 && e_open.wet_state().e_chiller_travel < -10.0,
            "with the door open the chiller falls and the cab stays on its stop");

    // ---- E: door shut, the ride ------------------------------------------------------
    Simulation e_ride(InitialSpawn::WetECab);
    (void)e_ride.advance_frame(1.0);
    require(shut_wet_door(e_ride), "the rider must swing E's door shut until it latches");
    const double e_rider_y0 = e_ride.snapshot().player_position.y;
    const double e_chiller_y0 = kit_y(e_ride, Simulation::kWetEChillerEntityId);
    const double e_bucket_y0 = kit_y(e_ride, Simulation::kWetEBucketEntityId);
    require(pull_wet_trip(e_ride), "the rider must trip the chiller with the door shut");
    const bool e_arrived = wait_for(e_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return e_ride.wet_state().e_cab_travel >= 41.95;
    });
    (void)e_ride.advance_frame(1.0);
    const auto e_top = e_ride.snapshot();
    require(e_arrived, "the chiller's air must lift E's cab to its stop at 298.25");
    require(on_support(e_top, Simulation::kWetECabEntityId),
            "the rider must ride E's cab all the way");
    const double e_gain = kRiderMassKg * kG * (e_top.player_position.y - e_rider_y0);
    const double e_released =
        5000.0 * kG * (e_chiller_y0 - kit_y(e_ride, Simulation::kWetEChillerEntityId)) -
        300.0 * kG * (kit_y(e_ride, Simulation::kWetEBucketEntityId) - e_bucket_y0);
    require(e_gain > 0.0 && e_gain <= e_released,
            "E's rider must never gain more than the chiller released");
    std::cout << "PASS scraperx_sim AS-007 E: rider_y=" << e_top.player_position.y
              << " cab_pa=" << e_ride.wet_state().e_cab_pa << " gain_J=" << e_gain
              << " released_J=" << e_released << '\n';

    // ---- F: hose free, the accumulator dumps -----------------------------------------
    Simulation f_free(InitialSpawn::WetFPlatform);
    (void)f_free.advance_frame(1.0);
    require(!f_free.wet_state().f_hose_coupled && f_free.wet_state().f_catch_latched,
            "as found, F's hose lies free and the accumulator stands in its catch");
    require(pull_wet_stop_valve(f_free), "the rider must throw F's stop valve");
    (void)f_free.advance_frame(40.0);
    require(f_free.wet_state().f_platform_travel < 0.05 &&
                f_free.wet_state().f_accumulator_travel < -2.9,
            "with the hose free the accumulator dumps and F's platform stays");

    // ---- F: hose coupled, the ride ----------------------------------------------------
    Simulation f_ride(InitialSpawn::WetFPlatform);
    (void)f_ride.advance_frame(1.0);
    require(couple_wet_hose(f_ride), "the rider must couple F's hose to the ram's inlet");
    const double f_rider_y0 = f_ride.snapshot().player_position.y;
    const double f_acc_y0 = kit_y(f_ride, Simulation::kWetFAccumulatorEntityId);
    require(pull_wet_stop_valve(f_ride), "the rider must throw F's stop valve, coupled");
    const bool f_arrived = wait_for(f_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return f_ride.wet_state().f_platform_travel >= 41.95;
    });
    (void)f_ride.advance_frame(1.0);
    const auto f_top = f_ride.snapshot();
    require(f_arrived, "the accumulator must lift F's platform to TP-340");
    require(on_support(f_top, Simulation::kWetFPlatformEntityId),
            "the rider must ride F's platform all the way");
    const double f_gain = kRiderMassKg * kG * (f_top.player_position.y - f_rider_y0);
    const double f_released =
        20000.0 * kG * (f_acc_y0 - kit_y(f_ride, Simulation::kWetFAccumulatorEntityId));
    require(f_gain > 0.0 && f_gain <= f_released,
            "F's rider must never gain more than the accumulator released");
    std::cout << "PASS scraperx_sim AS-007 F: rider_y=" << f_top.player_position.y
              << " gain_J=" << f_gain << " released_J=" << f_released << '\n';
}

// The band in one run on player inputs, from the 220 ring to standing on
// TP-340, then the header's dump re-arms it.
void run_wet_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::Ring220North);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(seat_wet_spool(band), "band: the spool into its gap");
    require(throw_wet_fill(band), "band: D's fill bar thrown from the platform");
    require(wait_for(band, 120.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().d_platform_travel >= 35.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetDPlatformEntityId),
            "band: D carries the rider to 256.25");
    require(walk_to(band, 13.2, -134.5, 6.0) && walk_to(band, 13.2, -135.8, 6.0),
            "band: from D's platform through E's door into the cab");
    require(shut_wet_door(band), "band: E's door shut behind the rider");
    require(pull_wet_trip(band), "band: E's chiller tripped");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().e_cab_travel >= 41.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetECabEntityId),
            "band: E carries the rider to 298.25");
    require(walk_to(band, 13.4, -137.2, 4.0) && walk_to(band, 13.4, -139.9, 6.0),
            "band: from E's cab over the rim onto F's platform");
    require(couple_wet_hose(band), "band: F's hose coupled");
    require(pull_wet_stop_valve(band), "band: F's stop valve thrown");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().f_platform_travel >= 41.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetFPlatformEntityId),
            "band: F carries the rider to TP-340");
    require(walk_to(band, 13.0, -137.2, 6.0), "band: off F's platform onto TP-340");
    (void)band.advance_frame(0.5);
    const auto on_plate = band.snapshot();
    require(on_plate.player_grounded && on_plate.player_position.y > 340.25 &&
                on_plate.support_entity_id == Simulation::kWetFrameEntityId,
            "band: the rider stands on TP-340");
    const double to_plate = on_plate.simulation_time_seconds - start;

    // The cascade: the header's dump thrown over.
    require(pull_facing(band, 11.6, -137.3, -1.0, 0.0, Simulation::kWetHeaderHandleEntityId, 0.5,
                        4.0,
                        [&](const scraperx::sim::Snapshot &) {
                            return band.wet_state().dump_angle > 1.8;
                        }),
            "band: the header's dump thrown");
    const bool rearmed = wait_for(band, 150.0, [&](const scraperx::sim::Snapshot &) {
        const auto w = band.wet_state();
        return w.e_catch_latched && w.f_catch_latched && w.f_accumulator_travel >= -0.05 &&
               w.d_tank_kg >= 240000.0 && w.e_cab_travel <= 0.05 && w.f_platform_travel <= 0.05;
    });
    const auto w = band.wet_state();
    require(rearmed, "band: the dump must hoist the chiller back, recharge the accumulator and "
                     "refill D's tank, bringing E's cab and F's platform back down");
    std::cout << "PASS scraperx_sim AS-007 band: to_340_s=" << to_plate
              << " plate_y=" << on_plate.player_position.y << " tank_kg=" << w.d_tank_kg
              << " header_kg=" << w.header_kg << " bucket_kg=" << w.e_bucket_kg << '\n';
}

// AS-007's climbing route: at (x, z) facing (fx, fz), take the hold in front
// (jumping for it when `jump`) and climb, stick toward it, until standing
// above top.
bool climb_wet_hold(scraperx::sim::Simulation &simulation, const double x, const double z,
                    const double fx, const double fz, const bool jump, const double top) {
    if (!walk_to(simulation, x, z, 8.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.4);
    if (jump) {
        (void)simulation.request_jump();
        if (!hold_stick(simulation, 0.6 * fx, 0.6 * fz, fx, fz, 3.0,
                        [](const scraperx::sim::Snapshot &state) { return is_climbing(state); })) {
            return false;
        }
    } else {
        if (!simulation.snapshot().grip_available) {
            return false;
        }
        (void)simulation.request_traversal();
        (void)simulation.advance_frame(0.2);
        if (!is_climbing(simulation.snapshot())) {
            return false;
        }
    }
    return hold_stick(simulation, fx, fz, fx, fz, 60.0,
                      [top](const scraperx::sim::Snapshot &state) { return standing_above(state, top); });
}

void run_wet_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalState;
    Simulation route(InitialSpawn::Ring220North);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    // 220 -> 242: the L of boards, the ladder.
    require(walk_to(route, 3.5, -130.5, 6.0, 0.1) && walk_to(route, 3.5, -132.4, 6.0, 0.1) &&
                climb_wet_hold(route, 2.5, -132.4, 0.0, 1.0, false, 242.5),
            "route: boards and ladder from the 220 ring to the 242");
    // 242 -> 264: the beam, the panel caught with a jump.
    require(walk_to(route, -1.0, -131.2, 8.0, 0.1) &&
                climb_wet_hold(route, -1.0, -133.2, 0.0, 1.0, true, 264.5),
            "route: the beam and the jump to the panel, to the 264 ring");
    // 264: the bulkhead leaves only the lip. Back to the edge, drop into a
    // hang, shimmy under it, climb back up.
    require(walk_to(route, 1.2, -131.4, 6.0, 0.08), "route: west of the bulkhead on the 264 ring");
    (void)route.set_facing(0.0, 1.0);
    (void)hold_stick(route, 0.0, -0.4, 0.0, 1.0, 2.0, [](const scraperx::sim::Snapshot &state) {
        return state.player_position.z <= -132.09;
    });
    (void)route.advance_frame(0.3);
    require(route.snapshot().edge_drop_available, "route: the 264 ring's edge offers a drop");
    (void)route.request_release();
    require(wait_for(route, 1.5,
                     [](const scraperx::sim::Snapshot &state) {
                         return state.traversal_state == TraversalState::Hanging;
                     }),
            "route: the drop lowers the body into a hang on the lip");
    require(hold_stick(route, 1.0, 0.0, 0.0, 1.0, 6.0,
                       [](const scraperx::sim::Snapshot &state) { return state.player_position.x >= 3.45; }),
            "route: the shimmy carries the body under the bulkhead");
    (void)route.request_jump();
    require(wait_for(route, 2.5,
                     [](const scraperx::sim::Snapshot &state) { return standing_above(state, 264.5); }),
            "route: up from the hang east of the bulkhead");
    // 264 -> 286: the L of boards, the standpipe.
    require(walk_to(route, 5.35, -132.2, 6.0, 0.1) && walk_to(route, 5.35, -134.4, 6.0, 0.1) &&
                walk_to(route, 5.0, -134.4, 6.0, 0.1) &&
                climb_wet_hold(route, 5.0, -134.2, 0.0, 1.0, false, 286.5),
            "route: boards and standpipe to the 286 ring");
    // 286 -> 308: the L of boards, the ladder.
    require(walk_to(route, 4.0, -133.0, 6.0, 0.1) && walk_to(route, 4.0, -135.1, 6.0, 0.1) &&
                climb_wet_hold(route, 3.0, -135.1, 0.0, 1.0, false, 308.5),
            "route: boards and ladder to the 308 ring");
    // 308 -> 330: the catwalk, the panel caught with a jump.
    require(walk_to(route, 1.5, -134.0, 6.0, 0.1) &&
                climb_wet_hold(route, 1.5, -135.95, 0.0, 1.0, true, 330.5),
            "route: the catwalk and the jump to the panel, to the 330 ring");
    // 330 -> TP-340: the ladder on the plate's north face, facing south.
    require(climb_wet_hold(route, 3.0, -134.9, 0.0, -1.0, false, 340.5),
            "route: the ladder on TP-340's face, onto the plate");
    const auto top = route.snapshot();
    const auto w = route.wet_state();
    require(w.d_platform_travel < 0.02 && w.e_cab_travel < 0.05 && w.f_platform_travel < 0.05 &&
                w.e_catch_latched && w.f_catch_latched,
            "route: no lift in the band moved");
    std::cout << "PASS scraperx_sim AS-007 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lifts_untouched=1\n";
}

// The hands' handling rules, each falsified on the machine that showed it
// through the game's input: a taken handle is drawn in to the hands at
// kCarryPullInSpeed (3 m/s), never snapped to them; and a heavy free load is
// carried slowly and stopped gently enough to stay in them. (That a catch
// never draws back a body already leaving its seat is falsified by pad_wet:
// F relatched at 0.00 m under the old rule, on that input's timing, which
// no native pull here reproduces.)
void run_wet_handling() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;

    // N's flap chain hangs over the hands: taken, the hands draw it down
    // at 3 m/s while it swings on its chain, never faster than the hands'
    // own limit on screen, 0.1 m a frame (6 m/s). Snapped to the hands, a
    // handle moved 0.23 m in the first tick (S1's chain, since replaced by
    // its scale plate; S2's handle, since replaced by its shoot bolt; S3's,
    // since replaced by its pan).
    Simulation chain(InitialSpawn::Deck684);
    require(chain.advance_frame(1.0).accepted, "N chain settle interval must be accepted");
    require(walk_to(chain, -5.6, -155.35, 20.0) && walk_to(chain, -6.6, -155.35, 6.0, 0.06),
            "the player must walk into N's cage beside its chain");
    (void)chain.set_facing(-1.0, 0.0);
    (void)chain.advance_frame(0.5);
    require(chain.snapshot().carry_target_entity_id == Simulation::kServiceNHandleEntityId,
            "N's chain must be in reach");
    const auto handle = chain.kit_body_index(Simulation::kServiceNHandleEntityId);
    auto handle_last = chain.kit_body_position(handle);
    const double handle_y0 = handle_last.y;
    (void)chain.request_pick_up();
    double worst_handle_tick = 0.0;
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz / 2; ++tick) {
        require(chain.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "N chain pull-in tick must be accepted");
        const auto now = chain.kit_body_position(handle);
        worst_handle_tick =
            std::max(worst_handle_tick, std::hypot(std::hypot(now.x - handle_last.x, now.y - handle_last.y),
                                                   now.z - handle_last.z));
        handle_last = now;
    }
    std::cout << "  N chain taken: came down " << handle_y0 - handle_last.y << " m, worst tick "
              << worst_handle_tick << " m\n";
    require(chain.snapshot().carrying_entity_id == Simulation::kServiceNHandleEntityId &&
                handle_y0 - handle_last.y > 0.25,
            "taken, N's chain must come down to the hands");
    require(worst_handle_tick <= 6.0 * Simulation::kFixedStepSeconds,
            "a taken handle must be drawn in to the hands, not snapped to them");

    // D's 50 kg spool, carried along the 220 ring at full stick and stopped:
    // held to 1 - weight / (0.8 grip) of full speed (1.76 m/s), started and
    // stopped within the grip, it stays in the hands. At full speed it was
    // thrown.
    Simulation spool(InitialSpawn::Ring220North);
    (void)spool.advance_frame(1.0);
    require(walk_to(spool, 9.5, -130.6, 8.0, 0.08), "the walk to D's spool must be accepted");
    (void)spool.set_facing(0.0, 1.0);
    (void)spool.advance_frame(0.4);
    require(spool.snapshot().carry_target_entity_id == Simulation::kWetDSpoolEntityId,
            "D's spool must be in reach");
    (void)spool.request_pick_up();
    (void)spool.advance_frame(0.5);
    require(spool.snapshot().carrying_entity_id == Simulation::kWetDSpoolEntityId, "PICK UP must lift D's spool");
    double spool_peak_speed = 0.0;
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz; ++tick) {
        (void)spool.set_move_input(-1.0, 0.0);
        (void)spool.set_facing(0.0, 1.0);
        (void)spool.advance_frame(Simulation::kFixedStepSeconds);
        spool_peak_speed = std::max(spool_peak_speed, horizontal_magnitude(spool.snapshot().player_linear_velocity));
    }
    (void)spool.set_move_input(0.0, 0.0);
    (void)spool.advance_frame(1.5);
    require(spool_peak_speed <= 1.8, "carrying D's 50 kg spool, the carrier must be held to 1.76 m/s");
    require(spool.snapshot().carrying_entity_id == Simulation::kWetDSpoolEntityId,
            "carried at full stick and stopped, D's spool must stay in the hands");

    std::cout << "PASS scraperx_sim handling: handle_tick_m=" << worst_handle_tick
              << " spool_peak_mps=" << spool_peak_speed << "\n";
}
