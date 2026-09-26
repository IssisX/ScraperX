#include "sim/water_screw.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTau = 2.0 * kPi;

[[nodiscard]] double clamp01(const double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double sign_of(const double value, const double fallback) noexcept {
    if (value > 1.0e-9) return 1.0;
    if (value < -1.0e-9) return -1.0;
    return fallback >= 0.0 ? 1.0 : -1.0;
}

} // namespace

WaterScrew::WaterScrew(const WaterScrewConfig &config) noexcept : config_(config) {
    state_.motor_torque_limit_nm = config_.rated_motor_torque_nm;
    state_.basin_volume_m3 =
        std::clamp(config_.initial_basin_volume_m3, 0.0, config_.basin_capacity_m3);
}

void WaterScrew::toggle_motor() noexcept {
    state_.motor_enabled = !state_.motor_enabled;
}

void WaterScrew::set_motor_torque_limit_nm(const double torque_nm) noexcept {
    if (std::isfinite(torque_nm)) {
        state_.motor_torque_limit_nm = std::max(0.0, torque_nm);
    }
}

void WaterScrew::set_outlet_blocked(const bool blocked) noexcept {
    state_.outlet_blocked = blocked;
}

void WaterScrew::set_drive_direction(const int direction) noexcept {
    state_.drive_direction = direction < 0 ? -1 : 1;
}

void WaterScrew::set_basin_volume_m3(const double volume_m3) noexcept {
    if (std::isfinite(volume_m3)) {
        state_.basin_volume_m3 = std::clamp(volume_m3, 0.0, config_.basin_capacity_m3);
    }
}

double WaterScrew::withdraw_tank_volume_m3(const double requested_m3) noexcept {
    if (!std::isfinite(requested_m3) || requested_m3 <= 0.0) return 0.0;
    const double moved = std::min(requested_m3, state_.tank_volume_m3);
    state_.tank_volume_m3 -= moved;
    return moved;
}

double WaterScrew::return_to_basin_m3(const double requested_m3) noexcept {
    if (!std::isfinite(requested_m3) || requested_m3 <= 0.0) return 0.0;
    const double room = config_.basin_capacity_m3 - state_.basin_volume_m3;
    const double moved = std::min(requested_m3, std::max(0.0, room));
    state_.basin_volume_m3 += moved;
    return moved;
}

double WaterScrew::effective_displacement_m3_per_rev() const noexcept {
    const double annulus_area =
        kPi * 0.25 *
        (config_.outside_diameter_m * config_.outside_diameter_m -
         config_.core_diameter_m * config_.core_diameter_m);
    return annulus_area * config_.pitch_m * config_.fill_factor *
           (1.0 - config_.intrinsic_leak_fraction);
}

double WaterScrew::inlet_immersion_fraction() const noexcept {
    const double depth = state_.basin_volume_m3 / config_.basin_area_m2;
    const double span = config_.full_inlet_depth_m - config_.minimum_inlet_depth_m;
    if (!(span > 0.0)) return 0.0;
    return clamp01((depth - config_.minimum_inlet_depth_m) / span);
}

double WaterScrew::current_head_m() const noexcept {
    const double basin_surface_y =
        std::clamp(state_.basin_volume_m3 / config_.basin_area_m2,
                   0.0, config_.basin_capacity_m3 / config_.basin_area_m2);
    return std::max(0.5, config_.outlet_elevation_m - basin_surface_y);
}

void WaterScrew::step(const double delta_seconds) noexcept {
    if (!std::isfinite(delta_seconds) || !(delta_seconds > 0.0)) return;

    const double nominal_omega = config_.nominal_rpm * kTau / 60.0;
    const double target_omega =
        state_.motor_enabled ? static_cast<double>(state_.drive_direction) * nominal_omega : 0.0;
    const double immersion = inlet_immersion_fraction();
    const double effective_displacement = effective_displacement_m3_per_rev();
    const double head = current_head_m();

    const double hydraulic_torque =
        config_.water_density_kg_m3 * config_.gravity_m_s2 * head *
        effective_displacement /
        (std::max(0.05, config_.hydraulic_efficiency) * kTau) *
        immersion * config_.drivetrain_loss_factor;
    const double speed_fraction =
        nominal_omega > 0.0 ? std::min(1.0, std::abs(state_.shaft_angular_velocity_rad_s) /
                                                nominal_omega) : 0.0;
    const double bearing_load =
        config_.bearing_torque_nm + config_.viscous_drag_torque_nm * speed_fraction;
    double process_load = 0.0;
    if (state_.drive_direction > 0 && immersion > 0.0) {
        process_load = hydraulic_torque;
    } else if (state_.drive_direction < 0 && state_.tank_volume_m3 > 0.0) {
        process_load = hydraulic_torque * 0.25;
    }
    const double resisting_torque = bearing_load + process_load;

    double motor_torque = 0.0;
    if (state_.motor_enabled) {
        const double requested =
            (target_omega - state_.shaft_angular_velocity_rad_s) *
                config_.rotor_inertia_kg_m2 * 3.0 +
            sign_of(target_omega, 1.0) * resisting_torque;
        motor_torque = std::clamp(requested, -state_.motor_torque_limit_nm,
                                  state_.motor_torque_limit_nm);
    }

    const double resistance_sign =
        std::abs(state_.shaft_angular_velocity_rad_s) > 1.0e-5
            ? sign_of(state_.shaft_angular_velocity_rad_s, target_omega)
            : (state_.motor_enabled ? sign_of(target_omega, 1.0) : 0.0);
    const double net_torque = motor_torque - resistance_sign * resisting_torque;
    double next_omega =
        state_.shaft_angular_velocity_rad_s +
        net_torque / std::max(1.0, config_.rotor_inertia_kg_m2) * delta_seconds;

    if (!state_.motor_enabled &&
        state_.shaft_angular_velocity_rad_s * next_omega <= 0.0) next_omega = 0.0;
    if (state_.motor_enabled && target_omega > 0.0) {
        if (next_omega < 0.0) next_omega = 0.0;
        if (state_.shaft_angular_velocity_rad_s <= target_omega &&
            next_omega > target_omega) next_omega = target_omega;
    } else if (state_.motor_enabled && target_omega < 0.0) {
        if (state_.shaft_angular_velocity_rad_s >= target_omega &&
            next_omega < target_omega) next_omega = target_omega;
    }

    state_.shaft_angular_velocity_rad_s = next_omega;
    state_.shaft_angle_radians =
        std::fmod(state_.shaft_angle_radians + next_omega * delta_seconds, kTau);
    if (state_.shaft_angle_radians < 0.0) state_.shaft_angle_radians += kTau;
    state_.shaft_rpm = next_omega * 60.0 / kTau;
    state_.motor_torque_nm = motor_torque;
    state_.delivered_flow_m3_s = 0.0;

    const double revolutions_per_second = std::abs(next_omega) / kTau;
    const double annulus_area =
        kPi * 0.25 *
        (config_.outside_diameter_m * config_.outside_diameter_m -
         config_.core_diameter_m * config_.core_diameter_m);
    const double geometric_displacement =
        annulus_area * config_.pitch_m * config_.fill_factor;

    if (next_omega > 1.0e-6 && immersion > 0.0) {
        const double gross_flow = geometric_displacement * revolutions_per_second * immersion;
        const double intrinsic_leak = gross_flow * config_.intrinsic_leak_fraction;
        const double potential_delivery = std::max(0.0, gross_flow - intrinsic_leak);
        double transfer = 0.0;
        if (!state_.outlet_blocked && state_.tank_volume_m3 < config_.tank_capacity_m3) {
            transfer = std::min({potential_delivery * delta_seconds,
                                 state_.basin_volume_m3,
                                 config_.tank_capacity_m3 - state_.tank_volume_m3});
            state_.basin_volume_m3 -= transfer;
            state_.tank_volume_m3 += transfer;
        }
        state_.delivered_flow_m3_s = transfer / delta_seconds;
        state_.cumulative_leakage_or_recycle_m3 +=
            intrinsic_leak * delta_seconds +
            std::max(0.0, potential_delivery * delta_seconds - transfer);
        state_.hydraulic_work_j +=
            transfer * config_.water_density_kg_m3 * config_.gravity_m_s2 * head;
    } else if (next_omega < -1.0e-6 && state_.tank_volume_m3 > 0.0) {
        const double return_capacity = effective_displacement * revolutions_per_second * 0.80;
        const double transfer = std::min({return_capacity * delta_seconds,
                                          state_.tank_volume_m3,
                                          config_.basin_capacity_m3 - state_.basin_volume_m3});
        state_.tank_volume_m3 -= transfer;
        state_.basin_volume_m3 += transfer;
        state_.delivered_flow_m3_s = -transfer / delta_seconds;
    }

    state_.shaft_work_j += std::abs(motor_torque * next_omega) * delta_seconds;
}

} // namespace scraperx::sim
