#include "sim/slingshot_model.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim::slingshot {
namespace {
constexpr double kDirectionEpsilon = 1.0e-12;

bool finite(const Vec3 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool valid_bands(const BandParameters &p) noexcept {
    return std::isfinite(p.natural_length_m) && p.natural_length_m >= 0.0 &&
           std::isfinite(p.stiffness_n_per_m) && p.stiffness_n_per_m >= 0.0 &&
           std::isfinite(p.damping_ns_per_m) && p.damping_ns_per_m >= 0.0;
}
} // namespace

Vec3 operator+(const Vec3 a, const Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(const Vec3 a, const Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3 a, const double scale) noexcept { return {a.x * scale, a.y * scale, a.z * scale}; }
Vec3 operator/(const Vec3 a, const double scale) noexcept { return a * (1.0 / scale); }
double dot(const Vec3 a, const Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(const Vec3 a) noexcept { return std::hypot(a.x, a.y, a.z); }
Vec3 normalized(const Vec3 a) noexcept {
    const double magnitude = length(a);
    return magnitude > kDirectionEpsilon && std::isfinite(magnitude) ? a / magnitude : Vec3{};
}

BandEvaluation evaluate_bands(const Vec3 pouch_position, const Vec3 pouch_velocity,
                              const AnchorPoints &anchors,
                              const BandParameters &parameters) noexcept {
    BandEvaluation result;
    if (!valid_bands(parameters) || !finite(pouch_position) || !finite(pouch_velocity)) {
        return result;
    }
    for (std::size_t i = 0; i < anchors.position.size(); ++i) {
        if (!finite(anchors.position[i]) || !finite(anchors.velocity[i])) {
            return result;
        }
    }
    result.valid = true;
    for (std::size_t i = 0; i < anchors.position.size(); ++i) {
        auto &band = result.bands[i];
        const Vec3 separation = pouch_position - anchors.position[i];
        band.length_m = length(separation);
        band.extension_m = std::max(0.0, band.length_m - parameters.natural_length_m);
        if (band.length_m <= kDirectionEpsilon || band.extension_m == 0.0) {
            continue;
        }
        const Vec3 outward = separation / band.length_m;
        band.extension_rate_mps = dot(pouch_velocity - anchors.velocity[i], outward);
        band.elastic_tension_n = parameters.stiffness_n_per_m * band.extension_m;
        band.tension_n = std::max(0.0, band.elastic_tension_n +
                                           parameters.damping_ns_per_m * band.extension_rate_mps);
        band.elastic_energy_j = 0.5 * parameters.stiffness_n_per_m * band.extension_m * band.extension_m;
        const double damping_tension = band.tension_n - band.elastic_tension_n;
        band.dissipated_power_w = std::max(0.0, damping_tension * band.extension_rate_mps);
        band.force_on_pouch_n = outward * -band.tension_n;
        band.reaction_on_anchor_n = outward * band.tension_n;
        result.elastic_force_n = result.elastic_force_n + outward * -band.elastic_tension_n;
        result.force_n = result.force_n + band.force_on_pouch_n;
        result.elastic_energy_j += band.elastic_energy_j;
        result.dissipated_power_w += band.dissipated_power_w;
    }
    return result;
}

Vec3 aim_axis(const double yaw_radians, const double elevation_radians) noexcept {
    return {std::sin(yaw_radians) * std::cos(elevation_radians), std::sin(elevation_radians),
            -std::cos(yaw_radians) * std::cos(elevation_radians)};
}

} // namespace scraperx::sim::slingshot
