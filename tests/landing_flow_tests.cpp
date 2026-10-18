#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "LANDING_FLOW FAIL " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void tick(Simulation &sim, const int count = 1) {
    for (int i = 0; i < count; ++i)
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fixed physics step rejected");
}
double speed(const Snapshot &state) {
    return std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.z);
}
Snapshot fall(Simulation &sim, const bool prepared, const bool forward = true,
              const double height = 5.9, const double x = 40, const double z = -55.5) {
    require(sim.debug_restart_at({x, height, z}), "yard staging rejected");
    require(sim.set_facing(forward ? 1 : -1, 0) && sim.set_move_input(1, 0) &&
            sim.set_sprint_input(true) && sim.set_crouch_input(prepared),
            "ordinary approach controls rejected");
    const auto before = sim.snapshot().landing_count;
    for (int i = 0; i < 270; ++i) {
        tick(sim);
        if (sim.snapshot().landing_count > before) return sim.snapshot();
    }
    require(false, "actual fall produced no landing");
    return {};
}
void require_moderate_impact(const Snapshot &impact) {
    require(impact.player_grounded && impact.support_entity_id == 1 && impact.death_count == 0 &&
            impact.landing_normal_speed_mps >= 8 && impact.landing_normal_speed_mps < 12 &&
            impact.landing_tangent_speed_mps > 2.5,
            "fixture did not reach a moderate forward impact on actual grade");
}
void flow() {
    Simulation prepared;
    const auto impact = fall(prepared, true);
    require_moderate_impact(impact);
    require(prepared.set_crouch_input(false), "post-impact stand intent rejected");
    tick(prepared, 18);
    const auto carried = prepared.snapshot();
    std::cout << "prepared normal_mps=" << impact.landing_normal_speed_mps
              << " response=" << impact.landing_response << " arrival_mps=" << speed(impact)
              << " after_0.2s_mps=" << speed(carried)
              << " distance_m=" << carried.player_position.x - impact.player_position.x << std::endl;
    require(impact.landing_response == 2 && carried.player_crouched &&
            carried.landing_recovering && !carried.player_sliding && carried.death_count == 0 &&
            speed(carried) > .9 * speed(impact),
            "prepared moderate landing failed to carry momentum through compact recovery");

    Simulation unprepared;
    const auto braced = fall(unprepared, false);
    require_moderate_impact(braced);
    tick(unprepared, 18);
    require(braced.landing_response == 1 && unprepared.snapshot().death_count == 0 &&
            std::abs(braced.landing_normal_speed_mps - impact.landing_normal_speed_mps) < .05 &&
            std::abs(braced.landing_observed_normal_impulse_ns -
                     impact.landing_observed_normal_impulse_ns) < 5 &&
            speed(unprepared.snapshot()) < speed(carried) - .5,
            "preparation changed impact physics or failed to distinguish bracing");
    for (int i = 0; i < 180 && (prepared.snapshot().landing_recovering ||
                               prepared.snapshot().player_crouched); ++i) tick(prepared);
    require(prepared.snapshot().player_grounded && !prepared.snapshot().player_crouched &&
            !prepared.snapshot().landing_recovering &&
            speed(prepared.snapshot()) > .85 * speed(impact),
            "compact recovery failed to hand back running with earned momentum");
}
void jump() {
    Simulation sim;
    const auto impact = fall(sim, true);
    require_moderate_impact(impact);
    require(impact.landing_response == 2, "prepared moderate landing did not enter compact recovery");
    tick(sim, 6);
    const auto before = sim.snapshot();
    require(before.player_crouched && before.landing_recovering, "no actual compact recovery to exit");
    require(sim.request_jump(), "onward Jump rejected");
    tick(sim);
    const auto after = sim.snapshot();
    require(!after.player_grounded && !after.player_crouched && !after.player_sliding &&
            !after.landing_recovering && after.player_linear_velocity.y > 5 &&
            std::abs(after.player_linear_velocity.x - before.player_linear_velocity.x) < .15 &&
            after.landing_jump_work_j > before.landing_jump_work_j,
            "onward Jump failed to preserve actual momentum and perform real takeoff work");
}
void guards() {
    Simulation sideways;
    const auto crossed = fall(sideways, true, false);
    require_moderate_impact(crossed);
    require(crossed.landing_response == 1,
            "sideways arrival selected a forward compact roll");
    // The stock row at x=27 blocks the runout after real grade contact.
    Simulation obstructed;
    const auto blocked = fall(obstructed, true, true, 5.9, 20, -50);
    require_moderate_impact(blocked);
    require(blocked.landing_response == 1,
            "blocked compact-capsule runout selected a roll");
    Simulation light;
    const auto ordinary = fall(light, true, true, 3.0);
    require(ordinary.landing_count > 0 && ordinary.death_count == 0 &&
            ordinary.landing_normal_speed_mps < 8 && ordinary.landing_response == 0,
            "light landing acquired an unnecessary recovery phase");
}
} // namespace

int main(int argc, char **argv) {
    require(argc == 2, "expected flow, jump or guards mode");
    const std::string mode = argv[1];
    if (mode == "flow") flow();
    else if (mode == "jump") jump();
    else if (mode == "guards") guards();
    else require(false, "unknown landing flow mode");
    std::cout << "LANDING_FLOW PASS mode=" << mode << '\n';
    return EXIT_SUCCESS;
}
