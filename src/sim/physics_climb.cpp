#include "sim/physics_internal.hpp"

// Grip, free climbing, the pitman bar, and topping out.

namespace scraperx::sim {

bool Simulation::PhysicsWorld::cast_ray(const JPH::RVec3 origin,
                            const JPH::Vec3 direction,
                            JPH::RayCastResult &hit) const noexcept {
    const JPH::RRayCast ray(origin, direction);
    hit.Reset();
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    return physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter);
}

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

void Simulation::PhysicsWorld::release_pitman_hang(JPH::BodyInterface &bodies) noexcept {
    JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    const float speed = velocity.Length();
    if (speed > kReleaseSpeedCap) {
        velocity *= kReleaseSpeedCap / speed;
    }
    if (pitman_grip_ != nullptr) {
        physics_system_.RemoveConstraint(pitman_grip_);
        pitman_grip_ = nullptr;
    }
    bodies.SetLinearVelocity(player_id_, velocity);
    airborne_inherited_velocity_ = JPH::Vec3(velocity.GetX(), 0.0F, velocity.GetZ());
    bodies.SetGravityFactor(player_id_, 1.0F);
    regrab_lockout_ticks_ = kPitmanRegrabTicks;
    clear_traversal();
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                               JPH::Vec3(velocity.GetX(), 0.0F, velocity.GetZ()).Length());
}

void Simulation::PhysicsWorld::begin_pitman_hang(JPH::BodyInterface &bodies, const Grip &grip) noexcept {
    JPH::RVec3 pivot;
    if (kit_ == nullptr || !kit_->hinge_pivot(grip.entity_id, pivot)) {
        ++rejected_traversal_count_;
        return;
    }
    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = grip.point;
    settings.mPoint2 = grip.point;
    pitman_grip_ = static_cast<JPH::PointConstraint *>(
        create_constraint(settings, player_id_, grip.body, false));
    if (pitman_grip_ == nullptr) {
        ++rejected_traversal_count_;
        return;
    }
    const JPH::RVec3 hold = bodies.GetPosition(player_id_);
    pitman_pivot_ = pivot;
    traversal_state_ = TraversalState::Hanging;
    traversal_body_ = grip.body;
    traversal_entity_id_ = grip.entity_id;
    traversal_target_body_ = grip.body;
    traversal_normal_ = facing_;
    traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, grip.point);
    traversal_local_target_ = traversal_local_ledge_;
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    swing_angle_ = 0.0F;
    swing_rate_ = 0.0F;
    traversal_desired_ = hold;
    bodies.SetGravityFactor(player_id_, 0.0F);
    bodies.ActivateBody(grip.body);
}

void Simulation::PhysicsWorld::pump_pitman(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (pitman_grip_ == nullptr || traversal_body_.IsInvalid()) {
        return;
    }
    const JPH::RVec3 grip = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::Vec3 radius(grip - pitman_pivot_);
    const JPH::Vec3 tangent = JPH::Vec3::sAxisX().Cross(radius);
    if (tangent.Length() < 0.25F) {
        return;
    }
    const JPH::Vec3 north = (-tangent).Normalized();
    const float drive = static_cast<float>(commands.move_input_x) * north.GetX() +
                        static_cast<float>(commands.move_input_z) * north.GetZ();
    if (std::abs(drive) < static_cast<float>(kClimbInputDeadzone)) {
        return;
    }
    const JPH::Vec3 direction = drive > 0.0F ? north : -north;
    const JPH::Vec3 velocity = bodies.GetPointVelocity(traversal_body_, grip);
    float scale = 1.0F;
    if (velocity.Length() > kPitmanCoastSpeed && direction.Dot(velocity) < 0.0F) {
        scale = kPitmanPumpOpposed;
    }
    const JPH::Vec3 force = direction * (kPitmanPumpForce * std::abs(drive) * scale);
    // The pull is on the bar. The grip constraint is what hauls the body
    // along with it, and that reaction is already in the constraint.
    // A second, opposite push on the body cancels the pull.
    bodies.AddForce(traversal_body_, force, grip);
}

void Simulation::PhysicsWorld::load_hold(JPH::BodyInterface &bodies) noexcept {
    if ((traversal_state_ != TraversalState::Hanging &&
         traversal_state_ != TraversalState::Climbing) ||
        traversal_body_.IsInvalid() ||
        bodies.GetMotionType(traversal_body_) != JPH::EMotionType::Dynamic) {
        return;
    }
    const JPH::RVec3 at = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    bodies.AddForce(traversal_body_, physics_system_.GetGravity() * kPlayerMassKg, at);
}

JPH::Vec3 Simulation::PhysicsWorld::traversal_right() const noexcept {
    return JPH::Vec3(-traversal_normal_.GetZ(), 0.0F, traversal_normal_.GetX());
}

Grip Simulation::PhysicsWorld::find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                             const JPH::Vec3 facing) const noexcept {
    Grip best;
    if (facing.IsNearZero()) {
        return best;
    }
    const JPH::Quat rotation =
        JPH::Quat::sRotation(JPH::Vec3::sAxisY(), std::atan2(facing.GetX(), facing.GetZ()));
    JPH::CollideShapeSettings settings;
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
    physics_system_.GetNarrowPhaseQuery().CollideShape(
        grip_region_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sRotationTranslation(rotation, aim), settings, aim, hits, {}, {}, filter);
    float best_distance = std::numeric_limits<float>::max();
    for (const JPH::CollideShapeResult &hit : hits.mHits) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(),
                                     hit.mBodyID2);
        if (!lock.Succeeded()) {
            continue;
        }
        const JPH::Body &body = lock.GetBody();
        if (body.IsSensor() ||
            (body.IsDynamic() &&
             body.GetMotionProperties()->GetInverseMass() * kGripMinBodyMassKg > 1.0F)) {
            continue;
        }
        LeafBox box;
        if (!leaf_box(body, hit.mSubShapeID2, box)) {
            continue;
        }
        std::uint32_t long_axis = 0;
        if (!box_is_hold(box, long_axis)) {
            continue;
        }
        const JPH::Vec3 along = box.axes[long_axis];
        const float half_length = box.half[long_axis];
        const float t =
            std::clamp(JPH::Vec3(aim - box.centre).Dot(along), -half_length, half_length);
        const JPH::RVec3 point = box.centre + along * t;
        // In front of the body: not beside it, not behind it.
        if (JPH::Vec3(point - centre).Dot(facing) < 0.5F * kPlayerRadius) {
            continue;
        }
        const float distance = JPH::Vec3(point - aim).Length();
        if (distance < best_distance) {
            best_distance = distance;
            best.valid = true;
            best.body = hit.mBodyID2;
            best.entity_id = body.GetUserData();
            best.point = point;
        }
    }
    return best;
}

JPH::RVec3 Simulation::PhysicsWorld::hand_aim(const JPH::RVec3 centre, const float height,
                                  const float across) const noexcept {
    return centre + JPH::Vec3(0.0F, height, 0.0F) + traversal_normal_ * kClimbHandReach +
           traversal_right() * across;
}

JPH::RVec3 Simulation::PhysicsWorld::hand_point(const JPH::BodyInterface &bodies,
                                    const std::uint32_t hand) const noexcept {
    return from_support_local(bodies, hands_[hand].body, hands_[hand].local);
}

void Simulation::PhysicsWorld::set_hand(const JPH::BodyInterface &bodies, const std::uint32_t hand,
              const Grip &grip) noexcept {
    hands_[hand].body = grip.body;
    hands_[hand].local = to_support_local(bodies, grip.body, grip.point);
    hands_[hand].valid = true;
}

void Simulation::PhysicsWorld::take_hand_holds(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                     const Grip &fallback) noexcept {
    for (std::uint32_t hand = 0; hand < 2; ++hand) {
        const float side = hand == 0 ? -1.0F : 1.0F;
        const float height = hand == 0 ? kClimbHandMid : kClimbHandMid + 0.2F;
        const Grip grip =
            find_grip(centre, hand_aim(centre, height, kClimbHandSpan * side), traversal_normal_);
        set_hand(bodies, hand, grip.valid ? grip : fallback);
    }
    regrip_cooldown_ticks_ = 0;
}

bool Simulation::PhysicsWorld::climb_move_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept {
    const JPH::Vec3 displacement(to - from);
    if (displacement.IsNearZero(1.0e-10F)) {
        return true;
    }
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const JPH::RShapeCast sweep(climb_sweep_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
                                JPH::RMat44::sTranslation(from), displacement);
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
    physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), from,
                                                    collector, {}, {}, filter);
    return !collector.HadHit();
}

void Simulation::PhysicsWorld::begin_climb(JPH::BodyInterface &bodies, const Grip &grip) noexcept {
    if (grip.entity_id == scraperx::sim::Simulation::kPitmanEntityId) {
        begin_pitman_hang(bodies, grip);
        return;
    }
    const JPH::RVec3 hold = bodies.GetPosition(player_id_);
    traversal_state_ = TraversalState::Climbing;
    traversal_body_ = grip.body;
    traversal_entity_id_ = grip.entity_id;
    traversal_target_body_ = {};
    traversal_normal_ = facing_;
    traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, grip.point);
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = hold;
    bodies.SetGravityFactor(player_id_, 0.0F);
    take_hand_holds(bodies, hold, grip);
    ++climb_count_;
}

Grip Simulation::PhysicsWorld::grip_in_front(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const JPH::RVec3 aim = centre + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach;
    return find_grip(centre, aim, facing_);
}

void Simulation::PhysicsWorld::update_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                  const float delta_seconds) noexcept {
    const JPH::Vec3 normal = traversal_normal_;
    const JPH::Vec3 right = traversal_right();
    const double up_input =
        commands.move_input_x * normal.GetX() + commands.move_input_z * normal.GetZ();
    const double side_input =
        commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    JPH::RVec3 next = hold;
    Grip carries;
    if (up_input > kClimbInputDeadzone) {
        // A ledge in reach is climbed over before any hold above it: grab
        // rails that run on past a deck's edge must not carry the body
        // past the height it can mantle from.
        if (up_input > 0.5 && try_top_out(bodies)) {
            return;
        }
        const JPH::RVec3 moved =
            hold + JPH::Vec3(0.0F, static_cast<float>(up_input) * kClimbUpSpeed * delta_seconds, 0.0F);
        const Grip above = find_grip(moved, hand_aim(moved, kClimbMoveAim, 0.0F), normal);
        if (above.valid) {
            next = moved;
            carries = above;
        }
    } else if (up_input < -kClimbInputDeadzone) {
        const JPH::RVec3 moved =
            hold + JPH::Vec3(0.0F, static_cast<float>(up_input) * kClimbDownSpeed * delta_seconds, 0.0F);
        if (feet_on_ground(moved)) {
            step_off_climb(bodies);
            return;
        }
        const Grip below = find_grip(moved, hand_aim(moved, kClimbHandLow, 0.0F), normal);
        if (below.valid) {
            next = moved;
            carries = below;
        }
    }
    if (std::abs(side_input) > kClimbInputDeadzone) {
        const float lead = side_input > 0.0 ? kClimbSideLead : -kClimbSideLead;
        const JPH::RVec3 moved =
            next + right * (static_cast<float>(side_input) * kClimbSideSpeed * delta_seconds);
        const Grip beside = find_grip(moved, hand_aim(moved, kClimbHandMid, lead), normal);
        if (beside.valid) {
            next = moved;
            carries = beside;
        }
    }
    if (carries.valid && climb_move_clear(hold, next)) {
        traversal_body_ = carries.body;
        traversal_entity_id_ = carries.entity_id;
        traversal_local_hold_ = to_support_local(bodies, traversal_body_, next);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, carries.point);
    }
    move_hands(bodies, from_support_local(bodies, traversal_body_, traversal_local_hold_),
               up_input, side_input);
}

void Simulation::PhysicsWorld::move_hands(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                const double up_input, const double side_input) noexcept {
    if (regrip_cooldown_ticks_ > 0) {
        --regrip_cooldown_ticks_;
        return;
    }
    const JPH::Vec3 right = traversal_right();
    std::int32_t mover = -1;
    float height = kClimbHandMid;
    float across = 0.0F;
    const auto rise = [&](const std::uint32_t hand) {
        return static_cast<float>(hand_point(bodies, hand).GetY() - centre.GetY());
    };
    const auto offset = [&](const std::uint32_t hand) {
        return JPH::Vec3(hand_point(bodies, hand) - centre).Dot(right);
    };
    if (up_input > kClimbInputDeadzone) {
        const std::uint32_t low = rise(0) <= rise(1) ? 0U : 1U;
        if (rise(low) < kClimbHandMid - 0.15F) {
            mover = static_cast<std::int32_t>(low);
            height = kClimbHandHigh;
        }
    } else if (up_input < -kClimbInputDeadzone) {
        const std::uint32_t high = rise(0) >= rise(1) ? 0U : 1U;
        if (rise(high) > kClimbHandHigh + 0.10F) {
            mover = static_cast<std::int32_t>(high);
            height = kClimbHandLow;
        }
    } else if (std::abs(side_input) > kClimbInputDeadzone) {
        const float sign = side_input > 0.0 ? 1.0F : -1.0F;
        const std::uint32_t behind = offset(0) * sign <= offset(1) * sign ? 0U : 1U;
        if (offset(behind) * sign < -kClimbHandSpan) {
            mover = static_cast<std::int32_t>(behind);
            across = sign * kClimbSideLead;
        }
    }
    // A hand the body has left out of reach takes the nearest hold again.
    for (std::uint32_t hand = 0; hand < 2 && mover < 0; ++hand) {
        const JPH::RVec3 shoulder = centre + JPH::Vec3(0.0F, kClimbHandMid, 0.0F);
        if (!hands_[hand].valid ||
            JPH::Vec3(hand_point(bodies, hand) - shoulder).Length() > 0.95F) {
            mover = static_cast<std::int32_t>(hand);
            across = hand == 0 ? -kClimbHandSpan : kClimbHandSpan;
        }
    }
    if (mover < 0) {
        return;
    }
    const std::uint32_t hand = static_cast<std::uint32_t>(mover);
    if (across == 0.0F) {
        across = hand == 0 ? -kClimbHandSpan : kClimbHandSpan;
    }
    const Grip grip = find_grip(centre, hand_aim(centre, height, across), traversal_normal_);
    if (grip.valid) {
        set_hand(bodies, hand, grip);
        regrip_cooldown_ticks_ = kClimbRegripTicks;
    }
}

bool Simulation::PhysicsWorld::feet_on_ground(const JPH::RVec3 centre) const noexcept {
    JPH::RayCastResult hit;
    const JPH::Vec3 down(0.0F, -(kPlayerHalfHeight + kClimbFootReach), 0.0F);
    if (!cast_ray(centre, down, hit)) {
        return false;
    }
    const JPH::RVec3 point = JPH::RRayCast(centre, down).GetPointOnRay(hit.mFraction);
    return surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() >= kSupportNormalThreshold;
}

bool Simulation::PhysicsWorld::try_top_out(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    LedgeProbe probe;
    if (!facing_.IsNearZero()) {
        probe = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true, true);
    }
    if (!probe.valid) {
        probe = probe_ledge(origin, traversal_normal_, kMantleMinimumRise, kMantleMaximumRise,
                            true, true);
    }
    if (!probe.valid) {
        return false;
    }
    hands_[0].valid = false;
    hands_[1].valid = false;
    begin_mantle(bodies, probe, origin);
    return true;
}

void Simulation::PhysicsWorld::step_off_climb(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    bodies.SetLinearVelocity(player_id_, bodies.GetPointVelocity(traversal_body_, hold));
    bodies.SetGravityFactor(player_id_, 1.0F);
    ++accepted_traversal_count_;
    clear_traversal();
}

void Simulation::PhysicsWorld::let_go_climb(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    const JPH::Vec3 support_velocity = bodies.GetPointVelocity(traversal_body_, hold);
    release_with_speed(bodies, support_velocity);
}

void Simulation::PhysicsWorld::jump_off_climb(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    const JPH::Vec3 support_velocity = bodies.GetPointVelocity(traversal_body_, hold);
    bodies.SetLinearVelocity(player_id_, support_velocity -
                                             traversal_normal_ * kClimbJumpBackSpeed +
                                             JPH::Vec3(0.0F, kClimbJumpUpSpeed, 0.0F));
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
}

} // namespace scraperx::sim
