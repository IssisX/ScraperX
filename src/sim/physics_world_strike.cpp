// The player's strike (the Action button's "punch"): wind-up, a short reach
// in which the fist can land on her, then the recoil. A landing is a force
// on her body at the fist, held over kStrikeContactTicks and drawn from his
// strike limits (player_strike_limits.hpp), with the equal force back on
// him. Nothing here writes a velocity or a position.
#include "physics_world.hpp"
#include "player_strike_limits.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {

namespace {
constexpr int kStrikeWindupTicks = 11;    // ~0.12 s drawn back
constexpr int kStrikeReachTicks = 6;      // ~0.07 s the fist is out
constexpr int kStrikeRecoilTicks = 16;    // ~0.18 s back to guard
constexpr int kStrikeContactTicks = 3;    // ~1/30 s the force is held on a landing
constexpr float kStrikeChestUpM = 0.25F;  // over his body's centre
constexpr float kStrikeReachM = 0.75F;    // shoulder to fist, the arm out
constexpr float kStrikeFistRadiusM = 0.06F;
constexpr float kStrikeOfferM = 1.6F;     // she is close enough for Action to say punch
constexpr float kStrikeOfferCos = 0.5F;   // within 60 degrees of his facing
constexpr float kStrikeMinClosingMps = 0.5F;

// The nearest point on her upright capsule's axis to `p`.
JPH::RVec3 axis_point(const JPH::RVec3 centre, const float half, const JPH::RVec3 p) noexcept {
    const double y = std::clamp(p.GetY(), centre.GetY() - half, centre.GetY() + half);
    return JPH::RVec3(centre.GetX(), y, centre.GetZ());
}
} // namespace

void Simulation::PhysicsWorld::strike_step(JPH::BodyInterface &bodies, const StepCommands &commands,
                                           const float dt) noexcept {
    const double fl = std::hypot(commands.facing_x, commands.facing_z);
    const JPH::Vec3 facing = fl > 1.0e-6
                                 ? JPH::Vec3(static_cast<float>(commands.facing_x / fl), 0.0F,
                                             static_cast<float>(commands.facing_z / fl))
                                 : JPH::Vec3(0.0F, 0.0F, 1.0F);
    const JPH::BodyID her = matron_->body();
    const bool her_here = !her.IsInvalid() && bodies.IsAdded(her);
    const JPH::RVec3 at = bodies.GetPosition(player_id_);
    const JPH::RVec3 chest = at + JPH::Vec3(0.0F, kStrikeChestUpM, 0.0F);
    // Whether Action offers the punch: her body close in front.
    strike_offered_ = false;
    if (her_here) {
        const JPH::RVec3 c = bodies.GetPosition(her);
        JPH::Vec3 to(static_cast<float>(c.GetX() - at.GetX()), 0.0F, static_cast<float>(c.GetZ() - at.GetZ()));
        const float d = to.Length();
        strike_offered_ = d < kStrikeOfferM && d > 1.0e-3F && to.Dot(facing) > kStrikeOfferCos * d &&
                          std::abs(static_cast<float>(c.GetY() - at.GetY())) < 1.5F;
    }
    const bool free = grip_ == nullptr && !player_free_rot_ && !matron_->claimed() &&
                      traversal_state_ == TraversalState::None;
    if (commands.strike_requested && strike_phase_ == StrikePhase::Guard && free) {
        strike_phase_ = StrikePhase::Windup;
        strike_ticks_ = 0;
        strike_landed_ = false;
        strike_contact_left_ = 0;
        ++strike_count_;
    }
    if (strike_phase_ == StrikePhase::Guard) {
        return;
    }
    ++strike_ticks_;
    if (strike_phase_ == StrikePhase::Windup && strike_ticks_ >= kStrikeWindupTicks) {
        strike_phase_ = StrikePhase::Reach;
        strike_ticks_ = 0;
    }
    if (strike_phase_ == StrikePhase::Reach) {
        if (!strike_landed_ && her_here) {
            const float out = kStrikeReachM * std::min(1.0F, static_cast<float>(strike_ticks_ + 1) / 3.0F);
            const JPH::RVec3 fist = chest + facing * out;
            const JPH::RVec3 c = bodies.GetPosition(her);
            const JPH::RVec3 q = axis_point(c, Matron::kHalfHeight, fist);
            const JPH::Vec3 off(fist - q);
            if (off.Length() <= Matron::kRadius + kStrikeFistRadiusM) {
                strike_landed_ = true;
                strike_contact_left_ = kStrikeContactTicks;
                const float ol = off.Length();
                strike_normal_ = ol > 1.0e-4F ? off / ol : -facing;
                strike_point_ = q + strike_normal_ * Matron::kRadius;
                strike_impulse_ns_ = 0.0F;
                ++strike_hit_count_;
            }
        }
        if (strike_ticks_ >= kStrikeReachTicks) {
            strike_phase_ = StrikePhase::Recoil;
            strike_ticks_ = 0;
        }
    }
    if (strike_contact_left_ > 0 && her_here) {
        // Force times time: the same force on her at the fist and back on him.
        const JPH::Vec3 dir = -strike_normal_;
        const float closing = std::max(kStrikeMinClosingMps,
                                       (bodies.GetLinearVelocity(player_id_) - bodies.GetLinearVelocity(her)).Dot(dir));
        const float force_n = std::min(strike::kPlayerStrikeForceN, strike::kPlayerStrikePowerW / closing);
        bodies.AddForce(her, dir * force_n, strike_point_, JPH::EActivation::Activate);
        bodies.AddForce(player_id_, -dir * force_n, JPH::EActivation::Activate);
        strike_impulse_ns_ += force_n * dt;
        --strike_contact_left_;
    }
    if (strike_phase_ == StrikePhase::Recoil && strike_ticks_ >= kStrikeRecoilTicks) {
        strike_phase_ = StrikePhase::Guard;
        strike_ticks_ = 0;
    }
}

void Simulation::PhysicsWorld::read_strike_state() noexcept {
    state_.strike_phase = static_cast<std::uint8_t>(strike_phase_);
    state_.strike_phase_ticks = static_cast<std::uint32_t>(strike_ticks_);
    state_.strike_offered = strike_offered_;
    state_.strike_landed = strike_landed_;
    state_.strike_count = strike_count_;
    state_.strike_hit_count = strike_hit_count_;
    state_.strike_hit_point = Vector3{strike_point_.GetX(), strike_point_.GetY(), strike_point_.GetZ()};
    state_.strike_hit_normal = Vector3{strike_normal_.GetX(), strike_normal_.GetY(), strike_normal_.GetZ()};
    state_.strike_impulse_ns = strike_impulse_ns_;
}

} // namespace scraperx::sim
