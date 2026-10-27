// The Matron frail (Stage::Pest, outside her Turn) and a parked line
// (sim/matron_line.cpp, follow_line): with her lead a floor or more under her
// deck, she must not be held on the 154 m line's stand. Refusing a parked
// line only once she stood on it sent her to the stand, off it, and back to
// it the next tick: on trunk eb67b7ed she made 7 trips onto it and stood on
// it 41 s of 60 (the dither Cory's phone showed at about 5.6, 156.25, -131.9).
#include "sim_tests.hpp"

#include <cmath>
#include <cstdio>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

// The 154 m line's stand under its parked trolley (sim/zipline.cpp, line.stand).
constexpr double kStandX = 5.6;
constexpr double kStandZ = -131.9;
constexpr double kOnStandY = 155.9;   // her centre on the stand (155.05 on the deck)

void lead_below(const char *name, const Vector3 lead) {
    Simulation s(InitialSpawn::Deck154);
    for (int i = 0; i < 45; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    s.launch_matron_for_test(Vector3{8.0, 155.1, -128.5});
    int visits = 0;
    bool on = false;
    double on_s = 0.0;
    const int ticks = static_cast<int>(60.0 * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        if (i % static_cast<int>(8 * Simulation::kTickRateHz) == 10) {
            s.debug_matron_hear(lead);   // a sound every 8 s, the chase-time rule
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        require(m.alive, "matron stand: she lives");
        require(m.stage == 0, "matron stand: frail (Pest) throughout");
        const bool now = std::hypot(m.position.x - kStandX, m.position.z - kStandZ) < 0.8 && m.position.y > kOnStandY;
        if (now && !on) {
            ++visits;
        }
        if (now) {
            on_s += Simulation::kFixedStepSeconds;
        }
        on = now;
    }
    std::printf("matron stand %s: %d trips onto the parked 154 stand, %.1f s on it\n", name, visits, on_s);
    require(visits <= 1, "matron stand: frail, she does not go back and forth to a parked line's stand");
    require(on_s < 5.0, "matron stand: frail, a parked line's stand does not hold her");
}
}  // namespace

void run_matron_stand() {
    lead_below("lead at 132", Vector3{2.5, 132.9, -127.2});
    lead_below("lead at 121", Vector3{-12.0, 121.9, -128.0});
}
