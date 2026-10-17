// Ledges: the hang, vault and mantle, and the traversal state machine that drives them.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim {

void Simulation::PhysicsWorld::try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (grounded_ || crouched_ || carry_constraint_ != nullptr || regrab_lockout_ticks_ > 0) {
        return;
    }
    // Rising, the hands close only in a jump (M6's cat leap): a step's
    // lift pops the body up at the same 2 m/s, reaching for nothing.
    const float rising = bodies.GetLinearVelocity(player_id_).GetY();
    if (rising > (flight_from_jump_ ? kHangMaximumClimbSpeed : kHangApexSpeed)) {
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
        // No ledge: a hold reached for in the air is caught and climbed --
        // rising (M6), only where the first pull up from it is clear.
        const Grip grip = find_grip(
            origin, origin + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach,
            facing_);
        if (grip.valid) {
            if (rising <= kHangApexSpeed ||
                climb_move_clear(origin, origin + JPH::Vec3(0.0F, kClimbMoveAim, 0.0F))) {
                begin_climb(bodies, grip);
            }
            return;
        }
        // None in front: a bar or pipe over the head is swung from -- in
        // flight (a jump, or no ground within a step under the soles),
        // never on a step's brief lift.
        if (!flight_from_jump_ &&
            feet_supported_at(origin, static_cast<float>(origin.GetY()) - active_half_height())) {
            return;
        }
        const Grip bar = find_grip(
            origin, origin + JPH::Vec3(0.0F, kSwingCatchAimRise, 0.0F) + facing_ * kSwingCatchAimAhead,
            facing_, -kPlayerRadius);
        if (bar.valid && bar.point.GetY() - origin.GetY() >= kSwingMinHandRise) {
            begin_swing(bodies, bar);
        }
        return;
    }

    begin_hang(bodies, probe, origin);
}

// The hands closed on `probe`'s lip, the body hung from them by the arms.
void Simulation::PhysicsWorld::begin_hang(JPH::BodyInterface &bodies, const LedgeProbe &probe, const JPH::RVec3 origin) noexcept {
    const JPH::RVec3 hold(probe.wall_point.GetX() - facing_.GetX() * (kPlayerRadius + kHangWallGap),
                          probe.ledge_point.GetY() - kHangDropBelowLedge,
                          probe.wall_point.GetZ() - facing_.GetZ() * (kPlayerRadius + kHangWallGap));

    traversal_state_ = TraversalState::Hanging;
    traversal_entry_offset_ = JPH::Vec3(origin - hold);
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
    traversal_desired_ = origin;
    attach_hang_arms(bodies);
}

// How far ahead a vault is offered and taken: arm's length at a walk; at
// a run (kSpeedVaultMinimumSpeed) a quarter second's travel, up to 2 m --
// a speed vault takes off well short of the rail, not at it.
float Simulation::PhysicsWorld::vault_reach(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) - airborne_inherited_velocity_;
    const float along = relative.GetX() * facing_.GetX() + relative.GetZ() * facing_.GetZ();
    if (along < static_cast<float>(kSpeedVaultMinimumSpeed)) {
        return kTraversalReach;
    }
    return std::clamp(along * kSpeedVaultLookaheadSeconds, kTraversalReach, kSpeedVaultMaximumReach);
}

bool Simulation::PhysicsWorld::try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept {
    if (!grounded_ || facing_.IsNearZero()) {
        return false;
    }

    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const float feet_y = origin.GetY() - kPlayerHalfHeight;

    const LedgeProbe vault_probe =
        probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false, false, vault_reach(bodies));
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

// Real support under the capsule's axis at `centre`: walkable ground no
// deeper than the step's own lift plus the landing tolerance.
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

// Where a standing mantle's step-in ends (see kMantleApproachStandoff).
// Returns `origin` itself when no step is possible.
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
    {
        const JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) -
                                   bodies.GetPointVelocity(probe.ledge_body, origin);
        const float along = std::max(0.0F, relative.GetX() * facing_.GetX() + relative.GetZ() * facing_.GetZ() -
                                               kMantleExitCostMps);
        traversal_exit_relative_velocity_ = facing_ * along;
    }
    bodies.SetGravityFactor(player_id_, 0.0F);
}

void Simulation::PhysicsWorld::begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept {
    drop_hang_arms();
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    traversal_state_ = TraversalState::Mantling;
    traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
    traversal_local_approach_ = traversal_local_start_;
    traversal_progress_ = 0.0;
    // Begun while the hands still draw the body in from where they caught
    // (a running leap's catch is made at full reach), the climb-up has
    // that much further to go: it takes that much longer, so it moves no
    // faster than one begun from the hold.
    traversal_duration_ =
        kMantleDurationSeconds *
        (1.0 + static_cast<double>(traversal_entry_offset_.Length()) / kHangHoldToLandingMeters);
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
    // A vault keeps the run's speed, flow's included (kFlowSpeedMps).
    const float speed_cap = kFlowSpeedMps;
    if (relative_speed > speed_cap) {
        relative = relative * (speed_cap / relative_speed);
    }

    const double travel_x = landing_centre.GetX() - origin.GetX();
    const double travel_z = landing_centre.GetZ() - origin.GetZ();
    const double travel_squared = travel_x * travel_x + travel_z * travel_z;
    // A speed vault crosses at the run's own pace (see
    // kSpeedVaultMinimumSpeed); a slower one takes a vault's time.
    const double along_speed =
        travel_squared > 1.0e-6
            ? (relative.GetX() * travel_x + relative.GetZ() * travel_z) / std::sqrt(travel_squared)
            : 0.0;
    double vault_seconds = kVaultDurationSeconds;
    traversal_vault_descend_at_ = 0.5F;
    traversal_speed_vault_ = along_speed >= kSpeedVaultMinimumSpeed;
    if (traversal_speed_vault_) {
        vault_seconds = std::max(kSpeedVaultMinimumSeconds, std::sqrt(travel_squared) / along_speed);
        // Crossing at a run's pace the body is still over the top at the
        // half: it holds its apex until it is past the far edge (the top's
        // depth felt by down-rays every 0.1 m past the face).
        const JPH::Vec3 across_dir(static_cast<float>(travel_x), 0.0F, static_cast<float>(travel_z));
        const JPH::Vec3 dir = across_dir.Normalized();
        float depth = 0.0F;
        for (int step = 1; step <= 14; ++step) {
            const float along = 0.1F * static_cast<float>(step);
            JPH::RayCastResult hit;
            if (!cast_ray(probe.wall_point + dir * along + JPH::Vec3(0.0F, 0.3F, 0.0F),
                          JPH::Vec3(0.0F, -0.4F, 0.0F), hit)) {
                break;
            }
            depth = along;
        }
        const float far_edge = static_cast<float>(JPH::Vec3(probe.wall_point - origin).Dot(dir)) + depth +
                               0.1F + kPlayerRadius;
        traversal_vault_descend_at_ =
            std::clamp(far_edge / static_cast<float>(std::sqrt(travel_squared)), 0.5F, 1.0F);
    }
    traversal_vault_horizontal_slope_ = travel_squared > 1.0e-6
        ? std::clamp(static_cast<float>(
              vault_seconds * (relative.GetX() * travel_x +
                               relative.GetZ() * travel_z) / travel_squared),
              0.0F, 3.0F)
        : 0.0F;

    const JPH::RVec3 apex(origin.GetX(),
                          probe.ledge_point.GetY() + kPlayerHalfHeight + kVaultApexClearance,
                          origin.GetZ());

    // Started close to the obstacle, the eased path would carry the body
    // into its face before it had risen clear of the top: it pushed on the
    // face, then caught its path up in a lurch (0.24 m in a tick, off a
    // parapet 0.1 m away). Then the vault first rises clear while it
    // closes on the face, and crosses after, at a standing vault's pace.
    traversal_vault_wall_fraction_ = 1.0F;
    traversal_vault_clear_progress_ = 0.0F;
    const double travel_length = std::sqrt(travel_squared);
    const float rise = static_cast<float>(apex.GetY() - origin.GetY());
    const float clear = static_cast<float>(probe.ledge_point.GetY() + kPlayerHalfHeight - origin.GetY());
    if (travel_length > 1.0e-3 && rise > 1.0e-3F && clear > 0.0F) {
        const double face_along = ((probe.wall_point.GetX() - origin.GetX()) * travel_x +
                                   (probe.wall_point.GetZ() - origin.GetZ()) * travel_z) /
                                  travel_length;
        const float face_fraction = std::clamp(
            static_cast<float>((face_along - kPlayerRadius - kVaultFaceGap) / travel_length), 0.0F, 1.0F);
        // The rise is smoothstep(2 p) of the way to the apex: invert it at
        // the height where the body's soles clear the top.
        const float wanted = std::min(clear / rise, 1.0F);
        float low = 0.0F;
        float high = 1.0F;
        for (int i = 0; i < 24; ++i) {
            const float mid = 0.5F * (low + high);
            if (smoothstep(0.0F, 1.0F, mid) < wanted) {
                low = mid;
            } else {
                high = mid;
            }
        }
        const float clear_progress = 0.5F * high;
        if (ease_with_carried_speed(clear_progress, traversal_vault_horizontal_slope_) > face_fraction) {
            traversal_vault_wall_fraction_ = face_fraction;
            traversal_vault_clear_progress_ = clear_progress;
        }
    }

    traversal_state_ = TraversalState::Vaulting;
    traversal_body_ = probe.ledge_body;
    traversal_entity_id_ = probe.ledge_entity_id;
    traversal_target_body_ = landing_body;
    traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
    traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
    traversal_local_apex_ = to_support_local(bodies, traversal_body_, apex);
    traversal_local_target_ = to_support_local(bodies, traversal_target_body_, landing_centre);
    traversal_progress_ = 0.0;
    // Rising clear first adds its time: the crossing keeps a vault's.
    traversal_duration_ = vault_seconds / (1.0 - traversal_vault_clear_progress_);
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
        float horizontal;
        const float clear_at = traversal_vault_clear_progress_;
        if (clear_at > 0.0F) {
            // Up to the face while rising clear, from the speed carried in
            // to a stop at the face; then over and down, leaving with the
            // carried speed as any vault does.
            const float face = traversal_vault_wall_fraction_;
            const float slope = traversal_vault_horizontal_slope_;
            if (progress < clear_at) {
                const float t = progress / clear_at;
                const float in = face > 1.0e-4F
                                     ? std::clamp(slope * clear_at / (face * (1.0F - clear_at)), 0.0F, 3.0F)
                                     : 0.0F;
                horizontal = face * (in * t * (1.0F - t) * (1.0F - t) + t * t * (3.0F - 2.0F * t));
            } else {
                const float out = face < 1.0F ? slope / (1.0F - face) : 0.0F;
                const float t = (progress - clear_at) / (1.0F - clear_at);
                const float u = std::clamp(t, 0.0F, 1.0F);
                horizontal = face + (1.0F - face) * (u * u * (3.0F - 2.0F * u) +
                                                     std::clamp(out, 0.0F, 3.0F) * u * u * (u - 1.0F));
            }
        } else {
            horizontal = ease_with_carried_speed(progress, traversal_vault_horizontal_slope_);
        }
        float height;
        if (clear_at > 0.0F) {
            // Up to the apex while closing on the face; held over the
            // first half of the crossing, down over the second.
            const float q = (progress - clear_at) / (1.0F - clear_at);
            if (progress < clear_at) {
                height = start.GetY() +
                         (apex.GetY() - start.GetY()) * smoothstep(0.0F, 1.0F, progress / clear_at);
            } else if (q < 0.5F) {
                height = apex.GetY();
            } else {
                height = apex.GetY() + (target.GetY() - apex.GetY()) * smoothstep(0.0F, 1.0F, (q - 0.5F) * 2.0F);
            }
        } else if (progress < 0.5F) {
            height = start.GetY() +
                     (apex.GetY() - start.GetY()) * smoothstep(0.0F, 1.0F, progress * 2.0F);
        } else if (progress < traversal_vault_descend_at_) {
            height = apex.GetY();
        } else {
            const float down = traversal_vault_descend_at_;
            height = apex.GetY() + (target.GetY() - apex.GetY()) *
                                       smoothstep(0.0F, 1.0F, (progress - down) / (1.0F - down));
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

    if (traversal_state_ == TraversalState::Hanging) {
        // Held by the arms: nothing placed. The body steadies its swing.
        draw_in_hang_arms(delta_seconds);
        traversal_desired_ = current;
        steady_hang(bodies);
        return;
    }
    if (traversal_state_ == TraversalState::Swinging) {
        draw_in_hang_arms(delta_seconds);   // nothing placed, nothing steadied
        traversal_desired_ = current;
        return;
    }
    if (traversal_state_ == TraversalState::Climbing) {
        const float pull = kHangPullInSpeed * delta_seconds;
        const float gap = traversal_entry_offset_.Length();
        traversal_entry_offset_ =
            gap > pull ? traversal_entry_offset_ * ((gap - pull) / gap) : JPH::Vec3::sZero();
        traversal_desired_ = from_support_local(bodies, traversal_body_, traversal_local_hold_) +
                             traversal_entry_offset_;
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
    if (traversal_state_ == TraversalState::Hanging || traversal_state_ == TraversalState::Swinging) {
        return;   // held by the arms, nothing placed to fall behind
    }
    if (traversal_state_ == TraversalState::Vaulting && traversal_speed_vault_ &&
        traversal_progress_ >= traversal_vault_descend_at_) {
        // Past the far edge at the apex: the run's speed carries on and
        // gravity brings the body down where it will.
        flight_from_jump_ = true;
        complete_traversal(bodies);
        return;
    }
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
    if (traversal_state_ == TraversalState::Vaulting || traversal_state_ == TraversalState::Mantling) {
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

// Where the shoulders hang from, in the body's frame: at the wall's face
// (kPlayerRadius + kHangWallGap toward it) and kHangShoulderAboveCentre up.
JPH::Vec3 Simulation::PhysicsWorld::hang_shoulder_offset() const noexcept {
    if (traversal_state_ == TraversalState::Swinging) {
        return JPH::Vec3(0.0F, kHangShoulderAboveCentre, 0.0F);   // straight under the bar
    }
    return traversal_normal_ * (kPlayerRadius + kHangWallGap) +
           JPH::Vec3(0.0F, kHangShoulderAboveCentre, 0.0F);
}

// Closes the hands on the lip at traversal_local_ledge_: the arms taut
// at their reach now, never shorter than kHangArmStop.
void Simulation::PhysicsWorld::attach_hang_arms(JPH::BodyInterface &bodies) noexcept {
    drop_hang_arms();
    if (traversal_body_.IsInvalid()) {
        return;
    }
    const JPH::RVec3 hands = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::RVec3 shoulders = bodies.GetPosition(player_id_) + hang_shoulder_offset();
    const float reach = std::max(kHangArmStop, JPH::Vec3(shoulders - hands).Length());
    JPH::DistanceConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = hands;
    settings.mPoint2 = shoulders;
    settings.mMinDistance = 0.0F;
    settings.mMaxDistance = reach;
    settings.mLimitsSpringSettings.mMode = JPH::ESpringMode::FrequencyAndDamping;
    settings.mLimitsSpringSettings.mFrequency = Trapeze::kArmSpringHz;
    settings.mLimitsSpringSettings.mDamping = Trapeze::kArmDamping;
    hang_arms_ = static_cast<JPH::DistanceConstraint *>(
        create_constraint(settings, traversal_body_, player_id_, false));
    hang_over_grip_ticks_ = 0;
    hang_over_grip_impulse_ = 0.0F;
    bodies.SetGravityFactor(player_id_, 1.0F);
}

void Simulation::PhysicsWorld::drop_hang_arms() noexcept {
    if (hang_arms_ != nullptr) {
        physics_system_.RemoveConstraint(hang_arms_);
        hang_arms_ = nullptr;
    }
    hang_over_grip_ticks_ = 0;
    hang_over_grip_impulse_ = 0.0F;
}

// The shoulders and core stilling the swing (see kHangSteadyForceN):
// a finite force against the body's motion across the arms relative to
// the hands; the hold takes the reaction.
void Simulation::PhysicsWorld::steady_hang(JPH::BodyInterface &bodies) noexcept {
    if (hang_arms_ == nullptr || traversal_body_.IsInvalid()) {
        return;
    }
    const JPH::RVec3 hands = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::RVec3 shoulders = bodies.GetPosition(player_id_) + hang_shoulder_offset();
    JPH::Vec3 arm(hands - shoulders);
    const float arm_length = arm.Length();
    if (arm_length < 1.0e-3F) {
        return;
    }
    arm /= arm_length;
    JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) - bodies.GetPointVelocity(traversal_body_, hands);
    relative -= arm * relative.Dot(arm);
    JPH::Vec3 force = relative * (-kPlayerMassKg * kHangSteadyRate);
    const float magnitude = force.Length();
    if (magnitude > kHangSteadyForceN) {
        force *= kHangSteadyForceN / magnitude;
    }
    bodies.AddForce(player_id_, force);
    if (bodies.GetMotionType(traversal_body_) == JPH::EMotionType::Dynamic) {
        bodies.AddForce(traversal_body_, -force, hands);
    }
}

// The arms drawn in toward kHangArmStop (write phase).
void Simulation::PhysicsWorld::draw_in_hang_arms(const float delta_seconds) noexcept {
    if (hang_arms_ == nullptr) {
        return;
    }
    const float reach = hang_arms_->GetMaxDistance();
    if (reach > kHangArmStop) {
        hang_arms_->SetDistance(0.0F, std::max(kHangArmStop, reach - kHangPullInSpeed * delta_seconds));
    }
}

// After the step: the hands' load, and the slip past the grip.
void Simulation::PhysicsWorld::check_hang_grip(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
    if (hang_arms_ == nullptr ||
        (traversal_state_ != TraversalState::Hanging && traversal_state_ != TraversalState::Swinging)) {
        return;
    }
    const float load = std::abs(hang_arms_->GetTotalLambdaPosition()) / delta_seconds;
    if (load > kLedgeGripMaxForceN) {
        ++hang_over_grip_ticks_;
        hang_over_grip_impulse_ += (load - kLedgeGripMaxForceN) * delta_seconds;
    } else {
        hang_over_grip_ticks_ = 0;
        hang_over_grip_impulse_ = 0.0F;
    }
    if (hang_over_grip_ticks_ >= Trapeze::kSlipTicks || hang_over_grip_impulse_ > kLedgeGripTearImpulseNs) {
        ++hang_slip_count_;
        release_hang(bodies);
    }
}

// Letting go: the hands open and nothing else; the body keeps the
// velocity it had on the arms.
void Simulation::PhysicsWorld::release_hang(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::Vec3 support_velocity =
        traversal_body_.IsInvalid() ? JPH::Vec3::sZero()
                                    : bodies.GetPointVelocity(traversal_body_, ledge);
    airborne_inherited_velocity_ = support_velocity;
    bodies.SetGravityFactor(player_id_, 1.0F);
    regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
    clear_traversal();
}

} // namespace scraperx::sim
