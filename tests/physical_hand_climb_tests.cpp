#include <Jolt/Jolt.h>
#include "sim/physical_hand_climb.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

// Native coupling fixtures, not ordinary-input acquisition, traversal or device proof.
// Removing coupling fails sag/load/recoil; reversing targets fails ascent;
// unbounded motors/commands fail catches/budget; copied release velocity fails momentum.
namespace {
using namespace JPH;
using scraperx::sim::PhysicalHandClimb;
constexpr float h = 1.0F / 90.0F;
constexpr double weight = 85.0 * double(9.81F);
void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "hand-test"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Contacts final : ContactListener {
    unsigned count = 0;
    void OnContactAdded(const Body &, const Body &, const ContactManifold &, ContactSettings &) override { ++count; }
    void OnContactPersisted(const Body &, const Body &, const ContactManifold &, ContactSettings &) override { ++count; }
};
struct ForceObserver final : PhysicsStepListener {
    PhysicalHandClimb &hands;
    explicit ForceObserver(PhysicalHandClimb &c): hands(c) {}
    void OnStep(const PhysicsStepListenerContext &c) override { hands.post_step(c.mDeltaTime); }
};
struct World {
    BP bp; BV bv; LP lp;
    PhysicsSystem system;
    TempAllocatorImpl temporary { 16 * 1024 * 1024 };
    JobSystemSingleThreaded jobs { 1024 };
    Contacts contacts;
    std::vector<BodyID> ids;
    BodyID player, support;
    explicit World(bool dynamic_support = false, Vec3 velocity = Vec3::sZero()) {
        system.Init(64, 0, 128, 256, bp, bv, lp);
        system.SetGravity(Vec3(0, -9.81F, 0));
        system.SetContactListener(&contacts);
        check(system.GetPhysicsSettings().mNumVelocitySteps == 10 &&
              system.GetPhysicsSettings().mNumPositionSteps == 2, "unexpected shipping global solver defaults");
        BodyCreationSettings p(new CapsuleShape(0.55F, 0.35F), RVec3(0, 4.3, -0.2),
                               Quat::sIdentity(), EMotionType::Dynamic, 1);
        p.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        p.mMassPropertiesOverride.mMass = 85.0F;
        p.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        p.mLinearDamping = p.mAngularDamping = 0;
        p.mAllowSleeping = false; p.mFriction = p.mRestitution = 0;
        p.mMotionQuality = EMotionQuality::LinearCast;
        p.mLinearVelocity = velocity;
        player = add(p);
        BodyCreationSettings s(new BoxShape(Vec3(0.5F, 0.1F, 0.1F)), RVec3(0, 5.1, 0.65),
                               Quat::sIdentity(), dynamic_support ? EMotionType::Dynamic : EMotionType::Static,
                               dynamic_support ? 1 : 0);
        s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass = 25.0F;
        s.mLinearDamping = s.mAngularDamping = 0; s.mAllowSleeping = false;
        support = add(s);
        // Grip lies on the visible support front/bottom edge, 0.4m clear of
        // the capsule. There is no platform/contact capable of faking a hang.
    }
    ~World() {
        system.SetContactListener(nullptr);
        check(system.GetConstraints().empty(), "constraint leaked past hand-owner cleanup");
        for (auto id : ids) { system.GetBodyInterface().RemoveBody(id); system.GetBodyInterface().DestroyBody(id); }
    }
    BodyID add(const BodyCreationSettings &settings) {
        auto id = system.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        check(!id.IsInvalid(), "native body allocation failed"); ids.push_back(id); return id;
    }
    Vec3 velocity(BodyID id) { return system.GetBodyInterface().GetLinearVelocity(id); }
    RVec3 position(BodyID id) { return system.GetBodyInterface().GetPosition(id); }
    void tick(PhysicalHandClimb &hands, unsigned frames) {
        ForceObserver observer(hands);
        system.AddStepListener(&observer);
        for (unsigned i = 0; i < frames; ++i) {
            check(system.Update(h, 4, &temporary, &jobs) == EPhysicsUpdateError::None, "native update error");
            hands.post_step(h / 4);
            check(finite(position(player)) && finite(velocity(player)) && finite(position(support)) &&
                  finite(velocity(support)), "nonfinite constrained body state");
        }
        system.RemoveStepListener(&observer);
    }
    void acquire(PhysicalHandClimb &hands) {
        check(hands.attach(0, support, RVec3(-0.2, 5.0, 0.55)), "first physical hand was not acquired");
        check(hands.attach(1, support, RVec3(0.2, 5.0, 0.55)), "second physical hand was not acquired");
    }
};
void static_load_and_release() {
    World w;
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands);
    check(hands.active() && hands.attached(0) && hands.attached(1), "hand state disagrees with acquisition");
    const double initial_y = w.position(w.player).GetY();
    w.tick(hands, 360);
    const double sag = initial_y - w.position(w.player).GetY();
    const double reaction = hands.hand_force(0).GetY() + hands.hand_force(1).GetY();
    check(std::abs(sag - weight / 10000.0) < 0.002, "two-hand gravity-on compliance differs from mg/(2k)");
    check(std::abs(reaction - weight) < 2.0, "hand reaction does not support actual player weight");
    check(w.velocity(w.player).Length() < 0.002F, "hanging equilibrium still moving");
    {
        BodyLockRead lock(w.system.GetBodyLockInterface(), w.player);
        check(lock.GetBody().GetMotionProperties()->GetGravityFactor() == 1.0F, "hand acquisition disabled gravity");
        check(std::abs(1.0 / lock.GetBody().GetMotionProperties()->GetInverseMass() - 85) < 0.001, "hand acquisition changed mass");
    }
    const auto before = w.velocity(w.player);
    hands.detach(0);
    check((w.velocity(w.player) - before).LengthSq() == 0, "one-hand detach changed momentum");
    check(!hands.attached(0) && hands.attached(1), "detach removed wrong hand");
    w.tick(hands, 360);
    std::cout << "one-hand at4s sag=" << initial_y - w.position(w.player).GetY() << " reaction=" << hands.hand_force(1).GetY()
              << " vy=" << w.velocity(w.player).GetY() << '\n';
    // The specified per-axis cap is 866N: only 32N above mg. Detachment's
    // downward transient is still force-saturated at4s; wait for equilibrium
    // without changing the chosen actuator or relaxing static balance checks.
    w.tick(hands, 1080);
    const double one_sag = initial_y - w.position(w.player).GetY();
    check(std::abs(one_sag - weight / 5000.0) < 0.002, "remaining hand fails real weight transfer");
    check(std::abs(hands.hand_force(1).GetY() - weight) < 2.0, "one-hand reaction is wrong");
    const auto departure = w.velocity(w.player);
    hands.clear();
    check((w.velocity(w.player) - departure).LengthSq() == 0, "release copied support velocity or added boost");
    check(!hands.active() && w.system.GetConstraints().empty(), "clear left a hand constraint");
    w.tick(hands, 9);
    check(std::abs(w.velocity(w.player).GetY() - departure.GetY() + 9.81F * 9 * h) < 0.002, "released body is not freely falling");
    check(w.contacts.count == 0, "fixture contact could have faked hand support");
    std::cout << "static sag=" << sag << " reaction=" << reaction << " one_sag=" << one_sag << " peak_force=" << hands.peak_hand_force_n() << '\n';
}
void command_sign_and_budget() {
    World w;
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands); w.tick(hands, 360);
    const double y = w.position(w.player).GetY();
    hands.advance_targets(Vec3(0, 0.2F, 0), 0.2F);
    check(hands.last_command_work_bound_j() > 599.9 && hands.last_command_work_bound_j() <= 600.001, "two-hand command debit is not shared Fmax-distance budget");
    w.tick(hands, 270);
    check(w.position(w.player).GetY() - y > 0.195 && w.position(w.player).GetY() - y < 0.205, "positive climb command moved player in wrong direction");
    const double previous = hands.command_work_bound_j();
    hands.advance_targets(Vec3(100, 30, -70), h);
    check(hands.last_command_work_bound_j() > 33.32 && hands.last_command_work_bound_j() <= 3000.0 * h + 0.0001, "command exceeded 3000W aggregate bound");
    check(std::abs(hands.command_work_bound_j() - previous - hands.last_command_work_bound_j()) < 0.0001, "cumulative command debit missing");
    const double ledger = hands.command_work_bound_j();
    hands.advance_targets(Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0), h);
    check(hands.last_command_work_bound_j() == 0 && hands.command_work_bound_j() == ledger, "invalid command entered ledger");
    hands.detach(0); hands.advance_targets(Vec3(100, 30, -70), h);
    check(hands.last_command_work_bound_j() > 33.32 && hands.last_command_work_bound_j() <= 3000.0 * h + 0.0001, "one-hand budget unexpectedly doubled or halved");
    hands.clear(); hands.advance_targets(Vec3(1, 1, 1), h);
    check(hands.last_command_work_bound_j() == 0, "unattached command consumed work budget");
    std::cout << "command ascent=" << w.position(w.player).GetY() - y << " cumulative_bound=" << hands.command_work_bound_j() << '\n';
}
void dynamic_recoil() {
    World w(true, Vec3(2, 0, 0));
    PhysicalHandClimb hands(w.system, w.player);
    const auto pv = w.velocity(w.player), sv = w.velocity(w.support);
    w.acquire(hands);
    check((w.velocity(w.player) - pv).LengthSq() == 0 && (w.velocity(w.support) - sv).LengthSq() == 0, "catch changed actual initial velocities");
    w.tick(hands, 45);
    const auto player_v = w.velocity(w.player), support_v = w.velocity(w.support);
    check(support_v.GetX() > 0.2F && player_v.GetX() < 1.9F, "free support did not receive reciprocal recoil");
    check(std::abs(85.0 * player_v.GetX() + 25.0 * support_v.GetX() - 170.0) < 0.05, "hand forces violated horizontal momentum balance");
    check(w.contacts.count == 0, "dynamic recoil fixture had external contact");
    hands.clear();
    check((w.velocity(w.player) - player_v).LengthSq() == 0 && (w.velocity(w.support) - support_v).LengthSq() == 0, "dynamic departure replaced body velocities");
    std::cout << "recoil player_vx=" << player_v.GetX() << " support_vx=" << support_v.GetX() << " peak_force=" << hands.peak_hand_force_n() << '\n';
}
void finite_landing_catch() {
    // An oblique landing saturates all three axes and can reject a per-axis
    // 1500N error that a purely vertical catch would not distinguish.
    World w(false, Vec3(-6, -6, -6));
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands);
    check((w.velocity(w.player) - Vec3(-6, -6, -6)).LengthSq() == 0, "catch froze incoming landing velocity");
    double minimum_y = w.position(w.player).GetY();
    for (unsigned i = 0; i < 540; ++i) { w.tick(hands, 1); minimum_y = std::min(minimum_y, double(w.position(w.player).GetY())); }
    check(minimum_y < 4.0 && minimum_y > 1.0, "catch was rigid snap or failed to arrest falling player");
    check(w.velocity(w.player).Length() < 0.005F, "finite catch did not settle");
    check(hands.peak_hand_force_n() > 1490 && hands.peak_hand_force_n() <= PhysicalHandClimb::force_bound_n() + 0.05F, "catch force was absent or exceeded vector bound");
    check(w.contacts.count == 0, "catch fixture had external collision");
    std::cout << "catch minimum_y=" << minimum_y << " peak_force=" << hands.peak_hand_force_n() << '\n';
}
void invalid_holds_and_cleanup() {
    World w;
    {
        PhysicalHandClimb hands(w.system, w.player);
        check(!hands.attach(2, w.support, RVec3::sZero()), "invalid hand index accepted");
        check(!hands.attach(0, BodyID(), RVec3::sZero()), "invalid body accepted");
        check(!hands.attach(0, w.player, RVec3::sZero()), "self grip accepted");
        w.acquire(hands);
        Ref<SoftBodySharedSettings> mesh = new SoftBodySharedSettings;
        for (auto p : { Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0) }) {
            SoftBodySharedSettings::Vertex v; p.StoreFloat3(&v.mPosition); mesh->mVertices.push_back(v);
        }
        SoftBodySharedSettings::Face f; f.mVertex[0] = 0; f.mVertex[1] = 1; f.mVertex[2] = 2; mesh->AddFace(f);
        const SoftBodySharedSettings::VertexAttributes material(1e-5F, 1e-5F, 1e-5F);
        mesh->CreateConstraints(&material, 1); mesh->Optimize();
        SoftBodyCreationSettings settings(mesh, RVec3(10, 10, 0), Quat::sIdentity(), 1);
        const auto soft = w.system.GetBodyInterface().CreateAndAddSoftBody(settings, EActivation::Activate);
        check(!soft.IsInvalid(), "soft-body rejection fixture failed allocation"); w.ids.push_back(soft);
        check(!hands.attach(0, soft, RVec3(10, 10, 0)), "rigid-hand adapter accepted soft body");
        check(hands.attached(0) && w.system.GetConstraints().size() == 2, "invalid replacement destroyed good grip");
        check(!hands.attach(0, w.support, RVec3(std::numeric_limits<float>::infinity(), 0, 0)), "nonfinite grip accepted");
    }
    check(w.system.GetConstraints().empty(), "destructor leaked native constraints");
}
}
int main() {
    JPH::RegisterDefaultAllocator(); JPH::Factory::sInstance = new JPH::Factory; JPH::RegisterTypes();
    unsigned failures = 0;
    for (const auto &test : std::vector<std::pair<const char *, void (*)()>> {
        { "static_load_and_release", static_load_and_release }, { "command_sign_and_budget", command_sign_and_budget },
        { "dynamic_recoil", dynamic_recoil }, { "finite_landing_catch", finite_landing_catch },
        { "invalid_holds_and_cleanup", invalid_holds_and_cleanup } }) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception &e) { ++failures; std::cerr << "FAIL " << test.first << ": " << e.what() << '\n'; }
    }
    JPH::UnregisterTypes(); delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr;
    std::cout << "physical hand checks: " << 5 - failures << "/5\n";
    return failures ? 1 : 0;
}
