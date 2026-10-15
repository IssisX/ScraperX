#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace JPH;
namespace {
void require(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
struct Layers final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 1; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer) const override { return BroadPhaseLayer(0); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "soft"; }
};
struct BroadFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; }
};
struct PairFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return false; }
};
struct Fixture {
    Layers layers;
    BroadFilter broad;
    PairFilter pairs;
    PhysicsSystem physics;
    TempAllocatorImpl allocator{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID id;
    Fixture(bool corrected, bool stretched) {
        physics.Init(16, 0, 16, 16, layers, broad, pairs);
        physics.SetGravity(Vec3::sZero());
        Ref<SoftBodySharedSettings> settings = new SoftBodySharedSettings;
        for (const Float3 p : {Float3(.1F, .2F, .03F), Float3(.3F, .2F, .03F), Float3(.1F, .4F, .03F)}) {
            SoftBodySharedSettings::Vertex v;
            v.mPosition = p;
            v.mInvMass = 1;
            settings->mVertices.push_back(v);
        }
        settings->mFaces.emplace_back(0, 1, 2);
        if (stretched) {
            settings->mEdgeConstraints.emplace_back(0, 1);
            settings->mEdgeConstraints.back().mRestLength = .1F;
        }
        settings->Optimize();
        SoftBodyCreationSettings creation(settings, RVec3(100, 100, 100), Quat::sIdentity(), 0);
        creation.mNumIterations = stretched ? 1 : 12;
        creation.mUpdatePosition = false;
        creation.mLinearDamping = 0;
        creation.mAllowSleeping = false;
        creation.mMaxLinearVelocity = 1000;
        id = physics.GetBodyInterface().CreateAndAddSoftBody(creation, EActivation::Activate);
        motion().SetPreserveIntegratedVelocity(corrected);
    }
    ~Fixture() {
        physics.GetBodyInterface().RemoveBody(id);
        physics.GetBodyInterface().DestroyBody(id);
    }
    SoftBodyMotionProperties &motion() {
        return *static_cast<SoftBodyMotionProperties *>(physics.GetBodyLockInterfaceNoLock().TryGetBody(id)->GetMotionProperties());
    }
    void step() {
        require(physics.Update(1.F / 360, 1, &allocator, &jobs) == EPhysicsUpdateError::None, "soft update capacity");
    }
};
void drift() {
    const Vec3 velocity(.17F, -.31F, .053F);
    float legacy_error = 0;
    for (bool corrected : {false, true}) {
        Fixture f(corrected, false);
        for (auto &v : f.motion().GetVertices()) v.mVelocity = velocity;
        for (int i = 0; i < 96; ++i) f.step();
        for (const auto &v : f.motion().GetVertices()) {
            if (corrected) require(v.mVelocity == velocity, "unconstrained integration preserves actual float velocity exactly");
            else legacy_error = std::max(legacy_error, (v.mVelocity - velocity).Length());
        }
    }
    require(legacy_error > 1.e-6F, "drift fixture distinguishes original position roundtrip");
}
void constraint_and_restore() {
    Fixture f(true, true);
    const auto before0 = f.motion().GetVertices()[0].mPosition;
    const auto before1 = f.motion().GetVertices()[1].mPosition;
    const Vec3 velocity(3.F, 4.F, 5.F);
    for (auto &v : f.motion().GetVertices()) v.mVelocity = velocity;
    const auto predicted0 = before0 + velocity * (1.F / 360);
    const auto predicted1 = before1 + velocity * (1.F / 360);
    f.step();
    const auto &vertices = f.motion().GetVertices();
    const auto correction0 = vertices[0].mPosition - predicted0;
    const auto correction1 = vertices[1].mPosition - predicted1;
    require(correction0.Length() > .01F && correction1.Length() > .01F, "real edge position constraint solves");
    require((vertices[0].mVelocity - velocity - correction0 * 360).Length() < 1.e-5F &&
            (vertices[1].mVelocity - velocity - correction1 * 360).Length() < 1.e-5F,
            "constraint correction contributes its actual velocity");
    require((vertices[0].mVelocity + vertices[1].mVelocity - 2 * velocity).Length() < .0005F, "edge correction preserves reciprocal momentum");
    StateRecorderImpl saved;
    f.motion().SaveState(saved);
    f.motion().SetPreserveIntegratedVelocity(false);
    saved.Rewind();
    f.motion().RestoreState(saved);
    require(f.motion().GetPreserveIntegratedVelocity(), "restore preserves explicitly selected reconstruction mode");
}
}
int main() {
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    int result = 0;
    try { drift(); constraint_and_restore(); std::cout << "PASS soft velocity drift/real edge correction/mode restore\n"; }
    catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; result = 1; }
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    return result;
}
