// Water: the player's body in a pool (Kit::add_pool). Every tick the capsule
// is measured against each pool's box and its water's level; the submerged
// fraction of the body carries buoyancy and drag, and a body afloat swims and
// treads instead of walking. The pools' own walls are escaped by their rungs
// (the bands build them) or by the rim caught from a kick (try_begin_hang).
// Members of Simulation::PhysicsWorld (physics_world.hpp).

#include "sim/physics_world.hpp"

namespace scraperx::sim {

void Simulation::PhysicsWorld::update_water(JPH::BodyInterface &bodies, const float dt) noexcept {
    const bool was_in = in_water_;
    in_water_ = false;
    swimming_ = false;
    head_underwater_ = false;
    water_submersion_ = 0.0F;
    if (swim_kick_cooldown_ticks_ > 0) {
        --swim_kick_cooldown_ticks_;
    }
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const float feet = static_cast<float>(at.GetY()) - kPlayerHalfHeight;
    const float head = static_cast<float>(at.GetY()) + kPlayerHalfHeight;
    for (std::uint32_t index = 0; index < kit_->pool_count(); ++index) {
        const kit::PoolIndex pool{index};
        JPH::Vec3 lo;
        JPH::Vec3 hi;
        kit_->pool_box(pool, lo, hi);
        const float level = kit_->pool_level(pool);
        if (at.GetX() < lo.GetX() || at.GetX() > hi.GetX() || at.GetZ() < lo.GetZ() || at.GetZ() > hi.GetZ() ||
            feet >= level || head <= lo.GetY()) {
            continue;
        }
        const float depth = std::min(level, head) - std::max(feet, lo.GetY());
        const float fraction = std::clamp(depth / (2.0F * kPlayerHalfHeight), 0.0F, 1.0F);
        if (fraction > water_submersion_) {
            water_submersion_ = fraction;
            water_surface_y_ = level;
            water_pool_ = static_cast<int>(index);
            head_underwater_ = head < level;
        }
    }
    if (water_submersion_ < kWaterWetFraction) {
        water_submersion_ = 0.0F;
        if (was_in) {
            ++water_exit_count_;
        }
        return;
    }
    in_water_ = true;
    swimming_ = water_submersion_ >= kSwimFraction && !grounded_;
    const JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    if (!was_in) {
        ++water_entry_count_;
        water_entry_speed_mps_ = std::max(0.0F, -velocity.GetY());
    }
    // Buoyancy: the displaced water's weight, the body a little lighter than
    // the water it fills (air in the lungs, a treading kick): afloat at about
    // 1/kBodyBuoyancyRatio submerged, the face above the surface.
    const float buoyancy = water_submersion_ * kPlayerMassKg * kWaterGravity * kBodyBuoyancyRatio;
    // Drag: linear and quadratic in the velocity through water, on the
    // submerged fraction; finite, and never more than stops the body in a
    // tick (no reversal).
    const float speed = velocity.Length();
    JPH::Vec3 drag = JPH::Vec3::sZero();
    if (speed > 1.0e-4F) {
        const float magnitude = water_submersion_ * (kWaterLinearDragNsPerM * speed +
                                                     kWaterQuadraticDragNs2PerM2 * speed * speed);
        const float stop = kPlayerMassKg * speed / dt;
        drag = -velocity / speed * std::min(magnitude, stop);
    }
    bodies.AddForce(player_id_, JPH::Vec3(0.0F, buoyancy, 0.0F) + drag);
}

void Simulation::PhysicsWorld::swim_locomotion(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    const JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
    const JPH::Vec3 stick(static_cast<float>(commands.move_input_x), 0.0F, static_cast<float>(commands.move_input_z));
    const float amount = std::min(1.0F, stick.Length());
    const JPH::Vec3 wanted = amount > 1.0e-3F ? stick / stick.Length() * (amount * kSwimMaxMps) : JPH::Vec3::sZero();
    // The stroke holds the asked-for speed against the water's drag (what
    // update_water applies at it) and closes on it at kSwimResponse.
    const JPH::Vec3 flat(velocity.GetX(), 0.0F, velocity.GetZ());
    const float along = wanted.IsNearZero() ? 0.0F : std::max(0.0F, flat.Dot(wanted.Normalized()));
    const float held = water_submersion_ * (kWaterLinearDragNsPerM * along + kWaterQuadraticDragNs2PerM2 * along * along);
    JPH::Vec3 push = (wanted - flat) * (kPlayerMassKg * kSwimResponse) +
                     (wanted.IsNearZero() ? JPH::Vec3::sZero() : wanted.Normalized() * held);
    if (push.Length() > kSwimForceN) {
        push = push.Normalized() * kSwimForceN;
    }
    bodies.AddForce(player_id_, push);
    // A kick: Jump drives the body up, a stroke's worth of momentum, once
    // per kSwimKickTicks: from the surface the chest comes up and the hands
    // reach the rim (try_begin_hang takes it on the way back down).
    if (commands.jump_requested && swim_kick_cooldown_ticks_ == 0) {
        bodies.AddImpulse(player_id_, JPH::Vec3(0.0F, kPlayerMassKg * kSwimKickMps, 0.0F));
        swim_kick_cooldown_ticks_ = kSwimKickTicks;
        ++swim_kick_count_;
    }
}

} // namespace scraperx::sim
