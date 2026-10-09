#pragma once

#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <memory>

namespace scraperx::sim {

// Owner thread only. Begin, clear and post_step run outside PhysicsSystem::Update.
// pre_step runs in the existing serialized collision-step listener while bodies
// are locked. Clear before either body is removed or PhysicsSystem is destroyed.
// This is a transient gameplay constraint; snapshots/replay must clear it.
class PhysicalFootPush final {
public:
    enum class StopReason {
        None, Cleared, InvalidBody, InvalidTimeStep, InvalidGeometry,
        MaterialFaceLost, TractionLost, LateralReachLost, VerticalReachLost, Expired,
        StrokeExhausted, CommandBudgetExhausted,
    };
    struct Readback {
        bool active = false;
        bool stop_requested = false;
        StopReason stop_reason = StopReason::None;
        JPH::BodyID support;
        JPH::RVec3 world_anchor = JPH::RVec3::sZero();
        JPH::Vec3 leg_axis = JPH::Vec3::sAxisY();
        JPH::Vec3 last_solve_axis = JPH::Vec3::sAxisY();
        float last_collision_dt = 0;
        JPH::Vec3 player_force = JPH::Vec3::sZero();
        float initial_distance_m = 0;
        float rest_distance_m = 0;
        float actual_distance_m = 0;
        float stroke_limit_m = 0; // Requested command effort; geometric reach stays .3 m.
        float commanded_stroke_m = 0;
        float last_impulse_ns = 0; // LAST collision-step impulse, never Update sum.
        float last_load_n = 0;
        float peak_load_n = 0;
        double elapsed_seconds = 0;
        double command_work_bound_j = 0; // Fmax * representable rest increment.
        double last_command_work_bound_j = 0;
        // Nominal compression-spring diagnostic, not a second actuator budget
        // or a complete energy ledger for the force-capped implicit solver.
        double nominal_spring_storage_j = 0;
        double target_storage_delta_j = 0;
    };

    PhysicalFootPush(JPH::PhysicsSystem &system, JPH::BodyID player);
    ~PhysicalFootPush();
    PhysicalFootPush(const PhysicalFootPush &) = delete;
    PhysicalFootPush &operator=(const PhysicalFootPush &) = delete;

    // Retains this actual material point in support COM-local space, never
    // migrates to a neighbouring cell. Player endpoint is its actual COM.
    // Failed begin leaves any existing push intact.
    bool begin(JPH::BodyID support, JPH::RVec3 world_material_point,
               JPH::Vec3 world_surface_normal, float friction = .85F,
               float requested_stroke_m = maximum_stroke_m());
    void clear(StopReason reason = StopReason::Cleared);
    // Only advance/validate/disable the existing row here; no manager mutation.
    // Face and traction validity come from the existing native contact owner.
    void pre_step(float collision_dt, bool material_face_valid = true,
                  bool traction_valid = true) noexcept;
    // Samples the final collision row and removes requested stops outside Update.
    void post_step(float collision_dt);
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool stop_requested() const noexcept;
    [[nodiscard]] const Readback &readback() const noexcept;

    [[nodiscard]] static constexpr float force_bound_n() noexcept { return 3500; }
    [[nodiscard]] static constexpr float stiffness_n_per_m() noexcept { return 40000; }
    [[nodiscard]] static constexpr float damping_ns_per_m() noexcept { return 1000; }
    [[nodiscard]] static constexpr float command_power_bound_w() noexcept { return 5000; }
    [[nodiscard]] static constexpr double command_budget_j() noexcept { return 1285.625; }
    [[nodiscard]] static constexpr float maximum_stroke_m() noexcept { return .3F; }
    [[nodiscard]] static constexpr float duration_seconds() noexcept { return .25F; }
    [[nodiscard]] static constexpr float lateral_reach_m() noexcept { return .45F; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace scraperx::sim
#endif // SCRAPERX_HAS_JOLT
