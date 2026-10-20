#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;
namespace {
[[maybe_unused]] unsigned airborne_line_ticks=0, loaded_airborne_ticks=0, receiver_ticks=0;
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
Vector3 rail(const Simulation &) {
    return {-49,2.55,-57.55}; // Actual front face of the fixed bearing post.
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
[[maybe_unused]] void take_up(Simulation &sim) {
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
[[maybe_unused]] void retry_entry(Simulation &sim) {
    const auto p=sim.snapshot().player_position;
    walk(sim,p.x,-60.5,.4);walk(sim,-53,-60.5);walk(sim,-53,-59);walk(sim,-50.35,-59,.4);
}
void release(Simulation &sim) {
    require(sim.request_release(),"ordinary Drop",sim);tick(sim);
    require(!sim.traversal_line_state().gripping && sim.traversal_line_state().installed &&
        sim.snapshot().traversal_hand_constraint_count==0,"Drop removes hands and preserves world line",sim);
    report("released",sim);
}
[[maybe_unused]] void recover(Simulation &sim,double stand_off=0) {
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
    walk(sim,-80,-43.5);walk(sim,-54,-43.5);walk(sim,-54,-59);walk(sim,-53,-59);walk(sim,-50.35,-59,.4);walk(sim,-50.2,-58.6,.3);
    require(sim.request_traversal_line(TraversalLineCommandKind::Equip),"equip carried spool",sim);tick(sim);
    aim(sim,rail(sim));action(sim);
    require(sim.set_traversal_line_reel_input(-1),"pay out carried end for installation approach",sim);tick(sim,180);
    require(sim.set_traversal_line_reel_input(0),"neutral payout",sim);
    walk(sim,-50.35,-60.3,.35);walk(sim,-47.9,-60.3,.35);walk(sim,-47.9,-58.6,.3);
    aim(sim,{-47.9,2.55,-57.55});
    require(sim.traversal_line_state().can_pin_end,"unloaded carried fitting reaches second actual clamp",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action,sim.traversal_line_state().target),"clamp second end",sim);tick(sim,2);
    require(sim.traversal_line_state().two_ended && !sim.traversal_line_state().gripping,"two actual anchors retain installed line",sim);
    report("two-anchors",sim);
    walk(sim,-53,-60.3,.4);walk(sim,-53,-59,.4);walk(sim,-50.35,-59,.4);walk(sim,-50.2,-58.6,.3);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action),"grab actual material line",sim);tick(sim,2);
    require(sim.traversal_line_state().gripping,"material hands attached from entry",sim);
    require(sim.set_move_input(1,0),"deliberate short attempt",sim);
    bool missed=false;
    for(unsigned i=0;i<360;++i) {
        tick(sim);
        if(!sim.snapshot().player_grounded && sim.snapshot().player_position.x>-49.55) { release(sim); missed=true;break; }
    }
    require(missed,"short attempt lets go over real recovery grade",sim);
    settle_grade(sim);
    require(sim.snapshot().support_entity_id==1 && sim.traversal_line_state().two_ended,"miss keeps the installation for same-life retry",sim);
    retry_entry(sim);walk(sim,-50.2,-58.6,.3);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action),"retry grips retained material line",sim);tick(sim,2);
    require(sim.traversal_line_state().gripping,"same-life retry has real hands",sim);
    require(sim.set_move_input(1,0),"stick commands hand over hand",sim);
    unsigned regrips=0,loaded=0;
    for(unsigned i=0;i<900;++i) {
        tick(sim);const auto l=sim.traversal_line_state();const auto q=sim.snapshot();
        regrips=std::max(regrips,l.regrip_count);if(!q.player_grounded && l.gripping && l.tension_n>100)++loaded;
        if(i%30==0)report("crossing",sim);
        require(l.gripping && !l.broken,"crossing retains real finite hands",sim);
        if(q.player_position.x>-47.25)break;
    }
    require(regrips>=2 && loaded>=10,"alternating actual regrips carry airborne load",sim);
    release(sim);settle_grade(sim,true);walk(sim,-46.6,-58.6,.3);
    require(sim.snapshot().support_entity_id==1480 && sim.snapshot().player_position.x>-47.15,"actual crossing receiver footing",sim);
    require(sim.set_move_input(0,0),"stop after real receiver",sim);tick(sim,90);
    require(sim.traversal_line_state().two_ended,"dismount retains reusable line",sim);
    recover(sim,.4);
    walk(sim,sim.snapshot().player_position.x,-60.5,.4);walk(sim,-54,-60.5);walk(sim,-54,-43.5);walk(sim,-80,-43.5);walk(sim,-86,-43.5);walk(sim,-86,-46.6);walk(sim,-89.5,-46.6);
    aim(sim,{-89.5,2.3,-45.15});action(sim);
    require(sim.set_traversal_line_reel_input(-1),"pay out for alternative fixed-support placement",sim);tick(sim,100);
    require(sim.set_traversal_line_reel_input(0),"neutral payout",sim);walk(sim,-88.55,-46.6,.3);
    aim(sim,{-88.55,2.3,-45.15});
    require(sim.traversal_line_state().can_pin_end,"same spool accepts distinct points on ordinary fixed beam",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action,sim.traversal_line_state().target),"pin alternative second point",sim);tick(sim,2);
    require(sim.traversal_line_state().two_ended && sim.traversal_line_state().path_valid,"alternative material span installed without authored eyes",sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action),"grip alternative span",sim);tick(sim,2);
    require(sim.traversal_line_state().gripping,"alternative span uses actual material hands",sim);
    release(sim);settle_grade(sim);recover(sim,.7);
    require(sim.request_traversal_line(TraversalLineCommandKind::Unequip),"stow recovered reusable stock",sim);tick(sim,2);
    walk(sim,-89.5,-46.6,.4);walk(sim,-94,-46.6,.4);
    require(!sim.traversal_line_state().installed && sim.traversal_line_state().available_spools==1,"onward ordinary yard movement keeps recovered spool",sim);
    std::cout<<"PASS PORTABLE_LINE_CROSSING_PUBLIC public_arrival=1 regrips="<<regrips<<" loaded_airborne_ticks="<<loaded<<" receiver=1 miss_retry=1 local_recovery=1 alternative_mounting=1 onward=1 deaths=0 resets=0\n";
}
