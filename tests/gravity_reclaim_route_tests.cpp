#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using scraperx::sim::Simulation;
void require(bool ok,const char*why,const Simulation&s){if(ok)return;const auto v=s.snapshot();std::cerr<<"FAIL RECLAIM "<<why<<" pos="<<v.player_position.x<<','<<v.player_position.y<<','<<v.player_position.z<<" support="<<v.support_entity_id<<" station="<<int(v.supplied_machine_station)<<" machine="<<int(v.supplied_machine_index)<<" deck="<<v.supplied_machine_surface_y<<" angle="<<v.supplied_machine_wheel_angle_rad<<" hopper="<<v.supplied_machine_hopper_mass_kg<<" energy="<<v.supplied_machine_energy_j<<'\n';std::exit(1);}
void tick(Simulation&s,int n=1){while(n-->0)require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"native tick",s);}
scraperx::sim::Vector3 cabin_floor(const Simulation&s){
 const auto p=s.kit_body_position(s.kit_body_index(2201));const auto q=s.kit_body_rotation(s.kit_body_index(2201));
 return {p.x+5*(q.w*q.z-q.x*q.y),p.y-2.5*(1-2*(q.x*q.x+q.z*q.z)),p.z-5*(q.y*q.z+q.w*q.x)};
}
void walk(Simulation&s,double x,double z){
 const auto start=s.snapshot().player_position;const int budget=700+int(std::ceil(std::hypot(x-start.x,z-start.z)/5.5))*90;
 for(int i=0;i<budget;++i){const auto v=s.snapshot();const double dx=x-v.player_position.x,dz=z-v.player_position.z;
  (void)s.set_move_input(std::clamp(dx*1.8-v.player_linear_velocity.x*.28,-1.,1.),std::clamp(dz*1.8-v.player_linear_velocity.z*.28,-1.,1.));tick(s);
  if(std::hypot(dx,dz)<.06&&std::hypot(v.player_linear_velocity.x,v.player_linear_velocity.z)<.1&&v.player_grounded)break;
 }
 (void)s.set_move_input(0,0);tick(s,30);require(std::hypot(s.snapshot().player_position.x-x,s.snapshot().player_position.z-z)<.18,"ordinary movement reaches target",s);
}
int main(){
 Simulation s;require(s.debug_restart_at({-24.7,308.9,-142.65}),"initial staging only",s);tick(s,90);
 walk(s,-24.7,-158.15);walk(s,-33.4,-158.25);
 require(s.snapshot().supplied_machine_index==6&&s.snapshot().supplied_machine_station==2,"308 approach reaches physical feed control",s);
 require(s.can_restart_gravity_reclaim_attempt(),"safe pre-consumption encounter checkpoint is available",s);
 const double inventory=s.snapshot().supplied_machine_hopper_mass_kg;
 const auto fracture_serial_at_entry=s.snapshot().reclaim_break_serial;
 std::cout<<"RECLAIM entry_fractures="<<fracture_serial_at_entry<<'\n';
 require(inventory>1500,"finite authored material inventory",s);
 (void)s.set_supplied_machine_input(1);tick(s,18*90);
 const auto open_gate=s.kit_body_position(s.kit_body_index(2202));std::cout<<"RECLAIM open_gate="<<open_gate.x<<","<<open_gate.y<<","<<open_gate.z<<"\n";
 for(unsigned e=2219;e<2299;e+=8){const auto p=s.kit_body_position(s.kit_body_index(e));std::cout<<"MATERIAL "<<e<<" "<<p.x<<","<<p.y<<","<<p.z<<"\n";}
 (void)s.set_supplied_machine_input(0);tick(s,4*90);
 std::cout<<"RECLAIM feed before="<<inventory<<" remaining="<<s.snapshot().supplied_machine_hopper_mass_kg<<" deck="<<s.snapshot().supplied_machine_surface_y<<" contacts="<<s.snapshot().reclaim_impact_count<<'\n';
 std::cout<<"RECLAIM actual_fractures="<<s.snapshot().reclaim_break_serial<<" last_load_n="<<s.snapshot().reclaim_break_force_n<<" last_torque_nm="<<s.snapshot().reclaim_break_torque_nm<<'\n';
 require(s.snapshot().supplied_machine_hopper_mass_kg<inventory-800,"actual material passes gate/chute",s);
 require(std::abs(s.snapshot().supplied_machine_surface_y-308)<.1&&s.snapshot().supplied_machine_braking,"loaded wheel stays on finite brake",s);
 const auto gate=s.kit_body_position(s.kit_body_index(2202));std::cout<<"RECLAIM gate="<<gate.x<<","<<gate.y<<","<<gate.z<<"\n";
 const double cabin_x=-44+std::sqrt(12.5*12.5-11*11);
 walk(s,cabin_x-.65,-158.5);(void)s.request_jump();tick(s);walk(s,cabin_x-.65,-160.10);
 require(s.snapshot().player_grounded&&s.snapshot().supplied_machine_station==1&&s.snapshot().supplied_machine_index==6,"ordinary boarding and reachable cabin control",s);
 // Enter beside the post, then stand over the real suspension centre.
 // Off-centre rider loading must remain free to tilt the hanging cabin.
 walk(s,cabin_floor(s).x,-160.65);
 const double start_energy=s.snapshot().supplied_machine_energy_j;
 (void)s.set_supplied_machine_input(1);tick(s,3*90);
 require(s.snapshot().supplied_machine_surface_y>309,"falling/retained load raises real rider",s);
 (void)s.set_supplied_machine_input(0);
 int settled=0;double previous_angle=s.snapshot().supplied_machine_wheel_angle_rad;
 double previous_floor=s.snapshot().supplied_machine_surface_y;
 for(int i=0;i<8*90&&settled<30;++i){tick(s);const double angle=s.snapshot().supplied_machine_wheel_angle_rad;
  const double floor=s.snapshot().supplied_machine_surface_y;
  settled=std::abs(angle-previous_angle)<.00005&&std::abs(floor-previous_floor)<.0002?settled+1:0;
  previous_angle=angle;previous_floor=floor;}
 require(settled>=30,"finite bearing brake and passive suspension settle real inertia",s);
 const double held=s.snapshot().supplied_machine_surface_y;tick(s,90);
 require(std::abs(s.snapshot().supplied_machine_surface_y-held)<.04&&s.snapshot().supplied_machine_braking,"finite wheel brake stops and holds loaded cabin",s);
 (void)s.set_supplied_machine_input(1);int elapsed=0;
 for(;elapsed<65*90;++elapsed){tick(s);const auto v=s.snapshot();
  require(v.player_grounded&&v.supplied_machine_station==1,"rider retains actual footing and control",s);
  if(v.supplied_machine_surface_y>329.97)break;
 }
 require(elapsed<65*90,"gravity load reaches330 exit",s);(void)s.set_supplied_machine_input(0);tick(s,90);
 require(!s.snapshot().supplied_machine_wheel_motor_enabled&&s.snapshot().supplied_machine_energy_j<=start_energy&&
  s.snapshot().supplied_machine_energy_j>0&&!s.snapshot().supplied_machine_energy_cutoff,
  "wheel stays passive while any gate closure uses the finite actuator bank",s);
 const double x=cabin_floor(s).x-.65;walk(s,x,-160.0);(void)s.request_jump();tick(s);walk(s,x,-158.25);walk(s,-24.7,-158.25);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==11&&std::abs(s.snapshot().player_position.y-330.9)<.12&&s.snapshot().death_count==0,"stable onward Tower330 arrival",s);
 walk(s,-33.4,-158.25);require(s.snapshot().supplied_machine_station==3,"actual upper discharge control",s);
 const double discharge_energy=s.snapshot().supplied_machine_energy_j;
 (void)s.set_supplied_machine_input(-1);int returning=0;
 for(;returning<85*90;++returning){tick(s);if(s.snapshot().supplied_machine_surface_y<308.03)break;}
 const auto cabin_rotation=s.kit_body_rotation(s.kit_body_index(2201));
 const auto cabin_velocity=s.kit_body_velocity(s.kit_body_index(2201));
 std::cout<<"RECLAIM return cabin_q="<<cabin_rotation.x<<','<<cabin_rotation.y<<','<<cabin_rotation.z<<','<<cabin_rotation.w
          <<" velocity="<<cabin_velocity.x<<','<<cabin_velocity.y<<','<<cabin_velocity.z<<'\n';
 require(returning<85*90,"real discharge permits empty gravity return",s);
 (void)s.set_supplied_machine_input(0);tick(s,90);
 require(s.snapshot().supplied_machine_energy_j<discharge_energy-100&&s.snapshot().supplied_machine_energy_j>=0,"door work uses finite source",s);
 require(s.snapshot().reclaim_impact_count>0,"actual material contacts produce native audio receipts",s);
 require(s.snapshot().reclaim_break_serial>fracture_serial_at_entry,"actual feeding/discharge loads break a selected native brick after entry",s);
 const auto deaths=s.snapshot().death_count;
 const auto fracture_serial=s.snapshot().reclaim_break_serial;
 require(s.restart_gravity_reclaim_attempt(),"explicit retry restores the saved complete encounter",s);
 require(!s.snapshot().reclaim_break_valid&&s.snapshot().reclaim_break_serial==fracture_serial,"retry restores physical pieces without replaying a fracture",s);tick(s,90);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id==1941&&s.snapshot().death_count==deaths,
  "retry restores actual safe feeding approach without a death",s);
 require(s.snapshot().supplied_machine_hopper_mass_kg>inventory-100&&
  std::abs(s.snapshot().supplied_machine_surface_y-308)<.05,
  "retry restores finite saved material and real lower cabin",s);
 walk(s,-33.4,-158.25);
 (void)s.set_supplied_machine_input(1);tick(s,18*90);(void)s.set_supplied_machine_input(0);tick(s,4*90);
 require(s.snapshot().supplied_machine_hopper_mass_kg<inventory-800,"retry feeds the restored actual charge",s);
 walk(s,cabin_x-.65,-158.5);(void)s.request_jump();tick(s);walk(s,cabin_x-.65,-160.10);
 require(s.snapshot().player_grounded&&s.snapshot().supplied_machine_station==1,"retry uses ordinary actual reboarding",s);
 walk(s,cabin_floor(s).x,-160.65);
 (void)s.set_supplied_machine_input(1);int retry_ticks=0;
 for(;retry_ticks<65*90;++retry_ticks){tick(s);require(s.snapshot().player_grounded,"retry rider has real footing",s);
  if(s.snapshot().supplied_machine_surface_y>329.97)break;}
 require(retry_ticks<65*90,"restored finite load earns a second ascent",s);
 (void)s.set_supplied_machine_input(0);
 tick(s,90);
 // A deliberate late lateral miss bypasses the330m tongue. The existing
 // lower maintenance crossing must catch the actual fall, with the native
 // heavy-landing consequence intact and a safe ordinary walk out.
 (void)s.set_move_input(-1,.7);(void)s.request_jump();tick(s,35);
 walk(s,-41.6,-158.25);
 const auto recovery=s.snapshot();
 const auto soles_y=recovery.player_position.y-(recovery.player_crouched?.6:.9);
 std::cout<<"RECLAIM recovery soles_y="<<soles_y<<" compact="<<recovery.player_crouched<<
     " impact_mps="<<recovery.landing_normal_speed_mps<<'\n';
 require(recovery.support_entity_id==1945&&std::abs(soles_y-319)<.02&&
         recovery.death_count==deaths,
         "late exit miss lands on the real319m recovery crossing",s);
 require(s.snapshot().landing_normal_speed_mps>8,
         "recovery crossing retains a consequential actual landing",s);
 // The real recall panel at(-40.5,-158.25) obstructs a straight walk.
 // Use the open strip beside it, then return to the crossing centre.
 walk(s,-41.6,-158.75);
 walk(s,-39.6,-158.75);
 walk(s,-39.6,-158.25);
 walk(s,-24.7,-158.25);
 require(s.snapshot().player_grounded&&s.snapshot().support_entity_id!=2201,
         "miss recovery reaches supported onward Tower footing",s);
 require(s.restart_gravity_reclaim_attempt(),"recovered miss has the explicit finite-charge retry",s);
 require(s.snapshot().supplied_machine_hopper_mass_kg>inventory-.1&&
         s.snapshot().death_count==deaths,"recovery retry restores saved charge without a death grant",s);
 std::cout<<"PASS RECLAIM approach/finite-feed/loaded-gravity-ascent/brake/330exit/actual-discharge/empty-return/explicit-checkpoint-retry/second-loaded-ascent/late-miss319-recovery/Tower-exit initial_staging_only=1\n";
}
