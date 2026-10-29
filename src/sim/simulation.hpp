#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <memory>

namespace scraperx::sim {
namespace traversal {
class AffordanceMap;
enum class MoveKind : std::uint8_t;
}

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
    // The proving ground (WO-002, WO-003, WO-008): spawns beside the movement
    // fixtures -- moving supports, a vault rail, mantle and hang ledges, a
    // blocked ledge, a crawl beam -- which are built only in a world started
    // at one of these spawns. The game's world has none of them.
    StaticDeck = 0,
    TranslatingSupport = 1,
    RotatingSupport = 2,
    VaultApproach = 3,
    MantleApproach = 4,
    HangApproach = 5,
    MovingLedgeApproach = 6,
    BlockedLedgeApproach = 7,
    // The game's start: at grade, outdoors, south of the tower.
    ExteriorGrade = 8,
    // WO-008 falsifier spawn (proving ground): high above the grade with no
    // horizontal offset, so an unmitigated fall is unambiguously lethal and a
    // sufficiently early parachute deploy is unambiguously survivable.
    HighDrop = 9,
    // WO-008 falsifier spawn (proving ground): a short ~12 m drop, comfortably
    // under kLethalImpactSpeedMps, proving ordinary platforming falls stay
    // survivable (GDD 8.2) and are never mistaken for a lethal one.
    SurvivableDrop = 10,
    // AS-006: on the 154 m deck's north band, just north of Stage A's cage
    // and facing it. The band's tests start here.
    Deck154 = 11,
    // AS-006 Stage B falsifier spawn: standing in B's cage on its bottom stop
    // at 176.25, as a rider who has stepped across from A's parked cage.
    WellBCage = 12,
    // AS-006 Stage C falsifier spawn: standing on C's platform on its bottom
    // stop at 198.25, as a rider who has stepped across from B's parked cage.
    WellCPlatform = 13,
    // Step 2 movement spawn: on the 176 ring east of the climbing route's
    // boards, facing north with the ring's inner edge 1 m behind.
    Ring176East = 14,
    // And on the 198 ring, north of the route's catwalk.
    Ring198East = 15,
    // AS-007: on the 220 ring west of D's walkway, as a rider off C's
    // platform; in E's cab on its bottom stop; on F's platform.
    Ring220North = 16,
    WetECab = 17,
    WetFPlatform = 18,
    // AS-008: on TP-340 north of AS-007 F's hole; on the 374 ring's west
    // band by H's gangway; in I's cage at the 418 ring.
    PlateTop = 19,
    Ring374West = 20,
    ShopICage = 21,
    Ring484North = 22,
    CraneKCage = 23,
    CraneLCab = 24,
    // Band 0, the Stack: on deck 2's south band by S1's gangway, as a rider
    // off S1; on deck 4's south band by C1's davit, as a climber off C1.
    Deck2South = 25,
    Deck4South = 26,
    Deck6South = 27,
    Deck8North = 28,
    Deck12North = 29,
    // AS-010: on TP-640 north of AS-009 L's hole, as a rider off L's cab.
    PlateTop640 = 30,
    // AS-010: on the 662 deck north of C4's cabinet, where a rider off M climbs on.
    Deck662 = 31,
    // AS-010: on the 684 deck east of stage N's cage, where C4's climber
    // steps off its last mantle.
    Deck684 = 32,
    // AS-010: on the 706 deck by where C5's plant floor opens off it.
    Deck706 = 33,
    // AS-010: on the 728 deck north-east of stage O's cab, where C5's
    // climber comes over the girder.
    Deck728 = 34,
    // On the 242 ring's south side, where the swing (AS-012) sets its rider down.
    Ring242South = 35,
    // On the 264 ring's west band, where C6's climber tops out.
    Ring264West = 36,
    // On the 750 deck, where stage O's rider steps off.
    Deck750 = 37,
    // Six meters above the hang ledge, off its face: a catch from here
    // tears the grip (a falsifier for the physical hang).
    HangHighDrop = 38,
    // On the deck a meter short of the first monkey bar, facing along them.
    SwingApproach = 39,
    // On the pipe gallery's start stand in the yard, facing along its pipes.
    PipeGallery = 40,
    // On the deck west of the run wall, beside its line, facing along it.
    WallRunApproach = 41,
    // On the summit deck, off the derrick's upper receiver (task 92).
    Summit = 42,
    // Not a spawn: one past the last, so a spawn added above it is never refused.
    Count,
};

// A world started at one of these spawns is the proving ground: it carries
// the movement fixtures. Every other world is the game's.
[[nodiscard]] constexpr bool is_proving_spawn(const InitialSpawn spawn) noexcept {
    return static_cast<std::uint8_t>(spawn) <= static_cast<std::uint8_t>(InitialSpawn::BlockedLedgeApproach) ||
           spawn == InitialSpawn::HighDrop || spawn == InitialSpawn::SurvivableDrop ||
           spawn == InitialSpawn::HangHighDrop || spawn == InitialSpawn::SwingApproach ||
           spawn == InitialSpawn::WallRunApproach;
}

// One box of a mechanism-kit body, in the body's frame: what the presentation
// draws and what collides are the same list.
struct KitPart final {
    Vector3 half{};
    Vector3 offset{};
    Quaternion rotation{};
    std::uint8_t material = 0;
    // 0 box, 1 cylinder, 2 capsule (along the part's y; half.x the radius).
    std::uint8_t shape = 0;
};

// The ground slingshot (sim/slingshot.hpp) as the presentation reads it:
// what the rider can do there now, how far the pouch is drawn, where the
// bands run, and the aim. World meters, radians; yaw 0 points toward -Z.
struct SlingshotSnapshot final {
    bool station_available = false;  // ENTER POUCH (or, after a shot, RETRIEVE) offered
    bool seated = false;             // harnessed into the pouch
    bool drawing = false;
    bool released = false;
    bool release_ready = false;      // drawn enough to clear the launch rail
    bool can_retrieve = false;
    bool recovering = false;
    bool guided_launch = false;
    bool aim_ready = true;
    bool aim_locked = false;
    bool flight = false;             // the rider in free flight off the bands
    double draw_m = 0.0;
    double max_draw_m = 0.0;
    double energy_j = 0.0;           // in the stretched rubber
    double work_j = 0.0;             // supplied by the draw
    double yaw_rad = 0.0;
    double elevation_rad = 0.0;
    double target_yaw_rad = 0.0;
    double target_elevation_rad = 0.0;
    double band_rest_m = 0.0;
    double leather_deflection_m = 0.0;
    Vector3 anchor_left{};
    Vector3 anchor_right{};
    Vector3 pouch_position{};
    Vector3 neutral_position{};
    Vector3 retrieval_control_position{};
    Vector3 launch_track_start{};
    Vector3 launch_track_end{};
    std::uint32_t launch_count = 0;
};

// The summit (task 92): the searchlight's state, and the striker plate that
// lights it.
struct SummitSnapshot final {
    bool lit = false;                 // the lamp is lit for good
    double striker_travel_m = 0.0;    // the plate's way down from its rest
    double striker_peak_m = 0.0;      // the deepest it has gone
    double trip_travel_m = 0.0;       // where it lights the lamp
    double lamp_yaw_rad = 0.0;
    double lamp_speed_rad_s = 0.0;
};

// The Matron (the owner's chase): her state, where she is, the gauge on her
// locker, and the catwalk's plates.
struct MatronSnapshot final {
    std::uint8_t state = 0;           // Matron::State
    bool alive = true;
    bool released = false;
    bool sees_player = false;
    Vector3 position{};
    double yaw = 0.0;
    double speed = 0.0;
    double stride = 0.0;              // meters walked: the gait's phase
    double strain = 0.0;              // 0..1, the gauge
    double bolts_popped = 0.0;        // of the locker's four
    double noise_heard = 0.0;
    std::uint64_t catches = 0;
    int planks_fallen = 0;
    double plank_load_peak_n = 0.0;
    double target_x = 0.0, target_z = 0.0;
    bool catwalk_allowed = false;
    std::uint8_t turn = 0;            // Matron::Turn: 1 the wind-up, 2 the lunges
    double turn_s = 0.0;
    // Her mind: noises she ran to and found nobody at this run, whether she
    // is checking the present one from a vantage, and what she has vaulted.
    std::uint32_t fooled = 0;
    bool wary = false;
    std::uint32_t vaults = 0;
    // Off her deck: gaps leapt in the Turn, holding on to a floor that moves,
    // and the level she hunts (its floor's top).
    std::uint32_t leaps = 0;
    std::uint32_t unrigs = 0;   // ropes she took off a way out's eye
    bool on_line = false;       // going down a line's bare cable after the player
    std::uint32_t cable_slides = 0;
    bool climbing = false;      // going up a ladder after the player (stride: meters climbed)
    std::uint32_t climbs = 0;
    bool clinging = false;
    double floor_y = 154.0;
    Vector3 climb_hand{};
    Vector3 climb_hand2{};
    bool climb_hand2_known = false;
    Vector3 climb_feet_support{};
    bool climb_feet_support_known = false;
    // Her contacts as the simulation has them (Matron::View contact_*): her
    // feet's floor, her hand's grip, a second grip, a line under her arms.
    // Bodies and members are Jolt ids (0xffffffff: none).
    bool contact_supported = false;
    Vector3 contact_feet{};
    std::uint32_t contact_feet_body = 0xffffffffU;
    std::uint32_t contact_feet_sub = 0xffffffffU;
    bool contact_hand_held = false;
    Vector3 contact_hand{};
    std::uint32_t contact_hand_body = 0xffffffffU;
    std::uint32_t contact_hand_sub = 0xffffffffU;
    bool contact_hand2_held = false;
    Vector3 contact_hand2{};
    std::uint32_t contact_hand2_body = 0xffffffffU;
    bool contact_line = false;
    // On her pipe: the riser's face her feet press against.
    bool contact_braced = false;
    Vector3 contact_brace{};
    std::uint32_t contact_brace_body = 0xffffffffU;
    traversal::MoveKind climb_move{};
    int climb_phase = 0;
    bool climb_move_active = false;
    bool climb_airborne = false;
    bool climb_sliding = false;
    bool climb_leg_driving = false;
    bool climb_bar_traverse = false;
    double climb_force_n = 0.0;
    bool climb_lunge_drive = false;
    double foot_power_w = 0.0;
    std::uint32_t climb_grips = 0, climb_releases = 0;
    bool mantling = false;
    double lead_x = 0.0, lead_z = 0.0, lead_age_s = 0.0;
    bool lead_seen = false, lead_valid = false;
    std::uint32_t acoustic_queries = 0;
    double lead_floor_y = 154.0;   // the level of her lead: its floor's top
    // Her stages (matron.hpp): her rage, 0..1, the stage it has her in (0 a
    // pest, 1 dangerous, 2 fully changed), and her exposure, 0..1.
    double rage = 0.0;
    std::uint8_t stage = 0;
    double exposure = 0.0;
    std::uint32_t shoves = 0;   // the player thrown off their feet by her hands, this run
    std::uint64_t carrying = 0; // the stock in her hands (its entity), 0 for none
    std::uint32_t takes = 0;    // pieces of the player's stock she took, this run
    std::uint32_t throws = 0;   // and threw at the player
    // Her balance (matron.hpp): blows that took it and that took her off her
    // feet this run, the last blow (the change in her speed it made), and
    // down, the way she fell, kneeling where lying did not fit, and how long.
    std::uint32_t staggers = 0;
    std::uint32_t knockdowns = 0;
    double blow_mps = 0.0;
    double fall_x = 0.0, fall_z = 1.0;
    bool kneeling = false;
    double down_s = 0.0;
    double down_left_s = 0.0;   // of the time she lies there (0 and still down: pinned)
    // The shaft's corner pipe (matron.hpp): on it (getting on, on it, getting
    // off), head first, torn off and sliding, slammed onto a flange; the
    // stories she set out along it, her hands' holds (knocks), flanges passed
    // (bongs) and slammed onto, and the last slam's speed.
    bool on_pipe = false;
    bool pipe_head_down = false;
    bool pipe_sliding = false;
    bool pipe_stunned = false;
    bool pipe_listening = false;
    std::uint32_t pipe_links = 0;
    std::uint32_t pipe_holds = 0;
    std::uint32_t pipe_flanges = 0;
    std::uint32_t pipe_slams = 0;
    double pipe_slam_mps = 0.0;
    double pipe_peak_force_n = 0.0;
    double pipe_peak_power_w = 0.0;
    // The bay (bay.hpp): her hands holding its leaf and what they hold up, N;
    // her trap's phase (Matron::Trap: 0 none .. 4 holding, 5 climbing out)
    // and how many times she set it this run.
    bool bracing = false;
    double support_n = 0.0;
    std::uint8_t trap = 0;
    std::uint32_t traps = 0;
    // Her pace (Matron::Pace: 0 creep, 1 stalk, 2 stride, 3 hunt) and its
    // speed; getting unstuck; the player's trail; silence for silence.
    std::uint8_t pace = 0;
    double pace_mps = 0.0;
    std::uint32_t pace_changes = 0;
    std::uint32_t stalls = 0;
    std::uint32_t step_ups = 0;
    std::uint32_t clambers = 0;
    std::uint32_t sidesteps = 0;
    std::uint32_t reroutes = 0;
    double longest_stall_s = 0.0;
    std::uint32_t crumbs = 0;
    std::uint32_t crumbs_walked = 0;
    bool hushed = false;
    std::uint32_t creaks = 0;
    // Her hold (Matron::State 12..17): its edges counted, the voice's hooks.
    std::uint32_t grabs = 0;
    std::uint32_t grab_misses = 0;
    std::uint32_t carries = 0;
    std::uint32_t dangles = 0;
    std::uint32_t armplays = 0;
    std::uint32_t releases = 0;
    std::uint32_t spares = 0;
    std::uint32_t grips_torn = 0;
    std::uint32_t retakes = 0;   // her grip torn by the world (not the player), her hand on them again
    std::uint32_t arm_reposes = 0;   // ArmPlay re-posed: the wrist out of reach, or the pull not telling
    std::uint32_t brace_losses = 0;
    std::uint32_t carry_drops = 0;
    std::uint32_t arm_tears = 0;
    std::uint32_t arm_throws = 0;
    std::uint32_t claims = 0;
    // The fatality (Matron::kFatalTargetS): its path (Matron::FatalPath: 0
    // none, 1 the hang over the drop, 2 the floor), the hang's window and its
    // seconds, the edge and the way out over the drop, the release beat (a
    // count that rises as she lets go of him over the drop: the viewer's
    // slow motion, pull-out and fade), his escapes by his blow in it, and his
    // blows out over the drop in any hold that she reeled back from.
    std::uint8_t fatal_path = 0;
    bool fatal_hang = false;
    double fatal_window_s = 0.0;
    Vector3 fatal_edge{};
    double fatal_out_x = 0.0, fatal_out_z = 0.0;
    std::uint32_t fatal_release_beat = 0;
    std::uint32_t fatal_escapes = 0;
    double fatal_foreseen_mps = 0.0;   // in the hang: his fall from now foreseen to meet steel at this speed down
    double fatal_toss_mps = 0.0;       // her toss out with the let-go it needs
    std::uint32_t reels = 0;
    std::uint8_t armplay_beat = 0;
    std::uint8_t fatal_beat = 0;     // Matron::Fatal while Claimed
    std::uint32_t waste_not = 0;     // the 'Waste not.' voice hook
    Vector3 hand2{};
    bool grip_on = false;
    bool hold_braced = false;
    double hold_s = 0.0;
    Vector3 hand{};
    bool aim_ledge = false;
    Vector3 aim{};
    static constexpr int kPlanks = 11;
    // x, y, z, qx, qy, qz, qw per plate; bolted and gone as 0/1.
    std::array<double, kPlanks * 9> planks{};
};

// The bay on the 154 m deck (bay.hpp): its leaf, its district (the leaf's
// drive and the fan), the chain, and hands in it.
struct BaySnapshot final {
    double leaf_angle = 0.0;      // rad, 0 level on its stop, down negative
    double leaf_speed = 0.0;      // rad/s
    bool leaf_running = false;    // its motor holding it
    bool leaf_open = false;       // a floor to walk: level and driven
    double fan_fraction = 0.0;    // its speed over its running speed
    bool fan_running = false;
    bool hooked = false;          // the chain on the leaf's eye
    double chain_n = 0.0;
    bool hands = false;           // hands in the district (hers, holding the leaf)
    double support_n = 0.0;
    Vector3 hand{};               // where her palms are
    std::uint32_t trips = 0;      // times hands stopped the district
    std::uint32_t restarts = 0;   // times it started again by itself
    double cover = 1.0;           // of the player's noise, what the fan leaves her to hear (1 none covered)
};

// AS-012, the swing off the 220 ring: the rider's seat and the ram on their
// arms, the buffer between them, the trip and the rack.
struct SwingSnapshot final {
    bool station_available = false;  // standing in the seat: STRAP IN offered
    bool seated = false;             // harnessed into the seat
    bool may_leave = true;           // the harness opens now (not mid-ride)
    bool ram_held = true;            // the ram on its hook
    bool tripped = false;            // the kick has let the ram go
    bool held_at_top = false;        // the pawl in a tooth of the rack
    int tooth = -1;
    double seat_angle_rad = 0.0;     // north of plumb
    double ram_angle_rad = 0.0;      // north of plumb (held back: negative)
    double seat_speed_mps = 0.0;
    double ram_speed_mps = 0.0;
    double seat_floor_y = 0.0;
    double apex_floor_y = 0.0;
    double kick_travel_m = 0.0;
    double buffer_compression_m = 0.0;
    double buffer_force_n = 0.0;
    double peak_buffer_force_n = 0.0;
    double peak_seat_accel_mps2 = 0.0;
    double buffer_loss_j = 0.0;
    double energy_residual_j = 0.0;
    double mechanical_j = 0.0;
    Vector3 seat_pivot{};
    Vector3 ram_pivot{};
    Vector3 seat_pin{};
    Vector3 ram_pin{};
};

// A forklift (src/sim/forklift.hpp): the yard's, or the yard crane's.
struct ForkliftSnapshot final {
    int index = 0;                   // 0 the yard's, then the yard crane's
    std::uint64_t chassis_entity = 0;
    bool driving = false;
    bool at_controls = false;        // on its floor at the wheel: DRIVE offered
    double lift_m = 0.0;             // the forks above their lowest
    double speed_mps = 0.0;          // along its forward
    double steer = 0.0;              // the steered wheels, -1 .. 1 (right +)
    double drive = 0.0;              // the throttle as applied, -1 .. 1
    double lift = 0.0;               // the lift lever as applied: + raising, - lowering
    double lift_n = 0.0;             // the lift's push up the mast this tick
    double lift_max_n = 0.0;         // the most it can push (the relief valve)
    double pitch_rad = 0.0;          // nose down, + (on its front wheels)
    std::array<double, 4> wheel_load_n{};
    std::array<bool, 4> wheel_contact{};
    // Each wheel: its center and its turn, a cylinder along its own y (the
    // axle), and its size. Front left, front right, rear left, rear right.
    std::array<Vector3, 4> wheel_position{};
    std::array<Quaternion, 4> wheel_rotation{};
    std::array<double, 4> wheel_radius{};
    std::array<double, 4> wheel_width{};
    // What its parts are doing (forklift.hpp State): the gear selected (+1,
    // -1, 0), the throttle, the engine's rpm, the forks' speed up the mast,
    // each wheel's spin (rad/s) and what it rolls on (an entity, 0 none), and
    // the blades' clearance over what is under them and their scrape speed.
    int gear = 0;
    double throttle = 0.0;
    double engine_rpm = 0.0;
    double lift_mps = 0.0;
    std::array<double, 4> wheel_spin_rad_s{};
    std::array<std::uint64_t, 4> wheel_ground{};
    double fork_gap_m = 1.0;
    double fork_drag_mps = 0.0;
    std::uint64_t fork_ground = 0;
};

// The yard crane (src/sim/yard_crane.hpp).
struct YardCraneSnapshot final {
    bool has_remote = false;
    bool remote_in_reach = false;   // the remote in its cabinet, within the hands' reach: TAKE offered
    bool able = false;              // had, and the body could work it now
    bool operating = false;
    double slew_rad = 0.0;          // the jib's heading: 0 along +x, + toward -z
    double slew_rate = 0.0;
    double trolley_m = 0.0;         // out from the mast
    double trolley_rate = 0.0;
    double hoist_m = 0.0;           // the hoist rope as it lies
    double hoist = 0.0;             // as worked: + raising, - lowering
    double hoist_n = 0.0;           // the rope's pull this tick
    double hoist_max_n = 0.0;       // the winch's most
    bool hoist_stalled = false;
    bool upper_limit = false;
    bool slack = false;
    std::uint64_t load_entity = 0;  // what the sling is hooked onto, 0 for none
    Vector3 rope_top{};             // the hoist rope's ends: the trolley's sheaves
    Vector3 rope_bottom{};          // and the hook block's
    Vector3 hook{};                 // the hook's throat
    // The hoist's drum as driven and what the parts measure after the step
    // (yard_crane.hpp State): m/s of rope (+ in), the jib's turn (rad/s), the
    // slewing torque, the trolley's run along the jib (+ out) and its drive's
    // pull, the rope coming in (+), the hook block's speed, the sling's pull.
    double hoist_rate = 0.0;
    double slew_measured = 0.0;
    double slew_torque_nm = 0.0;
    double trolley_mps = 0.0;
    double trolley_force_n = 0.0;
    double rope_mps = 0.0;
    double hook_mps = 0.0;
    double sling_n = 0.0;
    // The device on the sling (0 none, 1 the magnet, 2 the spreader), the
    // magnet's switch, and what the device holds (0 nothing): the hold's pull
    // and what it holds to before the load tears off. The spreader: its
    // twistlocks over a box's castings, sitting on it; landed, what it sits
    // on or holds at rest and the rope slack (LOCK and UNLOCK only then).
    int device = 0;
    bool magnet_on = false;
    std::uint64_t holding_entity = 0;
    double hold_n = 0.0;
    double hold_rated_n = 0.0;
    bool spreader_seated = false;
    bool spreader_landed = false;
};

// What a struck body sounds like (src/sim/impacts.hpp), by what it is made
// of and how it is built: not its paint (kit::Material).
enum class ImpactSound : std::uint8_t {
    Timber = 0,      // crates, pallets, baulks, sleepers, planks, wooden reels
    Hollow = 1,      // thin steel round air: drums, gas bottles, a toolbox
    HeavySteel = 2,  // solid steel: beams, pipe, plate, billets, coils
    Container = 3,   // a shipping container's corrugated box
    Machine = 4,     // a forklift's frame or forks, the yard crane's hook block
};

// Why a fabricated object's data (sim/fabricated.hpp) was refused, None when
// it was taken. Nothing in the world changes when it is refused.
enum class FabricatedRefusal : std::uint8_t {
    None = 0,
    Size = 1,           // fewer or more bytes than a hull file can have
    Format = 2,         // not a hull file: its magic, or its parts do not add up to its size
    Version = 3,
    Material = 4,       // a material the game does not have
    HullCount = 5,      // no hull, or more than fabricated::kMaxHulls
    PointCount = 6,     // a hull of fewer than 4 or more than 32 points
    NotFinite = 7,      // a number that is NaN or infinite
    Oversized = 8,      // a side over the dispatch carrier's bound, or a point outside its box
    Undersized = 9,     // a side under the least an object may be
    Degenerate = 10,    // a hull that is no solid
    TooHeavy = 11,      // more than the hands carry
    TooLight = 12,
    MassMismatch = 13,  // its mass is not its material's density times its hulls' volume
    Full = 14,          // the world holds fabricated::kMaxObjects already, or has no room for a body
    Placement = 15,     // where it was to go is not a finite place in the world, or inside something
};

// One fabricated object as the world holds it: its entity and kit body, what
// it is made of, its mass, its box about its origin, and the SHA-512 of the
// model the presentation draws it with.
struct FabricatedObject final {
    std::uint64_t entity = 0;
    std::uint32_t body = 0xFFFFFFFFU;
    std::uint8_t material = 0;
    double mass_kg = 0.0;
    Vector3 half_extent{};
    std::array<std::uint8_t, 64> glb_sha512{};
};

// One hit a tracked body took: its change of velocity in one tick past
// ImpactLedger::kMinChangeMps.
struct ImpactSnapshot final {
    std::uint64_t sequence = 0;   // 1 for the world's first hit, counting up
    std::uint64_t entity = 0;     // the body struck
    Vector3 at{};                 // its center of mass as the tick ended
    double energy_j = 0.0;        // half its mass times its change of speed squared
    double speed_mps = 0.0;       // that change of velocity
    double mass_kg = 0.0;
    ImpactSound sound = ImpactSound::Timber;
};

// Phase 3: the trapeze in the yard (src/sim/trapeze.hpp).
struct TrapezeSnapshot final {
    bool catch_available = false;    // in the air, the bar in the hands' reach
    bool catch_near = false;         // in the air, closing on the bar: GRAB offered (held 0.33 s)
    bool harnessed = false;          // hands on the bar
    bool pulled = false;             // the arms pulling up (Crouch held)
    double catch_speed_mps = 0.0;    // the body's speed relative to the bar at the last catch
    double catch_loss_j = 0.0;       // what that catch took out
    double hand_force_n = 0.0;
    double peak_hand_force_n = 0.0;
    int catches = 0;
    int slips = 0;
    double angle_rad = 0.0;          // east of plumb
    double angular_speed = 0.0;
    double peak_angle_rad = 0.0;
    double pulled_in_m = 0.0;
    double arm_work_j = 0.0;         // the arms' net work since the bar was taken
    double arm_work_out_j = 0.0;     // of which taken out
    double mechanical_j = 0.0;
    double energy_residual_j = 0.0;
    int pumps = 0;
    Vector3 bar{};
};

// The owner's vertical machines (src/sim/vertical): the one the player stands
// on, if any, and where every deck is between its receivers.
struct LiftSnapshot final {
    int machine = -1;         // the machine underfoot, -1 when on none
    int role = 0;             // 1 its deck, 2 its lower receiver, 3 its upper one
    double travel = 0.0;      // that deck: 0 at the lower receiver, 1 at the upper
    double target = 0.0;      // where it was last sent
    bool moving = false;
    int machine_count = 0;
    std::array<double, 16> travels{};
    // The slab incline: 0 armed (slab caught at its top), 1 released, 2 being
    // wound back down; the friction its brake treadle presses on the rail;
    // its trolley's speed along the rail.
    int incline_state = 0;
    double incline_brake_n = 0.0;
    double incline_speed_mps = 0.0;
    // The traction tram: seconds until it leaves the end it stands at, -1
    // while it runs.
    double tram_departs_s = -1.0;
    // The luffing derrick: iron pigs on its cradle now, and whether its
    // motor holds the cradle down (called down, until a rider boards).
    int derrick_iron_aboard = 0;
    bool derrick_holding = false;
    // The pitman lift (-1 when not placed), and the pull in the hands now
    // when hauling its line (N; 0 when not).
    int pitman = -1;
    double pitman_haul_n = 0.0;
    // The stone wheel's stone: in its hopper, and in the reload skip over it.
    double wheel_hopper_kg = 0.0;
    double wheel_skip_kg = 0.0;
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

// A vent collar (AS-007 E): where it stands, low to high, and the air it let
// out last step, m^3/s at atmospheric pressure.
struct KitVent final {
    Vector3 low{};
    Vector3 high{};
    double flow_m3s = 0.0;
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
    double d_drain_angle = 0.0;
    double dump_angle = 0.0;
    // E: both leaves under 0.05 rad; the wider-open leaf's angle; how far
    // the deck stands below where it hangs (0 hung, 0.15 on the cab's
    // plate); the trip lever's angle; the cab's speed up its shaft; the air
    // through the throttle into the cab's cell and out of the vent collar,
    // m^3/s at atmospheric pressure.
    bool e_door_shut = false;
    double e_door_angle = 0.0;
    double e_deck_drop = 0.0;
    double e_lever_angle = 0.0;
    double e_cab_speed = 0.0;
    double e_throttle_m3s = 0.0;
    double e_vent_m3s = 0.0;
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

// AS-010's machine state, for the falsifiers.
struct ServiceState final {
    bool m_rope_on_eye = false;
    bool m_reel_latched = false;
    double m_cage_travel = 0.0;
    double m_reel_travel = 0.0;
    double m_reel_kg = 0.0;       // drum, yoke and the cable still wound
    double m_cable_paid = 0.0;    // m
    double m_rope_tension = 0.0;  // N
    // Stage N, the granular discharge hoist.
    double n_cage_travel = 0.0;
    double n_cage_peak_speed = 0.0;
    double n_hopper_travel = 0.0;
    double n_gate_angle = 0.0;
    double n_silo_kg = 0.0;       // gravel still in the silo
    double n_hopper_kg = 0.0;     // gravel in the hopper
    double n_chain_kg = 0.0;      // chain hanging from the hopper
    double n_hopper_mass = 0.0;   // the hopper, its gravel and its hanging chain
    double n_rope_tension = 0.0;  // N
    // Stage O, the gravel wheel.
    bool o_ram_latched = false;
    bool o_gate_latched = false;
    double o_cab_travel = 0.0;
    double o_cab_peak_speed = 0.0;
    double o_bucket_travel = 0.0;
    double o_wheel_angle = 0.0;   // rad, in [-pi, pi]
    double o_ram_angle = 0.0;
    double o_latch_angle = 0.0;
    double o_gate_angle = 0.0;
    double o_bin_kg = 0.0;        // gravel still in the bin
    double o_bucket_kg = 0.0;     // gravel in the bucket
    double o_chain_kg = 0.0;      // chain hanging from the bucket
    double o_bucket_mass = 0.0;   // the bucket, its gravel and its hanging chain
    double o_rope_tension = 0.0;  // N, on the bucket's side of the wheel
};

// Band 0, the Stack's machine state, for the falsifiers.
struct StackState final {
    // S1, the water-balance hoist.
    double s1_cage_travel = 0.0;
    double s1_cage_peak_speed = 0.0;
    double s1_bucket_travel = 0.0;
    double s1_bucket_water_kg = 0.0;
    double s1_tank_water_kg = 0.0;
    double s1_valve_angle = 0.0;
    bool s1_catch_latched = false;
    bool s1_valve_pawled = false;   // the pawl holds the valve open
    double s1_plate_travel = 0.0;   // the scale plate, from where it hangs proud of the floor
    double s1_rope_tension = 0.0;

    // S2, the walking beam hoist with fixed ballast cart.
    double s2_cage_travel = 0.0;
    double s2_cage_peak_speed = 0.0;
    double s2_cart_travel = 0.0;
    double s2_beam_angle = 0.0;
    double s2_bolt_travel = 0.0;    // how far the shoot bolt is shoved out
    bool s2_bolt_shot = false;      // the bolt holds the beam
    bool s2_cage_latched = false;
    double s2_rope_tension = 0.0;

    // S3, the brake-override hoist.
    double s3_cage_travel = 0.0;
    double s3_cage_peak_speed = 0.0;
    double s3_car_travel = 0.0;
    double s3_brake_angle = 0.0;
    bool s3_brake_latched = false;
    bool s3_cage_latched = false;
    double s3_rope_tension = 0.0;
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
    // Parkour step 2: hanging by the hands from a bar or pipe overhead, the
    // body free to swing under it and reach hand over hand to the next.
    Swinging = 6,
};

// The gesture controls' one "up" (request_up): the move the body makes
// upward from where it is, chosen here from what is ahead of it.
enum class UpMove : std::uint8_t {
    None = 0,          // nothing to do now (a mantle under way, a seat)
    Jump = 1,          // a plain jump; in the air, held for the landing
    Precision = 2,     // the sighted landing jumped (M3)
    Springboard = 3,   // a run's foot planted on a top close ahead
    Vault = 4,         // over what is ahead (a speed vault at a run)
    Mantle = 5,        // up on to what is ahead
    Climb = 6,         // the hold ahead at hand height taken
    Stand = 7,         // crouched: stand up, no jump
    ClimbUp = 8,       // hanging: pull up over the lip
    TopOut = 9,        // on a climb's holds at the top: over on to it
    JumpOff = 10,      // on a climb's holds: jump off them
    Launch = 11,       // on a bar or zipline: thrown on as the hands open
    Kick = 12,         // running a wall: kicked off it
    Catch = 13,        // the trapeze's bar in reach: taken
    LetGo = 14,        // on the trapeze: its bar let go
    Throw = 15,        // a piece of loose stock in the hands: thrown along the view
};

// WO-008. Parachuting is a sub-state of Airborne, not a fourth motion primitive
// -- the player is still under normal air control, with drag layered on top.
enum class FallState : std::uint8_t {
    Grounded = 0,
    Airborne = 1,
    Parachuting = 2,
};

// One of the yard crew (Life L2, sim/crew.hpp), as the presentation needs it.
struct WorkerView final {
    Vector3 position{};       // the capsule's center
    double yaw = 0.0;         // radians; 0 faces +z
    std::uint8_t state = 0;   // 0 walking, 1 working, 2 yielding, 3 startled, 4 stumbling
    double speed = 0.0;       // horizontal, m/s
    double stride = 0.0;      // meters walked
    bool grounded = true;
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
    // Mirror's Edge's tuck: the legs drawn up in a jump (see kTuckMaximumFallMps).
    bool player_tucked = false;
    std::uint64_t tuck_count = 0;
    // Springboards jumped (physics_world kSpringboardMinimumSpeed), and the
    // plant under way: a foot going onto a top to jump from it.
    std::uint64_t springboard_count = 0;
    std::uint64_t throw_count = 0;   // loose stock thrown from the hands (UpMove::Throw)
    bool player_springboarding = false;
    // Parkour step 3: crouched at a run, the body slides on (see kSlideFriction).
    bool player_sliding = false;
    std::uint64_t slide_count = 0;
    // Parkour step 4: steps taken on a wall in the air (see kWallStepPushMps),
    // and whether one was taken in the last kWallRunHoldSeconds.
    std::uint64_t wall_step_count = 0;
    std::uint64_t wall_kick_count = 0;
    // Mirror's Edge's wall climb: a run jumped up a tall wall (see
    // kWallClimbMinimumSpeed).
    std::uint64_t wall_climb_count = 0;
    // The bodies deaths left (sim/corpse.hpp): how many lie in the world,
    // how many deaths have left one, and the latest death's place (where
    // the body met what killed it) and speed.
    std::uint32_t corpse_count = 0;
    std::uint64_t corpses_spawned = 0;
    Vector3 death_point{};
    double death_speed_mps = 0.0;
    // The latest death came side-on (run into something, or run into), not
    // from a fall onto the floor (see kHitFeltSpeed).
    bool death_side_on = false;
    // Hits the body felt and lived (see kHitFeltSpeed), and the last one's
    // closing speed.
    std::uint64_t hit_count = 0;
    double hit_speed_mps = 0.0;
    // Riding a zipline's trolley (sim/zipline.hpp): the swing's hands on
    // its bar.
    bool zipline_riding = false;
    int zipline_line = -1;            // the line ridden
    std::uint64_t zipline_ride_count = 0;
    double zipline_speed_mps = 0.0;   // down the line
    double zipline_along_m = 0.0;     // from the top anchor
    bool zipline_braking = false;     // the hand brake on, or in the end buffer
    double zipline_hand_brake = 0.0;  // the squeeze, 0..1
    double zipline_hands_load_n = 0.0;  // what the bar puts on the hands (they tear past the ledge grip)
    std::uint64_t zipline_torn_count = 0;   // rides ended with the hands torn off at the stop
    bool player_wall_running = false;
    // The wall run along: its face's outward normal, level (zero off a wall).
    Vector3 player_wall_normal{};
    // On the wall, run up it (a climb this flight, the foot still on it).
    bool player_wall_climbing = false;
    // Parkour step 5, the grappling hook. hook_state: 0 stowed, 1 flying,
    // 2 bitten. The line runs from the body's chest to hook_position;
    // hook_line_length is its present limit, hook_tension_n its pull.
    bool has_hook = false;
    bool hook_take_available = false;   // its rack in reach, not yet taken
    std::uint8_t hook_state = 0;
    Vector3 hook_position{};
    double hook_line_length = 0.0;
    double hook_tension_n = 0.0;
    std::uint64_t hook_fire_count = 0;
    std::uint64_t hook_bite_count = 0;
    // The line as it lies (task #102): from the chest, through each place it
    // bends round the structure (nearest the body first), to hook_position.
    // hook_line_length is the whole rope out: the free length to the nearest
    // bend plus what lies laid beyond it.
    static constexpr std::uint32_t kHookLinePointsMax = 10;
    std::uint32_t hook_line_point_count = 0;
    std::array<Vector3, kHookLinePointsMax> hook_line_points{};
    std::uint32_t hook_bend_count = 0;
    std::uint64_t hook_wrap_count = 0;
    std::uint64_t hook_unwrap_count = 0;
    std::uint64_t hook_bends_refused = 0;
    // Bites torn or slipped off their member by a load past the tines' hold.
    std::uint64_t hook_tear_count = 0;
    // The crew: how many, each one, and what the player has made them do.
    std::uint8_t worker_count = 0;
    std::array<WorkerView, 14> workers{};
    std::uint32_t worker_yields = 0;
    std::uint32_t worker_startles = 0;
    std::uint32_t worker_stumbles = 0;
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
    // The fireman's slide down a climb's holds (physics_world kClimbSlideMps).
    bool player_climb_sliding = false;
    std::uint64_t climb_slide_count = 0;
    // Hangs torn off by a load past the hands' grip (kLedgeGripMaxForceN).
    std::uint64_t hang_slip_count = 0;
    // Bars and pipes caught overhead, and reaches from one to the next.
    std::uint64_t swing_catch_count = 0;
    std::uint64_t swing_reach_count = 0;
    std::uint64_t swing_launch_count = 0;   // Jumps that threw the body on off a swing
    // Static dressing loaded from world_solids.inc: bodies built, drawn
    // mirrors of owned bodies skipped, and hulls Jolt refused (must be 0).
    // Edges under kStepMaximumHeight walked up (see try_step_up).
    std::uint64_t step_up_count = 0;
    // The feet put back on the ground a step below after a crest or a
    // stair's step (try_step_down).
    std::uint64_t step_down_count = 0;
    // The height those steps lifted the body, in all: a step is one tick's
    // lift, which the presentation eases the eye over instead of snapping.
    double step_up_meters = 0.0;
    // Vaults begun by a second Jump inside the window after a takeoff.
    std::uint64_t jump_vault_count = 0;
    // M3, the precision jump: a landing the eyes are on now, and the jumps
    // made at one and the landings that hit it.
    bool precision_sighted = false;
    Vector3 precision_point{};
    std::uint64_t precision_jump_count = 0;
    std::uint64_t precision_landing_count = 0;
    std::uint32_t world_solid_bodies = 0;
    std::uint32_t world_solid_mirrors = 0;
    std::uint32_t world_solid_rejected = 0;
    std::uint64_t rejected_traversal_count = 0;
    std::uint64_t aborted_traversal_count = 0;
    // What request_up would do now, and what the last one did (and how many
    // were taken).
    UpMove up_move = UpMove::None;
    UpMove up_resolved = UpMove::None;
    std::uint64_t up_count = 0;

    // --- WO-008 fall / parachute / checkpoint -----------------------------
    FallState fall_state = FallState::Grounded;
    double fall_peak_speed_mps = 0.0;
    double last_impact_speed_mps = 0.0;
    // Phase 4: the last landing and the recovery from it. landing_response:
    // 0 none yet, 1 absorbed by the legs, 2 heavy, 3 roll, 4 stumble (a slide
    // across the facing). recovery_balance runs from 1 - landing_balance_loss
    // back to 1; traction and reachable speed scale with it. roll_progress is
    // 0..1 through a roll, 0 otherwise; roll_travel_m the last roll's travel.
    std::uint8_t landing_response = 0;
    double landing_normal_speed_mps = 0.0;
    double landing_tangent_speed_mps = 0.0;
    double landing_balance_loss = 0.0;
    double recovery_balance = 1.0;
    double roll_progress = 0.0;
    double roll_travel_m = 0.0;
    std::uint64_t landing_count = 0;
    std::uint64_t roll_count = 0;
    // Rolls asked for (crouch held, fast, forward) with no clear lane.
    std::uint64_t roll_refused_count = 0;
    // Rolls ended by something in the lane.
    std::uint64_t roll_blocked_count = 0;
    std::uint64_t stumble_count = 0;
    // Injuries (see kAnkleBreakSpeed): a broken ankle's limp, the time left
    // on it, and every break so far.
    bool limping = false;
    double limp_seconds_left = 0.0;
    std::uint64_t ankle_break_count = 0;
    // A broken arm (see kArmBreakHitSpeed): one good arm, the time left on
    // it, every break so far, and which (0 the left, 1 the right).
    bool arm_broken = false;
    double arm_broken_seconds_left = 0.0;
    std::uint64_t arm_break_count = 0;
    std::int32_t broken_arm_side = 1;
    // Water (Kit::add_pool): the body in a pool's water; afloat; the head
    // under; how much of the body is under (0..1); the surface's height; the
    // pool (-1 none); every entry and exit, and the latest entry's falling
    // speed (the splash's size and the haptic's).
    bool in_water = false;
    bool swimming = false;
    bool head_underwater = false;
    double water_submersion = 0.0;
    double water_surface_y = 0.0;
    std::int32_t water_pool = -1;
    std::uint64_t water_entry_count = 0;
    std::uint64_t water_exit_count = 0;
    double water_entry_speed_mps = 0.0;
    std::uint64_t swim_kick_count = 0;
    // Her hold (physics_world_grab.cpp). The body turned freely (held, or
    // righting after), its rotation and spin for the camera; her grip's load
    // and strain; the player's brace; the thrash; every edge counted.
    bool held_by_matron = false;
    bool player_free_rotation = false;
    // The player body's length, crown to soles (or hips, both legs gone).
    double player_body_length_m = 0.0;
    double player_rot_x = 0.0, player_rot_y = 0.0, player_rot_z = 0.0, player_rot_w = 1.0;
    Vector3 player_angular_velocity{};
    double grip_load_n = 0.0;
    double grip_peak_n = 0.0;
    double grip_strain_ns = 0.0;
    bool grip_hinged = false;
    bool braced_on_rail = false;
    double brace_load_n = 0.0;
    double thrash = 0.0;
    std::uint64_t grip_count = 0;
    std::uint64_t grip_tear_count = 0;
    std::uint64_t grip_release_count = 0;
    std::uint64_t brace_count = 0;
    std::uint64_t brace_tear_count = 0;
    std::uint64_t thrash_kick_count = 0;
    std::uint64_t upright_count = 0;
    std::uint64_t upright_wedged_count = 0;
    double max_hold_step_m = 0.0;
    // Her hand (the body her muscle drives) while it is out; and, while her
    // fingers hold him, the held ankle point: where on his body they hold,
    // in the world (her hand is on it, by the grip). The drawing's arm goes
    // from her shoulder to matron_hand, his leg's end to held_ankle.
    Vector3 matron_hand{};
    bool matron_hand_present = false;
    Vector3 held_ankle{};
    // Arms (physics_world_arm.cpp): lost per side until a death or a
    // restart; the severed arm's body; her win.
    bool left_arm_lost = false;
    bool right_arm_lost = false;
    int last_severed_side = -1;
    std::uint64_t arm_tear_count = 0;
    std::uint64_t arm_throw_count = 0;
    std::uint64_t severed_arm_despawn_count = 0;
    bool severed_arm_present = false;
    Vector3 severed_arm_position{};
    double severed_arm_rot_x = 0.0, severed_arm_rot_y = 0.0, severed_arm_rot_z = 0.0, severed_arm_rot_w = 1.0;
    double wrist_load_n = 0.0;
    double wrist_strain_ns = 0.0;
    int wrist_side = -1;          // the arm she has by the wrist (0 left, 1 right)
    bool one_arm = false;
    Vector3 matron_hand2{};
    bool claimed_by_matron = false;
    double claimed_s = 0.0;
    // The fatality (Claimed), its beats (Matron::Fatal: 0 lay, 1 pin, 2 left
    // leg, 3 right leg, 4 drag, 5 kick, 6 haul, 7 fall, 8 wipe, 9 black) and
    // each beat's entry counted: an edge for the camera, the sound and the VO.
    // One good arm: the hand the hang is on (-1 both), the lunge and the
    // heel hook.
    int hang_hand_side = -1;
    std::uint64_t dyno_launch_count = 0;
    std::uint64_t dyno_catch_count = 0;
    std::uint64_t dyno_miss_count = 0;
    std::uint64_t dyno_refused_count = 0;
    double dyno_timing_error_s = 0.0;   // the last press, from the dead point (+ late)
    bool dyno_in_flight = false;
    std::uint64_t heel_hook_count = 0;
    std::uint64_t heel_hook_refused_count = 0;
    int fatality_beat = -1;   // -1 when not Claimed
    std::array<std::uint64_t, 10> fatality_beat_edges{};
    std::uint64_t knee_pin_count = 0;
    std::uint64_t leg_tear_count = 0;
    std::uint64_t leg_toss_count = 0;
    bool left_leg_lost = false;
    bool right_leg_lost = false;
    double leg_load_n = 0.0;
    double leg_peak_n = 0.0;     // the hip's peak, the last leg
    std::array<double, 2> leg_strain_ns{};  // per hip, toward Matron::kLegTearNs
    std::array<bool, 2> severed_leg_present{};
    std::array<Vector3, 2> severed_leg_position{};
    std::array<Quaternion, 2> severed_leg_rotation{};
    std::uint64_t collar_count = 0;
    std::uint64_t shackle_unhook_count = 0;
    std::uint64_t knot_count = 0;
    double knot_load_n = 0.0;
    double knot_peak_n = 0.0;
    std::uint64_t brake_kick_count = 0;   // the catch's pin out: the skip goes
    std::uint64_t haul_count = 0;
    double haul_from_y = 0.0;
    double haul_peak_y = 0.0;
    std::uint64_t knot_part_count = 0;
    std::uint64_t fall_landing_count = 0;
    double fall_landing_speed_mps = 0.0;
    double fall_drop_m = 0.0;
    std::uint64_t waste_not_count = 0;    // the voice hook: "Waste not."
    double wipe_s = 0.0;
    Vector3 matron_knee{};
    bool matron_knee_down = false;
    std::uint64_t claimed_count = 0;
    std::uint64_t runs_ended_by_matron = 0;   // the edge: a new count, the run ended and started again whole
    // The XCoM past the edge of the footing, the stick not driving that way:
    // the feet brake only as hard as soles can (see kTeeterComHeight).
    bool player_teetering = false;
    // On a beam: the XCoM across it as a fraction of the center of
    // pressure's reach (see kBalanceHipMargin); past +-1 it cannot come back.
    double beam_xcom = 0.0;
    // The same, as a horizontal world vector toward the side it is on.
    Vector3 beam_lean{};
    // A support jerked under the feet (kSupportJoltMargin): the largest XCoM
    // shift a jolt has left, and the stumbles jolts have caused (also counted
    // in stumble_count).
    double support_jolt_peak_m = 0.0;
    std::uint64_t support_stumble_count = 0;
    // Jumps that pushed off a dynamic support (its equal and opposite impulse).
    std::uint64_t jump_reaction_count = 0;
    // The wind off the west face where C7 goes out (kWindZoneX): its speed at
    // the body (0 outside), and the push it gave this tick (0 standing).
    double wind_mps = 0.0;
    double wind_force_n = 0.0;
    double wind_peak_force_n = 0.0;   // the most it has pushed, N
    // Blows from something moving that reached the body's balance
    // (kShoveClosingMps); whether one became a stumble is in stumble_count.
    std::uint64_t shove_count = 0;
    // The player's strike (Action's punch): its phase (0 guard, 1 wind-up,
    // 2 reach, 3 recoil) and ticks in it, whether Action offers it now, the
    // strikes thrown and landed (a landing is an edge on strike_hit_count),
    // and the last landing's point, her surface normal there and the
    // impulse it put into her (N*s, force times time).
    std::uint8_t strike_phase = 0;
    std::uint32_t strike_phase_ticks = 0;
    bool strike_offered = false;
    std::int8_t strike_arm = 1;   // the fist he throws with: 1 right, 0 left, -1 none usable
    bool strike_ready = false;    // a punch can be thrown now: the PUNCH button lit, else greyed

    bool strike_landed = false;   // this strike has landed on her
    std::uint64_t strike_count = 0;
    std::uint64_t strike_hit_count = 0;
    Vector3 strike_hit_point{};
    Vector3 strike_hit_normal{};
    double strike_impulse_ns = 0.0;
    // The top of the route (kRouteTopEntityId) stood on, firm, outside a
    // traversal: latched with the simulation time and the deaths then.
    bool summit_reached = false;
    double summit_seconds = 0.0;
    std::uint64_t summit_deaths = 0;
    std::uint64_t teeter_count = 0;
    bool parachute_deployed = false;
    Vector3 checkpoint_position{};
    std::uint64_t checkpoint_commit_count = 0;
    std::uint64_t death_count = 0;

    // The carry. carrying_entity_id is the body on the player's carry point
    // (0 for none), read from the carry constraint's actual presence;
    // carry_target_entity_id is what a pick-up would take this tick.
    std::uint64_t carrying_entity_id = 0;
    std::uint64_t carry_target_entity_id = 0;

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
    // The player's arm she tore off (physics_world_arm.cpp).
    static constexpr std::uint64_t kSeveredArmEntityId = 3290;
    static constexpr std::uint64_t kSeveredLegEntityId = 3291;   // and 3292, the right
    static constexpr std::uint64_t kTranslatingSupportEntityId = 3;
    static constexpr std::uint64_t kRotatingSupportEntityId = 4;
    static constexpr std::uint64_t kVaultRailEntityId = 5;
    static constexpr std::uint64_t kMantleLedgeEntityId = 6;
    static constexpr std::uint64_t kHangLedgeEntityId = 7;
    static constexpr std::uint64_t kMovingLedgeEntityId = 8;
    static constexpr std::uint64_t kBlockedLedgeEntityId = 9;
    static constexpr std::uint64_t kBlockedLedgeCanopyEntityId = 10;
    static constexpr std::uint64_t kTowerEntityId = 11;
    // Every static body drawn by the presentation's builders that the native
    // world did not already own: frame dressing, footings, halls, rails,
    // trees. Generated into world_solids.inc; see build_world_solids().
    static constexpr std::uint64_t kWorldSolidEntityId = 51;
    // Crouch fixture beside the WO-003 traversal fixtures: a beam and its two
    // posts, the beam's underside 1.45 m over the deck.
    static constexpr std::uint64_t kCrawlBeamEntityId = 52;
    // Brachiation fixture: six bars a meter apart, 3.1 m up, between two
    // beams on four posts, and a landing block past the last.
    static constexpr std::uint64_t kMonkeyBarsEntityId = 53;
    // Wall-run fixture: a wall 12 m long and 4 m tall, its face at z = -26.75.
    static constexpr std::uint64_t kRunWallEntityId = 54;
    // Where the grappling hook is found (a rack; see request_hook_take), and
    // the hook itself while it is out.
    static constexpr std::uint64_t kHookRackEntityId = 55;
    static constexpr std::uint64_t kHookEntityId = 56;
    // The game's hook rack on the 220 ring, and the hook gun lying on it.
    static constexpr std::uint64_t kHookRackFrameEntityId = 1959;
    static constexpr std::uint64_t kHookGunEntityId = 2970;

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
    static constexpr std::uint64_t kWetEDoorEntityId = 2042;       // the west leaf
    static constexpr std::uint64_t kWetEEastLeafEntityId = 2043;
    static constexpr std::uint64_t kWetELeverEntityId = 2044;
    static constexpr std::uint64_t kWetEDeckEntityId = 2045;
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
    // AS-010 Midstack Service: its static structure and its climbing route,
    // C4, the service gantry climbed from the 662 deck to the 684 deck, stage
    // N's silo, C5, the cooling plant climbed from the 706 deck to the 728
    // deck, and its manifold; then stage M, the service lift on the cable
    // reel, and stage N, the granular discharge hoist.
    static constexpr std::uint64_t kServiceFrameEntityId = 1012;
    static constexpr std::uint64_t kServiceRouteEntityId = 1013;
    static constexpr std::uint64_t kServiceC4EntityId = 1014;
    static constexpr std::uint64_t kServiceNSiloEntityId = 1015;
    static constexpr std::uint64_t kServiceC5EntityId = 1016;
    static constexpr std::uint64_t kServiceC5ManifoldEntityId = 1017;
    static constexpr std::uint64_t kServiceOBinEntityId = 1018;
    static constexpr std::uint64_t kServiceMCageEntityId = 2130;
    static constexpr std::uint64_t kServiceMReelEntityId = 2131;
    static constexpr std::uint64_t kServiceMChockEntityId = 2132;
    static constexpr std::uint64_t kServiceMHandleEntityId = 2133;
    static constexpr std::uint64_t kServiceMShackleEntityId = 2134;
    static constexpr std::uint64_t kServiceNCageEntityId = 2135;
    static constexpr std::uint64_t kServiceNHopperEntityId = 2136;
    static constexpr std::uint64_t kServiceNGateEntityId = 2137;
    static constexpr std::uint64_t kServiceNHandleEntityId = 2138;
    static constexpr std::uint64_t kServiceOCabEntityId = 2139;
    static constexpr std::uint64_t kServiceOBucketEntityId = 2140;
    static constexpr std::uint64_t kServiceOWheelEntityId = 2141;
    static constexpr std::uint64_t kServiceOGateEntityId = 2142;
    static constexpr std::uint64_t kServiceOLatchEntityId = 2143;
    static constexpr std::uint64_t kServiceORamEntityId = 2144;
    static constexpr std::uint64_t kServiceOChockEntityId = 2145;
    static constexpr std::uint64_t kServiceOHandleEntityId = 2146;
    // Band 0, the Stack (grade -> 154 m): its static structure, then S1, the
    // skip hoist.
    static constexpr std::uint64_t kStackFrameEntityId = 1020;
    static constexpr std::uint64_t kStackRouteEntityId = 1021;
    static constexpr std::uint64_t kStackC2RouteEntityId = 1022;
    static constexpr std::uint64_t kStackC3RouteEntityId = 1023;
    static constexpr std::uint64_t kStackS1CageEntityId = 2200;
    static constexpr std::uint64_t kStackS1BucketEntityId = 2201;
    static constexpr std::uint64_t kStackS1LeverEntityId = 2202;
    static constexpr std::uint64_t kStackS1PlateEntityId = 2203;
    static constexpr std::uint64_t kStackS1StrikerEntityId = 2204;
    // S2, the walking beam hoist with fixed ballast cart.
    static constexpr std::uint64_t kStackS2CageEntityId = 2210;
    static constexpr std::uint64_t kStackS2BeamEntityId = 2211;
    static constexpr std::uint64_t kStackS2CartEntityId = 2212;
    static constexpr std::uint64_t kStackS2BoltEntityId = 2213;
    // S3, the brake-override elevator hoist.
    static constexpr std::uint64_t kStackS3CageEntityId = 2220;
    static constexpr std::uint64_t kStackS3CarEntityId = 2221;
    static constexpr std::uint64_t kStackS3BrakeEntityId = 2222;
    // The pan its brake's trip line ends in (once a hanging handle: the id
    // kept, so saves line up).
    static constexpr std::uint64_t kStackS3HandleEntityId = 2223;

    // Height of the tower mass, meters. The crown is far past anything the
    // player can resolve from grade; haze and stack plume shear it earlier.
    static constexpr double kTowerHeightMeters = 1600.0;
    // The top of the route as built: the floor of the luffing derrick's upper
    // receiver, the last thing the climb stands on. What stands up there is
    // the owner's to decide; reaching it is the native's (summit_reached).
    static constexpr double kRouteTopMeters = 802.64;
    static constexpr std::uint64_t kRouteTopEntityId = 1978;

    explicit Simulation(InitialSpawn initial_spawn = InitialSpawn::ExteriorGrade);
    // Tests only (never bound to the bridge): a static box built into the
    // world over and above its own solids, where the world builds the boxes
    // of the 22 m band its y falls in (before the first of them, in the
    // order of world_solids.inc), or after every body the world builds when
    // at_end. The body-order falsifier builds the world with and without
    // them and requires every simulation elsewhere unchanged, bit for bit
    // (03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md).
    struct TestExtraSolid final {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        float half_x = 0.5F;
        float half_y = 0.5F;
        float half_z = 0.5F;
        bool at_end = false;
    };
    Simulation(InitialSpawn initial_spawn, std::vector<TestExtraSolid> test_extra_solids);
    // Tests only: the extra solids every world built with the one-argument
    // constructor gets from now on, so a whole test run can be played in a
    // world with a body added in one band and compared with the run as
    // built (the test binary reads SCRAPERX_EXTRA_SOLID_Y for this).
    static void set_test_extra_solids_for_every_world(std::vector<TestExtraSolid> test_extra_solids);
    ~Simulation();

    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;
    Simulation(Simulation &&) = delete;
    Simulation &operator=(Simulation &&) = delete;

    [[nodiscard]] bool set_move_input(double world_x, double world_z) noexcept;
    [[nodiscard]] bool set_facing(double world_x, double world_z) noexcept;
    // The view's pitch, radians, up positive: what the eyes are on, for the
    // precision jump (PARKOUR_MOVE_SET.md M3).
    [[nodiscard]] bool set_look_pitch(double radians) noexcept;
    [[nodiscard]] bool request_jump() noexcept;
    [[nodiscard]] bool request_traversal() noexcept;
    // The Action button's punch, one-shot (physics_world_strike.cpp).
    [[nodiscard]] bool request_strike() noexcept;
    // The gesture controls' "up", one-shot: the native picks the move, as
    // snapshot.up_move says (UpMove), and makes it through the same paths
    // as Jump and Action. Refused where up_move is None (a mantle, vault or
    // lowering under way, a seat, a machine's controls, a crouch with no
    // room to stand, the tick after a load), so it can be sent again.
    [[nodiscard]] bool request_up() noexcept;
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
    // The player's real room (03_EXECUTION/UNCANNY/HER_CHART.md, her
    // hearing): how loud it is over its own quiet, 0..1, measured from the
    // microphone by the presentation and held like set_move_input. The
    // Matron hears it as the player's own noise, where the player is: a
    // sound of 1 carries as far as a hard landing. Clamped to 0..1; refused
    // when not finite.
    [[nodiscard]] bool set_room_loudness(double loudness) noexcept;
    // Sprint, held: the native sprints only standing, hands free, off a
    // balance beam, with the stick at least 0.7 and within 45 degrees of the
    // facing.
    [[nodiscard]] bool set_sprint_input(bool held) noexcept;

    // Carry commands. One-shot, like request_parachute. A pick-up
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

    [[nodiscard]] AdvanceResult advance_frame(double frame_delta_seconds) noexcept;
    [[nodiscard]] Snapshot snapshot() const noexcept;
    // Read-only visual pose between fixed ticks. Physics, probes and input keep
    // using snapshot().player_position as their sole authority.
    [[nodiscard]] Vector3 render_player_position() const noexcept;
    // Hers the same way: where her body is drawn between two ticks, and her
    // facing (the shorter way round). matron_state() stays her authority.
    [[nodiscard]] Vector3 render_matron_position() const noexcept;
    [[nodiscard]] double render_matron_yaw() const noexcept;

    // Mechanism-kit read back, for the presentation and the falsifiers. Body
    // indices run 0 .. kit_body_count() - 1 in build order.
    static constexpr std::uint32_t kKitNone = 0xFFFFFFFFU;
    [[nodiscard]] std::uint32_t kit_body_count() const noexcept;
    [[nodiscard]] std::uint64_t kit_body_entity(std::uint32_t body) const noexcept;
    [[nodiscard]] bool kit_body_dynamic(std::uint32_t body) const noexcept;
    // What the hands would take the body as (kit::CarryKind: 0 nothing it is
    // taken by, 1 a load, 2 a rope's shackle, 3 a handle).
    [[nodiscard]] std::uint8_t kit_body_carry_kind(std::uint32_t body) const noexcept;
    [[nodiscard]] bool kit_body_enabled(std::uint32_t body) const noexcept;
    [[nodiscard]] std::uint32_t kit_body_part_count(std::uint32_t body) const noexcept;
    [[nodiscard]] KitPart kit_body_part(std::uint32_t body, std::uint32_t part) const noexcept;
    [[nodiscard]] Vector3 kit_body_position(std::uint32_t body) const noexcept;
    [[nodiscard]] Vector3 kit_body_center_of_mass(std::uint32_t body) const noexcept;
    [[nodiscard]] Quaternion kit_body_rotation(std::uint32_t body) const noexcept;
    [[nodiscard]] Vector3 kit_body_velocity(std::uint32_t body) const noexcept;
    [[nodiscard]] double kit_body_mass(std::uint32_t body) const noexcept;
    [[nodiscard]] std::uint32_t kit_body_index(std::uint64_t entity) const noexcept;
    // Loose stock (crates, drums, beams, containers lying about): piece i is
    // kit entity kLooseStockFirstEntity + i. Its kind (LooseStock::Kind) and
    // its paint, for the presentation; an index past the end reads as 0, 0.
    static constexpr std::uint64_t kLooseStockFirstEntity = 5000;
    struct LooseStockPiece {
        std::uint8_t kind = 0;
        std::uint8_t variant = 0;
    };
    [[nodiscard]] std::uint32_t loose_stock_count() const noexcept;
    [[nodiscard]] LooseStockPiece loose_stock_piece(std::uint32_t index) const noexcept;
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
    [[nodiscard]] std::uint32_t kit_vent_count() const noexcept;
    [[nodiscard]] KitVent kit_vent(std::uint32_t vent) const noexcept;
    [[nodiscard]] WetState wet_state() const noexcept;
    [[nodiscard]] ShopState shop_state() const noexcept;
    [[nodiscard]] CraneState crane_state() const noexcept;
    [[nodiscard]] ServiceState service_state() const noexcept;
    [[nodiscard]] StackState stack_state() const noexcept;

    // The ground slingshot. Held inputs, as set_move_input: draw effort 0..1
    // (the stick pulled back), and the aim. One-shot commands: the Action
    // (enter the pouch, release a drawn shot, or retrieve the spent pouch)
    // and Drop (out of the pouch, or stop retrieving).
    [[nodiscard]] bool set_slingshot_input(double draw, double yaw, double elevation) noexcept;
    [[nodiscard]] bool request_slingshot_action() noexcept;
    [[nodiscard]] bool request_slingshot_drop() noexcept;
    [[nodiscard]] SlingshotSnapshot slingshot_state() const noexcept;
    // AS-012, the swing: Action straps in, kicks the trip, and at the top
    // unbuckles; Drop leaves the seat.
    [[nodiscard]] bool request_swing_action() noexcept;
    [[nodiscard]] bool request_swing_drop() noexcept;
    [[nodiscard]] SwingSnapshot swing_state() const noexcept;
    [[nodiscard]] SummitSnapshot summit_state() const noexcept;
    [[nodiscard]] MatronSnapshot matron_state() const noexcept;
    [[nodiscard]] BaySnapshot bay_state() const noexcept;
    // While a piece of loose stock is in the hands: the path a throw would
    // send it on now (from the hands, cut at the first thing it would hit;
    // empty otherwise).
    [[nodiscard]] std::vector<Vector3> throw_prediction() const;
    // Tests only (never bound to the bridge): the Matron's stage, with the
    // rage it takes, so a test can meet what each stage does to the player.
    void debug_set_matron_stage(std::uint8_t stage) noexcept;
    // Tests only (never bound to the bridge): her hands held off the player
    // for `seconds` (no grab, no shove), so a test can meet her body alone.
    void debug_hold_matron_hands(double seconds) noexcept;
    // Tests only (never bound to the bridge): her hands never take hold (as
    // in a grab's cooldown that does not end: a touch is her shove, not a
    // catch), so a test can meet her Turn as a pest's hands left it.
    void debug_hold_matron_grab(bool held) noexcept;
    // Tests only: her pace fixed (0 creep .. 3 hunt) with no hush; -1 free.
    void debug_fix_matron_pace(int pace) noexcept;
    // Tests only: her out of the locker and on her patrol loop.
    void debug_matron_patrol() noexcept;
    // Tests only (never bound to the bridge): her body put standing at `feet`,
    // awake and hunting; and one of her climbs begun from there, up or down:
    // the corner pipe (false if she found no hold), or the scanned climb
    // nearest her off her floor (false if there is none; her Climber steps in
    // under its first hold from her next tick, and a hold not there is a
    // failed climb, counted in climb_releases).
    void debug_place_matron(Vector3 feet) noexcept;
    [[nodiscard]] bool debug_matron_climb_way(bool pipe, bool down) noexcept;
    // Tests only: the corner pipe ruled out for her, as a pipe way that has
    // failed three times is (her own found ways tested alone).
    void debug_matron_rule_out_pipe() noexcept;
    // Tests only (never bound to the bridge): no edge of her floor counts for
    // the fatality's hang (Matron::rule_out_hang_for_test): its floor version.
    void debug_matron_rule_out_hang() noexcept;
    // Tests only (never bound to the bridge): every let-go of the hang
    // foreseen as a fall he would live (Matron::foresee_no_kill_for_test).
    void debug_matron_foresee_no_kill() noexcept;
    // Tests only: she hears the climber at `at` (feet): evidence with an age.
    void debug_matron_hear(Vector3 at) noexcept;
    // Tests only: her hands open on the climb or pipe she is on.
    void debug_matron_let_go() noexcept;
    // The Matron's climbing (matron_traversal.hpp): what the tower's steel
    // offers her, scanned from the real collision geometry on first use.
    [[nodiscard]] const traversal::AffordanceMap &matron_affordances();
    // Out of her locker (patrolling), then off on a journey to the region of
    // the scanned map whose top is nearest `top`: false if she has no way.
    // Tests: a journey to one region of her scanned map, by its id.
    [[nodiscard]] bool debug_matron_travel_region(int region) noexcept;
    bool debug_matron_travel(double top) noexcept;
    // Her feet's floor search between two points (traversal::grid_walk), for tests.
    [[nodiscard]] std::size_t debug_matron_grid_walk(double fx, double fy, double fz, double tx, double ty, double tz,
                                                     float margin, int max_nodes);
    struct MatronJourney {
        bool active = false;
        int goal = -1, region = -1, steps = 0, step = 0;
        double region_top = 0.0;
        std::uint32_t moves_done = 0, moves_failed = 0, replans = 0, arrivals = 0, yields = 0;
        std::uint32_t grips = 0, releases = 0, slips = 0, catches = 0, misses = 0;
        double force_n = 0.0, peak_force_n = 0.0;
        bool holding = false;
        std::uint8_t move = 0, last_failure = 0;
        double gravity_factor = 1.0;
    };
    [[nodiscard]] MatronJourney matron_journey() const noexcept;
    // Tests: her hold's choices fixed (spare: -1 her roll, 0 never, 1 always; a dangle's seconds, <0 rolled).
    void debug_matron_hold(int spare, double dangle_seconds) noexcept;
    // Tests: ArmPlay in a hold (-1 or 1 her rule: every hold while an arm is left; 0 never, the
    // hold alone); an arm already lost.
    void debug_matron_armplay(int armplay) noexcept;
    void debug_lose_arm(int side) noexcept;
    // Tests only (never bound to the bridge): an arm broken as a hard hit
    // breaks it (0 the left, 1 the right), out of use a while.
    void debug_break_arm(int side) noexcept;
    // Tests only (never bound to the bridge): the player's body caught on
    // steel where it is (a point at its middle fixed to the world) till the
    // load on it passes `max_n`: a body pinned on a rail's edge.
    void debug_snag_player(double max_n) noexcept;
    [[nodiscard]] bool debug_player_snagged() const noexcept;
    // Tests only (never bound to the bridge): the blow she takes next tagged
    // as the player's own (his punch's tag, Matron::player_blow_landed).
    void debug_matron_player_blow() noexcept;
    // Tests only (never bound to the bridge): kit body `body` put at `at` and
    // sent off at `velocity` (a throw without the arms, to meet her balance).
    void debug_launch_kit_body(std::uint32_t body, Vector3 at, Vector3 velocity) noexcept;
    // Tests only (never bound to the bridge): the player's body put at `at`
    // and sent off at `velocity` (a fall into a pool, met from its start).
    void debug_launch_player(Vector3 at, Vector3 velocity) noexcept;
    // Tests only (never bound to the bridge): the height of the first thing
    // straight under `at` within `depth`, or NaN where nothing is.
    [[nodiscard]] double debug_support_y(Vector3 at, double depth) const noexcept;
    // Tests only, never bound to the bridge. Optional placement holds the NPC
    // fixture between ticks; a null pointer only reads the last physical step.
    struct MatronFixtureSample {
        bool present = false;
        Vector3 position{};
        bool player_contact = false;
        bool standing_on_static = false;
        std::uint64_t floor_entity = 0;
        std::vector<std::uint64_t> static_contacts;
        std::vector<std::uint64_t> moving_contacts;
        bool player_support_static = false;
        bool traversal_support_static = false;
        double hang_arm_limit = 0.0;
        double hang_overload_impulse = 0.0;
    };
    [[nodiscard]] MatronFixtureSample debug_matron_fixture(const Vector3 *place = nullptr);
    // A world started after the beacon was lit before (the profile keeps it).
    void light_summit() noexcept;
    // The trapeze: Action catches the bar in the air when it is in reach;
    // Drop lets go; Crouch held pulls up on it.
    [[nodiscard]] bool request_trapeze_action() noexcept;
    [[nodiscard]] bool request_trapeze_drop() noexcept;
    [[nodiscard]] TrapezeSnapshot trapeze_state() const noexcept;
    // The yard's forklift: at its wheel, Action takes the controls (refused
    // elsewhere) and, driving, lets go of them. Driving, the stick drives and
    // steers and the forks' buttons, held, raise (1) and lower (-1) them.
    [[nodiscard]] bool request_forklift_action() noexcept;
    [[nodiscard]] bool set_forklift_lift(double lift) noexcept;
    // The forklift being driven, else the one at whose wheel the player
    // stands, else the yard's; or forklift `index` of forklift_count().
    [[nodiscard]] ForkliftSnapshot forklift_state() const noexcept;
    [[nodiscard]] std::uint32_t forklift_count() const noexcept;
    [[nodiscard]] ForkliftSnapshot forklift_state(std::uint32_t index) const noexcept;
    // The yard crane: by its cabinet, Take takes its remote (refused out of
    // reach); with the remote, the switch starts and stops working it
    // (refused where it cannot be worked: hands full, on a hold, in a seat).
    // Working it, the stick sends the hook and the hoist's buttons, held,
    // raise (1) and lower (-1) it. The device switch works the device on the
    // sling (the magnet: on, off; the spreader: LOCK, UNLOCK), refused unless
    // the crane is worked with a device on its sling.
    [[nodiscard]] bool request_yard_crane_take() noexcept;
    [[nodiscard]] bool request_yard_crane_operate() noexcept;
    [[nodiscard]] bool request_yard_crane_device() noexcept;
    [[nodiscard]] bool set_yard_crane_hoist(double hoist) noexcept;
    [[nodiscard]] YardCraneSnapshot yard_crane_state() const noexcept;
    // The last hits the tracked bodies took (src/sim/impacts.hpp), oldest
    // first: a reader keeps the last sequence it has seen.
    [[nodiscard]] std::vector<ImpactSnapshot> impacts() const;
    [[nodiscard]] std::uint64_t last_impact_sequence() const noexcept;
    // The ziplines (sim/zipline.hpp): each line's anchors, where its trolley
    // and its brake block are down it, and the trolley's bar while held.
    struct ZiplineLine {
        Vector3 top{};
        Vector3 bottom{};
        double length = 0.0;
        double brake_m = 0.0;     // the block's travel, its rest to the stop
        double trolley_m = 0.0;   // from the top anchor
        double block_m = 0.0;
        bool parked = false;
        Vector3 stand{};          // the stand's top, under the parked bar
        Vector3 ramp_foot{};      // the foot of the ramp up to it
    };
    [[nodiscard]] std::uint32_t zipline_count() const noexcept;
    [[nodiscard]] ZiplineLine zipline_line(std::uint32_t index) const noexcept;
    [[nodiscard]] Vector3 zipline_bar() const noexcept;
    // The corpses (sim/corpse.hpp), oldest first: each part's place and turn,
    // and whether the corpse has come to rest. Parts: Corpses::Part.
    struct CorpsePart {
        Vector3 position{};
        Quaternion rotation{};
    };
    [[nodiscard]] CorpsePart corpse_part(std::uint32_t corpse, std::uint32_t part) const noexcept;
    [[nodiscard]] bool corpse_resting(std::uint32_t corpse) const noexcept;
    // A vertical machine: from its deck Action sends it to the other end;
    // from a receiver, Action calls it there. Refused off every machine.
    [[nodiscard]] bool request_lift_action() noexcept;
    // The hands on a hand line, held (the pitman lift's haul): a held input,
    // like set_move_input.
    [[nodiscard]] bool set_lift_haul(bool held) noexcept;
    // The grappling hook: aimed along a world direction (the camera's), fired,
    // reeled in while held, let go; taken from its rack in reach.
    [[nodiscard]] bool set_hook_aim(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_hook_fire() noexcept;
    [[nodiscard]] bool set_hook_reel(bool held) noexcept;
    [[nodiscard]] bool request_hook_release() noexcept;
    [[nodiscard]] bool request_hook_take() noexcept;
    // The height of the first surface straight below (x, y, z) within reach
    // (at most 2 km), the player ignored; false when there is none. Read only.
    [[nodiscard]] bool surface_below(double x, double y, double z, double reach, double &hit_y) const noexcept;
    // What a look from (x, y, z) along (dx, dy, dz) meets first within reach
    // (at most 2 km): the entity of the body it meets, the point, the
    // surface's normal there and how far off it is. The player's own body,
    // the load in the hands and sensors are seen through; false when nothing
    // is met. Read only: the presentation's attention (what the eyes rest on).
    struct LookHit final {
        std::uint64_t entity = 0;
        Vector3 point{};
        Vector3 normal{};
        double distance = 0.0;
    };
    [[nodiscard]] bool look_target(double x, double y, double z, double dx, double dy, double dz, double reach,
                                   LookHit &hit) const noexcept;
    // A saved climb: the committed checkpoint (where the player last stood
    // firm, every machine with it) and the tick, as bytes for this build.
    // load_game restores the world from one as a death would, without
    // counting one; it refuses bytes from another build or world, changing
    // nothing.
    [[nodiscard]] std::vector<std::uint8_t> save_game() const;
    [[nodiscard]] bool load_game(const std::uint8_t *data, std::size_t size);
    // Live fabrication (03_EXECUTION/LIVE_FABRICATION/DESIGN.md): an object
    // made outside the simulation, handed in between ticks as data, like a
    // touch. `hull` is its hull file (sim/fabricated.hpp), untrusted: its
    // signature is the game layer's to check before it gets here, and here
    // every field is bounded again. Taken, it is a real body built from its
    // convex hulls with the mass its material gives, after every other body
    // in the world; its underside's middle at `at`, turned `yaw` radians about
    // +y; loose stock from then on (carried, thrown, saved). Returns its
    // entity, the next from fabricated::kEntityBase; 0 when it is refused,
    // `refusal` saying why, the world unchanged.
    [[nodiscard]] std::uint64_t add_fabricated(const std::uint8_t *hull, std::size_t size, Vector3 at, double yaw,
                                               FabricatedRefusal *refusal = nullptr);
    [[nodiscard]] std::uint32_t fabricated_count() const noexcept;
    // Index past the end: entity 0.
    [[nodiscard]] FabricatedObject fabricated_object(std::uint32_t index) const noexcept;
    // Its hull file, as the world holds it (and a save carries it).
    [[nodiscard]] std::vector<std::uint8_t> fabricated_data(std::uint32_t index) const;
    // Its hulls as triangles in its own frame, three points each: what a
    // presentation with no model for it draws (a plain crate of its hulls).
    [[nodiscard]] std::vector<Vector3> fabricated_hull_triangles(std::uint32_t index) const;
    // Tests only (never bound to the bridge): how many bodies in the physics
    // system have an entity in [first, limit).
    [[nodiscard]] std::uint32_t debug_bodies_with_entity_in(std::uint64_t first, std::uint64_t limit) const;
    [[nodiscard]] LiftSnapshot lift_state() const noexcept;
    // The shot as it would fly from the pouch now, cut where it would first
    // hit something; empty unless a rider is seated.
    [[nodiscard]] std::vector<Vector3> slingshot_prediction() const;
    // The faint route hint: what the hands could take ahead (each a lip's or
    // a hold's point), nearest first; empty off the ground or on a hold.
    [[nodiscard]] std::vector<Vector3> route_ahead() const;

// Native opt-in, intended for a planner calling between fixed steps.
    bool request_matron_climb_goal(Vector3 goal, Vector3 facing);
    void cancel_matron_climb_goal();
    int matron_climb_status() const;
    std::uint32_t matron_climb_queries() const;
    // Fixture setup only, never exposed to Godot.
    void launch_matron_for_test(Vector3 at);
    // Test setup: put her home (her locker, where she wakes) on the deck whose
    // top is deck_top, kept across a world rebuilt after she ends a run. The
    // game's home is the stack's deck 1 (11 m); tests written for her old
    // start put it back at 154. Returns false if she has no such level.
    bool place_matron_home(float deck_top);
    // Tests only: her body's gravity factor.
    [[nodiscard]] double debug_matron_gravity_factor() const noexcept;

private:
    class PhysicsWorld;

    void step_fixed() noexcept;

    std::unique_ptr<PhysicsWorld> physics_world_;
    // To start again whole (a run the Matron ended): how the world was made.
    InitialSpawn initial_spawn_ = InitialSpawn{};
    float matron_home_ = -1.0F;   // place_matron_home's deck top; -1: the world's own
    std::vector<TestExtraSolid> test_extra_solids_;
    std::uint64_t runs_ended_by_matron_ = 0;
    std::uint64_t tick_index_ = 0;
    double remainder_seconds_ = 0.0;
    double move_input_x_ = 0.0;
    double move_input_z_ = 0.0;
    double facing_x_ = 0.0;
    double facing_z_ = 0.0;
    double look_pitch_input_ = 0.0;
    bool jump_requested_ = false;
    bool strike_requested_ = false;
    bool traversal_requested_ = false;
    bool up_requested_ = false;
    bool release_requested_ = false;
    bool crouch_input_ = false;
    bool sprint_input_ = false;
    double room_loudness_ = 0.0;
    bool pick_up_requested_ = false;
    bool set_down_requested_ = false;
    bool rig_requested_ = false;
    bool parachute_toggle_requested_ = false;
    double sling_draw_ = 0.0;
    double sling_yaw_ = 0.0;
    double sling_elevation_ = 1.4311699866353502;
    bool sling_action_ = false;
    bool sling_drop_ = false;
    bool swing_action_ = false;
    bool swing_drop_ = false;
    bool trapeze_action_ = false;
    bool trapeze_drop_ = false;
    bool lift_action_ = false;
    bool lift_haul_ = false;
    bool forklift_action_ = false;
    double forklift_lift_ = 0.0;
    bool yard_crane_take_ = false;
    bool yard_crane_operate_ = false;
    bool yard_crane_device_ = false;
    double yard_crane_hoist_ = 0.0;
    double hook_aim_x_ = 0.0;
    double hook_aim_y_ = 0.0;
    double hook_aim_z_ = -1.0;
    bool hook_fire_ = false;
    bool hook_reel_ = false;
    bool hook_release_ = false;
    bool hook_take_ = false;
    Snapshot snapshot_{};
    Vector3 previous_player_position_{};
    Vector3 previous_matron_position_{};
    double previous_matron_yaw_ = 0.0;
    // Her body and facing at the start of this tick, for render_matron_*().
    void keep_matron_render_start() noexcept;
};

} // namespace scraperx::sim
