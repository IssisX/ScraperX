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

// J-085 part 2: the pipe is not her only way up the lower stack. Where a way
// of her own map starts nearer her than the pipe's corner, she takes it with
// the pipe there: the C1 davit ladder (33 -> 44), the C2 rack and its davit
// ladder (77 -> 88), the S3 hoist (88 -> 132). Dangerous, a lead heard every
// 4 s on the deck above; her feet and hands in her one budget, gravity on.
void lower_way(const char *name, const Vector3 her, const double goal, const bool hoist) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron lower ways: settle");
    (void)s.matron_affordances();
    const Vector3 him{0.0, goal, -128.5};
    s.debug_launch_player(Vector3{him.x, him.y + 1.2, him.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(her);
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    const auto m0 = s.matron_state();
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    bool there = false, alive = true;
    double t = 0.0, worst_gravity = 1.0, worst_force = 0.0, worst_power = 0.0;
    for (int i = 0; i < 120 * hz && !there && alive; ++i) {
        s.debug_set_matron_stage(1);
        if (i % (4 * hz) == 0) {
            s.debug_matron_hear(him);
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        alive = m.alive;
        worst_gravity = std::min(worst_gravity, s.debug_matron_gravity_factor());
        worst_force = std::max(worst_force, s.matron_journey().force_n + double(s.matron_hoist().hand_n));
        worst_power = std::max(worst_power, s.matron_journey().power_w + double(s.matron_hoist().hand_power_w));
        t = static_cast<double>(i) / hz;
        there = !m.on_pipe && std::abs(m.position.y - 1.05 - goal) < 0.4;
    }
    const auto m = s.matron_state();
    const auto climbs = m.climbs - m0.climbs;
    const auto rides = s.matron_hoist().rides;
    std::cout << "  matron lower ways: " << name << ": on the deck above " << there << " after " << t << " s, climbs "
              << climbs << ", hoist rides " << rides << ", pipe links " << m.pipe_links - m0.pipe_links << ", feet and hands "
              << worst_force << " N " << worst_power << " W\n";
    require(there && alive, "matron lower ways: by a way of her own map she is on the deck above");
    require(m.pipe_links == m0.pipe_links && (hoist ? s.matron_hoist().phase > 0 || rides > 0 : climbs > 0),
            "matron lower ways: her map's own way, nearer her than the pipe, not the pipe");
    require(worst_gravity == 1.0 && worst_force <= kForceCapN + 1.0 && worst_power <= kPowerCapW + 1.0,
            "matron lower ways: gravity on, her feet and hands inside her one budget");
}

void run_matron_lower_ways() {
    // C1 and C2's first stories: up onto the cabinet, the jump for the duct's
    // lip (3.6 m) and its mantle, then the vent stack's pole or the wall
    // ladder (J-085, with the cabinet and the switchgear 5-10 cm nearer the
    // ducts: her take-off within her 0.70 m of the lip).
    lower_way("C1 cabinet, duct and vent stack, 22 -> 33", Vector3{19.0, 22.02, -125.0}, 33.0, false);
    lower_way("C2 switchgear, duct and wall ladder, 66 -> 77", Vector3{21.0, 66.02, -142.0}, 77.0, false);
    lower_way("C1 davit ladder, 33 -> 44", Vector3{12.5, 33.02, -122.0}, 44.0, false);
    lower_way("C2 rack and davit ladder, 77 -> 88", Vector3{21.0, 77.02, -156.0}, 88.0, false);
    lower_way("S3 hoist, 88 -> 132", Vector3{-8.0, 88.02, -162.0}, 132.0, true);
    std::cout << "PASS scraperx_sim matron lower ways: C1 and C2 by their jumps and ladders, and the S3 hoist, with the pipe there\n";
}

void run_matron_home_pursuit_slice() {
    run_matron_home_pursuit();
    run_matron_lower_ways();
}

}  // namespace

SCRAPERX_SLICE("matron-home-pursuit", run_matron_home_pursuit_slice);
