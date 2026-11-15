// J-102 (#421): off her corner pipe's top on deck 2, she gets to him. In the
// game's own world (ExteriorGrade) her locker is on deck 1; heard there, she
// comes out and he is on deck 2, making noise the way the game hears him (the
// room's loudness, a pulse every 4 s): first at S1's gangway end of the
// walkway, where she hears him from deck 1, then, once her feet are on deck 2,
// where each case puts him. Her pipe tops out on deck 2 into the pocket behind
// the parkour layer's blast barrier (band_stack.cpp, build_deck2_traversal):
// the shaft's rail behind her, the broken catwalk either side, the barrier
// between her and the walkway he crosses from S1 to C1. Her band line and the
// scan's floor patch both run under the barrier, so her walk at him met its
// steel and she stood there, heard him every 4 s and never reached him (J-085's
// slice moves him up a deck the moment her feet are on his). Each case: she
// tops out on deck 2 and her hands are on him inside kReachBoundS of her feet
// on that deck, alive, gravity on her the whole way, her grip on the pipe
// inside her one budget (8 kN, 44 kW). Her stage is the game's own (not forced).

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
constexpr double kClimbBoundS = 90.0;   // out of her locker to her feet on deck 2 (measured about 40 s)
constexpr double kReachBoundS = 30.0;   // her feet on deck 2 to her hands on him (measured 9.0-14.5 s)
constexpr double kHeardX = 10.0;        // on deck 2, heard from her deck 1 by the room's pulse
constexpr double kHeardZ = -125.0;
constexpr double kForceCapN = 8000.0;
constexpr double kPowerCapW = 44000.0;

void reach_him(const char *name, const double hx, const double hz) {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(1.0).accepted, "matron pipe top: settle");
    // Heard first on her own deck, ten metres from her locker.
    s.debug_launch_player(Vector3{1.0, kPitch + 1.2, -126.0}, Vector3{0.0, 0.0, 0.0});
    const int hz_ = static_cast<int>(Simulation::kTickRateHz);
    bool out = false, on_deck = false, alive = true;
    double out_at = 0.0, deck_at = 0.0, caught_at = -1.0, worst_gravity = 1.0, peak_force = 0.0, peak_power = 0.0;
    const int limit = static_cast<int>((25.0 + kClimbBoundS + kReachBoundS) * hz_);
    for (int i = 0; i < limit && caught_at < 0.0; ++i) {
        const double t = static_cast<double>(i) / hz_;
        (void)s.set_room_loudness(i % (4 * hz_) < hz_ / 2 ? 1.0 : 0.0);
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        alive = alive && m.alive;
        worst_gravity = std::min(worst_gravity, s.debug_matron_gravity_factor());
        peak_force = std::max(peak_force, m.pipe_peak_force_n);
        peak_power = std::max(peak_power, m.pipe_peak_power_w);
        if (!out && m.released) {
            out = true;
            out_at = t;
            // Where she hears him from deck 1 (the walkway at S1's gangway).
            s.debug_launch_player(Vector3{kHeardX, kDeck2 + 1.2, kHeardZ}, Vector3{0.0, 0.0, 0.0});
        }
        if (out && !on_deck && !m.on_pipe && std::abs(m.position.y - 1.05 - kDeck2) < 0.4) {
            on_deck = true;
            deck_at = t;
            // Her feet on his deck: he is where this case puts him.
            s.debug_launch_player(Vector3{hx, kDeck2 + 1.2, hz}, Vector3{0.0, 0.0, 0.0});
        }
        if (m.catches > 0) {
            caught_at = t;
        }
        if ((out && !on_deck && t - out_at > kClimbBoundS) || (on_deck && t - deck_at > kReachBoundS)) {
            break;
        }
    }
    const auto m = s.matron_state();
    std::cout << "  matron pipe top: " << name << ": out " << out << " at " << out_at << " s, on deck 2 " << on_deck
              << " at " << deck_at << " s, hands on him " << (caught_at >= 0.0) << " at " << caught_at << " s, her ("
              << m.position.x << ", " << m.position.y << ", " << m.position.z << "), gravity " << worst_gravity
              << ", pipe peak " << peak_force << " N " << peak_power << " W\n";
    require(out, "matron pipe top: his noise on her deck wakes her and she comes out");
    require(on_deck, "matron pipe top: she climbs her corner pipe to deck 2 after him");
    require(caught_at >= 0.0, "matron pipe top: off the pipe's top she gets round the barrier to him on deck 2");
    require(alive, "matron pipe top: she is alive");
    require(worst_gravity == 1.0, "matron pipe top: gravity on her the whole way");
    require(peak_force <= kForceCapN + 1.0 && peak_power <= kPowerCapW + 1.0,
            "matron pipe top: her grip on the pipe stays inside her one budget");
}

void run_matron_pipe_top() {
    // The walkway S1's gangway to C1, past the barrier's north face.
    reach_him("the walkway west, (10, -125)", 10.0, -125.0);
    reach_him("the walkway over the barrier, (14, -125)", 14.0, -125.0);
    // Behind the barrier's post, a metre off her band line.
    reach_him("the bay under the runway, (10, -128)", 10.0, -128.0);
    std::cout << "PASS scraperx_sim matron-pipe-top: off her pipe's top on deck 2, round the barrier to him\n";
}

}  // namespace

SCRAPERX_SLICE("matron-pipe-top", run_matron_pipe_top);
