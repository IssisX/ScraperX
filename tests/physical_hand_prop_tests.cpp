#include "sim/physical_hand_prop.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
using namespace JPH;
using scraperx::sim::PhysicalHandProp;
constexpr float dt = 1.0F / 360.0F;
constexpr float player_mass = 85;
const char *scenario = "initialization";
void require(bool ok, const char *message) {
    if (ok) return;
    std::cerr << "FAIL HAND_PROP scenario=" << scenario << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "hand_prop"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return false; }
};
struct State { RVec3 center; Vec3 velocity, angular; Mat44 inverse_inertia; double mass, energy; };
struct Fixture final {
    BP bp; BV bv; LP lp;
    PhysicsSystem system;
    TempAllocatorImpl allocator{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID player, support;
    RVec3 palm;
    Fixture(bool dynamic = false, float mass = 25, float half_height = .1F,
            Vec3 velocity = Vec3(.7F, -2, .2F)) {
        system.Init(16, 0, 32, 32, bp, bv, lp);
        system.SetGravity(Vec3::sZero());
        BodyCreationSettings p(new CapsuleShape(.55F, .35F), RVec3::sZero(), Quat::sIdentity(), EMotionType::Dynamic, 1);
        p.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        p.mMassPropertiesOverride.mMass = player_mass;
        p.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        p.mLinearDamping = p.mAngularDamping = 0; p.mAllowSleeping = false;
        p.mLinearVelocity = velocity;
        player = system.GetBodyInterface().CreateAndAddBody(p, EActivation::Activate);
        BodyCreationSettings s(new BoxShape(Vec3(.3F, half_height, .2F), 0), RVec3(.8F, 0, 0),
                               Quat::sIdentity(), dynamic ? EMotionType::Dynamic : EMotionType::Static, dynamic ? 1 : 0);
        s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass = mass;
        s.mLinearDamping = s.mAngularDamping = 0; s.mAllowSleeping = false;
        support = system.GetBodyInterface().CreateAndAddBody(s, EActivation::Activate);
        palm = RVec3(.95F, half_height, 0); // Actual exposed box top; offset from its COM.
        require(!player.IsInvalid() && !support.IsInvalid(), "real diagnostic bodies allocated");
        // Isolate the palm row from ordinary collision impulses. Both actual
        // shapes remain present; their nearest X faces already have a .15m gap.
        // This fixture proves row mechanics, not acquisition or gameplay.
    }
    ~Fixture() {
        require(system.GetConstraints().empty(), "owner removes every transient constraint");
        for (auto id : {player, support}) {
            system.GetBodyInterface().RemoveBody(id);
            system.GetBodyInterface().DestroyBody(id);
        }
    }
    State state(BodyID id) {
        BodyLockRead lock(system.GetBodyLockInterface(), id);
        require(lock.Succeeded(), "diagnostic body lock succeeds");
        const auto &b = lock.GetBody();
        const auto *m = b.GetMotionProperties();
        const auto r = Mat44::sRotation(b.GetRotation());
        const auto inv = r * m->GetLocalSpaceInverseInertia() * r.Transposed3x3();
        const auto v = b.GetLinearVelocity(), w = b.GetAngularVelocity();
        const double mass = 1.0 / m->GetInverseMass();
        const double angular_energy = w.LengthSq() > 0 ? .5 * w.Dot(inv.Inversed3x3() * w) : 0;
        return {b.GetCenterOfMassPosition(), v, w, inv, mass, .5 * mass * v.LengthSq() + angular_energy};
    }
};
struct Listener final : PhysicsStepListener {
    PhysicalHandProp &prop;
    bool valid = true;
    explicit Listener(PhysicalHandProp &p) : prop(p) {}
    void OnStep(const PhysicsStepListenerContext &c) override { prop.pre_step(c.mDeltaTime, valid); }
};
struct Owner final {
    Fixture &f;
    PhysicalHandProp prop;
    Listener listener;
    explicit Owner(Fixture &fixture) : f(fixture), prop(f.system, f.player, .45F, 1.05F), listener(prop) {
        f.system.AddStepListener(&listener);
    }
    ~Owner() { f.system.RemoveStepListener(&listener); }
    bool begin() { return prop.begin(f.support, SubShapeID(), f.palm, Vec3::sAxisY()); }
    void tick() {
        require(f.system.Update(dt, 1, &f.allocator, &f.jobs) == EPhysicsUpdateError::None, "actual Jolt substep succeeds");
        prop.post_step(dt);
        const auto &s = prop.readback();
        require(std::isfinite(s.last_event_work_j) && s.last_event_work_j <= 1e-9 && s.command_work_j == 0 &&
                s.last_impulse_ns >= 0 && s.last_impulse_ns <= PhysicalHandProp::force_bound_n() * dt + 1e-5F &&
                s.peak_load_n <= PhysicalHandProp::force_bound_n() + .01F,
                "every admitted row is passive and respects one collision-step force cap");
    }
};
void static_contact() {
    scenario = "static_closing_surface";
    Fixture f; Owner owner(f);
    const auto before = f.state(f.player);
    require(owner.begin(), "closing actual exposed reachable top admits palm");
    owner.tick();
    const auto after = f.state(f.player);
    const Vec3 impulse = player_mass * (after.velocity - before.velocity);
    const auto &s = owner.prop.readback();
    const double cap = PhysicalHandProp::force_bound_n() * dt;
    require(std::abs(impulse.GetY() - cap) < 3e-5 && std::abs(impulse.GetX()) < 1e-6 &&
            std::abs(impulse.GetZ()) < 1e-6 && after.angular.Length() == 0,
            "closing palm delivers one capped upward impulse without horizontal or angular gift");
    const double measured_work = after.energy - before.energy;
    require(measured_work < 0 && std::abs(measured_work - s.last_event_work_j) < 5e-5,
            "independent kinetic-energy change matches nonpositive reported row work");
    std::cout << "HAND_PROP_STATIC impulse_ns=" << impulse.GetY() << " work_j=" << measured_work << '\n';
    scenario = "stationary_zero_preload";
    Fixture neutral(false, 25, .1F, Vec3::sZero()); Owner passive(neutral);
    require(passive.begin(), "stationary exposed palm may engage");
    passive.tick();
    require(neutral.state(neutral.player).energy == 0 && passive.prop.readback().last_impulse_ns == 0,
            "stationary passive row has no warm-start, preload or position correction");
}
double dynamic_contact(float mass, float half_height) {
    Fixture f(true, mass, half_height, Vec3(.7F, -.1F, .2F)); Owner owner(f);
    const auto player_before = f.state(f.player), support_before = f.state(f.support);
    const auto lever = Vec3(f.palm - support_before.center);
    const auto angular_axis = lever.Cross(Vec3::sAxisY());
    const double inverse_mass = 1.0 / player_mass + 1.0 / mass + angular_axis.Dot(support_before.inverse_inertia * angular_axis);
    const double expected = std::min(double(PhysicalHandProp::force_bound_n() * dt), .1 / inverse_mass);
    require(owner.begin(), "offset actual dynamic material point admits closing palm");
    owner.tick();
    const auto player_after = f.state(f.player), support_after = f.state(f.support);
    const Vec3 impulse = player_mass * (player_after.velocity - player_before.velocity);
    const Vec3 reaction = mass * (support_after.velocity - support_before.velocity);
    const Vec3 expected_angular = support_before.inverse_inertia * lever.Cross(-impulse);
    require(impulse.GetY() > 0 && std::abs(impulse.GetY() - expected) < 2e-5 &&
            (impulse + reaction).Length() < 2e-5 && impulse.Cross(Vec3::sAxisY()).Length() < 1e-6,
            "independent effective mass predicts equal/opposite normal momentum");
    require((support_after.angular - support_before.angular - expected_angular).Length() < 2e-5 &&
            player_after.angular.Length() == 0,
            "support receives real offset r-cross-J torque while player rotation remains locked");
    const double measured_work = player_after.energy + support_after.energy - player_before.energy - support_before.energy;
    require(measured_work <= 1e-7 && std::abs(measured_work - owner.prop.readback().last_event_work_j) < 5e-6,
            "both actual kinetic energies confirm passive reciprocal work");
    std::cout << "HAND_PROP_DYNAMIC mass_kg=" << mass << " half_height_m=" << half_height
              << " impulse_ns=" << impulse.GetY() << " angular_radps=" << support_after.angular.GetZ() << '\n';
    return impulse.GetY();
}
void dynamics() {
    scenario = "dynamic_mass_and_inertia";
    const double light = dynamic_contact(25, .1F), heavy = dynamic_contact(100, .1F), thick = dynamic_contact(25, .3F);
    require(heavy > light && thick > light && std::abs(heavy - light) > .1 && std::abs(thick - light) > .1,
            "real support mass and independent rotational inertia change outcome");
}
void lifecycle() {
    scenario = "opening_refusal";
    { Fixture f(false, 25, .1F, Vec3(0, 1, 0)); Owner o(f);
      require(!o.begin(), "opening palm cannot acquire a retained push"); o.tick();
      require(f.state(f.player).velocity.GetY() == 1 && !o.prop.active(), "refused contact leaves no ghost impulse"); }
    scenario = "active_separation";
    { Fixture f(false, 25, .1F, Vec3::sZero()); Owner o(f);
      // Explicit external initial loading, not work supplied by the palm.
      f.system.SetGravity(Vec3(0, 9.81F, 0));
      require(o.begin(), "stationary palm begins before externally caused opening");
      o.tick();
      require(!o.prop.active() && o.prop.readback().stop_reason == PhysicalHandProp::StopReason::Separated &&
              o.prop.readback().last_impulse_ns == 0 &&
              std::abs(f.state(f.player).velocity.GetY() - 9.81F * dt) < 1e-6,
              "actual opening motion clears active palm without a cached impulse");
      o.tick();
      require(std::abs(f.state(f.player).velocity.GetY() - 2 * 9.81F * dt) < 1e-6 &&
              f.system.GetConstraints().empty(), "separated row cannot replay force on a later collision step"); }
    scenario = "invalid_geometry";
    { Fixture f; Owner o(f);
      require(!o.prop.begin(f.support, SubShapeID(), f.palm, Vec3::sZero()) &&
              !o.prop.begin(f.support, SubShapeID(), RVec3(std::numeric_limits<float>::quiet_NaN(), 0, 0), Vec3::sAxisY()) &&
              !o.prop.begin(f.support, SubShapeID(), RVec3(10, .1F, 0), Vec3::sAxisY()),
              "invalid normal, nonfinite anchor and out-of-reach geometry refuse acquisition"); }
    scenario = "original_age_expiry";
    { Fixture f(false, 25, .1F, Vec3::sZero()); Owner o(f); require(o.begin(), "stationary age fixture begins");
      o.tick(); const auto generation = o.prop.readback().generation; const auto age = o.prop.readback().elapsed_seconds;
      require(!o.begin() && o.prop.readback().generation == generation && o.prop.readback().elapsed_seconds == age,
              "active replacement cannot renew original age or grip identity");
      unsigned ticks = 1; while (o.prop.active() && ticks < 94) { o.tick(); ++ticks; }
      require(!o.prop.active() && o.prop.readback().stop_reason == PhysicalHandProp::StopReason::Expired &&
              o.prop.readback().elapsed_seconds <= .250001 && ticks >= 90 && ticks <= 92,
              "fixed native clock expires unrenewed palm within original quarter second");
      for (unsigned i=0; i<4; ++i) o.tick();
      require(f.state(f.player).energy == 0 && o.prop.readback().last_load_n == 0,
              "expired row has no later preload or ghost force"); }
    for (const std::string invalidation : {"surface", "shape", "time", "clear"}) {
        scenario = invalidation.c_str(); Fixture f; Owner o(f); require(o.begin(), "invalidation fixture begins");
        if (invalidation == "surface") o.listener.valid = false;
        else if (invalidation == "shape")
            f.system.GetBodyInterface().SetShape(f.support, new BoxShape(Vec3(.3F, .12F, .2F), 0), false, EActivation::Activate);
        else if (invalidation == "time") o.prop.pre_step(std::numeric_limits<float>::quiet_NaN());
        else o.prop.clear();
        for (unsigned i=0; i<4; ++i) o.tick();
        require(!o.prop.active() && f.system.GetConstraints().empty() && o.prop.readback().last_load_n == 0 &&
                std::abs(f.state(f.player).velocity.GetY() + 2) < 1e-6,
                "invalidated row removes native constraint and never applies a later ghost impulse");
    }
    std::cout << "HAND_PROP_LIFECYCLE opening=1 separation=1 geometry=1 expiry=1 surface=1 shape=1 time=1 clear=1\n";
}
} // namespace
int main(int argc, char **argv) {
    const std::string mode = argc > 1 ? argv[1] : "static";
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    if (mode == "static") static_contact();
    else if (mode == "dynamic") dynamics();
    else if (mode == "lifecycle") lifecycle();
    else require(false, "known test mode required");
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
    std::cout << "PASS PHYSICAL_HAND_PROP mode=" << mode << " isolated_row=1 gameplay=unverified\n";
}
