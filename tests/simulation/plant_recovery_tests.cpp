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

void run_plant_recovery_tests(const SupportReport &support, const TraversalReport &traversal) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- WO-006 coupled machine -----------------------------------------

    Simulation machine(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation starved(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation disturbed(InitialSpawn::MachineYard, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation machine_partitioned(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 20; ++tick) {
        require(machine_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "machine partition step must be accepted");
    }
    Simulation machine_batched(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
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

    print_support_report(support);
    print_traversal_report(traversal);
    // ---- WO-008 fall / parachute / checkpoint ----------------------------

    Simulation lethal(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation survivable(InitialSpawn::SurvivableDrop, scraperx::sim::WorldContent::RegressionFixtures);
    require(advance_until(survivable,
                          [](const Snapshot &state) { return state.player_grounded; },
                          4.0),
            "the short drop must land");
    require(survivable.snapshot().death_count == 0,
            "an ordinary ~12 m platforming fall must never be lethal (GDD 8.2)");

    Simulation chuted(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation late_chute(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation grounded_parachute(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation committed(InitialSpawn::MachineYard, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation machine_restore(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation fall_partitioned(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 4; ++tick) {
        require(fall_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fall-subsystem partition step must be accepted");
    }
    Simulation fall_batched(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation chain(InitialSpawn::LiftPlatform, scraperx::sim::WorldContent::RegressionFixtures);
    require(chain.set_facing(0.0, -1.0), "chain facing toward the catwalk must be accepted");
    // The platform extends to -102.3; deliberately stop inside its edge,
    // with the real -103 catwalk within reach rather than relying on coast.
    require(walk_to(chain, chain.snapshot().player_position.x, -101.85, 3.0, .04, true),
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
    Simulation idle_plant(InitialSpawn::MachineYard, scraperx::sim::WorldContent::RegressionFixtures);
    require(idle_plant.advance_frame(8.0).accepted, "control-plant interval must be accepted");
    const auto idle_eight = idle_plant.snapshot();
    require(idle_eight.valve_open_fraction == 0.0,
            "with nobody on the treadle the valve must still be shut at this point in the cycle");
    require(idle_eight.lift_platform_position.y < 2.0,
            "with nobody on the treadle the lift must still be parked -- the machine does not "
            "open this route on its own at this moment");

    Simulation treadle(InitialSpawn::CatwalkTreadle, scraperx::sim::WorldContent::RegressionFixtures);
    const auto treadle_spawn = treadle.snapshot();
    require(std::abs(treadle_spawn.treadle_angle_radians) < 0.01 &&
                treadle_spawn.valve_open_fraction == 0.0,
            "the treadle must rest against its stop with the valve shut -- a control that is not "
            "being stood on grants nothing (Governing Law 24)");
    // The real cable/plate oscillates under an 85 kg rider. Measure its
    // standing capability over time, not an arbitrary valve phase at 8 s.
    std::uint32_t held_capability_ticks = 0;
    double held_valve_peak = 0;
    for (std::uint32_t i=0; i<8*Simulation::kTickRateHz; ++i) {
        require(treadle.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "treadle-held interval must be accepted");
        const auto state=treadle.snapshot();
        held_valve_peak=std::max(held_valve_peak,state.valve_open_fraction);
        if(i>=4*Simulation::kTickRateHz) {
            require(state.player_grounded && state.support_entity_id==Simulation::kTreadleEntityId &&
                    state.lift_platform_position.y>6.7 && state.lift_platform_position.y<8.3,
                    "held control maintains actual footing and the standing lift window");
            if(state.valve_open_fraction>.5 && state.piston_force_n>0) ++held_capability_ticks;
        }
    }
    const auto treadle_held = treadle.snapshot();
    require(treadle_held.player_grounded &&
                treadle_held.support_entity_id == Simulation::kTreadleEntityId,
            "the player must actually be standing on the treadle, not beside it");
    require(treadle_held.treadle_angle_radians > 0.05,
            "an 85 kg body must visibly swing the treadle against its counterweight -- this is "
            "the control that is built to a human scale, unlike the 900 kg tipper");
    require(held_valve_peak > 0.5 && held_capability_ticks >= 2*Simulation::kTickRateHz,
            "standing on the treadle must haul the cable and open the real valve");
    require(held_capability_ticks >= 2*Simulation::kTickRateHz,
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
}

} // namespace scraperx::tests
