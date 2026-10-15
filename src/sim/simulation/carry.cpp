#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::carry_hand_offset(const JPH::Vec3 bearing) noexcept {
    return bearing * kCarryHandForward + JPH::Vec3(0.0F, kCarryHandUp, 0.0F);
}

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::carry_hand_bearing(const JPH::BodyInterface &bodies,
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

[[nodiscard]] JPH::Vec3 Simulation::PhysicsWorld::carry_handle(const std::uint64_t entity) const noexcept {
    if (scraperx::sim::kit::is_kit_entity(entity)) {
        return kit_->carry_handle(entity);
    }
    return JPH::Vec3(0.0F,
                     entity == Simulation::kHook5BarEntityId ? kHook5BarHalfSection
                                                             : kHook5BlockHalfY,
                     0.0F);
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
    const std::pair<JPH::BodyID, std::uint64_t> carryables[] = {
        {hook5_bar_id_, Simulation::kHook5BarEntityId},
        {hook5_block_id_, Simulation::kHook5BlockEntityId},
    };
    const auto consider = [&](const JPH::BodyID id, const std::uint64_t candidate_entity,
                              const JPH::Vec3 local_handle, const bool use_handle) {
        if (id.IsInvalid()) { return; }
        const JPH::RVec3 target = use_handle
            ? bodies.GetCenterOfMassTransform(id) * local_handle
            : bodies.GetCenterOfMassPosition(id);
        const JPH::Vec3 to_body(target - at);
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
    for (const auto &[id, candidate_entity] : carryables) {
        consider(id, candidate_entity, JPH::Vec3::sZero(), false);
    }
    kit_->carry_candidates(kit_carryables_);
    for (const auto &candidate : kit_carryables_) {
        // Long control grips must be reachable at the actual hand point,
        // which can be well below their centre of mass.
        consider(candidate.id, candidate.entity, candidate.handle,
                 candidate.kind == scraperx::sim::kit::CarryKind::Handle);
    }
    return best;
}

void Simulation::PhysicsWorld::attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept {
    JPH::PointConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
    settings.mPoint1 = JPH::RVec3(carry_hand_offset(facing_));
    settings.mPoint2 = JPH::RVec3(carry_handle(entity));
    if (kit_->carry_kind(entity) == scraperx::sim::kit::CarryKind::Handle) {
        // A control grip is reached where it stands. Snapping a constrained
        // handle to the portable-load pose creates an artificial impulse
        // and can break the grip before the player even starts pulling.
        const auto &bodies = physics_system_.GetBodyInterface();
        const auto contact = bodies.GetCenterOfMassTransform(id) * settings.mPoint2;
        settings.mPoint1 = bodies.GetCenterOfMassTransform(player_id_).Inversed() * contact;
    }
    carry_constraint_ = static_cast<JPH::PointConstraint *>(
        create_constraint(settings, player_id_, id, false));
    if (carry_constraint_ == nullptr) {
        return;
    }
    carried_id_ = id;
    carried_entity_ = entity;
    grip_over_steps_ = 0;
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
        const JPH::Vec3 offset = kit_->carry_kind(carried_entity_) == scraperx::sim::kit::CarryKind::Handle
            ? carry_constraint_->GetLocalSpacePoint1()
            : carry_hand_offset(carry_hand_bearing(bodies, delta_seconds));
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
            grip_over_steps_ = pull > kGripNewtons ? grip_over_steps_ + 1U : 0U;
            if (grip_over_steps_ >= kGripSteps) {
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
    if (committed_entity == Simulation::kHook5BarEntityId) {
        attach_carry(hook5_bar_id_, committed_entity);
    } else if (committed_entity == Simulation::kHook5BlockEntityId) {
        attach_carry(hook5_block_id_, committed_entity);
    } else if (scraperx::sim::kit::is_kit_entity(committed_entity)) {
        const scraperx::sim::kit::BodyIndex body = kit_->body_for_entity(committed_entity);
        if (kit_->body_enabled(body)) {
            attach_carry(kit_->body_id(body), committed_entity);
        }
    }
}

[[nodiscard]] JPH::RVec3 Simulation::PhysicsWorld::hand_position(const JPH::BodyInterface &bodies) const noexcept {
    if (carry_constraint_ != nullptr &&
        kit_->carry_kind(carried_entity_) == scraperx::sim::kit::CarryKind::Handle) {
        return bodies.GetCenterOfMassTransform(player_id_) * carry_constraint_->GetLocalSpacePoint1();
    }
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

} // namespace scraperx::sim
