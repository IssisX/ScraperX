#include "sim/physical_story_actor.hpp"

#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace scraperx::sim {
namespace {
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
bool bottom_point(JPH::RVec3 center, JPH::RVec3 point) {
    const auto d = JPH::Vec3(center - point);
    return d.GetY() >= PhysicalStoryActor::half_height_m() - .035F &&
           d.GetY() <= PhysicalStoryActor::half_height_m() + .035F &&
           d.GetX()*d.GetX() + d.GetZ()*d.GetZ() <= .13F*.13F;
}
}

PhysicalStoryActor::PhysicalStoryActor(JPH::PhysicsSystem &system,
    JPH::ObjectLayer layer, std::uint64_t entity) : system_(system), layer_(layer), entity_(entity) {}
PhysicalStoryActor::~PhysicalStoryActor() { destroy(); }

bool PhysicalStoryActor::spawn(JPH::RVec3 center) {
    if (!body_.IsInvalid() || !finite(center)) return false;
    JPH::BodyCreationSettings settings(new JPH::CapsuleShape(half_height_m()-radius_m(), radius_m()),
        center, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, layer_);
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
        JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = mass_kg();
    settings.mFriction = 0; // This finite reciprocal actuator owns ground traction.
    settings.mLinearDamping = 0;
    settings.mAllowSleeping = false; // Fresh active contact callbacks remain available.
    settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    settings.mUserData = entity_;
    body_ = system_.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    return !body_.IsInvalid();
}
bool PhysicalStoryActor::reset(JPH::RVec3 center) {
    if (!finite(center)) return false;
    destroy();
    return spawn(center);
}
void PhysicalStoryActor::destroy() {
    clear_contact();
    drive_ = {};
    have_pre_tick_ = false;
    if (body_.IsInvalid()) return;
    auto &bodies = system_.GetBodyInterface();
    bodies.RemoveBody(body_);
    bodies.DestroyBody(body_);
    body_ = JPH::BodyID();
}
void PhysicalStoryActor::clear_contact() noexcept {
    have_contact_ = false;
    contact_ = {};
    drive_ = {};
}

bool PhysicalStoryActor::capture_contact(JPH::BodyID actor_id, const JPH::Body &first,
    const JPH::Body &second, const JPH::ContactManifold &manifold, std::uint64_t tick,
    Contact &out) noexcept {
    const bool actor_first = first.GetID() == actor_id;
    if (!actor_first && second.GetID() != actor_id) return false;
    const auto &actor = actor_first ? first : second;
    const auto &support = actor_first ? second : first;
    if (!actor.IsRigidBody() || !support.IsRigidBody() || actor.IsSensor() ||
        support.IsSensor() || manifold.mPenetrationDepth < 0 ||
        manifold.mRelativeContactPointsOn1.empty() || manifold.mRelativeContactPointsOn2.empty()) return false;
    const auto normal = actor_first ? -manifold.mWorldSpaceNormal : manifold.mWorldSpaceNormal;
    if (!finite(normal) || normal.GetY() < .7F) return false;
    const auto point = actor_first ? manifold.GetWorldSpaceContactPointOn2(0) :
                                    manifold.GetWorldSpaceContactPointOn1(0);
    if (!finite(point) || !bottom_point(actor.GetPosition(), point)) return false;
    out.actor = actor_id;
    out.support = support.GetID();
    out.subshape = actor_first ? manifold.mSubShapeID2 : manifold.mSubShapeID1;
    out.local_point = JPH::Vec3(support.GetInverseCenterOfMassTransform()*point);
    out.local_normal = support.GetRotation().Conjugated()*normal;
    out.shape = support.GetShape();
    out.solved_tick = tick;
    return true;
}
void PhysicalStoryActor::submit_solved_contact(const Contact &contact) {
    if (body_.IsInvalid() || contact.actor != body_ || contact.support.IsInvalid() ||
        contact.support == body_ || contact.shape == nullptr ||
        !finite(contact.local_point) || !finite(contact.local_normal) ||
        std::abs(contact.local_normal.LengthSq()-1) > .01F) return;
    // One physical support. Stable identity ordering, never callback order,
    // selects among coplanar compound leaves or neighboring actual contacts.
    const auto key = [](const Contact &c) {
        return std::make_tuple(c.support.GetIndexAndSequenceNumber(), c.subshape.GetValue(),
            c.local_point.GetX(), c.local_point.GetY(), c.local_point.GetZ());
    };
    if (have_contact_ && (contact.solved_tick < contact_.solved_tick ||
        (contact.solved_tick == contact_.solved_tick && !(key(contact) < key(contact_))))) return;
    contact_ = contact;
    have_contact_ = true;
}

void PhysicalStoryActor::pre_step(std::uint64_t tick, float dt, JPH::Vec3 desired) {
    // Duplicate/out-of-order calls cannot queue the same force twice.
    if (have_pre_tick_ && tick <= last_pre_tick_) return;
    last_pre_tick_ = tick;
    have_pre_tick_ = true;
    drive_ = {};
    if (body_.IsInvalid() || !std::isfinite(dt) || dt <= 0 || dt > .05F ||
        !finite(desired) || !have_contact_ || tick == 0 || contact_.solved_tick != tick-1) return;
    const auto contact = contact_;
    have_contact_ = false; // A receipt is consumed once; it cannot renew itself.
    auto &bodies = system_.GetBodyInterface();
    JPH::RVec3 center;
    JPH::Vec3 velocity;
    {
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(), body_);
        if (!lock.Succeeded()) return;
        center = lock.GetBody().GetPosition();
        velocity = lock.GetBody().GetLinearVelocity();
    }
    JPH::RVec3 point;
    JPH::Vec3 support_velocity, normal;
    float friction = 0, inverse_mass = 1/mass_kg();
    bool dynamic = false;
    JPH::Vec3 force;
    {
        JPH::BodyLockRead lock(system_.GetBodyLockInterface(), contact.support);
        if (!lock.Succeeded()) return;
        const auto &support = lock.GetBody();
        if (!support.IsRigidBody() || support.IsSensor() || support.GetShape() != contact.shape.GetPtr()) return;
        point = support.GetCenterOfMassTransform()*contact.local_point;
        normal = support.GetRotation()*contact.local_normal;
        support_velocity = support.GetPointVelocity(point);
        if (!bottom_point(center, point) || normal.GetY() < .7F ||
            (velocity-support_velocity).Dot(normal) > .2F) return;
        friction = std::clamp(support.GetFriction(), 0.0F, .7F);
        desired.SetY(0);
        const float requested_speed = desired.Length();
        if (requested_speed > speed_bound_mps()) desired *= speed_bound_mps()/requested_speed;
        auto relative = velocity-support_velocity;
        relative.SetY(0);
        force = (desired-relative)*(mass_kg()/.25F);
        // Same conservative gravity-load traction law as native player ground
        // locomotion. This is not a measured solver normal-force receipt.
        const float cap = std::min(force_bound_n(), friction*mass_kg()*
            std::max(0.0F, -system_.GetGravity().GetY())*normal.GetY());
        const float magnitude = force.Length();
        if (!(magnitude > 0) || !(cap > 0)) force = JPH::Vec3::sZero();
        else if (magnitude > cap) force *= cap/magnitude;
        dynamic = support.IsDynamic();
        if (dynamic && !force.IsNearZero()) {
            const auto lever = JPH::Vec3(point-support.GetCenterOfMassPosition());
            const auto axis = lever.Cross(force.Normalized());
            inverse_mass += support.GetMotionProperties()->GetInverseMass() +
                axis.Dot(support.GetInverseInertia().Multiply3x3(axis));
        }
    }
    // Bound positive command work for the frozen two-body inertia. Braking
    // cannot replenish the motor's per-tick energy allowance.
    double impulse = double(force.Length())*dt;
    if (impulse > 0) {
        const double u = force.Normalized().Dot(velocity-support_velocity);
        const double budget = power_bound_w()*double(dt);
        const double w = inverse_mass;
        const double cap = u < 0 ? -u/w + std::sqrt(2*budget/w) :
            2*budget/(u+std::hypot(u, std::sqrt(2*w*budget)));
        if (impulse > cap) force *= float(cap/impulse*.999999);
        impulse = double(force.Length())*dt;
        const double before = std::max(0.0, u);
        const double after = std::max(0.0, u+w*impulse);
        drive_.command_work_bound_j = std::max(0.0, (after*after-before*before)/(2*w));
    }
    bodies.AddForce(body_, force);
    if (dynamic && !force.IsNearZero()) bodies.AddForce(contact.support, -force, point);
    drive_.supported = true;
    drive_.support = contact.support;
    drive_.support_point = point;
    drive_.support_point_velocity = support_velocity;
    drive_.applied_force_n = force;
}
PhysicalStoryActor::Readback PhysicalStoryActor::readback() const {
    auto result = drive_;
    result.body = body_;
    if (body_.IsInvalid()) return result;
    JPH::BodyLockRead lock(system_.GetBodyLockInterface(), body_);
    if (!lock.Succeeded()) return result;
    result.present = true;
    result.position = lock.GetBody().GetPosition();
    result.velocity = lock.GetBody().GetLinearVelocity();
    return result;
}
} // namespace scraperx::sim
#endif
