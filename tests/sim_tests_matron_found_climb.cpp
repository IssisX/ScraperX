// The Matron finds her own way between levels on the tower (the climbing lab,
// src/sim/matron_found_climb.cpp): no table of ladders. She hears the climber
// on another level, finds the bars round her in the world, walks to a column
// that runs toward that level, and climbs it by routecraft's hands (finite
// force, gravity on). Gravity stays 1 on her every tick: the old authored
// climb (gravity off) never runs. Up and down are separate runs.

#include "sim_tests.hpp"

#include "sim/matron_routes.hpp"

#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

struct Leg {
    bool arrived = false;
    bool alive = true;
    bool gravity_kept = true;
    bool her_hands = false;   // routecraft seen moving her (the old climb sets gravity off instead)
    double seconds = 0.0;
    double worst_step = 0.0;
    std::uint32_t climbs = 0;
    bool let_go = false;       // her hands opened on a climb (as a hold giving way opens them)
    double fell_mps = 0.0;     // her fastest fall after
    double let_go_s = 0.0;
};

// Her at `from` (feet), dangerous; the climber heard at `prey` (feet) every
// 8 s. Until she stands a second on the prey's floor.
Leg leg(const Vector3 from, const Vector3 prey, const double seconds, const double let_go_at = 1.0e9) {
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron found climb: settle");
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.launch_matron_for_test(Vector3{from.x, from.y + 1.05, from.z});
    // Her authored map's ways all ruled out: the way she takes is one she finds.
    s.debug_matron_only_way(scraperx::sim::routes::kMaxEdges);
    s.debug_set_matron_stage(1);
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    Leg out;
    auto last = s.matron_state().position;
    const auto climbs0 = s.matron_state().climbs;
    int on_goal = 0;
    for (int i = 0; i < static_cast<int>(seconds) * hz; ++i) {
        if (i % (8 * hz) == 0) {
            s.debug_matron_hear(prey);
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        out.worst_step = std::max(out.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                             m.position.z - last.z));
        const double vy = (m.position.y - last.y) * hz;
        last = m.position;
        if (!out.let_go && m.climbing && m.position.y - 1.05 >= let_go_at) {
            s.debug_matron_let_go();
            out.let_go = true;
            out.let_go_s = static_cast<double>(i) / hz;
        } else if (out.let_go) {
            out.fell_mps = std::max(out.fell_mps, -vy);
        }
        out.gravity_kept = out.gravity_kept && s.debug_matron_gravity_factor() == 1.0;
        out.her_hands = out.her_hands || m.climbing;
        if (!m.alive) {
            out.alive = false;
            break;
        }
        const bool there = std::abs(m.position.y - 1.05 - prey.y) < 0.35 && !m.climbing;
        on_goal = there ? on_goal + 1 : 0;
        if (on_goal >= hz) {
            out.arrived = true;
            out.seconds = static_cast<double>(i) / hz;
            break;
        }
    }
    out.climbs = s.matron_state().climbs - climbs0;
    return out;
}

void report(const char *name, const Leg &l) {
    std::cout << "  matron found climb: " << name << ": arrived " << l.arrived << " in " << l.seconds << " s, climbs "
              << l.climbs << ", worst step " << l.worst_step << " m, gravity kept " << l.gravity_kept
              << ", by her hands " << l.her_hands << ", alive " << l.alive << '\n';
}

} // namespace

void run_matron_found_climb() {
    {
        const Leg l = leg(Vector3{1.0, 154.0, -129.0}, Vector3{5.0, 176.25, -127.5}, 120.0);
        report("154 up to the 176.25 ring", l);
        require(l.alive && l.arrived, "matron found climb: up to the climber's level, a way she found herself");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on (not the old climb)");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{5.0, 176.25, -127.5}, Vector3{1.0, 154.0, -129.0}, 120.0);
        report("the 176.25 ring down to 154", l);
        require(l.alive && l.arrived, "matron found climb: down to the climber's level, a way she found herself");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on (not the old climb)");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    // Above her map's last deck: the standpipe (a 0.10 m pole: the traversal
    // Climber's friction grip) and the scaffold panel (bars).
    {
        const Leg l = leg(Vector3{7.05, 176.25, -128.4}, Vector3{8.0, 198.25, -129.3}, 240.0);
        report("176.25 up to 198.25 (the standpipe)", l);
        require(l.alive && l.arrived, "matron found climb: up the standpipe to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{8.0, 198.25, -129.3}, Vector3{7.05, 176.25, -128.4}, 240.0);
        report("198.25 down to 176.25 (the standpipe)", l);
        require(l.alive && l.arrived, "matron found climb: down the standpipe to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{8.0, 198.25, -129.3}, Vector3{11.0, 220.25, -129.0}, 240.0);
        report("198.25 up to 220.25 (the scaffold panel)", l);
        require(l.alive && l.arrived, "matron found climb: up the panel to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{11.0, 220.25, -129.0}, Vector3{8.0, 198.25, -129.3}, 240.0);
        report("220.25 down to 198.25 (the scaffold panel)", l);
        require(l.alive && l.arrived, "matron found climb: down the panel to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    std::cout << "PASS scraperx_sim matron found climb\n";
}
