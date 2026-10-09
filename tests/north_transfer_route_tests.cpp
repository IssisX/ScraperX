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
constexpr std::uint64_t kTransfer = 1938;
constexpr std::uint64_t kWorld = Simulation::kWorldSolidEntityId;
const char *phase = "entry418";

double horizontal_speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}

void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    std::cout << "NORTH_TRANSFER_CSV," << phase << ',' << event << ',' << v.tick_index << ','
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
    std::cerr << "FAIL NORTH_TRANSFER phase=" << phase << " reason=" << why << '\n';
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

// Normal Slingshot world only. One initial relocation is the already accepted
// 418.25 m roof. Every subsequent position/contact is earned by ordinary input.
void approach(Simulation &s) {
    walk(s, -21.0, -128.5, "north418_band");
    walk(s, 0.0, -128.0, "north418_toe_approach");
    walk(s, 0.0, -126.73, "lower_brace_toe418");
    walk(s, -21.5, -126.73, "lower_brace_ascent429", 4500);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kWorld &&
                s.snapshot().player_position.y > 428.8,
            "actual existing lower brace earns entry height", s);
    inward_jump(s, -21.5, -128.3, kTransfer, 429.9, "entry429_inward_jump");
}

void handrail(Simulation &s) {
    walk(s, -19.6, -128.2, "rail_jump_runup");
    require(footing(s.snapshot(), kTransfer, 429.9), "rail run-up has actual entry rest contact", s);
    const auto start = s.snapshot();
    begin_phase(s, "jump_action_catch1938");
    require(s.set_facing(0, 1) && s.set_move_input(.5, 0) && s.request_jump(),
            "ordinary supported Jump toward actual rail accepted", s);
    bool airborne = false, action = false, caught = false;
    for (int i = 0; i < 135; ++i) {
        tick(s);
        const auto v = s.snapshot();
        airborne = airborne || !v.player_grounded;
        if (v.traversal_state == TraversalState::Climbing &&
            v.traversal_support_entity_id == kTransfer && v.traversal_hand_constraint_count == 2) {
            caught = true;
            break;
        }
        if (!action && !v.player_grounded && v.grip_available && v.grip_entity_id == kTransfer) {
            require(s.request_traversal(), "Action requests actual airborne rail grip", s);
            action = true;
            sample(s, "action_requested");
        }
    }
    require(airborne && action && caught, "Jump/Action acquires two actual rail hands", s);
    const auto first = s.snapshot();
    sample(s, "caught");
    begin_phase(s, "finite_rail_regrip");
    bool regripped = false, end_reached = false;
    for (int i = 0; i < 1800; ++i) {
        const auto v = s.snapshot();
        require(v.traversal_state == TraversalState::Climbing &&
                    v.traversal_support_entity_id == kTransfer &&
                    v.traversal_hand_constraint_count == 2 && !v.player_swinging,
                "static rail retains actual finite hands", s);
        const double hand_x = .5 * (v.traversal_left_hand.x + v.traversal_right_hand.x);
        regripped = regripped || v.traversal_left_hand_generation > first.traversal_left_hand_generation ||
                                v.traversal_right_hand_generation > first.traversal_right_hand_generation;
        // Receiver starts at -16.2; centre >= -15.85 puts the full .35m
        // footprint over it. Do not demand travel beyond the actual bar end.
        if (hand_x > -15.95 && v.player_position.x >= -15.85) {
            end_reached = true;
            break;
        }
        // Facing remains toward the rail (+Z); +X is lateral movement, not
        // a pose override. The authored level rail uses ordinary sideways
        // regripping; finite grip queries bound every receiving hand target.
        require(s.set_move_input(.99, 0), "ordinary lateral rail input accepted", s);
        tick(s);
    }
    require(end_reached && regripped && s.snapshot().traversal_actuator_positive_work_j >
                first.traversal_actuator_positive_work_j,
            "actual regrips and finite hand work traverse rail", s);
    sample(s, "rail_end");
    begin_phase(s, "rail_drop_to_real_endrest");
    const auto attached = s.snapshot();
    require(attached.player_position.x > -16.2 && attached.player_position.x < -14.2 &&
                attached.player_position.z > -129.2 && attached.player_position.z < -127.1 &&
                .5 * (attached.traversal_left_hand.x + attached.traversal_right_hand.x) > -15.95 &&
                attached.traversal_actuator_positive_work_j > first.traversal_actuator_positive_work_j,
            "loaded real hands carry body above actual receiving rest before Drop", s);
    sample(s, "before_drop");
    require(s.set_move_input(0, 0) && s.request_release(),
            "ordinary Drop requests passive release over actual receiver", s);
    const auto queued = s.snapshot();
    require(queued.player_position.x == attached.player_position.x &&
                queued.player_position.y == attached.player_position.y &&
                queued.player_position.z == attached.player_position.z &&
                queued.player_linear_velocity.x == attached.player_linear_velocity.x &&
                queued.player_linear_velocity.y == attached.player_linear_velocity.y &&
                queued.player_linear_velocity.z == attached.player_linear_velocity.z,
            "Drop request does not instantly rewrite native pose or momentum", s);
    sample(s, "drop_requested");
    tick(s);
    require(s.snapshot().traversal_state == TraversalState::None &&
                s.snapshot().traversal_hand_constraint_count == 0,
            "ordinary Drop actually detaches hands", s);
    sample(s, "drop_first_native_tick");
    bool landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, -15.2, -128.4);
        tick(s);
        const auto v = s.snapshot();
        require(v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
                "released hand-to-foot transfer remains ordinary gravity-on motion", s);
        if (footing(v, kTransfer, 429.9) && v.player_position.x > -16.2) {
            landed = true;
            break;
        }
    }
    require(landed && s.snapshot().landing_count > start.landing_count,
            "passive rail release reaches actual end-rest foot contact", s);
    walk(s, -15.0, -128.4, "endrest_braking");
    stable(s, kTransfer, 429.9);
}

void balance_and_pocket(Simulation &s) {
    begin_phase(s, "narrow_balance");
    bool balancing = false;
    for (int i = 0; i < 1200; ++i) {
        steer(s, -10.0, -128.4);
        tick(s);
        const auto v = s.snapshot();
        balancing = balancing || (v.player_balancing && v.player_grounded && v.support_entity_id == kTransfer);
        if (v.player_grounded && std::hypot(v.player_position.x + 10.0,
                                           v.player_position.z + 128.4) < .12 && horizontal_speed(v) < .15)
            break;
    }
    require(balancing, "actual narrow beam activates balance under real contact", s);
    walk(s, -9.5, -128.4, "pocket_approach");
    require(s.set_crouch_input(true), "ordinary held crouch accepted", s);
    walk(s, -9.5, -129.5, "sheltered_crouch_pocket");
    require(s.snapshot().player_crouched && s.snapshot().player_grounded &&
                s.snapshot().support_entity_id == kTransfer,
            "short native capsule enters actual sheltered pocket", s);
    require(s.set_crouch_input(false), "ordinary stand request accepted", s);
    tick(s, 30);
    require(s.snapshot().player_crouched && s.snapshot().player_grounded,
            "actual low header prevents standing inside pocket", s);
    sample(s, "blocked_stand");
    require(s.set_crouch_input(true), "ordinary exit crouch accepted", s);
    walk(s, -9.5, -128.4, "pocket_exit");
    require(s.set_crouch_input(false), "ordinary stand after clearance accepted", s);
    tick(s, 45);
    require(footing(s.snapshot(), kTransfer, 429.9), "real beam clearance permits standing", s);
}

void gap_jump(Simulation &s) {
    walk(s, -11.0, -128.4, "offset_gap_runup");
    begin_phase(s, "supported_run_jump");
    require(s.set_facing(1, 0) && s.set_move_input(1, 0), "ordinary gap run-up accepted", s);
    bool launched = false;
    for (int i = 0; i < 270; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.player_grounded && v.support_entity_id == kTransfer,
                "run-up remains on actual narrow beam until Jump", s);
        if (v.player_position.x >= -8.8) {
            require(v.player_position.x < -8.4 && v.player_linear_velocity.x > 2.0 &&
                        s.set_facing(1, -.2) && s.request_jump(),
                    "single gap Jump starts before real unsupported edge with earned speed", s);
            launched = true;
            break;
        }
    }
    require(launched, "run-up reaches ordinary gap takeoff", s);
    const auto start = s.snapshot();
    begin_phase(s, "offset_gap_free_flight");
    bool airborne = false, landed = false;
    for (int i = 0; i < 360; ++i) {
        steer(s, -4.4, -129.15);
        tick(s);
        const auto v = s.snapshot();
        require(v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
                "offset jump remains ordinary unassisted free flight", s);
        airborne = airborne || (!v.player_grounded && v.support_entity_id == 0);
        if (airborne && v.player_grounded) {
            require(footing(v, kTransfer, 429.9) && v.player_position.x >= -5.2 &&
                        v.player_position.z >= -129.65 && v.player_position.z <= -128.65,
                    "first receiving contact is actual offset landing pad", s);
            landed = true;
            break;
        }
    }
    require(airborne && landed && s.snapshot().landing_jump_work_j > start.landing_jump_work_j,
            "finite ordinary push crosses actual3.2m gap", s);
    walk(s, -4.4, -129.15, "offset_receiver_braking");
    stable(s, kTransfer, 429.9);
}

void miss_and_retry(Simulation &s) {
    walk(s, -12.0, -128.4, "earned_beam_miss_start");
    require(footing(s.snapshot(), kTransfer, 429.9), "miss starts on earned beam contact", s);
    const auto start = s.snapshot();
    begin_phase(s, "deliberate_beam_walkoff");
    require(s.set_facing(0, -1) && s.set_move_input(0, -1), "ordinary beam walk-off accepted", s);
    bool falling = false, landed = false;
    std::uint64_t first_loss_tick = 0;
    bool corrective_input = false;
    for (int i = 0; i < 720; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
                "deliberate miss has no Jump/catch/assisted traversal", s);
        if (!falling && !v.player_grounded && v.support_entity_id == 0) {
            falling = true;
            first_loss_tick = v.tick_index;
            sample(s, "actual_beam_support_loss");
        }
        // Neutral input preserves existing air momentum. A delayed ordinary
        // opposite stick brakes it through native finite air force, rather
        // than pretending release stops a body or adding a catch platform.
        if (falling && v.tick_index - first_loss_tick >= 23) {
            steer(s, -12.0, -129.3);
            if (!corrective_input) sample(s, "delayed_air_countersteer");
            corrective_input = true;
        } else if (v.player_position.z < -129.1) {
            require(s.set_move_input(0, 0), "release walking stick while preserving air momentum", s);
        }
        if (falling && v.player_grounded && v.support_entity_id == kWorld &&
            std::abs(v.player_position.y - 419.15) < .25) {
            landed = true;
            break;
        }
    }
    require(falling && corrective_input && landed && s.snapshot().landing_count > start.landing_count &&
                s.snapshot().last_impact_speed_mps > 8.0,
            "real beam miss falls onto actual418floor on same life", s);
    sample(s, "actual418_recovery_impact");
    walk(s, -12.0, -129.3, "recovery_braking");
    tick(s, 180);
    stable(s, kWorld, 419.15);
    walk(s, -21.0, -128.5, "recovery_north418_reset_approach");
    approach(s);
    handrail(s);
}

void route(const std::string &mode) {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    require(s.entity_body_count(kTransfer) == 1, "actual1938 transfer geometry exists in normal world", s);
    require(s.debug_restart_at({-21.0, 419.15, -142.0}), "single actual418roof staging accepted", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary standing input accepted", s);
    tick(s, 45);
    stable(s, kWorld, 419.15);
    approach(s);
    if (mode == "approach") {
        std::cout << "PASS NORTH_TRANSFER approach418_to429 full_route=0 initial_staging_only=1\n";
        return;
    }
    handrail(s);
    if (mode == "miss-retry") miss_and_retry(s);
    balance_and_pocket(s);
    gap_jump(s);
    walk(s, 0, -129.15, "429_onward_to_upper_toe");
    walk(s, 0, -127.185, "upper_brace_toe429");
    walk(s, -20.0, -127.185, "upper_brace_ascent440", 4500);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kWorld &&
                s.snapshot().player_position.y > 439.0,
            "actual existing upper brace earns440floor height", s);
    inward_jump(s, -20.0, -129.0, kWorld, 441.15, "actual440_floor_jump");
    walk(s, -20.0, -142.0, "onward440_west_roof");
    stable(s, kWorld, 441.15);
    std::cout << "PASS NORTH_TRANSFER mode=" << mode
              << " ordinary_inputs_only=1 initial_staging_only=1 full_route=1 rail_regrip=1 "
                 "real_hand_to_foot_transfer=1 balance=1 low_clearance=1 offset_gap_jump=1 "
                 "stable_onward_contact=1 gravity=1 deaths=0 miss_retry=" << (mode == "miss-retry") << '\n';
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "NORTH_TRANSFER_CSV_HEADER,phase,event,tick,x,y,z,vx,vy,vz,grounded,support,traversal,"
                 "traversal_support,grip_available,grip_entity,swinging,hands,gravity,deaths,landings,"
                 "impact_mps,jump_work_j,hand_positive_work_j,firm_footing,balance,crouch,left_hand_x,"
                 "left_hand_y,left_hand_z,right_hand_x,right_hand_y,right_hand_z,left_generation,"
                 "right_generation,foot_transfer_count,foot_transfer_support,foot_transfer_peak_n\n";
    const std::string mode = argc > 1 ? argv[1] : "primary";
    if (mode != "primary" && mode != "approach" && mode != "miss-retry") {
        std::cerr << "FAIL NORTH_TRANSFER unknown mode\n";
        return EXIT_FAILURE;
    }
    route(mode);
    return EXIT_SUCCESS;
}
