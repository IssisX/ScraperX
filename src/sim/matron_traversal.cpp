#include "sim/matron_traversal.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include <algorithm>
#include <utility>
#include <cstdlib>
#include <chrono>
#include <cmath>
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
    case MoveKind::GapJump: return "gap_jump";
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
constexpr float kSoleSupportProbeM = 0.10F;

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

float leg_speed_cap(const Limits &limits) {
    if (limits.mass_kg <= 0.0F || limits.leg_travel_m <= 0.0F || limits.power_w <= 0.0F) return 0.0F;
    const float acceleration = limits.total_force_n / limits.mass_kg - kGravity;
    if (acceleration <= 0.0F) return 0.0F;
    return std::max(0.0F, std::min({limits.launch_dv_mps, std::sqrt(2.0F * acceleration * limits.leg_travel_m),
                                   limits.power_w / (limits.mass_kg * kGravity)}));
}

constexpr float kGapHopM = 0.16F;
constexpr float kGapRunMps = 3.2F;
constexpr float kGapDropM = 1.25F;
constexpr float kPlanTickS = 1.0F / 90.0F;

struct GapSample {
    float seconds = 0.0F;
    float height = 0.0F;
};
struct GapFlight {
    std::vector<GapSample> samples;
    float seconds = 0.0F;
    float drive_seconds = 0.0F;
};

// Predict a short push on stationary steel followed by gravity. This is
// planning only: the executor uses the same planted-leg drive as every
// other jump, and loses that push immediately if its real contact goes.
GapFlight gap_flight(const Limits &limits, const float drop, const float initial_speed = 0.0F) {
    GapFlight flight;
    if (drop <= 0.0F || drop > kGapDropM || leg_speed_cap(limits) <= 0.0F) return flight;
    float height = 0.0F, speed = initial_speed;
    bool pushing = true;
    flight.samples.push_back({0.0F, 0.0F});
    for (int tick = 0; tick < 180; ++tick) {
        const float remaining = limits.leg_travel_m - height;
        const float coast_speed = speed - kGravity * kPlanTickS;
        const float next_cap = std::min(limits.launch_dv_mps, remaining / kPlanTickS);
        if (pushing && (remaining <= 0.0F ||
            (tick > 0 && speed >= std::sqrt(2.0F * kGravity * std::max(kGapHopM - height, 0.0F))) ||
            speed >= limits.launch_dv_mps - 0.001F || coast_speed >= next_cap)) {
            pushing = false;
            flight.drive_seconds = float(tick) * kPlanTickS;
        }
        float force = 0.0F;
        if (pushing) {
            const float q = kPlanTickS / limits.mass_kg;
            const float end_power_force = (-coast_speed + std::sqrt(coast_speed * coast_speed +
                                           4.0F * q * limits.power_w)) / (2.0F * q);
            force = std::max(0.0F, std::min({limits.total_force_n, end_power_force,
                                           (next_cap - coast_speed) / q}));
        }
        speed = coast_speed + force * (kPlanTickS / limits.mass_kg);
        const float next = height + speed * kPlanTickS;
        if (!pushing && next <= -drop) {
            const float fraction = (height + drop) / (height - next);
            flight.seconds = (float(tick) + fraction) * kPlanTickS;
            flight.samples.push_back({flight.seconds, -drop});
            return flight;
        }
        height = next;
        flight.samples.push_back({float(tick + 1) * kPlanTickS, height});
    }
    return {};
}

bool gap_segment_free(const PhysicsSystem &physics, const Ignore &ignore, const Limits &limits,
                      const Vec3 from, const Vec3 to) {
    // This volume contains her whole capsule, including its bottom. The
    // extra radius may touch a floor her real soles are just above; ignore
    // only those upward floor contacts, never a raised obstacle or side.
    const CapsuleShape room(limits.half_height, limits.radius + 0.02F);
    const RVec3 centre = from + Vec3(0, limits.center_above_feet(), 0);
    const RShapeCast sweep(&room, Vec3::sReplicate(1.0F), RMat44::sTranslation(centre), to - from);
    AllHitCollisionCollector<CastShapeCollector> hits;
    const FixedFilter filter(ignore);
    physics.GetNarrowPhaseQuery().CastShape(sweep, ShapeCastSettings(), centre, hits,
                                           Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), filter);
    for (const ShapeCastResult &hit : hits.mHits) {
        const float floor_y = float(centre.GetY()) + hit.mContactPointOn2.GetY();
        if (hit.mPenetrationAxis.NormalizedOr(Vec3::sZero()).GetY() < -0.999F &&
            std::min(from.GetY(), to.GetY()) >= floor_y - 0.005F) continue;
        return false;
    }
    return true;
}

bool gap_floor_at(const PhysicsSystem &physics, const Ignore &ignore, const Vec3 feet) {
    const RRayCast ray(feet + Vec3(0, 0.08F, 0), Vec3(0, -0.16F, 0));
    RayCastResult hit;
    const FixedFilter filter(ignore);
    if (!physics.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), filter))
        return false;
    const RVec3 at = ray.GetPointOnRay(hit.mFraction);
    const BodyLockRead lock(physics.GetBodyLockInterface(), hit.mBodyID);
    return lock.Succeeded() && std::abs(float(at.GetY()) - feet.GetY()) < 0.03F &&
           lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, at).GetY() > 0.99F;
}

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

// A leaf's bounds in its body's own frame. Transforming its world AABB
// backward is not equivalent: the box grows when the same member rotates.
bool member_local_bounds(const Body &body, const SubShapeID &sub, AABox &out) {
    SubShapeID remainder;
    const TransformedShape member = body.GetShape()->GetSubShapeTransformedShape(
        sub, RVec3::sZero(), Quat::sIdentity(), Vec3::sReplicate(1.0F), remainder);
    if (member.mShape == nullptr) return false;
    out = member.GetWorldSpaceBounds();
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
    } else {
        brake_used -= power;
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
    if (foot_.valid() && bodies.IsAdded(foot_.body)) {
        support = bodies.GetPointVelocity(foot_.body, foot_world_);
    } else if (hold_.valid() && bodies.IsAdded(hold_.body)) {
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

bool Climber::supported(float *top, BodyID *by, RVec3 *point) const {
    const RVec3 c = center();
    const SteelFilter filter(ignore_);
    const float reach = limits_.center_above_feet() + kSoleSupportProbeM;
    for (const Vec3 off : {Vec3::sZero(), Vec3(0.18F, 0, 0), Vec3(-0.18F, 0, 0), Vec3(0, 0, 0.18F), Vec3(0, 0, -0.18F)}) {
        RayCastResult hit;
        const RRayCast ray(c + off, Vec3(0.0F, -reach, 0.0F));
        if (physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter)) {
            const RVec3 contact = ray.GetPointOnRay(hit.mFraction);
            if (top != nullptr) {
                *top = float(contact.GetY());
            }
            if (by != nullptr) {
                *by = hit.mBodyID;
            }
            if (point != nullptr) {
                *point = contact;
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
    return d.GetY() <= limits_.hand_above_center + 0.15F && d.GetY() >= -limits_.reach_low - 0.6F &&
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
    // The top she means first: a lower ridge in front of it (a wall standing
    // on the step below) is not its lip. Only if no top at that height is in
    // reach does a near one do.
    for (const float tol : {0.15F, 0.7F}) {
    for (const float ahead : {0.35F, 0.55F, 0.8F, 1.1F}) {
        const RVec3 from = RVec3(spec_.line.GetX(), near_y + 1.2F, spec_.line.GetZ()) + toward * ahead;
        RayCastResult hit;
        const RRayCast ray(from, Vec3(0.0F, -2.0F, 0.0F));
        if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter)) {
            continue;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mFraction);
        if (std::abs(float(point.GetY()) - near_y) > tol) {
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

// The leg extends beneath the capsule while its foot remains planted.
// Keep that contact throughout the stroke; a new floor under an airborne
// body cannot restart the push.
bool Climber::plant_leg(const float target_hand_y, const bool required) {
    const RVec3 c = center();
    const float stroke = limits_.leg_travel_m;
    const float speed_cap = leg_speed_cap(limits_);
    if (speed_cap <= 0.0F) {
        if (required) fail(Failure::OutOfRange);
        return false;
    }
    if (target_hand_y > float(c.GetY()) + limits_.hand_above_center + stroke + speed_cap * speed_cap / (2.0F * kGravity)) {
        if (required) fail(Failure::OutOfRange);
        return false;
    }
    const SteelFilter filter(ignore_);
    for (const Vec3 off : {Vec3::sZero(), Vec3(0.18F, 0, 0), Vec3(-0.18F, 0, 0), Vec3(0, 0, 0.18F), Vec3(0, 0, -0.18F)}) {
        RayCastResult hit;
        const RRayCast ray(c + off, Vec3(0, -(limits_.center_above_feet() + kSoleSupportProbeM), 0));
        if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter)) continue;
        const RVec3 at = ray.GetPointOnRay(hit.mFraction);
        if (float(c.GetY() - at.GetY()) > limits_.center_above_feet() + 0.03F) continue;
        BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, at).GetY() < 0.7F) continue;
        const Body &floor = lock.GetBody();
        AABox bounds;
        bool bar = false;
        if (!member_bounds(floor, hit.mSubShapeID2, bounds, bar)) continue;
        AABox local_bounds;
        if (!member_local_bounds(floor, hit.mSubShapeID2, local_bounds)) continue;
        foot_.body = hit.mBodyID;
        foot_.sub = hit.mSubShapeID2;
        foot_.local = Vec3(floor.GetInverseCenterOfMassTransform() * at);
        foot_.member_local = local_bounds;
        foot_world_ = at;
        leg_start_m_ = float(c.GetY() - at.GetY());
        leg_target_y_ = target_hand_y;
        leg_push_ticks_ = 0;
        view_.leg_driving = true;
        view_.leg_travel_m = 0.0F;
        return true;
    }
    if (required) fail(Failure::NotSupported);
    return false;
}

bool Climber::planted_leg_contact(RVec3 &at) const {
    if (!foot_.valid()) return false;
    RVec3 anchor;
    {
        BodyLockRead lock(physics_.GetBodyLockInterface(), foot_.body);
        if (!lock.Succeeded()) return false;
        const Body &floor = lock.GetBody();
        if (!floor.IsInBroadPhase() || floor.IsSensor()) return false;
        AABox bounds;
        bool bar = false;
        if (!member_bounds(floor, foot_.sub, bounds, bar)) return false;
        AABox local;
        if (!member_local_bounds(floor, foot_.sub, local)) return false;
        if ((local.mMin - foot_.member_local.mMin).Length() > 0.02F ||
            (local.mMax - foot_.member_local.mMax).Length() > 0.02F) return false;
        anchor = floor.GetCenterOfMassTransform() * foot_.local;
        bounds.ExpandBy(Vec3::sReplicate(0.04F));
        if (!bounds.Contains(anchor)) return false;
    }
    const RVec3 c = center();
    if (horizontal(c - anchor) > limits_.radius) return false;
    // A leg has its own reach, not the lower reach of a handhold. Query
    // outside the body lock and require the original member to remain the
    // first real surface under that planted foot.
    const float reach = limits_.center_above_feet() + limits_.leg_travel_m;
    const RRayCast ray(RVec3(anchor.GetX(), c.GetY(), anchor.GetZ()), Vec3(0, -reach, 0));
    RayCastResult hit;
    const SteelFilter filter(ignore_);
    if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter) ||
        hit.mBodyID != foot_.body || hit.mSubShapeID2 != foot_.sub) return false;
    at = ray.GetPointOnRay(hit.mFraction);
    if ((at - anchor).Length() > 0.04F) return false;
    BodyLockRead lock(physics_.GetBodyLockInterface(), foot_.body);
    return lock.Succeeded() && lock.GetBody().GetWorldSpaceSurfaceNormal(foot_.sub, at).GetY() >= 0.7F;
}

void Climber::release_leg() {
    foot_ = Hold{};
    view_.leg_driving = false;
}

bool Climber::drive_leg(const float dt, Budget &budget) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 c = center();
    const auto toe_off = [&]() {
        const bool pushed = leg_push_ticks_ > 0;
        release_leg();
        if (!pushed) fail(Failure::NotSupported);
        return pushed;
    };
    if (dt <= 0.0F || phase_s_ > 1.0F) {
        fail(Failure::Stalled);
        return false;
    }
    if (!planted_leg_contact(foot_world_)) {
        return toe_off();
    }
    float inverse_mass = 1.0F / limits_.mass_kg;
    float gravity_relative = -kGravity;
    {
        BodyLockRead lock(physics_.GetBodyLockInterface(), foot_.body);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(foot_.sub, foot_world_).GetY() < 0.7F) {
            return toe_off();
        }
        const Body &floor = lock.GetBody();
        if (floor.IsDynamic()) {
            // The opposite push accelerates a free tread too, including its
            // rotation about the contact. Account for both bodies when
            // bounding extension and actuator power on that moving support.
            const Vec3 arm = foot_world_ - floor.GetCenterOfMassPosition();
            const Vec3 moment = arm.Cross(Vec3::sAxisY());
            inverse_mass += floor.GetMotionProperties()->GetInverseMass() + moment.Dot(floor.GetInverseInertia() * moment);
            gravity_relative += kGravity * floor.GetMotionProperties()->GetGravityFactor();
        }
    }
    const float extension = float(c.GetY() - foot_world_.GetY()) - leg_start_m_;
    view_.leg_travel_m = std::max(extension, 0.0F);
    const float remaining = std::min(limits_.leg_travel_m - view_.leg_travel_m,
                                    limits_.center_above_feet() + limits_.leg_travel_m - float(c.GetY() - foot_world_.GetY()));
    const Vec3 support_v = bodies.GetPointVelocity(foot_.body, foot_world_);
    budget.velocity = velocity() - support_v;
    const float v = budget.velocity.GetY();
    const float rise = std::max(leg_target_y_ - float(c.GetY()) - limits_.hand_above_center, 0.0F);
    if (remaining <= 0.0F || extension < -limits_.leg_travel_m ||
        (leg_push_ticks_ > 0 && velocity().GetY() >= std::sqrt(2.0F * kGravity * rise)) || v >= limits_.launch_dv_mps - 0.001F) {
        return toe_off();
    }
    // Bound the next semi-implicit integration step, not a position corrected
    // after it. Leave the foot before even coasting would overextend the leg.
    const float next_without_push = v + gravity_relative * dt;
    const float next_cap = std::min(limits_.launch_dv_mps, remaining / dt);
    if (next_without_push >= next_cap) {
        return toe_off();
    }
    float force_n = std::min(budget.left, (next_cap - next_without_push) / (inverse_mass * dt));
    // Positive work is capped at the end of this accelerating tick as well
    // as at its beginning. F * (v + (F/m - g) dt) <= P.
    const float q = dt * inverse_mass;
    const float power = std::max(budget.power_left, 0.0F);
    const float power_force = (-next_without_push + std::sqrt(next_without_push * next_without_push + 4.0F * q * power)) / (2.0F * q);
    force_n = std::min(force_n, power_force);
    const Vec3 push = budget.take(Vec3(0, std::max(force_n, 0.0F), 0));
    bodies.AddForce(body_, push);
    if (push.GetY() > 0.0F) ++leg_push_ticks_;
    if (bodies.GetMotionType(foot_.body) == EMotionType::Dynamic) {
        bodies.AddForce(foot_.body, -push, foot_world_);
        view_.support_reaction_n += push.Length();
    }
    return false;
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

bool Climber::floor_under(const RVec3 c, const Vec3 off) const {
    const SteelFilter filter(ignore_);
    const float reach = limits_.center_above_feet() + limits_.step_max_m + 0.05F;   // a step down is floor too
    RayCastResult hit;
    return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(c + off, Vec3(0.0F, -reach, 0.0F)), hit, Layers(physics_, ignore_).bp(),
                                                  Layers(physics_, ignore_).ol(), filter);
}

// No floor straight under her middle but some under her soles to one side
// (a board, a narrow ledge): the mean way to that floor.
bool Climber::narrow_floor_lean(const RVec3 c, Vec3 &to_floor) const {
    if (floor_under(c, Vec3::sZero())) return false;
    Vec3 lean = Vec3::sZero();
    int hits = 0;
    for (int a = 0; a < 8; ++a) {
        const float ang = 0.785398F * float(a);
        for (const float r : {0.08F, 0.16F, 0.24F}) {
            const Vec3 off(r * std::cos(ang), 0.0F, r * std::sin(ang));
            if (floor_under(c, off)) {
                lean += off;
                ++hits;
            }
        }
    }
    if (hits == 0) return false;
    to_floor = lean / float(hits);
    return true;
}

// Standing still on a narrow floor with nothing else asked of her: her feet
// keep her middle over it and take out her drift, inside the same caps (a
// hard enough shove still puts her off). On wide floor it does nothing.
float Climber::stand(const float dt) {
    (void)dt;
    Budget budget{limits_.total_force_n, 0.0F, limits_.power_w, 0.0F, relative_velocity()};
    const RVec3 c = center();
    Vec3 to_floor;
    bool narrow = narrow_floor_lean(c, to_floor);
    if (!narrow && floor_under(c, Vec3::sZero())) {
        // Floor under her middle but not all round her soles (near a
        // board's edge): her middle back toward the floor's middle.
        Vec3 sum = Vec3::sZero();
        int hits = 1, probes = 1;
        for (int a = 0; a < 8; ++a) {
            const float ang = 0.785398F * float(a);
            for (const float r : {0.08F, 0.16F, 0.24F}) {
                const Vec3 off(r * std::cos(ang), 0.0F, r * std::sin(ang));
                ++probes;
                if (floor_under(c, off)) {
                    sum += off;
                    ++hits;
                }
            }
        }
        if (hits < probes) {
            to_floor = sum / float(hits);
            narrow = true;
        }
    }
    if (narrow) {
        const Vec3 v = flat(velocity());
        const Vec3 across = to_floor.NormalizedOr(Vec3::sZero());
        Vec3 want = to_floor * (limits_.mass_kg * 40.0F) - v * (limits_.mass_kg * 8.0F) -
                    across * (v.Dot(across) * limits_.mass_kg * 10.0F);
        if (want.Length() > limits_.foot_force_n) want = want.Normalized() * limits_.foot_force_n;
        physics_.GetBodyInterface().AddForce(body_, budget.take(want));
    } else if (floor_under(c, Vec3::sZero())) {
        // Floor under her middle: feet planted, her drift taken out.
        Vec3 want = -flat(velocity()) * (limits_.mass_kg * 8.0F);
        if (want.Length() > limits_.foot_force_n) want = want.Normalized() * limits_.foot_force_n;
        physics_.GetBodyInterface().AddForce(body_, budget.take(want));
    }
    view_.force_n = budget.used;
    view_.peak_force_n = std::max(view_.peak_force_n, budget.used);
    return budget.used;
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
        const auto floor_ahead = [&](const Vec3 dir, const float ahead = -1.0F) {
            if (dir.LengthSq() < 1.0e-4F) return true;
            const RVec3 from = c + dir.Normalized() * (ahead > 0.0F ? ahead : limits_.radius + 0.1F) +
                               Vec3(0.0F, limits_.step_max_m + 0.05F - limits_.center_above_feet(), 0.0F);
            const float below = std::max(limits_.step_max_m, may_drop);
            const float depth = limits_.step_max_m + below + 0.1F;
            RayCastResult hit;
            return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter) &&
                   float(from.GetY()) - hit.mFraction * depth > feet_y - below;
        };
        // Do not look past a close goal: floor_ahead defaults to a
        // radius-plus stride ahead, which overshoots a leg a quarter
        // metre away on a small step and reports no floor, planting
        // her feet even though the leg itself is on floor. Check
        // floor at the goal when the goal is closer than a stride.
        const float ahead_goal = std::min(limits_.radius + 0.1F, d);
        if (!floor_ahead(want_v, ahead_goal) || (v.Length() > 0.3F && !floor_ahead(v))) {
            want_dir_v = floor_ahead(want_v, ahead_goal) ? want_v * 0.3F : Vec3::sZero();
            if (want_dir_v.IsNearZero() && !want_v.IsNearZero()) {
                // No floor straight on (a corner cut over a gap): along the
                // edge instead, the nearest turn with floor that still gains.
                for (const float turn : {0.5F, -0.5F, 1.0F, -1.0F, 1.4F, -1.4F}) {
                    const float cs = std::cos(turn), sn = std::sin(turn);
                    const Vec3 dir(want_v.GetX() * cs - want_v.GetZ() * sn, 0.0F, want_v.GetX() * sn + want_v.GetZ() * cs);
                    if (floor_ahead(dir)) {
                        want_dir_v = dir * (0.5F * cs);
                        break;
                    }
                }
                // Nothing a stride on (a corner where one narrow top meets
                // another): a short step, her foot just past her own edge.
                if (want_dir_v.IsNearZero() && floor_ahead(want_v, 0.5F * limits_.radius)) {
                    want_dir_v = want_v.Normalized() * 0.25F;
                }
            }
        }
    }
    Vec3 want = (want_dir_v - v) * (limits_.mass_kg * 8.0F);
    // Balance on a narrow floor (a board, a ledge): no floor straight under
    // her middle but some under her soles to one side, she keeps her middle
    // over it (against a wall beside it, the wall takes the push), slowed to
    // a shuffle. Her feet's force is capped like any other: pushed harder
    // than that across it, she goes off it.
    {
        Vec3 to_floor;
        // Stepping across to floor her goal lies on (a board's end over a
        // drop, the next plank) is a step, not falling off: no balancing back.
        // Nor is stepping down off it on purpose (a drop the move allows).
        const bool stepping_on =
            d > 0.05F && (may_drop > limits_.step_max_m || floor_under(c, to * (std::min(d, 0.3F) / d)));
        if (!stepping_on && narrow_floor_lean(c, to_floor)) {
            const Vec3 across = to_floor.NormalizedOr(Vec3::sZero());
            const Vec3 along = want_dir_v - across * want_dir_v.Dot(across);
            want = ((along.Length() > 0.5F ? along.Normalized() * 0.5F : along) - v) * (limits_.mass_kg * 8.0F) +
                   to_floor * (limits_.mass_kg * 40.0F) - across * (v.Dot(across) * limits_.mass_kg * 10.0F);
        }
    }
    if (want.Length() > limits_.foot_force_n) {
        want = want.Normalized() * limits_.foot_force_n;
    }
    const Vec3 applied = budget.take(want);
    physics_.GetBodyInterface().AddForce(body_, applied);
    // A stair or plate edge within her stride in front of her: her leg lifts
    // her onto it, a capped push out of the same budget.
    if (d > 0.05F) {
        const Vec3 dir = to / d;
        const float feet = float(c.GetY()) - limits_.center_above_feet();
        const SteelFilter filter(ignore_);
        float rise = -1.0F;
        // The nearest edge ahead is the one her foot goes on (stairs taken
        // slantwise put the one after it within the same stride).
        for (const float ahead : {0.36F, 0.5F}) {
            const RVec3 from = c + dir * ahead + Vec3(0.0F, limits_.step_max_m + 0.05F - limits_.center_above_feet(), 0.0F);
            const float depth = limits_.step_max_m + 0.1F;
            RayCastResult hit;
            const bool inside_step = physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), hit, Layers(physics_, ignore_).bp(), Layers(physics_, ignore_).ol(), filter);
            const float r = inside_step ? float(from.GetY()) - hit.mFraction * depth - feet : -1.0F;
            if (inside_step && hit.mFraction <= 0.0F) break;   // ray starts inside more than a step: beyond is no step
            rise = std::max(rise, r);
            if (rise > 0.04F) break;
        }
        // Only from a footing under her soles: a leg cannot push off air
        // (her middle out past an edge, falling).
        Vec3 unused;
        if (rise > 0.04F && rise <= limits_.step_max_m && (floor_under(c, Vec3::sZero()) || narrow_floor_lean(c, unused))) {
            const float lift = std::clamp(limits_.mass_kg * (kGravity + (1.6F - velocity().GetY()) * 12.0F), 0.0F,
                                          limits_.total_force_n * 0.85F);
            physics_.GetBodyInterface().AddForce(body_, budget.take(Vec3(0.0F, lift, 0.0F)));
        }
    }
}

float Climber::walk_between(const float dt, const Vec3 feet_goal, const float speed, const float may_drop) {
    Budget budget{limits_.total_force_n, 0.0F, limits_.power_w, 0.0F, relative_velocity()};
    walk(dt, feet_goal, speed, budget, may_drop);
    view_.force_n = budget.used;
    view_.power_w = budget.power_used;
    view_.peak_power_w = std::max(view_.peak_power_w, budget.power_used);
    view_.peak_brake_w = std::max(view_.peak_brake_w, budget.brake_used);
    view_.peak_force_n = std::max(view_.peak_force_n, budget.used);
    return budget.used;
}

void Climber::begin(const MoveSpec &spec) {
    release_leg();
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
    dyno_ = 0;
    dyno_tries_ = 0;
    view_.active = true;
    view_.kind = spec.kind;
    view_.last_failure = Failure::None;
}

void Climber::cancel() {
    release_leg();
    if (hold_.valid()) {
        ++view_.releases;
    }
    hold_ = Hold{};
    status_ = Status::Idle;
    view_.active = false;
    view_.holding = false;
}

void Climber::fail(const Failure failure) {
    release_leg();
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
    release_leg();
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
        // A move done (or none begun) and nothing else asked: she stands,
        // balanced on whatever narrow floor she is on.
        if (status_ == Status::Done || status_ == Status::Idle) (void)stand(dt);
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
    case MoveKind::GapJump: step_gap(dt, budget); break;
    }
    view_.force_n = budget.used;
    view_.power_w = budget.power_used;
    view_.peak_power_w = std::max(view_.peak_power_w, budget.power_used);
    view_.peak_brake_w = std::max(view_.peak_brake_w, budget.brake_used);
    view_.peak_force_n = std::max(view_.peak_force_n, budget.used);
    view_.phase = phase_;
    view_.sliding = sliding_;
    view_.holding = hold_.valid();
    view_.hand = hold_world_;
    view_.airborne = !foot_.valid() && !hold_.valid() && !supported();
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
    if (dyno_ != 0) {
        return step_dyno(dt, budget);
    }
    if (!revalidate(hold_, hold_world_)) {
        fail(Failure::LostHold);
        return status_;
    }
    // The next rung up as soon as one is within reach over the present one.
    Hold next;
    RVec3 next_world;
    const float hold_y = float(hold_world_.GetY());
    const float top_reach = float(c.GetY()) + limits_.hand_above_center;
    bool took = false;
    if (hold_y - float(c.GetY()) < 0.95F && top_reach > hold_y + 0.2F && hold_y < spec_.top_y - 0.05F &&
        find_rung(hold_y + 0.12F, std::min(top_reach, hold_y + 0.65F), true, next, next_world)) {
        hold_ = next;
        hold_world_ = next_world;
        ++view_.grips;
        took = true;
    }
    if (!took && hold_y < spec_.top_y - 0.05F && float(c.GetY()) >= hold_y - 0.35F - 0.25F &&
        !find_rung(hold_y + 0.12F, hold_y + 0.65F, true, next, next_world)) {
        // A gap in the rungs over her: her other hand up as far as it goes
        // (a long reach), else a throw for the rung past it.
        if (find_rung(hold_y + 0.65F, top_reach, true, next, next_world)) {
            hold_ = next;
            hold_world_ = next_world;
            ++view_.grips;
        } else if (find_rung(top_reach, top_reach + limits_.dyno_rise_m, true, next, next_world) && dyno_tries_ < 2) {
            dyno_hold_ = next;
            dyno_world_ = next_world;
            dyno_ = 1;
            phase_s_ = 0.0F;
            return step_dyno(dt, budget);
        }
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
    const bool on_rungs = spec_.kind == MoveKind::RungUp || spec_.kind == MoveKind::JumpCatch;
    const Vec3 in = on_rungs ? -spec_.out : flat(spec_.goal - lip_world_).NormalizedOr(-spec_.out);
    const float top = float(lip_world_.GetY());
    RVec3 over = lip_world_ + in * (limits_.radius + 0.08F) + Vec3(0.0F, limits_.center_above_feet() + 0.06F, 0.0F);
    RVec3 stand = RVec3(spec_.goal.GetX(), top + limits_.center_above_feet() + 0.04F, spec_.goal.GetZ());
    if (!capsule_free(stand) || !capsule_free(over)) {
        // A narrow ledge (a shelf on a wall): no room to stand square on it,
        // so she stands leaning off the wall, her middle back toward the lip
        // and out past it no farther than her soles still bear on it.
        for (float k = limits_.radius + 0.04F; k >= -(limits_.radius - 0.05F); k -= 0.04F) {
            const RVec3 lean = lip_world_ + in * k + Vec3(0.0F, limits_.center_above_feet() + 0.06F, 0.0F);
            if (capsule_free(lean)) {
                over = lean;
                stand = RVec3(lean.GetX(), top + limits_.center_above_feet() + 0.04F, lean.GetZ());
                spec_.goal = Vec3(float(lean.GetX()), top, float(lean.GetZ()));
                break;
            }
        }
    }
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
    {
        // The lip moves with what it is part of (a deck on a hoist, a boom):
        // where she is going moves with it.
        const Vec3 moved = Vec3(hold_world_ - lip_world_);
        if (moved.LengthSq() > 1.0e-8F) {
            lip_world_ = hold_world_;
            spec_.goal += moved;
            spec_.goal_top += moved.GetY();
        }
    }
    const float top = float(lip_world_.GetY());
    const bool on_rungs = spec_.kind == MoveKind::RungUp || spec_.kind == MoveKind::JumpCatch;
    const Vec3 in = on_rungs ? -spec_.out : flat(spec_.goal - lip_world_).NormalizedOr(-spec_.out);
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    const float over_y = top + limits_.center_above_feet() + 0.08F;
    float progress = 0.0F;
    if (feet < top + 0.04F) {
        // Straight up on the climb line where she hangs if that is clear of the
        // lip's face, else just off it.
        const float off = horizontal(c - lip_world_);
        // A rung column is pulled onto the rung line, so a split patch beside
        // the rail cannot walk her off the ladder. Any other lip (a narrow
        // shelf) rises where she already hangs when that spot is clear.
        Vec3 beside;
        if (on_rungs) {
            const float hang_off = (off >= limits_.radius + 0.03F && off <= limits_.reach_out)
                                       ? std::clamp((Vec3(c - lip_world_)).Dot(-in), limits_.radius + 0.03F, limits_.reach_out)
                                       : (limits_.radius + 0.06F);
            beside = Vec3(lip_world_) - in * hang_off;
        } else {
            beside = (off >= limits_.radius + 0.03F && off <= limits_.reach_out)
                         ? Vec3(c)
                         : Vec3(lip_world_) - in * (limits_.radius + 0.06F);
        }
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

// --- a throw across a gap in the rungs ----------------------------------------
// Down to a low hang on the rung she holds, a pull with everything her one
// budget gives (force-capped, power-capped, the reaction into the rung), the
// hand off at a bent arm, and a catch on the rung over the gap if her hands
// get there slowly enough. Short, she falls back past the rung she left and
// tries to catch it; a second miss and she has failed (Missed).
Climber::Status Climber::step_dyno(const float dt, Budget &budget) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 c = center();
    const Vec3 line_pos = Vec3(spec_.line.GetX(), 0.0F, spec_.line.GetZ()) + spec_.out * 0.45F;
    const float hands = float(c.GetY()) + limits_.hand_above_center;
    const float rung = float(dyno_world_.GetY());
    if (dyno_ == 4) {
        if (drive_leg(dt, budget)) {
            if (hold_.valid()) ++view_.releases;
            ++view_.launches;
            hold_ = Hold{};
            dyno_ = 3;
            phase_s_ = 0.0F;
        }
        return status_;
    }
    if (dyno_ == 1 || dyno_ == 2) {
        if (!revalidate(hold_, hold_world_)) {
            dyno_ = 0;
            fail(Failure::LostHold);
            return status_;
        }
        const float hold_y = float(hold_world_.GetY());
        if (dyno_ == 1) {
            // As low as the hang goes, or as low as the floor under her lets her.
            const bool standing = supported(nullptr);
            const float low = standing ? float(c.GetY()) : hold_y - limits_.hang_below_hold + 0.15F;
            drive(dt, Vec3(line_pos.GetX(), low, line_pos.GetZ()), Vec3::sZero(), 30.0F, budget, true, hold_world_);
            if (float(c.GetY()) < low + 0.06F && std::abs(velocity().GetY()) < 0.3F) {
                dyno_ = 2;
                phase_s_ = 0.0F;
            } else if (phase_s_ > 3.0F) {
                dyno_ = 0;
                fail(Failure::Stalled);
            }
            return status_;
        }
        // The pull: straight up, all she has, until her arm is bent or her
        // speed carries her hands to the rung.
        const float v_rel = (velocity() - bodies.GetPointVelocity(hold_.body, hold_world_)).GetY();
        const float need = std::sqrt(2.0F * kGravity * std::max(rung + 0.06F - hands, 0.0F));
        if (supported()) {
            // A support ray can hit a rung just below her sole even when
            // there is no upward-bearing foothold. Only a verified plant
            // drives her legs; otherwise she still has her original grip
            // and makes the pull with her arms.
            if (plant_leg(rung + 0.06F, false)) {
                dyno_ = 4;
                phase_s_ = 0.0F;
                return step_dyno(dt, budget);
            }
        }
        if (v_rel >= need || float(c.GetY()) >= hold_y - 0.35F) {
            ++view_.releases;
            ++view_.launches;
            hold_ = Hold{};
            dyno_ = 3;
            phase_s_ = 0.0F;
            return status_;
        }
        const Vec3 pull = budget.take(Vec3(0.0F, limits_.total_force_n, 0.0F));
        bodies.AddForce(body_, pull);
        push_reaction(pull, hold_world_);
        const Vec3 side = flat(line_pos - Vec3(c)) * 400.0F;
        bodies.AddForce(body_, budget.take(side));
        return status_;
    }
    // In the air: the rung over the gap, or (falling back) the one she left.
    const Vec3 v = velocity();
    const auto catch_at = [&](const float y, const RVec3 at_world) {
        if (std::abs(hands - y) > 0.25F || v.GetY() > 0.8F) return false;
        Hold h;
        RVec3 p;
        if (!take_hold(RVec3(at_world.GetX(), hands, at_world.GetZ()), limits_.catch_radius_m, true, h, &p)) return false;
        if ((v - bodies.GetPointVelocity(h.body, p)).Length() > limits_.catch_arrest_mps) return false;
        hold_ = h;
        hold_world_ = p;
        ++view_.grips;
        ++view_.catches;
        dyno_ = 0;
        target_y_ = float(c.GetY());
        best_progress_ = float(c.GetY());
        stall_s_ = 0.0F;
        return true;
    };
    if (catch_at(rung, dyno_world_)) {
        dyno_tries_ = 0;
        return status_;
    }
    if ((v.GetY() <= 0.0F && hands < rung - 0.3F) || phase_s_ > 2.0F) {
        // Short: past the rung she let go of, her hands snatch at any rung.
        Hold back;
        RVec3 back_world;
        if (find_rung(hands + 0.2F, hands - 0.25F, false, back, back_world) && catch_at(float(back_world.GetY()), back_world)) {
            ++view_.misses;
            ++dyno_tries_;
            return status_;
        }
        if (hands < rung - 1.6F || phase_s_ > 2.0F || supported(nullptr)) {
            ++view_.misses;
            dyno_ = 0;
            fail(Failure::Missed);
        }
    }
    (void)dt;
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
            // The floor's edge where her soles meet it: she may stand a
            // little high on a plate or a lip's kerb, so feel down for it.
            for (const float under : {0.03F, 0.12F, 0.22F}) {
                for (float ahead = 0.0F; ahead < 0.9F && !found; ahead += 0.1F) {
                    found = take_hold(RVec3(c.GetX(), feet - under, c.GetZ()) + toward * ahead, 0.05F, false, hold_, &where);
                }
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
    if (hold_y - float(c.GetY()) > limits_.hand_above_center - 0.2F) {
        Hold next;
        RVec3 next_world;
        if (find_rung(hold_y - 0.12F, hold_y - 0.65F, false, next, next_world)) {
            hold_ = next;
            hold_world_ = next_world;
            ++view_.grips;
        } else if (hold_y - float(c.GetY()) > limits_.hang_below_hold - 0.1F &&
                   find_rung(hold_y - 0.65F, float(c.GetY()) - 0.2F, false, next, next_world)) {
            // A gap in the rungs under her: from a full hang her other hand
            // reaches down past it to her hip.
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
        // Down, her hand on its head where it is (within its length), not in
        // the air over it.
        const RVec3 at(axis.GetX(), up ? float(c.GetY()) + 0.4F : std::min(feet - 0.3F, spec_.top_y - 0.05F), axis.GetZ());
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
        // Down, the head may be as low as she took it from her floor (a step
        // under her feet, and its own grip's width).
        if (hy < float(c.GetY()) - (up ? 0.5F : limits_.center_above_feet() + 0.4F)) {
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
    // Down at its foot, the floor not under her (a pole that stands off its
    // deck's edge): across onto the floor she came for, still holding it.
    const Vec3 aim = !up && feet <= spec_.goal_top + 0.3F ? Vec3(spec_.goal.GetX(), 0.0F, spec_.goal.GetZ()) : hug;
    const Vec3 side = flat(aim - c) * (limits_.mass_kg * 20.0F) - flat(rel) * (limits_.mass_kg * 6.0F);
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

// --- jump across to a lower floor ---------------------------------------------
// All horizontal speed is earned on the run-up. The planted leg supplies
// the short vertical push; once released there is no steering or thrust in
// the air. Only a real landing permits her feet to brake again.
Climber::Status Climber::step_gap(const float dt, Budget &budget) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 c = center();
    const Vec3 v = velocity();
    const float feet = float(c.GetY()) - limits_.center_above_feet();
    float floor_top = 0.0F;
    BodyID floor;
    RVec3 contact;
    bool grounded = supported(&floor_top, &floor, &contact) && std::abs(feet - floor_top) < 0.035F;
    Vec3 support_v = Vec3::sZero();
    if (grounded) {
        support_v = bodies.GetPointVelocity(floor, contact);
        BodyLockRead lock(physics_.GetBodyLockInterface(), floor);
        grounded = lock.Succeeded() && std::abs(v.GetY() - support_v.GetY()) < 0.3F;
    }
    const Vec3 direction = flat(spec_.goal - spec_.line).NormalizedOr(Vec3::sZero());
    const auto traction = [&](const Vec3 wanted_velocity) {
        if (!grounded) return;
        budget.velocity = v - support_v;
        Vec3 want = (wanted_velocity - flat(budget.velocity)) * (limits_.mass_kg * 16.0F);
        if (want.Length() > limits_.foot_force_n) want = want.Normalized() * limits_.foot_force_n;
        const Vec3 force = budget.take(want);
        bodies.AddForce(body_, force);
        if (bodies.GetMotionType(floor) == EMotionType::Dynamic) {
            bodies.AddForce(floor, -force, contact);
            view_.support_reaction_n += force.Length();
        }
    };
    if (phase_ == 0) {
        if (!grounded) {
            if (phase_s_ > 1.0F) fail(Failure::NotSupported);
            return status_;
        }
        if (horizontal(spec_.start - c) > 0.10F || flat(v - support_v).Length() > 0.3F) {
            if (phase_s_ > 10.0F) { fail(Failure::Stalled); return status_; }
            const Vec3 to = flat(spec_.start - c);
            traction(to.NormalizedOr(Vec3::sZero()) * std::min(1.2F, to.Length() * 3.0F));
            return status_;
        }
        const GapFlight flight = gap_flight(limits_, spec_.line.GetY() - spec_.goal_top);
        gap_speed_mps_ = flight.seconds > 0.0F ? horizontal(spec_.goal - spec_.line) / flight.seconds : 0.0F;
        if (gap_speed_mps_ <= 0.0F || gap_speed_mps_ > kGapRunMps ||
            gap_speed_mps_ * flight.drive_seconds > limits_.radius - 0.02F) {
            fail(Failure::OutOfRange);
            return status_;
        }
        phase_ = 1;
        phase_s_ = 0.0F;
    }
    if (phase_ == 1) {
        if (!grounded) { fail(Failure::NotSupported); return status_; }
        const Vec3 to_takeoff = flat(spec_.line - c);
        const float ahead = to_takeoff.Dot(direction);
        if (ahead > 0.5F * gap_speed_mps_ * dt) {
            if (phase_s_ > 4.0F) { fail(Failure::Stalled); return status_; }
            const Vec3 across = to_takeoff - direction * ahead;
            traction(direction * gap_speed_mps_ + across * 6.0F);
            return status_;
        }
        if (horizontal(flat(v) - direction * gap_speed_mps_) > 0.25F) {
            fail(Failure::OutOfRange);
            return status_;
        }
        // The scan proposes a nominal arc. Check again from her measured
        // pose and velocity, so an accepted run-up error cannot put her
        // body outside the corridor that was actually checked.
        const Vec3 from(c.GetX(), feet, c.GetZ());
        const GapFlight flight = gap_flight(limits_, feet - spec_.goal_top, v.GetY());
        Vec3 previous = from;
        bool clear = flight.seconds > 0.0F && flat(v).Length() * flight.drive_seconds <= limits_.radius - 0.02F;
        for (const GapSample &sample : flight.samples) {
            if (!clear) break;
            const Vec3 next = from + flat(v) * sample.seconds + Vec3(0, sample.height, 0);
            clear = gap_segment_free(physics_, ignore_, limits_, previous, next);
            previous = next;
        }
        const float acceleration = std::min(limits_.foot_force_n, limits_.total_force_n) / limits_.mass_kg;
        const Vec3 runout = previous + flat(v).NormalizedOr(Vec3::sZero()) *
                           (flat(v).LengthSq() / (2.0F * std::max(acceleration, 0.01F)) + 0.15F);
        if (!clear || !gap_floor_at(physics_, ignore_, previous) || !gap_floor_at(physics_, ignore_, runout) ||
            !gap_segment_free(physics_, ignore_, limits_, previous, runout)) {
            fail(Failure::BlockedSweep);
            return status_;
        }
        if (!plant_leg(float(c.GetY()) + limits_.hand_above_center + kGapHopM)) return status_;
        phase_ = 2;
        phase_s_ = 0.0F;
    }
    if (phase_ == 2) {
        if (drive_leg(dt, budget)) {
            ++view_.launches;
            phase_ = 3;
            phase_s_ = 0.0F;
        }
        return status_;
    }
    if (phase_ == 3) {
        if (grounded && phase_s_ > 0.1F) {
            if (std::abs(floor_top - spec_.goal_top) > 0.08F) { fail(Failure::NotSupported); return status_; }
            phase_ = 4;
            phase_s_ = 0.0F;
        } else {
            if (feet < spec_.goal_top - 0.3F || phase_s_ > 2.0F) { ++view_.misses; fail(Failure::Missed); }
            return status_;
        }
    }
    if (!grounded) { fail(Failure::NotSupported); return status_; }
    const Vec3 to = flat(spec_.goal - c);
    traction(to.NormalizedOr(Vec3::sZero()) * std::min(1.2F, to.Length() * 3.0F));
    if (to.Length() < 0.25F && flat(v - support_v).Length() < 0.2F) finish();
    else if (phase_s_ > 4.0F) fail(Failure::Stalled);
    return status_;
}

// --- jump to a hold ------------------------------------------------------------
// Phases: 0 to the take-off, 1 set, 2 leg drive, 3 in the air; caught, she goes on up it
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
        if (!plant_leg(spec_.bottom_y + 0.08F)) return status_;
        phase_ = 2;
        phase_s_ = 0.0F;
    }
    if (phase_ == 2) {
        if (drive_leg(dt, budget)) {
            ++view_.launches;
            phase_ = 3;
            phase_s_ = 0.0F;
        }
        return status_;
    }
    const Vec3 v = velocity();
    const RVec3 hands = c + Vec3(0.0F, limits_.hand_above_center, 0.0F);
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
            // Felt for at her soles, and (her feet a little up or down on it)
            // at the top her scan read for where she stands.
            RVec3 probe = RVec3(spec_.start.GetX(), feet - 0.03F, spec_.start.GetZ());
            bool found = false;
            for (const float y : {feet - 0.03F, spec_.start.GetY() - 0.03F}) {
                probe = RVec3(spec_.start.GetX(), y, spec_.start.GetZ());
                for (float ahead = 0.0F; ahead < 0.9F && !found; ahead += 0.1F) {
                    found = take_hold(probe + out * ahead, 0.05F, false, lip, &at);
                }
                if (found) {
                    break;
                }
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
            // No room to hang off the face there: the spot under the lip her
            // scan found free to stand on (its goal), if her hands reach it.
            if (!capsule_free(hang_at)) {
                const RVec3 below = RVec3(spec_.goal) + Vec3(0.0F, limits_.center_above_feet() + 0.05F, 0.0F);
                if (Vec3(below - at).Length() < limits_.hang_below_hold + limits_.radius + 0.6F && capsule_free(below)) {
                    hang_at = below;
                }
            }
            if (!capsule_free(hang_at)) {
                fail(Failure::BlockedSweep);
                return status_;
            }
            hold_ = lip;
            hold_world_ = at;
            hang_off_ = Vec3(hang_at - at);
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
        // Where she chose to hang, kept on the hold (it may move).
        RVec3 hang_at = hold_world_ + hang_off_;
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
        if (leaf.axis_aligned && std::min(sx, sz) >= 0.18F) {   // a board or ledge down to 0.18 m: her soles fit
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
    // A ladder's stile is no pole: an upright that two or more bars end at
    // along its height is the side of a ladder, climbed by its rungs, not
    // gripped round. (Geometry alone: any upright with rungs off it.)
    poles_.erase(std::remove_if(poles_.begin(), poles_.end(),
                                [&](const Pole &pole) {
                                    int ends = 0;
                                    for (const Bar &bar : bars) {
                                        const float y = bar.c.GetY();
                                        if (y < pole.bottom - 0.05F || y > pole.top + 0.05F) {
                                            continue;
                                        }
                                        const Vec3 along = bar.along_x ? Vec3::sAxisX() : Vec3::sAxisZ();
                                        for (const float side : {-1.0F, 1.0F}) {
                                            const Vec3 end = bar.c + along * (side * bar.half);
                                            if (std::hypot(end.GetX() - pole.x, end.GetZ() - pole.z) <=
                                                0.5F * pole.section + 0.12F) {
                                                ++ends;
                                            }
                                        }
                                    }
                                    return ends >= 2;
                                }),
                 poles_.end());
    stats_.patches = static_cast<std::uint32_t>(patches_.size());
    stats_.bars = static_cast<std::uint32_t>(bars.size());
    stats_.poles = static_cast<std::uint32_t>(poles_.size());

    // 3a. What stands on a top splits it (split_patches).
    split_patches(physics, ignore, limits);
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
        // Still no room: a top with something standing on part of it (a
        // wall across a step, the next step over its back) is walkable in
        // any gap her capsule fits, so sweep across it in 5 cm steps.
        for (int axis = 0; axis < 2 && !ok; ++axis) {
            const float half = 0.5F * (axis == 0 ? lx : lz);
            for (float g = 0.05F; !ok && g < half + limits.radius - 0.05F; g += 0.05F) {   // out to leaning off its edge, her soles still on it
                for (const float side : {-1.0F, 1.0F}) {
                    const Vec3 s = axis == 0 ? Vec3(cx + side * g, p.top, cz) : Vec3(cx, p.top, cz + side * g);
                    if (capsule_free(physics, ignore, limits, s)) {
                        ok = true;
                        break;
                    }
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
            // At a climbing pitch, or across a gap of missing rungs no wider
            // than her long reach and a throw cross (the climb checks it).
            const float widest = limits.hand_above_center - 0.35F + limits.dyno_rise_m;
            if (dy >= 0.18F && (dy <= 0.46F || (dy <= widest && chain.size() >= 1))) {
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

    build_regions(physics, ignore, limits);
    build_edges(physics, ignore, limits);
    stats_.regions = static_cast<std::uint32_t>(regions_.size());
    stats_.edges = static_cast<std::uint32_t>(edges_.size());
    for (const Edge &e : edges_) stats_.usable_edges += e.usable ? 1U : 0U;
    stats_.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

// 3b. A top with something standing on it taller than a step (a wall across
// a deck, a block on it) is not one floor: her feet cannot cross it. Each
// such top is read in 0.25 m cells, a cell her floor where nothing above a
// step stands on it, and split into the rectangles of floor cells that
// touch each other. A curb within a step leaves it whole.
void AffordanceMap::split_patches(const PhysicsSystem &physics, const Ignore &ignore, const Limits &limits) {
    // Half the thinnest wall or board she must see (0.2 m): any such feature
    // covers at least one cell centre, wherever it stands on the grid.
    constexpr float kCell = 0.10F;
    constexpr float kTall = 3.0F;   // what stands on it rises within this of its top
    const FixedFilter filter(ignore);
    std::vector<Patch> out;
    out.reserve(patches_.size());
    for (std::size_t i = 0; i < patches_.size(); ++i) {
        const Patch &p = patches_[i];
        const float lx = p.max_x - p.min_x, lz = p.max_z - p.min_z;
        // Only a top something might stand on: another top over its plan,
        // above a step and within kTall of it.
        bool candidate = false;
        for (std::size_t j = 0; j < patches_.size() && !candidate; ++j) {
            const Patch &q = patches_[j];
            candidate = j != i && q.top > p.top + limits.step_max_m && q.top < p.top + kTall && q.max_x > p.min_x &&
                        q.min_x < p.max_x && q.max_z > p.min_z && q.min_z < p.max_z;
        }
        const int nx = static_cast<int>(std::ceil(lx / kCell));
        const int nz = static_cast<int>(std::ceil(lz / kCell));
        if (!candidate || nx < 2 || nz < 2 || nx * nz > 400000) {
            out.push_back(p);
            continue;
        }
        std::vector<std::uint8_t> floor(static_cast<std::size_t>(nx * nz), 0);
        // From over her head: a beam or pipe she cannot pass under parts the
        // floor as surely as a wall does.
        const float from_y = p.top + 2.0F * limits.center_above_feet() + 0.05F;
        for (int ix = 0; ix < nx; ++ix)
            for (int iz = 0; iz < nz; ++iz) {
                const float x = std::min(p.min_x + (float(ix) + 0.5F) * kCell, p.max_x - 0.01F);
                const float z = std::min(p.min_z + (float(iz) + 0.5F) * kCell, p.max_z - 0.01F);
                const float depth = from_y - p.top + 0.1F;
                RayCastResult hit;
                const bool any = physics.GetNarrowPhaseQuery().CastRay(RRayCast(RVec3(x, from_y, z), Vec3(0.0F, -depth, 0.0F)), hit,
                                                                       Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), filter);
                const float h = any ? from_y - hit.mFraction * depth : p.top;
                floor[static_cast<std::size_t>(ix * nz + iz)] = h <= p.top + limits.step_max_m ? 1 : 0;
            }
        // Connected floor cells (4-way), each component cut into rectangles
        // greedily (runs along z, grown along x while they match).
        std::vector<int> comp(floor.size(), -1);
        int comps = 0;
        std::vector<int> stack;
        for (int c0 = 0; c0 < nx * nz; ++c0) {
            if (!floor[static_cast<std::size_t>(c0)] || comp[static_cast<std::size_t>(c0)] >= 0) continue;
            stack.assign(1, c0);
            comp[static_cast<std::size_t>(c0)] = comps;
            while (!stack.empty()) {
                const int c = stack.back();
                stack.pop_back();
                const int cx = c / nz, cz = c % nz;
                const int nb[4][2] = {{cx + 1, cz}, {cx - 1, cz}, {cx, cz + 1}, {cx, cz - 1}};
                for (const auto &n : nb) {
                    if (n[0] < 0 || n[0] >= nx || n[1] < 0 || n[1] >= nz) continue;
                    const int k = n[0] * nz + n[1];
                    if (floor[static_cast<std::size_t>(k)] && comp[static_cast<std::size_t>(k)] < 0) {
                        comp[static_cast<std::size_t>(k)] = comps;
                        stack.push_back(k);
                    }
                }
            }
            ++comps;
        }
        if (comps <= 1) {
            // One floor (perhaps with a block on part of it): left whole.
            out.push_back(p);
            continue;
        }
        std::vector<std::uint8_t> used(floor.size(), 0);
        for (int ix = 0; ix < nx; ++ix)
            for (int iz = 0; iz < nz; ++iz) {
                const std::size_t c = static_cast<std::size_t>(ix * nz + iz);
                if (!floor[c] || used[c]) continue;
                const int id = comp[c];
                int z1 = iz;
                while (z1 + 1 < nz && floor[static_cast<std::size_t>(ix * nz + z1 + 1)] &&
                       !used[static_cast<std::size_t>(ix * nz + z1 + 1)] && comp[static_cast<std::size_t>(ix * nz + z1 + 1)] == id) ++z1;
                int x1 = ix;
                for (bool grow = true; grow && x1 + 1 < nx;) {
                    for (int z = iz; z <= z1 && grow; ++z) {
                        const std::size_t k = static_cast<std::size_t>((x1 + 1) * nz + z);
                        grow = floor[k] && !used[k] && comp[k] == id;
                    }
                    if (grow) ++x1;
                }
                for (int x = ix; x <= x1; ++x)
                    for (int z = iz; z <= z1; ++z) used[static_cast<std::size_t>(x * nz + z)] = 1;
                Patch q = p;
                q.min_x = p.min_x + float(ix) * kCell;
                q.max_x = std::min(p.max_x, p.min_x + float(x1 + 1) * kCell);
                q.min_z = p.min_z + float(iz) * kCell;
                q.max_z = std::min(p.max_z, p.min_z + float(z1 + 1) * kCell);
                q.narrow = std::min(q.max_x - q.min_x, q.max_z - q.min_z) < 2.0F * limits.radius;
                q.split = id;
                q.split_of = static_cast<int>(i);
                out.push_back(q);
            }
        ++stats_.split_patches;
    }
    patches_ = std::move(out);
}

void AffordanceMap::build_regions(const PhysicsSystem &physics, const Ignore &ignore, const Limits &limits) {
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
    // A narrow top (a board, a wall's top) joins its neighbor only where
    // she can stand across the seam between them: a wall's top that only
    // meets a step at a corner under a ledge is not a way onto it.
    const auto seam_open = [&](const Patch &a, const Patch &b) {
        const float narrow = 0.28F;
        if (std::min(a.max_x - a.min_x, a.max_z - a.min_z) >= narrow && std::min(b.max_x - b.min_x, b.max_z - b.min_z) >= narrow) {
            return true;
        }
        const float m = 0.1F;   // her soles on both
        const float x0 = std::max(a.min_x, b.min_x) - m, x1 = std::min(a.max_x, b.max_x) + m;
        const float z0 = std::max(a.min_z, b.min_z) - m, z1 = std::min(a.max_z, b.max_z) + m;
        const float y = std::max(a.top, b.top);
        for (float x = x0; x <= x1 + 1.0e-4F; x += 0.1F)
            for (float z = z0; z <= z1 + 1.0e-4F; z += 0.1F) {
                if (capsule_free(physics, ignore, limits, Vec3(x, y, z))) return true;
            }
        return false;
    };
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
                        // Pieces of one split top: joined only within their own floor
                        // (touching pieces), never across what split them.
                        const bool same_top = p.split_of >= 0 && p.split_of == q.split_of;
                        if (same_top ? (p.split == q.split && gap_xz(p, q) <= 0.01F)
                                     : (std::abs(q.top - p.top) <= limits.step_max_m && gap_xz(p, q) <= 0.25F && seam_open(p, q))) {
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
                const float speed_cap = leg_speed_cap(limits);
                const bool jump_ok = !jump || (speed_cap > 0.0F && reach + 0.08F <= limits.hand_reach_standing() +
                                               limits.leg_travel_m + speed_cap * speed_cap / (2.0F * kGravity));
                for (const int hi : heads) {
                    const Patch &hp = patches_[static_cast<std::size_t>(hi)];
                    if (hp.region == fp.region || hp.top < fp.top + 1.0F) continue;
                    Vec3 stand_foot{}, stand_head{};
                    // Her foot spot on this side of the rungs: the side the
                    // climb's own corridor is checked on.
                    const bool foot_ok = free_spot(fp, foot_at, stand_foot) && (stand_foot - line).Dot(out) > 0.15F;
                    const bool head_ok = free_spot(hp, line - out * 0.6F, stand_head);
                    const Vec3 lip_in = closest_on(hp, line, limits.radius + 0.1F);
                    // The head's lip within her hands' reach of the top rung's
                    // line (a deck there, not one a stride off it).
                    const bool lip_near = horizontal(closest_on(hp, line, 0.0F) - line) <= 0.55F && hp.top - l.top <= 0.6F;
                    const bool over_ok = lip_near && capsule_free(physics, ignore, limits, lip_in);
                    // The climb itself: her body on the rungs, foot to head,
                    // every half meter (a deck's edge over the ladder stops her).
                    // The rise runs the full way to mantle clearance: standing
                    // centre on the head (its top plus her centre height), not
                    // just to the top rung. A deck over the column stops her
                    // head before she can mantle, so a sample that ends below
                    // it passes a climb she cannot finish. Only the climb
                    // column is sampled: a mantle-rise column built from scan
                    // data was tried three ways (full rise; top-only from
                    // the inset lip point; top-only from the patch edge with
                    // the executor's clamped hang offset) and each falsely
                    // blocked ladders that climb clean -- the executor's lip
                    // search and diagonal rise are dynamic, and no static
                    // column from edge data models them. The head-hit that
                    // blocks these climbs happens in the climb column itself.
                    bool corridor_ok = true;
                    float corridor_block = 0.0F;
                    {
                        const Vec3 on = line + out * 0.42F;
                        const float lo = fp.top + limits.center_above_feet() + 0.05F;
                        const float hi = hp.top + limits.center_above_feet();
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
                    const float align_pen =
                        0.01F * (horizontal(stand_foot - foot_at) + horizontal(stand_head - (line - out * 0.6F)));
                    e.cost = (l.top - l.bottom) / limits.climb_mps + (jump ? 6.0F : 2.0F) + align_pen;
                    e.usable = foot_ok && head_ok && over_ok && corridor_ok && jump_ok;
                    e.reason = facts;
                    char corridor[96];
                    std::snprintf(corridor, sizeof corridor, "; blocked: her body does not fit on the rungs at %.2f",
                                  double(corridor_block));
                    if (!e.usable) {
                        e.reason += !jump_ok ? "; out of range: capped leg drive cannot reach the first rung"
                                    : !foot_ok ? "; blocked: no room to stand at the foot"
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
                    d.cost = (l.top - l.bottom) / limits.climb_mps + 2.0F + align_pen;
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
                // Over the head onto the floor: where the pole's head stands
                // above the lip, her body has to get from beside the pole to
                // her stand on the floor without passing through it or what
                // flanks the way (a rail post): a search over the plane at
                // the lip, her capsule tested at each step.
                bool head_room = true;
                if (pole.top > hp.top + limits.step_max_m && head_ok) {
                    // Her whole width here (no squeeze allowance): a gap
                    // narrower than her body is one she does not pass.
                    Limits body = limits;
                    body.radius += 0.02F;
                    const float y = hp.top + 0.02F;
                    const Vec3 from = axis + out * (limits.radius + 0.1F) + Vec3(0, y, 0);
                    const Vec3 to(stand_head.GetX(), y, stand_head.GetZ());
                    const float step = 0.05F;
                    const float lo_x = std::min(from.GetX(), to.GetX()) - 0.8F, lo_z = std::min(from.GetZ(), to.GetZ()) - 0.8F;
                    const int nx = static_cast<int>((std::max(from.GetX(), to.GetX()) + 0.8F - lo_x) / step) + 1;
                    const int nz = static_cast<int>((std::max(from.GetZ(), to.GetZ()) + 0.8F - lo_z) / step) + 1;
                    const auto at = [&](const int i, const int k) { return Vec3(lo_x + step * float(i), y, lo_z + step * float(k)); };
                    const auto cell = [&](const Vec3 v) {
                        return std::make_pair(std::clamp(static_cast<int>(std::lround((v.GetX() - lo_x) / step)), 0, nx - 1),
                                              std::clamp(static_cast<int>(std::lround((v.GetZ() - lo_z) / step)), 0, nz - 1));
                    };
                    std::vector<std::uint8_t> state(static_cast<std::size_t>(nx * nz), 0);   // 0 new, 1 queued, 2 blocked
                    // Plain ints, not structured bindings: lambdas capture them (C++17).
                    const int si = cell(from).first, sk = cell(from).second;
                    const int gi = cell(to).first, gk = cell(to).second;
                    std::vector<int> queue{sk * nx + si};
                    state[static_cast<std::size_t>(sk * nx + si)] = 1;
                    bool reached = false;
                    for (std::size_t q = 0; q < queue.size() && !reached; ++q) {
                        const int i0 = queue[q] % nx, k0 = queue[q] / nx;
                        for (int dk = -1; dk <= 1 && !reached; ++dk)
                            for (int di = -1; di <= 1; ++di) {
                                const int i = i0 + di, k = k0 + dk;
                                if ((di == 0 && dk == 0) || i < 0 || k < 0 || i >= nx || k >= nz) continue;
                                std::uint8_t &st = state[static_cast<std::size_t>(k * nx + i)];
                                if (st != 0) continue;
                                if (!capsule_free(physics, ignore, body, at(i, k))) { st = 2; continue; }
                                st = 1;
                                if (std::abs(i - gi) <= 1 && std::abs(k - gk) <= 1) { reached = true; break; }
                                queue.push_back(k * nx + i);
                            }
                    }
                    head_room = reached;
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
                e.usable = foot_ok && head_ok && climb_room && head_room;
                e.reason = facts;
                if (!foot_ok) e.reason += "; blocked: no room at the foot";
                if (!head_ok) e.reason += "; blocked: no room at the head";
                if (!climb_room) e.reason += "; blocked: capsule does not fit along the pole";
                if (!head_room) e.reason += "; blocked: no way for her body past the pole's head onto the floor";
                // Its head where her hands change from it to the floor: no
                // lower than a step's reach under that floor (either way).
                if (pole.top < hp.top - 0.4F) {
                    e.usable = false;
                    e.reason += "; blocked: its head ends too far under the floor";
                }
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
                // Down she steps from the lip out onto it holding its head: no
                // further out than her body's width past the lip (up, her
                // mantle reaches back across a wider gap).
                if (horizontal(lip - axis) > limits.radius + 0.35F) {
                    d.usable = false;
                    d.reason += "; blocked: too far out from the lip to step onto";
                }
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
                        const float gap = gap_xz(lo, hi);
                        if (gap > 1.4F) continue;
                        if (std::find(seen.begin(), seen.end(), j) != seen.end()) continue;
                        seen.push_back(j);
                        const Vec3 lc((lo.min_x + lo.max_x) * 0.5F, lo.top, (lo.min_z + lo.max_z) * 0.5F);
                        Vec3 lip = closest_on(hi, lc, 0.0F);
                        if (gap > 0.6F) {
                            // A short downward jump between stationary broad
                            // tops. Both the run-up and landing run-out must
                            // fit on real steel; a nearby lower floor alone
                            // does not make a usable jump.
                            if (rise > kGapDropM || hi.narrow || lo.narrow ||
                                physics.GetBodyInterface().GetMotionType(hi.body) != EMotionType::Static ||
                                physics.GetBodyInterface().GetMotionType(lo.body) != EMotionType::Static) continue;
                            const Vec3 landing = closest_on(lo, lip, limits.radius + 0.3F);
                            const Vec3 direction = flat(landing - lip).NormalizedOr(Vec3::sZero());
                            const Vec3 takeoff = lip - direction * 0.20F;
                            const GapFlight flight = gap_flight(limits, rise);
                            if (flight.seconds <= 0.0F) continue;
                            const float speed = horizontal(landing - takeoff) / flight.seconds;
                            const float acceleration = std::min(limits.foot_force_n, limits.total_force_n) / limits.mass_kg;
                            if (acceleration <= 0.0F || speed > kGapRunMps ||
                                speed * flight.drive_seconds > limits.radius - 0.02F) continue;
                            const float stopping = speed * speed / (2.0F * acceleration);
                            const Vec3 start = takeoff - direction * (stopping + 0.65F);
                            const Vec3 runout = landing + direction * (stopping + 0.15F);
                            const auto inside = [&](const Patch &p, const Vec3 at) {
                                return at.GetX() >= p.min_x + limits.radius && at.GetX() <= p.max_x - limits.radius &&
                                       at.GetZ() >= p.min_z + limits.radius && at.GetZ() <= p.max_z - limits.radius;
                            };
                            if (!inside(hi, start) || !inside(lo, landing) || !inside(lo, runout)) continue;
                            // Sweep the full body, with 2 cm clearance,
                            // through the run-up, each flight tick and the
                            // braking distance on the destination.
                            const auto clear_segment = [&](const Vec3 from, const Vec3 to) {
                                ++stats_.capsule_queries;
                                return gap_segment_free(physics, ignore, limits, from, to);
                            };
                            bool clear = clear_segment(start, takeoff);
                            Vec3 previous = takeoff;
                            for (const GapSample &sample : flight.samples) {
                                if (!clear) break;
                                const Vec3 next = takeoff + direction * (speed * sample.seconds) + Vec3(0, sample.height, 0);
                                clear = clear_segment(previous, next);
                                previous = next;
                            }
                            clear = clear && clear_segment(landing, runout);
                            Edge jump;
                            jump.from = hi.region;
                            jump.to = lo.region;
                            jump.kind = MoveKind::GapJump;
                            jump.spec.kind = jump.kind;
                            jump.spec.line = takeoff;
                            jump.spec.out = direction;
                            jump.spec.start = start;
                            jump.spec.goal = landing;
                            jump.spec.top_y = hi.top;
                            jump.spec.bottom_y = lo.top;
                            jump.spec.goal_top = lo.top;
                            jump.cost = 3.0F + stopping + flight.seconds;
                            jump.usable = clear;
                            jump.reason = clear ? "supported run-up, capped hop and clear descent to a lower top"
                                                : "blocked: downward jump capsule corridor";
                            add_edge(jump);
                            continue;
                        }
                        Vec3 below = closest_on(lo, lip, limits.radius + 0.1F);
                        Vec3 above = closest_on(hi, lip, limits.radius + 0.1F);
                        bool below_ok = horizontal(below - lip) <= 0.9F && capsule_free(physics, ignore, limits, below);
                        // Over the lip itself: her body at the edge (a rail along it stops her).
                        const auto passage = [&](const Vec3 edge, const Vec3 inward) {
                            return capsule_free(physics, ignore, limits, Vec3(edge.GetX(), hi.top, edge.GetZ()) + inward * 0.05F);
                        };
                        bool above_ok = capsule_free(physics, ignore, limits, above) &&
                                        passage(lip, flat(above - lip).NormalizedOr(Vec3::sZero()));
                        if (!below_ok && above_ok && lo.narrow) {
                            // A narrow low top (a board between the face and a
                            // wall): her body shifted across it, her soles still
                            // on it, toward the face or away.
                            const Vec3 toward = flat(lip - below).NormalizedOr(Vec3::sZero());
                            const float lean = limits.radius - 0.05F;
                            for (float k = 0.05F; k <= 0.5F && !below_ok; k += 0.05F) {
                                for (const float sgn : {1.0F, -1.0F}) {
                                    const Vec3 b = below + toward * (sgn * k);
                                    if (b.GetX() < lo.min_x - lean || b.GetX() > lo.max_x + lean || b.GetZ() < lo.min_z - lean ||
                                        b.GetZ() > lo.max_z + lean || horizontal(b - lip) > 0.9F) continue;
                                    if (capsule_free(physics, ignore, limits, b)) {
                                        below = b;
                                        below_ok = true;
                                        break;
                                    }
                                }
                            }
                        }
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

std::vector<int> AffordanceMap::plan(const int from, const int to, const std::vector<int> &avoid) const {
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
        // A ladder's own route between two regions beats a cheaper shortcut
        // that leaves the ladder's line (a pole beside it, a lip drop off a
        // neighbouring strip): she climbs the ladder she means to climb, on
        // its line, instead of sliding down something a metre to one side
        // and drifting off the line the whole way down. Only a usable rung
        // edge this plan may still take blocks the shortcuts: an avoided
        // or unusable rung edge leaves them open, so a failed rung climb
        // still falls back to the next way. Reachability is untouched: the
        // shortcuts end in the same region the rung edge does. Nor does the
        // preference change what any route costs: the rung edge is charged
        // the cheapest price of the ways it replaces between those two
        // regions. Blocking a cheap shortcut at its full price made every
        // region with a ladder look dearer than it is, so the search went
        // round the ladder's deck altogether (a drop onto a board beside it
        // and off again) - the opposite of climbing the ladder she means to.
        const auto is_rung = [](const MoveKind kind) {
            return kind == MoveKind::RungUp || kind == MoveKind::RungDown || kind == MoveKind::JumpCatch;
        };
        std::vector<int> rung_to;
        std::vector<std::pair<int, float>> cheapest_to;   // per neighbour region: the cheapest usable way there
        for (auto jt = std::lower_bound(edges_.begin(), edges_.end(), r, [](const Edge &e, const int v) { return e.from < v; });
             jt != edges_.end() && jt->from == r; ++jt) {
            if (!jt->usable) continue;
            if (std::find(avoid.begin(), avoid.end(), static_cast<int>(jt - edges_.begin())) != avoid.end()) continue;
            const int t = jt->to;
            const auto ct = std::find_if(cheapest_to.begin(), cheapest_to.end(), [t](const std::pair<int, float> &c) { return c.first == t; });
            if (ct == cheapest_to.end()) cheapest_to.emplace_back(t, jt->cost);
            else ct->second = std::min(ct->second, jt->cost);
            if (is_rung(jt->kind)) rung_to.push_back(t);
        }
        auto it = std::lower_bound(edges_.begin(), edges_.end(), r, [](const Edge &e, const int v) { return e.from < v; });
        for (; it != edges_.end() && it->from == r; ++it) {
            if (!it->usable) continue;
            if (std::find(avoid.begin(), avoid.end(), static_cast<int>(it - edges_.begin())) != avoid.end()) continue;
            const bool to_ladder = std::find(rung_to.begin(), rung_to.end(), it->to) != rung_to.end();
            if (!is_rung(it->kind) && to_ladder) continue;
            float cost = it->cost;
            if (to_ladder) {
                const int t = it->to;
                const auto ct = std::find_if(cheapest_to.begin(), cheapest_to.end(), [t](const std::pair<int, float> &c) { return c.first == t; });
                if (ct != cheapest_to.end()) cost = std::min(cost, ct->second);
            }
            const float nd = d + cost;
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
    // Her body as it walks: her full radius less a hair (a gap narrower than
    // she is wedges her; it is no way for her), her soles a step's
    // rise clear (a step's riser is under her stride, not in her way).
    const float lift = limits.step_max_m;
    const CapsuleShape capsule(limits.half_height - 0.5F * lift, limits.radius - 0.01F);
    // A floor for her feet near a grid point, and where her body stands on
    // it: under the point, or (a narrow board, a ledge) under her soles a
    // little to one side; her body over it, or leaning off a wall beside it
    // as far as her soles still bear on it (radius - 0.05 from the foot).
    const auto stand = [&](const int i, const int k, const float near_y, float &feet, float &bx, float &bz) {
        const float px = x0 + cell * float(i), pz = z0 + cell * float(k);
        const float depth = limits.step_max_m + 1.15F;
        bool found = false;
        float fx = px, fz = pz;
        static constexpr float kFoot[5][2] = {{0, 0}, {0.15F, 0}, {-0.15F, 0}, {0, 0.15F}, {0, -0.15F}};
        for (const auto &o : kFoot) {
            const RVec3 top(px + o[0], near_y + limits.step_max_m + 0.05F, pz + o[1]);
            RayCastResult hit;
            if (!physics.GetNarrowPhaseQuery().CastRay(RRayCast(top, Vec3(0.0F, -depth, 0.0F)), hit, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things)) {
                continue;
            }
            if (hit.mFraction <= 1.0e-4F) {
                return false;   // the point is inside something solid (a wall's body): not a place for her
            }
            feet = float(top.GetY()) - hit.mFraction * depth;
            fx = px + o[0];
            fz = pz + o[1];
            found = true;
            if (o[0] != 0.0F || o[1] != 0.0F) {
                // Found to one side (a board's edge): her foot as far in as the
                // same floor goes, toward its middle, not on its edge.
                const RVec3 deeper(px + 2.0F * o[0], top.GetY(), pz + 2.0F * o[1]);
                RayCastResult h2;
                if (physics.GetNarrowPhaseQuery().CastRay(RRayCast(deeper, Vec3(0.0F, -depth, 0.0F)), h2, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things) &&
                    h2.mFraction > 1.0e-4F && std::abs(float(deeper.GetY()) - h2.mFraction * depth - feet) < 0.05F) {
                    fx = px + 2.0F * o[0];
                    fz = pz + 2.0F * o[1];
                }
            }
            break;
        }
        if (!found) return false;
        {
            // Her foot at the middle of the floor round it (a board's middle,
            // not its edge): the mean of where a step either way still has
            // the same floor.
            float sx = 0.0F, sz = 0.0F;
            int n = 0;
            for (const auto &o : kFoot) {
                const RVec3 top(fx + o[0], feet + limits.step_max_m, fz + o[1]);
                RayCastResult h;
                const float dd = 2.0F * limits.step_max_m;
                if (physics.GetNarrowPhaseQuery().CastRay(RRayCast(top, Vec3(0.0F, -dd, 0.0F)), h, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things) &&
                    h.mFraction > 1.0e-4F && std::abs(float(top.GetY()) - h.mFraction * dd - feet) < 0.05F) {
                    sx += fx + o[0];
                    sz += fz + o[1];
                    ++n;
                }
            }
            if (n > 0) {
                fx = sx / float(n);
                fz = sz / float(n);
            }
        }
        bx = fx;
        bz = fz;
        // At the move's own start the move's approach is the judge (it gets
        // her there by contact, or fails): floor is enough.
        if (std::hypot(px - to.GetX(), pz - to.GetZ()) <= 1.0F) {
            return true;
        }
        const auto body_free = [&](const float x, const float z) {
            CollideShapeSettings settings;
            settings.mActiveEdgeMode = EActiveEdgeMode::CollideOnlyWithActive;
            AnyHitCollisionCollector<CollideShapeCollector> collector;
            const RVec3 c(x, feet + lift + 0.02F + (limits.radius - 0.01F) + (limits.half_height - 0.5F * lift), z);
            physics.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1.0F), RMat44::sTranslation(c), settings,
                                                       RVec3::sZero(), collector, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things);
            return !collector.HadHit();
        };
        if (body_free(fx, fz)) return true;
        const float lean = limits.radius - 0.05F;
        static constexpr float kLean[8][2] = {{0.5F, 0}, {-0.5F, 0}, {0, 0.5F}, {0, -0.5F}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};   // the least lean first
        for (const auto &o : kLean) {
            if (body_free(fx + o[0] * lean, fz + o[1] * lean)) {
                bx = fx + o[0] * lean;
                bz = fz + o[1] * lean;
                return true;
            }
        }
        return false;
    };
    struct Node {
        float g = std::numeric_limits<float>::max();
        float feet = 0.0F;
        float bx = 0.0F, bz = 0.0F;   // where her body stands for this point
        int via = -1;
        bool closed = false;
    };
    std::unordered_map<int, Node> nodes;
    // Plain ints, not structured bindings: the heuristic lambda captures them (C++17).
    const int si = cell_of(from).first, sk = cell_of(from).second;
    const int gi = cell_of(to).first, gk = cell_of(to).second;
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    // A node is a grid point at one level (decks over decks share a point
    // in plan: her feet on each are different places).
    const auto level = [&](const float feet) { return static_cast<int>(std::floor((feet - from.GetY()) / 1.0F + 0.5F)) & 63; };
    const auto key3 = [&](const int i, const int k, const float feet) { return key(i, k) * 64 + level(feet); };
    nodes[key3(si, sk, from.GetY())] = Node{0.0F, from.GetY(), from.GetX(), from.GetZ(), -1, false};
    open.push({0.0F, key3(si, sk, from.GetY())});
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
        const int ci = (n / 64) % nx;
        const int ck = (n / 64) / nx;
        if (h(ci, ck) < nearest_h && std::abs(cur.feet - to.GetY()) <= limits.step_max_m + 0.3F) {
            nearest_h = h(ci, ck);
            nearest = n;
        }
        if (ci == gi && ck == gk && std::abs(cur.feet - to.GetY()) <= limits.step_max_m + 0.3F) {
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
                float feet = 0.0F;
                float bx = 0.0F, bz = 0.0F;
                // Up a step at most; down as far as she steps off a box (a
                // drop under 1 m she just walks off).
                if (!stand(i, k, cur_feet, feet, bx, bz) || feet - cur_feet > limits.step_max_m + 0.08F ||
                    cur_feet - feet > 1.0F) {
                    continue;   // no floor for her there from here (maybe from another side)
                }
                if (feet - cur_feet > 0.04F) {
                    // A step up takes a tread wide enough across her way for
                    // her body (not a stair's end from the side, not its
                    // corner): floor at the same height a hand's width either side.
                    const float dl = std::hypot(float(di), float(dk));
                    const float ax = -float(dk) / dl * 0.2F, az = float(di) / dl * 0.2F;
                    bool tread = true;
                    for (const float side : {1.0F, -1.0F}) {
                        const RVec3 top(bx + side * ax, feet + 0.1F, bz + side * az);
                        RayCastResult hit;
                        tread = tread && physics.GetNarrowPhaseQuery().CastRay(RRayCast(top, Vec3(0.0F, -0.2F, 0.0F)), hit, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things) &&
                                hit.mFraction > 1.0e-4F;
                    }
                    if (!tread) continue;
                }
                const int m = key3(i, k, feet);
                auto it = nodes.find(m);
                if (it != nodes.end() && it->second.closed) continue;
                {
                    // Leaning lets two neighboring cells put her body on two
                    // sides of a thin wall: the step between them must be open
                    // at her hips.
                    const Node &cn = nodes[n];
                    const float hy = std::max(cur_feet, feet) + limits.step_max_m + 0.25F;
                    const RVec3 a(cn.bx, hy, cn.bz);
                    const Vec3 d(bx - cn.bx, 0.0F, bz - cn.bz);
                    RayCastResult hh;
                    if (d.LengthSq() > 1.0e-6F &&
                        physics.GetNarrowPhaseQuery().CastRay(RRayCast(a, d), hh, Layers(physics, ignore).bp(), Layers(physics, ignore).ol(), things)) {
                        continue;
                    }
                }
                const float g = cur_g + cell * ((di != 0 && dk != 0) ? 1.4142F : 1.0F);
                Node &next = nodes[m];
                if (g < next.g) {
                    next.g = g;
                    next.feet = feet;
                    next.bx = bx;
                    next.bz = bz;
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
        path.push_back(Vec3(nodes[n].bx, nodes[n].feet, nodes[n].bz));
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
