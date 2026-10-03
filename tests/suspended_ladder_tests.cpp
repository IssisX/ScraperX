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
Vector3 local(Quaternion q,Vector3 origin,Vector3 point) {
    return rotate({-q.x,-q.y,-q.z,q.w},sub(point,origin));
}
void require(bool value, const char *why, const Simulation &s) {
    if(value) return;
    auto p=s.snapshot();
    std::cerr << "FAIL AS-026 " << why << " at=" << p.player_position.x << ',' << p.player_position.y << ',' << p.player_position.z << " traversal=" << int(p.traversal_state) << " grip=" << p.grip_entity_id << " support=" << p.support_entity_id << '\n';
    std::exit(1);
}
void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"fixed tick",s); }
void wait(Simulation &s, int n) { while(n-->0) tick(s); }
void stop(Simulation &s, int n=30) { (void)s.set_move_input(0,0); wait(s,n); }
void walk(Simulation &s,double x,double z,int limit=1000) {
    for(int i=0;i<limit;++i) {
        auto p=s.snapshot().player_position; double dx=x-p.x,dz=z-p.z,d=std::hypot(dx,dz);
        if(d<0.12) { stop(s); return; }
        double strength=std::min(1.0,d/0.5);
        (void)s.set_move_input(dx/d*strength,dz/d*strength);(void)s.set_facing(dx,dz);tick(s);
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
    walk(s,10,-179.40);
    climb(s,114.7);
    walk(s,9.3,-180.25);
    (void)s.set_facing(0,-1); (void)s.set_move_input(-1,-0.35); (void)s.request_jump();
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
    wait(miss,55);stop(miss,160);
    require(miss.snapshot().player_grounded && std::abs(miss.snapshot().player_position.y-110.9)<.12 && miss.snapshot().death_count==0,"wrong-way jump misses grip and recovers on catch deck",miss);
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
        const auto current=left?s.snapshot().traversal_left_hand:s.snapshot().traversal_right_hand;
        const auto grip=local(s.kit_body_rotation(body),s.kit_body_position(body),current);
        const auto expected=add(s.render_kit_body_position(body),rotate(s.render_kit_body_rotation(body),grip));
        require(distance(s.render_traversal_hand(left),expected)<.000015,
                "acquisition plants the new grip on the rendered support, without blending previous action",s);
    }
    std::cout << "CATCH y=" << s.snapshot().player_position.y << " x=" << s.snapshot().player_position.x << " qz=" << s.kit_body_rotation(s.kit_body_index(2960)).z << "\n";
    require(distance(s.kit_carry_grip_position(body),s.kit_body_position(body))>4,
            "rotating test body has a genuinely displaced compound COM",s);
    int retained_samples=0,transfer_samples=0;
    double maximum_chord_error=0,maximum_rendered_chord_distance=0;
    const auto sample_hands=[&]() {
        const auto before=s.snapshot();const auto origin=s.kit_body_position(body);const auto q=s.kit_body_rotation(body);
        require(s.advance_frame(Simulation::kFixedStepSeconds*(before.interpolation_alpha>.25?.5:1)).steps_advanced==1,"next interpolation endpoint",s);
        const auto after=s.snapshot();const auto new_origin=s.kit_body_position(body);const auto new_q=s.kit_body_rotation(body);
        require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==0,"half tick renders without physics",s);
        for(bool left:{false,true}) {
            const auto a=left?before.traversal_left_hand:before.traversal_right_hand;
            const auto b=left?after.traversal_left_hand:after.traversal_right_hand;
            const auto r=s.render_traversal_hand(left);
            const auto old_local=local(q,origin,a),new_local=local(new_q,new_origin,b);
            if(before.traversal_state==TraversalState::Climbing && after.traversal_state==TraversalState::Climbing &&
               before.traversal_support_entity_id==2960 && after.traversal_support_entity_id==2960) {
                if(distance(old_local,new_local)<.0001) {
                    const auto expected=add(s.render_kit_body_position(body),rotate(s.render_kit_body_rotation(body),scale(add(old_local,new_local),.5)));
                    maximum_chord_error=std::max(maximum_chord_error,distance(expected,scale(add(a,b),.5)));
                    maximum_rendered_chord_distance=std::max(maximum_rendered_chord_distance,distance(r,scale(add(a,b),.5)));
                    require(distance(r,expected)<.000015,"retained hand stays on SLERP-rendered rotating support",s);
                    ++retained_samples;
                } else if(distance(old_local,new_local)>.01) {
                    const auto expected=add(s.render_kit_body_position(body),rotate(s.render_kit_body_rotation(body),new_local));
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
    tick(s);
    require(distance(s.render_traversal_hand(true),{})==0 && distance(s.render_traversal_hand(false),{})==0,
            "departure immediately releases rendered planted hands",s);
    for(int i=0;i<150;++i) {tick(s);if(i%30==0)std::cout<<"FLIGHT x="<<s.snapshot().player_position.x<<" y="<<s.snapshot().player_position.y<<" z="<<s.snapshot().player_position.z<<"\n"; if(s.snapshot().player_grounded)break;}
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
