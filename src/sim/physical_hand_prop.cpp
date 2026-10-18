#include "sim/physical_hand_prop.hpp"

#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
template<class V> bool finite(const V &v) noexcept {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
constexpr float kOpeningNoiseMps = 1.0e-4F;

// Zero-bias unilateral normal contact at one actual material palm point.
// Both lever arms use that SAME world point, so the reciprocal reaction has
// the real support torque. There is no geometric spring/rotating-normal bias.
class PalmNormalRow final : public JPH::TwoBodyConstraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    PalmNormalRow(JPH::Body &support, JPH::Body &player,
                  const JPH::DistanceConstraintSettings &settings,
                  JPH::Vec3 normal, float shoulder_offset, float maximum_reach)
        : TwoBodyConstraint(support, player, settings),
          local_anchor_(JPH::Vec3(support.GetInverseCenterOfMassTransform() * settings.mPoint1)),
          local_normal_(support.GetRotation().Conjugated() * normal),
          shape_identity_(support.GetShape()), shoulder_offset_(shoulder_offset),
          maximum_reach_(maximum_reach) {}

    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User3; }
    void NotifyShapeChanged(const JPH::BodyID &body, JPH::Vec3Arg) override {
        if (body == mBody1->GetID()) surface_changed_ = true;
        // COM-preserving player tuck changes its capsule, not this palm.
        // The next solve rebuilds the actual contact-point Jacobian.
        axis_part_.Deactivate();
    }
    JPH::Mat44 GetConstraintToBody1Matrix() const override {
        return JPH::Mat44::sTranslation(local_anchor_);
    }
    JPH::Mat44 GetConstraintToBody2Matrix() const override {
        return JPH::Mat44::sTranslation(JPH::Vec3(mBody2->GetInverseCenterOfMassTransform() * anchor_));
    }
    bool bodies_valid() const noexcept {
        return mBody1->IsRigidBody() && mBody2->IsRigidBody() && mBody2->IsDynamic() &&
            !mBody1->IsSensor() && !mBody2->IsSensor() &&
            mBody1->IsInBroadPhase() && mBody2->IsInBroadPhase() &&
            !surface_changed_ && mBody1->GetShape() == shape_identity_.GetPtr();
    }
    bool update_geometry() noexcept {
        anchor_ = mBody1->GetCenterOfMassTransform() * local_anchor_;
        normal_ = mBody1->GetRotation() * local_normal_;
        const auto shoulder = mBody2->GetCenterOfMassPosition() +
            mBody2->GetRotation() * JPH::Vec3(0, shoulder_offset_, 0);
        const JPH::Vec3 delta(shoulder - anchor_);
        if (!finite(anchor_) || !finite(normal_) || !finite(delta)) return false;
        reachable_ = delta.LengthSq() <= maximum_reach_ * maximum_reach_ &&
            delta.Dot(normal_) >= 0;
        relative_normal_speed_ = normal_.Dot(
            mBody2->GetPointVelocity(anchor_) - mBody1->GetPointVelocity(anchor_));
        return std::isfinite(relative_normal_speed_);
    }
    void SetupVelocityConstraint(float dt) override {
        axis_part_.Deactivate();
        applied_ = false;
        impulse_ = 0;
        event_work_ = 0;
        solve_dt_ = dt;
        if (!GetEnabled() || !std::isfinite(dt) || dt <= 0 ||
            !bodies_valid() || !update_geometry() || !reachable_) return;
        solve_anchor_ = anchor_;
        solve_normal_ = normal_;
        const JPH::Vec3 r1(anchor_ - mBody1->GetCenterOfMassPosition());
        const JPH::Vec3 r2(anchor_ - mBody2->GetCenterOfMassPosition());
        axis_part_.CalculateConstraintProperties(*mBody1, r1, *mBody2, r2, solve_normal_);
    }
    void ResetWarmStart() override { axis_part_.Deactivate(); }
    void WarmStartVelocityConstraint(float) override {}
    bool SolveVelocityConstraint(float) override {
        if (applied_ || !axis_part_.IsActive()) return false;
        applied_ = true;
        // Sample immediately before the one admitted impulse, after any
        // earlier island rows. No cached impulse may push a separating palm.
        const double u = solve_normal_.Dot(mBody2->GetPointVelocity(solve_anchor_) -
                                           mBody1->GetPointVelocity(solve_anchor_));
        double inverse_mass = 0;
        for (const auto *body : {mBody1, mBody2}) {
            if (!body->IsDynamic()) continue;
            const auto *motion = body->GetMotionPropertiesUnchecked();
            const auto lever = JPH::Vec3(solve_anchor_ - body->GetCenterOfMassPosition()).Cross(solve_normal_);
            inverse_mass += motion->GetInverseMass() + lever.Dot(
                motion->MultiplyWorldSpaceInverseInertiaByVector(body->GetRotation(), lever));
        }
        if (!(inverse_mass > 0) || !std::isfinite(inverse_mass) || !std::isfinite(u) || u >= 0)
            return false;
        const double stopping_impulse = -u / inverse_mass;
        float admitted = static_cast<float>(std::min(
            double(PhysicalHandProp::force_bound_n()) * solve_dt_, stopping_impulse));
        while (double(admitted) > stopping_impulse ||
               double(admitted) > double(PhysicalHandProp::force_bound_n()) * solve_dt_)
            admitted = std::nextafter(admitted, 0.0F);
        if (!(admitted > 0)) return false;
        impulse_ = admitted;
        event_work_ = double(admitted) * u + .5 * inverse_mass * admitted * admitted;
        absorbed_work_ += std::max(0.0, -event_work_);
        peak_load_ = std::max(peak_load_, admitted / solve_dt_);
        // A single reciprocal Jolt Jacobian impulse. No parallel AddImpulse,
        // no forward row, no position solve, and no later iteration repeats it.
        return axis_part_.SolveVelocityConstraint(*mBody1, *mBody2, solve_normal_, admitted, admitted);
    }
    bool SolvePositionConstraint(float, float) override { return false; }
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override { return nullptr; }
    void SaveState(JPH::StateRecorder &) const override {
        throw std::logic_error("PhysicalHandProp is transient; clear before physics SaveState");
    }
    void RestoreState(JPH::StateRecorder &) override {
        throw std::logic_error("PhysicalHandProp is transient; clear before physics RestoreState");
    }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *renderer) const override {
        renderer->DrawLine(anchor_, anchor_ + normal_ * .3F, JPH::Color::sGreen);
    }
#endif
    [[nodiscard]] bool reachable() const noexcept { return reachable_; }
    [[nodiscard]] float opening_speed() const noexcept { return relative_normal_speed_; }
    [[nodiscard]] JPH::RVec3 anchor() const noexcept { return anchor_; }
    [[nodiscard]] JPH::Vec3 normal() const noexcept { return normal_; }
    [[nodiscard]] JPH::Vec3 point_velocity() const noexcept { return mBody1->GetPointVelocity(anchor_); }
    [[nodiscard]] JPH::Vec3 local_shoulder() const noexcept {
        const auto shoulder = mBody2->GetCenterOfMassPosition() +
            mBody2->GetRotation() * JPH::Vec3(0, shoulder_offset_, 0);
        return JPH::Vec3(mBody1->GetInverseCenterOfMassTransform() * shoulder);
    }
    [[nodiscard]] JPH::Vec3 local_normal() const noexcept { return local_normal_; }
    [[nodiscard]] JPH::Vec3 local_anchor() const noexcept { return local_anchor_; }
    [[nodiscard]] float impulse() const noexcept { return impulse_; }
    [[nodiscard]] float solve_dt() const noexcept { return solve_dt_; }
    [[nodiscard]] JPH::Vec3 solve_normal() const noexcept { return solve_normal_; }
    [[nodiscard]] double event_work() const noexcept { return event_work_; }
    [[nodiscard]] double absorbed_work() const noexcept { return absorbed_work_; }
    [[nodiscard]] float peak_load() const noexcept { return peak_load_; }
private:
    JPH::Vec3 local_anchor_, local_normal_;
    JPH::RefConst<JPH::Shape> shape_identity_;
    float shoulder_offset_, maximum_reach_;
    JPH::AxisConstraintPart axis_part_;
    JPH::RVec3 anchor_ = JPH::RVec3::sZero(), solve_anchor_ = JPH::RVec3::sZero();
    JPH::Vec3 normal_ = JPH::Vec3::sAxisY(), solve_normal_ = JPH::Vec3::sAxisY();
    bool reachable_ = false, applied_ = false, surface_changed_ = false;
    float relative_normal_speed_ = 0, impulse_ = 0, solve_dt_ = 0, peak_load_ = 0;
    double event_work_ = 0, absorbed_work_ = 0;
};
} // namespace

struct PhysicalHandProp::Impl {
    JPH::PhysicsSystem &system;
    JPH::BodyID player;
    float shoulder_offset, maximum_reach;
    JPH::Ref<PalmNormalRow> row;
    Readback state{};
    JPH::Vec3 previous_local_shoulder = JPH::Vec3::sZero();
    bool stop_requested = false;
    Impl(JPH::PhysicsSystem &value, JPH::BodyID body, float shoulder, float reach)
        : system(value), player(body), shoulder_offset(shoulder), maximum_reach(reach) {}
    void stop(StopReason reason) noexcept {
        stop_requested = true;
        state.stop_reason = reason;
        if (row) row->SetEnabled(false);
    }
    void sample_geometry() noexcept {
        state.world_anchor = row->anchor();
        state.surface_normal = row->normal();
        state.point_velocity = row->point_velocity();
    }
    void sample_yield() noexcept {
        const auto current = row->local_shoulder();
        state.yielded_normal_distance_m += std::max(0.0F,
            -(current - previous_local_shoulder).Dot(row->local_normal()));
        previous_local_shoulder = current;
    }
};

PhysicalHandProp::PhysicalHandProp(JPH::PhysicsSystem &system, JPH::BodyID player,
                                 float shoulder_offset_m, float maximum_reach_m)
    : impl_(std::make_unique<Impl>(system, player, shoulder_offset_m, maximum_reach_m)) {}
PhysicalHandProp::~PhysicalHandProp() { clear(); }

bool PhysicalHandProp::begin(JPH::BodyID support, JPH::SubShapeID subshape,
                           JPH::RVec3 point, JPH::Vec3 normal) {
    auto &owner = *impl_;
    if (owner.row || owner.player.IsInvalid() || support.IsInvalid() || support == owner.player ||
        !finite(point) || !finite(normal) || normal.LengthSq() < 1.0e-8F ||
        !std::isfinite(owner.shoulder_offset) || !std::isfinite(owner.maximum_reach) ||
        owner.shoulder_offset < 0 || owner.maximum_reach <= 0) return false;
    normal = normal.Normalized();
    JPH::Ref<PalmNormalRow> candidate;
    bool wake_support = false;
    {
        const JPH::BodyID ids[]{support, owner.player};
        JPH::BodyLockMultiWrite lock(owner.system.GetBodyLockInterface(), ids, 2);
        auto *hold = lock.GetBody(0), *player = lock.GetBody(1);
        if (!hold || !player || !hold->IsRigidBody() || !player->IsRigidBody() ||
            !player->IsDynamic() || hold->IsSensor() || player->IsSensor() ||
            !hold->IsInBroadPhase() || !player->IsInBroadPhase()) return false;
        JPH::DistanceConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = point;
        settings.mNumPositionStepsOverride = 0;
        candidate = new PalmNormalRow(*hold, *player, settings, normal,
                                     owner.shoulder_offset, owner.maximum_reach);
        if (!candidate->update_geometry() || !candidate->reachable() ||
            candidate->opening_speed() > kOpeningNoiseMps) return false;
        wake_support = hold->IsDynamic();
    }
    const auto generation = owner.state.generation + 1;
    owner.row = candidate;
    owner.state = {};
    owner.state.active = true;
    owner.state.generation = generation;
    owner.state.support = support;
    owner.state.subshape = subshape;
    owner.stop_requested = false;
    owner.previous_local_shoulder = candidate->local_shoulder();
    owner.sample_geometry();
    owner.system.AddConstraint(candidate.GetPtr());
    owner.system.GetBodyInterface().ActivateBody(owner.player);
    if (wake_support) owner.system.GetBodyInterface().ActivateBody(support);
    return true;
}

void PhysicalHandProp::clear(StopReason reason) {
    auto &owner = *impl_;
    if (!owner.row) return;
    owner.row->SetEnabled(false);
    owner.system.RemoveConstraint(owner.row.GetPtr());
    owner.row = nullptr;
    owner.stop_requested = false;
    owner.state.active = false;
    owner.state.stop_reason = reason;
    ++owner.state.generation;
    owner.state.last_impulse_ns = owner.state.last_load_n = 0;
    owner.state.player_force = JPH::Vec3::sZero();
}

void PhysicalHandProp::pre_step(float dt, bool surface_valid) noexcept {
    auto &owner = *impl_;
    if (!owner.row || owner.stop_requested) return;
    if (!std::isfinite(dt) || dt <= 0) { owner.stop(StopReason::InvalidTimeStep); return; }
    if (!surface_valid) { owner.stop(StopReason::SurfaceLost); return; }
    if (!owner.row->bodies_valid()) { owner.stop(StopReason::InvalidBody); return; }
    if (!owner.row->update_geometry()) { owner.stop(StopReason::InvalidGeometry); return; }
    owner.sample_geometry();
    owner.sample_yield();
    if (!owner.row->reachable()) { owner.stop(StopReason::ReachLost); return; }
    if (owner.row->opening_speed() > kOpeningNoiseMps) { owner.stop(StopReason::Separated); return; }
    if (owner.state.yielded_normal_distance_m >= maximum_yield_m()) {
        owner.stop(StopReason::StrokeExhausted); return;
    }
    if (owner.state.elapsed_seconds + double(dt) > duration_seconds() + 1.0e-8) {
        owner.stop(StopReason::Expired); return;
    }
    owner.state.elapsed_seconds += double(dt);
}

void PhysicalHandProp::post_step(float dt) {
    auto &owner = *impl_;
    if (!owner.row) return;
    if (!owner.stop_requested && (!std::isfinite(dt) || dt <= 0)) owner.stop(StopReason::InvalidTimeStep);
    if (!owner.stop_requested && !owner.row->bodies_valid()) owner.stop(StopReason::InvalidBody);
    else if (!owner.stop_requested && !owner.row->update_geometry()) owner.stop(StopReason::InvalidGeometry);
    else if (!owner.stop_requested) {
        owner.sample_geometry();
        owner.sample_yield();
        if (!owner.row->reachable()) owner.stop(StopReason::ReachLost);
        else if (owner.row->opening_speed() > kOpeningNoiseMps) owner.stop(StopReason::Separated);
        else if (owner.state.yielded_normal_distance_m >= maximum_yield_m())
            owner.stop(StopReason::StrokeExhausted);
    }
    owner.state.last_collision_dt = owner.row->solve_dt();
    owner.state.last_impulse_ns = owner.row->impulse();
    owner.state.last_load_n = owner.state.last_collision_dt > 0 ?
        owner.state.last_impulse_ns / owner.state.last_collision_dt : 0;
    owner.state.peak_load_n = owner.row->peak_load();
    owner.state.player_force = owner.row->solve_normal() * owner.state.last_load_n;
    owner.state.last_event_work_j = owner.row->event_work();
    owner.state.absorbed_event_work_j = owner.row->absorbed_work();
    if (owner.stop_requested) clear(owner.state.stop_reason);
}

bool PhysicalHandProp::active() const noexcept { return impl_->state.active; }
const PhysicalHandProp::Readback &PhysicalHandProp::readback() const noexcept { return impl_->state; }
JPH::Vec3 PhysicalHandProp::support_local_anchor() const noexcept {
    return impl_->row ? impl_->row->local_anchor() : JPH::Vec3::sZero();
}
float PhysicalHandProp::shoulder_offset_m() const noexcept { return impl_->shoulder_offset; }

} // namespace scraperx::sim
#endif
