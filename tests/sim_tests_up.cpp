// The gesture controls' one "up" (Simulation::request_up): every move a
// runner makes upward, made by that one request alone, the native choosing
// it from what is ahead (UpMove). Each test reads what up_move offers before
// the request, makes the request, and checks the move made is that move and
// is the move the old button made.

#include "sim_tests.hpp"

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
    require(far_boards == 0, "up springboard: 3 m short, a jump from the ground");
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
    require(advance_until(top, [](const Snapshot &n) { return n.player_grounded && n.traversal_state == TraversalState::None; },
                          3.0),
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

    // Held crouched (the button scheme's toggle still on): up still only stands
    // for its tick, and never jumps.
    Simulation held(InitialSpawn::ExteriorGrade);
    require(held.advance_frame(0.5).accepted && held.set_crouch_input(true) && held.advance_frame(0.3).accepted,
            "up stand: crouched, held");
    up_as(held, UpMove::Stand, "up stand: up, crouch still held");
    for (int tick = 0; tick < 45; ++tick) {
        require(held.advance_frame(Simulation::kFixedStepSeconds).accepted && held.snapshot().player_grounded,
                "up stand: held, never off the ground");
    }

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

void run_up_controls() {
    up_springboard();
    up_vault();
    up_mantle_and_climb_up();
    up_ladder();
    up_swing();
    up_wall_kicks();
    up_from_crouch();
    up_trapeze();
    std::cout << "PASS scraperx_sim up controls: springboard, vault, speed vault, mantle, climb up, ladder, "
                 "jump off, top out, bars, launch, wall kicks, stand, slide jump, trapeze\n";
}
