#include "sim/routed_cable.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace JPH;
using scraperx::sim::RoutedCable;
namespace {
constexpr float h=1.F/360;
void require(bool ok,const char *why){if(!ok){std::cerr<<"FAIL ROUTED_CABLE "<<why<<'\n';std::exit(1);}}
struct BP final:BroadPhaseLayerInterface {
 uint GetNumBroadPhaseLayers()const override{return 1;}
 BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer)const override{return BroadPhaseLayer(0);}
 const char *GetBroadPhaseLayerName(BroadPhaseLayer)const override{return "routed cable";}
};
struct BF final:ObjectVsBroadPhaseLayerFilter{bool ShouldCollide(ObjectLayer,BroadPhaseLayer)const override{return false;}};
struct LF final:ObjectLayerPairFilter{bool ShouldCollide(ObjectLayer,ObjectLayer)const override{return false;}};
struct Listener final:PhysicsStepListener {
 RoutedCable *cable=nullptr;
 void OnStep(const PhysicsStepListenerContext &context)override{if(cable)cable->collision_step(context.mDeltaTime);}
};
struct Fixture {
 BP bp;BF bf;LF lf;PhysicsSystem world;TempAllocatorImpl temp{8*1024*1024};JobSystemSingleThreaded jobs{1024};Listener listener;
 std::vector<BodyID> bodies;std::unique_ptr<RoutedCable> cable;
 Fixture(){world.Init(32,0,64,64,bp,bf,lf);world.SetGravity(Vec3::sZero());world.AddStepListener(&listener);}
 ~Fixture(){world.RemoveStepListener(&listener);cable.reset();for(auto id:bodies){world.GetBodyInterface().RemoveBody(id);world.GetBodyInterface().DestroyBody(id);}}
 BodyID body(RVec3 pos,float mass,Shape *shape=nullptr){
  BodyCreationSettings c(shape?shape:new BoxShape(Vec3(.1F,.1F,.1F)),pos,Quat::sIdentity(),mass>0?EMotionType::Dynamic:EMotionType::Static,0);
  c.mAllowSleeping=false;c.mLinearDamping=c.mAngularDamping=0;
  if(mass>0){c.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;c.mMassPropertiesOverride.mMass=mass;}
  auto id=world.GetBodyInterface().CreateAndAddBody(c,EActivation::Activate);bodies.push_back(id);return id;
 }
 void attach(RoutedCable::Config c){cable=std::make_unique<RoutedCable>(world,c);listener.cable=cable.get();}
 void tick(unsigned frames=1,unsigned substeps=4){for(unsigned i=0;i<frames;++i){cable->pre_step();require(world.Update(h*substeps,static_cast<int>(substeps),&temp,&jobs)==EPhysicsUpdateError::None,"native Update");cable->post_step();auto s=cable->state();require(!s.invalid_geometry&&s.tension_n>=0&&std::isfinite(s.path_length_m),"finite physical state");}}
 double energy(Vec3 gravity=Vec3::sZero()){double e=0;for(auto id:bodies){BodyLockRead lock(world.GetBodyLockInterface(),id);const auto &b=lock.GetBody();if(!b.IsDynamic())continue;const auto *m=b.GetMotionPropertiesUnchecked();const auto v=b.GetLinearVelocity(),w=b.GetAngularVelocity();const auto inertia=b.GetInverseInertia().Inversed3x3();e+=.5*v.LengthSq()/m->GetInverseMass()+.5*w.Dot(inertia*w)-gravity.Dot(Vec3(b.GetCenterOfMassPosition()))/m->GetInverseMass();}return e;}
};
void repeated_guide() {
 Fixture f;auto a=f.body(RVec3(-2,-2,0),0),g=f.body(RVec3::sZero(),10),b=f.body(RVec3(2,-2,0),0);
 RoutedCable::Config c;c.node_count=4;c.nodes[0]={a,Vec3::sZero()};c.nodes[1]={g,Vec3(-.5F,0,0)};c.nodes[2]={g,Vec3(.5F,0,0)};c.nodes[3]={b,Vec3::sZero()};c.paid_length_m=6;
 f.attach(c);f.world.GetBodyInterface().SetLinearVelocity(g,Vec3(0,1,0));const double before=f.energy();f.tick(1,1);
 const auto s=f.cable->state();const auto v=f.world.GetBodyInterface().GetLinearVelocity(g);const auto w=f.world.GetBodyInterface().GetAngularVelocity(g);
 std::cout<<"REPEATED_GUIDE vy="<<v.GetY()<<" spin="<<w.Length()<<" tension_n="<<s.tension_n<<" work_j="<<s.kinetic_work_j<<'\n';
 require(std::abs(v.GetY())<1e-5&&w.Length()<1e-5,"same-body gradients aggregate before inverse mass");
 require(s.reaction_count==3&&std::abs(s.tension_n-2250)<.1F,"one shared impulse and deduplicated reactions");
 require(f.energy()<before&&s.positive_event_work_j<1e-7,"passive row removes energy");
}
void slack_and_loaded_guide() {
 Fixture slack;auto a=slack.body(RVec3(-2,2,0),0),g=slack.body(RVec3::sZero(),10),b=slack.body(RVec3(2,2,0),0);
 RoutedCable::Config c;c.node_count=3;c.nodes[0]={a,Vec3::sZero()};c.nodes[1]={g,Vec3(.2F,0,0)};c.nodes[2]={b,Vec3::sZero()};c.paid_length_m=8;
 slack.attach(c);slack.tick(20);require(slack.cable->state().impulse_ns==0&&slack.energy()==0,"slack never pushes");
 Fixture f;a=f.body(RVec3(-2,2,0),0);g=f.body(RVec3::sZero(),10,new OffsetCenterOfMassShape(new BoxShape(Vec3(.5F,.3F,.2F)),Vec3(.15F,.1F,0)));b=f.body(RVec3(2,2,0),0);
 c.nodes[0].body=a;c.nodes[1].body=g;c.nodes[2].body=b;
 // Eye is offset from actual COM; exact initial path is the one paid length.
 const auto eye=f.world.GetBodyInterface().GetWorldTransform(g)*c.nodes[1].point;
 c.paid_length_m=Vec3(eye-RVec3(-2,2,0)).Length()+Vec3(eye-RVec3(2,2,0)).Length();f.attach(c);f.world.SetGravity(Vec3(0,-9.81F,0));
 const double initial=f.energy(Vec3(0,-9.81F,0));float worst=0,spin=0;double max_gain=0;
 for(unsigned i=0;i<450;++i){f.tick();worst=std::max(worst,f.cable->state().length_error_m);spin=std::max(spin,f.world.GetBodyInterface().GetAngularVelocity(g).Length());max_gain=std::max(max_gain,f.energy(Vec3(0,-9.81F,0))-initial);}
 const auto s=f.cable->state();
 std::cout<<"LOADED_GUIDE duration_s=5 max_length_error_m="<<worst<<" max_spin_rps="<<spin<<" peak_tension_n="<<s.peak_tension_n<<" max_mechanical_gain_j="<<max_gain<<" row_work_j="<<s.kinetic_work_j<<'\n';
 require(worst<.03F&&s.peak_tension_n>50&&spin>.01F,"loaded moving off-COM guide carries real load and torque with bounded drift");
 require(max_gain<.1&&s.kinetic_work_j<=1e-5,"passive loaded guide has no hidden work source");
}
void sphere_strike() {
 Fixture f;auto a=f.body(RVec3(-3,0,0),0),b=f.body(RVec3(3,0,0),50);
 auto sphere=f.body(RVec3(0,-1.1F,0),10,new OffsetCenterOfMassShape(new SphereShape(1),Vec3(.25F,0,0)));
 RoutedCable::Config c;c.node_count=2;c.nodes[0]={a,Vec3::sZero()};c.nodes[1]={b,Vec3::sZero()};c.paid_length_m=6;c.sphere={sphere,Vec3::sZero(),1};f.attach(c);
 f.world.SetGravity(Vec3(9.81F,0,0));f.tick(2);
 require(f.cable->state().contact_count==0&&f.cable->state().tension_n>400,"actual sphere initially clear of an already loaded cable");f.world.GetBodyInterface().SetLinearVelocity(sphere,Vec3(0,2,0));const auto initial=f.energy(Vec3(9.81F,0,0));
 float spin=0,peak=0,worst=0;unsigned contacts=0;double gain=0;
 for(unsigned i=0;i<90;++i){f.tick();const auto s=f.cable->state();contacts+=s.contact_count>0;spin=std::max(spin,f.world.GetBodyInterface().GetAngularVelocity(sphere).Length());peak=std::max(peak,s.tension_n);worst=std::max(worst,s.length_error_m);gain=std::max(gain,f.energy(Vec3(9.81F,0,0))-initial);}
 auto s=f.cable->state();std::cout<<"SPHERE_STRIKE contact_frames="<<contacts<<" peak_tension_n="<<peak<<" max_spin_rps="<<spin<<" max_length_error_m="<<worst<<" energy_gain_j="<<gain<<" row_work_j="<<s.kinetic_work_j<<'\n';
 require(contacts>0&&peak>1&&spin>.01F,"free physical sphere receives reciprocal off-COM contact force and torque");require(worst<.02F&&gain<.001,"sphere wrap has bounded drift and no passive energy injection");
 // A real separating velocity must release tangencies without a retained pin.
 f.world.GetBodyInterface().SetLinearVelocity(sphere,Vec3(0,-2,0));f.world.GetBodyInterface().SetAngularVelocity(sphere,Vec3::sZero());f.tick(90);
 s=f.cable->state();bool sphere_reaction=false;for(unsigned i=0;i<s.reaction_count;++i)if(s.reactions[i].body==sphere&&s.reactions[i].force_n.LengthSq()>1e-8F)sphere_reaction=true;
 std::cout<<"SPHERE_RELEASE contacts="<<s.contact_count<<" line_nodes="<<s.line_node_count<<" remaining_loaded_tension_n="<<s.tension_n<<" sphere_reaction="<<sphere_reaction<<'\n';
 require(s.contact_count==0&&s.line_node_count==2&&!sphere_reaction,"sphere separation releases contact, reaction and visual arc while endpoint stays loaded");
}
}
int main(){RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();repeated_guide();slack_and_loaded_guide();sphere_strike();UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;std::cout<<"PASS ROUTED_CABLE 3\n";}
