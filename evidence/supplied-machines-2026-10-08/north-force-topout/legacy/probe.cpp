#include "sim/simulation.hpp"
#include <iostream>
#include <cmath>
using namespace scraperx::sim;
int main(){Simulation s; if(!s.debug_restart_at({16,88.9,-179.55}))return 2;s.set_facing(0,-1);for(int i=0;i<90;++i)s.advance_frame(Simulation::kFixedStepSeconds);
auto initial=s.snapshot();std::cout<<"START y="<<initial.player_position.y<<" support="<<initial.support_entity_id<<" grip="<<initial.grip_entity_id<<'\n';if(!initial.player_grounded||initial.support_entity_id!=1901||!initial.grip_available)return 3;
s.request_traversal();for(int i=0;i<27;++i)s.advance_frame(Simulation::kFixedStepSeconds);if(s.snapshot().traversal_state!=TraversalState::Climbing)return 4;s.set_move_input(0,-1);bool gravity_off=false;int transfer_start=-1;
for(int i=0;i<1080;++i){s.advance_frame(Simulation::kFixedStepSeconds);auto v=s.snapshot();if(v.player_gravity_factor!=1)gravity_off=true;if(transfer_start<0&&v.traversal_target_point.y>94)transfer_start=i;
if(i%30==0)std::cout<<"STEP "<<i<<" y="<<v.player_position.y<<" z="<<v.player_position.z<<" state="<<int(v.traversal_state)<<" hands="<<v.traversal_hand_constraint_count<<" gravity="<<v.player_gravity_factor<<" target_y="<<v.traversal_target_point.y<<" handwork="<<v.traversal_actuator_positive_work_j<<'\n';
if(v.death_count)return 5;if(v.player_grounded&&v.support_entity_id==1901&&std::abs(v.player_position.y-94.9)<.12&&v.traversal_hand_constraint_count==0){s.set_move_input(0,0);for(int j=0;j<270;++j)s.advance_frame(Simulation::kFixedStepSeconds);auto end=s.snapshot();std::cout<<"RESULT gravity_off="<<gravity_off<<" transfer_seconds="<<(i-transfer_start+1)/90.<<" y="<<end.player_position.y<<" support="<<end.support_entity_id<<" foot_transfers="<<end.foot_transfer_count<<" work="<<end.traversal_actuator_positive_work_j<<" deaths="<<end.death_count<<'\n';return end.death_count||!end.player_grounded||end.support_entity_id!=1901?6:0;}}
return 7;}
