#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *message) {
    if (!value) {
        std::cerr << "FAIL forklift public flow: " << message << '\n';
        std::exit(1);
    }
}
void tick(Simulation &simulation, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "public forklift native tick accepted");
}
} // namespace

int main() {
    Simulation simulation;
    require(simulation.forklift_state().available, "shipping world contains physical truck");
    const auto chassis = simulation.kit_body_index(2440);
    const auto first_crate = simulation.kit_body_index(2444);
    require(chassis != Simulation::kKitNone && first_crate != Simulation::kKitNone,
            "truck and independently dynamic cargo have stable identities");
    const auto initial_energy = simulation.forklift_state().battery_j;
    require(simulation.set_forklift_input(.5, 0, 1, 0, 0), "finite commands accepted");
    tick(simulation, 20);
    require(!simulation.forklift_state().controls_reachable &&
            simulation.forklift_state().applied_drive == 0 &&
            simulation.forklift_state().battery_j == initial_energy,
            "grade spawn cannot remotely power or move the forklift");

    require(simulation.debug_restart_at({-100, 1.34, -57.65}), "stage once over real operator platform");
    tick(simulation, 90);
    const auto ready = simulation.forklift_state();
    std::cout << "FORKLIFT_PLATFORM grounded=" << simulation.snapshot().player_grounded
              << " support=" << simulation.snapshot().support_entity_id
              << " reachable=" << ready.controls_reachable
              << " player_y=" << simulation.snapshot().player_position.y << '\n';
    require(ready.controls_reachable, "actual solved operator-floor contact grants controls");
    require(simulation.set_forklift_input(0, 0, 1, 0, 1), "lift with finite parking brake");
    tick(simulation, 90);
    const auto raised = simulation.forklift_state();
    std::cout << "FORKLIFT_PUBLIC_LIFT stroke=" << raised.lift_position_m
              << " energy=" << raised.battery_j << " force=" << raised.lift_force_n << '\n';
    require(raised.lift_position_m > ready.lift_position_m + .12 && raised.battery_j < ready.battery_j,
            "public input raises real carriage while spending native energy");
    require(!simulation.set_forklift_input(std::numeric_limits<double>::quiet_NaN(), 0, 0, 0, 0),
            "nonfinite demand rejected and neutralized");
    tick(simulation, 1);
    require(simulation.forklift_state().applied_drive == 0 && simulation.forklift_state().applied_brake > .9,
            "invalid demand leaves finite brake instead of stale throttle");

    require(simulation.debug_restart_at({-97, .9, -58}), "leave controls onto real grade");
    require(!simulation.forklift_state().controls_reachable, "restart clears stale platform authority immediately");
    const auto energy = simulation.forklift_state().battery_j;
    require(simulation.set_forklift_input(1, 1, 1, 1, 0), "unsupported intent can still be queued");
    tick(simulation, 45);
    require(!simulation.forklift_state().controls_reachable &&
            simulation.forklift_state().applied_drive == 0 &&
            simulation.forklift_state().battery_j == energy,
            "losing actual station contact brakes and cannot spend energy remotely");
    std::cout << "PASS forklift public station, energy, release and finite-input flow\n";
}
