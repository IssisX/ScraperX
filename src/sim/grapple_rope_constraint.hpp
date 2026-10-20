#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <algorithm>
#include <cmath>

namespace scraperx::sim {
// Finite unilateral two-body rope. Body1 attaches at COM; body2 at its real eye.
// Add/remove outside PhysicsSystem::Update. Jolt sets up this row per collision
// substep; it already applies the reciprocal force and body2 eye torque.
// Release removes the constraint without assigning either endpoint velocity.
class GrappleRopeSettings final : public JPH::TwoBodyConstraintSettings {
    JPH_DECLARE_SERIALIZABLE_VIRTUAL(, GrappleRopeSettings)
public:
    static constexpr float stiffness_n_per_m = 6000.0F;
    static constexpr float damping_ns_per_m = 180.0F;
    static constexpr float strength_n = 3000.0F;
    static constexpr float reel_force_n = 1500.0F;
    // Published API: mLocalEye is relative to body2 COM, in body coordinates.
    // Installations specified at a shape-origin local eye convert exactly once.
    void set_shape_local_eye(const JPH::Body &support, JPH::Vec3Arg eye) {
        mLocalEye = eye - support.GetShape()->GetCenterOfMass();
    }
    JPH::Vec3 mLocalEye = JPH::Vec3::sZero();
    float mRestLength = 0;
    JPH::TwoBodyConstraint *Create(JPH::Body &player, JPH::Body &hook) const override;
    void SaveBinaryState(JPH::StreamOut &stream) const override {
        ConstraintSettings::SaveBinaryState(stream);
        stream.Write(mLocalEye); stream.Write(mRestLength);
    }
protected:
    void RestoreBinaryState(JPH::StreamIn &stream) override {
        ConstraintSettings::RestoreBinaryState(stream);
        stream.Read(mLocalEye); stream.Read(mRestLength);
    }
};
class GrappleRope final : public JPH::TwoBodyConstraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    GrappleRope(JPH::Body &player, JPH::Body &hook, const GrappleRopeSettings &settings)
        : TwoBodyConstraint(player,hook,settings), local_eye_(settings.mLocalEye), rest_(settings.mRestLength) {}
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User2; }
    JPH::Mat44 GetConstraintToBody1Matrix() const override { return JPH::Mat44::sIdentity(); }
    JPH::Mat44 GetConstraintToBody2Matrix() const override { return JPH::Mat44::sTranslation(local_eye_); }
    void NotifyShapeChanged(const JPH::BodyID &id, JPH::Vec3Arg delta) override {
        if (id == mBody2->GetID()) local_eye_ -= delta;
        ResetWarmStart();
    }
    void set_rest(float rest) { rest_=rest; }
    void set_drive_limited(bool limited) { drive_limited_=limited; }
    float rest() const { return rest_; }
    // Same implicit spring/damper demand used by Setup, including the rotating
    // hook eye's point velocity and reciprocal angular effective mass.
    float prospective_tension(float rest, float dt) const {
        using namespace JPH;
        const RVec3 eye=mBody2->GetCenterOfMassTransform()*local_eye_;
        const Vec3 separation(eye-mBody1->GetCenterOfMassPosition());
        const float radius=separation.Length();
        if (radius<rest || radius<1.0e-6F) return 0;
        const Vec3 axis=separation/radius;
        const Vec3 torque_axis=Vec3(eye-mBody2->GetCenterOfMassPosition()).Cross(axis);
        const auto *motion=mBody2->IsStatic() ? nullptr : mBody2->GetMotionProperties();
        const float inverse_mass=mBody1->GetMotionProperties()->GetInverseMass()+
            (mBody2->IsDynamic() ? motion->GetInverseMass() : 0)+
            (mBody2->IsDynamic() ? torque_axis.Dot(motion->MultiplyWorldSpaceInverseInertiaByVector(mBody2->GetRotation(),torque_axis)) : 0);
        const float radial_speed=(mBody2->GetPointVelocity(eye)-mBody1->GetLinearVelocity()).Dot(axis);
        const float implicit_damping=GrappleRopeSettings::damping_ns_per_m+dt*GrappleRopeSettings::stiffness_n_per_m;
        const float demand=(GrappleRopeSettings::stiffness_n_per_m*(radius-rest)+implicit_damping*radial_speed)/
            (1+dt*implicit_damping*inverse_mass);
        return std::isfinite(demand) ? std::max(0.0F,demand) : demand;
    }
    void inhibit() { permitted_=false; ResetWarmStart(); }
    bool broken() const { return broken_; }
    float tension() const {
        return solved_ && row_.IsActive() && dt_>0 ? std::max(0.0F,-row_.GetTotalLambda()/dt_) : 0;
    }
    void SetupVelocityConstraint(float dt) override {
        using namespace JPH;
        solved_=false;
        dt_=dt;
        const RVec3 eye=mBody2->GetCenterOfMassTransform()*local_eye_;
        const Vec3 separation(eye-mBody1->GetCenterOfMassPosition());
        const float radius=separation.Length();
        if (!permitted_ || broken_ || !std::isfinite(dt) || dt<=0 ||
            !finite(separation) || !std::isfinite(rest_) || rest_<=0 || radius<rest_ || radius<1.0e-6F) {
            ResetWarmStart(); return;
        }
        axis_=separation/radius;
        const float force_limit=drive_limited_ ? GrappleRopeSettings::reel_force_n : GrappleRopeSettings::strength_n;
        cap_=force_limit*dt;
        if (double(cap_)/dt>force_limit) cap_=std::nextafter(cap_,0.0F);
        const Vec3 arm(eye-mBody2->GetCenterOfMassPosition());
        const float extension=radius-rest_;
        const float demand=prospective_tension(rest_,dt);
        // The cap is numerical protection, not unlimited extension assistance.
        // Either excess elastic stretch or excess constitutive demand parts the
        // line before warmstart/solve; post_step removes the registered row.
        if (!std::isfinite(demand) || extension>GrappleRopeSettings::strength_n/GrappleRopeSettings::stiffness_n_per_m ||
            demand>GrappleRopeSettings::strength_n) {
            broken_=true; ResetWarmStart(); return;
        }
        row_.CalculateConstraintPropertiesWithStiffnessAndDamping(dt,*mBody1,separation,*mBody2,arm,
            axis_,0,extension,GrappleRopeSettings::stiffness_n_per_m,GrappleRopeSettings::damping_ns_per_m);
    }
    void ResetWarmStart() override {
        row_.Deactivate(); axis_=JPH::Vec3::sZero(); dt_=cap_=0; solved_=false;
    }
    void WarmStartVelocityConstraint(float ratio) override {
        if (!row_.IsActive()) return;
        const float lambda=row_.GetTotalLambda();
        if (!std::isfinite(lambda) || lambda>0) {
            const float dt=dt_; ResetWarmStart(); SetupVelocityConstraint(dt); ratio=0;
        }
        if (!std::isfinite(ratio) || ratio<0) ratio=0;
        if (lambda<0) {
            ratio=std::min(ratio,cap_/(-lambda));
            if ((-lambda)*ratio>cap_) ratio=std::nextafter(ratio,0.0F);
        }
        if (row_.IsActive()) row_.WarmStart(*mBody1,*mBody2,axis_,ratio);
    }
    bool SolveVelocityConstraint(float) override {
        if (!row_.IsActive()) return false;
        solved_=true;
        return row_.SolveVelocityConstraint(*mBody1,*mBody2,axis_,-cap_,0);
    }
    bool SolvePositionConstraint(float,float) override { return false; }
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override {
        auto *settings=new GrappleRopeSettings;
        ToConstraintSettings(*settings); settings->mLocalEye=local_eye_; settings->mRestLength=rest_;
        return settings;
    }
    void SaveState(JPH::StateRecorder &stream) const override {
        TwoBodyConstraint::SaveState(stream); row_.SaveState(stream);
        stream.Write(local_eye_); stream.Write(rest_); stream.Write(axis_);
        stream.Write(dt_); stream.Write(cap_); stream.Write(solved_); stream.Write(permitted_); stream.Write(broken_);
        stream.Write(drive_limited_);
    }
    void RestoreState(JPH::StateRecorder &stream) override {
        TwoBodyConstraint::RestoreState(stream); row_.RestoreState(stream);
        stream.Read(local_eye_); stream.Read(rest_); stream.Read(axis_);
        stream.Read(dt_); stream.Read(cap_); stream.Read(solved_); stream.Read(permitted_); stream.Read(broken_);
        stream.Read(drive_limited_);
    }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *) const override {}
#endif
private:
    static bool finite(JPH::Vec3Arg value) {
        return std::isfinite(value.GetX()) && std::isfinite(value.GetY()) && std::isfinite(value.GetZ());
    }
    JPH::Vec3 local_eye_, axis_=JPH::Vec3::sZero();
    JPH::AxisConstraintPart row_;
    float rest_, dt_=0, cap_=0;
    bool permitted_=true, broken_=false, solved_=false, drive_limited_=false;
};
} // namespace scraperx::sim
