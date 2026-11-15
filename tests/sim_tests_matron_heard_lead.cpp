// J-127 (#451): a heard lead lands on the floor he really stands on.
// Deck 7's stepped monorail beam (band_stack.cpp) is one region with the
// pipe-rack platform under it: her map's region top is ~79.81 m (the
// platform dominates), the beam's level top stands at 82.00 m. Her levels
// table knows no floor between the 77 m deck and the 88 m deck, so a heard
// lead up there read 77 m — outside region_at's 0.3 m reach and 2.8 m below
// her floor — and the hunt called it elsewhere. On this geometry trunk does
// NOT stand her still for the 9 s lose-track window (that wait was measured
// on the J-105 spiral): the wrong floor projects the lead onto her lines and
// walks her off the platform's north edge to the 77 deck, she climbs back
// and grabs him, then falls off the 0.9 m beam and dies. The fix reads the
// floor under a heard body off her scan, and she walks the beam straight to
// him instead. The start require (within 1 s) passes on trunk too — the
// discriminator here is the route: arrival, alive, corridor, never falling.
// The heard floor decides her direction in the window before sight takes
// over: with it right she turns west and sees him at once; with it wrong
// she walks east and does not see him for 6 s.
//
// He runs the beam so she hears him for real (no debug_matron_hear: that
// sets the lead's floor directly and never runs the perception code under
// test). She starts facing away from the beam, so the lead is heard only:
// she must not see him before she starts walking, or the run proves
// nothing about hearing. The seen run is the control: facing him, she sees
// him at once and walks the same route.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

// The beam's centreline, foot to top: ten 0.30 m steps then the level beam.
const std::vector<Vector3> kBeamWaypoints = {
    {17.75, 79.05, -162.0}, {17.25, 79.35, -162.0}, {16.75, 79.65, -162.0}, {16.25, 79.95, -162.0},
    {15.75, 80.25, -162.0}, {15.25, 80.55, -162.0}, {14.75, 80.85, -162.0}, {14.25, 81.15, -162.0},
    {13.75, 81.45, -162.0}, {13.25, 81.75, -162.0}, {12.0, 82.0, -162.0},  {10.5, 82.0, -162.0},
};

struct BeamRun {
    double heard_s = -1.0;
    double saw_s = -1.0;
    double started_s = -1.0;
    double arrived_s = -1.0;
    bool arrived = false;
    bool alive = true;
    double worst_step = 0.0;
    double lowest_feet = 1.0e9;
    bool in_corridor = true;
};

BeamRun run_beam(Simulation &s, const Vector3 &foot, const double face_yaw, const bool heard_only) {
    constexpr int hz = static_cast<int>(Simulation::kTickRateHz);
    BeamRun r;
    s.debug_place_matron(foot);
    s.debug_face_matron(face_yaw);
    s.debug_launch_player(Vector3{17.75, 79.05 + 0.9, -162.0}, Vector3{0.0, 0.0, 0.0});
    tick(s);
    const auto start = s.matron_state().position;
    auto last = start;
    std::size_t wp = 0;
    int dir = 1;
    int on_him = 0;
    for (int i = 0; i < 90 * hz && !r.arrived; ++i) {
        s.debug_set_matron_stage(1);
        // Him running the beam, end to end and back, so she hears him.
        const auto him = s.snapshot().player_position;
        const Vector3 &w = kBeamWaypoints[wp];
        const double d = std::hypot(w.x - him.x, w.z - him.z);
        const std::size_t next = dir > 0 ? wp + 1 : wp - 1;
        const double d_next = next < kBeamWaypoints.size()
                                  ? std::hypot(kBeamWaypoints[next].x - him.x, kBeamWaypoints[next].z - him.z)
                                  : 1.0e9;
        if ((d < 0.5 || d_next < d) && next < kBeamWaypoints.size()) wp = next;
        if (wp == 0) dir = 1;
        if (wp + 1 == kBeamWaypoints.size()) dir = -1;
        if (d > 1e-6) (void)s.set_move_input((w.x - him.x) / d, (w.z - him.z) / d);
        tick(s);
        const auto m = s.matron_state();
        const double seconds = static_cast<double>(i + 1) / hz;
        if (r.heard_s < 0.0 && m.noise_heard > 0.0) r.heard_s = seconds;
        if (r.saw_s < 0.0 && m.sees_player) r.saw_s = seconds;
        r.worst_step = std::max(r.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z));
        last = m.position;
        if (!m.alive) {
            r.alive = false;
            break;
        }
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        r.lowest_feet = std::min(r.lowest_feet, feet);
        if (m.position.x < 9.0 || m.position.x > 21.0 || m.position.z < -163.5 || m.position.z > -160.0) {
            r.in_corridor = false;
        }
        if (r.started_s < 0.0 && std::hypot(m.position.x - start.x, m.position.z - start.z) > 0.2) {
            r.started_s = seconds;
        }
        // Near him, on his floor, for a second.
        const auto now = s.snapshot().player_position;
        const double his_feet = now.y - 0.9;
        const bool there = std::hypot(m.position.x - now.x, m.position.z - now.z) < 1.5 &&
                           std::abs(feet - his_feet) < 0.5 && !m.climbing;
        on_him = there ? on_him + 1 : 0;
        if (on_him >= hz) {
            r.arrived = true;
            r.arrived_s = seconds;
        }
        if (std::getenv("SCRAPERX_MATRON_TRACE") != nullptr && i % (5 * hz) == 0) {
            std::cout << "    t " << seconds << " her (" << m.position.x << ", " << feet << ", " << m.position.z
                      << ") him (" << now.x << ", " << his_feet << ", " << now.z << ") heard " << m.noise_heard
                      << " sees " << m.sees_player << " state " << int(m.state) << '\n';
        }
    }
    (void)heard_only;
    return r;
}

void print_run(const char *name, const BeamRun &r) {
    std::cout << "  matron heard lead " << name << ": heard " << r.heard_s << " s, saw " << r.saw_s
              << " s, started " << r.started_s << " s, arrived " << r.arrived_s << " s, alive " << r.alive
              << ", lowest feet " << r.lowest_feet << " m, worst step " << r.worst_step << " m, corridor "
              << r.in_corridor << '\n';
}

}  // namespace

void run_matron_heard_lead() {
    constexpr double kPi = 3.14159265358979323846;
    // Her foot on the pipe-rack platform under the beam, his run on the beam.
    const Vector3 foot{20.0, 78.75, -161.5};
    const Vector3 beam_top{10.5, 82.0, -162.0};
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron heard lead: settle");

    // The map this test stands on: platform and beam top in one region, the
    // top more than 2 m above that region's area-weighted top.
    const scraperx::sim::traversal::AffordanceMap &map = s.matron_affordances();
    const int foot_region = map.region_at(JPH::Vec3(float(foot.x), float(foot.y), float(foot.z)));
    const int beam_region = map.region_at(JPH::Vec3(float(beam_top.x), float(beam_top.y), float(beam_top.z)));
    require(foot_region >= 0 && foot_region == beam_region,
            "matron heard lead: the platform and the beam top are one region of her map");
    const float region_top = map.regions()[static_cast<std::size_t>(foot_region)].top;
    std::cout << "  matron heard lead: region " << foot_region << ", area-weighted top " << region_top
              << " m, beam top " << beam_top.y << " m (" << beam_top.y - region_top << " m above)\n";
    require(beam_top.y - region_top > 2.0, "matron heard lead: the beam top is over 2 m above the region's top");

    // Heard only: she faces east, away from the beam, and never sees him
    // before she starts. She hears him running the beam and walks the lead
    // within 1 s; with the floor right she turns west to it at once and
    // reaches him on the beam, alive and on the steel.
    const BeamRun heard = run_beam(s, foot, kPi / 2.0, true);
    print_run("heard only", heard);
    require(heard.heard_s >= 0.0, "matron heard lead: she hears him running the beam");
    require(heard.saw_s < 0.0 || heard.saw_s >= heard.started_s,
            "matron heard lead: she does not see him before she starts walking");
    require(heard.started_s >= 0.0 && heard.started_s <= 1.0,
            "matron heard lead: she starts walking the heard lead within 1 s");
    require(heard.arrived, "matron heard lead: she reaches him on the beam");
    require(heard.alive, "matron heard lead: she is alive");
    require(heard.lowest_feet > 78.0, "matron heard lead: she never falls off the deck");
    require(heard.worst_step <= 0.35, "matron heard lead: her worst step is a step, not a teleport");
    require(heard.in_corridor, "matron heard lead: she stays on the platform and beam");

    // Seen: the same setup, her facing the beam. She sees him at once and
    // walks the same route; the heard run must match it.
    Simulation s2(InitialSpawn::Deck154);
    s2.place_matron_home(154.0F);
    require(s2.advance_frame(1.0).accepted, "matron heard lead: settle the seen world");
    const BeamRun seen = run_beam(s2, foot, -kPi / 2.0, false);
    print_run("seen", seen);
    require(seen.saw_s >= 0.0 && seen.saw_s <= 1.0, "matron heard lead: seen, she sees him at once");
    require(seen.started_s >= 0.0 && seen.started_s <= 2.0, "matron heard lead: seen, she starts at once");
    require(seen.arrived, "matron heard lead: seen, she reaches him on the beam");
    require(seen.alive, "matron heard lead: seen, she is alive");
    require(seen.in_corridor, "matron heard lead: seen, she stays on the platform and beam");
    require(heard.arrived_s <= seen.arrived_s * 2.0 + 5.0,
            "matron heard lead: the heard run reaches him on the seen run's route and pace");
    std::cout << "PASS scraperx_sim matron heard lead: heard only, she starts in " << heard.started_s
              << " s and reaches him in " << heard.arrived_s << " s (seen: " << seen.arrived_s << " s)\n";
}

SCRAPERX_GROUP("matron_heard_lead", 1100, run_matron_heard_lead);
SCRAPERX_SELECT("matron-heard-lead", run_matron_heard_lead);
