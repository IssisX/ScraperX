#pragma once

// The kit's rope: two bodies joined over two fixed sheaves, its length
// |p1 - f1| + ratio |p2 - f2| (p the ends on the bodies, f the sheaves). It
// pulls only, never letting the length past its maximum; a strut, given a
// minimum, pushes only, never letting it under.
//
// Jolt's PulleyConstraint, which the kit used before, is solved only in a
// step that begins with the rope at or past a limit: a rope that begins a
// step slack by any amount is left out of that step whole. A loaded rope sits
// exactly at its length, and up the stack single-precision positions put that
// length in steps of 6.1e-5 m (2^-14 m, 512 to 1024 m up). A taut rope read a
// step short dropped its load for a whole tick (AS-010 O's cab fell free at g
// for 11 ms) and snatched it the next at twice the tension; which ticks did
// turned on rounding, so any change to the world moved them.
//
// This rope is solved in every step, speculatively, as Jolt solves contacts:
// a slack rope may run out the slack it has within the step and no more, so
// it takes up its load in the step it comes taut, however little it was
// slack. Stretched past its length by the solver's error, it is drawn back by
// the position solver as Jolt's was.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

namespace scraperx::sim::kit {

class RopeConstraint final : public JPH::TwoBodyConstraint {
public:
    JPH_OVERRIDE_NEW_DELETE

    // The settings as Jolt's pulley takes them: a negative minimum or
    // maximum length is the length as laid. A direct rope runs straight from
    // body1's point to body2's, over no sheave (the grappling hook's line):
    // its length |p1 - p2|, the fixed points unused.
    RopeConstraint(JPH::Body &body1, JPH::Body &body2, const JPH::PulleyConstraintSettings &settings,
                   bool direct = false);

    [[nodiscard]] JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User1; }
    void NotifyShapeChanged(const JPH::BodyID &body_id, JPH::Vec3Arg delta_com) override;
    void SetupVelocityConstraint(float delta_time) override;
    void ResetWarmStart() override;
    void WarmStartVelocityConstraint(float warm_start_impulse_ratio) override;
    bool SolveVelocityConstraint(float delta_time) override;
    bool SolvePositionConstraint(float delta_time, float baumgarte) override;
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *renderer) const override;
#endif
    void SaveState(JPH::StateRecorder &stream) const override;
    void RestoreState(JPH::StateRecorder &stream) override;
    [[nodiscard]] JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override;
    [[nodiscard]] JPH::Mat44 GetConstraintToBody1Matrix() const override {
        return JPH::Mat44::sTranslation(local1_);
    }
    [[nodiscard]] JPH::Mat44 GetConstraintToBody2Matrix() const override {
        return JPH::Mat44::sTranslation(local2_);
    }

    [[nodiscard]] float GetMinLength() const noexcept { return min_length_; }
    [[nodiscard]] float GetMaxLength() const noexcept { return max_length_; }
    // Paid out or reeled in: the length the rope may run to from the next step.
    void SetMaxLength(const float length) noexcept { max_length_ = length; }
    // A winch on the rope (the grappling hook's reel): while pulling, it
    // takes the rope in at up to `rate` m/s with at most `force` N, over and
    // above holding it at its length; past that force it stalls. A rate of
    // 0 is off. The winch is solved before the rope's limit, each with its
    // own impulse, so a stalled winch still has the rope's full hold behind it.
    void SetMotor(const float rate, const float force) noexcept {
        motor_rate_ = rate;
        motor_force_ = force;
    }
    // The winch's impulse in the last step, N s (negative: pulling).
    [[nodiscard]] float GetMotorLambda() const noexcept { return motor_lambda_; }
    // The impulse along the rope in the last step, N s: negative pulling,
    // positive pushing.
    [[nodiscard]] float GetTotalLambdaPosition() const noexcept { return total_lambda_; }

private:
    enum class Side : unsigned char { Pull, Push, Rod };

    // World ends and normals from the bodies as they are now; the length.
    float update_geometry();
    // Lever arms and effective mass along the rope. False if neither end can
    // move along it.
    bool update_mass();
    // Body velocity along the rope, d(length)/dt.
    [[nodiscard]] float rate() const;
    bool apply_impulse(float lambda);

    JPH::Vec3 local1_;
    JPH::Vec3 local2_;
    JPH::RVec3 fixed1_;
    JPH::RVec3 fixed2_;
    float ratio_ = 1.0F;
    bool direct_ = false;
    float min_length_ = 0.0F;
    float max_length_ = 0.0F;

    JPH::RVec3 world1_;
    JPH::RVec3 world2_;
    // Kept from the last step when an end sits on its sheave.
    JPH::Vec3 normal1_ = -JPH::Vec3::sAxisY();
    JPH::Vec3 normal2_ = -JPH::Vec3::sAxisY();
    JPH::Vec3 r1xn1_ = JPH::Vec3::sZero();
    JPH::Vec3 inv_i1_r1xn1_ = JPH::Vec3::sZero();
    JPH::Vec3 ratio_r2xn2_ = JPH::Vec3::sZero();
    JPH::Vec3 inv_i2_ratio_r2xn2_ = JPH::Vec3::sZero();
    float effective_mass_ = 0.0F;
    Side side_ = Side::Pull;
    // d(length)/dt this step may reach (Pull) or fall to (Push) before the
    // rope acts: the slack it may run out, over the step.
    float allowed_rate_ = 0.0F;
    float total_lambda_ = 0.0F;
    float motor_rate_ = 0.0F;
    float motor_force_ = 0.0F;
    float motor_impulse_max_ = 0.0F;
    float motor_lambda_ = 0.0F;
};

} // namespace scraperx::sim::kit
