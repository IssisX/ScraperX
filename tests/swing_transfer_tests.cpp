#include "sim/simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
constexpr std::uint64_t first_bar = 2480, second_bar = 2481, frame = 1480;
void require(bool ok, const char *why, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL SWING_TRANSFER " << why << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y
              << " state=" << int(s.traversal_state) << " hands=" << s.traversal_hand_constraint_count
              << " owner=" << s.traversal_support_entity_id << " generations="
              << s.traversal_left_hand_generation << ',' << s.traversal_right_hand_generation << '\n';
    std::exit(EXIT_FAILURE);
}
double length(Vector3 v) { return std::hypot(v.x, v.y, v.z); }
double distance(Vector3 a, Vector3 b) { return std::hypot(a.x-b.x, a.y-b.y, a.z-b.z); }
Vector3 local(const Simulation &sim, std::uint64_t entity, Vector3 point) {
    const auto body = sim.kit_body_index(entity);
    const auto at = sim.kit_body_position(body);
    const auto q = sim.kit_body_rotation(body);
    const Vector3 v{point.x-at.x, point.y-at.y, point.z-at.z}, u{-q.x,-q.y,-q.z};
    const Vector3 twice{2*(u.y*v.z-u.z*v.y),2*(u.z*v.x-u.x*v.z),2*(u.x*v.y-u.y*v.x)};
    return {v.x+q.w*twice.x+u.y*twice.z-u.z*twice.y,
            v.y+q.w*twice.y+u.z*twice.x-u.x*twice.z,
            v.z+q.w*twice.z+u.x*twice.y-u.y*twice.x};
}
double angle(const Simulation &sim, std::uint64_t entity) {
    const auto q = sim.kit_body_rotation(sim.kit_body_index(entity));
    return 2*std::atan2(q.z,q.w);
}
Vector3 bar_point(const Simulation &sim, std::uint64_t entity) {
    const auto body = sim.kit_body_index(entity);
    const auto q = sim.kit_body_rotation(body);
    const auto p = sim.kit_body_position(body);
    return {p.x+1.2*2*q.w*q.z,p.y-1.2*(1-2*q.z*q.z),p.z};
}
Snapshot tick(Simulation &sim) {
    const auto before = sim.snapshot();
    require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "public fixed tick", sim);
    const auto s = sim.snapshot();
    require(s.tick_index == before.tick_index+1 && s.death_count == 0 && s.player_gravity_factor == 1,
            "one real solve, survival and ordinary gravity", sim);
    require(length(s.traversal_left_hand_force) <= 1501 && length(s.traversal_right_hand_force) <= 1501,
            "unchanged finite hand forces", sim);
    require(std::isfinite(s.traversal_command_work_bound_j) && s.traversal_command_work_bound_j >= 0 &&
            s.traversal_command_work_bound_j-before.traversal_command_work_bound_j <=
                6600*Simulation::kFixedStepSeconds+.01 &&
            std::isfinite(s.traversal_actuator_positive_work_j) && s.traversal_actuator_positive_work_j >= 0 &&
            s.traversal_actuator_positive_work_j-before.traversal_actuator_positive_work_j <=
                6600*Simulation::kFixedStepSeconds+.01,
            "unchanged finite hand command-work rate", sim);
    return s;
}
void stage(Simulation &sim) {
    require(sim.entity_body_count(first_bar) == 1 && sim.entity_body_count(second_bar) == 1,
            "both actual passive hangers installed", sim);
    require(sim.debug_restart_at({-50.25,1.35,-59}) && sim.set_move_input(0,0) && sim.set_facing(1,0),
            "one initial actual entry-plinth staging", sim);
    for (unsigned i=0;i<45;++i) tick(sim);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == frame,
            "real receiving contact under initial feet", sim);
    require(sim.request_jump() && sim.set_move_input(1,0), "earned ordinary jump toward first bar", sim);
    for (unsigned i=0;i<180;++i) {
        const auto s = tick(sim);
        if (s.player_swinging && s.traversal_hand_constraint_count == 2 &&
            s.traversal_support_entity_id == first_bar) return;
    }
    require(false, "ordinary jump catches first bar with two real hands", sim);
}
struct Transfer {
    std::array<std::uint64_t,2> owners{first_bar,first_bar};
    std::uint64_t last_placement = 0, last_attempt = 0;
    unsigned placements = 0;
    double prior_angle = 0;
    bool shared_load = false;

    void step(Simulation &sim) {
        const auto before = sim.snapshot();
        const std::array<Vector3,2> old{before.traversal_left_hand,before.traversal_right_hand};
        const std::array<Vector3,2> material{local(sim,owners[0],old[0]),local(sim,owners[1],old[1])};
        const auto s = tick(sim);
        require(s.player_swinging && s.traversal_state == TraversalState::Climbing &&
                s.traversal_hand_constraint_count == 2 && !s.foot_push_active && !s.player_grounded,
                "actual swinging hands retain exclusive airborne support", sim);
        const std::array<bool,2> changed{s.traversal_left_hand_generation != before.traversal_left_hand_generation,
                                        s.traversal_right_hand_generation != before.traversal_right_hand_generation};
        const std::array<Vector3,2> now{s.traversal_left_hand,s.traversal_right_hand};
        require(!(changed[0] && changed[1]), "one actual hand per placement", sim);
        for (unsigned hand=0;hand<2;++hand) {
            if (!changed[hand]) {
                require(distance(material[hand],local(sim,owners[hand],now[hand])) < 1e-4,
                        "planted hand follows its own actual moving material frame", sim);
                continue;
            }
            require(owners[hand] == first_bar &&
                    (hand == 0 ? s.traversal_left_hand_generation == before.traversal_left_hand_generation+1 :
                                 s.traversal_right_hand_generation == before.traversal_right_hand_generation+1) &&
                    (!last_placement || s.tick_index-last_placement >= 18),
                    "fresh distinct forward transfer at existing placement cadence", sim);
            owners[hand] = second_bar;
            ++placements;
            last_placement = s.tick_index;
            std::cout << "SWING_REGRIP tick=" << s.tick_index << " hand=" << hand
                      << " v_before=" << before.player_linear_velocity.x << ',' << before.player_linear_velocity.y
                      << " v_after=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << '\n';
        }
        if (owners[0] != owners[1]) {
            require(s.traversal_support_entity_id == first_bar && !s.hanging_free_arc_active,
                    "mixed-bar handoff retains existing Cartesian principal", sim);
            shared_load = shared_load || (length(s.traversal_left_hand_force) > 1 &&
                                          length(s.traversal_right_hand_force) > 1);
        } else if (owners[0] == second_bar)
            require(s.traversal_support_entity_id == second_bar,
                    "principal changes only when both actual hands reach destination", sim);
    }
    void pump(Simulation &sim) {
        const auto s = sim.snapshot();
        const auto current = angle(sim,first_bar);
        const double direction = placements > 0 ? 1 : (current-prior_angle >= 0 ? 1 : -1);
        prior_angle = current;
        require(sim.set_move_input(direction,0) && sim.set_facing(1,0), "ordinary phase-chosen lean", sim);
        const auto target = bar_point(sim,second_bar);
        const double reach = std::hypot(target.x-s.player_position.x,target.y-s.player_position.y-.45);
        if (reach < 1.04 && target.x-s.player_position.x > .18 && s.tick_index > last_attempt+18 && direction > 0) {
            require(sim.request_traversal(), "one deliberate reachable next-bar Action", sim);
            last_attempt = s.tick_index;
        }
        step(sim);
    }
    void mixed(Simulation &sim) {
        prior_angle = angle(sim,first_bar);
        for (unsigned i=0;i<1080 && placements == 0;++i) pump(sim);
        require(placements == 1, "bounded actual first-hand transfer", sim);
        require(sim.set_move_input(0,0), "neutral midway through two-bar handoff", sim);
        const auto second_angle = angle(sim,second_bar);
        double receiver_motion = 0;
        for (unsigned i=0;i<30;++i) {
            step(sim);
            receiver_motion = std::max(receiver_motion,std::abs(angle(sim,second_bar)-second_angle));
        }
        require(placements == 1 && shared_load && receiver_motion > 1e-4,
                "neutral preserves one hand per bar with actual solved loads and recipient motion", sim);
    }
};
void release(Simulation &sim, bool jump) {
    require(sim.set_move_input(jump ? 0 : 1,0) && (jump ? sim.request_jump() : sim.request_release()),
            "ordinary Jump/Drop release", sim);
    bool landed = false;
    for (unsigned i=0;i<180;++i) {
        const auto s = tick(sim);
        require(s.traversal_hand_constraint_count == 0 && s.traversal_state == TraversalState::None &&
                s.traversal_plan_kind == 0, "released handoff cannot replay itself", sim);
        if (s.player_grounded) { landed = true; break; }
    }
    require(landed, "real yard receives released body", sim);
}
} // namespace
int main(int argc,char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "flow";
    Simulation sim;
    stage(sim);
    const auto before = sim.snapshot();
    require(sim.set_facing(-1,0) && sim.set_move_input(1,0) && sim.request_traversal(),
            "deliberate Action away from every next bar", sim);
    const auto rejected = tick(sim);
    require(rejected.traversal_hand_constraint_count == 2 && rejected.player_swinging &&
            rejected.traversal_left_hand_generation == before.traversal_left_hand_generation &&
            rejected.traversal_right_hand_generation == before.traversal_right_hand_generation &&
            rejected.rejected_traversal_count == before.rejected_traversal_count+1,
            "missing next grip rejects Action while both real hands remain", sim);
    Transfer transfer;
    transfer.mixed(sim);
    if (mode == "flow") {
        for (unsigned i=0;i<1080 && transfer.placements < 2;++i) transfer.pump(sim);
        require(transfer.placements == 2 && sim.snapshot().traversal_support_entity_id == second_bar,
                "two separate Actions complete destination ownership", sim);
        release(sim,false);
        const auto at = sim.snapshot();
        require(sim.set_facing(1,0) && sim.set_move_input(.5,0), "ordinary onward yard movement", sim);
        for (unsigned i=0;i<60;++i) tick(sim);
        require(sim.snapshot().player_position.x > at.player_position.x+.2 &&
                sim.snapshot().traversal_hand_constraint_count == 0, "actual onward movement after transfer", sim);
    } else if (mode == "release" || mode == "jump") release(sim,mode == "jump");
    else if (mode == "restart") {
        require(sim.restart_checkpoint(), "checkpoint retry during mixed handoff", sim);
        for (unsigned i=0;i<30;++i) tick(sim);
        require(sim.snapshot().traversal_hand_constraint_count == 0 &&
                sim.snapshot().traversal_plan_kind == 0 && !sim.snapshot().foot_push_active &&
                sim.entity_body_count(first_bar) == 1 && sim.entity_body_count(second_bar) == 1,
                "retry clears transient hands and retains real route bodies", sim);
    } else require(false,"known test mode",sim);
    std::cout << "PASS SWING_TRANSFER mode=" << mode << " placements=" << transfer.placements << '\n';
}
