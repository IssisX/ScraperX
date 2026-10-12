#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

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
using scraperx::sim::Vector3;
using scraperx::sim::Quaternion;
Vector3 rotate_vector(Quaternion q,Vector3 v) {
    const Vector3 t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
    return {v.x+q.w*t.x+q.y*t.z-q.z*t.y,v.y+q.w*t.y+q.z*t.x-q.x*t.z,v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
Vector3 inverse_rotate(Quaternion q,Vector3 v){return rotate_vector({-q.x,-q.y,-q.z,q.w},v);}
double length(Vector3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
bool material_fascia_intersection(Vector3 from,Vector3 to,Vector3 at,Quaternion rotation,const scraperx::sim::KitPart &part) {
    const auto local=[&](Vector3 p){auto v=inverse_rotate(rotation,{p.x-at.x,p.y-at.y,p.z-at.z});
        return inverse_rotate(part.rotation,{v.x-part.offset.x,v.y-part.offset.y,v.z-part.offset.z});};
    const auto a=local(from),b=local(to);const double starts[]{a.x,a.y,a.z},ends[]{b.x,b.y,b.z},halves[]{part.half.x,part.half.y,part.half.z};
    double lo=0,hi=1;
    for(int axis=0;axis<3;++axis){const double d=ends[axis]-starts[axis];
        if(std::abs(d)<1e-12){if(std::abs(starts[axis])>halves[axis])return false;}
        else {double first=(-halves[axis]-starts[axis])/d,last=(halves[axis]-starts[axis])/d;
            if(first>last)std::swap(first,last);
            lo=std::max(lo,first);hi=std::min(hi,last);if(lo>hi)return false;}}
    return hi>0&&lo<1;
}
double material_capsule_vertex_gap(const Simulation &s){const auto centre=s.snapshot().player_position;
    double closest=std::numeric_limits<double>::infinity();
    // Actual native net vertices against the standing capsule axis; this
    // logs geometric vertex overlap, not an invented contact impulse.
    for(const auto vertex:s.cargo_net_vertices()){const double dy=std::max(0.,std::abs(vertex.y-centre.y)-.55);
        closest=std::min(closest,std::sqrt(std::pow(vertex.x-centre.x,2)+dy*dy+std::pow(vertex.z-centre.z,2))-.35-.035);}
    return closest;
}
// Independent geometry witness: closest point on a real native triangle edge
// to the centre query aim. This observes material; it never grants a grip.
bool material_mesh_witness(const Simulation &s,Vector3 actor,double facing,Vector3 &point){
    const auto vertices=s.cargo_net_vertices();const auto indices=s.cargo_net_indices();
    const Vector3 aim{actor.x,actor.y+.45,actor.z+.4*facing};
    double best=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i+2<indices.size();i+=3)for(int edge=0;edge<3;++edge){
        const auto ia=indices[i+edge],ib=indices[i+(edge+1)%3];
        if(ia>=vertices.size()||ib>=vertices.size())return false;
        const auto a=vertices[ia],b=vertices[ib];const Vector3 d{b.x-a.x,b.y-a.y,b.z-a.z};
        const double dd=d.x*d.x+d.y*d.y+d.z*d.z;if(!std::isfinite(dd)||dd<1e-16)continue;
        const double t=std::clamp(((aim.x-a.x)*d.x+(aim.y-a.y)*d.y+(aim.z-a.z)*d.z)/dd,0.,1.);
        const Vector3 q{a.x+t*d.x,a.y+t*d.y,a.z+t*d.z};
        const double distance=length({q.x-aim.x,q.y-aim.y,q.z-aim.z});
        const double reach=length({q.x-actor.x,q.y-actor.y,q.z-actor.z});
        if(std::isfinite(distance)&&std::abs(q.x-aim.x)<=.34&&std::abs(q.y-aim.y)<=.34&&
           std::abs(q.z-aim.z)<=.39&&(q.z-actor.z)*facing>.16&&reach<=1.75&&distance<best){best=distance;point=q;}
    }
    return std::isfinite(best);
}
struct MaterialApproachResult{bool setup,passed;};
MaterialApproachResult material_approach(bool blocked,bool back_lower=false,bool fallback=false){Simulation s;
    const char *label=fallback?"visible_seed_occluded_upper_hand":blocked?"occluded_back_fascia":back_lower?"clear_back_below_fascia":"clear_front_fascia_behind";
    for(int i=0;i<180;++i)if(!tick(s))return {false,false}; // settle world before sole actor staging
    const double z=blocked||back_lower||fallback?-118.55:-117.35,y=fallback?9.25:back_lower?8.0:9.8,facing=blocked||back_lower||fallback?1:-1;
    if(!s.debug_restart_at({20,y,z})||!s.set_facing(0,facing)||!s.set_move_input(0,0)||!s.set_crouch_input(false)||!tick(s)){
        std::cerr<<"SETUP UNRESOLVED material="<<label<<" stage/one actual tick rejected\n";return {false,false};}
    const auto seed=s.snapshot();Vector3 witness{};
    const bool witnessed=material_mesh_witness(s,seed.player_position,facing,witness);
    const bool live_seed=seed.grip_available&&seed.grip_entity_id==1953;
    const Vector3 target=live_seed?seed.grip_point:witness;
    const Vector3 relative{target.x-seed.player_position.x,target.y-seed.player_position.y,target.z-seed.player_position.z};
    const auto frame=s.kit_body_index(2952);const auto vertices=s.cargo_net_vertices();
    std::cout<<"MATERIAL APPROACH case="<<label<<" position="<<seed.player_position.x<<','<<seed.player_position.y<<','<<seed.player_position.z
        <<" facing=0,"<<facing<<" grounded="<<seed.player_grounded<<" grip="<<seed.grip_available<<" grip_entity="<<seed.grip_entity_id
        <<" point="<<seed.grip_point.x<<','<<seed.grip_point.y<<','<<seed.grip_point.z<<" reach="<<length(relative)<<" native_vertices="<<vertices.size()<<'\n';
    std::cout<<"MATERIAL WITNESS case="<<label<<" actual_mesh_edge="<<witnessed<<" point="<<witness.x<<','<<witness.y<<','<<witness.z<<" using_live_seed="<<live_seed<<'\n';
    if(seed.player_grounded||!witnessed||(!blocked&&!live_seed)||!std::isfinite(length(relative))||length(relative)>1.75||frame==Simulation::kKitNone||vertices.empty()){
        std::cerr<<"SETUP UNRESOLVED material="<<label<<" no actual airborne in-reach material seed/frame\n";return {false,false};}
    double nearest=std::numeric_limits<double>::infinity();Vector3 nearest_vertex{};
    for(const auto vertex:vertices){const double d=length({vertex.x-target.x,vertex.y-target.y,vertex.z-target.z});if(d<nearest){nearest=d;nearest_vertex=vertex;}}
    scraperx::sim::KitPart fascia{};bool found=false;
    for(unsigned part=0;part<s.kit_body_part_count(frame);++part){const auto value=s.kit_body_part(frame,part);
        if(std::abs(value.half.x-2)<.001&&std::abs(value.half.y-.625)<.001&&std::abs(value.half.z-.07)<.001){fascia=value;found=true;break;}}
    if(!found||!std::isfinite(nearest)||nearest>.4){std::cerr<<"SETUP UNRESOLVED material="<<label<<" native fascia/material geometry absent\n";return {false,false};}
    const auto frame_at=s.kit_body_position(frame);const auto frame_rotation=s.kit_body_rotation(frame);
    const Vector3 centre_from{seed.player_position.x,seed.player_position.y+.45,seed.player_position.z};
    const bool centre_blocked=material_fascia_intersection(centre_from,target,frame_at,frame_rotation,fascia);
    bool hands_blocked=true,any_hand_blocked=false;bool hand_hits[2]{};
    for(int hand=0;hand<2;++hand){const double side=hand?1:-1;const Vector3 from{seed.player_position.x-.2*side*facing,seed.player_position.y+.45+.2*hand,seed.player_position.z};
        const bool hit=material_fascia_intersection(from,target,frame_at,frame_rotation,fascia);
        hand_hits[hand]=hit;hands_blocked&=hit;any_hand_blocked|=hit;}
    std::cout<<"MATERIAL GEOMETRY case="<<label<<" frame_position="<<frame_at.x<<','<<frame_at.y<<','<<frame_at.z
        <<" frame_rotation="<<frame_rotation.x<<','<<frame_rotation.y<<','<<frame_rotation.z<<','<<frame_rotation.w
        <<" fascia_local="<<fascia.offset.x<<','<<fascia.offset.y<<','<<fascia.offset.z
        <<" nearest_native_vertex="<<nearest_vertex.x<<','<<nearest_vertex.y<<','<<nearest_vertex.z<<" distance="<<nearest
        <<" centre_intersects_fascia="<<centre_blocked<<" both_hand_seed_approaches_intersect="<<hands_blocked<<" any_hand_seed_approach_intersects="<<any_hand_blocked<<" individual_hand_seed_intersects="<<hand_hits[0]<<','<<hand_hits[1]<<'\n';
    if(centre_blocked!=blocked||(blocked&&!hands_blocked)||(!blocked&&!fallback&&any_hand_blocked)||(fallback&&(hand_hits[0]||!hand_hits[1]))){
        std::cerr<<"SETUP UNRESOLVED material="<<label<<" live fascia does not match intended approach\n";return {false,false};}
    if(!s.request_traversal()||!tick(s))return {false,false};
    const auto caught=s.snapshot();
    std::cout<<"MATERIAL ACTION case="<<label<<" state="<<int(caught.traversal_state)<<" hand_count="<<caught.traversal_hand_constraint_count
        <<" hand_entity="<<caught.traversal_support_entity_id<<" gravity="<<caught.player_gravity_factor
        <<" hands="<<caught.traversal_left_hand.x<<','<<caught.traversal_left_hand.y<<','<<caught.traversal_left_hand.z<<';'
        <<caught.traversal_right_hand.x<<','<<caught.traversal_right_hand.y<<','<<caught.traversal_right_hand.z<<'\n';
    if(blocked||fallback){const bool passed=caught.traversal_state==TraversalState::None&&caught.traversal_hand_constraint_count==0;
        std::cout<<(passed?"PASS":"FAIL")<<" material_occluded_refusal actual_fascia=1 case="<<label<<'\n';return {true,passed};}
    const auto force_ok=[](Vector3 force){const double n=length(force);return std::isfinite(n)&&n<=1501;};
    if(caught.traversal_state!=TraversalState::Climbing||caught.traversal_support_entity_id!=1953||caught.traversal_hand_constraint_count!=2||
        caught.player_gravity_factor!=1||caught.player_grounded||!force_ok(caught.traversal_left_hand_force)||!force_ok(caught.traversal_right_hand_force)){
        std::cerr<<"FAIL material clear actual two-hand catch case="<<label<<'\n';return {true,false};}
    const auto before=s.snapshot();const double before_gap=material_capsule_vertex_gap(s);
    if(!std::isfinite(before_gap)||before_gap<=0){std::cerr<<"SETUP UNRESOLVED clear material Drop capsule/native-net gap="<<before_gap<<'\n';return {false,false};}
    if(!s.request_release()||!tick(s))return {true,false};
    const auto after=s.snapshot();const double after_gap=material_capsule_vertex_gap(s);
    const double vertical_error=std::abs(after.player_linear_velocity.y-before.player_linear_velocity.y+9.81*Simulation::kFixedStepSeconds);
    const double horizontal_error=std::hypot(after.player_linear_velocity.x-before.player_linear_velocity.x,after.player_linear_velocity.z-before.player_linear_velocity.z);
    const bool passed=after.traversal_state==TraversalState::None&&after.traversal_hand_constraint_count==0&&!after.player_grounded&&
        after.player_gravity_factor==1&&vertical_error<.002&&horizontal_error<.002;
    std::cout<<(passed?"PASS":"FAIL")<<" material_clear_catch_Drop case="<<label<<" before_vy="<<before.player_linear_velocity.y
        <<" after_vy="<<after.player_linear_velocity.y<<" before_xz="<<before.player_linear_velocity.x<<','<<before.player_linear_velocity.z
        <<" after_xz="<<after.player_linear_velocity.x<<','<<after.player_linear_velocity.z<<" release_position="<<after.player_position.x<<','<<after.player_position.y<<','<<after.player_position.z
        <<" contact_support="<<after.support_entity_id<<" before_native_vertex_gap="<<before_gap<<" after_native_vertex_gap="<<after_gap
        <<" after_air_force="<<after.air_control_force_n.x<<','<<after.air_control_force_n.y<<','<<after.air_control_force_n.z<<" vertical_error="<<vertical_error<<" horizontal_error="<<horizontal_error<<'\n';
    return {true,passed};
}
int material_approach_mode(){const auto blocked=material_approach(true);if(!blocked.setup)return 1;
    const auto front=material_approach(false);if(!front.setup)return 1;
    const auto back=material_approach(false,true);if(!back.setup)return 1;
    const auto fallback=material_approach(false,false,true);if(!fallback.setup)return 1;
    return blocked.passed&&front.passed&&back.passed&&fallback.passed?0:1;
}

}

int main(int argc,char **argv) {
    if(argc>1){if(std::string(argv[1])=="material_approach")return material_approach_mode();
        if(std::string(argv[1])=="material_front"){const auto result=material_approach(false);return result.setup&&result.passed?0:1;}
        if(std::string(argv[1])=="material_fallback"){const auto result=material_approach(false,false,true);return result.setup&&result.passed?0:1;}
        return 2;}
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
