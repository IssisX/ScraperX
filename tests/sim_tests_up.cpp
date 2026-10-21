// The gesture controls' one "up" (Simulation::request_up): every move a
// runner makes upward, made by that one request alone, the native choosing
// it from what is ahead (UpMove). Each test reads what up_move offers before
// the request, makes the request, checks it was taken as that move, and then
// checks the move's own effect (a vault, a springboard's height, a kick).
// And where up must offer nothing: at a machine's controls, crouched under a
// beam; and never a kick off a wall run that has ended on the ground.

#include "sim_tests.hpp"
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"

namespace {

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
using scraperx::sim::UpMove;

const char *up_name(const UpMove move) {
    switch (move) {
    case UpMove::None: return "none";
    case UpMove::Jump: return "jump";
    case UpMove::Precision: return "precision";
    case UpMove::Springboard: return "springboard";
    case UpMove::Vault: return "vault";
    case UpMove::Mantle: return "mantle";
    case UpMove::Climb: return "climb";
    case UpMove::Stand: return "stand";
    case UpMove::ClimbUp: return "climb up";
    case UpMove::TopOut: return "top out";
    case UpMove::JumpOff: return "jump off";
    case UpMove::Launch: return "launch";
    case UpMove::Kick: return "kick";
    case UpMove::Catch: return "catch";
    case UpMove::LetGo: return "let go";
    case UpMove::Throw: return "throw";
    }
    return "?";
}

// Up, where up_move offers `expected`: the request taken, and resolved as it.
void up_as(Simulation &s, const UpMove expected, const char *what) {
    const UpMove offered = s.snapshot().up_move;
    if (offered != expected) {
        std::cerr << "  up: " << what << ": offered " << up_name(offered) << ", not " << up_name(expected) << '\n';
    }
    require(offered == expected, what);
    const std::uint64_t before = s.snapshot().up_count;
    require(s.request_up(), what);
    require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, what);
    require(s.snapshot().up_count == before + 1 && s.snapshot().up_resolved == expected, what);
}

// The plate stack's slab, 1.1 m up, sprinting west at it (run_springboard's
// run): up a stride short plants a foot on its top; 3 m short, a jump.
void up_springboard() {
    const auto run_at = [](const double up_x, const UpMove expected) {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.5).accepted, "up springboard: settle");
        require(walk_to(s, 30.0, 6.0, 30.0) && walk_to(s, 58.0, 6.0, 30.0) && walk_to(s, 58.0, 12.0, 20.0, 0.1),
                "up springboard: round to the plate stack's east side");
        double soles_peak = 0.0;
        bool upped = false;
        for (int tick = 0; tick < 3 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
            (void)s.set_facing(-1.0, 0.0);
            (void)s.set_move_input(-1.0, 0.0);
            (void)s.set_sprint_input(true);
            if (!upped && s.snapshot().player_position.x < up_x) {
                up_as(s, expected, "up springboard: up");
                upped = true;
                continue;
            }
            require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "up springboard: tick");
            soles_peak = std::max(soles_peak, s.snapshot().player_position.y - 0.9);
            if (upped && tick > 45 && s.snapshot().player_grounded) {
                break;
            }
        }
        return std::make_pair(s.snapshot().springboard_count, soles_peak);
    };
    const auto [close_boards, close_peak] = run_at(49.6, UpMove::Springboard);
    const auto [far_boards, far_peak] = run_at(51.5, UpMove::Jump);
    std::cout << "  up: a stride short of the slab, " << close_boards << " springboard, soles to " << close_peak
              << " m; 3 m short, a jump, soles to " << far_peak << " m\n";
    require(close_boards == 1 && close_peak > 1.1 + 1.4, "up springboard: a stride short, from the top");
    require(far_boards == 0 && far_peak > 1.0, "up springboard: 3 m short, a jump from the ground");
}

// The proving ground's 0.44 m rail: walked at, up vaults it; sprinted at,
// up speed-vaults it at the run's pace (no foothold on a rail that thin).
void up_vault() {
    for (const bool sprint : {false, true}) {
        Simulation s(InitialSpawn::VaultApproach);
        require(s.advance_frame(0.3).accepted, "up vault: settle");
        require(s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0) && s.set_sprint_input(sprint), "up vault: run");
        require(s.snapshot().up_move == UpMove::Jump, "up vault: nothing in reach at the start, a jump");
        require(advance_until(s, [](const Snapshot &n) { return n.up_move == UpMove::Vault && n.player_position.x > 3.0; },
                              3.0),
                "up vault: the rail offered");
        const double entry = horizontal_speed(s.snapshot());
        up_as(s, UpMove::Vault, "up vault: up");
        require(s.snapshot().traversal_state == TraversalState::Vaulting, "up vault: vaulting");
        double slowest = 99.0;
        require(advance_until(s, [&](const Snapshot &n) {
                    if (n.traversal_state == TraversalState::Vaulting) {
                        slowest = std::min(slowest, n.player_linear_velocity.x);
                    }
                    return n.traversal_state == TraversalState::None;
                }, 2.0),
                "up vault: over");
        require(s.snapshot().player_position.x > 5.6, "up vault: landed past the rail");
        std::cout << "  up: " << (sprint ? "sprinted" : "walked") << " at the rail at " << entry
                  << " m/s, vaulted it, at least " << slowest << " m/s over\n";
        if (sprint) {
            require(entry > 7.5 && slowest > 6.0, "up vault: sprinted, a speed vault at the run's pace");
        }
    }
}

// The proving ground's mantle ledge, 1.5 m up, faced at arm's length: up
// mantles; and a ledge too high to mantle, hung from (run_traversal's), up
// climbs up over it.
void up_mantle_and_climb_up() {
    Simulation mantle(InitialSpawn::MantleApproach);
    require(mantle.set_facing(1.0, 0.0) && mantle.advance_frame(1.0).accepted, "up mantle: settle");
    up_as(mantle, UpMove::Mantle, "up mantle: up");
    require(mantle.snapshot().traversal_state == TraversalState::Mantling, "up mantle: mantling");
    require(advance_until(mantle, [](const Snapshot &n) {
                return n.player_grounded && n.support_entity_id == Simulation::kMantleLedgeEntityId;
            }, 2.0),
            "up mantle: up on the ledge");

    Simulation hang(InitialSpawn::HangApproach);
    require(hang.set_facing(1.0, 0.0) && hang.set_move_input(1.0, 0.0), "up climb up: approach");
    require(advance_until(hang, [](const Snapshot &n) { return n.traversal_state == TraversalState::Hanging; }, 2.0),
            "up climb up: hanging");
    require(hang.set_move_input(0.0, 0.0) && hang.advance_frame(0.5).accepted, "up climb up: hold");
    up_as(hang, UpMove::ClimbUp, "up climb up: up");
    require(hang.snapshot().traversal_state == TraversalState::Mantling, "up climb up: pulling up");
    require(advance_until(hang, [](const Snapshot &n) { return n.player_grounded; }, 2.0) &&
                hang.snapshot().player_position.y > 4.4,
            "up climb up: on the ledge's top");
    std::cout << "  up: the 1.5 m ledge mantled; the high ledge hung from and climbed up\n";
}

// The 154 m deck's ladder (run_climb_moves'): faced at its foot, up takes
// it; part way up, up jumps off it; climbed to its top slowly (no top-out by
// the stick), up tops out on to the 176 ring.
void up_ladder() {
    const auto to_foot = [](Simulation &s) {
        require(s.advance_frame(1.0).accepted, "up ladder: settle");
        require(walk_to(s, 3.5, -128.2, 16.0) && walk_to(s, 3.5, -129.8, 4.0) && walk_to(s, 5.0, -129.6, 4.0, 0.08),
                "up ladder: to the ladder's foot");
        require(s.set_facing(0.0, 1.0) && s.advance_frame(0.4).accepted, "up ladder: faced");
        up_as(s, UpMove::Climb, "up ladder: up at its foot");
        require(s.advance_frame(0.2).accepted && is_climbing(s.snapshot()), "up ladder: on it");
    };
    Simulation off(InitialSpawn::Deck154);
    to_foot(off);
    for (int tick = 0; tick < 3 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        require(off.set_move_input(0.0, 1.0) && off.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "up ladder: climbing");
    }
    require(off.set_move_input(0.0, 0.0) && off.advance_frame(0.2).accepted, "up ladder: holding");
    up_as(off, UpMove::JumpOff, "up ladder: up part way");
    const Snapshot sprung = off.snapshot();
    require(!is_climbing(sprung) && sprung.player_linear_velocity.y > 3.0, "up ladder: jumped off");

    Simulation top(InitialSpawn::Deck154);
    to_foot(top);
    bool offered = false;
    for (int tick = 0; tick < 90 * static_cast<int>(Simulation::kTickRateHz) && !offered; ++tick) {
        // Under half the stick: the climb's own top-out is not reached for.
        require(top.set_move_input(0.0, 0.45) && top.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "up ladder: climbing slowly");
        require(is_climbing(top.snapshot()), "up ladder: still on the holds");
        offered = top.snapshot().up_move == UpMove::TopOut;
    }
    require(offered, "up ladder: at the top, a top-out offered");
    require(top.set_move_input(0.0, 0.0), "up ladder: stick let go");
    const double at_y = top.snapshot().player_position.y;
    up_as(top, UpMove::TopOut, "up ladder: up at the top");
    const std::uint64_t deaths = top.snapshot().death_count;
    require(advance_until(top, [](const Snapshot &n) { return n.player_grounded && n.traversal_state == TraversalState::None; },
                          3.0) &&
                top.snapshot().player_position.y > at_y && top.snapshot().death_count == deaths,
            "up ladder: over on to the top");
    std::cout << "  up: the ladder taken, jumped off at " << sprung.player_linear_velocity.y
              << " m/s up; topped out from " << at_y << " m to " << top.snapshot().player_position.y << " m\n";
}

// The monkey bars (run_swing_launch's): up from the ground to the first
// bar, then on the last bar's forward swing up throws the body on.
void up_swing() {
    Simulation s(InitialSpawn::SwingApproach);
    require(s.set_facing(1.0, 0.0) && s.advance_frame(0.4).accepted && s.set_move_input(1.0, 0.0), "up swing: settle");
    up_as(s, UpMove::Jump, "up swing: up at the bars");
    require(advance_until(s, [](const Snapshot &n) { return n.traversal_state == TraversalState::Swinging; }, 1.0),
            "up swing: the first bar caught");
    require(s.snapshot().up_move == UpMove::Launch, "up swing: on a bar, up throws on");
    require(advance_until(s, [](const Snapshot &n) { return n.swing_reach_count >= 5; }, 5.0) &&
                advance_until(s, [](const Snapshot &n) {
                    return n.player_linear_velocity.x > 1.0 && n.player_linear_velocity.y > 0.8 && n.player_position.x > 5.0;
                }, 4.0),
            "up swing: the last bar's forward swing");
    up_as(s, UpMove::Launch, "up swing: up on the last bar");
    require(s.snapshot().swing_launch_count == 1 && s.snapshot().traversal_state == TraversalState::None,
            "up swing: thrown on");
    std::cout << "  up: the bars reached by up, the last one launched off\n";
}

// The wall chain (run_wall_chain's passage): up beside the first wall
// jumps; on each wall, up kicks to the other.
void up_wall_kicks() {
    Simulation s(InitialSpawn::WallRunApproach);
    require(s.advance_frame(0.3).accepted && walk_to(s, -19.0, -28.5, 6.0, 0.1), "up kicks: to the passage");
    require(s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0) && s.set_sprint_input(true), "up kicks: sprint");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.x > -12.5; }, 3.0), "up kicks: approach");
    up_as(s, UpMove::Jump, "up kicks: up beside the wall");
    double toward = 0.6;
    require(s.set_move_input(1.0, toward), "up kicks: stick into the wall");
    std::uint64_t kicks = 0;
    std::uint64_t steps_at_last_kick = 0;
    for (int tick = 0; tick < 5 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "up kicks: tick");
        const Snapshot n = s.snapshot();
        if (n.player_grounded) {
            break;
        }
        if (n.player_wall_running && n.wall_kick_count == kicks && n.wall_step_count > steps_at_last_kick) {
            toward = -toward;
            require(s.set_move_input(1.0, toward), "up kicks: stick turned");
            up_as(s, UpMove::Kick, "up kicks: up on the wall");
            steps_at_last_kick = n.wall_step_count;
            ++kicks;
            require(s.snapshot().wall_kick_count == kicks, "up kicks: kicked");
        }
    }
    std::cout << "  up: " << kicks << " wall kicks by up, " << s.snapshot().wall_step_count << " wall steps\n";
    require(kicks >= 3 && s.snapshot().death_count == 0, "up kicks: wall to wall and back");
}

// One run along the passage's wall, no kick, ending on the ground inside the
// run's 0.3 s hold: landed, the run is over. Out of the passage, a standing
// jump offers no kick in the air, and up makes none (it was a second jump).
void up_wall_run_ends() {
    Simulation s(InitialSpawn::WallRunApproach);
    require(s.advance_frame(0.3).accepted && walk_to(s, -19.0, -28.5, 6.0, 0.1), "up run ends: to the passage");
    require(s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0) && s.set_sprint_input(true), "up run ends: sprint");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.x > -12.5; }, 3.0), "up run ends: approach");
    up_as(s, UpMove::Jump, "up run ends: up beside the wall");
    require(s.set_move_input(1.0, 0.6) && advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 3.0) &&
                s.snapshot().wall_step_count >= 1,
            "up run ends: a run on the wall, then the ground");
    require(s.set_move_input(0.0, 0.0) && s.set_sprint_input(false) && s.advance_frame(0.5).accepted &&
                !s.snapshot().player_wall_running,
            "up run ends: landed, no longer running a wall");
    require(walk_to(s, 10.0, -28.5, 10.0, 0.1) && s.advance_frame(1.0).accepted, "up run ends: out of the passage");
    up_as(s, UpMove::Jump, "up run ends: a standing jump in the open");
    require(advance_until(s, [](const Snapshot &n) { return !n.player_grounded; }, 0.5), "up run ends: off the ground");
    const std::uint64_t kicks = s.snapshot().wall_kick_count;
    require(s.snapshot().up_move == UpMove::Jump, "up run ends: in the open air, no kick offered");
    require(s.request_up() && s.advance_frame(Simulation::kFixedStepSeconds).accepted && s.snapshot().wall_kick_count == kicks,
            "up run ends: in the open air, up kicks off nothing");
    std::cout << "  up: a wall run ended on the ground; in the open, no kick offered or made\n";
}

// The wall kicks again from a fresh world: the same input, the same world to
// the bit (up's choice is the world's own, never the frame's).
std::vector<std::uint64_t> wall_kick_run() {
    Simulation s(InitialSpawn::WallRunApproach);
    require(s.advance_frame(0.3).accepted && walk_to(s, -19.0, -28.5, 6.0, 0.1), "up again: to the passage");
    require(s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0) && s.set_sprint_input(true), "up again: sprint");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.x > -12.5; }, 3.0), "up again: approach");
    require(s.request_up(), "up again: up beside the wall");
    double toward = 0.6;
    for (int tick = 0; tick < 4 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)s.set_move_input(1.0, toward);
        if (s.snapshot().up_move == UpMove::Kick) {
            toward = -toward;
            (void)s.set_move_input(1.0, toward);
            (void)s.request_up();
        }
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "up again: tick");
    }
    return world_bits(s);
}

// Crouched, up stands the body and does not jump it; under the proving
// ground's beam, where it cannot stand, it stays down and does not jump.
// Sliding, up jumps out of the slide at its speed (run_slide_jump's).
void up_from_crouch() {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(0.5).accepted && s.set_crouch_input(true) && s.advance_frame(0.3).accepted, "up stand: crouched");
    require(s.snapshot().player_crouched, "up stand: crouched");
    require(s.set_crouch_input(false), "up stand: the crouch let go with the up, as the gestures do");
    up_as(s, UpMove::Stand, "up stand: up");
    bool left_ground = false;
    for (int tick = 0; tick < 45; ++tick) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "up stand: tick");
        left_ground = left_ground || !s.snapshot().player_grounded;
    }
    require(!s.snapshot().player_crouched && !left_ground, "up stand: stood, not jumped");

    // Under the proving ground's crawl beam (1.45 m clear) there is no room to
    // stand: up offers nothing and is refused, and the body stays down.
    // Walked out from under it, Stand is offered again.
    Simulation beam(InitialSpawn::StaticDeck);
    require(beam.advance_frame(0.5).accepted && beam.set_crouch_input(true) && beam.advance_frame(0.3).accepted &&
                walk_to(beam, -1.55, -13.3, 8.0, 0.1),
            "up stand: crouched under the crawl beam");
    require(beam.snapshot().player_crouched && beam.snapshot().up_move == UpMove::None && !beam.request_up(),
            "up stand: under the beam, nothing offered and up refused");
    require(walk_to(beam, -2.1, -15.4, 8.0, 0.1) && beam.snapshot().up_move == UpMove::Stand,
            "up stand: out from under the beam, Stand offered");

    // Crouch-walked off the trapeze's launch stand: in the coyote ticks up is
    // the jump the legs still have, standing the body to make it.
    Simulation edge(InitialSpawn::ExteriorGrade);
    require(edge.advance_frame(0.5).accepted && walk_to(edge, 14.0, -28.0, 12.0) && walk_to(edge, 29.6, -28.0, 12.0, 0.1),
            "up coyote: on the launch stand");
    require(edge.set_facing(1.0, 0.0) && edge.set_crouch_input(true) && edge.advance_frame(0.3).accepted &&
                edge.set_move_input(1.0, 0.0),
            "up coyote: crouched, walking at the edge");
    require(advance_until(edge, [](const Snapshot &n) { return !n.player_grounded; }, 4.0), "up coyote: off the edge");
    require(edge.advance_frame(2.0 * Simulation::kFixedStepSeconds).accepted && edge.snapshot().player_crouched,
            "up coyote: two ticks over the edge, still crouched");
    require(edge.set_crouch_input(false), "up coyote: the crouch let go with the up");
    up_as(edge, UpMove::Jump, "up coyote: up in the coyote ticks");
    require(advance_until(edge, [](const Snapshot &n) { return n.player_linear_velocity.y > 4.0; }, 0.1),
            "up coyote: jumped off the edge");

    Simulation slide(InitialSpawn::ExteriorGrade);
    require(slide.advance_frame(0.5).accepted && slide.set_facing(1.0, 0.0) && slide.set_move_input(1.0, 0.0) &&
                slide.set_sprint_input(true) && slide.advance_frame(1.5).accepted && slide.set_crouch_input(true) &&
                slide.advance_frame(0.3).accepted,
            "up slide: sliding");
    require(slide.snapshot().player_sliding, "up slide: sliding");
    const double slide_mps = horizontal_speed(slide.snapshot());
    require(slide.set_crouch_input(false), "up slide: the crouch let go with the up");
    up_as(slide, UpMove::Jump, "up slide: up");
    double peak = 0.0;
    double least = 99.0;
    for (int tick = 0; tick < 2 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        require(slide.advance_frame(Simulation::kFixedStepSeconds).accepted, "up slide: tick");
        const Snapshot n = slide.snapshot();
        peak = std::max(peak, n.player_position.y - 0.9);
        if (n.player_grounded && tick > 9) {
            break;
        }
        if (!n.player_grounded) {
            least = std::min(least, horizontal_speed(n));
        }
    }
    std::cout << "  up: crouched, stood without a jump; out of a " << slide_mps << " m/s slide, jumped to "
              << peak << " m at no less than " << least << " m/s\n";
    require(peak > 1.3 && least > slide_mps - 0.2, "up slide: jumped out at the slide's speed");
}

// At the yard forklift's wheel (run_forklift's boarding), the stick and the
// buttons are the truck's: up offers nothing and is refused.
void up_at_the_wheel() {
    using scraperx::sim::Forklift;
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(0.5).accepted && walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
                walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1),
            "up at the wheel: aboard");
    require(s.set_facing(0.0, 1.0) && s.advance_frame(0.5).accepted && s.forklift_state().at_controls &&
                s.request_forklift_action() && s.advance_frame(0.2).accepted && s.forklift_state().driving,
            "up at the wheel: driving");
    require(s.snapshot().up_move == UpMove::None && !s.request_up(), "up at the wheel: nothing offered, up refused");
    std::cout << "  up: at the forklift's wheel, nothing offered\n";
}

// The trapeze (run_trapeze's run off the launch stand): up in the air as
// the bar comes into reach takes it; on it, up lets it go.
void up_trapeze() {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(0.5).accepted, "up trapeze: settle");
    require(walk_to(s, 14.0, -28.0, 12.0) && walk_to(s, 27.6, -28.0, 12.0, 0.1), "up trapeze: on the launch stand");
    require(s.advance_frame(0.3).accepted && s.set_facing(1.0, 0.0) && s.set_move_input(1.0, 0.0), "up trapeze: run");
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.x > 30.5; }, 3.0), "up trapeze: the edge");
    up_as(s, UpMove::Jump, "up trapeze: up at the stand's edge");
    require(s.set_move_input(0.0, 0.0), "up trapeze: stick let go");
    require(advance_until(s, [](const Snapshot &n) { return n.up_move == UpMove::Catch; }, 1.0),
            "up trapeze: the bar comes into reach");
    up_as(s, UpMove::Catch, "up trapeze: up for the bar");
    require(advance_until(s, [&](const Snapshot &) { return s.trapeze_state().harnessed; }, 0.5),
            "up trapeze: the bar taken");
    require(s.advance_frame(1.0).accepted && s.snapshot().up_move == UpMove::LetGo, "up trapeze: on it, up lets go");
    up_as(s, UpMove::LetGo, "up trapeze: up on the bar");
    require(!s.trapeze_state().harnessed, "up trapeze: let go");
    std::cout << "  up: the trapeze's bar taken by up in the air, and let go by up\n";
}

}   // namespace

// The faint route hint (route_ahead): 3 m short of the 154 m deck's ladder
// and facing it, out of the hands' reach (no hold offered), it picks out a
// hold on the ladder ahead; turned away from it, not; in the air, nothing.
void up_route_hint() {
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "route hint: settle");
    require(walk_to(s, 3.5, -128.2, 16.0) && walk_to(s, 3.5, -132.4, 6.0) && walk_to(s, 5.0, -132.4, 4.0, 0.08),
            "route hint: 3 m short of the ladder");
    require(s.set_facing(0.0, 1.0) && s.advance_frame(0.4).accepted, "route hint: facing the ladder");
    const auto ahead = s.route_ahead();
    bool on_ladder = false;
    for (const auto &point : ahead) {
        on_ladder = on_ladder || (std::abs(point.x - 5.0) < 0.35 && std::abs(point.z - -129.05) < 0.2 &&
                                  point.y > 154.5 && point.y < 157.5);
    }
    std::cout << "  route hint: facing the ladder from 3 m, " << ahead.size() << " picked out, one on its rungs "
              << on_ladder << ", a hold in reach " << s.snapshot().grip_available << "\n";
    require(!s.snapshot().grip_available && on_ladder, "route hint: the ladder's hold ahead, out of reach, picked out");
    require(s.set_facing(-1.0, 0.0) && s.advance_frame(0.4).accepted, "route hint: turned west");
    for (const auto &point : s.route_ahead()) {
        require(!(std::abs(point.x - 5.0) < 0.35 && std::abs(point.z - -129.05) < 0.2),
                "route hint: turned away, the ladder is not picked out");
    }
    require(s.set_facing(0.0, 1.0) && s.request_jump() && s.advance_frame(0.15).accepted && !s.snapshot().player_grounded,
            "route hint: a jump");
    require(s.route_ahead().empty(), "route hint: in the air, nothing picked out");
    std::cout << "  route hint: picked out ahead, not behind, not in the air\n";
}

// The throw (the owner, 2026-10-19: throw what you carry): a piece of loose
// stock in the hands, up is THROW, and the arms send it along the view at
// what 300 J gives its mass (a toolbox 5.8 m/s, a drum 3.3); it is not heard
// as a hit at the hands, it is heard landing, and the path drawn ahead of
// the throw comes down where the piece does.
void up_throw() {
    using scraperx::sim::LooseStock;
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(0.5).accepted, "up throw: settle");
    const std::uint32_t first = s.loose_stock_count() - static_cast<std::uint32_t>(LooseStock::kMatronThrowables);
    const auto throw_one = [&](const std::uint32_t piece, const double pick_x, const double pick_z, const double lo,
                               const double hi, const char *what, const double along_x = 1.0) {
        const std::uint64_t entity = LooseStock::kFirstEntity + piece;
        const std::uint32_t body = s.kit_body_index(entity);
        require(walk_to(s, pick_x, pick_z, 30.0, 0.08), what);
        (void)s.set_facing(0.0, 1.0);
        (void)s.advance_frame(0.3);
        require(s.snapshot().carry_target_entity_id == entity && s.request_pick_up(), what);
        (void)s.advance_frame(0.6);
        require(s.snapshot().carrying_entity_id == entity, what);
        // Turned along the band (east, or west where the bay's rail stands
        // east of it, bay.hpp), the throw's way clear.
        (void)s.set_facing(along_x, 0.0);
        (void)s.advance_frame(0.8);
        const auto path = s.throw_prediction();
        require(path.size() >= 3, "up throw: the throw's path is drawn ahead while the piece is held");
        const std::uint64_t throws = s.snapshot().throw_count;
        std::uint64_t seen = 0;
        for (const auto &hit : s.impacts()) {
            seen = std::max(seen, hit.sequence);
        }
        up_as(s, UpMove::Throw, what);
        const auto after = s.snapshot();
        const auto v = s.kit_body_velocity(body);
        const double relative = std::hypot(v.x - after.player_linear_velocity.x, v.y - after.player_linear_velocity.y,
                                           v.z - after.player_linear_velocity.z);
        std::cout << "  up throw: " << what << " left the hands at " << relative << " m/s relative to the thrower\n";
        require(after.carrying_entity_id == 0 && after.throw_count == throws + 1, what);
        require(relative > lo && relative < hi, what);
        for (const auto &hit : s.impacts()) {
            require(!(hit.entity == entity && hit.sequence > seen), "up throw: not heard as a hit at the hands");
        }
        // Down where the drawn path came down, and heard landing.
        bool landed = false;
        scraperx::sim::Vector3 at{};
        for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz) && !landed; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            for (const auto &hit : s.impacts()) {
                if (hit.entity == entity && hit.sequence > seen) {
                    landed = true;
                    at = hit.at;
                }
            }
        }
        const auto end = path.back();
        const double off = std::hypot(at.x - end.x, at.z - end.z);
        std::cout << "  up throw: " << what << " landed at (" << at.x << ", " << at.y << ", " << at.z
                  << "), the drawn path ended at (" << end.x << ", " << end.y << ", " << end.z << "), " << off
                  << " m apart\n";
        require(landed, "up throw: the thrown piece is heard landing");
        require(off < 1.0, "up throw: it comes down where its drawn path did");
    };
    // Her deck's toolbox, from the pick spot south of it; then the drum.
    throw_one(first, -6.5, -125.75, 5.4, 6.2, "a toolbox thrown", -1.0);
    (void)s.advance_frame(1.0);
    throw_one(first + 2, -15.0, -125.95, 2.9, 3.7, "a drum thrown");
    std::cout << "PASS scraperx_sim up throw: toolbox and drum thrown, unheard at the hands, landing on their drawn "
                 "paths\n";
}

void run_up_controls() {
    up_springboard();
    up_vault();
    up_mantle_and_climb_up();
    up_ladder();
    up_swing();
    up_wall_kicks();
    up_wall_run_ends();
    up_from_crouch();
    up_at_the_wheel();
    up_trapeze();
    up_route_hint();
    up_throw();
    const auto first = wall_kick_run();
    const auto second = wall_kick_run();
    require(first == second, "up again: the same input, the same world");
    std::cout << "PASS scraperx_sim up controls: springboard, vault, speed vault, mantle, climb up, ladder, "
                 "jump off, top out, bars, launch, wall kicks, stand, slide jump, trapeze; none under a beam or "
                 "at the wheel; the coyote jump; no kick off an ended wall run; the route hint; deterministic\n";
}

void run_up_throw() {
    up_throw();
}
