#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <vector>

namespace scraperx::sim {

// Opt-in, single-threaded pre-physics controller. Owns NO bodies, constraints,
// transforms or velocities. The world owns all borrowed handles and must detach
// this controller before destroying them. All distances are metres, forces N.
class SharedSurvival final {
public:
    struct Support final {
        JPH::BodyID structure;
        // The hand is a compliant contact patch fixed in the actor's frame.
        JPH::Vec3 hand_local{0.65F, 0.65F, 0.0F};
        float feet_below_com = 1.05F;
        float stance_radius = 0.28F;
        float grip_n = 1600.0F;
        float stiffness_n_m = 40000.0F;
        float damping_ns_m = 600.0F;
        float sole_friction = 0.8F; // planted feet; original locomotion friction restored on release
    };
    enum class Effort { Free, Bracing, AtCapacity };
    enum class Restart { Started, Isolated, Moving, HandsOccupied, GuardOpen, Unavailable };
    struct Snapshot final {
        Effort effort = Effort::Free;
        bool hands_busy = false;
        bool footing = false;
        float applied_n = 0.0F;
        float capacity_n = 0.0F;
        float demand_n = 0.0F;
        JPH::RVec3 hand = JPH::RVec3::sZero();
    };
    struct Drive final {
        // Hinge must join a static frame (body 1) to rotor (body 2).
        JPH::HingeConstraint *hinge = nullptr;
        // A separate physical hinged guard; closed angle is its build pose.
        JPH::HingeConstraint *guard = nullptr;
        float speed_rad_s = 1.0F;
        float torque_nm = 150.0F;
        float off_brake_nm = 0.0F; // zero coasts; finite friction dissipates energy
    };
    struct DriveView final {
        bool running = false;
        float speed_rad_s = 0.0F;
        float angle_rad = 0.0F;
        // Mechanical speed, for presentation/audio; not fabricated sound input.
        float motion_fraction = 0.0F;
    };

    SharedSurvival(JPH::PhysicsSystem &physics, Support support);
    ~SharedSurvival();
    SharedSurvival(const SharedSurvival &) = delete;
    SharedSurvival &operator=(const SharedSurvival &) = delete;
    void release_hands();
    std::size_t add_drive(Drive drive);
    void isolate();
    void restore_supply(); // no automatic restart
    Restart start(std::size_t channel);
    void stop(std::size_t channel);
    void set_brake(std::size_t channel, float torque_nm);
    // After a checkpoint restore only: the channel as it was saved, running
    // or not, without a fresh start's checks (its bodies were restored with
    // it; restoring is not starting).
    void restore_running(std::size_t channel, bool running);
    // Called once per physics tick, before Jolt::Update, with the actual actor.
    // Eligibility is AI intent, never a substitute for contact or footing.
    // Facing rotates the authored hand offset inside the body frame (Matron
    // owns yaw separately from her translation-only capsule).
    void pre_step(JPH::BodyID actor, bool eligible, JPH::Quat facing = JPH::Quat::sIdentity());
    [[nodiscard]] const Snapshot &snapshot() const noexcept { return view_; }
    [[nodiscard]] DriveView drive_view(std::size_t channel) const;
    [[nodiscard]] bool supplied() const noexcept { return supplied_; }
    // Called after checkpoint restore: transient hand effort cannot survive it;
    // electrical starts must be deliberately reissued after revalidation.
    void reset();

private:
    struct Channel {
        Drive drive;
        JPH::Ref<JPH::HingeConstraint> hinge, guard; // keep handles alive after removal
        JPH::BodyID rotor, frame, guard1, guard2;
        bool running = false;
    };
    bool available(const Channel &channel) const;
    void release_footing();
    JPH::BodyID planted_actor_;
    float original_friction_ = 0.0F;
    bool guard_closed(const Channel &channel) const;
    void configure(Channel &channel);
    JPH::PhysicsSystem &physics_;
    Support support_;
    Snapshot view_;
    std::vector<Channel> channels_;
    bool supplied_ = false;
};
} // namespace scraperx::sim
