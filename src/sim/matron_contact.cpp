// Her contacts as the simulation itself has them, for the presentation: what
// her feet stand on and what her hand holds, this tick (issue #3, S6 "one
// contact truth"). The view used to infer these (a second hand 0.45 m along a
// rung wherever any solid was near, a deck under her feet mid-mantle, a fall
// from how fast she was drawn dropping); it now plants a limb only on what is
// here. Read-only: no force, no velocity, no position is written, and nothing
// in her movement reads these fields back.

#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <array>
#include <cmath>

namespace scraperx::sim {
using namespace JPH;

void Matron::refresh_contact_view() {
    view_.contact_supported = false;
    view_.contact_feet_body = BodyID();
    view_.contact_feet_sub = SubShapeID();
    view_.contact_feet = RVec3::sZero();
    view_.contact_hand_held = false;
    view_.contact_hand_body = BodyID();
    view_.contact_hand_sub = SubShapeID();
    view_.contact_hand = RVec3::sZero();
    view_.contact_hand2_held = false;
    view_.contact_hand2_body = BodyID();
    view_.contact_hand2_sub = SubShapeID();
    view_.contact_hand2 = RVec3::sZero();
    view_.contact_line = on_line_;
    if (!body_in_world_) {
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const Vec3 velocity = bodies.GetLinearVelocity(body_);
    // Her feet: walk_toward's own test of a floor under her, in its order
    // (her middle first, then her feet's span across and fore-aft): the first
    // hit that is a body's floor (not a sensor, its normal at least half up)
    // she is not rising off faster than it rises.
    IgnoreMultipleBodiesFilter filter;
    filter.Reserve(2);
    filter.IgnoreBody(body_);
    filter.IgnoreBody(player_);
    const Vec3 facing(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 across(std::cos(yaw_), 0.0F, -std::sin(yaw_));
    const std::array<Vec3, 5> offsets{Vec3::sZero(), across * kFootSpanM, -across * kFootSpanM, facing * kFootSpanM,
                                      -facing * kFootSpanM};
    for (const Vec3 &off : offsets) {
        RayCastResult hit;
        const RRayCast ray(at + off, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F));
        if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) {
            continue;
        }
        const RVec3 contact = ray.GetPointOnRay(hit.mFraction);
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().IsSensor()) {
            continue;
        }
        const Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, contact);
        if (normal.GetY() < 0.5F) {
            continue;
        }
        if (velocity.GetY() < lock.GetBody().GetPointVelocity(contact).GetY() + 1.0F) {
            view_.contact_supported = true;
            view_.contact_feet_body = hit.mBodyID;
            view_.contact_feet_sub = hit.mSubShapeID2;
            view_.contact_feet = contact;
            break;
        }
    }
    // Her hand: the grip of the mover that has her, where it is now on its
    // body (it moves with what it holds).
    BodyID body;
    SubShapeID sub;
    Vec3 local = Vec3::sZero();
    bool held = false;
    if (climber_ != nullptr && climber_->on_steel()) {
        body = climber_->hold().body;
        sub = climber_->hold().sub;
        local = climber_->hold().local;
        held = true;
    } else if (routecraft_.owns_motion() && routecraft_.grip(body, sub, local)) {
        held = true;
    } else if (view_.on_pipe && !pipe_support_.IsInvalid()) {
        body = pipe_support_;
        sub = pipe_member_;
        local = pipe_local_hand_;
        held = true;
    }
    if (held && bodies.IsAdded(body)) {
        view_.contact_hand_held = true;
        view_.contact_hand_body = body;
        view_.contact_hand_sub = sub;
        view_.contact_hand = bodies.GetCenterOfMassTransform(body) * local;
    }
}

} // namespace scraperx::sim
