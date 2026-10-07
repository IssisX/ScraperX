#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <array>

namespace scraperx::sim {
// Owner-thread operations: attach/detach/commands run outside PhysicsSystem::Update.
// Clear before removing either constrained body, and before destroying the system.
// The production player's rotational DOFs must remain locked while attached.
class CargoNet;
class PhysicalHandClimb final {
public:
    PhysicalHandClimb(JPH::PhysicsSystem &system, JPH::BodyID player, CargoNet *net = nullptr);
    ~PhysicalHandClimb();
    PhysicalHandClimb(const PhysicalHandClimb &) = delete;
    PhysicalHandClimb &operator=(const PhysicalHandClimb &) = delete;
    bool attach(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip);
    // Relocate a real grip while preserving the current spring extension.
    bool regrip(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip);
    void detach(unsigned hand);
    void clear();
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool attached(unsigned hand) const noexcept;
    // Brief ledge pull-up: higher bounded command power, damped rigid hands.
    void set_transfer_profile();
    void advance_targets(JPH::Vec3 desired_player_displacement, float dt);
    // Neutral actuator position from current anchors/targets; not actual pose.
    [[nodiscard]] JPH::RVec3 commanded_position() const;
    // Jolt exposes the LAST collision-step impulse, not the outer update's sum.
    // Pass that collision step's dt (shipping Update(h,4): h/4). May be sampled
    // under the step-listener lock; it reads constraints, never acquires body locks.
    // Soft/rigid material coupling runs in the single serialized collision callback.
    void pre_step(float dt, bool bodies_locked = false);
    void post_step(float collision_dt);
    [[nodiscard]] JPH::Vec3 hand_force(unsigned hand) const noexcept;
    [[nodiscard]] float peak_hand_force_n() const noexcept { return peak_force_; }
    [[nodiscard]] static constexpr float force_bound_n() noexcept { return 1500.0F; }
    // Default sustained climbing budget; transfer has a separate finite burst.
    [[nodiscard]] float active_command_power_bound_w() const noexcept { return transfer_profile_ ? 6600.0F : command_power_bound_w(); }
    [[nodiscard]] static constexpr float command_power_bound_w() noexcept { return 3000.0F; }
    // Conservative Fmax * commanded-target-distance debit, NOT motor work,
    // spring storage, positive muscle work, or an energy-conservation ledger.
    [[nodiscard]] double command_work_bound_j() const noexcept { return command_bound_; }
    [[nodiscard]] double last_command_work_bound_j() const noexcept { return last_bound_; }
private:
    bool replace(unsigned hand, JPH::BodyID support, JPH::RVec3 actual_grip, bool preserve_extension);
    JPH::PhysicsSystem &system_;
    JPH::BodyID player_;
    CargoNet *net_;
    struct SoftHand {
        bool attached=false;
        JPH::Vec3 material=JPH::Vec3::sZero();
        JPH::Vec3 rest_offset=JPH::Vec3::sZero();
    };
    std::array<SoftHand, 2> soft_;
    std::array<JPH::Ref<JPH::SixDOFConstraint>, 2> hands_;
    std::array<JPH::Vec3, 2> forces_ { JPH::Vec3::sZero(), JPH::Vec3::sZero() };
    bool transfer_profile_ = false;
    float peak_force_ = 0.0F;
    double command_bound_ = 0.0, last_bound_ = 0.0;
};
}
