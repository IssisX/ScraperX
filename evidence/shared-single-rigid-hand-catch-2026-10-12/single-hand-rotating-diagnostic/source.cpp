#include "sim/simulation.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <limits>
#include <string>

using scraperx::sim::Simulation;
using scraperx::sim::TraversalState;

namespace {
double input_x=0,input_z=0;bool queued_action=false,queued_jump=false;
const char *phase="setup";
void log(const char* event,const scraperx::sim::Snapshot&v){
 std::cout<<"ROT,"<<event<<",phase="<<phase<<",tick="<<v.tick_index<<",input="<<input_x<<','<<input_z
 <<",action="<<queued_action<<",jump="<<queued_jump<<",com="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z
 <<",velocity="<<v.player_linear_velocity.x<<','<<v.player_linear_velocity.y<<','<<v.player_linear_velocity.z
 <<",grounded="<<v.player_grounded<<",support="<<v.support_entity_id<<",state="<<int(v.traversal_state)
 <<",entity="<<v.traversal_support_entity_id<<",hands="<<v.traversal_hand_constraint_count
 <<",gens="<<v.traversal_left_hand_generation<<','<<v.traversal_right_hand_generation
 <<",left="<<v.traversal_left_hand.x<<','<<v.traversal_left_hand.y<<','<<v.traversal_left_hand.z
 <<",right="<<v.traversal_right_hand.x<<','<<v.traversal_right_hand.y<<','<<v.traversal_right_hand.z
 <<",forces="<<v.traversal_left_hand_force.x<<','<<v.traversal_left_hand_force.y<<','<<v.traversal_left_hand_force.z<<','<<v.traversal_right_hand_force.x<<','<<v.traversal_right_hand_force.y<<','<<v.traversal_right_hand_force.z
 <<",radial="<<v.hanging_free_arc_active<<",span="<<v.hanging_actual_span_m<<",rest="<<v.hanging_commanded_span_m
 <<",work="<<v.traversal_command_work_bound_j<<",positive="<<v.traversal_actuator_positive_work_j
 <<",gravity="<<v.player_gravity_factor<<",deaths="<<v.death_count<<'\n';}
bool set_move(Simulation&s,double x,double z){input_x=x;input_z=z;return s.set_move_input(x,z);}
bool request_action(Simulation&s){queued_action=true;return s.request_traversal();}
bool request_jump(Simulation&s){queued_jump=true;return s.request_jump();}

bool tick(Simulation &s) {
    const auto before=s.snapshot();
 const bool accepted=s.advance_frame(Simulation::kFixedStepSeconds).accepted;
 const auto after=s.snapshot();
 if(before.traversal_state!=after.traversal_state || before.traversal_hand_constraint_count!=after.traversal_hand_constraint_count || before.traversal_support_entity_id!=after.traversal_support_entity_id){log("before_transition",before);log("after_transition",after);}
 queued_action=queued_jump=false;return accepted;
}

bool walk(Simulation &s, double x, double z) {
    for (int i = 0; i < 1500; ++i) {
        const auto state = s.snapshot();
        const auto p = state.player_position;
        const auto v = state.player_linear_velocity;
        const double dx = x - p.x, dz = z - p.z, d = std::hypot(dx, dz);
        if (d < .04 && state.player_grounded && std::hypot(v.x, v.z) < .15) {
            (void)set_move(s,0, 0);
            for (int j = 0; j < 30; ++j) if (!tick(s)) return false;
            return s.snapshot().player_grounded;
        }
        const double ix = dx * 1.8 - v.x * .28;
        const double iz = dz * 1.8 - v.z * .28;
        const double scale = std::max(1.0, std::hypot(ix, iz));
        (void)set_move(s,ix / scale, iz / scale);
        if (d > .001) (void)s.set_facing(dx, dz);
        if (!tick(s)) return false;
    }
    return false;
}

bool passive_departure(Simulation &s, const char *surface) {
    phase="passive_departure";log("helper_entry",s.snapshot());
    const auto work_start=s.snapshot().traversal_command_work_bound_j;
    (void)set_move(s,0, -1);
    for (int i = 0; i < 360; ++i) {
        if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) return false;
        if(i<3 || i==89 || i%30==29 || i==359)log("helper_sample",s.snapshot());
        // Acquire a genuinely ascending departure, rather than assuming every
        // compliant support settles into it at the same elapsed time.
        if (i >= 89 && s.snapshot().player_linear_velocity.y > .25) break;
    }
    const auto before = s.snapshot();log("helper_final",before);
    std::cout<<"ROT_WORK_DELTA,"<<before.traversal_command_work_bound_j-work_start<<'\n';
    const bool backend_ok = before.player_gravity_factor == 1.0 &&
        before.traversal_hand_constraint_count == 2 && before.traversal_command_work_bound_j > 0;
    if (!backend_ok) {
        std::cerr << "FAIL climb backend surface=" << surface
                  << " gravity=" << before.player_gravity_factor
                  << " constraints=" << before.traversal_hand_constraint_count << '\n';
        return false;
    }
    (void)set_move(s,0, 0);
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

bool rotating_departure() {
    Simulation s;
    // Actual AS-026 entry shelf; development staging is not campaign proof.
    if (!s.debug_restart_at({10, 110.9, -174})) return false;
    for (int i = 0; i < 30; ++i) if (!tick(s)) return false;
    // Settled feet must place the lower rung inside actual hand reach;
    // the old -179.4 approach relied on residual coast toward the ladder.
    if (!walk(s, 10, -179.55)) return false;
    (void)s.set_facing(0, -1);
    (void)request_action(s);
    if (!tick(s) || s.snapshot().traversal_state != TraversalState::Climbing) return false;
    (void)set_move(s,0, -1);
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
    phase="ordinary_rotating_jump";log("before_jump",s.snapshot());
    (void)set_move(s,-1, -.35);
    (void)request_jump(s);
    for (int i = 0; i < 135; ++i) {
        if (!tick(s)) return false;
        if(i<3 || i%30==29)log("jump_sample",s.snapshot());
        if (s.snapshot().traversal_state == TraversalState::Climbing &&
            s.snapshot().traversal_support_entity_id == 2960)
            return passive_departure(s, "rotating_as026");
    }
    std::cerr << "FAIL rotating departure did not catch actual AS-026\n";
    return false;
}
}
int main(){std::cout<<std::setprecision(12);return rotating_departure()?0:1;}
