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

struct Quaternion final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;
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
    // AS-001 falsifier spawn: on the MOD-INTAKE-BELT catwalk at the
    // MOD-YARD-JIB pendant, where CAP-PENDANT actually lives (Atlas B00). The
    // whole freight sequence is driven from here without a climb.
    IntakePendant = 18,
    // AS-001 falsifier spawn: on the apron directly outside the MOD-DOG-A
    // throat, facing the bay. One spawn serves both "MOD-STAIR-A is physically
    // impassable while the pack pins the dog" (walk forward immediately) and
    // "it is passable once the dog has travelled" -- matching the needle and
    // sump station pattern.
    IntakeThroat = 19,
    // AS-001 falsifier spawn: at the foot of MOD-SKIN-LADDER-S. The SKIN braid
    // is a legal bypass of the whole freight sequence (Atlas B00 coupling 3),
    // so it is proven from its own spawn with the pack untouched.
    IntakeSkinFoot = 20,
    // AS-002 falsifier spawn: standing on the +24.1872 m handoff deck itself,
    // facing MOD-STAIR-A-SWING's stowed footprint. Isolates "the unloaded
    // flight is not a route" (walk at it immediately) from the separately
    // proven jib/cradle mechanics, matching the throat/sump station pattern.
    IntakeHandoffDeck = 21,
    // AS-003 falsifier spawn: inside MOD-HOOK5-RACK's cage, on the floor under
    // the roof hatch -- where the drop from the roof lands. Isolates "the bar
    // is a body, and moving it travels the door" and "the block is a body"
    // from the separately proven way in over the roof.
    Hook5Cage = 22,
    // AS-003 falsifier spawn: on the apron at grade, south-east of the cage and
    // clear of the belt's corridor. Isolates "grade and the apron's own raised
    // surfaces cannot reach the cage" from the belt that can.
    Hook5Apron = 23,
    // AS-006 falsifier spawn: on the stair's 154 m top deck, north band, just
    // north of Stage A's cage and facing it. The stair itself is proven by
    // the world-solids group; the band's tests start where it ends.
    StairTop = 24,
    // AS-006 Stage B falsifier spawn: standing in B's cage on its bottom stop
    // at 176.25, as a rider who has stepped across from A's parked cage.
    WellBCage = 25,
    // AS-006 Stage C falsifier spawn: standing on C's platform on its bottom
    // stop at 198.25, as a rider who has stepped across from B's parked cage.
    WellCPlatform = 26,
    // Step 2 movement spawn: on the 176 ring east of the climbing route's
    // boards, facing north with the ring's inner edge 1 m behind.
    Ring176East = 27,
    // And on the 198 ring, north of the route's catwalk.
    Ring198East = 28,
    // AS-007: on the 220 ring west of D's walkway, as a rider off C's
    // platform; in E's cab on its bottom stop; on F's platform.
    Ring220North = 29,
    WetECab = 30,
    WetFPlatform = 31,
    // AS-008: on TP-340 north of AS-007 F's hole; on the 374 ring's west
    // band by H's gangway; in I's cage at the 418 ring.
    PlateTop = 32,
    Ring374West = 33,
    ShopICage = 34,
    Ring484North = 35,
    CraneKCage = 36,
    CraneLCab = 37,
    // AS-010: on TP-640, north of the service cage.
    Plate640 = 38,
    // Where the crane ride steps off onto that same floor, south-east of the cage.
    Plate640Dismount = 39,
};

// One box of a mechanism-kit body, in the body's frame: what the presentation
// draws and what collides are the same list.
struct KitPart final {
    Vector3 half{};
    Vector3 offset{};
    Quaternion rotation{};
    std::uint8_t material = 0;
};

// A kit bin (the declared granular model, mechanism_kit.hpp): the kit body it
// is, the rubble in it, and while it pours, the stream from its mouth down to
// where the stream lands. Its rubble lies on its floor, the body's first part.
struct KitBin final {
    std::uint32_t body = 0;
    double contents_kg = 0.0;
    double capacity_kg = 0.0;
    bool flowing = false;
    bool water = false;
    Vector3 stream_from{};
    Vector3 stream_to{};
};

// A pool of the declared water model (AS-007): its box and its level.
struct KitPool final {
    Vector3 min_corner{};
    Vector3 max_corner{};
    double level_m = 0.0;
    double water_kg = 0.0;
};

// A spout pouring this step, from the spout to where it lands.
struct KitSpout final {
    bool pouring = false;
    Vector3 from{};
    Vector3 to{};
};

// AS-007's machine state, for the falsifiers.
struct WetState final {
    bool d_pipe_whole = false;
    double d_fill_kg_s = 0.0;
    double d_tank_kg = 0.0;
    double d_tube_kg = 0.0;
    double d_tank_level = 0.0;
    double d_tube_level = 0.0;
    double d_platform_travel = 0.0;
    double d_valve_angle = 0.0;
    double d_drain_angle = 0.0;
    double dump_angle = 0.0;
    bool e_door_latched = false;
    double e_door_angle = 0.0;
    bool e_catch_latched = false;
    double e_duct_pa = 0.0;
    double e_cab_pa = 0.0;
    double e_cab_travel = 0.0;
    double e_chiller_travel = 0.0;
    double e_bucket_kg = 0.0;
    bool f_catch_latched = false;
    bool f_hose_coupled = false;
    double f_platform_travel = 0.0;
    double f_accumulator_travel = 0.0;
    double header_kg = 0.0;
    double drained_kg = 0.0;
};

// AS-008's machine state, for the falsifiers.
struct ShopState final {
    bool g_rope_on_eye = false;
    bool g_tower_latched = false;
    double g_platform_travel = 0.0;
    double g_tower_travel = 0.0;
    bool h_girder_latched = false;
    bool h_trolley_latched = false;
    double h_girder_angle = 0.0;
    double h_platform_travel = 0.0;
    bool i_rope_on_eye = false;
    bool i_domino_latched = false;
    bool i_monolith_latched = false;
    double i_domino_angle = 0.0;
    double i_trip_angle = 0.0;
    double i_monolith_angle = 0.0;
    double i_cage_travel = 0.0;
};

// AS-009's machine state, for the falsifiers.
struct CraneState final {
    bool j_rail_whole = false;
    bool j_wagon_latched = false;
    double j_traveler_travel = 0.0;
    double j_wagon_travel = 0.0;
    bool k_rope_on_eye = false;
    bool k_jib_latched = false;
    double k_jib_angle = 0.0;
    double k_cage_travel = 0.0;
    bool l_clutch_in = false;
    bool l_weight_latched = false;
    bool l_cart_latched = false;
    double l_cart_travel = 0.0;
    double l_cab_travel = 0.0;
};

// AS-010's first lift: the service cage off TP-640.
struct ServiceState final {
    bool m_catch_latched = true;
    double m_cage_travel = 0.0;
    double m_skip_travel = 0.0;
    std::uint64_t m_rope_end_entity_id = 0;
};

// Rubble spilled onto a static surface, piled where it landed.
struct KitPile final {
    Vector3 at{};
    double kg = 0.0;
};

enum class TraversalState : std::uint8_t {
    None = 0,
    Hanging = 1,
    Mantling = 2,
    Vaulting = 3,
    // Step 2 (MECHANISM_ASCENT_PLAN.md §8): on the holds of a ladder, pipe,
    // bar or lattice; and lowering over an edge into a hang.
    Climbing = 4,
    Lowering = 5,
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
    // Crouched: the short capsule (see Simulation::set_crouch_input).
    bool player_crouched = false;
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
    // Step 2 movement. The points a traversal's hands are on (a climb's two
    // holds; a hang's lip either side of the body) and the horizontal
    // direction it faces the structure; zero outside a traversal.
    Vector3 traversal_left_hand{};
    Vector3 traversal_right_hand{};
    Vector3 traversal_normal{};
    bool player_sprinting = false;
    // Walking a support narrower than 0.5 m and at least 1.5 m long.
    bool player_balancing = false;
    // A hold in reach at hand height, faced from the ground: Action climbs it.
    bool grip_available = false;
    std::uint64_t grip_entity_id = 0;
    Vector3 grip_point{};
    // An edge behind the body with a drop beyond it: Drop lowers into a hang.
    bool edge_drop_available = false;
    Vector3 edge_drop_point{};
    std::uint64_t climb_count = 0;
    // Static dressing loaded from world_solids.inc: bodies built, drawn
    // mirrors of owned bodies skipped, and hulls Jolt refused (must be 0).
    // Edges under kStepMaximumHeight walked up (see try_step_up).
    std::uint64_t step_up_count = 0;
    // Vaults begun by a second Jump inside the window after a takeoff.
    std::uint64_t jump_vault_count = 0;
    std::uint32_t world_solid_bodies = 0;
    std::uint32_t world_solid_mirrors = 0;
    std::uint32_t world_solid_rejected = 0;
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

    // --- AS-001 B00 intake rise (Ascent Atlas §6 band B00, §7 chain K0).
    // MOD-YARD-JIB lifts the 4 t pack off MOD-DOG-A; the dog is a real hinged
    // body under a permanent, finite opening torque, so it travels the instant
    // the pack stops physically blocking its swing. Nothing here is a flag:
    // intake_pack_pins_dog is derived from the pack's measured pose purely for
    // the HUD and the falsifiers, and no simulation branch reads it.
    bool intake_station_active = false;
    double intake_boom_angle_radians = 0.0;
    Vector3 intake_hook_position{};
    Vector3 intake_pack_position{};
    Vector3 intake_overweight_pack_position{};
    double intake_dog_angle_radians = 0.0;
    bool intake_pack_pins_dog = false;
    bool intake_throat_clear = false;

    // --- AS-002 Legal Forty (Ascent Atlas §6 band B00 leftover, §7 chain K0
    // PLAY). MOD-STAIR-A-SWING is a passive, gravity-restored hinge; only
    // MOD-CW-CRADLE's rope tension -- real solver tension, not a flag -- can
    // swing it to its deployed stop. legal_forty_pack_slung is the hook-pack
    // PointConstraint's actual presence, never a derived guess.
    bool legal_forty_pack_slung = true;
    double legal_forty_swing_travel_radians = 0.0;   // 0 stowed .. ~0.9076 deployed
    Vector3 legal_forty_swing_flight_position{};
    Vector3 legal_forty_cradle_position{};

    // AS-003 MOD-HOOK5-RACK. carrying_entity_id is the body on the player's
    // carry point (0 for none), read from the carry constraint's actual
    // presence; carry_target_entity_id is what a pick-up would take this
    // tick. hook_in_rack is derived from the block's pose, never stored, and
    // CAP-HOOK5 is its negation.
    std::uint64_t carrying_entity_id = 0;
    std::uint64_t carry_target_entity_id = 0;
    bool hook_in_rack = true;
    double hook5_door_angle_radians = 0.0;   // 0 shut .. inward stop
    Vector3 hook5_bar_position{};
    Quaternion hook5_bar_rotation{};
    Vector3 hook5_block_position{};
    Quaternion hook5_block_rotation{};

    // AS-006 and the mechanism kit. rig_action is what request_rig would do
    // now: 0 nothing, 1 hook the carried shackle onto rig_target_entity_id,
    // 2 take a slack hooked end off rig_target_entity_id into the hands.
    // carry_target_kind names what a pick-up would take: 0 a load, 1 a
    // rope's shackle, 2 a handle.
    std::uint8_t rig_action = 0;
    std::uint64_t rig_target_entity_id = 0;
    std::uint8_t carry_target_kind = 0;
    // Stage A, the skip lift, read back from its bodies and constraints.
    double well_a_cage_travel = 0.0;
    double well_a_skip_travel = 0.0;
    double well_a_cage_peak_speed = 0.0;
    bool well_a_catch_latched = true;
    std::uint64_t well_a_rope_end_entity_id = 0;
    double well_a_rope_tension_n = 0.0;
    double well_a_lever_angle = 0.0;
    // Stage B, the derrick boom. The boom's angle is 0 level and pi/2 hanging.
    double well_b_cage_travel = 0.0;
    double well_b_cage_peak_speed = 0.0;
    double well_b_boom_angle = 0.0;
    bool well_b_catch_latched = true;
    std::uint64_t well_b_rope_end_entity_id = 0;
    bool well_b_rope_let_go = false;
    double well_b_rope_tension_n = 0.0;
    // Stage C and the cascade. Rubble in kg (the declared granular model).
    double well_c_platform_travel = 0.0;
    double well_c_platform_peak_speed = 0.0;
    double well_c_dumpster_travel = 0.0;
    bool well_c_catch_latched = true;
    double well_c_hopper_kg = 0.0;
    double well_c_dumpster_kg = 0.0;
    double well_a_cage_rubble_kg = 0.0;
    double well_rubble_spilled_kg = 0.0;
    double well_c_rebar_angle = 0.0;
    double well_c_latch_angle = 0.0;
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
    // WO-008 landing consequence: a genuine landing faster than this restores
    // the checkpoint. Public so presentation can show the real bound (the
    // fall-danger gauge) instead of mirroring a gameplay rule it cannot own.
    static constexpr double kLethalImpactSpeedMps = 20.0;
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

    // AS-001 campaign entities (Ascent Atlas §6, band B00 "Apron and Intake").
    // These are MOD-* campaign modules in the real tower yard, not KX-*
    // kernel fixtures: the kernel at x ~ 200 stays untouched regression
    // substrate and is never retitled into campaign geometry.
    static constexpr std::uint64_t kIntakeApronEntityId = 35;
    static constexpr std::uint64_t kIntakeBeltEntityId = 36;
    static constexpr std::uint64_t kIntakeJibMastEntityId = 37;
    static constexpr std::uint64_t kIntakeJibBoomEntityId = 38;
    static constexpr std::uint64_t kIntakeJibHookEntityId = 39;
    static constexpr std::uint64_t kIntakePackEntityId = 40;
    static constexpr std::uint64_t kIntakeOverweightPackEntityId = 41;
    static constexpr std::uint64_t kIntakeDogEntityId = 42;
    static constexpr std::uint64_t kIntakeBayEntityId = 43;
    static constexpr std::uint64_t kIntakeStairEntityId = 44;
    static constexpr std::uint64_t kIntakeHandoffEntityId = 45;
    static constexpr std::uint64_t kIntakeSkinEntityId = 46;

    // AS-002 campaign entities (Ascent Atlas §6, band B00's 24-40 m leftover).
    // MOD-STAIR-A's static upper flight, mid-landing and the SKIN continuation
    // reuse kIntakeStairEntityId / kIntakeSkinEntityId above -- same modules,
    // continued -- since only the dynamic swing flight and the cradle need
    // their own identity.
    static constexpr std::uint64_t kIntakeSwingFlightEntityId = 47;
    static constexpr std::uint64_t kIntakeCwCradleEntityId = 48;
    static constexpr std::uint64_t kIntakeHallDeckEntityId = 49;
    // The swing hinge's own static anchor body (add_hinge's first body) --
    // NOT grouped under kIntakeStairEntityId like the rest of the static
    // structure above, because it alone needs to be excluded from contact
    // with kIntakeSwingFlightEntityId (simulation.cpp's OnContactValidate):
    // the flight's own cross-section is coincident with this anchor at
    // every sweep angle by construction (it sits AT the hinge pivot), so
    // ordinary rigid-body contact between them is never meaningful -- the
    // hinge constraint alone is what should relate their motion.
    static constexpr std::uint64_t kIntakeSwingAnchorEntityId = 50;
    // Every static body drawn by the presentation's builders that the native
    // world did not already own: frame dressing, footings, halls, rails,
    // trees. Generated into world_solids.inc; see build_world_solids().
    static constexpr std::uint64_t kWorldSolidEntityId = 51;
    // Crouch fixture beside the WO-003 traversal fixtures: a beam and its two
    // posts, the beam's underside 1.45 m over the deck.
    static constexpr std::uint64_t kCrawlBeamEntityId = 52;
    // AS-003 MOD-HOOK5-RACK: the cage's static members (buttress, walls, roof,
    // bar brackets, rack), its inward-swinging door, the bar holding the door
    // shut, and the hook block itself (CAP-HOOK5).
    static constexpr std::uint64_t kHook5CageEntityId = 53;
    static constexpr std::uint64_t kHook5DoorEntityId = 54;
    static constexpr std::uint64_t kHook5BarEntityId = 55;
    static constexpr std::uint64_t kHook5BlockEntityId = 56;

    // AS-006, the Counterweight Well. Mechanism-kit ids: band structure from
    // 1000, moving bodies from 2000 (sim/mechanism_kit.hpp).
    static constexpr std::uint64_t kWellAFrameEntityId = 1000;
    static constexpr std::uint64_t kWellACageEntityId = 2000;
    static constexpr std::uint64_t kWellASkipEntityId = 2001;
    static constexpr std::uint64_t kWellAShackleEntityId = 2002;
    static constexpr std::uint64_t kWellALeverEntityId = 2003;
    static constexpr std::uint64_t kWellAHandleEntityId = 2004;
    static constexpr std::uint64_t kWellBFrameEntityId = 1001;
    static constexpr std::uint64_t kWellBCageEntityId = 2010;
    static constexpr std::uint64_t kWellBBoomEntityId = 2011;
    static constexpr std::uint64_t kWellBShackleEntityId = 2012;
    static constexpr std::uint64_t kWellBLeverEntityId = 2013;
    static constexpr std::uint64_t kWellBHandleEntityId = 2014;
    static constexpr std::uint64_t kWellBStrikerEntityId = 2015;
    static constexpr std::uint64_t kWellCFrameEntityId = 1002;
    static constexpr std::uint64_t kWellCHopperEntityId = 1003;
    static constexpr std::uint64_t kWellCPlatformEntityId = 2020;
    static constexpr std::uint64_t kWellCDumpsterEntityId = 2021;
    static constexpr std::uint64_t kWellCLatchEntityId = 2022;
    static constexpr std::uint64_t kWellCRebarEntityId = 2023;
    static constexpr std::uint64_t kWellCHandleEntityId = 2024;
    static constexpr std::uint64_t kWellCStrikerEntityId = 2025;
    static constexpr std::uint64_t kWellATipOutEntityId = 2005;
    static constexpr std::uint64_t kWellATipHandleEntityId = 2006;
    static constexpr std::uint64_t kWellCLatchHandleEntityId = 2026;
    // AS-006's climbing route: ladder, boards, standpipe, catwalk, scaffold.
    static constexpr std::uint64_t kWellRouteEntityId = 1004;
    // AS-007 Wet Isolation.
    static constexpr std::uint64_t kWetFrameEntityId = 1005;
    static constexpr std::uint64_t kWetRouteEntityId = 1006;
    static constexpr std::uint64_t kWetDPlatformEntityId = 2030;
    static constexpr std::uint64_t kWetDSpoolEntityId = 2031;
    static constexpr std::uint64_t kWetDValveEntityId = 2032;
    static constexpr std::uint64_t kWetDValveHandleEntityId = 2033;
    static constexpr std::uint64_t kWetDDrainEntityId = 2034;
    static constexpr std::uint64_t kWetDDrainHandleEntityId = 2035;
    static constexpr std::uint64_t kWetECabEntityId = 2040;
    static constexpr std::uint64_t kWetEChillerEntityId = 2041;
    static constexpr std::uint64_t kWetEDoorEntityId = 2042;
    static constexpr std::uint64_t kWetELatchEntityId = 2043;
    static constexpr std::uint64_t kWetELeverEntityId = 2044;
    static constexpr std::uint64_t kWetEHandleEntityId = 2045;
    static constexpr std::uint64_t kWetEBucketEntityId = 2046;
    static constexpr std::uint64_t kWetEStrikerEntityId = 2047;
    static constexpr std::uint64_t kWetFPlatformEntityId = 2050;
    static constexpr std::uint64_t kWetFAccumulatorEntityId = 2051;
    static constexpr std::uint64_t kWetFHoseEntityId = 2052;
    static constexpr std::uint64_t kWetFLeverEntityId = 2053;
    static constexpr std::uint64_t kWetFHandleEntityId = 2054;
    static constexpr std::uint64_t kWetHeaderDumpEntityId = 2060;
    static constexpr std::uint64_t kWetHeaderHandleEntityId = 2061;
    // AS-008 Plate Shop.
    static constexpr std::uint64_t kShopFrameEntityId = 1007;
    static constexpr std::uint64_t kShopRouteEntityId = 1008;
    static constexpr std::uint64_t kUpperFrameEntityId = 1009;
    static constexpr std::uint64_t kShopGPlatformEntityId = 2070;
    static constexpr std::uint64_t kShopGTowerEntityId = 2071;
    static constexpr std::uint64_t kShopGPinEntityId = 2072;
    static constexpr std::uint64_t kShopGHandleEntityId = 2073;
    static constexpr std::uint64_t kShopGShackleEntityId = 2074;
    static constexpr std::uint64_t kShopHPlatformEntityId = 2080;
    static constexpr std::uint64_t kShopHGirderEntityId = 2081;
    static constexpr std::uint64_t kShopHTrolleyEntityId = 2082;
    static constexpr std::uint64_t kShopHGirderPinEntityId = 2084;
    static constexpr std::uint64_t kShopHChockEntityId = 2085;
    static constexpr std::uint64_t kShopHHandleEntityId = 2086;
    static constexpr std::uint64_t kShopICageEntityId = 2090;
    static constexpr std::uint64_t kShopIMonolithEntityId = 2091;
    static constexpr std::uint64_t kShopIDominoEntityId = 2092;
    static constexpr std::uint64_t kShopIDominoPinEntityId = 2093;
    static constexpr std::uint64_t kShopITripEntityId = 2094;
    static constexpr std::uint64_t kShopIHandleEntityId = 2095;
    static constexpr std::uint64_t kShopIShackleEntityId = 2096;
    // AS-009, the Facade Crane Stack (Atlas B05): frame and route statics,
    // then stages J, K, L.
    static constexpr std::uint64_t kCraneFrameEntityId = 1010;
    static constexpr std::uint64_t kCraneRouteEntityId = 1011;
    static constexpr std::uint64_t kCraneJTravelerEntityId = 2100;
    static constexpr std::uint64_t kCraneJWagonEntityId = 2101;
    static constexpr std::uint64_t kCraneJJointEntityId = 2102;
    static constexpr std::uint64_t kCraneJChockEntityId = 2103;
    static constexpr std::uint64_t kCraneJHandleEntityId = 2104;
    static constexpr std::uint64_t kCraneKCageEntityId = 2110;
    static constexpr std::uint64_t kCraneKJibEntityId = 2111;
    static constexpr std::uint64_t kCraneKPinEntityId = 2112;
    static constexpr std::uint64_t kCraneKHandleEntityId = 2113;
    static constexpr std::uint64_t kCraneKShackleEntityId = 2114;
    static constexpr std::uint64_t kCraneLCabEntityId = 2120;
    static constexpr std::uint64_t kCraneLCartEntityId = 2121;
    static constexpr std::uint64_t kCraneLCatchEntityId = 2122;
    static constexpr std::uint64_t kCraneLWeightEntityId = 2123;
    static constexpr std::uint64_t kCraneLPinEntityId = 2124;
    static constexpr std::uint64_t kCraneLPinHandleEntityId = 2125;
    static constexpr std::uint64_t kCraneLClutchEntityId = 2126;
    static constexpr std::uint64_t kCraneLClutchHandleEntityId = 2127;
    // AS-010, the service cage off TP-640.
    static constexpr std::uint64_t kServiceFrameEntityId = 1012;
    static constexpr std::uint64_t kServiceRouteEntityId = 1013;
    static constexpr std::uint64_t kServiceMCageEntityId = 2130;
    static constexpr std::uint64_t kServiceMSkipEntityId = 2131;
    static constexpr std::uint64_t kServiceMShackleEntityId = 2132;
    static constexpr std::uint64_t kServiceMLeverEntityId = 2133;
    static constexpr std::uint64_t kServiceMHandleEntityId = 2134;

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
    // Let go: of a hung ledge or a climb's holds; on the ground, with an edge
    // behind the body and a drop beyond it, lower over it into a hang.
    [[nodiscard]] bool request_release() noexcept;

    // Crouch (GDD 7.2, Governing Law 4). A held state, like set_move_input,
    // not a one-shot: while true and the body is on the ground outside a
    // traversal, it takes a 1.2 m capsule with its feet where they were;
    // while false it stands again, but only where the full 1.8 m capsule is
    // clear. So it can be crouched into a low gap and cannot stand up in one.
    // A Jump or traversal request stands the body first, and is refused
    // where it cannot stand.
    [[nodiscard]] bool set_crouch_input(bool held) noexcept;
    // Sprint, held: the native sprints only standing, hands free, off a
    // balance beam, with the stick at least 0.7 and within 45 degrees of the
    // facing.
    [[nodiscard]] bool set_sprint_input(bool held) noexcept;

    // AS-003 carry commands. One-shot, like request_valve_toggle. A pick-up
    // takes the carryable the snapshot names in carry_target_entity_id -- a
    // real body within reach, at rest, in front -- onto a point constraint at
    // the hands; while held, no vault, mantle or hang begins (both hands are
    // on it). Setting down removes the constraint and nothing else: the body
    // falls, rests, and can be picked up again where it lies.
    [[nodiscard]] bool request_pick_up() noexcept;
    [[nodiscard]] bool request_set_down() noexcept;

    // AS-006 rigging. One-shot, contextual, as snapshot.rig_action says:
    // with a rope's shackle in the hands, hooks it onto the anchor in reach
    // (the rope keeps its length, so a hook the rope cannot reach is
    // refused); with empty hands, takes a slack hooked end off its anchor.
    [[nodiscard]] bool request_rig() noexcept;

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

    // AS-001 MOD-YARD-JIB pendant (CAP-PENDANT). Same continuous, persistent,
    // signed-axis contract as the kernel jib, gated on the B00 pendant station
    // rather than the kernel one. The dog has no command of its own: it is
    // always under opening torque and is held only by the pack's mass.
    [[nodiscard]] bool set_intake_slew_input(double value) noexcept;
    [[nodiscard]] bool set_intake_hoist_input(double value) noexcept;

    // AS-002 sling commands. One-shot, like request_valve_toggle -- not a
    // held axis -- gated identically on the shared B00 pendant station, and
    // further gated on the pack/hook actually being in physical tolerance
    // (see update_legal_forty): a command outside tolerance is accepted but
    // has no effect, exactly like every other pendant command away from its
    // station.
    [[nodiscard]] bool request_intake_sling_release() noexcept;
    [[nodiscard]] bool request_intake_sling_attach() noexcept;

    // Disables the boiler feed so the plant becomes a strictly finite reservoir.
    // Used to prove the machine cannot manufacture work.
    void set_boiler_feed_enabled(bool enabled) noexcept;
    [[nodiscard]] AdvanceResult advance_frame(double frame_delta_seconds) noexcept;
    [[nodiscard]] Snapshot snapshot() const noexcept;

    // Mechanism-kit read back, for the presentation and the falsifiers. Body
    // indices run 0 .. kit_body_count() - 1 in build order.
    static constexpr std::uint32_t kKitNone = 0xFFFFFFFFU;
    [[nodiscard]] std::uint32_t kit_body_count() const noexcept;
    [[nodiscard]] std::uint64_t kit_body_entity(std::uint32_t body) const noexcept;
    [[nodiscard]] bool kit_body_dynamic(std::uint32_t body) const noexcept;
    [[nodiscard]] bool kit_body_enabled(std::uint32_t body) const noexcept;
    [[nodiscard]] std::uint32_t kit_body_part_count(std::uint32_t body) const noexcept;
    [[nodiscard]] KitPart kit_body_part(std::uint32_t body, std::uint32_t part) const noexcept;
    [[nodiscard]] Vector3 kit_body_position(std::uint32_t body) const noexcept;
    [[nodiscard]] Vector3 kit_body_center_of_mass(std::uint32_t body) const noexcept;
    [[nodiscard]] Quaternion kit_body_rotation(std::uint32_t body) const noexcept;
    [[nodiscard]] Vector3 kit_body_velocity(std::uint32_t body) const noexcept;
    [[nodiscard]] double kit_body_mass(std::uint32_t body) const noexcept;
    [[nodiscard]] std::uint32_t kit_body_index(std::uint64_t entity) const noexcept;
    // Cables are the kit's ropes, then its trip lines.
    [[nodiscard]] std::uint32_t kit_cable_count() const noexcept;
    // Writes up to capacity points of the cable as drawn (first end, sheaves,
    // other end) and returns how many; 0 for a parted rope.
    [[nodiscard]] std::uint32_t kit_cable_points(std::uint32_t cable, Vector3 *out,
                                                 std::uint32_t capacity) const noexcept;
    // Indices run 0 .. count - 1; an index past the end reads as empty.
    [[nodiscard]] std::uint32_t kit_bin_count() const noexcept;
    [[nodiscard]] KitBin kit_bin(std::uint32_t bin) const noexcept;
    [[nodiscard]] std::uint32_t kit_pile_count() const noexcept;
    [[nodiscard]] KitPile kit_pile(std::uint32_t pile) const noexcept;
    [[nodiscard]] std::uint32_t kit_pool_count() const noexcept;
    [[nodiscard]] KitPool kit_pool(std::uint32_t pool) const noexcept;
    [[nodiscard]] std::uint32_t kit_spout_count() const noexcept;
    [[nodiscard]] KitSpout kit_spout(std::uint32_t spout) const noexcept;
    [[nodiscard]] WetState wet_state() const noexcept;
    [[nodiscard]] ShopState shop_state() const noexcept;
    [[nodiscard]] CraneState crane_state() const noexcept;
    [[nodiscard]] ServiceState service_state() const noexcept;

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
    bool crouch_input_ = false;
    bool sprint_input_ = false;
    bool pick_up_requested_ = false;
    bool set_down_requested_ = false;
    bool rig_requested_ = false;
    bool parachute_toggle_requested_ = false;
    double jib_slew_input_ = 0.0;
    double jib_hoist_input_ = 0.0;
    double needle_hoist_input_ = 0.0;
    double intake_slew_input_ = 0.0;
    double intake_hoist_input_ = 0.0;
    bool valve_toggle_requested_ = false;
    bool intake_sling_release_requested_ = false;
    bool intake_sling_attach_requested_ = false;
    Snapshot snapshot_{};
};

} // namespace scraperx::sim
