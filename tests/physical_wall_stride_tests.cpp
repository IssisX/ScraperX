#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool condition, const char *message) {
    if (condition) return;
    std::cerr << "WALL_STRIDE FAIL " << message << '\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation &sim, int count = 1) {
    for (int i = 0; i < count; ++i) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "public fixed step rejected");
        const auto s = sim.snapshot();
        require(s.player_gravity_factor == 1 && s.death_count == 0 &&
                s.traversal_hand_constraint_count == 0,
                "wall movement changed gravity, fabricated a grip or lost the player");
    }
}
void stage(Simulation &sim, double z = -38.45, double height = .9, double x = -46) {
    // The opening workshop already has a16m-long,3.8m-high south wall.
    // Its visible wall and actual native collision share z=-38.08.
    require(sim.debug_restart_at({x, height, z}), "real workshop-side staging rejected");
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false) &&
            sim.set_crouch_input(false) && sim.set_facing(1, 0), "neutral approach rejected");
    if (height == .9) {
        tick(sim, 30);
        require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,
                "approach has no actual grade footing");
    }
}
Snapshot sprint(Simulation &sim) {
    require(sim.set_move_input(1, 0) && sim.set_sprint_input(true), "sprint input rejected");
    for (int i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        require(s.wall_stride_count == 0, "ground running created a wall step");
        if (s.player_sprinting && s.player_grounded && s.player_linear_velocity.x > 6.7) return s;
    }
    require(false, "ordinary sprint did not earn approach momentum");
    return {};
}
Snapshot approach(Simulation &sim) {
    stage(sim);
    const auto running = sprint(sim);
    require(sim.set_move_input(.9968, .08) && sim.request_jump(), "sprint-jump input rejected");
    tick(sim);
    require(!sim.snapshot().player_grounded && sim.snapshot().player_linear_velocity.y > 4 &&
            sim.snapshot().landing_jump_work_j > running.landing_jump_work_j,
            "approach did not perform a real paid takeoff");
    for (int i = 0; i < 125; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        if (i % 15 == 0)
            std::cout << "approach tick=" << s.tick_index << " pos=" << s.player_position.x
                << ',' << s.player_position.y << ',' << s.player_position.z
                << " velocity=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y
                << ',' << s.player_linear_velocity.z << " count=" << s.wall_stride_count
                << " active=" << s.wall_stride_active << std::endl;
        if (s.wall_stride_active && s.foot_push_last_load_n > 0) {
            require(s.wall_stride_count == 1 && !s.wall_stride_pushing_off &&
                    s.foot_push_support_entity_id == Simulation::kWorldSolidEntityId &&
                    s.foot_push_source_contact_tick > running.tick_index &&
                    s.wall_stride_normal.z < -.9 && std::abs(s.wall_stride_normal.y) < .2,
                    "step lost actual wall identity, contact age or its native phase");
            return s;
        }
    }
    require(false, "committed public sprint-jump never reached a loaded automatic wall step");
    return {};
}
void limits(const Snapshot &s) {
    require(s.foot_push_peak_load_n <= 3500.01 && s.foot_push_command_work_bound_j <= 1285.62501 &&
            s.foot_push_stroke_m <= .300001 && s.foot_push_elapsed_seconds <= .250001,
            "wall phases exceeded existing finite foot limits");
}
Snapshot land(Simulation &sim, const Snapshot &step) {
    bool landed = false;
    for (int i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        limits(s);
        require(s.wall_stride_count == step.wall_stride_count,
                "held input refilled the automatic wall step");
        if (s.player_grounded && s.support_entity_id == 1) { landed = true; break; }
    }
    const auto s = sim.snapshot();
    require(landed && !s.wall_stride_active && !s.foot_push_active &&
            s.player_position.x > step.player_position.x + 1 && s.player_linear_velocity.x > 4,
            "wall departure failed to reach real grade with retained onward momentum");
    require(sim.set_facing(0, -1) && sim.set_move_input(0, -1) && sim.set_sprint_input(false),
            "onward run input rejected");
    tick(sim, 45);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1 &&
            sim.snapshot().player_position.z < s.player_position.z - .3,
            "receiving footing failed to hand back ordinary movement");
    return s;
}
void flow(bool push_off) {
    Simulation sim;
    const auto step = approach(sim);
    limits(step);
    if (push_off) {
        require(sim.set_move_input(0, 0) && sim.set_sprint_input(false) && sim.request_jump(),
                "timed wall Jump rejected");
        tick(sim);
        const auto switched = sim.snapshot();
        require(switched.wall_stride_active && switched.wall_stride_pushing_off &&
                switched.wall_stride_count == step.wall_stride_count &&
                switched.foot_push_start_count == step.foot_push_start_count &&
                switched.foot_push_source_contact_tick == step.foot_push_source_contact_tick &&
                switched.foot_push_initial_distance_m == step.foot_push_initial_distance_m &&
                switched.foot_push_elapsed_seconds > step.foot_push_elapsed_seconds &&
                switched.foot_push_command_work_bound_j >= step.foot_push_command_work_bound_j &&
                switched.player_linear_velocity.z < step.player_linear_velocity.z - .1 &&
                std::abs(switched.player_linear_velocity.x - step.player_linear_velocity.x) < .3,
                "Jump replanted or refilled the wall foot instead of changing its remaining effort");
    }
    const auto receiving = land(sim, step);
    require(sim.restart_checkpoint(), "normal retry request rejected");
    tick(sim, 30);
    require(!sim.snapshot().wall_stride_active && sim.snapshot().wall_stride_count == 0 &&
            sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,
            "normal checkpoint retry retained a wall foot or lost actual receiving grade");
    std::cout << "WALL_STRIDE flow push_off=" << push_off
        << " entry_tangent_mps=" << step.player_linear_velocity.x
        << " receiving_tangent_mps=" << receiving.player_linear_velocity.x
        << " step_work_j=" << step.foot_push_command_work_bound_j << '\n';
}
void cancel() {
    Simulation sim;
    const auto step = approach(sim);
    require(sim.request_release(), "deliberate wall-step release rejected");
    tick(sim, 3);
    require(!sim.snapshot().wall_stride_active && !sim.snapshot().foot_push_active &&
            sim.snapshot().wall_stride_count == step.wall_stride_count,
            "release retained or automatically renewed the planted foot");
    (void)land(sim, step);
}
void guards() {
    Simulation sim;
    stage(sim, -42);
    (void)sprint(sim);
    require(sim.set_move_input(.9968, .08) && sim.request_jump(), "empty-air takeoff rejected");
    tick(sim, 105);
    require(sim.snapshot().wall_stride_count == 0 && !sim.snapshot().wall_stride_active,
            "prospective wall geometry created an empty-air foot step");
    stage(sim, -38.45, 3.0, -41);
    require(sim.set_move_input(.9968, .08) && sim.set_sprint_input(true), "falling input rejected");
    tick(sim, 80);
    require(sim.snapshot().wall_stride_count == 0 && !sim.snapshot().wall_stride_active,
            "falling without an actual takeoff armed an automatic step");
}
} // namespace
int main(int argc, char **argv) {
    require(argc == 2, "expected flow, jump, cancel or guards mode");
    const std::string mode = argv[1];
    if (mode == "flow") flow(false);
    else if (mode == "jump") flow(true);
    else if (mode == "cancel") cancel();
    else if (mode == "guards") guards();
    else require(false, "unknown wall-stride mode");
    std::cout << "WALL_STRIDE PASS mode=" << mode << '\n';
    return EXIT_SUCCESS;
}
