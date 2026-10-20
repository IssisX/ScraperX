#include "sim/grapple_rope_constraint.hpp"
#include "sim/physical_hand_climb.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float dt=1.0F/360.0F;
void require(bool condition,const char *message) {
    if (!condition) throw std::runtime_error(message);
}
struct Layers final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "portable-grip"; }
};
// Isolate the existing hand/rope solve. This fixture supplies no world-route or
// cable-obstacle proof; those require the installed tool's public-input route.
struct BroadFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer,BroadPhaseLayer) const override { return false; }
};
struct PairFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer,ObjectLayer) const override { return false; }
};
struct Fixture {
    Layers layers;
    BroadFilter broad;
    PairFilter pair;
    PhysicsSystem world;
    TempAllocatorImpl allocator{8*1024*1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID player,grip,anchor;
    Ref<GrappleRope> line;
    std::unique_ptr<PhysicalHandClimb> hands;
    Fixture() {
        world.Init(16,0,16,16,layers,broad,pair);
        auto &bodies=world.GetBodyInterface();
        BodyCreationSettings player_settings(new CapsuleShape(.55F,.35F),RVec3(0,.9F,0),
            Quat::sIdentity(),EMotionType::Dynamic,1);
        player_settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
        player_settings.mMassPropertiesOverride.mMass=85;
        player_settings.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
        player_settings.mAllowSleeping=false;
        player=bodies.CreateAndAddBody(player_settings,EActivation::Activate);
        BodyCreationSettings grip_settings(new BoxShape(Vec3(.14F,.02F,.025F)),RVec3(0,1.95F,0),
            Quat::sIdentity(),EMotionType::Dynamic,1);
        grip_settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
        grip_settings.mMassPropertiesOverride.mMass=.25F;
        grip=bodies.CreateAndAddBody(grip_settings,EActivation::Activate);
        BodyCreationSettings anchor_settings(new BoxShape(Vec3(.2F,.2F,.2F)),RVec3(0,5.15F,0),
            Quat::sIdentity(),EMotionType::Static,0);
        anchor=bodies.CreateAndAddBody(anchor_settings,EActivation::DontActivate);
        {
            BodyLockWrite load(world.GetBodyLockInterface(),grip);
            BodyLockWrite support(world.GetBodyLockInterface(),anchor);
            require(load.Succeeded() && support.Succeeded(),"real line endpoints exist");
            GrappleRopeSettings settings;
            settings.set_shape_local_eye(support.GetBody(),Vec3(0,-.2F,0));
            settings.mRestLength=3;
            line=static_cast<GrappleRope *>(settings.Create(load.GetBody(),support.GetBody()));
        }
        world.AddConstraint(line);
        hands=std::make_unique<PhysicalHandClimb>(world,player);
        grab();
        bodies.SetLinearVelocity(player,Vec3(2,0,0));
        bodies.SetLinearVelocity(grip,Vec3(2,0,0));
    }
    ~Fixture() {
        hands.reset();
        if (line) world.RemoveConstraint(line);
    }
    void grab() {
        const auto transform=world.GetBodyInterface().GetWorldTransform(grip);
        require(hands->attach(0,grip,transform*Vec3(-.10F,0,0)),"left hand grips actual fitting");
        require(hands->attach(1,grip,transform*Vec3(.10F,0,0)),"right hand grips actual fitting");
        hands->set_swing_profile();
    }
    void step() {
        hands->pre_step(dt);
        require(world.Update(dt,1,&allocator,&jobs)==EPhysicsUpdateError::None,"native coupled solve");
        hands->post_step(dt);
    }
};
void physical_grip_and_release() {
    Fixture f;
    auto &bodies=f.world.GetBodyInterface();
    const auto initial=bodies.GetCenterOfMassPosition(f.player);
    float peak_tension=0,peak_hand=0;
    float maximum_x=0;
    unsigned loaded_steps=0;
    for (unsigned i=0;i<720;++i) {
        f.step();
        const float tension=f.line->tension();
        peak_tension=std::max(peak_tension,tension);
        peak_hand=std::max(peak_hand,f.hands->hand_force(0).Length());
        maximum_x=std::max(maximum_x,float(bodies.GetCenterOfMassPosition(f.player).GetX()));
        require(!f.line->broken(),"normal rider swing remains within existing rope strength");
        require(f.hands->attached_count()==2,"two actual grips carry swing");
        require(tension<=GrappleRopeSettings::strength_n,"existing rope force bound");
        require(f.hands->hand_force(0).Length()<=PhysicalHandClimb::force_bound_n()+.01F &&
                f.hands->hand_force(1).Length()<=PhysicalHandClimb::force_bound_n()+.01F,
            "existing per-hand force bounds");
        if (tension>400 && f.hands->hand_force(0).Length()+f.hands->hand_force(1).Length()>400) ++loaded_steps;
        const auto left=f.hands->grip_readback(0),right=f.hands->grip_readback(1);
        require(left.support==f.grip && right.support==f.grip,"readback names real fitting body");
        require((left.world_anchor-right.world_anchor).Length()>.19F,"separate planted hand points");
    }
    require(loaded_steps>360,"actual rider load reaches rope through physical hands");
    require(maximum_x>.5F,"committed input momentum produces real swing displacement");
    require(bodies.GetCenterOfMassPosition(f.player).GetY()>initial.GetY()-.5F,"line and hands support rider against gravity");
    require(f.hands->actuator_positive_work_j()==0,"passive swing supplies no hidden target work");
    const Vec3 departure=bodies.GetLinearVelocity(f.player);
    const auto grip_before=bodies.GetLinearVelocity(f.grip);
    f.hands->clear();
    require((bodies.GetLinearVelocity(f.player)-departure).LengthSq()==0,"grip release preserves exact departure velocity");
    require((bodies.GetLinearVelocity(f.grip)-grip_before).LengthSq()==0,"release does not kick fitting");
    require(f.line->GetEnabled() && !f.line->broken(),"world line survives player release");
    const float before_y=bodies.GetCenterOfMassPosition(f.player).GetY();
    for (unsigned i=0;i<90;++i) f.step();
    require(bodies.GetLinearVelocity(f.player).GetY()<departure.GetY()-2,"released player resumes ordinary gravity");
    require(bodies.GetCenterOfMassPosition(f.player).GetY()<before_y,"no retained hidden grip");
    require(f.line->GetEnabled() && !f.line->broken(),"unloaded installed line remains after departure");
    std::cout<<"PASS PORTABLE_LINE_GRIP loaded_substeps="<<loaded_steps<<" peak_line_n="<<peak_tension
        <<" peak_hand_n="<<peak_hand<<" swing_x_m="<<maximum_x
        <<" passive_target_work_j=0 release_preserves_velocity=1 installed_line_retained=1\n";
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance=new Factory; RegisterTypes();
    int result=0;
    try { physical_grip_and_release(); }
    catch (const std::exception &error) { std::cerr<<"FAIL PORTABLE_LINE_GRIP "<<error.what()<<'\n';result=1; }
    UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;
    return result;
}
