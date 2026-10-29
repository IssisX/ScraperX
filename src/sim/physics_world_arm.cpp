// Arm loss (matron.hpp "ArmPlay"): her second hand on a wrist, pulling it
// away from the body her other hand holds by the ankle. The wrist's load
// past kArmHoldN, over time, past kArmTearNs tears the arm from the
// shoulder (and the body pulling out of her ankle hand while she has the
// wrist leaves it in her hand at once: tear_grip): a body of its own
// (kArmMassKg) in her hand, lifted, swung and thrown; the player is lighter
// by it, and one-armed (one_arm) until a death or a restart. Both gone, she
// has won (Claimed): the run ends and starts again whole. Members of
// Simulation::PhysicsWorld.

#include "sim/physics_world.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
constexpr float kHandSpringNpm = 3500.0F;
constexpr float kHandDampNspm = 500.0F;
constexpr float kWristCloseM = 0.14F;
// The arm, in the body's frame: the shoulder and the wrist hanging past the
// head (in the hang, below it).
constexpr float kShoulderUp = 0.45F;
constexpr float kWristUp = 1.05F;
constexpr float kArmSideM = 0.25F;
// The severed arm despawns this long after it is thrown, or this far below
// where it was thrown from.
constexpr float kSeveredArmSeconds = 90.0F;
constexpr float kSeveredArmFallM = 30.0F;
} // namespace

JPH::Vec3 Simulation::PhysicsWorld::wrist_local(const int side) const noexcept {
    return JPH::Vec3(side == 0 ? -kArmSideM : kArmSideM, kWristUp, 0.0F);
}

void Simulation::PhysicsWorld::despawn_severed_arm(JPH::BodyInterface &bodies) noexcept {
    destroy_grab_constraint(arm_in_hand_);
    if (!severed_arm_id_.IsInvalid()) {
        remove_and_destroy(bodies, severed_arm_id_);
        severed_arm_id_ = JPH::BodyID();
        ++severed_arm_despawn_count_;
    }
    severed_arm_thrown_ = false;
    severed_arm_s_ = 0.0F;
}

void Simulation::PhysicsWorld::arm_reset(JPH::BodyInterface &bodies) noexcept {
    destroy_grab_constraint(wrist_);
    destroy_grab_constraint(hand2_arm_);
    if (!hand2_id_.IsInvalid()) {
        destroy_grab_constraint(arm_in_hand_);
        remove_and_destroy(bodies, hand2_id_);
        hand2_id_ = JPH::BodyID();
    }
    despawn_severed_arm(bodies);
    // A death or a restart: whole again.
    arm_lost_ = {false, false};
    fatality_reset(bodies);
    wrist_side_ = -1;
    wrist_strain_ns_ = 0.0F;
    wrist_load_n_ = 0.0F;
    claimed_s_ = 0.0F;
    JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
    if (lock.Succeeded()) {
        JPH::Body &body = lock.GetBody();
        JPH::MassProperties mass = body.GetShape()->GetMassProperties();
        mass.ScaleToMass(kPlayerMassKg);
        body.GetMotionProperties()->SetMassProperties(body.GetMotionProperties()->GetAllowedDOFs(), mass);
    }
}

void Simulation::PhysicsWorld::sever_arm(JPH::BodyInterface &bodies) noexcept {
    const int side = wrist_side_;
    destroy_grab_constraint(wrist_);
    arm_lost_[static_cast<std::size_t>(side)] = true;
    last_severed_side_ = side;
    ++arm_tear_count_;
    // The arm, where it was: from the shoulder to her hand.
    const JPH::RMat44 frame = bodies.GetWorldTransform(player_id_);
    const JPH::RVec3 shoulder = frame * JPH::Vec3(side == 0 ? -kArmSideM : kArmSideM, kShoulderUp, 0.0F);
    const JPH::RVec3 hand = bodies.GetPosition(hand2_id_);
    JPH::Vec3 along = JPH::Vec3(hand - shoulder);
    const float length = std::max(0.2F, along.Length());
    along /= length;
    if (severed_arm_shape_ == nullptr) {
        severed_arm_shape_ = new JPH::CapsuleShape(0.27F, 0.05F);
    }
    const JPH::Quat rot = JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), along);
    const JPH::RVec3 middle = hand - along * 0.32F;
    JPH::BodyCreationSettings settings(severed_arm_shape_, middle, rot, JPH::EMotionType::Dynamic,
                                       object_layers::kHand);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = Matron::kArmMassKg;
    settings.mAllowSleeping = false;
    settings.mUserData = Simulation::kSeveredArmEntityId;
    settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    severed_arm_id_ = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
    bodies.SetLinearVelocity(severed_arm_id_, bodies.GetPointVelocity(player_id_, middle));
    JPH::PointConstraintSettings held;
    held.mSpace = JPH::EConstraintSpace::WorldSpace;
    held.mPoint1 = hand;
    held.mPoint2 = hand;
    arm_in_hand_ = create_constraint(held, hand2_id_, severed_arm_id_, false);
    severed_arm_thrown_ = false;
    severed_arm_s_ = 0.0F;
    // The body is lighter by it.
    JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
    if (lock.Succeeded()) {
        JPH::Body &body = lock.GetBody();
        JPH::MassProperties mass = body.GetShape()->GetMassProperties();
        mass.ScaleToMass(kPlayerMassKg - Matron::kArmMassKg * static_cast<float>(arms_lost()));
        body.GetMotionProperties()->SetMassProperties(body.GetMotionProperties()->GetAllowedDOFs(), mass);
    }
}

void Simulation::PhysicsWorld::arm_drive(JPH::BodyInterface &bodies, const float /*dt*/) noexcept {
    if (matron_->take_wrist_reseat()) {
        // She re-posed with the wrist out of her reach: her hand goes for the
        // nearer whole arm again.
        destroy_grab_constraint(wrist_);
        wrist_side_ = -1;
    }
    // Which arm she goes for: a whole one, the nearer her; and where on it
    // her hand takes it: the point of it (shoulder to wrist) nearest her
    // other shoulder, the most of it her arm reaches.
    const JPH::RMat44 frame = bodies.GetWorldTransform(player_id_);
    const JPH::RVec3 her = bodies.GetPosition(matron_->body());
    const JPH::RVec3 her_shoulder = her + matron_->shoulder2_local();
    const auto arm_point = [&](const int k) {
        const JPH::RVec3 root = frame * JPH::Vec3(k == 0 ? -kArmSideM : kArmSideM, kShoulderUp, 0.0F);
        const JPH::Vec3 along = JPH::Vec3(frame * wrist_local(k) - root);
        const float t = std::clamp(JPH::Vec3(her_shoulder - root).Dot(along) / along.LengthSq(), 0.0F, 1.0F);
        return root + along * t;
    };
    int side = wrist_side_;
    if (side < 0 || arm_lost_[static_cast<std::size_t>(side)]) {
        side = -1;
        float best = 1.0e9F;
        for (int k = 0; k < 2; ++k) {
            if (arm_lost_[static_cast<std::size_t>(k)]) {
                continue;
            }
            const float d = static_cast<float>(JPH::Vec3(arm_point(k) - her_shoulder).Length());
            if (d < best) {
                best = d;
                side = k;
            }
        }
    }
    const JPH::RVec3 wrist = side >= 0 ? arm_point(side) : bodies.GetPosition(player_id_);
    matron_->set_player_body(wrist, 2 - arms_lost());
    if (matron_->reaching_wrist()) {
        wrist_side_ = side;
    }
    // Her second hand works while her first has the body, in the fatality,
    // and while it holds an arm the body pulled away from (tear_grip).
    const bool active =
        matron_->hand2_active() && (grip_ != nullptr || matron_->claimed() || arm_in_hand_ != nullptr);
    if (!active && !hand2_id_.IsInvalid()) {
        destroy_grab_constraint(wrist_);
        destroy_grab_constraint(hand2_arm_);
        if (arm_in_hand_ != nullptr) {
            // Still in her hand at the end: dropped where it is.
            destroy_grab_constraint(arm_in_hand_);
            severed_arm_thrown_ = true;
            severed_arm_thrown_y_ = static_cast<float>(bodies.GetPosition(severed_arm_id_).GetY());
            bodies.SetObjectLayer(severed_arm_id_, object_layers::kCorpse);
        }
        remove_and_destroy(bodies, hand2_id_);
        hand2_id_ = JPH::BodyID();
    }
    if (active && hand2_id_.IsInvalid()) {
        JPH::Ref<JPH::Shape> sphere = new JPH::SphereShape(0.06F);
        const JPH::RVec3 shoulder = her + matron_->shoulder2_local();
        JPH::BodyCreationSettings settings(sphere, shoulder, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                           object_layers::kHand);
        settings.mAllowedDOFs =
            JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
        settings.mGravityFactor = 0.0F;
        settings.mAllowSleeping = false;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = Matron::kHandMassKg;
        hand2_id_ = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
        bodies.SetLinearVelocity(hand2_id_, bodies.GetLinearVelocity(matron_->body()));
        JPH::DistanceConstraintSettings arm;
        arm.mSpace = JPH::EConstraintSpace::WorldSpace;
        arm.mPoint1 = shoulder;
        arm.mPoint2 = shoulder;
        arm.mMinDistance = 0.0F;
        arm.mMaxDistance = Matron::kArmReachM;
        // Stiff (Matron::kArmLimitHz): her arm gives a few centimetres under a
        // jolt, never past her arm's length.
        arm.mLimitsSpringSettings.mFrequency = Matron::kArmLimitHz;
        arm.mLimitsSpringSettings.mDamping = 1.0F;
        hand2_arm_ = create_constraint(arm, matron_->body(), hand2_id_, false);
        hand2_arm_at_ = matron_->shoulder2_local();
    }
    if (hand2_id_.IsInvalid()) {
        return;
    }
    rehang_arm(bodies, hand2_arm_, hand2_arm_at_, hand2_id_, matron_->shoulder2_local());
    const JPH::RVec3 hand = bodies.GetPosition(hand2_id_);
    if (matron_->reaching_wrist() && wrist_ == nullptr && side >= 0 &&
        JPH::Vec3(wrist - hand).Length() <= kWristCloseM) {
        JPH::PointConstraintSettings grip;
        grip.mSpace = JPH::EConstraintSpace::WorldSpace;
        grip.mPoint1 = hand;
        grip.mPoint2 = hand;
        wrist_ = create_constraint(grip, hand2_id_, player_id_, false);
        if (wrist_ != nullptr) {
            wrist_strain_ns_ = 0.0F;
            matron_->wrist_taken();
        }
    }
    JPH::Vec3 force = kHandSpringNpm * JPH::Vec3(matron_->hand2_target() - hand) +
                      kHandDampNspm * (matron_->hand2_target_velocity() - bodies.GetLinearVelocity(hand2_id_));
    if (arm_in_hand_ != nullptr) {
        force += JPH::Vec3(0.0F, Matron::kArmMassKg * 9.81F, 0.0F);
    }
    const float cap = matron_->hand2_pull_n();
    if (force.Length() > cap) {
        force *= cap / force.Length();
    }
    bodies.AddForce(hand2_id_, force);
    bodies.AddForce(matron_->body(), -force);
    JPH::Vec3 throw_v;
    if (matron_->take_arm_throw(throw_v) && arm_in_hand_ != nullptr) {
        destroy_grab_constraint(arm_in_hand_);
        bodies.SetObjectLayer(severed_arm_id_, object_layers::kCorpse);
        bodies.SetLinearVelocity(severed_arm_id_, throw_v);
        severed_arm_thrown_ = true;
        severed_arm_thrown_y_ = static_cast<float>(bodies.GetPosition(severed_arm_id_).GetY());
        severed_arm_s_ = 0.0F;
        ++arm_throw_count_;
    }
}

void Simulation::PhysicsWorld::arm_post_step(JPH::BodyInterface &bodies, const float dt) noexcept {
    if (wrist_ != nullptr) {
        const JPH::Vec3 lambda = static_cast<JPH::PointConstraint *>(wrist_.GetPtr())->GetTotalLambdaPosition();
        wrist_load_n_ = lambda.Length() / std::max(dt, 1.0e-4F);
        wrist_strain_ns_ += std::max(0.0F, wrist_load_n_ - Matron::kArmHoldN) * dt;
        if (wrist_strain_ns_ >= Matron::kArmTearNs) {
            sever_arm(bodies);
            matron_->arm_torn();
        }
    } else {
        wrist_load_n_ = 0.0F;
    }
    if (!severed_arm_id_.IsInvalid() && severed_arm_thrown_) {
        severed_arm_s_ += dt;
        const float y = static_cast<float>(bodies.GetPosition(severed_arm_id_).GetY());
        if (severed_arm_s_ > kSeveredArmSeconds || y < severed_arm_thrown_y_ - kSeveredArmFallM) {
            despawn_severed_arm(bodies);
        }
    }
    if (matron_->claimed()) {
        if (claimed_s_ == 0.0F) {
            ++claimed_count_;
        }
        claimed_s_ += dt;   // the run ends with the fatality (physics_world_fatality.cpp)
    }
}

} // namespace scraperx::sim
