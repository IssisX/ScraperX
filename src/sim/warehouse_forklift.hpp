#pragma once

#include "sim/mechanism_kit.hpp"
#include <array>
#include <memory>
#include <string>

namespace scraperx::sim {

// Kit owns the solids, mass, COM and body checkpoints. This owner adds only
// finite actuators, the native three-wheel vehicle and tool coupling. All
// methods run serially at the host's existing physics boundaries.
class WarehouseForklift final {
public:
    struct Config {
        kit::BodyIndex chassis, mast, carriage, fork_frame;
        JPH::ObjectLayer wheel_layer = 1;
        // Authored world-space joint frames at construction; zero travel is
        // the authored initial pose. Truck local +Z is forward, +Y is up.
        JPH::RVec3 tilt_pivot = JPH::RVec3::sZero();
        JPH::Vec3 tilt_axis = JPH::Vec3::sAxisX();
        JPH::Vec3 tilt_normal = JPH::Vec3::sAxisY();
        JPH::Vec3 lift_axis = JPH::Vec3::sAxisY();
        std::array<JPH::Vec3, 3> wheel_positions{
            JPH::Vec3(-.66F, -.18F, .75F), JPH::Vec3(.66F, -.18F, .75F),
            JPH::Vec3(0, -.18F, -.90F)};
        float wheel_radius_m = .30F, wheel_width_m = .22F;
        float suspension_min_m = .06F, suspension_max_m = .14F;
        float suspension_frequency_hz = 5, suspension_damping = 1;
        float drive_torque_nm = 180, engine_max_rpm = 1800, reduction = 18;
        float wheel_brake_torque_nm = 1600;
        float steering_angle_rad = .90F, steering_rate_rad_s = .70F;
        float lift_stroke_m = 2.4F, lift_speed_mps = .32F, lift_acceleration_mps2 = .65F;
        float lift_force_n = 18000, lift_brake_force_n = 22000;
        float tilt_min_rad = -.139626F, tilt_max_rad = .209440F;
        float tilt_speed_rad_s = .10F, tilt_acceleration_rad_s2 = .25F;
        float tilt_torque_nm = 14000, tilt_brake_torque_nm = 17000;
        double battery_capacity_j = 12000000, hydraulic_power_w = 12000;
        double efficiency = .80;
        // Mating hardware in each body's shape-origin frame, not COM frame.
        JPH::Vec3 carriage_mount = JPH::Vec3::sZero();
        JPH::Vec3 tool_mount = JPH::Vec3::sZero();
        JPH::Quat carriage_mount_rotation = JPH::Quat::sIdentity();
        JPH::Quat tool_mount_rotation = JPH::Quat::sIdentity();
        float capture_distance_m = .04F, capture_angle_rad = .05F;
        float capture_speed_mps = .12F, capture_angular_speed_rad_s = .10F;
        bool initially_coupled = true;
    };
    struct Command {
        // Positive steer follows Jolt right: for local +Z forward, yaw turns
        // toward local -X. Presentation maps its input convention here.
        float drive = 0, steer = 0, lift = 0, tilt = 0, brake = 1;
    };
    struct State {
        double battery_j = 0, reserved_debit_j = 0;
        double hydraulic_positive_work_j = 0, hydraulic_dissipation_j = 0;
        double drivetrain_work_estimate_j = 0, reservation_excess_j = 0;
        float steering_rad = 0, lift_target_mps = 0, tilt_target_rad_s = 0;
        float lift_position_m = 0, tilt_angle_rad = 0;
        float lift_force_n = 0, tilt_torque_nm = 0;
        float applied_drive = 0, applied_brake = 1;
        bool energy_cutoff = false, coupled = false;
        std::uint64_t coupling_generation = 0;
    };
    struct WheelReceipt {
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Quat rotation = JPH::Quat::sIdentity();
        JPH::BodyID contact_body;
        JPH::RVec3 contact_position = JPH::RVec3::sZero();
        JPH::Vec3 contact_normal = JPH::Vec3::sZero();
        float radius_m = 0, width_m = 0, angular_velocity_rad_s = 0;
        float steer_rad = 0, suspension_length_m = 0;
        float normal_force_n = 0, longitudinal_force_n = 0, lateral_force_n = 0;
        bool contact = false;
    };
    struct Checkpoint {
        State state;
        // Pinned Jolt replay data: engine/transmission, wheel spin/suspension,
        // joint motor state and warm starts. Not a portable save-file format.
        std::string native_state;
    };
    WarehouseForklift(JPH::PhysicsSystem &world, kit::Kit &kit, const Config &config);
    ~WarehouseForklift();
    WarehouseForklift(const WarehouseForklift &) = delete;
    WarehouseForklift &operator=(const WarehouseForklift &) = delete;
    // Host supplies only station-authorized commands. Invalid inputs neutralize.
    void pre_step(const Command &command);
    // The registered serial listener prepares each actual collision substep
    // then runs VehicleConstraint::OnStep. Host calls only pre/post_step.
    void post_step();
    [[nodiscard]] State state() const;
    [[nodiscard]] const Config &config() const;
    [[nodiscard]] std::array<WheelReceipt, 3> wheels() const;
    void capture(Checkpoint &out) const;
    // Restore Kit first, then this owner. Restores native replay state and
    // neutralizes demand; rebuilding joints preserves their captured frames.
    void restore(const Checkpoint &checkpoint);
    // Shared eligibility queries are also used by the mutations. These inspect
    // live Jolt contacts/joints and never cache support or coupling authority.
    enum class ToolRefusal : std::uint8_t {
        None = 0, StepPending = 6, AlreadyCoupled = 7, AlreadyDetached = 8,
        HardwareAlignment = 9, Unsupported = 10, MatingObstructed = 11,
        JointLoaded = 12
    };
    [[nodiscard]] ToolRefusal couple_refusal() const;
    [[nodiscard]] ToolRefusal detach_refusal() const;
    // Current-pose joint, never pose snapping. Capture requires aligned slow
    // hardware, unobstructed mating line and real support under the loose tool.
    bool try_couple();
    // Requires actual static support, slow tool and no suspended joint load.
    bool try_detach();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace scraperx::sim
