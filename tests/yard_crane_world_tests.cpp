#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace scraperx::sim;
namespace {
void require(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL public yard crane: " << message << std::endl; std::exit(1); }
}
void tick(Simulation &s, unsigned count = 1) {
    while (count--) require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick");
}
}
int main() {
    Simulation s(InitialSpawn::ExteriorGrade, WorldContent::Slingshot);
    require(s.debug_restart_at({-68.5, .9, -150}), "staged ordinary ground approach");
    tick(s, 90);
    const auto before = s.yard_crane_state();
    require(before.available && !before.controls_reachable && !before.rigging_reachable,
            "nearby grade cannot authorize cab or distant rigging");
    require(!s.request_yard_crane_attach() && !s.request_yard_crane_detach(), "no remote rigging");
    require(s.set_yard_crane_input(0, 0, 1), "normalized request accepted");
    tick(s, 30);
    require(std::abs(s.yard_crane_state().hoist_length_m - before.hoist_length_m) < .002,
            "unauthorized input cannot retract native hoist");
    require(s.set_yard_crane_input(0, 0, 0), "clear demand");
    require(s.set_facing(1, 0), "face actual booth");
    for (unsigned i = 0; i < 540 && !s.yard_crane_state().controls_reachable; ++i) {
        const auto p = s.snapshot();
        const double drive = std::clamp(1.2 * (-66.5 - p.player_position.x) -
            .45 * p.player_linear_velocity.x, -.3, .3);
        require(s.set_move_input(drive, 0), "ordinary board input");
        tick(s);
        require(p.traversal_hand_constraint_count == 0 && p.player_gravity_factor == 1 &&
                p.step_up_pose_count == 0, "walking entry keeps physical authority");
    }
    require(s.set_move_input(0, 0), "stop on actual booth floor");
    tick(s, 90);
    require(s.yard_crane_state().controls_reachable && s.snapshot().support_entity_id == 1450,
            "real booth floor authorizes controls");
    const auto ready = s.yard_crane_state();
    require(s.set_yard_crane_input(0, 0, 1), "hoist request");
    tick(s, 90);
    const auto raised = s.yard_crane_state();
    require(raised.hoist_length_m < ready.hoist_length_m - .20 &&
            raised.hoist_source_used_j > ready.hoist_source_used_j && raised.hoist_work_j > ready.hoist_work_j,
            "real empty spreader rises with finite work from actual cab");
    require(s.set_yard_crane_input(0, 0, 0), "neutral hold");
    const auto held = s.yard_crane_state(); tick(s, 90);
    require(s.yard_crane_state().hoist_source_used_j == held.hoist_source_used_j,
            "neutral hold adds no source work");
    require(!s.set_yard_crane_input(0, 2, 0), "invalid request rejected and neutralized");
    require(!s.request_yard_crane_attach(), "cab cannot attach distant lifting eyes");
    require(s.restart_checkpoint(), "native checkpoint restore"); tick(s, 30);
    require(s.yard_crane_state().available && s.yard_crane_state().attached_mask == 0,
            "checkpoint restores native crane inventory");
    std::cout << "PASS public yard crane ordinary cab entry finite hoist neutral no-remote-rig and checkpoint; loaded dock transfer separate\n";
}
