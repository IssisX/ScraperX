#include "sim/service_lift.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace JPH;
using scraperx::sim::ServiceLift;
namespace {
void require(bool ok,const char*why){if(!ok){std::cerr<<"FAIL AS027 physics "<<why<<'\n';std::exit(1);}}
struct BP:BroadPhaseLayerInterface{uint GetNumBroadPhaseLayers()const override{return 2;}BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l)const override{return BroadPhaseLayer(l);}const char*GetBroadPhaseLayerName(BroadPhaseLayer)const override{return "lift";}};
struct BF:ObjectVsBroadPhaseLayerFilter{bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a||b.GetValue();}};
struct LF:ObjectLayerPairFilter{bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a||b;}};
struct Step:PhysicsStepListener{ServiceLift*lift=nullptr;void OnStep(const PhysicsStepListenerContext&c)override{lift->collision_step(c.mDeltaTime);}};
struct Contacts:ContactListener{bool blocked=false;void OnContactAdded(const Body&a,const Body&b,const ContactManifold&,ContactSettings&)override{if((a.GetUserData()==1990&&b.GetUserData()==2973)||(b.GetUserData()==1990&&a.GetUserData()==2973))blocked=true;}};
struct Fixture{
 BP bp;BF bf;LF lf;PhysicsSystem world;TempAllocatorImpl temp{32*1024*1024};JobSystemSingleThreaded jobs{1024};Step listener;Contacts contacts;
 std::unique_ptr<scraperx::sim::kit::Kit> kit;std::unique_ptr<ServiceLift> lift;
 Fixture(){world.Init(64,0,256,2048,bp,bf,lf);world.SetGravity(Vec3(0,-9.81F,0));kit=std::make_unique<scraperx::sim::kit::Kit>(world,0,1);lift=std::make_unique<ServiceLift>(world,*kit);listener.lift=lift.get();world.AddStepListener(&listener);world.SetContactListener(&contacts);world.OptimizeBroadPhase();tick(0,180);}
 ~Fixture(){world.RemoveStepListener(&listener);world.SetContactListener(nullptr);lift.reset();kit.reset();}
 void tick(float effort,int n=1){while(n-->0){lift->pre_step(effort);require(world.Update(1.F/90,4,&temp,&jobs)==EPhysicsUpdateError::None,"native update");lift->post_step();}}
};
void cutoff(double energy){Fixture f;auto state=f.lift->state();state.energy_j=energy;f.lift->restore(state);double cutoff_y=0,drop=0;bool stopped=false;
 for(int i=0;i<8*90;++i){const double before=f.lift->walking_surface_y();f.tick(1);const auto now=f.lift->state();if(now.energy_cutoff&&!stopped){cutoff_y=before;stopped=true;}if(stopped)drop=std::max(drop,cutoff_y-f.lift->walking_surface_y());}
 const auto final=f.lift->state();std::cout<<"AS027_CUTOFF supplied="<<energy<<" remaining="<<final.energy_j<<" drop="<<drop<<" peak_w="<<final.peak_electrical_w<<" overdraft="<<final.energy_overdraft_j<<'\n';
 require(stopped&&final.braking,"finite source latches brake");require(final.energy_j>=0&&final.energy_j<energy&&final.energy_overdraft_j==0,"no energy overdraft or refill");require(drop<.02,"native four-substep cutoff settles within20mm");require(final.peak_electrical_w<=f.lift->design().electrical_rating_w,"finite power rating");
 scraperx::sim::kit::Kit::Checkpoint bodies;f.kit->capture(bodies);const double y=f.lift->walking_surface_y();f.tick(-1,90);f.kit->restore(bodies);f.lift->restore(final);f.tick(0,90);require(std::abs(f.lift->walking_surface_y()-y)<.02&&f.lift->state().energy_j==final.energy_j&&f.lift->state().energy_cutoff,"checkpoint preserves exhausted inventory and hold");
}
void empty(){Fixture f;auto state=f.lift->state();state.energy_j=0;f.lift->restore(state);const double y=f.lift->walking_surface_y();f.tick(1,180);require(f.lift->state().positive_work_j==state.positive_work_j&&f.lift->state().energy_j==0&&std::abs(f.lift->walking_surface_y()-y)<.02,"empty source cannot drive");}
void return_cycle(){
 Fixture f;const auto design=f.lift->design();const double initial=f.lift->walking_surface_y();
 f.tick(1,65*90);require(f.lift->walking_surface_y()>144.2&&f.lift->walking_surface_y()<144.4,"finite source reaches physical upper limit");
 f.tick(-1,65*90);f.tick(0,90);const auto state=f.lift->state();
 require(std::abs(f.lift->walking_surface_y()-initial)<.02&&state.braking,"reverse returns to physical lower landing and holds");
 require(state.energy_j>0&&state.energy_j<design.capacity_j&&state.energy_overdraft_j==0&&state.peak_electrical_w<=design.electrical_rating_w,"complete return preserves finite source bounds");
 std::cout<<"AS027_CYCLE mass="<<design.moving_mass_kg<<" effective_mass="<<design.effective_vertical_mass_kg<<" force="<<design.rated_force_n<<" brake="<<design.brake_force_n<<" rating="<<design.electrical_rating_w<<" capacity="<<design.capacity_j<<" remaining="<<state.energy_j<<" return_error="<<f.lift->walking_surface_y()-initial<<'\n';
}
void obstruction(){Fixture f;scraperx::sim::kit::Part block;block.half=Vec3(.8F,.2F,.8F);f.kit->add_body(1990,{block},RVec3(-31,122.2,-162),Quat::sIdentity(),0,.8F);f.tick(1,8*90);const auto state=f.lift->state();std::cout<<"AS027_OBSTRUCTION y="<<f.lift->walking_surface_y()<<" force="<<state.actuator_force_n<<" peak_w="<<state.peak_electrical_w<<" energy="<<state.energy_j<<" overdraft="<<state.energy_overdraft_j<<'\n';require(f.contacts.blocked&&f.lift->walking_surface_y()<122.05,"actual collision blocks lift");require(std::abs(state.actuator_force_n)<=f.lift->design().rated_force_n+10,"obstruction cannot increase rated force");require(state.energy_overdraft_j==0&&state.peak_electrical_w<=f.lift->design().electrical_rating_w,"obstructed actuator preserves energy/power bounds");f.tick(0,90);require(f.lift->state().braking,"obstruction can be released");}
}
int main(){RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();cutoff(1000);cutoff(250000);empty();obstruction();return_cycle();UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;std::cout<<"PASS AS027 finite source, native cutoff, obstruction and exhausted checkpoint\n";}
