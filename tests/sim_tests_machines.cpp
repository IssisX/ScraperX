// AS-010 stage O, the gravel wheel, and the ground slingshot.

#include "sim_tests.hpp"

// ---- AS-010 stage O, the gravel wheel (728 -> 750 m) -------------------------------

constexpr double kOCabKg = 2400.0;
constexpr double kOBucketKg = 400.0;
constexpr double kOGravelKg = 498.0;
constexpr double kOChainFullKg = 16.0 * 45.0;
constexpr double kODeckTop = 728.3;         // the cab's floor at rest
constexpr double kO750Top = 750.1;
constexpr double kOStopTravel = 22.9;       // the guide's top, a dog's tooth
constexpr double kOLatchRelease = 0.15;     // the latch's angle at which the gate lets go
constexpr double kODrumRadius = 1.0;        // the wheel turns a radian for each metre the cab rises

// From the 728 deck into O's cab, to beside the handle hanging in it: face it
// and take hold. True once the handle is in the hands.
bool take_o_handle(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    if (!(walk_to(simulation, 8.0, -150.3, 10.0) && walk_to(simulation, 5.2, -150.4, 6.0) &&
          walk_to(simulation, 3.3, -150.6, 6.0, 0.06))) {
        return false;
    }
    (void)wait_for(simulation, 2.0, [](const Snapshot &state) { return state.player_grounded; });
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kServiceOHandleEntityId || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kServiceOHandleEntityId;
}

// Holding O's handle, step back until the chock lets the ram go, then let go
// of it. True once the ram is free; `freed_at` is the time it was.
bool pull_o_handle(scraperx::sim::Simulation &simulation, double *freed_at) {
    using scraperx::sim::Snapshot;
    if (!take_o_handle(simulation)) {
        return false;
    }
    const bool freed = hold_stick(simulation, 0.0, 0.35, 0.0, -1.0, 4.0, [&simulation](const Snapshot &) {
        return !simulation.service_state().o_ram_latched;
    });
    if (freed && freed_at != nullptr) {
        *freed_at = simulation.snapshot().simulation_time_seconds;
    }
    (void)simulation.request_set_down();
    return freed;
}

ORide ride_service_o(scraperx::sim::Simulation &simulation, const bool rider_aboard, const double ram_freed_at) {
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;
    constexpr double kPi = 3.14159265358979323846;
    ORide ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cab_y0 = kit_com_y(simulation, Simulation::kServiceOCabEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    double bucket_y = kit_com_y(simulation, Simulation::kServiceOBucketEntityId);
    double last = simulation.service_state().o_cab_travel;
    double wheel_last = simulation.service_state().o_wheel_angle;
    double wheel_turn = 0.0;
    double t_latch = -1.0;
    double t_gate = -1.0;
    double t_pour = -1.0;
    double t_move = -1.0;
    int still = 0;
    observe_path(simulation, false);
    for (int tick = 0; tick < 60 * 90; ++tick) {
        const double bucket_kg = simulation.service_state().o_bucket_mass;
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(simulation, true);
        const auto o = simulation.service_state();
        const double now = simulation.snapshot().simulation_time_seconds;
        const double now_y = kit_com_y(simulation, Simulation::kServiceOBucketEntityId);
        ride.released += bucket_kg * kG * (bucket_y - now_y);
        bucket_y = now_y;
        double turn = o.o_wheel_angle - wheel_last;
        turn = turn > kPi ? turn - 2.0 * kPi : (turn < -kPi ? turn + 2.0 * kPi : turn);
        wheel_last = o.o_wheel_angle;
        wheel_turn += turn;
        ride.wheel_gap = std::max(ride.wheel_gap, std::abs(wheel_turn - o.o_cab_travel / kODrumRadius));
        const double travel = o.o_cab_travel;
        const double speed = std::abs(travel - last) / Simulation::kFixedStepSeconds;
        last = travel;
        ride.apex = std::max(ride.apex, travel);
        ride.peak_speed = std::max(ride.peak_speed, speed);
        if (t_latch < 0.0 && o.o_latch_angle >= kOLatchRelease) {
            t_latch = now;
        }
        if (t_gate < 0.0 && !o.o_gate_latched) {
            t_gate = now;
        }
        if (t_pour < 0.0 && o.o_bin_kg < kOGravelKg - 1.0) {
            t_pour = now;
        }
        if (t_move < 0.0 && travel > 0.05) {
            t_move = now;
        }
        if (travel > 1.0 && travel < kOStopTravel - 2.0 && speed > 0.05) {
            ride.min_tension = std::min(ride.min_tension, o.o_rope_tension);
        }
        if (rider_aboard && travel > 0.05 && speed > 0.05) {
            ride.rode_on_cab = ride.rode_on_cab && on_support(simulation.snapshot(), Simulation::kServiceOCabEntityId);
        }
        still = travel > 1.0 && speed < 0.005 ? still + 1 : 0;
        if (still >= static_cast<int>(Simulation::kTickRateHz)) {
            ride.settled = true;
            break;
        }
    }
    const auto end = simulation.snapshot();
    ride.seconds = end.simulation_time_seconds - start;
    ride.in_order = ram_freed_at >= 0.0 && t_latch >= ram_freed_at && t_gate >= t_latch && t_pour >= t_gate &&
                    t_move >= t_pour;
    ride.gain = kOCabKg * kG * (kit_com_y(simulation, Simulation::kServiceOCabEntityId) - cab_y0);
    if (rider_aboard) {
        ride.gain += kRiderMassKg * kG * (end.player_position.y - rider_y0);
    }
    return ride;
}

// Off O's cab at the top, east onto the 750 deck.
bool step_off_o(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 5.2, -150.0, 6.0)) {
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto off = simulation.snapshot();
    return on_support(off, Simulation::kServiceFrameEntityId) && off.player_position.y > kO750Top + 0.5;
}

void run_service_o() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    constexpr double kG = 9.81;

    // ---- As found, nothing moves; a tug short of the chock's release lets it
    // fall back and no gravel moves --------------------------------------------------
    Simulation found(InitialSpawn::Deck728);
    (void)found.advance_frame(2.0);
    const auto as_found = found.service_state();
    require(as_found.o_ram_latched && as_found.o_gate_latched && std::abs(as_found.o_bin_kg - kOGravelKg) < 0.5 &&
                as_found.o_bucket_kg < 0.5 && std::abs(as_found.o_chain_kg - kOChainFullKg) < 0.5 &&
                as_found.o_cab_travel < 0.01 && as_found.o_bucket_travel < 0.01 &&
                std::abs(as_found.o_wheel_angle) < 0.01,
            "as found, O's ram is chocked and its gate latched shut on a full bin, its bucket empty at the top of the "
            "well on its whole chain, the cab down");
    const double found_pull = (kOBucketKg + kOChainFullKg) * kG;
    require(std::abs(as_found.o_rope_tension - found_pull) < 0.01 * found_pull,
            "as found, O's rope must carry the empty bucket and its chain");
    (void)found.advance_frame(10.0);
    const auto untouched = found.service_state();
    require(untouched.o_cab_travel < 0.01 && untouched.o_ram_latched && untouched.o_gate_latched &&
                std::abs(untouched.o_bin_kg - kOGravelKg) < 0.5,
            "untouched, O stays as found: the empty bucket and its chain are lighter than the cab");

    // Beside the cab on the deck, east, south, north and west of it, facing
    // it: the handle is out of reach.
    Simulation reach(InitialSpawn::Deck728);
    (void)reach.advance_frame(1.0);
    bool reachable = false;
    const auto look = [&reach, &reachable](const double fx, const double fz) {
        (void)reach.set_facing(fx, fz);
        (void)reach.advance_frame(0.5);
        reachable = reachable || reach.snapshot().carry_target_entity_id == Simulation::kServiceOHandleEntityId;
    };
    require(walk_to(reach, 4.85, -151.2, 20.0, 0.08), "to beside O's cab on the 728 deck, east");
    look(-1.0, 0.0);
    require(walk_to(reach, 4.85, -153.6, 6.0, 0.1) && walk_to(reach, 3.3, -153.6, 6.0, 0.1),
            "to beside O's cab on the 728 deck, south");
    look(0.0, 1.0);
    require(walk_to(reach, 0.9, -153.6, 6.0, 0.1) && walk_to(reach, 0.9, -148.2, 8.0, 0.1) &&
                walk_to(reach, 3.3, -148.2, 6.0, 0.1),
            "to beside O's cab on the 728 deck, north");
    look(0.0, -1.0);
    require(walk_to(reach, 0.9, -148.2, 6.0, 0.1) && walk_to(reach, 0.9, -151.2, 6.0, 0.1),
            "to beside O's cab on the 728 deck, west");
    look(1.0, 0.0);
    require(!reachable, "O's handle must be in reach only from inside the cab");

    Simulation tug(InitialSpawn::Deck728);
    (void)tug.advance_frame(1.0);
    require(take_o_handle(tug), "the rider must take hold of O's handle from inside the cab");
    (void)hold_stick(tug, 0.0, 0.35, 0.0, -1.0, 0.2, [](const Snapshot &) { return false; });
    (void)tug.request_set_down();
    (void)tug.advance_frame(10.0);
    const auto tugged = tug.service_state();
    require(tugged.o_ram_latched && std::abs(tugged.o_bin_kg - kOGravelKg) < 0.5 && tugged.o_cab_travel < 0.01,
            "a tug on O's handle short of the chock's release lets the chock fall back: the ram stays held and no "
            "gravel moves");

    // ---- The ride ---------------------------------------------------------------------
    Simulation o(InitialSpawn::Deck728);
    require(o.advance_frame(1.0).accepted, "O settle interval must be accepted");
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    double freed_at = -1.0;
    require(pull_o_handle(o, &freed_at), "the rider's pull must draw O's chock clear of the ram");
    const ORide ride = ride_service_o(o, true, freed_at);
    g_path_watch.armed = false;
    const auto top = o.snapshot();
    const auto held = o.service_state();
    require(held.o_bin_kg < 0.5 && std::abs(held.o_bucket_kg - kOGravelKg) < 0.5,
            "O's bin must pour its whole charge into the bucket");
    require(ride.in_order,
            "O must go in its order: the chock lets the ram go, the ram trips the latch, the gate falls open, the "
            "gravel pours, the cab rises");
    require(ride.settled, "O's cab must come to rest on its dogs");
    require(held.o_cab_travel >= kO750Top - 0.35 - kODeckTop && held.o_cab_travel <= kO750Top + 0.65 - kODeckTop &&
                ride.apex < kOStopTravel,
            "the gravel must lift O's cab to the 750 deck, a step from it either way, clear of its stop");
    require(ride.peak_speed <= 3.5, "O's cab must never pass 3.5 m/s: the chain sets the bucket's drive down as it goes");
    require(ride.wheel_gap <= 0.1,
            "O's wheel must turn with the cab it lifts, a radian for each metre it rises");
    require(ride.min_tension > 5000.0, "O's rope must stay taut all the way up");
    require(ride.rode_on_cab && on_support(top, Simulation::kServiceOCabEntityId),
            "the rider must ride O's cab all the way");
    require(ride.gain > 0.0 && ride.gain <= ride.released,
            "O's cab and rider must never gain more than the bucket, its gravel and its chain released");
    const double at_rest = held.o_bucket_mass * kG;
    require(std::abs(held.o_rope_tension - at_rest) < 0.02 * at_rest,
            "at rest, O's rope must carry the weight of the bucket, its gravel and the chain still hanging");
    require(g_path_watch.worst <= 0.15, "riding O, the body must never move more than 0.15 m in one tick");
    (void)o.advance_frame(10.0);
    require(std::abs(o.service_state().o_cab_travel - held.o_cab_travel) < 0.01,
            "with its rider aboard, O's cab must stay on its dogs");
    require(step_off_o(o), "the rider must step off O's cab onto the 750 deck");
    (void)o.advance_frame(10.0);
    require(std::abs(o.service_state().o_cab_travel - held.o_cab_travel) < 0.01,
            "empty, O's cab must stay on its dogs: the bucket, its gravel and the chain still hanging are lighter");
    require(o.snapshot().death_count == 0, "O must be ridden without a death");

    // ---- The chock let go, the rider steps out: the cab goes up alone -------------------
    Simulation empty(InitialSpawn::Deck728);
    (void)empty.advance_frame(1.0);
    double empty_freed = -1.0;
    require(pull_o_handle(empty, &empty_freed) && walk_to(empty, 5.2, -150.0, 3.0, 0.15) &&
                empty.service_state().o_cab_travel < 0.01,
            "the rider must be able to free O's ram and step out onto the 728 deck before the cab moves");
    const ORide alone = ride_service_o(empty, false, empty_freed);
    const auto alone_held = empty.service_state();
    // The top of the guide is a dog's tooth: an empty cab that reaches its
    // head is held there.
    require(alone.settled && alone.in_order && alone_held.o_cab_travel >= 21.1 &&
                alone_held.o_cab_travel <= kOStopTravel + 0.01 && empty.snapshot().player_position.y < 729.5 &&
                empty.snapshot().death_count == 0,
            "sent up empty, O's cab must run to its head and come to rest on its dogs, the rider left on the 728 deck");

    // ---- The decks' edges, the cab where it was found ---------------------------------
    // Walked into the rails round the well on the 728 deck, and, up the ladder
    // through the 750 deck's hatch, into the parapets round the 750 deck's four
    // edges, the well, and the cab's hole (open only on the east side, where the
    // cab lands, as N's cage lands over its west edge): a body stays up.
    Simulation deck(InitialSpawn::Deck728);
    (void)deck.advance_frame(1.0);
    require(walk_to(deck, 8.0, -150.3, 10.0) && c4_stays_up(deck, 6.0, -150.3, 0.0, 1.0, 728.1),
            "walked into the rail round O's well on the 728 deck, a body must stay up");
    require(climb_wet_hold(deck, 10.7, -157.43, 0.0, -1.0, false, 750.5),
            "O: up the ladder onto the 750 deck, for its edges");
    require(c4_stays_up(deck, 9.0, -160.4, 0.0, -1.0, kO750Top),
            "walked into the 750 deck's south parapet, a body must stay up");
    require(walk_to(deck, 9.0, -150.0, 8.0) && c4_stays_up(deck, 11.0, -150.0, 1.0, 0.0, kO750Top),
            "walked into the 750 deck's east parapet, a body must stay up");
    require(c4_stays_up(deck, 11.0, -143.6, 0.0, 1.0, kO750Top),
            "walked into the 750 deck's north parapet, a body must stay up");
    require(walk_to(deck, 3.0, -143.6, 8.0, 0.1) && c4_stays_up(deck, 3.0, -147.5, 0.0, -1.0, kO750Top),
            "walked into the parapet on the north side of O's cab hole, a body must stay up");
    require(walk_to(deck, 0.5, -147.5, 6.0, 0.1) && c4_stays_up(deck, 0.5, -150.9, 1.0, 0.0, kO750Top),
            "walked into the parapet on the west side of O's cab hole, a body must stay up");
    require(walk_to(deck, 0.5, -153.6, 6.0, 0.1) && walk_to(deck, 3.0, -153.6, 6.0, 0.1) &&
                c4_stays_up(deck, 3.0, -153.6, 0.0, 1.0, kO750Top),
            "walked into the parapet on the south side of O's cab hole, a body must stay up");
    require(c4_stays_up(deck, -0.8, -153.6, -1.0, 0.0, kO750Top),
            "walked into the 750 deck's west parapet, a body must stay up");
    require(deck.snapshot().death_count == 0, "O's decks must be walked without a death");

    std::cout << "PASS scraperx_sim AS-010 O: apex_m=" << ride.apex << " held_m=" << held.o_cab_travel
              << " peak_mps=" << ride.peak_speed << " seconds=" << ride.seconds << " rider_y=" << top.player_position.y
              << " gain_J=" << ride.gain << " released_J=" << ride.released << " wheel_gap_rad=" << ride.wheel_gap
              << " min_rope_N=" << ride.min_tension << " empty_apex_m=" << alone.apex
              << " empty_peak_mps=" << alone.peak_speed << " worst_step_m=" << g_path_watch.worst << '\n';
}

// AS-010 on player inputs from the TP-640 start: M hooked on, the chock
// pulled, the ride, off onto the 662 deck; up C4 to the 684 deck; N's flap
// open, the ride, off onto the 706 deck; up C5 to the 728 deck; O's chock
// drawn, the ride, off onto the 750 deck.
// The ground slingshot (sim/slingshot.hpp), played from the game's start: a
// walk to the pouch, ENTER, the stick held back to draw, RELEASE, and the
// chute opened over the tower with the stick steering onto a ring. Returns
// the end state; `apex` the highest the body went.
struct SlingshotShot final {
    bool boarded = false;
    bool held = false;
    double draw = 0.0;
    double energy = 0.0;
    double work = 0.0;
    double apex = 0.0;
    double peak_speed = 0.0;
    bool launched = false;
    bool flew = false;
    scraperx::sim::Snapshot end{};
};

SlingshotShot slingshot_shot(const double draw_seconds, const bool chute) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kElevation = 1.4311699866353502;
    constexpr double kRingX = -3.0;
    constexpr double kRingZ = -128.7;
    SlingshotShot shot;
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    // Round the pouch's east block to its low front lip, and step in.
    if (!(walk_to(s, 6.0, -40.0, 20.0) && walk_to(s, 6.0, -57.6, 20.0) && walk_to(s, -3.0, -57.6, 10.0, 0.1) &&
          walk_to(s, -3.0, -55.0, 6.0, 0.08))) {
        return shot;
    }
    (void)s.advance_frame(0.6);
    if (!s.slingshot_state().station_available) {
        return shot;
    }
    (void)s.set_slingshot_input(0.0, 0.0, kElevation);
    (void)s.request_slingshot_action();
    (void)s.advance_frame(0.2);
    shot.boarded = s.slingshot_state().seated;
    (void)s.set_slingshot_input(1.0, 0.0, kElevation);
    (void)s.advance_frame(draw_seconds);
    (void)s.set_slingshot_input(0.0, 0.0, kElevation);
    (void)s.advance_frame(0.1);
    const auto drawn = s.slingshot_state();
    shot.draw = drawn.draw_m;
    shot.energy = drawn.energy_j;
    shot.work = drawn.work_j;
    (void)s.advance_frame(1.0);
    shot.held = std::abs(s.slingshot_state().draw_m - shot.draw) < 0.03;
    (void)s.request_slingshot_action();
    bool opened = false;
    for (int tick = 0; tick < 90 * 30; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.snapshot();
        const auto &v = state.player_linear_velocity;
        shot.apex = std::max(shot.apex, state.player_position.y);
        shot.peak_speed = std::max(shot.peak_speed, std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
        shot.launched = shot.launched || s.slingshot_state().launch_count > 0;
        shot.flew = shot.flew || s.slingshot_state().flight;
        if (chute && !opened && v.y < 0.0 && state.player_position.z < -118.0 && state.player_position.y > 226.0) {
            (void)s.request_parachute();
            opened = true;
        }
        if (opened) {
            const double dx = kRingX - state.player_position.x;
            const double dz = kRingZ - state.player_position.z;
            const double l = std::hypot(dx, dz);
            const double k = std::min(1.0, l);
            (void)s.set_move_input(l > 0.05 ? dx / l * k : 0.0, l > 0.05 ? dz / l * k : 0.0);
        }
        if ((state.player_grounded && tick > 90) || state.death_count > 0) {
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.0);
    shot.end = s.snapshot();
    return shot;
}

void run_slingshot() {
    // A drawn shot with the chute opened over the tower lands on the 220
    // ring, where the route from grade arrives.
    const SlingshotShot shot = slingshot_shot(2.8, true);
    require(shot.boarded, "slingshot: walked from grade into the pouch, ENTER must harness the rider");
    require(shot.draw > 9.0 && shot.energy > 200000.0,
            "slingshot: 2.8 s of the stick held back must draw the pouch over 9 m and store over 200 kJ");
    require(shot.energy <= shot.work,
            "slingshot: the stretched rubber can hold no more than the draw put in");
    require(shot.held, "slingshot: drawn and let go of, the ratchet must hold the pouch where it is");
    require(shot.launched && shot.flew, "slingshot: RELEASE must throw the rider off the bands");
    require(shot.apex > 240.0 && shot.apex < 275.0, "slingshot: a 2.8 s draw must throw the rider 240-275 m up");
    require(shot.peak_speed < 90.0, "slingshot: the throw must stay under 90 m/s");
    const auto &end = shot.end;
    require(end.death_count == 0, "slingshot: chute and stick must bring the rider down alive");
    require(end.player_grounded && end.player_position.y > 221.0 && end.player_position.y < 221.3 &&
                end.player_position.z > -130.73 && end.player_position.z < -126.73,
            "slingshot: the rider must stand on the 220 ring");
    // Same start, same inputs, same landing, to the bit.
    const SlingshotShot again = slingshot_shot(2.8, true);
    require(std::memcmp(&again.end.player_position, &end.player_position, sizeof(end.player_position)) == 0 &&
                again.end.tick_index == end.tick_index,
            "slingshot: the same shot twice must land at the same place on the same tick");
    // Less draw, a lower throw; no chute from it, a lethal fall.
    const SlingshotShot short_shot = slingshot_shot(2.0, false);
    require(short_shot.apex > 80.0 && short_shot.apex < shot.apex - 80.0,
            "slingshot: a shorter draw must throw lower");
    require(short_shot.end.death_count == 1,
            "slingshot: a thrown rider who falls 100 m without the chute must die");
    // Used again: a short shot into the yard under the chute, the spent pouch
    // reeled back from the post beside it, and the pouch takes a rider again.
    {
        using scraperx::sim::InitialSpawn;
        using scraperx::sim::Simulation;
        constexpr double kElevation = 1.4311699866353502;
        Simulation s(InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        require(walk_to(s, 6.0, -40.0, 20.0) && walk_to(s, 6.0, -57.6, 20.0) && walk_to(s, -3.0, -57.6, 10.0, 0.1) &&
                    walk_to(s, -3.0, -55.0, 6.0, 0.08),
                "slingshot reuse: the walk into the pouch");
        (void)s.advance_frame(0.6);
        (void)s.set_slingshot_input(0.0, 0.0, kElevation);
        (void)s.request_slingshot_action();
        (void)s.advance_frame(0.2);
        (void)s.set_slingshot_input(1.0, 0.0, kElevation);
        (void)s.advance_frame(1.4);
        (void)s.set_slingshot_input(0.0, 0.0, kElevation);
        (void)s.advance_frame(0.3);
        (void)s.request_slingshot_action();
        bool opened = false;
        require(wait_for(s, 30.0, [&s, &opened](const scraperx::sim::Snapshot &state) {
                    if (!opened && !state.player_grounded && state.player_linear_velocity.y < 0.0 &&
                        s.slingshot_state().flight) {
                        (void)s.request_parachute();
                        opened = true;
                    }
                    return opened && state.player_grounded;
                }),
                "slingshot reuse: a short shot must come down in the yard under the chute");
        require(s.snapshot().death_count == 0 && s.slingshot_state().released,
                "slingshot reuse: down alive, the pouch spent");
        (void)s.advance_frame(6.0);
        const auto post = s.slingshot_state().retrieval_control_position;
        // Round the slingshot's frame on its east side, as a walker does:
        // where the chute comes down, the frame can stand between it and the
        // post, and a straight walk only leans on it.
        require(walk_to(s, 1.0, s.snapshot().player_position.z, 20.0, 0.3) && walk_to(s, 1.0, post.z - 0.6, 20.0, 0.3) &&
                    walk_to(s, post.x, post.z, 20.0, 0.5),
                "slingshot reuse: the walk round the frame to the post");
        (void)s.advance_frame(0.4);
        require(s.slingshot_state().can_retrieve, "slingshot reuse: at the post, RETRIEVE POUCH must be offered");
        (void)s.request_slingshot_action();
        (void)s.advance_frame(0.2);
        require(s.slingshot_state().recovering, "slingshot reuse: RETRIEVE must start the reel");
        (void)s.set_slingshot_input(1.0, 0.0, kElevation);
        require(wait_for(s, 30.0, [&s](const scraperx::sim::Snapshot &) { return !s.slingshot_state().recovering; }),
                "slingshot reuse: the stick held back must reel the pouch home");
        (void)s.set_slingshot_input(0.0, 0.0, kElevation);
        const auto home = s.slingshot_state();
        const auto rest = home.neutral_position;
        require(!home.released && std::hypot(home.pouch_position.x - rest.x, home.pouch_position.z - rest.z) < 0.05,
                "slingshot reuse: the reeled pouch must be back at rest, ready");
        require(walk_to(s, -3.0, -57.6, 10.0, 0.1) && walk_to(s, -3.0, -55.0, 6.0, 0.08), "slingshot reuse: back to the pouch");
        (void)s.advance_frame(0.6);
        require(s.slingshot_state().station_available, "slingshot reuse: the reeled pouch must take a rider again");
    }
    std::cout << "PASS scraperx_sim slingshot: draw_m=" << shot.draw << " energy_kJ=" << shot.energy / 1000.0
              << " work_kJ=" << shot.work / 1000.0 << " apex=" << shot.apex << " peak_mps=" << shot.peak_speed
              << " landed_y=" << end.player_position.y << " short_apex=" << short_shot.apex << '\n';
}

std::vector<std::uint64_t> world_bits(const scraperx::sim::Simulation &simulation) {
    std::vector<std::uint64_t> bits;
    const auto put = [&bits](const double value) {
        std::uint64_t word = 0;
        std::memcpy(&word, &value, sizeof(word));
        bits.push_back(word);
    };
    const auto put3 = [&put](const scraperx::sim::Vector3 &v) {
        put(v.x);
        put(v.y);
        put(v.z);
    };
    const auto state = simulation.snapshot();
    bits.push_back(state.tick_index);
    put(state.simulation_time_seconds);
    put3(state.player_position);
    put3(state.player_linear_velocity);
    bits.push_back(state.support_entity_id);
    bits.push_back(state.carrying_entity_id);
    bits.push_back(state.accepted_traversal_count);
    bits.push_back(state.death_count);
    for (std::uint32_t body = 0; body < simulation.kit_body_count(); ++body) {
        bits.push_back(simulation.kit_body_enabled(body) ? 1U : 0U);
        put3(simulation.kit_body_position(body));
        const auto q = simulation.kit_body_rotation(body);
        put(q.x);
        put(q.y);
        put(q.z);
        put(q.w);
        put3(simulation.kit_body_velocity(body));
        put(simulation.kit_body_mass(body));
    }
    std::array<scraperx::sim::Vector3, 8> points{};
    for (std::uint32_t cable = 0; cable < simulation.kit_cable_count(); ++cable) {
        const std::uint32_t n =
            simulation.kit_cable_points(cable, points.data(), static_cast<std::uint32_t>(points.size()));
        bits.push_back(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            put3(points[i]);
        }
    }
    for (std::uint32_t bin = 0; bin < simulation.kit_bin_count(); ++bin) {
        put(simulation.kit_bin(bin).contents_kg);
    }
    return bits;
}
