#include "sim/simulation.hpp"
#include "passive_receiver_plant_observation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
constexpr double kX = -5.85, kZ = -128.2;
constexpr std::uint64_t kSteel = 1935;
const char *phase = "initialization";
const char *mode = "entry";

bool wood(const Snapshot &v) {
    return v.player_grounded && v.support_entity_id >= 2880 && v.support_entity_id <= 2891;
}
double speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}
void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    std::cout << "ENTRY_OVERLAP mode=" << mode << " phase=" << phase << " event=" << event << " tick=" << v.tick_index
              << " position=" << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z
              << " velocity=" << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ',' << v.player_linear_velocity.z
              << " grounded=" << v.player_grounded << " support=" << v.support_entity_id
              << " active=" << v.foot_push_active << " foot_support=" << v.foot_push_support_entity_id
              << " starts=" << v.foot_push_start_count << " stop=" << int(v.foot_push_stop_reason)
              << " command_j=" << v.foot_push_command_work_bound_j << " stroke_m=" << v.foot_push_stroke_m
              << " stroke_limit_m=" << v.foot_push_stroke_limit_m
              << " force_n=" << v.foot_push_player_force.x << ',' << v.foot_push_player_force.y << ',' << v.foot_push_player_force.z
              << " time_s=" << v.foot_push_elapsed_seconds << " peak_n=" << v.foot_push_peak_load_n
              << " impulse_ns=" << v.foot_push_last_impulse_ns << " distance_m=" << v.foot_push_actual_distance_m
              << " initial_distance_m=" << v.foot_push_initial_distance_m
              << " gravity=" << v.player_gravity_factor << " deaths=" << v.death_count
              << " broken=" << v.plank_broken_joint_mask << " peak_strength=" << v.plank_peak_strength_ratio
              << " traversal=" << int(v.traversal_state) << " hands=" << v.traversal_hand_constraint_count
              << " shared=" << v.hand_foot_overlap_solved_ticks
              << " overlap=" << v.hand_foot_overlap_active
              << " shared_tick=" << v.hand_foot_last_solve_tick
              << " hand_n=" << v.hand_foot_last_hand_load_n << " foot_n=" << v.hand_foot_last_foot_load_n
              << " radial=" << v.hanging_free_arc_active << " kind=" << int(v.traversal_plan_kind)
              << " rejection=" << int(v.traversal_rejection_reason)
              << " left=" << v.traversal_left_hand.x << "," << v.traversal_left_hand.y << "," << v.traversal_left_hand.z
              << " right=" << v.traversal_right_hand.x << "," << v.traversal_right_hand.y << "," << v.traversal_right_hand.z
              << " support_velocity=" << v.support_point_linear_velocity.x << ','
              << v.support_point_linear_velocity.y << ',' << v.support_point_linear_velocity.z << '\n';
}
void require(bool condition, const char *message, const Simulation &s) {
    if (condition) return;
    sample(s, "failure");
    std::cerr << "FAIL ENTRY_OVERLAP phase=" << phase << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}
void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto previous = s.snapshot();
        const auto result = s.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1, "one ordinary native tick", s);
        const auto v = s.snapshot();
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                    std::isfinite(v.player_position.z) && std::isfinite(v.player_linear_velocity.y) &&
                    v.player_gravity_factor == 1 && v.death_count == 0 && !v.parachute_deployed,
                "active gravity and finite player state", s);
        require(v.foot_push_command_work_bound_j <= 1285.62501 && v.foot_push_stroke_m <= .300001 &&
                    v.foot_push_peak_load_n <= 3500.01 &&
                    v.foot_push_elapsed_seconds <= .25 + Simulation::kFixedStepSeconds &&
                    v.foot_push_last_impulse_ns >= 0 &&
                    v.foot_push_last_impulse_ns <= 3500.0 / 360.0 + .0001,
                "actual last collision row and command stay within finite physical limits", s);
        if (v.foot_push_active)
            require((v.foot_push_support_entity_id >= 2880 && v.foot_push_support_entity_id <= 2891) ||
                        scraperx::test::passive_receiver_plant(v, previous, kSteel),
                    "active foot is powered material or a finite zero-command plant from actual receiver contact", s);
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
void approach(Simulation &s, double target_x = kX) {
    // Actual normal Slingshot world; sole initial stage is real left steel.
    // Subsequent motion uses ordinary input only, never body/velocity/force writes.
    phase = "actual_steel_to_loaded_timber";
    require(s.debug_restart_at({-8.8, 407.9, kZ}), "sole supported steel entry", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_facing(1, 0), "ordinary standing input", s);
    tick(s, 360);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kSteel &&
                s.snapshot().checkpoint_footing_valid, "real firm steel entry", s);
    walk(s, target_x, true);
    tick(s, 360);
    require(wood(s.snapshot()) && speed(s.snapshot()) < .15 && s.snapshot().plank_broken_joint_mask == 0,
            "actual85kg load rests at receiving end of intact timber", s);
    sample(s, "loaded_centre");
}
void begin_jump(Simulation &s) {
    require(wood(s.snapshot()) && !s.snapshot().foot_push_active && s.request_jump(), "ordinary supported wood Jump", s);
    tick(s);
    require(s.snapshot().foot_push_active && s.snapshot().foot_push_command_work_bound_j > 0,
            "ordinary Jump starts actual finite leg, not instantaneous velocity assignment", s);
    sample(s, "first_finite_command");
}


Snapshot catch_shared(Simulation &s) {
    approach(s);
    phase = "ordinary_jump_cartesian_catch";
    const bool natural = std::string(mode) == "natural";
    require(s.set_facing(natural ? 1 : -1, natural ? -1 : 0), "ordinary facing before Jump", s);
    begin_jump(s);
    const auto begun = s.snapshot();
    if (natural) {
        require(s.set_move_input(0, 0), "neutral input while already looking at receiver rail", s);
        tick(s, 8);
        sample(s, "natural_jump_before_action");
    }
    require(s.set_facing(1, -1) && s.request_traversal(), "ordinary Action toward receiver rail", s);
    tick(s);
    sample(s, "catch");
    const auto caught = s.snapshot();
    require(caught.traversal_hand_constraint_count == 2 && caught.foot_push_active &&
                caught.hand_foot_overlap_active && !caught.hanging_free_arc_active &&
                caught.hand_foot_overlap_solved_ticks > begun.hand_foot_overlap_solved_ticks &&
                caught.hand_foot_last_solve_tick == caught.tick_index &&
                caught.hand_foot_last_hand_load_n > 0 && caught.hand_foot_last_foot_load_n > 0,
            "real two-hand Cartesian catch shares positive native loads with existing leg", s);
    require(caught.foot_push_start_count == begun.foot_push_start_count &&
                caught.foot_push_support_entity_id == begun.foot_push_support_entity_id &&
                caught.foot_push_initial_distance_m == begun.foot_push_initial_distance_m &&
                caught.foot_push_stroke_limit_m == begun.foot_push_stroke_limit_m,
            "catch retains original foot row, support, initial distance and stroke budget", s);
    return begun;
}
void entry() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    const auto begun = catch_shared(s);
    require(s.set_move_input(0, 0), "neutral compatible catch", s);
    auto previous = s.snapshot();
    int shared_positive_ticks = 1;
    for (int i = 0; i < 45 && s.snapshot().foot_push_active; ++i) {
        tick(s);
        const auto now = s.snapshot();
        require(now.foot_push_start_count == begun.foot_push_start_count &&
                    now.foot_push_support_entity_id == begun.foot_push_support_entity_id &&
                    now.foot_push_initial_distance_m == begun.foot_push_initial_distance_m &&
                    now.foot_push_elapsed_seconds >= previous.foot_push_elapsed_seconds &&
                    now.foot_push_command_work_bound_j >= previous.foot_push_command_work_bound_j &&
                    now.foot_push_stroke_m >= previous.foot_push_stroke_m &&
                    now.foot_push_stroke_m <= begun.foot_push_stroke_limit_m + .000001 &&
                    now.traversal_plan_kind == 0,
                "original finite leg progresses without restart, budget refresh or topout plan credit", s);
        if (now.foot_push_active) {
            require(!now.hanging_free_arc_active && now.traversal_hand_constraint_count == 2 &&
                        now.hand_foot_overlap_active &&
                        now.hand_foot_overlap_solved_ticks > previous.hand_foot_overlap_solved_ticks &&
                        now.hand_foot_last_solve_tick == now.tick_index &&
                        now.hand_foot_last_hand_load_n > 0 && now.hand_foot_last_foot_load_n > 0,
                    "each active overlap tick solves Cartesian hands and foot with positive native loads", s);
            ++shared_positive_ticks;
        }
        sample(s, "finite_shared_progress");
        previous = now;
    }
    require(shared_positive_ticks >= 8 && !s.snapshot().foot_push_active &&
                !s.snapshot().hand_foot_overlap_active && s.snapshot().foot_push_stop_reason != 0 &&
                s.snapshot().foot_push_elapsed_seconds <= .25 + Simulation::kFixedStepSeconds &&
                s.snapshot().foot_push_command_work_bound_j <= 3500 * begun.foot_push_stroke_limit_m + .001,
            "sustained shared solve ends at original finite leg limits", s);
    phase = "ordinary_receiver_exit";
    require(s.request_release(), "ordinary release after finite shared support", s);
    tick(s);
    walk(s, -5.15, false);
    for (int i = 0; i < 90; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.player_grounded && v.support_entity_id == kSteel && v.checkpoint_footing_valid &&
                    !v.foot_push_active && v.traversal_hand_constraint_count == 0,
                "ninety ordinary ticks retain real firm receiver steel after release and walk", s);
    }
    sample(s, "firm_receiver_exit");
}
void release_shared() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    const auto begun = catch_shared(s);
    phase = "deliberate_shared_release";
    require(s.request_release(), "ordinary Drop during shared support", s);
    tick(s);
    require(!s.snapshot().foot_push_active && !s.snapshot().hand_foot_overlap_active &&
                s.snapshot().traversal_hand_constraint_count == 0 && s.snapshot().foot_push_last_impulse_ns == 0,
            "deliberate release clears both attachments and finite foot command", s);
    const auto released = s.snapshot();
    tick(s, 30);
    require(s.snapshot().foot_push_start_count == begun.foot_push_start_count &&
                !s.snapshot().foot_push_active && !s.snapshot().hand_foot_overlap_active &&
                s.snapshot().foot_push_command_work_bound_j == released.foot_push_command_work_bound_j &&
                s.snapshot().hand_foot_overlap_solved_ticks == released.hand_foot_overlap_solved_ticks,
            "release cannot restart old foot or issue later shared solves", s);
    sample(s, "released_without_restart");
}
void expired_foot() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    approach(s);
    phase = "naturally_expired_foot_catch";
    require(s.set_facing(-1, 0), "ordinary Jump away from receiver", s);
    begin_jump(s);
    const auto begun = s.snapshot();
    bool airborne = false, landed = false;
    for (int i = 0; i < 360; ++i) {
        tick(s);
        const auto v = s.snapshot();
        airborne = airborne || !v.player_grounded;
        if (airborne && wood(v) && !v.foot_push_active) { landed = true; break; }
    }
    require(landed && !s.snapshot().foot_push_active, "ordinary finite Jump expires and lands naturally", s);
    const auto expired = s.snapshot();
    require(s.set_facing(1, -1) && s.request_traversal(), "ordinary Action at same real receiver after expiry", s);
    tick(s);
    sample(s, "expired_catch");
    require(s.snapshot().traversal_hand_constraint_count > 0, "expired-foot case actually catches a real handhold", s);
    for (int i = 0; i < 15; ++i) {
        require(!s.snapshot().foot_push_active && !s.snapshot().hand_foot_overlap_active &&
                    s.snapshot().foot_push_start_count == begun.foot_push_start_count &&
                    s.snapshot().hand_foot_overlap_solved_ticks == expired.hand_foot_overlap_solved_ticks,
                "real late catch cannot revive expired foot or fabricate shared solves", s);
        tick(s);
    }
}
void radial() {
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    const auto begun = catch_shared(s);
    phase = "crouch_changes_airborne_shared_cartesian_to_radial";
    require(s.set_move_input(0, 0), "neutral shared catch retains compatible Cartesian rows", s);
    for (int i = 0; i < 45 && s.snapshot().player_grounded && s.snapshot().foot_push_active; ++i)
        tick(s);
    const auto before = s.snapshot();
    const double grip_height = .5 * (before.traversal_left_hand.y + before.traversal_right_hand.y);
    require(!before.player_grounded && before.foot_push_active && before.hand_foot_overlap_active &&
                before.traversal_hand_constraint_count == 2 && !before.hanging_free_arc_active &&
                grip_height > before.player_position.y + .35,
            "actual airborne shared support reaches physical radial entry height before crouch", s);
    sample(s, "before_radial_transition");
    require(s.set_crouch_input(true), "ordinary hanging crouch requests incompatible radial posture", s);
    tick(s);
    const auto radial = s.snapshot();
    sample(s, "radial_transition");
    require(radial.traversal_hand_constraint_count == 2 && radial.hanging_free_arc_active,
            "ordinary crouch retains real two-hand grip and enters radial hanging mode", s);
    require(!radial.foot_push_active && !radial.hand_foot_overlap_active &&
                radial.foot_push_start_count == before.foot_push_start_count &&
                radial.foot_push_command_work_bound_j == before.foot_push_command_work_bound_j &&
                radial.foot_push_stroke_m == before.foot_push_stroke_m &&
                radial.foot_push_elapsed_seconds == before.foot_push_elapsed_seconds &&
                radial.hand_foot_overlap_solved_ticks == before.hand_foot_overlap_solved_ticks &&
                radial.foot_push_last_impulse_ns == 0,
            "incompatible radial transition cancels original leg before another shared solve", s);
    for (int i = 0; i < 15; ++i) {
        tick(s);
        const auto held = s.snapshot();
        require(!held.foot_push_active && !held.hand_foot_overlap_active &&
                    held.foot_push_start_count == begun.foot_push_start_count &&
                    held.hand_foot_overlap_solved_ticks == before.hand_foot_overlap_solved_ticks &&
                    held.foot_push_last_impulse_ns == 0,
                "ordinary radial hold cannot restart old leg or solve incompatible shared support", s);
    }
    sample(s, "radial_held_without_restart");
}
}
int main(int argc, char **argv) {
    if (argc != 2) { std::cerr << "usage: physical_entry_overlap_tests entry|natural|release|expired|radial\n"; return EXIT_FAILURE; }
    mode = argv[1];
    const std::string selected(mode);
    if (selected == "entry" || selected == "natural") entry();
    else if (selected == "release") release_shared();
    else if (selected == "expired") expired_foot();
    else if (selected == "radial") radial();
    else { std::cerr << "unknown entry-overlap mode\n"; return EXIT_FAILURE; }
    std::cout << "PASS PHYSICAL_ENTRY_OVERLAP mode=" << mode << " public_input=1 actual_jolt=1 gravity=1 deaths=0\n";
}
