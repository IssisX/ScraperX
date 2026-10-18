#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace scraperx::sim;
namespace {
void require(bool ok, const char *message, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL AIR_TUCK " << message << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " compact=" << s.player_crouched << " ground=" << s.player_grounded
              << " support=" << s.support_entity_id << '\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation &sim, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "fixed step accepted", sim);
        require(sim.snapshot().death_count == 0 && sim.snapshot().player_gravity_factor == 1,
                "ordinary gravity and survival", sim);
    }
}
void stage(Simulation &sim, Vector3 at = {40, .9, -55.5}) {
    require(sim.debug_restart_at(at), "initial grade staging", sim);
    require(sim.set_move_input(0, 0) && sim.set_crouch_input(false) && sim.set_sprint_input(false),
            "neutral initial stance", sim);
    tick(sim, 60);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,
            "actual grade supports initial stance", sim);
}
void jump(Simulation &sim) {
    require(sim.request_jump(), "one ordinary Jump", sim);
    for (unsigned i = 0; i < 30; ++i) {
        tick(sim);
        if (!sim.snapshot().player_grounded && sim.snapshot().player_position.y > 1.2) return;
    }
    require(false, "actual takeoff reaches clear free flight", sim);
}
std::vector<Snapshot> flight_trace(bool tuck) {
    Simulation sim(InitialSpawn::ExteriorGrade);
    stage(sim);
    jump(sim);
    const auto before = sim.snapshot();
    std::vector<Snapshot> trace;
    for (unsigned i = 0; i < 60; ++i) {
        const bool held = tuck && (i < 20 || (i >= 30 && i < 40) || (i >= 50 && i < 55));
        require(sim.set_crouch_input(held), "held airborne posture input", sim);
        tick(sim);
        const auto s = sim.snapshot();
        require(!s.player_grounded && s.support_entity_id == 0 && s.traversal_hand_constraint_count == 0,
                "shape change manufactures no receiving support", sim);
        require(s.player_crouched == held, "clear airborne tuck and release change actual capsule stance", sim);
        require(s.foot_push_start_count == before.foot_push_start_count && s.landing_jump_work_j == before.landing_jump_work_j,
                "posture toggles supply no new takeoff or recovery-jump work", sim);
        trace.push_back(s);
    }
    return trace;
}
void flight(Simulation &diagnostic) {
    const auto control = flight_trace(false), tucked = flight_trace(true);
    double peak_position_delta = 0, peak_velocity_delta = 0;
    for (unsigned i = 0; i < control.size(); ++i) {
        const auto &a = control[i], &b = tucked[i];
        const double dp = std::hypot(a.player_position.x - b.player_position.x,
            a.player_position.y - b.player_position.y, a.player_position.z - b.player_position.z);
        const double dv = std::hypot(a.player_linear_velocity.x - b.player_linear_velocity.x,
            a.player_linear_velocity.y - b.player_linear_velocity.y, a.player_linear_velocity.z - b.player_linear_velocity.z);
        peak_position_delta = std::max(peak_position_delta, dp);
        peak_velocity_delta = std::max(peak_velocity_delta, dv);
        require(dp < 1e-9 && dv < 1e-9 && a.tick_index == b.tick_index &&
                a.foot_push_command_work_bound_j == b.foot_push_command_work_bound_j &&
                a.air_control_command_positive_work_j == b.air_control_command_positive_work_j &&
                a.air_control_command_absorbed_work_j == b.air_control_command_absorbed_work_j,
                "tuck and repeated release preserve control COM, velocity and actuator work", diagnostic);
    }
    std::cout << "AIR_TUCK_FLIGHT samples=" << control.size() << " peak_position_delta_m=" << peak_position_delta
              << " peak_velocity_delta_mps=" << peak_velocity_delta << '\n';
}
void route(Simulation &sim) {
    stage(sim, {-57, .9, -65});
    require(sim.set_facing(0, -1) && sim.set_move_input(0, -1) && sim.set_sprint_input(true),
            "run through actual gap between retained partitions", sim);
    bool launched = false;
    for (unsigned i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        if (s.player_position.z < -68.6) {
            require(s.player_grounded && s.player_linear_velocity.z < -5.5 && sim.request_jump(),
                    "duct approach earns its real running takeoff", sim);
            launched = true;
            break;
        }
    }
    require(launched, "bounded approach reaches duct jump point", sim);
    bool tucked = false, brake = false;
    for (unsigned i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        if (!tucked && !s.player_grounded && s.player_position.y > 1.2) {
            require(sim.set_crouch_input(true), "tuck actual airborne knees", sim);
            tucked = true;
        }
        if (!brake && s.player_position.z < -72.5) {
            require(sim.set_move_input(0, 0) && sim.set_sprint_input(false), "brake over actual receiving duct", sim);
            brake = true;
        }
        if (tucked && s.player_grounded && s.support_entity_id == Simulation::kWorldSolidEntityId &&
                s.player_position.z < -72.5 && s.player_position.z > -75.5 &&
                std::abs(s.player_position.y - 1.65 - .6) < .12) {
            require(s.player_crouched && s.traversal_hand_constraint_count == 0 && s.traversal_plan_kind == 0,
                    "compact capsule earns real duct contact without hands or a traversal plan", sim);
            require(sim.set_crouch_input(false), "release compact posture on clear receiver", sim);
            tick(sim);
            require(sim.snapshot().player_grounded && !sim.snapshot().player_crouched &&
                    sim.snapshot().support_entity_id == Simulation::kWorldSolidEntityId &&
                    std::abs(sim.snapshot().player_position.y - 1.65 - .9) < .12,
                    "clear duct top receives normal planted-feet standing", sim);
            const auto standing = sim.snapshot();
            bool departed = false;
            for (unsigned onward = 0; onward < 180; ++onward) {
                tick(sim);
                const auto exit = sim.snapshot();
                require(exit.traversal_hand_constraint_count == 0 && exit.traversal_plan_kind == 0,
                        "onward departure uses no hand support or traversal plan", sim);
                departed = departed || !exit.player_grounded;
                if (departed && exit.player_grounded && exit.support_entity_id == 1 && exit.player_position.y < 1.1) {
                    std::cout << "AIR_TUCK_ROUTE duct_top_m=1.65 contact_tick=" << s.tick_index
                              << " stand_tick=" << standing.tick_index << " grade_tick=" << exit.tick_index << '\n';
                    return;
                }
            }
            require(false, "earned onward momentum departs duct and receives actual grade", sim);
        }
    }
    require(false, "actual jump and compact capsule receive the existing duct top", sim);
}
void blocked(Simulation &sim) {
    stage(sim);
    jump(sim);
    require(sim.set_crouch_input(true), "tuck in real free flight", sim);
    bool requested = false;
    for (unsigned i = 0; i < 160; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        if (!s.player_grounded && s.player_linear_velocity.y < 0 && s.player_position.y < .85 && s.player_position.y > .65) {
            require(s.player_crouched && sim.set_crouch_input(false), "request standing above real grade obstruction", sim);
            const double y = s.player_position.y;
            tick(sim);
            const auto after = sim.snapshot();
            require(after.player_crouched && !after.player_grounded && after.player_position.y < y,
                    "actual grade blocks full airborne capsule without raising COM or fabricating contact", sim);
            requested = true;
            break;
        }
    }
    require(requested, "compact descent exposes an honestly blocked airborne stand", sim);
    for (unsigned i = 0; i < 45; ++i) tick(sim);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1 && !sim.snapshot().player_crouched &&
            std::abs(sim.snapshot().player_position.y - .9) < .08,
            "actual grade contact restores ordinary planted-feet standing", sim);
}
} // namespace
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "flight";
    Simulation sim(InitialSpawn::ExteriorGrade);
    if (mode == "flight") flight(sim);
    else if (mode == "route") route(sim);
    else if (mode == "blocked") blocked(sim);
    else require(false, "known mode required", sim);
    std::cout << "PASS AIR_TUCK mode=" << mode << '\n';
}
