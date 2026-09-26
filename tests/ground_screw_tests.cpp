#include "sim/ground_stage.hpp"
#include "sim/water_screw.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void run_for(scraperx::sim::GroundStage &stage, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        require(stage.advance_frame(1.0 / 90.0), "fixed step rejected");
    }
}

} // namespace

int main() {
    using scraperx::sim::GroundStage;
    using scraperx::sim::WaterScrew;

    GroundStage idle;
    require(!idle.request_motor_toggle(20.0, 0.0, 20.0),
            "motor accepted a remote interaction");
    require(!idle.request_motor_toggle(NAN, 0.0, 0.0),
            "motor accepted a non-finite player position");
    run_for(idle, 90 * 12);
    require(idle.state().tank_volume_m3 == 0.0,
            "water climbed with the motor off");
    require(idle.request_motor_toggle(3.5, 0.0, 0.5),
            "ground switch did not start motor");
    require(idle.state().motor_enabled, "motor state did not change");

    GroundStage pump;
    require(pump.request_motor_toggle(3.5, 0.0, 0.5),
            "local motor start failed");
    run_for(pump, 90 * 45);
    const auto full = pump.state();
    require(full.tank_volume_m3 > 1.999 && full.tank_volume_m3 <= 2.0,
            "screw did not fill the 2 m3 upper tank");
    require(std::abs(full.basin_volume_m3 + full.tank_volume_m3 - 2.4) < 1e-9,
            "water was created or lost during pumping");
    require(full.shaft_rpm <= 18.001,
            "shaft exceeded its rated speed");
    require(full.motor_torque_nm <= 5000.001,
            "motor exceeded its torque rating");
    require(full.shaft_work_j > full.hydraulic_work_j,
            "hydraulic output exceeded shaft work");
    run_for(pump, 90 * 4);
    require(pump.state().tank_volume_m3 == full.tank_volume_m3 &&
                pump.state().delivered_flow_m3_s == 0.0,
            "full tank kept accepting water");
    require(!pump.request_reverse(3.5, 0.0, 0.5),
            "direction changed while the motor was running");
    require(pump.request_motor_toggle(3.5, 0.0, 0.5),
            "local motor stop failed");
    run_for(pump, 90 * 2);
    require(pump.request_reverse(3.5, 0.0, 0.5),
            "stopped motor could not reverse");
    require(pump.request_motor_toggle(3.5, 0.0, 0.5),
            "reverse start failed");
    run_for(pump, 90 * 10);
    require(pump.state().tank_volume_m3 < full.tank_volume_m3 - 0.10,
            "reverse did not return water to basin");
    require(std::abs(pump.state().basin_volume_m3 +
                         pump.state().tank_volume_m3 - 2.4) < 1e-9,
            "reverse broke water conservation");

    WaterScrew dry;
    dry.set_basin_volume_m3(0.10);
    dry.toggle_motor();
    for (int i = 0; i < 90 * 8; ++i) dry.step(1.0 / 90.0);
    require(dry.state().tank_volume_m3 < 1e-8,
            "unimmersed inlet delivered water");

    WaterScrew weak;
    weak.set_motor_torque_limit_nm(2500.0);
    weak.toggle_motor();
    for (int i = 0; i < 90 * 8; ++i) weak.step(1.0 / 90.0);
    require(weak.state().tank_volume_m3 < 1e-8,
            "underrated motor lifted water");

    WaterScrew blocked;
    blocked.set_outlet_blocked(true);
    blocked.toggle_motor();
    for (int i = 0; i < 90 * 8; ++i) blocked.step(1.0 / 90.0);
    require(blocked.state().tank_volume_m3 < 1e-8,
            "blocked outlet delivered water");

    GroundStage coarse;
    GroundStage fine;
    require(coarse.request_motor_toggle(3.5, 0.0, 0.5) &&
                fine.request_motor_toggle(3.5, 0.0, 0.5),
            "frame-partition setup failed");
    for (int i = 0; i < 900; ++i) {
        require(coarse.advance_frame(1.0 / 30.0), "coarse frame rejected");
        for (int j = 0; j < 3; ++j) {
            require(fine.advance_frame(1.0 / 90.0), "fine frame rejected");
        }
    }
    require(std::abs(coarse.state().tank_volume_m3 - fine.state().tank_volume_m3) < 1e-8,
            "water delivery changed with render frame partition");

    GroundStage ride;
    require(ride.request_motor_toggle(3.5, 0.0, 0.5), "ride pump start failed");
    run_for(ride, 90 * 45);
    require(ride.request_motor_toggle(3.5, 0.0, 0.5), "ride pump stop failed");
    run_for(ride, 90 * 8); // Let finite rotor inertia settle before opening the valve.
    require(ride.request_valve_toggle(3.5, 0.0, 0.5), "upper tank valve inaccessible");
    run_for(ride, 90 * 15);
    require(ride.lift_state().bucket_water_m3 > 1.999,
            "conserved tank water did not fill the lift bucket");
    require(ride.state().tank_volume_m3 < 1e-8,
            "bucket fill left phantom water in the tank");
    require(!ride.request_release(4.0, 0.0, -9.0),
            "cage release accepted without a rider");
    ride.set_rider_on_cage(true);
    require(ride.request_release(4.0, 0.0, -9.0),
            "aboard rider could not release the lift");
    bool caught = false;
    for (int tick = 0; tick < 90 * 15; ++tick) {
        require(ride.advance_frame(1.0 / 90.0), "lift frame rejected");
        if (ride.lift_state().upper_catch_latched) {
            caught = true;
            break;
        }
    }
    require(caught && ride.lift_state().cage_travel_m > 7.999,
            "screw-fed bucket did not carry the rider to +8 m catch");
    require(ride.lift_state().peak_cage_speed_m_s < 2.5,
            "finite governor allowed excessive cage speed");
    require(ride.lift_state().rope_tension_n > 1000.0,
            "successful lift has no transmitted rope tension");
    require(std::abs(ride.lift_energy_residual_j()) <
                0.02 * std::abs(ride.lift_state().gravity_work_j),
            "lift energy ledger exceeds two percent of gravity work");
    ride.set_rider_on_cage(false);
    run_for(ride, 90 * 15);
    require(ride.lift_state().bucket_water_m3 < 1e-8,
            "caught lift did not drain the bucket to the basin");
    require(ride.lift_state().upper_catch_latched,
            "upper catch released during bucket drainage");
    require(std::abs(ride.total_water_m3() - 2.4) < 1e-8,
            "pump, valve, lift and drain did not conserve water");
    require(ride.request_reset(8.0, 8.0, -9.0),
            "upper deck reset control did not release caught lift");
    run_for(ride, 90 * 15);
    require(ride.lift_state().cage_travel_m < 0.001 &&
                ride.lift_state().bucket_top_catch_latched,
            "empty bucket did not return to its top catch");

    require(ride.request_motor_toggle(3.5, 0.0, 0.5),
            "second cycle pump could not start");
    run_for(ride, 90 * 45);
    require(ride.request_motor_toggle(3.5, 0.0, 0.5),
            "second cycle pump could not stop");
    run_for(ride, 90 * 8);
    require(ride.request_valve_toggle(3.5, 0.0, 0.5),
            "second cycle valve could not open");
    run_for(ride, 90 * 15);
    ride.set_rider_on_cage(true);
    require(ride.request_release(4.0, 0.0, -9.0),
            "second cycle release failed");
    run_for(ride, 90 * 15);
    require(ride.lift_state().upper_catch_latched,
            "second cycle did not reach the upper catch");
    ride.set_rider_on_cage(false);
    run_for(ride, 90 * 15);
    require(ride.request_reset(3.5, 0.0, 0.5),
            "fall recovery cannot reset the empty cage from grade");
    run_for(ride, 90 * 15);
    require(ride.lift_state().bucket_top_catch_latched,
            "grade reset did not recover the cage");

    GroundStage dry_lift;
    dry_lift.set_rider_on_cage(true);
    require(dry_lift.request_release(4.0, 0.0, -9.0),
            "dry lift release should be a physical action");
    run_for(dry_lift, 90 * 5);
    require(dry_lift.lift_state().cage_travel_m < 0.001,
            "dry bucket lifted the rider");
    require(dry_lift.lift_state().bucket_top_catch_latched,
            "dry release stranded the bucket catch");

    scraperx::sim::WaterScrewConfig low_water;
    low_water.initial_basin_volume_m3 = 0.50;
    GroundStage partial_lift({}, low_water);
    require(partial_lift.request_motor_toggle(3.5, 0.0, 0.5),
            "partial-water setup could not start pump");
    run_for(partial_lift, 90 * 45);
    require(partial_lift.request_valve_toggle(3.5, 0.0, 0.5),
            "partial-water valve inaccessible");
    run_for(partial_lift, 90 * 15);
    partial_lift.set_rider_on_cage(true);
    require(partial_lift.request_release(4.0, 0.0, -9.0),
            "partial-water release should be possible");
    run_for(partial_lift, 90 * 5);
    require(partial_lift.lift_state().cage_travel_m < 0.001 &&
                partial_lift.lift_state().bucket_top_catch_latched,
            "insufficient water lifted rider or stranded catch");

    std::cout << "PASS ground screw ascent: local control, finite drive, conserved "
                 "delivery, stall, blocked outlet, reverse, rider lift, catch, "
                 "drain, reset and fixed step\n";
}
