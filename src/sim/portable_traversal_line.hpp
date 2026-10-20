#pragma once

#include "sim/portable_line_path.hpp"
#include "sim/physical_hand_climb.hpp"
#include "sim/material_traversal_line.hpp"
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <cstdint>

namespace scraperx::sim {
class RiggingSystem;

// One reusable carried spool. Installation and the player's actual grips have
// separate lifetimes. The fitting is a bounded, preallocated native body, not a
// new Kit inventory entry; checkpoints therefore cannot change Kit body counts.
// Lifecycle commands run outside Update. collision_step only changes the rope
// target, through the existing finite human reel law.
class PortableTraversalLine final {
public:
    static constexpr float maximum_length_m = 18;
    static constexpr float minimum_length_m = .6F;
    static constexpr float install_reach_m = 1.8F;
    static constexpr float fitting_mass_kg = .25F;
    enum class Failure : std::uint8_t {
        None, UnsafeContext, NoTarget, StaleTarget, UnsuitableSupport,
        OutOfReach, Obstructed, AlreadyInstalled, NoLine, Loaded,
        MovingTooFast, StrengthExceeded, SupportChanged, HandsBusy
    };
    struct Anchor {
        JPH::BodyID body;
        JPH::SubShapeID subshape;
        // Shape-origin local, including the actual clamp's 25mm stand-off.
        JPH::Vec3 point = JPH::Vec3::sZero();
        JPH::Vec3 normal = JPH::Vec3::sAxisY();
        JPH::RefConst<JPH::Shape> shape_identity;
    };
    struct Checkpoint {
        bool installed = false, broken = false;
        Anchor anchor{};
        Anchor second_anchor{};
        MaterialTraversalLine::Checkpoint crossing{};
        PortableLinePath::State path{};
        float paid_length_m = 0;
        JPH::RVec3 fitting_position = JPH::RVec3::sZero();
        JPH::Quat fitting_rotation = JPH::Quat::sIdentity();
        JPH::Vec3 linear_velocity = JPH::Vec3::sZero();
        JPH::Vec3 angular_velocity = JPH::Vec3::sZero();
        double reel_work_j = 0, spool_absorbed_j = 0, recovery_absorbed_j = 0;
    };
    struct Readback {
        bool equipped = false, installed = false, broken = false, gripping = false;
        bool target_valid = false, can_grab = false, can_recover = false;
        bool two_ended = false, can_pin_end = false;
        unsigned regrip_count = 0;
        unsigned available_spools = 1;
        std::uint64_t target = 0, generation = 0, event_serial = 0;
        Failure failure = Failure::None;
        JPH::BodyID support, fitting;
        JPH::RVec3 target_point = JPH::RVec3::sZero();
        JPH::RVec3 anchor_point = JPH::RVec3::sZero();
        JPH::RVec3 grip_point = JPH::RVec3::sZero();
        float paid_length_m = 0, tension_n = 0;
        bool path_valid = false;
        unsigned path_reason = 0, contact_count = 0, path_point_count = 0;
        double path_length_m = 0;
        std::uint64_t path_token = 0;
        std::array<JPH::RVec3, PortableLinePath::visual_capacity> path_points{};
        double reel_work_j = 0, spool_absorbed_j = 0, recovery_absorbed_j = 0;
    };

    PortableTraversalLine(JPH::PhysicsSystem &, JPH::BodyID player,
                          JPH::ObjectLayer fitting_layer, PhysicalHandClimb &, const RiggingSystem *eyes = nullptr);
    ~PortableTraversalLine();
    PortableTraversalLine(const PortableTraversalLine &) = delete;
    PortableTraversalLine &operator=(const PortableTraversalLine &) = delete;
    void equip(bool);
    // Deterministic small aim fan; every accepted point is on a real compatible
    // Box leaf (including compounds), within reach and unobstructed.
    void aim(JPH::RVec3 hand, JPH::Vec3 direction, bool allowed);
    bool install(std::uint64_t shown_target);
    bool grab();
    bool pin_second_end(std::uint64_t shown_target);
    void traverse(JPH::Vec3 world_stick, float dt);
    [[nodiscard]] bool two_ended() const { return material_line_.installed(); }
    void release(); // Removes only this fitting's hand rows; leaves world line.
    bool recover(); // Reachable, unloaded and slow; a settled hand grip is legal.
    void set_reel_input(float);
    void collision_step(float collision_dt);
    void post_step();
    [[nodiscard]] Readback readback() const;
    [[nodiscard]] bool grip_active() const { return gripping(); }
    [[nodiscard]] Checkpoint capture() const;
    void detach_for_restore(); // Before restoring any supporting body.
    bool restore(const Checkpoint &); // Rebuild cold; never restores player grip.

private:
    JPH::PhysicsSystem &world_;
    JPH::BodyID player_, fitting_;
    PhysicalHandClimb &hands_;
    const RiggingSystem *eyes_;
    MaterialTraversalLine material_line_;
    JPH::Ref<FiniteCableConstraint> rope_;
    PortableLinePath::State path_state_{};
    PortableLinePath::Result path_{};
    Anchor anchor_{}, candidate_{};
    Anchor second_anchor_{};
    std::array<float, 2> hand_coordinates_{};
    unsigned next_hand_ = 0, regrip_count_ = 0;
    float traverse_stroke_ = 0;
    float hang_drop_m_ = 0;
    bool regrip_solved_ = false;
    JPH::RVec3 hand_ = JPH::RVec3::sZero();
    JPH::Vec3 aim_ = -JPH::Vec3::sAxisZ();
    std::uint64_t epoch_ = 1, target_ = 0, generation_ = 0, event_serial_ = 0;
    bool equipped_ = false, allowed_ = false, installed_ = false, broken_ = false;
    bool held_solved_ = false;
    float reel_input_ = 0, paid_length_m_ = 0;
    Failure failure_ = Failure::None;
    double reel_work_j_ = 0, spool_absorbed_j_ = 0, recovery_absorbed_j_ = 0;
    bool reject(Failure);
    [[nodiscard]] JPH::Body *support(const Anchor &) const;
    [[nodiscard]] bool registered_eye(const Anchor &) const;
    [[nodiscard]] JPH::RVec3 point(const Anchor &) const;
    [[nodiscard]] bool clear(JPH::RVec3, JPH::RVec3, JPH::BodyID ignored) const;
    [[nodiscard]] bool gripping() const;
    [[nodiscard]] bool unloaded() const;
    [[nodiscard]] bool reachable_equipment() const;
    [[nodiscard]] JPH::RVec3 deployment_position() const;
    [[nodiscard]] JPH::Quat deployment_rotation() const;
    [[nodiscard]] bool deployment_clear() const;
    [[nodiscard]] PortableLinePath::Input path_input() const;
    bool connect();
    bool grab_material();
    void disconnect();
    void remove_fitting();
};
} // namespace scraperx::sim
