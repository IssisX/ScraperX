#pragma once

// The mechanism kit (03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md §4): the
// parts every lift stage of the mechanism ascent is built from. Bodies are
// declared here with the boxes they are made of, so the presentation draws
// exactly what collides. Ropes are real tension-only PulleyConstraints whose
// loose end is a shackle body the player carries and hooks onto an anchor.
// Guides are SliderConstraints whose governor is a velocity motor that may
// only ever oppose motion. Catches hold a body while a pin is seated; a trip
// line turns the pin's lever. Nothing here reads a flag to decide motion:
// every link is a body, a constraint or a force.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <vector>

namespace scraperx::sim::kit {

// Entity ids. Band structure is static and takes ids from kStaticEntityBase;
// bodies that move take ids from kDynamicEntityBase, so the support rule can
// tell a machine from the floor by id alone.
constexpr std::uint64_t kStaticEntityBase = 1000;
constexpr std::uint64_t kDynamicEntityBase = 2000;
constexpr std::uint64_t kEntityLimit = 3000;

[[nodiscard]] constexpr bool is_kit_entity(const std::uint64_t entity) noexcept {
    return entity >= kStaticEntityBase && entity < kEntityLimit;
}

[[nodiscard]] constexpr bool is_dynamic_entity(const std::uint64_t entity) noexcept {
    return entity >= kDynamicEntityBase && entity < kEntityLimit;
}

// What a surface is made of, for the presentation's palette. Not physics.
enum class Material : std::uint8_t {
    Steel = 0,
    Rust = 1,
    Timber = 2,
    Concrete = 3,
    Hazard = 4,
    Galvanised = 5,
    Rubble = 6,
    Yellow = 7,
};

// One box of a body, in the body's own frame. A body is the union of its
// parts: the collision shape and the drawn shape are the same list.
struct Part final {
    JPH::Vec3 half = JPH::Vec3::sReplicate(0.5F);
    JPH::Vec3 offset = JPH::Vec3::sZero();
    JPH::Quat rotation = JPH::Quat::sIdentity();
    Material material = Material::Steel;
};

// What a pick-up of this body is, for the prompt: a load, a rope's shackle,
// or a handle on a trip line.
enum class CarryKind : std::uint8_t {
    None = 0,
    Load = 1,
    Shackle = 2,
    Handle = 3,
};

constexpr std::uint32_t kNone = 0xFFFFFFFFU;

// An index into one of the kit's tables. Each table has its own index type,
// so an index from one table cannot be handed to another table's reader.
// Readers answer a neutral value for an index their table does not hold.
template <typename Tag>
struct Index final {
    std::uint32_t value = kNone;
    [[nodiscard]] constexpr bool valid() const noexcept { return value != kNone; }
    [[nodiscard]] friend constexpr bool operator==(const Index a, const Index b) noexcept {
        return a.value == b.value;
    }
    [[nodiscard]] friend constexpr bool operator!=(const Index a, const Index b) noexcept {
        return a.value != b.value;
    }
};
using BodyIndex = Index<struct BodyTag>;
using AnchorIndex = Index<struct AnchorTag>;
using GuideIndex = Index<struct GuideTag>;
using RopeIndex = Index<struct RopeTag>;
using LeverIndex = Index<struct LeverTag>;
using CatchIndex = Index<struct CatchTag>;
using LineIndex = Index<struct LineTag>;
using SlipIndex = Index<struct SlipTag>;
using BinIndex = Index<struct BinTag>;
using PoolIndex = Index<struct PoolTag>;
using PipeIndex = Index<struct PipeTag>;
using CellIndex = Index<struct CellTag>;

class Kit final {
public:
    Kit(JPH::PhysicsSystem &system, JPH::ObjectLayer static_layer, JPH::ObjectLayer moving_layer);
    ~Kit();

    Kit(const Kit &) = delete;
    Kit &operator=(const Kit &) = delete;

    // ---- building -------------------------------------------------------
    // mass_kg == 0 makes a static body.
    BodyIndex add_body(std::uint64_t entity, const std::vector<Part> &parts,
                       JPH::RVec3 position, JPH::Quat rotation, float mass_kg, float friction);
    void set_carry(BodyIndex body, CarryKind kind, JPH::Vec3 handle_local);
    // Velocity damping per second, for light bodies on lines that air and
    // the line's own stiffness would settle: a handle bumped on its line
    // stops swinging in a second or two.
    void set_damping(BodyIndex body, float linear, float angular);
    AnchorIndex add_anchor(BodyIndex body, JPH::Vec3 local, float reach);

    // A straight guide along a world axis through the body's present
    // position. Travel is measured from there. governor_speed == 0 means no
    // governor. The governor brakes toward a target speed that falls to
    // zero at each end of travel at level_accel, so a governed body comes
    // to its stop gently instead of on the limit.
    GuideIndex add_guide(BodyIndex body, JPH::Vec3 axis, float min_travel, float max_travel,
                         float governor_speed, float governor_force, float level_accel);

    // Safety dogs on a guide: a pawl drops into a rack tooth every `pitch`
    // of travel, so the body climbs its guide but never falls back more than
    // one tooth. Teeth are counted down from max_travel, so the top of travel
    // is a tooth.
    void set_dogs(GuideIndex guide, float pitch);

    // A rail joint missing from a guide (declared, AS-009): the body's
    // rollers cannot pass the gap, so its travel stops at gap_travel until
    // the joint body lies in its seat -- within `tolerance` of `seat`, its x
    // axis within `angle` of seat_axis -- as a pipe's spool (AS-007).
    void set_rail_gap(GuideIndex guide, float gap_travel, BodyIndex joint, JPH::RVec3 seat,
                      JPH::Vec3 seat_axis, float tolerance, float angle);

    // A dog clutch between a winch's drums (declared, AS-009): the rope turns
    // nothing until the lever is thrown past engage_angle; then it takes up
    // its length as it is at that moment and holds from then on.
    void add_clutch(RopeIndex rope, LeverIndex lever, float engage_angle);

    // A rope from body1's point over fixed1 ... fixed2 to its end:
    // |p1 - fixed1| + ratio |end - fixed2| <= max_length, tension only. A
    // ratio of 0.5 is a two-part purchase on body1's side: body1 is pulled
    // twice as hard as the end and moves half as far. The end starts on
    // end_body at end_point: a shackle body (loose end) or any body (an end
    // already made fast). rating_newtons == 0: it never parts.
    RopeIndex add_rope(BodyIndex body1, JPH::Vec3 point1, JPH::RVec3 fixed1, BodyIndex end_body,
                       JPH::Vec3 end_point, JPH::RVec3 fixed2, float ratio, float max_length,
                       float rating_newtons);

    // A lever on a hinge fixed to the world, between stops at min_angle and
    // max_angle; the angle is 0 as built. Nothing but its own weight returns
    // it: a counterweighted lever rests on a stop, and a pull turns it.
    LeverIndex add_lever(BodyIndex body, JPH::RVec3 pivot, JPH::Vec3 axis, JPH::Vec3 normal,
                         float min_angle, float max_angle);

    // A catch holding body fast to the world while its pin is seated. The
    // pin leaves its hole when lever turns past release_angle. relatch:
    // a spring catch that seats itself again when the body comes back to
    // within seat_tolerance of where it was caught, slow, with the lever
    // returned.
    CatchIndex add_catch(BodyIndex body, LeverIndex lever, float release_angle,
                         float seat_tolerance, bool relatch);

    // A pin catch (AS-008): holds body fast to the world while `pin` sits
    // within pin_tolerance of where it was built -- a pin in its hole, a
    // chock under a wheel. Carry or pull the pin out and the body goes. It
    // relatches when the pin is back in and the body is back on its seat.
    CatchIndex add_pin_catch(BodyIndex body, BodyIndex pin, float pin_tolerance,
                             float seat_tolerance);

    // A slip hook (a pelican hook) holding a rope's body1 end: shut while
    // lever is below release_angle; past it, the hook opens and the rope runs
    // out for good, as if parted.
    SlipIndex add_slip(RopeIndex rope, LeverIndex lever, float release_angle);

    // A bin holds rubble (declared granular model, AS-006 Stage C): its
    // contents in kg, added to its body's mass when the body is dynamic
    // (MotionProperties::ScaleToMass: the rubble spreads like the body, so
    // inertia scales with mass). Its mouth is a point on its underside.
    // While its gate lever is turned past gate_open_angle and the lever's
    // pivot is within gate_reach of the mouth, rubble leaves the mouth at
    // flow_rate and falls straight down to the first bin or the first static
    // surface under it, passing any other moving body: a bin receives it up
    // to its capacity; a static surface takes it as a spill, onto a pile
    // there. The stream carries no momentum into what receives it, and a
    // stream that finds nothing within kStreamReach does not run. Rubble in
    // a bin lies on the bin's floor, its first part. gate invalid: no outlet.
    BinIndex add_bin(BodyIndex body, float contents_kg, float capacity_kg, JPH::Vec3 mouth_local,
                     LeverIndex gate, float gate_open_angle, float gate_reach, float flow_rate);

    // A trip line: a light rope from a lever's arm point over sheave1, along
    // to sheave2 and down to a handle hanging there, as laid. Pulling the
    // handle away from sheave2 turns the lever.
    LineIndex add_trip_line(BodyIndex lever_body, JPH::Vec3 lever_point, BodyIndex handle_body,
                            JPH::Vec3 handle_point, JPH::RVec3 sheave1, JPH::RVec3 sheave2);

    // As found: a catch standing open, and a body placed where it was left
    // (a door hooked back). Build time only.
    void open_catch(CatchIndex catch_index);
    void place_body(BodyIndex body, JPH::RVec3 position, JPH::Quat rotation);

    // Water in this bin (AS-007): what it spills drains away instead of
    // lying in a pile.
    void set_bin_water(BinIndex bin);

    // A hydraulic line (declared model, AS-007): the rope becomes a push-only
    // strut, |p1 - fixed1| + ratio |end - fixed2| >= what the line holds.
    // Fixed points sit under the rams. It is connected only while its end is
    // hooked (a coupled hose); coupling takes whatever fluid it holds then.
    void set_strut(RopeIndex rope);

    // ---- water (declared model, AS-007) ----------------------------------
    // A pool is a sealed box of water between min and max (floor and top in
    // y). Its level follows from its water and the displacement of the floats
    // in it. A pipe into a full pool stops; water a float pushes over its top
    // leaves it and drains away.
    PoolIndex add_pool(JPH::Vec3 min_corner, JPH::Vec3 max_corner, float water_kg);
    // A float: the body's box of half_x by half_z, from bottom_local (body
    // y) up height, feels rho g A d up at its centre of mass, d its depth
    // under the pool's level, and water drag on its vertical speed.
    void add_float(PoolIndex pool, BodyIndex body, float half_x, float half_z,
                   float bottom_local, float height);
    // A pipe from a pool's outlet at from_y to another pool's inlet at to_y
    // (both ways, from the higher head), or, with to invalid, to a spout at
    // spout that pours down into the first bin under it or spills.
    // Q = 0.6 area f sqrt(2 g dh), f the valve's opening between its shut
    // and open angles (no valve: open).
    PipeIndex add_pipe(PoolIndex from, float from_y, PoolIndex to, float to_y, JPH::RVec3 spout,
                       float area, LeverIndex valve, float shut_angle, float open_angle);
    // The pipe is whole only while spool's centre sits within tolerance of
    // seat and its local x axis within angle_tolerance of axis (either way).
    void set_pipe_spool(PipeIndex pipe, BodyIndex spool, JPH::RVec3 seat, JPH::Vec3 axis,
                        float tolerance, float angle_tolerance);
    // A charge line: while valve is past open_angle and the pool holds water
    // over from_y, its head pushes body up with rho g (level - base_y) area,
    // and the water that takes leaves the pool.
    void add_charge(PoolIndex pool, float from_y, BodyIndex body, float area, float base_y,
                    LeverIndex valve, float open_angle);

    // ---- air (declared model, AS-007) ------------------------------------
    // A sealed cell of air at atmospheric pressure as built, isothermal.
    CellIndex add_cell(float volume_m3);
    // A piston with the cell under it: the cell grows by area for each metre
    // the body rises, and the body feels (P - P_atm) area up.
    void add_piston(CellIndex cell, BodyIndex body, float area);
    // A throttle between two cells: 0.6 area sqrt(2 rho dP), linear under
    // 50 Pa.
    void add_throttle(CellIndex a, CellIndex b, float area);
    // A door in the cell's wall: it leaks area times its opening (its angle
    // over full_angle), and the cell presses it outward along normal.
    void add_door(CellIndex cell, LeverIndex door, float area, float full_angle,
                  JPH::Vec3 normal);
    // A vacuum breaker: admits air through area below P_atm - 100 Pa.
    void add_breaker(CellIndex cell, float area);
    // A bleed: a small opening to the air, always open, either way.
    void add_bleed(CellIndex cell, float area);

    // ---- stepping -------------------------------------------------------
    void pre_step(float delta_seconds);
    void post_step(float delta_seconds);

    // ---- player verbs ---------------------------------------------------
    struct CarryCandidate final {
        JPH::BodyID id;
        std::uint64_t entity = 0;
        JPH::Vec3 handle = JPH::Vec3::sZero();
        CarryKind kind = CarryKind::None;
    };
    // Every body the player could pick up: enabled, carryable.
    void carry_candidates(std::vector<CarryCandidate> &out) const;
    [[nodiscard]] JPH::Vec3 carry_handle(std::uint64_t entity) const noexcept;
    [[nodiscard]] CarryKind carry_kind(std::uint64_t entity) const noexcept;

    // The anchor a carried shackle would hook onto from where it is now,
    // invalid for none: within the anchor's reach, and not so far that the
    // rope would have to stretch.
    [[nodiscard]] AnchorIndex hook_target(std::uint64_t shackle_entity) const noexcept;
    // Moves the rope's end from the shackle to the anchor. The shackle leaves
    // the world; the rope keeps its length.
    bool hook(std::uint64_t shackle_entity, AnchorIndex anchor);
    // A hooked end within reach of hand that carries no load, invalid for
    // none.
    [[nodiscard]] RopeIndex unhook_target(JPH::RVec3 hand) const noexcept;
    // Moves the rope's end back onto its shackle, placed at the anchor.
    // Returns the shackle's entity, 0 when the rope has no hooked shackle.
    std::uint64_t unhook(RopeIndex rope);
    [[nodiscard]] std::uint64_t anchor_entity(AnchorIndex anchor) const noexcept;
    [[nodiscard]] std::uint64_t rope_shackle_entity(RopeIndex rope) const noexcept;

    // ---- checkpoint -----------------------------------------------------
    struct BodyState final {
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Quat rotation = JPH::Quat::sIdentity();
        JPH::Vec3 linear = JPH::Vec3::sZero();
        JPH::Vec3 angular = JPH::Vec3::sZero();
        bool enabled = true;
    };
    // Rubble spilled onto a static surface: a pile where it landed. Spills
    // landing within kPileMergeRadius of a pile join it; past kMaxPiles, a
    // spill joins the nearest pile.
    struct Pile final {
        JPH::RVec3 at = JPH::RVec3::sZero();
        float kg = 0.0F;
    };
    struct Checkpoint final {
        std::vector<BodyState> bodies;
        std::vector<AnchorIndex> rope_anchor;   // invalid: on its shackle
        std::vector<float> dog_floor;
        std::vector<float> bin_contents;
        std::vector<bool> rope_parted;
        std::vector<bool> catch_latched;
        std::vector<Pile> piles;
        std::vector<float> pool_water;
        std::vector<float> cell_air;
        float drained = 0.0F;
    };
    void capture(Checkpoint &out) const;
    void restore(const Checkpoint &in);

    // ---- read back ------------------------------------------------------
    [[nodiscard]] std::uint32_t body_count() const noexcept {
        return static_cast<std::uint32_t>(bodies_.size());
    }
    [[nodiscard]] std::uint64_t body_entity(BodyIndex body) const noexcept;
    [[nodiscard]] bool body_dynamic(BodyIndex body) const noexcept;
    [[nodiscard]] bool body_enabled(BodyIndex body) const noexcept;
    // Empty for an index the kit does not hold.
    [[nodiscard]] const std::vector<Part> &body_parts(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::RVec3 body_position(BodyIndex body) const noexcept;
    // Where the body's mass is centred now, contents included (a bin's
    // rubble is spread like the body).
    [[nodiscard]] JPH::RVec3 body_center_of_mass(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::Quat body_rotation(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::Vec3 body_velocity(BodyIndex body) const noexcept;
    // The body's mass now: its own, plus the rubble in it if it is a bin.
    [[nodiscard]] float body_mass(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::BodyID body_id(BodyIndex body) const noexcept;
    [[nodiscard]] BodyIndex body_for_entity(std::uint64_t entity) const noexcept;

    [[nodiscard]] std::uint32_t rope_count() const noexcept {
        return static_cast<std::uint32_t>(ropes_.size());
    }
    // The rope as drawn: its first end, its sheaves, its other end. Parted
    // or let go, only its end's side: from the end up to its sheave.
    void rope_polyline(RopeIndex rope, std::vector<JPH::RVec3> &out) const;
    // Everything drawn as a cable: the ropes, then the trip lines.
    [[nodiscard]] std::uint32_t cable_count() const noexcept {
        return static_cast<std::uint32_t>(ropes_.size() + lines_.size());
    }
    void cable_polyline(std::uint32_t cable, std::vector<JPH::RVec3> &out) const;
    [[nodiscard]] bool rope_parted(RopeIndex rope) const noexcept;
    // The entity of the body the rope's end is on now: an anchor's body, its
    // shackle, or the body it was made fast to.
    [[nodiscard]] std::uint64_t rope_end_entity(RopeIndex rope) const noexcept;
    // The pull on the rope's first end (body1's side), N.
    [[nodiscard]] float rope_tension(RopeIndex rope) const noexcept;

    [[nodiscard]] std::uint32_t bin_count() const noexcept {
        return static_cast<std::uint32_t>(bins_.size());
    }
    [[nodiscard]] float bin_contents(BinIndex bin) const noexcept;
    [[nodiscard]] float bin_capacity(BinIndex bin) const noexcept;
    [[nodiscard]] BodyIndex bin_body(BinIndex bin) const noexcept;
    [[nodiscard]] bool bin_water(BinIndex bin) const noexcept;
    // The stream leaving the bin this step: from its mouth down to where it
    // lands. False when its gate is shut or it is empty.
    [[nodiscard]] bool bin_stream(BinIndex bin, JPH::RVec3 &from, JPH::RVec3 &to) const noexcept;
    // Rubble that has left the system onto static surfaces, kg.
    [[nodiscard]] float spilled() const noexcept;
    [[nodiscard]] const std::vector<Pile> &piles() const noexcept { return piles_; }

    [[nodiscard]] std::uint32_t pool_count() const noexcept {
        return static_cast<std::uint32_t>(pools_.size());
    }
    [[nodiscard]] float pool_level(PoolIndex pool) const noexcept;
    [[nodiscard]] float pool_water(PoolIndex pool) const noexcept;
    [[nodiscard]] float pool_floor(PoolIndex pool) const noexcept;
    // The pool's box, floor to top.
    void pool_box(PoolIndex pool, JPH::Vec3 &min_corner, JPH::Vec3 &max_corner) const noexcept;
    // Water gone from the system: overflows and spills, kg.
    [[nodiscard]] float drained() const noexcept { return drained_; }
    [[nodiscard]] std::uint32_t pipe_count() const noexcept {
        return static_cast<std::uint32_t>(pipes_.size());
    }
    // A spout pouring this step: from the spout to where it lands.
    [[nodiscard]] bool pipe_stream(PipeIndex pipe, JPH::RVec3 &from, JPH::RVec3 &to) const noexcept;
    // The water through the pipe this step, kg/s (from -> to positive).
    [[nodiscard]] float pipe_flow(PipeIndex pipe) const noexcept;
    [[nodiscard]] bool pipe_whole(PipeIndex pipe) const noexcept;
    [[nodiscard]] float cell_pressure(CellIndex cell) const noexcept;

    [[nodiscard]] bool catch_latched(CatchIndex catch_index) const noexcept;
    [[nodiscard]] float lever_angle(LeverIndex lever) const noexcept;
    [[nodiscard]] float guide_travel(GuideIndex guide) const noexcept;
    // True while the guide has no rail gap or its joint lies in its seat.
    [[nodiscard]] bool rail_whole(GuideIndex guide) const noexcept;
    // True while a clutched rope's clutch is in, or the rope has no clutch.
    [[nodiscard]] bool clutch_in(RopeIndex rope) const noexcept;
    [[nodiscard]] float guide_peak_speed(GuideIndex guide) const noexcept;

private:
    struct Body final {
        JPH::BodyID id;
        std::uint64_t entity = 0;
        bool dynamic = false;
        bool enabled = true;
        float mass = 0.0F;
        std::vector<Part> parts;
        CarryKind carry = CarryKind::None;
        JPH::Vec3 handle = JPH::Vec3::sZero();
        JPH::RVec3 parked = JPH::RVec3::sZero();
    };
    struct Anchor final {
        BodyIndex body;
        JPH::Vec3 local = JPH::Vec3::sZero();
        float reach = 1.0F;
    };
    struct Guide final {
        BodyIndex body;
        JPH::Ref<JPH::SliderConstraint> slider;
        JPH::Vec3 axis = JPH::Vec3::sAxisY();
        float min_travel = 0.0F;
        float max_travel = 0.0F;
        float governor_speed = 0.0F;
        float governor_force = 0.0F;
        float level_accel = 0.0F;
        float peak_speed = 0.0F;
        float dog_pitch = 0.0F;     // 0: no dogs
        float dog_floor = 0.0F;     // the tooth the pawl rests above
        BodyIndex gap_joint;        // invalid: no rail gap
        JPH::RVec3 gap_seat = JPH::RVec3::sZero();
        JPH::Vec3 gap_axis = JPH::Vec3::sAxisX();
        float gap_travel = 0.0F;
        float gap_tolerance = 0.0F;
        float gap_angle = 0.0F;
        float limit_low = 0.0F;     // the slider's limits as last set
        float limit_high = 0.0F;
    };
    struct Rope final {
        BodyIndex body1;
        JPH::Vec3 point1 = JPH::Vec3::sZero();
        JPH::RVec3 fixed1 = JPH::RVec3::sZero();
        JPH::RVec3 fixed2 = JPH::RVec3::sZero();
        BodyIndex shackle;     // the loose-end body, invalid if none
        JPH::Vec3 shackle_point = JPH::Vec3::sZero();
        AnchorIndex anchor;    // hooked anchor, invalid if on its shackle
        BodyIndex fast_body;   // an end made fast at build
        JPH::Vec3 fast_point = JPH::Vec3::sZero();
        float ratio = 1.0F;
        float length = 0.0F;
        float rating = 0.0F;
        std::uint32_t over_rating_steps = 0;
        bool parted = false;
        bool strut = false;
        LeverIndex clutch;          // invalid: no clutch
        float clutch_angle = 0.0F;
        bool clutch_engaged = false;
        float tension = 0.0F;
        JPH::Ref<JPH::PulleyConstraint> constraint;
    };
    struct Lever final {
        BodyIndex body;
        JPH::Ref<JPH::HingeConstraint> hinge;
        JPH::RVec3 pivot = JPH::RVec3::sZero();   // world: every lever hinges on the world
    };
    struct Line final {
        BodyIndex lever_body;
        JPH::Vec3 lever_point = JPH::Vec3::sZero();
        BodyIndex handle_body;
        JPH::Vec3 handle_point = JPH::Vec3::sZero();
        JPH::RVec3 sheave1 = JPH::RVec3::sZero();
        JPH::RVec3 sheave2 = JPH::RVec3::sZero();
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
    };
    struct Bin final {
        BodyIndex body;
        float contents = 0.0F;
        float capacity = 0.0F;
        JPH::Vec3 mouth = JPH::Vec3::sZero();
        LeverIndex gate;
        float gate_open_angle = 0.0F;
        float gate_reach = 0.0F;
        float flow_rate = 0.0F;
        bool flowing = false;
        bool water = false;
        JPH::RVec3 stream_from = JPH::RVec3::sZero();
        JPH::RVec3 stream_to = JPH::RVec3::sZero();
    };
    struct Float final {
        BodyIndex body;
        float half_x = 0.0F;
        float half_z = 0.0F;
        float bottom = 0.0F;   // body-local y
        float height = 0.0F;
    };
    struct Pool final {
        JPH::Vec3 min_corner = JPH::Vec3::sZero();
        JPH::Vec3 max_corner = JPH::Vec3::sZero();
        float water = 0.0F;   // kg
        float level = 0.0F;
        std::vector<Float> floats;
    };
    struct Pipe final {
        PoolIndex from;
        float from_y = 0.0F;
        PoolIndex to;
        float to_y = 0.0F;
        JPH::RVec3 spout = JPH::RVec3::sZero();
        float area = 0.0F;
        LeverIndex valve;
        float shut_angle = 0.0F;
        float open_angle = 0.0F;
        BodyIndex spool;
        JPH::RVec3 seat = JPH::RVec3::sZero();
        JPH::Vec3 seat_axis = JPH::Vec3::sAxisX();
        float seat_tolerance = 0.0F;
        float seat_angle = 0.0F;
        float flow = 0.0F;   // kg/s this step
        bool pouring = false;
        JPH::RVec3 stream_to = JPH::RVec3::sZero();
    };
    struct Charge final {
        PoolIndex pool;
        float from_y = 0.0F;
        BodyIndex body;
        float area = 0.0F;
        float base_y = 0.0F;
        LeverIndex valve;
        float open_angle = 0.0F;
        float last_y = 0.0F;
    };
    struct Piston final {
        BodyIndex body;
        float area = 0.0F;
        float rest_y = 0.0F;   // centre-of-mass height as built
    };
    struct Door final {
        LeverIndex lever;
        float area = 0.0F;
        float full_angle = 1.0F;
        JPH::Vec3 normal = JPH::Vec3::sAxisZ();
    };
    struct Cell final {
        float volume0 = 0.0F;
        float air = 0.0F;   // P V, Pa m^3
        std::vector<Piston> pistons;
        std::vector<Door> doors;
        float breaker = 0.0F;
        float bleed = 0.0F;
    };
    struct Throttle final {
        CellIndex a;
        CellIndex b;
        float area = 0.0F;
    };
    struct Slip final {
        RopeIndex rope;
        LeverIndex lever;
        float release_angle = 0.0F;
    };
    struct Catch final {
        BodyIndex body;
        LeverIndex lever;
        float release_angle = 0.0F;
        float seat_tolerance = 0.0F;
        bool relatch = false;
        JPH::RVec3 seat = JPH::RVec3::sZero();
        JPH::Quat seat_rotation = JPH::Quat::sIdentity();
        BodyIndex pin_body;                 // a pin catch's pin, invalid for a lever catch
        JPH::RVec3 pin_seat = JPH::RVec3::sZero();
        float pin_tolerance = 0.0F;
        JPH::Ref<JPH::FixedConstraint> pin;
    };

    template <typename Record, typename Tag>
    [[nodiscard]] static const Record *find(const std::vector<Record> &table,
                                            const Index<Tag> index) noexcept {
        return index.value < table.size() ? &table[index.value] : nullptr;
    }
    // Build-time lookups: the band builder passes indices the kit just
    // returned, so these index directly.
    [[nodiscard]] JPH::Body &jolt_body(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::RVec3 world_point(BodyIndex body, JPH::Vec3 local) const noexcept;
    void connect_rope(Rope &rope);
    void disconnect_rope(Rope &rope);
    [[nodiscard]] BodyIndex rope_end_body(const Rope &rope) const noexcept;
    [[nodiscard]] JPH::Vec3 rope_end_point(const Rope &rope) const noexcept;
    void set_enabled(BodyIndex body, bool enabled);
    void latch(Catch &catch_record);
    void unlatch(Catch &catch_record);
    void govern(Guide &guide) noexcept;
    void engage_dogs(Guide &guide);
    [[nodiscard]] bool gap_closed(const Guide &guide) const;
    void apply_limits(Guide &guide);
    void flow_bins(float delta_seconds);
    void apply_bin_mass(const Bin &bin);
    // The middle of the top of a bin's floor, its body's first part, local.
    [[nodiscard]] JPH::Vec3 floor_top(const Bin &bin) const;
    void spill(JPH::RVec3 at, float kg);
    // Straight down from `from`, passing moving bodies that are not bins: the
    // first bin, or the first static surface. False for nothing within
    // kStreamReach.
    bool land_stream(JPH::RVec3 from, JPH::BodyID ignore, Bin *&receiver, JPH::RVec3 &lands);
    void flow_water(float delta_seconds);
    void flow_air(float delta_seconds);
    void press(float delta_seconds);
    void settle_level(Pool &pool);
    [[nodiscard]] float pool_volume_at(const Pool &pool, float level) const;
    [[nodiscard]] float pipe_opening(const Pipe &pipe) const;
    [[nodiscard]] float cell_volume(const Cell &cell) const;

    JPH::PhysicsSystem &system_;
    JPH::ObjectLayer static_layer_;
    JPH::ObjectLayer moving_layer_;
    std::vector<Body> bodies_;
    std::vector<Anchor> anchors_;
    std::vector<Guide> guides_;
    std::vector<Rope> ropes_;
    std::vector<Lever> levers_;
    std::vector<Catch> catches_;
    std::vector<Line> lines_;
    std::vector<Slip> slips_;
    std::vector<Bin> bins_;
    std::vector<Pile> piles_;
    std::vector<Pool> pools_;
    std::vector<Pipe> pipes_;
    std::vector<Charge> charges_;
    std::vector<Cell> cells_;
    std::vector<Throttle> throttles_;
    float drained_ = 0.0F;
};

} // namespace scraperx::sim::kit
