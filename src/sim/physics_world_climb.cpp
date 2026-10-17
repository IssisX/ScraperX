// Climbing (Step 2 movement): grips, swing, climb, hands, shimmy, lowering, beams.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim {

// A hang or climb on a dynamic body hangs the climber's weight on it at
// the hold: the structure feels who is on it. Static and kinematic
// bodies take no force.
void Simulation::PhysicsWorld::load_hold(JPH::BodyInterface &bodies) noexcept {
    // A hang's arms are a constraint: the hold takes its load through them.
    if (traversal_state_ != TraversalState::Climbing ||
        traversal_body_.IsInvalid() ||
        bodies.GetMotionType(traversal_body_) != JPH::EMotionType::Dynamic) {
        return;
    }
    const JPH::RVec3 at = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    bodies.AddForce(traversal_body_, physics_system_.GetGravity() * kPlayerMassKg, at);
}

// The hold nearest `aim` that a hand in front of `centre` can close
// round: a box whose two thinner dimensions are at most kGripMaxSection
// and whose length is at least kGripMinLength, on a static body or one of
// at least kGripMinBodyMassKg, searched in a kGripSearchHalf box round
// `aim` squared to `facing`. The point is on the member's axis.
Grip Simulation::PhysicsWorld::find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                                         const JPH::Vec3 facing,
                                         const float min_ahead) const noexcept {
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
        if (JPH::Vec3(point - centre).Dot(facing) < min_ahead) {
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

// Where a hand in front of `centre` aims: `height` over the centre,
// kClimbHandReach toward the structure, `across` to its right.
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

// Both hands onto the holds nearest their rest aims, left a little lower
// than right; a hand that finds none takes the hold the climb began on.
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

// The swept move of a climbing or hanging body, slimmed so the holds it
// already touches do not stop it.
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

// Take hold of `grip` where the body is, facing it.
// Hands closed on a bar overhead: hung from it by the arms, the body
// keeping the momentum it caught it with.
void Simulation::PhysicsWorld::begin_swing(JPH::BodyInterface &bodies, const Grip &grip) noexcept {
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    traversal_state_ = TraversalState::Swinging;
    traversal_entry_offset_ = JPH::Vec3::sZero();
    traversal_body_ = grip.body;
    traversal_entity_id_ = grip.entity_id;
    traversal_target_body_ = {};
    traversal_normal_ = facing_;
    traversal_local_hold_ = to_support_local(bodies, traversal_body_, origin);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, grip.point);
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = origin;
    attach_hang_arms(bodies);
    regrip_cooldown_ticks_ = kSwingRegripTicks;
    ++swing_catch_count_;
}

// Swinging: the kip pumps the swing; held forward, the free hand takes
// the next hold ahead in reach.
void Simulation::PhysicsWorld::update_swing(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (hang_arms_ == nullptr || facing_.IsNearZero()) {
        return;
    }
    const JPH::Vec3 input(static_cast<float>(commands.move_input_x), 0.0F,
                          static_cast<float>(commands.move_input_z));
    const JPH::Vec3 velocity =
        bodies.GetLinearVelocity(player_id_) -
        bodies.GetPointVelocity(traversal_body_,
                                from_support_local(bodies, traversal_body_, traversal_local_ledge_));
    const float along_velocity = velocity.Dot(input);
    if (input.LengthSq() < 1.0e-2F) {
        steady_hang(bodies);   // the stick let go: the body stills its swing
    } else if (along_velocity > -kSwingKipFromRestMps) {
        const JPH::Vec3 force = input * (std::min(1.0F, input.Length()) * kSwingKipForceN / input.Length());
        bodies.AddForce(player_id_, force);
        if (bodies.GetMotionType(traversal_body_) == JPH::EMotionType::Dynamic) {
            bodies.AddForce(traversal_body_, -force,
                            from_support_local(bodies, traversal_body_, traversal_local_ledge_));
        }
    }
    if (regrip_cooldown_ticks_ > 0) {
        --regrip_cooldown_ticks_;
        return;
    }
    if (input.Dot(facing_) < 0.5F) {
        return;
    }
    const JPH::RVec3 hands = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::RVec3 shoulders = bodies.GetPosition(player_id_) + hang_shoulder_offset();
    const Grip next = find_grip(
        shoulders,
        shoulders + facing_ * kSwingReachAimAhead + JPH::Vec3(0.0F, kSwingReachAimRise, 0.0F),
        facing_, 0.0F);
    if (!next.valid || JPH::Vec3(next.point - shoulders).Length() > kSwingReachM ||
        JPH::Vec3(next.point - hands).Dot(facing_) < kSwingMinAdvanceM ||
        next.point.GetY() < shoulders.GetY()) {
        // No bar ahead: a ledge's lip in the same reach is taken instead,
        // and the body hangs from it as from any ledge caught.
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        const LedgeProbe lip = probe_ledge(origin, facing_, kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                           kPlayerHalfHeight + kHangMaximumRiseAboveCentre, true);
        if (lip.valid && JPH::Vec3(lip.ledge_point - shoulders).Length() <= kSwingReachM &&
            JPH::Vec3(lip.ledge_point - hands).Dot(facing_) >= kSwingMinAdvanceM) {
            clear_traversal();
            begin_hang(bodies, lip, origin);
            ++swing_reach_count_;
        }
        return;
    }
    traversal_body_ = next.body;
    traversal_entity_id_ = next.entity_id;
    traversal_normal_ = facing_;
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, next.point);
    attach_hang_arms(bodies);
    regrip_cooldown_ticks_ = kSwingRegripTicks;
    ++swing_reach_count_;
}

void Simulation::PhysicsWorld::begin_climb(JPH::BodyInterface &bodies, const Grip &grip) noexcept {
    const JPH::RVec3 hold = bodies.GetPosition(player_id_);
    traversal_state_ = TraversalState::Climbing;
    traversal_entry_offset_ = JPH::Vec3::sZero();
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

// From the ground, a hold faced at hand height.
Grip Simulation::PhysicsWorld::grip_in_front(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const JPH::RVec3 aim = centre + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach;
    return find_grip(centre, aim, facing_);
}

// Climbing, the stick toward the structure climbs, away climbs down,
// sideways moves across -- each only onto holds the hands can reach.
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

// Hand over hand: the hand furthest behind the way the body moves
// reaches ahead to the next hold, one hand every kClimbRegripTicks.
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

// Soles within kClimbFootReach of walkable ground under a body at `centre`.
bool Simulation::PhysicsWorld::feet_on_ground(const JPH::RVec3 centre) const noexcept {
    JPH::RayCastResult hit;
    const JPH::Vec3 down(0.0F, -(kPlayerHalfHeight + kClimbFootReach), 0.0F);
    if (!cast_ray(centre, down, hit)) {
        return false;
    }
    const JPH::RVec3 point = JPH::RRayCast(centre, down).GetPointOnRay(hit.mFraction);
    return surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() >= kSupportNormalThreshold;
}

// Climbing over a ledge in reach: the mantle takes over from where the
// body is. The ledge is looked for where the player looks, then along
// the structure's normal: a climber on a pipe faced from an angle looks
// round at the deck it means to climb onto.
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
    bodies.SetLinearVelocity(player_id_, support_velocity);
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
}

// Springing back off the structure, away from it and up.
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

// The lip of a ledge near `near` (a point on or near its top edge),
// faced along `normal`: a ray across just under the top onto its face,
// then a ray down onto the top just inside the face.
Lip Simulation::PhysicsWorld::probe_lip(const JPH::RVec3 near, const JPH::Vec3 normal) const noexcept {
    Lip lip;
    const JPH::RVec3 outside =
        near - normal * 0.5F - JPH::Vec3(0.0F, kLipProbeDepth, 0.0F);
    const JPH::Vec3 across = normal * 0.9F;
    JPH::RayCastResult face_hit;
    if (!cast_ray(outside, across, face_hit)) {
        return lip;
    }
    const JPH::RVec3 face = JPH::RRayCast(outside, across).GetPointOnRay(face_hit.mFraction);
    const JPH::RVec3 above = face + normal * kTopProbeInset + JPH::Vec3(0.0F, 0.30F, 0.0F);
    const JPH::Vec3 down(0.0F, -0.45F, 0.0F);
    JPH::RayCastResult top_hit;
    if (!cast_ray(above, down, top_hit) || top_hit.mBodyID != face_hit.mBodyID) {
        return lip;
    }
    const JPH::RVec3 top = JPH::RRayCast(above, down).GetPointOnRay(top_hit.mFraction);
    if (surface_normal(top_hit.mBodyID, top_hit.mSubShapeID2, top).GetY() <
        kLedgeTopNormalThreshold) {
        return lip;
    }
    lip.valid = true;
    lip.body = face_hit.mBodyID;
    lip.entity_id = physics_system_.GetBodyInterfaceNoLock().GetUserData(face_hit.mBodyID);
    lip.ledge = top;
    const JPH::RVec3 off_face = face - normal * (kPlayerRadius + kHangWallGap);
    lip.hold = JPH::RVec3(off_face.GetX(), top.GetY() - kHangDropBelowLedge, off_face.GetZ());
    const JPH::RVec3 inside = face + normal * kLandingInset;
    lip.landing =
        JPH::RVec3(inside.GetX(), top.GetY() + kPlayerHalfHeight + kLandingSkin, inside.GetZ());
    return lip;
}

// Hanging, the stick sideways moves along the ledge while its lip goes
// on under the hands. Something standing on the ledge back from its lip
// does not stop a shimmy; only climbing up needs the landing clear.
void Simulation::PhysicsWorld::update_shimmy(JPH::BodyInterface &bodies, const StepCommands &commands,
                                             const float delta_seconds) noexcept {
    const JPH::Vec3 right = traversal_right();
    const double side_input =
        commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
    if (std::abs(side_input) <= kClimbInputDeadzone || traversal_normal_.IsNearZero()) {
        return;
    }
    const float lead = side_input > 0.0 ? 0.25F : -0.25F;
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::Vec3 step = right * (static_cast<float>(side_input) * kShimmySpeed * delta_seconds);
    const Lip at = probe_lip(ledge + step, traversal_normal_);
    const Lip ahead = probe_lip(ledge + step + right * lead, traversal_normal_);
    if (!at.valid || !ahead.valid || std::abs(at.ledge.GetY() - ledge.GetY()) > 0.15 ||
        !climb_move_clear(hold, at.hold)) {
        return;
    }
    traversal_body_ = at.body;
    traversal_entity_id_ = at.entity_id;
    traversal_local_hold_ = to_support_local(bodies, at.body, at.hold);
    traversal_local_ledge_ = to_support_local(bodies, at.body, at.ledge);
    traversal_target_body_ = at.body;
    traversal_local_target_ = to_support_local(bodies, at.body, at.landing);
    // The hands move along the lip: they close again where they now are,
    // the arms keeping the reach they had.
    const float reach = hang_arms_ != nullptr ? hang_arms_->GetMaxDistance() : kHangArmStop;
    attach_hang_arms(bodies);
    if (hang_arms_ != nullptr && hang_arms_->GetMaxDistance() > reach) {
        hang_arms_->SetDistance(0.0F, std::max(reach, kHangArmStop));
    }
}

// An edge behind a standing body with a drop beyond it: the floor ends
// within kEdgeSearchReach behind the body and nothing is under the space
// beyond for kEdgeDropMinimum, and a hanging body fits below its lip.
Lip Simulation::PhysicsWorld::probe_edge_drop(const JPH::BodyInterface &bodies) const noexcept {
    if (!grounded_ || crouched_ || carry_constraint_ != nullptr || facing_.IsNearZero()) {
        return {};
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const double feet_y = origin.GetY() - kPlayerHalfHeight;
    const JPH::Vec3 back = -facing_;
    for (float behind = kEdgeSearchStart; behind <= kEdgeSearchReach + 1.0e-4F;
         behind += kEdgeSearchStep) {
        const JPH::RVec3 at = origin + back * behind;
        const JPH::RVec3 from(at.GetX(), feet_y + 0.05, at.GetZ());
        JPH::RayCastResult floor;
        if (cast_ray(from, JPH::Vec3(0.0F, -(0.05F + kEdgeDropMinimum), 0.0F), floor)) {
            continue;
        }
        const JPH::RVec3 last = origin + back * (behind - kEdgeSearchStep);
        const Lip lip = probe_lip(JPH::RVec3(last.GetX(), feet_y, last.GetZ()), facing_);
        if (!lip.valid || std::abs(lip.ledge.GetY() - feet_y) > 0.10 ||
            !capsule_pose_is_clear(lip.hold)) {
            return {};
        }
        return lip;
    }
    return {};
}

// Drop, standing with an edge behind: lower over it into a hang.
bool Simulation::PhysicsWorld::try_begin_lowering(JPH::BodyInterface &bodies) noexcept {
    const Lip lip = probe_edge_drop(bodies);
    if (!lip.valid) {
        return false;
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    traversal_state_ = TraversalState::Lowering;
    traversal_body_ = lip.body;
    traversal_entity_id_ = lip.entity_id;
    traversal_target_body_ = lip.body;
    traversal_normal_ = facing_;
    traversal_local_start_ = to_support_local(bodies, lip.body, origin);
    traversal_local_hold_ = to_support_local(bodies, lip.body, lip.hold);
    traversal_local_ledge_ = to_support_local(bodies, lip.body, lip.ledge);
    traversal_local_target_ = to_support_local(bodies, lip.body, lip.landing);
    traversal_progress_ = 0.0;
    traversal_duration_ = kLoweringSeconds;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = origin;
    bodies.SetGravityFactor(player_id_, 0.0F);
    return true;
}

// Lowered: hanging from the lip, as if the hang had been caught there.
void Simulation::PhysicsWorld::finish_lowering() noexcept {
    traversal_state_ = TraversalState::Hanging;
    traversal_entry_offset_ = JPH::Vec3::sZero();
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    ++accepted_traversal_count_;
    JPH::BodyInterface &bodies = physics_system_.GetBodyInterfaceNoLock();
    bodies.SetLinearVelocity(player_id_, JPH::Vec3::sZero());
    attach_hang_arms(bodies);
}

Simulation::PhysicsWorld::Beam Simulation::PhysicsWorld::beam_underfoot(const JPH::BodyInterface &bodies) const noexcept {
    Beam beam;
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const float half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    JPH::RayCastResult hit;
    JPH::RVec3 probe = centre;
    float reach = half_height + kCheckpointFootingSlack;
    if (!cast_ray(probe, JPH::Vec3(0.0F, -reach, 0.0F), hit)) {
        // The centre already past the beam's side, the capsule's rim
        // still on it: the beam is where the contact is.
        if (!support_sample_.grounded) {
            return beam;
        }
        probe = JPH::RVec3(support_sample_.contact_point.x, centre.GetY(), support_sample_.contact_point.z);
        reach += kPlayerRadius;
        if (!cast_ray(probe, JPH::Vec3(0.0F, -reach, 0.0F), hit)) {
            return beam;
        }
    }
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), hit.mBodyID);
    if (!lock.Succeeded()) {
        return beam;
    }
    LeafBox box;
    if (!leaf_box(lock.GetBody(), hit.mSubShapeID2, box)) {
        return beam;
    }
    std::uint32_t up = 0;
    for (std::uint32_t axis = 1; axis < 3; ++axis) {
        if (std::abs(box.axes[axis].GetY()) > std::abs(box.axes[up].GetY())) {
            up = axis;
        }
    }
    if (std::abs(box.axes[up].GetY()) < 0.9F) {
        return beam;
    }
    const std::uint32_t a = (up + 1) % 3;
    const std::uint32_t b = (up + 2) % 3;
    const std::uint32_t long_axis = box.half[a] >= box.half[b] ? a : b;
    const std::uint32_t short_axis = long_axis == a ? b : a;
    if (2.0F * box.half[short_axis] > kBalanceMaxWidth ||
        2.0F * box.half[long_axis] < kBalanceMinLength) {
        return beam;
    }
    JPH::Vec3 along = box.axes[long_axis];
    along.SetY(0.0F);
    if (along.IsNearZero()) {
        return beam;
    }
    const JPH::Vec3 unit_along = along.Normalized();
    const JPH::Vec3 across(-unit_along.GetZ(), 0.0F, unit_along.GetX());
    const float offset = JPH::Vec3(centre - box.centre).Dot(across);
    // A beam is a beam over a fall. Ground within a step beside it (the
    // yard's kerb, a sill) is walked on and off like any floor: held on
    // its line, a body could not step down off a kerb.
    const JPH::RVec3 underfoot =
        centre + JPH::Vec3(0.0F, -reach * hit.mFraction, 0.0F) -
        across * offset;
    for (const float side : {-1.0F, 1.0F}) {
        const JPH::RVec3 beside = underfoot +
                                  across * (side * (box.half[short_axis] + kBalanceSideProbe)) +
                                  JPH::Vec3(0.0F, kBalanceSideProbeLift, 0.0F);
        JPH::RayCastResult below;
        const JPH::Vec3 down(0.0F, -(kStepMaximumHeight + kBalanceSideProbeLift), 0.0F);
        if (cast_ray(beside, down, below) &&
            surface_normal(below.mBodyID, below.mSubShapeID2, beside + down * below.mFraction)
                    .GetY() >= kSupportNormalThreshold) {
            return beam;
        }
    }
    beam.valid = true;
    beam.along = unit_along;
    beam.across = across;
    beam.offset = offset;
    beam.half_width = box.half[short_axis];
    return beam;
}

void Simulation::PhysicsWorld::clear_traversal() noexcept {
    drop_hang_arms();
    traversal_speed_vault_ = false;
    traversal_springboard_ = false;
    traversal_vault_descend_at_ = 0.5F;
    traversal_state_ = TraversalState::None;
    traversal_normal_ = JPH::Vec3::sZero();
    hands_[0].valid = false;
    hands_[1].valid = false;
    air_full_speed_ = kPlayerMaximumRelativeSpeed;
    traversal_body_ = {};
    traversal_target_body_ = {};
    traversal_entity_id_ = 0;
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    traversal_vault_horizontal_slope_ = 0.0F;
    traversal_vault_wall_fraction_ = 1.0F;
    traversal_vault_clear_progress_ = 0.0F;
}

} // namespace scraperx::sim
