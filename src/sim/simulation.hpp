#pragma once

#include <cstddef>
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
    HighDeck = 7,
    JibStation = 8,
    JibOverweight = 9,
    NeedleBay = 10,
    NeedleNearLanding = 11,
    NeedleSeated = 12,
    CageDeck = 13,
    CageSeated = 14,
    SumpLanding = 15,
    SumpDrained = 16,
    ScrewDeck = 17,
    KernelChain = 18,
    RefugeDeck = 19,
};

enum class TraversalMode : std::uint8_t {
    None = 0,
    Vault = 1,
    Mantle = 2,
    Hang = 3,
};

enum class HookLoad : std::uint8_t {
    None = 0,
    Crate = 1,
    Needle = 2,
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

    bool player_alive = true;
    bool parachute_deployed = false;
    bool parachute_allowed = false;
    std::uint8_t fall_severity = 0;
    double airborne_seconds = 0.0;
    std::uint32_t fear_event_id = 0;
    std::uint64_t checkpoint_tick = 0;
    bool checkpoint_committed = false;

    Vector3 impact_rocker_position{};
    Vector3 impact_rocker_angular_velocity{};
    double impact_rocker_angle_radians = 0.0;
    bool impact_rocker_struck = false;

    Vector3 jib_pendant_position{};
    Vector3 jib_mast_position{};
    Vector3 jib_boom_tip_position{};
    Vector3 jib_hook_position{};
    Vector3 jib_crate_position{};
    Vector3 jib_crate_linear_velocity{};
    double jib_slew_radians = 0.0;
    double jib_winch_length_meters = 0.0;
    double jib_crate_mass_kg = 0.0;
    double jib_swl_kg = 0.0;
    double jib_load_newtons = 0.0;
    bool jib_station_available = false;
    bool jib_station_occupied = false;
    bool jib_brake_engaged = false;
    bool jib_stalled = false;
    bool jib_at_hoist_limit = false;
    bool jib_at_slew_limit = false;
    bool jib_hook_attached = false;
    HookLoad jib_hook_load = HookLoad::None;

    Vector3 needle_position{};
    Vector3 needle_linear_velocity{};
    double needle_yaw_radians = 0.0;
    bool needle_seated = false;
    Vector3 needle_near_landing_position{};
    Vector3 needle_far_landing_position{};
    Vector3 needle_bay_floor_position{};

    Vector3 cage_position{};
    Vector3 cage_linear_velocity{};
    Vector3 cage_lever_position{};
    Vector3 cage_upper_landing_position{};
    bool cage_lever_available = false;
    bool cage_brake_engaged = true;
    bool cage_stalled = false;
    bool cage_at_limit = false;
    double cage_command = 0.0;

    Vector3 sump_grate_position{};
    Vector3 sump_valve_position{};
    Vector3 sump_drain_position{};
    Vector3 sump_far_landing_position{};
    Vector3 sump_floor_position{};
    bool sump_valve_available = false;
    bool sump_drain_available = false;
    bool sump_isolated = false;
    bool sump_drain_open = false;
    bool sump_grate_safe = false;
    double sump_inventory = 1.0;

    Vector3 screw_position{};
    Vector3 screw_linear_velocity{};
    Vector3 screw_wheel_position{};
    Vector3 refuge_position{};
    bool screw_wheel_available = false;
    bool screw_brake_engaged = true;
    bool screw_stalled = false;
    bool screw_at_limit = false;
    double screw_command = 0.0;
    bool refuge_occupied = false;
};

struct CheckpointRecord final {
    static constexpr char kMagic[4] = {'S', 'X', 'K', '1'};
    static constexpr std::uint32_t kVersion = 1;

    char magic[4]{'S', 'X', 'K', '1'};
    std::uint32_t version = kVersion;
    double player_x = 0.0;
    double player_y = 0.0;
    double player_z = 0.0;
    double player_vx = 0.0;
    double player_vy = 0.0;
    double player_vz = 0.0;
    std::uint8_t needle_seated = 0;
    double needle_x = 0.0;
    double needle_y = 0.0;
    double needle_z = 0.0;
    std::uint8_t sump_isolated = 0;
    std::uint8_t sump_drain_open = 0;
    double sump_inventory = 1.0;
    double screw_y = 0.0;
    double screw_command = 0.0;
    std::uint8_t screw_brake = 1;
    double cage_y = 0.0;
    double cage_command = 0.0;
    std::uint8_t cage_brake = 1;
    double crate_x = 0.0;
    double crate_y = 0.0;
    double crate_z = 0.0;
    std::uint64_t checkpoint_tick = 0;
    std::uint8_t grate_safe = 0;
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
    static constexpr std::uint64_t kHighPlatformEntityId = 16;
    static constexpr std::uint64_t kJibMastEntityId = 17;
    static constexpr std::uint64_t kJibBoomEntityId = 18;
    static constexpr std::uint64_t kJibHookEntityId = 19;
    static constexpr std::uint64_t kJibCrateEntityId = 20;
    static constexpr std::uint64_t kNeedleEntityId = 21;
    static constexpr std::uint64_t kNeedleWestPocketEntityId = 22;
    static constexpr std::uint64_t kNeedleEastPocketEntityId = 23;
    static constexpr std::uint64_t kNeedleNearLandingEntityId = 24;
    static constexpr std::uint64_t kNeedleFarLandingEntityId = 25;
    static constexpr std::uint64_t kNeedleBayFloorEntityId = 26;
    static constexpr std::uint64_t kCageEntityId = 27;
    static constexpr std::uint64_t kCageUpperLandingEntityId = 28;
    static constexpr std::uint64_t kSumpGrateEntityId = 29;
    static constexpr std::uint64_t kNeedleStairEntityIdBegin = 30;
    static constexpr std::uint32_t kNeedleWestStairCount = 15;
    static constexpr std::uint32_t kNeedleEastStairCount = 9;
    static constexpr std::uint64_t kSumpFloorEntityId = 54;
    static constexpr std::uint64_t kSumpFarLandingEntityId = 55;
    static constexpr std::uint64_t kScrewEntityId = 56;
    static constexpr std::uint64_t kRefugeEntityId = 57;
    static constexpr std::uint64_t kSumpSkinEntityId = 58;

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
    [[nodiscard]] bool request_parachute() noexcept;
    [[nodiscard]] bool commit_checkpoint() noexcept;
    [[nodiscard]] bool can_enter_jib_station() const noexcept;
    [[nodiscard]] bool request_enter_jib_station() noexcept;
    [[nodiscard]] bool request_exit_jib_station() noexcept;
    [[nodiscard]] bool set_jib_hoist_input(double hoist) noexcept;
    [[nodiscard]] bool set_jib_slew_input(double slew) noexcept;
    [[nodiscard]] bool set_jib_brake(bool engaged) noexcept;
    [[nodiscard]] bool can_operate_cage() const noexcept;
    [[nodiscard]] bool request_cage_lever() noexcept;
    [[nodiscard]] bool can_operate_sump_valve() const noexcept;
    [[nodiscard]] bool request_sump_valve() noexcept;
    [[nodiscard]] bool can_operate_sump_drain() const noexcept;
    [[nodiscard]] bool request_sump_drain() noexcept;
    [[nodiscard]] bool can_operate_screw() const noexcept;
    [[nodiscard]] bool request_screw_wheel() noexcept;
    [[nodiscard]] bool export_checkpoint(CheckpointRecord &record) const noexcept;
    [[nodiscard]] bool import_checkpoint(const CheckpointRecord &record) noexcept;
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
    bool parachute_requested_ = false;
    bool jib_enter_requested_ = false;
    bool jib_exit_requested_ = false;
    double jib_hoist_input_ = 0.0;
    double jib_slew_input_ = 0.0;
    bool jib_brake_engaged_ = true;
    bool jib_brake_command_valid_ = false;
    bool cage_lever_requested_ = false;
    bool sump_valve_requested_ = false;
    bool sump_drain_requested_ = false;
    bool screw_wheel_requested_ = false;
    Snapshot snapshot_{};
};

} // namespace scraperx::sim
