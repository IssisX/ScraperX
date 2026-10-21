// Claimed (matron.hpp "Fatal"): the second arm gone, she has won, and she
// finishes it with her hands and the machine on her deck. Every beat is a
// physical act: her knee and fist are capped muscles on bodies
// (physics_world_fatality.cpp), the legs tear by the same strain law as the
// arms, the torso goes up on the Counterweight Well's stage-A rope as the
// 800 kg skip drops, and it comes back down when her knot parts at the jerk.
// Nothing here moves the player; it moves her and poses her hands.

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;
constexpr float kPiF = 3.14159265358979F;
constexpr float kHoldTurnRate = 5.0F;
float xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
} // namespace

void Matron::fatal_next(const int beat) noexcept {
    fatal_beat_ = beat;
    fatal_sub_ = 0;
    hold_s_ = 0.0F;
    if (beat == static_cast<int>(Fatal::Wipe)) {
        ++waste_not_;   // the voice hook: "Waste not."
    }
}

void Matron::fatal_pinned() noexcept {
    if (claimed() && fatal_beat_ == static_cast<int>(Fatal::Pin)) {
        fatal_next(static_cast<int>(Fatal::Leg0));
    }
}
void Matron::fatal_leg_taken() noexcept {
    if (leg_side() >= 0 && fatal_sub_ == 0) {
        fatal_sub_ = 1;
        hold_s_ = 0.0F;
    }
}
void Matron::fatal_leg_torn() noexcept {
    const int side = leg_side();
    if (side < 0) {
        return;
    }
    // Tossed aside, behind her, low.
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    leg_toss_v_ = (right * (side == 0 ? -0.8F : 0.8F) - fwd * 0.5F + Vec3(0.0F, 0.35F, 0.0F)).Normalized() * kLegTossMps;
    leg_toss_ = true;
    fatal_next(side == 0 ? static_cast<int>(Fatal::Leg1) : static_cast<int>(Fatal::Drag));
}
void Matron::fatal_collar_taken() noexcept {
    if (reaching_collar()) {
        fatal_sub_ = 1;
        hold_s_ = 0.0F;
    }
}
void Matron::fatal_hooked() noexcept {
    if (hooking()) {
        fatal_next(static_cast<int>(Fatal::Kick));
    }
}
void Matron::fatal_handle_taken() noexcept {
    if (reaching_handle()) {
        fatal_sub_ = 1;
        hold_s_ = 0.0F;
    }
}
void Matron::fatal_tripped() noexcept {
    if (claimed() && fatal_beat_ == static_cast<int>(Fatal::Kick)) {
        fatal_next(static_cast<int>(Fatal::Haul));
    }
}
void Matron::fatal_dropped() noexcept {
    if (claimed() && fatal_beat_ == static_cast<int>(Fatal::Haul)) {
        fatal_next(static_cast<int>(Fatal::Fall));
    }
}
void Matron::fatal_landed() noexcept {
    if (claimed() && fatal_beat_ == static_cast<int>(Fatal::Fall)) {
        fatal_next(static_cast<int>(Fatal::Wipe));
    }
}

void Matron::step_claimed(const float dt, const RVec3 at, const float feet) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const RVec3 shoulder = at + shoulder_local();
    const auto stand = [&] { walk_toward(dt, ax, az, 0.0F); };
    const auto go_to = [&](const float x, const float z, const float pace) {
        const float off = xz(ax, az, x, z);
        if (off <= 0.25F) {
            stand();
            return true;
        }
        float wx = x;
        float wz = z;
        if (off > 6.0F) {
            route_toward(ax, az, x, z, on_catwalk_line(ax, az, 0.0F), wx, wz);
        }
        walk_toward(dt, wx, wz, std::min(pace, 0.5F + 1.5F * off));
        return false;
    };
    const auto face = [&](const RVec3 p) {
        const float want = std::atan2(float(p.GetX()) - ax, float(p.GetZ()) - az);
        yaw_ = turn_to(yaw_, want, kHoldTurnRate, dt);
        return std::abs(std::remainder(want - yaw_, 2.0F * kPiF)) < 0.35F;
    };
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    const FatalPoints &p = fatal_points_;
    const RVec3 rest_hand = RVec3(at.GetX(), feet + 1.0F, at.GetZ()) + right * 0.35F + fwd * 0.2F;
    const Fatal beat = static_cast<Fatal>(fatal_beat_);
    // A beat the world will not let her finish (a jammed rope, a body caught
    // on a post): she is done with it, and with them.
    if (hold_s_ > kFatalGiveUpSeconds && beat != Fatal::Wipe && beat != Fatal::Black) {
        grip_on_ = false;
        fatal_next(static_cast<int>(Fatal::Wipe));
        return;
    }
    switch (beat) {
    case Fatal::Lay: {
        // Still by its ankle: to the spot, and down on the steel.
        arm_reach_m_ = kStoopReachM;
        pull_n_ = kHandPullN;
        if (fatal_sub_ == 0) {
            const bool there = go_to(kPinSpotX, kPinSpotZ, kHuntMps * kCarryPaceScale);
            pose_hand(dt, RVec3(at.GetX(), feet + kCarryHandUpM, at.GetZ()) + right * kCarrySideM, kHandSpeedMps);
            if (there) {
                fatal_sub_ = 1;
                hold_s_ = 0.0F;
            }
        } else {
            stand();
            // Lowered until the head and shoulders are on the deck.
            pose_hand(dt, RVec3(at.GetX(), feet + 1.15F, at.GetZ()) + right * kCarrySideM + fwd * 0.4F,
                      0.5F * kHandSpeedMps);
            if (grip_on_ && hold_s_ > 1.6F) {
                grip_on_ = false;           // her fingers open: not a Release
                let_go_ = true;
                let_go_push_ = Vec3::sZero();
                hold_s_ = 0.0F;
            }
            if (!grip_on_ && hold_s_ > 1.0F && p.torso_grounded) {
                fatal_next(static_cast<int>(Fatal::Pin));
            }
        }
        break;
    }
    case Fatal::Pin:
    case Fatal::Leg0:
    case Fatal::Leg1: {
        // Down over it, her knee on the chest.
        const Vec3 to_torso = Vec3(p.torso - at);
        Vec3 flat(to_torso.GetX(), 0.0F, to_torso.GetZ());
        const RVec3 beside = flat.Length() > 0.75F ? p.torso - flat.Normalized() * 0.7F : at;
        if (go_to(float(beside.GetX()), float(beside.GetZ()), 1.2F)) {
            (void)face(p.torso);
        }
        knee_target_ = p.torso + Vec3(0.0F, 0.12F, 0.0F);
        arm_reach_m_ = kStoopReachM;
        if (beat == Fatal::Pin) {
            pose_hand(dt, rest_hand, kHandSpeedMps);
            break;
        }
        const int side = leg_side();
        const RVec3 ankle = p.ankle[side];
        if (fatal_sub_ == 0) {
            pull_n_ = kHandReachPullN;
            pose_hand(dt, ankle, kHandReachSpeedMps);
        } else {
            // Drawn out along the leg, away from her knee: the hip takes it.
            pull_n_ = std::min(kLegPullN, std::max(pull_n_, kHandPullN) + kRipRampNps * dt);
            Vec3 away = Vec3(ankle - knee_target_);
            away.SetY(0.0F);
            away = away.Length() > 1.0e-3F ? away.Normalized() : fwd;
            pose_hand(dt, ankle + away * 0.8F + Vec3(0.0F, 0.15F, 0.0F), 2.0F * kHandReachSpeedMps);
        }
        (void)shoulder;
        break;
    }
    case Fatal::Drag: {
        arm_reach_m_ = kStoopReachM;
        if (fatal_sub_ == 0) {
            // Her fist in the collar.
            stand();
            (void)face(p.torso);
            pull_n_ = kHandReachPullN;
            pose_hand(dt, p.collar, kHandReachSpeedMps);
            break;
        }
        // Dragged on its back to the bollard; the collar up to the shackle.
        pull_n_ = kHandPullN;
        const bool there = go_to(kBollardStandX, kBollardStandZ, 1.1F);
        if (!there) {
            pose_hand(dt, RVec3(at.GetX(), feet + 0.9F, at.GetZ()) - fwd * 0.6F + right * 0.3F, kHandSpeedMps);
        } else {
            fatal_sub_ = std::max(fatal_sub_, 1);
            if (face(p.shackle)) {
                pose_hand(dt, p.shackle - Vec3(0.0F, 0.05F, 0.0F), 0.7F * kHandSpeedMps);
            }
        }
        break;
    }
    case Fatal::Kick: {
        // To the catch's trip handle under the gantry, and down with it.
        arm_reach_m_ = kArmReachM;
        pose_hand(dt, rest_hand, kHandSpeedMps);
        const bool there = go_to(kTripStandX, kTripStandZ, 1.4F);
        if (there) {
            (void)face(p.handle);
        }
        if (fatal_sub_ == 0) {
            pull2_n_ = kHandReachPullN;
            pose_hand2(dt, there ? p.handle : shoulder + fwd * 0.4F, kHandReachSpeedMps);
        } else {
            // Her boot on the bar and her weight through it: down and back.
            pull2_n_ = std::min(1400.0F, std::max(pull2_n_, 600.0F) + 1500.0F * dt);
            pose_hand2(dt, p.handle + Vec3(0.0F, -0.9F, 0.0F) - fwd * 0.3F, 2.0F);
        }
        break;
    }
    case Fatal::Haul:
    case Fatal::Fall: {
        // She steps back from the well's rail and watches it go.
        stand();
        (void)face(p.torso);
        pose_hand(dt, rest_hand, kHandSpeedMps);
        break;
    }
    case Fatal::Wipe: {
        stand();
        // Her palms down the apron, one, the other, twice.
        const float s = std::sin(hold_s_ * 2.0F * kPiF / 1.3F);
        pose_hand(dt, RVec3(at.GetX(), feet + 0.95F + 0.18F * s, at.GetZ()) + fwd * 0.38F + right * 0.12F,
                  kHandSpeedMps);
        if (hold_s_ >= kWipeSeconds) {
            fatal_next(static_cast<int>(Fatal::Black));
        }
        break;
    }
    case Fatal::Black:
        stand();
        pose_hand(dt, rest_hand, kHandSpeedMps);
        break;
    }
}

} // namespace scraperx::sim
