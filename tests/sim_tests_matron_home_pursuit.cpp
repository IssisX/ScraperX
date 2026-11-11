// J-085 (#403): she follows him up the lower stack from her home. In the
// game's own world (ExteriorGrade) her locker is on deck 1 (11 m). He is
// heard there, she comes out, and then he is always a deck over her, making
// noise the way the game hears him (the room's loudness, a pulse every few
// seconds); each time her feet are on his deck he goes up one more, to the
// 132 m deck. Her way up each story is the shaft's corner pipe by her own
// hands (matron_pipe.cpp), over the rail of the deck she leaves and the rail
// of the deck she reaches; on deck 6 the machinery standing on the rail's
// corner is the top she gets off onto and on from (pipe_corner_raised), up
// onto it from that deck by her own mantle. Gravity on her the whole way,
// her grip inside her one budget (8 kN, 44 kW).
//
// Her stage is set Dangerous: this test is about her ways, not her rage.

#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kPitch = 11.0;
constexpr int kTopDeck = 12;            // 132 m
constexpr double kWakeBoundS = 25.0;    // his first pulse on her deck to her out of the locker
constexpr double kStoryBoundS = 90.0;   // a deck reached to her feet on the next (measured 29-40 s)
constexpr double kHimX = 10.0;          // where he stands on each deck: the south band, west of the pipe's corner
constexpr double kHimZ = -128.0;
constexpr double kForceCapN = 8000.0;
constexpr double kPowerCapW = 44000.0;

void run_matron_home_pursuit() {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(1.0).accepted, "matron home pursuit: settle");
    s.debug_set_matron_stage(1);
    const auto her0 = s.matron_state();
    require(!her0.released && std::abs(her0.position.y - (kPitch + 1.05)) < 0.2,
            "matron home pursuit: she starts in her locker on deck 1, the game's home");
    // Heard first on her own deck, ten metres from her locker.
    s.debug_launch_player(Vector3{1.0, kPitch + 1.2, -126.0}, Vector3{0.0, 0.0, 0.0});

    const int hz = static_cast<int>(Simulation::kTickRateHz);
    int deck = 1;
    double since = 0.0, worst_gravity = 1.0, peak_force = 0.0, peak_power = 0.0;
    double story_s[kTopDeck + 1] = {};
    const int limit = static_cast<int>((kWakeBoundS + kStoryBoundS * (kTopDeck - 1)) * hz);
    bool out = false, alive = true;
    for (int i = 0; i < limit && deck <= kTopDeck; ++i) {
        const double t = static_cast<double>(i) / hz;
        (void)s.set_room_loudness(i % (4 * hz) < hz / 2 ? 1.0 : 0.0);
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        alive = alive && m.alive;
        worst_gravity = std::min(worst_gravity, s.debug_matron_gravity_factor());
        peak_force = std::max(peak_force, m.pipe_peak_force_n);
        peak_power = std::max(peak_power, m.pipe_peak_power_w);
        if (std::getenv("SCRAPERX_HOME_PURSUIT_TRACE") != nullptr && i % hz == 0) {
            std::cout << "    t " << i / hz << " deck " << deck << " state " << int(m.state) << " her (" << m.position.x
                      << ", " << m.position.y << ", " << m.position.z << ") pipe " << m.on_pipe << '\n';
        }
        const double feet = m.position.y - 1.05;
        const bool there = deck == 1 ? m.released : (!m.on_pipe && std::abs(feet - kPitch * deck) < 0.4);
        if (!there) {
            if (t - since > (deck == 1 ? kWakeBoundS : kStoryBoundS)) {
                break;
            }
            continue;
        }
        story_s[deck] = t - since;
        std::cout << "  matron home pursuit: deck " << deck << " (" << kPitch * deck << " m) at " << t << " s, "
                  << story_s[deck] << " s after the deck under it\n";
        out = true;
        since = t;
        ++deck;
        if (deck <= kTopDeck) {
            s.debug_launch_player(Vector3{kHimX, kPitch * deck + 1.2, kHimZ}, Vector3{0.0, 0.0, 0.0});
        }
    }
    const auto m = s.matron_state();
    std::cout << "  matron home pursuit: last deck reached " << deck - 1 << ", her y " << m.position.y << ", gravity "
              << worst_gravity << ", pipe peak " << peak_force << " N " << peak_power << " W, flanges "
              << m.pipe_flanges << '\n';
    require(out, "matron home pursuit: his noise on her deck wakes her and she comes out");
    require(deck > kTopDeck, "matron home pursuit: she reaches his deck at every height of the lower stack, to 132 m");
    require(alive, "matron home pursuit: she climbs it alive");
    require(worst_gravity == 1.0, "matron home pursuit: gravity on her the whole way");
    require(peak_force <= kForceCapN + 1.0 && peak_power <= kPowerCapW + 1.0,
            "matron home pursuit: her grip on the pipe stays inside her one budget");
    std::cout << "PASS scraperx_sim matron-home-pursuit: deck 1 to 132 m, " << kTopDeck - 1 << " stories, slowest "
              << *std::max_element(story_s + 2, story_s + kTopDeck + 1) << " s\n";
}

}  // namespace

SCRAPERX_SLICE("matron-home-pursuit", run_matron_home_pursuit);
