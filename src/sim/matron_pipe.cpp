// The shaft's corner pipe by her hands (matron.hpp kPipe*): over the rail of
// the deck she leaves, along the riser's west side, over the rail of the deck
// she goes to. Gravity is on all the way and she meets every steel she
// passes. Her hands are one capped force at a real contact, the rail's top or
// the riser's face, kept in that body's frame and checked every step, as on
// her ladders (her Climber's holds); a dynamic support takes the reaction. On
// the riser's face the grip is friction: what holds her up or lets her down
// is at most her squeeze times kPipeFriction. A hand that loses its steel, a
// load past her grip for a moment, or no headway for 2.5 s lets go: on the
// riser her legs alone slow her (kPipeSlideAccel) until they meet the flange
// under her; at a rail she drops where she is.

#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"
#include "sim/matron_effort.hpp"

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
constexpr float kPipeOffAxisM = 0.85F;    // a hold on the riser or its flange: its corner is 0.49 m off its axis, a flange's 0.68
// Where her legs bear on a flange: on its top just out from the riser's
// face (0.35 m off its axis) and in from the flange's (0.40 m, clear of her
// on her line: world_solids.inc), on her side of it.
constexpr float kPipeLegsOffAxisM = 0.38F;
constexpr float kPipeLegsLineN = 800.0F;  // her legs' pull back onto her line while she slides
constexpr float kHoistRideMps = 1.0F;     // a hoist's car at her pace on it (matron_hoist.cpp's kBoardMps)
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

bool Matron::pipe_top_under(const float x, const float top_y, const float z, BodyID &body, SubShapeID &member,
                            RVec3 &point) const {
    const ClimbBodyFilter filter(body_, player_);
    for (const float dz : {0.0F, 0.03F, -0.03F}) {
        const RRayCast ray(RVec3(x, top_y + 0.15F, z + dz), Vec3(0.0F, -0.3F, 0.0F));
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
        if (!hit.HadHit()) {
            continue;
        }
        const RVec3 at = ray.GetPointOnRay(hit.mHit.mFraction);
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, at).GetY() < 0.7F) {
            continue;
        }
        BodyID rests_body;
        SubShapeID rests_member;
        if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), at, filter, rests_body, rests_member) ||
            rests_body != hit.mHit.mBodyID) {
            continue;
        }
        body = rests_body;
        member = rests_member;
        point = at;
        return true;
    }
    return false;
}

bool Matron::pipe_riser_top_under(const RVec3 at, float &top_y) const {
    const ClimbBodyFilter filter(body_, player_);
    const RRayCast ray(RVec3(kPipeCentreX, at.GetY(), kPipeCentreZ), Vec3(0.0F, -(kStandCentre + kPipeCatchFallM), 0.0F));
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = true;   // starting inside the riser: a hit at 0, it goes on past her
    ClosestHitCollisionCollector<CastRayCollector> hit;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
    if (!hit.HadHit() || hit.mHit.mFraction <= 0.0F) {
        return false;
    }
    const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
    const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
    if (!lock.Succeeded() || !lock.GetBody().IsStatic() ||
        lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY() < 0.7F) {
        return false;
    }
    top_y = float(point.GetY());
    return true;
}

bool Matron::pipe_corner_raised(const float deck) const {
    // Straight down at her stand by the rail's corner, from over the rail:
    // the first top met. Raised when it is within her perch of the rail's
    // top (a top she stands on as she perches on the rail), not the deck.
    const ClimbBodyFilter filter(body_, player_);
    const float from = deck + kPipeRailTopM + 0.5F;
    const RRayCast ray(RVec3(kPipeDeckX, from, kPipeDeckZ), Vec3(0.0F, -(kPipeRailTopM + 0.6F), 0.0F));
    RayCastResult hit;
    if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) {
        return false;
    }
    const float top = float(ray.GetPointOnRay(hit.mFraction).GetY());
    return std::abs(top - (deck + kPipeRailTopM)) <= 0.1F;
}

void Matron::pipe_take(const PipeGrip grip, const BodyID body, const SubShapeID member, const RVec3 point) {
    const BodyLockRead lock(physics_.GetBodyLockInterface(), body);
    pipe_support_ = body;
    pipe_member_ = member;
    pipe_local_hand_ = lock.Succeeded() ? Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point) : Vec3(point);
    pipe_hand_ = point;
    pipe_grip_ = grip;
}

// A hand on a top that runs along x (a rail's, the edge beam's) at z: where
// it is level with her middle, or under either shoulder.
bool Matron::pipe_top_hold(const float top_y, const float z, const RVec3 at, const PipeGrip grip) {
    const float side_x = std::cos(yaw_) * kPipeShoulderSideM;
    for (const float dx : {0.0F, side_x, -side_x}) {
        BodyID body;
        SubShapeID member;
        RVec3 point;
        if (pipe_top_under(float(at.GetX()) + dx, top_y, z, body, member, point) && pipe_in_hand_reach(at, point)) {
            pipe_take(grip, body, member, point);
            return true;
        }
    }
    return false;
}

bool Matron::pipe_rail_hold(const float deck, const RVec3 at) {
    if (pipe_top_hold(deck + kPipeRailTopM, kPipeRailZ, at, PipeGrip::Rail)) {
        return true;
    }
    // A raised corner's top runs out over the rail, its steel's edge on the
    // rail's line: her hand takes the top a hand's width in from that edge,
    // on her side of it.
    const float toward = float(at.GetZ()) < kPipeRailZ ? -0.03F : 0.03F;
    return pipe_corner_raised(deck) && pipe_top_hold(deck + kPipeRailTopM, kPipeRailZ + toward, at, PipeGrip::Rail);
}

bool Matron::pipe_lip_hold(const float deck, const RVec3 at) {
    return pipe_top_hold(deck - kPipeLipUnderDeckM, kPipeLipZ, at, PipeGrip::Lip);
}

// Her feet on the rail's top: under her soles' span, as her feet stand on a
// floor (sole_contact: from her middle, kStandCentre + 0.3 m down at most).
bool Matron::pipe_feet_hold(const float deck, const RVec3 at) {
    BodyID body;
    SubShapeID member;
    RVec3 point;
    if (!pipe_top_under(float(at.GetX()), deck + kPipeRailTopM, kPipeRailZ, body, member, point)) {
        return false;
    }
    const float below = float(at.GetY() - point.GetY());
    if (std::abs(float(at.GetZ() - point.GetZ())) > kPipeFootSpanM || below < kStandCentre - 0.1F ||
        below > kStandCentre + 0.3F) {
        return false;
    }
    pipe_take(PipeGrip::Feet, body, member, point);
    return true;
}

bool Matron::pipe_in_hand_reach(const RVec3 at, const RVec3 point) const noexcept {
    const Vec3 ahead(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 side(ahead.GetZ(), 0.0F, -ahead.GetX());
    const RVec3 middle = at + Vec3(0.0F, kPipeShoulderUpM - kPipeTuckDropM * pipe_tuck_drawn_, 0.0F) +
                         ahead * (kPipeShoulderAheadM + kPipeTuckAheadM * pipe_tuck_drawn_);
    for (const float s : {1.0F, -1.0F}) {
        if (Vec3(point - (middle + side * (s * kPipeShoulderSideM))).Length() <= kPipeHandReachM) {
            return true;
        }
    }
    return false;
}

bool Matron::pipe_grip_in_reach(const RVec3 at) const {
    switch (pipe_grip_) {
    case PipeGrip::Rail:
    case PipeGrip::Face:
    case PipeGrip::Lip:
        return pipe_in_hand_reach(at, pipe_hand_);
    case PipeGrip::Feet: {
        const float below = float(at.GetY() - pipe_hand_.GetY());
        // Her soles within kPipeFootSpanM of her middle (her soles' span,
        // matron.hpp): the full horizontal distance, not the z component
        // alone. The Perch step drives her middle along x toward kPipeDeckX
        // away from the planted take point, so the z-only check let the
        // point sit 0.26-0.28 m off her middle — past her span — while
        // still calling it held, and the contact truth exported a floor her
        // soles cannot be on (S6, J-063: 5 ticks on the pipe descent).
        // Out of reach it is let go, and the step below takes what is in
        // her reach now.
        const float across = std::hypot(float(at.GetX() - pipe_hand_.GetX()), float(at.GetZ() - pipe_hand_.GetZ()));
        return across <= kPipeFootSpanM + 0.01F && below >= kStandCentre - 0.1F && below <= kStandCentre + 0.3F;
    }
    case PipeGrip::Legs:
        return true;
    case PipeGrip::None:
    default:
        return false;
    }
}

// Reaching level toward the riser's axis from her line at height y: the first
// steel met is a hold if it is the riser's (or a flange's) standing face in
// her real arm's reach. Anything loose between her and it is not the riser.
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
    if (!pipe_in_hand_reach(at, point) ||
        distance_xz(float(point.GetX()), float(point.GetZ()), kPipeCentreX, kPipeCentreZ) > kPipeOffAxisM) {
        return false;
    }
    const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
    if (!lock.Succeeded() || !lock.GetBody().IsStatic() ||
        std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY()) > 0.3F) {
        return false;
    }
    pipe_take(PipeGrip::Face, hit.mHit.mBodyID, hit.mHit.mSubShapeID2, point);
    return true;
}

// Her hands on the rail where she stands, and room for all of her over it and
// out at her line; else nothing starts and she tries again in a moment.
bool Matron::get_on_pipe(const float from_floor, const float to_floor) {
    if (climb_retry_s_ > 0.0F) {
        return false;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    const float over = from_floor + kPipeRailTopM + kPipePerchM + kStandCentre;
    BodyID rail;
    SubShapeID member;
    RVec3 top;
    if (!shape_fits(shape_, RVec3(kPipeDeckX, over, kPipeDeckZ)) ||
        !shape_fits(shape_, RVec3(kPipeLineX, over, kPipeLineZ)) ||
        !pipe_top_under(float(at.GetX()), from_floor + kPipeRailTopM, kPipeRailZ, rail, member, top)) {
        pipe_support_ = BodyID();
        climb_retry_s_ = 2.0F;
        return false;
    }
    // Her hands on the rail's top if it is in their reach; else she steps up
    // to it first (the press). Standing on a raised corner (the top level
    // with the rail), her feet are already over the rail: she takes it on
    // her feet there and goes on from over it.
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_grip_ = PipeGrip::None;
    pipe_support_ = BodyID();
    const bool raised = pipe_corner_raised(from_floor);
    if (raised && !pipe_feet_hold(from_floor, at)) {
        climb_retry_s_ = 2.0F;
        return false;
    }
    if (!raised) {
        (void)pipe_rail_hold(from_floor, at);
    }
    drop_stock();
    pipe_from_ = from_floor;
    pipe_to_ = to_floor;
    pipe_phase_ = PipePhase::Mount;
    pipe_step_ = raised ? PipeStep::Over : PipeStep::Press;
    pipe_s_ = 0.0F;
    pipe_hold_m_ = 0.0F;
    pipe_best_ = -std::numeric_limits<float>::max();
    pipe_stall_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    ++pipe_links_;
    ++pipe_holds_;   // her hands on the rail: a knock
    return true;
}

void Matron::get_off_pipe(const bool failed) {
    pipe_phase_ = PipePhase::Off;
    pipe_support_ = BodyID();
    pipe_grip_ = PipeGrip::None;
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_swat_body_ = BodyID();
    pipe_drive_dv_ = Vec3::sZero();
    pipe_force_n_ = 0.0F;
    pipe_slide_mps_ = 0.0F;
    pipe_stun_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    pipe_stall_s_ = 0.0F;
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
    const RRayCast ray(RVec3(kPipeCentreX - kPipeLegsOffAxisM, feet + 0.05F, at.GetZ()), Vec3(0.0F, -2.0F * kStackPitch, 0.0F));
    RayCastResult hit;
    pipe_stop_y_ = physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)
                       ? float(ray.GetPointOnRay(hit.mFraction).GetY())
                       : -std::numeric_limits<float>::max();
    pipe_slide_mps_ = std::max(0.0F, -bodies.GetLinearVelocity(body_).GetY());
    pipe_support_ = BodyID();
    pipe_grip_ = PipeGrip::None;
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_swat_body_ = BodyID();
    pipe_best_ = -std::numeric_limits<float>::max();
    pipe_stall_s_ = 0.0F;
    pipe_overload_s_ = 0.0F;
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
        if (on_line && (pipe_phase_ != PipePhase::Mount || pipe_step_ == PipeStep::Catch)) {
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
            const RVec3 legs(kPipeCentreX - kPipeLegsOffAxisM, pipe_stop_y_, az);
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
            pipe_grip_ = PipeGrip::Legs;
            pipe_phase_ = PipePhase::Stunned;
            pipe_stall_s_ = 0.0F;
            return;
        }
        // Her squeeze, less than her weight, and her legs round to her line.
        Vec3 line = kMassKg * (Vec3(kPipeLineX - ax, 0.0F, kPipeLineZ - az) * 3.0F - Vec3(v.GetX(), 0.0F, v.GetZ())) * 7.0F;
        if (line.Length() > kPipeLegsLineN) {
            line *= kPipeLegsLineN / line.Length();
        }
        const float slide_progress = -feet;
        if (slide_progress > pipe_best_ + 0.04F) {
            pipe_best_ = slide_progress;
            pipe_stall_s_ = 0.0F;
        } else {
            pipe_stall_s_ += dt;
        }
        if (!on_line || pipe_stall_s_ > kPipeStallS) {
            get_off_pipe(true);
            return;
        }
        pipe_slide_mps_ = std::max(0.0F, -v.GetY());
        apply(line + Vec3(0.0F, kMassKg * (-physics_.GetGravity().GetY() - kPipeSlideAccel), 0.0F));
        return;
    }
    // What has her: still on its steel, the very part of it, and in the reach
    // of the hand or the feet that hold it (rule 37: her real arm, her soles'
    // span). Out of reach it is let go, and the step below takes what is in
    // her reach now, or she is off the pipe: never a hold past her reach.
    if (pipe_grip_ != PipeGrip::None) {
        bool held = !pipe_support_.IsInvalid() && bodies.IsAdded(pipe_support_);
        if (held) {
            pipe_hand_ = bodies.GetCenterOfMassTransform(pipe_support_) * pipe_local_hand_;
            BodyID body;
            SubShapeID member;
            held = (pipe_grip_ == PipeGrip::Face ? hand_on_face(query, pipe_hand_, filter, body, member)
                                                  : hand_rests_on(query, pipe_hand_, filter, body, member)) &&
                   body == pipe_support_ && member == pipe_member_ && pipe_grip_in_reach(at);
        }
        if (!held) {
            pipe_grip_ = PipeGrip::None;
            pipe_support_ = BodyID();
            pipe_let_go_y_ = float(at.GetY());
        }
    }
    float pace = pipe_pace(down);
    if (stage_ == Stage::Changed) {
        const bool burst = std::fmod(pipe_s_, kTurnCycleSeconds) < kTurnBurstSeconds;
        pace = burst ? (down ? kPipeChangedDownBurstMps : kPipeChangedUpBurstMps) : kPipeChangedPauseMps;
    }
    // The rail she crosses (Mount: the deck she leaves; else the deck she
    // goes to), her middle perched over it, and her tuck as she rises over it.
    const float deck = pipe_phase_ == PipePhase::Mount ? pipe_from_ : pipe_to_;
    const float rail = deck + kPipeRailTopM;
    const float perch_y = rail + kStandCentre + kPipePerchM;
    const auto tuck_for = [&](const float y) { return std::clamp(y - rail - kPipeTuckFromM, 0.0F, 1.0F); };
    const float shoulder_y = float(at.GetY()) + kPipeShoulderUpM - kPipeTuckDropM * pipe_tuck_drawn_;
    // The riser's face level with her shoulders, a little under or over them.
    const auto face_near = [&] {
        return pipe_face_hold(at, shoulder_y) || pipe_face_hold(at, shoulder_y - 0.3F) ||
               pipe_face_hold(at, shoulder_y + 0.3F);
    };
    // The riser's standing face level with her shins, in her legs' reach (her
    // brace on it: matron_contact.cpp).
    const auto riser_at_shins = [&] {
        const RVec3 shins(at.GetX(), at.GetY() - kStandCentre + kBraceShinM, at.GetZ());
        const RRayCast ray(shins, into_riser(shins) * kBraceReachM);
        RayCastSettings settings;
        settings.mTreatConvexAsSolid = false;
        ClosestHitCollisionCollector<CastRayCollector> hit;
        query.CastRay(ray, settings, hit, {}, {}, filter);
        if (!hit.HadHit()) {
            return false;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
        return lock.Succeeded() && lock.GetBody().IsStatic() &&
               std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY()) < 0.3F;
    };
    const auto yaw_off = [&](const float want) { return std::abs(std::remainder(want - yaw_, 2.0F * kPiF)); };
    const auto next_step = [&](const PipeStep step) {
        pipe_step_ = step;
        pipe_best_ = -std::numeric_limits<float>::max();
        pipe_stall_s_ = 0.0F;
    };
    constexpr float kToShaft = kPiF;   // facing the shaft (-z) from a deck
    constexpr float kToDeck = 0.0F;    // facing a deck (+z) from the shaft
    const float to_riser = std::atan2(kPipeCentreX - ax, kPipeCentreZ - az);
    Vec3 target(at);
    float rate = kMantleMps;
    float progress = 0.0F;
    float facing = to_riser;
    float tuck_want = 0.0F;
    switch (pipe_phase_) {
    case PipePhase::Mount:
        switch (pipe_step_) {
        case PipeStep::Press:
            // Up beside the rail on her hands, her legs drawn up under her so
            // her shoulders stay over it (a squat vault). Not yet on it, she
            // steps up to it on her feet, turned to it.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                walk_toward(dt, kPipeDeckX, kPipeDeckZ, 1.0F, kFootAccel);
                yaw_ = turn_to(yaw_, kToShaft, kTurnRate, dt);
                const float near = -distance_xz(ax, az, kPipeDeckX, kPipeDeckZ) - yaw_off(kToShaft);
                if (near > pipe_best_ + 0.04F) {
                    pipe_best_ = near;
                    pipe_stall_s_ = 0.0F;
                } else if ((pipe_stall_s_ += dt) > kPipeStallS) {
                    lost();
                    return;
                }
                pipe_force_n_ = 0.0F;
                return;
            }
            target = Vec3(kPipeDeckX, perch_y, kPipeDeckZ);
            tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            facing = kToShaft;
            progress = feet;
            rate = kPipePressMps;
            if (feet > rail + 0.5F * kPipePerchM) {
                next_step(PipeStep::Over);
            }
            break;
        case PipeStep::Over:
            // Over its top, tucked; her feet onto it once it is under her soles.
            if (pipe_grip_ != PipeGrip::Feet) {
                (void)pipe_feet_hold(deck, at);
            }
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeDeckX, perch_y, kPipeRailZ);
            tuck_want = 1.0F;
            facing = kToShaft;
            progress = -std::abs(az - kPipeRailZ);
            if (std::abs(az - kPipeRailZ) <= 0.04F && pipe_grip_ == PipeGrip::Feet) {
                next_step(PipeStep::Perch);
            }
            break;
        case PipeStep::Perch:
            // Perched on it, turned to the riser.
            if (pipe_grip_ == PipeGrip::None && !pipe_feet_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeDeckX, perch_y, kPipeRailZ);
            tuck_want = 1.0F;
            progress = -yaw_off(to_riser);
            if (yaw_off(to_riser) < 0.15F) {
                next_step(PipeStep::StepOff);
            }
            break;
        case PipeStep::StepOff:
            // Off it toward her line; the riser taken as soon as it is in her
            // reach, else she lets go and falls to it (Catch).
            if (face_near()) {
                pipe_phase_ = PipePhase::Climb;
                next_step(PipeStep::Press);
                return;
            }
            if (pipe_grip_ == PipeGrip::None) {
                next_step(PipeStep::Catch);
                return;
            }
            // Where the riser ends below her (its top found straight down its
            // axis) she falls to it: off the rail just fast enough to be at
            // her line when she has fallen to its top, else at kPipeStepOffMps.
            target = Vec3(kPipeLineX, perch_y, kPipeLineZ);
            rate = kPipeStepOffMps;
            if (float riser_top = 0.0F; pipe_riser_top_under(at, riser_top)) {
                const float fall = std::max(0.1F, float(at.GetY()) - (riser_top + kPipeCatchOverTopM));
                const float out = std::max(0.0F, distance_xz(ax, az, kPipeLineX, kPipeLineZ) - kPipeFootSpanM);
                rate = std::clamp(out / std::sqrt(2.0F * fall / 9.81F), 0.2F, kPipeStepOffMps);
            }
            tuck_want = 1.0F;
            progress = -distance_xz(ax, az, kPipeLineX, kPipeLineZ);
            break;
        case PipeStep::Catch:
        default:
            // Falling: the riser taken once her hand reaches it and her legs
            // can wrap it (its face level with her shins, as on it).
            if (riser_at_shins() && face_near()) {
                pipe_phase_ = PipePhase::Climb;
                next_step(PipeStep::Press);
                return;
            }
            if (pipe_let_go_y_ - float(at.GetY()) > kPipeCatchFallM) {
                lost();
                return;
            }
            tuck_want = 1.0F;
            progress = pipe_let_go_y_ - float(at.GetY());
            break;
        }
        break;
    case PipePhase::Climb: {
        rate = pace;
        // Off her line (just caught from a rail), she keeps her tuck till her
        // hold has drawn her in to it.
        const bool off_line = distance_xz(ax, az, kPipeLineX, kPipeLineZ) > 0.15F;
        if (off_line) {
            tuck_want = pipe_tuck_;
        }
        if (pipe_grip_ == PipeGrip::None && !face_near()) {
            lost();
            return;
        }
        // Near the deck she goes to, turned to it: its rail's top (or, where
        // the riser ends at it, its edge beam) in her reach, she takes it.
        if (std::abs(shoulder_y - rail) < kPipeDeckNearM) {
            facing = kToDeck;
            if (pipe_rail_hold(deck, at) || pipe_lip_hold(deck, at)) {
                pipe_phase_ = PipePhase::Dismount;
                next_step(PipeStep::Hang);
                return;
            }
        }
        if (!down) {
            // Hand over hand: with this hold down at her chest (pulled up to
            // within a few centimetres of it, not crawling the last of the
            // way), the next over it, in her reach.
            // (Just caught, still tucked, her shoulders low: the face over
            // them, else level with them, if that is higher than her hold.)
            if (pipe_hand_.GetY() - at.GetY() < kPipeRegripUpM) {
                const RVec3 kept = pipe_hand_;
                if (!pipe_face_hold(at, float(at.GetY()) + kPipeNextHoldUpM) &&
                    !pipe_face_hold(at, float(at.GetY()) + kPipeNextHoldLowM) &&
                    shoulder_y + 0.3F > float(kept.GetY()) + 0.05F) {
                    (void)(pipe_face_hold(at, shoulder_y + 0.3F) ||
                           (shoulder_y > float(kept.GetY()) + 0.05F && pipe_face_hold(at, shoulder_y)));
                }
            }
            // Always pulling for the hand at her shoulder, so each stroke runs at
            // her pace to the next hold rather than easing into a stop. Her drawn
            // shoulders, not upright ones: just caught, tucked, they sit under her
            // middle, and a pull for upright shoulders at the hold would draw her
            // down the riser till her stall ran out.
            const float shoulder_up = shoulder_y - float(at.GetY());
            target = Vec3(kPipeLineX, float(pipe_hand_.GetY()) - shoulder_up + (kPipeShoulderUpM - 0.30F), kPipeLineZ);
            progress = float(at.GetY());
        } else {
            // Down it at her pace, her grip sliding with her, past the deck's
            // rail till it is in her reach.
            (void)pipe_face_hold(at, float(at.GetY()) + 0.3F);
            // (no more than a metre under her at a time, so her pull in to
            // her line is not lost in her way down)
            target = Vec3(kPipeLineX, std::max(rail - 0.6F, float(at.GetY()) - (off_line ? 0.2F : 1.0F)), kPipeLineZ);
            progress = -float(at.GetY());
        }
        break;
    }
    case PipePhase::Dismount:
        switch (pipe_step_) {
        case PipeStep::Hang:
            // Hanging off the rail (or the edge beam, till the rail is in her
            // reach) beside the deck's edge, turned to it.
            if (pipe_grip_ != PipeGrip::Rail) {
                (void)pipe_rail_hold(deck, at);
            }
            if (pipe_grip_ == PipeGrip::None && !pipe_lip_hold(deck, at)) {
                lost();
                return;
            }
            facing = kToDeck;
            rate = kClimbMps;
            if (pipe_grip_ == PipeGrip::Lip) {
                target = Vec3(kPipeLineX, rail - 0.70F, kPipeHangZ);
                tuck_want = 0.3F;
            } else {
                target = Vec3(kPipeLineX, float(at.GetY()), kPipeHangZ);
                tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            }
            progress = -Vec3(target - Vec3(at)).Length();
            if (pipe_grip_ == PipeGrip::Rail && std::abs(az - kPipeHangZ) <= 0.06F) {
                next_step(PipeStep::Rise);
            }
            break;
        case PipeStep::Rise:
            // Up on the rail's top, tucked: clear of the edge beam, then in by
            // the rail, till her soles are over it.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeLineX, perch_y,
                          feet < deck - kPipeLipUnderDeckM + 0.05F ? kPipeHangZ : kPipeOutZ);
            tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            facing = kToDeck;
            progress = feet;
            rate = kPipePressMps;
            if (feet > rail + 0.5F * kPipePerchM) {
                next_step(PipeStep::Over);
            }
            break;
        case PipeStep::Over:
            if (pipe_grip_ != PipeGrip::Feet) {
                (void)pipe_feet_hold(deck, at);
            }
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeDeckX, perch_y, kPipeRailZ);
            tuck_want = 1.0F;
            facing = kToDeck;
            progress = -std::abs(az - kPipeRailZ);
            if (std::abs(az - kPipeRailZ) <= 0.04F && pipe_grip_ == PipeGrip::Feet) {
                // Over a raised corner her feet stand on its top: that is
                // her floor, and she is off the pipe onto it.
                if (pipe_corner_raised(deck)) {
                    get_off_pipe();
                    pipe_listen_s_ = kPipeListenSeconds;
                    return;
                }
                next_step(PipeStep::Perch);
            }
            break;
        case PipeStep::Perch:
            // Perched on it, turned to the shaft to go down off it backwards.
            if (pipe_grip_ == PipeGrip::None && !pipe_feet_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeDeckX, perch_y, kPipeRailZ);
            tuck_want = 1.0F;
            facing = kToShaft;
            progress = -yaw_off(kToShaft);
            if (yaw_off(kToShaft) < 0.15F) {
                next_step(PipeStep::Back);
            }
            break;
        case PipeStep::Back:
            // Back off it over the deck, her hands taking the rail as her feet
            // leave it.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeDeckX, perch_y, kPipeDeckZ);
            rate = kClimbMps;
            tuck_want = 1.0F;
            facing = kToShaft;
            progress = -std::abs(az - kPipeDeckZ);
            if (std::abs(az - kPipeDeckZ) <= 0.05F && pipe_grip_ == PipeGrip::Rail) {
                next_step(PipeStep::Down);
            }
            break;
        case PipeStep::Down:
        default: {
            // Down onto the deck on her hands: done only standing on its steel.
            if (pipe_grip_ == PipeGrip::None && !pipe_rail_hold(deck, at)) {
                lost();
                return;
            }
            target = Vec3(kPipeDeckX, pipe_to_ + kStandCentre, kPipeDeckZ);
            tuck_want = tuck_for(float(at.GetY()) + v.GetY() * kPipeTuckLeadS);
            rate = kPipePressMps;
            facing = kToShaft;
            progress = -feet;
            RayCastResult floor;
            if (feet <= pipe_to_ + 0.06F &&
                query.CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor, {}, {}, filter)) {
                get_off_pipe();
                pipe_listen_s_ = kPipeListenSeconds;
                return;
            }
            break;
        }
        }
        break;
    case PipePhase::Stunned: {
        // Her legs over the flange, still a moment; then her hands again.
        target = Vec3(kPipeLineX, pipe_stop_y_ + kStandCentre, kPipeLineZ);
        rate = kClimbMps;
        progress = 0.0F;
        pipe_stall_s_ = 0.0F;
        pipe_stun_s_ -= dt;
        if (pipe_stun_s_ <= 0.0F) {
            // (Folded over the flange her shoulders are low: the face level
            // with her middle first.)
            if (!pipe_face_hold(at, float(at.GetY())) && !pipe_face_hold(at, float(at.GetY()) + 0.3F)) {
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
    // Her tuck follows what the step asks, at a joint's pace.
    const float tuck_step = kPipeTuckRate * dt;
    pipe_tuck_ = std::clamp(tuck_want, pipe_tuck_ - tuck_step, pipe_tuck_ + tuck_step);
    pipe_tuck_drawn_ += (pipe_tuck_ - pipe_tuck_drawn_) * std::min(1.0F, dt / kPipeTuckDrawnTauS);
    if (progress > pipe_best_ + 0.04F) {
        pipe_best_ = progress;
        pipe_stall_s_ = 0.0F;
    } else {
        pipe_stall_s_ += dt;
    }
    // Nothing in her reach: no hold to pull on, she falls (gravity alone).
    if (pipe_grip_ == PipeGrip::None) {
        pipe_force_n_ = 0.0F;
        pipe_drive_dv_ = physics_.GetGravity() * dt;
        pipe_overload_s_ = 0.0F;
        if (pipe_stall_s_ > kPipeStallS) {
            lost();
            return;
        }
        yaw_ = turn_to(yaw_, facing, kTurnRate, dt);
        pipe_swat_body_ = BodyID();
        return;
    }
    // One capped force toward where her hands (or her feet, perched on a
    // rail's top) take her, as on her ladders; on the riser's face its hold
    // along the steel is friction.
    const Vec3 support_v = bodies.GetPointVelocity(pipe_support_, pipe_hand_);
    Vec3 wanted = (target - Vec3(at)) * 3.0F;
    if (wanted.Length() > rate) {
        wanted *= rate / wanted.Length();
    }
    Vec3 force = kMassKg * ((wanted - (v - support_v)) * 7.0F - physics_.GetGravity());
    const float friction = kPipeSqueezeN * kPipeFriction;
    if (pipe_grip_ == PipeGrip::Face && std::abs(force.GetY()) > friction) {
        force.SetY(std::copysign(friction, force.GetY()));
    }
    // Her legs (on a flange, or her feet on a rail's top) bear on it, a
    // support, not a grip: her one budget, and they do not open.
    const bool on_legs = pipe_grip_ == PipeGrip::Legs || pipe_grip_ == PipeGrip::Feet;
    const float most = on_legs ? kPipeLegsOnFlangeN : kClimbGripMaxN;
    const float requested = force.Length();
    pipe_overload_s_ = requested > most && !on_legs ? pipe_overload_s_ + dt : 0.0F;
    if (requested > most) {
        force *= most / requested;
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
    const Vec3 swat = Vec3(-0.7F, 0.3F, -0.7F).Normalized();
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
        // J-046 measurement only: the take above put +push on her in the
        // ledger; on her body it is -push (no new force, charged above).
        effort::add(effort::Path::HandsPush, 0.0F, 0.0F, 0.0F, -2.0F * push);
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
    if (floor_y < kPipeLowestFloor - 0.1F || floor_y > kDeckTop + 0.1F) {
        return false;
    }
    const float stories = floor_y / kStackPitch;
    return std::abs(stories - std::round(stories)) < 0.02F;
}

float Matron::pipe_next_toward(const float floor_y, const float lead_y) noexcept {
    // The deck nearest the lead, and the lead on it or on a top within
    // kPipeLeadM of it: a story toward it. Else (her own deck, or a top deep
    // inside a story) not the pipe's.
    const float deck = std::round(lead_y / kStackPitch) * kStackPitch;
    if (std::abs(deck - floor_y) < 1.0F || std::abs(lead_y - deck) > kPipeLeadM) {
        return floor_y;
    }
    return deck < floor_y ? floor_y - kStackPitch : floor_y + kStackPitch;
}

bool Matron::pipe_serves(const float next) {
    if (scanned_ == nullptr) {
        return true;
    }
    const int lead = scanned_->region_at(Vec3(heard_x_, lead_floor_, heard_z_));
    // The floor she gets off onto there: by the rail's corner, else that deck's largest.
    int far = scanned_->region_at(Vec3(kPipeDeckX, next, kPipeDeckZ));
    if (far < 0) {
        far = scanned_->region_near_top(next, 0.3F);
    }
    if (lead < 0 || far < 0) {
        return true;   // no floor of her map under the lead (a ladder, the air): the deck is as near as she gets
    }
    if (lead != pipe_serves_to_ || far != pipe_serves_from_) {
        pipe_serves_from_ = far;
        pipe_serves_to_ = lead;
        pipe_serves_ok_ = far == lead || std::abs(scanned_->regions()[static_cast<std::size_t>(lead)].top - next) > kPipeLeadM ||
                          !scanned_->plan(far, lead).empty();
    }
    return pipe_serves_ok_;
}

float Matron::pipe_pace(const bool down) const noexcept {
    if (stage_ == Stage::Changed) {
        // Its bursts and pauses, over a turn's cycle.
        const float burst = kTurnBurstSeconds / kTurnCycleSeconds;
        return burst * (down ? kPipeChangedDownBurstMps : kPipeChangedUpBurstMps) + (1.0F - burst) * kPipeChangedPauseMps;
    }
    if (stage_ == Stage::Dangerous) {
        return down ? kPipeDownMps : kPipeUpMps;
    }
    return 0.4F;   // gone frail on it: slow
}

float Matron::pipe_time_s(const RVec3 at, const float floor_y, const float stand_z) const noexcept {
    const float stories = std::max(1.0F, std::round(std::abs(lead_floor_ - floor_y) / kStackPitch));
    const float walk = distance_xz(float(at.GetX()), float(at.GetZ()), kPipeDeckX, stand_z) +
                       distance_xz(kPipeDeckX, stand_z, heard_x_, heard_z_);
    const bool down = lead_floor_ < floor_y;
    return walk / kHuntMps + stories * (kStackPitch / pipe_pace(down) + (down ? kPipeDownExtraS : kPipeUpExtraS));
}

bool Matron::own_way_sooner(const RVec3 at, const float pipe_s) {
    if (scanned_ == nullptr) {
        return false;
    }
    const int here = region_under();
    int goal = scanned_->region_at(Vec3(heard_x_, lead_floor_, heard_z_));
    if (goal < 0) {
        goal = scanned_->region_near_top(lead_floor_, 0.3F);
    }
    if (here < 0 || goal < 0 || here == goal) {
        return false;
    }
    if (here != own_way_from_ || goal != own_way_to_) {
        own_way_from_ = here;
        own_way_to_ = goal;
        own_way_moves_.clear();
        own_way_hoist_ = false;
        // Her map's own way there: its moves off the floor, each with its
        // cost (the scan's seconds); her walks between them are on her feet.
        for (const int e : scanned_->plan(here, goal)) {
            const traversal::Edge &edge = scanned_->edges()[static_cast<std::size_t>(e)];
            if (edge.kind != traversal::MoveKind::Walk) {
                own_way_moves_.push_back(OwnMove{edge.spec.start, edge.spec.goal, edge.spec.line, edge.cost});
            }
        }
        // Or a hoist (matron_hoist.cpp) where no way of the map joins them.
        own_way_hoist_ = own_way_moves_.empty() && find_hoist(here, goal, own_way_leg_);
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    float own_s = 0.0F;
    if (own_way_hoist_) {
        // To its landing, the car's travel at her own pace on it (she works
        // it: matron_hoist.cpp), and off its far landing to him.
        const Vec3 board = own_way_leg_.board_at;
        const Vec3 exit = own_way_leg_.exit_at;
        if (column_blocked_near(board.GetX(), board.GetZ())) {
            return false;
        }
        own_s = (distance_xz(ax, az, board.GetX(), board.GetZ()) + distance_xz(exit.GetX(), exit.GetZ(), heard_x_, heard_z_)) /
                    kHuntMps +
                std::abs(exit.GetY() - board.GetY()) / kHoistRideMps;
        return own_s < pipe_s;
    }
    if (own_way_moves_.empty()) {
        return false;
    }
    // Only a way her journey plans: each of its moves inside the reach of
    // her journey's own scan around her and the lead (start_journey_to).
    // From the deck 6 landing the C2 pole and ladder up from the duct lie
    // past it: not hers from there.
    const float lo_x = std::min(ax, heard_x_) - kJourneyScanMarginM;
    const float hi_x = std::max(ax, heard_x_) + kJourneyScanMarginM;
    const float lo_z = std::min(az, heard_z_) - kJourneyScanMarginM;
    const float hi_z = std::max(az, heard_z_) + kJourneyScanMarginM;
    const auto inside = [&](const Vec3 p) {
        return p.GetX() >= lo_x && p.GetX() <= hi_x && p.GetZ() >= lo_z && p.GetZ() <= hi_z;
    };
    float x = ax;
    float z = az;
    for (const OwnMove &move : own_way_moves_) {
        if (!inside(move.start) || !inside(move.goal) ||
            ((move.line.GetX() != 0.0F || move.line.GetZ() != 0.0F) && !inside(move.line))) {
            return false;
        }
        own_s += distance_xz(x, z, move.start.GetX(), move.start.GetZ()) / kHuntMps + move.cost;
        x = move.goal.GetX();
        z = move.goal.GetZ();
    }
    own_s += distance_xz(x, z, heard_x_, heard_z_) / kHuntMps;
    // One that failed under her a moment ago she leaves alone, as her plans do.
    const Vec3 first = own_way_moves_.front().start;
    return !column_blocked_near(first.GetX(), first.GetZ()) && own_s < pipe_s;
}

bool Matron::follow_pipe(const float dt, const RVec3 at) {
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const float next = pipe_next_toward(floor_y, lead_floor_);
    if (next == floor_y || !pipe_reaches(floor_y) || !pipe_reaches(next) || !pipe_serves(next)) {
        return false;
    }
    // Frail, she climbs it up after him, slow (Cory, 11-11: she climbs as a
    // pest too), but does not take hold of it to go down it. Up means over
    // her feet, not over her levels_ entry: topped out of a ladder onto a
    // landing over a deck, that entry is still the deck she left.
    if (stage_ == Stage::Pest && next < float(at.GetY()) - kStandCentre + 1.0F) {
        return false;
    }
    // A way that failed under her a moment ago she leaves alone a while.
    if (const int way = pipe_way(floor_y, next); way >= 0 && !pipe_way_open(way)) {
        return false;
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    // A raised corner (a top level with the rail standing on her deck
    // there): she gets on from over the rail's line on that top, as perched
    // on the rail.
    const bool raised = pipe_corner_raised(floor_y);
    const float stand_z = raised ? kPipeRailZ : kPipeDeckZ;
    const float off = distance_xz(ax, az, kPipeDeckX, stand_z);
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    // A way of her own map that has her to him sooner than the pipe is hers
    // first: the pipe is not her only way between decks (J-085).
    if (off > 0.25F && own_way_sooner(at, pipe_time_s(at, floor_y, stand_z))) {
        return false;
    }
    if (off > 0.25F) {
        // To the rail's corner along her lines, slowing into it.
        float wx = kPipeDeckX;
        float wz = kPipeDeckZ;
        route_toward(ax, az, kPipeDeckX, kPipeDeckZ, false, wx, wz);
        if (raised) {
            // From the deck she first goes up onto that top by her own way
            // over the steel (her scan's mantle, by her Climber's hands),
            // then along it to the rail's line.
            if (float(at.GetY()) - kStandCentre < floor_y + kStepUpM) {
                journey_wait_ticks_ = std::max(0, journey_wait_ticks_ - 1);   // its tick here, as a found climb counts it
                (void)start_journey_to(at, kPipeDeckX, floor_y + kPipeRailTopM, kPipeDeckZ);
                walk_toward(dt, ax, az, 0.0F, accel);
                return true;
            }
            wx = kPipeDeckX;
            wz = stand_z;
        }
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // Her hands on the rail: over it and onto the pipe (matron_pipe.cpp).
    walk_toward(dt, ax, az, 0.0F, accel);
    yaw_ = turn_to(yaw_, std::atan2(0.0F, kPipeRailZ - kPipeDeckZ), kTurnRate, dt);
    (void)get_on_pipe(floor_y, next);
    return true;
}

int Matron::pipe_way(const float from_floor, const float to_floor) noexcept {
    // Its decks, kPipeLowestFloor up a story at a time to kDeckTop
    // (pipe_reaches): one story up from deck d is way d, one story down from
    // deck d is way kPipeDecks - 2 + d.
    const auto deck = [](const float floor_y) {
        const int d = static_cast<int>(std::lround((floor_y - kPipeLowestFloor) / kStackPitch));
        return d >= 0 && d < kPipeDecks && std::abs(floor_y - (kPipeLowestFloor + kStackPitch * float(d))) <= kOnSteelM ? d : -1;
    };
    const int from = deck(from_floor);
    const int to = deck(to_floor);
    if (from < 0 || to < 0) {
        return -1;
    }
    if (to == from + 1) {
        return from;
    }
    if (to == from - 1) {
        return kPipeDecks - 2 + from;
    }
    return -1;
}

bool Matron::pipe_way_open(const int way) const noexcept {
    const PipeWayMemory &memory = pipe_ways_[static_cast<std::size_t>(way)];
    return !memory.ruled_out && clock_s_ >= memory.blocked_until_s;
}

void Matron::pipe_struck(const RVec3 at) {
    if (distance_xz(float(at.GetX()), float(at.GetZ()), kPipeLineX, kPipeLineZ) < 0.5F) {
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
    view_.pipe_stunned = pipe_phase_ == PipePhase::Stunned;
    view_.pipe_tuck = pipe_phase_ == PipePhase::Off ? 0.0F : pipe_tuck_;
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
    pipe_grip_ = PipeGrip::None;
    pipe_tuck_ = 0.0F;
    pipe_tuck_drawn_ = 0.0F;
    pipe_swat_body_ = BodyID();
    pipe_drive_dv_ = Vec3::sZero();
    pipe_force_n_ = 0.0F;
    pipe_slide_mps_ = 0.0F;
    pipe_overload_s_ = 0.0F;
    pipe_stall_s_ = 0.0F;
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
    m_.pipe_grip_ = PipeGrip::None;
    m_.pipe_tuck_ = 0.0F;
    m_.pipe_tuck_drawn_ = 0.0F;
    m_.pipe_swat_body_ = BodyID();
    m_.pipe_drive_dv_ = Vec3::sZero();
    m_.pipe_force_n_ = 0.0F;
    m_.pipe_slide_mps_ = 0.0F;
    m_.pipe_stun_s_ = 0.0F;
    m_.pipe_overload_s_ = 0.0F;
    m_.pipe_stall_s_ = 0.0F;
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
