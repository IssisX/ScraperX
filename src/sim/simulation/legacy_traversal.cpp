#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] bool Simulation::PhysicsWorld::try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept {
    if (!grounded_ || facing_.IsNearZero()) {
        return false;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const float feet_y = origin.GetY() - kPlayerHalfHeight;

    const LedgeProbe vault_probe =
        probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
    if (vault_probe.valid) {
        if(!regression_fixtures_ || entity_is_causal_section_support(vault_probe.ledge_entity_id))
            return begin_cargo_transfer(bodies,vault_probe,origin);
        JPH::RVec3 landing_centre;
        JPH::BodyID landing_body;
        if (probe_vault_landing(vault_probe, facing_, feet_y, landing_centre, landing_body)) {
            begin_vault(bodies, vault_probe, landing_centre, landing_body, origin);
            return true;
        }
    }

    const LedgeProbe mantle_probe =
        probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
    if (!mantle_probe.valid) {
        return false;
    }
    // Production Tower lips and the teeter ballast roof use the same
    // finite hand transfer as the repaired1.7s mantle. Preserve other
    // legacy fixture/vault owners until their replacements are proven.
    // A reachable lip must engage both hands; there is no gravity-off
    // fallback when the real attachment or receiving clearance fails.
    if(!regression_fixtures_ || entity_is_causal_section_support(mantle_probe.ledge_entity_id) ||
       (!regression_fixtures_ && (mantle_probe.ledge_entity_id==Simulation::kTowerEntityId ||
                                 mantle_probe.ledge_entity_id==2801)))
        return begin_cargo_transfer(bodies,mantle_probe,origin);
    begin_mantle(bodies, mantle_probe, origin);
    return true;
}

[[nodiscard]] bool Simulation::PhysicsWorld::step_is_supported(const JPH::RVec3 centre) const noexcept {
    const JPH::RVec3 origin(centre.GetX(),
                            centre.GetY() - kPlayerHalfHeight + kLandingSupportProbeUp,
                            centre.GetZ());
    const JPH::Vec3 direction(
        0.0F, -(kLandingSupportProbeUp + kMantleApproachLift + kLandingSupportTolerance), 0.0F);
    JPH::RayCastResult hit;
    if (!cast_ray(origin, direction, hit)) {
        return false;
    }
    const JPH::RVec3 point = JPH::RRayCast(origin, direction).GetPointOnRay(hit.mFraction);
    return surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() >= kSupportNormalThreshold;
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::mantle_approach_end(const JPH::BodyInterface &bodies,
                                             const LedgeProbe &probe,
                                             const JPH::RVec3 origin) const noexcept {
    // The path lives in the ledge's frame; ground moving against that
    // frame would carry the feet off it mid-step.
    const JPH::Vec3 drift = current_support_point_velocity(bodies) -
                            bodies.GetPointVelocity(probe.ledge_body, origin);
    if (drift.Length() > kMantleApproachFrameDriftMps) {
        return origin;
    }
    const JPH::Vec3 to_wall(static_cast<float>(probe.wall_point.GetX() - origin.GetX()),
                            0.0F,
                            static_cast<float>(probe.wall_point.GetZ() - origin.GetZ()));
    const float wall_distance = to_wall.Length();
    const float travel = wall_distance - kMantleApproachStandoff;
    if (travel <= kMantleApproachSweepMargin) {
        return origin;
    }
    const JPH::Vec3 direction = to_wall / wall_distance;
    const JPH::RVec3 lifted(origin.GetX(), origin.GetY() + kMantleApproachLift, origin.GetZ());
    if (!capsule_pose_is_clear(lifted)) {
        return origin;
    }

    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const JPH::RShapeCast sweep(player_shape_,
                                JPH::Vec3::sReplicate(1.0F),
                                JPH::RMat44::sTranslation(lifted),
                                direction * travel);
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CastShape(
        sweep, JPH::ShapeCastSettings(), lifted, collector, {}, {}, body_filter);
    float allowed = travel;
    if (collector.HadHit()) {
        allowed = collector.mHit.mFraction * travel - kMantleApproachSweepMargin;
    }

    // Back off from the swept limit until the step ends on support: a step
    // over a gap is a float, not a step.
    for (; allowed > kMantleApproachSweepMargin; allowed -= kMantleApproachSupportSpacing) {
        const JPH::RVec3 step_end = lifted + direction * allowed;
        if (step_is_supported(step_end)) {
            return step_end;
        }
    }
    return origin;
}

void Simulation::PhysicsWorld::begin_mantle(JPH::BodyInterface &bodies,
                  const LedgeProbe &probe,
                  const JPH::RVec3 origin) noexcept {
    if (!regression_fixtures_) {
        (void)begin_cargo_transfer(bodies,probe,origin);
        return;
    }
    const JPH::RVec3 approach = mantle_approach_end(bodies, probe, origin);
    const float step_length = static_cast<float>(
        std::hypot(approach.GetX() - origin.GetX(), approach.GetZ() - origin.GetZ()));
    const double approach_seconds = static_cast<double>(step_length / kMantleApproachSpeedMps);
    // A player running in keeps their speed into the step; one standing
    // still starts it from rest.
    float entry_slope = 0.0F;
    if (step_length > 0.0F) {
        const JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) -
                                   bodies.GetPointVelocity(probe.ledge_body, origin);
        const float toward =
            relative.GetX() * facing_.GetX() + relative.GetZ() * facing_.GetZ();
        entry_slope = std::clamp(
            toward / kMantleApproachSpeedMps, 0.0F, kMantleApproachMaximumEntrySlope);
    }

    traversal_state_ = TraversalState::Mantling;
    traversal_body_ = probe.ledge_body;
    traversal_entity_id_ = probe.ledge_entity_id;
    traversal_target_body_ = probe.landing_body;
    traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
    traversal_local_approach_ = to_support_local(bodies, traversal_body_, approach);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
    traversal_local_target_ =
        to_support_local(bodies, traversal_target_body_, probe.landing_centre);
    traversal_progress_ = 0.0;
    traversal_duration_ = kMantleDurationSeconds + approach_seconds;
    traversal_approach_fraction_ = approach_seconds / traversal_duration_;
    traversal_approach_entry_slope_ = entry_slope;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = origin;
    traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    bodies.SetGravityFactor(player_id_, 0.0F);
}

void Simulation::PhysicsWorld::begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept {
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) return;
    if (!leave_radial_profile(bodies)) return;
    // Catching a lip needs room for a hanging body. Climbing onto it is
    // separately guarded by the current full standing capsule clearance.
    // The support may have moved, or the player may have shimmied to a
    // different patch since the initial catch.
    const JPH::RVec3 landing =
        from_support_local(bodies, traversal_target_body_, traversal_local_target_);
    if (!capsule_pose_is_clear(landing)) {
        ++rejected_traversal_count_;
        return;
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    if(!regression_fixtures_ || entity_is_causal_section_support(traversal_entity_id_) ||
       (traversal_entity_id_==1901 && physical_hands_->active())) {
        const auto remembered_ledge =
            from_support_local(bodies,traversal_body_,traversal_local_ledge_);
        const Lip lip = probe_lip(remembered_ledge,traversal_normal_);
        if (!lip.valid || lip.body != traversal_body_ ||
            JPH::Vec3(lip.ledge-remembered_ledge).Length() > .15F) {
            ++rejected_traversal_count_;
            return;
        }
        LedgeProbe probe;
        probe.ledge_body=lip.body;
        // A reconstructed hang probe must retain the actual compound
        // child identity. A default SubShapeID cannot validate a plan.
        probe.ledge_sub_shape=lip.sub_shape;
        probe.ledge_entity_id=lip.entity_id;
        probe.landing_body=traversal_target_body_;
        probe.ledge_point=lip.ledge;
        probe.landing_centre=landing;
        probe.inward=traversal_normal_;
        if(!begin_cargo_transfer(bodies,probe,origin)) ++rejected_traversal_count_;
        return;
    }
    physical_hands_->clear();
    traversal_state_ = TraversalState::Mantling;
    traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
    traversal_local_approach_ = traversal_local_start_;
    traversal_progress_ = 0.0;
    traversal_duration_ = kMantleDurationSeconds;
    traversal_approach_fraction_ = 0.0;
    traversal_approach_entry_slope_ = 0.0F;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = origin;
    traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    bodies.SetGravityFactor(player_id_, 0.0F);
}

void Simulation::PhysicsWorld::begin_vault(JPH::BodyInterface &bodies,
                 const LedgeProbe &probe,
                 const JPH::RVec3 landing_centre,
                 const JPH::BodyID landing_body,
                 const JPH::RVec3 origin) noexcept {
    if (!regression_fixtures_) {
        (void)begin_cargo_transfer(bodies,probe,origin);
        return;
    }
    const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    const JPH::Vec3 support_velocity =
        grounded_ ? current_support_point_velocity(bodies) : airborne_inherited_velocity_;
    JPH::Vec3 relative(player_velocity.GetX() - support_velocity.GetX(),
                       0.0F,
                       player_velocity.GetZ() - support_velocity.GetZ());
    const float relative_speed = relative.Length();
    if (relative_speed > kPlayerMaximumRelativeSpeed) {
        relative = relative * (kPlayerMaximumRelativeSpeed / relative_speed);
    }

    const double travel_x = landing_centre.GetX() - origin.GetX();
    const double travel_z = landing_centre.GetZ() - origin.GetZ();
    const double travel_squared = travel_x * travel_x + travel_z * travel_z;
    traversal_vault_horizontal_slope_ = travel_squared > 1.0e-6
        ? std::clamp(static_cast<float>(
              kVaultDurationSeconds * (relative.GetX() * travel_x +
                                       relative.GetZ() * travel_z) / travel_squared),
              0.0F, 3.0F)
        : 0.0F;

    const JPH::RVec3 apex(origin.GetX(),
                          probe.ledge_point.GetY() + kPlayerHalfHeight + kVaultApexClearance,
                          origin.GetZ());

    traversal_state_ = TraversalState::Vaulting;
    traversal_body_ = probe.ledge_body;
    traversal_entity_id_ = probe.ledge_entity_id;
    traversal_target_body_ = landing_body;
    traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
    traversal_local_apex_ = to_support_local(bodies, traversal_body_, apex);
    traversal_local_target_ = to_support_local(bodies, traversal_target_body_, landing_centre);
    traversal_progress_ = 0.0;
    traversal_duration_ = kVaultDurationSeconds;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = origin;
    traversal_exit_relative_velocity_ = relative;
    bodies.SetGravityFactor(player_id_, 0.0F);
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::traversal_point(const JPH::BodyInterface &bodies,
                                         const float progress) const noexcept {
    const JPH::RVec3 start = from_support_local(bodies, traversal_body_, traversal_local_start_);
    const JPH::RVec3 target =
        from_support_local(bodies, traversal_target_body_, traversal_local_target_);

    if (traversal_state_ == TraversalState::Lowering) {
        // Back over the edge at standing height, then down to the hold
        // once the body is clear of the lip.
        const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        const float over = smoothstep(0.0F, 0.55F, progress);
        const float down = smoothstep(0.55F, 1.0F, progress);
        return JPH::RVec3(start.GetX() + (hold.GetX() - start.GetX()) * over,
                          start.GetY() + (hold.GetY() - start.GetY()) * down,
                          start.GetZ() + (hold.GetZ() - start.GetZ()) * over);
    }

    if (traversal_state_ == TraversalState::Vaulting) {
        const JPH::RVec3 apex = from_support_local(bodies, traversal_body_, traversal_local_apex_);
        const float horizontal = ease_with_carried_speed(
            progress, traversal_vault_horizontal_slope_);
        float height;
        if (progress < 0.5F) {
            height = start.GetY() +
                     (apex.GetY() - start.GetY()) * smoothstep(0.0F, 1.0F, progress * 2.0F);
        } else {
            height = apex.GetY() + (target.GetY() - apex.GetY()) *
                                       smoothstep(0.0F, 1.0F, (progress - 0.5F) * 2.0F);
        }
        return JPH::RVec3(start.GetX() + (target.GetX() - start.GetX()) * horizontal,
                          height,
                          start.GetZ() + (target.GetZ() - start.GetZ()) * horizontal);
    }

    // Step in (standing mantles only), then the climb from the step's end.
    const JPH::RVec3 approach =
        from_support_local(bodies, traversal_body_, traversal_local_approach_);
    const float approach_fraction = static_cast<float>(traversal_approach_fraction_);
    if (progress < approach_fraction) {
        const float phase = progress / approach_fraction;
        const float along = ease_to_rest(phase, traversal_approach_entry_slope_);
        const float up = smoothstep(0.0F, 1.0F, phase);
        return JPH::RVec3(start.GetX() + (approach.GetX() - start.GetX()) * along,
                          start.GetY() + (approach.GetY() - start.GetY()) * up,
                          start.GetZ() + (approach.GetZ() - start.GetZ()) * along);
    }
    const float climb = approach_fraction > 0.0F
                            ? (progress - approach_fraction) / (1.0F - approach_fraction)
                            : progress;
    const float vertical = smoothstep(0.0F, 0.55F, climb);
    const float horizontal = smoothstep(0.45F, 1.0F, climb);
    const float lift =
        kMantleClearanceLift * std::sin(static_cast<float>(kPi) * std::clamp(climb, 0.0F, 1.0F));
    return JPH::RVec3(approach.GetX() + (target.GetX() - approach.GetX()) * horizontal,
                      approach.GetY() + (target.GetY() - approach.GetY()) * vertical + lift,
                      approach.GetZ() + (target.GetZ() - approach.GetZ()) * horizontal);
}

void Simulation::PhysicsWorld::drive_traversal(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
    const JPH::RVec3 current = bodies.GetPosition(player_id_);

    if (traversal_state_ == TraversalState::Hanging ||
        traversal_state_ == TraversalState::Climbing) {
        traversal_desired_ = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    } else {
        traversal_progress_ =
            std::min(1.0, traversal_progress_ + static_cast<double>(delta_seconds) /
                                                    traversal_duration_);
        traversal_desired_ = traversal_point(bodies, static_cast<float>(traversal_progress_));
    }

    bodies.SetLinearVelocity(player_id_,
                             JPH::Vec3(traversal_desired_ - current) / delta_seconds);
}

void Simulation::PhysicsWorld::resolve_traversal_outcome(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 actual = bodies.GetPosition(player_id_);
    const float error = JPH::Vec3(actual - traversal_desired_).Length();
    if (error > kTraversalStallTolerance) {
        ++traversal_stall_ticks_;
    } else {
        traversal_stall_ticks_ = 0;
    }

    if (traversal_stall_ticks_ >= kTraversalStallAbortTicks) {
        abort_traversal(bodies);
        return;
    }

    if (traversal_state_ == TraversalState::Lowering && traversal_progress_ >= 1.0) {
        finish_lowering(bodies);
    } else if (traversal_state_ != TraversalState::Hanging &&
               traversal_state_ != TraversalState::Climbing && traversal_progress_ >= 1.0) {
        complete_traversal(bodies);
    }
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::traversal_support_point_velocity(
    const JPH::BodyInterface &bodies) const noexcept {
    if (traversal_target_body_.IsInvalid()) {
        return JPH::Vec3::sZero();
    }
    const JPH::RVec3 target =
        from_support_local(bodies, traversal_target_body_, traversal_local_target_);
    return support_point_velocity(bodies, traversal_target_body_, target);
}

void Simulation::PhysicsWorld::complete_traversal(JPH::BodyInterface &bodies) noexcept {
    const JPH::Vec3 support_velocity = traversal_support_point_velocity(bodies);
    JPH::Vec3 exit_velocity = support_velocity;
    if (traversal_state_ == TraversalState::Vaulting) {
        exit_velocity.SetX(support_velocity.GetX() + traversal_exit_relative_velocity_.GetX());
        exit_velocity.SetZ(support_velocity.GetZ() + traversal_exit_relative_velocity_.GetZ());
    }
    bodies.SetLinearVelocity(player_id_, exit_velocity);
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    ++accepted_traversal_count_;
    clear_traversal();
}

void Simulation::PhysicsWorld::abort_traversal(JPH::BodyInterface &bodies) noexcept {
    JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    const JPH::Vec3 support_velocity = traversal_support_point_velocity(bodies);
    JPH::Vec3 relative(velocity.GetX() - support_velocity.GetX(),
                       0.0F,
                       velocity.GetZ() - support_velocity.GetZ());
    const float relative_speed = relative.Length();
    if (relative_speed > kPlayerMaximumRelativeSpeed) {
        relative = relative * (kPlayerMaximumRelativeSpeed / relative_speed);
    }
    velocity.SetX(support_velocity.GetX() + relative.GetX());
    velocity.SetZ(support_velocity.GetZ() + relative.GetZ());
    bodies.SetLinearVelocity(player_id_, velocity);
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    ++aborted_traversal_count_;
    clear_traversal();
}

void Simulation::PhysicsWorld::release_hang(JPH::BodyInterface &bodies) noexcept {
    if(!regression_fixtures_ || entity_is_causal_section_support(traversal_entity_id_)) causal_airborne_=true;
    JPH::Vec3 support_velocity=JPH::Vec3::sZero();
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) {
        support_velocity=climb_support_velocity(bodies);
    } else if (!traversal_body_.IsInvalid()) {
        const auto ledge=from_support_local(bodies,traversal_body_,traversal_local_ledge_);
        support_velocity=support_point_velocity(bodies,traversal_body_,ledge);
    }
    // Detachment changes the constraint, not the rider's momentum. The
    // current velocity already includes motion acquired while attached.
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    last_released_grip_ = traversal_body_;
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
    const auto departure=bodies.GetLinearVelocity(player_id_)-support_velocity;
    air_full_speed_=std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(),0,departure.GetZ()).Length());
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

} // namespace scraperx::sim
