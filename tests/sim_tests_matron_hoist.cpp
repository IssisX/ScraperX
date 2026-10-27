// The Matron rides a hoist (sim/matron_hoist.cpp): put down on the deck under
// one, sent to the deck above, she boards it, frees what holds it with her own
// hands and weight (S2's shoot bolt shoved clear; S3's pan weighed down),
// rides it and steps off onto the far deck. Never placed there, never moved
// faster than her feet and the car carry her, never over her 8 kN budget.

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void ride(const char *name, const Vector3 start, const double goal_y) {
    Simulation s(InitialSpawn::Deck154);
    s.debug_place_matron(start);
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(s.debug_matron_travel(goal_y), "matron hoist: a journey to the deck above plans by its hoist");
    Vector3 last = s.matron_state().position;
    double worst = 0.0;
    float peak = 0.0F;
    double t = 0.0;
    for (; t < 150.0; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Vector3 at = s.matron_state().position;
        worst = std::max(worst, std::hypot(at.x - last.x, at.y - last.y, at.z - last.z));
        last = at;
        peak = std::max(peak, s.matron_hoist().hand_n);
        if (!s.matron_journey().active && t > 3.0) {
            break;
        }
    }
    const double feet = s.matron_state().position.y - double(scraperx::sim::Matron::kStandCentre);
    std::cout << "  " << name << ": t=" << t << " rides=" << s.matron_hoist().rides << " feet=" << feet
              << " worst_step=" << worst << " peak_hand=" << peak << '\n';
    require(s.matron_hoist().rides >= 1, "matron hoist: she rode it");
    require(std::abs(feet - goal_y) < 0.4, "matron hoist: she stands on the far deck");
    require(worst < 0.2, "matron hoist: never snapped (no step over 0.2 m a tick)");
    require(peak > 0.0F && peak <= 8000.0F, "matron hoist: her hands freed it, inside her 8 kN budget");
    std::cout << "PASS scraperx_sim matron hoist " << name << '\n';
}
} // namespace

void run_matron_hoist() {
    ride("S2 44->66", Vector3{10.0, 44.02, -130.0}, 66.0);
    ride("S3 88->132", Vector3{-8.0, 88.02, -162.0}, 132.0);
}
