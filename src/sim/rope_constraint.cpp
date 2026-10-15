#include "sim/rope_constraint.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/StateRecorder.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif

#include <algorithm>
#include <limits>

// The solver arithmetic follows Jolt's IndependentAxisConstraintPart (MIT,
// Jorrit Rouwe), with a velocity bias it does not take: the slack's.

namespace scraperx::sim::kit {

RopeConstraint::RopeConstraint(JPH::Body &body1, JPH::Body &body2, const JPH::PulleyConstraintSettings &settings,
                               const bool direct)
    : JPH::TwoBodyConstraint(body1, body2, settings), fixed1_(settings.mFixedPoint1), fixed2_(settings.mFixedPoint2),
      ratio_(direct ? 1.0F : settings.mRatio), direct_(direct), min_length_(settings.mMinLength),
      max_length_(settings.mMaxLength) {
    if (settings.mSpace == JPH::EConstraintSpace::WorldSpace) {
        local1_ = JPH::Vec3(body1.GetInverseCenterOfMassTransform() * settings.mBodyPoint1);
        local2_ = JPH::Vec3(body2.GetInverseCenterOfMassTransform() * settings.mBodyPoint2);
    } else {
        local1_ = JPH::Vec3(settings.mBodyPoint1);
        local2_ = JPH::Vec3(settings.mBodyPoint2);
    }
    const float length = update_geometry();
    if (min_length_ < 0.0F) {
        min_length_ = length;
    }
    if (max_length_ < 0.0F) {
        max_length_ = length;
    }
}

float RopeConstraint::update_geometry() {
    world1_ = mBody1->GetCenterOfMassTransform() * local1_;
    world2_ = mBody2->GetCenterOfMassTransform() * local2_;
    if (direct_) {
        // Straight between the ends: d(length)/dt = n . (v1 - v2), so the
        // ends' normals are n and -n and the pulley's solver is unchanged.
        const JPH::Vec3 run(world1_ - world2_);
        const float length = run.Length();
        if (length > 0.0F) {
            normal1_ = run / length;
            normal2_ = -normal1_;
        }
        return length;
    }
    const JPH::Vec3 run1(world1_ - fixed1_);
    const JPH::Vec3 run2(world2_ - fixed2_);
    const float length1 = run1.Length();
    const float length2 = run2.Length();
    if (length1 > 0.0F) {
        normal1_ = run1 / length1;
    }
    if (length2 > 0.0F) {
        normal2_ = run2 / length2;
    }
    return length1 + ratio_ * length2;
}

bool RopeConstraint::update_mass() {
    float inverse_mass = 0.0F;
    r1xn1_ = inv_i1_r1xn1_ = ratio_r2xn2_ = inv_i2_ratio_r2xn2_ = JPH::Vec3::sZero();
    if (!mBody1->IsStatic()) {
        const JPH::MotionProperties *motion = mBody1->GetMotionProperties();
        r1xn1_ = JPH::Vec3(world1_ - mBody1->GetCenterOfMassPosition()).Cross(normal1_);
        inv_i1_r1xn1_ = motion->MultiplyWorldSpaceInverseInertiaByVector(mBody1->GetRotation(), r1xn1_);
        inverse_mass += motion->GetInverseMass() + inv_i1_r1xn1_.Dot(r1xn1_);
    }
    if (!mBody2->IsStatic()) {
        const JPH::MotionProperties *motion = mBody2->GetMotionProperties();
        ratio_r2xn2_ = ratio_ * JPH::Vec3(world2_ - mBody2->GetCenterOfMassPosition()).Cross(normal2_);
        inv_i2_ratio_r2xn2_ = motion->MultiplyWorldSpaceInverseInertiaByVector(mBody2->GetRotation(), ratio_r2xn2_);
        inverse_mass += ratio_ * ratio_ * motion->GetInverseMass() + inv_i2_ratio_r2xn2_.Dot(ratio_r2xn2_);
    }
    effective_mass_ = inverse_mass > 0.0F ? 1.0F / inverse_mass : 0.0F;
    return effective_mass_ > 0.0F;
}

float RopeConstraint::rate() const {
    return normal1_.Dot(mBody1->GetLinearVelocity()) + r1xn1_.Dot(mBody1->GetAngularVelocity()) +
           ratio_ * normal2_.Dot(mBody2->GetLinearVelocity()) + ratio_r2xn2_.Dot(mBody2->GetAngularVelocity());
}

bool RopeConstraint::apply_impulse(const float lambda) {
    if (lambda == 0.0F) {
        return false;
    }
    if (mBody1->IsDynamic()) {
        JPH::MotionProperties *motion = mBody1->GetMotionProperties();
        motion->AddLinearVelocityStep((motion->GetInverseMass() * lambda) * normal1_);
        motion->AddAngularVelocityStep(inv_i1_r1xn1_ * lambda);
    }
    if (mBody2->IsDynamic()) {
        JPH::MotionProperties *motion = mBody2->GetMotionProperties();
        motion->AddLinearVelocityStep((ratio_ * motion->GetInverseMass() * lambda) * normal2_);
        motion->AddAngularVelocityStep(inv_i2_ratio_r2xn2_ * lambda);
    }
    return true;
}

void RopeConstraint::NotifyShapeChanged(const JPH::BodyID &body_id, const JPH::Vec3Arg delta_com) {
    if (mBody1->GetID() == body_id) {
        local1_ -= delta_com;
    } else if (mBody2->GetID() == body_id) {
        local2_ -= delta_com;
    }
}

void RopeConstraint::SetupVelocityConstraint(const float delta_time) {
    const float length = update_geometry();
    // The limit the rope is nearer: a rope works at its maximum, a strut at
    // its minimum.
    Side side = Side::Rod;
    if (min_length_ < max_length_) {
        side = max_length_ - length <= length - min_length_ ? Side::Pull : Side::Push;
    }
    if (side != side_) {
        total_lambda_ = 0.0F;
        side_ = side;
    }
    switch (side_) {
    case Side::Pull:
        allowed_rate_ = std::max(0.0F, max_length_ - length) / delta_time;
        break;
    case Side::Push:
        allowed_rate_ = -std::max(0.0F, length - min_length_) / delta_time;
        break;
    case Side::Rod:
        allowed_rate_ = 0.0F;
        break;
    }
    if (!update_mass()) {
        total_lambda_ = 0.0F;
    }
    // The winch pulls only on a rope working at its length.
    motor_impulse_max_ = motor_force_ * delta_time;
    if (motor_rate_ <= 0.0F || side_ != Side::Pull || effective_mass_ == 0.0F) {
        motor_lambda_ = 0.0F;
    }
}

void RopeConstraint::ResetWarmStart() {
    total_lambda_ = 0.0F;
    motor_lambda_ = 0.0F;
}

void RopeConstraint::WarmStartVelocityConstraint(const float warm_start_impulse_ratio) {
    total_lambda_ *= warm_start_impulse_ratio;
    motor_lambda_ *= warm_start_impulse_ratio;
    (void)apply_impulse(total_lambda_ + motor_lambda_);
}

bool RopeConstraint::SolveVelocityConstraint(const float /*delta_time*/) {
    if (effective_mass_ == 0.0F) {
        return false;
    }
    bool applied = false;
    if (motor_rate_ > 0.0F && side_ == Side::Pull) {
        // Toward d(length)/dt = -rate, with no more than the winch's force.
        const float motor = std::clamp(motor_lambda_ - effective_mass_ * (rate() + motor_rate_), -motor_impulse_max_, 0.0F);
        applied = apply_impulse(motor - motor_lambda_);
        motor_lambda_ = motor;
    }
    constexpr float kNone = std::numeric_limits<float>::max();
    const float low = side_ == Side::Push ? 0.0F : -kNone;
    const float high = side_ == Side::Pull ? 0.0F : kNone;
    // lambda = -K^-1 (J v - allowed): nothing while the rope runs out no
    // more than its slack this step.
    const float total = std::clamp(total_lambda_ - effective_mass_ * (rate() - allowed_rate_), low, high);
    const float lambda = total - total_lambda_;
    total_lambda_ = total;
    return apply_impulse(lambda) || applied;
}

bool RopeConstraint::SolvePositionConstraint(const float /*delta_time*/, const float baumgarte) {
    const float length = update_geometry();
    float error = 0.0F;
    if (length < min_length_) {
        error = length - min_length_;
    } else if (length > max_length_) {
        error = length - max_length_;
    }
    if (error == 0.0F || !update_mass()) {
        return false;
    }
    // Split impulse: moved, not accelerated (Catto, GDC 2007), as Jolt does.
    const float lambda = -effective_mass_ * baumgarte * error;
    if (mBody1->IsDynamic()) {
        mBody1->AddPositionStep((lambda * mBody1->GetMotionPropertiesUnchecked()->GetInverseMass()) * normal1_);
        mBody1->AddRotationStep(lambda * inv_i1_r1xn1_);
    }
    if (mBody2->IsDynamic()) {
        mBody2->AddPositionStep((lambda * ratio_ * mBody2->GetMotionPropertiesUnchecked()->GetInverseMass()) *
                                normal2_);
        mBody2->AddRotationStep(lambda * inv_i2_ratio_r2xn2_);
    }
    return true;
}

#ifdef JPH_DEBUG_RENDERER
void RopeConstraint::DrawConstraint(JPH::DebugRenderer *renderer) const {
    renderer->DrawLine(world1_, fixed1_, JPH::Color::sGreen);
    renderer->DrawLine(fixed1_, fixed2_, JPH::Color::sGreen);
    renderer->DrawLine(fixed2_, world2_, JPH::Color::sGreen);
}
#endif

void RopeConstraint::SaveState(JPH::StateRecorder &stream) const {
    JPH::TwoBodyConstraint::SaveState(stream);
    stream.Write(total_lambda_);
    stream.Write(side_);
    stream.Write(normal1_);
    stream.Write(normal2_);
}

void RopeConstraint::RestoreState(JPH::StateRecorder &stream) {
    JPH::TwoBodyConstraint::RestoreState(stream);
    stream.Read(total_lambda_);
    stream.Read(side_);
    stream.Read(normal1_);
    stream.Read(normal2_);
}

JPH::Ref<JPH::ConstraintSettings> RopeConstraint::GetConstraintSettings() const {
    // Debug only (Jolt's object stream): it reads back as Jolt's pulley.
    auto *settings = new JPH::PulleyConstraintSettings;
    ToConstraintSettings(*settings);
    settings->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
    settings->mBodyPoint1 = JPH::RVec3(local1_);
    settings->mFixedPoint1 = fixed1_;
    settings->mBodyPoint2 = JPH::RVec3(local2_);
    settings->mFixedPoint2 = fixed2_;
    settings->mRatio = ratio_;
    settings->mMinLength = min_length_;
    settings->mMaxLength = max_length_;
    return settings;
}

} // namespace scraperx::sim::kit
