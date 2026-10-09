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
constexpr double kX = -6.8, kZ = -128.2;
constexpr std::uint64_t kSteel = 1935;
const char *phase = "initialization";

bool wood(const Snapshot &v) {
    return v.player_grounded && v.support_entity_id >= 2880 && v.support_entity_id <= 2891;
}
double speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}
void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    std::cout << "WOOD_FOOT_PUSH phase=" << phase << " event=" << event << " tick=" << v.tick_index
              << " position=" << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z
              << " velocity=" << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ',' << v.player_linear_velocity.z
              << " grounded=" << v.player_grounded << " support=" << v.support_entity_id
              << " active=" << v.foot_push_active << " foot_support=" << v.foot_push_support_entity_id
              << " starts=" << v.foot_push_start_count << " stop=" << int(v.foot_push_stop_reason)
              << " command_j=" << v.foot_push_command_work_bound_j << " stroke_m=" << v.foot_push_stroke_m
              << " time_s=" << v.foot_push_elapsed_seconds << " peak_n=" << v.foot_push_peak_load_n
              << " impulse_ns=" << v.foot_push_last_impulse_ns << " distance_m=" << v.foot_push_actual_distance_m
              << " initial_distance_m=" << v.foot_push_initial_distance_m
              << " gravity=" << v.player_gravity_factor << " deaths=" << v.death_count
              << " broken=" << v.plank_broken_joint_mask << " peak_strength=" << v.plank_peak_strength_ratio
              << " traversal=" << int(v.traversal_state) << " hands=" << v.traversal_hand_constraint_count
              << " support_velocity=" << v.support_point_linear_velocity.x << ','
              << v.support_point_linear_velocity.y << ',' << v.support_point_linear_velocity.z << '\n';
}
void require(bool condition, const char *message, const Simulation &s) {
    if (condition) return;
    sample(s, "failure");
    std::cerr << "FAIL WOOD_FOOT_PUSH_PLAYER phase=" << phase << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto result = s.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1, "one ordinary native tick", s);
        const auto v = s.snapshot();
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                    std::isfinite(v.player_position.z) && std::isfinite(v.player_linear_velocity.y) &&
                    v.player_gravity_factor == 1 && v.death_count == 0 && !v.parachute_deployed &&
                    v.traversal_state == TraversalState::None && v.traversal_hand_constraint_count == 0,
                "actual finite player state with gravity and no assisted traversal", s);
        require(v.foot_push_command_work_bound_j <= 1285.62501 && v.foot_push_stroke_m <= .300001 &&
                    v.foot_push_peak_load_n <= 3500.01 &&
                    v.foot_push_elapsed_seconds <= .25 + Simulation::kFixedStepSeconds &&
                    v.foot_push_last_impulse_ns >= 0 &&
                    v.foot_push_last_impulse_ns <= 3500.0 / 360.0 + .0001,
                "actual last collision row and command stay within finite physical limits", s);
        if (v.foot_push_active)
            require(v.foot_push_support_entity_id >= 2880 && v.foot_push_support_entity_id <= 2891,
                    "active foot remains attached to real material, not steel or world", s);
        if (v.tick_index % 90 == 0) sample(s, "tick");
    }
}
void steer(Simulation &s, double x, double extra_x = 0, double extra_z = 0) {
    const auto v = s.snapshot();
    const double ix = 1.8 * (x - v.player_position.x) - .28 * v.player_linear_velocity.x + extra_x;
    const double iz = 1.8 * (kZ - v.player_position.z) - .28 * v.player_linear_velocity.z + extra_z;
    const double scale = std::max(1.0, std::hypot(ix, iz));
    require(s.set_move_input(ix / scale, iz / scale), "ordinary bounded stick", s);
}
void walk(Simulation &s, double x, bool expect_wood) {
    double effort_x = 0, effort_z = 0;
    for (int i = 0; i < 1350; ++i) {
        const auto v = s.snapshot();
        if ((expect_wood ? wood(v) : v.player_grounded && v.support_entity_id == kSteel) &&
            std::abs(v.player_position.x - x) < .08 && std::abs(v.player_position.z - kZ) < .03 && speed(v) < .12) {
            require(s.set_move_input(0, 0), "ordinary arrival releases stick", s);
            return;
        }
        const double dx = x - v.player_position.x, dz = kZ - v.player_position.z;
        if (std::hypot(dx, dz) > .75) effort_x = effort_z = 0;
        else {
            effort_x += .4 * dx * Simulation::kFixedStepSeconds;
            effort_z += .4 * dz * Simulation::kFixedStepSeconds;
            const double scale = std::max(1.0, std::hypot(effort_x, effort_z) / .35);
            effort_x /= scale; effort_z /= scale;
        }
        steer(s, x, effort_x, effort_z);
        tick(s);
    }
    require(false, "ordinary route reaches supported target", s);
}
void approach(Simulation &s) {
    // Actual normal Slingshot world; sole initial stage is real left steel.
    // Subsequent motion uses ordinary input only, never body/velocity/force writes.
    phase = "actual_steel_to_loaded_timber";
    require(s.debug_restart_at({-8.8, 407.9, kZ}), "sole supported steel entry", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_facing(1, 0), "ordinary standing input", s);
    tick(s, 360);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kSteel &&
                s.snapshot().checkpoint_footing_valid, "real firm steel entry", s);
    walk(s, kX, true);
    tick(s, 360);
    require(wood(s.snapshot()) && speed(s.snapshot()) < .15 && s.snapshot().plank_broken_joint_mask == 0,
            "actual85kg load rests on intact centre timber", s);
    sample(s, "loaded_centre");
}
void begin_jump(Simulation &s) {
    require(wood(s.snapshot()) && !s.snapshot().foot_push_active && s.request_jump(), "ordinary supported wood Jump", s);
    tick(s);
    require(s.snapshot().foot_push_active && s.snapshot().foot_push_command_work_bound_j > 0,
            "ordinary Jump starts actual finite leg, not instantaneous velocity assignment", s);
    sample(s, "first_finite_command");
}
void settle_wood_landing(Simulation &s) {
    phase = "actual_wood_landing_settle";
    sample(s, "begin");
    require(s.set_move_input(0, 0), "neutral ordinary input lets wood landing settle", s);
    int quiet = 0;
    for (int i = 0; i < 360; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.plank_broken_joint_mask == 0, "actual wood stays intact while landing settles", s);
        const double rx = v.player_linear_velocity.x - v.support_point_linear_velocity.x;
        const double ry = v.player_linear_velocity.y - v.support_point_linear_velocity.y;
        const double rz = v.player_linear_velocity.z - v.support_point_linear_velocity.z;
        if (wood(v) && std::abs(ry) < .1 && std::hypot(rx, rz) < .1) ++quiet;
        else quiet = 0;
        if (quiet == 30) { sample(s, "actual_settled_wood"); return; }
    }
    require(false, "real supported intact wood settles30ticks before onward movement", s);
}
void jump_and_spam() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    approach(s);
    phase = "finite_wood_jump_and_no_refresh";
    const auto before = s.snapshot();
    begin_jump(s);
    const auto attached = s.snapshot();
    bool airborne = false, landed = false;
    double peak_vy = attached.player_linear_velocity.y, apex = attached.player_position.y;
    double previous_time = attached.foot_push_elapsed_seconds, previous_work = attached.foot_push_command_work_bound_j;
    int spam_requests = 0;
    for (int i = 0; i < 540; ++i) {
        const auto v = s.snapshot();
        if (v.foot_push_active) {
            require(s.request_jump(), "repeated ordinary Jump input accepted", s);
            ++spam_requests;
        }
        steer(s, kX);
        tick(s);
        const auto now = s.snapshot();
        require(now.foot_push_start_count == before.foot_push_start_count + 1 &&
                    now.foot_push_support_entity_id == attached.foot_push_support_entity_id &&
                    now.foot_push_elapsed_seconds >= previous_time &&
                    now.foot_push_command_work_bound_j >= previous_work,
                "active Jump spam cannot refresh, rebind or reset finite command", s);
        previous_time = now.foot_push_elapsed_seconds;
        previous_work = now.foot_push_command_work_bound_j;
        peak_vy = std::max(peak_vy, now.player_linear_velocity.y);
        apex = std::max(apex, now.player_position.y);
        if (!now.player_grounded && now.support_entity_id == 0) airborne = true;
        if (airborne && wood(now) && now.landing_count > before.landing_count) { landed = true; break; }
    }
    sample(s, "jump_landing");
    std::cout << "WOOD_FOOT_PUSH_TAKEOFF peak_vy_mps=" << peak_vy
              << " apex_rise_m=" << apex - before.player_position.y
              << " old_instantaneous_vy_mps=.197759 spam_requests=" << spam_requests << '\n';
    require(spam_requests > 1 && airborne && landed && peak_vy > .8 &&
                apex - before.player_position.y > .05 && !s.snapshot().foot_push_active &&
                s.snapshot().plank_broken_joint_mask == 0,
            "finite paid stroke materially improves takeoff and lands on intact wood", s);
    settle_wood_landing(s);
    phase = "ordinary_wood_to_steel_exit";
    walk(s, -4.9, false);
    tick(s, 180);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kSteel &&
                s.snapshot().checkpoint_footing_valid && !s.snapshot().foot_push_active,
            "real steel onward remains stable after wood launch", s);
    std::cout << "PASS WOOD_FOOT_PUSH_PLAYER mode=jump native85kg=1 finite_stroke=1 no_refresh=1 "
                 "actual_wood_landing=1 stable_steel_exit=1 gravity=1 deaths=0\n";
}
void cleanup_and_static_jump() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    approach(s);
    phase = "ordinary_drop_cancels_foot";
    begin_jump(s);
    require(s.request_release(), "ordinary Drop requests cancel", s);
    tick(s);
    require(!s.snapshot().foot_push_active && s.snapshot().foot_push_last_impulse_ns == 0,
            "ordinary Drop removes foot row before another impulse", s);
    const double cancelled_work = s.snapshot().foot_push_command_work_bound_j;
    tick(s, 12);
    require(!s.snapshot().foot_push_active && s.snapshot().foot_push_command_work_bound_j == cancelled_work,
            "cancelled push supplies no later command work", s);
    walk(s, kX, true);
    tick(s, 90);
    phase = "checkpoint_restart_clears_active_foot";
    const auto saved_checkpoint = s.snapshot().checkpoint_position;
    begin_jump(s);
    const auto active_checkpoint = s.snapshot().checkpoint_position;
    require(std::hypot(active_checkpoint.x - saved_checkpoint.x,
                       active_checkpoint.z - saved_checkpoint.z) < 1.0e-6 &&
                std::abs(active_checkpoint.y - saved_checkpoint.y) < 1.0e-6,
            "active leg does not replace actual saved firm checkpoint", s);
    require(s.restart_checkpoint(), "ordinary checkpoint restart accepted", s);
    const auto restarted = s.snapshot();
    require(!restarted.foot_push_active && restarted.foot_push_last_impulse_ns == 0 &&
                std::hypot(restarted.player_position.x - saved_checkpoint.x,
                           restarted.player_position.z - saved_checkpoint.z) < 1.0e-4 &&
                std::abs(restarted.player_position.y - saved_checkpoint.y) < 1.0e-4,
            "restart clears transient foot before restoring native bodies", s);
    tick(s, 90);
    const auto supported_restart = s.snapshot();
    const auto offset = supported_restart.player_position;
    require(!supported_restart.foot_push_active && supported_restart.foot_push_last_impulse_ns == 0 &&
                supported_restart.player_grounded &&
                (wood(supported_restart) || supported_restart.support_entity_id == kSteel) &&
                supported_restart.plank_broken_joint_mask == 0 &&
                std::sqrt((offset.x - saved_checkpoint.x) * (offset.x - saved_checkpoint.x) +
                          (offset.y - saved_checkpoint.y) * (offset.y - saved_checkpoint.y) +
                          (offset.z - saved_checkpoint.z) * (offset.z - saved_checkpoint.z)) < .15,
            "restart retains actual saved supported footing with no resumed leg", s);
    sample(s, "actual_saved_supported_footing");
    phase = "unchanged_static_steel_jump";
    // Firm saved end timber is a valid checkpoint. Reach steel by ordinary
    // walking before testing the separate unchanged static Jump contract.
    walk(s, -8.8, false);
    tick(s, 90);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kSteel &&
                s.snapshot().checkpoint_footing_valid,
            "ordinary backtrack reaches real firm steel before static Jump", s);
    const auto before = s.snapshot();
    require(s.request_jump(), "ordinary supported steel Jump", s);
    tick(s);
    const auto after = s.snapshot();
    const double relative_vy = after.player_linear_velocity.y - before.support_point_linear_velocity.y;
    require(!after.foot_push_active && after.foot_push_start_count == before.foot_push_start_count &&
                !after.player_grounded && relative_vy > 5.34 && relative_vy < 5.51 &&
                after.landing_jump_work_j > before.landing_jump_work_j,
            "ordinary steel retains causal5.5mps impulse and receives no wood leg", s);
    sample(s, "static_jump_unchanged");
    std::cout << "PASS WOOD_FOOT_PUSH_PLAYER mode=cleanup drop=1 checkpoint_restart=1 static_jump_unchanged=1\n";
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::fixed << std::setprecision(9);
    const std::string mode = argc > 1 ? argv[1] : "both";
    if (mode != "jump" && mode != "cleanup" && mode != "both") return EXIT_FAILURE;
    if (mode != "cleanup") jump_and_spam();
    if (mode != "jump") cleanup_and_static_jump();
    return EXIT_SUCCESS;
}
