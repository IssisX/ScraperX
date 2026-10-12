#include "sim/physical_hand_climb.hpp"
#include "sim/cargo_net.hpp"
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>
#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
constexpr float kAxisForce = 866.025390625F; // 1500 / sqrt(3), rounded down.
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
double length(JPH::Vec3 v) {
    return std::hypot(double(v.GetX()), double(v.GetY()), double(v.GetZ()));
}
}

// Private native settings, not an unbounded DistanceConstraint approximation.
// The type owns the immutable 5000N/m,180Ns/m,1500N unilateral constitutive law.
// GetConstraintSettings can recreate it; registering its RTTI with a global
// binary/object archive factory remains the archive owner's responsibility.
class RadialHandConstraintSettings final : public JPH::TwoBodyConstraintSettings {
    JPH_DECLARE_SERIALIZABLE_VIRTUAL(, RadialHandConstraintSettings)
public:
    JPH::Vec3 mLocalGrip=JPH::Vec3::sZero();
    float mRestLength=0.0F;
    JPH::TwoBodyConstraint *Create(JPH::Body &player, JPH::Body &support) const override;
    void SaveBinaryState(JPH::StreamOut &stream) const override {
        ConstraintSettings::SaveBinaryState(stream);
        stream.Write(mLocalGrip); stream.Write(mRestLength);
    }
protected:
    void RestoreBinaryState(JPH::StreamIn &stream) override {
        ConstraintSettings::RestoreBinaryState(stream);
        stream.Read(mLocalGrip); stream.Read(mRestLength);
    }
};

// Jolt's reflection macros use offsetof on polymorphic settings, as its own
// settings do. Keep the suppression confined to that generated metadata.
JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Winvalid-offsetof")
JPH_GCC_SUPPRESS_WARNING("-Winvalid-offsetof")
JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(RadialHandConstraintSettings) {
    JPH_ADD_BASE_CLASS(RadialHandConstraintSettings, JPH::TwoBodyConstraintSettings)
    JPH_ADD_ATTRIBUTE(RadialHandConstraintSettings, mLocalGrip)
    JPH_ADD_ATTRIBUTE(RadialHandConstraintSettings, mRestLength)
}
JPH_SUPPRESS_WARNING_POP

class RadialHandConstraint final : public JPH::TwoBodyConstraint {
public:
    JPH_OVERRIDE_NEW_DELETE
    RadialHandConstraint(JPH::Body &player, JPH::Body &support,
                         const RadialHandConstraintSettings &settings,
                         JPH::SixDOFConstraint *anchor_readback=nullptr)
        : TwoBodyConstraint(player,support,settings), local_grip_(settings.mLocalGrip),
          rest_length_(settings.mRestLength), anchor_readback_(anchor_readback) {}
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User2; }
    JPH::Mat44 GetConstraintToBody1Matrix() const override { return JPH::Mat44::sIdentity(); }
    JPH::Mat44 GetConstraintToBody2Matrix() const override { return JPH::Mat44::sTranslation(local_grip_); }
    void NotifyShapeChanged(const JPH::BodyID &body, JPH::Vec3Arg delta_com) override {
        if (body==mBody2->GetID()) local_grip_-=delta_com;
        // Keep the dormant Cartesian local anchors honest for readback/return.
        // The radial player attachment always follows the actual player COM.
        if (anchor_readback_) anchor_readback_->NotifyShapeChanged(body,delta_com);
        ResetWarmStart();
    }
    void SetRestLength(float rest) { rest_length_=rest; }
    float GetRestLength() const { return rest_length_; }
    void SetupVelocityConstraint(float dt) override {
        using namespace JPH;
        solved_=false;
        step_dt_=dt;
        const RVec3 grip=mBody2->GetCenterOfMassTransform()*local_grip_;
        const Vec3 separation(grip-mBody1->GetCenterOfMassPosition());
        const float radius=separation.Length();
        if (!mBody1->IsDynamic() || !mBody1->IsRigidBody() || !mBody2->IsRigidBody() ||
            !std::isfinite(dt) || dt<=0 || !finite(separation) || !std::isfinite(radius) ||
            !std::isfinite(rest_length_) || rest_length_<=0 || radius<rest_length_ || radius<=1.0e-6F) {
            ResetWarmStart(); return;
        }
        axis_=separation/radius; // player COM -> grip; negative lambda is tension.
        impulse_cap_=PhysicalHandClimb::force_bound_n()*dt;
        if (!std::isfinite(impulse_cap_)) { ResetWarmStart(); return; }
        const float error=radius-rest_length_;
        const float tangent=error<.3F ? 5000.0F : 0.0F;
        // A=180+h*k_t, gamma=1/(h*A), bias=f0/A. In saturation k_t=0
        // and bias=1500/180: retaining k=5000 there changes the old law.
        const float bias=tangent==0.0F ? PhysicalHandClimb::force_bound_n()/180.0F : 0.0F;
        row_.CalculateConstraintPropertiesWithStiffnessAndDamping(dt,*mBody1,
            separation,*mBody2,Vec3(grip-mBody2->GetCenterOfMassPosition()),
            axis_,bias,error,tangent,180.0F);
        // r1+u is parallel to the axis: no player torque. The support's actual
        // local grip, COM and inertia supply its reciprocal anchor torque.
    }
    void ResetWarmStart() override {
        row_.Deactivate(); // pinned Jolt also clears accumulated lambda.
        axis_=JPH::Vec3::sZero();
        step_dt_=impulse_cap_=0.0F;
        solved_=false;
    }
    void WarmStartVelocityConstraint(float ratio) override {
        if (!row_.IsActive()) return;
        const float old_lambda=row_.GetTotalLambda();
        // WarmStart scales and applies without a built-in clamp. Bound the
        // scaled accumulated cache BEFORE application, including variable h.
        if (!std::isfinite(old_lambda) || old_lambda>0) {
            const float dt=step_dt_;
            ResetWarmStart(); SetupVelocityConstraint(dt);
            ratio=0.0F;
        }
        if (!std::isfinite(ratio) || ratio<0) ratio=0.0F;
        if (old_lambda<0) {
            ratio=std::min(ratio,impulse_cap_/(-old_lambda));
            // Float division/multiplication must not round the cap upward.
            if ((-old_lambda)*ratio>impulse_cap_) ratio=std::nextafter(ratio,0.0F);
        }
        if (row_.IsActive()) row_.WarmStart(*mBody1,*mBody2,axis_,ratio);
    }
    bool SolveVelocityConstraint(float) override {
        if (!row_.IsActive()) return false;
        solved_=true;
        return row_.SolveVelocityConstraint(*mBody1,*mBody2,axis_,-impulse_cap_,0.0F);
    }
    bool SolvePositionConstraint(float,float) override { return false; }
    JPH::Vec3 PlayerForce() const {
        return solved_ && IsActive() && row_.IsActive() && step_dt_>0 ?
            -axis_*(row_.GetTotalLambda()/step_dt_) : JPH::Vec3::sZero();
    }
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override {
        auto *settings=new RadialHandConstraintSettings;
        ToConstraintSettings(*settings);
        settings->mLocalGrip=local_grip_;
        settings->mRestLength=rest_length_;
        return settings;
    }
    void SaveState(JPH::StateRecorder &stream) const override {
        TwoBodyConstraint::SaveState(stream);
        row_.SaveState(stream);
        stream.Write(local_grip_); stream.Write(rest_length_);
        stream.Write(axis_); stream.Write(step_dt_); stream.Write(impulse_cap_); stream.Write(solved_);
    }
    void RestoreState(JPH::StateRecorder &stream) override {
        TwoBodyConstraint::RestoreState(stream);
        row_.RestoreState(stream);
        stream.Read(local_grip_); stream.Read(rest_length_);
        stream.Read(axis_); stream.Read(step_dt_); stream.Read(impulse_cap_); stream.Read(solved_);
        // AxisConstraintPart records lambda only; its Jacobian/effective mass
        // are reconstructed by the next Setup, exactly as in Jolt's Distance.
    }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *renderer) const override {
        if (anchor_readback_) anchor_readback_->DrawConstraint(renderer);
    }
#endif
private:
    JPH::Vec3 local_grip_;
    float rest_length_;
    JPH::Ref<JPH::SixDOFConstraint> anchor_readback_;
    JPH::AxisConstraintPart row_;
    JPH::Vec3 axis_=JPH::Vec3::sZero();
    float step_dt_=0.0F, impulse_cap_=0.0F;
    bool solved_=false;
};

JPH::TwoBodyConstraint *RadialHandConstraintSettings::Create(JPH::Body &player,JPH::Body &support) const {
    return new RadialHandConstraint(player,support,*this);
}
PhysicalHandClimb::PhysicalHandClimb(JPH::PhysicsSystem &system, JPH::BodyID player, CargoNet *net)
    : system_(system), player_(player), net_(net) {}
PhysicalHandClimb::~PhysicalHandClimb() { clear(); }
bool PhysicalHandClimb::attach(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip) {
    return replace(hand, support, actual_grip, false);
}
bool PhysicalHandClimb::regrip(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip) {
    return replace(hand, support, actual_grip, true);
}
bool PhysicalHandClimb::replace(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip,
                                bool preserve_extension) {
    using namespace JPH;
    if (hand >= hands_.size() || player_.IsInvalid() || support.IsInvalid() ||
        support == player_ || !finite(actual_grip)) return false;
    if (radial_profile_) {
        // Validate the replacement before the dissipative mode change. An
        // invalid requested support must not discard a valid old radial pair.
        const BodyID candidate_ids[]{player_,support};
        {
            BodyLockMultiRead lock(system_.GetBodyLockInterface(),candidate_ids,2);
            const Body *player=lock.GetBody(0), *hold=lock.GetBody(1);
            if (!player || !hold || !player->IsDynamic() || !player->IsInBroadPhase() ||
                !hold->IsInBroadPhase() || player->IsSensor() || hold->IsSensor() ||
                (!(net_ && support==net_->body()) && !hold->IsRigidBody())) return false;
        }
        if (!set_radial_profile(false)) return false;
    }
    if (net_ && support == net_->body()) {
        const BodyLockRead lock(system_.GetBodyLockInterface(), player_);
        if (!lock.Succeeded() || !lock.GetBody().IsDynamic()) return false;
        const auto material=net_->material_point(actual_grip);
        const auto actual=net_->world_point(material);
        Vec3 offset(lock.GetBody().GetPosition()-actual);
        if (preserve_extension && soft_[hand].attached)
            offset=soft_[hand].rest_offset+Vec3(net_->world_point(soft_[hand].material)-actual);
        detach(hand);
        soft_[hand]={true,material,offset};
        return true;
    }
    const BodyID previous_support = hands_[hand] ? hands_[hand]->GetBody2()->GetID() : support;
    // A rigid replacement is only extension-preserving for another rigid grip.
    preserve_extension=preserve_extension && hands_[hand].GetPtr()!=nullptr;
    const BodyID ids[] { player_, support, previous_support };
    Ref<SixDOFConstraint> constraint;
    bool wake_support = false;
    {
        BodyLockMultiWrite lock(system_.GetBodyLockInterface(), ids, 3);
        Body *player = lock.GetBody(0), *hold = lock.GetBody(1);
        if (player == nullptr || hold == nullptr || !player->IsRigidBody() || !hold->IsRigidBody() ||
            !player->IsDynamic() ||
            player->IsSensor() || hold->IsSensor() || !player->IsInBroadPhase() || !hold->IsInBroadPhase()) return false;
        const auto translation = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        if (player->GetMotionProperties()->GetAllowedDOFs() != translation) return false;
        SixDOFConstraintSettings settings;
        settings.mSpace = EConstraintSpace::WorldSpace;
        settings.mPosition1 = settings.mPosition2 = actual_grip;
        Vec3 target = Vec3::sZero();
        if (preserve_extension && attached(hand)) {
            const Body *previous = lock.GetBody(2);
            if (previous == nullptr || !previous->IsInBroadPhase()) return false;
            const auto &old = hands_[hand];
            const RVec3 old_grip = previous->GetCenterOfMassTransform() *
                                  old->GetConstraintToBody2Matrix().GetTranslation();
            settings.mPosition1 = player->GetCenterOfMassTransform() *
                                  old->GetConstraintToBody1Matrix().GetTranslation();
            // The upright player's constraint axes are world identity. Moving
            // the support anchor changes separation and target equally, so
            // neither existing spring error nor body momentum is reset.
            target = old->GetTargetPositionCS() + Vec3(actual_grip - old_grip);
        }
        settings.mNumVelocityStepsOverride = 40;
        settings.mNumPositionStepsOverride = 8;
        for (int i = 0; i < SixDOFConstraintSettings::EAxis::Num; ++i)
            settings.MakeFreeAxis(static_cast<SixDOFConstraintSettings::EAxis>(i));
        for (int i = 0; i < 3; ++i) {
            settings.mMotorSettings[i].mSpringSettings = SpringSettings(ESpringMode::StiffnessAndDamping, 5000.0F, 180.0F);
            settings.mMotorSettings[i].SetForceLimit(kAxisForce);
        }
        constraint = static_cast<SixDOFConstraint *>(settings.Create(*player, *hold));
        for (int i = 0; i < 3; ++i)
            constraint->SetMotorState(static_cast<SixDOFConstraint::EAxis>(i), EMotorState::Position);
        constraint->SetTargetPositionCS(target);
        constraint->SetTargetVelocityCS(Vec3::sZero());
        wake_support = hold->IsDynamic();
    }
    // Invalid replacement leaves the old hand intact. A valid replacement only
    // changes constraints; neither body's instantaneous velocity is overwritten.
    detach(hand);
    hands_[hand] = constraint;
    system_.AddConstraint(constraint.GetPtr());
    auto &bodies = system_.GetBodyInterface();
    bodies.ActivateBody(player_);
    if (wake_support) bodies.ActivateBody(support);
    return true;
}
void PhysicalHandClimb::detach(unsigned hand) {
    if (!attached(hand)) return;
    if (radial_profile_ && !set_radial_profile(false)) { clear(); return; }
    soft_[hand].attached=false;
    if (hands_[hand]) system_.RemoveConstraint(hands_[hand].GetPtr());
    hands_[hand] = nullptr;
    forces_[hand] = JPH::Vec3::sZero();
}
void PhysicalHandClimb::clear() {
    // Always remove registered radial rows, even if returning to Cartesian
    // failed because an attached body left the broad phase. No stale body refs.
    for (auto &radial:radial_hands_) if (radial) {
        system_.RemoveConstraint(radial.GetPtr());
        radial=nullptr;
    }
    for (unsigned i=0;i<hands_.size();++i) {
        if (hands_[i] && !radial_profile_) system_.RemoveConstraint(hands_[i].GetPtr());
        hands_[i]=nullptr;
        soft_[i].attached=false;
        forces_[i]=JPH::Vec3::sZero();
    }
    radial_profile_=false;
    radial_lengths_.fill(0.0F);
    last_bound_ = 0.0;
    last_actuator_positive_work_ = 0.0;
    transfer_profile_ = false;
    swing_profile_ = false;
}
void PhysicalHandClimb::set_swing_profile() {
    swing_profile_ = true;
    // The existing finite reciprocal hands carry the real body. Lower command
    // power bounds deliberate body lean; the hanger hinge remains passive.
    for (auto &hand : hands_) if (hand) for (int axis=0; axis<3; ++axis)
        hand->GetMotorSettings(static_cast<JPH::SixDOFConstraint::EAxis>(axis)).mSpringSettings.mDamping=120.0F;
}
void PhysicalHandClimb::set_transfer_profile(float rigid_hand_damping) {
    if (!std::isfinite(rigid_hand_damping) || rigid_hand_damping <= 0) return;
    if (radial_profile_ && !set_radial_profile(false)) return;
    transfer_profile_ = true;
    // Default CHOSEN600Ns/m per rigid hand: ~0.65 critical damping for85kg
    // and two5000N/m springs. Force limits remain1500N per hand.
    for(auto &hand:hands_) if(hand) for(int axis=0;axis<3;++axis)
        hand->GetMotorSettings(static_cast<JPH::SixDOFConstraint::EAxis>(axis)).mSpringSettings.mDamping=rigid_hand_damping;
}
bool PhysicalHandClimb::set_radial_profile(bool enabled) {
    using namespace JPH;
    if (enabled==radial_profile_) return true;
    if (enabled && (!compatible_hands() || transfer_profile_)) return false;
    const BodyID ids[]{player_, hands_[0] ? hands_[0]->GetBody2()->GetID() : player_,
                       hands_[1] ? hands_[1]->GetBody2()->GetID() : player_};
    std::array<Ref<SixDOFConstraint>,2> fresh;
    std::array<Ref<TwoBodyConstraint>,2> fresh_radial;
    std::array<float,2> spans{};
    std::array<Vec3,2> soft_offsets{};
    {
        BodyLockMultiWrite lock(system_.GetBodyLockInterface(),ids,3);
        Body *player=lock.GetBody(0);
        if (!player || !player->IsDynamic() || !player->IsRigidBody() ||
            player->IsSensor() || !player->IsInBroadPhase()) return false;
        for (unsigned i=0;i<hands_.size();++i) if (attached(i)) {
            Body *hold=lock.GetBody(i+1);
            if (!soft_[i].attached && (!hold || !hold->IsRigidBody() || hold->IsSensor() ||
                !hold->IsInBroadPhase())) return false;
            const RVec3 grip=soft_[i].attached ? net_->world_point(soft_[i].material) :
                hold->GetCenterOfMassTransform()*hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
            const Vec3 radius(player->GetCenterOfMassPosition()-grip);
            if (!finite(radius) || !std::isfinite(radius.Length()) ||
                (enabled && (radius.Length()<=1.0e-6F || radius.Length()>1.75F))) return false;
            spans[i]=radius.Length();
            soft_offsets[i]=Vec3(player->GetPosition()-grip);
            if (enabled && hands_[i]) {
                RadialHandConstraintSettings settings;
                settings.mLocalGrip=hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
                settings.mRestLength=spans[i];
                const auto saved=hands_[i]->GetConstraintSettings();
                settings.mNumVelocityStepsOverride=saved->mNumVelocityStepsOverride;
                settings.mNumPositionStepsOverride=saved->mNumPositionStepsOverride;
                settings.mConstraintPriority=saved->mConstraintPriority;
                settings.mUserData=saved->mUserData;
                fresh_radial[i]=new RadialHandConstraint(*player,*hold,settings,hands_[i].GetPtr());
            }
            if (!enabled && hands_[i]) {
                // Copy the old limits/motor damping, but reconstruct at the
                // unchanged actual local anchors. Fresh solver: no old lambda.
                const auto saved=hands_[i]->GetConstraintSettings();
                auto *settings=static_cast<SixDOFConstraintSettings *>(saved.GetPtr());
                // Retain exact COM-local anchors from GetConstraintSettings;
                // a world->local round trip would quantize them on every toggle.
                // Both new world frames are identity at the actual p1/p2.
                settings->mSpace=EConstraintSpace::LocalToBodyCOM;
                settings->mAxisX1=player->GetRotation().Conjugated()*Vec3::sAxisX();
                settings->mAxisY1=player->GetRotation().Conjugated()*Vec3::sAxisY();
                settings->mAxisX2=hold->GetRotation().Conjugated()*Vec3::sAxisX();
                settings->mAxisY2=hold->GetRotation().Conjugated()*Vec3::sAxisY();
                fresh[i]=static_cast<SixDOFConstraint *>(settings->Create(*player,*hold));
                for (int axis=0;axis<3;++axis)
                    fresh[i]->SetMotorState(static_cast<SixDOFConstraint::EAxis>(axis),EMotorState::Position);
                const auto p1=player->GetCenterOfMassTransform()*fresh[i]->GetConstraintToBody1Matrix().GetTranslation();
                const auto p2=hold->GetCenterOfMassTransform()*fresh[i]->GetConstraintToBody2Matrix().GetTranslation();
                fresh[i]->SetTargetPositionCS(Vec3(p2-p1));
                fresh[i]->SetTargetVelocityCS(Vec3::sZero());
            }
        }
    }
    // Validation/construction precede every mutation: failed handoff leaves
    // valid old attachments intact. Both directions discard stored preload;
    // neither positions/velocities nor the muscle ledgers receive a refund.
    for (unsigned i=0;i<hands_.size();++i) if (attached(i)) {
        if (enabled && hands_[i]) {
            system_.RemoveConstraint(hands_[i].GetPtr());
            radial_hands_[i]=fresh_radial[i];
            system_.AddConstraint(radial_hands_[i].GetPtr());
        }
        if (!enabled && hands_[i]) {
            system_.RemoveConstraint(radial_hands_[i].GetPtr());
            radial_hands_[i]=nullptr;
            hands_[i]=fresh[i];
            system_.AddConstraint(hands_[i].GetPtr());
        }
        if (!enabled && soft_[i].attached) soft_[i].rest_offset=soft_offsets[i];
        forces_[i]=Vec3::sZero();
    }
    radial_profile_=enabled;
    radial_lengths_=enabled ? spans : std::array<float,2>{};
    last_bound_=last_actuator_positive_work_=0.0;
    auto &bodies=system_.GetBodyInterface();
    bodies.ActivateBody(player_);
    for (unsigned i=0;i<hands_.size();++i) if (hands_[i] && hands_[i]->GetBody2()->IsDynamic())
        bodies.ActivateBody(hands_[i]->GetBody2()->GetID());
    return true;
}
float PhysicalHandClimb::radial_commanded_span_m() const {
    return radial_profile_ ? .5F*(radial_rest_length(0)+radial_rest_length(1)) : 0.0F;
}
float PhysicalHandClimb::radial_rest_length(unsigned hand) const {
    return radial_hands_[hand] ?
        static_cast<const RadialHandConstraint *>(radial_hands_[hand].GetPtr())->GetRestLength() : radial_lengths_[hand];
}
float PhysicalHandClimb::radial_actual_span_m() const {
    using namespace JPH;
    if (!radial_profile_ || !compatible_hands()) return 0.0F;
    const BodyID ids[]{player_,hands_[0] ? hands_[0]->GetBody2()->GetID() : player_,
                       hands_[1] ? hands_[1]->GetBody2()->GetID() : player_};
    BodyLockMultiRead lock(system_.GetBodyLockInterface(),ids,3);
    const Body *player=lock.GetBody(0);
    if (!player) return 0.0F;
    float sum=0.0F;
    for (unsigned i=0;i<2;++i) {
        const Body *hold=lock.GetBody(i+1);
        if (!soft_[i].attached && !hold) return 0.0F;
        const RVec3 grip=soft_[i].attached ? net_->world_point(soft_[i].material) :
            hold->GetCenterOfMassTransform()*hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
        sum+=Vec3(player->GetCenterOfMassPosition()-grip).Length();
    }
    return .5F*sum;
}
float PhysicalHandClimb::advance_radial_targets(float shortening_m,float dt) {
    using namespace JPH;
    last_bound_=last_actuator_positive_work_=0.0;
    if (!radial_profile_ || !compatible_hands() || !std::isfinite(shortening_m) ||
        !std::isfinite(dt) || dt<=0 || shortening_m==0) return 0.0F;
    const BodyID ids[]{player_,hands_[0] ? hands_[0]->GetBody2()->GetID() : player_,
                       hands_[1] ? hands_[1]->GetBody2()->GetID() : player_};
    std::array<float,2> radii{};
    {
        BodyLockMultiRead lock(system_.GetBodyLockInterface(),ids,3);
        const Body *player=lock.GetBody(0);
        if (!player) return 0.0F;
        for (unsigned i=0;i<2;++i) {
            const Body *hold=lock.GetBody(i+1);
            if (!soft_[i].attached && !hold) return 0.0F;
            const RVec3 grip=soft_[i].attached ? net_->world_point(soft_[i].material) :
                hold->GetCenterOfMassTransform()*hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
            radii[i]=Vec3(player->GetCenterOfMassPosition()-grip).Length();
            if (!std::isfinite(radii[i])) return 0.0F;
        }
    }
    const auto potential=[](double e) {
        e=std::max(e,0.0);
        return e<=.3 ? 2500.0*e*e : 1500.0*e-225.0;
    };
    const double stroke_limit=.6*double(dt), budget=500.0*double(dt);
    const double stroke=std::clamp(double(shortening_m),-stroke_limit,stroke_limit);
    const std::array<float,2> previous{radial_rest_length(0),radial_rest_length(1)};
    std::array<float,2> targets{};
    double supplied=0,absorbed=0,debit=0,mean=0;
    const auto evaluate=[&](double fraction) {
        supplied=absorbed=debit=mean=0;
        bool valid=true;
        for (unsigned i=0;i<2;++i) {
            targets[i]=float(std::max(1.0e-6,double(previous[i])-stroke*fraction));
            const double delta=double(previous[i])-targets[i];
            valid=valid && std::isfinite(targets[i]) && std::abs(delta)<=stroke_limit &&
                std::abs(delta)<=std::abs(double(shortening_m));
            const double work=potential(double(radii[i])-targets[i])-potential(double(radii[i])-previous[i]);
            supplied+=std::max(work,0.0); absorbed+=std::max(-work,0.0);
            debit+=force_bound_n()*std::abs(delta); mean+=.5*delta;
        }
        return valid && supplied<=budget;
    };
    if (!evaluate(1.0)) {
        // Search on actual float lengths, so quantized stroke/work cannot
        // slip through a continuous-budget estimate. Absorption is separate.
        double low=0,high=1;
        for (int iteration=0;iteration<40;++iteration) {
            const double middle=.5*(low+high);
            if (evaluate(middle)) low=middle; else high=middle;
        }
        if (!evaluate(low)) return 0.0F;
    }
    radial_lengths_=targets;
    for (unsigned i=0;i<hands_.size();++i) if (radial_hands_[i])
        static_cast<RadialHandConstraint *>(radial_hands_[i].GetPtr())->SetRestLength(targets[i]);
    last_bound_=debit; command_bound_+=debit;
    last_actuator_positive_work_=supplied; actuator_positive_work_+=supplied;
    actuator_absorbed_work_+=absorbed;
    auto &bodies=system_.GetBodyInterface();
    bodies.ActivateBody(player_);
    for (const auto &hand:hands_) if (hand && hand->GetBody2()->IsDynamic())
        bodies.ActivateBody(hand->GetBody2()->GetID());
    return float(mean);
}
bool PhysicalHandClimb::active() const noexcept { return attached(0) || attached(1); }
bool PhysicalHandClimb::attached(unsigned hand) const noexcept {
    return hand < hands_.size() && (hands_[hand].GetPtr() != nullptr || soft_[hand].attached);
}
JPH::Vec3 PhysicalHandClimb::advance_targets(JPH::Vec3 desired_player_displacement, float dt,
                                            std::optional<float> command_power_cap_w) {
    using namespace JPH;
    last_bound_ = 0.0;
    last_actuator_positive_work_ = 0.0;
    if (radial_profile_) return Vec3::sZero();
    const unsigned count = unsigned(attached(0)) + unsigned(attached(1));
    if (count == 0 || !finite(desired_player_displacement) || !std::isfinite(dt) || dt <= 0 ||
        (command_power_cap_w && (!std::isfinite(*command_power_cap_w) || *command_power_cap_w <= 0)))
        return Vec3::sZero();
    const double distance = length(desired_player_displacement);
    if (distance == 0.0) return Vec3::sZero();
    const float power = command_power_cap_w ?
        std::min(active_command_power_bound_w(), *command_power_cap_w) : active_command_power_bound_w();
    const double budget = double(power) * dt;
    const bool bounded_posture_stroke = swing_profile_ || command_power_cap_w.has_value();
    // Swing lean is bounded by real spring work and a chosen0.6m/s body
    // stroke, rather than charging1500N even when the hand load is small.
    // Existing climb/transfer keep their conservative command-distance bound.
    double scale = bounded_posture_stroke ? std::min(1.0, .6 * dt / distance) :
        std::min(1.0, budget / (double(count) * force_bound_n() * distance));
    std::array<Vec3, 2> extensions;
    for (unsigned i=0;i<hands_.size();++i) if(attached(i)) extensions[i]=spring_extension(i);
    const auto work_for_delta = [&](unsigned i, Vec3 delta) {
        // Both rigid p2-p1-target and soft target-player gain the commanded
        // player displacement. Use double products to resolve small work.
        const auto e=extensions[i];
        return 5000.0*(double(e.GetX())*delta.GetX()+double(e.GetY())*delta.GetY()+
                       double(e.GetZ())*delta.GetZ()+.5*double(delta.LengthSq()));
    };
    const auto positive_work = [&](double fraction) {
        const Vec3 delta=desired_player_displacement*float(fraction);
        double supplied=0;
        for(unsigned i=0;i<hands_.size();++i) if(attached(i)) supplied+=std::max(0.0,work_for_delta(i,delta));
        return supplied;
    };
    if(positive_work(scale)>budget) {
        double low=0,high=scale;
        for(int n=0;n<24;++n) {const double middle=.5*(low+high);
            if(positive_work(middle)<=budget) low=middle; else high=middle;}
        scale=low*.999999;
    }
    std::array<Vec3, 2> targets;
    double debit = 0.0, supplied=0.0, absorbed=0.0, max_stroke=0.0;
    // Charge actual representable target changes, including float rounding.
    // One downward correction handles the budget boundary; otherwise reject the
    // command instead of silently admitting an unbounded/nonfinite target.
    for (int attempt = 0; attempt < 2; ++attempt) {
        const Vec3 delta(float(double(desired_player_displacement.GetX()) * scale),
                         float(double(desired_player_displacement.GetY()) * scale),
                         float(double(desired_player_displacement.GetZ()) * scale));
        debit = supplied = absorbed = max_stroke = 0.0;
        for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
            const auto previous = soft_[i].attached ? soft_[i].rest_offset : hands_[i]->GetTargetPositionCS();
            // Jolt's separation is p2-p1. Positive player motion reduces it.
            targets[i] = soft_[i].attached ? previous + delta : previous - delta;
            if (!finite(targets[i])) return Vec3::sZero();
            debit += force_bound_n() * length(targets[i] - previous);
            const auto actual_delta=soft_[i].attached ? targets[i]-previous : previous-targets[i];
            max_stroke=std::max(max_stroke,length(actual_delta));
            const double work=work_for_delta(i,actual_delta);
            supplied+=std::max(0.0,work);
            absorbed+=std::max(0.0,-work);
        }
        const double limiting_debit=std::max(supplied,bounded_posture_stroke?0.0:debit);
        const bool stroke_exceeded=command_power_cap_w && max_stroke > .6 * dt;
        if (limiting_debit <= budget && !stroke_exceeded) break;
        const double correction=std::min(limiting_debit > budget ? budget / limiting_debit : 1.0,
            stroke_exceeded ? .6 * dt / max_stroke : 1.0);
        scale *= 0.999999 * correction;
    }
    if (supplied > budget || (!bounded_posture_stroke && debit > budget) ||
        (command_power_cap_w && max_stroke > .6 * dt)) return Vec3::sZero();
    Vec3 delivered = Vec3::sZero();
    for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
        delivered += soft_[i].attached ? targets[i] - soft_[i].rest_offset :
                                         hands_[i]->GetTargetPositionCS() - targets[i];
        if (soft_[i].attached) { soft_[i].rest_offset=targets[i]; continue; }
        hands_[i]->SetTargetPositionCS(targets[i]);
        const Body *hold = hands_[i]->GetBody2();
        if (hold->IsDynamic()) system_.GetBodyInterface().ActivateBody(hold->GetID());
    }
    system_.GetBodyInterface().ActivateBody(player_);
    last_bound_ = debit;
    command_bound_ += debit;
    last_actuator_positive_work_=supplied;
    actuator_positive_work_+=supplied;
    actuator_absorbed_work_+=absorbed;
    return delivered / float(count);
}
bool PhysicalHandClimb::compatible_hands() const noexcept {
    return attached(0) && attached(1) && soft_[0].attached == soft_[1].attached;
}
JPH::RVec3 PhysicalHandClimb::grip_midpoint() const {
    using namespace JPH;
    if (!compatible_hands()) return RVec3::sZero();
    if (soft_[0].attached)
        return .5F * (net_->world_point(soft_[0].material) + net_->world_point(soft_[1].material));
    const BodyID ids[] { hands_[0]->GetBody2()->GetID(), hands_[1]->GetBody2()->GetID() };
    BodyLockMultiRead lock(system_.GetBodyLockInterface(), ids, 2);
    if (!lock.GetBody(0) || !lock.GetBody(1)) return RVec3::sZero();
    return .5F * (lock.GetBody(0)->GetCenterOfMassTransform() * hands_[0]->GetConstraintToBody2Matrix().GetTranslation() +
                 lock.GetBody(1)->GetCenterOfMassTransform() * hands_[1]->GetConstraintToBody2Matrix().GetTranslation());
}
JPH::Vec3 PhysicalHandClimb::spring_extension(unsigned hand) const {
    using namespace JPH;
    if(soft_[hand].attached) {
        const auto target=net_->world_point(soft_[hand].material)+RVec3(soft_[hand].rest_offset);
        return Vec3(target-system_.GetBodyInterface().GetPosition(player_));
    }
    const auto &constraint=hands_[hand];
    const BodyID ids[]{player_,constraint->GetBody2()->GetID()};
    BodyLockMultiRead lock(system_.GetBodyLockInterface(),ids,2);
    const auto *player=lock.GetBody(0), *hold=lock.GetBody(1);
    if(!player || !hold) return Vec3::sZero();
    const auto p1=player->GetCenterOfMassTransform()*constraint->GetConstraintToBody1Matrix().GetTranslation();
    const auto p2=hold->GetCenterOfMassTransform()*constraint->GetConstraintToBody2Matrix().GetTranslation();
    return Vec3(p2-p1)-constraint->GetTargetPositionCS();
}
JPH::RVec3 PhysicalHandClimb::commanded_position() const {
    using namespace JPH;
    const BodyID ids[] { player_, hands_[0] ? hands_[0]->GetBody2()->GetID() : player_,
                        hands_[1] ? hands_[1]->GetBody2()->GetID() : player_ };
    BodyLockMultiRead lock(system_.GetBodyLockInterface(), ids, 3);
    const Body *player = lock.GetBody(0);
    if (player == nullptr) return RVec3::sZero();
    RVec3 sum = RVec3::sZero();
    unsigned count = 0;
    for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
        if (radial_profile_) {
            const Body *hold=lock.GetBody(i+1);
            if (!soft_[i].attached && !hold) continue;
            const RVec3 grip=soft_[i].attached ? net_->world_point(soft_[i].material) :
                hold->GetCenterOfMassTransform()*hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
            const Vec3 n=Vec3(player->GetCenterOfMassPosition()-grip).NormalizedOr(Vec3::sAxisY());
            sum+=grip+n*radial_rest_length(i); ++count; continue;
        }
        if (soft_[i].attached) {
            sum+=net_->world_point(soft_[i].material)+RVec3(soft_[i].rest_offset); ++count; continue;
        }
        const Body *hold = lock.GetBody(i + 1);
        if (hold == nullptr) continue;
        const RVec3 anchor1 = player->GetCenterOfMassTransform() *
                             hands_[i]->GetConstraintToBody1Matrix().GetTranslation();
        const RVec3 anchor2 = hold->GetCenterOfMassTransform() *
                             hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
        sum += anchor2 - Vec3(anchor1 - player->GetPosition()) - hands_[i]->GetTargetPositionCS();
        ++count;
    }
    return count ? sum / float(count) : player->GetPosition();
}
void PhysicalHandClimb::pre_step(float dt, bool bodies_locked) {
    using namespace JPH;
    if (!std::isfinite(dt) || dt<=0) return;
    if (radial_profile_) {
        // Standalone calls may wake here. Shipping calls wake outside Update;
        // never acquire locks or add/remove constraints in OnStep.
        if (!bodies_locked) {
            auto &bodies=system_.GetBodyInterface();
            bodies.ActivateBody(player_);
            for (const auto &hand:hands_) if (hand && hand->GetBody2()->IsDynamic())
                bodies.ActivateBody(hand->GetBody2()->GetID());
        }
        // Rigid radial tension belongs exclusively to the native solver rows.
        // Keep the actual SoftCargoNet material adapter on its manual path.
        if (!soft_[0].attached && !soft_[1].attached) return;
        // The production callback already owns all body locks. Never reacquire
        // a BodyLock there; standalone owner-thread calls acquire one batch.
        const auto apply = [&](Body *player) {
            if (!player || !player->IsDynamic()) return;
            for (unsigned i=0;i<hands_.size();++i) if (soft_[i].attached) {
                forces_[i]=Vec3::sZero();
                const RVec3 grip=net_->world_point(soft_[i].material);
                const Vec3 radius(player->GetCenterOfMassPosition()-grip);
                const float r=radius.Length();
                if (!std::isfinite(r) || r<radial_lengths_[i] || r<=1.0e-6F) continue;
                const Vec3 n=radius/r;
                const float e=std::max(r-radial_lengths_[i],0.0F);
                const Vec3 grip_velocity=net_->material_velocity(soft_[i].material);
                const float w=player->GetMotionProperties()->GetInverseMass()+
                    net_->material_inverse_mass(soft_[i].material);
                const float rdot=(player->GetLinearVelocity()-grip_velocity).Dot(n);
                const float implicit_damping=180.0F+(e<.3F ? 5000.0F : 0.0F)*dt;
                const float tension=std::clamp((std::min(5000.0F*e,force_bound_n())+implicit_damping*rdot)/
                    (1.0F+implicit_damping*dt*w),0.0F,force_bound_n());
                const Vec3 impulse=-n*(tension*dt);
                player->AddImpulse(impulse); // COM: no manufactured player torque.
                net_->impulse(soft_[i].material,-impulse);
                forces_[i]=-n*tension;
                peak_force_=std::max(peak_force_,tension);
                // The next hand samples the velocities changed by this pair.
            }
        };
        if (bodies_locked) {
            const auto &access=system_.GetBodyLockInterfaceNoLock();
            apply(access.TryGetBody(player_));
        } else {
            BodyLockWrite lock(system_.GetBodyLockInterface(),player_);
            apply(lock.Succeeded() ? &lock.GetBody() : nullptr);
        }
        return;
    }
    if (!net_) return;
    auto &bodies=bodies_locked ? system_.GetBodyInterfaceNoLock() : system_.GetBodyInterface();
    for (unsigned i=0;i<soft_.size();++i) if (soft_[i].attached) {
        const auto &hand=soft_[i];
        const auto target=net_->world_point(hand.material)+RVec3(hand.rest_offset);
        const auto error=Vec3(target-bodies.GetPosition(player_));
        const auto relative=bodies.GetLinearVelocity(player_)-net_->material_velocity(hand.material);
        // Resolve the actual Kelvin-Voigt hand force at every collision step.
        // For the lightest sampled knot (0.24kg), 60Ns/m is near its ~69Ns/m
        // critical damping at5000N/m. Shipping h/4 keeps the explicit pair
        // stable; there is no root velocity write or surrogate rider load.
        constexpr float stiffness=5000.0F,damping=60.0F;
        auto impulse=(error*stiffness-relative*damping)*dt;
        const float cap=force_bound_n()*dt;
        if (impulse.Length()>cap) impulse*=cap/impulse.Length();
        bodies.AddImpulse(player_,impulse);
        net_->impulse(hand.material,-impulse);
        forces_[i]=impulse/dt;
        peak_force_=std::max(peak_force_,forces_[i].Length());
    }
}
void PhysicalHandClimb::post_step(float collision_dt) {
    if (radial_profile_) {
        for (unsigned i=0;i<hands_.size();++i) {
            if (radial_hands_[i]) {
                forces_[i]=static_cast<const RadialHandConstraint *>(radial_hands_[i].GetPtr())->PlayerForce();
                peak_force_=std::max(peak_force_,forces_[i].Length());
            } else if (!soft_[i].attached) forces_[i]=JPH::Vec3::sZero();
        }
        return;
    }
    if (!std::isfinite(collision_dt) || collision_dt <= 0.0F) return;
    for (unsigned i = 0; i < hands_.size(); ++i) if (hands_[i]) {
        // Translation axes were world identity at acquisition and the upright
        // player's rotation is locked. Lambda acts on body2, opposite on player.
        forces_[i] = -hands_[i]->GetTotalLambdaMotorTranslation() / collision_dt;
        peak_force_ = std::max(peak_force_, forces_[i].Length());
    }
}
JPH::Vec3 PhysicalHandClimb::hand_force(unsigned hand) const noexcept {
    return hand < forces_.size() ? forces_[hand] : JPH::Vec3::sZero();
}
}
