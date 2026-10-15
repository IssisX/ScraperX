#include "sim/physics_internal.hpp"

// Ledge probes, mantles, vaults, hangs, lips, swings, and shimmy.

namespace scraperx::sim {

const JPH::Shape *Simulation::PhysicsWorld::active_player_shape() const noexcept {
    return crouched_ ? player_crouch_shape_.GetPtr() : player_shape_.GetPtr();
}

bool Simulation::PhysicsWorld::capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept {
    return shape_pose_is_clear(active_player_shape(), centre);
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

LedgeProbe Simulation::PhysicsWorld::probe_ledge(const JPH::RVec3 origin,
                                     const JPH::Vec3 facing,
                                     const float minimum_rise,
                                     const float maximum_rise,
                                     const bool require_supported_landing,
                                     const bool see_past_holds) const noexcept {
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

    const float feet_y = origin.GetY() - kPlayerHalfHeight;
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
    if (top_hit.mBodyID != wall_hit.mBodyID) {
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
        if (landing_hit.mBodyID != top_hit.mBodyID) {
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

void Simulation::PhysicsWorld::apply_traversal_commands(JPH::BodyInterface &bodies,
                              const StepCommands &commands) noexcept {
    if (traversal_state_ == TraversalState::Hanging) {
        if (commands.release_requested) {
            release_hang(bodies);
        } else if (pitman_grip_ != nullptr) {
            // Jump and Action mantle a ledge. On this bar they do not:
            // the stick pumps it, and letting go is the release.
            if (commands.jump_requested || commands.traversal_requested) {
                ++rejected_traversal_count_;
            }
        } else if (commands.jump_requested || commands.traversal_requested) {
            begin_mantle_from_hang(bodies);
        }
        return;
    }

    if (traversal_state_ == TraversalState::Climbing) {
        if (commands.release_requested) {
            let_go_climb(bodies);
        } else if (commands.jump_requested) {
            jump_off_climb(bodies);
        } else if (commands.traversal_requested && !try_top_out(bodies)) {
            ++rejected_traversal_count_;
        }
        return;
    }

    if (traversal_state_ != TraversalState::None) {
        if (commands.traversal_requested || commands.release_requested) {
            ++rejected_traversal_count_;
        }
        return;
    }

    // Still crouched here means update_crouch could not stand the body:
    // every vault, mantle and hang is a standing pose, so none begins.
    if (crouched_) {
        jump_vault_ticks_left_ = 0;
        if (commands.traversal_requested) {
            ++rejected_traversal_count_;
        }
        return;
    }

    // Both hands on a carried body (AS-003 §8.4.3): vault, mantle and hang
    // are all pull-ups, and none begins until it is set down.
    if (carry_constraint_ != nullptr) {
        jump_vault_ticks_left_ = 0;
        if (commands.traversal_requested) {
            ++rejected_traversal_count_;
        }
        return;
    }

    if (jump_vault_ticks_left_ > 0) {
        --jump_vault_ticks_left_;
        if (commands.jump_requested && try_begin_jump_vault(bodies)) {
            jump_vault_ticks_left_ = 0;
            return;
        }
    }

    // Drop with an edge behind: lower over it into a hang.
    if (commands.release_requested && grounded_) {
        if (!try_begin_lowering(bodies)) {
            ++rejected_traversal_count_;
        }
        return;
    }

    // Action: a vault or mantle where one is offered, else a hold faced
    // at hand height is climbed.
    if (commands.traversal_requested && !try_begin_ground_traversal(bodies)) {
        if (!grounded_) {
            if (try_air_catch(bodies)) {
                return;
            }
            ++rejected_traversal_count_;
            return;
        }
        const Grip grip = grip_in_front(bodies);
        if (grip.valid) {
            begin_climb(bodies, grip);
        } else {
            ++rejected_traversal_count_;
        }
    }
}

bool Simulation::PhysicsWorld::try_begin_jump_vault(JPH::BodyInterface &bodies) noexcept {
    if (facing_.IsNearZero()) {
        return false;
    }
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const JPH::RVec3 takeoff(at.GetX(), jump_takeoff_feet_y_ + kPlayerHalfHeight, at.GetZ());
    const LedgeProbe probe =
        probe_ledge(takeoff, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
    if (!probe.valid) {
        return false;
    }
    JPH::RVec3 landing_centre;
    JPH::BodyID landing_body;
    if (!probe_vault_landing(probe, facing_, jump_takeoff_feet_y_, landing_centre,
                             landing_body)) {
        return false;
    }
    begin_vault(bodies, probe, landing_centre, landing_body, at);
    ++jump_vault_count_;
    return true;
}

void Simulation::PhysicsWorld::try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (grounded_ || crouched_ || carry_constraint_ != nullptr || regrab_lockout_ticks_ > 0) {
        return;
    }
    if (bodies.GetLinearVelocity(player_id_).GetY() > kHangMaximumClimbSpeed) {
        return;
    }
    if (facing_.IsNearZero()) {
        return;
    }
    const double intent = commands.move_input_x * static_cast<double>(facing_.GetX()) +
                          commands.move_input_z * static_cast<double>(facing_.GetZ());
    if (intent < kHangIntentDotThreshold) {
        return;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const LedgeProbe probe = probe_ledge(origin,
                                         facing_,
                                         kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                         kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                                         true);
    if (!probe.valid) {
        // No ledge: a hold reached for in the air is caught and climbed.
        const Grip grip = find_grip(
            origin, origin + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach,
            facing_);
        if (grip.valid) {
            begin_climb(bodies, grip);
            return;
        }
        const Lip lip = lip_in_front(bodies);
        if (lip.valid) {
            begin_hang_on_lip(bodies, lip);
        }
        return;
    }

    const JPH::RVec3 hold(probe.wall_point.GetX() - facing_.GetX() * (kPlayerRadius + kHangWallGap),
                          probe.ledge_point.GetY() - kHangDropBelowLedge,
                          probe.wall_point.GetZ() - facing_.GetZ() * (kPlayerRadius + kHangWallGap));

    traversal_state_ = TraversalState::Hanging;
    traversal_body_ = probe.ledge_body;
    traversal_entity_id_ = probe.ledge_entity_id;
    traversal_target_body_ = probe.landing_body;
    traversal_normal_ = facing_;
    traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
    traversal_local_target_ =
        to_support_local(bodies, traversal_target_body_, probe.landing_centre);
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = hold;
    bodies.SetGravityFactor(player_id_, 0.0F);
}

bool Simulation::PhysicsWorld::try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept {
    if (!grounded_ || facing_.IsNearZero()) {
        return false;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const float feet_y = origin.GetY() - kPlayerHalfHeight;

    const LedgeProbe vault_probe =
        probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
    if (vault_probe.valid) {
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
    begin_mantle(bodies, mantle_probe, origin);
    return true;
}

bool Simulation::PhysicsWorld::step_is_supported(const JPH::RVec3 centre) const noexcept {
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

JPH::RVec3 Simulation::PhysicsWorld::mantle_approach_end(const JPH::BodyInterface &bodies,
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
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
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

JPH::RVec3 Simulation::PhysicsWorld::traversal_point(const JPH::BodyInterface &bodies,
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

    // The pitman hold is a point constraint. The bar moves the body.
    // A kinematic chase on top of that would fight the hinge.
    if (pitman_grip_ != nullptr) {
        traversal_desired_ = current;
        return;
    }

    if (traversal_state_ == TraversalState::Hanging ||
        traversal_state_ == TraversalState::Climbing) {
        traversal_desired_ = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        if (traversal_state_ == TraversalState::Hanging && swing_angle_ > 1.0e-4F) {
            const JPH::RVec3 ledge =
                from_support_local(bodies, traversal_body_, traversal_local_ledge_);
            const JPH::Quat swung =
                JPH::Quat::sRotation(traversal_right(), -swing_angle_);
            traversal_desired_ = ledge + swung * JPH::Vec3(traversal_desired_ - ledge);
        }
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
        finish_lowering();
    } else if (traversal_state_ != TraversalState::Hanging &&
               traversal_state_ != TraversalState::Climbing && traversal_progress_ >= 1.0) {
        complete_traversal(bodies);
    }
}

JPH::Vec3 Simulation::PhysicsWorld::traversal_support_point_velocity(
    const JPH::BodyInterface &bodies) const noexcept {
    if (traversal_target_body_.IsInvalid()) {
        return JPH::Vec3::sZero();
    }
    const JPH::RVec3 target =
        from_support_local(bodies, traversal_target_body_, traversal_local_target_);
    return bodies.GetPointVelocity(traversal_target_body_, target);
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

void Simulation::PhysicsWorld::release_with_speed(JPH::BodyInterface &bodies, const JPH::Vec3 support_velocity) noexcept {
    JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) - support_velocity;
    const float speed = relative.Length();
    if (speed > kReleaseSpeedCap) {
        relative *= kReleaseSpeedCap / speed;
    }
    bodies.SetLinearVelocity(player_id_, support_velocity + relative);
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
}

void Simulation::PhysicsWorld::release_hang(JPH::BodyInterface &bodies) noexcept {
    if (pitman_grip_ != nullptr) {
        release_pitman_hang(bodies);
        return;
    }
    const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::Vec3 support_velocity =
        traversal_body_.IsInvalid() ? JPH::Vec3::sZero()
                                    : bodies.GetPointVelocity(traversal_body_, ledge);
    release_with_speed(bodies, support_velocity);
}

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

void Simulation::PhysicsWorld::update_swing(JPH::BodyInterface &bodies, const StepCommands &commands,
                  const float delta_seconds) noexcept {
    if (pitman_grip_ != nullptr) {
        pump_pitman(bodies, commands);
        return;
    }
    if (traversal_normal_.IsNearZero()) {
        return;
    }
    const double into = commands.move_input_x * traversal_normal_.GetX() +
                        commands.move_input_z * traversal_normal_.GetZ();
    const float pump = into < -kClimbInputDeadzone ? static_cast<float>(-into) : 0.0F;
    const float spring = pump * kSwingPump - swing_angle_ * kSwingSpring - swing_rate_ * kSwingDamp;
    swing_rate_ += spring * delta_seconds;
    swing_angle_ = std::clamp(swing_angle_ + swing_rate_ * delta_seconds, 0.0F, kSwingMax);
}

Lip Simulation::PhysicsWorld::lip_in_front(const JPH::BodyInterface &bodies) const noexcept {
    Lip none;
    if (facing_.IsNearZero()) {
        return none;
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    JPH::RayCastResult wall_hit;
    JPH::RVec3 wall_point;
    if (!cast_wall(origin, facing_ * (kTraversalReach + kPlayerRadius), true, wall_hit,
                   wall_point)) {
        return none;
    }
    const Lip lip = probe_lip(wall_point + JPH::Vec3(0.0F, 0.15F, 0.0F), facing_);
    if (!lip.valid || !capsule_pose_is_clear(lip.hold)) {
        return none;
    }
    if (JPH::Vec3(lip.hold - origin).Length() > kLipCatchReach) {
        return none;
    }
    const float above_centre = static_cast<float>(lip.ledge.GetY() - origin.GetY());
    if (above_centre < 0.15F || above_centre > 1.55F) {
        return none;
    }
    return lip;
}

void Simulation::PhysicsWorld::begin_hang_on_lip(JPH::BodyInterface &bodies, const Lip &lip) noexcept {
    traversal_state_ = TraversalState::Hanging;
    traversal_body_ = lip.body;
    traversal_entity_id_ = lip.entity_id;
    traversal_target_body_ = lip.body;
    traversal_normal_ = facing_;
    traversal_local_hold_ = to_support_local(bodies, lip.body, lip.hold);
    traversal_local_ledge_ = to_support_local(bodies, lip.body, lip.ledge);
    traversal_local_target_ = to_support_local(bodies, lip.body, lip.landing);
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    swing_angle_ = 0.0F;
    swing_rate_ = 0.0F;
    traversal_desired_ = lip.hold;
    bodies.SetGravityFactor(player_id_, 0.0F);
}

bool Simulation::PhysicsWorld::try_air_catch(JPH::BodyInterface &bodies) noexcept {
    if (crouched_ || carry_constraint_ != nullptr || facing_.IsNearZero() ||
        regrab_lockout_ticks_ > 0) {
        return false;
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const Grip grip = find_grip(
        origin, origin + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach,
        facing_);
    if (grip.valid) {
        begin_climb(bodies, grip);
        return true;
    }
    const Lip lip = lip_in_front(bodies);
    if (!lip.valid) {
        return false;
    }
    begin_hang_on_lip(bodies, lip);
    return true;
}

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
}

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

void Simulation::PhysicsWorld::finish_lowering() noexcept {
    traversal_state_ = TraversalState::Hanging;
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    ++accepted_traversal_count_;
}

void Simulation::PhysicsWorld::clear_traversal() noexcept {
    if (pitman_grip_ != nullptr) {
        physics_system_.RemoveConstraint(pitman_grip_);
        pitman_grip_ = nullptr;
    }
    traversal_state_ = TraversalState::None;
    traversal_normal_ = JPH::Vec3::sZero();
    hands_[0].valid = false;
    hands_[1].valid = false;
    swing_angle_ = 0.0F;
    swing_rate_ = 0.0F;
    air_full_speed_ = kPlayerMaximumRelativeSpeed;
    traversal_body_ = {};
    traversal_target_body_ = {};
    traversal_entity_id_ = 0;
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    traversal_vault_horizontal_slope_ = 0.0F;
}

void Simulation::PhysicsWorld::update_affordance(const JPH::BodyInterface &bodies) noexcept {
    affordance_ = {};
    grip_affordance_ = {};
    edge_affordance_ = {};
    carry_target_entity_ = 0;
    (void)carry_candidate(bodies, carry_target_entity_);
    find_rig_action(bodies);
    if (traversal_state_ == TraversalState::None && !crouched_ &&
        carry_constraint_ == nullptr && !facing_.IsNearZero()) {
        if (grounded_) {
            grip_affordance_ = grip_in_front(bodies);
            edge_affordance_ = probe_edge_drop(bodies);
        } else if (regrab_lockout_ticks_ == 0) {
            const JPH::RVec3 origin = bodies.GetPosition(player_id_);
            grip_affordance_ = find_grip(
                origin,
                origin + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach,
                facing_);
        }
    }
    // The probes measure rises from standing feet and test standing
    // landing poses; a crouched body is offered none (a request stands
    // it first, see update_crouch). Hands full, no ledge is offered at all.
    if (traversal_state_ != TraversalState::None || facing_.IsNearZero() || crouched_ ||
        carry_constraint_ != nullptr) {
        return;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    if (grounded_) {
        const LedgeProbe vault_probe =
            probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
        if (vault_probe.valid) {
            JPH::RVec3 landing_centre;
            JPH::BodyID landing_body;
            if (probe_vault_landing(vault_probe,
                                    facing_,
                                    origin.GetY() - kPlayerHalfHeight,
                                    landing_centre,
                                    landing_body)) {
                affordance_ = vault_probe;
                return;
            }
        }
        affordance_ = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
        return;
    }

    affordance_ = probe_ledge(origin,
                              facing_,
                              kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                              kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                              true);
    if (!affordance_.valid && regrab_lockout_ticks_ == 0) {
        const Lip lip = lip_in_front(bodies);
        if (lip.valid) {
            affordance_.valid = true;
            affordance_.ledge_body = lip.body;
            affordance_.ledge_entity_id = lip.entity_id;
            affordance_.ledge_point = lip.ledge;
            affordance_.rise =
                static_cast<float>(lip.ledge.GetY() - (origin.GetY() - kPlayerHalfHeight));
        }
    }
}

} // namespace scraperx::sim
