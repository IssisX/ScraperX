#pragma once
#include "sim/supplied/vertical_machine.hpp"
#include "sim/gravity_wheel_geometry.hpp"
#include "sim/gravity_cart_geometry.hpp"
#include <utility>
namespace scraperx::sim {
// Production bindings for completed supplied encounters. Bodies and constraints
// stay in the existing Kit/Jolt world; no secondary elevation authority.
class SuppliedAscent final {
public:
 enum class Station : unsigned char { None, Deck, Lower, Upper, Intermediate };
 struct Selection { int machine=-1; Station station=Station::None; };
 struct State { double energy_j=2000000, positive_work_j=0, heat_j=0, power_w=0, force_n=0, energy_overdraft_j=0; double hopper_mass_kg=0, wheel_angle_rad=0, bearing_heat_j=0, bearing_positive_residual_j=0, suspension_heat_j=0, suspension_positive_residual_j=0; double cart_passive_brake_heat_j=0,cart_passive_brake_positive_residual_j=0,cart_actuator_rounding_residual_j=0; bool braking=true,energy_cutoff=false,wheel_motor_enabled=false; };
 struct Checkpoint { std::vector<State> states; unsigned active=0; };
 static constexpr double kCapacityJ=2000000,kPowerW=100000,kBrakeN=80000,kBrakeTorque=400000;
 SuppliedAscent(JPH::PhysicsSystem&,kit::Kit&);
 void append_gravity_wheel(JPH::PhysicsSystem&,kit::Kit&);
 void append_gravity_cart(JPH::PhysicsSystem&,kit::Kit&);
 std::pair<std::uint64_t,std::uint64_t> reclaim_material_range()const;
 void pre_step(int machine,float effort,Station station=Station::Deck);
 void collision_step(float dt);
 void post_step();
 unsigned count()const{return static_cast<unsigned>(instances_.size());}
 unsigned active()const{return active_;}
 State state(unsigned i)const{return instances_.at(i)->state;}
 double capacity_j(unsigned i)const{return instances_.at(i)->cart?instances_.at(i)->cart->capacity_j:kCapacityJ;}
 Checkpoint capture()const;
 void restore(const Checkpoint&);
 JPH::RVec3 station_position(Station,unsigned i,unsigned panel=0)const;
 unsigned station_count(Station s,unsigned i)const{
  if(s==Station::None)return 0;
  if(s==Station::Intermediate)return instances_.at(i)->cart?1:0;
  return s==Station::Upper && instances_.at(i)->machine->ports.size()>3 && instances_.at(i)->machine->ports[3].name=="upper_recall" ? 2:1;
 }
 std::uint64_t station_entity(Station,unsigned i,unsigned panel=0)const;
 double walking_surface_y(unsigned i)const;
 static const char* name(unsigned i){return i==0?"GRAVITY BALANCE":i==1?"CROWN GONDOLA":i==2?"TRACTION TRAM":i==3?"BARREL HELIX":i==4?"CASCADE MAST":i==5?"PITMAN LIFT":i==7?"SLAB HAUL CART":"REFRACTORY RECLAIM";}
 static bool owns_support(std::uint64_t e){return(e>=1920&&e<=1922)||(e>=2920&&e<=2922)||(e>=1980&&e<=1996)||(e>=2980&&e<=2992)||(e>=1940&&e<=1945)||(e>=2200&&e<=2302)||(e>=1954&&e<=1959)||(e>=2320&&e<=2334);}
 const vertical::Machine& machine(unsigned i=0)const{return *instances_.at(i)->machine;}
private:
 struct Motor { double rating=0,last_q=0,cap=0,speed=0;float target_speed=0,request=0; };
 struct Instance { std::unique_ptr<vertical::Machine> machine;State state;float effort=0,step_dt=0;std::vector<Motor> motors;Station station=Station::None;std::unique_ptr<GravityWheelGeometry> gravity;std::unique_ptr<GravityCartGeometry> cart;double bearing_q=0,suspension_q=0,cart_brake_q=0;bool pending=false,energized=false,cart_brake_receipt=false; };
 std::vector<std::unique_ptr<Instance>> instances_;unsigned active_=0;
 void append(std::unique_ptr<vertical::Machine>);
 static void disable_cart_actuators(Instance&);
 static void step_cart(Instance&,float);
 static void account_cart(Instance&);
 static void account(Instance&);
};
}
