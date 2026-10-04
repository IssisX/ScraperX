#include "sim/physical_hand_climb.hpp"
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
PhysicalHandClimb::PhysicalHandClimb(JPH::PhysicsSystem &system, JPH::BodyID player)
    : system_(system), player_(player) {}
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
    const BodyID previous_support = attached(hand) ? hands_[hand]->GetBody2()->GetID() : support;
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
    system_.RemoveConstraint(hands_[hand].GetPtr());
    hands_[hand] = nullptr;
    forces_[hand] = JPH::Vec3::sZero();
}
void PhysicalHandClimb::clear() {
    detach(0); detach(1);
    last_bound_ = 0.0;
}
bool PhysicalHandClimb::active() const noexcept { return attached(0) || attached(1); }
bool PhysicalHandClimb::attached(unsigned hand) const noexcept {
    return hand < hands_.size() && hands_[hand].GetPtr() != nullptr;
}
void PhysicalHandClimb::advance_targets(JPH::Vec3 desired_player_displacement, float dt) {
    using namespace JPH;
    last_bound_ = 0.0;
    const unsigned count = unsigned(attached(0)) + unsigned(attached(1));
    if (count == 0 || !finite(desired_player_displacement) || !std::isfinite(dt) || dt <= 0) return;
    const double distance = length(desired_player_displacement);
    if (distance == 0.0) return;
    const double budget = double(command_power_bound_w()) * dt;
    double scale = std::min(1.0, budget / (double(count) * force_bound_n() * distance));
    std::array<Vec3, 2> targets;
    double debit = 0.0;
    // Charge actual representable target changes, including float rounding.
    // One downward correction handles the budget boundary; otherwise reject the
    // command instead of silently admitting an unbounded/nonfinite target.
    for (int attempt = 0; attempt < 2; ++attempt) {
        const Vec3 delta(float(double(desired_player_displacement.GetX()) * scale),
                         float(double(desired_player_displacement.GetY()) * scale),
                         float(double(desired_player_displacement.GetZ()) * scale));
        debit = 0.0;
        for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
            const auto previous = hands_[i]->GetTargetPositionCS();
            // Jolt's separation is p2-p1. Positive player motion reduces it.
            targets[i] = previous - delta;
            if (!finite(targets[i])) return;
            debit += force_bound_n() * length(targets[i] - previous);
        }
        if (debit <= budget) break;
        scale *= 0.999999 * budget / debit;
    }
    if (debit > budget) return;
    for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
        hands_[i]->SetTargetPositionCS(targets[i]);
        const Body *hold = hands_[i]->GetBody2();
        if (hold->IsDynamic()) system_.GetBodyInterface().ActivateBody(hold->GetID());
    }
    system_.GetBodyInterface().ActivateBody(player_);
    last_bound_ = debit;
    command_bound_ += debit;
}
JPH::RVec3 PhysicalHandClimb::commanded_position() const {
    using namespace JPH;
    const BodyID ids[] { player_, attached(0) ? hands_[0]->GetBody2()->GetID() : player_,
                        attached(1) ? hands_[1]->GetBody2()->GetID() : player_ };
    BodyLockMultiRead lock(system_.GetBodyLockInterface(), ids, 3);
    const Body *player = lock.GetBody(0);
    if (player == nullptr) return RVec3::sZero();
    RVec3 sum = RVec3::sZero();
    unsigned count = 0;
    for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
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
void PhysicalHandClimb::post_step(float collision_dt) {
    if (!std::isfinite(collision_dt) || collision_dt <= 0.0F) return;
    for (unsigned i = 0; i < hands_.size(); ++i) if (attached(i)) {
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
