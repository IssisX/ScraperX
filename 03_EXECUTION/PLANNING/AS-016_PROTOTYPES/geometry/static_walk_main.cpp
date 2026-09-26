#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iomanip>
using scraperx::sim::Simulation;
// Same public-input steering policy as tests/simulation_tests.cpp::walk_to.
bool walk_to(Simulation &sim, double x, double z, double budget, double tolerance=.1) {
    for (unsigned tick=0; tick<static_cast<unsigned>(budget*Simulation::kTickRateHz); ++tick) {
        const auto s=sim.snapshot();
        const double dx=x-s.player_position.x, dz=z-s.player_position.z;
        const double length=std::hypot(dx,dz);
        if (length<=tolerance) { (void)sim.set_move_input(0,0); return true; }
        const double scale=std::min(1.,length/.6);
        (void)sim.set_move_input(dx/length*scale,dz/length*scale);
        (void)sim.set_facing(dx/length,dz/length);
        if (!sim.advance_frame(Simulation::kFixedStepSeconds).accepted) return false;
    }
    (void)sim.set_move_input(0,0); return false;
}
int main() {
    std::cout<<std::setprecision(9);
    const double target=std::getenv("SCRAPERX_PROBE_TIP_Y")?std::atof(std::getenv("SCRAPERX_PROBE_TIP_Y")):8.;
    const double tip_z=-90-20*std::cos(std::asin(7.4/20));
    Simulation sim;
    (void)sim.advance_frame(.5);
    std::cout<<"STATIC_RECEIVER_PROBE tip_target="<<target<<" spawn=("<<sim.snapshot().player_position.x<<","<<sim.snapshot().player_position.y<<","<<sim.snapshot().player_position.z<<") no_teleport=1 jump_requests=0 mechanism_static=1\n";
    struct Point {const char *label;double x,z,seconds,min_y;unsigned long long support;};
    const Point points[]={
      {"yard_east",10,-75,20,.5,0},
      {"release_station",10,-82,5,.5,0},
      {"rack_south",10,-75,5,.5,0},
      {"west_access",-.6,-75,5,.5,0},
      {"west_approach",-.6,-86,5,.5,0},
      {"ramp_foot",1.94,-86,4,.5,0},
      {"lower_cheek",1.94,-91.1,5,1.5,600},
      {"bridge_board",6,-91.1,5,1.5,601},
      {"bridge_cross",6,tip_z+1.5,8,6.7,601},
      {"side_cheek",9.06,tip_z+1.5,5,7.0,602},
      {"fixed_receiver",9.06,tip_z-2.,5,8.6,603},
      {"connector_start",9.06,tip_z-3.8,4,8.6,0},
      {"native_ring",9.06,-125.2,8,11.6,Simulation::kTowerEntityId}
    };
    for(const auto &p:points) {
      const bool reached=walk_to(sim,p.x,p.z,p.seconds);
      (void)sim.advance_frame(.2);
      const auto s=sim.snapshot();
      std::cout<<"WAYPOINT "<<p.label<<" reached="<<reached<<" pos=("<<s.player_position.x<<","<<s.player_position.y<<","<<s.player_position.z<<") grounded="<<s.player_grounded<<" support="<<s.support_entity_id<<" deaths="<<s.death_count<<" tick="<<s.tick_index<<"\n";
      if(!reached || !s.player_grounded || s.player_position.y<p.min_y || s.death_count || (p.support && s.support_entity_id!=p.support)) {
       std::cerr<<"FAIL STATIC_RECEIVER_PROBE waypoint="<<p.label<<" target="<<target<<"\n";return 1;
      }
    }
    const auto s=sim.snapshot();
    std::cout<<"PASS STATIC_RECEIVER_PROBE tip_target="<<target<<" ring_player_y="<<s.player_position.y<<" support="<<s.support_entity_id<<" no_teleport=1 jump_requests=0 deaths=0 mechanism_static=1\n";
}
