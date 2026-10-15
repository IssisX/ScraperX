#include "sim/traversal_affordance.hpp"

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <cmath>
#include <limits>

namespace scraperx::sim {
namespace {
template<class V> bool finite(const V &v) noexcept {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}

bool surface_frame(const RigidTraversalAnchor &anchor) noexcept {
    if (!finite(anchor.local_point) || !finite(anchor.local_normal) || !finite(anchor.local_tangent))
        return false;
    const float normal_length = anchor.local_normal.LengthSq();
    const float tangent_length = anchor.local_tangent.LengthSq();
    const float cross_length = anchor.local_normal.Cross(anchor.local_tangent).LengthSq();
    return std::isfinite(normal_length) && std::isfinite(tangent_length) &&
        std::isfinite(cross_length) && normal_length > 1.0e-8F && tangent_length > 1.0e-8F &&
        cross_length > 1.0e-8F * normal_length * tangent_length;
}

bool valid_body(const JPH::Body *body, const std::uint64_t entity,
                const JPH::Shape *shape, const JPH::SubShapeID subshape) noexcept {
    if (!body || !body->IsRigidBody() || !body->IsInBroadPhase() || body->IsSensor() ||
        body->GetUserData() != entity || body->GetShape() != shape)
        return false;
    // The shape pointer is checked before use, so a stale raw identity is never
    // dereferenced. GetLeafShape safely rejects removed compound children.
    JPH::SubShapeID remainder;
    return body->GetShape()->GetLeafShape(subshape, remainder) != nullptr;
}
} // namespace

std::uint64_t TraversalAffordanceCache::remember(const TraversalAffordance &sample,
                                                const std::uint64_t tick) noexcept {
    if (sample.anchor_kind != TraversalAnchorKind::RigidSubshape ||
        sample.rigid.body.IsInvalid() || sample.rigid.entity_id == 0 ||
        sample.rigid.geometry_generation == 0 || !sample.rigid.shape_identity ||
        sample.receiver_body.IsInvalid() || sample.receiver_entity_id == 0 ||
        !sample.receiver_shape_identity || !sample.grip_capable ||
        !sample.receiver_footing_capable || sample.receiver_patch.overflow ||
        sample.receiver_patch.count > sample.receiver_patch.subshapes.size() ||
        (sample.receiver_patch.count != 0 && !sample.receiver_patch.contains(sample.receiver_subshape)) ||
        !surface_frame(sample.rigid) ||
        generation_ == std::numeric_limits<std::uint64_t>::max())
        return 0;

    // When the owner supplies a new geometry revision for the same live body,
    // retire observations of its previous geometry rather than keeping both.
    for (auto &entry : entries_)
        if (entry.generation != 0 && entry.rigid.body == sample.rigid.body &&
            entry.rigid.geometry_generation != sample.rigid.geometry_generation)
            entry.generation = 0;

    auto &entry = entries_[next_slot_];
    entry = sample;
    if (entry.receiver_patch.count == 0) entry.receiver_patch.add(entry.receiver_subshape);
    entry.observed_tick = tick;
    entry.generation = ++generation_;
    last_refresh_tick_[next_slot_] = tick;
    next_slot_ = (next_slot_ + 1) % entries_.size();
    return entry.generation;
}

const TraversalAffordance *TraversalAffordanceCache::refresh(const std::uint64_t generation,
    const JPH::PhysicsSystem &system, const std::uint64_t tick,
    const std::uint64_t max_age_ticks) noexcept {
    if (generation == 0) return nullptr;
    for (unsigned index = 0; index < entries_.size(); ++index) {
        auto &entry = entries_[index];
        if (entry.generation != generation) continue;
        const auto invalidate = [&entry]() -> const TraversalAffordance * {
            entry.generation = 0;
            return nullptr;
        };
        // Check rollback before subtraction to avoid unsigned age wraparound.
        if (tick < entry.observed_tick || tick < last_refresh_tick_[index] ||
            tick - entry.observed_tick > max_age_ticks)
            return invalidate();

        const JPH::BodyID ids[] = {entry.rigid.body, entry.receiver_body};
        const JPH::BodyLockMultiRead lock(system.GetBodyLockInterface(), ids, 2);
        const auto *support = lock.GetBody(0);
        const auto *receiver = lock.GetBody(1);
        if (!valid_body(support, entry.rigid.entity_id, entry.rigid.shape_identity, entry.rigid.subshape) ||
            !valid_body(receiver, entry.receiver_entity_id, entry.receiver_shape_identity, entry.receiver_subshape))
            return invalidate();

        for (unsigned i = 0; i < entry.receiver_patch.count; ++i)
            if (!valid_body(receiver, entry.receiver_entity_id, entry.receiver_shape_identity,
                            entry.receiver_patch.subshapes[i])) return invalidate();

        entry.world_point = support->GetCenterOfMassTransform() * JPH::RVec3(entry.rigid.local_point);
        entry.world_normal = support->GetRotation() * entry.rigid.local_normal;
        entry.world_tangent = support->GetRotation() * entry.rigid.local_tangent;
        entry.point_velocity = support->GetPointVelocity(entry.world_point);
        if (!finite(entry.world_point) || !finite(entry.world_normal) ||
            !finite(entry.world_tangent) || !finite(entry.point_velocity))
            return invalidate();
        last_refresh_tick_[index] = tick;
        return &entry;
    }
    return nullptr;
}

const TraversalAffordance *TraversalAffordanceCache::find(const std::uint64_t generation) const noexcept {
    if (generation != 0)
        for (const auto &entry : entries_)
            if (entry.generation == generation) return &entry;
    return nullptr;
}

void TraversalAffordanceCache::clear() noexcept {
    for (auto &entry : entries_) entry.generation = 0;
    // Keep generation_ monotonic across clears so old handles cannot revive.
    next_slot_ = 0;
}

} // namespace scraperx::sim
