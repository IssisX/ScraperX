// Distance-row construction adapted from Jolt Physics DistanceConstraint.cpp.
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT
// Original: https://github.com/jrouwe/JoltPhysics (pinned Jolt 5.6 headers).
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "sim/physical_foot_push.hpp"

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
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) &&
           std::isfinite(v.GetZ());
}

// One private, compression-only distance spring. The built-in final
// DistanceConstraint has no force cap; a SixDOF normal row puts r1+u at
// the player COM even when u is not parallel to the normal. This distance
// axis makes u x axis zero: the support torque is at the actual foot point.
class FootDistanceRow final : public JPH::TwoBodyConstraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    FootDistanceRow(JPH::Body &support, JPH::Body &player,
                    const JPH::DistanceConstraintSettings &settings,
                    JPH::Vec3 world_normal, float friction)
        : TwoBodyConstraint(support, player, settings),
          local_anchor_(JPH::Vec3(support.GetInverseCenterOfMassTransform() * settings.mPoint1)),
          local_normal_(support.GetRotation().Conjugated() * world_normal),
          rest_distance_(settings.mMinDistance), friction_(friction) {}

    JPH::EConstraintSubType GetSubType() const override {
        return JPH::EConstraintSubType::User1;
    }
    void NotifyShapeChanged(const JPH::BodyID &id, JPH::Vec3Arg delta_com) override {
        if (id == mBody1->GetID()) local_anchor_ -= delta_com;
        // Body2 endpoint deliberately remains its actual COM, not a surface.
    }
    JPH::Mat44 GetConstraintToBody1Matrix() const override {
        return JPH::Mat44::sTranslation(local_anchor_);
    }
    JPH::Mat44 GetConstraintToBody2Matrix() const override {
        return JPH::Mat44::sIdentity();
    }
    void SetupVelocityConstraint(float dt) override {
        if (!GetEnabled() || !std::isfinite(dt) || dt <= 0 || !update_geometry()) {
            axis_part_.Deactivate();
            return;
        }
        solve_dt_ = dt;
        solve_axis_ = axis_;
        max_lambda_ = PhysicalFootPush::force_bound_n() * dt;
        if (distance_ >= rest_distance_) {
            axis_part_.Deactivate(); // Never pull the player back to the support.
            return;
        }
        const JPH::Vec3 r1_plus_u(player_point_ - mBody1->GetCenterOfMassPosition());
        // u is parallel to axis_, so (r1+u) x axis_ = r1 x axis_. Body2
        // has r2=0: equal/opposite reaction and zero leg torque at player COM.
        axis_part_.CalculateConstraintPropertiesWithStiffnessAndDamping(
            dt, *mBody1, r1_plus_u, *mBody2, JPH::Vec3::sZero(), axis_,
            0.0F, distance_ - rest_distance_, PhysicalFootPush::stiffness_n_per_m(),
            PhysicalFootPush::damping_ns_per_m());
    }
    void ResetWarmStart() override { axis_part_.Deactivate(); }
    void WarmStartVelocityConstraint(float ratio) override {
        if (!axis_part_.IsActive() || !std::isfinite(ratio) || ratio < 0) return;
        // AxisConstraintPart's warm start itself is unclamped. Limit its
        // scale so the cached positive impulse also obeys the CURRENT dt cap.
        const float previous = axis_part_.GetTotalLambda();
        if (previous > 0) ratio = std::min(ratio, max_lambda_ / previous);
        axis_part_.WarmStart(*mBody1, *mBody2, axis_, ratio);
    }
    bool SolveVelocityConstraint(float) override {
        return axis_part_.IsActive() &&
            axis_part_.SolveVelocityConstraint(*mBody1, *mBody2, axis_, 0, max_lambda_);
    }
    bool SolvePositionConstraint(float, float) override {
        return false; // No unbudgeted hard position correction.
    }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *renderer) const override {
        renderer->DrawLine(anchor_, player_point_, JPH::Color::sYellow);
    }
#endif
    // This short-lived gameplay row has no serialized creation/command
    // state. Returning built-in DistanceConstraintSettings would silently
    // restore an uncapped bilateral spring. Explicitly reject that promise.
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override {
        return nullptr;
    }
    void SaveState(JPH::StateRecorder &) const override {
        throw std::logic_error("PhysicalFootPush is transient; clear before physics SaveState");
    }
    void RestoreState(JPH::StateRecorder &) override {
        throw std::logic_error("PhysicalFootPush is transient; clear before physics RestoreState");
    }

    bool update_geometry() noexcept {
        anchor_ = mBody1->GetCenterOfMassTransform() * local_anchor_;
        player_point_ = mBody2->GetCenterOfMassPosition();
        normal_ = mBody1->GetRotation() * local_normal_;
        const JPH::Vec3 delta(player_point_ - anchor_);
        distance_ = delta.Length();
        if (!finite(anchor_) || !finite(player_point_) || !finite(normal_) ||
            !std::isfinite(distance_) || distance_ < 1.0e-4F) return false;
        axis_ = delta / distance_;
        const float height = delta.Dot(normal_);
        lateral_distance_ = (delta - height * normal_).Length();
        const float axial_normal = axis_.Dot(normal_);
        traction_ok_ = axial_normal > 0 &&
            (axis_ - axial_normal * normal_).Length() <= friction_ * axial_normal;
        return std::isfinite(lateral_distance_);
    }
    bool bodies_valid() const noexcept {
        return mBody1->IsRigidBody() && mBody2->IsRigidBody() && mBody2->IsDynamic() &&
            !mBody1->IsSensor() && !mBody2->IsSensor() &&
            mBody1->IsInBroadPhase() && mBody2->IsInBroadPhase();
    }
    void disable() noexcept { SetEnabled(false); ResetWarmStart(); }
    float impulse() const noexcept { return axis_part_.GetTotalLambda(); }
    float impulse_dt() const noexcept { return solve_dt_; }
    JPH::Vec3 impulse_axis() const noexcept { return solve_axis_; }
    float distance() const noexcept { return distance_; }
    float rest_distance() const noexcept { return rest_distance_; }
    void set_rest_distance(float value) noexcept { rest_distance_ = value; }
    float lateral_distance() const noexcept { return lateral_distance_; }
    bool traction_ok() const noexcept { return traction_ok_; }
    JPH::RVec3 anchor() const noexcept { return anchor_; }
    JPH::Vec3 axis() const noexcept { return axis_; }

private:
    JPH::Vec3 local_anchor_, local_normal_;
    float rest_distance_;
    float friction_;
    JPH::AxisConstraintPart axis_part_;
    JPH::RVec3 anchor_ = JPH::RVec3::sZero(), player_point_ = JPH::RVec3::sZero();
    JPH::Vec3 normal_ = JPH::Vec3::sAxisY(), axis_ = JPH::Vec3::sAxisY();
    JPH::Vec3 solve_axis_ = JPH::Vec3::sAxisY();
    float distance_ = 0, lateral_distance_ = 0, max_lambda_ = 0;
    float solve_dt_ = 0;
    bool traction_ok_ = false;
};

double nominal_storage(float rest, float distance) noexcept {
    const double compression = std::max(0.0, double(rest) - double(distance));
    return .5 * PhysicalFootPush::stiffness_n_per_m() * compression * compression;
}
} // namespace

struct PhysicalFootPush::Impl {
    Impl(JPH::PhysicsSystem &physics, JPH::BodyID player_id)
        : system(physics), player(player_id) {}
    JPH::PhysicsSystem &system;
    JPH::BodyID player;
    JPH::Ref<FootDistanceRow> row;
    Readback state;
    float requested_stroke_m = PhysicalFootPush::maximum_stroke_m();

    void request_stop(StopReason reason) noexcept {
        if (!row || state.stop_requested) return;
        row->disable();
        state.active = false;
        state.stop_requested = true;
        state.stop_reason = reason;
    }
    void sample_geometry() noexcept {
        state.world_anchor = row->anchor();
        state.leg_axis = row->axis();
        state.actual_distance_m = row->distance();
        state.rest_distance_m = row->rest_distance();
        state.commanded_stroke_m = state.rest_distance_m - state.initial_distance_m;
        state.nominal_spring_storage_j = nominal_storage(
            state.rest_distance_m, state.actual_distance_m);
    }
    void sample_impulse() noexcept {
        state.last_impulse_ns = row->impulse();
        state.last_collision_dt = row->impulse_dt();
        state.last_solve_axis = row->impulse_axis();
        state.last_load_n = state.last_collision_dt > 0 ?
            state.last_impulse_ns / state.last_collision_dt : 0;
        state.peak_load_n = std::max(state.peak_load_n, state.last_load_n);
        state.player_force = state.last_load_n * state.last_solve_axis;
    }
};

PhysicalFootPush::PhysicalFootPush(JPH::PhysicsSystem &system, JPH::BodyID player)
    : impl_(std::make_unique<Impl>(system, player)) {}
PhysicalFootPush::~PhysicalFootPush() { clear(); }

bool PhysicalFootPush::begin(JPH::BodyID support, JPH::RVec3 point,
                            JPH::Vec3 normal, float friction, float requested_stroke_m) {
    using namespace JPH;
    if (impl_->row || impl_->player.IsInvalid() || support.IsInvalid() || support == impl_->player ||
        !finite(point) || !finite(normal) || !std::isfinite(friction) || friction < 0 ||
        !std::isfinite(requested_stroke_m) || requested_stroke_m <= 0 ||
        requested_stroke_m > maximum_stroke_m() ||
        normal.LengthSq() < 1.0e-8F) return false;
    normal = normal.Normalized();
    Ref<FootDistanceRow> candidate;
    bool wake_support = false;
    {
        const BodyID ids[]{support, impl_->player};
        BodyLockMultiWrite lock(impl_->system.GetBodyLockInterface(), ids, 2);
        Body *hold = lock.GetBody(0), *player = lock.GetBody(1);
        if (!hold || !player || !hold->IsRigidBody() || !player->IsRigidBody() ||
            !player->IsDynamic() || hold->IsSensor() || player->IsSensor() ||
            !hold->IsInBroadPhase() || !player->IsInBroadPhase()) return false;
        const Vec3 delta(player->GetCenterOfMassPosition() - point);
        const float distance = delta.Length();
        if (!std::isfinite(distance) || distance < 1.0e-4F) return false;
        DistanceConstraintSettings settings;
        settings.mPoint1 = point;
        settings.mPoint2 = player->GetCenterOfMassPosition();
        settings.mMinDistance = distance;
        settings.mNumVelocityStepsOverride = 40;
        settings.mNumPositionStepsOverride = 0;
        candidate = new FootDistanceRow(*hold, *player, settings, normal, friction);
        if (!candidate->update_geometry() || !candidate->traction_ok() ||
            candidate->lateral_distance() > lateral_reach_m()) return false;
        // Capture the representable round-tripped material anchor exactly:
        // no preload from COM-local/world conversion rounding at attachment.
        candidate->set_rest_distance(candidate->distance());
        wake_support = hold->IsDynamic();
    }
    clear();
    impl_->row = candidate;
    impl_->requested_stroke_m = requested_stroke_m;
    impl_->state = Readback{};
    impl_->state.stroke_limit_m = requested_stroke_m;
    impl_->state.support = support;
    impl_->state.active = true;
    impl_->state.initial_distance_m = candidate->distance();
    impl_->sample_geometry();
    impl_->system.AddConstraint(candidate.GetPtr());
    auto &bodies = impl_->system.GetBodyInterface();
    bodies.ActivateBody(impl_->player);
    if (wake_support) bodies.ActivateBody(support);
    return true;
}

void PhysicalFootPush::clear(StopReason reason) {
    if (!impl_->row) return;
    impl_->row->disable();
    impl_->system.RemoveConstraint(impl_->row.GetPtr());
    impl_->row = nullptr;
    impl_->state.active = false;
    impl_->state.stop_requested = false;
    impl_->state.stop_reason = reason;
    impl_->state.last_impulse_ns = impl_->state.last_load_n = 0;
    impl_->state.player_force = JPH::Vec3::sZero();
}

void PhysicalFootPush::pre_step(float dt, bool face_valid, bool traction_valid) noexcept {
    auto &owner = *impl_;
    owner.state.last_command_work_bound_j = 0;
    owner.state.target_storage_delta_j = 0;
    if (!owner.row || owner.state.stop_requested) return;
    // Read the PREVIOUS solved substep before target/geometry changes or a
    // stop resets its cached impulse. A peak is a max, never an impulse sum.
    owner.sample_impulse();
    if (!std::isfinite(dt) || dt <= 0) {
        owner.request_stop(StopReason::InvalidTimeStep); return;
    }
    if (!face_valid) { owner.request_stop(StopReason::MaterialFaceLost); return; }
    if (!traction_valid) { owner.request_stop(StopReason::TractionLost); return; }
    if (!owner.row->bodies_valid()) { owner.request_stop(StopReason::InvalidBody); return; }
    if (!owner.row->update_geometry()) {
        owner.request_stop(StopReason::InvalidGeometry); return;
    }
    owner.sample_geometry();
    if (owner.row->distance() > owner.state.initial_distance_m + maximum_stroke_m() + 1.0e-4F) {
        owner.request_stop(StopReason::VerticalReachLost); return;
    }
    if (owner.row->lateral_distance() > lateral_reach_m()) {
        owner.request_stop(StopReason::LateralReachLost); return;
    }
    if (!owner.row->traction_ok()) { owner.request_stop(StopReason::TractionLost); return; }
    if (owner.state.elapsed_seconds + double(dt) > duration_seconds() + 1.0e-8) {
        owner.request_stop(StopReason::Expired); return;
    }
    const double previous = owner.row->rest_distance();
    const double stroke_left = double(owner.requested_stroke_m) -
        (previous - double(owner.state.initial_distance_m));
    const double budget_left = command_budget_j() - owner.state.command_work_bound_j;
    if (stroke_left <= 0) { owner.request_stop(StopReason::StrokeExhausted); return; }
    if (budget_left <= 0) { owner.request_stop(StopReason::CommandBudgetExhausted); return; }
    const double force = force_bound_n();
    const double allowed = std::min({
        double(owner.requested_stroke_m) / duration_seconds() * dt,
        double(command_power_bound_w()) * dt / force,
        budget_left / force, stroke_left});
    float next = static_cast<float>(previous + allowed);
    // Float rounding must never credit an increment that exceeds the real
    // rate/power/work/stroke allowance. Debit the actual stored float target.
    while (double(next) - previous > allowed)
        next = std::nextafter(next, static_cast<float>(previous));
    const double increment = std::max(0.0, double(next) - previous);
    owner.state.target_storage_delta_j = nominal_storage(next, owner.row->distance()) -
        nominal_storage(static_cast<float>(previous), owner.row->distance());
    owner.row->set_rest_distance(next);
    owner.state.last_command_work_bound_j = force * increment;
    owner.state.command_work_bound_j += owner.state.last_command_work_bound_j;
    owner.state.elapsed_seconds += double(dt);
    owner.sample_geometry();
}

void PhysicalFootPush::post_step(float dt) {
    auto &owner = *impl_;
    if (!owner.row) return;
    if (!std::isfinite(dt) || dt <= 0) owner.request_stop(StopReason::InvalidTimeStep);
    if (!owner.row->bodies_valid()) owner.request_stop(StopReason::InvalidBody);
    else if (owner.row->update_geometry()) owner.sample_geometry();
    else owner.request_stop(StopReason::InvalidGeometry);
    owner.sample_impulse();
    if (owner.state.stop_requested) {
        const auto reason = owner.state.stop_reason;
        clear(reason); // Manager mutation only outside PhysicsSystem::Update.
    }
}

bool PhysicalFootPush::active() const noexcept { return impl_->state.active; }
bool PhysicalFootPush::stop_requested() const noexcept { return impl_->state.stop_requested; }
const PhysicalFootPush::Readback &PhysicalFootPush::readback() const noexcept {
    return impl_->state;
}

} // namespace scraperx::sim
#endif // SCRAPERX_HAS_JOLT
