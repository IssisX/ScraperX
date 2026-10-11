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
    std::array<float,2> spans{};
    std::array<Vec3,2> soft_offsets{};
    {
        BodyLockMultiWrite lock(system_.GetBodyLockInterface(),ids,3);
        Body *player=lock.GetBody(0);
        if (!player || !player->IsDynamic() || !player->IsInBroadPhase()) return false;
        for (unsigned i=0;i<hands_.size();++i) if (attached(i)) {
            Body *hold=lock.GetBody(i+1);
            if (!soft_[i].attached && (!hold || !hold->IsInBroadPhase())) return false;
            const RVec3 grip=soft_[i].attached ? net_->world_point(soft_[i].material) :
                hold->GetCenterOfMassTransform()*hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
            const Vec3 radius(player->GetCenterOfMassPosition()-grip);
            if (!finite(radius) || (enabled && (radius.Length()<=1.0e-6F || radius.Length()>1.75F))) return false;
            spans[i]=radius.Length();
            soft_offsets[i]=Vec3(player->GetPosition()-grip);
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
        if (enabled && hands_[i]) system_.RemoveConstraint(hands_[i].GetPtr());
        if (!enabled && hands_[i]) {
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
    return radial_profile_ ? .5F*(radial_lengths_[0]+radial_lengths_[1]) : 0.0F;
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
    std::array<float,2> targets{};
    double supplied=0,absorbed=0,debit=0,mean=0;
    const auto evaluate=[&](double fraction) {
        supplied=absorbed=debit=mean=0;
        bool valid=true;
        for (unsigned i=0;i<2;++i) {
            targets[i]=float(std::max(1.0e-6,double(radial_lengths_[i])-stroke*fraction));
            const double delta=double(radial_lengths_[i])-targets[i];
            valid=valid && std::isfinite(targets[i]) && std::abs(delta)<=stroke_limit &&
                std::abs(delta)<=std::abs(double(shortening_m));
            const double work=potential(double(radii[i])-targets[i])-potential(double(radii[i])-radial_lengths_[i]);
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
            sum+=grip+n*radial_lengths_[i]; ++count; continue;
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
        // Standalone calls have no owner tick to wake the disconnected pair.
        // Shipping calls wake outside Update; never acquire locks in OnStep.
        if (!bodies_locked) {
            auto &bodies=system_.GetBodyInterface();
            bodies.ActivateBody(player_);
            for (const auto &hand:hands_) if (hand && hand->GetBody2()->IsDynamic())
                bodies.ActivateBody(hand->GetBody2()->GetID());
        }
        const BodyID ids[]{player_, hands_[0] ? hands_[0]->GetBody2()->GetID() : player_,
                           hands_[1] ? hands_[1]->GetBody2()->GetID() : player_};
        // The production callback already owns all body locks. Never reacquire
        // a BodyLock there; standalone owner-thread calls acquire one batch.
        const auto apply = [&](Body *player, Body *hold0, Body *hold1) {
            if (!player || !player->IsDynamic()) return;
            Body *holds[]{hold0,hold1};
            for (unsigned i=0;i<hands_.size();++i) if (attached(i)) {
                forces_[i]=Vec3::sZero();
                Body *hold=holds[i];
                if (!soft_[i].attached && !hold) continue;
                const RVec3 grip=soft_[i].attached ? net_->world_point(soft_[i].material) :
                    hold->GetCenterOfMassTransform()*hands_[i]->GetConstraintToBody2Matrix().GetTranslation();
                const Vec3 radius(player->GetCenterOfMassPosition()-grip);
                const float r=radius.Length();
                if (!std::isfinite(r) || r<radial_lengths_[i] || r<=1.0e-6F) continue;
                const Vec3 n=radius/r;
                const float e=std::max(r-radial_lengths_[i],0.0F);
                const Vec3 grip_velocity=soft_[i].attached ? net_->material_velocity(soft_[i].material) : hold->GetPointVelocity(grip);
                float w=player->GetMotionProperties()->GetInverseMass();
                if (soft_[i].attached) w+=net_->material_inverse_mass(soft_[i].material);
                else if (hold->IsDynamic()) {
                    const Vec3 angular=Vec3(grip-hold->GetCenterOfMassPosition()).Cross(n);
                    w+=hold->GetMotionProperties()->GetInverseMass()+angular.Dot(hold->GetInverseInertia()*angular);
                }
                const float rdot=(player->GetLinearVelocity()-grip_velocity).Dot(n);
                const float implicit_damping=180.0F+(e<.3F ? 5000.0F : 0.0F)*dt;
                const float tension=std::clamp((std::min(5000.0F*e,force_bound_n())+implicit_damping*rdot)/
                    (1.0F+implicit_damping*dt*w),0.0F,force_bound_n());
                const Vec3 impulse=-n*(tension*dt);
                player->AddImpulse(impulse); // COM: no manufactured player torque.
                if (soft_[i].attached) net_->impulse(soft_[i].material,-impulse);
                else if (hold->IsDynamic()) hold->AddImpulse(-impulse,grip);
                forces_[i]=-n*tension;
                peak_force_=std::max(peak_force_,tension);
                // The next hand samples the velocities changed by this pair.
            }
        };
        if (bodies_locked) {
            const auto &access=system_.GetBodyLockInterfaceNoLock();
            apply(access.TryGetBody(ids[0]),access.TryGetBody(ids[1]),access.TryGetBody(ids[2]));
        } else {
            BodyLockMultiWrite lock(system_.GetBodyLockInterface(),ids,3);
            apply(lock.GetBody(0),lock.GetBody(1),lock.GetBody(2));
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
    if (!std::isfinite(collision_dt) || collision_dt <= 0.0F) return;
    if (radial_profile_) return;
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
