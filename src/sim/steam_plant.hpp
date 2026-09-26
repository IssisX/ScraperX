#pragma once

#include <cstdint>

namespace scraperx::sim {

// Reduced-order steam plant: one fired pressure vessel venting through a real
// orifice into one actuator cylinder, which pushes a piston.
//
// This is an authoritative representation, not a cosmetic proxy. It owns the
// pressures that decide how much work the lift can do, and it obeys a hard
// energy bound: with the boiler feed disabled the vessel is a finite reservoir,
// so the piston delivers finite work and then stalls. Governing Law 24 —
// machinery cannot create capability from nothing.
//
// Units are SI throughout: kilograms, cubic metres, kelvin, pascals, seconds,
// newtons, joules.
struct SteamPlantConfig final {
    double vessel_volume_m3 = 6.0;
    double cylinder_volume_m3 = 0.32;
    double temperature_k = 450.0;
    double gas_constant_j_per_kg_k = 461.5; // water vapour
    double heat_capacity_ratio = 1.30;      // gamma for low-pressure steam
    double ambient_pressure_pa = 101325.0;

    // Orifice between vessel and cylinder, fully open.
    double orifice_area_m2 = 0.0120;
    double orifice_discharge_coefficient = 0.72;

    // Permanent bleed port on the cylinder. The actuator is not sealed, so the
    // platform sinks again once the valve closes.
    double cylinder_bleed_area_m2 = 0.00185;
    double cylinder_bleed_discharge_coefficient = 0.68;

    // Piston face the cylinder pushes on.
    double piston_area_m2 = 0.045;

    // Pressure-regulated boiler feed. Finite: it cannot exceed this rate, and it
    // stops entirely at the regulated pressure.
    double regulated_pressure_pa = 4.60e5;
    double maximum_feed_kg_per_s = 0.145;

    double initial_vessel_pressure_pa = 4.60e5;
};

struct SteamPlantState final {
    double vessel_pressure_pa = 0.0;
    double cylinder_pressure_pa = 0.0;
    double vessel_mass_kg = 0.0;
    double cylinder_mass_kg = 0.0;
    double valve_open_fraction = 0.0;
    double orifice_mass_flow_kg_per_s = 0.0;
    double bleed_mass_flow_kg_per_s = 0.0;
    double feed_mass_flow_kg_per_s = 0.0;
    double piston_force_n = 0.0;
    double vented_mass_kg = 0.0;
    double fed_mass_kg = 0.0;
};

class SteamPlant final {
public:
    explicit SteamPlant(const SteamPlantConfig &config = {}) noexcept;

    // Valve position comes from the real lever body angle. Values outside [0, 1]
    // are clamped rather than rejected: the lever cannot rotate past its stops,
    // so an out-of-range reading is a numerical edge, not a command.
    void set_valve_open_fraction(double fraction) noexcept;

    // Disables the boiler feed. Used to prove the plant cannot do unbounded work.
    void set_feed_enabled(bool enabled) noexcept;
    [[nodiscard]] bool feed_enabled() const noexcept { return feed_enabled_; }

    // Checkpoint restore (WO-008 / TDD 14.1): forces the two integrated mass
    // variables to committed values and immediately recomputes the pressures
    // that derive from them, so the very next published snapshot is correct.
    // Flow/force fields are zeroed rather than left stale -- they are only
    // ever a function of the current tick's valve fraction and masses, so
    // "unknown until next step" is the honest value, not whatever they
    // happened to be at the moment of death.
    void restore_state(double vessel_mass_kg, double cylinder_mass_kg) noexcept;

    // Advances one authoritative fixed step. Non-finite or non-positive steps are
    // ignored so a bad frame can never corrupt plant state.
    void step(double delta_seconds) noexcept;

    [[nodiscard]] const SteamPlantState &state() const noexcept { return state_; }
    [[nodiscard]] const SteamPlantConfig &config() const noexcept { return config_; }

    // Internal energy of the vessel gas above ambient-pressure conditions, in
    // joules. Strictly the ceiling on the work the vessel can still deliver.
    [[nodiscard]] double vessel_available_energy_j() const noexcept;

private:
    [[nodiscard]] double mass_flow_kg_per_s(double upstream_pressure_pa,
                                            double downstream_pressure_pa,
                                            double area_m2,
                                            double discharge_coefficient) const noexcept;

    SteamPlantConfig config_{};
    SteamPlantState state_{};
    bool feed_enabled_ = true;
};

} // namespace scraperx::sim
