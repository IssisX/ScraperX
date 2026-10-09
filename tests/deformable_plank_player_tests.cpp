#include "sim/simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using scraperx::sim::Quaternion;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
using scraperx::sim::Vector3;
constexpr std::uint64_t kFirstSegment = 2880;
constexpr std::uint64_t kSteel = 1935;
constexpr std::size_t kCount = 12;
constexpr double kWalkZ = -128.2;
constexpr double kCentreX = -6.8;
const char *phase = "steel_start";
std::array<std::uint32_t, kCount> body_indices{};
std::array<Vector3, kCount> resting_positions{};
std::array<bool, kCount> selected_supports{};
std::array<bool, kCount - 1> seams{};
bool record_walking_seams = false;
bool allow_physical_climb = false;

bool segment_support(const Snapshot &v) {
    return v.player_grounded && v.support_entity_id >= kFirstSegment &&
           v.support_entity_id < kFirstSegment + kCount;
}

double horizontal_speed(const Snapshot &v) {
    return std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z);
}

double centre_y(const Simulation &s) {
    return .5 * (s.kit_body_position(body_indices[5]).y + s.kit_body_position(body_indices[6]).y);
}

double baseline_centre_y() {
    return .5 * (resting_positions[5].y + resting_positions[6].y);
}

Vector3 material_endpoint(const Simulation &s, std::size_t segment, double local_x) {
    const auto p = s.kit_body_position(body_indices[segment]);
    const auto q = s.kit_body_rotation(body_indices[segment]);
    // Actual native orientation applied to the physical +/-0.1m end point.
    return {p.x + local_x * (1 - 2 * (q.y * q.y + q.z * q.z)),
            p.y + local_x * 2 * (q.x * q.y + q.z * q.w),
            p.z + local_x * 2 * (q.x * q.z - q.y * q.w)};
}

void sample(const Simulation &s, const char *event) {
    const auto v = s.snapshot();
    std::cout << "PLANK_PLAYER," << phase << ',' << event << ",tick=" << v.tick_index
              << ",x=" << v.player_position.x << ",y=" << v.player_position.y
              << ",z=" << v.player_position.z << ",vx=" << v.player_linear_velocity.x
              << ",vy=" << v.player_linear_velocity.y << ",vz=" << v.player_linear_velocity.z
              << ",grounded=" << v.player_grounded << ",support=" << v.support_entity_id
              << ",board_centre_y=" << centre_y(s) << ",gravity=" << v.player_gravity_factor
              << ",deaths=" << v.death_count << ",landings=" << v.landing_count
              << ",jump_work_j=" << v.landing_jump_work_j
              << ",foot_active=" << v.foot_push_active
              << ",foot_support=" << v.foot_push_support_entity_id
              << ",foot_command_j=" << v.foot_push_command_work_bound_j
              << ",foot_stroke_m=" << v.foot_push_stroke_m
              << ",foot_peak_n=" << v.foot_push_peak_load_n
              << ",peak_strength_ratio=" << v.plank_peak_strength_ratio
              << ",broken_mask=" << v.plank_broken_joint_mask << '\n';
    if (allow_physical_climb) std::cout << "PLANK_HANDS,grip=" << v.grip_available
        << ",grip_entity=" << v.grip_entity_id << ",constraints=" << v.traversal_hand_constraint_count
        << ",state=" << int(v.traversal_state) << ",left=" << v.traversal_left_hand.x << ','
        << v.traversal_left_hand.y << ',' << v.traversal_left_hand.z << ",right="
        << v.traversal_right_hand.x << ',' << v.traversal_right_hand.y << ',' << v.traversal_right_hand.z
        << ",force_y=" << v.traversal_left_hand_force.y + v.traversal_right_hand_force.y << '\n';
}

void poses(const Simulation &s, const char *event) {
    for (std::size_t i = 0; i < kCount; ++i) {
        if (body_indices[i] == Simulation::kKitNone) {
            std::cout << "PLANK_NATIVE_POSE," << phase << ',' << event
                      << ",entity=" << kFirstSegment + i << ",missing=1\n";
            continue;
        }
        const auto p = s.kit_body_position(body_indices[i]);
        const auto q = s.kit_body_rotation(body_indices[i]);
        const auto velocity = s.kit_body_velocity(body_indices[i]);
        std::cout << "PLANK_NATIVE_POSE," << phase << ',' << event << ",entity=" << kFirstSegment + i
                  << ",body_index=" << body_indices[i] << ",x=" << p.x << ",y=" << p.y
                  << ",z=" << p.z << ",qx=" << q.x << ",qy=" << q.y << ",qz=" << q.z
                  << ",qw=" << q.w << ",vx=" << velocity.x << ",vy=" << velocity.y
                  << ",vz=" << velocity.z << '\n';
    }
}

void require(bool condition, const char *message, const Simulation &s) {
    if (condition) return;
    sample(s, "failure");
    poses(s, "failure");
    std::cerr << "FAIL DEFORMABLE_PLANK_PLAYER phase=" << phase << " reason=" << message << '\n';
    std::exit(EXIT_FAILURE);
}

void begin(Simulation &s, const char *next) {
    phase = next;
    sample(s, "begin");
}

void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto before = s.snapshot();
        const auto result = s.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1, "one actual native tick accepted", s);
        const auto v = s.snapshot();
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                    std::isfinite(v.player_position.z) && std::isfinite(v.player_linear_velocity.x) &&
                    std::isfinite(v.player_linear_velocity.y) && std::isfinite(v.player_linear_velocity.z),
                "native player motion remains finite", s);
        require(v.player_gravity_factor == 1 && v.death_count == 0 && !v.parachute_deployed &&
                    (v.traversal_state == TraversalState::None ||
                     (allow_physical_climb && v.traversal_state == TraversalState::Climbing)) &&
                    (allow_physical_climb || v.traversal_hand_constraint_count == 0),
                "ordinary gravity and same life without assisted traversal", s);
        if (segment_support(v)) selected_supports[v.support_entity_id - kFirstSegment] = true;
        for (std::size_t i = 0; i < kCount; ++i) {
            const auto p = s.kit_body_position(body_indices[i]);
            const auto q = s.kit_body_rotation(body_indices[i]);
            const auto velocity = s.kit_body_velocity(body_indices[i]);
            require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
                        std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
                        std::isfinite(velocity.x) && std::isfinite(velocity.y) && std::isfinite(velocity.z),
                    "actual native plank segments remain finite", s);
        }
        if (record_walking_seams && segment_support(before) && segment_support(v)) {
            for (std::size_t i = 0; i + 1 < kCount; ++i) {
                const double seam_x = .5 * (material_endpoint(s, i, .1).x +
                                              material_endpoint(s, i + 1, -.1).x);
                if (before.player_position.x <= seam_x && v.player_position.x >= seam_x && !seams[i]) {
                    seams[i] = true;
                    std::cout << "PLANK_WALKED_SEAM,index=" << i << ",native_x=" << seam_x
                              << ",support=" << v.support_entity_id << ",tick=" << v.tick_index << '\n';
                }
            }
        }
        if (v.tick_index % 90 == 0) sample(s, "tick");
    }
}

void steer(Simulation &s, double x, double extra_x = 0, double extra_z = 0) {
    const auto v = s.snapshot();
    const double ix = 1.8 * (x - v.player_position.x) - .28 * v.player_linear_velocity.x + extra_x;
    const double iz = 1.8 * (kWalkZ - v.player_position.z) - .28 * v.player_linear_velocity.z + extra_z;
    const double scale = std::max(1.0, std::hypot(ix, iz));
    require(s.set_move_input(ix / scale, iz / scale), "ordinary stick steering accepted", s);
    if (std::abs(x - v.player_position.x) > .02)
        require(s.set_facing(x - v.player_position.x, kWalkZ - v.player_position.z),
                "ordinary facing accepted", s);
}

void walk(Simulation &s, double x, const char *next, bool expected_wood) {
    begin(s, next);
    record_walking_seams = true;
    double effort_x = 0, effort_z = 0;
    for (int i = 0; i < 1350; ++i) {
        const auto v = s.snapshot();
        const bool supported = expected_wood ? segment_support(v) :
            (v.player_grounded && v.support_entity_id == kSteel);
        const double dx = x - v.player_position.x, dz = kWalkZ - v.player_position.z;
        if (supported && std::abs(dx) < .04 && std::abs(dz) < .02 && horizontal_speed(v) < .1) {
            require(s.set_move_input(0, 0), "arrival releases ordinary stick", s);
            record_walking_seams = false;
            sample(s, "arrived");
            return;
        }
        if (std::hypot(dx, dz) > .75) effort_x = effort_z = 0;
        else {
            effort_x += .4 * dx * Simulation::kFixedStepSeconds;
            effort_z += .4 * dz * Simulation::kFixedStepSeconds;
            const double scale = std::max(1.0, std::hypot(effort_x, effort_z) / .35);
            effort_x /= scale;
            effort_z /= scale;
        }
        steer(s, x, effort_x, effort_z);
        tick(s);
    }
    require(false, "bounded ordinary walking reaches the real receiver", s);
}

void stable_steel(Simulation &s, int ticks) {
    require(s.set_move_input(0, 0), "neutral steel input accepted", s);
    for (int i = 0; i < ticks; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.player_grounded && v.support_entity_id == kSteel &&
                    std::abs(v.player_position.y - 407.9) < .1,
                "actual407 steel remains supported", s);
    }
    require(s.snapshot().checkpoint_footing_valid, "steel exit has honest firm native footing", s);
}

void settle_timber_landing(Simulation &s) {
    begin(s, "real_timber_landing_settle");
    require(s.set_move_input(0, 0), "neutral input lets actual timber landing settle", s);
    int quiet = 0;
    for (int i = 0; i < 360; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(v.plank_broken_joint_mask == 0, "wood remains intact during physical landing settle", s);
        const double relative_x = v.player_linear_velocity.x - v.support_point_linear_velocity.x;
        const double relative_y = v.player_linear_velocity.y - v.support_point_linear_velocity.y;
        const double relative_z = v.player_linear_velocity.z - v.support_point_linear_velocity.z;
        if (segment_support(v) && std::abs(relative_y) < .1 && std::hypot(relative_x, relative_z) < .1)
            ++quiet;
        else quiet = 0;
        if (quiet == 30) {
            sample(s, "actual_settled_timber_landing");
            return;
        }
    }
    require(false, "actual intact timber landing settles30ticks before onward walking", s);
}

void run() {
    // This is the actual normal-world encounter, with native85kg player,
    // gravity and contact. It is not an isolated spring/module benchmark.
    body_indices.fill(Simulation::kKitNone);
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    for (std::size_t i = 0; i < kCount; ++i) {
        body_indices[i] = s.kit_body_index(kFirstSegment + i);
        require(body_indices[i] != Simulation::kKitNone && s.entity_body_count(kFirstSegment + i) == 1 &&
                    s.kit_body_dynamic(body_indices[i]) && s.kit_body_enabled(body_indices[i]) &&
                    std::abs(s.kit_body_mass(body_indices[i]) - .722) < 1.0e-5,
                "normal world contains each actual .722kg timber segment", s);
    }
    // Sole staging operation, on the authored left steel approach. No later
    // relocation, direct force, velocity override or support grant is used.
    require(s.debug_restart_at({-8.8, 407.9, kWalkZ}), "actual steel starting point accepted", s);
    require(s.set_move_input(0, 0) && s.set_crouch_input(false) && s.set_sprint_input(false),
            "ordinary standing input accepted", s);
    tick(s, 270);
    stable_steel(s, 90);
    for (std::size_t i = 0; i < kCount; ++i) resting_positions[i] = s.kit_body_position(body_indices[i]);
    poses(s, "gravity_only_baseline");
    walk(s, kCentreX, "walk_left_seams_to_centre", true);
    begin(s, "real85kg_standing_load");
    tick(s, 270);
    for (int i = 0; i < 90; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(segment_support(v) && std::abs(v.player_position.x - kCentreX) < .12 &&
                    std::abs(v.player_position.z - kWalkZ) < .05 && horizontal_speed(v) < .15,
                "real player rests near centre on segment contact rather than steel", s);
    }
    const double loaded_sag = baseline_centre_y() - centre_y(s);
    std::cout << "PLANK_PLAYER_LOAD,extra_sag_m=" << loaded_sag
              << ",native_player_mass_kg=85,point_load_reference_m=0.03114\n";
    require(loaded_sag >= .02 && loaded_sag <= .045,
            "real player weight produces20to45mm additional sag without hidden steel or double weight", s);
    poses(s, "real_standing_load");
    begin(s, "ordinary_centre_jump");
    const auto before_jump = s.snapshot();
    const double before_board = centre_y(s);
    require(segment_support(before_jump) && s.set_facing(1, 0) && s.set_move_input(0, 0) && s.request_jump(),
            "ordinary Jump starts on actual loaded timber", s);
    bool airborne = false, landed = false, finite_push_observed = false;
    double board_motion = 0;
    for (int i = 0; i < 540; ++i) {
        steer(s, kCentreX);
        tick(s);
        const auto v = s.snapshot();
        if (v.foot_push_active) {
            finite_push_observed = true;
            require(v.foot_push_support_entity_id >= kFirstSegment &&
                        v.foot_push_support_entity_id < kFirstSegment + kCount &&
                        v.foot_push_stroke_m <= .300001 && v.foot_push_peak_load_n <= 3500.01 &&
                        v.foot_push_elapsed_seconds <= .25 + Simulation::kFixedStepSeconds &&
                        v.foot_push_command_work_bound_j <= 1285.62501,
                    "ordinary wood Jump uses a bounded real material foot push", s);
        }
        board_motion = std::max(board_motion, std::abs(centre_y(s) - before_board));
        if (!airborne && !v.player_grounded && v.support_entity_id == 0) {
            airborne = true;
            sample(s, "actual_free_flight");
            poses(s, "jump_board_motion");
        }
        if (airborne && segment_support(v) && v.landing_count > before_jump.landing_count) {
            landed = true;
            break;
        }
    }
    require(airborne && landed && finite_push_observed &&
                s.snapshot().foot_push_start_count == before_jump.foot_push_start_count + 1 &&
                s.snapshot().foot_push_command_work_bound_j > 0 && board_motion > .002,
            "finite native Jump changes plank motion and lands back on actual timber", s);
    std::cout << "PLANK_PLAYER_JUMP,board_motion_m=" << board_motion
              << ",foot_command_work_bound_j=" << s.snapshot().foot_push_command_work_bound_j
              << ",instant_jump_work_delta_j=" << s.snapshot().landing_jump_work_j - before_jump.landing_jump_work_j << '\n';
    settle_timber_landing(s);
    walk(s, kCentreX, "actual_timber_landing_braking", true);
    poses(s, "actual_timber_landing");
    walk(s, -4.9, "walk_remaining_seams_to_right_steel", false);
    begin(s, "plank_unloads_on_steel_exit");
    stable_steel(s, 360);
    double unloaded_error = 0;
    for (std::size_t i = 0; i < kCount; ++i)
        unloaded_error = std::max(unloaded_error,
            std::abs(s.kit_body_position(body_indices[i]).y - resting_positions[i].y));
    std::cout << "PLANK_PLAYER_UNLOAD,max_segment_y_error_m=" << unloaded_error << '\n';
    require(unloaded_error < .002 && unloaded_error < loaded_sag * .15,
            "unloaded segment shape returns near its own-weight baseline", s);
    poses(s, "unloaded_baseline_return");
    for (const bool crossed : seams)
        require(crossed, "ordinary grounded segment walking crossed every actual internal seam", s);
    std::cout << "PLANK_SELECTED_SUPPORTS";
    for (std::size_t i = 0; i < kCount; ++i)
        if (selected_supports[i]) std::cout << ',' << kFirstSegment + i;
    std::cout << '\n';
    // Capsule manifolds can select neighbouring segment IDs; all twelve
    // selected IDs are not required. The eleven walked seams are required.
    walk(s, -4.5, "onward_right_steel_walking", false);
    stable_steel(s, 90);
    std::cout << "PASS DEFORMABLE_PLANK_PLAYER normal_world=1 seams=11 native85kg_load=1 "
                 "ordinary_jump_landing=1 unload_return=1 onward_steel=1 initial_steel_staging_only=1 "
                 "gravity=1 deaths=0 grip=unverified\n";
}

void run_grip() {
    body_indices.fill(Simulation::kKitNone);
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    for (std::size_t i = 0; i < kCount; ++i) body_indices[i] = s.kit_body_index(kFirstSegment + i);
    require(s.debug_restart_at({-8.8, 407.9, kWalkZ}), "sole steel staging accepted", s);
    tick(s, 360);
    for (std::size_t i = 0; i < kCount; ++i) resting_positions[i] = s.kit_body_position(body_indices[i]);
    walk(s, kCentreX, "grip_approach_on_actual_wood", true);
    allow_physical_climb = true;
    begin(s, "ordinary_side_walkoff_catch");
    require(s.set_facing(0, -1), "ordinary inward facing", s);
    bool airborne = false, caught = false;
    for (int i = 0; i < 180; ++i) {
        const auto v = s.snapshot();
        require(s.set_move_input(0, airborne ? 0 : .20), "ordinary sideways walking then neutral", s);
        if (airborne && v.grip_available && v.grip_entity_id >= kFirstSegment &&
            v.grip_entity_id < kFirstSegment + kCount)
            require(s.request_traversal(), "ordinary Action requests reachable plank catch", s);
        tick(s);
        const auto now = s.snapshot();
        if (!now.player_grounded) airborne = true;
        if (now.traversal_state == TraversalState::Climbing && now.traversal_hand_constraint_count == 2) {
            caught = true;
            sample(s, "actual_two_hand_catch");
            break;
        }
    }
    require(airborne && caught, "ordinary walkoff and Action catch actual timber", s);
    require(s.set_move_input(0, 0), "neutral hanging supplies no climb stroke", s);
    begin(s, "actual_hanging_player_load");
    double maximum_hand_force = 0;
    for (int i = 0; i < 270; ++i) {
        tick(s);
        const auto v = s.snapshot();
        require(!v.player_grounded && v.traversal_hand_constraint_count == 2 &&
            v.traversal_state == TraversalState::Climbing, "two real hands carry airborne player", s);
        const auto magnitude = [](Vector3 f) { return std::sqrt(f.x*f.x+f.y*f.y+f.z*f.z); };
        maximum_hand_force = std::max({maximum_hand_force, magnitude(v.traversal_left_hand_force),
                                      magnitude(v.traversal_right_hand_force)});
        require(maximum_hand_force <= 1501, "actual per-hand force remains bounded", s);
    }
    const double sag = baseline_centre_y() - centre_y(s);
    std::cout << "PLANK_HANG_LOAD,extra_sag_m=" << sag << ",max_hand_force_n=" << maximum_hand_force << '\n';
    require(sag > .02 && sag < .045, "actual hand-transferred load bends plank without duplicate weight", s);
    poses(s, "actual_hand_load");
    const auto before = s.snapshot();
    require(s.request_release(), "ordinary deliberate Drop requests release", s);
    tick(s);
    const auto after = s.snapshot();
    require(after.traversal_hand_constraint_count == 0 && after.traversal_state == TraversalState::None &&
        std::hypot(after.player_linear_velocity.x-before.player_linear_velocity.x,
                   after.player_linear_velocity.z-before.player_linear_velocity.z) < .02 &&
        std::abs(after.player_linear_velocity.y-before.player_linear_velocity.y + 9.81/90) < .05,
        "passive release preserves momentum apart from native gravity tick", s);
    begin(s, "ordinary_release_to_real_recovery_tray");
    bool recovered = false;
    for (int i = 0; i < 360; ++i) {
        tick(s);
        const auto v = s.snapshot();
        if (v.player_grounded && v.support_entity_id == 1936 && std::abs(v.player_position.y-404.4)<.15) {
            recovered = true;
            break;
        }
    }
    require(recovered, "released climber lands on actual recovery tray", s);
    tick(s, 360);
    require(std::abs(baseline_centre_y()-centre_y(s)) < .002,
        "released grip load restores own-weight shape", s);
    std::cout << "PASS DEFORMABLE_PLANK_GRIP actual_two_hands=1 actual_segment_edges=1 finite_force=1 "
        "native85kg_load=1 passive_release=1 actual_recovery_tray=1 unload_return=1\n";
}

void run_impact() {
    body_indices.fill(Simulation::kKitNone);
    allow_physical_climb = false;
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::Slingshot);
    for (std::size_t i = 0; i < kCount; ++i) body_indices[i] = s.kit_body_index(kFirstSegment + i);
    // One explicit airborne staging for this impact falsifier. Ordinary
    // native gravity supplies the load; no force/velocity or authored break.
    require(s.debug_restart_at({kCentreX, 410.9, kWalkZ}), "bounded3m fall staging accepted", s);
    require(s.set_move_input(0, 0), "neutral falling input accepted", s);
    begin(s, "native_player_fall_overload");
    bool broken = false, recovered = false, clear_steel = false;
    for (int i = 0; i < 720; ++i) {
        const auto v = s.snapshot();
        if (broken) {
            const double iz = 1.8 * (-127.2 - v.player_position.z) - .28 * v.player_linear_velocity.z;
            if (!clear_steel) {
                // First escape sideways from actual fragments, preserving
                // the observed ordinary Z-only input and its full .4 effort.
                require(s.set_move_input(0, std::clamp(iz, -.4, .4)),
                    "ordinary side steering toward clear recovery tray", s);
            } else {
                // Only after actual steel contact away from the wood, brake
                // fragment-imparted X momentum and walk into the tray patch.
                // A grounded edge perch is not yet honest resting footing.
                const double ix = 1.8 * (kCentreX - v.player_position.x) - .28 * v.player_linear_velocity.x;
                const double scale = std::max(1.0, std::hypot(ix, iz) / .4);
                require(s.set_move_input(ix / scale, iz / scale),
                    "bounded ordinary two-axis braking on actual recovery steel", s);
            }
        }
        tick(s);
        const auto now = s.snapshot();
        require(!now.foot_push_active && now.foot_push_start_count == 0 &&
                    now.foot_push_last_impulse_ns == 0,
                "gravity-loaded fracture and fragment recovery do not create a phantom foot push", s);
        if (!broken && now.plank_broken_joint_mask != 0) {
            broken = true;
            require(now.plank_fracture_count > 0 && now.plank_peak_strength_ratio >= 1,
                "actual player impact exceeds section strength and breaks real rows", s);
            sample(s, "actual_player_rupture");
            poses(s, "actual_player_rupture");
        }
        if (!clear_steel && broken && now.player_grounded && now.support_entity_id == 1936 &&
            std::abs(now.player_position.z + 127.2) < .1) {
            clear_steel = true;
            sample(s, "actual_clear_steel_before_braking");
        }
        if (broken && clear_steel && now.player_grounded && now.support_entity_id == 1936 &&
            now.checkpoint_footing_valid && horizontal_speed(now) < .1 &&
            std::abs(now.player_position.x - kCentreX) < .1 &&
            std::abs(now.player_position.y-404.4) < .15 && std::abs(now.player_position.z+127.2)<.1) {
            recovered = true;
            sample(s, "actual_firm_braked_tray");
            std::cout << "PLANK_IMPACT_RECOVERY,firm=" << now.checkpoint_footing_valid
                      << ",horizontal_speed_mps=" << horizontal_speed(now)
                      << ",target_x=" << kCentreX << ",target_z=-127.2\n";
            break;
        }
    }
    require(broken && recovered, "actual fracture permits fall and input-directed recovery on real tray", s);
    require(s.set_move_input(0, 0), "recovered player releases stick", s);
    tick(s, 180);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == 1936 &&
        s.snapshot().checkpoint_footing_valid && s.snapshot().plank_broken_joint_mask != 0,
        "real steel recovery is checkpointable and damage persists", s);
    sample(s, "stable_recovered_with_damage");
    poses(s, "native_fragments_after_player_impact");
    std::cout << "PASS DEFORMABLE_PLANK_PLAYER_FRACTURE actual_gravity_loaded85kg=1 actual_rupture=1 "
        "ordinary_air_steering=1 actual_recovery_tray=1 persistent_damage=1 no_assisted_elevation=1\n";
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::fixed << std::setprecision(9);
    if (argc > 1 && std::string(argv[1]) == "grip") run_grip();
    else if (argc > 1 && std::string(argv[1]) == "impact") run_impact();
    else { run(); run_grip(); run_impact(); }
    return EXIT_SUCCESS;
}
