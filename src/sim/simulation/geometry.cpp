#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] bool Simulation::PhysicsWorld::cast_ray(const JPH::RVec3 origin,
                            const JPH::Vec3 direction,
                            JPH::RayCastResult &hit,
                            const bool see_past_soft_holds) const noexcept {
    const JPH::RRayCast ray(origin, direction);
    hit.Reset();
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    if (see_past_soft_holds && cargo_net_) {
        // A curved mesh may sit immediately ahead of a ledge. Filter it
        // from the query instead of advancing the ray into the wall.
        const JPH::IgnoreSingleBodyFilterChained filter(cargo_net_->body(), body_filter);
        return physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter);
    }
    return physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter);
}

[[nodiscard]] bool Simulation::PhysicsWorld::cast_wall(const JPH::RVec3 origin, const JPH::Vec3 direction,
                             const bool see_past_holds, JPH::RayCastResult &hit,
                             JPH::RVec3 &point) const noexcept {
    const float length = direction.Length();
    if (length <= 0.0F) {
        return false;
    }
    const JPH::Vec3 unit = direction / length;
    JPH::RVec3 from = origin;
    float left = length;
    for (std::uint32_t pass = 0; pass < 4; ++pass) {
        const JPH::Vec3 ray = unit * left;
        if (!cast_ray(from, ray, hit, see_past_holds)) {
            return false;
        }
        point = JPH::RRayCast(from, ray).GetPointOnRay(hit.mFraction);
        if (!see_past_holds || !is_hold(hit.mBodyID, hit.mSubShapeID2)) {
            return true;
        }
        constexpr float kPastHold = 0.20F;
        left -= hit.mFraction * left + kPastHold;
        if (left <= 0.0F) {
            return false;
        }
        from = point + unit * kPastHold;
    }
    return false;
}

[[nodiscard]] bool Simulation::PhysicsWorld::is_hold(const JPH::BodyID body_id,
                           const JPH::SubShapeID &sub_shape_id) const noexcept {
    if (cargo_net_ && body_id == cargo_net_->body()) return true;
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
    LeafBox box;
    std::uint32_t long_axis = 0;
    return lock.Succeeded() && leaf_box(lock.GetBody(), sub_shape_id, box) &&
           box_is_hold(box, long_axis);
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::surface_normal(const JPH::BodyID body_id,
                                       const JPH::SubShapeID &sub_shape_id,
                                       const JPH::RVec3 point) const noexcept {
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
    if (!lock.Succeeded()) {
        return JPH::Vec3::sZero();
    }
    return lock.GetBody().GetWorldSpaceSurfaceNormal(sub_shape_id, point);
}

[[nodiscard]] bool Simulation::PhysicsWorld::footing_is_firm(const JPH::BodyInterface &bodies) const noexcept {
    if (deformable_plank_ &&
        deformable_plank_->logical_member_for_body(support_sample_.body_id) &&
        !deformable_plank_->fragment_supported(support_sample_.body_id)) return false;
    if (net_for_body(support_sample_.body_id)) return false;
    const float half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    const float reach = half_height + kCheckpointFootingSlack;
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const JPH::Vec3 down(0.0F, -reach, 0.0F);
    const JPH::RRayCast ray(centre, down);
    JPH::RayCastResult hit;
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) {
        return false;
    }
    if (net_for_body(hit.mBodyID)) return false;
    const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);
    const JPH::Vec3 normal = surface_normal(hit.mBodyID, hit.mSubShapeID2, point);
    if (normal.GetY() < kSupportNormalThreshold) return false;
    if (deformable_plank_ && deformable_plank_->logical_member_for_body(hit.mBodyID) &&
        !deformable_plank_->fragment_supported(hit.mBodyID)) return false;
    // For a vertical capsule tangent to a plane, the vertical ray depth
    // is cylinder_half_height + radius / normal.y. Permit twice Jolt's
    // declared penetration slop, not an unrelated checkpoint tolerance.
    const float tolerance = 2.0F * physics_system_.GetPhysicsSettings().mPenetrationSlop;
    const float rest_depth = half_height - kPlayerRadius + kPlayerRadius / normal.GetY();
    if (std::abs(reach * hit.mFraction - rest_depth) > tolerance) return false;
    for (const JPH::Vec3 offset : {
             JPH::Vec3(kCheckpointFootingMargin, 0, 0),
             JPH::Vec3(-kCheckpointFootingMargin, 0, 0),
             JPH::Vec3(0, 0, kCheckpointFootingMargin),
             JPH::Vec3(0, 0, -kCheckpointFootingMargin)}) {
        const JPH::RRayCast probe(centre + offset, down);
        JPH::RayCastResult nearby;
        if (!physics_system_.GetNarrowPhaseQuery().CastRay(probe, nearby, {}, {}, filter)) {
            return false;
        }
        const JPH::RVec3 nearby_point = probe.GetPointOnRay(nearby.mFraction);
        if (deformable_plank_ && deformable_plank_->logical_member_for_body(nearby.mBodyID) &&
            !deformable_plank_->fragment_supported(nearby.mBodyID)) return false;
        if (surface_normal(nearby.mBodyID, nearby.mSubShapeID2, nearby_point).GetY() <
                kSupportNormalThreshold ||
            std::abs(JPH::Vec3(nearby_point - point).Dot(normal)) > tolerance) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] const JPH::Shape *Simulation::PhysicsWorld::active_player_shape() const noexcept {
    return crouched_ ? player_crouch_shape_.GetPtr() : player_shape_.GetPtr();
}

[[nodiscard]] bool Simulation::PhysicsWorld::capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept {
    return shape_pose_is_clear(active_player_shape(), centre);
}

[[nodiscard]] bool Simulation::PhysicsWorld::shape_pose_is_clear(const JPH::Shape *shape,
                                       const JPH::RVec3 centre,
                                       const float contact_slop) const noexcept {
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    JPH::CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CollideShape(shape,
                                                       JPH::Vec3::sReplicate(1.0F),
                                                       JPH::RMat44::sTranslation(centre),
                                                       settings,
                                                       centre,
                                                       collector,
                                                       {},
                                                       {},
                                                       body_filter);
    // Debug relocation may reuse a standing pose with ordinary solver
    // contact slop. Traversal callers retain strict clearance by default.
    return std::none_of(collector.mHits.begin(), collector.mHits.end(),
        [contact_slop](const auto &hit) { return hit.mPenetrationDepth > contact_slop; });
}

[[nodiscard]] LedgeProbe Simulation::PhysicsWorld::probe_ledge(const JPH::RVec3 origin,
                                     const JPH::Vec3 facing,
                                     const float minimum_rise,
                                     const float maximum_rise,
                                     const bool require_supported_landing,
                                     const bool see_past_holds,
                                     const bool require_clear_landing) const noexcept {
    LedgeProbe probe;
    if (facing.IsNearZero()) {
        return probe;
    }

    JPH::RayCastResult wall_hit;
    JPH::RVec3 wall_point;
    if (!cast_wall(origin, facing * (kTraversalReach + kPlayerRadius), see_past_holds, wall_hit,
                   wall_point)) {
        return probe;
    }

    return probe_ledge_from_wall(origin, facing, minimum_rise, maximum_rise,
                                 require_supported_landing, require_clear_landing,
                                 wall_hit, wall_point);
}

[[nodiscard]] LedgeProbe Simulation::PhysicsWorld::probe_ledge_from_wall(
    const JPH::RVec3 origin, const JPH::Vec3 facing,
    const float minimum_rise, const float maximum_rise,
    const bool require_supported_landing, const bool require_clear_landing,
    const JPH::RayCastResult &wall_hit, const JPH::RVec3 wall_point,
    const bool require_same_sub_shape) const noexcept {
    LedgeProbe probe;

    const JPH::Vec3 wall_surface =
        surface_normal(wall_hit.mBodyID, wall_hit.mSubShapeID2, wall_point);
    JPH::Vec3 outward(wall_surface.GetX(), 0.0F, wall_surface.GetZ());
    if (outward.IsNearZero()) {
        return probe;
    }
    outward = outward.Normalized();
    const JPH::Vec3 inward = -outward;
    const JPH::Vec3 intended = facing.Normalized();
    if (intended.Dot(inward) <= 0.05F) {
        return probe;
    }

    const float feet_y = origin.GetY() - kPlayerHalfHeight;
    const JPH::RVec3 top_origin(wall_point.GetX() + inward.GetX() * kTopProbeInset,
                                feet_y + maximum_rise + kTopProbeMargin,
                                wall_point.GetZ() + inward.GetZ() * kTopProbeInset);
    const float top_ray_length = maximum_rise + kTopProbeMargin - minimum_rise;
    if (top_ray_length <= 0.0F) {
        return probe;
    }
    const JPH::Vec3 top_direction(0.0F, -top_ray_length, 0.0F);

    JPH::RayCastResult top_hit;
    if (!cast_ray(top_origin, top_direction, top_hit)) {
        return probe;
    }
    if (top_hit.mBodyID != wall_hit.mBodyID ||
        (require_same_sub_shape && top_hit.mSubShapeID2 != wall_hit.mSubShapeID2)) {
        return probe;
    }
    const JPH::RVec3 ledge_point =
        JPH::RRayCast(top_origin, top_direction).GetPointOnRay(top_hit.mFraction);
    if (surface_normal(top_hit.mBodyID, top_hit.mSubShapeID2, ledge_point).GetY() <
        kLedgeTopNormalThreshold) {
        return probe;
    }

    const float rise = ledge_point.GetY() - feet_y;
    if (rise < minimum_rise || rise > maximum_rise) {
        return probe;
    }

    const JPH::RVec3 landing_centre(wall_point.GetX() + inward.GetX() * kLandingInset,
                                    ledge_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                                    wall_point.GetZ() + inward.GetZ() * kLandingInset);

    JPH::BodyID landing_body = top_hit.mBodyID;
    JPH::SubShapeID landing_subshape = top_hit.mSubShapeID2;
    if (require_supported_landing) {
        const JPH::RVec3 support_origin(landing_centre.GetX(),
                                        ledge_point.GetY() + kLandingSupportProbeUp,
                                        landing_centre.GetZ());
        const JPH::Vec3 support_direction(
            0.0F, -(kLandingSupportProbeUp + kLandingSupportTolerance), 0.0F);
        JPH::RayCastResult landing_hit;
        if (!cast_ray(support_origin, support_direction, landing_hit)) {
            return probe;
        }
        if (landing_hit.mBodyID != top_hit.mBodyID) {
            return probe;
        }
        if (require_same_sub_shape) {
            const auto support_point = JPH::RRayCast(support_origin, support_direction)
                                           .GetPointOnRay(landing_hit.mFraction);
            if (landing_hit.mSubShapeID2 != top_hit.mSubShapeID2 ||
                surface_normal(landing_hit.mBodyID, landing_hit.mSubShapeID2,
                               support_point).GetY() < kLedgeTopNormalThreshold ||
                std::abs(support_point.GetY() - ledge_point.GetY()) >
                    kLandingSupportTolerance) return probe;
        }
        landing_body = landing_hit.mBodyID;
        landing_subshape = landing_hit.mSubShapeID2;
    }

    if (require_clear_landing && !capsule_pose_is_clear(landing_centre)) {
        return probe;
    }

    const auto &bodies = physics_system_.GetBodyInterface();
    probe.valid = true;
    probe.ledge_body = top_hit.mBodyID;
    probe.ledge_sub_shape = top_hit.mSubShapeID2;
    probe.ledge_entity_id = bodies.GetUserData(top_hit.mBodyID);
    probe.wall_point = wall_point;
    probe.inward = inward;
    probe.ledge_point = ledge_point;
    probe.landing_centre = landing_centre;
    probe.landing_body = landing_body;
    probe.landing_subshape = landing_subshape;
    probe.landing_entity_id = bodies.GetUserData(landing_body);
    probe.rise = rise;
    return probe;
}

[[nodiscard]] bool Simulation::PhysicsWorld::probe_vault_landing(const LedgeProbe &obstacle,
                                       const JPH::Vec3 facing,
                                       const float feet_y,
                                       JPH::RVec3 &landing_centre,
                                       JPH::BodyID &landing_body) const noexcept {
    const JPH::RVec3 far_origin(
        obstacle.wall_point.GetX() + facing.GetX() * kVaultCrossDistance,
        obstacle.ledge_point.GetY() + 0.40F,
        obstacle.wall_point.GetZ() + facing.GetZ() * kVaultCrossDistance);
    const float drop_length = obstacle.ledge_point.GetY() + 0.40F - (feet_y - kVaultMaximumDrop);
    if (drop_length <= 0.0F) {
        return false;
    }
    const JPH::Vec3 drop_direction(0.0F, -drop_length, 0.0F);

    JPH::RayCastResult landing_hit;
    if (!cast_ray(far_origin, drop_direction, landing_hit)) {
        return false;
    }
    if (landing_hit.mBodyID == obstacle.ledge_body) {
        return false;
    }
    const JPH::RVec3 landing_point =
        JPH::RRayCast(far_origin, drop_direction).GetPointOnRay(landing_hit.mFraction);
    if (surface_normal(landing_hit.mBodyID, landing_hit.mSubShapeID2, landing_point).GetY() <
        kLedgeTopNormalThreshold) {
        return false;
    }

    const JPH::RVec3 candidate(landing_point.GetX(),
                               landing_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                               landing_point.GetZ());
    if (!capsule_pose_is_clear(candidate)) {
        return false;
    }

    landing_centre = candidate;
    landing_body = landing_hit.mBodyID;
    return true;
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::to_support_local(const JPH::BodyInterface &bodies,
                                         const JPH::BodyID body_id,
                                         const JPH::RVec3 world_point) const noexcept {
    if (body_id.IsInvalid()) {
        return JPH::Vec3(world_point);
    }
    if (const auto *net = net_for_body(body_id)) return net->material_point(world_point);
    return JPH::Vec3(bodies.GetCenterOfMassTransform(body_id).Inversed() * world_point);
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::from_support_local(const JPH::BodyInterface &bodies,
                                            const JPH::BodyID body_id,
                                            const JPH::Vec3 local_point) const noexcept {
    if (body_id.IsInvalid()) {
        return JPH::RVec3(local_point);
    }
    if (const auto *net = net_for_body(body_id)) return net->world_point(local_point);
    return bodies.GetCenterOfMassTransform(body_id) * JPH::RVec3(local_point);
}

} // namespace scraperx::sim
