// AS-007, Wet Isolation (220 -> 340 m), and its handling.

#include "sim_tests.hpp"

#include "sim/bay.hpp"

// Through the gap in D's fill line, where it crosses the walkway, onto the
// platform.
bool board_wet_d(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, 13.0, -130.2, 8.0) && walk_to(simulation, 13.0, -133.4, 6.0);
}

// D's spool, from where it lies on the 220 ring, carried through its gap
// onto the platform, turned round there and set down in the gap behind:
// the rider is aboard when the water runs.
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
    if (simulation.snapshot().carrying_entity_id != Simulation::kWetDSpoolEntityId ||
        !board_wet_d(simulation)) {
        return false;
    }
    // Turned to face the gap, and a step toward it to 0.1 m inside the
    // platform's north edge, so the spool, hanging 0.6 m ahead, is over the
    // gap's line. The walk steers the facing at its target to the last
    // tick, so it is faced north again after it and the hands given the
    // second they take to swing round (kCarryTurnRadiansPerSecond).
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.8);
    if (!walk_to(simulation, 13.0, -132.4, 4.0, 0.12)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(1.2);
    if (simulation.wet_state().d_pipe_whole) {
        return false;   // seated before it was let go of
    }
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.5);
    return simulation.wet_state().d_pipe_whole &&
           on_support(simulation.snapshot(), Simulation::kWetDPlatformEntityId);
}

// From D's platform at the top, through E's doors onto no floor: walked
// south until the feet leave the sill, then the stick let go, so the body
// drops onto E's deck. True once airborne.
bool drop_into_wet_e(scraperx::sim::Simulation &simulation) {
    if (!walk_to(simulation, 13.2, -134.5, 6.0)) {
        return false;
    }
    const bool airborne = hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 4.0, [](const scraperx::sim::Snapshot &state) {
        return !state.player_grounded && state.player_position.y < 257.0;
    });
    (void)simulation.set_move_input(0.0, 0.0);
    return airborne;
}

// From D's platform: backed to the doorway's sill facing north, dropped into
// a hang on its lip, then let go. True once hanging; `hang_seconds` is how
// long the hands hold before letting go (0: the caller lets go).
bool hang_drop_into_wet_e(scraperx::sim::Simulation &simulation, const double hang_seconds) {
    using scraperx::sim::TraversalState;
    if (!walk_to(simulation, 13.2, -134.6, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.3);
    (void)hold_stick(simulation, 0.0, -0.4, 0.0, 1.0, 3.0, [](const scraperx::sim::Snapshot &state) {
        return state.edge_drop_available;
    });
    (void)simulation.advance_frame(0.2);
    if (!simulation.snapshot().edge_drop_available) {
        return false;
    }
    (void)simulation.request_release();
    if (!wait_for(simulation, 1.5, [](const scraperx::sim::Snapshot &state) {
            return state.traversal_state == TraversalState::Hanging;
        })) {
        return false;
    }
    if (hang_seconds > 0.0) {
        (void)simulation.advance_frame(hang_seconds);
        (void)simulation.request_release();
    }
    return true;
}

// E's cab at the top, still: its speed under 0.05 m/s for 2 s running.
bool wait_wet_e_top(scraperx::sim::Simulation &simulation, const double budget_seconds) {
    using scraperx::sim::Simulation;
    const auto ticks = static_cast<std::uint32_t>(budget_seconds * Simulation::kTickRateHz);
    std::uint32_t still = 0;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto w = simulation.wet_state();
        still = w.e_cab_travel > 40.0 && std::abs(w.e_cab_speed) < 0.05 ? still + 1 : 0;
        if (still >= 2 * Simulation::kTickRateHz) {
            return true;
        }
    }
    return false;
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

namespace {

// FNV-1a, 64 bits, over whatever words are fed to it.
struct Fnv final {
    std::uint64_t hash = 14695981039346656037ULL;
    void add(const std::uint64_t word) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (word >> (8 * byte)) & 0xFFU;
            hash *= 1099511628211ULL;
        }
    }
    void add_mm(const double meters) { add(static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(meters * 1000.0)))); }
};

struct Box final {
    double x0, y0, z0, x1, y1, z1;
    [[nodiscard]] bool overlaps(const Box &other) const {
        return x0 < other.x1 && other.x0 < x1 && y0 < other.y1 && other.y0 < y1 && z0 < other.z1 && other.z0 < z1;
    }
};

// Where stage E stands and moves since its rebuild (chiller_drop_spec.md
// section 4): the shaft and the travel of the cab and deck in it, the duct
// and the chiller's travel, the bucket's column, and the line's post and arm
// on the shaft's north-east corner.
const Box kWetEZones[] = {
    {11.50, 249.35, -138.15, 14.50, 300.75, -135.15},   // shaft, cab and deck
    {14.00, 298.00, -135.65, 14.55, 305.05, -135.10},   // the post and its sheave arm
    {8.50, 247.60, -138.15, 11.50, 306.74, -135.15},    // duct, chiller, sheave beam
    {6.15, 249.35, -137.60, 8.05, 306.74, -135.70},     // basin, bucket and striker
};

} // namespace

// The rebuilt stage E moved no body, line or piece of stock elsewhere: the kit's
// bodies stand in the same order, every zipline and every piece of stock is
// built where it was, and none of them reaches into E's shaft, duct or bucket
// column. Prints its counts and hashes; when `expect` is set, requires them.
void run_wet_e_guard(const bool expect) {
    using scraperx::sim::Bay;
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation world(InitialSpawn::WetFPlatform);
    // The bodies there were before E's rebuild, in order; and those built
    // since, after them (the bay on the 154 m deck, bay.hpp).
    constexpr std::uint32_t kBodiesBeforeE = 595;
    Fnv bodies;
    for (std::uint32_t i = 0; i < std::min(world.kit_body_count(), kBodiesBeforeE); ++i) {
        bodies.add(world.kit_body_entity(i));
    }
    const std::uint64_t kSince[] = {Bay::kFrameEntity, Bay::kLeafEntity, Bay::kGuardEntity, Bay::kFanEntity,
                                    Bay::kShackleEntity};
    bool since_after = world.kit_body_count() == kBodiesBeforeE + std::size(kSince);
    for (std::uint32_t i = kBodiesBeforeE; since_after && i < world.kit_body_count(); ++i) {
        since_after = world.kit_body_entity(i) == kSince[i - kBodiesBeforeE];
    }
    Fnv lines;
    bool lines_clear = true;
    for (std::uint32_t i = 0; i < world.zipline_count(); ++i) {
        const auto line = world.zipline_line(i);
        lines.add_mm(line.top.x);
        lines.add_mm(line.top.y);
        lines.add_mm(line.top.z);
        lines.add_mm(line.bottom.x);
        lines.add_mm(line.bottom.y);
        lines.add_mm(line.bottom.z);
        // The rider's room under the cable, 1.0 m wide and 2.7 m deep.
        const double length = std::hypot(std::hypot(line.bottom.x - line.top.x, line.bottom.y - line.top.y),
                                         line.bottom.z - line.top.z);
        const int samples = std::max(2, static_cast<int>(length / 0.25));
        for (int s = 0; s <= samples; ++s) {
            const double t = static_cast<double>(s) / samples;
            const double x = line.top.x + t * (line.bottom.x - line.top.x);
            const double y = line.top.y + t * (line.bottom.y - line.top.y);
            const double z = line.top.z + t * (line.bottom.z - line.top.z);
            const Box room{x - 0.5, y - 2.7, z - 0.5, x + 0.5, y, z + 0.5};
            for (const Box &zone : kWetEZones) {
                lines_clear = lines_clear && !room.overlaps(zone);
            }
        }
    }
    Fnv stock;
    bool stock_clear = true;
    for (std::uint32_t i = 0; i < world.loose_stock_count(); ++i) {
        const std::uint32_t body = world.kit_body_index(Simulation::kLooseStockFirstEntity + i);
        const auto at = world.kit_body_position(body);
        stock.add(world.loose_stock_piece(i).kind);
        stock.add_mm(at.x);
        stock.add_mm(at.y);
        stock.add_mm(at.z);
        // Its parts' reach about where it was built, as a ball round each part.
        for (std::uint32_t p = 0; p < world.kit_body_part_count(body); ++p) {
            const auto part = world.kit_body_part(body, p);
            const double reach =
                std::hypot(std::hypot(part.half.x, part.half.y), part.half.z) +
                std::hypot(std::hypot(part.offset.x, part.offset.y), part.offset.z);
            const Box piece{at.x - reach, at.y - reach, at.z - reach, at.x + reach, at.y + reach, at.z + reach};
            for (const Box &zone : kWetEZones) {
                stock_clear = stock_clear && !piece.overlaps(zone);
            }
        }
    }
    std::cout << "  E guard: bodies=" << world.kit_body_count() << " body_hash=" << bodies.hash
              << " since_after=" << since_after
              << " lines=" << world.zipline_count() << " line_hash=" << lines.hash
              << " stock=" << world.loose_stock_count() << " stock_hash=" << stock.hash
              << " lines_clear=" << lines_clear << " stock_clear=" << stock_clear << '\n';
    if (!expect) {
        return;
    }
    // As recorded on the tree before E's rebuild (its parent, 4118862).
    require(bodies.hash == 3514033760028014323ULL,
            "E guard: the kit's bodies stand in the same order as before E's rebuild");
    require(since_after, "E guard: the bodies built since come after them, the bay's five");
    require(world.zipline_count() == 22 && lines.hash == 2122763527019327139ULL,
            "E guard: every zipline is built where it was, to the millimeter");
    require(world.loose_stock_count() == 325 && stock.hash == 12537791641516680014ULL,
            "E guard: every piece of loose stock is built where it was, to the millimeter");
    require(lines_clear && stock_clear,
            "E guard: no zipline's rider room and no piece of stock reaches into E's shaft, duct or bucket");
}


void run_wet_isolation() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- D: no spool, no water -----------------------------------------------
    Simulation d_dry(InitialSpawn::Ring220North);
    (void)d_dry.advance_frame(1.0);
    require(!d_dry.wet_state().d_pipe_whole, "as found, D's spool lies off its gap");
    require(board_wet_d(d_dry), "the rider must walk through the gap onto D's platform");
    (void)d_dry.advance_frame(10.0);
    require(d_dry.wet_state().d_platform_travel < 0.02 && d_dry.wet_state().d_tube_kg <= 0.0,
            "with the spool on the deck the couplings hold the water and D's platform stays down");

    // ---- D: the spool seated, the ride -----------------------------------------
    Simulation d_ride(InitialSpawn::Ring220North);
    (void)d_ride.advance_frame(1.0);
    const auto d_before = d_ride.wet_state();
    require(seat_wet_spool(d_ride),
            "the rider must carry D's spool onto the platform and set it in the gap behind");
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
    require(seat_wet_spool(band), "band: the spool carried aboard and set in its gap");
    require(wait_for(band, 120.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().d_platform_travel >= 35.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetDPlatformEntityId),
            "band: D carries the rider to 256.25");
    require(drop_into_wet_e(band), "band: from D's platform through E's doors, off the sill");
    require(wait_wet_e_top(band, 60.0) && on_support(band.snapshot(), Simulation::kWetEDeckEntityId),
            "band: E blows the rider up its shaft to 298.25, standing on its deck");
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
    // E re-armed: its deck hung proud again, its doors shut, its chiller in
    // its catch; and the cab's jerk coming back down onto its line is no blow.
    require(w.e_deck_drop <= 0.01 && w.e_door_shut && w.e_catch_latched,
            "band: re-armed, E's deck hangs proud again over its shut doors' cab");
    double rearmed_worst_travel = 0.0;
    bool rearmed_held = true;
    for (std::uint32_t tick = 0; tick < 20U * Simulation::kTickRateHz; ++tick) {
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
        const auto e = band.wet_state();
        rearmed_held = rearmed_held && e.e_catch_latched;
        rearmed_worst_travel = std::max(rearmed_worst_travel, e.e_cab_travel);
    }
    require(rearmed_held && rearmed_worst_travel <= 0.05,
            "band: re-armed, E stays set for 20 s: nothing released, its cab on its stop");
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

// ---- AS-007 E, the chiller drop (03_EXECUTION/CLIMB_REDESIGN/chiller_drop_spec.md) ----

namespace {

// Where the rider stands at the top: level with F's platform.
constexpr double kWetETopY = 298.25;
// The skirt's edge reaches the vent collar's lower edge (296.85 m) at this
// much of the cab's travel (the edge starts at 251.5 m).
constexpr double kWetECollarTravel = 296.85 - 251.5;
// The deck's top, as found: where it hangs, and the height it passes once it
// has risen with the cab to the doorway's foot.
constexpr double kWetEDeckHungY = 253.45;
constexpr double kWetEDoorFootY = 256.0;
// The rider's capsule (physics_world_tuning.hpp): radius and half height.
constexpr double kRiderRadius = 0.35;
constexpr double kRiderHalfHeight = 0.9;

scraperx::sim::Vector3 rotate_back(const scraperx::sim::Quaternion &q, const scraperx::sim::Vector3 &v) {
    // v rotated by q's conjugate: v + 2w (u x v) + 2 u x (u x v), u = -q.xyz.
    const double ux = -q.x;
    const double uy = -q.y;
    const double uz = -q.z;
    const double cx = uy * v.z - uz * v.y;
    const double cy = uz * v.x - ux * v.z;
    const double cz = ux * v.y - uy * v.x;
    return {v.x + 2.0 * (q.w * cx + uy * cz - uz * cy), v.y + 2.0 * (q.w * cy + uz * cx - ux * cz),
            v.z + 2.0 * (q.w * cz + ux * cy - uy * cx)};
}

// How far the rider's capsule stands clear of the leaf's box, m (under 0:
// touching).
double leaf_clearance(const scraperx::sim::Simulation &sim, const std::uint64_t leaf) {
    const std::uint32_t index = sim.kit_body_index(leaf);
    const auto centre = sim.kit_body_center_of_mass(index);
    const auto turn = sim.kit_body_rotation(index);
    const auto half = sim.kit_body_part(index, 0).half;
    const auto at = sim.snapshot().player_position;
    const double reach = kRiderHalfHeight - kRiderRadius;
    double nearest = std::numeric_limits<double>::infinity();
    for (int i = 0; i <= 44; ++i) {
        const double y = at.y - reach + 2.0 * reach * static_cast<double>(i) / 44.0;
        const auto local = rotate_back(turn, {at.x - centre.x, y - centre.y, at.z - centre.z});
        const double dx = local.x - std::clamp(local.x, -half.x, half.x);
        const double dy = local.y - std::clamp(local.y, -half.y, half.y);
        const double dz = local.z - std::clamp(local.z, -half.z, half.z);
        nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    return nearest - kRiderRadius;
}

// The ride, from the step off the sill (or the hands letting go) until the
// cab has stood still for 2 s: what each falsifier of check 3 reads.
struct WetERide final {
    bool landed = false;
    double land_t = -1.0;
    double land_speed = 0.0;
    int land_response = 0;
    double release_t = -1.0;
    double drop_by_1s = 0.0;          // the deck's deepest drop within 1 s of landing
    double shut_t = -1.0;
    double deck_door_t = -1.0;        // the deck's top past the doorway's foot
    double leaf_clear = std::numeric_limits<double>::infinity();
    double launch_accel = 0.0;        // the cab's peak upward acceleration, first 3 s after landing
    double later_accel = 0.0;         // its peak acceleration either way after that
    double peak_deck = 0.0;
    double airborne_s = 0.0;
    bool airborne_low = false;        // off the deck before the skirt reached the collar
    double eye_t = -1.0;
    double eye_y = 0.0;
    double eye_closing = 0.0;
    double still_t = -1.0;            // the cab still for 2 s, at the end of those 2 s
    double rest_deck = 0.0;
    bool saved = false;
    std::vector<std::uint8_t> save;
    scraperx::sim::Vector3 saved_at[4];   // cab, deck, west leaf, chiller, as saved
    double saved_leaf_east_y = 0.0;
};

const std::uint64_t kWetESavedBodies[4] = {scraperx::sim::Simulation::kWetECabEntityId,
                                           scraperx::sim::Simulation::kWetEDeckEntityId,
                                           scraperx::sim::Simulation::kWetEDoorEntityId,
                                           scraperx::sim::Simulation::kWetEChillerEntityId};

// Steps the world from the step off (or the letting go) until E's cab has
// stood still 2 s or `budget` passes; with save_after >= 0, saves the climb
// that long after the landing.
WetERide ride_wet_e(scraperx::sim::Simulation &sim, const double budget, const double save_after) {
    using scraperx::sim::Simulation;
    WetERide ride;
    const double t0 = sim.snapshot().simulation_time_seconds;
    const std::uint32_t ticks = static_cast<std::uint32_t>(budget * Simulation::kTickRateHz);
    double last_speed = sim.wet_state().e_cab_speed;
    std::uint32_t still = 0;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        (void)sim.advance_frame(Simulation::kFixedStepSeconds);
        const auto s = sim.snapshot();
        const auto w = sim.wet_state();
        const double t = s.simulation_time_seconds - t0;
        const double accel = (w.e_cab_speed - last_speed) / Simulation::kFixedStepSeconds;
        last_speed = w.e_cab_speed;
        const double deck = kit_y(sim, Simulation::kWetEDeckEntityId);
        const bool on_deck = s.player_grounded && s.support_entity_id == Simulation::kWetEDeckEntityId;
        if (!ride.landed && on_deck) {
            ride.landed = true;
            ride.land_t = t;
            ride.land_speed = s.last_impact_speed_mps;
            ride.land_response = s.landing_response;
        }
        if (ride.release_t < 0.0 && !w.e_catch_latched) {
            ride.release_t = t;
        }
        if (ride.shut_t < 0.0 && w.e_door_shut) {
            ride.shut_t = t;
        }
        if (ride.deck_door_t < 0.0 && deck > kWetEDoorFootY) {
            ride.deck_door_t = t;
        }
        ride.peak_deck = std::max(ride.peak_deck, deck);
        if (!ride.landed) {
            continue;
        }
        if (t - ride.land_t <= 1.0) {
            ride.drop_by_1s = std::max(ride.drop_by_1s, w.e_deck_drop);
        }
        ride.leaf_clear = std::min({ride.leaf_clear, leaf_clearance(sim, Simulation::kWetEDoorEntityId),
                                    leaf_clearance(sim, Simulation::kWetEEastLeafEntityId)});
        if (t - ride.land_t <= 3.0) {
            ride.launch_accel = std::max(ride.launch_accel, accel);
        } else {
            ride.later_accel = std::max(ride.later_accel, std::abs(accel));
        }
        if (!on_deck) {
            ride.airborne_s += Simulation::kFixedStepSeconds;
            ride.airborne_low = ride.airborne_low || w.e_cab_travel < kWetECollarTravel;
        }
        const double eye = s.player_position.y + 0.62;
        const double chiller = kit_y(sim, Simulation::kWetEChillerEntityId);
        if (ride.eye_t < 0.0 && chiller <= eye) {
            ride.eye_t = t - ride.land_t;
            ride.eye_y = eye;
            ride.eye_closing = s.player_linear_velocity.y -
                               sim.kit_body_velocity(sim.kit_body_index(Simulation::kWetEChillerEntityId)).y;
        }
        if (save_after >= 0.0 && !ride.saved && t - ride.land_t >= save_after) {
            ride.saved = true;
            ride.save = sim.save_game();
            for (int i = 0; i < 4; ++i) {
                ride.saved_at[i] = sim.kit_body_position(sim.kit_body_index(kWetESavedBodies[i]));
            }
            ride.saved_leaf_east_y = kit_y(sim, Simulation::kWetEEastLeafEntityId);
        }
        still = w.e_cab_travel > 40.0 && std::abs(w.e_cab_speed) < 0.05 ? still + 1 : 0;
        if (still >= 2 * Simulation::kTickRateHz) {
            ride.still_t = t;
            ride.rest_deck = deck;
            break;
        }
    }
    return ride;
}

// From the 220 ring: D's spool seated and D ridden to 256.25.
bool ride_wet_d_up(scraperx::sim::Simulation &sim) {
    using scraperx::sim::Simulation;
    (void)sim.advance_frame(1.0);
    return seat_wet_spool(sim) &&
           wait_for(sim, 120.0, [&](const scraperx::sim::Snapshot &) { return sim.wet_state().d_platform_travel >= 35.95; }) &&
           on_support(sim.snapshot(), Simulation::kWetDPlatformEntityId);
}

// Off D's platform into E's doorway, the leaves pushed open on the body.
bool stand_in_wet_e_doorway(scraperx::sim::Simulation &sim) {
    return walk_to(sim, 13.2, -134.5, 6.0) && walk_to(sim, 13.2, -135.15, 4.0, 0.1);
}

// The first piece of loose stock of a kind (LooseStock::Kind), its kit index.
std::uint32_t first_stock(const scraperx::sim::Simulation &sim, const std::uint8_t kind) {
    for (std::uint32_t i = 0; i < sim.loose_stock_count(); ++i) {
        if (sim.loose_stock_piece(i).kind == kind) {
            return sim.kit_body_index(scraperx::sim::Simulation::kLooseStockFirstEntity + i);
        }
    }
    return scraperx::sim::Simulation::kKitNone;
}

constexpr std::uint8_t kStockDrum = 3;
constexpr std::uint8_t kStockToolbox = 16;
const scraperx::sim::Vector3 kOverWetEDeck{13.0, 256.4, -136.65};

} // namespace

void run_wet_e() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- 1. As found ----------------------------------------------------------------
    {
        Simulation found(InitialSpawn::Ring220North);
        (void)found.advance_frame(1.0);
        const auto w = found.wet_state();
        require(std::abs(w.e_deck_drop) < 0.005 && w.e_door_angle < 0.02 && w.e_catch_latched &&
                    std::abs(w.e_cab_travel) < 0.005 && std::abs(w.e_lever_angle) < 0.02,
                "E as found: the deck hung proud, both leaves shut, the chiller in its catch, the cab on "
                "its stop and the trip lever at rest");
        for (std::uint64_t entity = Simulation::kWetECabEntityId; entity <= Simulation::kWetEStrikerEntityId; ++entity) {
            require(found.kit_body_carry_kind(found.kit_body_index(entity)) == 0,
                    "E as found: nothing in it is taken in the hands");
        }
    }

    // ---- 2. The brink: in the doorway, nothing moves and nothing is offered ------------
    Simulation ride(InitialSpawn::Ring220North);
    require(ride_wet_d_up(ride), "E brink: D carries the rider to 256.25");
    require(stand_in_wet_e_doorway(ride), "E brink: the rider steps into E's doorway");
    const double brink_leaves = ride.wet_state().e_door_angle;
    require(brink_leaves > 0.3, "E brink: the leaves stand open on the body in the doorway");
    bool brink_held = true;
    bool brink_offered = false;
    for (std::uint32_t tick = 0; tick < 10U * Simulation::kTickRateHz; ++tick) {
        (void)ride.advance_frame(Simulation::kFixedStepSeconds);
        const auto s = ride.snapshot();
        const auto w = ride.wet_state();
        brink_held = brink_held && w.e_catch_latched && std::abs(w.e_cab_travel) < 0.005 &&
                     std::abs(w.e_deck_drop) < 0.005 && w.e_lever_angle < 0.05 && s.player_grounded;
        brink_offered = brink_offered || s.carry_target_entity_id != 0 || s.rig_action != 0;
    }
    require(brink_held, "E brink: standing 10 s in the doorway sets nothing off (rule 1)");
    require(!brink_offered, "E brink: nothing in E is offered to pull, rig or take");
    std::cout << "  E brink: leaves " << brink_leaves << " rad on the body, 10 s, nothing moved\n";

    // ---- 3. The drop and the ride ------------------------------------------------------
    const auto before = ride.snapshot();
    const double rider_y0 = before.player_position.y;
    const double chiller_y0 = kit_y(ride, Simulation::kWetEChillerEntityId);
    const double bucket_y0 = kit_y(ride, Simulation::kWetEBucketEntityId);
    require(drop_into_wet_e(ride), "E: the rider walks off the sill through the doors");
    const WetERide up = ride_wet_e(ride, 40.0, 8.0);
    const auto top = ride.snapshot();
    std::cout << "  E ride: landed at " << up.land_speed << " m/s (response " << up.land_response
              << "), deck down " << up.drop_by_1s << " m within 1 s, catch released "
              << up.release_t - up.land_t << " s after landing; leaves shut "
              << up.deck_door_t - up.shut_t << " s before the deck reached the doorway; nearest leaf "
              << up.leaf_clear << " m; launch " << up.launch_accel << " m/s^2, top " << up.later_accel
              << " m/s^2; peak deck " << up.peak_deck << "; off the deck " << up.airborne_s
              << " s; chiller past the eye at " << up.eye_y << " m, " << up.eye_t
              << " s after landing, closing " << up.eye_closing << " m/s; still "
              << up.still_t - up.land_t << " s after landing, deck " << up.rest_deck << '\n';
    require(up.landed && up.land_speed < 8.0 && top.death_count == before.death_count &&
                up.land_response >= 1 && up.land_response <= 3,
            "E: the rider lands on the deck under 8 m/s, alive, no worse than a heavy landing");
    require(up.drop_by_1s >= 0.12 && up.release_t >= 0.0 && up.release_t - up.land_t <= 1.0,
            "E: within 1 s of the landing the blow carries the deck 0.12 m down and the chiller's catch lets go");
    require(up.shut_t >= 0.0 && up.deck_door_t >= 0.0 && up.shut_t < up.deck_door_t,
            "E: both leaves swing shut before the deck rises to the doorway");
    require(up.leaf_clear > 0.0, "E: no leaf ever touches the rider");
    require(up.launch_accel <= 12.0, "E: the launch heaves the floor at no more than 12 m/s^2");
    require(up.later_accel <= 16.0, "E: at the top the cab slows and bobs at no more than 16 m/s^2");
    require(up.peak_deck <= 299.65, "E: the deck peaks at most 299.65 m, short of the cab's hard top");
    require(up.still_t >= 0.0 && up.still_t - up.land_t <= 25.0 && std::abs(up.rest_deck - kWetETopY) <= 0.10,
            "E: within 25 s of the landing the cab is still with the deck within 0.10 m of 298.25");
    require(on_support(top, Simulation::kWetEDeckEntityId) && up.airborne_s <= 0.6 && !up.airborne_low,
            "E: the rider rides the deck all the way, off it at most 0.6 s and only once the skirt is in the collar");
    const double gain = kRiderMassKg * kGravity * (top.player_position.y - rider_y0);
    const double released = 5000.0 * kGravity * (chiller_y0 - kit_y(ride, Simulation::kWetEChillerEntityId)) -
                            300.0 * kGravity * (kit_y(ride, Simulation::kWetEBucketEntityId) - bucket_y0);
    require(gain > 0.0 && gain <= released, "E: the rider never gains more than the chiller released");
    double rest_30 = 0.0;
    double rest_60 = 0.0;
    for (std::uint32_t tick = 1; tick <= 60U * Simulation::kTickRateHz; ++tick) {
        (void)ride.advance_frame(Simulation::kFixedStepSeconds);
        if (tick == 30U * Simulation::kTickRateHz) {
            rest_30 = kit_y(ride, Simulation::kWetEDeckEntityId);
        }
    }
    rest_60 = kit_y(ride, Simulation::kWetEDeckEntityId);
    std::cout << "  E rest: deck " << up.rest_deck << " still, " << rest_30 << " 30 s on, " << rest_60
              << " 60 s on\n";
    require(std::abs(rest_30 - kWetETopY) <= 0.12 && std::abs(rest_60 - kWetETopY) <= 0.12,
            "E: the cab holds within 0.12 m of 298.25 30 s and 60 s after it stills");
    require(walk_to(ride, 13.4, -137.2, 4.0) && walk_to(ride, 13.4, -139.9, 6.0) &&
                on_support(ride.snapshot(), Simulation::kWetFPlatformEntityId),
            "E: from the deck the rider walks south onto F's platform");

    // ---- 7. Saved mid-ride, loaded into a fresh world -----------------------------------
    require(up.saved, "E save: the climb saved 8 s into the ride");
    {
        Simulation loaded(InitialSpawn::Ring220North);
        (void)loaded.advance_frame(0.5);
        require(loaded.load_game(up.save.data(), up.save.size()), "E save: a fresh world loads the ride");
        double worst = 0.0;
        for (int i = 0; i < 4; ++i) {
            const auto at = loaded.kit_body_position(loaded.kit_body_index(kWetESavedBodies[i]));
            worst = std::max({worst, std::abs(at.x - up.saved_at[i].x), std::abs(at.y - up.saved_at[i].y),
                              std::abs(at.z - up.saved_at[i].z)});
        }
        worst = std::max(worst, std::abs(kit_y(loaded, Simulation::kWetEEastLeafEntityId) - up.saved_leaf_east_y));
        require(worst <= 0.001, "E save: cab, deck, leaves and chiller load where they were saved, within 1 mm");
        require(wait_wet_e_top(loaded, 40.0), "E save: the loaded ride comes to rest at the top");
        const double loaded_rest = kit_y(loaded, Simulation::kWetEDeckEntityId);
        std::cout << "  E save: loaded within " << worst << " m, rests at " << loaded_rest << '\n';
        require(std::abs(loaded_rest - up.rest_deck) <= 0.02, "E save: the loaded ride ends within 0.02 m of the ride");
    }

    // ---- 4. Hang from the sill and let go -------------------------------------------------
    {
        Simulation hang(InitialSpawn::Ring220North);
        require(ride_wet_d_up(hang), "E hang: D carries the rider to 256.25");
        require(hang_drop_into_wet_e(hang, 0.0), "E hang: the rider drops into a hang on the sill's lip");
        bool held = true;
        for (std::uint32_t tick = 0; tick < 3U * Simulation::kTickRateHz; ++tick) {
            (void)hang.advance_frame(Simulation::kFixedStepSeconds);
            held = held && hang.wet_state().e_catch_latched &&
                   hang.snapshot().traversal_state == TraversalState::Hanging;
        }
        require(held && hang.wet_state().e_door_shut,
                "E hang: hanging 3 s the chiller stays in its catch and the leaves shut over the hands");
        (void)hang.request_release();
        const WetERide let_go = ride_wet_e(hang, 40.0, -1.0);
        std::cout << "  E hang: landed at " << let_go.land_speed << " m/s, deck down " << let_go.drop_by_1s
                  << " m, catch released " << let_go.release_t - let_go.land_t << " s after landing, still deck "
                  << let_go.rest_deck << '\n';
        require(let_go.landed && let_go.release_t >= 0.0 && let_go.release_t - let_go.land_t <= 1.0,
                "E hang: let go, the drop sets the chiller off within 1 s of the landing");
        require(let_go.still_t >= 0.0 && let_go.still_t - let_go.land_t <= 25.0 &&
                    std::abs(let_go.rest_deck - kWetETopY) <= 0.10 &&
                    on_support(hang.snapshot(), Simulation::kWetEDeckEntityId),
                "E hang: the cab carries the rider to the top as from the walk off");
    }

    // ---- 5. Loads, the rider not in E ----------------------------------------------------
    double empty_rest = 0.0;
    {
        Simulation tool(InitialSpawn::WetFPlatform);
        (void)tool.advance_frame(1.0);
        const std::uint32_t box = first_stock(tool, kStockToolbox);
        require(box != Simulation::kKitNone, "E loads: the yard's toolbox is found");
        tool.debug_launch_kit_body(box, kOverWetEDeck, {0.0, 0.0, 0.0});
        double tool_drop = 0.0;
        for (std::uint32_t tick = 0; tick < 5U * Simulation::kTickRateHz; ++tick) {
            (void)tool.advance_frame(Simulation::kFixedStepSeconds);
            tool_drop = std::max(tool_drop, tool.wet_state().e_deck_drop);
        }
        const auto w = tool.wet_state();
        std::cout << "  E loads: an 18 kg toolbox dropped on the deck took it " << tool_drop << " m down\n";
        require(w.e_catch_latched && std::abs(w.e_deck_drop) < 0.01 && std::abs(w.e_cab_travel) < 0.005,
                "E loads: an 18 kg toolbox dropped on the deck sets nothing off");

        Simulation drum(InitialSpawn::WetFPlatform);
        (void)drum.advance_frame(1.0);
        const std::uint32_t barrel = first_stock(drum, kStockDrum);
        require(barrel != Simulation::kKitNone, "E loads: a drum is found");
        drum.debug_launch_kit_body(barrel, kOverWetEDeck, {0.0, 0.0, 0.0});
        const bool released = wait_for(drum, 3.0, [&](const Snapshot &) { return !drum.wet_state().e_catch_latched; });
        require(released, "E loads: a 55 kg drum dropped on the deck sets the chiller off");
        require(wait_wet_e_top(drum, 40.0), "E loads: the cab blown up empty comes to rest at the top");
        empty_rest = kit_y(drum, Simulation::kWetEDeckEntityId);
        std::cout << "  E loads: the drum's ride rests at " << empty_rest << '\n';
        require(std::abs(empty_rest - up.rest_deck) <= 0.15,
                "E loads: empty, the cab rests within 0.15 m of where it rests with the rider");
    }

    // ---- 6. The doors held open, a load thrown in ------------------------------------------
    {
        Simulation held(InitialSpawn::Ring220North);
        require(ride_wet_d_up(held), "E jam: D carries the rider to 256.25");
        require(stand_in_wet_e_doorway(held) && held.wet_state().e_door_angle > 0.3,
                "E jam: the rider holds the leaves open in the doorway");
        const std::uint64_t hits0 = held.snapshot().hit_count;
        const std::uint32_t barrel = first_stock(held, kStockDrum);
        held.debug_launch_kit_body(barrel, kOverWetEDeck, {0.0, 0.0, 0.0});
        bool released = false;
        double highest = 0.0;
        // The car jammed under the doors: the cab, the deck, both leaves and
        // the trip lever. (The spec first asked every body in E to be still;
        // the 5 t chiller cannot be: hung on the air it has trapped, it bounces
        // on it as on a spring, and is held to what follows instead.)
        const std::uint64_t car[] = {Simulation::kWetECabEntityId, Simulation::kWetEDeckEntityId,
                                     Simulation::kWetEDoorEntityId, Simulation::kWetEEastLeafEntityId,
                                     Simulation::kWetELeverEntityId};
        double car_fastest = 0.0;
        // The chiller: within its travel, never onto a stop faster than
        // kWetEStopStrikeMps; each swing (half a bounce) peaking slower than the
        // one before.
        constexpr double kWetEStopStrikeMps = 0.5;
        const std::uint32_t chiller = held.kit_body_index(Simulation::kWetEChillerEntityId);
        double low = 0.0;
        double high = -1.0e9;
        double stop_strike = 0.0;
        std::vector<double> swing_peaks;
        std::vector<double> lows_t;
        double swing_peak = 0.0;
        double last_v = 0.0;
        double last_travel = 0.0;
        for (std::uint32_t tick = 0; tick < 18U * Simulation::kTickRateHz; ++tick) {
            (void)held.set_move_input(0.0, 0.0);
            (void)held.advance_frame(Simulation::kFixedStepSeconds);
            const auto w = held.wet_state();
            released = released || !w.e_catch_latched;
            highest = std::max(highest, kit_y(held, Simulation::kWetEDeckEntityId));
            const double v = held.kit_body_velocity(chiller).y;
            if (w.e_chiller_travel <= -49.99 || w.e_chiller_travel >= -0.001) {
                stop_strike = std::max(stop_strike, std::abs(v));
            }
            if (tick < 3U * Simulation::kTickRateHz) {
                last_v = v;
                last_travel = w.e_chiller_travel;
                continue;
            }
            for (const std::uint64_t entity : car) {
                const auto u = held.kit_body_velocity(held.kit_body_index(entity));
                car_fastest = std::max(car_fastest, std::sqrt(u.x * u.x + u.y * u.y + u.z * u.z));
            }
            low = std::min(low, w.e_chiller_travel);
            high = std::max(high, w.e_chiller_travel);
            swing_peak = std::max(swing_peak, std::abs(v));
            if ((last_v < 0.0) != (v < 0.0)) {
                // A swing ends where the chiller turns; a turn at the bottom
                // marks the bounce's period.
                swing_peaks.push_back(swing_peak);
                swing_peak = 0.0;
                if (last_v < 0.0) {
                    lows_t.push_back(tick * Simulation::kFixedStepSeconds);
                }
            }
            last_v = v;
            last_travel = w.e_chiller_travel;
        }
        (void)last_travel;
        bool decays = swing_peaks.size() >= 3;
        // The first swing counted may have begun before the window: from the
        // second on, each is slower than the last.
        for (std::size_t i = 2; i < swing_peaks.size(); ++i) {
            decays = decays && swing_peaks[i] < swing_peaks[i - 1];
        }
        const double period = lows_t.size() >= 2 ? (lows_t.back() - lows_t.front()) / static_cast<double>(lows_t.size() - 1)
                                                 : 0.0;
        std::cout << "  E jam: deck rose to " << highest << " m; after 3 s the car's fastest body moved " << car_fastest
                  << " m/s; the chiller bounced between " << low << " and " << high << " m of travel (amplitude "
                  << 0.5 * (high - low) << " m), period " << period << " s, swing peaks";
        for (const double peak : swing_peaks) {
            std::cout << ' ' << peak;
        }
        std::cout << " m/s; onto a stop at " << stop_strike << " m/s\n";
        require(released && highest <= 256.40,
                "E jam: the drum sets the chiller off, and the cab rises no higher than the open leaves");
        require(car_fastest < 0.2, "E jam: the car stays jammed under the doors: cab, deck, leaves and lever still");
        require(low > -50.0 && high < 0.0 && stop_strike <= kWetEStopStrikeMps,
                "E jam: the chiller bounces within its travel, never onto a stop faster than 0.5 m/s");
        require(decays, "E jam: the chiller's bounce dies away, each swing slower than the last");
        require(held.snapshot().hit_count == hits0, "E jam: nothing strikes the rider");
        require(walk_to(held, 13.2, -134.0, 6.0) && on_support(held.snapshot(), Simulation::kWetDPlatformEntityId),
                "E jam: the rider walks back onto D's platform");
    }

    // ---- 8. The world guard ----------------------------------------------------------------
    run_wet_e_guard(true);

    std::cout << "PASS scraperx_sim AS-007 E: landing_mps=" << up.land_speed
              << " release_s=" << up.release_t - up.land_t << " shut_margin_s=" << up.deck_door_t - up.shut_t
              << " launch_mps2=" << up.launch_accel << " top_mps2=" << up.later_accel << " peak_deck=" << up.peak_deck
              << " still_s=" << up.still_t - up.land_t << " rest_deck=" << up.rest_deck << " rest_60=" << rest_60
              << " empty_rest=" << empty_rest << " gain_J=" << gain << " released_J=" << released << '\n';
}
