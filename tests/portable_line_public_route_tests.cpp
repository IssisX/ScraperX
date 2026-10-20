#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;
namespace {
unsigned airborne_line_ticks=0, loaded_airborne_ticks=0, receiver_ticks=0;
float maximum_tension=0;
void report(const char *stage,const Simulation &sim) {
    const auto s=sim.snapshot(); const auto l=sim.traversal_line_state();
    std::cout << "PORTABLE_LINE_PUBLIC " << stage << " tick=" << s.tick_index
        << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
        << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
        << " grounded=" << s.player_grounded << " support=" << s.support_entity_id
        << " traversal=" << unsigned(s.traversal_state) << " hands=" << s.traversal_hand_constraint_count
        << " installed=" << l.installed << " gripping=" << l.gripping << " broken=" << l.broken
        << " target=" << l.target << " failure=" << l.failure << " spools=" << l.available_spools
        << " tension_n=" << l.tension_n << " length_m=" << l.paid_length_m
        << " anchor=" << l.anchor_point.x << ',' << l.anchor_point.y << ',' << l.anchor_point.z
        << " fitting=" << l.grip_point.x << ',' << l.grip_point.y << ',' << l.grip_point.z
        << " can_recover=" << l.can_recover << '\n';
}
void require(bool value,const char *message,const Simulation &sim) {
    if (value) return;
    report(message,sim); std::cerr << "FAIL PORTABLE_LINE_PUBLIC " << message << '\n'; std::exit(1);
}
void tick(Simulation &sim,unsigned n=1) {
    while(n--) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,"ordinary fixed tick",sim);
        const auto s=sim.snapshot(); const auto l=sim.traversal_line_state();
        require(s.death_count==0 && s.player_gravity_factor==1,"same-life route retains ordinary gravity",sim);
        require(std::isfinite(l.paid_length_m) && l.paid_length_m>=0 && l.paid_length_m<=18 &&
            std::isfinite(l.tension_n) && l.tension_n>=0 &&
            (l.installed ? l.paid_length_m>=.6 && l.available_spools==0 : l.paid_length_m==0 && l.tension_n==0),
            "native line receipts match finite stock and length contract",sim);
        if(l.gripping && !s.player_grounded) { ++airborne_line_ticks; if(l.tension_n>100) ++loaded_airborne_ticks; }
        if(s.player_grounded && s.support_entity_id==1480 && s.player_position.x>-47.15) ++receiver_ticks;
        maximum_tension=std::max(maximum_tension,float(l.tension_n));
    }
}
void walk(Simulation &sim,double x,double z,double cap=.65) {
    for(unsigned i=0;i<2400;++i) {
        const auto s=sim.snapshot(); const auto p=s.player_position,v=s.player_linear_velocity;
        const double dx=x-p.x,dz=z-p.z;
        if(std::hypot(dx,dz)<.09 && s.player_grounded && std::hypot(v.x,v.z)<.18) {
            require(sim.set_move_input(0,0),"finish walking",sim);tick(sim,8);report("waypoint",sim);return;
        }
        require(sim.set_move_input(std::clamp(1.3*dx-.45*v.x,-cap,cap),std::clamp(1.3*dz-.45*v.z,-cap,cap)),"ordinary movement stick",sim);
        tick(sim);
        if(i && i%600==0) report("walk-progress",sim);
    }
    require(false,"walking reaches real waypoint",sim);
}
Vector3 rail(const Simulation &sim) {
    const auto index=sim.kit_body_index(2480); const auto p=sim.kit_body_position(index);
    const auto q=sim.kit_body_rotation(index);
    return {p.x+2*(q.x*q.y-q.z*q.w)*-1.2,p.y+(1-2*(q.x*q.x+q.z*q.z))*-1.2,
        p.z+2*(q.y*q.z+q.x*q.w)*-1.2};
}
void aim(Simulation &sim,Vector3 target) {
    const auto p=sim.snapshot().player_position;
    require(sim.set_rigging_aim(target.x-p.x,target.y-p.y-.6,target.z-p.z),"aim at actual broad beam",sim);
    tick(sim,3);
}
void action(Simulation &sim) {
    const auto l=sim.traversal_line_state();
    require(l.installed ? l.can_grab : l.target_valid,"shown surface or fitting within ordinary reach",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action,l.target),"ordinary contextual line Action",sim);
    tick(sim,2);
    require(sim.traversal_line_state().installed && sim.traversal_line_state().gripping &&
        sim.snapshot().traversal_hand_constraint_count==2,"Action installs real fitting and two finite hands",sim);
    report("held",sim);
}
void take_up(Simulation &sim) {
    require(sim.set_traversal_line_reel_input(1),"finite human takeup removes initial slack",sim);
    for(unsigned i=0;i<600 && sim.traversal_line_state().paid_length_m>.78;++i) {
        tick(sim);
        require(sim.traversal_line_state().gripping && !sim.traversal_line_state().broken,"takeup retains real line grip",sim);
    }
    require(sim.set_traversal_line_reel_input(0),"neutral reel after paid takeup",sim);tick(sim,30);
    require(sim.traversal_line_state().paid_length_m<=.78 && sim.traversal_line_state().reel_work_j>0,
        "finite paid takeup completes useful shorter line",sim);
    report("paid-takeup",sim);
}
void retry_entry(Simulation &sim) {
    const auto p=sim.snapshot().player_position;
    walk(sim,p.x,-60.5,.4);walk(sim,-53,-60.5);walk(sim,-53,-59);walk(sim,-50.35,-59,.4);
}
void release(Simulation &sim) {
    require(sim.request_release(),"ordinary Drop",sim);tick(sim);
    require(!sim.traversal_line_state().gripping && sim.traversal_line_state().installed &&
        sim.snapshot().traversal_hand_constraint_count==0,"Drop removes hands and preserves world line",sim);
    report("released",sim);
}
void recover(Simulation &sim,double stand_off=0) {
    const auto anchor=sim.traversal_line_state().anchor_point;
    const auto player=sim.snapshot().player_position;
    const double distance=std::hypot(anchor.x-player.x,anchor.z-player.z);
    const double approach=distance>0 ? std::max(0.0,distance-stand_off)/distance : 0;
    // A beam can be reached from beside its real front face. Packing never
    // requires walking through that face or standing exactly under the clamp.
    walk(sim,player.x+(anchor.x-player.x)*approach,player.z+(anchor.z-player.z)*approach,.3);
    require(sim.set_move_input(0,0),"stop below actual installed line",sim);
    for(unsigned i=0;i<1200 && !sim.traversal_line_state().can_recover;++i) tick(sim);
    require(sim.traversal_line_state().can_recover,"local unloaded fitting settles into recovery permission",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Recover),"separate ordinary PACK LINE",sim);tick(sim,2);
    const auto line=sim.traversal_line_state();
    require(!line.installed && line.available_spools==1 && !line.gripping,"local recovery returns same carried stock",sim);
    report("packed",sim);
}
void settle_grade(Simulation &sim,bool onward=false) {
    require(sim.set_move_input(onward ? 1 : 0,0),"ordinary landing or recovery input",sim);
    for(unsigned i=0;i<360;++i) {
        tick(sim);
        if(sim.snapshot().player_grounded) {require(sim.set_move_input(0,0),"stop after real landing",sim);tick(sim,30);report("landed",sim);return;}
    }
    require(false,"released player reaches real footing",sim);
}
}
int main() {
    std::cout << std::unitbuf;
    Simulation sim(InitialSpawn::FreightYardArrival);tick(sim,90);report("arrival",sim);
    walk(sim,-80,-43.5);walk(sim,-54,-43.5);walk(sim,-54,-59);walk(sim,-53,-59);walk(sim,-50.35,-59,.4);
    require(sim.snapshot().support_entity_id==1480,"ordinary stairs reach actual swing entry plinth",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Equip),"equip carried spool",sim);tick(sim);
    aim(sim,rail(sim));action(sim);take_up(sim);
    const auto fitting_body=sim.traversal_line_state().fitting_body;
    // Deliberately let go just beyond the entry, before the receiving pad.
    require(sim.set_move_input(1,0),"approach early missed release",sim);
    bool missed=false;
    for(unsigned i=0;i<360;++i) {
        tick(sim);
        if(!sim.snapshot().player_grounded && sim.snapshot().player_position.x>-49.35) {
            release(sim);missed=true;break;
        }
    }
    require(missed,"early Drop occurs during real airborne line support",sim);
    settle_grade(sim);
    require(sim.snapshot().support_entity_id==1 && sim.snapshot().player_position.x<-47.15,
        "miss lands on survivable grade before receiver",sim);
    report("miss-grade",sim);
    recover(sim);
    retry_entry(sim);
    aim(sim,rail(sim));action(sim);take_up(sim);
    require(sim.set_move_input(1,0),"finite hand lean and supported approach",sim);
    for(unsigned i=0;i<1200;++i) {
        tick(sim);
        const auto s=sim.snapshot();
        if(i%15==0)report("swing",sim);
        require(sim.traversal_line_state().gripping,"line remains gripped through real swing",sim);
        // Use observed motion to choose a deliberate release that clears the
        // receiver's real front face. This predicts input timing, never state.
        const double discriminant=s.player_linear_velocity.y*s.player_linear_velocity.y+
            2*9.81*(s.player_position.y-1.20);
        if(discriminant>0 && s.player_linear_velocity.x>.5) {
            const double flight=(s.player_linear_velocity.y+std::sqrt(discriminant))/9.81;
            const double landing_x=s.player_position.x+s.player_linear_velocity.x*flight;
            if(flight>0 && landing_x>-47.35 && landing_x<-45.8) {release(sim);break;}
        }
        if(!s.player_grounded) require(sim.set_move_input(s.player_linear_velocity.x>=0 ? 1 : -1,0),"phase-chosen finite human lean",sim);
        if(i==1199) require(false,"finite swing reaches deliberate release point",sim);
    }
    settle_grade(sim,true);
    require(sim.snapshot().support_entity_id==1480 && sim.snapshot().player_position.x>-47.15,
        "released swing reaches actual receiving pad",sim);
    tick(sim,90);require(receiver_ticks>=90 && loaded_airborne_ticks>=10,"swing and sustained receiving footing are observed",sim);
    report("receiver",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Unequip),"stow preserves installed world route",sim);tick(sim,2);
    require(sim.traversal_line_state().installed && !sim.traversal_line_state().equipped,
        "tool change leaves world installation in place",sim);
    walk(sim,-46.2,-59,.3);
    require(sim.request_traversal_line(TraversalLineCommandKind::Equip),"draw retained installed line for local recovery",sim);tick(sim,2);
    recover(sim);
    walk(sim,sim.snapshot().player_position.x,-60.5,.4);walk(sim,-54,-60.5);walk(sim,-54,-43.5);walk(sim,-80,-43.5);walk(sim,-86,-43.5);walk(sim,-86,-46.6);walk(sim,-89.5,-46.6);
    aim(sim,{-89.5,2.3,-45.15});action(sim);
    require(sim.traversal_line_state().fitting_body==fitting_body && sim.traversal_line_state().available_spools==0,
        "same finite spool and fitting reused at a different ordinary structure",sim);
    tick(sim,30);
    require(sim.set_traversal_line_reel_input(-1),"ordinary second-structure payout",sim);tick(sim,10);
    require(sim.set_traversal_line_reel_input(0),"neutral second-structure reel",sim);
    release(sim);settle_grade(sim);recover(sim,.7);
    require(sim.request_traversal_line(TraversalLineCommandKind::Unequip),"stow recovered tool",sim);tick(sim);
    walk(sim,-89.5,-46.6,.4);walk(sim,-94,-46.6,.4);
    require(!sim.traversal_line_state().installed && sim.traversal_line_state().available_spools==1,
        "onward normal yard movement preserves recovered stock",sim);
    std::cout << "PASS PORTABLE_LINE_PUBLIC public_arrival=1 real_airborne_line_ticks=" << airborne_line_ticks
        << " loaded_airborne_ticks=" << loaded_airborne_ticks << " receiver_footing_ticks=" << receiver_ticks << " maximum_tension_n=" << maximum_tension
        << " miss_grade_retry=1 local_recovery=1 second_structure_reuse=1 deaths=0 resets=0\n";
}
