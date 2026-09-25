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
    void set_body_mass(BodyIndex body, float mass_kg);
    AnchorIndex add_anchor(BodyIndex body, JPH::Vec3 local, float reach);

    // A straight guide along a world axis through the body's present
    // position. Travel is measured from there. governor_speed == 0 means no
    // governor. The governor brakes toward a target speed that falls to
    // zero at each end of travel at level_accel, so a governed body comes
    // to its stop gently instead of on the limit.
    GuideIndex add_guide(BodyIndex body, JPH::Vec3 axis, float min_travel, float max_travel,
                         float governor_speed, float governor_force, float level_accel);

    // A rope from body1's point over fixed1 ... fixed2 to its end. The end
    // starts on end_body at end_point: a shackle body (loose end) or any
    // body (an end that is already made fast). Length is fixed for the
    // rope's life: max_length, tension only. rating_newtons == 0: it never
    // parts.
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
    CatchIndex add_catch_at(BodyIndex body, LeverIndex lever, JPH::RVec3 seat,
                            float release_angle, float seat_tolerance, bool relatch,
                            bool initially_latched);

    // A trip line: a light rope from a lever's arm point over sheave1, along
    // to sheave2 and down to a handle hanging there, as laid. Pulling the
    // handle away from sheave2 turns the lever.
    LineIndex add_trip_line(BodyIndex lever_body, JPH::Vec3 lever_point, BodyIndex handle_body,
                            JPH::Vec3 handle_point, JPH::RVec3 sheave1, JPH::RVec3 sheave2);

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
    void set_rope_connected(RopeIndex rope, bool connected);
    void release_catch(CatchIndex catch_index);
    void arm_catch(CatchIndex catch_index);
    void engage_catch(CatchIndex catch_index);
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
    struct Checkpoint final {
        std::vector<BodyState> bodies;
        std::vector<AnchorIndex> rope_anchor;   // invalid: on its shackle
        std::vector<bool> rope_parted;
        std::vector<bool> catch_latched;
        std::vector<bool> catch_armed;
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
    [[nodiscard]] JPH::Quat body_rotation(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::Vec3 body_velocity(BodyIndex body) const noexcept;
    [[nodiscard]] float body_mass(BodyIndex body) const noexcept;
    [[nodiscard]] JPH::BodyID body_id(BodyIndex body) const noexcept;
    [[nodiscard]] BodyIndex body_for_entity(std::uint64_t entity) const noexcept;

    [[nodiscard]] std::uint32_t rope_count() const noexcept {
        return static_cast<std::uint32_t>(ropes_.size());
    }
    // The rope as drawn: its first end, its sheaves, its other end. Empty
    // when parted.
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
    [[nodiscard]] float rope_tension(RopeIndex rope) const noexcept;

    [[nodiscard]] bool catch_latched(CatchIndex catch_index) const noexcept;
    [[nodiscard]] float lever_angle(LeverIndex lever) const noexcept;
    [[nodiscard]] float guide_travel(GuideIndex guide) const noexcept;
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
        float tension = 0.0F;
        JPH::Ref<JPH::PulleyConstraint> constraint;
    };
    struct Lever final {
        BodyIndex body;
        JPH::Ref<JPH::HingeConstraint> hinge;
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
    struct Catch final {
        BodyIndex body;
        LeverIndex lever;
        float release_angle = 0.0F;
        float seat_tolerance = 0.0F;
        bool relatch = false;
        bool armed = true;
        JPH::RVec3 seat = JPH::RVec3::sZero();
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
};

} // namespace scraperx::sim::kit
