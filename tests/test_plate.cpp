#include "sim_checks.hpp"
#include "sim_routes.hpp"

// The plate shop: the cleat, the girder, the cage, and the wrecks.

// ---- AS-008, the Plate Shop (340 -> 484 m) ------------------------------------

// From TP-340's north side by F's hole, west past the header, to the south of
// G's cleat.
bool walk_to_shop_cleat(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, 11.3, -138.0, 6.0) && walk_to(simulation, 1.0, -141.0, 16.0) &&
           walk_to(simulation, -5.0, -153.5, 12.0) && walk_to(simulation, -9.7, -152.9, 8.0, 0.08);
}


// From TP-340's north side into G's shaft between its posts and west onto the
// platform.
bool board_shop_g(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, 11.3, -138.0, 6.0) && walk_to(simulation, 1.0, -141.0, 16.0) &&
           walk_to(simulation, -8.0, -147.0, 12.0) && walk_to(simulation, -8.0, -150.0, 4.0) &&
           walk_to(simulation, -11.2, -150.0, 6.0, 0.08);
}


// G's rope off its cleat, carried round the shaft's posts onto the platform
// and hooked on the platform's eye.
bool rig_shop_g(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    const auto at_cleat = simulation.snapshot();
    if (at_cleat.rig_action != 2 || at_cleat.rig_target_entity_id != Simulation::kShopGShackleEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kShopGShackleEntityId ||
        !(walk_to(simulation, -8.5, -152.9, 4.0) && walk_to(simulation, -8.5, -150.1, 4.0) &&
          walk_to(simulation, -11.2, -150.0, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(1.5);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kShopGPlatformEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.shop_state().g_rope_on_eye;
}


// G's prop pin, drawn by its lanyard's handle over the platform's north rail.
bool pull_shop_g(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -11.3, -149.25, 0.0, 1.0, Simulation::kShopGHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.shop_state().g_tower_latched;
                       });
}


// From the 374 ring's west band round to its north band, the girder's tail
// pin carried out of its socket and set down.
bool pull_shop_girder_pin(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, -14.5, -134.3, 16.0) && walk_to(simulation, -2.8, -134.0, 16.0) &&
          walk_to(simulation, -3.6, -134.0, 4.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kShopHGirderPinEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kShopHGirderPinEntityId ||
        !walk_to(simulation, -2.6, -134.0, 4.0)) {
        return false;
    }
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.0);
    return !simulation.shop_state().h_girder_latched;
}


// Round the ring to the gangway, over it onto H's platform.
bool board_shop_h(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -14.5, -134.3, 16.0) && walk_to(simulation, -14.5, -145.6, 16.0) &&
           walk_to(simulation, -9.6, -145.6, 10.0);
}


// H's chock, yanked by its lanyard's handle over the platform's north rail.
bool pull_shop_chock(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -9.4, -144.9, 0.0, 1.0, Simulation::kShopHHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.shop_state().h_trolley_latched;
                       });
}


// On I's cage: the rope's shackle, hanging a metre west of the eye, onto it.
bool rig_shop_i(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -7.3, -145.6, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kShopIShackleEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kShopIShackleEntityId ||
        !walk_to(simulation, -6.3, -145.6, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kShopICageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.shop_state().i_rope_on_eye;
}


// The domino's pin, drawn by its lanyard's handle over the cage's north rail.
bool pull_shop_domino(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -6.5, -144.9, 0.0, 1.0, Simulation::kShopIHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.shop_state().i_domino_latched;
                       });
}


void run_plate_shop() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- G: the rope on its cleat, the tower hangs on it ------------------------------
    Simulation g_fast(InitialSpawn::PlateTop);
    (void)g_fast.advance_frame(1.0);
    require(!g_fast.shop_state().g_rope_on_eye && g_fast.shop_state().g_tower_latched,
            "as found, G's rope is made fast on its cleat and the tower stands pinned");
    require(board_shop_g(g_fast) && pull_shop_g(g_fast), "the rider must pull G's pin from the platform");
    (void)g_fast.advance_frame(10.0);
    const auto g_hung = g_fast.shop_state();
    const auto g_at = g_fast.snapshot();
    std::cout << "G cleat platform=" << g_hung.g_platform_travel << " tower=" << g_hung.g_tower_travel
              << " rope_on_eye=" << g_hung.g_rope_on_eye << " latched=" << g_hung.g_tower_latched
              << " player=" << g_at.player_position.x << "," << g_at.player_position.y << ","
              << g_at.player_position.z << "\n";
    require(g_hung.g_platform_travel < 0.02 && g_hung.g_tower_travel > -0.2,
            "with the rope on its cleat the tower hangs on it and G's platform stays");

    // ---- G: the rope on the eye, the ride ------------------------------------------
    Simulation g_ride(InitialSpawn::PlateTop);
    (void)g_ride.advance_frame(1.0);
    require(walk_to_shop_cleat(g_ride) && rig_shop_g(g_ride),
            "the rider must take G's rope off its cleat and hook it on the platform's eye");
    const double g_rider_y0 = g_ride.snapshot().player_position.y;
    const double g_tower_y0 = kit_com_y(g_ride, Simulation::kShopGTowerEntityId);
    require(pull_shop_g(g_ride), "the rider must pull G's pin, the rope on the eye");
    const bool g_arrived = wait_for(g_ride, 60.0, [&](const scraperx::sim::Snapshot &) {
        return g_ride.shop_state().g_platform_travel >= 33.7;
    });
    (void)g_ride.advance_frame(1.0);
    const auto g_top = g_ride.snapshot();
    require(g_arrived, "the slumping tower must lift G's platform to the 374 ring");
    require(on_support(g_top, Simulation::kShopGPlatformEntityId), "the rider must ride G's platform all the way");
    const double g_gain = kRiderMassKg * kG * (g_top.player_position.y - g_rider_y0);
    const double g_released = 3000.0 * kG * (g_tower_y0 - kit_com_y(g_ride, Simulation::kShopGTowerEntityId));
    require(g_gain > 0.0 && g_gain <= g_released, "G's rider must never gain more than the tower released");
    std::cout << "PASS scraperx_sim AS-008 G: rider_y=" << g_top.player_position.y << " gain_J=" << g_gain
              << " released_J=" << g_released << '\n';

    // ---- H: the girder's tail pinned, the girder stays -------------------------------
    Simulation h_pinned(InitialSpawn::Ring374West);
    (void)h_pinned.advance_frame(1.0);
    require(h_pinned.shop_state().h_girder_latched && h_pinned.shop_state().h_trolley_latched,
            "as found, H's girder is pinned down by its tail and the trolley stands chocked");
    require(walk_to(h_pinned, -9.6, -145.6, 10.0) && pull_shop_chock(h_pinned),
            "the rider must yank H's chock from the platform");
    (void)h_pinned.advance_frame(15.0);
    require(h_pinned.shop_state().h_platform_travel < 0.05 && h_pinned.shop_state().h_girder_angle < 0.05,
            "with the tail pinned the trolley runs out, the girder stays and H's platform stays");

    // ---- H: the tail pin out, the ride --------------------------------------------------
    Simulation h_ride(InitialSpawn::Ring374West);
    (void)h_ride.advance_frame(1.0);
    require(pull_shop_girder_pin(h_ride), "the rider must carry the girder's tail pin out of its socket");
    require(board_shop_h(h_ride), "the rider must cross the gangway onto H's platform");
    const double h_rider_y0 = h_ride.snapshot().player_position.y;
    const double h_trolley_y0 = kit_com_y(h_ride, Simulation::kShopHTrolleyEntityId);
    const double h_girder_y0 = kit_com_y(h_ride, Simulation::kShopHGirderEntityId);
    require(pull_shop_chock(h_ride), "the rider must yank H's chock, the tail free");
    const bool h_arrived = wait_for(h_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return h_ride.shop_state().h_platform_travel >= 43.9;
    });
    (void)h_ride.advance_frame(1.0);
    const auto h_top = h_ride.snapshot();
    require(h_arrived, "the tipping girder must haul H's platform to the 418 ring");
    require(on_support(h_top, Simulation::kShopHPlatformEntityId), "the rider must ride H's platform all the way");
    const double h_gain = kRiderMassKg * kG * (h_top.player_position.y - h_rider_y0);
    const double h_released =
        14000.0 * kG * (h_trolley_y0 - kit_com_y(h_ride, Simulation::kShopHTrolleyEntityId)) +
        4000.0 * kG * (h_girder_y0 - kit_com_y(h_ride, Simulation::kShopHGirderEntityId));
    require(h_gain > 0.0 && h_gain <= h_released, "H's rider must never gain more than the trolley and girder released");
    std::cout << "PASS scraperx_sim AS-008 H: rider_y=" << h_top.player_position.y
              << " girder_angle=" << h_ride.shop_state().h_girder_angle << " gain_J=" << h_gain
              << " released_J=" << h_released << '\n';

    // ---- I: the shackle free, the monolith falls for nothing --------------------------
    Simulation i_free(InitialSpawn::ShopICage);
    (void)i_free.advance_frame(1.0);
    require(!i_free.shop_state().i_rope_on_eye && i_free.shop_state().i_domino_latched &&
                i_free.shop_state().i_monolith_latched,
            "as found, I's shackle hangs free and the domino and monolith stand caught");
    require(pull_shop_domino(i_free), "the rider must pull the domino's pin from the cage");
    (void)i_free.advance_frame(20.0);
    require(!i_free.shop_state().i_monolith_latched && i_free.shop_state().i_monolith_angle > 0.9 &&
                i_free.shop_state().i_cage_travel < 0.05,
            "with the shackle free the domino trips the monolith, which falls, and I's cage stays");

    // ---- I: the shackle on the eye, the cascade and the ride ----------------------------
    Simulation i_ride(InitialSpawn::ShopICage);
    (void)i_ride.advance_frame(1.0);
    require(rig_shop_i(i_ride), "the rider must hook I's shackle on the cage's eye");
    const double i_rider_y0 = i_ride.snapshot().player_position.y;
    const double i_monolith_y0 = kit_com_y(i_ride, Simulation::kShopIMonolithEntityId);
    const double i_domino_y0 = kit_com_y(i_ride, Simulation::kShopIDominoEntityId);
    require(pull_shop_domino(i_ride), "the rider must pull the domino's pin, hooked on");
    const bool i_arrived = wait_for(i_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return i_ride.shop_state().i_cage_travel >= 43.9;
    });
    (void)i_ride.advance_frame(1.0);
    const auto i_top = i_ride.snapshot();
    require(i_arrived, "the monolith's fall must haul I's cage to 462.25");
    require(on_support(i_top, Simulation::kShopICageEntityId), "the rider must ride I's cage all the way");
    const double i_gain = kRiderMassKg * kG * (i_top.player_position.y - i_rider_y0);
    const double i_released =
        20000.0 * kG * (i_monolith_y0 - kit_com_y(i_ride, Simulation::kShopIMonolithEntityId)) +
        800.0 * kG * (i_domino_y0 - kit_com_y(i_ride, Simulation::kShopIDominoEntityId));
    require(i_gain > 0.0 && i_gain <= i_released, "I's rider must never gain more than the beams released");
    std::cout << "PASS scraperx_sim AS-008 I: rider_y=" << i_top.player_position.y
              << " monolith_angle=" << i_ride.shop_state().i_monolith_angle << " gain_J=" << i_gain
              << " released_J=" << i_released << '\n';
}


// The band in one run on player inputs: from TP-340 by F's hole, G's rope
// onto its platform and its pin out, the ride to the 374 ring; round the ring
// for the girder's tail pin, over the gangway onto H, its chock, the ride to
// the 418 ring; across onto I's cage, its shackle on, the domino's pin, the
// cascade and the ride to 462; up the ladder onto the 484 ring.
void run_plate_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::PlateTop);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(walk_to_shop_cleat(band) && rig_shop_g(band) && pull_shop_g(band),
            "band: G rigged and its pin pulled from the platform");
    require(wait_for(band, 60.0,
                     [&](const scraperx::sim::Snapshot &) { return band.shop_state().g_platform_travel >= 33.7; }),
            "band: G's platform reaches the 374 ring");
    (void)band.advance_frame(1.0);
    require(on_support(band.snapshot(), Simulation::kShopGPlatformEntityId), "band: the rider rides G");
    require(walk_to(band, -14.5, -149.3, 8.0) && pull_shop_girder_pin(band),
            "band: off G onto the 374 ring and the girder's tail pin out");
    require(board_shop_h(band) && pull_shop_chock(band), "band: over the gangway onto H and its chock out");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) { return band.shop_state().h_platform_travel >= 43.9; }),
            "band: H's platform reaches the 418 ring");
    (void)band.advance_frame(1.0);
    require(on_support(band.snapshot(), Simulation::kShopHPlatformEntityId), "band: the rider rides H");
    require(rig_shop_i(band) && pull_shop_domino(band), "band: across onto I's cage, hooked on, the domino's pin out");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) { return band.shop_state().i_cage_travel >= 43.9; }),
            "band: the cascade hauls I's cage to 462.25");
    (void)band.advance_frame(1.0);
    require(on_support(band.snapshot(), Simulation::kShopICageEntityId), "band: the rider rides I");
    require(climb_wet_hold(band, -7.6, -145.6, -1.0, 0.0, false, 484.5),
            "band: up the ladder from the cage onto the 484 ring");
    const auto top = band.snapshot();
    std::cout << "PASS scraperx_sim AS-008 band: to_484_s=" << top.simulation_time_seconds - start
              << " ring_y=" << top.player_position.y << '\n';
}


// AS-008's climbing route, on player inputs: from TP-340 by F's hole round the
// hole and the header to the east band, then one climb per ring gap to the
// 484 ring, every lift in the band where it was found.
void run_plate_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation route(InitialSpawn::PlateTop);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    require(walk_to(route, 11.0, -138.0, 6.0) && walk_to(route, 10.4, -142.3, 8.0) &&
                walk_to(route, 13.07, -157.0, 12.0) &&
                climb_wet_hold(route, 13.07, -158.0, 1.0, 0.0, false, 352.5),
            "route: across TP-340 and the ladder onto the 352 ring");
    for (double h = 352.0; h < 483.9; h += 22.0) {
        const double s = 14.72 - 0.91 * (h - 330.0) / 22.0;
        const bool leg = walk_to(route, s + 0.25, -157.0, 12.0, 0.1) && walk_to(route, s - 1.67, -157.0, 8.0, 0.1) &&
                         climb_wet_hold(route, s - 1.67, -158.0, 1.0, 0.0, false, h + 22.5);
        if (!leg) {
            std::cout << "route: stuck above " << h << " at y=" << route.snapshot().player_position.y << '\n';
        }
        require(leg, "route: boards and a ladder to the next ring");
    }
    const auto top = route.snapshot();
    const auto shop = route.shop_state();
    require(shop.g_platform_travel < 0.02 && shop.h_platform_travel < 0.02 && shop.i_cage_travel < 0.02 &&
                shop.g_tower_latched && shop.h_girder_latched && shop.i_domino_latched,
            "route: no lift in the band moved");
    std::cout << "PASS scraperx_sim AS-008 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lifts_untouched=1\n";
}


// Takes hold of the wreck `entity` faced from (x, z) along (fx, fz) and climbs
// it until standing above `top`; false as soon as the hold is not the wreck's.
bool climb_wreck(scraperx::sim::Simulation &simulation, const double x, const double z, const double fx,
                 const double fz, const std::uint64_t entity, const double top) {
    if (!walk_to(simulation, x, z, 10.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    const auto hold = simulation.snapshot();
    if (!is_climbing(hold) || hold.traversal_support_entity_id != entity) {
        return false;
    }
    return hold_stick(simulation, fx, fz, fx, fz, 60.0,
                      [top](const scraperx::sim::Snapshot &state) { return standing_above(state, top); });
}


// One-shot wreckage is climbable (MECHANISM_ASCENT_PLAN.md §3 rule 6), on
// player inputs: each of AS-008's spent machines is reached and climbed.
void run_plate_wreckage() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    // ---- G, fired from the plate with no one aboard ---------------------------------
    Simulation g(InitialSpawn::PlateTop);
    (void)g.advance_frame(1.0);
    require(walk_to_shop_cleat(g) && rig_shop_g(g), "G wreck: its rope hooked on the platform's eye");
    require(walk_to(g, -8.3, -150.0, 6.0) && walk_to(g, -8.3, -147.6, 6.0) && walk_to(g, -11.3, -147.6, 6.0) &&
                pull_facing(g, -11.3, -147.8, 0.0, -1.0, Simulation::kShopGHandleEntityId, 0.5, 4.0,
                            [&](const scraperx::sim::Snapshot &) { return !g.shop_state().g_tower_latched; }),
            "G wreck: off the platform and its pin pulled from the plate");
    require(wait_for(g, 60.0, [&](const scraperx::sim::Snapshot &) { return g.shop_state().g_platform_travel >= 33.7; }) &&
                g.snapshot().player_position.y < 342.0,
            "G wreck: the tower slumps and lifts the platform without the rider");
    require(climb_wreck(g, -8.0, -147.9, 0.0, -1.0, Simulation::kShopGTowerEntityId, 374.3) &&
                g.snapshot().support_entity_id == Simulation::kShopGTowerEntityId,
            "G wreck: the slumped tower climbed from the plate to its deck");
    const double g_deck_y = g.snapshot().player_position.y;
    require(walk_to(g, -8.0, -150.0, 6.0) && walk_to(g, -11.3, -150.0, 8.0) && walk_to(g, -14.5, -150.0, 8.0) &&
                standing_above(g.snapshot(), 374.2),
            "G wreck: from the tower's deck over the platform onto the 374 ring");
    std::cout << "PASS scraperx_sim AS-008 wreckage G: deck_y=" << g_deck_y
              << " ring_y=" << g.snapshot().player_position.y << '\n';

    // ---- I, fired from the cage with its shackle free -----------------------------------
    Simulation i(InitialSpawn::ShopICage);
    (void)i.advance_frame(1.0);
    require(pull_shop_domino(i) && wait_for(i, 20.0,
                                            [&](const scraperx::sim::Snapshot &) {
                                                return i.shop_state().i_monolith_angle > 0.95;
                                            }) &&
                i.shop_state().i_cage_travel < 0.05,
            "I wreck: the cascade drops the monolith beside the cage, which stays");
    require(walk_to(i, -5.6, -145.6, 6.0, 0.08), "I wreck: to the cage's east side");
    (void)i.set_facing(1.0, 0.0);
    (void)i.advance_frame(0.4);
    (void)i.request_traversal();
    require(wait_for(i, 3.0,
                     [](const scraperx::sim::Snapshot &state) {
                         return state.support_entity_id == scraperx::sim::Simulation::kShopIMonolithEntityId &&
                                standing_above(state, 419.3);
                     }),
            "I wreck: up from the cage onto the fallen monolith");
    const double i_slab_y = i.snapshot().player_position.y;
    require(walk_to(i, -4.2, -138.2, 16.0) && walk_to(i, -4.2, -136.4, 6.0) &&
                wait_for(i, 2.0, [](const scraperx::sim::Snapshot &state) { return standing_above(state, 418.2); }) &&
                i.snapshot().player_position.y < 419.3,
            "I wreck: along the monolith and down onto the 418 ring");
    std::cout << "PASS scraperx_sim AS-008 wreckage I: slab_y=" << i_slab_y
              << " ring_y=" << i.snapshot().player_position.y << '\n';

    // ---- H, its chock pulled with the tail pinned, then the pin drawn from the ring ----
    Simulation h(InitialSpawn::Ring374West);
    (void)h.advance_frame(1.0);
    require(board_shop_h(h) && pull_shop_chock(h) && walk_to(h, -9.6, -145.6, 6.0) &&
                walk_to(h, -14.5, -145.6, 12.0) && pull_shop_girder_pin(h),
            "H wreck: the chock pulled, back over the gangway, the girder's tail pin drawn");
    require(wait_for(h, 60.0,
                     [&](const scraperx::sim::Snapshot &) { return h.shop_state().h_platform_travel >= 43.9; }) &&
                h.snapshot().player_position.y < 376.0,
            "H wreck: the girder tips and lifts the platform without the rider");
    require(walk_to(h, -6.6, -134.0, 8.0) &&
                climb_wreck(h, -6.6, -136.75, 0.0, -1.0, Simulation::kShopHGirderEntityId, 378.7) &&
                h.snapshot().support_entity_id == Simulation::kShopHGirderEntityId,
            "H wreck: the tipped girder's ladder climbed from the 374 ring onto its landing");
    std::cout << "PASS scraperx_sim AS-008 wreckage H: girder_angle=" << h.shop_state().h_girder_angle
              << " landing_y=" << h.snapshot().player_position.y << '\n';
}


