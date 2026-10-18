#pragma once

#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <cstdint>
#include <memory>

namespace scraperx::sim {

// One non-gripping palm. Lifecycle changes run on the owner thread outside
// Update; pre_step runs under the serialized collision listener's body locks.
// A fresh exposed surface and unobstructed reachable approach are required by
// the acquisition owner. This row creates no receiving foot support.
class PhysicalHandProp final {
public:
    enum class StopReason : std::uint8_t {
        None, Cleared, InvalidBody, InvalidGeometry, InvalidTimeStep,
        SurfaceLost, ReachLost, Separated, StrokeExhausted, Expired,
    };
    struct Readback final {
        bool active = false;
        StopReason stop_reason = StopReason::None;
        JPH::BodyID support;
        JPH::SubShapeID subshape;
        JPH::RVec3 world_anchor = JPH::RVec3::sZero();
        JPH::Vec3 surface_normal = JPH::Vec3::sAxisY();
        JPH::Vec3 point_velocity = JPH::Vec3::sZero();
        JPH::Vec3 player_force = JPH::Vec3::sZero();
        std::uint64_t generation = 0;
        float last_impulse_ns = 0;
        float last_collision_dt = 0;
        float last_load_n = 0;
        float peak_load_n = 0;
        float yielded_normal_distance_m = 0;
        double elapsed_seconds = 0;
        // Work of this row's admitted frozen-Jacobian impulse. Never a
        // whole-system energy closure or a positive muscle-work command.
        double last_event_work_j = 0;
        double absorbed_event_work_j = 0;
        double command_work_j = 0;
    };

    PhysicalHandProp(JPH::PhysicsSystem &system, JPH::BodyID player,
                     float shoulder_offset_m, float maximum_reach_m);
    ~PhysicalHandProp();
    PhysicalHandProp(const PhysicalHandProp &) = delete;
    PhysicalHandProp &operator=(const PhysicalHandProp &) = delete;

    // Failed acquisition preserves an existing contact and its original age.
    // The normal points out of the support. Only an approaching/stationary
    // reachable palm may engage; upward separation is never held for later.
    bool begin(JPH::BodyID support, JPH::SubShapeID subshape,
               JPH::RVec3 actual_surface_point, JPH::Vec3 outward_normal);
    void clear(StopReason reason = StopReason::Cleared);
    void pre_step(float collision_dt, bool surface_valid = true) noexcept;
    void post_step(float collision_dt);
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] const Readback &readback() const noexcept;
    [[nodiscard]] JPH::Vec3 support_local_anchor() const noexcept;
    [[nodiscard]] float shoulder_offset_m() const noexcept;

    // Same per-hand ceiling as PhysicalHandClimb. The new passive contact
    // has no motor, spring reference, preload or positive command budget.
    [[nodiscard]] static constexpr float force_bound_n() noexcept { return 1500; }
    // Initial gameplay hypotheses; must remain finite and cannot be renewed.
    [[nodiscard]] static constexpr float maximum_yield_m() noexcept { return .20F; }
    [[nodiscard]] static constexpr float duration_seconds() noexcept { return .25F; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace scraperx::sim
#endif
