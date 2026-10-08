#include "sim/supplied_ascent.hpp"
#include <algorithm>
#include <cmath>
namespace scraperx::sim {
using namespace JPH;
SuppliedAscent::SuppliedAscent(PhysicsSystem&w,kit::Kit&k){
 append(vertical::gravity_balance({k,w,{RVec3(-32.8,139.7,-142),0},{1980,3},{2980,2}}));
 append(vertical::crown_gondola({k,w,{RVec3(-44,162.75,-155),JPH_PI},{1983,3},{2982,2}}));
 append(vertical::traction_tram({k,w,{RVec3(-26,187.15,-118.5),0},{1986,5},{2984,3}}));
 append(vertical::barrel_helix({k,w,{RVec3(-41.3,226.45,-142),0},{1991,3},{2987,3}}));
 append(vertical::cascade_mast({k,w,{RVec3(-32.8,249.7,-160.45),0},{1994,3},{2990,3}}));
 append(vertical::pitman_lift({k,w,{RVec3(-42.5,282.7,-142),0},{1920,3},{2920,3}}));
}
void SuppliedAscent::append_gravity_wheel(PhysicsSystem&w,kit::Kit&k){
 auto geometry=std::make_unique<GravityWheelGeometry>(build_gravity_reclaim_wheel(w,k));
 append(std::move(geometry->machine));instances_.back()->gravity=std::move(geometry);
 // The guillotine has no holding friction; gravity assists its finite
 // closing actuator instead of silently holding a released feed open.
 instances_.back()->machine->drives.front().slider->SetMaxFrictionForce(0);
}
std::pair<std::uint64_t,std::uint64_t> SuppliedAscent::reclaim_material_range()const{
 for(const auto&i:instances_)if(i->gravity&&!i->gravity->material.empty()){
  const auto&k=i->machine->context.kit;
  return {k.body_entity(i->gravity->material.front()),k.body_entity(i->gravity->material.back())};
 }
 return {0,0};
}
void SuppliedAscent::append(std::unique_ptr<vertical::Machine> machine){
 auto m=std::make_unique<Instance>();m->machine=std::move(machine);
 for(auto&d:m->machine->drives){
  Motor motor;motor.rating=d.slider?std::max(std::abs(d.slider->GetMotorSettings().mMinForceLimit),std::abs(d.slider->GetMotorSettings().mMaxForceLimit)):d.hinge->GetMotorSettings().mMaxTorqueLimit;
  if(d.slider)d.slider->SetMaxFrictionForce(motor.rating);else d.hinge->SetMaxFrictionTorque(motor.rating);
  m->motors.push_back(motor);
 }
 instances_.push_back(std::move(m));
}
void SuppliedAscent::pre_step(int machine,float effort,Station station){
 for(auto&i:instances_){i->effort=0;i->station=Station::None;}
 if(machine<0||static_cast<unsigned>(machine)>=count())return;
 active_=static_cast<unsigned>(machine);auto&m=*instances_[active_];m.station=station;m.effort=std::isfinite(effort)?std::clamp(effort,-1.F,1.F):0;
 if(m.effort!=0&&m.gravity)m.machine->context.world.GetBodyInterface().ActivateConstraint(m.gravity->bearing.GetPtr());
 if(m.effort!=0)for(auto&d:m.machine->drives)m.machine->context.world.GetBodyInterface().ActivateConstraint(d.slider?static_cast<TwoBodyConstraint*>(d.slider.GetPtr()):static_cast<TwoBodyConstraint*>(d.hinge.GetPtr()));
}
void SuppliedAscent::account(Instance&m){
 if(!m.pending)return;
 double positive=0,negative=0;m.state.force_n=0;m.state.power_w=0;
 for(unsigned n=0;n<m.motors.size();++n){auto&d=m.machine->drives[n];auto&motor=m.motors[n];
  const double q=d.slider?d.slider->GetCurrentPosition():d.hinge->GetCurrentAngle();
  const double force=(d.slider?d.slider->GetTotalLambdaMotor():d.hinge->GetTotalLambdaMotor())/m.step_dt;
  // Wheel angles wrap at +/-pi; work uses the physical substep rotation.
  const double delta=d.slider?q-motor.last_q:std::remainder(q-motor.last_q,2*JPH_PI);
  const double work=force*delta;positive+=std::max(0.,work);negative+=std::max(0.,-work);m.state.force_n+=force;
 }
 if(m.energized){const double debit=positive/.85+80*m.step_dt;
  m.state.positive_work_j+=positive;m.state.heat_j+=negative+debit-positive;
  m.state.energy_overdraft_j=std::max(m.state.energy_overdraft_j,debit-m.state.energy_j);
  m.state.energy_j=std::max(0.,m.state.energy_j-debit);m.state.power_w=debit/m.step_dt;
 }else m.state.heat_j+=negative;
 if(m.gravity){
  m.state.wheel_motor_enabled=m.gravity->bearing->GetMotorState()!=EMotorState::Off;
  const double angle=m.gravity->bearing->GetCurrentAngle();
  const double work=m.gravity->bearing->GetTotalLambdaMotor()/m.step_dt*std::remainder(angle-m.bearing_q,2*JPH_PI);
  // Passive friction receipt only: not a full wheel/contact energy audit.
  m.state.bearing_heat_j+=std::max(0.,-work);m.state.bearing_positive_residual_j+=std::max(0.,work);
  const double suspension_angle=m.gravity->suspension->GetCurrentAngle();
  const double suspension_work=m.gravity->suspension->GetTotalLambdaMotor()/m.step_dt*
   std::remainder(suspension_angle-m.suspension_q,2*JPH_PI);
  m.state.suspension_heat_j+=std::max(0.,-suspension_work);
  m.state.suspension_positive_residual_j+=std::max(0.,suspension_work);
  m.state.wheel_angle_rad=angle;m.state.hopper_mass_kg=0;
  for(auto body:m.gravity->material){const auto p=m.machine->context.kit.body_position(body);
   if(p.GetY()>336.6 && p.GetX()>-53.4 && p.GetX()<-50.6 && p.GetZ()>-167.05 && p.GetZ()<-165.05)
    m.state.hopper_mass_kg+=m.machine->context.kit.body_mass(body);
  }
 }
 m.pending=false;
}
void SuppliedAscent::collision_step(float dt){
 for(auto&i:instances_){auto&m=*i;account(m);const unsigned count=static_cast<unsigned>(m.motors.size());
  // Ordinary supplied drives retain their existing single travel command.
  // Reclaim gate/floor requests differ by the real station being operated.
  for(auto&motor:m.motors)motor.request=m.effort;
  if(m.gravity){
   const bool feed=m.effort>0&&m.station==Station::Lower;
   bool floors_closed=true;
   for(unsigned n=1;n<count;++n){const double q=m.machine->drives[n].hinge->GetCurrentAngle();
    floors_closed=floors_closed&&q<.015;
    m.motors[n].request=m.effort<0&&q<m.machine->drives[n].extent-.015?1.F:feed&&q>.01?-1.F:0.F;
   }
   // Closed doors retain their finite passive holding torque. Feeding cannot
   // lower their capacity by sharing gate power with unnecessary motor holds.
   const double gate_q=m.machine->drives[0].slider->GetCurrentPosition();
   m.motors[0].request=feed&&floors_closed?1.F:gate_q>.01?-1.F:0.F;
   const bool released=(m.effort>0&&m.station==Station::Deck)||m.effort<0;
   // A dissipative overspeed governor adds finite brake torque; never drives.
   const auto axis=m.gravity->bearing->GetBody1()->GetRotation()*m.gravity->bearing->GetLocalSpaceHingeAxis1();
   const double speed=std::abs((m.gravity->bearing->GetBody2()->GetAngularVelocity()-m.gravity->bearing->GetBody1()->GetAngularVelocity()).Dot(axis));
   m.gravity->bearing->SetMotorState(EMotorState::Off);
   m.gravity->bearing->SetMaxFrictionTorque(released?std::clamp((speed-.16)*2500000.,0.,250000.):250000.);
   m.bearing_q=m.gravity->bearing->GetCurrentAngle();m.state.braking=!released;
   // Finite passive suspension brake: its speed-dependent torque capacity
   // models a rotational damper through the existing reciprocal hinge.
   // Jolt opposes relative rotation; no orientation target or world damping.
   const auto suspension=m.gravity->suspension;
   const auto suspension_axis=suspension->GetBody1()->GetRotation()*suspension->GetLocalSpaceHingeAxis1();
   const double suspension_speed=std::abs((suspension->GetBody2()->GetAngularVelocity()-suspension->GetBody1()->GetAngularVelocity()).Dot(suspension_axis));
   suspension->SetMotorState(EMotorState::Off);
   suspension->SetMaxFrictionTorque(std::min(2000.,100.+2500.*suspension_speed));
   m.suspension_q=suspension->GetCurrentAngle();
  }
  const unsigned requested=static_cast<unsigned>(std::count_if(m.motors.begin(),m.motors.end(),[](const Motor&motor){return motor.request!=0;}));
  double reserve=80*dt;
  for(unsigned n=0;n<count;++n){auto&d=m.machine->drives[n];auto&motor=m.motors[n];
   if(d.slider)motor.speed=std::abs(d.slider->GetBody2()->GetLinearVelocity().GetY()-d.slider->GetBody1()->GetLinearVelocity().GetY());
   else{const auto axis=d.hinge->GetBody1()->GetRotation()*d.hinge->GetLocalSpaceHingeAxis1();motor.speed=std::abs((d.hinge->GetBody2()->GetAngularVelocity()-d.hinge->GetBody1()->GetAngularVelocity()).Dot(axis));}
   // Both tram wheels draw from the same finite supply, not a bank each.
   motor.cap=std::min(motor.rating,(kPowerW-80)*.85/std::max(1U,requested)/std::max(motor.speed,double(d.speed)+.02));
   if(motor.request!=0)reserve+=motor.cap*(std::max(motor.speed,double(d.speed))+.02)/.85*dt;
  }
  m.energized=requested!=0&&!m.state.energy_cutoff&&m.state.energy_j>=reserve;
  if(requested!=0&&!m.energized)m.state.energy_cutoff=true;
  for(unsigned n=0;n<count;++n){auto&d=m.machine->drives[n];auto&motor=m.motors[n];
   const double q=d.slider?d.slider->GetCurrentPosition():d.hinge->GetCurrentAngle();
   double progress=q,factor=1;
   if(d.feedback_body.valid()){progress=Vec3(m.machine->context.kit.body_position(d.feedback_body)-m.machine->point(d.feedback_origin)).Dot(m.machine->direction(d.feedback_axis));factor=d.radians_per_metre;}
   else if(d.translation_feedback){progress=d.translation_feedback->GetCurrentPosition();factor=d.radians_per_metre;}
   const bool driving=m.energized&&motor.request!=0;
   const double error=motor.request>0?d.extent-progress:-progress;
   const float wanted=driving?std::clamp(float(2*factor*error),-d.speed*std::abs(motor.request),d.speed*std::abs(motor.request)):0;
   motor.target_speed=driving?std::clamp(wanted,motor.target_speed-d.acceleration*dt,motor.target_speed+d.acceleration*dt):0;
   if(d.slider){
    // The balance motor can only pull down; its ballast supplies ascent work.
    d.slider->GetMotorSettings().SetForceLimits(-motor.cap,i==instances_[0]?0:motor.cap);d.slider->SetMaxFrictionForce(driving|| (m.gravity&&n==0)?0:motor.rating);
    d.slider->SetMotorState(driving?EMotorState::Velocity:EMotorState::Off);d.slider->SetTargetVelocity(motor.target_speed);
   }else{
    d.hinge->GetMotorSettings().SetTorqueLimit(motor.cap);d.hinge->SetMaxFrictionTorque(driving?0:motor.rating);
    d.hinge->SetMotorState(driving?EMotorState::Velocity:EMotorState::Off);d.hinge->SetTargetAngularVelocity(motor.target_speed);
   }
   motor.last_q=q;
  }
  if(!m.gravity)m.state.braking=!m.energized;
  m.step_dt=dt;m.pending=true;
 }
}
void SuppliedAscent::post_step(){for(auto&m:instances_)account(*m);}
SuppliedAscent::Checkpoint SuppliedAscent::capture()const{Checkpoint c;c.active=active_;for(auto&i:instances_)c.states.push_back(i->state);return c;}
void SuppliedAscent::restore(const Checkpoint&c){
 if(c.states.size()!=instances_.size())return;
 active_=std::min(c.active,count()-1);
 for(unsigned i=0;i<count();++i){auto&m=*instances_[i];m.state=c.states[i];m.state.energy_j=std::clamp(m.state.energy_j,0.,kCapacityJ);m.effort=0;m.pending=m.energized=false;m.state.braking=true;
  if(m.gravity){m.gravity->bearing->SetMotorState(EMotorState::Off);m.gravity->bearing->SetMaxFrictionTorque(250000);}
  auto control=m.machine->capture_control();control.command={0,false};m.machine->restore_control(control);
  for(unsigned n=0;n<m.motors.size();++n){auto&motor=m.motors[n];auto&d=m.machine->drives[n];motor.target_speed=0;
   if(d.slider){d.slider->SetMaxFrictionForce(m.gravity&&n==0?0:motor.rating);motor.last_q=d.slider->GetCurrentPosition();}
   else{d.hinge->SetMaxFrictionTorque(motor.rating);motor.last_q=d.hinge->GetCurrentAngle();}
  }
 }
}
std::uint64_t SuppliedAscent::station_entity(Station s,unsigned i,unsigned panel)const{
 const auto&m=*instances_.at(i)->machine;return m.context.kit.body_entity(m.ports[panel&&s==Station::Upper?3:s==Station::Deck?0:s==Station::Lower?1:2].body);
}
RVec3 SuppliedAscent::station_position(Station s,unsigned i,unsigned panel)const{
 if(panel && s==Station::Upper)return instances_.at(i)->machine->port_position(instances_.at(i)->machine->ports.at(3))+(instances_.at(i)->gravity?Vec3(0,1.1F,0):Vec3::sZero());
 const auto&m=*instances_.at(i)->machine;
 if(instances_.at(i)->gravity){const auto&p=m.ports[s==Station::Deck?0:s==Station::Lower?1:2];
  return m.port_position(p)+m.context.kit.body_rotation(p.body)*Vec3(0,1.1F,s==Station::Deck?.56F:0.F);}
 const auto body=m.ports[s==Station::Deck?0:s==Station::Lower?1:2].body;
 return m.context.kit.body_position(body)+m.context.kit.body_rotation(body)*(s==Station::Deck?Vec3(0,1.15F,-1.2F):Vec3(0,1.1F,0));}
double SuppliedAscent::walking_surface_y(unsigned i)const{const auto&m=*instances_.at(i)->machine;return m.port_position(m.ports[0]).GetY();}
}
