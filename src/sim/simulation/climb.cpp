#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] bool Simulation::PhysicsWorld::probe_lip_pair(JPH::RVec3 ledge,JPH::Vec3 inward,JPH::BodyID owner,
                                  Lip (&lips)[2]) const noexcept {
    const JPH::Vec3 right(inward.GetZ(),0,-inward.GetX());
    // CHOSEN hand spacing adapts to actual available edge width. Both
    // grips must still be distinct real surfaces on the named body;
    // maximum reach is unchanged and no wider beam is fabricated.
    for(float span:{kHangHandSpan,0.14F,0.08F}) {
        bool valid=true;
        for(unsigned hand=0;hand<2;++hand) {
            lips[hand]=probe_lip(ledge+right*(hand==0?-span:span),inward);
            if(!lips[hand].valid || lips[hand].body!=owner) {valid=false;break;}
        }
        if(valid && JPH::Vec3(lips[0].ledge-lips[1].ledge).Length()>0.10F) return true;
    }
    return false;
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::hang_hold(const LedgeProbe &probe) noexcept {
    return JPH::RVec3(
        probe.wall_point.GetX() - probe.inward.GetX() * (kPlayerRadius + kHangWallGap),
        probe.ledge_point.GetY() - kHangDropBelowLedge,
        probe.wall_point.GetZ() - probe.inward.GetZ() * (kPlayerRadius + kHangWallGap));
}

[[nodiscard]] bool Simulation::PhysicsWorld::hang_ledge_is_reachable(const JPH::RVec3 origin,
                                           const JPH::Vec3 facing,
                                           const LedgeProbe &probe) const noexcept {
    if (!capsule_pose_is_clear(hang_hold(probe))) return false;
    Lip lips[2];
    if (!probe_lip_pair(probe.ledge_point, probe.inward, probe.ledge_body, lips)) return false;
    const JPH::Vec3 right(probe.inward.GetZ(), 0, -probe.inward.GetX());
    for (const Lip &lip : lips) {
        const JPH::Vec3 reach(lip.ledge - origin);
        const float forward = reach.Dot(facing);
        if (lip.sub_shape != probe.ledge_sub_shape ||
            reach.GetY() < kHangMinimumRiseAboveCentre ||
            reach.GetY() > kHangMaximumRiseAboveCentre ||
            forward <= 0.0F || forward > kTraversalReach + kPlayerRadius ||
            reach.Length() > 1.75F ||
            std::abs(lip.ledge.GetY() - probe.ledge_point.GetY()) > 0.15F) return false;
        // Unattached hands approach from their actual upright-body aim
        // height. Local lip probes alone cannot see an intervening wall.
        const float across = JPH::Vec3(lip.ledge - probe.ledge_point).Dot(right);
        const auto from = origin + right * across + JPH::Vec3(0, kClimbHandMid, 0);
        const JPH::Vec3 approach(lip.ledge - from);
        if (approach.IsNearZero()) return false;
        JPH::RayCastResult first;
        // Extend only the query past the surface so an endpoint contact
        // is reported; physical grip/reach stays at the actual lip.
        if (!cast_ray(from, approach + approach.Normalized() * kLandingSkin, first) ||
            first.mFraction <= 0.0F || first.mBodyID != lip.body ||
            first.mSubShapeID2 != lip.sub_shape) return false;
    }
    return true;
}

[[nodiscard]] LedgeProbe Simulation::PhysicsWorld::probe_hang_ledge(const JPH::RVec3 origin,
                                          const JPH::Vec3 facing) const noexcept {
    if (facing.IsNearZero()) return {};
    const JPH::Vec3 forward = facing.Normalized();
    constexpr float kQueryHalfDepth = 0.01F;
    constexpr float kForwardReach = kTraversalReach + kPlayerRadius;
    const float half_band = 0.5F * (kHangMaximumRiseAboveCentre - kHangMinimumRiseAboveCentre);
    const JPH::BoxShape band(JPH::Vec3(kQueryHalfDepth, half_band, kQueryHalfDepth), 0.0F);
    const auto start = origin + forward * kQueryHalfDepth +
        JPH::Vec3(0, kHangMinimumRiseAboveCentre + half_band, 0);
    const auto rotation = JPH::Quat::sRotation(
        JPH::Vec3::sAxisY(), std::atan2(forward.GetX(), forward.GetZ()));
    // Include the box's finite depth in both ends of the reach bound.
    const JPH::RShapeCast sweep(&band, JPH::Vec3::sReplicate(1.0F),
        JPH::RMat44::sRotationTranslation(rotation, start),
        forward * (kForwardReach - 2.0F * kQueryHalfDepth));
    JPH::AllHitCollisionCollector<JPH::CastShapeCollector> seeds;
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CastShape(
        sweep, JPH::ShapeCastSettings(), start, seeds, {}, {}, player_filter);
    seeds.Sort();
    std::uint32_t checked = 0;
    for (const JPH::ShapeCastResult &seed : seeds.mHits) {
        // Initial overlaps/back faces never prove an exposed reachable
        // side. Rejected seeds also cannot hide nearer ray obstructions.
        if (seed.mFraction <= 0.0F || seed.mIsBackFaceHit ||
            is_hold(seed.mBodyID2, seed.mSubShapeID2)) continue;
        {
            const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(),
                                         seed.mBodyID2);
            if (!lock.Succeeded() || !lock.GetBody().IsRigidBody() ||
                lock.GetBody().IsSensor()) continue;
        }
        const auto contact = start + JPH::RVec3(seed.mContactPointOn2);
        const float projection = JPH::Vec3(contact - origin).Dot(forward);
        const float height = contact.GetY() - origin.GetY();
        if (projection <= 0.0F || projection > kForwardReach ||
            height < kHangMinimumRiseAboveCentre || height > kHangMaximumRiseAboveCentre) continue;
        // Bound expensive validation work even in a dense compound scene.
        if (++checked > 16) break;
        // A contact exactly on an edge has an ambiguous target normal.
        // Tiny local refinements prove the actual side, within the band;
        // discovery itself remains a continuous sweep, not a height grid.
        for (const float offset : {0.0F, -kQueryHalfDepth, kQueryHalfDepth}) {
            const float wall_height = height + offset;
            if (wall_height < kHangMinimumRiseAboveCentre ||
                wall_height > kHangMaximumRiseAboveCentre) continue;
            JPH::RayCastResult wall_hit;
            JPH::RVec3 wall_point;
            if (!cast_wall(origin + JPH::Vec3(0, wall_height, 0), forward * kForwardReach,
                           false, wall_hit, wall_point) ||
                wall_hit.mFraction <= 0.0F || wall_hit.mBodyID != seed.mBodyID2 ||
                wall_hit.mSubShapeID2 != seed.mSubShapeID2 ||
                std::abs(surface_normal(wall_hit.mBodyID, wall_hit.mSubShapeID2,
                                        wall_point).GetY()) >= kSupportNormalThreshold) continue;
            const auto probe = probe_ledge_from_wall(origin, forward,
                kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                // A real handhold need not have a standing receiver behind
                // it. Top-out validates that separate opportunity later.
                false, false, wall_hit, wall_point, true);
            if (probe.valid && hang_ledge_is_reachable(origin, forward, probe)) return probe;
        }
    }
    return {};
}

[[nodiscard]] bool Simulation::PhysicsWorld::acquire_hang_hands(JPH::BodyInterface &bodies) noexcept {
    hanging_posture_offset_m_ = 0.0F;
    const auto lip = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    Lip lips[2];
    if(!probe_lip_pair(lip,traversal_normal_,traversal_body_,lips)) return false;
    for (unsigned hand = 0; hand < 2; ++hand) {
        const auto &actual=lips[hand];
        if(std::abs(actual.ledge.GetY()-lip.GetY())>0.15 ||
           !physical_hands_->attach(hand,traversal_body_,actual.ledge)) {
            physical_hands_->clear();
            hands_[0].valid=hands_[1].valid=false;
            return false;
        }
        ++hands_[hand].generation;
        hands_[hand].body = traversal_body_;
        hands_[hand].local = to_support_local(bodies, traversal_body_, actual.ledge);
        hands_[hand].valid = true;
    }
    regrip_cooldown_ticks_ = 0;
    bodies.SetGravityFactor(player_id_, 1.0F);
    return true;
}

void Simulation::PhysicsWorld::try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (grounded_ || crouched_ || carry_constraint_ != nullptr) {
        return;
    }
    // A finite walking step may briefly leave its tread. Keep that rising
    // intent until real footing; Action can still deliberately take a handhold,
    // and a descending failed step can still catch to recover.
    if (physical_step_pending_ && !commands.traversal_requested &&
        bodies.GetLinearVelocity(player_id_).GetY() - airborne_inherited_velocity_.GetY() > 0)
        return;
    if (facing_.IsNearZero()) {
        return;
    }
    const double intent = commands.move_input_x * static_cast<double>(facing_.GetX()) +
                          commands.move_input_z * static_cast<double>(facing_.GetZ());
    if (intent < kHangIntentDotThreshold) {
        return;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const Grip reached = find_grip(origin,
        origin + JPH::Vec3(0,kClimbHandMid,0) + facing_*kClimbHandReach, facing_,
        regrab_lockout_ticks_>0 ? last_released_grip_ : JPH::BodyID());
    const auto locked = [&](JPH::BodyID body) {
        return regrab_lockout_ticks_ > 0 && body == last_released_grip_;
    };
    // A reachable real bar can be caught during a rising jump. Existing
    // finite hands absorb the catch; acquiring it never resets velocity.
    if (reached.valid && !locked(reached.body) && (is_parkour_swing_entity(reached.entity_id) ||
        bodies.GetLinearVelocity(player_id_).GetY() > kHangMaximumClimbSpeed)) {
        begin_climb(bodies, reached);
        return;
    }
    // Reach, clearance and finite hands determine rising ledge acquisition;
    // acquiring a ledge never resets velocity.
    const LedgeProbe probe = probe_hang_ledge(origin, facing_);
    if (!probe.valid) {
        // No ledge: a hold reached for in the air is caught and climbed.
        const Grip grip = find_grip(
            origin, origin + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach,
            facing_, regrab_lockout_ticks_>0 ? last_released_grip_ : JPH::BodyID());
        if (grip.valid && !locked(grip.body)) {
            begin_climb(bodies, grip);
        }
        return;
    }

    if (locked(probe.ledge_body)) return;
    const JPH::RVec3 hold = hang_hold(probe);

    traversal_state_ = TraversalState::Hanging;
    traversal_body_ = probe.ledge_body;
    traversal_entity_id_ = probe.ledge_entity_id;
    traversal_target_body_ = probe.landing_body;
    traversal_normal_ = probe.inward;
    traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
    traversal_local_target_ =
        to_support_local(bodies, traversal_target_body_, probe.landing_centre);
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = hold;
    if (!acquire_hang_hands(bodies)) {
        clear_traversal();
        ++rejected_traversal_count_;
    }
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::traversal_right() const noexcept {
    return JPH::Vec3(-traversal_normal_.GetZ(), 0.0F, traversal_normal_.GetX());
}

[[nodiscard]] bool Simulation::PhysicsWorld::grip_approach_visible(const JPH::RVec3 centre,
                                         const JPH::RVec3 aim,
                                         const JPH::Vec3 facing,
                                         const Grip &grip) const noexcept {
    if (!grip.valid || facing.IsNearZero()) return false;
    // Grip visibility ignores only the player and non-solid sensors.
    // Soft bodies and carried objects remain physical occluders.
    class SolidApproachFilter final : public JPH::IgnoreSingleBodyFilter {
    public:
        explicit SolidApproachFilter(const JPH::BodyID player)
            : JPH::IgnoreSingleBodyFilter(player) {}
        bool ShouldCollideLocked(const JPH::Body &body) const override {
            return !body.IsSensor();
        }
    };
    const SolidApproachFilter approach_filter(player_id_);
    const JPH::Quat rotation =
        JPH::Quat::sRotation(JPH::Vec3::sAxisY(), std::atan2(facing.GetX(), facing.GetZ()));
    // Keep the requested hand height/lateral offset, but anchor it to
    // actual COM. A planned regrip centre cannot move the ray past a wall.
    const JPH::Vec3 aim_offset = rotation.Conjugated() * JPH::Vec3(aim - centre);
    const JPH::RVec3 approach_from =
        physics_system_.GetBodyInterface().GetCenterOfMassPosition(player_id_) +
        rotation * JPH::Vec3(aim_offset.GetX(), aim_offset.GetY(), 0.0F);
    const JPH::Vec3 approach(grip.point - approach_from);
    const float approach_length = approach.Length();
    if (!std::isfinite(approach_length) || approach_length <= 0.0F) return false;
    JPH::RayCastResult first;
    // Extend only the query so a surface endpoint is reported. The
    // chosen grip and existing reach/search bounds stay unchanged.
    const JPH::RRayCast ray(approach_from,
        approach + approach * (kLandingSkin / approach_length));
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, first, {}, {}, approach_filter) ||
        !(first.mFraction > 0.0F) || first.mBodyID != grip.body) return false;
    if (cargo_net_ && grip.body == cargo_net_->body()) {
        // Material segments have averaged grip points, not rigid leaves.
        // Conservatively require the actual mesh hit within the existing skin.
        return JPH::Vec3(ray.GetPointOnRay(first.mFraction) - grip.point).Length() <= kLandingSkin;
    }
    return first.mSubShapeID2 == grip.sub_shape;
}

[[nodiscard]] Grip Simulation::PhysicsWorld::find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                             const JPH::Vec3 facing, JPH::BodyID excluded) const noexcept {
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
    Grip material_candidate;
    if (cargo_net_ && cargo_net_->body()!=excluded &&
        cargo_net_->grip(centre, aim, facing, material_candidate.point)) {
        material_candidate.valid = true;
        material_candidate.body = cargo_net_->body();
        material_candidate.entity_id = CargoNet::kEntity;
        if (grip_approach_visible(centre, aim, facing, material_candidate)) {
            best = material_candidate;
            best_distance = JPH::Vec3(best.point - aim).Length();
        }
    }
    for (const JPH::CollideShapeResult &hit : hits.mHits) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(),
                                     hit.mBodyID2);
        if (!lock.Succeeded()) {
            continue;
        }
        const JPH::Body &body = lock.GetBody();
        if (body.GetID()==excluded) continue;
        const bool owned_plank = deformable_plank_ &&
            deformable_plank_->logical_member_for_body(body.GetID()) == kDeformablePlankMemberEntity &&
            deformable_plank_->fragment_supported(body.GetID());
        if (!body.IsRigidBody() || body.IsSensor()) {
            continue;
        }
        LeafBox box;
        if (!leaf_box(body, hit.mSubShapeID2, box)) {
            continue;
        }
        std::uint32_t long_axis = 0;
        if (!owned_plank && !box_is_hold(box, long_axis)) {
            continue;
        }
        // Only the actual mounted timber cells get an edge grip. Other
        // boxes retain the section/length requirements. The point is on
        // the transformed top edge, not the cell's centre.
        if (owned_plank && (std::abs(box.half[0] - .1F) > .001F ||
            std::abs(box.half[1] - .019F) > .001F ||
            std::abs(box.half[2] - .095F) > .001F)) continue;
        const JPH::Vec3 along = box.axes[long_axis];
        const float half_length = box.half[long_axis];
        const float t =
            std::clamp(JPH::Vec3(aim - box.centre).Dot(along), -half_length, half_length);
        JPH::RVec3 point = box.centre + along * t;
        if (owned_plank) {
            const float side = JPH::Vec3(centre - box.centre).Dot(box.axes[2]) >= 0 ? 1.F : -1.F;
            point += box.axes[1] * box.half[1] + box.axes[2] * (side * box.half[2]);
            // CollideShape found the box; the chosen edge must also be
            // inside the same oriented hand search, without extra reach.
            const auto search = rotation.Conjugated() * JPH::Vec3(point - aim);
            if (std::abs(search.GetX()) > kGripSearchHalf.GetX() ||
                std::abs(search.GetY()) > kGripSearchHalf.GetY() ||
                std::abs(search.GetZ()) > kGripSearchHalf.GetZ()) continue;
        }
        // In front of the body: not beside it, not behind it.
        if (JPH::Vec3(point - centre).Dot(facing) < 0.5F * kPlayerRadius) {
            continue;
        }
        Grip candidate;
        candidate.valid = true;
        candidate.body = hit.mBodyID2;
        candidate.sub_shape = hit.mSubShapeID2;
        candidate.entity_id = body.GetUserData();
        candidate.point = point;
        if (!grip_approach_visible(centre, aim, facing, candidate)) continue;
        const float distance = JPH::Vec3(point - aim).Length();
        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
        }
    }
    return best;
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::hand_aim(const JPH::RVec3 centre, const float height,
                                  const float across) const noexcept {
    return centre + JPH::Vec3(0.0F, height, 0.0F) + traversal_normal_ * kClimbHandReach +
           traversal_right() * across;
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::hand_point(const JPH::BodyInterface &bodies,
                                    const std::uint32_t hand) const noexcept {
    return from_support_local(bodies, hands_[hand].body, hands_[hand].local);
}

[[nodiscard]] bool Simulation::PhysicsWorld::grip_matches_climb_backend(const Grip &grip) const noexcept {
    const bool net_grip = cargo_net_ && grip.body == cargo_net_->body();
    const bool net_climb = cargo_net_ && traversal_body_ == cargo_net_->body();
    return net_grip == net_climb;
}

void Simulation::PhysicsWorld::set_hand(const JPH::BodyInterface &bodies, const std::uint32_t hand,
              const Grip &grip) noexcept {
    // The principal hold selects the backend before either hand attaches.
    // A soft/rigid transfer needs an explicit reciprocal material adapter.
    if (!grip_matches_climb_backend(grip)) return;
    const auto local = to_support_local(bodies, grip.body, grip.point);
    const bool changed = !hands_[hand].valid || hands_[hand].body != grip.body ||
                         hands_[hand].local != local;
    if (changed && !(physical_hands_->attached(hand) &&
        (hanging_posture_offset_m_ > 0.0F ||
         (cargo_net_ && grip.body == cargo_net_->body()) || is_facade_route_entity(grip.entity_id) || is_parkour_route_entity(grip.entity_id) ||
         (deformable_plank_ && deformable_plank_->logical_member_for_body(grip.body) == kDeformablePlankMemberEntity))
        ? physical_hands_->regrip(hand, grip.body, grip.point)
        : physical_hands_->attach(hand, grip.body, grip.point))) {
        return;
    }
    if (changed)
        ++hands_[hand].generation;
    hands_[hand].body = grip.body;
    hands_[hand].local = local;
    hands_[hand].valid = true;
}

void Simulation::PhysicsWorld::take_hand_holds(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                     const Grip &fallback) noexcept {
    for (std::uint32_t hand = 0; hand < 2; ++hand) {
        const float side = hand == 0 ? -1.0F : 1.0F;
        const float height = hand == 0 ? kClimbHandMid : kClimbHandMid + 0.2F;
        const JPH::RVec3 aim = hand_aim(centre, height, kClimbHandSpan * side);
        const Grip grip = find_grip(centre, aim, traversal_normal_);
        if (grip.valid && grip_matches_climb_backend(grip)) {
            set_hand(bodies, hand, grip);
        } else if (fallback.valid && grip_matches_climb_backend(fallback) &&
                   grip_approach_visible(centre, aim, traversal_normal_, fallback)) {
            set_hand(bodies, hand, fallback);
        }
    }
    regrip_cooldown_ticks_ = 0;
}

[[nodiscard]] bool Simulation::PhysicsWorld::climb_move_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept {
    const JPH::Vec3 displacement(to - from);
    if (displacement.IsNearZero(1.0e-10F)) {
        return true;
    }
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const JPH::RShapeCast sweep(climb_sweep_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
                                JPH::RMat44::sTranslation(from), displacement);
    const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
    const JPH::IgnoreSingleBodyFilterChained carried_filter(carried_id_, player_filter);
    if (cargo_net_ && traversal_body_ == cargo_net_->body()) {
        // The held mesh bends around the climber; its own cloth contacts
        // must not veto the hand-over-hand capsule sweep. Jolt still
        // resolves the actual player/cloth contact during every step.
        const JPH::IgnoreSingleBodyFilterChained held_net_filter(cargo_net_->body(), carried_filter);
        physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), from,
                                                        collector, {}, {}, held_net_filter);
    } else {
        physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), from,
                                                        collector, {}, {}, carried_filter);
    }
    return !collector.HadHit();
}

void Simulation::PhysicsWorld::begin_climb(JPH::BodyInterface &bodies, const Grip &grip,
                 const bool allow_single_hand) noexcept {
    hanging_posture_offset_m_ = 0.0F;
    JPH::RVec3 hold = bodies.GetPosition(player_id_);
    if (bodies.GetMotionType(grip.body) == JPH::EMotionType::Dynamic &&
        !(cargo_net_ && grip.body == cargo_net_->body())) {
        // A close airborne catch must not ask the climb servo to keep the
        // capsule intersecting its held member: that contact pins a free
        // structure. Resolve the native stance target outward, within the
        // existing hand-search reach. Jolt still resolves every contact;
        // no pose write or held-body collision exclusion is introduced.
        const float clearance = kPlayerRadius + 0.5F * kGripMaxSection + kHangWallGap;
        const float distance = JPH::Vec3(grip.point - hold).Dot(facing_);
        if (distance < clearance) hold -= facing_ * (clearance - distance);
    }
    traversal_state_ = TraversalState::Climbing;
    traversal_body_ = grip.body;
    traversal_entity_id_ = grip.entity_id;
    traversal_target_body_ = {};
    traversal_normal_ = facing_;
    traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
    if (cargo_net_ && traversal_body_ == cargo_net_->body()) {
        // Keep the capsule in front of the loaded mesh and receiver lip,
        // within the existing hand reach, even after walking into the rope.
        const float depth = traversal_local_hold_.GetZ();
        const float side = std::abs(depth) > 0.001F ? std::copysign(1.0F, depth)
                                                   : (facing_.GetZ() < 0.0F ? 1.0F : -1.0F);
        // Native capsule radius0.35m plus0.20m clearance, within the
        // 0.40m hand reach/search shell even as different rows deflect.
        // The finite hands approach this stance; the capsule is not moved.
        traversal_local_hold_.SetZ(side * 0.55F);
    }
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, grip.point);
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = hold;
    bodies.SetGravityFactor(player_id_, 1.0F);
    take_hand_holds(bodies, hold, grip);
    // Automatic directional reach requires both anchors for climbing.
    // Only deliberate Action may acquire a partial passive radial catch.
    if (!physical_hands_->compatible_hands()) {
        if (allow_single_hand && physical_hands_->radial_compatible_hands() && !grounded_ &&
            actual_hands_in_reach(bodies)) {
            const auto relative=JPH::Vec3(physical_hands_->radial_grip_centroid()-
                bodies.GetCenterOfMassPosition(player_id_));
            if (relative.GetY()>.35F && physical_hands_->set_radial_profile(true)) {
                const unsigned hand=physical_hands_->attached(0) ? 0 : 1;
                // The offered principal cue can differ from the real
                // independently selected grip. Ownership follows that grip.
                traversal_body_=hands_[hand].body;
                traversal_entity_id_=bodies.GetUserData(traversal_body_);
                traversal_local_ledge_=hands_[hand].local;
                traversal_local_hold_=to_support_local(bodies,traversal_body_,bodies.GetPosition(player_id_));
                ++climb_count_;
                return;
            }
        }
        clear_traversal();
        ++rejected_traversal_count_;
        return;
    }
    if (is_parkour_swing_entity(grip.entity_id)) {
        physical_hands_->set_swing_profile();
        const auto centre_grip = .5F*(hand_point(bodies,0)+hand_point(bodies,1));
        swing_rest_offset_ = JPH::Vec3(bodies.GetPosition(player_id_)-centre_grip);
        swing_rest_offset_.SetY(-.9F);
    }
    ++climb_count_;
}

[[nodiscard]] Grip Simulation::PhysicsWorld::grip_in_front(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const JPH::RVec3 aim = centre + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach;
    return find_grip(centre, aim, facing_,
        !grounded_ && regrab_lockout_ticks_ > 0 ? last_released_grip_ : JPH::BodyID());
}

[[nodiscard]] bool Simulation::PhysicsWorld::radial_ownership_eligible() const noexcept {
    return !grounded_ && (traversal_state_ == TraversalState::Climbing ||
        traversal_state_ == TraversalState::Hanging) && !cargo_transfer_active_ &&
        cargo_push_seconds_ < 0.0F && !(slingshot_ && slingshot_->controls_player()) &&
        physical_hands_ && physical_hands_->radial_compatible_hands();
}

[[nodiscard]] bool Simulation::PhysicsWorld::actual_hands_in_reach(const JPH::BodyInterface &bodies) const noexcept {
    if (!physical_hands_ || !physical_hands_->radial_compatible_hands()) return false;
    const auto centre=bodies.GetCenterOfMassPosition(player_id_);
    for (unsigned hand=0;hand<2;++hand) if (physical_hands_->attached(hand)) {
        if (!hands_[hand].valid) return false;
        {
            const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),hands_[hand].body);
            if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
                (!(cargo_net_ && hands_[hand].body==cargo_net_->body()) && !lock.GetBody().IsRigidBody())) return false;
        }
        const float distance=JPH::Vec3(hand_point(bodies,hand)-centre).Length();
        if (!std::isfinite(distance) || distance>1.75F) return false;
    }
    return true;
}

[[nodiscard]] Grip Simulation::PhysicsWorld::missing_radial_grip(const JPH::BodyInterface &bodies) const noexcept {
    if (!radial_ownership_eligible() || !physical_hands_->radial_active() ||
        physical_hands_->attached_count()!=1 || !actual_hands_in_reach(bodies) ||
        physical_hands_->swinging() || physical_foot_push_->active()) return {};
    const unsigned missing=physical_hands_->attached(0)?1U:0U;
    const auto planted=physical_hands_->grip_readback(1U-missing);
    if (!planted.attached || planted.material || planted.support!=traversal_body_) return {};
    const auto centre=bodies.GetCenterOfMassPosition(player_id_);
    const float height=missing==0?kClimbHandMid:kClimbHandMid+.2F;
    const auto aim=hand_aim(centre,height,missing==0?-kClimbHandSpan:kClimbHandSpan);
    const Grip candidate=find_grip(centre,aim,traversal_normal_);
    // Two existing 41 mm rendered palm radii give an 82 mm planning
    // footprint. This is a reduced-order clearance tuning bound, not
    // articulated hand collision or a new actuator limit.
    constexpr float palm_spacing_m=2*.041F;
    if (!candidate.valid || candidate.body!=planted.support ||
        JPH::Vec3(candidate.point-centre).Length()>1.75F ||
        JPH::Vec3(candidate.point-planted.world_anchor).Length()<palm_spacing_m) return {};
    return candidate;
}

[[nodiscard]] bool Simulation::PhysicsWorld::hanging_posture_eligible(const JPH::BodyInterface &bodies) const noexcept {
    if (!radial_ownership_eligible() || !actual_hands_in_reach(bodies)) return false;
    // Once acquired, the actual attached radius/reach/owner retain this
    // mode through the arc; world-up arm angle is only an entry condition.
    if (physical_hands_->radial_active()) return true;
    const auto relative=JPH::Vec3(physical_hands_->radial_grip_centroid()-bodies.GetPosition(player_id_));
    return relative.GetY()+hanging_posture_offset_m_>.35F && relative.Length()<=1.75F;
}

bool Simulation::PhysicsWorld::leave_radial_profile(const JPH::BodyInterface &bodies) noexcept {
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) return false;
    if (!physical_hands_->radial_active()) return true;
    if (!physical_hands_->set_radial_profile(false)) return false;
    if (traversal_state_ == TraversalState::Hanging) {
        // The old absolute hold must follow the zero-error fresh servo,
        // with the paid posture kept in the shared allowance ledger.
        const auto baseline=physical_hands_->commanded_position()-JPH::Vec3(0,hanging_posture_offset_m_,0);
        traversal_local_hold_=to_support_local(bodies,traversal_body_,baseline);
    }
    return true;
}

void Simulation::PhysicsWorld::sync_radial_profile(JPH::BodyInterface &bodies,const StepCommands &commands) noexcept {
    if ((traversal_state_ == TraversalState::Climbing || traversal_state_ == TraversalState::Hanging) &&
        physical_hands_->active() && !physical_hands_->compatible_hands()) {
        if (!radial_ownership_eligible() || !actual_hands_in_reach(bodies) ||
            !physical_hands_->radial_active()) {
            if (traversal_state_ == TraversalState::Hanging) release_hang(bodies);
            else let_go_climb(bodies);
            return;
        }
        // Commands are arbitrated by apply_traversal_commands. A fresh
        // Action already consumed by begin_climb must retain this row in
        // both syncs; direction never hands a solo catch to a pair servo.
        bodies.ActivateBody(player_id_);
        for (unsigned hand=0;hand<2;++hand) if (physical_hands_->attached(hand))
            bodies.ActivateBody(hands_[hand].body);
        return;
    }
    const auto right=traversal_right();
    const double climb_input=commands.move_input_x*traversal_normal_.GetX()+
        commands.move_input_z*traversal_normal_.GetZ();
    const double side_input=commands.move_input_x*right.GetX()+commands.move_input_z*right.GetZ();
    // Use the same physical traversal axes as climb/shimmy. Independent
    // world-axis deadzones swallow valid low-throw input at diagonal grips.
    // The successful reach consumes its entire transaction through both
    // profile syncs. A held direction cannot rebuild the planted row in
    // the very solve that adds its partner.
    const bool neutral=second_hand_added_this_tick_ ||
        (std::isfinite(commands.move_input_x) && std::isfinite(commands.move_input_z) &&
        std::abs(climb_input)<=kClimbInputDeadzone && std::abs(side_input)<=kClimbInputDeadzone &&
        !(physical_hands_->swinging() && commands.move_input_x!=0.0));
    const bool action=!second_hand_added_this_tick_ &&
        (commands.jump_requested || commands.traversal_requested || commands.release_requested);
    if (physical_hands_->radial_active()) {
        if (radial_ownership_eligible() && !actual_hands_in_reach(bodies)) {
            if (traversal_state_ == TraversalState::Hanging) release_hang(bodies);
            else let_go_climb(bodies);
            return;
        }
        // Successful top-out/release changes the owner before this sync.
        // A rejected neutral Action preserves the existing loaded radial row.
        if (!neutral || !radial_ownership_eligible()) (void)leave_radial_profile(bodies);
    } else if (neutral && !action && !plank_entry_overlap_is_legal() && hanging_posture_eligible(bodies)) {
        (void)physical_hands_->set_radial_profile(true);
    }
    if (physical_hands_->radial_active()) {
        // Wake actual attachments on the owner tick, outside OnStep. Rigid
        // radial rows join their native island; material grips use their adapter.
        bodies.ActivateBody(player_id_);
        for (const auto &hand:hands_) if (hand.valid) bodies.ActivateBody(hand.body);
    }
}

void Simulation::PhysicsWorld::advance_hanging_posture(const JPH::BodyInterface &bodies, JPH::Vec3 baseline_displacement,
                             float dt, float lead_limit) noexcept {
    const bool requested = hanging_posture_eligible(bodies) && hanging_posture_input_;
    if (physical_hands_->radial_active()) {
        const float desired=requested ? .25F : 0.0F;
        const float remaining=desired-hanging_posture_offset_m_;
        const float delivered=physical_hands_->advance_radial_targets(remaining,dt);
        hanging_posture_offset_m_=std::clamp(hanging_posture_offset_m_+delivered,0.0F,.25F);
        // The ledger can retain a rounding remainder smaller than the
        // radial target's float resolution. Settle only a zero-delivery
        // endpoint that cannot change the commanded span; no target,
        // motion or actuator work is granted. Otherwise that remainder
        // would keep the posture cap on a later ordinary climb forever.
        const float span=physical_hands_->radial_commanded_span_m();
        if (delivered==0.0F && span-remaining==span)
            hanging_posture_offset_m_=desired;
        return;
    }
    // CHOSEN0.25m world-up rest-target change, not a capsule/limb edit.
    // The actual85kg body, full standing collider and reciprocal hands
    // remain under Jolt. This is not an articulated free-arc swing model.
    const float desired_offset = requested ? .25F : 0.0F;
    const float remaining = desired_offset - hanging_posture_offset_m_;
    const auto combined = baseline_displacement + JPH::Vec3(0, remaining, 0);
    auto command = combined;
    if (lead_limit > 0.0F && command.Length() > lead_limit)
        command *= lead_limit / command.Length();
    const bool posture_active = requested || hanging_posture_offset_m_ > 0.0F;
    const auto delivered = physical_hands_->advance_targets(command, dt,
        posture_active ? std::optional<float>(500.0F) : std::nullopt);
    // Attribute only the actuator's actual representable combined stroke.
    // Repeated input never adds0.25m to current COM or earns another stroke.
    const double distance_sq = double(combined.LengthSq());
    if (distance_sq > 0.0) {
        const double fraction = std::clamp(double(delivered.Dot(combined)) / distance_sq, 0.0, 1.0);
        hanging_posture_offset_m_ = std::clamp(
            hanging_posture_offset_m_ + remaining * float(fraction), 0.0F, .25F);
    }
}

void Simulation::PhysicsWorld::update_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                  const float delta_seconds) noexcept {
    if (!physical_hands_->compatible_hands()) {
        if (physical_hands_->radial_active() && hanging_posture_eligible(bodies))
            advance_hanging_posture(bodies,JPH::Vec3::sZero(),delta_seconds);
        else let_go_climb(bodies);
        return;
    }
    if (physical_hands_->radial_active()) {
        advance_hanging_posture(bodies,JPH::Vec3::sZero(),delta_seconds);
        return; // No absolute hold correction or automatic regrip in the arc.
    }
    if (physical_hands_->swinging()) {
        const auto grip = .5F*(hand_point(bodies,0)+hand_point(bodies,1));
        // Human arm/torso reach, measured from the real grip to the body.
        // An overspeed miss must release rather than stretch invisible arms.
        if (JPH::Vec3(grip-bodies.GetPosition(player_id_)).Length()>1.75F) {
            let_go_climb(bodies);
            return;
        }
        const JPH::Vec3 radius(grip-parkour_swing_pivot(traversal_entity_id_));
        const JPH::Vec3 tangent = JPH::Vec3(-radius.GetY(),radius.GetX(),0).NormalizedOr(JPH::Vec3::sAxisX());
        // CHOSEN +/-0.15m body lean,0.6m/s stroke and500W measured
        // positive spring-rest-target work. No constant hinge torque.
        // Gravity, real player mass and the passive hinge create the swing.
        // The player chooses the phase; no automatic pumping or angle grant.
        const auto goal = grip+swing_rest_offset_+tangent*float(.15*commands.move_input_x);
        advance_hanging_posture(bodies, JPH::Vec3(goal-physical_hands_->commanded_position()) +
            JPH::Vec3(0, hanging_posture_offset_m_, 0), delta_seconds, .12F);
        if (grounded_ && support_entity_id_==parkour_swing_recovery(traversal_entity_id_))
            let_go_climb(bodies);
        return;
    }
    const JPH::Vec3 normal = traversal_normal_;
    const JPH::Vec3 right = traversal_right();
    const double up_input =
        commands.move_input_x * normal.GetX() + commands.move_input_z * normal.GetZ();
    const double side_input =
        commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
    if (cargo_push_seconds_>=0.0F) {
        if (cargo_push_seconds_<=1.0e-6F) {
            let_go_climb(bodies);
        } else {
            const float stroke_dt=std::min(delta_seconds,cargo_push_seconds_);
            // Voluntary0.20s stroke, not a prescribed departure speed.
            // Existing hands bound the force, command work and reactions.
            const auto direction=(-normal*kClimbJumpBackSpeed+
                JPH::Vec3(0,kClimbJumpUpSpeed,0)).Normalized();
            (void)physical_hands_->advance_targets(direction*(0.999F*stroke_dt),stroke_dt);
            cargo_push_seconds_-=stroke_dt;
        }
        return;
    }
    if (cargo_transfer_active_) {
        update_cargo_transfer(bodies,up_input,delta_seconds);
        return;
    }
    const JPH::RVec3 hold = physical_hands_->active() ?
        (cargo_net_ && traversal_body_ == cargo_net_->body() ? physical_hands_->commanded_position() : bodies.GetPosition(player_id_)) :
        from_support_local(bodies, traversal_body_, traversal_local_hold_);
    JPH::RVec3 next = hold;
    Grip carries;
    const auto follow_surface = [&](JPH::RVec3 moved) {
        if (cargo_net_ && traversal_body_ == cargo_net_->body()) {
            auto material = cargo_net_->material_point(moved);
            // Advance on the actual curved mesh with the same stand-off,
            // rather than accumulating depth error as its slope changes.
            material.SetZ(traversal_local_hold_.GetZ());
            return cargo_net_->world_point(material);
        }
        return moved;
    };
    if (up_input > kClimbInputDeadzone) {
        // A ledge in reach is climbed over before any hold above it: grab
        // rails that run on past a deck's edge must not carry the body
        // past the height it can mantle from.
        if (up_input > 0.5 && try_top_out(bodies)) {
            return;
        }
        const JPH::RVec3 moved =
            follow_surface(hold + JPH::Vec3(0.0F, static_cast<float>(up_input) * kClimbUpSpeed * delta_seconds, 0.0F));
        const Grip above = find_grip(moved, hand_aim(moved, kClimbMoveAim, 0.0F), normal);
        if (above.valid) {
            next = moved;
            carries = above;
        }
    } else if (up_input < -kClimbInputDeadzone) {
        const JPH::RVec3 moved =
            follow_surface(hold + JPH::Vec3(0.0F, static_cast<float>(up_input) * kClimbDownSpeed * delta_seconds, 0.0F));
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
            follow_surface(next + right * (static_cast<float>(side_input) * kClimbSideSpeed * delta_seconds));
        const Grip beside = find_grip(moved, hand_aim(moved, kClimbHandMid, lead), normal);
        if (beside.valid) {
            next = moved;
            carries = beside;
        }
    }
    const bool carries_movement = carries.valid && grip_matches_climb_backend(carries) && climb_move_clear(hold, next);
    if (physical_hands_->active() && (carries_movement ||
        (hanging_posture_eligible(bodies) && hanging_posture_input_) || hanging_posture_offset_m_ > 0.0F)) {
        // Ordinary climb is incremental. Add only the undelivered posture
        // component, including when no new carry grip was found.
        advance_hanging_posture(bodies, carries_movement ? JPH::Vec3(next - hold) : JPH::Vec3::sZero(),
                                delta_seconds);
    }
    if (carries_movement) {
        traversal_body_ = carries.body;
        traversal_entity_id_ = carries.entity_id;
        traversal_local_hold_ = to_support_local(bodies, traversal_body_, next);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, carries.point);
    }
    move_hands(bodies, physical_hands_->active() ? bodies.GetPosition(player_id_) :
               from_support_local(bodies, traversal_body_, traversal_local_hold_),
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
    if (grip.valid && grip_matches_climb_backend(grip)) {
        set_hand(bodies, hand, grip);
        regrip_cooldown_ticks_ = kClimbRegripTicks;
    }
}

[[nodiscard]] bool Simulation::PhysicsWorld::feet_on_ground(const JPH::RVec3 centre) const noexcept {
    JPH::RayCastResult hit;
    const JPH::Vec3 down(0.0F, -(kPlayerHalfHeight + kClimbFootReach), 0.0F);
    if (!cast_ray(centre, down, hit)) {
        return false;
    }
    const JPH::RVec3 point = JPH::RRayCast(centre, down).GetPointOnRay(hit.mFraction);
    return surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() >= kSupportNormalThreshold;
}

void Simulation::PhysicsWorld::step_off_climb(JPH::BodyInterface &bodies) noexcept {
    bodies.SetGravityFactor(player_id_, 1.0F);
    ++accepted_traversal_count_;
    clear_traversal();
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::climb_support_velocity(const JPH::BodyInterface &bodies) const noexcept {
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) {
        // Cleanup must also succeed when the held backend has left the
        // world. Read the retained rigid body only after a successful lock.
        if (!physical_hands_->radial_compatible_hands()) return JPH::Vec3::sZero();
        const unsigned hand=physical_hands_->attached(0) ? 0 : 1;
        if (!hands_[hand].valid) return JPH::Vec3::sZero();
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),hands_[hand].body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase()) return JPH::Vec3::sZero();
        const auto point=lock.GetBody().GetCenterOfMassTransform()*hands_[hand].local;
        return lock.GetBody().GetPointVelocity(point);
    }
    JPH::Vec3 velocity=JPH::Vec3::sZero();
    unsigned count=0;
    for(unsigned hand=0;hand<2;++hand) if(hands_[hand].valid) {
        velocity+=support_point_velocity(bodies,hands_[hand].body,hand_point(bodies,hand));
        ++count;
    }
    return count ? velocity/float(count) : support_point_velocity(bodies,traversal_body_,
        from_support_local(bodies,traversal_body_,traversal_local_hold_));
}

void Simulation::PhysicsWorld::let_go_climb(JPH::BodyInterface &bodies) noexcept {
    const JPH::Vec3 support_velocity = climb_support_velocity(bodies);
    if(!regression_fixtures_ || cargo_transfer_active_ || traversal_entity_id_==CargoNet::kEntity || entity_is_causal_section_support(traversal_entity_id_))
        causal_airborne_=true;
    // The rotating/deforming support velocity remains the reference for
    // airborne steering. It must not replace the body's actual velocity.
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    last_released_grip_ = traversal_body_;
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
    const auto departure=bodies.GetLinearVelocity(player_id_)-support_velocity;
    air_full_speed_=std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(),0,departure.GetZ()).Length());
}

void Simulation::PhysicsWorld::jump_off_climb(JPH::BodyInterface &bodies) noexcept {
    if(!regression_fixtures_ || cargo_transfer_active_ || traversal_entity_id_==CargoNet::kEntity ||
       entity_is_causal_section_support(traversal_entity_id_) ||
       (traversal_entity_id_==1901 && physical_hands_->active())) {
        // North-frame rungs use the same finite hand stroke and passive
        // departure as physical cargo climbs, rather than a velocity grant.
        if(cargo_push_seconds_<0.0F) cargo_push_seconds_=0.20F;
        return;
    }
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    const JPH::Vec3 support_velocity = support_point_velocity(bodies, traversal_body_, hold);
    bodies.SetLinearVelocity(player_id_, support_velocity -
                                             traversal_normal_ * kClimbJumpBackSpeed +
                                             JPH::Vec3(0.0F, kClimbJumpUpSpeed, 0.0F));
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    last_released_grip_ = traversal_body_;
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
    const auto departure=bodies.GetLinearVelocity(player_id_)-support_velocity;
    air_full_speed_=std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(),0,departure.GetZ()).Length());
}

} // namespace scraperx::sim
