#include "sim/grapple_rope_constraint.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace JPH;
using scraperx::sim::GrappleRope;
using scraperx::sim::GrappleRopeSettings;
constexpr float dt=1.0F/360.0F;
void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "rope-test"; }
};
// Endpoints deliberately cannot contact: measurements isolate the actual row.
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return false; }
};
struct Fixture {
    BP bp; BV bv; LP lp;
    PhysicsSystem world;
    TempAllocatorImpl allocator{8*1024*1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID player, support;
    Ref<GrappleRope> rope;
    explicit Fixture(EMotionType type=EMotionType::Dynamic, float rest=2.0F,
                     Vec3 eye=Vec3(0,.4F,0), bool drive=false) {
        world.Init(16,0,16,16,bp,bv,lp);
        world.SetGravity(Vec3::sZero());
        auto &bodies=world.GetBodyInterface();
        BodyCreationSettings p(new BoxShape(Vec3(.2F,.2F,.2F)),RVec3::sZero(),Quat::sIdentity(),EMotionType::Dynamic,1);
        p.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
        p.mMassPropertiesOverride.mMass=85;
        p.mLinearDamping=p.mAngularDamping=0;
        p.mAllowSleeping=false;
        player=bodies.CreateAndAddBody(p,EActivation::Activate);
        // Offset COM ensures a shape-origin/COM frame error cannot pass unnoticed.
        BodyCreationSettings s(new OffsetCenterOfMassShape(new BoxShape(Vec3(.3F,.3F,.3F)),Vec3(.1F,0,0)),
                               RVec3(2.2F,0,0),Quat::sIdentity(),type,1);
        s.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass=20;
        s.mLinearDamping=s.mAngularDamping=0;
        s.mAllowSleeping=false;
        support=bodies.CreateAndAddBody(s,EActivation::Activate);
        BodyLockWrite p_lock(world.GetBodyLockInterface(),player);
        BodyLockWrite s_lock(world.GetBodyLockInterface(),support);
        check(p_lock.Succeeded() && s_lock.Succeeded(),"endpoint bodies exist");
        GrappleRopeSettings settings;
        settings.set_shape_local_eye(s_lock.GetBody(),eye);
        settings.mRestLength=rest;
        rope=static_cast<GrappleRope *>(settings.Create(p_lock.GetBody(),s_lock.GetBody()));
        rope->set_drive_limited(drive);
        world.AddConstraint(rope);
    }
    ~Fixture() { if (rope) world.RemoveConstraint(rope); }
    void tick() {
        check(world.Update(dt,1,&allocator,&jobs)==EPhysicsUpdateError::None,"actual native substep succeeds");
    }
};
void slack_and_break() {
    Fixture slack(EMotionType::Dynamic,3);
    slack.tick();
    check(slack.rope->tension()==0 && !slack.rope->broken(),"slack has no load");
    check(slack.world.GetBodyInterface().GetLinearVelocity(slack.player).LengthSq()==0,"slack cannot push player");
    Fixture broken(EMotionType::Dynamic,1);
    broken.tick();
    check(broken.rope->broken() && broken.rope->tension()==0,"excess extension parts before impulse");
}
void reaction_and_release() {
    Fixture f;
    auto &b=f.world.GetBodyInterface();
    f.tick();
    const float tension=f.rope->tension();
    check(tension>0 && tension<=3000,"finite loaded tension");
    const Vec3 vp=b.GetLinearVelocity(f.player),vs=b.GetLinearVelocity(f.support);
    check((85*vp+20*vs).Length()<1.e-4F,"equal and opposite dynamic momentum");
    check(b.GetAngularVelocity(f.support).GetZ()>0,"offcentre support eye receives torque");
    check(std::abs(85*vp.Length()-tension*dt)<1.e-4F,"readback is solved impulse per actual dt");
    const Vec3 ws=b.GetAngularVelocity(f.support);
    f.world.RemoveConstraint(f.rope);
    f.rope=nullptr;
    check((b.GetLinearVelocity(f.player)-vp).LengthSq()==0 &&
          (b.GetLinearVelocity(f.support)-vs).LengthSq()==0 &&
          (b.GetAngularVelocity(f.support)-ws).LengthSq()==0,"removal preserves instantaneous velocities");
    f.tick();
    check((b.GetLinearVelocity(f.player)-vp).LengthSq()==0,"released player carries momentum");
    std::cout<<"dynamic tension_N="<<tension<<" torque_omega_z="<<ws.GetZ()<<'\n';
}
void static_and_moving_eye() {
    Fixture fixed(EMotionType::Static);
    fixed.tick();
    check(fixed.rope->tension()>0 && fixed.rope->tension()<=3000,"static support loads finite row");
    check(fixed.world.GetBodyInterface().GetLinearVelocity(fixed.player).GetX()>0,"static eye pulls player");
    Fixture moving;
    auto &b=moving.world.GetBodyInterface();
    b.SetPositionAndRotation(moving.support,RVec3(2.1F,.1F,0),Quat::sRotation(Vec3::sAxisZ(),.3F),EActivation::Activate);
    b.SetLinearAndAngularVelocity(moving.support,Vec3(.2F,0,0),Vec3(0,0,.5F));
    {
        BodyLockRead p(moving.world.GetBodyLockInterface(),moving.player);
        BodyLockRead s(moving.world.GetBodyLockInterface(),moving.support);
        const RVec3 eye=s.GetBody().GetWorldTransform()*Vec3(0,.4F,0);
        const Vec3 delta(eye-p.GetBody().GetCenterOfMassPosition());
        const Vec3 axis=delta.Normalized();
        const Vec3 torque=Vec3(eye-s.GetBody().GetCenterOfMassPosition()).Cross(axis);
        const auto *motion=s.GetBody().GetMotionProperties();
        const float inverse_mass=1.0F/85+motion->GetInverseMass()+torque.Dot(motion->MultiplyWorldSpaceInverseInertiaByVector(s.GetBody().GetRotation(),torque));
        const float speed=(s.GetBody().GetPointVelocity(eye)-p.GetBody().GetLinearVelocity()).Dot(axis);
        const float damping=180+dt*6000;
        const float expected=std::max(0.0F,(6000*(delta.Length()-2)+damping*speed)/(1+dt*damping*inverse_mass));
        check(std::abs(moving.rope->prospective_tension(2,dt)-expected)<.001F,"demand follows actual rotated translated eye and point velocity");
    }
    moving.tick();
    check(moving.rope->tension()>0 && !moving.rope->broken(),"moving actual support remains loaded");
}
void caps() {
    Fixture passive(EMotionType::Static,1.8F,Vec3::sZero());
    Fixture driven(EMotionType::Static,1.8F,Vec3::sZero(),true);
    passive.tick(); driven.tick();
    check(passive.rope->tension()>1500 && passive.rope->tension()<=3000,"passive cap remains 3000N");
    check(driven.rope->tension()>1499 && driven.rope->tension()<=1500,"drive cap remains 1500N");
    passive.tick(); driven.tick(); // Exercise retained lambda and bounded warmstart.
    check(passive.rope->tension()<=3000 && driven.rope->tension()<=1500,"warmstart retains finite caps");
}
}
int main() {
    RegisterDefaultAllocator();
    Factory::sInstance=new Factory;
    RegisterTypes();
    int result=0;
    try {
        slack_and_break(); reaction_and_release(); static_and_moving_eye(); caps();
        std::cout<<"PASS grapple_rope_constraint slack reaction torque moving_eye release passive_drive_caps\n";
    } catch (const std::exception &error) { std::cerr<<"FAIL "<<error.what()<<'\n'; result=1; }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance=nullptr;
    return result;
}
