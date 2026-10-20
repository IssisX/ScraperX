#include "sim/powered_cable.hpp"
#include "sim/finite_cable_drive.hpp"
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
  geometry(); net_impulse=0; dt=h; part.Deactivate();
  state.last_impulse_ns=state.tensile_force_n=0;
  state.load_force=JPH::Vec3::sZero();state.last_collision_dt=h;
  drive.begin(config,state,state.rest_length_m,h,effort);
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
  const float next=drive.project(config,state,net_impulse,u,w);
  if(next==net_impulse)return false;
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
 double w=0;float dt=0,net_impulse=0;finite_cable_drive::Step drive;
};
}
struct PoweredCable::Impl {
 JPH::PhysicsSystem &world; Config config; State state; JPH::Ref<CableRow> row;float effort=0;
 Impl(JPH::PhysicsSystem &w,const Config &c):world(w),config(c) {
  if(c.support==c.load||c.support.IsInvalid()||c.load.IsInvalid()||!finite(c.support_eye)||!finite(c.load_eye)||
   !finite_cable_drive::valid_config(c,c.initial_length_m))throw std::invalid_argument("PoweredCable invalid config");
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
 if(!finite_cable_drive::valid_state(impl_->config,s,s.rest_length_m))throw std::invalid_argument("PoweredCable invalid state");
 impl_->state=s;impl_->effort=0;impl_->row->ResetWarmStart();impl_->row->dt=0;impl_->row->net_impulse=0;impl_->row->drive.reset();
 impl_->state.last_impulse_ns=impl_->state.tensile_force_n=0;impl_->state.load_force=JPH::Vec3::sZero();impl_->state.target_speed_mps=0;impl_->state.braking=true;impl_->row->geometry();
}
} // namespace scraperx::sim
#endif
