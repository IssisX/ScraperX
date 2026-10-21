// The bay on the 154 m deck (bay.hpp): its leaf a walkway under its motor,
// the fan's noise over a climber's steps, and her trap: held on her palms,
// the leaf takes her but not her and a climber; the chain off its cleat,
// hooked onto the leaf's eye, carries it, and she lets go. Every leg on
// player inputs from the deck spawn.
#include "sim_tests.hpp"

#include "sim/bay.hpp"
#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
using scraperx::sim::Bay;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;

constexpr int kTrapHolding = 4;

void tick(Simulation &s) {
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
}

// Steps toward (x, z) at a sprint, a walk or crouched until within
// `tolerance`; false when `seconds` run out.
bool go(Simulation &s, const double x, const double z, const double seconds, const bool sprint,
        const double tolerance = 0.4) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        const auto now = s.snapshot().player_position;
        const double dx = x - now.x;
        const double dz = z - now.z;
        const double length = std::hypot(dx, dz);
        if (length < tolerance) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
            return true;
        }
        (void)s.set_move_input(dx / length, dz / length);
        (void)s.set_facing(dx / length, dz / length);
        (void)s.set_sprint_input(sprint);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    return false;
}

void stand(Simulation &s, const double seconds) {
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    for (int i = 0; i < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++i) {
        tick(s);
    }
}

// Wakes her, then, crouched, west over the leaf and down the west band,
// along the south band and up the east band, back along the north band to
// stand east of the bay, while she, having lost the player at its west end,
// keeps the mouth of the way out there. True with the player at (x, z).
bool stage_across(Simulation &s, const double x, const double z) {
    require(s.advance_frame(1.0).accepted, "bay trap: settle");
    if (!go(s, 10.0, -128.5, 4.0, true, 1.0)) {
        return false;
    }
    (void)s.set_crouch_input(true);
    const bool round = go(s, -Matron::kBandX, Matron::kBandNorthZ, 60.0, false) &&
                       go(s, -Matron::kBandX, Matron::kBandSouthZ, 60.0, false) &&
                       go(s, Matron::kBandX, Matron::kBandSouthZ, 60.0, false) &&
                       go(s, Matron::kBandX, Matron::kBandNorthZ, 60.0, false) && go(s, x, z, 60.0, false, 0.2);
    return round;
}
} // namespace

// Staged across the bay, the dash she hears; true once she is down in it,
// holding the leaf on her palms.
bool stage_hold(Simulation &s) {
    require(stage_across(s, 3.0, Matron::kBandNorthZ), "bay trap: round the deck to east of the bay");
    (void)s.set_crouch_input(false);
    require(go(s, 8.0, Matron::kBandNorthZ, 4.0, true, 0.3) && go(s, 3.0, Matron::kBandNorthZ, 4.0, false, 0.3),
            "bay trap: a dash along the band, heard across the bay");
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        tick(s);
        const auto m = s.matron_state();
        if (m.trap == kTrapHolding && m.bracing && s.bay_state().hands) {
            stand(s, 1.0);
            return true;
        }
    }
    return false;
}

void run_bay() {
    // ---- the walkway: level under its motor, a climber over and back; the
    //      fan's noise over the steps beside it ---------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        require(s.advance_frame(1.0).accepted, "bay walkway: settle");
        (void)s.set_crouch_input(true);
        stand(s, 4.0);
        const auto found = s.bay_state();
        require(found.leaf_open && found.leaf_running && found.fan_running && found.fan_fraction > 0.95 &&
                    !found.hooked && found.trips == 0,
                "bay walkway: as found, the leaf level under its motor and the fan running");
        require(found.cover > 0.999, "bay walkway: away from the fan, nothing of a step is covered");
        double low_angle = 0.0;
        double low_y = 1.0e9;
        double cover = 1.0;
        const auto watch = [&] {
            low_angle = std::min(low_angle, s.bay_state().leaf_angle);
            low_y = std::min(low_y, s.snapshot().player_position.y);
            if (std::abs(s.snapshot().player_position.x + 1.0) < 0.3) {
                cover = std::min(cover, s.bay_state().cover);
            }
        };
        for (const double x : {0.4, -4.6, 0.4}) {
            bool there = false;
            for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !there; ++i) {
                steer_toward(s, x, Matron::kBandNorthZ);
                tick(s);
                watch();
                there = std::hypot(s.snapshot().player_position.x - x, s.snapshot().player_position.z - Matron::kBandNorthZ) < 0.2;
            }
            require(there, "bay walkway: over the leaf and back");
        }
        std::cout << "  bay walkway: the leaf at most " << low_angle << " rad down under the crossing, the player no lower than "
                  << low_y << " m; the fan leaves " << cover << " of a step on the leaf\n";
        require(low_angle > -0.01 && low_y > Bay::kDeckTop, "bay walkway: crossed, the leaf stays level on its stop");
        require(cover < 0.3, "bay walkway: on the leaf beside the running fan, three quarters of a step is covered");
        require(s.matron_state().state == 0, "bay walkway: crouched over it, she sleeps on");
        std::cout << "PASS scraperx_sim bay walkway: low_angle=" << low_angle << " cover=" << cover << "\n";
    }
    // ---- her trap: she holds the leaf; stepped onto, it goes down -------------
    {
        Simulation s(InitialSpawn::Deck154);
        require(stage_hold(s), "bay trap: across the bay from her, the player heard: she holds the leaf from below");
        const auto held = s.bay_state();
        const auto her = s.matron_state();
        std::cout << "  bay trap: her hands at (" << held.hand.x << ", " << held.hand.y << ", " << held.hand.z << "), "
                  << held.support_n << " N up; the leaf at " << held.leaf_angle << " rad\n";
        require(her.bracing && her.traps == 1 && held.hands && !held.leaf_running && held.trips == 1 &&
                    held.support_n > 0.0 && std::abs(held.leaf_angle) < 0.02,
                "bay trap: her hands in it stop its motor, and the leaf rests on her palms, level");
        // Its fan, on the same district, coasts down: nothing covers a step now.
        const double cover_running = held.cover;
        bool fan_still = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !fan_still; ++i) {
            tick(s);
            fan_still = s.bay_state().fan_fraction < 1.0e-3;
        }
        const double cover_still = s.bay_state().cover;
        require(fan_still && s.matron_state().bracing, "bay trap: the fan coasts to a stop while she holds the leaf");
        require(cover_running < 0.8 && cover_still > 0.999,
                "bay trap: the fan stopped, the steps beside it are no longer covered");
        // Onto the leaf: past what her hands can hold it goes down, onto her.
        const auto deaths = s.snapshot().death_count;
        double off_hinge = 0.0;
        double low_angle = 0.0;
        bool fell = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !fell; ++i) {
            const auto p = s.snapshot().player_position;
            if (p.y > Bay::kDeckTop) {
                steer_toward(s, -3.0, Matron::kBandNorthZ, 0.5);
            } else {
                (void)s.set_move_input(0.0, 0.0);
            }
            tick(s);
            const auto b = s.bay_state();
            if (b.leaf_angle < -0.02 && off_hinge == 0.0) {
                off_hinge = Bay::kLeafEastX - p.x;
            }
            low_angle = std::min(low_angle, b.leaf_angle);
            fell = s.snapshot().player_grounded && s.snapshot().player_position.y < 150.0;
        }
        std::cout << "  bay trap: the leaf went with the player " << off_hinge << " m out from its hinge, down to "
                  << low_angle << " rad; the player down at " << s.snapshot().player_position.y << " m\n";
        require(off_hinge > 0.8 && off_hinge < 2.5 && low_angle < Bay::kLeafHangAngle + 0.05,
                "bay trap: stepped onto, the leaf goes down past what she can hold");
        require(fell && s.snapshot().death_count == deaths, "bay trap: the player goes down through the bay and lives");
        // She lets go, climbs out after them; the district starts again by
        // itself, and the motor brings the leaf up level.
        bool out = false;
        bool level = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !(out && level); ++i) {
            tick(s);
            const auto m = s.matron_state();
            out = out || (m.trap == 0 && !m.bracing && m.position.y > Bay::kDeckTop);
            level = s.bay_state().leaf_open;
        }
        const auto after = s.bay_state();
        require(out && s.matron_state().alive, "bay trap: she lets go and climbs out by the bracket's rungs");
        require(level && after.restarts == 1 && after.fan_running,
                "bay trap: its hands out and guard shut, the district starts again and the leaf comes up level");
        std::cout << "PASS scraperx_sim bay trap: support_n=" << held.support_n << " off_hinge_m=" << off_hinge
                  << " restarts=" << after.restarts << "\n";
    }
    // ---- her trap beaten: the chain off its cleat onto the leaf's eye --------
    {
        Simulation s(InitialSpawn::Deck154);
        require(stage_hold(s), "bay chain: she holds the leaf");
        const std::vector<std::uint8_t> holding = s.save_game();
        require(walk_to(s, 0.4, -129.5, 6.0, 0.08), "bay chain: to the cleat on the rail's end post");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(0.4);
        require(s.snapshot().rig_action == 2 && s.snapshot().rig_target_entity_id == Bay::kShackleEntity && s.request_rig(),
                "bay chain: TAKE CHAIN is offered at the cleat, and taken");
        (void)s.advance_frame(0.4);
        require(s.snapshot().carrying_entity_id == Bay::kShackleEntity, "bay chain: the shackle in hand");
        // A step onto the leaf at its hinge (her hands hold that), the eye in reach.
        require(walk_to(s, -0.6, Bay::kEyeZ, 6.0, 0.08), "bay chain: a step onto the leaf by its hinge");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(0.4);
        require(s.snapshot().rig_action == 1 && s.snapshot().rig_target_entity_id == Bay::kLeafEntity && s.request_rig(),
                "bay chain: HOOK ON is offered at the leaf's eye, and done");
        (void)s.advance_frame(0.3);
        require(s.bay_state().hooked, "bay chain: the chain on the leaf's eye");
        // The chain has it: she lets go; the district starts again; the leaf
        // carries the player over the bay's far half, where she could not
        // have held it.
        bool let_go = false;
        bool restarted = false;
        double chain_n = 0.0;
        double held_low = 0.0;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !(let_go && restarted); ++i) {
            tick(s);
            let_go = let_go || !s.matron_state().bracing;
            const auto b = s.bay_state();
            if (let_go && !b.leaf_running) {
                chain_n = std::max(chain_n, b.chain_n);
                held_low = std::min(held_low, b.leaf_angle);
            }
            restarted = b.restarts > 0 && b.leaf_open;
        }
        std::cout << "  bay chain: her hands out, the chain holds the leaf at " << held_low << " rad with " << chain_n
                  << " N\n";
        require(let_go && chain_n > 1000.0 && held_low > -0.05,
                "bay chain: hooked, she lets go, and the chain holds the leaf up, its motor still stopped");
        require(restarted, "bay chain: her hands out, the district starts again and drives the leaf level");
        // Over its far half, where she could not have held it with the
        // player on it, and back.
        double low_y = 1.0e9;
        for (const double x : {-3.0, 0.6}) {
            bool there = false;
            for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !there; ++i) {
                steer_toward(s, x, Matron::kBandNorthZ, 0.6);
                tick(s);
                low_y = std::min(low_y, s.snapshot().player_position.y);
                there = std::abs(s.snapshot().player_position.x - x) < 0.2;
            }
            require(there, "bay chain: out over the leaf's far half and back");
        }
        require(low_y > Bay::kDeckTop && s.snapshot().death_count == 0,
                "bay chain: hooked and driven, the leaf carries the player over the bay");
        // A save made while she held it, loaded: the climb as it was when she
        // burst out (the checkpoint is not renewed while she is out), the bay
        // with it: the leaf up under its motor, the chain on its cleat, her
        // hands out of it.
        Simulation t(InitialSpawn::Deck154);
        require(t.load_game(holding.data(), holding.size()), "bay chain: the save made while she held it loads");
        (void)t.advance_frame(0.5);
        const auto loaded = t.bay_state();
        require(loaded.leaf_open && loaded.fan_running && !loaded.hands && !loaded.hooked && !t.matron_state().bracing &&
                    t.matron_state().trap == 0,
                "bay chain: loaded, the bay as at her release: the leaf driven level, the chain on its cleat");
        std::cout << "PASS scraperx_sim bay chain: chain_n=" << chain_n << " held_rad=" << held_low << "\n";
    }
}
