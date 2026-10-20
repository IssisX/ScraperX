#include "sim/routed_cable.hpp"
#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/LargeIslandSplitter.h>
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
class CableRow final : public JPH::Constraint {
public:
 JPH_OVERRIDE_NEW_DELETE
 struct Participant {
  JPH::Body *body = nullptr;
  JPH::Vec3 linear = JPH::Vec3::sZero(), angular = JPH::Vec3::sZero();
 };
 CableRow(const RoutedCable::Config &c,RoutedCable::State &s):Constraint(JPH::DistanceConstraintSettings()),config(c),state(s) {normals.fill(JPH::Vec3::sZero());}
 JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User3; }
 void NotifyShapeChanged(const JPH::BodyID &,JPH::Vec3Arg) override {
  throw std::logic_error("RoutedCable: reconstruct after shape changes");
 }
 bool IsActive() const override {
  if(!GetEnabled()||state.invalid_geometry)return false;
  for(unsigned i=0;i<count;++i)if(parts[i].body->IsActive())return true;
  return false;
 }
 void ResetWarmStart() override {lambda=0;}
 void WarmStartVelocityConstraint(float) override {}
 void BuildIslands(JPH::uint32 index,JPH::IslandBuilder &builder,JPH::BodyManager &manager) override {
  std::array<JPH::BodyID,RoutedCable::max_bodies> ids{};unsigned n=0;
  for(unsigned i=0;i<count;++i)if(parts[i].body->IsDynamic())ids[n++]=parts[i].body->GetID();
  if(!n)return;
  manager.ActivateBodies(ids.data(),static_cast<int>(n));
  const auto first=manager.GetBody(ids[0]).GetIndexInActiveBodiesInternal();
  for(unsigned i=1;i<n;++i)builder.LinkBodies(first,manager.GetBody(ids[i]).GetIndexInActiveBodiesInternal());
  builder.LinkConstraint(index,first);
 }
 JPH::uint BuildIslandSplits(JPH::LargeIslandSplitter &splitter) const override {
  // Every touched dynamic body must be serialized, not just the first endpoint.
  for(unsigned i=0;i<count;++i)if(parts[i].body->IsDynamic())splitter.AssignToNonParallelSplit(parts[i].body);
  return JPH::LargeIslandSplitter::cNonParallelSplitIdx;
 }
 void add(JPH::Body *body,JPH::RVec3Arg point,JPH::Vec3Arg gradient) {
  unsigned i=0;while(i<count&&parts[i].body!=body)++i;
  if(i==count){parts[count++]={body,JPH::Vec3::sZero(),JPH::Vec3::sZero()};}
  parts[i].linear+=gradient;
  parts[i].angular+=JPH::Vec3(point-body->GetCenterOfMassPosition()).Cross(gradient);
 }
 void geometry(bool discover) {
  count=0;state.path_length_m=0;state.line_node_count=0;state.contact_count=0;state.invalid_geometry=false;
  const auto append=[&](JPH::RVec3Arg p){state.line_nodes[state.line_node_count++]=p;};
  std::array<JPH::RVec3,RoutedCable::max_nodes> p{};
  for(unsigned i=0;i<config.node_count;++i)p[i]=nodes[i]->GetWorldTransform()*config.nodes[i].point;
  const JPH::RVec3 center=sphere?sphere->GetWorldTransform()*config.sphere.center:JPH::RVec3::sZero();
  append(p[0]);
  for(unsigned i=0;i+1<config.node_count;++i) {
   const JPH::Vec3 d(p[i+1]-p[i]);const float length=d.Length();
   if(length<1e-6F){state.invalid_geometry=true;continue;}
   bool wrap=false;JPH::Vec3 a,b;float ra=0,rb=0,theta=0,alpha=0,beta=0;
   if(sphere&&sphere!=nodes[i]&&sphere!=nodes[i+1]) {
    a=JPH::Vec3(p[i]-center);b=JPH::Vec3(p[i+1]-center);ra=a.Length();rb=b.Length();
    if(ra<=config.sphere.radius_m||rb<=config.sphere.radius_m)state.invalid_geometry=true;
    else {
     a/=ra;b/=rb;theta=std::acos(std::clamp(a.Dot(b),-1.F,1.F));
     alpha=std::acos(config.sphere.radius_m/ra);beta=std::acos(config.sphere.radius_m/rb);
     wrap=discover?theta>alpha+beta:contacts[i];
     // Release on separation; never preserve an old obstacle material pin.
     wrap=wrap&&theta>alpha+beta;
    }
   }
   if(discover)contacts[i]=wrap;
   if(!wrap) {
    const auto g=d/length;state.path_length_m+=length;
    add(nodes[i],p[i],-g);add(nodes[i+1],p[i+1],g);append(p[i+1]);continue;
   }
   ++state.contact_count;
   JPH::Vec3 normal=a.Cross(b);
   if(normal.LengthSq()<1e-10F)normal=normals[i];else normal=normal.Normalized();
   if(normal.LengthSq()<.5F)normal=a.GetNormalizedPerpendicular();
   normals[i]=normal;
   const JPH::Vec3 toward_b=normal.Cross(a),toward_a=b.Cross(normal);
   const auto ta=std::cos(alpha)*a+std::sin(alpha)*toward_b;
   const auto tb=std::cos(beta)*b+std::sin(beta)*toward_a;
   const auto pa=center+config.sphere.radius_m*ta,pb=center+config.sphere.radius_m*tb;
   const auto ga=JPH::Vec3(p[i]-pa).Normalized(),gb=JPH::Vec3(p[i+1]-pb).Normalized();
   const float arc=theta-alpha-beta;
   state.path_length_m+=std::sqrt(ra*ra-config.sphere.radius_m*config.sphere.radius_m)+std::sqrt(rb*rb-config.sphere.radius_m*config.sphere.radius_m)+config.sphere.radius_m*arc;
   add(nodes[i],p[i],ga);add(nodes[i+1],p[i+1],gb);add(sphere,center,-ga-gb);
   append(pa);
   // Samples are presentation only; the exact arc above owns length/forces.
   const auto along=normal.Cross(ta);
   for(unsigned k=1;k<=8;++k){const float angle=arc*k/8;append(center+config.sphere.radius_m*(std::cos(angle)*ta+std::sin(angle)*along));}
   append(p[i+1]);
  }
  state.length_error_m=state.path_length_m-config.paid_length_m;
 }
 void begin(float h) {
  dt=h;lambda=0;state.tension_n=state.impulse_ns=0;state.collision_dt=h;
  geometry(true);state.reaction_count=count;
  for(unsigned i=0;i<count;++i)state.reactions[i]={parts[i].body->GetID(),JPH::Vec3::sZero(),JPH::Vec3::sZero()};
 }
 void SetupVelocityConstraint(float) override {
  // Contact membership was frozen before island setup. Geometry and all repeated
  // body contributions are combined before applying inverse mass or inertia.
  geometry(false);inverse_mass=0;
  if(state.invalid_geometry||dt<=0||state.length_error_m<0)return;
  for(unsigned i=0;i<count;++i)if(parts[i].body->IsDynamic()) {
   const auto &p=parts[i];const auto *mp=p.body->GetMotionPropertiesUnchecked();
   inverse_mass+=mp->GetInverseMass()*p.linear.LengthSq()+p.angular.Dot(mp->MultiplyWorldSpaceInverseInertiaByVector(p.body->GetRotation(),p.angular));
  }
 }
 bool SolveVelocityConstraint(float) override {
  if(!(inverse_mass>0))return false;
  double speed=0;
  for(unsigned i=0;i<count;++i)if(!parts[i].body->IsStatic())speed+=parts[i].linear.Dot(parts[i].body->GetLinearVelocity())+parts[i].angular.Dot(parts[i].body->GetAngularVelocity());
  const float next=static_cast<float>(std::clamp(double(lambda)+speed/inverse_mass,0.0,double(config.rating_n)*dt));
  const float delta=next-lambda;if(delta==0)return false;
  const double work=-speed*delta+.5*inverse_mass*delta*delta;
  state.kinetic_work_j+=work;state.positive_event_work_j+=std::max(0.0,work);state.dissipated_j+=std::max(0.0,-work);
  for(unsigned i=0;i<count;++i) {
   auto &p=parts[i];
   if(p.body->IsDynamic()){p.body->AddImpulse(-delta*p.linear);p.body->AddAngularImpulse(-delta*p.angular);}
   state.reactions[i]={p.body->GetID(),-next/dt*p.linear,-next/dt*p.angular};
  }
  lambda=next;state.impulse_ns=next;state.tension_n=next/dt;state.peak_tension_n=std::max(state.peak_tension_n,state.tension_n);
  return true;
 }
 bool SolvePositionConstraint(float,float) override{return false;}
 JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override{return nullptr;}
 void SaveState(JPH::StateRecorder &) const override{throw std::logic_error("RoutedCable: destroy before body snapshot; reconstruct cold");}
 void RestoreState(JPH::StateRecorder &) override{throw std::logic_error("RoutedCable: reconstruct cold after body restore");}
#ifdef JPH_DEBUG_RENDERER
 void DrawConstraint(JPH::DebugRenderer *r) const override {for(unsigned i=1;i<state.line_node_count;++i)r->DrawLine(state.line_nodes[i-1],state.line_nodes[i],JPH::Color::sYellow);}
#endif
 const RoutedCable::Config &config;RoutedCable::State &state;
 std::array<JPH::Body*,RoutedCable::max_nodes> nodes{};JPH::Body *sphere=nullptr;
 std::array<Participant,RoutedCable::max_bodies> parts{};unsigned count=0;
 std::array<bool,RoutedCable::max_nodes-1> contacts{};
 std::array<JPH::Vec3,RoutedCable::max_nodes-1> normals{};
 float dt=0,lambda=0;double inverse_mass=0;
};
}
struct RoutedCable::Impl {
 JPH::PhysicsSystem &world;Config config;State state;JPH::Ref<CableRow> row;
 Impl(JPH::PhysicsSystem &w,const Config &c):world(w),config(c) {
  if(c.node_count<2||c.node_count>max_nodes||!std::isfinite(c.paid_length_m)||c.paid_length_m<=0||!std::isfinite(c.rating_n)||c.rating_n<=0)throw std::invalid_argument("RoutedCable: invalid node count, paid length or rating");
  std::array<JPH::BodyID,max_bodies> ids{};
  for(unsigned i=0;i<c.node_count;++i){if(c.nodes[i].body.IsInvalid()||!finite(c.nodes[i].point))throw std::invalid_argument("RoutedCable: invalid attachment");ids[i]=c.nodes[i].body;}
  const bool obstacle=!c.sphere.body.IsInvalid();
  if(obstacle&&(!finite(c.sphere.center)||!std::isfinite(c.sphere.radius_m)||c.sphere.radius_m<=0))throw std::invalid_argument("RoutedCable: invalid sphere");
  if(obstacle)ids[c.node_count]=c.sphere.body;
  JPH::BodyLockMultiWrite lock(w.GetBodyLockInterface(),ids.data(),static_cast<int>(c.node_count+obstacle));
  row=new CableRow(config,state);bool dynamic=false;
  for(unsigned i=0;i<c.node_count+obstacle;++i){auto *body=lock.GetBody(static_cast<int>(i));if(!body||!body->IsRigidBody()||body->IsSensor()||body->IsKinematic())throw std::invalid_argument("RoutedCable: missing body; requires static/dynamic rigid nonsensor");dynamic|=body->IsDynamic();if(i<c.node_count)row->nodes[i]=body;else row->sphere=body;}
  if(!dynamic)throw std::invalid_argument("RoutedCable: requires a dynamic body");
  if(obstacle) {
   const auto *shape=row->sphere->GetShape();
   if(shape->GetSubType()==JPH::EShapeSubType::OffsetCenterOfMass)shape=static_cast<const JPH::OffsetCenterOfMassShape*>(shape)->GetInnerShape();
   if(shape->GetSubType()!=JPH::EShapeSubType::Sphere||c.sphere.center.LengthSq()>1e-12F||std::abs(static_cast<const JPH::SphereShape*>(shape)->GetRadius()-c.sphere.radius_m)>1e-6F)throw std::invalid_argument("RoutedCable: obstacle must match actual SphereShape surface");
  }
  state.paid_length_m=c.paid_length_m;row->geometry(true);
  if(state.invalid_geometry)throw std::invalid_argument("RoutedCable: degenerate leg or attachment inside obstacle");
  w.AddConstraint(row);
 }
 ~Impl(){world.RemoveConstraint(row);}
};
RoutedCable::RoutedCable(JPH::PhysicsSystem &w,const Config &c):impl_(std::make_unique<Impl>(w,c)){}
RoutedCable::~RoutedCable()=default;
void RoutedCable::pre_step(){}
void RoutedCable::collision_step(float dt){if(!std::isfinite(dt)||dt<=0)throw std::invalid_argument("RoutedCable: invalid collision step");impl_->row->begin(dt);}
void RoutedCable::post_step(){impl_->row->geometry(false);}
RoutedCable::State RoutedCable::state() const{return impl_->state;}
}
#endif
