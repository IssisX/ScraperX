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

    // The player is a body in the plant, not an audience: standing on the tipper
    // is enough to work the same linkage the ballast works.
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
    require(disturbed_result.tipper_angle_radians < standing.tipper_angle_radians - 0.05,
            "the player's own weight must rotate the tipper off its rest stop");
    require(disturbed_result.valve_open_fraction > 0.0,
            "a player standing on the tipper must open the same real valve the ballast opens");
    require(disturbed_result.cylinder_pressure_pa > machine_start.cylinder_pressure_pa,
            "the player-opened valve must actually charge the actuator cylinder");

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
    std::cout << "PASS scraperx_sim coupled machine: valve=" << first_cycle.peak_valve_fraction
              << " piston=" << first_cycle.peak_piston_force
              << "N lift=" << first_cycle.peak_lift_height
              << "m second_lift=" << second_cycle.peak_lift_height
              << "m starved_energy=" << starved_final.vessel_available_energy_j
              << "J player_valve=" << disturbed_result.valve_open_fraction
              << " tower_m=" << Simulation::kTowerHeightMeters << '\n';
    return EXIT_SUCCESS;
}
