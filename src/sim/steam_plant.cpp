#include "sim/steam_plant.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {

[[nodiscard]] double clamp01(const double value) noexcept {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    return std::clamp(value, 0.0, 1.0);
}

} // namespace

SteamPlant::SteamPlant(const SteamPlantConfig &config) noexcept : config_(config) {
    const double rt = config_.gas_constant_j_per_kg_k * config_.temperature_k;
    state_.vessel_mass_kg = config_.initial_vessel_pressure_pa * config_.vessel_volume_m3 / rt;
    state_.cylinder_mass_kg = config_.ambient_pressure_pa * config_.cylinder_volume_m3 / rt;
    state_.vessel_pressure_pa = config_.initial_vessel_pressure_pa;
    state_.cylinder_pressure_pa = config_.ambient_pressure_pa;
}

void SteamPlant::set_valve_open_fraction(const double fraction) noexcept {
    state_.valve_open_fraction = clamp01(fraction);
}

void SteamPlant::set_feed_enabled(const bool enabled) noexcept {
    feed_enabled_ = enabled;
}

void SteamPlant::restore_state(const double vessel_mass_kg, const double cylinder_mass_kg) noexcept {
    state_.vessel_mass_kg = std::max(0.0, vessel_mass_kg);
    state_.cylinder_mass_kg = std::max(0.0, cylinder_mass_kg);
    const double rt = config_.gas_constant_j_per_kg_k * config_.temperature_k;
    state_.vessel_pressure_pa = state_.vessel_mass_kg * rt / config_.vessel_volume_m3;
    state_.cylinder_pressure_pa = state_.cylinder_mass_kg * rt / config_.cylinder_volume_m3;
    state_.orifice_mass_flow_kg_per_s = 0.0;
    state_.bleed_mass_flow_kg_per_s = 0.0;
    state_.feed_mass_flow_kg_per_s = 0.0;
    state_.piston_force_n = 0.0;
}

// Compressible flow through a thin orifice. Choked above the critical pressure
// ratio, subcritical below it, and identically zero when the pressure difference
// or the area vanishes. No backflow is modelled; a reversed pressure difference
// yields zero rather than a negative flow, which keeps both reservoirs monotone
// under their own clamps.
double SteamPlant::mass_flow_kg_per_s(const double upstream_pressure_pa,
                                      const double downstream_pressure_pa,
                                      const double area_m2,
                                      const double discharge_coefficient) const noexcept {
    if (!(area_m2 > 0.0) || !(upstream_pressure_pa > downstream_pressure_pa)) {
        return 0.0;
    }

    const double gamma = config_.heat_capacity_ratio;
    const double rt = config_.gas_constant_j_per_kg_k * config_.temperature_k;
    if (!(rt > 0.0) || !(gamma > 1.0)) {
        return 0.0;
    }

    const double critical_ratio = std::pow(2.0 / (gamma + 1.0), gamma / (gamma - 1.0));
    const double ratio = downstream_pressure_pa / upstream_pressure_pa;

    double flow_function = 0.0;
    if (ratio <= critical_ratio) {
        flow_function =
            std::sqrt(gamma / rt * std::pow(2.0 / (gamma + 1.0), (gamma + 1.0) / (gamma - 1.0)));
    } else {
        const double term =
            std::pow(ratio, 2.0 / gamma) - std::pow(ratio, (gamma + 1.0) / gamma);
        if (!(term > 0.0)) {
            return 0.0;
        }
        flow_function = std::sqrt(2.0 * gamma / ((gamma - 1.0) * rt) * term);
    }

    const double flow =
        discharge_coefficient * area_m2 * upstream_pressure_pa * flow_function;
    return std::isfinite(flow) && flow > 0.0 ? flow : 0.0;
}

double SteamPlant::vessel_available_energy_j() const noexcept {
    // Internal energy of the gas above the mass that would remain at ambient
    // pressure: u = c_v * T per kilogram, c_v = R / (gamma - 1).
    const double rt = config_.gas_constant_j_per_kg_k * config_.temperature_k;
    const double ambient_mass = config_.ambient_pressure_pa * config_.vessel_volume_m3 / rt;
    const double usable_mass = std::max(0.0, state_.vessel_mass_kg - ambient_mass);
    const double specific_internal_energy =
        config_.gas_constant_j_per_kg_k * config_.temperature_k /
        (config_.heat_capacity_ratio - 1.0);
    return usable_mass * specific_internal_energy;
}

void SteamPlant::step(const double delta_seconds) noexcept {
    if (!std::isfinite(delta_seconds) || delta_seconds <= 0.0) {
        return;
    }

    const double rt = config_.gas_constant_j_per_kg_k * config_.temperature_k;

    const double orifice_area = config_.orifice_area_m2 * state_.valve_open_fraction;
    const double orifice_flow = mass_flow_kg_per_s(state_.vessel_pressure_pa,
                                                   state_.cylinder_pressure_pa,
                                                   orifice_area,
                                                   config_.orifice_discharge_coefficient);
    const double bleed_flow = mass_flow_kg_per_s(state_.cylinder_pressure_pa,
                                                 config_.ambient_pressure_pa,
                                                 config_.cylinder_bleed_area_m2,
                                                 config_.cylinder_bleed_discharge_coefficient);

    double feed_flow = 0.0;
    if (feed_enabled_ && state_.vessel_pressure_pa < config_.regulated_pressure_pa) {
        const double deficit = config_.regulated_pressure_pa - state_.vessel_pressure_pa;
        const double proportional = deficit / config_.regulated_pressure_pa * 4.0;
        feed_flow = std::min(config_.maximum_feed_kg_per_s,
                             config_.maximum_feed_kg_per_s * proportional);
    }

    // A single explicit step cannot move more mass than the source reservoir
    // holds above its floor. Clamping the transported mass, rather than the
    // resulting state, keeps both reservoirs consistent with one another.
    const double vessel_floor = config_.ambient_pressure_pa * config_.vessel_volume_m3 / rt;
    const double cylinder_floor = config_.ambient_pressure_pa * config_.cylinder_volume_m3 / rt;

    double orifice_mass = orifice_flow * delta_seconds;
    orifice_mass = std::min(orifice_mass, std::max(0.0, state_.vessel_mass_kg - vessel_floor));

    double bleed_mass = bleed_flow * delta_seconds;
    bleed_mass = std::min(bleed_mass, std::max(0.0, state_.cylinder_mass_kg - cylinder_floor));

    const double feed_mass = feed_flow * delta_seconds;

    state_.vessel_mass_kg = std::max(0.0, state_.vessel_mass_kg - orifice_mass + feed_mass);
    state_.cylinder_mass_kg = std::max(0.0, state_.cylinder_mass_kg + orifice_mass - bleed_mass);

    state_.vessel_pressure_pa = state_.vessel_mass_kg * rt / config_.vessel_volume_m3;
    state_.cylinder_pressure_pa = state_.cylinder_mass_kg * rt / config_.cylinder_volume_m3;

    state_.orifice_mass_flow_kg_per_s = orifice_mass / delta_seconds;
    state_.bleed_mass_flow_kg_per_s = bleed_mass / delta_seconds;
    state_.feed_mass_flow_kg_per_s = feed_mass / delta_seconds;
    state_.vented_mass_kg += orifice_mass;
    state_.fed_mass_kg += feed_mass;

    const double gauge = state_.cylinder_pressure_pa - config_.ambient_pressure_pa;
    state_.piston_force_n = std::max(0.0, gauge) * config_.piston_area_m2;
}

} // namespace scraperx::sim
