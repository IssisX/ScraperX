#pragma once

// AS-012, the swing (the owner's rule 11: "swung on a hanging mass"). Off the
// 220 ring's south face a jib carries two pendulums of one make: 28.8 m arms
// on one axle line, 3.15 m apart. One is the ram, hung back on a hook as
// found; the other is the rider's seat, at the end of a gangway out from the
// ring. Kicked from the seat, the trip wire lets the ram go: it swings down,
// strikes the seat's back through a rubber buffer and stops nearly dead, the
// two being of one weight with the rider aboard, and the seat goes up its
// arc to the 242 ring's edge, where the pawl on its frame drops into the
// last tooth of the rack it passed. Each hanging body is kept level by a
// parallel link (declared: its rotation is not a degree of freedom).
//
// Jolt owns every body and contact; this owner adds what Jolt does not
// model: the buffer's spring and its loss, the trip wire's release, the
// ratchet's teeth and the rider's harness. No velocity or position is set.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsStepListener.h>

#include <atomic>

namespace scraperx::sim {

class Swing final : private JPH::PhysicsStepListener {
public:
    static constexpr std::uint64_t kFrameEntity = 1960;
    static constexpr std::uint64_t kSeatArmEntity = 2910;
    static constexpr std::uint64_t kSeatEntity = 2911;
    static constexpr std::uint64_t kRamArmEntity = 2912;
    static constexpr std::uint64_t kRamEntity = 2913;
    static constexpr std::uint64_t kKickBarEntity = 2914;

    // The swing plane and the axle line, over open air south of the face.
    static constexpr double kPlaneX = -15.0;
    static constexpr double kPivotY = 251.5;
    static constexpr double kArmLengthM = 28.8;
    static constexpr double kSeatPivotZ = -98.7;
    static constexpr double kRamPivotZ = -95.55;
    // The seat's floor top below its pin, and the deck it is sent to.
    static constexpr double kPinAboveFloorM = 2.4;
    static constexpr double kDeckTopY = 242.25;

    static constexpr float kSeatMassKg = 150.0F;
    static constexpr float kRamMassKg = 235.0F;   // the seat and an 85 kg rider
    static constexpr float kArmMassKg = 200.0F;
    static constexpr float kKickBarMassKg = 4.0F;

    // Rubber buffer on the ram's face: a spring that only pushes, and its
    // loss. Its free length closes the gap between the two hanging plumb.
    static constexpr double kBufferStiffnessNPerM = 25000.0;
    static constexpr double kBufferDampingNsPerM = 80.0;
    static constexpr double kBufferFreeLengthM = 2.0;
    static constexpr double kRamRadiusM = 0.35;
    static constexpr double kSeatHalfDepthM = 0.8;

    // The ram's arm on its hook, south of plumb (negative is south).
    static constexpr double kRamHeldAngleRad = -1.44;
    // The rack's teeth, by the seat arm's angle (positive is north, toward
    // the face); the pawl rests on the last one it passed.
    static constexpr double kFirstToothRad = 1.20;
    static constexpr double kToothPitchRad = 0.006;
    static constexpr int kTeeth = 30;
    static constexpr double kSeatTopStopRad = 1.42;

    // The rider's kick: a leg's push on the bar at the seat's front.
    static constexpr double kKickForceN = 500.0;
    static constexpr double kKickSeconds = 0.35;
    static constexpr double kKickTravelM = 0.15;
    static constexpr double kTripTravelM = 0.10;
    static constexpr double kKickReturnNPerM = 2000.0;

    struct State final {
        bool station_available = false; // standing in the seat, unharnessed
        bool seated = false;
        bool may_leave = true;          // the harness opens: before the kick, or at rest on the rack
        bool ram_held = true;
        bool tripped = false;
        bool kicking = false;
        double kick_elapsed_s = 0.0;
        double kick_work_j = 0.0;
        double kick_travel_m = 0.0;
        int tooth = -1;                 // -1: the pawl rests on no tooth
        double seat_angle_rad = 0.0;
        double ram_angle_rad = kRamHeldAngleRad;
        double seat_speed_mps = 0.0;
        double ram_speed_mps = 0.0;
        double seat_floor_y = 0.0;
        double apex_floor_y = 0.0;      // the highest the floor has been
        double buffer_compression_m = 0.0;
        double buffer_force_n = 0.0;
        double peak_buffer_force_n = 0.0;
        double peak_seat_accel_mps2 = 0.0;
        double buffer_loss_j = 0.0;
        double buffer_stored_j = 0.0;
        double mechanical_j = 0.0;      // the machine's bodies (and a rider)
        double initial_mechanical_j = 0.0;
        double energy_residual_j = 0.0;
        bool ledger_valid = false;
    };

    Swing(JPH::PhysicsSystem &system, kit::Kit &kit, JPH::BodyID player);
    ~Swing() override;
    Swing(const Swing &) = delete;
    Swing &operator=(const Swing &) = delete;

    void pre_step(float delta_seconds, bool action, bool drop);
    void post_step(float delta_seconds);
    [[nodiscard]] bool controls_player() const noexcept { return harness_ != nullptr; }
    [[nodiscard]] const State &state() const noexcept { return state_; }
    void detach();
    void restore(const State &saved);

    [[nodiscard]] static JPH::RVec3 seat_pivot() noexcept;
    [[nodiscard]] static JPH::RVec3 ram_pivot() noexcept;
    [[nodiscard]] JPH::RVec3 seat_pin() const;
    [[nodiscard]] JPH::RVec3 ram_pin() const;
    [[nodiscard]] kit::BodyIndex seat_body() const noexcept { return seat_; }

private:
    void OnStep(const JPH::PhysicsStepListenerContext &context) override;
    [[nodiscard]] bool player_in_seat() const;
    void attach();
    void release_ram();
    void begin_ride_ledger();
    void hold_ram();
    void set_tooth(int tooth);
    [[nodiscard]] double machine_energy(bool include_rider) const;
    void refresh_state();

    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    JPH::BodyID player_;
    JPH::CollisionGroup saved_player_group_;
    kit::BodyIndex frame_{};
    kit::BodyIndex seat_arm_{};
    kit::BodyIndex seat_{};
    kit::BodyIndex ram_arm_{};
    kit::BodyIndex ram_{};
    kit::BodyIndex kick_bar_{};
    JPH::Ref<JPH::HingeConstraint> seat_hinge_;
    JPH::Ref<JPH::HingeConstraint> ram_hinge_;
    JPH::Ref<JPH::PointConstraint> seat_pin_;
    JPH::Ref<JPH::PointConstraint> ram_pin_;
    JPH::Ref<JPH::SliderConstraint> kick_slide_;
    JPH::Ref<JPH::SixDOFConstraint> harness_;
    State state_{};
    bool kick_requested_ = false;
    JPH::Vec3 previous_seat_velocity_ = JPH::Vec3::sZero();
    // Written by the physics step, read after it.
    std::atomic<double> step_buffer_force_n_{0.0};
    std::atomic<double> step_buffer_compression_m_{0.0};
    std::atomic<double> step_buffer_loss_j_{0.0};
    std::atomic<double> step_buffer_stored_j_{0.0};
};

} // namespace scraperx::sim
