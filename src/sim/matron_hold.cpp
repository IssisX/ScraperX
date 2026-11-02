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
    arm_reach_m_ = kHoldArmLimitM;
    reset_beats();
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    hand_target_ = at + shoulder_local();
    hand_target_v_ = Vec3::sZero();
    take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, Sense::Touch);
}

void Matron::grip_taken() noexcept {
    if (regrip_ && !grip_on_ && state_ != State::Grab) {
        // Her first hand on the next point she takes him by (his collar at
        // the pin, his ankle from her other hand): the hold goes on.
        grip_on_ = true;
        grip_was_on_ = true;
        regrip_ = false;
        return;
    }
    if (state_ == State::Grab && !grip_on_) {
        regrip_ = false;
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

void Matron::report_held_load(const JPH::RVec3 her_pos, const JPH::RVec3 player_pos, const float player_mass_kg,
                              const bool held) noexcept {
    if (!held || !body_in_world_ || player_mass_kg <= 0.0F) {
        load_held_ = false;
        load_mass_kg_ = 0.0F;
        load_offset_m_ = 0.0F;
        load_moment_nm_ = 0.0F;
        load_cop_shift_m_ = 0.0F;
        load_stance_base_m_ = kUprightBaseM;
        load_dir_x_ = 0.0F;
        load_dir_z_ = 0.0F;
        return;
    }
    const float dx = static_cast<float>(player_pos.GetX() - her_pos.GetX());
    const float dz = static_cast<float>(player_pos.GetZ() - her_pos.GetZ());
    const float offset = std::hypot(dx, dz);
    load_held_ = true;
    load_mass_kg_ = player_mass_kg;
    load_offset_m_ = offset;
    load_moment_nm_ = player_mass_kg * 9.81F * offset;
    load_cop_shift_m_ = player_mass_kg * offset / (kMassKg + player_mass_kg);
    load_dir_x_ = offset > 1.0e-4F ? dx / offset : 0.0F;
    load_dir_z_ = offset > 1.0e-4F ? dz / offset : 0.0F;
    // S2 stance, first answer: carrying a load past the upright base, the
    // reported stance base switches to the crouch base; anything else, the
    // upright base. This is a threshold for the View export (S3 draws
    // from it) — NOT a foot-placement change in sim: her body is one
    // capsule driven by walk_toward, and no sim code reads this value
    // (the extreme valve that once did is removed, 2026-10-31: it broke
    // tear holds). Do not describe it as physical widening.
    load_stance_base_m_ = (state_ == State::Carry && !fatal_hold_ &&
                            load_cop_shift_m_ > kUprightBaseM)
                               ? kCrouchBaseM
                               : kUprightBaseM;
}

void Matron::grip_lost(const bool torn, const bool fought) noexcept {
    const bool had = grip_on_;
    grip_on_ = false;
    if (!torn && regrip_ && holding_state(state_)) {
        // Her own hand opened to take him by another point: the hold goes on.
        return;
    }
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
        if (fatal_hold_ && state_ == State::Dangle && hold_phase_ >= 5) {
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
            if ((hold_phase_ >= 7 || past > 0.3F) && !no_kill_foreseen_ &&
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
        if (reel_) {
            end_reel(physics_.GetBodyInterface().GetPosition(player_));
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
    const RVec3 before = hand_target_;
    hand_target_ += carried * dt;
    const Vec3 to = Vec3(target - hand_target_);
    const float length = to.Length();
    const float step = speed * dt;
    hand_target_ = length <= step ? target : hand_target_ + to * (step / length);
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
    const float omega = std::sqrt(kGravity / kPlayerHalfM);
    return now + std::abs(swing_v) / omega;
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
    // Room to hang: the body's place past the side (and its swing), and its
    // way there: over the side at the top of her heave, and half way down out
    // past it.
    const float com_y = feet + kDangleHandUpM - kPlayerHalfM;
    const float over_y = feet + kCarryHandUpM + 0.35F - kPlayerHalfM;
    const auto clear = [&](const Shape *shape, const float out_m, const float y) {
        AnyHitCollisionCollector<CollideShapeCollector> touch;
        CollideShapeSettings settings;
        const RVec3 pose(sx + nx * out_m, y, sz + nz * out_m);
        physics_.GetNarrowPhaseQuery().CollideShape(shape, Vec3::sReplicate(1.0F), RMat44::sTranslation(pose),
                                                    settings, pose, touch, {}, {}, skip);
        return !touch.HadHit();
    };
    return clear(hang_room_shape_, kDangleOutM, com_y) && clear(hang_room_shape_, kDangleOutM, com_y + 0.8F) &&
           clear(player_cast_shape_, kHeaveOutM, over_y) &&
           clear(player_cast_shape_, 0.5F * (kHeaveOutM + kDangleOutM), 0.5F * (over_y + com_y)) &&
           clear(player_cast_shape_, kFatalWindowOutM, feet + kFatalWindowUpM - kPlayerHalfM) &&
           clear(player_cast_shape_, kFatalDropOutM, feet + kFatalDropUpM - kPlayerHalfM);
}

// The fall from where he will really hang at her let-go there (kFatalDropOutM
// past her middle, his middle under her hand at kFatalDropUpM), still and with
// no toss, kills; and so from where his swing may take him along the edge
// (kFatalSwayM either side): her toss at the let-go is for his swing, not for
// the edge.
bool Matron::lethal_from(const float sx, const float sz, const float nx, const float nz, const float feet) const {
    for (const float along : {0.0F, -kFatalSwayM, kFatalSwayM}) {
        const RVec3 hang(sx + nx * (kFatalDropOutM - 0.1F) + nz * along, feet + kFatalDropUpM - 0.75F,
                         sz + nz * (kFatalDropOutM - 0.1F) - nx * along);
        if (fall_impact_mps(hang, Quat::sIdentity(), Vec3::sZero(), Vec3::sZero(), kFatalChooseMarginM) <
            kFatalLandMps) {
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
    if (grip_on_) {
        let_go_ = true;
        let_go_push_ = Vec3::sZero();
    }
    state_ = next;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    grip_on_ = false;
    grip_was_on_ = false;
    player_braced_ = false;
    thrash_ = 0.0F;
    wrist_on_ = false;
    wrist_reseat_ = false;
    arm_throw_ = false;
    leg_toss_ = false;
    retake_ = false;
    reel_ = false;
    reel_to_floor_ = false;
    spare_walk_ = false;
    spare_side_set_ = false;
    spare_tries_ = 0;
    fatal_hold_ = false;
    fatal_path_ = FatalPath::None;
    fatal_hang_ = false;
    fatal_dropped_ = false;
    fatal_edge_ = RVec3::sZero();
    fatal_s_ = 0.0F;
    fatal_carry_s_ = 0.0F;
    fatal_window_s_ = 0.0F;
    fatal_check_ticks_ = 0;
    fatal_lethal_ = false;
    fatal_toss_ = 0.0F;
    fatal_lethal_mps_ = 0.0F;
    fatal_rim0_ = Vec3::sZero();
    watch_still_s_ = 0.0F;
    watch_y_ = 0.0F;
    pull_n_ = kHandReachPullN;
    pull2_n_ = kHandReachPullN;
    arm_reach_m_ = kArmReachM;
    reset_beats();
    fatal_work_reset();
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
    if (fatal_hold_ && !fatal_dropped_) {
        fatal_s_ += dt;
    }
    const bool on_catwalk = on_catwalk_line(ax, az, 0.0F);
    // The fatality's hold: her hand's pose carried along as she walks (her
    // carry is faster than her hand's own pace); any other hold as it was.
    const Vec3 moving = bodies.GetLinearVelocity(body_);
    const Vec3 her_v = fatal_hold_ ? Vec3(moving.GetX(), 0.0F, moving.GetZ()) : Vec3::sZero();
    // A braced player (their hands on a rail): her pull grows, finite.
    if (grip_on_) {
        pull_n_ = player_braced_ ? std::min(kRipMaxN, pull_n_ + kRipRampNps * dt) : kHandPullN;
        // A shortening spring limit would be a second, unbudgeted actuator.
        // On a ladder only the charged hand/feet forces supply her pull.
        // Her hand's physical limit in a hold: her real arm's envelope
        // (kHoldArmLimitM); its poses are within her real arm (bend_to_hands).
        arm_reach_m_ = kHoldArmLimitM;
    }
    // Beat 2's pose: his ankle at her side, low, a little behind her.
    const auto drag_pose = [&] {
        return RVec3(at.GetX(), feet + kDragHandUpM, at.GetZ()) + right * kDragHandSideM - fwd * kDragHandBackM;
    };
    const auto stand = [&] { walk_toward(dt, ax, az, 0.0F); };
    // S2 extreme valve, REMOVED (2026-10-31, proven wrong, not tuned):
    // the valve interrupted Carry with stagger-then-Spare whenever the
    // measured shift passed the crouch base (0.45 m). Measured ordinary
    // carries cluster on both sides of that line — S2 probe worst
    // 0.449706 m, valve-test max 0.453031 m, dangerous hold 0.4541 m —
    // so no threshold there separates an uncarriable load from an
    // ordinary hold, and the "extreme" scenario's peak is SMALLER than
    // the dangerous hold's. The interruption broke owner-blessed holds:
    // the dangerous hold failed on a shove from the pull-in and, with
    // the pull off, on a valve stagger that cost it its tear; the pest
    // hold ended spared with no tear. Disabling both interventions
    // restores the dangerous hold exactly (single grab, no shove, tear).
    // The body is rotation-locked, so the measured shift is not a real
    // tip in any case; the shift stays exported (S1) and the widened
    // stance stays the threshold, but no shift value abandons a hold.
    // (The old valve test is replaced by a natural-outcome probe below.)
    switch (state_) {
    case State::Grab: {
        if (!grip_on_) {
            const RVec3 ankle = player_ankle_;
            yaw_ = turn_to(yaw_, std::atan2(float(player.GetX()) - ax, float(player.GetZ()) - az), 7.0F, dt);
            if (hold_phase_ == 0) {
                // Beat 1, down: a man on his feet she knocks down first (her
                // swat, the world's kSwatN at his chest); on a ladder, or
                // down already, her hand goes for his ankle at once.
                if (player_down_ || player_on_ladder_ || retake_) {
                    hold_phase_ = 1;
                    hold_s_ = 0.0F;
                } else {
                    stand();
                    if (!swat_given_) {
                        swat_given_ = true;
                        swat_due_ = true;
                        swat_dir_ = swat_direction(player);
                        ++swats_;
                    }
                    // Her hand with the swat, at his chest.
                    pose_hand(dt, player + RVec3(0.0, 0.45, 0.0), kHandReachSpeedMps);
                    if (hold_s_ > kSwatWaitS) {
                        // Still on his feet: no hold.
                        ++grab_misses_;
                        grab_cooldown_s_ = kGrabCooldownSeconds;
                        end_hold(State::Hunting);
                    }
                    break;
                }
            }
            // Beat 2's start: down to his ankle, her feet taking her to it
            // (a man knocked down lies a step or two off), bent for it.
            const float reach_off = xz(ax, az, float(ankle.GetX()), float(ankle.GetZ()));
            if (reach_off > 0.6F && !player_on_ladder_) {
                // To his ankle, stopping short of it (her body never walks
                // into his and shoves it along).
                const float k = (reach_off - 0.6F) / reach_off;
                walk_toward(dt, ax + (float(ankle.GetX()) - ax) * k, az + (float(ankle.GetZ()) - az) * k,
                            std::min(kHuntMps, 0.5F + 2.0F * (reach_off - 0.6F)));
            } else {
                stand();
            }
            pose_hand(dt, ankle, kHandReachSpeedMps);
            if (hold_s_ > kGrabReachSeconds + 1.3F || reach_off > 3.5F) {
                ++grab_misses_;
                retake_ = false;
                grab_cooldown_s_ = kGrabCooldownSeconds;
                end_hold(State::Hunting);
            }
            break;
        }
        if (player_on_ladder_) {
            // Fingers alone do not open his hands. Back away, pulling the
            // ankle through her finite hand; his ordinary grip decides when
            // it can no longer hold. Only then does the normal hold proceed.
            walk_toward(dt, ax - fwd.GetX(), az - fwd.GetZ(), kHuntMps * kCarryPaceScale);
            pose_hand(dt, RVec3(ax, player_ankle_.GetY(), az) + fwd * kCatchM, kHandSpeedMps);
            break;
        }
        stand();
        pose_hand(dt, drag_pose(), 0.6F * kHandSpeedMps);
        if (fatal_hold_) {
            // His one arm left: the fatality. She drags him to its edge.
            retake_ = false;
            begin_fatal(ax, az, feet);
            break;
        }
        if (retake_ || !choose_rim(ax, az)) {
            // Taken again where the world tore him out of her hand, or no
            // rail to drag him to: the pin against her where she stands (her
            // spot standing in for the rail's), the arm there, then set down.
            retake_ = false;
            aim_ledge_ = false;
            stand_in_for_rim(at, player, fwd);
            pin_here_ = true;
            begin_pin();
            break;
        }
        ++carries_;
        state_ = State::Carry;
        hold_s_ = 0.0F;
        break;
    }
    case State::Carry: {
        // Beat 2, the drag: his ankle at her side, low, him on his back on the
        // deck behind her; her pull capped against his grip (pull_n_).
        const float off = xz(ax, az, rim_x_, rim_z_);
        if (off > kRimArriveM) {
            float wx = rim_x_;
            float wz = rim_z_;
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
                pin_here_ = false;
                begin_pin();
            }
        }
        pose_hand(dt, drag_pose(), kHandSpeedMps, her_v);
        load_pull_m_ = 0.0F;
        if (fatal_hold_ && state_ == State::Carry) {
            // The fatality's edge not reached in the drag's time: the floor
            // version, where she stands.
            fatal_carry_s_ += dt;
            if (fatal_carry_s_ > kFatalCarryS) {
                fatal_floor();
            }
        } else if (hold_s_ > kCarryMaxSeconds && state_ == State::Carry) {
            // Too long with him: the pin where she stands, the arm there, set down.
            stand_in_for_rim(at, player, fwd);
            pin_here_ = true;
            begin_pin();
        }
        break;
    }
    case State::Dangle:
        step_rail(dt, player, at, feet);
        break;
    case State::ArmPlay: {
        // The pin held (her first hand on his belt, or his collar with no
        // rail), his arm to her other.
        walk_toward(dt, rim_x_, rim_z_, std::min(0.8F, 3.0F * xz(ax, az, rim_x_, rim_z_)));
        yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        pose_hand(dt, grip_point_ == Grip::Belt ? belt_pose(at, feet) : pin_pose(at, feet), 0.6F * kHandSpeedMps);
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
        } else if (reel_ || (hold_phase_ == 0 && grip_point_ == Grip::Ankle && !pin_here_ && back_lifted_)) {
            // Reeling from his blow, or hauling him back in by the ankle over
            // the rail (her one hand cannot haul hand over hand: her feet do
            // it, walking back, the rail his pulley): her face still to it,
            // him still in her fist.
            const float facing_edge = yaw_;
            const float back = reel_ && grip_point_ != Grip::Ankle ? kReelBackM : kBackInWalkM;
            walk_toward(dt, rim_x_ - rim_nx_ * back, rim_z_ - rim_nz_ * back, reel_ ? 1.2F : 0.6F);
            yaw_ = turn_to(facing_edge, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
        } else {
            stand();
        }
        const float in_pace = reel_ ? 1.0F : 0.5F;
        // How far in from the rail's line his middle is.
        const float rail_in = (rim_x_ + rim_nx_ * kRimStandM - float(player.GetX())) * rim_nx_ +
                              (rim_z_ + rim_nz_ * kRimStandM - float(player.GetZ())) * rim_nz_;
        if (hold_phase_ == 0) {
            // Back in over the rail: her hand in and down, the rail his
            // pulley (by the ankle), or his collar drawn in off it.
            // By the ankle, her hand rides just over the rail's top (his leg
            // over it, not wedged under it) till his middle is in.
            // By the ankle, his body slides in across the rail's top along its
            // own length, tilted (a rigid body does not bend over it): her hand
            // at her side rising as she walks back (kBackInSlope a metre in from
            // the rail, at most kBackInTopM over her feet).
            const float her_in = (rim_x_ + rim_nx_ * kRimStandM - ax) * rim_nx_ +
                                 (rim_z_ + rim_nz_ * kRimStandM - az) * rim_nz_;
            const float up = grip_point_ != Grip::Ankle
                                 ? std::max(kPinHereUpM - 0.25F, rail_up_)
                                 : std::min(kBackInTopM, rail_up_ + 0.15F + kBackInSlope * std::max(0.0F, her_in - kRimStandM));
            RVec3 in = RVec3(at.GetX(), feet + up, at.GetZ()) -
                       out * (grip_point_ != Grip::Ankle ? kBackInM : 0.0F) + right * kBackInSideM;
            if (grip_point_ == Grip::Ankle && !pin_here_ && rail_in < 0.0F && !back_lifted_) {
                // His ankle not over the rail's top yet: up clear of it first
                // (not hauled into its side).
                in = RVec3(at.GetX(), feet + rail_up_ + kBackInLiftM, at.GetZ()) + out * kHangOutM + right * kHangSideM;
            }
            pose_hand(dt, in, in_pace * kHandSpeedMps);
            back_lifted_ = back_lifted_ || float(player_ankle_.GetY()) >= feet + rail_up_ + 0.3F || rail_in >= 0.0F;
            if (rail_in > 0.45F || (pin_here_ && Vec3(in - hand_target_).Length() < 0.05F)) {
                hold_phase_ = 1;
                hold_s_ = 0.0F;
            } else if (hold_s_ > 8.0F) {
                // Not coming in: out and up a little, and in again.
                hold_s_ = 0.0F;
                ++spare_tries_;
            }
            spare_side_set_ = false;
        } else if (hold_phase_ == 1) {
            // Down beside her onto her deck, and let go once her deck is close
            // under his body; not, she walks him on (spare_walk_).
            if (!spare_side_set_) {
                spare_side_ = spare_side(at, feet, right, out, spare_deck_);
                spare_side_set_ = true;
            }
            const RVec3 down = RVec3(at.GetX(), feet + 0.55F, at.GetZ()) + spare_side_;
            pose_hand(dt, down, in_pace * kHandSpeedMps);
            const auto set_down = [&]() {
                let_go_ = true;
                let_go_push_ = Vec3::sZero();
                grip_on_ = false;
                hold_phase_ = 2;
                hold_s_ = 0.0F;
                if (reel_to_floor_) {
                    reel_ = false;
                    reel_to_floor_ = false;
                    fatal_floor();
                    return true;
                }
                if (reel_) {
                    end_reel(player);
                    return true;
                }
                return false;
            };
            const bool there = Vec3(down - hand_target_).Length() < 0.05F || hold_s_ > 2.5F;
            if (there && deck_close_below(player, feet)) {
                spare_walk_ = false;
                if (set_down()) {
                    break;
                }
            } else if (there && !spare_walk_ && hold_s_ > kSpareStepSeconds) {
                spare_walk_ = true;
                spare_to_x_ = ax - rim_nx_ * 0.5F;
                spare_to_z_ = az - rim_nz_ * 0.5F;
                hold_s_ = 2.5F;
                ++spare_tries_;
            }
        } else {
            // The collar put straight; then she walks off.
            pose_hand(dt, player + RVec3(0.0, 0.5, 0.0), 0.5F * kHandSpeedMps);
            if (hold_s_ >= kSpareSetSeconds) {
                // She walks off for as long as her hands stay off him (rule
                // 35: 30 s after a tear she lets him run), not hunting him
                // meanwhile with no way to take hold (her body would only
                // shove him along, off an edge).
                grab_cooldown_s_ = std::max(kSpareCalmSeconds, hands_off_after_hold());
                calm_s_ = grab_cooldown_s_;
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
    // Every pose of her hands within her real arm (kRealArmM), her trunk
    // bent to it; not in the floor fatality's beats (Claimed), its own.
    if (holding_state(state_) && state_ != State::Claimed) {
        bend_to_hands(dt, at, feet);
    } else {
        trunk_ = Trunk{};
    }
}

// ---- The five beats' helpers (HOLD_WITH_REAL_ARMS.md) ----------------------

RVec3 Matron::shoulder_of(const RVec3 at, const float side) const noexcept {
    // Her shoulder on `side` (+1 her right, -1 her left) where her trunk puts
    // it: her hips trunk_.hip_up over her feet, her trunk pitched forward.
    const float feet = float(at.GetY()) - kStandCentre;
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    const Vec3 trunk = fwd * (kTrunkM * std::sin(trunk_.pitch)) + Vec3(0.0F, kTrunkM * std::cos(trunk_.pitch), 0.0F);
    return RVec3(at.GetX(), feet + trunk_.hip_up, at.GetZ()) + trunk + right * (side * kShoulderSideM);
}

void Matron::bend_to_hands(const float dt, const RVec3 at, const float feet) {
    // The least bend (pitch, then knees) that puts her right shoulder within
    // her real arm of her hand's pose; none does: the one that comes nearest.
    const Vec3 fwd(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 right(fwd.GetZ(), 0.0F, -fwd.GetX());
    const RVec3 want = hand_target_;
    const bool two = hand2_active();
    Trunk best = trunk_;
    float best_effort = 1.0e9F;
    float best_miss = 1.0e9F;
    for (const float hip : {kHipUpM, 0.5F * (kHipUpM + kHipLowM), kHipLowM}) {
        for (int k = 0; k <= 14; ++k) {
            const float pitch = std::min(kTrunkPitchMaxRad, 0.1F * static_cast<float>(k));
            const RVec3 shoulder = RVec3(at.GetX(), feet + hip, at.GetZ()) +
                                   fwd * (kTrunkM * std::sin(pitch)) +
                                   Vec3(0.0F, kTrunkM * std::cos(pitch), 0.0F) + right * kShoulderSideM;
            float miss = std::max(0.0F, Vec3(want - shoulder).Length() - (kRealArmM - 0.03F));
            if (two) {
                const RVec3 left = shoulder - right * (2.0F * kShoulderSideM);
                miss = std::max(miss, Vec3(hand2_target_ - left).Length() - (kRealArmM - 0.03F));
            }
            const float effort = pitch + 2.0F * (kHipUpM - hip);
            if (miss < best_miss - 1.0e-4F || (miss <= best_miss + 1.0e-4F && effort < best_effort)) {
                best_miss = miss;
                best_effort = effort;
                best = Trunk{hip, pitch};
            }
        }
    }
    // Eased (her joints at a finite rate: rule 37).
    const float pitch_step = 2.5F * dt;
    const float hip_step = 0.8F * dt;
    trunk_.pitch += std::clamp(best.pitch - trunk_.pitch, -pitch_step, pitch_step);
    trunk_.hip_up += std::clamp(best.hip_up - trunk_.hip_up, -hip_step, hip_step);
    // Each hand's pose within her real arm of its shoulder.
    const auto within = [&](RVec3 &hand, const float side) {
        const RVec3 shoulder = shoulder_of(at, side);
        const Vec3 to = Vec3(hand - shoulder);
        const float length = to.Length();
        if (length > kRealArmM) {
            hand = shoulder + to * (kRealArmM / length);
        }
    };
    within(hand_target_, 1.0F);
    if (hand2_active()) {
        within(hand2_target_, -1.0F);
    }
}

void Matron::find_rail_top(const RVec3 at, const float feet) {
    // The first steel under her rail's line (the rail's top, or a bare lip's
    // deck), from her hand's height down.
    rail_up_ = 0.0F;
    if (pin_here_) {
        return;
    }
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    const RVec3 from(rim_x_ + rim_nx_ * kRimStandM, feet + 1.7F, rim_z_ + rim_nz_ * kRimStandM);
    RayCastResult hit;
    if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -2.0F, 0.0F)), hit, {}, {}, skip)) {
        rail_up_ = std::max(0.0F, 1.7F - 2.0F * hit.mFraction);
    }
    (void)at;
}

RVec3 Matron::rail_pose(const RVec3 at, const float feet, const float up, const float out, const float side) const noexcept {
    const Vec3 o(rim_nx_, 0.0F, rim_nz_);
    const Vec3 right(o.GetZ(), 0.0F, -o.GetX());
    return RVec3(at.GetX(), feet + up, at.GetZ()) + o * out + right * side;
}

RVec3 Matron::belt_pose(const RVec3 at, const float feet) const noexcept {
    return pin_here_ ? rail_pose(at, feet, kPinHereUpM - 0.35F, 0.40F, kBeltSideM)
                     : rail_pose(at, feet, rail_up_ + kBeltUpM, kBeltOutM, kBeltSideM);
}

RVec3 Matron::pin_pose(const RVec3 at, const float feet) const noexcept {
    if (pin_here_) {
        return rail_pose(at, feet, kPinHereUpM, 0.50F, kPinSideM);
    }
    // His middle kept balanced on the rail's top (kPinBalanceM past its
    // line): slipping out, her hand on his collar comes in and up; in, out.
    const BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 middle = bodies.GetPosition(player_);
    const float past = (float(middle.GetX()) - (rim_x_ + rim_nx_ * kRimStandM)) * rim_nx_ +
                       (float(middle.GetZ()) - (rim_z_ + rim_nz_ * kRimStandM)) * rim_nz_;
    const float err = past - kPinBalanceM;
    const float out = kPinOutM - std::clamp(1.2F * err, -0.15F, 0.55F);
    const float up = rail_up_ + kPinUpM + std::clamp(0.8F * err, 0.0F, 0.35F);
    return rail_pose(at, feet, up, out, kPinSideM);
}

// Beat 1's swat: away from her, else the other way, else to either side:
// the first with her deck under where he falls (a body length on), never off
// an edge (no kill on touch).
Vec3 Matron::swat_direction(const RVec3 player) const {
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    Vec3 away(float(player.GetX() - at.GetX()), 0.0F, float(player.GetZ() - at.GetZ()));
    away = away.LengthSq() > 1.0e-4F ? away.Normalized() : Vec3(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 side(away.GetZ(), 0.0F, -away.GetX());
    const float feet = float(at.GetY()) - kStandCentre;
    IgnoreMultipleBodiesFilter skip;
    skip.IgnoreBody(body_);
    skip.IgnoreBody(player_);
    for (const Vec3 dir : {away, side, -side, -away}) {
        bool deck = true;
        for (const float on : {0.9F, 1.8F}) {
            const RVec3 from(player.GetX() + dir.GetX() * on, feet + 1.0F, player.GetZ() + dir.GetZ() * on);
            RayCastResult under;
            deck = deck && physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -1.6F, 0.0F)), under, {}, {},
                                                                skip) &&
                   std::abs(1.0F - 1.6F * under.mFraction) < 0.4F;
        }
        if (deck) {
            return dir;
        }
    }
    return -away;
}

void Matron::reset_beats() noexcept {
    grip_point_ = Grip::Ankle;
    regrip_ = false;
    regrip_let_go_ = false;
    hand2_want_ = 0;
    hand2_on_ = 0;
    swat_due_ = false;
    swat_given_ = false;
    pin_here_ = false;
    over_up_ = false;
    back_lifted_ = false;
    relaid_ = false;
    over_tries_ = 0;
    rail_up_ = 0.0F;
}

void Matron::begin_regrip(const Grip point) noexcept {
    grip_point_ = point;
    regrip_ = true;
    regrip_let_go_ = grip_on_;
}

// Beat 3's start: at the rail (or, none, where she stands), her first hand
// lets go of his ankle (him on her deck) and takes his collar.
void Matron::begin_pin() {
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    const float feet = float(at.GetY()) - kStandCentre;
    state_ = State::Dangle;
    hold_s_ = 0.0F;
    hold_phase_ = 0;
    over_up_ = false;
    find_rail_top(at, feet);
    // The rail she pins him on (her re-poses in the rip step back from it:
    // over is at this one).
    pin_rim_[0] = rim_x_;
    pin_rim_[1] = rim_z_;
    pin_rim_[2] = rim_nx_;
    pin_rim_[3] = rim_nz_;
    // Her hand opens on his ankle for his collar only with her deck under
    // him (step_rail, phase 0).
    fatal_window_s_ = 0.0F;
    fatal_hang_ = false;
    if (fatal_hold_) {
        dangle_s_ = kFatalHangWindowS;
    } else {
        dangle_s_ = dangle_for_test_s_ >= 0.0F ? dangle_for_test_s_ : kDangleMinSeconds + kDangleSpanSeconds * roll();
    }
}

void Matron::step_rail(const float dt, const RVec3 player, const RVec3 at, const float feet) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const Vec3 out(rim_nx_, 0.0F, rim_nz_);
    const float to_spot = xz(ax, az, rim_x_, rim_z_);
    const auto at_spot = [&] {
        walk_toward(dt, rim_x_, rim_z_, std::min(0.8F, 3.0F * to_spot));
        yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
    };
    const auto back_in = [&] {
        // Out of her plan: back in over the rail and set down.
        ++spares_;
        hand2_want_ = 0;
        regrip_ = false;
        state_ = State::Spare;
        hold_phase_ = 0;
        hold_s_ = 0.0F;
        spare_walk_ = false;
    };
    switch (hold_phase_) {
    case 0: {
        if (grip_point_ == Grip::Ankle && !regrip_) {
            // Her deck under the whole of him first (a drag's slide can take
            // his body on past her toward the edge): she keeps his ankle and
            // steps back in, hauling him after her.
            const float in_from_rail = (rim_x_ + rim_nx_ * kRimStandM - float(player.GetX())) * rim_nx_ +
                                       (rim_z_ + rim_nz_ * kRimStandM - float(player.GetZ())) * rim_nz_;
            const Vec3 his_v = physics_.GetBodyInterface().GetLinearVelocity(player_);
            const bool at_rest = Vec3(his_v.GetX(), 0.0F, his_v.GetZ()).Length() < 0.4F;
            if (pin_here_ || (in_from_rail >= 0.4F && at_rest && deck_close_below(player, feet))) {
                begin_regrip(Grip::Collar);
                break;   // the world opens her hand this tick
            } else {
                walk_toward(dt, rim_x_ - rim_nx_ * kBackInWalkM, rim_z_ - rim_nz_ * kBackInWalkM, 0.6F);
                yaw_ = turn_to(yaw_, std::atan2(rim_nx_, rim_nz_), kHoldTurnRate, dt);
                pose_hand(dt, RVec3(at.GetX(), feet + kDragHandUpM, at.GetZ()) - out * 0.3F, kHandSpeedMps);
                if (hold_s_ > 2.0F * kPinReachS) {
                    back_in();
                }
                break;
            }
        }
        // His collar: she steps in to his chest and takes it.
        const RVec3 c = player_collar_;
        const float off = xz(ax, az, float(c.GetX()), float(c.GetZ()));
        walk_toward(dt, float(c.GetX()), float(c.GetZ()), off > 0.55F ? std::min(0.9F, 0.4F + 1.5F * off) : 0.0F);
        yaw_ = turn_to(yaw_, std::atan2(float(c.GetX()) - ax, float(c.GetZ()) - az), kHoldTurnRate, dt);
        pose_hand(dt, c, kHandReachSpeedMps);
        if (grip_on_) {
            hold_phase_ = 1;
            hold_s_ = 0.0F;
        } else if (hold_s_ > kPinReachS) {
            // Up and away from her before her hand closed: the hold over.
            regrip_ = false;
            ++grab_misses_;
            state_ = State::Release;
            hold_s_ = 0.0F;
            hold_phase_ = 0;
            grab_cooldown_s_ = kGrabCooldownSeconds;
        }
        break;
    }
    case 1: {
        // The haul: back to her spot, her second hand on his belt; his collar
        // out past the rail and his belt up on its top: his middle balanced
        // on it, his back to it, his head and shoulders out over the drop.
        hand2_want_ = 2;
        pull2_n_ = hand2_on_ == 2 ? kHandPullN : kHandReachPullN;
        if (hand2_on_ != 2) {
            // She steps in to his middle (his collar in her first hand), her
            // second hand for his belt.
            const RVec3 b = player_belt_;
            const float off = xz(ax, az, float(b.GetX()), float(b.GetZ()));
            walk_toward(dt, float(b.GetX()), float(b.GetZ()), off > 0.5F ? std::min(0.8F, 1.5F * (off - 0.5F)) : 0.0F);
            yaw_ = turn_to(yaw_, std::atan2(float(b.GetX()) - ax, float(b.GetZ()) - az), kHoldTurnRate, dt);
            pose_hand(dt, hand_target_, kHandSpeedMps);
            pose_hand2(dt, b, kHandReachSpeedMps);
            if (hold_s_ > kPinReachS) {
                // His belt out of her other hand's reach (steel in the way):
                // the pin by his collar alone.
                hand2_want_ = 0;
                hold_phase_ = 2;
                hold_s_ = 0.0F;
                fatal_window_s_ = 0.0F;
            }
            break;
        }
        at_spot();
        const RVec3 pin = pin_pose(at, feet);
        const RVec3 belt = belt_pose(at, feet);
        pose_hand(dt, pin, 0.6F * kHandSpeedMps);
        pose_hand2(dt, belt, 0.6F * kHandSpeedMps);
        if ((to_spot < 0.15F && Vec3(pin - hand_target_).Length() < 0.05F &&
             Vec3(belt - hand2_target_).Length() < 0.05F) ||
            hold_s_ > kPinHaulS) {
            hold_phase_ = 2;
            hold_s_ = 0.0F;
            fatal_window_s_ = 0.0F;
        }
        break;
    }
    case 2: {
        // The pin: face to face, her forearm on his chest; his window (his
        // blow frees him: his_blow).
        at_spot();
        pose_hand(dt, pin_pose(at, feet), kHandSpeedMps);
        if (hand2_on_ == 2) {
            pose_hand2(dt, belt_pose(at, feet), kHandSpeedMps);
        }
        // His window, face to face (his punch reaches her face: his blow
        // frees him); the fatality's is kFatalHangWindowS.
        fatal_hang_ = fatal_hold_;
        fatal_window_s_ += dt;
        if (fatal_window_s_ >= (fatal_hold_ ? kFatalHangWindowS : kPinWindowS)) {
            fatal_hang_ = false;
            fatal_window_s_ = 0.0F;
            if (pin_here_) {
                hand2_want_ = 0;
                if (player_arms_left_ <= 0) {
                    claim();
                } else if (fatal_hold_ || !begin_armplay(State::Dangle)) {
                    ++spares_;
                    spare_here();
                }
                break;
            }
            // Her first hand from his collar to his belt beside her second
            // (his middle held on the rail's top: he cannot slide off it);
            // her second not on it, her first keeps his collar.
            if (hand2_on_ == 2) {
                begin_regrip(Grip::Belt);
            }
            hold_phase_ = 3;
            hold_s_ = 0.0F;
        }
        break;
    }
    case 3: {
        at_spot();
        pose_hand2(dt, belt_pose(at, feet), kHandSpeedMps);
        if (regrip_) {
            pose_hand(dt, player_belt_, kHandReachSpeedMps);
            if (hold_s_ > kHandoffS) {
                back_in();
            }
            break;
        }
        pose_hand(dt, grip_point_ == Grip::Belt ? belt_pose(at, feet) : pin_pose(at, feet), kHandSpeedMps);
        if (fatal_hold_) {
            hold_phase_ = 4;
            hold_s_ = 0.0F;
        } else if (player_arms_left_ <= 0) {
            claim();
        } else {
            // Her second hand off his belt for his arm.
            hand2_want_ = 0;
            if (!begin_armplay(State::Dangle)) {
                hold_phase_ = 4;
                hold_s_ = 0.0F;
            }
        }
        break;
    }
    case 4: {
        // Over's start: back at her spot, his middle on the rail's top in her
        // first hand; her second hand to his ankle.
        at_spot();
        pose_hand(dt, grip_point_ == Grip::Belt ? belt_pose(at, feet) : pin_pose(at, feet), kHandSpeedMps);
        if (to_spot > 0.2F && hold_s_ < 2.0F * kHandoffS) {
            break;
        }
        hand2_want_ = 1;
        if (hand2_on_ != 1) {
            pull2_n_ = kHandReachPullN;
            pose_hand2(dt, player_ankle_, kHandReachSpeedMps);
            if (hold_s_ > 2.0F * kHandoffS) {
                back_in();
            }
        } else {
            pull2_n_ = kHandPullN;
            hold_phase_ = 5;
            hold_s_ = 0.0F;
        }
        break;
    }
    case 5: {
        // Over: her first hand pushes his middle out over the rail, her
        // second lifts his ankle up over its top.
        at_spot();
        const RVec3 middle = physics_.GetBodyInterface().GetPosition(player_);
        const float past = (float(middle.GetX()) - (rim_x_ + rim_nx_ * kRimStandM)) * rim_nx_ +
                           (float(middle.GetZ()) - (rim_z_ + rim_nz_ * kRimStandM)) * rim_nz_;
        pose_hand(dt, rail_pose(at, feet, rail_up_ + kBeltUpM + 0.1F, kBeltOutM + kOverPushM, kBeltSideM),
                  0.8F * kHandSpeedMps);
        // Square across the rail first: his ankle drawn in behind her till his
        // body points out over it (not along it); then up.
        const Vec3 axis = physics_.GetBodyInterface().GetRotation(player_).RotateAxisY();
        const Vec3 along(rim_nz_, 0.0F, -rim_nx_);
        const bool square = std::abs(axis.Dot(along)) < 0.45F;
        const RVec3 in = rail_pose(at, feet, rail_up_ + kOverInUpM, kOverInOutM, kHangSideM);
        const RVec3 up = rail_pose(at, feet, rail_up_ + kOverUpM, kOverOutM, kHangSideM);
        if (!square && hold_s_ < 3.0F && !over_up_) {
            pose_hand2(dt, in, 0.8F * kHandSpeedMps);
            break;
        }
        over_up_ = true;   // squared (or as square as he comes): up from here
        pose_hand2(dt, up, 0.8F * kHandSpeedMps);
        if ((past > kOverOutPastM && Vec3(up - hand2_target_).Length() < 0.1F) || hold_s_ > 6.0F) {
            // His middle out: her first hand from his belt to his ankle.
            begin_regrip(Grip::Ankle);
            hold_phase_ = 6;
            hold_s_ = 0.0F;
            over_up_ = false;
        }
        break;
    }
    case 6: {
        // Her first hand takes his ankle from her second, up over the rail's
        // top and out onto it: he tips over head first.
        at_spot();
        const RVec3 up = rail_pose(at, feet, rail_up_ + kOverUpM, kOverOutM, kHangSideM);
        const RVec3 hang = rail_pose(at, feet, rail_up_ + kHangUpM, kHangOutM, kHangSideM);
        if (regrip_) {
            pose_hand(dt, player_ankle_, kHandReachSpeedMps);
            if (hold_s_ > kHandoffS) {
                back_in();
            }
            break;
        }
        hand2_want_ = 0;
        if (!over_up_) {
            pose_hand(dt, up, 0.8F * kHandSpeedMps);
            over_up_ = Vec3(up - hand_target_).Length() < 0.05F;
        } else {
            pose_hand(dt, hang, 0.6F * kHandSpeedMps);
            if (Vec3(hang - hand_target_).Length() < 0.03F) {
                hold_phase_ = 7;
                hold_s_ = 0.0F;
            }
        }
        if (hold_s_ > 10.0F) {
            back_in();
        }
        break;
    }
    default: {
        // Hanging by one ankle against the outside of her deck.
        at_spot();
        const RVec3 hang = rail_pose(at, feet, rail_up_ + kHangUpM, kHangOutM, kHangSideM);
        pose_hand(dt, hang, kHandSpeedMps);
        if (hold_s_ > 1.5F && physics_.GetBodyInterface().GetRotation(player_).RotateAxisY().GetY() > -0.5F &&
            over_tries_ < 2) {
            // Not over (lying on the rail's top): up again, and over.
            ++over_tries_;
            hold_phase_ = 6;
            hold_s_ = 0.0F;
            over_up_ = false;
            break;
        }
        if (!fatal_hold_) {
            if (hold_s_ >= dangle_s_) {
                finish_dangle();
            }
            break;
        }
        // The fatality's let-go (his window was at the pin): out as far as her
        // arm goes, him clear of the deck's face; no sooner than
        // kFatalLetGoFromS after her fingers closed, once his fall from where
        // he is, as he moves, is foreseen to kill; none by kFatalLetGoByS, she
        // hauls him back in for her floor version.
        const bool due = fatal_s_ >= kFatalLetGoFromS;
        pose_hand(dt, rail_pose(at, feet, rail_up_ + kHangUpM + 0.1F, kHangOutM + kFatalOutM, kHangSideM),
                  kHandSpeedMps);
        if (due || --fatal_check_ticks_ <= 0) {
            fatal_check_ticks_ = 3;
            foresee_fall(player, out);
            if (!fatal_lethal_ && xz(ax, az, rim_x_, rim_z_) < 0.1F) {
                (void)fatal_shift(feet);
            }
        }
        // His middle out over open air (kFatalOpenAirM past the edge).
        const RVec3 mid = physics_.GetBodyInterface().GetPosition(player_);
        const float mid_past = (float(mid.GetX()) - (rim_x_ + rim_nx_ * kRimStandM)) * rim_nx_ +
                               (float(mid.GetZ()) - (rim_z_ + rim_nz_ * kRimStandM)) * rim_nz_;
        // And hanging settled a moment (not still swinging in from the over).
        if (due && fatal_lethal_ && hold_s_ >= kFatalHangSettleS && mid_past >= kFatalOpenAirM + 0.02F &&
            fatal_s_ <= kFatalLetGoByS) {
            fatal_release();
        } else if (fatal_s_ >= kFatalLetGoByS) {
            reel_ = true;
            reel_to_floor_ = true;
            state_ = State::Spare;
            hold_phase_ = 0;
            hold_s_ = 0.0F;
            spare_walk_ = false;
        }
        break;
    }
    }
}

int Matron::hold_beat() const noexcept {
    switch (state_) {
    case State::Grab:
        return !grip_on_ && hold_phase_ == 0 ? 1 : 2;
    case State::Carry:
        return 2;
    case State::Dangle:
        return hold_phase_ <= 3 ? 3 : (hold_phase_ <= 6 ? 4 : 5);
    case State::ArmPlay:
        return 3;
    case State::Spare:
        return 6;
    default:
        return 0;
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
        // Her hand drawn on past where her arm reaches: the arm's limit and
        // her muscle (capped) put the pull on the wrist.
        // Away from her other hand, which keeps the ankle: the body drawn
        // out between her two hands, whichever side the arm is on.
        Vec3 away = Vec3(player_wrist_ - hand_target_);
        away = away.Length() > 1.0e-3F ? away.Normalized() : Vec3(0.0F, -1.0F, 0.0F);
        // With her real arm (kRealArmM) the pull is hers, not her arm's
        // reach: she wrenches the wrist in past her shoulder while her first
        // hand holds his body out (the pin), her pull capped (pull2_n_) over
        // the stretch to it (physics_world_arm.cpp kWrenchSpringScale).
        const RVec3 chest = shoulder_of(at, -1.0F) - fwd * 0.2F;
        const RVec3 wrench = Vec3(chest - player_wrist_).Dot(away) < -0.3F ? chest : player_wrist_ + away * 0.8F;
        // Her wrench builds at half the pace of her reach, not all at once.
        pose_hand2(dt, wrench, 4.0F * kArmPullMoveMps);
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
            // Along the band beside her and a little in off the rail, low,
            // her real arm's short lever (kArmThrowRealScale): it lands on her
            // deck, not down the shaft or off the band's far side.
            const Vec3 inboard(-rim_nx_, 0.0F, -rim_nz_);
            arm_throw_v_ = (right * 0.95F + inboard * 0.3F + Vec3(0.0F, 0.3F, 0.0F)).Normalized() *
                           (kArmThrowRealScale * kArmThrowMps);
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
        } else if (armplay_then_ == State::Dangle && !pin_here_) {
            // Back to the rail she pinned him on, then over it (beat 4).
            rim_x_ = pin_rim_[0];
            rim_z_ = pin_rim_[1];
            rim_nx_ = pin_rim_[2];
            rim_nz_ = pin_rim_[3];
            state_ = State::Dangle;
            hold_phase_ = 4;
            hold_s_ = 0.0F;
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
    if (state_ == State::Grab && !grip_on_) {
        grab_cooldown_s_ = kRegrabAfterTearS;
        end_hold(State::Searching);
        resume_state_ = State::Searching;
        state_ = State::Staggering;
        stagger_s_ = kStaggerMinSeconds;
        return true;
    }
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
    if ((state_ != State::Dangle || hold_phase_ <= 1) && over_her_deck(player, feet)) {
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
    arm_reach_m_ = kHoldArmLimitM;
    regrip_ = false;
    regrip_let_go_ = false;
    grip_point_ = Grip::Ankle;
    hand2_want_ = 0;
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
