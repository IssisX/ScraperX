// Geometry probes, the support frame, the carry, rigging, the crouch and the grappling hook.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim {

bool Simulation::PhysicsWorld::cast_ray(const JPH::RVec3 origin,
                                        const JPH::Vec3 direction,
                                        JPH::RayCastResult &hit) const noexcept {
    const JPH::RRayCast ray(origin, direction);
    hit.Reset();
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    return physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter);
}

namespace {

// The eye's sight line: through the player's own body, the load in the hands
// and sensors, which nobody sees.
class SightPasses final : public JPH::BodyFilter {
public:
    SightPasses(const JPH::BodyID player, const JPH::BodyID carried) : player_(player), carried_(carried) {}
    bool ShouldCollide(const JPH::BodyID &id) const override { return id != player_ && id != carried_; }
    bool ShouldCollideLocked(const JPH::Body &body) const override { return !body.IsSensor(); }

private:
    JPH::BodyID player_;
    JPH::BodyID carried_;
};

} // namespace

bool Simulation::PhysicsWorld::look_hit(const JPH::RVec3 from, const JPH::Vec3 run, std::uint64_t &entity,
                                        JPH::RVec3 &point, JPH::Vec3 &normal) const noexcept {
    if (run.IsNearZero()) {
        return false;
    }
    const JPH::RRayCast ray(from, run);
    JPH::RayCastResult hit;
    const SightPasses passes(player_id_, carried_id_);
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, passes)) {
        return false;
    }
    point = ray.GetPointOnRay(hit.mFraction);
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), hit.mBodyID);
    if (!lock.Succeeded()) {
        return false;
    }
    entity = lock.GetBody().GetUserData();
    normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
    return true;
}

// A ray for a wall. With see_past_holds, members a hand could close
// round (rungs, pipes, bars) are seen through, the way a climber on a
// ladder sees the wall behind it.
bool Simulation::PhysicsWorld::cast_wall(const JPH::RVec3 origin, const JPH::Vec3 direction,
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
        if (!cast_ray(from, ray, hit)) {
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

bool Simulation::PhysicsWorld::is_hold(const JPH::BodyID body_id,
                                       const JPH::SubShapeID &sub_shape_id) const noexcept {
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
    LeafBox box;
    std::uint32_t long_axis = 0;
    return lock.Succeeded() && leaf_box(lock.GetBody(), sub_shape_id, box) &&
           box_is_hold(box, long_axis);
}

JPH::Vec3 Simulation::PhysicsWorld::surface_normal(const JPH::BodyID body_id,
                                                   const JPH::SubShapeID &sub_shape_id,
                                                   const JPH::RVec3 point) const noexcept {
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
    if (!lock.Succeeded()) {
        return JPH::Vec3::sZero();
    }
    return lock.GetBody().GetWorldSpaceSurfaceNormal(sub_shape_id, point);
}

// Firm footing, for a checkpoint (kCheckpointFootingSlack): a ray straight
// down from the body's centre meets a walkable surface within reach. The
// carried body is not footing, whatever it hangs over.
bool Simulation::PhysicsWorld::footing_is_firm(const JPH::BodyInterface &bodies) const noexcept {
    const float reach =
        (crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight) + kCheckpointFootingSlack;
    const JPH::RRayCast ray(bodies.GetPosition(player_id_), JPH::Vec3(0.0F, -reach, 0.0F));
    JPH::RayCastResult hit;
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) {
        return false;
    }
    return surface_normal(hit.mBodyID, hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction))
               .GetY() >= kSupportNormalThreshold;
}

bool Simulation::PhysicsWorld::shape_pose_is_clear(const JPH::Shape *shape,
                                                   const JPH::RVec3 centre) const noexcept {
    JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
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
    return !collector.HadHit();
}

// Finds a ledge in front of `origin`. Every returned field comes from a real
// cast against the authoritative world; a failed reach, a missing top
// surface, a too-steep top, an out-of-band rise, an unsupported landing, or
// an obstructed landing pose all return an invalid probe.
LedgeProbe Simulation::PhysicsWorld::probe_ledge(const JPH::RVec3 origin,
                                                 const JPH::Vec3 facing,
                                                 const float minimum_rise,
                                                 const float maximum_rise,
                                                 const bool require_supported_landing,
                                                 const bool see_past_holds,
                                                 const float reach) const noexcept {
    LedgeProbe probe;
    if (facing.IsNearZero()) {
        return probe;
    }

    const float feet_y = origin.GetY() - kPlayerHalfHeight;
    JPH::RayCastResult wall_hit;
    JPH::RVec3 wall_point;
    // At the body's centre first; a wall lower than that (a bucket's rim,
    // a crate, a knee-high kerb: a rise of 0.35 to 0.9 m) is felt for
    // just above the lowest rise this probe takes.
    const double low_y = static_cast<double>(feet_y) + minimum_rise + kLowWallProbeAbove;
    const JPH::RVec3 low_origin(origin.GetX(), low_y, origin.GetZ());
    if (!cast_wall(origin, facing * (reach + kPlayerRadius), see_past_holds, wall_hit,
                   wall_point) &&
        (low_y >= origin.GetY() ||
         !cast_wall(low_origin, facing * (reach + kPlayerRadius), see_past_holds, wall_hit,
                    wall_point))) {
        return probe;
    }

    const JPH::RVec3 top_origin(wall_point.GetX() + facing.GetX() * kTopProbeInset,
                                feet_y + maximum_rise + kTopProbeMargin,
                                wall_point.GetZ() + facing.GetZ() * kTopProbeInset);
    const float top_ray_length = maximum_rise + kTopProbeMargin - minimum_rise;
    if (top_ray_length <= 0.0F) {
        return probe;
    }
    const JPH::Vec3 top_direction(0.0F, -top_ray_length, 0.0F);

    JPH::RayCastResult top_hit;
    if (!cast_ray(top_origin, top_direction, top_hit)) {
        return probe;
    }
    // The top may belong to another body than the face under it (a deck
    // over its edge beam, a crate on its pallet): the hands take whatever
    // edge is there.
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

    const JPH::RVec3 landing_centre(wall_point.GetX() + facing.GetX() * kLandingInset,
                                    ledge_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                                    wall_point.GetZ() + facing.GetZ() * kLandingInset);

    JPH::BodyID landing_body = top_hit.mBodyID;
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
        landing_body = landing_hit.mBodyID;
    }

    if (!capsule_pose_is_clear(landing_centre)) {
        return probe;
    }

    const auto &bodies = physics_system_.GetBodyInterface();
    probe.valid = true;
    probe.ledge_body = top_hit.mBodyID;
    probe.ledge_entity_id = bodies.GetUserData(top_hit.mBodyID);
    probe.wall_point = wall_point;
    probe.ledge_point = ledge_point;
    probe.landing_centre = landing_centre;
    probe.landing_body = landing_body;
    probe.landing_entity_id = bodies.GetUserData(landing_body);
    probe.rise = rise;
    return probe;
}

// Finds the far-side landing that makes an obstacle vaultable rather than
// mantleable. Without a real, clear landing beyond the obstacle there is no
// vault.
bool Simulation::PhysicsWorld::probe_vault_landing(const LedgeProbe &obstacle,
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

JPH::Vec3 Simulation::PhysicsWorld::to_support_local(const JPH::BodyInterface &bodies,
                                                     const JPH::BodyID body_id,
                                                     const JPH::RVec3 world_point) const noexcept {
    if (body_id.IsInvalid()) {
        return JPH::Vec3(world_point);
    }
    return JPH::Vec3(bodies.GetCenterOfMassTransform(body_id).Inversed() * world_point);
}

JPH::RVec3 Simulation::PhysicsWorld::from_support_local(const JPH::BodyInterface &bodies,
                                                        const JPH::BodyID body_id,
                                                        const JPH::Vec3 local_point) const noexcept {
    if (body_id.IsInvalid()) {
        return JPH::RVec3(local_point);
    }
    return bodies.GetCenterOfMassTransform(body_id) * JPH::RVec3(local_point);
}

// The bearing the hands hold the load on this tick: the load's present
// bearing from the body, turned toward the facing by at most
// kCarryTurnRadiansPerSecond. Derived from poses, so it adds no state.
JPH::Vec3 Simulation::PhysicsWorld::carry_hand_bearing(const JPH::BodyInterface &bodies,
                                                       const float delta_seconds) const noexcept {
    const JPH::Vec3 to_handle(
        bodies.GetCenterOfMassTransform(carried_id_) * carry_handle(carried_entity_) -
        bodies.GetPosition(player_id_));
    const JPH::Vec3 flat(to_handle.GetX(), 0.0F, to_handle.GetZ());
    if (flat.LengthSq() < 1.0e-4F) {
        return facing_;
    }
    const JPH::Vec3 bearing = flat.Normalized();
    if (facing_.IsNearZero()) {
        return bearing;
    }
    // Signed angle from the bearing to the facing about +Y.
    const float sine = bearing.GetZ() * facing_.GetX() - bearing.GetX() * facing_.GetZ();
    const float angle = std::atan2(sine, bearing.Dot(facing_));
    const float limit = kCarryTurnRadiansPerSecond * delta_seconds;
    return JPH::Quat::sRotation(JPH::Vec3::sAxisY(), std::clamp(angle, -limit, limit)) *
           bearing;
}

// What a pick-up would take now: the nearest carryable within reach of the
// body's centre, at rest, not behind it -- and only for a grounded body
// outside a traversal with its hands free.
JPH::BodyID Simulation::PhysicsWorld::carry_candidate(const JPH::BodyInterface &bodies,
                                                      std::uint64_t &entity) const noexcept {
    entity = 0;
    JPH::BodyID best;
    if (carry_constraint_ != nullptr || !grounded_ ||
        traversal_state_ != TraversalState::None || facing_.IsNearZero()) {
        return best;
    }
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    float best_distance = kCarryReach;
    const JPH::BodyID hers = matron_->held_body();
    const auto consider = [&](const JPH::BodyID id, const std::uint64_t candidate_entity) {
        if (id == hers) {
            return;   // in the Matron's hands
        }
        const JPH::Vec3 to_body(bodies.GetCenterOfMassPosition(id) - at);
        const float distance = to_body.Length();
        if (distance > best_distance ||
            bodies.GetLinearVelocity(id).Length() > kCarryMaxBodySpeed) {
            return;
        }
        const JPH::Vec3 flat(to_body.GetX(), 0.0F, to_body.GetZ());
        if (flat.Dot(facing_) < 0.0F) {
            return;
        }
        best = id;
        best_distance = distance;
        entity = candidate_entity;
    };
    kit_->carry_candidates(kit_carryables_);
    for (const auto &candidate : kit_carryables_) {
        consider(candidate.id, candidate.entity);
    }
    return best;
}

// A point constraint between the hands and the body's handle. Rotation is
// left free, so a block hangs and swings from it.
void Simulation::PhysicsWorld::attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept {
    const JPH::BodyInterface &bodies = physics_system_.GetBodyInterfaceNoLock();
    const JPH::Vec3 offset = carry_hand_offset(facing_);
    // The hands close on the handle where it is; update_carry draws it in.
    carry_entry_offset_ = JPH::Vec3(bodies.GetCenterOfMassTransform(id) * carry_handle(entity) -
                                    (bodies.GetPosition(player_id_) + offset));
    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
    settings.mPoint1 = JPH::RVec3(offset + carry_entry_offset_);
    settings.mPoint2 = JPH::RVec3(carry_handle(entity));
    carry_constraint_ = static_cast<JPH::PointConstraint *>(
        create_constraint(settings, player_id_, id, false));
    if (carry_constraint_ == nullptr) {
        return;
    }
    carried_id_ = id;
    carried_entity_ = entity;
    grip_over_seconds_ = 0.0F;
    carry_offset_last_ = offset + carry_entry_offset_;
    carry_yaw_ = load_yaw(bodies);
    contact_listener_.set_carried_entity(entity);
}

// Removes the constraint and nothing else: the body falls and rests.
void Simulation::PhysicsWorld::release_carry() noexcept {
    if (carry_constraint_ != nullptr) {
        physics_system_.RemoveConstraint(carry_constraint_);
        carry_constraint_ = nullptr;
    }
    carried_id_ = JPH::BodyID();
    carried_entity_ = 0;
    contact_listener_.set_carried_entity(0);
}

// The carried body's turn about the vertical: its own x axis, flattened.
float Simulation::PhysicsWorld::load_yaw(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::Vec3 axis = bodies.GetRotation(carried_id_) * JPH::Vec3::sAxisX();
    return std::atan2(-axis.GetZ(), axis.GetX());
}

// Both hands on a free load's flanks keep it from spinning in them: they
// hold its turn about the vertical as they took it, with no more torque
// than the grip gives at its flanks. Hung from one point it spun freely
// (D's spool came to its gap 27 degrees off, and the gap takes 20).
// Turned with the carrier instead, a load swept round in the hands as
// they walked a turning path, and knocked out of them (J's rail joint).
void Simulation::PhysicsWorld::hold_load_yaw(JPH::BodyInterface &bodies) noexcept {
    if (carried_load_mass() <= 0.0F) {
        return;
    }
    float inertia = 0.0F;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), carried_id_);
        if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) {
            return;
        }
        const float inverse = lock.GetBody().GetMotionProperties()->GetLocalSpaceInverseInertia()(1, 1);
        inertia = inverse > 0.0F ? 1.0F / inverse : 0.0F;
    }
    const float error = std::remainder(carry_yaw_ - load_yaw(bodies), 2.0F * static_cast<float>(kPi));
    const float spin = bodies.GetAngularVelocity(carried_id_).GetY();
    const float limit = kGripNewtons * kCarryFlankArm;
    const float torque = std::clamp(
        inertia * (kCarryYawRate * kCarryYawRate * error - 2.0F * kCarryYawRate * spin), -limit, limit);
    bodies.AddTorque(carried_id_, JPH::Vec3(0.0F, torque, 0.0F));
}

// The mass of the free load in the hands, or 0: a handle or a shackle is
// held up by its hinge or its line, not the hands.
float Simulation::PhysicsWorld::carried_load_mass() const noexcept {
    if (carry_constraint_ == nullptr || !scraperx::sim::kit::is_kit_entity(carried_entity_) ||
        kit_->carry_kind(carried_entity_) != scraperx::sim::kit::CarryKind::Load) {
        return 0.0F;
    }
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), carried_id_);
    if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) {
        return 0.0F;
    }
    const float inverse_mass = lock.GetBody().GetMotionProperties()->GetInverseMass();
    return inverse_mass > 0.0F ? 1.0F / inverse_mass : 0.0F;
}

// A free load in the hands slows its carrier. It swings from the hands,
// which hold its weight, the force that accelerates it (up to twice it,
// as the swing overshoots) and the pull of the swing itself; past
// kGripNewtons for kGripSeconds it tears out of them. So the heavier it
// is against the grip (kCarryGripShare of it), the slower it is carried
// -- top speed scaled by 1 - weight / grip, 1.76 m/s for D's 50 kg spool
// -- and the more gently it is started and stopped: 5.3 m/s^2 for the
// spool. (Carried at a key's full 5.5 m/s and stopped at
// kGroundAcceleration, the spool was thrown out of the hands into the
// shaft: 1030 N at the hands.) A handle is not capped: capped, a rider
// drawing E's 60 kg door shut stalled against it.
float Simulation::PhysicsWorld::carry_speed_scale() const noexcept {
    const float mass = carried_load_mass();
    if (mass <= 0.0F) {
        return 1.0F;
    }
    const float weight = mass * physics_system_.GetGravity().Length();
    return std::clamp(1.0F - weight / (kCarryGripShare * kGripNewtons), 0.0F, 1.0F);
}

float Simulation::PhysicsWorld::carry_ground_acceleration() const noexcept {
    const float mass = carried_load_mass();
    if (mass <= 0.0F) {
        return kGroundAcceleration;
    }
    const float weight = mass * physics_system_.GetGravity().Length();
    const float grip = kCarryGripShare * kGripNewtons;
    const float spare = grip * grip - weight * weight;
    const float limit = spare > 0.0F ? std::sqrt(spare) / (2.0F * mass) : 0.0F;
    return std::clamp(limit, kCarryMinimumAcceleration, kGroundAcceleration);
}

void Simulation::PhysicsWorld::update_carry(JPH::BodyInterface &bodies, const StepCommands &commands,
                                            const float delta_seconds) noexcept {
    if (carry_constraint_ != nullptr) {
        if (commands.set_down_requested) {
            release_carry();
            return;
        }
        if (commands.throw_requested && holding_throwable()) {
            throw_carry(bodies);
            return;
        }
        // The hands swing round to the facing, drawing a handle just taken
        // in to them at kCarryPullInSpeed.
        const float pull_in = kCarryPullInSpeed * delta_seconds;
        const float gap = carry_entry_offset_.Length();
        carry_entry_offset_ =
            gap > pull_in ? carry_entry_offset_ * ((gap - pull_in) / gap) : JPH::Vec3::sZero();
        const JPH::Vec3 offset =
            carry_hand_offset(carry_hand_bearing(bodies, delta_seconds)) + carry_entry_offset_;
        carry_constraint_->SetPoint1(JPH::EConstraintSpace::LocalToBodyCOM,
                                     JPH::RVec3(offset));
        const JPH::RVec3 hand = bodies.GetPosition(player_id_) + offset;
        const JPH::RVec3 handle =
            bodies.GetCenterOfMassTransform(carried_id_) * carry_handle(carried_entity_);
        const JPH::Vec3 apart(handle - hand);
        const JPH::Vec3 separating_velocity = bodies.GetPointVelocity(carried_id_, handle) -
                                              bodies.GetLinearVelocity(player_id_);
        if (apart.Length() > kCarrySlipDistance && apart.Dot(separating_velocity) > 0.0F) {
            release_carry();
            return;
        }
        // The grip: the force the hands put through the carry on the
        // last step.
        if (scraperx::sim::kit::is_kit_entity(carried_entity_)) {
            const float pull =
                carry_constraint_->GetTotalLambdaPosition().Length() / delta_seconds;
            grip_over_seconds_ = pull > kGripNewtons ? grip_over_seconds_ + delta_seconds : 0.0F;
            if (grip_over_seconds_ >= kGripSeconds) {
                release_carry();
                return;
            }
        }
        // Both hands steady a free load's swing: hung from one point 0.22 m
        // under them, D's 50 kg spool swung for a minute after every stop,
        // and let go mid-swing it landed on a flange's edge and tumbled out
        // of its gap. Its motion relative to the hand point (the pendulum's,
        // and its turning) is taken down at kCarrySwingRate, as velocity and
        // not force: a torque on it went through the hold and shoved the
        // carrier a metre sideways. Not while a load just taken is still
        // being drawn in to the hands: that is the hold's own motion, and
        // taking it down each tick read as a pull past the grip, which let go.
        if (carried_load_mass() > 0.0F && carry_entry_offset_.IsNearZero()) {
            const float damp = std::min(1.0F, kCarrySwingRate * delta_seconds);
            const JPH::Vec3 hand_velocity =
                bodies.GetLinearVelocity(player_id_) + (offset - carry_offset_last_) / delta_seconds;
            const JPH::Vec3 relative = bodies.GetLinearVelocity(carried_id_) - hand_velocity;
            bodies.SetLinearVelocity(carried_id_, hand_velocity + relative * (1.0F - damp));
            bodies.SetAngularVelocity(carried_id_, bodies.GetAngularVelocity(carried_id_) * (1.0F - damp));
        }
        carry_offset_last_ = offset;
        hold_load_yaw(bodies);
        return;
    }
    if (commands.pick_up_requested) {
        std::uint64_t entity = 0;
        const JPH::BodyID id = carry_candidate(bodies, entity);
        if (!id.IsInvalid()) {
            attach_carry(id, entity);
        }
    }
}

// Loose stock in the hands, other than a machine's own weight (S3's pig: a
// route part is set down, never thrown away); a fabricated object is loose
// stock too.
bool Simulation::PhysicsWorld::holding_throwable() const noexcept {
    if (scraperx::sim::kit::is_fabricated_entity(carried_entity_)) {
        return carried_load_mass() > 0.0F;
    }
    const auto count = static_cast<std::uint64_t>(loose_stock_->count());
    const std::uint64_t weights_end = count - static_cast<std::uint64_t>(LooseStock::kMatronThrowables);
    const std::uint64_t weights_begin = weights_end - static_cast<std::uint64_t>(LooseStock::kMachineWeights);
    if (carried_load_mass() <= 0.0F || carried_entity_ < LooseStock::kFirstEntity ||
        carried_entity_ >= LooseStock::kFirstEntity + count) {
        return false;
    }
    const std::uint64_t piece = carried_entity_ - LooseStock::kFirstEntity;
    return piece < weights_begin || piece >= weights_end;
}

namespace {

// What a thrown piece meets, as the aim and its drawn path see it: what a
// moving body collides with (not a corpse's parts, not a sensor), never the
// thrower or the piece itself.
class ThrowMeets final : public JPH::BodyFilter {
public:
    ThrowMeets(const JPH::BodyID player, const JPH::BodyID load) : player_(player), load_(load) {}
    bool ShouldCollide(const JPH::BodyID &id) const override { return id != player_ && id != load_; }
    bool ShouldCollideLocked(const JPH::Body &body) const override { return !body.IsSensor(); }

private:
    JPH::BodyID player_;
    JPH::BodyID load_;
};

} // namespace

JPH::Vec3 Simulation::PhysicsWorld::throw_velocity(const JPH::BodyInterface &bodies) const noexcept {
    const float mass = carried_load_mass();
    if (mass <= 0.0F) {
        return JPH::Vec3::sZero();
    }
    const float speed = std::min(kThrowMaxMps, std::sqrt(2.0F * kThrowEnergyJ / mass));
    // Toward what the eyes are on (from the load in the hands, not along
    // the view from wherever the hands are), raised for the arc.
    const JPH::Vec3 facing = JPH::Vec3(facing_.GetX(), 0.0F, facing_.GetZ()).NormalizedOr(JPH::Vec3::sAxisZ());
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const JPH::RVec3 eye(centre.GetX(), centre.GetY() + kPlayerHalfHeight - 0.15F, centre.GetZ());
    const float cos_p = std::cos(look_pitch_);
    const JPH::Vec3 look(facing.GetX() * cos_p, std::sin(look_pitch_), facing.GetZ() * cos_p);
    const ThrowMeets meets(player_id_, carried_id_);
    const JPH::RRayCast sight(eye, look * kThrowAimM);
    JPH::RayCastResult hit;
    const JPH::RVec3 aim = physics_system_.GetNarrowPhaseQuery().CastRay(
                               sight, hit, physics_system_.GetDefaultBroadPhaseLayerFilter(object_layers::kMoving),
                               physics_system_.GetDefaultLayerFilter(object_layers::kMoving), meets)
                               ? sight.GetPointOnRay(hit.mFraction)
                               : eye + look * kThrowAimM;
    const JPH::Vec3 to_aim(aim - bodies.GetCenterOfMassPosition(carried_id_));
    JPH::Vec3 flat(to_aim.GetX(), 0.0F, to_aim.GetZ());
    const float ahead = flat.Dot(facing);
    if (ahead < kThrowAimPastM) {
        flat = facing;
    } else {
        flat = flat.Normalized();
        if (flat.Dot(facing) < std::cos(kThrowAimSpreadRad)) {
            const JPH::Vec3 side = (flat - facing * flat.Dot(facing)).NormalizedOr(JPH::Vec3::sZero());
            flat = facing * std::cos(kThrowAimSpreadRad) + side * std::sin(kThrowAimSpreadRad);
        }
    }
    const float pitch = std::clamp(look_pitch_ + kThrowRaiseRad, -0.9F, 1.2F);
    const JPH::Vec3 along = flat * std::cos(pitch) + JPH::Vec3::sAxisY() * std::sin(pitch);
    return bodies.GetLinearVelocity(player_id_) + along * speed;
}

// The hands open as the arms finish: the piece flies off on the throw's
// velocity, turning end over end, and is not heard as a hit at the hands.
void Simulation::PhysicsWorld::throw_carry(JPH::BodyInterface &bodies) noexcept {
    const float mass = carried_load_mass();
    const JPH::Vec3 velocity = throw_velocity(bodies);
    const JPH::BodyID thrown = carried_id_;
    const std::uint64_t entity = carried_entity_;
    release_carry();
    // Its way across the ground: end over end about the line across it, and
    // the thrower pushed back along it.
    const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    const JPH::Vec3 flat = JPH::Vec3(velocity.GetX() - player_velocity.GetX(), 0.0F, velocity.GetZ() - player_velocity.GetZ())
                               .NormalizedOr(JPH::Vec3(facing_.GetX(), 0.0F, facing_.GetZ()).NormalizedOr(JPH::Vec3::sAxisZ()));
    const JPH::Vec3 across(flat.GetZ(), 0.0F, -flat.GetX());
    bodies.SetLinearAndAngularVelocity(thrown, velocity, across * kThrowSpinRadS);
    impacts_->resync_body(entity);
    // A heave heavier than the arms take without the feet: the rest of it
    // pushes the thrower back.
    const float impulse = mass * (velocity - player_velocity).Length();
    if (impulse > kThrowFreeImpulseNs) {
        bodies.SetLinearVelocity(player_id_, player_velocity - flat * ((impulse - kThrowFreeImpulseNs) / kPlayerMassKg));
    }
    ++throw_count_;
}

// The throw's path from the hands now, cut at the first thing it would hit.
std::vector<Vector3> Simulation::PhysicsWorld::throw_prediction() const {
    std::vector<Vector3> result;
    if (!holding_throwable()) {
        return result;
    }
    const JPH::BodyInterface &bodies = physics_system_.GetBodyInterfaceNoLock();
    JPH::RVec3 at = bodies.GetCenterOfMassPosition(carried_id_);
    JPH::Vec3 velocity = throw_velocity(bodies);
    const JPH::Vec3 gravity = physics_system_.GetGravity();
    const ThrowMeets meets(player_id_, carried_id_);
    const auto broad = physics_system_.GetDefaultBroadPhaseLayerFilter(object_layers::kMoving);
    const auto layers = physics_system_.GetDefaultLayerFilter(object_layers::kMoving);
    const auto to_vector = [](const JPH::RVec3 p) {
        return Vector3{static_cast<double>(p.GetX()), static_cast<double>(p.GetY()), static_cast<double>(p.GetZ())};
    };
    result.push_back(to_vector(at));
    for (float t = 0.0F; t < kThrowPreviewSeconds; t += kThrowPreviewStep) {
        const JPH::Vec3 step = velocity * kThrowPreviewStep + gravity * (0.5F * kThrowPreviewStep * kThrowPreviewStep);
        JPH::RayCastResult hit;
        if (physics_system_.GetNarrowPhaseQuery().CastRay(JPH::RRayCast(at, step), hit, broad, layers, meets)) {
            result.push_back(to_vector(at + step * hit.mFraction));
            break;
        }
        at += step;
        velocity += gravity * kThrowPreviewStep;
        result.push_back(to_vector(at));
    }
    return result;
}

// Checkpoint reconciliation: after the bodies' poses are restored, make
// the carry match what was committed.
void Simulation::PhysicsWorld::restore_carry_topology(const std::uint64_t committed_entity) noexcept {
    if (committed_entity == carried_entity_) {
        return;
    }
    release_carry();
    if (scraperx::sim::kit::is_kit_entity(committed_entity)) {
        const scraperx::sim::kit::BodyIndex body = kit_->body_for_entity(committed_entity);
        if (kit_->body_enabled(body)) {
            attach_carry(kit_->body_id(body), committed_entity);
        }
    }
}

// What the rig command would do now, from poses and the carry alone.
void Simulation::PhysicsWorld::find_rig_action(const JPH::BodyInterface &bodies) noexcept {
    rig_action_ = 0;
    rig_target_entity_ = 0;
    rig_anchor_ = scraperx::sim::kit::AnchorIndex{};
    rig_rope_ = scraperx::sim::kit::RopeIndex{};
    if (traversal_state_ != TraversalState::None) {
        return;
    }
    if (carry_constraint_ != nullptr) {
        if (kit_->carry_kind(carried_entity_) != scraperx::sim::kit::CarryKind::Shackle) {
            return;
        }
        rig_anchor_ = kit_->hook_target(carried_entity_);
        if (rig_anchor_.valid()) {
            rig_action_ = 1;
            rig_target_entity_ = kit_->anchor_entity(rig_anchor_);
        }
        return;
    }
    if (!grounded_) {
        return;
    }
    rig_rope_ = kit_->unhook_target(hand_position(bodies));
    if (rig_rope_.valid()) {
        rig_action_ = 2;
        rig_target_entity_ = kit_->rope_shackle_entity(rig_rope_);
    }
}

// Hook: the shackle leaves the hands and the rope's end goes onto the
// anchor. Unhook: the end comes off the anchor onto its shackle, in the
// hands.
void Simulation::PhysicsWorld::update_rig(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (!commands.rig_requested) {
        return;
    }
    find_rig_action(bodies);
    if (rig_action_ == 1) {
        const std::uint64_t shackle = carried_entity_;
        release_carry();
        (void)kit_->hook(shackle, rig_anchor_);
    } else if (rig_action_ == 2) {
        const std::uint64_t shackle = kit_->unhook(rig_rope_);
        const scraperx::sim::kit::BodyIndex body = kit_->body_for_entity(shackle);
        if (kit_->body_enabled(body)) {
            attach_carry(kit_->body_id(body), shackle);
        }
    }
    find_rig_action(bodies);
}

// Crouch and stand (GDD 7.2, Governing Law 4). The body swaps capsules
// with its soles fixed: the centre moves by kCrouchDrop, nothing under
// the feet does. It crouches only from the ground and never inside a
// traversal (every traversal pose is a standing pose), and it stands
// only where the full standing capsule is clear, so a gap entered
// crouched keeps the body crouched until it is out of it. A Jump or a
// traversal request asks to stand first; where the body cannot, they
// are refused further down this tick.
void Simulation::PhysicsWorld::update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (traversal_state_ != TraversalState::None) {
        return;
    }
    const bool wants_up = !rolling() &&
        (!commands.crouch_held || commands.jump_requested || commands.traversal_requested);
    if (crouched_) {
        if (grounded_) {
            tucked_ = false;   // landed tucked: an ordinary crouch now
        }
        if (wants_up) {
            if (tucked_) {
                (void)try_untuck(bodies);
            } else {
                (void)try_stand(bodies);
            }
        }
        return;
    }
    if (!wants_up && !grounded_ && carry_constraint_ == nullptr &&
        bodies.GetLinearVelocity(player_id_).GetY() - airborne_inherited_velocity_.GetY() >= -kTuckMaximumFallMps) {
        // The tuck: the legs drawn up, the head where it was. The short
        // capsule lies inside the tall one, so there is always room.
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        bodies.SetShape(player_id_, player_crouch_shape_.GetPtr(), false, JPH::EActivation::Activate);
        bodies.SetPosition(player_id_, at + JPH::Vec3(0.0F, kCrouchDrop, 0.0F), JPH::EActivation::Activate);
        crouched_ = true;
        tucked_ = true;
        ++tuck_count_;
        return;
    }
    if (!wants_up && grounded_) {
        begin_slide_if_running(bodies);
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        bodies.SetShape(player_id_, player_crouch_shape_.GetPtr(), false,
                        JPH::EActivation::Activate);
        bodies.SetPosition(player_id_, at - JPH::Vec3(0.0F, kCrouchDrop, 0.0F),
                           JPH::EActivation::Activate);
        crouched_ = true;
    }
}

namespace {
// The hook rack on the 220 ring's south band, and the member over the well
// its hook is set for: where a hook fired from the rack first bites, over
// well C's dumpster.
const JPH::Vec3 kHookRackBase(-22.4F, 220.25F, -127.6F);
const JPH::Vec3 kHookPracticeMember(-10.15F, 224.5F, -131.75F);
} // namespace

JPH::AABox Simulation::PhysicsWorld::hook_practice_ground() noexcept {
    JPH::AABox ground = JPH::AABox::sFromTwoPoints(kHookRackBase, kHookPracticeMember);
    ground.ExpandBy(JPH::Vec3(2.0F, 0.0F, 2.0F));
    return ground;
}

// The rack on the 220 ring (deck top 220.25) and the hook gun on its shelf.
void Simulation::PhysicsWorld::build_hook_rack() {
    const JPH::RVec3 base(kHookRackBase);
    std::vector<kit::Part> rack;
    const auto box = [](const JPH::Vec3 half, const JPH::Vec3 offset, const kit::Material material) {
        kit::Part part;
        part.half = half;
        part.offset = offset;
        part.material = material;
        return part;
    };
    rack.push_back(box(JPH::Vec3(0.08F, 0.75F, 0.08F), JPH::Vec3(0.0F, 0.75F, -0.3F), kit::Material::Rust));
    rack.push_back(box(JPH::Vec3(0.08F, 0.75F, 0.08F), JPH::Vec3(0.0F, 0.75F, 0.3F), kit::Material::Rust));
    rack.push_back(box(JPH::Vec3(0.22F, 0.03F, 0.42F), JPH::Vec3(0.0F, 1.02F, 0.0F), kit::Material::Steel));
    rack.push_back(box(JPH::Vec3(0.02F, 0.35F, 0.42F), JPH::Vec3(-0.2F, 1.35F, 0.0F), kit::Material::Hazard));
    (void)kit_->add_body(Simulation::kHookRackFrameEntityId, rack, base, JPH::Quat::sIdentity(), 0.0F, 0.8F);
    std::vector<kit::Part> gun;
    gun.push_back(box(JPH::Vec3(0.045F, 0.06F, 0.22F), JPH::Vec3::sZero(), kit::Material::Yellow));      // body
    gun.push_back(box(JPH::Vec3(0.03F, 0.03F, 0.16F), JPH::Vec3(0.0F, 0.02F, 0.36F), kit::Material::Steel));  // barrel
    gun.push_back(box(JPH::Vec3(0.09F, 0.02F, 0.03F), JPH::Vec3(0.0F, 0.02F, 0.53F), kit::Material::Steel));  // tines
    gun.push_back(box(JPH::Vec3(0.03F, 0.08F, 0.035F), JPH::Vec3(0.0F, -0.1F, -0.08F), kit::Material::Rubble)); // grip
    hook_gun_ = kit_->add_body(Simulation::kHookGunEntityId, gun, base + JPH::Vec3(0.0F, 1.13F, 0.0F),
                               JPH::Quat::sIdentity(), 3.0F, 0.9F);
}

bool Simulation::PhysicsWorld::hook_take_in_reach(const JPH::BodyInterface &bodies) const noexcept {
    if (has_hook_) {
        return false;
    }
    const JPH::RVec3 player = bodies.GetPosition(player_id_);
    if (!hook_rack_id_.IsInvalid() &&
        JPH::Vec3(bodies.GetPosition(hook_rack_id_) - player).Length() <= kHookRackReach) {
        return true;
    }
    return hook_gun_.valid() && kit_->body_enabled(hook_gun_) &&
           JPH::Vec3(kit_->body_position(hook_gun_) - player).Length() <= kHookRackReach;
}

// The hook is had once its gun is off the rack (parked).
void Simulation::PhysicsWorld::sync_hook_from_rack() noexcept {
    if (hook_gun_.valid() && !kit_->body_enabled(hook_gun_)) {
        has_hook_ = true;
    } else if (has_hook_ && hook_gun_.valid()) {
        kit_->park(hook_gun_);   // taken since the checkpoint: still taken
    }
}

void Simulation::PhysicsWorld::hook_pre_step(JPH::BodyInterface &bodies, const StepCommands &commands, const float dt) noexcept {
    if (commands.hook_take && !has_hook_) {
        const JPH::RVec3 player = bodies.GetPosition(player_id_);
        if (!hook_rack_id_.IsInvalid() &&
            JPH::Vec3(bodies.GetPosition(hook_rack_id_) - player).Length() <= kHookRackReach) {
            has_hook_ = true;
        } else if (hook_gun_.valid() && kit_->body_enabled(hook_gun_) &&
                   JPH::Vec3(kit_->body_position(hook_gun_) - player).Length() <= kHookRackReach) {
            kit_->park(hook_gun_);
            has_hook_ = true;
        }
    }
    if (!has_hook_) {
        return;
    }
    if (commands.hook_release && hook_state_ != HookState::Stowed) {
        stow_hook(bodies);
        return;
    }
    if (commands.hook_fire && hook_state_ == HookState::Stowed && traversal_state_ == TraversalState::None) {
        fire_hook(bodies, JPH::Vec3(static_cast<float>(commands.hook_aim_x), static_cast<float>(commands.hook_aim_y),
                                    static_cast<float>(commands.hook_aim_z)));
        return;
    }
    if (hook_state_ == HookState::Bitten && hook_line_ != nullptr) {
        // The reel is a winch on the free line, chest to the nearest bend (or
        // the bite): what lies laid round the structure beyond stays laid.
        // Turning, it takes the line in at kHookReelSpeedMps with at most
        // kHookReelForceN and pays none out (the line's length follows it
        // in); past that force it stalls and holds. Let go, it holds.
        const float lies = JPH::Vec3(hook_anchor(bodies) - hook_chest(bodies)).Length();
        if (commands.hook_reel && lies > kHookLineShortestM) {
            hook_line_->SetMaxLength(std::max(kHookLineShortestM, std::min(hook_line_->GetMaxLength(), lies)));
            hook_line_->SetMotor(kHookReelSpeedMps, kHookReelForceN);
            if (-hook_line_->GetMotorLambda() >= kHookReelForceN * dt * 0.999F) {
                ++hook_stall_ticks_;
            }
        } else {
            hook_line_->SetMotor(0.0F, 0.0F);
        }
    }
}

void Simulation::PhysicsWorld::fire_hook(JPH::BodyInterface &bodies, JPH::Vec3 aim) noexcept {
    if (aim.IsNearZero()) {
        return;
    }
    aim = aim.Normalized();
    const JPH::RVec3 start = hook_chest(bodies) + JPH::Vec3(0.0F, 0.3F, 0.0F) + aim * (kPlayerRadius + 0.3F);
    hook_body_ = add_shape_body(bodies, new JPH::SphereShape(kHookRadius), start, JPH::Quat::sIdentity(),
                                JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
                                Simulation::kHookEntityId, kHookMassKg)
                     ->GetID();
    bodies.SetMotionQuality(hook_body_, JPH::EMotionQuality::LinearCast);
    bodies.SetLinearVelocity(hook_body_, bodies.GetLinearVelocity(player_id_) + aim * kHookLaunchMps);
    hook_bend_count_ = 0;
    attach_hook_line(bodies, hook_body_, start, kHookLineMaxM);
    hook_state_ = HookState::Flying;
    hook_flight_seconds_ = 0.0;
    hook_tension_n_ = 0.0F;
    ++hook_fire_count_;
}

void Simulation::PhysicsWorld::stow_hook(JPH::BodyInterface &bodies) noexcept {
    if (hook_line_ != nullptr) {
        physics_system_.RemoveConstraint(hook_line_);
        hook_line_ = nullptr;
    }
    if (!hook_body_.IsInvalid()) {
        bodies.RemoveBody(hook_body_);
        bodies.DestroyBody(hook_body_);
        hook_body_ = {};
    }
    hook_state_ = HookState::Stowed;
    hook_tension_n_ = 0.0F;
    hook_bend_count_ = 0;
}

JPH::RVec3 Simulation::PhysicsWorld::closest_on_hold(const LeafBox &box, const std::uint32_t long_axis,
                                                     const JPH::RVec3 point) noexcept {
    const JPH::Vec3 along = box.axes[long_axis];
    const float t = std::clamp(JPH::Vec3(point - box.centre).Dot(along), -box.half[long_axis], box.half[long_axis]);
    return box.centre + along * t;
}

// Where the line ends: the hook in flight, the bite once bitten.
JPH::RVec3 Simulation::PhysicsWorld::hook_end(const JPH::BodyInterface &bodies) const noexcept {
    if (hook_state_ == HookState::Bitten) {
        return from_support_local(bodies, hook_bite_target_, hook_bite_local_);
    }
    return hook_body_.IsInvalid() ? JPH::RVec3::sZero() : bodies.GetPosition(hook_body_);
}

void Simulation::PhysicsWorld::hook_post_step(JPH::BodyInterface &bodies, const float dt) noexcept {
    if (hook_state_ == HookState::Stowed) {
        return;
    }
    hook_tension_n_ = hook_line_ != nullptr
                          ? std::abs(hook_line_->GetTotalLambdaPosition() + hook_line_->GetMotorLambda()) / dt
                          : 0.0F;
    if (hook_state_ == HookState::Bitten) {
        // The tines' hold on the member (kHookHoldForceN).
        if (hook_tension_n_ > kHookHoldForceN) {
            ++hook_over_ticks_;
            hook_over_impulse_ += (hook_tension_n_ - kHookHoldForceN) * dt;
        } else {
            hook_over_ticks_ = 0;
            hook_over_impulse_ = 0.0F;
        }
        if (hook_over_ticks_ >= kHookSlipTicks || hook_over_impulse_ > kHookTearImpulseNs) {
            tear_hook(bodies);
            return;
        }
        bend_hook_line(bodies);
        return;
    }
    hook_flight_seconds_ += dt;
    // The tines bite round a hold they strike.
    const JPH::RVec3 at = bodies.GetPosition(hook_body_);
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
    const JPH::IgnoreSingleBodyFilter not_player(player_id_);
    const JPH::IgnoreSingleBodyFilterChained filter(hook_body_, not_player);
    const JPH::SphereShape reach(kHookBiteReach);
    physics_system_.GetNarrowPhaseQuery().CollideShape(&reach, JPH::Vec3::sReplicate(1.0F),
                                                       JPH::RMat44::sTranslation(at), JPH::CollideShapeSettings(),
                                                       at, hits, {}, {}, filter);
    for (const JPH::CollideShapeResult &hit : hits.mHits) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), hit.mBodyID2);
        if (!lock.Succeeded()) {
            continue;
        }
        const JPH::Body &body = lock.GetBody();
        if (body.IsSensor() ||
            (body.IsDynamic() && body.GetMotionProperties()->GetInverseMass() * kGripMinBodyMassKg > 1.0F)) {
            continue;
        }
        LeafBox box;
        std::uint32_t long_axis = 0;
        if (!leaf_box(body, hit.mSubShapeID2, box) || !box_is_hold(box, long_axis)) {
            continue;
        }
        const JPH::BodyID held = hit.mBodyID2;
        // Bitten, the hook is part of the member: the line runs from the
        // chest to the bite on it (a 1.5 kg hook between an 80 kg body and
        // the structure would only be the solver's weak link).
        const JPH::RVec3 bite = closest_on_hold(box, long_axis, at);
        hook_bite_target_ = held;
        hook_bite_local_ = to_support_local(bodies, held, bite);
        const float length = std::min(kHookLineMaxM, JPH::Vec3(bite - hook_chest(bodies)).Length());
        physics_system_.RemoveConstraint(hook_line_);
        hook_line_ = nullptr;
        bodies.RemoveBody(hook_body_);
        bodies.DestroyBody(hook_body_);
        hook_body_ = {};
        hook_bend_count_ = 0;
        hook_last_chest_ = hook_chest(bodies);
        attach_hook_line(bodies, held, bite, length);
        hook_state_ = HookState::Bitten;
        hook_over_ticks_ = 0;
        hook_over_impulse_ = 0.0F;
        ++hook_bite_count_;
        return;
    }
    if (hook_flight_seconds_ > kHookMissSeconds) {
        stow_hook(bodies);   // no bite: reeled back in
    }
}

// The tines come off the member: the hook springs back along the line, the
// way it was pulled, clear of the member, moving with it plus
// kHookSpringBackMps, on the whole rope that was out (what lay wrapped round
// the structure pays off). It flies as a fired hook does from there.
void Simulation::PhysicsWorld::tear_hook(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 bite = hook_end(bodies);
    const JPH::RVec3 toward = hook_bend_count_ > 0 ? hook_bend_point(bodies, 0) : hook_chest(bodies);
    JPH::Vec3 back(toward - bite);
    back = back.IsNearZero() ? JPH::Vec3::sAxisY() : back.Normalized();
    float rope = hook_line_ != nullptr ? hook_line_->GetMaxLength() : kHookLineMaxM;
    for (std::uint32_t bend = 0; bend < hook_bend_count_; ++bend) {
        rope += hook_bends_[bend].laid_m;
    }
    const JPH::Vec3 carried = bodies.GetPointVelocity(hook_bite_target_, bite);
    const JPH::RVec3 start = bite + back * kHookSpringBackClearM;
    hook_body_ = add_shape_body(bodies, new JPH::SphereShape(kHookRadius), start, JPH::Quat::sIdentity(),
                                JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
                                Simulation::kHookEntityId, kHookMassKg)
                     ->GetID();
    bodies.SetMotionQuality(hook_body_, JPH::EMotionQuality::LinearCast);
    bodies.SetLinearVelocity(hook_body_, carried + back * kHookSpringBackMps);
    hook_bend_count_ = 0;
    attach_hook_line(bodies, hook_body_, start, std::min(kHookLineMaxM, std::max(rope, kHookLineShortestM)));
    hook_state_ = HookState::Flying;
    hook_flight_seconds_ = 0.0;
    hook_over_ticks_ = 0;
    hook_over_impulse_ = 0.0F;
    hook_bite_target_ = {};
    ++hook_tear_count_;
}

// The line from the chest to `at` on `anchor`, at most `max_length` long:
// the kit's rope run direct (rope_constraint.hpp), solved every step, so a
// body hung at the line's length never drops its load for a tick at
// altitude (Jolt's distance constraint, which it was before, sat out any
// step it began a hair slack). Replaces the line there was.
void Simulation::PhysicsWorld::attach_hook_line(JPH::BodyInterface &bodies, const JPH::BodyID anchor,
                                                const JPH::RVec3 at, const float max_length) noexcept {
    if (hook_line_ != nullptr) {
        physics_system_.RemoveConstraint(hook_line_);
        hook_line_ = nullptr;
    }
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = hook_chest(bodies);
    settings.mBodyPoint2 = at;
    settings.mFixedPoint1 = at;
    settings.mFixedPoint2 = at;
    settings.mMinLength = 0.0F;
    settings.mMaxLength = max_length;
    const JPH::BodyID ids[2] = {player_id_, anchor};
    JPH::BodyLockMultiWrite lock(physics_system_.GetBodyLockInterface(), ids, 2);
    JPH::Body *chest = lock.GetBody(0);
    JPH::Body *held = lock.GetBody(1);
    if (chest == nullptr || held == nullptr) {
        return;
    }
    hook_line_ = new kit::RopeConstraint(*chest, *held, settings, true);
    physics_system_.AddConstraint(hook_line_);
}

JPH::RVec3 Simulation::PhysicsWorld::hook_bend_point(const JPH::BodyInterface &bodies,
                                                     const std::uint32_t bend) const noexcept {
    return from_support_local(bodies, hook_bends_[bend].body, hook_bends_[bend].local);
}

// Where the free line ends: the nearest bend, or the hook.
JPH::RVec3 Simulation::PhysicsWorld::hook_anchor(const JPH::BodyInterface &bodies) const noexcept {
    return hook_bend_count_ > 0 ? hook_bend_point(bodies, hook_bend_count_ - 1) : hook_end(bodies);
}

// Whether the straight line from `from` to `to` passes through a body the
// rope catches on (static, or as heavy as a hold's body): the player, the
// hook, sensors and light loose bodies do not take it, and a hit within
// kHookWrapEndM of `to` is the member the line is fast to there.
bool Simulation::PhysicsWorld::hook_line_blocked(const JPH::RVec3 from, const JPH::RVec3 to,
                                                 JPH::RayCastResult &hit) const noexcept {
    class Takes final : public JPH::BodyFilter {
    public:
        Takes(const JPH::BodyID player, const JPH::BodyID hook) : player_(player), hook_(hook) {}
        bool ShouldCollide(const JPH::BodyID &id) const override { return id != player_ && id != hook_; }
        bool ShouldCollideLocked(const JPH::Body &body) const override {
            return !body.IsSensor() &&
                   (!body.IsDynamic() ||
                    body.GetMotionProperties()->GetInverseMass() * kGripMinBodyMassKg <= 1.0F);
        }

    private:
        JPH::BodyID player_;
        JPH::BodyID hook_;
    };
    const JPH::Vec3 run(to - from);
    const float length = run.Length();
    if (length <= kHookWrapEndM) {
        return false;
    }
    hit.Reset();
    const Takes takes(player_id_, hook_body_);
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(JPH::RRayCast(from, run), hit, {}, {}, takes)) {
        return false;
    }
    return hit.mFraction * length < length - kHookWrapEndM;
}

// The rope against the structure, once a tick while the hook is bitten:
// off the nearest bend if the line has swung back round it, then onto any
// edge the free line now crosses (see kHookBendsMax).
void Simulation::PhysicsWorld::bend_hook_line(JPH::BodyInterface &bodies) noexcept {
    if (hook_line_ == nullptr) {
        return;
    }
    const JPH::RVec3 chest = hook_chest(bodies);
    JPH::RayCastResult hit;
    if (hook_bend_count_ > 0) {
        const HookBend &bend = hook_bends_[hook_bend_count_ - 1];
        const JPH::RVec3 at = hook_bend_point(bodies, hook_bend_count_ - 1);
        const bool first = hook_bend_count_ == 1;
        const JPH::RVec3 before = first ? hook_end(bodies) : hook_bend_point(bodies, hook_bend_count_ - 2);
        const JPH::Vec3 turn = JPH::Vec3(at - before).Cross(JPH::Vec3(chest - at));
        if (turn.Dot(bend.turn) <= 0.0F && !hook_line_blocked(chest, before, hit)) {
            const float free = hook_line_->GetMaxLength() + bend.laid_m;
            const JPH::BodyID body = first ? hook_bite_target_ : hook_bends_[hook_bend_count_ - 2].body;
            --hook_bend_count_;
            attach_hook_line(bodies, body, before, free);
            ++hook_unwrap_count_;
        }
    }
    const JPH::RVec3 anchor = hook_anchor(bodies);
    if (hook_line_ != nullptr && hook_line_blocked(chest, anchor, hit)) {
        if (hook_bend_count_ >= kHookBendsMax) {
            ++hook_bends_refused_;
        } else {
            // Where the line, swept from the chest's last place to this one,
            // first met the body: its edge.
            JPH::RVec3 from = chest;
            JPH::RayCastResult edge = hit;
            JPH::RayCastResult probe;
            if (!hook_line_blocked(hook_last_chest_, anchor, probe)) {
                float clear = 0.0F;
                float blocked = 1.0F;
                for (int step = 0; step < kHookWrapSearchSteps; ++step) {
                    const float mid = 0.5F * (clear + blocked);
                    const JPH::RVec3 at = hook_last_chest_ + JPH::Vec3(chest - hook_last_chest_) * mid;
                    if (hook_line_blocked(at, anchor, probe)) {
                        blocked = mid;
                        from = at;
                        edge = probe;
                    } else {
                        clear = mid;
                    }
                }
            }
            const JPH::Vec3 run(anchor - from);
            const JPH::RVec3 point = JPH::RRayCast(from, run).GetPointOnRay(edge.mFraction);
            const JPH::RVec3 bend_at = point + surface_normal(edge.mBodyID, edge.mSubShapeID2, point) * kHookWrapStandoffM;
            const float laid = JPH::Vec3(anchor - bend_at).Length();
            const float free = hook_line_->GetMaxLength() - laid;
            if (laid > kHookWrapEndM && free > 0.0F) {
                HookBend &bend = hook_bends_[hook_bend_count_];
                bend.body = edge.mBodyID;
                bend.local = to_support_local(bodies, edge.mBodyID, bend_at);
                bend.turn = JPH::Vec3(bend_at - anchor).Cross(JPH::Vec3(chest - bend_at));
                bend.laid_m = laid;
                ++hook_bend_count_;
                attach_hook_line(bodies, edge.mBodyID, bend_at, free);
                ++hook_wrap_count_;
            }
        }
    }
    hook_last_chest_ = chest;
}

} // namespace scraperx::sim
