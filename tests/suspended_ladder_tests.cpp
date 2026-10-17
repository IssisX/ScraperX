#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using scraperx::sim::Simulation;
using scraperx::sim::TraversalState;
using scraperx::sim::Vector3;
using scraperx::sim::Quaternion;
namespace {
Vector3 add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 sub(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 scale(Vector3 a,double k) { return {a.x*k,a.y*k,a.z*k}; }
double distance(Vector3 a,Vector3 b) { auto d=sub(a,b);return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z); }
Vector3 rotate(Quaternion q,Vector3 v) {
    // Independent double-precision quaternion-vector product. Expected hands
    // are fixed in the visible shape-origin frame, not the displaced COM frame.
    const Vector3 t{2*(q.y*v.z-q.z*v.y),2*(q.z*v.x-q.x*v.z),2*(q.x*v.y-q.y*v.x)};
    return {v.x+q.w*t.x+q.y*t.z-q.z*t.y,
            v.y+q.w*t.y+q.z*t.x-q.x*t.z,
            v.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
void require(bool value, const char *why, const Simulation &s) {
    if(value) return;
    auto p=s.snapshot();
    std::cerr << "FAIL AS-026 " << why << " at=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z << " traversal=" << int(p.traversal_state) << " grip=" << p.grip_entity_id << " support=" << p.support_entity_id
        << " phase=" << int(p.traversal_phase) << " rejection=" << int(p.traversal_rejection_reason)
        << " plan=" << int(p.traversal_plan_kind) << ':' << p.traversal_plan_generation
        << " receiver=" << p.traversal_receiver_entity_id
        << " target=" << p.traversal_target_point.x << ',' << p.traversal_target_point.y << ',' << p.traversal_target_point.z
        << " ledge=" << p.traversal_ledge_point.x << ',' << p.traversal_ledge_point.y << ',' << p.traversal_ledge_point.z
        << " contact_tick=" << p.foot_contact_tick << " foot_source_tick=" << p.foot_push_source_contact_tick
        << " foot=" << p.foot_push_active << ':' << p.foot_push_support_entity_id << ':' << int(p.foot_push_stop_reason)
        << " foot_start=" << p.foot_push_start_count << " stroke=" << p.foot_push_stroke_m << ':' << p.foot_push_stroke_limit_m
        << " foot_elapsed=" << p.foot_push_elapsed_seconds << " foot_load=" << p.foot_push_last_load_n << ':' << p.foot_push_peak_load_n
        << " foot_work=" << p.foot_push_command_work_bound_j << " overlap=" << p.hand_foot_overlap_solved_ticks
        << " transfer=" << p.foot_transfer_count << '\n';
    for(bool left:{false,true}) {
        const auto anchor=s.traversal_hand_anchor(left);
        std::cerr << "HAND left=" << left << " valid=" << anchor.valid << " body=" << anchor.support
            << " generation=" << anchor.generation << " local=" << anchor.local.x << ',' << anchor.local.y << ',' << anchor.local.z << '\n';
    }
    std::exit(1);
}
void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"fixed tick",s); }
void wait(Simulation &s, int n) { while(n-->0) tick(s); }
void stop(Simulation &s, int n=30) { (void)s.set_move_input(0,0); wait(s,n); }
void walk(Simulation &s,double x,double z,int limit=1000) {
    for(int i=0;i<limit;++i) {
        const auto state=s.snapshot(); const auto p=state.player_position;
        const auto v=state.player_linear_velocity;
        const double dx=x-p.x,dz=z-p.z,d=std::hypot(dx,dz);
        if(d<0.12 && state.player_grounded && std::hypot(v.x,v.z)<.15) { stop(s); return; }
        const double ix=dx*1.8-v.x*.28,iz=dz*1.8-v.z*.28;
        const double scale=std::max(1.0,std::hypot(ix,iz));
        (void)s.set_move_input(ix/scale,iz/scale);
        if(d>.001)(void)s.set_facing(dx,dz);
        tick(s);
    }
    require(false,"walking connection",s);
}
void climb(Simulation &s,double height) {
    (void)s.set_facing(0,-1); (void)s.request_traversal();tick(s);
    require(s.snapshot().traversal_state==TraversalState::Climbing,"reachable CLIMB takes hold",s);
    (void)s.set_move_input(0,-1);
    for(int i=0;i<1200;++i) { tick(s); if(s.snapshot().player_grounded && s.snapshot().player_position.y>height) {stop(s);return;} }
    require(false,"climb reaches supported receiver",s);
}
void catch_ladder(Simulation &s) {
    require(s.debug_restart_at({10,110.9,-174}),"supported +110m staging accepted",s);
    stop(s);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id==11,"real +110m footing",s);
    // Deliberate ordinary approach, not coast after a distance-only arrival.
    walk(s,10,-179.55);
    climb(s,114.7);
    walk(s,9.3,-180.25);
    // Clear the outer rail and aim toward the rung while retaining deliberate catch intent.
    (void)s.set_facing(-0.20,-1); (void)s.set_move_input(-1,-0.20); (void)s.request_jump();
    for(int i=0;i<135;++i) {
        tick(s);
        if(s.snapshot().traversal_state==TraversalState::Climbing && s.snapshot().traversal_support_entity_id==2960) return;
    }
    require(false,"ordinary jump catches dynamic ladder",s);
}
}
int main() {
    Simulation miss;
    require(miss.debug_restart_at({10,114.9,-181}),"supported miss-path staging",miss);
    stop(miss);
    (void)miss.set_facing(0,-1);(void)miss.set_move_input(-1,.35);(void)miss.request_jump();
    wait(miss,55);
    // Neutral airborne input preserves the committed jump's momentum.
    // Recover by deliberately countersteering, using the ordinary controller.
    for(int i=0;i<160;++i) {
        const auto state=miss.snapshot();
        const auto v=state.player_linear_velocity;
        if(!state.player_grounded)
            (void)miss.set_move_input(std::clamp(-v.x*.4,-1.,1.),std::clamp(-v.z*.4,-1.,1.));
        else (void)miss.set_move_input(0,0);
        tick(miss);
    }
    (void)miss.set_move_input(0,0);
    require(miss.snapshot().player_grounded && std::abs(miss.snapshot().player_position.y-110.9)<.12 && miss.snapshot().death_count==0,"wrong-way jump misses grip and recovers on catch deck",miss);
    require(miss.snapshot().support_entity_id==1960 && miss.snapshot().grip_entity_id==0,
            "miss recovery uses actual catch-deck footing without an unintended grip",miss);
    require(miss.restart_checkpoint(),"checkpoint restores supported state",miss);stop(miss);
    require(miss.snapshot().player_grounded,"restored footing is physical",miss);
    std::cout<<"PASS AS-026 missed jump recovery and checkpoint restore\n";
    Simulation reset;
    catch_ladder(reset);
    tick(reset);
    require(reset.restart_checkpoint(),"restart while physically holding ladder",reset);
    require(reset.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==0,"restore render does not advance",reset);
    require(distance(reset.render_traversal_hand(true),{})==0 && distance(reset.render_traversal_hand(false),{})==0,
            "checkpoint clears old anchor render history",reset);
    require(distance(reset.render_player_position(),reset.snapshot().player_position)==0,"checkpoint snaps render pose to restored player",reset);
    Simulation s;
    // Explicit supported-ring development staging, never campaign ascent proof.
    catch_ladder(s);
    std::cout << "PASS AS-026 launch shelf supported\n";
    const auto body=s.kit_body_index(2960);
    for(bool left:{false,true}) {
        const auto anchor=s.traversal_hand_anchor(left);
        require(anchor.valid && !anchor.deforming && anchor.kit_body==body &&
                    anchor.support!=~std::uint32_t{0} && anchor.generation>0,
                "acquisition exposes the actual rigid ladder hand identity and local anchor",s);
        const auto expected=add(s.render_kit_body_position(body),rotate(s.render_kit_body_rotation(body),anchor.local));
        require(distance(s.render_traversal_hand(left),expected)<.000015,
                "acquisition plants the new grip on the rendered support, without blending previous action",s);
    }
    std::cout << "CATCH y=" << s.snapshot().player_position.y << " x=" << s.snapshot().player_position.x << " qz=" << s.kit_body_rotation(s.kit_body_index(2960)).z << "\n";
    require(distance(s.kit_carry_grip_position(body),s.kit_body_position(body))>4,
            "rotating test body has a genuinely displaced compound COM",s);
    int retained_samples=0,transfer_samples=0;
    double maximum_chord_error=0,maximum_rendered_chord_distance=0;
    const auto sample_hands=[&]() {
        const auto before=s.snapshot();
        const Simulation::RenderHandAnchor before_anchors[]{s.traversal_hand_anchor(true),s.traversal_hand_anchor(false)};
        require(s.advance_frame(Simulation::kFixedStepSeconds*(before.interpolation_alpha>.25?.5:1)).steps_advanced==1,"next interpolation endpoint",s);
        const auto after=s.snapshot();const auto new_origin=s.kit_body_position(body);
        require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==0,"half tick renders without physics",s);
        for(bool left:{false,true}) {
            const auto a=left?before.traversal_left_hand:before.traversal_right_hand;
            const auto b=left?after.traversal_left_hand:after.traversal_right_hand;
            const auto r=s.render_traversal_hand(left);
            const auto &old_anchor=before_anchors[left?0:1];
            const auto anchor=s.traversal_hand_anchor(left);
            if(before.traversal_state==TraversalState::Climbing && after.traversal_state==TraversalState::Climbing &&
               before.traversal_support_entity_id==2960 && after.traversal_support_entity_id==2960) {
                require(old_anchor.valid && anchor.valid && !old_anchor.deforming && !anchor.deforming &&
                            old_anchor.kit_body==body && anchor.kit_body==body &&
                            old_anchor.support==anchor.support,
                        "sampled hands retain the exact rigid ladder support",s);
                // World endpoints have float spacing up to15micrometers here.
                // Use the native local receipt, then independently evaluate the
                // quaternion transform in double precision at the render clock.
                if(old_anchor.generation==anchor.generation) {
                    const auto expected=add(s.render_kit_body_position(body),rotate(s.render_kit_body_rotation(body),scale(add(old_anchor.local,anchor.local),.5)));
                    maximum_chord_error=std::max(maximum_chord_error,distance(expected,scale(add(a,b),.5)));
                    maximum_rendered_chord_distance=std::max(maximum_rendered_chord_distance,distance(r,scale(add(a,b),.5)));
                    require(distance(r,expected)<.000015,"retained hand stays on SLERP-rendered rotating support",s);
                    ++retained_samples;
                } else {
                    require(anchor.generation>old_anchor.generation,"regrip advances actual hand identity",s);
                    const auto expected=add(s.render_kit_body_position(body),rotate(s.render_kit_body_rotation(body),anchor.local));
                    require(distance(r,expected)<.000015,
                            "regrip plants the new local hold on the rendered support instead of the physics endpoint",s);
                    ++transfer_samples;
                }
            }
        }
        const auto unchanged=s.snapshot();
        require(unchanged.tick_index==after.tick_index && distance(unchanged.player_position,after.player_position)==0 &&
                distance(unchanged.traversal_left_hand,after.traversal_left_hand)==0 && distance(s.kit_body_position(body),new_origin)==0,
                "render reads preserve native player, hand, body and tick",s);
    };
    sample_hands();
    require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==1,"finish half tick",s);
    stop(s,45);
    const auto initial=s.kit_body_rotation(body);
    double angular_span=0;
    (void)s.set_move_input(0,-1);
    bool arrived=false;
    for(int i=0;i<1500;++i) {
        sample_hands();auto q=s.kit_body_rotation(body);if(i%45==0) std::cout << "TRACE t="<<i<<" qz="<<q.z<<" x="<<s.snapshot().player_position.x<<" vy="<<s.snapshot().player_linear_velocity.y<<" support="<<s.snapshot().traversal_support_entity_id<<" y="<<s.snapshot().player_position.y<<"\n";angular_span=std::max(angular_span,std::abs(q.z-initial.z));
        if(s.snapshot().player_position.y>119.8) {arrived=true;break;}
    }
    require(arrived,"loaded swinging climb reaches release height",s);
    require(angular_span>0.002,"real hinge moves during climbing",s);
    std::cout << "HAND_SAMPLES retained=" << retained_samples << " transfers=" << transfer_samples << " max_chord=" << maximum_chord_error << '\n';
    require(retained_samples>50 && transfer_samples>0,"actual retained anchors and same-support regrips were exercised",s);
    require(maximum_rendered_chord_distance>.000001,"rendered retained anchors follow support rotation rather than exact world-space LERP",s);
    std::cout << "PASS hand support-frame interpolation retained=" << retained_samples << " regrips=" << transfer_samples << " maximum_chord_error_m=" << maximum_chord_error << " rendered_chord_distance_m=" << maximum_rendered_chord_distance << '\n';
    require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==1,"finish route sampling half tick",s);
    stop(s,1);
    for(int i=0;i<600 && s.kit_body_velocity(body).x<0.12;++i)tick(s);
    require(s.kit_body_velocity(body).x>0.1,"release is timed with rightward swing",s);
    (void)s.set_move_input(.7,.35);(void)s.request_jump();
    // Production Jump commands a0.20s finite hand stroke. Contact remains
    // authoritative through that stroke, then the following fixed tick releases.
    const int departure_tick_budget=static_cast<int>(std::ceil(.20/Simulation::kFixedStepSeconds))+1;
    bool departed=false;
    for(int i=0;i<departure_tick_budget;++i) {
        tick(s);
        const auto state=s.snapshot();
        require(state.player_gravity_factor==1.0 && state.death_count==0,
                "finite departure stroke keeps ordinary gravity and the same life",s);
        if(state.traversal_state==TraversalState::None) {
            departed=true;
            break;
        }
        require(state.traversal_state==TraversalState::Climbing &&
                    state.traversal_support_entity_id==2960 && state.traversal_hand_constraint_count==2 &&
                    s.traversal_hand_anchor(true).valid && s.traversal_hand_anchor(false).valid &&
                    distance(s.render_traversal_hand(true),{})>0 && distance(s.render_traversal_hand(false),{})>0,
                "finite departure stroke retains actual attached and rendered hands",s);
    }
    require(departed,"finite hand stroke departs within0.20s plus one release tick",s);
    require(distance(s.render_traversal_hand(true),{})==0 && distance(s.render_traversal_hand(false),{})==0,
            "actual departure immediately releases rendered planted hands",s);
    const auto require_physical_receipt=[&]() {
        const auto state=s.snapshot();
        require(state.player_gravity_factor==1.0 && state.death_count==0 &&
                    std::isfinite(state.traversal_command_work_bound_j) && state.traversal_command_work_bound_j>=0 &&
                    std::isfinite(state.traversal_actuator_positive_work_j) && state.traversal_actuator_positive_work_j>=0 &&
                    std::isfinite(state.traversal_actuator_absorbed_work_j) && state.traversal_actuator_absorbed_work_j>=0 &&
                    distance(state.traversal_left_hand_force,{})<=1500.01 &&
                    distance(state.traversal_right_hand_force,{})<=1500.01,
                "receiver handoff preserves gravity and finite bounded hand forces and work",s);
        if(state.foot_push_active)
            require(state.foot_push_peak_load_n<=3500.01 && state.foot_push_stroke_m<=.300001 &&
                        state.foot_push_command_work_bound_j<=1285.62501,
                    "receiver top-out preserves existing finite foot limits",s);
    };
    bool caught_receiver=false;
    const double approach_braking_before=s.snapshot().air_control_command_absorbed_work_j;
    for(int i=0;i<150;++i) {
        const auto before=s.snapshot();
        // Reach the existing inboard stance with ordinary bounded air control.
        // Neutral radial hands carry tension; they do not cancel the committed
        // sideways flight. Reuse walk's position/velocity feedback, capped at
        // the original0.7 stick throw, before either hand acquires the lip.
        const double side=std::clamp((8.25-before.player_position.x)*1.8-
            before.player_linear_velocity.x*.28,-.7,.7);
        require(s.set_move_input(side,.35),"ordinary air input brakes the receiver approach",s);
        require(s.set_facing(9-before.player_position.x,-178.45-before.player_position.z),
                "ordinary airborne facing reaches toward the receiver lip",s);
        tick(s);
        require_physical_receipt();
        const auto state=s.snapshot();
        if(i%30==0)std::cout<<"FLIGHT x="<<state.player_position.x<<" y="<<state.player_position.y<<" z="<<state.player_position.z
            <<" traversal="<<int(state.traversal_state)<<" support="<<state.traversal_support_entity_id<<"\n";
        if(state.traversal_state==TraversalState::Hanging) {
            require(state.traversal_support_entity_id==1960 && state.traversal_hand_constraint_count==2 &&
                        s.traversal_hand_anchor(true).valid && s.traversal_hand_anchor(false).valid,
                    "descending reach catches the actual receiver lip with two real hands",s);
            caught_receiver=true;
            break;
        }
        if(state.player_grounded)break;
    }
    require(caught_receiver,"swing departure reaches a real receiver lip catch within the flight budget",s);
    require(s.snapshot().air_control_command_absorbed_work_j>approach_braking_before,
            "receiver approach actually brakes through finite ordinary air control",s);
    const auto catch_receipt=[&](int ordinal) {
        const auto v=s.snapshot();
        std::cout<<"RECEIVER_CATCH ordinal="<<ordinal<<" tick="<<v.tick_index
            <<" position="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z
            <<" velocity="<<v.player_linear_velocity.x<<','<<v.player_linear_velocity.y<<','<<v.player_linear_velocity.z
            <<" traversal="<<int(v.traversal_state)<<" support="<<v.traversal_support_entity_id
            <<" hands="<<v.traversal_hand_constraint_count
            <<" reach="<<distance(v.player_position,v.traversal_left_hand)<<','<<distance(v.player_position,v.traversal_right_hand)
            <<" force="<<distance(v.traversal_left_hand_force,{})<<','<<distance(v.traversal_right_hand_force,{})
            <<" anchor0="<<v.traversal_left_hand.x<<','<<v.traversal_left_hand.y<<','<<v.traversal_left_hand.z
            <<" anchor1="<<v.traversal_right_hand.x<<','<<v.traversal_right_hand.y<<','<<v.traversal_right_hand.z
            <<" normal="<<v.traversal_normal.x<<','<<v.traversal_normal.y<<','<<v.traversal_normal.z
            <<" command_j="<<v.traversal_command_work_bound_j
            <<" supplied_j="<<v.traversal_actuator_positive_work_j
            <<" absorbed_j="<<v.traversal_actuator_absorbed_work_j<<'\n';
    };
    catch_receipt(-1);
    // Let the real radial hang absorb the falling catch before commanding
    // the Cartesian shimmy. Crouch commands the existing paid radial shortening.
    require(s.set_crouch_input(true),"paid radial shortening uses ordinary crouch input",s);
    require(s.set_move_input(0,0),"neutral input retains the finite physical catch",s);
    bool descent_arrested=false;
    for(int i=0;i<2*90 && !descent_arrested;++i) {
        tick(s);
        require_physical_receipt();
        const auto state=s.snapshot();
        catch_receipt(i);
        require(state.traversal_state==TraversalState::Hanging && state.traversal_support_entity_id==1960 &&
                    state.traversal_hand_constraint_count==2,
                "actual receiver hands arrest the fall before lateral transfer",s);
        const auto velocity=state.player_linear_velocity;
        descent_arrested=velocity.y>=0;
    }
    require(descent_arrested,"finite paid receiver catch arrests descent before shimmy",s);
    // Keep paid shortened posture while approaching a genuine mantle stance.
    const auto caught=s.snapshot();
    const auto caught_left=s.traversal_hand_anchor(true),caught_right=s.traversal_hand_anchor(false);
    require(s.set_facing(0,1) && s.set_move_input(1,0),"ordinary shimmy moves inboard along the receiver lip",s);
    bool shimmy_inboard=false;
    for(int i=0;i<3*90;++i) {
        const auto state=s.snapshot();
        require(state.traversal_state==TraversalState::Hanging && state.traversal_support_entity_id==1960 &&
                    state.traversal_hand_constraint_count==2 &&
                    s.traversal_hand_anchor(true).valid && s.traversal_hand_anchor(false).valid,
                "inboard shimmy retains the actual receiver lip and both hands",s);
        const bool actual_regrip=s.traversal_hand_anchor(true).generation>caught_left.generation ||
            s.traversal_hand_anchor(false).generation>caught_right.generation;
        const double moved_anchor=.5*(state.traversal_left_hand.x+state.traversal_right_hand.x)-
            .5*(caught.traversal_left_hand.x+caught.traversal_right_hand.x);
        if(state.player_position.x>=8.25 && state.traversal_ledge_point.x>=8.25 &&
           distance(add(state.player_position,{0,.45,0}),state.traversal_ledge_point)<=1.05 &&
           actual_regrip && moved_anchor>.1) {shimmy_inboard=true;break;}
        tick(s);
        require_physical_receipt();
    }
    require(shimmy_inboard,"real receiver shimmy clears the capsule footprint from the corner within3seconds",s);
    require((s.traversal_hand_anchor(true).generation>caught_left.generation ||
             s.traversal_hand_anchor(false).generation>caught_right.generation) &&
                (.5*(s.snapshot().traversal_left_hand.x+s.snapshot().traversal_right_hand.x)-
                 .5*(caught.traversal_left_hand.x+caught.traversal_right_hand.x))>.1,
            "inboard shimmy uses actual hand regrips and material anchor movement",s);
    require(s.set_move_input(0,1) && s.request_traversal(),
            "one ordinary Action requests top-out after the real inboard shimmy",s);
    bool receiver_topout=false;
    // This deep catch needs backoff, rise, then forward transfer. Its measured
    // mean hand-target path is3.624m; even the2.2m/s target limit needs1.65s,
    // before finite-force response and real footing settle. Use the existing
    // three-second maneuver scale; shallow top-out tests retain153ticks.
    for(int i=0;i<3*90;++i) {
        tick(s);
        require_physical_receipt();
        const auto state=s.snapshot();
        if(state.player_grounded && state.support_entity_id==1960 && state.player_position.y>119.7 &&
           state.traversal_state==TraversalState::None && state.traversal_hand_constraint_count==0) {
            require(state.foot_transfer_count>caught.foot_transfer_count,
                    "receiver footing follows an actual physical hand-to-foot transfer",s);
            receiver_topout=true;
            break;
        }
    }
    require(receiver_topout,"real receiver hand-to-foot top-out completes within3seconds after the deep catch",s);
    require(s.set_crouch_input(false),"ordinary crouch release after actual top-out",s);
    (void)s.set_move_input(.7,.35);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id==1960,"swing release reaches offset receiver",s);
    stop(s);
    std::cout<<"RECEIVER "<<s.snapshot().player_position.x<<","<<s.snapshot().player_position.y<<","<<s.snapshot().player_position.z<<"\n";
    require(s.snapshot().player_grounded && s.snapshot().player_position.y>119.7,"receiver braking holds footing",s);
    walk(s,9,-178.45);
    (void)s.set_facing(0,1);(void)s.request_traversal();tick(s);
    std::cout<<"FINAL_GRIP state="<<int(s.snapshot().traversal_state)<<" xyz="<<s.snapshot().player_position.x<<","<<s.snapshot().player_position.y<<","<<s.snapshot().player_position.z<<"\n";
    (void)s.set_move_input(0,1);
    for(int i=0;i<500;++i){tick(s);if(s.snapshot().player_grounded && s.snapshot().player_position.y>121.7)break;}
    stop(s);walk(s,9,-174.5);
    auto p=s.snapshot();
    require(p.player_grounded && p.support_entity_id==11 && std::abs(p.player_position.y-121.9)<0.12,"supported +121m tower exit",s);
    require(p.death_count==0,"route has no deaths",s);
    std::cout << "PASS AS-026 suspended_ladder support=11 walking_surface_m=121 deaths=0 swing_quaternion_span=" << angular_span << '\n';
}
