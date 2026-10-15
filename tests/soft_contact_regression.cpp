#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyManifold.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using namespace JPH;
struct BP : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "contact"; }
};
struct BV : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LP : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Contact : SoftBodyContactListener {
    BodyID expected;
    unsigned calls = 0;
    Vec3 normal = Vec3::sZero();
    void OnSoftBodyContactAdded(const Body &, const SoftBodyManifold &manifold) override {
        const auto &vertex = manifold.GetVertices()[0];
        if (manifold.HasContact(vertex) && manifold.GetContactBodyID(vertex) == expected) {
            ++calls;
            normal = -manifold.GetContactNormal(vertex);
        }
    }
};

bool run(bool corrected, bool dynamic, float previous_normal, float current_normal, float restitution,
         float friction = 0, float tangent = 0, bool history = false) {
    constexpr float dt = 1.0F / 90, mass = 85, tolerance = 2e-5F;
    BP bp; BV bv; LP lp;
    PhysicsSystem system;
    system.Init(16, 0, 64, 64, bp, bv, lp);
    system.SetGravity(Vec3::sZero());
    Contact contact;
    system.SetSoftBodyContactListener(&contact);
    auto &bodies = system.GetBodyInterface();
    BodyCreationSettings rigid(new BoxShape(Vec3(1, .5F, 1)), RVec3::sZero(), Quat::sIdentity(),
                               dynamic ? EMotionType::Dynamic : EMotionType::Static, dynamic ? 1 : 0);
    rigid.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    rigid.mMassPropertiesOverride.mMass = mass;
    rigid.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
    rigid.mLinearDamping = rigid.mAngularDamping = 0;
    rigid.mFriction = friction;
    rigid.mRestitution = restitution;
    const BodyID receiver = bodies.CreateAndAddBody(rigid, dynamic ? EActivation::Activate : EActivation::DontActivate);
    contact.expected = receiver;

    Ref<SoftBodySharedSettings> settings = new SoftBodySharedSettings;
    settings->mVertices.emplace_back(Float3(0, 0, 0), Float3(tangent, previous_normal, 0), 1);
    if (history) {
        // A real long-range attachment moves the predicted vertex to current_normal * dt.
        // Integration retains previous_normal for restitution. No callback edits solver data.
        settings->mVertices.emplace_back(Float3(0, .5F + current_normal * dt, 0), Float3(0, 0, 0), 0);
        settings->mLRAConstraints.emplace_back(1, 0, .5F);
    }
    settings->Optimize();
    SoftBodyCreationSettings soft(settings, RVec3(0, .48, 0), Quat::sIdentity(), 1);
    soft.mNumIterations = 1;
    soft.mLinearDamping = soft.mGravityFactor = 0;
    soft.mFriction = friction;
    soft.mRestitution = restitution;
    soft.mUpdatePosition = soft.mAllowSleeping = false;
    const BodyID particle = bodies.CreateAndAddSoftBody(soft, EActivation::Activate);
    Vec3 after;
    float height;
    {
        // Serialized CustomUpdate takes its own receiver locks.
        BodyLockWrite lock(system.GetBodyLockInterfaceNoLock(), particle);
        auto *motion = static_cast<SoftBodyMotionProperties *>(lock.GetBody().GetMotionProperties());
        motion->SetPreserveIntegratedVelocity(corrected);
        motion->CustomUpdate(dt, lock.GetBody(), system);
        after = motion->GetVertex(0).mVelocity;
        height = (lock.GetBody().GetCenterOfMassTransform() * motion->GetVertex(0).mPosition).GetY();
    }
    const Vec3 receiver_velocity = bodies.GetLinearVelocity(receiver);
    const float relative_normal = (after - receiver_velocity).Dot(contact.normal);
    // With e=.5 and previous=-1 the target is +.5. If XPBD already exceeds that
    // target, a unilateral contact must retain its larger separating speed.
    const float expected_normal = previous_normal < 0
        ? std::max(current_normal, -restitution * previous_normal) : current_normal;
    const Vec3 before_contact(tangent, current_normal, 0);
    const float momentum_error = (after + mass * receiver_velocity - before_contact).Length();
    const float energy_before = .5F * before_contact.LengthSq();
    const float energy_after = .5F * (after.LengthSq() + mass * receiver_velocity.LengthSq());
    bool pass = contact.calls == 1 && contact.normal.GetY() > .99F && height >= .49999F
        && std::abs(relative_normal - expected_normal) < tolerance;
    if (dynamic) pass &= momentum_error < tolerance;
    if (!history) pass &= energy_after <= energy_before + tolerance;
    const bool separating_without_impulse = current_normal > 0 && expected_normal == current_normal;
    if (separating_without_impulse)
        pass &= (after - before_contact).Length() < tolerance && receiver_velocity.Length() < tolerance;
    if (current_normal < 0 && friction > 0)
        pass &= after.GetX() < tangent - .01F && energy_after < energy_before - .01F;
    std::cout << "SOFT_CONTACT corrected=" << corrected << " dynamic=" << dynamic << " history=" << history
        << " previous_normal=" << previous_normal << " current_normal=" << current_normal
        << " restitution=" << restitution << " friction=" << friction
        << " contacts=" << contact.calls << " relative_normal=" << relative_normal
        << " expected_normal=" << expected_normal << " vertex_y=" << after.GetY()
        << " receiver_y=" << receiver_velocity.GetY() << " tangent=" << after.GetX()
        << " momentum_error=" << momentum_error << " energy_before=" << energy_before
        << " energy_after=" << energy_after << " height=" << height
        << " verdict=" << (pass ? "PASS" : "FAIL") << '\n';
    bodies.RemoveBody(particle); bodies.DestroyBody(particle);
    bodies.RemoveBody(receiver); bodies.DestroyBody(receiver);
    return pass;
}
}

// The existing model-test executable owns allocator/type/factory lifetime.
bool soft_contact_regression() {
    bool pass = true;
    for (bool corrected : {false, true}) for (bool dynamic : {true, false}) {
        pass = run(corrected, dynamic, 1, 1, 0) && pass;
        pass = run(corrected, dynamic, -1, -1, 0) && pass;
        pass = run(corrected, dynamic, 1, 1, 0, .8F, 2) && pass;
        pass = run(corrected, dynamic, -1, -1, 0, .8F, 2) && pass;
        pass = run(corrected, dynamic, -1, -1, .5F) && pass;
        pass = run(corrected, dynamic, -1, .1F, .5F, 0, 0, true) && pass;
        pass = run(corrected, dynamic, -1, 1, .5F, 0, 0, true) && pass;
    }
    return pass;
}
