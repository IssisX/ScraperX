#include "sim/simulation.hpp"
#include "sim/cargo_net.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
using scraperx::sim::Vector3;
namespace {
Simulation *world=nullptr;
const char *phase="setup";
std::size_t material_index=0;
Vector3 material_start{};
double magnitude(Vector3 v){return std::hypot(std::hypot(v.x,v.y),v.z);}
bool finite(Vector3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
void log(const char *event){
 const auto v=world->snapshot(); const auto mesh=world->cargo_net_vertices();
 const auto material=material_index<mesh.size()?mesh[material_index]:Vector3{};
 std::cout<<"NET,"<<phase<<','<<event<<",tick="<<v.tick_index
  <<",com="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z
  <<",velocity="<<v.player_linear_velocity.x<<','<<v.player_linear_velocity.y<<','<<v.player_linear_velocity.z
  <<",grounded="<<v.player_grounded<<",support="<<v.support_entity_id<<",traversal="<<int(v.traversal_state)
  <<",owner="<<v.traversal_support_entity_id<<",hands="<<v.traversal_hand_constraint_count
  <<",radial="<<v.hanging_free_arc_active<<",rest="<<v.hanging_commanded_span_m<<",span="<<v.hanging_actual_span_m
  <<",available="<<v.swing_tuck_available<<",requested="<<v.swing_tuck_requested
  <<",actual_below="<<v.swing_body_below_grip_m<<",command_below="<<v.swing_commanded_body_below_grip_m
  <<",positive_work="<<v.traversal_actuator_positive_work_j<<",command_work="<<v.traversal_command_work_bound_j
  <<",gravity="<<v.player_gravity_factor<<",deaths="<<v.death_count
  <<",generations="<<v.traversal_left_hand_generation<<','<<v.traversal_right_hand_generation
  <<",left="<<v.traversal_left_hand.x<<','<<v.traversal_left_hand.y<<','<<v.traversal_left_hand.z
  <<",right="<<v.traversal_right_hand.x<<','<<v.traversal_right_hand.y<<','<<v.traversal_right_hand.z
  <<",forces="<<v.traversal_left_hand_force.x<<','<<v.traversal_left_hand_force.y<<','<<v.traversal_left_hand_force.z
  <<','<<v.traversal_right_hand_force.x<<','<<v.traversal_right_hand_force.y<<','<<v.traversal_right_hand_force.z
  <<",material="<<material.x<<','<<material.y<<','<<material.z
  <<",material_motion="<<magnitude({material.x-material_start.x,material.y-material_start.y,material.z-material_start.z})<<'\n';
}
void check(bool ok,const char *message){if(!ok){log("failure");std::cerr<<"FAIL NET_PROBE "<<message<<'\n';std::exit(1);}}
void tick(){const auto result=world->advance_frame(Simulation::kFixedStepSeconds);
 check(result.accepted&&result.steps_advanced==1,"one native tick");const auto v=world->snapshot();
 check(finite(v.player_position)&&finite(v.player_linear_velocity)&&v.death_count==0,"finite same-life player");}
void settle(int count=45){check(world->set_move_input(0,0),"neutral input");while(count-->0)tick();}
void walk(double x,double z){for(int i=0;i<2700;++i){const auto p=world->snapshot().player_position;
 const double dx=x-p.x,dz=z-p.z,d=std::hypot(dx,dz);
 if(d<.12){settle();return;}const double scale=std::min(1.0,d/.5)/d;
 check(world->set_move_input(dx*scale,dz*scale)&&world->set_facing(dx/d,dz/d),"ordinary grade input");tick();}
 check(false,"bounded ordinary approach");}
void held_checks(const Snapshot &v){
 check(!v.player_grounded&&v.support_entity_id==0&&v.traversal_state==TraversalState::Climbing&&
  v.traversal_support_entity_id==scraperx::sim::CargoNet::kEntity&&v.traversal_hand_constraint_count==2&&
  v.player_gravity_factor==1,"actual airborne soft material hands");
 check(finite(v.traversal_left_hand_force)&&finite(v.traversal_right_hand_force)&&
  magnitude(v.traversal_left_hand_force)<=1501&&magnitude(v.traversal_right_hand_force)<=1501,"finite bounded readback hand force");
 check(std::isfinite(v.hanging_commanded_span_m)&&std::isfinite(v.hanging_actual_span_m)&&
  std::isfinite(v.traversal_actuator_positive_work_j),"finite radial readbacks");
}
void hold(const char *name,bool tucked){phase=name;check(world->set_crouch_input(tucked),"ordinary Crouch input");
 const auto start=world->snapshot();double mean_span=0,mean_rest=0,peak_force=0,min_rest=1e9,max_rest=-1e9;
 log("begin");for(int i=0;i<180;++i){tick();const auto v=world->snapshot();held_checks(v);
  check(v.hanging_free_arc_active&&v.swing_tuck_available,"neutral material radial ownership");
  check(v.swing_tuck_requested==tucked,"native actual posture request");
  check(v.traversal_left_hand_generation==start.traversal_left_hand_generation&&
   v.traversal_right_hand_generation==start.traversal_right_hand_generation,"retained material grips during neutral posture");
  if(!tucked&&std::string(name)=="neutral")check(v.traversal_command_work_bound_j==start.traversal_command_work_bound_j&&
   v.traversal_actuator_positive_work_j==start.traversal_actuator_positive_work_j,"neutral adds no command work");
  peak_force=std::max({peak_force,magnitude(v.traversal_left_hand_force),magnitude(v.traversal_right_hand_force)});
  if(i>=135){mean_span+=v.hanging_actual_span_m;mean_rest+=v.hanging_commanded_span_m;
   min_rest=std::min(min_rest,v.hanging_commanded_span_m);max_rest=std::max(max_rest,v.hanging_commanded_span_m);}
  if(i<5||i%10==9||i==179)log("sample");}
 std::cout<<"NET_WINDOW,"<<phase<<",last45_mean_span="<<mean_span/45<<",last45_mean_rest="<<mean_rest/45
  <<",last45_rest_range="<<max_rest-min_rest<<",peak_force="<<peak_force<<'\n';log("end");}
}
int main(){std::cout<<std::setprecision(12);Simulation s;world=&s;
 settle();walk(20,-114);check(s.set_facing(0,-1)&&s.set_move_input(0,-.25),"approach existing offer");
 int i=0;for(;i<360&&!(s.snapshot().grip_available&&s.snapshot().grip_entity_id==scraperx::sim::CargoNet::kEntity);++i)tick();
 check(i<360&&s.snapshot().player_grounded,"existing net offer on real grade");check(s.set_move_input(0,0)&&s.request_traversal(),"ordinary Action");tick();
 check(s.snapshot().traversal_state==TraversalState::Climbing&&s.snapshot().traversal_hand_constraint_count==2,"Action acquired two soft hands");
 check(s.set_move_input(0,-1),"ordinary upward climb");
 for(i=0;i<1200&&s.snapshot().player_position.y<=4.5;++i){tick();
  check(s.snapshot().traversal_state==TraversalState::Climbing&&s.snapshot().traversal_hand_constraint_count==2,"retained hands during ascent");}
 check(i<1200&&!s.snapshot().player_grounded,"bounded real airborne ascent to4.5");
 const auto mesh=s.cargo_net_vertices();check(!mesh.empty(),"actual material mesh");
 double nearest=std::numeric_limits<double>::infinity();const auto hand=s.snapshot().traversal_left_hand;
 for(std::size_t j=0;j<mesh.size();++j){const double d=magnitude({mesh[j].x-hand.x,mesh[j].y-hand.y,mesh[j].z-hand.z});if(d<nearest){nearest=d;material_index=j;}}
 material_start=mesh[material_index];check(s.set_move_input(0,0)&&s.set_crouch_input(false),"neutral actual input");
 hold("neutral",false);hold("tuck",true);hold("extension",false);
 phase="drop";log("before");check(s.request_release(),"ordinary Drop");tick();log("after");
 check(s.snapshot().traversal_hand_constraint_count==0&&!s.snapshot().hanging_free_arc_active,"Drop cleared hands and radial owner");
 for(i=0;i<90;++i){tick();if(i<5||i%10==9)log("gravity_sample");check(s.snapshot().traversal_hand_constraint_count==0,"Drop did not reacquire neutral hands");}
 std::cout<<"PASS NET_PROBE ordinary_action=1 radial=1 neutral_work=0 tuck_request=1 extension_request=1 drop=1 measurements_not_paired=1\n";
}
