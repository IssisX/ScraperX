#pragma once

#include "sim/water_screw.hpp"

namespace scraperx::sim {

struct GroundLiftConfig final {
    double fixed_step_s = 1.0 / 90.0;
    double cage_travel_m = 8.0;
    double cage_mass_kg = 500.0;
    double rider_mass_kg = 80.0;
    double bucket_dry_mass_kg = 200.0;
    double transmission_mass_kg = 100.0;
    double bucket_capacity_m3 = 2.0;
    double valve_flow_m3_s = 0.22;
    double drain_flow_m3_s = 0.22;
    double static_friction_n = 700.0;
    double kinetic_friction_n = 400.0;
    double governor_n_per_m_s = 3200.0;
    double terminal_brake_n_per_m_s = 30000.0;
};

struct GroundLiftState final {
    bool valve_open = false;
    bool bucket_top_catch_latched = true;
    bool upper_catch_latched = false;
    bool rider_on_cage = false;
    double bucket_water_m3 = 0.0;
    double valve_flow_m3_s = 0.0;
    double cage_travel_m = 0.0;
    double cage_speed_m_s = 0.0;
    double peak_cage_speed_m_s = 0.0;
    double rope_tension_n = 0.0;
    double gravity_work_j = 0.0;
    double dissipated_work_j = 0.0;
};

// One owner for the whole first ascent: water, bucket, rope ratio, cage and catches.
// q is the cage's rise; the bucket descends q/2 by the same rope constraint.
class GroundStage final {
public:
    explicit GroundStage(const GroundLiftConfig &lift_config = {},
                         const WaterScrewConfig &screw_config = {}) noexcept;

    [[nodiscard]] bool advance_frame(double frame_seconds) noexcept;
    [[nodiscard]] bool request_motor_toggle(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_reverse(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_valve_toggle(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_release(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_reset(double x, double y, double z) noexcept;
    void set_rider_on_cage(bool aboard) noexcept;

    [[nodiscard]] bool at_pump_station(double x, double y, double z) const noexcept;
    [[nodiscard]] bool at_cage_control(double x, double y, double z) const noexcept;
    [[nodiscard]] bool at_upper_control(double x, double y, double z) const noexcept;
    [[nodiscard]] const WaterScrewState &state() const noexcept { return screw_.state(); }
    [[nodiscard]] const GroundLiftState &lift_state() const noexcept { return lift_; }
    [[nodiscard]] double total_water_m3() const noexcept;
    [[nodiscard]] double lift_energy_residual_j() const noexcept;

private:
    void step(double h) noexcept;
    void step_lift(double h) noexcept;

    GroundLiftConfig config_{};
    WaterScrew screw_{};
    GroundLiftState lift_{};
    double accumulator_s_ = 0.0;
    bool lift_released_ = false;
};

} // namespace scraperx::sim
