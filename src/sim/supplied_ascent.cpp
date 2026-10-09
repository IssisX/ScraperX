#include "sim/supplied_ascent.hpp"
#include <Jolt/Physics/Body/MotionProperties.h>
#include <algorithm>
#include <cmath>
#include <limits>
namespace scraperx::sim {
using namespace JPH;
namespace {
constexpr double kCartCapacityJ=200000,kCartPowerW=10000;
constexpr double kCartBrakeN=9000,kCartOverheadW=80,kCartEfficiency=.85;
constexpr double kCartSpeedMps=.6,kCartLatchSpeedRadps=.6;
// Provisional geometric clearance candidates, not a stored latch state.
constexpr double kCartUpperWindowM=1.,kCartLatchClearanceRad=.02;

bool finite_vector(Vec3Arg v){
 return std::isfinite(v.GetX())&&std::isfinite(v.GetY())&&std::isfinite(v.GetZ());
}
double precise_dot(Vec3Arg a,Vec3Arg b){
 return double(a.GetX())*b.GetX()+double(a.GetY())*b.GetY()+double(a.GetZ())*b.GetZ();
}
// With the impulse magnitude absorbed into p and r, these are the exact
// positive/negative work integrals along the positive impulse trajectory.
double impulse_positive_work(double p,double r){
 if(p>=0)return p+.5*r;
 if(p+r<=0)return 0;
 return (p+r)*(p+r)/(2*r);
}
double impulse_negative_work(double p,double r){
 if(p>=0)return 0;
 if(p+r<=0)return -p-.5*r;
 return p*p/(2*r);
}
enum class CartImpulseResult { None,Ready,Invalid };
struct CartImpulse {
 Vec3 impulse=Vec3::sZero();
 double positive_j=0,negative_j=0,charged_work_j=0;
};

CartImpulseResult cart_impulse(Body&body,bool angular,Vec3 axis,
 double requested_velocity,double maximum_impulse,double work_budget,CartImpulse&out){
 out=CartImpulse{};
 if(!body.IsDynamic()||!finite_vector(axis)||
    !std::isfinite(requested_velocity)||requested_velocity<=0||
    !std::isfinite(maximum_impulse)||maximum_impulse<=0||
    !std::isfinite(work_budget)||work_budget<0)return CartImpulseResult::Invalid;
 const auto*motion=body.GetMotionProperties();
 if(!motion)return CartImpulseResult::Invalid;
 const double axis_length_sq=precise_dot(axis,axis);
 if(!std::isfinite(axis_length_sq)||axis_length_sq<=0)return CartImpulseResult::Invalid;
 axis=axis.Normalized();
 if(!finite_vector(axis))return CartImpulseResult::Invalid;
 const Vec3 velocity=angular?body.GetAngularVelocity():body.GetLinearVelocity();
 if(!finite_vector(velocity))return CartImpulseResult::Invalid;
 const float inverse_mass=motion->GetInverseMass();
 if(!std::isfinite(inverse_mass)||inverse_mass<=0)return CartImpulseResult::Invalid;
 const Vec3 unit_response=angular?
  motion->MultiplyWorldSpaceInverseInertiaByVector(body.GetRotation(),axis):
  axis*inverse_mass;
 const double k=precise_dot(axis,unit_response),u=precise_dot(axis,velocity);
 const double maximum_velocity=angular?motion->GetMaxAngularVelocity():motion->GetMaxLinearVelocity();
 if(!finite_vector(unit_response)||!std::isfinite(k)||k<=0||
    !std::isfinite(u)||!std::isfinite(maximum_velocity)||maximum_velocity<=0||
    precise_dot(velocity,velocity)>=maximum_velocity*maximum_velocity)
  return CartImpulseResult::Invalid;
 if(u>=requested_velocity)return CartImpulseResult::None;
 const double positive_u=std::max(0.,u);
 const double root=std::sqrt(positive_u*positive_u+2*k*work_budget);
 if(!std::isfinite(root))return CartImpulseResult::Invalid;
 // Rationalize the nonnegative-u case to avoid cancellation at a small budget.
 const double work_cap=u>=0?
  (work_budget>0?2*work_budget/(root+u):0):
  (root-u)/k;
 const double cap=std::min({maximum_impulse,(requested_velocity-u)/k,work_cap});
 if(!std::isfinite(cap)||cap<0)return CartImpulseResult::Invalid;
 if(cap==0)return CartImpulseResult::None;
 float j=static_cast<float>(std::min(cap,double(std::numeric_limits<float>::max())));
 if(double(j)>cap)j=std::nextafter(j,0.F);
 // Validate the actual float impulse and the exact native float additions.
 // Shrinking is only for rounding of the work/speed cap. A native velocity
 // clamp would invalidate the receipt and therefore rejects this attempt.
 for(unsigned retry=0;retry<64&&j>0;++retry){
  const Vec3 impulse=axis*j;
  const Vec3 response=angular?
   motion->MultiplyWorldSpaceInverseInertiaByVector(body.GetRotation(),impulse):
   impulse*inverse_mass;
  const Vec3 predicted=velocity+response;
  if(!finite_vector(impulse)||!finite_vector(response)||!finite_vector(predicted)||
     precise_dot(predicted,predicted)>=maximum_velocity*maximum_velocity)
   return CartImpulseResult::Invalid;
  const double p=precise_dot(velocity,impulse),r=precise_dot(impulse,response);
  if(!std::isfinite(p)||!std::isfinite(r)||r<=0)return CartImpulseResult::Invalid;
  const double rounded_r=precise_dot(predicted,impulse)-p;
  if(!std::isfinite(rounded_r))return CartImpulseResult::Invalid;
  if(rounded_r<=0)return CartImpulseResult::None;
  const double positive=impulse_positive_work(p,r);
  const double negative=impulse_negative_work(p,r);
  // Charge a conservative float-addition margin as well as trajectory work.
  // It is not credited as additional actuator positive work.
  const double charged=std::max(positive,impulse_positive_work(p,rounded_r));
  if(!std::isfinite(positive)||!std::isfinite(negative)||!std::isfinite(charged))
   return CartImpulseResult::Invalid;
  if(precise_dot(predicted,axis)<=requested_velocity&&charged<=work_budget){
   out.impulse=impulse;out.positive_j=positive;out.negative_j=negative;
   out.charged_work_j=charged;
   return CartImpulseResult::Ready;
  }
  j=std::nextafter(j,0.F);
 }
 return CartImpulseResult::None;
}

const vertical::Port* cart_control_port(const vertical::Machine&m,SuppliedAscent::Station station){
 const char*name=station==SuppliedAscent::Station::Deck?"deck_control":
  station==SuppliedAscent::Station::Lower?"lower_control":
  station==SuppliedAscent::Station::Upper?"upper_control":
  station==SuppliedAscent::Station::Intermediate?"intermediate_control":nullptr;
 if(!name)return nullptr;
 for(const auto&p:m.ports)if(p.name==name)return &p;
 return nullptr;
}
}
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
void SuppliedAscent::append_gravity_cart(PhysicsSystem&w,kit::Kit&k){
 auto geometry=std::make_unique<GravityCartGeometry>(build_gravity_cart(w,k));
 auto m=std::make_unique<Instance>();m->machine=std::move(geometry->machine);
 m->cart=std::move(geometry);m->state.energy_j=std::min(kCartCapacityJ,m->cart->capacity_j);
 // Bind native ownership without enrolling factory drives in the powered loop.
 disable_cart_actuators(*m);
 instances_.push_back(std::move(m));
}
void SuppliedAscent::disable_cart_actuators(Instance&m){
 m.effort=0;m.step_dt=0;m.pending=m.energized=false;
 m.cart_brake_q=0;m.cart_brake_receipt=false;
 m.state.braking=true;m.state.power_w=m.state.force_n=0;
 if(m.cart->slab_axis){
  m.cart->slab_axis->SetMotorState(EMotorState::Off);
  m.cart->slab_axis->SetTargetVelocity(0);
  m.cart->slab_axis->SetMaxFrictionForce(float(kCartBrakeN));
 }
 if(m.cart->upper_latch_hinge){
  m.cart->upper_latch_hinge->SetMotorState(EMotorState::Off);
  m.cart->upper_latch_hinge->SetTargetAngularVelocity(0);
  m.cart->upper_latch_hinge->SetMaxFrictionTorque(0);
 }
}
void SuppliedAscent::account_cart(Instance&m){
 if(!m.pending)return;
 if(m.cart_brake_receipt&&m.step_dt>0&&m.cart->slab_axis){
  const double q=m.cart->slab_axis->GetCurrentPosition();
  const double force=m.cart->slab_axis->GetTotalLambdaMotor()/m.step_dt;
  const double work=force*(q-m.cart_brake_q);
  if(std::isfinite(q)&&std::isfinite(force)&&std::isfinite(work)){
   // Off-motor friction receipt only; never source work or regeneration.
   m.state.cart_passive_brake_heat_j+=std::max(0.,-work);
   m.state.cart_passive_brake_positive_residual_j+=std::max(0.,work);
  }
 }
 m.pending=false;m.cart_brake_receipt=false;
}
void SuppliedAscent::step_cart(Instance&m,float dt){
 auto&cart=*m.cart;
 m.state.power_w=m.state.force_n=0;m.state.braking=true;
 m.energized=false;m.pending=false;m.cart_brake_receipt=false;m.step_dt=0;
 if(!cart.slab_axis||!cart.upper_latch_hinge){
  disable_cart_actuators(m);return;
 }
 cart.slab_axis->SetMotorState(EMotorState::Off);
 cart.slab_axis->SetTargetVelocity(0);
 cart.slab_axis->SetMaxFrictionForce(float(kCartBrakeN));
 cart.upper_latch_hinge->SetMotorState(EMotorState::Off);
 cart.upper_latch_hinge->SetTargetAngularVelocity(0);
 // The physical arm returns under gravity, including when away from the top.
 cart.upper_latch_hinge->SetMaxFrictionTorque(0);
 if(!std::isfinite(dt)||dt<=0)return;
 const double q=cart.slab_axis->GetCurrentPosition();
 if(!std::isfinite(q))return;
 m.cart_brake_q=q;m.step_dt=dt;m.pending=true;m.cart_brake_receipt=true;
 if(!std::isfinite(m.state.energy_j)||m.state.energy_j<=0){
  m.state.energy_j=0;m.state.energy_cutoff=true;return;
 }
 m.state.energy_j=std::min(m.state.energy_j,kCartCapacityJ);
 if(m.state.energy_cutoff||m.effort==0||!cart_control_port(*m.machine,m.station))return;
 if(!cart.slab.valid()||!cart.upper_latch.valid()||!cart.deck.valid())return;
 auto&slab=m.machine->native(cart.slab);
 auto&latch=m.machine->native(cart.upper_latch);
 auto&deck=m.machine->native(cart.deck);
 const auto*slab_world=cart.slab_axis->GetBody1();
 const auto*latch_world=cart.upper_latch_hinge->GetBody1();
 const auto*slab_driven=cart.slab_axis->GetBody2();
 const auto*latch_driven=cart.upper_latch_hinge->GetBody2();
 if(!slab.IsDynamic()||!latch.IsDynamic()||!deck.IsDynamic()||
    !slab_world||!latch_world||!slab_world->IsStatic()||!latch_world->IsStatic()||
    !slab_driven||!latch_driven||
    slab_driven->GetID()!=slab.GetID()||latch_driven->GetID()!=latch.GetID())return;
 const auto*slab_motion=slab.GetMotionProperties();
 if(!slab_motion)return;
 const double k=slab_motion->GetInverseMass();
 const double slab_u=slab.GetLinearVelocity().GetY();
 const double speed=std::min(kCartSpeedMps,cart.reset_speed_mps);
 if(!std::isfinite(k)||k<=0||!finite_vector(slab.GetLinearVelocity())||
    !std::isfinite(speed)||speed<=0)return;
 auto&bodies=m.machine->context.world.GetBodyInterfaceNoLock();
 bodies.ActivateConstraint(cart.slab_axis.GetPtr());
 bodies.ActivateConstraint(cart.upper_latch_hinge.GetPtr());
 bodies.ActivateBody(slab.GetID());
 bodies.ActivateBody(latch.GetID());
 bodies.ActivateBody(deck.GetID());
 if(m.effort>0){
  // Passive finite friction capacity, with a free-gravity lookahead because
  // this callback precedes gravity. Tow/loading/contact reactions remain
  // native solver effects; this is not a prescribed slab velocity.
  const double gravity_y=m.machine->context.world.GetGravity().GetY();
  if(!std::isfinite(gravity_y))return;
  const double free_down=-slab_u+std::max(0.,-gravity_y)*dt;
  const double brake=std::clamp((free_down-speed)/(k*dt),0.,kCartBrakeN);
  cart.slab_axis->SetMaxFrictionForce(float(brake));
  m.cart_brake_receipt=brake>0;m.state.braking=false;
  return;
 }
 // Native slider q=0 is the reset endpoint. No cart descent is manufactured:
 // the tension-only tow can pull the deck up, but cannot push it down.
 const double angle=cart.upper_latch_hinge->GetCurrentAngle();
 // Body origin is 2.75 m below the level deck. Compare actual native
 // walking geometry with the authored upper deck, not unlike references.
 const auto deck_position=m.machine->port_position(m.machine->ports.front());
 const double upper_gap=std::abs(double(deck_position.GetY()-cart.upper_deck.GetY()));
 const double open_angle=cart.retainer_open_angle_rad;
 const double torque=std::min(120.,cart.retainer_release_torque_nm);
 const double force=std::min(kCartBrakeN,cart.reset_force_n);
 const double source_power=std::min(kCartPowerW,cart.power_w);
 if(!std::isfinite(angle)||!std::isfinite(upper_gap)||
    !std::isfinite(open_angle)||open_angle<=kCartLatchClearanceRad||
    !std::isfinite(torque)||torque<=0||!std::isfinite(force)||force<=0||
    !std::isfinite(source_power)||source_power<=kCartOverheadW)return;
 const double overhead=kCartOverheadW*dt;
 if(m.state.energy_j<overhead){m.state.energy_cutoff=true;return;}
 double source_left=std::min(m.state.energy_j,source_power*dt);
 bool overhead_paid=false;
 const double effort=std::abs(double(m.effort));
 auto actuate=[&](Body&body,bool angular,Vec3 axis,double target,double rating){
  if(m.state.energy_cutoff)return CartImpulseResult::None;
  const double startup=overhead_paid?0:overhead;
  if(source_left<startup)return CartImpulseResult::None;
  double work_budget=(source_left-startup)*kCartEfficiency;
  CartImpulse impulse;
  for(unsigned retry=0;retry<8;++retry){
   const auto result=cart_impulse(body,angular,axis,target,rating*dt,work_budget,impulse);
   if(result!=CartImpulseResult::Ready)return result;
   const double debit=impulse.charged_work_j/kCartEfficiency+startup;
   if(std::isfinite(debit)&&debit<=source_left&&debit<=m.state.energy_j){
    // Debit once, before the native write. Both actuators share this bank
    // and this substep's source-power allowance.
    m.state.energy_j-=debit;source_left-=debit;overhead_paid=true;
    m.state.positive_work_j+=impulse.positive_j;
    m.state.heat_j+=impulse.negative_j+debit-impulse.charged_work_j;
    m.state.cart_actuator_rounding_residual_j+=impulse.charged_work_j-impulse.positive_j;
    m.state.power_w+=debit/dt;m.energized=true;
    if(angular)body.AddAngularImpulse(impulse.impulse);
    else{
     body.AddImpulse(impulse.impulse);
     m.state.force_n+=double(impulse.impulse.GetY())/dt;
    }
    if(m.state.energy_j==0)m.state.energy_cutoff=true;
    return CartImpulseResult::Ready;
   }
   // Account for division/addition rounding in the final source debit.
   work_budget=std::nextafter(work_budget,0.);
  }
  return CartImpulseResult::None;
 };
 const bool at_retainer=upper_gap<=kCartUpperWindowM;
 const bool arm_clear=angle>=open_angle-kCartLatchClearanceRad;
 if(at_retainer&&angle<open_angle){
  const Vec3 axis=latch_world->GetRotation()*cart.upper_latch_hinge->GetLocalSpaceHingeAxis1();
  // Gravity and the pivot reaction follow this callback. Keep requesting
  // bounded opening until the actual arm clears; tapering its pre-solve
  // velocity to zero can stall below clearance while spending the bank.
  const double target=kCartLatchSpeedRadps*effort;
  if(actuate(latch,true,axis,target,torque*effort)==CartImpulseResult::Invalid)return;
 }
 // An impulse changes arm velocity, not its current angle. Opening therefore
 // cannot unlock hoisting in the same callback before actual clearance.
 if((at_retainer&&!arm_clear)||m.state.energy_cutoff)return;
 // A slack tow can leave the cart at its retainer while the slab is
 // already reset. Only the hoist stops at q=0; the real arm may still open.
 if(q>=-.001)return;
 const double target=speed*effort;
 if(slab_u>=target)return; // Off friction brakes positive-Y overspeed.
 if(actuate(slab,false,Vec3(0,1,0),target,force*effort)==CartImpulseResult::Ready&&
    !m.state.energy_cutoff){
  // No simultaneous active impulse and slider-friction fiction.
  cart.slab_axis->SetMaxFrictionForce(0);
  m.cart_brake_receipt=false;m.state.braking=false;
 }
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
 if(m.cart)return; // Cart activation and native writes belong to serial OnStep.
 if(m.effort!=0&&m.gravity)m.machine->context.world.GetBodyInterface().ActivateConstraint(m.gravity->bearing.GetPtr());
 if(m.effort!=0)for(auto&d:m.machine->drives)m.machine->context.world.GetBodyInterface().ActivateConstraint(d.slider?static_cast<TwoBodyConstraint*>(d.slider.GetPtr()):static_cast<TwoBodyConstraint*>(d.hinge.GetPtr()));
}
void SuppliedAscent::account(Instance&m){
 if(m.cart){account_cart(m);return;}
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
 for(auto&i:instances_){auto&m=*i;
  if(m.cart){account_cart(m);step_cart(m,dt);continue;}
  account(m);const unsigned count=static_cast<unsigned>(m.motors.size());
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
 for(unsigned i=0;i<count();++i){auto&m=*instances_[i];const auto live=m.state;const double remaining=live.energy_j;const bool cutoff=live.energy_cutoff;
  m.state=c.states[i];m.state.energy_j=std::clamp(m.cart?std::min(remaining,m.state.energy_j):m.state.energy_j,0.,capacity_j(i));m.effort=0;m.pending=m.energized=false;m.state.braking=true;
  if(m.cart){
   // The cart bank and its expenditure ledger persist together. Restoring
   // mechanical pose must not erase work already charged to that bank.
   m.state.energy_cutoff=m.state.energy_cutoff||cutoff;
   m.state.positive_work_j=std::max(live.positive_work_j,m.state.positive_work_j);
   m.state.heat_j=std::max(live.heat_j,m.state.heat_j);
   m.state.energy_overdraft_j=std::max(live.energy_overdraft_j,m.state.energy_overdraft_j);
   m.state.cart_passive_brake_heat_j=std::max(live.cart_passive_brake_heat_j,m.state.cart_passive_brake_heat_j);
   m.state.cart_passive_brake_positive_residual_j=std::max(live.cart_passive_brake_positive_residual_j,m.state.cart_passive_brake_positive_residual_j);
   m.state.cart_actuator_rounding_residual_j=std::max(live.cart_actuator_rounding_residual_j,m.state.cart_actuator_rounding_residual_j);
  }
  if(m.gravity){m.gravity->bearing->SetMotorState(EMotorState::Off);m.gravity->bearing->SetMaxFrictionTorque(250000);}
  auto control=m.machine->capture_control();control.command={0,false};m.machine->restore_control(control);
  if(m.cart){m.station=Station::None;disable_cart_actuators(m);continue;}
  for(unsigned n=0;n<m.motors.size();++n){auto&motor=m.motors[n];auto&d=m.machine->drives[n];motor.target_speed=0;
   if(d.slider){d.slider->SetMaxFrictionForce(m.gravity&&n==0?0:motor.rating);motor.last_q=d.slider->GetCurrentPosition();}
   else{d.hinge->SetMaxFrictionTorque(motor.rating);motor.last_q=d.hinge->GetCurrentAngle();}
  }
 }
}
std::uint64_t SuppliedAscent::station_entity(Station s,unsigned i,unsigned panel)const{
 const auto&instance=*instances_.at(i);
 if(s==Station::None||(s==Station::Intermediate&&!instance.cart))return 0;
 const auto&m=*instance.machine;
 if(instance.cart){const auto*p=cart_control_port(m,s);return p?m.context.kit.body_entity(p->body):0;}
 return m.context.kit.body_entity(m.ports[panel&&s==Station::Upper?3:s==Station::Deck?0:s==Station::Lower?1:2].body);
}
RVec3 SuppliedAscent::station_position(Station s,unsigned i,unsigned panel)const{
 const auto&instance=*instances_.at(i);
 if(s==Station::None||(s==Station::Intermediate&&!instance.cart))return RVec3::sZero();
 const auto&m=*instance.machine;
 if(instance.cart){const auto*p=cart_control_port(m,s);return p?m.port_position(*p):RVec3::sZero();}
 if(panel && s==Station::Upper)return instances_.at(i)->machine->port_position(instances_.at(i)->machine->ports.at(3))+(instances_.at(i)->gravity?Vec3(0,1.1F,0):Vec3::sZero());
 if(instances_.at(i)->gravity){const auto&p=m.ports[s==Station::Deck?0:s==Station::Lower?1:2];
  return m.port_position(p)+m.context.kit.body_rotation(p.body)*Vec3(0,1.1F,s==Station::Deck?.56F:0.F);}
 const auto body=m.ports[s==Station::Deck?0:s==Station::Lower?1:2].body;
 return m.context.kit.body_position(body)+m.context.kit.body_rotation(body)*(s==Station::Deck?Vec3(0,1.15F,-1.2F):Vec3(0,1.1F,0));}
double SuppliedAscent::walking_surface_y(unsigned i)const{const auto&m=*instances_.at(i)->machine;return m.port_position(m.ports[0]).GetY();}
}
