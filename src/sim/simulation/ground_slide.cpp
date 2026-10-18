#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

void Simulation::PhysicsWorld::update_ground_slide(const JPH::BodyInterface &bodies,
                                                   const StepCommands &commands) noexcept {
    // Releasing under a low ceiling keeps the existing compact slide until
    // the ordinary stand-clearance owner can actually stand the capsule.
    const bool requested = commands.crouch_held || (ground_sliding_ && crouched_);
    if (!requested) {
        ground_sliding_ = false;
        ground_slide_armed_ = true;
        return;
    }
    const bool cancelled = commands.release_requested || commands.cancel_jump_requested ||
        commands.set_down_requested || commands.sling_drop ||
        traversal_state_ != TraversalState::None || carry_constraint_ != nullptr ||
        physical_hands_->active() || (slingshot_ && slingshot_->controls_player()) ||
        (landing_recovery_seconds_ > 0 && landing_response_ != 0);
    if (cancelled) {
        ground_sliding_ = false;
        ground_slide_armed_ = false;
        return;
    }
    const auto support = support_sample_.body_id;
    if (!grounded_ || !crouched_ || support.IsInvalid() || net_for_body(support)) {
        if (ground_sliding_) ground_slide_armed_ = false;
        ground_sliding_ = false;
        return;
    }
    const JPH::RVec3 point(support_sample_.contact_point.x,
        support_sample_.contact_point.y, support_sample_.contact_point.z);
    const auto normal = support_traction_normal();
    const auto relative = bodies.GetLinearVelocity(player_id_) -
        support_point_velocity(bodies, support, point);
    const auto tangent = relative - normal * relative.Dot(normal);
    const float speed = tangent.Length();
    if (ground_sliding_) {
        // Hand off at ordinary crouch speed rather than clamping a fast
        // body down to that speed when the player first lowers their stance.
        if (speed <= kPlayerMaximumRelativeSpeed * float(kCrouchSpeedScale))
            ground_sliding_ = false;
        return;
    }
    const float forward = tangent.Dot(facing_);
    if (ground_slide_armed_ && speed >= .9F * kPlayerMaximumRelativeSpeed &&
        forward >= speed * float(kSprintMaximumAngleCos)) {
        ground_sliding_ = true;
        ground_slide_armed_ = false;
        ++ground_slide_count_;
    }
}

void Simulation::PhysicsWorld::apply_ground_slide(JPH::BodyInterface &bodies,
                                                  const StepCommands &commands,
                                                  const float dt) noexcept {
    const auto normal = support_traction_normal();
    const auto support_velocity = current_support_point_velocity(bodies);
    const auto relative = bodies.GetLinearVelocity(player_id_) - support_velocity;
    const auto tangent = relative - normal * relative.Dot(normal);
    const float speed = tangent.Length();
    if (speed <= 0 || dt <= 0) return;
    const auto direction = tangent / speed;
    auto intent = JPH::Vec3(float(commands.move_input_x), 0,
                            float(commands.move_input_z));
    intent -= normal * intent.Dot(normal);
    const float along = intent.Dot(direction);
    const auto lateral = intent - direction * along;
    // Reduced-order compact ground travel: the existing contact actuator
    // supplies drag, deliberate braking and steering. It grants no speed.
    // These posture requests stay inside its unchanged traction/3 kW caps.
    // 0.16 is a slide-drag tuning hypothesis, below the existing traction cap.
    const float normal_gravity = std::max(0.0F,
        -physics_system_.GetGravity().Dot(normal));
    const float drag_acceleration = .16F * normal_gravity;
    const float brake_acceleration = std::max(0.0F, -along) * kGroundAcceleration;
    const float deceleration = std::min(speed / dt,
        drag_acceleration + brake_acceleration);
    const auto force = (-direction * deceleration +
        lateral * (.15F * kGroundAcceleration)) * kPlayerMassKg;
    // Gravity along a slope remains ordinary gravity during a slide.
    apply_ground_contact_force(bodies, force, dt, false);
    airborne_inherited_velocity_ = support_velocity;
    air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed, speed);
}

} // namespace scraperx::sim
