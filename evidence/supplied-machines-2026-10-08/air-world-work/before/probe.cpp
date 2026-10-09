#include "sim/simulation.hpp"
#include <cmath>
#include <iostream>
using namespace scraperx::sim;
double kh(const Snapshot &v) {return .5*85*(v.player_linear_velocity.x*v.player_linear_velocity.x+v.player_linear_velocity.z*v.player_linear_velocity.z);}
int main(){
 Simulation s(InitialSpawn::TranslatingSupport,WorldContent::RegressionFixtures);
 s.set_move_input(0,0);s.set_facing(1,0);
 for(int i=0;i<90;++i)s.advance_frame(Simulation::kFixedStepSeconds);
 auto floor=s.snapshot();
 std::cout<<"FLOOR tick="<<floor.tick_index<<" P="<<floor.player_position.x<<","<<floor.player_position.y<<","<<floor.player_position.z<<" V="<<floor.player_linear_velocity.x<<","<<floor.player_linear_velocity.y<<","<<floor.player_linear_velocity.z<<" support="<<floor.support_entity_id<<" grounded="<<floor.player_grounded<<" inherited="<<floor.support_point_linear_velocity.x<<"\n";
 if(!floor.player_grounded||floor.support_entity_id!=Simulation::kTranslatingSupportEntityId||floor.support_point_linear_velocity.x<=.5)return 2;
 s.request_jump();s.advance_frame(Simulation::kFixedStepSeconds);
 auto jump=s.snapshot();
 std::cout<<"JUMP tick="<<jump.tick_index<<" P="<<jump.player_position.x<<","<<jump.player_position.y<<","<<jump.player_position.z<<" V="<<jump.player_linear_velocity.x<<","<<jump.player_linear_velocity.y<<","<<jump.player_linear_velocity.z<<" support="<<jump.support_entity_id<<" grounded="<<jump.player_grounded<<"\n";
 s.set_move_input(1,0);double worst=0;bool violated=false;
 for(int i=0;i<30;++i){auto before=s.snapshot();s.advance_frame(Simulation::kFixedStepSeconds);auto after=s.snapshot();
  if(before.player_grounded||after.player_grounded||after.support_entity_id!=0||after.traversal_state!=TraversalState::None||after.parachute_deployed||after.death_count!=0)return 3;
  double work=kh(after)-kh(before);double power=work/Simulation::kFixedStepSeconds;worst=std::max(worst,power);violated|=power>3000.1;
  std::cout<<"AIR tick="<<after.tick_index<<" vx_before="<<before.player_linear_velocity.x<<" vx_after="<<after.player_linear_velocity.x<<" y="<<after.player_position.y<<" horizontal_work="<<work<<" world_power="<<power<<" recovery_receipt="<<after.landing_recovery_work_j-before.landing_recovery_work_j<<" jump_receipt="<<after.landing_jump_work_j-before.landing_jump_work_j<<" support="<<after.support_entity_id<<" grounded="<<after.player_grounded<<"\n";
 }
 std::cout<<"BEFORE result="<<(violated?"WORLD_POWER_BOUND_VIOLATED":"NOT_REPRODUCED")<<" worst_world_power="<<worst<<" steering_acceleration_request=14\n";
 return violated?0:4;
}
