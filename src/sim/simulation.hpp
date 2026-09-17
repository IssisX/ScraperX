#pragma once

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
    ApproachGrade = 3,
    HopperControl = 4,
    TraversalCourse = 5,
    HangCourse = 6,
};

enum class TraversalMode : std::uint8_t {
    None = 0,
    Vault = 1,
    Mantle = 2,
    Hang = 3,
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

    TraversalMode traversal_mode = TraversalMode::None;
    TraversalMode traversal_candidate_mode = TraversalMode::None;
    bool traversal_assist_available = false;
    Vector3 traversal_target_position{};

    Vector3 translating_support_position{};
    Vector3 translating_support_linear_velocity{};
    Vector3 rotating_support_position{};
    double rotating_support_yaw_radians = 0.0;
    Vector3 rotating_support_angular_velocity{};

    Vector3 hopper_control_position{};
    Vector3 hopper_gate_position{};
    Vector3 hopper_load_position{};
    Vector3 hopper_load_linear_velocity{};
    bool hopper_interaction_available = false;
    bool hopper_release_started = false;
    bool hopper_gate_open = false;
    bool hopper_load_moved = false;

    Vector3 impact_rocker_position{};
    Vector3 impact_rocker_angular_velocity{};
    double impact_rocker_angle_radians = 0.0;
    bool impact_rocker_struck = false;
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
    static constexpr std::uint64_t kHopperGateEntityId = 5;
    static constexpr std::uint64_t kHopperLoadEntityId = 6;
    static constexpr std::uint64_t kHopperChuteEntityId = 7;
    static constexpr std::uint64_t kTowerLeftPierEntityId = 8;
    static constexpr std::uint64_t kTowerRightPierEntityId = 9;
    static constexpr std::uint64_t kHopperLeftWallEntityId = 10;
    static constexpr std::uint64_t kHopperRightWallEntityId = 11;
    static constexpr std::uint64_t kImpactRockerEntityId = 12;
    static constexpr std::uint64_t kVaultBlockEntityId = 13;
    static constexpr std::uint64_t kMantleBlockEntityId = 14;
    static constexpr std::uint64_t kHangLedgeEntityId = 15;

    explicit Simulation(InitialSpawn initial_spawn = InitialSpawn::ApproachGrade);
    ~Simulation();

    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;
    Simulation(Simulation &&) = delete;
    Simulation &operator=(Simulation &&) = delete;

    [[nodiscard]] bool set_move_input(double world_x, double world_z) noexcept;
    [[nodiscard]] bool request_jump() noexcept;
    [[nodiscard]] bool can_traverse() const noexcept;
    [[nodiscard]] bool request_traversal() noexcept;
    [[nodiscard]] bool request_drop_from_hang() noexcept;
    [[nodiscard]] bool can_operate_hopper() const noexcept;
    [[nodiscard]] bool request_hopper_release() noexcept;
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
    bool jump_requested_ = false;
    bool traversal_requested_ = false;
    bool drop_from_hang_requested_ = false;
    bool hopper_release_requested_ = false;
    Snapshot snapshot_{};
};

} // namespace scraperx::sim
