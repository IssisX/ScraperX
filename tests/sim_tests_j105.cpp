// J-105 (#424), step 1: one flight of the leg spiral at the north-west leg,
// 462.25 -> 467.75 m. The flight is 20 steps of 0.275 m rise, 0.675 x 1.3 m,
// corner landings 1.3 m square, full-height plate on the outside corners.
// Every tread is in her map and her planner's grid walk routes foot to top;
// he walks it too. Her hunt-walk up the flight is NOT in this test: her
// planner cannot climb a within-region stair more than ~1 m above its
// region's top (see the note where her walk would be) — a planner job,
// reported with this branch. Only step 1 is approved — the rest of the
// spiral is not built and not tested here.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

namespace {

struct Step {
    float x;
    float z;
    float top;
};

// The flight, from the issue's numbers: the north-west leg's corner column
// is centred (-13.0325, -163.0325) (ring_inner(467.5) + 4.0 m out on both
// axes); the square is 5.3 m across (four 0.675 m treads plus two 1.3 m
// landings a side) with a 1.3 m walkway; the foot is the 462 ring at 462.25,
// step n tops at 462.25 + n * 0.275. The turn runs counter-clockwise seen
// from above: south westward, west northward, north eastward, east southward.
std::vector<Step> leg_spiral_steps() {
    constexpr float kLegX = -13.0325F;
    constexpr float kLegZ = -163.0325F;
    constexpr float kOuter = 2.65F;
    constexpr float kInner = 1.35F;
    constexpr float kGoing = 0.675F;
    constexpr float kFoot = 462.25F;
    constexpr float kRise = 0.275F;
    std::vector<Step> steps;
    int n = 0;
    const auto top = [&]() { return kFoot + static_cast<float>(++n) * kRise; };
    const float zc_s = kLegZ + 0.5F * (kInner + kOuter);   // south strip centre
    const float xc_w = kLegX - 0.5F * (kInner + kOuter);   // west strip centre
    const float zc_n = kLegZ - 0.5F * (kInner + kOuter);   // north strip centre
    const float xc_e = kLegX + 0.5F * (kInner + kOuter);   // east strip centre
    for (int k = 0; k < 4; ++k)   // south flight, westward
        steps.push_back({kLegX + kInner - (static_cast<float>(k) + 0.5F) * kGoing, zc_s, top()});
    steps.push_back({xc_w, zc_s, top()});   // south-west landing
    for (int k = 0; k < 4; ++k)   // west flight, northward
        steps.push_back({xc_w, kLegZ + kInner - (static_cast<float>(k) + 0.5F) * kGoing, top()});
    steps.push_back({xc_w, zc_n, top()});   // north-west landing
    for (int k = 0; k < 4; ++k)   // north flight, eastward
        steps.push_back({kLegX - kInner + (static_cast<float>(k) + 0.5F) * kGoing, zc_n, top()});
    steps.push_back({xc_e, zc_n, top()});   // north-east landing
    for (int k = 0; k < 4; ++k)   // east flight, southward
        steps.push_back({xc_e, kLegZ - kInner + (static_cast<float>(k) + 0.5F) * kGoing, top()});
    steps.push_back({xc_e, zc_s, top()});   // south-east landing, the top
    return steps;
}

// The trace prints her position every 5 s, as the pursuit tests do.
bool trace() { return std::getenv("SCRAPERX_J105_TRACE") != nullptr; }

}  // namespace

void run_j105_leg_spiral() {
    const std::vector<Step> steps = leg_spiral_steps();
    require(steps.size() == 20, "j105: one flight is 20 steps");
    require(std::abs(steps.back().top - 467.75F) < 0.01F, "j105: the top landing is 467.75 m");
    const Step &top = steps.back();
    constexpr double kFootX = -11.0325;   // the foot, on the 462 ring's north band
    constexpr double kFootZ = -161.0325;
    constexpr float kFootY = 462.25F;

    // Her map: every step has a patch at its top over its centre, and the
    // 20 steps are 20 distinct patches (none lost in the map's merge).
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "j105: settle the world");
    const scraperx::sim::traversal::AffordanceMap &map = s.matron_affordances();
    std::vector<int> patch_ids;
    for (const Step &st : steps) {
        const int p = map.patch_at(JPH::Vec3(st.x, st.top, st.z), 0.05F);
        if (p < 0) std::cout << "  j105: no patch at step top " << st.top << " (" << st.x << ", " << st.z << ")\n";
        require(p >= 0, "j105: every tread and landing is in her map");
        patch_ids.push_back(p);
    }
    std::sort(patch_ids.begin(), patch_ids.end());
    patch_ids.erase(std::unique(patch_ids.begin(), patch_ids.end()), patch_ids.end());
    require(patch_ids.size() == 20, "j105: the 20 steps are 20 distinct patches");
    std::cout << "  j105: all 20 steps in her map, " << map.regions().size() << " regions, " << map.patches().size()
              << " patches\n";

    // Her planner's floor search finds a way up the flight: the grid walk
    // from the foot to the top landing returns a path (the search her
    // journeys use to walk within a region).
    const std::size_t gw = s.debug_matron_grid_walk(kFootX, kFootY, kFootZ, top.x, top.top, top.z, 8.0F, 20000);
    std::cout << "  j105: grid walk foot to top: " << gw << " points\n";
    require(gw > 0, "j105: her planner's floor search finds a way up the flight");

    // She walks it: NOT in this test, and the reason is her planner's, not
    // the flight's — measured, not assumed. The hunt drives her up the
    // south flight to T4 (463.35 m) on a lead a few steps ahead, then stops
    // her dead: once the lead's floor is more than 1.0 m above her region's
    // area-weighted top (~462.5 m, the ring dominating the flight), the
    // lead is "elsewhere" (matron.cpp, the hunt's elsewhere check), and the
    // hunt calls take_way, which starts a travel to the lead's region —
    // the same region she stands in. A same-region travel is accepted as
    // "already there" (matron_journey.cpp travel_to), its step makes her
    // stand still for its 1 s arrival, and while a travel runs she is about
    // nothing else (matron.cpp, the travel preemption), so the hunt's
    // scan-walk never runs again: she stands at T4 and the hunt re-starts
    // the same stand-still travel every tick. A fixed lead at the top
    // landing from the foot fails earlier still (her scan-walk refuses a
    // winding path far longer than the straight line: the flight wound is
    // 16 m for a straight line of nothing). The flight itself is walkable
    // by her planner — the grid walk above finds a route foot to top, and
    // the map holds every tread — but her hunt cannot climb a within-region
    // stair more than ~1 m above its region's top. That is a planner job
    // (the travel way must not start for a same-region goal, or must walk
    // to the goal point within the region), reported to main with this
    // branch; the her-walk test joins it when that lands. This test proves
    // the geometry and his walk, and does not claim her walk.

    // He walks it: a fresh world, her far away at 154, him launched at the
    // foot and steered along the step centres.
    constexpr int hz = 90;
    Simulation h(InitialSpawn::Deck154);
    h.place_matron_home(154.0F);
    require(h.advance_frame(1.0).accepted, "j105: settle his world");
    h.debug_launch_player(Vector3{kFootX, kFootY + 1.0, kFootZ}, Vector3{0.0, 0.0, 0.0});
    for (int i = 0; i < hz; ++i) (void)h.advance_frame(Simulation::kFixedStepSeconds);
    std::size_t wp = 0;
    double h_worst = 0.0;
    double h_min_feet = 1.0e9;
    long grounded = 0;
    long total = 0;
    long air_run = 0;
    long max_air_run = 0;
    auto h_last = h.snapshot().player_position;
    int h_on_top = 0;
    bool h_arrived = false;
    double h_seconds = 0.0;
    for (int i = 0; i < 90 * hz && !h_arrived; ++i) {
        const auto snap = h.snapshot();
        const double dx = steps[wp].x - snap.player_position.x;
        const double dz = steps[wp].z - snap.player_position.z;
        const double d = std::hypot(dx, dz);
        const double d_next = wp + 1 < steps.size()
                                  ? std::hypot(steps[wp + 1].x - snap.player_position.x,
                                               steps[wp + 1].z - snap.player_position.z)
                                  : 1.0e9;
        if ((d < 0.5 || d_next < d) && wp + 1 < steps.size()) ++wp;
        if (d > 1e-6) (void)h.set_move_input(dx / d, dz / d);
        (void)h.advance_frame(Simulation::kFixedStepSeconds);
        ++total;
        if (snap.player_grounded) {
            ++grounded;
            air_run = 0;
        } else {
            ++air_run;
            max_air_run = std::max(max_air_run, air_run);
        }
        h_seconds = static_cast<double>(i + 1) / hz;
        const auto now = h.snapshot().player_position;
        h_worst = std::max(h_worst, std::hypot(now.x - h_last.x, now.y - h_last.y, now.z - h_last.z));
        h_last = now;
        const double feet_now = now.y - 0.9;
        h_min_feet = std::min(h_min_feet, feet_now);
        const bool there = std::abs(now.x - top.x) <= 0.6 && std::abs(now.z - top.z) <= 0.6 &&
                           std::abs(feet_now - top.top) < 0.15 && h.snapshot().player_grounded;
        h_on_top = there ? h_on_top + 1 : 0;
        if (h_on_top >= hz / 2) h_arrived = true;
        if (trace() && i % (5 * hz) == 0)
            std::cout << "  j105: " << h_seconds << " s, his feet (" << now.x << ", " << feet_now << ", " << now.z
                      << "), wp " << wp << "\n";
    }
    std::cout << "  j105: his walk: " << h_seconds << " s, grounded " << grounded << "/" << total << " ("
              << (total > 0 ? 100.0 * static_cast<double>(grounded) / static_cast<double>(total) : 0.0)
              << "%), longest airborne run " << max_air_run << " ticks, worst step " << h_worst << " m, lowest feet "
              << h_min_feet << " m\n";
    require(h_arrived, "j105: he reaches the top landing");
    require(h_min_feet > 462.0, "j105: he never falls off the stair");
    require(h_worst <= 0.35, "j105: his worst step is a step-up, not a teleport");
    // Mostly grounded, not flying: he goes briefly airborne at each 0.275 m
    // riser (the controller's step-up, ~5 ticks a step, longest run 9 ticks
    // measured), so 20 step-ups in a 4.4 s climb leave him grounded ~77%.
    // 70% keeps margin for that mechanism; a flying player would be far
    // below it and airborne for seconds, not ticks.
    require(static_cast<double>(grounded) / static_cast<double>(total) >= 0.70, "j105: he walks, mostly grounded");
    require(max_air_run <= 15, "j105: he never flies, longest hop 15 ticks");
    std::cout << "  j105: he walks it in " << h_seconds << " s, worst step " << h_worst << " m, grounded "
              << grounded << "/" << total << "\n";

    std::cout << "PASS scraperx_sim j105 leg spiral: her map holds all 20 steps, her planner's grid walk routes foot "
              << "to top (" << gw << " points), he walks it in " << h_seconds << " s\n";
}

SCRAPERX_GROUP("j105_leg_spiral", 1095, run_j105_leg_spiral);
SCRAPERX_SELECT("J-105", run_j105_leg_spiral);
SCRAPERX_SLICE("j105-leg-spiral", run_j105_leg_spiral);
