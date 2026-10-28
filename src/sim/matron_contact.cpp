// Her contacts as the simulation itself has them, for the presentation: what
// her feet stand on and what her hand holds, this tick (issue #3, S6 "one
// contact truth"). The view used to infer these (a second hand 0.45 m along a
// rung wherever any solid was near, a deck under her feet mid-mantle, a fall
// from how fast she was drawn dropping); it now plants a limb only on what is
// here. Read-only: no force, no velocity, no position is written, and nothing
// in her movement reads these fields back.

#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>

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
    view_.contact_braced = false;
    view_.contact_brace_body = BodyID();
    view_.contact_brace = RVec3::sZero();
    if (!body_in_world_) {
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    // Her feet: walk_toward's own floor under her, the one her feet push on
    // (sole_contact: her middle first, then her feet's span; a body's floor,
    // not a sensor, its normal at least half up, she not rising off it faster
    // than it rises). Not a second test of it.
    Sole sole;
    if (sole_contact(bodies.GetPosition(body_), bodies.GetLinearVelocity(body_), yaw_, sole)) {
        view_.contact_supported = true;
        view_.contact_feet_body = sole.body;
        view_.contact_feet_sub = sole.sub;
        view_.contact_feet = sole.point;
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
    // On her pipe, hand over hand (her hand on its face): her squeeze is her
    // hands' pull against her feet's push, so her feet are on the riser too.
    // Level from her shins toward the riser's axis, the first steel within
    // her legs' reach, a standing face of static steel: the face they press.
    if (view_.on_pipe && pipe_phase_ == PipePhase::Climb && pipe_on_face_ && view_.contact_hand_held) {
        const RVec3 at = bodies.GetPosition(body_);
        const RVec3 shins(at.GetX(), at.GetY() - kStandCentre + kBraceShinM, at.GetZ());
        Vec3 in(kPipeCentreX - float(shins.GetX()), 0.0F, kPipeCentreZ - float(shins.GetZ()));
        if (in.LengthSq() > 1.0e-6F) {
            in = in.Normalized();
            const grip::ClimbBodyFilter filter(body_, player_);
            const RRayCast ray(shins, in * kBraceReachM);
            RayCastSettings settings;
            settings.mTreatConvexAsSolid = false;
            ClosestHitCollisionCollector<CastRayCollector> hit;
            physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hit, {}, {}, filter);
            if (hit.HadHit()) {
                const RVec3 point = ray.GetPointOnRay(hit.mHit.mFraction);
                const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mHit.mBodyID);
                if (lock.Succeeded() && lock.GetBody().IsStatic() &&
                    std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mHit.mSubShapeID2, point).GetY()) < 0.3F) {
                    view_.contact_braced = true;
                    view_.contact_brace_body = hit.mHit.mBodyID;
                    view_.contact_brace = point;
                }
            }
        }
    }
}

} // namespace scraperx::sim
