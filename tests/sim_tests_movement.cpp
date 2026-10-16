// Parkour movement: brachiation, the pipe gallery, slide, speed vault, wall-run, hook, workers, C7, jump buffer, parkour seams.

#include "sim_tests.hpp"

// Parkour step 2: brachiation on the proving ground's monkey bars (six bars a
// metre apart, 3.1 m up, x = 0..5 at z = -20; a landing block past the last).
// A running jump catches the first; held forward, the body swings hand over
// hand to the last, one bar at a time and never moved faster than it swings;
// let go at the bottom of a swing, it flies on to the block. From a still
// hang with the stick let go nothing reaches; pumped, it reaches.
void run_brachiation() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    const double dt = Simulation::kFixedStepSeconds;

    Simulation s(InitialSpawn::SwingApproach);
    require(s.set_facing(1.0, 0.0), "swing facing must be accepted");
    require(s.advance_frame(0.4).accepted, "swing settle must advance");
    require(s.set_move_input(1.0, 0.0), "swing run-up must be accepted");
    require(s.request_jump(), "swing jump must be accepted");
    require(advance_until(s,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Swinging;
                          },
                          1.0),
            "a running jump under the monkey bars must catch the first bar");
    require(s.snapshot().swing_catch_count == 1, "one catch");
    require(std::abs(s.snapshot().traversal_left_hand.x) < 0.05, "the hands must be on the first bar");

    Snapshot last = s.snapshot();
    double worst_tick = 0.0;
    std::uint64_t reaches = 0;
    double crossed_s = -1.0;
    for (int tick = 0; tick < 90 * 5 && crossed_s < 0.0; ++tick) {
        require(s.advance_frame(dt).accepted, "swing tick must advance");
        const Snapshot now = s.snapshot();
        require(now.traversal_state == TraversalState::Swinging, "the swing must hold across the bars");
        const auto v = last.player_linear_velocity;
        const double speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        const double moved = std::hypot(horizontal_distance(now.player_position, last.player_position),
                                        now.player_position.y - last.player_position.y);
        worst_tick = std::max(worst_tick, moved / ((speed + 0.5) * dt));
        if (now.swing_reach_count != reaches) {
            reaches = now.swing_reach_count;
            require(std::abs(now.traversal_left_hand.x - static_cast<double>(reaches)) < 0.05,
                    "each reach must take the next bar, never skip one");
        }
        if (reaches == 5) {
            crossed_s = (tick + 1) * dt;
        }
        last = now;
    }
    require(crossed_s > 0.0, "held forward, the swing must carry the body hand over hand to the last bar");
    require(worst_tick <= 1.0, "no reach may move the body faster than it swings");
    require(s.snapshot().hang_slip_count == 0, "the bars must hold a swinging body");

    // Let go at the bottom of a forward swing: the body flies on.
    require(advance_until(s,
                          [](const Snapshot &state) {
                              return state.player_linear_velocity.x > 2.0 &&
                                     state.player_position.x > 4.95;
                          },
                          4.0),
            "the last bar's swing must come forward fast");
    const Snapshot before = s.snapshot();
    require(s.request_release(), "a swinging body must be allowed to let go");
    require(s.advance_frame(dt).accepted, "swing release tick");
    const Snapshot flying = s.snapshot();
    require(flying.traversal_state == TraversalState::None, "letting go must end the swing");
    require(std::abs(flying.player_linear_velocity.x - before.player_linear_velocity.x) < 0.3,
            "a swing let go must keep its velocity");
    require(advance_until(s, [](const Snapshot &state) { return state.player_grounded; }, 2.0),
            "the fling must come down");
    const Snapshot landed = s.snapshot();
    require(landed.support_entity_id == Simulation::kMonkeyBarsEntityId && landed.player_position.x > 5.9,
            "the fling from the last bar must land on the block past it");

    // A still hang: the stick let go stills the swing and reaches nothing;
    // pressed, the kip pumps a swing that carries the hand to the next bar.
    Simulation d(InitialSpawn::SwingApproach);
    require(d.set_facing(1.0, 0.0), "still-hang facing must be accepted");
    require(d.advance_frame(0.4).accepted, "still-hang settle must advance");
    require(d.set_move_input(1.0, 0.0), "still-hang run-up must be accepted");
    require(d.request_jump(), "still-hang jump must be accepted");
    require(advance_until(d,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Swinging;
                          },
                          1.0),
            "the still-hang run must catch a bar");
    require(d.set_move_input(0.0, 0.0), "letting the stick go must be accepted");
    const std::uint64_t held_reaches = d.snapshot().swing_reach_count;
    require(d.advance_frame(4.0).accepted, "still-hang interval must advance");
    const Snapshot still = d.snapshot();
    require(still.traversal_state == TraversalState::Swinging, "a still hang must hold");
    require(std::hypot(still.player_linear_velocity.x, still.player_linear_velocity.z) < 0.05,
            "with the stick let go the body must still its swing");
    require(still.swing_reach_count == held_reaches, "a still hang must reach nothing");
    require(d.set_move_input(1.0, 0.0), "pumping must be accepted");
    require(advance_until(d,
                          [&](const Snapshot &state) { return state.swing_reach_count > held_reaches; },
                          3.0),
            "pumped from a still hang, the swing must carry the hand to the next bar");

    std::cout << "PASS scraperx_sim brachiation: crossed_s=" << crossed_s << " worst_tick=" << worst_tick
              << " fling_vx=" << before.player_linear_velocity.x << " landed_x=" << landed.player_position.x
              << "\n";
}

// The pipe gallery in the yard (sim/pipe_gallery.hpp), played: a run along
// the start stand, a jump from its painted edge for the first pipe, the stick
// held forward hand over hand along all ten, the platform's lip taken from
// the front of a swing, and a climb up onto it.
void run_pipe_gallery() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::PipeGallery;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    Simulation s(InitialSpawn::PipeGallery);
    require(s.set_facing(1.0, 0.0), "gallery facing must be accepted");
    require(s.advance_frame(0.4).accepted, "gallery settle must advance");
    require(s.snapshot().player_grounded, "the gallery spawn must stand on the start stand");
    require(s.set_move_input(1.0, 0.0), "gallery run-up must be accepted");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.x > PipeGallery::kStandEastX - 0.5; },
                          2.0),
            "the run must reach the take-off");
    require(s.request_jump(), "gallery jump must be accepted");
    double caught_s = -1.0;
    double ledge_s = -1.0;
    double hand_x = 0.0;
    std::uint64_t reaches = 0;
    bool in_order = true;
    for (int tick = 0; tick < 90 * 12 && ledge_s < 0.0; ++tick) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "gallery tick");
        const Snapshot n = s.snapshot();
        if (std::getenv("SWING_TRACE") != nullptr && tick % 9 == 0) {
            std::cout << "G t=" << tick << " st=" << static_cast<int>(n.traversal_state) << " x=" << n.player_position.x
                      << " y=" << n.player_position.y << " vx=" << n.player_linear_velocity.x
                      << " r=" << n.swing_reach_count << " hand=" << n.traversal_left_hand.x << "\n";
        }
        if (caught_s < 0.0 && n.traversal_state == TraversalState::Swinging) {
            caught_s = (tick + 1) * Simulation::kFixedStepSeconds;
            hand_x = n.traversal_left_hand.x;
        }
        if (n.traversal_state == TraversalState::Swinging && n.swing_reach_count != reaches) {
            in_order = in_order && std::abs(n.traversal_left_hand.x - hand_x - PipeGallery::kPipeSpacing) < 0.05;
            hand_x = n.traversal_left_hand.x;
            reaches = n.swing_reach_count;
        }
        if (n.traversal_state == TraversalState::Hanging) {
            ledge_s = (tick + 1) * Simulation::kFixedStepSeconds;
        }
    }
    require(caught_s > 0.0 && caught_s < 1.0, "a jump from the take-off must catch the first pipe");
    require(ledge_s > 0.0, "held forward, the gallery must carry the hands along every pipe to the platform's lip");
    const double last_pipe_x =
        PipeGallery::kFirstPipeX + PipeGallery::kPipeSpacing * (PipeGallery::kPipeCount - 1);
    require(in_order && std::abs(hand_x - last_pipe_x) < 0.05,
            "each reach must take the next pipe, none skipped, the lip taken from the last");
    require(s.request_jump(), "the climb up must be accepted");
    require(advance_until(s,
                          [](const Snapshot &n) {
                              return n.player_grounded && n.traversal_state == TraversalState::None;
                          },
                          3.0),
            "from the lip the body must climb up onto the platform");
    const Snapshot top = s.snapshot();
    require(top.player_position.y > PipeGallery::kPlatformTop + 0.8 && top.player_position.x > PipeGallery::kPlatformWestX,
            "the climb must stand the body on the platform");
    require(top.death_count == 0, "the gallery is not lethal");
    std::cout << "PASS scraperx_sim pipe gallery: caught_s=" << caught_s << " lip_s=" << ledge_s
              << " top_y=" << top.player_position.y << "\n";
}

// Parkour step 3: the slide, on the proving ground's crawl beam (a 4 m lane
// at x = -2 under a beam 1.45 m up at z = -13). Sprinted at and crouched
// 3 m short, the body slides under it on its own momentum, braked only by
// sliding friction (0.35 g); crouched at a walk, it does not slide.
void run_slide() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    const double dt = Simulation::kFixedStepSeconds;
    Simulation s(InitialSpawn::StaticDeck);
    require(s.advance_frame(0.4).accepted, "slide settle");
    // Onto the lane's centre line, stopped, then down it at a sprint.
    require(s.set_facing(-1.0, 0.0) && s.set_move_input(-0.5, 0.0), "slide line-up");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.x < -1.95; }, 4.0), "slide line-up walk");
    require(s.set_move_input(0.0, 0.0) && s.advance_frame(0.5).accepted, "slide line-up stop");
    require(std::abs(s.snapshot().player_position.x + 2.0) < 0.15, "the slide must start on the lane's centre line");
    require(s.set_facing(0.0, -1.0) && s.set_move_input(0.0, -1.0) && s.set_sprint_input(true), "slide sprint");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.z < -10.0; }, 3.0), "slide run-up");
    const Snapshot run = s.snapshot();
    require(std::hypot(run.player_linear_velocity.x, run.player_linear_velocity.z) > 6.0, "the run-up must reach a sprint");
    require(s.set_crouch_input(true), "slide crouch");
    require(s.advance_frame(dt * 2).accepted, "slide onset");
    require(s.snapshot().player_sliding && s.snapshot().slide_count == 1, "crouching at a sprint must slide");
    const Snapshot a = s.snapshot();
    require(s.advance_frame(0.5).accepted, "slide half second");
    const Snapshot b = s.snapshot();
    const double va = std::hypot(a.player_linear_velocity.x, a.player_linear_velocity.z);
    const double vb = std::hypot(b.player_linear_velocity.x, b.player_linear_velocity.z);
    if (std::getenv("SLIDE_TRACE") != nullptr) {
        std::cout << "slide va=" << va << " vb=" << vb << " za=" << a.player_position.z << " zb=" << b.player_position.z << "\n";
    }
    require(std::abs((va - vb) - 0.35 * 9.81 * 0.5) < 0.3,
            "a slide must be braked by sliding friction only (0.35 g), not stopped by the crouch walk's traction");
    double ended_z = 0.0;
    require(advance_until(s,
                          [&](const Snapshot &n) {
                              ended_z = n.player_position.z;
                              return !n.player_sliding;
                          },
                          3.0),
            "the slide must run out");
    require(ended_z < -14.0, "the slide must carry the body under the crawl beam and out past it");
    require(s.snapshot().player_crouched, "a slide run out leaves the body crouched");
    require(s.snapshot().death_count == 0, "the slide is not lethal");

    // Crouched at a walk: no slide.
    Simulation w(InitialSpawn::StaticDeck);
    require(w.advance_frame(0.4).accepted && w.set_facing(0.0, -1.0) && w.set_move_input(0.0, -0.5), "walk");
    require(w.advance_frame(1.0).accepted && w.set_crouch_input(true) && w.advance_frame(0.3).accepted, "walk crouch");
    require(w.snapshot().slide_count == 0 && !w.snapshot().player_sliding, "crouching at a walk must not slide");

    std::cout << "PASS scraperx_sim slide: entry_mps=" << va << " after_half_s=" << vb << " ran_out_z=" << ended_z
              << "\n";
}

// Parkour step 3: the speed vault, over the proving ground's 0.95 m rail at
// x = 5. At a sprint the vault crosses at the run's pace and leaves at it;
// the scripted 0.38 s crossing used to carry 8 m/s down to about 1 m/s.
void run_speed_vault() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    Simulation s(InitialSpawn::VaultApproach);
    require(s.advance_frame(0.3).accepted, "speed-vault settle");
    require(s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0) && s.set_sprint_input(true), "speed-vault sprint");
    bool requested = false;
    bool vaulted = false;
    double slowest = 1.0e9;
    double entry = 0.0;
    double exit_speed = 0.0;
    for (int tick = 0; tick < 90 * 3; ++tick) {
        const Snapshot n = s.snapshot();
        if (!requested && n.ledge_available && n.player_position.x > 3.0) {
            entry = std::hypot(n.player_linear_velocity.x, n.player_linear_velocity.z);
            require(s.request_traversal(), "speed-vault request");
            requested = true;
        }
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "speed-vault tick");
        const Snapshot m = s.snapshot();
        if (m.traversal_state == TraversalState::Vaulting) {
            vaulted = true;
            slowest = std::min(slowest, m.player_linear_velocity.x);
        } else if (vaulted) {
            exit_speed = std::hypot(m.player_linear_velocity.x, m.player_linear_velocity.z);
            break;
        }
    }
    require(requested && vaulted, "a sprint at the rail must vault it");
    require(entry > 7.5, "the run-up must be a sprint");
    require(slowest > 6.0, "a speed vault must cross at the run's pace, not slow to a walk over the rail");
    require(exit_speed > 7.5, "a speed vault must leave at the run's speed");
    require(s.snapshot().player_position.x > 5.6 && s.snapshot().death_count == 0, "the speed vault must land past the rail");
    std::cout << "PASS scraperx_sim speed vault: entry_mps=" << entry << " slowest_over_mps=" << slowest
              << " exit_mps=" << exit_speed << "\n";
}

// Parkour step 4: the wall-run, on the proving ground's run wall (12 m long,
// 4 m tall, its face at z = -26.75, x = -12..0). A sprint along it, a jump
// beside it, the stick held along and into it: the feet take steps on the
// wall that slow the fall; the same jump in the open has none. Turned away
// from the wall, no step is taken.
void run_wall_run() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    const auto airtime = [](const double into_wall, const double lane_z, std::uint64_t &steps, double &far_x) {
        Simulation s(InitialSpawn::WallRunApproach);
        require(s.advance_frame(0.3).accepted, "wall-run settle");
        (void)lane_z;
        require(s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0) && s.set_sprint_input(true), "wall-run sprint");
        require(advance_until(s, [](const Snapshot &n) { return n.player_position.x > -12.5; }, 3.0), "wall-run approach");
        require(s.request_jump(), "wall-run jump");
        (void)s.advance_frame(Simulation::kFixedStepSeconds * 2);
        require(s.set_move_input(1.0, -into_wall), "wall-run stick");
        double air = 0.0;
        for (int tick = 0; tick < 90 * 3; ++tick) {
            require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "wall-run tick");
            const Snapshot n = s.snapshot();
            if (n.player_grounded) {
                break;
            }
            air += Simulation::kFixedStepSeconds;
            if (std::getenv("WALL_TRACE") != nullptr && tick % 6 == 0) {
                std::cout << "W t=" << tick << " x=" << n.player_position.x << " y=" << n.player_position.y
                          << " z=" << n.player_position.z << " vy=" << n.player_linear_velocity.y
                          << " steps=" << n.wall_step_count << " run=" << n.player_wall_running << "\n";
            }
        }
        steps = s.snapshot().wall_step_count;
        far_x = s.snapshot().player_position.x;
        return air;
    };
    std::uint64_t wall_steps = 0;
    double wall_x = 0.0;
    const double wall_air = airtime(0.4, -26.3, wall_steps, wall_x);
    std::uint64_t away_steps = 0;
    double away_x = 0.0;
    const double away_air = airtime(-0.4, -26.3, away_steps, away_x);
    require(wall_steps >= 2, "running along the wall, the feet must take steps on it");
    require(away_steps == 0, "turned away from the wall, no step may be taken");
    // Each step stops at most 0.8 x 1.2 m/s of fall, and only as much as there
    // is: two or three steps keep the body up about a tenth of a second more.
    require(wall_air > away_air + 0.1, "steps on the wall must keep the body up longer than the same jump turned away");

    // Kicked off: a jump while on the wall throws the body out from it and up.
    Simulation k(InitialSpawn::WallRunApproach);
    require(k.advance_frame(0.3).accepted && k.set_facing(1.0, 0.0) && k.set_move_input(1.0, 0.0) &&
                k.set_sprint_input(true),
            "kick run");
    require(advance_until(k, [](const Snapshot &n) { return n.player_position.x > -12.5; }, 3.0), "kick approach");
    require(k.request_jump() && k.advance_frame(Simulation::kFixedStepSeconds * 2).accepted &&
                k.set_move_input(1.0, -0.4),
            "kick jump");
    require(advance_until(k, [](const Snapshot &n) { return n.player_wall_running; }, 1.5), "kick: on the wall");
    const Snapshot before_kick = k.snapshot();
    require(k.request_jump() && k.advance_frame(Simulation::kFixedStepSeconds).accepted, "the kick");
    const Snapshot after_kick = k.snapshot();
    require(after_kick.player_linear_velocity.z - before_kick.player_linear_velocity.z > 2.0,
            "a kick off the wall must throw the body out from it");
    require(after_kick.player_linear_velocity.y > before_kick.player_linear_velocity.y + 1.5,
            "a kick off the wall must lift the body by the wall's friction on the push");
    std::cout << "PASS scraperx_sim wall run: steps=" << wall_steps << " air_s=" << wall_air << " away_air_s=" << away_air
              << " far_x=" << wall_x << "\n";
}

// Parkour step 5: the grappling hook on the proving ground. Taken from its
// rack beside the monkey bars; fired at a bar it bites round it; reeled, it
// lifts the body off the deck no faster than the reel turns and at no more
// line tension than the reel's motor gives; let go, the body falls. Fired at
// the sky it bites nothing and comes back; fired at the plain face of the run
// wall it glances off -- not a hold.
void run_hook() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    const double dt = Simulation::kFixedStepSeconds;
    Simulation s(InitialSpawn::SwingApproach);
    require(s.advance_frame(0.4).accepted, "hook settle");
    require(!s.request_hook_fire(), "without the hook nothing fires");
    require(s.request_hook_take() && s.advance_frame(dt).accepted && s.snapshot().has_hook,
            "the hook must be taken from its rack in reach");
    // Aim from the eye at bar 3 (x 3, y 3.1, z -20).
    const auto aim_at = [&](const double x, const double y, const double z) {
        const auto p = s.snapshot().player_position;
        require(s.set_hook_aim(x - p.x, y - (p.y + 0.6), z - p.z), "hook aim");
    };
    aim_at(3.0, 3.1, -20.0);
    require(s.request_hook_fire(), "the hook must fire");
    require(advance_until(s, [](const Snapshot &n) { return n.hook_state == 2; }, 1.0), "fired at a bar, the hook must bite");
    const auto bit = s.snapshot().hook_position;
    require(std::abs(bit.x - 3.0) < 0.25 && std::abs(bit.y - 3.1) < 0.25, "the hook must bite at the bar it hit");
    const double y0 = s.snapshot().player_position.y;
    require(s.set_hook_reel(true), "reel");
    // The line as it lies, chest to hook (the snapshot's own points).
    const auto line_lies = [](const Snapshot &n) {
        double length = 0.0;
        for (std::uint32_t i = 1; i < n.hook_line_point_count; ++i) {
            const auto a = n.hook_line_points[i - 1];
            const auto b = n.hook_line_points[i];
            length += std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
        }
        return length;
    };
    // The reel is a winch (kHookReelForceN): it drags the soles toward the
    // bar (a line carrying part of the weight takes that share of their
    // grip), lifts the body and swings it under the bar. What it may not do
    // is take the line in faster than it turns.
    double worst_reel = 0.0;
    double worst_tension = 0.0;
    double peak_y = y0;
    double last_line = line_lies(s.snapshot());
    for (int tick = 0; tick < 90 * 3; ++tick) {
        require(s.advance_frame(dt).accepted, "reel tick");
        const Snapshot n = s.snapshot();
        const double line = line_lies(n);
        worst_reel = std::max(worst_reel, (last_line - line) / dt);
        worst_tension = std::max(worst_tension, n.hook_tension_n);
        peak_y = std::max(peak_y, n.player_position.y);
        last_line = line;
    }
    const Snapshot reeled = s.snapshot();
    require(peak_y > y0 + 0.8 && !reeled.player_grounded,
            "reeled in, the hook must lift the body off the deck");
    require(worst_reel < 3.0 * 1.15, "the reel must take the line in no faster than it turns");
    require(s.set_hook_reel(false) && s.request_hook_release(), "let go");
    require(s.advance_frame(0.3).accepted, "let-go fall");
    require(s.snapshot().hook_state == 0 && s.snapshot().player_linear_velocity.y < -0.5,
            "let go, the hook comes back and the body falls");
    require(advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 2.0), "down again");
    // Reeled, the winch swung the body in under the bars: back to the open
    // deck it was taken from before firing anywhere else.
    require(walk_to(s, -1.0, -20.0, 8.0), "back out from under the bars");

    // At the sky: no bite, and it comes back.
    aim_at(-1.0, 50.0, -20.0);
    require(s.request_hook_fire(), "sky fire");
    require(s.advance_frame(3.0).accepted, "sky flight");
    require(s.snapshot().hook_state == 0 && s.snapshot().hook_bite_count == 1, "fired at the sky, nothing may bite");

    // At the run wall's plain face (z -26.75): no bite.
    aim_at(-6.0, 2.0, -26.75);
    require(s.request_hook_fire(), "wall fire");
    require(s.advance_frame(1.0).accepted, "wall flight");
    require(s.snapshot().hook_bite_count == 1, "a plain wall is not a hold: the hook must not bite on it");
    // In the game: found on the 220 ring's north-west corner. Walked to from
    // where C's platform sets a rider down, taken (the gun leaves its rack),
    // kept through a save and a load.
    Simulation g(InitialSpawn::Ring220North);
    require(g.advance_frame(0.4).accepted && !g.snapshot().has_hook, "the game starts without the hook");
    require(walk_to(g, -21.6, -127.6, 14.0, 0.3), "the rider must walk the 220 ring's north band to the hook rack");
    require(g.request_hook_take() && g.advance_frame(Simulation::kFixedStepSeconds).accepted && g.snapshot().has_hook,
            "the hook must be taken from its rack on the 220 ring");
    const auto saved = g.save_game();
    Simulation l(InitialSpawn::Ring220North);
    require(l.advance_frame(0.1).accepted && l.load_game(saved.data(), saved.size()), "the save must load");
    require(l.advance_frame(Simulation::kFixedStepSeconds).accepted && l.snapshot().has_hook,
            "the hook must be kept through a save and a load");

    // A real rope (task #102): bitten at bar 0 from 5 m west of it and
    // walked south-east past the frame's west post, the line catches on
    // the post's edge instead of passing through it; walked back, it comes
    // off. Checked against the post's own box here, not the native's test:
    // no stretch of the line may cross it (the box is taken 1 cm in, the
    // bend sits 3 cm off its face). Round the bend, the line as it lies is
    // never longer than the rope out.
    {
        Simulation w(InitialSpawn::SwingApproach);
        require(w.advance_frame(0.4).accepted && w.request_hook_take() && w.advance_frame(dt).accepted &&
                    w.snapshot().has_hook,
                "wrap: the hook must be taken");
        require(walk_to(w, -5.0, -20.0, 8.0, 0.1), "wrap: back off 5 m west of the bars");
        const auto p = w.snapshot().player_position;
        require(w.set_hook_aim(0.0 - p.x, 3.1 - (p.y + 0.6), -20.0 - p.z) && w.request_hook_fire(), "wrap: fire");
        require(advance_until(w, [](const Snapshot &n) { return n.hook_state == 2; }, 1.0), "wrap: bite on bar 0");
        const double rope = w.snapshot().hook_line_length;
        // The west post nearer the walk: x -0.9, z -19.26, 0.12 half, 3.38 tall.
        const auto crosses_post = [](const scraperx::sim::Vector3 a, const scraperx::sim::Vector3 b) {
            const double lo[3] = {-1.02 + 0.01, 0.0 + 0.01, -19.38 + 0.01};
            const double hi[3] = {-0.78 - 0.01, 3.38 - 0.01, -19.14 - 0.01};
            const double from[3] = {a.x, a.y, a.z};
            const double run[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
            double t0 = 0.0;
            double t1 = 1.0;
            for (int k = 0; k < 3; ++k) {
                if (std::abs(run[k]) < 1.0e-12) {
                    if (from[k] < lo[k] || from[k] > hi[k]) {
                        return false;
                    }
                    continue;
                }
                double ta = (lo[k] - from[k]) / run[k];
                double tb = (hi[k] - from[k]) / run[k];
                if (ta > tb) {
                    std::swap(ta, tb);
                }
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1) {
                    return false;
                }
            }
            return true;
        };
        int crossings = 0;
        std::uint32_t most_bends = 0;
        double worst_over = 0.0;
        const auto watch = [&](const Snapshot &n) {
            for (std::uint32_t i = 1; i < n.hook_line_point_count; ++i) {
                crossings += crosses_post(n.hook_line_points[i - 1], n.hook_line_points[i]) ? 1 : 0;
            }
            most_bends = std::max(most_bends, n.hook_bend_count);
            worst_over = std::max(worst_over, line_lies(n) - n.hook_line_length);
            return false;
        };
        const auto walk_watching = [&](const double x, const double z, const double seconds) {
            for (int tick = 0; tick < static_cast<int>(seconds / dt); ++tick) {
                const auto at = w.snapshot().player_position;
                if (std::hypot(x - at.x, z - at.z) < 0.15) {
                    break;
                }
                steer_toward(w, x, z);
                require(w.advance_frame(dt).accepted, "wrap walk tick");
                (void)watch(w.snapshot());
            }
            (void)w.set_move_input(0.0, 0.0);
        };
        walk_watching(-3.5, -17.2, 6.0);
        const Snapshot wrapped = w.snapshot();
        require(most_bends >= 1 && wrapped.hook_wrap_count >= 1,
                "walked past the post, the line must catch on its edge");
        walk_watching(-5.0, -20.0, 6.0);
        const Snapshot back = w.snapshot();
        require(back.hook_bend_count == 0 && back.hook_unwrap_count >= 1,
                "walked back, the line must come off the post");
        require(crossings == 0, "no stretch of the line may pass through the post");
        require(worst_over < 0.02, "the line as it lies must never be longer than the rope out");
        require(std::abs(back.hook_line_length - rope) < 0.02,
                "round the bend and off it again, no rope may be made or lost");
        std::cout << "PASS scraperx_sim hook rope: bends=" << most_bends << " wraps=" << wrapped.hook_wrap_count
                  << " unwraps=" << back.hook_unwrap_count << " crossings=" << crossings
                  << " worst_over_m=" << worst_over << "\n";
    }
    // The bite is not a weld (kHookHoldForceN). From the 220 ring's rack, bite
    // a member over the well, then walk in under it (the rope stays as long as
    // it was) and step off the ring's inner edge: the long fall caught on the
    // line tears the tines off the member. The same bite with the rope taut,
    // walked off the ring's south edge (a short drop over the lip), holds.
    for (const bool slack : {true, false}) {
        Simulation r(InitialSpawn::Ring220North);
        require(r.advance_frame(0.4).accepted && walk_to(r, -21.6, -127.6, 14.0, 0.3) &&
                    r.request_hook_take() && r.advance_frame(dt).accepted && r.snapshot().has_hook,
                "hook hold: the hook taken from its rack");
        const auto at = r.snapshot().player_position;
        require(r.set_hook_aim(-6.0 - at.x, 226.0 - (at.y + 0.6), -133.0 - at.z) && r.request_hook_fire(),
                "hook hold: fired over the well");
        require(advance_until(r, [](const Snapshot &n) { return n.hook_state == 2; }, 1.0),
                "hook hold: the hook bites the member over the well");
        const double rope = r.snapshot().hook_line_length;
        if (slack) {
            require(walk_to(r, -6.0, -129.5, 10.0, 0.2), "hook hold: in under the bite, the rope gone slack");
            (void)hold_stick(r, 0.0, -1.0, 0.0, -1.0, 3.0, [](const Snapshot &n) { return !n.player_grounded; });
            double worst = 0.0;
            bool torn = false;
            for (int tick = 0; tick < 90 * 4 && !torn; ++tick) {
                require(r.advance_frame(dt).accepted, "hook hold: fall tick");
                worst = std::max(worst, r.snapshot().hook_tension_n);
                torn = r.snapshot().hook_tear_count > 0;
            }
            std::cout << "  hook hold: slack fall of " << rope << " m of rope, peak pull " << worst << " N, torn="
                      << torn << "\n";
            require(torn, "hook hold: a long fall caught on the line must tear the hook off its member");
            require(r.snapshot().hook_state != 2 || r.snapshot().hook_bite_count > 1,
                    "hook hold: torn off, the hook is loose (in flight, or caught again somewhere new)");
        } else {
            (void)walk_to(r, -19.0, -125.0, 4.0, 0.3);
            (void)r.advance_frame(3.0);
            const Snapshot n = r.snapshot();
            std::cout << "  hook hold: off the south edge, hung at y=" << n.player_position.y << " bends="
                      << n.hook_bend_count << " tears=" << n.hook_tear_count << "\n";
            require(n.hook_state == 2 && n.hook_tear_count == 0 && n.death_count == 0,
                    "hook hold: a short drop over the lip on a taut line must hold");
        }
    }
    std::cout << "PASS scraperx_sim hook: lifted_m=" << peak_y - y0 << " worst_reel_mps=" << worst_reel
              << " worst_tension_n=" << worst_tension << "\n";
}

// Life L2: the yard crew. Real bodies on grade: they walk their routes with no
// shove from anything when the player leaves them alone, step out of a walking
// player's way, flinch at a hard landing near them, and stumble when run
// into -- then go back to work.
void run_workers() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    const double dt = Simulation::kFixedStepSeconds;
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.2).accepted && s.snapshot().worker_count == 14,
                "six of the yard crew, three on the decks, three on the high rings, two welders in the Plate Shop");
        double fastest = 0.0;
        for (int tick = 0; tick < 90 * 30; ++tick) {
            require(s.advance_frame(dt).accepted, "crew tick");
            const Snapshot n = s.snapshot();
            for (std::size_t w = 0; w < n.worker_count; ++w) {
                fastest = std::max(fastest, n.workers[w].speed);
            }
        }
        const Snapshot n = s.snapshot();
        require(n.worker_stumbles == 0 && n.worker_startles == 0 && n.worker_yields == 0,
                "left alone, nothing may shove, startle or turn aside the crew");
        require(fastest < 1.5 * 1.1, "the crew walks, at most its walking pace");
        for (std::size_t w = 0; w < n.worker_count; ++w) {
            const double floor = w < 6    ? 0.0
                                 : w < 8  ? 22.0
                                 : w < 9  ? 44.0
                                 : w < 10 ? 220.25
                                 : w < 12 ? 264.25
                                 : w < 13 ? 374.25
                                          : 418.25;
            require(n.workers[w].position.y > floor + 0.8 && n.workers[w].position.y < floor + 1.4,
                    "every worker is on its floor (or stepping over a kerb)");
        }
        require(n.workers[0].stride > 20.0 && n.workers[4].stride > 20.0 && n.workers[6].stride > 20.0 &&
                    n.workers[8].stride > 20.0 && n.workers[9].stride > 15.0 && n.workers[10].stride > 15.0,
                "the walkers walk their routes, on the yard, the decks and the rings");
        std::cout << "  crew 30 s: foreman_m=" << n.workers[0].stride << " runner_m=" << n.workers[4].stride
                  << " fastest_mps=" << fastest << "\n";
    }
    {
        // A worker never steps off an open edge. The 374 ring's welder kneels
        // 0.7 m from the well. A player walking past on the band's outer side
        // would have it step 1.1 m aside, away from them: into the well. Its
        // feet find no floor there, so it holds its ground on the ring.
        Simulation s(InitialSpawn::Ring374West);
        require(s.advance_frame(0.2).accepted, "settle on the 374 ring");
        const std::size_t welder = 12;
        require(walk_to(s, -14.9, -164.9, 20.0, 0.4) && walk_to(s, 14.9, -164.9, 20.0, 0.4) &&
                    walk_to(s, 15.8, -161.0, 10.0, 0.3),
                "round the 374 ring to its east band");
        const Snapshot before = s.snapshot();
        require(walk_to(s, 15.8, -138.0, 15.0, 0.3), "along the band's outer side, past the welder");
        (void)s.advance_frame(3.0);
        const Snapshot n = s.snapshot();
        std::cout << "  welder at the edge: yields=" << n.worker_yields - before.worker_yields
                  << " welder=(" << n.workers[welder].position.x << "," << n.workers[welder].position.y << ","
                  << n.workers[welder].position.z << ")\n";
        require(n.worker_yields > before.worker_yields, "the welder turns aside for the passing player");
        require(n.workers[welder].position.y > 374.25 + 0.8 && n.workers[welder].position.x > 12.9,
                "and stays on its ring: it never steps into the well");
    }
    {
        // Phase 4: struck by something moving. Standing still in the runner's
        // lane, the player is walked into at 1.45 m/s by an 80 kg body: the
        // blow moves the XCoM past half a foot and the player stumbles. The
        // walk across the yard to get there, past kerbs and walls, is never a
        // shove: a wall does not close.
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.2).accepted, "settle");
        require(walk_to(s, -30.0, -60.0, 20.0, 0.15), "to the runner's lane");
        const auto standing = s.snapshot();
        require(standing.shove_count == 0, "walking across the yard is never a shove");
        bool shoved = false;
        for (int tick = 0; tick < 90 * 45 && !shoved; ++tick) {
            (void)s.set_move_input(0.0, 0.0);
            require(s.advance_frame(dt).accepted, "tick");
            shoved = s.snapshot().shove_count > 0;
        }
        (void)s.advance_frame(0.3);
        const Snapshot n = s.snapshot();
        std::cout << "  shoved by the runner: shoves=" << n.shove_count << " stumbles="
                  << n.stumble_count - standing.stumble_count << " loss=" << n.landing_balance_loss << "\n";
        require(shoved && n.stumble_count > standing.stumble_count && n.death_count == 0,
                "walked into by a worker, the standing player must stumble, and live");
    }
    {
        // Walking at the foreman as he sets off from his first station: he
        // steps aside, and nobody is shoved.
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(2.5).accepted, "yield settle");
        for (int tick = 0; tick < 90 * 10 && s.snapshot().worker_yields == 0; ++tick) {
            const auto p = s.snapshot().player_position;
            const auto w = s.snapshot().workers[0].position;
            (void)s.set_move_input(w.x - p.x, w.z - p.z);
            require(s.advance_frame(dt).accepted, "approach tick");
        }
        (void)s.set_move_input(0.0, 0.0);
        require(s.snapshot().worker_yields >= 1, "a walking player coming at a worker: it must step aside");
        require(s.advance_frame(2.0).accepted, "yield after");
        const Snapshot n = s.snapshot();
        require(n.worker_stumbles == 0, "stepping aside in time, nobody is shoved");
        require(advance_until(s, [](const Snapshot &m) { return m.workers[0].state <= 1; }, 8.0),
                "the player gone by, he goes back to his walk");
    }
    {
        // A hard landing beside the foreman's first station.
        Simulation s(InitialSpawn::SurvivableDrop);
        require(advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 4.0), "drop lands");
        require(s.advance_frame(0.1).accepted && s.snapshot().worker_startles >= 1,
                "a hard landing near the crew: someone flinches");
    }
    {
        // Sprinting straight into the rigger at the slingshot's anchor.
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.2).accepted, "shove settle");
        const auto rigger = s.snapshot().workers[1].position;
        require(walk_to(s, rigger.x, rigger.z + 14.0, 12.0, 0.4), "line up on the rigger");
        require(s.set_sprint_input(true), "sprint");
        const bool hit = advance_until(s, [](const Snapshot &n) { return n.worker_stumbles >= 1; }, 0.1);
        (void)hit;
        for (int tick = 0; tick < 90 * 4 && s.snapshot().worker_stumbles == 0; ++tick) {
            const auto p = s.snapshot().player_position;
            const auto w = s.snapshot().workers[1].position;
            (void)s.set_move_input(w.x - p.x, w.z - p.z);
            require(s.advance_frame(dt).accepted, "sprint tick");
        }
        (void)s.set_sprint_input(false);
        (void)s.set_move_input(0.0, 0.0);
        const Snapshot hit_at = s.snapshot();
        require(hit_at.worker_stumbles >= 1, "run into, a worker must stumble");
        require(advance_until(s, [](const Snapshot &n) { return n.workers[1].state <= 1; }, 6.0),
                "a stumbled worker recovers and goes back to work");
        std::cout << "PASS scraperx_sim workers: startles=" << hit_at.worker_startles
                  << " stumbles=" << hit_at.worker_stumbles << "\n";
    }
}

// C7, the west face: from the 264 ring to the mast-pitman walkway at
// 291.25 m on player inputs, the mast's alternative. Its two galleries are
// swung hand over hand: gallery one from a running leap, gallery two, rising
// 0.3 m a pipe, from a standing leap off the outrigger.
bool report_c7(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cerr << "C7 stalled at " << leg << ": (" << state.player_position.x << ", " << state.player_position.y << ", "
              << state.player_position.z << ") grounded=" << state.player_grounded
              << " traversal=" << static_cast<int>(state.traversal_state) << " support=" << state.support_entity_id
              << '\n';
    return false;
}

// Held forward along -z: the hands swing pipe to pipe until they take a lip.
bool c7_swing_to_lip(scraperx::sim::Simulation &simulation, int *reaches) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 1.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Swinging; })) {
        return false;
    }
    const auto start = simulation.snapshot().swing_reach_count;
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 14.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        return false;
    }
    *reaches = static_cast<int>(simulation.snapshot().swing_reach_count - start);
    return true;
}

bool climb_c7(scraperx::sim::Simulation &simulation, int *reaches_one, int *reaches_two) {
    using scraperx::sim::ClimbC7;
    using scraperx::sim::Snapshot;
    // 1. South along the 264 ring's west band, under the galleries, to the
    // kentledge, and onto it.
    if (!(walk_to(simulation, -19.2, -155.0, 8.0) && walk_to(simulation, -17.6, -150.0, 4.0) &&
          walk_to(simulation, -17.6, -133.4, 12.0) && walk_to(simulation, -19.2, -133.6, 4.0, 0.06))) {
        return report_c7(simulation, "to the kentledge");
    }
    if (!c4_mantle(simulation, 0.0, -1.0, ClimbC7::kKentledgeTop + 0.45, "C7 onto the kentledge")) {
        return false;
    }
    // 2. A run north along it and, from its striped end, the leap: the hands
    // close on gallery one's first pipe, swing along it, take the lip.
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.2);
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 2.0, [](const Snapshot &state) {
            return state.player_position.z <= ClimbC7::kKentledgeNorth + 0.5;
        })) {
        return report_c7(simulation, "the run along the kentledge");
    }
    (void)simulation.request_jump();
    if (!c7_swing_to_lip(simulation, reaches_one)) {
        return report_c7(simulation, "gallery one");
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 3.0, [](const Snapshot &state) {
            return standing_above(state, ClimbC7::kPlatformOneTop + 0.4);
        })) {
        return report_c7(simulation, "up onto platform one");
    }
    // 3. Up onto the outrigger from platform one's north edge, and out west.
    if (!(walk_to(simulation, ClimbC7::kLaneOneX, -147.4, 4.0, 0.06) &&
          c4_hang_up(simulation, 0.0, -1.0, ClimbC7::kOutriggerTop + 0.4, "C7 up onto the outrigger"))) {
        return false;
    }
    if (!(walk_to(simulation, -24.0, -149.1, 6.0) &&
          walk_to(simulation, ClimbC7::kLaneTwoX, ClimbC7::kGalleryTwoFirstZ, 6.0, 0.06))) {
        return report_c7(simulation, "out along the outrigger");
    }
    // 4. A standing leap to gallery two's first pipe overhead, and up it.
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!c7_swing_to_lip(simulation, reaches_two)) {
        return report_c7(simulation, "gallery two");
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 3.0, [](const Snapshot &state) {
            return standing_above(state, ClimbC7::kPlatformTwoTop + 0.4);
        })) {
        return report_c7(simulation, "up onto platform two");
    }
    // 5. Platform to platform: east onto three, north onto four, east onto
    // the 286 ring's west band.
    if (!(walk_to(simulation, -28.4, -158.4, 4.0, 0.06) &&
          c4_hang_up(simulation, 1.0, 0.0, ClimbC7::kPlatformThreeTop + 0.4, "C7 up onto platform three"))) {
        return false;
    }
    if (!(walk_to(simulation, -23.5, -159.7, 6.0, 0.06) &&
          c4_hang_up(simulation, 0.0, -1.0, ClimbC7::kPlatformFourTop + 0.4, "C7 up onto platform four"))) {
        return false;
    }
    if (!(walk_to(simulation, ClimbC7::kRing286Edge - 0.6, -161.9, 6.0, 0.06) &&
          c4_hang_up(simulation, 1.0, 0.0, ClimbC7::kRing286Top + 0.4, "C7 up onto the 286 ring"))) {
        return false;
    }
    // 6. South along the band to the block, onto it, and up onto the walkway.
    if (!(walk_to(simulation, -17.5, -161.9, 4.0) && walk_to(simulation, -17.5, -143.6, 12.0) &&
          walk_to(simulation, -19.0, -143.6, 4.0, 0.06))) {
        return report_c7(simulation, "along the 286 ring to the block");
    }
    if (!c4_mantle(simulation, 0.0, -1.0, ClimbC7::kBlockTop + 0.45, "C7 onto the block")) {
        return false;
    }
    if (!walk_to(simulation, -19.0, -147.4, 4.0, 0.06)) {
        return report_c7(simulation, "up the block's step");
    }
    if (!c4_hang_up(simulation, 0.0, -1.0, ClimbC7::kWalkwayTop + 0.4, "C7 up onto the walkway")) {
        return false;
    }
    return true;
}

void run_c7() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation s(InitialSpawn::Ring264West);
    require(s.advance_frame(1.0).accepted, "C7: settle");
    const double start = s.snapshot().simulation_time_seconds;
    int one = 0;
    int two = 0;
    require(climb_c7(s, &one, &two), "C7: the climb from the 264 ring to the walkway on player inputs");
    const auto top = s.snapshot();
    require(top.death_count == 0 && top.player_grounded &&
                top.support_entity_id == scraperx::sim::vertical::Route::kMastPitmanWalkway &&
                top.player_position.y > 291.9,
            "C7: the climb must end standing on the walkway, alive, without the mast");
    require(one == scraperx::sim::ClimbC7::kGalleryOnePipes - 1 && two == scraperx::sim::ClimbC7::kGalleryTwoPipes - 1,
            "C7: every pipe of both galleries must be swung, none skipped");
    require(std::abs(s.lift_state().travels[0]) < 0.005, "C7: the mast never moved");
    // Pressure from the world: out over the drop the wind off the west face
    // pushed the body while it was off its feet (in the air, on the pipes).
    require(top.wind_peak_force_n > 50.0, "C7: out over the drop the wind must push the climber");
    // The galleries are the way: from the band under platform one, a jump at
    // its fascia catches nothing.
    {
        Simulation band(InitialSpawn::Ring264West);
        (void)band.advance_frame(1.0);
        require(walk_to(band, -19.2, -155.0, 8.0) && walk_to(band, -19.2, -144.2, 8.0, 0.08),
                "C7 falsifier: to under platform one's lip");
        (void)band.set_facing(0.0, -1.0);
        (void)band.advance_frame(0.3);
        (void)band.request_jump();
        require(!hold_stick(band, 0.0, -0.4, 0.0, -1.0, 2.0,
                            [](const scraperx::sim::Snapshot &n) {
                                return n.traversal_state == scraperx::sim::TraversalState::Hanging;
                            }) &&
                    band.snapshot().player_position.y < 265.5,
                "C7 falsifier: from the band, platform one is out of reach");
    }
    std::cout << "PASS scraperx_sim c7: seconds=" << top.simulation_time_seconds - start << " pipes=" << one << "+" << two
              << " top_y=" << top.player_position.y << " wind_peak_n=" << top.wind_peak_force_n << "\n";
}

// Jump pressed in the air just before the feet land is kept and spent on the
// first tick they stand; pressed too early it is gone; it never jumps in the
// air.
void run_jump_buffer() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    const double dt = Simulation::kFixedStepSeconds;
    // A plain jump on the flat grade: how many ticks from the press to the
    // feet back down.
    Simulation probe(InitialSpawn::ExteriorGrade);
    (void)probe.advance_frame(1.0);
    require(probe.snapshot().player_grounded, "jump buffer: the body must start on the grade");
    (void)probe.request_jump();
    int airborne = 0;
    int land_tick = -1;
    for (int t = 1; t < 300 && land_tick < 0; ++t) {
        (void)probe.advance_frame(dt);
        airborne = probe.snapshot().player_grounded ? airborne : airborne + 1;
        if (airborne > 5 && probe.snapshot().player_grounded) {
            land_tick = t;
        }
    }
    require(land_tick > 40, "jump buffer: a plain jump must leave the grade and come down");
    // Pressed `early` ticks before that landing, mid-air: true if the body
    // took off again within a few ticks of landing, and never rose in the air.
    const auto pressed = [&](const int early, bool &rose_in_air) {
        Simulation sim(InitialSpawn::ExteriorGrade);
        (void)sim.advance_frame(1.0);
        (void)sim.request_jump();
        double last_vy = 1.0e9;
        rose_in_air = false;
        bool again = false;
        for (int t = 1; t < land_tick + 30; ++t) {
            if (t == land_tick - early) {
                require(!sim.snapshot().player_grounded, "jump buffer: the early press must be made in the air");
                (void)sim.request_jump();
            }
            (void)sim.advance_frame(dt);
            const auto now = sim.snapshot();
            const double vy = now.player_linear_velocity.y;
            if (t < land_tick && t > 2) {
                rose_in_air = rose_in_air || vy > last_vy + 1e-3;
            }
            last_vy = vy;
            again = again || (t >= land_tick && vy > 4.0);
        }
        return again;
    };
    bool rose_late = false;
    bool rose_early = false;
    const bool late = pressed(5, rose_late);
    const bool early = pressed(25, rose_early);
    require(!rose_late && !rose_early, "jump buffer: a press in the air must never lift the body in the air");
    require(late, "jump buffer: jump pressed 5 ticks before landing must take off on landing");
    require(!early, "jump buffer: jump pressed 25 ticks before landing must be gone by then");
    std::cout << "PASS scraperx_sim jump buffer: land_tick=" << land_tick << " window_ticks=9\n";
}

// PARKOUR_MOVE_SET.md, the seams between moves. Each proof fails without its
// change: a running mantle that stops on top (M5), a lip missed while still
// rising (M6), a kick that goes only straight out from a wall (M7), and a
// jump at a sighted landing that is the plain jump (M3).
void run_parkour_seams() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    const double dt = Simulation::kFixedStepSeconds;

    // M5: a mantle made at a run leaves at a run. The mantle ledge's face is
    // at x = 9.0 (top 1.55 m); from the spawn 1.1 m short of it, running east,
    // the ledge is taken at a run and the body is still moving east on top.
    {
        Simulation m(InitialSpawn::MantleApproach);
        require(m.set_facing(1.0, 0.0) && m.advance_frame(0.5).accepted, "running mantle settle");
        require(m.set_move_input(-1.0, 0.0) && advance_until(m, [](const Snapshot &n) { return n.player_position.x < 5.9; }, 3.0),
                "running mantle: back off from the ledge");
        require(m.set_move_input(1.0, 0.0) && m.set_sprint_input(true), "running mantle: run at it");
        // Taken the moment it is offered at a run, as a player's thumb does.
        double entry_speed = 0.0;
        require(advance_until(m, [&](const Snapshot &n) {
                    if (n.traversal_state == TraversalState::Mantling) {
                        return true;
                    }
                    entry_speed = n.player_linear_velocity.x;
                    if (n.ledge_available && n.ledge_entity_id == Simulation::kMantleLedgeEntityId) {
                        (void)m.request_traversal();
                    }
                    return false;
                }, 4.0),
                "running at the mantle ledge, the body must take it");
        require(entry_speed > 5.0, "the mantle must have been entered at a run");
        require(advance_until(m, [](const Snapshot &n) {
                    return n.player_grounded && n.support_entity_id == Simulation::kMantleLedgeEntityId;
                }, 2.5),
                "the running mantle must end on the ledge");
        const Snapshot top = m.snapshot();
        require(top.player_linear_velocity.x > 2.5,
                "a mantle made at a run must leave at a run, not standing (x speed on top)");
        std::cout << "PASS scraperx_sim parkour running mantle: entry_mps=" << entry_speed
                  << " exit_mps=" << top.player_linear_velocity.x << "\n";
    }

    // M6: the lip is caught on the way up. From the deck under the hang
    // ledge's lip (its top at 3.6 m, the deck at 0), a sprint jump at it
    // catches with the body still rising, and before the apex of the jump.
    {
        Simulation h(InitialSpawn::HangApproach);
        require(h.advance_frame(1.0).accepted, "rising catch settle");
        // The hang ledge's face is at x = 8.5; walk to 1.2 m short of it on
        // the deck below, then run and jump at the lip.
        require(walk_to(h, 5.8, 4.0, 4.0, 0.1), "rising catch: back from the ledge's foot");
        require(h.set_facing(1.0, 0.0) && h.set_move_input(1.0, 0.0) && h.set_sprint_input(true), "rising catch run");
        require(advance_until(h, [](const Snapshot &n) { return n.player_position.x > 7.3; }, 3.0),
                "rising catch: up to the face");
        require(h.request_jump(), "rising catch jump");
        double catch_vy = -100.0;
        double peak_y = -100.0;
        const bool caught = advance_until(h, [&](const Snapshot &n) {
            peak_y = std::max(peak_y, n.player_position.y);
            if (n.traversal_state == TraversalState::Hanging) {
                catch_vy = n.player_linear_velocity.y;
                return true;
            }
            return false;
        }, 2.0);
        require(caught, "a sprint jump at the lip must catch it");
        require(catch_vy > 0.3, "the lip must be caught while the body is still rising");
        std::cout << "PASS scraperx_sim parkour rising catch: catch_vy=" << catch_vy << " peak_y=" << peak_y << "\n";
    }

    // M7: the tic-tac. On the run wall (its face at z = -26.75, run along +x),
    // a kick with the stick held back along the wall (-x) sends the body
    // out (+z) and back (-x); straight out with the stick neutral.
    {
        const auto kick = [&](const double stick_x, double &vx_change, double &vz_change, std::uint64_t &kicks) {
            Simulation k(InitialSpawn::WallRunApproach);
            require(k.advance_frame(0.3).accepted && k.set_facing(1.0, 0.0) && k.set_move_input(1.0, 0.0) &&
                        k.set_sprint_input(true),
                    "tic-tac run");
            require(advance_until(k, [](const Snapshot &n) { return n.player_position.x > -12.5; }, 3.0),
                    "tic-tac approach");
            require(k.request_jump() && k.advance_frame(dt * 2).accepted && k.set_move_input(1.0, -0.4), "tic-tac jump");
            require(advance_until(k, [](const Snapshot &n) { return n.player_wall_running; }, 1.5), "tic-tac: on the wall");
            const Snapshot before = k.snapshot();
            require(k.set_move_input(stick_x, -0.4) && k.request_jump() && k.advance_frame(dt).accepted, "the kick");
            const Snapshot after = k.snapshot();
            vx_change = after.player_linear_velocity.x - before.player_linear_velocity.x;
            vz_change = after.player_linear_velocity.z - before.player_linear_velocity.z;
            kicks = after.wall_kick_count;
        };
        double back_vx = 0.0, back_vz = 0.0, neutral_vx = 0.0, neutral_vz = 0.0;
        std::uint64_t back_kicks = 0, neutral_kicks = 0;
        kick(-1.0, back_vx, back_vz, back_kicks);
        kick(0.0, neutral_vx, neutral_vz, neutral_kicks);
        require(back_kicks == 1 && neutral_kicks == 1, "each run must kick once");
        require(back_vz > 2.0 && neutral_vz > 2.0, "a kick must throw the body out from the wall");
        require(back_vx < neutral_vx - 1.5,
                "the stick held back along the wall must turn the kick back along it (the tic-tac)");
        std::cout << "PASS scraperx_sim parkour tic-tac: back_dvx=" << back_vx << " neutral_dvx=" << neutral_vx
                  << " out_dvz=" << back_vz << "\n";
    }

    // M3: the precision jump. From the deck east of the vault rail (top at
    // 0.95, 2.5 m long in z), standing 3.0 m short of the rail's top and
    // looking down at it, the sighted landing is offered; JUMP lands the feet
    // on the rail's top, a jump the plain jump (2.2 m from a stand at most
    // ~1.5 m out) does not make.
    {
        const auto attempt = [&](const bool look_at_it, bool &sighted, double &landed_x, bool &on_rail,
                                 std::uint64_t &precise) {
            Simulation j(InitialSpawn::VaultApproach);
            require(j.advance_frame(0.5).accepted, "precision settle");
            // Walk west to 2.4 m short of the rail's top (its west face at
            // x = 4.78; its top centre line at x = 5.0).
            require(walk_to(j, 2.0, -6.0, 4.0, 0.1), "precision: to the mark");
            require(j.set_facing(1.0, 0.0) && j.set_move_input(0.0, 0.0), "precision facing");
            // The eyes find the rail's top: the look is lowered until the
            // sighted point lies on the rail (a player's eyes settle on it).
            sighted = false;
            if (look_at_it) {
                for (double pitch = -0.15; pitch > -0.6 && !sighted; pitch -= 0.01) {
                    require(j.set_look_pitch(pitch) && j.advance_frame(dt * 2).accepted, "precision look");
                    const Snapshot n = j.snapshot();
                    sighted = n.precision_sighted && n.precision_point.y > 0.9 && n.precision_point.x > 4.7 &&
                              n.precision_point.x < 5.1;
                }
            } else {
                require(j.set_look_pitch(0.0) && j.advance_frame(0.3).accepted, "precision level look");
                sighted = j.snapshot().precision_sighted;
            }
            require(j.request_jump(), "precision jump");
            require(advance_until(j, [](const Snapshot &n) { return !n.player_grounded; }, 0.5), "precision takeoff");
            require(advance_until(j, [](const Snapshot &n) { return n.player_grounded; }, 3.0), "precision landing");
            const Snapshot l = j.snapshot();
            landed_x = l.player_position.x;
            on_rail = l.support_entity_id == Simulation::kVaultRailEntityId;
            precise = l.precision_landing_count;
        };
        bool sighted = false, plain_sighted = false, on_rail = false, plain_on_rail = false;
        double landed = 0.0, plain_landed = 0.0;
        std::uint64_t precise = 0, plain_precise = 0;
        attempt(true, sighted, landed, on_rail, precise);
        attempt(false, plain_sighted, plain_landed, plain_on_rail, plain_precise);
        require(sighted, "looking at the rail's top from 3 m, the precision landing must be sighted");
        require(!plain_sighted, "looking level, nothing is sighted");
        require(on_rail, "the precision jump must land the feet on the sighted rail top");
        require(precise == 1, "a landing on the sighted point must count as the prepared landing it was");
        require(!plain_on_rail && plain_precise == 0, "the plain standing jump must not reach the rail's top");
        std::cout << "PASS scraperx_sim parkour precision: landed_x=" << landed << " plain_x=" << plain_landed << "\n";
    }
}
