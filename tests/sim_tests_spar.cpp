// J-082 set piece 1, the tipping spar (src/sim/vertical/tipping_spar.cpp), on
// player inputs: from TP-340 out along the gangway off its south edge into
// the cabin, Action to let the spar go, the ride over the top, and off along
// the upper gangway onto the 484 ring's south band. 144 m in one ride, no
// ladder, no handle.
#include "sim_tests.hpp"
#include "sim_registry.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::vertical::Route;

constexpr int kSpar = Route::kSparIndex;
constexpr std::uint64_t kSparArm = Route::kSparDynamicFirst;
constexpr std::uint64_t kSparCabin = Route::kSparDynamicFirst + 1;
constexpr double kSparArmKg = 9000.0;
constexpr double kSparCabinKg = 1200.0;
constexpr double kRiderKg = 85.0;
constexpr double kG = 9.81;

double speed_of(const scraperx::sim::Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

bool spar_settled_at(const scraperx::sim::Simulation &s, const double travel) {
    const auto lift = s.lift_state();
    return lift.machine_count > kSpar && std::abs(lift.travels[kSpar] - travel) < 0.005 &&
           speed_of(s.kit_body_velocity(s.kit_body_index(kSparCabin))) < 0.05;
}

// From the PlateTop spawn, north of the header's dump bar, round it west to
// the lower gangway and out along it into the cabin.
bool board_spar(scraperx::sim::Simulation &s) {
    return walk_to(s, 11.0, -137.6, 6.0) && walk_to(s, 7.71, -137.6, 6.0, 0.1) &&
           walk_to(s, 7.71, -128.0, 8.0, 0.1) && walk_to(s, 7.71, -122.3, 6.0, 0.1) &&
           walk_to(s, 9.3, -120.2, 6.0, 0.1) && s.advance_frame(0.5).accepted &&
           s.lift_state().machine == kSpar && s.lift_state().role == 1;
}
} // namespace

// Let go, the spar falls free; the rider brakes it by holding ACTION (the
// held haul input) as a player does: on when the cabin passes 2.2 m/s or nears
// the top, off again under 1.4 m/s.
SparRide spar_ride(scraperx::sim::Simulation &s, const bool brake) {
    SparRide ride;
    ride.boarded = board_spar(s);
    if (!ride.boarded) {
        return ride;
    }
    const double rider_y0 = s.snapshot().player_position.y;
    const double arm_y0 = kit_com_y(s, kSparArm);
    const double cabin_y0 = kit_com_y(s, kSparCabin);
    (void)s.request_lift_action();
    const double start = s.snapshot().simulation_time_seconds;
    bool held = false;
    ride.arrived = wait_for(s, 200.0, [&s, &ride, &held, brake](const scraperx::sim::Snapshot &state) {
        const double cabin = speed_of(s.kit_body_velocity(s.kit_body_index(kSparCabin)));
        ride.peak_speed = std::max(ride.peak_speed, cabin);
        ride.stayed_aboard = ride.stayed_aboard && state.death_count == 0;
        const double travel = s.lift_state().travels[kSpar];
        if (brake) {
            if (cabin > 2.2 || travel > 0.93) {
                held = true;
            } else if (cabin < 1.4) {
                held = false;
            }
            ride.braked_ticks += held ? 1 : 0;
            ride.free_ticks += held ? 0 : 1;
            (void)s.set_lift_haul(held);
        }
        return spar_settled_at(s, 1.0) && state.player_grounded;
    });
    (void)s.set_lift_haul(false);
    ride.ride_s = s.snapshot().simulation_time_seconds - start;
    const auto top = s.snapshot();
    ride.on_cabin_at_top = ride.arrived && s.lift_state().machine == kSpar && s.lift_state().role == 1;
    ride.rider_gain_j = kRiderKg * kG * (top.player_position.y - rider_y0);
    ride.cabin_gain_j = kSparCabinKg * kG * (kit_com_y(s, kSparCabin) - cabin_y0);
    ride.released_j = kSparArmKg * kG * (arm_y0 - kit_com_y(s, kSparArm));
    // Off west along the upper gangway onto the 484 ring's south band.
    ride.on_ring = ride.on_cabin_at_top && walk_to(s, 5.2, -121.0, 6.0, 0.1) && walk_to(s, 5.2, -130.0, 8.0, 0.1) &&
                   walk_to(s, 5.2, -139.6, 8.0, 0.1) && s.advance_frame(0.5).accepted &&
                   standing_above(s.snapshot(), 484.2);
    ride.end = s.snapshot();
    return ride;
}

void run_spar() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    // Left alone, its brake holds it as found.
    {
        Simulation idle(InitialSpawn::PlateTop);
        (void)idle.advance_frame(20.0);
        require(spar_settled_at(idle, 0.0), "spar: left alone 20 s, it must stand as found, its cabin at TP-340");
    }
    Simulation s(InitialSpawn::PlateTop);
    (void)s.advance_frame(1.0);
    const double start = s.snapshot().simulation_time_seconds;
    const SparRide ride = spar_ride(s, true);
    require(ride.boarded, "spar: across TP-340 and out along the lower gangway into the cabin");
    require(ride.arrived && ride.ride_s > 60.0 && ride.ride_s < 160.0,
            "spar: Action in the cabin must let it topple over and bring the cabin to the top");
    require(ride.stayed_aboard && ride.on_cabin_at_top, "spar: the rider rides the cabin all the way, alive");
    require(ride.peak_speed < 3.5, "spar: held, the brake holds the cabin to its pace");
    require(ride.braked_ticks > 0 && ride.free_ticks > 0, "spar: the rider both lets it fall and brakes it");
    require(ride.rider_gain_j > 0.0 && ride.rider_gain_j + ride.cabin_gain_j <= ride.released_j,
            "spar: rider and cabin never gain more than the counterweight's fall released");
    require(ride.on_ring && ride.end.death_count == 0 && ride.end.player_position.y < 485.6,
            "spar: and off along the upper gangway onto the 484 ring, alive");
    // Never braked, the fall runs away: the cabin goes far past a rider's pace.
    double runaway_mps = 0.0;
    {
        Simulation loose(InitialSpawn::PlateTop);
        (void)loose.advance_frame(1.0);
        require(board_spar(loose), "spar falsifier: into the cabin");
        (void)loose.request_lift_action();
        (void)wait_for(loose, 120.0, [&loose, &runaway_mps](const scraperx::sim::Snapshot &) {
            runaway_mps = std::max(runaway_mps, speed_of(loose.kit_body_velocity(loose.kit_body_index(kSparCabin))));
            return loose.lift_state().travels[kSpar] > 0.999;
        });
        require(runaway_mps > 8.0, "spar falsifier: unbraked, the cabin must run away past 8 m/s");
    }
    std::cout << "PASS scraperx_sim spar: to_484_s=" << ride.end.simulation_time_seconds - start
              << " ride_s=" << ride.ride_s << " ring_y=" << ride.end.player_position.y
              << " peak_mps=" << ride.peak_speed << " braked_s=" << ride.braked_ticks / 90.0
              << " free_s=" << ride.free_ticks / 90.0 << " runaway_mps=" << runaway_mps << " rider_J=" << ride.rider_gain_j
              << " cabin_J=" << ride.cabin_gain_j << " released_J=" << ride.released_j << '\n';
}
SCRAPERX_GROUP("spar", 1100, run_spar);
SCRAPERX_SELECT("spar", run_spar);
