#pragma once

#include "sim/mechanism_kit.hpp"
#include "sim/slingshot_model.hpp"

#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <atomic>

namespace scraperx::sim {

// One manually charged machine. The kit owns every visible collision body;
// this owner manages the removable draw restraint and the rider harness.
// The velocity motor is the player's explicitly exaggerated muscle source,
// bounded in both force and work rate. Release removes restraints only.
class Slingshot final : private JPH::PhysicsStepListener {
public:
    static constexpr std::uint64_t kPouchEntity = 2900;
    static constexpr std::uint64_t kFrameEntity = 1950;
    static constexpr std::uint64_t kRailEntity = 1951;
    static constexpr std::uint64_t kLaunchRailEntity = 2901;
    static constexpr double kMaximumDrawM = 12.0;
    static constexpr double kForkRadiusM = 10.0;
    static constexpr double kHalfSpanM = 3.0;
    static constexpr double kBandStiffnessNPerM = 10000.0;
    static constexpr double kPlayerPowerW = 200000.0;
    static constexpr double kTransmissionEfficiency = 0.88;
    static constexpr double kMaximumPouchForceN = 160000.0;
    static constexpr double kMaximumDrawSpeedMps = 4.0;
    static constexpr double kRetrievalPowerW = 20000.0;
    static constexpr double kRetrievalForceN = 16000.0;
    static constexpr float kPouchMassKg = 15.0F;
    static constexpr float kLaunchRailMassKg = 60.0F;
    static constexpr double kLaunchTrackLengthM = 12.0;

    struct State final {
        bool station_available = false;
        bool seated = false;
        bool drawing = false;
        bool released = false;
        bool guide_latched = true;
        bool can_retrieve = false;
        bool recovering = false;
        bool guided_launch = false;
        bool track_exit = false;
        bool release_ready = false;
        bool ledger_valid = true;
        bool pouch_pair_excluded = false;
        bool aim_ready = true;
        bool aim_locked = false;
        double leather_deflection_m = 0.0;
        double leather_energy_j = 0.0;
        double leather_port_violation_j = 0.0;
        JPH::RVec3 harness_rest_local = JPH::RVec3::sZero();
        double draw_m = 0.0;
        double ratchet_draw_m = 0.0;
        double energy_j = 0.0;
        double work_j = 0.0;             // work supplied by arcade player
        double source_work_j = 0.0;      // same value, explicit ledger name
        double source_power_w = 0.0;
        double transmission_loss_j = 0.0;
        double dissipated_work_j = 0.0;
        double kinetic_j = 0.0;          // pouch AND physical rider
        double gravity_j = 0.0;
        double energy_residual_j = 0.0;
        double retrieval_work_j = 0.0;
        double retrieval_source_power_w = 0.0;
        double retrieval_braking_work_j = 0.0;
        double aim_control_work_j = 0.0;
        double aim_source_power_w = 0.0;
        double target_yaw_rad = 0.0;
        double target_elevation_rad = 1.4311699866353502;
        double max_draw_m = kMaximumDrawM;
        double band_rest_m = 10.44030650891055;
        double power_limit_w = kPlayerPowerW;
        double yaw_rad = 0.0;
        double elevation_rad = 1.4311699866353502;
        JPH::RVec3 anchor_left = JPH::RVec3::sZero();
        JPH::RVec3 anchor_right = JPH::RVec3::sZero();
        JPH::RVec3 pouch_position = JPH::RVec3::sZero();
        // The rail's real seat clearance accepts the measured returned pose.
        // This checkpoint field avoids a scripted post-capture alignment.
        // Defaults match neutral_position(); create_guide runs before the
        // first refresh and reads them.
        JPH::RVec3 guide_origin = JPH::RVec3(-18.0, .35, -42.0);
        JPH::RVec3 carrier_origin = JPH::RVec3(-18.0, .35, -42.0);
        JPH::RVec3 launch_track_start = JPH::RVec3::sZero();
        JPH::RVec3 launch_track_end = JPH::RVec3::sZero();
        std::uint32_t launch_count = 0;
    };

    Slingshot(JPH::PhysicsSystem &system, kit::Kit &kit, JPH::BodyID player);
    ~Slingshot();
    Slingshot(const Slingshot &) = delete;
    Slingshot &operator=(const Slingshot &) = delete;

    // Aim arguments are absolute radians; yaw zero points toward -Z.
    // Positive draw is backwards (+Z). The fixed fork anchors never move;
    // paid rail aiming is permitted with held draw until physical release.
    void pre_step(float delta_seconds, double draw_input, double yaw,
                  double elevation, bool action, bool release, bool rider_supported = false);
    void post_step(float delta_seconds);
    [[nodiscard]] bool controls_player() const noexcept { return harness_ != nullptr; }
    [[nodiscard]] const State &state() const noexcept { return state_; }
    [[nodiscard]] kit::BodyIndex pouch_body() const noexcept { return pouch_; }
    [[nodiscard]] kit::BodyIndex frame_body() const noexcept { return frame_; }
    // Advisory free-flight preview integrated from the same force law and
    // real loaded mass. The caller may clip segments against world geometry.
    [[nodiscard]] std::vector<JPH::RVec3> prediction(double duration_seconds = 12.0,
                                                    double sample_seconds = .10) const;
    [[nodiscard]] static JPH::RVec3 neutral_position() noexcept {
        // West of the grade-to-S1 walk and the CI approach. The ChatGPT
        // yard site (6, 0.35, -55) stands on both of those paths.
        return JPH::RVec3(-18.0, 0.35, -42.0);
    }
    [[nodiscard]] static JPH::RVec3 retrieval_control_position() noexcept {
        return neutral_position() + JPH::RVec3(1.5, .35, -1.8);
    }
    // Shared rest/fold surface. Deflection uses the same rim-anchored weight
    // as the visible leather; default zero keeps the rest collision panels.
    [[nodiscard]] static JPH::Vec3 leather_surface(float u, float v,
                                                  float deflection_m = 0.0F) noexcept;
    void detach();
    // Explicit relocation ends player operation while retaining all machine
    // bodies and stored spring energy at their measured physical state.
    void cancel_player_interaction();
    // Forward real events; transient CCD contacts can leave no cached pair.
    void note_pouch_contact() noexcept { contact_observed_.store(true, std::memory_order_relaxed); }
    void note_carrier_contact() noexcept { contact_observed_.store(true, std::memory_order_relaxed); }
    // Rider contact, air steering and parachute forces are outside this
    // machine's independent force ledger. The owner consumes this witness
    // after the physics step while that rider belongs to the audited system.
    void note_external_influence() noexcept { external_influence_observed_.store(true, std::memory_order_relaxed); }
    // Restore is called after restoring the kit/player physical checkpoint.
    void restore(const State &state);
    // Explicit level-reset abstraction, never an automatic post-shot return.
    void reset();

private:
    [[nodiscard]] bool player_in_pouch() const;
    [[nodiscard]] bool unloaded_pouch_ready() const;
    [[nodiscard]] bool player_at_control() const;
    [[nodiscard]] slingshot::BandEvaluation bands(bool no_lock = false) const;
    void aim(double yaw, double elevation, float dt = 0.0F);
    void create_aim_gimbal();
    void remove_aim_gimbal();
    void refresh_geometry(bool no_lock = false);
    [[nodiscard]] JPH::Vec3 aim_torque(float dt, bool no_lock) const;
    void create_guide(double held_draw);
    void remove_guide();
    void create_carrier_guide();
    void bind_carrier();
    void unbind_carrier();
    void create_launch_guide(bool restoring = false);
    void remove_launch_guide();
    void attach(bool checkpoint = false);
    [[nodiscard]] double leather_deflection(bool no_lock = false) const;
    void refresh_state();
    void finish_previous_step(bool no_lock = false);
    void OnStep(const JPH::PhysicsStepListenerContext &context) override;
    void drive_ratchet(const slingshot::BandEvaluation &evaluation,
                       double actual_speed);
    void begin_recovery();
    void finish_recovery();
    [[nodiscard]] JPH::Vec3 recovery_force(const slingshot::BandEvaluation &evaluation,
                                         JPH::RVec3 position, JPH::Vec3 velocity,
                                         float dt) const;
    [[nodiscard]] double body_energy(JPH::BodyID id, double &kinetic,
                                     double &gravity) const;

    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    JPH::BodyID player_;
    kit::BodyIndex pouch_, frame_, rail_, launch_rail_;
    JPH::Ref<JPH::SliderConstraint> guide_;
    JPH::Ref<JPH::SixDOFConstraint> harness_;
    JPH::Ref<JPH::SliderConstraint> carrier_guide_, launch_guide_;
    JPH::Ref<JPH::FixedConstraint> carrier_bind_;
    JPH::Ref<JPH::SixDOFConstraint> aim_gimbal_;
    JPH::CollisionGroup saved_player_group_;
    slingshot::AnchorPoints anchors_{};
    slingshot::BandParameters band_parameters_{10.44030650891055, kBandStiffnessNPerM, 15.0};
    State state_{};
    bool rider_supported_ = false;
    bool step_pending_ = false;
    bool audit_rider_ = false;
    bool audit_initialized_ = false;
    float previous_dt_ = 0.0F;
    double previous_leather_q_ = 0.0;
    double previous_leather_energy_ = 0.0;
    bool previous_leather_active_ = false;
    double previous_draw_ = 0.0;
    double previous_band_loss_w_ = 0.0;
    double initial_mechanical_energy_j_ = 0.0;
    double draw_effort_ = 0.0;
    JPH::Vec3 applied_band_force_ = JPH::Vec3::sZero();
    JPH::Vec3 applied_recovery_force_ = JPH::Vec3::sZero();
    JPH::Vec3 applied_aim_torque_ = JPH::Vec3::sZero();
    JPH::Vec3 previous_aim_torque_ = JPH::Vec3::sZero();
    JPH::Quat previous_rail_rotation_ = JPH::Quat::sIdentity();
    JPH::Vec3 previous_recovery_force_ = JPH::Vec3::sZero();
    JPH::RVec3 previous_position_ = JPH::RVec3::sZero();
    double previous_carrier_z_ = 0;
    bool collision_exclusion_active_ = false;
    std::atomic<bool> contact_observed_{false};
    std::atomic<bool> external_influence_observed_{false};
};

} // namespace scraperx::sim
