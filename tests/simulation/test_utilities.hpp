#pragma once

#include "sim/simulation.hpp"

namespace scraperx::tests {

void require(const bool condition, const char *message);

bool nearly_equal(const double a, const double b, const double epsilon = 1.0e-12);

double horizontal_distance(const scraperx::sim::Vector3 &a,
                           const scraperx::sim::Vector3 &b);

double horizontal_magnitude(const scraperx::sim::Vector3 &value);

double horizontal_dot(const scraperx::sim::Vector3 &a,
                      const scraperx::sim::Vector3 &b);

double walk_toward(scraperx::sim::Simulation &simulation,
                   const double target_x,
                   const double target_z,
                   const double seconds,
                   const bool settled_arrival = false,
                   const bool north_facing = false);

void steer_toward(scraperx::sim::Simulation &simulation, const double x, const double z,
                  const double speed = 1.0);

bool walk_to(scraperx::sim::Simulation &simulation, const double x, const double z,
             const double budget_seconds, const double tolerance = 0.15,
             const bool settled_arrival = false);

// Advance the authoritative fixed clock until a real snapshot satisfies the predicate.
template <typename Predicate>
bool advance_until(scraperx::sim::Simulation &simulation,
                   Predicate predicate,
                   const double budget_seconds) {
    const auto budget_ticks = static_cast<std::uint32_t>(
        budget_seconds * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < budget_ticks; ++tick) {
        if (!simulation.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds).accepted) {
            return false;
        }
        if (predicate(simulation.snapshot())) {
            return true;
        }
    }
    return false;
}

} // namespace scraperx::tests
