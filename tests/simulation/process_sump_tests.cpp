#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

void run_process_sump_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- WO-013 first process coupling: KX-SUMP / KX-GRATE (Ascent Atlas --
    // ---- v1.0 kernel) --------------------------------------------------------
    // Geometry hardcoded from the design (kSump* constants in simulation.cpp),
    // the same convention used above. Approach decking spans x=[195.5,198.5];
    // the grate spans x=[198.5,201.5]; far decking spans x=[201.5,204.5]. The
    // valve station sits on the approach decking at x=197.0, z=16.0.

    // A toggle request away from the station must have no effect -- isolation
    // is only legal "at the real station" (WO-006/007 text).
    Simulation away(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    require(away.request_valve_toggle(), "off-station toggle request must be accepted as a command");
    require(away.advance_frame(1.0).accepted, "off-station settle interval must be accepted");
    require(!away.snapshot().sump_isolated,
            "a valve toggle must be a no-op anywhere but the real station, exactly like the jib "
            "and needle pendants -- the far side of the map must not be able to touch it");

    // Wet (the default): the grate must not be a crossable support. Walking
    // straight at it from the station, with the valve never touched, must
    // drop the player through rather than deliver them to the far decking.
    Simulation wet(InitialSpawn::KernelSumpStation, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation dry(InitialSpawn::KernelSumpStation, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation dump_check(InitialSpawn::KernelSumpStation, scraperx::sim::WorldContent::RegressionFixtures);
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
}

} // namespace scraperx::tests
