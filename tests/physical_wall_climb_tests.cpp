#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool ok, const char *message, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL WALL_CLIMB " << message << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " steps=" << s.wall_stride_count << " foot=" << s.foot_push_active
              << " hands=" << s.traversal_hand_constraint_count << '\n';
    std::exit(EXIT_FAILURE);
}
double length(Vector3 v) { return std::hypot(v.x, v.y, v.z); }
Snapshot tick(Simulation &sim, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "public native tick", sim);
        const auto s = sim.snapshot();
        require(s.player_gravity_factor == 1 && s.death_count == 0, "ordinary gravity and survival", sim);
        require(length(s.traversal_left_hand_force) <= 1501 && length(s.traversal_right_hand_force) <= 1501,
                "existing finite hand forces", sim);
        if (s.foot_push_active)
            require(s.foot_push_peak_load_n <= 3500.01 && s.foot_push_stroke_m <= .300001 &&
                    s.foot_push_command_work_bound_j <= 1285.62501 && s.foot_push_elapsed_seconds <= .250001,
                    "existing finite leg force, stroke, work and age", sim);
    }
    return sim.snapshot();
}
void stage(Simulation &sim, double x = -41, double height = .9, double z = -42) {
    // The actual workshop front is x[-44,-28], z=-38.08, top3.8m.
    // Staging x=-46 deliberately leaves this panel for the no-wall negative.
    require(sim.debug_restart_at({x, height, z}), "one initial actual workshop-side staging", sim);
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false) && sim.set_facing(0, 1),
            "neutral approach input", sim);
    if (height == .9)
        require(tick(sim, 30).player_grounded, "actual grade before approach", sim);
}
std::uint64_t takeoff(Simulation &sim, double x = -41, bool sprint = true) {
    stage(sim, x);
    require(sim.set_move_input(0, 1) && sim.set_sprint_input(sprint), "ordinary forward approach", sim);
    bool running = false;
    for (unsigned i = 0; i < 180; ++i) {
        const auto s = tick(sim);
        require(s.wall_stride_count == 0, "ground running creates no wall step", sim);
        if (s.player_grounded && s.player_linear_velocity.z > (sprint ? 6.7 : 5.0)) {
            running = true;
            break;
        }
    }
    require(running, "earn actual approach momentum", sim);
    const auto input_tick = sim.snapshot().tick_index;
    require(sim.request_jump(), "ordinary paid Jump", sim);
    const auto s = tick(sim);
    require(!s.player_grounded && s.player_linear_velocity.y > 4, "real takeoff", sim);
    return input_tick;
}
Snapshot loaded_step(Simulation &sim) {
    const auto input_tick = takeoff(sim);
    for (unsigned i = 0; i < 130; ++i) {
        const auto s = tick(sim);
        require(s.traversal_hand_constraint_count == 0, "this catch must follow the loaded wall step", sim);
        if (s.wall_stride_active && s.foot_push_last_load_n > 0) {
            require(s.wall_stride_count == 1 && s.wall_stride_pushing_off &&
                    s.foot_push_source_contact_tick > input_tick &&
                    s.foot_push_support_entity_id == Simulation::kWorldSolidEntityId &&
                    s.wall_stride_normal.z < -.9 && s.foot_push_player_force.y > 0 &&
                    s.player_linear_velocity.y > 0,
                    "fresh actual lower-wall contact supplies a finite upward/outward push", sim);
            std::cout << "WALL_CLIMB_STEP tick=" << s.tick_index << " contact=" << s.foot_push_source_contact_tick
                      << " force_y_n=" << s.foot_push_player_force.y << " work_j=" << s.foot_push_command_work_bound_j
                      << " upward_m_s=" << s.player_linear_velocity.y << '\n';
            return s;
        }
    }
    require(false, "real forward sprint-jump reaches a loaded upward wall step", sim);
    return {};
}
void recover(Simulation &sim, std::uint64_t steps) {
    require(sim.set_move_input(0, -1) && sim.set_facing(0, -1) && sim.set_sprint_input(false),
            "ordinary away-from-wall recovery input", sim);
    bool landed = false;
    for (unsigned i = 0; i < 180; ++i) {
        const auto s = tick(sim);
        require(s.wall_stride_count == steps && !s.wall_stride_active &&
                s.traversal_hand_constraint_count == 0, "released action cannot renew itself", sim);
        if (s.player_grounded && s.support_entity_id == 1) { landed = true; break; }
    }
    const auto received = sim.snapshot();
    require(landed && received.player_position.y < 1 && received.checkpoint_footing_valid,
            "actual grade receives the released body", sim);
    const auto onward = tick(sim, 30);
    require(onward.player_grounded && onward.player_position.z < received.player_position.z - .3,
            "receiving feet permit ordinary onward movement", sim);
    require(sim.restart_checkpoint(), "ordinary checkpoint retry", sim);
    const auto reset = tick(sim, 30);
    require(reset.player_grounded && !reset.foot_push_active && reset.wall_stride_count == 0 &&
            reset.traversal_hand_constraint_count == 0 && reset.traversal_plan_kind == 0,
            "retry clears the transient foot, hands and one-shot wall action", sim);
}
void flow() {
    Simulation sim;
    const auto step = loaded_step(sim);
    bool caught = false;
    for (unsigned i = 0; i < 90; ++i) {
        const auto s = tick(sim);
        require(s.wall_stride_count == 1, "held sprint cannot refill the wall step", sim);
        if (s.traversal_hand_constraint_count > 0) { caught = true; break; }
    }
    const auto catch_state = sim.snapshot();
    require(caught && catch_state.traversal_state == TraversalState::Hanging &&
            catch_state.traversal_hand_constraint_count == 2 && !catch_state.foot_push_active &&
            catch_state.player_position.y > step.player_position.y,
            "earned wall step reaches a real two-hand lip catch and retires the leg", sim);
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false), "neutral real hang", sim);
    const auto held = tick(sim, 12);
    require(held.traversal_hand_constraint_count == 2 && !held.player_grounded &&
            length(held.traversal_left_hand_force) + length(held.traversal_right_hand_force) > 0,
            "real finite hands support the body", sim);
    // This thin wall has no receiving deck behind its lip. It is a valid
    // handhold, but Action must not manufacture a standing top-out.
    require(sim.request_traversal(), "attempt top-out from actual narrow lip", sim);
    const auto rejected = tick(sim, 14);
    require(rejected.traversal_hand_constraint_count == 2 && !rejected.player_grounded &&
            !rejected.foot_push_active && rejected.foot_transfer_count == 0 && rejected.traversal_plan_kind == 0,
            "no receiver means no foot push or false top-out; the genuine hang remains", sim);
    require(sim.request_release(), "deliberate Drop from actual catch", sim);
    require(tick(sim).traversal_hand_constraint_count == 0, "Drop detaches the real hands", sim);
    recover(sim, step.wall_stride_count);
    std::cout << "WALL_CLIMB_FLOW catch_tick=" << catch_state.tick_index << " y=" << catch_state.player_position.y
              << " hand_n=" << length(held.traversal_left_hand_force) + length(held.traversal_right_hand_force)
              << " actual_grade_recovery=1 onward=1 retry=1\n";
}
void cancel() {
    Simulation sim;
    const auto step = loaded_step(sim);
    require(sim.request_release() && sim.set_move_input(0, 0) && sim.set_sprint_input(false),
            "deliberate cancellation of loaded wall foot", sim);
    const auto stopped = tick(sim, 3);
    require(!stopped.wall_stride_active && !stopped.foot_push_active &&
            stopped.traversal_hand_constraint_count == 0 && stopped.wall_stride_count == step.wall_stride_count,
            "cancellation removes the foot without a deferred catch or refill", sim);
    recover(sim, step.wall_stride_count);
}
void guards() {
    {
        Simulation sim;
        (void)takeoff(sim, -46);
        const auto s = tick(sim, 130);
        require(s.wall_stride_count == 0 && !s.wall_stride_active && !s.foot_push_active,
                "a real sprint-jump beside the wall cannot create contact", sim);
    }
    {
        Simulation sim;
        stage(sim, -41, 3.0, -38.45);
        require(sim.set_move_input(0, 1) && sim.set_sprint_input(true), "forward falling input", sim);
        const auto s = tick(sim, 80);
        // A legitimate reachable hand catch is allowed; it cannot grant a leg.
        require(s.wall_stride_count == 0 && !s.wall_stride_active && !s.foot_push_active,
                "falling against a wall without earned Jump grants no automatic step", sim);
    }
    {
        Simulation sim;
        (void)takeoff(sim, -41, false);
        const auto s = tick(sim, 130);
        require(s.wall_stride_count == 0 && !s.wall_stride_active,
                "ordinary jump without sprint intent grants no automatic wall step", sim);
    }
}
} // namespace
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "flow";
    if (mode == "flow") flow();
    else if (mode == "cancel") cancel();
    else if (mode == "guards") guards();
    else { std::cerr << "Expected flow, cancel or guards\n"; return EXIT_FAILURE; }
    std::cout << "PASS WALL_CLIMB mode=" << mode << '\n';
}
