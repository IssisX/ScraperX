#include "sim/ground_stage.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {

constexpr double kGravity = 9.81;
constexpr double kWaterDensity = 1000.0;
constexpr double kStationRadiusSquared = 2.4 * 2.4;

bool near_station(double x, double y, double z,
                  double sx, double sy, double sz) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    const double dx = x - sx;
    const double dy = y - sy;
    const double dz = z - sz;
    return dx * dx + dy * dy + dz * dz <= kStationRadiusSquared;
}

} // namespace

GroundStage::GroundStage(const GroundLiftConfig &lift_config,
                         const WaterScrewConfig &screw_config) noexcept
    : config_(lift_config), screw_(screw_config) {}

bool GroundStage::at_pump_station(double x, double y, double z) const noexcept {
    return near_station(x, y, z, 3.5, 0.0, 0.5);
}

bool GroundStage::at_cage_control(double x, double y, double z) const noexcept {
    return near_station(x, y, z, 4.0, lift_.cage_travel_m, -9.0);
}

bool GroundStage::at_upper_control(double x, double y, double z) const noexcept {
    return near_station(x, y, z, 8.0, 8.0, -9.0);
}

bool GroundStage::request_motor_toggle(double x, double y, double z) noexcept {
    if (!at_pump_station(x, y, z)) return false;
    screw_.toggle_motor();
    return true;
}

bool GroundStage::request_reverse(double x, double y, double z) noexcept {
    if (!at_pump_station(x, y, z) || screw_.state().motor_enabled) return false;
    screw_.set_drive_direction(-screw_.state().drive_direction);
    return true;
}

bool GroundStage::request_valve_toggle(double x, double y, double z) noexcept {
    if (!at_pump_station(x, y, z) || !lift_.bucket_top_catch_latched) return false;
    lift_.valve_open = !lift_.valve_open;
    return true;
}

bool GroundStage::request_release(double x, double y, double z) noexcept {
    if (!at_cage_control(x, y, z) || !lift_.rider_on_cage ||
        !lift_.bucket_top_catch_latched || lift_.upper_catch_latched) return false;
    lift_.bucket_top_catch_latched = false;
    lift_released_ = true;
    lift_.valve_open = false;
    return true;
}

bool GroundStage::request_reset(double x, double y, double z) noexcept {
    if (!(at_upper_control(x, y, z) || at_pump_station(x, y, z)) ||
        !lift_.upper_catch_latched ||
        lift_.rider_on_cage || lift_.bucket_water_m3 > 1e-6) return false;
    lift_.upper_catch_latched = false;
    lift_released_ = true;
    return true;
}

void GroundStage::set_rider_on_cage(bool aboard) noexcept {
    lift_.rider_on_cage = aboard;
}

bool GroundStage::advance_frame(double frame_seconds) noexcept {
    if (!std::isfinite(frame_seconds) || frame_seconds <= 0.0 ||
        frame_seconds > 0.25 || config_.fixed_step_s <= 0.0) return false;
    accumulator_s_ += frame_seconds;
    while (accumulator_s_ + 1e-12 >= config_.fixed_step_s) {
        step(config_.fixed_step_s);
        accumulator_s_ -= config_.fixed_step_s;
    }
    if (accumulator_s_ < 0.0) accumulator_s_ = 0.0;
    return true;
}

void GroundStage::step(double h) noexcept {
    screw_.step(h);
    lift_.valve_flow_m3_s = 0.0;
    if (lift_.valve_open && lift_.bucket_top_catch_latched &&
        lift_.bucket_water_m3 < config_.bucket_capacity_m3) {
        const double request = std::min(config_.valve_flow_m3_s * h,
                                        config_.bucket_capacity_m3 - lift_.bucket_water_m3);
        const double moved = screw_.withdraw_tank_volume_m3(request);
        lift_.bucket_water_m3 += moved;
        lift_.valve_flow_m3_s = moved / h;
    }
    if (lift_.upper_catch_latched && lift_.bucket_water_m3 > 0.0) {
        const double request = std::min(config_.drain_flow_m3_s * h,
                                        lift_.bucket_water_m3);
        const double moved = screw_.return_to_basin_m3(request);
        lift_.bucket_water_m3 -= moved;
        lift_.valve_flow_m3_s = -moved / h;
    }
    step_lift(h);
}

void GroundStage::step_lift(double h) noexcept {
    if (!lift_released_ || lift_.upper_catch_latched) {
        lift_.cage_speed_m_s = 0.0;
        return;
    }

    const double bucket_mass = config_.bucket_dry_mass_kg +
                               kWaterDensity * lift_.bucket_water_m3;
    const double cage_mass = config_.cage_mass_kg +
                             (lift_.rider_on_cage ? config_.rider_mass_kg : 0.0);
    const double effective_mass = cage_mass + bucket_mass * 0.25 +
                                  config_.transmission_mass_kg;
    const double drive = (bucket_mass * 0.5 - cage_mass) * kGravity;
    double v = lift_.cage_speed_m_s;
    double q = lift_.cage_travel_m;
    if (std::abs(v) < 1e-6 && std::abs(drive) <= config_.static_friction_n) {
        lift_.cage_speed_m_s = 0.0;
        if (q <= 1e-6) {
            lift_.bucket_top_catch_latched = true;
            lift_released_ = false;
        }
        return;
    }
    const double direction = std::abs(v) >= 1e-6 ? (v > 0.0 ? 1.0 : -1.0)
                                                   : (drive > 0.0 ? 1.0 : -1.0);
    const double governor = config_.governor_n_per_m_s *
                            std::max(0.0, std::abs(v) - 0.4);
    const double terminal_fraction = std::clamp(q - 7.0, 0.0, 1.0);
    const double terminal = v > 0.0 ?
        config_.terminal_brake_n_per_m_s * terminal_fraction * v : 0.0;
    const double resistance = config_.kinetic_friction_n + governor + terminal;
    const double acceleration = (drive - direction * resistance) / effective_mass;
    double next_v = v + acceleration * h;
    if (next_v * direction < 0.0) next_v = 0.0;
    double next_q = q + next_v * h;
    const double travel = std::abs(next_q - q);
    lift_.gravity_work_j += drive * (next_q - q);
    lift_.dissipated_work_j += resistance * travel;
    lift_.rope_tension_n = std::max(0.0, bucket_mass *
        (kGravity - acceleration * 0.5) * 0.5);

    if (next_q >= config_.cage_travel_m) {
        const double overshoot = next_q - config_.cage_travel_m;
        lift_.gravity_work_j -= drive * overshoot;
        lift_.dissipated_work_j -= resistance * overshoot;
        lift_.dissipated_work_j += 0.5 * effective_mass * next_v * next_v;
        next_q = config_.cage_travel_m;
        next_v = 0.0;
        lift_.upper_catch_latched = true;
        lift_released_ = false;
    } else if (next_q <= 0.0) {
        const double overshoot = -next_q;
        lift_.gravity_work_j -= drive * (-overshoot);
        lift_.dissipated_work_j -= resistance * overshoot;
        lift_.dissipated_work_j += 0.5 * effective_mass * next_v * next_v;
        next_q = 0.0;
        next_v = 0.0;
        lift_.bucket_top_catch_latched = true;
        lift_released_ = false;
    }
    q = next_q;
    v = next_v;
    lift_.cage_travel_m = q;
    lift_.cage_speed_m_s = v;
    lift_.peak_cage_speed_m_s =
        std::max(lift_.peak_cage_speed_m_s, std::abs(v));
}

double GroundStage::total_water_m3() const noexcept {
    return screw_.state().basin_volume_m3 + screw_.state().tank_volume_m3 +
           lift_.bucket_water_m3;
}

double GroundStage::lift_energy_residual_j() const noexcept {
    const double bucket_mass = config_.bucket_dry_mass_kg +
                               kWaterDensity * lift_.bucket_water_m3;
    const double cage_mass = config_.cage_mass_kg +
                             (lift_.rider_on_cage ? config_.rider_mass_kg : 0.0);
    const double effective_mass = cage_mass + bucket_mass * 0.25 +
                                  config_.transmission_mass_kg;
    return lift_.gravity_work_j - lift_.dissipated_work_j -
           0.5 * effective_mass * lift_.cage_speed_m_s * lift_.cage_speed_m_s;
}

} // namespace scraperx::sim
