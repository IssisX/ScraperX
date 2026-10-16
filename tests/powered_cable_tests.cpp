#include "sim/powered_cable.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace JPH;
using scraperx::sim::PoweredCable;
namespace {
constexpr float dt = 1.F / 360;
void require(bool v, const char *why) { if (!v) { std::cerr << "FAIL POWERED_CABLE " << why << '\n'; std::exit(1); } }
struct BP : BroadPhaseLayerInterface {
 uint GetNumBroadPhaseLayers() const override { return 1; }
 BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer) const override { return BroadPhaseLayer(0); }
 const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "cable"; }
};
struct BF : ObjectVsBroadPhaseLayerFilter { bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; } };
struct LF : ObjectLayerPairFilter { bool ShouldCollide(ObjectLayer,ObjectLayer) const override { return false; } };
struct Step : PhysicsStepListener {
 PoweredCable *cable = nullptr;
 std::array<PoweredCable*,4> slings{};
 void OnStep(const PhysicsStepListenerContext &c) override { if (cable) cable->collision_step(c.mDeltaTime); for(auto *s:slings)if(s)s->collision_step(c.mDeltaTime); }
};
struct Fixture {
 BP bp; BF bf; LF lf; PhysicsSystem world;
 TempAllocatorImpl temp{8*1024*1024}; JobSystemSingleThreaded jobs{1024}; Step listener;
 BodyID support, load;
 std::unique_ptr<PoweredCable> cable;
 Fixture(float mass=3217, float support_mass=0, Vec3 eye=Vec3::sZero()) {
  world.Init(16,0,32,64,bp,bf,lf); world.SetGravity(Vec3(0,-9.81F,0));
  support = body(RVec3(0,10,0), support_mass);
  load = body(RVec3(0,0,0), mass);
  PoweredCable::Config c; c.support=support; c.load=load; c.support_eye=eye; c.load_eye=eye;
  c.initial_length_m=10; c.min_length_m=1; c.max_length_m=20;
  cable=std::make_unique<PoweredCable>(world,c);
  listener.cable=cable.get(); world.AddStepListener(&listener);
 }
 BodyID body(RVec3 pos,float mass) {
  BodyCreationSettings c(new BoxShape(Vec3(.5F,.5F,.5F)),pos,Quat::sIdentity(),mass>0?EMotionType::Dynamic:EMotionType::Static,0);
  c.mLinearDamping=0; c.mAngularDamping=0; c.mAllowSleeping=false;
  if(mass>0) { c.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia; c.mMassPropertiesOverride.mMass=mass; }
  return world.GetBodyInterface().CreateAndAddBody(c,EActivation::Activate);
 }
 ~Fixture() { world.RemoveStepListener(&listener); cable.reset(); for(auto id:{load,support}) {world.GetBodyInterface().RemoveBody(id);world.GetBodyInterface().DestroyBody(id);} }
 void tick(float effort,int n=1,int substeps=1) { while(n-->0) { cable->pre_step(effort); require(world.Update(dt*substeps,substeps,&temp,&jobs)==EPhysicsUpdateError::None,"native update"); cable->post_step(); const auto s=cable->state(); require(s.tensile_force_n>=0&&s.tensile_force_n<=50000.01F&&s.energy_j>=0&&s.energy_overdraft_j==0&&s.peak_electrical_w<=25000.01,"finite limits"); } }
 float y() { return world.GetBodyInterface().GetCenterOfMassPosition(load).GetY(); }
};
void loaded_cycle() {
 Fixture f; const float y0=f.y();
 f.tick(1,1800); const auto lifted=f.cable->state(); const float height=f.y()-y0;
 std::cout<<"HOIST_LIFT mass_kg=3217 height_m="<<height<<" source_j="<<lifted.source_energy_used_j<<" actual_positive_work_j="<<lifted.positive_motor_work_j<<" mgh_j="<<3217*9.81*height<<" force_n="<<lifted.tensile_force_n<<" peak_force_n="<<lifted.peak_force_n<<'\n';
 require(height>1.5&&height<1.8,"3217kg load physically rises at finite speed");
 const double final_v=f.world.GetBodyInterface().GetLinearVelocity(f.load).GetY();
 const double gravity_discretization_j=1800*.5*3217*9.81*9.81*dt*dt+3217*9.81*dt*final_v;
 const double closure=lifted.positive_motor_work_j-lifted.absorbed_work_j+gravity_discretization_j-3217*9.81*height-.5*3217*final_v*final_v;
 std::cout<<"HOIST_ENERGY gravity_discretization_j="<<gravity_discretization_j<<" closure_residual_j="<<closure<<'\n';
 require(lifted.source_energy_used_j>=3217*9.81*height&&std::abs(closure)<2,"source covers lift and measured event work closes with explicit semi-implicit gravity residual");
 f.tick(0,360); require(std::abs(f.y()-height)<.015,"finite passive brake holds loaded mass");
 const auto held=f.cable->state(); require(held.source_energy_used_j==lifted.source_energy_used_j,"hold uses no source");
 f.tick(-1,900); f.tick(0,360); require(f.y()<height-.7,"payout lowers actual body");
 require(f.cable->state().source_energy_used_j==held.source_energy_used_j,"lowering is passive dissipation");
 std::cout<<"HOIST_HOLD_LOWER held_y="<<height<<" final_y="<<f.y()<<" brake_heat_j="<<f.cable->state().heat_j<<'\n';
}
void slack_empty_overload() {
 Fixture f; auto s=f.cable->state(); s.rest_length_m=15; s.energy_j=0; f.cable->restore(s);
 f.world.SetGravity(Vec3::sZero()); f.tick(1,50);
 require(f.cable->state().last_impulse_ns==0&&f.y()==0&&f.cable->state().source_energy_used_j==0,"slack empty cable has no impulse or powered energy");
 s=f.cable->state(); s.rest_length_m=10; f.cable->restore(s); f.world.SetGravity(Vec3(0,-9.81F,0)); f.tick(1,360);
 require(std::abs(f.y())<.01&&f.cable->state().positive_motor_work_j==0,"empty source only brakes taut load");
 Fixture overloaded(10000); overloaded.tick(1,360);
 std::cout<<"HOIST_OVERLOAD y="<<overloaded.y()<<" force_n="<<overloaded.cable->state().tensile_force_n<<'\n';
 require(overloaded.y()<-.5&&overloaded.cable->state().peak_force_n<=50000.01F,"overload stalls without rescue force");
}
void articulated_spreader() {
 Fixture f; f.cable.reset();
 const BodyID spreader=f.body(RVec3(0,2,0),200);
 PoweredCable::Config c;c.support=f.support;c.load=spreader;c.initial_length_m=8;c.min_length_m=1;c.max_length_m=15;
 f.cable=std::make_unique<PoweredCable>(f.world,c);f.listener.cable=f.cable.get();
 std::array<std::unique_ptr<PoweredCable>,4> slings;
 for(unsigned i=0;i<4;++i) {
  PoweredCable::Config rope;rope.support=spreader;rope.load=f.load;rope.capacity_j=0;
  const float x=(i&1)?.4F:-.4F,z=(i&2)?.2F:-.2F;
  rope.support_eye=Vec3(x,-.5F,z);rope.load_eye=Vec3(x,.5F,z);
  rope.initial_length_m=1;rope.min_length_m=.1F;rope.max_length_m=2;
  slings[i]=std::make_unique<PoweredCable>(f.world,rope);f.listener.slings[i]=slings[i].get();
 }
 f.tick(1,1800);
 const float raised=f.y();const auto active=f.cable->state();
 double sling_force=0;for(auto &rope:slings) {rope->post_step();sling_force+=rope->state().tensile_force_n;require(rope->state().source_energy_used_j==0&&rope->state().positive_motor_work_j==0,"passive slings inject no energy");}
 std::cout<<"HOIST_ARTICULATED truck_kg=3217 spreader_kg=200 raised_m="<<raised<<" source_j="<<active.source_energy_used_j<<" positive_event_j="<<active.positive_motor_work_j<<" hoist_force_n="<<active.tensile_force_n<<" sling_sum_n="<<sling_force<<'\n';
 require(raised>1&&active.source_energy_used_j>3417*9.81*raised,"finite hoist raises actual truck through four tensile slings");
 f.tick(0,720);
 std::cout<<"HOIST_ARTICULATED_HOLD displacement_m="<<f.y()-raised<<" velocity_mps="<<f.world.GetBodyInterface().GetLinearVelocity(f.load).GetY()<<'\n';
 require(std::abs(f.y()-raised)<.03&&f.cable->state().source_energy_used_j==active.source_energy_used_j,"finite articulated neutral hold without source draw");
 f.listener.slings.fill(nullptr);for(auto &rope:slings)rope.reset();f.cable.reset();f.listener.cable=nullptr;
 f.world.GetBodyInterface().RemoveBody(spreader);f.world.GetBodyInterface().DestroyBody(spreader);
}
void substep_clock() {
 Fixture a,b; a.tick(1,360); b.tick(1,90,4);
 require(std::abs(a.y()-b.y())<1e-6&&a.cable->state().source_energy_used_j==b.cable->state().source_energy_used_j&&a.cable->state().positive_motor_work_j==b.cable->state().positive_motor_work_j,"four collision substeps debit and solve once each");
}
void reciprocal_rotating_release_restore() {
 Fixture f(3217,6000,Vec3(.4F,0,.2F)); f.world.SetGravity(Vec3::sZero());
 auto &bi=f.world.GetBodyInterface();
 bi.SetAngularVelocity(f.support,Vec3(0,0,.2F));
 bi.SetLinearVelocity(f.support,Vec3(.2F,.1F,0));
 const Vec3 p0=6000*bi.GetLinearVelocity(f.support)+3217*bi.GetLinearVelocity(f.load);
 const Vec3 w0=bi.GetAngularVelocity(f.load), support_w0=bi.GetAngularVelocity(f.support);
 const double energy0=.5*6000*bi.GetLinearVelocity(f.support).LengthSq()+.5*(6000.F/6)*support_w0.LengthSq();
 f.tick(1);
 const auto s=f.cable->state();
 const Vec3 p1=6000*bi.GetLinearVelocity(f.support)+3217*bi.GetLinearVelocity(f.load);
 require((p1-p0).Length()<.001,"dynamic pair equal opposite reaction");
 require((bi.GetAngularVelocity(f.load)-w0).Length()>.001,"off COM load eye creates torque");
 const Vec3 ls=bi.GetLinearVelocity(f.load), ss=bi.GetLinearVelocity(f.support);
 const Vec3 lw=bi.GetAngularVelocity(f.load), sw=bi.GetAngularVelocity(f.support);
 // Impulse at (.4,0,.2) gives torque from actual tensile load force.
 const Vec3 expected=Vec3(.4F,0,.2F).Cross(s.load_force*dt);
 const Vec3 observed=(3217.F/6)*(lw-w0); // .5m cube half extents => I=m/6
 require((expected-observed).Length()<.003,"actual angular impulse uses eye lever arm");
 const Vec3 support_torque=(6000.F/6)*(sw-support_w0);
 require((expected+support_torque).Length()<.003,"support receives actual opposite eye torque");
 const double energy1=.5*6000*ss.LengthSq()+.5*3217*ls.LengthSq()+.5*(6000.F/6)*sw.LengthSq()+.5*(3217.F/6)*lw.LengthSq();
 require(std::abs(energy1-energy0-s.positive_motor_work_j+s.absorbed_work_j)<.003,"moving rotating pair event work closes actual linear and angular kinetic energy");
 std::cout<<"HOIST_REACTION residual_ns="<<(p1-p0).Length()<<" impulse_ns="<<s.last_impulse_ns<<" torque_error_nms="<<(expected-observed).Length()<<" work_j="<<s.positive_motor_work_j<<'\n';
 f.cable->restore(s); f.tick(0);
 require(f.cable->state().source_energy_used_j==s.source_energy_used_j,"restore retains finite source target without warm impulse");
 f.cable.reset();f.listener.cable=nullptr;
 const auto release_v=bi.GetLinearVelocity(f.load), release_w=bi.GetAngularVelocity(f.load);
 require(f.world.Update(dt,1,&f.temp,&f.jobs)==EPhysicsUpdateError::None,"release update");
 require((bi.GetLinearVelocity(f.load)-release_v).Length()<1e-6F&&(bi.GetAngularVelocity(f.load)-release_w).Length()<1e-6F,"release preserves solved momentum");
 (void)ls;(void)ss;(void)sw;
}
}
int main(){std::cout<<std::fixed<<std::setprecision(9);RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();loaded_cycle();slack_empty_overload();reciprocal_rotating_release_restore();substep_clock();articulated_spreader();UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;std::cout<<"PASS POWERED_CABLE isolated_native=1 crane_world=unverified\n";}
