#include "sim/simulation.hpp"
#include "sim/workshop_roof_route.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;

namespace {
void require(bool ok, const char *operation, const Simulation &simulation) {
    if (ok) return;
    const auto s = simulation.snapshot();
    std::cerr << "FAIL WORKSHOP_ROOF " << operation << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " grounded=" << s.player_grounded << " support=" << s.support_entity_id
              << " hands=" << s.traversal_hand_constraint_count << '\n';
    std::exit(1);
}

void tick(Simulation &simulation, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "ordinary fixed tick accepted", simulation);
        const auto s = simulation.snapshot();
        require(s.death_count == 0 && s.player_gravity_factor == 1,
                "ordinary gravity and survival throughout route", simulation);
    }
}

void arrive(Simulation &simulation, double x, double z, double top) {
    require(simulation.set_sprint_input(false), "release sprint for approach", simulation);
    for (unsigned i = 0; i < 1800; ++i) {
        const auto s = simulation.snapshot();
        const double dx = x - s.player_position.x, dz = z - s.player_position.z;
        if (std::hypot(dx, dz) < .18 && s.player_grounded &&
                std::abs(s.player_position.y - top - (s.player_crouched ? .6 : .9)) < .13 &&
                std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .4) {
            require(s.support_entity_id == (top > 0 ? Simulation::kWorldSolidEntityId : 1),
                    "arrival has the actual receiving support", simulation);
            require(simulation.set_move_input(0, 0), "release arrival input", simulation);
            tick(simulation, 6);
            std::cout << "WORKSHOP_ARRIVAL x=" << x << " z=" << z << " top=" << top
                      << " tick=" << simulation.snapshot().tick_index << '\n';
            return;
        }
        require(simulation.set_move_input(std::clamp(1.3 * dx - .45 * s.player_linear_velocity.x, -.6, .6),
                                         std::clamp(1.3 * dz - .45 * s.player_linear_velocity.z, -.6, .6)),
                "ordinary approach steering", simulation);
        tick(simulation);
    }
    require(false, "bounded route reaches receiving footing", simulation);
}

void climb_service_stairs(Simulation &simulation) {
    require(simulation.set_facing(0, 1) && simulation.set_move_input(0, .75),
            "walk into the actual service stairs", simulation);
    bool used_foot = false;
    for (unsigned i = 0; i < 450; ++i) {
        tick(simulation);
        const auto s = simulation.snapshot();
        used_foot = used_foot || s.foot_push_active;
        if (s.player_position.z > -43.9 && s.player_grounded &&
                s.support_entity_id == kWorkshopRoofAccessEntity &&
                std::abs(s.player_position.y - kWorkshopLaunchTopM - .9) < .1) {
            require(used_foot && s.traversal_hand_constraint_count == 0,
                    "real foot pushes climb service treads without hands", simulation);
            std::cout << "WORKSHOP_STAIRS top=" << kWorkshopLaunchTopM
                      << " speed_mps=" << s.player_linear_velocity.z << '\n';
            return;
        }
    }
    require(false, "ordinary walking reaches service launch plinth", simulation);
}

void catch_roof(Simulation &simulation) {
    require(simulation.set_move_input(0, 1) && simulation.set_sprint_input(true),
            "carry stair approach into a running jump", simulation);
    bool jumped = false;
    for (unsigned i = 0; i < 90; ++i) {
        tick(simulation);
        const auto s = simulation.snapshot();
        if (s.player_grounded && s.support_entity_id == kWorkshopRoofAccessEntity &&
                s.player_position.z > -41.8) {
            require(s.player_linear_velocity.z > 3 && simulation.request_jump(),
                    "takeoff is earned from real launch footing", simulation);
            jumped = true;
            break;
        }
    }
    require(jumped, "launch happens before leaving the real plinth", simulation);
    bool caught = false, wall_requested = false, topout_requested = false;
    for (unsigned i = 0; i < 500; ++i) {
        tick(simulation);
        const auto s = simulation.snapshot();
        caught = caught || s.traversal_hand_constraint_count > 0;
        const double reach = std::hypot(std::hypot(s.traversal_ledge_point.x - s.player_position.x,
                                                   s.traversal_ledge_point.z - s.player_position.z),
                                        s.traversal_ledge_point.y - s.player_position.y - .45);
        if (caught && !topout_requested && s.traversal_state == TraversalState::Hanging && reach < 1.0) {
            require(simulation.request_jump(), "physical top-out input inside existing hand reach", simulation);
            topout_requested = true;
        }
        if (!wall_requested && !caught && i > 20 && !s.player_grounded &&
                s.player_linear_velocity.y <= .05) {
            require(simulation.request_jump(), "ordinary wall jump/catch input", simulation);
            wall_requested = true;
        }
        if (caught && s.player_grounded && s.support_entity_id == Simulation::kWorldSolidEntityId &&
                s.traversal_hand_constraint_count == 0 &&
                std::abs(s.player_position.y - kWorkshopRoofTopM - .9) < .15) {
            require(simulation.set_move_input(0, 0), "roof arrival releases forward input", simulation);
            std::cout << "WORKSHOP_CATCH roof_top=" << kWorkshopRoofTopM
                      << " tick=" << s.tick_index << '\n';
            return;
        }
    }
    require(false, "real catch and physical top-out reach workshop roof", simulation);
}

void return_to_yard(Simulation &simulation) {
    arrive(simulation, -40, -30, kWorkshopRoofTopM);
    const auto before = simulation.snapshot().landing_count;
    require(simulation.set_facing(-1, 0) && simulation.set_move_input(-1, 0) &&
            simulation.set_sprint_input(true),
            "run toward the actual west roof edge", simulation);
    bool airborne = false, prepared = false;
    for (unsigned i = 0; i < 450; ++i) {
        tick(simulation);
        const auto s = simulation.snapshot();
        if (!prepared && s.player_position.x < -43.0) {
            require(std::abs(s.player_linear_velocity.x) > 5.5 && simulation.set_crouch_input(true),
                    "prepare compact posture after earning actual runup speed", simulation);
            prepared = true;
        }
        airborne = airborne || (!s.player_grounded && s.player_position.x < -44.6);
        if (airborne && s.player_grounded && s.support_entity_id == 1 &&
                s.landing_count > before && s.player_position.y < 1) {
            std::cout << "WORKSHOP_IMPACT response=" << s.landing_response << " normal_mps=" << s.landing_normal_speed_mps << " tangent_mps=" << s.landing_tangent_speed_mps << '\n';
            require(prepared && s.landing_response == 2 && s.landing_normal_speed_mps >= 8 &&
                    s.landing_normal_speed_mps < 12 && s.landing_tangent_speed_mps > 2.5,
                    "prepared roof drop enters actual momentum-carrying compact recovery", simulation);
            require(std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) > 3,
                    "actual landing preserves useful onward momentum", simulation);
            std::cout << "WORKSHOP_RETURN normal_mps=" << s.landing_normal_speed_mps
                      << " onward_mps=" << std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z)
                      << " support=" << s.support_entity_id << '\n';
            require(simulation.set_crouch_input(false), "release compact posture in clear yard", simulation);
            arrive(simulation, -49, -42, 0);
            arrive(simulation, -49, -43.5, 0);
            arrive(simulation, -39.2, -43.5, 0);
            arrive(simulation, -39.2, -48.5, 0);
            arrive(simulation, -36, -48.5, 0);
            climb_service_stairs(simulation);
            require(simulation.set_move_input(0, 0), "retry plinth remains usable", simulation);
            return;
        }
    }
    require(false, "actual west-edge drop returns to live yard footing", simulation);
}
} // namespace

int main() {
    std::cout << std::unitbuf;
    Simulation simulation(InitialSpawn::FreightYardArrival);
    tick(simulation, 90);
    require(simulation.snapshot().player_grounded, "ordinary freight arrival supplies real ground", simulation);
    arrive(simulation, -80, -43.5, 0);
    arrive(simulation, -48, -43.5, 0);
    // Pass east of the retained timber stock and west of the new plinth.
    arrive(simulation, -39.2, -43.5, 0);
    arrive(simulation, -39.2, -48.5, 0);
    arrive(simulation, -36, -48.5, 0);
    climb_service_stairs(simulation);
    catch_roof(simulation);
    return_to_yard(simulation);
    std::cout << "SCRAPERX_WORKSHOP_ROOF PASS public_arrival=1 physical_stairs=8 catch=1 topout=1 roof=1 prepared_landing=1 retry=1\n";
}
