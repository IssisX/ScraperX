#pragma once

// Phase 3: the yard's trapeze. A bar on two 7 m rods hangs plumb from a
// gantry east of the game's start. West of it a launch stand (top 2.2 m); east,
// a landing stand (top 5.0 m), out of reach of the swing a running catch gives.
//
// Catching: in the air with the bar in the hands' reach, GRAB closes them on
// it. The hands join the body's centre of mass to the bar with a constraint:
// fixed across the arm and out of the swing plane; along the arm the arms are
// a stiff spring at their straight stop (they give under a hard catch, then
// hold), and a motor of at most kPumpForceN that can only pull the body up
// toward the bar (Crouch held), never push it away. More than the hands can
// hold (kGripMaxForceN) for kSlipTicks and they slip off.
//
// Pumping: pulling up near the bottom, where the arm's tension is greatest,
// and letting down near the ends puts work in; the other way round takes it
// out; holding still does nothing. The ledger records the arms' work from the
// motor's impulse against the mechanical energy of bar and body.
//
// Letting go removes the constraint and nothing else: the body keeps its
// velocity. A light friction at the axle settles an empty bar back to plumb.
// Jolt owns every body and contact; this owner adds the hands.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>

namespace scraperx::sim {

class Trapeze final {
public:
    static constexpr std::uint64_t kFrameEntity = 1998;
    static constexpr std::uint64_t kBarEntity = 2951;

    // The swing plane is east-west through z = kPlaneZ; angles are east of
    // plumb.
    static constexpr double kPivotX = 34.0;
    static constexpr double kPivotY = 12.0;
    static constexpr double kPlaneZ = -28.0;
    static constexpr double kArmLengthM = 7.0;
    static constexpr double kLaunchTopY = 2.2;
    static constexpr double kLaunchEastX = 31.0;
    static constexpr double kLandingTopY = 5.0;
    static constexpr double kLandingWestX = 39.5;
    static constexpr float kBarMassKg = 30.0F;
    // The axle's friction: settles an empty bar, CHOSEN small against a
    // rider's swing (about 1 % of its energy a swing).
    static constexpr float kAxleFrictionNm = 8.0F;

    // The hands' reach for a catch: the bar between kCatchLowM and
    // kCatchHighM above the body's centre, within kCatchAcrossM of it along
    // the swing and kCatchSideM out of the plane.
    static constexpr double kCatchLowM = 0.2;
    static constexpr double kCatchHighM = 1.2;
    static constexpr double kCatchAcrossM = 0.6;
    static constexpr double kCatchSideM = 0.5;
    // The arms: straight, the body's centre hangs kHangLengthM below the bar;
    // pulled up, kPullTravelM nearer, by at most kPumpForceN (CHOSEN: 1.8
    // body weights). The straight stop gives as a spring (kArmSpringHz,
    // kArmDamping: a catch is taken over about a tenth of a second).
    static constexpr double kHangLengthM = 1.15;
    static constexpr double kPullTravelM = 0.6;
    static constexpr float kPumpForceN = 1500.0F;
    static constexpr float kArmSpringHz = 2.0F;
    static constexpr float kArmDamping = 1.0F;
    // What two hands hold: past it for this long, they slip.
    static constexpr float kGripMaxForceN = 3000.0F;
    static constexpr int kSlipTicks = 14;
    // GRAB pressed early is held this long (native ticks, 0.33 s): the hands
    // close the moment the bar comes into reach. Offered from kNearM.
    static constexpr int kGrabBufferTicks = 30;
    static constexpr double kNearM = 1.6;

    struct State final {
        bool catch_available = false;   // airborne, the bar in the hands' reach
        bool catch_near = false;        // airborne, closing on the bar: GRAB is held for it
        bool harnessed = false;
        bool pulled = false;            // the arms asked to pull up
        double angle_rad = 0.0;
        double angular_speed = 0.0;
        double peak_angle_rad = 0.0;    // the furthest east since the catch
        double pulled_in_m = 0.0;       // how far the body is up its arm from straight
        double catch_speed_mps = 0.0;   // the body's speed relative to the bar at the catch
        double catch_loss_j = 0.0;      // what the catch took out
        double hand_force_n = 0.0;      // the hands' load this step
        double peak_hand_force_n = 0.0;
        double arm_work_j = 0.0;        // the arms' net work since the catch settled
        double arm_work_out_j = 0.0;    // of which taken out (negative work)
        double mechanical_j = 0.0;
        double initial_mechanical_j = 0.0;
        double energy_residual_j = 0.0;
        int pumps = 0;                  // pulls begun while hanging
        int catches = 0;
        int slips = 0;
    };

    Trapeze(JPH::PhysicsSystem &system, kit::Kit &kit, JPH::BodyID player);
    ~Trapeze();
    Trapeze(const Trapeze &) = delete;
    Trapeze &operator=(const Trapeze &) = delete;

    // grab: close the hands on the bar (in reach, airborne); drop: let go;
    // pull: the arms pulled up (held).
    void pre_step(float delta_seconds, bool grab, bool drop, bool pull, bool airborne);
    void post_step(float delta_seconds);
    [[nodiscard]] bool controls_player() const noexcept { return harness_ != nullptr; }
    [[nodiscard]] const State &state() const noexcept { return state_; }
    void detach();
    // After a death or a load: hands off, the bar where the kit put it.
    void restore();

    [[nodiscard]] static JPH::RVec3 pivot() noexcept;
    [[nodiscard]] JPH::RVec3 bar_position() const;

private:
    [[nodiscard]] bool bar_in_reach() const;
    [[nodiscard]] double player_radius() const;
    // The harness's coordinate up the arm: the body's centre in the bar's
    // frame along the constraint's y axis.
    [[nodiscard]] double arm_coordinate() const;
    // The body's speed up the arm relative to the bar.
    [[nodiscard]] double arm_rate() const;
    void attach();
    [[nodiscard]] double mechanical_energy(bool include_rider) const;
    void refresh_state();

    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    JPH::BodyID player_;
    JPH::CollisionGroup saved_player_group_;
    kit::BodyIndex frame_{};
    kit::BodyIndex bar_{};
    JPH::Ref<JPH::HingeConstraint> hinge_;
    JPH::Ref<JPH::SixDOFConstraint> harness_;
    State state_{};
    bool airborne_ = false;
    double previous_pulled_in_m_ = 0.0;
    double rate_before_step_ = 0.0;
    double straight_radius_m_ = 0.0;   // the body's distance from the pivot, arms straight
    double catch_energy_j_ = 0.0;      // bar and body just before the hands closed
    int ledger_wait_ticks_ = 0;        // the catch settling before the ledger opens
    int over_grip_ticks_ = 0;
    int grab_pending_ticks_ = 0;
    float straight_target_ = 0.0F;     // the harness's y at the straight stop and pulled up
    float pulled_target_ = 0.0F;
    JPH::Vec3 arm_axis_local_{JPH::Vec3::sAxisY()};   // the constraint's y in the bar's frame
    JPH::Vec3 arm_origin_local_{JPH::Vec3::sZero()};  // where the body was held, in the bar's frame
};

} // namespace scraperx::sim
