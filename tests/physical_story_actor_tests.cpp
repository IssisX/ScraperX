#include "sim/physical_story_actor.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/RegisterTypes.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
using namespace JPH;
using scraperx::sim::PhysicalStoryActor;
constexpr float dt = 1.0F/90.0F;
const char *scenario = "startup";
void require(bool value, const char *message) {
    if (value) return;
    std::cerr << "FAIL PHYSICAL_STORY_ACTOR scenario=" << scenario << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}
struct BroadPhase final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "story_actor"; }
};
struct BroadFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
struct PairFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a || b; }
};
struct Listener final : ContactListener {
    BodyID actor;
    std::uint64_t tick = 0;
    std::array<PhysicalStoryActor::Contact, 32> contacts{};
    size_t count = 0;
    bool overflow = false;
    void observe(const Body &a, const Body &b, const ContactManifold &m) {
        PhysicalStoryActor::Contact contact;
        if (!PhysicalStoryActor::capture_contact(actor, a, b, m, tick, contact)) return;
        if (count == contacts.size()) { overflow = true; return; }
        contacts[count++] = contact;
    }
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &m, ContactSettings &) override {
        observe(a, b, m);
    }
    void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &m, ContactSettings &) override {
        observe(a, b, m);
    }
};
struct World final {
    BroadPhase broad;
    BroadFilter broad_filter;
    PairFilter pair_filter;
    PhysicsSystem system;
    TempAllocatorImpl allocator{8*1024*1024};
    JobSystemSingleThreaded jobs{1024};
    Listener listener;
    std::vector<BodyID> owned;
    std::uint64_t serial = 0;
    World() {
        system.Init(64, 0, 128, 256, broad, broad_filter, pair_filter);
        system.SetContactListener(&listener);
    }
    ~World() {
        system.SetContactListener(nullptr);
        for (const auto body : owned) {
            system.GetBodyInterface().RemoveBody(body);
            system.GetBodyInterface().DestroyBody(body);
        }
    }
    BodyID box(Vec3 half, RVec3 position, EMotionType motion = EMotionType::Static,
               float friction = .7F, float mass = 300, Vec3 velocity = Vec3::sZero(),
               Vec3 angular = Vec3::sZero()) {
        BodyCreationSettings settings(new BoxShape(half), position, Quat::sIdentity(), motion,
                                      motion == EMotionType::Static ? 0 : 1);
        settings.mFriction = friction;
        settings.mLinearDamping = 0;
        settings.mAngularDamping = 0;
        settings.mAllowSleeping = false;
        settings.mLinearVelocity = velocity;
        settings.mAngularVelocity = angular;
        if (motion == EMotionType::Dynamic) {
            settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = mass;
        }
        auto body = system.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        require(!body.IsInvalid(), "fixture body allocated");
        owned.push_back(body);
        return body;
    }
    void tick(PhysicalStoryActor &actor, Vec3 intent = Vec3::sZero(), bool publish = true) {
        ++serial;
        actor.pre_step(serial, dt, intent);
        const auto drive = actor.readback();
        require(drive.applied_force_n.Length() <= PhysicalStoryActor::force_bound_n()+.001F,
                "drive respects finite horizontal force bound");
        require(std::abs(drive.applied_force_n.GetY()) < .00001F, "drive never manufactures vertical support");
        require(drive.command_work_bound_j <= PhysicalStoryActor::power_bound_w()*dt+.00001,
                "positive command work respects per-tick power bound");
        listener.actor = actor.body_id();
        listener.tick = serial;
        listener.count = 0;
        listener.overflow = false;
        require(system.Update(dt, 4, &allocator, &jobs) == EPhysicsUpdateError::None,
                "actual Jolt update succeeds");
        require(!listener.overflow, "contact buffer never silently truncates");
        if (publish) for (size_t i=0; i<listener.count; ++i) actor.submit_solved_contact(listener.contacts[i]);
    }
    void ticks(PhysicalStoryActor &actor, int count, Vec3 intent = Vec3::sZero()) {
        for (int i=0; i<count; ++i) tick(actor, intent);
    }
};
void passive_fall_and_lifecycle() {
    scenario = "passive_fall_reset_destroy";
    World w;
    PhysicalStoryActor actor(w.system, 1, 9001);
    require(actor.spawn(RVec3(0, 5, 0)), "spawn creates real body");
    {
        BodyLockRead lock(w.system.GetBodyLockInterface(), actor.body_id());
        require(lock.Succeeded() && lock.GetBody().IsDynamic() &&
                std::abs(1/lock.GetBody().GetMotionProperties()->GetInverseMass()-85) < .001F,
                "production actor body has actual85kg dynamic mass");
    }
    require(!actor.spawn(RVec3(0, 6, 0)), "second spawn cannot leak original body");
    w.ticks(actor, 45, Vec3(1, 0, 0));
    auto state = actor.readback();
    require(state.position.GetY() < 4 && state.velocity.GetY() < -4, "unsupported body falls with gravity");
    require(std::abs(state.position.GetX()) < .0001 && !state.supported, "no air drive without contact");
    const auto old = actor.body_id();
    require(actor.reset(RVec3(2, 4, 0)), "explicit reset recreates initial placement");
    require(actor.body_id() != old && w.system.GetNumBodies() == 1, "reset retires old identity without body leak");
    state = actor.readback();
    require((state.position-RVec3(2,4,0)).Length() < .0001 && state.velocity.IsNearZero(), "reset clears old momentum and contact");
    actor.destroy(); actor.destroy();
    require(w.system.GetNumBodies() == 0 && !actor.readback().present, "destroy is idempotent and frees native body");
}
void stationary_advance_obstruction_push() {
    scenario = "stationary_advance_obstruction_push";
    World w;
    w.box(Vec3(20,.5F,20), RVec3(0,-.5,0));
    w.box(Vec3(.2F,2,3), RVec3(3,2,0));
    PhysicalStoryActor actor(w.system, 1, 9001);
    require(actor.spawn(RVec3(0,1.1,0)), "actor spawns above actual floor");
    w.ticks(actor, 180);
    auto state = actor.readback();
    require(state.supported && std::abs(state.position.GetY()-.9) < .04 && state.velocity.Length() < .02,
            "actual contact supports stationary adult capsule");
    w.ticks(actor, 90, Vec3(1.4F,0,0));
    require(actor.readback().position.GetX() > .8, "finite contact actuation advances real body");
    w.ticks(actor, 360, Vec3(1.4F,0,0));
    state = actor.readback();
    require(state.position.GetX() < 2.53 && state.position.GetX() > 2.3,
            "persistent command cannot push capsule through real wall");
    const auto before = state.position;
    w.system.GetBodyInterface().AddImpulse(actor.body_id(), Vec3(-170,0,0));
    w.tick(actor);
    require(actor.readback().velocity.GetX() < -1.9F && actor.readback().position.GetX() < before.GetX(),
            "external physical push changes actual actor momentum despite finite braking");
}
void lost_and_stale_support() {
    scenario = "lost_and_stale_support";
    World w;
    w.box(Vec3(1,.5F,2), RVec3(0,-.5,0));
    PhysicalStoryActor actor(w.system, 1, 9001);
    require(actor.spawn(RVec3(0,1,0)), "spawn above finite ledge");
    w.ticks(actor, 90);
    require(actor.readback().supported, "ledge support is real before walking off");
    // Deliberately omit publication: a cached material point cannot manufacture footing.
    w.tick(actor, Vec3(1,0,0), false);
    w.tick(actor, Vec3(1,0,0), false);
    require(!actor.readback().supported && actor.readback().applied_force_n.IsNearZero(),
            "stale actual contact cancels ground motor");
    w.ticks(actor, 200, Vec3(1.4F,0,0));
    auto state = actor.readback();
    require(state.position.GetX() > 1.3 && state.position.GetY() < -1 && !state.supported,
            "walking off actual ledge falls rather than hovering");
    const auto vx = state.velocity.GetX();
    w.ticks(actor, 20, Vec3(-1.4F,0,0));
    require(std::abs(actor.readback().velocity.GetX()-vx) < .0001,
            "opposite intent cannot steer unsupported flight");
}
void dynamic_receiver_reaction() {
    scenario = "dynamic_receiver_reaction";
    World w;
    w.box(Vec3(30,.5F,30), RVec3(0,-.5,0), EMotionType::Static, 0);
    const auto slab = w.box(Vec3(5,.2F,5), RVec3(0,.2,0), EMotionType::Dynamic, .7F, 300);
    PhysicalStoryActor actor(w.system, 1, 9001);
    require(actor.spawn(RVec3(1.2,1.31,0)), "actor spawns on freely moving massive receiver");
    w.ticks(actor, 180);
    const auto actor_before = actor.readback().velocity;
    const auto slab_before = w.system.GetBodyInterface().GetLinearVelocity(slab);
    w.tick(actor, Vec3(0,0,1.4F));
    const auto a = actor.readback();
    const auto v = w.system.GetBodyInterface().GetLinearVelocity(slab);
    require(a.applied_force_n.GetZ() > 100 && a.velocity.GetZ() > actor_before.GetZ(), "supported drive loads actual actor");
    require(v.GetZ() < slab_before.GetZ()-.001F, "dynamic receiver gets opposite horizontal reaction");
    const float momentum = PhysicalStoryActor::mass_kg()*(a.velocity.GetZ()-actor_before.GetZ())+
        300*(v.GetZ()-slab_before.GetZ());
    require(std::abs(momentum) < .005F, "one reciprocal actuator preserves pair horizontal momentum");
    require(w.system.GetBodyInterface().GetAngularVelocity(slab).GetY() > .0001F,
            "off-center support reaction transmits real moment");
}
void rotating_support_point_velocity() {
    scenario = "rotating_support_point_velocity";
    World w;
    const auto slab = w.box(Vec3(5,.2F,5), RVec3(0,0,0), EMotionType::Kinematic, .7F, 300,
                            Vec3::sZero(), Vec3(0,.2F,0));
    PhysicalStoryActor actor(w.system, 1, 9001);
    require(actor.spawn(RVec3(1.2,1.11,0)), "actor spawns off rotating support center");
    w.ticks(actor, 90);
    const auto state = actor.readback();
    require(state.supported && state.support == slab, "rotating surface remains actual support");
    require(state.support_point_velocity.Length() > .15F &&
            w.system.GetBodyInterface().GetLinearVelocity(slab).Length() < .001F,
            "drive reads angular support point velocity rather than COM velocity");
    require(state.velocity.GetZ() < -.1F, "finite traction responds to rotating support");
}
void sensor_and_traction_negatives() {
    scenario = "sensor_cannot_supply_footing";
    {
        World w;
        const auto sensor = w.box(Vec3(20,.5F,20), RVec3(0,-.5,0));
        {
            BodyLockWrite lock(w.system.GetBodyLockInterface(), sensor);
            require(lock.Succeeded(), "sensor fixture body exists");
            lock.GetBody().SetIsSensor(true);
        }
        PhysicalStoryActor actor(w.system, 1, 9001);
        require(actor.spawn(RVec3(0,1,0)), "actor spawns above sensor floor");
        w.ticks(actor, 90, Vec3(1.4F,0,0));
        const auto state = actor.readback();
        require(state.position.GetY() < -2 && std::abs(state.position.GetX()) < .0001 && !state.supported,
                "actual sensor overlap cannot become ground traction or support");
    }
    scenario = "low_friction_and_duplicate_tick";
    World w;
    w.box(Vec3(20,.5F,20), RVec3(0,-.5,0), EMotionType::Static, .01F);
    PhysicalStoryActor actor(w.system, 1, 9001);
    require(actor.spawn(RVec3(0,1,0)), "actor spawns above low-friction floor");
    w.ticks(actor, 90);
    actor.pre_step(w.serial+1, dt, Vec3(1.4F,0,0));
    const auto force = actor.readback().applied_force_n;
    require(force.GetX() > 1 && force.Length() <= .01F*85*9.81F+.001F,
            "actual support material limits traction below motor force cap");
    actor.pre_step(w.serial+1, dt, Vec3(1.4F,0,0));
    {
        BodyLockRead lock(w.system.GetBodyLockInterface(), actor.body_id());
        require(lock.Succeeded(), "actor exists while checking queued force");
        require((lock.GetBody().GetAccumulatedForce()-force).Length() < .0001F,
                "duplicate native tick cannot queue motor force twice");
    }
    w.tick(actor, Vec3(1.4F,0,0));
}
}
int main() {
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    passive_fall_and_lifecycle();
    stationary_advance_obstruction_push();
    lost_and_stale_support();
    dynamic_receiver_reaction();
    rotating_support_point_velocity();
    sensor_and_traction_negatives();
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    std::cout << "PASS PHYSICAL_STORY_ACTOR real_jolt=1 cases=7 simulation_integration=unverified\n";
    return EXIT_SUCCESS;
}
