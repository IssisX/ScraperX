#include "sim/physical_hand_climb.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace JPH;
using namespace scraperx::sim;
namespace {
constexpr float h=1.0F/360.0F;
void check(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char *message) {
    if(std::abs(a-b)>tolerance) { std::cerr<<a<<" != "<<b<<' ';check(false,message); }
}
struct BP final:BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "material-hand"; }
};
struct BV final:ObjectVsBroadPhaseLayerFilter { bool ShouldCollide(ObjectLayer,BroadPhaseLayer) const override { return false; } };
struct LP final:ObjectLayerPairFilter { bool ShouldCollide(ObjectLayer,ObjectLayer) const override { return false; } };
struct Source final:LineMaterialSource {
    Body *a=nullptr,*b=nullptr;
    Vec3 local_a{.2F,.3F,.25F},local_b{-.1F,.2F,.35F};
    std::uint64_t generation=7;
    bool valid=true;
    bool sample_material(LineMaterialHandle handle,LineMaterialSample &sample) const override {
        if(!valid || handle.generation!=generation || handle.coordinate_m<0 || handle.coordinate_m>2) return false;
        const float t=handle.coordinate_m*.5F;
        const auto pa=a->GetWorldTransform()*local_a,pb=b->GetWorldTransform()*local_b;
        sample.world_point=(1-t)*pa+t*pb;
        sample.point_velocity=(1-t)*a->GetPointVelocity(pa)+t*b->GetPointVelocity(pb);
        sample.endpoints={LineMaterialSample::Endpoint{a,local_a,1-t},LineMaterialSample::Endpoint{b,local_b,t}};
        return true;
    }
};
struct World final:PhysicsStepListener {
    BP bp;BV bv;LP lp;PhysicsSystem system;
    TempAllocatorImpl allocator{8*1024*1024};JobSystemSingleThreaded jobs{1024};
    Source source;Body *player;PhysicalHandClimb *hands=nullptr;
    explicit World(bool repeated=false,bool single_endpoint=false) {
        system.Init(32,0,32,32,bp,bv,lp);system.SetGravity(Vec3::sZero());
        auto settings=system.GetPhysicsSettings();settings.mNumVelocitySteps=40;system.SetPhysicsSettings(settings);
        player=body(RVec3(0,-1,0),85,true);
        source.a=body(RVec3(-1,0,0),12,false);
        source.b=repeated ? source.a : body(RVec3(1,0,0),18,false);
        if(single_endpoint) source.local_a=source.local_b=.75F*source.local_a+.25F*source.local_b;
        hands=new PhysicalHandClimb(system,player->GetID());system.AddStepListener(this);
    }
    ~World() { system.RemoveStepListener(this);delete hands; }
    Body *body(RVec3 position,float mass,bool capsule) {
        RefConst<Shape> shape=capsule ? static_cast<Shape *>(new CapsuleShape(.55F,.25F)) : static_cast<Shape *>(new BoxShape(Vec3(.15F,.15F,.15F),.02F));
        if(!capsule) shape=new OffsetCenterOfMassShape(shape,Vec3(.04F,-.03F,.02F));
        BodyCreationSettings settings(shape,position,Quat::sIdentity(),EMotionType::Dynamic,1);
        settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;settings.mMassPropertiesOverride.mMass=mass;
        settings.mLinearDamping=settings.mAngularDamping=0;settings.mAllowSleeping=false;
        if(capsule)settings.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
        const auto id=system.GetBodyInterface().CreateAndAddBody(settings,EActivation::Activate);
        return system.GetBodyLockInterfaceNoLock().TryGetBody(id);
    }
    void OnStep(const PhysicsStepListenerContext &context) override {
        near(context.mDeltaTime,h,1e-9,"uses collision timestep");hands->pre_step(context.mDeltaTime,true);
    }
    void tick() {
        check(system.Update(h,1,&allocator,&jobs)==EPhysicsUpdateError::None,"actual Jolt update");hands->post_step(h);
    }
    void attach(unsigned hand=0,float coordinate=.5F) { check(hands->attach_line_material(hand,source,{source.generation,coordinate}),"material attach"); }
};
void physical_reaction_and_lifecycle() {
    World w;w.attach();
    check(w.hands->attached(0) && w.hands->attached_count()==1 && w.hands->attached_to_line(0,w.source),"attachment identity");
    check(!w.hands->attached_to(0,w.source.a->GetID()),"distributed grip has no single support identity");
    const auto neutral=w.hands->commanded_position();
    near(Vec3(neutral-w.player->GetPosition()).Length(),0,1e-6,"zero-preload acquisition");
    w.tick();near(w.player->GetLinearVelocity().Length(),0,1e-6,"neutral row adds no preload");
    near(w.source.a->GetAngularVelocity().Length(),0,1e-6,"neutral cable stays still");
    const auto initial=w.hands->grip_readback(0);
    check(initial.attached && initial.material && initial.distributed && initial.support.IsInvalid(),"explicit distributed receipt");
    near(initial.coordinate_m,.5F,0,"fixed material coordinate");check(initial.source_generation==7,"source generation receipt");
    const Vec3 demand(1,2,-.5F);
    const auto delta=w.hands->advance_targets(demand,h,500.0F);
    check(delta.Length()>0 && delta.Length()<=.6F*h,"finite posture stroke");
    near(w.hands->last_actuator_positive_work_j(),2500.0*delta.LengthSq(),1e-5,"actual spring work");
    check(w.hands->last_actuator_positive_work_j()<=500*h,"500W command budget");
    LineMaterialSample sample;check(w.source.sample_material({7,.5F},sample),"source sample");
    Vec3 before_p=w.player->GetLinearVelocity(),before_a=w.source.a->GetLinearVelocity(),before_b=w.source.b->GetLinearVelocity();
    Vec3 angular_a=w.source.a->GetAngularVelocity(),angular_b=w.source.b->GetAngularVelocity();
    const Vec3 arm_a(w.source.a->GetWorldTransform()*w.source.local_a-w.source.a->GetCenterOfMassPosition());
    const Vec3 arm_b(w.source.b->GetWorldTransform()*w.source.local_b-w.source.b->GetCenterOfMassPosition());
    const auto rotation_a=w.source.a->GetRotation(),rotation_b=w.source.b->GetRotation();
    w.tick();const Vec3 impulse=w.hands->hand_force(0)*h;
    check(impulse.Length()>0 && w.hands->hand_force(0).Length()<=1500.001F,"actual finite hand load");
    near((85*(w.player->GetLinearVelocity()-before_p)-impulse).Length(),0,2e-5,"player impulse matches solved receipt");
    near((12*(w.source.a->GetLinearVelocity()-before_a)+.75F*impulse).Length(),0,2e-5,"first endpoint receives reciprocal weight");
    near((18*(w.source.b->GetLinearVelocity()-before_b)+.25F*impulse).Length(),0,2e-5,"second endpoint receives reciprocal weight");
    const auto expected_a=w.source.a->GetMotionProperties()->MultiplyWorldSpaceInverseInertiaByVector(rotation_a,arm_a.Cross(-.75F*impulse));
    const auto expected_b=w.source.b->GetMotionProperties()->MultiplyWorldSpaceInverseInertiaByVector(rotation_b,arm_b.Cross(-.25F*impulse));
    near((w.source.a->GetAngularVelocity()-angular_a-expected_a).Length(),0,3e-5,"first endpoint torque uses actual arm");
    near((w.source.b->GetAngularVelocity()-angular_b-expected_b).Length(),0,3e-5,"second endpoint torque uses actual arm");
    check(w.source.a->GetAngularVelocity().Length()>0 && w.source.b->GetAngularVelocity().Length()>0,"real offcenter response");
    const auto planted=w.hands->grip_readback(0);
    near(planted.coordinate_m,.5F,0,"body motion preserves material coordinate");
    check(w.source.sample_material({7,.5F},sample),"moved sample");
    near(Vec3(planted.world_anchor-sample.world_point).Length(),0,1e-6,"readback follows barycentric material");
    near((planted.point_velocity-sample.point_velocity).Length(),0,1e-6,"readback includes actual endpoint angular speed");
    const auto pose_a=w.source.a->GetPosition(),pose_b=w.source.b->GetPosition();
    const auto velocity_p=w.player->GetLinearVelocity(),velocity_a=w.source.a->GetLinearVelocity(),velocity_b=w.source.b->GetLinearVelocity();
    const auto paid=w.hands->actuator_positive_work_j();
    w.attach(0,1.5F);
    near(Vec3(w.source.a->GetPosition()-pose_a).Length()+Vec3(w.source.b->GetPosition()-pose_b).Length(),0,0,"regrip never moves source nodes");
    near((w.player->GetLinearVelocity()-velocity_p).Length()+(w.source.a->GetLinearVelocity()-velocity_a).Length()+(w.source.b->GetLinearVelocity()-velocity_b).Length(),0,0,"regrip preserves momentum");
    near(Vec3(w.hands->commanded_position()-w.player->GetPosition()).Length(),0,1e-6,"regrip starts without new preload");
    near(w.hands->actuator_positive_work_j(),paid,0,"regrip does not refund work");
    w.attach(1,.25F);check(w.hands->compatible_hands(),"same-source pair compatible");
    check(!w.hands->radial_compatible_hands() && !w.hands->set_radial_profile(true),"distributed rows reject radial conversion");
    w.hands->detach(0);check(w.hands->attached_count()==1,"individual detach retains planted hand");
    const auto departure=w.player->GetLinearVelocity(),depart_a=w.source.a->GetLinearVelocity(),depart_b=w.source.b->GetLinearVelocity();
    const auto depart_angular_a=w.source.a->GetAngularVelocity(),depart_angular_b=w.source.b->GetAngularVelocity();
    w.hands->clear();check(!w.hands->active(),"clear removes distributed rows");
    near((w.player->GetLinearVelocity()-departure).Length()+(w.source.a->GetLinearVelocity()-depart_a).Length()+(w.source.b->GetLinearVelocity()-depart_b).Length(),0,0,"release preserves all linear velocities");
    near((w.source.a->GetAngularVelocity()-depart_angular_a).Length()+(w.source.b->GetAngularVelocity()-depart_angular_b).Length(),0,0,"release preserves angular velocities");
    w.tick();near((w.player->GetLinearVelocity()-departure).Length(),0,1e-7,"released player remains free");
    std::cout<<"PASS physical_reaction_and_lifecycle force_n="<<impulse.Length()/h<<" work_j="<<paid<<'\n';
}
void force_and_loaded_work_limits() {
    World w;w.attach();
    const auto stroke=w.hands->advance_targets(Vec3(1,1,1),1.0F,500.0F);
    check(stroke.Length()>0 && stroke.Length()<=.6F,"long command retains stroke cap");
    check(w.hands->last_actuator_positive_work_j()<=500.0 && w.hands->last_actuator_positive_work_j()>499.9,"spring-work budget limits unloaded stroke");
    const auto loaded=w.hands->advance_targets(Vec3(1,1,1),h,500.0F);
    check(loaded.Length()>0 && loaded.Length()<.6F*h,"loaded work budget shortens command");
    check(w.hands->last_actuator_positive_work_j()<=500.0*h && w.hands->last_actuator_positive_work_j()>1.38,"loaded command honors 500W budget");
    w.tick();const auto force=w.hands->hand_force(0);
    check(force.Length()<=1500.001F,"saturated Cartesian vector bound");
    check(std::max({std::abs(force.GetX()),std::abs(force.GetY()),std::abs(force.GetZ())})>865.9F,"actual solver reaches finite axis cap");
    for(unsigned a=0;a<3;++a) check(std::abs(force[a])<=866.0254F,"per-axis force bound");
    std::cout<<"PASS force_and_loaded_work_limits force_n="<<force.Length()<<" loaded_work_j="<<w.hands->last_actuator_positive_work_j()<<'\n';
}
void repeated_bodies_and_stale_source() {
    World repeated(true),single(true,true);repeated.attach();single.attach();
    for(int i=0;i<12;++i) {
        const auto commanded=repeated.hands->advance_targets(Vec3(.2F,.4F,.1F),h,500);
        const auto other=single.hands->advance_targets(Vec3(.2F,.4F,.1F),h,500);
        near((commanded-other).Length(),0,2e-7,"matched commands");
        repeated.tick();single.tick();
        near((repeated.player->GetLinearVelocity()-single.player->GetLinearVelocity()).Length(),0,3e-6,"combine same body before inverse mass");
        near((repeated.source.a->GetAngularVelocity()-single.source.a->GetAngularVelocity()).Length(),0,3e-5,"combine weighted arms before inverse inertia");
    }
    const auto velocity=repeated.player->GetLinearVelocity();
    const auto angular=repeated.source.a->GetAngularVelocity();
    repeated.source.valid=false;repeated.tick();
    near(repeated.hands->hand_force(0).Length(),0,0,"stale material disables solver row");
    near((repeated.player->GetLinearVelocity()-velocity).Length(),0,1e-7,"stale row applies no player impulse");
    near((repeated.source.a->GetAngularVelocity()-angular).Length(),0,1e-7,"stale row applies no endpoint torque");
    check(!repeated.hands->grip_readback(0).attached && repeated.hands->attached_to_line(0,repeated.source),"stale readback with retained lifecycle identity");
    near(repeated.hands->advance_targets(Vec3::sAxisY(),h,500).Length(),0,0,"stale source rejects paid commands");
    repeated.source.valid=true;repeated.source.generation=8;
    check(!repeated.hands->attach_line_material(0,repeated.source,{7,.5F}),"stale generation acquisition rejected");
    check(repeated.hands->attached_count()==1,"failed regrip preserves old topology");
    repeated.tick();near(repeated.hands->hand_force(0).Length(),0,0,"generation mismatch stays disabled");
    std::cout<<"PASS repeated_bodies_and_stale_source\n";
}
}
int main() {
    RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();
    int result=0;
    try { physical_reaction_and_lifecycle();force_and_loaded_work_limits();repeated_bodies_and_stale_source(); }
    catch(const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n';result=1; }
    UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return result;
}
