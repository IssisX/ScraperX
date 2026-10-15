#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;
namespace {
void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void tick(Simulation &run, int count = 1) {
    for (int i = 0; i < count; ++i) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted, "physics tick rejected");
        require(run.snapshot().death_count == 0, "yard traversal killed the player");
    }
}
void report(const char *name, const Snapshot &state) {
    std::cout << name << " at=" << state.player_position.x << ',' << state.player_position.y
              << ',' << state.player_position.z << " grounded=" << state.player_grounded
              << " support=" << state.support_entity_id << '\n';
}
void shed_entry_and_back_wall() {
    Simulation run;
    require(run.debug_restart_at({-36, .9, -19}), "open-front approach staging rejected");
    tick(run, 90);
    require(run.snapshot().player_grounded, "shed approach has no grade support");
    require(run.set_move_input(0, -1), "shed walk input rejected");
    for (int i = 0; i < 360 && run.snapshot().player_position.z > -32; ++i) tick(run);
    const auto inside = run.snapshot();
    report("shed entry", inside);
    require(inside.player_position.z <= -32 && inside.player_position.z > -34 &&
                std::abs(inside.player_position.x + 36) < .3 && inside.player_grounded &&
                inside.player_position.y < 1.05,
            "open shed front or central aisle obstructs ordinary walking");

    // This clear strip beside the benches reaches the back sheet itself.
    require(run.set_move_input(0, 0), "stop input rejected");
    require(run.debug_restart_at({-29, .9, -33}), "back-wall approach staging rejected");
    tick(run, 90);
    require(run.set_move_input(0, -1), "back-wall walk input rejected");
    tick(run, 270);
    const auto blocked = run.snapshot();
    report("shed back wall", blocked);
    require(blocked.player_grounded && blocked.player_position.z < -37.2 &&
                blocked.player_position.z > -37.8 && blocked.player_position.y < 1.05,
            "shed back sheet failed to block a grounded player");
}
void receiving_stock_supports_real_landing() {
    // The shipping world enables physical landing feedback; GroundFoundation
    // intentionally omits those receipts. Neither test changes the player pose
    // after the initial clear-air staging.
    Simulation run;
    require(run.debug_restart_at({35, 3.5, -49}), "stock drop staging rejected");
    const auto before = run.snapshot().landing_count;
    bool falling = false;
    for (int i = 0; i < 270 && run.snapshot().landing_count == before; ++i) {
        tick(run);
        falling = falling || run.snapshot().player_linear_velocity.y < -1;
    }
    const auto landing = run.snapshot();
    require(falling && landing.landing_count > before && landing.landing_normal_speed_mps > 1 &&
                landing.landing_observed_normal_impulse_ns > 0 &&
                landing.landing_support_entity_id == Simulation::kWorldSolidEntityId,
            "stock has no real falling-player contact and landing impulse");
    tick(run, 90);
    const auto standing = run.snapshot();
    report("receiving stock", standing);
    // Hand-checked surface: upper steel layer .78 + .09 = .87 m,
    // plus the standing capsule's .9 m half-height, allowing contact slop.
    require(standing.player_grounded && standing.support_entity_id == Simulation::kWorldSolidEntityId &&
                std::abs(standing.player_position.y - 1.77) < .06 &&
                std::abs(standing.player_position.x - 35) < .2 &&
                std::abs(standing.player_position.z + 49) < .2,
            "receiving stock does not retain an actual standing player");
}
void central_lane_stays_open() {
    Simulation run;
    require(run.debug_restart_at({0, .9, -19}), "central-lane approach staging rejected");
    tick(run, 90);
    require(run.set_move_input(0, -1), "central-lane walk input rejected");
    for (int i = 0; i < 450 && run.snapshot().player_position.z > -39; ++i) {
        tick(run);
        const auto state = run.snapshot();
        require(state.player_grounded && state.player_position.y < 1.05 &&
                    std::abs(state.player_position.x) < .3,
                "central lane forces an obstacle climb, fall or lateral detour");
    }
    report("central lane", run.snapshot());
    require(run.snapshot().player_position.z <= -39, "central lane blocks twenty meters of walking");
}
}
int main() {
    shed_entry_and_back_wall();
    receiving_stock_supports_real_landing();
    central_lane_stays_open();
    std::cout << "PASS native yard entry, back-wall collision, stock landing and clear lane\n";
    return EXIT_SUCCESS;
}
