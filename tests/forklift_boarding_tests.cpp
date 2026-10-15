#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace scraperx::sim;
namespace {
void require(bool ok, const char *why) {
    if (!ok) { std::cerr << "FAIL BOARDING " << why << std::endl; std::exit(1); }
}
void tick(Simulation &s, unsigned count=1) {
    for(unsigned i=0;i<count;++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"tick");
        require(s.snapshot().death_count==0,"no death");
    }
}
void observe(Simulation &s,const char *phase,int side) {
    const auto &p=s.snapshot();
    std::cout<<phase<<" side="<<side<<" p="<<p.player_position.x<<','<<p.player_position.y<<','<<p.player_position.z
             <<" grounded="<<p.player_grounded<<" support="<<p.support_entity_id<<" traversal="<<unsigned(p.traversal_state)
             <<" reachable="<<s.forklift_state().controls_reachable<<" pose_steps="<<p.step_up_pose_count<<std::endl;
}
}
int main() {
    for(int side : {-1,1}) {
        Simulation s;
        require(s.debug_restart_at({-100+2.*side,.9,-57.65}),"initial grade only");
        tick(s,45);
        require(s.snapshot().player_grounded&&!s.forklift_state().controls_reachable,"grade start");
        const auto poses=s.snapshot().step_up_pose_count;
        require(s.set_facing(-side,0),"face open doorway");
        bool boarded=false;
        for(unsigned i=0;i<540;++i) {
            const auto truck=s.kit_body_position(s.kit_body_index(2440));
            const auto p=s.snapshot().player_position;
            const auto v=s.snapshot().player_linear_velocity;
            require(s.set_move_input(std::clamp(1.2*(truck.x-p.x)-.45*v.x,-.3,.3),
                                    std::clamp(1.2*(truck.z-1.65-p.z)-.45*v.z,-.15,.15)),"ordinary walk");
            tick(s);
            if(i%90==0)observe(s,"WALK",side);
            if(s.forklift_state().controls_reachable) {boarded=true;break;}
        }
        observe(s,"BOARD_RESULT",side);
        require(boarded,"walk boards actual floor");
        require(s.snapshot().step_up_pose_count==poses,"no step pose writes");
        require(s.set_move_input(0,0),"stand");tick(s,90);
        require(s.forklift_state().controls_reachable&&s.snapshot().support_entity_id==2440,"retain floor 90ticks");
        require(s.set_move_input(.3*side,0),"walk off same side");
        bool left=false;
        for(unsigned i=0;i<360;++i) {
            tick(s);
            const auto p=s.snapshot();
            if(p.player_grounded&&p.player_position.y<1.&& !s.forklift_state().controls_reachable &&
               side*(p.player_position.x+100)>1.5) {left=true;break;}
        }
        require(s.set_move_input(0,0),"stop grade");tick(s,45);
        observe(s,"LEFT",side);
        require(left&&s.snapshot().traversal_state==TraversalState::None,"ordinary exit to grade");
        require(s.snapshot().step_up_pose_count==poses,"whole entry exit uses no pose steps");
    }
    std::cout<<"PASS BOARDING ordinary walk both sides, real floor, grade exit, no pose steps\n";
}
