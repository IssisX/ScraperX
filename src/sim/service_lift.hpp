#pragma once

#include "sim/mechanism_kit.hpp"
#include <memory>

namespace scraperx::sim {

// AS-027. Kit owns all rigid bodies; this module owns only their joints and
// finite drive state. Called serially by the existing PhysicsWorld owner.
class ServiceLift final {
public:
    static constexpr std::uint64_t kFrameEntity = 1970;
    static constexpr std::uint64_t kGuideEntity = 1971;
    static constexpr std::uint64_t kLandingEntity = 1972;
    static constexpr std::uint64_t kDeckEntity = 2973;
    enum class Station : std::uint8_t { None, Deck, Lower, Upper };
    struct State {
        double energy_j = 0;
        double positive_work_j = 0;
        double heat_j = 0;
        double peak_electrical_w = 0;
        double energy_overdraft_j = 0;
        double actuator_force_n = 0;
        double electrical_power_w = 0;
        float target_speed_mps = 0;
        bool energy_cutoff = false;
        bool braking = true;
    };
    struct Design {
        double moving_mass_kg = 0;
        double effective_vertical_mass_kg = 0;
        double rated_force_n = 0;
        double brake_force_n = 0;
        double electrical_rating_w = 0;
        double capacity_j = 0;
        double stroke_m = 0;
    };
    ServiceLift(JPH::PhysicsSystem &world, kit::Kit &kit);
    ~ServiceLift();
    ServiceLift(const ServiceLift &) = delete;
    ServiceLift &operator=(const ServiceLift &) = delete;
    // The host checks reach, support and sight before supplying effort.
    void pre_step(float effort);
    void collision_step(float dt);
    void post_step();
    [[nodiscard]] State state() const;
    void restore(const State &state);
    [[nodiscard]] Design design() const;
    [[nodiscard]] JPH::RVec3 station_position(Station station) const;
    [[nodiscard]] double walking_surface_y() const;
    [[nodiscard]] kit::BodyIndex deck() const;
    [[nodiscard]] static bool owns_support(std::uint64_t entity) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace scraperx::sim
