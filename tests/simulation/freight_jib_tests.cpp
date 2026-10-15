#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

void run_freight_jib_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- WO-011 first freight: KX-JIB / KX-CRATE (Ascent Atlas v1.0 kernel) --
    // A pendant-controlled crane, not an autonomous cycle: Drive/Raise/Lower/
    // Brake through a real, finite-torque/force Jolt constraint motor, and
    // only while the player is at the station.
    Simulation jib(InitialSpawn::KernelJibStation, scraperx::sim::WorldContent::RegressionFixtures);
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
    Simulation rider(InitialSpawn::KernelCrateTop, scraperx::sim::WorldContent::RegressionFixtures);
    require(rider.advance_frame(1.0).accepted, "crate-rider settle interval must be accepted");
    const auto riding = rider.snapshot();
    require(riding.player_grounded && riding.support_entity_id == Simulation::kCrateEntityId,
            "the player must be able to stand on the crate as a real support");

    // The capacity-proving stand: same rated force as the jib's winch,
    // permanently overweight, continuously commanded to raise. If it ever
    // rises, the rated force is not real.
    Simulation capacity(InitialSpawn::KernelJibStation, scraperx::sim::WorldContent::RegressionFixtures);
    const double stand_start_y = capacity.snapshot().jib_capacity_stand_load_position.y;
    require(capacity.advance_frame(6.0).accepted, "capacity-stand interval must be accepted");
    const auto stand_after = capacity.snapshot();
    require(std::abs(stand_after.jib_capacity_stand_load_position.y - stand_start_y) < 0.01,
            "a load past the rated winch force must never rise -- 'unlimited winch force' stays "
            "forbidden whether or not the player is watching");

    // A hold target is part of machine state: rewinding the bodies must not
    // leave a later powered target pulling the restored hook back upward.
    Simulation hold_restore(InitialSpawn::KernelJibStation,
                            scraperx::sim::WorldContent::RegressionFixtures);
    require(hold_restore.advance_frame(.5).accepted &&
                hold_restore.set_jib_hoist_input(1.0) &&
                hold_restore.advance_frame(2.0).accepted &&
                hold_restore.set_jib_hoist_input(0.0) &&
                hold_restore.advance_frame(1.8).accepted,
            "prepare a supported midtravel powered-hold checkpoint");
    const auto saved_hold = hold_restore.snapshot();
    require(saved_hold.player_grounded, "hold checkpoint must have real footing");
    const auto airborne_hold_ticks = [&](unsigned count) {
        for (unsigned tick = 0; tick < count; ++tick) {
            require(hold_restore.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "airborne hold tick must advance");
            const auto state = hold_restore.snapshot();
            require(!state.player_grounded &&
                        state.checkpoint_commit_count == saved_hold.checkpoint_commit_count,
                    "ordinary jump must freeze the earlier machine checkpoint");
            require(std::abs(state.jib_hoist_motor_force_n) <= 20001.0,
                    "powered hold and movement retain the same finite winch rating");
        }
    };
    require(hold_restore.request_jump(), "hold restore departure uses an ordinary jump");
    airborne_hold_ticks(11);
    require(hold_restore.set_jib_hoist_input(1.0), "move the hook after checkpoint departure");
    airborne_hold_ticks(54);
    require(hold_restore.set_jib_hoist_input(0.0), "latch a distinct later hold target");
    airborne_hold_ticks(9);
    require(hold_restore.snapshot().jib_hoist_position_meters >
                saved_hold.jib_hoist_position_meters + .35,
            "the later hold must be distinct from the saved extension");
    require(hold_restore.restart_checkpoint(), "ordinary checkpoint restart must succeed");
    require(std::abs(hold_restore.snapshot().jib_hoist_position_meters -
                     saved_hold.jib_hoist_position_meters) < .0001,
            "checkpoint restart must restore the earlier hook extension");
    for (unsigned tick = 0; tick < 2 * Simulation::kTickRateHz; ++tick) {
        require(hold_restore.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "restored neutral hold tick must advance");
        const auto state = hold_restore.snapshot();
        require(std::abs(state.jib_hoist_position_meters - saved_hold.jib_hoist_position_meters) < .1 &&
                    std::abs(state.jib_hoist_motor_force_n) <= 20001.0,
                "restored powered hold must retain the saved target and finite rating");
    }

    std::cout << "PASS scraperx_sim first freight: station=" << int(jib_idle.jib_station_active)
              << " raised_y=" << jib_raised.jib_hook_position.y
              << " braked_delta=" << (jib_braked.jib_hook_position.y - held_hook_y)
              << " slewed_rad=" << jib_slewed.jib_boom_angle_radians
              << " crate_carried=" << int(std::abs(jib_slewed.jib_crate_position.x - 206.0) > 5.0)
              << " rides_crate=" << int(riding.support_entity_id == Simulation::kCrateEntityId)
              << " capacity_held=" << int(std::abs(stand_after.jib_capacity_stand_load_position.y -
                                                    stand_start_y) < 0.01)
              << '\n';
}

} // namespace scraperx::tests
