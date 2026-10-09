#pragma once

#include "sim/mechanism_kit.hpp"

#include <Jolt/Physics/Constraints/SixDOFConstraint.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace scraperx::sim {

// 1960/2960 belong to the suspended ladder and 2970/2971 to the service
// lift. These unused IDs preserve those existing owners.
inline constexpr std::uint64_t kDeformablePlankMemberEntity = 1962;
inline constexpr std::uint64_t kDeformablePlankSeatEntity = 1937;
inline constexpr std::uint64_t kDeformablePlankSegmentEntityBase = 2880;
inline constexpr std::size_t kDeformablePlankSegmentCount = 12;

[[nodiscard]] constexpr bool is_deformable_plank_segment_entity(
    const std::uint64_t entity) noexcept {
    return entity >= kDeformablePlankSegmentEntityBase &&
           entity < kDeformablePlankSegmentEntityBase + kDeformablePlankSegmentCount;
}

// Owner-thread construction/destruction only, outside PhysicsSystem::Update.
// Kit owns the twelve bodies; this object owns only their elastic constraints.
// Destroy this object before either Kit or PhysicsSystem, and before removing
// any segment body. A logical member is metadata, never a substitute support.
class DeformablePlank final {
public:
    enum class Support : std::uint8_t { PinRoller, Cantilever, GuidedPinRoller };

    static constexpr std::size_t kSegmentCount = kDeformablePlankSegmentCount;
    static constexpr float kLengthM = 2.4F;
    static constexpr float kSegmentLengthM = .2F;
    static constexpr float kWidthM = .19F;
    static constexpr float kThicknessM = .038F;
    static constexpr float kYoungsModulusPa = 9.0e9F;
    static constexpr float kDensityKgPerM3 = 500.0F;
    static constexpr float kSegmentMassKg = .722F;
    static constexpr float kAxialStiffnessNPerM = 297825000.0F;
    static constexpr float kWeakBendingStiffnessNmPerRad = 39096.3F;
    static constexpr float kStrongBendingStiffnessNmPerRad = 977407.5F;
    // Explicit isotropic wood surrogate, not measured orthotropic properties:
    // nu=.30, G=E/[2(1+nu)], rectangular Saint-Venant J, k=GJ/.2.
    static constexpr float kAssumedPoissonRatio = .30F;
    static constexpr float kTorsionStiffnessNmPerRad = 52570.5F;
    // Kelvin-Voigt material-loss surrogate: c=tau*k for every elastic axis.
    // tau=2*zeta/omega1, omega1=pi^2*sqrt(EI/(rho*A))/L^2 for the
    // pin/roller beam. The .10 first-mode ratio is an authoring assumption,
    // not measured wood damping or a claim about all other modes/supports.
    static constexpr float kFirstModeDampingRatio = .10F;
    static constexpr float kViscousTimeSeconds = .002508006F;
    // Explicit game-authoring assumptions, not certified timber grading.
    // Sound timber rather than a weak/defected construction offcut.
    static constexpr float kTensileStrengthPa = 40.0e6F;
    static constexpr float kCompressiveStrengthPa = 45.0e6F;
    static constexpr float kShearStrengthPa = 6.0e6F;
    struct State {
        std::uint16_t broken_mask = 0;
        bool right_support_released = false;
        float peak_strength_ratio = 0;
        std::uint64_t fracture_serial = 0;
        // Small-rotation strain estimate discarded at instantaneous rupture.
        // This is dissipated, never converted to an extra launch impulse.
        double discarded_strain_energy_j = 0;
    };

    DeformablePlank(JPH::PhysicsSystem &system, kit::Kit &kit,
                    JPH::RVec3 left_endpoint, Support support,
                    JPH::Quat world_basis = JPH::Quat::sIdentity(),
                    std::uint32_t velocity_iterations = 0,
                    std::uint32_t position_iterations = 0);
    ~DeformablePlank();
    DeformablePlank(const DeformablePlank &) = delete;
    DeformablePlank &operator=(const DeformablePlank &) = delete;
    DeformablePlank(DeformablePlank &&) = delete;
    DeformablePlank &operator=(DeformablePlank &&) = delete;

    [[nodiscard]] const std::array<JPH::BodyID, kSegmentCount> &body_ids() const noexcept {
        return body_ids_;
    }
    [[nodiscard]] const std::array<kit::BodyIndex, kSegmentCount> &segment_indices() const noexcept {
        return segment_indices_;
    }
    // Zero means this body is not one of this object's actual twelve segments.
    [[nodiscard]] std::uint64_t logical_member_for_body(JPH::BodyID body) const noexcept;
    void reset_warm_start() noexcept;
    void enable_strength_failure(bool enabled = true) noexcept { strength_failure_enabled_ = enabled; }
    // Observe every preceding substep; mutate topology only outside Update.
    void begin_collision_step(float dt);
    void finish_collision_steps();
    [[nodiscard]] State capture() const noexcept { return state_; }
    void restore(const State &state);
    [[nodiscard]] std::uint8_t fragment_for_body(JPH::BodyID body) const noexcept;
    [[nodiscard]] bool fragment_supported(JPH::BodyID body) const noexcept;
    [[nodiscard]] std::uint32_t velocity_iterations() const noexcept { return velocity_iterations_; }
    [[nodiscard]] std::uint32_t position_iterations() const noexcept { return position_iterations_; }

    // Shared by the real chain and isolated axis tests. Rest axes are local
    // X length / Y thickness / Z width, transformed by world_basis. Anchors
    // coincide in world space. A fixed endpoint uses half-cell rotational
    // compliance (2*k), while all translations there are locked.
    [[nodiscard]] static JPH::SixDOFConstraintSettings joint_settings(
        JPH::RVec3 anchor, JPH::Quat world_basis,
        bool fixed_endpoint = false, std::uint32_t velocity_iterations = 80,
        std::uint32_t position_iterations = 8);

    // Body references must be protected by the caller's body-write locks.
    // Returns an unregistered joint with zero rest targets and Position mode
    // only on configured elastic axes. Caller owns Add/RemoveConstraint.
    [[nodiscard]] static JPH::Ref<JPH::SixDOFConstraint> create_joint(
        JPH::Body &first, JPH::Body &second,
        const JPH::SixDOFConstraintSettings &settings);

private:
    void attach_joint(JPH::BodyID first, JPH::BodyID second,
                      const JPH::SixDOFConstraintSettings &settings);
    void clear_constraints() noexcept;
    void rebuild_solve_groups();
    void observe_collision_step();
    [[nodiscard]] double joint_strain_energy(std::size_t joint) const;

    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    std::array<kit::BodyIndex, kSegmentCount> segment_indices_{};
    std::array<JPH::BodyID, kSegmentCount> body_ids_{};
    std::vector<JPH::Ref<JPH::SixDOFConstraint>> joints_;
    std::vector<JPH::Ref<JPH::Constraint>> solve_groups_;
    State state_{};
    std::uint16_t queued_breaks_ = 0;
    float pending_dt_ = 0;
    bool pending_step_ = false;
    bool strength_failure_enabled_ = false;
    bool guided_support_ = false;
    bool right_support_release_queued_ = false;
    std::uint32_t velocity_iterations_ = 0;
    std::uint32_t position_iterations_ = 0;
};

} // namespace scraperx::sim
