#include "sim/simulation.hpp"
#include "sim/traversal_plan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
constexpr std::uint64_t receiver = 1960;
constexpr auto receiving_phase = static_cast<std::uint8_t>(TraversalPhase::ReceiverApproach);
void require(bool ok, const char *message, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL RECEIVER_HANDOFF " << message << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " grounded=" << s.player_grounded << " support=" << s.support_entity_id
              << " hands=" << s.traversal_hand_constraint_count << " phase=" << int(s.traversal_phase)
              << " transfer=" << s.foot_transfer_count << '\n';
    std::exit(EXIT_FAILURE);
}
struct Probe {
    Simulation sim;
    Snapshot previous = sim.snapshot();
    bool receiving_seen = false;
    bool fresh_foot_seen = false;
    Snapshot completion{};
    std::uint64_t input_tick = 0;
    void tick() {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick accepted", sim);
        const auto s = sim.snapshot();
        require(s.player_gravity_factor == 1 && s.death_count == 0, "real gravity and survival throughout handoff", sim);
        for (const auto force : {s.traversal_left_hand_force, s.traversal_right_hand_force})
            require(std::hypot(force.x, force.y, force.z) <= 1501,
                    "existing per-hand force ceiling remains finite", sim);
        const double debit = s.traversal_command_work_bound_j - previous.traversal_command_work_bound_j;
        const double work = s.traversal_actuator_positive_work_j - previous.traversal_actuator_positive_work_j;
        require(std::isfinite(s.traversal_command_work_bound_j) && s.traversal_command_work_bound_j >= 0 &&
                std::isfinite(s.traversal_actuator_positive_work_j) && s.traversal_actuator_positive_work_j >= 0 &&
                debit <= 6600 * Simulation::kFixedStepSeconds + .01 &&
                work <= 6600 * Simulation::kFixedStepSeconds + .01,
                "bounded existing hand command and positive-work rate", sim);
        if (s.foot_push_active)
            require(s.foot_push_peak_load_n <= 3500.01 && s.foot_push_stroke_m <= .300001 &&
                    s.foot_push_command_work_bound_j <= 1285.62501,
                    "receiving leg retains finite force, stroke and work", sim);
        receiving_seen = receiving_seen || (s.traversal_phase == receiving_phase && s.traversal_hand_constraint_count > 0);
        if (input_tick && s.foot_push_active && s.foot_push_support_entity_id == receiver &&
                s.foot_push_source_contact_tick > input_tick)
            fresh_foot_seen = true;
        if (s.foot_transfer_count > previous.foot_transfer_count) completion = s;
        previous = s;
    }
};
void steering(Probe &p, double x, double z) {
    const auto s = p.sim.snapshot();
    const double dx = x - s.player_position.x, dz = z - s.player_position.z;
    const double ix = dx * 1.8 - s.player_linear_velocity.x * .28;
    const double iz = dz * 1.8 - s.player_linear_velocity.z * .28;
    const double scale = std::max(1.0, std::hypot(ix, iz));
    require(p.sim.set_move_input(ix / scale, iz / scale), "ordinary waypoint steering", p.sim);
    if (std::hypot(dx, dz) > .001) require(p.sim.set_facing(dx, dz), "ordinary waypoint facing", p.sim);
}
void walk(Probe &p, double x, double z) {
    // Same public-input controller as physical_traversal_tests.cpp.
    for (unsigned i = 0; i < 1500; ++i) {
        const auto s = p.sim.snapshot();
        if (std::hypot(x - s.player_position.x, z - s.player_position.z) < .04 && s.player_grounded &&
                std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .15) {
            require(p.sim.set_move_input(0, 0), "release waypoint input", p.sim);
            for (unsigned j = 0; j < 30; ++j) p.tick();
            require(p.sim.snapshot().player_grounded, "waypoint settles on actual contact", p.sim);
            return;
        }
        steering(p, x, z);
        p.tick();
    }
    require(false, "bounded ordinary walking reaches target", p.sim);
}
void approach(Probe &p) {
    require(p.sim.debug_restart_at({10, 110.9, -174}), "one initial actual AS026 shelf staging", p.sim);
    p.previous = p.sim.snapshot();
    for (unsigned i = 0; i < 30; ++i) p.tick();
    require(p.sim.snapshot().player_grounded, "actual entry shelf supplies footing", p.sim);
    walk(p, 10, -179.55);
    require(p.sim.set_facing(0, -1) && p.sim.request_traversal(), "ordinary Action acquires actual ladder", p.sim);
    p.input_tick = p.sim.snapshot().tick_index;
    p.tick();
    require(p.sim.snapshot().traversal_state == TraversalState::Climbing &&
            p.sim.snapshot().traversal_hand_constraint_count == 2, "two real climbing constraints acquired", p.sim);
    require(p.sim.set_move_input(0, -1), "climb toward actual receiver", p.sim);
    for (unsigned i = 0; i < 1200; ++i) {
        p.tick();
        const auto s = p.sim.snapshot();
        if (s.player_grounded && s.player_position.y > 114.7) {
            std::cout << "RECEIVER_FIRST tick=" << s.tick_index << " y=" << s.player_position.y
                      << " z=" << s.player_position.z << " hands=" << s.traversal_hand_constraint_count
                      << " phase=" << int(s.traversal_phase) << '\n';
            return;
        }
    }
    require(false, "actual climb reaches first receiving contact", p.sim);
}
void onward(Probe &p) {
    approach(p);
    walk(p, 9.15, -180.10);
    const auto s = p.sim.snapshot();
    const auto &c = p.completion;
    std::cout << "RECEIVER_RECEIPT approach=" << p.receiving_seen << " fresh_foot=" << p.fresh_foot_seen
              << " input_tick=" << p.input_tick << " transfer=" << c.foot_transfer_count
              << " receiver=" << c.foot_transfer_support_entity_id << " event_tick=" << c.foot_transfer_tick
              << " shared_ticks=" << c.hand_foot_overlap_solved_ticks << " shared_event_tick=" << c.hand_foot_last_solve_tick
              << " hand_n=" << c.hand_foot_last_hand_load_n << " foot_n=" << c.hand_foot_last_foot_load_n
              << " foot_source=" << c.foot_push_source_contact_tick << " foot_active=" << c.foot_push_active << '\n';
    require(p.receiving_seen && p.fresh_foot_seen && c.foot_transfer_count == 1 &&
            c.foot_transfer_support_entity_id == receiver && c.foot_transfer_tick > p.input_tick &&
            c.hand_foot_overlap_solved_ticks > 0 && c.hand_foot_last_solve_tick > p.input_tick &&
            c.hand_foot_last_hand_load_n > 0 && c.hand_foot_last_foot_load_n > 0,
            "new receiving approach earns fresh actual footing and durable positive shared-solve receipt", p.sim);
    require(s.player_grounded && s.support_entity_id == receiver && s.checkpoint_footing_valid &&
            s.traversal_state == TraversalState::None && s.traversal_hand_constraint_count == 0 &&
            s.traversal_plan_kind == 0 && s.foot_transfer_count == 1 &&
            std::hypot(s.player_position.x - 9.15, s.player_position.z + 180.10) < .05,
            "handoff releases hands and ordinary steering reaches firm receiving target", p.sim);
    std::cout << "RECEIVER_ONWARD tick=" << s.tick_index << " support=" << s.support_entity_id
              << " transfer=" << s.foot_transfer_count << " hand_n=" << c.hand_foot_last_hand_load_n
              << " foot_n=" << c.hand_foot_last_foot_load_n << '\n';
}
void release(Probe &p) {
    approach(p);
    bool requested = false;
    for (unsigned i = 0; i < 300; ++i) {
        const auto s = p.sim.snapshot();
        if (s.traversal_phase == receiving_phase && s.traversal_hand_constraint_count > 0) {
            require(p.sim.set_move_input(0, 0) && p.sim.request_release(), "deliberate release during actual receiving approach", p.sim);
            requested = true;
            break;
        }
        steering(p, 9.15, -180.10);
        p.tick();
    }
    require(requested, "observable receiving phase permits real deliberate release", p.sim);
    const auto before = p.sim.snapshot();
    p.tick();
    require(p.sim.snapshot().traversal_hand_constraint_count == 0 &&
            p.sim.snapshot().traversal_state == TraversalState::None && p.sim.snapshot().traversal_plan_kind == 0,
            "Release immediately detaches actual hands and plan", p.sim);
    for (unsigned i = 0; i < 90; ++i) {
        p.tick();
        const auto s = p.sim.snapshot();
        require(s.traversal_hand_constraint_count == 0 && s.traversal_state == TraversalState::None &&
                s.traversal_plan_kind == 0 && s.foot_transfer_count == before.foot_transfer_count &&
                s.accepted_traversal_count == before.accepted_traversal_count,
                "released approach cannot replay completion or reacquire hands without input", p.sim);
    }
    const auto s = p.sim.snapshot();
    std::cout << "RECEIVER_RELEASE tick=" << s.tick_index << " grounded=" << s.player_grounded
              << " support=" << s.support_entity_id << " y=" << s.player_position.y << '\n';
}
} // namespace
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "onward";
    Probe p;
    if (mode == "onward") onward(p);
    else if (mode == "release") release(p);
    else require(false, "known mode required", p.sim);
    std::cout << "PASS RECEIVER_HANDOFF mode=" << mode << '\n';
}
