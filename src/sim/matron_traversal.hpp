#pragma once

// The Matron's climbing, as reusable moves on real steel (2026-10-21).
//
// Instead of a route authored per deck, she has a fixed set of physical
// moves and works out for herself where the tower lets her use them:
//
//   Moves (Climber): rung climb up and down, mantle/top-out, pole grip
//   (friction bounded: it slips under overload), jump to a hold (a bounded
//   leg drive, then a real catch or a real miss), ledge jump (a bounded
//   leg drive to a lip, a real hand catch on it, a hang, then its mantle),
//   hang-drop. Every
//   force she makes comes out of one shared budget (kTotalForceN); every
//   force on a hold goes back, equal and opposite, onto that hold's body at
//   the contact when the body is dynamic. Gravity is never turned off and
//   nothing ever sets her position: a move that fails lets go, and she falls.
//
//   Holds are a body, a sub-shape and an anchor in the body's own frame,
//   checked every tick: the body still there, not a sensor, the sub-shape
//   still the member it was, the anchor still on it, and still in her reach.
//
//   Scanner (AffordanceMap::scan): at tower load, it reads the real Jolt
//   collision geometry (never an authored list), tags what each move can
//   use (walkable tops with her headroom, rungs and ladders, poles by their
//   section, lips) and joins them into a directed graph of regions and
//   edges using her real limits. Up and down are separate edges, each with
//   its own checks (her full capsule swept where she would stand and where
//   she would pass a lip).
//
//   Planner (AffordanceMap::plan): a directed search to a region.
//
// Only simulation state decides anything: no clock, no randomness.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <string>
#include <vector>

namespace scraperx::sim::traversal {

// Her body and her limits. All CHOSEN for a 2.1 m, 220 kg body.
// The player's jump-and-hang reach over his feet, about 4.14 m: capsule
// centre over the feet (kPlayerHalfHeight 0.9 + kPlayerRadius 0.35), a
// standing jump's rise (kJumpSpeed 5.5 under 9.81 m/s^2: 1.54) and the hang
// band's top over the centre (kHangMaximumRiseAboveCentre 1.35). Written out:
// Limits is used in constant expressions, and physics_world_tuning.hpp
// cannot be included here (it includes this header). The test
// matron-climb's escape-margin check recomputes it from those constants.
inline constexpr float kPlayerJumpHangReachM = 4.14F;
// What the player keeps over her planned jump-catch.
inline constexpr float kEscapeMarginM = 0.5F;

struct Limits {
    float radius = 0.30F;
    float half_height = 0.75F;   // the capsule's cylinder half; center is radius + half above the feet
    float mass_kg = 220.0F;
    // Hands overhead, standing: feet + 2.45 m.
    float hand_above_center = 1.40F;
    // Hanging at full stretch: the hold this far over her center.
    float hang_below_hold = 1.30F;
    // Holds she can take: up to this far out from her middle's line, and
    // from this far under her center to hand_above_center over it.
    float reach_out = 0.95F;
    float reach_low = 0.80F;   // pressing down on a lip, hands at her hips
    // The one budget every simultaneous actuator (hands, feet, squeeze)
    // draws from: about 3.7 times her weight. Left at 8 kN on purpose.
    float total_force_n = 8000.0F;
    // 8 kN still applied at the end of a 6 m/s push is nearly 50 kW. At 22 kW
    // the leg had to let go while the stroke still had rise in it, and the
    // earned jump stayed 0.74 m. 44 kW is the shared cap that keeps 8 kN on
    // the floor long enough for the stroke below to finish.
    float power_w = 44000.0F;   // actuator power: force along her own velocity, all actuators together
    float foot_force_n = 220.0F * 9.0F;   // her feet on a floor: kFootAccel
    // During push-off the planted foot can extend this far below the fixed
    // capsule. 0.40 m at 8 kN tops out near 4.6 m/s before power even binds
    // (about 3.5 ft). A 2.1 m body can spend 0.85 m and still be a body.
    float leg_travel_m = 0.85F;
    // Measured at 90 Hz from 8 kN, 44 kW and the 0.85 m stroke. A take-off
    // speed the push earns. Never an impulse added at toe-off.
    float launch_dv_mps = 6.30F;
    // Her own jump comes back near 7.2 m/s. A catch that cannot stop that
    // return rips her off a jump she was allowed to make.
    float catch_arrest_mps = 7.60F;
    float catch_radius_m = 0.22F;
    // A gap in the rungs: from a bent arm she reaches up to hand_above_center
    // over her center; past that she throws for the next rung (a dyno), her
    // arms' pull from a low hang giving at most this much more rise.
    float dyno_rise_m = 0.45F;
    // Pole grip: squeeze (normal force) and the steel's friction on her.
    float pole_squeeze_n = 4600.0F;
    float pole_mu_static = 0.60F;
    float pole_mu_kinetic = 0.45F;
    // Mantle: a lip up to this over her feet; a hang-drop: down to this
    // under the lip she hangs from.
    float mantle_max_m = 2.30F;
    float step_max_m = 0.36F;
    float drop_max_m = 4.60F;     // lip to landing; she hangs first (her feet 2.4 m under it)
    // A hold this high over her feet she can jump for: kept kEscapeMarginM
    // under the player's own jump-and-hang reach (Cory, 2026-10-27 15:50 CT),
    // so a hold he can reach from a jump is one she cannot follow him to.
    float jump_hold_max_m = kPlayerJumpHangReachM - kEscapeMarginM;
    float climb_mps = 0.6F;
    float mantle_mps = 1.4F;
    float pole_up_mps = 0.5F;
    float pole_down_mps = 1.6F;
    [[nodiscard]] float center_above_feet() const noexcept { return radius + half_height; }
    [[nodiscard]] float hand_reach_standing() const noexcept { return center_above_feet() + hand_above_center; }
};

// A hold: a body, the member of it, and where on the member (body frame).
// One tick's actuator allowance: total force and total power (the force's
// component along her velocity, which is what does work). Every AddForce of
// hers goes through take(); contact impulses are Jolt's and not counted.
struct Budget {
    float left = 0.0F;          // newtons still available this tick
    float used = 0.0F;
    float power_left = 0.0F;    // watts still available this tick
    float power_used = 0.0F;
    JPH::Vec3 velocity = JPH::Vec3::sZero();   // hers relative to the support (hold or floor) at the tick's start
    float brake_used = 0.0F;    // watts of braking (negative work) granted: capped by force alone, never by power
    JPH::Vec3 take(JPH::Vec3 want) noexcept;
};

struct Hold {
    JPH::BodyID body;
    JPH::SubShapeID sub;
    JPH::Vec3 local = JPH::Vec3::sZero();
    JPH::AABox member_local;   // the member's bounds in the body's frame when taken
    [[nodiscard]] bool valid() const noexcept { return !body.IsInvalid(); }
};

enum class MoveKind : std::uint8_t {
    Walk = 0,
    RungUp = 1,
    RungDown = 2,
    Mantle = 3,     // a lip from a floor
    StepDown = 4,   // off an edge onto what is under it (a short drop)
    PoleUp = 5,
    PoleDown = 6,
    JumpCatch = 7,  // a jump to a bar or the bottom of a ladder; then up it
    HangDrop = 8,
    GapJump = 9,    // a supported run and short leg drive, then ballistic flight to a lower floor
    LedgeJump = 10, // a jump to a lip; then a hang and its mantle onto the floor over it
};
[[nodiscard]] const char *move_name(MoveKind kind) noexcept;

enum class Failure : std::uint8_t {
    None = 0,
    NoHold,         // nothing real to take where the move needs it
    LostHold,       // a hold went (removed, a sensor, a different member, out of reach)
    MissingRung,    // no next rung within reach
    BlockedSweep,   // her capsule does not fit where she would stand or pass
    Slipped,        // grip overloaded past recovery
    Missed,         // the jump did not catch
    RippedOff,      // caught, too fast to hold
    OutOfRange,     // the launch it needs is more than her legs have
    Stalled,        // no progress
    NotSupported,   // landed or ended without real support at the goal
};
[[nodiscard]] const char *failure_name(Failure failure) noexcept;

// What a move needs to know, all of it from the scanner (or a test).
struct MoveSpec {
    MoveKind kind = MoveKind::Walk;
    JPH::Vec3 line = JPH::Vec3::sZero();   // a ladder/pole axis; a gap jump's take-off point (feet)
    JPH::Vec3 out = JPH::Vec3::sAxisZ();   // horizontal: from the steel toward her side
    float bottom_y = 0.0F;                 // lowest rung / pole foot / lip for drops
    float top_y = 0.0F;                    // highest rung / pole head / lip
    JPH::Vec3 start = JPH::Vec3::sZero();  // feet, where the move begins
    JPH::Vec3 goal = JPH::Vec3::sZero();   // feet, on the support it ends on
    float goal_top = 0.0F;
    float bar_half = 0.3F;                 // a rung's half length (probe width)
};

// One tick's account of what she did with her hands and feet.
struct ClimbView {
    bool active = false;
    bool holding = false;
    bool airborne = false;
    bool sliding = false;
    MoveKind kind = MoveKind::Walk;
    int phase = 0;
    float force_n = 0.0F;          // her total actuator force this tick
    float peak_force_n = 0.0F;
    float power_w = 0.0F;
    float peak_power_w = 0.0F;
    float peak_brake_w = 0.0F;     // braking against the support: force-capped only
    float support_reaction_n = 0.0F;   // pushed onto a dynamic hold's body this tick
    std::uint32_t grips = 0;
    std::uint32_t releases = 0;
    std::uint32_t slips = 0;
    std::uint32_t catches = 0;
    std::uint32_t misses = 0;
    std::uint32_t launches = 0;
    bool leg_driving = false;
    float leg_travel_m = 0.0F;    // extension of the planted leg during this push
    Failure last_failure = Failure::None;
    JPH::RVec3 hand = JPH::RVec3::sZero();
    // This tick a top-out has her up: her middle over the floor it was going
    // for, her soles within the clearance it pulled them to over the lip. Only
    // her weight is left to come onto her feet (its own Done waits for that).
    bool topped = false;
};

// Bodies her probes never see: herself, the player, anything else named.
struct Ignore {
    JPH::BodyID self;
    JPH::BodyID player;
    // Her body's object layer: probes see only what her body collides with
    // (-1: everything).
    int layer = -1;
};

// The move executor on one body. step() is called once a tick before the
// physics update; it only adds forces.
class Climber final {
public:
    enum class Status : std::uint8_t { Idle = 0, Running = 1, Done = 2, Failed = 3 };

    Climber(JPH::PhysicsSystem &physics, JPH::BodyID body, Ignore ignore, Limits limits = {});

    void begin(const MoveSpec &spec);
    void cancel();   // lets go of everything (a fall if she was on steel)
    Status step(float dt);
    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] Failure failure() const noexcept { return failure_; }
    [[nodiscard]] const ClimbView &view() const noexcept { return view_; }
    [[nodiscard]] const Limits &limits() const noexcept { return limits_; }
    [[nodiscard]] int phase() const noexcept { return phase_; }
    // Her feet alone toward a point on the floor she stands on, out of the
    // one budget (between moves: a journey's walking). Returns the force used.
    float walk_between(float dt, JPH::Vec3 feet_goal, float speed, float may_drop = 0.0F);
    // Standing still: balanced on a narrow floor (a board, a ledge), nothing
    // else. step() does this itself once a move is done.
    float stand(float dt);
    [[nodiscard]] const MoveSpec &spec() const noexcept { return spec_; }
    [[nodiscard]] bool on_steel() const noexcept { return hold_.valid(); }
    // The hold her hand has now (its body and member; invalid when none).
    [[nodiscard]] const Hold &hold() const noexcept { return hold_; }
    // Her velocity against what she pushes on (the hold at its anchor, else
    // the floor under her): the velocity her power budget is charged on.
    [[nodiscard]] JPH::Vec3 relative_velocity() const;
    void set_ignore(Ignore ignore) noexcept { ignore_ = ignore; }
    // This tick's climbing rate on rungs (her pace, a Turn's jerks); <0: her own.
    void set_rate(const float mps) noexcept { rate_override_ = mps; }
    // A ladder's rung line, felt for from `from` along `dir` at heights y0..y1.
    [[nodiscard]] bool feel_for_rungs(JPH::Vec3 from, JPH::Vec3 dir, float y0, float y1, JPH::Vec3 &line) const;
    // What she stands on now (a ray under her), or an invalid id.
    // The actual sole contact can lie to one side of her middle on a narrow
    // floor. Callers planning from support must use that point, not invent a
    // floor under the body's centre.
    [[nodiscard]] bool supported(float *top = nullptr, JPH::BodyID *by = nullptr, JPH::RVec3 *point = nullptr) const;
    // Tests and the planner: does her capsule fit, standing, feet at `feet`?
    [[nodiscard]] bool capsule_free(JPH::RVec3 center) const;
    // Hold validation, exposed for tests.
    [[nodiscard]] bool revalidate(Hold &hold, JPH::RVec3 &world) const;

private:
    void fail(Failure failure);
    void finish();
    bool take_hold(JPH::RVec3 near, float radius, bool bar_only, Hold &out, JPH::RVec3 *where = nullptr) const;
    bool find_rung(float from_y, float to_y, bool upward, Hold &out, JPH::RVec3 &where) const;
    bool find_lip(JPH::Vec3 toward, float near_y, Hold &out, JPH::RVec3 &where) const;
    void drive(float dt, JPH::Vec3 target_centre, JPH::Vec3 target_velocity, float stiffness, Budget &budget,
               bool carry_weight, JPH::RVec3 at_hold);
    // may_drop: how far below her feet a floor ahead still counts (a step down she means).
    void walk(float dt, JPH::Vec3 feet_goal, float speed, Budget &budget, float may_drop = 0.0F);
    [[nodiscard]] bool floor_under(JPH::RVec3 c, JPH::Vec3 off) const;
    [[nodiscard]] bool narrow_floor_lean(JPH::RVec3 c, JPH::Vec3 &to_floor) const;
    [[nodiscard]] JPH::RVec3 center() const;
    [[nodiscard]] JPH::Vec3 velocity() const;
    void push_reaction(JPH::Vec3 force, JPH::RVec3 at);
    bool plant_leg(float target_hand_y, bool required = true);
    [[nodiscard]] bool planted_leg_contact(JPH::RVec3 &at) const;
    bool drive_leg(float dt, Budget &budget);   // true at toe-off; no airborne thrust
    void release_leg();

    Status step_rung_up(float dt, Budget &budget);
    Status step_rung_down(float dt, Budget &budget);
    Status step_mantle(float dt, Budget &budget);
    Status step_pole(float dt, Budget &budget, bool up);
    Status step_jump(float dt, Budget &budget);
    Status step_ledge_jump(float dt, Budget &budget);
    Status step_gap(float dt, Budget &budget);
    Status step_drop(float dt, Budget &budget, bool hang);
    Status step_walk(float dt, Budget &budget);
    Status begin_mantle_from_hang();

    JPH::PhysicsSystem &physics_;
    JPH::BodyID body_;
    Ignore ignore_;
    Limits limits_;
    MoveSpec spec_;
    Status status_ = Status::Idle;
    Failure failure_ = Failure::None;
    int phase_ = 0;
    float phase_s_ = 0.0F;
    float stall_s_ = 0.0F;
    float best_progress_ = 0.0F;
    float target_y_ = 0.0F;
    Hold hold_;
    JPH::RVec3 hold_world_ = JPH::RVec3::sZero();
    JPH::Vec3 hang_off_ = JPH::Vec3::sZero();   // a hang drop's hang point, from its hold
    Hold lip_;
    JPH::RVec3 lip_world_ = JPH::RVec3::sZero();
    bool sliding_ = false;
    bool mantle_after_ = false;
    bool approach_near_ = false;
    // A dyno across a gap in the rungs: 0 off, 1 sinking to a low hang, 2
    // pulling, 3 in the air, 4 driving from the floor; the target and tries.
    int dyno_ = 0;
    Hold dyno_hold_;
    JPH::RVec3 dyno_world_ = JPH::RVec3::sZero();
    int dyno_tries_ = 0;
    Status step_dyno(float dt, Budget &budget);
    Hold foot_;
    JPH::RVec3 foot_world_ = JPH::RVec3::sZero();
    float leg_start_m_ = 0.0F;
    float leg_target_y_ = 0.0F;
    int leg_push_ticks_ = 0;
    float gap_speed_mps_ = 0.0F;
    float rate_override_ = -1.0F;
    [[nodiscard]] float rate_mps() const noexcept { return rate_override_ >= 0.0F ? rate_override_ : limits_.climb_mps; }
    ClimbView view_;
};

// Her feet's way over floor between two points: a grid search over the real
// collision world (a floor within a step of the last, her capsule free on
// it), A* within `margin` of the box spanning them. Empty if none.
std::vector<JPH::Vec3> grid_walk(const JPH::PhysicsSystem &physics, const Ignore &ignore, const Limits &limits,
                                 JPH::Vec3 from, JPH::Vec3 to, float margin = 8.0F, float cell = 0.4F,
                                 int max_nodes = 20000);

// ---------------------------------------------------------------------------
// The scanner: what the tower's steel offers her.

struct Patch {   // a walkable top (one leaf of a body)
    JPH::BodyID body;
    JPH::SubShapeID sub;
    float top = 0.0F;
    float min_x = 0.0F, max_x = 0.0F, min_z = 0.0F, max_z = 0.0F;
    int region = -1;
    bool narrow = false;   // under her diameter across (a board: her axis over it)
    int split = -1, split_of = -1;   // a piece of a top split by what stands on it: its floor, and the top
};
struct Ladder {   // three or more bars stacked on one line
    float x = 0.0F, z = 0.0F;
    bool along_x = true;   // the rungs' long axis
    float half = 0.0F;
    float bottom = 0.0F, top = 0.0F, pitch = 0.0F;
    int rungs = 0;
    std::uint64_t entity = 0;
};
struct Pole {     // a vertical member she can wrap (section up to kPoleMaxSection)
    float x = 0.0F, z = 0.0F;
    float section = 0.0F;
    float bottom = 0.0F, top = 0.0F;
};
struct Region {
    int id = -1;
    float top = 0.0F;   // area-weighted
    float min_x = 0.0F, max_x = 0.0F, min_y = 0.0F, max_y = 0.0F, min_z = 0.0F, max_z = 0.0F;
    float area = 0.0F;
    std::vector<int> patches;
};
struct Edge {
    int from = -1, to = -1;
    MoveKind kind = MoveKind::Walk;
    MoveSpec spec;
    float cost = 0.0F;
    bool usable = true;          // false: geometry found but a check failed (reason)
    std::string reason;          // why not, or the facts that decided it
};

struct ScanStats {
    double seconds = 0.0;
    std::uint32_t bodies = 0, leaves = 0, patches = 0, usable_patches = 0, bars = 0, ladders = 0, poles = 0;
    std::uint32_t regions = 0, edges = 0, usable_edges = 0, capsule_queries = 0, split_patches = 0;
};

class AffordanceMap final {
public:
    // Reads every static (and kinematic) non-sensor body overlapping bounds.
    void scan(const JPH::PhysicsSystem &physics, const JPH::AABox &bounds, const Ignore &ignore,
              const Limits &limits = {});
    [[nodiscard]] const std::vector<Region> &regions() const noexcept { return regions_; }
    [[nodiscard]] const std::vector<Edge> &edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<Ladder> &ladders() const noexcept { return ladders_; }
    [[nodiscard]] const std::vector<Pole> &poles() const noexcept { return poles_; }
    [[nodiscard]] const std::vector<Patch> &patches() const noexcept { return patches_; }
    [[nodiscard]] const ScanStats &stats() const noexcept { return stats_; }
    // The region whose walkable top is under these feet (within 0.3 m), or -1.
    [[nodiscard]] int region_at(JPH::Vec3 feet) const;
    // The largest region whose top is within tol of y, or -1.
    [[nodiscard]] int region_near_top(float y, float tol = 0.2F) const;
    // Directed search: the edges, in order, from one region to another
    // (usable edges only). Empty if there is no way.
    // avoid: edges not to take (ones this journey already found it cannot start).
    [[nodiscard]] std::vector<int> plan(int from, int to, const std::vector<int> &avoid = {}) const;
    // A walk inside one region: patch-to-patch waypoints (feet).
    [[nodiscard]] std::vector<JPH::Vec3> walk_path(JPH::Vec3 from, JPH::Vec3 to) const;
    [[nodiscard]] int patch_at(JPH::Vec3 feet, float tol) const;

private:
    void split_patches(const JPH::PhysicsSystem &physics, const Ignore &ignore, const Limits &limits);
    void build_regions(const JPH::PhysicsSystem &physics, const Ignore &ignore, const Limits &limits);
    void build_edges(const JPH::PhysicsSystem &physics, const Ignore &ignore, const Limits &limits);
    [[nodiscard]] bool capsule_free(const JPH::PhysicsSystem &physics, const Ignore &ignore, const Limits &limits,
                                    JPH::Vec3 feet);
    void add_edge(Edge edge);

    std::vector<Patch> patches_;
    std::vector<Region> regions_;
    std::vector<Edge> edges_;
    std::vector<Ladder> ladders_;
    std::vector<Pole> poles_;
    std::vector<std::vector<int>> patch_links_;
    ScanStats stats_;
};

} // namespace scraperx::sim::traversal
