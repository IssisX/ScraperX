// The Matron rides a hoist (sim/matron_hoist.cpp): put down on the deck under
// one, sent to the deck above, she boards it, frees what holds it with her own
// hands and weight (S2's shoot bolt shoved clear; S3's pan weighed down),
// rides it and steps off onto the far deck. Never placed there, never moved
// faster than her feet and the car carry her, never over her 8 kN budget.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
namespace traversal = scraperx::sim::traversal;

void ride(const char *name, const Vector3 start, const double goal_y) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    s.debug_place_matron(start);
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(s.debug_matron_travel(goal_y), "matron hoist: a journey to the deck above plans by its hoist");
    Vector3 last = s.matron_state().position;
    double worst = 0.0;
    float peak = 0.0F;
    double worst_force = 0.0, worst_power = 0.0;   // her feet plus her hands, one budget
    double t = 0.0;
    for (; t < 150.0; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Vector3 at = s.matron_state().position;
        worst = std::max(worst, std::hypot(at.x - last.x, at.y - last.y, at.z - last.z));
        last = at;
        peak = std::max(peak, s.matron_hoist().hand_n);
        // Her hands' push and her feet's spend share the one budget (8 kN,
        // 44 kW): the push is charged against what her feet already spent.
        worst_force = std::max(worst_force, s.matron_journey().force_n + double(s.matron_hoist().hand_n));
        worst_power = std::max(worst_power, s.matron_journey().power_w + double(s.matron_hoist().hand_power_w));
        if (!s.matron_journey().active && t > 3.0) {
            break;
        }
    }
    const double feet = s.matron_state().position.y - double(scraperx::sim::Matron::kStandCentre);
    std::cout << "  " << name << ": t=" << t << " rides=" << s.matron_hoist().rides << " feet=" << feet
              << " worst_step=" << worst << " peak_hand=" << peak << " worst_force=" << worst_force
              << " worst_power=" << worst_power << '\n';
    require(s.matron_hoist().rides >= 1, "matron hoist: she rode it");
    require(std::abs(feet - goal_y) < 0.4, "matron hoist: she stands on the far deck");
    require(worst < 0.2, "matron hoist: never snapped (no step over 0.2 m a tick)");
    require(peak > 0.0F && peak <= 8000.0F, "matron hoist: her hands freed it, inside her 8 kN budget");
    require(worst_force <= 8000.0, "matron hoist: her feet and hands together stay inside her 8 kN budget");
    require(worst_power <= 44000.0, "matron hoist: her feet and hands together stay inside her 44 kW budget");
    std::cout << "PASS scraperx_sim matron hoist " << name << '\n';
}
// In ordinary pursuit (not the debug travel), a lead on a floor her map
// cannot join to hers except by a hoist starts the ride: take_way asks her
// travel, and its hoist fallback plans it.
void pursuit_rides_hoist() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron hoist pursuit: settle");
    const Vector3 prey{10.0, 66.02, -130.0};   // on the deck the S2 car lands on, over it
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.launch_matron_for_test(Vector3{10.0, 44.02 + 1.05, -130.0});
    s.debug_matron_rule_out_pipe();
    s.debug_set_matron_stage(1);
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    Vector3 last = s.matron_state().position;
    double worst = 0.0;
    int on_goal = 0;
    int max_phase = 0;
    bool rode = false;
    double t = 0.0;
    for (int i = 0; i < 240 * hz; ++i) {
        if (i % (8 * hz) == 0) {
            s.debug_matron_hear(prey);
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        t = double(i) / hz;
        const auto m = s.matron_state();
        worst = std::max(worst, std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z));
        last = m.position;
        max_phase = std::max(max_phase, s.matron_hoist().phase);
        if (s.matron_hoist().rides >= 1) {
            rode = true;
        }
        if (!m.alive) {
            break;
        }
        const double feet = m.position.y - double(scraperx::sim::Matron::kStandCentre);
        // On the lead's deck, not on the car still carrying her there: a
        // hoist leg under way holds the arrival until it steps off.
        const bool there =
            std::abs(feet - prey.y) < 0.35 && !m.climbing && s.matron_hoist().phase == 0;
        on_goal = there ? on_goal + 1 : 0;
        if (on_goal >= hz) {
            break;
        }
    }
    const double feet = s.matron_state().position.y - double(scraperx::sim::Matron::kStandCentre);
    std::cout << "  matron hoist pursuit: rode=" << rode << " max_phase=" << max_phase << " feet=" << feet
              << " worst_step=" << worst << " t=" << t << '\n';
    require(rode, "matron hoist pursuit: her hunt rides the hoist to the lead's floor");
    require(std::abs(feet - prey.y) < 0.4, "matron hoist pursuit: she stands on the lead's deck");
    require(worst < 0.2, "matron hoist pursuit: never snapped (no step over 0.2 m a tick)");
    std::cout << "PASS scraperx_sim matron hoist pursuit\n";
}

// A new destination she cannot reach, asked while she is riding, leaves the
// ride alone: the request fails and the hoist leg rides on.
void unreachable_request_keeps_ride() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    s.debug_place_matron(Vector3{10.0, 44.02, -130.0});
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(s.debug_matron_travel(66.0), "matron hoist: a journey to the deck above plans by its hoist");
    // To the car, and onto it: the leg is under way.
    int phase = 0;
    double t = 0.0;
    for (; t < 150.0; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        phase = s.matron_hoist().phase;
        if (phase >= 2) {
            break;
        }
    }
    require(phase >= 2, "matron hoist: she is on the hoist when the new request comes");
    // A region neither her board nor the far stop reaches: no plan joins it,
    // and no hoist does either.
    const traversal::AffordanceMap &map = s.matron_affordances();
    const int board = map.region_at(JPH::Vec3(10.0F, 44.02F, -130.0F));
    const int exit = map.region_at(JPH::Vec3(10.0F, 66.02F, -130.0F));
    require(board >= 0 && exit >= 0, "matron hoist: her board and the far stop are regions on her map");
    int unreachable = -1;
    for (const traversal::Region &r : map.regions()) {
        if (r.id == board || r.id == exit) {
            continue;
        }
        if (!map.plan(board, r.id).empty()) {
            continue;
        }
        if (exit >= 0 && !map.plan(exit, r.id).empty()) {
            continue;
        }
        unreachable = r.id;
        break;
    }
    require(unreachable >= 0, "matron hoist: the map has a region neither the board nor the exit reaches");
    const int phase_before = s.matron_hoist().phase;
    require(!s.debug_matron_travel_region(unreachable), "matron hoist: the new destination is unreachable");
    require(s.matron_hoist().phase == phase_before, "matron hoist: an unreachable request leaves the ride alone");
    // Ride it out.
    Vector3 last = s.matron_state().position;
    double worst = 0.0;
    for (; t < 150.0; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Vector3 at = s.matron_state().position;
        worst = std::max(worst, std::hypot(at.x - last.x, at.y - last.y, at.z - last.z));
        last = at;
        if (!s.matron_journey().active && t > 3.0) {
            break;
        }
    }
    const double feet = s.matron_state().position.y - double(scraperx::sim::Matron::kStandCentre);
    std::cout << "  matron hoist unreachable request: phase " << phase_before << " kept, rides=" << s.matron_hoist().rides
              << " feet=" << feet << " worst_step=" << worst << '\n';
    require(s.matron_hoist().rides >= 1, "matron hoist: she still rode it");
    require(std::abs(feet - 66.0) < 0.4, "matron hoist: she stands on the far deck");
    require(worst < 0.2, "matron hoist: never snapped (no step over 0.2 m a tick)");
    std::cout << "PASS scraperx_sim matron hoist unreachable request\n";
}
} // namespace

void run_matron_hoist() {
    ride("S2 44->66", Vector3{10.0, 44.02, -130.0}, 66.0);
    ride("S3 88->132", Vector3{-8.0, 88.02, -162.0}, 132.0);
    unreachable_request_keeps_ride();
    pursuit_rides_hoist();
}
SCRAPERX_GROUP("matron_hoist", 945, run_matron_hoist);
SCRAPERX_SLICE("matron-hoist", run_matron_hoist);
