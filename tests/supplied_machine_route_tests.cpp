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
void parkour(){Simulation s;require(s.debug_restart_at({-41.0759,198.9,-160.45}),"initial supported198m staging only",s);tick(s,90);
 catch_swing(s);
 // Deliberately let go over the gap near the unpumped forward turnaround:
 // there is insufficient horizontal departure speed to reach the receiver.
 int miss=0;for(;miss<10*90;++miss) {const auto v=s.snapshot();
  if(v.player_position.x>-36.4 && v.player_linear_velocity.x>0 && v.player_linear_velocity.x<2.5)break;
  tick(s);
 }
 require(miss<10*90,"neutral swing reaches a real poorly timed release opportunity",s);
 passive_swing_release(s);
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
 cross_swing(s);
}
int main(int argc,char**argv){if(argc>1){if(std::string(argv[1])=="tram")tram();else if(std::string(argv[1])=="helix")helix();else if(std::string(argv[1])=="cascade")cascade();else if(std::string(argv[1])=="pitman")pitman();else if(std::string(argv[1])=="parkour")parkour();else return 2;}else{balance();crown();tram();helix();cascade();pitman();}}
