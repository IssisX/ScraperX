#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using scraperx::sim::Simulation;
using scraperx::sim::TraversalState;
namespace {
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
    Simulation s;
    // Explicit supported-ring development staging, never campaign ascent proof.
    require(s.debug_restart_at({10,110.9,-174}),"supported +110m staging accepted",s);
    stop(s);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id==11,"real +110m footing",s);
    walk(s,10,-179.40);
    climb(s,114.7);
    std::cout << "PASS AS-026 launch shelf supported\n";
    walk(s,9.3,-180.25);
    (void)s.set_facing(0,-1); (void)s.set_move_input(-1,-0.35); (void)s.request_jump();
    bool grabbed=false;
    for(int i=0;i<135;++i) {
        tick(s); if(s.snapshot().traversal_state==TraversalState::Climbing && s.snapshot().traversal_support_entity_id==2960) { grabbed=true;break; }
    }
    require(grabbed,"ordinary jump catches dynamic ladder",s);
    std::cout << "CATCH y=" << s.snapshot().player_position.y << " x=" << s.snapshot().player_position.x << " qz=" << s.kit_body_rotation(s.kit_body_index(2960)).z << "\n";
    // Render state must use the same half-tick clock as player/Kit, without
    // advancing or mutating authoritative gameplay. Check both held hands.
    const auto previous=s.snapshot();tick(s);const auto current=s.snapshot();
    require(s.advance_frame(Simulation::kFixedStepSeconds*.5).steps_advanced==0,"half tick renders without physics",s);
    for(bool left:{false,true}) {
        const auto a=left?previous.traversal_left_hand:previous.traversal_right_hand;
        const auto b=left?current.traversal_left_hand:current.traversal_right_hand;
        const auto r=s.render_traversal_hand(left);
        require(std::abs(r.x-(a.x+b.x)*.5)<1e-7 && std::abs(r.y-(a.y+b.y)*.5)<1e-7 && std::abs(r.z-(a.z+b.z)*.5)<1e-7,"hands interpolate with native render clock",s);
    }
    require(s.snapshot().tick_index==current.tick_index,"render reads preserve authoritative tick",s);
    require(s.advance_frame(Simulation::kFixedStepSeconds*.5).accepted,"finish half tick",s);
    stop(s,45);
    const auto body=s.kit_body_index(2960);
    const auto initial=s.kit_body_rotation(body);
    double angular_span=0;
    (void)s.set_move_input(0,-1);
    bool arrived=false;
    for(int i=0;i<1500;++i) {
        tick(s);auto q=s.kit_body_rotation(body);if(i%45==0) std::cout << "TRACE t="<<i<<" qz="<<q.z<<" x="<<s.snapshot().player_position.x<<" vy="<<s.snapshot().player_linear_velocity.y<<" support="<<s.snapshot().traversal_support_entity_id<<" y="<<s.snapshot().player_position.y<<"\n";angular_span=std::max(angular_span,std::abs(q.z-initial.z));
        if(s.snapshot().player_position.y>119.8) {arrived=true;break;}
    }
    require(arrived,"loaded swinging climb reaches release height",s);
    require(angular_span>0.002,"real hinge moves during climbing",s);
    stop(s,1);
    for(int i=0;i<600 && s.kit_body_velocity(body).x<0.12;++i)tick(s);
    require(s.kit_body_velocity(body).x>0.1,"release is timed with rightward swing",s);
    (void)s.set_move_input(.7,.35);(void)s.request_jump();
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
