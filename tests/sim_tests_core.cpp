// The fixed step, athletic traversal (WO-003), fall, parachute and checkpoint (WO-008), and the world's solids.

#include "sim_tests.hpp"

void run_core_traversal() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
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

    // Double-tap Jump: the second press inside the window after a takeoff
    // vaults the same rail, on the ground vault's own terms; a second press
    // after the window, or with nothing vaultable ahead, is only a jump.
    const auto jump_vault_attempt = [](const double facing_x, const std::uint32_t gap_ticks) {
        Simulation run(InitialSpawn::VaultApproach);
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
    require(hang.advance_frame(0.5).accepted, "hang pull-in interval must be accepted");
    const auto hang_settled = hang.snapshot();
    require(hang.advance_frame(1.0).accepted, "hang hold interval must be accepted");
    const auto hang_held = hang.snapshot();
    require(hang_held.traversal_state == TraversalState::Hanging,
            "the hang must hold against gravity on real geometry");
    require(std::abs(hang_held.player_position.y - hang_settled.player_position.y) < 0.05,
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

    // Reached for at arm's length -- falling past the same ledge's band 0.9 m
    // out from its wall, forward pressed only then -- the ledge is caught
    // well off its hold. The arms hold the body (phase 3): it swings in
    // under the hands with the speed it caught them at, and is never moved
    // faster than that -- no snap to the hold.
    Simulation reach(InitialSpawn::HangApproach);
    require(reach.set_facing(1.0, 0.0), "arm's-length hang facing must be accepted");
    require(advance_until(reach,
                          [](const Snapshot &state) { return state.player_position.y < 3.1; },
                          2.0),
            "the fall must pass the hang ledge's band");
    require(reach.snapshot().traversal_state == TraversalState::None,
            "nothing is caught without reaching for it");
    require(reach.set_move_input(1.0, 0.0), "reaching for the ledge must be accepted");
    auto reach_last = reach.snapshot();
    const double reach_from_x = reach_last.player_position.x;
    double worst_hang_tick = 0.0;
    double catch_speed = -1.0;
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz / 2; ++tick) {
        require(reach.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "arm's-length hang tick must be accepted");
        const auto now = reach.snapshot();
        if (now.traversal_state == TraversalState::Hanging && catch_speed < 0.0) {
            const auto v = reach_last.player_linear_velocity;
            catch_speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }
        if (now.traversal_state == TraversalState::Hanging) {
            worst_hang_tick = std::max(
                worst_hang_tick, std::hypot(horizontal_distance(now.player_position, reach_last.player_position),
                                            now.player_position.y - reach_last.player_position.y));
        }
        reach_last = now;
    }
    require(reach_last.traversal_state == TraversalState::Hanging,
            "a ledge reached for at arm's length must be caught");
    require(reach_last.player_position.x - reach_from_x > 0.3,
            "the arm's-length catch must start well out from its hold");
    require(worst_hang_tick <= (catch_speed + 0.5) * Simulation::kFixedStepSeconds,
            "a ledge caught at arm's length must swing the body in under its hold, not snap it there");

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

    // The hands stay where they closed on the moving ledge; the body hangs
    // from them and leans a little as the ledge speeds up and slows.
    const double hang_offset_before = carried.traversal_left_hand.z - carried.moving_ledge_position.z;
    const double body_offset_before = carried.player_position.z - carried.moving_ledge_position.z;
    require(moving.advance_frame(0.5).accepted, "second moving-hang interval must be accepted");
    const auto carried_later = moving.snapshot();
    const double hang_offset_after =
        carried_later.traversal_left_hand.z - carried_later.moving_ledge_position.z;
    const double body_offset_after = carried_later.player_position.z - carried_later.moving_ledge_position.z;
    require(std::abs(hang_offset_after - hang_offset_before) < 0.01,
            "the hang hold must stay fixed in the moving support's own frame");
    require(std::abs(body_offset_after - body_offset_before) < 0.2,
            "the body hanging from a moving ledge must stay under its hands");
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

    // The hang is the arms' physics, not a pose (a falsifier): the ledge
    // caught after a short fall holds without a slip; the same ledge caught
    // after a 6 m fall loads the hands past their grip and tears them off,
    // the body still falling; a hang let go mid-swing keeps its swing.
    require(hang.snapshot().hang_slip_count == 0, "a hang caught from a short fall must not slip");

    Simulation torn(InitialSpawn::HangHighDrop);
    require(torn.set_facing(1.0, 0.0), "high-drop facing must be accepted");
    require(advance_until(torn,
                          [](const Snapshot &state) { return state.player_position.y < 5.0; },
                          2.0),
            "the high drop must fall to the hang ledge");
    require(torn.snapshot().player_linear_velocity.y < -9.0, "the high drop must arrive fast");
    require(torn.set_move_input(1.0, 0.0), "high-drop reach must be accepted");
    require(advance_until(torn,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "a ledge passed in a 6 m fall must still be caught");
    require(advance_until(torn,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::None;
                          },
                          0.5),
            "a ledge caught at the end of a 6 m fall must tear out of the hands");
    const auto tore = torn.snapshot();
    require(tore.hang_slip_count == 1, "the torn catch must be the grip's slip, not a release");
    require(tore.arm_broken && tore.arm_break_count == 1 && tore.broken_arm_side == 1,
            "a catch torn out of the hands by its spike must wrench the right arm");
    require(tore.player_linear_velocity.y < -2.0, "a torn grip must leave the body still falling");

    Simulation swing(InitialSpawn::HangApproach);
    require(swing.set_facing(1.0, 0.0), "swing facing must be accepted");
    require(advance_until(swing,
                          [](const Snapshot &state) { return state.player_position.y < 3.1; },
                          2.0),
            "the swing fall must pass the hang ledge's band");
    require(swing.set_move_input(1.0, 0.0), "reaching for the swing ledge must be accepted");
    require(advance_until(swing,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          0.5),
            "the swing ledge must be caught at arm's length");
    require(swing.advance_frame(0.1).accepted, "swing interval must be accepted");
    const auto swinging = swing.snapshot();
    require(swinging.traversal_state == TraversalState::Hanging, "the swing must still be hanging");
    require(swing.request_release(), "a swinging hang must be allowed to let go");
    require(swing.advance_frame(Simulation::kFixedStepSeconds).accepted, "swing release tick must advance");
    const auto let_go = swing.snapshot();
    require(let_go.traversal_state == TraversalState::None, "letting go must end the swing hang");
    require(std::abs(swinging.player_linear_velocity.x) > 0.5, "the caught body must be swinging in");
    require(std::abs(let_go.player_linear_velocity.x - swinging.player_linear_velocity.x) < 0.3,
            "a hang let go mid-swing must keep its swing");

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

    // The game starts outdoors at grade, short of the tower.
    Simulation start(InitialSpawn::ExteriorGrade);
    const auto start_state = start.snapshot();
    require(start_state.player_position.z < -20.0 && start_state.player_position.y < 2.0,
            "the default spawn must be outdoors at grade, short of the tower");

    // A support that moves smoothly is re-matched tick by tick: riding the
    // translating and turning supports never adds up to a stumble.
    require(translating.snapshot().support_stumble_count == 0 && rotating.snapshot().support_stumble_count == 0 &&
                translating.snapshot().support_jolt_peak_m < 0.15 && rotating.snapshot().support_jolt_peak_m < 0.15,
            "smoothly moving supports must not jolt the rider off balance");
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
              << " double_tap_vault_x=" << double_tapped.player_position.x
              << " vault_speed=" << horizontal_magnitude(vaulted.player_linear_velocity)
              << " mantle_support=" << mantled.support_entity_id
              << " mantle_rise_setback=" << mantle_rise_setback
              << " mantle_y=" << mantled.player_position.y
              << " hang_support=" << hang_start.traversal_support_entity_id
              << " moving_hang_support=" << carried.traversal_support_entity_id
              << " moving_hang_vz=" << carried.player_linear_velocity.z
              << " moving_ledge_vz=" << carried.moving_ledge_linear_velocity.z
              << " release_vz=" << after_release.player_linear_velocity.z
              << " rejected=" << blocked_result.rejected_traversal_count
              << " accepted=" << moving.snapshot().accepted_traversal_count << '\n';
}

void run_fall_checkpoint() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

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

    // The checkpoint follows the last firm footing, and freezes in the air.
    Simulation committed(InitialSpawn::StaticDeck);
    require(committed.set_facing(0.0, 1.0), "checkpoint test facing must be accepted");
    require(committed.set_move_input(0.0, 1.0), "checkpoint test walk input must be accepted");
    require(committed.advance_frame(1.0).accepted, "the checkpoint test walk must advance");
    require(committed.set_move_input(0.0, 0.0), "checkpoint test halt input must be accepted");
    const auto pre_commit_checkpoints = committed.snapshot().checkpoint_commit_count;
    require(committed.advance_frame(1.0).accepted,
            "standing must advance and keep auto-committing");
    const auto disturbed_checkpoint = committed.snapshot();
    require(disturbed_checkpoint.player_grounded, "the checkpoint test must be standing");
    require(disturbed_checkpoint.checkpoint_commit_count > pre_commit_checkpoints,
            "standing grounded must keep advancing the automatic commit count");
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

    std::cout << "PASS scraperx_sim fall/parachute/checkpoint: lethal_impact="
              << lethal_result.last_impact_speed_mps
              << " chuted_impact=" << chuted_result.last_impact_speed_mps
              << " late_chute_deaths=" << late_chute.snapshot().death_count
              << " grounded_deploy_blocked="
              << int(!grounded_parachute.snapshot().parachute_deployed)
              << " commits=" << airborne_checkpoint.checkpoint_commit_count << '\n';
}

void run_world_solids() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    // ---- World solids: every visible body is a body -----------------------
    // The dressing drawn by the Godot builders (world_solids.inc) is native
    // collision, not decoration. The buttress footings at the stack's corners
    // are the case a player walked straight through: a 6 x 2.6 x 6 m block
    // under a 2.4 m splayed leg. Walked into from the yard, the capsule must
    // stop at its face -- centre never nearer than half-width (3.0 m) plus
    // most of the capsule radius (0.35 m) on the dominant axis.
    Simulation solid(InitialSpawn::ExteriorGrade);
    require(solid.advance_frame(0.5).accepted, "world-solids settling interval must be accepted");
    const auto solid_loaded = solid.snapshot();
    require(solid_loaded.world_solid_rejected == 0,
            "every generated hull must be a valid Jolt convex shape");
    require(solid_loaded.world_solid_bodies > 1400,
            "the generated world-solid table must load as native bodies");
    require(solid_loaded.world_solid_mirrors > 0,
            "drawn mirrors of owned bodies must be recognised and not doubled");
    constexpr double kFootX = -39.0;
    constexpr double kFootZ = -111.0;
    double foot_closest = 1.0e9;
    const auto track_foot = [&]() {
        const auto at = solid.snapshot().player_position;
        foot_closest = std::min(foot_closest,
                                std::max(std::abs(at.x - kFootX), std::abs(at.z - kFootZ)));
    };
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        walk_toward(solid, -30.0, -100.0, Simulation::kFixedStepSeconds);
        track_foot();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        walk_toward(solid, kFootX, kFootZ, Simulation::kFixedStepSeconds);
        track_foot();
    }
    require(foot_closest < 3.6, "the walk must actually reach the buttress footing");
    require(foot_closest > 3.25,
            "walking into a buttress footing must be stopped at its face by native collision");
    require(solid.snapshot().player_grounded,
            "the player stopped by the footing must still stand on the yard");

    // No ramp and no stair: walking the stack's bands at grade, under the
    // decks where fourteen flights once climbed, never leaves the ground.
    Simulation stair(InitialSpawn::ExteriorGrade);
    require(stair.advance_frame(0.5).accepted, "stack walk settling interval must be accepted");
    walk_toward(stair, -25.0, -118.0, 20.0);
    walk_toward(stair, -25.0, -128.5, 6.0);
    double stack_walk_peak_y = 0.0;
    for (const double side : {1.0, -1.0}) {
        const double band = -150.0 + side * 21.5;
        for (std::uint32_t tick = 0; tick < 90 * 12; ++tick) {
            walk_toward(stair, side * 21.0, band, Simulation::kFixedStepSeconds);
            stack_walk_peak_y = std::max(stack_walk_peak_y, stair.snapshot().player_position.y);
        }
    }
    require(stack_walk_peak_y < 2.0, "no ramp or stair may carry a walker off the grade");

    // The yard sits in a basin, not on a slab in a void: off the grade's
    // east edge the valley floor carries the player 0.3 m lower, the step
    // back up is walked, and walking out to the rim ends against rock --
    // grounded, low, and never past the ridge line.
    Simulation basin(InitialSpawn::ExteriorGrade);
    require(basin.advance_frame(0.5).accepted, "basin settling interval must be accepted");
    walk_toward(basin, 270.0, -60.0, 70.0);
    const auto on_valley = basin.snapshot();
    require(on_valley.player_grounded && on_valley.player_position.x > 250.0 &&
                std::abs(on_valley.player_position.y - (-0.3 + 0.9)) < 0.1,
            "walking off the grade must land on the valley floor, not fall");
    walk_toward(basin, 200.0, -60.0, 20.0);
    const auto back_on_grade = basin.snapshot();
    require(back_on_grade.player_grounded && back_on_grade.player_position.x < 230.0 &&
                back_on_grade.player_position.y > 0.8,
            "the 0.3 m step from the valley floor back onto the grade must be walked");
    walk_toward(basin, 200.0, 1400.0, 320.0);
    const auto at_rim = basin.snapshot();
    const double rim_reach = std::hypot(at_rim.player_position.x - 0.0,
                                        at_rim.player_position.z - (-100.0));
    require(at_rim.player_grounded && at_rim.player_position.y < 5.0 && rim_reach < 1100.0,
            "walking out to the basin rim must end against its rock, low and grounded");

    std::cout << "PASS scraperx_sim world solids: bodies=" << solid_loaded.world_solid_bodies
              << " mirrors=" << solid_loaded.world_solid_mirrors
              << " rejected=" << solid_loaded.world_solid_rejected
              << " buttress_foot_closest=" << foot_closest
              << " stack_walk_peak_y=" << stack_walk_peak_y
              << " rim_reach=" << rim_reach << '\n';

    // Crouch (GDD 7.2, Governing Law 4). The crawl beam beside the traversal
    // fixtures has its underside 1.45 m over the deck: under the standing
    // capsule's 1.8 m, over the crouched one's 1.2 m. Standing, the beam
    // stops the body at its face; crouched, it walks under; beneath it,
    // letting go of crouch does not stand it up, and neither Jump nor Action
    // does anything; walked out past it, it stands up by itself.
    constexpr double kCrawlLaneX = -2.0;
    constexpr double kCrawlBeamZ = -13.0;
    constexpr double kCrawlBeamHalfZ = 0.3;
    constexpr double kCrouchSpeedLimit = 5.5 * 0.45;
    Simulation crouch(InitialSpawn::StaticDeck);
    require(crouch.advance_frame(1.0).accepted, "crouch settling interval must be accepted");
    require(crouch.snapshot().player_grounded && !crouch.snapshot().player_crouched,
            "the crouch run must start standing on the deck");
    const double standing_deepest = walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ - 2.0, 5.0);
    require(standing_deepest > kCrawlBeamZ + kCrawlBeamHalfZ,
            "a standing capsule must not get under a beam 1.45 m over the deck");
    walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ + 3.0, 2.0);
    require(crouch.set_move_input(0.0, 0.0), "crouch stop input must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch standing settle must be accepted");
    const auto crouch_standing = crouch.snapshot();
    require(crouch_standing.player_grounded && !crouch_standing.player_crouched,
            "backing off the beam must leave the body standing");

    require(crouch.set_crouch_input(true), "crouch input must be accepted");
    require(crouch.advance_frame(0.3).accepted, "crouch interval must be accepted");
    const auto crouched = crouch.snapshot();
    const double crouch_drop = crouch_standing.player_position.y - crouched.player_position.y;
    require(crouched.player_crouched && crouched.player_grounded,
            "crouch held on the ground must crouch the body");
    require(std::abs(crouch_drop - 0.3) < 0.03,
            "crouching must lower the centre by 0.3 m and leave the feet where they were");

    double crouched_top_speed = 0.0;
    for (int i = 0; i < 4 * 90; ++i) {
        const auto state = crouch.snapshot();
        double dx = kCrawlLaneX - state.player_position.x;
        double dz = kCrawlBeamZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len < 0.05) {
            break;
        }
        (void)crouch.set_move_input(dx / len, dz / len);
        (void)crouch.set_facing(dx / len, dz / len);
        (void)crouch.advance_frame(Simulation::kFixedStepSeconds);
        crouched_top_speed = std::max(crouched_top_speed,
                                      horizontal_magnitude(crouch.snapshot().player_linear_velocity));
    }
    require(crouch.set_move_input(0.0, 0.0), "crouch stop input must be accepted");
    require(crouch.advance_frame(0.3).accepted, "crouch under-beam settle must be accepted");
    const auto under = crouch.snapshot();
    require(std::abs(under.player_position.z - kCrawlBeamZ) < 0.3 && under.player_crouched &&
                under.player_grounded,
            "a crouched capsule must walk in under the beam and stand on the deck there");
    require(crouched_top_speed > 2.0 && crouched_top_speed < kCrouchSpeedLimit + 0.05,
            "crouched walking must run at the crouch speed, not the standing one");

    require(crouch.set_crouch_input(false), "crouch release must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch release interval must be accepted");
    require(crouch.snapshot().player_crouched,
            "under the beam, letting go of crouch must not stand the body up into it");
    const double under_y = crouch.snapshot().player_position.y;
    const auto rejected_before = crouch.snapshot().rejected_traversal_count;
    require(crouch.request_jump(), "a jump request under the beam must be queued");
    require(crouch.advance_frame(0.3).accepted, "under-beam jump interval must be accepted");
    const auto after_jump = crouch.snapshot();
    require(after_jump.player_crouched && after_jump.player_grounded &&
                after_jump.player_position.y < under_y + 0.05,
            "with no room to stand there is no jump");
    require(crouch.request_traversal(), "a traversal request under the beam must be queued");
    require(crouch.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "under-beam traversal tick must advance");
    require(crouch.snapshot().rejected_traversal_count == rejected_before + 1 &&
                crouch.snapshot().traversal_state == TraversalState::None,
            "a traversal request with no room to stand must be refused, and counted");

    walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ - 2.0, 3.0);
    require(crouch.set_move_input(0.0, 0.0), "crouch exit stop input must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch exit settle must be accepted");
    const auto beyond = crouch.snapshot();
    require(beyond.player_position.z < kCrawlBeamZ - kCrawlBeamHalfZ - 0.35,
            "the released-crouch body must still walk on out from under the beam");
    require(!beyond.player_crouched && beyond.player_grounded &&
                std::abs(beyond.player_position.y - crouch_standing.player_position.y) < 0.03,
            "clear of the beam, a released crouch must stand the body back up");

    std::cout << "PASS scraperx_sim crouch: standing_deepest_z=" << standing_deepest
              << " crouch_drop=" << crouch_drop << " crouched_top_speed=" << crouched_top_speed
              << " under_z=" << under.player_position.z
              << " stood_y=" << beyond.player_position.y << '\n';
}
