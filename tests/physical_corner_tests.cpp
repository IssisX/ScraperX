#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
constexpr double corner_x = -44.25, corner_z = -38.5, roof_top = 5.14;
constexpr double front_z = -38.38, west_x = -44.13;
void require(bool ok, const char *message, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL PHYSICAL_CORNER " << message << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " hands=" << s.traversal_hand_constraint_count << " state=" << int(s.traversal_state)
              << " grips=" << s.traversal_left_hand_generation << ',' << s.traversal_right_hand_generation
              << " left=" << s.traversal_left_hand.x << ',' << s.traversal_left_hand.z
              << " right=" << s.traversal_right_hand.x << ',' << s.traversal_right_hand.z
              << " support=" << s.support_entity_id << '\n';
    std::exit(EXIT_FAILURE);
}
double distance(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z); }
bool front(Vector3 p) { return std::abs(p.z - front_z) < .025 && p.x > corner_x; }
bool west(Vector3 p) { return std::abs(p.x - west_x) < .025 && p.z > -38.25; }
bool mixed(const Snapshot &s) {
    return (west(s.traversal_left_hand) && front(s.traversal_right_hand)) ||
           (front(s.traversal_left_hand) && west(s.traversal_right_hand));
}
double along_lip(Vector3 p) {
    // Signed material distance along the two actual roof faces. It orders
    // the trailing hand through the corner without relying on global yaw.
    return west(p) ? p.z - corner_z : corner_x - p.x;
}
void tick(Simulation &sim) {
    const auto before = sim.snapshot();
    require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "ordinary native tick accepted", sim);
    const auto s = sim.snapshot();
    require(s.tick_index == before.tick_index + 1 && s.death_count == 0 && s.player_gravity_factor == 1,
            "one real native solve with gravity and survival", sim);
    for (const auto f : {s.traversal_left_hand_force, s.traversal_right_hand_force})
        require(std::hypot(f.x, f.y, f.z) <= 1501, "unchanged finite per-hand force", sim);
    require(std::isfinite(s.traversal_command_work_bound_j) && s.traversal_command_work_bound_j >= 0 &&
            std::isfinite(s.traversal_actuator_positive_work_j) && s.traversal_actuator_positive_work_j >= 0 &&
            s.traversal_command_work_bound_j - before.traversal_command_work_bound_j <=
                6600 * Simulation::kFixedStepSeconds + .01 &&
            s.traversal_actuator_positive_work_j - before.traversal_actuator_positive_work_j <=
                6600 * Simulation::kFixedStepSeconds + .01,
            "existing finite command and actuator work rates", sim);
    if (s.foot_push_active)
        require(s.foot_push_peak_load_n <= 3500.01 && s.foot_push_stroke_m <= .300001 &&
                s.foot_push_command_work_bound_j <= 1285.62501, "existing finite foot contract", sim);
}
void hanging(const Simulation &sim, bool corner_owned = true) {
    const auto s = sim.snapshot();
    require(s.traversal_state == TraversalState::Hanging && s.traversal_hand_constraint_count == 2 &&
            s.traversal_support_entity_id == Simulation::kWorldSolidEntityId && !s.foot_push_active &&
            (!corner_owned || !s.hanging_free_arc_active) && !s.player_grounded,
            "actual two-hand roof hang; active corner retains Cartesian ownership", sim);
    if (mixed(s))
        require(s.traversal_normal.z > .99 && std::abs(s.traversal_normal.x) < .01,
                "mixed grips retain original global face until both palms and body arrive", sim);
    if (s.traversal_normal.x > .99)
        require(west(s.traversal_left_hand) && west(s.traversal_right_hand) && s.player_position.x < -44.6,
                "global west-face frame requires both destination palms and actual exterior body", sim);
    for (const auto p : {s.traversal_left_hand, s.traversal_right_hand})
        require(std::abs(p.y - roof_top) < .025 && (front(p) || west(p)),
                "each hand remains on an exposed face of the actual roof top", sim);
}
struct Placements {
    std::uint64_t last_tick = 0, started_tick = 0;
    unsigned count = 0;
    void observe(const Snapshot &before, const Simulation &sim, bool forward) {
        hanging(sim);
        const auto s = sim.snapshot();
        const bool left = s.traversal_left_hand_generation != before.traversal_left_hand_generation;
        const bool right = s.traversal_right_hand_generation != before.traversal_right_hand_generation;
        require(!(left && right), "one hand changes at each real placement", sim);
        if (!left) require(distance(s.traversal_left_hand, before.traversal_left_hand) < 1e-5,
                           "planted left material anchor stays fixed", sim);
        if (!right) require(distance(s.traversal_right_hand, before.traversal_right_hand) < 1e-5,
                            "planted right material anchor stays fixed", sim);
        if (!left && !right) return;
        require((left ? s.traversal_left_hand_generation == before.traversal_left_hand_generation + 1 :
                        s.traversal_right_hand_generation == before.traversal_right_hand_generation + 1) &&
                (!last_tick || s.tick_index - last_tick >= 18), "placement uses one generation and existing eighteen-tick cadence", sim);
        const auto old = left ? before.traversal_left_hand : before.traversal_right_hand;
        const auto planted = left ? before.traversal_right_hand : before.traversal_left_hand;
        const auto next = left ? s.traversal_left_hand : s.traversal_right_hand;
        require(distance(old, next) > .01 &&
                (forward ? along_lip(old) <= along_lip(planted) + .025 :
                           along_lip(old) >= along_lip(planted) - .025),
                "real trailing hand moves while its partner retains material support", sim);
        last_tick = s.tick_index;
        ++count;
        std::cout << "CORNER_REGRIP tick=" << s.tick_index << " left=" << left << " right=" << right
                  << " forward=" << forward << '\n';
    }
};
void neutral(Simulation &sim, unsigned frames, bool corner_owned = true) {
    require(sim.set_move_input(0, 0), "neutral mixed-grip input", sim);
    const auto before = sim.snapshot();
    for (unsigned i = 0; i < frames; ++i) {
        tick(sim);
        hanging(sim, corner_owned);
        const auto s = sim.snapshot();
        require(s.traversal_left_hand_generation == before.traversal_left_hand_generation &&
                s.traversal_right_hand_generation == before.traversal_right_hand_generation &&
                distance(s.traversal_left_hand, before.traversal_left_hand) < 1e-5 &&
                distance(s.traversal_right_hand, before.traversal_right_hand) < 1e-5 &&
                (!corner_owned || s.hanging_commanded_span_m == before.hanging_commanded_span_m) &&
                s.foot_transfer_count == before.foot_transfer_count,
                "neutral retains both anchors and Cartesian ownership without a new radial baseline", sim);
    }
}
void stage(Simulation &sim) {
    require(sim.debug_restart_at({-43.65, 4.05, -39.1}), "one initial actual workshop approach staging", sim);
    require(sim.set_facing(0, 1) && sim.set_move_input(0, 1), "ordinary forward approach to actual roof lip", sim);
    bool caught = false;
    for (unsigned i = 0; i < 180; ++i) {
        tick(sim);
        if (sim.snapshot().traversal_state == TraversalState::Hanging && sim.snapshot().traversal_hand_constraint_count == 2) {
            caught = true;
            break;
        }
    }
    require(caught, "actual two-hand roof catch", sim);
    hanging(sim, false);
    neutral(sim, 30, false);
}
void reach_mixed(Simulation &sim, Placements &placements) {
    placements.started_tick = sim.snapshot().tick_index;
    require(sim.set_move_input(-.7071, .7071), "ordinary diagonal input through both face frames", sim);
    for (unsigned i = 0; i < 720; ++i) {
        const auto before = sim.snapshot();
        tick(sim);
        placements.observe(before, sim, true);
        if (mixed(sim.snapshot())) {
            std::cout << "CORNER_MIXED tick=" << sim.snapshot().tick_index << '\n';
            neutral(sim, 36);
            require(mixed(sim.snapshot()), "neutral preserves the actual mixed-face hold", sim);
            return;
        }
    }
    require(false, "bounded forward shimmy reaches one real grip on each face", sim);
}
void finish_corner(Simulation &sim, Placements &placements) {
    require(sim.set_move_input(-.7071, .7071), "continue ordinary diagonal around outside corner", sim);
    for (unsigned i = 0; i < 720 && sim.snapshot().tick_index - placements.started_tick < 720; ++i) {
        const auto before = sim.snapshot();
        tick(sim);
        placements.observe(before, sim, true);
        const auto s = sim.snapshot();
        if (s.player_position.x < -44.6 && s.player_position.z > -37.9 &&
                west(s.traversal_left_hand) && west(s.traversal_right_hand) &&
                std::abs(s.traversal_left_hand.z - s.traversal_right_hand.z) >= .10 && s.traversal_normal.x > .99)
            return;
    }
    require(false, "body and both distinct actual palms finish on west face before global frame changes", sim);
}
void flow(Simulation &sim, Placements &placements) {
    reach_mixed(sim, placements);
    finish_corner(sim, placements);
    require(placements.count >= 2, "outside corner uses real alternating placements", sim);
    require(sim.set_move_input(0, 0) && sim.set_facing(1, 0), "face actual roof interior before Action", sim);
    const auto before = sim.snapshot();
    require(sim.request_traversal(), "one ordinary west-face top-out Action", sim);
    bool received = false;
    for (unsigned i = 0; i < 240; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        if (s.player_grounded && s.support_entity_id == Simulation::kWorldSolidEntityId &&
                s.traversal_state == TraversalState::None && s.traversal_hand_constraint_count == 0 &&
                std::abs(s.player_position.y - roof_top - .9) < .15) {
            require(s.foot_transfer_count == before.foot_transfer_count + 1 &&
                    s.foot_transfer_support_entity_id == Simulation::kWorldSolidEntityId && s.foot_transfer_peak_hand_load_n > 0,
                    "physical top-out earns actual roof receiving contact", sim);
            received = true;
            break;
        }
    }
    require(received, "bounded physical top-out receives actual roof footing", sim);
    const auto at = sim.snapshot();
    require(sim.set_move_input(1, 0), "ordinary onward movement across roof", sim);
    for (unsigned i = 0; i < 60; ++i) {
        tick(sim);
        require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == Simulation::kWorldSolidEntityId &&
                sim.snapshot().traversal_hand_constraint_count == 0, "onward roof movement retains actual footing", sim);
    }
    require(sim.snapshot().player_position.x > at.player_position.x + .25, "real onward roof progress", sim);
}
void reverse(Simulation &sim, Placements &placements) {
    reach_mixed(sim, placements);
    const auto before = placements.count;
    require(sim.set_move_input(1, 0), "ordinary reversal toward original front-face hold", sim);
    bool returned = false;
    for (unsigned i = 0; i < 720; ++i) {
        const auto prior = sim.snapshot();
        tick(sim);
        placements.observe(prior, sim, false);
        const auto s = sim.snapshot();
        if (placements.count > before && front(s.traversal_left_hand) && front(s.traversal_right_hand) &&
                s.player_position.z < -38.85 && s.player_position.x > -44.05 && s.traversal_normal.z > .99) {
            returned = true;
            break;
        }
    }
    require(returned, "reverse physically returns to original face through fresh validated placement", sim);
    neutral(sim, 36, false);
}
void release(Simulation &sim, Placements &placements) {
    reach_mixed(sim, placements);
    const auto before = sim.snapshot();
    require(sim.request_release(), "ordinary Drop during actual mixed-face corner", sim);
    tick(sim);
    for (unsigned i = 0; i < 90; ++i) {
        const auto s = sim.snapshot();
        require(s.traversal_state == TraversalState::None && s.traversal_hand_constraint_count == 0 &&
                s.traversal_plan_kind == 0 && s.foot_transfer_count == before.foot_transfer_count &&
                s.accepted_traversal_count == before.accepted_traversal_count,
                "Drop clears actual corner ownership without deferred reacquisition or completion", sim);
        tick(sim);
    }
}
} // namespace
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "flow";
    Simulation sim;
    Placements placements;
    stage(sim);
    if (mode == "flow") flow(sim, placements);
    else if (mode == "reverse") reverse(sim, placements);
    else if (mode == "release") release(sim, placements);
    else require(false, "known test mode required", sim);
    std::cout << "PASS PHYSICAL_CORNER mode=" << mode << " placements=" << placements.count << '\n';
}
