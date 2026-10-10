#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace scraperx::sim;
void tick(Simulation&s,int n){while(n-->0)(void)s.advance_frame(Simulation::kFixedStepSeconds);}
void report(Simulation&s,const char*name,int t){auto p=s.snapshot(); std::cout<<name<<" tick="<<t<<" p="<<p.player_position.x<<','<<p.player_position.y<<','<<p.player_position.z<<" v="<<p.player_linear_velocity.x<<','<<p.player_linear_velocity.y<<','<<p.player_linear_velocity.z<<" ground="<<p.player_grounded<<" support="<<p.support_entity_id<<" balance="<<p.player_balancing<<" slip="<<p.landing_slip_velocity.x<<','<<p.landing_slip_velocity.y<<','<<p.landing_slip_velocity.z<<" contact="<<p.support_contact_point.x<<','<<p.support_contact_point.y<<','<<p.support_contact_point.z<<" deaths="<<p.death_count<<'\n';}
void walk(Simulation&s,double x,double z){for(int t=0;t<1800;++t){auto p=s.snapshot();double dx=x-p.player_position.x,dz=z-p.player_position.z;double ix=1.8*dx-.28*p.player_linear_velocity.x,iz=1.8*dz-.28*p.player_linear_velocity.z,scale=std::max(1.,std::hypot(ix,iz));(void)s.set_move_input(ix/scale,iz/scale);tick(s,1);if(std::hypot(dx,dz)<.005&&std::hypot(p.player_linear_velocity.x,p.player_linear_velocity.z)<.01)break;}(void)s.set_move_input(0,0);tick(s,90);}
void run(const char*name,double x,double z,bool normal){Simulation s(InitialSpawn::BracedBayEntry,WorldContent::PipeBridge);tick(s,45);if(normal){walk(s,28,-131);walk(s,x,z);}else{double y=z<-135?80.17:87.91;(void)s.debug_restart_at({x,y,z});tick(s,90);}report(s,name,0);for(int t=1;t<=2700;++t){(void)s.set_move_input(0,0);tick(s,1);if(t%900==0)report(s,name,t);}}
int main(){std::cout.precision(10);run("slope_edge",28.215,-137,true);run("level_centre",30,-132,false);run("level_edge",30,-131.785,false);}
