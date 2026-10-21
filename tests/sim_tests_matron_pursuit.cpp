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
    bool let_go = false;          // her hands opened on the climb (let_go_at)
    double fell_mps = 0.0;        // the fastest she fell after it
    double retook_after_s = -1.0; // from her hands opening to her next climb or pipe
    std::uint32_t knockdowns = 0;
    bool arrived = false;
    double seconds = 0.0;
    double worst_step = 0.0;
    std::uint32_t pipe_links = 0, climbs = 0, route_edges = 0, route_failures = 0;
    bool alive = true;
};

// Her at `from` (feet), dangerous; the climber standing at `prey` (feet),
// heard there every kHearEveryS. Until she stands on his deck, or `seconds`.
Chase chase(const Vector3 from, const Vector3 prey, const Region goal, const double seconds,
            const double let_go_at = 1.0e9, const std::size_t only_way = routes::kMaxEdges) {
    constexpr double kHearEveryS = 8.0;
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron pursuit: settle");
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(from);
    if (only_way < routes::kMaxEdges) {
        s.debug_matron_only_way(only_way);
    }
    tick(s);   // her view of herself is refreshed in the step: from here, where she was put
    const routes::Map map;
    Chase c;
    auto last = s.matron_state().position;
    const auto climbs0 = s.matron_state().climbs;
    const auto knockdowns0 = s.matron_state().knockdowns;
    int let_go_tick = -1;
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
        const double vy = (m.position.y - last.y) * hz;
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
        if (!c.let_go && m.climbing && feet >= let_go_at) {
            s.debug_matron_let_go();
            c.let_go = true;
            let_go_tick = i;
        } else if (c.let_go) {
            c.fell_mps = std::max(c.fell_mps, -vy);
            if (c.retook_after_s < 0.0 && i > let_go_tick + hz && (m.climbing || m.on_pipe)) {
                c.retook_after_s = static_cast<double>(i - let_go_tick) / hz;
            }
        }
        // Alone on one way, she is there when it is done to its last step.
        const bool there = !m.on_pipe && !m.climbing &&
                           map.region_at(float(m.position.x), float(feet), float(m.position.z)) == goal &&
                           (only_way >= routes::kMaxEdges || m.route_edges >= 1);
        on_goal = there ? on_goal + 1 : 0;
        if (on_goal >= hz) {   // a second on its steel
            c.arrived = true;
            c.seconds = static_cast<double>(i) / hz;
            break;
        }
    }
    const auto m = s.matron_state();
    c.knockdowns = m.knockdowns - knockdowns0;
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
    // A way that fails under her: up the home ladder after him, her hands
    // open 4.5 m up (as a hold that gave way would open them). She falls for
    // real and lands hard on 154, alive, down a while; her map leaves the
    // ladder alone its block time; and on what she still hears she takes it
    // again once it may be tried, and reaches him.
    {
        const Chase c = chase(Vector3{3.9, 154.0, -129.0}, Vector3{5.0, 176.25, -127.5}, Region::Ring176, 300.0, 158.5);
        report("154 up the home ladder, her hands opening on it", c);
        std::cout << "    fell at " << c.fell_mps << " m/s, knockdowns " << c.knockdowns << ", took it again after "
                  << c.retook_after_s << " s\n";
        require(c.let_go && c.fell_mps >= Matron::kHardLandingMps && c.alive,
                "matron pursuit: her hands open, she falls for real and lives");
        require(c.knockdowns >= 1, "matron pursuit: the landing puts her down");
        require(c.route_failures >= 1, "matron pursuit: the way that failed is on her map");
        require(c.retook_after_s >= scraperx::sim::routes::kFailedBlockS - 1.0,
                "matron pursuit: she leaves the failed way alone its block time");
        require(c.arrived, "matron pursuit: then takes it again and reaches him");
        require(c.worst_step < 0.2, "matron pursuit: never carried or snapped (a step a tick)");
    }
    std::cout << "PASS scraperx_sim matron pursuit\n";
}

// Every way her map plans over, end to end and alone (every other ruled
// out): from where it starts on its deck, after the climber standing where it
// leaves her, until she stands a second on that deck's steel. Its walks, its
// climbs and its top-outs are all her own body in the world.
void run_matron_ways() {
    const routes::Map map;
    int taken = 0;
    for (std::size_t i = 0; i < map.edge_count(); ++i) {
        const routes::Edge &e = map.edge(i);
        if (e.status != routes::Status::Qualified) {
            continue;
        }
        if (const char *one = std::getenv("SCRAPERX_WAY"); one != nullptr && std::atoi(one) != int(i)) {
            continue;
        }
        const double from_y = map.region(e.from).top_y;
        const double to_y = map.region(e.to).top_y;
        const Chase c = chase(Vector3{e.entry_x, from_y, e.entry_z}, Vector3{e.exit_x, to_y, e.exit_z}, e.to, 240.0,
                              1.0e9, i);
        std::cout << "  matron way " << map.region(e.from).name << " to " << map.region(e.to).name << " ("
                  << int(e.kind) << "): arrived " << c.arrived << " in " << c.seconds << " s, map ways "
                  << c.route_edges << " (failed " << c.route_failures << "), worst step " << c.worst_step
                  << " m, alive " << c.alive << '\n';
        require(c.alive && c.arrived, "matron ways: along it, standing on its far deck");
        require(c.route_failures == 0 && c.route_edges >= 1, "matron ways: every step of it, without a failure");
        require(c.worst_step < 0.2, "matron ways: never carried or snapped (a step a tick)");
        ++taken;
    }
    if (std::getenv("SCRAPERX_WAY") == nullptr) {
        require(taken >= 10, "matron ways: every way she plans over");
    }
    std::cout << "PASS scraperx_sim matron ways: " << taken << " ways\n";
}
