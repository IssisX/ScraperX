#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool nearly_equal(const double a, const double b, const double epsilon = 1.0e-12) {
    return std::abs(a - b) <= epsilon;
}

double horizontal_distance(const scraperx::sim::Vector3 &a,
                           const scraperx::sim::Vector3 &b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}

double distance_3d(const scraperx::sim::Vector3 &a,
                   const scraperx::sim::Vector3 &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) +
                     (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
}

double horizontal_magnitude(const scraperx::sim::Vector3 &value) {
    return std::hypot(value.x, value.z);
}

double horizontal_dot(const scraperx::sim::Vector3 &a,
                      const scraperx::sim::Vector3 &b) {
    return a.x * b.x + a.z * b.z;
}

} // namespace

int main() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalMode;

    Simulation partitioned;
    for (std::uint32_t i = 0; i < Simulation::kTickRateHz; ++i) {
        const auto result = partitioned.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted, "fixed-step input must be accepted");
        require(result.steps_advanced == 1, "each exact fixed step must advance once");
    }

    const auto partitioned_snapshot = partitioned.snapshot();
    require(partitioned_snapshot.tick_index == 90, "90 fixed steps must produce tick 90");
    require(nearly_equal(partitioned_snapshot.simulation_time_seconds, 1.0),
            "tick-derived simulation time must equal one second");
    require(partitioned_snapshot.player_position.z > 50.0,
            "default native spawn must remain at exterior approach grade");

    Simulation batched;
    const auto batched_result = batched.advance_frame(1.0);
    const auto batched_snapshot = batched.snapshot();
    require(batched_result.accepted, "one-second frame input must be accepted");
    require(batched_result.steps_advanced == 90, "one second must advance 90 authoritative ticks");
    require(batched_snapshot.tick_index == partitioned_snapshot.tick_index,
            "frame partitioning must not change tick count");
    require(nearly_equal(batched_snapshot.simulation_time_seconds,
                         partitioned_snapshot.simulation_time_seconds),
            "frame partitioning must not change simulation time");
    require(nearly_equal(batched_snapshot.player_position.x,
                         partitioned_snapshot.player_position.x,
                         1.0e-6) &&
                nearly_equal(batched_snapshot.player_position.y,
                             partitioned_snapshot.player_position.y,
                             1.0e-6) &&
                nearly_equal(batched_snapshot.player_position.z,
                             partitioned_snapshot.player_position.z,
                             1.0e-6),
            "frame partitioning must not change native player position");

    Simulation remainder(InitialSpawn::StaticDeck);
    require(remainder.advance_frame(Simulation::kFixedStepSeconds * 0.5).steps_advanced == 0,
            "half a fixed step must remain buffered");
    require(remainder.advance_frame(Simulation::kFixedStepSeconds * 0.5).steps_advanced == 1,
            "two half steps must advance exactly once");
    require(nearly_equal(remainder.snapshot().interpolation_alpha, 0.0),
            "exactly consumed time must leave no interpolation remainder");

    const auto before_invalid = remainder.snapshot();
    require(!remainder.advance_frame(-0.001).accepted, "negative delta must be rejected");
    require(!remainder.advance_frame(std::numeric_limits<double>::quiet_NaN()).accepted,
            "NaN delta must be rejected");
    require(remainder.snapshot().tick_index == before_invalid.tick_index,
            "rejected frame input must not mutate authoritative state");

    Simulation supported(InitialSpawn::StaticDeck);
    require(supported.advance_frame(2.0).accepted,
            "static-deck settling interval must be accepted");
    const auto supported_snapshot = supported.snapshot();
    require(supported_snapshot.player_grounded,
            "the native player capsule must be grounded after falling onto exterior grade");
    require(supported_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static support must expose the stable grade entity ID");
    require(nearly_equal(supported_snapshot.player_position.y, 0.9, 0.03),
            "the supported capsule center must settle at the grade contact height");
    require(horizontal_magnitude(supported_snapshot.support_point_linear_velocity) < 1.0e-5,
            "the static grade support-point velocity must be zero");

    require(supported.set_move_input(1.0, 0.0),
            "finite desired movement input must be accepted");
    require(supported.advance_frame(0.5).accepted,
            "static-grade locomotion interval must be accepted");
    const auto moved_snapshot = supported.snapshot();
    require(moved_snapshot.player_position.x > supported_snapshot.player_position.x + 1.0,
            "desired relative velocity input must move the native body across grade");
    require(moved_snapshot.player_grounded,
            "horizontal locomotion must preserve grade support");
    require(moved_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "grade locomotion support identity must remain native and stable");

    require(!supported.set_move_input(std::numeric_limits<double>::quiet_NaN(), 0.0),
            "non-finite desired movement input must be rejected");
    const auto before_rejected_command = supported.snapshot();
    require(supported.advance_frame(0.25).accepted,
            "simulation must remain usable after rejecting a movement command");
    const auto after_rejected_command = supported.snapshot();
    require(horizontal_distance(after_rejected_command.player_position,
                                before_rejected_command.player_position) > 0.5,
            "a rejected movement command must not replace the last accepted command");

    Simulation translating(InitialSpawn::TranslatingSupport);
    require(translating.set_move_input(0.0, 0.0),
            "zero relative movement must be accepted on a translating support");
    require(translating.advance_frame(1.0).accepted,
            "translating-support settling interval must be accepted");
    const auto translating_grounded = translating.snapshot();
    require(translating_grounded.player_grounded,
            "the player must settle on the native translating support");
    require(translating_grounded.support_entity_id == Simulation::kTranslatingSupportEntityId,
            "translating support must expose its stable native entity ID");
    require(std::abs(translating_grounded.support_point_linear_velocity.x) > 0.5,
            "translating support must expose non-zero contact-point velocity");
    require(std::abs(translating_grounded.player_linear_velocity.x -
                     translating_grounded.support_point_linear_velocity.x) < 0.75,
            "zero-input grounded locomotion must remain relative to translating support velocity");

    const auto translating_support_velocity = translating_grounded.support_point_linear_velocity;
    require(translating.request_jump(), "grounded jump request must be accepted");
    require(translating.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "jump tick must advance");
    const auto translating_jump = translating.snapshot();
    require(!translating_jump.player_grounded,
            "jump must detach the player from translating support");
    require(translating_jump.player_linear_velocity.y > 4.0,
            "jump must create upward velocity without teleporting");
    require(translating_jump.player_linear_velocity.x * translating_support_velocity.x > 0.0,
            "jump must preserve the translating support velocity direction");
    require(std::abs(translating_jump.player_linear_velocity.x) >
                std::abs(translating_support_velocity.x) * 0.45,
            "jump must preserve a material fraction of inherited translating support velocity");

    require(translating.advance_frame(0.20).accepted,
            "airborne inherited-momentum interval must advance");
    const auto translating_airborne = translating.snapshot();
    require(!translating_airborne.player_grounded,
            "short post-jump interval must remain airborne");
    require(translating_airborne.player_linear_velocity.x * translating_support_velocity.x > 0.0,
            "air control must not immediately cancel inherited translating support momentum");

    Simulation rotating(InitialSpawn::RotatingSupport);
    require(rotating.set_move_input(0.0, 0.0),
            "zero relative movement must be accepted on a rotating support");
    require(rotating.advance_frame(1.2).accepted,
            "rotating-support settling interval must be accepted");
    const auto rotating_grounded = rotating.snapshot();
    require(rotating_grounded.player_grounded,
            "the player must settle on the native rotating support");
    require(rotating_grounded.support_entity_id == Simulation::kRotatingSupportEntityId,
            "rotating support must expose its stable native entity ID");
    require(std::abs(rotating_grounded.rotating_support_angular_velocity.y) > 0.5,
            "rotating support must expose material native angular velocity");
    require(horizontal_magnitude(rotating_grounded.support_point_linear_velocity) > 0.5,
            "rotating support must produce non-zero support-point linear velocity away from its axis");

    const double radius_x =
        rotating_grounded.support_contact_point.x - rotating_grounded.rotating_support_position.x;
    const double radius_z =
        rotating_grounded.support_contact_point.z - rotating_grounded.rotating_support_position.z;
    const double omega_y = rotating_grounded.rotating_support_angular_velocity.y;
    const double expected_point_x = omega_y * radius_z;
    const double expected_point_z = -omega_y * radius_x;
    require(nearly_equal(rotating_grounded.support_point_linear_velocity.x,
                         expected_point_x,
                         0.12) &&
                nearly_equal(rotating_grounded.support_point_linear_velocity.z,
                             expected_point_z,
                             0.12),
            "rotating support point velocity must obey omega cross r at the actual contact point");
    require(horizontal_dot(rotating_grounded.player_linear_velocity,
                           rotating_grounded.support_point_linear_velocity) > 0.15,
            "rotating support motion must be imparted to the grounded player");

    Simulation approach(InitialSpawn::ApproachGrade);
    require(approach.advance_frame(1.0).accepted,
            "exterior approach settling interval must be accepted");
    const auto approach_spawn = approach.snapshot();
    require(approach_spawn.player_grounded,
            "exterior approach spawn must settle on authoritative grade");
    require(approach_spawn.player_position.z > 50.0,
            "exterior approach spawn must begin a measured distance from the tower");
    require(!approach.can_operate_hopper(),
            "hopper release must reject remote interaction from the spawn point");
    require(!approach.request_hopper_release(),
            "remote player must not release the hopper through presentation-only authority");
    require(!approach.can_traverse(),
            "traversal assist must not advertise a route from empty approach grade");

    Simulation hopper(InitialSpawn::HopperControl);
    require(hopper.advance_frame(1.0).accepted,
            "hopper-control settling interval must be accepted");
    const auto hopper_closed = hopper.snapshot();
    require(hopper.can_operate_hopper(),
            "player at the native control point must be eligible to operate the hopper");
    require(hopper_closed.hopper_interaction_available,
            "snapshot must expose native hopper interaction eligibility");
    require(!hopper_closed.hopper_release_started,
            "hopper release must begin closed");
    require(!hopper_closed.hopper_gate_open,
            "hopper gate must begin closed");
    require(!hopper_closed.impact_rocker_struck,
            "downstream rocker must begin mechanically undisturbed");

    const auto load_initial = hopper_closed.hopper_load_position;
    const auto gate_initial = hopper_closed.hopper_gate_position;
    const double rocker_angle_initial = hopper_closed.impact_rocker_angle_radians;
    require(hopper.request_hopper_release(),
            "native control interaction must accept hopper release in range");
    require(!hopper.request_hopper_release(),
            "duplicate release request before the next tick must be rejected");
    require(hopper.advance_frame(5.5).accepted,
            "hopper-to-rocker causal interval must advance");

    const auto hopper_released = hopper.snapshot();
    require(hopper_released.hopper_release_started,
            "hopper release state must persist after the command");
    require(hopper_released.hopper_gate_open,
            "hopper gate must complete finite native travel");
    require(hopper_released.hopper_gate_position.x > gate_initial.x + 4.0,
            "hopper gate must move materially through Jolt rather than animation");
    require(hopper_released.hopper_load_moved,
            "released hopper matter must move in authoritative native state");
    require(distance_3d(hopper_released.hopper_load_position, load_initial) > 1.0,
            "hopper load must physically depart its initial support");
    require(hopper_released.hopper_load_position.y < load_initial.y - 0.8 ||
                hopper_released.hopper_load_position.z < load_initial.z - 0.8,
            "released matter must fall or travel down the physical chute");
    require(!hopper_released.hopper_interaction_available,
            "one-shot release control must stop advertising an already-started action");
    require(!hopper.request_hopper_release(),
            "released hopper must reject repeated action commands");
    std::cerr << "DIAG hopper_load=(" << hopper_released.hopper_load_position.x << ','
              << hopper_released.hopper_load_position.y << ','
              << hopper_released.hopper_load_position.z << ") load_v=("
              << hopper_released.hopper_load_linear_velocity.x << ','
              << hopper_released.hopper_load_linear_velocity.y << ','
              << hopper_released.hopper_load_linear_velocity.z << ") rocker=("
              << hopper_released.impact_rocker_position.x << ','
              << hopper_released.impact_rocker_position.y << ','
              << hopper_released.impact_rocker_position.z << ") rocker_angle="
              << hopper_released.impact_rocker_angle_radians << " rocker_w=("
              << hopper_released.impact_rocker_angular_velocity.x << ','
              << hopper_released.impact_rocker_angular_velocity.y << ','
              << hopper_released.impact_rocker_angular_velocity.z << ")\n";
    require(hopper_released.impact_rocker_struck,
            "released hopper matter must physically propagate into the downstream rocker");
    require(std::abs(hopper_released.impact_rocker_angle_radians - rocker_angle_initial) > 0.03 ||
                std::abs(hopper_released.impact_rocker_angular_velocity.x) > 0.10,
            "second machine consequence must come from real rocker rotation/angular velocity");

    Simulation traversal(InitialSpawn::TraversalCourse);
    require(traversal.advance_frame(1.0).accepted,
            "traversal-course settling interval must be accepted");
    const auto vault_ready = traversal.snapshot();
    require(vault_ready.player_grounded,
            "vault course must begin from real grade support");
    require(vault_ready.traversal_assist_available,
            "low obstacle must expose a bounded native traversal candidate");
    require(vault_ready.traversal_candidate_mode == TraversalMode::Vault,
            "first course candidate must classify as vault from actual obstacle geometry");
    require(traversal.request_traversal(),
            "vault request must be accepted only in the valid candidate window");
    require(!traversal.request_traversal(),
            "duplicate traversal request before a tick must be rejected");
    require(traversal.advance_frame(0.78).accepted,
            "vault controller interval must advance through physical simulation");
    const auto vaulted = traversal.snapshot();
    require(vaulted.player_position.z < 35.25,
            "vault must carry the physical player beyond the low blocker without teleporting");
    require(vaulted.player_position.y > 0.80,
            "vault must preserve a physically valid player height");

    require(traversal.set_move_input(0.0, -1.0),
            "course locomotion toward mantle must be accepted");
    bool mantle_candidate_found = false;
    for (int step = 0; step < 180; ++step) {
        require(traversal.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "mantle approach fixed step must advance");
        const auto candidate = traversal.snapshot();
        if (candidate.traversal_candidate_mode == TraversalMode::Mantle) {
            mantle_candidate_found = true;
            break;
        }
    }
    require(mantle_candidate_found,
            "real high obstacle geometry must become a mantle candidate after approach");
    require(traversal.set_move_input(0.0, 0.0),
            "mantle test must be able to stop ordinary locomotion");
    require(traversal.request_traversal(),
            "mantle request must be accepted from the bounded candidate window");
    require(traversal.advance_frame(1.10).accepted,
            "mantle controller interval must advance through physical simulation");
    require(traversal.advance_frame(0.45).accepted,
            "mantle landing interval must allow gravity/contact to settle");
    const auto mantled = traversal.snapshot();
    require(mantled.player_position.y > 2.85,
            "mantle must raise the actual player body onto the higher support");
    require(mantled.player_position.z < 32.9,
            "mantle must move the actual player body past the ledge face");
    require(mantled.player_grounded,
            "mantle must terminate on physically supported geometry rather than a scripted pose");
    require(mantled.support_entity_id == Simulation::kMantleBlockEntityId,
            "mantle landing support must be the actual native ledge body");

    Simulation hang(InitialSpawn::HangCourse);
    const auto hang_candidate = hang.snapshot();
    require(hang_candidate.traversal_assist_available,
            "airborne player within ledge reach must expose a native hang candidate");
    require(hang_candidate.traversal_candidate_mode == TraversalMode::Hang,
            "ledge candidate must classify as hang rather than a route flag");
    require(hang.request_traversal(),
            "ledge-grab request must be accepted inside the reach window");
    require(hang.advance_frame(0.55).accepted,
            "hang acquisition controller must advance through physical simulation");
    const auto hanging = hang.snapshot();
    require(hanging.traversal_mode == TraversalMode::Hang,
            "hang controller must remain active after acquiring the physical ledge");
    require(distance_3d(hanging.player_position, hanging.traversal_target_position) < 0.45,
            "hang controller must converge toward the native ledge target without teleporting");
    require(hanging.player_position.y > 2.75,
            "hang controller must arrest the fall at the actual ledge");

    require(hang.request_jump(),
            "jump from a valid hang must request a climb transition");
    require(hang.advance_frame(0.95).accepted,
            "hang-to-mantle controller interval must advance");
    require(hang.advance_frame(0.55).accepted,
            "hang-to-mantle landing interval must settle on contact");
    const auto climbed = hang.snapshot();
    require(climbed.player_position.y > 4.80,
            "hang-to-mantle must raise the physical player above the ledge top");
    require(climbed.player_position.z < 27.0,
            "hang-to-mantle must move the physical player behind the ledge face");
    require(climbed.player_grounded,
            "hang-to-mantle must finish on real support rather than a floating controller state");
    require(climbed.support_entity_id == Simulation::kHangLedgeEntityId,
            "climb completion must report the actual native hang ledge as support");

    Simulation hang_drop(InitialSpawn::HangCourse);
    require(hang_drop.request_traversal(),
            "second hang fixture must accept a ledge grab");
    require(hang_drop.advance_frame(0.40).accepted,
            "second hang acquisition must advance");
    const auto before_drop = hang_drop.snapshot();
    require(before_drop.traversal_mode == TraversalMode::Hang,
            "drop test must begin in hang mode");
    require(hang_drop.request_drop_from_hang(),
            "explicit drop must be accepted only from hang mode");
    require(hang_drop.advance_frame(0.25).accepted,
            "drop interval must advance under gravity");
    const auto after_drop = hang_drop.snapshot();
    require(after_drop.traversal_mode != TraversalMode::Hang,
            "drop command must release the native hang controller");
    require(after_drop.player_position.y < before_drop.player_position.y - 0.10,
            "released hang must actually fall rather than switch animation state");

    std::cout << "PASS scraperx_sim CP-004 traversal-impact checkpoint: "
              << "approach_z=" << approach_spawn.player_position.z
              << " translating_vx=" << translating_support_velocity.x
              << " jump_vx=" << translating_jump.player_linear_velocity.x
              << " rocker_angle=" << hopper_released.impact_rocker_angle_radians
              << " rocker_omega_x=" << hopper_released.impact_rocker_angular_velocity.x
              << " vault_z=" << vaulted.player_position.z
              << " mantle_support=" << mantled.support_entity_id
              << " hang_support=" << climbed.support_entity_id
              << " hz=" << Simulation::kTickRateHz << '\n';
    return EXIT_SUCCESS;
}
