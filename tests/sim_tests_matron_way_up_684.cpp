// Her way back up from the 684 deck (#427, J-108): the bait lift, west of the
// 706 deck's strip, takes her down to the 684 deck, so it lands only while she
// has her own way back up to the 706 deck. Put down where its platform A lets
// her off and sent to the 706 deck, she plans it on her scanned map and climbs
// it on her own moves (today the backup ladder through the 706 deck's hatch). Every move ends Done; gravity stays
// on; her feet and hands stay inside the one budget (8 kN, 44 kW); never
// placed, never snapped.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
namespace traversal = scraperx::sim::traversal;

constexpr double kDeck684 = 684.1;
constexpr double kDeck706 = 706.1;

void climb_from_684() {
    Simulation s(InitialSpawn::Deck684);
    // On the 684 deck, where the bait lift's A lets her off (x -7.0 to -5.15,
    // z -146.2 to -143.8).
    s.debug_place_matron(Vector3{-6.1, kDeck684, -145.0});
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    const traversal::AffordanceMap &map = s.matron_affordances();
    const std::uint32_t failed_before = s.matron_journey().moves_failed;
    require(s.debug_matron_travel(kDeck706), "way up 684: a journey from the 684 deck to the 706 deck plans");
    Vector3 last = s.matron_state().position;
    double worst_step = 0.0, worst_force = 0.0, worst_power = 0.0, min_gravity = 1.0;
    std::uint8_t move = 0;
    std::string moves;
    double t = 0.0;
    for (; t < 150.0; t += Simulation::kFixedStepSeconds) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Simulation::MatronJourney j = s.matron_journey();
        const Vector3 at = s.matron_state().position;
        worst_step = std::max(worst_step, std::hypot(at.x - last.x, at.y - last.y, at.z - last.z));
        last = at;
        worst_force = std::max(worst_force, j.force_n + double(s.matron_hoist().hand_n));
        worst_power = std::max(worst_power, j.power_w + double(s.matron_hoist().hand_power_w));
        min_gravity = std::min(min_gravity, j.gravity_factor);
        if (j.move != move && j.move != 0) {
            moves += std::string(moves.empty() ? "" : ",") + traversal::move_name(static_cast<traversal::MoveKind>(j.move));
        }
        move = j.move;
        if (!j.active) {
            break;
        }
    }
    const Simulation::MatronJourney j = s.matron_journey();
    const double top = j.region >= 0 ? map.regions()[static_cast<std::size_t>(j.region)].top : -1.0;
    const double feet = s.matron_state().position.y - double(scraperx::sim::Matron::kStandCentre);
    std::cout << "  way up 684 -> 706: t=" << t << " arrivals=" << j.arrivals << " region_top=" << top
              << " feet=" << feet << " x=" << last.x << " way=" << moves << " moves=" << j.moves_done
              << " failed=" << (j.moves_failed - failed_before) << " replans=" << j.replans
              << " worst_step=" << worst_step << " worst_force=" << worst_force << " worst_power=" << worst_power
              << '\n';
    require(!j.active && j.arrivals > 0 && std::abs(top - kDeck706) < 0.3,
            "way up 684: she arrives on the 706 deck by her own plan");
    require(std::abs(feet - kDeck706) < 0.4, "way up 684: she stands on the 706 deck");
    require(j.moves_failed == failed_before, "way up 684: every move of the climb ends Done");
    require(min_gravity == 1.0, "way up 684: her gravity stays on");
    require(worst_step < 0.2, "way up 684: never snapped (no step over 0.2 m a tick)");
    require(worst_force <= double(traversal::Limits{}.total_force_n) + 1.0,
            "way up 684: her feet and hands stay inside her 8 kN budget");
    require(worst_power <= 44000.0, "way up 684: her feet and hands stay inside her 44 kW budget");
    std::cout << "PASS scraperx_sim matron way up from 684: seconds=" << t << " way=" << moves
              << " worst_force_n=" << worst_force << '\n';
}
} // namespace

void run_matron_way_up_684() { climb_from_684(); }
SCRAPERX_GROUP("matron_way_up_684", 947, run_matron_way_up_684);
SCRAPERX_SLICE("matron-way-up-684", run_matron_way_up_684);
