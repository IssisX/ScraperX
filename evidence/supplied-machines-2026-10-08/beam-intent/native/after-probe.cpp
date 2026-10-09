#include "sim/simulation.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace scraperx::sim;
void line(const char *phase,const Snapshot &v,double input,double net_force){
 std::cout<<phase<<" tick="<<v.tick_index<<" time="<<v.simulation_time_seconds
 <<" input_x="<<input<<" P="<<v.player_position.x<<","<<v.player_position.y<<","<<v.player_position.z
 <<" V="<<v.player_linear_velocity.x<<","<<v.player_linear_velocity.y<<","<<v.player_linear_velocity.z
 <<" slip="<<v.landing_slip_velocity.x<<","<<v.landing_slip_velocity.y<<","<<v.landing_slip_velocity.z
 <<" net_lateral_force_n="<<net_force<<" support="<<v.support_entity_id
 <<" grounded="<<v.player_grounded<<" balancing="<<v.player_balancing
 <<" ground_work_j="<<v.landing_recovery_work_j<<" deaths="<<v.death_count<<'\n';
}
int main(){
 std::cout<<std::setprecision(10);
 Simulation s(InitialSpawn::BracedBayEntry,WorldContent::PipeBridge);
 // Only initial pose setup. This point is inside the real0.46m top width.
 // Real native gravity/contact establishes footing; no later pose/velocity writes.
 if(!s.debug_restart_at({28.12,80.2727272727,-137.0})){std::cout<<"UNRESOLVED staging_rejected=1\n";return 2;}
 s.set_move_input(0,0);s.set_facing(1,0);
 int settle=0;
 for(;settle<90;++settle){
  const auto b=s.snapshot();
  if(!s.advance_frame(Simulation::kFixedStepSeconds).accepted)return 3;
  const auto a=s.snapshot();
  line("SETTLE",a,0,85*(a.player_linear_velocity.x-b.player_linear_velocity.x)/Simulation::kFixedStepSeconds);
  if(a.player_grounded&&a.player_balancing&&a.support_entity_id==1900)break;
 }
 const auto initial=s.snapshot();
 if(!(initial.player_grounded&&initial.player_balancing&&initial.support_entity_id==1900&&initial.player_position.x>28.06&&initial.death_count==0)){std::cout<<"UNRESOLVED real_offcenter_footing_not_established=1\n";return 4;}
 line("START",initial,0,0);
 s.set_move_input(.25,0);
 for(int i=0;i<27;++i){auto b=s.snapshot();if(!s.advance_frame(Simulation::kFixedStepSeconds).accepted)return 5;auto a=s.snapshot();
  line("GENTLE",a,.25,85*(a.player_linear_velocity.x-b.player_linear_velocity.x)/Simulation::kFixedStepSeconds);
  if(!a.player_grounded||!a.player_balancing||a.support_entity_id!=1900||a.death_count!=0){std::cout<<"UNRESOLVED gentle_support_changed=1\n";return 6;}}
 const auto gentle=s.snapshot();
 s.set_move_input(0,0);
 for(int i=0;i<27;++i){auto b=s.snapshot();if(!s.advance_frame(Simulation::kFixedStepSeconds).accepted)return 7;auto a=s.snapshot();
  line("NEUTRAL",a,0,85*(a.player_linear_velocity.x-b.player_linear_velocity.x)/Simulation::kFixedStepSeconds);
  if(!a.player_grounded||!a.player_balancing||a.support_entity_id!=1900||a.death_count!=0){std::cout<<"UNRESOLVED neutral_support_changed=1\n";return 8;}}
 const auto neutral=s.snapshot();
 const bool ignored=gentle.player_position.x>initial.player_position.x+.01&&gentle.player_linear_velocity.x>.01;
 const bool recentered=neutral.player_position.x>=gentle.player_position.x-.001&&std::abs(neutral.player_linear_velocity.x)<.01;
 std::cout<<"AFTER result="<<(ignored&&recentered?"DELIBERATE_CORRECTION_AND_NEUTRAL_BRAKING_PASSED":"NOT_REPRODUCED")
 <<" settled_ticks="<<settle+1<<" start_x="<<initial.player_position.x<<" gentle_x="<<gentle.player_position.x
 <<" neutral_x="<<neutral.player_position.x<<" gentle_delta_x="<<gentle.player_position.x-initial.player_position.x
 <<" neutral_delta_x="<<neutral.player_position.x-gentle.player_position.x<<" support="<<neutral.support_entity_id
 <<" source_ground_command_force_unexposed=1 net_force_includes_solver=1 post_stage_pose_or_velocity_writes=0\n";
 return ignored&&recentered?0:9;
}
