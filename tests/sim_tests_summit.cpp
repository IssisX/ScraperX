// The summit (task 92): the deck west of the derrick's upper receiver, the
// searchlight a striker plate lights, and the runway arm. Every leg is on
// player inputs from the summit spawn.
#include "sim_tests.hpp"

#include "sim/summit.hpp"

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Summit;
using scraperx::sim::TraversalState;

// Up the two blocks and onto the gantry, as a climber would: a mantle onto
// block one from the south, one onto block two from block one, one onto the
// gantry from block two.
bool climb_gantry(Simulation &s) {
    constexpr double kBlockOneSouthStand = Summit::kStrikerSouthZ + 0.35;
    const auto failed = [&s](const char *leg) {
        const auto now = s.snapshot();
        std::cerr << "  summit climb failed at: " << leg << " pos=(" << now.player_position.x << ","
                  << now.player_position.y << "," << now.player_position.z << ") grounded=" << now.player_grounded
                  << " ledge=" << now.ledge_available << " trav=" << static_cast<int>(now.traversal_state) << '\n';
        return false;
    };
    if (!(walk_to(s, 8.0, -184.6, 6.0, 0.2) && walk_to(s, -3.2, -184.6, 14.0, 0.2) &&
          walk_to(s, -3.2, kBlockOneSouthStand, 4.0, 0.08))) {
        return failed("to block one");
    }
    if (!c4_mantle(s, 0.0, -1.0, Summit::kBlockOneTop + 0.45, "the summit: onto block one")) return failed("block one");
    if (!walk_to(s, -2.95, -187.5, 3.0, 0.06)) return failed("across block one");
    if (!c4_hang_up(s, 1.0, 0.0, Summit::kBlockTwoTop + 0.4, "the summit: up onto block two")) return failed("block two");
    if (!walk_to(s, -1.25, -187.5, 3.0, 0.06)) return failed("across block two");
    if (!c4_hang_up(s, 1.0, 0.0, Summit::kGantryTop + 0.4, "the summit: up onto the gantry")) return failed("the gantry");
    return true;
}

// Walks east off the gantry's edge onto the plate at `speed` (the stick).
bool step_off_gantry(Simulation &s, const double z, const double speed) {
    if (!walk_to(s, 1.9, z, 5.0, 0.08)) return false;
    (void)s.set_facing(1.0, 0.0);
    return hold_stick(s, speed, 0.0, 1.0, 0.0, 6.0, [](const Snapshot &state) {
        return state.player_grounded && state.player_position.y < Summit::kGantryTop - 2.0;
    });
}

// From the spawn to the arm's root along the deck's south side (the plate
// stands a metre proud in the middle of the deck, so the way is round it).
bool walk_to_arm(Simulation &s) {
    return walk_to(s, 8.0, -185.0, 6.0, 0.2) && walk_to(s, -10.5, -185.0, 14.0, 0.2) &&
           walk_to(s, -11.0, Summit::kArmCentreZ, 4.0, 0.15);
}

// A metre to mantle, from the south, onto the plate's middle.
bool climb_onto_plate(Simulation &s) {
    return walk_to(s, 9.0, Summit::kStrikerSouthZ + 0.35, 6.0, 0.2) &&
           walk_to(s, 5.1, Summit::kStrikerSouthZ + 0.35, 6.0, 0.08) &&
           c4_mantle(s, 0.0, -1.0, Summit::kStrikerRestTop + 0.45, "the summit: onto the striker plate");
}

void settle_on_plate(Simulation &s) {
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(2.0);
}
} // namespace

void run_summit() {
    // ---- the deck and the arm: walked end to end ------------------------------
    {
        Simulation s(InitialSpawn::Summit);
        require(s.advance_frame(1.0).accepted, "summit: settle");
        const auto start = s.snapshot();
        require(start.player_grounded && start.support_entity_id == Summit::kFrameEntity,
                "summit: the spawn stands on the summit deck's frame");
        require(!s.summit_state().lit && std::abs(s.summit_state().lamp_speed_rad_s) < 1.0e-3,
                "summit: the searchlight is dark and still as found");
        (void)s.advance_frame(10.0);
        require(!s.summit_state().lit && std::abs(s.summit_state().lamp_yaw_rad) < 1.0e-3 &&
                    s.summit_state().striker_peak_m < 1.0e-3,
                "summit: left alone ten seconds, nothing moves and the lamp stays dark");
        require(walk_to_arm(s) && walk_to(s, Summit::kArmEndX + 1.5, -187.5, 14.0, 0.15),
                "summit: a walk out along the arm to its end");
        const auto end = s.snapshot();
        require(end.player_grounded && end.death_count == 0 && end.support_entity_id == Summit::kFrameEntity &&
                    end.player_position.y > Summit::kDeckTop + 0.8,
                "summit: standing at the arm's end, on the frame, alive");
    }
    // ---- the plate is not a button: standing and hopping do not light it ----------
    {
        Simulation s(InitialSpawn::Summit);
        (void)s.advance_frame(1.0);
        require(climb_onto_plate(s), "summit: onto the striker plate's middle");
        require(walk_to(s, 5.0, -187.5, 4.0, 0.1), "summit: to the plate's middle");
        settle_on_plate(s);
        const double rested = s.summit_state().striker_travel_m;
        require(s.snapshot().player_grounded && s.snapshot().support_entity_id == Summit::kStrikerEntity &&
                    rested < 0.03,
                "summit: standing in the plate's middle, the plate holds against its stop");
        require(walk_to(s, 5.0, -186.55, 4.0, 0.1), "summit: out to the plate's free edge");
        settle_on_plate(s);
        require(s.summit_state().striker_travel_m < 0.05,
                "summit: standing on the plate's free edge, it does not give");
        for (int hop = 0; hop < 3; ++hop) {
            (void)s.request_jump();
            (void)s.advance_frame(1.2);
        }
        std::cout << "  summit: three hops on the free edge: peak=" << s.summit_state().striker_peak_m << " m\n";
        require(!s.summit_state().lit, "summit: hopping on the plate's free edge does not light the lamp");
    }
    // ---- the strike from the gantry ---------------------------------------------
    {
        Simulation s(InitialSpawn::Summit);
        (void)s.advance_frame(1.0);
        require(climb_gantry(s), "summit: the climb to the gantry on player inputs");
        require(!s.summit_state().lit, "summit: the lamp is still dark on the gantry");
        require(step_off_gantry(s, -187.5, 0.5), "summit: off the gantry's edge and down onto the plate");
        (void)s.advance_frame(1.5);
        const auto landed = s.snapshot();
        const auto summit = s.summit_state();
        std::cout << "  summit: strike peak=" << summit.striker_peak_m << " m (trip " << Summit::kTripTravel
                  << ") landed y=" << landed.player_position.y << " deaths=" << landed.death_count << '\n';
        require(landed.death_count == 0, "summit: the drop from the gantry is survivable");
        require(summit.lit, "summit: landing from the gantry must light the lamp");
        // The beam turns: its turntable's motor runs, nothing sets its angle.
        const double yaw_then = s.summit_state().lamp_yaw_rad;
        (void)s.advance_frame(5.0);
        const double yaw_now = s.summit_state().lamp_yaw_rad;
        require(std::abs(yaw_now - yaw_then) > 0.8 && std::abs(s.summit_state().lamp_speed_rad_s - Summit::kLampSpeedRadPerS) < 0.05,
                "summit: lit, the lamp's turntable must turn at its motor's speed");
        // Once lit it stays lit, the plate back on its stop.
        (void)s.advance_frame(4.0);
        require(s.summit_state().lit && s.summit_state().striker_travel_m < 0.03,
                "summit: the plate returns on its counterweight and the lamp stays lit");
    }
    // ---- the strike from a lower perch is not enough ------------------------------
    {
        Simulation s(InitialSpawn::Summit);
        (void)s.advance_frame(1.0);
        constexpr double kBlockOneSouthStand = Summit::kStrikerSouthZ + 0.35;
        require(walk_to(s, 8.0, -184.6, 6.0, 0.2) && walk_to(s, -3.2, -184.6, 14.0, 0.2) &&
                    walk_to(s, -3.2, kBlockOneSouthStand, 4.0, 0.08) &&
                    c4_mantle(s, 0.0, -1.0, Summit::kBlockOneTop + 0.45, "the summit: onto block one"),
                "summit falsifier: onto block one");
        require(walk_to(s, -3.2, -187.0, 3.0, 0.08), "summit falsifier: to block one's south edge");
        require(hold_stick(s, 0.0, 0.5, 0.0, 1.0, 6.0, [](const Snapshot &state) {
                    return state.player_grounded && state.player_position.y < Summit::kBlockOneTop - 0.5;
                }),
                "summit falsifier: off block one onto the deck");
        (void)s.advance_frame(1.0);
        require(!s.summit_state().lit, "summit falsifier: a step down from block one must not light the lamp");
    }
}

void run_summit_arm() {
    // ---- run off the arm: the parachute is the way down; without it, no -----------
    for (const bool chute : {true, false}) {
        Simulation s(InitialSpawn::Summit);
        (void)s.advance_frame(1.0);
        require(walk_to_arm(s) && walk_to(s, Summit::kArmEndX + 6.0, -187.5, 14.0, 0.15),
                "summit arm: to the run-up");
        (void)s.set_facing(-1.0, 0.0);
        require(hold_stick(s, -1.0, 0.0, -1.0, 0.0, 6.0, [](const Snapshot &state) {
                    return !state.player_grounded && state.player_position.x < Summit::kArmEndX;
                }),
                "summit arm: a run off the end of the arm");
        bool opened = false;
        const auto ticks = static_cast<std::uint32_t>(260.0 * Simulation::kTickRateHz);
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            const auto now = s.snapshot();
            if (chute && !opened && now.player_linear_velocity.y < -12.0 && !now.parachute_deployed) {
                opened = s.request_parachute();
            }
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            if (s.snapshot().player_grounded || s.snapshot().death_count > 0) break;
        }
        const auto end = s.snapshot();
        std::cout << "  summit arm: chute=" << chute << " ended y=" << end.player_position.y << " x="
                  << end.player_position.x << " z=" << end.player_position.z << " t="
                  << end.simulation_time_seconds << " deaths=" << end.death_count << '\n';
        if (chute) {
            require(opened && end.death_count == 0 && end.player_grounded,
                    "summit arm: off the arm's end with the parachute open, down alive");
            // Out along the arm the wind blows off the tower (-x) and pushes the
            // body once it is off its feet; the deck itself is sheltered.
            require(end.wind_peak_force_n > 20.0, "summit arm: off the arm's end the wind must push the climber");
        } else {
            require(end.death_count > 0, "summit arm falsifier: off the arm's end without the parachute is a fatal fall");
        }
    }
}


// The crew's last beam: no test repositioning, gravity retained, and every
// operation uses the same native commands as the touch controls.
void run_summit_ending() {
    Simulation s(InitialSpawn::Summit);
    (void)s.advance_frame(1.0);
    const auto beam = s.kit_body_index(Summit::kLastBeamEntity);
    require(beam < s.kit_body_count(), "ending: the last beam exists in the player's world");
    const auto beam_y = [&]() { return s.kit_body_position(static_cast<std::uint32_t>(beam)).y; };
    const double hanging_y = beam_y();
    const auto hanging_save = s.save_game();
    require(!s.request_summit_unrig(), "ending: a hanging load cannot be unrigged");
    (void)s.set_summit_lower(true);
    (void)s.advance_frame(12.0);
    (void)s.set_summit_lower(false);
    require(std::abs(beam_y() - hanging_y) < 0.02 && s.summit_state().chain_paid_m == 0.0 &&
                !s.summit_state().beam_seated && s.summit_state().ending == 0,
            "ending falsifier: time, summit arrival and remote commands cannot seat the beam");
    require(walk_to(s, 8.0, -185.0, 5.0, 0.15) &&
                walk_to(s, Summit::kWorkX, Summit::kWorkZ, 12.0, 0.12),
            "ending: walk to the chain block through the existing summit");
    (void)s.set_move_input(0, 0);
    require(s.summit_state().work_in_reach, "ending: the hands can reach the chain block");
    (void)s.set_summit_lower(true);
    (void)s.advance_frame(2.0);
    (void)s.set_summit_lower(false);
    (void)s.advance_frame(1.0);
    const double halfway_y = beam_y();
    const double paid = s.summit_state().chain_paid_m;
    require(paid > 0.35 && paid < 0.45 && hanging_y - halfway_y > 0.3 &&
                !s.summit_state().beam_seated && !s.request_summit_unrig(),
            "ending: easing the chain lowers the real load, but a partial lower is no finish");
    (void)s.advance_frame(2.0);
    require(std::abs(beam_y() - halfway_y) < 0.02 && s.summit_state().chain_paid_m == paid,
            "ending: releasing the control brakes the load above its bearings");
    (void)s.set_summit_lower(true);
    (void)s.advance_frame(3.95);
    (void)s.set_summit_lower(false);
    (void)s.advance_frame(1.0);
    require(beam_y() > Summit::kBeamSeatY + 0.005 && beam_y() < Summit::kBeamSeatY + 0.025 &&
                s.summit_state().chain_tension_n > 8000.0 && !s.summit_state().beam_seated &&
                s.summit_state().ending == 0 && !s.request_summit_unrig(),
            "ending falsifier: a motionless load just above both bearings is still hanging, not seated");
    (void)s.set_summit_lower(true);
    (void)s.advance_frame(3.0);
    (void)s.set_summit_lower(false);
    const auto seated = s.summit_state();
    std::cout << "  ending: beam_y=" << beam_y() << " paid=" << seated.chain_paid_m
              << " left=" << seated.left_bearing << " right=" << seated.right_bearing
              << " pull=" << seated.chain_tension_n << " N\n";
    require(seated.beam_seated && seated.left_bearing && seated.right_bearing && seated.ending == 1 &&
                std::abs(beam_y() - Summit::kBeamSeatY) < 0.02 && seated.chain_tension_n < 100.0,
            "ending: both flanges rest on real steel, motion settled and the chain unloaded");
    require(s.request_summit_unrig(), "ending: free the slack sling with the second player action");
    (void)s.advance_frame(3.0);
    require(s.summit_state().sling_free && s.summit_state().beam_seated && s.summit_state().ending == 2 &&
                std::abs(beam_y() - Summit::kBeamSeatY) < 0.02,
            "ending: the beam stays seated after both load-bearing ropes are removed");
    require(walk_to_arm(s) && walk_to(s, Summit::kArmEndX + 6.0, Summit::kArmCentreZ, 14.0, 0.15),
            "ending: walk to the existing parachute runway");
    require(hold_stick(s, -1, 0, -1, 0, 6.0, [](const Snapshot &state) {
                return !state.player_grounded && state.player_position.x < Summit::kArmEndX;
            }), "ending: jump clear of the tower by player input");
    bool canopy_seen = false;
    bool stowed = false;
    bool stow_refused_finish = false;
    for (int tick = 0; tick < 260 * 90; ++tick) {
        const auto now = s.snapshot();
        const bool stow_this_tick = s.summit_state().ending == 3 && !stowed;
        if (stow_this_tick) {
            require(s.request_parachute(), "ending: the real chute control stows the canopy");
            stowed = true;
        } else if (now.player_linear_velocity.y < -12.0 && !now.parachute_deployed) {
            (void)s.request_parachute();
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        canopy_seen = canopy_seen || s.summit_state().ending == 3;
        if (stow_this_tick) {
            require(!s.snapshot().parachute_deployed && s.summit_state().ending == 2, "ending falsifier: a stowed canopy clears the flight proof");
            stow_refused_finish = true;
        }
        if (s.snapshot().player_grounded || s.snapshot().death_count != 0) break;
    }
    const auto home = s.snapshot();
    require(canopy_seen && stow_refused_finish && home.player_grounded && home.player_position.y < 5.0 && home.death_count == 0 &&
                s.summit_state().ending == 4,
            "ending: only the live canopy descent to ground reaches the quiet end card");
    require(s.load_game(hanging_save.data(), hanging_save.size()), "ending restore: load the unfinished summit save");
    (void)s.advance_frame(2.0);
    require(!s.summit_state().sling_free && s.summit_state().ending == 0 &&
                !s.summit_state().beam_seated && std::abs(beam_y() - hanging_y) < 0.02,
            "ending restore: a previous unrig cannot leave the restored hanging load without its chains");
    std::cout << "PASS scraperx_sim summit-ending: physical beam, slack unrig, canopy, home, restore\n";
}
