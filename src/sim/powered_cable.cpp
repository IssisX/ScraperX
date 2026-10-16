#include "sim/powered_cable.hpp"
#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/StateRecorder.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace scraperx::sim {
namespace {
bool finite(JPH::Vec3Arg v) { return std::isfinite(v.GetX())&&std::isfinite(v.GetY())&&std::isfinite(v.GetZ()); }
class CableRow final : public JPH::TwoBodyConstraint {
public:
 JPH_OVERRIDE_NEW_DELETE
 CableRow(JPH::Body &a,JPH::Body &b,const PoweredCable::Config &c,PoweredCable::State &s)
 : TwoBodyConstraint(a,b,JPH::DistanceConstraintSettings()),config(c),state(s),
   local1(c.support_eye-a.GetShape()->GetCenterOfMass()),local2(c.load_eye-b.GetShape()->GetCenterOfMass()) {
  // Cold-start reciprocal rows need enough native iterations to transmit a
  // light spreader's load. Derive a convergence floor from pair mass ratio;
  // never substitute hard position correction or unbudgeted warm impulses.
  if(a.IsDynamic()&&b.IsDynamic()) {
   const double ia=a.GetMotionPropertiesUnchecked()->GetInverseMass(),ib=b.GetMotionPropertiesUnchecked()->GetInverseMass();
   const double contraction=std::max(ia,ib)/(ia+ib);
   const double steps=std::ceil(std::log(1e-4)/std::log(contraction));
   SetNumVelocityStepsOverride(static_cast<unsigned>(std::clamp(steps,10.0,255.0)));
  }
  geometry();
 }
 JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User2; }
 void NotifyShapeChanged(const JPH::BodyID &id,JPH::Vec3Arg delta) override { if(id==mBody1->GetID())local1-=delta;if(id==mBody2->GetID())local2-=delta; }
 JPH::Mat44 GetConstraintToBody1Matrix() const override {return JPH::Mat44::sTranslation(local1);}
 JPH::Mat44 GetConstraintToBody2Matrix() const override {return JPH::Mat44::sTranslation(local2);}
 void geometry() {
  state.support_endpoint=mBody1->GetCenterOfMassTransform()*local1;
  state.load_endpoint=mBody2->GetCenterOfMassTransform()*local2;
  const JPH::Vec3 d(state.load_endpoint-state.support_endpoint);
  state.actual_length_m=d.Length();
  axis=state.actual_length_m>1e-6F?d/state.actual_length_m:JPH::Vec3::sAxisY();
 }
 void begin(float h,float effort) {
  geometry(); net_impulse=0; step_work_budget=step_positive_work=0; motor=false; dt=h; part.Deactivate();
  state.last_impulse_ns=state.tensile_force_n=state.electrical_power_w=0;
  state.load_force=JPH::Vec3::sZero();state.last_collision_dt=h;
  state.target_speed_mps=0;state.braking=true;
  if(!std::isfinite(h)||h<=0) return;
  state.energy_cutoff=state.energy_j<=0;
  const float direction=effort>0?-1.F:effort<0?1.F:0.F;
  double travel=std::abs(effort)*config.max_speed_mps*h;
  if(direction<0) {
   travel=std::min(travel,double(state.rest_length_m-config.min_length_m));
   travel=std::min(travel,std::min(config.electrical_power_w*h,state.energy_j)*config.efficiency/config.pull_force_n);
   // Round the target toward the current length: never exceed the admitted
   // source/power reservation through float target representation.
   float next=static_cast<float>(state.rest_length_m-travel);
   while(double(state.rest_length_m)-next>travel) next=std::nextafter(next,state.rest_length_m);
   travel=double(state.rest_length_m)-next;
   if(travel>0) {
    const double mechanical=config.pull_force_n*travel;
    const double source=mechanical/config.efficiency;
    state.energy_j-=source;state.source_energy_used_j+=source;
    state.command_work_bound_j+=mechanical;step_work_budget=mechanical;state.electrical_power_w=source/h;
    state.peak_electrical_w=std::max(state.peak_electrical_w,state.electrical_power_w);
    state.rest_length_m=next;state.target_speed_mps=-float(travel/h);motor=true;state.braking=false;
   }
  } else if(direction>0&&!state.energy_cutoff) {
   travel=std::min(travel,double(config.max_length_m-state.rest_length_m));
   const float old=state.rest_length_m;
   float next=static_cast<float>(old+travel);
   while(double(next)-old>travel)next=std::nextafter(next,old);
   state.rest_length_m=next;state.target_speed_mps=(next-old)/h;
  }
  state.at_min=state.rest_length_m<=config.min_length_m;
  state.at_max=state.rest_length_m>=config.max_length_m;
  state.energy_cutoff=state.energy_j<=0;
  state.heat_j=state.source_energy_used_j-state.positive_motor_work_j+state.absorbed_work_j;
 }
 void SetupVelocityConstraint(float) override {
  part.Deactivate(); geometry();
  if(dt<=0||!GetEnabled()||state.actual_length_m<state.rest_length_m||state.actual_length_m<1e-6F)return;
  r1=JPH::Vec3(state.support_endpoint-mBody1->GetCenterOfMassPosition());
  r2=JPH::Vec3(state.load_endpoint-mBody2->GetCenterOfMassPosition());
  w=0;
  for(auto pair:{std::pair<JPH::Body*,JPH::Vec3>{mBody1,r1},{mBody2,r2}})if(pair.first->IsDynamic()) {
   const auto *mp=pair.first->GetMotionPropertiesUnchecked();const auto cross=pair.second.Cross(axis);
   w+=mp->GetInverseMass()+cross.Dot(mp->MultiplyWorldSpaceInverseInertiaByVector(pair.first->GetRotation(),cross));
  }
  // r1+u has the same cross product as r1 because u is axial.
  part.CalculateConstraintProperties(*mBody1,JPH::Vec3(state.load_endpoint-mBody1->GetCenterOfMassPosition()),*mBody2,r2,axis);
 }
 void ResetWarmStart() override {part.Deactivate();}
 void WarmStartVelocityConstraint(float) override {}
 bool SolveVelocityConstraint(float) override {
  if(!part.IsActive()||!(w>0))return false;
  const auto point_speed=[&](const JPH::Body &b,JPH::Vec3Arg r) {
   return b.IsStatic()?JPH::Vec3::sZero():b.GetLinearVelocity()+b.GetAngularVelocity().Cross(r);
  };
  const double u=axis.Dot(point_speed(*mBody2,r2)-point_speed(*mBody1,r1));
  const double force_cap=(motor?config.pull_force_n:config.brake_force_n)*dt;
  float maximum=float(force_cap);while(double(maximum)>force_cap)maximum=std::nextafter(maximum,0.F);
  const double wanted=std::clamp(double(net_impulse)+(u-state.target_speed_mps)/w,0.0,double(maximum));
  float next=std::clamp(float(wanted),0.F,maximum);
  const double remaining=std::max(0.0,step_work_budget-step_positive_work);
  const auto positive_work=[&](float delta) {
   const double along_force=delta>=0?-u:u;
   const double after=along_force+w*std::abs(double(delta));
   return std::max(0.0,(std::pow(std::max(0.0,after),2)-std::pow(std::max(0.0,along_force),2))/(2*w));
  };
  float delta=next-net_impulse;
  if(positive_work(delta)>remaining) {
   const double along_force=delta>=0?-u:u;
   const double root=std::sqrt(std::max(0.0,along_force)*std::max(0.0,along_force)+2*w*remaining);
   const double allowed=along_force>=0?(remaining>0?2*remaining/(root+along_force):0):(-along_force+root)/w;
   next=std::clamp(float(net_impulse+std::copysign(allowed,double(delta))),0.F,maximum);
   delta=next-net_impulse;
   while(positive_work(delta)>remaining) {next=std::nextafter(next,net_impulse);delta=next-net_impulse;}
  }
  if(delta==0)return false;
  const double positive=positive_work(delta);
  const double kinetic_change=-u*delta+.5*w*delta*delta;
  const double absorbed=std::max(0.0,positive-kinetic_change);
  step_positive_work+=positive;state.positive_motor_work_j+=positive;state.absorbed_work_j+=absorbed;
  state.heat_j=state.source_energy_used_j-state.positive_motor_work_j+state.absorbed_work_j;
  net_impulse=next;state.last_impulse_ns=next;state.tensile_force_n=next/dt;
  state.peak_force_n=std::max(state.peak_force_n,state.tensile_force_n);state.load_force=-state.tensile_force_n*axis;
  // Project the accumulated tensile lambda, not a fresh F*h per iteration.
  // Every signed correction spends its own positive event work from the same
  // finite substep reservation. Dissipation never refunds this budget.
  return part.SolveVelocityConstraint(*mBody1,*mBody2,axis,-next,-next);
 }
 bool SolvePositionConstraint(float,float) override {return false;}
 JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override {return nullptr;}
 // The finite actuator inventory must be captured together with owner State.
 // Native-only snapshots must not silently omit its energy and target.
 void SaveState(JPH::StateRecorder &) const override {throw std::logic_error("PoweredCable: capture owner State with body snapshot");}
 void RestoreState(JPH::StateRecorder &) override {throw std::logic_error("PoweredCable: restore owner State with body snapshot");}
#ifdef JPH_DEBUG_RENDERER
 void DrawConstraint(JPH::DebugRenderer *r) const override {r->DrawLine(state.support_endpoint,state.load_endpoint,JPH::Color::sYellow);}
#endif
 const PoweredCable::Config &config;PoweredCable::State &state;
 JPH::Vec3 local1,local2,axis=JPH::Vec3::sAxisY(),r1,r2;JPH::AxisConstraintPart part;
 double w=0,step_work_budget=0,step_positive_work=0;float dt=0,net_impulse=0;bool motor=false;
};
}
struct PoweredCable::Impl {
 JPH::PhysicsSystem &world; Config config; State state; JPH::Ref<CableRow> row;float effort=0;
 Impl(JPH::PhysicsSystem &w,const Config &c):world(w),config(c) {
  if(c.support==c.load||c.support.IsInvalid()||c.load.IsInvalid()||!finite(c.support_eye)||!finite(c.load_eye)||
   !(c.min_length_m>0&&c.initial_length_m>=c.min_length_m&&c.max_length_m>=c.initial_length_m&&std::isfinite(c.max_length_m))||
   !(c.max_speed_mps>0&&std::isfinite(c.max_speed_mps)&&c.pull_force_n>0&&std::isfinite(c.pull_force_n)&&c.brake_force_n>0&&std::isfinite(c.brake_force_n))||
   !(c.electrical_power_w>0&&std::isfinite(c.electrical_power_w)&&c.efficiency>0&&c.efficiency<=1&&c.capacity_j>=0&&std::isfinite(c.capacity_j)))throw std::invalid_argument("PoweredCable invalid config");
  state.energy_j=c.capacity_j;state.rest_length_m=c.initial_length_m;
  const JPH::BodyID ids[]{c.support,c.load};JPH::BodyLockMultiWrite lock(w.GetBodyLockInterface(),ids,2);
  auto *a=lock.GetBody(0),*b=lock.GetBody(1);
  if(!a||!b||!b->IsDynamic()||!a->IsRigidBody()||!b->IsRigidBody()||a->IsSensor()||b->IsSensor())throw std::invalid_argument("PoweredCable invalid bodies");
  row=new CableRow(*a,*b,config,state);w.AddConstraint(row);
 }
 ~Impl(){world.RemoveConstraint(row);}
};
PoweredCable::PoweredCable(JPH::PhysicsSystem &w,const Config &c):impl_(std::make_unique<Impl>(w,c)){}
PoweredCable::~PoweredCable()=default;
void PoweredCable::pre_step(float e){
 impl_->effort=std::isfinite(e)?std::clamp(e,-1.F,1.F):0.F;
 if(impl_->effort!=0) {impl_->world.GetBodyInterface().ActivateBody(impl_->config.load);impl_->world.GetBodyInterface().ActivateBody(impl_->config.support);}
}
void PoweredCable::collision_step(float dt){impl_->row->begin(dt,impl_->effort);}
void PoweredCable::post_step(){impl_->row->geometry();}
PoweredCable::State PoweredCable::state() const{return impl_->state;}
void PoweredCable::restore(const State &s){
 if(!std::isfinite(s.energy_j)||s.energy_j<0||s.energy_j>impl_->config.capacity_j||!std::isfinite(s.rest_length_m)||s.rest_length_m<impl_->config.min_length_m||s.rest_length_m>impl_->config.max_length_m||
  !std::isfinite(s.source_energy_used_j)||s.source_energy_used_j<0||!std::isfinite(s.positive_motor_work_j)||s.positive_motor_work_j<0||s.positive_motor_work_j>s.command_work_bound_j+1e-6||!std::isfinite(s.absorbed_work_j)||s.absorbed_work_j<0||!std::isfinite(s.command_work_bound_j)||s.command_work_bound_j<0)throw std::invalid_argument("PoweredCable invalid state");
 impl_->state=s;impl_->effort=0;impl_->row->ResetWarmStart();impl_->row->dt=0;impl_->row->net_impulse=0;impl_->row->step_work_budget=impl_->row->step_positive_work=0;impl_->row->motor=false;
 impl_->state.last_impulse_ns=impl_->state.tensile_force_n=0;impl_->state.load_force=JPH::Vec3::sZero();impl_->state.target_speed_mps=0;impl_->state.braking=true;impl_->row->geometry();
}
} // namespace scraperx::sim
#endif
