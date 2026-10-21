#include "sim/physics_world.hpp"
#include <Jolt/Physics/Body/MotionProperties.h>

namespace scraperx::sim {
namespace {
// Tunable actuator budgets, not measurements of a person.
constexpr float kGripN = 3800.0F;
constexpr float kEscapeN = 350.0F;
constexpr float kRecoveryNm = 650.0F;
JPH::Vec3 limited(JPH::Vec3 v, float cap) {
    const float length = v.Length();
    return length > cap ? v * (cap / length) : v;
}
}

void Simulation::PhysicsWorld::player_rotation_dofs(bool free) noexcept {
    JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
    if (!lock.Succeeded()) return;
    auto &body = lock.GetBody();
    auto mass = body.GetShape()->GetMassProperties();
    mass.ScaleToMass(kPlayerMassKg);
    body.GetMotionProperties()->SetMassProperties(free ? JPH::EAllowedDOFs::All :
        JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ, mass);
    player_rotation_free_ = free;
}

void Simulation::PhysicsWorld::start_capture(JPH::BodyInterface &bodies) noexcept {
    if (!matron_->can_hold_player()) return;
    // Keep the current capsule, including crouch, position and both velocities.
    capture_ankle_ = JPH::Vec3(0, -(crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight) + 0.12F, 0);
    const auto ankle = bodies.GetPosition(player_id_) + bodies.GetRotation(player_id_) * capture_ankle_;
    capture_hand_ = JPH::Vec3(ankle - bodies.GetPosition(matron_->body()));
    if (capture_hand_.Length() > 2.0F) return; // arm cannot reach it
    capture_side_ = JPH::Vec3(capture_hand_.GetX(), 0, capture_hand_.GetZ()).NormalizedOr(JPH::Vec3::sAxisX());
    release_carry();
    if (hook_state_ != HookState::Stowed) stow_hook(bodies);
    matron_->drop_stock();
    player_rotation_dofs(true);
    bodies.SetMotionQuality(player_id_, JPH::EMotionQuality::LinearCast);
    bodies.SetGravityFactor(player_id_, 1.0F);
    bodies.SetFriction(player_id_, 0.6F);
    bodies.SetFriction(matron_->body(), 0.9F);
    parachute_deployed_ = false;
    teetering_ = line_held_ = sliding_ = false;
    jump_buffer_ticks_ = coyote_ticks_ = 0;
    capture_seconds_ = capture_escape_ = capture_overload_ = 0;
    capture_phase_ = 1;
    matron_->set_capturing(true);
    matron_->count_catch();
}

void Simulation::PhysicsWorld::release_capture(JPH::BodyInterface &bodies) noexcept {
    if (capture_phase_ != 1 && capture_phase_ != 2) return;
    capture_phase_ = 3;
    capture_force_ = 0;
    capture_cooldown_ = 4.0F;
    ++capture_releases_;
    matron_->set_capturing(false);
    bodies.SetFriction(matron_->body(), 0.0F);
    // No position, orientation or velocity write: gravity and contacts own the fall.
}

void Simulation::PhysicsWorld::reset_capture(JPH::BodyInterface &bodies) noexcept {
    release_capture(bodies);
    capture_phase_ = 0;
    capture_force_ = capture_torque_ = capture_escape_ = capture_seconds_ = 0;
    capture_cooldown_ = 4.0F;
    matron_->set_capturing(false);
    if (player_rotation_free_) player_rotation_dofs(false);
    bodies.SetFriction(player_id_, 0.0F);
}

void Simulation::PhysicsWorld::step_capture(JPH::BodyInterface &bodies, const StepCommands &input, float dt) noexcept {
    capture_cooldown_ = std::max(0.0F, capture_cooldown_ - dt);
    capture_force_ = capture_torque_ = 0;
    if (capture_phase_ == 1 || capture_phase_ == 2) {
        capture_seconds_ += dt;
        if (!matron_->can_hold_player() || capture_seconds_ > 9.0F) {
            release_capture(bodies);
        } else {
            const auto other = matron_->body();
            const auto rotation = bodies.GetRotation(player_id_);
            const auto ankle = bodies.GetPosition(player_id_) + rotation * capture_ankle_;
            // Reachable, finite-speed hand path. The body must swing below it
            // under gravity; no orientation target or invisible safe landing.
            const auto target = capture_side_ * 1.05F + JPH::Vec3(0, 1.35F, 0);
            const auto travel = limited(target - capture_hand_, 0.8F * dt);
            capture_hand_ += travel;
            const auto hand = bodies.GetPosition(other) + capture_hand_;
            const auto error = JPH::Vec3(hand - ankle);
            if (error.Length() > 1.0F) {
                release_capture(bodies);
            } else {
                const auto relative = bodies.GetPointVelocity(other, hand) + travel / dt - bodies.GetPointVelocity(player_id_, ankle);
                auto effort = JPH::Vec3(float(input.move_input_x), 0, float(input.move_input_z));
                effort = limited(effort, 1.0F);
                const float strength = effort.Length();
                capture_escape_ = std::clamp(capture_escape_ + (strength > 0.15F ? strength * dt / 2.5F : -dt * 0.12F), 0.0F, 1.0F);
                // A single total force budget includes escape. Both bodies get
                // the same opposite force. Her existing upright capsule takes
                // the reaction through its translation and floor friction.
                const auto demand = error * 2400.0F + relative * 240.0F + effort * kEscapeN;
                const auto force = limited(demand, kGripN);
                capture_force_ = force.Length();
                capture_overload_ = demand.Length() > kGripN ? capture_overload_ + dt : 0.0F;
                if (capture_escape_ >= 1.0F || capture_overload_ > 0.3F) {
                    release_capture(bodies);
                } else {
                    bodies.AddForce(player_id_, force, ankle);
                    bodies.AddForce(other, -force, ankle);
                    if ((rotation * JPH::Vec3::sAxisY()).GetY() < -0.75F) capture_phase_ = 2;
                }
            }
        }
    }
    if (player_rotation_free_ && capture_phase_ != 1 && capture_phase_ != 2) {
        // Return the whole physical frame, including yaw, so the normal
        // view-relative input frame agrees after an oblique escape.
        JPH::Vec3 axis;
        float angle;
        bodies.GetRotation(player_id_).Conjugated().EnsureWPositive().GetAxisAngle(axis, angle);
        const auto angular = bodies.GetAngularVelocity(player_id_);
        const auto torque = limited(axis * (angle * 1400.0F) - angular * 200.0F, kRecoveryNm);
        capture_torque_ = torque.Length();
        bodies.AddTorque(player_id_, torque);
        // Recovered (within 1.4 degrees, turning under 0.08 rad/s, on its
        // feet): the rotation locked again, upright. Every other system acts
        // off the body's centre on a body that cannot turn: a hang's arms at
        // the shoulders, a carry 0.6 m out at the hands, a harness. Left free
        // under the servo, a 36 kg carry alone would hold the view about 9
        // degrees over (212 N m against 1400 N m per radian). The last 1.4
        // degrees and the residual turn go with the lock.
        if (capture_phase_ == 3 && grounded_ && angle < 0.025F && angular.Length() < 0.08F) {
            capture_phase_ = 0;
            capture_cooldown_ = 4.0F; // recovery time must not consume the escape window
            bodies.SetFriction(player_id_, 0.0F);
            regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
            bodies.SetRotation(player_id_, JPH::Quat::sIdentity(), JPH::EActivation::Activate);
            bodies.SetAngularVelocity(player_id_, JPH::Vec3::sZero());
            player_rotation_dofs(false);
        }
    }
}
} // namespace scraperx::sim
