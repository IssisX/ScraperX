#include "sim/shared_survival.hpp"
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
constexpr float kSkin = 0.04F; // finite compliant palm travel, not reach at a distance
constexpr float kStopped = 0.05F;
bool positive(float v) { return std::isfinite(v) && v > 0.0F; }
}
SharedSurvival::SharedSurvival(JPH::PhysicsSystem &physics, Support support)
    : physics_(physics), support_(support) {
    if (support.structure.IsInvalid() || !positive(support.feet_below_com) ||
        !positive(support.stance_radius) || !positive(support.grip_n) ||
        !std::isfinite(support.sole_friction) || support.sole_friction < 0.0F || support.sole_friction > 1.0F ||
        !positive(support.stiffness_n_m) || !positive(support.damping_ns_m) ||
        !std::isfinite(support.hand_local.LengthSq()) || support.hand_local.Length() > 2.0F) {
        throw std::invalid_argument("Invalid shared-survival support geometry/capacity");
    }
}
SharedSurvival::~SharedSurvival() { release_hands(); isolate(); }
void SharedSurvival::release_footing() {
    if (!planted_actor_.IsInvalid()) {
        const JPH::BodyLockWrite lock(physics_.GetBodyLockInterface(), planted_actor_);
        // A knockdown or another owner may have replaced the planted setting.
        // Never overwrite that newer physical state during release.
        if (lock.Succeeded() && lock.GetBody().GetFriction() == support_.sole_friction)
            lock.GetBody().SetFriction(original_friction_);
        planted_actor_ = JPH::BodyID();
    }
}
void SharedSurvival::release_hands() { release_footing(); view_ = {}; }
bool SharedSurvival::available(const Channel &c) const {
    auto &b = physics_.GetBodyInterface();
    const auto live = physics_.GetConstraints();
    const auto present = [&](const JPH::Constraint *constraint) {
        return std::any_of(live.begin(), live.end(), [&](const auto &p) { return p.GetPtr() == constraint; });
    };
    return present(c.hinge) && present(c.guard) && b.IsAdded(c.rotor) && (c.frame.IsInvalid() || b.IsAdded(c.frame)) &&
           (c.guard1.IsInvalid() || b.IsAdded(c.guard1)) && b.IsAdded(c.guard2) &&
           c.hinge->GetEnabled() && c.guard->GetEnabled();
}
std::size_t SharedSurvival::add_drive(Drive drive) {
    if (!drive.hinge || !drive.guard || drive.hinge == drive.guard ||
        !drive.hinge->GetBody1()->IsStatic() || !drive.hinge->GetBody2()->IsDynamic() ||
        !std::isfinite(drive.speed_rad_s) || std::abs(drive.speed_rad_s) < kStopped ||
        !positive(drive.torque_nm) || !std::isfinite(drive.off_brake_nm) || drive.off_brake_nm < 0.0F) {
        throw std::invalid_argument("Invalid shared-survival district drive");
    }
    for (const auto &c : channels_) {
        if (c.drive.hinge == drive.hinge) throw std::invalid_argument("Drive already owned by district");
    }
    channels_.push_back({drive, drive.hinge, drive.guard,
        drive.hinge->GetBody2()->GetID(), drive.hinge->GetBody1()->GetID(),
        drive.guard->GetBody1()->GetID(), drive.guard->GetBody2()->GetID(), false});
    configure(channels_.back());
    return channels_.size() - 1;
}
bool SharedSurvival::guard_closed(const Channel &c) const {
    return available(c) && std::abs(c.drive.guard->GetCurrentAngle()) < 0.08F;
}
void SharedSurvival::configure(Channel &c) {
    auto &h = *c.drive.hinge;
    if (!available(c)) { c.running = false; h.SetMotorState(JPH::EMotorState::Off); return; }
    h.GetMotorSettings().mMinTorqueLimit = -c.drive.torque_nm;
    h.GetMotorSettings().mMaxTorqueLimit = c.drive.torque_nm;
    h.SetMaxFrictionTorque(c.drive.off_brake_nm);
    h.SetTargetAngularVelocity(c.drive.speed_rad_s);
    h.SetMotorState(c.running ? JPH::EMotorState::Velocity : JPH::EMotorState::Off);
    physics_.GetBodyInterface().ActivateBody(h.GetBody2()->GetID());
}
void SharedSurvival::isolate() {
    supplied_ = false;
    for (auto &c : channels_) { c.running = false; configure(c); }
}
void SharedSurvival::restore_supply() { supplied_ = true; }
SharedSurvival::Restart SharedSurvival::start(std::size_t channel) {
    auto &c = channels_.at(channel);
    if (!available(c)) return Restart::Unavailable;
    if (!supplied_) return Restart::Isolated;
    if (view_.hands_busy) return Restart::HandsOccupied;
    if (!guard_closed(c)) return Restart::GuardOpen;
    if (std::abs(drive_view(channel).speed_rad_s) > kStopped) return Restart::Moving;
    c.running = true;
    configure(c);
    return Restart::Started;
}
void SharedSurvival::stop(std::size_t channel) {
    auto &c = channels_.at(channel);
    c.running = false;
    configure(c);
}
void SharedSurvival::set_brake(std::size_t channel, float torque_nm) {
    if (!std::isfinite(torque_nm) || torque_nm < 0.0F) throw std::invalid_argument("Invalid brake torque");
    auto &c = channels_.at(channel);
    c.drive.off_brake_nm = torque_nm;
    configure(c);
}
void SharedSurvival::restore_running(std::size_t channel, bool running) {
    auto &c = channels_.at(channel);
    c.running = running && available(c);
    configure(c);
}
SharedSurvival::DriveView SharedSurvival::drive_view(std::size_t channel) const {
    const auto &c = channels_.at(channel);
    if (!available(c)) return {};
    const auto &h = *c.drive.hinge;
    const auto *rotor = h.GetBody2();
    const auto axis = rotor->GetRotation() * h.GetLocalSpaceHingeAxis2();
    const float speed = rotor->GetAngularVelocity().Dot(axis);
    return {c.running, speed, h.GetCurrentAngle(),
            std::clamp(std::abs(speed / c.drive.speed_rad_s), 0.0F, 1.0F)};
}
void SharedSurvival::reset() { release_hands(); isolate(); }
void SharedSurvival::pre_step(JPH::BodyID actor, bool eligible, JPH::Quat facing) {
    view_ = {};
    if (planted_actor_ != actor) release_footing();
    auto &bodies = physics_.GetBodyInterface();
    if (eligible && !actor.IsInvalid() && actor != support_.structure &&
        bodies.IsAdded(actor) && bodies.IsAdded(support_.structure)) {
        float weight = 0.0F;
        {
            const JPH::BodyLockRead lock(physics_.GetBodyLockInterface(), actor);
            if (lock.Succeeded() && lock.GetBody().IsDynamic() && !lock.GetBody().IsSensor()) {
                weight = -physics_.GetGravity().GetY() / lock.GetBody().GetMotionProperties()->GetInverseMass();
            }
        }
        const auto actor_transform = bodies.GetWorldTransform(actor);
        const auto com = bodies.GetCenterOfMassPosition(actor);
        const auto hand = actor_transform * (facing * support_.hand_local);
        view_.hand = hand;
        const auto feet = com - JPH::Vec3(0, support_.feet_below_com, 0);
        const JPH::IgnoreSingleBodyFilter not_actor(actor);
        JPH::RayCastResult foot_hit;
        const JPH::RRayCast foot_ray(feet + JPH::Vec3(0, 0.03F, 0), JPH::Vec3(0, -0.12F, 0));
        if (weight > 0.0F && physics_.GetNarrowPhaseQuery().CastRay(foot_ray, foot_hit, {}, {}, not_actor) &&
            foot_hit.mBodyID != support_.structure && physics_.WereBodiesInContact(actor, foot_hit.mBodyID)) {
            JPH::Vec3 floor_normal;
            float friction = 0;
            bool solid_floor = false;
            {
                const JPH::BodyLockRead lock(physics_.GetBodyLockInterface(), foot_hit.mBodyID);
                solid_floor = !lock.GetBody().IsSensor();
                floor_normal = lock.GetBody().GetWorldSpaceSurfaceNormal(foot_hit.mSubShapeID2,
                    foot_ray.GetPointOnRay(foot_hit.mFraction));
                friction = std::min(support_.sole_friction, lock.GetBody().GetFriction());
            }
            view_.footing = solid_floor && floor_normal.GetY() > 0.7F;
            // The palm can compress by at most 8 cm before the member escapes it.
            // Cast from below the palm, so even slightly compressed contact is found.
            const JPH::RRayCast palm_ray(hand - JPH::Vec3(0, kSkin, 0), JPH::Vec3(0, 2*kSkin, 0));
            JPH::RayCastResult hit;
            if (view_.footing && physics_.GetNarrowPhaseQuery().CastRay(palm_ray, hit, {}, {}, not_actor) &&
                hit.mBodyID == support_.structure) {
                const auto point = palm_ray.GetPointOnRay(hit.mFraction);
                JPH::Vec3 normal;
                bool dynamic = false;
                {
                    const JPH::BodyLockRead lock(physics_.GetBodyLockInterface(), support_.structure);
                    dynamic = lock.GetBody().IsDynamic() && !lock.GetBody().IsSensor();
                    normal = -lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point);
                }
                if (dynamic && normal.GetY() > 0.7F) {
                    if (planted_actor_.IsInvalid()) {
                        planted_actor_ = actor;
                        original_friction_ = bodies.GetFriction(actor);
                    }
                    bodies.SetFriction(actor, support_.sole_friction);
                    const float moment_arm = JPH::Vec3(point - feet).Cross(normal).Length();
                    const float balance = weight * support_.stance_radius / std::max(moment_arm, 0.01F);
                    const float horizontal = std::sqrt(normal.GetX()*normal.GetX() + normal.GetZ()*normal.GetZ());
                    // Downward hand reaction adds normal load at her feet.
                    const float slip = horizontal - friction * normal.GetY();
                    const float traction = slip > 0.0F ? friction * weight / slip : support_.grip_n;
                    view_.capacity_n = std::min({support_.grip_n, balance, traction});
                    const float compression = kSkin - float((point - hand).GetY());
                    const float closing = (bodies.GetPointVelocity(actor, point) -
                                           bodies.GetPointVelocity(support_.structure, point)).Dot(normal);
                    view_.demand_n = std::max(0.0F, compression * normal.GetY() * support_.stiffness_n_m +
                                                   closing * support_.damping_ns_m);
                    view_.applied_n = std::min(view_.demand_n, view_.capacity_n);
                    view_.hands_busy = view_.applied_n > 1.0F;
                    if (view_.hands_busy) {
                        view_.effort = view_.demand_n >= view_.capacity_n ? Effort::AtCapacity : Effort::Bracing;
                        const auto force = normal * view_.applied_n;
                        bodies.AddForce(support_.structure, force, point);
                        bodies.AddForce(actor, -force, point);
                    }
                }
            }
        }
    }
    if (!view_.hands_busy) release_footing();
    // Loss of any interlock stops the drive; closing a guard never restarts it.
    for (auto &c : channels_) {
        if (c.running && (!supplied_ || view_.hands_busy || !guard_closed(c))) {
            c.running = false;
            configure(c);
        }
    }
}
} // namespace scraperx::sim
