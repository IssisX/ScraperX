#pragma once

namespace scraperx::sim {

struct WaterScrewConfig final {
    double water_density_kg_m3 = 1000.0;
    double gravity_m_s2 = 9.81;
    double outside_diameter_m = 1.20;
    double core_diameter_m = 0.25;
    double pitch_m = 0.80;
    double fill_factor = 0.31;
    double intrinsic_leak_fraction = 0.087;
    double incline_radians = 0.5235987755982988;
    double flighted_length_m = 11.0;
    double outlet_elevation_m = 5.5;
    double hydraulic_efficiency = 0.55;
    double drivetrain_loss_factor = 1.03;
    double nominal_rpm = 18.0;
    double rated_motor_torque_nm = 5000.0;
    double rotor_inertia_kg_m2 = 500.0;
    double bearing_torque_nm = 180.0;
    double viscous_drag_torque_nm = 100.0;
    double basin_area_m2 = 2.0;
    double basin_capacity_m3 = 2.5;
    double initial_basin_volume_m3 = 2.4;
    double minimum_inlet_depth_m = 0.08;
    double full_inlet_depth_m = 0.18;
    double tank_area_m2 = 4.0;
    double tank_capacity_m3 = 2.0;
};

struct WaterScrewState final {
    bool motor_enabled = false;
    bool outlet_blocked = false;
    int drive_direction = 1;
    double motor_torque_limit_nm = 5000.0;
    double shaft_angle_radians = 0.0;
    double shaft_angular_velocity_rad_s = 0.0;
    double shaft_rpm = 0.0;
    double motor_torque_nm = 0.0;
    double delivered_flow_m3_s = 0.0;
    double basin_volume_m3 = 2.4;
    double tank_volume_m3 = 0.0;
    double cumulative_leakage_or_recycle_m3 = 0.0;
    double shaft_work_j = 0.0;
    double hydraulic_work_j = 0.0;
};

class WaterScrew final {
public:
    explicit WaterScrew(const WaterScrewConfig &config = {}) noexcept;

    void toggle_motor() noexcept;
    void set_motor_torque_limit_nm(double torque_nm) noexcept;
    void set_outlet_blocked(bool blocked) noexcept;
    void set_drive_direction(int direction) noexcept;
    void set_basin_volume_m3(double volume_m3) noexcept;
    [[nodiscard]] double withdraw_tank_volume_m3(double requested_m3) noexcept;
    [[nodiscard]] double return_to_basin_m3(double requested_m3) noexcept;
    void step(double delta_seconds) noexcept;

    [[nodiscard]] const WaterScrewState &state() const noexcept { return state_; }
    [[nodiscard]] const WaterScrewConfig &config() const noexcept { return config_; }
    [[nodiscard]] double effective_displacement_m3_per_rev() const noexcept;

private:
    [[nodiscard]] double inlet_immersion_fraction() const noexcept;
    [[nodiscard]] double current_head_m() const noexcept;

    WaterScrewConfig config_{};
    WaterScrewState state_{};
};

} // namespace scraperx::sim
