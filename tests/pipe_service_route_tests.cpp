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
constexpr std::uint64_t kService = 1939;
constexpr std::uint64_t kWorld = Simulation::kWorldSolidEntityId;
const char *phase = "entry440";

double horizontal_speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}

void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    std::cout << "PIPE_SERVICE_CSV," << phase << ',' << event << ',' << v.tick_index << ','
              << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z << ','
              << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ','
              << v.player_linear_velocity.z << ',' << v.player_grounded << ',' << v.support_entity_id << ','
              << static_cast<int>(v.traversal_state) << ',' << v.traversal_support_entity_id << ','
              << v.grip_available << ',' << v.grip_entity_id << ',' << v.player_swinging << ','
              << v.traversal_hand_constraint_count << ',' << v.player_gravity_factor << ','
              << v.death_count << ',' << v.landing_count << ',' << v.last_impact_speed_mps << ','
              << v.landing_jump_work_j << ',' << v.traversal_actuator_positive_work_j << ','
              << v.checkpoint_footing_valid << ',' << v.player_balancing << ',' << v.player_crouched << ','
              << v.traversal_left_hand.x << ',' << v.traversal_left_hand.y << ',' << v.traversal_left_hand.z << ','
              << v.traversal_right_hand.x << ',' << v.traversal_right_hand.y << ',' << v.traversal_right_hand.z << ','
              << v.traversal_left_hand_generation << ',' << v.traversal_right_hand_generation << ','
              << v.foot_transfer_count << ',' << v.foot_transfer_support_entity_id << ','
              << v.foot_transfer_peak_hand_load_n << std::endl;
}

void require(bool condition, const char *why, const Simulation &s) {
    if (condition) return;
    sample(s, "failure");
    std::cerr << "FAIL PIPE_SERVICE phase=" << phase << " reason=" << why << '\n';
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
        require(v.player_gravity_factor == 1.0 && v.death_count == 0 && !v.parachute_deployed &&
                    v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0 &&
                    v.jump_vault_count == 0,
                "ordinary gravity and same life without parachute or assisted traversal", s);
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

// Actual normal-world roof and original Tower diagonals. The sole staging
// operation is in route(); all later movement uses ordinary player inputs.
void approach(Simulation &s) {
    walk(s, -20.0, -130.0, "north440_band");
    walk(s, 0.0, -129.7, "north440_toe_approach");
    walk(s, 0.0, -127.64, "lower_brace_toe440");
    walk(s, -20.7, -127.64, "lower_brace_ascent451", 4500);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kWorld &&
                s.snapshot().player_position.y > 450.8,
            "actual original lower brace earns451 entry height", s);
    inward_jump(s, -20.7, -129.375, kService, 451.9, "entry451_inward_jump");
}

void quick(Simulation &s) {
    walk(s, -18.5, -129.375, "quick_pipe_runup");
    require(footing(s.snapshot(), kService, 451.9), "quick run-up has actual floor contact", s);
    begin_phase(s, "ordinary_walk_pipe_collision_probe");
    require(s.set_facing(1, 0) && s.set_move_input(1, 0), "ordinary walk-only pipe approach accepted", s);
    double furthest_x = s.snapshot().player_position.x;
    for (int i = 0; i < 180; ++i) {
        tick(s);
        furthest_x = std::max(furthest_x, s.snapshot().player_position.x);
    }
    sample(s, "walk_only_contact_result");
    std::cout << "PIPE_WALK_PROBE,furthest_x=" << furthest_x
              << ",grounded=" << s.snapshot().player_grounded
              << ",support=" << s.snapshot().support_entity_id
              << ",vaults=" << s.snapshot().jump_vault_count << '\n';
    require(furthest_x > -16.3 && furthest_x < -14.15,
            "ordinary walk reaches real pipe but cannot cross onto continuation without Jump", s);
    walk(s, -18.5, -129.375, "ordinary_backtrack_to_quick_runup");
    stable(s, kService, 451.9);
    begin_phase(s, "fresh_supported_pipe_jump");
    require(s.set_facing(1, 0) && s.set_move_input(1, 0), "ordinary exposed run-up accepted", s);
    bool requested = false;
    double work_before = 0;
    std::uint64_t vaults_before = 0;
    for (int i = 0; i < 180; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(footing(v, kService, 451.9), "fresh pipe Jump begins on actual supported floor", s);
        if (v.player_position.x >= -16.7) {
            require(v.player_position.x < -16.35 && v.player_linear_velocity.x > 3.0,
                    "quick takeoff has earned speed before pipe collision", s);
            work_before = v.landing_jump_work_j;
            vaults_before = v.jump_vault_count;
            require(s.request_jump(), "single fresh supported pipe Jump accepted", s);
            requested = true;
            sample(s, "jump_requested");
            break;
        }
    }
    require(requested, "ordinary run-up reaches pipe takeoff", s);
    begin_phase(s, "pipe_ballistic_crossing");
    bool airborne = false, cleared = false, landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, -12.5, -129.375);
        tick(s);
        const auto v = s.snapshot();
        require(v.jump_vault_count == vaults_before, "pipe crossing invokes no second-Jump legacy vault", s);
        airborne = airborne || (!v.player_grounded && v.support_entity_id == 0);
        if (std::abs(v.player_position.x + 15.0) < .12) {
            require(airborne && !v.player_grounded && v.player_position.y - .9 > 452.03,
                    "actual airborne soles clear the real pipe crest", s);
            cleared = true;
            sample(s, "actual_pipe_crest_clearance");
        }
        if (airborne && v.player_grounded) {
            require(footing(v, kService, 451.9) && v.player_position.x > -14.15,
                    "ballistic pipe crossing lands beyond pipe on actual continuation", s);
            landed = true;
            break;
        }
    }
    require(airborne && cleared && landed && s.snapshot().landing_jump_work_j > work_before,
            "single finite supported Jump crosses real pipe and earns receiving contact", s);
    walk(s, -12.5, -129.375, "quick_receiving_braking");
    stable(s, kService, 451.9);
}

void sheltered(Simulation &s) {
    walk(s, -18.0, -129.375, "shelter_choice_approach");
    walk(s, -18.0, -130.65, "shelter_entry_alignment");
    require(s.set_crouch_input(true), "ordinary crouch selects sheltered passage", s);
    walk(s, -16.2, -130.65, "actual_crouched_shelter_entry");
    require(s.snapshot().player_crouched && s.snapshot().player_grounded &&
                s.snapshot().support_entity_id == kService,
            "actual short capsule enters low shelter on real floor", s);
    require(s.set_crouch_input(false), "ordinary stand request inside actual shelter accepted", s);
    tick(s, 30);
    require(s.snapshot().player_crouched && s.snapshot().player_grounded &&
                s.snapshot().support_entity_id == kService,
            "actual1.45m roof clearance rejects standing inside shelter", s);
    sample(s, "real_shelter_blocked_stand");
    require(s.set_crouch_input(true), "ordinary held crouch continues sheltered route", s);
    walk(s, -8.0, -130.65, "long_crouched_clearance_passage");
    require(s.snapshot().player_crouched && s.snapshot().player_grounded &&
                s.snapshot().support_entity_id == kService,
            "long sheltered choice crosses pipe station with actual crouched footing", s);
    walk(s, -6.5, -130.65, "shelter_real_exit");
    require(s.set_crouch_input(false), "ordinary stand after shelter clearance accepted", s);
    tick(s, 45);
    require(footing(s.snapshot(), kService, 451.9), "real sheltered exit permits standing", s);
    stable(s, kService, 451.9);
}

void miss_and_retry(Simulation &s) {
    // The receiving toe lies above the real440northroof. A missed onward
    // alignment falls off its X end, well inside that roof's X extent. There
    // is no extra catch tray or position/velocity rewrite in this scenario.
    walk(s, -6.5, -129.375, "miss_approach_past_shelter_end");
    walk(s, -6.5, -130.5, "miss_toe_alignment");
    walk(s, .25, -130.5, "earned_toe_miss_start");
    require(footing(s.snapshot(), kService, 451.9), "miss starts on earned actual receiving toe", s);
    const auto start = s.snapshot();
    begin_phase(s, "actual_toe_walkoff");
    require(s.set_facing(1, 0) && s.set_move_input(1, 0), "ordinary receiving-toe walk-off accepted", s);
    bool falling = false, countersteered = false, landed = false;
    std::uint64_t loss_tick = 0;
    for (int i = 0; i < 720; ++i) {
        tick(s);
        const auto v = s.snapshot();
        if (!falling && !v.player_grounded && v.support_entity_id == 0) {
            falling = true;
            loss_tick = v.tick_index;
            sample(s, "actual_toe_support_loss");
        }
        if (falling && v.tick_index - loss_tick >= 23) {
            steer(s, 1.6, -130.5);
            if (!countersteered) sample(s, "delayed_ordinary_air_braking");
            countersteered = true;
        } else if (v.player_position.x > 1.5) {
            require(s.set_move_input(0, 0), "neutral stick preserves ordinary airborne momentum", s);
        }
        if (falling && v.player_grounded && v.support_entity_id == kWorld &&
            std::abs(v.player_position.y - 441.15) < .25) {
            landed = true;
            break;
        }
    }
    require(falling && countersteered && landed && s.snapshot().landing_count > start.landing_count &&
                s.snapshot().last_impact_speed_mps > 8.0,
            "actual onward miss lands on real440roof with gravity and same life", s);
    sample(s, "actual440_recovery_impact");
    walk(s, 1.6, -130.0, "actual440_recovery_braking");
    tick(s, 180);
    stable(s, kWorld, 441.15);
    approach(s);
    sheltered(s);
}

void onward(Simulation &s) {
    walk(s, 0, -129.375, "451_onward_to_upper_toe");
    walk(s, 0, -128.095, "upper_brace_toe451");
    walk(s, -20.3, -128.095, "upper_brace_ascent462", 4500);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kWorld &&
                s.snapshot().player_position.y > 461.8,
            "actual original upper brace earns462floor height", s);
    inward_jump(s, -20.3, -130.0, kWorld, 463.15, "actual462_floor_jump");
    walk(s, -19.5, -142.0, "onward462_west_roof");
    stable(s, kWorld, 463.15);
}

void route(const std::string &mode) {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    require(s.entity_body_count(kService) == 1, "real pipe service compound exists in normal world", s);
    require(s.debug_restart_at({-20.0, 441.15, -142.0}), "single actual440roof staging accepted", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary initial standing input accepted", s);
    tick(s, 45);
    stable(s, kWorld, 441.15);
    approach(s);
    if (mode == "sheltered") sheltered(s);
    else quick(s);
    if (mode == "miss-retry") miss_and_retry(s);
    onward(s);
    std::cout << "PASS PIPE_SERVICE mode=" << mode
              << " ordinary_inputs_only=1 initial_staging_only=1 full_route=1 gravity=1 deaths=0 "
                 "vaults=0 actual462floor=1 stable_onward_contact=1 quick=" << (mode != "sheltered")
              << " sheltered=" << (mode != "quick") << " miss_retry=" << (mode == "miss-retry") << '\n';
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "PIPE_SERVICE_CSV_HEADER,phase,event,tick,x,y,z,vx,vy,vz,grounded,support,traversal,"
                 "traversal_support,grip_available,grip_entity,swinging,hands,gravity,deaths,landings,"
                 "impact_mps,jump_work_j,hand_positive_work_j,firm_footing,balance,crouch,left_hand_x,"
                 "left_hand_y,left_hand_z,right_hand_x,right_hand_y,right_hand_z,left_generation,"
                 "right_generation,foot_transfer_count,foot_transfer_support,foot_transfer_peak_n\n";
    const std::string mode = argc > 1 ? argv[1] : "quick";
    if (mode != "quick" && mode != "sheltered" && mode != "miss-retry") {
        std::cerr << "FAIL PIPE_SERVICE unknown mode\n";
        return EXIT_FAILURE;
    }
    route(mode);
    return EXIT_SUCCESS;
}
