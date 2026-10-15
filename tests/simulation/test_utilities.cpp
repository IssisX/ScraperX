#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace scraperx::tests {

void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool nearly_equal(const double a, const double b, const double epsilon) {
    return std::abs(a - b) <= epsilon;
}

double horizontal_distance(const scraperx::sim::Vector3 &a,
                           const scraperx::sim::Vector3 &b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}

double horizontal_magnitude(const scraperx::sim::Vector3 &value) {
    return std::hypot(value.x, value.z);
}

double horizontal_dot(const scraperx::sim::Vector3 &a,
                      const scraperx::sim::Vector3 &b) {
    return a.x * b.x + a.z * b.z;
}

double walk_toward(scraperx::sim::Simulation &simulation,
                   const double target_x,
                   const double target_z,
                   const double seconds,
                   const bool settled_arrival,
                   const bool north_facing) {
    const double step = scraperx::sim::Simulation::kFixedStepSeconds;
    const auto ticks = static_cast<std::uint32_t>(seconds / step);
    double deepest_z = simulation.snapshot().player_position.z;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        const auto state = simulation.snapshot();
        double dx = target_x - state.player_position.x;
        double dz = target_z - state.player_position.z;
        const double length = std::hypot(dx, dz);
        if (length > 1.0e-6) {
            dx /= length;
            dz /= length;
        }
        if (settled_arrival) {
            const double input_x = (target_x - state.player_position.x) * 1.8 -
                (state.player_linear_velocity.x - state.support_point_linear_velocity.x) * .28;
            const double input_z = (target_z - state.player_position.z) * 1.8 -
                (state.player_linear_velocity.z - state.support_point_linear_velocity.z) * .28;
            const double scale = std::max(1.0, std::hypot(input_x, input_z));
            (void)simulation.set_move_input(input_x / scale, input_z / scale);
        } else {
            // Impassability proofs retain sustained full-stick pressure.
            (void)simulation.set_move_input(dx, dz);
        }
        // A fixed northward facing keeps a carried load beside an east/west
        // ramp climb; movement still follows the same horizontal target.
        (void)simulation.set_facing(north_facing ? 0.0 : dx, north_facing ? -1.0 : dz);
        (void)simulation.advance_frame(step);
        deepest_z = std::min(deepest_z, simulation.snapshot().player_position.z);
    }
    return deepest_z;
}

void steer_toward(scraperx::sim::Simulation &simulation, const double x, const double z,
                  const double speed) {
    const auto state = simulation.snapshot();
    const double dx = x - state.player_position.x;
    const double dz = z - state.player_position.z;
    const double length = std::hypot(dx, dz);
    if (length < 0.05) {
        (void)simulation.set_move_input(0.0, 0.0);
        return;
    }
    const double scale = std::min(1.0, length / 0.6) * speed;
    (void)simulation.set_move_input(dx / length * scale, dz / length * scale);
    (void)simulation.set_facing(dx / length, dz / length);
}

bool walk_to(scraperx::sim::Simulation &simulation, const double x, const double z,
             const double budget_seconds, const double tolerance,
             const bool settled_arrival) {
    using scraperx::sim::Simulation;
    const auto ticks = static_cast<std::uint32_t>(
        budget_seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        const auto state = simulation.snapshot();
        const double dx=x-state.player_position.x,dz=z-state.player_position.z;
        if (std::hypot(dx,dz) <= tolerance &&
            (!settled_arrival || (state.player_grounded &&
             std::hypot(state.player_linear_velocity.x,state.player_linear_velocity.z)<.15))) {
            (void)simulation.set_move_input(0.0, 0.0);
            return true;
        }
        if (settled_arrival) {
            const double ix=dx*1.8-state.player_linear_velocity.x*.28;
            const double iz=dz*1.8-state.player_linear_velocity.z*.28;
            const double scale=std::max(1.0,std::hypot(ix,iz));
            (void)simulation.set_move_input(ix/scale,iz/scale);
            if (std::hypot(dx,dz)>.001) (void)simulation.set_facing(dx,dz);
        } else steer_toward(simulation, x, z);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    return false;
}

} // namespace scraperx::tests
