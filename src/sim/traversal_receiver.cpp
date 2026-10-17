#include "sim/traversal_receiver.hpp"
#include "sim/traversal_plan.hpp"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
namespace {
template<class V> bool finite(const V &v) noexcept {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
class ReceiverFilter final : public JPH::BodyFilter {
public:
    explicit ReceiverFilter(JPH::BodyID id) : id_(id) {}
    bool ShouldCollide(const JPH::BodyID &id) const override { return id == id_; }
private:
    JPH::BodyID id_;
};
class PatchCollector final : public JPH::CastRayCollector {
public:
    PatchCollector(const JPH::RRayCast &ray, JPH::RVec3 point, JPH::Vec3 normal,
                   const JPH::Shape *shape, std::uint64_t entity, float tolerance)
        : ray_(ray), point_(point), normal_(normal), shape_(shape), entity_(entity), tolerance_(tolerance) {}
    void OnBody(const JPH::Body &body) override {
        body_ = body.IsRigidBody() && body.IsInBroadPhase() && !body.IsSensor() &&
            body.GetShape() == shape_ && body.GetUserData() == entity_ ? &body : nullptr;
    }
    void OnBodyEnd() override { body_ = nullptr; }
    void AddHit(const JPH::RayCastResult &hit) override {
        if (!body_ || !std::isfinite(hit.mFraction) || hit.mFraction < 0 || hit.mFraction > 1) return;
        const auto point = ray_.GetPointOnRay(hit.mFraction);
        const auto normal = body_->GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
        // Four float epsilons cover float ray/COM/shape reconstruction. This
        // scales with representable coordinates, never gameplay clearance.
        constexpr float normal_tolerance = 4 * std::numeric_limits<float>::epsilon();
        if (finite(point) && finite(normal) &&
            (normal - normal_).LengthSq() <= normal_tolerance * normal_tolerance &&
            std::abs(JPH::Vec3(point - point_).Dot(normal_)) <= tolerance_) {
            if (!patch.add(hit.mSubShapeID2)) ForceEarlyOut();
        }
    }
    ReceiverSurfacePatch patch;
private:
    const JPH::RRayCast &ray_;
    JPH::RVec3 point_;
    JPH::Vec3 normal_;
    const JPH::Shape *shape_;
    std::uint64_t entity_;
    float tolerance_;
    const JPH::Body *body_ = nullptr;
};
}
bool collect_receiver_surface_patch(const JPH::PhysicsSystem &system,
    const JPH::RRayCast &ray, const JPH::RayCastResult &canonical,
    ReceiverSurfacePatch &patch) noexcept {
    patch = {};
    if (canonical.mBodyID.IsInvalid() || !finite(ray.mOrigin) || !finite(ray.mDirection) ||
        !std::isfinite(canonical.mFraction) || canonical.mFraction < 0 || canonical.mFraction > 1 ||
        ray.mDirection.IsNearZero()) return false;
    const auto point = ray.GetPointOnRay(canonical.mFraction);
    JPH::Vec3 normal;
    const JPH::Shape *shape;
    std::uint64_t entity;
    float scale;
    {
        JPH::BodyLockRead lock(system.GetBodyLockInterface(), canonical.mBodyID);
        if (!lock.Succeeded()) return false;
        const auto &body = lock.GetBody();
        if (!body.IsRigidBody() || !body.IsInBroadPhase() || body.IsSensor()) return false;
        JPH::SubShapeID remainder;
        if (!body.GetShape()->GetLeafShape(canonical.mSubShapeID2, remainder)) return false;
        normal = body.GetWorldSpaceSurfaceNormal(canonical.mSubShapeID2, point);
        if (!finite(normal) || normal.GetY() <= 0 || normal.IsNearZero()) return false;
        shape = body.GetShape(); entity = body.GetUserData();
        scale = std::max({1.F, float(std::abs(point.GetX())), float(std::abs(point.GetY())),
            float(std::abs(point.GetZ())), ray.mDirection.Length(),
            shape->GetLocalBounds().GetExtent().Length()});
    }
    PatchCollector collector(ray, point, normal, shape, entity,
        4 * std::numeric_limits<float>::epsilon() * scale);
    ReceiverFilter filter(canonical.mBodyID);
    system.GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector, {}, {}, filter);
    if (collector.patch.overflow || !collector.patch.contains(canonical.mSubShapeID2)) return false;
    patch = collector.patch;
    return true;
}

bool receiver_supports_continuation(const JPH::PhysicsSystem &system,
    const JPH::RRayCast &ray, const TraversalPlanState &plan,
    JPH::BodyID ignored_body, float minimum_normal_y) noexcept {
    if (!finite(ray.mOrigin) || !finite(ray.mDirection) || ray.mDirection.IsNearZero() ||
        plan.receiver.IsInvalid() || !plan.receiver_shape) return false;
    JPH::RayCastResult hit;
    const JPH::IgnoreSingleBodyFilter filter(ignored_body);
    if (!system.GetNarrowPhaseQuery().CastRay(ray,hit,{},{},filter) ||
        hit.mBodyID!=plan.receiver || !plan.receiver_patch.contains(hit.mSubShapeID2)) return false;
    const JPH::BodyLockRead lock(system.GetBodyLockInterface(),hit.mBodyID);
    if (!lock.Succeeded()) return false;
    const auto &body=lock.GetBody();
    return body.IsRigidBody() && body.IsInBroadPhase() && !body.IsSensor() &&
        body.GetShape()==plan.receiver_shape && body.GetUserData()==plan.receiver_entity &&
        body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,ray.GetPointOnRay(hit.mFraction)).GetY()>=minimum_normal_y;
}
}
