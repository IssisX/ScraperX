// The player's strike (the Action button's "punch"): wind-up, a short reach
// in which the fist can land on her, then the recoil. A landing is a force
// on her body at the fist, held over kStrikeContactTicks and drawn from his
// strike limits (player_strike_limits.hpp), with the equal force back on
// him. Nothing here writes a velocity or a position.
#include "physics_world.hpp"
#include "matron_effort.hpp"
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
// The low sweep (move 1): slower to draw back and to recover, the same
// reach out, one more contact tick. Its gap is centre to centre minus
// both bodies' radii; its point is her ankle, 0.15 m over her feet.
constexpr int kSweepWindupTicks = 16;
constexpr int kSweepReachTicks = 6;
constexpr int kSweepRecoilTicks = 22;
constexpr int kSweepContactTicks = 4;
constexpr float kSweepGapM = 0.60F;
constexpr float kSweepAnkleUpM = 0.15F;
// The heavy kick (move 2): slowest, longest reach, one more contact
// tick than the sweep; its foot lands on her capsule like a fist.
constexpr int kKickWindupTicks = 18;
constexpr int kKickReachTicks = 8;
constexpr int kKickRecoilTicks = 26;
constexpr int kKickContactTicks = 5;
constexpr float kKickReachM = 1.10F;
constexpr float kKickFootRadiusM = 0.06F;
// The 1-2 (spec section 9.4): the jab is the quick left lead (4 contact
// ticks); the cross the rear right (5), thrown after the jab's recoil
// or on a press in the 6-tick grace after it.
constexpr int kJabWindupTicks = 7;
constexpr int kJabReachTicks = 4;
constexpr int kJabRecoilTicks = 10;
constexpr int kJabContactTicks = 4;
constexpr int kCrossWindupTicks = 10;
constexpr int kCrossReachTicks = 6;
constexpr int kCrossRecoilTicks = 16;
constexpr int kCrossContactTicks = 5;
constexpr int kComboGraceTicks = 6;
// How far her ankle may sit off level: a sweep's foot travels at his foot
// height, rising at most about a shin-height off the level ankle. Beyond
// that the ankle is out of the leg's arc, whatever the X/Z gap says.
constexpr float kSweepAnkleTolM = 0.35F;
// How far the sweeping foot skims over the ground: the cast starts this
// far over his soles, or its own sphere would sit in the deck he stands
// on and every sweep would read blocked.
constexpr float kSweepFootClearM = 0.08F;

int move_windup_ticks(const std::uint8_t move) noexcept {
    return move == 1 ? kSweepWindupTicks : (move == 2 ? kKickWindupTicks : (move == 3 ? kJabWindupTicks : (move == 4 ? kCrossWindupTicks : kStrikeWindupTicks)));
}
int move_reach_ticks(const std::uint8_t move) noexcept {
    return move == 1 ? kSweepReachTicks : (move == 2 ? kKickReachTicks : (move == 3 ? kJabReachTicks : (move == 4 ? kCrossReachTicks : kStrikeReachTicks)));
}
int move_recoil_ticks(const std::uint8_t move) noexcept {
    return move == 1 ? kSweepRecoilTicks : (move == 2 ? kKickRecoilTicks : (move == 3 ? kJabRecoilTicks : (move == 4 ? kCrossRecoilTicks : kStrikeRecoilTicks)));
}
int move_contact_ticks(const std::uint8_t move) noexcept {
    return move == 1 ? kSweepContactTicks : (move == 2 ? kKickContactTicks : (move == 3 ? kJabContactTicks : (move == 4 ? kCrossContactTicks : kStrikeContactTicks)));
}

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
    ignore.Reserve(8);
    ignore.IgnoreBody(player_id_);
    ignore.IgnoreBody(matron_->body());
    if (!hand_id_.IsInvalid()) {
        ignore.IgnoreBody(hand_id_);
    }
    // J-044c: the upper bodies are her body too — the fist is thrown at
    // them, so they must not block its path the way steel does.
    for (int i = 0; i < Matron::kUpperCount; ++i) {
        const JPH::BodyID seg = matron_->upper_body(i);
        if (!seg.IsInvalid()) {
            ignore.IgnoreBody(seg);
        }
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

// A sweep thrown now would land on her ankle (`c` her body's middle): the
// gap between the bodies' surfaces within its reach, her ankle within a
// leg's arc of his foot, and the way from his foot to her ankle clear of
// steel, so a low rail, a platform's side or a bar takes it first, as a
// post takes a punch. One helper for both the offer and the contact, so
// the two can never disagree about the geometry: the offer's height check
// alone never guarded the contact, because a press only needs can_start.
// His foot is his body's centre less its capsule's total half-height
// (crouched, the short capsule: the centre drops with it, so the soles
// stay where they are). The cast
// starts at his foot, not at her ankle height: a cast at her ankle height
// never meets the platform he stands beside, and never starts from where
// his foot really is.
bool Simulation::PhysicsWorld::sweep_land_point(const JPH::RVec3 at, const JPH::Vec3 facing,
                                                const JPH::RVec3 c, JPH::RVec3 *point,
                                                JPH::Vec3 *normal) const noexcept {
    JPH::Vec3 to(static_cast<float>(c.GetX() - at.GetX()), 0.0F, static_cast<float>(c.GetZ() - at.GetZ()));
    const float d = to.Length();
    if (d <= 1.0e-3F || to.Dot(facing) <= kStrikeOfferCos * d) {
        return false;
    }
    const JPH::Vec3 dir = to / d;
    if (d - Matron::kRadius - kPlayerRadius > kSweepGapM) {
        return false;
    }
    // His foot: his body's centre less its capsule's total half-height
    // (crouched, the short capsule: the centre drops with it, so the soles
    // stay where they are).
    const double foot_y = at.GetY() - active_half_height();
    const double ankle_y = c.GetY() - Matron::kHalfHeight - Matron::kRadius + kSweepAnkleUpM;
    if (std::abs((ankle_y - foot_y) - kSweepAnkleUpM) > kSweepAnkleTolM) {
        return false;
    }
    const JPH::RVec3 from(at.GetX() + dir.GetX() * kPlayerRadius, foot_y + kSweepFootClearM,
                          at.GetZ() + dir.GetZ() * kPlayerRadius);
    const JPH::RVec3 ankle(c.GetX() - dir.GetX() * Matron::kRadius, ankle_y,
                           c.GetZ() - dir.GetZ() * Matron::kRadius);
    if (!fist_path_clear(from, ankle)) {
        return false;
    }
    if (point != nullptr) {
        *point = ankle;
    }
    if (normal != nullptr) {
        *normal = -dir;
    }
    return true;
}

// A sweep thrown now would land on her ankle: what the offer of a sweep asks.
bool Simulation::PhysicsWorld::sweep_would_land(const JPH::RVec3 at, const JPH::Vec3 facing,
                                                const JPH::RVec3 c) const noexcept {
    return sweep_land_point(at, facing, c, nullptr, nullptr);
}

// A kick thrown now would land on her: the foot's reach as strike_step
// sweeps it, within her capsule, its way clear (the punch's path check).
bool Simulation::PhysicsWorld::kick_would_land(const JPH::RVec3 chest, const JPH::Vec3 facing,
                                               const JPH::RVec3 c) const noexcept {
    for (int t = 0; t < 3; ++t) {
        const JPH::RVec3 foot = chest + facing * (kKickReachM * static_cast<float>(t + 1) / 3.0F);
        if (JPH::Vec3(foot - axis_point(c, Matron::kHalfHeight, foot)).Length() <= Matron::kRadius + kKickFootRadiusM) {
            return fist_path_clear(chest, foot);
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
    strike_queued_move_ = 0;
    strike_grace_ = 0;
}

void Simulation::PhysicsWorld::strike_begin(const std::uint8_t move, const int side, const bool in_hand) noexcept {
    strike_move_ = move;
    strike_phase_ = StrikePhase::Windup;
    strike_ticks_ = 0;
    strike_landed_ = false;
    strike_contact_left_ = 0;
    strike_in_hand_ = in_hand;
    strike_side_ = side;
    ++strike_count_;
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
    // The move a press throws now: in her hand the punch (his escape);
    // crouched the low sweep; moving toward her the heavy kick; else the
    // jab, the left lead. Frozen at guard; moving reads the held input,
    // not velocity.
    bool moving_toward_her = false;
    if (her_here) {
        const JPH::RVec3 hc = bodies.GetPosition(her);
        const JPH::Vec3 to_her(static_cast<float>(hc.GetX() - at.GetX()), 0.0F,
                               static_cast<float>(hc.GetZ() - at.GetZ()));
        const float to_len = to_her.Length();
        const double input_len = std::hypot(commands.move_input_x, commands.move_input_z);
        if (to_len > 1.0e-3F && input_len > 1.0e-6) {
            const float along = static_cast<float>(commands.move_input_x) * to_her.GetX() +
                                static_cast<float>(commands.move_input_z) * to_her.GetZ();
            moving_toward_her = along > kStrikeOfferCos * static_cast<float>(input_len) * to_len;
        }
    }
    const std::uint8_t press_move = in_her_hand ? 0 : (crouched_ ? 1 : (moving_toward_her ? 2 : 3));
    // The move and fist a press at guard would throw: in the grace, the follow-up.
    std::uint8_t guard_move = press_move;
    int guard_side = arm;
    if (strike_grace_ > 0 && !in_her_hand) {
        guard_move = (strike_side_ == 0 && fist_usable(1)) ? 4 : 3;
        guard_side = guard_move == 4 ? 1 : strike_side_;
    } else if (press_move == 3) guard_side = fist_usable(0) ? 0 : arm;
    if (strike_phase_ == StrikePhase::Guard) {
        strike_move_ = guard_move;
    }
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
                          std::abs(static_cast<float>(c.GetY() - at.GetY())) < 1.5F &&
                          (strike_move_ == 1 ? sweep_would_land(at, facing, c) : strike_move_ == 2 ? kick_would_land(chest, facing, c)
                                           : strike_would_land(chest, facing, c));
    }
    if (commands.strike_requested && can_start) {
        strike_begin(guard_move, guard_side, in_her_hand);
    }
    // A press in the jab's recoil queues its follow-up (spec section 9.4):
    // the cross off a left lead, else the lead fist again as a jab.
    if (commands.strike_requested && strike_phase_ == StrikePhase::Recoil && strike_move_ == 3 &&
        strike_queued_move_ == 0) {
        strike_queued_move_ = (strike_side_ == 0 && fist_usable(1)) ? 4 : 3;
    }
    // The grace runs out a tick at a time at guard, after the press was read.
    if (strike_phase_ == StrikePhase::Guard && strike_grace_ > 0) --strike_grace_;
    if (strike_phase_ == StrikePhase::Guard) {
        return;
    }
    ++strike_ticks_;
    if (strike_phase_ == StrikePhase::Windup && strike_ticks_ >= move_windup_ticks(strike_move_)) {
        strike_phase_ = StrikePhase::Reach;
        strike_ticks_ = 0;
    }
    if (strike_phase_ == StrikePhase::Reach) {
        if (!strike_landed_ && her_here && !matron_->down()) {
            const JPH::RVec3 c = bodies.GetPosition(her);
            if (strike_move_ == 1) {
                // The sweep lands on her ankle: the same geometry the
                // offer asked, re-checked at contact (her ankle within
                // his foot's arc, the way from his foot clear).
                JPH::RVec3 point;
                JPH::Vec3 normal;
                if (sweep_land_point(at, facing, c, &point, &normal)) {
                    strike_landed_ = true;
                    strike_contact_left_ = move_contact_ticks(strike_move_);
                    strike_normal_ = normal;
                    strike_point_ = point;
                    strike_impulse_ns_ = 0.0F;
                    ++strike_hit_count_;
                }
            } else if (strike_move_ == 2) {
                // The kick lands on her capsule like a punch's fist, at
                // the foot's longer reach: the offer's geometry, re-checked.
                const float out = kKickReachM * std::min(1.0F, static_cast<float>(strike_ticks_ + 1) / 3.0F);
                const JPH::RVec3 foot = chest + facing * out;
                const JPH::RVec3 q = axis_point(c, Matron::kHalfHeight, foot);
                const JPH::Vec3 off(foot - q);
                if (off.Length() <= Matron::kRadius + kKickFootRadiusM && fist_path_clear(chest, foot)) {
                    strike_landed_ = true;
                    strike_contact_left_ = move_contact_ticks(strike_move_);
                    const float ol = off.Length();
                    strike_normal_ = ol > 1.0e-4F ? off / ol : -facing;
                    strike_point_ = q + strike_normal_ * Matron::kRadius;
                    strike_impulse_ns_ = 0.0F;
                    ++strike_hit_count_;
                }
            } else {
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
                    strike_contact_left_ = move_contact_ticks(strike_move_);
                    const float ol = off.Length();
                    strike_normal_ = ol > 1.0e-4F ? off / ol : -facing;
                    strike_point_ = q + strike_normal_ * Matron::kRadius;
                    strike_impulse_ns_ = 0.0F;
                    ++strike_hit_count_;
                }
            }
        }
        if (strike_ticks_ >= move_reach_ticks(strike_move_)) {
            strike_phase_ = StrikePhase::Recoil;
            strike_ticks_ = 0;
        }
    }
    if (strike_contact_left_ > 0 && her_here) {
        // Force times time: the same force on her at the fist and back on him.
        // The punch is an arm: its force against the bodies' closing speed,
        // capped by what an arm and trunk can deliver. The sweep and the
        // kick are legs driven through the ground: the spec's force flat.
        const JPH::Vec3 dir = -strike_normal_;
        const float closing = std::max(kStrikeMinClosingMps,
                                       (bodies.GetLinearVelocity(player_id_) - bodies.GetLinearVelocity(her)).Dot(dir));
        const float force_n = strike_move_ == 1   ? strike::kPlayerSweepForceN
                              : strike_move_ == 2 ? strike::kPlayerKickForceN
                                                  : std::min(strike::kPlayerStrikeForceN, strike::kPlayerStrikePowerW / closing);
        // J-044c: the hit lands on the upper segment it falls on (head,
        // neck, chest, arm), so the joint it hangs from takes the torque;
        // take_blow still reads the root, so stagger/knockdown is unchanged.
        const float feet_y = c_feet(bodies, her);
        const JPH::BodyID struck = matron_->strike_body_for_point(strike_point_, feet_y);
        bodies.AddForce(struck, dir * force_n, strike_point_, JPH::EActivation::Activate);
        bodies.AddForce(player_id_, -dir * force_n, JPH::EActivation::Activate);
        // J-046 measurement only: his blow, received (not her budget's). It
        // lands on the upper segment it falls on, and the residual read sums
        // the accumulated force over all six of her bodies (J-044c gains), so
        // the blow is recorded as on her wherever it lands; the joint
        // carrying it on does not make it not on her. Charged 0, Blow
        // unsummed: still not her budget's.
        effort::add_force(effort::Path::Blow, dir * force_n,
                          bodies.GetLinearVelocity(her) - bodies.GetLinearVelocity(player_id_), 0.0F,
                          dir * force_n);
        strike_impulse_ns_ += force_n * dt;
        // Read by her balance as a blow (matron.cpp take_blow).
        const bool low = static_cast<float>(strike_point_.GetY() - feet_y) < 0.5F;
        matron_->struck_by_player(dir * (force_n * dt), strike_point_, low);
        --strike_contact_left_;
    }
    if (strike_phase_ == StrikePhase::Recoil && strike_ticks_ >= move_recoil_ticks(strike_move_)) {
        const std::uint8_t queued = strike_queued_move_;
        const bool was_jab = strike_move_ == 3;
        strike_queued_move_ = 0;
        strike_grace_ = 0;
        strike_phase_ = StrikePhase::Guard;
        strike_ticks_ = 0;
        if (queued != 0) strike_begin(queued, queued == 4 ? 1 : strike_side_, false);  // the queued follow-up throws the tick after the recoil ends
        else if (was_jab) strike_grace_ = kComboGraceTicks;
    }
}

void Simulation::PhysicsWorld::strike_reset() noexcept {
    strike_phase_ = StrikePhase::Guard;
    strike_move_ = 0;
    strike_ticks_ = 0;
    strike_contact_left_ = 0;
    strike_landed_ = false;
    strike_offered_ = false;
    strike_ready_ = false;
    strike_in_hand_ = false;
    strike_point_ = JPH::RVec3::sZero();
    strike_normal_ = JPH::Vec3::sZero();
    strike_impulse_ns_ = 0.0F;
    strike_queued_move_ = 0;
    strike_grace_ = 0;
    matron_->clear_player_blow();
}

void Simulation::PhysicsWorld::read_strike_state() noexcept {
    state_.strike_phase = static_cast<std::uint8_t>(strike_phase_);
    state_.strike_move = strike_move_;
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
