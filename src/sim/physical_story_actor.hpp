#pragma once

#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <cstdint>

namespace scraperx::sim {

// One upright, translation-only human capsule, like the native player. This
// deliberately does not model articulated limbs or supply stairs/jump behavior.
// Owner-thread lifecycle/drive calls run outside PhysicsSystem::Update. The
// PhysicsSystem must outlive this owner; no other owner removes its body.
class PhysicalStoryActor final {
public:
    struct Contact {
        JPH::BodyID actor, support;
        JPH::SubShapeID subshape;
        JPH::Vec3 local_point = JPH::Vec3::sZero(); // Support COM frame, meters.
        JPH::Vec3 local_normal = JPH::Vec3::sAxisY();
        JPH::RefConst<JPH::Shape> shape; // Retain exact root identity.
        std::uint64_t solved_tick = 0;
    };
    struct Readback {
        bool present = false, supported = false;
        JPH::BodyID body, support;
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Vec3 velocity = JPH::Vec3::sZero();
        JPH::Vec3 applied_force_n = JPH::Vec3::sZero();
        JPH::RVec3 support_point = JPH::RVec3::sZero();
        JPH::Vec3 support_point_velocity = JPH::Vec3::sZero();
        double command_work_bound_j = 0; // Last frozen-inertia positive-work bound.
    };

    PhysicalStoryActor(JPH::PhysicsSystem &, JPH::ObjectLayer moving_layer,
                       std::uint64_t entity_id);
    ~PhysicalStoryActor();
    PhysicalStoryActor(const PhysicalStoryActor &) = delete;
    PhysicalStoryActor &operator=(const PhysicalStoryActor &) = delete;

    // Initial/reset placement only. Reset destroys/recreates the body and
    // clears commands/contact identity; it never preserves an old attachment.
    bool spawn(JPH::RVec3 world_center);
    bool reset(JPH::RVec3 world_center);
    void destroy();
    [[nodiscard]] JPH::BodyID body_id() const noexcept { return body_; }

    // Read-only callback helper: accepts actual rigid, non-sensor bottom
    // manifolds, never a speculative gap or ray. Safe while callback bodies
    // are locked. Buffer the result in the existing listener, then submit it
    // only AFTER that tick's successful Update. Do not call submit in callbacks.
    static bool capture_contact(JPH::BodyID actor, const JPH::Body &first,
                                const JPH::Body &second,
                                const JPH::ContactManifold &, std::uint64_t tick,
                                Contact &out) noexcept;
    void submit_solved_contact(const Contact &);
    void clear_contact() noexcept;

    // Exactly once before each native Update, using consecutive tick serials.
    // Only the previous solved tick can drive. Desired world XZ velocity is
    // bounded to walking speed; Y is ignored. Zero requests finite braking
    // relative to the support's actual point velocity. No air control.
    // Forces persist for this Update; use its full dt, not collision-substep dt.
    void pre_step(std::uint64_t next_tick, float dt_seconds,
                  JPH::Vec3 desired_world_velocity_mps);
    [[nodiscard]] Readback readback() const;

    static constexpr float mass_kg() noexcept { return 85; }
    static constexpr float half_height_m() noexcept { return .9F; }
    static constexpr float radius_m() noexcept { return .3F; }
    static constexpr float speed_bound_mps() noexcept { return 1.4F; }
    static constexpr float force_bound_n() noexcept { return 220; }
    static constexpr float power_bound_w() noexcept { return 300; }

private:
    JPH::PhysicsSystem &system_;
    JPH::ObjectLayer layer_;
    std::uint64_t entity_;
    JPH::BodyID body_;
    Contact contact_{};
    Readback drive_{};
    std::uint64_t last_pre_tick_ = 0;
    bool have_pre_tick_ = false, have_contact_ = false;
};
} // namespace scraperx::sim
#endif
