#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <array>
#include <cstdint>
#include <optional>

namespace scraperx::sim {
// Owner-thread operations: attach/detach/commands run outside PhysicsSystem::Update.
// Clear before removing either constrained body, and before destroying the system.
// The production player's rotational DOFs must remain locked while attached.
class CargoNet;
class PhysicalHandClimb final {
public:
    struct GripReadback final {
        bool attached = false;
        bool material = false;
        JPH::BodyID support;
        JPH::RVec3 world_anchor = JPH::RVec3::sZero();
        JPH::Vec3 point_velocity = JPH::Vec3::sZero();
        JPH::Vec3 player_force = JPH::Vec3::sZero();
        std::uint64_t generation = 0;
    };
    // Owner thread, outside Update: actual per-hand anchor/point velocity and
    // last solved player force. Missing bodies yield an unattached readback.
    // Generation changes only on successful attachment/regrip or actual release;
    // switching actuator profiles preserves the attachment identity.
    [[nodiscard]] GripReadback grip_readback(unsigned hand) const;
    PhysicalHandClimb(JPH::PhysicsSystem &system, JPH::BodyID player, CargoNet *net = nullptr);
    ~PhysicalHandClimb();
    PhysicalHandClimb(const PhysicalHandClimb &) = delete;
    PhysicalHandClimb &operator=(const PhysicalHandClimb &) = delete;
    bool attach(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip);
    // Owner thread, outside Update: add only a missing rigid radial hand.
    // Retains the planted row/rest/cache/identity and all actuator ledgers;
    // the new row starts at its actual radius with no added spring preload.
    bool attach_radial_hand(unsigned missing, JPH::BodyID support, JPH::RVec3 actual_grip);
    // Relocate a real grip while preserving the current spring extension.
    bool regrip(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip);
    void detach(unsigned hand);
    void clear();
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool attached(unsigned hand) const noexcept;
    [[nodiscard]] unsigned attached_count() const noexcept;
    // Brief ledge pull-up: higher bounded command power, damped rigid hands.
    void set_transfer_profile(float rigid_hand_damping = 600.0F);
    // Passive mounted hanger: bounded human lean commands, not a hinge drive.
    void set_swing_profile();
    [[nodiscard]] bool swinging() const noexcept { return swing_profile_; }
    // Dissipative controller handoff, preserving both bodies and local anchors.
    bool set_radial_profile(bool enabled);
    [[nodiscard]] bool radial_active() const noexcept { return radial_profile_; }
    // Positive shortens. Actual representable mean stroke, 500W / 0.6m/s.
    float advance_radial_targets(float shortening_m, float dt);
    [[nodiscard]] float radial_commanded_span_m() const;
    [[nodiscard]] float radial_actual_span_m() const;
    // Optional shared-posture cap uses the same spring-work budget and 0.6m/s
    // combined stroke as swing lean. Omission preserves climb/transfer limits.
    // Returns the mean actual representable rest-target displacement.
    JPH::Vec3 advance_targets(JPH::Vec3 desired_player_displacement, float dt,
                             std::optional<float> command_power_cap_w = std::nullopt);
    [[nodiscard]] bool compatible_hands() const noexcept;
    [[nodiscard]] JPH::RVec3 grip_midpoint() const;
    // Passive radial ownership: a compatible pair or exactly one rigid hand.
    // Full climbing/transfer still require compatible_hands(). No material solo.
    [[nodiscard]] bool radial_compatible_hands() const noexcept;
    [[nodiscard]] JPH::RVec3 radial_grip_centroid() const;
    // Neutral actuator position from current anchors/targets; not actual pose.
    [[nodiscard]] JPH::RVec3 commanded_position() const;
    // pre_step applies only the soft material adapter; rigid radial rows solve
    // inside Jolt's ordinary joint/contact island. The production listener owns
    // body locks already: it must pass bodies_locked=true, outside Update false.
    void pre_step(float dt, bool bodies_locked = false);
    // Cartesian readback needs the LAST collision-step dt (Update(h,4): h/4).
    // Radial readback uses the row's actual last solved dt and axis, never an
    // outer-update impulse sum or pre-solver force estimate. No body locks.
    void post_step(float collision_dt);
    [[nodiscard]] JPH::Vec3 hand_force(unsigned hand) const noexcept;
    [[nodiscard]] float peak_hand_force_n() const noexcept { return peak_force_; }
    [[nodiscard]] static constexpr float force_bound_n() noexcept { return 1500.0F; }
    // Default sustained climbing budget; transfer has a separate finite burst.
    [[nodiscard]] float active_command_power_bound_w() const noexcept { return radial_profile_ ? 500.0F : transfer_profile_ ? 6600.0F : swing_profile_ ? 500.0F : command_power_bound_w(); }
    [[nodiscard]] static constexpr float command_power_bound_w() noexcept { return 3000.0F; }
    // Conservative Fmax * commanded-target-distance debit, NOT motor work,
    // spring storage, positive muscle work, or an energy-conservation ledger.
    [[nodiscard]] double command_work_bound_j() const noexcept { return command_bound_; }
    [[nodiscard]] double last_command_work_bound_j() const noexcept { return last_bound_; }
    // Work supplied/absorbed by changing the actual spring rest targets at
    // fixed body poses: Cartesian delta(1/2*k*extension^2), radial delta of the
    // unilateral quadratic-to-linear potential capped at1500N. Separate from Fmax-distance
    // and solver motor work. Implicit damping/solver residual are not inferred
    // from this receipt; this is not a claim of complete energy closure.
    [[nodiscard]] double actuator_positive_work_j() const noexcept { return actuator_positive_work_; }
    [[nodiscard]] double actuator_absorbed_work_j() const noexcept { return actuator_absorbed_work_; }
    [[nodiscard]] double last_actuator_positive_work_j() const noexcept { return last_actuator_positive_work_; }
private:
    bool replace(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip, bool preserve_extension);
    [[nodiscard]] JPH::Vec3 spring_extension(unsigned hand) const;
    [[nodiscard]] float radial_rest_length(unsigned hand) const;
    JPH::PhysicsSystem &system_;
    JPH::BodyID player_;
    CargoNet *net_;
    struct SoftHand {
        bool attached=false;
        JPH::Vec3 material=JPH::Vec3::sZero();
        JPH::Vec3 rest_offset=JPH::Vec3::sZero();
    };
    std::array<SoftHand, 2> soft_;
    std::array<std::uint64_t, 2> grip_generations_{};
    // In radial mode these SixDOFs are dormant anchor/profile readback only.
    // Exactly one backend is registered per rigid hand. Private User2 radial
    // settings recreate the finite law; global archive registration is not owned
    // here. Row StateRecorder snapshots include rest length/solved impulse, not
    // this owner's attachment topology or actuator work ledger.
    std::array<JPH::Ref<JPH::SixDOFConstraint>, 2> hands_;
    std::array<JPH::Ref<JPH::TwoBodyConstraint>, 2> radial_hands_;
    std::array<JPH::Vec3, 2> forces_ { JPH::Vec3::sZero(), JPH::Vec3::sZero() };
    // Soft rest lengths / rigid commit mirrors. Rigid readback and subsequent
    // charging use the actual row rest length, including after row state restore.
    std::array<float, 2> radial_lengths_ { 0.0F, 0.0F };
    bool radial_profile_ = false;
    bool transfer_profile_ = false;
    bool swing_profile_ = false;
    float peak_force_ = 0.0F;
    double command_bound_ = 0.0, last_bound_ = 0.0;
    double actuator_positive_work_ = 0.0, actuator_absorbed_work_ = 0.0;
    double last_actuator_positive_work_ = 0.0;
};
}
