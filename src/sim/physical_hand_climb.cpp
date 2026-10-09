#include "sim/physical_hand_climb.hpp"
#include "sim/cargo_net.hpp"
#include <Jolt/Physics/Body/BodyLockMulti.h>
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
    soft_[hand].attached=false;
    if (hands_[hand]) system_.RemoveConstraint(hands_[hand].GetPtr());
    hands_[hand] = nullptr;
    forces_[hand] = JPH::Vec3::sZero();
}
void PhysicalHandClimb::clear() {
    detach(0); detach(1);
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
    transfer_profile_ = true;
    // Default CHOSEN600Ns/m per rigid hand: ~0.65 critical damping for85kg
    // and two5000N/m springs. Force limits remain1500N per hand.
    for(auto &hand:hands_) if(hand) for(int axis=0;axis<3;++axis)
        hand->GetMotorSettings(static_cast<JPH::SixDOFConstraint::EAxis>(axis)).mSpringSettings.mDamping=rigid_hand_damping;
}
bool PhysicalHandClimb::active() const noexcept { return attached(0) || attached(1); }
bool PhysicalHandClimb::attached(unsigned hand) const noexcept {
    return hand < hands_.size() && (hands_[hand].GetPtr() != nullptr || soft_[hand].attached);
}
void PhysicalHandClimb::advance_targets(JPH::Vec3 desired_player_displacement, float dt) {
    using namespace JPH;
    last_bound_ = 0.0;
    last_actuator_positive_work_ = 0.0;
    const unsigned count = unsigned(attached(0)) + unsigned(attached(1));
    if (count == 0 || !finite(desired_player_displacement) || !std::isfinite(dt) || dt <= 0) return;
    const double distance = length(desired_player_displacement);
    if (distance == 0.0) return;
    const double budget = double(active_command_power_bound_w()) * dt;
    // Swing lean is bounded by real spring work and a chosen0.6m/s body
    // stroke, rather than charging1500N even when the hand load is small.
    // Existing climb/transfer keep their conservative command-distance bound.
    double scale = swing_profile_ ? std::min(1.0, .6 * dt / distance) :
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
    double debit = 0.0, supplied=0.0, absorbed=0.0;
    // Charge actual representable target changes, including float rounding.
    // One downward correction handles the budget boundary; otherwise reject the
    // command instead of silently admitting an unbounded/nonfinite target.
    for (int attempt = 0; attempt < 2; ++attempt) {
        const Vec3 delta(float(double(desired_player_displacement.GetX()) * scale),
                         float(double(desired_player_displacement.GetY()) * scale),
                         float(double(desired_player_displacement.GetZ()) * scale));
        debit = supplied = absorbed = 0.0;
        for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
            const auto previous = soft_[i].attached ? soft_[i].rest_offset : hands_[i]->GetTargetPositionCS();
            // Jolt's separation is p2-p1. Positive player motion reduces it.
            targets[i] = soft_[i].attached ? previous + delta : previous - delta;
            if (!finite(targets[i])) return;
            debit += force_bound_n() * length(targets[i] - previous);
            const auto actual_delta=soft_[i].attached ? targets[i]-previous : previous-targets[i];
            const double work=work_for_delta(i,actual_delta);
            supplied+=std::max(0.0,work);
            absorbed+=std::max(0.0,-work);
        }
        const double limiting_debit=std::max(supplied,swing_profile_?0.0:debit);
        if (limiting_debit <= budget) break;
        scale *= 0.999999 * budget / limiting_debit;
    }
    if (supplied > budget || (!swing_profile_ && debit > budget)) return;
    for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
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
    if (!net_ || !std::isfinite(dt) || dt<=0) return;
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
