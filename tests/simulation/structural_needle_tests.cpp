#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

void run_structural_needle_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

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
    Simulation gap(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation crossing(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation unseat_check(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation checkpoint_capture(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
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
}

} // namespace scraperx::tests
