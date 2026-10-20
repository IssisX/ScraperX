#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;
bool Simulation::request_traversal_line(TraversalLineCommandKind kind, std::uint64_t target) noexcept {
    if (unsigned(kind) > unsigned(TraversalLineCommandKind::Recover)) return false;
    // Explicit cancellation wins over pending transactions and cannot be lost
    // to a full input queue. Admission is never a claim of physical success.
    if (kind == TraversalLineCommandKind::Release || kind == TraversalLineCommandKind::Unequip)
        traversal_line_command_count_ = 0;
    if (traversal_line_command_count_ == traversal_line_commands_.size()) return false;
    traversal_line_commands_[traversal_line_command_count_++] = {kind, target};
    return true;
}
bool Simulation::set_traversal_line_reel_input(double input) noexcept {
    if (!std::isfinite(input) || std::abs(input) > 1) return false;
    traversal_line_reel_input_ = input;
    return true;
}
TraversalLineSnapshot Simulation::traversal_line_state() const noexcept {
    return physics_world_->traversal_line_state();
}
TraversalLineSnapshot Simulation::render_traversal_line_state() const noexcept {
    auto out = traversal_line_state();
    const auto &previous = previous_traversal_line_state_;
    if (!out.installed || !previous.installed || previous.generation != out.generation ||
        previous.fitting_body != out.fitting_body || previous.support_body != out.support_body) return out;
    const double alpha = snapshot().interpolation_alpha;
    const auto interpolate = [alpha](Vector3 a, Vector3 b) {
        return Vector3{a.x + (b.x-a.x)*alpha, a.y + (b.y-a.y)*alpha, a.z + (b.z-a.z)*alpha};
    };
    out.anchor_point = interpolate(previous.anchor_point, out.anchor_point);
    out.grip_point = interpolate(previous.grip_point, out.grip_point);
    if (out.path_valid && previous.path_valid && out.path_token == previous.path_token &&
        out.path_point_count == previous.path_point_count) {
        for (unsigned i = 0; i < out.path_point_count; ++i)
            out.path_points[i] = interpolate(previous.path_points[i], out.path_points[i]);
    }
    const JPH::Quat a(float(previous.fitting_rotation.x), float(previous.fitting_rotation.y),
        float(previous.fitting_rotation.z), float(previous.fitting_rotation.w));
    const JPH::Quat b(float(out.fitting_rotation.x), float(out.fitting_rotation.y),
        float(out.fitting_rotation.z), float(out.fitting_rotation.w));
    out.fitting_rotation = to_quaternion(a.SLERP(b, float(alpha)));
    return out;
}
bool Simulation::PhysicsWorld::traversal_line_gripping() const noexcept {
    return traversal_line_ && traversal_line_->grip_active();
}
void Simulation::PhysicsWorld::apply_traversal_line_commands(const StepCommands &commands, float dt) {
    if (!traversal_line_) return;
    const auto finish_release = [&] {
        traversal_line_->release();
        if (traversal_state_ == TraversalState::LineSwing) {
            const auto velocity = physics_system_.GetBodyInterface().GetLinearVelocity(player_id_);
            airborne_inherited_velocity_ = JPH::Vec3::sZero();
            air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                JPH::Vec3(velocity.GetX(), 0, velocity.GetZ()).Length());
            causal_airborne_ = true;
            traversal_state_ = TraversalState::None; traversal_body_ = JPH::BodyID();
            traversal_entity_id_ = 0; traversal_line_lean_ = JPH::Vec3::sZero();
        }
    };
    if (traversal_state_ == TraversalState::LineSwing && !traversal_line_gripping()) finish_release();
    if (traversal_line_gripping() && (commands.release_requested || commands.jump_requested)) {
        finish_release(); automatic_grip_cancelled_ = true;
    }
    bool machine = commands.yard_crane_attach || commands.yard_crane_detach ||
        commands.forklift_tool_request != ForkliftToolRequest::None;
    for (unsigned i = 0; i < 4; ++i) machine |= commands.forklift_input[i] != 0;
    for (double value : commands.yard_crane_input) machine |= value != 0;
    const auto allowed = [&] {
        return (traversal_state_ == TraversalState::None ||
            (traversal_state_ == TraversalState::LineSwing && traversal_line_gripping())) &&
            (!physical_hands_->active() || traversal_line_gripping()) && !palm_props_active() &&
            !carry_constraint_ && !(slingshot_ && slingshot_->controls_player()) &&
            !machine && !commands.pick_up_requested;
    };
    const auto refresh = [&] {
        const auto hand = physics_system_.GetBodyInterface().GetPosition(player_id_) +
            JPH::Vec3(0, crouched_ ? .35F : .6F, 0);
        traversal_line_->aim(hand, JPH::Vec3(float(commands.rigging_aim.x),
            float(commands.rigging_aim.y), float(commands.rigging_aim.z)), allowed());
    };
    refresh();
    for (std::size_t i = 0; i < commands.traversal_line_command_count; ++i) {
        const auto command = commands.traversal_line_commands[i];
        switch (command.kind) {
        case TraversalLineCommandKind::Equip:
            if (allowed()) {
                if (grappling_gun_) grappling_gun_->release();
                grapple_equipped_ = false; clear_rigging_input();
                traversal_line_->equip(true);
            }
            break;
        case TraversalLineCommandKind::Unequip:
            finish_release(); traversal_line_->equip(false); break;
        case TraversalLineCommandKind::Release: finish_release(); break;
        case TraversalLineCommandKind::Recover:
            if (traversal_line_->recover()) finish_release();
            break;
        case TraversalLineCommandKind::Action: {
            if (commands.release_requested) break;
            if (!traversal_line_->readback().installed && !traversal_line_->install(command.target)) break;
            const bool held = traversal_line_gripping();
            if (traversal_line_->grab()) {
                traversal_state_ = TraversalState::LineSwing;
                const auto readback = traversal_line_->readback();
                traversal_body_ = readback.fitting; traversal_entity_id_ = 0;
                traversal_normal_ = JPH::Vec3(float(commands.rigging_aim.x), 0,
                    float(commands.rigging_aim.z)).NormalizedOr(-JPH::Vec3::sAxisZ());
                if (!held) traversal_line_lean_ = JPH::Vec3::sZero();
                ordinary_jump_ticks_left_ = air_catch_action_ticks_left_ = 0;
                wall_stride_available_ = false; physical_step_pending_ = false;
                physical_foot_push_->clear();
            }
            break;
        }
        }
        refresh();
    }
    traversal_line_->set_reel_input(float(commands.traversal_line_reel));
    if (traversal_line_gripping()) {
        // The movement stick commands a bounded human lean through the current
        // hand owner (500W/0.6m/s swing profile), never a capsule velocity.
        // 0.32m is a posture tuning bound, not a source of support or energy.
        auto input = JPH::Vec3(float(commands.move_input_x), 0, float(commands.move_input_z));
        if (input.LengthSq() > 1) input = input.Normalized();
        const auto delivered = physical_hands_->advance_targets(input * .32F - traversal_line_lean_, dt, 500.F);
        traversal_line_lean_ += delivered;
        physics_system_.GetBodyInterface().ActivateBody(player_id_);
    }
}
TraversalLineSnapshot Simulation::PhysicsWorld::traversal_line_state() const noexcept {
    TraversalLineSnapshot out;
    if (!traversal_line_) return out;
    const auto r = traversal_line_->readback();
    out.available = true; out.equipped = r.equipped; out.installed = r.installed;
    out.broken = r.broken; out.gripping = r.gripping; out.target_valid = r.target_valid;
    out.can_grab = r.can_grab; out.can_recover = r.can_recover;
    out.available_spools = r.available_spools; out.failure = unsigned(r.failure);
    out.target = r.target; out.generation = r.generation; out.event_serial = r.event_serial;
    out.support_body = r.support.GetIndexAndSequenceNumber(); out.fitting_body = r.fitting.GetIndexAndSequenceNumber();
    out.target_point = to_vector3(r.target_point); out.anchor_point = to_vector3(r.anchor_point);
    out.grip_point = to_vector3(r.grip_point);
    if (r.installed) out.fitting_rotation = to_quaternion(physics_system_.GetBodyInterface().GetRotation(r.fitting));
    out.paid_length_m = r.paid_length_m; out.tension_n = r.tension_n;
    out.path_valid = r.path_valid; out.path_reason = r.path_reason;
    out.path_length_m = r.path_length_m; out.path_token = r.path_token;
    out.contact_count = r.contact_count; out.path_point_count = r.path_point_count;
    for (unsigned i = 0; i < r.path_point_count; ++i) out.path_points[i] = to_vector3(r.path_points[i]);
    out.reel_work_j = r.reel_work_j; out.spool_absorbed_j = r.spool_absorbed_j;
    out.recovery_absorbed_j = r.recovery_absorbed_j;
    return out;
}
} // namespace scraperx::sim
