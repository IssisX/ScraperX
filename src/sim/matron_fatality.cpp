// Claimed (matron.hpp "Fatal"): the fatality's floor version (no edge in
// reach for the hang, matron_hold.cpp), and a body left armless. She lays
// him down on the steel where she stands, her knee on his chest, her palms
// down her apron, black: about kFatalFloorS, and the world's death and the
// checkpoint's restore end it (physics_world_fatality.cpp). Every beat is a
// physical act: her knee and fist are capped muscles on bodies. The beats
// that were between (the legs by the strain law, the Counterweight Well's
// rope and its 800 kg skip) are no longer begun; their steps are kept below.
// Nothing here moves the player; it moves her and poses her hands.

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;
constexpr float kPiF = 3.14159265358979F;
constexpr float kHoldTurnRate = 5.0F;
// Held up short of her spot (no nearer in kWorkStallS) with the thing she
// works on in reach less kWorkMarginM, she works from there; once at it she
// stays until pushed kWorkSlackM past her reach.
constexpr float kWorkStallS = 0.5F;
constexpr float kWorkMarginM = 0.15F;
constexpr float kWorkSlackM = 0.3F;
// The well's skip-lift cage and the trip gantry over its open north side
// (band_counterweight_well.cpp) stand between the deck and the bollard. She
// goes round them, never through: out north past the gantry's posts (z
// -129.6..-129.5), west past its west post (x -12.6..-12.5), then south to
// the bollard; and back the same way to the trip handle. The body she drags
// follows her line, so it clears them too.
constexpr float kGantryClearZ = -128.8F;   // her line north of the posts
constexpr float kPastGantryZ = -129.1F;    // she is north of them
constexpr float kPastCageX = -13.0F;       // she is west of them
constexpr float kWestCornerX = -13.7F;     // where she turns south
float xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
// The next point on her way to (x, z) round the cage and the gantry.
void round_the_cage(const float ax, const float az, const float x, const float z, float &wx, float &wz) {
    wx = x;
    wz = z;
    const bool west = ax <= kPastCageX;
    const bool north = az >= kPastGantryZ;
    if (x <= kPastCageX && !west) {
        wx = north ? kWestCornerX : ax;
        wz = kGantryClearZ;
    } else if (z >= kPastGantryZ && !north) {
        wx = ax;
        wz = kGantryClearZ;
    }
}
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
} // namespace

void Matron::fatal_work_reset() noexcept {
    fatal_at_work_ = false;
    fatal_best_off_ = 1.0e9F;
    fatal_stall_s_ = 0.0F;
}

void Matron::fatal_next(const int beat) noexcept {
    fatal_beat_ = beat;
    fatal_sub_ = 0;
    hold_s_ = 0.0F;
    fatal_work_reset();
    if (beat == static_cast<int>(Fatal::Wipe)) {
        ++waste_not_;   // the voice hook: "Waste not."
    }
}

void Matron::fatal_pinned() noexcept {
    if (claimed() && fatal_beat_ == static_cast<int>(Fatal::Pin) && fatal_sub_ == 0) {
        // Her knee on his chest: kFloorPinSeconds of her weight, then her
        // apron (step_claimed).
        fatal_sub_ = 1;
        hold_s_ = 0.0F;
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
        fatal_work_reset();
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
    // To the spot she works the thing from: there within 0.25 m, or held up
    // short of it with the thing already in her reach. One walk a tick.
    const auto at_work = [&](const float x, const float z, const float pace, const RVec3 thing,
                             const float reach) {
        const float to_thing = Vec3(thing - shoulder).Length();
        if (fatal_at_work_ && to_thing > reach + kWorkSlackM) {
            fatal_work_reset();
        }
        if (!fatal_at_work_) {
            const float off = xz(ax, az, x, z);
            if (off < fatal_best_off_ - 0.02F) {
                fatal_best_off_ = off;
                fatal_stall_s_ = 0.0F;
            } else {
                fatal_stall_s_ += dt;
            }
            fatal_at_work_ =
                off <= 0.25F || (fatal_stall_s_ >= kWorkStallS && to_thing <= reach - kWorkMarginM);
        }
        if (fatal_at_work_) {
            stand();
            return true;
        }
        float wx = x;
        float wz = z;
        round_the_cage(ax, az, x, z, wx, wz);
        (void)go_to(wx, wz, pace);
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
    // Beside the torso, wherever it lies now: 0.7 m short of it on her side.
    const auto beside_torso = [&] {
        const Vec3 to_torso = Vec3(fatal_points_.torso - at);
        const Vec3 flat(to_torso.GetX(), 0.0F, to_torso.GetZ());
        return flat.Length() > 0.75F ? fatal_points_.torso - flat.Normalized() * 0.7F : at;
    };
    const Fatal beat = static_cast<Fatal>(fatal_beat_);
    // A beat the world will not let her finish (a jammed rope, a body caught
    // on a post): she is done with it, and with them.
    if (hold_s_ > kFatalGiveUpSeconds && beat != Fatal::Wipe && beat != Fatal::Black) {
        if (grip_on_) {
            let_go_ = true;
            let_go_push_ = Vec3::sZero();
        }
        grip_on_ = false;
        fatal_next(static_cast<int>(Fatal::Wipe));
        return;
    }
    switch (beat) {
    case Fatal::Lay: {
        // Still by its ankle: down on the steel where she stands (no edge in
        // her reach), lowered at her arm's pace; her fingers open
        // kFloorLaySeconds into it.
        arm_reach_m_ = kStoopReachM;
        pull_n_ = kHandPullN;
        const bool deck_under = deck_close_below(p.torso, feet);
        if (grip_on_ && hold_s_ > kFloorLaySeconds && !deck_under) {
            // Her deck not close under him (an edge by her): she keeps hold
            // and walks him on, back from where she faces.
            const float facing = yaw_;
            walk_toward(dt, ax - 0.6F * fwd.GetX(), az - 0.6F * fwd.GetZ(), 0.8F);
            yaw_ = facing;
        } else {
            stand();
        }
        pose_hand(dt, RVec3(at.GetX(), feet + 1.15F, at.GetZ()) + right * kCarrySideM + fwd * 0.4F, kHandSpeedMps);
        if (grip_on_ && hold_s_ > kFloorLaySeconds && deck_under) {
            grip_on_ = false;           // her fingers open: not a Release
            let_go_ = true;
            let_go_push_ = Vec3::sZero();
            hold_s_ = 0.0F;
        }
        // Down on it (or not settling in kFloorBeatGiveUpS): her knee.
        if (!grip_on_ && ((hold_s_ > 0.2F && p.torso_grounded) || hold_s_ > kFloorBeatGiveUpS)) {
            fatal_next(static_cast<int>(Fatal::Pin));
        }
        break;
    }
    case Fatal::Pin:
    case Fatal::Leg0:
    case Fatal::Leg1: {
        // Down over it, her knee on the chest.
        const RVec3 beside = beside_torso();
        if (go_to(float(beside.GetX()), float(beside.GetZ()), 1.2F)) {
            (void)face(p.torso);
        }
        knee_target_ = p.torso + Vec3(0.0F, 0.12F, 0.0F);
        arm_reach_m_ = kStoopReachM;
        if (beat == Fatal::Pin) {
            pose_hand(dt, rest_hand, kHandSpeedMps);
            // Her weight on his chest kFloorPinSeconds (fatal_pinned); a knee
            // that cannot get there in kFloorBeatGiveUpS, she leaves it.
            if ((fatal_sub_ == 1 && hold_s_ >= kFloorPinSeconds) || (fatal_sub_ == 0 && hold_s_ > kFloorBeatGiveUpS)) {
                fatal_next(static_cast<int>(Fatal::Wipe));
            }
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
            // To it, wherever the last leg left it, and her fist in the collar.
            const RVec3 beside = beside_torso();
            if (go_to(float(beside.GetX()), float(beside.GetZ()), 1.2F)) {
                (void)face(p.torso);
            }
            pull_n_ = kHandReachPullN;
            pose_hand(dt, p.collar, kHandReachSpeedMps);
            break;
        }
        // Dragged on its back to the bollard; the collar up to the shackle.
        pull_n_ = kHandPullN;
        const bool there = at_work(kBollardStandX, kBollardStandZ, 1.1F, p.shackle, arm_reach_m_);
        if (!there) {
            pose_hand(dt, RVec3(at.GetX(), feet + 0.9F, at.GetZ()) - fwd * 0.6F + right * 0.3F, kHandSpeedMps);
        } else {
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
        const bool there = at_work(kTripStandX, kTripStandZ, 1.4F, p.handle, kArmReachM);
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
