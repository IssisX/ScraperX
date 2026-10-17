// The incline, the stone wheel, the teeter, the trapeze, landings, saves, the tram, determinism.

#include "sim_tests.hpp"

// The owner's slab incline, from the game's start: across the yard, up the
// ramp onto its boarding platform, onto the trolley. Nothing is pressed: the
// rider's weight on the yellow trip plate pulls the pin that holds the 2.5 t
// slab at the top of its tower, and the slab's fall hauls the trolley 30 m up
// the 60-degree incline. The only thing holding it back is the rider's weight
// on the brake treadle: held, the shoe stops the trolley and holds it on the
// incline; let off, it runs. Ridden with the brake, it arrives at the head
// slowly and the rider walks off onto deck 3; ridden without, it strikes the
// head at speed and the rider is thrown.
namespace {
using scraperx::sim::Simulation;
constexpr std::uint64_t kInclineDeck = scraperx::sim::vertical::Route::kInclineDynamicFirst;
constexpr std::uint64_t kInclineTrip = kInclineDeck + 1;
constexpr std::uint64_t kInclineBrake = kInclineDeck + 2;
constexpr double kOffBrakeM = 1.0;

scraperx::sim::Vector3 kit_entity_position(const Simulation &s, std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

// Steer toward a body that moves (a plate on the trolley), or a point
// `offset_x` west of it on the deck, for `ticks`.
void hold_on(Simulation &s, std::uint64_t entity, int ticks, double offset_x = 0.0) {
    for (int i = 0; i < ticks; ++i) {
        auto at = kit_entity_position(s, entity);
        at.x -= offset_x;
        const auto me = s.snapshot().player_position;
        const double dx = at.x - me.x, dz = at.z - me.z, d = std::hypot(dx, dz);
        const double gain = std::min(1.0, d / 0.3);
        (void)s.set_move_input(d > 0.05 ? gain * dx / d : 0.0, d > 0.05 ? gain * dz / d : 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
}

bool board_incline(Simulation &s) {
    (void)s.advance_frame(1.0);
    return walk_to(s, 6.0, -70.0, 20.0) && walk_to(s, -13.4, -86.0, 20.0) && walk_to(s, -13.4, -103.0, 12.0) &&
           s.advance_frame(0.5).accepted && s.lift_state().machine == 5 && s.lift_state().role == 2 &&
           walk_to(s, -10.0, -103.08, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 1;
}
}  // namespace

void run_incline() {
    using scraperx::sim::InitialSpawn;
    {
        Simulation idle(InitialSpawn::ExteriorGrade);
        (void)idle.advance_frame(20.0);
        require(std::abs(idle.lift_state().travels[5]) < 0.005 && idle.lift_state().incline_state == 0 &&
                    idle.lift_state().incline_brake_n < 100.0,
                "incline: left alone, the slab stays caught and the trolley at the foot");
    }
    // Held on the brake from the moment the pin is out: the shoe holds it.
    {
        Simulation held(InitialSpawn::ExteriorGrade);
        require(board_incline(held), "incline: across the yard, up the ramp, onto the trolley");
        hold_on(held, kInclineTrip, 90);
        require(held.lift_state().incline_state == 1, "incline: standing on the trip plate must pull the pin");
        hold_on(held, kInclineBrake, 360);
        const double stopped_at = held.lift_state().travels[5];
        hold_on(held, kInclineBrake, 180);
        const auto lift = held.lift_state();
        require(lift.incline_brake_n > 20000.0 && lift.incline_speed_mps < 0.01 && lift.travels[5] < 0.5 &&
                    std::abs(lift.travels[5] - stopped_at) < 0.001,
                "incline: standing on the brake treadle must stop the trolley and hold it on the incline");
    }
    // Ridden: on the brake whenever faster than the head allows, off it
    // otherwise; at the head, off onto its receiver and down onto deck 3.
    Simulation s(InitialSpawn::ExteriorGrade);
    require(board_incline(s), "incline: across the yard, up the ramp, onto the trolley");
    hold_on(s, kInclineTrip, 60);
    const double start = s.snapshot().simulation_time_seconds;
    double peak = 0.0, arrival = 0.0;
    const double length = scraperx::sim::vertical::Route::kInclineSlabTravel;
    for (int i = 0; i < 90 * 60; ++i) {
        const auto lift = s.lift_state();
        const double remaining = (1.0 - lift.travels[5]) * length;
        const double allowed = std::min(2.5, std::sqrt(2.0 * 0.8 * std::max(0.0, remaining - 0.5)) + 0.2);
        peak = std::max(peak, lift.incline_speed_mps);
        if (lift.travels[5] > 0.995 && lift.incline_speed_mps < 0.05) break;
        arrival = lift.incline_speed_mps;
        // Off the brake: a metre west of the treadle, clear of it on the deck.
        hold_on(s, kInclineBrake, 1, lift.incline_speed_mps > allowed ? 0.0 : kOffBrakeM);
    }
    const double ride_s = s.snapshot().simulation_time_seconds - start;
    require(s.lift_state().travels[5] > 0.995 && s.snapshot().death_count == 0,
            "incline: worked by the trip plate and the brake, the slab must haul the trolley to the head");
    require(peak < 5.0 && arrival < 1.5, "incline: ridden on the brake, the trolley must reach the head slowly");
    require(walk_to(s, -13.4, -120.4, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 3,
            "incline: off onto the head's receiver");
    require(walk_to(s, -13.4, -126.0, 8.0) && s.advance_frame(1.0).accepted,
            "incline: over the plate and down onto deck 3");
    const auto end = s.snapshot();
    require(end.death_count == 0 && end.player_grounded && end.player_position.y > 33.7 && end.player_position.y < 34.2,
            "incline: standing on deck 3, alive");
    // Unbraked: the slab drives the trolley into the head at speed.
    Simulation wild(InitialSpawn::ExteriorGrade);
    require(board_incline(wild), "incline: across the yard, up the ramp, onto the trolley");
    hold_on(wild, kInclineTrip, 60);
    double wild_peak = 0.0;
    for (int i = 0; i < 90 * 20; ++i) {
        wild_peak = std::max(wild_peak, wild.lift_state().incline_speed_mps);
        hold_on(wild, kInclineBrake, 1, kOffBrakeM);
    }
    require(wild_peak > 10.0 && (wild.snapshot().death_count > 0 || wild.snapshot().support_stumble_count > 0),
            "incline: ridden without the brake, the trolley must strike the head at speed and throw the rider");
    std::cout << "PASS scraperx_sim incline: ride_s=" << ride_s << " peak_mps=" << peak << " arrival_mps=" << arrival
              << " unbraked_peak_mps=" << wild_peak << " unbraked_deaths=" << wild.snapshot().death_count
              << " deck3_y=" << end.player_position.y << '\n';
}

// The owner's stone wheel, from deck 2: down the ramp to its lower receiver,
// into the bucket at the bottom, RAISE: the hopper's gate opens, stone pours
// into the bucket past the top, and the loaded side turns the 26 m wheel,
// carrying the rider's bucket round and up to the top (its motor only brakes
// and holds); off, and down the ramp onto deck 4.
void run_wheel() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    // Entities: the wheel's dynamic band starts at 2936 (wheel, striker,
    // gate, plank, then eight buckets); its statics at 1984 (frame, entry,
    // exit, hopper).
    constexpr std::uint64_t kPlank = 2936 + 3, kRiderBucket = 2936 + 4, kTopBucket = 2936 + 8;
    constexpr std::uint64_t kEntry = 1984 + 1, kExit = 1984 + 2;
    const auto plank_tilt = [](const Simulation &sim) {
        const auto q = sim.kit_body_rotation(sim.kit_body_index(kPlank));
        return 2.0 * std::asin(std::min(1.0, std::hypot(q.x, q.y, q.z)));
    };
    const auto mass_of = [](const Simulation &sim, std::uint64_t entity) {
        return sim.kit_body_mass(sim.kit_body_index(entity));
    };
    const auto bucket_y = [](const Simulation &sim) {
        return sim.kit_body_position(sim.kit_body_index(kRiderBucket)).y;
    };
    // Down from deck 2 onto the lower receiver, out on the plank for `seconds`,
    // back in, and into the bucket at the receiver.
    const auto feed_and_board = [&](Simulation &sim, double seconds) {
        return walk_to(sim, 14.19, -125.0, 10.0) && walk_to(sim, 14.19, -113.3, 12.0) &&
               sim.advance_frame(0.5).accepted && sim.snapshot().support_entity_id == kEntry &&
               walk_to(sim, 20.2, -113.3, 8.0) && sim.advance_frame(seconds).accepted &&
               walk_to(sim, 14.19, -113.3, 8.0) && walk_to(sim, 14.19, -110.4, 6.0) &&
               sim.advance_frame(0.5).accepted && sim.snapshot().support_entity_id == kRiderBucket;
    };

    // Too little stone: a second out on the plank pours about 40 kg, less than
    // the rider; the wheel does not move with the rider aboard.
    {
        Simulation s(InitialSpawn::Deck2South);
        (void)s.advance_frame(1.0);
        require(!s.request_lift_action(), "wheel: no RAISE; Action does nothing here");
        require(feed_and_board(s, 1.0), "wheel (light): fed briefly, boarded");
        const double y0 = bucket_y(s);
        (void)s.advance_frame(15.0);
        require(std::abs(bucket_y(s) - y0) < 0.3 && s.snapshot().support_entity_id == kRiderBucket,
                "wheel (light): too little stone must not lift the rider");
    }

    // A missed boarding: fed and released with nobody in the bucket, the wheel
    // carries the empty bucket up and brings the opposite one, emptied at the
    // striker, down to the receiver. Feeding again and boarding that one rides.
    {
        Simulation s(InitialSpawn::Deck2South);
        (void)s.advance_frame(1.0);
        const bool missed = walk_to(s, 14.19, -125.0, 10.0) && walk_to(s, 14.19, -113.3, 12.0) &&
                            walk_to(s, 20.2, -113.3, 8.0) && s.advance_frame(8.0).accepted &&
                            walk_to(s, 14.19, -113.3, 8.0) && s.advance_frame(60.0).accepted &&
                            bucket_y(s) > 48.5;
        require(missed, "wheel (miss): released unboarded, the empty bucket rides up to the top");
        constexpr std::uint64_t kOpposite = 2936 + 8;
        require(s.kit_body_position(s.kit_body_index(kOpposite)).y < 23.5 && mass_of(s, kOpposite) < 260.0,
                "wheel (miss): the opposite bucket, emptied, stands at the receiver");
        const bool out_again = walk_to(s, 20.2, -113.3, 8.0) && s.advance_frame(8.0).accepted;
        require(mass_of(s, kRiderBucket) > 450.0, "wheel (miss): fed again, stone fills the bucket now at the top");
        const bool back_in = out_again && walk_to(s, 14.19, -113.3, 8.0) && walk_to(s, 14.19, -110.4, 6.0) &&
                             s.advance_frame(0.5).accepted;
        const bool retried = back_in && s.snapshot().support_entity_id == kOpposite &&
                             wait_for(s, 120.0, [](const Snapshot &state) {
                                 return state.player_position.y > 47.0 && state.player_grounded &&
                                        std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.y) < 0.05;
                             });
        require(retried && s.snapshot().death_count == 0, "wheel (miss): fed again, the next bucket carries the rider up");
    }

    Simulation s(InitialSpawn::Deck2South);
    (void)s.advance_frame(1.0);
    require(walk_to(s, 14.19, -125.0, 10.0) && walk_to(s, 14.19, -113.3, 12.0) &&
                s.advance_frame(0.5).accepted && s.snapshot().support_entity_id == kEntry,
            "wheel: down the ramp from deck 2 onto the lower receiver");
    const double top_before = mass_of(s, kTopBucket);
    const double y_before = bucket_y(s);
    require(walk_to(s, 20.2, -113.3, 8.0), "wheel: out along the feed plank");
    (void)s.advance_frame(8.0);
    const double tilt = plank_tilt(s);
    const double poured = mass_of(s, kTopBucket) - top_before;
    require(tilt > 0.1, "wheel: the player's weight tips the feed plank onto its stop");
    require(poured > 200.0, "wheel: the tipped plank opens the hopper and stone fills the top bucket");
    require(std::abs(bucket_y(s) - y_before) < 0.05, "wheel: the tipped plank holds the loaded wheel");
    require(walk_to(s, 14.19, -113.3, 8.0) && walk_to(s, 14.19, -110.4, 6.0) && s.advance_frame(0.5).accepted &&
                s.snapshot().support_entity_id == kRiderBucket,
            "wheel: back in off the plank and into the bucket at the receiver");
    const double start = s.snapshot().simulation_time_seconds;
    double peak = 0.0;
    const bool up = wait_for(s, 150.0, [&](const Snapshot &state) {
        peak = std::max(peak, std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.y));
        return state.player_position.y > 47.0 && state.player_grounded &&
               std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.y) < 0.05;
    });
    const double ride_s = s.snapshot().simulation_time_seconds - start;
    if (!up) {
        const auto st = s.snapshot();
        std::cerr << "wheel ride: at (" << st.player_position.x << "," << st.player_position.y << ","
                  << st.player_position.z << ") poured=" << poured << " deaths=" << st.death_count << '\n';
    }
    require(up, "wheel: the stone must carry the rider's bucket over the top and stop it there");
    require(peak < 2.5, "wheel: the brake must hold the turn to its pace");
    require(walk_to(s, 17.81, -113.3, 6.0) && s.advance_frame(0.5).accepted && s.snapshot().support_entity_id == kExit,
            "wheel: off onto the upper receiver");
    // The reload: at the receiver's north-east corner the skip's trip line
    // hangs. Taken and pulled back, it opens the skip's gate and the skip
    // pours into the hopper, kilogram for kilogram; let go, the gate shuts.
    {
        using scraperx::sim::vertical::Route;
        const auto lift = [&s]() { return s.lift_state(); };
        const double hopper0 = lift().wheel_hopper_kg;
        const double skip0 = lift().wheel_skip_kg;
        require(skip0 > Route::kWheelSkipKg - 1.0, "wheel reload: the skip stands full");
        (void)s.advance_frame(3.0);
        require(std::abs(lift().wheel_skip_kg - skip0) < 1.0, "wheel reload: untouched, the skip keeps its stone");
        require(walk_to(s, 19.0, -112.9, 4.0, 0.08), "wheel reload: to the handle at the receiver's corner");
        (void)s.set_facing(0.0, 1.0);
        (void)s.advance_frame(0.3);
        require(s.request_pick_up() && s.advance_frame(0.5).accepted &&
                    s.snapshot().carrying_entity_id == Route::kWheelReloadHandleEntity,
                "wheel reload: the handle taken");
        // The gate's 0.6 rad stop lets the line out about 0.3 m: a step back
        // until it is taut; further would tear the handle from the hands.
        require(walk_to(s, 19.0, -113.25, 4.0, 0.06), "wheel reload: stepped back with it");
        (void)s.advance_frame(10.0);
        const double hopper1 = lift().wheel_hopper_kg;
        const double skip1 = lift().wheel_skip_kg;
        std::cout << "  wheel reload: hopper " << hopper0 << " -> " << hopper1 << " skip " << skip0 << " -> " << skip1
                  << '\n';
        require(hopper1 - hopper0 > 1000.0, "wheel reload: pulled, the skip refills the hopper");
        require(std::abs((hopper1 - hopper0) - (skip0 - skip1)) < 5.0,
                "wheel reload: every kilogram in the hopper came from the skip");
        (void)s.request_set_down();
        (void)s.advance_frame(2.0);
        const double skip2 = lift().wheel_skip_kg;
        (void)s.advance_frame(3.0);
        require(std::abs(lift().wheel_skip_kg - skip2) < 1.0, "wheel reload: let go, the gate shuts and the pour stops");
        require(walk_to(s, 17.81, -113.3, 4.0) && s.advance_frame(0.5).accepted &&
                    s.snapshot().support_entity_id == kExit,
                "wheel reload: back on the receiver");
    }
    require(walk_to(s, 17.81, -125.5, 12.0) && s.advance_frame(1.0).accepted, "wheel: down the ramp onto deck 4");
    const auto end = s.snapshot();
    require(end.death_count == 0 && end.player_grounded && end.player_position.y > 44.7 && end.player_position.y < 45.2,
            "wheel: standing on deck 4, alive");
    std::cout << "PASS scraperx_sim wheel: poured_kg=" << poured << " ride_s=" << ride_s << " peak_mps=" << peak
              << " deck4_y=" << end.player_position.y << '\n';
}

// A saved climb (release plan B5): after a real ride on the stone wheel (the
// wheel turned, stone poured and spilled), the save restores that world in a
// fresh simulation; restored from the same bytes, the original and the fresh
// one play on bit for bit; bytes cut short or from another build are refused
// and change nothing.
// Phase 4: balance at deck 4's south edge (44 m up, so going over is fatal).
// A run at the edge, the stick let go or pulled back at a set distance from
// the edge: the XCoM past it limits the brake to the soles' traction, so
// close in the body goes over, further out it stops, and between the two,
// pulling back saves it where letting go does not. Running off on purpose is
// not braked at all.
void run_teeter() {
    constexpr double kTeeterEntryMps = 6.0;
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    // Where the deck ends: walk off it and note the last supported z.
    double edge_z = 0.0;
    {
        Simulation sim(InitialSpawn::Deck4South);
        require(sim.set_facing(0.0, 1.0) && sim.set_move_input(0.0, 1.0), "inputs must be accepted");
        double last = sim.snapshot().player_position.z;
        require(advance_until(sim, [&](const Snapshot &s) {
                    if (s.player_grounded) last = s.player_position.z;
                    return s.death_count >= 1;
                }, 20.0),
                "walking south off deck 4 must fall");
        if (std::getenv("SCRAPERX_TEETER_DETAIL") != nullptr) std::cout << "  edge probe last_z=" << last << '\n';
        edge_z = last;
    }
    struct Outcome {
        bool fell;
        double stop_z;
        std::uint64_t teeters;
        int over_tick;               // first tick off the deck after the release
        double slowest_teeter_mps;   // lowest horizontal speed seen while teetering
        bool restore_clears = false;  // a load taken mid-teeter publishes no teeter
    };
    // Back 5.5 m from the edge, run at it, and at `release` metres short of
    // the edge set the stick to `stick_z`. The run-up is short: the sprint
    // arrives at its own 8 m/s, not yet gathered on into flow (run_flow).
    const auto attempt = [&](const double release, const double stick_z, const bool sprint,
                             const bool check_restore = false) {
        Simulation sim(InitialSpawn::Deck4South);
        require(sim.set_facing(0.0, -1.0) && sim.set_move_input(0.0, -1.0), "inputs must be accepted");
        require(advance_until(sim, [&](const Snapshot &s) { return s.player_position.z < edge_z - 5.5; }, 6.0),
                "the walk back must get there");
        require(sim.set_facing(0.0, 1.0) && sim.set_move_input(0.0, 1.0) && sim.set_sprint_input(sprint),
                "inputs must be accepted");
        require(advance_until(sim, [&](const Snapshot &s) { return s.player_position.z > edge_z - release; }, 6.0),
                "the run must reach the release point");
        require(sim.set_move_input(0.0, stick_z) && sim.set_sprint_input(false), "inputs must be accepted");
        Outcome out{false, 0.0, 0, -1, 1.0e9};
        for (int i = 0; i < 270; ++i) {
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
            const auto now = sim.snapshot();
            if (out.over_tick < 0 && !now.player_grounded && now.player_position.z > edge_z) {
                out.over_tick = i;
            }
            if (now.player_teetering) {
                out.slowest_teeter_mps = std::min(out.slowest_teeter_mps,
                                                  std::hypot(now.player_linear_velocity.x, now.player_linear_velocity.z));
            }
            if (now.player_teetering && check_restore) {
                // Loaded mid-teeter, the body comes back at rest on its
                // checkpoint, not balancing at an edge.
                const auto bytes = sim.save_game();
                require(sim.load_game(bytes.data(), bytes.size()), "a mid-teeter save must load");
                out.restore_clears = !sim.snapshot().player_teetering;
                return out;
            }
            // Pulling back only until the body has stopped.
            if (stick_z < 0.0 && sim.snapshot().player_linear_velocity.z <= 0.0) {
                require(sim.set_move_input(0.0, 0.0), "inputs must be accepted");
            }
        }
        out.fell = sim.snapshot().player_position.y < 44.0 || sim.snapshot().death_count > 0;
        out.stop_z = sim.snapshot().player_position.z;
        out.teeters = sim.snapshot().teeter_count;
        if (std::getenv("SCRAPERX_TEETER_DETAIL") != nullptr) {
            std::cout << "  release=" << release << " stick=" << stick_z << " sprint=" << sprint
                      << " fell=" << out.fell << " stop_z=" << out.stop_z << " teeters=" << out.teeters
                      << " over_tick=" << out.over_tick << " slowest_teeter=" << out.slowest_teeter_mps << '\n';
        }
        return out;
    };
    // At a sprint (8 m/s) the XCoM runs 2.56 m ahead; the soles need 3.9 m
    // to stop it even pulling back. The ground's own 22 m/s/s would stop it
    // in 1.45 m: so 2 m out only the teeter sends the body over.
    const auto sprint_let_go = attempt(2.0, 0.0, true);
    const auto sprint_pull = attempt(2.0, -1.0, true);
    const auto sprint_far = attempt(3.0, 0.0, true);
    const auto run_let_go = attempt(2.0, 0.0, false);
    const auto sprint_off = attempt(2.0, 1.0, true);
    require(sprint_let_go.fell && sprint_pull.fell && sprint_let_go.teeters >= 1,
            "sprinting within the XCoM's reach of the edge must teeter and go over");
    require(!sprint_far.fell && sprint_far.teeters == 0, "sprinting, let go further out, the body must stop");
    require(!run_let_go.fell && run_let_go.teeters == 0, "at a run, let go 2 m out, the body must stop");
    require(sprint_off.fell && sprint_off.teeters == 0, "running off on purpose must not be braked as a teeter");
    // Past the XCoM no braking saves the body (pulling back, the stop needs
    // v^2/2a > v/omega above 5.3 m/s): what pulling back buys is time.
    require(sprint_pull.over_tick > sprint_let_go.over_tick,
            "pulling back in a teeter must hold the body at the edge longer than letting go");
    // Entered at a sprint, the teeter lasts below its entry speed: the
    // ground's 22 m/s/s does not come back halfway.
    require(sprint_pull.slowest_teeter_mps < kTeeterEntryMps - 0.2,
            "a teeter entered at a sprint must persist below the entry speed");
    require(attempt(2.0, 0.0, true, true).restore_clears,
            "a climb loaded from a mid-teeter save must not come back teetering");
    std::cout << "PASS scraperx_sim teeter: edge_z=" << edge_z << " sprint_far_stop_z=" << sprint_far.stop_z
              << " run_stop_z=" << run_let_go.stop_z << '\n';
}

// Phase 3: the yard's trapeze. Up the ramp onto the launch stand, run off its
// edge and catch the hanging bar in the air, and swing: held still, the swing
// gains nothing and the ledger closes; pulled up through the bottom and let
// down at the ends, it grows; the other way round, it dies; let go near the
// far end of a grown swing, the body flies onto the landing stand, out of the
// reach of the swing the run gave.
void run_trapeze() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    struct Ride {
        double peak = 0.0;          // the furthest east after the first half swing
        double last_peak = 0.0;     // the latest swing's east end
        double clear_work = 0.0;    // the arms' net work while the swing stays short of the landing stand
        double start_peak = 0.0;    // the first half swing's
        double residual = 0.0;      // the worst ledger residual
        double arm_work = 0.0;
        double landed_x = 0.0;
        double landed_y = 0.0;
        bool landed = false;
        int pumps = 0;
        double catch_speed = 0.0;
        double catch_loss = 0.0;
        double peak_hand = 0.0;
        int slips = 0;
    };
    // pump: 0 held still, 1 in time, -1 against it. release_at: let go once
    // going east past this angle (0: never).
    const auto ride = [](const int pump, const double seconds, const double release_at) {
        Simulation sim(InitialSpawn::ExteriorGrade);
        require(sim.advance_frame(0.5).accepted, "settle must be accepted");
        require(walk_to(sim, 14.0, -28.0, 12.0) && walk_to(sim, 27.6, -28.0, 12.0, 0.1),
                "the walk up the ramp onto the launch stand must arrive");
        (void)sim.advance_frame(0.3);
        // The run east along the stand, the jump at its edge, and GRAB as
        // soon as the bar is in the hands' reach.
        require(sim.set_facing(1.0, 0.0) && sim.set_move_input(1.0, 0.0), "the run must be accepted");
        require(advance_until(sim, [](const Snapshot &s) { return s.player_position.x > 30.5; }, 3.0),
                "the run must reach the stand's edge");
        require(sim.request_jump(), "the jump must be accepted");
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
        require(sim.set_move_input(0.0, 0.0), "release must be accepted");
        bool caught = false;
        for (int i = 0; i < 90 && !caught; ++i) {
            if (sim.trapeze_state().catch_available) (void)sim.request_trapeze_action();
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
            caught = sim.trapeze_state().harnessed;
        }
        require(caught, "a running jump off the stand must bring the bar into reach and the hands must close on it");
        Ride out;
        bool first_half = true;
        const auto ticks = static_cast<int>(seconds * Simulation::kTickRateHz);
        for (int i = 0; i < ticks; ++i) {
            const auto t = sim.trapeze_state();
            if (!t.harnessed) break;
            const bool bottom = std::abs(t.angle_rad) < 0.12;
            const bool ends = std::abs(t.angular_speed) < 0.25;
            bool pull = t.pulled;
            if (pump == 1) {
                if (bottom) pull = true;
                else if (ends) pull = false;
            } else if (pump == -1) {
                if (ends) pull = true;
                else if (bottom) pull = false;
            }
            require(sim.set_crouch_input(pull), "crouch must be accepted");
            if (release_at > 0.0 && t.angle_rad > release_at && t.angular_speed > 0.0) {
                require(sim.request_trapeze_drop(), "letting go must be accepted");
            }
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
            const auto after = sim.trapeze_state();
            if (first_half && after.angular_speed < 0.0 && after.angle_rad > 0.0) {
                first_half = false;
                out.start_peak = after.peak_angle_rad;
            }
            out.peak = std::max(out.peak, after.angle_rad);
            if (t.angular_speed > 0.0 && after.angular_speed <= 0.0 && after.angle_rad > 0.0) {
                out.last_peak = after.angle_rad;   // the latest swing's east end
            }
            if (std::getenv("SCRAPERX_TRAPEZE_TRACE") != nullptr && i < 90 && i % 3 == 0) {
                const auto p = sim.snapshot();
                std::cout << "    t" << i << " angle=" << after.angle_rad << " w=" << after.angular_speed
                          << " pos=(" << p.player_position.x << "," << p.player_position.y << ") v=("
                          << p.player_linear_velocity.x << "," << p.player_linear_velocity.y << ") g="
                          << p.player_grounded << " in=" << after.pulled_in_m << " h=" << after.harnessed << '\n';
            }
            // Past 0.7 rad the feet can brush the landing stand's edge: a
            // contact the ledger does not count, so it is read short of it.
            if (after.peak_angle_rad < 0.7) {
                out.residual = std::max(out.residual, std::abs(after.energy_residual_j));
                out.clear_work = after.arm_work_j;
            }
            out.arm_work = after.arm_work_j;
            out.pumps = after.pumps;
            out.catch_speed = after.catch_speed_mps;
            out.catch_loss = after.catch_loss_j;
            out.peak_hand = std::max(out.peak_hand, after.peak_hand_force_n);
            out.slips = after.slips;
        }
        require(sim.set_crouch_input(false), "crouch must be accepted");
        if (release_at > 0.0) {
            require(advance_until(sim, [](const Snapshot &s) { return s.player_grounded || s.death_count > 0; }, 4.0),
                    "the flight must come down");
            (void)sim.advance_frame(0.5);
            const auto end = sim.snapshot();
            out.landed_x = end.player_position.x;
            out.landed_y = end.player_position.y;
            out.landed = end.player_grounded && end.death_count == 0 &&
                         end.player_position.y > scraperx::sim::Trapeze::kLandingTopY + 0.5;
        }
        if (std::getenv("SCRAPERX_TRAPEZE_DETAIL") != nullptr) {
            std::cout << "  trapeze pump=" << pump << " start_peak=" << out.start_peak << " peak=" << out.peak
                      << " last_peak=" << out.last_peak
                      << " residual_J=" << out.residual << " arm_work_J=" << out.arm_work << " pumps=" << out.pumps
                      << " landed=" << out.landed << " at=(" << out.landed_x << "," << out.landed_y << ")"
                      << " catch_mps=" << out.catch_speed << " catch_loss_J=" << out.catch_loss
                      << " peak_hand_N=" << out.peak_hand << " slips=" << out.slips << "\n";
        }
        return out;
    };
    const auto still = ride(0, 20.0, 0.0);
    const auto timed = ride(1, 40.0, 0.0);
    const auto against = ride(-1, 30.0, 0.0);
    const auto flown = ride(1, 60.0, 0.82);
    require(still.peak <= still.start_peak + 0.01 && still.pumps == 0,
            "held still, the swing must not grow past its first swing");
    require(still.residual < 60.0 && against.residual < 0.1 * std::abs(against.arm_work) + 60.0,
            "held still or pumped against the timing, the ledger must close");
    require(timed.peak > 0.8 && timed.clear_work > 0.0 && timed.residual < 0.1 * std::abs(timed.clear_work) + 60.0,
            "pulled up through the bottom and let down at the ends, the swing must grow, by the arms' work");
    require(against.last_peak < still.last_peak - 0.1, "pumped against its timing, the swing must die");
    require(flown.landed && flown.landed_x > scraperx::sim::Trapeze::kLandingWestX,
            "let go near the far end of a grown swing, the body must land on the landing stand");
    std::cout << "PASS scraperx_sim trapeze: still_peak=" << still.peak << " timed_peak=" << timed.peak
              << " against_peak=" << against.peak << " arm_work_J=" << timed.arm_work
              << " residual_J=" << timed.residual << " landed=(" << flown.landed_x << "," << flown.landed_y
              << ")\n";
}

// Phase 4: landing recovery and the roll, from the 12 m drop over the yard.
// One drop each way: straight down, forward and unprepared, forward with
// crouch held (a roll), forward with crouch held into an obstructed lane (a
// refused roll), across the facing (a stumble); then a jump landing (the legs
// absorb it), and the lethal drop with crouch held (still lethal).
void run_landing() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    struct Drop {
        double move_x, move_z, face_x, face_z;
        bool crouch;
    };
    struct Landed {
        Snapshot at;          // the landing tick
        Snapshot later;       // kRollSeconds on, input released at landing
        double travel_0_6 = 0.0;
    };
    const auto drop = [](const InitialSpawn spawn, const Drop d) {
        Simulation sim(spawn);
        require(sim.set_facing(d.face_x, d.face_z), "facing must be accepted");
        require(sim.set_move_input(d.move_x, d.move_z), "move must be accepted");
        require(sim.set_crouch_input(d.crouch), "crouch must be accepted");
        require(advance_until(sim, [](const Snapshot &s) { return s.landing_count >= 1 || s.death_count >= 1; }, 6.0),
                "the drop must land");
        Landed out;
        out.at = sim.snapshot();
        require(sim.set_move_input(0.0, 0.0), "release must be accepted");
        const auto start = out.at.player_position;
        for (int i = 0; i < 54; ++i) {
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
        }
        out.later = sim.snapshot();
        out.travel_0_6 = std::hypot(out.later.player_position.x - start.x, out.later.player_position.z - start.z);
        return out;
    };
    const auto show = [](const char *name, const Landed &l) {
        std::cout << "  " << name << ": response=" << int(l.at.landing_response)
                  << " vn=" << l.at.landing_normal_speed_mps << " vt=" << l.at.landing_tangent_speed_mps
                  << " loss=" << l.at.landing_balance_loss << " balance=" << l.at.recovery_balance
                  << " travel_0_6=" << l.travel_0_6 << " roll_travel=" << l.later.roll_travel_m
                  << " deaths=" << l.later.death_count << " refused=" << l.later.roll_refused_count
                  << " blocked=" << l.later.roll_blocked_count << " at=(" << l.at.player_position.x << ","
                  << l.at.player_position.z << ")\n";
    };
    const auto straight = drop(InitialSpawn::SurvivableDrop, {0, 0, 0, 1, false});
    const auto plain = drop(InitialSpawn::SurvivableDrop, {0, 1, 0, 1, false});
    const auto roll = drop(InitialSpawn::SurvivableDrop, {0, 1, 0, 1, true});
    const auto refused = drop(InitialSpawn::SurvivableDrop, {1, 0, 1, 0, true});
    const auto stumble = drop(InitialSpawn::SurvivableDrop, {0, 1, 1, 0, false});
    const auto lethal = drop(InitialSpawn::HighDrop, {0, 1, 0, 1, true});
    if (std::getenv("SCRAPERX_LANDING_DETAIL") != nullptr) {
        show("straight", straight);
        show("plain", plain);
        show("roll", roll);
        show("refused", refused);
        show("stumble", stumble);
        show("lethal", lethal);
    }

    require(straight.at.landing_response == 2 && straight.at.death_count == 0,
            "an unprepared 12 m drop must be a heavy, survivable landing");
    require(straight.at.recovery_balance < 0.6, "a heavy landing must cost balance");
    require(plain.at.landing_response == 2 && roll.at.landing_response == 3,
            "the same forward drop must roll only with crouch held");
    require(roll.at.recovery_balance > plain.at.recovery_balance + 0.3,
            "a roll must leave far less balance debt than a planted landing");
    require(roll.later.roll_travel_m > 2.0 && roll.travel_0_6 > 2.0 * plain.travel_0_6,
            "a roll must carry the run's momentum forward, a planted landing must not");
    require(roll.later.roll_progress == 0.0, "the roll must end within its time");
    require(refused.at.landing_response == 2 && refused.later.roll_refused_count == 1,
            "a roll into an obstructed lane must be refused, not driven through it");
    require(stumble.at.landing_response == 4 && stumble.later.stumble_count == 1,
            "a fast landing across the facing must stumble");
    require(stumble.at.landing_balance_loss > plain.at.landing_balance_loss &&
                stumble.later.recovery_balance < plain.later.recovery_balance,
            "a stumble must cost more balance, for longer, than the same landing facing the run");
    require(lethal.at.death_count == 1, "crouch and a run must not survive a lethal fall");

    // Control through recovery: forward input from the landing tick of the
    // straight drop reaches less speed in 0.25 s than on settled ground, and
    // the full speed once recovered.
    const auto speed_after = [](Simulation &sim, const int ticks) {
        require(sim.set_move_input(0.0, 1.0), "move must be accepted");
        for (int i = 0; i < ticks; ++i) {
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
        }
        const auto v = sim.snapshot().player_linear_velocity;
        require(sim.set_move_input(0.0, 0.0), "release must be accepted");
        return std::hypot(v.x, v.z);
    };
    Simulation heavy(InitialSpawn::SurvivableDrop);
    require(advance_until(heavy, [](const Snapshot &s) { return s.landing_count >= 1; }, 6.0), "must land");
    const double recovering = speed_after(heavy, 22);
    require(advance_until(heavy, [](const Snapshot &s) { return s.recovery_balance >= 1.0; }, 3.0),
            "balance must come back");
    for (int i = 0; i < 90; ++i) {
        require(heavy.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
    }
    const double settled = speed_after(heavy, 22);
    require(recovering > 0.5 && recovering < 0.8 * settled,
            "a heavy landing must reduce traction, not remove control");

    // A roll keeps the body low for its length, whatever crouch does.
    {
        Simulation sim(InitialSpawn::SurvivableDrop);
        require(sim.set_facing(0.0, 1.0) && sim.set_move_input(0.0, 1.0) && sim.set_crouch_input(true),
                "inputs must be accepted");
        require(advance_until(sim, [](const Snapshot &s) { return s.landing_count >= 1; }, 6.0), "must land");
        require(sim.snapshot().landing_response == 3, "the crouched forward drop must roll");
        require(sim.set_crouch_input(false), "release must be accepted");
        for (int i = 0; i < 18; ++i) {
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
        }
        require(sim.snapshot().roll_progress > 0.0 && sim.snapshot().player_crouched,
                "letting go of crouch mid-roll must not stand the body up");
    }

    // Debt is not forgiven by the next landing: a hop off a heavy landing
    // lands light (absorbed) and still owes what the heavy one left.
    {
        Simulation sim(InitialSpawn::SurvivableDrop);
        require(advance_until(sim, [](const Snapshot &s) { return s.landing_count >= 1; }, 6.0), "must land");
        const double owed = sim.snapshot().recovery_balance;
        require(advance_until(sim, [](const Snapshot &s) { return s.player_grounded; }, 1.0), "must stand");
        require(sim.request_jump(), "jump must be accepted");
        require(advance_until(sim, [](const Snapshot &s) { return s.landing_count >= 2; }, 3.0), "the hop must land");
        require(sim.snapshot().landing_response == 1 && sim.snapshot().recovery_balance < owed + 0.1,
                "a light landing must not wipe the debt a heavy one left");
    }

    // Phase 2: neutral air input keeps momentum. A running jump on the open
    // yard, the stick let go on the takeoff tick: at the apex the body still
    // carries its run (the old air control braked it to rest within 0.4 s).
    {
        Simulation sim(InitialSpawn::ExteriorGrade);
        require(sim.advance_frame(0.5).accepted, "settle must be accepted");
        require(sim.set_facing(0.0, -1.0) && sim.set_move_input(0.0, -1.0), "inputs must be accepted");
        require(sim.advance_frame(1.0).accepted, "the run must be accepted");
        const auto run = sim.snapshot();
        require(run.player_grounded, "the run must be on the ground");
        require(sim.request_jump(), "jump must be accepted");
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "takeoff must advance");
        require(sim.set_move_input(0.0, 0.0), "release must be accepted");
        const double takeoff = std::hypot(sim.snapshot().player_linear_velocity.x, sim.snapshot().player_linear_velocity.z);
        require(advance_until(sim, [](const Snapshot &s) { return s.player_linear_velocity.y <= 0.0; }, 1.0),
                "the jump must reach its apex");
        const double apex = std::hypot(sim.snapshot().player_linear_velocity.x, sim.snapshot().player_linear_velocity.z);
        require(takeoff > 5.0 && apex > 0.95 * takeoff, "a jump with the stick let go must keep its run");
        std::cout << "  air: takeoff_mps=" << takeoff << " apex_mps=" << apex << '\n';
    }

    // A standing jump's landing is the legs' own: no debt.
    Simulation hop(InitialSpawn::SurvivableDrop);
    require(advance_until(hop, [](const Snapshot &s) { return s.landing_count >= 1; }, 6.0), "must land");
    require(advance_until(hop, [](const Snapshot &s) { return s.recovery_balance >= 1.0; }, 3.0), "recovers");
    for (int i = 0; i < 45; ++i) {
        require(hop.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
    }
    require(hop.request_jump(), "jump must be accepted");
    require(advance_until(hop, [](const Snapshot &s) { return s.landing_count >= 2; }, 3.0), "the hop must land");
    require(hop.snapshot().landing_response == 1 && hop.snapshot().recovery_balance == 1.0,
            "an ordinary jump landing must be absorbed with no debt");

    std::cout << "PASS scraperx_sim landing: heavy_balance=" << straight.at.recovery_balance
              << " roll_balance=" << roll.at.recovery_balance << " roll_travel_m=" << roll.later.roll_travel_m
              << " planted_travel_m=" << plain.travel_0_6 << " recovering_mps=" << recovering
              << " settled_mps=" << settled << '\n';
}

void run_save() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    const auto world_hash = [](const Simulation &sim) {
        std::uint64_t h = 1469598103934665603ULL;
        const auto mix = [&h](double v) {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            h = (h ^ bits) * 1099511628211ULL;
        };
        const auto st = sim.snapshot();
        mix(st.player_position.x);
        mix(st.player_position.y);
        mix(st.player_position.z);
        for (std::uint32_t i = 0; i < sim.kit_body_count(); ++i) {
            const auto p = sim.kit_body_position(i);
            mix(p.x);
            mix(p.y);
            mix(p.z);
        }
        return h;
    };
    Simulation a(InitialSpawn::Deck2South);
    (void)a.advance_frame(1.0);
    const bool rode = walk_to(a, 14.19, -125.0, 10.0) && walk_to(a, 14.19, -113.3, 12.0) &&
                      walk_to(a, 20.2, -113.3, 8.0) && a.advance_frame(8.0).accepted &&
                      walk_to(a, 14.19, -113.3, 8.0) && walk_to(a, 14.19, -110.4, 6.0) &&
                      wait_for(a, 120.0, [](const scraperx::sim::Snapshot &state) {
                          return state.player_position.y > 47.0 && state.player_grounded &&
                                 std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.y) < 0.05;
                      }) &&
                      walk_to(a, 17.81, -113.3, 6.0) && a.advance_frame(1.0).accepted;
    require(rode, "save: ride the stone wheel to its upper receiver");
    const std::vector<std::uint8_t> bytes = a.save_game();
    const auto saved_at = a.snapshot().player_position;

    Simulation b(InitialSpawn::Deck2South);
    (void)b.advance_frame(0.5);
    const auto fresh = b.snapshot().player_position;
    std::vector<std::uint8_t> cut(bytes.begin(), bytes.end() - 3);
    std::vector<std::uint8_t> foreign = bytes;
    foreign[0] ^= 0x55;
    require(!b.load_game(cut.data(), cut.size()) && !b.load_game(foreign.data(), foreign.size()) &&
                b.snapshot().player_position.y == fresh.y,
            "save: bytes cut short or from another build are refused and change nothing");
    // A save from a world whose bodies stand in another order (a body
    // removed or moved since) is refused: its states would land on the wrong
    // bodies. Here two bodies' entities are swapped in the saved list.
    {
        const std::uint64_t first = b.kit_body_entity(0);
        const std::uint64_t second = b.kit_body_entity(1);
        std::vector<std::uint8_t> moved = bytes;
        bool swapped = false;
        for (std::size_t at = 0; !swapped && at + 16 <= moved.size(); ++at) {
            std::uint64_t one = 0, two = 0;
            std::memcpy(&one, moved.data() + at, 8);
            std::memcpy(&two, moved.data() + at + 8, 8);
            if (one == first && two == second) {
                std::memcpy(moved.data() + at, &second, 8);
                std::memcpy(moved.data() + at + 8, &first, 8);
                swapped = true;
            }
        }
        require(swapped && first != second && !b.load_game(moved.data(), moved.size()) &&
                    b.snapshot().player_position.y == fresh.y,
                "save: a save whose bodies stand in another order is refused and changes nothing");
    }
    require(b.load_game(bytes.data(), bytes.size()), "save: a fresh simulation loads the saved climb");
    const auto st = b.snapshot();
    require(std::abs(st.player_position.y - saved_at.y) < 0.3 &&
                std::hypot(st.player_position.x - saved_at.x, st.player_position.z - saved_at.z) < 0.3 &&
                st.death_count == a.snapshot().death_count,
            "save: the loaded climb stands where it was saved, its deaths uncounted");
    const auto wheel_bucket = [](const Simulation &sim) {
        return sim.kit_body_position(sim.kit_body_index(2936 + 4)).y;
    };
    require(wheel_bucket(b) > 48.5, "save: the loaded wheel stands as it was left, the first bucket at the top");

    // Two fresh simulations given the same save play on bit for bit (the
    // initial state determines the final state). The original, restored from
    // its own save, plays on within 1 mm of them: it keeps the solver's
    // history (its contacts' and ropes' last impulses), which a save does not
    // carry.
    Simulation c(InitialSpawn::Deck2South);
    (void)c.advance_frame(0.5);
    require(c.load_game(bytes.data(), bytes.size()) && a.load_game(bytes.data(), bytes.size()),
            "save: a second fresh simulation, and the original, load the same save");
    for (Simulation *sim : {&a, &b, &c}) {
        (void)walk_to(*sim, 17.81, -125.5, 12.0);
        (void)sim->advance_frame(3.0);
    }
    require(world_hash(b) == world_hash(c) && b.snapshot().tick_index == c.snapshot().tick_index,
            "save: two fresh simulations given the same save play on bit for bit");
    double worst = 0.0;
    std::uint64_t worst_entity = 0;
    for (std::uint32_t i = 0; i < a.kit_body_count(); ++i) {
        const auto pa = a.kit_body_position(i), pb = b.kit_body_position(i);
        const double d = std::hypot(pa.x - pb.x, pa.y - pb.y, pa.z - pb.z);
        if (d > worst) {
            worst = d;
            worst_entity = a.kit_body_entity(i);
        }
    }
    std::cout << "  save: the original's worst drift " << worst << " m (entity " << worst_entity << ")\n";
    const auto pa = a.snapshot().player_position, pb = b.snapshot().player_position;
    worst = std::max(worst, std::hypot(pa.x - pb.x, pa.y - pb.y, pa.z - pb.z));
    require(worst < 1e-3, "save: the original, restored from its own save, plays on with the fresh ones");
    std::cout << "PASS scraperx_sim save: bytes=" << bytes.size() << " saved_y=" << saved_at.y << " original_drift_m=" << worst
              << " deck4_y=" << b.snapshot().player_position.y << '\n';
}

// The traction tram from deck 6: east onto the east bridge, off its open
// end onto a landing, down the gangway onto the tram's lower receiver. The
// tram is a shuttle: two motors drive its 4 m wheels up the 35-degree track
// by their grip on it, 26 m up and 44 m west; it waits at each end, then
// leaves for the other. Nothing is pressed: the rider boards in its wait at
// the foot. Stayed aboard through its wait at the head, it carries the rider
// back down (the miss); ridden up again and stepped off in the wait, the
// rider walks along a plate to deck 8's east edge and down onto deck 8.
void run_tram() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    Simulation s(InitialSpawn::Deck6South);
    (void)s.advance_frame(1.0);
    // Along deck 6's east band west of C2's switchgear and duct, out east
    // along the bridge, off its open end onto the landing and down the gangway.
    bool ok = walk_to(s, 18.5, -128.0, 12.0) && walk_to(s, 18.5, -153.0, 12.0) && walk_to(s, 79.5, -153.0, 25.0) &&
              walk_to(s, 81.2, -153.0, 4.0) && walk_to(s, 81.1, -157.5, 6.0);
    const bool reached_gangway = ok;
    ok = ok && walk_to(s, 80.6, -161.0, 8.0) && s.advance_frame(0.5).accepted && s.lift_state().machine == 7 &&
         s.lift_state().role == 2;
    if (!ok) {
        const auto st = s.snapshot();
        std::cerr << "tram board: reached_gangway=" << reached_gangway << " at (" << st.player_position.x << "," << st.player_position.y
                  << "," << st.player_position.z << ") machine=" << s.lift_state().machine
                  << " role=" << s.lift_state().role << '\n';
    }
    require(ok, "tram: along the bridge, off its end, down the gangway onto its lower receiver");
    // It has been shuttling since the world began: wait on the receiver for
    // the carrier to stand at the foot with time left to walk on.
    const auto standing_at = [&s](double end, double seconds_left) {
        const auto lift = s.lift_state();
        return std::abs(lift.travels[7] - end) < 0.01 && lift.tram_departs_s >= seconds_left;
    };
    require(wait_for(s, 120.0, [&](const Snapshot &) { return standing_at(0.0, 3.0); }),
            "tram: the shuttle must come down and wait at the foot");
    require(walk_to(s, 74.0, -161.0, 6.0) && s.advance_frame(0.3).accepted && s.lift_state().role == 1,
            "tram: on board in its wait at the foot");
    (void)s.request_lift_action();   // Action does nothing to the shuttle
    const double start = s.snapshot().simulation_time_seconds;
    require(wait_for(s, 90.0, [&](const Snapshot &state) { return standing_at(1.0, 0.0) && state.player_grounded; }),
            "tram: the shuttle must carry the rider 26 m up its track");
    const double ride_s = s.snapshot().simulation_time_seconds - start;
    const double head_wait_s = s.lift_state().tram_departs_s;
    // The miss: stayed aboard, it leaves again and carries the rider down.
    require(wait_for(s, 10.0, [&s](const Snapshot &) { return s.lift_state().travels[7] < 0.9; }) &&
                s.lift_state().role == 1 && s.snapshot().death_count == 0,
            "tram: stayed aboard through its wait at the head, it must carry the rider back down");
    require(wait_for(s, 90.0, [&](const Snapshot &) { return standing_at(0.0, 0.0); }) && s.lift_state().role == 1,
            "tram: back at the foot, the rider still aboard");
    // The recovery: ride it up again and step off in its wait.
    require(wait_for(s, 120.0, [&](const Snapshot &state) { return standing_at(1.0, 0.0) && state.player_grounded; }),
            "tram: up again, the rider aboard");
    require(walk_to(s, 29.9, -161.0, 12.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 3,
            "tram: off onto its upper receiver in its wait");
    (void)walk_to(s, 23.5, -161.0, 6.0);
    (void)s.advance_frame(1.5);
    const auto end = s.snapshot();
    require(end.death_count == 0 && end.player_grounded && end.player_position.y > 88.7 && end.player_position.y < 89.2,
            "tram: down onto deck 8, alive");
    std::cout << "PASS scraperx_sim tram: ride_s=" << ride_s << " head_wait_s=" << head_wait_s
              << " deck8_y=" << end.player_position.y << '\n';
}

// The owner's law for this world (plan §2.6 rule 10): the initial state
// determines the final state. The same start and the same inputs, twice: up
// C5 on player inputs (walk, vault, crawl, sprint, leap, hang, climb), then O
// (carry, rope, wheel, pour, catches, ride) and off onto the 750 deck. The
// second run follows the first in the same process, so anything the first
// leaves behind (a static, a seed, an allocator's state) shows as a difference.
void run_determinism() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    const auto run = []() {
        Simulation simulation(InitialSpawn::Deck706);
        require(simulation.advance_frame(1.0).accepted, "determinism: the settle interval must be accepted");
        const auto start = world_bits(simulation);
        double freed = -1.0;
        require(climb_c5(simulation) && pull_o_handle(simulation, &freed) &&
                    ride_service_o(simulation, true, freed).settled && step_off_o(simulation),
                "determinism: each run must climb C5, ride O and step off onto the 750 deck");
        (void)simulation.advance_frame(2.0);
        const auto end = world_bits(simulation);
        require(end.size() == start.size() && end.size() > kClockAndPlayerWords &&
                    !std::equal(start.begin() + kClockAndPlayerWords, start.end(),
                                end.begin() + kClockAndPlayerWords),
                "determinism: the compared state must include the machines, which moved");
        return end;
    };
    const auto first = run();
    const auto second = run();
    require(first.size() == second.size(), "determinism: two runs must end with the same bodies and ropes");
    const auto differ = std::mismatch(first.begin(), first.end(), second.begin());
    if (differ.first != first.end()) {
        std::cerr << "determinism: first different word " << (differ.first - first.begin()) << " of "
                  << first.size() << '\n';
    }
    require(differ.first == first.end(),
            "determinism: the same start and the same inputs must end in the same state, bit for bit");
    std::cout << "PASS scraperx_sim determinism: words=" << first.size() << " ticks=" << first.front() << '\n';
}

void run_service_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::PlateTop640);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(rig_service_m(band) && pull_service_chock(band), "band: M hooked on and its reel let go");
    require(ride_service_m(band).settled, "band: M's cage on its dogs at the 662 deck");
    require(walk_to(band, -5.8, -143.5, 6.0), "band: off M's cage onto the 662 deck");
    (void)band.advance_frame(0.5);
    const auto deck662 = band.snapshot();
    require(standing_above(deck662, 662.5), "band: standing on the 662 deck");
    require(climb_c4(band), "band: up C4 to the 684 deck");
    const auto deck684 = band.snapshot();
    require(open_n_flap(band) && ride_service_n(band, true).settled, "band: N's flap open and its cage on its dogs");
    require(step_off_n(band), "band: off N's cage onto the 706 deck");
    const auto deck706 = band.snapshot();
    require(climb_c5(band), "band: up C5 to the 728 deck");
    const auto deck728 = band.snapshot();
    double band_freed = -1.0;
    require(pull_o_handle(band, &band_freed) && ride_service_o(band, true, band_freed).settled,
            "band: O's chock drawn and its cab on its dogs at the 750 deck");
    require(step_off_o(band), "band: off O's cab onto the 750 deck");
    const auto top = band.snapshot();
    require(top.death_count == 0, "band: no death from TP-640 to the 750 deck");
    std::cout << "PASS scraperx_sim AS-010 band: to_662_s=" << deck662.simulation_time_seconds - start
              << " to_684_s=" << deck684.simulation_time_seconds - start
              << " to_706_s=" << deck706.simulation_time_seconds - start
              << " to_728_s=" << deck728.simulation_time_seconds - start
              << " to_750_s=" << top.simulation_time_seconds - start << " deck_y=" << top.player_position.y << '\n';
}

// AS-010's climbing route, on player inputs: across TP-640 to the ladder,
// up through the 662 deck's hatch, and up the second ladder through the 684
// deck's, the lift where it was found.
void run_service_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation route(InitialSpawn::PlateTop640);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    require(walk_to(route, 8.9, -156.5, 12.0) &&
                climb_wet_hold(route, 8.9, -146.02, 0.0, 1.0, false, 662.5),
            "route: across TP-640 and up the ladder through the 662 deck's hatch");
    require(climb_wet_hold(route, -4.0, -146.02, 0.0, 1.0, false, 684.5),
            "route: across the 662 deck and up the ladder through the 684 deck's hatch");
    require(walk_to(route, -2.6, -142.5, 8.0) && walk_to(route, -2.6, -140.5, 4.0) &&
                climb_wet_hold(route, -3.5, -140.73, 0.0, -1.0, false, 706.5),
            "route: across the 684 deck, round the ladder and up it through the 706 deck's hatch");
    require(route.snapshot().player_position.z < -141.5,
            "route: off the 706 deck's ladder onto the open deck, south of its hatch");
    require(climb_wet_hold(route, -1.2, -147.83, 0.0, -1.0, false, 728.5),
            "route: onto the plant floor and up the ladder through the 728 deck's hatch");
    require(walk_to(route, -1.2, -157.43, 8.0) && climb_wet_hold(route, 10.7, -157.43, 0.0, -1.0, false, 750.5),
            "route: across the 728 deck and up the ladder through the 750 deck's hatch");
    const auto top = route.snapshot();
    const auto service = route.service_state();
    require(service.m_cage_travel < 0.02 && service.m_reel_latched && service.n_cage_travel < 0.02 &&
                service.n_gate_angle < 0.02 && service.o_cab_travel < 0.02 && service.o_ram_latched &&
                service.o_gate_latched,
            "route: the lifts did not move");
    std::cout << "PASS scraperx_sim AS-010 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lift_untouched=1\n";
}
