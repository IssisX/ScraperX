#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <cstdlib>
using namespace scraperx::sim;
void need(bool p,const char*m){if(!p){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
void step(Simulation&s,int n=1){while(n-->0)need(s.advance_frame(Simulation::kFixedStepSeconds).accepted,"native tick");}
void row(const char*tag,const Snapshot&s){std::cout<<tag<<" tick="<<s.tick_index<<" P="<<s.player_position.x<<','<<s.player_position.y<<','<<s.player_position.z<<" V="<<s.player_linear_velocity.x<<','<<s.player_linear_velocity.y<<','<<s.player_linear_velocity.z<<" support="<<s.support_entity_id<<" grounded="<<s.player_grounded<<" contact="<<s.support_contact_point.x<<','<<s.support_contact_point.y<<','<<s.support_contact_point.z<<" support_V="<<s.support_point_linear_velocity.x<<','<<s.support_point_linear_velocity.y<<','<<s.support_point_linear_velocity.z<<" gravity="<<s.player_gravity_factor<<" traversal="<<int(s.traversal_state)<<" hands="<<s.traversal_hand_constraint_count<<" jump_work="<<s.landing_jump_work_j<<" deaths="<<s.death_count<<'\n';}
int main(){std::cout<<std::setprecision(15);Simulation s;
 need(s.debug_restart_at({-24,358.6,-151.25}),"clear normal-world taper incline staging");step(s,180);row("settled",s.snapshot());
 need(s.snapshot().player_grounded&&s.snapshot().support_entity_id==1932,"actual1932 footing");
 need(s.set_move_input(0,1)&&s.set_facing(0,1),"ordinary downhill stick");step(s,60);row("downhill",s.snapshot());
 const auto before=s.snapshot();need(before.player_grounded&&before.support_entity_id==1932&&before.player_linear_velocity.y-before.support_point_linear_velocity.y<-.5,"supported descending relative velocity");
 need(s.set_move_input(0,0),"release stick for isolated jump tick");need(s.request_jump(),"ordinary Jump");step(s);const auto after=s.snapshot();row("after_jump",after);
 const double m=85,k=1/m,u=before.player_linear_velocity.y-before.support_point_linear_velocity.y;
 const double gravity_dt=double(float(9.81F)*float(Simulation::kFixedStepSeconds));
 const double j=m*(after.player_linear_velocity.y-before.player_linear_velocity.y+gravity_dt);
 const double signed_work=u*j+.5*k*j*j,braking_energy=.5*u*u/k,positive=.5*k*std::pow(std::max(0.,j+u/k),2),receipt=after.landing_jump_work_j-before.landing_jump_work_j;
 std::cout<<"ACCOUNT u="<<u<<" k="<<k<<" inferred_impulse_ns="<<j<<" gravity_dt="<<gravity_dt<<" signed_work_j="<<signed_work<<" braking_energy_j="<<braking_energy<<" positive_work_j="<<positive<<" ledger_delta_j="<<receipt<<" positive_minus_ledger_j="<<positive-receipt<<" reported_vs_signed_j="<<receipt-signed_work<<'\n';
 need(!after.player_grounded&&after.traversal_state==TraversalState::None&&after.player_gravity_factor==1&&after.death_count==0,"real ordinary free flight");
 return 0;}
