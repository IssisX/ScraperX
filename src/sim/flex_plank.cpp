#include "sim/flex_plank.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {

namespace {

// A cantilever's own mass as seen at its tip, its first mode (Rayleigh: 33/140).
constexpr float kCantileverTipMassShare = 33.0F / 140.0F;
// Dry planed softwood on boots and steel.
constexpr float kTimberFriction = 0.5F;

// sum_i (L - x_i)^2 over the hinges, x_i = i L / n (flex_plank.hpp).
float lever_sum(const float length_m) noexcept {
    const float s = length_m / static_cast<float>(FlexPlank::kSegments);
    float sum = 0.0F;
    for (int i = 0; i < FlexPlank::kSegments; ++i) {
        const float arm = length_m - s * static_cast<float>(i);
        sum += arm * arm;
    }
    return sum;
}

// The segment's real mass and inertia: a solid box of the timber.
JPH::MassProperties real_mass_properties(const JPH::Vec3 half, const float mass_kg) {
    JPH::MassProperties mass;
    mass.SetMassAndInertiaOfSolidBox(2.0F * half, mass_kg / (8.0F * half.GetX() * half.GetY() * half.GetZ()));
    return mass;
}

} // namespace

// While the hinges hold: the real mass, and the rotational inertia raised to
// the design load's own about the segment's centre when it stands at the
// segment's end, m_design (s / 2)^2 (flex_plank.hpp, "Inertia floor").
JPH::MassProperties FlexPlank::solver_mass_properties(const JPH::Vec3 half) const {
    JPH::MassProperties mass = real_mass_properties(half, segment_mass_kg());
    const float real = mass.mInertia(2, 2);   // about the box's local z: the hinge's axis
    const float floor = spec_.design_load_kg * half.GetX() * half.GetX();
    if (floor > real) {
        mass.mInertia = mass.mInertia * (floor / real);
        mass.mInertia(3, 3) = 1.0F;
    }
    return mass;
}

float FlexPlank::second_moment_m4() const noexcept {
    return spec_.width_m * spec_.thickness_m * spec_.thickness_m * spec_.thickness_m / 12.0F;
}

float FlexPlank::section_modulus_m3() const noexcept {
    return spec_.width_m * spec_.thickness_m * spec_.thickness_m / 6.0F;
}

float FlexPlank::rupture_moment_nm() const noexcept {
    return spec_.timber.bending_strength_pa * section_modulus_m3();
}

float FlexPlank::segment_mass_kg() const noexcept {
    return spec_.timber.density_kg_m3 * spec_.width_m * spec_.thickness_m * spec_.length_m /
           static_cast<float>(kSegments);
}

FlexPlank::FlexPlank(JPH::PhysicsSystem &physics, const Spec &spec, const std::uint64_t entity_base)
    : physics_(physics), spec_(spec), group_(new JPH::GroupFilterTable(kSegments)) {
    const float L = spec_.length_m;
    const float EI = spec_.timber.youngs_modulus_pa * second_moment_m4();
    tip_stiffness_ = 3.0F * EI / (L * L * L);
    const float levers = lever_sum(L);
    joint_stiffness_ = tip_stiffness_ * levers;
    const float plank_kg = segment_mass_kg() * static_cast<float>(kSegments);
    const float tip_damping =
        2.0F * std::sqrt(tip_stiffness_ * (spec_.design_load_kg + kCantileverTipMassShare * plank_kg));
    joint_damping_ = tip_damping * levers;

    const JPH::Vec3 along = JPH::Vec3(spec_.along.GetX(), 0.0F, spec_.along.GetZ()).Normalized();
    const JPH::Vec3 up = JPH::Vec3::sAxisY();
    const JPH::Vec3 cross = up.Cross(along).Normalized();
    const JPH::Quat rotation = JPH::Quat::sRotation(up, std::atan2(-along.GetZ(), along.GetX()));
    const float s = L / static_cast<float>(kSegments);
    const float h = spec_.thickness_m;
    const JPH::Vec3 half(0.5F * s, 0.5F * h, 0.5F * spec_.width_m);
    // The hinges sit on the neutral axis, mid-thickness, at each segment's root end.
    const JPH::RVec3 axis_root = spec_.root - JPH::RVec3(0.0F, 0.5F * h, 0.0F);

    JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    for (int i = 0; i < kSegments; ++i) {
        if (i > 0) {
            // Neighbours meet at their hinge: they never collide while it holds.
            group_->DisableCollision(static_cast<JPH::CollisionGroup::SubGroupID>(i - 1),
                                     static_cast<JPH::CollisionGroup::SubGroupID>(i));
        }
        const JPH::RVec3 centre = axis_root + JPH::RVec3(along * (s * (static_cast<float>(i) + 0.5F)));
        JPH::BodyCreationSettings settings(new JPH::BoxShape(half, std::min(0.005F, 0.5F * half.ReduceMin())),
                                           centre, rotation, JPH::EMotionType::Dynamic, spec_.layer);
        settings.mFriction = kTimberFriction;
        settings.mUserData = entity_base + static_cast<std::uint64_t>(i);
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
        settings.mMassPropertiesOverride = solver_mass_properties(half);
        settings.mCollisionGroup =
            JPH::CollisionGroup(group_, static_cast<JPH::CollisionGroup::GroupID>(entity_base),
                                static_cast<JPH::CollisionGroup::SubGroupID>(i));
        segments_[static_cast<std::size_t>(i)] = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
    }
    for (int i = 0; i < kSegments; ++i) {
        JPH::HingeConstraintSettings hinge;
        hinge.mSpace = JPH::EConstraintSpace::WorldSpace;
        hinge.mPoint1 = hinge.mPoint2 = axis_root + JPH::RVec3(along * (s * static_cast<float>(i)));
        hinge.mHingeAxis1 = hinge.mHingeAxis2 = cross;
        hinge.mNormalAxis1 = hinge.mNormalAxis2 = along;
        hinge.mNumVelocityStepsOverride = kVelocitySteps;
        hinge.mMotorSettings = JPH::MotorSettings(JPH::ESpringMode::StiffnessAndDamping, joint_stiffness_,
                                                  joint_damping_);
        JPH::HingeConstraint *made = nullptr;
        if (i == 0) {
            JPH::BodyLockWrite lock(physics_.GetBodyLockInterface(), segments_[0]);
            made = static_cast<JPH::HingeConstraint *>(hinge.Create(JPH::Body::sFixedToWorld, lock.GetBody()));
        } else {
            const JPH::BodyID ids[2] = {segments_[static_cast<std::size_t>(i - 1)],
                                        segments_[static_cast<std::size_t>(i)]};
            JPH::BodyLockMultiWrite lock(physics_.GetBodyLockInterface(), ids, 2);
            made = static_cast<JPH::HingeConstraint *>(hinge.Create(*lock.GetBody(0), *lock.GetBody(1)));
        }
        made->SetMotorState(JPH::EMotorState::Position);
        made->SetTargetAngle(0.0F);
        physics_.AddConstraint(made);
        joints_[static_cast<std::size_t>(i)] = made;
    }
}

FlexPlank::~FlexPlank() {
    for (JPH::Ref<JPH::HingeConstraint> &joint : joints_) {
        if (joint != nullptr) {
            physics_.RemoveConstraint(joint);
            joint = nullptr;
        }
    }
    JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    for (const JPH::BodyID id : segments_) {
        if (!id.IsInvalid()) {
            bodies.RemoveBody(id);
            bodies.DestroyBody(id);
        }
    }
}

void FlexPlank::post_step() {
    for (int i = 0; i < kSegments; ++i) {
        const JPH::Ref<JPH::HingeConstraint> &joint = joints_[static_cast<std::size_t>(i)];
        moments_[static_cast<std::size_t>(i)] =
            joint != nullptr ? joint_stiffness_ * std::abs(joint->GetCurrentAngle()) : 0.0F;
        peak_moment_ = std::max(peak_moment_, moments_[static_cast<std::size_t>(i)]);
    }
    if (snapped()) {
        return;
    }
    const float rupture = rupture_moment_nm();
    for (int i = 0; i < kSegments; ++i) {
        if (joints_[static_cast<std::size_t>(i)] != nullptr && moments_[static_cast<std::size_t>(i)] > rupture) {
            snapped_joint_ = i;
            snap_moment_ = moments_[static_cast<std::size_t>(i)];
            break_from(i);
            return;
        }
    }
}

void FlexPlank::break_from(const int joint) {
    JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    for (int i = joint; i < kSegments; ++i) {
        JPH::Ref<JPH::HingeConstraint> &hinge = joints_[static_cast<std::size_t>(i)];
        if (hinge != nullptr) {
            physics_.RemoveConstraint(hinge);
            hinge = nullptr;
        }
        if (i > 0) {
            // The broken ends knock together as the pieces they now are.
            group_->EnableCollision(static_cast<JPH::CollisionGroup::SubGroupID>(i - 1),
                                    static_cast<JPH::CollisionGroup::SubGroupID>(i));
        }
        {
            // A loose piece is the timber it is: its real inertia back.
            JPH::BodyLockWrite lock(physics_.GetBodyLockInterface(), segments_[static_cast<std::size_t>(i)]);
            if (lock.Succeeded()) {
                const JPH::Vec3 half(0.5F * spec_.length_m / static_cast<float>(kSegments), 0.5F * spec_.thickness_m,
                                     0.5F * spec_.width_m);
                lock.GetBody().GetMotionProperties()->SetMassProperties(
                    JPH::EAllowedDOFs::All, real_mass_properties(half, segment_mass_kg()));
            }
        }
        bodies.ActivateBody(segments_[static_cast<std::size_t>(i)]);
    }
}

JPH::RVec3 FlexPlank::tip() const {
    const JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    const JPH::BodyID last = segments_[kSegments - 1];
    const float s = spec_.length_m / static_cast<float>(kSegments);
    return bodies.GetPosition(last) + bodies.GetRotation(last) * JPH::Vec3(0.5F * s, 0.5F * spec_.thickness_m, 0.0F);
}

} // namespace scraperx::sim
