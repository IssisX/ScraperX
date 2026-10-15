#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

namespace {

scraperx::sim::Snapshot run_mantle_command_stream(const bool single_fixed_steps) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    Simulation simulation(InitialSpawn::MantleApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

} // namespace

TraversalReport run_athletic_traversal_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- WO-003 athletic traversal ---------------------------------------

    Simulation vault(InitialSpawn::VaultApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    // Double-tap Jump: the second press inside the window after a takeoff
    // vaults the same rail, on the ground vault's own terms; a second press
    // after the window, or with nothing vaultable ahead, is only a jump.
    const auto jump_vault_attempt = [](const double facing_x, const std::uint32_t gap_ticks) {
        Simulation run(InitialSpawn::VaultApproach, scraperx::sim::WorldContent::RegressionFixtures);
        (void)run.set_facing(facing_x, 0.0);
        (void)run.set_move_input(0.0, 0.0);
        (void)run.advance_frame(1.0);
        (void)run.set_move_input(facing_x, 0.0);
        if (facing_x > 0.0) {
            (void)advance_until(run,
                                [](const Snapshot &state) {
                                    return state.ledge_available &&
                                           state.ledge_entity_id == Simulation::kVaultRailEntityId;
                                },
                                2.0);
        } else {
            (void)run.advance_frame(0.5);
        }
        (void)run.request_jump();
        for (std::uint32_t tick = 0; tick <= gap_ticks; ++tick) {
            (void)run.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)run.request_jump();
        (void)run.advance_frame(Simulation::kFixedStepSeconds);
        const bool vaulting = run.snapshot().traversal_state == TraversalState::Vaulting;
        (void)advance_until(run, [](const Snapshot &state) { return state.player_grounded &&
                                                                    state.traversal_state ==
                                                                        TraversalState::None; },
                            3.0);
        return std::make_pair(vaulting, run.snapshot());
    };
    const auto [double_tap_vaulting, double_tapped] = jump_vault_attempt(1.0, 12);
    require(double_tap_vaulting && double_tapped.jump_vault_count == 1,
            "a second Jump 0.13 s after takeoff at the rail must commit a vault");
    require(double_tapped.player_position.x > 5.6 && double_tapped.aborted_traversal_count == 0,
            "the double-tap vault must cross the rail and land on the far side");
    const auto [late_tap_vaulting, late_tapped] = jump_vault_attempt(1.0, 40);
    require(!late_tap_vaulting && late_tapped.jump_vault_count == 0,
            "a second Jump after the 0.30 s window must not vault");
    const auto [away_tap_vaulting, away_tapped] = jump_vault_attempt(-1.0, 12);
    require(!away_tap_vaulting && away_tapped.jump_vault_count == 0,
            "a double-tap with nothing vaultable ahead must not vault");

    // A close diagonal approach near the ledge's corner must use the wall face
    // actually hit, not the requested facing vector, to locate the top and
    // landing. The old facing-offset probe stepped sideways past this top.
    Simulation oblique_mantle(InitialSpawn::MantleApproach,
                              scraperx::sim::WorldContent::RegressionFixtures);
    require(oblique_mantle.advance_frame(0.5).accepted,
            "oblique mantle settling interval must be accepted");
    require(walk_to(oblique_mantle, 8.45, -4.60, 2.0, 0.12, true),
            "the player must reach the close oblique mantle approach");
    require(oblique_mantle.set_move_input(0.0, 0.0),
            "oblique mantle stop input must be accepted");
    require(oblique_mantle.set_facing(1.0, 1.0),
            "oblique mantle facing must be accepted");
    require(oblique_mantle.advance_frame(0.2).accepted,
            "oblique mantle probe interval must be accepted");
    const auto oblique_ready = oblique_mantle.snapshot();
    require(oblique_ready.player_grounded,
            "the close oblique mantle approach must remain grounded");
    require(oblique_ready.ledge_available &&
                oblique_ready.ledge_entity_id == Simulation::kMantleLedgeEntityId,
            "the close oblique wall hit must offer the real mantle ledge");
    require(oblique_mantle.request_traversal(),
            "oblique mantle traversal request must be accepted");
    require(oblique_mantle.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "oblique mantle commit tick must advance");
    require(oblique_mantle.snapshot().traversal_state == TraversalState::Mantling,
            "the close oblique ledge must commit a native mantle");
    require(advance_until(oblique_mantle,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kMantleLedgeEntityId;
                          },
                          2.5),
            "the close oblique mantle must end supported on the real ledge");
    const auto oblique_mantled = oblique_mantle.snapshot();
    require(oblique_mantled.accepted_traversal_count == 1 &&
                oblique_mantled.aborted_traversal_count == 0,
            "the close oblique mantle must complete once without aborting");
    std::cout << "PASS scraperx_sim oblique close pull-up: ledge="
              << oblique_mantled.support_entity_id << " mantle="
              << oblique_mantled.accepted_traversal_count << '\n';

    Simulation mantle(InitialSpawn::MantleApproach, scraperx::sim::WorldContent::RegressionFixtures);
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
    // A standing mantle closes on the wall before it climbs. The ledge is
    // offered an arm's length back (x = 7.9 against the face at 9.0); by the
    // time the body has risen 0.1 m the capsule must stand at the hang
    // standoff: outside the face (ledge point - top-probe inset 0.12 -
    // radius 0.35 = 0.47 m back) and within reach of the lip.
    double mantle_rise_x = mantle_ready.player_position.x;
    require(advance_until(mantle,
                          [&](const Snapshot &state) {
                              mantle_rise_x = state.player_position.x;
                              return state.player_position.y > mantle_ready.player_position.y + 0.1;
                          },
                          1.0),
            "the standing mantle must begin to rise");
    const double mantle_rise_setback = mantle_ready.ledge_point.x - mantle_rise_x;
    require(mantle_rise_setback > 0.47 && mantle_rise_setback < 0.60,
            "a standing mantle must step in to the hang standoff before it rises");
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

    Simulation hang(InitialSpawn::HangApproach, scraperx::sim::WorldContent::RegressionFixtures);
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
    require(hang_start.player_gravity_factor == 1.0 &&
                hang_start.traversal_hand_constraint_count == 2,
            "a caught lip must use two finite hands with gravity active");
    // Catching has finite deceleration and compliant weight support. First
    // catch is not a settled pose; require settling before measuring drift.
    require(hang.advance_frame(2.0).accepted, "finite catch settling interval must be accepted");
    const auto hang_settled = hang.snapshot();
    require(std::abs(hang_settled.player_linear_velocity.y) < 0.08,
            "the finite catch must arrest falling motion, not drift forever");
    require(hang.advance_frame(1.0).accepted, "hang hold interval must be accepted");
    const auto hang_held = hang.snapshot();
    require(hang_held.traversal_state == TraversalState::Hanging,
            "the hang must hold against gravity on real geometry");
    require(std::abs(hang_held.player_position.y - hang_settled.player_position.y) < 0.05,
            "a settled finite hang on a static ledge must not drift");

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

    Simulation moving(InitialSpawn::MovingLedgeApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation released(InitialSpawn::MovingLedgeApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation blocked(InitialSpawn::BlockedLedgeApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    return {
        vaulted.player_position.x,
        double_tapped.player_position.x,
        horizontal_magnitude(vaulted.player_linear_velocity),
        mantled.support_entity_id,
        mantle_rise_setback,
        mantled.player_position.y,
        hang_start.traversal_support_entity_id,
        carried.traversal_support_entity_id,
        carried.player_linear_velocity.z,
        carried.moving_ledge_linear_velocity.z,
        after_release.player_linear_velocity.z,
        blocked_result.rejected_traversal_count,
        moving.snapshot().accepted_traversal_count
    };
}

void print_traversal_report(const TraversalReport &report) {
    std::cout << "PASS scraperx_sim athletic traversal: vault_x=" << report.vault_x
              << " double_tap_vault_x=" << report.double_tap_vault_x
              << " vault_speed=" << report.vault_speed
              << " mantle_support=" << report.mantle_support
              << " mantle_rise_setback=" << report.mantle_rise_setback
              << " mantle_y=" << report.mantle_y
              << " hang_support=" << report.hang_support
              << " moving_hang_support=" << report.moving_hang_support
              << " moving_hang_vz=" << report.moving_hang_vz
              << " moving_ledge_vz=" << report.moving_ledge_vz
              << " release_vz=" << report.release_vz
              << " rejected=" << report.rejected
              << " accepted=" << report.accepted << '\n';
}

} // namespace scraperx::tests
