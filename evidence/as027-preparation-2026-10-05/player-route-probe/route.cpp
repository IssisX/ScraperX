#include "sim/simulation.hpp"
#include "lift_fixture.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
int main(){
 using namespace scraperx::sim;using namespace scraperx::sim::insertable;
 Simulation sim;
 if(!sim.debug_restart_at({-25,121.9,-162}))return 1;
 auto tick=[&](){(void)sim.advance_frame(Simulation::kFixedStepSeconds);};
 for(int i=0;i<180;++i)tick();
 auto report=[&](const char* stage){auto s=sim.snapshot();std::printf("%s position=(%.6f,%.6f,%.6f) grounded=%d support=%llu energy=%.3f\n",stage,s.player_position.x,s.player_position.y,s.player_position.z,s.player_grounded,(unsigned long long)s.support_entity_id,fixture_lift->energy);};
 auto walk=[&](double target){for(int i=0;i<450;++i){auto s=sim.snapshot();double dx=target-s.player_position.x;double effort=std::clamp(dx*1.8-s.player_linear_velocity.x*.28,-1.,1.);if(std::abs(dx)<.04&&std::abs(s.player_linear_velocity.x)<.1)effort=0;(void)sim.set_move_input(effort,0);tick();} (void)sim.set_move_input(0,0);};
 report("LOWER_RING");walk(-30.8);report("BOARDED");
 if(!sim.snapshot().player_grounded||sim.snapshot().support_entity_id!=2973)return 2;
 fixture_direction=-1;
 int ticks=0;for(;ticks<65*90;++ticks){tick();if(sim.kit_body_position(sim.kit_body_index(2973)).y+.3>=142.99)break;}
 fixture_direction=0;for(int i=0;i<180;++i)tick();report("AT_RECEIVER");
 if(ticks==65*90)return 3;
 walk(-24.7);report("UPPER_RING");
 auto s=sim.snapshot();
 if(!s.player_grounded||s.support_entity_id!=11||std::abs(s.player_position.y-143.9)>.1)return 4;
 std::printf("PASS ordinary native walk-on, physical ride, release and walk-off; debug staging only on initial121m ring; direct fixture drive input remains\n");
}
