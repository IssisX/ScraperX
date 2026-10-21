// One good arm (an arm torn off by the Matron, or broken): the moves a
// one-armed body still has, and what it no longer does. Members of
// Simulation::PhysicsWorld.
//
//  * The grip is per hand: a hang hangs from the good hand's shoulder
//    (hang_shoulder_offset leans the body under it), and holds
//    kOneArmGripScale of two hands' (check_hang_grip). No hand over hand.
//  * The plain pull-up from a hang needs two arms. One-armed, the climb-up
//    is the heel hook: the swing stilled (under kHeelHookMaxSwingMps), a heel
//    up on the lip, and the mantle from there, kOneArmMantleScale slower.
//  * The lunge (a dyno): jump from a one-armed hang toward a lip above. The
//    pull throws the body up at what it takes to bring the hand to the lip
//    at the top of the flight (no more than kDynoMaxMps); the hand closes on
//    it only if Action comes within kDynoWindowS of that dead point. Early
//    or late, the hand finds nothing and the body falls.

#include "sim/physics_world.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
constexpr float kOneArmShoulderSideM = 0.2F;   // the good shoulder off the body's centre line
constexpr float kHeelHookMaxSwingMps = 1.2F;
constexpr double kOneArmMantleScale = 2.2;
constexpr float kDynoMaxMps = 4.4F;            // one arm's throw: about 1 m of rise
constexpr float kDynoProbeLiftM = 1.2F;        // the lip above felt for from here: over the lip in hand
constexpr float kDynoMinGainM = 0.35F;         // above the lip now, or it is the same lip
constexpr float kDynoWindowS = 0.12F;          // either side of the dead point
constexpr float kDynoLateGiveUpS = 0.35F;      // past the dead point with nothing pressed: falling
constexpr float kGravity = 9.81F;
constexpr float kDynoReachM = 1.4F;
constexpr float kDynoAsideM = 1.2F;           // a lunge to the side: a hold over           // the lunge's hand: the arm thrown out at full stretch
} // namespace

int Simulation::PhysicsWorld::good_arm_side() const noexcept {
    if (arms_lost() == 1) {
        return arm_lost_[0] ? 1 : 0;
    }
    if (arm_broken()) {
        return 1 - (broken_arm_side_ & 1);
    }
    return -1;   // both: the hang is centred
}

JPH::Vec3 Simulation::PhysicsWorld::one_arm_shoulder_lean() const noexcept {
    const int side = good_arm_side();
    if (side < 0) {
        return JPH::Vec3::sZero();
    }
    const JPH::Vec3 n = traversal_normal_;
    const JPH::Vec3 right(n.GetZ(), 0.0F, -n.GetX());
    return right * (side == 0 ? -kOneArmShoulderSideM : kOneArmShoulderSideM);
}

// Jump or Action in a one-armed hang (apply_traversal_commands). True when
// it was taken here.
bool Simulation::PhysicsWorld::one_arm_hang_command(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
    if (!one_arm() || traversal_state_ != TraversalState::Hanging) {
        return false;
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    if (commands.jump_requested) {
        // The lunge: a lip above, within one arm's throw; up, and to the side
        // the stick points (one hold on to the next).
        const JPH::RVec3 current = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 n = traversal_normal_;
        const JPH::Vec3 right(n.GetZ(), 0.0F, -n.GetX());
        const float side = std::clamp(static_cast<float>(commands.move_input_x * right.GetX() +
                                                         commands.move_input_z * right.GetZ()),
                                      -1.0F, 1.0F);
        const float aside = std::abs(side) > 0.3F ? (side > 0.0F ? kDynoAsideM : -kDynoAsideM) : 0.0F;
        const JPH::RVec3 lifted = origin + JPH::Vec3(0.0F, kDynoProbeLiftM, 0.0F) + right * aside;
        const LedgeProbe above = probe_ledge(lifted, traversal_normal_, kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                             kPlayerHalfHeight + kHangMaximumRiseAboveCentre, true, false, kDynoReachM);
        if (!above.valid || above.ledge_point.GetY() < current.GetY() + kDynoMinGainM) {
            ++dyno_refused_count_;
            return true;
        }
        // To bring the hang's centre to the new lip's hang height at the top.
        const float rise = static_cast<float>(above.ledge_point.GetY() - kHangDropBelowLedge - origin.GetY());
        const float needed = std::sqrt(2.0F * kGravity * std::max(0.05F, rise + 0.05F));
        const float v = std::min(kDynoMaxMps, needed);
        const JPH::Vec3 normal = traversal_normal_;
        const float flight = v / kGravity;
        const JPH::Vec3 across = right * (aside / std::max(0.2F, flight));
        release_hang(bodies);
        regrab_lockout_ticks_ = 0;
        bodies.SetLinearVelocity(player_id_, JPH::Vec3(0.0F, v, 0.0F) + normal * 0.15F + across);
        dyno_active_ = true;
        dyno_t_ = 0.0F;
        dyno_apex_t_ = v / kGravity;
        dyno_short_ = needed > kDynoMaxMps;
        dyno_target_y_ = static_cast<float>(above.ledge_point.GetY());
        dyno_last_error_s_ = 0.0F;
        ++dyno_launch_count_;
        flight_from_jump_ = true;
        return true;
    }
    if (commands.traversal_requested) {
        // The heel hook: only from a still hang.
        const JPH::Vec3 v = bodies.GetLinearVelocity(player_id_);
        if (v.Length() > kHeelHookMaxSwingMps) {
            ++heel_hook_refused_count_;
            return true;
        }
        ++heel_hook_count_;
        begin_mantle_from_hang(bodies);
        traversal_duration_ *= kOneArmMantleScale;
        return true;
    }
    return false;
}

// In the lunge's flight: true while it has the controls (no automatic catch).
bool Simulation::PhysicsWorld::dyno_step(JPH::BodyInterface &bodies, const StepCommands &commands,
                                         const float dt) noexcept {
    if (!dyno_active_) {
        return false;
    }
    if (traversal_state_ != TraversalState::None || grounded_) {
        dyno_active_ = false;
        return false;
    }
    dyno_t_ += dt;
    const bool press = commands.traversal_requested || commands.jump_requested || commands.up_requested;
    const float error = dyno_t_ - dyno_apex_t_;
    if (press) {
        dyno_last_error_s_ = error;
        dyno_active_ = false;
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        // Felt for at the lip it was thrown at (over the one let go of).
        const JPH::RVec3 feel(origin.GetX(), dyno_target_y_ - 0.7F, origin.GetZ());
        const LedgeProbe probe = probe_ledge(feel, facing_, kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                             kPlayerHalfHeight + kHangMaximumRiseAboveCentre, true, false, kDynoReachM);
        const bool reach = probe.valid && std::abs(static_cast<float>(probe.ledge_point.GetY()) - dyno_target_y_) < 0.15F &&
                           JPH::Vec3(probe.ledge_point - (origin + JPH::Vec3(0.0F, kHangDropBelowLedge, 0.0F))).Length() <
                               kDynoReachM;
        if (std::abs(error) <= kDynoWindowS && reach) {
            ++dyno_catch_count_;
            begin_hang(bodies, probe, origin);
        } else {
            // The hand slaps the wall: nothing in it.
            ++dyno_miss_count_;
            dyno_no_catch_ = true;   // until the feet are down
        }
        return true;
    }
    if (error > kDynoLateGiveUpS) {
        dyno_active_ = false;
        ++dyno_miss_count_;
        dyno_last_error_s_ = error;
        dyno_no_catch_ = true;
    }
    return true;
}

} // namespace scraperx::sim
