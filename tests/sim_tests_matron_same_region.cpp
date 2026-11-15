// J-126 (#449): a lead inside the region she stands in, above that region's
// area-weighted top, is walked to over the region itself. Deck 12's stepped
// incline (band_stack.cpp, build_c3) is one region with the deck: her map's
// region top is ~132.1 m (the deck dominates), the incline's landing stands
// at 135.5 m, 3.4 m above it. Before J-126 that lead read as "elsewhere",
// the hunt asked her ways for it, and a travel to her own region was taken
// as "already there": she stood at the incline's foot for the whole chase.
// Now she walks up the incline to the landing on her own feet.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

}  // namespace

void run_matron_same_region() {
    constexpr int hz = static_cast<int>(Simulation::kTickRateHz);
    // The incline's foot on Deck 12 and its landing (band_stack.cpp build_c3).
    const Vector3 foot{-10.5, 132.0, -172.0};
    const Vector3 landing{-10.5, 135.5, -144.75};
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron same region: settle");

    // The map this test stands on: foot and landing in one region, the
    // landing more than 3 m above that region's area-weighted top.
    const scraperx::sim::traversal::AffordanceMap &map = s.matron_affordances();
    const int foot_region = map.region_at(JPH::Vec3(float(foot.x), float(foot.y), float(foot.z)));
    const int landing_region = map.region_at(JPH::Vec3(float(landing.x), float(landing.y), float(landing.z)));
    require(foot_region >= 0 && foot_region == landing_region,
            "matron same region: the incline's foot and landing are one region of her map");
    const float region_top = map.regions()[static_cast<std::size_t>(foot_region)].top;
    std::cout << "  matron same region: region " << foot_region << ", area-weighted top " << region_top
              << " m, landing " << landing.y << " m (" << landing.y - region_top << " m above)\n";
    require(landing.y - region_top > 3.0, "matron same region: the landing is over 3 m above the region's top");

    // Him standing on the landing, heard there every 8 s; her at the foot,
    // dangerous, hunting.
    s.debug_launch_player(Vector3{landing.x, landing.y + 0.3, landing.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(foot);
    tick(s);
    auto last = s.matron_state().position;
    double worst_step = 0.0;
    double lowest_feet = 1.0e9;
    double highest_feet = -1.0e9;
    double seconds = 0.0;
    bool arrived = false;
    bool alive = true;
    int on_landing = 0;
    for (int i = 0; i < 60 * hz; ++i) {
        s.debug_set_matron_stage(1);
        if (i % (8 * hz) == 0) s.debug_matron_hear(landing);
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const auto m = s.matron_state();
        worst_step = std::max(worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z));
        last = m.position;
        if (!m.alive) {
            alive = false;
            break;
        }
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        lowest_feet = std::min(lowest_feet, feet);
        highest_feet = std::max(highest_feet, feet);
        if (std::getenv("SCRAPERX_MATRON_TRACE") != nullptr && i % (5 * hz) == 0) {
            std::cout << "    t " << i / hz << " at (" << m.position.x << ", " << feet << ", " << m.position.z
                      << ") state " << int(m.state) << " climbing " << m.climbing << '\n';
        }
        // Up on the incline's top, near the landing: within 1.5 m of it, her
        // feet within a step of its floor, for a second.
        const bool there = std::hypot(m.position.x - landing.x, m.position.z - landing.z) < 1.5 &&
                           std::abs(feet - landing.y) < 0.3 && !m.climbing;
        on_landing = there ? on_landing + 1 : 0;
        if (on_landing >= hz) {
            arrived = true;
            seconds = static_cast<double>(i) / hz;
            break;
        }
    }
    std::cout << "  matron same region: arrived " << arrived << " in " << seconds << " s, feet " << lowest_feet << ".."
              << highest_feet << " m, worst step " << worst_step << " m, alive " << alive << '\n';
    require(alive, "matron same region: she is alive");
    require(arrived, "matron same region: she walks up her own region's incline to the lead on its landing");
    require(lowest_feet > 131.5, "matron same region: she never falls off the deck");
    require(worst_step <= 0.35, "matron same region: her worst step is a step, not a teleport");
    std::cout << "PASS scraperx_sim matron same region: up the incline to a lead " << landing.y - region_top
              << " m above her region's top in " << seconds << " s\n";
}

SCRAPERX_GROUP("matron_same_region", 935, run_matron_same_region);
SCRAPERX_SELECT("matron-same-region", run_matron_same_region);
