#pragma once

// The slingshot's rubber, as a constitutive law in SI units (from the ChatGPT
// branch's slingshot_model, 2026-10-01). Jolt stays the authority for the
// pouch, the rider and every contact: this never sets a velocity or
// integrates a body of its own; it says what force the stretched bands put
// on the pouch where it is, moving as it is.

#include <array>

namespace scraperx::sim::slingshot {

struct Vec3 final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] Vec3 operator+(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 operator-(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 operator*(Vec3 a, double scale) noexcept;
[[nodiscard]] Vec3 operator/(Vec3 a, double scale) noexcept;
[[nodiscard]] double dot(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] double length(Vec3 a) noexcept;
[[nodiscard]] Vec3 normalized(Vec3 a) noexcept;

struct BandParameters final {
    // The fork's midpoint 10 m from the pouch at rest, its tips 3 m either
    // side: each band's natural length is sqrt(10^2 + 3^2) m.
    double natural_length_m = 10.44030650891055;
    double stiffness_n_per_m = 10000.0;
    // A small loss in the rubber, not a governor on the launch.
    double damping_ns_per_m = 15.0;
};

struct AnchorPoints final {
    std::array<Vec3, 2> position{};
    std::array<Vec3, 2> velocity{};
};

struct BandState final {
    double length_m = 0.0;
    double extension_m = 0.0;
    double extension_rate_mps = 0.0;
    double elastic_tension_n = 0.0;
    double tension_n = 0.0;
    double elastic_energy_j = 0.0;
    double dissipated_power_w = 0.0;
    Vec3 force_on_pouch_n{};
    Vec3 reaction_on_anchor_n{};
};

struct BandEvaluation final {
    std::array<BandState, 2> bands{};
    Vec3 elastic_force_n{};
    Vec3 force_n{};
    double elastic_energy_j = 0.0;
    double dissipated_power_w = 0.0;
    bool valid = false;
};

// Each band pulls only when stretched: x = max(0, |p - a| - L0), U = k x^2 / 2,
// F = -grad U, plus a dashpot that acts only in a taut band and never pushes.
// The anchors take equal and opposite reactions.
[[nodiscard]] BandEvaluation evaluate_bands(Vec3 pouch_position, Vec3 pouch_velocity,
                                            const AnchorPoints &anchors,
                                            const BandParameters &parameters = {}) noexcept;

// yaw = 0 points toward -Z; positive elevation points up.
[[nodiscard]] Vec3 aim_axis(double yaw_radians, double elevation_radians) noexcept;

} // namespace scraperx::sim::slingshot
