#include "sim/simulation.hpp"
#include "sim/parkour_route.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using scraperx::sim::Simulation;
void cross_swing(Simulation &);
void require(bool ok,const char*why,const Simulation&s){if(ok)return;const auto v=s.snapshot();std::cerr<<"FAIL SUPPLIED_ROUTE "<<why<<" pos="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z<<" support="<<v.support_entity_id<<" station="<<int(v.supplied_machine_station)<<" deck="<<v.supplied_machine_surface_y<<" energy="<<v.supplied_machine_energy_j<<'\n';std::exit(1);}
void tick(Simulation&s,int n=1){while(n-->0)require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"tick",s);}
void walk(Simulation&s,double x,double z){
 const auto start=s.snapshot().player_position;const int budget=700+int(std::ceil(std::hypot(x-start.x,z-start.z)/5.5))*90;
 for(int i=0;i<budget;++i){auto v=s.snapshot();const double dx=x-v.player_position.x,dz=z-v.player_position.z;
  (void)s.set_move_input(std::clamp(dx*1.8-v.player_linear_velocity.x*.28,-1.,1.),std::clamp(dz*1.8-v.player_linear_velocity.z*.28,-1.,1.));tick(s);
  if(std::hypot(dx,dz)<.06&&std::hypot(v.player_linear_velocity.x,v.player_linear_velocity.z)<.1)break;
 }
 (void)s.set_move_input(0,0);tick(s,45);require(std::hypot(s.snapshot().player_position.x-x,s.snapshot().player_position.z-z)<.18,"walk reaches target",s);
}
void balance(){Simulation s;bool found=false;
 for(unsigned i=0;i<s.kit_body_count();++i)if(s.kit_body_entity(i)==2980)found=true;
 require(found,"native imported deck exists",s);
 require(s.debug_restart_at({-24.7,143.9,-142.65}),"initial143m staging only",s);tick(s,90);
 require(!s.set_supplied_machine_input(std::numeric_limits<double>::quiet_NaN()),"reject nonfinite",s);
 (void)s.set_supplied_machine_input(1);tick(s,90);
 require(s.snapshot().supplied_machine_braking&&std::abs(s.snapshot().supplied_machine_surface_y-143)<.02,"remote command rejected",s);
 (void)s.set_supplied_machine_input(0);walk(s,-27.5,-142.65);
 require(s.snapshot().supplied_machine_station==2,"lower panel accessible",s);
 walk(s,-32.8,-142.5);require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==2980&&s.snapshot().supplied_machine_station==1,"ordinary boarding",s);
 (void)s.set_supplied_machine_input(1);tick(s,180);require(s.snapshot().supplied_machine_surface_y>144,"gravity ascent",s);
 (void)s.set_supplied_machine_input(0);tick(s,45);const double hold=s.snapshot().supplied_machine_surface_y;tick(s,180);
 require(std::abs(s.snapshot().supplied_machine_surface_y-hold)<.02&&s.snapshot().supplied_machine_braking,"finite passive hold",s);
 const double saved=s.snapshot().supplied_machine_energy_j;
 const double saved_y=s.snapshot().supplied_machine_surface_y;
 require(s.restart_checkpoint(),"checkpoint accepted",s);tick(s,45);
 require(std::abs(s.snapshot().supplied_machine_energy_j-saved)<.01&&std::abs(s.snapshot().supplied_machine_surface_y-saved_y)<.003&&s.snapshot().supplied_machine_braking,"checkpoint restores coherent bodies and source",s);
 // Checkpoint is an explicit retry; reach the physical deck again if it restored entry.
 if(s.snapshot().support_entity_id!=2980){walk(s,-27.5,-142.65);walk(s,-32.8,-142.5);}
 (void)s.set_supplied_machine_input(1);int elapsed=0;double peak=0;
 for(;elapsed<65*90;++elapsed){tick(s);const auto v=s.snapshot();peak=std::max(peak,v.supplied_machine_power_w);
  require(v.supplied_machine_station==1&&v.player_grounded&&v.support_entity_id==2980,"actual rider remains supported",s);
  require(v.supplied_machine_energy_j>=0&&v.supplied_machine_power_w<100001,"finite bank and power",s);
  if(v.supplied_machine_surface_y>164.98)break;
 }
 require(elapsed<65*90,"reaches165m",s);(void)s.set_supplied_machine_input(0);tick(s,90);
 walk(s,-27.5,-142.65);walk(s,-24.7,-142.65);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-165.9)<.1,"stable existing165m Tower exit",s);
 walk(s,-27.5,-142.65);require(s.snapshot().supplied_machine_station==3,"upper recall panel",s);
 const double energy=s.snapshot().supplied_machine_energy_j;(void)s.set_supplied_machine_input(-1);tick(s,180);
 require(s.snapshot().supplied_machine_surface_y<164&&s.snapshot().supplied_machine_energy_j<energy-1000,"reset requires paid work",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);require(s.snapshot().supplied_machine_braking,"recall stops without power",s);
 (void)s.set_supplied_machine_input(1);for(int i=0;i<30*90&&s.snapshot().supplied_machine_surface_y<164.98;++i)tick(s);
 (void)s.set_supplied_machine_input(0);tick(s,90);walk(s,-32.8,-142.5);
 (void)s.set_supplied_machine_input(-1);tick(s,90);const double support=s.snapshot().support_point_linear_velocity.y;
 (void)s.request_jump();tick(s);const double takeoff=s.snapshot().player_linear_velocity.y;
 require(support<-.1&&takeoff-support>5&&takeoff-support<5.6,"jump preserves support momentum",s);
 tick(s,5);require(s.snapshot().supplied_machine_braking,"airborne loses authority",s);
 require(s.snapshot().death_count==0,"no deaths",s);
 std::cout<<"PASS BALANCE approach/board/hold/checkpoint/ascent/165exit/paidreset/recall/departure peak_power="<<peak<<" saved_energy="<<saved<<" support_vy="<<support<<" takeoff_vy="<<takeoff<<" initial_staging_only=1\n";
}

void crown(){Simulation s;require(s.debug_restart_at({-24.7,165.9,-159.8}),"initial165m staging only",s);tick(s,90);
 walk(s,-40.5,-160.45);require(s.snapshot().supplied_machine_index==1&&s.snapshot().supplied_machine_station==2,"crown lower station",s);
 walk(s,-44,-159);(void)s.request_jump();tick(s);walk(s,-44,-155.45);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==2983&&s.snapshot().supplied_machine_station==1,"crown ordinary boarding",s);
 const double start=s.snapshot().supplied_machine_energy_j;(void)s.set_supplied_machine_input(1);double peak=0;int elapsed=0;
 for(;elapsed<70*90;++elapsed){tick(s);auto v=s.snapshot();peak=std::max(peak,v.supplied_machine_power_w);
  require(v.player_grounded&&v.support_entity_id==2983&&v.supplied_machine_station==1,"crown actual rider remains on cabin with reachable control",s);
  require(v.supplied_machine_energy_j>=0&&v.supplied_machine_power_w<=100001,"crown finite power/bank",s);
  if(v.supplied_machine_surface_y>=197.97)break;
 }
 require(elapsed<70*90,"crown reaches198 receiver",s);(void)s.set_supplied_machine_input(0);tick(s,12*90);
 require(s.snapshot().supplied_machine_braking&&s.snapshot().supplied_machine_energy_j<start-300000,"paid climb and passive hold",s);
 const double x=s.snapshot().player_position.x;walk(s,x,-157.5);(void)s.request_jump();tick(s);walk(s,x,-159.8);
 walk(s,-41.0759,-160.45);cross_swing(s);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-198.9)<.12&&s.snapshot().death_count==0,"crown stable existing198m Tower exit",s);
 walk(s,-26.7379,-160.1);require(s.snapshot().supplied_machine_station==3&&s.snapshot().supplied_machine_index==1,"crown far-side upper control reachable",s);
 (void)s.set_supplied_machine_input(-1);tick(s,5*90);require(s.snapshot().supplied_machine_surface_y<197,"crown ordinary recall moves cabin",s);
 (void)s.set_supplied_machine_input(0);tick(s,5*90);require(s.snapshot().supplied_machine_braking,"crown recall releases to real brake",s);
 std::cout<<"PASS CROWN ordinary approach/board/paid33mride/198exit/upperrecall peak_power="<<peak<<" draw_j="<<start-s.snapshot().supplied_machine_energy_j<<" initial_staging_only=1\n";
}
void tram(){Simulation s;require(s.debug_restart_at({-24.7,198.9,-159.8}),"initial198m staging only",s);tick(s,90);
 for(auto p:{std::pair{-22.5,-159.8},std::pair{-22.5,-125.},std::pair{-23.15,-125.},std::pair{-23.15,-122.75},std::pair{-22.5,-122.75}})walk(s,p.first,p.second);
 require(s.snapshot().supplied_machine_index==2&&s.snapshot().supplied_machine_station==2,"tram lower physical control",s);
 walk(s,-23.15,-119.1);walk(s,-22.5,-119.1);require(s.snapshot().support_entity_id==2984&&s.snapshot().supplied_machine_station==1,"tram ordinary lateral boarding",s);
 const double start=s.snapshot().supplied_machine_energy_j;(void)s.set_supplied_machine_input(1);tick(s,180);
 require(s.snapshot().supplied_machine_surface_y>198.15,"traction raises actual rider",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);const double held=s.snapshot().supplied_machine_surface_y;tick(s,180);
 require(std::abs(s.snapshot().supplied_machine_surface_y-held)<.03&&s.snapshot().supplied_machine_braking,"wheel friction brakes hold slope",s);
 (void)s.set_supplied_machine_input(1);double peak=0;int elapsed=0;
 for(;elapsed<80*90;++elapsed){tick(s);auto v=s.snapshot();peak=std::max(peak,v.supplied_machine_power_w);
  require(v.player_grounded&&v.support_entity_id==2984&&v.supplied_machine_station==1,"tram remains contact-supported and operable",s);
  require(v.supplied_machine_energy_j>=0&&v.supplied_machine_power_w<=100001,"both wheels share finite source",s);
  if(v.supplied_machine_surface_y>=230.98)break;
 }
 require(elapsed<80*90,"tram reaches231 receiver",s);(void)s.set_supplied_machine_input(0);tick(s,90);
 require(s.snapshot().supplied_machine_energy_j<start-650000,"tram uphill real work charged",s);
 const double x=s.snapshot().player_position.x;walk(s,x-.65,-119.1);walk(s,x-.65,-122.75);walk(s,x-.65,-127);walk(s,24.628884,-127);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-231.9)<.12,"tram stable231 Tower exit",s);
 walk(s,23.978884,-122.75);walk(s,24.628884,-122.75);require(s.snapshot().supplied_machine_station==3,"tram upper recall reachable",s);
 (void)s.set_supplied_machine_input(-1);tick(s,270);require(s.snapshot().supplied_machine_surface_y<230,"tram recall through real wheel contacts",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);require(s.snapshot().supplied_machine_braking&&s.snapshot().death_count==0,"tram recovery release brakes",s);
 std::cout<<"PASS TRAM ordinary approach/traction/stop-resume/paid33mride/231exit/upperrecall peak_power="<<peak<<" draw_j="<<start-s.snapshot().supplied_machine_energy_j<<" initial_staging_only=1\n";
}
void helix(){Simulation s;require(s.debug_restart_at({24.628884,231.9,-127}),"initial231m staging only",s);tick(s,90);
 for(auto p:{std::pair{23.,-127.},std::pair{-22.5,-127.},std::pair{-22.5,-142.},std::pair{-24.7,-142.65},std::pair{-27.5,-142.65}})walk(s,p.first,p.second);
 require(s.snapshot().supplied_machine_index==3&&s.snapshot().supplied_machine_station==2,"helix lower physical control",s);
 walk(s,-31.3,-142.5);require(s.snapshot().support_entity_id==2988&&s.snapshot().supplied_machine_station==1,"helix ordinary boarding",s);
 const double start=s.snapshot().supplied_machine_energy_j;(void)s.set_supplied_machine_input(1);tick(s,3*90);
 require(s.snapshot().supplied_machine_surface_y>231.3,"cam contact lifts loaded carriage",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);const double held=s.snapshot().supplied_machine_surface_y;tick(s,180);
 require(std::abs(s.snapshot().supplied_machine_surface_y-held)<.03&&s.snapshot().supplied_machine_braking,"shaft brake holds through roller contact",s);
 (void)s.set_supplied_machine_input(1);double peak=0;int elapsed=0;
 for(;elapsed<65*90;++elapsed){tick(s);auto v=s.snapshot();peak=std::max(peak,v.supplied_machine_power_w);
  require(v.player_grounded&&v.support_entity_id==2988&&v.supplied_machine_station==1,"helix real supported and operable rider",s);
  require(v.supplied_machine_energy_j>=0&&v.supplied_machine_power_w<=100001,"helix finite source",s);
  if(v.supplied_machine_surface_y>=252.98)break;
 }
 require(elapsed<65*90,"helix reaches253 receiver",s);(void)s.set_supplied_machine_input(0);tick(s,90);
 require(s.snapshot().supplied_machine_energy_j<start-280000,"helix work charged to source",s);
 walk(s,-27.5,-142.65);walk(s,-24.7,-142.65);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-253.9)<.12,"helix stable253 Tower exit",s);
 walk(s,-27.5,-142.65);require(s.snapshot().supplied_machine_station==3,"helix upper recall reachable",s);
 (void)s.set_supplied_machine_input(-1);tick(s,270);require(s.snapshot().supplied_machine_surface_y<252.5,"helix physical follower recall",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);require(s.snapshot().supplied_machine_braking&&s.snapshot().death_count==0,"helix recall releases to brake",s);
 std::cout<<"PASS HELIX ordinary approach/contact-cam/stop-resume/paid22mride/253exit/upperrecall peak_power="<<peak<<" draw_j="<<start-s.snapshot().supplied_machine_energy_j<<" initial_staging_only=1\n";
}
void cascade(){Simulation s;require(s.debug_restart_at({-24.7,253.9,-142.65}),"initial253m staging only",s);tick(s,90);
 walk(s,-24.7,-161.1);walk(s,-27.5,-161.1);require(s.snapshot().supplied_machine_index==4&&s.snapshot().supplied_machine_station==2,"cascade lower physical control",s);
 walk(s,-32.8,-160.95);require(s.snapshot().support_entity_id==2992&&s.snapshot().supplied_machine_station==1,"cascade ordinary boarding",s);
 const auto outer=s.kit_body_index(2990),inner=s.kit_body_index(2991),deck=s.kit_body_index(2992);
 const double outer_y=s.kit_body_position(outer).y,inner_y=s.kit_body_position(inner).y,deck_y=s.kit_body_position(deck).y;
 const double start=s.snapshot().supplied_machine_energy_j;(void)s.set_supplied_machine_input(1);tick(s,3*90);
 require(s.snapshot().supplied_machine_surface_y>253.4,"rope cascade lifts loaded carriage",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);const double held=s.snapshot().supplied_machine_surface_y;tick(s,180);
 require(std::abs(s.snapshot().supplied_machine_surface_y-held)<.03&&s.snapshot().supplied_machine_braking,"outer brake holds through tension-only cascade",s);
 (void)s.set_supplied_machine_input(1);double peak=0;int elapsed=0;
 for(;elapsed<65*90;++elapsed){tick(s);auto v=s.snapshot();peak=std::max(peak,v.supplied_machine_power_w);
  require(v.player_grounded&&v.support_entity_id==2992&&v.supplied_machine_station==1,"cascade real supported and operable rider",s);
  require(v.supplied_machine_energy_j>=0&&v.supplied_machine_power_w<=100001,"cascade finite source",s);
  if(v.supplied_machine_surface_y>=285.98)break;
 }
 require(elapsed<65*90,"cascade reaches286 receiver",s);(void)s.set_supplied_machine_input(0);tick(s,90);
 const double a=s.kit_body_position(outer).y-outer_y,b=s.kit_body_position(inner).y-inner_y,c=s.kit_body_position(deck).y-deck_y;
 require(std::abs(a-11)<.03&&std::abs(b-2*a)<.03&&std::abs(c-3*a)<.04,"physical1:2:3 displacement transfer",s);
 require(s.snapshot().supplied_machine_energy_j<start-750000,"cascade input work charged to source",s);
 walk(s,-27.5,-161.1);walk(s,-24.7,-161.1);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-286.9)<.12,"cascade stable286 Tower exit",s);
 walk(s,-27.5,-161.1);require(s.snapshot().supplied_machine_station==3,"cascade upper recall reachable",s);
 (void)s.set_supplied_machine_input(-1);tick(s,270);require(s.snapshot().supplied_machine_surface_y<286-.5,"cascade real rope reset",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);require(s.snapshot().supplied_machine_braking&&s.snapshot().death_count==0,"cascade recall releases to brake",s);
 std::cout<<"PASS CASCADE ordinary approach/1:2:3-cables/stop-resume/paid33mride/286exit/upperrecall peak_power="<<peak<<" draw_j="<<start-s.snapshot().supplied_machine_energy_j<<" initial_staging_only=1\n";
}
void pitman(){Simulation s;require(s.debug_restart_at({-24.7,286.9,-161.1}),"initial286m staging only",s);tick(s,90);
 walk(s,-24.7,-142.65);walk(s,-32.25,-142.65);require(s.snapshot().supplied_machine_index==5&&s.snapshot().supplied_machine_station==2,"pitman lower physical control",s);
 walk(s,-42.5,-142.5);require(s.snapshot().support_entity_id==2921&&s.snapshot().supplied_machine_station==1,"pitman ordinary boarding",s);
 const double start=s.snapshot().supplied_machine_energy_j;(void)s.set_supplied_machine_input(1);tick(s,3*90);
 require(s.snapshot().supplied_machine_surface_y>286.5,"crank and real rod lift carriage",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);const double held=s.snapshot().supplied_machine_surface_y;tick(s,180);
 require(std::abs(s.snapshot().supplied_machine_surface_y-held)<.03&&s.snapshot().supplied_machine_braking,"crank brake holds through rod",s);
 (void)s.set_supplied_machine_input(1);double peak=0;int elapsed=0;
 for(;elapsed<65*90;++elapsed){tick(s);auto v=s.snapshot();peak=std::max(peak,v.supplied_machine_power_w);
  require(v.player_grounded&&v.support_entity_id==2921&&v.supplied_machine_station==1,"pitman real supported and operable rider",s);
  require(v.supplied_machine_energy_j>=0&&v.supplied_machine_power_w<=100001,"pitman finite source",s);
  if(v.supplied_machine_surface_y>=307.98)break;
 }
 require(elapsed<65*90,"pitman reaches308 receiver",s);(void)s.set_supplied_machine_input(0);tick(s,90);
 require(s.snapshot().supplied_machine_energy_j<start-500000,"pitman work charged to source",s);
 walk(s,-32.25,-142.65);walk(s,-24.7,-142.65);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-308.9)<.12,"pitman stable308 Tower exit",s);
 walk(s,-32.25,-142.65);require(s.snapshot().supplied_machine_station==3,"pitman upper recall reachable",s);
 (void)s.set_supplied_machine_input(-1);tick(s,270);require(s.snapshot().supplied_machine_surface_y<307.8,"pitman real rod reset",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);require(s.snapshot().supplied_machine_braking&&s.snapshot().death_count==0,"pitman recall releases to brake",s);
 std::cout<<"PASS PITMAN ordinary approach/crank-rod/stop-resume/paid22mride/308exit/upperrecall peak_power="<<peak<<" draw_j="<<start-s.snapshot().supplied_machine_energy_j<<" initial_staging_only=1\n";
}
void catch_swing(Simulation &s){
 walk(s,-38.1,-160.45);(void)s.set_facing(1,0);(void)s.set_move_input(1,0);(void)s.request_jump();
 int caught=0;for(;caught<100&&!s.snapshot().player_swinging;++caught)tick(s);
 require(s.snapshot().player_swinging&&s.snapshot().traversal_hand_constraint_count==2,"ordinary rising jump catches mounted swing bar",s);
 (void)s.set_move_input(0,0);tick(s,180);
}
void passive_swing_release(Simulation &s){
 (void)s.set_move_input(0,0);const auto before=s.snapshot().player_linear_velocity;
 (void)s.request_jump();tick(s);const auto after=s.snapshot();
 require(!after.player_swinging,"jump releases swing",s);
 require(std::abs(after.player_linear_velocity.x-before.x)<.05&&
         std::abs(after.player_linear_velocity.z-before.z)<.05&&
         std::abs(after.player_linear_velocity.y-before.y+9.81/90.)<.05,
         "passive release preserves momentum with ordinary gravity only",s);
}
double vector_length(const scraperx::sim::Vector3 &v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
void tuck_readback_absent(const Simulation &s){const auto v=s.snapshot();
 require(!v.swing_tuck_available&&!v.swing_tuck_requested&&v.swing_body_below_grip_m==0&&
         v.swing_commanded_body_below_grip_m==0,"tuck readback clears outside attached hanging swing bar",s);
}
void tuck_tick(Simulation &s,bool requested,double lean=0,std::uint64_t support=2930,bool swinging=true,
               double forward_z=0,bool posture_power_bound=true){
 const auto before=s.snapshot();
 require(s.set_crouch_input(requested)&&s.set_move_input(lean,forward_z),"ordinary posture and lean commands accepted",s);
 tick(s);const auto v=s.snapshot();
 require(v.player_swinging==swinging&&v.traversal_support_entity_id==support&&v.traversal_hand_constraint_count==2&&
         v.player_gravity_factor==1&&v.swing_tuck_available&&v.swing_tuck_requested==requested,
         "manual tuck retains compatible actual hands/gravity and exact request readback",s);
 const double work=v.traversal_actuator_positive_work_j-before.traversal_actuator_positive_work_j;
 require(std::isfinite(work)&&work>=-.00001&&(!posture_power_bound||work<=500.0/90.0+.00001),
         "lean and tuck share aggregate500W positive spring-rest-target work",s);
 require(vector_length(v.traversal_left_hand_force)<=1501&&vector_length(v.traversal_right_hand_force)<=1501,
         "tuck uses bounded1500N physical hands",s);
 require(std::isfinite(v.swing_body_below_grip_m)&&std::isfinite(v.swing_commanded_body_below_grip_m),
         "actual and commanded posture are finite",s);
 const double observed_below=.5*(v.traversal_left_hand.y+v.traversal_right_hand.y)-v.player_position.y;
 require(std::abs(observed_below-v.swing_body_below_grip_m)<.0001,
         "posture readback agrees with actual hand midpoint and physical COM",s);
 if(lean==0&&forward_z==0) {
  require(v.hanging_free_arc_active&&v.hanging_commanded_span_m>0,
          "eligible neutral commands use exclusive radial hands",s);
  const auto span=[&](const scraperx::sim::Vector3 &grip){return std::sqrt(
   std::pow(grip.x-v.player_position.x,2)+std::pow(grip.y-v.player_position.y,2)+std::pow(grip.z-v.player_position.z,2));};
  require(std::abs(.5*(span(v.traversal_left_hand)+span(v.traversal_right_hand))-v.hanging_actual_span_m)<.0001,
          "radial actual-span readback agrees with real grips and physical COM",s);
 }else require(!v.hanging_free_arc_active,"explicit directional input resumes Cartesian hands",s);
}
void start_tuck_swing(Simulation &s){
 require(s.debug_restart_at({-41.0759,198.9,-160.45}),"initial supported198m tuck staging only",s);
 tick(s,90);tuck_readback_absent(s);catch_swing(s);
 require(s.snapshot().swing_tuck_available&&!s.snapshot().swing_tuck_requested,
         "hanging swing bar catch exposes posture without requesting it",s);
}
// Neutral free arc has a scalar rest length; literal worldY readbacks remain
// checked by tuck_tick, but do not assert a fixed world posture during an arc.
template<class Snapshot> double posture_actual_metric(const Snapshot &v){
 return v.hanging_free_arc_active?v.hanging_actual_span_m:v.swing_body_below_grip_m;
}
template<class Snapshot> double posture_command_metric(const Snapshot &v){
 return v.hanging_free_arc_active?v.hanging_commanded_span_m:v.swing_commanded_body_below_grip_m;
}
void tuck_posture(){Simulation neutral,tucked;start_tuck_swing(neutral);start_tuck_swing(tucked);
 double extended_body=0,tucked_body=0,extended_target=0,tucked_target=0;
 double held_min=std::numeric_limits<double>::infinity(),held_max=-held_min;
 double held_work_start=0,held_work_end=0;
 for(int i=0;i<360;++i){tuck_tick(neutral,false);tuck_tick(tucked,true);
  const auto a=neutral.snapshot(),b=tucked.snapshot();
  if(i>=270){held_min=std::min(held_min,posture_command_metric(b));
   held_max=std::max(held_max,posture_command_metric(b));
   if(i==270)held_work_start=b.traversal_actuator_positive_work_j;
   held_work_end=b.traversal_actuator_positive_work_j;}
  if(i>=315){extended_body+=posture_actual_metric(a);tucked_body+=posture_actual_metric(b);
   extended_target+=posture_command_metric(a);tucked_target+=posture_command_metric(b);}
 }
 extended_body/=45;tucked_body/=45;extended_target/=45;tucked_target/=45;
 require(extended_body-tucked_body>.10,"tuck meaningfully reduces actual body span from real grips",tucked);
 // Chosen before runtime:0.01m is4% of the0.25m commanded stroke. Actual COM
 // response still independently must exceed0.10m; work and force bounds are exact.
 require(std::abs((extended_target-tucked_target)-.25)<=.01,
         "manual tuck changes bounded posture command by0.25m",tucked);
 require(held_max-held_min<=.01,"held tuck does not choose swing phases or switch its target",tucked);
 // The ledger may still increase while the real bar moves: no zero-work claim.
 double restored_body=0,restored_target=0;
 for(int i=0;i<360;++i){tuck_tick(tucked,false);
  if(i>=315){restored_body+=posture_actual_metric(tucked.snapshot());
   restored_target+=posture_command_metric(tucked.snapshot());}}
 restored_body/=45;restored_target/=45;
 require(restored_body-tucked_body>.10&&restored_target-tucked_target>.20,
         "explicit extension restores actual COM and commanded body distance",tucked);
 // Keep the posture command held across release; availability/readback must clear.
 for(int i=0;i<90;++i)tuck_tick(tucked,true);
 require(tucked.snapshot().swing_tuck_requested,"release test is actually tucked",tucked);
 passive_swing_release(tucked);tuck_readback_absent(tucked);(void)tucked.set_crouch_input(false);
 std::cout<<"PASS TUCK actual_body_rise_m="<<extended_body-tucked_body
          <<" commanded_rise_m="<<extended_target-tucked_target<<" restored_body_m="<<restored_body
          <<" held_target_span_m="<<held_max-held_min<<" held_tracking_positive_work_j="<<held_work_end-held_work_start
          <<" passive_tucked_release=1\n";
}
struct TuckOutcome{scraperx::sim::Vector3 position,velocity,bar_position,bar_velocity;
 scraperx::sim::Quaternion bar_rotation;std::uint64_t tick;};
TuckOutcome fixed_tuck_schedule(int pulse_start){Simulation s;start_tuck_swing(s);
 const auto bar=s.kit_body_index(2930);require(bar!=Simulation::kKitNone,"phase oracle has actual hanging bar body",s);
 for(int i=0;i<360;++i)tuck_tick(s,i>=pulse_start&&i<pulse_start+45);
 const auto v=s.snapshot();
 std::cout<<"TUCK SCHEDULE start="<<pulse_start<<" pulse_ticks=45 horizon_ticks=360 position="
          <<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z<<" velocity="
          <<v.player_linear_velocity.x<<','<<v.player_linear_velocity.y<<','<<v.player_linear_velocity.z
          <<" positive_target_work_j="<<v.traversal_actuator_positive_work_j<<'\n';
 const auto position=s.kit_body_position(bar),velocity=s.kit_body_velocity(bar);const auto rotation=s.kit_body_rotation(bar);
 std::cout<<"TUCK BAR start="<<pulse_start<<" tick="<<v.tick_index<<" position="<<position.x<<','<<position.y<<','<<position.z
          <<" velocity="<<velocity.x<<','<<velocity.y<<','<<velocity.z<<" rotation="
          <<rotation.x<<','<<rotation.y<<','<<rotation.z<<','<<rotation.w<<'\n';
 return {v.player_position,v.player_linear_velocity,position,velocity,rotation,v.tick_index};
}
void tuck_schedule_falsifier(){const auto a=fixed_tuck_schedule(0),b=fixed_tuck_schedule(120);
 if(a.tick!=b.tick){std::cerr<<"FAIL TUCK schedule outcomes do not share a native tick\n";std::exit(1);}
 const double delta=std::sqrt(std::pow(a.position.x-b.position.x,2)+std::pow(a.position.y-b.position.y,2)+
   std::pow(a.position.z-b.position.z,2)+std::pow(a.velocity.x-b.velocity.x,2)+
   std::pow(a.velocity.y-b.velocity.y,2)+std::pow(a.velocity.z-b.velocity.z,2));
 const auto distance=[](const scraperx::sim::Vector3 &u,const scraperx::sim::Vector3 &v){
  return std::sqrt(std::pow(u.x-v.x,2)+std::pow(u.y-v.y,2)+std::pow(u.z-v.z,2));};
 const double bar_position_delta=distance(a.bar_position,b.bar_position),bar_velocity_delta=distance(a.bar_velocity,b.bar_velocity);
 const auto &q=a.bar_rotation;const auto &r=b.bar_rotation;
 const double norms=std::sqrt((q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w)*(r.x*r.x+r.y*r.y+r.z*r.z+r.w*r.w));
 if(!std::isfinite(norms)||norms<=0){std::cerr<<"FAIL TUCK invalid actual bar rotation\n";std::exit(1);}
 const double bar_angle_delta=2*std::acos(std::clamp(std::abs((q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w)/norms),0.,1.));
 // The player may change posture without changing the bar. Reject that false
 // positive separately using actual common-tick body pose/velocity, not playerY.
 const double bar_delta=std::max({bar_position_delta,bar_velocity_delta,bar_angle_delta});
 std::cout<<"TUCK PHASE player_state_delta="<<delta<<" bar_position_delta_m="<<bar_position_delta
          <<" bar_velocity_delta_mps="<<bar_velocity_delta<<" bar_angle_delta_rad="<<bar_angle_delta<<'\n';
 if(!std::isfinite(delta)||delta<=.0001){std::cerr<<"FAIL TUCK fixed phase schedules indistinguishable delta="<<delta<<'\n';std::exit(1);}
 if(!std::isfinite(bar_delta)||bar_delta<=.0001){std::cerr<<"FAIL TUCK fixed phase schedules leave actual bar indistinguishable delta="<<bar_delta<<'\n';std::exit(1);}
 std::cout<<"PASS TUCK phase_schedule_state_delta="<<delta<<" actual_bar_delta="<<bar_delta<<" threshold=0.0001\n";
}
void catch_regular_hang(Simulation &s,std::uint64_t support){
 tuck_readback_absent(s);require(s.set_facing(1,0)&&s.set_move_input(1,0),"ordinary ledge approach commands",s);
 int elapsed=0;for(;elapsed<180&&s.snapshot().traversal_state!=scraperx::sim::TraversalState::Hanging;++elapsed)tick(s);
 require(elapsed<180&&s.snapshot().traversal_support_entity_id==support&&s.snapshot().traversal_hand_constraint_count==2,
         "ordinary ledge approach catches compatible actual hands",s);
 (void)s.set_move_input(0,0);tick(s,180);
 require(s.snapshot().swing_tuck_available&&!s.snapshot().swing_tuck_requested,
         "posture eligibility follows actual compatible ledge hands, not bar identity",s);
}
void regular_hang_posture(scraperx::sim::InitialSpawn spawn,std::uint64_t support){
 Simulation control(spawn,scraperx::sim::WorldContent::RegressionFixtures),posture(spawn,scraperx::sim::WorldContent::RegressionFixtures);
 catch_regular_hang(control,support);catch_regular_hang(posture,support);
 double control_body=0,tucked_body=0,control_target=0,tucked_target=0;
 double held_min=std::numeric_limits<double>::infinity(),held_max=-held_min;
 double held_work_start=0,held_work_end=0;
 for(int i=0;i<360;++i){tuck_tick(control,false,0,support,false);tuck_tick(posture,true,0,support,false);
  const auto a=control.snapshot(),b=posture.snapshot();
  if(i>=270){held_min=std::min(held_min,posture_command_metric(b));held_max=std::max(held_max,posture_command_metric(b));
   if(i==270){held_work_start=b.traversal_actuator_positive_work_j;}
   held_work_end=b.traversal_actuator_positive_work_j;}
  if(i>=315){control_body+=posture_actual_metric(a);tucked_body+=posture_actual_metric(b);
   control_target+=posture_command_metric(a);tucked_target+=posture_command_metric(b);}
 }
 control_body/=45;tucked_body/=45;control_target/=45;tucked_target/=45;
 require(control_body-tucked_body>.10,"regular hang tuck changes actual hand-to-COM distance",posture);
 require(std::abs(control_target-tucked_target-.25)<=.01&&held_max-held_min<=.01,
         "regular held posture reaches one bounded target without upward ratcheting",posture);
 double restored_body=0;
 for(int i=0;i<360;++i){tuck_tick(posture,false,0,support,false);if(i>=315)restored_body+=posture_actual_metric(posture.snapshot());}
 restored_body/=45;require(restored_body-tucked_body>.10,"regular explicit extension restores actual body distance",posture);
 // Jump retains the existing ledge mantle action while tucked, rather than
 // introducing a posture-specific release or transferring ownership silently.
 for(int i=0;i<90;++i)tuck_tick(control,true,0,support,false);
 require(control.request_jump(),"tucked ordinary ledge Jump accepted",control);tick(control);
 require(control.snapshot().traversal_state==scraperx::sim::TraversalState::Mantling,
         "ordinary ledge Jump still commits its validated mantle while tucked",control);
 tuck_readback_absent(control);(void)control.set_crouch_input(false);
 int mantle=0;for(;mantle<180&&!(control.snapshot().player_grounded&&control.snapshot().support_entity_id==support);++mantle)tick(control);
 require(mantle<180&&control.snapshot().death_count==0,"tucked ordinary mantle reaches actual ledge footing",control);
 // Drop releases without choosing a phase or adding an impulse. Compare actual
 // velocities over exactly one native tick, including ordinary gravity.
 for(int i=0;i<90;++i)tuck_tick(posture,true,0,support,false);
 const auto before=posture.snapshot();require(posture.request_release(),"tucked ordinary Drop accepted",posture);tick(posture);
 const auto after=posture.snapshot();
 require(after.traversal_state==scraperx::sim::TraversalState::None&&after.traversal_hand_constraint_count==0&&
   after.player_gravity_factor==1&&std::abs(after.player_linear_velocity.x-before.player_linear_velocity.x)<.05&&
   std::abs(after.player_linear_velocity.z-before.player_linear_velocity.z)<.05&&
   std::abs(after.player_linear_velocity.y-before.player_linear_velocity.y+9.81/90.)<.05,
   "tucked ordinary Drop preserves actual momentum with gravity only",posture);
 tuck_readback_absent(posture);(void)posture.set_crouch_input(false);
 std::cout<<"PASS TUCK regular_hang_entity="<<support<<" actual_rise_m="<<control_body-tucked_body
   <<" commanded_rise_m="<<control_target-tucked_target<<" held_target_span_m="<<held_max-held_min
   <<" held_tracking_positive_work_j="<<held_work_end-held_work_start<<" extension=1 ordinary_Jump_mantle=1 Drop_momentum=1\n";
}
void posture_unavailable_without_hands(){Simulation ground;tick(ground,90);
 require(ground.snapshot().player_grounded&&ground.snapshot().traversal_hand_constraint_count==0,"no-hand ground case is real supported footing",ground);
 require(ground.set_crouch_input(true),"ordinary grounded Crouch accepted",ground);tick(ground);tuck_readback_absent(ground);
 Simulation airborne(scraperx::sim::InitialSpawn::HighDrop,scraperx::sim::WorldContent::RegressionFixtures);
 require(airborne.set_crouch_input(true),"ordinary airborne Crouch accepted",airborne);tick(airborne);
 require(!airborne.snapshot().player_grounded&&airborne.snapshot().traversal_hand_constraint_count==0,
         "unavailable case is actual airborne body without hands",airborne);tuck_readback_absent(airborne);
}
void start_ordinary_posture_climb(Simulation &s,double heading_x=0,double heading_z=-1){
 // These are the existing normal-world north-frame catch inputs. No synthetic
 // ClimbApproach fixture or extra climbing geometry exists in this repository.
 require(s.debug_restart_at({16,88.9,-179.55})&&s.set_facing(heading_x,heading_z),"initial actual north frame stance",s);tick(s,45);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==1901&&s.snapshot().grip_available,
         "ordinary climb starts from real supported reachable grip",s);
 require(s.request_traversal(),"ordinary climb Action accepted",s);tick(s);
 (void)s.set_move_input(heading_x,heading_z);
 int elapsed=0;for(;elapsed<500&&s.snapshot().player_position.y<90;++elapsed)tick(s);
 (void)s.set_move_input(0,0);tick(s,30);const auto v=s.snapshot();
 std::cout<<"CLIMB ACQUIRE heading="<<heading_x<<','<<heading_z<<" elapsed="<<elapsed
   <<" state="<<int(v.traversal_state)<<" hands="<<v.traversal_hand_constraint_count
   <<" hand_support="<<v.traversal_support_entity_id<<" grounded="<<v.player_grounded
   <<" gravity="<<v.player_gravity_factor<<" eligible="<<v.swing_tuck_available
   <<" radial="<<v.hanging_free_arc_active<<" actual_below="<<v.swing_body_below_grip_m
   <<" hand_y="<<v.traversal_left_hand.y<<','<<v.traversal_right_hand.y
   <<" normal="<<v.traversal_normal.x<<','<<v.traversal_normal.z<<'\n';
 require(elapsed<500&&v.traversal_state==scraperx::sim::TraversalState::Climbing&&!v.player_swinging&&
   !v.player_grounded&&v.traversal_support_entity_id==1901&&v.traversal_hand_constraint_count==2&&v.player_gravity_factor==1&&
   v.swing_tuck_available&&!v.swing_tuck_requested&&v.death_count==0,
   "ordinary non-swing climb has actual eligible airborne hands",s);
}
void log_ordinary_posture(const char *phase,const Simulation &s){const auto v=s.snapshot();
 const double mid_x=.5*(v.traversal_left_hand.x+v.traversal_right_hand.x);
 const double mid_y=.5*(v.traversal_left_hand.y+v.traversal_right_hand.y);
 const double mid_z=.5*(v.traversal_left_hand.z+v.traversal_right_hand.z);
 std::cout<<"CLIMB STATE phase="<<phase<<" tick="<<v.tick_index
   <<" actual_com="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z
   <<" actual_midgrip="<<mid_x<<','<<mid_y<<','<<mid_z
   <<" commanded_com_y_from_readback="<<mid_y-v.swing_commanded_body_below_grip_m
   <<" actual_below="<<v.swing_body_below_grip_m<<" commanded_below="<<v.swing_commanded_body_below_grip_m
   <<" hand_y="<<v.traversal_left_hand.y<<','<<v.traversal_right_hand.y
   <<" generations="<<v.traversal_left_hand_generation<<','<<v.traversal_right_hand_generation
   <<" requested="<<v.swing_tuck_requested<<'\n';
}
void parkour_climb_posture(){Simulation control,posture;
 start_ordinary_posture_climb(control);start_ordinary_posture_climb(posture);
 require(control.snapshot().tick_index==posture.snapshot().tick_index,"paired ordinary climbs share initial native tick",posture);
 double normal_body=0,tucked_body=0,normal_target=0,tucked_target=0;
 double held_min=std::numeric_limits<double>::infinity(),held_max=-held_min;
 for(int i=0;i<360;++i){tuck_tick(control,false,0,1901,false);tuck_tick(posture,true,0,1901,false);
  const auto a=control.snapshot(),b=posture.snapshot();
  if(i>=270){held_min=std::min(held_min,posture_command_metric(b));held_max=std::max(held_max,posture_command_metric(b));}
  if(i>=315){normal_body+=posture_actual_metric(a);tucked_body+=posture_actual_metric(b);
   normal_target+=posture_command_metric(a);tucked_target+=posture_command_metric(b);}}
 normal_body/=45;tucked_body/=45;normal_target/=45;tucked_target/=45;
 std::cout<<"CLIMB TUCK metric="<<(posture.snapshot().hanging_free_arc_active?"radial_span":"world_vertical")<<" actual_rise_m="<<normal_body-tucked_body<<" commanded_rise_m="<<normal_target-tucked_target
   <<" held_target_span_m="<<held_max-held_min<<'\n';
 require(normal_body-tucked_body>.10&&std::abs(normal_target-tucked_target-.25)<=.01&&held_max-held_min<=.01,
         "ordinary climb has actual bounded posture with no upward ratchet",posture);
 log_ordinary_posture("tucked_before_ascent",posture);
 const auto generations=posture.snapshot();
 // One second of ordinary ascending input stays below the first top-out and
 // must regrip while preserving the explicit held posture command.
 for(int i=0;i<90;++i){tuck_tick(control,false,0,1901,false,-1,false);tuck_tick(posture,true,0,1901,false,-1);}
 const auto regripped=posture.snapshot();
 log_ordinary_posture("after_held_ascent_regrip",posture);
 require(regripped.traversal_left_hand_generation>generations.traversal_left_hand_generation||
   regripped.traversal_right_hand_generation>generations.traversal_right_hand_generation,
   "held posture survives actual ordinary climb regrips",posture);
 double regripped_tuck=0,extended_body=0,extended_target=0;
 held_min=std::numeric_limits<double>::infinity();held_max=-held_min;
 for(int i=0;i<180;++i){tuck_tick(control,false,0,1901,false);tuck_tick(posture,true,0,1901,false);
  if(i>=90){held_min=std::min(held_min,posture_command_metric(posture.snapshot()));
   held_max=std::max(held_max,posture_command_metric(posture.snapshot()));}
  if(i>=135)regripped_tuck+=posture_actual_metric(posture.snapshot());}
 log_ordinary_posture("settled_before_extension",posture);
 const auto extension_start=posture.snapshot();
 const double extension_mid_start=.5*(extension_start.traversal_left_hand.y+extension_start.traversal_right_hand.y);
 const double extension_command_start=extension_mid_start-extension_start.swing_commanded_body_below_grip_m;
 regripped_tuck/=45;require(held_max-held_min<=.01,"ordinary regripped held target does not ratchet",posture);
 for(int i=0;i<360;++i){tuck_tick(control,false,0,1901,false);tuck_tick(posture,false,0,1901,false);
  if(i>=315){extended_body+=posture_actual_metric(posture.snapshot());
   extended_target+=posture_command_metric(posture.snapshot());}}
 extended_body/=45;extended_target/=45;
 log_ordinary_posture("after_extension",posture);
 std::cout<<"CLIMB EXTEND metric="<<(posture.snapshot().hanging_free_arc_active?"radial_span":"world_vertical")<<" tucked_body_below_m="<<regripped_tuck<<" extended_body_below_m="<<extended_body
   <<" actual_extension_m="<<extended_body-regripped_tuck<<" commanded_body_below_m="<<extended_target
   <<" requested="<<posture.snapshot().swing_tuck_requested
   <<" regrips="<<regripped.traversal_left_hand_generation-generations.traversal_left_hand_generation
   <<','<<regripped.traversal_right_hand_generation-generations.traversal_right_hand_generation<<'\n';
 const auto extension_end=posture.snapshot();
 const double extension_mid_end=.5*(extension_end.traversal_left_hand.y+extension_end.traversal_right_hand.y);
 const double extension_command_end=extension_mid_end-extension_end.swing_commanded_body_below_grip_m;
 const double world_descent=extension_start.player_position.y-extension_end.player_position.y;
 const double command_descent=extension_command_start-extension_command_end;
 std::cout<<"CLIMB WORLD EXTEND actual_descent_m="<<world_descent<<" commanded_descent_m="<<command_descent
   <<" midgrip_descent_m="<<extension_mid_start-extension_mid_end<<'\n';
 // A stationary-frame hand can regrip lower while the body extends. Preserve
 // the actual world reference; report the old grip-relative metric separately.
 if(extension_start.hanging_free_arc_active&&extension_end.hanging_free_arc_active)
  require(extended_body-regripped_tuck>.10&&
          std::abs(extension_end.hanging_commanded_span_m-extension_start.hanging_commanded_span_m-.25)<=.01,
          "neutral radial extension restores actual body span and bounded rest length",posture);
 else require(world_descent>.10&&std::abs(command_descent-.25)<=.01,
              "ordinary climb extension restores actual world COM and commanded target",posture);
 if(std::abs(extension_mid_start-extension_mid_end)>.01)
  require(extension_end.traversal_left_hand_generation>extension_start.traversal_left_hand_generation||
          extension_end.traversal_right_hand_generation>extension_start.traversal_right_hand_generation,
          "extension grip-reference shift comes from an actual ordinary regrip",posture);
 const auto normal_start=control.snapshot(),resumed_start=posture.snapshot();
 // CHOSEN before runtime: allow10% rate difference to cover regrip transients,
 // while rejecting a persistent0.6m/s posture cap on the existing0.9m/s input.
 // The oracle uses actual paired neutral-input ascent, not a source-only rate.
 for(int i=0;i<180;++i){tuck_tick(control,false,0,1901,false,-1,false);tuck_tick(posture,false,0,1901,false,-1,false);}
 const auto a=control.snapshot(),b=posture.snapshot();
 const double normal_rise=a.player_position.y-normal_start.player_position.y;
 const double resumed_rise=b.player_position.y-resumed_start.player_position.y;
 std::cout<<"CLIMB RESUME control_rise_m="<<normal_rise<<" extended_rise_m="<<resumed_rise
   <<" control_rate_mps="<<normal_rise/2<<" extended_rate_mps="<<resumed_rise/2
   <<" rate_ratio="<<resumed_rise/normal_rise<<" requested="<<b.swing_tuck_requested
   <<" extended_body_below_m="<<extended_body<<" extended_target_below_m="<<extended_target
   <<" regrips="<<regripped.traversal_left_hand_generation-generations.traversal_left_hand_generation
   <<','<<regripped.traversal_right_hand_generation-generations.traversal_right_hand_generation<<'\n';
 require(a.tick_index==b.tick_index&&normal_rise>.20&&resumed_rise>=.90*normal_rise,
         "explicit extension restores meaningful ordinary climb rate relative to neutral control",posture);
 std::cout<<"PASS TUCK ordinary_non_swing_climb=1 posture=1 regrips=1 extension=1 resumed_rate=1\n";
}
void start_fixed_free_arc(Simulation &s){
 start_ordinary_posture_climb(s);
 require(s.snapshot().hanging_free_arc_active&&s.snapshot().hanging_commanded_span_m>0,
         "actual ordinary fixed-frame hands enter neutral radial controller",s);
}
void parkour_free_arc_small_lean(){Simulation s;start_tuck_swing(s);
 require(s.set_move_input(0,0),"actual bar neutral command accepted",s);tick(s);
 const auto before=s.snapshot();
 require(before.player_swinging&&before.hanging_free_arc_active&&before.traversal_hand_constraint_count==2,
         "actual caught hanging bar uses neutral radial hands",s);
 require(s.set_move_input(.1,0),"legacy small nonzero bar lean accepted",s);tick(s);
 const auto after=s.snapshot();const double work=after.traversal_actuator_positive_work_j-before.traversal_actuator_positive_work_j;
 std::cout<<"BAR SMALL LEAN x=.1 radial_before="<<before.hanging_free_arc_active
   <<" radial_after="<<after.hanging_free_arc_active<<" positive_target_work_j="<<work<<'\n';
 require(after.player_swinging&&!after.hanging_free_arc_active&&after.traversal_hand_constraint_count==2&&
         after.player_gravity_factor==1&&work>=0&&work<=500./90.+.00001&&
         vector_length(after.traversal_left_hand_force)<=1501&&vector_length(after.traversal_right_hand_force)<=1501,
         "legacy nonzero bar lean exits radial mode through finite native hands",s);
 std::cout<<"PASS FREE ARC small_bar_lean_handoff=1\n";
}
void parkour_free_arc(){Simulation control,posture;
 start_fixed_free_arc(control);start_fixed_free_arc(posture);
 const auto entry=control.snapshot();double low=1e9,high=-1e9;
 for(int i=0;i<360;++i){tuck_tick(control,false,0,1901,false);tuck_tick(posture,false,0,1901,false);
  const auto a=control.snapshot();
  require(a.hanging_free_arc_active&&std::abs(a.hanging_commanded_span_m-entry.hanging_commanded_span_m)<.00001&&
          a.traversal_actuator_positive_work_j==entry.traversal_actuator_positive_work_j,
          "neutral production arc retains fixed rest length and zero positive target work",control);
  if(i>=270){low=std::min(low,a.player_position.z);high=std::max(high,a.player_position.z);}}
 const auto late=control.snapshot();
 std::cout<<"PRODUCTION NEUTRAL FRAME entity=1901 late270_360_z_span_m="<<high-low
          <<" commanded_span_m="<<late.hanging_commanded_span_m<<" actual_span_m="<<late.hanging_actual_span_m
          <<" initial_velocity="<<entry.player_linear_velocity.x<<','<<entry.player_linear_velocity.y<<','<<entry.player_linear_velocity.z
          <<" late_velocity="<<late.player_linear_velocity.x<<','<<late.player_linear_velocity.y<<','<<late.player_linear_velocity.z<<'\n';
 // This real frame is not an established open, tangentially seeded fixture.
 // Lateral motion is diagnostic; the seeded native unit separately rejects pinning.
 double normal=0,tucked=0,normal_command=0,tucked_command=0,held_low=1e9,held_high=-1e9;
 for(int i=0;i<360;++i){tuck_tick(control,false,0,1901,false);tuck_tick(posture,true,0,1901,false);
  const auto a=control.snapshot(),b=posture.snapshot();
  require(a.hanging_free_arc_active&&b.hanging_free_arc_active,"neutral posture retains radial ownership",posture);
  if(i>=270){held_low=std::min(held_low,b.hanging_commanded_span_m);held_high=std::max(held_high,b.hanging_commanded_span_m);}
  if(i>=315){normal+=a.hanging_actual_span_m;tucked+=b.hanging_actual_span_m;
   normal_command+=a.hanging_commanded_span_m;tucked_command+=b.hanging_commanded_span_m;}}
 normal/=45;tucked/=45;normal_command/=45;tucked_command/=45;
 std::cout<<"PRODUCTION RADIAL TUCK actual_shortening_m="<<normal-tucked<<" command_shortening_m="
          <<normal_command-tucked_command<<" held_length_span_m="<<held_high-held_low<<'\n';
 require(normal-tucked>.10&&std::abs(normal_command-tucked_command-.25)<=.01&&held_high-held_low<=.01,
         "real radial tuck moves physical body and bounded held rest length",posture);
 double extended=0;
 for(int i=0;i<360;++i){tuck_tick(posture,false,0,1901,false);if(i>=315)extended+=posture.snapshot().hanging_actual_span_m;}
 extended/=45;require(extended-tucked>.10,"explicit radial extension restores actual body span",posture);
 const auto before=posture.snapshot();require(posture.request_release(),"real radial Drop accepted",posture);tick(posture);
 const auto after=posture.snapshot();
 require(!after.hanging_free_arc_active&&after.hanging_commanded_span_m==0&&after.hanging_actual_span_m==0&&
         after.traversal_hand_constraint_count==0&&after.player_gravity_factor==1&&
         std::abs(after.player_linear_velocity.x-before.player_linear_velocity.x)<.05&&
         std::abs(after.player_linear_velocity.z-before.player_linear_velocity.z)<.05&&
         std::abs(after.player_linear_velocity.y-before.player_linear_velocity.y+9.81/90.)<.05,
         "radial Drop preserves actual departure velocity with gravity only",posture);
 tuck_readback_absent(posture);
 parkour_climb_posture();parkour_free_arc_small_lean();
 std::cout<<"PASS NEUTRAL FRAME fixed_length=1 radial_tuck=1 extension=1 real_Drop=1 forward_climb_resume=1 small_bar_lean=1\n";
}
void log_thin_edge(const char *phase,const Simulation &s){const auto v=s.snapshot();
 std::cout<<"THIN EDGE phase="<<phase<<" tick="<<v.tick_index<<" position="
  <<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z<<" velocity="
  <<v.player_linear_velocity.x<<','<<v.player_linear_velocity.y<<','<<v.player_linear_velocity.z
  <<" state="<<int(v.traversal_state)<<" support="<<v.traversal_support_entity_id
  <<" hands="<<v.traversal_hand_constraint_count<<" grounded="<<v.player_grounded
  <<" gravity="<<v.player_gravity_factor<<" radial="<<v.hanging_free_arc_active<<'\n';
}
void thin_edge_refusal(const char *name,scraperx::sim::Vector3 initial,double facing_z){Simulation s;
 require(s.debug_restart_at(initial)&&s.set_facing(0,facing_z)&&s.set_crouch_input(false)&&
         s.set_move_input(0,.3*facing_z),"initial negative thin-edge approach",s);
 for(int i=0;i<180;++i){tick(s);const auto v=s.snapshot();
  require(!(v.traversal_support_entity_id==1800&&v.traversal_hand_constraint_count>0),
          "invalid approach refuses target shelf hands",s);}
 log_thin_edge(name,s);
 require(s.snapshot().player_position.y<initial.y-.20&&s.snapshot().death_count==0,
         "refused shelf approach has actual physical fall",s);
 std::cout<<"PASS THIN EDGE refusal="<<name<<" target=1800 actual_fall=1\n";
}
void parkour_thin_edge(){
 // Existing geometry only: top63.55, underside63.27, south edgeZ-137.65.
 // These inputs cannot discover the target when facing away, already below
 // the1.35m reach band, or too distant while falling through that band.
 thin_edge_refusal("facing_away",{34.4,62.8,-137.0},1);
 thin_edge_refusal("below_reach_band",{34.4,61.9,-137.0},-1);
 thin_edge_refusal("too_far_forward",{34.4,62.8,-135.5},-1);
 Simulation s;require(s.debug_restart_at({34.4,62.8,-137.0})&&s.set_facing(0,-1)&&
   s.set_move_input(0,-.3)&&s.set_crouch_input(false),"initial airborne thin-edge approach only",s);
 int elapsed=0;for(;elapsed<180;++elapsed){tick(s);const auto v=s.snapshot();
  if(elapsed%30==0)log_thin_edge("approach",s);
  if(v.traversal_state==scraperx::sim::TraversalState::Hanging&&v.traversal_support_entity_id==1800&&
     v.traversal_hand_constraint_count==2&&v.player_gravity_factor==1)break;}
 log_thin_edge("acquisition_end",s);
 require(elapsed<180,"ordinary airborne approach acquires actual two-hand Hanging shelf1800",s);
 require(s.set_move_input(0,0),"neutral after actual catch",s);double upward_load=0;
 for(int i=0;i<30;++i){tuck_tick(s,false,0,1800,false);
  const auto v=s.snapshot();require(v.traversal_state==scraperx::sim::TraversalState::Hanging&&
   !v.player_grounded&&v.death_count==0,"thin shelf sustains actual airborne Hanging",s);
  upward_load+=v.traversal_left_hand_force.y+v.traversal_right_hand_force.y;}
 log_thin_edge("neutral_loaded",s);
 // A flag-only attachment cannot carry this85kg body: require measured mean
 // upward native force above100N during the short30tick real loading interval.
 require(upward_load/30>100,"actual finite hands bear sustained thin-edge load",s);
 const auto before=s.snapshot();require(s.request_release(),"ordinary thin-edge Drop accepted",s);tick(s);
 const auto after=s.snapshot();log_thin_edge("after_Drop",s);
 require(after.traversal_hand_constraint_count==0&&!after.hanging_free_arc_active&&after.player_gravity_factor==1&&
  std::abs(after.player_linear_velocity.x-before.player_linear_velocity.x)<.05&&
  std::abs(after.player_linear_velocity.z-before.player_linear_velocity.z)<.05&&
  std::abs(after.player_linear_velocity.y-before.player_linear_velocity.y+9.81/90.)<.05,
  "thin-edge Drop preserves actual departure velocity with gravity only",s);
 tuck_readback_absent(s);
 std::cout<<"PASS THIN EDGE ordinary_airborne_discovery=1 target=1800 neutral_radial=1 mean_upward_load_N="
  <<upward_load/30<<" actual_Drop_momentum=1 initial_staging_only=1\n";
}
void cross_swing_tucked(Simulation &s){catch_swing(s);bool released=false,saw_tuck=false,saw_extension=false;
 int ticks=0;double peak=-std::numeric_limits<double>::infinity();
 for(;ticks<180*90;++ticks){const auto v=s.snapshot();
  const double lean=v.player_linear_velocity.x-1.2*(v.traversal_left_hand.x+36.95)>0?1:-1;
  // A test controller chooses these visible phases; production receives only inputs.
  const bool tuck=v.player_linear_velocity.x>0&&v.player_position.x>-37.7;
  tuck_tick(s,tuck,lean);saw_tuck|=tuck;saw_extension|=!tuck;const auto a=s.snapshot();
  peak=std::max(peak,a.player_position.x);
  if(a.player_position.x>-32.4&&a.player_linear_velocity.x>2.5&&a.player_linear_velocity.y>1.5&&
     a.swing_tuck_requested&&a.swing_commanded_body_below_grip_m<.8){
   require(saw_tuck&&saw_extension,"encounter deliberately uses both tuck and extension",s);
   passive_swing_release(s);tuck_readback_absent(s);released=true;break;
  }
 }
 require(released,"manual lean plus timed tucked rising release reaches receiver window",s);
 (void)s.set_crouch_input(false);(void)s.set_move_input(0,0);tick(s,130);
 const auto landed=s.snapshot();
 require(landed.player_grounded&&landed.support_entity_id==1985&&landed.player_position.x>-30.05,
         "timed posture and earned momentum land on real receiver1985",s);
 walk(s,-24.7,-160.45);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&s.snapshot().death_count==0,
         "tuck encounter exits by ordinary walking to Tower11",s);
 std::cout<<"PASS TUCK encounter timed_tuck_extend=1 existing_manual_lean=1 tucked_momentum_release=1 receiver=1985 Tower=11 deaths=0 peak_x="
          <<peak<<" pump_ticks="<<ticks<<'\n';
}
void cross_swing(Simulation &s){
 catch_swing(s);
 double peak=-std::numeric_limits<double>::infinity();bool released=false;int ticks=0;
 for(;ticks<180*90;++ticks){const auto v=s.snapshot();
  // Inputs respond to the visible swing, without changing body poses or forces.
  const double vx=v.player_linear_velocity.x;
  (void)s.set_move_input(vx-1.2*(v.traversal_left_hand.x+36.95)>0?1:-1,0);
  tick(s);const auto a=s.snapshot();peak=std::max(peak,a.player_position.x);
  require(a.traversal_actuator_positive_work_j-v.traversal_actuator_positive_work_j<=500.0/90.0+.00001,
          "swing real positive rest-target work stays within500W",s);
  if(ticks%450==0)std::cerr<<"SWING t="<<ticks/90.<<" x="<<a.player_position.x<<" y="<<a.player_position.y<<" vx="<<a.player_linear_velocity.x<<" vy="<<a.player_linear_velocity.y<<" spring_actuator_work="<<a.traversal_actuator_positive_work_j<<'\n';
  require(a.player_swinging&&a.traversal_hand_constraint_count==2,"real hanging swing remains attached",s);
  if(a.player_position.x>-32.4&&a.player_linear_velocity.x>2.5&&a.player_linear_velocity.y>1.5){
   passive_swing_release(s);released=true;break;
  }
 }
 require(released,"human pumping reaches rising release window",s);
 (void)s.set_move_input(0,0);tick(s,130);
 auto v=s.snapshot();std::cerr<<"SWING RELEASE result pos="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z<<" support="<<v.support_entity_id<<" traversal="<<int(v.traversal_state)<<'\n';
 require(v.player_grounded&&v.support_entity_id==1985&&v.player_position.x>-30.05,"earned momentum lands on narrow receiver",s);
 walk(s,-24.7,-160.45);require(s.snapshot().support_entity_id==11&&s.snapshot().death_count==0,"swing stable198m Tower exit",s);
 std::cout<<"PASS PARKOUR ordinary jump/catch/player-pump/rising-release/receiver/Tower198 peak_x="<<peak<<" pump_ticks="<<ticks<<" ordinary_inputs_only=1\n";
}
void swing_miss_recovery(Simulation &s,bool posture=false){
 catch_swing(s);
 if(posture){for(int i=0;i<45;++i)tuck_tick(s,true);for(int i=0;i<45;++i)tuck_tick(s,false);}
 // Deliberately let go over the gap near the unpumped forward turnaround:
 // there is insufficient horizontal departure speed to reach the receiver.
 int miss=0;for(;miss<10*90;++miss) {const auto v=s.snapshot();
  if(v.player_position.x>-36.4 && v.player_linear_velocity.x>0 && v.player_linear_velocity.x<2.5)break;
  tick(s);
 }
 require(miss<10*90,"neutral swing reaches a real poorly timed release opportunity",s);
 passive_swing_release(s);
 if(posture)tuck_readback_absent(s);
 for(int i=0;i<5*90&&!s.snapshot().player_grounded;++i)tick(s);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==scraperx::sim::kParkourRecoveryEntity,
         "poorly timed release reaches real lower recovery footing",s);
 walk(s,-37.0,-159.8);(void)s.set_facing(-1,0);(void)s.request_traversal();tick(s);
 require(s.snapshot().traversal_hand_constraint_count==2,"recovery ladder catch uses real hands",s);
 (void)s.set_move_input(-1,0);int climb=0;
 for(;climb<35*90;++climb){tick(s);const auto v=s.snapshot();
  if(v.player_grounded&&v.support_entity_id==1985&&v.player_position.y>198.7&&v.traversal_hand_constraint_count==0)break;
 }
 require(climb<35*90&&s.snapshot().death_count==0,"ladder physically recovers to start platform",s);
 (void)s.set_move_input(0,0);tick(s,90);walk(s,-41.0759,-160.45);
 std::cout<<"PASS PARKOUR missed-release/recovery-footing/real-ladder/retry ordinary_inputs_only=1\n";
}
void parkour(){Simulation s;require(s.debug_restart_at({-41.0759,198.9,-160.45}),"initial supported198m staging only",s);tick(s,90);
 swing_miss_recovery(s);
 cross_swing(s);
}
void parkour_tuck(){posture_unavailable_without_hands();tuck_posture();tuck_schedule_falsifier();
 parkour_climb_posture();
 regular_hang_posture(scraperx::sim::InitialSpawn::HangApproach,Simulation::kHangLedgeEntityId);
 regular_hang_posture(scraperx::sim::InitialSpawn::MovingLedgeApproach,Simulation::kMovingLedgeEntityId);Simulation s;
 require(s.debug_restart_at({-41.0759,198.9,-160.45}),"initial supported198m tuck encounter staging only",s);
 tick(s,90);tuck_readback_absent(s);swing_miss_recovery(s,true);cross_swing_tucked(s);
}
void parkour_ledge_hang(){
 regular_hang_posture(scraperx::sim::InitialSpawn::HangApproach,Simulation::kHangLedgeEntityId);
 regular_hang_posture(scraperx::sim::InitialSpawn::MovingLedgeApproach,Simulation::kMovingLedgeEntityId);
}
int main(int argc,char**argv){if(argc>1){if(std::string(argv[1])=="balance")balance();else if(std::string(argv[1])=="tram")tram();else if(std::string(argv[1])=="helix")helix();else if(std::string(argv[1])=="cascade")cascade();else if(std::string(argv[1])=="pitman")pitman();else if(std::string(argv[1])=="parkour")parkour();else if(std::string(argv[1])=="parkour_tuck")parkour_tuck();else if(std::string(argv[1])=="parkour_climb_posture")parkour_climb_posture();else if(std::string(argv[1])=="parkour_free_arc")parkour_free_arc();else if(std::string(argv[1])=="parkour_free_arc_small_lean")parkour_free_arc_small_lean();else if(std::string(argv[1])=="parkour_thin_edge")parkour_thin_edge();else if(std::string(argv[1])=="parkour_ledge_hang")parkour_ledge_hang();else return 2;}else{balance();crown();tram();helix();cascade();pitman();}}
