#include "sim/rigging_bay.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float dt = 1.F / 360;
void require(bool value, const char *why) {
    if (!value) { std::cerr << "FAIL RIGGING_BAY " << why << '\n'; std::exit(1); }
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "rigging"; }
};
struct BF final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LF final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Fixture {
    BP bp; BF bf; LF lf; PhysicsSystem world;
    TempAllocatorImpl temp{8 * 1024 * 1024}; JobSystemSingleThreaded jobs{1024};
    std::unique_ptr<kit::Kit> parts; std::unique_ptr<RiggingSystem> rig;
    BodyID floor, rider;
    kit::BodyIndex platform, weight, lever;
    std::uint32_t inventory = 0;
    Fixture(bool occupied) {
        world.Init(128, 0, 512, 512, bp, bf, lf);
        BodyCreationSettings ground(new BoxShape(Vec3(20,.5F,20)), kRiggingBayOrigin + Vec3(0,-.5F,0), Quat::sIdentity(), EMotionType::Static, 0);
        floor = world.GetBodyInterface().CreateAndAddBody(ground,EActivation::DontActivate);
        parts = std::make_unique<kit::Kit>(world,0,1);
        rig = std::make_unique<RiggingSystem>(world,*parts);
        build_rigging_bay(*parts,*rig);
        platform=parts->body_for_entity(kRiggingPlatformEntity);
        weight=parts->body_for_entity(kRiggingWeightEntity);
        lever=parts->body_for_entity(kRiggingLeverEntity);
        inventory=parts->body_count();
        if (occupied) {
            BodyCreationSettings person(new BoxShape(Vec3(.25F,.4F,.25F)),
                parts->body_position(platform)+Vec3(0,.50F,0),Quat::sIdentity(),EMotionType::Dynamic,1);
            person.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
            person.mMassPropertiesOverride.mMass=85; person.mFriction=.7F; person.mAllowSleeping=false;
            rider=world.GetBodyInterface().CreateAndAddBody(person,EActivation::Activate);
        }
        tick(180); rig->equip(true);
    }
    ~Fixture() {
        rig.reset(); parts.reset(); auto &bi=world.GetBodyInterface();
        if(!rider.IsInvalid()){bi.RemoveBody(rider);bi.DestroyBody(rider);}
        bi.RemoveBody(floor);bi.DestroyBody(floor);
    }
    void tick(unsigned n) {
        while(n--) {
            parts->pre_step(dt);
            require(world.Update(dt,1,&temp,&jobs)==EPhysicsUpdateError::None,"bay Jolt update");
            parts->post_step(dt); rig->post_step(dt);
        }
    }
    void install() {
        for(unsigned id:{1U,3U,2U}) {
            RVec3 at;
            if(id==1) at=parts->body_position(platform)+Vec3(-1,.48F,.9F);
            else if(id==2) at=parts->body_position(weight)+Vec3(.94F,.7F,.9F);
            else at=kRiggingBayOrigin+Vec3(3.44F,4.3F,.9F);
            rig->aim(at+Vec3(0,0,1),-Vec3::sAxisZ(),rider,true);
            const auto target=rig->readback().target;
            require(target && unsigned(target&0xffffffffU)==id,"bay eye can be aimed from staged hand");
            require(rig->action(target),"bay authored cable installs using normal action");
        }
    }
    void lever_angle(float angle) {
        // Native-owner setup: stage only the actual lever transform, then let
        // its real hinge, gravity and catch run freely without further input.
        auto &bi=world.GetBodyInterface();
        bi.SetPositionAndRotation(parts->body_id(lever),kRiggingBayOrigin+Vec3(-4.5F,1.5F,-.5F),
            Quat::sRotation(Vec3::sAxisZ(),angle),EActivation::Activate);
        bi.SetLinearAndAngularVelocity(parts->body_id(lever),Vec3::sZero(),Vec3::sZero());
    }
    void ballast() {
        // Stage the three authored25kg bodies on the tray, never change mass.
        const Vec3 offsets[3]={{-.26F,.27F,-.26F},{.26F,.27F,-.26F},{0,.27F,.26F}};
        auto &bi=world.GetBodyInterface();
        for(unsigned i=0;i<3;++i) {
            const auto b=parts->body_for_entity(2496+i);
            require(parts->body_mass(b)==25,"ballast is actual authored25kg body");
            bi.SetPositionAndRotation(parts->body_id(b),parts->body_position(weight)+offsets[i],Quat::sIdentity(),EActivation::Activate);
            bi.SetLinearAndAngularVelocity(parts->body_id(b),Vec3::sZero(),Vec3::sZero());
        }
        tick(180);
    }
    float height() const { return float((parts->body_position(platform)-kRiggingBayOrigin).GetY()); }
    void contact_rider() {
        require(!rider.IsInvalid() && world.WereBodiesInContact(rider,parts->body_id(platform)),"actual85kg rider contacts85kg platform");
    }
};
void unloaded_lift() {
    Fixture f(false); f.install(); f.lever_angle(.8F); f.tick(2880);
    std::cout<<"BAY_UNLOADED platform_y="<<f.height()<<" lever_angle="<<f.parts->lever_angle(kit::LeverIndex{0})<<'\n';
    require(f.height()>2.25F && f.height()<2.40F,"unoccupied85kg platform rises to receiver on120kg weight");
    require(!f.parts->catch_latched(kit::CatchIndex{0}),"released over-center lever stays released");
}
void occupied_retry_and_ballast() {
    Fixture f(true); f.contact_rider(); f.install(); f.lever_angle(.8F); f.tick(1440);
    std::cout<<"BAY_OCCUPIED_BASE platform_y="<<f.height()<<" weight_y="<<(f.parts->body_position(f.weight)-kRiggingBayOrigin).GetY()<<'\n';
    require(f.height()<.35F,"120kg base weight cannot lift85kg platform plus85kg rider");
    f.contact_rider();
    require(!f.parts->catch_latched(kit::CatchIndex{0}),"released lever does not auto-relatch during low retry");
    f.lever_angle(0); f.tick(1);
    require(f.parts->catch_latched(kit::CatchIndex{0}),"returned real lever reseats catch for safe low retry");
    f.ballast();
    for(unsigned i=0;i<3;++i) require(f.world.WereBodiesInContact(f.parts->body_id(f.parts->body_for_entity(2496+i)),f.parts->body_id(f.weight)),"all three ballast bodies contact tray");
    f.lever_angle(.8F); f.tick(2880);
    const auto reached=f.height();
    std::cout<<"BAY_BALLAST platform_y="<<reached<<" rider_y="<<(f.world.GetBodyInterface().GetPosition(f.rider)-kRiggingBayOrigin).GetY()<<" tension_n="<<f.rig->readback().tension_n<<'\n';
    require(reached>2.25F && reached<2.40F,"three actual25kg ballast blocks raise occupied platform to receiver");
    f.contact_rider(); f.tick(720);
    require(std::abs(f.height()-reached)<.03F && f.parts->body_velocity(f.platform).Length()<.05F,"actual upper travel limits remain stable");
    require(f.parts->body_count()==f.inventory && f.parts->rope_count()==0,"operation adds no Kit bodies or ropes");
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance=new Factory; RegisterTypes();
    unloaded_lift(); occupied_retry_and_ballast();
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance=nullptr;
    std::cout<<"PASS RIGGING_BAY staged_native_owner=1 public_player_route=unverified\n";
}
