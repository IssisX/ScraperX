#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] Simulation::PhysicsWorld::Beam Simulation::PhysicsWorld::beam_underfoot(const JPH::BodyInterface &bodies) const noexcept {
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

void Simulation::PhysicsWorld::clear_traversal(bool preserve_valid_foot) noexcept {
    clear_palm_props();
    palm_vault_used_ = false;
    hang_topout_ticks_left_ = 0;
    hang_topout_left_generation_ = hang_topout_right_generation_ = 0;
    second_hand_added_this_tick_ = false;
    regrip_cooldown_ticks_ = 0;
    hanging_posture_offset_m_ = 0.0F;
    hanging_posture_input_ = false;
    cargo_transfer_active_=false;
    cargo_push_seconds_=-1.0F;
    if (physical_hands_) physical_hands_->clear();
    if (physical_foot_push_ && !preserve_valid_foot) physical_foot_push_->clear();
    if (!preserve_valid_foot) {
        foot_push_purpose_ = FootPushPurpose::None;
        foot_contact_ = {};
        foot_push_contact_ = {};
        rigid_transfer_foot_valid_ = false;
        state_.hand_foot_last_solve_tick=0;
        state_.hand_foot_last_hand_load_n=state_.hand_foot_last_foot_load_n=0;
    }
    traversal_plan_.clear();
    traversal_affordances_.clear();
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
        grip_affordance_ = grip_in_front(bodies);
        if (grounded_) edge_affordance_ = probe_edge_drop(bodies);
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
}

} // namespace scraperx::sim
