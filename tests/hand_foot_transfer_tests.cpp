#include "sim/physical_hand_climb.hpp"
#include "sim/physical_foot_push.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

// Primitive integration evidence only; no Simulation acquisition/controller proof.
namespace {
using namespace JPH;
using scraperx::sim::PhysicalHandClimb;
using scraperx::sim::PhysicalFootPush;
constexpr float h=1.0F/90,dt=h/4;
void require(bool condition,const char *message) {
    if(!condition){std::cerr<<"FAIL hand_foot_transfer: "<<message<<'\n';std::exit(1);}
}
struct BP final:BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override{return 2;}
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override{return BroadPhaseLayer(l);}
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override{return "hand_foot";}
};
struct BV final:ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b) const override{return a!=0||b.GetValue()!=0;}
};
struct LP final:ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a,ObjectLayer b) const override{return a!=0||b!=0;}
};
struct Contact final:ContactListener {
    BodyID player,receiver;
    bool found=false;
    unsigned epoch=0,last_epoch=0;
    RVec3 point=RVec3::sZero();
    Vec3 normal=Vec3::sZero();
    void record(const Body &a,const Body &b,const ContactManifold &m) {
        const bool player_first=a.GetID()==player&&b.GetID()==receiver;
        if(!player_first&&!(b.GetID()==player&&a.GetID()==receiver))return;
        const auto n=player_first?-m.mWorldSpaceNormal:m.mWorldSpaceNormal;
        if(n.GetY()<.9F||m.mRelativeContactPointsOn1.empty())return;
        point=player_first?m.GetWorldSpaceContactPointOn2(0):m.GetWorldSpaceContactPointOn1(0);
        normal=n;found=true;last_epoch=epoch;
    }
    void OnContactAdded(const Body &a,const Body &b,const ContactManifold &m,ContactSettings &) override{record(a,b,m);}
    void OnContactPersisted(const Body &a,const Body &b,const ContactManifold &m,ContactSettings &) override{record(a,b,m);}
};
struct World final {
    BP bp;BV bv;LP lp;PhysicsSystem system;
    TempAllocatorImpl temp{8*1024*1024};JobSystemSingleThreaded jobs{1024};
    Contact contact;std::vector<BodyID> ids;BodyID player,receiver,rail;
    explicit World(bool dynamic,bool gap=false) {
        system.Init(32,0,64,128,bp,bv,lp);system.SetGravity(Vec3(0,-9.81F,0));
        BodyCreationSettings p(new CapsuleShape(.55F,.35F),RVec3(0,.92,0),Quat::sIdentity(),EMotionType::Dynamic,1);
        p.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;p.mMassPropertiesOverride.mMass=85;
        p.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
        p.mLinearVelocity=Vec3(0,-.3F,0);p.mLinearDamping=p.mAngularDamping=0;p.mAllowSleeping=false;
        player=add(p);
        BodyCreationSettings r(new BoxShape(Vec3(2,.15F,2)),RVec3(0,gap?-.45:-.129,0),Quat::sIdentity(),dynamic?EMotionType::Dynamic:EMotionType::Static,dynamic?1:0);
        r.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;r.mMassPropertiesOverride.mMass=250;
        r.mLinearDamping=r.mAngularDamping=0;r.mAllowSleeping=false;receiver=add(r);
        BodyCreationSettings s(new BoxShape(Vec3(.7F,.1F,.1F)),RVec3(0,1.6,.7),Quat::sIdentity(),EMotionType::Static,0);
        rail=add(s);contact.player=player;contact.receiver=receiver;system.SetContactListener(&contact);
    }
    BodyID add(const BodyCreationSettings &s){const auto id=system.GetBodyInterface().CreateAndAddBody(s,EActivation::Activate);require(!id.IsInvalid(),"body allocation");ids.push_back(id);return id;}
    ~World(){system.SetContactListener(nullptr);require(system.GetConstraints().empty(),"constraint cleanup");for(auto id:ids){system.GetBodyInterface().RemoveBody(id);system.GetBodyInterface().DestroyBody(id);}}
    void update(){++contact.epoch;require(system.Update(h,4,&temp,&jobs)==EPhysicsUpdateError::None,"native Update(h,4)");}
    Vec3 velocity(BodyID id){return system.GetBodyInterface().GetLinearVelocity(id);}
};
struct Solve final:PhysicsStepListener {
    World &w;PhysicalHandClimb &hands;PhysicalFootPush &foot;
    bool pending=false,dynamic;unsigned shared=0,consecutive=0,max_consecutive=0;
    Vec3 before_player=Vec3::sZero(),before_receiver=Vec3::sZero();
    double residual=0;float peak_hand=0,peak_foot=0;
    Solve(World &world,PhysicalHandClimb &a,PhysicalFootPush &b,bool moving):w(world),hands(a),foot(b),dynamic(moving){}
    void finish(bool locked) {
        if(!pending)return;
        hands.post_step(dt);
        const auto &access=locked?static_cast<const BodyLockInterface &>(w.system.GetBodyLockInterfaceNoLock()):static_cast<const BodyLockInterface &>(w.system.GetBodyLockInterface());
        const auto *p=access.TryGetBody(w.player),*r=access.TryGetBody(w.receiver);
        require(p&&r,"solve bodies retained");
        Vec3 hand_force=Vec3::sZero();bool loaded=false;
        for(unsigned i=0;i<2;++i){const auto force=hands.hand_force(i);require(force.Length()<=1500.02F,"1500N hand cap");peak_hand=std::max(peak_hand,force.Length());hand_force+=force;loaded|=force.Length()>1;}
        // foot.pre_step samples the preceding solve before advancing its target.
        const auto &f=foot.readback();peak_foot=std::max(peak_foot,f.last_load_n);
        require(f.last_load_n>=0&&f.last_load_n<=3500.02F,"3500N foot cap");
        require(f.commanded_stroke_m<=.300001F&&f.elapsed_seconds<=.250001&&f.command_work_bound_j<=1285.62501,"finite stroke/time/work");
        require(f.last_command_work_bound_j<=5000*dt+.001,"finite foot command power");
        if(loaded&&f.last_load_n>1){++shared;++consecutive;max_consecutive=std::max(max_consecutive,consecutive);}else consecutive=0;
        if(dynamic){
            const Vec3 gravity(0,-9.81F*dt,0);
            const auto error=85*(p->GetLinearVelocity()-before_player-gravity)+250*(r->GetLinearVelocity()-before_receiver-gravity)-hand_force*dt;
            residual=std::max(residual,double(error.Length()));
            require(error.Length()<.015F,"combined reciprocal momentum including actual contacts");
        }
        pending=false;
    }
    void OnStep(const PhysicsStepListenerContext &c) override {
        // Both native owners execute in the same production collision cadence.
        foot.pre_step(c.mDeltaTime);
        finish(true);
        hands.pre_step(c.mDeltaTime,true);
        const auto &access=w.system.GetBodyLockInterfaceNoLock();
        before_player=access.TryGetBody(w.player)->GetLinearVelocity();before_receiver=access.TryGetBody(w.receiver)->GetLinearVelocity();pending=true;
    }
    void after(){hands.post_step(dt);foot.post_step(dt);finish(false);}
};
void scenario(bool dynamic,bool gap=false) {
    World w(dynamic,gap);PhysicalHandClimb hands(w.system,w.player);PhysicalFootPush foot(w.system,w.player);
    require(hands.attach(0,w.rail,RVec3(-.2,1.5,.6))&&hands.attach(1,w.rail,RVec3(.2,1.5,.6)),"actual paired rigid hands");
    Solve solve(w,hands,foot,dynamic);w.system.AddStepListener(&solve);
    w.update();solve.after();
    if(gap){
        require(!w.contact.found&&!foot.active(),"blocked gap generated no actual foot contact or row");
        for(unsigned i=0;i<30;++i){w.update();solve.after();}
        const BodyLockRead lock(w.system.GetBodyLockInterface(),w.player);
        require(hands.attached_count()==2&&!foot.active()&&!w.contact.found&&lock.GetBody().GetMotionProperties()->GetGravityFactor()==1,"gap retains real hang and native gravity");
    }else{
        require(w.contact.found&&w.contact.last_epoch==w.contact.epoch,"fresh receiver contact from native manifold");
        require(foot.begin(w.receiver,w.contact.point,w.contact.normal),"foot begins at actual recorded material point");
        const auto initial_receiver=w.velocity(w.receiver);
        for(unsigned i=0;i<3;++i){w.update();solve.after();}
        require(solve.max_consecutive>=2,"consecutive genuinely shared positive hand and foot loads");
        require(foot.active(),"foot remains valid through overlap");
        if(dynamic)require(w.velocity(w.receiver).GetY()<initial_receiver.GetY()-9.81F*(3*h)-.001F,"dynamic receiver gets native downward reaction beyond gravity");
        const auto player_velocity=w.velocity(w.player),receiver_velocity=w.velocity(w.receiver);
        hands.clear();
        require(foot.active()&&w.velocity(w.player)==player_velocity&&w.velocity(w.receiver)==receiver_velocity,"detaching hands retains foot and exact velocities");
        w.update();solve.after();require(foot.active()&&foot.readback().last_load_n>0,"actual foot solve continues after hand release");
        const auto release_player=w.velocity(w.player),release_receiver=w.velocity(w.receiver);
        foot.clear();
        require(w.velocity(w.player)==release_player&&w.velocity(w.receiver)==release_receiver,"clearing foot grants no velocity");
    }
    w.system.RemoveStepListener(&solve);
    std::cout<<"PASS hand_foot_transfer dynamic="<<dynamic<<" gap="<<gap<<" shared_substeps="<<solve.shared<<" consecutive="<<solve.max_consecutive<<" hand_peak_n="<<solve.peak_hand<<" foot_peak_n="<<solve.peak_foot<<" reciprocal_residual_ns="<<solve.residual<<'\n';
}
}
int main(){RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();scenario(false);scenario(true);scenario(false,true);UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return 0;}
