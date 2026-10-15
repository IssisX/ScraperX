#include "sim/grappling_gun.hpp"
#include "grappling_retention_observation.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

// Direct native owner fixtures. No ordinary-input, receiver acquisition, rendered
// route, or complete reel/solver energy-closure claim is made by this executable.
namespace {
using namespace JPH;
using Gun = scraperx::sim::GrapplingGun;
constexpr float dt = 1.0F / 360.0F;
constexpr float player_mass = 85.0F;
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "grapple-test"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Listener final : PhysicsStepListener {
    Gun &gun;
    float actual_dt = 0;
    explicit Listener(Gun &g) : gun(g) {}
    void OnStep(const PhysicsStepListenerContext &context) override {
        actual_dt = context.mDeltaTime;
        gun.pre_step(actual_dt);
    }
};
struct State { RVec3 center; Vec3 velocity, angular; double kinetic; };
struct Fixture final {
    BP bp; BV bv; LP lp;
    PhysicsSystem world;
    TempAllocatorImpl allocator{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID player;
    std::unique_ptr<Gun> gun;
    std::unique_ptr<Listener> listener;
    unsigned collision_steps=1;
    explicit Fixture(Vec3 gravity = Vec3::sZero()) {
        world.Init(64, 0, 128, 128, bp, bv, lp);
        world.SetGravity(gravity);
        BodyCreationSettings settings(new BoxShape(Vec3(.2F,.3F,.2F)), RVec3::sZero(),
                                      Quat::sIdentity(), EMotionType::Dynamic, 1);
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = player_mass;
        settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        settings.mLinearDamping = settings.mAngularDamping = 0;
        settings.mAllowSleeping = false;
        player = world.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        check(!player.IsInvalid(), "85 kg dynamic translation player created");
        gun = std::make_unique<Gun>(world, player, 1);
        listener = std::make_unique<Listener>(*gun);
        world.AddStepListener(listener.get());
    }
    ~Fixture() { world.RemoveStepListener(listener.get()); gun.reset(); }
    void tick(unsigned count = 1) {
        for (unsigned i=0; i<count; ++i) {
            check(world.Update(dt*collision_steps, collision_steps, &allocator, &jobs) == EPhysicsUpdateError::None, "native update succeeds");
            gun->post_step(); // Constraint teardown is outside Update.
            check(std::abs(listener->actual_dt-dt)<1e-8F, "listener receives actual collision dt");
        }
    }
    State state(BodyID id) {
        BodyLockRead lock(world.GetBodyLockInterface(), id);
        check(lock.Succeeded(), "actual endpoint body exists");
        const Body &body=lock.GetBody();
        const auto *motion=body.GetMotionProperties();
        const Vec3 v=body.GetLinearVelocity(), w=body.GetAngularVelocity();
        double rotational=0;
        if (w.LengthSq()>0) {
            const Mat44 r=Mat44::sRotation(body.GetRotation());
            const Mat44 inertia=r*motion->GetLocalSpaceInverseInertia().Inversed3x3()*r.Transposed3x3();
            rotational=.5*double(w.Dot(inertia*w));
        }
        const double energy=.5*double(v.LengthSq())/motion->GetInverseMass()+rotational;
        check(std::isfinite(energy), "finite endpoint kinetic energy");
        return {body.GetCenterOfMassPosition(),v,w,energy};
    }
    BodyID fire() { check(gun->fire(Vec3::sAxisX()), "finite cartridge fires"); return gun->readback().hook; }
    void velocity(BodyID id, Vec3 v) { world.GetBodyInterface().SetLinearAndAngularVelocity(id,v,Vec3::sZero()); }
    void move(BodyID id, Vec3 delta) {
        auto &bodies=world.GetBodyInterface();
        bodies.SetPosition(id,bodies.GetPosition(id)+delta,EActivation::Activate);
    }
    void wall(RVec3 position, Vec3 half) {
        BodyCreationSettings settings(new BoxShape(half),position,Quat::sIdentity(),EMotionType::Static,0);
        check(!world.GetBodyInterface().CreateAndAddBody(settings,EActivation::DontActivate).IsInvalid(), "real wall exists");
    }
};
bool unchanged(const State &a, const State &b) {
    return (a.velocity-b.velocity).LengthSq()==0 && (a.angular-b.angular).LengthSq()==0;
}
void terminal_mass_and_inertia() {
    Fixture f;
    const BodyID hook=f.fire();
    BodyLockRead lock(f.world.GetBodyLockInterface(),hook);
    check(lock.Succeeded(), "launched terminal body exists");
    const Body &body=lock.GetBody();
    const auto *motion=body.GetMotionProperties();
    const Vec3 com=body.GetShape()->GetCenterOfMass();
    const Mat44 inertia=motion->GetLocalSpaceInverseInertia().Inversed3x3();
    std::cout << "TERMINAL mass_kg=" << 1.0F/motion->GetInverseMass()
              << " com_y_m=" << com.GetY() << " inertia_x=" << inertia.GetColumn3(0).GetX()
              << " inertia_y=" << inertia.GetColumn3(1).GetY() << '\n';
    check(std::abs(1.0F/motion->GetInverseMass()-2.0F)<1e-5F,
          "actual terminal remains 2 kg");
    check(std::abs(body.GetShape()->GetMassProperties().mMass-2.0F)<1e-4F,
          "actual steel volumes produce 2 kg without envelope-density substitution");
    check((com-Vec3(0,.0681952268F,0)).Length()<2e-5F,
          "crook COM follows actual nonoverlapping steel prisms");
    check(com.GetY()<.10F-.03F,
          "real terminal mass hangs below the cavity without an orientation motor");
    // Independently derived rectangular/triangular prism and cylinder tensor,
    // including parallel-axis terms for the actual steel constituents.
    check(std::abs(inertia.GetColumn3(0).GetX()-.01366051939F)<3e-6F &&
          std::abs(inertia.GetColumn3(1).GetY()-.02089808558F)<3e-6F &&
          std::abs(inertia.GetColumn3(2).GetZ()-.02564149302F)<3e-6F,
          "actual body inertia follows authored steel geometry");
    check(std::abs(inertia.GetColumn3(0).GetY())<3e-6F,
          "symmetric steel layout preserves independently integrated zero cross inertia");
}
void launch_momentum_energy() {
    check(Gun::hook_mass_kg==2.0F && Gun::cartridge_energy_j==180.0F, "published hook mass and cartridge energy contract");
    Fixture f(Vec3(0,-9.81F,0));
    const Vec3 inherited(3,2,-4);
    f.velocity(f.player,inherited);
    const BodyID hook=f.fire();
    const State p=f.state(f.player), h=f.state(hook);
    const Vec3 reaction=player_mass*(p.velocity-inherited);
    const Vec3 shot=Gun::hook_mass_kg*(h.velocity-inherited);
    check((reaction+shot).Length()<2e-5F && shot.GetX()>20, "actual launch has equal/opposite impulse");
    check(std::abs(h.velocity.GetY()-inherited.GetY())<1e-6F && std::abs(h.velocity.GetZ()-inherited.GetZ())<1e-6F,
          "hook inherits world velocity instead of replacing it with shot speed");
    const double initial=.5*(player_mass+Gun::hook_mass_kg)*inherited.LengthSq();
    const double added=p.kinetic+h.kinetic-initial;
    const auto rb=f.gun->readback();
    check(added>179.99 && added<=Gun::cartridge_energy_j+.001 && std::abs(added-rb.last_launch_work_j)<.001,
          "measured kinetic increment matches finite 180 J cartridge");
    const double start=p.kinetic+h.kinetic+9.81*(player_mass*p.center.GetY()+Gun::hook_mass_kg*h.center.GetY());
    f.tick(30);
    const State endp=f.state(f.player), endh=f.state(hook);
    const double end=endp.kinetic+endh.kinetic+9.81*(player_mass*endp.center.GetY()+Gun::hook_mass_kg*endh.center.GetY());
    check(end<=start+.02, "free payout supplies no energy after accounting for gravity");
    std::cout << "LAUNCH added_j=" << added << " paid_j=" << rb.last_launch_work_j << " momentum_error=" << (reaction+shot).Length() << '\n';
}
void launch_orientation_follows_horizontal_bearing() {
    const Vec3 aim=Vec3(.13F,.9915F,0).Normalized();
    Fixture reference;
    check(reference.gun->fire(aim),"reference high shot launches");
    const auto base=reference.world.GetBodyInterface().GetRotation(reference.gun->readback().hook);
    for(float angle:{JPH_PI*.5F,JPH_PI,JPH_PI*1.5F}) {
        const auto yaw=Quat::sRotation(Vec3::sAxisY(),angle);
        Fixture turned;
        check(turned.gun->fire(yaw*aim),"turned ordinary aim launches");
        const auto actual=turned.world.GetBodyInterface().GetRotation(turned.gun->readback().hook);
        check((actual*Vec3::sAxisY()-yaw*aim).Length()<2.e-5F,
              "actual launched stem follows aim");
        check((actual*Vec3::sAxisZ()-yaw*(base*Vec3::sAxisZ())).Length()<2.e-5F &&
              (actual*Vec3::sAxisX()-yaw*(base*Vec3::sAxisX())).Length()<2.e-5F,
              "turning the gun rotates its actual finger row and eye with horizontal bearing");
    }
    // The presentation prediction must name the actual authored hook origin,
    // which differs from its center of mass once the crook is rotated.
    const Vec3 directions[]{Vec3::sAxisX(),-Vec3::sAxisZ(),
        Vec3(-.6F,.8F,0),Vec3(.3F,-.4F,.5F).Normalized(),
        Vec3(.00001F,1,-.00002F).Normalized(),Vec3::sAxisY(),-Vec3::sAxisY()};
    for(const Vec3 direction:directions) {
        Fixture shot;
        shot.world.GetBodyInterface().SetPosition(shot.player,RVec3(17.25F,42.5F,-9.75F),EActivation::Activate);
        const State before=shot.state(shot.player);
        const auto inventory=shot.gun->readback();
        const RVec3 start=Gun::muzzle_start(before.center);
        const RVec3 origin=Gun::launch_origin(before.center,direction);
        check((start-(before.center+Vec3(0,.35F,0))).LengthSq()==0,
              "published muzzle start preserves the existing 350 mm COM offset");
        check((Vec3(origin-start)-direction*.85F).Length()<4.e-6F,
              "published launch frame preserves existing 850 mm forward clearance");
        check(shot.gun->fire(direction),"ordinary clear yaw/pitch prediction fires a real hook");
        const auto fired=shot.gun->readback();
        const auto actual=shot.world.GetBodyInterface().GetPosition(fired.hook);
        check((actual-origin).Length()<4.e-6F,
              "actual fired hook body origin agrees with the shared launch prediction before any solve");
        check((shot.world.GetBodyInterface().GetPosition(shot.player)-before.center).LengthSq()==0,
              "predicting and firing never relocates the player");
        check(fired.cartridges+1==inventory.cartridges&&fired.available_hooks+1==inventory.available_hooks&&
              fired.last_launch_work_j>179.99&&fired.last_launch_work_j<=180.0,
              "predicted real shot spends exactly one finite hook/cartridge and existing 180 J work");
        const State player=shot.state(shot.player),hook=shot.state(fired.hook);
        check(player.velocity.Dot(direction)<0&&hook.velocity.Dot(direction)>0&&
              (player_mass*player.velocity+Gun::hook_mass_kg*hook.velocity).Length()<3.e-5F,
              "launch-frame integration preserves actual equal/opposite recoil across aim directions");
    }
}
void passive_release() {
    for (bool moving : {false,true}) {
        Fixture f;
        const BodyID hook=f.fire();
        check(f.gun->lock_line(), "line locks");
        f.velocity(f.player,moving?Vec3(0,3,0):Vec3::sZero());
        f.velocity(hook,moving?Vec3(0,-2,1):Vec3::sZero());
        f.move(hook,Vec3(.02F,0,0)); // Explicit initial elastic load; not tool actuation.
        if (moving) f.tick();
        const State p=f.state(f.player), h=f.state(hook);
        check(f.world.GetConstraints().size()==1, "real rope row exists before release");
        f.gun->release();
        check(!f.gun->line_active() && f.world.GetConstraints().empty(), "release removes real constraint");
        check(unchanged(p,f.state(f.player)) && unchanged(h,f.state(hook)), "release preserves both instantaneous linear/angular velocities");
        f.tick();
        check(unchanged(p,f.state(f.player)) && unchanged(h,f.state(hook)), "released solve applies no residual impulse");
    }
}
void slack_and_neutral() {
    Fixture f;
    const BodyID hook=f.fire();
    check(f.gun->lock_line(), "line locks at actual eye distance");
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    f.tick(20);
    check(f.state(f.player).kinetic+f.state(hook).kinetic<1e-9 && f.gun->readback().reel_work_j==0,
          "neutral rope adds neither energy nor uncharged work");
    f.move(hook,Vec3(-.1F,0,0));
    f.velocity(hook,Vec3(-.5F,0,0));
    const State p=f.state(f.player), h=f.state(hook);
    f.tick(10);
    check(unchanged(p,f.state(f.player)) && unchanged(h,f.state(hook)) && f.gun->readback().tension_n==0,
          "slack shortening rope never pushes or brakes endpoints");
}
void finite_inventory_retrieval() {
    check(Gun::capacity==4, "published four-cartridge stock contract");
    Fixture f;
    for (unsigned i=0;i<Gun::capacity;++i) {
        const BodyID hook=f.fire();
        check(!f.gun->retrieve() && f.gun->readback().failure==Gun::Failure::RetrievalTooFast,
              "flying hook cannot be remotely collected");
        f.gun->release();
        f.velocity(hook,f.state(f.player).velocity); // Explicit settled retrieval initial condition.
        const auto &bodies=f.world.GetBodyInterface();
        const auto grasp_side=bodies.GetPosition(hook)+bodies.GetRotation(hook)*Vec3(-.091F,.1F,.5F);
        f.world.GetBodyInterface().SetPosition(f.player,grasp_side,EActivation::Activate);
        check(f.gun->retrieve(), "nearby stationary released hook retrieves from exposed bridge side");
        const auto rb=f.gun->readback();
        check(rb.available_hooks==Gun::capacity && rb.cartridges==Gun::capacity-i-1,
              "retrieval returns hardware but never spent cartridges");
    }
    const State p=f.state(f.player);
    check(!f.gun->fire(Vec3::sAxisX()) && f.gun->readback().failure==Gun::Failure::NoInventory,
          "fifth shot fails with finite stock despite recovered hooks");
    check(unchanged(p,f.state(f.player)) && !f.gun->line_active(), "empty shot has no recoil or rope");
}
void resting_terminal_retrieval() {
    Fixture f;
    const BodyID hook=f.fire();
    f.gun->release();
    auto &bodies=f.world.GetBodyInterface();
    // Explicit settled initial condition: annular eye lower edge touches floor,
    // while the finite back bridge is exposed above and beside the support.
    bodies.SetPositionAndRotation(hook,RVec3(.3F,.37F-Gun::hook_eye_local().GetY(),0),Quat::sIdentity(),EActivation::Activate);
    bodies.SetPosition(f.player,bodies.GetPosition(hook)+Vec3(-.091F,.11F,.60F),EActivation::Activate);
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    f.wall(RVec3(.3F,.33F,0),Vec3(.5F,.02F,.5F));
    const auto before=f.gun->readback();
    check(f.gun->retrieve(), "resting terminal retrieves by its exposed head with full hand clearance");
    const auto after=f.gun->readback();
    check(after.available_hooks==before.available_hooks+1 && after.cartridges==before.cartridges,
          "actual loose hardware returns without restoring spent cartridge");
}
void gravity_settled_terminal_retrieval() {
    Fixture f(Vec3(0,-9.81F,0)); auto &bodies=f.world.GetBodyInterface();
    f.collision_steps=4; // Production90Hz Update, four collision steps, one post_step.
    // Match the real miss launch, height, orientation, friction and24mm plate.
    // The player's later staging isolates grasp, never repairs the settled hook.
    bodies.SetPosition(f.player,RVec3(12,485.15F,-135.5F),EActivation::Activate);
    bodies.SetGravityFactor(f.player,0); f.velocity(f.player,Vec3::sZero());
    BodyCreationSettings plate(new BoxShape(Vec3(5.7F,.012F,1.875F),0),
        RVec3(16.5F,481.737F,-136.425F),Quat::sIdentity(),EMotionType::Static,0);
    plate.mFriction=.8F;
    check(!bodies.CreateAndAddBody(plate,EActivation::DontActivate).IsInvalid(),
          "actual zero-radius24mm plate supports gravity-settled hook");
    check(f.gun->fire({.8F,-.6F,0}),"actual180J miss angle launches real hook");
    const BodyID hook=f.gun->readback().hook; f.gun->release();
    f.velocity(f.player,Vec3::sZero());
    f.tick(360);
    const auto hook_position=bodies.GetPosition(hook);
    const auto hook_rotation=bodies.GetRotation(hook);
    const auto bridge=hook_position+hook_rotation*Vec3(-.091F,.1F,0);
    bodies.SetPosition(f.player,bridge+Vec3(.4F,.6F,.05F),EActivation::Activate);
    const auto before=f.gun->readback(); const auto settled=f.state(hook);
    std::cout << "GRAVITY_PICKUP center=" << settled.center.GetX() << ',' << settled.center.GetY() << ','
              << settled.center.GetZ() << " speed=" << settled.velocity.Length() << " body="
              << hook_position.GetX() << ',' << hook_position.GetY() << ',' << hook_position.GetZ()
              << " q=" << hook_rotation.GetX() << ',' << hook_rotation.GetY() << ',' << hook_rotation.GetZ() << ',' << hook_rotation.GetW() << '\n';
    if (f.gun->retrieve()) {
        const auto after=f.gun->readback();
        check(after.available_hooks==before.available_hooks+1 && after.cartridges==before.cartridges,
              "direct clear gravity retrieval restores hardware without ammunition");
        std::cout << "GRAVITY_PICKUP_WORK paid_j=0 recovered=1 direct=1\n";
        return;
    }
    check(f.gun->pickup_active(),"blocked gravity pose starts physical pull without inventory removal");
    const State p0=f.state(f.player), h0=f.state(hook);
    const double work0=f.gun->pickup_work_j();
    f.gun->pre_step(dt);
    const State p1=f.state(f.player), h1=f.state(hook);
    const Vec3 impulse=player_mass*(p1.velocity-p0.velocity);
    check((impulse+Gun::hook_mass_kg*(h1.velocity-h0.velocity)).Length()<2.e-4F,
          "physical pickup impulse and player reaction conserve pair linear momentum");
    check(impulse.Length()>0 && impulse.Length()<=1500*dt+1.e-5F &&
          f.gun->pickup_work_j()-work0<=750*dt+1.e-5 &&
          f.gun->pickup_work_j()-work0>=p1.kinetic+h1.kinetic-p0.kinetic-h0.kinetic-1.e-4,
          "actual pickup respects force and positive paired kineticwork");
    check((p1.center-p0.center).LengthSq()==0 && (h1.center-h0.center).LengthSq()==0,
          "pickup supplies impulse without initial pose assignment");
    bool recovered=false;
    for (unsigned i=0;i<300;++i) {
        f.tick();
        if(f.gun->readback().available_hooks==before.available_hooks+1) { recovered=true; break; }
        if(i%30==29 && !f.gun->pickup_active()) (void)f.gun->retrieve();
    }
    std::cout << "GRAVITY_PICKUP_WORK paid_j=" << f.gun->pickup_work_j() << " recovered=" << recovered << '\n';
    check(recovered,"real gravity-settled hook retrieves through collision-clear physical pickup");
    const auto after=f.gun->readback();
    check(after.available_hooks==before.available_hooks+1 && after.cartridges==before.cartridges,
          "gravity recovery restores actual hardware without spent ammunition");
}
void captured_terminal_cannot_withdraw_through_slot() {
    // Identical hand approach succeeds without the bar and fails when the
    // complete closed C sector would have to pass through real retaining steel.
    for (bool retained:{false,true}) {
        Fixture f;
        const BodyID hook=f.fire(); f.gun->release();
        auto &bodies=f.world.GetBodyInterface();
        bodies.SetPositionAndRotation(hook,RVec3::sZero(),Quat::sIdentity(),EActivation::Activate);
        bodies.SetPosition(f.player,RVec3(-.091F,-.35F,.50F),EActivation::Activate);
        f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
        if (retained) {
            BodyCreationSettings bar(new CylinderShape(.115F,.020F,0),RVec3(0,.129F,0),
                Quat::sFromTo(Vec3::sAxisY(),Vec3::sAxisZ()),EMotionType::Static,0);
            check(!bodies.CreateAndAddBody(bar,EActivation::DontActivate).IsInvalid(),
                  "actual solid retaining bar exists inside C aperture");
        }
        const auto before=f.gun->readback();
        const bool retrieved=f.gun->retrieve();
        if (retained) {
            check(!retrieved && f.gun->readback().failure==Gun::Failure::RetrievalBlocked,
                  "clear hand approach never permits whole crook withdrawal through retaining bar");
            check(f.gun->readback().available_hooks==before.available_hooks &&
                  f.gun->readback().cartridges==before.cartridges,
                  "blocked withdrawal preserves actual hardware and spent stock");
        } else check(retrieved,"same exposed grasp and withdrawal is physically clear without bar");
    }
}
void hand_clearance_includes_other_hook_parts() {
    Fixture f;
    const BodyID hook=f.fire(); f.gun->release();
    auto &bodies=f.world.GetBodyInterface();
    bodies.SetPositionAndRotation(hook,RVec3::sZero(),Quat::sIdentity(),EActivation::Activate);
    bodies.SetPosition(f.player,RVec3(-.091F,.1F,0),EActivation::Activate);
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    const auto before=f.gun->readback();
    check(!f.gun->retrieve() && f.gun->readback().failure==Gun::Failure::RetrievalBlocked,
          "initial hand penetrating multiple crook leaves is never admitted as clear grasp");
    check(f.gun->readback().available_hooks==before.available_hooks,
          "inaccessible grasp leaves actual hardware in world");
}
void pickup_overspeed_cancels_without_impulse() {
    Fixture f; const BodyID hook=f.fire(); f.gun->release();
    auto &bodies=f.world.GetBodyInterface();
    bodies.SetPositionAndRotation(hook,RVec3::sZero(),Quat::sIdentity(),EActivation::Activate);
    bodies.SetPosition(f.player,RVec3(-.091F,-.35F,.50F),EActivation::Activate);
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    BodyCreationSettings bar(new CylinderShape(.115F,.020F,0),RVec3(0,.129F,0),
        Quat::sFromTo(Vec3::sAxisY(),Vec3::sAxisZ()),EMotionType::Static,0);
    check(!bodies.CreateAndAddBody(bar,EActivation::DontActivate).IsInvalid(),"physical blocking bar exists");
    check(!f.gun->retrieve() && f.gun->pickup_active(),"clear grasp requests blocked physical pickup");
    // External motion before the next collision can exceed the grasp bound.
    // Rejection must not enter an infeasible billion-ULP rounding loop.
    f.velocity(hook,Vec3(-.001F,1.00001F,0));
    const auto p=f.state(f.player),h=f.state(hook); const auto work=f.gun->pickup_work_j();
    f.gun->pre_step(dt);
    check(!f.gun->pickup_active() && unchanged(p,f.state(f.player)) && unchanged(h,f.state(hook)) &&
          f.gun->pickup_work_j()==work,"overspeed pickup cancels promptly without impulse or paid work");
}
void pickup_completion_keeps_requested_hook() {
    Fixture f; auto &bodies=f.world.GetBodyInterface();
    const BodyID requested=f.fire(); f.gun->release();
    f.velocity(f.player,Vec3::sZero());
    bodies.SetPosition(requested,RVec3(5,0,0),EActivation::Activate);
    const BodyID other=f.fire(); f.gun->release();
    bodies.SetPositionAndRotation(requested,RVec3::sZero(),Quat::sIdentity(),EActivation::Activate);
    bodies.SetPositionAndRotation(other,RVec3(4,0,0),Quat::sIdentity(),EActivation::Activate);
    bodies.SetPosition(f.player,RVec3(-.091F,-.35F,.50F),EActivation::Activate);
    f.velocity(f.player,Vec3::sZero()); f.velocity(requested,Vec3::sZero()); f.velocity(other,Vec3::sZero());
    BodyCreationSettings bar(new CylinderShape(.115F,.020F,0),RVec3(0,.129F,0),
        Quat::sFromTo(Vec3::sAxisY(),Vec3::sAxisZ()),EMotionType::Static,0);
    check(!bodies.CreateAndAddBody(bar,EActivation::DontActivate).IsInvalid(),"request identity blocking bar exists");
    check(!f.gun->retrieve() && f.gun->pickup_active(),"Action requests actual obstructed first hook");
    const auto before=f.gun->readback();
    // Explicit next-frame pose isolates identity when another loose hook is
    // nearer and clear. It must not inherit automatic grasp completion.
    bodies.SetPosition(other,RVec3(-.6F,-.45F,.4F),EActivation::Activate);
    f.gun->post_step();
    check(f.gun->readback().available_hooks==before.available_hooks && f.gun->pickup_active(),
          "automatic completion never collects a different nearer old hook");
    (void)f.state(other); // Its actual body must still exist.
}
void wall_collision_does_not_anchor() {
    Fixture f(Vec3(0,-9.81F,0));
    f.wall(RVec3(3,0,0),Vec3(.1F,10,10));
    const BodyID hook=f.fire();
    f.tick(90);
    const State hit=f.state(hook);
    check(hit.center.GetX()<2.81 && hit.velocity.GetX()<1, "actual hook is stopped by wall contact");
    f.gun->release();
    check(f.world.GetConstraints().empty(), "wall collision creates no fixed anchor");
    f.tick(90);
    const State falling=f.state(hook);
    check(falling.center.GetY()<hit.center.GetY()-.1 && falling.velocity.GetY()<-.5F,
          "released collided hook remains dynamic and falls under gravity");
}
void obstructed_line_cancels_force() {
    Fixture f;
    const BodyID hook=f.fire();
    check(f.gun->lock_line(), "line locks");
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    f.move(hook,Vec3(.1F,0,0)); // Loaded rope would pull without obstruction.
    const auto rb=f.gun->readback();
    f.wall((rb.player_endpoint+rb.hook_endpoint)*.5F,Vec3(.025F,.025F,.3F));
    const State p=f.state(f.player), h=f.state(hook);
    f.tick();
    check(f.gun->readback().failure==Gun::Failure::Obstructed && !f.gun->line_active() && f.world.GetConstraints().empty(),
          "straight rope obstruction disables row and removes it after update");
    check(unchanged(p,f.state(f.player)) && unchanged(h,f.state(hook)), "blocked rope cannot pull through real wall");
}
void finite_paid_reel_reaction() {
    check(Gun::reel_power_w==750.0F && Gun::reel_force_n==1500.0F && Gun::reel_speed_m_per_s==1.0F,
          "published finite human reel contract");
    Fixture f;
    const BodyID hook=f.fire();
    check(f.gun->lock_line(), "line locks");
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    f.move(hook,Vec3(.2F,0,0)); // Known external preload below 1500 N actuator limit.
    const auto before=f.gun->readback();
    const State p=f.state(f.player), h=f.state(hook);
    const double radius=Vec3(before.hook_endpoint-before.player_endpoint).Length();
    f.gun->set_reel_input(1);
    f.tick();
    const auto after=f.gun->readback();
    const double stroke=double(before.line_length_m)-after.line_length_m;
    const double work=after.reel_work_j-before.reel_work_j;
    const double extension=std::max(0.0,radius-before.line_length_m);
    const double elastic_added=.5*Gun::stiffness_n_per_m*(std::pow(extension+stroke,2)-extension*extension);
    check(stroke>0 && stroke<=Gun::reel_speed_m_per_s*dt+1e-8 && work>0 && work<=Gun::reel_power_w*dt+1e-6,
          "actual reel target advance obeys speed and paid per-step power");
    check(Gun::stiffness_n_per_m*(extension+stroke)<=Gun::reel_force_n+1e-4 && work+1e-6>=elastic_added,
          "actual target respects force limit and charges added elastic storage");
    const State endp=f.state(f.player), endh=f.state(hook);
    const Vec3 impulse=player_mass*(endp.velocity-p.velocity), reaction=Gun::hook_mass_kg*(endh.velocity-h.velocity);
    check(impulse.Length()>.1F && impulse.Dot(Vec3(before.hook_endpoint-before.player_endpoint))>0 &&
          (impulse+reaction).Length()<1e-4F, "real loaded rope gives reciprocal tensile endpoint impulses");
    f.gun->set_reel_input(0);
    const double paid=after.reel_work_j;
    const float rest=after.line_length_m;
    f.tick(10);
    check(f.gun->line_active() && f.gun->readback().line_length_m==rest && f.gun->readback().reel_work_j==paid,
          "neutral reel stops target advancement and additional work");
    std::cout << "REEL stroke_m=" << stroke << " paid_j=" << work << " elastic_target_added_j=" << elastic_added
              << " reciprocal_impulse_error=" << (impulse+reaction).Length() << " full_energy_closure=unverified\n";
}
void outward_loaded_reel_force_limit() {
    Fixture f;
    const BodyID hook=f.fire();
    check(f.gun->lock_line(), "outward loaded line locks");
    const auto initial=f.gun->readback();
    const Vec3 axis=Vec3(initial.hook_endpoint-initial.player_endpoint).Normalized();
    // Explicit external initial energy, not work supplied by the reel. At this
    // collision dt, .20 m stretch plus 6 m/s separation loads the damping row
    // beyond 1500 N even though the previous solved tension is still zero.
    f.move(hook,.20F*axis);
    f.velocity(f.player,Vec3::sZero());
    f.velocity(hook,6.0F*axis);
    const auto before=f.gun->readback();
    const State p=f.state(f.player), h=f.state(hook);
    check(before.tension_n<=Gun::reel_force_n, "previous solved tension alone admits counterexample");
    f.gun->set_reel_input(1);
    f.tick();
    const auto after=f.gun->readback();
    const double stroke=double(before.line_length_m)-after.line_length_m;
    const double work=after.reel_work_j-before.reel_work_j;
    check(f.gun->line_active() && after.tension_n>0 && after.tension_n<=Gun::strength_n+.1F,
          "outward preload exercises actual surviving passive rope row");
    check(stroke>=0 && stroke<=Gun::reel_speed_m_per_s*dt+1e-8 && work>=0 && work<=Gun::reel_power_w*dt+1e-6,
          "outward loaded take-up retains per-step speed and paid power bounds");
    check(stroke==0 || after.tension_n<=Gun::reel_force_n+.1F,
          "powered loaded wind never solves above the 1500 N reel limit");
    if (stroke==0)
        check(work==0, "stalled take-up charges no work");
    const State endp=f.state(f.player), endh=f.state(hook);
    const Vec3 impulse=player_mass*(endp.velocity-p.velocity);
    const Vec3 reaction=Gun::hook_mass_kg*(endh.velocity-h.velocity);
    check(impulse.Dot(axis)>.1F && (impulse+reaction).Length()<1e-4F,
          "force limit is checked on actual reciprocal solver impulses");
    std::cout << "OUTWARD_REEL stroke_m=" << stroke << " solved_tension_n=" << after.tension_n
              << " paid_j=" << work << " passive_strength_n=" << Gun::strength_n << '\n';
}
void automatic_reel_latches_and_stops() {
    Fixture f; const BodyID hook=f.fire();
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    check(f.gun->set_auto_reel(true),"one request starts automatic finite reel and locks line");
    check(f.gun->readback().auto_reel && f.gun->readback().state==Gun::LineState::Locked,
          "native automatic state reports real locked reel intent");
    const float before=f.gun->readback().line_length_m;
    for(unsigned i=0;i<12;++i) { f.gun->set_reel_input(0); f.tick(); }
    check(f.gun->readback().line_length_m<before && f.gun->readback().auto_reel,
          "neutral frame input never cancels latched automatic shortening");
    check(f.gun->set_auto_reel(false),"explicit Stop accepted");
    const auto stopped=f.gun->readback(); f.tick(8);
    check(!f.gun->readback().auto_reel && f.gun->readback().line_length_m==stopped.line_length_m &&
          f.gun->readback().reel_work_j==stopped.reel_work_j,"Stop prevents target motion and added reelwork");
    check(f.gun->set_auto_reel(true),"automatic mode restarts");
    f.gun->set_reel_input(-1); f.tick();
    check(!f.gun->readback().auto_reel,"ordinary manual payout cancels automatic mode");
    f.gun->set_reel_input(0); check(f.gun->set_auto_reel(true),"mode restarts after manualoverride");
    f.gun->release(); check(!f.gun->readback().auto_reel && !f.gun->line_active(),"Release clears automatic intent");
    check(!f.gun->set_auto_reel(true),"absent line cannot start automatic reel");
}
void automatic_reel_limits_and_failure() {
    Fixture f; const BodyID hook=f.fire();
    f.velocity(f.player,Vec3::sZero()); f.velocity(hook,Vec3::sZero());
    check(f.gun->set_auto_reel(true),"automatic reel starts for boundtest");
    for(unsigned i=0;i<1200 && f.gun->readback().auto_reel;++i) {
        const auto before=f.gun->readback(); f.gun->set_reel_input(0); f.tick();
        const auto after=f.gun->readback();
        check(after.line_length_m>=Gun::minimum_line_length_m &&
              double(before.line_length_m)-after.line_length_m<=Gun::reel_speed_m_per_s*dt+1.e-6 &&
              after.reel_work_j-before.reel_work_j<=Gun::reel_power_w*dt+1.e-5,
              "automatic mode uses existing finite length speed and paidpower");
    }
    check(!f.gun->readback().auto_reel && f.gun->readback().line_length_m<=Gun::minimum_line_length_m+1.e-6,
          "automatic reel stops at real winding limit");
    // A separate unobstructed muzzle isolates line-failure cleanup: the first
    // actual rewound hook remains a body and can legitimately block another shot.
    Fixture blocked; const BodyID next=blocked.fire();
    blocked.velocity(blocked.player,Vec3::sZero()); blocked.velocity(next,Vec3::sZero());
    check(blocked.gun->set_auto_reel(true),"new shot starts separate automatic mode");
    const auto g=blocked.gun->readback(); blocked.wall((g.player_endpoint+g.hook_endpoint)*.5F,{.02F,.1F,.1F}); blocked.tick();
    check(blocked.gun->readback().failure==Gun::Failure::Obstructed && !blocked.gun->readback().auto_reel &&
          !blocked.gun->line_active(),"real rope obstruction clears automatic mode");
    f.gun->clear_transients();check(!f.gun->readback().auto_reel,"teardown clears automatic state");
}
void square_retention_observation() {
 const auto retained=[](Vec3 center,float width=.025F,float length=.54F,Vec3 axis=Vec3::sAxisZ()) {
  return scraperx::test::retained_square_in_crook(center,axis,Vec3::sAxisX(),Vec3::sAxisY(),width,length,PhysicsSettings{}.mPenetrationSlop);
 };
 check(retained({0,.1F,0}),"real50mm square fits all native fingers");
 check(!retained({0,.089F,0}),"whole square escaping open tips is rejected without mouth slop");
 check(!retained({.10F,.1F,0}),"square beyond closed steel rejected");
 check(!retained({0,.1F,.55F}),"finite rail end cannot cover outer fingers");
 check(!retained({0,.1F,0},.025F,.05F),"too short rail misses outer fingers");
 check(!retained({0,.1F,0},.025F,.54F,Vec3::sZero()),"invalid rail frame rejected");
 check(!retained({0,.1F,0},0),"zero section rejected");
 //70mm rail measured perching has aperture64mm above bar center.
 check(!retained({0,.036F,0},.035F,.6F),"measured70mm tip perch never claims capture");
}
void rotated_retention_observation() {
    const auto retained=[](Vec3 center,Vec3 axis=Vec3::sAxisZ()) {
        return scraperx::test::retained_bar_in_crook(center,axis,.020F,.115F,.103F,
                                                    PhysicsSettings{}.mPenetrationSlop);
    };
    // Actual loaded tick1353: closed left contact, independently checked
    // four-face penetration3.17..3.21mm and collar clearance8.53mm.
    check(retained({-.0314965F,.106847F,.00278337F},
                   {-.000203827F,-.00217806F,.999998F}),
          "rotated real closed-face contact remains retained");
    check(retained({.0178458F,.123548F,.0026473F},
                   {-.000524756F,.000185047F,1}),
          "later actual closed upper contact remains retained");
    check(!retained({0,.025F,0}),"actual downward mouth escape rejected");
    check(!retained({.095F,.1F,0}),"crossbar outside closed steel rejected");
    check(!retained({0,.129F,.020F}),"hook beyond actual collar innerface rejected");
    check(!retained({0,.129F,0},{.8F,0,.6F}),"tilted bar missing outer fingers rejected");
}
} // namespace
int main() {
    RegisterDefaultAllocator(); Factory::sInstance=new Factory; RegisterTypes();
    const std::pair<const char *,void(*)()> tests[]{
        {"automatic_reel_latches_and_stops",automatic_reel_latches_and_stops},
        {"automatic_reel_limits_and_failure",automatic_reel_limits_and_failure},
        {"rotated_retention_observation",rotated_retention_observation},
        {"square_retention_observation",square_retention_observation},
        {"terminal_mass_and_inertia",terminal_mass_and_inertia},
        {"launch_momentum_energy",launch_momentum_energy}, {"passive_release",passive_release},
        {"launch_orientation_follows_horizontal_bearing",launch_orientation_follows_horizontal_bearing},
        {"slack_and_neutral",slack_and_neutral}, {"finite_inventory_retrieval",finite_inventory_retrieval},
        {"resting_terminal_retrieval",resting_terminal_retrieval},
        {"gravity_settled_terminal_retrieval",gravity_settled_terminal_retrieval},
        {"captured_terminal_cannot_withdraw_through_slot",captured_terminal_cannot_withdraw_through_slot},
        {"hand_clearance_includes_other_hook_parts",hand_clearance_includes_other_hook_parts},
        {"pickup_overspeed_cancels_without_impulse",pickup_overspeed_cancels_without_impulse},
        {"pickup_completion_keeps_requested_hook",pickup_completion_keeps_requested_hook},
        {"wall_collision_does_not_anchor",wall_collision_does_not_anchor},
        {"obstructed_line_cancels_force",obstructed_line_cancels_force}, {"finite_paid_reel_reaction",finite_paid_reel_reaction},
        {"outward_loaded_reel_force_limit",outward_loaded_reel_force_limit}};
    unsigned failures=0;
    for (const auto &test:tests) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception &e) { ++failures; std::cerr << "FAIL " << test.first << ": " << e.what() << '\n'; }
    }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance=nullptr;
    std::cout << "grappling gun checks: " << std::size(tests)-failures << '/' << std::size(tests) << '\n';
    return failures?1:0;
}
