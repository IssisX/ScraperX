#include "sim/matron_traversal.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>

namespace scraperx::sim::traversal {

using namespace JPH;

const char *move_name(const MoveKind kind) noexcept {
    switch (kind) {
    case MoveKind::Walk: return "walk";
    case MoveKind::RungUp: return "rung_up";
    case MoveKind::RungDown: return "rung_down";
    case MoveKind::Mantle: return "mantle";
    case MoveKind::StepDown: return "step_down";
    case MoveKind::PoleUp: return "pole_up";
    case MoveKind::PoleDown: return "pole_down";
    case MoveKind::JumpCatch: return "jump_catch";
    case MoveKind::HangDrop: return "hang_drop";
    }
    return "?";
}

const char *failure_name(const Failure failure) noexcept {
    switch (failure) {
    case Failure::None: return "none";
    case Failure::NoHold: return "no_hold";
    case Failure::LostHold: return "lost_hold";
    case Failure::MissingRung: return "missing_rung";
    case Failure::BlockedSweep: return "blocked_sweep";
    case Failure::Slipped: return "slipped";
    case Failure::Missed: return "missed";
    case Failure::RippedOff: return "ripped_off";
    case Failure::OutOfRange: return "out_of_range";
    case Failure::Stalled: return "stalled";
    case Failure::NotSupported: return "not_supported";
    }
    return "?";
}

namespace {
constexpr float kGravity = 9.81F;

// Probes see real steel only: never her, never the player, never a sensor.
class SteelFilter final : public BodyFilter {
public:
    explicit SteelFilter(const Ignore &ignore) : ignore_(ignore) {}
    bool ShouldCollide(const BodyID &id) const override { return id != ignore_.self && id != ignore_.player; }
    bool ShouldCollideLocked(const Body &body) const override {
        return !body.IsSensor() && body.GetID() != ignore_.self && body.GetID() != ignore_.player;
    }

private:
    Ignore ignore_;
};

// What her body's layer collides with (Ignore::layer), or everything.
class Layers {
public:
    Layers(const PhysicsSystem &physics, const Ignore &ignore)
        : bp_(physics.GetDefaultBroadPhaseLayerFilter(ObjectLayer(ignore.layer < 0 ? 0 : ignore.layer))),
          ol_(physics.GetDefaultLayerFilter(ObjectLayer(ignore.layer < 0 ? 0 : ignore.layer))), all_(ignore.layer < 0) {}
    const BroadPhaseLayerFilter &bp() const { return all_ ? any_bp_ : static_cast<const BroadPhaseLayerFilter &>(bp_); }
    const ObjectLayerFilter &ol() const { return all_ ? any_ol_ : static_cast<const ObjectLayerFilter &>(ol_); }

private:
    DefaultBroadPhaseLayerFilter bp_;
    DefaultObjectLayerFilter ol_;
    BroadPhaseLayerFilter any_bp_;
    ObjectLayerFilter any_ol_;
    bool all_;
};

// Static or kinematic steel only (what the scanner reads).
class FixedFilter final : public BodyFilter {
public:
    explicit FixedFilter(const Ignore &ignore) : ignore_(ignore) {}
    bool ShouldCollide(const BodyID &id) const override { return id != ignore_.self && id != ignore_.player; }
    bool ShouldCollideLocked(const Body &body) const override {
        return !body.IsSensor() && !body.IsDynamic() && body.GetID() != ignore_.self && body.GetID() != ignore_.player;
    }

private:
    Ignore ignore_;
};

float horizontal(const Vec3 v) { return std::sqrt(v.GetX() * v.GetX() + v.GetZ() * v.GetZ()); }
Vec3 flat(const Vec3 v) { return Vec3(v.GetX(), 0.0F, v.GetZ()); }

// A member's world bounds (one leaf of a body), or false if the sub-shape no
// longer names a member. `bar`: two of its dimensions small (a bar, a pole).
bool member_bounds(const Body &body, const SubShapeID &sub, AABox &out, bool &bar) {
    SubShapeID remainder;
    const TransformedShape ts = body.GetShape()->GetSubShapeTransformedShape(
        sub, body.GetCenterOfMassPosition(), body.GetRotation(), Vec3::sReplicate(1.0F), remainder);
    if (ts.mShape == nullptr) {
        return false;
    }
    out = ts.GetWorldSpaceBounds();
    const Vec3 size = out.GetSize();
    const float thin = std::min({size.GetX(), size.GetY(), size.GetZ()});
    const float mid = size.GetX() + size.GetY() + size.GetZ() - thin - std::max({size.GetX(), size.GetY(), size.GetZ()});
    bar = mid <= 0.16F;
    return true;
}
} // namespace

// ===========================================================================
// Climber

Climber::Climber(PhysicsSystem &physics, const BodyID body, const Ignore ignore, const Limits limits)
    : physics_(physics), body_(body), ignore_(ignore), limits_(limits) {}

Vec3 Budget::take(const Vec3 want) noexcept {
    const float length = want.Length();
    if (length <= 1.0e-6F || left <= 0.0F) {
        return Vec3::sZero();
    }
    Vec3 granted = want * (std::min(length, left) / length);
    // Power: only the part along her motion does work. Braking (negative)
    // costs none here; positive work beyond what is left scales the push.
    const float power = granted.Dot(velocity);
    if (power > 0.0F) {
        const float allowed = std::max(power_left, 0.0F);
        if (power > allowed) {
            const float speed2 = velocity.LengthSq();
            // Remove just enough of the along-velocity component.
            granted -= velocity * ((power - allowed) / speed2);
        }
        const float spent = std::max(granted.Dot(velocity), 0.0F);
        power_left -= spent;
        power_used += spent;
    }
    const float g = granted.Length();
    left -= g;
    used += g;
    return granted;
}

RVec3 Climber::center() const { return physics_.GetBodyInterface().GetCenterOfMassPosition(body_); }
Vec3 Climber::velocity() const { return physics_.GetBodyInterface().GetLinearVelocity(body_); }

// Her velocity against what she pushes on: the hold's point velocity at the
// anchor when holding, else the floor's under her feet. Power is spent only
// on motion relative to the support (riding a moving deck costs nothing).
Vec3 Climber::relative_velocity() const {
    const BodyInterface &bodies = physics_.GetBodyInterface();
    Vec3 support = Vec3::sZero();
    if (hold_.valid() && bodies.IsAdded(hold_.body)) {
        support = bodies.GetPointVelocity(hold_.body, hold_world_);
    } else {
        BodyID by;
        if (supported(nullptr, &by) && !by.IsInvalid()) {
            const RVec3 c = center();
            support = bodies.GetPointVelocity(by, RVec3(c.GetX(), c.GetY() - limits_.center_above_feet(), c.GetZ()));
        }
    }
    return velocity() - support;
}

bool Climber::supported(float *top, BodyID *by) const {
    const RVec3 c = center();
    const SteelFilter filter(ignore_);
    const float reach = limits_.center_above_feet() + 0.10F;
    for (const Vec3 off : {Vec3::sZero(), Vec3(0.18F, 0, 0), Vec3(-0.18F, 0, 0), Vec3(0, 0, 0.18F), Vec3(0, 0, -0.18F)}) {
        RayCastResult hit;
        const RRayCast ray(c + off, Vec3(0.0F, -reach, 0.0F));
        if (physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter)) {
            if (top != nullptr) {
                *top = float(ray.GetPointOnRay(hit.mFraction).GetY());
            }
            if (by != nullptr) {
                *by = hit.mBodyID;
            }
            return true;
        }
    }
    return false;
}

bool Climber::capsule_free(const RVec3 c) const {
    const CapsuleShape capsule(limits_.half_height, limits_.radius - 0.02F);
    CollideShapeSettings settings;
    settings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;
    AnyHitCollisionCollector<CollideShapeCollector> collector;
    const SteelFilter filter(ignore_);
    physics_.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1.0F), RMat44::sTranslation(c), settings,
                                                RVec3::sZero(), collector, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter);
    return !collector.HadHit();
}

bool Climber::revalidate(Hold &hold, RVec3 &world) const {
    if (!hold.valid()) {
        return false;
    }
    BodyLockRead lock(physics_.GetBodyLockInterface(), hold.body);
    if (!lock.Succeeded()) {
        return false;
    }
    const Body &body = lock.GetBody();
    if (!body.IsInBroadPhase() || body.IsSensor()) {
        return false;
    }
    AABox bounds;
    bool bar = false;
    if (!member_bounds(body, hold.sub, bounds, bar)) {
        return false;
    }
    // The same member: its bounds in the body's frame unchanged, the anchor on it.
    const AABox local = bounds.Transformed(body.GetInverseCenterOfMassTransform());
    if ((local.mMin - hold.member_local.mMin).Length() > 0.02F || (local.mMax - hold.member_local.mMax).Length() > 0.02F) {
        return false;
    }
    world = body.GetCenterOfMassTransform() * hold.local;
    AABox grown = bounds;
    grown.ExpandBy(Vec3::sReplicate(0.04F));
    if (!grown.Contains(world)) {
        return false;
    }
    const Vec3 d = world - center();
    return d.GetY() <= limits_.hand_above_centre + 0.15F && d.GetY() >= -limits_.reach_low - 0.6F &&
           horizontal(d) <= limits_.reach_out + 0.15F;
}

bool Climber::take_hold(const RVec3 near, const float radius, const bool bar_only, Hold &out, RVec3 *where) const {
    const SphereShape probe(radius);
    CollideShapeSettings settings;
    AllHitCollisionCollector<CollideShapeCollector> collector;
    const SteelFilter filter(ignore_);
    physics_.GetNarrowPhaseQuery().CollideShape(&probe, Vec3::sReplicate(1.0F), RMat44::sTranslation(near), settings,
                                                RVec3::sZero(), collector, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter);
    float best = std::numeric_limits<float>::max();
    bool found = false;
    for (const CollideShapeResult &hit : collector.mHits) {
        BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
        if (!lock.Succeeded() || lock.GetBody().IsSensor()) {
            continue;
        }
        const Body &body = lock.GetBody();
        AABox bounds;
        bool bar = false;
        if (!member_bounds(body, hit.mSubShapeID2, bounds, bar) || (bar_only && !bar)) {
            continue;
        }
        const Vec3 point = bounds.GetClosestPoint(near);
        const float d = (point - near).Length();
        if (d < best) {
            best = d;
            out.body = hit.mBodyID2;
            out.sub = hit.mSubShapeID2;
            out.local = body.GetInverseCenterOfMassTransform() * point;
            out.member_local = bounds.Transformed(body.GetInverseCenterOfMassTransform());
            if (where != nullptr) {
                *where = point;
            }
            found = true;
        }
    }
    return found;
}

bool Climber::find_rung(const float from_y, const float to_y, const bool upward, Hold &out, RVec3 &where) const {
    const float step = 0.04F;
    const int n = static_cast<int>(std::abs(to_y - from_y) / step) + 1;
    // Along the rung as well as at its middle: a post crossing the rungs on
    // the ladder's line (a lattice panel's) is not the rung her hands take.
    const Vec3 across(-spec_.out.GetZ(), 0.0F, spec_.out.GetX());
    const float side = std::max(0.0F, std::min(0.22F, spec_.bar_half - 0.05F));
    for (int i = 0; i < n; ++i) {
        const float y = upward ? from_y + step * float(i) : from_y - step * float(i);
        for (const float off : {0.0F, side, -side}) {
            const RVec3 at = RVec3(spec_.line.GetX(), y, spec_.line.GetZ()) + across * off;
            Hold hold;
            RVec3 point;
            if (take_hold(at, 0.05F, true, hold, &point)) {
                // A rung across the ladder (not a stile): short in y.
                BodyLockRead lock(physics_.GetBodyLockInterface(), hold.body);
                AABox bounds;
                bool bar = false;
                if (lock.Succeeded() && member_bounds(lock.GetBody(), hold.sub, bounds, bar) && bounds.GetSize().GetY() < 0.2F) {
                    out = hold;
                    where = point;
                    return true;
                }
            }
            if (side == 0.0F) {
                break;
            }
        }
    }
    return false;
}

bool Climber::find_lip(const Vec3 toward, const float near_y, Hold &out, RVec3 &where) const {
    const SteelFilter filter(ignore_);
    for (const float ahead : {0.35F, 0.55F, 0.8F, 1.1F}) {
        const RVec3 from = RVec3(spec_.line.GetX(), near_y + 1.2F, spec_.line.GetZ()) + toward * ahead;
        RayCastResult hit;
        const RRayCast ray(from, Vec3(0.0F, -2.0F, 0.0F));
        if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter)) {
            continue;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mFraction);
        if (std::abs(float(point.GetY()) - near_y) > 0.7F) {
            continue;
        }
        // Back toward her to the floor's edge: the lip her hands take.
        RVec3 edge = point;
        for (float back = ahead - 0.02F; back >= 0.0F; back -= 0.02F) {
            const RVec3 f2 = RVec3(spec_.line.GetX(), near_y + 1.2F, spec_.line.GetZ()) + toward * back;
            RayCastResult h2;
            const RRayCast r2(f2, Vec3(0.0F, -2.0F, 0.0F));
            if (!physics_.GetNarrowPhaseQuery().CastRay(r2, h2, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter) ||
                std::abs(float(r2.GetPointOnRay(h2.mFraction).GetY()) - float(point.GetY())) > 0.05F) {
                break;
            }
            edge = r2.GetPointOnRay(h2.mFraction);
        }
        Hold hold;
        RVec3 p;
        if (take_hold(edge - Vec3(0, 0.02F, 0), 0.04F, false, hold, &p)) {
            hold.local = [&] {
                BodyLockRead lock(physics_.GetBodyLockInterface(), hold.body);
                return lock.Succeeded() ? Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * edge) : hold.local;
            }();
            out = hold;
            where = edge;
            return true;
        }
    }
    return false;
}

void Climber::push_reaction(const Vec3 force, const RVec3 at) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    if (hold_.valid() && bodies.GetMotionType(hold_.body) == EMotionType::Dynamic) {
        bodies.AddForce(hold_.body, -force, at);
        view_.support_reaction_n += force.Length();
    }
}

void Climber::drive(const float dt, const Vec3 target_centre, const Vec3 target_velocity, const float stiffness,
                    Budget &budget, const bool carry_weight, const RVec3 at_hold) {
    (void)dt;
    BodyInterface &bodies = physics_.GetBodyInterface();
    Vec3 support_v = Vec3::sZero();
    if (hold_.valid()) {
        support_v = bodies.GetPointVelocity(hold_.body, at_hold);
    }
    const Vec3 rel = velocity() - support_v;
    const Vec3 error = target_centre - center();
    const float damping = 2.0F * std::sqrt(stiffness);
    const Vec3 accel = error * stiffness + (target_velocity - rel) * damping;
    Vec3 want = accel * limits_.mass_kg;
    if (carry_weight) {
        want += Vec3(0.0F, limits_.mass_kg * kGravity, 0.0F);
    }
    const Vec3 force = budget.take(want);
    bodies.AddForce(body_, force);
    push_reaction(force, at_hold);
}

void Climber::walk(const float dt, const Vec3 feet_goal, const float speed, Budget &budget, const float may_drop) {
    (void)dt;
    const RVec3 c = center();
    const Vec3 to = flat(feet_goal - c);
    const float d = to.Length();
    const Vec3 want_v = d > 1.0e-4F ? to / d * std::min(speed, 0.5F + 2.0F * d) : Vec3::sZero();
    const Vec3 v = flat(velocity());
    // Her feet keep to floor: where she is going (the goal's way and her
    // own drift), a floor within a step under her feet, or she plants them.
    Vec3 want_dir_v = want_v;
    {
        const float feet_y = float(c.GetY()) - limits_.center_above_feet();
        const SteelFilter filter(ignore_);
        const auto floor_ahead = [&](const Vec3 dir) {
            if (dir.LengthSq() < 1.0e-4F) return true;
            const RVec3 from = c + dir.Normalized() * (limits_.radius + 0.1F) +
                               Vec3(0.0F, limits_.step_max_m + 0.05F - limits_.center_above_feet(), 0.0F);
            const float below = std::max(limits_.step_max_m, may_drop);
            const float depth = limits_.step_max_m + below + 0.1F;
            RayCastResult hit;
            return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter) &&
                   float(from.GetY()) - hit.mFraction * depth > feet_y - below;
        };
        if (!floor_ahead(want_v) || (v.Length() > 0.3F && !floor_ahead(v))) {
            want_dir_v = floor_ahead(want_v) ? want_v * 0.3F : Vec3::sZero();
        }
    }
    Vec3 want = (want_dir_v - v) * (limits_.mass_kg * 8.0F);
    if (want.Length() > limits_.foot_force_n) {
        want = want.Normalized() * limits_.foot_force_n;
    }
    physics_.GetBodyInterface().AddForce(body_, budget.take(want));
    // A stair or plate edge within her stride in front of her: her leg lifts
    // her onto it, a capped push out of the same budget.
    if (d > 0.05F) {
        const Vec3 dir = to / d;
        const float feet = float(c.GetY()) - limits_.center_above_feet();
        const SteelFilter filter(ignore_);
        float rise = -1.0F;
        for (const float ahead : {0.36F, 0.5F}) {
            const RVec3 from = c + dir * ahead + Vec3(0.0F, limits_.step_max_m + 0.05F - limits_.center_above_feet(), 0.0F);
            const float depth = limits_.step_max_m + 0.1F;
            RayCastResult hit;
            if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter)) {
                rise = std::max(rise, float(from.GetY()) - hit.mFraction * depth - feet);
            }
        }
        if (rise > 0.04F && rise <= limits_.step_max_m) {
            const float lift = std::clamp(limits_.mass_kg * (kGravity + (1.6F - velocity().GetY()) * 12.0F), 0.0F,
                                          limits_.total_force_n * 0.85F);
            physics_.GetBodyInterface().AddForce(body_, budget.take(Vec3(0.0F, lift, 0.0F)));
        }
    }
}

float Climber::walk_between(const float dt, const Vec3 feet_goal, const float speed) {
    Budget budget{limits_.total_force_n, 0.0F, limits_.power_w, 0.0F, relative_velocity()};
    walk(dt, feet_goal, speed, budget);
    view_.force_n = budget.used;
    view_.power_w = budget.power_used;
    view_.peak_power_w = std::max(view_.peak_power_w, budget.power_used);
    view_.peak_force_n = std::max(view_.peak_force_n, budget.used);
    return budget.used;
}

void Climber::begin(const MoveSpec &spec) {
    spec_ = spec;
    // Her side of a ladder is the side she starts (up) or ends (down) on.
    if (spec_.kind == MoveKind::RungUp || spec_.kind == MoveKind::JumpCatch || spec_.kind == MoveKind::RungDown) {
        const Vec3 foot = spec_.kind == MoveKind::RungDown ? spec_.goal : spec_.start;
        const Vec3 side = flat(foot - Vec3(spec_.line.GetX(), foot.GetY(), spec_.line.GetZ()));
        if (side.Dot(spec_.out) < -0.05F) {
            spec_.out = -spec_.out;
        }
    }
    status_ = Status::Running;
    failure_ = Failure::None;
    phase_ = 0;
    phase_s_ = 0.0F;
    stall_s_ = 0.0F;
    best_progress_ = -std::numeric_limits<float>::max();
    hold_ = Hold{};
    lip_ = Hold{};
    sliding_ = false;
    mantle_after_ = false;
    approach_near_ = false;
    view_.active = true;
    view_.kind = spec.kind;
    view_.last_failure = Failure::None;
}

void Climber::cancel() {
    if (hold_.valid()) {
        ++view_.releases;
    }
    hold_ = Hold{};
    status_ = Status::Idle;
    view_.active = false;
    view_.holding = false;
}

void Climber::fail(const Failure failure) {
    if (hold_.valid()) {
        ++view_.releases;
    }
    hold_ = Hold{};
    failure_ = failure;
    status_ = Status::Failed;
    view_.last_failure = failure;
    view_.holding = false;
    view_.active = false;
}

void Climber::finish() {
    if (hold_.valid()) {
        ++view_.releases;
    }
    hold_ = Hold{};
    status_ = Status::Done;
    view_.holding = false;
    view_.active = false;
}

Climber::Status Climber::step(const float dt) {
    view_.force_n = 0.0F;
    view_.support_reaction_n = 0.0F;
    if (status_ != Status::Running) {
        return status_;
    }
    Budget budget{limits_.total_force_n, 0.0F, limits_.power_w, 0.0F, relative_velocity()};
    phase_s_ += dt;
    switch (spec_.kind) {
    case MoveKind::Walk: step_walk(dt, budget); break;
    case MoveKind::RungUp: step_rung_up(dt, budget); break;
    case MoveKind::RungDown: step_rung_down(dt, budget); break;
    case MoveKind::Mantle: step_mantle(dt, budget); break;
    case MoveKind::StepDown: step_drop(dt, budget, false); break;
    case MoveKind::PoleUp: step_pole(dt, budget, true); break;
    case MoveKind::PoleDown: step_pole(dt, budget, false); break;
    case MoveKind::JumpCatch: step_jump(dt, budget); break;
    case MoveKind::HangDrop: step_drop(dt, budget, true); break;
    }
    view_.force_n = budget.used;
    view_.power_w = budget.power_used;
    view_.peak_power_w = std::max(view_.peak_power_w, budget.power_used);
    view_.peak_force_n = std::max(view_.peak_force_n, budget.used);
    view_.phase = phase_;
    view_.sliding = sliding_;
    view_.holding = hold_.valid();
    view_.hand = hold_world_;
    view_.airborne = !hold_.valid() && !supported();
    return status_;
}

// --- walking ---------------------------------------------------------------
Climber::Status Climber::step_walk(const float dt, Budget &budget) {
    const RVec3 c = center();
    const float d = horizontal(spec_.goal - c);
    if (!supported()) {
        if (phase_s_ > 4.0F) {
            fail(Failure::NotSupported);
        }
        return status_;
    }
    if (d < 0.25F) {
        finish();
        return status_;
    }
    if (-d > best_progress_ + 0.05F) {
        best_progress_ = -d;
        stall_s_ = 0.0F;
    } else if ((stall_s_ += dt) > 3.0F) {
        fail(Failure::Stalled);
        return status_;
    }
    walk(dt, spec_.goal, 2.4F, budget);
    return status_;
}

// --- rungs, up --------------------------------------------------------------
// Phases: 0 to the foot, 2 hand over hand, 3 over the head (mantle).
Climber::Status Climber::step_rung_up(const float dt, Budget &budget) {
    const RVec3 c = center();
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    const Vec3 stand = Vec3(spec_.line.GetX(), 0.0F, spec_.line.GetZ()) + spec_.out * 0.45F;
    if (phase_ == 0) {
        if (horizontal(Vec3(stand.GetX(), 0, stand.GetZ()) - flat(c)) > 0.2F) {
            if (phase_s_ > 8.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            walk(dt, Vec3(stand.GetX(), feet, stand.GetZ()), 1.4F, budget);
            return status_;
        }
        RVec3 where;
        if (!find_rung(feet + 0.15F, feet + limits_.hand_reach_standing(), true, hold_, where)) {
            fail(Failure::NoHold);
            return status_;
        }
        hold_world_ = where;
        ++view_.grips;
        phase_ = 2;
        target_y_ = float(c.GetY());
        stall_s_ = 0.0F;
        best_progress_ = float(c.GetY());
    }
    if (phase_ >= 3) {
        return step_mantle(dt, budget);
    }
    if (!revalidate(hold_, hold_world_)) {
        fail(Failure::LostHold);
        return status_;
    }
    // The next rung up as soon as one is within reach over the present one.
    Hold next;
    RVec3 next_world;
    const float hold_y = float(hold_world_.GetY());
    const float top_reach = float(c.GetY()) + limits_.hand_above_centre;
    if (top_reach > hold_y + 0.2F && hold_y < spec_.top_y - 0.05F &&
        find_rung(hold_y + 0.12F, std::min(top_reach, hold_y + 0.65F), true, next, next_world)) {
        hold_ = next;
        hold_world_ = next_world;
        ++view_.grips;
    }
    const float hold_now = float(hold_world_.GetY());
    const float ceiling = hold_now - 0.35F;   // the hold at her chest, no higher
    target_y_ = std::min(target_y_ + rate_mps() * dt, ceiling);
    const bool at_top = hold_now >= spec_.top_y - 0.05F;
    if (at_top && float(c.GetY()) >= ceiling - 0.25F) {
        if (!find_lip(-spec_.out, spec_.goal_top, lip_, lip_world_)) {
            fail(Failure::NoHold);
            return status_;
        }
        hold_ = lip_;
        hold_world_ = lip_world_;
        ++view_.grips;
        return begin_mantle_from_hang();
    }
    if (float(c.GetY()) > best_progress_ + 0.05F) {
        best_progress_ = float(c.GetY());
        stall_s_ = 0.0F;
    } else if ((stall_s_ += dt) > 2.5F) {
        fail(at_top ? Failure::Stalled : Failure::MissingRung);
        return status_;
    }
    const Vec3 target(stand.GetX() - spec_.out.GetX() * 0.03F, target_y_, stand.GetZ() - spec_.out.GetZ() * 0.03F);
    drive(dt, target, Vec3(0.0F, rate_mps(), 0.0F), 30.0F, budget, true, hold_world_);
    return status_;
}

// A mantle from a hang: her capsule must fit over the lip and where she will
// stand, or she does not go.
Climber::Status Climber::begin_mantle_from_hang() {
    const Vec3 in = flat(spec_.goal - lip_world_).NormalizedOr(-spec_.out);
    const float top = float(lip_world_.GetY());
    const RVec3 over = lip_world_ + in * (limits_.radius + 0.08F) + Vec3(0.0F, limits_.center_above_feet() + 0.06F, 0.0F);
    const RVec3 stand = RVec3(spec_.goal.GetX(), top + limits_.center_above_feet() + 0.04F, spec_.goal.GetZ());
    if (!capsule_free(over) || !capsule_free(stand)) {
        fail(Failure::BlockedSweep);
        return status_;
    }
    phase_ = 3;
    phase_s_ = 0.0F;
    stall_s_ = 0.0F;
    best_progress_ = -std::numeric_limits<float>::max();
    return status_;
}

// --- mantle ------------------------------------------------------------------
// Up beside the lip until her feet clear it, in over it, then her own weight
// onto her feet.
Climber::Status Climber::step_mantle(const float dt, Budget &budget) {
    const RVec3 c = center();
    if (spec_.kind == MoveKind::Mantle && phase_ == 0) {
        const Vec3 to_lip = flat(spec_.goal - spec_.start).NormalizedOr(Vec3::sAxisZ());
        if (horizontal(spec_.start - c) > 0.25F) {
            if (phase_s_ > 8.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            walk(dt, spec_.start, 1.4F, budget);
            return status_;
        }
        spec_.line = spec_.start;
        if (!find_lip(to_lip, spec_.goal_top, lip_, lip_world_)) {
            fail(Failure::NoHold);
            return status_;
        }
        if (float(lip_world_.GetY()) - (float(c.GetY()) - limits_.center_above_feet()) > limits_.mantle_max_m) {
            fail(Failure::OutOfRange);
            return status_;
        }
        hold_ = lip_;
        hold_world_ = lip_world_;
        ++view_.grips;
        begin_mantle_from_hang();
        if (status_ != Status::Running) {
            return status_;
        }
    }
    if (phase_ == 4) {
        // Hands off, over the floor: her own weight onto her feet.
        float top = 0.0F;
        if (supported(&top) && std::abs(top - spec_.goal_top) < 0.3F && std::abs(velocity().GetY()) < 0.3F) {
            finish();
        } else if (phase_s_ > 2.0F) {
            fail(Failure::NotSupported);
        }
        return status_;
    }
    if (!revalidate(hold_, hold_world_)) {
        // Past the lip's reach: if her feet are over the floor she was going
        // for, she lets go onto it; else the hold is lost.
        const SteelFilter filter(ignore_);
        RayCastResult hit;
        const RRayCast ray(c, Vec3(0.0F, -2.5F, 0.0F));
        if (float(c.GetY()) - limits_.center_above_feet() > spec_.goal_top - 0.1F &&
            physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter) &&
            std::abs(float(ray.GetPointOnRay(hit.mFraction).GetY()) - spec_.goal_top) < 0.3F) {
            ++view_.releases;
            hold_ = Hold{};
            phase_ = 4;
            phase_s_ = 0.0F;
            return status_;
        }
        fail(Failure::LostHold);
        return status_;
    }
    const float top = float(lip_world_.GetY());
    const Vec3 in = flat(spec_.goal - lip_world_).NormalizedOr(-spec_.out);
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    const float over_y = top + limits_.center_above_feet() + 0.08F;
    float progress = 0.0F;
    if (feet < top + 0.04F) {
        // Straight up where she hangs if that is clear of the lip's face,
        // else just off it.
        const float off = horizontal(c - lip_world_);
        const Vec3 beside = off >= limits_.radius + 0.03F && off <= limits_.reach_out ? Vec3(c) : lip_world_ - in * (limits_.radius + 0.06F);
        drive(dt, Vec3(beside.GetX(), over_y, beside.GetZ()), Vec3(0.0F, limits_.mantle_mps, 0.0F), 25.0F, budget, true,
              hold_world_);
        progress = feet;
    } else {
        // Over the lip no farther than her hand reaches from it; a lip over
        // the floor (a rail, a toe board) she lets go of past it, onto the floor.
        Vec3 dest = spec_.goal;
        const float span = limits_.reach_out - 0.1F;
        if (horizontal(dest - lip_world_) > span) {
            dest = Vec3(lip_world_) + flat(dest - Vec3(lip_world_)).Normalized() * span;
        }
        float support = 0.0F;
        if (supported(&support) && std::abs(support - top) < 0.25F && horizontal(dest - c) < 0.35F) {
            finish();
            return status_;
        }
        if (horizontal(dest - c) < 0.15F && top > spec_.goal_top + 0.3F) {
            ++view_.releases;
            hold_ = Hold{};
            phase_ = 4;
            phase_s_ = 0.0F;
            return status_;
        }
        const float left = horizontal(dest - c);
        drive(dt, Vec3(dest.GetX(), over_y, dest.GetZ()), flat(dest - c).NormalizedOr(in) * std::min(limits_.mantle_mps, 1.5F * left), 20.0F, budget, true,
              hold_world_);
        progress = 10.0F - horizontal(dest - c);
    }
    if (progress > best_progress_ + 0.03F) {
        best_progress_ = progress;
        stall_s_ = 0.0F;
    } else if ((stall_s_ += dt) > 2.5F) {
        fail(Failure::Stalled);
    }
    return status_;
}

// --- rungs, down -------------------------------------------------------------
// Phases: 0 to the head; 1 over the lip onto the top rung; 2 hand under hand;
// done when her feet are on the floor below.
Climber::Status Climber::step_rung_down(const float dt, Budget &budget) {
    const RVec3 c = center();
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    if (phase_ == 0) {
        // To the head: the start, then on over the floor until the line is
        // in arm's reach (a head bridge can carry the floor on past the start).
        Vec3 approach = spec_.start;
        const Vec3 line_flat(spec_.line.GetX(), spec_.start.GetY(), spec_.line.GetZ());
        const Vec3 back = flat(spec_.start - line_flat);
        if (horizontal(back) > 0.55F && horizontal(spec_.start - c) <= 0.25F) {
            approach_near_ = true;
        }
        if (approach_near_) {
            approach = line_flat + back.Normalized() * 0.5F;
        }
        if (horizontal(approach - c) > 0.2F) {
            if (phase_s_ > 14.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            walk(dt, approach, approach_near_ ? 0.7F : 1.4F, budget);
            return status_;
        }
        RVec3 where;
        // The top rung within her reach as she stands (no lower than she
        // can keep hold of); else the lip she stands on, from which she goes
        // over and down hand under hand to the rungs.
        const float lowest = float(c.GetY()) - limits_.reach_low - 0.55F;
        if (!find_rung(feet + 0.3F, std::max(feet - 0.8F, lowest), false, hold_, where)) {
            const Vec3 toward = flat(Vec3(spec_.line.GetX(), 0.0F, spec_.line.GetZ()) - Vec3(c)).NormalizedOr(-spec_.out);
            bool found = false;
            for (float ahead = 0.0F; ahead < 0.9F && !found; ahead += 0.1F) {
                found = take_hold(RVec3(c.GetX(), feet - 0.03F, c.GetZ()) + toward * ahead, 0.05F, false, hold_, &where);
            }
            if (found) {
                // Her hands at the edge itself, on the side she goes over.
                BodyLockRead lock(physics_.GetBodyLockInterface(), hold_.body);
                AABox bounds;
                bool bar = false;
                if (lock.Succeeded() && member_bounds(lock.GetBody(), hold_.sub, bounds, bar)) {
                    where = bounds.GetClosestPoint(RVec3(c.GetX(), feet, c.GetZ()) + toward * 3.0F);
                    hold_.local = lock.GetBody().GetInverseCenterOfMassTransform() * where;
                }
            }
            if (!found) {
                fail(Failure::NoHold);
                return status_;
            }
        }
        const Vec3 hang = Vec3(spec_.line.GetX(), float(where.GetY()) - limits_.hang_below_hold + 0.4F, spec_.line.GetZ()) +
                          spec_.out * 0.42F;
        if (!capsule_free(hang)) {
            hold_ = Hold{};
            fail(Failure::BlockedSweep);
            return status_;
        }
        hold_world_ = where;
        ++view_.grips;
        phase_ = 1;
        phase_s_ = 0.0F;
        target_y_ = float(c.GetY());
        best_progress_ = -float(c.GetY());
    }
    if (!revalidate(hold_, hold_world_)) {
        fail(Failure::LostHold);
        return status_;
    }
    const Vec3 line_pos = Vec3(spec_.line.GetX(), 0.0F, spec_.line.GetZ()) + spec_.out * 0.42F;
    float support = 0.0F;
    if (phase_ >= 2 && supported(&support) && std::abs(support - spec_.goal_top) < 0.3F && velocity().GetY() > -0.6F) {
        finish();
        return status_;
    }
    if (phase_ == 1) {
        const float out_d = horizontal(line_pos - c);
        if (out_d <= 0.15F) {
            phase_ = 2;
            target_y_ = float(c.GetY());
        } else {
            if (phase_s_ > 5.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            drive(dt, Vec3(line_pos.GetX(), std::max(float(c.GetY()), float(hold_world_.GetY()) + 0.1F), line_pos.GetZ()),
                  Vec3::sZero(), 18.0F, budget, true, hold_world_);
            return status_;
        }
    }
    const float hold_y = float(hold_world_.GetY());
    if (hold_y - float(c.GetY()) > limits_.hand_above_centre - 0.2F) {
        Hold next;
        RVec3 next_world;
        if (find_rung(hold_y - 0.12F, hold_y - 0.65F, false, next, next_world)) {
            hold_ = next;
            hold_world_ = next_world;
            ++view_.grips;
        }
    }
    const float floor = float(hold_world_.GetY()) - limits_.hang_below_hold;
    target_y_ = std::max(target_y_ - rate_mps() * dt, floor);
    if (-float(c.GetY()) > best_progress_ + 0.05F) {
        best_progress_ = -float(c.GetY());
        stall_s_ = 0.0F;
    } else if ((stall_s_ += dt) > 2.5F) {
        // No rung under her hands and no floor under her feet: she lets go.
        fail(Failure::MissingRung);
        return status_;
    }
    drive(dt, Vec3(line_pos.GetX(), target_y_, line_pos.GetZ()), Vec3(0.0F, -rate_mps(), 0.0F), 30.0F, budget,
          true, hold_world_);
    return status_;
}

// --- pole grip -----------------------------------------------------------------
// Her squeeze is capped; the friction it buys holds her only so far. Past
// static friction she slides (kinetic friction, less), and takes hold again
// only when the slip has slowed and the load is back under what static
// friction holds.
Climber::Status Climber::step_pole(const float dt, Budget &budget, const bool up) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 c = center();
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    const Vec3 axis(spec_.line.GetX(), 0.0F, spec_.line.GetZ());
    const Vec3 hug = axis + spec_.out * (limits_.radius + 0.1F);
    if (phase_ == 0) {
        // Up: from where she stands at its foot, if the pole is in arm's
        // reach from there; else hugging it.
        const bool start_reaches = horizontal(flat(spec_.start - axis)) <= limits_.reach_out;
        const Vec3 approach = up ? (start_reaches ? spec_.start : hug + Vec3(0, feet, 0)) : spec_.start;
        // Already standing where the pole is in arm's reach: grip from here.
        const bool here_reaches = up && horizontal(flat(Vec3(c) - axis)) <= limits_.reach_out && supported();
        if (!here_reaches && horizontal(approach - c) > 0.22F) {
            if (phase_s_ > 10.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            walk(dt, approach, 1.2F, budget);
            return status_;
        }
        const RVec3 at(axis.GetX(), up ? float(c.GetY()) + 0.4F : feet - 0.3F, axis.GetZ());
        RVec3 where;
        if (!take_hold(at, 0.12F, true, hold_, &where)) {
            fail(Failure::NoHold);
            return status_;
        }
        if (!up && !capsule_free(Vec3(hug.GetX(), feet + limits_.center_above_feet() - 1.1F, hug.GetZ()))) {
            hold_ = Hold{};
            fail(Failure::BlockedSweep);
            return status_;
        }
        hold_world_ = where;
        ++view_.grips;
        phase_ = up ? 2 : 1;
        phase_s_ = 0.0F;
        target_y_ = float(c.GetY());
        best_progress_ = up ? float(c.GetY()) : -float(c.GetY());
        stall_s_ = 0.0F;
    }
    if (phase_ >= 3) {
        return step_mantle(dt, budget);
    }
    // Her hands slide along the same member with her.
    {
        BodyLockRead lock(physics_.GetBodyLockInterface(), hold_.body);
        if (!lock.Succeeded() || lock.GetBody().IsSensor()) {
            hold_ = Hold{};
            fail(Failure::LostHold);
            return status_;
        }
        AABox bounds;
        bool bar = false;
        if (!member_bounds(lock.GetBody(), hold_.sub, bounds, bar)) {
            hold_ = Hold{};
            fail(Failure::LostHold);
            return status_;
        }
        const float hy = std::clamp(float(c.GetY()) + 0.4F, bounds.mMin.GetY(), bounds.mMax.GetY());
        if (hy < float(c.GetY()) - (up ? 0.5F : 1.3F)) {
            fail(Failure::LostHold);   // its head is under her: nothing left to squeeze
            return status_;
        }
        const RVec3 point = bounds.GetClosestPoint(RVec3(axis.GetX(), hy, axis.GetZ()));
        hold_.local = lock.GetBody().GetInverseCenterOfMassTransform() * point;
    }
    if (!revalidate(hold_, hold_world_)) {
        fail(Failure::LostHold);
        return status_;
    }
    if (phase_ == 1) {
        // Off the lip onto it: out over the pole first, carrying herself.
        if (horizontal(hug - c) <= 0.12F) {
            phase_ = 2;
            target_y_ = float(c.GetY());
        } else {
            if (phase_s_ > 6.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            drive(dt, Vec3(hug.GetX(), target_y_, hug.GetZ()), Vec3::sZero(), 18.0F, budget, true, hold_world_);
            return status_;
        }
    }
    const float rate = sliding_ ? 0.0F : up ? limits_.pole_up_mps : -limits_.pole_down_mps;
    target_y_ = sliding_ ? float(c.GetY()) : target_y_ + rate * dt;
    if (!sliding_) {
        // The target never runs ahead of her by more than her grip can close.
        target_y_ = std::clamp(target_y_, float(c.GetY()) - 0.3F, float(c.GetY()) + 0.3F);
    }
    if (up) {
        target_y_ = std::min(target_y_, spec_.top_y - 0.4F);
    }
    const Vec3 support_v = bodies.GetPointVelocity(hold_.body, hold_world_);
    const Vec3 rel = velocity() - support_v;
    const float k = 20.0F;
    const float want_y = limits_.mass_kg * (k * (target_y_ - float(c.GetY())) + 2.0F * std::sqrt(k) * (rate - rel.GetY()) + kGravity);
    // The squeeze, from the shared budget first: friction is all it buys.
    const float squeeze = std::min(limits_.pole_squeeze_n, budget.left * 0.8F);
    budget.left -= squeeze;
    budget.used += squeeze;
    const float static_cap = limits_.pole_mu_static * squeeze;
    const float kinetic_cap = limits_.pole_mu_kinetic * squeeze;
    float fy = 0.0F;
    // She never asks more of the steel than friction gives: a slip is a load
    // she did not choose (her grip at its limit and still going down).
    const bool overloaded = rel.GetY() < -0.35F && want_y >= 0.9F * static_cap;
    if (!sliding_ && !overloaded) {
        fy = std::clamp(want_y, -0.92F * static_cap, 0.92F * static_cap);
    } else {
        if (!sliding_) {
            ++view_.slips;
        }
        sliding_ = true;
        fy = rel.GetY() <= 0.0F ? kinetic_cap : -kinetic_cap;
        // Taken hold again once the slip is slow and her load is holdable.
        if (std::abs(rel.GetY()) < 0.25F && std::abs(want_y) <= static_cap) {
            sliding_ = false;
            target_y_ = float(c.GetY());
        }
    }
    const Vec3 side = flat(hug - c) * (limits_.mass_kg * 20.0F) - flat(rel) * (limits_.mass_kg * 6.0F);
    const Vec3 force = Vec3(0.0F, fy, 0.0F) + budget.take(side);
    bodies.AddForce(body_, force);
    push_reaction(force, hold_world_);
    float support = 0.0F;
    if (!up && supported(&support) && std::abs(support - spec_.goal_top) < 0.35F) {
        finish();
        return status_;
    }
    if (up && !sliding_ && float(c.GetY()) >= spec_.top_y - 0.6F) {
        // The head of the pole: hands onto the lip of the floor beside it.
        const Vec3 toward = flat(spec_.goal - axis).NormalizedOr(-spec_.out);
        if (find_lip(toward, spec_.goal_top, lip_, lip_world_)) {
            hold_ = lip_;
            hold_world_ = lip_world_;
            ++view_.grips;
            return begin_mantle_from_hang();
        }
    }
    const float progress = up ? float(c.GetY()) : -float(c.GetY());
    if (progress > best_progress_ + 0.05F) {
        best_progress_ = progress;
        stall_s_ = 0.0F;
    } else if ((stall_s_ += dt) > (sliding_ ? 6.0F : 4.0F)) {
        fail(sliding_ ? Failure::Slipped : Failure::Stalled);
    }
    return status_;
}

// --- jump to a hold ------------------------------------------------------------
// Phases: 0 to the take-off, 1 set, 2 in the air; caught, she goes on up it
// as a ladder (RungUp, from her hang).
Climber::Status Climber::step_jump(const float dt, Budget &budget) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 c = center();
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    const Vec3 bar(spec_.line.GetX(), spec_.bottom_y, spec_.line.GetZ());
    const Vec3 takeoff = Vec3(bar.GetX(), 0.0F, bar.GetZ()) + spec_.out * 0.35F;
    if (phase_ == 0) {
        if (horizontal(takeoff - c) > 0.15F || flat(velocity()).Length() > 0.3F) {
            if (phase_s_ > 10.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            walk(dt, Vec3(takeoff.GetX(), feet, takeoff.GetZ()), 1.2F, budget);
            return status_;
        }
        phase_ = 1;
        phase_s_ = 0.0F;
    }
    if (phase_ == 1) {
        if (phase_s_ < 0.25F) {
            return status_;   // set
        }
        BodyID floor;
        if (!supported(nullptr, &floor)) {
            fail(Failure::NotSupported);
            return status_;
        }
        // The launch that brings her hands to the bar at the top of the jump.
        const float rise = spec_.bottom_y + 0.08F - (float(c.GetY()) + limits_.hand_above_centre);
        const float vy = rise > 0.0F ? std::sqrt(2.0F * kGravity * rise) : 0.0F;
        const float t = std::max(vy / kGravity, 0.2F);
        const Vec3 across = flat(bar + spec_.out * 0.42F - c);
        const Vec3 dv = Vec3(across.GetX() / t, vy, across.GetZ() / t) - velocity();
        if (dv.Length() > limits_.launch_dv_mps) {
            fail(Failure::OutOfRange);
            return status_;
        }
        const Vec3 impulse = dv * limits_.mass_kg;
        bodies.AddImpulse(body_, impulse);
        if (bodies.GetMotionType(floor) == EMotionType::Dynamic) {
            bodies.AddImpulse(floor, -impulse, c - Vec3(0, limits_.center_above_feet(), 0));
        }
        ++view_.launches;
        phase_ = 2;
        phase_s_ = 0.0F;
        return status_;
    }
    const Vec3 v = velocity();
    const RVec3 hands = c + Vec3(0.0F, limits_.hand_above_centre, 0.0F);
    Hold bar_hold;
    RVec3 bar_at;
    const bool near_bar = std::abs(float(hands.GetY()) - spec_.bottom_y) < 0.25F &&
                          horizontal(bar - hands) < limits_.catch_radius_m + 0.3F;
    if (near_bar && v.GetY() < 0.8F &&
        take_hold(RVec3(bar.GetX(), float(hands.GetY()), bar.GetZ()), limits_.catch_radius_m, true, bar_hold, &bar_at)) {
        const Vec3 support_v = bodies.GetPointVelocity(bar_hold.body, bar_at);
        if ((v - support_v).Length() > limits_.catch_arrest_mps) {
            ++view_.misses;
            fail(Failure::RippedOff);
            return status_;
        }
        hold_ = bar_hold;
        hold_world_ = bar_at;
        ++view_.grips;
        ++view_.catches;
        spec_.kind = MoveKind::RungUp;
        view_.kind = MoveKind::RungUp;
        phase_ = 2;
        target_y_ = float(c.GetY());
        best_progress_ = float(c.GetY());
        stall_s_ = 0.0F;
        return status_;
    }
    if ((v.GetY() < 0.0F && float(hands.GetY()) < spec_.bottom_y - 0.3F) || phase_s_ > 2.0F) {
        ++view_.misses;
        fail(Failure::Missed);
    }
    (void)budget;
    (void)dt;
    return status_;
}

// --- off an edge: a step down, or a hang and a drop --------------------------------
// Phases: 0 to the edge, 1 hands on the lip and down to a hang, 2 let go,
// 3 falling; done when her feet are on real support at the goal.
Climber::Status Climber::step_drop(const float dt, Budget &budget, const bool hang) {
    const RVec3 c = center();
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    const Vec3 out = flat(spec_.goal - spec_.start).NormalizedOr(spec_.out);
    if (phase_ == 0) {
        if (horizontal(spec_.start - c) > 0.25F) {
            if (phase_s_ > 10.0F) {
                fail(Failure::Stalled);
                return status_;
            }
            walk(dt, spec_.start, 1.4F, budget);
            return status_;
        }
        if (!hang) {
            phase_ = 2;
        } else {
            // The edge of what she stands on, under her hands as she goes over.
            Hold lip;
            RVec3 at;
            const RVec3 probe = RVec3(spec_.start.GetX(), feet - 0.03F, spec_.start.GetZ());
            bool found = false;
            for (float ahead = 0.0F; ahead < 0.9F && !found; ahead += 0.1F) {
                found = take_hold(probe + out * ahead, 0.05F, false, lip, &at);
            }
            if (found) {
                BodyLockRead lock(physics_.GetBodyLockInterface(), lip.body);
                AABox bounds;
                bool bar = false;
                if (lock.Succeeded() && member_bounds(lock.GetBody(), lip.sub, bounds, bar)) {
                    at = bounds.GetClosestPoint(RVec3(probe + out * 2.0F) + Vec3(0, 0.03F, 0));
                    lip.local = lock.GetBody().GetInverseCenterOfMassTransform() * at;
                }
            }
            if (!found) {
                fail(Failure::NoHold);
                return status_;
            }
            // At full stretch, or (a short drop) as low as the floor below lets her.
            RVec3 hang_at = at + out * (limits_.radius + 0.08F) - Vec3(0.0F, limits_.hang_below_hold, 0.0F);
            const float lowest = spec_.goal.GetY() + limits_.center_above_feet() + 0.05F;
            if (float(hang_at.GetY()) < lowest) {
                hang_at = RVec3(hang_at.GetX(), lowest, hang_at.GetZ());
            }
            if (!capsule_free(hang_at)) {
                fail(Failure::BlockedSweep);
                return status_;
            }
            hold_ = lip;
            hold_world_ = at;
            ++view_.grips;
            phase_ = 1;
            phase_s_ = 0.0F;
        }
    }
    if (phase_ == 1) {
        if (!revalidate(hold_, hold_world_)) {
            fail(Failure::LostHold);
            return status_;
        }
        RVec3 hang_at = hold_world_ + out * (limits_.radius + 0.08F) - Vec3(0.0F, limits_.hang_below_hold, 0.0F);
        const float lowest = spec_.goal.GetY() + limits_.center_above_feet() + 0.05F;
        if (float(hang_at.GetY()) < lowest) {
            hang_at = RVec3(hang_at.GetX(), lowest, hang_at.GetZ());
        }
        const float off = horizontal(hang_at - c);
        const Vec3 target = off > 0.12F ? Vec3(hang_at.GetX(), std::max(float(c.GetY()), float(hold_world_.GetY()) + 0.3F), hang_at.GetZ())
                                        : Vec3(hang_at);
        drive(dt, target, Vec3::sZero(), 16.0F, budget, true, hold_world_);
        if ((Vec3(hang_at) - c).Length() < 0.2F && velocity().Length() < 0.5F) {
            phase_ = 2;
        } else if (phase_s_ > 6.0F) {
            phase_ = 2;   // she lets go where she is
        }
        return status_;
    }
    if (phase_ == 2) {
        if (hold_.valid()) {
            ++view_.releases;
            hold_ = Hold{};
        }
        phase_ = 3;
        phase_s_ = 0.0F;
    }
    float support = 0.0F;
    const bool on = supported(&support);
    if (on && std::abs(velocity().GetY()) < 0.3F && phase_s_ > 0.1F) {
        if (std::abs(support - spec_.goal_top) < 0.35F) {
            finish();
            return status_;
        }
        if (support < spec_.goal_top - 0.35F) {
            fail(Failure::NotSupported);   // she went past it
            return status_;
        }
    }
    if (on && !hang) {
        // On over the edge: the floor she steps down to counts as floor.
        walk(dt, spec_.goal, 1.6F, budget, std::max(0.0F, support - spec_.goal_top) + 0.15F);
    }
    if (phase_s_ > 8.0F) {
        fail(Failure::NotSupported);
    }
    return status_;
}

// ===========================================================================
// The scanner

namespace {
struct Leaf {
    BodyID body;
    SubShapeID sub;
    AABox box;
    bool axis_aligned = true;
    std::uint64_t entity = 0;
};

class LeafCollector final : public TransformedShapeCollector {
public:
    std::vector<TransformedShape> shapes;
    void AddHit(const TransformedShape &shape) override { shapes.push_back(shape); }
};

class BodyListCollector final : public CollideShapeBodyCollector {
public:
    std::vector<BodyID> ids;
    void AddHit(const BodyID &id) override { ids.push_back(id); }
};

long long cell_key(const int x, const int y, const int z) {
    return (static_cast<long long>(x + 4096) << 42) ^ (static_cast<long long>(y + 4096) << 21) ^
           static_cast<long long>(z + 4096);
}

struct UnionFind {
    std::vector<int> parent;
    explicit UnionFind(const std::size_t n) : parent(n) {
        for (std::size_t i = 0; i < n; ++i) parent[i] = static_cast<int>(i);
    }
    int find(int i) {
        while (parent[static_cast<std::size_t>(i)] != i) {
            parent[static_cast<std::size_t>(i)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(i)])];
            i = parent[static_cast<std::size_t>(i)];
        }
        return i;
    }
    void join(const int a, const int b) { parent[static_cast<std::size_t>(find(a))] = find(b); }
};

float gap_xz(const Patch &a, const Patch &b) {
    const float gx = std::max({0.0F, a.min_x - b.max_x, b.min_x - a.max_x});
    const float gz = std::max({0.0F, a.min_z - b.max_z, b.min_z - a.max_z});
    return std::sqrt(gx * gx + gz * gz);
}

int cell_of(const float v, const float cell) { return static_cast<int>(std::floor(v / cell)); }
} // namespace

bool AffordanceMap::capsule_free(const PhysicsSystem &physics, const Ignore &ignore, const Limits &limits, const Vec3 feet) {
    ++stats_.capsule_queries;
    const CapsuleShape capsule(limits.half_height, limits.radius - 0.02F);
    CollideShapeSettings settings;
    settings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;
    AnyHitCollisionCollector<CollideShapeCollector> collector;
    const FixedFilter filter(ignore);
    const RVec3 c = feet + Vec3(0.0F, limits.center_above_feet() + 0.04F, 0.0F);
    physics.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1.0F), RMat44::sTranslation(c), settings,
                                               RVec3::sZero(), collector, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), filter);
    return !collector.HadHit();
}

void AffordanceMap::scan(const PhysicsSystem &physics, const AABox &bounds, const Ignore &ignore, const Limits &limits) {
    const auto started = std::chrono::steady_clock::now();
    patches_.clear();
    regions_.clear();
    edges_.clear();
    ladders_.clear();
    poles_.clear();
    patch_links_.clear();
    stats_ = ScanStats{};

    // 1. Every leaf of every fixed, solid body in the bounds.
    BodyListCollector bodies;
    physics.GetBroadPhaseQuery().CollideAABox(bounds, bodies);
    std::sort(bodies.ids.begin(), bodies.ids.end());
    std::vector<Leaf> leaves;
    for (const BodyID id : bodies.ids) {
        if (id == ignore.self || id == ignore.player) continue;
        BodyLockRead lock(physics.GetBodyLockInterface(), id);
        if (!lock.Succeeded()) continue;
        const Body &body = lock.GetBody();
        if (body.IsSensor() || body.IsDynamic()) continue;
        ++stats_.bodies;
        LeafCollector collector;
        body.GetShape()->CollectTransformedShapes(bounds, body.GetCenterOfMassPosition(), body.GetRotation(),
                                                  Vec3::sReplicate(1.0F), SubShapeIDCreator(), collector, {});
        for (const TransformedShape &ts : collector.shapes) {
            Leaf leaf;
            leaf.body = id;
            leaf.sub = ts.mSubShapeIDCreator.GetID();
            leaf.box = ts.GetWorldSpaceBounds();
            leaf.axis_aligned = ts.mShapeRotation.RotateAxisY().GetY() > 0.995F &&
                                ts.mShape->GetSubType() == EShapeSubType::Box;
            leaf.entity = body.GetUserData();
            leaves.push_back(leaf);
        }
    }
    stats_.leaves = static_cast<std::uint32_t>(leaves.size());

    // 2. Classify: bars (rungs), poles, walkable tops.
    struct Bar {
        Vec3 c;
        bool along_x;
        float half;
        std::uint64_t entity;
    };
    std::vector<Bar> bars;
    for (const Leaf &leaf : leaves) {
        const Vec3 size = leaf.box.GetSize();
        const Vec3 mid = leaf.box.GetCenter();
        const float sx = size.GetX(), sy = size.GetY(), sz = size.GetZ();
        if (sy <= 0.10F && std::min(sx, sz) <= 0.10F && std::max(sx, sz) >= 0.25F && std::max(sx, sz) <= 3.0F) {
            bars.push_back({mid, sx > sz, 0.5F * std::max(sx, sz), leaf.entity});
            continue;
        }
        if (sy >= 1.5F && std::max(sx, sz) <= 0.16F) {
            poles_.push_back({mid.GetX(), mid.GetZ(), std::max(sx, sz), leaf.box.mMin.GetY(), leaf.box.mMax.GetY()});
            continue;
        }
        if (leaf.axis_aligned && std::min(sx, sz) >= 0.28F) {
            Patch p;
            p.body = leaf.body;
            p.sub = leaf.sub;
            p.top = leaf.box.mMax.GetY();
            p.min_x = leaf.box.mMin.GetX();
            p.max_x = leaf.box.mMax.GetX();
            p.min_z = leaf.box.mMin.GetZ();
            p.max_z = leaf.box.mMax.GetZ();
            p.narrow = std::min(sx, sz) < 2.0F * limits.radius;
            patches_.push_back(p);
        }
    }
    stats_.patches = static_cast<std::uint32_t>(patches_.size());
    stats_.bars = static_cast<std::uint32_t>(bars.size());
    stats_.poles = static_cast<std::uint32_t>(poles_.size());

    // 3. Headroom: a top is walkable only where her standing capsule fits on
    // it somewhere (sampled along its long axis).
    std::vector<Patch> usable;
    for (const Patch &p : patches_) {
        const float cx = 0.5F * (p.min_x + p.max_x);
        const float cz = 0.5F * (p.min_z + p.max_z);
        const float lx = p.max_x - p.min_x;
        const float lz = p.max_z - p.min_z;
        bool ok = false;
        // Along its long axis first, then across it too (a step whose back
        // part is covered by the next step up is walkable at its front).
        static constexpr float kAlong[7] = {0.0F, -0.35F, 0.35F, -0.15F, 0.15F, -0.45F, 0.45F};
        static constexpr float kAcross[5] = {0.0F, -0.4F, 0.4F, -0.2F, 0.2F};
        for (int a = 0; a < 5 && !ok; ++a) {
            const float g = kAcross[a];
            if (g != 0.0F && std::min(lx, lz) < 4.0F * limits.radius) continue;
            for (const float f : kAlong) {
                const Vec3 s = lx >= lz ? Vec3(cx + f * lx, p.top, cz + g * lz) : Vec3(cx + g * lx, p.top, cz + f * lz);
                if (capsule_free(physics, ignore, limits, s)) {
                    ok = true;
                    break;
                }
            }
        }
        if (ok) usable.push_back(p);
    }
    patches_ = std::move(usable);
    stats_.usable_patches = static_cast<std::uint32_t>(patches_.size());

    // 4. Ladders: bars on one line, stacked at a climbing pitch.
    std::sort(bars.begin(), bars.end(), [](const Bar &a, const Bar &b) {
        if (a.along_x != b.along_x) return a.along_x < b.along_x;
        const float ka = a.along_x ? a.c.GetZ() : a.c.GetX();
        const float kb = b.along_x ? b.c.GetZ() : b.c.GetX();
        if (std::abs(ka - kb) > 1.0e-3F) return ka < kb;
        const float la = a.along_x ? a.c.GetX() : a.c.GetZ();
        const float lb = b.along_x ? b.c.GetX() : b.c.GetZ();
        if (std::abs(la - lb) > 1.0e-3F) return la < lb;
        return a.c.GetY() < b.c.GetY();
    });
    std::vector<bool> used(bars.size(), false);
    for (std::size_t i = 0; i < bars.size(); ++i) {
        if (used[i]) continue;
        std::vector<std::size_t> chain{i};
        used[i] = true;
        for (std::size_t j = i + 1; j < bars.size(); ++j) {
            const Bar &a = bars[chain.back()];
            const Bar &b = bars[j];
            if (used[j] || b.along_x != a.along_x) continue;
            const float across = a.along_x ? std::abs(b.c.GetZ() - a.c.GetZ()) : std::abs(b.c.GetX() - a.c.GetX());
            if (across > 0.06F) break;
            const float along = a.along_x ? std::abs(b.c.GetX() - a.c.GetX()) : std::abs(b.c.GetZ() - a.c.GetZ());
            if (along > 0.1F) continue;
            const float dy = b.c.GetY() - a.c.GetY();
            if (dy >= 0.18F && dy <= 0.46F) {
                chain.push_back(j);
                used[j] = true;
            }
        }
        if (chain.size() >= 3) {
            const Bar &lo = bars[chain.front()];
            const Bar &hi = bars[chain.back()];
            Ladder l;
            l.x = lo.c.GetX();
            l.z = lo.c.GetZ();
            l.along_x = lo.along_x;
            l.half = lo.half;
            l.bottom = lo.c.GetY();
            l.top = hi.c.GetY();
            l.rungs = static_cast<int>(chain.size());
            l.pitch = (l.top - l.bottom) / float(l.rungs - 1);
            l.entity = lo.entity;
            ladders_.push_back(l);
        } else {
            for (std::size_t k = 1; k < chain.size(); ++k) used[chain[k]] = false;
        }
    }
    stats_.ladders = static_cast<std::uint32_t>(ladders_.size());

    build_regions(limits);
    build_edges(physics, ignore, limits);
    stats_.regions = static_cast<std::uint32_t>(regions_.size());
    stats_.edges = static_cast<std::uint32_t>(edges_.size());
    for (const Edge &e : edges_) stats_.usable_edges += e.usable ? 1U : 0U;
    stats_.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

void AffordanceMap::build_regions(const Limits &limits) {
    // Tops join when they are a step apart and touch in plan.
    const float cell = 4.0F;
    std::unordered_map<long long, std::vector<int>> grid;
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const Patch &p = patches_[i];
        const int y = cell_of(p.top, 2.0F);
        for (int x = cell_of(p.min_x - 0.3F, cell); x <= cell_of(p.max_x + 0.3F, cell); ++x)
            for (int z = cell_of(p.min_z - 0.3F, cell); z <= cell_of(p.max_z + 0.3F, cell); ++z)
                grid[cell_key(x, y, z)].push_back(static_cast<int>(i));
    }
    UnionFind uf(patches_.size());
    patch_links_.assign(patches_.size(), {});
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const Patch &p = patches_[i];
        const int y0 = cell_of(p.top, 2.0F);
        for (int y = y0 - 1; y <= y0 + 1; ++y)
            for (int x = cell_of(p.min_x - 0.3F, cell); x <= cell_of(p.max_x + 0.3F, cell); ++x)
                for (int z = cell_of(p.min_z - 0.3F, cell); z <= cell_of(p.max_z + 0.3F, cell); ++z) {
                    const auto it = grid.find(cell_key(x, y, z));
                    if (it == grid.end()) continue;
                    for (const int j : it->second) {
                        if (j <= static_cast<int>(i)) continue;
                        const Patch &q = patches_[static_cast<std::size_t>(j)];
                        if (std::abs(q.top - p.top) <= limits.step_max_m && gap_xz(p, q) <= 0.25F) {
                            uf.join(static_cast<int>(i), j);
                            auto &li = patch_links_[i];
                            if (std::find(li.begin(), li.end(), j) == li.end()) {
                                li.push_back(j);
                                patch_links_[static_cast<std::size_t>(j)].push_back(static_cast<int>(i));
                            }
                        }
                    }
                }
    }
    std::unordered_map<int, int> root_to_region;
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const int root = uf.find(static_cast<int>(i));
        auto it = root_to_region.find(root);
        if (it == root_to_region.end()) {
            Region r;
            r.id = static_cast<int>(regions_.size());
            r.min_x = r.min_y = r.min_z = std::numeric_limits<float>::max();
            r.max_x = r.max_y = r.max_z = -std::numeric_limits<float>::max();
            regions_.push_back(r);
            it = root_to_region.emplace(root, r.id).first;
        }
        Region &r = regions_[static_cast<std::size_t>(it->second)];
        Patch &p = patches_[i];
        p.region = r.id;
        r.patches.push_back(static_cast<int>(i));
        const float a = std::max(1.0e-4F, (p.max_x - p.min_x) * (p.max_z - p.min_z));
        r.top = (r.top * r.area + p.top * a) / (r.area + a);
        r.area += a;
        r.min_x = std::min(r.min_x, p.min_x);
        r.max_x = std::max(r.max_x, p.max_x);
        r.min_z = std::min(r.min_z, p.min_z);
        r.max_z = std::max(r.max_z, p.max_z);
        r.min_y = std::min(r.min_y, p.top);
        r.max_y = std::max(r.max_y, p.top);
    }
}

int AffordanceMap::patch_at(const Vec3 feet, const float tol) const {
    int best = -1;
    float best_dy = tol;
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const Patch &p = patches_[i];
        if (feet.GetX() < p.min_x - 0.05F || feet.GetX() > p.max_x + 0.05F || feet.GetZ() < p.min_z - 0.05F ||
            feet.GetZ() > p.max_z + 0.05F) {
            continue;
        }
        const float dy = std::abs(feet.GetY() - p.top);
        if (dy <= best_dy) {
            best_dy = dy;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int AffordanceMap::region_at(const Vec3 feet) const {
    const int p = patch_at(feet, 0.3F);
    return p < 0 ? -1 : patches_[static_cast<std::size_t>(p)].region;
}

int AffordanceMap::region_near_top(const float y, const float tol) const {
    int best = -1;
    float area = 0.0F;
    for (const Region &r : regions_) {
        if (std::abs(r.top - y) <= tol && r.area > area) {
            area = r.area;
            best = r.id;
        }
    }
    return best;
}

void AffordanceMap::add_edge(Edge edge) {
    for (Edge &e : edges_) {
        if (e.from == edge.from && e.to == edge.to && e.kind == edge.kind) {
            if ((edge.usable && !e.usable) || (edge.usable == e.usable && edge.cost < e.cost)) {
                e = std::move(edge);
            }
            return;
        }
    }
    edges_.push_back(std::move(edge));
}

void AffordanceMap::build_edges(const PhysicsSystem &physics, const Ignore &ignore, const Limits &limits) {
    const auto patches_near = [&](const Vec3 at, const float r, const float lo, const float hi) {
        std::vector<int> out;
        for (std::size_t i = 0; i < patches_.size(); ++i) {
            const Patch &p = patches_[i];
            if (p.top < lo || p.top > hi) continue;
            const float gx = std::max({0.0F, p.min_x - at.GetX(), at.GetX() - p.max_x});
            const float gz = std::max({0.0F, p.min_z - at.GetZ(), at.GetZ() - p.max_z});
            if (gx * gx + gz * gz <= r * r) out.push_back(static_cast<int>(i));
        }
        return out;
    };
    const auto closest_on = [](const Patch &p, const Vec3 at, const float inset) {
        const float ix = std::min(inset, 0.5F * (p.max_x - p.min_x));
        const float iz = std::min(inset, 0.5F * (p.max_z - p.min_z));
        return Vec3(std::clamp(at.GetX(), p.min_x + ix, p.max_x - ix), p.top, std::clamp(at.GetZ(), p.min_z + iz, p.max_z - iz));
    };
    const auto free_spot = [&](const Patch &p, const Vec3 at, Vec3 &out, const bool ring = false) {
        for (const float inset : {limits.radius + 0.05F, limits.radius + 0.35F, limits.radius + 0.7F}) {
            const Vec3 s = closest_on(p, at, inset);
            if (capsule_free(physics, ignore, limits, s)) {
                out = s;
                return true;
            }
        }
        // Round a member standing in the floor (a pole's foot): the nearest
        // point is the member itself, so a ring round it on the same top.
        const float rings[3] = {limits.radius + 0.12F, limits.radius + 0.3F, limits.radius + 0.5F};
        for (int ri = 0; ring && ri < 3; ++ri) {
            const float r = rings[ri];
            for (int k = 0; k < 16; ++k) {
                const float a = float(k) * 0.3926991F;
                const Vec3 s = closest_on(p, at + Vec3(std::cos(a) * r, 0.0F, std::sin(a) * r), limits.radius + 0.05F);
                if (horizontal(s - at) < limits.radius && p.max_x - p.min_x > 2.0F * limits.radius) continue;
                if (capsule_free(physics, ignore, limits, s)) {
                    out = s;
                    return true;
                }
            }
        }
        out = closest_on(p, at, limits.radius + 0.05F);
        return false;
    };

    // Ladders: up and down, each with its own checks.
    for (const Ladder &l : ladders_) {
        const Vec3 line(l.x, 0.0F, l.z);
        for (const float side : {1.0F, -1.0F}) {
            const Vec3 out = l.along_x ? Vec3(0, 0, side) : Vec3(side, 0, 0);
            const Vec3 foot_at = line + out * 0.45F;
            const auto feet = patches_near(foot_at, 0.6F, l.bottom - limits.jump_hold_max_m, l.bottom - 0.05F);
            const auto heads = patches_near(line - out * 0.3F, 1.1F, l.top - 0.7F, l.top + 1.3F);
            for (const int fi : feet) {
                const Patch &fp = patches_[static_cast<std::size_t>(fi)];
                const float reach = l.bottom - fp.top;
                const bool jump = reach > limits.hand_reach_standing() - 0.1F;
                for (const int hi : heads) {
                    const Patch &hp = patches_[static_cast<std::size_t>(hi)];
                    if (hp.region == fp.region || hp.top < fp.top + 1.0F) continue;
                    Vec3 stand_foot{}, stand_head{};
                    const bool foot_ok = free_spot(fp, foot_at, stand_foot);
                    const bool head_ok = free_spot(hp, line - out * 0.6F, stand_head);
                    const Vec3 lip_in = closest_on(hp, line, limits.radius + 0.1F);
                    const bool over_ok = capsule_free(physics, ignore, limits, lip_in);
                    // The climb itself: her body on the rungs, foot to head,
                    // every half meter (a deck's edge over the ladder stops her).
                    bool corridor_ok = true;
                    float corridor_block = 0.0F;
                    {
                        const Vec3 on = line + out * 0.42F;
                        const float lo = fp.top + limits.center_above_feet() + 0.05F;
                        const float hi = l.top - 0.35F;
                        for (float y = lo; y <= hi + 0.01F && corridor_ok; y += 0.5F) {
                            if (!capsule_free(physics, ignore, limits, on + Vec3(0.0F, y - limits.center_above_feet(), 0.0F))) {
                                corridor_ok = false;
                                corridor_block = y;
                            }
                        }
                    }
                    MoveSpec up;
                    up.kind = jump ? MoveKind::JumpCatch : MoveKind::RungUp;
                    up.line = line;
                    up.out = out;
                    up.bottom_y = l.bottom;
                    up.top_y = l.top;
                    up.start = stand_foot;
                    up.goal = stand_head;
                    up.goal_top = hp.top;
                    up.bar_half = l.half;
                    char facts[256];
                    std::snprintf(facts, sizeof facts, "ladder %d rungs pitch %.2f, %.2f..%.2f; first rung %.2f over the floor%s",
                                  l.rungs, double(l.pitch), double(l.bottom), double(l.top), double(reach),
                                  jump ? " (jump catch)" : "");
                    Edge e;
                    e.from = fp.region;
                    e.to = hp.region;
                    e.kind = up.kind;
                    e.spec = up;
                    e.cost = (l.top - l.bottom) / limits.climb_mps + (jump ? 6.0F : 2.0F);
                    e.usable = foot_ok && head_ok && over_ok && corridor_ok;
                    e.reason = facts;
                    char corridor[96];
                    std::snprintf(corridor, sizeof corridor, "; blocked: her body does not fit on the rungs at %.2f",
                                  double(corridor_block));
                    if (!e.usable) {
                        e.reason += !foot_ok ? "; blocked: no room to stand at the foot"
                                    : !head_ok ? "; blocked: no room to stand at the head"
                                    : !over_ok ? "; blocked: capsule does not pass the lip"
                                               : corridor;
                    }
                    add_edge(e);
                    // Down, separately: top entry over the head, and a floor at
                    // the foot within a hang-drop of the bottom rung.
                    Edge d;
                    d.from = hp.region;
                    d.to = fp.region;
                    d.kind = MoveKind::RungDown;
                    d.spec = up;
                    d.spec.kind = MoveKind::RungDown;
                    d.spec.start = closest_on(hp, line, limits.radius + 0.05F);
                    d.spec.goal = stand_foot;
                    d.spec.goal_top = fp.top;
                    const float hang_feet = l.bottom - limits.hang_below_hold - limits.center_above_feet();
                    const bool foot_reached = hang_feet - fp.top <= 0.4F;
                    const bool drop_ok = (l.bottom - fp.top) <= limits.drop_max_m;
                    const Vec3 hang_c = line + out * 0.42F + Vec3(0, l.top - limits.hang_below_hold + 0.4F, 0);
                    const bool hang_ok = capsule_free(physics, ignore, limits, hang_c - Vec3(0, limits.center_above_feet() + 0.04F, 0));
                    d.usable = over_ok && hang_ok && foot_ok && corridor_ok && (foot_reached || drop_ok);
                    d.cost = (l.top - l.bottom) / limits.climb_mps + 2.0F;
                    d.reason = facts;
                    if (!foot_reached) d.reason += drop_ok ? "; ends in a drop off the bottom rung" : "; blocked: drop off the bottom rung too far";
                    if (!hang_ok) d.reason += "; blocked: capsule does not fit going over the head";
                    if (!over_ok) d.reason += "; blocked: capsule does not fit at the lip";
                    if (!foot_ok) d.reason += "; blocked: no room to stand at the foot";
                    if (!corridor_ok) d.reason += corridor;
                    add_edge(d);
                }
            }
        }
    }

    // Poles: up and down.
    for (const Pole &pole : poles_) {
        const Vec3 axis(pole.x, 0.0F, pole.z);
        const auto feet = patches_near(axis, 0.75F, pole.bottom - 0.4F, pole.bottom + 1.2F);
        const auto heads = patches_near(axis, 0.9F, pole.top - 1.4F, pole.top + 0.6F);
        for (const int fi : feet) {
            const Patch &fp = patches_[static_cast<std::size_t>(fi)];
            for (const int hi : heads) {
                const Patch &hp = patches_[static_cast<std::size_t>(hi)];
                if (hp.region == fp.region || hp.top < fp.top + 2.0F) continue;
                const Vec3 lip = closest_on(hp, axis, 0.0F);
                const Vec3 away = flat(axis - lip).NormalizedOr(Vec3::sAxisZ());
                // Which side of it she hugs: away from the lip, unless its head
                // stands above the lip (then her way over from there crosses the
                // pole): from beside it, where her body fits along it.
                Vec3 out = away;
                if (pole.top > hp.top + 0.3F) {
                    const Vec3 perp(-away.GetZ(), 0.0F, away.GetX());
                    for (const Vec3 side : {perp, -perp}) {
                        bool fits = true;
                        for (float y = fp.top + 0.5F; y < hp.top + 0.5F && fits; y += 1.0F) {
                            fits = capsule_free(physics, ignore, limits, axis + side * (limits.radius + 0.1F) + Vec3(0, y, 0));
                        }
                        if (fits) {
                            out = side;
                            break;
                        }
                    }
                }
                Vec3 stand_foot{}, stand_head{};
                // Where she stands at its foot: the nearest of that floor to
                // the pole (a board beside it as much as a deck round it).
                const bool foot_ok = free_spot(fp, closest_on(fp, axis, 0.0F), stand_foot, true) &&
                                     horizontal(stand_foot - axis) <= limits.reach_out;
                const bool head_ok = free_spot(hp, lip, stand_head);
                bool climb_room = true;
                for (float y = fp.top + 0.5F; y < hp.top - 2.0F && climb_room; y += 2.0F) {
                    climb_room = capsule_free(physics, ignore, limits, axis + out * (limits.radius + 0.1F) + Vec3(0, y, 0));
                }
                MoveSpec up;
                up.kind = MoveKind::PoleUp;
                up.line = axis;
                up.out = out;
                up.bottom_y = pole.bottom;
                up.top_y = pole.top;
                up.start = stand_foot;
                up.goal = stand_head;
                up.goal_top = hp.top;
                char facts[200];
                std::snprintf(facts, sizeof facts, "pole section %.2f, %.2f..%.2f; lip %.2f off the axis",
                              double(pole.section), double(pole.bottom), double(pole.top), double(horizontal(lip - axis)));
                Edge e;
                e.from = fp.region;
                e.to = hp.region;
                e.kind = MoveKind::PoleUp;
                e.spec = up;
                e.cost = (hp.top - fp.top) / limits.pole_up_mps + 2.0F;
                e.usable = foot_ok && head_ok && climb_room;
                e.reason = facts;
                if (!foot_ok) e.reason += "; blocked: no room at the foot";
                if (!head_ok) e.reason += "; blocked: no room at the head";
                if (!climb_room) e.reason += "; blocked: capsule does not fit along the pole";
                add_edge(e);
                Edge d = e;
                d.from = hp.region;
                d.to = fp.region;
                d.kind = MoveKind::PoleDown;
                d.spec.kind = MoveKind::PoleDown;
                d.spec.start = closest_on(hp, axis, limits.radius + 0.02F);
                d.spec.goal = stand_foot;
                d.spec.goal_top = fp.top;
                d.cost = (hp.top - fp.top) / limits.pole_down_mps + 2.0F;
                add_edge(d);
            }
        }
    }

    // Lips between regions: a mantle up; a step or a hang-drop down.
    const float cell = 3.0F;
    std::unordered_map<long long, std::vector<int>> grid;
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const Patch &p = patches_[i];
        for (int x = cell_of(p.min_x - 0.7F, cell); x <= cell_of(p.max_x + 0.7F, cell); ++x)
            for (int z = cell_of(p.min_z - 0.7F, cell); z <= cell_of(p.max_z + 0.7F, cell); ++z)
                grid[cell_key(x, cell_of(p.top, 5.0F), z)].push_back(static_cast<int>(i));
    }
    std::vector<int> seen;
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const Patch &lo = patches_[i];
        const int y0 = cell_of(lo.top, 5.0F);
        seen.clear();
        for (int y = y0; y <= y0 + 1; ++y)
            for (int x = cell_of(lo.min_x - 0.7F, cell); x <= cell_of(lo.max_x + 0.7F, cell); ++x)
                for (int z = cell_of(lo.min_z - 0.7F, cell); z <= cell_of(lo.max_z + 0.7F, cell); ++z) {
                    const auto it = grid.find(cell_key(x, y, z));
                    if (it == grid.end()) continue;
                    for (const int j : it->second) {
                        const Patch &hi = patches_[static_cast<std::size_t>(j)];
                        const float rise = hi.top - lo.top;
                        if (hi.region == lo.region || rise <= limits.step_max_m || rise > limits.drop_max_m) continue;
                        if (gap_xz(lo, hi) > 0.6F) continue;
                        if (std::find(seen.begin(), seen.end(), j) != seen.end()) continue;
                        seen.push_back(j);
                        const Vec3 lc((lo.min_x + lo.max_x) * 0.5F, lo.top, (lo.min_z + lo.max_z) * 0.5F);
                        Vec3 lip = closest_on(hi, lc, 0.0F);
                        Vec3 below = closest_on(lo, lip, limits.radius + 0.1F);
                        Vec3 above = closest_on(hi, lip, limits.radius + 0.1F);
                        bool below_ok = horizontal(below - lip) <= 0.9F && capsule_free(physics, ignore, limits, below);
                        // Over the lip itself: her body at the edge (a rail along it stops her).
                        const auto passage = [&](const Vec3 edge, const Vec3 inward) {
                            return capsule_free(physics, ignore, limits, Vec3(edge.GetX(), hi.top, edge.GetZ()) + inward * 0.05F);
                        };
                        bool above_ok = capsule_free(physics, ignore, limits, above) &&
                                        passage(lip, flat(above - lip).NormalizedOr(Vec3::sZero()));
                        if (!(below_ok && above_ok)) {
                            // Walk the high top's edges for a lip with the low top
                            // just outside it and room for her on both sides.
                            float best = 1.0e9F;
                            const float step = 0.3F;
                            const float edges[4][4] = {{hi.min_x, hi.min_z, 1, 0}, {hi.min_x, hi.max_z, 1, 0},
                                                       {hi.min_x, hi.min_z, 0, 1}, {hi.max_x, hi.min_z, 0, 1}};
                            const Vec3 normals[4] = {Vec3(0, 0, -1), Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(1, 0, 0)};
                            for (int side = 0; side < 4; ++side) {
                                const float len = edges[side][2] > 0 ? hi.max_x - hi.min_x : hi.max_z - hi.min_z;
                                const int n = std::max(1, int(len / step));
                                for (int t = 0; t <= n; ++t) {
                                    const float u = len * (float(t) + 0.5F * (t < n)) / float(n + 1) + 0.0F;
                                    const Vec3 e(edges[side][0] + edges[side][2] * u, hi.top, edges[side][1] + edges[side][3] * u);
                                    const Vec3 b = e + normals[side] * (limits.radius + 0.1F);
                                    if (b.GetX() < lo.min_x || b.GetX() > lo.max_x || b.GetZ() < lo.min_z || b.GetZ() > lo.max_z) continue;
                                    const float d = horizontal(e - lc);
                                    if (d >= best) continue;
                                    const Vec3 bb(b.GetX(), lo.top, b.GetZ());
                                    const Vec3 aa = closest_on(hi, e - normals[side] * (limits.radius + 0.1F), limits.radius + 0.1F);
                                    if (capsule_free(physics, ignore, limits, bb) && capsule_free(physics, ignore, limits, aa) &&
                                        passage(e, -normals[side])) {
                                        best = d;
                                        lip = e;
                                        below = bb;
                                        above = aa;
                                        below_ok = above_ok = true;
                                    }
                                }
                            }
                        }
                        if (horizontal(below - lip) > 0.9F) continue;
                        char facts[120];
                        std::snprintf(facts, sizeof facts, "lip %.2f over the floor below", double(rise));
                        Edge up;
                        up.from = lo.region;
                        up.to = hi.region;
                        up.spec.start = below;
                        up.spec.goal = above;
                        up.spec.goal_top = hi.top;
                        up.spec.line = below;
                        up.spec.out = flat(below - above).NormalizedOr(Vec3::sAxisZ());
                        up.reason = facts;
                        if (rise <= limits.mantle_max_m) {
                            up.kind = MoveKind::Mantle;
                            up.spec.kind = MoveKind::Mantle;
                            up.cost = 3.0F + rise;
                            up.usable = below_ok && above_ok;
                            if (!up.usable) up.reason += "; blocked: capsule sweep";
                            add_edge(up);
                        }
                        Edge down = up;
                        down.from = hi.region;
                        down.to = lo.region;
                        down.spec.start = above;
                        down.spec.goal = below;
                        down.spec.goal_top = lo.top;
                        down.spec.out = flat(below - above).NormalizedOr(Vec3::sAxisZ());
                        down.kind = rise <= 1.0F ? MoveKind::StepDown : MoveKind::HangDrop;
                        down.spec.kind = down.kind;
                        down.cost = 2.0F + rise;
                        down.usable = below_ok && above_ok;
                        down.reason = facts;
                        if (!down.usable) down.reason += "; blocked: capsule sweep";
                        add_edge(down);
                    }
                }
    }
    std::sort(edges_.begin(), edges_.end(), [](const Edge &a, const Edge &b) {
        if (a.from != b.from) return a.from < b.from;
        if (a.to != b.to) return a.to < b.to;
        return a.kind < b.kind;
    });
}

std::vector<int> AffordanceMap::plan(const int from, const int to) const {
    std::vector<int> result;
    if (from < 0 || to < 0 || from >= static_cast<int>(regions_.size()) || to >= static_cast<int>(regions_.size())) {
        return result;
    }
    std::vector<float> dist(regions_.size(), std::numeric_limits<float>::max());
    std::vector<int> via(regions_.size(), -1);
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    dist[static_cast<std::size_t>(from)] = 0.0F;
    open.push({0.0F, from});
    while (!open.empty()) {
        const auto [d, r] = open.top();
        open.pop();
        if (d > dist[static_cast<std::size_t>(r)]) continue;
        if (r == to) break;
        // edges_ is sorted by from: the run for r.
        auto it = std::lower_bound(edges_.begin(), edges_.end(), r, [](const Edge &e, const int v) { return e.from < v; });
        for (; it != edges_.end() && it->from == r; ++it) {
            if (!it->usable) continue;
            const float nd = d + it->cost;
            if (nd < dist[static_cast<std::size_t>(it->to)]) {
                dist[static_cast<std::size_t>(it->to)] = nd;
                via[static_cast<std::size_t>(it->to)] = static_cast<int>(it - edges_.begin());
                open.push({nd, it->to});
            }
        }
    }
    if (from == to || via[static_cast<std::size_t>(to)] < 0) {
        return result;
    }
    for (int r = to; r != from;) {
        const int e = via[static_cast<std::size_t>(r)];
        result.push_back(e);
        r = edges_[static_cast<std::size_t>(e)].from;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<Vec3> AffordanceMap::walk_path(const Vec3 from, const Vec3 to) const {
    std::vector<Vec3> path;
    const int a = patch_at(from, 0.4F);
    const int b = patch_at(to, 0.4F);
    if (a < 0 || b < 0 || a == b) {
        path.push_back(to);
        return path;
    }
    std::vector<int> via(patches_.size(), -2);
    std::queue<int> open;
    open.push(a);
    via[static_cast<std::size_t>(a)] = -1;
    while (!open.empty()) {
        const int p = open.front();
        open.pop();
        if (p == b) break;
        for (const int q : patch_links_[static_cast<std::size_t>(p)]) {
            if (via[static_cast<std::size_t>(q)] == -2) {
                via[static_cast<std::size_t>(q)] = p;
                open.push(q);
            }
        }
    }
    if (via[static_cast<std::size_t>(b)] == -2) {
        path.push_back(to);
        return path;
    }
    std::vector<int> chain;
    for (int p = b; p >= 0; p = via[static_cast<std::size_t>(p)]) chain.push_back(p);
    std::reverse(chain.begin(), chain.end());
    // Into each shared stretch, as near the line to the goal as it allows,
    // and her own radius in from its ends.
    for (std::size_t i = 1; i < chain.size(); ++i) {
        const Patch &p = patches_[static_cast<std::size_t>(chain[i - 1])];
        const Patch &q = patches_[static_cast<std::size_t>(chain[i])];
        const float x0 = std::max(p.min_x, q.min_x), x1 = std::min(p.max_x, q.max_x);
        const float z0 = std::max(p.min_z, q.min_z), z1 = std::min(p.max_z, q.max_z);
        // The middle of the shared stretch: as far from either patch's
        // unsupported sides as the overlap allows.
        const auto pick = [](const float lo, const float hi, const float want) {
            (void)want;
            return 0.5F * (lo + hi);
        };
        // The crossing: just inside the one, then just inside the other (a
        // seam where they only touch has no inside of its own).
        const float cx = pick(x0, x1, to.GetX());
        const float cz = pick(z0, z1, to.GetZ());
        const auto inside = [](const Patch &r, const float x, const float z) {
            const float ix = std::min(0.45F, 0.5F * (r.max_x - r.min_x));
            const float iz = std::min(0.45F, 0.5F * (r.max_z - r.min_z));
            return std::make_pair(std::clamp(x, r.min_x + ix, r.max_x - ix), std::clamp(z, r.min_z + iz, r.max_z - iz));
        };
        const auto [px, pz] = inside(p, cx, cz);
        const auto [qx, qz] = inside(q, cx, cz);
        path.push_back(Vec3(px, p.top, pz));
        path.push_back(Vec3(qx, q.top, qz));
    }
    path.push_back(to);
    return path;
}

} // namespace scraperx::sim::traversal

namespace scraperx::sim::traversal {
bool Climber::feel_for_rungs(const JPH::Vec3 from, const JPH::Vec3 dir, const float y0, const float y1, JPH::Vec3 &line) const {
    for (float s = 0.1F; s <= 0.9F; s += 0.05F) {
        for (float y = y0; y <= y1; y += 0.1F) {
            Hold hold;
            JPH::RVec3 at;
            const JPH::Vec3 p = JPH::Vec3(from.GetX(), y, from.GetZ()) + dir * s;
            if (take_hold(p, 0.05F, true, hold, &at)) {
                JPH::BodyLockRead lock(physics_.GetBodyLockInterface(), hold.body);
                JPH::AABox bounds;
                bool bar = false;
                if (lock.Succeeded() && member_bounds(lock.GetBody(), hold.sub, bounds, bar) && bounds.GetSize().GetY() < 0.2F) {
                    const JPH::Vec3 c = bounds.GetCenter();
                    line = JPH::Vec3(c.GetX(), 0.0F, c.GetZ());
                    return true;
                }
            }
        }
    }
    return false;
}
std::vector<Vec3> grid_walk(const PhysicsSystem &physics, const Ignore &ignore, const Limits &limits, const Vec3 from,
                           const Vec3 to, const float margin, const float cell, const int max_nodes) {
    // The grid on the goal: its lines pass through the goal (a narrow board
    // the goal stands on is on a grid line, not between two).
    const float x0 = to.GetX() - cell * std::ceil((to.GetX() - (std::min(from.GetX(), to.GetX()) - margin)) / cell);
    const float z0 = to.GetZ() - cell * std::ceil((to.GetZ() - (std::min(from.GetZ(), to.GetZ()) - margin)) / cell);
    const int nx = static_cast<int>((std::max(from.GetX(), to.GetX()) + margin - x0) / cell) + 1;
    const int nz = static_cast<int>((std::max(from.GetZ(), to.GetZ()) + margin - z0) / cell) + 1;
    const auto key = [&](const int i, const int k) { return k * nx + i; };
    const auto cell_of = [&](const Vec3 p) {
        return std::make_pair(std::clamp(static_cast<int>(std::lround((p.GetX() - x0) / cell)), 0, nx - 1),
                              std::clamp(static_cast<int>(std::lround((p.GetZ() - z0) / cell)), 0, nz - 1));
    };
    const FixedFilter filter(ignore);
    const SteelFilter things(ignore);   // loose things in her way count too
    // Her body as it walks: a hair inside her radius, her soles a step's
    // rise clear (a step's riser is under her stride, not in her way).
    const float lift = limits.step_max_m;
    const CapsuleShape capsule(limits.half_height - 0.5F * lift, limits.radius - 0.04F);
    const auto stand = [&](const int i, const int k, const float near_y, float &feet) {
        const RVec3 top(x0 + cell * float(i), near_y + limits.step_max_m + 0.05F, z0 + cell * float(k));
        const float depth = limits.step_max_m + 1.15F;
        RayCastResult hit;
        if (!physics.GetNarrowPhaseQuery().CastRay(RRayCast(top, Vec3(0.0F, -depth, 0.0F)), hit, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things)) {
            return false;
        }
        feet = float(top.GetY()) - hit.mFraction * depth;
        // At the move's own start the move's approach is the judge (it gets
        // her there by contact, or fails): floor is enough.
        if (std::hypot(float(top.GetX()) - to.GetX(), float(top.GetZ()) - to.GetZ()) <= 1.0F) {
            return true;
        }
        CollideShapeSettings settings;
        settings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;
        AnyHitCollisionCollector<CollideShapeCollector> collector;
        const RVec3 c(top.GetX(), feet + lift + 0.02F + (limits.radius - 0.04F) + (limits.half_height - 0.5F * lift), top.GetZ());
        physics.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1.0F), RMat44::sTranslation(c), settings,
                                                   RVec3::sZero(), collector, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things);
        return !collector.HadHit();
    };
    struct Node {
        float g = std::numeric_limits<float>::max();
        float feet = 0.0F;
        int via = -1;
        bool closed = false;
    };
    std::unordered_map<int, Node> nodes;
    const auto [si, sk] = cell_of(from);
    const auto [gi, gk] = cell_of(to);
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    nodes[key(si, sk)] = Node{0.0F, from.GetY(), -1, false};
    open.push({0.0F, key(si, sk)});
    const auto h = [&](const int i, const int k) { return cell * std::hypot(float(i - gi), float(k - gk)); };
    int expanded = 0;
    int found = -1;
    int nearest = -1;
    float nearest_h = std::numeric_limits<float>::max();
    while (!open.empty() && expanded < max_nodes) {
        const int n = open.top().second;
        open.pop();
        Node &cur = nodes[n];
        if (cur.closed) continue;
        cur.closed = true;
        ++expanded;
        const int ci = n % nx;
        const int ck = n / nx;
        if (h(ci, ck) < nearest_h) {
            nearest_h = h(ci, ck);
            nearest = n;
        }
        if (ci == gi && ck == gk) {
            found = n;
            break;
        }
        const float cur_g = cur.g;
        const float cur_feet = cur.feet;
        for (int dk = -1; dk <= 1; ++dk) {
            for (int di = -1; di <= 1; ++di) {
                if (di == 0 && dk == 0) continue;
                const int i = ci + di;
                const int k = ck + dk;
                if (i < 0 || k < 0 || i >= nx || k >= nz) continue;
                const int m = key(i, k);
                auto it = nodes.find(m);
                if (it != nodes.end() && it->second.closed) continue;
                float feet = 0.0F;
                // Up a step at most; down as far as she steps off a box (a
                // drop under 1 m she just walks off).
                if (!stand(i, k, cur_feet, feet) || feet - cur_feet > limits.step_max_m + 0.08F ||
                    cur_feet - feet > 1.0F) {
                    continue;   // no floor for her there from here (maybe from another side)
                }
                const float g = cur_g + cell * ((di != 0 && dk != 0) ? 1.4142F : 1.0F);
                Node &next = nodes[m];
                if (g < next.g) {
                    next.g = g;
                    next.feet = feet;
                    next.via = n;
                    open.push({g + h(i, k), m});
                }
            }
        }
    }
    std::vector<Vec3> path;
    if (found < 0 && max_nodes > 2 && nearest_h <= 1.5F) found = nearest;   // the goal's own spot crowded: as near as she fits
    if (found < 0) return path;
    for (int n = found; n >= 0; n = nodes[n].via) {
        path.push_back(Vec3(x0 + cell * float(n % nx), nodes[n].feet, z0 + cell * float(n / nx)));
    }
    std::reverse(path.begin(), path.end());
    // Keep the turns only (straight runs collapse).
    std::vector<Vec3> turns;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i == 0 || i + 1 == path.size()) {
            if (i != 0) turns.push_back(path[i]);
            continue;
        }
        const Vec3 a = path[i] - path[i - 1];
        const Vec3 b = path[i + 1] - path[i];
        if (std::abs(a.GetX() * b.GetZ() - a.GetZ() * b.GetX()) > 1.0e-3F) turns.push_back(path[i]);
    }
    turns.push_back(to);
    return turns;
}

} // namespace scraperx::sim::traversal
