// AS-006, the Counterweight Well: stages A, B and C, and the band in one run.

#include "sim_tests.hpp"

void run_well_stage_a() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    // ---- AS-006 Stage A, the skip lift (03_EXECUTION/ASCENT/AS-006_CW_PIN.md)
    //
    // No link, no lift: as found, the rope's end is made fast on the bollard.
    // Take the trip handle and keep pulling past the lever's stop: the catch
    // lets go, the bollard holds the skip within its 0.03 m of slack, and the
    // cage never moves. Let go: the lever falls back, the catch seats the
    // skip again and takes the load off the rope, and the rope's end comes
    // off the bollard onto the cage as before -- the stage is not stranded.
    Simulation unlinked(InitialSpawn::Deck154);
    unlinked.place_matron_home(154.0F);
    require(unlinked.advance_frame(1.0).accepted, "the stair-top settle interval must be accepted");
    const auto well_stair_top = unlinked.snapshot();
    require(well_stair_top.player_grounded && well_stair_top.player_position.y > 154.8 &&
                well_stair_top.player_position.y < 155.0,
            "Deck154 must stand the player on the 154 m deck");
    require(well_stair_top.well_a_catch_latched &&
                well_stair_top.well_a_rope_end_entity_id == Simulation::kWellAFrameEntityId,
            "as found, the skip must be in its catch and the rope made fast on the bollard");
    const double unlinked_skip_y0 = kit_y(unlinked, Simulation::kWellASkipEntityId);
    const double unlinked_cage_y0 = kit_y(unlinked, Simulation::kWellACageEntityId);
    require(board_well_a(unlinked), "the player must walk into Stage A's cage");
    require(walk_to(unlinked, -11.05, -130.35, 3.0, 0.08),
            "the player must reach the trip handle from inside the cage");
    (void)unlinked.set_facing(0.0, 1.0);
    (void)unlinked.advance_frame(0.5);
    require(unlinked.snapshot().carry_target_entity_id == Simulation::kWellAHandleEntityId &&
                unlinked.snapshot().carry_target_kind == 2,
            "the trip handle must be offered to a player facing it from the cage");
    (void)unlinked.request_pick_up();
    (void)unlinked.advance_frame(0.3);
    require(unlinked.snapshot().carrying_entity_id == Simulation::kWellAHandleEntityId,
            "the player must take the trip handle");
    double unlinked_cage_worst = 0.0;
    double unlinked_skip_drop = 0.0;
    double bollard_tension = 0.0;
    bool unlinked_opened = false;
    const auto note_unlinked = [&]() {
        const auto state = unlinked.snapshot();
        unlinked_opened = unlinked_opened || !state.well_a_catch_latched;
        unlinked_cage_worst =
            std::max(unlinked_cage_worst,
                     std::abs(kit_y(unlinked, Simulation::kWellACageEntityId) - unlinked_cage_y0));
        unlinked_skip_drop = std::max(
            unlinked_skip_drop, unlinked_skip_y0 - kit_y(unlinked, Simulation::kWellASkipEntityId));
        if (!state.well_a_catch_latched) {
            bollard_tension = std::max(bollard_tension, state.well_a_rope_tension_n);
        }
    };
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)unlinked.set_move_input(0.0, -0.35);
        (void)unlinked.set_facing(0.0, 1.0);
        (void)unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_unlinked();
    }
    (void)unlinked.set_move_input(0.0, 0.0);
    if (unlinked.snapshot().carrying_entity_id != 0) {
        (void)unlinked.request_set_down();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_unlinked();
    }
    const auto after_unlinked = unlinked.snapshot();
    require(unlinked_opened, "pulling the trip handle must open the catch, linked or not");
    require(unlinked_cage_worst <= 0.05,
            "no link, no lift: with the rope on the bollard the cage must not move");
    require(unlinked_skip_drop <= 0.05,
            "the bollard must hold the skip within its slack when the catch opens");
    require(bollard_tension > 0.9 * kWellASkipMassKg * kGravity,
            "with the catch open, the bollard must carry the skip's weight through the rope");
    require(after_unlinked.well_a_catch_latched && after_unlinked.well_a_rope_tension_n < 60.0,
            "let go, the catch must seat the skip again and take the load off the rope");
    require(rig_well_a(unlinked),
            "after the wasted pull the rope's end must still come off the bollard onto the cage");
    std::cout << "PASS scraperx_sim AS-006 A no link: cage_worst=" << unlinked_cage_worst
              << " skip_drop=" << unlinked_skip_drop << " bollard_tension=" << bollard_tension
              << " relatched=" << int(after_unlinked.well_a_catch_latched) << " rerigged=1\n";

    // The ride: take the rope's end off the bollard, hook it on the cage's
    // eye, take the handle and step back. The skip falls 22 m and the cage
    // carries the rider 22 m under a governor that can only brake. The rider
    // stands on the cage the whole way; the floor stops flush with the 176
    // ring; at no tick has the payload gained more energy than the skip
    // released.
    Simulation well(InitialSpawn::Deck154);
    well.place_matron_home(154.0F);
    require(well.advance_frame(1.0).accepted, "the ride's settle interval must be accepted");
    require(board_well_a(well) && rig_well_a(well),
            "the player must rig Stage A from inside its cage");
    const double skip_y0 = kit_y(well, Simulation::kWellASkipEntityId);
    const double cage_y0 = kit_y(well, Simulation::kWellACageEntityId);
    const double rider_y0 = well.snapshot().player_position.y;
    require(pull_well_a(well, 0.5, 3.0), "stepping back with the handle must trip the catch");
    const double well_ride_start = well.snapshot().simulation_time_seconds;
    bool rode_on_cage = true;
    double well_ride_seconds = 0.0;
    double worst_energy_margin = std::numeric_limits<double>::infinity();
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        const double cage_rise = kit_y(well, Simulation::kWellACageEntityId) - cage_y0;
        if (cage_rise > 0.05 && cage_rise < kWellATravel - 0.05) {
            rode_on_cage = rode_on_cage && state.player_grounded &&
                           state.support_entity_id == Simulation::kWellACageEntityId;
        }
        const double released =
            kWellASkipMassKg * kGravity * (skip_y0 - kit_y(well, Simulation::kWellASkipEntityId));
        const double gained = kWellACageMassKg * kGravity * cage_rise +
                              kRiderMassKg * kGravity * (state.player_position.y - rider_y0);
        worst_energy_margin = std::min(worst_energy_margin, released - gained);
        if (well_ride_seconds == 0.0 && state.well_a_cage_travel >= kWellATravel - 0.01) {
            well_ride_seconds = state.simulation_time_seconds - well_ride_start;
        }
    }
    const auto at_top = well.snapshot();
    const double floor_y = kit_y(well, Simulation::kWellACageEntityId) + 0.10;
    const double skip_released =
        kWellASkipMassKg * kGravity * (skip_y0 - kit_y(well, Simulation::kWellASkipEntityId));
    const double payload_gained =
        kWellACageMassKg * kGravity * (kit_y(well, Simulation::kWellACageEntityId) - cage_y0) +
        kRiderMassKg * kGravity * (at_top.player_position.y - rider_y0);
    require(rode_on_cage, "the rider must stand on the cage for the whole ride");
    require(std::abs(floor_y - (kWellACageFloorTop + kWellATravel)) <= 0.05,
            "the cage's floor must stop flush with the 176 ring at 176.25 m");
    require(at_top.well_a_cage_peak_speed <= 2.6, "the governor must hold the cage to 2.5 m/s");
    require(at_top.player_grounded && at_top.support_entity_id == Simulation::kWellACageEntityId &&
                at_top.player_position.y > 177.0,
            "the rider must arrive standing in the cage at the top");
    require(worst_energy_margin >= -kStanceJitterJ,
            "at no tick may the payload have gained more energy than the skip released");

    // Dying restores the last commit, machines included: from the parked
    // cage walk east, across Stage B's cage waiting flush beside it, and off
    // B's open east side into the well. The fall is lethal, and the restore
    // returns the rider to the last firm footing, on the two cages' floor at
    // 176.25, with Stage A as it was committed: the cage at the top, the rope
    // on its eye.
    const auto well_deaths = at_top.death_count;
    for (std::uint32_t tick = 0; tick < 90 * 8 && well.snapshot().death_count == well_deaths;
         ++tick) {
        (void)well.set_move_input(well.snapshot().player_grounded ? 0.6 : 0.0, 0.0);
        (void)well.set_facing(1.0, 0.0);
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(well.snapshot().death_count == well_deaths + 1,
            "walking off the parked cage must be a lethal fall");
    (void)well.set_move_input(0.0, 0.0);
    require(well.advance_frame(1.0).accepted, "the restore settle interval must be accepted");
    const auto well_restored = well.snapshot();
    require(well_restored.player_position.y > 177.0 &&
                (well_restored.support_entity_id == Simulation::kWellACageEntityId ||
                 well_restored.support_entity_id == Simulation::kWellBCageEntityId) &&
                std::abs(well_restored.well_a_cage_travel - kWellATravel) <= 0.05 &&
                well_restored.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "the restore must return the rider to the parked cages' floor with Stage A as committed");
    std::cout << "PASS scraperx_sim AS-006 A ride: ride_s=" << well_ride_seconds
              << " floor_y=" << floor_y << " peak_speed=" << at_top.well_a_cage_peak_speed
              << " rode_on_cage=" << int(rode_on_cage) << " skip_released_J=" << skip_released
              << " payload_gained_J=" << payload_gained
              << " energy_margin_J=" << worst_energy_margin
              << " restored_y=" << well_restored.player_position.y << '\n';
}

void run_well_stage_b() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    // ---- AS-006 Stage B, the derrick boom ------------------------------------
    //
    // No link, no lift: as found, the hoist line's end is made fast on the
    // cleat in front of B's cage. Take the trip handle and keep pulling past
    // the lever's stop: the catch lets go of the 2.5 t boom, the cleat holds
    // it through the line within its 0.03 m of slack, and the cage never
    // moves. Let go: the catch seats the boom again and takes the load off
    // the line, and the line's end still comes off the cleat onto the cage.
    Simulation b_unlinked(InitialSpawn::WellBCage);
    require(b_unlinked.advance_frame(1.0).accepted, "the Stage B settle interval must be accepted");
    const auto b_found = b_unlinked.snapshot();
    require(b_found.player_grounded &&
                b_found.support_entity_id == Simulation::kWellBCageEntityId &&
                b_found.well_b_catch_latched &&
                b_found.well_b_rope_end_entity_id == Simulation::kWellBFrameEntityId,
            "as found, the rider stands in B's cage, the boom is in its catch and the line is "
            "made fast on the cleat");
    const double b_boom_y0 = kit_com_y(b_unlinked, Simulation::kWellBBoomEntityId);
    const double b_cage_y0 = kit_y(b_unlinked, Simulation::kWellBCageEntityId);
    // Whatever line holds the boom level has a moment arm no longer than
    // the boom: at least the boom's weight moment over its length.
    const double b_pivot_z =
        b_unlinked.kit_body_position(b_unlinked.kit_body_index(Simulation::kWellBBoomEntityId)).z;
    const double b_line_floor =
        kWellBBoomMassKg * kGravity *
        std::abs(kit_com_z(b_unlinked, Simulation::kWellBBoomEntityId) - b_pivot_z) /
        kWellBBoomLength;
    require(walk_to(b_unlinked, -8.0, -130.25, 3.0, 0.08),
            "the player must reach B's trip handle from inside the cage");
    (void)b_unlinked.set_facing(0.0, 1.0);
    (void)b_unlinked.advance_frame(0.5);
    require(b_unlinked.snapshot().carry_target_entity_id == Simulation::kWellBHandleEntityId &&
                b_unlinked.snapshot().carry_target_kind == 2,
            "B's trip handle must be offered to a player facing it from the cage");
    (void)b_unlinked.request_pick_up();
    (void)b_unlinked.advance_frame(0.3);
    require(b_unlinked.snapshot().carrying_entity_id == Simulation::kWellBHandleEntityId,
            "the player must take B's trip handle");
    double b_cage_worst = 0.0;
    double b_boom_drop = 0.0;
    double b_cleat_tension = 0.0;
    bool b_opened = false;
    const auto note_b_unlinked = [&]() {
        const auto state = b_unlinked.snapshot();
        b_opened = b_opened || !state.well_b_catch_latched;
        b_cage_worst =
            std::max(b_cage_worst,
                     std::abs(kit_y(b_unlinked, Simulation::kWellBCageEntityId) - b_cage_y0));
        b_boom_drop = std::max(
            b_boom_drop, b_boom_y0 - kit_com_y(b_unlinked, Simulation::kWellBBoomEntityId));
        if (!state.well_b_catch_latched) {
            b_cleat_tension = std::max(b_cleat_tension, state.well_b_rope_tension_n);
        }
    };
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)b_unlinked.set_move_input(0.0, -0.35);
        (void)b_unlinked.set_facing(0.0, 1.0);
        (void)b_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_b_unlinked();
    }
    (void)b_unlinked.set_move_input(0.0, 0.0);
    if (b_unlinked.snapshot().carrying_entity_id != 0) {
        (void)b_unlinked.request_set_down();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)b_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_b_unlinked();
    }
    const auto after_b_unlinked = b_unlinked.snapshot();
    require(b_opened, "pulling B's trip handle must open the boom's catch, linked or not");
    require(b_cage_worst <= 0.05,
            "no link, no lift: with the line on the cleat B's cage must not move");
    require(b_boom_drop <= 0.05,
            "the cleat must hold the boom through the line within its slack when the catch opens");
    require(b_cleat_tension >= b_line_floor,
            "with the catch open, the line must carry the boom's weight to the cleat");
    require(after_b_unlinked.well_b_catch_latched && after_b_unlinked.well_b_rope_tension_n < 60.0,
            "let go, the catch must seat the boom again and take the load off the line");
    require(rig_well_b(b_unlinked),
            "after the wasted pull the line's end must still come off the cleat onto the cage");
    std::cout << "PASS scraperx_sim AS-006 B no link: cage_worst=" << b_cage_worst
              << " boom_drop=" << b_boom_drop << " cleat_tension=" << b_cleat_tension
              << " line_floor=" << b_line_floor
              << " relatched=" << int(after_b_unlinked.well_b_catch_latched) << " rerigged=1\n";

    // The ride: the line's end off the cleat onto the cage's eye, the handle
    // pulled. The boom swings down about its pivot on the 220 ring, its block
    // draws away from S2 under the 242 ring, and the two-part purchase takes
    // the cage 22 m under a brake-only governor. At the top the cage's rail
    // lifts the striker and the slip hook lets the line run: the boom hangs
    // from its pivot, spent, and the cage stays up on its dogs. At no tick
    // has the payload gained more energy than the boom released.
    Simulation b_well(InitialSpawn::WellBCage);
    require(b_well.advance_frame(1.0).accepted, "B's ride settle interval must be accepted");
    require(rig_well_b(b_well), "the player must rig Stage B from inside its cage");
    const double b_ride_boom_y0 = kit_com_y(b_well, Simulation::kWellBBoomEntityId);
    const double b_ride_cage_y0 = kit_y(b_well, Simulation::kWellBCageEntityId);
    const double b_rider_y0 = b_well.snapshot().player_position.y;
    require(pull_well_b(b_well), "stepping back with B's handle must trip the boom's catch");
    const double b_ride_start = b_well.snapshot().simulation_time_seconds;
    bool b_rode_on_cage = true;
    double b_ride_seconds = 0.0;
    double b_worst_margin = std::numeric_limits<double>::infinity();
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)b_well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = b_well.snapshot();
        const double rise = kit_y(b_well, Simulation::kWellBCageEntityId) - b_ride_cage_y0;
        // While the line hauls: once it runs, the cage drops the last few
        // centimetres onto its dogs under the rider.
        if (rise > 0.05 && !state.well_b_rope_let_go) {
            b_rode_on_cage = b_rode_on_cage && state.player_grounded &&
                             state.support_entity_id == Simulation::kWellBCageEntityId;
        }
        const double released = kWellBBoomMassKg * kGravity *
                                (b_ride_boom_y0 - kit_com_y(b_well, Simulation::kWellBBoomEntityId));
        const double gained = kWellACageMassKg * kGravity * rise +
                               kRiderMassKg * kGravity * (state.player_position.y - b_rider_y0);
        b_worst_margin = std::min(b_worst_margin, released - gained);
        if (b_ride_seconds == 0.0 && state.well_b_cage_travel >= kWellATravel - kWellDogPitch) {
            b_ride_seconds = state.simulation_time_seconds - b_ride_start;
        }
    }
    const auto b_top = b_well.snapshot();
    const double b_floor_y = kit_y(b_well, Simulation::kWellBCageEntityId) + 0.10;
    const double b_top_floor = kWellBCageFloorTop + kWellATravel;
    require(b_rode_on_cage, "the rider must stand on B's cage for the whole ride");
    require(b_top.well_b_rope_let_go,
            "at the top the striker must open the slip hook and let the line run");
    require(b_top.well_b_boom_angle > 1.45,
            "let go, the spent boom must swing on to hang from its pivot");
    require(b_floor_y <= b_top_floor + 0.02 && b_floor_y >= b_top_floor - kWellDogPitch - 0.02,
            "with the line gone, B's cage must stand on its dogs flush with the 198 ring");
    require(b_top.well_b_cage_peak_speed <= 3.1, "the governor must hold B's cage to 3 m/s");
    require(b_worst_margin >= -kStanceJitterJ,
            "at no tick may B's payload have gained more energy than the boom released");
    require(b_top.player_grounded && b_top.support_entity_id == Simulation::kWellBCageEntityId &&
                b_top.player_position.y > 199.0,
            "the rider must arrive standing in B's cage at the top");
    require(walk_to(b_well, -7.2, -128.8, 4.0), "from B's parked cage the rider must step onto the 198 ring");
    (void)b_well.advance_frame(0.5);
    const auto on_198 = b_well.snapshot();
    require(on_198.player_grounded && on_198.player_position.y > 199.0 &&
                on_198.player_position.y < 199.3 && on_198.player_position.z > -129.82 &&
                on_198.support_entity_id != Simulation::kWellBCageEntityId,
            "the rider must stand on the 198 ring");
    // Dying restores the last commit with the spent stage as committed: the
    // line run out, the boom hanging, the cage on its dogs.
    const auto b_deaths = on_198.death_count;
    require(walk_to(b_well, 2.5, -128.8, 6.0), "the rider must walk the 198 ring east of the boom");
    for (std::uint32_t tick = 0; tick < 90 * 10 && b_well.snapshot().death_count == b_deaths;
         ++tick) {
        (void)b_well.set_move_input(0.0, b_well.snapshot().player_grounded ? -0.6 : 0.0);
        (void)b_well.set_facing(0.0, -1.0);
        (void)b_well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(b_well.snapshot().death_count == b_deaths + 1,
            "walking off the 198 ring into the well must be a lethal fall");
    (void)b_well.set_move_input(0.0, 0.0);
    require(b_well.advance_frame(1.0).accepted, "B's restore settle interval must be accepted");
    const auto b_restored = b_well.snapshot();
    require(b_restored.player_grounded && b_restored.player_position.y > 199.0 &&
                b_restored.player_position.y < 199.3 && b_restored.well_b_rope_let_go &&
                b_restored.well_b_boom_angle > 1.45 &&
                b_restored.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01,
            "the restore must return the rider to the 198 ring with Stage B spent as committed");
    std::cout << "PASS scraperx_sim AS-006 B ride: ride_s=" << b_ride_seconds
              << " floor_y=" << b_floor_y << " peak_speed=" << b_top.well_b_cage_peak_speed
              << " boom_angle=" << b_top.well_b_boom_angle
              << " let_go=" << int(b_top.well_b_rope_let_go)
              << " rode_on_cage=" << int(b_rode_on_cage) << " energy_margin_J=" << b_worst_margin
              << " restored_y=" << b_restored.player_position.y << '\n';

    // B's wreckage: along the 198 ring and out on the board beside the spent
    // boom, a hold on its lower end, the climb, and a look round at the 220
    // ring to top out onto it.
    require(walk_to(b_well, 1.0, -128.6, 14.0) && walk_to(b_well, 1.0, -131.8, 6.0, 0.08),
            "B wreck: along the 198 ring and out on the board beside the hanging boom");
    (void)b_well.set_facing(-1.0, 0.0);
    (void)b_well.advance_frame(0.4);
    require(b_well.snapshot().grip_available, "B wreck: the hanging boom's lower end in reach from the board");
    (void)b_well.request_traversal();
    (void)b_well.advance_frame(0.2);
    require(is_climbing(b_well.snapshot()) &&
                b_well.snapshot().traversal_support_entity_id == Simulation::kWellBBoomEntityId,
            "B wreck: a hold on the hanging boom");
    require(hold_stick(b_well, -1.0, 0.0, -1.0, 0.0, 40.0,
                       [](const Snapshot &state) { return state.player_position.y >= 219.8; }),
            "B wreck: up the boom to the 220 ring's edge");
    require(hold_stick(b_well, -1.0, 0.0, 0.0, 1.0, 4.0,
                       [](const Snapshot &state) { return standing_above(state, 220.2); }),
            "B wreck: over the 220 ring's edge from the boom");
    std::cout << "PASS scraperx_sim AS-006 wreckage B: ring_y=" << b_well.snapshot().player_position.y << '\n';
}

void run_well_stage_c() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    // ---- AS-006 Stage C, the debris chute, the band's finale ------------------
    //
    // No link, no lift, both ways round. With the chute jammed the dumpster
    // is empty and lighter than the platform: pull the keeper latch's handle
    // and hold it, and nothing moves; let go, the latch seats itself again.
    // Throw the rebar clear and the dumpster fills with 900 kg of rubble,
    // but with the latch shut the full dumpster just hangs.
    Simulation c_unlinked(InitialSpawn::WellCPlatform);
    require(c_unlinked.advance_frame(1.0).accepted, "the Stage C settle interval must be accepted");
    const auto c_found = c_unlinked.snapshot();
    require(c_found.player_grounded &&
                c_found.support_entity_id == Simulation::kWellCPlatformEntityId &&
                c_found.well_c_catch_latched && c_found.well_c_hopper_kg >= kWellRubbleKg - 1.0 &&
                c_found.well_c_dumpster_kg <= 0.0,
            "as found, the rider stands on C's platform, latched, with the rubble in the hopper");
    const double c_platform_y0 = kit_y(c_unlinked, Simulation::kWellCPlatformEntityId);
    const double c_dumpster_y0 = kit_y(c_unlinked, Simulation::kWellCDumpsterEntityId);
    double c_platform_worst = 0.0;
    double c_dumpster_worst = 0.0;
    std::uint32_t c_open_ticks = 0;
    const auto note_c_unlinked = [&]() {
        const auto state = c_unlinked.snapshot();
        c_open_ticks += state.well_c_catch_latched ? 0U : 1U;
        c_platform_worst = std::max(
            c_platform_worst,
            std::abs(kit_y(c_unlinked, Simulation::kWellCPlatformEntityId) - c_platform_y0));
        c_dumpster_worst = std::max(
            c_dumpster_worst,
            std::abs(kit_y(c_unlinked, Simulation::kWellCDumpsterEntityId) - c_dumpster_y0));
    };
    // The latch pulled open and held there, the dumpster empty.
    require(walk_to(c_unlinked, -3.0, -131.6, 3.0, 0.08),
            "the player must reach the latch's handle from the platform");
    (void)c_unlinked.set_facing(0.0, 1.0);
    (void)c_unlinked.advance_frame(0.5);
    require(c_unlinked.snapshot().carry_target_entity_id ==
                    Simulation::kWellCLatchHandleEntityId &&
                c_unlinked.snapshot().carry_target_kind == 2,
            "the latch's handle must be offered to a player facing it from the platform");
    (void)c_unlinked.request_pick_up();
    (void)c_unlinked.advance_frame(0.3);
    require(c_unlinked.snapshot().carrying_entity_id == Simulation::kWellCLatchHandleEntityId,
            "the player must take the latch's handle");
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        const bool open = !c_unlinked.snapshot().well_c_catch_latched;
        (void)c_unlinked.set_move_input(0.0, open ? 0.0 : -0.35);
        (void)c_unlinked.set_facing(0.0, 1.0);
        (void)c_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_c_unlinked();
    }
    if (c_unlinked.snapshot().carrying_entity_id != 0) {
        (void)c_unlinked.request_set_down();
    }
    require(wait_for(c_unlinked, 3.0,
                     [](const Snapshot &state) { return state.well_c_catch_latched; }),
            "let go, the latch must seat itself on the unmoved platform again");
    const std::uint32_t c_latch_open_ticks = c_open_ticks;
    // The rebar thrown clear, the latch shut.
    require(clear_well_c_chute(c_unlinked),
            "a full pull on the rebar's handle must throw it over upright, clear of the mouth");
    (void)c_unlinked.advance_frame(1.0);
    require(c_unlinked.snapshot().well_c_rebar_angle > 1.7,
            "thrown over upright, the rebar must lie back on its far stop, the mouth clear");
    // Mid-pour, the hopper's stream runs from its mouth down to the rubble's
    // floor in the dumpster, past the dumpster's bail.
    require(wait_for(c_unlinked, 3.0,
                     [](const Snapshot &state) { return state.well_c_dumpster_kg > 100.0; }),
            "the rubble must start pouring into the dumpster");
    const std::uint32_t dumpster_body = c_unlinked.kit_body_index(Simulation::kWellCDumpsterEntityId);
    const auto dumpster_floor = c_unlinked.kit_body_part(dumpster_body, 0);
    const double dumpster_floor_top = kit_y(c_unlinked, Simulation::kWellCDumpsterEntityId) +
                                      dumpster_floor.offset.y + dumpster_floor.half.y;
    bool c_stream_seen = false;
    for (std::uint32_t bin = 0; bin < c_unlinked.kit_bin_count(); ++bin) {
        const auto stream = c_unlinked.kit_bin(bin);
        if (stream.flowing) {
            c_stream_seen = true;
            require(std::abs(stream.stream_to.y - dumpster_floor_top) <= 0.02 &&
                        stream.stream_from.y > dumpster_floor_top + 1.5,
                    "the hopper's stream must fall from its mouth to the dumpster's floor");
        }
    }
    require(c_stream_seen, "mid-pour, one bin must be pouring");
    require(fill_well_c(c_unlinked, 10.0), "the rubble must pour into the dumpster");
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)c_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_c_unlinked();
    }
    const auto c_full = c_unlinked.snapshot();
    require(c_latch_open_ticks >= Simulation::kTickRateHz,
            "the latch must have been held open for at least a second");
    require(c_platform_worst <= 0.05 && c_dumpster_worst <= 0.05,
            "no link, no lift: C's platform must not move with the latch open and the dumpster "
            "empty, nor with the dumpster full and the latch shut");
    require(c_full.well_c_catch_latched && c_full.well_c_hopper_kg <= 1.0 &&
                c_full.well_c_dumpster_kg >= kWellRubbleKg - 1.0,
            "with the latch shut, the full dumpster must just hang");
    std::cout << "PASS scraperx_sim AS-006 C no link: platform_worst=" << c_platform_worst
              << " dumpster_worst=" << c_dumpster_worst << " latch_open_s="
              << static_cast<double>(c_latch_open_ticks) / Simulation::kTickRateHz
              << " dumpster_kg=" << c_full.well_c_dumpster_kg
              << " rebar=" << c_full.well_c_rebar_angle << '\n';

    // The ride: throw the rebar, let the dumpster fill, pull the latch. The
    // full dumpster (1 150 kg) outweighs platform and rider (785 kg) and
    // sinks 22 m, lifting them 22 m under a brake-only governor; at no tick
    // has the payload gained more energy than the dumpster and its rubble
    // released. At the foot of its guide the dumpster presses the striker
    // that opens its gate and empties it -- here, with A's cage as found at
    // the foot of its well, into A's cage. Emptied, the dumpster no longer
    // holds the platform up: the platform's dogs do.
    Simulation c_well(InitialSpawn::WellCPlatform);
    require(c_well.advance_frame(1.0).accepted, "C's ride settle interval must be accepted");
    require(clear_well_c_chute(c_well) && fill_well_c(c_well, 10.0),
            "the rider must clear the chute and let the dumpster fill");
    const double c_ride_platform_y0 = kit_y(c_well, Simulation::kWellCPlatformEntityId);
    const double c_rider_y0 = c_well.snapshot().player_position.y;
    double c_dumpster_prev = kit_y(c_well, Simulation::kWellCDumpsterEntityId);
    require(pull_well_c_latch(c_well, 3.0), "stepping back with the latch's handle must free the platform");
    const double c_ride_start = c_well.snapshot().simulation_time_seconds;
    double c_released = 0.0;
    double c_worst_margin = std::numeric_limits<double>::infinity();
    bool c_rode_on_platform = true;
    double c_ride_seconds = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 25; ++tick) {
        (void)c_well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = c_well.snapshot();
        const double rise = kit_y(c_well, Simulation::kWellCPlatformEntityId) - c_ride_platform_y0;
        if (rise > 0.05 && rise < kWellATravel - kWellDogPitch) {
            c_rode_on_platform = c_rode_on_platform && state.player_grounded &&
                                 state.support_entity_id == Simulation::kWellCPlatformEntityId;
        }
        const double dumpster_y = kit_y(c_well, Simulation::kWellCDumpsterEntityId);
        c_released += (kWellCDumpsterMassKg + state.well_c_dumpster_kg) * kGravity *
                      (c_dumpster_prev - dumpster_y);
        c_dumpster_prev = dumpster_y;
        const double gained = kWellCPlatformMassKg * kGravity * rise +
                              kRiderMassKg * kGravity * (state.player_position.y - c_rider_y0);
        c_worst_margin = std::min(c_worst_margin, c_released - gained);
        if (c_ride_seconds == 0.0 && state.well_c_platform_travel >= kWellATravel - kWellDogPitch) {
            c_ride_seconds = state.simulation_time_seconds - c_ride_start;
        }
    }
    const auto c_top = c_well.snapshot();
    const double c_floor_y = kit_y(c_well, Simulation::kWellCPlatformEntityId) + 0.10;
    const double c_top_floor = kWellCPlatformFloorTop + kWellATravel;
    require(c_rode_on_platform, "the rider must stand on C's platform for the whole ride");
    require(c_top.well_c_dumpster_travel <= -kWellATravel + 0.01 &&
                c_top.well_c_dumpster_kg <= 1.0 &&
                c_top.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0,
            "at the foot of its guide the dumpster's gate must open and empty it into A's cage");
    require(c_floor_y <= c_top_floor + 0.02 && c_floor_y >= c_top_floor - kWellDogPitch - 0.02,
            "emptied, the dumpster no longer holds the platform: it must stand on its dogs flush "
            "with the 220 ring");
    require(c_top.well_c_platform_peak_speed <= 3.1, "the governor must hold C's platform to 3 m/s");
    require(c_worst_margin >= -kStanceJitterJ,
            "at no tick may C's payload have gained more energy than the dumpster released");
    require(c_top.player_grounded &&
                c_top.support_entity_id == Simulation::kWellCPlatformEntityId &&
                c_top.player_position.y > 221.0,
            "the rider must arrive standing on C's platform at the top");
    require(walk_to(c_well, -3.0, -129.0, 4.0), "from C's platform the rider must step onto the 220 ring");
    (void)c_well.advance_frame(0.5);
    const auto on_220 = c_well.snapshot();
    require(on_220.player_grounded && on_220.player_position.y > 221.0 &&
                on_220.player_position.y < 221.3 && on_220.player_position.z > -130.73 &&
                on_220.support_entity_id != Simulation::kWellCPlatformEntityId,
            "the rider must stand on the 220 ring");
    // Dying restores the last commit with the spent stage as committed: the
    // platform on its dogs, the dumpster down and empty, the rubble in A's
    // cage and none left in the hopper.
    const auto c_deaths = on_220.death_count;
    require(walk_to(c_well, 2.5, -129.2, 8.0), "the rider must walk the 220 ring east of the boom");
    for (std::uint32_t tick = 0; tick < 90 * 10 && c_well.snapshot().death_count == c_deaths;
         ++tick) {
        (void)c_well.set_move_input(0.0, c_well.snapshot().player_grounded ? -0.6 : 0.0);
        (void)c_well.set_facing(0.0, -1.0);
        (void)c_well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(c_well.snapshot().death_count == c_deaths + 1,
            "walking off the 220 ring into the well must be a lethal fall");
    (void)c_well.set_move_input(0.0, 0.0);
    require(c_well.advance_frame(1.0).accepted, "C's restore settle interval must be accepted");
    const auto c_restored = c_well.snapshot();
    require(c_restored.player_grounded && c_restored.player_position.y > 221.0 &&
                c_restored.player_position.y < 221.3 &&
                c_restored.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01 &&
                c_restored.well_c_dumpster_travel <= -kWellATravel + 0.01 &&
                c_restored.well_c_dumpster_kg <= 1.0 && c_restored.well_c_hopper_kg <= 1.0 &&
                c_restored.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0,
            "the restore must return the rider to the 220 ring with Stage C spent as committed");
    std::cout << "PASS scraperx_sim AS-006 C ride: ride_s=" << c_ride_seconds
              << " floor_y=" << c_floor_y << " peak_speed=" << c_top.well_c_platform_peak_speed
              << " released_J=" << c_released << " energy_margin_J=" << c_worst_margin
              << " a_cage_rubble_kg=" << c_top.well_a_cage_rubble_kg
              << " rode_on_platform=" << int(c_rode_on_platform)
              << " restored_y=" << c_restored.player_position.y << '\n';
}

void run_well_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    // ---- AS-006, the band in one run -----------------------------------------
    //
    // From the 154 m deck to standing on the 220 ring on player
    // inputs alone: rig and ride A; step across into B's cage waiting flush
    // beside A's; rig and ride B; step across onto C's platform; throw the
    // rebar, let the dumpster fill, pull the latch, ride C; step off onto the
    // 220 ring. Then the cascade: C's spent dumpster empties into A's parked
    // cage, which sinks and hauls A's skip back up into its catch -- C's
    // source re-arms A. Down again by parachute, the rider holds A's tip-out
    // handle down to dump the rubble on the deck and rides the re-armed A up.
    Simulation band(InitialSpawn::Deck154);
    band.place_matron_home(154.0F);
    require(band.advance_frame(1.0).accepted, "the band's settle interval must be accepted");
    const double band_start = band.snapshot().simulation_time_seconds;
    require(board_well_a(band) && rig_well_a(band) && pull_well_a(band, 0.5, 3.0),
            "the rider must board, rig and trip Stage A");
    require(wait_for(band, 16.0,
                     [](const Snapshot &state) {
                         return state.well_a_cage_travel >= kWellATravel - 0.01;
                     }),
            "A must carry the rider to the 176 ring");
    (void)band.advance_frame(1.0);
    require(walk_to(band, -9.4, -131.2, 4.0) && walk_to(band, -7.6, -131.2, 4.0),
            "from A's parked cage the rider must step across into B's");
    require(rig_well_b(band) && pull_well_b(band), "the rider must rig and trip Stage B");
    require(wait_for(band, 16.0,
                     [](const Snapshot &state) {
                         return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
            "B must carry the rider to the 198 ring");
    (void)band.advance_frame(3.0);
    require(band.snapshot().player_grounded &&
                band.snapshot().support_entity_id == Simulation::kWellBCageEntityId,
            "the rider must stand in B's parked cage");
    require(walk_to(band, -6.2, -131.9, 4.0) && walk_to(band, -4.4, -131.9, 4.0),
            "from B's parked cage the rider must step across onto C's platform");
    require(clear_well_c_chute(band) && fill_well_c(band, 10.0) && pull_well_c_latch(band, 3.0),
            "the rider must clear C's chute, let the dumpster fill and pull the latch");
    require(wait_for(band, 16.0,
                     [](const Snapshot &state) {
                         return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
            "C must carry the rider to the 220 ring");
    (void)band.advance_frame(2.0);
    require(walk_to(band, -3.0, -129.0, 4.0), "from C's platform the rider must step onto the 220 ring");
    (void)band.advance_frame(1.0);
    const auto band_220 = band.snapshot();
    require(band_220.player_grounded && band_220.player_position.y > 221.0 &&
                band_220.player_position.y < 221.3 && band_220.player_position.z > -130.73 &&
                band_220.support_entity_id != Simulation::kWellCPlatformEntityId,
            "one run from the 154 m deck must end standing on the 220 ring");
    const double band_seconds = band_220.simulation_time_seconds - band_start;
    require(wait_for(band, 25.0,
                     [](const Snapshot &state) {
                         return state.well_a_catch_latched && state.well_a_cage_travel <= 0.01;
                     }),
            "C's spent dumpster must empty into A's parked cage, which must sink and haul A's "
            "skip back up into its catch");
    const auto rearmed = band.snapshot();
    require(rearmed.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0 &&
                rearmed.well_c_dumpster_kg <= 1.0 && rearmed.well_a_skip_travel >= -0.01 &&
                rearmed.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "re-armed, A's skip must be up in its catch, the rubble in its cage, the rope on its eye");
    // Down by parachute, east of the boom, onto the deck.
    require(walk_to(band, 2.5, -129.6, 8.0), "the rider must walk the 220 ring east of the boom");
    const auto band_deaths = band.snapshot().death_count;
    bool chute = false;
    bool landed = false;
    for (std::uint32_t tick = 0; tick < 90 * 40 && !landed; ++tick) {
        const auto state = band.snapshot();
        if (!chute && !state.player_grounded) {
            (void)band.request_parachute();
            chute = true;
        }
        landed = chute && state.player_grounded && state.player_position.y < 160.0;
        (void)band.set_move_input(0.0, chute ? 0.0 : -0.6);
        (void)band.set_facing(0.0, -1.0);
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)band.set_move_input(0.0, 0.0);
    (void)band.advance_frame(0.5);
    const auto band_landed = band.snapshot();
    std::cout << "  band: chute " << chute << " landed " << landed << " at (" << band_landed.player_position.x << ", "
              << band_landed.player_position.y << ", " << band_landed.player_position.z << ") grounded "
              << band_landed.player_grounded << " deaths " << band_landed.death_count - band_deaths << "\n";
    require(landed && band_landed.death_count == band_deaths && band_landed.player_grounded &&
                band_landed.player_position.y > 154.8 && band_landed.player_position.y < 155.0,
            "a parachute must set the rider down alive on the 154 m deck");
    // Back into A's cage: take the tip-out handle, pull it down until the
    // rubble runs, and hold it there until the cage is empty.
    require(walk_to(band, 2.6, -128.2, 6.0) && walk_to(band, -10.0, -128.2, 14.0) &&
                board_well_a(band) && walk_to(band, -9.6, -130.35, 4.0, 0.08),
            "the rider must walk back into A's cage to the tip-out handle");
    (void)band.set_facing(0.0, 1.0);
    (void)band.advance_frame(0.5);
    require(band.snapshot().carry_target_entity_id == Simulation::kWellATipHandleEntityId &&
                band.snapshot().carry_target_kind == 2,
            "A's tip-out handle must be offered to a player facing it from the cage");
    (void)band.request_pick_up();
    (void)band.advance_frame(0.3);
    require(band.snapshot().carrying_entity_id == Simulation::kWellATipHandleEntityId,
            "the player must take A's tip-out handle");
    bool running = false;
    for (std::uint32_t tick = 0; tick < 90 * 6 && band.snapshot().well_a_cage_rubble_kg > 0.0;
         ++tick) {
        running = running || band.snapshot().well_a_cage_rubble_kg < kWellRubbleKg - 1.0;
        (void)band.set_move_input(0.0, running ? 0.0 : -0.35);
        (void)band.set_facing(0.0, 1.0);
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)band.set_move_input(0.0, 0.0);
    if (band.snapshot().carrying_entity_id != 0) {
        (void)band.request_set_down();
    }
    (void)band.advance_frame(1.0);
    const auto tipped = band.snapshot();
    require(tipped.well_a_cage_rubble_kg <= 0.0 &&
                tipped.well_rubble_spilled_kg >= kWellRubbleKg - 1.0,
            "held down, the tip-out must dump all of A's rubble onto the deck");
    require(tipped.well_a_cage_travel <= 0.01 && tipped.well_a_catch_latched &&
                tipped.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "emptied, A must stand re-armed: the skip up in its catch, the rope on the cage's eye");
    const double a_cage_x = band.kit_body_position(band.kit_body_index(Simulation::kWellACageEntityId)).x;
    const double a_cage_z = band.kit_body_position(band.kit_body_index(Simulation::kWellACageEntityId)).z;
    require(band.kit_pile_count() == 1 &&
                std::abs(band.kit_pile(0).kg - tipped.well_rubble_spilled_kg) <= 0.5 &&
                std::abs(band.kit_pile(0).at.y - 154.0) <= 0.05 &&
                std::hypot(band.kit_pile(0).at.x - a_cage_x, band.kit_pile(0).at.z - a_cage_z) <= 0.1,
            "the tip-out's rubble must lie in one pile on the deck under A's cage");
    require(pull_well_a(band, 0.5, 3.0) &&
                wait_for(band, 16.0,
                         [](const Snapshot &state) {
                             return state.well_a_cage_travel >= kWellATravel - 0.01;
                         }),
            "the re-armed A must carry the rider up again");
    (void)band.advance_frame(1.0);
    const auto band_again = band.snapshot();
    require(band_again.player_grounded &&
                band_again.support_entity_id == Simulation::kWellACageEntityId &&
                band_again.player_position.y > 177.0,
            "the rider must arrive standing in A's cage at the 176 ring again");
    // Dying now restores the band as committed, the spilled pile with it: off
    // the parked cage's open east side, B's cage no longer waits beside it.
    const auto again_deaths = band_again.death_count;
    for (std::uint32_t tick = 0; tick < 90 * 8 && band.snapshot().death_count == again_deaths;
         ++tick) {
        (void)band.set_move_input(band.snapshot().player_grounded ? 0.6 : 0.0, 0.0);
        (void)band.set_facing(1.0, 0.0);
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(band.snapshot().death_count == again_deaths + 1,
            "with B's cage gone up, walking east off A's parked cage must be a lethal fall");
    (void)band.set_move_input(0.0, 0.0);
    require(band.advance_frame(1.0).accepted, "the band's restore settle interval must be accepted");
    const auto band_restored = band.snapshot();
    require(band_restored.player_grounded &&
                band_restored.support_entity_id == Simulation::kWellACageEntityId &&
                band_restored.well_a_cage_travel >= kWellATravel - 0.01 &&
                band_restored.well_a_cage_rubble_kg <= 0.0 && band.kit_pile_count() == 1 &&
                std::abs(band_restored.well_rubble_spilled_kg - tipped.well_rubble_spilled_kg) <= 0.5,
            "the restore must return the rider to A's parked cage with the rubble still on the "
            "deck, not back in the cage");
    std::cout << "PASS scraperx_sim AS-006 band: to_220_s=" << band_seconds
              << " y_220=" << band_220.player_position.y
              << " rearmed_skip_travel=" << rearmed.well_a_skip_travel
              << " landed_impact=" << band_landed.last_impact_speed_mps
              << " spilled_kg=" << tipped.well_rubble_spilled_kg
              << " again_y=" << band_again.player_position.y << '\n';

    // C's wreckage: from A's parked cage, the spent dumpster hangs just over
    // it; up its grab bar from the cage's west side and onto its grate.
    require(walk_to(band, -11.45, -132.25, 6.0, 0.08), "C wreck: to the cage's west side under the spent dumpster");
    (void)band.set_facing(1.0, 0.0);
    (void)band.advance_frame(0.4);
    (void)band.request_traversal();
    (void)band.advance_frame(0.2);
    require(is_climbing(band.snapshot()) &&
                band.snapshot().traversal_support_entity_id == Simulation::kWellCDumpsterEntityId,
            "C wreck: a hold on the spent dumpster's grab bar");
    require(hold_stick(band, 1.0, 0.0, 1.0, 0.0, 20.0,
                       [](const Snapshot &state) { return standing_above(state, 180.6); }) &&
                band.snapshot().support_entity_id == Simulation::kWellCDumpsterEntityId,
            "C wreck: up the bar and onto the dumpster's grate");
    std::cout << "PASS scraperx_sim AS-006 wreckage C: grate_y=" << band.snapshot().player_position.y << '\n';
}
