#include "sim/simulation/queries.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using namespace JPH;
using scraperx::sim::detail::walking_top_face_normal;
void require(bool value, const char *message) {
    if (value) return;
    std::cerr << "FAIL WALKING_CONTACT_NORMAL " << message << '\n';
    std::exit(EXIT_FAILURE);
}
struct BroadPhase final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 1; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer) const override { return BroadPhaseLayer(0); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "walking_contact"; }
#endif
};
struct BroadPhaseFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return true; }
};
struct LayerFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return true; }
};
struct Fixture final {
    BroadPhase broad_phase;
    BroadPhaseFilter broad_phase_filter;
    LayerFilter layer_filter;
    PhysicsSystem system;
    Fixture() { system.Init(16, 0, 16, 16, broad_phase, broad_phase_filter, layer_filter); }
    Body &body(const Shape *shape, RVec3 position = RVec3::sZero(), Quat rotation = Quat::sIdentity()) {
        auto *result = system.GetBodyInterface().CreateBody(
            BodyCreationSettings(shape, position, rotation, EMotionType::Static, 0));
        require(result != nullptr, "actual Jolt body creation");
        return *result;
    }
};
void accepted(const Body &body, SubShapeID sub_shape, RVec3 point, Vec3 expected) {
    const auto original_position = body.GetCenterOfMassPosition();
    const auto original_rotation = body.GetRotation();
    Vec3 normal(1, 2, 3);
    require(walking_top_face_normal(body, sub_shape, point, normal), "actual sharp top point accepted");
    require((normal - expected).Length() < 1.0e-6F, "normal belongs to the exact leaf top face");
    require(body.GetCenterOfMassPosition() == original_position && body.GetRotation() == original_rotation,
        "geometry query does not mutate body pose");
}
void rejected(const Body &body, SubShapeID sub_shape, RVec3 point) {
    const Vec3 sentinel(1, 2, 3);
    Vec3 normal = sentinel;
    require(!walking_top_face_normal(body, sub_shape, point, normal), "invalid top point rejected");
    require(normal == sentinel, "failure preserves caller normal");
}
void cases() {
    Fixture fixture;
    RefConst<Shape> sharp = new BoxShape(Vec3(1, .25F, 2), 0);
    auto &box = fixture.body(sharp);
    for (const auto point : {RVec3(0, .25, 0), RVec3(1, .25, 0), RVec3(1, .25, 2)})
        accepted(box, SubShapeID(), point, Vec3::sAxisY());
    rejected(box, SubShapeID(), RVec3(1, .24, 0));
    rejected(box, SubShapeID(), RVec3(0, .24, 0));
    rejected(box, SubShapeID(), RVec3(1.01, .25, 0));
    rejected(box, SubShapeID(), RVec3(0, .25, 2.01));
    rejected(box, SubShapeID(), RVec3(0, -.25, 0));
    RefConst<Shape> rounded = new BoxShape(Vec3(1, .25F, 2), .05F);
    rejected(fixture.body(rounded), SubShapeID(), RVec3(0, .25, 0));
    RefConst<Shape> sphere = new SphereShape(.25F);
    rejected(fixture.body(sphere), SubShapeID(), RVec3(0, .25, 0));
    RefConst<Shape> scaled = new ScaledShape(sharp, Vec3(2, 1, 1));
    rejected(fixture.body(scaled), SubShapeID(), RVec3(0, .25, 0));
    const auto steep = Quat::sRotation(Vec3::sAxisZ(), 1.1F);
    rejected(fixture.body(sharp, RVec3::sZero(), steep), SubShapeID(), RVec3(steep * Vec3(0, .25F, 0)));

    // Two differently rotated leaves ensure the subshape, rather than the
    // parent body's axes or another compound member, supplies the normal.
    StaticCompoundShapeSettings compound_settings;
    compound_settings.AddShape(Vec3(-4, 0, 0), Quat::sIdentity(), sharp);
    const Vec3 offset(3, .5F, -2);
    const auto leaf_rotation = Quat::sRotation(Vec3::sAxisX(), .2F);
    compound_settings.AddShape(offset, leaf_rotation, sharp);
    const auto result = compound_settings.Create();
    require(!result.HasError(), "actual compound shape creation");
    const auto *compound = static_cast<const StaticCompoundShape *>(result.Get().GetPtr());
    const auto sub_shape = compound->GetSubShapeIDFromIndex(1, SubShapeIDCreator()).GetID();
    const RVec3 translation(120, 1500, -90);
    const auto rotation = Quat::sRotation(Vec3::sAxisZ(), .3F);
    auto &body = fixture.body(compound, translation, rotation);
    const auto world_point = [&](Vec3 local) {
        return translation + RVec3(rotation * (offset + leaf_rotation * local));
    };
    const auto expected = rotation * leaf_rotation * Vec3::sAxisY();
    accepted(body, sub_shape, world_point(Vec3(0, .25F, 0)), expected);
    accepted(body, sub_shape, world_point(Vec3(1, .25F, 2)), expected);
    rejected(body, sub_shape, world_point(Vec3(1, .24F, 0)));
    rejected(body, sub_shape, world_point(Vec3(1.01F, .25F, 0)));
    const auto other_leaf = compound->GetSubShapeIDFromIndex(0, SubShapeIDCreator()).GetID();
    rejected(body, other_leaf, world_point(Vec3(0, .25F, 0)));
    fixture.system.GetBodyInterface().SetPositionAndRotation(body.GetID(), translation + RVec3(3, 2, 1),
        rotation, EActivation::DontActivate);
    accepted(body, sub_shape, world_point(Vec3(0, .25F, 0)) + RVec3(3, 2, 1), expected);
    std::cout << "PASS WALKING_CONTACT_NORMAL sharp top/edge/corner, exclusions, exact compound leaf and moved pose\n";
}
}
int main() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory;
    JPH::RegisterTypes();
    cases();
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}
