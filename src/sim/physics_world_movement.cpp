// Ground movement: the wall-run, slide, jump vault, landing and roll, wind, locomotion, step-up, parachute.
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim {

// In flight, a wall beside the body: a step on it when one is due (see
// kWallStepPushMps).
void Simulation::PhysicsWorld::update_wall_run(JPH::BodyInterface &bodies, const StepCommands &commands, JPH::Vec3 &velocity,
                                               const JPH::Vec3 reference) noexcept {
    if (wall_run_seconds_left_ > 0.0) {
        wall_run_seconds_left_ -= kFixedStepSeconds;
    }
    if (wall_step_cooldown_ > 0) {
        --wall_step_cooldown_;
        return;
    }
    if (!flight_from_jump_ || crouched_ || carry_constraint_ != nullptr) {
        return;
    }
    if (try_wall_climb(bodies, commands, velocity, reference)) {
        return;
    }
    if (wall_steps_this_flight_ >= kWallStepsMax) {
        return;
    }
    const JPH::Vec3 relative = velocity - reference;
    const JPH::Vec3 horizontal(relative.GetX(), 0.0F, relative.GetZ());
    const float speed = horizontal.Length();
    // Rising, a foot on the wall has nothing to take: a step is planted
    // once the body comes down fast enough to load it (half what one step
    // can stop).
    if (speed < kWallRunMinimumSpeed || relative.GetY() > -0.5F * kWallFriction * kWallStepPushMps) {
        return;
    }
    const JPH::Vec3 run = horizontal / speed;
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    for (const float side : {-1.0F, 1.0F}) {
        const JPH::Vec3 out(-run.GetZ() * side, 0.0F, run.GetX() * side);
        JPH::RayCastResult hit;
        if (!cast_ray(centre, out * (kPlayerRadius + kWallRunProbe), hit)) {
            continue;
        }
        const JPH::RVec3 point = JPH::RRayCast(centre, out * (kPlayerRadius + kWallRunProbe)).GetPointOnRay(hit.mFraction);
        const JPH::Vec3 normal = surface_normal(hit.mBodyID, hit.mSubShapeID2, point);
        if (std::abs(normal.GetY()) > 0.2F) {
            continue;   // not a wall
        }
        const JPH::Vec3 wall_out = JPH::Vec3(normal.GetX(), 0.0F, normal.GetZ()).Normalized();
        // Running along it, not into it or away from it.
        if (std::abs(run.Dot(wall_out)) > std::sin(kWallRunMaximumAngle)) {
            continue;
        }
        const float stick_out = static_cast<float>(commands.move_input_x * wall_out.GetX() +
                                                   commands.move_input_z * wall_out.GetZ());
        if (stick_out > 0.1F) {
            continue;   // the stick turned away from the wall
        }
        // The step: pushed off, and the sole's slide down resisted.
        velocity += wall_out * kWallStepPushMps;
        const float falling = reference.GetY() - velocity.GetY();
        if (falling > 0.0F) {
            velocity.SetY(velocity.GetY() + std::min(falling, kWallFriction * kWallStepPushMps));
        }
        wall_normal_ = wall_out;
        wall_step_cooldown_ = kWallStepTicks;
        wall_run_seconds_left_ = kWallRunHoldSeconds;
        ++wall_steps_this_flight_;
        ++wall_step_count_;
        return;
    }
}

// Jumped at a tall wall: a foot planted on it turns the run into height (see
// kWallClimbMinimumSpeed). True when it plants.
bool Simulation::PhysicsWorld::try_wall_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                                              JPH::Vec3 &velocity, const JPH::Vec3 reference) noexcept {
    if (wall_climbed_this_flight_) {
        return false;
    }
    const JPH::Vec3 relative = velocity - reference;
    const JPH::Vec3 horizontal(relative.GetX(), 0.0F, relative.GetZ());
    const float speed = horizontal.Length();
    // Still rising or near the top of the jump: a foot planted on the way
    // down takes the fall, and that is the wall-run's step.
    if (speed < kWallClimbMinimumSpeed || relative.GetY() < -1.0F) {
        return false;
    }
    const JPH::Vec3 run = horizontal / speed;
    const JPH::Vec3 stick(static_cast<float>(commands.move_input_x), 0.0F, static_cast<float>(commands.move_input_z));
    if (stick.Dot(run) < 0.3F) {
        return false;   // the stick not held at the wall
    }
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const float feet = static_cast<float>(centre.GetY()) - active_half_height();
    const JPH::Vec3 reach = run * (kPlayerRadius + kWallClimbProbe);
    // A wall whose top the jump's own arc brings into the hands is caught,
    // not run up: the wall must stand above the highest the hands reach.
    const float gravity = -physics_system_.GetGravity().GetY();
    const float rising = std::max(relative.GetY(), 0.0F);
    const float hands_reach = static_cast<float>(centre.GetY()) + rising * rising / (2.0F * gravity) +
                              kHangMaximumRiseAboveCentre;
    JPH::Vec3 wall_out = JPH::Vec3::sZero();
    for (const float at : {feet + kWallClimbChestAboveFeet, hands_reach}) {
        const JPH::RVec3 from(centre.GetX(), at, centre.GetZ());
        JPH::RayCastResult hit;
        if (!cast_ray(from, reach, hit)) {
            return false;   // not a wall, or not a tall one
        }
        const JPH::Vec3 normal = surface_normal(hit.mBodyID, hit.mSubShapeID2, JPH::RRayCast(from, reach).GetPointOnRay(hit.mFraction));
        if (std::abs(normal.GetY()) > 0.2F) {
            return false;
        }
        wall_out = JPH::Vec3(normal.GetX(), 0.0F, normal.GetZ()).Normalized();
    }
    const float into = -horizontal.Dot(wall_out);
    if (into < kWallClimbMinimumSpeed * std::cos(kWallClimbMaximumAngle) ||
        into < speed * std::cos(kWallClimbMaximumAngle)) {
        return false;
    }
    // The plant: the speed into the wall taken by the leg, and as much of it
    // turned upward as the sole's friction can give.
    velocity += wall_out * into;
    const float rise = velocity.GetY() - reference.GetY();
    const float climb = std::max(rise, std::min(rise + kWallFriction * into, kWallClimbMaxRiseMps));
    velocity.SetY(reference.GetY() + climb);
    wall_normal_ = wall_out;
    // On the wall all the way up (a jump kicks off it, the wall turn), and
    // a moment past the top.
    wall_run_seconds_left_ =
        std::max(kWallRunHoldSeconds, static_cast<double>(climb / -physics_system_.GetGravity().GetY()) + 0.15);
    wall_step_cooldown_ = kWallStepTicks;
    wall_climbed_this_flight_ = true;
    ++wall_climb_count_;
    return true;
}

// At the moment of crouching: fast enough, and running the way the body
// faces, it slides (see kSlideMinimumSpeed).
void Simulation::PhysicsWorld::begin_slide_if_running(JPH::BodyInterface &bodies) noexcept {
    if (rolling() || carry_constraint_ != nullptr) {
        return;
    }
    const JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) - airborne_inherited_velocity_;
    const JPH::Vec3 horizontal(relative.GetX(), 0.0F, relative.GetZ());
    const float speed = horizontal.Length();
    if (speed < kSlideMinimumSpeed || facing_.IsNearZero() ||
        horizontal.Dot(facing_) < std::cos(0.25F * static_cast<float>(kPi)) * speed) {
        return;
    }
    sliding_ = true;
    slide_direction_ = horizontal / speed;
    ++slide_count_;
}

// Sliding: along the slide only friction acts; across it the stick steers.
void Simulation::PhysicsWorld::slide_locomotion(JPH::Vec3 &velocity, const JPH::Vec3 reference, const StepCommands &commands,
                                                const float dt) noexcept {
    const float gravity = -physics_system_.GetGravity().GetY();
    const JPH::Vec3 relative = velocity - reference;
    const JPH::Vec3 across(-slide_direction_.GetZ(), 0.0F, slide_direction_.GetX());
    const float along_speed = std::max(0.0F, relative.Dot(slide_direction_) - kSlideFriction * gravity * dt);
    const float wanted_across =
        float(commands.move_input_x * across.GetX() + commands.move_input_z * across.GetZ()) * kSlideMinimumSpeed * 0.5F;
    const float across_speed = relative.Dot(across);
    const float across_change = std::clamp(wanted_across - across_speed, -kSlideSteerAcceleration * dt,
                                           kSlideSteerAcceleration * dt);
    const JPH::Vec3 horizontal = slide_direction_ * along_speed + across * (across_speed + across_change);
    velocity.SetX(reference.GetX() + horizontal.GetX());
    velocity.SetZ(reference.GetZ() + horizontal.GetZ());
    if (horizontal.Length() > 1.0e-3F) {
        slide_direction_ = horizontal.Normalized();
    }
    if (along_speed <= kPlayerMaximumRelativeSpeed * static_cast<float>(kCrouchSpeedScale)) {
        sliding_ = false;
    }
}

bool Simulation::PhysicsWorld::try_stand(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 standing =
        bodies.GetPosition(player_id_) + JPH::Vec3(0.0F, kCrouchDrop, 0.0F);
    if (!shape_pose_is_clear(player_shape_.GetPtr(),
                             standing + JPH::Vec3(0.0F, kStandClearanceSkin, 0.0F))) {
        return false;
    }
    bodies.SetShape(player_id_, player_shape_.GetPtr(), false, JPH::EActivation::Activate);
    bodies.SetPosition(player_id_, standing, JPH::EActivation::Activate);
    crouched_ = false;
    return true;
}

// The legs let down out of the tuck, the head where it is, where there is
// room under the body.
bool Simulation::PhysicsWorld::try_untuck(JPH::BodyInterface &bodies) noexcept {
    const JPH::RVec3 standing = bodies.GetPosition(player_id_) - JPH::Vec3(0.0F, kCrouchDrop, 0.0F);
    if (!shape_pose_is_clear(player_shape_.GetPtr(), standing - JPH::Vec3(0.0F, kStandClearanceSkin, 0.0F))) {
        return false;
    }
    bodies.SetShape(player_id_, player_shape_.GetPtr(), false, JPH::EActivation::Activate);
    bodies.SetPosition(player_id_, standing, JPH::EActivation::Activate);
    crouched_ = false;
    tucked_ = false;
    return true;
}

// request_up, made. up_move_ was chosen at the end of the last tick, from
// the state this tick starts from (update_up_move), so the move made is the
// move offered; each goes by the path its own control takes, so it is
// tested, refused and counted as that control's.
void Simulation::PhysicsWorld::resolve_up(StepCommands &commands) noexcept {
    if (!commands.up_requested) {
        return;
    }
    commands.up_requested = false;
    switch (up_move_) {
    case UpMove::None:
        return;
    case UpMove::Jump:
    case UpMove::Precision:
    case UpMove::Springboard:
    case UpMove::ClimbUp:
    case UpMove::JumpOff:
    case UpMove::Launch:
    case UpMove::Kick:
        commands.jump_requested = true;
        break;
    case UpMove::Vault:
    case UpMove::Mantle:
    case UpMove::Climb:
    case UpMove::TopOut:
        commands.traversal_requested = true;
        break;
    case UpMove::Stand:
        // Up from a crouch is up on to the feet: the crouch let go this
        // tick, and no jump (update_crouch stands the body where it can).
        commands.crouch_held = false;
        break;
    case UpMove::Catch:
        commands.trapeze_action = true;
        break;
    case UpMove::LetGo:
        commands.trapeze_drop = true;
        break;
    case UpMove::Throw:
        commands.throw_requested = true;
        break;
    }
    up_resolved_ = up_move_;
    ++up_count_;
}

void Simulation::PhysicsWorld::apply_traversal_commands(JPH::BodyInterface &bodies,
                                                        const StepCommands &commands) noexcept {
    if (traversal_state_ == TraversalState::Hanging) {
        if (commands.release_requested) {
            release_hang(bodies);
        } else if (commands.jump_requested || commands.traversal_requested) {
            begin_mantle_from_hang(bodies);
        }
        return;
    }

    if (traversal_state_ == TraversalState::Swinging) {
        // Letting go: the hands open; the body flies on with the swing's
        // velocity. Jumping off: thrown on as they open (kSwingLaunchMps).
        if (commands.jump_requested) {
            launch_from_swing(bodies);
        } else if (commands.release_requested) {
            release_hang(bodies);
        }
        return;
    }

    if (traversal_state_ == TraversalState::Climbing) {
        if (commands.release_requested) {
            let_go_climb(bodies);
        } else if (commands.jump_requested) {
            jump_off_climb(bodies, commands);
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
        const Grip grip = grounded_ ? grip_in_front(bodies) : Grip{};
        if (grip.valid) {
            begin_climb(bodies, grip);
        } else {
            ++rejected_traversal_count_;
        }
    }
}

// The vault the takeoff could have been: probed from the takeoff floor at
// the body's current plan position, with the same rise band and the same
// far-side landing test as a vault started on the ground.
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

// See kSpringboardMinimumSpeed. Felt for along the run: a face within a
// stride, its top a vault's rise above the soles, and room to stand there
// (the mantle's own landing). The plant is a vault's path onto that spot;
// complete_traversal jumps from it.
auto Simulation::PhysicsWorld::springboard_ahead(
    const JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity) const noexcept -> LedgeProbe {
    const JPH::Vec3 run(relative_velocity.GetX(), 0.0F, relative_velocity.GetZ());
    const float speed = run.Length();
    if (speed < kSpringboardMinimumSpeed || carry_constraint_ != nullptr || balancing_) {
        return {};
    }
    const JPH::Vec3 along = run / speed;
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const LedgeProbe probe = probe_ledge(at, along, kVaultMinimumRise, kVaultMaximumRise, true, false,
                                         speed * kSpringboardStrideSeconds);
    if (!probe.valid) {
        return {};
    }
    for (float depth = 0.1F; depth <= kSpringboardTopDepth + 1.0e-3F; depth += 0.1F) {
        const JPH::RVec3 over(probe.wall_point.GetX() + along.GetX() * depth, probe.ledge_point.GetY() + 0.3,
                              probe.wall_point.GetZ() + along.GetZ() * depth);
        JPH::RayCastResult hit;
        if (!cast_ray(over, JPH::Vec3(0.0F, -0.4F, 0.0F), hit) || std::abs(0.3F - 0.4F * hit.mFraction) > 0.06F) {
            return {};
        }
    }
    return probe;
}

bool Simulation::PhysicsWorld::try_begin_springboard(JPH::BodyInterface &bodies,
                                                     const JPH::Vec3 relative_velocity) noexcept {
    const LedgeProbe probe = springboard_ahead(bodies, relative_velocity);
    if (!probe.valid) {
        return false;
    }
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    begin_vault(bodies, probe, probe.landing_centre, probe.landing_body, at);
    traversal_springboard_ = true;
    return true;
}

// Something the soles could stand on under this point: within 0.3 m
// either side of their height.
bool Simulation::PhysicsWorld::feet_supported_at(const JPH::RVec3 at, const float soles) const noexcept {
    JPH::RayCastResult hit;
    return cast_ray(JPH::RVec3(at.GetX(), soles + 0.3F, at.GetZ()), JPH::Vec3(0.0F, -0.6F, 0.0F), hit);
}

// The ground runs on from the feet to `centre + offset`: walked in steps
// of at most 0.4 m, each finding a surface no more than 0.3 m above the
// last and no further below it than 0.3 m plus the step (a 45 degree
// descent). A slope, a stair run or a curb is ground; an edge is not.
bool Simulation::PhysicsWorld::ground_runs_to(const JPH::RVec3 centre, const JPH::Vec3 offset, const float soles) const noexcept {
    const float length = JPH::Vec3(offset.GetX(), 0.0F, offset.GetZ()).Length();
    const int steps = std::max(1, static_cast<int>(std::ceil(length / 0.4F)));
    const float step = length / static_cast<float>(steps);
    float floor = soles;
    for (int k = 1; k <= steps; ++k) {
        const JPH::RVec3 at = centre + offset * (static_cast<float>(k) / static_cast<float>(steps));
        const float top = floor + 0.3F;
        const float reach = 0.6F + step;
        JPH::RayCastResult hit;
        if (!cast_ray(JPH::RVec3(at.GetX(), top, at.GetZ()), JPH::Vec3(0.0F, -reach, 0.0F), hit)) {
            return false;
        }
        floor = top - hit.mFraction * reach;
    }
    return true;
}

// 1 when recovered; 1 - loss just after the landing.
double Simulation::PhysicsWorld::recovery_balance() const noexcept {
    if (recovery_duration_ <= 0.0) {
        return 1.0;
    }
    return 1.0 - landing_loss_ * std::clamp(recovery_seconds_left_ / recovery_duration_, 0.0, 1.0);
}

void Simulation::PhysicsWorld::clear_recovery() noexcept {
    landing_response_ = LandingResponse::None;
    landing_loss_ = 0.0;
    recovery_seconds_left_ = recovery_duration_ = 0.0;
    roll_seconds_left_ = 0.0;
}

// A clear, supported lane for the crouched capsule from the landing
// centre along `direction` for `length`: swept, and stood on every 0.5 m.
bool Simulation::PhysicsWorld::roll_lane_is_clear(const JPH::RVec3 standing_centre, const JPH::Vec3 direction,
                                                  const float length) const noexcept {
    const JPH::RVec3 crouched = standing_centre - JPH::Vec3(0.0F, kCrouchDrop - kStandClearanceSkin, 0.0F);
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const JPH::RShapeCast sweep(player_crouch_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
                                JPH::RMat44::sTranslation(crouched), direction * length);
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), crouched, collector,
                                                    {}, {}, body_filter);
    if (collector.HadHit()) {
        return false;
    }
    for (float ahead = 0.5F; ahead <= length + 0.01F; ahead += 0.5F) {
        JPH::RayCastResult hit;
        const JPH::RVec3 at = standing_centre + direction * ahead;
        if (!cast_ray(at, JPH::Vec3(0.0F, -(kPlayerHalfHeight + 0.3F), 0.0F), hit)) {
            return false;
        }
    }
    return true;
}

void Simulation::PhysicsWorld::record_landing(JPH::BodyInterface &bodies, const bool prepared) noexcept {
    const JPH::Vec3 support_velocity(static_cast<float>(support_sample_.point_velocity.x),
                                     static_cast<float>(support_sample_.point_velocity.y),
                                     static_cast<float>(support_sample_.point_velocity.z));
    const JPH::Vec3 relative = pre_contact_velocity_ - support_velocity;
    const float normal_speed = std::max(0.0F, -relative.GetY());
    const JPH::Vec3 tangent(relative.GetX(), 0.0F, relative.GetZ());
    const float tangent_speed = tangent.Length();
    const JPH::Vec3 along = tangent_speed > 1.0e-4F ? tangent / tangent_speed : JPH::Vec3::sZero();
    const bool forward = tangent_speed > 1.0e-4F && along.Dot(facing_) >= 0.7F;
    ++landing_count_;
    landing_normal_speed_mps_ = normal_speed;
    landing_tangent_speed_mps_ = tangent_speed;
    // Debt still owed from an earlier landing is not forgiven by this
    // one: the larger of the two is carried, on its own clock.
    const double owed = 1.0 - recovery_balance();
    const double owed_seconds = recovery_seconds_left_;
    clear_recovery();

    const float free_speed = prepared ? kLandingPreparedFreeSpeed : kLandingFreeSpeed;
    const double mass = kPlayerMassKg;
    const double normal_excess =
        0.5 * mass * std::max(0.0, double(normal_speed) * normal_speed - double(free_speed) * free_speed);
    const double lethal_span = 0.5 * mass *
        (double(kLethalImpactSpeedMps) * kLethalImpactSpeedMps - double(kLandingFreeSpeed) * kLandingFreeSpeed);
    const bool stumble = !forward && tangent_speed >= kStumbleMinimumSpeed;
    const double tangent_energy = stumble ? 0.25 * 0.5 * mass * double(tangent_speed) * tangent_speed : 0.0;
    double loss = std::clamp(kLandingMaxLoss * (normal_excess + tangent_energy) / lethal_span, 0.0,
                             kLandingMaxLoss);
    if (prepared) {
        loss *= kLandingPreparedLossScale;
    }
    const double full_loss = loss;
    landing_response_ = loss <= 0.0 ? LandingResponse::Absorbed
                        : stumble   ? LandingResponse::Stumble
                                    : LandingResponse::Heavy;
    if (prepared && forward && tangent_speed >= kRollMinimumSpeed && normal_speed >= kRollMinimumNormalSpeed) {
        const float runout = std::max(0.5F, tangent_speed * float(kRollSeconds) -
                                                0.5F * kRollDeceleration * float(kRollSeconds * kRollSeconds));
        // The lane is measured from where the body would stand on the
        // ground it struck: a hard landing sinks it into the ground for a
        // tick, a tucked one is the crouched capsule.
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        const JPH::RVec3 standing(at.GetX(), support_sample_.contact_point.y + kPlayerHalfHeight, at.GetZ());
        if (roll_lane_is_clear(standing, along, runout)) {
            landing_response_ = LandingResponse::Roll;
            loss *= kRollLossScale;
            roll_seconds_left_ = kRollSeconds;
            roll_direction_ = along;
            roll_entry_speed_ = tangent_speed;
            roll_blocked_loss_ = std::max(full_loss / kLandingPreparedLossScale, owed);
            roll_start_ = bodies.GetPosition(player_id_);
            roll_travel_m_ = 0.0;
            ++roll_count_;
            // The feet keep the speed the body arrived with along the
            // lane; the solver's contact may already have shaved it.
            const JPH::Vec3 now = bodies.GetLinearVelocity(player_id_) - support_velocity;
            const float carried = now.Dot(along);
            if (carried < tangent_speed) {
                bodies.SetLinearVelocity(player_id_, bodies.GetLinearVelocity(player_id_) +
                                                         along * (tangent_speed - carried));
            }
        } else {
            ++roll_refused_count_;
        }
    }
    // The bones: a landing past what they take breaks an ankle.
    break_ankle_if(normal_speed, landing_response_ == LandingResponse::Roll ? kAnkleBreakRolledSpeed
                                 : prepared                                  ? kAnkleBreakPreparedSpeed
                                                                             : kAnkleBreakSpeed);
    if (landing_response_ == LandingResponse::Stumble) {
        ++stumble_count_;
    }
    landing_loss_ = loss;
    if (loss > 0.0) {
        recovery_duration_ = std::clamp(0.15 + 0.06 * std::max(0.0, double(normal_speed) - free_speed) +
                                            (stumble ? 0.05 * tangent_speed : 0.0),
                                        0.15, 2.0);
        recovery_seconds_left_ = recovery_duration_;
    }
    if (owed > loss && owed_seconds > 0.0) {
        landing_loss_ = owed;
        recovery_duration_ = recovery_seconds_left_ = owed_seconds;
    }
}

// Along the lane the roll only brakes, at rolling friction; across it the
// stick steers with a small force. A blocked lane (the speed along it
// collapsing) ends the roll as a heavy landing at the full debt.
void Simulation::PhysicsWorld::roll_locomotion(JPH::Vec3 &velocity, const JPH::Vec3 reference, const StepCommands &commands,
                                               const float dt) noexcept {
    const JPH::Vec3 relative = velocity - reference;
    const JPH::Vec3 across = JPH::Vec3(-roll_direction_.GetZ(), 0.0F, roll_direction_.GetX());
    const float along_speed = std::max(0.0F, relative.Dot(roll_direction_) - kRollDeceleration * dt);
    const float elapsed = float(kRollSeconds - roll_seconds_left_);
    const float expected = std::max(0.0F, roll_entry_speed_ - kRollDeceleration * elapsed);
    if (elapsed > 0.1F && along_speed < 0.5F * expected) {
        ++roll_blocked_count_;
        landing_response_ = LandingResponse::Heavy;
        landing_loss_ = roll_blocked_loss_;
        // Planted after all: the bones take it as a crouched landing (a
        // break the roll's speed already made is not made twice).
        if (landing_normal_speed_mps_ < kAnkleBreakRolledSpeed) {
            break_ankle_if(static_cast<float>(landing_normal_speed_mps_), kAnkleBreakPreparedSpeed);
        }
        recovery_seconds_left_ = recovery_duration_;
        roll_seconds_left_ = 0.0;
    }
    const float wanted_across = float(commands.move_input_x * across.GetX() + commands.move_input_z * across.GetZ()) *
                                kRollMinimumSpeed;
    const float across_speed = relative.Dot(across);
    const float across_change = std::clamp(wanted_across - across_speed, -kRollSteerAcceleration * dt,
                                           kRollSteerAcceleration * dt);
    const JPH::Vec3 horizontal = roll_direction_ * along_speed + across * (across_speed + across_change);
    velocity.SetX(reference.GetX() + horizontal.GetX());
    velocity.SetZ(reference.GetZ() + horizontal.GetZ());
}

// Phase 2: the share of the ground's traction (kGroundAcceleration, a
// project parameter on level ground) the footing gives: the vertical part
// of the face under the soles, as a slope tips the load off them. The
// contact's own normal is not used: it tilts on a step's edge. The
// bodies' friction values were set for the machines' contacts, not as
// grip, so they do not enter until surfaces are given one; nor does a
// moving support's acceleration (it changed when S1's cage starts, which
// rides on the rider's weight).
float Simulation::PhysicsWorld::ground_grip(const JPH::BodyInterface &bodies) const noexcept {
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const float reach = active_half_height() + kCheckpointFootingSlack;
    JPH::RayCastResult hit;
    if (!cast_ray(centre, JPH::Vec3(0.0F, -reach, 0.0F), hit)) {
        return 1.0F;
    }
    return std::clamp(
        surface_normal(hit.mBodyID, hit.mSubShapeID2, centre + JPH::Vec3(0.0F, -reach * hit.mFraction, 0.0F))
            .GetY(),
        0.0F, 1.0F);
}

// See kSupportJoltMargin. Called each grounded tick with the support's
// velocity under the feet.
void Simulation::PhysicsWorld::track_support_jolt(const JPH::Vec3 support_velocity, const float dt) noexcept {
    if (support_entity_id_ != 0 && support_entity_id_ == jolt_support_entity_) {
        const JPH::Vec3 change = support_velocity - jolt_support_velocity_;
        support_jolt_ += JPH::Vec3(change.GetX(), 0.0F, change.GetZ());
    }
    jolt_support_entity_ = support_entity_id_;
    jolt_support_velocity_ = support_velocity;
    const float size = support_jolt_.Length();
    const float matched = std::max(0.0F, size - kGroundAcceleration * dt);
    support_jolt_ = size > 1.0e-6F ? support_jolt_ * (matched / size) : JPH::Vec3::sZero();
    const float gravity = -physics_system_.GetGravity().GetY();
    const float shift = size / std::sqrt(gravity / kTeeterComHeight);
    support_jolt_peak_m_ = std::max(support_jolt_peak_m_, shift);
    if (shift < 0.5F * kSupportJoltMargin) {
        jolt_stepped_ = false;
    }
    if (shift <= kSupportJoltMargin || jolt_stepped_ || rolling()) {
        return;
    }
    jolt_stepped_ = true;
    ++stumble_count_;
    ++support_stumble_count_;
    const double loss = std::clamp(kLandingMaxLoss * (shift - kSupportJoltMargin) / kSupportJoltSpan, 0.0,
                                   kLandingMaxLoss);
    if (loss > 1.0 - recovery_balance()) {
        landing_response_ = LandingResponse::Stumble;
        landing_loss_ = loss;
        recovery_duration_ = recovery_seconds_left_ = std::clamp(0.3 + loss, 0.3, 1.5);
    }
}

// See kWindZoneX. The wind's speed at the body (m/s, 0 outside the zone)
// and its push on a body that is not standing.
void Simulation::PhysicsWorld::apply_wind(JPH::BodyInterface &bodies, const double t) noexcept {
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const float x = static_cast<float>(at.GetX());
    const float y = static_cast<float>(at.GetY());
    const float z = static_cast<float>(at.GetZ());
    wind_mps_ = 0.0F;
    wind_force_n_ = 0.0F;
    // The west face's zone, and the summit's: out along the runway arm, west
    // of the deck's end, the same wind takes hold (outward, -x).
    float exposure = 0.0F;
    if (x <= kWindZoneX && y >= kWindZoneLowY && y <= kWindZoneHighY && z >= kWindZoneNorthZ &&
        z <= kWindZoneSouthZ) {
        exposure = std::clamp((kWindZoneX - x) / kWindZoneRampM, 0.0F, 1.0F);
    } else if (x <= kSummitWindX && y >= kSummitWindLowY && y <= kSummitWindHighY && z >= kSummitWindNorthZ &&
               z <= kSummitWindSouthZ) {
        exposure = std::clamp((kSummitWindX - x) / kSummitWindRampM, 0.0F, 1.0F);
    }
    if (exposure <= 0.0F) {
        return;
    }
    // Two swells of incommensurate periods: never long calm, now and
    // then both at once, the full gust.
    constexpr double kTau = 2.0 * 3.141592653589793;
    const float gust = static_cast<float>(std::clamp(
        0.35 + 0.35 * std::sin(kTau * t / 7.3) + 0.3 * std::sin(kTau * t / 2.9 + 1.0), 0.0, 1.0));
    wind_mps_ = exposure * (kWindBaseMps + (kWindGustMps - kWindBaseMps) * gust);
    if (grounded_) {
        return;   // standing, the soles' traction takes it
    }
    wind_force_n_ = kWindDragHalfRhoCdA * wind_mps_ * wind_mps_;
    wind_peak_force_n_ = std::max(wind_peak_force_n_, wind_force_n_);
    bodies.AddForce(player_id_, JPH::Vec3(-wind_force_n_, 0.0F, 0.0F));
}

// Phase 4: struck by something moving (kShoveClosingMps closing at a side
// contact). What the blow gave the body since the feet last set its
// velocity, horizontally, moves the XCoM just as a support jerked under
// the feet does: it joins the same reckoning (kSupportJoltMargin), and
// past half a foot of shift it is a stumble.
void Simulation::PhysicsWorld::take_shove(const JPH::Vec3 velocity_now) noexcept {
    if (!last_set_valid_ || struck_closing_mps_ < kShoveClosingMps) {
        return;
    }
    const JPH::Vec3 given = velocity_now - last_set_velocity_;
    const JPH::Vec3 horizontal(given.GetX(), 0.0F, given.GetZ());
    if (horizontal.LengthSq() < kShoveMinimumMps * kShoveMinimumMps) {
        return;
    }
    // The jolt reckons the support's change under the feet; a blow to the
    // body is the same relative motion with the opposite sign.
    support_jolt_ -= horizontal;
    ++shove_count_;
}

void Simulation::PhysicsWorld::clear_support_jolt() noexcept {
    jolt_support_entity_ = 0;
    jolt_support_velocity_ = JPH::Vec3::sZero();
    support_jolt_ = JPH::Vec3::sZero();
    jolt_stepped_ = false;
}

void Simulation::PhysicsWorld::advance_recovery(JPH::BodyInterface &bodies, const float dt) noexcept {
    if (roll_seconds_left_ > 0.0) {
        const JPH::Vec3 moved(bodies.GetPosition(player_id_) - roll_start_);
        roll_travel_m_ = JPH::Vec3(moved.GetX(), 0.0F, moved.GetZ()).Length();
        roll_seconds_left_ = std::max(0.0, roll_seconds_left_ - dt);
        // Off the end of the lane: an ordinary fall takes over.
        if (!grounded_) {
            roll_seconds_left_ = 0.0;
        }
    }
    if (recovery_seconds_left_ > 0.0 && grounded_) {
        recovery_seconds_left_ = std::max(0.0, recovery_seconds_left_ - dt);
    }
    if (limp_seconds_left_ > 0.0) {
        limp_seconds_left_ = std::max(0.0, limp_seconds_left_ - dt);
    }
    if (arm_broken_seconds_left_ > 0.0) {
        arm_broken_seconds_left_ = std::max(0.0, arm_broken_seconds_left_ - dt);
    }
}

void Simulation::PhysicsWorld::break_ankle_if(const float normal_speed, const float break_speed) noexcept {
    if (normal_speed < break_speed) {
        return;
    }
    ++ankle_break_count_;
    limp_seconds_left_ = kLimpSeconds;
}

void Simulation::PhysicsWorld::break_arm(const int side) noexcept {
    ++arm_break_count_;
    broken_arm_side_ = side;
    arm_broken_seconds_left_ = kArmBrokenSeconds;
}

bool Simulation::PhysicsWorld::apply_locomotion(JPH::BodyInterface &bodies,
                                                const StepCommands &commands,
                                                const float delta_seconds) noexcept {
    JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    JPH::Vec3 reference_velocity = airborne_inherited_velocity_;
    // Crouched, the legs walk short; tucked in the air, they do not walk at all.
    const double speed_scale = crouched_ && !tucked_ ? kCrouchSpeedScale : 1.0;
    double move_x = commands.move_input_x * speed_scale;
    double move_z = commands.move_input_z * speed_scale;
    const bool had_line = line_held_;
    if (!grounded_) {
        sliding_ = false;   // off the ground the slide is a fall
    }
    sprinting_ = false;
    balancing_ = false;
    beam_xcom_ = 0.0F;
    beam_lean_ = JPH::Vec3::sZero();
    line_held_ = false;
    float planted = 0.0F;

    if (grounded_ && support_entity_id_ != 0) {
        reference_velocity = current_support_point_velocity(bodies);
        airborne_inherited_velocity_ = reference_velocity;
        take_shove(player_velocity);
        track_support_jolt(reference_velocity, delta_seconds);
        const Beam beam = beam_underfoot(bodies);
        const double input = std::hypot(commands.move_input_x, commands.move_input_z);
        bool on_line = false;
        float line_lean = 0.0F;
        if (beam.valid) {
            // On a beam: along it at a walk, held on its line unless the
            // stick means to step off.
            balancing_ = true;
            const double along = move_x * beam.along.GetX() + move_z * beam.along.GetZ();
            const double across = move_x * beam.across.GetX() + move_z * beam.across.GetZ();
            if (std::abs(across) < kBalanceStepOffInput) {
                const double walk = std::clamp(along, -kBalanceSpeedScale, kBalanceSpeedScale);
                move_x = beam.along.GetX() * walk;
                move_z = beam.along.GetZ() * walk;
                on_line = true;
                line_lean = static_cast<float>(across / kBalanceStepOffInput);
            }
        } else if (commands.sprint_held && !crouched_ && carry_constraint_ == nullptr &&
                   input >= kSprintMinimumInput &&
                   (commands.move_input_x * facing_.GetX() +
                    commands.move_input_z * facing_.GetZ()) >= kSprintMaximumAngleCos * input) {
            sprinting_ = true;
        }
        // Balance at an edge (see kTeeterComHeight).
        // The feet push only as hard as they are pressed down: a hook's line
        // taking part of the body's weight takes that share of their traction
        // (task #102), so a reel that lifts drags the soles rather than
        // being out-pushed by them.
        float pressed = 1.0F;
        if (hook_state_ == HookState::Bitten && hook_tension_n_ > 0.0F) {
            const JPH::Vec3 line(hook_anchor(bodies) - hook_chest(bodies));
            const float length = line.Length();
            if (length > 1.0e-3F) {
                const float lift = hook_tension_n_ * line.GetY() / length;
                const float weight = kPlayerMassKg * -physics_system_.GetGravity().GetY();
                pressed = std::clamp(1.0F - lift / weight, 0.0F, 1.0F);
            }
        }
        const float traction = carry_ground_acceleration() * static_cast<float>(0.25 + 0.75 * recovery_balance()) *
                               ground_grip(bodies) * pressed;
        // While teetering, the most the feet may take off the speed toward
        // the edge in one second; steering across it keeps full traction.
        float teeter_brake = 0.0F;
        JPH::Vec3 teeter_direction = JPH::Vec3::sZero();
        const bool was_teetering = teetering_;
        teetering_ = false;
        if (!beam.valid && !rolling() && !sliding_) {
            const JPH::Vec3 relative = player_velocity - reference_velocity;
            const JPH::Vec3 horizontal(relative.GetX(), 0.0F, relative.GetZ());
            const float speed = horizontal.Length();
            // Entered at a sprint, a teeter lasts until the body stops or
            // its XCoM is back over the footing.
            if (speed >= kTeeterMinimumSpeed || (was_teetering && speed > 0.3F)) {
                const float gravity = -physics_system_.GetGravity().GetY();
                const float height = crouched_ ? kTeeterCrouchedComHeight : kTeeterComHeight;
                const JPH::Vec3 xcom_offset = horizontal / std::sqrt(gravity / height);
                const JPH::RVec3 centre = bodies.GetPosition(player_id_);
                const float soles = static_cast<float>(centre.GetY()) - active_half_height();
                const JPH::Vec3 direction = horizontal / speed;
                const double drive = commands.move_input_x * direction.GetX() + commands.move_input_z * direction.GetZ();
                if (drive <= 0.0 && feet_supported_at(centre, soles) &&
                    !ground_runs_to(centre, xcom_offset, soles)) {
                    teetering_ = true;
                    teeter_direction = direction;
                    teeter_brake = drive <= -kTeeterDeliberateInput
                                       ? kTeeterBrakeAcceleration
                                       : kTeeterBrakeAcceleration * kTeeterPassiveScale;
                    if (!was_teetering) {
                        ++teeter_count_;
                    }
                }
            }
        }
        const double balance = recovery_balance();
        if (balance < 0.8 || limping()) {
            sprinting_ = false;
        }
        float stride_speed = kPlayerMaximumRelativeSpeed * carry_speed_scale();
        if (sprinting_) {
            // See kFlowSpeedMps: the sprint runs at what the body carries,
            // gathering on, between the sprint's speed and flow's.
            const JPH::Vec3 relative = player_velocity - reference_velocity;
            const float carried = JPH::Vec3(relative.GetX(), 0.0F, relative.GetZ()).Length();
            const float sprint = static_cast<float>(kPlayerMaximumRelativeSpeed * kSprintSpeedScale);
            stride_speed = std::clamp(carried + kFlowGatherMps2 * delta_seconds, sprint, kFlowSpeedMps);
        }
        if (limping()) {
            stride_speed *= kLimpSpeedScale;
        }
        const float full_speed = stride_speed * static_cast<float>(0.45 + 0.55 * balance);
        if (sliding_ && !crouched_) {
            sliding_ = false;
        }
        if (rolling()) {
            roll_locomotion(player_velocity, reference_velocity, commands, delta_seconds);
        } else if (sliding_) {
            sprinting_ = false;
            slide_locomotion(player_velocity, reference_velocity, commands, delta_seconds);
        } else {
            const JPH::Vec3 before = player_velocity - reference_velocity;
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  move_x,
                                                  move_z,
                                                  traction,
                                                  delta_seconds,
                                                  full_speed);
            if (on_line) {
                // Across the beam only the pendulum (see kBalanceHipMargin).
                const float gravity = -physics_system_.GetGravity().GetY();
                const float omega = std::sqrt(gravity / (crouched_ ? kTeeterCrouchedComHeight : kTeeterComHeight));
                const float reach = beam.half_width + kBalanceHipMargin;
                const float x = beam.offset;
                // The pendulum's own velocity across the line, carried from
                // the last tick: the soles' grip holds the capsule's rim
                // contact, whose normal tilts outward past the beam's side.
                const float v = had_line ? line_velocity_.Dot(beam.across) : before.Dot(beam.across);
                const float xcom = x + v / omega;
                const float target = line_lean * kBalanceLeanFraction * reach;
                const float pressure = std::clamp(xcom + kBalanceCaptureGain * (xcom - target), -reach, reach);
                const float v_next = v + omega * omega * (x - pressure) * delta_seconds;
                const float now = (player_velocity - reference_velocity).Dot(beam.across);
                player_velocity += beam.across * (v_next - now);
                line_velocity_ = beam.across * v_next;
                line_held_ = true;
                beam_xcom_ = xcom / reach;
                beam_lean_ = beam.across * beam_xcom_;
            }
            if (teetering_) {
                const JPH::Vec3 change = player_velocity - reference_velocity - before;
                const float along = change.Dot(teeter_direction);
                const float limit = -teeter_brake * delta_seconds;
                if (along < limit) {
                    player_velocity += teeter_direction * (limit - along);
                }
            }
        }
        // The air keeps what the ground gave: a running jump, or a run
        // off an edge, carries its speed.
        const JPH::Vec3 relative = player_velocity - reference_velocity;
        air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                                   JPH::Vec3(relative.GetX(), 0.0F, relative.GetZ()).Length());
        // Planted: the stick let go, no line hauling the body (taut, or its
        // reel turning: a reel drags the soles, through `pressed` above), and
        // the feet have brought it to rest on its support. They hold it
        // through the solve as well (PlayerContactListener::set_planted_grip),
        // with the grip they brake with on level ground: the load on them (a
        // slope's share, a line's lift) is the contact's own force there, so
        // it is not taken off twice. Never while the body moves: a stop takes
        // as long as it did, and a stick pushed toward an edge walks off it as
        // freely.
        const bool hauled = hook_state_ == HookState::Bitten && (commands.hook_reel || hook_tension_n_ > 0.0F);
        if (move_x == 0.0 && move_z == 0.0 && !rolling() && !sliding_ && !teetering_ && !hauled &&
            JPH::Vec3(relative.GetX(), 0.0F, relative.GetZ()).IsNearZero()) {
            planted = kGroundAcceleration * static_cast<float>(0.25 + 0.75 * recovery_balance()) /
                      -physics_system_.GetGravity().GetY();
        }
    } else {
        teetering_ = false;
        clear_support_jolt();
        // In the air the stick steers toward what it asks for with at most
        // kAirAcceleration (weaker than the ground's). Let go in flight (a
        // jump, or ground no longer within a step under the soles), nothing
        // acts but gravity and drag: a committed arc keeps its momentum.
        // A step's brief lift, the foot still on the ground below, is not
        // flight.
        const JPH::RVec3 centre = bodies.GetPosition(player_id_);
        const bool in_flight = flight_from_jump_ ||
                               !feet_supported_at(centre, static_cast<float>(centre.GetY()) - active_half_height());
        if (hook_state_ == HookState::Bitten && hook_tension_n_ > kHookTautTensionN) {
            // On the taut line the stick pumps the swing, as on a bar.
            const JPH::Vec3 input(static_cast<float>(commands.move_input_x), 0.0F,
                                  static_cast<float>(commands.move_input_z));
            const JPH::Vec3 relative = player_velocity - reference_velocity;
            if (input.LengthSq() > 1.0e-2F && relative.Dot(input) > -kSwingKipFromRestMps) {
                bodies.AddForce(player_id_,
                                input * (std::min(1.0F, input.Length()) * kSwingKipForceN / input.Length()));
            }
        } else if (std::hypot(move_x, move_z) > 1.0e-3 || !in_flight) {
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  move_x,
                                                  move_z,
                                                  kAirAcceleration,
                                                  delta_seconds,
                                                  air_full_speed_);
        }
        update_wall_run(bodies, commands, player_velocity, reference_velocity);
    }

    // The feet just off the ground at a walk's speeds (kCoyoteTicks): a jump
    // still goes, as from the ground it ran off; otherwise, with walkable
    // support within a step below (a ramp's crest run over, a stair's step
    // walked off), the legs extend and put the feet back on it, rather than
    // the body sailing on over the crest.
    {
        const float lift = player_velocity.GetY() - reference_velocity.GetY();
        if (grounded_) {
            coyote_ticks_ = kCoyoteTicks;
        } else if (coyote_ticks_ > 0 && lift < kCoyoteMaxRiseMps && lift > -kCoyoteMaxFallMps) {
            --coyote_ticks_;
        } else {
            coyote_ticks_ = 0;
        }
        if (step_up_grace_ticks_ > 0) {
            --step_up_grace_ticks_;
        }
        // Only a body going somewhere over the ground (at least a step's
        // walking speed) and not hauled by a line that takes its weight: a
        // hook's reel lifting a body that stands still is not a crest, and
        // the legs do not pull it back down.
        const JPH::Vec3 over_ground = player_velocity - reference_velocity;
        const bool walking_over =
            JPH::Vec3(over_ground.GetX(), 0.0F, over_ground.GetZ()).Length() >= kStepMinimumSpeed;
        const bool line_lifting = hook_state_ == HookState::Bitten && hook_tension_n_ > 0.0F;
        const bool settling_on_step = step_up_grace_ticks_ > 0 && lift < kStepUpLaunchMps;
        if (!grounded_ && coyote_ticks_ > 0 && !settling_on_step && walking_over && !line_lifting &&
            !commands.jump_requested && jump_buffer_ticks_ == 0 &&
            try_step_down(bodies, player_velocity, reference_velocity)) {
            ++step_down_count_;
        }
    }
    // Crouched here means there was no room to stand, so no room to jump.
    const bool jump_wanted = commands.jump_requested || jump_buffer_ticks_ > 0;
    const bool jump_started = jump_wanted && (grounded_ || coyote_ticks_ > 0) && !crouched_;
    bool kicked = false;
    if (grounded_) {
        flight_from_jump_ = false;
        wall_steps_this_flight_ = 0;
        wall_kicks_this_flight_ = 0;
        wall_step_cooldown_ = 0;
        wall_climbed_this_flight_ = false;
        // The run on the wall ends with the feet on the ground. Its hold only
        // counts down in the air, so landed inside it, it was kept: the body
        // read as running a wall on open ground (the view leaning off it), and
        // the next jump's second press kicked off nothing, a jump in the air.
        wall_run_seconds_left_ = 0.0;
    }
    if (commands.jump_requested && !grounded_ && wall_running() && wall_kicks_this_flight_ < kWallKicksMax) {
        // Kicked off the wall: out along its normal, along it where the
        // stick points (the tic-tac), and up by what the wall's friction
        // gives that push.
        const JPH::Vec3 along(-wall_normal_.GetZ(), 0.0F, wall_normal_.GetX());
        const float steer = std::clamp(static_cast<float>(commands.move_input_x * along.GetX() +
                                                          commands.move_input_z * along.GetZ()),
                                       -1.0F, 1.0F);
        player_velocity += wall_normal_ * kWallKickMps + along * (steer * kWallKickAlongMps);
        player_velocity.SetY(std::max(player_velocity.GetY() - reference_velocity.GetY(), 0.0F) +
                             reference_velocity.GetY() + kWallFriction * kWallKickMps);
        // The steps start again off the kick: a second wall can be run.
        wall_steps_this_flight_ = 0;
        wall_step_cooldown_ = kWallStepTicks;
        wall_run_seconds_left_ = 0.0;
        ++wall_kicks_this_flight_;
        ++wall_kick_count_;
        kicked = true;
    }
    if (jump_started) {
        jump_buffer_ticks_ = 0;
        coyote_ticks_ = 0;
    } else if (commands.jump_requested && !grounded_ && !kicked && jump_vault_ticks_left_ == 0) {
        jump_buffer_ticks_ = kJumpBufferTicks;
    }
    // A run's jump at something knee- to waist-high close ahead goes from
    // its top (kSpringboardMinimumSpeed); a sighted landing is jumped as it
    // was sighted.
    if (jump_started && !precision_sighted_ && try_begin_springboard(bodies, player_velocity - reference_velocity)) {
        flight_from_jump_ = false;
        return true;
    }
    if (jump_started) {
        flight_from_jump_ = true;
        precision_target_valid_ = false;
        // A broken ankle pushes off weakly (kLimpJumpScale).
        const float jump_speed = kJumpSpeed * (limping() ? kLimpJumpScale : 1.0F);
        if (precision_sighted_) {
            // The sighted landing: the horizontal speed that lands the
            // feet there under the jump's own arc, within the legs' push.
            const JPH::RVec3 centre = bodies.GetPosition(player_id_);
            const float feet_y = static_cast<float>(centre.GetY()) - kPlayerHalfHeight;
            const float rise = static_cast<float>(precision_point_.GetY()) - feet_y;
            const float gravity = -physics_system_.GetGravity().GetY();
            const float disc = jump_speed * jump_speed - 2.0F * gravity * rise;
            if (disc > 0.0F) {
                const float flight = (jump_speed + std::sqrt(disc)) / gravity;   // the later root: landing
                const JPH::Vec3 to(static_cast<float>(precision_point_.GetX() - centre.GetX()), 0.0F,
                                   static_cast<float>(precision_point_.GetZ() - centre.GetZ()));
                const float ahead = to.Length();
                if (ahead > 1.0e-3F && flight > 1.0e-3F) {
                    const JPH::Vec3 dir = to / ahead;
                    const JPH::Vec3 wanted =
                        dir * std::min(ahead / flight,
                                       kPlayerMaximumRelativeSpeed * static_cast<float>(kSprintSpeedScale));
                    const JPH::Vec3 have(player_velocity.GetX() - reference_velocity.GetX(), 0.0F,
                                         player_velocity.GetZ() - reference_velocity.GetZ());
                    JPH::Vec3 change = wanted - have;
                    const float size = change.Length();
                    if (size > kPrecisionPushMps) {
                        change *= kPrecisionPushMps / size;
                    }
                    player_velocity += change;
                    precision_target_valid_ = true;
                    precision_target_ = precision_point_;
                    ++precision_jump_count_;
                }
            }
        }
        const float push = reference_velocity.GetY() + jump_speed - player_velocity.GetY();
        player_velocity.SetY(reference_velocity.GetY() + jump_speed);
        // The legs push off what they stand on: a dynamic support takes the
        // equal and opposite impulse at the contact (a cage dips, a beam
        // rocks), so the jump's work is shared with what it left.
        const JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
        if (!support_id.IsInvalid() && push > 0.0F &&
            bodies.GetMotionType(support_id) == JPH::EMotionType::Dynamic) {
            bodies.AddImpulse(support_id, JPH::Vec3(0.0F, -push * static_cast<float>(kPlayerMassKg), 0.0F),
                              JPH::RVec3(support_sample_.contact_point.x, support_sample_.contact_point.y,
                                         support_sample_.contact_point.z));
            ++jump_reactions_;
        }
        jump_takeoff_feet_y_ =
            static_cast<float>(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
        jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    }
    bodies.SetLinearVelocity(player_id_, player_velocity);
    last_set_velocity_ = player_velocity;
    last_set_valid_ = grounded_ && !jump_started;
    planted_grip_ = jump_started ? 0.0F : planted;
    if (!jump_started && grounded_ && support_entity_id_ != 0) {
        try_step_up(bodies, player_velocity - reference_velocity, delta_seconds);
    }
    return jump_started;
}

// The capsule swept from `from` by `displacement`: true on a hit, with
// the hit fraction and the surface normal (pointing out of what was hit).
bool Simulation::PhysicsWorld::cast_capsule(const JPH::RVec3 from, const JPH::Vec3 displacement,
                                            float &fraction, JPH::Vec3 &normal) const {
    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    const JPH::RShapeCast sweep(active_player_shape(), JPH::Vec3::sReplicate(1.0F),
                                JPH::RMat44::sTranslation(from), displacement);
    const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
    physics_system_.GetNarrowPhaseQuery().CastShape(
        sweep, JPH::ShapeCastSettings(), from, collector, {}, {}, body_filter);
    if (!collector.HadHit()) {
        return false;
    }
    fraction = collector.mHit.mFraction;
    normal = -collector.mHit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
    return true;
}

// Stepping up. Blocked at the feet, clear with the body raised by the
// maximum step, and walkable support found by sweeping back down: then
// the body is lifted onto the step, as Jolt's own character controller
// walks stairs. Every stage is a sweep against real geometry, so nothing
// is passed through and nothing is stood on that is not there.
void Simulation::PhysicsWorld::try_step_up(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                                           const float delta_seconds) {
    const JPH::Vec3 horizontal(relative_velocity.GetX(), 0.0F, relative_velocity.GetZ());
    const float speed = horizontal.Length();
    if (speed < kStepMinimumSpeed) {
        return;
    }
    const JPH::Vec3 ahead = horizontal / speed * (speed * delta_seconds + kStepLookahead);
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    float fraction = 1.0F;
    JPH::Vec3 normal = JPH::Vec3::sZero();
    if (!cast_capsule(at, ahead, fraction, normal) ||
        normal.GetY() >= kSupportNormalThreshold) {
        return;  // Nothing in the way, or only a slope the feet can walk.
    }
    const JPH::RVec3 raised = at + JPH::Vec3(0.0F, kStepMaximumHeight, 0.0F);
    if (!capsule_pose_is_clear(raised) || cast_capsule(raised, ahead, fraction, normal)) {
        return;  // No headroom, or the obstacle is taller than a step.
    }
    const JPH::RVec3 over = raised + ahead;
    const JPH::Vec3 down(0.0F, -(kStepMaximumHeight + kStepMinimumHeight), 0.0F);
    if (!cast_capsule(over, down, fraction, normal) ||
        normal.GetY() < kSupportNormalThreshold) {
        return;  // Nothing walkable to put the feet on.
    }
    const float rise = kStepMaximumHeight + down.GetY() * fraction;
    if (rise < kStepMinimumHeight) {
        return;
    }
    bodies.SetPosition(player_id_, at + JPH::Vec3(0.0F, rise + 0.01F, 0.0F),
                       JPH::EActivation::Activate);
    ++step_up_count_;
    step_up_meters_ += static_cast<double>(rise + 0.01F);
    step_up_grace_ticks_ = kStepUpGraceTicks;
}

// Stepping down. The feet just off the ground (the coyote window) with
// ground under the body's centre within the maximum step (a ray: a corner
// the capsule's side still grazes as it goes over an edge is not ground to
// stand on, and the body must leave an edge cleanly) are swept down onto
// it: the body put at the touch and its upward speed, the crest's launch,
// taken off. Off a real edge nothing is found, and the body falls.
bool Simulation::PhysicsWorld::try_step_down(JPH::BodyInterface &bodies, JPH::Vec3 &player_velocity,
                                             const JPH::Vec3 reference_velocity) {
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const float half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    const JPH::RRayCast under(at, JPH::Vec3(0.0F, -(half_height + kStepMaximumHeight), 0.0F));
    JPH::RayCastResult under_hit;
    const JPH::IgnoreSingleBodyFilter self(player_id_);
    if (!physics_system_.GetNarrowPhaseQuery().CastRay(under, under_hit, {}, {}, self)) {
        return false;
    }
    const JPH::Vec3 down(0.0F, -kStepMaximumHeight, 0.0F);
    float fraction = 1.0F;
    JPH::Vec3 normal = JPH::Vec3::sZero();
    if (!cast_capsule(at, down, fraction, normal) || normal.GetY() < kSupportNormalThreshold) {
        return false;
    }
    bodies.SetPosition(player_id_, at + down * fraction, JPH::EActivation::Activate);
    player_velocity.SetY(std::min(player_velocity.GetY(), reference_velocity.GetY()));
    return true;
}

// Real quadratic drag opposing the full velocity vector, not a clamp: it
// can only ever pull speed toward the terminal value, never accelerate
// the player upward past what deceleration implies (Governing Law 7 --
// no powered ascent). Applied on top of ordinary air control, so existing
// horizontal steering doubles as the "redirection" the same law permits.
void Simulation::PhysicsWorld::apply_parachute_drag(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
    if (!parachute_deployed_) {
        return;
    }
    JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    const float speed = velocity.Length();
    if (speed > 1.0e-4F) {
        const JPH::Vec3 drag_acceleration =
            -(velocity / speed) * (kParachuteDragCoefficient * speed * speed);
        velocity += drag_acceleration * delta_seconds;
        bodies.SetLinearVelocity(player_id_, velocity);
    }
}

// How loud the player is this tick, 0..1, for the Matron's hearing (see
// matron.hpp): a hard landing is loudest, then a sprint, then a walk or a
// climb on rungs; a crouched creep is nearly silent and standing still makes
// no sound.
float Simulation::PhysicsWorld::matron_loudness(JPH::BodyInterface &bodies) noexcept {
    float loud = 0.0F;
    if (landing_count_ != matron_landing_seen_) {
        matron_landing_seen_ = landing_count_;
        const float s = landing_normal_speed_mps_;
        loud = s >= 8.0F ? 1.0F : (s >= 5.0F ? 0.7F : (s >= 3.0F ? 0.35F : 0.0F));
    }
    const JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    const float speed = std::hypot(velocity.GetX(), velocity.GetZ());
    if (grounded_) {
        if (crouched_) {
            if (speed > 0.5F) {
                loud = std::max(loud, 0.04F);
            }
        } else if (sprinting_ && speed > 6.0F) {
            loud = std::max(loud, 0.8F);
        } else if (speed >= 3.5F) {
            loud = std::max(loud, 0.30F);
        }
    } else if (traversal_state_ == TraversalState::Climbing && std::abs(velocity.GetY()) > 0.3F) {
        // Boots and hands on steel rungs, going up or down them.
        loud = std::max(loud, 0.25F);
    }
    // Driving a forklift, its noise is the driver's (CHOSEN): the engine from
    // a tick-over (0.2, 11 m) to flat out (0.5, 27 m), more with the pump
    // working the forks; backing, its reverse alarm (0.6, 33 m).
    const int truck = active_forklift();
    if (truck >= 0 && forklifts_[static_cast<std::size_t>(truck)]->driving()) {
        const Forklift::State &f = forklifts_[static_cast<std::size_t>(truck)]->state();
        float engine = 0.2F + 0.3F * std::min(1.0F, f.engine_rpm / 3000.0F);
        if (f.lift > 0.05F) {
            engine += 0.15F * std::min(1.0F, f.lift_n / Forklift::kLiftForceN);
        }
        loud = std::max(loud, engine);
        if (f.gear < 0) {
            loud = std::max(loud, 0.6F);
        }
    }
    return loud;
}

} // namespace scraperx::sim
