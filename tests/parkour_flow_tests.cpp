#include "sim/simulation.hpp"

#include <algorithm>
#include <limits>
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

double horizontal_speed(const scraperx::sim::Snapshot &s) {
    return std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z);
}

void neutral_airborne_input_preserves_horizontal_momentum() {
    using namespace scraperx::sim;

    Simulation run(InitialSpawn::ExteriorGrade, WorldContent::GroundFoundation);
    require(run.set_move_input(0.0, 0.0), "neutral-air settle input accepted");
    require(run.advance_frame(1.0).accepted && run.snapshot().player_grounded,
            "ordinary ground settles before the neutral-air run");

    for (int tick = 0; tick < 45; ++tick) {
        require(run.set_move_input(1.0, 0.0), "eastward run input accepted");
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "eastward ground tick accepted");
    }
    const auto approach = run.snapshot();
    const double approach_speed = horizontal_speed(approach);
    require(approach.player_grounded && approach_speed > 5.0 && approach_speed <= 5.55,
            "ordinary ground run reaches the existing 5.5 m/s walking speed");

    require(run.request_jump(), "physical jump request accepted");
    require(run.set_move_input(1.0, 0.0), "direction remains held through takeoff");
    require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "directional physical takeoff tick accepted");
    const auto takeoff = run.snapshot();
    require(!takeoff.player_grounded && takeoff.support_entity_id == 0 &&
                takeoff.traversal_state == TraversalState::None &&
                takeoff.traversal_hand_constraint_count == 0 &&
                takeoff.player_linear_velocity.y > 5.0,
            "ordinary jump is airborne without support or traversal");
    const double takeoff_speed = horizontal_speed(takeoff);

    require(run.set_move_input(0.0, 0.0), "neutral airborne input accepted");
    for (int tick = 0; tick < 18; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "neutral airborne tick accepted");
        const auto state = run.snapshot();
        require(!state.player_grounded && state.support_entity_id == 0 &&
                    state.traversal_state == TraversalState::None &&
                    state.traversal_hand_constraint_count == 0,
                "neutral-air sample remains airborne without contact or traversal");
    }

    const auto after_neutral = run.snapshot();
    const double neutral_speed = horizontal_speed(after_neutral);
    std::cout << "INFO neutral-air momentum approach=" << approach_speed
              << " takeoff=" << takeoff_speed << " after_18_ticks=" << neutral_speed
              << " delta=" << neutral_speed - takeoff_speed << " y="
              << after_neutral.player_position.y << " vy="
              << after_neutral.player_linear_velocity.y << '\n';
    require(neutral_speed >= takeoff_speed - 0.25,
            "neutral airborne input preserves horizontal momentum without contact or forces");
}

void sprint_keeps_jump_momentum() {
    using scraperx::sim::Simulation;
    using scraperx::sim::InitialSpawn;
    Simulation sprint(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::GroundFoundation);
    require(sprint.advance_frame(1.0).accepted, "the sprint settle interval must be accepted");
    // Runs along the deck, east (+1) or west (-1).
    const auto run_east = [&](const double seconds, double &peak, bool &flag,
                              const double way = 1.0) {
        const auto ticks = static_cast<std::uint32_t>(seconds * Simulation::kTickRateHz);
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            peak = std::max(peak, horizontal_speed(sprint.snapshot()));
            flag = flag || sprint.snapshot().player_sprinting;
        }
    };
    // A jump east at whatever speed the body has, stick held: its distance
    // and its slowest speed in the air.
    const auto jump_east = [&](double &distance, double &air_slowest, const double way = 1.0) {
        const double x0 = sprint.snapshot().player_position.x;
        (void)sprint.request_jump();
        air_slowest = std::numeric_limits<double>::infinity();
        for (std::uint32_t tick = 0; tick < 2U * Simulation::kTickRateHz; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            const auto state = sprint.snapshot();
            if (!state.player_grounded) {
                air_slowest = std::min(air_slowest, horizontal_speed(state));
            } else if (tick > 10U) {
                break;
            }
        }
        distance = (sprint.snapshot().player_position.x - x0) * way;
    };
    (void)sprint.set_sprint_input(true);
    double sprint_peak = 0.0;
    bool sprint_flag = false;
    run_east(2.0, sprint_peak, sprint_flag);
    double sprint_jump = 0.0;
    double sprint_air_slowest = 0.0;
    jump_east(sprint_jump, sprint_air_slowest);
    (void)sprint.set_sprint_input(false);
    // Walking back west: settled to a walk, then measured.
    double settle_peak = 0.0;
    bool settle_flag = false;
    run_east(0.6, settle_peak, settle_flag, -1.0);
    double walk_peak = 0.0;
    bool walk_flag = false;
    run_east(1.0, walk_peak, walk_flag, -1.0);
    double walk_jump = 0.0;
    double walk_air_slowest = 0.0;
    jump_east(walk_jump, walk_air_slowest, -1.0);
    require(sprint_flag && sprint_peak >= 7.9 && sprint_peak <= 8.05,
            "sprinting, the body must run at 8 m/s");
    require(sprint_air_slowest >= 7.9, "a sprinting jump must keep its speed in the air");
    require(sprint_jump >= walk_jump + 2.0,
            "a sprinting jump must carry at least 2 m past a walking one");
    require(!walk_flag && walk_peak <= 5.55, "without sprint held the body walks at 5.5 m/s");
    (void)sprint.set_sprint_input(true);
    (void)sprint.set_crouch_input(true);
    (void)sprint.advance_frame(0.3);
    double crouched_peak = 0.0;
    bool crouched_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(-1.0, 0.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 45U) {
            crouched_peak = std::max(crouched_peak, horizontal_speed(sprint.snapshot()));
        }
        crouched_flag = crouched_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_crouch_input(false);
    (void)sprint.set_move_input(0.0, 0.0);
    (void)sprint.advance_frame(0.5);
    double across_peak = 0.0;
    bool across_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(0.0, 1.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        across_peak = std::max(across_peak, horizontal_speed(sprint.snapshot()));
        across_flag = across_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_move_input(0.0, 0.0);
    require(!crouched_flag && crouched_peak <= 2.6, "crouched, sprint held must not sprint");
    require(!across_flag && across_peak <= 5.55,
            "with the stick across the facing, sprint held must not sprint");
    std::cout << "PASS scraperx_sim step2 sprint: peak=" << sprint_peak
              << " air_slowest=" << sprint_air_slowest << " sprint_jump_m=" << sprint_jump
              << " walk_jump_m=" << walk_jump << " crouched_peak=" << crouched_peak
              << " across_peak=" << across_peak << '\n';

}

} // namespace

int main() {
    vault_carries_entry_and_exit_speed();
    neutral_airborne_input_preserves_horizontal_momentum();
    sprint_keeps_jump_momentum();
    rendered_pose_fills_between_fixed_ticks();
    std::cout << "PASS scraperx_sim parkour flow\n";
    return EXIT_SUCCESS;
}
