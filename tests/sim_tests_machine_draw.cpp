// The tower's loud machines draw her (issue #391, gap 10): a far
// counterweight slam draws her to it; a quiet machine at the same range
// does not. The slam is Stage A's skip coming down 22 m onto its buffer;
// the quiet machine is the same stage running, before the stop.
#include "sim/impacts.hpp"
#include "sim/matron.hpp"
#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

using scraperx::sim::ImpactLedger;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;

namespace {

// The skip's centre, where it lands: (-10.5, -135). She stands on the
// south band of the 154 m deck, 36.5 m from it: past the running machines'
// reach (a run carries at most 0.35 * 55 = 19.25 m) and inside a hard
// slam's reach. The far spot MUST sit on the south band (z -176..-167):
// the middle of the deck (x -17..17, z -167..-133) is the open well shaft,
// and a Matron placed there falls down it and dies before the pull.
constexpr double kSlamX = -10.5;
constexpr double kSlamZ = -135.0;
constexpr double kFarX = -10.5;
constexpr double kFarZ = -171.5;
constexpr double kDeckY = 154.0;

double dist_to_slam(const Simulation &simulation) {
    const auto at = simulation.matron_state().position;
    return std::hypot(at.x - kSlamX, at.z - kSlamZ);
}

// The skip's speed: the ride ends when it has stopped, not when its centre
// crosses a height — the centre passes 167 m while it is still doing 2 m/s,
// and the slam is the stop, about a second later. The 0.1 m/s bar is the
// ledger's own rest bar (ImpactLedger::kRunMinMps).
double kit_speed(const Simulation &simulation, const std::uint64_t entity) {
    const auto v = simulation.kit_body_velocity(simulation.kit_body_index(entity));
    return std::hypot(v.x, std::hypot(v.y, v.z));
}

// Board and rig Stage A, then put her at the far spot and trip the catch.
// She stands far only for the pull itself, so no patrol drift moves the
// starting range before the ride.
bool trip_stage_a(Simulation &simulation) {
    if (!board_well_a(simulation) || !rig_well_a(simulation)) {
        return false;
    }
    simulation.debug_place_matron(scraperx::sim::Vector3{static_cast<float>(kFarX),
                                                         static_cast<float>(kDeckY),
                                                         static_cast<float>(kFarZ)});
    (void)simulation.advance_frame(0.5);
    return pull_well_a(simulation, 0.5, 3.0);
}

void counterweight_slam_draws_her() {
    Simulation s(scraperx::sim::InitialSpawn::Deck154);
    (void)s.advance_frame(1.0);
    s.place_matron_home(154.0F);
    require(trip_stage_a(s), "machine draw: Stage A trips with her placed far");
    const auto placed = s.matron_state();
    require(placed.alive, "machine draw: she is alive at the far spot");
    require(std::abs(placed.position.y - kDeckY) < 2.0,
            "machine draw: the far spot is solid deck under her feet");
    const double d0 = dist_to_slam(s);
    std::cout << "  machine draw: slam: she starts " << d0 << " m from the skip\n";
    require(d0 > 25.0, "machine draw: she starts far from the well");

    // The ride, tick by tick: the slam is a one-tick sound, so the ear is
    // read every tick, not sampled. The skip's hit is read from the ledger,
    // which keeps it. The slam is the stop, and a leveled stop finishes
    // after the skip reaches its buffer, so the ride ends when the skip has
    // stopped (low and slow), and the ear is kept open half a second past
    // that: breaking at the buffer's height would cut the sound off before
    // it is read.
    const std::uint64_t seen = s.last_impact_sequence();
    double peak_heard = 0.0;
    double peak_hit_j = 0.0;
    bool rode = false;
    int settle = 0;
    for (int tick = 0; tick < 40 * 90; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        peak_heard = std::max(peak_heard, s.matron_state().noise_heard);
        for (const auto &hit : s.impacts()) {
            if (hit.sequence > seen && hit.entity == Simulation::kWellASkipEntityId) {
                peak_hit_j = std::max(peak_hit_j, hit.energy_j);
            }
        }
        if (!rode && kit_y(s, Simulation::kWellASkipEntityId) <= 167.0 &&
            kit_speed(s, Simulation::kWellASkipEntityId) < 0.1) {
            rode = true;
            settle = 45;
        } else if (rode && --settle <= 0) {
            break;
        }
    }
    require(rode, "machine draw: the skip comes down to its buffer");
    std::cout << "  machine draw: slam: skip hit " << peak_hit_j << " J (loudness "
              << ImpactLedger::loudness(static_cast<float>(peak_hit_j)) << "), heard " << peak_heard << "\n";
    require(peak_heard > 0.0, "machine draw: she hears the counterweight slam");
    require(peak_hit_j > 0.0, "machine draw: the slam is a hit in the ledger");

    // She comes: 90 s after the stop, her closest approach to the skip.
    double closest = d0;
    double dend = d0;
    for (int step = 0; step < 180; ++step) {
        (void)s.advance_frame(0.5);
        dend = dist_to_slam(s);
        closest = std::min(closest, dend);
    }
    const auto end = s.matron_state();
    std::cout << "  machine draw: slam: she closes to " << closest << " m of the skip, ends " << dend
              << " m away, state " << static_cast<int>(end.state) << "\n";
    require(end.alive, "machine draw: she is alive after the slam");
    // She comes to the nearest line she can walk, the north band's centre,
    // 6.5 m from a sound over the open shaft: she never peers at it, because
    // Peering stands kVantageM (6.0 m) off a sound she can reach, and this
    // one she cannot reach. The draw is the 30 m approach, not the last
    // half metre: measured closest 6.34 m.
    require(closest < 7.0, "machine draw: the far counterweight slam draws her to it");
}

void quiet_machine_does_not_draw_her() {
    Simulation s(scraperx::sim::InitialSpawn::Deck154);
    (void)s.advance_frame(1.0);
    s.place_matron_home(154.0F);
    require(trip_stage_a(s), "machine draw: Stage A trips with her placed far");
    const auto placed = s.matron_state();
    require(placed.alive, "machine draw: she is alive at the far spot");
    const double d0 = dist_to_slam(s);
    std::cout << "  machine draw: quiet: she starts " << d0 << " m from the skip\n";
    require(d0 > 25.0, "machine draw: she starts far from the well");

    // The run phase only: while the skip is still falling, before it reaches
    // its buffer. A running machine at this range is quiet to her.
    double peak_heard = 0.0;
    bool ran = false;
    for (int tick = 0; tick < 20 * 90; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        peak_heard = std::max(peak_heard, s.matron_state().noise_heard);
        if (kit_y(s, Simulation::kWellASkipEntityId) <= 172.0) {
            ran = true;
            break;
        }
    }
    require(ran, "machine draw: the skip runs down before the stop");
    const double dend = dist_to_slam(s);
    std::cout << "  machine draw: quiet: heard " << peak_heard << " while it ran, she ends " << dend
              << " m from the skip\n";
    require(s.matron_state().alive, "machine draw: she is alive after the quiet run");
    require(peak_heard == 0.0, "machine draw: the quiet machine at this range is not heard");
    // She heard nothing, so any closing is her patrol, not a draw: over the
    // run she shuffles 3.19 m toward the skip on her patrol. The draw this
    // bar must catch is the slam's 30 m approach, so the bar sits well
    // clear of the patrol drift and well short of a draw.
    require(d0 - dend < 5.0, "machine draw: the quiet machine at the same range does not draw her");
}

} // namespace

void run_machine_draw() {
    counterweight_slam_draws_her();
    quiet_machine_does_not_draw_her();
}

SCRAPERX_GROUP("machine_draw", 1075, run_machine_draw);
SCRAPERX_SLICE("machine-draw", run_machine_draw);
