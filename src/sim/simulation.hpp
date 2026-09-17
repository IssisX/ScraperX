#pragma once

#include "sim/steam_plant.hpp"

#include <cstdint>
#include <memory>

namespace scraperx::sim {

struct Vector3 final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class InitialSpawn : std::uint8_t {
    StaticDeck = 0,
    TranslatingSupport = 1,
    RotatingSupport = 2,
    VaultApproach = 3,
    MantleApproach = 4,
    HangApproach = 5,
    MovingLedgeApproach = 6,
    BlockedLedgeApproach = 7,
    ExteriorGrade = 8,
    MachineYard = 9,
    LiftPlatform = 10,
};

enum class TraversalState : std::uint8_t {
    None = 0,
    Hanging = 1,
    Mantling = 2,
    Vaulting = 3,
};

struct Snapshot final {
    std::uint64_t tick_index = 0;
    double simulation_time_seconds = 0.0;
    double fixed_step_seconds = 0.0;
    double interpolation_alpha = 0.0;
    Vector3 player_position{};
    Vector3 player_linear_velocity{};
    bool player_grounded = false;
    std::uint64_t support_entity_id = 0;
    Vector3 support_contact_point{};
    Vector3 support_point_linear_velocity{};
    Vector3 translating_support_position{};
    Vector3 translating_support_linear_velocity{};
    Vector3 rotating_support_position{};
    double rotating_support_yaw_radians = 0.0;
    Vector3 rotating_support_angular_velocity{};
    Vector3 moving_ledge_position{};
    Vector3 moving_ledge_linear_velocity{};
    TraversalState traversal_state = TraversalState::None;
    std::uint64_t traversal_support_entity_id = 0;
    Vector3 traversal_ledge_point{};
    Vector3 traversal_target_point{};
    double traversal_progress = 0.0;
    bool ledge_available = false;
    std::uint64_t ledge_entity_id = 0;
    Vector3 ledge_point{};
    double ledge_rise_meters = 0.0;
    std::uint64_t accepted_traversal_count = 0;
    std::uint64_t rejected_traversal_count = 0;
    std::uint64_t aborted_traversal_count = 0;

    // --- coupled machine: every field below is read back from the authoritative
    // Jolt bodies or from the reduced-order steam plant, never authored.
    Vector3 hoist_scoop_position{};
    double hoist_scoop_tilt_radians = 0.0;
    Vector3 ballast_position{};
    Vector3 ballast_linear_velocity{};
    Vector3 tipper_position{};
    double tipper_angle_radians = 0.0;
    double valve_lever_angle_radians = 0.0;
    double valve_open_fraction = 0.0;
    double rope_extension_meters = 0.0;
    Vector3 lift_platform_position{};
    Vector3 lift_platform_linear_velocity{};
    Vector3 counterweight_position{};
    double vessel_pressure_pa = 0.0;
    double cylinder_pressure_pa = 0.0;
    double orifice_mass_flow_kg_per_s = 0.0;
    double vented_mass_kg = 0.0;
    double piston_force_n = 0.0;
    double vessel_available_energy_j = 0.0;
    double machine_cycle_phase_seconds = 0.0;
};

struct AdvanceResult final {
    bool accepted = false;
    std::uint32_t steps_advanced = 0;
};

class Simulation final {
public:
    static constexpr std::uint32_t kTickRateHz = 90;
    static constexpr double kFixedStepSeconds = 1.0 / static_cast<double>(kTickRateHz);
    static constexpr double kMaximumAcceptedFrameDeltaSeconds = 3600.0;
    static constexpr std::uint64_t kStaticDeckEntityId = 1;
    static constexpr std::uint64_t kPlayerEntityId = 2;
    static constexpr std::uint64_t kTranslatingSupportEntityId = 3;
    static constexpr std::uint64_t kRotatingSupportEntityId = 4;
    static constexpr std::uint64_t kVaultRailEntityId = 5;
    static constexpr std::uint64_t kMantleLedgeEntityId = 6;
    static constexpr std::uint64_t kHangLedgeEntityId = 7;
    static constexpr std::uint64_t kMovingLedgeEntityId = 8;
    static constexpr std::uint64_t kBlockedLedgeEntityId = 9;
    static constexpr std::uint64_t kBlockedLedgeCanopyEntityId = 10;
    static constexpr std::uint64_t kTowerEntityId = 11;
    static constexpr std::uint64_t kHoistScoopEntityId = 12;
    static constexpr std::uint64_t kBallastEntityId = 13;
    static constexpr std::uint64_t kTipperEntityId = 14;
    static constexpr std::uint64_t kValveLeverEntityId = 15;
    static constexpr std::uint64_t kLiftPlatformEntityId = 16;
    static constexpr std::uint64_t kCounterweightEntityId = 17;
    static constexpr std::uint64_t kVesselShellEntityId = 18;
    static constexpr std::uint64_t kCatwalkEntityId = 19;
    static constexpr std::uint64_t kMachinePylonEntityId = 20;
    static constexpr std::uint64_t kCatchBasinEntityId = 21;
    static constexpr std::uint64_t kChuteEntityId = 22;
    static constexpr std::uint64_t kLiftMastEntityId = 23;

    // Height of the tower mass, metres. The crown is far past anything the
    // player can resolve from grade; haze and stack plume shear it earlier.
    static constexpr double kTowerHeightMeters = 1600.0;

    explicit Simulation(InitialSpawn initial_spawn = InitialSpawn::ExteriorGrade);
    ~Simulation();

    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;
    Simulation(Simulation &&) = delete;
    Simulation &operator=(Simulation &&) = delete;

    [[nodiscard]] bool set_move_input(double world_x, double world_z) noexcept;
    [[nodiscard]] bool set_facing(double world_x, double world_z) noexcept;
    [[nodiscard]] bool request_jump() noexcept;
    [[nodiscard]] bool request_traversal() noexcept;
    [[nodiscard]] bool request_release() noexcept;

    // Disables the boiler feed so the plant becomes a strictly finite reservoir.
    // Used to prove the machine cannot manufacture work.
    void set_boiler_feed_enabled(bool enabled) noexcept;
    [[nodiscard]] AdvanceResult advance_frame(double frame_delta_seconds) noexcept;
    [[nodiscard]] Snapshot snapshot() const noexcept;

private:
    class PhysicsWorld;

    void step_fixed() noexcept;

    std::unique_ptr<PhysicsWorld> physics_world_;
    std::uint64_t tick_index_ = 0;
    double remainder_seconds_ = 0.0;
    double move_input_x_ = 0.0;
    double move_input_z_ = 0.0;
    double facing_x_ = 0.0;
    double facing_z_ = 0.0;
    bool jump_requested_ = false;
    bool traversal_requested_ = false;
    bool release_requested_ = false;
    Snapshot snapshot_{};
};

} // namespace scraperx::sim
