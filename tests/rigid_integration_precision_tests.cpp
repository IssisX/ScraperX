#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace {
using namespace JPH;
int checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) { std::cerr << "FAIL " << message << '\n'; std::exit(EXIT_FAILURE); }
}
class BroadLayers final : public BroadPhaseLayerInterface {
public:
    uint GetNumBroadPhaseLayers() const override { return 1; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer) const override { return BroadPhaseLayer(0); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "precision"; }
#endif
};
class BroadFilter final : public ObjectVsBroadPhaseLayerFilter {
public: bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; }
};
class PairFilter final : public ObjectLayerPairFilter {
public: bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return false; }
};
void opt_in(Body &body, bool enabled = true) {
#ifndef SCRAPERX_ACCURATE_INTEGRATION_UNPATCHED
    body.GetMotionProperties()->SetUseAccurateIntegration(enabled);
#else
    (void)body; (void)enabled;
#endif
}
struct World {
    BroadLayers broad;
    BroadFilter broad_filter;
    PairFilter pair_filter;
    PhysicsSystem physics;
    TempAllocatorImpl temp { 4 * 1024 * 1024 };
    JobSystemSingleThreaded jobs { 128 };
    World() {
        physics.Init(32, 0, 32, 32, broad, broad_filter, pair_filter);
        physics.SetGravity(Vec3::sZero());
    }
    Body &body(float height, bool accurate, EMotionType type = EMotionType::Dynamic,
               EAllowedDOFs dofs = EAllowedDOFs::All) {
        BodyCreationSettings settings(new BoxShape(Vec3(.25f, .5f, .75f)),
            RVec3(0, height, 0), Quat::sIdentity(), type, 0);
        settings.mLinearDamping = settings.mAngularDamping = 0;
        settings.mAllowSleeping = false;
        settings.mAllowedDOFs = dofs;
        Body *result = physics.GetBodyInterface().CreateBody(settings);
        require(result != nullptr, "native body created");
#ifndef SCRAPERX_ACCURATE_INTEGRATION_UNPATCHED
        require(!result->GetMotionProperties()->GetUseAccurateIntegration(), "precision default is off");
#endif
        opt_in(*result, accurate);
        physics.GetBodyInterface().AddBody(result->GetID(), EActivation::Activate);
        return *result;
    }
    void ticks(int count) {
        for (int i = 0; i < count; ++i)
            require(physics.Update(1.0f/90.0f, 4, &temp, &jobs) == EPhysicsUpdateError::None,
                    "native four-substep update succeeded");
    }
};
void translation(float height, EMotionType type) {
    World world;
    Body &accurate = world.body(height, true, type);
    Body &legacy = world.body(height, false, type);
    constexpr float speed = .00523685f;
    accurate.SetLinearVelocity(Vec3(0, speed, 0));
    legacy.SetLinearVelocity(Vec3(0, speed, 0));
    world.ticks(90);
    const double expected = double(height) + double(speed);
    const double ulp = double(std::nextafter(height, INFINITY)) - height;
    const double actual = accurate.GetCenterOfMassPosition().GetY();
    std::cout << "translation height=" << height << " type=" << int(type)
              << " actual=" << actual << " expected=" << expected << " ulp=" << ulp << '\n';
    require(std::abs(actual - expected) <= .51 * ulp, "slow translation retains sub-ULP displacement");
    require(legacy.GetCenterOfMassPosition().GetY() == height, "default path preserves rounded-away sentinel");
    require(accurate.GetLinearVelocity().GetY() == speed, "precision does not change velocity");
    accurate.SetLinearVelocity(Vec3(0, -speed, 0));
    world.ticks(90);
    require(accurate.GetCenterOfMassPosition().GetY() == height, "translation reversal returns to origin");
}
void tiny_rotation() {
    World world;
    Body &accurate = world.body(473, true);
    Body &legacy = world.body(473, false);
    constexpr float omega = .000314288f;
    accurate.SetAngularVelocity(Vec3(0, 0, omega));
    legacy.SetAngularVelocity(Vec3(0, 0, omega));
    world.ticks(90);
    const double angle = 2 * std::atan2(double(accurate.GetRotation().GetZ()), double(accurate.GetRotation().GetW()));
    std::cout << "rotation actual=" << angle << " expected=" << omega << '\n';
    require(std::abs(angle - omega) < 2e-9, "tiny angular steps integrate analytic angle");
    require(legacy.GetRotation() == Quat::sIdentity(), "default tiny-angle cutoff remains unchanged");
    require(accurate.GetAngularVelocity().GetZ() == omega, "precision does not change angular velocity");
    accurate.SetAngularVelocity(Vec3(0, 0, -omega));
    world.ticks(90);
    require(std::abs(accurate.GetRotation().GetZ()) < 2e-10f, "angular reversal returns to identity");
}
void residual_lifecycle() {
    World world;
    Body &body = world.body(1600, true);
    const float ulp = std::nextafter(1600.0f, INFINITY) - 1600.0f;
    const Vec3 quarter(0, .25f * ulp, 0);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "quarter ULP stays in residual");
    StateRecorderImpl saved;
    body.SaveState(saved);
    body.AddPositionStep(quarter);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600 + ulp, "three quarter ULP advances rounded pose");
    saved.Rewind();
    body.RestoreState(saved);
    body.AddPositionStep(quarter);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600 + ulp, "replay restores fractional displacement");
    body.SetPositionAndRotationInternal(RVec3(0,1600,0), Quat::sIdentity());
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "teleport clears prior residual");
    opt_in(body, false);
    opt_in(body);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "toggle clears residual");
    body.SubPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "add/sub position correction cancels residual");
    body.AddPositionStep(quarter);
    body.SetShapeInternal(new BoxShape(Vec3(.5f,.5f,.5f)), false);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "shape change clears residual");
    body.GetMotionProperties()->SetMassProperties(EAllowedDOFs::TranslationY, body.GetShape()->GetMassProperties());
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "DOF change clears residual");
    body.SetMotionType(EMotionType::Kinematic);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "motion type change clears residual");
    body.SetPositionAndRotationInternal(RVec3(0,1600,0), Quat::sIdentity());
    body.SubPositionStep(quarter);
    body.SubPositionStep(quarter);
    body.SubPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600 - ulp, "subtraction retains negative fractional corrections");
    body.AddPositionStep(quarter);
    body.AddPositionStep(quarter);
    body.AddPositionStep(quarter);
    require(body.GetCenterOfMassPosition().GetY() == 1600, "positive corrections reverse accumulated subtraction");
    body.AddRotationStep(Vec3(0,0,1e-7f));
    body.SubRotationStep(Vec3(0,0,1e-7f));
    require(std::abs(body.GetRotation().GetZ()) < 1e-14f, "tiny solver rotation add/sub cancels");
    opt_in(body, false);
    StateRecorderImpl disabled;
    body.SaveState(disabled);
    opt_in(body);
    disabled.Rewind(); body.RestoreState(disabled);
#ifndef SCRAPERX_ACCURATE_INTEGRATION_UNPATCHED
    require(!body.GetMotionProperties()->GetUseAccurateIntegration(), "replay restores opt-in flag");
#endif
}
void locked_axes() {
    World world;
    Body &body = world.body(473, true, EMotionType::Dynamic, EAllowedDOFs::TranslationY);
    body.AddPositionStep(Vec3(1, .001f, 1));
    require(body.GetCenterOfMassPosition().GetX() == 0 && body.GetCenterOfMassPosition().GetZ() == 0,
            "compensation respects locked translation axes");
    body.SetIsSensor(true);
    body.SetLinearVelocity(Vec3(0,.00523685f,0));
    const double start = body.GetCenterOfMassPosition().GetY();
    world.ticks(90);
    require(std::abs(double(body.GetCenterOfMassPosition().GetY()) - start - .00523685) < 3.1e-5,
            "sensor uses same native precision owner");
}
} // namespace
int main() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory;
    JPH::RegisterTypes();
    std::cout << std::setprecision(12);
    translation(473, JPH::EMotionType::Dynamic);
    translation(1600, JPH::EMotionType::Dynamic);
    translation(1600, JPH::EMotionType::Kinematic);
    tiny_rotation(); residual_lifecycle(); locked_axes();
    JPH::UnregisterTypes(); delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr;
    std::cout << "PASS rigid integration precision checks=" << checks << '\n';
}
