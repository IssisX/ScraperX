// Band 0, the Stack, from the yard to deck 14.

#include "sim_tests.hpp"

void run_stack() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;

    // The yard's kerb is 0.5 m wide and 86 m long, but a step high: it is
    // walked on and off, not balanced along. Held on its line as a beam, a
    // body on it could not step down off it with a gentle stick.
    Simulation kerb(InitialSpawn::ExteriorGrade);
    require(kerb.advance_frame(0.5).accepted, "kerb settle interval must be accepted");
    require(walk_to(kerb, 20.0, -117.0, 40.0) && walk_to(kerb, 20.0, -118.0, 6.0, 0.08),
            "the walk must reach the top of the yard's kerb");
    const auto on_kerb = kerb.snapshot();
    for (std::uint32_t tick = 0; tick < 90; ++tick) {
        (void)kerb.set_move_input(0.0, 0.35);
        (void)kerb.set_facing(0.0, -1.0);
        (void)kerb.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto off_kerb = kerb.snapshot();
    require(on_kerb.player_position.y > 1.1 && !on_kerb.player_balancing,
            "standing on the yard's kerb must not be balancing");
    require(off_kerb.player_position.z > on_kerb.player_position.z + 0.4 &&
                off_kerb.player_position.y < 0.95,
            "a gentle stick must step down off the yard's kerb");
    std::cout << "PASS scraperx_sim kerb: on_y=" << on_kerb.player_position.y
              << " stepped_z=" << off_kerb.player_position.z - on_kerb.player_position.z << "\n";

    // A knee-to-waist edge is climbed. The owner, on the device: stuck where
    // "I wasn't able to reach up and climb - buckets and shit". The wall ray
    // ran only at the body's centre (0.9 m), so a rim from 0.35 to 0.9 m
    // offered nothing: too high to step, invisible to the climb. The
    // slingshot's 0.7 m timber block in the yard is one.
    {
        Simulation low(InitialSpawn::ExteriorGrade);
        (void)low.advance_frame(0.5);
        require(walk_to(low, 0.0, -49.0, 30.0) && walk_to(low, 0.0, -51.55, 4.0, 0.05),
                "the walk to the slingshot's timber block must be accepted");
        (void)low.set_facing(0.0, -1.0);
        (void)low.advance_frame(0.4);
        require(low.snapshot().ledge_available, "a 0.7 m edge ahead must offer CLIMB");
        (void)low.request_traversal();
        require(advance_until(low, [](const scraperx::sim::Snapshot &state) { return standing_above(state, 1.5); }, 2.0),
                "CLIMB must put the body on top of a 0.7 m block");
        std::cout << "PASS scraperx_sim low edge: top_y=" << low.snapshot().player_position.y << "\n";
    }

    // Left alone, S1 waits as found: the plate proud of the floor on its line,
    // the lever on its stop, the valve shut, the bucket dry on its catch, the
    // cage at grade.
    Simulation idle(InitialSpawn::ExteriorGrade);
    require(idle.advance_frame(30.0).accepted, "S1 idle interval must be accepted");
    const auto idle_state = idle.stack_state();
    require(idle_state.s1_valve_angle < 0.005 && idle_state.s1_bucket_water_kg < 0.01 &&
                idle_state.s1_catch_latched && std::abs(idle_state.s1_cage_travel) <= 0.01 &&
                s1_plate_at_rest(idle),
            "left alone for 30 s, S1 must wait as found");

    // (a) The plate is what starts it, and it does not wait. Stood in the cage
    // off the plate, a rider weighs nothing on the line: nothing moves.
    Simulation off_plate(InitialSpawn::ExteriorGrade);
    require(off_plate.advance_frame(0.5).accepted, "S1 off-plate settle interval must be accepted");
    // In at a creep, so as to stop on the strip of floor south of the plate.
    require(walk_to(off_plate, 10.0, -117.3, 30.0) && walk_to(off_plate, 10.0, -118.6, 6.0, 0.08) &&
                hold_stick(off_plate, 0.0, -0.3, 0.0, -1.0, 4.0,
                           [](const scraperx::sim::Snapshot &state) { return state.player_position.z <= -119.55; }),
            "the player must stand in S1's cage, south of its plate");
    (void)off_plate.advance_frame(5.0);
    const auto off_state = off_plate.stack_state();
    require(off_plate.snapshot().support_entity_id == Simulation::kStackS1CageEntityId &&
                off_state.s1_valve_angle < 0.01 && !off_state.s1_valve_pawled &&
                off_state.s1_bucket_water_kg < 0.01 && std::abs(off_state.s1_cage_travel) <= 0.01,
            "stood in S1's cage off its plate, nothing may move");
    // Stepped on and straight off again, the pawl has dropped: the water runs
    // on, outweighs the empty cage and sends it up; drained at the foot, the
    // bucket brings it back down by itself, and the next rider finds S1 as it
    // was.
    Simulation brief(InitialSpawn::ExteriorGrade);
    require(brief.advance_frame(0.5).accepted, "S1 step-on settle interval must be accepted");
    require(board_s1(brief), "the player must walk from the yard into S1's cage and onto its plate");
    require(walk_to(brief, 10.0, -116.5, 6.0), "the player must step straight back off S1's cage");
    bool sent_up = false;
    bool came_back = false;
    double back_s = 0.0;
    const double brief_t0 = brief.snapshot().simulation_time_seconds;
    for (std::uint32_t tick = 0; tick < 90 * 120 && !came_back; ++tick) {
        (void)brief.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = brief.stack_state();
        sent_up = sent_up || state.s1_cage_travel >= kS1Travel - 0.01;
        came_back = sent_up && std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched &&
                    s1_plate_at_rest(brief);
        back_s = brief.snapshot().simulation_time_seconds - brief_t0;
    }
    require(sent_up, "stepped on and off S1's plate, the pawl must hold the valve and the water send the empty cage up");
    require(came_back, "S1's empty cage must come back down by itself and wait as found");
    std::cout << "PASS scraperx_sim S1 step on and off: empty_round_trip_s=" << back_s << "\n";

    // (b) The ride, from the game's spawn on player inputs: into the cage and
    // onto its plate. Water runs into the bucket until it outweighs the cage
    // and the rider (235 kg); the bucket falls 21.8 m, knocking the pawl out as
    // it leaves the head, and the cage carries the rider 21.8 m under a
    // governor that can only brake. The valve shuts within the first metre. At
    // no tick has the payload gained more energy than the bucket and its water
    // released.
    Simulation ride(InitialSpawn::ExteriorGrade);
    require(ride.advance_frame(0.5).accepted, "S1 ride settle interval must be accepted");
    const double tank0 = ride.stack_state().s1_tank_water_kg;
    require(board_s1(ride), "the rider must walk from the yard into S1's cage and onto its plate");
    const auto rode = ride_s1(ride, 20.0);
    const auto top_state = ride.stack_state();
    const double floor_y = kit_y(ride, Simulation::kStackS1CageEntityId) + 0.10;
    require(rode.reached_top, "standing on S1's plate must carry the rider to the top of the cage's travel");
    require(rode.lift_water_kg >= kS1BalanceKg - 5.0 && rode.lift_water_kg <= kS1BalanceKg + 60.0,
            "S1's cage must leave the yard only once the bucket's water outweighs the cage and rider");
    require(rode.shut_travel > 0.0 && rode.shut_travel < 1.0,
            "rising, the bucket must knock S1's pawl out and the valve shut within the first metre");
    require(std::abs(tank0 - rode.shut_tank_kg - rode.shut_water_kg) < 2.0,
            "every kilogram in S1's bucket must have come out of its tank");
    require(rode.shut_tank_kg - rode.top_tank_kg < 1.0 && rode.top_water_kg - rode.shut_water_kg < 1.0,
            "with the valve shut, no more water may leave S1's tank on the way up");
    require(rode.rode_on_cage, "the rider must stand on S1's cage for the whole ride");
    require(std::abs(floor_y - kS1FloorTopUp) <= 0.05, "S1's floor must stop at 22.05 m");
    require(top_state.s1_cage_peak_speed <= 2.6, "S1's governor must hold the cage to 2.5 m/s");
    require(rode.worst_margin_j >= -kStanceJitterJ,
            "at no tick may S1's payload have gained more energy than the bucket released");

    // (d) The receiver: off the cage's open north side, along the gangway and
    // onto deck 2's south band, standing on the tower.
    require(walk_to(ride, 10.0, -123.2, 6.0) && walk_to(ride, 10.0, -126.5, 6.0),
            "the rider must walk off S1's cage over the gangway onto deck 2");
    (void)ride.advance_frame(0.5);
    const auto on_deck = ride.snapshot();
    require(on_deck.player_grounded && on_deck.player_position.y > kDeck2Top + 0.5 &&
                on_deck.player_position.z < -124.0 &&
                on_deck.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 2's south band");

    // Recovery: at the foot of its guide the bucket sits on the striker and
    // drains; lighter than the cage again, it rises, the cage comes back down
    // and the catch seats the bucket at the top. S1 is as it was found, its
    // plate proud of the floor for the next rider.
    bool returned = false;
    double descent_water = -1.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !returned; ++tick) {
        (void)ride.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = ride.stack_state();
        if (descent_water < 0.0 && state.s1_cage_travel < kS1Travel - 0.10) {
            descent_water = state.s1_bucket_water_kg;
        }
        returned = std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched &&
                   std::abs(state.s1_bucket_travel) <= 0.05;
    }
    (void)ride.advance_frame(2.0);
    const auto back = ride.stack_state();
    require(returned, "drained, S1's bucket must rise and bring the cage back down to the yard");
    require(descent_water > 0.0 && descent_water < kS1CageMassKg - kS1BucketMassKg,
            "S1's empty cage must start down only once the bucket is lighter than it");
    require(s1_plate_at_rest(ride), "S1's plate must hang proud of the cage's floor again, the pawl out");
    std::cout << "PASS scraperx_sim S1 ride: ride_s=" << rode.seconds << " lift_water_kg=" << rode.lift_water_kg
              << " valve_shut_at_m=" << rode.shut_travel << " top_water_kg=" << rode.top_water_kg
              << " floor_y=" << floor_y << " peak_speed=" << top_state.s1_cage_peak_speed
              << " energy_margin_J=" << rode.worst_margin_j << " deck2_y=" << on_deck.player_position.y
              << " descent_water_kg=" << descent_water << " return_water_kg=" << back.s1_bucket_water_kg << "\n";

    // (c) The rider who stays aboard: standing still on the plate at the top,
    // the bucket drains until it is lighter than the cage and rider, and the
    // cage brings them back down to the yard. There, still on the plate, their
    // weight turns the lever again and the machine carries them up a second
    // time: nobody is stranded, and the tank holds water for many rides.
    Simulation stay(InitialSpawn::ExteriorGrade);
    require(stay.advance_frame(0.5).accepted, "S1 stay-aboard settle interval must be accepted");
    require(board_s1(stay) && ride_s1(stay, 20.0).reached_top, "S1 must carry the rider up");
    bool brought_down = false;
    bool aboard = true;
    double stay_descent_water = -1.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !brought_down; ++tick) {
        (void)stay.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = stay.stack_state();
        if (stay_descent_water < 0.0 && state.s1_cage_travel < kS1Travel - 0.10) {
            stay_descent_water = state.s1_bucket_water_kg;
        }
        if (state.s1_cage_travel > 0.05 && state.s1_cage_travel < kS1Travel - 0.05) {
            const auto snap = stay.snapshot();
            aboard = aboard && snap.player_grounded &&
                     (snap.support_entity_id == Simulation::kStackS1CageEntityId ||
                      snap.support_entity_id == Simulation::kStackS1PlateEntityId);
        }
        brought_down = std::abs(state.s1_cage_travel) <= 0.02;
    }
    require(brought_down && aboard, "a rider who stays aboard must be brought back down to the yard on S1's cage");
    require(stay_descent_water > 0.0 && stay_descent_water < kS1BalanceKg,
            "S1's cage must start down with its rider only once the bucket is lighter than both");
    const double stay_tank = stay.stack_state().s1_tank_water_kg;
    require(advance_until(stay, [&](const scraperx::sim::Snapshot &) { return stay.stack_state().s1_valve_pawled; }, 2.0) &&
                ride_s1(stay, 20.0).reached_top,
            "still on S1's plate at the foot, the rider must be carried up a second time");
    const double per_ride = stay_tank - stay.stack_state().s1_tank_water_kg;
    require(per_ride > 0.0 && stay.stack_state().s1_tank_water_kg > 10.0 * per_ride,
            "S1's tank must hold water for many more rides");
    std::cout << "PASS scraperx_sim S1 stay aboard: descent_water_kg=" << stay_descent_water
              << " per_ride_kg=" << per_ride << " tank_kg=" << stay.stack_state().s1_tank_water_kg << "\n";

    // C1: from deck 2's south band, where S1 leaves its rider, up the facade
    // to deck 4 on player inputs.
    Simulation facade(InitialSpawn::Deck2South);
    require(facade.advance_frame(1.0).accepted, "C1 settle interval must be accepted");
    const double facade_start = facade.snapshot().simulation_time_seconds;
    C1Notes notes;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool climbed = climb_c1(facade, &notes);
    g_path_watch.armed = false;
    require(climbed, "C1 must carry a climber from deck 2 to deck 4");
    if (g_path_watch.worst > 0.15) {
        std::cout << "C1 jump: step=" << g_path_watch.worst << " at=" << g_path_watch.worst_at.x << ","
                  << g_path_watch.worst_at.y << "," << g_path_watch.worst_at.z
                  << " traversal=" << g_path_watch.worst_traversal << "\n";
    }
    require(g_path_watch.worst <= 0.15,
            "climbing C1, the body must never move more than 0.15 m sideways in one tick");
    require(!notes.ladder_in_reach_standing,
            "the davit's ladder must be out of reach from the monorail without the leap");
    const auto facade_top = facade.snapshot();
    std::cout << "PASS scraperx_sim C1 climb: seconds=" << facade_top.simulation_time_seconds - facade_start
              << " deck4_y=" << facade_top.player_position.y << " ladder_needs_leap=1"
              << " worst_tick_step_m=" << g_path_watch.worst << "\n";

    // The Stack so far in one run from the game's spawn, on player inputs:
    // hold S1's chain, ride it to deck 2, climb C1 to deck 4.
    Simulation band(InitialSpawn::ExteriorGrade);
    require(band.advance_frame(0.5).accepted, "the Stack's settle interval must be accepted");
    const double band_start = band.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(board_s1(band), "the Stack: into S1's cage and onto its plate");
    require(ride_s1(band, 20.0).reached_top, "the Stack: S1 carries the rider to deck 2");
    require(walk_to(band, 10.0, -123.2, 6.0) && walk_to(band, 10.0, -126.0, 6.0),
            "the Stack: off S1 onto deck 2");
    const double at_deck2 = band.snapshot().simulation_time_seconds - band_start;
    require(climb_c1(band), "the Stack: up C1 to deck 4");
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack: from the yard to deck 4 the body must never move more than 0.15 m sideways in one tick");
    const auto band_top = band.snapshot();
    require(band_top.death_count == 0, "the Stack: from the yard to deck 4 without dying");
    std::cout << "PASS scraperx_sim Stack to deck 4: seconds=" << band_top.simulation_time_seconds - band_start
              << " at_deck2=" << at_deck2 << " deck4_y=" << band_top.player_position.y
              << " worst_tick_step_m=" << g_path_watch.worst << "\n";

    // -------------------------------------------------------------------------
    // Industrial Athletic Traversal Layer: Proofs
    // -------------------------------------------------------------------------
    // 1. Yard Athletic Approach & Curb Step-Up:
    Simulation yard(InitialSpawn::ExteriorGrade);
    require(yard.advance_frame(0.5).accepted, "yard settle interval must be accepted");
    require(walk_to(yard, 6.0, -105.0, 30.0) && walk_to(yard, -5.0, -110.0, 15.0) && walk_to(yard, -5.0, -111.8, 6.0),
            "the player must walk into west yard and step onto the containment curb");
    (void)yard.advance_frame(0.3);
    const auto on_curb = yard.snapshot();
    require(on_curb.player_grounded && on_curb.player_position.y > 1.15,
            "standing on the transformer containment curb must elevate above yard grade");
    std::cout << "PASS scraperx_sim yard athletic approach: on_curb_y=" << on_curb.player_position.y << "\n";

    // 2. Deck 2 Overhead Crane Runway Balance Beam:
    Simulation d2_sim(InitialSpawn::Deck2South);
    require(d2_sim.advance_frame(0.5).accepted, "Deck 2 settle interval must be accepted");
    // Up the stair, then along its top landing to the beam's line and onto
    // its end, as a walker steps onto a 0.3 m beam: square to it, from a
    // stand. (Cut across onto it at a run, 1.7 m/s sideways, its balance
    // cannot hold the body; this proof once passed that way only because
    // the body hopped off the top step and landed braked on the beam, a
    // hop try_step_down now keeps the feet from.)
    const bool s1 = walk_to(d2_sim, 7.5, -125.8, 4.0);
    const bool s2 = walk_to(d2_sim, 7.5, -128.8, 6.0) && d2_sim.advance_frame(0.3).accepted &&
                    walk_to(d2_sim, 7.5, -129.5, 4.0) && d2_sim.advance_frame(0.3).accepted;
    const bool s3 = walk_to(d2_sim, 8.5, -129.5, 4.0);
    std::cout << "DEBUG d2 steps: s1=" << s1 << " s2=" << s2 << " s3=" << s3
              << " pos=(" << d2_sim.snapshot().player_position.x << ","
              << d2_sim.snapshot().player_position.y << "," << d2_sim.snapshot().player_position.z << ")\n";
    require(s1 && s2 && s3, "must walk west and ascend access stair onto crane runway I-beam");
    (void)d2_sim.advance_frame(0.2);
    const auto on_d2_runway = d2_sim.snapshot();
    require(on_d2_runway.player_grounded && on_d2_runway.player_position.y > 24.0 && on_d2_runway.player_balancing,
            "on the crane runway I-beam the player must balance at elevation 23.35m");
    require(walk_to(d2_sim, 12.5, -129.5, 6.0, 0.15), "must balance east along runway girder");
    const auto along_d2_runway = d2_sim.snapshot();
    require(along_d2_runway.player_position.x > 12.0 && along_d2_runway.player_position.y > 24.0,
            "must traverse across the open machinery bay on the runway girder");
    std::cout << "PASS scraperx_sim deck 2 crane runway: y=" << on_d2_runway.player_position.y
              << " balancing=" << int(on_d2_runway.player_balancing) << " crossed_x=" << along_d2_runway.player_position.x << "\n";

    // 3. C1 Suspended Maintenance Recovery Cradle:
    Simulation c1_rec(InitialSpawn::Deck2South);
    require(c1_rec.advance_frame(0.5).accepted, "Deck 2 settle for C1 recovery");
    require(climb_c1_to_monorail(c1_rec), "climb C1 up to monorail end");
    (void)hold_stick(c1_rec, 0.8, 0.0, 1.0, 0.0, 0.5,
                     [](const Snapshot &s) { return !s.player_grounded; });
    require(advance_until(c1_rec, [](const Snapshot &s) { return s.player_grounded; }, 4.0),
            "player falling from monorail leap must land on recovery cradle platform");
    const auto on_cradle = c1_rec.snapshot();
    std::cout << "DEBUG on_cradle: pos=(" << on_cradle.player_position.x << ","
              << on_cradle.player_position.y << "," << on_cradle.player_position.z
              << ") grounded=" << on_cradle.player_grounded
              << " deaths=" << on_cradle.death_count
              << " sup=" << on_cradle.support_entity_id << "\n";
    require(on_cradle.death_count == 0, "drop into recovery cradle must be survivable / non-lethal");
    require(on_cradle.player_position.y > 31.0 && on_cradle.player_position.y < 32.5,
            "must stand on recovery cradle floor at elevation 30.50m");
    require(walk_to(c1_rec, 11.20, -123.05, 6.0, 0.15), "walk to cradle recovery ladder");
    (void)c1_rec.set_facing(0.0, -1.0);
    (void)c1_rec.advance_frame(0.4);
    require(c1_rec.snapshot().grip_available, "recovery ladder grip must be offered");
    (void)c1_rec.request_traversal();
    (void)c1_rec.advance_frame(0.2);
    require(is_climbing(c1_rec.snapshot()), "must climb recovery ladder");
    const bool climbed_deck3 = hold_stick(c1_rec, 0.0, -1.0, 0.0, -1.0, 10.0,
                                          [](const Snapshot &s) { return standing_above(s, 33.5); });
    std::cout << "DEBUG c1_rec after climb: pos=(" << c1_rec.snapshot().player_position.x << ","
              << c1_rec.snapshot().player_position.y << "," << c1_rec.snapshot().player_position.z
              << ") traversal=" << int(c1_rec.snapshot().traversal_state)
              << " grounded=" << c1_rec.snapshot().player_grounded << "\n";
    require(climbed_deck3, "must climb recovery ladder back onto Deck 3 floor");
    std::cout << "PASS scraperx_sim C1 recovery cradle: fell 2.8m safe landed_y=" << on_cradle.player_position.y
              << " recovered_deck3_y=" << c1_rec.snapshot().player_position.y << "\n";

    // 4. Deck 4 Steam Receiver Skid & Runway Girder:
    Simulation d4_sim(InitialSpawn::Deck4South);
    require(d4_sim.advance_frame(0.5).accepted, "Deck 4 settle interval must be accepted");
    require(walk_to(d4_sim, 12.5, -127.0, 6.0), "walk east toward steam machinery bay");
    require(walk_to(d4_sim, 12.5, -129.5, 6.0), "ascend access stair");
    require(walk_to(d4_sim, 12.5, -132.0, 6.0), "step onto balance runway girder");
    const auto on_d4_runway = d4_sim.snapshot();
    std::cout << "DEBUG on_d4_runway: pos=(" << on_d4_runway.player_position.x << ","
              << on_d4_runway.player_position.y << "," << on_d4_runway.player_position.z
              << ") grounded=" << on_d4_runway.player_grounded
              << " balancing=" << int(on_d4_runway.player_balancing) << "\n";
    require(on_d4_runway.player_grounded && on_d4_runway.player_position.y > 46.0 && on_d4_runway.player_balancing,
            "must balance on Deck 4 runway girder at elevation 45.35m");
    std::cout << "PASS scraperx_sim deck 4 runway girder: y=" << on_d4_runway.player_position.y
              << " balancing=" << int(on_d4_runway.player_balancing) << "\n";

    // S2 at rest: left alone for 10 s, the walking beam and cage wait as found.
    Simulation s2_idle(InitialSpawn::Deck4South);
    require(s2_idle.advance_frame(10.0).accepted, "S2 idle interval must be accepted");
    const auto s2_idle_state = s2_idle.stack_state();
    require(std::abs(s2_idle_state.s2_cage_travel) <= 0.03 && s2_bolt_at_rest(s2_idle),
            "left alone, S2 must wait as found, its shoot bolt home");
    std::cout << "PASS scraperx_sim S2 at rest: cage_travel=" << s2_idle_state.s2_cage_travel
              << " bolt_shot=" << s2_idle_state.s2_bolt_shot << "\n";

    // S2 half shoved: the bolt pushed part way and let be still holds the cage.
    {
        Simulation half(InitialSpawn::Deck4South);
        require(half.advance_frame(0.5).accepted, "S2 settle interval must be accepted");
        require(walk_to(half, 10.0, -130.5, 30.0) && walk_to(half, 10.0, -137.6, 20.0, 0.10) &&
                    walk_to(half, 9.1, -138.3, 6.0, 0.08),
                "into S2's cage, at the bolt's push plate");
        for (int tick = 0; tick < 90 && half.stack_state().s2_bolt_travel < 0.35; ++tick) {
            (void)half.set_move_input(0.0, 0.6);
            (void)half.set_facing(0.0, 1.0);
            (void)half.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)half.set_move_input(0.0, 0.0);
        (void)half.advance_frame(5.0);
        const auto held = half.stack_state();
        std::cout << "  S2 half shoved: bolt_travel=" << held.s2_bolt_travel << " shot=" << held.s2_bolt_shot
                  << " cage_travel=" << held.s2_cage_travel << "\n";
        require(held.s2_bolt_travel > 0.2 && held.s2_bolt_shot && std::abs(held.s2_cage_travel) <= 0.03,
                "a bolt shoved part way stays put and still holds S2's cage");
    }

    // S2 ride: board the cage at deck 4, shove the shoot bolt out with the body, ride the walking beam to deck 6.
    Simulation s2_sim(InitialSpawn::Deck4South);
    require(s2_sim.advance_frame(0.5).accepted, "S2 settle interval must be accepted");
    require(shove_s2_bolt(s2_sim), "the player must walk from deck 4 into S2's cage and shove its bolt out");
    std::cout << "  S2: the bolt out " << s2_sim.stack_state().s2_bolt_travel << " m\n";
    const auto s2_rode = ride_s2(s2_sim, 20.0);
    require(s2_rode.reached_top, "shoving S2's bolt out must carry the rider to deck 6");
    require(s2_rode.rode_on_cage, "the rider must stand on S2's cage for the whole ride");
    const auto s2_top_state = s2_sim.stack_state();
    require(s2_top_state.s2_cage_peak_speed <= 2.6, "S2's governor must hold the cage to 2.5 m/s");
    require(s2_rode.worst_margin_j >= 0.0, "S2 payload energy gain must not exceed source energy released");

    // Walk off onto deck 6's south band
    require(walk_to(s2_sim, 10.0, -133.0, 6.0) && walk_to(s2_sim, 10.0, -126.0, 6.0),
            "the rider must walk off S2's cage over the upper gangway onto deck 6");
    (void)s2_sim.advance_frame(0.5);
    const auto on_deck6 = s2_sim.snapshot();
    require(on_deck6.player_grounded && on_deck6.player_position.y > kDeck6Top + 0.5 &&
                on_deck6.player_position.z < -124.0 &&
                on_deck6.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 6's south band");
    const double s2_floor_y = kit_y(s2_sim, Simulation::kStackS2CageEntityId) + 0.10;
    require(std::abs(s2_floor_y - kS2FloorTopUp) <= 0.05, "S2 floor must stop at 66.05 m");
    // Phase 2: a jump pushes off what it stands on. Two identical rides; at
    // 3 s one rider jumps in the cage. On that tick the cage, carried up by
    // its cart, takes the legs' equal and opposite impulse and dips relative
    // to the ride without the jump.
    {
        const auto cage_vy_after = [&](const bool jump) {
            Simulation sim(InitialSpawn::Deck4South);
            require(sim.advance_frame(0.5).accepted, "S2 settle interval must be accepted");
            require(shove_s2_bolt(sim), "the rider must shove S2's bolt out");
            for (int i = 0; i < 270; ++i) {
                (void)sim.set_move_input(0.0, 0.0);
                require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
            }
            require(sim.snapshot().support_entity_id == Simulation::kStackS2CageEntityId,
                    "the rider must be standing in the cage mid-ride");
            const auto reactions = sim.snapshot().jump_reaction_count;
            if (jump) {
                require(sim.request_jump(), "jump must be accepted");
            }
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
            const auto index = sim.kit_body_index(Simulation::kStackS2CageEntityId);
            return std::pair<double, std::uint64_t>(sim.kit_body_velocity(index).y,
                                                    sim.snapshot().jump_reaction_count - reactions);
        };
        const auto still = cage_vy_after(false);
        const auto jumped = cage_vy_after(true);
        std::cout << "  push-off: cage_vy still=" << still.first << " jumped=" << jumped.first << '\n';
        require(jumped.second == 1 && still.second == 0 && jumped.first < still.first - 0.05,
                "a jump in a moving cage must push the cage down by the legs' reaction");
    }

    std::cout << "PASS scraperx_sim S2 ride: ride_s=" << s2_rode.seconds
              << " floor_y=" << s2_floor_y
              << " peak_speed=" << s2_top_state.s2_cage_peak_speed
              << " deck6_y=" << on_deck6.player_position.y << "\n";

    // S2 checkpoint continuation: commit checkpoint on deck 4, fatal fall, restore, then shove S2's bolt and ride.
    Simulation s2_cp(InitialSpawn::Deck4South);
    require(s2_cp.advance_frame(0.5).accepted, "S2 CP settle interval must be accepted");
    require(walk_to(s2_cp, 10.0, -125.5, 4.0, 0.10), "walk to checkpoint location on deck 4");
    (void)s2_cp.advance_frame(0.5);
    const auto s2_cp_pos = s2_cp.snapshot().checkpoint_position;
    require(s2_cp_pos.y > 44.0, "Deck 4 checkpoint committed above 44m");
    (void)s2_cp.set_facing(0.0, 1.0);
    (void)s2_cp.set_move_input(0.0, 1.0);
    const auto initial_deaths = s2_cp.snapshot().death_count;
    for (int t = 0; t < 400 && s2_cp.snapshot().death_count == initial_deaths; ++t) {
        (void)s2_cp.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s2_cp.set_move_input(0.0, 0.0);
    require(s2_cp.snapshot().death_count > initial_deaths, "fall off deck 4 must be fatal");
    (void)s2_cp.advance_frame(0.5);
    require(s2_cp.snapshot().player_grounded, "player must be grounded after checkpoint restore");
    require(std::abs(s2_cp.snapshot().player_position.y - s2_cp_pos.y) <= 0.5, "player must restore to deck 4 altitude");
    require(shove_s2_bolt(s2_cp), "player must be able to shove S2's bolt after checkpoint restore");
    const auto s2_cp_rode = ride_s2(s2_cp, 20.0);
    require(s2_cp_rode.reached_top, "S2 must carry rider to deck 6 after checkpoint restore");
    std::cout << "PASS scraperx_sim S2 checkpoint continuation: deaths=" << s2_cp.snapshot().death_count << "\n";

    // Full Stack ascent in one run from grade: S1 (0->22m) -> C1 (22->44m) -> S2 (44->66m)
    require(shove_s2_bolt(band), "the Stack: from deck 4 into S2's cage and shove its bolt out");
    const auto band_s2_rode = ride_s2(band, 20.0);
    require(band_s2_rode.reached_top, "the Stack: S2 carries the rider to deck 6");
    require(walk_to(band, 10.0, -133.0, 6.0) && walk_to(band, 10.0, -126.0, 6.0),
            "the Stack: off S2 onto deck 6");
    (void)band.advance_frame(0.5);
    const auto band_deck6_top = band.snapshot();
    require(band_deck6_top.death_count == 0, "the Stack: from the yard to deck 6 without dying");
    require(band_deck6_top.player_grounded && band_deck6_top.player_position.y > kDeck6Top + 0.5,
            "the Stack: standing on deck 6");
    std::cout << "PASS scraperx_sim Stack to deck 6: seconds="
              << band_deck6_top.simulation_time_seconds - band_start
              << " deck6_y=" << band_deck6_top.player_position.y << "\n";

    // C2 climb: from Deck 6 South band up to Deck 8 North band
    Simulation c2_sim(InitialSpawn::Deck6South);
    require(c2_sim.advance_frame(1.0).accepted, "C2 settle interval must be accepted");
    const double c2_start = c2_sim.snapshot().simulation_time_seconds;
    C2Notes c2_notes;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool c2_climbed = climb_c2(c2_sim, &c2_notes);
    g_path_watch.armed = false;
    require(c2_climbed, "C2 must carry a climber from deck 6 to deck 8");
    require(g_path_watch.worst <= 0.15,
            "climbing C2, the body must never move more than 0.15 m sideways in one tick");
    require(!c2_notes.ladder_in_reach_standing,
            "the davit ladder on C2 must be out of reach standing on the monorail");
    const auto c2_top = c2_sim.snapshot();
    std::cout << "PASS scraperx_sim C2 climb: seconds=" << c2_top.simulation_time_seconds - c2_start
              << " deck8_y=" << c2_top.player_position.y << " ladder_needs_leap=1\n";

    // S3 at rest: left alone for 10 s, S3 waits as found
    Simulation s3_idle(InitialSpawn::Deck8North);
    require(s3_idle.advance_frame(10.0).accepted, "S3 idle interval must be accepted");
    const auto s3_idle_state = s3_idle.stack_state();
    std::cout << "S3 idle: travel=" << s3_idle_state.s3_cage_travel
              << " latched=" << s3_idle_state.s3_brake_latched
              << " pan=" << s3_pan_at_rest(s3_idle) << "\n";
    require(std::abs(s3_idle_state.s3_cage_travel) <= 0.08 && s3_idle_state.s3_brake_latched &&
                s3_pan_at_rest(s3_idle),
            "left alone, S3 must wait as found with brake latched and its pan hanging empty");
    std::cout << "PASS scraperx_sim S3 at rest: cage_travel=" << s3_idle_state.s3_cage_travel
              << " brake_latched=" << s3_idle_state.s3_brake_latched << "\n";

    // S3 ride: the pig of iron from the gangway set in the pan in the cage at
    // deck 8, the brake trips, ride 44 m to deck 12
    Simulation s3_sim(InitialSpawn::Deck8North);
    require(s3_sim.advance_frame(0.5).accepted, "S3 settle interval must be accepted");
    require(load_s3_pan(s3_sim), "the pig of iron from deck 8's gangway, set in S3's pan, must trip its brake");
    const auto s3_rode = ride_s3(s3_sim, 25.0);
    require(s3_rode.reached_top, "the pig in S3's pan must carry the rider to deck 12");
    require(s3_rode.rode_on_cage, "the rider must stand on S3's cage for the whole ride");
    const auto s3_top_state = s3_sim.stack_state();
    require(s3_top_state.s3_cage_peak_speed <= 2.6, "S3's governor must hold the cage to 2.5 m/s");
    require(s3_rode.worst_margin_j >= 0.0, "S3 payload energy gain must not exceed source energy released");

    // Walk off onto deck 12 North band
    require(walk_to(s3_sim, -8.0, -164.0, 6.0) && walk_to(s3_sim, -8.0, -171.2, 6.0),
            "the rider must walk off S3's cage over the upper gangway onto deck 12");
    (void)s3_sim.advance_frame(0.5);
    const auto on_deck12 = s3_sim.snapshot();
    require(on_deck12.player_grounded && on_deck12.player_position.y > kDeck12Top + 0.5 &&
                on_deck12.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 12's north band");
    const double s3_floor_y = kit_y(s3_sim, Simulation::kStackS3CageEntityId) + 0.10;
    require(std::abs(s3_floor_y - kS3FloorTopUp) <= 0.05, "S3 floor must stop at 132.05 m");
    std::cout << "PASS scraperx_sim S3 ride: ride_s=" << s3_rode.seconds
              << " floor_y=" << s3_floor_y
              << " peak_speed=" << s3_top_state.s3_cage_peak_speed
              << " deck12_y=" << on_deck12.player_position.y << "\n";

    // C3 climb: from Deck 12 to Deck 14 (154 m)
    Simulation c3_sim(InitialSpawn::Deck12North);
    require(c3_sim.advance_frame(1.0).accepted, "C3 settle interval must be accepted");
    const double c3_start = c3_sim.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool c3_climbed = climb_c3(c3_sim);
    g_path_watch.armed = false;
    require(c3_climbed, "C3 must carry a climber from deck 12 to deck 14");
    require(g_path_watch.worst <= 0.15,
            "climbing C3, the body must never move more than 0.15 m sideways in one tick");
    const auto c3_top = c3_sim.snapshot();
    std::cout << "PASS scraperx_sim C3 climb: seconds=" << c3_top.simulation_time_seconds - c3_start
              << " deck14_y=" << c3_top.player_position.y << "\n";

    // Complete continuous Stack ascent in one unbroken run from Grade (0.0 m) to Deck 14 (+154.0 m)
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(climb_c2(band), "the Stack: up C2 to deck 8");
    require(load_s3_pan(band), "the Stack: from deck 8 into S3's cage with the pig, set in its pan");
    const auto band_s3_rode = ride_s3(band, 25.0);
    require(band_s3_rode.reached_top, "the Stack: S3 carries the rider to deck 12");
    require(walk_to(band, -8.0, -164.0, 6.0) && walk_to(band, -8.0, -171.2, 6.0),
            "the Stack: off S3 onto deck 12 landing");
    require(climb_c3(band), "the Stack: up C3 to deck 14 (Deck154)");
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack: from yard to deck 14 the body must never move more than 0.15 m sideways in one tick");
    const auto band_deck14_top = band.snapshot();
    require(band_deck14_top.death_count == 0, "the Stack: from yard to deck 14 without dying");
    require(band_deck14_top.player_grounded && band_deck14_top.player_position.y > kDeck14Top + 0.5,
            "the Stack: standing on deck 14");
    std::cout << "PASS scraperx_sim Stack to deck 14: seconds="
              << band_deck14_top.simulation_time_seconds - band_start
              << " deck14_y=" << band_deck14_top.player_position.y
              << " pos=(" << band_deck14_top.player_position.x << ", "
              << band_deck14_top.player_position.y << ", "
              << band_deck14_top.player_position.z << ")\n";

    // ---- Mega-Ascent Continuation: Stack -> Counterweight Well (0 to 220 m) ----
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(board_well_a(band), "the Stack to A: board Stage A cage from Deck 14");
    require(rig_well_a(band), "the Stack to A: rig Stage A shackle to cage eye");
    require(pull_well_a(band, 0.5, 3.0), "the Stack to A: pull Stage A trip handle");
    require(wait_for(band, 16.0, [](const Snapshot &state) {
        return state.well_a_cage_travel >= kWellATravel - 0.01;
    }), "the Stack to A: Stage A carries rider to Ring 176 (176.25 m)");
    (void)band.advance_frame(1.0);
    const auto band_ring176 = band.snapshot();
    require(band_ring176.death_count == 0, "the Stack to Ring 176 without dying");
    std::cout << "PASS scraperx_sim Stack to Ring 176: seconds="
              << band_ring176.simulation_time_seconds - band_start
              << " ring176_y=" << band_ring176.player_position.y << "\n";

    // Step across into B's cage
    require(walk_to(band, -9.4, -131.2, 4.0) && walk_to(band, -7.6, -131.2, 4.0),
            "from A's parked cage the rider must step across into B's");
    require(rig_well_b(band) && pull_well_b(band), "the rider must rig and trip Stage B");
    require(wait_for(band, 16.0, [](const Snapshot &state) {
        return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
    }), "B must carry the rider to the 198 ring");
    (void)band.advance_frame(3.0);
    require(band.snapshot().player_grounded &&
            band.snapshot().support_entity_id == Simulation::kWellBCageEntityId,
            "the rider must stand in B's parked cage");

    // Step across onto C's platform
    require(walk_to(band, -6.2, -131.9, 4.0) && walk_to(band, -4.4, -131.9, 4.0),
            "from B's parked cage the rider must step across onto C's platform");
    require(clear_well_c_chute(band) && fill_well_c(band, 10.0) && pull_well_c_latch(band, 3.0),
            "the rider must clear C's chute, let the dumpster fill and pull the latch");
    require(wait_for(band, 16.0, [](const Snapshot &state) {
        return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
    }), "C must carry the rider to the 220 ring");
    (void)band.advance_frame(2.0);

    // Step off onto Ring 220
    require(walk_to(band, -3.0, -129.0, 4.0), "from C's platform the rider must step onto the 220 ring");
    (void)band.advance_frame(1.0);
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack to 220m: body must never move more than 0.15 m sideways in one tick");
    const auto band_ring220 = band.snapshot();
    require(band_ring220.death_count == 0, "Grade to Ring 220 without dying");
    require(band_ring220.player_grounded && band_ring220.player_position.y > 221.0 &&
            band_ring220.player_position.y < 221.3 && band_ring220.player_position.z > -130.73 &&
            band_ring220.support_entity_id != Simulation::kWellCPlatformEntityId,
            "continuous ascent from Grade must end standing on Ring 220");
    std::cout << "PASS scraperx_sim Stack to Ring 220: seconds="
              << band_ring220.simulation_time_seconds - band_start
              << " ring220_y=" << band_ring220.player_position.y << "\n";

    // Verify cascade re-arms Stage A:
    require(wait_for(band, 25.0, [](const Snapshot &state) {
        return state.well_a_catch_latched && state.well_a_cage_travel <= 0.01;
    }), "C's spent dumpster empties into A's cage, re-arming A");
    const auto rearmed = band.snapshot();
    require(rearmed.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0 &&
            rearmed.well_c_dumpster_kg <= 1.0 && rearmed.well_a_skip_travel >= -0.01,
            "re-armed, A's skip must be up in its catch, rubble in its cage");
    std::cout << "PASS scraperx_sim Mega-Ascent Grade to 220m: seconds="
              << band_ring220.simulation_time_seconds - band_start
              << " deck14_y=" << band_deck14_top.player_position.y
              << " ring176_y=" << band_ring176.player_position.y
              << " ring220_y=" << band_ring220.player_position.y
              << " rearmed=1\n";

    // On in the same run, cycle 10: along the 220 ring to AS-007 and up its
    // three machines -- D's spool, E's door and chiller, F's hose and stop
    // valve -- onto TP-340.
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(walk_to(band, 2.5, -129.6, 8.0) && walk_to(band, 4.0, -129.2, 6.0) && seat_wet_spool(band),
            "along the 220 ring, D's spool carried aboard and seated");
    require(wait_for(band, 120.0, [&](const Snapshot &) { return band.wet_state().d_platform_travel >= 35.95; }),
            "D must carry the rider to 256.25");
    require(drop_into_wet_e(band) && wait_wet_e_top(band, 60.0),
            "through E's doors onto its deck, and E must blow the rider up to 298.25");
    require(walk_to(band, 13.4, -137.2, 4.0) && walk_to(band, 13.4, -139.9, 6.0) && couple_wet_hose(band) &&
                pull_wet_stop_valve(band),
            "onto F, its hose coupled and its stop valve thrown");
    require(wait_for(band, 90.0, [&](const Snapshot &) { return band.wet_state().f_platform_travel >= 41.95; }),
            "F must carry the rider to TP-340");
    require(walk_to(band, 13.0, -137.2, 6.0), "off F onto TP-340");
    (void)band.advance_frame(0.5);
    g_path_watch.armed = false;
    const auto band_tp340 = band.snapshot();
    require(g_path_watch.worst <= 0.15,
            "Ring 220 to TP-340: body must never move more than 0.15 m sideways in one tick");
    require(band_tp340.death_count == 0 && band_tp340.player_grounded && band_tp340.player_position.y > 340.25 &&
                band_tp340.support_entity_id == Simulation::kWetFrameEntityId,
            "continuous ascent from Grade must end standing on TP-340");
    std::cout << "PASS scraperx_sim Mega-Ascent Grade to TP-340: seconds="
              << band_tp340.simulation_time_seconds - band_start
              << " ring220_y=" << band_ring220.player_position.y << " tp340_y=" << band_tp340.player_position.y
              << "\n";
}
