// The Matron after the climber across decks, up and down (sim/matron_travel.cpp
// and matron_pipe.cpp): she knows where he is only from what she hears (a
// sound every few seconds where he stands, as her ears would report it), and
// gets there by the ways of her map, by her own hands and feet: the corner
// pipe, the home ladder. Each run ends with her standing on his deck's steel,
// alive, never carried or snapped there.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_routes.hpp"

namespace {
namespace routes = scraperx::sim::routes;
using routes::Region;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

struct Chase {
    bool arrived = false;
    double seconds = 0.0;
    double worst_step = 0.0;
    std::uint32_t pipe_links = 0, climbs = 0, route_edges = 0, route_failures = 0;
    bool alive = true;
};

// Her at `from` (feet), dangerous; the climber standing at `prey` (feet),
// heard there every kHearEveryS. Until she stands on his deck, or `seconds`.
Chase chase(const Vector3 from, const Vector3 prey, const Region goal, const double seconds) {
    constexpr double kHearEveryS = 8.0;
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron pursuit: settle");
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(from);
    tick(s);   // her view of herself is refreshed in the step: from here, where she was put
    const routes::Map map;
    Chase c;
    auto last = s.matron_state().position;
    const auto climbs0 = s.matron_state().climbs;
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    int on_goal = 0;
    for (int i = 0; i < static_cast<int>(seconds) * hz; ++i) {
        s.debug_set_matron_stage(1);
        if (i % (static_cast<int>(kHearEveryS) * hz) == 0) {
            s.debug_matron_hear(prey);
        }
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const auto m = s.matron_state();
        c.worst_step = std::max(c.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                         m.position.z - last.z));
        last = m.position;
        if (!m.alive) {
            c.alive = false;
            break;
        }
        if (std::getenv("SCRAPERX_PURSUIT_TRACE") != nullptr && i % (5 * hz) == 0) {
            std::cout << "    t " << i / hz << " at (" << m.position.x << ", " << m.position.y << ", " << m.position.z
                      << ") floor " << m.floor_y << " state " << int(m.state) << " pipe " << m.on_pipe
                      << " climbing " << m.climbing << " edges " << m.route_edges << " fails " << m.route_failures
                      << '\n';
        }
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        const bool there = !m.on_pipe && !m.climbing &&
                           map.region_at(float(m.position.x), float(feet), float(m.position.z)) == goal;
        on_goal = there ? on_goal + 1 : 0;
        if (on_goal >= hz) {   // a second on its steel
            c.arrived = true;
            c.seconds = static_cast<double>(i) / hz;
            break;
        }
    }
    const auto m = s.matron_state();
    c.pipe_links = m.pipe_links;
    c.climbs = m.climbs - climbs0;
    c.route_edges = m.route_edges;
    c.route_failures = m.route_failures;
    return c;
}

void report(const char *name, const Chase &c) {
    std::cout << "  matron pursuit: " << name << ": arrived " << c.arrived << " in " << c.seconds << " s, pipe "
              << c.pipe_links << " ways, climbs " << c.climbs << ", map ways " << c.route_edges << " (failed "
              << c.route_failures << "), worst step " << c.worst_step << " m, alive " << c.alive << '\n';
}

} // namespace

void run_matron_pursuit() {
    // Up: from 132 by the corner pipe to 143 and 154, then the home ladder.
    {
        const Chase c = chase(Vector3{14.85, 132.0, -131.5}, Vector3{5.0, 176.25, -127.5}, Region::Ring176, 300.0);
        report("132 up to the 176 ring", c);
        require(c.alive && c.arrived, "matron pursuit: up three decks to the climber, on her own hands and feet");
        require(c.pipe_links + c.climbs >= 3, "matron pursuit: by three ways, one a storey");
        require(c.worst_step < 0.2, "matron pursuit: never carried or snapped (a step a tick)");
    }
    // Down: from the ring by the home ladder to 154, then the pipe.
    {
        const Chase c = chase(Vector3{5.0, 176.25, -127.5}, Vector3{14.85, 132.0, -131.5}, Region::Deck132, 300.0);
        report("the 176 ring down to 132", c);
        require(c.alive && c.arrived, "matron pursuit: down three decks to the climber");
        require(c.pipe_links + c.climbs >= 3, "matron pursuit: by three ways down, one a storey");
        require(c.worst_step < 0.2, "matron pursuit: never carried or snapped (a step a tick)");
    }
    std::cout << "PASS scraperx_sim matron pursuit\n";
}
