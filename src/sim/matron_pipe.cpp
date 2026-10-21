// The shaft's corner pipe by her hands (matron.hpp kPipe*): over the rail of
// the deck she leaves, along the riser's west side, over the rail of the deck
// she goes to. Gravity is on all the way and she meets every steel she
// passes. Her hands are one capped force at a real contact, the rail's top or
// the riser's face, kept in that body's frame and checked every step, as on
// her ladders (matron.cpp climb()); a dynamic support takes the reaction. On
// the riser's face the grip is friction: what holds her up or lets her down
// is at most her squeeze times kPipeFriction. A hand that loses its steel, a
// load past her grip for a moment, or no headway for 2.5 s lets go: on the
// riser her legs alone slow her (kPipeSlideAccel) until they meet the flange
// under her; at a rail she drops where she is.

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
// What they can take is her legs' strength, about four times her weight; past
// it they give and she sinks, but they do not open.
constexpr float kPipeLegsOnFlangeN = 9000.0F;
float turn_to(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPiF);
    delta = std::clamp(delta, -rate * dt, rate * dt);
    return std::remainder(yaw + delta, 2.0F * kPiF);
}
float distance_xz(const float ax, const float az, const float bx, const float bz) { return std::hypot(ax - bx, az - bz); }
// Level, from a point toward the riser's axis.
Vec3 into_riser(const RVec3 from) {
    const Vec3 in(Matron::kPipeCentreX - float(from.GetX()), 0.0F, Matron::kPipeCentreZ - float(from.GetZ()));
    return in.LengthSq() > 1.0e-6F ? in.Normalized() : Vec3::sAxisX();
}
// What a hand on a face holds: the face met reaching in toward the riser
// from just off the hand, within a few centimetres.
bool hand_on_face(const NarrowPhaseQuery &query, const RVec3 point, const BodyFilter &filter, BodyID &body,
                  SubShapeID &member) {
    const Vec3 in = into_riser(point);
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

bool Matron::pipe_rail_hold(const float deck, const RVec3 at) {
    const ClimbBodyFilter filter(body_, player_);
    const float top = deck + kPipeRailTopM;
    for (const float dz : {0.0F, 0.03F, -0.03F}) {
        const RRayCast ray(RVec3(at.GetX(), top + 0.15F, kPipeRailZ + dz), Vec3(0.0F, -0.3F, 0.0F));
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
bool Matron::pipe_face_hold(const RVec3 at, const float y) {
    const ClimbBodyFilter filter(body_, player_);
    const RVec3 from(at.GetX(), y, at.GetZ());
    const RRayCast ray(from, into_riser(from) * kClimbReachM);
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false;
    ClosestHitCollisionCollector<CastRayCollector> hit;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit()) {
        return false;
    }
    const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
    if (Vec3(point - at).Length() > kClimbReachM ||
        distance_xz(float(point.GetX()), float(point.GetZ()), kPipeCentreX, kPipeCentreZ) > kPipeOffAxisM) {
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

// Her hands on the rail where she stands, and room for all of her over it and
// out at her line; else nothing starts and she tries again in a moment.
bool Matron::get_on_pipe(const float from_floor, const float to_floor) {
    if (climb_retry_s_ > 0.0F) {
        return false;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    const float over = from_floor + kPipeRailTopM + kPipeOverRailM + kStandCentre;
    if (!shape_fits(shape_, RVec3(kPipeDeckX, over, kPipeDeckZ)) ||
        !shape_fits(shape_, RVec3(kPipeLineX, over, kPipeLineZ)) || !pipe_rail_hold(from_floor, at)) {
        pipe_support_ = BodyID();
        climb_retry_s_ = 2.0F;
        return false;
    }
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
    climb_end_ = ClimbEnd::None;
    ++pipe_links_;
    ++pipe_holds_;   // her hands on the rail: a knock
    return true;
}

void Matron::get_off_pipe(const bool failed) {
    pipe_phase_ = PipePhase::Off;
    pipe_support_ = BodyID();
    pipe_drive_dv_ = Vec3::sZero();
    pipe_force_n_ = 0.0F;
    climb_end_ = failed ? ClimbEnd::Failed : ClimbEnd::Done;
    if (failed) {
        // Set out on by herself (a route records its own): her map leaves
        // this way alone a while. Her level is what her feet find
        // (settle_level), not where she meant to go.
        const std::size_t way = pipe_way(pipe_from_, pipe_to_);
        if (!route_on_edge_ && way < routes::kMaxEdges) {
            routes_.fail(way, routes::Reason::Failed, clock_s_);
            ++route_failures_;
        }
        ++climb_releases_;
        climb_retry_s_ = 2.0F;
        return;
    }
    // Along it by herself (a route records its own): her map knows it carried her.
    const std::size_t way = pipe_way(pipe_from_, pipe_to_);
    if (!route_on_edge_ && way < routes::kMaxEdges) {
        routes_.succeed(way);
        ++route_edges_done_;
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
    const RRayCast ray(RVec3(kPipeCentreX - 0.45F, feet + 0.05F, at.GetZ()), Vec3(0.0F, -2.0F * kStackPitch, 0.0F));
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
    const float from_rail = pipe_from_ + kPipeRailTopM;
    const float to_rail = pipe_to_ + kPipeRailTopM;
    const float over_centre = kPipeOverRailM + kStandCentre;   // her centre over a rail's top, going over it
    const bool on_line = distance_xz(ax, az, kPipeLineX, kPipeLineZ) < 0.5F;
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
    for (float deck = kPipeLowestFloor; deck <= kDeckTop + 0.1F; deck += kStackPitch) {
        const float top = deck - kPipeFlangeUnderDeckM;
        if ((was_feet - top) * (feet - top) < 0.0F) {
            ++pipe_flanges_;
        }
    }
    const auto apply = [&](const Vec3 force) {
        pipe_force_n_ = force.Length();
        bodies.AddForce(body_, force);
        if (!pipe_support_.IsInvalid() && bodies.GetMotionType(pipe_support_) == EMotionType::Dynamic) {
            bodies.AddForce(pipe_support_, -force, pipe_hand_);
        }
        pipe_drive_dv_ = (force / kMassKg + physics_.GetGravity()) * dt;
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
            const RVec3 legs(kPipeCentreX - 0.45F, pipe_stop_y_, az);
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
        Vec3 line = kMassKg * (Vec3(kPipeLineX - ax, 0.0F, kPipeLineZ - az) * 3.0F - Vec3(v.GetX(), 0.0F, v.GetZ())) * 7.0F;
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
        const bool held = pipe_on_face_ ? hand_on_face(query, pipe_hand_, filter, body, member)
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
    float facing = std::atan2(kPipeCentreX - ax, kPipeCentreZ - az);
    switch (pipe_phase_) {
    case PipePhase::Mount: {
        // Up beside the rail on her hands, out over it to her line, and onto
        // the riser; at 154, down on the rail until the riser is in reach.
        const float to_line = distance_xz(ax, az, kPipeLineX, kPipeLineZ);
        pipe_over_ = pipe_over_ || feet > from_rail + 0.03F;
        if (!pipe_over_) {
            target = Vec3(kPipeDeckX, from_rail + over_centre, kPipeDeckZ);
            progress = feet - from_rail;
        } else if (to_line > 0.12F) {
            target = Vec3(kPipeLineX, from_rail + over_centre, kPipeLineZ);
            progress = 10.0F - to_line;
        } else if (pipe_face_hold(at, float(at.GetY()) + (down ? 0.3F : 0.6F)) ||
                   pipe_face_hold(at, float(at.GetY())) || pipe_face_hold(at, float(at.GetY()) - 0.3F)) {
            pipe_phase_ = PipePhase::Climb;
            pipe_best_ = -std::numeric_limits<float>::max();
            pipe_stall_s_ = 0.0F;
            return;
        } else {
            target = Vec3(kPipeLineX, from_rail - 1.15F, kPipeLineZ);
            rate = kClimbMps;
            progress = 20.0F + from_rail - feet;
        }
        break;
    }
    case PipePhase::Climb: {
        rate = pace;
        if (!down) {
            // The deck's rail over her in her reach: onto it, and over.
            if (Vec3(RVec3(ax, to_rail, kPipeRailZ) - at).Length() < kClimbReachM - 0.1F && pipe_rail_hold(pipe_to_, at)) {
                pipe_phase_ = PipePhase::Dismount;
                pipe_over_ = false;
                pipe_best_ = -std::numeric_limits<float>::max();
                pipe_stall_s_ = 0.0F;
                return;
            }
            // Hand over hand: with this hold at her chest, the next overhead.
            bool reaching = false;
            if (pipe_hand_.GetY() - at.GetY() < 0.5F) {
                const RVec3 kept = pipe_hand_;
                const BodyID kept_body = pipe_support_;
                const SubShapeID kept_member = pipe_member_;
                const Vec3 kept_local = pipe_local_hand_;
                if (!pipe_face_hold(at, float(at.GetY()) + 0.95F) && !pipe_face_hold(at, float(at.GetY()) + 0.7F)) {
                    pipe_hand_ = kept;
                    pipe_support_ = kept_body;
                    pipe_member_ = kept_member;
                    pipe_local_hand_ = kept_local;
                    pipe_on_face_ = true;
                    reaching = true;
                }
            }
            target = Vec3(kPipeLineX, float(pipe_hand_.GetY()) - (reaching ? 0.30F : 0.50F), kPipeLineZ);
            progress = float(at.GetY());
        } else {
            // Down it at her pace, her grip sliding with her; with her feet
            // just over the deck's rail, onto it and over.
            if (feet <= to_rail + kPipeOverRailM + 0.1F && pipe_rail_hold(pipe_to_, at)) {
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
            if (!pipe_face_hold(at, float(at.GetY()) + 0.3F)) {
                pipe_hand_ = kept;
                pipe_support_ = kept_body;
                pipe_member_ = kept_member;
                pipe_local_hand_ = kept_local;
                pipe_on_face_ = true;
            }
            target = Vec3(kPipeLineX, to_rail + over_centre, kPipeLineZ);
            progress = -float(at.GetY());
        }
        break;
    }
    case PipePhase::Dismount: {
        // Up on the rail's top until her feet clear it, over it, and down
        // onto the deck: done only standing on its steel.
        const float out = az - kPipeLineZ;
        pipe_over_ = pipe_over_ || feet > to_rail + 0.03F;
        facing = std::atan2(0.0F, 1.0F);
        if (!pipe_over_) {
            target = Vec3(kPipeLineX, to_rail + over_centre, kPipeLineZ);
            progress = feet - to_rail;
        } else if (out < kPipeDeckZ - kPipeLineZ - 0.05F) {
            target = Vec3(kPipeDeckX, to_rail + over_centre, kPipeDeckZ);
            progress = 10.0F + out;
        } else {
            target = Vec3(kPipeDeckX, pipe_to_ + kStandCentre, kPipeDeckZ);
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
        target = Vec3(kPipeLineX, pipe_stop_y_ + kStandCentre, kPipeLineZ);
        rate = kClimbMps;
        progress = 0.0F;
        pipe_stall_s_ = 0.0F;
        pipe_stun_s_ -= dt;
        if (pipe_stun_s_ <= 0.0F) {
            if (!pipe_face_hold(at, float(at.GetY()) + (down ? 0.3F : 0.6F)) && !pipe_face_hold(at, float(at.GetY()))) {
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
    // A loose piece lying in her way (a drum come to rest on a flange) she
    // swats out into the shaft: a blow of her arm, its reaction hers.
    const Vec3 swat = Vec3(-0.7F, 0.3F, -0.7F).Normalized();
    for (std::size_t i = 0; i < near_count_; ++i) {
        const Near &n = near_[i];
        if (!bodies.IsAdded(n.body) || n.velocity.Length() > 0.5F) {
            continue;
        }
        const RVec3 there = bodies.GetCenterOfMassPosition(n.body);
        const float along = std::clamp(float(there.GetY() - at.GetY()), -kHalfHeight, kHalfHeight);
        if ((Vec3(there - at) - Vec3(0.0F, along, 0.0F)).Length() > kRadius + n.radius + 0.1F) {
            continue;
        }
        const float impulse = std::min(n.mass_kg * 1.6F, kPipeSwatNs);
        bodies.AddImpulse(n.body, swat * impulse);
        bodies.AddImpulse(body_, -swat * impulse);
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
}

} // namespace scraperx::sim
