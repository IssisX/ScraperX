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
        // Legacy: Fmax * representable rest increment. Force profiles:
        // sum of admitted positive work of its actual frozen-Jacobian impulses.
        double command_work_bound_j = 0;
        double last_command_work_bound_j = 0;
        // Nominal passive/legacy spring storage, not a complete energy ledger
        // for the force-capped implicit solver. Motor activation retires it;
        // the direct motor does not move a spring reference or store preload.
        double nominal_spring_storage_j = 0;
        double target_storage_delta_j = 0;
        bool target_held = false;
        bool passive_plant = false;
        bool planted_motor_profile = false;
        double motor_positive_work_j = 0;
        double motor_absorbed_work_j = 0; // Never credited to command budget.
        double retired_nominal_storage_j = 0; // Diagnostic at dissipative clutch-over.
        float actual_motor_extension_m = 0; // Actual L-L0, not commanded rest travel.
        bool walking_step = false;
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
    // Retain a zero-command compression row at the actual attachment distance.
    // The held row still responds to external compression and expires normally.
    // A planted leg swivels within its original radial reach plus .30m and the
    // material friction cone. That same geometry survives activation/hold;
    // legacy begin retains its separate .45m tangential-cylinder restriction.
    bool begin_passive_plant(JPH::BodyID support, JPH::RVec3 world_material_point,
                             JPH::Vec3 world_surface_normal, float friction = .85F);
    // Engage a finite force motor on the existing valid passive row without
    // renewing its anchor, rest target, work ledger or original finite lifetime.
    // One reciprocal impulse per collision step is capped at3500N*h and its
    // positive event work at5000W*h plus the unchanged total budget. The old
    // spring's nominal storage is retired, never credited to the motor. Active
    // motor rows have no warm start; holding stops drive without adding a spring.
    // A refused activation leaves the row unchanged.
    bool activate_planted_push();
    // Switch an existing approach-phased step to full finite push-off.
    // The material anchor, age, stroke and admitted-work ledger are retained.
    // Failed or repeated transitions leave the current row unchanged.
    bool push_off_existing_step();
    // A single finite walking stroke from a solved foot contact. The damped
    // compression motor shares the jump motor's force/power/work/reach limits,
    // and phases lift with measured approach instead of launching a free hop.
    bool begin_walking_step(JPH::BodyID support, JPH::RVec3 world_material_point,
                            JPH::Vec3 world_surface_normal, float friction,
                            float requested_rise_m, JPH::Vec3 world_approach_direction,
                            float approach_distance_m);
    // Refresh an admissible surface normal of the same retained material contact.
    // Does not move its anchor or renew time, work, stroke or force limits.
    bool refresh_walking_contact_normal(JPH::Vec3 world_surface_normal);
    void clear(StopReason reason = StopReason::Cleared);
    // Stop further target extension without changing the actual row, its
    // compression or its original finite lifetime. Only a new begin resets it.
    // On a planted motor this stops drive; it never restores a spring.
    void hold_target() noexcept;
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
    bool begin_contact(JPH::BodyID support, JPH::RVec3 world_material_point,
                       JPH::Vec3 world_surface_normal, float friction,
                       float requested_stroke_m, bool planted_geometry);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace scraperx::sim
#endif // SCRAPERX_HAS_JOLT
