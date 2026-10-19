#include "sim/simulation.hpp"
#include "sim/loose_gangway_route.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *operation, const Simulation &simulation) {
    if (value) return;
    const auto s = simulation.snapshot();
    const auto board = simulation.kit_body_position(simulation.kit_body_index(kLooseGangwayEntity));
    std::cerr << "FAIL LOOSE_GANGWAY " << operation << " tick=" << s.tick_index
              << " player=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " velocity=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " support=" << s.support_entity_id << " carrying=" << s.carrying_entity_id
              << " board=" << board.x << ',' << board.y << ',' << board.z << '\n';
    std::exit(EXIT_FAILURE);
}
unsigned board_footing_ticks = 0;
void tick(Simulation &simulation, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "ordinary native tick", simulation);
        const auto s = simulation.snapshot();
        require(s.death_count == 0 && s.player_gravity_factor == 1 &&
                s.traversal_hand_constraint_count == 0 && s.step_up_pose_count == 0,
                "gravity, collision and survival without traversal pose assist", simulation);
        if (s.player_grounded && s.support_entity_id == kLooseGangwayEntity) ++board_footing_ticks;
    }
}
void report(const char *stage, const Simulation &simulation) {
    const auto s = simulation.snapshot();
    const auto b = simulation.kit_body_position(simulation.kit_body_index(kLooseGangwayEntity));
    const auto q = simulation.kit_body_rotation(simulation.kit_body_index(kLooseGangwayEntity));
    std::cout << "LOOSE_GANGWAY " << stage << " tick=" << s.tick_index
              << " player=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " support=" << s.support_entity_id << " carrying=" << s.carrying_entity_id
              << " target=" << s.carry_target_entity_id << " board=" << b.x << ',' << b.y << ',' << b.z
              << " rotation=" << q.x << ',' << q.y << ',' << q.z << ',' << q.w << '\n';
}
void move_to(Simulation &simulation, double x, double z, double cap = .6,
             std::uint64_t required_carry = 0) {
    for (unsigned i = 0; i < 1800; ++i) {
        const auto s = simulation.snapshot();
        require(s.carrying_entity_id == required_carry, "carried object remains the actual requested body", simulation);
        const double dx = x - s.player_position.x, dz = z - s.player_position.z;
        if (std::hypot(dx, dz) < .08 && s.player_grounded &&
                std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .18) {
            require(simulation.set_move_input(0, 0), "finish movement input", simulation);
            tick(simulation, 8);
            report("arrival", simulation);
            return;
        }
        require(simulation.set_move_input(
            std::clamp(1.3 * dx - .45 * s.player_linear_velocity.x, -cap, cap),
            std::clamp(1.3 * dz - .45 * s.player_linear_velocity.z, -cap, cap)),
            "ordinary directional movement", simulation);
        tick(simulation);
    }
    require(false, "bounded movement reaches the requested point", simulation);
}

void place_board(Simulation &simulation, bool diagnostic = false) {
    move_to(simulation, -71, -26.97);
    require(simulation.set_facing(0, -1), "face actual central board grip", simulation);
    tick(simulation, 12);
    report("pickup-ready", simulation);
    require(simulation.snapshot().carry_target_entity_id == kLooseGangwayEntity &&
            simulation.request_pick_up(), "ordinary pickup acquires loose board", simulation);
    tick(simulation, 30);
    require(simulation.snapshot().carrying_entity_id == kLooseGangwayEntity,
            "real carry constraint retains the staging board", simulation);
    const auto held = simulation.snapshot();
    const auto at = simulation.kit_body_position(simulation.kit_body_index(kLooseGangwayEntity));
    move_to(simulation, -65 - (at.x - held.player_position.x), held.player_position.z, .08, kLooseGangwayEntity);
    require(simulation.request_set_down(), "ordinary drop removes carry constraint", simulation);
    tick(simulation, 180);
    const auto b = simulation.kit_body_position(simulation.kit_body_index(kLooseGangwayEntity));
    const auto q = simulation.kit_body_rotation(simulation.kit_body_index(kLooseGangwayEntity));
    const double axis_y = 2 * (q.x * q.y + q.w * q.z);
    report("released-observation", simulation);
    if (diagnostic) return;
    require(simulation.snapshot().carrying_entity_id == 0 &&
            std::abs(b.y - kLooseGangwayDockTop - .06) < .04 &&
            std::abs(b.z + 28) < .25 && std::abs(axis_y) < .04,
            "released board rests across actual plinths without being carried", simulation);
    report("placed", simulation);
}

void cross_and_exit(Simulation &simulation) {
    move_to(simulation, -71, -26.0);
    move_to(simulation, -71, -28);
    move_to(simulation, -68.1, -28, .3);
    require(simulation.snapshot().support_entity_id == kLooseGangwayDockEntity,
            "actual approach plinth footing", simulation);
    const unsigned before = board_footing_ticks;
    move_to(simulation, -61.9, -28, .25);
    require(board_footing_ticks >= before + 30 &&
            simulation.snapshot().support_entity_id == kLooseGangwayDockEntity,
            "crossing uses solved loose-board footing before actual receiver", simulation);
    move_to(simulation, -61.9, -29.65);
    tick(simulation, 60);
    require(simulation.snapshot().support_entity_id == kLooseGangwayDockEntity,
            "onward perch has stable real support", simulation);
    move_to(simulation, -61.9, -32.4);
    require(simulation.snapshot().support_entity_id == 1,
            "ordinary step-down reaches yard recovery footing", simulation);
    report("onward-ground", simulation);
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    Simulation simulation(InitialSpawn::FreightYardArrival);
    if (argc == 2 && std::string_view(argv[1]) == "placement-diagnostic") {
        require(simulation.debug_restart_at({-71, .9, -26.97}), "stage initial diagnostic footing", simulation);
        tick(simulation, 45);
        place_board(simulation, true);
        std::cout << "OBSERVE LOOSE_GANGWAY diagnostic only; no public-route result\n";
        return EXIT_SUCCESS;
    }
    require(argc == 1, "supported command-line mode", simulation);
    tick(simulation, 90);
    move_to(simulation, -80, -43.5);
    move_to(simulation, -76, -43.5);
    move_to(simulation, -76, -20.8);
    move_to(simulation, -71, -20.8);
    place_board(simulation);
    cross_and_exit(simulation);
    std::cout << "SCRAPERX_LOOSE_GANGWAY PASS public_arrival=1 pickup=1 placement=1"
              << " actual_board_footing_ticks=" << board_footing_ticks
              << " onward=1 ground_recovery=1 resets=0 deaths=0 tick="
              << simulation.snapshot().tick_index << '\n';
}
