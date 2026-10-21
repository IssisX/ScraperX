#pragma once
#include "sim/grapple_rope_constraint.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <array>
#include <memory>

namespace scraperx::sim {
// Owner-thread commands/post_step/teardown are outside PhysicsSystem::Update.
// pre_step is called once per actual collision substep by the serialized native
// step listener. No contact callback writes or fixed-world anchors.
class GrapplingGun final {
public:
    static constexpr unsigned capacity = 4;
    static constexpr float hook_mass_kg = 2.0F;
    // Powered cartridge: finite work includes reciprocal player recoil.
    static constexpr float cartridge_energy_j = 900.0F;
    static constexpr float precision_energy_j = 180.0F;
    enum class ShotProfile : unsigned { Near, Long };
    static constexpr float maximum_line_length_m = 60.0F;
    static constexpr float strength_n = GrappleRopeSettings::strength_n;
    static constexpr float stiffness_n_per_m = GrappleRopeSettings::stiffness_n_per_m;
    static constexpr float damping_ns_per_m = GrappleRopeSettings::damping_ns_per_m;
    static constexpr float reel_power_w = 750.0F;
    static constexpr float reel_force_n = GrappleRopeSettings::reel_force_n;
    static constexpr float reel_speed_m_per_s = 1.0F;
    static constexpr float minimum_line_length_m = .6F;
    // Prospective launch geometry shared by fire and read-only presentation.
    // The caller supplies validated unit aim; no contact/attachment is implied.
    static JPH::RVec3 muzzle_start(JPH::RVec3 player_com) noexcept {
        return player_com + JPH::Vec3(0, .35F, 0);
    }
    static JPH::RVec3 launch_origin(JPH::RVec3 player_com, JPH::Vec3 normalized_aim) noexcept {
        return muzzle_start(player_com) + normalized_aim * .85F;
    }
    // Body-position coordinates: four open steel C fingers, finite back bridge,
    // hollow square stem and annular eye derive mass/COM/full inertia at7850kg/m3.
    // The native prism layout also drives Godot presentation. No envelope mass,
    // orientation assignment, latch or fixed anchor. Receivers retain via contact.
    static JPH::Vec3 hook_eye_local() noexcept;
    enum class LineState : unsigned { Ready, PayingOut, Locked, Released, Failed };
    enum class Failure : unsigned {
        None, InvalidAim, NoInventory, Busy, MuzzleBlocked, BodyUnavailable,
        Obstructed, StrengthExceeded, OutOfReach, LineTaut, RetrievalBlocked,
        RetrievalTooFast
    };
    struct HookPose {
        bool present = false;
        JPH::BodyID body;
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Quat rotation = JPH::Quat::sIdentity();
    };
    struct Readback {
        LineState state = LineState::Ready;
        Failure failure = Failure::None;
        bool auto_reel = false;
        ShotProfile shot_profile = ShotProfile::Long;
        float shot_energy_j = cartridge_energy_j;
        unsigned cartridges = capacity;
        unsigned available_hooks = capacity;
        JPH::BodyID hook;
        JPH::RVec3 player_endpoint = JPH::RVec3::sZero();
        JPH::RVec3 hook_endpoint = JPH::RVec3::sZero();
        float line_length_m = 0, tension_n = 0, extension_m = 0;
        double launch_work_j = 0, last_launch_work_j = 0;
        double stored_elastic_energy_j = 0, spool_absorbed_energy_j = 0;
        double release_discarded_elastic_energy_j = 0;
        // Conservative finite human take-up work supplied by target changes;
        // massless slack take-up costs zero. Not a complete solver energy ledger.
        double reel_work_j = 0;
        // Explicit inventory abstraction: retrieved bodies leave the world;
        // cartridges remain spent. This is not a complete energy ledger.
        double retrieval_absorbed_relative_energy_j = 0;
        std::array<HookPose, capacity> hooks{};
    };
    GrapplingGun(JPH::PhysicsSystem &system, JPH::BodyID player, JPH::ObjectLayer hook_layer);
    ~GrapplingGun();
    GrapplingGun(const GrapplingGun &) = delete;
    GrapplingGun &operator=(const GrapplingGun &) = delete;
    bool set_shot_profile(ShotProfile profile);
    bool fire(JPH::Vec3 normalized_aim);
    bool lock_line();
    // Owner-thread intent: +1 takes up, -1 pays out. Only Locked line responds;
    // finite human work/speed/force, no body position or velocity assignments.
    // Current spring/damping demand stalls take-up above1500N. On a substep
    // that actually shortens, the solver's1500N traction limit permits slip;
    // other passive substeps retain the separate3000N rope strength.
    void set_reel_input(float input);
    bool set_auto_reel(bool enabled);
    // Native read-only grasp/load admission, including a slow slack hook while
    // paying out. Final hand/withdrawal clearance is validated on the request.
    bool recovery_in_reach() const;
    bool retrieve();
    // A blocked, otherwise valid grasp can perform a short reciprocal physical
    // pull. Inventory removal still requires the unchanged whole-hook cast.
    [[nodiscard]] bool pickup_active() const noexcept;
    [[nodiscard]] double pickup_work_j() const noexcept;
    // Removing the rope changes neither endpoint body's instantaneous velocity.
    void release();
    void pre_step(float actual_collision_dt);
    void post_step();
    [[nodiscard]] Readback readback() const;
    [[nodiscard]] bool line_active() const noexcept;
    // Call before checkpoint restore/player destruction; resets finite inventory
    // along with all shot bodies and owned constraints. Never remote retrieval.
    void clear_transients();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace scraperx::sim
