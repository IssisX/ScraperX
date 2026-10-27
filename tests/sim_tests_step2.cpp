// Step 2 movement: sprint, climb, shimmy, balance and the controlled drop.

#include "sim_tests.hpp"

void run_step2_movement() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    // ---- Step 2 movement (03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md §8) --
    //
    // Sprint: held with the stick forward the body runs at 8 m/s at once,
    // gathering on toward flow's 9 (run_flow), and a jump keeps that speed in
    // the air and carries past where a walking jump lands. Crouched, or with
    // the stick across the facing, it walks.
    Simulation sprint(InitialSpawn::Deck154);
    sprint.place_matron_home(154.0F);
    require(sprint.advance_frame(1.0).accepted, "the sprint settle interval must be accepted");
    // Runs along the deck, east (+1) or west (-1).
    const auto run_east = [&](const double seconds, double &peak, bool &flag,
                              const double way = 1.0) {
        const auto ticks = static_cast<std::uint32_t>(seconds * Simulation::kTickRateHz);
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            peak = std::max(peak, horizontal_speed(sprint.snapshot()));
            flag = flag || sprint.snapshot().player_sprinting;
        }
    };
    // A jump east at whatever speed the body has, stick held: its distance
    // and its slowest speed in the air.
    const auto jump_east = [&](double &distance, double &air_slowest, const double way = 1.0) {
        const double x0 = sprint.snapshot().player_position.x;
        (void)sprint.request_jump();
        air_slowest = std::numeric_limits<double>::infinity();
        for (std::uint32_t tick = 0; tick < 2U * Simulation::kTickRateHz; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            const auto state = sprint.snapshot();
            if (!state.player_grounded) {
                air_slowest = std::min(air_slowest, horizontal_speed(state));
            } else if (tick > 10U) {
                break;
            }
        }
        distance = (sprint.snapshot().player_position.x - x0) * way;
    };
    (void)sprint.set_sprint_input(true);
    double sprint_peak = 0.0;
    bool sprint_flag = false;
    run_east(2.0, sprint_peak, sprint_flag);
    double sprint_jump = 0.0;
    double sprint_air_slowest = 0.0;
    jump_east(sprint_jump, sprint_air_slowest);
    (void)sprint.set_sprint_input(false);
    // Walking back west: settled to a walk, then measured.
    double settle_peak = 0.0;
    bool settle_flag = false;
    run_east(0.6, settle_peak, settle_flag, -1.0);
    double walk_peak = 0.0;
    bool walk_flag = false;
    run_east(1.0, walk_peak, walk_flag, -1.0);
    double walk_jump = 0.0;
    double walk_air_slowest = 0.0;
    jump_east(walk_jump, walk_air_slowest, -1.0);
    require(sprint_flag && sprint_peak >= 7.9 && sprint_peak <= 9.05,
            "sprinting, the body must run at 8 m/s, gathering on to at most 9");
    require(sprint_air_slowest >= 7.9, "a sprinting jump must keep its speed in the air");
    require(sprint_jump >= walk_jump + 2.0,
            "a sprinting jump must carry at least 2 m past a walking one");
    require(!walk_flag && walk_peak <= 5.55, "without sprint held the body walks at 5.5 m/s");
    // Stopped first: crouching at a run is a slide (run_slide), not this.
    (void)sprint.set_move_input(0.0, 0.0);
    (void)sprint.advance_frame(0.6);
    (void)sprint.set_sprint_input(true);
    (void)sprint.set_crouch_input(true);
    (void)sprint.advance_frame(0.3);
    double crouched_peak = 0.0;
    bool crouched_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(-1.0, 0.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 45U) {
            crouched_peak = std::max(crouched_peak, horizontal_speed(sprint.snapshot()));
        }
        crouched_flag = crouched_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_crouch_input(false);
    (void)sprint.set_move_input(0.0, 0.0);
    (void)sprint.advance_frame(0.5);
    double across_peak = 0.0;
    bool across_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(0.0, 1.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        across_peak = std::max(across_peak, horizontal_speed(sprint.snapshot()));
        across_flag = across_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_move_input(0.0, 0.0);
    require(!crouched_flag && crouched_peak <= 2.6, "crouched, sprint held must not sprint");
    require(!across_flag && across_peak <= 5.55,
            "with the stick across the facing, sprint held must not sprint");
    std::cout << "PASS scraperx_sim step2 sprint: peak=" << sprint_peak
              << " air_slowest=" << sprint_air_slowest << " sprint_jump_m=" << sprint_jump
              << " walk_jump_m=" << walk_jump << " crouched_peak=" << crouched_peak
              << " across_peak=" << across_peak << '\n';

    // Climb: facing a hold at hand height, Action takes it; the stick climbs
    // at 0.9 m/s hand over hand, the hands always within an arm of the
    // shoulders; the top-out mantles onto the ring. A column too thick to
    // close a hand round is no hold.
    Simulation climb(InitialSpawn::Deck154);
    climb.place_matron_home(154.0F);
    require(climb.advance_frame(1.0).accepted, "the climb settle interval must be accepted");
    require(walk_to(climb, -13.2, -128.2, 5.0) && walk_to(climb, -13.2, -131.6, 5.0, 0.08),
            "the player must reach Stage A's head post");
    (void)climb.set_facing(0.0, -1.0);
    (void)climb.advance_frame(0.4);
    const auto facing_post = climb.snapshot();
    (void)climb.request_traversal();
    (void)climb.advance_frame(0.3);
    require(!facing_post.grip_available && !is_climbing(climb.snapshot()),
            "a 0.4 m column must not be a hold");
    require(walk_to(climb, -13.2, -128.2, 5.0) && walk_to(climb, -10.0, -128.2, 4.0),
            "the player must step back onto the deck");
    bool climb_grip_offered = false;
    double climb_rate_peak = 0.0;
    double hand_reach_worst = 0.0;
    std::uint32_t regrips = 0;
    Vector3 last_left{};
    {
        // The ladder, instrumented: rate, hands, regrips.
        require(walk_to(climb, 3.5, -128.2, 16.0) && walk_to(climb, 3.5, -129.8, 4.0) &&
                    walk_to(climb, 5.0, -129.6, 4.0, 0.08),
                "the player must reach the ladder's foot");
        (void)climb.set_facing(0.0, 1.0);
        (void)climb.advance_frame(0.4);
        climb_grip_offered = climb.snapshot().grip_available;
        (void)climb.request_traversal();
        (void)climb.advance_frame(0.2);
        require(is_climbing(climb.snapshot()), "Action facing the ladder must take hold of it");
        double last_y = climb.snapshot().player_position.y;
        last_left = climb.snapshot().traversal_left_hand;
        for (std::uint32_t tick = 0; tick < 40U * Simulation::kTickRateHz; ++tick) {
            (void)climb.set_move_input(0.0, 1.0);
            (void)climb.set_facing(0.0, 1.0);
            (void)climb.advance_frame(Simulation::kFixedStepSeconds);
            const auto state = climb.snapshot();
            if (!is_climbing(state)) {
                break;
            }
            climb_rate_peak = std::max(climb_rate_peak,
                                       (state.player_position.y - last_y) * Simulation::kTickRateHz);
            last_y = state.player_position.y;
            const Vector3 shoulder{state.player_position.x, state.player_position.y + 0.45,
                                   state.player_position.z};
            for (const Vector3 &hand : {state.traversal_left_hand, state.traversal_right_hand}) {
                hand_reach_worst = std::max(
                    hand_reach_worst, std::hypot(hand.x - shoulder.x, hand.y - shoulder.y, hand.z - shoulder.z));
            }
            if (std::abs(state.traversal_left_hand.y - last_left.y) > 0.05) {
                ++regrips;
            }
            last_left = state.traversal_left_hand;
        }
        require(hold_stick(climb, 0.0, 1.0, 0.0, 1.0, 3.0,
                           [](const Snapshot &state) { return standing_above(state, 176.5); }),
                "at the ladder's head the climb must mantle onto the 176 ring");
    }
    const auto on_176 = climb.snapshot();
    require(climb_grip_offered, "the ladder's rungs must be offered as a hold");
    require(climb_rate_peak <= 0.91, "the climb must be no faster than 0.9 m/s");
    require(hand_reach_worst <= 1.0, "the hands must stay within an arm's reach of the shoulders");
    require(regrips >= 20, "the hands must go hand over hand up the ladder");
    require(on_176.player_position.z > -128.91 && on_176.player_position.y > 177.0 &&
                on_176.player_position.y < 177.3,
            "the climb must end standing on the 176 ring");
    std::cout << "PASS scraperx_sim step2 climb: rate_peak=" << climb_rate_peak
              << " hand_reach_worst=" << hand_reach_worst << " left_regrips=" << regrips
              << " top_y=" << on_176.player_position.y
              << " column_hold=" << int(facing_post.grip_available) << '\n';

    // Past the last hold: climbing down the scaffold panel stops at its
    // bottom horizontal, over the catwalk, and letting go drops onto it.
    Simulation panel(InitialSpawn::Ring198East);
    require(panel.advance_frame(1.0).accepted, "the panel settle interval must be accepted");
    require(walk_to(panel, 11.0, -129.3, 6.0, 0.1) && walk_to(panel, 11.0, -131.40, 6.0, 0.08),
            "the player must reach the catwalk under the scaffold panel");
    (void)panel.set_facing(0.0, 1.0);
    (void)panel.advance_frame(0.4);
    (void)panel.request_jump();
    require(hold_stick(panel, 0.0, 0.6, 0.0, 1.0, 3.0,
                       [](const Snapshot &state) { return is_climbing(state); }),
            "a jump at the scaffold panel must catch it");
    const double caught_y = panel.snapshot().player_position.y;
    (void)hold_stick(panel, 0.0, -1.0, 0.0, 1.0, 4.0, [](const Snapshot &) { return false; });
    const auto bottom = panel.snapshot();
    // The lowest hold is the bottom horizontal at 200.50: a body climbing
    // down stops with it at the reach of its lower hand, feet 0.8 m over the
    // catwalk -- not stepping off onto it (0.1 m) and not falling past it.
    require(is_climbing(bottom) && bottom.player_position.y > 199.6 &&
                bottom.player_position.y < caught_y,
            "climbing down must stop at the panel's last hold, not run on past it");
    (void)panel.request_release();
    require(wait_for(panel, 2.0,
                     [](const Snapshot &state) {
                         return state.player_grounded && state.traversal_state == TraversalState::None;
                     }),
            "let go, the body must drop onto the catwalk");
    const auto panel_dropped = panel.snapshot();
    require(panel_dropped.death_count == 0 && panel_dropped.player_position.y > 199.0 &&
                panel_dropped.player_position.y < 199.3,
            "the drop from the panel's foot must land on the catwalk alive");
    std::cout << "PASS scraperx_sim step2 last hold: caught_y=" << caught_y
              << " stopped_y=" << bottom.player_position.y << " dropped_y="
              << panel_dropped.player_position.y << '\n';

    // Controlled drop and shimmy: on a deck, Drop does nothing; with the
    // ring's edge behind, it lowers the body over it into a hang under the
    // lip. Hanging, the stick moves it along the lip at 0.6 m/s until the lip
    // stops (a scaffold board lies across it); Jump climbs back up.
    Simulation ledge(InitialSpawn::Ring176East);
    require(ledge.advance_frame(1.0).accepted, "the ledge settle interval must be accepted");
    (void)ledge.set_facing(0.0, 1.0);
    (void)ledge.advance_frame(0.3);
    const auto mid_deck = ledge.snapshot();
    (void)ledge.request_release();
    (void)ledge.advance_frame(0.3);
    require(!mid_deck.edge_drop_available &&
                ledge.snapshot().traversal_state == TraversalState::None &&
                ledge.snapshot().player_grounded,
            "on the middle of a deck, Drop must not lower the body anywhere");
    (void)hold_stick(ledge, 0.0, -0.4, 0.0, 1.0, 1.0,
                     [](const Snapshot &state) { return state.player_position.z <= -128.45; });
    (void)ledge.advance_frame(0.3);
    require(ledge.snapshot().edge_drop_available,
            "backed up to the ring's inner edge, a drop must be offered");
    (void)ledge.request_release();
    require(wait_for(ledge, 1.5,
                     [](const Snapshot &state) {
                         return state.traversal_state == TraversalState::Hanging;
                     }),
            "Drop with the edge behind must lower the body into a hang");
    (void)ledge.advance_frame(0.3);
    const auto hanging = ledge.snapshot();
    require(hanging.player_position.y > 175.1 && hanging.player_position.y < 175.3 &&
                hanging.player_position.z < -128.91 && hanging.traversal_left_hand.y > 176.2,
            "the hang must hold the body under the 176 ring's lip, hands on it");
    // The hands go along the lip at the shimmy's pace; the body, hanging
    // from them, follows a little behind.
    const double shimmy_x0 = hanging.traversal_left_hand.x;
    const double body_x0 = hanging.player_position.x;
    (void)hold_stick(ledge, 1.0, 0.0, 0.0, 1.0, 2.0, [](const Snapshot &) { return false; });
    const double shimmy_east = ledge.snapshot().traversal_left_hand.x - shimmy_x0;
    const double body_east = ledge.snapshot().player_position.x - body_x0;
    (void)hold_stick(ledge, -1.0, 0.0, 0.0, 1.0, 10.0, [](const Snapshot &) { return false; });
    const auto stopped = ledge.snapshot();
    (void)ledge.request_jump();
    require(wait_for(ledge, 2.0, [](const Snapshot &state) { return standing_above(state, 177.0); }),
            "Jump from the hang must climb back onto the ring");
    require(shimmy_east >= 1.15 && shimmy_east <= 1.25,
            "the shimmy must move along the lip at 0.6 m/s");
    require(body_east > shimmy_east - 0.25 && body_east <= shimmy_east + 0.05,
            "the body must follow its hands along the lip");
    require(stopped.traversal_state == TraversalState::Hanging && stopped.player_position.x > 7.2 &&
                stopped.player_position.x < 7.9,
            "the shimmy must stop where a board across the lip ends the hold");
    std::cout << "PASS scraperx_sim step2 ledge: mid_deck_edge=" << int(mid_deck.edge_drop_available)
              << " hang_y=" << hanging.player_position.y << " shimmy_east_m=" << shimmy_east
              << " stopped_x=" << stopped.player_position.x
              << " up_y=" << ledge.snapshot().player_position.y << '\n';

    // Balance: on the scaffold boards (0.3 m wide) the body walks at 2 m/s
    // along them and a sideways push under 0.8 keeps it on their line; a full
    // push steps it off. On the ring's deck it is not balancing, nor on the
    // boards' first metre, which lies over the deck: a step off them there
    // lands on it. Out over the well (z < -129.0) the boards are a beam, and
    // by -129.4 (the run walked off over the first 0.4 m of beam) the walk
    // has settled to the balance pace.
    Simulation beam(InitialSpawn::Ring176East);
    require(beam.advance_frame(1.0).accepted, "the balance settle interval must be accepted");
    const bool deck_balancing = beam.snapshot().player_balancing;
    require(walk_to(beam, 7.05, -128.4, 8.0, 0.1), "the player must reach the boards");
    (void)beam.advance_frame(0.5);
    require(walk_to(beam, 7.05, -128.4, 2.0, 0.06), "the player must stand at the boards' end");
    (void)beam.advance_frame(0.5);
    double beam_peak = 0.0;
    bool beam_flag = true;
    (void)hold_stick(beam, 0.0, -1.0, 0.0, -1.0, 0.6, [&](const Snapshot &state) {
        if (state.player_position.z < -129.4) {
            beam_peak = std::max(beam_peak, horizontal_speed(state));
            beam_flag = beam_flag && state.player_balancing;
        }
        return false;
    });
    (void)beam.advance_frame(0.3);
    // Out over the well on the first board, well short of the corner.
    double beam_drift = 0.0;
    (void)hold_stick(beam, 0.5, 0.0, 0.0, -1.0, 1.5, [&](const Snapshot &state) {
        beam_drift = std::max(beam_drift, std::abs(state.player_position.x - 7.05));
        return false;
    });
    const auto still_on = beam.snapshot();
    const auto beam_deaths = still_on.death_count;
    (void)hold_stick(beam, 1.0, 0.0, 0.0, -1.0, 6.0,
                     [&](const Snapshot &state) { return state.death_count > beam_deaths; });
    require(!deck_balancing, "on the ring's deck the body must not be balancing");
    require(beam_flag && beam_peak <= 2.05, "on the boards the body must balance at 2 m/s");
    require(still_on.player_grounded && still_on.player_position.y > 177.3 && beam_drift <= 0.15,
            "a half sideways push must keep the body on the boards' line");
    require(beam.snapshot().death_count == beam_deaths + 1,
            "a full sideways push must step the body off the boards");
    std::cout << "PASS scraperx_sim step2 balance: peak=" << beam_peak << " drift_m=" << beam_drift
              << " deck_balancing=" << int(deck_balancing) << " stepped_off=1\n";

    // Phase 4: across the boards only the pendulum acts (kBalanceHipMargin).
    // A sideways shove (the stick full across for a few ticks, then let go)
    // that leaves the XCoM inside the centre of pressure's reach (0.15 m
    // half-width + 0.12) is caught; one that carries it past cannot be, and
    // the body goes off. The old centring (22 m/s/s toward the line) caught
    // both: a 1.2 m/s shove stopped in 3 cm.
    const auto shove = [](const int ticks) {
        Simulation sim(InitialSpawn::Ring176East);
        require(sim.advance_frame(1.0).accepted, "settle must be accepted");
        require(walk_to(sim, 7.05, -128.4, 8.0, 0.1), "the player must reach the boards");
        (void)sim.advance_frame(0.5);
        require(walk_to(sim, 7.05, -128.4, 2.0, 0.06), "the player must stand at the boards' end");
        (void)sim.advance_frame(0.5);
        (void)hold_stick(sim, 0.0, -1.0, 0.0, -1.0, 0.6, [](const Snapshot &) { return false; });
        (void)sim.advance_frame(0.5);
        require(sim.snapshot().player_balancing, "the shove must start on the boards");
        const auto deaths = sim.snapshot().death_count;
        require(sim.set_move_input(1.0, 0.0), "move must be accepted");
        for (int i = 0; i < ticks; ++i) {
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
        }
        require(sim.set_move_input(0.0, 0.0), "release must be accepted");
        double peak_xcom = 0.0;
        for (int i = 0; i < 270; ++i) {
            require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick must advance");
            peak_xcom = std::max(peak_xcom, std::abs(sim.snapshot().beam_xcom));
            if (std::getenv("SCRAPERX_BALANCE_TRACE") != nullptr && i < 60) {
                const auto t = sim.snapshot();
                std::cout << "    t" << i << " x=" << t.player_position.x << " y=" << t.player_position.y
                          << " vx=" << t.player_linear_velocity.x << " xcom=" << t.beam_xcom
                          << " bal=" << t.player_balancing << " g=" << t.player_grounded << '\n';
            }
        }
        const auto end = sim.snapshot();
        struct { bool off; double peak_xcom; double x; } out{end.death_count > deaths || end.player_position.y < 177.0,
                                                              peak_xcom, end.player_position.x};
        if (std::getenv("SCRAPERX_BALANCE_DETAIL") != nullptr) {
            std::cout << "  shove ticks=" << ticks << " off=" << out.off << " peak_xcom=" << out.peak_xcom
                      << " x=" << out.x << '\n';
        }
        return out;
    };
    const auto caught = shove(3);
    const auto lost = shove(6);
    require(!caught.off && caught.peak_xcom < 1.0 && std::abs(caught.x - 7.05) < 0.15,
            "a shove that keeps the XCoM within reach must be caught back to the line");
    require(lost.off && lost.peak_xcom > 1.0,
            "a shove that carries the XCoM past the centre of pressure's reach must take the body off");
    std::cout << "PASS scraperx_sim step2 beam pendulum: caught_peak_xcom=" << caught.peak_xcom
              << " lost_peak_xcom=" << lost.peak_xcom << '\n';

    // AS-006's no-lift route: from the 154 m deck to standing on the 220 ring
    // by ladder, boards and standpipe, catwalk and scaffold, with every lift
    // in the band left where it was found.
    Simulation route(InitialSpawn::Deck154);
    route.place_matron_home(154.0F);
    require(route.advance_frame(1.0).accepted, "the route settle interval must be accepted");
    const double route_start = route.snapshot().simulation_time_seconds;
    require(climb_route_ladder(route), "the route's ladder must take the player to the 176 ring");
    require(climb_route_pipe(route), "the route's boards and standpipe must take the player to 198");
    require(climb_route_panel(route), "the route's catwalk and scaffold must take the player to 220");
    (void)route.advance_frame(0.5);
    const auto route_top = route.snapshot();
    require(route_top.player_grounded && route_top.player_position.y > 221.0 &&
                route_top.player_position.y < 221.3 && route_top.player_position.z > -130.73 &&
                route_top.death_count == 0,
            "the no-lift route must end standing on the 220 ring");
    require(std::abs(route_top.well_a_cage_travel) <= 0.01 &&
                std::abs(route_top.well_b_cage_travel) <= 0.01 &&
                std::abs(route_top.well_c_platform_travel) <= 0.01 &&
                route_top.well_a_catch_latched && route_top.well_b_catch_latched &&
                route_top.well_c_catch_latched,
            "the no-lift route must leave every lift in the band where it was found");
    std::cout << "PASS scraperx_sim AS-006 route: seconds="
              << route_top.simulation_time_seconds - route_start
              << " top_y=" << route_top.player_position.y << " climbs=" << route_top.climb_count
              << " lifts_untouched=1\n";
}
