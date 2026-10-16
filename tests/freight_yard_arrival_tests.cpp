#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace scraperx::sim;
namespace {
void require(bool ok, const char *why) {
    if (!ok) { std::cerr << "FAIL ARRIVAL " << why << std::endl; std::exit(1); }
}
void tick(Simulation &s, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "tick accepted");
        require(s.snapshot().death_count == 0, "no death on arrival path");
    }
}
bool walk(Simulation &s, double x, double z, unsigned limit, double cap) {
    for (unsigned i = 0; i < limit; ++i) {
        const auto p = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        if (std::hypot(x - p.x, z - p.z) < .16) return true;
        require(s.set_move_input(std::clamp(1.2 * (x - p.x) - .45 * v.x, -cap, cap),
                                 std::clamp(1.2 * (z - p.z) - .45 * v.z, -cap, cap)), "ordinary walk input");
        tick(s);
    }
    const auto p = s.snapshot().player_position;
    std::cerr << "WALK_END p=" << p.x << ',' << p.y << ',' << p.z << std::endl;
    return false;
}
}
int main() {
    // Same enum selected by the shipping bridge constructor. No debug placement.
    Simulation s(InitialSpawn::FreightYardArrival);
    const auto start = s.snapshot().player_position;
    require(std::abs(start.x + 94) < .001 && std::abs(start.z + 44) < .001, "real arrival selected");
    tick(s, 45);
    require(s.snapshot().player_grounded, "actual grade contact");
    require(s.forklift_state().available && s.yard_crane_state().available, "both real machines exist");
    require(!s.forklift_state().controls_reachable, "arrival is outside operator station");
    const auto poses = s.snapshot().step_up_pose_count;
    require(walk(s, -98, -57.65, 1800, .55), "clear walk from arrival to truck doorway");
    require(s.set_facing(-1, 0), "face open truck side");
    bool boarded = false;
    for (unsigned i = 0; i < 540; ++i) {
        const auto truck = s.kit_body_position(s.kit_body_index(2440));
        const auto p = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        require(s.set_move_input(std::clamp(1.2 * (truck.x - p.x) - .45 * v.x, -.3, .3),
                                 std::clamp(1.2 * (truck.z - 1.65 - p.z) - .45 * v.z, -.15, .15)), "walk onto truck floor");
        tick(s);
        if (s.forklift_state().controls_reachable) { boarded = true; break; }
    }
    require(boarded, "arrival reaches actual forklift controls");
    require(s.set_move_input(0, 0), "stand in cab");
    tick(s, 90);
    require(s.snapshot().support_entity_id == 2440 && s.forklift_state().controls_reachable, "retains real cab footing");
    const auto lift = s.forklift_state().lift_position_m;
    require(s.set_forklift_input(0, 0, .2, 0, 1), "operate real lift");
    tick(s, 90);
    require(s.set_forklift_input(0, 0, 0, 0, 1), "neutral lift");
    require(s.forklift_state().lift_position_m > lift + .05, "hydraulics respond after ordinary boarding");
    require(walk(s, -98, -57.65, 540, .3), "ordinary exit to grade");
    require(s.set_move_input(0, 0), "stop outside"); tick(s, 45);
    require(s.snapshot().player_grounded && !s.forklift_state().controls_reachable, "actual grade exit");
    require(s.snapshot().step_up_pose_count == poses, "whole arrival uses no pose steps");
    std::cout << "PASS ARRIVAL normal spawn -> grade walk -> real cab -> finite lift -> grade exit ticks=" << s.snapshot().tick_index << '\n';
}
