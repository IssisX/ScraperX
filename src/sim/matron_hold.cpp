// The Matron's hold (matron.hpp, "Her hold"), at every stage: her hand goes
// for the player's ankle, closes on it, she walks them upside down to the
// shaft's rail, holds them out over the drop, tears an arm off (ArmPlay),
// and then lets go -- aimed at a ledge whose fall will not kill them -- or
// sets them back on the deck. The last arm gone, the run is hers (Claimed).
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
} // namespace

Vec3 Matron::shoulder_local() const noexcept { return Vec3(0.0F, kShoulderUpM - kStandCentre, 0.0F); }

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
    pull_n_ = kHandReachPullN;
    arm_reach_m_ = kStoopReachM;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    hand_target_ = at + shoulder_local();
    hand_target_v_ = Vec3::sZero();
    take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, true);
}

void Matron::grip_taken() noexcept {
    if (state_ == State::Grab && !grip_on_) {
        grip_on_ = true;
        grip_was_on_ = true;
        ++grabs_;
        hold_s_ = 0.0F;
        pull_n_ = kHandPullN;
    }
}

void Matron::grip_lost(const bool torn) noexcept {
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
        if (player_arms_left_ <= 0) {
            // Torn out of her hand with no arm left: the run is hers all
            // the same.
            claim();
            return;
        }
        // Torn out of her hand (or the hold given up): the hand stays out a
        // moment, then she is after them again.
        state_ = State::Release;
        hold_s_ = 0.0F;
        hold_phase_ = 0;
        grab_cooldown_s_ = kAfterReleaseSeconds;
    }
}

// Her hand's pose moves toward `target` no faster than `speed`.
void Matron::pose_hand(const float dt, const RVec3 target, const float speed) {
    const Vec3 to = Vec3(target - hand_target_);
    const float length = to.Length();
    const float step = speed * dt;
    const RVec3 before = hand_target_;
    hand_target_ = length <= step ? target : hand_target_ + to * (step / length);
    hand_target_v_ = Vec3(hand_target_ - before) / std::max(dt, 1.0e-4F);
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
            const float com_y = floor_y + kDangleHandUpM - kPlayerHalfM;
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

void Matron::end_hold(const State next) {
    state_ = next;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    grip_on_ = false;
    pull_n_ = kHandReachPullN;
    arm_reach_m_ = kArmReachM;
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
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float feet = float(at.GetY()) - kStandCentre;
    const RVec3 shoulder = at + shoulder_local();
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    hold_s_ += dt;
    const bool on_catwalk = on_catwalk_line(ax, az, 0.0F);
    // A braced player (their hands on a rail): her pull grows, finite.
    if (grip_on_) {
        pull_n_ = player_braced_ ? std::min(kRipMaxN, pull_n_ + kRipRampNps * dt) : kHandPullN;
        arm_reach_m_ = std::max(kArmReachM, arm_reach_m_ - kArmRetractMps * dt);
    }
    const auto carry_pose = [&] {
        return RVec3(at.GetX(), feet + kCarryHandUpM, at.GetZ()) + right * kCarrySideM;
    };
    const auto stand = [&] { walk_toward(dt, ax, az, 0.0F); };
    switch (state_) {
    case State::Grab: {
        if (!grip_on_) {
            // The reach: down to the ankle, stooping for it.
            const RVec3 ankle = player - RVec3(0.0, kPlayerHalfM - 0.08, 0.0);
            Vec3 to = Vec3(ankle - shoulder);
            const float length = to.Length();
            if (length > kStoopReachM) {
                to *= kStoopReachM / length;
            }
            stand();
            yaw_ = turn_to(yaw_, std::atan2(float(player.GetX()) - ax, float(player.GetZ()) - az), 7.0F, dt);
            pose_hand(dt, shoulder + to, kHandReachSpeedMps);
            if (hold_s_ > kGrabReachSeconds || length > kStoopReachM + 0.4F) {
                ++grab_misses_;
                grab_cooldown_s_ = kGrabCooldownSeconds;
                end_hold(State::Hunting);
            }
            break;
        }
        // Closed on it: up, the body coming off its feet head down.
        stand();
        // A slow first heave: the body coming off its feet and over.
        pose_hand(dt, carry_pose(), 0.6F * kHandSpeedMps);
        if (hold_s_ >= kGrabLiftSeconds && Vec3(carry_pose() - hand_target_).Length() < 0.05F) {
            if (!choose_rim(ax, az)) {
                // No rail to take them to: the arm where she stands (her spot
                // and the way to the body stand in for the rail's), then back
                // on the deck.
                aim_ledge_ = false;
                stand_in_for_rim(at, player, fwd);
                if (!begin_armplay(State::Spare)) {
                    ++spares_;
                    state_ = State::Spare;
                    hold_s_ = 0.0F;
                    hold_phase_ = 1;
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
        if (off > kRimArriveM) {
            float wx = rim_x_;
            float wz = rim_z_;
            // The last of it straight across the band to the rail (the rail's
            // spot is off her walking lines); farther, along them.
            if (off > 6.0F) {
                route_toward(ax, az, rim_x_, rim_z_, on_catwalk, wx, wz);
            }
            walk_toward(dt, wx, wz, std::min(kHuntMps * kCarryPaceScale, 0.6F + 2.0F * off));
        } else {
            stand();
            const float want = std::atan2(rim_nx_, rim_nz_);
            yaw_ = turn_to(yaw_, want, kHoldTurnRate, dt);
            if (std::abs(std::remainder(want - yaw_, 2.0F * kPiF)) < 0.3F && braced_at_rim(ax, az)) {
                ++dangles_;
                state_ = State::Dangle;
                hold_s_ = 0.0F;
                hold_phase_ = 0;
                dangle_s_ = dangle_for_test_s_ >= 0.0F ? dangle_for_test_s_
                                                      : kDangleMinSeconds + kDangleSpanSeconds * roll();
            }
        }
        pose_hand(dt, carry_pose(), kHandSpeedMps);
        if (hold_s_ > kCarryMaxSeconds && state_ == State::Carry) {
            // Too long with them: the arm where she stands, then dropped
            // where she is.
            stand_in_for_rim(at, player, fwd);
            if (!begin_armplay(State::Release)) {
                carry_drop();
            }
        }
        break;
    }
    case State::Dangle: {
        // Her feet keep her at her spot against the rail.
        walk_toward(dt, rim_x_, rim_z_, std::min(0.8F, 3.0F * xz(ax, az, rim_x_, rim_z_)));
        yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        if (!braced_at_rim(ax, az)) {
            // Pulled off her brace: back from the rail, the body back in.
            ++brace_losses_;
            state_ = State::Carry;
            hold_s_ = brace_losses_ % 3 == 0 ? kCarryMaxSeconds : 0.0F;   // the third time, she gives it up
            hold_phase_ = 0;
            break;
        }
        const Vec3 out(rim_nx_, 0.0F, rim_nz_);
        if (hold_phase_ == 0) {
            // The heave: up over her head and out over the rail.
            const RVec3 over = RVec3(at.GetX(), feet + kCarryHandUpM + 0.35F, at.GetZ()) + out * kHeaveOutM;
            pose_hand(dt, over, kHandSpeedMps);
            if (Vec3(over - hand_target_).Length() < 0.02F) {
                hold_phase_ = 1;
            }
        } else if (hold_phase_ == 1) {
            // Down: the arm over the rail, the body below the deck's edge.
            const RVec3 low = RVec3(at.GetX(), feet + kDangleHandUpM, at.GetZ()) + out * kDangleOutM;
            pose_hand(dt, low, 0.6F * kHandSpeedMps);
            if (Vec3(low - hand_target_).Length() < 0.02F) {
                hold_phase_ = 2;
                hold_s_ = 0.0F;
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
                    finish_dangle(player);
                }
            }
        }
        break;
    }
    case State::ArmPlay: {
        walk_toward(dt, rim_x_, rim_z_, std::min(0.8F, 3.0F * xz(ax, az, rim_x_, rim_z_)));
        yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        // The body hauled up in her fist, its arms to her other hand.
        pose_hand(dt, RVec3(at.GetX(), feet + kCarryHandUpM + 0.35F, at.GetZ()) + Vec3(rim_nx_, 0.0F, rim_nz_) * kHeaveOutM,
                  0.6F * kHandSpeedMps);
        step_armplay(dt, player, at, feet);
        break;
    }
    case State::Claimed:
        step_claimed(dt, at, feet);
        break;
    case State::Spare: {
        stand();
        const Vec3 out(rim_nx_, 0.0F, rim_nz_);
        if (hold_phase_ == 0) {
            // Back up over the rail.
            const RVec3 over = RVec3(at.GetX(), feet + kCarryHandUpM + 0.35F, at.GetZ()) + out * 0.3F;
            pose_hand(dt, over, 0.35F * kHandSpeedMps);
            if (Vec3(over - hand_target_).Length() < 0.02F) {
                hold_phase_ = 1;
            }
        } else if (hold_phase_ == 1) {
            // Down beside her till their head is on the deck.
            const RVec3 down = RVec3(at.GetX(), feet + 2.0F * kPlayerHalfM + 0.1F, at.GetZ()) + right * 0.6F - out * 0.4F;
            pose_hand(dt, down, 0.5F * kHandSpeedMps);
            // Over the deck (in from the rail), the head on it: let go. Never
            // over the drop: past the time, back up and over again.
            const float rail_in = (rim_x_ + rim_nx_ * kRimStandM - float(player.GetX())) * rim_nx_ +
                                  (rim_z_ + rim_nz_ * kRimStandM - float(player.GetZ())) * rim_nz_;
            if (Vec3(down - hand_target_).Length() < 0.02F && player.GetY() < feet + 1.2 && rail_in > 0.5F) {
                let_go_ = true;
                let_go_push_ = Vec3::sZero();
                grip_on_ = false;
                hold_phase_ = 2;
                hold_s_ = 0.0F;
            } else if (hold_s_ > 8.0F) {
                hold_phase_ = 0;
                hold_s_ = 0.0F;
            }
        } else {
            // The collar put straight; then she walks off.
            const RVec3 collar = player + RVec3(0.0, 0.5, 0.0);
            Vec3 to = Vec3(collar - shoulder);
            if (to.Length() > kArmReachM) {
                to *= kArmReachM / to.Length();
            }
            pose_hand(dt, shoulder + to, 0.5F * kHandSpeedMps);
            if (hold_s_ >= kSpareSetSeconds) {
                calm_s_ = kSpareCalmSeconds;
                grab_cooldown_s_ = kSpareCalmSeconds;
                end_hold(State::Shuffling);
            }
        }
        break;
    }
    case State::Release: {
        stand();
        if (hold_s_ >= kReleasePoseSeconds) {
            take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, true);
            end_hold(State::Searching);
        }
        break;
    }
    default:
        break;
    }
}

void Matron::finish_dangle(const RVec3 player) {
    const bool spare = spare_for_test_ >= 0 ? spare_for_test_ != 0 : roll() < kSpareOdds;
    if (spare || !aim_ledge_) {
        ++spares_;
        state_ = State::Spare;
        hold_s_ = 0.0F;
        hold_phase_ = 0;
        return;
    }
    // Aimed: the push that puts them over the ledge as they fall.
    const float drop = std::max(0.2F, float(player.GetY() - (aim_.GetY() + kPlayerHalfM)));
    const float fall_s = std::sqrt(2.0F * drop / kGravity);
    Vec3 push(float(aim_.GetX() - player.GetX()) / fall_s, 0.0F, float(aim_.GetZ() - player.GetZ()) / fall_s);
    const float length = push.Length();
    if (length > kReleasePushMps) {
        push *= kReleasePushMps / length;
    }
    ++releases_;
    let_go_ = true;
    let_go_push_ = push;
    state_ = State::Release;
    hold_s_ = 0.0F;
    grip_on_ = false;
    grab_cooldown_s_ = kAfterReleaseSeconds;
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
        player_arms_left_ = std::max(0, player_arms_left_ - 1);   // the world tells her again next tick
        hold_phase_ = 2;
        hold_s_ = 0.0F;
        pull2_n_ = kHandPullN;
    }
}

// ArmPlay's beats, her second hand: 0 to the wrist, 1 the pull, then (the
// arm hers) 2 up to her face, 3 the swing, 4 the throw and its follow-through.
void Matron::step_armplay(const float dt, const RVec3 player, const RVec3 at, const float feet) {
    const RVec3 shoulder = at + shoulder_local();
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    const auto reach = [&](RVec3 want) {
        Vec3 to = Vec3(want - shoulder);
        if (to.Length() > kArmReachM) {
            to *= kArmReachM / to.Length();
        }
        return shoulder + to;
    };
    switch (hold_phase_) {
    case 0:
        // To the wrist, for as long as it takes: she does not give it up.
        pull2_n_ = kHandReachPullN;
        pose_hand2(dt, reach(player_wrist_), kHandReachSpeedMps);
        break;
    case 1: {
        // Away from the body, up and out: the shoulder takes it.
        pull2_n_ = std::min(kArmPullMaxN, pull2_n_ + kRipRampNps * dt);
        // Her hand drawn on past where her arm reaches: the arm's limit and
        // her muscle (capped) put the pull on the wrist.
        // Away from her other hand, which keeps the ankle: the body drawn
        // out between her two hands, whichever side the arm is on.
        Vec3 away = Vec3(player_wrist_ - hand_target_);
        away = away.Length() > 1.0e-3F ? away.Normalized() : Vec3(0.0F, -1.0F, 0.0F);
        (void)player;
        pose_hand2(dt, player_wrist_ + away * 0.8F, 8.0F * kArmPullMoveMps);
        // No give-up: she pulls until the shoulder tears.
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
        } else if (armplay_then_ == State::Dangle) {
            // Back down over the drop, then the dangle's end.
            state_ = State::Dangle;
            hold_phase_ = 1;
            hold_s_ = 0.0F;
            dangle_s_ = 0.8F;
        } else if (armplay_then_ == State::Spare) {
            // No rail: set back on the deck beside her.
            ++spares_;
            state_ = State::Spare;
            hold_s_ = 0.0F;
            hold_phase_ = 1;
        } else {
            carry_drop();
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
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    wrist_on_ = false;
    pull2_n_ = kHandReachPullN;
    hand2_target_ = physics_.GetBodyInterface().GetPosition(body_) + shoulder_local();
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

// Too long with them on the way to the rail: dropped where she is.
void Matron::carry_drop() {
    ++carry_drops_;
    let_go_ = true;
    let_go_push_ = Vec3::sZero();
    state_ = State::Release;
    hold_s_ = 0.0F;
    grip_on_ = false;
    grab_cooldown_s_ = kAfterReleaseSeconds;
}

// The last arm gone: the run is hers (Claimed, matron_fatality.cpp), at every
// stage. The body still in her hand, she takes it by the ankle to her spot
// (Lay); out of it, she goes to it where it lies (Pin).
void Matron::claim() noexcept {
    ++claims_;
    state_ = State::Claimed;
    hold_phase_ = 0;
    player_arms_left_ = 0;
    fatal_next(static_cast<int>(grip_on_ ? Fatal::Lay : Fatal::Pin));
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
    const Vec3 to = Vec3(target - hand2_target_);
    const float length = to.Length();
    const float step = speed * dt;
    const RVec3 before = hand2_target_;
    hand2_target_ = length <= step ? target : hand2_target_ + to * (step / length);
    hand2_target_v_ = Vec3(hand2_target_ - before) / std::max(dt, 1.0e-4F);
}

} // namespace scraperx::sim
