// The Matron's hold (matron.hpp, "Her hold"), at every stage: her hand goes
// for the player's ankle, closes on it, she walks them upside down to the
// shaft's rail, holds them out over the drop, tears an arm off (ArmPlay),
// and then sets them back on her deck (an ordinary hold never lets go over
// open air). Her second catch, his one arm left, is the fatality
// (matron.hpp kFatalTargetS): out over the nearest edge with a killing fall
// under it, a window for his blow, and her fingers opening; no edge in
// reach, the floor version (Claimed, matron_fatality.cpp).
// No kill on touch: a grip she can lose (physics_world_grab.cpp tears it on
// its load and strain), a hand that moves at a finite speed with a finite
// pull, a hold she has to brace for. Members of Matron; pre_step calls
// step_hold in the hold's states. Every choice is simulation state and her
// seeded roll: deterministic.

#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;
constexpr float kPiF = 3.14159265358979F;
constexpr float kGravity = 9.81F;
constexpr float kHoldTurnRate = 5.0F;   // her turn with a body in her hand
float xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
// The player's capsule (physics_world_tuning.hpp: 1.8 m, its middle 0.9 up).
constexpr float kPlayerHalfM = 0.9F;
// His middle under her hand, hung by the thigh; and his swing about her
// fingers as a pendulum (a 1.8 m body turning about a point that far from
// its middle: g d / (L^2 / 12 + d^2)).
constexpr float kHungM = Matron::kHeldBelowMiddleM;
const float kSwingOmega = std::sqrt(9.81F * kHungM / (1.8F * 1.8F / 12.0F + kHungM * kHungM));
} // namespace

Vec3 Matron::shoulder_at(const float lean, const float crouch, const float side) const noexcept {
    // Her trunk from the hips, leant forward `lean` (her hunch in it), the
    // hips down `crouch`; the shoulder `side` (1 right, -1 left) of her middle.
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    const float torso = std::sqrt(kTorsoUpM * kTorsoUpM + kTorsoFwdM * kTorsoFwdM);
    const float tilt = std::atan2(kTorsoFwdM, kTorsoUpM) + lean;
    return right * (side * kShoulderSideM) + fwd * (torso * std::sin(tilt)) +
           Vec3(0.0F, kHipUpM - crouch - kStandCentre + torso * std::cos(tilt), 0.0F);
}

void Matron::posture_for(const RVec3 want, const float side, float &lean, float &crouch) const noexcept {
    // The least of her body that brings it in reach: upright if she can,
    // else leant, else leant and down (each in steps of 0.05); out of reach
    // however she goes, the nearest she gets.
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    float best = 1.0e9F;
    lean = 0.0F;
    crouch = 0.0F;
    constexpr int kSteps = 10;
    for (int c = 0; c <= kSteps; ++c) {
        const float down = kCrouchMaxM * float(c) / float(kSteps);
        for (int t = 0;; ++t) {
            const float bend = std::min(kLeanMaxRad, 0.05F * float(t));
            const float d = Vec3(want - (at + shoulder_at(bend, down, side))).Length();
            if (d <= pose_reach_) {
                lean = bend;
                crouch = down;
                return;
            }
            if (d < best) {
                best = d;
                lean = bend;
                crouch = down;
            }
            if (bend >= kLeanMaxRad) {
                break;
            }
        }
    }
}

float Matron::reach_to(const RVec3 want, const float side) const noexcept {
    float lean = 0.0F;
    float crouch = 0.0F;
    posture_for(want, side, lean, crouch);
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    return Vec3(want - (at + shoulder_at(lean, crouch, side))).Length();
}

RVec3 Matron::in_reach(const RVec3 want, const float side) const noexcept {
    const RVec3 shoulder = physics_.GetBodyInterface().GetPosition(body_) + shoulder_at(lean_, crouch_, side);
    const Vec3 to = Vec3(want - shoulder);
    const float length = to.Length();
    return length <= pose_reach_ ? want : shoulder + to * (pose_reach_ / length);
}

bool Matron::bodies_hung(const float cos_down) const noexcept {
    return physics_.GetBodyInterface().GetRotation(player_).RotateAxisY().GetY() <= -cos_down;
}

void Matron::step_posture(const float dt, const RVec3 want, const bool both) noexcept {
    // As much of her body as this hand's pose needs, and her other hand's
    // reach too while it leads (the wrist): the more of the two.
    float lean = 0.0F;
    float crouch = 0.0F;
    posture_for(want, 1.0F, lean, crouch);
    if (both) {
        float lean2 = 0.0F;
        float crouch2 = 0.0F;
        posture_for(hand2_want_, -1.0F, lean2, crouch2);
        lean = std::max(lean, lean2);
        crouch = std::max(crouch, crouch2);
    }
    if (grip_on_) {
        // Her trunk never takes her shoulder further from her hand, where it
        // really is with his weight in it, than her arm: she straightens only
        // as her hand comes in (else her own body, not her capped muscle,
        // would haul on him through her arm).
        float lean_h = 0.0F;
        float crouch_h = 0.0F;
        posture_for(player_ankle_, 1.0F, lean_h, crouch_h);
        lean = std::max(lean, lean_h);
        crouch = std::max(crouch, crouch_h);
        // Nor does her other hand's reach (the wrist) bend her past it: her
        // loaded arm at its length, the nearest that hand gets is from a
        // body that keeps it (her trunk would drag him through her arm).
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        const auto keeps_hand = [&](const float l, const float c) {
            return Vec3(player_ankle_ - (at + shoulder_at(l, c, 1.0F))).Length() <= kArmReachM;
        };
        if (both && !keeps_hand(lean, crouch)) {
            float best = 1.0e9F;
            lean = lean_h;
            crouch = crouch_h;
            constexpr int kSteps = 10;
            for (int c = 0; c <= kSteps; ++c) {
                const float down = kCrouchMaxM * float(c) / float(kSteps);
                for (int t = 0; 0.05F * float(t) <= kLeanMaxRad + 1.0e-4F; ++t) {
                    const float bend = std::min(kLeanMaxRad, 0.05F * float(t));
                    if (!keeps_hand(bend, down)) {
                        continue;
                    }
                    const float d = Vec3(hand2_want_ - (at + shoulder_at(bend, down, -1.0F))).Length();
                    if (d < best - 1.0e-4F) {
                        best = d;
                        lean = bend;
                        crouch = down;
                    }
                }
            }
        }
    }
    // With his weight in her hand, her body comes up under it slowly
    // (kLoadedPostureScale of her pace): her arm is never jerked by it.
    const float pace = grip_on_ ? kLoadedPostureScale : 1.0F;
    const float turn = pace * kLeanRadps * dt;
    const float sink = pace * kCrouchMps * dt;
    lean_ += std::clamp(lean - lean_, -turn, turn);
    crouch_ += std::clamp(crouch - crouch_, -sink, sink);
}

void Matron::begin_grab(const RVec3 player) {
    routecraft_.cancel();
    if (!body_in_world_ || dead_ || holding()) {
        return;
    }
    drop_stock();
    turn_ = Turn::None;
    turn_s_ = 0.0F;
    state_ = State::Grab;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    grip_on_ = false;
    grip_was_on_ = false;
    armplay_done_ = false;
    retake_ = false;
    hold_retakes_ = 0;
    spare_side_set_ = false;
    spare_tries_ = 0;
    hold_tears_at_ = arm_tears_;
    fatal_hold_ = false;   // known as her fingers close (grip_taken)
    fatal_path_ = FatalPath::None;
    fatal_carry_s_ = 0.0F;
    fatal_window_s_ = 0.0F;
    fatal_hang_ = false;
    fatal_dropped_ = false;
    fatal_s_ = 0.0F;
    fatal_check_ticks_ = 0;
    fatal_lethal_ = false;
    fatal_toss_ = 0.0F;
    reel_ = false;
    reel_to_floor_ = false;
    spare_walk_ = false;
    pull_n_ = kHandReachPullN;
    lean_ = 0.0F;
    crouch_ = 0.0F;
    hand2_leads_ = false;
    tip_step_ = 0;
    over_drop_ = false;
    grip_free_ = false;
    fatal_heave_back_ = false;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    hand_target_ = at + shoulder_local();
    hand_target_v_ = Vec3::sZero();
    take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, Sense::Touch);
}

void Matron::grip_taken() noexcept {
    if (state_ == State::Grab && !grip_on_) {
        grip_on_ = true;
        grip_was_on_ = true;
        ++grabs_;
        hold_s_ = 0.0F;
        pull_n_ = kHandPullN;
        // His one arm left (the world's count, given every tick her hand is
        // out), and none off him this hold: this catch is the fatality.
        // Its clock runs from these fingers (a re-take keeps it running).
        if (!fatal_hold_ && player_arms_left_ == 1 && arm_tears_ == hold_tears_at_) {
            fatal_hold_ = true;
            fatal_s_ = 0.0F;
        }
    }
}

void Matron::grip_lost(const bool torn, const bool fought) noexcept {
    const bool had = grip_on_;
    grip_on_ = false;
    if (torn && had) {
        ++grips_torn_;
    }
    if (had && state_ == State::Claimed) {
        // Out of her hand on the way to her spot: she goes to it where it
        // lies. The run stays hers.
        if (fatal_beat_ == static_cast<int>(Fatal::Lay)) {
            fatal_next(static_cast<int>(Fatal::Pin));
        }
        return;
    }
    if (had && holding_state(state_)) {
        if (fatal_hold_ && state_ == State::Dangle) {
            // Out of her fist with him out past the edge, over the drop (his
            // thrashing, the world), into a fall foreseen to kill: he goes
            // down it all the same (the release beat). Short of it (hooked on
            // the rail in her heave), or into a fall he can survive, as any
            // hold.
            const float lip_x = rim_x_ + rim_nx_ * kRimStandM;
            const float lip_z = rim_z_ + rim_nz_ * kRimStandM;
            const float past = (float(player_ankle_.GetX()) - lip_x) * rim_nx_ +
                               (float(player_ankle_.GetZ()) - lip_z) * rim_nz_;
            const BodyInterface &bodies = physics_.GetBodyInterface();
            if ((fatal_hang_ || past > 0.3F) && !no_kill_foreseen_ &&
                fall_impact_mps(bodies.GetPosition(player_), bodies.GetRotation(player_),
                                bodies.GetLinearVelocity(player_), bodies.GetAngularVelocity(player_),
                                kFatalLetGoMarginM) >= kFatalLandMps) {
                fatal_release();
                return;
            }
        }
        if (player_arms_left_ <= 0) {
            // Torn out of her hand with no arm left: the run is hers all
            // the same.
            claim();
            return;
        }
        if (state_ == State::ArmPlay && hold_phase_ >= 2) {
            // The arm hers already (left in her other hand as the body
            // pulled away, tear_grip, or torn before): its beats go on, the
            // body away; the hold ends after them (step_armplay).
            return;
        }
        if (torn && !fought && arm_tears_ == hold_tears_at_ && armplay_for_test_ != 0 &&
            hold_retakes_ < kMaxRetakes) {
            // Torn out of her hand by the world, not by them (her heave
            // against the rail, a snag), before their arm: no escape. She
            // takes them again where they land, no cooldown, and the arm
            // comes off there (at most kMaxRetakes times a hold).
            retake();
            return;
        }
        // Torn out of her hand (by them, or by the world past her
        // re-takes): the hand stays out a moment, then she is after them
        // again.
        state_ = State::Release;
        hold_s_ = 0.0F;
        hold_phase_ = 0;
        reel_ = false;
        grab_cooldown_s_ = hands_off_after_hold();
    }
}

// Her hand's pose moves toward `target` no faster than `speed`.
void Matron::pose_hand(const float dt, const RVec3 target, const float speed, const Vec3 carried) {
    // Carried along with her as she walks (`carried`, her own velocity: the
    // fatality's carry), then toward `target` no faster than `speed`.
    // Her body leans and crouches toward the reach it needs (her second
    // hand's, while it goes for a wrist), and the pose stays within her arm
    // of her shoulder as her body has it.
    const RVec3 before = hand_target_;
    step_posture(dt, target, hand2_leads_);
    hand2_leads_ = false;
    hand_target_ += carried * dt;
    const Vec3 to = Vec3(target - hand_target_);
    const float length = to.Length();
    const float step = speed * dt;
    hand_target_ = in_reach(length <= step ? target : hand_target_ + to * (step / length));
    hand_target_v_ = Vec3(hand_target_ - before) / std::max(dt, 1.0e-4F);
}

void Matron::foresee_fall(const RVec3 player, const Vec3 out) {
    // His fall foreseen from now, as he moves and spins, with the least of
    // her tosses that kills.
    const BodyInterface &bodies = physics_.GetBodyInterface();
    const Quat rot = bodies.GetRotation(player_);
    const Vec3 v = bodies.GetLinearVelocity(player_);
    const Vec3 w = bodies.GetAngularVelocity(player_);
    fatal_lethal_ = false;
    for (const float toss : kFatalTossMps) {
        fatal_lethal_mps_ = fall_impact_mps(player, rot, v + out * toss, w, kFatalLetGoMarginM);
        if (fatal_lethal_mps_ >= kFatalLandMps && !no_kill_foreseen_) {
            fatal_lethal_ = true;
            fatal_toss_ = toss;
            return;
        }
    }
}

float Matron::fist_reach_m(const RVec3 at, const RVec3 player, const float swing_v) const {
    // His shoulder (his punch's start: kShoulderOfMiddleM along his body's
    // up from his middle, under it hung head down) to her body's surface,
    // and so where his swing outward (`swing_v`, as a pendulum under her
    // hand) carries it; a tilt her grip's friction holds stays where it is.
    const RVec3 shoulder = player + physics_.GetBodyInterface().GetRotation(player_) * Vec3(0.0F, kShoulderOfMiddleM, 0.0F);
    const float y = std::clamp(float(shoulder.GetY()), float(at.GetY()) - kHalfHeight, float(at.GetY()) + kHalfHeight);
    const float now = Vec3(shoulder - RVec3(at.GetX(), y, at.GetZ())).Length() - kRadius;
    return now + std::abs(swing_v) / kSwingOmega;
}

bool Matron::braced_at_rim(const float ax, const float az) const noexcept {
    const float rail_x = rim_x_ + rim_nx_ * kRimStandM;
    const float rail_z = rim_z_ + rim_nz_ * kRimStandM;
    const float in = (rail_x - ax) * rim_nx_ + (rail_z - az) * rim_nz_;
    const float along = std::abs((ax - rim_x_) * rim_nz_ - (az - rim_z_) * rim_nx_);
    return in >= 0.0F && in <= kRimBraceM && along <= 0.6F;
}

// Her spot at the shaft's rail: of the spots along it within kRimSearchM,
// the nearest she can walk to with a ledge below it whose fall is under
// kReleaseSafeMps (in reach of a push of kReleasePushMps), else the nearest.
bool Matron::choose_rim(const float ax, const float az) {
    if (player_cast_shape_ == nullptr) {
        player_cast_shape_ = new CapsuleShape(0.55F, 0.35F);
        hang_room_shape_ = new CapsuleShape(0.55F, 0.5F);   // the body and its swing
    }
    const Level &level = levels_[static_cast<std::size_t>(level_)];
    const float floor_y = level.floor_y;
    const float half = kShaftHalfM;
    float best = std::numeric_limits<float>::max();
    bool found = false;
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    for (int side = 0; side < 4; ++side) {
        // North, south (z rails), east, west (x rails); the normal out over the shaft.
        const float nx = side == 2 ? -1.0F : side == 3 ? 1.0F : 0.0F;
        const float nz = side == 0 ? -1.0F : side == 1 ? 1.0F : 0.0F;
        for (float t = -half + 2.5F; t <= half - 2.5F; t += 1.0F) {
            const float rail_x = side == 2 ? half : side == 3 ? -half : t;
            const float rail_z = side == 0 ? kMidZ + half : side == 1 ? kMidZ - half : kMidZ + t;
            const float sx = rail_x - nx * kRimStandM;
            const float sz = rail_z - nz * kRimStandM;
            const float d = xz(ax, az, sx, sz);
            if (d > kRimSearchM || on_catwalk_line(sx, sz, 1.2F) || on_catwalk_line(rail_x + nx, rail_z + nz, 1.2F)) {
                continue;
            }
            float wx = sx;
            float wz = sz;
            if (!route_toward(ax, az, sx, sz, false, wx, wz)) {
                continue;
            }
            // A ledge below where they will hang.
            const float hang_x = sx + nx * kDangleOutM;
            const float hang_z = sz + nz * kDangleOutM;
            const float com_y = floor_y + kDangleHandUpM - kHungM;
            {
                // Room to hang there: the body's place over the drop (and its
                // way over the rail) clear of steel.
                bool room = true;
                for (const float up : {0.0F, 0.8F}) {
                    AnyHitCollisionCollector<CollideShapeCollector> touch;
                    CollideShapeSettings settings;
                    const RVec3 pose(hang_x, com_y + up, hang_z);
                    physics_.GetNarrowPhaseQuery().CollideShape(hang_room_shape_, Vec3::sReplicate(1.0F),
                                                                RMat44::sTranslation(pose), settings, pose, touch, {},
                                                                {}, skip);
                    room = room && !touch.HadHit();
                }
                if (!room) {
                    continue;
                }
            }
            bool ledge = false;
            RVec3 ledge_at = RVec3::sZero();
            for (const float off : {0.0F, 0.4F, 0.8F, 1.2F}) {
                for (const float lat : {0.0F, -0.4F, 0.4F}) {
                    if (ledge) {
                        break;
                    }
                    const float px = hang_x + nx * off + nz * lat;
                    const float pz = hang_z + nz * off - nx * lat;
                    // The body itself swept down from where it hangs: what it
                    // meets first is what it lands on.
                    const RVec3 from(px, com_y, pz);
                    const float reach = 40.0F;
                    RShapeCast cast(player_cast_shape_, Vec3::sReplicate(1.0F), RMat44::sTranslation(from),
                                    Vec3(0.0F, -reach, 0.0F));
                    ShapeCastSettings settings;
                    ClosestHitCollisionCollector<CastShapeCollector> hit;
                    physics_.GetNarrowPhaseQuery().CastShape(cast, settings, from, hit, {}, {}, skip);
                    if (!hit.HadHit()) {
                        continue;
                    }
                    const float land_y = float(from.GetY()) - reach * hit.mHit.mFraction;   // the body's middle
                    const Vec3 normal = -hit.mHit.mPenetrationAxis.Normalized();
                    const float drop = com_y - land_y;
                    const float speed = std::sqrt(2.0F * kGravity * std::max(drop, 0.0F));
                    const float fall_s = std::sqrt(2.0F * std::max(drop, 0.01F) / kGravity);
                    const float shift = std::hypot(px - hang_x, pz - hang_z);
                    if (drop < 1.0F || speed >= kReleaseSafeMps || shift / fall_s > kReleasePushMps ||
                        normal.GetY() < 0.7F) {
                        continue;
                    }
                    // Footing: flat under the whole of the feet, a plate, not a rung.
                    const float feet_y = land_y - kPlayerHalfM;
                    bool flat = true;
                    for (const Vec3 off2 : {Vec3(0.3F, 0.0F, 0.0F), Vec3(-0.3F, 0.0F, 0.0F), Vec3(0.0F, 0.0F, 0.3F),
                                            Vec3(0.0F, 0.0F, -0.3F)}) {
                        RayCastResult under;
                        const RVec3 top(px + off2.GetX(), feet_y + 0.15F, pz + off2.GetZ());
                        flat = flat && physics_.GetNarrowPhaseQuery().CastRay(RRayCast(top, Vec3(0.0F, -0.3F, 0.0F)),
                                                                              under, {}, {}, skip);
                    }
                    if (flat) {
                        ledge = true;
                        ledge_at = RVec3(px, feet_y, pz);
                    }
                }
            }
            const float score = d + (ledge ? 0.0F : 1000.0F);
            if (score < best) {
                best = score;
                found = true;
                rim_x_ = sx;
                rim_z_ = sz;
                rim_nx_ = nx;
                rim_nz_ = nz;
                aim_ledge_ = ledge;
                aim_ = ledge_at;
            }
        }
    }
    return found;
}

bool Matron::straight_on_floor(const float ax, const float az, const float x, const float z, const float feet) const {
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    const float length = xz(ax, az, x, z);
    const int steps = std::max(1, static_cast<int>(std::ceil(length / 0.5F)));
    for (int k = 0; k <= steps; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(steps);
        const RVec3 top(ax + (x - ax) * t, feet + 0.4F, az + (z - az) * t);
        RayCastResult under;
        if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(top, Vec3(0.0F, -0.8F, 0.0F)), under, {}, {}, skip) ||
            std::abs(feet + 0.4F - 0.8F * under.mFraction - feet) > kStepUpM) {
            return false;
        }
    }
    return way_clear(ax, az, x, z);
}

// The fatality's edge (matron.hpp kFatalTargetS): of the sides of the
// walkable tops of her floor in her scan's map, the spots kRimStandM in from
// them she can walk to straight within kFatalCarryS of her carry, the
// nearest where she has room to stand, the body room to hang out past the
// side (and its way over it), and a fall under it that kills: the body swept
// down from where it would hang meets steel, and not before kFatalDropM.
// Nothing here knows a place: what is open air is what the sweep finds.
bool Matron::choose_fatal_edge(const float ax, const float az, const float feet) {
    if (scanned_ == nullptr || hang_ruled_out_) {
        return false;
    }
    if (player_cast_shape_ == nullptr) {
        player_cast_shape_ = new CapsuleShape(0.55F, 0.35F);
        hang_room_shape_ = new CapsuleShape(0.55F, 0.5F);   // the body and its swing
    }
    const float reach = kHuntMps * kCarryPaceScale * (kFatalCarryS - kFatalCarryEaseS);
    struct Spot {
        float d, lip_x, lip_z, nx, nz;
        bool across;   // her floor again across the open air: a shaft in the steel, not the tower's outside
    };
    // Her floor: a top she stands on, broad enough to stand at its side.
    const auto her_floor = [&](const traversal::Patch &patch) {
        return !patch.narrow && std::abs(patch.top - feet) <= 0.3F && patch.max_x - patch.min_x >= 1.2F &&
               patch.max_z - patch.min_z >= 1.2F;
    };
    // Past the lip (lip_x, lip_z) along (nx, nz): a top of her floor farther
    // than a body's length on, on that line.
    const auto floor_across = [&](const float lip_x, const float lip_z, const float nx, const float nz) {
        for (const traversal::Patch &other : scanned_->patches()) {
            if (!her_floor(other)) {
                continue;
            }
            const bool on_line = nx != 0.0F ? lip_z >= other.min_z && lip_z <= other.max_z
                                            : lip_x >= other.min_x && lip_x <= other.max_x;
            const float gap = nx > 0.0F ? other.min_x - lip_x : nx < 0.0F ? lip_x - other.max_x
                            : nz > 0.0F ? other.min_z - lip_z : lip_z - other.max_z;
            if (on_line && gap > 2.0F) {
                return true;
            }
        }
        return false;
    };
    std::vector<Spot> spots;
    for (const traversal::Patch &patch : scanned_->patches()) {
        if (!her_floor(patch)) {
            continue;
        }
        if (ax < patch.min_x - reach - 1.0F || ax > patch.max_x + reach + 1.0F || az < patch.min_z - reach - 1.0F ||
            az > patch.max_z + reach + 1.0F) {
            continue;
        }
        for (int side = 0; side < 4; ++side) {
            // Its -x, +x, -z and +z sides; the normal out past each.
            const float nx = side == 0 ? -1.0F : side == 1 ? 1.0F : 0.0F;
            const float nz = side == 2 ? -1.0F : side == 3 ? 1.0F : 0.0F;
            const bool along_z = side < 2;
            const float lo = (along_z ? patch.min_z : patch.min_x) + 0.6F;
            const float hi = (along_z ? patch.max_z : patch.max_x) - 0.6F;
            for (float t = lo; t <= hi + 1.0e-3F; t += 0.5F) {
                const float lip_x = side == 0 ? patch.min_x : side == 1 ? patch.max_x : t;
                const float lip_z = side == 2 ? patch.min_z : side == 3 ? patch.max_z : t;
                const float d = xz(ax, az, lip_x - nx * kRimStandM, lip_z - nz * kRimStandM);
                if (d <= reach) {
                    spots.push_back({d, lip_x, lip_z, nx, nz, floor_across(lip_x, lip_z, nx, nz)});
                }
            }
        }
    }
    // Over the open shaft first (her floor across it), the nearest; then over
    // the tower's outside. Among equals, in the map's order (the sort is
    // stable).
    std::stable_sort(spots.begin(), spots.end(), [](const Spot &a, const Spot &b) {
        return a.across != b.across ? a.across : a.d < b.d;
    });
    for (const Spot &spot : spots) {
        const float sx = spot.lip_x - spot.nx * kRimStandM;
        const float sz = spot.lip_z - spot.nz * kRimStandM;
        if (!straight_on_floor(ax, az, sx, sz, feet) || !hang_room(sx, sz, spot.nx, spot.nz, feet) ||
            !lethal_from(sx, sz, spot.nx, spot.nz, feet)) {
            continue;
        }
        rim_x_ = sx;
        rim_z_ = sz;
        rim_nx_ = spot.nx;
        rim_nz_ = spot.nz;
        fatal_rim0_ = Vec3(sx, 0.0F, sz);
        aim_ledge_ = false;
        fatal_edge_ = RVec3(spot.lip_x, feet, spot.lip_z);
        return true;
    }
    return false;
}

bool Matron::hang_room(const float sx, const float sz, const float nx, const float nz, const float feet) const {
    if (!room_to_stand(RVec3(sx, feet + kStandCentre, sz))) {
        return false;
    }
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    // Room to hang: the body's place past the side (and its swing), at his
    // window and at her let-go. (His way there is over the rail, tipped
    // across it: step_hold.)
    const float com_y = feet + kDangleHandUpM - kHungM;
    const auto clear = [&](const Shape *shape, const float out_m, const float y) {
        AnyHitCollisionCollector<CollideShapeCollector> touch;
        CollideShapeSettings settings;
        const RVec3 pose(sx + nx * out_m, y, sz + nz * out_m);
        physics_.GetNarrowPhaseQuery().CollideShape(shape, Vec3::sReplicate(1.0F), RMat44::sTranslation(pose),
                                                    settings, pose, touch, {}, {}, skip);
        return !touch.HadHit();
    };
    return clear(hang_room_shape_, kDangleOutM, com_y) && clear(hang_room_shape_, kDangleOutM, com_y + 0.8F) &&
           clear(player_cast_shape_, kFatalWindowOutM, feet + kFatalWindowUpM - kHungM) &&
           clear(player_cast_shape_, kFatalDropOutM, feet + kFatalDropUpM - kHungM);
}

// The fall from where she lets him go there (his middle kFatalOpenAirM out
// past the lip, under her hand at kFatalDropUpM), moving out with her arm's
// thrust (kFatalThrustMps) and the least of her tosses (kFatalTossMps), kills; and so from where his swing may take
// him along the edge (kFatalSwayM either side). A real arm holds him no
// further out than that (the owner, 2026-10-29): steel under the edge is
// cleared by her throw, as a body is thrown off a rail, not by an arm's
// length she has not.
bool Matron::lethal_from(const float sx, const float sz, const float nx, const float nz, const float feet) const {
    const float out = kRimStandM + kFatalOpenAirM;
    for (const float along : {0.0F, -kFatalSwayM, kFatalSwayM}) {
        const RVec3 hang(sx + nx * out + nz * along, feet + kFatalDropUpM - kHungM, sz + nz * out - nx * along);
        bool kills = false;
        for (const float toss : kFatalTossMps) {
            if (fall_impact_mps(hang, Quat::sIdentity(), Vec3(nx, 0.0F, nz) * (kFatalThrustMps + toss), Vec3::sZero(),
                                kFatalChooseMarginM) >= kFatalLandMps) {
                kills = true;
                break;
            }
        }
        if (!kills) {
            return false;
        }
    }
    return true;
}

namespace {
// What a falling body lands on: the static world and what moves in it (not
// her hand, a corpse, nor her on the pipe).
class LandingLayers final : public ObjectLayerFilter {
public:
    LandingLayers(const ObjectLayer a, const ObjectLayer b) : a_(a), b_(b) {}
    [[nodiscard]] bool ShouldCollide(const ObjectLayer layer) const override { return layer == a_ || layer == b_; }

private:
    ObjectLayer a_;
    ObjectLayer b_;
};
} // namespace

float Matron::fall_impact_mps(const RVec3 com, const Quat rot, const Vec3 v, const Vec3 w, const float margin) const {
    // His body: the player's 1.8 m capsule (its half kPlayerHalfM), its
    // breadth 0.35, the margin round it.
    CapsuleShape body(kPlayerHalfM - 0.35F, 0.35F + margin);
    body.SetEmbedded();
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    const LandingLayers landing(static_cast<ObjectLayer>(static_layer_), static_cast<ObjectLayer>(moving_layer_));
    const Vec3 g = physics_.GetGravity();
    const float bottom = float(physics_.GetBounds().mMin.GetY()) - 1.0F;
    const float spin = w.Length();
    const Vec3 axis = spin > 1.0e-4F ? w / spin : Vec3::sAxisY();
    RVec3 from = com;
    float t = 0.0F;
    for (int k = 0; k < 80; ++k) {
        // Short steps past the steel he hangs by, longer once clear of it
        // (a step's chord is within 0.08 m of the arc).
        const float step = t < 0.6F ? 0.1F : 0.25F;
        const float t1 = t + step;
        const RVec3 to = com + v * t1 + 0.5F * g * (t1 * t1);
        const Quat turned = spin > 1.0e-4F ? Quat::sRotation(axis, spin * t) * rot : rot;
        RShapeCast cast(&body, Vec3::sReplicate(1.0F), RMat44::sRotationTranslation(turned, from), Vec3(to - from));
        ShapeCastSettings settings;
        ClosestHitCollisionCollector<CastShapeCollector> hit;
        physics_.GetNarrowPhaseQuery().CastShape(cast, settings, from, hit, {}, landing, skip);
        if (hit.HadHit()) {
            // The first meeting must itself kill: a landing (a face that would
            // bear him), by its speed down.
            const float when = t + step * hit.mHit.mFraction;
            const Vec3 at_hit = v + g * when;
            const Vec3 normal = -hit.mHit.mPenetrationAxis.NormalizedOr(Vec3::sAxisY());
            // Side-on (a wall, a beam's edge) it is a graze, not a landing:
            // foreseen not to kill.
            return normal.GetY() >= 0.55F ? -at_hit.GetY() : 0.0F;
        }
        if (to.GetY() < bottom) {
            return -1.0F;
        }
        from = to;
        t = t1;
    }
    return -1.0F;
}

// The let-go foreseen not to kill: her stand a step (kFatalShiftM) along the
// edge toward the nearest spot (within kFatalShiftMaxM of her first) whose
// fall from his hang does, with room for her and him there.
bool Matron::fatal_shift(const float feet) {
    const float along_x = rim_nz_;
    const float along_z = -rim_nx_;
    for (const float off : {0.4F, -0.4F, 0.8F, -0.8F, 1.2F, -1.2F}) {
        const float sx = rim_x_ + along_x * off;
        const float sz = rim_z_ + along_z * off;
        if (std::hypot(sx - fatal_rim0_.GetX(), sz - fatal_rim0_.GetZ()) > kFatalShiftMaxM + 1.0e-3F ||
            !hang_room(sx, sz, rim_nx_, rim_nz_, feet) || !lethal_from(sx, sz, rim_nx_, rim_nz_, feet)) {
            continue;
        }
        const float step = std::copysign(std::min(std::abs(off), kFatalShiftM), off);
        rim_x_ += along_x * step;
        rim_z_ += along_z * step;
        return true;
    }
    return false;
}

bool Matron::deck_close_below(const RVec3 player, const float feet) const {
    constexpr float kBreadthM = 0.35F;   // the body's, as in spare_side
    constexpr float kDownM = 2.2F;       // his middle, lowered head first, is under 1.4 m over her deck
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    SphereShape breadth(kBreadthM);
    breadth.SetEmbedded();
    RShapeCast cast(&breadth, Vec3::sReplicate(1.0F), RMat44::sTranslation(player), Vec3(0.0F, -kDownM, 0.0F));
    ShapeCastSettings settings;
    ClosestHitCollisionCollector<CastShapeCollector> met;
    physics_.GetNarrowPhaseQuery().CastShape(cast, settings, player, met, {}, {}, skip);
    if (!met.HadHit()) {
        return false;
    }
    const float top = float(player.GetY()) - kDownM * met.mHit.mFraction - kBreadthM;
    return top <= feet + 0.15F && top >= feet - 0.6F;
}

void Matron::end_hold(const State next) {
    state_ = next;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    grip_on_ = false;
    reel_ = false;
    fatal_hang_ = false;
    pull_n_ = kHandReachPullN;
    lean_ = 0.0F;
    crouch_ = 0.0F;
    if (next == State::Searching) {
        search_s_ = 0.0F;
        search_budget_s_ = 1.0e9F;
        search_trip_s_ = 0.0F;
        since_heard_s_ = 0.0F;
        pursuit_s_ = 0.0F;
    }
    if (next == State::Shuffling) {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        pick_patrol(float(at.GetX()), float(at.GetZ()));
    }
}

void Matron::step_hold(const float dt, const RVec3 player) {
    if (player_blow_tag_) {
        // His blow landed, tagged with no blow of hers to read it with (a
        // punch too small to shift her): before the rip, it frees him; after,
        // it is spent.
        player_blow_tag_ = false;
        if (his_blow() && !holding_state(state_)) {
            return;
        }
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float feet = float(at.GetY()) - kStandCentre;
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    hold_s_ += dt;
    grip_free_ = false;
    pose_reach_ = kPoseReachM;
    if (fatal_hold_ && !fatal_dropped_) {
        fatal_s_ += dt;
    }
    const bool on_catwalk = on_catwalk_line(ax, az, 0.0F);
    // The fatality's hold: her hand's pose carried along as she walks (her
    // carry is faster than her hand's own pace); any other hold as it was.
    const Vec3 moving = bodies.GetLinearVelocity(body_);
    const Vec3 her_v = fatal_hold_ ? Vec3(moving.GetX(), 0.0F, moving.GetZ()) : Vec3::sZero();
    // A braced player (their hands on a rail): her pull grows, finite. So
    // too her haul with her weight in it, out over the rail with his arm
    // still out of her reach after a re-pose (hauled_in(): a body caught on
    // steel out there).
    if (grip_on_) {
        pull_n_ = player_braced_ || hauled_in() ? std::min(kRipMaxN, pull_n_ + kRipRampNps * dt) : kHandPullN;
    }
    const auto carry_pose = [&] {
        return RVec3(at.GetX(), feet + kCarryHandUpM, at.GetZ()) + right * kCarrySideM;
    };
    // With him in her fist, her feet change pace gently (kCarryAccel): a
    // body hung from a hand swings back as far as she accelerates.
    // (Her fatality is in a hurry: the carry to its edge at her own pace,
    // kFatalCarryAccel, his swing kept by her grip's cone.)
    const float feet_accel = !grip_on_ ? kFootAccel : fatal_hold_ ? kFatalCarryAccel : kCarryAccel;
    const auto stand = [&] { walk_toward(dt, ax, az, 0.0F, feet_accel); };
    switch (state_) {
    case State::Grab: {
        if (!grip_on_) {
            // The reach: down to the ankle (the feet end of the body, however
            // it lies), stooping for it (her lean and crouch: pose_hand).
            const RVec3 ankle = player_ankle_;
            stand();
            yaw_ = turn_to(yaw_, std::atan2(float(player.GetX()) - ax, float(player.GetZ()) - az), 7.0F, dt);
            pose_hand(dt, ankle, kHandReachSpeedMps);
            if (hold_s_ > kGrabReachSeconds || reach_to(ankle) > kArmReachM + 0.4F) {
                ++grab_misses_;
                retake_ = false;
                grab_cooldown_s_ = kGrabCooldownSeconds;
                end_hold(State::Hunting);
            }
            break;
        }
        if (fatal_hold_) {
            // His one arm left: the fatality (taken again where the world tore
            // him out of her hand, the same). She is on her way to its edge as
            // her fingers close, the body coming up in her fist as she goes
            // (Carry's pose): no time to savor it.
            retake_ = false;
            begin_fatal(ax, az, feet);
            if (state_ == State::Carry) {
                pose_hand(dt, carry_pose(), kHandSpeedMps, her_v);
            }
            break;
        }
        // Closed on it: up, the body coming off its feet head down.
        stand();
        // A slow first heave: the body coming off its feet and over.
        pose_hand(dt, carry_pose(), 0.6F * kHandSpeedMps);
        // Him hanging head down from her fist before she walks (a body
        // carried any other way swings about her fingers), or past
        // kGrabInvertMaxS of it.
        const bool hung = bodies_hung() || hold_s_ >= kGrabLiftSeconds + kGrabInvertMaxS;
        if (hold_s_ >= kGrabLiftSeconds && hung && Vec3(in_reach(carry_pose()) - hand_target_).Length() < 0.05F) {
            if (retake_) {
                // Taken again where the world tore them out of her hand: no
                // second heave. The arm here (her spot and the way to the
                // body stand in for the rail's), then back on the deck.
                retake_ = false;
                aim_ledge_ = false;
                stand_in_for_rim(at, player, fwd);
                if (!begin_armplay(State::Spare)) {
                    spare_here();
                }
                break;
            }
            if (!choose_rim(ax, az)) {
                // No rail to take them to: the arm where she stands (her spot
                // and the way to the body stand in for the rail's), then back
                // on the deck.
                aim_ledge_ = false;
                stand_in_for_rim(at, player, fwd);
                if (!begin_armplay(State::Spare)) {
                    spare_here();
                }
                break;
            }
            ++carries_;
            state_ = State::Carry;
            hold_s_ = 0.0F;
        }
        break;
    }
    case State::Carry: {
        const float off = xz(ax, az, rim_x_, rim_z_);
        // Her feet go slow till he hangs head down in her fist
        // (kCarryComingOverMps, kGrabInvertMaxS at most): a body coming over
        // swings wide about her fingers.
        const bool hung = bodies_hung(kCarryHeadDownCos) || hold_s_ >= kGrabInvertMaxS;
        if (off > kRimArriveM) {
            float wx = rim_x_;
            float wz = rim_z_;
            // The last of it straight across the band to the rail (the rail's
            // spot is off her walking lines); farther, along them.
            if (off > 6.0F) {
                route_toward(ax, az, rim_x_, rim_z_, on_catwalk, wx, wz);
            }
            const float facing = yaw_;
            const float pace = kHuntMps * (fatal_hold_ ? kFatalCarryPaceScale : kCarryPaceScale);
            walk_toward(dt, wx, wz, std::min(hung ? pace : kCarryComingOverMps, 0.6F + 2.0F * off),
                        feet_accel);
            if (!hung) {
                // (Coming over, he turns about her fingers in the plane she
                // lifts him in: she does not swing him round with her yet,
                // his legs would sweep into her.)
                yaw_ = facing;
            }
        } else {
            stand();
            const float want = std::atan2(rim_nx_, rim_nz_);
            yaw_ = turn_to(yaw_, want, kHoldTurnRate, dt);
            if (std::abs(std::remainder(want - yaw_, 2.0F * kPiF)) < 0.3F && braced_at_rim(ax, az)) {
                ++dangles_;
                state_ = State::Dangle;
                hold_s_ = 0.0F;
                hold_phase_ = 0;
                if (fatal_hold_) {
                    // The fatality's hang: its window, not a dangle's roll.
                    dangle_s_ = kFatalHangWindowS;
                    fatal_window_s_ = 0.0F;
                } else {
                    dangle_s_ = dangle_for_test_s_ >= 0.0F ? dangle_for_test_s_
                                                          : kDangleMinSeconds + kDangleSpanSeconds * roll();
                }
            }
        }
        pose_hand(dt, carry_pose(), kHandSpeedMps, her_v);
        if (fatal_hold_ && state_ == State::Carry) {
            // The fatality's edge not reached in the carry's time: the floor
            // version, where she stands.
            fatal_carry_s_ += dt;
            if (fatal_carry_s_ > kFatalCarryS) {
                fatal_floor();
            }
        } else if (hold_s_ > kCarryMaxSeconds && state_ == State::Carry) {
            // Too long with them: the arm where she stands, then set down
            // beside her on her deck (never dropped where she is: she may be
            // at an edge).
            stand_in_for_rim(at, player, fwd);
            if (!begin_armplay(State::Spare)) {
                spare_here();
            }
        }
        break;
    }
    case State::Dangle: {
        // Her feet keep her at her spot against the rail (the fatality's: her
        // front to it, kFatalStandM in, him close for his window).
        const float stand_in = fatal_hold_ ? kFatalStandM : kRimStandM;
        const float spot_x = rim_x_ + rim_nx_ * (kRimStandM - stand_in);
        const float spot_z = rim_z_ + rim_nz_ * (kRimStandM - stand_in);
        walk_toward(dt, spot_x, spot_z, std::min(0.8F, 3.0F * xz(ax, az, spot_x, spot_z)));
        yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        if (!braced_at_rim(ax, az)) {
            // Pulled off her brace: back from the rail, the body back in (the
            // fatality's carry keeps its time: fatal_carry_s_).
            ++brace_losses_;
            state_ = State::Carry;
            hold_s_ = brace_losses_ % 3 == 0 ? kCarryMaxSeconds : 0.0F;   // the third time, she gives it up
            hold_phase_ = 0;
            fatal_hang_ = false;
            break;
        }
        const Vec3 out(rim_nx_, 0.0F, rim_nz_);
        if (hold_phase_ == 0) {
            // Over the rail, tipped across it (a real arm cannot lift a
            // man over it): her fist up over the rail's line (kHeaveUpM),
            // his body leant on the rail inside; then out and down past it
            // (kTipPastM past the side, kTipUpM up), the rail under his
            // thigh, till his middle is kTipDoneM out past the side and he
            // goes over head first, turning free about her fingers
            // (grip_free_). Then down to the hang.
            grip_free_ = true;
            const RVec3 lip(rim_x_ + rim_nx_ * kRimStandM, feet, rim_z_ + rim_nz_ * kRimStandM);
            const float past = float(player.GetX() - lip.GetX()) * rim_nx_ + float(player.GetZ() - lip.GetZ()) * rim_nz_;
            const RVec3 onto = lip + out * (fatal_hold_ ? 0.0F : kHeaveOverM) + Vec3(0.0F, kHeaveUpM, 0.0F);
            const RVec3 tip = lip + out * kTipPastM + Vec3(0.0F, kTipUpM, 0.0F);
            const float tip_mps = (fatal_hold_ ? 1.0F : kTipPaceScale) * kHandSpeedMps;
            if (tip_step_ == 0) {
                pose_hand(dt, onto, kHandSpeedMps, her_v);
                if (Vec3(in_reach(onto) - hand_target_).Length() < 0.03F) {
                    tip_step_ = 1;
                }
            } else {
                pose_hand(dt, tip, tip_mps, her_v);
            }
            if (past >= kTipDoneM) {
                over_drop_ = true;
                tip_step_ = 0;
                hold_phase_ = 1;
            }
        } else if (hold_phase_ == 1) {
            // Down: the arm over the rail, the body below the deck's edge.
            const RVec3 low = RVec3(at.GetX(), feet + kDangleHandUpM, at.GetZ()) + right * kShoulderSideM + out * kDangleOutM;
            if (fatal_hold_) {
                // The fatality's: out over the edge to his window's pose, close
                // (kFatalWindowOutM out, kFatalWindowUpM up: his fist's reach
                // of her), held there kFatalHangWindowS from when he hangs
                // still within it (kFatalReachM, kFatalWindowPastM; her grip's
                // friction stills his swing); then out and down to her
                // let-go's pose (kFatalDropOutM out, kFatalDropUpM up), past the
                // rail's height only while his swing cannot bring his middle
                // back within kFatalClearM of the lip (he would lie across
                // the rail); swung in over it, up again till he is out.
                // (Straight out from her shoulder; at her let-go's pose her
                // arm all the way out over the drop: kFatalPoseReachM.)
                const RVec3 win = RVec3(at.GetX(), feet + kFatalWindowUpM, at.GetZ()) + right * kShoulderSideM +
                                  out * kFatalWindowOutM;
                const RVec3 drop = RVec3(at.GetX(), feet + kFatalDropUpM, at.GetZ()) + right * kShoulderSideM +
                                   out * kFatalDropOutM;
                const float held_past = float(player_ankle_.GetX() - fatal_edge_.GetX()) * rim_nx_ +
                                        float(player_ankle_.GetZ() - fatal_edge_.GetZ()) * rim_nz_;
                const float off = float(player.GetX() - player_ankle_.GetX()) * rim_nx_ +
                                  float(player.GetZ() - player_ankle_.GetZ()) * rim_nz_;
                const float swing_v = (bodies.GetLinearVelocity(player_) - her_v).Dot(out);
                const float omega = kSwingOmega;   // his swing under her hand
                const float inmost = held_past - std::sqrt(off * off + (swing_v / omega) * (swing_v / omega));
                const bool clear = held_past + off >= kFatalClearM;
                // (Head down in her fist: over the rail he turns over about her
                // fingers, and his window waits till he hangs.)
                const bool head_down = bodies_hung();
                if (!fatal_hang_ && head_down && held_past + off >= kFatalWindowPastM &&
                    fist_reach_m(at, player, swing_v) <= kFatalReachM) {
                    // Her arm out, him head down by the ankle out past the
                    // lip, his fist within reach of her however he swings:
                    // his window opens.
                    fatal_hang_ = true;
                }
                if (fatal_hang_) {
                    fatal_window_s_ += dt;
                }
                const bool window_over = fatal_window_s_ >= kFatalHangWindowS;
                RVec3 aim = win;
                float pace = kFatalLowerScale * kHandSpeedMps;
                if (window_over) {
                    // Out first; down past the rail's height once he is clear.
                    const RVec3 here(drop.GetX(), hand_target_.GetY(), drop.GetZ());
                    const RVec3 up_out(drop.GetX(), win.GetY(), drop.GetZ());
                    aim = inmost >= kFatalClearM ? drop : clear ? here : up_out;
                    pose_reach_ = kFatalPoseReachM;
                } else {
                    // Into his window's pose easing off as she gets there (at
                    // kFatalEaseRate of what is left, a second): his body comes
                    // to it with little swing.
                    pace = std::min(pace, kFatalEaseRate * Vec3(in_reach(aim) - hand_target_).Length() + 0.05F);
                }
                pose_hand(dt, aim, pace, her_v);
                if (window_over && fatal_s_ >= kFatalLetGoFromS && fatal_s_ <= kFatalLetGoByS &&
                    held_past + off >= kFatalOpenAirM && swing_v >= 0.0F && --fatal_check_ticks_ <= 0) {
                    // On her way out, his middle out over open air and on his
                    // swing out (or still): she lets go as soon as his fall
                    // from there, as he moves, kills.
                    fatal_check_ticks_ = 3;
                    foresee_fall(player, out);
                    if (fatal_lethal_) {
                        fatal_release();
                    }
                }
                if (state_ != State::Dangle) {
                    // Let go.
                } else if (window_over && Vec3(in_reach(drop) - hand_target_).Length() < 0.02F) {
                    hold_phase_ = 2;
                    hold_s_ = 0.0F;
                } else if (fatal_s_ >= kFatalLetGoByS) {
                    // Not out at her let-go in time: back in over the edge,
                    // set down on her deck, and her floor version there.
                    reel_ = true;
                    reel_to_floor_ = true;
                    fatal_hang_ = false;
                    state_ = State::Spare;
                    hold_phase_ = 0;
                    hold_s_ = 0.0F;
                    spare_walk_ = false;
                }
            } else {
                pose_hand(dt, low, 0.6F * kHandSpeedMps, her_v);
                if (Vec3(in_reach(low) - hand_target_).Length() < 0.02F) {
                    hold_phase_ = 2;
                    hold_s_ = 0.0F;
                    fatal_window_s_ = 0.0F;
                }
            }
        } else if (fatal_hold_) {
            // The fatality's let-go: his window over (phase 1), him upside
            // down by the ankle out over the drop at her arm's length. No
            // sooner than kFatalLetGoFromS after her fingers closed, she lets
            // go once his fall from where he is, as he moves, is foreseen to
            // kill; till then a step along the edge at a time toward one that
            // does; none by kFatalLetGoByS, she hauls him back in for her
            // floor version.
            // At her arm's end and not yet a fall that kills: she heaves him
            // back in (kFatalHeaveBackM) and thrusts him out again, letting
            // go on the thrust (his swing out: foresee_fall).
            pose_reach_ = kFatalPoseReachM;
            const RVec3 drop = RVec3(at.GetX(), feet + kFatalDropUpM, at.GetZ()) + right * kShoulderSideM +
                               out * kFatalDropOutM;
            const RVec3 heave = in_reach(drop) - out * kFatalHeaveBackM;
            const RVec3 aim = fatal_heave_back_ ? heave : drop;
            // (Back at her hand's pace; the thrust out at her reaching pace.
            // Held still out there while his fall from there already kills.)
            pose_hand(dt, aim, fatal_heave_back_ ? kHandSpeedMps : kHandReachSpeedMps, her_v);
            if (Vec3(in_reach(aim) - hand_target_).Length() < 0.02F && (fatal_heave_back_ || !fatal_lethal_)) {
                fatal_heave_back_ = !fatal_heave_back_;
            }
            const float swing_out = (bodies.GetLinearVelocity(player_) - her_v).Dot(out);
            fatal_hang_ = true;
            fatal_window_s_ += dt;
            const bool due = fatal_window_s_ >= kFatalHangWindowS && fatal_s_ >= kFatalLetGoFromS;
            if (due || --fatal_check_ticks_ <= 0) {
                fatal_check_ticks_ = 3;
                foresee_fall(player, out);
                if (!fatal_lethal_ && xz(ax, az, rim_x_, rim_z_) < 0.1F) {
                    (void)fatal_shift(feet);
                }
            }
            // His middle over open air (kFatalOpenAirM past the lip).
            const float out_past = float(player.GetX() - fatal_edge_.GetX()) * rim_nx_ +
                                   float(player.GetZ() - fatal_edge_.GetZ()) * rim_nz_;
            if (due && fatal_lethal_ && swing_out >= 0.0F && out_past >= kFatalOpenAirM && fatal_s_ <= kFatalLetGoByS) {
                fatal_release();
            } else if (fatal_s_ >= kFatalLetGoByS) {
                // No let-go that kills, in time: back in over the edge, set
                // down on her deck, and her floor version there.
                reel_ = true;
                reel_to_floor_ = true;
                fatal_hang_ = false;
                state_ = State::Spare;
                hold_phase_ = 0;
                hold_s_ = 0.0F;
                spare_walk_ = false;
            }
        } else {
            pose_hand(dt, hand_target_, kHandSpeedMps);
            if (hold_s_ >= dangle_s_) {
                // Every hold, at every stage: while they have an arm, her
                // other hand goes for it (no roll); with none left, the run
                // is hers.
                if (player_arms_left_ <= 0) {
                    claim();
                } else if (!begin_armplay(State::Dangle)) {
                    finish_dangle();
                }
            }
        }
        break;
    }
    case State::ArmPlay: {
        walk_toward(dt, rim_x_, rim_z_, std::min(0.8F, 3.0F * xz(ax, az, rim_x_, rim_z_)));
        yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        // The body hauled up in her fist against the rail's far side, its
        // arm to her other hand (her fist kHeaveOutM out past her spot,
        // however her pull rocks her back: she leans into it).
        // (Over the drop after a re-pose, her spot a step along the rail: the
        // body dragged after her along it and in against the rail, her pull
        // rising, capped: hauled_in().)
        pose_hand(dt,
                  RVec3(rim_x_, feet + kArmPlayUpM, rim_z_) +
                      Vec3(rim_nx_, 0.0F, rim_nz_) * (hauled_in() ? kRimStandM : kHeaveOutM),
                  0.6F * kHandSpeedMps);
        step_armplay(dt, player, at, feet);
        break;
    }
    case State::Claimed:
        step_claimed(dt, at, feet);
        break;
    case State::Spare: {
        const Vec3 out(rim_nx_, 0.0F, rim_nz_);
        if (spare_walk_) {
            // Her deck not close under him where she would let go: she walks
            // him on, away from the edge, her face still to it.
            const float facing_edge = yaw_;
            walk_toward(dt, spare_to_x_, spare_to_z_, 0.8F);
            yaw_ = turn_to(facing_edge, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
            spare_walk_ = xz(ax, az, spare_to_x_, spare_to_z_) > 0.1F;
        } else if (reel_ || (hold_phase_ == 0 && over_drop_ && tip_step_ == 1)) {
            // Reeling from his blow (or hauling him back in, over the rail):
            // her feet catch her kReelBackM back from the edge, her face still
            // to it, him still in her fist.
            const float facing_edge = yaw_;
            // (Over the rail with him, a slow step, kTipBackMps: he comes in
            // across it with her.)
            const bool tipping = !reel_;
            // Her arm already at its length with his weight (caught, or
            // heavy on it): she does not step on and drag him through it; her
            // capped pull brings him, and she steps as her hand comes in.
            const bool arm_full =
                tipping && Vec3(player_ankle_ - (at + shoulder_local())).Length() >= kArmReachM - 0.02F;
            if (arm_full) {
                stand();
            } else {
                walk_toward(dt, rim_x_ - rim_nx_ * kReelBackM, rim_z_ - rim_nz_ * kReelBackM,
                            tipping ? kTipBackMps : 1.2F, feet_accel);
            }
            yaw_ = turn_to(facing_edge, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        } else {
            stand();
        }
        // Reeling, her arm comes in at its full pace.
        const float in_pace = reel_ ? 1.0F : 0.35F;
        const float down_pace = reel_ ? 1.0F : 0.5F;
        if (hold_phase_ == 0) {
            // Out over the drop: back in over the rail the way he went out,
            // tipped across it: her fist up over the rail's line, then in
            // (kTipBackFwdM in from the side, kTipBackUpM up) as she steps
            // back, the rail under him, till his middle is kTipDoneM in
            // from the side; on her deck already, straight on.
            if (over_drop_) {
                grip_free_ = true;
                const RVec3 lip(rim_x_ + rim_nx_ * kRimStandM, feet, rim_z_ + rim_nz_ * kRimStandM);
                const float past =
                    float(player.GetX() - lip.GetX()) * rim_nx_ + float(player.GetZ() - lip.GetZ()) * rim_nz_;
                const RVec3 onto = lip + Vec3(0.0F, kHeaveUpM, 0.0F);
                const RVec3 in = lip - out * kTipBackFwdM + Vec3(0.0F, kTipBackUpM, 0.0F);
                if (tip_step_ == 0) {
                    pose_hand(dt, onto, in_pace * kHandSpeedMps);
                    if (Vec3(in_reach(onto) - hand_target_).Length() < 0.03F) {
                        tip_step_ = 1;
                    }
                } else {
                    pose_hand(dt, in, kTipPaceScale * kHandSpeedMps);
                }
                if (past <= -kTipDoneM) {
                    over_drop_ = false;
                    tip_step_ = 0;
                }
            }
            if (!over_drop_) {
                hold_phase_ = 1;
            }
            spare_side_set_ = false;
        } else if (hold_phase_ == 1) {
            // Down beside her till their head is on the deck: on the side of
            // her with her deck under it (spare_side), chosen as she starts.
            if (!spare_side_set_) {
                spare_side_ = spare_side(at, feet, right, out, spare_deck_);
                spare_side_set_ = true;
            }
            const RVec3 down = RVec3(at.GetX(), feet + kHungM + kPlayerHalfM + 0.1F, at.GetZ()) + spare_side_;
            pose_hand(dt, down, down_pace * kHandSpeedMps);
            // Over the deck (her deck under the spot, or in from the rail),
            // the head on it: let go. Never over the drop: past the time, back
            // up and over again; tried once already, her deck under them, she
            // lets go once her hand is there (a body hung against her short
            // of it). Whichever: only with her deck found close under his
            // body; not, she keeps hold and walks him on (spare_walk_).
            const float rail_in = (rim_x_ + rim_nx_ * kRimStandM - float(player.GetX())) * rim_nx_ +
                                  (rim_z_ + rim_nz_ * kRimStandM - float(player.GetZ())) * rim_nz_;
            const bool ready = !spare_walk_ && Vec3(in_reach(down) - hand_target_).Length() < 0.02F &&
                               (spare_deck_ || rail_in > 0.5F) &&
                               (player.GetY() < feet + 1.2 || (spare_tries_ > 0 && spare_deck_));
            if (ready && !deck_close_below(player, feet)) {
                spare_walk_ = true;
                spare_to_x_ = ax - rim_nx_ * 0.5F;
                spare_to_z_ = az - rim_nz_ * 0.5F;
            } else if (ready) {
                let_go_ = true;
                let_go_push_ = Vec3::sZero();
                grip_on_ = false;
                hold_phase_ = 2;
                hold_s_ = 0.0F;
                if (reel_to_floor_) {
                    // Hauled back in, no lethal let-go in time: her floor
                    // version, where he lies.
                    reel_ = false;
                    reel_to_floor_ = false;
                    fatal_floor();
                    break;
                }
                if (reel_) {
                    // Set down after his blow: her stagger, then after him.
                    end_reel(player);
                    break;
                }
            } else if (hold_s_ > 8.0F) {
                ++spare_tries_;
                hold_phase_ = 0;
                hold_s_ = 0.0F;
            }
        } else {
            // The collar put straight; then she walks off.
            const RVec3 collar = player + RVec3(0.0, 0.5, 0.0);
            pose_hand(dt, collar, 0.5F * kHandSpeedMps);
            if (hold_s_ >= kSpareSetSeconds) {
                // She walks off; an arm torn this hold, her hands stay off
                // him kRegrabAfterTearS.
                calm_s_ = kSpareCalmSeconds;
                grab_cooldown_s_ = std::max(kSpareCalmSeconds, hands_off_after_hold());
                end_hold(State::Shuffling);
            }
        }
        break;
    }
    case State::Release: {
        stand();
        bool over = hold_s_ >= kReleasePoseSeconds;
        if (fatal_dropped_) {
            // Let go over the drop: she watches him go down it (the death and
            // the restore end it), till his fall is over (him still
            // kFatalStillS, kFatalWatchS at least); then after him.
            yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
            const float y = float(player.GetY());
            watch_still_s_ = std::abs(y - watch_y_) < 0.3F * dt ? watch_still_s_ + dt : 0.0F;
            watch_y_ = y;
            over = (hold_s_ >= kFatalWatchS && watch_still_s_ >= kFatalStillS) || hold_s_ >= kFatalWatchMaxS;
        }
        if (over) {
            take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, Sense::Touch);
            end_hold(State::Searching);
        }
        break;
    }
    default:
        break;
    }
}

// The dangle over: back up over the rail and set down on her deck (Spare),
// every time (the owner, 2026-10-28: ordinary drops never happen over open
// air; a ledge below is no longer let go onto). Her roll that once chose a
// spare is still made where it was (and not when a test fixes the choice),
// so every roll of hers after it is the one it was.
void Matron::finish_dangle() {
    if (spare_for_test_ < 0) {
        (void)roll();
    }
    ++spares_;
    state_ = State::Spare;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    spare_walk_ = false;
}

void Matron::wrist_taken() noexcept {
    if (state_ == State::ArmPlay && hold_phase_ == 0) {
        wrist_on_ = true;
        wrist_taken_at_ = player_wrist_;
        hold_phase_ = 1;
        hold_s_ = 0.0F;
        pull2_n_ = kArmPullStartN;
    }
}

void Matron::arm_torn() noexcept {
    if (state_ == State::ArmPlay && hold_phase_ == 1) {
        ++arm_tears_;
        // The world's count is the truth (set_player_body, every tick, from
        // the arms it has torn); this keeps hers right until it next tells
        // her: a grip torn later this same tick (grip_lost) reads it.
        player_arms_left_ = std::max(0, player_arms_left_ - 1);
        hold_phase_ = 2;
        hold_s_ = 0.0F;
        pull2_n_ = kHandPullN;
    }
}

// ArmPlay's beats, her second hand: 0 to the wrist, 1 the pull, then (the
// arm hers) 2 up to her face, 3 the swing, 4 the throw and its follow-through.
void Matron::step_armplay(const float dt, const RVec3 player, const RVec3 at, const float feet) {
    // Her hand's poses are kept in her reach by pose_hand2 (in_reach).
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    const auto reach = [](const RVec3 want) { return want; };
    switch (hold_phase_) {
    case 0:
        // To the wrist. Not at it in kArmReachSeconds (out of her reach: the
        // body caught on steel, or hung past her arm), she re-poses and goes
        // for it again: she does not give it up.
        pull2_n_ = kHandReachPullN;
        pose_hand2(dt, reach(player_wrist_), kHandReachSpeedMps);
        if (hold_s_ > kArmReachSeconds) {
            arm_repose(at, player, true);
        }
        break;
    case 1: {
        // Away from the body, up and out: the shoulder takes it.
        pull2_n_ = std::min(kArmPullMaxN, pull2_n_ + kRipRampNps * dt);
        if (armplay_for_test_ == 2 || armplay_for_test_ == 3) {
            // A test's arm that will not come: her pull kept under the hold.
            pull2_n_ = std::min(pull2_n_, 0.5F * kArmHoldN);
        }
        // Wrenched in to her chest, past it: her other fist keeps him out
        // past the rail by the thigh, the rail (or her own body) stops his,
        // and her muscle (capped) puts the pull on the arm.
        const RVec3 chest = at + Vec3(0.0F, kShoulderUpM - kStandCentre - 0.3F, 0.0F) - fwd * 0.3F;
        pose_hand2(dt, chest, 8.0F * kArmPullMoveMps);
        // No give-up: the shoulder not torn kArmPullSeconds into the pull
        // (her pull spent on her own arm's reach, not on it), she re-poses
        // with the wrist in her hand, her weight put into it.
        if (hold_s_ > kArmPullSeconds) {
            arm_repose(at, player, false);
        }
        break;
    }
    case 2:
        // Up to her face, to look at it.
        pose_hand2(dt, reach(RVec3(at.GetX(), feet + kShoulderUpM + 0.25F, at.GetZ()) + fwd * 0.35F), kHandSpeedMps);
        if (hold_s_ > 1.5F) {
            hold_phase_ = 3;
        }
        break;
    case 3: {
        const float sway = std::sin((hold_s_ - 1.5F) * 6.0F) * 0.25F;
        pose_hand2(dt, reach(RVec3(at.GetX(), feet + kShoulderUpM + 0.1F, at.GetZ()) + fwd * 0.4F + right * sway),
                   kHandSpeedMps);
        if (hold_s_ > 3.0F) {
            hold_phase_ = 4;
        }
        break;
    }
    case 4: {
        // Back over her shoulder and thrown, behind her onto the deck.
        pose_hand2(dt, reach(RVec3(at.GetX(), feet + kShoulderUpM + 0.5F, at.GetZ()) - fwd * 0.3F), 2.0F * kHandSpeedMps);
        if (hold_s_ > 3.5F && !arm_throw_ && arm_throws_ < arm_tears_) {
            ++arm_throws_;
            arm_throw_ = true;
            // Along the band beside her, low: it lands on her deck.
            arm_throw_v_ = (right * 0.95F + Vec3(0.0F, 0.3F, 0.0F)).Normalized() * kArmThrowMps;
        }
        if (hold_s_ >= kArmPlaySeconds) {
            hold_phase_ = 5;
        }
        break;
    }
    default:
        break;
    }
    if (hold_phase_ == 5) {
        armplay_done_ = true;
        if (player_arms_left_ <= 0) {
            // The last arm gone: at any stage, she finishes them (Claimed).
            claim();
        } else if (!grip_on_) {
            // The body out of her ankle hand (it pulled away, the arm left in
            // her other): one-armed and alive, away from her. The hold over:
            // her hand out a moment, then she is after them again, by sound,
            // her hands off them kRegrabAfterTearS.
            state_ = State::Release;
            hold_s_ = 0.0F;
            hold_phase_ = 0;
            grab_cooldown_s_ = hands_off_after_hold();
        } else if (armplay_then_ == State::Dangle) {
            // Back down over the drop, then the dangle's end.
            state_ = State::Dangle;
            hold_phase_ = 1;
            hold_s_ = 0.0F;
            dangle_s_ = 0.8F;
        } else {
            // No rail (or carried too long): set back on the deck beside her.
            spare_here();
        }
    }
}

// Her other hand for an arm (ArmPlay), when they have one and this hold has
// not had it (and no test has her leave it); `then` follows it. False when
// there is none to take.
bool Matron::begin_armplay(const State then) {
    if (armplay_for_test_ == 0 || player_arms_left_ <= 0 || armplay_done_) {
        return false;
    }
    ++armplays_;
    state_ = State::ArmPlay;
    armplay_then_ = then;
    armplay_reposes_ = 0;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    wrist_on_ = false;
    pull2_n_ = kHandReachPullN;
    hand2_target_ = physics_.GetBodyInterface().GetPosition(body_) + shoulder2_local();
    hand2_target_v_ = Vec3::sZero();
    return true;
}

// No rail for the arm (none to walk to, or not reached in time): her own spot
// stands in for the rail's, and the way to the body for the way out over the
// drop, so it is hauled up where it already hangs.
void Matron::stand_in_for_rim(const RVec3 at, const RVec3 player, const Vec3 fwd) noexcept {
    rim_x_ = float(at.GetX());
    rim_z_ = float(at.GetZ());
    Vec3 to(float(player.GetX() - at.GetX()), 0.0F, float(player.GetZ() - at.GetZ()));
    to = to.LengthSq() > 1.0e-4F ? to.Normalized() : fwd;
    rim_nx_ = to.GetX();
    rim_nz_ = to.GetZ();
}

// ArmPlay not getting there (step_armplay): the wrist out of her reach
// (`reseat`: her hand goes for the nearer whole arm again, the world told by
// take_wrist_reseat), or her pull not telling on the shoulder (the wrist
// kept, and its strain). Either way a step of kArmReposeStepM back from the
// body, the way to it her new "out" (as where there is no rail): she hauls
// it after her and up, her weight in it; each time turned another way
// (straight back, then 60 degrees to one side, then to the other), so a body
// caught on steel is dragged off it. No floor there for her feet: she stays
// where she is.
void Matron::arm_repose(const RVec3 at, const RVec3 player, const bool reseat) {
    if (armplay_reposes_ >= (armplay_for_test_ == 3 ? 0 : kMaxArmReposes)) {
        // Past her re-poses, the arm still not hers (Cory, 2026-10-28): she
        // keeps the ankle and sets them down on her deck (Spare: only with her
        // deck close under him, else she walks him on: spare_walk_), never over
        // the drop. The arm on, no loop on the steel.
        armplay_done_ = true;
        wrist_on_ = false;
        ++spares_;
        state_ = State::Spare;
        hold_s_ = 0.0F;
        hold_phase_ = 0;
        spare_side_set_ = false;
        pick_patrol(float(at.GetX()), float(at.GetZ()));
        return;
    }
    ++arm_reposes_;
    const int k = armplay_reposes_++;
    hold_s_ = 0.0F;
    if (reseat) {
        wrist_reseat_ = true;
        wrist_on_ = false;
        hold_phase_ = 0;
        pull2_n_ = kHandReachPullN;
    }
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    Vec3 away(float(at.GetX() - player.GetX()), 0.0F, float(at.GetZ() - player.GetZ()));
    away = away.LengthSq() > 1.0e-4F ? away.Normalized() : -fwd;
    const float turn = k % 3 == 0 ? 0.0F : (k % 3 == 1 ? 1.0F : -1.0F) * (kPiF / 3.0F);
    if (over_drop_) {
        // Him out over the rail in her fist: not back (that would drag him
        // into the rail) but a step along it, one way then the other, her
        // pull rising as she goes (hauled_in()): a body caught on steel out
        // there is dragged along off it. Her spot only where her deck is.
        const Vec3 along(-rim_nz_, 0.0F, rim_nx_);
        IgnoreMultipleBodiesFilter skip_along;
        skip_along.IgnoreBody(body_);
        skip_along.IgnoreBody(player_);
        for (const float way : {k % 2 == 0 ? 1.0F : -1.0F, k % 2 == 0 ? -1.0F : 1.0F}) {
            const RVec3 step = RVec3(rim_x_, at.GetY(), rim_z_) + along * (way * kArmReposeStepM);
            RayCastResult under;
            if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(step, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), under,
                                                       {}, {}, skip_along)) {
                rim_x_ = float(step.GetX());
                rim_z_ = float(step.GetZ());
                break;
            }
        }
        return;
    }
    RVec3 spot = at + (Quat::sRotation(Vec3::sAxisY(), turn) * away) * kArmReposeStepM;
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    RayCastResult floor;
    if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(spot, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor, {},
                                                {}, skip)) {
        spot = at;
    }
    stand_in_for_rim(spot, player, fwd);
}

// Where she sets them down beside her (Spare): at her right, a little back
// from the way out, else (that spot taken) the first of the ways round her
// with her deck under the whole breadth of the body (her own floor or a step
// below it, met first: not a rail, a post, a kerb or a crate, and not the
// drop); `deck` false when none has, and her right it is.
Vec3 Matron::spare_side(const RVec3 at, const float feet, const Vec3 right, const Vec3 out, bool &deck) const {
    const Vec3 first = right * 0.6F - out * 0.4F;
    constexpr float kBreadthM = 0.35F;   // the body's, as in choose_rim
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    SphereShape breadth(kBreadthM);
    breadth.SetEmbedded();
    const auto on_deck = [&](const float y) { return y <= feet + 0.15F && y >= feet - 0.6F; };
    deck = true;
    for (int k = 0; k < 8; ++k) {
        // Her right first; then round her an eighth of a turn at a time,
        // a little farther out (clear of her).
        const Vec3 side =
            k == 0 ? first : Quat::sRotation(Vec3::sAxisY(), 0.25F * kPiF * static_cast<float>(k)) * first.Normalized() * 0.85F;
        const RVec3 from(at.GetX() + side.GetX(), feet + 1.2F, at.GetZ() + side.GetZ());
        RayCastResult under;
        if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -2.0F, 0.0F)), under, {}, {}, skip) ||
            !on_deck(feet + 1.2F - 2.0F * under.mFraction)) {
            continue;
        }
        RShapeCast cast(&breadth, Vec3::sReplicate(1.0F), RMat44::sTranslation(from), Vec3(0.0F, -2.0F, 0.0F));
        ShapeCastSettings settings;
        ClosestHitCollisionCollector<CastShapeCollector> met;
        physics_.GetNarrowPhaseQuery().CastShape(cast, settings, from, met, {}, {}, skip);
        if (met.HadHit() && on_deck(feet + 1.2F - 2.0F * met.mHit.mFraction - kBreadthM)) {
            return side;
        }
    }
    deck = false;
    return first;
}

// Set down beside her where she stands: no rail to take them to, or carried
// too long on the way to it (once dropped where she was, which could be at an
// edge: never over open air now).
void Matron::spare_here() noexcept {
    ++spares_;
    state_ = State::Spare;
    hold_s_ = 0.0F;
    hold_phase_ = 1;
    spare_walk_ = false;
}

// His body up in her fist at the second catch: to the fatality's edge if
// there is one in reach (Carry, then the hang), else the floor version here.
void Matron::begin_fatal(const float ax, const float az, const float feet) {
    if (choose_fatal_edge(ax, az, feet)) {
        fatal_path_ = FatalPath::Hang;
        ++carries_;
        state_ = State::Carry;
        hold_s_ = 0.0F;
        hold_phase_ = 0;
        fatal_carry_s_ = 0.0F;
        fatal_check_ticks_ = 0;
        fatal_lethal_ = false;
        fatal_toss_ = 0.0F;
        return;
    }
    fatal_floor();
}

// No edge for the hang in reach: the fatality on her floor, where she stands
// (Claimed's short beats, matron_fatality.cpp).
void Matron::fatal_floor() noexcept {
    fatal_path_ = FatalPath::Floor;
    fatal_hang_ = false;
    claim();
}

// Her fingers open over the drop (or he is out of them there): gravity does
// the rest. The release beat rises: the viewer's slow motion, its pull-out
// down the drop and its fade. The landing kills him (the world's death and
// the checkpoint's restore); she stands at the edge and watches him go.
void Matron::fatal_release() noexcept {
    ++fatal_release_beat_;
    if (grip_on_) {
        // Her arm's toss out over the drop with it (the least the foreseen
        // fall needed: kFatalTossMps).
        let_go_ = true;
        let_go_push_ = Vec3(rim_nx_, 0.0F, rim_nz_) * fatal_toss_;
    }
    grip_on_ = false;
    fatal_hang_ = false;
    fatal_dropped_ = true;
    watch_still_s_ = 0.0F;
    watch_y_ = 0.0F;
    state_ = State::Release;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    grab_cooldown_s_ = kAfterReleaseSeconds;
}

// His own blow landed on her in her hold, before the rip (his punch's tag:
// step_hold, or take_blow with the blow it is part of). Cory, 2026-10-27, and
// the owner's fatality, 2026-10-28: it frees him, however small (a punch is
// under the blow that takes her balance): her hand opens, a flinch. Over her
// deck, he drops onto it at once; out over the drop (the dangle, the
// fatality's hang, the way back in over the edge, or anywhere her deck is
// not under him) she reels back from the edge, him still in her fist, and
// sets him down on her deck first: a body is let go over open air only by
// the fatality's release. In the fatality's hold it is his escape: alive,
// one-armed, on her deck. After the rip has begun, nothing frees him.
bool Matron::his_blow() {
    if (!body_in_hold() || rip_begun() || reel_) {
        return false;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const RVec3 player = bodies.GetPosition(player_);
    const float feet = float(at.GetY()) - kStandCentre;
    ++reels_;
    if (fatal_hold_) {
        ++fatal_escapes_;
    }
    fatal_hang_ = false;
    if (state_ != State::Dangle && over_her_deck(player, feet)) {
        let_go_ = true;
        let_go_push_ = Vec3::sZero();
        grip_on_ = false;
        end_reel(player);
        return true;
    }
    begin_reel(at, player);
    return true;
}

// Her deck all round under him: the first thing under his body's breadth,
// swept down from its middle and from a body's half length about it (the
// way a body dropped head first topples), is her own floor or a step below
// it: not a rail, a kerb, a crate or the drop.
bool Matron::over_her_deck(const RVec3 player, const float feet) const {
    constexpr float kBreadthM = 0.35F;   // the body's, as in spare_side
    constexpr float kDownM = 3.0F;
    constexpr float kToppleM = 1.0F;
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    SphereShape breadth(kBreadthM);
    breadth.SetEmbedded();
    for (const Vec3 off : {Vec3::sZero(), Vec3(kToppleM, 0.0F, 0.0F), Vec3(-kToppleM, 0.0F, 0.0F),
                           Vec3(0.0F, 0.0F, kToppleM), Vec3(0.0F, 0.0F, -kToppleM)}) {
        const RVec3 from = player + off;
        RShapeCast cast(&breadth, Vec3::sReplicate(1.0F), RMat44::sTranslation(from), Vec3(0.0F, -kDownM, 0.0F));
        ShapeCastSettings settings;
        ClosestHitCollisionCollector<CastShapeCollector> met;
        physics_.GetNarrowPhaseQuery().CastShape(cast, settings, from, met, {}, {}, skip);
        if (!met.HadHit()) {
            return false;
        }
        const float top = float(from.GetY()) - kDownM * met.mHit.mFraction - kBreadthM;
        if (top > feet + 0.15F || top < feet - 0.6F) {
            return false;
        }
    }
    return true;
}

// Reeling from his blow with him where her deck is not under him: out over
// the drop, back up over the edge first (Spare from its start); at her side
// (a grab, a carry), down beside her where her deck is (Spare's lowering, her
// spot and the way to him standing in for the edge's). Spare's set-down at
// her arm's full pace, her feet stepping back; then end_reel.
void Matron::begin_reel(const RVec3 at, const RVec3 player) {
    reel_ = true;
    spare_walk_ = false;
    if (state_ == State::Dangle) {
        state_ = State::Spare;
        hold_phase_ = 0;
        hold_s_ = 0.0F;
    } else if (state_ != State::Spare) {
        stand_in_for_rim(at, player, Vec3(std::sin(yaw_), 0.0F, std::cos(yaw_)));
        state_ = State::Spare;
        hold_phase_ = 1;
        hold_s_ = 0.0F;
        spare_side_set_ = false;
    }
}

// Set down on her deck after his blow: her feet catch her up a moment
// (Staggering), then she is after him by sound, her hands off him
// kRegrabAfterTearS.
void Matron::end_reel(const RVec3 player) {
    take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, Sense::Touch);
    // Freed: no fatality to show (its path, its edge, and the way out over
    // its drop).
    fatal_path_ = FatalPath::None;
    fatal_edge_ = RVec3::sZero();
    end_hold(State::Searching);
    grab_cooldown_s_ = kRegrabAfterTearS;
    resume_state_ = State::Searching;
    state_ = State::Staggering;
    stagger_s_ = kStaggerMinSeconds;
}

// The last arm gone: the run is hers (Claimed, matron_fatality.cpp), at every
// stage; and the fatality's floor version. The body still in her hand, she
// lays it down where she stands (Lay); out of it, she goes to it where it
// lies (Pin).
void Matron::claim() noexcept {
    ++claims_;
    state_ = State::Claimed;
    hold_phase_ = 0;
    player_arms_left_ = 0;
    fatal_next(static_cast<int>(grip_on_ ? Fatal::Lay : Fatal::Pin));
}

// Her grip torn by the world before their arm (grip_lost): her hand goes
// for the ankle again where they land, at once; closed on it, the arm comes
// off where she stands (step_hold, Grab).
void Matron::retake() noexcept {
    ++retakes_;
    ++hold_retakes_;
    retake_ = true;
    spare_side_set_ = false;
    state_ = State::Grab;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    pull_n_ = kHandReachPullN;
}

bool Matron::claim_armless() noexcept {
    player_arms_left_ = 0;
    if (!body_in_world_ || dead_) {
        return false;
    }
    // Her arm play runs on to it (matron_hold.cpp); knocked about, she
    // claims it once she is on her feet again.
    if (state_ != State::Claimed && state_ != State::ArmPlay && state_ != State::Staggering &&
        state_ != State::Down) {
        claim();
    }
    return true;
}

void Matron::pose_hand2(const float dt, const RVec3 target, const float speed) {
    // Reaching for his wrist (or the fatality's trip handle), this hand's
    // reach leads her lean (pose_hand).
    if (reaching_wrist() || claimed()) {
        hand2_leads_ = true;
        hand2_want_ = target;
    }
    const Vec3 to = Vec3(target - hand2_target_);
    const float length = to.Length();
    const float step = speed * dt;
    const RVec3 before = hand2_target_;
    // Pulling his arm, her muscle draws on past her reach: her arm's limit
    // holds her hand, and the two put the pull on the arm.
    const RVec3 next = length <= step ? target : hand2_target_ + to * (step / length);
    hand2_target_ = pulling_wrist() ? next : in_reach(next, -1.0F);
    hand2_target_v_ = Vec3(hand2_target_ - before) / std::max(dt, 1.0e-4F);
}

} // namespace scraperx::sim
