#include "sim/simulation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
using namespace scraperx::sim;
namespace {
void require(bool value,const char *why) { if(!value) {std::cerr<<"FAIL "<<why<<'\n';std::exit(1);} }
void tick(Simulation &s,unsigned n=1) {for(unsigned i=0;i<n;++i)require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"native tick accepted");}
void leave_stock(Simulation &s) {
    require(s.debug_restart_at({35,1.77,-48}),"stage actual receiving stock");
    tick(s,45);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id==Simulation::kWorldSolidEntityId,"real stock footing before departure");
    require(s.set_facing(0,1) && s.set_move_input(0,.4),"walk toward real stock edge");
    bool departed=false;
    for(unsigned i=0;i<240;++i) {
        const auto before=s.snapshot();
        tick(s);
        if(before.foot_push_active && s.snapshot().foot_push_active)
            require(s.snapshot().foot_push_elapsed_seconds > before.foot_push_elapsed_seconds,
                    "an actual edge plant retains its original clock across grounded departure");
        if(!s.snapshot().player_grounded) {departed=true;break;}
    }
    require(departed,"ordinary capsule leaves real edge");
    require(s.snapshot().traversal_state==TraversalState::None,"departure is free of hand traversal");
}
void late_jump() {
    Simulation control;leave_stock(control);tick(control,2);
    double passive_rise=-100;
    for(unsigned i=0;i<20;++i){tick(control);passive_rise=std::max(passive_rise,control.snapshot().player_linear_velocity.y);}
    require(control.snapshot().foot_push_command_work_bound_j==0,"passive trailing plant supplies no commanded work");
    Simulation s;leave_stock(s);tick(s,2);
    require(!s.snapshot().player_grounded,"late press follows ordinary ground-contact loss");
    require(s.request_jump(),"late Jump input accepted");
    double rise=-100,work=0;
    for(unsigned i=0;i<20;++i) {tick(s);rise=std::max(rise,s.snapshot().player_linear_velocity.y);work=std::max(work,s.snapshot().foot_push_command_work_bound_j);}
    std::cout<<"late_jump peak_vy="<<rise<<" command_bound_J="<<work<<'\n';
    require(rise>1 && rise>passive_rise+1 && work>0,"late Jump extends an actual trailing foot plant");
}
void empty_air_rejects() {
    Simulation s;require(s.debug_restart_at({0,6,-20}),"stage empty air");
    require(s.request_jump(),"air intent accepted");tick(s,8);
    require(s.snapshot().player_linear_velocity.y<-.5 && !s.snapshot().foot_push_active,"empty air cannot manufacture a foot push");
}
void expired_plant_rejects() {
    Simulation s;leave_stock(s);tick(s,10);require(s.request_jump(),"expired late input buffered");
    double rise=-100;for(unsigned i=0;i<10;++i){tick(s);rise=std::max(rise,s.snapshot().player_linear_velocity.y);}
    require(rise<=0,"expired grace cannot push from stale footing");
}
void drop_cancels() {
    Simulation s;leave_stock(s);tick(s,2);
    require(s.request_jump(),"late Jump queued before explicit Drop");
    // No held attachment makes Release return false; it still queues native
    // cancellation. Its effect, rather than attachment availability, is tested.
    require(!s.request_release(),"empty-handed Drop reports no held attachment");
    double rise=-100;for(unsigned i=0;i<10;++i){tick(s);rise=std::max(rise,s.snapshot().player_linear_velocity.y);}
    require(rise<=0 && !s.snapshot().foot_push_active,"Drop cancels trailing plant and queued takeoff");
}
void restart_clears_plant() {
    Simulation s;leave_stock(s);
    require(s.debug_restart_at({0,6,-20}),"restart into clear air");
    require(s.request_jump(),"post-restart intent accepted");tick(s,8);
    require(s.snapshot().player_linear_velocity.y<-.5 && !s.snapshot().foot_push_active,"restart cannot reuse a prior physical plant");
}
void sprint_momentum_survives() {
    Simulation s;require(s.debug_restart_at({0,.9,-70}),"stage clear sprint lane");tick(s,30);
    require(s.set_facing(1,0) && s.set_move_input(1,0) && s.set_sprint_input(true),"sprint inputs accepted");
    // The real ground owner is capped by traction and3000W. Even its ideal
    // flat-ground envelope cannot reach7.5m/s in the old one-second fixture.
    // Await actual running speed, with a bounded two-second clear approach.
    unsigned acceleration_ticks=0;
    while(acceleration_ticks<180 && s.snapshot().player_linear_velocity.x<=7.5) {
        tick(s);++acceleration_ticks;
        require(s.snapshot().player_grounded,"sprint approach stays on actual clear ground");
    }
    const auto before=s.snapshot().player_linear_velocity.x;
    std::cout<<"sprint_preflight vx="<<before<<" x="<<s.snapshot().player_position.x
             <<" y="<<s.snapshot().player_position.y<<" grounded="<<s.snapshot().player_grounded
             <<" sprinting="<<s.snapshot().player_sprinting<<" acceleration_ticks="<<acceleration_ticks<<'\n';
    require(before>7.5,"real sprint reaches speed before takeoff");
    require(s.request_jump(),"running Jump requested");tick(s,3);
    require(s.set_move_input(0,0),"release air steering");tick(s,20);
    require(!s.snapshot().player_grounded && s.snapshot().player_linear_velocity.x>before-.2,"sprint-jump carries actual horizontal momentum");
}
}
int main(int argc,char **argv) {
    const auto mode=argc>1?std::string_view(argv[1]):std::string_view("all");
    if(mode=="late"||mode=="all")late_jump();
    if(mode=="air"||mode=="all")empty_air_rejects();
    if(mode=="expiry"||mode=="all")expired_plant_rejects();
    if(mode=="cancel"||mode=="all")drop_cancels();
    if(mode=="restart"||mode=="all")restart_clears_plant();
    if(mode=="sprint"||mode=="all")sprint_momentum_survives();
    std::cout<<"PASS physical parkour intent "<<mode<<'\n';
    return 0;
}
