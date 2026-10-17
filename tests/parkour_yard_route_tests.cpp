#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;

namespace {
void require(bool ok, const char *operation, const Simulation &simulation) {
    if (ok) return;
    const auto s = simulation.snapshot();
    std::cerr << "FAIL PARKOUR_YARD " << operation << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " grounded=" << s.player_grounded << " support=" << s.support_entity_id << '\n';
    std::exit(1);
}

void tick(Simulation &simulation, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "ordinary fixed tick accepted", simulation);
        require(simulation.snapshot().death_count == 0, "route preserves live player", simulation);
    }
}

void steer(Simulation &simulation, double x, double z, double cap) {
    const auto s = simulation.snapshot();
    const double dx = x - s.player_position.x, dz = z - s.player_position.z;
    require(simulation.set_move_input(std::clamp(1.3 * dx - .45 * s.player_linear_velocity.x, -cap, cap),
                                     std::clamp(1.3 * dz - .45 * s.player_linear_velocity.z, -cap, cap)),
            "ordinary movement input accepted", simulation);
}

void arrive(Simulation &simulation, double x, double z, double top, bool jump = false) {
    bool requested = false, airborne = false;
    require(simulation.set_sprint_input(jump), "sprint input accepted", simulation);
    for (unsigned i = 0; i < 1800; ++i) {
        const auto s = simulation.snapshot();
        const double distance = std::hypot(x - s.player_position.x, z - s.player_position.z);
        if (jump && !requested && distance < 3.3) {
            require(s.player_grounded, "jump starts from actual footing", simulation);
            require(simulation.set_facing(x - s.player_position.x, z - s.player_position.z),
                    "takeoff facing accepted", simulation);
            require(simulation.request_jump(), "ordinary jump request accepted", simulation);
            requested = true;
        }
        airborne = airborne || (requested && !s.player_grounded);
        const double capsule_height = s.player_crouched ? .6 : .9;
        if (distance < .18 && s.player_grounded && std::abs(s.player_position.y - (top + capsule_height)) < .13 &&
                std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .4) {
            require(!jump || (requested && airborne), "elevated arrival follows actual airborne motion", simulation);
            if (top > 0) require(s.support_entity_id == Simulation::kWorldSolidEntityId,
                                 "authored surface supplies actual native support", simulation);
            require(simulation.set_move_input(0, 0), "arrival releases movement", simulation);
            tick(simulation, 6);
            std::cout << "PASS PARKOUR_YARD arrival=" << x << ',' << z << " top=" << top
                      << " tick=" << simulation.snapshot().tick_index << '\n';
            return;
        }
        steer(simulation, x, z, jump ? .9 : .6);
        tick(simulation);
    }
    require(false, "bounded public route reaches actual receiving footing", simulation);
}
}

int main() {
    Simulation simulation(InitialSpawn::FreightYardArrival);
    tick(simulation, 90);
    require(simulation.snapshot().player_grounded, "ordinary opening supplies real ground", simulation);
    // Approach from the shipping arrival, without restart/pose setters.
    // Stay north of the existing x=-82 stock instead of crossing its volume.
    arrive(simulation, -80, -43.5, 0);
    arrive(simulation, -48, -43.5, 0);
    arrive(simulation, -48, -49, 0);
    arrive(simulation, -48, -55, 0);
    for (unsigned i = 0; i < 5; ++i)
        arrive(simulation, -52 - 4 * static_cast<double>(i), -55, .45 + .18 * i, true);
    // Deliberately leave the last plinth and recover through the ground aisle.
    arrive(simulation, -72, -55, 0);
    arrive(simulation, -72, -67.5, 0);
    arrive(simulation, -52, -67.5, 0);
    require(simulation.set_crouch_input(true), "ordinary crouch accepted", simulation);
    tick(simulation, 12);
    arrive(simulation, -52, -74, 0);
    require(simulation.set_crouch_input(false), "ordinary stand intent accepted", simulation);
    tick(simulation, 30);
    require(simulation.snapshot().player_crouched, "real duct underside blocks standing", simulation);
    require(simulation.set_crouch_input(true), "continue crouched passage", simulation);
    arrive(simulation, -58, -74, 0);
    arrive(simulation, -58, -70, 0);
    require(simulation.set_crouch_input(false), "clear-air stand accepted", simulation);
    tick(simulation, 30);
    require(!simulation.snapshot().player_crouched, "standing returns after leaving real roof", simulation);
    arrive(simulation, -46, -68, 0);
    arrive(simulation, -46, -55, 0);
    arrive(simulation, -48, -55, 0);
    arrive(simulation, -52, -55, .45, true);
    std::cout << "SCRAPERX_PARKOUR_YARD PASS public_arrival=1 jumps=6 crouch=1 blocked_stand=1 recovery=1 retry=1\n";
}
