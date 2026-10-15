#include "sim/physics_internal.hpp"

// Walking, stepping up, crouch, carry, rigging, parachute, and beams.

namespace scraperx::sim {

void Simulation::PhysicsWorld::remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
    // A proving-ground fixture is never built in the game's world.
    if (body_id.IsInvalid()) {
        return;
    }
    bodies.RemoveBody(body_id);
    bodies.DestroyBody(body_id);
}

JPH::Body *Simulation::PhysicsWorld::add_shape_body(JPH::BodyInterface &bodies,
                                 const JPH::Shape *shape,
                                 const JPH::RVec3 position,
                                 const JPH::Quat rotation,
                                 const JPH::EMotionType motion_type,
                                 const JPH::ObjectLayer layer,
                                 const float friction,
                                 const std::uint64_t entity_id,
                                 const float mass_kg) {
    JPH::BodyCreationSettings settings(shape, position, rotation, motion_type, layer);
    settings.mFriction = friction;
    settings.mUserData = entity_id;
    settings.mAllowSleeping = false;
    if (mass_kg > 0.0F) {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass_kg;
    }
    JPH::Body *body = bodies.CreateBody(settings);
    const JPH::EActivation activation = motion_type == JPH::EMotionType::Static
                                            ? JPH::EActivation::DontActivate
                                            : JPH::EActivation::Activate;
    bodies.AddBody(body->GetID(), activation);
    return body;
}

JPH::BodyID Simulation::PhysicsWorld::add_box(JPH::BodyInterface &bodies,
                           const JPH::Vec3 half_extent,
                           const JPH::RVec3 position,
                           const JPH::EMotionType motion_type,
                           const JPH::ObjectLayer layer,
                           const float friction,
                           const std::uint64_t entity_id,
                           const JPH::Quat rotation,
                           const float mass_kg) {
    return add_shape_body(bodies,
                          new JPH::BoxShape(half_extent),
                          position,
                          rotation,
                          motion_type,
                          layer,
                          friction,
                          entity_id,
                          mass_kg)
        ->GetID();
}

JPH::Vec3 Simulation::PhysicsWorld::normalized_horizontal(const double x, const double z) noexcept {
    const double length = std::hypot(x, z);
    if (!(length > 1.0e-6)) {
        return JPH::Vec3::sZero();
    }
    return JPH::Vec3(static_cast<float>(x / length), 0.0F, static_cast<float>(z / length));
}

JPH::BodyID Simulation::PhysicsWorld::body_id_for_entity(const std::uint64_t entity_id) const noexcept {
    switch (entity_id) {
    case Simulation::kStaticDeckEntityId:
        return deck_id_;
    case Simulation::kTranslatingSupportEntityId:
        return translating_support_id_;
    case Simulation::kRotatingSupportEntityId:
        return rotating_support_id_;
    case Simulation::kVaultRailEntityId:
        return vault_rail_id_;
    case Simulation::kMantleLedgeEntityId:
        return mantle_ledge_id_;
    case Simulation::kHangLedgeEntityId:
        return hang_ledge_id_;
    case Simulation::kMovingLedgeEntityId:
        return moving_ledge_id_;
    case Simulation::kBlockedLedgeEntityId:
        return blocked_ledge_id_;
    case Simulation::kBlockedLedgeCanopyEntityId:
        return blocked_ledge_canopy_id_;
    case Simulation::kTowerEntityId:
        return tower_id_;
    default:
        return {};
    }
}

JPH::Vec3 Simulation::PhysicsWorld::carry_hand_offset(const JPH::Vec3 bearing) noexcept {
    return bearing * kCarryHandForward + JPH::Vec3(0.0F, kCarryHandUp, 0.0F);
}

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

JPH::Vec3 Simulation::PhysicsWorld::carry_handle(const std::uint64_t entity) const noexcept {
    return kit_->carry_handle(entity);
}

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
    const auto consider = [&](const JPH::BodyID id, const std::uint64_t candidate_entity) {
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

void Simulation::PhysicsWorld::attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept {
    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
    settings.mPoint1 = JPH::RVec3(carry_hand_offset(facing_));
    settings.mPoint2 = JPH::RVec3(carry_handle(entity));
    carry_constraint_ = static_cast<JPH::PointConstraint *>(
        create_constraint(settings, player_id_, id, false));
    if (carry_constraint_ == nullptr) {
        return;
    }
    carried_id_ = id;
    carried_entity_ = entity;
    grip_over_seconds_ = 0.0F;
    contact_listener_.set_carried_entity(entity);
}

void Simulation::PhysicsWorld::release_carry() noexcept {
    if (carry_constraint_ != nullptr) {
        physics_system_.RemoveConstraint(carry_constraint_);
        carry_constraint_ = nullptr;
    }
    carried_id_ = JPH::BodyID();
    carried_entity_ = 0;
    contact_listener_.set_carried_entity(0);
}

void Simulation::PhysicsWorld::update_carry(JPH::BodyInterface &bodies, const StepCommands &commands,
                  const float delta_seconds) noexcept {
    if (carry_constraint_ != nullptr) {
        if (commands.set_down_requested) {
            release_carry();
            return;
        }
        // The hands swing round to the facing.
        const JPH::Vec3 offset = carry_hand_offset(carry_hand_bearing(bodies, delta_seconds));
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
            }
        }
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

JPH::RVec3 Simulation::PhysicsWorld::hand_position(const JPH::BodyInterface &bodies) const noexcept {
    return bodies.GetPosition(player_id_) + carry_hand_offset(facing_);
}

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

void Simulation::PhysicsWorld::update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (traversal_state_ != TraversalState::None) {
        return;
    }
    const bool wants_up =
        !commands.crouch_held || commands.jump_requested || commands.traversal_requested;
    if (crouched_) {
        if (wants_up) {
            (void)try_stand(bodies);
        }
        return;
    }
    if (!wants_up && grounded_) {
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        bodies.SetShape(player_id_, player_crouch_shape_.GetPtr(), false,
                        JPH::EActivation::Activate);
        bodies.SetPosition(player_id_, at - JPH::Vec3(0.0F, kCrouchDrop, 0.0F),
                           JPH::EActivation::Activate);
        crouched_ = true;
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

bool Simulation::PhysicsWorld::apply_locomotion(JPH::BodyInterface &bodies,
                                    const StepCommands &commands,
                                    const float delta_seconds) noexcept {
    JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    JPH::Vec3 reference_velocity = airborne_inherited_velocity_;
    const double speed_scale = crouched_ ? kCrouchSpeedScale : 1.0;
    double move_x = commands.move_input_x * speed_scale;
    double move_z = commands.move_input_z * speed_scale;
    sprinting_ = false;
    balancing_ = false;

    if (grounded_ && support_entity_id_ != 0) {
        reference_velocity = current_support_point_velocity(bodies);
        airborne_inherited_velocity_ = reference_velocity;
        const Beam beam = beam_underfoot(bodies);
        const double input = std::hypot(commands.move_input_x, commands.move_input_z);
        if (beam.valid) {
            // On a beam: along it at a walk, held on its line unless the
            // stick means to step off.
            balancing_ = true;
            const double along = move_x * beam.along.GetX() + move_z * beam.along.GetZ();
            const double across = move_x * beam.across.GetX() + move_z * beam.across.GetZ();
            if (std::abs(across) < kBalanceStepOffInput) {
                const double walk = std::clamp(along, -kBalanceSpeedScale, kBalanceSpeedScale);
                const double centre =
                    std::clamp(-static_cast<double>(beam.offset * kBalanceCentering),
                               -static_cast<double>(kBalanceCenteringMaxMps),
                               static_cast<double>(kBalanceCenteringMaxMps)) /
                    kPlayerMaximumRelativeSpeed;
                move_x = beam.along.GetX() * walk + beam.across.GetX() * centre;
                move_z = beam.along.GetZ() * walk + beam.across.GetZ() * centre;
            }
        } else if (commands.sprint_held && !crouched_ && carry_constraint_ == nullptr &&
                   input >= kSprintMinimumInput &&
                   (commands.move_input_x * facing_.GetX() +
                    commands.move_input_z * facing_.GetZ()) >= kSprintMaximumAngleCos * input) {
            sprinting_ = true;
        }
        const float full_speed = sprinting_
                                     ? static_cast<float>(kPlayerMaximumRelativeSpeed * kSprintSpeedScale)
                                     : kPlayerMaximumRelativeSpeed;
        approach_relative_horizontal_velocity(player_velocity,
                                              reference_velocity,
                                              move_x,
                                              move_z,
                                              kGroundAcceleration,
                                              delta_seconds,
                                              full_speed);
        // The air keeps what the ground gave: a running jump, or a run
        // off an edge, carries its speed.
        const JPH::Vec3 relative = player_velocity - reference_velocity;
        air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                                   JPH::Vec3(relative.GetX(), 0.0F, relative.GetZ()).Length());
    } else {
        approach_relative_horizontal_velocity(player_velocity,
                                              reference_velocity,
                                              move_x,
                                              move_z,
                                              kAirAcceleration,
                                              delta_seconds,
                                              air_full_speed_);
    }

    // Crouched here means there was no room to stand, so no room to jump.
    const bool jump_started = commands.jump_requested && grounded_ && !crouched_;
    if (jump_started) {
        player_velocity.SetY(reference_velocity.GetY() + kJumpSpeed);
        jump_takeoff_feet_y_ =
            static_cast<float>(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
        jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    }
    bodies.SetLinearVelocity(player_id_, player_velocity);
    if (!jump_started && grounded_ && support_entity_id_ != 0) {
        try_step_up(bodies, player_velocity - reference_velocity, delta_seconds);
    }
    return jump_started;
}

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
}

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

Simulation::PhysicsWorld::Beam Simulation::PhysicsWorld::beam_underfoot(const JPH::BodyInterface &bodies) const noexcept {
    Beam beam;
    const JPH::RVec3 centre = bodies.GetPosition(player_id_);
    const float half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    JPH::RayCastResult hit;
    if (!cast_ray(centre, JPH::Vec3(0.0F, -(half_height + kCheckpointFootingSlack), 0.0F), hit)) {
        return beam;
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
        centre + JPH::Vec3(0.0F, -(half_height + kCheckpointFootingSlack) * hit.mFraction, 0.0F) -
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
    return beam;
}

} // namespace scraperx::sim
