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
    // WO-008 falsifier spawn: high above the static deck with no horizontal
    // offset, so an unmitigated fall is unambiguously lethal and a
    // sufficiently early parachute deploy is unambiguously survivable.
    HighDrop = 11,
    // WO-008 falsifier spawn: a short ~12 m drop, comfortably under
    // kLethalImpactSpeedMps, proving ordinary platforming falls stay
    // survivable (GDD 8.2) and are never mistaken for a lethal one.
    SurvivableDrop = 12,
    // WO-010 falsifier spawn: directly above the catwalk treadle, the plant's
    // human-scale control. Isolates "a body on this pedal works the valve" from
    // the separately-proven question of how the player reaches the catwalk.
    CatwalkTreadle = 13,
    // WO-011 falsifier spawn: at the KX-JIB pendant station, per Ascent Atlas
    // v1.0 kernel (section 9). The atlas kernel is a bounded, separate proof
    // volume -- not the Kellerworks yard -- so this spawns well clear of it.
    KernelJibStation = 14,
    // WO-011 falsifier spawn: directly on the crate's top surface, isolating
    // "the crate is a real moving support" (WO-005 proof path item 3, WO-002
    // law) from the separately-proven pendant/motor mechanics above.
    KernelCrateTop = 15,
    // WO-012 falsifier spawn: at the KX-NEEDLE pendant station, on the
    // approach pier facing the gap. One spawn serves both "the unseated gap
    // cannot be crossed" (walk forward immediately) and "seating it with the
    // jib makes it walkable" (operate the hoist first) -- the station sits
    // on the pier specifically so both are reachable without a climb move
    // this kernel slice has no mechanism for.
    KernelNeedleStation = 16,
    // WO-013 falsifier spawn: at the KX-SUMP valve station, on the fixed
    // approach decking facing the grate (not on the grate itself, which
    // starts wet and unsafe). One spawn serves both "wet grate cannot be
    // crossed" (walk forward immediately) and "isolate and drain makes it
    // ordinary support" (close the valve first, then walk) -- matching the
    // pattern established for the needle station.
    KernelSumpStation = 17,
};

enum class TraversalState : std::uint8_t {
    None = 0,
    Hanging = 1,
    Mantling = 2,
    Vaulting = 3,
};

// WO-008. Parachuting is a sub-state of Airborne, not a fourth motion primitive
// -- the player is still under normal air control, with drag layered on top.
enum class FallState : std::uint8_t {
    Grounded = 0,
    Airborne = 1,
    Parachuting = 2,
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

    // --- WO-008 fall / parachute / checkpoint -----------------------------
    FallState fall_state = FallState::Grounded;
    double fall_peak_speed_mps = 0.0;
    double last_impact_speed_mps = 0.0;
    bool parachute_deployed = false;
    Vector3 checkpoint_position{};
    std::uint64_t checkpoint_commit_count = 0;
    std::uint64_t death_count = 0;

    // --- coupled machine: every field below is read back from the authoritative
    // Jolt bodies or from the reduced-order steam plant, never authored.
    Vector3 hoist_scoop_position{};
    double hoist_scoop_tilt_radians = 0.0;
    Vector3 ballast_position{};
    Vector3 ballast_linear_velocity{};
    Vector3 tipper_position{};
    double tipper_angle_radians = 0.0;
    double valve_lever_angle_radians = 0.0;
    double treadle_angle_radians = 0.0;
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

    // --- WO-011 KX-JIB / KX-CRATE (Ascent Atlas v1.0 kernel, atlas-authority
    // §9). A pendant-controlled crane, not an autonomous cycle: the boom slews
    // and the hook raises/lowers only while the player is at the station and
    // only as fast as a finite, real Jolt constraint motor allows.
    bool jib_station_active = false;
    double jib_boom_angle_radians = 0.0;
    Vector3 jib_hook_position{};
    Vector3 jib_hook_linear_velocity{};
    Vector3 jib_crate_position{};
    Vector3 jib_crate_linear_velocity{};
    // A separate, fixed capacity-proving stand: the same rated winch force as
    // the jib's hoist, permanently loaded past that rating, so "the winch
    // force is finite" is falsifiable without staging an unsafe lift on the
    // real jib.
    Vector3 jib_capacity_stand_load_position{};

    // --- WO-012 KX-NEEDLE / KX-POCKETS (Ascent Atlas v1.0 kernel, §9). A
    // needle beam lowered by its own finite-force hoist; once its pose is
    // within seat tolerance and settled, it is pinned into both pockets and
    // becomes real, walkable structural support. Unseated, the gap has none.
    bool needle_station_active = false;
    bool needle_seated = false;
    Vector3 needle_position{};
    Vector3 needle_linear_velocity{};

    // --- WO-013 KX-SUMP / KX-GRATE (Ascent Atlas v1.0 kernel, §9). A lumped
    // process volume: one isolation edge (the valve), one drain sink, one
    // derived "grate safe" predicate. The grate's own collidability is what
    // changes -- not a decal, not a flag the traversal system trusts blindly.
    bool sump_station_active = false;
    bool sump_isolated = false;
    double sump_volume_kg = 0.0;
    bool grate_safe = false;
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
    // WO-010. The plant's human-scale control: a foot treadle on the catwalk,
    // cabled across the yard to the valve gear. The player cannot move machine-
    // scale mass with their body, so this is how a body enters the machine.
    static constexpr std::uint64_t kTreadleEntityId = 24;
    // WO-011 Ascent Atlas kernel entities (§9): KX-JIB is the mast+boom+hook,
    // KX-CRATE is the load, plus a fixed, always-overweight capacity-proving
    // stand that shares the jib's rated winch force.
    static constexpr std::uint64_t kJibMastEntityId = 25;
    static constexpr std::uint64_t kJibBoomEntityId = 26;
    static constexpr std::uint64_t kJibHookEntityId = 27;
    static constexpr std::uint64_t kCrateEntityId = 28;
    static constexpr std::uint64_t kCapacityStandEntityId = 29;
    // WO-012 Ascent Atlas kernel entities (§9): KX-NEEDLE is the seatable
    // beam; KX-POCKETS is represented by the two piers it seats into. The
    // hoist mast is proof scaffolding (a second, minimal jib-pattern
    // mechanism), like WO-011's capacity stand -- not an atlas-named module.
    static constexpr std::uint64_t kNeedlePierApproachEntityId = 30;
    static constexpr std::uint64_t kNeedlePierFarEntityId = 31;
    static constexpr std::uint64_t kNeedleHoistMastEntityId = 32;
    static constexpr std::uint64_t kNeedleBeamEntityId = 33;
    // WO-013 Ascent Atlas kernel entity (§9): KX-GRATE is the walkway whose
    // collidability the process network derives. Falling through it lands on
    // the existing world deck below -- a real, measured drop, not a new floor
    // body invented just to catch it.
    static constexpr std::uint64_t kSumpGrateEntityId = 34;

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

    // Toggles the always-carried parachute. Only takes effect while airborne
    // (GDD 8.3); queued and resolved on the authoritative tick like every
    // other command, so a press while grounded is accepted as a command but
    // produces no state change.
    [[nodiscard]] bool request_parachute() noexcept;

    // WO-011 KX-JIB pendant commands. Continuous, persistent axes -- like
    // set_move_input, not a one-shot event -- matching Drive (slew) and
    // Raise/Lower (hoist); zero on either axis is the brake, not "let go":
    // the winch motor holds against gravity up to its rated force, it does
    // not free-fall the instant input stops. Both axes are clamped to
    // [-1, 1] and take effect only while the player is at the station
    // (jib_station_active); away from the station they are accepted as
    // commands but produce no motion, exactly like a parachute request while
    // grounded.
    [[nodiscard]] bool set_jib_slew_input(double value) noexcept;
    [[nodiscard]] bool set_jib_hoist_input(double value) noexcept;

    // WO-012 KX-NEEDLE pendant command. Same continuous, persistent, signed-
    // axis contract as the jib's hoist: positive raises, negative lowers,
    // zero brakes against gravity up to the rated force. Takes effect only
    // while the player is at the needle station.
    [[nodiscard]] bool set_needle_hoist_input(double value) noexcept;

    // WO-013 KX-SUMP valve command. A one-shot toggle, like request_parachute
    // -- not a continuous axis, since isolation is a real binary state (open
    // feeding the sump, or closed and letting the drain win) -- gated the
    // same way: accepted as a command anywhere, but only takes effect while
    // the player is at the sump station.
    [[nodiscard]] bool request_valve_toggle() noexcept;

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
    bool parachute_toggle_requested_ = false;
    double jib_slew_input_ = 0.0;
    double jib_hoist_input_ = 0.0;
    double needle_hoist_input_ = 0.0;
    bool valve_toggle_requested_ = false;
    Snapshot snapshot_{};
};

} // namespace scraperx::sim
