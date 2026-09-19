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

    Simulation grounded_block(InitialSpawn::StaticDeck);
    require(grounded_block.advance_frame(1.0).accepted, "static deck settle must advance");
    require(grounded_block.snapshot().player_grounded,
            "static-deck spawn must finish on the deck after a full fall from authored height");
    require(!grounded_block.request_parachute(),
            "parachute must refuse deployment while supported");
    require(grounded_block.commit_checkpoint(),
            "grounded player must be able to commit a checkpoint");
    require(grounded_block.snapshot().checkpoint_committed,
            "committed checkpoint flag must be visible on the snapshot");

    Simulation high(InitialSpawn::HighDeck);
    require(high.advance_frame(0.50).accepted, "high platform settle must advance");
    require(high.snapshot().player_grounded, "high-deck spawn must stand on the high platform");
    require(high.commit_checkpoint(), "high platform must accept a checkpoint");
    require(high.set_move_input(0.0, 1.0), "walk-off input must be accepted");
    require(high.advance_frame(1.10).accepted, "walk-off from the high platform must advance");
    require(!high.snapshot().player_grounded, "walking off the high platform must create a real fall");
    require(high.request_parachute(), "open fall with clearance must accept a chute");
    require(high.advance_frame(0.80).accepted, "chute descent interval must advance");
    const auto chuted = high.snapshot();
    require(chuted.parachute_deployed, "accepted chute request must mark deployed state");
    require(chuted.player_linear_velocity.y > -10.0,
            "chute must cap sink rate rather than grant a powered climb or a free fall");
    require(chuted.player_position.y > 0.5, "chute must not teleport to the refuge");
    require(chuted.fear_event_id >= 1, "a material fall must emit a fear event");

    Simulation lethal(InitialSpawn::HighDeck);
    require(lethal.advance_frame(0.50).accepted, "lethal fixture settle must advance");
    const auto committed_pose = lethal.snapshot().player_position;
    require(lethal.commit_checkpoint(), "lethal fixture must commit before the fall");
    require(lethal.set_move_input(0.0, 1.0), "lethal walk-off input must be accepted");
    bool left_committed_support = false;
    bool restored_near_platform = false;
    const int lethal_budget_steps =
        static_cast<int>(4.0 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < lethal_budget_steps; ++step) {
        require(lethal.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "un-chuted high fall must be allowed to finish");
        const auto now = lethal.snapshot();
        if (now.player_position.y < committed_pose.y - 3.0) {
            left_committed_support = true;
        }
        if (left_committed_support && now.player_grounded &&
            now.support_entity_id == Simulation::kHighPlatformEntityId &&
            std::abs(now.player_position.x - committed_pose.x) < 2.5 &&
            std::abs(now.player_position.z - committed_pose.z) < 2.5) {
            restored_near_platform = true;
            break;
        }
    }
    require(left_committed_support, "lethal walk-off must produce a real fall");
    require(restored_near_platform,
            "restored pose must return near the committed platform, not a mid-air teleport");
    require(lethal.snapshot().checkpoint_committed, "death must keep the committed timeline");

    Simulation remote_jib;
    require(remote_jib.advance_frame(1.0).accepted, "approach settle for remote jib must advance");
    require(!remote_jib.can_enter_jib_station(),
            "KX-JIB pendant must reject station entry from the distant approach grade");
    require(!remote_jib.request_enter_jib_station(),
            "remote Action must not become a hoist solve button");
    require(!remote_jib.set_jib_hoist_input(1.0),
            "raise must be rejected when the player is not at the pendant");
    require(!remote_jib.set_jib_brake(false),
            "brake release must be rejected away from the station");

    Simulation jib(InitialSpawn::JibStation);
    require(jib.advance_frame(1.0).accepted, "jib-station settling interval must be accepted");
    const auto jib_idle = jib.snapshot();
    require(jib_idle.player_grounded, "pendant spawn must settle on grade");
    require(jib.can_enter_jib_station(),
            "player at CAP-PENDANT must be eligible to enter the local station");
    require(jib_idle.jib_brake_engaged, "cold KX-JIB must start with the brake holding");
    require(jib_idle.jib_crate_mass_kg < jib_idle.jib_swl_kg,
            "rated crate must be inside the 5 t SWL design target");
    require(jib_idle.jib_hook_attached,
            "CAP-HOOK5 pre-placement must be a live hook/crate constraint");
    const auto crate_rest = jib_idle.jib_crate_position;
    require(jib.request_enter_jib_station(), "Action at the pendant must enter the station");
    require(jib.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "station-enter tick must advance");
    require(jib.snapshot().jib_station_occupied, "native station occupancy must persist");
    require(!jib.can_enter_jib_station(),
            "occupied station must stop advertising enter as a solve");

    require(jib.set_jib_hoist_input(1.0), "raise command at an occupied station must be accepted");
    require(jib.advance_frame(2.0).accepted, "braked raise interval must advance");
    const auto braked = jib.snapshot();
    require(braked.jib_brake_engaged, "brake must remain engaged until explicitly released");
    require(braked.jib_stalled, "locked brake must refuse free hoist work");
    require(std::abs(braked.jib_crate_position.y - crate_rest.y) < 0.20,
            "locked brake must not lift the crate");

    require(jib.set_jib_brake(false), "pendant must accept a brake release");
    require(jib.set_jib_hoist_input(1.0), "raise after brake release must be accepted");
    require(jib.advance_frame(2.6).accepted, "in-SWL hoist interval must advance");
    const auto lifted = jib.snapshot();
    require(lifted.jib_crate_position.y > crate_rest.y + 1.40,
            "KX-JIB must lift KX-CRATE by bounded winch work, not a teleport");
    require(lifted.jib_hook_attached, "raised crate must remain on the real hook constraint");
    require(lifted.jib_winch_length_meters < jib_idle.jib_winch_length_meters - 1.20,
            "winch length must shorten as the actuator pays in");
    const auto lifted_crate_y = lifted.jib_crate_position.y;

    require(jib.set_jib_brake(true), "pendant must accept a holding brake");
    require(jib.set_jib_hoist_input(1.0), "raise against a holding brake must still be accepted as a command");
    require(jib.advance_frame(1.2).accepted, "holding-brake interval must advance");
    const auto holding = jib.snapshot();
    require(holding.jib_stalled, "holding brake must report stall rather than free work");
    require(std::abs(holding.jib_crate_position.y - lifted_crate_y) < 0.25,
            "brake must hold the suspended crate instead of continuing the hoist");

    require(jib.set_jib_brake(false), "second brake release must be accepted");
    require(jib.set_jib_hoist_input(1.0), "raise to travel limit must be accepted");
    require(jib.advance_frame(6.0).accepted, "travel-limit hoist interval must advance");
    const auto at_limit = jib.snapshot();
    require(at_limit.jib_at_hoist_limit, "winch must stop at the authored travel limit");
    require(at_limit.jib_winch_length_meters <= 2.45,
            "travel-limit stop must be the minimum winch length, not a solved pose");
    const auto limit_y = at_limit.jib_crate_position.y;
    require(jib.advance_frame(1.0).accepted, "post-limit raise interval must advance");
    require(std::abs(jib.snapshot().jib_crate_position.y - limit_y) < 0.35,
            "commanding raise at the travel stop must not produce extra free lift");

    require(jib.set_jib_hoist_input(-1.0), "lower command must be accepted at the station");
    require(jib.advance_frame(1.6).accepted, "lower interval must advance");
    require(jib.snapshot().jib_crate_position.y < limit_y - 0.60,
            "paying out the winch must lower the constrained crate");

    require(jib.set_jib_hoist_input(0.0), "neutral hoist must be accepted");
    require(jib.set_jib_slew_input(1.0), "slew command must be accepted at the station");
    const auto pre_slew = jib.snapshot().jib_slew_radians;
    require(jib.advance_frame(1.4).accepted, "slew interval must advance");
    require(jib.snapshot().jib_slew_radians > pre_slew + 0.20,
            "slew must rotate the boom through finite actuator travel");

    require(jib.commit_checkpoint(), "freight checkpoint must accept a grounded commit");
    const auto committed_crate = jib.snapshot().jib_crate_position;
    const auto committed_winch = jib.snapshot().jib_winch_length_meters;

    Simulation ride(InitialSpawn::JibStation);
    require(ride.advance_frame(1.0).accepted, "ride fixture settle must advance");
    require(ride.request_enter_jib_station(), "ride fixture must enter the pendant");
    require(ride.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "ride fixture enter tick must advance");
    require(ride.set_move_input(1.0, -0.2), "move from pendant onto the crate must be accepted");
    bool boarded = false;
    for (int step = 0; step < 220; ++step) {
        require(ride.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "crate boarding step must advance");
        const auto now = ride.snapshot();
        if (now.support_entity_id == Simulation::kJibCrateEntityId && now.player_grounded) {
            boarded = true;
            break;
        }
    }
    require(boarded, "player must be able to stand on KX-CRATE as real support");
    require(ride.snapshot().jib_station_occupied,
            "boarding the nearby crate must keep the player inside the pendant radius");
    require(ride.set_move_input(0.0, 0.0), "zero relative input on the crate must be accepted");
    require(ride.set_jib_brake(false), "occupied station must accept brake release while on the crate");
    require(ride.set_jib_hoist_input(1.0), "occupied station must accept raise while on the crate");
    const auto boarded_y = ride.snapshot().player_position.y;
    require(ride.advance_frame(2.2).accepted, "ridden hoist interval must advance");
    const auto ridden = ride.snapshot();
    require(ridden.support_entity_id == Simulation::kJibCrateEntityId,
            "player must remain supported by the moving crate");
    require(ridden.player_position.y > boarded_y + 0.70,
            "valid crate support must carry the player with the hoisted load");
    require(std::abs(ridden.player_linear_velocity.y - ridden.support_point_linear_velocity.y) < 1.6,
            "ridden crate must impart support-point velocity (WO-002 law)");

    Simulation overweight(InitialSpawn::JibOverweight);
    require(overweight.advance_frame(1.0).accepted, "overweight fixture settle must advance");
    const auto heavy_rest = overweight.snapshot().jib_crate_position;
    require(overweight.snapshot().jib_crate_mass_kg > overweight.snapshot().jib_swl_kg,
            "overweight fixture must exceed the 5 t SWL design target");
    require(overweight.request_enter_jib_station(), "overweight pendant entry must be accepted");
    require(overweight.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "overweight enter tick must advance");
    require(overweight.set_jib_brake(false), "overweight brake release must be accepted");
    require(overweight.set_jib_hoist_input(1.0), "overweight raise command must be accepted");
    require(overweight.advance_frame(3.0).accepted, "overweight stall interval must advance");
    const auto stalled_heavy = overweight.snapshot();
    require(stalled_heavy.jib_stalled, "raise against overweight must stall the actuator");
    require(std::abs(stalled_heavy.jib_crate_position.y - heavy_rest.y) < 0.25,
            "overweight crate must not receive free unlimited-force lift");
    require(std::abs(stalled_heavy.jib_winch_length_meters - jib_idle.jib_winch_length_meters) < 0.20,
            "stalled winch must not shorten past the load's SWL");

    (void)committed_crate;
    (void)committed_winch;

    Simulation gap(InitialSpawn::NeedleNearLanding);
    require(gap.advance_frame(1.0).accepted, "needle near-landing settle must advance");
    const auto gap_idle = gap.snapshot();
    require(gap_idle.player_grounded, "near-landing spawn must stand on the bay deck");
    require(gap_idle.support_entity_id == Simulation::kNeedleNearLandingEntityId,
            "unseated fixture must begin on the near landing, not an invisible walkbox");
    require(!gap_idle.needle_seated, "KX-NEEDLE must begin unseated in the gap fixture");
    require(gap.set_move_input(1.0, 0.0), "walk toward the far landing must be accepted");
    bool fell_the_gap = false;
    bool cheated_across = false;
    const int gap_budget = static_cast<int>(3.6 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < gap_budget; ++step) {
        require(gap.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "unseated gap crossing step must advance");
        const auto now = gap.snapshot();
        if (now.support_entity_id == Simulation::kNeedleFarLandingEntityId ||
            now.support_entity_id == Simulation::kNeedleBayFloorEntityId) {
            cheated_across = true;
        }
        if (now.player_position.y < 3.2) {
            fell_the_gap = true;
            break;
        }
    }
    require(fell_the_gap, "unseated KX-NEEDLE must leave the bay as a real fall");
    require(!cheated_across, "the far landing must not exist as support while the needle is unseated");

    Simulation seated(InitialSpawn::NeedleSeated);
    require(seated.advance_frame(1.0).accepted, "seated-needle settle must advance");
    const auto seated_idle = seated.snapshot();
    require(seated_idle.needle_seated, "NeedleSeated spawn must begin with a seated structural member");
    require(seated_idle.player_grounded, "seated fixture must stand on the near landing");
    require(seated.set_move_input(1.0, 0.0), "walk across the seated needle must be accepted");
    bool walked_needle = false;
    bool reached_far = false;
    const int seat_budget = static_cast<int>(5.5 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < seat_budget; ++step) {
        require(seated.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "seated crossing step must advance");
        const auto now = seated.snapshot();
        if (now.support_entity_id == Simulation::kNeedleEntityId && now.player_grounded) {
            walked_needle = true;
        }
        if (now.player_grounded && now.player_position.y > 5.0 &&
            (now.support_entity_id == Simulation::kNeedleFarLandingEntityId ||
             now.support_entity_id == Simulation::kNeedleBayFloorEntityId ||
             (now.support_entity_id == Simulation::kNeedleEntityId && now.player_position.x > -2.0))) {
            if (now.player_position.x > -1.8) {
                reached_far = true;
            }
        }
        if (walked_needle && reached_far) {
            break;
        }
    }
    require(walked_needle, "player must walk the seated KX-NEEDLE as real support");
    require(reached_far, "seated needle must change traversal so the far landing is reachable");
    require(seated.snapshot().player_position.y > 4.8,
            "the seated span must carry the player, not drop them to grade");
    require(seated.commit_checkpoint(), "seated span must accept a grounded checkpoint");
    require(seated.snapshot().checkpoint_committed && seated.snapshot().needle_seated,
            "checkpoint must store seated structural state");

    Simulation bay(InitialSpawn::NeedleBay);
    require(bay.advance_frame(1.0).accepted, "needle-bay settle must advance");
    const auto bay_idle = bay.snapshot();
    require(!bay_idle.needle_seated, "jib fixture must begin with a free needle");
    require(bay_idle.jib_hook_load == scraperx::sim::HookLoad::Needle,
            "NeedleBay must pre-hook KX-NEEDLE rather than auto-snap it into the pockets");
    require(bay.request_enter_jib_station(), "needle bay pendant entry must be accepted");
    require(bay.advance_frame(Simulation::kFixedStepSeconds).accepted, "needle bay enter tick must advance");
    require(bay.set_jib_brake(false), "needle bay brake release must be accepted");
    const double target_slew = std::atan2(43.90 - 48.0, -5.15 - (-12.0));
    bool jib_seated = false;
    int lower_phase = 0;
    const int jib_budget = static_cast<int>(16.0 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < jib_budget; ++step) {
        const auto now = bay.snapshot();
        if (now.needle_seated) {
            jib_seated = true;
            break;
        }
        const double slew_err = target_slew - now.jib_slew_radians;
        double slew_cmd = slew_err / 0.12;
        if (slew_cmd > 1.0) {
            slew_cmd = 1.0;
        }
        if (slew_cmd < -1.0) {
            slew_cmd = -1.0;
        }
        if (std::abs(slew_err) < 0.05 && now.jib_winch_length_meters < 2.80) {
            lower_phase = 1;
        }
        const double winch_target = lower_phase == 0 ? 2.55 : 3.55;
        double hoist_cmd = (now.jib_winch_length_meters - winch_target) / 0.35;
        if (hoist_cmd > 1.0) {
            hoist_cmd = 1.0;
        }
        if (hoist_cmd < -1.0) {
            hoist_cmd = -1.0;
        }
        require(bay.set_jib_slew_input(slew_cmd), "needle placement slew must be accepted");
        require(bay.set_jib_hoist_input(hoist_cmd), "needle placement hoist must be accepted");
        require(bay.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "needle placement step must advance");
    }
    require(jib_seated, "KX-JIB must seat KX-NEEDLE in KX-POCKETS by bounded hoist/slew, not a flag");

    require(bay.set_jib_hoist_input(1.0), "raise after seat must be accepted as the legal unseat path");
    require(bay.advance_frame(2.2).accepted, "unseat hoist interval must advance");
    require(!bay.snapshot().needle_seated,
            "raising a hooked seated needle must unseat it and remove the span");

    Simulation locked(InitialSpawn::CageDeck);
    require(locked.advance_frame(1.0).accepted, "unseated cage settle must advance");
    const auto locked_idle = locked.snapshot();
    require(locked_idle.player_grounded, "cage deck spawn must land on KX-CAGE");
    require(locked_idle.support_entity_id == Simulation::kCageEntityId,
            "unseated cage fixture must stand on the cage, not an invisible floor");
    require(!locked_idle.needle_seated, "CageDeck must begin with the needle free");
    require(locked_idle.cage_brake_engaged, "cage brake must default ON");
    require(locked.can_operate_cage(), "player on the cage must reach the local lever");
    const auto locked_y = locked_idle.cage_position.y;
    require(locked.request_cage_lever(), "unseated lever pull must be accepted as a real request");
    require(locked.advance_frame(2.0).accepted, "interlock stall interval must advance");
    const auto locked_after = locked.snapshot();
    require(locked_after.cage_stalled, "raise with KX-NEEDLE unseated must stall the cage interlock");
    require(std::abs(locked_after.cage_position.y - locked_y) < 0.08,
            "interlock stall must not give free shaft travel");
    require(locked_after.support_entity_id == Simulation::kCageEntityId,
            "stalled cage must remain the standing support");

    Simulation ride_cage(InitialSpawn::CageSeated);
    require(ride_cage.advance_frame(1.0).accepted, "seated cage settle must advance");
    const auto ride_idle = ride_cage.snapshot();
    require(ride_idle.needle_seated, "CageSeated spawn must begin with a seated needle");
    require(ride_idle.support_entity_id == Simulation::kCageEntityId,
            "seated cage fixture must stand on KX-CAGE");
    require(ride_cage.can_operate_cage(), "seated cage lever must be in reach");
    require(ride_cage.request_cage_lever(), "seated lever pull must be accepted");
    require(ride_cage.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "cage lever tick must advance");
    const auto boarded_cage_y = ride_cage.snapshot().player_position.y;
    require(ride_cage.advance_frame(3.2).accepted, "cage raise interval must advance");
    const auto raising = ride_cage.snapshot();
    require(raising.support_entity_id == Simulation::kCageEntityId,
            "player must remain supported by the moving cage");
    require(raising.player_position.y > boarded_cage_y + 2.4,
            "valid cage support must carry the player up the well");
    require(raising.cage_position.y > locked_y + 2.4,
            "seated interlock must allow finite drum travel");
    require(std::abs(raising.player_linear_velocity.y - raising.support_point_linear_velocity.y) < 1.6,
            "ridden cage must impart support-point velocity (WO-002 law)");
    require(!raising.cage_brake_engaged, "live raise must hold the brake off");

    bool reached_top = raising.cage_at_limit && raising.cage_position.y > 21.0;
    if (!reached_top) {
        const int travel_budget = static_cast<int>(12.0 / Simulation::kFixedStepSeconds + 0.5);
        for (int step = 0; step < travel_budget; ++step) {
            require(ride_cage.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "cage travel-limit step must advance");
            const auto now = ride_cage.snapshot();
            if (now.cage_at_limit && now.cage_position.y > 21.0) {
                reached_top = true;
                break;
            }
        }
    }
    require(reached_top, "cage drum must stop at the authored upper landing, not climb forever");
    const auto at_top = ride_cage.snapshot();
    require(at_top.cage_position.y <= 21.90,
            "travel-limit stop must be the authored cage maximum");
    const auto cage_limit_y = at_top.cage_position.y;
    require(ride_cage.advance_frame(1.0).accepted, "post-limit idle interval must advance");
    require(std::abs(ride_cage.snapshot().cage_position.y - cage_limit_y) < 0.08,
            "commanding extra raise at the travel stop must not produce free lift");

    require(ride_cage.set_move_input(0.0, 1.0), "walk off the raised cage must be accepted");
    bool boarded_upper = false;
    const int walk_budget = static_cast<int>(3.5 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < walk_budget; ++step) {
        require(ride_cage.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "upper-landing boarding step must advance");
        const auto now = ride_cage.snapshot();
        if (now.player_grounded && now.player_position.y > 21.0 &&
            (now.support_entity_id == Simulation::kCageUpperLandingEntityId ||
             now.player_position.z > 36.4)) {
            boarded_upper = true;
            break;
        }
    }
    require(boarded_upper, "raised cage must change traversal onto the upper landing");
    const auto upper_support = ride_cage.snapshot().support_entity_id;
    require(ride_cage.snapshot().player_position.y > 21.0,
            "the upper landing must exist as support at the cage travel stop");

    require(ride_cage.set_move_input(0.0, -1.0), "return onto the cage to lower must be accepted");
    bool back_on_cage = false;
    for (int step = 0; step < walk_budget; ++step) {
        require(ride_cage.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "return-to-cage step must advance");
        if (ride_cage.snapshot().support_entity_id == Simulation::kCageEntityId &&
            ride_cage.snapshot().player_grounded) {
            back_on_cage = true;
            break;
        }
    }
    require(back_on_cage, "player must be able to step back onto the cage at the upper stop");
    require(ride_cage.request_cage_lever(), "lever pull at the top must start the down-drum");
    require(ride_cage.advance_frame(3.0).accepted, "cage lower interval must advance");
    require(ride_cage.snapshot().cage_position.y < cage_limit_y - 2.0,
            "paying the drum out from the top must lower the cage");

    Simulation remote_sump;
    require(remote_sump.advance_frame(1.0).accepted, "approach settle for remote sump must advance");
    require(!remote_sump.can_operate_sump_valve(),
            "KX-SUMP valve must reject isolation from the distant approach grade");
    require(!remote_sump.request_sump_valve(),
            "remote Action must not become an isolate-solve button");
    require(!remote_sump.can_operate_sump_drain(),
            "KX-SUMP drain must reject operation away from the cock");
    require(!remote_sump.request_sump_drain(),
            "remote Action must not open the drain");

    Simulation wet(InitialSpawn::SumpLanding);
    require(wet.advance_frame(1.0).accepted, "sump-landing settle must advance");
    const auto wet_idle = wet.snapshot();
    require(wet_idle.player_grounded, "sump fixture must stand on the cage-house landing");
    require(!wet_idle.sump_grate_safe, "cold KX-SUMP must start wet");
    require(!wet_idle.sump_isolated, "live fill line must start open");
    require(wet_idle.sump_inventory > 0.90, "undrained inventory must be a full lumped volume");
    require(wet.can_operate_sump_valve(), "player on the landing must reach the isolation wheel");
    require(wet.can_operate_sump_drain(), "player on the landing must reach the drain cock");

    require(wet.set_move_input(0.0, 1.0), "walk onto the wet grate must be accepted");
    bool fell_wet = false;
    const int wet_walk_budget = static_cast<int>(1.6 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < wet_walk_budget; ++step) {
        require(wet.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "wet-grate walk step must advance");
        const auto now = wet.snapshot();
        if (now.player_position.y < wet_idle.player_position.y - 1.20 &&
            now.support_entity_id != Simulation::kSumpGrateEntityId) {
            fell_wet = true;
            break;
        }
    }
    require(fell_wet, "wet KX-GRATE must be a hole, not a painted solid walkway");

    Simulation live_drain(InitialSpawn::SumpLanding);
    require(live_drain.advance_frame(1.0).accepted, "live-drain settle must advance");
    require(live_drain.request_sump_drain(), "drain cock must accept an open while the line is live");
    require(live_drain.advance_frame(5.0).accepted, "unisolated drain interval must advance");
    const auto still_wet = live_drain.snapshot();
    require(!still_wet.sump_isolated, "drain-without-isolate must leave the fill line live");
    require(still_wet.sump_inventory > 0.80,
            "live fill must overwhelm the drain so inventory does not magically empty");
    require(!still_wet.sump_grate_safe, "a timer must not dry the grate without isolation");

    Simulation isolate(InitialSpawn::SumpLanding);
    require(isolate.advance_frame(1.0).accepted, "isolate fixture settle must advance");
    require(isolate.request_sump_valve(), "isolation wheel must accept a local close");
    require(isolate.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "isolate tick must advance");
    require(isolate.snapshot().sump_isolated, "valve Action must isolate the lumped volume");
    require(!isolate.snapshot().sump_grate_safe,
            "isolation alone must not invent a dry grate without drain inventory");
    require(isolate.request_sump_drain(), "isolated drain cock must accept an open");
    require(isolate.advance_frame(5.0).accepted, "isolated drain interval must advance");
    const auto dried = isolate.snapshot();
    require(dried.sump_isolated, "isolation must hold while the volume dumps");
    require(dried.sump_drain_open, "drain cock must remain open during the dump");
    require(dried.sump_inventory <= 0.08, "isolated drain must empty the lumped inventory");
    require(dried.sump_grate_safe, "grate-safe must be derived from isolated empty inventory");

    require(isolate.set_move_input(0.0, 1.0), "walk the drained grate must be accepted");
    bool crossed = false;
    const int dry_walk_budget = static_cast<int>(2.8 / Simulation::kFixedStepSeconds + 0.5);
    for (int step = 0; step < dry_walk_budget; ++step) {
        require(isolate.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "dry-grate walk step must advance");
        const auto now = isolate.snapshot();
        if (now.player_grounded && now.player_position.y > 21.0 &&
            (now.support_entity_id == Simulation::kSumpGrateEntityId ||
             now.support_entity_id == Simulation::kSumpFarLandingEntityId)) {
            crossed = true;
            if (now.support_entity_id == Simulation::kSumpFarLandingEntityId) {
                break;
            }
        }
        if (now.player_position.y < 19.5) {
            break;
        }
    }
    require(crossed, "dry KX-GRATE must be ordinary support to the far landing");
    require(isolate.snapshot().player_position.y > 21.0,
            "drained traversal must stay on the +22 m machine floor, not drop into the pit");

    Simulation dump(InitialSpawn::SumpDrained);
    require(dump.advance_frame(1.0).accepted, "drained fixture settle must advance");
    const auto dump_idle = dump.snapshot();
    require(dump_idle.sump_grate_safe, "SumpDrained spawn must begin with a walkable grate");
    require(dump_idle.sump_isolated, "SumpDrained spawn must begin isolated");
    require(dump.can_operate_sump_valve(), "dump fixture must reach the isolation wheel");
    require(dump.request_sump_valve(), "opening the isolation wheel must be a real dump");
    require(dump.advance_frame(3.0).accepted, "dump refill interval must advance");
    const auto dumped = dump.snapshot();
    require(!dumped.sump_isolated, "dump must put the fill line back on the volume");
    require(dumped.sump_inventory > 0.50, "live line must restore inventory after a dump");
    require(!dumped.sump_grate_safe, "dump/fail must make the grate unsafe again");
    require(dump.set_move_input(0.0, 1.0), "walk the dumped grate must be accepted");
    bool fell_dump = false;
    for (int step = 0; step < wet_walk_budget; ++step) {
        require(dump.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "dumped-grate walk step must advance");
        const auto now = dump.snapshot();
        if (now.player_position.y < dump_idle.player_position.y - 1.20 &&
            now.support_entity_id != Simulation::kSumpGrateEntityId) {
            fell_dump = true;
            break;
        }
    }
    require(fell_dump, "dump/fail must restore the fall, not keep a solved walkbox");

    require(isolate.commit_checkpoint(), "process checkpoint must accept a grounded commit");
    require(isolate.snapshot().checkpoint_committed,
            "committed sump state must be visible on the snapshot");

    std::cout << "PASS scraperx_sim WO-005 first freight: "
              << "approach_z=" << approach_spawn.player_position.z
              << " translating_vx=" << translating_support_velocity.x
              << " jump_vx=" << translating_jump.player_linear_velocity.x
              << " rocker_angle=" << hopper_released.impact_rocker_angle_radians
              << " rocker_omega_x=" << hopper_released.impact_rocker_angular_velocity.x
              << " vault_z=" << vaulted.player_position.z
              << " mantle_support=" << mantled.support_entity_id
              << " hang_support=" << climbed.support_entity_id
              << " crate_lift_y=" << lifted.jib_crate_position.y
              << " winch=" << lifted.jib_winch_length_meters
              << " stall=" << stalled_heavy.jib_stalled
              << " hz=" << Simulation::kTickRateHz << '\n';
    std::cout << "PASS scraperx_sim WO-006 first structural coupling: "
              << "unseated_fall_y=" << gap.snapshot().player_position.y
              << " seated_far_x=" << seated.snapshot().player_position.x
              << " seated_support=" << seated.snapshot().support_entity_id
              << " jib_seated=" << jib_seated
              << " unseated_after_raise=" << !bay.snapshot().needle_seated
              << " hz=" << Simulation::kTickRateHz << '\n';
    std::cout << "PASS scraperx_sim KX-CAGE first shaft: "
              << "interlock_stall=" << locked_after.cage_stalled
              << " raise_y=" << raising.cage_position.y
              << " top_y=" << at_top.cage_position.y
              << " upper_support=" << upper_support
              << " hz=" << Simulation::kTickRateHz << '\n';
    std::cout << "PASS scraperx_sim WO-007 first process coupling: "
              << "wet_fall=" << fell_wet
              << " isolated=" << dried.sump_isolated
              << " inventory=" << dried.sump_inventory
              << " grate_safe=" << dried.sump_grate_safe
              << " dump_unsafe=" << !dumped.sump_grate_safe
              << " hz=" << Simulation::kTickRateHz << '\n';
    return EXIT_SUCCESS;
}
