#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
constexpr std::uint64_t kFrame = 1935;
constexpr std::uint64_t kRecovery = 1936;
constexpr std::uint64_t kHanger = 2935;
constexpr std::uint64_t kWorld = Simulation::kWorldSolidEntityId;
const char *phase = "entry396";

double horizontal_speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}

void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    std::cout << "INSPECTION_CSV," << phase << ',' << event << ',' << v.tick_index << ','
              << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z << ','
              << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ','
              << v.player_linear_velocity.z << ',' << v.player_grounded << ',' << v.support_entity_id << ','
              << static_cast<int>(v.traversal_state) << ',' << v.traversal_support_entity_id << ','
              << v.grip_available << ',' << v.grip_entity_id << ',' << v.player_swinging << ','
              << v.traversal_hand_constraint_count << ',' << v.player_gravity_factor << ','
              << v.death_count << ',' << v.landing_count << ',' << v.last_impact_speed_mps << ','
              << v.landing_jump_work_j << ',' << v.traversal_actuator_positive_work_j << ','
              << v.checkpoint_footing_valid << std::endl;
}

void require(bool condition, const char *why, const Simulation &s) {
    if (condition) return;
    sample(s, "failure");
    std::cerr << "FAIL INSPECTION_JUNCTION phase=" << phase << " reason=" << why << '\n';
    std::exit(EXIT_FAILURE);
}

void begin_phase(Simulation &s, const char *next) {
    phase = next;
    sample(s, "begin");
}

void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto result = s.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1, "one native tick accepted", s);
        const auto v = s.snapshot();
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                    std::isfinite(v.player_position.z) && std::isfinite(v.player_linear_velocity.x) &&
                    std::isfinite(v.player_linear_velocity.y) && std::isfinite(v.player_linear_velocity.z),
                "finite native motion", s);
        require(v.player_gravity_factor == 1.0 && v.death_count == 0 && !v.parachute_deployed,
                "ordinary gravity and same life without parachute", s);
        if (v.tick_index % 90 == 0) sample(s, "tick");
    }
}

// These are ordinary desired movement inputs, not a body controller. The
// bounded integral supplies uphill stick effort; native contact owns motion.
void steer(Simulation &s, double x, double z, double extra_x = 0, double extra_z = 0) {
    const auto v = s.snapshot();
    const double dx = x - v.player_position.x, dz = z - v.player_position.z;
    const double ix = 1.8 * dx - .28 * v.player_linear_velocity.x + extra_x;
    const double iz = 1.8 * dz - .28 * v.player_linear_velocity.z + extra_z;
    const double scale = std::max(1.0, std::hypot(ix, iz));
    require(s.set_move_input(ix / scale, iz / scale), "ordinary steering accepted", s);
    if (std::hypot(dx, dz) > .02)
        require(s.set_facing(dx, dz), "ordinary facing accepted", s);
}

void walk(Simulation &s, double x, double z, const char *next, int budget = 2700) {
    begin_phase(s, next);
    double effort_x = 0, effort_z = 0;
    for (int i = 0; i < budget; ++i) {
        const auto v = s.snapshot();
        const double dx = x - v.player_position.x, dz = z - v.player_position.z;
        if (v.player_grounded && std::hypot(dx, dz) < .10 && horizontal_speed(v) < .14) {
            require(s.set_move_input(0, 0), "arrival releases ordinary stick", s);
            sample(s, "arrived");
            return;
        }
        if (std::hypot(dx, dz) > .75) effort_x = effort_z = 0;
        else {
            effort_x += dx * Simulation::kFixedStepSeconds * .4;
            effort_z += dz * Simulation::kFixedStepSeconds * .4;
            const double scale = std::max(1.0, std::hypot(effort_x, effort_z) / .35);
            effort_x /= scale;
            effort_z /= scale;
        }
        steer(s, x, z, effort_x, effort_z);
        tick(s);
    }
    require(false, "bounded ordinary walking did not reach target", s);
}

bool footing(const Snapshot &v, std::uint64_t support, double standing_y) {
    return v.player_grounded && v.support_entity_id == support && !v.player_crouched &&
           std::abs(v.player_position.y - standing_y) < .15 &&
           v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0;
}

void stable(Simulation &s, std::uint64_t support, double standing_y) {
    require(s.set_move_input(0, 0), "neutral footing input accepted", s);
    for (int i = 0; i < 90; ++i) {
        tick(s);
        require(footing(s.snapshot(), support, standing_y), "real receiving contact stays supported", s);
    }
    require(s.snapshot().checkpoint_footing_valid, "receiver has native resting support patch", s);
    sample(s, "stable");
}

// A single supported Jump clears the brace/floor height difference. Air
// steering remains ordinary input; neither destination nor support is granted.
void inward_jump(Simulation &s, double x, double z, std::uint64_t support,
                 double standing_y, const char *next) {
    begin_phase(s, next);
    const auto start = s.snapshot();
    require(start.player_grounded, "inward Jump starts on actual brace contact", s);
    require(s.set_facing(x - start.player_position.x, z - start.player_position.z) &&
                s.set_move_input(0, 0) && s.request_jump(), "single ordinary inward Jump accepted", s);
    bool airborne = false, landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, x, z);
        tick(s);
        const auto v = s.snapshot();
        airborne = airborne || (!v.player_grounded && v.support_entity_id == 0);
        require(v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
                "inward Jump remains free flight without assisted traversal", s);
        if (airborne && footing(v, support, standing_y)) {
            landed = true;
            break;
        }
    }
    require(airborne && landed && s.snapshot().landing_jump_work_j > start.landing_jump_work_j,
            "finite push-off reaches actual inward receiver", s);
    walk(s, x, z, "inward_jump_braking");
    stable(s, support, standing_y);
}

void cross_hanger(Simulation &s) {
    walk(s, -16.65, -128.2, "left_transom_runup");
    require(footing(s.snapshot(), kFrame, 407.9), "run-up has actual407 platform contact", s);
    begin_phase(s, "jump_catch2935");
    require(s.set_facing(1, 0) && s.set_move_input(.55, 0) && s.request_jump(),
            "ordinary rising Jump toward hanger accepted", s);
    bool action_sent = false, caught = false;
    for (int i = 0; i < 110; ++i) {
        tick(s);
        const auto v = s.snapshot();
        if (v.player_swinging && v.traversal_support_entity_id == kHanger &&
            v.traversal_hand_constraint_count == 2) {
            caught = true;
            break;
        }
        if (!action_sent && !v.player_grounded && v.grip_available && v.grip_entity_id == kHanger) {
            require(s.request_traversal(), "Action requests actual airborne hanger grip", s);
            action_sent = true;
        }
    }
    require(caught, "rising Jump acquires two actual hanger hands", s);
    sample(s, "caught");
    begin_phase(s, "pump_and_release2935");
    bool released = false;
    for (int i = 0; i < 90 * 60; ++i) {
        const auto v = s.snapshot();
        require(v.player_swinging && v.traversal_support_entity_id == kHanger &&
                    v.traversal_hand_constraint_count == 2, "short hanger remains physically attached", s);
        // Respond to actual visible motion around the real -14m bearing.
        const double effort = v.player_linear_velocity.x -
                              1.2 * (v.traversal_left_hand.x + 14.0) > 0 ? 1.0 : -1.0;
        require(s.set_move_input(effort, 0), "ordinary player pumping input accepted", s);
        tick(s);
        const auto a = s.snapshot();
        if (a.player_position.x > -12.7 && a.player_linear_velocity.x > 1.5 &&
            a.player_linear_velocity.y > .3) {
            require(s.set_move_input(0, 0) && s.request_release(), "ordinary Drop releases rising swing", s);
            tick(s);
            require(!s.snapshot().player_swinging && s.snapshot().traversal_hand_constraint_count == 0,
                    "Drop detaches real hands", s);
            released = true;
            sample(s, "released");
            break;
        }
    }
    require(released, "short hanger reaches an observed rightward rising release window", s);
    begin_phase(s, "right407_receiver");
    bool landed = false;
    for (int i = 0; i < 540; ++i) {
        steer(s, -10.8, -128.2);
        tick(s);
        if (footing(s.snapshot(), kFrame, 407.9) && s.snapshot().player_position.x > -11.6) {
            landed = true;
            break;
        }
    }
    require(landed, "earned swing release reaches right407 receiver", s);
    walk(s, -10.8, -128.2, "right407_braking");
    stable(s, kFrame, 407.9);
}

void run_recovery() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    require(s.entity_body_count(kFrame) == 1 && s.entity_body_count(kRecovery) == 1,
            "real407 gap and403.5 catch tray exist in normal world", s);
    begin_phase(s, "recovery_left407_start");
    // This scenario has its own single supported start. It proves local
    // failure/recovery only, independently of the primary396-to418 route.
    require(s.debug_restart_at({-16.65, 407.9, -128.2}), "actual left407 staging accepted", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary standing recovery input accepted", s);
    tick(s, 45);
    stable(s, kFrame, 407.9);
    const auto before = s.snapshot();
    begin_phase(s, "deliberate_gap_walkoff");
    require(s.set_facing(1, 0) && s.set_move_input(1, 0), "ordinary walk-off input accepted", s);
    bool unsupported = false, caught = false;
    for (int i = 0; i < 540; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.traversal_state == TraversalState::None && !v.player_swinging &&
                    v.traversal_hand_constraint_count == 0,
                "miss uses no Jump, grip or assisted traversal", s);
        unsupported = unsupported || (!v.player_grounded && v.support_entity_id == 0);
        if (v.player_position.x > -15.25)
            require(s.set_move_input(0, 0), "miss releases walking stick above real catch tray", s);
        if (unsupported && footing(v, kRecovery, 404.4)) {
            caught = true;
            break;
        }
    }
    require(unsupported && caught && s.snapshot().landing_count > before.landing_count &&
                s.snapshot().last_impact_speed_mps > 4.0,
            "actual unsupported fall makes a nontrivial tray impact on the same life", s);
    sample(s, "tray_impact");
    // Native heavy-landing compression and finite braking precede standing
    // footing. Do not treat the first contact tick as a recovered stance.
    walk(s, -12.4, -129.3, "tray_landing_recovery");
    tick(s, 180);
    stable(s, kRecovery, 404.4);
    walk(s, -20.0, -129.65, "tray_to_return_brace");
    walk(s, -12.4, -129.65, "actual_return_brace_ascent");
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kRecovery &&
                s.snapshot().player_position.y > 407.35 &&
                s.snapshot().traversal_state == TraversalState::None,
            "ordinary footing on the return brace earns recovery height", s);
    walk(s, -10.8, -129.65, "return_brace_to407_receiver");
    require(footing(s.snapshot(), kFrame, 407.9),
            "walked recovery reaches actual407 receiving contact", s);
    walk(s, -10.8, -128.2, "recovered_right407_braking");
    stable(s, kFrame, 407.9);
    std::cout << "PASS INSPECTION_JUNCTION recovery407_gap_to403.5_to407 "
                 "ordinary_inputs_only=1 actual_tray_impact=1 actual_return_brace_contact=1 "
                 "gravity=1 deaths=0 initial_staging_only=1 full_route=0 gap_retry=0\n";
}

void run_route(bool approach_only) {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    require(s.entity_body_count(kFrame) == 1 && s.entity_body_count(kHanger) == 1,
            "junction and physical hanger exist in normal world", s);
    // Sole staging operation: the accepted ordinary west-brace exit. All
    // subsequent progress comes from normal movement/Jump/Action/Drop inputs.
    require(s.debug_restart_at({-22.5, 397.15, -142.0}), "actual396 roof staging accepted", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary standing input accepted", s);
    tick(s, 45);
    stable(s, kWorld, 397.15);
    walk(s, -22.5, -128.3, "north396_band");
    walk(s, 0, -127.2, "north396_toe_approach");
    walk(s, 0, -125.82, "lower_brace_toe396");
    walk(s, -22.5, -125.82, "lower_brace_ascent", 4500);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kWorld &&
                s.snapshot().player_position.y > 407.0, "actual lower diagonal earns height", s);
    inward_jump(s, -22.5, -127.3, kFrame, 407.9, "left407_inward_jump");
    if (approach_only) {
        std::cout << "PASS INSPECTION_JUNCTION approach396_to_left407 initial_staging_only=1 full_route=0\n";
        return;
    }
    cross_hanger(s);
    walk(s, 0, -128.2, "right407_to_upper_toe");
    walk(s, 0, -126.275, "upper_brace_toe407");
    walk(s, -21.5, -126.275, "upper_brace_ascent", 4500);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kWorld &&
                s.snapshot().player_position.y > 417.7, "actual upper diagonal earns height", s);
    inward_jump(s, -21.5, -128.9, kWorld, 419.15, "actual418_floor_jump");
    walk(s, -21.0, -142.0, "onward418_west_roof");
    stable(s, kWorld, 419.15);
    std::cout << "PASS INSPECTION_JUNCTION primary396_to_actual418.25 ordinary_inputs_only=1 "
                 "lower_brace=1 hanger2935=1 upper_brace=1 actual_receiving_contact=1 "
                 "onward_walking=1 gravity=1 deaths=0 initial_staging_only=1\n";
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "INSPECTION_CSV_HEADER,phase,event,tick,x,y,z,vx,vy,vz,grounded,support,traversal,"
                 "traversal_support,grip_available,grip_entity,swinging,hands,gravity,deaths,landings,"
                 "impact_mps,jump_work_j,hand_positive_work_j,firm_footing\n";
    const std::string mode = argc > 1 ? argv[1] : "primary";
    if (mode != "primary" && mode != "approach" && mode != "recovery") {
        std::cerr << "FAIL INSPECTION_JUNCTION unknown mode\n";
        return EXIT_FAILURE;
    }
    if (mode == "recovery") run_recovery();
    else run_route(mode == "approach");
    return EXIT_SUCCESS;
}
