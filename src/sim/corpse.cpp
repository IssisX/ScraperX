// Corpses (see corpse.hpp).
#include "sim/corpse.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Skeleton/Skeleton.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;

// The joints' play (CHOSEN, a body's own ranges, a little short of them):
// the spine and neck bend and twist some way; the shoulders swing wide; the
// hips forward more than sideways; the elbows and knees each bend one way.
constexpr float kSpineCone = 0.6F;
constexpr float kSpineTwist = 0.4F;
constexpr float kNeckCone = 0.7F;
constexpr float kNeckTwist = 0.9F;
constexpr float kShoulderCone = 1.5F;
constexpr float kShoulderTwist = 0.7F;
constexpr float kHipForwardCone = 1.3F;
constexpr float kHipSideCone = 0.6F;
constexpr float kHipTwist = 0.4F;
constexpr float kElbowBend = 2.5F;
constexpr float kKneeBend = 2.5F;
// What the joints' tissue holds back of a swing (CHOSEN): enough that a
// corpse settles rather than flails, not so much that it lands stiff.
constexpr float kJointFrictionNm = 6.0F;
// Clothes and skin on concrete or steel: a body that lands does not slide.
constexpr float kFriction = 0.9F;
// A body comes down with its knees a little bent (CHOSEN): laid out so, half
// the bend each side of the knee (the shin forward of the ankle, the thigh
// back from the knee), a hit straight down buckles the legs. Laid out
// straight, the column stood on its locked knees through any vertical hit.
constexpr float kLandingKneeBendRad = 0.6F;
// A body is not rigid: what it came down into the ground with is spent
// crushing it, and the corpse is laid down carrying at most this much of it
// downward (CHOSEN; the speed along the ground it keeps). Rigid parts given
// the whole 30 m/s threw the body a metre back up off the floor.
constexpr float kCrushCarriedMps = 4.0F;

// The axis in the plane of `along` and `forward` square to `along`.
Vec3 square_to(const Vec3 along, const Vec3 forward) {
    return (forward - along * forward.Dot(along)).Normalized();
}
// Every corpse's bodies carry this, plus its number (the contact listener
// and the presentation can tell a corpse from a machine).
constexpr std::uint64_t kUserDataBase = 3300;

Ref<TwoBodyConstraintSettings> swing_twist(const RVec3 at, const Vec3 twist, const Vec3 plane, const float normal_cone,
                                           const float plane_cone, const float twist_half) {
    Ref<SwingTwistConstraintSettings> s = new SwingTwistConstraintSettings;
    s->mSpace = EConstraintSpace::WorldSpace;
    s->mPosition1 = at;
    s->mPosition2 = at;
    s->mTwistAxis1 = twist;
    s->mTwistAxis2 = twist;
    s->mPlaneAxis1 = plane;
    s->mPlaneAxis2 = plane;
    s->mNormalHalfConeAngle = normal_cone;
    s->mPlaneHalfConeAngle = plane_cone;
    s->mTwistMinAngle = -twist_half;
    s->mTwistMaxAngle = twist_half;
    s->mMaxFrictionTorque = kJointFrictionNm;
    return Ref<TwoBodyConstraintSettings>(s.GetPtr());
}

// A joint bending one way about `axis` from the limb hanging along `normal`:
// [lo, hi] radians, one of them zero.
Ref<TwoBodyConstraintSettings> hinge(const RVec3 at, const Vec3 axis, const Vec3 normal, const float lo, const float hi) {
    Ref<HingeConstraintSettings> h = new HingeConstraintSettings;
    h->mSpace = EConstraintSpace::WorldSpace;
    h->mPoint1 = at;
    h->mPoint2 = at;
    h->mHingeAxis1 = axis;
    h->mHingeAxis2 = axis;
    h->mNormalAxis1 = normal;
    h->mNormalAxis2 = normal;
    h->mLimitsMin = lo;
    h->mLimitsMax = hi;
    h->mMaxFrictionTorque = kJointFrictionNm;
    return Ref<TwoBodyConstraintSettings>(h.GetPtr());
}

} // namespace

Corpses::Corpses(PhysicsSystem &physics, const ObjectLayer layer) : physics_(physics), layer_(layer) {}

Corpses::~Corpses() {
    clear();
}

void Corpses::spawn(const RVec3 soles, const Vec3 facing, const Vec3 velocity) {
    if (static_cast<int>(corpses_.size()) >= kMaxCorpses) {
        corpses_.front()->RemoveFromPhysicsSystem();
        corpses_.pop_front();
    }
    const Vec3 up = Vec3::sAxisY();
    const Vec3 forward = Vec3(facing.GetX(), 0.0F, facing.GetZ()).NormalizedOr(Vec3::sAxisZ());
    const Vec3 right = forward.Cross(up).Normalized();
    // The bent legs (kLandingKneeBendRad): where the knee and the hips come,
    // and the body above the hips laid that much lower and that far forward.
    const float tilt = 0.5F * kLandingKneeBendRad;
    const float shin_length = kKneeY - kSoleY;
    const float thigh_length = kHipY - kKneeY;
    const float knee_height = kSoleY + shin_length * std::cos(tilt);
    const float knee_ahead = shin_length * std::sin(tilt);
    const float hip_sink = kHipY - (knee_height + thigh_length * std::cos(tilt));
    const float hip_ahead = knee_ahead - thigh_length * std::sin(tilt);
    const auto at = [&](const float height, const float across) {
        return soles + up * (height - hip_sink) + forward * hip_ahead + right * across;
    };

    Ref<Skeleton> skeleton = new Skeleton;
    const int pelvis = skeleton->AddJoint("pelvis");
    const int chest = skeleton->AddJoint("chest", pelvis);
    skeleton->AddJoint("head", chest);
    const int upper_left = skeleton->AddJoint("upper_arm_left", chest);
    skeleton->AddJoint("forearm_left", upper_left);
    const int upper_right = skeleton->AddJoint("upper_arm_right", chest);
    skeleton->AddJoint("forearm_right", upper_right);
    const int thigh_left = skeleton->AddJoint("thigh_left", pelvis);
    skeleton->AddJoint("shin_left", thigh_left);
    const int thigh_right = skeleton->AddJoint("thigh_right", pelvis);
    skeleton->AddJoint("shin_right", thigh_right);

    Ref<RagdollSettings> settings = new RagdollSettings;
    settings->mSkeleton = skeleton;
    settings->mParts.resize(kParts);
    // Each part: its capsule at its centre, along `along` (up for every
    // limb hanging, right for the pelvis across the hips).
    const auto place = [&](const int index, const RVec3 centre, const Vec3 along) {
        RagdollSettings::Part &part = settings->mParts[static_cast<std::size_t>(index)];
        const PartPlan &plan = kPlan[static_cast<std::size_t>(index)];
        part.SetShape(new CapsuleShape(plan.half_length, plan.radius));
        part.mPosition = centre;
        part.mRotation = Quat::sFromTo(Vec3::sAxisY(), along);
        part.mMotionType = EMotionType::Dynamic;
        part.mObjectLayer = layer_;
        part.mMotionQuality = EMotionQuality::LinearCast;
        part.mFriction = kFriction;
        part.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        part.mMassPropertiesOverride.mMass = plan.mass_kg;
        part.mUserData = kUserDataBase + spawned_;
    };
    place(kPelvis, at(kHipY + 0.05F, 0.0F), right);
    place(kChest, at(0.5F * (kSpineY + kNeckY), 0.0F), up);
    place(kHead, at(kNeckY + kPlan[kHead].radius + kPlan[kHead].half_length, 0.0F), up);
    for (const float side : {-1.0F, 1.0F}) {
        const float shoulder = side * kShoulderHalfSpan;
        const float hip = side * kHipHalfSpan;
        const int upper = side < 0.0F ? kUpperArmLeft : kUpperArmRight;
        const int fore = side < 0.0F ? kForearmLeft : kForearmRight;
        const int thigh = side < 0.0F ? kThighLeft : kThighRight;
        const int shin = side < 0.0F ? kShinLeft : kShinRight;
        place(upper, at(0.5F * (kShoulderY + kElbowY), shoulder), up);
        place(fore, at(0.5F * (kElbowY + kWristY), shoulder), up);
        const RVec3 hip_joint = at(kHipY, hip);
        const RVec3 knee = soles + up * knee_height + forward * knee_ahead + right * hip;
        const RVec3 sole = soles + up * kSoleY + right * hip;
        const Vec3 thigh_down = Vec3(knee - hip_joint).Normalized();
        const Vec3 shin_down = Vec3(sole - knee).Normalized();
        place(thigh, hip_joint + 0.5F * Vec3(knee - hip_joint), -thigh_down);
        place(shin, knee + 0.5F * Vec3(sole - knee), -shin_down);
        // The shoulder swings about the arm hanging down; the elbow bends the
        // forearm forward (a positive turn about `right` carries -up toward
        // forward); the hip swings the thigh, forward more than out; the knee
        // bends the shin back.
        settings->mParts[static_cast<std::size_t>(upper)].mToParent =
            swing_twist(at(kShoulderY, shoulder), -up, forward, kShoulderCone, kShoulderCone, kShoulderTwist);
        settings->mParts[static_cast<std::size_t>(fore)].mToParent = hinge(at(kElbowY, shoulder), right, -up, 0.0F, kElbowBend);
        settings->mParts[static_cast<std::size_t>(thigh)].mToParent = swing_twist(
            hip_joint, thigh_down, square_to(thigh_down, forward), kHipForwardCone, kHipSideCone, kHipTwist);
        // The knee bends on from the bend it came down with, never back past it.
        settings->mParts[static_cast<std::size_t>(shin)].mToParent =
            hinge(knee, right, shin_down, -(kKneeBend - kLandingKneeBendRad), 0.0F);
    }
    settings->mParts[kChest].mToParent = swing_twist(at(kSpineY, 0.0F), up, forward, kSpineCone, kSpineCone, kSpineTwist);
    settings->mParts[kHead].mToParent = swing_twist(at(kNeckY, 0.0F), up, forward, kNeckCone, kNeckCone, kNeckTwist);
    settings->Stabilize();
    settings->DisableParentChildCollisions();
    settings->CalculateBodyIndexToConstraintIndex();
    settings->CalculateConstraintIndexToBodyIdxPair();

    Ref<Ragdoll> corpse = settings->CreateRagdoll(static_cast<CollisionGroup::GroupID>(spawned_), kUserDataBase + spawned_,
                                                  &physics_);
    corpse->AddToPhysicsSystem(EActivation::Activate);
    // The crush (kCrushCarriedMps): what went into the ground is spent there.
    corpse->SetLinearVelocity(Vec3(velocity.GetX(), std::max(velocity.GetY(), -kCrushCarriedMps), velocity.GetZ()));
    corpses_.push_back(corpse);
    ++spawned_;
}

void Corpses::clear() {
    for (const Ref<Ragdoll> &corpse : corpses_) {
        corpse->RemoveFromPhysicsSystem();
    }
    corpses_.clear();
}

Corpses::PartPose Corpses::part(const int corpse, const int part) const {
    PartPose pose;
    if (corpse < 0 || corpse >= count() || part < 0 || part >= kParts) {
        return pose;
    }
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const BodyID id = corpses_[static_cast<std::size_t>(corpse)]->GetBodyID(part);
    pose.position = bodies.GetPosition(id);
    pose.rotation = bodies.GetRotation(id);
    return pose;
}

bool Corpses::resting(const int corpse) const {
    return corpse >= 0 && corpse < count() && !corpses_[static_cast<std::size_t>(corpse)]->IsActive();
}

} // namespace scraperx::sim
