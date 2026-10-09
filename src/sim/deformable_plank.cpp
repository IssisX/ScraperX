#include "sim/deformable_plank.hpp"

#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/LargeIslandSplitter.h>

#include <algorithm>

#include <cfloat>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
using Axis = JPH::SixDOFConstraintSettings::EAxis;

// Solve this member's existing SixDOF rows together inside Jolt's ordinary
// contact/island solve. This introduces no additional mechanical rows, forces,
// targets or bodies. Each joint is set up and warm-started once per substep.
class PlankSolveGroup final : public JPH::Constraint {
public:
    PlankSolveGroup(const std::vector<JPH::Ref<JPH::SixDOFConstraint>> &joints,
                    const JPH::ConstraintSettings &settings)
        : Constraint(settings), joints_(joints) {
        frames_.resize(joints_.size());
        for (const auto &joint : joints_) {
            for (auto *body : {joint->GetBody1(), joint->GetBody2()}) {
                if (!body->IsDynamic()) continue;
                bool found = false;
                for (auto *existing : bodies_) found |= existing == body;
                if (!found) bodies_.push_back(body);
            }
        }
    }
    JPH::EConstraintSubType GetSubType() const override { return JPH::EConstraintSubType::User1; }
    bool IsActive() const override {
        if (!Constraint::IsActive()) return false;
        for (auto *body : bodies_) if (body->IsActive()) return true;
        return false;
    }
    void NotifyShapeChanged(const JPH::BodyID &body, JPH::Vec3Arg delta) override {
        for (const auto &joint : joints_) joint->NotifyShapeChanged(body, delta);
    }
    void SetupVelocityConstraint(float dt) override {
        for (std::size_t j = 0; j < joints_.size(); ++j) {
            const auto &joint = joints_[j];
            frames_[j][0] = joint->GetBody1()->GetRotation() * joint->GetConstraintToBody1Matrix().GetQuaternion();
            frames_[j][1] = joint->GetBody2()->GetRotation() * joint->GetConstraintToBody2Matrix().GetQuaternion();
            joint->SetupVelocityConstraint(dt);
        }
    }
    bool solve_frames(const JPH::SixDOFConstraint *joint, JPH::Quat &first, JPH::Quat &second) const {
        for (std::size_t j = 0; j < joints_.size(); ++j) if (joints_[j].GetPtr() == joint) {
            first = frames_[j][0]; second = frames_[j][1]; return true;
        }
        return false;
    }
    void ResetWarmStart() override { for (const auto &joint : joints_) joint->ResetWarmStart(); }
    void WarmStartVelocityConstraint(float ratio) override {
        for (const auto &joint : joints_) joint->WarmStartVelocityConstraint(ratio);
    }
    bool SolveVelocityConstraint(float dt) override {
        bool changed = false;
        for (int sweep = 0; sweep < 8; ++sweep) {
            float max_delta_squared = 0;
            const auto solve = [&](const auto &joint) {
                const auto translation = joint->GetTotalLambdaMotorTranslation();
                const auto rotation = joint->GetTotalLambdaMotorRotation();
                const auto lock = joint->GetTotalLambdaPosition();
                const auto angular_lock = joint->GetTotalLambdaRotation();
                changed |= joint->SolveVelocityConstraint(dt);
                max_delta_squared = std::max({max_delta_squared,
                    (joint->GetTotalLambdaMotorTranslation() - translation).LengthSq(),
                    (joint->GetTotalLambdaMotorRotation() - rotation).LengthSq(),
                    (joint->GetTotalLambdaPosition() - lock).LengthSq(),
                    (joint->GetTotalLambdaRotation() - angular_lock).LengthSq()});
            };
            for (const auto &joint : joints_) solve(joint);
            for (auto it = joints_.rbegin(); it != joints_.rend(); ++it) solve(*it);
            if (sweep >= 1 && max_delta_squared < 1.0e-12F) break;
        }
        return changed;
    }
    bool SolvePositionConstraint(float dt, float baumgarte) override {
        bool changed = false;
        for (const auto &joint : joints_) changed |= joint->SolvePositionConstraint(dt, baumgarte);
        return changed;
    }
    void BuildIslands(JPH::uint32 index, JPH::IslandBuilder &builder,
                      JPH::BodyManager &manager) override {
        for (auto *body : bodies_) {
            if (!body->IsActive()) {
                const auto id = body->GetID();
                manager.ActivateBodies(&id, 1);
            }
        }
        const auto first = bodies_.front()->GetIndexInActiveBodiesInternal();
        for (auto *body : bodies_) builder.LinkBodies(first, body->GetIndexInActiveBodiesInternal());
        builder.LinkConstraint(index, first);
    }
    JPH::uint BuildIslandSplits(JPH::LargeIslandSplitter &splitter) const override {
        // Every body touched by this multi-body solve must be protected.
        for (auto *body : bodies_) splitter.AssignToNonParallelSplit(body);
        return JPH::LargeIslandSplitter::cNonParallelSplitIdx;
    }
    JPH::Ref<JPH::ConstraintSettings> GetConstraintSettings() const override {
        // Member reconstruction remains owned by DeformablePlank. This exposes
        // its constitutive joint law, not a serialized multi-body factory.
        auto settings = joints_.front()->GetConstraintSettings();
        ToConstraintSettings(*settings);
        return settings;
    }
    void SaveState(JPH::StateRecorder &stream) const override {
        Constraint::SaveState(stream);
        for (const auto &joint : joints_) joint->SaveState(stream);
    }
    void RestoreState(JPH::StateRecorder &stream) override {
        Constraint::RestoreState(stream);
        for (const auto &joint : joints_) joint->RestoreState(stream);
    }
#ifdef JPH_DEBUG_RENDERER
    void DrawConstraint(JPH::DebugRenderer *renderer) const override {
        for (const auto &joint : joints_) joint->DrawConstraint(renderer);
    }
    void DrawConstraintLimits(JPH::DebugRenderer *renderer) const override {
        for (const auto &joint : joints_) joint->DrawConstraintLimits(renderer);
    }
#endif
private:
    std::vector<JPH::Ref<JPH::SixDOFConstraint>> joints_;
    std::vector<JPH::Body *> bodies_;
    std::vector<std::array<JPH::Quat, 2>> frames_;
};

void validate_joint_frame(const JPH::RVec3 anchor, const JPH::Quat basis,
                          const std::uint32_t velocity_iterations,
                          const std::uint32_t position_iterations) {
    if (!std::isfinite(anchor.GetX()) || !std::isfinite(anchor.GetY()) ||
        !std::isfinite(anchor.GetZ()) || !basis.IsNormalized())
        throw std::invalid_argument("deformable plank: finite anchor and normalized basis required");
    // Jolt stores overrides in uint8; zero would silently inherit the global
    // solver budget rather than the requested per-joint budget.
    if (velocity_iterations == 0 || velocity_iterations >= 256 ||
        position_iterations == 0 || position_iterations >= 256)
        throw std::invalid_argument("deformable plank: joint iterations must be in [1,255]");
}

void set_elastic_axis(JPH::SixDOFConstraintSettings &settings, const Axis axis,
                      const float stiffness) {
    const float damping = DeformablePlank::kViscousTimeSeconds * stiffness;
    settings.mMotorSettings[axis].mSpringSettings = JPH::SpringSettings(
        JPH::ESpringMode::StiffnessAndDamping, stiffness, damping);
}

JPH::SixDOFConstraintSettings passive_support_settings(
    const JPH::RVec3 anchor, const JPH::Quat basis, const bool roller,
    const std::uint32_t velocity_iterations, const std::uint32_t position_iterations,
    const bool guided) {
    auto settings = DeformablePlank::joint_settings(
        anchor, basis, true, velocity_iterations, position_iterations);
    // Both supports have free rotations. The right roller has no axial
    // spring or friction; only its Y/Z translations transmit reactions.
    for (auto &motor : settings.mMotorSettings)
        motor.mSpringSettings = JPH::SpringSettings(
            JPH::ESpringMode::StiffnessAndDamping, 0, 0);
    if (roller) settings.MakeFreeAxis(Axis::TranslationX);
    // The encounter's visible fork/roller guides restrain roll and yaw.
    // Bending about Z and the roller's axial travel remain free. The pure
    // pin/roller benchmark keeps all endpoint rotations free.
    if (guided) {
        settings.MakeFixedAxis(Axis::RotationX);
        settings.MakeFixedAxis(Axis::RotationY);
    }
    return settings;
}
} // namespace

JPH::SixDOFConstraintSettings DeformablePlank::joint_settings(
    const JPH::RVec3 anchor, const JPH::Quat world_basis, const bool fixed_endpoint,
    const std::uint32_t velocity_iterations, const std::uint32_t position_iterations) {
    validate_joint_frame(anchor, world_basis, velocity_iterations, position_iterations);
    JPH::SixDOFConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPosition1 = settings.mPosition2 = anchor;
    settings.mAxisX1 = settings.mAxisX2 = world_basis * JPH::Vec3::sAxisX();
    settings.mAxisY1 = settings.mAxisY2 = world_basis * JPH::Vec3::sAxisY();
    settings.mNumVelocityStepsOverride = velocity_iterations;
    settings.mNumPositionStepsOverride = position_iterations;
    for (int axis = 0; axis < Axis::Num; ++axis) {
        settings.MakeFreeAxis(static_cast<Axis>(axis));
        settings.mMotorSettings[axis].mSpringSettings = JPH::SpringSettings(
            JPH::ESpringMode::StiffnessAndDamping, 0, 0);
        settings.mMotorSettings[axis].SetForceLimit(FLT_MAX);
        settings.mMotorSettings[axis].SetTorqueLimit(FLT_MAX);
    }
    settings.MakeFixedAxis(Axis::TranslationY);
    settings.MakeFixedAxis(Axis::TranslationZ);
    if (fixed_endpoint) settings.MakeFixedAxis(Axis::TranslationX);
    else set_elastic_axis(settings, Axis::TranslationX,
                          kAxialStiffnessNPerM);

    // One material relaxation time gives positive viscous loss on each
    // axis without altering the elastic static compliance. Boundary
    // half-cell stiffness and damping both double.
    const float stiffness_factor = fixed_endpoint ? 2.0F : 1.0F;
    set_elastic_axis(settings, Axis::RotationX, kTorsionStiffnessNmPerRad * stiffness_factor);
    set_elastic_axis(settings, Axis::RotationY, kStrongBendingStiffnessNmPerRad * stiffness_factor);
    set_elastic_axis(settings, Axis::RotationZ, kWeakBendingStiffnessNmPerRad * stiffness_factor);
    return settings;
}

JPH::Ref<JPH::SixDOFConstraint> DeformablePlank::create_joint(
    JPH::Body &first, JPH::Body &second, const JPH::SixDOFConstraintSettings &settings) {
    JPH::Ref<JPH::SixDOFConstraint> joint =
        static_cast<JPH::SixDOFConstraint *>(settings.Create(first, second));
    for (int axis = 0; axis < Axis::Num; ++axis) {
        const auto selected = static_cast<Axis>(axis);
        if (!settings.IsFixedAxis(selected) &&
            settings.mMotorSettings[axis].mSpringSettings.HasStiffness())
            joint->SetMotorState(selected, JPH::EMotorState::Position);
    }
    // A fixed rest configuration stores strain energy. No gameplay controller
    // updates these targets, and no external load or player weight is added.
    joint->SetTargetPositionCS(JPH::Vec3::sZero());
    joint->SetTargetVelocityCS(JPH::Vec3::sZero());
    joint->SetTargetOrientationCS(JPH::Quat::sIdentity());
    joint->SetTargetAngularVelocityCS(JPH::Vec3::sZero());
    return joint;
}

DeformablePlank::DeformablePlank(
    JPH::PhysicsSystem &system, kit::Kit &kit_owner, const JPH::RVec3 left_endpoint,
    const Support support, const JPH::Quat world_basis,
    std::uint32_t velocity_iterations, std::uint32_t position_iterations)
    : system_(system), kit_(kit_owner) {
    guided_support_ = support == Support::GuidedPinRoller;
    // The free cantilever is more poorly conditioned than the supported
    // encounter. This is a local numerical budget, never a material change.
    if (velocity_iterations == 0) velocity_iterations = support == Support::Cantilever ? 250 : 80;
    if (position_iterations == 0) position_iterations = support == Support::Cantilever ? 32 : 8;
    velocity_iterations_ = velocity_iterations;
    position_iterations_ = position_iterations;
    validate_joint_frame(left_endpoint, world_basis, velocity_iterations, position_iterations);
    if (support != Support::PinRoller && support != Support::Cantilever &&
        support != Support::GuidedPinRoller)
        throw std::invalid_argument("deformable plank: unknown support configuration");
    if (kit_owner.body_for_entity(kDeformablePlankMemberEntity).valid())
        throw std::invalid_argument("deformable plank: logical member ID already in use");
    for (std::size_t segment = 0; segment < kSegmentCount; ++segment)
        if (kit_owner.body_for_entity(kDeformablePlankSegmentEntityBase + segment).valid())
            throw std::invalid_argument("deformable plank: segment ID already in use");
    joints_.reserve(kSegmentCount + 1);
    const auto point = [&](const float x) {
        return left_endpoint + JPH::RVec3(world_basis * JPH::Vec3(x, 0, 0));
    };

    try {
        kit::Part wood{{kSegmentLengthM * .5F, kThicknessM * .5F, kWidthM * .5F},
                       JPH::Vec3::sZero(), JPH::Quat::sIdentity(), kit::Material::Timber};
        wood.mass_kg = kSegmentMassKg;
        for (std::size_t segment = 0; segment < kSegmentCount; ++segment) {
            const float x = (static_cast<float>(segment) + .5F) * kSegmentLengthM;
            const auto index = kit_owner.add_body(kDeformablePlankSegmentEntityBase + segment,
                {wood}, point(x), world_basis, kSegmentMassKg, .85F);
            segment_indices_[segment] = index;
            body_ids_[segment] = kit_owner.body_id(index);
            kit_owner.set_damping(index, 0, 0);
            kit_owner.set_continuous_collision(index);
            // Jolt clamps free force/torque integration before constraint
            // solving. A 500N endpoint load gives this .722kg segment about
            // 223rad/s there; the stock47rad/s cap silently clips its moment.
            // Leave room for the real elastic constraints to oppose it.
            // This changes only these segments, never the player controller.
            {
                JPH::BodyLockWrite lock(system_.GetBodyLockInterface(), body_ids_[segment]);
                lock.GetBody().GetMotionProperties()->SetMaxAngularVelocity(500.0F);
            }
        }
        for (std::size_t segment = 1; segment < kSegmentCount; ++segment) {
            kit_owner.disable_collision(segment_indices_[segment - 1], segment_indices_[segment]);
            attach_joint(body_ids_[segment - 1], body_ids_[segment], joint_settings(
                point(static_cast<float>(segment) * kSegmentLengthM), world_basis, false,
                velocity_iterations, position_iterations));
        }
        const auto seat = kit_owner.body_for_entity(kDeformablePlankSeatEntity);
        const auto support_id = seat.valid() ? kit_owner.body_id(seat) : JPH::BodyID();
        if (seat.valid()) {
            // Only mating pin hardware is filtered, never the walking steel
            // or neighboring structure. These joints remain the load path.
            kit_owner.disable_collision(seat, segment_indices_.front());
            kit_owner.disable_collision(seat, segment_indices_.back());
        }
        if (support == Support::Cantilever) {
            attach_joint(support_id, body_ids_.front(), joint_settings(
                left_endpoint, world_basis, true, velocity_iterations, position_iterations));
        } else {
            attach_joint(support_id, body_ids_.front(), passive_support_settings(
                left_endpoint, world_basis, false, velocity_iterations, position_iterations,
                support == Support::GuidedPinRoller));
            attach_joint(support_id, body_ids_.back(), passive_support_settings(
                point(kLengthM), world_basis, true, velocity_iterations, position_iterations,
                support == Support::GuidedPinRoller));
        }
        rebuild_solve_groups();
    } catch (...) {
        // A failed construction cannot leave constraints referencing Kit bodies.
        // Kit retains any already-created bodies until its own destruction;
        // construction failure therefore requires rebuilding that fixture.
        clear_constraints();
        throw;
    }
}

void DeformablePlank::attach_joint(
    const JPH::BodyID first, const JPH::BodyID second,
    const JPH::SixDOFConstraintSettings &settings) {
    const JPH::BodyID ids[]{first, second};
    JPH::Ref<JPH::SixDOFConstraint> joint;
    {
        JPH::BodyLockMultiWrite lock(system_.GetBodyLockInterface(), ids, 2);
        JPH::Body *parent = first.IsInvalid() ? &JPH::Body::sFixedToWorld : lock.GetBody(0);
        JPH::Body *child = lock.GetBody(1);
        if (parent == nullptr || child == nullptr || !child->IsDynamic())
            throw std::runtime_error("deformable plank: segment body unavailable for joint creation");
        joint = create_joint(*parent, *child, settings);
    }
    // Capacity is reserved before creating bodies. Register only after body
    // locks are released, following the native hand constraint owner pattern.
    joints_.push_back(joint);
}

void DeformablePlank::clear_constraints() noexcept {
    for (const auto &group : solve_groups_) system_.RemoveConstraint(group.GetPtr());
    solve_groups_.clear();
    joints_.clear();
}

DeformablePlank::~DeformablePlank() {
    clear_constraints();
}

void DeformablePlank::reset_warm_start() noexcept {
    for (const auto &joint : joints_) joint->ResetWarmStart();
    pending_step_ = false;
    pending_dt_ = 0;
    queued_breaks_ = 0;
    right_support_release_queued_ = false;
}

void DeformablePlank::rebuild_solve_groups() {
    for (const auto &group : solve_groups_) system_.RemoveConstraint(group.GetPtr());
    solve_groups_.clear();
    for (std::size_t first = 0; first < kSegmentCount;) {
        std::size_t last = first;
        while (last + 1 < kSegmentCount && !(state_.broken_mask & (1u << last))) ++last;
        std::vector<JPH::Ref<JPH::SixDOFConstraint>> component;
        for (std::size_t j = first; j < last; ++j) component.push_back(joints_[j]);
        if (first == 0) component.push_back(joints_[kSegmentCount - 1]);
        if (last + 1 == kSegmentCount && joints_.size() > kSegmentCount &&
            !state_.right_support_released)
            component.push_back(joints_[kSegmentCount]);
        if (!component.empty()) {
            JPH::SixDOFConstraintSettings settings;
            settings.mNumVelocityStepsOverride = velocity_iterations_;
            settings.mNumPositionStepsOverride = position_iterations_;
            JPH::Ref<JPH::Constraint> group = new PlankSolveGroup(component, settings);
            system_.AddConstraint(group.GetPtr());
            solve_groups_.push_back(group);
        }
        first = last + 1;
    }
}

void DeformablePlank::begin_collision_step(const float dt) {
    if (!std::isfinite(dt) || dt <= 0)
        throw std::invalid_argument("deformable plank: positive finite substep required");
    observe_collision_step();
    pending_dt_ = dt;
    pending_step_ = true;
}

void DeformablePlank::observe_collision_step() {
    if (!pending_step_) return;
    pending_step_ = false;
    constexpr float area = kWidthM * kThicknessM;
    constexpr float weak_i = kWidthM * kThicknessM*kThicknessM*kThicknessM / 12;
    constexpr float strong_i = kThicknessM * kWidthM*kWidthM*kWidthM / 12;
    for (std::size_t j = 0; j + 1 < kSegmentCount; ++j) {
        const auto &joint = joints_[j];
        if (!joint->GetEnabled() || !joint->IsActive()) continue;
        // Partial translation lock and motor lambdas share frame1. Rotation
        // motors use frame2: transform before evaluating the section demand.
        JPH::Quat q1, q2;
        bool solved = false;
        for (const auto &group : solve_groups_)
            solved |= static_cast<const PlankSolveGroup *>(group.GetPtr())->solve_frames(joint.GetPtr(), q1, q2);
        if (!solved) continue;
        const auto force = (joint->GetTotalLambdaPosition() + joint->GetTotalLambdaMotorTranslation()) / pending_dt_;
        const auto moment = q1.Conjugated() * (q2 * joint->GetTotalLambdaMotorRotation()) / pending_dt_;
        const float bending = std::abs(moment.GetY()) * (.5F*kWidthM) / strong_i +
                              std::abs(moment.GetZ()) * (.5F*kThicknessM) / weak_i;
        // Positive force on the second body compresses the section.
        const float axial_tension = -force.GetX() / area;
        const float tensile = std::max(0.F, axial_tension + bending) / kTensileStrengthPa;
        const float compression = std::max(0.F, -axial_tension + bending) / kCompressiveStrengthPa;
        // Conservative rectangular section bound. alpha=.291 for b/t=5;
        // maxima need not coincide, so this is an authored failure envelope.
        const float shear = (1.5F * std::hypot(force.GetY(), force.GetZ()) / area +
            std::abs(moment.GetX()) / (.291F*kWidthM*kThicknessM*kThicknessM)) / kShearStrengthPa;
        const float ratio = std::sqrt(std::max(tensile*tensile, compression*compression) + shear*shear);
        state_.peak_strength_ratio = std::max(state_.peak_strength_ratio, ratio);
        if (strength_failure_enabled_ && ratio >= 1) queued_breaks_ |= 1u << j;
    }
    if (guided_support_ && !state_.right_support_released) {
        const auto &bearing = joints_[kSegmentCount];
        const auto first = bearing->GetBody1()->GetCenterOfMassTransform() * bearing->GetConstraintToBody1Matrix();
        const auto second = bearing->GetBody2()->GetCenterOfMassTransform() * bearing->GetConstraintToBody2Matrix();
        // The real fork/shoe is only90mm long. A fractured piece can leave
        // its roller; it must then lose bearing rather than hang in space.
        if (std::abs(JPH::Vec3(second.GetTranslation()-first.GetTranslation()).Dot(first.GetAxisX())) > .045F)
            right_support_release_queued_ = true;
    }
}

double DeformablePlank::joint_strain_energy(const std::size_t index) const {
    const auto &joint = joints_[index];
    const auto frame1 = joint->GetBody1()->GetCenterOfMassTransform() * joint->GetConstraintToBody1Matrix();
    const auto frame2 = joint->GetBody2()->GetCenterOfMassTransform() * joint->GetConstraintToBody2Matrix();
    const double extension = JPH::Vec3(frame2.GetTranslation()-frame1.GetTranslation()).Dot(frame1.GetAxisX());
    auto q = joint->GetRotationInConstraintSpace();
    if (q.GetW() < 0) q = -q;
    const auto v = q.GetXYZ();
    const float length = v.Length();
    const auto angle = length > 1.0e-8F ? v * (2.F*std::atan2(length,q.GetW())/length) : 2.F*v;
    return .5 * (kAxialStiffnessNPerM*extension*extension +
        kTorsionStiffnessNmPerRad*angle.GetX()*angle.GetX() +
        kStrongBendingStiffnessNmPerRad*angle.GetY()*angle.GetY() +
        kWeakBendingStiffnessNmPerRad*angle.GetZ()*angle.GetZ());
}

void DeformablePlank::finish_collision_steps() {
    observe_collision_step();
    pending_dt_ = 0;
    const auto breaks = queued_breaks_ & ~state_.broken_mask;
    queued_breaks_ = 0;
    if (!breaks && !right_support_release_queued_) return;
    for (std::size_t j = 0; j + 1 < kSegmentCount; ++j) if (breaks & (1u << j)) {
        state_.discarded_strain_energy_j += joint_strain_energy(j);
        state_.broken_mask |= 1u << j;
        ++state_.fracture_serial;
        joints_[j]->SetEnabled(false);
        joints_[j]->ResetWarmStart();
        kit_.set_pair_collision(segment_indices_[j], segment_indices_[j+1], true);
        auto &bodies = system_.GetBodyInterface();
        bodies.ActivateBody(body_ids_[j]);
        bodies.ActivateBody(body_ids_[j+1]);
    }
    if (right_support_release_queued_) {
        right_support_release_queued_ = false;
        state_.right_support_released = true;
        joints_[kSegmentCount]->SetEnabled(false);
        joints_[kSegmentCount]->ResetWarmStart();
        const auto seat = kit_.body_for_entity(kDeformablePlankSeatEntity);
        if (seat.valid()) kit_.set_pair_collision(seat, segment_indices_.back(), true);
        system_.GetBodyInterface().ActivateBody(body_ids_.back());
    }
    // No transform, mass, velocity or grip write. Only the failed rows cease
    // transferring force; real fragment motion follows the remaining solve.
    rebuild_solve_groups();
}

void DeformablePlank::restore(const State &state) {
    if (state.broken_mask & ~((1u << (kSegmentCount-1))-1))
        throw std::invalid_argument("deformable plank: invalid restored fracture mask");
    if (state.right_support_released && !guided_support_)
        throw std::invalid_argument("deformable plank: ideal support has no finite release state");
    const bool changed = state_.broken_mask != state.broken_mask ||
        state_.right_support_released != state.right_support_released;
    state_ = state;
    for (std::size_t j = 0; j + 1 < kSegmentCount; ++j) {
        const bool broken = state_.broken_mask & (1u << j);
        joints_[j]->SetEnabled(!broken);
        kit_.set_pair_collision(segment_indices_[j], segment_indices_[j+1], broken);
    }
    if (guided_support_) {
        joints_[kSegmentCount]->SetEnabled(!state_.right_support_released);
        const auto seat = kit_.body_for_entity(kDeformablePlankSeatEntity);
        if (seat.valid()) kit_.set_pair_collision(seat, segment_indices_.back(), state_.right_support_released);
    }
    reset_warm_start();
    if (changed) rebuild_solve_groups();
}

std::uint8_t DeformablePlank::fragment_for_body(const JPH::BodyID body) const noexcept {
    for (std::size_t segment = 0; segment < kSegmentCount; ++segment) if (body == body_ids_[segment]) {
        while (segment > 0 && !(state_.broken_mask & (1u << (segment-1)))) --segment;
        return static_cast<std::uint8_t>(segment);
    }
    return UINT8_MAX;
}

bool DeformablePlank::fragment_supported(const JPH::BodyID body) const noexcept {
    const auto first = fragment_for_body(body);
    if (first == UINT8_MAX) return false;
    if (first == 0) return true;
    std::size_t last = first;
    while (last + 1 < kSegmentCount && !(state_.broken_mask & (1u << last))) ++last;
    return last + 1 == kSegmentCount && joints_.size() > kSegmentCount && !state_.right_support_released;
}

std::uint64_t DeformablePlank::logical_member_for_body(const JPH::BodyID body) const noexcept {
    if (body.IsInvalid()) return 0;
    for (const auto segment : body_ids_)
        if (segment == body) return kDeformablePlankMemberEntity;
    return 0;
}

} // namespace scraperx::sim
