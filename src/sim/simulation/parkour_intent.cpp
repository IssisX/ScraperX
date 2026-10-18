#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

namespace {
bool braking_horizontal_motion(double move_x, double move_z,
                               JPH::Vec3Arg relative) noexcept {
    return std::hypot(move_x, move_z) > .001 &&
        move_x * relative.GetX() + move_z * relative.GetZ() < 0;
}
} // namespace

void Simulation::PhysicsWorld::prepare_trailing_plant(const StepCommands &commands) noexcept {
    const bool trailing = foot_push_purpose_ == FootPushPurpose::TrailingPlant;
    const bool commanded = foot_push_purpose_ == FootPushPurpose::CoyotePush;
    const bool wall_kick = foot_push_purpose_ == FootPushPurpose::WallKick;
    if (!trailing && !commanded && !wall_kick) return;
    const bool cancelled = commands.cancel_jump_requested || commands.release_requested ||
        commands.set_down_requested || commands.sling_drop || commands.crouch_held ||
        traversal_state_ != TraversalState::None || physical_hands_->active() ||
        carry_constraint_ != nullptr || (slingshot_ && slingshot_->controls_player());
    const bool same_contact = foot_contact_.valid &&
        foot_contact_.solved_tick == contact_solve_serial_ &&
        foot_contact_.body == foot_push_contact_.body &&
        foot_contact_.subshape == foot_push_contact_.subshape;
    // Keep the first actual edge plant instead of rolling its anchor/rest
    // target down the rounded capsule every grounded tick. Fresh real contact
    // updates only input age; it never renews the actuator's original lifetime.
    if (trailing && grounded_ && same_contact)
        trailing_foot_contact_tick_ = contact_solve_serial_;
    const bool expired = !grounded_ &&
        contact_solve_serial_ - trailing_foot_contact_tick_ > kCoyotePlantIntentTicks;
    const bool ordinary_jump = grounded_ &&
        (commands.jump_requested || ordinary_jump_ticks_left_ > 0);
    bool returning_on_contact = false;
    if (trailing && grounded_ && same_contact) {
        const auto &bodies = physics_system_.GetBodyInterface();
        const auto anchor = from_support_local(bodies, foot_contact_.body, foot_contact_.local_point);
        const auto relative = bodies.GetLinearVelocity(player_id_) -
            support_point_velocity(bodies, foot_contact_.body, anchor);
        returning_on_contact = braking_horizontal_motion(
            commands.move_input_x, commands.move_input_z, relative);
    }
    // A trailing plant preserves an outgoing step. During deliberate return
    // input its compression can instead lift the capsule off a rounded edge
    // before ordinary traction/stepping recovers. Release only this passive
    // grounded topology; neutral/airborne plants and commanded pushes remain.
    if (cancelled || (trailing && (expired || ordinary_jump || returning_on_contact ||
                                  (grounded_ && !same_contact)))) {
        physical_foot_push_->clear();
        foot_push_purpose_ = FootPushPurpose::None;
    }
}

void Simulation::PhysicsWorld::retain_trailing_plant(const StepCommands &commands, bool jump_started) noexcept {
    if (jump_started || physical_foot_push_->active() || !grounded_ || crouched_ ||
        traversal_state_ != TraversalState::None || physical_hands_->active() || carry_constraint_ ||
        commands.cancel_jump_requested || commands.release_requested || commands.set_down_requested ||
        commands.sling_drop || commands.crouch_held || (slingshot_ && slingshot_->controls_player()) ||
        !foot_contact_.valid || foot_contact_.solved_tick != contact_solve_serial_ ||
        (deformable_plank_ && deformable_plank_->logical_member_for_body(foot_contact_.body))) return;
    auto &bodies = physics_system_.GetBodyInterface();
    const auto anchor = from_support_local(bodies, foot_contact_.body, foot_contact_.local_point);
    const auto at = bodies.GetCenterOfMassPosition(player_id_);
    const JPH::Vec3 delta(at - anchor);
    // Coverage ranks an edge opportunity only. The fresh solved bottom
    // contact above supplies the material anchor; no ray creates a foothold.
    // Waiting for half a capsule radius of lateral offset captures too late,
    // after the capsule has already rolled and accelerated down the edge.
    const JPH::Vec3 horizontal(delta.GetX(), 0, delta.GetZ());
    const auto relative = bodies.GetLinearVelocity(player_id_) -
        support_point_velocity(bodies, foot_contact_.body, anchor);
    if (horizontal.Dot(relative) <= 0 ||
        braking_horizontal_motion(commands.move_input_x, commands.move_input_z, relative) ||
        footing_is_firm(bodies) ||
        delta.GetY() < kPlayerHalfHeight - kPlayerRadius - kStandClearanceSkin ||
        delta.GetY() > kPlayerHalfHeight + kStandClearanceSkin) return;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), foot_contact_.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            lock.GetBody().GetShape() != foot_contact_.shape_identity ||
            lock.GetBody().GetUserData() != foot_contact_.entity_id) return;
    }
    const auto normal = bodies.GetRotation(foot_contact_.body) * foot_contact_.local_normal;
    if (!physical_foot_push_->begin_passive_plant(foot_contact_.body, anchor, normal, foot_contact_.friction)) return;
    foot_push_contact_ = foot_contact_;
    foot_push_purpose_ = FootPushPurpose::TrailingPlant;
    trailing_foot_contact_tick_ = contact_solve_serial_;
}

bool Simulation::PhysicsWorld::try_begin_coyote_push(JPH::BodyInterface &bodies) noexcept {
    if (grounded_ || ordinary_jump_ticks_left_ == 0 || crouched_ ||
        foot_push_purpose_ != FootPushPurpose::TrailingPlant || !physical_foot_push_->active() ||
        contact_solve_serial_ - trailing_foot_contact_tick_ > kCoyotePlantIntentTicks) return false;
    refresh_rigid_transfer_foot();
    if (!rigid_transfer_foot_valid_ || !physical_foot_push_->activate_planted_push()) return false;
    foot_push_purpose_ = FootPushPurpose::CoyotePush;
    ordinary_jump_ticks_left_ = 0;
    foot_push_started_this_tick_ = true;
    ++foot_push_start_count_;
    const auto anchor = physical_foot_push_->readback().world_anchor;
    airborne_inherited_velocity_ = support_point_velocity(bodies, foot_push_contact_.body, anchor);
    const auto departure = bodies.GetLinearVelocity(player_id_) - airborne_inherited_velocity_;
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(), 0, departure.GetZ()).Length());
    causal_airborne_ = true;
    jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
    jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    clear_landing_recovery();
    return true;
}

bool Simulation::PhysicsWorld::try_begin_wall_kick(JPH::BodyInterface &bodies) noexcept {
    if (grounded_ || ordinary_jump_ticks_left_ == 0 || crouched_ ||
        physical_foot_push_->active() || physical_hands_->active() || carry_constraint_ ||
        traversal_state_ != TraversalState::None ||
        (slingshot_ && slingshot_->controls_player()) || !wall_contact_.valid ||
        wall_contact_.solved_tick != contact_solve_serial_ ||
        wall_contact_.body == wall_kick_consumed_body_) return false;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),wall_contact_.body);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            !lock.GetBody().IsRigidBody() || lock.GetBody().GetShape() != wall_contact_.shape_identity ||
            lock.GetBody().GetUserData() != wall_contact_.entity_id) return false;
    }
    const auto anchor = from_support_local(bodies,wall_contact_.body,wall_contact_.local_point);
    const auto normal = bodies.GetRotation(wall_contact_.body)*wall_contact_.local_normal;
    const auto delta = JPH::Vec3(bodies.GetCenterOfMassPosition(player_id_)-anchor);
    const float height = delta.Dot(normal);
    const float tangent = (delta-normal*height).Length();
    const float contact_depth = kPlayerRadius+
        (kPlayerHalfHeight-kPlayerRadius)*std::abs(normal.GetY())+kLandingSkin;
    if (std::abs(normal.GetY()) > .2F || delta.GetY() < .05F || height <= 0 ||
        height > contact_depth ||
        tangent > PhysicalFootPush::lateral_reach_m() ||
        tangent > wall_contact_.friction*height ||
        !physical_foot_push_->begin_passive_plant(wall_contact_.body,anchor,normal,
                                                wall_contact_.friction)) return false;
    if (!physical_foot_push_->activate_planted_push()) {
        physical_foot_push_->clear();
        return false;
    }
    foot_push_contact_ = wall_contact_;
    foot_push_contact_.world_point = anchor;
    foot_push_contact_.world_normal = normal;
    foot_push_purpose_ = FootPushPurpose::WallKick;
    wall_kick_consumed_body_ = wall_contact_.body;
    ordinary_jump_ticks_left_ = 0;
    foot_push_started_this_tick_ = true;
    ++foot_push_start_count_;
    refresh_rigid_transfer_foot();
    airborne_inherited_velocity_ = support_point_velocity(bodies,wall_contact_.body,anchor);
    const auto departure = bodies.GetLinearVelocity(player_id_)-airborne_inherited_velocity_;
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
        JPH::Vec3(departure.GetX(),0,departure.GetZ()).Length());
    causal_airborne_ = true;
    jump_takeoff_feet_y_ = float(bodies.GetPosition(player_id_).GetY())-kPlayerHalfHeight;
    jump_vault_ticks_left_ = kJumpVaultWindowTicks;
    clear_landing_recovery();
    return true;
}
} // namespace scraperx::sim
