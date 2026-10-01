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

bool valid_path(const DrawPath &path) noexcept {
    if (!finite(path.undrawn_pouch_position) || !finite(path.pull_direction) ||
        length(path.pull_direction) <= kDirectionEpsilon ||
        !std::isfinite(path.opposing_load_n) || path.opposing_load_n < 0.0)
        return false;
    for (std::size_t i = 0; i < path.anchors.position.size(); ++i)
        if (!finite(path.anchors.position[i]) || !finite(path.anchors.velocity[i]))
            return false;
    return true;
}

bool valid_winch(const WinchParameters &p) noexcept {
    return std::isfinite(p.maximum_draw_m) && p.maximum_draw_m >= 0.0 &&
           std::isfinite(p.maximum_source_power_w) && p.maximum_source_power_w >= 0.0 &&
           std::isfinite(p.maximum_source_force_n) && p.maximum_source_force_n >= 0.0 &&
           std::isfinite(p.maximum_source_speed_mps) && p.maximum_source_speed_mps >= 0.0 &&
           std::isfinite(p.mechanical_advantage) && p.mechanical_advantage > 0.0 &&
           std::isfinite(p.efficiency) && p.efficiency > 0.0 && p.efficiency <= 1.0;
}

double draw_band_loss(const DrawPath &path, const BandParameters &bands,
                      const double lower, const double upper,
                      const double delta_seconds) noexcept {
    if (upper <= lower || delta_seconds == 0.0 || bands.damping_ns_per_m == 0.0)
        return 0.0;
    // The integration owner imposes one finite draw stroke per fixed tick.
    // Integrate its passive material work, not an extra energy reservoir.
    constexpr std::array<double, 5> nodes{-0.9061798459386640, -0.5384693101056831,
                                         0.0, 0.5384693101056831, 0.9061798459386640};
    constexpr std::array<double, 5> weights{0.2369268850561891, 0.4786286704993665,
                                           0.5688888888888889, 0.4786286704993665,
                                           0.2369268850561891};
    const double half_stroke = (upper - lower) * 0.5;
    const double middle = lower + half_stroke;
    const Vec3 velocity = normalized(path.pull_direction) * ((upper - lower) / delta_seconds);
    double average_power = 0.0;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto evaluation = evaluate_bands(pouch_at_draw(path, middle + nodes[i] * half_stroke),
                                               velocity, path.anchors, bands);
        average_power += 0.5 * weights[i] * evaluation.dissipated_power_w;
    }
    return average_power * delta_seconds;
}

template <typename Accepts>
double greatest_accepted_draw(double lower, double upper, Accepts accepts) noexcept {
    // Fixed iteration count, no machine-dependent stopping condition.
    // Convex spring U makes resistance and work monotone on a drawing path.
    for (int iteration = 0; iteration < 56; ++iteration) {
        const double middle = lower + (upper - lower) * 0.5;
        if (accepts(middle))
            lower = middle;
        else
            upper = middle;
    }
    return lower;
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
    if (!valid_bands(parameters) || !finite(pouch_position) || !finite(pouch_velocity))
        return result;
    for (std::size_t i = 0; i < anchors.position.size(); ++i)
        if (!finite(anchors.position[i]) || !finite(anchors.velocity[i]))
            return result;

    result.valid = true;
    for (std::size_t i = 0; i < anchors.position.size(); ++i) {
        auto &band = result.bands[i];
        const Vec3 separation = pouch_position - anchors.position[i];
        band.length_m = length(separation);
        band.extension_m = std::max(0.0, band.length_m - parameters.natural_length_m);
        if (band.length_m <= kDirectionEpsilon || band.extension_m == 0.0)
            continue;
        const Vec3 outward = separation / band.length_m;
        band.extension_rate_mps = dot(pouch_velocity - anchors.velocity[i], outward);
        band.elastic_tension_n = parameters.stiffness_n_per_m * band.extension_m;
        band.tension_n = std::max(0.0, band.elastic_tension_n +
                                      parameters.damping_ns_per_m * band.extension_rate_mps);
        band.elastic_energy_j = 0.5 * parameters.stiffness_n_per_m *
                                band.extension_m * band.extension_m;
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
    return {std::sin(yaw_radians) * std::cos(elevation_radians),
            std::sin(elevation_radians),
            -std::cos(yaw_radians) * std::cos(elevation_radians)};
}

AnchorPoints symmetric_anchors(const Vec3 midpoint, const double half_span_m,
                               const double yaw_radians) noexcept {
    const Vec3 right{std::cos(yaw_radians), 0.0, std::sin(yaw_radians)};
    AnchorPoints result;
    result.position = {midpoint - right * half_span_m, midpoint + right * half_span_m};
    return result;
}

Vec3 pouch_at_draw(const DrawPath &path, const double draw_m) noexcept {
    return path.undrawn_pouch_position + normalized(path.pull_direction) * draw_m;
}

double spring_energy_at_draw(const DrawPath &path, const double draw_m,
                             const BandParameters &bands) noexcept {
    return evaluate_bands(pouch_at_draw(path, draw_m), {}, path.anchors, bands).elastic_energy_j;
}

double draw_resistance_n(const DrawPath &path, const double draw_m,
                        const BandParameters &bands) noexcept {
    const auto evaluation = evaluate_bands(pouch_at_draw(path, draw_m), {}, path.anchors, bands);
    return -dot(evaluation.elastic_force_n, normalized(path.pull_direction)) + path.opposing_load_n;
}

DrawWinch::DrawWinch(const WinchParameters parameters) noexcept : parameters_(parameters) {}

DrawStep DrawWinch::crank(const DrawPath &path, const BandParameters &bands,
                          const double effort, const double delta_seconds) noexcept {
    DrawStep result;
    result.held_draw_m = state_.held_draw_m;
    if (!valid_winch(parameters_) || !valid_path(path) || !valid_bands(bands) ||
        !std::isfinite(effort) || !std::isfinite(delta_seconds) || delta_seconds < 0.0)
        return result;
    result.valid = true;
    const double old_draw = state_.held_draw_m;
    const double old_energy = spring_energy_at_draw(path, old_draw, bands);
    result.elastic_energy_j = old_energy;
    result.draw_resistance_n = slingshot::draw_resistance_n(path, old_draw, bands);
    result.at_draw_stop = old_draw >= parameters_.maximum_draw_m;
    const double input = std::clamp(effort, 0.0, 1.0);
    if (!state_.latch_closed || input == 0.0 || delta_seconds == 0.0 || result.at_draw_stop)
        return result;

    const double output_force_limit = input * parameters_.maximum_source_force_n *
                                      parameters_.mechanical_advantage * parameters_.efficiency;
    // A path pointing toward the anchors isn't a draw. Reject it rather
    // than crediting a reduction in U as new player work.
    if (result.draw_resistance_n < 0.0 || result.draw_resistance_n >= output_force_limit) {
        result.force_stalled = true;
        return result;
    }

    const double output_work_limit = input * parameters_.maximum_source_power_w *
                                     parameters_.efficiency * delta_seconds;
    const double source_travel_limit = input * parameters_.maximum_source_speed_mps * delta_seconds;
    double new_draw = std::min(parameters_.maximum_draw_m,
                               old_draw + source_travel_limit / parameters_.mechanical_advantage);
    const auto dynamic_resistance = [&](const double q) {
        const Vec3 draw_velocity = normalized(path.pull_direction) * ((q - old_draw) / delta_seconds);
        const auto end = evaluate_bands(pouch_at_draw(path, q), draw_velocity, path.anchors, bands);
        const auto begin = evaluate_bands(pouch_at_draw(path, old_draw), draw_velocity, path.anchors, bands);
        return std::max(-dot(end.force_n, normalized(path.pull_direction)),
                        -dot(begin.force_n, normalized(path.pull_direction))) + path.opposing_load_n;
    };
    if (dynamic_resistance(new_draw) > output_force_limit)
        new_draw = greatest_accepted_draw(old_draw, new_draw, [&](const double q) {
            return dynamic_resistance(q) <= output_force_limit;
        });
    const auto output_work = [&](const double q) {
        return spring_energy_at_draw(path, q, bands) - old_energy +
               path.opposing_load_n * (q - old_draw) +
               draw_band_loss(path, bands, old_draw, q, delta_seconds);
    };
    if (output_work(new_draw) > output_work_limit)
        new_draw = greatest_accepted_draw(old_draw, new_draw, [&](const double q) {
            return output_work(q) <= output_work_limit;
        });

    const double delta_draw = std::max(0.0, new_draw - old_draw);
    result.elastic_energy_j = spring_energy_at_draw(path, new_draw, bands);
    result.spring_work_j = std::max(0.0, result.elastic_energy_j - old_energy);
    result.opposing_load_work_j = path.opposing_load_n * delta_draw;
    result.band_loss_j = draw_band_loss(path, bands, old_draw, new_draw, delta_seconds);
    const double useful_work = result.spring_work_j + result.opposing_load_work_j + result.band_loss_j;
    result.source_work_j = useful_work / parameters_.efficiency;
    result.source_travel_m = delta_draw * parameters_.mechanical_advantage;
    result.average_source_force_n = result.source_travel_m > 0.0 ?
                                    result.source_work_j / result.source_travel_m : 0.0;
    result.held_draw_m = new_draw;
    result.draw_resistance_n = slingshot::draw_resistance_n(path, new_draw, bands);
    result.at_draw_stop = new_draw >= parameters_.maximum_draw_m;
    state_.held_draw_m = new_draw;
    state_.source_work_j += result.source_work_j;
    state_.spring_work_j += result.spring_work_j;
    state_.opposing_load_work_j += result.opposing_load_work_j;
    state_.band_loss_j += result.band_loss_j;
    state_.transmission_loss_j += result.source_work_j - useful_work;
    return result;
}

bool DrawWinch::relatch(const double actual_draw_m) noexcept {
    if (!valid_winch(parameters_) || !std::isfinite(actual_draw_m) ||
        actual_draw_m < 0.0 || actual_draw_m > parameters_.maximum_draw_m)
        return false;
    state_.held_draw_m = actual_draw_m;
    state_.latch_closed = true;
    return true;
}

bool DrawWinch::restore(const WinchState &state) noexcept {
    if (!valid_winch(parameters_) || !std::isfinite(state.held_draw_m) ||
        state.held_draw_m < 0.0 || state.held_draw_m > parameters_.maximum_draw_m ||
        !std::isfinite(state.source_work_j) || state.source_work_j < 0.0 ||
        !std::isfinite(state.spring_work_j) || state.spring_work_j < 0.0 ||
        !std::isfinite(state.opposing_load_work_j) || state.opposing_load_work_j < 0.0 ||
        !std::isfinite(state.transmission_loss_j) || state.transmission_loss_j < 0.0 ||
        !std::isfinite(state.band_loss_j) || state.band_loss_j < 0.0)
        return false;
    state_ = state;
    return true;
}

} // namespace scraperx::sim::slingshot
