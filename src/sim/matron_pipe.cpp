// A shaft riser by her hands (matron.hpp PipeTarget, discovered, never a
// fixed coordinate): over the rail of the deck she leaves, along the riser's
// climb side, over the rail of the deck she goes to. Gravity is on all the
// way and she meets every steel she passes. Her hands are one capped force
// at a real contact, the rail's top or the riser's face, kept in that body's
// frame and checked every step, as on her ladders (her Climber's holds); a
// dynamic support takes the reaction. On the riser's face the grip is
// friction: what holds her up or lets her down is at most her squeeze times
// kPipeFriction. A hand that loses its steel, a load past her grip for a
// moment, or no headway for 2.5 s lets go: on the riser her legs alone slow
// her (kPipeSlideAccel) until they meet the flange under her; at a rail she
// drops where she is.

#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
using namespace JPH;

namespace {
using grip::ClimbBodyFilter;
using grip::hand_rests_on;
constexpr float kTurnRate = 7.0F;
constexpr float kPiF = 3.14159265358979F;
constexpr float kPipeStallS = 2.5F;
constexpr float kPipeOverloadS = 0.18F;
constexpr float kPipeOffAxisM = 0.85F;    // a hold on the riser or its flange: its corner is 0.49 m off its axis, a flange's 0.78
constexpr float kPipeLegsLineN = 800.0F;  // her legs' pull back onto her line while she slides
// Her legs come down onto a flange: they bear on it, a support, not a grip.
// What they can take is her one budget (3.7 times her weight); past it they
// give and she sinks, but they do not open.
constexpr float kPipeLegsOnFlangeN = traversal::Limits{}.total_force_n;
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
float distance_xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
// Level, from a point toward a discovered riser's axis.
Vec3 into_riser(const RVec3 from, const float centre_x, const float centre_z) {
    const Vec3 in(centre_x - float(from.GetX()), 0.0F, centre_z - float(from.GetZ()));
    return in.LengthSq() > 1.0e-6F ? in.Normalized() : Vec3::sAxisX();
}
// What a hand on a face holds: the face met reaching in toward the riser
// from just off the hand, within a few centimetres.
bool hand_on_face(const NarrowPhaseQuery &query, const RVec3 point, const BodyFilter &filter, BodyID &body,
                  SubShapeID &member, const float centre_x, const float centre_z) {
    const Vec3 in = into_riser(point, centre_x, centre_z);
    const RRayCast ray(point - in * 0.05F, in * 0.1F);
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    ClosestHitCollisionCollector<CastRayCollector> hit;
    query.CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit()) {
        return false;
    }
    body = hit.mHit.mBodyID;
    member = hit.mHit.mSubShapeID2;
    return true;
}
} // namespace

bool Matron::pipe_rail_hold_for(const PipeTarget &target, const float deck, const RVec3 at) {
    const ClimbBodyFilter filter(body_, player_);
    const float top = deck + target.rail_top_m;
    for (const float dz : {0.0F, 0.03F, -0.03F}) {
        const RVec3 origin = target.crossing_axis == 0 ? RVec3(at.GetX(), top + 0.15F, target.rail_coord + dz)
                                                       : RVec3(target.rail_coord + dz, top + 0.15F, at.GetZ());
        const RRayCast ray(origin, Vec3(0.0F, -0.3F, 0.0F));
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
        if (!hit.HadHit()) {
            continue;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
        if (Vec3(point - at).Length() > kClimbReachM) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
        if (!lock.Succeeded() ||
            lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY() < 0.7F) {
            continue;
        }
        BodyID rests_body;
        SubShapeID rests_member;
        if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), point, filter, rests_body, rests_member) ||
            rests_body != hit.mHit.mBodyID) {
            continue;
        }
        pipe_support_ = rests_body;
        pipe_member_ = rests_member;
        pipe_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point);
        pipe_hand_ = point;
        pipe_on_face_ = false;
        return true;
    }
    return false;
}

// Reaching level toward the riser's axis from her line at height y: the first
// steel met is a hold if it is the riser's (or a flange's) standing face in
// her reach. Anything loose between her and it is not the riser.
bool Matron::pipe_face_hold_for(const PipeTarget &target, const RVec3 at, const float y) {
    const ClimbBodyFilter filter(body_, player_);
    const RVec3 from(at.GetX(), y, at.GetZ());
    const RRayCast ray(from, into_riser(from, target.centre_x, target.centre_z) * kClimbReachM);
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    ClosestHitCollisionCollector<CastRayCollector> hit;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit()) {
        return false;
    }
    const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
    if (Vec3(point - at).Length() > kClimbReachM ||
        distance_xz(float(point.GetX()), float(point.GetZ()), target.centre_x, target.centre_z) > kPipeOffAxisM) {
        return false;
    }
    const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
    if (!lock.Succeeded() || !lock.GetBody().IsStatic() ||
        std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY()) > 0.3F) {
        return false;
    }
    pipe_support_ = hit.mHit.mBodyID;
    pipe_member_ = hit.mHit.mSubShapeID2;
    pipe_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point);
    pipe_hand_ = point;
    pipe_on_face_ = true;
    return true;
}

// A riser she discovered for a pipe run, all of it from the scan and from
// probing the real steel. The scan's risers that span both decks, nearest
// first; for each, the corner nearest her approach, then the rail she must
// cross: stepping from her stand toward that corner, the first horizontal
// top a short way over her deck is the rail, and its height over the deck
// is the rail's top. Her line is that corner out along the climb axis by
// her radius plus clearance, and across the rail by her radius plus
// clearance; her deck stand is the same line back across the rail by her
// radius plus a stand clearance. Nothing here names a place in the world.
bool Matron::discover_pipe_target(const float from_floor, const float to_floor, const float ax, const float az,
                                  PipeTarget &out) {
    if (scanned_ == nullptr) {
        return false;
    }
    const float lo = std::min(from_floor, to_floor);
    const float hi = std::max(from_floor, to_floor);
    std::vector<const traversal::Riser *> candidates;
    for (const traversal::Riser &riser : scanned_->risers()) {
        if (riser.bottom <= lo + 0.1F && riser.top >= hi - 0.1F) {
            candidates.push_back(&riser);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [&](const traversal::Riser *a, const traversal::Riser *b) {
        return distance_xz(ax, az, a->x, a->z) < distance_xz(ax, az, b->x, b->z);
    });
    const ClimbBodyFilter filter(body_, player_);
    for (const traversal::Riser *riser : candidates) {
        const float cx = riser->x;
        const float cz = riser->z;
        const int sx = ax >= cx ? 1 : -1;
        const int sz = az >= cz ? 1 : -1;
        const float corner_x = cx + float(sx) * riser->half_x;
        const float corner_z = cz + float(sz) * riser->half_z;
        const float dx = corner_x - ax;
        const float dz = corner_z - az;
        const float dist = std::hypot(dx, dz);
        if (dist < 0.5F) {
            continue;
        }
        (void)dx;
        (void)dz;
        (void)dist;
        // The rail: probed on fine lines across the corner, not along
        // her approach and not on a coarse grid. Her approach direction
        // says nothing about which way the rail runs, a far approach
        // crosses the rail line well before the riser, and a thin rail
        // falls between coarse grid lines. A rail is a horizontal top
        // found at one coordinate across, on several lines along.
        struct RailHypothesis {
            int axis;
            float coord;
            float top;
            int score;
        };
        std::vector<RailHypothesis> hypotheses;
        for (int axis = 0; axis < 2; ++axis) {
            const float corner_along = axis == 0 ? corner_x : corner_z;
            const float corner_across = axis == 0 ? corner_z : corner_x;
            // Per line (5 offsets along the rail), per step across: the
            // top hit, if it is a static horizontal top over her deck.
            // [line][step] with step 0 at corner_across - 2.0 m.
            float line_top[5][81];
            bool line_hit[5][81] = {};
            for (int li = 0; li < 5; ++li) {
                const float along = corner_along - 0.6F + float(li) * 0.3F;
                for (int si = 0; si < 81; ++si) {
                    const float across = corner_across - 2.0F + float(si) * 0.05F;
                    const float px = axis == 0 ? along : across;
                    const float pz = axis == 0 ? across : along;
                    const RRayCast ray(RVec3(px, from_floor + 2.5F, pz), Vec3(0.0F, -3.0F, 0.0F));
                    RayCastSettings settings;
                    settings.mTreatConvexAsSolid = false;
                    ClosestHitCollisionCollector<CastRayCollector> hit;
                    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
                    if (!hit.HadHit()) {
                        continue;
                    }
                    const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
                    const float top = float(point.GetY());
                    if (top <= from_floor + 0.3F || top >= from_floor + 1.6F) {
                        continue;
                    }
                    const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
                    if (!lock.Succeeded() || !lock.GetBody().IsStatic() ||
                        lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY() < 0.7F) {
                        continue;
                    }
                    line_hit[li][si] = true;
                    line_top[li][si] = top;
                }
            }
            for (int si = 0; si < 81; ++si) {
                int score = 0;
                float top_min = 0.0F;
                float top_max = 0.0F;
                float top_sum = 0.0F;
                for (int li = 0; li < 5; ++li) {
                    if (!line_hit[li][si]) {
                        continue;
                    }
                    ++score;
                    top_sum += line_top[li][si];
                    top_min = score == 1 ? line_top[li][si] : std::min(top_min, line_top[li][si]);
                    top_max = score == 1 ? line_top[li][si] : std::max(top_max, line_top[li][si]);
                }
                if (score >= 3 && top_max - top_min < 0.25F) {
                    hypotheses.push_back(
                        RailHypothesis{axis, corner_across - 2.0F + float(si) * 0.05F, top_sum / float(score) - from_floor, score});
                }
            }
        }
        std::sort(hypotheses.begin(), hypotheses.end(), [&](const RailHypothesis &a, const RailHypothesis &b) {
            if (a.score != b.score) {
                return a.score > b.score;
            }
            const float da = std::abs((a.axis == 0 ? corner_z : corner_x) - a.coord);
            const float db = std::abs((b.axis == 0 ? corner_z : corner_x) - b.coord);
            return da < db;
        });

        // Each hypothesis in turn: room for all of her over the rail and
        // out at her line, and a hand on that rail; else it is not the
        // rail she takes, and neither is this riser on this hypothesis.
        for (const RailHypothesis &hyp : hypotheses) {
            const int crossing_axis = hyp.axis;
            const float rail_coord = hyp.coord;
            const float rail_top = hyp.top;
            PipeTarget target;
            target.centre_x = cx;
            target.centre_z = cz;
            target.half_x = riser->half_x;
            target.half_z = riser->half_z;
            target.corner_x = corner_x;
            target.corner_z = corner_z;
            target.crossing_axis = crossing_axis;
            // From the rail toward the riser: the corner sits on the
            // riser's side of the rail, so the corner's coordinate along
            // the crossing axis, against the rail's, gives the crossing
            // direction.
            const float corner_across = crossing_axis == 0 ? corner_z : corner_x;
            const float diff = corner_across - rail_coord;
            target.crossing_sign = diff != 0.0F ? (diff > 0.0F ? 1 : -1) : (crossing_axis == 0 ? sz : sx);
            target.deck_sign = -target.crossing_sign;
            target.climb_sign = crossing_axis == 0 ? sx : sz;
            constexpr float kLineOutM = 0.60F;   // her radius plus clearance, off the riser's face
            constexpr float kLineOverRailM = 0.75F;  // her radius plus clearance, past the rail
            constexpr float kDeckBackM = 0.45F;      // her radius plus clearance, back from the rail
            if (crossing_axis == 0) {
                target.line_x = corner_x + float(sx) * kLineOutM;
                target.line_z = rail_coord + float(target.crossing_sign) * kLineOverRailM;
                target.deck_x = target.line_x;
                target.deck_z = rail_coord + float(target.deck_sign) * kDeckBackM;
            } else {
                target.line_z = corner_z + float(sz) * kLineOutM;
                target.line_x = rail_coord + float(target.crossing_sign) * kLineOverRailM;
                target.deck_z = target.line_z;
                target.deck_x = rail_coord + float(target.deck_sign) * kDeckBackM;
            }
            target.rail_coord = rail_coord;
            target.rail_top_m = rail_top;
            const float over = from_floor + rail_top + kPipeOverRailM + kStandCentre;
            const bool deck_fits = shape_fits(shape_, RVec3(target.deck_x, over, target.deck_z));
            const bool line_fits = shape_fits(shape_, RVec3(target.line_x, over, target.line_z));
            // The deck stand must stand on her own deck: a riser across
            // the world has stands on decks far above her floor, and
            // walking to one of those strands her away from her hunt.
            // The scan's own patches say what deck is under the stand.
            const int stand_patch = scanned_->patch_at(Vec3(target.deck_x, from_floor, target.deck_z), 0.5F);
            const bool stand_on_floor = stand_patch >= 0;
            // No hand-on-rail check here: that check measures reach from
            // where she stands, and discovery runs from far off. The
            // mount itself (get_on_pipe) still checks her hand on the
            // rail when she is at the deck stand, as before.
            if (!deck_fits || !line_fits || !stand_on_floor) {
                pipe_support_ = BodyID();
                continue;
            }
            out = target;
            out.valid = true;
            out.from_floor = from_floor;
            out.to_floor = to_floor;
            return true;
        }
    }
    return false;
}

bool Matron::pipe_riser_spans(const float from_floor, const float to_floor) const {
    if (scanned_ == nullptr) {
        return false;
    }
    const float lo = std::min(from_floor, to_floor);
    const float hi = std::max(from_floor, to_floor);
    for (const traversal::Riser &riser : scanned_->risers()) {
        if (riser.bottom <= lo + 0.1F && riser.top >= hi - 0.1F) {
            return true;
        }
    }
    return false;
}

// Her hands on the rail where she stands, and room for all of her over it and
// out at her line; else nothing starts and she tries again in a moment.
bool Matron::get_on_pipe(const float from_floor, const float to_floor) {
    if (climb_retry_s_ > 0.0F) {
        return false;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    // Mount the riser the approach walked to, not a fresh discovery: a
    // fresh discovery at the stand can pick a nearer riser than the one
    // she walked to, and then the mount fails against the wrong rail
    // forever. Fresh discovery only when no target is cached for the pair.
    PipeTarget target;
    if (pipe_target_.valid && pipe_target_.from_floor == from_floor && pipe_target_.to_floor == to_floor) {
        target = pipe_target_;
    } else if (!discover_pipe_target(from_floor, to_floor, float(at.GetX()), float(at.GetZ()), target)) {
        pipe_support_ = BodyID();
        climb_retry_s_ = 2.0F;
        return false;
    }
    const float over = from_floor + target.rail_top_m + kPipeOverRailM + kStandCentre;
    if (!shape_fits(shape_, RVec3(target.deck_x, over, target.deck_z)) ||
        !shape_fits(shape_, RVec3(target.line_x, over, target.line_z)) ||
        !pipe_rail_hold_for(target, from_floor, at)) {
        pipe_support_ = BodyID();
        climb_retry_s_ = 2.0F;
        return false;
    }
    pipe_target_ = target;
    pipe_mount_failed_ = false;
    drop_stock();
    pipe_from_ = from_floor;
    pipe_to_ = to_floor;
    pipe_phase_ = PipePhase::Mount;
    pipe_over_ = false;
    pipe_s_ = 0.0F;
    pipe_hold_m_ = 0.0F;
    pipe_best_ = -std::numeric_limits<float>::max();
    pipe_stall_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    pipe_flange_seen_ = false;
    ++pipe_links_;
    ++pipe_holds_;   // her hands on the rail: a knock
    return true;
}

void Matron::get_off_pipe(const bool failed) {
    pipe_phase_ = PipePhase::Off;
    pipe_support_ = BodyID();
    pipe_drive_dv_ = Vec3::sZero();
    pipe_force_n_ = 0.0F;
    const int way = pipe_way(pipe_from_, pipe_to_);
    if (failed) {
        // This way of the pipe she leaves alone a while (past its retries,
        // until the world changes). Her level is what her feet find
        // (settle_level), not where she meant to go.
        if (way >= 0) {
            PipeWayMemory &memory = pipe_ways_[static_cast<std::size_t>(way)];
            if (memory.failures < 255) {
                ++memory.failures;
            }
            memory.blocked_until_s = memory.failures > kPipeWayRetries ? std::numeric_limits<float>::infinity()
                                                                       : clock_s_ + kPipeWayBlockS;
        }
        ++climb_releases_;
        climb_retry_s_ = 2.0F;
        return;
    }
    // Along it: this way of the pipe carried her, and is worth trying again.
    if (way >= 0) {
        pipe_ways_[static_cast<std::size_t>(way)].failures = 0;
        pipe_ways_[static_cast<std::size_t>(way)].blocked_until_s = 0.0F;
    }
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        if (std::abs(levels_[i].floor_y - pipe_to_) < 0.3F) {
            level_ = static_cast<int>(i);
        }
    }
    feet_y_ = pipe_to_;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    pick_patrol(float(at.GetX()), float(at.GetZ()));
}

void Matron::tear_off_pipe() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const float feet = float(at.GetY()) - kStandCentre;
    // The flange under her legs: straight down beside the riser's face, the
    // first top met.
    const ClimbBodyFilter filter(body_, player_);
    const float legs_x = pipe_target_.crossing_axis == 0
                             ? pipe_target_.centre_x + float(pipe_target_.climb_sign) * (pipe_target_.half_x + 0.10F)
                             : float(at.GetX());
    const float legs_z = pipe_target_.crossing_axis == 0
                             ? float(at.GetZ())
                             : pipe_target_.centre_z + float(pipe_target_.climb_sign) * (pipe_target_.half_z + 0.10F);
    const RRayCast ray(RVec3(legs_x, feet + 0.05F, legs_z), Vec3(0.0F, -2.0F * kStackPitch, 0.0F));
    RayCastResult hit;
    pipe_stop_y_ = physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)
                       ? float(ray.GetPointOnRay(hit.mFraction).GetY())
                       : -std::numeric_limits<float>::max();
    pipe_slide_mps_ = std::max(0.0F, -bodies.GetLinearVelocity(body_).GetY());
    pipe_support_ = BodyID();
    pipe_phase_ = PipePhase::Slide;
}

void Matron::ride_pipe(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const NarrowPhaseQuery &query = physics_.GetNarrowPhaseQuery();
    const RVec3 at = bodies.GetPosition(body_);
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float feet = float(at.GetY()) - kStandCentre;
    const Vec3 v = bodies.GetLinearVelocity(body_);
    const ClimbBodyFilter filter(body_, player_);
    const bool down = pipe_to_ < pipe_from_;
    const float from_rail = pipe_from_ + pipe_target_.rail_top_m;
    const float to_rail = pipe_to_ + pipe_target_.rail_top_m;
    const float over_centre = kPipeOverRailM + kStandCentre;   // her centre over a rail's top, going over it
    const bool on_line = distance_xz(ax, az, pipe_target_.line_x, pipe_target_.line_z) < 0.5F;
    const float legs_x = pipe_target_.crossing_axis == 0
                             ? pipe_target_.centre_x + float(pipe_target_.climb_sign) * (pipe_target_.half_x + 0.10F)
                             : ax;
    const float legs_z = pipe_target_.crossing_axis == 0
                             ? az
                             : pipe_target_.centre_z + float(pipe_target_.climb_sign) * (pipe_target_.half_z + 0.10F);
    pipe_s_ += dt;
    speed_ = 0.0F;
    stride_ += v.Length() * dt;
    // Each kPipeHoldM her hands go a knock (the rail's too); each flange passed a bong.
    pipe_hold_m_ += v.Length() * dt;
    while (pipe_hold_m_ >= kPipeHoldM) {
        pipe_hold_m_ -= kPipeHoldM;
        ++pipe_holds_;
    }
    const float was_feet = feet - v.GetY() * dt;
    // Each flange passed a bong: a short downward ray at her legs point,
    // spanning where her feet were to where they are, crossing a horizontal
    // top she has not counted at that height yet.
    {
        const float hi = std::max(was_feet, feet) + 0.10F;
        const RRayCast ray(RVec3(legs_x, hi, legs_z), Vec3(0.0F, -(std::abs(was_feet - feet) + 0.20F), 0.0F));
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        query.CastRay(ray, settings, hit, {}, {}, filter);
        if (hit.HadHit()) {
            const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
            const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
            if (lock.Succeeded() && lock.GetBody().IsStatic() &&
                lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY() > 0.7F) {
                const float top = float(point.GetY());
                if (!pipe_flange_seen_ || std::abs(top - pipe_last_flange_top_) > 0.5F) {
                    ++pipe_flanges_;
                    pipe_last_flange_top_ = top;
                    pipe_flange_seen_ = true;
                }
            }
        }
    }
    // Her one budget on the pipe this tick: her hands' (or legs') force on
    // the steel, and any swat of her arm, all drawn from it.
    const traversal::Limits law{};
    traversal::Budget budget{law.total_force_n, 0.0F, law.power_w, 0.0F, v};
    Vec3 on_her = Vec3::sZero();
    const auto report = [&](const Vec3 force, const Vec3 against) {
        on_her += force;
        pipe_force_n_ = budget.used;
        pipe_peak_force_n_ = std::max(pipe_peak_force_n_, pipe_force_n_);
        pipe_peak_power_w_ = std::max(pipe_peak_power_w_, std::abs(force.Dot(against)));
        pipe_drive_dv_ = (on_her / kMassKg + physics_.GetGravity()) * dt;
    };
    const auto apply = [&](const Vec3 wanted_force) {
        const bool support = !pipe_support_.IsInvalid() && bodies.IsAdded(pipe_support_);
        budget.velocity = v - (support ? bodies.GetPointVelocity(pipe_support_, pipe_hand_) : Vec3::sZero());
        const Vec3 force = budget.take(wanted_force);
        bodies.AddForce(body_, force);
        if (support && bodies.GetMotionType(pipe_support_) == EMotionType::Dynamic) {
            bodies.AddForce(pipe_support_, -force, pipe_hand_);
        }
        report(force, budget.velocity);
    };
    // Her hands gone: on the riser her legs still hold it; at a rail, down.
    const auto lost = [&] {
        if (on_line && pipe_phase_ != PipePhase::Mount) {
            tear_off_pipe();
        } else {
            get_off_pipe(true);
        }
    };
    if (pipe_phase_ == PipePhase::Slide) {
        if (feet <= pipe_stop_y_ + 0.02F) {
            // Her legs onto the flange: the slam, and it holds her.
            pipe_slam_mps_ = std::max(0.0F, -v.GetY());
            pipe_stun_s_ = kPipeStunSeconds + kPipeStunPerMps * std::max(0.0F, pipe_slam_mps_ - kPipeStunFromMps);
            pipe_slide_mps_ = 0.0F;
            ++pipe_slams_;
            ++knockdowns_;
            rage_ = std::min(1.0F, rage_ + kKnockdownRage);
            BodyID flange;
            SubShapeID member;
            const RVec3 legs(legs_x, pipe_stop_y_, legs_z);
            if (!hand_rests_on(query, legs, filter, flange, member)) {
                get_off_pipe(true);
                return;
            }
            const BodyLockRead lock(physics_.GetBodyLockInterface(), flange);
            if (!lock.Succeeded()) {
                get_off_pipe(true);
                return;
            }
            pipe_support_ = flange;
            pipe_member_ = member;
            pipe_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * legs);
            pipe_hand_ = legs;
            pipe_on_face_ = false;
            pipe_phase_ = PipePhase::Stunned;
            pipe_stall_s_ = 0.0F;
            return;
        }
        // Her squeeze, less than her weight, and her legs round to her line.
        Vec3 line = kMassKg * (Vec3(pipe_target_.line_x - ax, 0.0F, pipe_target_.line_z - az) * 3.0F -
                               Vec3(v.GetX(), 0.0F, v.GetZ())) *
                    7.0F;
        if (line.Length() > kPipeLegsLineN) {
            line *= kPipeLegsLineN / line.Length();
        }
        pipe_slide_mps_ = std::max(0.0F, -v.GetY());
        apply(line + Vec3(0.0F, kMassKg * (-physics_.GetGravity().GetY() - kPipeSlideAccel), 0.0F));
        return;
    }
    // The hand she has: still on its steel, the very part of it, in reach.
    if (pipe_support_.IsInvalid() || !bodies.IsAdded(pipe_support_)) {
        lost();
        return;
    }
    pipe_hand_ = bodies.GetCenterOfMassTransform(pipe_support_) * pipe_local_hand_;
    {
        BodyID body;
        SubShapeID member;
        const bool held = pipe_on_face_
                              ? hand_on_face(query, pipe_hand_, filter, body, member, pipe_target_.centre_x,
                                             pipe_target_.centre_z)
                              : hand_rests_on(query, pipe_hand_, filter, body, member);
        if (!held || body != pipe_support_ || member != pipe_member_ || Vec3(pipe_hand_ - at).Length() > kClimbReachM) {
            lost();
            return;
        }
    }
    float pace = 0.4F;   // gone frail on it: slow
    if (stage_ == Stage::Changed) {
        const bool burst = std::fmod(pipe_s_, kTurnCycleSeconds) < kTurnBurstSeconds;
        pace = burst ? (down ? kPipeChangedDownBurstMps : kPipeChangedUpBurstMps) : kPipeChangedPauseMps;
    } else if (stage_ == Stage::Dangerous) {
        pace = down ? kPipeDownMps : kPipeUpMps;
    }
    Vec3 target(at);
    float rate = kMantleMps;
    float progress = 0.0F;
    float facing = std::atan2(pipe_target_.centre_x - ax, pipe_target_.centre_z - az);
    switch (pipe_phase_) {
    case PipePhase::Mount: {
        // Up beside the rail on her hands, out over it to her line, and onto
        // the riser; where the riser ends flush with the deck, down on the
        // rail until the riser is in reach.
        const float to_line = distance_xz(ax, az, pipe_target_.line_x, pipe_target_.line_z);
        pipe_over_ = pipe_over_ || feet > from_rail + 0.03F;
        if (!pipe_over_) {
            target = Vec3(pipe_target_.deck_x, from_rail + over_centre, pipe_target_.deck_z);
            progress = feet - from_rail;
        } else if (to_line > 0.12F) {
            target = Vec3(pipe_target_.line_x, from_rail + over_centre, pipe_target_.line_z);
            progress = 10.0F - to_line;
        } else if (pipe_face_hold_for(pipe_target_, at, float(at.GetY()) + (down ? 0.3F : 0.6F)) ||
                   pipe_face_hold_for(pipe_target_, at, float(at.GetY())) ||
                   pipe_face_hold_for(pipe_target_, at, float(at.GetY()) - 0.3F)) {
            pipe_phase_ = PipePhase::Climb;
            pipe_best_ = -std::numeric_limits<float>::max();
            pipe_stall_s_ = 0.0F;
            return;
        } else {
            target = Vec3(pipe_target_.line_x, from_rail - 1.15F, pipe_target_.line_z);
            rate = kClimbMps;
            progress = 20.0F + from_rail - feet;
        }
        break;
    }
    case PipePhase::Climb: {
        rate = pace;
        if (!down) {
            // The deck's rail over her in her reach: onto it, and over.
            const RVec3 rail_point =
                pipe_target_.crossing_axis == 0 ? RVec3(ax, to_rail, pipe_target_.rail_coord) : RVec3(pipe_target_.rail_coord, to_rail, az);
            if (Vec3(rail_point - at).Length() < kClimbReachM - 0.1F && pipe_rail_hold_for(pipe_target_, pipe_to_, at)) {
                pipe_phase_ = PipePhase::Dismount;
                pipe_over_ = false;
                pipe_best_ = -std::numeric_limits<float>::max();
                pipe_stall_s_ = 0.0F;
                return;
            }
            // Hand over hand: with this hold at her chest (pulled up to within
            // a few centimetres of it, not crawling the last of the way), the
            // next overhead.
            bool reaching = false;
            if (pipe_hand_.GetY() - at.GetY() < 0.56F) {
                const RVec3 kept = pipe_hand_;
                const BodyID kept_body = pipe_support_;
                const SubShapeID kept_member = pipe_member_;
                const Vec3 kept_local = pipe_local_hand_;
                if (!pipe_face_hold_for(pipe_target_, at, float(at.GetY()) + 0.95F) &&
                    !pipe_face_hold_for(pipe_target_, at, float(at.GetY()) + 0.7F)) {
                    pipe_hand_ = kept;
                    pipe_support_ = kept_body;
                    pipe_member_ = kept_member;
                    pipe_local_hand_ = kept_local;
                    pipe_on_face_ = true;
                    reaching = true;
                }
            }
            // Always pulling for the hand at her shoulder, so each stroke runs at
            // her pace to the next hold rather than easing into a stop.
            (void)reaching;
            target = Vec3(pipe_target_.line_x, float(pipe_hand_.GetY()) - 0.30F, pipe_target_.line_z);
            progress = float(at.GetY());
        } else {
            // Down it at her pace, her grip sliding with her; with her feet
            // just over the deck's rail, onto it and over.
            if (feet <= to_rail + kPipeOverRailM + 0.1F && pipe_rail_hold_for(pipe_target_, pipe_to_, at)) {
                pipe_phase_ = PipePhase::Dismount;
                pipe_over_ = feet > to_rail + 0.03F;
                pipe_best_ = -std::numeric_limits<float>::max();
                pipe_stall_s_ = 0.0F;
                return;
            }
            const RVec3 kept = pipe_hand_;
            const BodyID kept_body = pipe_support_;
            const SubShapeID kept_member = pipe_member_;
            const Vec3 kept_local = pipe_local_hand_;
            if (!pipe_face_hold_for(pipe_target_, at, float(at.GetY()) + 0.3F)) {
                pipe_hand_ = kept;
                pipe_support_ = kept_body;
                pipe_member_ = kept_member;
                pipe_local_hand_ = kept_local;
                pipe_on_face_ = true;
            }
            target = Vec3(pipe_target_.line_x, to_rail + over_centre, pipe_target_.line_z);
            progress = -float(at.GetY());
        }
        break;
    }
    case PipePhase::Dismount: {
        // Up on the rail's top until her feet clear it, over it, and down
        // onto the deck: done only standing on its steel.
        const float out = pipe_target_.deck_sign *
                          (pipe_target_.crossing_axis == 0 ? az - pipe_target_.line_z : ax - pipe_target_.line_x);
        const float deck_off = pipe_target_.crossing_axis == 0 ? std::abs(pipe_target_.deck_z - pipe_target_.line_z)
                                                               : std::abs(pipe_target_.deck_x - pipe_target_.line_x);
        pipe_over_ = pipe_over_ || feet > to_rail + 0.03F;
        facing = pipe_target_.crossing_axis == 0 ? std::atan2(0.0F, float(pipe_target_.deck_sign))
                                                 : std::atan2(float(pipe_target_.deck_sign), 0.0F);
        if (!pipe_over_) {
            target = Vec3(pipe_target_.line_x, to_rail + over_centre, pipe_target_.line_z);
            progress = feet - to_rail;
        } else if (out < deck_off - 0.05F) {
            target = Vec3(pipe_target_.deck_x, to_rail + over_centre, pipe_target_.deck_z);
            progress = 10.0F + out;
        } else {
            target = Vec3(pipe_target_.deck_x, pipe_to_ + kStandCentre, pipe_target_.deck_z);
            progress = 20.0F + to_rail - feet;
            RayCastResult floor;
            if (feet <= pipe_to_ + 0.06F &&
                query.CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor, {}, {}, filter)) {
                get_off_pipe();
                pipe_listen_s_ = kPipeListenSeconds;
                return;
            }
        }
        break;
    }
    case PipePhase::Stunned: {
        // Her legs over the flange, still a moment; then her hands again.
        target = Vec3(pipe_target_.line_x, pipe_stop_y_ + kStandCentre, pipe_target_.line_z);
        rate = kClimbMps;
        progress = 0.0F;
        pipe_stall_s_ = 0.0F;
        pipe_stun_s_ -= dt;
        if (pipe_stun_s_ <= 0.0F) {
            if (!pipe_face_hold_for(pipe_target_, at, float(at.GetY()) + (down ? 0.3F : 0.6F)) &&
                !pipe_face_hold_for(pipe_target_, at, float(at.GetY()))) {
                lost();
                return;
            }
            pipe_phase_ = PipePhase::Climb;   // and on, the way she was going
            pipe_best_ = -std::numeric_limits<float>::max();
            return;
        }
        break;
    }
    default:
        return;
    }
    // One capped force toward where her hands take her (as on her ladders);
    // on the riser's face its hold along the steel is friction.
    const Vec3 support_v = bodies.GetPointVelocity(pipe_support_, pipe_hand_);
    Vec3 wanted = (target - Vec3(at)) * 3.0F;
    if (wanted.Length() > rate) {
        wanted *= rate / wanted.Length();
    }
    Vec3 force = kMassKg * ((wanted - (v - support_v)) * 7.0F - physics_.GetGravity());
    const float friction = kPipeSqueezeN * kPipeFriction;
    if (pipe_on_face_ && std::abs(force.GetY()) > friction) {
        force.SetY(std::copysign(friction, force.GetY()));
    }
    const bool on_flange = pipe_phase_ == PipePhase::Stunned;
    const float most = on_flange ? kPipeLegsOnFlangeN : kClimbGripMaxN;
    const float requested = force.Length();
    pipe_overload_s_ = requested > most && !on_flange ? pipe_overload_s_ + dt : 0.0F;
    if (requested > most) {
        force *= most / requested;
    }
    if (progress > pipe_best_ + 0.04F) {
        pipe_best_ = progress;
        pipe_stall_s_ = 0.0F;
    } else {
        pipe_stall_s_ += dt;
    }
    if (pipe_overload_s_ > kPipeOverloadS || pipe_stall_s_ > kPipeStallS) {
        lost();
        return;
    }
    apply(force);
    yaw_ = turn_to(yaw_, facing, kTurnRate, dt);
    // A loose piece lying in her way (a drum come to rest on a flange) she
    // swats out into the shaft: her arm's push on it over the ticks it is
    // against her, from what her budget has left after her hold, the equal
    // and opposite push on her. It ends when the piece has what her swat
    // gives (1.6 m/s, at most kPipeSwatNs of momentum) or is out of reach.
    const Vec3 swat = (pipe_target_.crossing_axis == 0
                           ? Vec3(float(pipe_target_.climb_sign) * 0.7F, 0.3F,
                                  float(pipe_target_.crossing_sign) * 0.7F)
                           : Vec3(float(pipe_target_.crossing_sign) * 0.7F, 0.3F,
                                  float(pipe_target_.climb_sign) * 0.7F))
                          .Normalized();
    bool swatting = false;
    for (std::size_t i = 0; i < near_count_; ++i) {
        const Near &n = near_[i];
        if (!bodies.IsAdded(n.body) || n.mass_kg <= 0.0F) {
            continue;
        }
        const bool on_it = n.body == pipe_swat_body_;
        if (!on_it && n.velocity.Length() > 0.5F) {
            continue;
        }
        const RVec3 there = bodies.GetCenterOfMassPosition(n.body);
        const float along = std::clamp(float(there.GetY() - at.GetY()), -kHalfHeight, kHalfHeight);
        if ((Vec3(there - at) - Vec3(0.0F, along, 0.0F)).Length() > kRadius + n.radius + 0.1F) {
            continue;
        }
        const Vec3 its_v = bodies.GetLinearVelocity(n.body);
        const float gives = std::min(1.6F, kPipeSwatNs / n.mass_kg);
        const float has = its_v.Dot(swat);
        if (has >= gives) {
            continue;
        }
        budget.velocity = its_v - v;
        const Vec3 push = budget.take(swat * (n.mass_kg * (gives - has) / dt));
        if (push.LengthSq() <= 0.0F) {
            break;
        }
        bodies.AddForce(n.body, push, EActivation::Activate);
        bodies.AddForce(body_, -push);
        report(-push, budget.velocity);
        pipe_swat_body_ = n.body;
        swatting = true;
        break;
    }
    if (!swatting) {
        pipe_swat_body_ = BodyID();
    }
}

bool Matron::pipe_reaches(const float floor_y) noexcept {
    // A story floor (a whole number of stack pitches), not a named deck:
    // whether a riser actually spans it is checked where the riser is used.
    const float stories = floor_y / kStackPitch;
    return std::abs(stories - std::round(stories)) < 0.02F;
}

bool Matron::follow_pipe(const float dt, const RVec3 at) {
    // Frail, she cannot take hold of it.
    if (stage_ == Stage::Pest) {
        return false;
    }
    // Ruled out for the test: no way of the pipe, made or unmade.
    if (pipe_ruled_out_) {
        return false;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    float next = floor_y;
    if (lead_floor_ < floor_y - 0.6F) {
        next = floor_y - kStackPitch;
    } else if (lead_floor_ > floor_y + 0.6F) {
        next = floor_y + kStackPitch;
    }
    if (next == floor_y || !pipe_reaches(floor_y) || !pipe_reaches(next) || !pipe_riser_spans(floor_y, next)) {
        return false;
    }
    // A way that failed under her a moment ago she leaves alone a while.
    if (const int way = pipe_way(floor_y, next); way >= 0 && !pipe_way_open(way)) {
        return false;
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    // Discover the target from the scan, from wherever she stands, and
    // walk to its deck stand along her lines, as the fixed stand was
    // walked to before. Walking to a bare corner instead could put her
    // off the deck when that corner belongs to another member. A target
    // already found for this deck pair is reused, not re-probed each tick.
    PipeTarget target;
    if (pipe_target_.valid && pipe_target_.from_floor == floor_y && pipe_target_.to_floor == next) {
        target = pipe_target_;
    } else {
        if (!discover_pipe_target(floor_y, next, ax, az, target)) {
            return false;
        }
        pipe_target_ = target;
        pipe_mount_failed_ = false;
    }
    const float off = distance_xz(ax, az, target.deck_x, target.deck_z);
    if (off > 0.25F) {
        float wx = target.deck_x;
        float wz = target.deck_z;
        route_toward(ax, az, target.deck_x, target.deck_z, false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // Her hands on the rail: over it and onto the pipe (matron_pipe.cpp).
    // Whether this tick really attempts a mount is decided after the
    // retry countdown, as get_on_pipe decides it: a countdown that only
    // reaches zero on this tick still attempts.
    climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
    const bool retry_gated = climb_retry_s_ > 0.0F;
    walk_toward(dt, ax, az, 0.0F, accel);
    yaw_ = turn_to(yaw_, std::atan2(pipe_target_.crossing_axis == 0 ? 0.0F : float(pipe_target_.crossing_sign),
                                   pipe_target_.crossing_axis == 0 ? float(pipe_target_.crossing_sign) : 0.0F),
                  kTurnRate, dt);
    const bool mounted = get_on_pipe(floor_y, next);
    if (!mounted && !retry_gated) {
        // A real mount attempt failed. Once is a retry; twice on the same
        // target means the way is bogus, and standing here forever is
        // worse than leaving it alone a while so other movers carry her.
        const bool same_target =
            pipe_mount_failed_ && pipe_mount_failed_target_.centre_x == pipe_target_.centre_x &&
            pipe_mount_failed_target_.centre_z == pipe_target_.centre_z &&
            pipe_mount_failed_target_.deck_x == pipe_target_.deck_x &&
            pipe_mount_failed_target_.deck_z == pipe_target_.deck_z;
        if (same_target) {
            if (const int way = pipe_way(floor_y, next); way >= 0) {
                PipeWayMemory &memory = pipe_ways_[static_cast<std::size_t>(way)];
                if (memory.failures < 255) {
                    ++memory.failures;
                }
                memory.blocked_until_s = memory.failures > kPipeWayRetries
                                             ? std::numeric_limits<float>::infinity()
                                             : clock_s_ + kPipeWayBlockS;
            }
            pipe_target_.valid = false;
            pipe_mount_failed_ = false;
            return false;
        }
        pipe_mount_failed_target_ = pipe_target_;
        pipe_mount_failed_ = true;
    }
    return true;
}

int Matron::pipe_way(const float from_floor, const float to_floor) {
    // One story apart, one memory per deck pair, created on first use.
    if (std::abs(std::abs(from_floor - to_floor) - kStackPitch) > 0.1F) {
        return -1;
    }
    for (std::size_t i = 0; i < pipe_ways_.size(); ++i) {
        if (std::abs(pipe_ways_[i].from - from_floor) < 0.1F && std::abs(pipe_ways_[i].to - to_floor) < 0.1F) {
            return static_cast<int>(i);
        }
    }
    pipe_ways_.push_back(PipeWayMemory{from_floor, to_floor, 0, 0.0F, false});
    return static_cast<int>(pipe_ways_.size() - 1);
}

bool Matron::pipe_way_open(const int way) const noexcept {
    if (way < 0 || static_cast<std::size_t>(way) >= pipe_ways_.size()) {
        return false;
    }
    const PipeWayMemory &memory = pipe_ways_[static_cast<std::size_t>(way)];
    return !memory.ruled_out && clock_s_ >= memory.blocked_until_s;
}

void Matron::pipe_struck(const RVec3 at) {
    if (pipe_target_.valid &&
        distance_xz(float(at.GetX()), float(at.GetZ()), pipe_target_.line_x, pipe_target_.line_z) < 0.5F) {
        tear_off_pipe();
        return;
    }
    // Still at a rail, over the band: her hands off it, down onto whatever
    // steel her feet find.
    get_off_pipe(true);
    const Vec3 way = blow_impulse_.LengthSq() > 1.0e-4F ? Vec3(blow_impulse_.GetX(), 0.0F, blow_impulse_.GetZ())
                                                       : Vec3::sZero();
    fall_x_ = way.LengthSq() > 1.0e-6F ? way.Normalized().GetX() : std::sin(yaw_);
    fall_z_ = way.LengthSq() > 1.0e-6F ? way.Normalized().GetZ() : std::cos(yaw_);
    knock_down();
}

void Matron::report_pipe() {
    view_.on_pipe = pipe_phase_ != PipePhase::Off;
    view_.pipe_head_down = pipe_phase_ == PipePhase::Climb && stage_ == Stage::Changed && pipe_to_ < pipe_from_;
    view_.pipe_sliding = pipe_phase_ == PipePhase::Slide;
    view_.pipe_peak_force_n = pipe_peak_force_n_;
    view_.pipe_peak_power_w = pipe_peak_power_w_;
    if (pipe_target_.valid) {
        view_.pipe_centre_x = pipe_target_.centre_x;
        view_.pipe_centre_z = pipe_target_.centre_z;
        view_.pipe_line_x = pipe_target_.line_x;
        view_.pipe_line_z = pipe_target_.line_z;
    }
    view_.pipe_stunned = pipe_phase_ == PipePhase::Stunned;
    view_.pipe_listening = pipe_phase_ == PipePhase::Off && pipe_listen_s_ > 0.0F;
    view_.pipe_links = pipe_links_;
    view_.pipe_holds = pipe_holds_;
    view_.pipe_flanges = pipe_flanges_;
    view_.pipe_slams = pipe_slams_;
    view_.pipe_slam_mps = pipe_slam_mps_;
}

void Matron::restore_pipe() noexcept {
    // Off the pipe, whatever had her on it.
    pipe_support_ = BodyID();
    pipe_drive_dv_ = Vec3::sZero();
    pipe_target_ = PipeTarget{};
    pipe_mount_failed_ = false;
    pipe_flange_seen_ = false;
    pipe_phase_ = PipePhase::Off;
    pipe_links_ = 0;
    pipe_holds_ = 0;
    pipe_flanges_ = 0;
    pipe_slams_ = 0;
    pipe_slam_mps_ = 0.0F;
    pipe_stun_s_ = 0.0F;
    pipe_listen_s_ = 0.0F;
}

// The corner pipe as a way of hers (matron_mover.hpp): to the rail's corner
// and onto it (follow_pipe), then along it (ride_pipe) inside her hunt.
MoverStatus Matron::PipeWay::step(const float dt, traversal::Budget & /*budget*/) {
    if (m_.pipe_phase_ != PipePhase::Off) {
        m_.ride_pipe(dt);
        return MoverStatus::Running;
    }
    if (!m_.follow_pipe(dt, goal_.at)) {
        return MoverStatus::Idle;
    }
    return status() == MoverStatus::Running ? MoverStatus::Running : MoverStatus::Walking;
}

void Matron::PipeWay::cancel() {
    m_.pipe_phase_ = PipePhase::Off;
    m_.pipe_support_ = BodyID();
    m_.pipe_drive_dv_ = Vec3::sZero();
    m_.pipe_force_n_ = 0.0F;
}

MoverStatus Matron::PipeWay::status() const {
    return m_.pipe_phase_ != PipePhase::Off ? MoverStatus::Running : MoverStatus::Idle;
}

MoverView Matron::PipeWay::view() const {
    MoverView v;
    v.tick = MoverTick::Hunt;
    if (m_.pipe_phase_ == PipePhase::Off) {
        return v;
    }
    v.drives = true;
    v.held = true;
    v.lands = true;
    // Her legs sliding, or stunned on a flange: no blow is read. Else her
    // hands hold her against her weight, and a blow is read in all three
    // directions (take_blow).
    v.blow_proof = m_.pipe_phase_ == PipePhase::Slide || m_.pipe_phase_ == PipePhase::Stunned;
    v.floor_in_hands = true;
    v.drive_dv = m_.pipe_drive_dv_;
    v.force_shown = true;
    v.force_n = m_.pipe_force_n_;
    v.has_goal = true;
    v.goal_floor_y = m_.pipe_to_;
    return v;
}

bool Matron::PipeWay::let_go() {
    if (m_.pipe_phase_ == PipePhase::Off || m_.pipe_phase_ == PipePhase::Slide) {
        return false;
    }
    m_.tear_off_pipe();
    return true;
}

} // namespace scraperx::sim
