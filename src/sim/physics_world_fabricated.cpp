// Live fabrication: an object made outside the simulation becomes a real body
// at run time, built from its hulls, and is saved and restored with them.
// Members of Simulation::PhysicsWorld (physics_world.hpp); the hull file is
// read by sim/fabricated.cpp.
//
// A fabricated object takes its entity from a range of its own
// (fabricated::kEntityBase on, above every other in the world) and its kit
// body is built after every other body, the kit's last. The solver orders
// its contacts by entity (03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md),
// so one delivered anywhere leaves every other band's simulation as it was,
// bit for bit (native group "fabricated").

#include "sim/physics_world.hpp"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

namespace scraperx::sim {

static_assert(kit::kFabricatedEntityBase == fabricated::kEntityBase &&
                  kit::kFabricatedEntityLimit == fabricated::kEntityLimit,
              "the kit's and the fabrication's entity ranges are one range");

namespace {

// Set down 4 mm clear of what it is set on, as loose stock is, so nothing
// starts in contact.
constexpr float kSetClearanceM = 0.004F;
// Where in the world an object may be put: the basin and the tower.
constexpr double kWorldHalfM = 2000.0;
constexpr double kWorldLowM = -100.0;
constexpr double kWorldHighM = 2000.0;
// A save's fabricated objects follow its grips under this mark ("FAB1").
constexpr std::uint32_t kSaveMark = 0x31424146U;

// What a delivery may not be put inside: anything solid (a sensor is not).
class Solid final : public JPH::BodyFilter {
public:
    [[nodiscard]] bool ShouldCollideLocked(const JPH::Body &body) const override { return !body.IsSensor(); }
};

[[nodiscard]] bool in_world(const double x, const double y, const double z) noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::abs(x) <= kWorldHalfM &&
           std::abs(z) <= kWorldHalfM && y >= kWorldLowM && y <= kWorldHighM;
}

} // namespace

kit::BodyIndex Simulation::PhysicsWorld::build_fabricated(const Fabricated &object, const JPH::RVec3 origin,
                                                          const JPH::Quat rotation) {
    const fabricated::MaterialSpec &material = fabricated::material_spec(object.spec.material);
    const kit::BodyIndex body =
        kit_->add_shape_body(object.entity, object.shape.GetPtr(), origin, rotation, material.friction,
                             material.restitution);
    if (!body.valid()) {
        return body;
    }
    // Lifted by the top of its box, over its center of mass (the handle is in
    // the center of mass's frame), so it hangs level from the hands.
    const JPH::Vec3 center = object.shape->GetCenterOfMass();
    kit_->set_carry(body, kit::CarryKind::Load, JPH::Vec3(0.0F, object.spec.half_extent[1] - center.GetY(), 0.0F));
    kit_->allow_sleeping(body);
    kit_->set_continuous_collision(body);
    impacts_->track(kit_->body_id(body), object.entity, material.sound, object.mass_kg);
    return body;
}

bool Simulation::PhysicsWorld::fabricated_overlaps(const JPH::Shape *shape, const JPH::RVec3 origin,
                                                   const JPH::Quat rotation) const {
    JPH::CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> hit;
    const Solid solid;
    physics_system_.GetNarrowPhaseQuery().CollideShape(
        shape, JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sRotationTranslation(rotation, origin + rotation * shape->GetCenterOfMass()), settings, origin,
        hit, physics_system_.GetDefaultBroadPhaseLayerFilter(object_layers::kMoving),
        physics_system_.GetDefaultLayerFilter(object_layers::kMoving), solid);
    return hit.HadHit();
}

std::uint64_t Simulation::PhysicsWorld::add_fabricated(const std::uint8_t *data, const std::size_t size,
                                                       const JPH::RVec3 at, const float yaw,
                                                       FabricatedRefusal &refusal) {
    Fabricated object;
    refusal = fabricated::parse(data, size, object.spec);
    if (refusal == FabricatedRefusal::None) {
        refusal = fabricated::build_shape(object.spec, object.shape, object.mass_kg);
    }
    if (refusal != FabricatedRefusal::None) {
        return 0;
    }
    if (fabricated_.size() >= fabricated::kMaxObjects ||
        physics_system_.GetNumBodies() >= physics_system_.GetMaxBodies()) {
        refusal = FabricatedRefusal::Full;
        return 0;
    }
    if (!in_world(at.GetX(), at.GetY(), at.GetZ()) || !std::isfinite(yaw)) {
        refusal = FabricatedRefusal::Placement;
        return 0;
    }
    const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yaw);
    const JPH::RVec3 origin = at + JPH::Vec3(0.0F, object.spec.half_extent[1] + kSetClearanceM, 0.0F);
    if (fabricated_overlaps(object.shape.GetPtr(), origin, rotation)) {
        refusal = FabricatedRefusal::Placement;
        return 0;
    }
    object.entity = fabricated::kEntityBase + fabricated_.size();
    object.data.assign(data, data + size);
    object.body = build_fabricated(object, origin, rotation);
    if (!object.body.valid()) {
        refusal = FabricatedRefusal::Full;
        return 0;
    }
    // The checkpoint holds every kit body: the delivery as it arrived, until
    // the next commit takes it as it lies.
    kit::Kit::Checkpoint now;
    kit_->capture(now);
    checkpoint_.kit.bodies.push_back(now.bodies.back());
    fabricated_.push_back(std::move(object));
    return fabricated_.back().entity;
}

std::vector<Vector3> Simulation::PhysicsWorld::fabricated_hull_triangles(const std::size_t index) const {
    std::vector<Vector3> out;
    if (index >= fabricated_.size()) {
        return out;
    }
    const JPH::Shape *shape = fabricated_[index].shape.GetPtr();
    // Each hull's faces, fanned into triangles, in the object's own frame
    // (Jolt keeps a hull's points about its center of mass, and a compound's
    // parts about the compound's).
    const auto add_hull = [&out](const JPH::ConvexHullShape &hull, const JPH::Vec3 offset, const JPH::Quat turn) {
        std::array<JPH::uint, 64> face{};
        for (JPH::uint f = 0; f < hull.GetNumFaces(); ++f) {
            const JPH::uint n = hull.GetFaceVertices(f, static_cast<JPH::uint>(face.size()), face.data());
            for (JPH::uint v = 1; v + 1 < n; ++v) {
                for (const JPH::uint corner : {face[0], face[v], face[v + 1]}) {
                    const JPH::Vec3 p = offset + turn * hull.GetPoint(corner);
                    out.push_back({p.GetX(), p.GetY(), p.GetZ()});
                }
            }
        }
    };
    if (shape->GetSubType() == JPH::EShapeSubType::ConvexHull) {
        add_hull(*static_cast<const JPH::ConvexHullShape *>(shape), shape->GetCenterOfMass(), JPH::Quat::sIdentity());
    } else if (shape->GetSubType() == JPH::EShapeSubType::StaticCompound) {
        const auto *compound = static_cast<const JPH::StaticCompoundShape *>(shape);
        for (JPH::uint s = 0; s < compound->GetNumSubShapes(); ++s) {
            const JPH::CompoundShape::SubShape &sub = compound->GetSubShape(s);
            if (sub.mShape->GetSubType() == JPH::EShapeSubType::ConvexHull) {
                add_hull(*static_cast<const JPH::ConvexHullShape *>(sub.mShape.GetPtr()),
                         compound->GetCenterOfMass() + sub.GetPositionCOM(), sub.GetRotation());
            }
        }
    }
    return out;
}

std::uint32_t Simulation::PhysicsWorld::bodies_with_entity_in(const std::uint64_t first,
                                                              const std::uint64_t limit) const {
    JPH::BodyIDVector ids;
    physics_system_.GetBodies(ids);
    std::uint32_t count = 0;
    const JPH::BodyLockInterfaceNoLock &locks = physics_system_.GetBodyLockInterfaceNoLock();
    for (const JPH::BodyID id : ids) {
        const JPH::BodyLockRead lock(locks, id);
        if (lock.Succeeded() && lock.GetBody().GetUserData() >= first && lock.GetBody().GetUserData() < limit) {
            ++count;
        }
    }
    return count;
}

// After the grips: each fabricated object's entity, hull file and body state
// at the committed checkpoint. Written only when the world holds one, so a
// save of a world with none is byte for byte what it was before.
void Simulation::PhysicsWorld::write_fabricated(SaveWriter &w) const {
    if (fabricated_.empty()) {
        return;
    }
    w.pod(kSaveMark);
    w.pod(static_cast<std::uint32_t>(fabricated_.size()));
    const std::size_t built = kit_built_count();
    for (std::size_t i = 0; i < fabricated_.size(); ++i) {
        const Fabricated &object = fabricated_[i];
        w.pod(object.entity);
        w.bytes(object.data);
        const kit::Kit::BodyState &state = checkpoint_.kit.bodies[built + i];
        const float numbers[13] = {static_cast<float>(state.position.GetX()), static_cast<float>(state.position.GetY()),
                                   static_cast<float>(state.position.GetZ()), state.rotation.GetX(),
                                   state.rotation.GetY(), state.rotation.GetZ(), state.rotation.GetW(),
                                   state.linear.GetX(), state.linear.GetY(), state.linear.GetZ(),
                                   state.angular.GetX(), state.angular.GetY(), state.angular.GetZ()};
        w.pod(numbers);
        w.pod(static_cast<std::uint8_t>(state.enabled ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(state.asleep ? 1 : 0));
    }
}

// The save's fabricated objects, bounded as a delivery is (the hull file read
// and its shape built: a save is untrusted too), each in the world, at rest
// or moving at a finite speed, in the world's own order of entities.
bool Simulation::PhysicsWorld::read_fabricated(SaveReader &r, std::vector<SavedFabricated> &out) const {
    std::uint32_t mark = 0, count = 0;
    if (!r.pod(mark) || mark != kSaveMark || !r.pod(count) || count < 1 || count > fabricated::kMaxObjects) {
        return false;
    }
    out.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        SavedFabricated &saved = out[i];
        Fabricated &object = saved.object;
        float numbers[13] = {};
        std::uint8_t enabled = 0, asleep = 0;
        if (!r.pod(object.entity) || object.entity != fabricated::kEntityBase + i ||
            !r.bytes(object.data, fabricated::kMinFileBytes, fabricated::kMaxFileBytes) || !r.pod(numbers) ||
            !r.pod(enabled) || !r.pod(asleep) || enabled != 1 || asleep > 1) {
            return false;
        }
        if (fabricated::parse(object.data.data(), object.data.size(), object.spec) != FabricatedRefusal::None ||
            fabricated::build_shape(object.spec, object.shape, object.mass_kg) != FabricatedRefusal::None) {
            return false;
        }
        for (const float number : numbers) {
            if (!std::isfinite(number)) {
                return false;
            }
        }
        const JPH::Quat rotation(numbers[3], numbers[4], numbers[5], numbers[6]);
        if (!in_world(numbers[0], numbers[1], numbers[2]) || std::abs(rotation.Length() - 1.0F) > 1.0e-3F) {
            return false;
        }
        saved.state.position = JPH::RVec3(numbers[0], numbers[1], numbers[2]);
        saved.state.rotation = rotation.Normalized();
        saved.state.linear = JPH::Vec3(numbers[7], numbers[8], numbers[9]);
        saved.state.angular = JPH::Vec3(numbers[10], numbers[11], numbers[12]);
        saved.state.enabled = true;
        saved.state.asleep = asleep == 1;
    }
    return true;
}

} // namespace scraperx::sim
