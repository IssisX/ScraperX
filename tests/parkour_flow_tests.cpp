#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL parkour flow: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void vault_carries_entry_and_exit_speed() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalState;
    using scraperx::sim::WorldContent;

    Simulation run(InitialSpawn::VaultApproach, WorldContent::RegressionFixtures);
    require(run.set_facing(1.0, 0.0), "vault facing accepted");
    require(run.set_move_input(0.0, 0.0), "settle input accepted");
    require(run.advance_frame(1.0).accepted && run.snapshot().player_grounded,
            "vault approach settles on the deck");
    require(run.set_move_input(1.0, 0.0), "approach input accepted");

    bool rail_offered = false;
    for (int tick = 0; tick < 180; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "approach tick accepted");
        const auto state = run.snapshot();
        if (state.ledge_available && state.ledge_entity_id == Simulation::kVaultRailEntityId) {
            rail_offered = true;
            break;
        }
    }
    require(rail_offered, "real rail becomes vaultable");
    const double approach_speed = run.snapshot().player_linear_velocity.x;
    require(approach_speed > 3.0, "approach has running speed");
    require(run.request_traversal(), "vault request accepted");
    require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "vault starts on the native clock");
    const auto entry = run.snapshot();
    require(entry.traversal_state == TraversalState::Vaulting, "rail commits a vault");
    std::cout << "INFO vault entry approach=" << approach_speed
              << " first=" << entry.player_linear_velocity.x << '\n';
    require(std::abs(entry.player_linear_velocity.x - approach_speed) < 0.6,
            "vault must enter near running speed instead of snapping to path-average speed");

    double last_vault_speed = entry.player_linear_velocity.x;
    bool completed = false;
    for (int tick = 0; tick < 90; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted, "vault tick accepted");
        const auto state = run.snapshot();
        if (state.traversal_state == TraversalState::Vaulting) {
            last_vault_speed = state.player_linear_velocity.x;
        } else if (state.accepted_traversal_count == 1) {
            std::cout << "INFO vault exit before=" << last_vault_speed
                      << " after=" << state.player_linear_velocity.x << '\n';
            require(std::abs(state.player_linear_velocity.x - last_vault_speed) < 0.6,
                    "vault must leave near its carried speed instead of snapping at exit");
            require(state.player_position.x > 5.6, "vault crosses the rail");
            require(state.aborted_traversal_count == 0, "valid vault does not abort");
            completed = true;
            break;
        }
    }
    require(completed, "vault finishes on its real landing");
}

void rendered_pose_fills_between_fixed_ticks() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::WorldContent;

    Simulation run(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    require(run.set_move_input(0.0, 0.0), "settle input accepted");
    require(run.advance_frame(1.0).accepted, "deck settle accepted");
    require(run.set_move_input(1.0, 0.0), "running input accepted");
    const auto before = run.snapshot().player_position;
    require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "first moving fixed tick accepted");
    const auto after = run.snapshot().player_position;
    require(after.x > before.x, "player really advances on a fixed tick");

    const auto at_tick = run.render_player_position();
    require(std::abs(at_tick.x - before.x) < 1.0e-5,
            "display pose begins at previous authoritative tick");
    require(run.advance_frame(Simulation::kFixedStepSeconds * 0.5).accepted,
            "half display tick accepted");
    const auto halfway = run.render_player_position();
    require(std::abs(halfway.x - (before.x + after.x) * 0.5) < 1.0e-5,
            "display pose is halfway between authoritative positions");
    require(std::abs(run.snapshot().player_position.x - after.x) < 1.0e-8,
            "render interpolation does not move native physics");
}

} // namespace

int main() {
    vault_carries_entry_and_exit_speed();
    rendered_pose_fills_between_fixed_ticks();
    std::cout << "PASS scraperx_sim parkour flow\n";
    return EXIT_SUCCESS;
}
