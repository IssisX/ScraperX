#include "sim/simulation.hpp"

#include <algorithm>
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

double horizontal_magnitude(const scraperx::sim::Vector3 &value) {
    return std::hypot(value.x, value.z);
}

double horizontal_dot(const scraperx::sim::Vector3 &a,
                      const scraperx::sim::Vector3 &b) {
    return a.x * b.x + a.z * b.z;
}

// Steps the authoritative clock one fixed step at a time until the predicate
// holds against a real snapshot, or the budget expires. Tests never reach into
// the simulation to force a state.
template <typename Predicate>
bool advance_until(scraperx::sim::Simulation &simulation,
                   Predicate predicate,
                   const double budget_seconds) {
    const auto budget_ticks = static_cast<std::uint32_t>(
        budget_seconds * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < budget_ticks; ++tick) {
        if (!simulation.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds).accepted) {
            return false;
        }
        if (predicate(simulation.snapshot())) {
            return true;
        }
    }
    return false;
}

scraperx::sim::Snapshot run_mantle_command_stream(const bool single_fixed_steps) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    Simulation simulation(InitialSpawn::MantleApproach);
    require(simulation.set_facing(1.0, 0.0), "partition run must accept facing");

    const auto run_one_second = [&simulation, single_fixed_steps]() {
        if (single_fixed_steps) {
            for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz; ++tick) {
                require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                        "partition run fixed step must be accepted");
            }
        } else {
            require(simulation.advance_frame(1.0).accepted,
                    "partition run batched second must be accepted");
        }
    };

    run_one_second();
    require(simulation.request_traversal(), "partition run traversal request must be accepted");
    run_one_second();
    return simulation.snapshot();
}


// Runs the machine for a whole number of cycles and reports what the chain did.
struct MachineCycleReport final {
    double peak_valve_fraction = 0.0;
    double peak_lift_height = 0.0;
    double peak_piston_force = 0.0;
    double mass_flow_while_shut = 0.0;
    double final_available_energy = 0.0;
};

MachineCycleReport run_machine_cycles(scraperx::sim::Simulation &simulation,
                                      const double seconds) {
    using scraperx::sim::Simulation;
    MachineCycleReport report;
    const auto ticks = static_cast<std::uint32_t>(
        seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "machine cycle step must be accepted");
        const auto state = simulation.snapshot();
        report.peak_valve_fraction =
            std::max(report.peak_valve_fraction, state.valve_open_fraction);
        report.peak_lift_height =
            std::max(report.peak_lift_height, state.lift_platform_position.y);
        report.peak_piston_force = std::max(report.peak_piston_force, state.piston_force_n);
        if (state.valve_open_fraction <= 0.0) {
            report.mass_flow_while_shut =
                std::max(report.mass_flow_while_shut, state.orifice_mass_flow_kg_per_s);
        }
        report.final_available_energy = state.vessel_available_energy_j;
    }
    return report;
}

} // namespace

int main() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

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
            "the native player capsule must be grounded after falling onto the static deck");
    require(supported_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static support must expose the stable deck entity ID");
    require(nearly_equal(supported_snapshot.player_position.y, 0.9, 0.03),
            "the supported capsule center must settle at the static deck contact height");
    require(horizontal_magnitude(supported_snapshot.support_point_linear_velocity) < 1.0e-5,
            "the static deck support-point velocity must be zero");

    require(supported.set_move_input(1.0, 0.0),
            "finite desired movement input must be accepted");
    require(supported.advance_frame(0.5).accepted,
            "static-deck locomotion interval must be accepted");
    const auto moved_snapshot = supported.snapshot();
    require(moved_snapshot.player_position.x > supported_snapshot.player_position.x + 1.0,
            "desired relative velocity input must move the native body across the static deck");
    require(moved_snapshot.player_grounded,
            "horizontal locomotion must preserve static-deck support");
    require(moved_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static locomotion support identity must remain native and stable");

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

    const auto translating_support_velocity =
        translating_grounded.support_point_linear_velocity;
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


    // ---- WO-003 athletic traversal ---------------------------------------

    Simulation vault(InitialSpawn::VaultApproach);
    require(vault.set_facing(1.0, 0.0), "vault facing must be accepted");
    require(vault.set_move_input(0.0, 0.0), "vault settle input must be accepted");
    require(vault.advance_frame(1.0).accepted, "vault settling interval must be accepted");
    require(vault.snapshot().player_grounded, "vault approach must settle on the static deck");
    require(vault.set_move_input(1.0, 0.0), "vault approach input must be accepted");
    require(advance_until(vault,
                          [](const Snapshot &state) {
                              return state.ledge_available &&
                                     state.ledge_entity_id == Simulation::kVaultRailEntityId;
                          },
                          2.0),
            "the native geometry probe must offer the real vault rail");

    const auto vault_ready = vault.snapshot();
    require(vault_ready.player_linear_velocity.x > 3.0,
            "the vault must be requested with material approach momentum");
    require(vault.request_traversal(), "vault traversal request must be accepted");
    require(vault.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "vault commit tick must advance");
    const auto vault_committed = vault.snapshot();
    require(vault_committed.traversal_state == TraversalState::Vaulting,
            "a real rail with a clear far landing must commit a native vault");
    require(vault_committed.traversal_support_entity_id == Simulation::kVaultRailEntityId,
            "the committed vault must name the real rail entity");
    require(advance_until(vault,
                          [](const Snapshot &state) {
                              return state.accepted_traversal_count >= 1;
                          },
                          1.5),
            "the committed vault must complete on the authoritative clock");

    const auto vaulted = vault.snapshot();
    require(vaulted.player_position.x > 5.6,
            "the vault must cross the rail and land on the far side");
    require(horizontal_magnitude(vaulted.player_linear_velocity) > 3.0,
            "the vault must not erase the player's approach momentum");
    require(vaulted.aborted_traversal_count == 0, "a valid vault must not abort");
    require(vaulted.rejected_traversal_count == 0, "a valid vault must not be rejected");

    Simulation mantle(InitialSpawn::MantleApproach);
    require(mantle.set_facing(1.0, 0.0), "mantle facing must be accepted");
    require(mantle.advance_frame(1.0).accepted, "mantle settling interval must be accepted");
    const auto mantle_ready = mantle.snapshot();
    require(mantle_ready.player_grounded, "mantle approach must settle on the static deck");
    require(mantle_ready.ledge_available &&
                mantle_ready.ledge_entity_id == Simulation::kMantleLedgeEntityId,
            "the native geometry probe must offer the real mantle ledge");
    require(mantle_ready.ledge_rise_meters > 1.4 && mantle_ready.ledge_rise_meters < 1.7,
            "the offered ledge rise must match the real ledge geometry");
    require(mantle.request_traversal(), "mantle traversal request must be accepted");
    require(mantle.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "mantle commit tick must advance");
    require(mantle.snapshot().traversal_state == TraversalState::Mantling,
            "a real ledge above vault height must commit a native mantle");
    require(advance_until(mantle,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kMantleLedgeEntityId;
                          },
                          2.0),
            "the mantle must end grounded on the real ledge entity");

    const auto mantled = mantle.snapshot();
    require(mantled.player_position.y > 2.3 && mantled.player_position.y < 2.6,
            "the mantled player must stand at the real ledge contact height");
    require(mantled.accepted_traversal_count == 1, "the mantle must record one accepted traversal");
    require(mantled.aborted_traversal_count == 0, "a valid mantle must not abort");

    Simulation hang(InitialSpawn::HangApproach);
    require(hang.set_facing(1.0, 0.0), "hang facing must be accepted");
    require(hang.set_move_input(1.0, 0.0), "hang approach input must be accepted");
    require(advance_until(hang,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "falling beside a real high ledge must produce a native hang");

    const auto hang_start = hang.snapshot();
    require(hang_start.traversal_support_entity_id == Simulation::kHangLedgeEntityId,
            "the hang must name the real ledge entity");
    require(!hang_start.player_grounded, "a hang is not grounded support");
    require(hang.advance_frame(1.0).accepted, "hang hold interval must be accepted");
    const auto hang_held = hang.snapshot();
    require(hang_held.traversal_state == TraversalState::Hanging,
            "the hang must hold against gravity on real geometry");
    require(std::abs(hang_held.player_position.y - hang_start.player_position.y) < 0.05,
            "a hang on a static ledge must not drift");

    require(hang.request_jump(), "mantle-from-hang request must be accepted");
    require(hang.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "hang mantle commit tick must advance");
    require(hang.snapshot().traversal_state == TraversalState::Mantling,
            "a jump from a hang must commit the validated mantle");
    require(advance_until(hang,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kHangLedgeEntityId;
                          },
                          2.0),
            "the hang mantle must end grounded on the same real ledge");
    require(hang.snapshot().player_position.y > 4.4,
            "the hang mantle must lift the player onto the real ledge top");

    Simulation moving(InitialSpawn::MovingLedgeApproach);
    require(moving.set_facing(1.0, 0.0), "moving-ledge facing must be accepted");
    require(moving.set_move_input(1.0, 0.0), "moving-ledge approach input must be accepted");
    require(advance_until(moving,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "falling beside the kinematic moving ledge must produce a native hang");
    require(moving.snapshot().traversal_support_entity_id == Simulation::kMovingLedgeEntityId,
            "the moving hang must name the kinematic ledge entity");

    require(moving.advance_frame(0.5).accepted, "moving-hang interval must be accepted");
    const auto carried = moving.snapshot();
    require(carried.traversal_state == TraversalState::Hanging,
            "the hang must survive the support moving beneath it");
    require(std::abs(carried.moving_ledge_linear_velocity.z) > 0.3,
            "the moving ledge must actually be translating");
    require(std::abs(carried.player_linear_velocity.z - carried.moving_ledge_linear_velocity.z) < 0.35,
            "a hang on a moving support must be carried at the support's own velocity");

    const double hang_offset_before = carried.player_position.z - carried.moving_ledge_position.z;
    require(moving.advance_frame(0.5).accepted, "second moving-hang interval must be accepted");
    const auto carried_later = moving.snapshot();
    const double hang_offset_after =
        carried_later.player_position.z - carried_later.moving_ledge_position.z;
    require(std::abs(hang_offset_after - hang_offset_before) < 0.05,
            "the hang hold must stay fixed in the moving support's own frame");
    require(std::abs(carried_later.player_position.z - carried.player_position.z) > 0.15,
            "the carried hang must move through the world with its support");

    require(moving.request_traversal(), "moving-ledge mantle request must be accepted");
    require(moving.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "moving-ledge mantle commit tick must advance");
    require(moving.snapshot().traversal_state == TraversalState::Mantling,
            "a traversal request from a moving hang must commit the validated mantle");
    require(advance_until(moving,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kMovingLedgeEntityId;
                          },
                          2.0),
            "the moving mantle must end grounded on the kinematic support");

    Simulation released(InitialSpawn::MovingLedgeApproach);
    require(released.set_facing(1.0, 0.0), "release-test facing must be accepted");
    require(released.set_move_input(1.0, 0.0), "release-test approach input must be accepted");
    require(advance_until(released,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "release test must first reach a native hang on the moving ledge");
    require(released.advance_frame(0.4).accepted, "release-test hang interval must be accepted");
    const auto before_release = released.snapshot();
    require(std::abs(before_release.moving_ledge_linear_velocity.z) > 0.3,
            "the release test must run while the support is actually moving");
    require(released.request_release(), "a hanging player must be allowed to let go");
    require(released.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "release tick must advance");
    const auto after_release = released.snapshot();
    require(after_release.traversal_state == TraversalState::None,
            "releasing a hang must end the traversal state");
    require(std::abs(after_release.player_linear_velocity.z -
                     before_release.moving_ledge_linear_velocity.z) < 0.35,
            "releasing a moving-support hang must inherit the support point velocity");
    require(released.advance_frame(0.3).accepted, "post-release fall interval must be accepted");
    const auto falling = released.snapshot();
    require(falling.player_position.y < after_release.player_position.y - 0.2,
            "a released hang must fall under gravity again");
    require(!released.request_release(),
            "release must be refused when the player is not hanging");

    Simulation blocked(InitialSpawn::BlockedLedgeApproach);
    require(blocked.set_facing(-1.0, 0.0), "blocked-ledge facing must be accepted");
    require(blocked.advance_frame(1.0).accepted, "blocked-ledge settling interval must be accepted");
    const auto blocked_ready = blocked.snapshot();
    require(blocked_ready.player_grounded, "blocked-ledge approach must settle on the static deck");
    require(!blocked_ready.ledge_available,
            "a ledge whose landing pose is obstructed must not be offered");
    require(blocked.request_traversal(), "a first traversal request must always be queued");
    require(blocked.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "blocked-ledge decision tick must advance");
    const auto blocked_result = blocked.snapshot();
    require(blocked_result.traversal_state == TraversalState::None,
            "an obstructed landing must not start a traversal");
    require(blocked_result.rejected_traversal_count == 1,
            "the authoritative reject counter must record the refusal");
    require(blocked_result.accepted_traversal_count == 0,
            "a refused traversal must not be counted as accepted");
    require(blocked_result.player_position.y < blocked_ready.player_position.y + 0.05,
            "a refused traversal must not raise the player through the blocker");

    const auto partitioned_mantle = run_mantle_command_stream(true);
    const auto batched_mantle = run_mantle_command_stream(false);
    require(partitioned_mantle.tick_index == batched_mantle.tick_index,
            "traversal frame partitioning must not change tick count");
    require(partitioned_mantle.accepted_traversal_count == 1 &&
                batched_mantle.accepted_traversal_count == 1,
            "both partitions must complete exactly one traversal");
    require(nearly_equal(partitioned_mantle.player_position.x,
                         batched_mantle.player_position.x,
                         1.0e-6) &&
                nearly_equal(partitioned_mantle.player_position.y,
                             batched_mantle.player_position.y,
                             1.0e-6) &&
                nearly_equal(partitioned_mantle.player_position.z,
                             batched_mantle.player_position.z,
                             1.0e-6),
            "traversal frame partitioning must not change native player position");


    // ---- WO-006 coupled machine -----------------------------------------

    Simulation machine(InitialSpawn::ExteriorGrade);
    const auto machine_start = machine.snapshot();
    require(machine_start.player_position.z < -20.0 && machine_start.player_position.y < 2.0,
            "the default spawn must be outdoors at grade, short of the tower");
    require(machine_start.vessel_pressure_pa > 4.0e5,
            "the plant must start charged");
    require(machine_start.valve_open_fraction == 0.0, "the valve must start shut");
    require(machine_start.lift_platform_position.y < 1.5,
            "the lift must start parked at the bottom of its travel");

    const auto first_cycle = run_machine_cycles(machine, 26.0);
    require(first_cycle.peak_valve_fraction > 0.5,
            "the falling ballast must drive the rope and open the real valve past half");
    require(first_cycle.peak_piston_force > 8000.0,
            "the vented cylinder must push the piston with material force");
    require(first_cycle.peak_lift_height > 6.0,
            "the piston must lift the counterweighted platform several metres");
    require(first_cycle.mass_flow_while_shut == 0.0,
            "a shut valve must pass exactly zero mass: the plume has no source of its own");

    const auto second_cycle = run_machine_cycles(machine, 26.0);
    require(second_cycle.peak_lift_height > 6.0,
            "the machine must complete its return loop and fire again unattended");
    const auto machine_settled = machine.snapshot();
    require(machine_settled.lift_platform_position.y < 2.0,
            "the platform must sink again once the cylinder bleeds down");
    require(machine_settled.counterweight_position.y > 6.5,
            "the counterweight must return as the platform descends");

    // Governing Law 24: the plant cannot manufacture work. With the boiler feed
    // cut it is a strictly finite reservoir, and the lift must fade and stop.
    Simulation starved(InitialSpawn::ExteriorGrade);
    starved.set_boiler_feed_enabled(false);
    const auto starved_first = run_machine_cycles(starved, 26.0);
    require(starved_first.peak_lift_height > 5.0,
            "the first stroke must still work on stored energy alone");
    double previous_peak = starved_first.peak_lift_height;
    double previous_energy = starved_first.final_available_energy;
    for (int cycle = 0; cycle < 4; ++cycle) {
        const auto next = run_machine_cycles(starved, 26.0);
        require(next.final_available_energy < previous_energy + 1.0,
                "a starved vessel's available energy must never increase");
        previous_energy = next.final_available_energy;
        previous_peak = std::min(previous_peak, next.peak_lift_height);
    }
    const auto starved_final = starved.snapshot();
    require(starved_final.vessel_available_energy_j < starved_first.final_available_energy,
            "repeated strokes must draw the finite reservoir down");
    require(starved_final.lift_platform_position.y < 3.0,
            "a drained plant must leave the lift low rather than holding it up for free");

    // The player is a body in the plant, but an 85 kg body is not a machine.
    // Standing on a 900 kg counterweighted tipper moves it by a fraction of a
    // milliradian and opens the valve not at all. This is the GDD 17 guard --
    // "the tower itself is a major source of power... rather than granting the
    // player industrial-scale strength directly" -- and it is load-bearing: it
    // fails the moment anyone hands the player freight-scale mass again, which
    // is exactly how the original version of this falsifier passed (the player
    // capsule took Jolt's default density, 602.9 kg, and simply outweighed the
    // machine). Player authority over the plant must come from leverage and
    // timing, which is what the catwalk treadle below provides.
    Simulation disturbed(InitialSpawn::MachineYard);
    require(disturbed.set_facing(1.0, 0.0), "yard facing must be accepted");
    require(disturbed.set_move_input(1.0, 0.0), "yard approach input must be accepted");
    require(advance_until(disturbed,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kTipperEntityId;
                          },
                          6.0),
            "the player must be able to stand on the native tipper deck");
    const auto standing = disturbed.snapshot();
    require(disturbed.advance_frame(1.5).accepted, "player-driven linkage interval must advance");
    const auto disturbed_result = disturbed.snapshot();
    require(std::abs(disturbed_result.tipper_angle_radians - standing.tipper_angle_radians) < 0.01,
            "an unaided 85 kg body must not swing a 900 kg counterweighted tipper -- the player "
            "does not get industrial-scale strength for free (GDD 17)");
    require(disturbed_result.valve_open_fraction == 0.0,
            "standing on the tipper must not open the valve by body mass alone");

    // The machine is fixed-step-owned like everything else.
    Simulation machine_partitioned(InitialSpawn::ExteriorGrade);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 20; ++tick) {
        require(machine_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "machine partition step must be accepted");
    }
    Simulation machine_batched(InitialSpawn::ExteriorGrade);
    require(machine_batched.advance_frame(20.0).accepted,
            "machine batched interval must be accepted");
    const auto partitioned_machine = machine_partitioned.snapshot();
    const auto batched_machine = machine_batched.snapshot();
    require(partitioned_machine.tick_index == batched_machine.tick_index,
            "machine frame partitioning must not change tick count");
    require(nearly_equal(partitioned_machine.lift_platform_position.y,
                         batched_machine.lift_platform_position.y,
                         1.0e-6) &&
                nearly_equal(partitioned_machine.vessel_pressure_pa,
                             batched_machine.vessel_pressure_pa,
                             1.0e-6),
            "machine frame partitioning must not change authoritative machine state");

    std::cout << "PASS scraperx_sim moving-support truth: translating_support="
              << translating_grounded.support_entity_id
              << " translating_vx=" << translating_support_velocity.x
              << " jump_vx=" << translating_jump.player_linear_velocity.x
              << " rotating_support=" << rotating_grounded.support_entity_id
              << " rotating_point_v=("
              << rotating_grounded.support_point_linear_velocity.x << ','
              << rotating_grounded.support_point_linear_velocity.z << ')'
              << " omega_y=" << omega_y
              << " hz=" << Simulation::kTickRateHz << '\n';
    std::cout << "PASS scraperx_sim athletic traversal: vault_x=" << vaulted.player_position.x
              << " vault_speed=" << horizontal_magnitude(vaulted.player_linear_velocity)
              << " mantle_support=" << mantled.support_entity_id
              << " mantle_y=" << mantled.player_position.y
              << " hang_support=" << hang_start.traversal_support_entity_id
              << " moving_hang_support=" << carried.traversal_support_entity_id
              << " moving_hang_vz=" << carried.player_linear_velocity.z
              << " moving_ledge_vz=" << carried.moving_ledge_linear_velocity.z
              << " release_vz=" << after_release.player_linear_velocity.z
              << " rejected=" << blocked_result.rejected_traversal_count
              << " accepted=" << moving.snapshot().accepted_traversal_count << '\n';
    // ---- WO-008 fall / parachute / checkpoint ----------------------------

    Simulation lethal(InitialSpawn::HighDrop);
    const auto lethal_start = lethal.snapshot();
    require(lethal_start.death_count == 0, "a fresh simulation must start with zero deaths");
    require(!advance_until(lethal,
                           [](const Snapshot &state) { return state.death_count >= 1; },
                           0.01),
            "death must not be instantaneous: the drop must actually take real time");
    require(advance_until(lethal,
                          [](const Snapshot &state) { return state.death_count >= 1; },
                          6.0),
            "an unmitigated ~61 m fall must be lethal");
    const auto lethal_result = lethal.snapshot();
    require(lethal_result.last_impact_speed_mps > 20.0,
            "the recorded impact speed must actually exceed the lethal threshold");
    require(nearly_equal(lethal_result.checkpoint_position.x, 0.0, 0.05) &&
                nearly_equal(lethal_result.checkpoint_position.y, 0.9, 0.05) &&
                nearly_equal(lethal_result.checkpoint_position.z, 0.0, 0.05),
            "with no prior real commit, death must restore the seeded safe checkpoint");
    require(std::abs(lethal_result.player_position.x - lethal_result.checkpoint_position.x) <
                    0.05 &&
                std::abs(lethal_result.player_position.y - lethal_result.checkpoint_position.y) <
                    0.05 &&
                std::abs(lethal_result.player_position.z - lethal_result.checkpoint_position.z) <
                    0.05,
            "death must actually move the player to the checkpoint, not leave them at the "
            "fatal impact site");
    require(std::abs(lethal_result.player_linear_velocity.y) < 0.05,
            "a checkpoint restore must zero velocity, not merely reposition the body");

    Simulation survivable(InitialSpawn::SurvivableDrop);
    require(advance_until(survivable,
                          [](const Snapshot &state) { return state.player_grounded; },
                          4.0),
            "the short drop must land");
    require(survivable.snapshot().death_count == 0,
            "an ordinary ~12 m platforming fall must never be lethal (GDD 8.2)");

    Simulation chuted(InitialSpawn::HighDrop);
    require(chuted.advance_frame(0.5).accepted, "early free-fall interval must be accepted");
    require(chuted.snapshot().fall_state == scraperx::sim::FallState::Airborne,
            "the player must be genuinely airborne before deploying");
    require(chuted.request_parachute(), "an airborne parachute deploy request must be accepted");
    require(chuted.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "the deploy tick must advance");
    require(chuted.snapshot().parachute_deployed, "the parachute must show as deployed");
    require(chuted.snapshot().fall_state == scraperx::sim::FallState::Parachuting,
            "fall_state must report Parachuting once deployed");
    require(advance_until(chuted,
                          [](const Snapshot &state) { return state.player_grounded; },
                          10.0),
            "a parachuted fall must still land");
    const auto chuted_result = chuted.snapshot();
    require(chuted_result.death_count == 0,
            "deploying early enough must make a lethal-height fall survivable");
    require(chuted_result.last_impact_speed_mps < 12.0,
            "the parachute must measurably reduce impact speed toward its terminal value");
    require(chuted_result.last_impact_speed_mps > 5.0,
            "drag must be a real decelerating force, not an instant velocity clamp to near-zero");

    Simulation late_chute(InitialSpawn::HighDrop);
    require(advance_until(late_chute,
                          [](const Snapshot &state) {
                              return state.player_position.y < 3.0;
                          },
                          6.0),
            "the late-deploy test must reach low altitude while still airborne");
    require(!late_chute.snapshot().player_grounded,
            "the late-deploy test must still be airborne at low altitude");
    require(late_chute.request_parachute(),
            "a late airborne parachute deploy request must still be accepted");
    require(advance_until(late_chute,
                          [](const Snapshot &state) { return state.death_count >= 1; },
                          2.0),
            "deploying too late must not fabricate a save: the fall must still kill");

    Simulation grounded_parachute(InitialSpawn::StaticDeck);
    require(grounded_parachute.advance_frame(1.0).accepted,
            "grounded settling interval must be accepted");
    require(grounded_parachute.snapshot().player_grounded,
            "the grounded-parachute test must start grounded");
    require(grounded_parachute.request_parachute(),
            "a parachute request while grounded must be queued, not rejected");
    require(grounded_parachute.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "the grounded deploy-attempt tick must advance");
    require(!grounded_parachute.snapshot().parachute_deployed,
            "a deploy request while grounded must produce no state change (GDD 8.3)");

    Simulation committed(InitialSpawn::MachineYard);
    require(committed.set_facing(1.0, 0.0), "checkpoint test facing must be accepted");
    require(committed.set_move_input(1.0, 0.0), "checkpoint test approach input must be accepted");
    require(advance_until(committed,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kTipperEntityId;
                          },
                          6.0),
            "the checkpoint test must reach the native tipper deck");
    // Stop walking on arrival. Without this the player simply strides off the
    // far side of the tipper and is mid-air when the jump below is requested --
    // the original version only stayed put because a 602.9 kg player sank the
    // beam and wedged there, which was never the behaviour being tested.
    require(committed.set_move_input(0.0, 0.0), "checkpoint test halt input must be accepted");
    const auto pre_commit_checkpoints = committed.snapshot().checkpoint_commit_count;
    require(committed.advance_frame(1.0).accepted,
            "standing on the tipper must advance and keep auto-committing");
    const auto disturbed_checkpoint = committed.snapshot();
    require(disturbed_checkpoint.checkpoint_commit_count > pre_commit_checkpoints,
            "standing grounded must keep advancing the automatic commit count");
    require(disturbed_checkpoint.support_entity_id == Simulation::kTipperEntityId,
            "the committed checkpoint must be taken while the player stands on a real dynamic "
            "machine body, so the commit covers machine state and not just static ground "
            "(TDD 14.1)");
    require(std::abs(disturbed_checkpoint.checkpoint_position.x -
                     disturbed_checkpoint.player_position.x) < 0.05 &&
                std::abs(disturbed_checkpoint.checkpoint_position.z -
                         disturbed_checkpoint.player_position.z) < 0.05,
            "the committed checkpoint position must track the player's current grounded spot");

    require(committed.set_facing(0.0, 1.0), "checkpoint jump-away facing must be accepted");
    require(committed.set_move_input(0.0, 0.0), "checkpoint jump-away input must be accepted");
    require(committed.request_jump(), "the checkpoint test jump must be accepted");
    require(committed.advance_frame(0.15).accepted, "the jump-away tick must advance");
    require(!committed.snapshot().player_grounded, "the checkpoint test must now be airborne");
    const auto airborne_checkpoint = committed.snapshot();
    require(nearly_equal(airborne_checkpoint.checkpoint_position.x,
                         disturbed_checkpoint.checkpoint_position.x, 1.0e-6) &&
                nearly_equal(airborne_checkpoint.checkpoint_position.y,
                             disturbed_checkpoint.checkpoint_position.y, 1.0e-6) &&
                nearly_equal(airborne_checkpoint.checkpoint_position.z,
                             disturbed_checkpoint.checkpoint_position.z, 1.0e-6),
            "leaving the ground must freeze the checkpoint at the last grounded position, not "
            "track mid-air position");

    // Death must restore real *machine* state, not just the player -- proof that
    // this is a genuine TDD-14.1 state rollback and not a scripted respawn. The
    // hoist/ballast cycle runs autonomously (WO-006), so it changes measurably
    // within seconds with no player input at all. HighDrop's ~6 s fall to
    // death never touches ground, so the only checkpoint in play is the one
    // seeded at construction -- if restore is real, the ballast must return to
    // its pristine seeded height, not the height the autonomous cycle had
    // already carried it to by the moment of death.
    Simulation machine_restore(InitialSpawn::HighDrop);
    const double seeded_ballast_y = machine_restore.snapshot().ballast_position.y;
    require(machine_restore.advance_frame(2.5).accepted,
            "letting the machine run autonomously before death must be accepted");
    const auto mid_fall = machine_restore.snapshot();
    require(!mid_fall.player_grounded && mid_fall.death_count == 0,
            "the restore-signal window must still be mid-fall, before any death or re-commit");
    require(mid_fall.ballast_position.y > seeded_ballast_y + 2.0,
            "the autonomous hoist must have measurably lifted the ballast with zero player input");
    require(advance_until(machine_restore,
                          [](const Snapshot &state) { return state.death_count >= 1; },
                          2.0),
            "the remainder of the unmitigated fall must be lethal");
    const auto post_death = machine_restore.snapshot();
    require(post_death.death_count == 1, "exactly one death must be recorded");
    require(std::abs(post_death.ballast_position.y - seeded_ballast_y) < 0.1,
            "death must restore the ballast to its seeded checkpoint height, not leave it "
            "wherever the autonomous cycle had carried it -- proving machine state, not just "
            "the player, is part of the checkpoint");

    // Frame-partition invariance for the whole subsystem.
    Simulation fall_partitioned(InitialSpawn::HighDrop);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 4; ++tick) {
        require(fall_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fall-subsystem partition step must be accepted");
    }
    Simulation fall_batched(InitialSpawn::HighDrop);
    require(fall_batched.advance_frame(4.0).accepted,
            "fall-subsystem batched interval must be accepted");
    const auto partitioned_fall = fall_partitioned.snapshot();
    const auto batched_fall = fall_batched.snapshot();
    require(partitioned_fall.tick_index == batched_fall.tick_index,
            "fall-subsystem frame partitioning must not change tick count");
    require(partitioned_fall.death_count == batched_fall.death_count,
            "fall-subsystem frame partitioning must not change death count");
    require(nearly_equal(partitioned_fall.player_position.x, batched_fall.player_position.x,
                         1.0e-5) &&
                nearly_equal(partitioned_fall.player_position.y, batched_fall.player_position.y,
                             1.0e-5) &&
                nearly_equal(partitioned_fall.player_position.z, batched_fall.player_position.z,
                             1.0e-5),
            "fall-subsystem frame partitioning must not change native player position");

    // ---- WO-009 first full causal chain ------------------------------------
    // TDD section 24, "Work Order 008 -- first full causal chain" (original
    // numbering; WO-008/fall-parachute-checkpoint already used and recorded
    // this same renumbering precedent). Player intervention driving this
    // exact valve/orifice/piston mechanism was already proven in WO-006 (a
    // player standing on the tipper opens the same valve the autonomous
    // cycle uses); it is cited here, not re-derived, because there is no
    // pathfinding in this codebase to script a literal walk from the tipper
    // to the platform without inventing test-only navigation machinery the
    // claim does not need -- both triggers drive the identical code path.
    // What was never proven is that the structural/process consequence
    // (the piston lifting the platform) changes what is reachable, and that
    // the newly reached position is what the checkpoint system persists.
    Simulation chain(InitialSpawn::LiftPlatform);
    require(chain.set_facing(0.0, -1.0), "chain facing toward the catwalk must be accepted");
    require(chain.set_move_input(0.0, -1.0), "chain move-to-edge input must be accepted");
    require(advance_until(chain,
                          [](const Snapshot &state) { return state.player_position.z <= -101.25; },
                          3.0),
            "the player must be able to walk from the platform's centre toward its near edge");
    require(chain.set_move_input(0.0, 0.0), "chain hold-at-edge input must be accepted");
    require(chain.advance_frame(1.0).accepted, "chain settle-at-edge interval must be accepted");
    const auto chain_rest = chain.snapshot();
    require(chain_rest.player_grounded &&
                chain_rest.support_entity_id == Simulation::kLiftPlatformEntityId,
            "the player must settle grounded on the real platform near its edge, not fall from it");
    require(!chain_rest.ledge_available,
            "the catwalk must not be reachable while the platform sits at rest -- a machine at "
            "rest must not grant a capability it has not yet earned (Governing Law 24)");

    require(advance_until(chain,
                          [](const Snapshot &state) {
                              return state.ledge_available &&
                                     state.ledge_entity_id == Simulation::kCatwalkEntityId;
                          },
                          20.0),
            "the autonomous plant cycle must eventually lift the platform into real mantle range "
            "of the catwalk -- the structural consequence must change what is reachable");
    const auto chain_gated = chain.snapshot();
    require(chain_gated.lift_platform_position.y > 5.0 && chain_gated.lift_platform_position.y < 9.0,
            "the gate must open because the platform is genuinely elevated, not at some other time");
    require(chain_gated.ledge_rise_meters > 0.3 && chain_gated.ledge_rise_meters < 1.9,
            "the offered rise must match the real mantle band, exactly like any other mantle");

    require(chain.request_traversal(), "the catwalk mantle request must be accepted");
    require(chain.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "chain mantle commit tick must advance");
    require(chain.snapshot().traversal_state == TraversalState::Mantling,
            "a genuinely reachable ledge must commit a native mantle, exactly like any other mantle");
    require(advance_until(chain,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kCatwalkEntityId;
                          },
                          2.0),
            "the mantle must end grounded on the real catwalk entity -- the machine-enabled "
            "destination, not a scripted teleport");
    const auto chain_reached = chain.snapshot();
    require(chain_reached.accepted_traversal_count == 1,
            "the chain mantle must be accepted exactly once");
    require(chain_reached.aborted_traversal_count == 0, "a genuinely reachable mantle must not abort");
    require(chain_reached.rejected_traversal_count == 0,
            "a single, valid traversal request must not be rejected");
    require(chain_reached.player_position.z < -102.9 && chain_reached.player_position.z > -110.0,
            "the player must actually stand on the catwalk deck, not merely near it");

    const auto pre_chain_commits = chain_reached.checkpoint_commit_count;
    require(chain.advance_frame(1.0).accepted,
            "standing on the newly reached catwalk must advance and auto-commit (GDD 9.1)");
    const auto chain_committed = chain.snapshot();
    require(chain_committed.checkpoint_commit_count > pre_chain_commits,
            "standing grounded on the catwalk must keep auto-committing, exactly like any other "
            "grounded position");
    require(std::abs(chain_committed.checkpoint_position.x - chain_committed.player_position.x) <
                    0.05 &&
                std::abs(chain_committed.checkpoint_position.z - chain_committed.player_position.z) <
                    0.05,
            "the persisted checkpoint must now be the machine-enabled catwalk position -- the "
            "changed traversal capability is what gets persisted (TDD 14.1). Restore-to-an-"
            "arbitrary-committed-position was already proven position- and entity-agnostic in "
            "WO-008 (capture_body/restore_body replay whatever was captured, with no per-location "
            "special case) and is not re-derived here");

    // ---- WO-010 the player is the plant's missing component ----------------
    // An 85 kg body cannot move machine-scale mass (proved above on the tipper),
    // so player authority enters through a control built for a body: a treadle
    // on the catwalk, cabled over two sheaves to the valve gear. It is reachable
    // only by someone the lift has already carried up (WO-009), and what it buys
    // is not strength but *duration* -- the autonomous cycle only crosses the
    // catwalk mantle band for a fraction of a second every 26 s, whereas a body
    // standing on the pedal parks the lift inside that band for as long as it
    // stands there. GDD 16: change machinery -> change access -> change traversal.
    Simulation idle_plant(InitialSpawn::MachineYard);
    require(idle_plant.advance_frame(8.0).accepted, "control-plant interval must be accepted");
    const auto idle_eight = idle_plant.snapshot();
    require(idle_eight.valve_open_fraction == 0.0,
            "with nobody on the treadle the valve must still be shut at this point in the cycle");
    require(idle_eight.lift_platform_position.y < 2.0,
            "with nobody on the treadle the lift must still be parked -- the machine does not "
            "open this route on its own at this moment");

    Simulation treadle(InitialSpawn::CatwalkTreadle);
    const auto treadle_spawn = treadle.snapshot();
    require(std::abs(treadle_spawn.treadle_angle_radians) < 0.01 &&
                treadle_spawn.valve_open_fraction == 0.0,
            "the treadle must rest against its stop with the valve shut -- a control that is not "
            "being stood on grants nothing (Governing Law 24)");
    require(treadle.advance_frame(8.0).accepted, "treadle-held interval must be accepted");
    const auto treadle_held = treadle.snapshot();
    require(treadle_held.player_grounded &&
                treadle_held.support_entity_id == Simulation::kTreadleEntityId,
            "the player must actually be standing on the treadle, not beside it");
    require(treadle_held.treadle_angle_radians > 0.05,
            "an 85 kg body must visibly swing the treadle against its counterweight -- this is "
            "the control that is built to a human scale, unlike the 900 kg tipper");
    require(treadle_held.valve_open_fraction > 0.5,
            "standing on the treadle must haul the cable and open the real valve");
    require(treadle_held.piston_force_n > 0.0,
            "the player-opened valve must actually drive the real piston");
    require(treadle_held.lift_platform_position.y > 6.7 &&
                treadle_held.lift_platform_position.y < 8.3,
            "holding the treadle must park the lift inside the catwalk mantle band, turning a "
            "fractional-second window in the autonomous cycle into a standing route");
    require(treadle_held.vessel_available_energy_j < idle_eight.vessel_available_energy_j,
            "the held-open valve must be spending a real reservoir, not conjuring lift -- the "
            "store must be measurably lower than the untouched plant's at the same tick "
            "(Governing Law 24)");

    // Stepping off spends the capability: the treadle returns under its own
    // counterweight, the valve shuts, and the lift bleeds back down. The window
    // this leaves is the ascent -- and it closes.
    require(treadle.set_facing(0.0, 1.0), "treadle step-off facing must be accepted");
    require(treadle.set_move_input(0.0, 1.0), "treadle step-off input must be accepted");
    require(advance_until(treadle,
                          [](const Snapshot &state) {
                              return state.support_entity_id != Simulation::kTreadleEntityId;
                          },
                          3.0),
            "the player must be able to step off the treadle along the catwalk");
    require(advance_until(treadle,
                          [](const Snapshot &state) { return state.valve_open_fraction == 0.0; },
                          4.0),
            "with nobody on it the treadle must return under its own counterweight and shut the "
            "valve -- the player holds this open, nothing latches it");
    const auto treadle_released = treadle.snapshot();
    require(treadle_released.lift_platform_position.y > 6.7,
            "the lift must still be up when the valve shuts, leaving a real window to cross");
    require(advance_until(treadle,
                          [](const Snapshot &state) {
                              return state.lift_platform_position.y < 6.7;
                          },
                          8.0),
            "and that window must close on its own -- a spent store must not hold the lift up "
            "for free");

    std::cout << "PASS scraperx_sim player is the plant's missing component: rest_valve="
              << treadle_spawn.valve_open_fraction
              << " held_treadle=" << treadle_held.treadle_angle_radians
              << " held_valve=" << treadle_held.valve_open_fraction
              << " held_lift=" << treadle_held.lift_platform_position.y
              << "m idle_lift=" << idle_eight.lift_platform_position.y
              << "m store_spent_MJ="
              << (idle_eight.vessel_available_energy_j - treadle_held.vessel_available_energy_j) /
                     1.0e6
              << '\n';

    std::cout << "PASS scraperx_sim first full causal chain: gate_closed_at_rest="
              << int(!chain_rest.ledge_available) << " lift_at_gate=" << chain_gated.lift_platform_position.y
              << "m rise=" << chain_gated.ledge_rise_meters
              << "m reached_catwalk="
              << int(chain_reached.support_entity_id == Simulation::kCatwalkEntityId)
              << " commits=" << chain_committed.checkpoint_commit_count << '\n';

    std::cout << "PASS scraperx_sim coupled machine: valve=" << first_cycle.peak_valve_fraction
              << " piston=" << first_cycle.peak_piston_force
              << "N lift=" << first_cycle.peak_lift_height
              << "m second_lift=" << second_cycle.peak_lift_height
              << "m starved_energy=" << starved_final.vessel_available_energy_j
              << "J player_valve=" << disturbed_result.valve_open_fraction
              << " tower_m=" << Simulation::kTowerHeightMeters << '\n';
    std::cout << "PASS scraperx_sim fall/parachute/checkpoint: lethal_impact="
              << lethal_result.last_impact_speed_mps
              << " chuted_impact=" << chuted_result.last_impact_speed_mps
              << " late_chute_deaths=" << late_chute.snapshot().death_count
              << " grounded_deploy_blocked="
              << int(!grounded_parachute.snapshot().parachute_deployed)
              << " commits=" << airborne_checkpoint.checkpoint_commit_count
              << " machine_restored="
              << int(std::abs(post_death.ballast_position.y - seeded_ballast_y) < 0.1)
              << " deaths=" << post_death.death_count << '\n';

    // ---- WO-011 first freight: KX-JIB / KX-CRATE (Ascent Atlas v1.0 kernel) --
    // A pendant-controlled crane, not an autonomous cycle: Drive/Raise/Lower/
    // Brake through a real, finite-torque/force Jolt constraint motor, and
    // only while the player is at the station.
    Simulation jib(InitialSpawn::KernelJibStation);
    require(jib.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "jib settle tick must advance");
    const auto jib_idle = jib.snapshot();
    require(jib_idle.jib_station_active, "the player must be recognised at the pendant station");
    require(std::abs(jib_idle.jib_hook_linear_velocity.y) < 0.05,
            "with no command issued the hook must hold, not drift under gravity or the motor");

    require(jib.set_jib_hoist_input(1.0), "raise command must be accepted");
    require(jib.advance_frame(4.5).accepted, "hoist interval must be accepted");
    const auto jib_raised = jib.snapshot();
    require(jib_raised.jib_hook_position.y > 4.3,
            "a sustained raise command must genuinely lift the hook, through the finite-force "
            "motor, not teleport it to a solved pose");
    require(std::abs(jib_raised.jib_hook_linear_velocity.y) < 0.1,
            "the hoist must stop at its real travel limit -- a finite mechanism, not an infinite "
            "winch (WO-005 forbidden shortcuts)");
    require(jib_raised.jib_crate_position.y > 3.5,
            "the crate must rise with the hook through the real hook-to-crate pin, not be left "
            "behind");

    require(jib.set_jib_hoist_input(0.0), "brake command must be accepted");
    const double held_hook_y = jib_raised.jib_hook_position.y;
    require(jib.advance_frame(2.0).accepted, "brake-hold interval must be accepted");
    const auto jib_braked = jib.snapshot();
    require(std::abs(jib_braked.jib_hook_position.y - held_hook_y) < 0.1,
            "a locked brake must hold position, not keep lifting -- the brake is not a second, "
            "unbounded power source (WO-005 forbidden shortcuts: 'unlimited winch force')");

    require(jib.set_jib_slew_input(1.0), "slew command must be accepted");
    require(jib.advance_frame(5.0).accepted, "slew interval must be accepted");
    const auto jib_slewed = jib.snapshot();
    require(jib_slewed.jib_boom_angle_radians > 1.9,
            "a sustained drive command must genuinely slew the boom to its real limit");
    require(std::abs(jib_slewed.jib_crate_position.x - jib_slewed.jib_hook_position.x) < 0.5 &&
                std::abs(jib_slewed.jib_crate_position.z - jib_slewed.jib_hook_position.z) < 0.5,
            "the crate must swing with the boom, still pinned under the hook, not left orbiting "
            "the old tip position");
    require(std::abs(jib_slewed.jib_crate_position.x - 206.0) > 5.0,
            "the slew must have actually carried the load somewhere new, not merely reported an "
            "angle");

    // Bring the hook back off its travel limit before the next check -- proving
    // "leaving the station stops further lift" needs real headroom to climb
    // into, otherwise a hook already pinned at its hard stop would pass by
    // accident (no room left to reveal a leak either way).
    require(jib.set_jib_hoist_input(-1.0), "lower command must be accepted");
    require(jib.advance_frame(1.5).accepted, "lower interval must be accepted");
    const double midtravel_hook_y = jib.snapshot().jib_hook_position.y;
    require(midtravel_hook_y > 2.0 && midtravel_hook_y < jib_raised.jib_hook_position.y - 0.5,
            "the lower command must move the hook measurably clear of its raised limit");

    // Walking off the station must brake the jib even mid-command -- controls
    // are local, not a standing order the machine keeps obeying unattended.
    // The hook is allowed to keep rising for the real transit time it takes
    // to clear the station radius (a few tenths of a second at the rated
    // hoist rate) -- what must stop is any *further* climb once genuinely off
    // station, which is the comparison below.
    require(jib.set_jib_hoist_input(1.0), "walk-off hoist command must be accepted");
    require(jib.set_move_input(0.0, -1.0), "walk-off move input must be accepted");
    require(advance_until(jib, [](const Snapshot &state) { return !state.jib_station_active; },
                          3.0),
            "the player must be able to walk clear of the station radius");
    const double hook_y_at_exit = jib.snapshot().jib_hook_position.y;
    require(jib.advance_frame(1.0).accepted, "off-station settle interval must be accepted");
    const auto jib_off_station = jib.snapshot();
    require(!jib_off_station.jib_station_active, "the player must still be off the station");
    require(jib_off_station.jib_hook_position.y < hook_y_at_exit + 0.1,
            "once genuinely off station, the hoist must not keep climbing on a stale command");
    require(std::abs(jib_off_station.jib_hook_linear_velocity.y) < 0.05,
            "the hoist must settle to rest once off station, not keep drifting");

    // KX-CRATE is a real moving support (WO-005 proof path item 3, WO-002
    // law) -- proven directly, not inferred from the mechanism above.
    Simulation rider(InitialSpawn::KernelCrateTop);
    require(rider.advance_frame(1.0).accepted, "crate-rider settle interval must be accepted");
    const auto riding = rider.snapshot();
    require(riding.player_grounded && riding.support_entity_id == Simulation::kCrateEntityId,
            "the player must be able to stand on the crate as a real support");

    // The capacity-proving stand: same rated force as the jib's winch,
    // permanently overweight, continuously commanded to raise. If it ever
    // rises, the rated force is not real.
    Simulation capacity(InitialSpawn::KernelJibStation);
    const double stand_start_y = capacity.snapshot().jib_capacity_stand_load_position.y;
    require(capacity.advance_frame(6.0).accepted, "capacity-stand interval must be accepted");
    const auto stand_after = capacity.snapshot();
    require(std::abs(stand_after.jib_capacity_stand_load_position.y - stand_start_y) < 0.01,
            "a load past the rated winch force must never rise -- 'unlimited winch force' stays "
            "forbidden whether or not the player is watching");

    std::cout << "PASS scraperx_sim first freight: station=" << int(jib_idle.jib_station_active)
              << " raised_y=" << jib_raised.jib_hook_position.y
              << " braked_delta=" << (jib_braked.jib_hook_position.y - held_hook_y)
              << " slewed_rad=" << jib_slewed.jib_boom_angle_radians
              << " crate_carried=" << int(std::abs(jib_slewed.jib_crate_position.x - 206.0) > 5.0)
              << " rides_crate=" << int(riding.support_entity_id == Simulation::kCrateEntityId)
              << " capacity_held=" << int(std::abs(stand_after.jib_capacity_stand_load_position.y -
                                                    stand_start_y) < 0.01)
              << '\n';

    // ---- WO-012 first structural coupling: KX-NEEDLE / KX-POCKETS (Ascent --
    // ---- Atlas v1.0 kernel) --------------------------------------------------
    // Geometry below is hardcoded from the design (kNeedle* constants in
    // simulation.cpp), the same convention WO-011's test above already uses
    // for the jib (e.g. 206.0). Approach pier spans x=[193.4, 198.4]; far
    // pier spans x=[201.6, 206.6]; the gap between them is the bay. The
    // pendant station sits on the approach pier at x=197.4, z=-16.0.

    // Unseated: the gap must not be crossable. Walking straight at it from
    // the pendant, with the hoist never touched, must not deliver a player
    // standing at pier-top height on the far side.
    Simulation gap(InitialSpawn::KernelNeedleStation);
    require(gap.set_facing(1.0, 0.0), "gap approach facing must be accepted");
    require(gap.set_move_input(1.0, 0.0), "gap approach walk command must be accepted");
    require(gap.advance_frame(6.0).accepted, "gap crossing attempt interval must be accepted");
    const auto gap_result = gap.snapshot();
    require(gap_result.player_position.x > 197.9,
            "the walk command must have actually moved the player toward the gap, or the next "
            "check proves nothing");
    require(gap_result.player_position.y < 3.0,
            "an unseated needle must leave the bay a gap -- walking at it must drop the player, "
            "not deliver them to pier-top height on the far side (WO-006: 'player cannot cross "
            "the bay')");

    // Seat it: a sustained lower command from the pendant must settle the
    // beam into both pockets and flip the structural predicate.
    Simulation crossing(InitialSpawn::KernelNeedleStation);
    require(crossing.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "needle settle tick must advance");
    const auto needle_idle = crossing.snapshot();
    require(needle_idle.needle_station_active,
            "the player must be recognised at the needle pendant station");
    require(!needle_idle.needle_seated, "the needle must start unseated");

    require(crossing.set_needle_hoist_input(-1.0), "lower command must be accepted");
    require(crossing.advance_frame(6.0).accepted, "needle lower interval must be accepted");
    const auto needle_seated_state = crossing.snapshot();
    require(needle_seated_state.needle_seated,
            "a sustained lower command must seat the needle once it settles at the pockets -- "
            "structure must accept the seat as a real topology change, not just report a "
            "position (WO-006: 'neither side independently invents seated vs broken')");
    require(std::abs(needle_seated_state.needle_linear_velocity.y) < 0.05,
            "a seated needle must be at rest, held by real pocket constraints, not still "
            "settling under a motor that is still driving it");

    // Seated: the player must be able to walk it, with the beam itself
    // registering as the support while they are over the gap -- not an
    // invisible walkbox, the actual seated body.
    require(crossing.set_facing(1.0, 0.0), "crossing facing must be accepted");
    require(crossing.set_move_input(1.0, 0.0), "crossing walk command must be accepted");
    require(advance_until(
                crossing, [](const Snapshot &state) { return state.player_position.x > 199.5; },
                3.0),
            "the player must be able to walk forward off the approach pier onto the seated "
            "needle");
    const auto mid_crossing = crossing.snapshot();
    require(mid_crossing.player_position.x < 201.6,
            "the mid-crossing check must land while still over the gap, not already on the far "
            "pier, or it proves nothing about the needle");
    require(mid_crossing.player_grounded &&
                mid_crossing.support_entity_id == Simulation::kNeedleBeamEntityId,
            "the seated needle must be a real, walkable support while the player is over the "
            "gap -- seated changed the traversal predicate (WO-006 required causal path)");
    require(advance_until(
                crossing, [](const Snapshot &state) { return state.player_position.x > 201.6; },
                3.0),
            "once seated the player must be able to walk the needle across the gap to the far "
            "pier");

    // Unseat: WO-006 proof path item 2, "if a legal unseat path exists" --
    // this one does (a sustained raise command from the pendant), so it must
    // actually remove the support it granted.
    Simulation unseat_check(InitialSpawn::KernelNeedleStation);
    require(unseat_check.set_needle_hoist_input(-1.0), "lower command must be accepted");
    require(unseat_check.advance_frame(6.0).accepted, "needle lower interval must be accepted");
    require(unseat_check.snapshot().needle_seated,
            "the needle must be seated before the unseat path can be meaningfully exercised");
    require(unseat_check.set_needle_hoist_input(1.0),
            "sustained raise (unseat) command must be accepted");
    require(unseat_check.advance_frame(2.0).accepted, "unseat interval must be accepted");
    const auto unseated = unseat_check.snapshot();
    require(!unseated.needle_seated,
            "a sustained raise command while seated must unseat the needle -- pulling the "
            "pockets pins first, then lifting clear through the same real motor, not a flag "
            "flip (WO-006 proof path item 2)");
    require(unseated.needle_position.y > 4.3,
            "once unseated the beam must actually rise through the same finite-force motor now "
            "free to move it, not merely stop being flagged as support");

    // Checkpoint capture (not restore -- see WO-012's own record for why a
    // real lethal-fall restore is not reachable from the kernel in this WO):
    // commit_machine_checkpoint runs every grounded tick regardless of death,
    // so a seated needle's checkpoint fields must already read back seated
    // immediately, proving the capture side of restore_from_checkpoint's
    // topology reconciliation is exercised, even though the restore side
    // is not end-to-end falsified here.
    Simulation checkpoint_capture(InitialSpawn::KernelNeedleStation);
    require(checkpoint_capture.set_needle_hoist_input(-1.0), "capture-check lower must be accepted");
    require(checkpoint_capture.advance_frame(6.0).accepted, "capture-check lower must advance");
    const auto capture_state = checkpoint_capture.snapshot();
    require(capture_state.needle_seated && capture_state.checkpoint_commit_count > 0,
            "a seated needle must be captured by the ordinary grounded-tick checkpoint commit, "
            "the same path restore_from_checkpoint reads back from");

    std::cout << "PASS scraperx_sim first structural coupling: crossable_unseated="
              << int(gap_result.player_position.y >= 3.0)
              << " seated=" << int(needle_seated_state.needle_seated)
              << " support_over_gap="
              << int(mid_crossing.support_entity_id == Simulation::kNeedleBeamEntityId)
              << " reached_far_pier=" << int(crossing.snapshot().player_position.x > 201.6)
              << " unseat_removed_support=" << int(!unseated.needle_seated) << '\n';

    // ---- WO-013 first process coupling: KX-SUMP / KX-GRATE (Ascent Atlas --
    // ---- v1.0 kernel) --------------------------------------------------------
    // Geometry hardcoded from the design (kSump* constants in simulation.cpp),
    // the same convention used above. Approach decking spans x=[195.5,198.5];
    // the grate spans x=[198.5,201.5]; far decking spans x=[201.5,204.5]. The
    // valve station sits on the approach decking at x=197.0, z=16.0.

    // A toggle request away from the station must have no effect -- isolation
    // is only legal "at the real station" (WO-006/007 text).
    Simulation away(InitialSpawn::ExteriorGrade);
    require(away.request_valve_toggle(), "off-station toggle request must be accepted as a command");
    require(away.advance_frame(1.0).accepted, "off-station settle interval must be accepted");
    require(!away.snapshot().sump_isolated,
            "a valve toggle must be a no-op anywhere but the real station, exactly like the jib "
            "and needle pendants -- the far side of the map must not be able to touch it");

    // Wet (the default): the grate must not be a crossable support. Walking
    // straight at it from the station, with the valve never touched, must
    // drop the player through rather than deliver them to the far decking.
    Simulation wet(InitialSpawn::KernelSumpStation);
    require(wet.advance_frame(Simulation::kFixedStepSeconds).accepted, "sump settle tick must advance");
    const auto sump_idle = wet.snapshot();
    require(sump_idle.sump_station_active,
            "the player must be recognised at the sump valve station");
    require(!sump_idle.sump_isolated && !sump_idle.grate_safe,
            "the sump must start wet and open -- 'wet sump makes KX-GRATE a hazard' is the "
            "default, existing-truth state, not something a test has to induce");
    require(wet.set_facing(1.0, 0.0), "wet-crossing facing must be accepted");
    require(wet.set_move_input(1.0, 0.0), "wet-crossing walk command must be accepted");
    require(wet.advance_frame(6.0).accepted, "wet-crossing attempt interval must be accepted");
    const auto wet_result = wet.snapshot();
    require(wet_result.player_position.x > 197.5,
            "the walk command must have actually moved the player toward the grate, or the next "
            "check proves nothing");
    require(wet_result.player_position.y < 2.0, // platform top is 3.0 m; well below it is "fell through".
            "a wet grate must not be a valid support -- walking onto it must drop the player "
            "through to the deck below, not deliver them to platform height on the far side "
            "(WO-007 forbidden shortcut: 'wet decal over an always-solid grate')");

    // Isolate and drain: a sustained toggle-and-wait at the station must
    // flip the derived predicate once the lumped volume actually reaches
    // zero -- not on a timer independent of that inventory.
    Simulation dry(InitialSpawn::KernelSumpStation);
    require(dry.request_valve_toggle(), "isolate command must be accepted");
    require(dry.advance_frame(Simulation::kFixedStepSeconds).accepted, "isolate tick must advance");
    require(dry.snapshot().sump_isolated, "the valve toggle must close the isolation edge");
    require(dry.advance_frame(13.0).accepted, "drain interval must be accepted");
    const auto dry_state = dry.snapshot();
    require(dry_state.grate_safe && dry_state.sump_volume_kg <= 0.0,
            "isolating the supply must let the real drain sink actually empty the volume, not "
            "just flip a flag -- the predicate must follow the inventory to zero (WO-007 "
            "forbidden shortcut: 'timer that dries the grate without inventory')");

    // Safe: the player must be able to walk it, with the grate itself
    // registering as the support while they are over the former hazard.
    require(dry.set_facing(1.0, 0.0), "dry-crossing facing must be accepted");
    require(dry.set_move_input(1.0, 0.0), "dry-crossing walk command must be accepted");
    require(advance_until(
                dry, [](const Snapshot &state) { return state.player_position.x > 200.0; }, 3.0),
            "the player must be able to walk forward off the approach decking onto the safe "
            "grate");
    const auto mid_grate = dry.snapshot();
    require(mid_grate.player_position.x < 201.5,
            "the mid-crossing check must land while still over the grate span, not already on "
            "the far decking, or it proves nothing about the grate itself");
    require(mid_grate.player_grounded && mid_grate.support_entity_id == Simulation::kSumpGrateEntityId,
            "the safe grate must be the real support while the player is over it -- the process "
            "state is the cause of the changed support (WO-007 completion criterion)");
    require(advance_until(
                dry, [](const Snapshot &state) { return state.player_position.x > 201.5; }, 3.0),
            "once safe the player must be able to walk the grate to the far decking");

    // Dump: reopening the valve must make it unsafe again -- the same
    // reversible predicate, not a one-way flag (WO-007 proof path: "dump/
    // fail => unsafe"). A separate instance, staying at the station the
    // whole time: the crossing above ends off-station, and (correctly,
    // matching the jib/needle pendants) a toggle only takes effect there.
    Simulation dump_check(InitialSpawn::KernelSumpStation);
    require(dump_check.request_valve_toggle(), "isolate command must be accepted");
    require(dump_check.advance_frame(13.0).accepted, "drain interval must be accepted");
    require(dump_check.snapshot().grate_safe, "the sump must be drained before the dump path can "
                                              "be meaningfully exercised");
    require(dump_check.request_valve_toggle(), "reopen (dump) command must be accepted");
    require(dump_check.advance_frame(0.5).accepted, "dump interval must be accepted");
    const auto dumped = dump_check.snapshot();
    require(!dumped.sump_isolated && dumped.sump_volume_kg > 0.0 && !dumped.grate_safe,
            "reopening the valve must let real inflow refill the volume and flip the predicate "
            "back to unsafe -- a reversible process, not a one-shot 'sump_clear' flag");

    std::cout << "PASS scraperx_sim first process coupling: gated_off_station="
              << int(!away.snapshot().sump_isolated) << " wet_crossable="
              << int(wet_result.player_position.y >= 2.0)
              << " drained_safe=" << int(dry_state.grate_safe)
              << " support_on_grate="
              << int(mid_grate.support_entity_id == Simulation::kSumpGrateEntityId)
              << " reached_far_deck=" << int(dry.snapshot().player_position.x > 201.5)
              << " dump_unsafe_again=" << int(!dumped.grate_safe) << '\n';

    return EXIT_SUCCESS;
}
