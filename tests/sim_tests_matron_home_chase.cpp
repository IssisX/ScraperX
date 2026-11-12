// J-102 (#421): her chase in the game's own world. Every other pursuit test
// moves her home to the 154 m deck (place_matron_home(154)), so none of them
// has her where the game puts her: in her locker on deck 1 (11 m), in the
// world the game starts (ExteriorGrade). Here he stands on her deck, ten
// metres from her locker, and is heard there the way the game hears him (the
// room's loudness, set_room_loudness, a pulse every few seconds). She must
// wake on it, come out, hunt him and close on him while he walks away along
// the deck, until her hands are on him, under gravity the whole way.
//
// What this does not cover yet: him on a deck she has no way to. From deck 1
// her own scan has no way up (#421: the lower stack's ladders refuse her
// body), and that pursuit is J-085's build, with its own test.

#include "sim_tests.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kDeck1Top = 11.0;
constexpr double kHimX = 1.0;       // ten metres west of her locker (x 11, z -126.2)
constexpr double kHimZ = -126.0;
constexpr double kRunToX = -12.0;   // where he walks once she is out: west along deck 1, then stands
constexpr double kWakeBoundS = 25.0;    // his first pulse to her out of the locker
constexpr double kCatchBoundS = 15.0;   // out of the locker to her hands on him
constexpr std::uint8_t kHunting = 3;

void run_matron_home_chase() {
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.advance_frame(1.0).accepted, "matron home chase: settle");
    s.debug_launch_player(Vector3{kHimX, kDeck1Top + 1.2, kHimZ}, Vector3{0.0, 0.0, 0.0});
    require(s.advance_frame(1.0).accepted, "matron home chase: him on deck 1");
    const auto him0 = s.snapshot().player_position;
    const auto her0 = s.matron_state();
    require(std::abs(him0.y - (kDeck1Top + 0.9)) < 0.2, "matron home chase: he stands on deck 1");
    require(!her0.released && std::abs(her0.position.y - (kDeck1Top + 1.05)) < 0.2,
            "matron home chase: she starts in her locker on deck 1, the game's home");

    const int hz = static_cast<int>(Simulation::kTickRateHz);
    double out_at = -1.0, hunt_at = -1.0, caught_at = -1.0;
    double gap_at_out = 0.0, closest = 1.0e9, worst_gravity = 1.0;
    const auto catches0 = her0.catches;
    const int limit = static_cast<int>((kWakeBoundS + kCatchBoundS + 2.0) * hz);
    for (int i = 0; i < limit && caught_at < 0.0; ++i) {
        const double t = static_cast<double>(i) / hz;
        // His noise where he is, a half-second pulse every 4 s.
        (void)s.set_room_loudness(i % (4 * hz) < hz / 2 ? 1.0 : 0.0);
        if (out_at >= 0.0) {
            // She is out: he walks away from her, west along her deck.
            const auto p = s.snapshot().player_position;
            const double dx = kRunToX - p.x;
            const double pace = std::abs(dx) > 0.2 ? 0.3 : 0.0;   // a walk, not a run
            (void)s.set_move_input(dx > 0.0 ? pace : -pace, 0.0);
            (void)s.set_facing(dx > 0.0 ? 1.0 : -1.0, 0.0);
        } else {
            (void)s.set_move_input(0.0, 0.0);
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        const auto p = s.snapshot().player_position;
        const double gap = std::hypot(m.position.x - p.x, m.position.y - p.y, m.position.z - p.z);
        if (m.released && out_at < 0.0) {
            out_at = t;
            gap_at_out = gap;
        }
        if (out_at >= 0.0) {
            closest = std::min(closest, gap);
            worst_gravity = std::min(worst_gravity, s.debug_matron_gravity_factor());
        }
        if (m.state == kHunting && hunt_at < 0.0) {
            hunt_at = t;
        }
        if (m.catches > catches0) {
            caught_at = t;
        }
        if (std::getenv("SCRAPERX_HOME_CHASE_TRACE") != nullptr && i % hz == 0) {
            std::cout << "    t " << i / hz << " state " << int(m.state) << " her (" << m.position.x << ", "
                      << m.position.y << ", " << m.position.z << ") him (" << p.x << ", " << p.y << ", " << p.z
                      << ") sees " << m.sees_player << " noise " << m.noise_heard << " gap " << gap << '\n';
        }
    }
    const auto m = s.matron_state();
    std::cout << "  matron home chase: out " << out_at << " s, hunting " << hunt_at << " s, caught " << caught_at
              << " s, gap at out " << gap_at_out << " m, closest " << closest << " m, her y " << m.position.y
              << ", gravity " << worst_gravity << '\n';
    require(out_at >= 0.0 && out_at <= kWakeBoundS, "matron home chase: his noise on her deck wakes her and she comes out");
    require(hunt_at >= out_at, "matron home chase: out of the locker she hunts him");
    require(caught_at >= 0.0 && caught_at - out_at <= kCatchBoundS,
            "matron home chase: she closes on him as he walks away and her hands are on him");
    require(worst_gravity == 1.0, "matron home chase: gravity on her the whole way");
    std::cout << "PASS scraperx_sim matron-home-chase: caught " << caught_at - out_at << " s after she came out\n";
}

}  // namespace

SCRAPERX_SLICE("matron-home-chase", run_matron_home_chase);
