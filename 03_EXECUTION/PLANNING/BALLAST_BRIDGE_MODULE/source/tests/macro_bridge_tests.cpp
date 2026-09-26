#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;

namespace {
void require(bool ok, const char *why) {
    if (!ok) { std::cerr << "FAIL macro bridge: " << why << '\n'; std::exit(1); }
}
void step(Simulation &sim, double dt=Simulation::kFixedStepSeconds) {
    require(sim.advance_frame(dt).accepted,"authoritative clock accepts input partition");
}
void trace(const Simulation &sim, const char *label) {
    auto s=sim.snapshot();
    std::cout<<label<<" t="<<s.simulation_time_seconds<<" player="<<s.player_position.x<<","<<s.player_position.y<<","<<s.player_position.z
        <<" support="<<s.support_entity_id<<" handle="<<s.macro_handle_angle<<" bridge="<<s.macro_bridge_angle
        <<" nose="<<s.macro_nose_position.y<<" drop="<<s.macro_ballast_drop<<" v="<<s.macro_ballast_speed
        <<" peak="<<s.macro_peak_speed<<" tension="<<s.macro_rope_tension<<" brake_J="<<s.macro_brake_work
        <<" source_J="<<s.macro_source_work<<" deck_J="<<s.macro_deck_potential<<" kinetic_J="<<s.macro_kinetic_energy<<'\n';
}
bool walk(Simulation &sim, double x, double z, double budget=20) {
    for(int i=0;i<int(budget*90);++i) {
        const auto p=sim.snapshot().player_position;
        const double dx=x-p.x,dz=z-p.z,d=std::hypot(dx,dz);
        if(d<0.06) { (void)sim.set_move_input(0,0);step(sim,0.12);return true; }
        const double throttle=std::min(1.0,d);
        (void)sim.set_move_input(dx/d*throttle,dz/d*throttle);
        (void)sim.set_facing(dx/d,dz/d);step(sim);
    }
    (void)sim.set_move_input(0,0);trace(sim,"walk timeout");return false;
}
void grab(Simulation &sim) {
    (void)sim.set_move_input(0,0);(void)sim.set_facing(0,-1);step(sim,0.3);
    require(sim.snapshot().carry_target_entity_id==2018,"reachable physical T-grip is offered");
    (void)sim.request_pick_up();step(sim,0.15);
    require(sim.snapshot().carrying_entity_id==2018,"normal grab attaches grip");
}
void pull(Simulation &sim, double angle, double throttle, double dt) {
    const auto start=sim.snapshot().simulation_time_seconds;
    while(sim.snapshot().macro_handle_angle<angle && sim.snapshot().simulation_time_seconds-start<8) {
        (void)sim.set_move_input(0,throttle);(void)sim.set_facing(0,-1);step(sim,dt);
        require(sim.snapshot().carrying_entity_id==2018,"ordinary straight pull must not lose grip");
    }
    (void)sim.set_move_input(0,0);
    require(sim.snapshot().macro_handle_angle>=angle,"trip cable physically turns brake arm");
}
void release(Simulation &sim) {
    (void)sim.request_set_down();step(sim,0.1);
    require(sim.snapshot().carrying_entity_id==0,"normal release frees hands");
}
void operate(Simulation &sim, double throttle=0.22, double dt=Simulation::kFixedStepSeconds) {
    require(walk(sim,7,-83.1),"reach handle from unchanged exterior spawn");
    grab(sim);pull(sim,1.05,throttle,dt);release(sim);
}
void settle(Simulation &sim) {
    const double start=sim.snapshot().simulation_time_seconds;
    bool arrived=false;
    while(sim.snapshot().simulation_time_seconds-start<15) {
        step(sim);
        const auto s=sim.snapshot();
        require(s.macro_peak_speed<1.5,"finite governor bounds actual source speed");
        require(s.macro_kinetic_energy+s.macro_deck_potential<=s.macro_source_work+3000,
                "measured potential and kinetic energy cannot exceed released source work");
        if(s.macro_nose_position.y>10.70 && std::abs(s.macro_ballast_speed)<0.03) {arrived=true;break;}
    }
    require(arrived,"real contacts seat bridge within visible stroke budget");
    const double arrival_y=sim.snapshot().macro_nose_position.y;step(sim,3);
    const auto at=sim.snapshot();
    require(std::abs(at.macro_nose_position.y-arrival_y)<0.03,
            "passive seated bridge remains stable without an arrival flag");
    require(at.macro_bridge_angle<0.345,"visible caps stop bridge before emergency hinge limit");
    require(at.macro_rope_tension>80000 && at.macro_rope_tension<120000,"hanging mass maintains finite holding tension");
    require(at.macro_brake_work<-200000,"brake dissipates measured work");
    const double unaccounted=at.macro_source_work-at.macro_deck_potential+at.macro_brake_work-at.macro_kinetic_energy;
    require(std::abs(unaccounted)<0.05*at.macro_source_work,"source, load, kinetic and brake work close within 5 percent");
    trace(sim,"PASS scraperx_macro stroke");
}
void ascent(Simulation &sim) {
    require(walk(sim,14,-90),"ordinary approach reaches bridge foot");
    require(walk(sim,14,-121.5),"walk entire real moving-support bridge");
    require(sim.snapshot().support_entity_id==2016,"bridge itself supports player near nose");
    require(walk(sim,14,-123.7),"cross nose onto fixed receiver without jumping");
    require(sim.snapshot().support_entity_id==1016 && sim.snapshot().player_position.y>11.5,
            "player stands on actual receiver at deck 1");
    require(walk(sim,23,-126),"walk off receiver onto existing tower deck");
    require((sim.snapshot().support_entity_id==11 || sim.snapshot().support_entity_id==51) &&
            sim.snapshot().player_position.y>11.5 && sim.snapshot().death_count==0,
            "macro exit joins retained tower geometry alive");
    trace(sim,"PASS scraperx_macro normal route");
}
}

int main() {
    {
        Simulation idle;step(idle,45);
        require(std::abs(idle.snapshot().macro_ballast_drop)<0.02 && idle.snapshot().macro_nose_position.y<0.3,
                "timer and gravity cannot bypass closed finite brake");
        require(idle.snapshot().macro_handle_angle<0.15,"gravity cannot auto-pull resting grip");
        std::cout<<"PASS scraperx_macro idle: seconds=45 spontaneous_release=0\n";
    }
    Simulation sim;step(sim,1);operate(sim);settle(sim);ascent(sim);
    // Ascend the retained ordinary stairs, then take a real lethal fall. The
    // restored checkpoint must preserve the earned bridge, control and cable.
    require(walk(sim,21.5,-171.5),"existing east deck connects to second flight");
    require(walk(sim,20,-171.5),"second flight approach");
    require(walk(sim,-21,-171.5),"walk second flight");
    require(walk(sim,-21.5,-128.5),"west deck connects third flight");
    require(walk(sim,-20,-128.5),"third flight approach");
    require(walk(sim,21,-128.5),"walk third flight");
    require(sim.snapshot().player_position.y>33.5,"reach actual 33 m deck for fatal-fall proof");
    const double seated=sim.snapshot().macro_nose_position.y;
    (void)sim.set_facing(1,0);(void)sim.set_move_input(1,0);(void)sim.request_jump();
    for(int i=0;i<1200 && sim.snapshot().death_count==0;++i) step(sim);
    (void)sim.set_move_input(0,0);step(sim,0.3);
    require(sim.snapshot().death_count==1,"real 33 m fall restores one committed state");
    require(std::abs(sim.snapshot().macro_nose_position.y-seated)<0.03 &&
            sim.snapshot().macro_handle_angle>1 && sim.snapshot().macro_rope_tension>80000,
            "death restores deployed body poses, grip position and cable topology");
    std::cout<<"PASS scraperx_macro checkpoint: deployed_route_preserved=1 lethal_restore=1\n";
    {
        Simulation weak;step(weak,1);weak.set_macro_ballast_mass_for_proof(300);operate(weak);step(weak,15);
        require(weak.snapshot().macro_handle_angle>1 && weak.snapshot().macro_nose_position.y<0.4,
                "open brake cannot manufacture lift from insufficient ballast");
        std::cout<<"PASS scraperx_macro no source: ballast_kg=300 useful_ascent=0\n";
    }
    {
        Simulation broken;step(broken,1);broken.set_macro_rope_connected_for_proof(false);operate(broken);step(broken,15);
        require(broken.snapshot().macro_ballast_drop>9 && broken.snapshot().macro_rope_tension==0 &&
                broken.snapshot().macro_nose_position.y<0.4,
                "broken transmission allows source descent but cannot raise bridge");
        std::cout<<"PASS scraperx_macro no link: source_descends=1 useful_ascent=0\n";
    }
    {
        Simulation retry;step(retry,3);
        require(walk(retry,7,-83.1),"retry approach");grab(retry);pull(retry,0.28,0.15,1.0/60.0);release(retry);step(retry,2);
        require(retry.snapshot().macro_nose_position.y<0.4,"incomplete pull remains held by finite brake");
        const auto h=retry.kit_body_position(retry.kit_body_index(2018));
        require(walk(retry,h.x,h.z+0.65),"walk to grip where it actually fell");grab(retry);
        pull(retry,1.05,0.17,1.0/30.0);release(retry);settle(retry);ascent(retry);
        std::cout<<"PASS scraperx_macro retry: unseeded_regrab=1 mixed_frame_partition=1\n";
    }
    {
        Simulation rider;step(rider,1);operate(rider,1.0);
        require(walk(rider,14,-90),"early rider reaches foot");
        require(walk(rider,14,-98),"early rider boards rising bridge");
        const auto boarded=rider.snapshot();
        require(boarded.support_entity_id==2016 && boarded.macro_ballast_speed < -0.2,
                "rider boards actual moving support before seating");
        (void)rider.set_move_input(0,0);step(rider,8);
        require(rider.snapshot().support_entity_id==2016 &&
                rider.snapshot().player_position.y > boarded.player_position.y+0.25,
                "rotating bridge passively carries rider with zero directional input");
        std::cout<<"PASS scraperx_macro passenger: early_board=1 passive_transport=1\n";
    }
    {
        Simulation overload;overload.set_macro_ballast_mass_for_proof(30000);step(overload,2);
        require(overload.snapshot().macro_ballast_drop>0.1 && overload.snapshot().macro_handle_angle<0.15,
                "overloaded finite brake must slip rather than become an infinite catch");
        std::cout<<"PASS scraperx_macro brake rating: overload_slips=1\n";
    }
    for(double throttle : {0.15,0.35,1.0}) {
        for (double frame : {1.0/60.0, 1.0/30.0}) {
            Simulation variation;step(variation,0.7);operate(variation,throttle,frame);settle(variation);ascent(variation);
        }
    }
    std::cout<<"PASS scraperx_macro complete: normal_route=1 idle=1 no_source=1 no_link=1 energy=1 checkpoint=1 retry=1 passenger=1 finite_brake=1 speed_variants=3\n";
}
