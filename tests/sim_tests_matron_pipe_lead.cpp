// J-102 (#421), her chase window: one sound of him, then silence, and she
// still gets to him. The game's opening gives her exactly that: his arrival
// off S1 onto deck 2, over her locker, is the one sound of his she hears. Her
// chase by a way of her own lasts kPipeChaseDangerousSeconds (40 s) at a pest
// or dangerous stage; the 16 s she strains in her locker and the 35-40 s a
// pest takes up the corner pipe both ran on that clock, so she topped out on
// deck 2 with the window spent, gave up at once and counted the sound as a
// lie (fooled). Here: the game's world, her own stage, him heard once on deck
// 2 at the moment she wakes, then standing silent where he was heard. She
// must come out, climb the pipe, get her hands on him within kReachBoundS of
// her feet on deck 2, and not count his sound as a lie. Gravity on her the
// whole way, her grip on the pipe inside her one budget (8 kN, 44 kW).

#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kPitch = 11.0;
constexpr double kDeck2 = 2.0 * kPitch;
constexpr double kHimX = 10.0;          // the walkway at S1's gangway, where his arrival is
constexpr double kHimZ = -125.0;
constexpr double kClimbBoundS = 120.0;  // her wake to her feet on deck 2 (measured about 56 s)
constexpr double kReachBoundS = 30.0;   // her feet on deck 2 to her hands on him
constexpr double kForceCapN = 8000.0;
constexpr double kPowerCapW = 44000.0;

void run_matron_pipe_lead() {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(1.0).accepted, "matron pipe lead: settle");
    // His noise on her deck wakes her (the room's pulse, as the game hears him).
    s.debug_launch_player(Vector3{1.0, kPitch + 1.2, -126.0}, Vector3{0.0, 0.0, 0.0});
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    bool woke = false, on_deck = false, alive = true;
    double woke_at = 0.0, deck_at = 0.0, caught_at = -1.0, worst_gravity = 1.0, peak_force = 0.0, peak_power = 0.0;
    std::uint32_t fooled_at_deck = 0;
    const int limit = static_cast<int>((kClimbBoundS + kReachBoundS + 2.0) * hz);
    for (int i = 0; i < limit && caught_at < 0.0; ++i) {
        const double t = static_cast<double>(i) / hz;
        (void)s.set_room_loudness(!woke && i % (4 * hz) < hz / 2 ? 1.0 : 0.0);
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        alive = alive && m.alive;
        worst_gravity = std::min(worst_gravity, s.debug_matron_gravity_factor());
        peak_force = std::max(peak_force, m.pipe_peak_force_n);
        peak_power = std::max(peak_power, m.pipe_peak_power_w);
        if (!woke && m.state != 0) {
            // Woken: from here he is on deck 2, heard there this once, and silent.
            woke = true;
            woke_at = t;
            s.debug_launch_player(Vector3{kHimX, kDeck2 + 1.2, kHimZ}, Vector3{0.0, 0.0, 0.0});
            s.debug_matron_hear(Vector3{kHimX, kDeck2, kHimZ});
        }
        if (woke && !on_deck && !m.on_pipe && std::abs(m.position.y - 1.05 - kDeck2) < 0.4) {
            on_deck = true;
            deck_at = t;
            fooled_at_deck = m.fooled;
        }
        if (m.catches > 0) {
            caught_at = t;
        }
        if ((woke && !on_deck && t - woke_at > kClimbBoundS) || (on_deck && t - deck_at > kReachBoundS)) {
            break;
        }
    }
    const auto m = s.matron_state();
    std::cout << "  matron pipe lead: woke " << woke << " at " << woke_at << " s, on deck 2 " << on_deck << " at "
              << deck_at << " s, hands on him " << (caught_at >= 0.0) << " at " << caught_at << " s, state "
              << int(m.state) << ", fooled " << m.fooled << " (" << fooled_at_deck << " on the deck), her ("
              << m.position.x << ", " << m.position.y << ", " << m.position.z << "), gravity " << worst_gravity
              << ", pipe peak " << peak_force << " N " << peak_power << " W\n";
    require(woke, "matron pipe lead: his noise on her deck wakes her");
    require(on_deck, "matron pipe lead: she climbs her corner pipe to the deck she heard him on");
    require(caught_at >= 0.0, "matron pipe lead: one sound of him is enough: off the pipe she gets to him");
    require(m.fooled == 0, "matron pipe lead: his real sound is not counted as a lie");
    require(alive, "matron pipe lead: she is alive");
    require(worst_gravity == 1.0, "matron pipe lead: gravity on her the whole way");
    require(peak_force <= kForceCapN + 1.0 && peak_power <= kPowerCapW + 1.0,
            "matron pipe lead: her grip on the pipe stays inside her one budget");
    std::cout << "PASS scraperx_sim matron-pipe-lead: one sound, up the pipe, hands on him " << caught_at - deck_at
              << " s after her feet on deck 2\n";
}

}  // namespace

SCRAPERX_SLICE("matron-pipe-lead", run_matron_pipe_lead);
