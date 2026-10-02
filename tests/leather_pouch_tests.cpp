#include "sim/simulation.hpp"
#include "sim/slingshot.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using scraperx::sim::Simulation;
void require(bool value,const char *why){if(!value){std::cerr<<"FAIL leather pouch: "<<why<<'\n';std::exit(1);}}
void advance(Simulation &s,int ticks){while(ticks-->0)require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"fixed tick");}
int main(){
    Simulation s;auto at=scraperx::sim::Slingshot::neutral_position();
    require(s.debug_restart_at({at.GetX(),at.GetY()+.85,at.GetZ()}),"ordinary pouch approach staging");
    advance(s,45);require(s.slingshot_state().station_available,"physical pouch entry available");
    const auto before=s.snapshot().player_position.y-s.slingshot_state().pouch_position.y;
    require(s.request_slingshot_action(),"BOARD input accepted");advance(s,90);
    const auto state=s.slingshot_state();
    const auto sag=before-(s.snapshot().player_position.y-state.pouch_position.y);
    std::cout<<"LEATHER loaded_sag_m="<<sag<<" residual_j="<<state.energy_residual_j<<" work_j="<<state.work_j<<'\n';
    require(state.seated && sag>.015 && sag<.14,"leather yields to actual rider weight and settles within reach");
    require(state.work_j==0 && state.source_power_w==0,"seating creates no powered work");
    require(std::abs(state.energy_residual_j)<12,"passive seating energy closes");
    require(s.snapshot().death_count==0,"seating is safe");
    std::cout<<"PASS leather pouch native rider load compliance\n";
}
