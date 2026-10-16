#pragma once
#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <memory>
namespace scraperx::sim {
// Owner thread: create/destroy/pre_step/post_step/restore outside Update;
// collision_step once per actual collision substep in the serialized listener.
// Clear this owner before either attachment body is removed. No own listener.
class PoweredCable final {
public:
    struct Config {
        JPH::BodyID support, load;
        JPH::Vec3 support_eye = JPH::Vec3::sZero(); // shape-origin local
        JPH::Vec3 load_eye = JPH::Vec3::sZero();
        float initial_length_m = 1, min_length_m = .1F, max_length_m = 100;
        float max_speed_mps = .35F, pull_force_n = 50000, brake_force_n = 50000;
        double electrical_power_w = 25000, efficiency = .8, capacity_j = 5e6;
    };
    struct State {
        float actual_length_m = 0, rest_length_m = 0, tensile_force_n = 0;
        float peak_force_n = 0, last_impulse_ns = 0, last_collision_dt = 0;
        float target_speed_mps = 0; // signed separation speed; retract is negative
        JPH::RVec3 support_endpoint = JPH::RVec3::sZero();
        JPH::RVec3 load_endpoint = JPH::RVec3::sZero();
        JPH::Vec3 load_force = JPH::Vec3::sZero();
        double energy_j = 0, source_energy_used_j = 0, positive_motor_work_j = 0;
        double absorbed_work_j = 0, heat_j = 0, command_work_bound_j = 0;
        double electrical_power_w = 0, peak_electrical_w = 0, energy_overdraft_j = 0;
        bool energy_cutoff = false, braking = true, at_min = false, at_max = false;
    };
    PoweredCable(JPH::PhysicsSystem &, const Config &);
    ~PoweredCable();
    PoweredCable(const PoweredCable &) = delete;
    PoweredCable &operator=(const PoweredCable &) = delete;
    void pre_step(float effort); // positive retracts; negative pays out
    void collision_step(float dt);
    void post_step();
    [[nodiscard]] State state() const;
    // Body snapshot is restored separately by owner. Warm start is always zero.
    void restore(const State &);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace scraperx::sim
#endif
