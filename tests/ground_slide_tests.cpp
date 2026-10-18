#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "GROUND_SLIDE FAIL " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void tick(Simulation &sim, int count = 1) {
    for (int i = 0; i < count; ++i)
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fixed physics step rejected");
}
double horizontal_speed(const Snapshot &state) {
    return std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.z);
}
void require_slide(const Snapshot &state) {
    require(state.player_sliding && state.player_crouched && state.player_grounded &&
            state.support_entity_id == 1 && state.death_count == 0,
            "slide lost solved rigid-grade support or compact stance");
    require(std::isfinite(state.slide_speed_mps) && state.slide_speed_mps > 0 &&
            std::abs(state.slide_speed_mps - horizontal_speed(state)) < .05,
            "slide receipt differs from measured tangent speed on stationary grade");
}
void stage(Simulation &sim) {
    // Use the clear aisle between stock rows centred at z=-49 and z=-62.
    require(sim.debug_restart_at({40, .9, -55.5}), "broad grade staging rejected");
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false) &&
            sim.set_crouch_input(false) && sim.set_facing(1, 0),
            "neutral standing input rejected");
    tick(sim, 90);
    const auto state = sim.snapshot();
    require(state.player_grounded && state.support_entity_id == 1 && state.death_count == 0,
            "staging did not establish real stationary grade support");
}
Snapshot enter(Simulation &sim) {
    stage(sim);
    require(sim.set_move_input(1, 0) && sim.set_sprint_input(true), "sprint input rejected");
    bool earned = false;
    for (int i = 0; i < 180; ++i) {
        tick(sim);
        const auto state = sim.snapshot();
        if (state.player_sprinting && state.player_grounded &&
            state.support_entity_id == 1 && state.player_linear_velocity.x > 7.5) {
            earned = true;
            break;
        }
    }
    require(earned, "public sprint never earned running momentum on grade");
    const auto before = sim.snapshot();
    require(sim.set_crouch_input(true), "crouch input rejected");
    tick(sim);
    const auto after = sim.snapshot();
    require_slide(after);
    require(after.slide_count == before.slide_count + 1 &&
            std::abs(after.player_linear_velocity.x - before.player_linear_velocity.x) < .5,
            "slide entry replaced earned velocity or failed to emit one entry receipt");
    return after;
}
void glide(Simulation &sim) {
    const auto start = enter(sim);
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false), "neutral glide input rejected");
    for (int i = 0; i < 45; ++i) {
        tick(sim);
        require_slide(sim.snapshot());
        require(sim.snapshot().slide_count == start.slide_count, "held crouch replayed slide entry");
    }
    const auto end = sim.snapshot();
    const double lost = start.slide_speed_mps - end.slide_speed_mps;
    require(lost > .1 && lost < 2.0 && end.player_position.x - start.player_position.x > 2.5,
            "neutral slide failed to glide with bounded dissipative deceleration");
    std::cout << "glide start_mps=" << start.slide_speed_mps << " end_mps=" << end.slide_speed_mps
              << " distance_m=" << end.player_position.x - start.player_position.x << '\n';
}
void flow(Simulation &sim) {
    const auto start = enter(sim);
    tick(sim, 27);
    const auto sliding = sim.snapshot();
    require_slide(sliding);
    require(sliding.player_position.x - start.player_position.x > 1.5 &&
            sliding.slide_speed_mps > 5.0,
            "crouch-slide did not cover ground while retaining running momentum");
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false) &&
            sim.set_crouch_input(false), "stand release rejected");
    tick(sim);
    const auto standing = sim.snapshot();
    require(!standing.player_crouched && !standing.player_sliding && standing.player_grounded &&
            standing.support_entity_id == 1 && standing.slide_speed_mps == 0 &&
            standing.slide_count == start.slide_count &&
            horizontal_speed(standing) > .85 * horizontal_speed(sliding),
            "standing cleared earned momentum or retained slide state");
    // Compare with ordinary crouch earned from rest, rather than a private
    // controller target or a test velocity injection.
    stage(sim);
    require(sim.set_crouch_input(true) && sim.set_move_input(1, 0), "slow crouch input rejected");
    tick(sim, 90);
    require(!sim.snapshot().player_sliding && sim.snapshot().player_crouched &&
            horizontal_speed(sim.snapshot()) > .5 &&
            sliding.slide_speed_mps > 1.8 * horizontal_speed(sim.snapshot()),
            "slide failed to retain markedly more speed than ordinary crouch walking");
}
void brake(Simulation &sim) {
    enter(sim);
    require(sim.set_move_input(0, 0), "neutral control input rejected");
    tick(sim, 18);
    const auto neutral = sim.snapshot();
    require_slide(neutral);
    enter(sim);
    require(sim.set_move_input(-1, 0), "reverse braking input rejected");
    tick(sim, 18);
    const auto braking = sim.snapshot();
    require_slide(braking);
    require(braking.slide_speed_mps < neutral.slide_speed_mps - .5 &&
            braking.player_linear_velocity.x > 0,
            "reverse input failed to brake existing travel before reversing it");
}
void steer(Simulation &sim) {
    const auto before = enter(sim);
    require(sim.set_move_input(0, 1), "lateral steering input rejected");
    for (int i = 0; i < 27; ++i) {
        tick(sim);
        require_slide(sim.snapshot());
    }
    const auto after = sim.snapshot();
    require(after.player_linear_velocity.z > before.player_linear_velocity.z + .3 &&
            after.player_position.z > before.player_position.z + .03 &&
            after.player_linear_velocity.x > 5 &&
            horizontal_speed(after) <= horizontal_speed(before) + .1,
            "lateral input failed to steer a dissipative forward slide");
}
void jump(Simulation &sim) {
    enter(sim);
    tick(sim, 9);
    const auto before = sim.snapshot();
    require_slide(before);
    require(sim.request_jump(), "slide Jump request rejected");
    bool took_off = false;
    for (int i = 0; i < 18; ++i) {
        tick(sim);
        const auto state = sim.snapshot();
        if (!state.player_grounded && state.player_linear_velocity.y > 1) {
            require(!state.player_crouched && !state.player_sliding && state.slide_speed_mps == 0 &&
                    state.player_linear_velocity.x > .8 * before.player_linear_velocity.x &&
                    state.landing_jump_work_j > before.landing_jump_work_j &&
                    state.slide_count == before.slide_count && state.death_count == 0,
                    "slide Jump failed to stand, physically push off, and carry tangent momentum");
            took_off = true;
            break;
        }
    }
    require(took_off, "slide Jump never left actual support");
}
void guards(Simulation &sim) {
    enter(sim);
    stage(sim);
    require(!sim.snapshot().player_sliding && sim.snapshot().slide_speed_mps == 0 &&
            sim.snapshot().slide_count == 0, "restart retained transient slide state");
    require(sim.set_crouch_input(true) && sim.set_move_input(1, 0), "walking crouch input rejected");
    for (int i = 0; i < 90; ++i) {
        tick(sim);
        const auto state = sim.snapshot();
        require(!state.player_sliding && state.slide_count == 0 && state.slide_speed_mps == 0,
                "slow crouch fabricated a slide");
    }
    require(sim.debug_restart_at({40, 6, -55.5}), "airborne staging rejected");
    require(sim.set_facing(1, 0) && sim.set_sprint_input(true) &&
            sim.set_move_input(1, 0) && sim.set_crouch_input(true), "airborne crouch input rejected");
    for (int i = 0; i < 18; ++i) {
        tick(sim);
        const auto state = sim.snapshot();
        require(!state.player_grounded && !state.player_sliding && state.slide_speed_mps == 0 &&
                state.slide_count == 0, "airborne crouch fabricated solved slide support");
    }
}
} // namespace

int main(int argc, char **argv) {
    require(argc == 2, "expected one mode: flow, glide, brake, steer, jump, guards");
    const std::string mode = argv[1];
    Simulation sim;
    if (mode == "flow") flow(sim);
    else if (mode == "glide") glide(sim);
    else if (mode == "brake") brake(sim);
    else if (mode == "steer") steer(sim);
    else if (mode == "jump") jump(sim);
    else if (mode == "guards") guards(sim);
    else require(false, "unknown ground slide mode");
    std::cout << "GROUND_SLIDE PASS mode=" << mode << '\n';
}
