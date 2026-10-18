// The swing, C6, the cascade mast, the pitman lift, the helix, the derrick, the high ride.

#include "sim_tests.hpp"

// AS-012, the swing, on player inputs from the 220 ring: out the gangway into
// the seat, STRAP IN and KICK THE TRIP; the ram comes down and strikes the
// seat's buffer, the seat swings up to the 242 ring's edge and the rack holds
// it; UNBUCKLE and step off north onto the ring.
struct SwingRide final {
    bool boarded = false;
    bool idle_held = false;       // walked in, not strapped: the ram stays on its hook
    bool strapped = false;
    bool strapped_held = false;   // strapped, not kicked: nothing moves
    bool tripped = false;
    bool struck = false;
    double hit_after_kick_s = 0.0;
    double ram_speed_after_blow = 0.0;
    double apex_floor = 0.0;
    double held_floor = 0.0;
    double held_drift = 0.0;
    int tooth = -1;
    double peak_g = 0.0;
    double buffer_loss = 0.0;
    double residual = 0.0;
    bool on_242 = false;
    scraperx::sim::Snapshot end{};
};

bool swing_board(scraperx::sim::Simulation &s) {
    return walk_to(s, -13.35, -127.5, 20.0) && walk_to(s, -13.35, -98.8, 25.0) &&
           walk_to(s, -15.0, -98.85, 6.0, 0.08);
}

SwingRide swing_ride(scraperx::sim::Simulation &s) {
    SwingRide ride;
    ride.boarded = swing_board(s);
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(5.0);
    ride.idle_held = s.swing_state().ram_held && std::abs(s.swing_state().seat_angle_rad) < 0.002;
    (void)s.request_swing_action();
    (void)s.advance_frame(0.3);
    ride.strapped = s.swing_state().seated;
    const double ram_before = s.swing_state().ram_angle_rad;
    (void)s.advance_frame(5.0);
    ride.strapped_held = s.swing_state().ram_held && s.swing_state().ram_angle_rad == ram_before &&
                         std::abs(s.swing_state().seat_angle_rad) < 0.002;
    (void)s.request_swing_action();
    const auto kicked = s.snapshot().tick_index;
    ride.tripped = wait_for(s, 0.5, [&s](const scraperx::sim::Snapshot &) { return s.swing_state().tripped; });
    ride.struck = wait_for(s, 4.0, [&s](const scraperx::sim::Snapshot &) {
        return s.swing_state().peak_buffer_force_n > 0.0;
    });
    ride.hit_after_kick_s = double(s.snapshot().tick_index - kicked) / 90.0;
    (void)wait_for(s, 0.5, [](const scraperx::sim::Snapshot &) { return false; });
    ride.ram_speed_after_blow = s.swing_state().ram_speed_mps;
    (void)wait_for(s, 12.0, [&s](const scraperx::sim::Snapshot &) {
        const auto state = s.swing_state();
        return state.held_at_top && state.seat_speed_mps < 0.05 && state.seat_angle_rad > 1.2;
    });
    // Off the apex the seat drops back onto the last tooth it passed; from
    // there, held.
    (void)s.advance_frame(2.0);
    const auto top = s.swing_state();
    (void)s.advance_frame(5.0);
    const auto later = s.swing_state();
    ride.apex_floor = later.apex_floor_y;
    ride.held_floor = later.seat_floor_y;
    ride.held_drift = std::abs(later.seat_angle_rad - top.seat_angle_rad);
    ride.tooth = later.tooth;
    ride.peak_g = later.peak_seat_accel_mps2 / 9.81;
    ride.buffer_loss = later.buffer_loss_j;
    ride.residual = later.energy_residual_j;
    (void)s.request_swing_action();
    (void)s.advance_frame(0.3);
    ride.on_242 = !s.swing_state().seated && walk_to(s, -15.0, -129.3, 6.0);
    (void)s.advance_frame(1.0);
    ride.end = s.snapshot();
    return ride;
}

void run_swing() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    // Left alone, the ram stays on its hook and the seat hangs plumb.
    {
        Simulation s(InitialSpawn::Ring220North);
        (void)s.advance_frame(30.0);
        const auto idle = s.swing_state();
        require(idle.ram_held && !idle.tripped && std::abs(idle.seat_angle_rad) < 0.001 &&
                    std::abs(idle.ram_angle_rad + 1.44) < 0.001,
                "swing: left alone 30 s, the ram must stay on its hook and the seat hang plumb");
    }
    Simulation s(InitialSpawn::Ring220North);
    (void)s.advance_frame(1.0);
    const SwingRide ride = swing_ride(s);
    require(ride.boarded, "swing: the walk out the gangway into the seat");
    require(ride.idle_held, "swing: standing in the seat unstrapped must not let the ram go");
    require(ride.strapped, "swing: STRAP IN must harness the rider");
    require(ride.strapped_held, "swing: strapped in and not kicked, nothing must move");
    require(ride.tripped, "swing: KICK THE TRIP must let the ram go");
    require(ride.struck && ride.hit_after_kick_s > 2.0 && ride.hit_after_kick_s < 3.5,
            "swing: the ram must come down and strike the seat's buffer 2-3.5 s after the kick");
    require(ride.ram_speed_after_blow < 3.0,
            "swing: of one weight with the rider aboard, the ram must give the seat its swing and nearly stop");
    require(ride.apex_floor > 242.5, "swing: the seat's floor must swing up past the 242 ring's top");
    require(ride.tooth >= 0 && ride.held_floor > 242.25 && ride.held_floor < 243.1 && ride.held_drift < 0.0001,
            "swing: the rack must hold the seat at the 242 ring's edge, its floor at the deck or a step above");
    require(ride.peak_g < 20.0, "swing: the blow must stay under 20 g");
    // Energy pays for height: what the ram's fall gave the seat side, less what
    // the buffer, the rack's catch and the step lost. Nothing may come from
    // nowhere.
    const double gained = (150.0 + 85.0 + 100.0) * 9.81 * (ride.apex_floor - 220.30);
    require(ride.buffer_loss > 0.01 * gained && ride.buffer_loss < 0.10 * gained,
            "swing: the buffer must take a small share of the blow");
    require(ride.residual < 0.005 * gained && ride.residual > -0.03 * gained,
            "swing: the ledger must close (losses only, within 3% of the energy)");
    const auto &end = ride.end;
    require(ride.on_242 && end.death_count == 0 && end.player_grounded && end.support_entity_id == 51 &&
                end.player_position.y > 242.9 && end.player_position.y < 243.4 &&
                end.player_position.z > -131.64 && end.player_position.z < -127.64,
            "swing: UNBUCKLE and the step north must put the rider on the 242 ring");
    // The same start and inputs, the same end, to the bit.
    {
        Simulation again(InitialSpawn::Ring220North);
        (void)again.advance_frame(1.0);
        const SwingRide repeat = swing_ride(again);
        const auto a = world_bits(s);
        const auto b = world_bits(again);
        require(a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()),
                "swing: the same ride twice must end in the same state, bit for bit");
        (void)repeat;
    }
    // Mid-ride the harness stays shut: LEAVE is refused until the rack holds
    // the seat at rest.
    {
        Simulation locked(InitialSpawn::Ring220North);
        (void)locked.advance_frame(1.0);
        require(swing_board(locked), "swing harness: the walk into the seat");
        (void)locked.advance_frame(1.0);
        (void)locked.request_swing_action();
        (void)locked.advance_frame(0.3);
        (void)locked.request_swing_action();
        require(wait_for(locked, 8.0, [&locked](const scraperx::sim::Snapshot &) {
                    return locked.swing_state().seat_angle_rad > 0.5;
                }),
                "swing harness: the seat must swing up");
        require(!locked.request_swing_drop(), "swing harness: LEAVE must be refused mid-ride");
        (void)locked.advance_frame(0.2);
        require(locked.swing_state().seated, "swing harness: mid-ride the rider must stay in the harness");
    }
    // Stepped off the seat's open front at the gangway's end, the rider falls
    // and dies; the restore brings back the rider in the seat, the ram on its
    // hook, and the swing rides to the 242 ring.
    {
        Simulation fall(InitialSpawn::Ring220North);
        (void)fall.advance_frame(1.0);
        require(swing_board(fall), "swing restore: the walk into the seat");
        (void)fall.advance_frame(1.0);
        (void)walk_to(fall, -15.0, -101.5, 4.0);
        require(wait_for(fall, 20.0, [](const scraperx::sim::Snapshot &state) { return state.death_count == 1; }),
                "swing restore: off the seat's front, the rider must fall to their death");
        (void)fall.advance_frame(1.0);
        const auto restored = fall.swing_state();
        require(restored.ram_held && !restored.tripped && std::abs(restored.seat_angle_rad) < 0.01 &&
                    restored.station_available,
                "swing restore: the restore must bring back the rider in the seat and the ram on its hook");
        (void)fall.set_facing(0.0, -1.0);
        (void)fall.advance_frame(1.0);
        (void)fall.request_swing_action();
        (void)fall.advance_frame(0.3);
        (void)fall.request_swing_action();
        require(wait_for(fall, 15.0, [&fall](const scraperx::sim::Snapshot &) {
                    const auto state = fall.swing_state();
                    return state.held_at_top && state.seat_speed_mps < 0.05 && state.seat_angle_rad > 1.2;
                }),
                "swing restore: restored, the swing must ride again to the rack");
        (void)fall.request_swing_action();
        (void)fall.advance_frame(0.3);
        require(walk_to(fall, -15.0, -129.3, 6.0) && fall.snapshot().player_position.y > 242.9 &&
                    fall.snapshot().death_count == 1,
                "swing restore: and step off onto the 242 ring");
    }
    std::cout << "PASS scraperx_sim swing: hit_s=" << ride.hit_after_kick_s << " ram_after_mps="
              << ride.ram_speed_after_blow << " apex_floor=" << ride.apex_floor << " held_floor=" << ride.held_floor
              << " tooth=" << ride.tooth << " peak_g=" << ride.peak_g << " buffer_loss_kJ=" << ride.buffer_loss / 1000.0
              << " residual_kJ=" << ride.residual / 1000.0 << " gained_kJ=" << gained / 1000.0
              << " on_242_y=" << end.player_position.y << '\n';
}

// C6, the west band: from the 242 ring to the 264 ring on player inputs, no
// ladder and no standpipe. What the climb saw, for its falsifiers.
struct C6Notes final {
    bool leap_caught = false;     // the running leap off the kentledge ended in the hands
    bool balanced_out = false;    // the girder out over the void held the body on its line
    bool balanced_up = false;     // so did the girder rising to the north
};

bool report_c6(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cerr << "C6 stalled at " << leg << ": (" << state.player_position.x << ", " << state.player_position.y
              << ", " << state.player_position.z << ") grounded=" << state.player_grounded
              << " support=" << state.support_entity_id << '\n';
    return false;
}

bool climb_c6(scraperx::sim::Simulation &simulation, C6Notes *notes = nullptr) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    C6Notes seen;
    // 1. West along the 242 ring past the swing's jib posts, and north along
    // the band to the kentledge, and onto it.
    if (!(walk_to(simulation, -17.0, -130.8, 6.0) && walk_to(simulation, -20.9, -133.0, 6.0) &&
          walk_to(simulation, -20.9, -141.2, 6.0, 0.06))) {
        return report_c6(simulation, "to the kentledge");
    }
    if (!c4_mantle(simulation, 0.0, -1.0, 244.3, "C6 onto the kentledge")) {
        return false;
    }
    // 2. A run north along its top and, from its striped end, the leap over
    // the gap: the hands catch the platform's lip, and up.
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.2);
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.player_position.z <= -144.4; })) {
        return report_c6(simulation, "the run along the kentledge");
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        return report_c6(simulation, "the leap for the platform's lip");
    }
    seen.leap_caught = true;
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 247.8); })) {
        return report_c6(simulation, "up onto the platform");
    }
    (void)simulation.advance_frame(0.4);
    // 4. Out west along the girder over the void to its landing.
    if (!(walk_to(simulation, -21.6, -151.6, 4.0, 0.08) && walk_to(simulation, -23.5, -151.6, 4.0, 0.1))) {
        return report_c6(simulation, "onto the girder out");
    }
    seen.balanced_out = simulation.snapshot().player_balancing;
    if (!(walk_to(simulation, -29.6, -151.6, 8.0, 0.1) && walk_to(simulation, -30.1, -151.6, 3.0, 0.08))) {
        return report_c6(simulation, "out along the girder");
    }
    // 5. Up onto the outrigger's end, and back east along it.
    (void)walk_to(simulation, -29.3, -151.6, 2.0, 0.05);
    if (!c4_hang_up(simulation, 1.0, 0.0, 251.1, "C6 up onto the outrigger")) {
        return false;
    }
    if (!walk_to(simulation, -21.2, -151.6, 8.0, 0.1)) {
        return report_c6(simulation, "east along the outrigger");
    }
    // 6. North up the rising girder to the platform at its head.
    if (!walk_to(simulation, -21.2, -153.5, 4.0, 0.1)) {
        return report_c6(simulation, "onto the rising girder");
    }
    seen.balanced_up = simulation.snapshot().player_balancing;
    if (!walk_to(simulation, -21.2, -162.6, 10.0, 0.1)) {
        return report_c6(simulation, "up the rising girder");
    }
    // 7. Up onto the crossbeam's end, and east along it into the tower.
    if (!(walk_to(simulation, -19.75, -163.0, 4.0, 0.06) &&
          c4_hang_up(simulation, 1.0, 0.0, 258.4, "C6 up onto the crossbeam"))) {
        return false;
    }
    if (!(walk_to(simulation, -16.4, -163.0, 6.0, 0.1) && walk_to(simulation, -16.4, -162.8, 2.0, 0.05))) {
        return report_c6(simulation, "along the crossbeam");
    }
    // 8. Up onto the block, and from its west edge onto the 264 ring.
    if (!c4_hang_up(simulation, 0.0, -1.0, 261.4, "C6 up onto the block")) {
        return false;
    }
    if (!(walk_to(simulation, -16.9, -164.5, 3.0, 0.05) &&
          c4_hang_up(simulation, -1.0, 0.0, 264.7, "C6 up onto the 264 ring"))) {
        return false;
    }
    if (!walk_to(simulation, -19.5, -164.5, 3.0, 0.1)) {
        return report_c6(simulation, "onto the 264 ring");
    }
    if (notes != nullptr) {
        *notes = seen;
    }
    return true;
}

// A jump at a face from where the body stands, pushing in for 2 s: did it
// catch anything?
bool c6_caught(scraperx::sim::Simulation &simulation, const double fx, const double fz) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    return hold_stick(simulation, 0.4 * fx, 0.4 * fz, fx, fz, 2.0,
                      [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; });
}

void run_c6() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation simulation(InitialSpawn::Ring242South);
    require(simulation.advance_frame(1.0).accepted, "C6: the settle interval must be accepted");
    const double start = simulation.snapshot().simulation_time_seconds;
    C6Notes seen;
    require(climb_c6(simulation, &seen), "C6: the climb from the 242 ring to the 264 ring on player inputs");
    const auto top = simulation.snapshot();
    require(top.death_count == 0 && top.player_grounded && top.support_entity_id == 51 &&
                top.player_position.y > 264.9 && top.player_position.y < 265.4,
            "C6: the climb must end standing on the 264 ring, alive");
    require(seen.leap_caught && seen.balanced_out && seen.balanced_up,
            "C6: the leap must end in the hands, and the girders out and up must be walked on the balance");
    require(simulation.snapshot().wind_peak_force_n == 0.0,
            "C6 is below the windy face: no wind may push its climber");
    // Designed, not free: from the ring, a jump under the platform's fascia
    // catches nothing; the kentledge is the way up.
    {
        Simulation ring(InitialSpawn::Ring242South);
        (void)ring.advance_frame(1.0);
        // East of the kentledge, between it and the band's inner edge.
        require(walk_to(ring, -17.0, -130.8, 6.0) && walk_to(ring, -19.1, -133.0, 6.0) &&
                    walk_to(ring, -19.1, -148.6, 8.0) && walk_to(ring, -20.9, -148.9, 4.0, 0.08),
                "C6 falsifier: the walk round the kentledge to under the fascia");
        require(!c6_caught(ring, 0.0, -1.0) && ring.snapshot().player_position.y < 243.5,
                "C6 falsifier: from the ring, a jump under the platform's fascia must catch nothing");
    }
    // The run carries the leap: stood at the kentledge's edge, a jump pushing
    // north the whole way falls short, under the fascia, onto the band alive.
    {
        Simulation stand(InitialSpawn::Ring242South);
        (void)stand.advance_frame(1.0);
        require(walk_to(stand, -17.0, -130.8, 6.0) && walk_to(stand, -20.9, -133.0, 6.0) &&
                    walk_to(stand, -20.9, -141.2, 6.0, 0.06) &&
                    c4_mantle(stand, 0.0, -1.0, 244.3, "falsifier kentledge") &&
                    walk_to(stand, -20.9, -144.65, 3.0, 0.05),
                "C6 falsifier: to the kentledge's edge");
        (void)stand.set_facing(0.0, -1.0);
        (void)stand.advance_frame(0.5);
        (void)stand.request_jump();
        require(!hold_stick(stand, 0.0, -1.0, 0.0, -1.0, 2.0,
                            [](const scraperx::sim::Snapshot &state) {
                                return state.traversal_state == scraperx::sim::TraversalState::Hanging;
                            }),
                "C6 falsifier: a standing leap from the kentledge's edge must not reach the platform's lip");
        (void)stand.advance_frame(1.0);
        require(stand.snapshot().death_count == 0 && stand.snapshot().player_grounded &&
                    stand.snapshot().player_position.y < 243.5,
                "C6 falsifier: the short leap lands on the 242 ring alive");
    }
    std::cout << "PASS scraperx_sim C6: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " leap_caught=" << seen.leap_caught
              << " balanced_out=" << seen.balanced_out
              << " balanced_up=" << seen.balanced_up << '\n';
}

// The owner's vertical machines (MECHANISM_ASCENT_PLAN.md rule 12), from the
// 264 ring: the cascade mast. Its 110 kN slider raises the outer stage 9 m;
// one rope makes the inner stage rise twice that and a second makes the deck
// rise three times, 27 m, to its upper receiver at 291.25 m.
struct MastRide final {
    bool on_entry = false;
    bool on_deck = false;
    bool arrived = false;
    double ride_s = 0.0;
    double peak_speed = 0.0;
    double deck_y = 0.0;
    bool on_exit = false;
    scraperx::sim::Snapshot end{};
};

constexpr double kMastX = -10.65;      // the deck's centre
constexpr double kMastEntryX = -16.0;  // on the lower and upper receivers
constexpr double kMastZ = -150.0;

bool mast_settled_at(const scraperx::sim::Simulation &s, const double travel) {
    const auto lift = s.lift_state();
    return std::abs(lift.travels[0] - travel) < 0.005 && !lift.moving;
}

MastRide mast_ride(scraperx::sim::Simulation &s) {
    MastRide ride;
    ride.on_entry = walk_to(s, -19.5, kMastZ, 12.0) && walk_to(s, kMastEntryX, kMastZ, 6.0) &&
                    s.advance_frame(0.5).accepted && s.lift_state().machine == 0 && s.lift_state().role == 2;
    ride.on_deck = ride.on_entry && walk_to(s, kMastX, kMastZ, 6.0) && s.advance_frame(0.5).accepted &&
                   s.lift_state().role == 1;
    if (!ride.on_deck) {
        return ride;
    }
    (void)s.advance_frame(0.5);
    (void)s.request_lift_action();
    const double start = s.snapshot().simulation_time_seconds;
    ride.arrived = wait_for(s, 90.0, [&s, &ride](const scraperx::sim::Snapshot &state) {
        ride.peak_speed = std::max(ride.peak_speed, std::abs(state.player_linear_velocity.y));
        return mast_settled_at(s, 1.0) && state.player_grounded;
    });
    ride.ride_s = s.snapshot().simulation_time_seconds - start;
    ride.deck_y = s.snapshot().player_position.y;
    ride.on_exit = ride.arrived && walk_to(s, kMastEntryX, kMastZ, 6.0) && s.advance_frame(0.5).accepted &&
                   s.lift_state().role == 3;
    ride.end = s.snapshot();
    return ride;
}

void run_mast() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    // Left alone, the deck stays at its lower receiver.
    {
        Simulation idle(InitialSpawn::Ring264West);
        (void)idle.advance_frame(20.0);
        require(idle.lift_state().machine_count >= 1 && mast_settled_at(idle, 0.0),
                "mast: left alone 20 s, the deck must stay at its lower receiver");
    }
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    const MastRide ride = mast_ride(s);
    require(ride.on_entry, "mast: the walk along the 264 ring's west band onto the lower receiver");
    require(ride.on_deck, "mast: from the receiver across the gap onto the deck");
    require(ride.arrived && ride.ride_s > 15.0 && ride.ride_s < 60.0,
            "mast: Action on the deck must raise it 27 m to its upper receiver");
    require(ride.peak_speed < 2.0, "mast: the rider must ride the deck up at its drive's pace");
    require(ride.on_exit && ride.end.death_count == 0 && ride.end.player_grounded &&
                ride.end.support_entity_id == scraperx::sim::vertical::Route::kCascadeStaticFirst + 2 &&
                ride.end.player_position.y > 292.0 && ride.end.player_position.y < 292.4,
            "mast: and step off onto the upper receiver at 291.25 m, alive");
    // The upper receiver calls the deck; on it, Action takes it back down.
    {
        require(walk_to(s, kMastX, kMastZ, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 1,
                "mast return: back onto the deck");
        (void)s.request_lift_action();
        require(wait_for(s, 90.0, [&s](const scraperx::sim::Snapshot &state) {
                    return mast_settled_at(s, 0.0) && state.player_grounded;
                }),
                "mast return: Action on the deck at the top must take it back down");
        require(walk_to(s, kMastEntryX, kMastZ, 6.0) && s.advance_frame(0.5).accepted &&
                    s.snapshot().player_position.y < 265.4 &&
                    s.lift_state().role == 2,
                "mast return: and step off onto the lower receiver");
        (void)walk_to(s, -19.5, kMastZ, 6.0);
        require(s.lift_state().machine < 0 && !s.request_lift_action(),
                "mast: off every machine, Action must be refused");
    }
    // The same start and inputs, the same end, to the bit.
    {
        Simulation a(InitialSpawn::Ring264West);
        Simulation b(InitialSpawn::Ring264West);
        (void)a.advance_frame(1.0);
        (void)b.advance_frame(1.0);
        (void)mast_ride(a);
        (void)mast_ride(b);
        const auto wa = world_bits(a);
        const auto wb = world_bits(b);
        require(wa.size() == wb.size() && std::equal(wa.begin(), wa.end(), wb.begin()),
                "mast: the same ride twice must end in the same state, bit for bit");
    }
    // Off the deck's open east side mid-ride, the rider falls into the well
    // and dies; the restore brings back the rider on the deck where it was,
    // and the deck goes on up to its upper receiver.
    {
        Simulation fall(InitialSpawn::Ring264West);
        (void)fall.advance_frame(1.0);
        require(walk_to(fall, -19.5, kMastZ, 12.0) && walk_to(fall, kMastX, kMastZ, 8.0),
                "mast restore: onto the deck");
        (void)fall.advance_frame(0.5);
        (void)fall.request_lift_action();
        require(wait_for(fall, 30.0, [&fall](const scraperx::sim::Snapshot &) {
                    return fall.lift_state().travels[0] > 0.4;
                }),
                "mast restore: the deck must rise");
        (void)walk_to(fall, -4.0, kMastZ, 4.0);
        require(wait_for(fall, 20.0, [](const scraperx::sim::Snapshot &state) { return state.death_count == 1; }),
                "mast restore: off the deck's open side, the rider must fall to their death");
        // Restored where the rider last stood, at the deck's edge: step back in.
        (void)fall.advance_frame(0.1);
        require(fall.lift_state().role == 1 && walk_to(fall, kMastX, kMastZ, 4.0),
                "mast restore: the restore must bring the rider back on the deck, and they step back in");
        require(wait_for(fall, 90.0, [&fall](const scraperx::sim::Snapshot &state) {
                    return mast_settled_at(fall, 1.0) && state.player_grounded && state.player_position.y > 292.0;
                }),
                "mast restore: restored on the deck, the rider must ride on to the top");
    }
    std::cout << "PASS scraperx_sim mast: ride_s=" << ride.ride_s << " peak_mps=" << ride.peak_speed
              << " deck_y=" << ride.deck_y << " exit_y=" << ride.end.player_position.y << '\n';
}

// The second of the owner's vertical machines: the pitman lift outside the
// west face. Its 24 m crank turns through 160 degrees as the deck rises 23.6 m
// on the crank's 18 m rod. No motor: a counterweight on a rope over the head
// balances the deck and half a rider, and the rider hauls the rest by hand,
// a gun tackle from the deck (Route::pitman_step). Let go, the brake holds.
constexpr double kPitmanX = -23.5;
constexpr double kPitmanDeckZ = -154.3;
constexpr double kPitmanReceiverZ = -149.0;
constexpr double kPitmanTravelM = 314.89 - 291.25;   // receiver to receiver

bool pitman_ride(scraperx::sim::Simulation &s, double *ride_s) {
    if (!(walk_to(s, -19.5, -149.25, 6.0) && walk_to(s, kPitmanX, kPitmanReceiverZ, 6.0) &&
          s.advance_frame(0.5).accepted && s.lift_state().machine == 1 && s.lift_state().role == 2 &&
          walk_to(s, kPitmanX, kPitmanDeckZ, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        return false;
    }
    // RAISE does nothing to it.
    (void)s.request_lift_action();
    if (!s.advance_frame(1.0).accepted || s.lift_state().travels[1] > 0.005) {
        return false;
    }
    (void)s.set_lift_haul(true);
    const double start = s.snapshot().simulation_time_seconds;
    const bool up = wait_for(s, 90.0, [&s](const scraperx::sim::Snapshot &state) {
        const auto lift = s.lift_state();
        return std::abs(lift.travels[1] - 1.0) < 0.005 && state.player_grounded;
    });
    (void)s.set_lift_haul(false);
    *ride_s = s.snapshot().simulation_time_seconds - start;
    return up && walk_to(s, kPitmanX, kPitmanReceiverZ, 6.0) && s.advance_frame(0.5).accepted &&
           s.lift_state().role == 3;
}

void run_pitman() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    require(mast_ride(s).on_exit, "pitman: the cascade mast to its upper receiver");
    require(std::abs(s.lift_state().travels[1]) < 0.005,
            "pitman: left alone through the mast's ride, its deck must stay at its lower receiver");
    {
        // Hauled halfway and let go, the brake holds the rider where they are;
        // the hands never pull the deck faster than half their line speed.
        Simulation h(InitialSpawn::Ring264West);
        (void)h.advance_frame(1.0);
        require(mast_ride(h).on_exit && walk_to(h, -19.5, -149.25, 6.0) && walk_to(h, kPitmanX, kPitmanReceiverZ, 6.0) &&
                    walk_to(h, kPitmanX, kPitmanDeckZ, 6.0) && h.advance_frame(0.5).accepted &&
                    h.lift_state().role == 1,
                "pitman: onto its deck");
        (void)h.set_lift_haul(true);
        double fastest = 0.0;
        double pull = 0.0;
        for (int tick = 0; tick < 90 * 60 && h.lift_state().travels[1] < 0.5; ++tick) {
            require(h.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds).accepted, "haul tick");
            fastest = std::max(fastest, h.snapshot().support_point_linear_velocity.y);
            pull = std::max(pull, h.lift_state().pitman_haul_n);
        }
        require(h.lift_state().travels[1] >= 0.5, "pitman: hauling must lift the deck");
        require(fastest < 0.5 * scraperx::sim::vertical::Route::kHaulLineMps * 1.05,
                "pitman: the deck never outruns the hands");
        (void)h.set_lift_haul(false);
        require(h.advance_frame(0.5).accepted, "let go");
        const double held_at = h.lift_state().travels[1];
        require(h.advance_frame(4.0).accepted && std::abs(h.lift_state().travels[1] - held_at) < 0.004 &&
                    h.snapshot().player_grounded,
                "pitman: let go, the brake must hold the deck and its rider");
        std::cout << "  pitman haul: fastest_mps=" << fastest << " most_pull_n=" << pull << "\n";
    }
    // The two calls, each with the deck stepped off a little way from a
    // receiver (a rider cannot otherwise leave it short of one).
    const auto aboard = [](Simulation &c) {
        (void)c.advance_frame(1.0);
        return mast_ride(c).on_exit && walk_to(c, -19.5, -149.25, 6.0) && walk_to(c, kPitmanX, kPitmanReceiverZ, 6.0) &&
               walk_to(c, kPitmanX, kPitmanDeckZ, 6.0) && c.advance_frame(0.5).accepted && c.lift_state().role == 1;
    };
    const auto haul_to = [](Simulation &c, const double travel) {
        (void)c.set_lift_haul(true);
        for (int tick = 0; tick < 90 * 90 && c.lift_state().travels[1] < travel; ++tick) {
            (void)c.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
        }
        (void)c.set_lift_haul(false);
        return c.advance_frame(0.5).accepted && c.lift_state().travels[1] >= travel;
    };
    double fastest_call = 0.0;
    {
        // From the lower receiver: the deck 0.3 m up and empty, the brake
        // holds it; the call line, held, brings it down (2:1, through the
        // tackle), never faster than half the line's speed through the hands.
        Simulation c(InitialSpawn::Ring264West);
        require(aboard(c) && haul_to(c, 0.3 / kPitmanTravelM), "pitman call: the deck hauled 0.3 m up");
        require(walk_to(c, kPitmanX, kPitmanReceiverZ, 4.0) && c.advance_frame(0.5).accepted &&
                    c.lift_state().role == 2 && c.snapshot().player_grounded,
                "pitman call: stepped down off it onto the lower receiver");
        const double left_at = c.lift_state().travels[1];
        require(c.advance_frame(3.0).accepted && std::abs(c.lift_state().travels[1] - left_at) < 0.004,
                "pitman call: empty and let alone, the brake must hold the deck");
        (void)c.set_lift_haul(true);
        bool down = false;
        for (int tick = 0; tick < 90 * 15 && !down; ++tick) {
            const double before = c.lift_state().travels[1];
            (void)c.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
            fastest_call = std::max(fastest_call, (before - c.lift_state().travels[1]) * kPitmanTravelM /
                                                      scraperx::sim::Simulation::kFixedStepSeconds);
            down = c.lift_state().travels[1] < 0.002;
        }
        (void)c.set_lift_haul(false);
        require(down, "pitman call: held at the lower receiver, the call line must bring the deck down");
        require(fastest_call < 0.5 * scraperx::sim::vertical::Route::kHaulLineMps * 1.05,
                "pitman call: the deck never outruns the call line");
    }
    double fastest_free = 0.0;
    {
        // From the upper receiver: the deck left 0.24 m short of it and
        // empty; freeing the brake lets it rise, governed, to the top.
        Simulation c(InitialSpawn::Ring264West);
        require(aboard(c) && haul_to(c, 1.0 - 0.24 / kPitmanTravelM), "pitman free: the deck hauled to 0.24 m short");
        require(walk_to(c, kPitmanX, kPitmanReceiverZ, 4.0) && c.advance_frame(0.5).accepted &&
                    c.lift_state().role == 3 && c.snapshot().player_grounded,
                "pitman free: stepped up off it onto the upper receiver");
        const double left_at = c.lift_state().travels[1];
        require(c.advance_frame(3.0).accepted && std::abs(c.lift_state().travels[1] - left_at) < 0.004,
                "pitman free: empty and let alone, the brake must hold the deck");
        (void)c.set_lift_haul(true);
        double fastest = 0.0;
        bool up = false;
        for (int tick = 0; tick < 90 * 15 && !up; ++tick) {
            const double before = c.lift_state().travels[1];
            (void)c.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
            fastest = std::max(fastest, (c.lift_state().travels[1] - before) * kPitmanTravelM /
                                            scraperx::sim::Simulation::kFixedStepSeconds);
            up = c.lift_state().travels[1] > 0.998;
        }
        (void)c.set_lift_haul(false);
        fastest_free = fastest;
        require(up, "pitman free: freed from the upper receiver, the empty deck must rise to it");
        require(fastest < scraperx::sim::vertical::Route::kPitmanGovernedMps * 1.1,
                "pitman free: the brake governs the empty deck's rise");
    }
    double ride_s = 0.0;
    require(pitman_ride(s, &ride_s), "pitman: across the walkway, onto its deck, HAUL, and off at the top");
    const auto end = s.snapshot();
    require(end.death_count == 0 && end.player_grounded &&
                end.support_entity_id == scraperx::sim::vertical::Route::kPitmanStaticFirst + 2 &&
                end.player_position.y > 315.6 && end.player_position.y < 316.0 && ride_s > 15.0 && ride_s < 75.0,
            "pitman: standing on its upper receiver at 314.89 m, alive");
    std::cout << "PASS scraperx_sim pitman: ride_s=" << ride_s << " exit_y=" << end.player_position.y
              << " call_fastest_mps=" << fastest_call << " freed_fastest_mps=" << fastest_free << '\n';
}

// The third: the barrel helix west of the pitman. A 100 kNm motor turns a
// 12 m barrel whose spiral flight, 24 m to the turn, pushes a free roller on
// the deck's arm up its guide; the deck rises 24 m. From its upper receiver a
// walkway runs east to TP-340's edge, and a mantle puts the rider on the plate.
bool helix_ride(scraperx::sim::Simulation &s, double *ride_s) {
    if (!(walk_to(s, -27.0, -149.0, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().machine == 2 &&
          s.lift_state().role == 2 && walk_to(s, -30.8, -149.0, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().role == 1)) {
        return false;
    }
    (void)s.request_lift_action();
    const double start = s.snapshot().simulation_time_seconds;
    const bool up = wait_for(s, 120.0, [&s](const scraperx::sim::Snapshot &state) {
        const auto lift = s.lift_state();
        return std::abs(lift.travels[2] - 1.0) < 0.01 && !lift.moving && state.player_grounded;
    });
    *ride_s = s.snapshot().simulation_time_seconds - start;
    return up && walk_to(s, -27.0, -149.0, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 3;
}

void run_helix() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    double pitman_s = 0.0;
    require(mast_ride(s).on_exit && pitman_ride(s, &pitman_s), "helix: the mast and the pitman to 314.89 m");
    double ride_s = 0.0;
    const bool rode = helix_ride(s, &ride_s);
    if (!rode) {
        const auto st = s.snapshot();
        std::cerr << "helix: at (" << st.player_position.x << "," << st.player_position.y << ","
                  << st.player_position.z << ") travel=" << s.lift_state().travels[2] << " role="
                  << s.lift_state().role << " deaths=" << st.death_count << '\n';
    }
    require(rode, "helix: onto its deck, RAISE, 24 m up, and off at its upper receiver");
    require(s.snapshot().player_position.y > 339.6 && s.snapshot().player_position.y < 340.0,
            "helix: standing on its upper receiver at 338.89 m");
    require(walk_to(s, -15.2, -149.0, 8.0) && c4_mantle(s, 1.0, 0.0, 340.6, "TP-340"),
            "helix: along the walkway east and a mantle onto TP-340");
    (void)walk_to(s, -12.0, -149.0, 4.0);
    (void)s.advance_frame(1.0);
    const auto end = s.snapshot();
    require(end.death_count == 0 && end.player_grounded && end.player_position.y > 341.0 &&
                end.player_position.y < 341.5,
            "helix: standing on TP-340, alive");
    std::cout << "PASS scraperx_sim helix: ride_s=" << ride_s << " tp340_y=" << end.player_position.y
              << " support=" << end.support_entity_id << '\n';
}

// From the 750 deck to about 800 m: the crown gondola and the luffing derrick.
// The crown: a 400 kNm motor turns a 28 m, 7 t crown through 178 degrees,
// carrying a cabin hung from its rim south, up and over to its top. The
// derrick: a counterweight and the chain hanging from it draw in the cable
// that luffs a 32 m boom from 8 to 65 degrees, lifting its hung cradle 24.5 m
// up and 18 m in, once the cradle is light enough (see derrick_ride). Both
// carry their rider sideways as well as up. The cradle's hanger plate stands
// through its floor's middle, so the rider stands west of it.
struct HighRide final {
    bool vaulted = false;
    bool crown_up = false;
    double crown_s = 0.0;
    bool on_crown_exit = false;
    bool derrick_up = false;
    double derrick_s = 0.0;
    scraperx::sim::Snapshot end{};
};

bool ride_to_top(scraperx::sim::Simulation &s, const std::size_t index, double *ride_s) {
    (void)s.advance_frame(0.5);
    if (s.lift_state().role != 1 || static_cast<std::size_t>(s.lift_state().machine) != index) {
        return false;
    }
    (void)s.request_lift_action();
    const double start = s.snapshot().simulation_time_seconds;
    const bool up = wait_for(s, 120.0, [&s, index](const scraperx::sim::Snapshot &state) {
        const auto lift = s.lift_state();
        return std::abs(lift.travels[index] - 1.0) < 0.01 && !lift.moving && state.player_grounded;
    });
    *ride_s = s.snapshot().simulation_time_seconds - start;
    return up;
}

// The derrick from its lower receiver: onto the cradle, west of its hanger
// plate. Six iron pigs lie on it; with them and a rider it outweighs its
// counterweight and stays down. The rider throws four over its open west side,
// one by one (picked up, carried to the edge, let go); with two left and the
// rider it is lighter than the counterweight holds, and luffs up.
constexpr std::uint64_t kDerrickCradle = scraperx::sim::vertical::Route::kDerrickDynamicFirst + 1;
constexpr std::uint64_t kDerrickIronFirst = kDerrickCradle + 1;

scraperx::sim::Vector3 derrick_body(const scraperx::sim::Simulation &s, std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

// Steer to a point `dx`, `dz` from the cradle's centre, following it.
bool stand_on_cradle(scraperx::sim::Simulation &s, double dx, double dz, double seconds) {
    const int ticks = static_cast<int>(seconds * 90.0);
    for (int i = 0; i < ticks; ++i) {
        const auto c = derrick_body(s, kDerrickCradle);
        const auto me = s.snapshot().player_position;
        const double ex = c.x + dx - me.x, ez = c.z + dz - me.z, d = std::hypot(ex, ez);
        const double gain = std::min(1.0, d / 0.3);
        (void)s.set_move_input(d > 0.05 ? gain * ex / d : 0.0, d > 0.05 ? gain * ez / d : 0.0);
        (void)s.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    const auto c = derrick_body(s, kDerrickCradle);
    const auto me = s.snapshot().player_position;
    return std::hypot(c.x + dx - me.x, c.z + dz - me.z) < 0.25;
}

// Throws the pig `entity` over the cradle's open west side.
bool dump_iron(scraperx::sim::Simulation &s, std::uint64_t entity) {
    const auto c = derrick_body(s, kDerrickCradle);
    const auto pig = derrick_body(s, entity);
    // Stand just north of it on the cradle, face south, and pick it up.
    if (!stand_on_cradle(s, pig.x - c.x, pig.z - c.z + 0.6, 2.5)) return false;
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.2);
    if (s.snapshot().carry_target_entity_id != entity || !s.request_pick_up()) return false;
    (void)s.advance_frame(0.3);
    if (s.snapshot().carrying_entity_id != entity) return false;
    // To the west edge, face west, and let it go over the side.
    if (!stand_on_cradle(s, -1.6, 1.0, 2.5)) return false;
    (void)s.set_facing(-1.0, 0.0);
    (void)s.advance_frame(0.6);
    if (!s.request_set_down()) return false;
    (void)s.advance_frame(1.5);
    return s.snapshot().carrying_entity_id == 0;
}

struct DerrickRide final {
    bool held_with_iron = false;
    int dumped = 0;
    bool up = false;
    double ride_s = 0.0;
    bool held_at_three = false;
    int iron_left = 0;
};

DerrickRide derrick_ride(scraperx::sim::Simulation &s) {
    DerrickRide ride;
    if (!stand_on_cradle(s, -1.2, 0.0, 3.0)) return ride;
    (void)s.advance_frame(5.0);
    ride.held_with_iron = s.lift_state().travels[4] < 0.005 && s.lift_state().derrick_iron_aboard == 6;
    // Nearest the rider first: the pigs lie in two rows of three running
    // south from beside the rider's spot.
    for (const std::uint64_t pig : {kDerrickIronFirst + 2, kDerrickIronFirst + 5, kDerrickIronFirst + 1,
                                    kDerrickIronFirst + 4}) {
        if (ride.dumped == 3) {
            // Three left and the rider: what the counterweight holds.
            (void)stand_on_cradle(s, -1.2, 0.0, 5.0);
            ride.held_at_three = s.lift_state().travels[4] < 0.005 && s.lift_state().derrick_iron_aboard == 3;
        }
        if (!dump_iron(s, pig)) {
            std::cerr << "derrick: dumping iron " << pig << " failed at travel " << s.lift_state().travels[4]
                      << " target " << s.snapshot().carry_target_entity_id << " carrying "
                      << s.snapshot().carrying_entity_id << '\n';
            return ride;
        }
        ++ride.dumped;
    }
    ride.iron_left = s.lift_state().derrick_iron_aboard;
    const double start = s.snapshot().simulation_time_seconds;
    for (int i = 0; i < 90 * 90; ++i) {
        (void)stand_on_cradle(s, -1.2, 0.0, 1.0 / 90.0);
        const auto lift = s.lift_state();
        if (std::abs(lift.travels[4] - 1.0) < 0.01 && !lift.moving && s.snapshot().player_grounded) {
            ride.up = true;
            break;
        }
    }
    ride.ride_s = s.snapshot().simulation_time_seconds - start;
    return ride;
}

HighRide high_ride(scraperx::sim::Simulation &s) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    HighRide ride;
    // Over the 750 deck's east parapet, in a vault, onto the crown's receiver.
    if (!(walk_to(s, 10.8, -152.0, 8.0) && walk_to(s, 11.3, -152.0, 3.0, 0.06))) {
        return ride;
    }
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.3);
    (void)s.request_traversal();
    ride.vaulted = hold_stick(s, 0.0, 0.0, 1.0, 0.0, 2.0, [](const Snapshot &state) {
        return state.traversal_state == TraversalState::None && state.player_grounded &&
               state.player_position.x > 12.4;
    });
    if (!(ride.vaulted && walk_to(s, 13.5, -152.0, 4.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().machine == 3 && s.lift_state().role == 2 && walk_to(s, 17.3, -152.0, 6.0))) {
        return ride;
    }
    ride.crown_up = ride_to_top(s, 3, &ride.crown_s);
    ride.on_crown_exit = ride.crown_up && walk_to(s, 13.5, -151.5, 6.0) && s.advance_frame(0.5).accepted &&
                         s.lift_state().role == 3;
    if (!(ride.on_crown_exit && walk_to(s, 13.5, -156.5, 6.0) && s.advance_frame(0.5).accepted &&
          s.lift_state().machine == 4 && s.lift_state().role == 2 && walk_to(s, 12.3, -157.5, 4.0) &&
          walk_to(s, 12.3, -161.3, 6.0))) {
        ride.end = s.snapshot();
        return ride;
    }
    const DerrickRide d = derrick_ride(s);
    if (!(d.held_with_iron && d.held_at_three && d.dumped == 4 && d.iron_left == 2 && d.up)) {
        std::cerr << "derrick: held=" << d.held_with_iron << " held_at_three=" << d.held_at_three
                  << " dumped=" << d.dumped << " left=" << d.iron_left << " up=" << d.up << " ride_s=" << d.ride_s
                  << " travel=" << s.lift_state().travels[4] << '\n';
    }
    ride.derrick_s = d.ride_s;
    ride.derrick_up = d.held_with_iron && d.held_at_three && d.dumped == 4 && d.iron_left == 2 && d.up &&
                      walk_to(s, 13.5, -184.3, 8.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 3;
    ride.end = s.snapshot();
    return ride;
}

// The derrick's miss and its recovery, from the 750 deck over the crown. On
// the cradle the rider throws two pigs over; four and the rider still hold it
// down. Stepping off onto the lower receiver leaves the cradle lighter than
// the counterweight holds, and it luffs away empty. CALL from the receiver:
// its motor winds the counterweight up, brings the cradle down and holds it
// there until the rider stands on it. Two more over the side, and it rises.
void run_derrick() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation s(InitialSpawn::Deck750);
    (void)s.advance_frame(1.0);
    // high_ride's way to the derrick's lower receiver, stopping there.
    require(walk_to(s, 10.8, -152.0, 8.0) && walk_to(s, 11.3, -152.0, 3.0, 0.06),
            "derrick: to the 750 deck's east parapet");
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.3);
    (void)s.request_traversal();
    require(hold_stick(s, 0.0, 0.0, 1.0, 0.0, 2.0, [](const scraperx::sim::Snapshot &state) {
                return state.traversal_state == scraperx::sim::TraversalState::None && state.player_grounded &&
                       state.player_position.x > 12.4;
            }) && walk_to(s, 13.5, -152.0, 4.0) && walk_to(s, 17.3, -152.0, 6.0),
            "derrick: over the parapet onto the crown's cabin");
    double crown_s = 0.0;
    require(ride_to_top(s, 3, &crown_s) && walk_to(s, 13.5, -151.5, 6.0) && walk_to(s, 13.5, -156.5, 6.0) &&
                s.advance_frame(0.5).accepted && s.lift_state().machine == 4 && s.lift_state().role == 2,
            "derrick: up the crown and onto the derrick's lower receiver");
    require(walk_to(s, 12.3, -157.5, 4.0) && stand_on_cradle(s, -1.2, 0.0, 3.0) &&
                dump_iron(s, kDerrickIronFirst + 2) && dump_iron(s, kDerrickIronFirst + 5),
            "derrick: on the cradle, two pigs over the side");
    (void)stand_on_cradle(s, -1.2, 0.0, 3.0);
    require(s.lift_state().travels[4] < 0.005 && s.lift_state().derrick_iron_aboard == 4,
            "derrick: four pigs and the rider must still hold the cradle down");
    // The miss: off onto the receiver, and the cradle goes without the rider.
    require(walk_to(s, 12.3, -156.5, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 2,
            "derrick: off onto the lower receiver");
    require(wait_for(s, 30.0, [&s](const scraperx::sim::Snapshot &) { return s.lift_state().travels[4] > 0.1; }),
            "derrick: lightened and left empty, the cradle must luff away without its rider");
    const double gone = s.lift_state().travels[4];
    // The recovery: CALL from the receiver.
    (void)s.request_lift_action();
    require(wait_for(s, 90.0, [&s](const scraperx::sim::Snapshot &) {
                return s.lift_state().travels[4] < 0.005 && s.lift_state().derrick_holding;
            }),
            "derrick: CALL from the lower receiver must wind the cradle back down");
    (void)s.advance_frame(5.0);
    require(s.lift_state().travels[4] < 0.005 && s.lift_state().derrick_holding,
            "derrick: called down, its motor must hold the light cradle until a rider boards");
    require(walk_to(s, 12.3, -157.5, 4.0) && stand_on_cradle(s, -1.2, 0.0, 2.0) && !s.lift_state().derrick_holding,
            "derrick: a rider on the cradle frees it again");
    require(dump_iron(s, kDerrickIronFirst + 1) && dump_iron(s, kDerrickIronFirst + 4),
            "derrick: two more pigs over the side");
    bool up = false;
    for (int i = 0; i < 90 * 90 && !up; ++i) {
        (void)stand_on_cradle(s, -1.2, 0.0, 1.0 / 90.0);
        const auto lift = s.lift_state();
        up = std::abs(lift.travels[4] - 1.0) < 0.01 && !lift.moving && s.snapshot().player_grounded;
    }
    require(up, "derrick: lightened with its rider aboard, it must luff up to the top");
    // At the top the cradle beds in level: 2 s at the rider's spot, then
    // walked to its south edge over the pigs' side and stood there. Hung free
    // it swung up to 11 degrees either side of a 5.5 degree lean, and leaned
    // 14 as its rider stepped off: its edge 0.7 m under the receiver's top.
    const auto lean_deg = [&s] {
        const auto q = s.kit_body_rotation(s.kit_body_index(kDerrickCradle));
        return std::asin(std::clamp(2.0 * (q.x * q.y + q.w * q.z), -1.0, 1.0)) * 180.0 / 3.141592653589793;
    };
    double worst_lean = 0.0;
    for (int i = 0; i < 4 * 90; ++i) {
        (void)stand_on_cradle(s, -1.2, i < 2 * 90 ? 0.0 : -2.6, 1.0 / 90.0);
        worst_lean = std::max(worst_lean, std::abs(lean_deg()));
    }
    require(worst_lean < 2.5, "derrick: seated at the top, the cradle must stay level under a rider at its edge");
    require(walk_to(s, 13.5, -184.3, 8.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 3 &&
                s.snapshot().death_count == 0,
            "derrick: off the cradle's edge onto its upper receiver, alive");
    std::cout << "PASS scraperx_sim derrick: gone_empty_travel=" << gone << " top_lean_deg=" << worst_lean
              << " top_y=" << s.snapshot().player_position.y << '\n';
}

void run_high() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation s(InitialSpawn::Deck750);
    (void)s.advance_frame(1.0);
    const HighRide ride = high_ride(s);
    const auto &e = ride.end;
    if (!ride.derrick_up) {
        std::cerr << "high: vaulted=" << ride.vaulted << " crown_up=" << ride.crown_up << " crown_s=" << ride.crown_s
                  << " on_crown_exit=" << ride.on_crown_exit << " at (" << e.player_position.x << ","
                  << e.player_position.y << "," << e.player_position.z << ") travels=" << s.lift_state().travels[3]
                  << "," << s.lift_state().travels[4] << " role=" << s.lift_state().role << " deaths=" << e.death_count
                  << '\n';
    }
    require(ride.vaulted, "high: a vault over the 750 deck's east parapet onto the crown's receiver");
    require(ride.crown_up && ride.on_crown_exit, "high: the crown gondola's cabin to its upper receiver at 778.09 m");
    require(ride.derrick_up, "high: the derrick's cradle to its upper receiver at 802.64 m");
    require(e.death_count == 0 && e.player_grounded &&
                e.support_entity_id == scraperx::sim::vertical::Route::kDerrickStaticFirst + 2 &&
                e.player_position.y > 803.3 && e.player_position.y < 803.8,
            "high: standing on the derrick's upper receiver, about 800 m up, alive");
    require(e.summit_reached && e.summit_deaths == 0 && e.summit_seconds > 0.0,
            "high: the derrick's upper receiver must read as the route's top");
    // The crown gondola's cabin swings as the crown carries it over: the
    // rider standing in it is jolted past the ankles' reach and steps.
    require(e.support_stumble_count >= 1 && e.support_jolt_peak_m > 0.15,
            "high: the gondola's swinging cabin must jolt its rider into a stumble");
    std::cout << "PASS scraperx_sim high: crown_s=" << ride.crown_s << " derrick_s=" << ride.derrick_s
              << " top_y=" << e.player_position.y << " jolt_peak_m=" << e.support_jolt_peak_m
              << " jolt_stumbles=" << e.support_stumble_count << '\n';
}
