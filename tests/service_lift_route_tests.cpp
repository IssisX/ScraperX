#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using scraperx::sim::Simulation;
namespace {
void require(bool ok,const char*why,const Simulation&s){if(ok)return;auto v=s.snapshot();std::cerr<<"FAIL AS027 "<<why<<" pos="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z<<" support="<<v.support_entity_id<<" station="<<int(v.service_lift_station)<<" energy="<<v.service_lift_energy_j<<'\n';std::exit(1);}
void tick(Simulation&s,int n=1){while(n-->0)require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"tick accepted",s);}
void walk(Simulation&s,double x,double z){
 for(int i=0;i<450;++i){auto v=s.snapshot();const double dx=x-v.player_position.x,dz=z-v.player_position.z;
  (void)s.set_move_input(std::clamp(dx*1.8-v.player_linear_velocity.x*.28,-1.,1.),std::clamp(dz*1.8-v.player_linear_velocity.z*.28,-1.,1.));tick(s);
  if(std::hypot(dx,dz)<.05&&std::hypot(v.player_linear_velocity.x,v.player_linear_velocity.z)<.1)break;
 }
 (void)s.set_move_input(0,0);tick(s,45);require(std::hypot(s.snapshot().player_position.x-x,s.snapshot().player_position.z-z)<.15,"walk reaches target",s);
}
struct LoadSample{double force=0,work=0;};
LoadSample drive_sample(Simulation&s){
 const double before=s.snapshot().service_lift_energy_j;LoadSample sample;
 (void)s.set_service_lift_input(1);
 for(int i=0;i<360;++i){tick(s);if(i>=270)sample.force+=std::abs(s.snapshot().service_lift_force_n)/90;}
 sample.work=before-s.snapshot().service_lift_energy_j;return sample;
}
LoadSample unloaded(){Simulation s;require(s.debug_restart_at({-27.4,121.9,-162}),"lower station staging",s);tick(s,180);require(s.snapshot().service_lift_station==2,"reachable lower call station",s);auto result=drive_sample(s);walk(s,-24.7,-162);require(s.snapshot().service_lift_station==0&&s.snapshot().service_lift_braking,"walking away releases stale effort",s);return result;}
}
int main(){
 const auto empty=unloaded();
 Simulation s;require(s.debug_restart_at({-25,121.9,-162}),"121m ring fixture staging",s);tick(s,180);
 require(!s.set_service_lift_input(std::numeric_limits<double>::quiet_NaN()),"reject nonfinite command",s);
 (void)s.set_service_lift_input(1);tick(s,90);
 require(s.snapshot().service_lift_station==0&&s.snapshot().service_lift_braking&&s.snapshot().service_lift_energy_j==s.snapshot().service_lift_capacity_j,"remote effort cannot drive",s);
 (void)s.set_service_lift_input(0);walk(s,-27.4,-162);require(s.snapshot().service_lift_station==2,"approach reaches lower control",s);
 walk(s,-30.8,-162);require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==2973&&s.snapshot().service_lift_station==1,"physical boarding reaches deck control",s);
 const auto loaded=drive_sample(s);
 std::cout<<"AS027_LOAD unloaded_force="<<empty.force<<" loaded_force="<<loaded.force<<" unloaded_draw="<<empty.work<<" loaded_draw="<<loaded.work<<'\n';
 require(loaded.force>empty.force+500&&loaded.work>empty.work+200,"actual rider increases actuator demand",s);
 const auto moving=s.snapshot();
 require(moving.player_grounded&&moving.support_entity_id==2973&&
         moving.support_point_linear_velocity.y>.5,"moving restart starts on real rising footing",s);
 require(s.restart_checkpoint(),"moving deck checkpoint restore accepted",s);
 const auto restored=s.snapshot();const auto deck_velocity=s.kit_body_velocity(s.kit_body_index(2973));
 std::cout<<"AS027_RESTORE player_vy="<<restored.player_linear_velocity.y<<
            " deck_vy="<<deck_velocity.y<<" fresh_grounded="<<restored.player_grounded<<'\n';
 require(!restored.player_grounded&&std::abs(deck_velocity.y)>.5&&
         std::abs(restored.player_linear_velocity.y-deck_velocity.y)<.02,
         "restart preserves restored deck transport without publishing stale footing",s);
 tick(s,30);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==2973,
         "neutral restored rider earns fresh deck contact",s);
 // Restart engaged the brake. Resume real motion so the existing release
 // check still measures braking a driven load, rather than an idle hold.
 (void)s.set_service_lift_input(1);tick(s,180);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==2973&&
         s.snapshot().support_point_linear_velocity.y>.5&&!s.snapshot().service_lift_braking,
         "ordinary release starts from genuine resumed rising drive",s);
 (void)s.set_service_lift_input(0);const double stop_y=s.snapshot().service_lift_surface_y;double max_drop=0;
 for(int i=0;i<180;++i){tick(s);max_drop=std::max(max_drop,stop_y-s.snapshot().service_lift_surface_y);}
 require(max_drop<.02&&s.snapshot().service_lift_braking,"release applies finite brake within20mm",s);
 const double energy=s.snapshot().service_lift_energy_j;
 require(s.restart_checkpoint(),"checkpoint restore accepted",s);require(std::abs(s.snapshot().service_lift_energy_j-energy)<.01,"checkpoint preserves spent energy",s);tick(s,30);require(s.snapshot().service_lift_braking,"restart clears held command",s);
 (void)s.set_service_lift_input(1);
 int ticks=0;for(;ticks<65*90;++ticks){tick(s);require(s.snapshot().service_lift_station==1,"deck control remains reachable",s);if(s.snapshot().service_lift_surface_y>=142.99)break;}
 require(ticks<65*90,"finite drive reaches receiver height",s);
 (void)s.set_service_lift_input(0);tick(s,180);walk(s,-24.7,-162);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-143.9)<.1,"ordinary exit reaches143m Tower11",s);
 walk(s,-27.4,-162);require(s.snapshot().service_lift_station==3,"upper call station is reachable",s);
 (void)s.set_service_lift_input(1);tick(s,15*90);(void)s.set_service_lift_input(0);tick(s,90);
 require(s.snapshot().service_lift_surface_y>144.2&&s.snapshot().service_lift_surface_y<144.4,"physical full-stroke limit gives23.3m travel",s);
 (void)s.set_service_lift_input(-1);for(int i=0;i<20*90;++i){tick(s);if(s.snapshot().service_lift_surface_y<=143)break;}
 (void)s.set_service_lift_input(0);tick(s,90);require(std::abs(s.snapshot().service_lift_surface_y-143)<.04,"upper call reverses overshoot",s);
 walk(s,-30.8,-162);require(s.snapshot().service_lift_station==1,"reboard after recovery",s);
 (void)s.set_service_lift_input(-1);tick(s,180);
 const double support_vy=s.snapshot().support_point_linear_velocity.y;
 (void)s.request_jump();tick(s);const double takeoff_vy=s.snapshot().player_linear_velocity.y;
 std::cout<<"AS027_DEPART support_vy="<<support_vy<<" takeoff_vy="<<takeoff_vy<<" brake_drop="<<max_drop<<'\n';
 require(support_vy<-.1&&takeoff_vy-support_vy>5.0&&takeoff_vy-support_vy<5.6,"departure preserves support momentum with finite push-off",s);
 tick(s,90);require(s.snapshot().service_lift_braking,"airborne operator loses drive authority",s);
 require(s.snapshot().service_lift_retry_available,"first command records explicit attempt checkpoint",s);
 require(s.restart_service_lift_attempt(),"explicit attempt restart accepted",s);tick(s,90);
 require(s.snapshot().service_lift_braking&&s.snapshot().service_lift_surface_y<121.1&&std::abs(s.snapshot().service_lift_energy_j-s.snapshot().service_lift_capacity_j)<.01&&s.snapshot().player_grounded,"explicit retry restores earlier bodies, energy and supported player",s);
 std::cout<<"PASS AS027 native approach/boarding/weight/release/checkpoint/ride/exit/limits/reverse/departure; initial fixture staging only\n";
}
