#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

void run_ground_water_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- Ground Archimedes screw: conserved water is the output port -----
    Simulation screw_off(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(screw_off.advance_frame(12.0).accepted, "stopped screw interval must advance");
    require(std::abs(screw_off.snapshot().water_screw_tank_volume_m3) < 1.0e-9,
            "motor off must move exactly no water to the upper tank");

    Simulation low_inlet(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    low_inlet.set_water_screw_basin_volume_m3(0.10);
    require(low_inlet.request_water_screw_toggle(), "low-inlet motor start must be accepted");
    require(low_inlet.advance_frame(12.0).accepted, "low-inlet interval must advance");
    require(low_inlet.snapshot().water_screw_tank_volume_m3 < 1.0e-6,
            "an inlet below minimum immersion must not pump water");

    Simulation under_torque(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    under_torque.set_water_screw_motor_torque_limit_nm(700.0);
    require(under_torque.request_water_screw_toggle(), "under-torque start must be accepted");
    require(under_torque.advance_frame(15.0).accepted, "under-torque interval must advance");
    require(std::abs(under_torque.snapshot().water_screw_rpm) < 0.5 &&
                under_torque.snapshot().water_screw_tank_volume_m3 < 1.0e-5,
            "a motor below hydraulic plus bearing torque must stall instead of faking flow");

    Simulation blocked_screw(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    blocked_screw.set_water_screw_outlet_blocked(true);
    require(blocked_screw.request_water_screw_toggle(), "blocked-outlet start must be accepted");
    require(blocked_screw.advance_frame(10.0).accepted, "blocked-outlet interval must advance");
    require(blocked_screw.snapshot().water_screw_tank_volume_m3 < 1.0e-6,
            "a blocked outlet must not fill the upper tank");

    Simulation screw(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(screw.advance_frame(0.5).accepted, "water-screw station settle must advance");
    require(screw.snapshot().water_screw_station_active,
            "the real apron spawn must be in reach of the screw control");
    require(screw.request_water_screw_toggle(), "normal screw start must be accepted");

    double screw_peak_rpm = 0.0;
    double screw_peak_torque = 0.0;
    double screw_peak_flow = 0.0;
    double worst_conservation = 0.0;
    const double initial_water = screw.snapshot().water_screw_basin_volume_m3 +
                                 screw.snapshot().water_screw_tank_volume_m3;
    bool screw_full = false;
    for (std::uint32_t tick = 0; tick < 90 * 125; ++tick) {
        require(screw.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "water-screw fixed step must advance");
        const auto state = screw.snapshot();
        screw_peak_rpm = std::max(screw_peak_rpm, std::abs(state.water_screw_rpm));
        screw_peak_torque =
            std::max(screw_peak_torque, std::abs(state.water_screw_motor_torque_nm));
        screw_peak_flow = std::max(screw_peak_flow, state.water_screw_flow_m3_s);
        worst_conservation =
            std::max(worst_conservation,
                     std::abs((state.water_screw_basin_volume_m3 +
                               state.water_screw_tank_volume_m3) - initial_water));
        if (state.water_screw_tank_volume_m3 >= 2.0 - 1.0e-9) {
            screw_full = true;
            break;
        }
    }
    require(screw_full, "rated screw must deliver the next mechanism's full 2.0 m3 input");
    const auto full_screw = screw.snapshot();
    require(screw_peak_rpm <= 22.05, "finite screw drive must respect its 22 rpm rating");
    require(screw_peak_torque <= 1500.01, "finite screw drive must respect 1.5 kN m torque");
    require(screw_peak_flow > 0.020 && screw_peak_flow < 0.023,
            "rated immersed screw must produce the derived ~0.022 m3/s flow");
    require(worst_conservation < 1.0e-9,
            "basin plus upper-tank water must be exactly conserved");
    require(full_screw.water_screw_basin_volume_m3 > 0.39 &&
                full_screw.water_screw_basin_volume_m3 < 0.41,
            "2.0 m3 delivered from 2.4 m3 must leave ~0.4 m3 in the basin");
    require(full_screw.water_screw_shaft_work_j >= 196200.0,
            "full delivery must spend at least the conservative 196.2 kJ shaft budget");

    const double full_before = full_screw.water_screw_tank_volume_m3;
    require(screw.advance_frame(8.0).accepted, "full-tank recycle interval must advance");
    const auto after_full = screw.snapshot();
    require(std::abs(after_full.water_screw_tank_volume_m3 - full_before) < 1.0e-9 &&
                std::abs(after_full.water_screw_flow_m3_s) < 1.0e-9,
            "a full tank must stop net delivery rather than delete or create water");

    screw.set_water_screw_drive_direction(-1);
    require(screw.advance_frame(10.0).accepted, "reverse interval must advance");
    const auto reversed_screw = screw.snapshot();
    require(reversed_screw.water_screw_tank_volume_m3 < full_before - 0.10 &&
                reversed_screw.water_screw_basin_volume_m3 >
                    after_full.water_screw_basin_volume_m3 + 0.10,
            "reverse shaft motion must return real water down to the basin");
    require(std::abs((reversed_screw.water_screw_basin_volume_m3 +
                      reversed_screw.water_screw_tank_volume_m3) - initial_water) < 1.0e-9,
            "reverse flow must conserve the same water inventory");
    std::cout << "PASS scraperx_sim ground water screw: tank_m3="
              << full_screw.water_screw_tank_volume_m3
              << " basin_m3=" << full_screw.water_screw_basin_volume_m3
              << " peak_rpm=" << screw_peak_rpm
              << " peak_torque_nm=" << screw_peak_torque
              << " peak_flow_m3_s=" << screw_peak_flow
              << " shaft_work_J=" << full_screw.water_screw_shaft_work_j
              << " leakage_m3=" << full_screw.water_screw_leakage_m3
              << " conserved_err=" << worst_conservation
              << " full_stop=1 reverse_return=1" << '\n';


    // ---- Ground water-weight lift: upstream mass becomes player ascent ----
    // The screw's delivery is already independently proven above. These runs
    // seed that exact conserved output at the seam, then falsify the receiving
    // mechanism itself. No seed supplies force, cage travel or catch state.
    const auto seed_water_lift = [](Simulation &sim, const double tank_m3) {
        sim.set_water_screw_tank_volume_m3_for_proof(tank_m3);
        require(sim.advance_frame(0.2).accepted, "water-lift seam seed must settle");
    };
    const auto fill_bucket = [](Simulation &sim, const double expected_m3) {
        const auto before = sim.snapshot();
        require(before.water_lift_valve_station_active,
                "water-lift fill control must be physically in reach");
        require(before.water_screw_tank_volume_m3 >= expected_m3 - 0.002,
                "water for the lift must exist in the authoritative upper tank");
        require(before.water_lift_bucket_catch_latched,
                "water-lift bucket must be physically caught at the fill station");
        require(sim.request_water_lift_valve_toggle(), "water-lift fill-open request accepted");
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "water-lift first fill tick must advance");
        const auto first = sim.snapshot();
        std::cout << "INFO ground water lift fill: expected_m3=" << expected_m3
                  << " valve=" << first.water_lift_valve_open
                  << " tank_m3=" << first.water_screw_tank_volume_m3
                  << " bucket_m3=" << first.water_lift_bucket_water_m3
                  << " bucket_travel=" << first.water_lift_bucket_travel_m
                  << " caught=" << first.water_lift_bucket_catch_latched
                  << " station=" << first.water_lift_valve_station_active
                  << " deaths=" << first.death_count << '\n';
        require(first.water_lift_valve_open,
                "water-lift fill command must open the authoritative valve");
        require(first.water_lift_bucket_water_m3 > before.water_lift_bucket_water_m3 &&
                    first.water_screw_tank_volume_m3 < before.water_screw_tank_volume_m3,
                "an open caught-bucket valve must transfer positive conserved water");
        require(advance_until(
                    sim,
                    [expected_m3](const Snapshot &s) {
                        return s.water_lift_bucket_water_m3 >= expected_m3 - 0.002;
                    },
                    24.0),
                "tank water must physically transfer into the caught bucket");
        require(sim.request_water_lift_valve_toggle(), "water-lift fill-close request accepted");
        require(sim.advance_frame(0.2).accepted, "water-lift valve close must resolve");
        require(!sim.snapshot().water_lift_valve_open,
                "water-lift valve must be shut before release");
    };
    const auto board_lift_cage = [](Simulation &sim) {
        // The controls sit outside the west rail, leaving the north opening
        // clear. Enter through that opening, then step left inside the cage to
        // the release lever instead of pathing through a pedestal.
        require(walk_to(sim, -12.50, -107.10, 5.0, 0.12) &&
                    walk_to(sim, -13.35, -107.65, 3.0, 0.12),
                "player must be able to enter the cage through its clear opening");
        require(sim.advance_frame(0.5).accepted, "cage stance must settle");
        const auto s = sim.snapshot();
        require(s.player_grounded &&
                    s.support_entity_id == Simulation::kGroundWaterLiftCageEntityId,
                "player must stand on the real moving cage before release");
        require(s.water_lift_release_station_active,
                "release lever must be reachable from inside the cage");
    };

    Simulation water_lift_dry(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(water_lift_dry.advance_frame(0.5).accepted, "dry lift spawn settle");
    board_lift_cage(water_lift_dry);
    require(water_lift_dry.request_water_lift_release(), "dry lift release request accepted");
    require(water_lift_dry.advance_frame(4.0).accepted, "dry lift interval must advance");
    require(water_lift_dry.snapshot().water_lift_cage_travel_m < 0.05,
            "an empty 200 kg bucket must not lift the player cage");

    Simulation water_lift_partial(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_partial, 0.40);
    fill_bucket(water_lift_partial, 0.40);
    board_lift_cage(water_lift_partial);
    require(water_lift_partial.request_water_lift_release(),
            "partial lift release request accepted");
    require(water_lift_partial.advance_frame(5.0).accepted, "partial lift interval must advance");
    require(water_lift_partial.snapshot().water_lift_cage_travel_m < 0.15,
            "400 kg of water must fail by force balance, not an arbitrary fill flag");

    Simulation water_lift_caught(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_caught, 2.0);
    fill_bucket(water_lift_caught, 2.0);
    const double caught_before = water_lift_caught.snapshot().water_lift_cage_travel_m;
    require(water_lift_caught.advance_frame(4.0).accepted, "caught lift interval must advance");
    require(std::abs(water_lift_caught.snapshot().water_lift_cage_travel_m - caught_before) < 0.02,
            "a full bucket still held by its physical catch must not move the cage");

    Simulation water_lift_disconnected(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_disconnected, 2.0);
    fill_bucket(water_lift_disconnected, 2.0);
    board_lift_cage(water_lift_disconnected);
    water_lift_disconnected.set_water_lift_rope_connected(false);
    require(water_lift_disconnected.request_water_lift_release(),
            "disconnected lift release request accepted");
    require(water_lift_disconnected.advance_frame(4.0).accepted,
            "disconnected lift interval must advance");
    require(water_lift_disconnected.snapshot().water_lift_cage_travel_m < 0.05,
            "a disconnected rope must transmit no counterweight lift");

    Simulation water_lift_overload(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_overload, 2.0);
    fill_bucket(water_lift_overload, 2.0);
    water_lift_overload.set_water_lift_cage_mass_kg(1300.0);
    board_lift_cage(water_lift_overload);
    require(water_lift_overload.request_water_lift_release(),
            "overloaded lift release request accepted");
    require(water_lift_overload.advance_frame(5.0).accepted,
            "overloaded lift interval must advance");
    require(water_lift_overload.snapshot().water_lift_cage_travel_m < 0.15,
            "an overloaded cage must fail from the real mass ratio");

    Simulation water_lift(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift, 2.0);
    const double water_total_start =
        water_lift.snapshot().water_screw_basin_volume_m3 +
        water_lift.snapshot().water_screw_tank_volume_m3 +
        water_lift.snapshot().water_lift_bucket_water_m3;
    fill_bucket(water_lift, 2.0);
    require(water_lift.snapshot().water_lift_bucket_mass_kg > 2198.0,
            "2.0 m3 must make the real bucket approximately 2200 kg");
    board_lift_cage(water_lift);
    require(water_lift.request_water_lift_release(), "rated lift release request accepted");

    bool rode_moving_cage = false;
    double lift_peak_rope_tension = 0.0;
    bool lift_caught = false;
    for (std::uint32_t tick = 0; tick < 12 * Simulation::kTickRateHz; ++tick) {
        require(water_lift.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "rated water-lift step must advance");
        const auto s = water_lift.snapshot();
        lift_peak_rope_tension =
            std::max(lift_peak_rope_tension, s.water_lift_rope_tension_n);
        rode_moving_cage =
            rode_moving_cage ||
            (s.support_entity_id == Simulation::kGroundWaterLiftCageEntityId &&
             s.water_lift_cage_travel_m > 0.25);
        if (s.water_lift_upper_catch_latched) {
            lift_caught = true;
            break;
        }
    }
    require(lift_caught,
            "full conserved water load must lift and catch the cage at +8 m");
    const auto lift_top = water_lift.snapshot();
    require(rode_moving_cage,
            "the player must ride the cage as a real moving support during ascent");
    require(lift_top.water_lift_cage_travel_m > 7.90,
            "upper catch must hold the cage at the +8 m travel seat");
    require(lift_top.water_lift_cage_peak_speed_mps <= 2.35,
            "finite brake/governor must keep cage speed near its 2.2 m/s rating");
    require(lift_peak_rope_tension > 1000.0,
            "the successful ascent must carry measurable solver rope tension");

    require(advance_until(
                water_lift,
                [](const Snapshot &s) { return s.water_lift_bucket_water_m3 < 0.01; },
                22.0),
            "caught cage must hold while the lower bucket drains by gravity");
    const auto drained = water_lift.snapshot();
    require(drained.water_lift_upper_catch_latched &&
                drained.water_lift_cage_travel_m > 7.90,
            "drain must not release the player cage from the +8 m pawl");
    const double water_total_drained =
        drained.water_screw_basin_volume_m3 + drained.water_screw_tank_volume_m3 +
        drained.water_lift_bucket_water_m3;
    require(std::abs(water_total_drained - water_total_start) < 1.0e-8,
            "tank -> bucket -> basin cycle must conserve the same water inventory");

    require(walk_to(water_lift, -9.45, -108.20, 5.0, 0.18, true),
            "player must step from caught cage onto the fixed +8 m dock");
    require(water_lift.advance_frame(0.5).accepted, "upper dock stance must settle");
    const auto on_dock = water_lift.snapshot();
    require(on_dock.player_grounded &&
                on_dock.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId,
            "the +8 m handoff must be fixed structural support, not the moving cage");
    // The reset pedestal itself is solid. The dock stance already lies
    // within the control's reach; walking into its centre is not a legal
    // navigation target.
    require(water_lift.snapshot().water_lift_reset_station_active,
            "upper reset control must be physically in reach");
    require(water_lift.request_water_lift_reset(), "water-lift reset request accepted");
    require(advance_until(
                water_lift,
                [](const Snapshot &s) {
                    return s.water_lift_cage_travel_m < 0.08 &&
                           s.water_lift_bucket_catch_latched;
                },
                14.0),
            "empty bucket must rise back to its top catch as the cage resets");

    const auto reset_lift = water_lift.snapshot();
    require(reset_lift.water_lift_bucket_water_m3 < 0.01 &&
                reset_lift.water_lift_cage_travel_m < 0.08,
            "reset must finish with an empty caught bucket and cage back at grade");

    // Continue the actual lift ride through its fixed dock and the new
    // transfer span. The cross brace is a real obstacle on the bridge; use the
    // existing jump, brake in air, land on that same static compound, then
    // walk onto MOD-STAIR-A and up its already-proven next flight.
    require(walk_to(water_lift, -8.55, -110.85, 4.0, 0.10),
            "the dock must feed the near end of the transfer span");
    const auto on_span = water_lift.snapshot();
    require(on_span.player_grounded &&
                on_span.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId &&
                on_span.player_position.y > 9.0,
            "the grating must support the player above the gate");
    (void)walk_toward(water_lift, -8.55, -114.0, 1.0);
    const auto at_brace = water_lift.snapshot();
    require(at_brace.player_grounded &&
                at_brace.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId &&
                at_brace.player_position.z > -111.70 &&
                at_brace.player_position.z < -111.40,
            "the visible cross brace must obstruct an ordinary walk on the grating");
    require(water_lift.set_facing(0.0, -1.0) &&
                water_lift.set_move_input(0.0, -1.0) &&
                water_lift.request_jump(),
            "the player must be able to jump the solid cross brace");
    bool landed_on_span = false;
    bool braked = false;
    for (int tick = 0; tick < 3 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        const auto s = water_lift.snapshot();
        if (!braked && !s.player_grounded && s.player_position.z < -113.0) {
            braked = true;
        }
        if (braked) {
            // Neutral preserves airborne momentum. Brake deliberately through
            // the ordinary bounded controller until actual grating contact.
            require(water_lift.set_move_input(
                        s.player_grounded ? 0.0 : std::clamp(-s.player_linear_velocity.x * 0.4, -1.0, 1.0),
                        s.player_grounded ? 0.0 : std::clamp(-s.player_linear_velocity.z * 0.4, -1.0, 1.0)),
                    "air control must accept deliberate countersteering");
        }
        require(water_lift.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "dock transfer jump tick must advance");
        const auto after = water_lift.snapshot();
        if (braked && after.player_grounded &&
            after.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId &&
            after.player_position.z < -112.5) {
            landed_on_span = true;
            break;
        }
    }
    require(landed_on_span,
            "jump over the brace must settle on visible grating, not a hidden support");
    require(walk_to(water_lift, -8.55, -115.5, 4.0, 0.15),
            "the span must join the existing +8 m stair landing");
    require(water_lift.advance_frame(0.3).accepted,
            "the stair handoff must settle under ordinary contact");
    require(water_lift.snapshot().player_grounded &&
                water_lift.snapshot().support_entity_id == Simulation::kIntakeStairEntityId,
            "the player must stand on the legal MOD-STAIR-A support");
    require(walk_to(water_lift, 7.85, -116.0, 9.0, 0.16),
            "the existing stair flight must carry the player to its next landing");
    require(water_lift.advance_frame(0.5).accepted,
            "the +12 m stair landing must hold a stable stance");
    const auto dock_stair_top = water_lift.snapshot();
    require(dock_stair_top.player_grounded &&
                dock_stair_top.support_entity_id == Simulation::kIntakeStairEntityId &&
                dock_stair_top.player_position.y > 12.95 &&
                dock_stair_top.player_position.y < 13.25,
            "the +8 m dock route must terminate on the stable +12 m stair landing");
    std::cout << "PASS scraperx_sim ground water lift: cage_travel="
              << lift_top.water_lift_cage_travel_m
              << " peak_speed=" << lift_top.water_lift_cage_peak_speed_mps
              << " rope_N=" << lift_peak_rope_tension
              << " bucket_kg=" << lift_top.water_lift_bucket_mass_kg
              << " dock_y=" << on_dock.player_position.y
              << " conserved_err=" << std::abs(water_total_drained - water_total_start)
              << " dry_fail=1 partial_fail=1 rope_fail=1 overload_fail=1"
              << " moving_support=1 reset=1 dock_to_stair12=1" << '\n';

    // One uninterrupted player run closes the producer/receiver seam. No
    // tank seed, cage placement, catch override, or teleport is used here.
    Simulation screw_ascent(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(screw_ascent.advance_frame(0.5).accepted,
            "continuous ascent screw-station settle must advance");
    const auto ascent_start = screw_ascent.snapshot();
    const double ascent_water_total =
        ascent_start.water_screw_basin_volume_m3 +
        ascent_start.water_screw_tank_volume_m3 +
        ascent_start.water_lift_bucket_water_m3;
    require(ascent_start.water_screw_station_active &&
                screw_ascent.request_water_screw_toggle(),
            "player must start the ground screw at its real station");
    require(advance_until(
                screw_ascent,
                [](const Snapshot &s) { return s.water_screw_tank_volume_m3 >= 1.999; },
                125.0),
            "running screw must supply the same tank used by the lift");
    require(screw_ascent.request_water_screw_toggle() &&
                screw_ascent.advance_frame(0.2).accepted,
            "player must be able to stop the screw before leaving its station");
    require(walk_to(screw_ascent, -14.45, -106.20, 8.0, 0.18),
            "player must walk from the screw to the bucket fill valve");
    require(screw_ascent.advance_frame(0.3).accepted &&
                screw_ascent.snapshot().water_lift_valve_station_active,
            "the walked-to valve must be physically in reach");
    fill_bucket(screw_ascent, 2.0);
    board_lift_cage(screw_ascent);
    require(screw_ascent.request_water_lift_release(),
            "boarded player must release the actual bucket catch");
    bool ascent_supported_on_moving_cage = false;
    bool ascent_caught = false;
    for (std::uint32_t tick = 0; tick < 12 * Simulation::kTickRateHz; ++tick) {
        require(screw_ascent.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "continuous player ascent tick must advance");
        const auto s = screw_ascent.snapshot();
        ascent_supported_on_moving_cage =
            ascent_supported_on_moving_cage ||
            (s.support_entity_id == Simulation::kGroundWaterLiftCageEntityId &&
             s.water_lift_cage_travel_m > 0.25);
        if (s.water_lift_upper_catch_latched) {
            ascent_caught = true;
            break;
        }
    }
    require(ascent_caught && ascent_supported_on_moving_cage &&
                screw_ascent.snapshot().water_lift_cage_travel_m > 7.90,
            "screw-fed bucket must physically carry its player to +8 m");
    require(walk_to(screw_ascent, -9.45, -108.20, 5.0, 0.18) &&
                screw_ascent.advance_frame(0.5).accepted,
            "player must step off the screw-fed lift onto its fixed dock");
    const auto ascent_dock = screw_ascent.snapshot();
    require(ascent_dock.player_grounded &&
                ascent_dock.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId,
            "continuous screw ascent must end on fixed +8 m support");
    const double ascent_water_end =
        ascent_dock.water_screw_basin_volume_m3 +
        ascent_dock.water_screw_tank_volume_m3 +
        ascent_dock.water_lift_bucket_water_m3;
    require(std::abs(ascent_water_end - ascent_water_total) < 1.0e-8,
            "continuous screw ascent must conserve basin, tank, and bucket water");
    std::cout << "PASS scraperx_sim continuous ground screw ascent: dock_y="
              << ascent_dock.player_position.y
              << " cage_travel=" << ascent_dock.water_lift_cage_travel_m
              << " conserved_err=" << std::abs(ascent_water_end - ascent_water_total)
              << " moving_support=1 no_seed=1" << '\n';
}

} // namespace scraperx::tests
