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
                    JPH::Vec3 world_normal, float friction, bool planted_geometry)
        : TwoBodyConstraint(support, player, settings),
          local_anchor_(JPH::Vec3(support.GetInverseCenterOfMassTransform() * settings.mPoint1)),
          local_normal_(support.GetRotation().Conjugated() * world_normal),
          rest_distance_(settings.mMinDistance), friction_(friction),
          planted_geometry_(planted_geometry) {}

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
        const JPH::Vec3 r1_plus_u(player_point_ - mBody1->GetCenterOfMassPosition());
        if (force_motor_) {
            axis_part_.Deactivate(); // No motor impulse is cached or warm-started.
            motor_lever_cross_ = r1_plus_u.Cross(axis_);
            if (motor_drive_ && !motor_applied_)
                axis_part_.CalculateConstraintProperties(*mBody1, r1_plus_u,
                    *mBody2, JPH::Vec3::sZero(), axis_);
            return;
        }
        if (distance_ >= rest_distance_) {
            axis_part_.Deactivate(); // Never pull the player back to the support.
            return;
        }
        // u is parallel to axis_, so (r1+u) x axis_ = r1 x axis_. Body2
        // has r2=0: equal/opposite reaction and zero leg torque at player COM.
        axis_part_.CalculateConstraintPropertiesWithStiffnessAndDamping(
            dt, *mBody1, r1_plus_u, *mBody2, JPH::Vec3::sZero(), axis_,
            0.0F, distance_ - rest_distance_, PhysicalFootPush::stiffness_n_per_m(),
            PhysicalFootPush::damping_ns_per_m());
    }
    void ResetWarmStart() override { axis_part_.Deactivate(); }
    void WarmStartVelocityConstraint(float ratio) override {
        if (force_motor_) return;
        if (!axis_part_.IsActive() || !std::isfinite(ratio) || ratio < 0) return;
        // AxisConstraintPart's warm start itself is unclamped. Limit its
        // scale so the cached positive impulse also obeys the CURRENT dt cap.
        const float previous = axis_part_.GetTotalLambda();
        if (previous > 0) ratio = std::min(ratio, max_lambda_ / previous);
        axis_part_.WarmStart(*mBody1, *mBody2, axis_, ratio);
    }
    bool SolveVelocityConstraint(float) override {
        if (force_motor_) {
            if (!motor_drive_ || motor_applied_ || !axis_part_.IsActive()) return false;
            motor_applied_ = true;
            const auto *player = mBody2->GetMotionPropertiesUnchecked();
            JPH::Vec3 support_velocity = JPH::Vec3::sZero();
            float relative = 0;
            double inverse_mass = player->GetInverseMass();
            if (!mBody1->IsStatic()) {
                const auto *support = mBody1->GetMotionPropertiesUnchecked();
                support_velocity = support->GetLinearVelocity();
                relative = motor_lever_cross_.Dot(support->GetAngularVelocity());
                if (mBody1->IsDynamic())
                    inverse_mass += support->GetInverseMass() + motor_lever_cross_.Dot(
                        support->MultiplyWorldSpaceInverseInertiaByVector(
                            mBody1->GetRotation(), motor_lever_cross_));
            }
            // Use the exact frozen Jacobian/order of AxisConstraintPart, with
            // opposite sign: positive u separates player and material support.
            relative += axis_.Dot(support_velocity - player->GetLinearVelocity());
            const double u = -double(relative);
            const double energy = std::max(0.0, std::min(
                double(PhysicalFootPush::command_power_bound_w()) * solve_dt_,
                PhysicalFootPush::command_budget_j() - motor_positive_work_));
            if (!(inverse_mass > 0) || !std::isfinite(inverse_mass) || !std::isfinite(u)) return false;
            const double speed = std::max(0.0, u);
            const double root = std::sqrt(speed * speed + 2 * inverse_mass * energy);
            const double power_cap = u >= 0 ? (energy > 0 ? 2 * energy / (root + u) : 0) :
                (-u + root) / inverse_mass;
            double requested_impulse = max_lambda_;
            if (walking_) {
                const double stiffness = PhysicalFootPush::stiffness_n_per_m();
                const double damping = 2 * std::sqrt(stiffness / inverse_mass);
                const auto delta = JPH::Vec3(player_point_-anchor_);
                const auto relative_velocity = player->GetLinearVelocity()-mBody1->GetPointVelocity(anchor_);
                const auto direction = mBody1->GetRotation()*walking_local_direction_;
                const double approach_speed = std::max(0.0F,relative_velocity.Dot(direction));
                // Approach follows material-relative progress. An already
                // blocking, validated low riser permits the requested finite
                // lift without waiting for impossible forward travel.
                const double response = std::sqrt(1/(stiffness*inverse_mass));
                const auto local_delta = mBody1->GetRotation().Conjugated()*delta;
                const double travelled = (local_delta-walking_local_start_).Dot(walking_local_direction_);
                const double phase = walking_committed_ ? 1.0 :
                    std::clamp((travelled+approach_speed*response)/walking_approach_m_,0.0,1.0);
                constexpr double half_pi = 1.5707963267948966;
                const double rise = walking_height_m_-walking_start_height_m_;
                const double target_height = walking_start_height_m_+rise*std::sin(half_pi*phase);
                const double target_speed = phase < 1 ?
                    rise*half_pi*std::cos(half_pi*phase)*approach_speed/walking_approach_m_ : 0;
                if (axis_.GetY() > .1F) {
                    const double force = std::clamp((walking_weight_n_+stiffness*(target_height-delta.GetY())+
                        damping*(target_speed-relative_velocity.GetY()))/axis_.GetY(),0.0,
                        double(PhysicalFootPush::force_bound_n()));
                    double vertical_inverse = player->GetInverseMass()*axis_.GetY();
                    if (mBody1->IsDynamic()) {
                        const auto *support = mBody1->GetMotionPropertiesUnchecked();
                        const auto lever = JPH::Vec3(anchor_-mBody1->GetCenterOfMassPosition());
                        const auto rotation = support->MultiplyWorldSpaceInverseInertiaByVector(
                            mBody1->GetRotation(),lever.Cross(axis_));
                        vertical_inverse += support->GetInverseMass()*axis_.GetY()+rotation.Cross(lever).GetY();
                    }
                    // This push-only leg cannot brake by pulling downward.
                    // Limit effort with a gravity stopping estimate; actual
                    // moving-support/contact forces still decide the outcome.
                    const double remaining = std::max(0.0,double(walking_height_m_)-delta.GetY());
                    const double gravity = walking_weight_n_*player->GetInverseMass();
                    const double stopping_speed = std::sqrt(2*gravity*remaining);
                    const double stopping_impulse = vertical_inverse > 0 ? std::max(0.0,
                        (stopping_speed-relative_velocity.GetY())/vertical_inverse) : 0;
                    requested_impulse = std::min(force*solve_dt_,stopping_impulse);
                } else requested_impulse = 0;
            }
            float impulse = static_cast<float>(std::min(requested_impulse, power_cap));
            const auto positive_work = [&](float value) {
                const double after = std::max(0.0, u + inverse_mass * value);
                return std::max(0.0, (after * after - speed * speed) / (2 * inverse_mass));
            };
            while (double(impulse) > power_cap || positive_work(impulse) > energy)
                impulse = std::nextafter(impulse, 0.0F);
            const double after = std::min(0.0, u + inverse_mass * impulse);
            const double before = std::min(0.0, u);
            last_motor_work_ = positive_work(impulse);
            motor_positive_work_ += last_motor_work_;
            motor_absorbed_work_ += std::max(0.0,
                (before * before - after * after) / (2 * inverse_mass));
            motor_impulse_ = impulse;
            // Exactly one admitted impulse, through Jolt's existing reciprocal
            // Jacobian. Later solver iterations cannot repeat or refund it.
            return axis_part_.SolveVelocityConstraint(*mBody1, *mBody2, axis_, impulse, impulse);
        }
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
    // Read-only activation validation: a refused command must not even change
    // the geometry/impulse cache of the retained row. Bodies are locked by caller.
    bool valid_for_activation(float initial_distance) const noexcept {
        if (!GetEnabled() || !bodies_valid()) return false;
        const JPH::RVec3 anchor = mBody1->GetCenterOfMassTransform() * local_anchor_;
        const JPH::RVec3 player = mBody2->GetCenterOfMassPosition();
        const JPH::Vec3 normal = mBody1->GetRotation() * local_normal_;
        const JPH::Vec3 delta(player - anchor);
        const float distance = delta.Length();
        if (!finite(anchor) || !finite(player) || !finite(normal) ||
            !std::isfinite(distance) || distance < 1.0e-4F ||
            distance > initial_distance + PhysicalFootPush::maximum_stroke_m() + 1.0e-4F)
            return false;
        const JPH::Vec3 axis = delta / distance;
        const float height = delta.Dot(normal), axial_normal = axis.Dot(normal);
        return (planted_geometry_ ||
                (delta - height * normal).Length() <= PhysicalFootPush::lateral_reach_m()) &&
            axial_normal > 0 &&
            (axis - axial_normal * normal).Length() <= friction_ * axial_normal;
    }
    void refresh_walking_normal(JPH::Vec3 normal) noexcept {
        local_normal_ = mBody1->GetRotation().Conjugated()*normal;
    }
    void disable() noexcept {
        SetEnabled(false);
        ResetWarmStart();
        motor_drive_ = false;
        motor_impulse_ = 0;
        last_motor_work_ = 0;
    }
    float impulse() const noexcept { return force_motor_ ? motor_impulse_ : axis_part_.GetTotalLambda(); }
    float impulse_dt() const noexcept { return solve_dt_; }
    JPH::Vec3 impulse_axis() const noexcept { return solve_axis_; }
    JPH::Vec3 surface_normal() const noexcept { return normal_; }
    float distance() const noexcept { return distance_; }
    float rest_distance() const noexcept { return rest_distance_; }
    void set_rest_distance(float value) noexcept { rest_distance_ = value; }
    void activate_motor() noexcept {
        motor_impulse_ = axis_part_.GetTotalLambda();
        force_motor_ = true;
        motor_drive_ = false;
    }
    void activate_walking(float goal_height, float weight_n, JPH::Vec3 direction, float approach) noexcept {
        activate_motor();
        walking_ = true;
        walking_committed_ = false;
        walking_weight_n_ = weight_n;
        walking_height_m_ = goal_height;
        const auto delta = JPH::Vec3(player_point_-anchor_);
        walking_start_height_m_ = delta.GetY();
        walking_local_start_ = mBody1->GetRotation().Conjugated()*delta;
        walking_local_direction_ = mBody1->GetRotation().Conjugated()*direction;
        walking_approach_m_ = approach;
    }
    void begin_motor_step(bool drive) noexcept {
        motor_drive_ = drive;
        motor_applied_ = false;
        motor_impulse_ = 0;
        last_motor_work_ = 0;
    }
    void hold_motor() noexcept { motor_drive_ = false; }
    void push_off_step() noexcept { walking_ = false; }
    void commit_walking_step() noexcept { walking_committed_ = true; }
    bool motor_profile() const noexcept { return force_motor_; }
    double motor_positive_work() const noexcept { return motor_positive_work_; }
    double motor_absorbed_work() const noexcept { return motor_absorbed_work_; }
    double last_motor_work() const noexcept { return last_motor_work_; }
    float lateral_distance() const noexcept { return lateral_distance_; }
    bool lateral_reach_ok() const noexcept {
        return planted_geometry_ || lateral_distance_ <= PhysicalFootPush::lateral_reach_m();
    }
    bool traction_ok() const noexcept { return traction_ok_; }
    JPH::RVec3 anchor() const noexcept { return anchor_; }
    JPH::Vec3 axis() const noexcept { return axis_; }

private:
    JPH::Vec3 local_anchor_, local_normal_;
    float rest_distance_;
    float friction_;
    bool planted_geometry_;
    bool force_motor_ = false, motor_drive_ = false, motor_applied_ = false;
    bool walking_ = false, walking_committed_ = false;
    float walking_weight_n_ = 0;
    float walking_height_m_ = 0, walking_start_height_m_ = 0, walking_approach_m_ = 1;
    JPH::Vec3 walking_local_start_ = JPH::Vec3::sZero();
    JPH::Vec3 walking_local_direction_ = JPH::Vec3::sAxisX();
    JPH::Vec3 motor_lever_cross_ = JPH::Vec3::sZero();
    float motor_impulse_ = 0;
    double motor_positive_work_ = 0, motor_absorbed_work_ = 0, last_motor_work_ = 0;
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
        state.world_surface_normal = row->surface_normal();
        state.leg_axis = row->axis();
        state.actual_distance_m = row->distance();
        state.rest_distance_m = row->rest_distance();
        state.commanded_stroke_m = state.rest_distance_m - state.initial_distance_m;
        state.nominal_spring_storage_j = row->motor_profile() ? 0 : nominal_storage(
            state.rest_distance_m, state.actual_distance_m);
        if (row->motor_profile())
            state.actual_motor_extension_m = std::max(0.0F, state.actual_distance_m - state.initial_distance_m);
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
    void sample_motor(bool last_step) noexcept {
        if (!row->motor_profile()) return;
        state.motor_positive_work_j = row->motor_positive_work();
        state.motor_absorbed_work_j = row->motor_absorbed_work();
        state.command_work_bound_j = state.motor_positive_work_j;
        if (last_step) state.last_command_work_bound_j = row->last_motor_work();
    }
};

PhysicalFootPush::PhysicalFootPush(JPH::PhysicsSystem &system, JPH::BodyID player)
    : impl_(std::make_unique<Impl>(system, player)) {}
PhysicalFootPush::~PhysicalFootPush() { clear(); }

bool PhysicalFootPush::begin(JPH::BodyID support, JPH::RVec3 point,
                            JPH::Vec3 normal, float friction, float requested_stroke_m) {
    return begin_contact(support, point, normal, friction, requested_stroke_m, false);
}

bool PhysicalFootPush::begin_contact(JPH::BodyID support, JPH::RVec3 point,
                                    JPH::Vec3 normal, float friction,
                                    float requested_stroke_m, bool planted_geometry) {
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
        candidate = new FootDistanceRow(*hold, *player, settings, normal, friction, planted_geometry);
        if (!candidate->update_geometry() || !candidate->traction_ok() ||
            !candidate->lateral_reach_ok()) return false;
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

bool PhysicalFootPush::begin_passive_plant(JPH::BodyID support, JPH::RVec3 point,
                                          JPH::Vec3 normal, float friction) {
    if (!begin_contact(support, point, normal, friction, maximum_stroke_m(), true)) return false;
    hold_target();
    impl_->state.passive_plant = true;
    return true;
}

bool PhysicalFootPush::begin_walking_step(JPH::BodyID support, JPH::RVec3 point,
                                        JPH::Vec3 normal, float friction, float rise,
                                        JPH::Vec3 direction, float approach) {
    if (!std::isfinite(rise) || rise <= 0 || !finite(direction) ||
        std::abs(direction.LengthSq()-1) > 1.0e-4F ||
        !std::isfinite(approach) || approach <= 0) return false;
    const float stroke = std::min(rise,maximum_stroke_m());
    if (!begin_contact(support, point, normal, friction, stroke, false)) return false;
    auto &owner = *impl_;
    const JPH::BodyLockRead lock(owner.system.GetBodyLockInterface(), owner.player);
    if (!lock.Succeeded()) { clear(StopReason::InvalidBody); return false; }
    const float inverse_mass = lock.GetBody().GetMotionProperties()->GetInverseMass();
    const float weight = -owner.system.GetGravity().GetY() / inverse_mass;
    const float height = JPH::Vec3(lock.GetBody().GetCenterOfMassPosition()-point).GetY()+rise;
    owner.row->activate_walking(height, weight, direction, approach);
    owner.state.walking_step = true;
    owner.sample_geometry();
    return true;
}

bool PhysicalFootPush::activate_planted_push() {
    auto &owner = *impl_;
    const auto &state = owner.state;
    if (!owner.row || !state.active || state.stop_requested || !state.passive_plant ||
        !state.target_held || state.commanded_stroke_m != 0 || state.command_work_bound_j != 0 ||
        state.elapsed_seconds >= duration_seconds()) return false;
    const JPH::BodyID ids[]{state.support, owner.player};
    JPH::BodyLockMultiRead lock(owner.system.GetBodyLockInterface(), ids, 2);
    if (!lock.GetBody(0) || !lock.GetBody(1) ||
        !owner.row->valid_for_activation(state.initial_distance_m)) return false;
    owner.state.target_held = false;
    owner.state.passive_plant = false;
    owner.row->update_geometry();
    owner.state.retired_nominal_storage_j = nominal_storage(
        owner.row->rest_distance(), owner.row->distance());
    owner.row->activate_motor();
    owner.state.planted_motor_profile = true;
    owner.sample_geometry();
    return true;
}

bool PhysicalFootPush::commit_blocked_walking_step() {
    auto &owner = *impl_;
    if (!owner.row || !owner.state.active || owner.state.stop_requested ||
        !owner.state.walking_step || owner.state.elapsed_seconds != 0) return false;
    owner.row->commit_walking_step();
    return true;
}

bool PhysicalFootPush::push_off_existing_step() {
    auto &owner = *impl_;
    const auto &state = owner.state;
    if (!owner.row || !state.active || state.stop_requested || !state.walking_step ||
        state.target_held || state.elapsed_seconds >= duration_seconds()) return false;
    const JPH::BodyID ids[]{state.support, owner.player};
    JPH::BodyLockMultiRead lock(owner.system.GetBodyLockInterface(), ids, 2);
    if (!lock.GetBody(0) || !lock.GetBody(1) ||
        !owner.row->valid_for_activation(state.initial_distance_m)) return false;
    // Only the effort profile changes. No begin, activation, warm-start reset
    // or ledger reset is allowed at this phase boundary.
    owner.row->push_off_step();
    owner.state.walking_step = false;
    owner.state.planted_motor_profile = true;
    return true;
}

bool PhysicalFootPush::refresh_walking_contact_normal(JPH::Vec3 normal) {
    auto &owner = *impl_;
    if (!owner.row || !owner.state.active || !owner.state.walking_step ||
        !finite(normal) || normal.LengthSq() < 1.0e-8F) return false;
    const JPH::BodyLockRead lock(owner.system.GetBodyLockInterface(),owner.state.support);
    if (!lock.Succeeded()) return false;
    owner.row->refresh_walking_normal(normal.Normalized());
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

bool PhysicalFootPush::can_retain_contact() const noexcept {
    const auto &owner = *impl_;
    if (!owner.row || !owner.state.active || owner.state.stop_requested ||
        owner.state.elapsed_seconds >= duration_seconds()) return false;
    const JPH::BodyID ids[]{owner.state.support, owner.player};
    JPH::BodyLockMultiRead lock(owner.system.GetBodyLockInterface(), ids, 2);
    return lock.GetBody(0) && lock.GetBody(1) &&
        owner.row->valid_for_activation(owner.state.initial_distance_m);
}

void PhysicalFootPush::hold_target() noexcept {
    if (impl_->row && !impl_->state.stop_requested) {
        impl_->state.target_held = true;
        impl_->row->hold_motor();
    }
}

void PhysicalFootPush::pre_step(float dt, bool face_valid, bool traction_valid) noexcept {
    auto &owner = *impl_;
    owner.state.last_command_work_bound_j = 0;
    owner.state.target_storage_delta_j = 0;
    if (!owner.row || owner.state.stop_requested) return;
    // Read the PREVIOUS solved substep before target/geometry changes or a
    // stop resets its cached impulse. A peak is a max, never an impulse sum.
    owner.sample_impulse();
    owner.sample_motor(false);
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
    if (!owner.row->lateral_reach_ok()) {
        owner.request_stop(StopReason::LateralReachLost); return;
    }
    if (!owner.row->traction_ok()) { owner.request_stop(StopReason::TractionLost); return; }
    if (owner.state.elapsed_seconds + double(dt) > duration_seconds() + 1.0e-8) {
        owner.request_stop(StopReason::Expired); return;
    }
    if (owner.row->motor_profile()) {
        owner.row->begin_motor_step(!owner.state.target_held);
        owner.state.elapsed_seconds += double(dt);
        return;
    }
    if (owner.state.target_held) {
        owner.state.elapsed_seconds += double(dt);
        return;
    }
    const double previous = owner.row->rest_distance();
    const double stroke_left = double(owner.requested_stroke_m) -
        (previous - double(owner.state.initial_distance_m));
    const double budget_left = command_budget_j() - owner.state.command_work_bound_j;
    const double force = force_bound_n();
    if (stroke_left <= 0) { owner.request_stop(StopReason::StrokeExhausted); return; }
    if (budget_left <= 0) { owner.request_stop(StopReason::CommandBudgetExhausted); return; }
    const float distance = owner.row->distance();
    const double storage_before = nominal_storage(static_cast<float>(previous), distance);
    const double energy_limit = double(command_power_bound_w()) * dt;
    const double allowed = std::min({
        double(owner.requested_stroke_m) / duration_seconds() * dt,
        energy_limit / force, budget_left / force, stroke_left});
    float next = static_cast<float>(previous + allowed);
    // Debit and validate the actual stored float target, rounding downward.
    // Nominal storage admission is not complete implicit-solver energy closure.
    while (double(next) - previous > allowed)
        next = std::nextafter(next, static_cast<float>(previous));
    const double increment = std::max(0.0, double(next) - previous);
    owner.state.target_storage_delta_j = nominal_storage(next, distance) - storage_before;
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
    owner.sample_motor(true);
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
