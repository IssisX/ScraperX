// The player's strike (the Action button's "punch"): wind-up, a short reach
// in which the fist can land on her, then the recoil. A landing is a force
// on her body at the fist, held over kStrikeContactTicks and drawn from his
// strike limits (player_strike_limits.hpp), with the equal force back on
// him. Nothing here writes a velocity or a position.
#include "physics_world.hpp"
#include "player_strike_limits.hpp"

#include "forklift.hpp"
#include "slingshot.hpp"
#include "swing.hpp"
#include "trapeze.hpp"
#include "yard_crane.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {

namespace {
constexpr int kStrikeWindupTicks = 11;    // ~0.12 s drawn back
constexpr int kStrikeReachTicks = 6;      // ~0.07 s the fist is out
constexpr int kStrikeRecoilTicks = 16;    // ~0.18 s back to guard
constexpr int kStrikeContactTicks = 3;    // ~1/30 s the force is held on a landing
constexpr float kStrikeChestUpM = 0.25F;  // over his body's center
constexpr float kStrikeReachM = 0.75F;    // shoulder to fist, the arm out
constexpr float kStrikeFistRadiusM = 0.06F;
constexpr float kStrikeOfferM = 1.6F;     // she is close enough for Action to say punch
constexpr float kStrikeOfferCos = 0.5F;   // within 60 degrees of his facing
constexpr float kStrikeMinClosingMps = 0.5F;

// The nearest point on her upright capsule's axis to `p`.
JPH::RVec3 axis_point(const JPH::RVec3 center, const float half, const JPH::RVec3 p) noexcept {
    const double y = std::clamp(p.GetY(), center.GetY() - half, center.GetY() + half);
    return JPH::RVec3(center.GetX(), y, center.GetZ());
}
double c_feet(const JPH::BodyInterface &bodies, const JPH::BodyID her) noexcept {
    return bodies.GetPosition(her).GetY() - Matron::kHalfHeight - Matron::kRadius;
}
} // namespace

// Nothing between his chest and where the fist is: the fist itself (a
// sphere of kStrikeFistRadiusM) swept along its way, so a rail, a bar or a
// wall in the way takes it first, however thin. Her body and her hand are
// what it is thrown at.
bool Simulation::PhysicsWorld::fist_path_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept {
    const JPH::Vec3 span(to - from);
    if (span.LengthSq() < 1.0e-6F) {
        return true;
    }
    JPH::SphereShape fist(kStrikeFistRadiusM);
    fist.SetEmbedded();
    const JPH::RShapeCast cast(&fist, JPH::Vec3::sReplicate(1.0F), JPH::RMat44::sTranslation(from), span);
    JPH::ShapeCastSettings settings;
    JPH::AnyHitCollisionCollector<JPH::CastShapeCollector> hit;
    JPH::IgnoreMultipleBodiesFilter ignore;
    ignore.Reserve(3);
    ignore.IgnoreBody(player_id_);
    ignore.IgnoreBody(matron_->body());
    if (!hand_id_.IsInvalid()) {
        ignore.IgnoreBody(hand_id_);
    }
    physics_system_.GetNarrowPhaseQuery().CastShape(cast, settings, from, hit, {}, {}, ignore);
    return !hit.HadHit();
}

// The fist he can throw with: the right, else the left; none when each is
// torn off or broken (a broken arm is out of use, held across the belly).
bool Simulation::PhysicsWorld::fist_usable(const int side) const noexcept {
    return side >= 0 && !arm_lost_[static_cast<std::size_t>(side & 1)] && !(arm_broken() && broken_arm_side_ == side);
}

int Simulation::PhysicsWorld::strike_arm() const noexcept {
    return fist_usable(1) ? 1 : (fist_usable(0) ? 0 : -1);
}

// A punch thrown now along `line` from his chest would land on her (`c` her
// body's middle): the reach as strike_step sweeps it, the fist within her
// capsule and its way clear of steel. What the offer of a free punch asks.
bool Simulation::PhysicsWorld::strike_would_land(const JPH::RVec3 chest, const JPH::Vec3 line,
                                                 const JPH::RVec3 c) const noexcept {
    for (int t = 0; t < 3; ++t) {
        const JPH::RVec3 fist = chest + line * (kStrikeReachM * static_cast<float>(t + 1) / 3.0F);
        if (JPH::Vec3(fist - axis_point(c, Matron::kHalfHeight, fist)).Length() <= Matron::kRadius + kStrikeFistRadiusM) {
            return fist_path_clear(chest, fist);
        }
    }
    return false;
}

// His hands on a machine's controls, its seat or its line: no punch from there.
bool Simulation::PhysicsWorld::hands_on_a_machine() const noexcept {
    for (const auto &truck : forklifts_) {
        if (truck->driving()) {
            return true;
        }
    }
    return yard_crane_->operating() || slingshot_->controls_player() || swing_->controls_player() ||
           trapeze_->controls_player() || hook_state_ != HookState::Stowed;
}

// A punch under way owns his hands till its recoil ends: no hand action of
// any other kind starts (a climb, a vault or a hang, a pick-up, a rig, a
// machine's control, the hook), nor the jump that would put them on one.
void Simulation::PhysicsWorld::strike_owns_hands(StepCommands &commands) const noexcept {
    if (strike_phase_ == StrikePhase::Guard) {
        return;
    }
    commands.jump_requested = false;
    commands.traversal_requested = false;
    commands.up_requested = false;
    commands.pick_up_requested = false;
    commands.set_down_requested = false;
    commands.throw_requested = false;
    commands.rig_requested = false;
    commands.sling_action = false;
    commands.swing_action = false;
    commands.trapeze_action = false;
    commands.lift_action = false;
    commands.lift_haul = false;
    commands.forklift_action = false;
    commands.yard_crane_take = false;
    commands.yard_crane_operate = false;
    commands.yard_crane_device = false;
    commands.hook_fire = false;
    commands.hook_take = false;
}

// The world took his hands from the punch (her fingers on him, a fall, a
// catch): it ends where it is, undrawn, and lands nothing more. Its counts
// and what it already put into her stay.
void Simulation::PhysicsWorld::strike_cancel() noexcept {
    strike_phase_ = StrikePhase::Guard;
    strike_ticks_ = 0;
    strike_contact_left_ = 0;
    strike_in_hand_ = false;
}

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
    // His chest along his body's up: over his middle on his feet, under it
    // hung head down in her hand.
    const JPH::RVec3 chest = at + bodies.GetRotation(player_id_) * JPH::Vec3(0.0F, kStrikeChestUpM, 0.0F);
    // His hands his own for a punch: not in her fingers or tumbling, not on
    // a climb, a load, a machine or the hook, not rolling out of a landing,
    // and a fist to throw it with.
    // A free punch is thrown from his feet on support (not in a flight, a
    // lunge or a wall run, whose presses are their own), the haul line out
    // of his hands.
    const int arm = strike_arm();
    const bool free = grip_ == nullptr && !player_free_rot_ && !matron_->claimed() &&
                      traversal_state_ == TraversalState::None && carried_entity_ == 0 && !rolling() &&
                      !hands_on_a_machine() && !commands.lift_haul && grounded_ && !dyno_active_ && arm >= 0;
    // In her hand before the rip begins his fist is his escape (the owner's
    // design: his own blow frees him then, Matron::State); during her reach,
    // before her fingers close, the brace has Action.
    const bool in_her_hand = her_here && grip_ != nullptr && arm >= 0 && matron_->his_blow_can_free();
    // Under way, the world can take his hands from it: begun free, her
    // fingers on him, a fall or a catch; begun in her hand, out of it. It
    // ends there, so it never lands undrawn.
    if (strike_phase_ != StrikePhase::Guard &&
        (!(strike_in_hand_ ? her_here && grip_ != nullptr : free) || !fist_usable(strike_side_))) {
        strike_cancel();
    }
    const bool can_start = strike_phase_ == StrikePhase::Guard &&
                           ((free && !(her_here && (matron_->holding() || matron_->reaching()))) || in_her_hand);
    strike_ready_ = can_start;
    // Whether Action offers the punch: one can start, and thrown now along
    // his facing it would land on her, on her feet (down, her body's capsule
    // stands over where she lies: nothing there to hit); or she has him in
    // her hand.
    strike_offered_ = false;
    if (can_start && in_her_hand) {
        strike_offered_ = true;
    } else if (can_start && her_here && !matron_->down()) {
        const JPH::RVec3 c = bodies.GetPosition(her);
        JPH::Vec3 to(static_cast<float>(c.GetX() - at.GetX()), 0.0F, static_cast<float>(c.GetZ() - at.GetZ()));
        const float d = to.Length();
        strike_offered_ = d < kStrikeOfferM && d > 1.0e-3F && to.Dot(facing) > kStrikeOfferCos * d &&
                          std::abs(static_cast<float>(c.GetY() - at.GetY())) < 1.5F && strike_would_land(chest, facing, c);
    }
    if (commands.strike_requested && can_start) {
        strike_phase_ = StrikePhase::Windup;
        strike_ticks_ = 0;
        strike_landed_ = false;
        strike_contact_left_ = 0;
        strike_in_hand_ = in_her_hand;
        strike_side_ = arm;   // the fist it is thrown with, till its recoil ends
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
        if (!strike_landed_ && her_here && !matron_->down()) {
            const JPH::RVec3 c = bodies.GetPosition(her);
            // Hung in her hand his arm swings at her (the view turns with his
            // body there, not with his facing); on his feet, along his facing.
            JPH::Vec3 line = facing;
            if (strike_in_hand_) {
                const JPH::Vec3 to(axis_point(c, Matron::kHalfHeight, chest) - chest);
                line = to.LengthSq() > 1.0e-6F ? to.Normalized() : facing;
            }
            const float out = kStrikeReachM * std::min(1.0F, static_cast<float>(strike_ticks_ + 1) / 3.0F);
            const JPH::RVec3 fist = chest + line * out;
            const JPH::RVec3 q = axis_point(c, Matron::kHalfHeight, fist);
            const JPH::Vec3 off(fist - q);
            if (off.Length() <= Matron::kRadius + kStrikeFistRadiusM && fist_path_clear(chest, fist)) {
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
        // Read by her balance as a blow (matron.cpp take_blow).
        const bool low = static_cast<float>(strike_point_.GetY() - c_feet(bodies, her)) < 0.5F;
        matron_->struck_by_player(dir * (force_n * dt), strike_point_, low);
        --strike_contact_left_;
    }
    if (strike_phase_ == StrikePhase::Recoil && strike_ticks_ >= kStrikeRecoilTicks) {
        strike_phase_ = StrikePhase::Guard;
        strike_ticks_ = 0;
    }
}

void Simulation::PhysicsWorld::strike_reset() noexcept {
    strike_phase_ = StrikePhase::Guard;
    strike_ticks_ = 0;
    strike_contact_left_ = 0;
    strike_landed_ = false;
    strike_offered_ = false;
    strike_ready_ = false;
    strike_in_hand_ = false;
    strike_point_ = JPH::RVec3::sZero();
    strike_normal_ = JPH::Vec3::sZero();
    strike_impulse_ns_ = 0.0F;
    matron_->clear_player_blow();
}

void Simulation::PhysicsWorld::read_strike_state() noexcept {
    state_.strike_phase = static_cast<std::uint8_t>(strike_phase_);
    state_.strike_phase_ticks = static_cast<std::uint32_t>(strike_ticks_);
    state_.strike_offered = strike_offered_;
    state_.strike_ready = strike_ready_;
    state_.strike_arm = static_cast<std::int8_t>(strike_phase_ != StrikePhase::Guard ? strike_side_ : strike_arm());
    state_.strike_landed = strike_landed_;
    state_.strike_count = strike_count_;
    state_.strike_hit_count = strike_hit_count_;
    state_.strike_hit_point = Vector3{strike_point_.GetX(), strike_point_.GetY(), strike_point_.GetZ()};
    state_.strike_hit_normal = Vector3{strike_normal_.GetX(), strike_normal_.GetY(), strike_normal_.GetZ()};
    state_.strike_impulse_ns = strike_impulse_ns_;
}

} // namespace scraperx::sim
