// The Matron on the tower's own ladders, up AND down (sim/matron.cpp
// climb(): her hand on a real hold, a capped force, gravity on): each climb of
// the first multi-deck territory taken both ways from a standing start, and
// each must leave her standing on the steel at its other end, in the region
// the map says, having never been carried, snapped or let fall.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_routes.hpp"

namespace {
namespace routes = scraperx::sim::routes;
using routes::Connector;
using routes::Region;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

struct Leg {
    const char *name;
    Connector connector;
    bool down;
    Vector3 start;      // her feet, standing
    double end_y;       // the top of the steel she ends on
    Region end_region;  // Region::Count: a ledge of C3's that is no deck
};

struct Result {
    double x = 0.0, y = 0.0, z = 0.0, peak_force = 0.0, worst_step = 0.0, seconds = 0.0;
    std::uint64_t releases = 0;
};

Result take(const Leg &leg) {
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron traverse: settle");
    s.debug_place_matron(leg.start);
    for (int i = 0; i < 30; ++i) {
        tick(s);
    }
    const auto before = s.matron_state();
    require(s.debug_matron_climb(static_cast<int>(leg.connector), leg.down), leg.name);
    Result r;
    auto last = s.matron_state().position;
    int ticks = 0;
    // (Her view of herself, climbing or not, is refreshed in the step.)
    for (bool climbing = true; ticks < 90 * static_cast<int>(Simulation::kTickRateHz) && climbing; ++ticks) {
        tick(s);
        climbing = s.matron_state().climbing;
        const auto m = s.matron_state();
        r.peak_force = std::max(r.peak_force, m.climb_force_n);
        r.worst_step = std::max(r.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                         m.position.z - last.z));
        last = m.position;
    }
    // Then a second on her feet, to show she stands there.
    for (int i = 0; i < 90; ++i) {
        tick(s);
    }
    const auto m = s.matron_state();
    r.x = m.position.x;
    r.y = m.position.y - Matron::kStandCentre;
    r.z = m.position.z;
    r.seconds = ticks / Simulation::kTickRateHz;
    r.releases = m.climb_releases - before.climb_releases;
    return r;
}

} // namespace

void run_matron_traverse() {
    const routes::Map map;
    const std::array<Leg, 8> legs{{
        {"home ladder up: 154 to the 176 ring", Connector::HomeLadder, false, {5.0, 154.0, -129.55}, 176.25,
         Region::Ring176},
        {"home ladder down: the 176 ring to 154", Connector::HomeLadder, true, {5.0, 176.25, -128.55}, 154.0,
         Region::Deck154},
        {"C3 riser up: 143 to its bridge over 154's rail", Connector::RiserLadder, false, {-6.0, 143.03, -133.85},
         Matron::kRiserBridgeTop, Region::Count},
        {"C3 riser down: its bridge to the 143 plate", Connector::RiserLadder, true, {-6.0, 155.18, -132.85}, 143.03,
         Region::Deck143},
        {"C3 wall ladder up: the duct to 143", Connector::C3WallLadder, false, {-10.5, 137.2, -135.85}, 143.03,
         Region::Deck143},
        {"C3 wall ladder down: 143 to the duct", Connector::C3WallLadder, true, {-10.5, 143.03, -134.85}, 137.2,
         Region::Count},
        {"C3 duct up: the landing onto the duct", Connector::C3Duct, false, {-10.5, 135.5, -144.4}, 137.2,
         Region::Count},
        {"C3 duct down: the duct to the landing", Connector::C3Duct, true, {-10.5, 137.2, -143.4}, 135.5,
         Region::Count},
    }};
    for (const Leg &leg : legs) {
        const Result r = take(leg);
        std::cout << "  matron traverse: " << leg.name << ": " << r.seconds << " s, ends (" << r.x << ", " << r.y
                  << ", " << r.z << "), peak " << r.peak_force << " N, worst step " << r.worst_step
                  << " m, releases " << r.releases << '\n';
        require(r.releases == 0, "matron traverse: no hold lost, no failed climb");
        require(std::abs(r.y - leg.end_y) < 0.08, "matron traverse: standing on the steel at its other end");
        require(r.peak_force <= Matron::kClimbGripMaxN + 0.1, "matron traverse: her hands never past their cap");
        require(r.worst_step < 0.15, "matron traverse: never carried or snapped (a step a tick)");
        if (leg.end_region != Region::Count) {
            require(map.region_at(float(r.x), float(r.y), float(r.z)) == leg.end_region,
                    "matron traverse: in the region the map says");
        }
    }
    std::cout << "PASS scraperx_sim matron traverse: legs=" << legs.size() << '\n';
}
