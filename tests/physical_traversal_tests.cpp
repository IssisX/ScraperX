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
        const auto state = s.snapshot();
        const auto p = state.player_position;
        const auto v = state.player_linear_velocity;
        const double dx = x - p.x, dz = z - p.z, d = std::hypot(dx, dz);
        if (d < .04 && state.player_grounded && std::hypot(v.x, v.z) < .15) {
            (void)s.set_move_input(0, 0);
            for (int j = 0; j < 30; ++j) if (!tick(s)) return false;
            return s.snapshot().player_grounded;
        }
        const double ix = dx * 1.8 - v.x * .28;
        const double iz = dz * 1.8 - v.z * .28;
        const double scale = std::max(1.0, std::hypot(ix, iz));
        (void)s.set_move_input(ix / scale, iz / scale);
        if (d > .001) (void)s.set_facing(dx, dz);
        if (!tick(s)) return false;
    }
    return false;
}

bool passive_departure(Simulation &s, const char *surface) {
    (void)s.set_move_input(0, -1);
    for (int i = 0; i < 360; ++i) {
        if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) return false;
        // Acquire a genuinely ascending departure, rather than assuming every
        // compliant support settles into it at the same elapsed time.
        if (i >= 89 && s.snapshot().player_linear_velocity.y > .25) break;
    }
    const auto before = s.snapshot();
    const bool backend_ok = before.player_gravity_factor == 1.0 &&
        before.traversal_hand_constraint_count == 2 && before.traversal_command_work_bound_j > 0;
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
    return passive_departure(s, "rigid_shipping_vent");
}

bool deforming_departure(bool jumping=false) {
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
    if (!jumping) return passive_departure(s, "deforming_cargo_net");
    (void)s.set_move_input(0,-1);
    for (int i=0;i<450 && s.snapshot().player_position.y<2.0;++i)
        if (!tick(s) || s.snapshot().traversal_state!=TraversalState::Climbing) return false;
    const auto before=s.snapshot();
    (void)s.set_move_input(0,0);
    (void)s.request_jump();
    if (!tick(s)) return false;
    const auto pushed=s.snapshot();
    if (pushed.traversal_state!=TraversalState::Climbing || pushed.traversal_hand_constraint_count!=2 ||
        pushed.player_gravity_factor!=1.0 ||
        std::abs(pushed.player_linear_velocity.y-before.player_linear_velocity.y)>.35 ||
        pushed.traversal_command_work_bound_j<=before.traversal_command_work_bound_j) {
        std::cerr<<"FAIL cargo Jump must start a finite reciprocal muscle stroke, not reset velocity\n";
        return false;
    }
    for(int i=0;i<24 && s.snapshot().traversal_state==TraversalState::Climbing;++i)
        if(!tick(s)) return false;
    const auto released=s.snapshot();
    const double debit=released.traversal_command_work_bound_j-before.traversal_command_work_bound_j;
    if(released.traversal_state!=TraversalState::None || released.traversal_hand_constraint_count!=0 ||
       debit<=0 || debit>600.01 || released.player_grounded) return false;
    if(!tick(s)) return false;
    const bool free=std::abs(s.snapshot().player_linear_velocity.y-released.player_linear_velocity.y+
                             9.81*Simulation::kFixedStepSeconds)<.002;
    std::cout<<(free?"PASS":"FAIL")<<" cargo_jump finite_stroke_debit="<<debit
             <<" departure_vy="<<released.player_linear_velocity.y<<'\n';
    return free;
}

bool selected_support_traction() {
    Simulation s;
    if(!s.debug_restart_at({20,1.0,-116.7})) return false;
    for(int i=0;i<180;++i) if(!tick(s)) return false;
    const auto before=s.snapshot();
    if(!before.player_grounded || before.support_entity_id!=2954 || before.landing_recovery_seconds>0) {
        std::cerr<<"FAIL tread traction staging must reach ordinary grounded walking\n";
        return false;
    }
    (void)s.set_move_input(1,0);
    if(!tick(s)) return false;
    const auto after=s.snapshot();
    const bool ok=after.landing_recovery_work_j>before.landing_recovery_work_j &&
        after.player_linear_velocity.x-before.player_linear_velocity.x>0 &&
        after.player_linear_velocity.x-before.player_linear_velocity.x<.10;
    std::cout<<(ok?"PASS":"FAIL")<<" ordinary_tread_traction force_work_delta="
             <<after.landing_recovery_work_j-before.landing_recovery_work_j
             <<" dvx="<<after.player_linear_velocity.x-before.player_linear_velocity.x<<'\n';
    return ok;
}

bool receiver_walk_off() {
    Simulation s;
    // Development staging isolates an ordinary walking departure. The cargo
    // route separately proves this receiver is reached from grade without it.
    if(!s.debug_restart_at({20,12.0,-119.0})) return false;
    for(int i=0;i<180;++i) if(!tick(s)) return false;
    if(!s.snapshot().player_grounded || s.snapshot().support_entity_id!=2952) return false;
    (void)s.set_facing(0,1);
    (void)s.set_move_input(0,1);
    for(int i=0;i<180 && s.snapshot().player_grounded;++i) if(!tick(s)) return false;
    const auto before=s.snapshot();
    if(before.player_grounded || before.traversal_state!=TraversalState::None ||
       before.player_linear_velocity.z<1.0) return false;
    (void)s.set_move_input(0,0);
    if(!tick(s)) return false;
    const auto after=s.snapshot();
    const double error=std::hypot(after.player_linear_velocity.x-before.player_linear_velocity.x,
                                  after.player_linear_velocity.z-before.player_linear_velocity.z);
    const bool ok=after.traversal_state==TraversalState::None && error<.002 &&
        std::abs(after.player_linear_velocity.y-before.player_linear_velocity.y+
                 9.81*Simulation::kFixedStepSeconds)<.002;
    std::cout<<(ok?"PASS":"FAIL")<<" receiver_walk_off momentum_error="<<error<<'\n';
    return ok;
}

bool lowering_release(bool cargo, bool immediate) {
    Simulation s;
    // Supported staging isolates the shipping cargo receiver's edge. No
    // traversal state, hand constraint or departure velocity is injected.
    if (!s.debug_restart_at(cargo ? scraperx::sim::Vector3{22, 12.0, -120.3} :
                                   scraperx::sim::Vector3{20, 17.2, -123.15})) return false;
    for (int i = 0; i < 180; ++i) if (!tick(s)) return false;
    (void)s.set_facing(cargo ? -1 : 0, cargo ? 0 : -1);
    if (!tick(s) || !s.snapshot().player_grounded ||
        s.snapshot().support_entity_id != (cargo ? 2952U : 2561U) || !s.snapshot().edge_drop_available) {
        const auto state = s.snapshot();
        std::cerr << "FAIL lowering receiver has no supported edge offer position="
                  << state.player_position.x << ',' << state.player_position.y << ',' << state.player_position.z
                  << " grounded=" << state.player_grounded << " support=" << state.support_entity_id
                  << " edge=" << state.edge_drop_available << '\n';
        return false;
    }
    (void)s.request_release();
    if (!tick(s) || s.snapshot().traversal_state != TraversalState::Lowering ||
        s.snapshot().traversal_hand_constraint_count != 2 ||
        s.snapshot().player_gravity_factor != 1.0) {
        std::cerr << "FAIL Drop must begin finite gravity-on receiver lowering\n";
        return false;
    }
    if (!immediate && !cargo) {
        for (int i = 0; i < 900; ++i) {
            const auto state = s.snapshot();
            if (!state.player_grounded && state.player_linear_velocity.y < -.2) break;
            if (!tick(s) || s.snapshot().traversal_state != TraversalState::Lowering) return false;
        }
        if (s.snapshot().player_grounded || s.snapshot().player_linear_velocity.y >= -.2) return false;
    }
    if (!immediate && cargo)
        for (int i = 0; i < 90; ++i) if (!tick(s)) return false;
    const auto before = s.snapshot();
    const bool accepted = s.request_release();
    if (!tick(s)) return false;
    const auto after = s.snapshot();
    const double error = std::abs(after.player_linear_velocity.y - before.player_linear_velocity.y +
                                  9.81 * Simulation::kFixedStepSeconds);
    bool ok = accepted && before.traversal_state == TraversalState::Lowering &&
        after.traversal_state == TraversalState::None &&
        after.traversal_hand_constraint_count == 0 && after.player_gravity_factor == 1.0 &&
        after.rejected_traversal_count == before.rejected_traversal_count;
    if (!immediate && !cargo) ok = ok && !after.player_grounded && error < .002;
    // Keep pressing toward the nearby lip: release must not immediately
    // auto-catch or leave a delayed lowering target active.
    (void)s.set_move_input(cargo ? -1 : 0, cargo ? 0 : -1);
    for (int i = 0; i < 6; ++i) {
        if (!tick(s)) return false;
        ok = ok && s.snapshot().traversal_state == TraversalState::None &&
            s.snapshot().traversal_hand_constraint_count == 0;
    }
    std::cout << (ok ? "PASS" : "FAIL") << " lowering_release cargo=" << cargo << " immediate=" << immediate
              << " accepted=" << accepted
              << " before_state=" << int(before.traversal_state)
              << " after_state=" << int(after.traversal_state)
              << " hands=" << after.traversal_hand_constraint_count
              << " rejected_delta=" << after.rejected_traversal_count - before.rejected_traversal_count
              << " gravity_velocity_error=" << error << '\n';
    return ok;
}

bool rotating_departure() {
    Simulation s;
    // Actual AS-026 entry shelf; development staging is not campaign proof.
    if (!s.debug_restart_at({10, 110.9, -174})) return false;
    for (int i = 0; i < 30; ++i) if (!tick(s)) return false;
    // Settled feet must place the lower rung inside actual hand reach;
    // the old -179.4 approach relied on residual coast toward the ladder.
    if (!walk(s, 10, -179.55)) return false;
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
            return passive_departure(s, "rotating_as026");
    }
    std::cerr << "FAIL rotating departure did not catch actual AS-026\n";
    return false;
}
}

int main() {
    const bool lowering_early = lowering_release(true, true);
    const bool lowering_delayed = lowering_release(true, false);
    const bool lowering_airborne = lowering_release(false, false);
    const bool rigid = rigid_departure();
    const bool deforming = deforming_departure();
    const bool jumping = deforming_departure(true);
    const bool traction = selected_support_traction();
    const bool walkoff = receiver_walk_off();
    const bool rotating = rotating_departure();
    if (!lowering_early || !lowering_delayed || !lowering_airborne || !rigid || !deforming || !jumping || !traction || !walkoff || !rotating) {
        std::cerr << "FAIL physical traversal: rigid=" << rigid
                  << " deforming=" << deforming << " rotating=" << rotating << '\n';
        return 1;
    }
}
