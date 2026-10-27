// Her old rung climber (matron.hpp): up or down a climb her scan found, hold
// by hold, and over its head. A Mover (matron_mover.hpp); Board v5 step 4
// replaces it with the Climber's rung moves.
#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/zipline.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace scraperx::sim {
using namespace JPH;

namespace {
constexpr float kPi = 3.14159265F;
constexpr float kTurnRate = 7.0F;
float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, 2.0F * kPi);
}
float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}
} // namespace

bool Matron::follow_climb(const float dt, const RVec3 at) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float level_floor = levels_[static_cast<std::size_t>(level_)].floor_y;
    const float feet = float(at.GetY()) - kStandCentre;
    // A climb from her level up toward her lead's (to it, or to a level on
    // the way): the one she saw taken, or the nearest. Its foot must be
    // under her own feet, not only under her level: her level can name a
    // floor she left (a scan-only floor has no level), and a climb from
    // that floor is far below her with its foot's x/z near hers — walking
    // to it strands her on the edge above it, claimed every tick.
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (int i = 0; i < climb_count(); ++i) {
        Climb c{};
        // Her hand climb in the hunt is for ladders; a lip she takes by her own plan.
        if (!climb_of(i, c, Ways::Ladders)) {
            continue;
        }
        if (!c.hers || std::abs(c.foot_y - level_floor) > 0.6F || std::abs(c.foot_y - feet) > 0.6F ||
            c.head_y > lead_floor_ + 0.6F || c.head_y < level_floor + 0.6F) {
            continue;
        }
        const float d = i == guess_climb_ ? -1.0F : distance_xz(ax, az, c.x, c.z);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    if (best < 0) {
        return false;
    }
    const Climb c = climb_spec(best);
    const float off = distance_xz(ax, az, c.x, c.z);
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (off > 0.2F) {
        // To its foot along her lines, slowing into it.
        float wx = c.x;
        float wz = c.z;
        route_toward(ax, az, c.x, c.z, false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // At its foot, looking up it. Frail, she cannot take hold of it; in the
    // Turn she does, and her hands have her weight. The bay's rungs she
    // always does: they are her way out of her own trap.
    walk_toward(dt, c.x, c.z, 0.0F, accel);
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    if (turn_ == Turn::None && best != bay_climb_) {
        return true;
    }
    begin_climb(best);
    return true;
}

namespace {
using grip::ClimbBodyFilter;
using grip::hand_rests_on;
using grip::PointHit;
using grip::RayHits;
} // namespace

// A real upward-facing contact on the climb's line of holds, in hand reach,
// visible from the chest, with room above it for her fingers (a rung buried
// in a fascia plate is no hold). As in the player's physical hang, anchors
// live in the support's frame and the support receives the reaction. The
// mechanism kit's welded load grips cannot attach to static steel and
// rigidly weld the bodies rather than allowing a climber to move relative to
// a hand contact. Going up she takes the highest such hold she can reach
// above the hand she has; going down, the highest below it, never the floor
// at the foot; starting down from the top, the first under its lip.
bool Matron::find_climb_hold(const RVec3 at, const bool down) {
    const Climb c = climb_spec(climb_);
    const bool holding = !climb_support_.IsInvalid();
    float top = 0.0F;
    float bottom = 0.0F;
    if (!down && holding) {
        top = std::min(c.lip_y + 0.04F, float(at.GetY()) + kClimbReachM);
        bottom = float(climb_hand_.GetY()) + 0.1F;
    } else if (!down) {
        // Her first hold, from her feet: overhead, as far as a standing reach.
        top = std::min(c.lip_y + 0.04F, float(at.GetY()) + kStandingReachM);
        bottom = top - 0.6F;
    } else if (holding) {
        top = float(climb_hand_.GetY()) - 0.1F;
        bottom = std::max(c.foot_y + 0.15F, float(at.GetY()) - 0.6F);
    } else {
        top = c.lip_y + 0.04F;
        bottom = c.lip_y - 0.6F;
    }
    if (top <= bottom) {
        return false;
    }
    const RVec3 origin(c.x + c.out_x * c.standoff, top, c.z + c.out_z * c.standoff);
    const ClimbBodyFilter filter(body_, player_);
    const RRayCast ray(origin, Vec3(0.0F, bottom - top, 0.0F));
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false; // a probe starting inside a rung must not invent a top normal
    RayHits hits;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hits, {}, {}, filter);
    hits.sort();
    for (const float step_cap : {0.65F, kClimbReachM}) {
    for (int i = 0; i < hits.count; ++i) {
        const RayCastResult &hit = hits.hits[static_cast<std::size_t>(i)];
        const RVec3 point = ray.GetPointOnRay(hit.mFraction);
        if ((!down && holding && float(point.GetY() - climb_hand_.GetY()) > step_cap) ||
            Vec3(point - at).Length() > kClimbReachM) {
            continue;
        }
        PointHit fingers;
        physics_.GetNarrowPhaseQuery().CollidePoint(point + Vec3(0.0F, 0.04F, 0.0F), fingers, {}, {}, filter);
        if (fingers.hit) {
            continue;
        }
        // Her arm's way to it: up in front of the face to just over the hold
        // (a hand goes over a lip's edge, not through its corner); the last
        // few centimeters down onto it are the room above it, checked above.
        const RVec3 chest = at + Vec3(0.0F, 0.6F, 0.0F);
        const RVec3 over = point + Vec3(0.0F, 0.08F, 0.0F) - Vec3(c.out_x, 0.0F, c.out_z) * 0.12F;
        const Vec3 reach(over - chest);
        RayCastResult obstruction;
        if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(chest, reach), obstruction, {}, {}, filter) &&
            (1.0F - obstruction.mFraction) * reach.Length() > 0.06F) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point).GetY() < 0.7F) {
            continue;
        }
        BodyID rests_body;
        SubShapeID rests_member;
        if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), point, filter, rests_body, rests_member) ||
            rests_body != hit.mBodyID) {
            continue;
        }
        climb_support_ = hit.mBodyID;
        climb_member_ = rests_member;
        climb_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point);
        climb_hand_ = point;
        climb_hand_known_ = true;
        ++climb_grips_;
        return true;
    }
    if (down || !holding) {
        break;
    }
    }
    return false;
}

bool Matron::begin_climb(const int index, const bool down) {
    if (climb_retry_s_ > 0.0F) {
        return false;
    }
    climb_ = index;
    climb_down_ = down;
    lowering_ = down;
    mantling_ = false;
    climb_support_ = BodyID();
    climb_member_ = SubShapeID();
    climb_hand_known_ = false;
    climb_force_n_ = 0.0F;
    climb_stall_s_ = 0.0F;
    climb_overload_s_ = 0.0F;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    climb_best_y_ = -std::numeric_limits<float>::max();
    // Up: a hold overhead. Down: from its top, the first hold under its lip.
    reaching_up_ = false;
    if (!find_climb_hold(at, down)) {
        release_climb(true);
        return false;
    }
    ++climbs_done_;
    chase_way_held_ = true;  // a climb taken holds the chase window past its own ticks
    return true;
}

void Matron::release_climb(const bool failed) {
    climb_end_ = failed ? ClimbEnd::Failed : ClimbEnd::Done;
    climb_ = -1;
    reaching_up_ = false;
    climb_down_ = false;
    lowering_ = false;
    mantling_ = false;
    climb_support_ = BodyID();
    climb_member_ = SubShapeID();
    climb_force_n_ = 0.0F;
    if (failed) {
        ++climb_releases_;
        climb_retry_s_ = 2.0F;
        // The column it dropped her from, her found ways leave alone as long
        // (none when no hold of it was ever found).
        if (climb_hand_known_) {
            block_climb_at(float(climb_hand_.GetX()), float(climb_hand_.GetZ()));
        }
    }
    // No velocity write: a missed hand or broken hold leaves her momentum
    // and gravity to the same collision solver as every other falling body.
}

void Matron::climb(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Climb c = climb_spec(climb_);
    const RVec3 at = bodies.GetPosition(body_);
    const float feet = float(at.GetY()) - kStandCentre;
    if (climb_support_.IsInvalid() || !bodies.IsAdded(climb_support_)) {
        release_climb(true);
        return;
    }
    climb_hand_ = bodies.GetCenterOfMassTransform(climb_support_) * climb_local_hand_;
    climb_hand_known_ = true;
    // The body can survive while a compound's rung disappears or moves.
    // Revalidate the contact, the very part of the support she holds and not
    // merely its body (a compound is many rungs), before applying load.
    const ClimbBodyFilter support_filter(body_, player_);
    BodyID rests_body;
    SubShapeID rests_member;
    if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), climb_hand_, support_filter, rests_body, rests_member) ||
        rests_body != climb_support_ || rests_member != climb_member_) {
        release_climb(true);
        return;
    }

    if (Vec3(climb_hand_ - at).Length() > kClimbReachM) {
        release_climb(true);
        return;
    }
    const auto floor_under = [&] {
        RayCastResult floor;
        return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor,
                                                      {}, {}, support_filter);
    };
    Vec3 target(c.x, float(climb_hand_.GetY()) - (reaching_up_ ? 0.30F : 0.65F), c.z);
    float progress = 0.0F;
    if (!climb_down_) {
        // The hand can only advance to another reachable surface; empty space
        // above the last rung is not a hold. Retain the old hand during a miss.
        // With none higher in reach yet, she pulls up higher on the one she
        // has (the hand at her chest), to reach further.
        if (!mantling_ && climb_hand_.GetY() - at.GetY() < 0.85F) {
            reaching_up_ = !find_climb_hold(at, false);
        }
        // Over the top from a hand near the lip, or from the last hold under
        // a lip she cannot hold on its line (a plate's edge stands out from
        // the rungs): up and over it once her body has climbed up to that top
        // hold, if her body fits all the way.
        if (!mantling_ && climb_hand_.GetY() - at.GetY() < 0.75F &&
            (climb_hand_.GetY() > c.lip_y - 0.4F || (reaching_up_ && climb_hand_.GetY() > c.lip_y - 0.7F))) {
            // Reuse the same full-body clearance query as standing after a fall,
            // up over the lip, half way out and landed: collisions still
            // arbitrate every step in between.
            const RVec3 lifted(c.x, c.lip_y + kStandCentre + 0.12F, c.z);
            const RVec3 half = lifted + Vec3(c.out_x, 0.0F, c.out_z) * (0.5F * kMantleOutM);
            const RVec3 landed = lifted + Vec3(c.out_x, 0.0F, c.out_z) * kMantleOutM;
            if (!shape_fits(shape_, lifted) || !shape_fits(shape_, half) || !shape_fits(shape_, landed)) {
                release_climb(true);
                return;
            }
            mantling_ = true;
            climb_stall_s_ = 0.0F;
        }
        if (mantling_) {
            target.SetY(c.lip_y + kStandCentre + 0.12F);
            if (feet > c.lip_y + 0.03F) {
                target.SetX(c.x + c.out_x * kMantleOutM);
                target.SetZ(c.z + c.out_z * kMantleOutM);
            }
            const float out = (float(at.GetX()) - c.x) * c.out_x + (float(at.GetZ()) - c.z) * c.out_z;
            // Verify actual landing steel, not just the authored destination.
            if (out > 0.8F && feet >= c.lip_y - 0.08F && floor_under()) {
                release_climb(false);
                return;
            }
        }
        progress = float(at.GetY()) +
                   (mantling_ ? (float(at.GetX()) - c.x) * c.out_x + (float(at.GetZ()) - c.z) * c.out_z : 0.0F);
    } else if (lowering_) {
        // Off the top backward onto its face, her hands on the hold under the
        // lip: at the height she has, to its line, then down it.
        target = Vec3(c.x, float(at.GetY()), c.z);
        const float to_line = distance_xz(float(at.GetX()), float(at.GetZ()), c.x, c.z);
        progress = -to_line;
        if (to_line < 0.12F) {
            // On its face: from here her progress is measured going down.
            lowering_ = false;
            climb_stall_s_ = 0.0F;
            progress = -float(at.GetY());
            climb_best_y_ = progress - 0.05F;
        }
    } else {
        // Hand under hand: hanging off the hold she has, and with her hands
        // high over her, the next one down at her chest. Her feet down onto
        // its foot's support is the end of it; air under them is not.
        if (climb_hand_.GetY() - at.GetY() > 0.95F) {
            find_climb_hold(at, true);
        }
        target.SetY(std::max(float(climb_hand_.GetY()) - 1.15F, c.foot_y + kStandCentre));
        if (feet <= c.foot_y + 0.06F && floor_under()) {
            release_climb(false);
            return;
        }
        progress = -float(at.GetY());
    }
    const Vec3 relative = bodies.GetLinearVelocity(body_) - bodies.GetPointVelocity(climb_support_, climb_hand_);
    Vec3 wanted = (target - Vec3(at)) * 3.0F;
    const float rate = mantling_ ? kMantleMps : kClimbMps;
    if (wanted.Length() > rate) {
        wanted *= rate / wanted.Length();
    }
    // Clearance is enforced by the real capsule and solver. A jam consumes
    // finite force and times out; it never teleports through the lip/ceiling.
    Vec3 force = kMassKg * ((wanted - relative) * 7.0F - physics_.GetGravity());
    const float requested = force.Length();
    climb_overload_s_ = requested > kClimbGripMaxN ? climb_overload_s_ + dt : 0.0F;
    if (climb_overload_s_ > 0.18F) {
        release_climb(true);
        return;
    }
    if (requested > kClimbGripMaxN) {
        force *= kClimbGripMaxN / requested;
    }
    if (progress > climb_best_y_ + 0.04F) {
        climb_best_y_ = progress;
        climb_stall_s_ = 0.0F;
    } else {
        climb_stall_s_ += dt;
    }
    if (climb_stall_s_ > 2.5F) {
        release_climb(true);
        return;
    }
    climb_force_n_ = force.Length();
    bodies.AddForce(body_, force);
    if (bodies.GetMotionType(climb_support_) == EMotionType::Dynamic) {
        bodies.AddForce(climb_support_, -force, climb_hand_);
    }
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    speed_ = 0.0F;
    stride_ += bodies.GetLinearVelocity(body_).Length() * dt;
}

// The old rung climber as a way of hers (matron_mover.hpp): to a ladder's
// foot and onto it (follow_climb), then up or down it (climb) inside her hunt.
MoverStatus Matron::ClimbWay::step(const float dt, traversal::Budget & /*budget*/) {
    if (m_.climb_ >= 0) {
        m_.climb(dt);
        return MoverStatus::Running;
    }
    if (!m_.follow_climb(dt, goal_.at)) {
        return MoverStatus::Idle;
    }
    return status() == MoverStatus::Running ? MoverStatus::Running : MoverStatus::Walking;
}

void Matron::ClimbWay::cancel() {
    if (m_.climb_ >= 0) {
        m_.release_climb(false);
    }
}

MoverStatus Matron::ClimbWay::status() const {
    return m_.climb_ >= 0 ? MoverStatus::Running : MoverStatus::Idle;
}

MoverView Matron::ClimbWay::view() const {
    MoverView v;
    v.tick = MoverTick::Hunt;
    v.turns = true;
    v.hand = m_.climb_hand_;   // her report's hand while no way shows one: the last she held
    if (m_.climb_ < 0) {
        return v;
    }
    v.drives = true;
    v.held = true;
    v.lands = true;
    v.holds_chase = true;
    v.blow_proof = true;
    v.hands = true;
    v.move_active = true;
    v.move = m_.mantling_ ? traversal::MoveKind::Mantle
                          : (m_.climb_down_ ? traversal::MoveKind::RungDown : traversal::MoveKind::RungUp);
    v.phase = m_.mantling_ ? 3 : 0;
    v.hand_shown = true;
    v.force_shown = true;
    v.force_n = m_.climb_force_n_;
    v.hand_held = !m_.climb_support_.IsInvalid();
    if (v.hand_held) {
        // Its rungs run across her facing (out is steel-to-her-side), so her
        // left is (out_z, 0, -out_x); her feet land on its top deck.
        const Climb spec = m_.climb_spec(m_.climb_);
        v.rungs_across = true;
        v.rung_left = Vec3(spec.out_z, 0.0F, -spec.out_x);
        v.hold_body = m_.climb_support_;
        v.hold_member = m_.climb_member_;
        v.top = RVec3(spec.top_x, spec.lip_y, spec.top_z);
    }
    return v;
}

bool Matron::ClimbWay::let_go() {
    if (m_.climb_ < 0) {
        return false;
    }
    m_.release_climb(true);
    return true;
}

} // namespace scraperx::sim
