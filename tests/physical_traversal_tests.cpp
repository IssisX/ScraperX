#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

using scraperx::sim::Simulation;
using scraperx::sim::TraversalState;

namespace {
bool tick(Simulation &s) {
    return s.advance_frame(Simulation::kFixedStepSeconds).accepted;
}

bool walk(Simulation &s, double x, double z) {
    for (int i = 0; i < 1500; ++i) {
        const auto p = s.snapshot().player_position;
        const double dx = x - p.x, dz = z - p.z, d = std::hypot(dx, dz);
        if (d < .04) {
            (void)s.set_move_input(0, 0);
            for (int j = 0; j < 30; ++j) if (!tick(s)) return false;
            return s.snapshot().player_grounded;
        }
        const double amount = std::min(1.0, d / .5) / d;
        (void)s.set_move_input(dx * amount, dz * amount);
        (void)s.set_facing(dx, dz);
        if (!tick(s)) return false;
    }
    return false;
}

bool passive_departure(Simulation &s, const char *surface, bool physical) {
    (void)s.set_move_input(0, -1);
    for (int i = 0; i < 90; ++i) {
        if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) return false;
    }
    const auto before = s.snapshot();
    const bool backend_ok = physical ?
        before.player_gravity_factor == 1.0 && before.traversal_hand_constraint_count == 2 &&
            before.traversal_command_work_bound_j > 0 :
        before.player_gravity_factor == 0.0 && before.traversal_hand_constraint_count == 0;
    if (!backend_ok) {
        std::cerr << "FAIL climb backend surface=" << surface
                  << " gravity=" << before.player_gravity_factor
                  << " constraints=" << before.traversal_hand_constraint_count << '\n';
        return false;
    }
    (void)s.set_move_input(0, 0);
    (void)s.request_release();
    if (!tick(s)) return false;
    const auto after = s.snapshot();
    // Released above the receiver, with no jump or impact: gravity changes
    // vertical velocity once. A passive release must retain climbing momentum.
    const double expected_y = before.player_linear_velocity.y -
                              9.81 * Simulation::kFixedStepSeconds;
    const double error = std::abs(after.player_linear_velocity.y - expected_y);
    const bool ok = before.player_linear_velocity.y > .2 && !after.player_grounded &&
                    after.traversal_state == TraversalState::None && error < .002 &&
                    after.player_gravity_factor == 1.0 && after.traversal_hand_constraint_count == 0;
    std::cout << (ok ? "PASS" : "FAIL") << " passive_departure surface=" << surface
              << " before_vy=" << before.player_linear_velocity.y
              << " after_vy=" << after.player_linear_velocity.y
              << " expected_vy=" << expected_y << " error=" << error
              << " grounded=" << after.player_grounded << '\n';
    return ok;
}

bool rigid_departure() {
    Simulation s;
    // Supported development staging on an actual shipping duct. This is a
    // contact regression; ordinary grade-route proof is a separate gate.
    // FacadeRoute's authored 27.3 m duct is instantiated at offset Y=-11.
    if (!s.debug_restart_at({24, 17.2, -123.0059})) {
        std::cerr << "FAIL rigid departure staging is not collision-clear\n";
        return false;
    }
    for (int i = 0; i < 30; ++i) if (!tick(s)) return false;
    if (!s.snapshot().player_grounded) {
        std::cerr << "FAIL rigid departure staging has no real footing\n";
        return false;
    }
    (void)s.set_facing(0, -1);
    (void)s.request_traversal();
    if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) {
        std::cerr << "FAIL rigid departure acquisition state=" << int(s.snapshot().traversal_state)
                  << " grip=" << s.snapshot().grip_entity_id << '\n';
        return false;
    }
    return passive_departure(s, "rigid_shipping_vent", true);
}

bool deforming_departure() {
    Simulation s;
    for (int i = 0; i < 45; ++i) if (!tick(s)) return false;
    // Actual ordinary grade approach to the cargo net, without relocation.
    for (int i = 0; i < 2700; ++i) {
        const auto p = s.snapshot().player_position;
        const double dx = 20 - p.x, dz = -114 - p.z;
        const double d = std::hypot(dx, dz);
        if (d < .12) break;
        const double amount = std::min(1.0, d / .5) / d;
        (void)s.set_move_input(dx * amount, dz * amount);
        (void)s.set_facing(dx, dz);
        if (!tick(s)) return false;
    }
    (void)s.set_move_input(0, 0);
    for (int i = 0; i < 30; ++i) if (!tick(s)) return false;
    (void)s.set_facing(0, -1);
    (void)s.set_move_input(0, -.25);
    for (int i = 0; i < 360 &&
         !(s.snapshot().grip_available && s.snapshot().grip_entity_id == 1953); ++i)
        if (!tick(s)) return false;
    (void)s.set_move_input(0, 0);
    if (!s.snapshot().player_grounded || !s.snapshot().grip_available) return false;
    (void)s.request_traversal();
    if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) return false;
    return passive_departure(s, "deforming_cargo_net", false);
}

bool rotating_departure() {
    Simulation s;
    // Actual AS-026 entry shelf; development staging is not campaign proof.
    if (!s.debug_restart_at({10, 110.9, -174})) return false;
    for (int i = 0; i < 30; ++i) if (!tick(s)) return false;
    if (!walk(s, 10, -179.4)) return false;
    (void)s.set_facing(0, -1);
    (void)s.request_traversal();
    if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) return false;
    (void)s.set_move_input(0, -1);
    bool shelf = false;
    for (int i = 0; i < 1200; ++i) {
        if (!tick(s)) return false;
        if (s.snapshot().player_grounded && s.snapshot().player_position.y > 114.7) {
            shelf = true;
            break;
        }
    }
    if (!shelf || !walk(s, 9.15, -180.10)) return false;
    (void)s.set_facing(0, -1);
    (void)s.set_move_input(-1, -.35);
    (void)s.request_jump();
    for (int i = 0; i < 135; ++i) {
        if (!tick(s)) return false;
        if (s.snapshot().traversal_state == TraversalState::Climbing &&
            s.snapshot().traversal_support_entity_id == 2960)
            return passive_departure(s, "rotating_as026", true);
    }
    std::cerr << "FAIL rotating departure did not catch actual AS-026\n";
    return false;
}
}

int main() {
    const bool rigid = rigid_departure();
    const bool deforming = deforming_departure();
    const bool rotating = rotating_departure();
    if (!rigid || !deforming || !rotating) {
        std::cerr << "FAIL physical traversal: rigid=" << rigid
                  << " deforming=" << deforming << " rotating=" << rotating << '\n';
        return 1;
    }
}
