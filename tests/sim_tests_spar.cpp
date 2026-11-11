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

SparRide spar_ride(scraperx::sim::Simulation &s) {
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
    ride.arrived = wait_for(s, 160.0, [&s, &ride](const scraperx::sim::Snapshot &state) {
        ride.peak_speed = std::max(ride.peak_speed, speed_of(state.player_linear_velocity));
        ride.stayed_aboard = ride.stayed_aboard && state.death_count == 0;
        return spar_settled_at(s, 1.0) && state.player_grounded;
    });
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
    const SparRide ride = spar_ride(s);
    require(ride.boarded, "spar: across TP-340 and out along the lower gangway into the cabin");
    require(ride.arrived && ride.ride_s > 60.0 && ride.ride_s < 160.0,
            "spar: Action in the cabin must let it topple over and bring the cabin to the top");
    require(ride.stayed_aboard && ride.on_cabin_at_top, "spar: the rider rides the cabin all the way, alive");
    require(ride.peak_speed < 3.5, "spar: the brake holds the cabin to its pace");
    require(ride.rider_gain_j > 0.0 && ride.rider_gain_j + ride.cabin_gain_j <= ride.released_j,
            "spar: rider and cabin never gain more than the counterweight's fall released");
    require(ride.on_ring && ride.end.death_count == 0 && ride.end.player_position.y < 485.6,
            "spar: and off along the upper gangway onto the 484 ring, alive");
    std::cout << "PASS scraperx_sim spar: to_484_s=" << ride.end.simulation_time_seconds - start
              << " ride_s=" << ride.ride_s << " ring_y=" << ride.end.player_position.y
              << " peak_mps=" << ride.peak_speed << " rider_J=" << ride.rider_gain_j
              << " cabin_J=" << ride.cabin_gain_j << " released_J=" << ride.released_j << '\n';
}
SCRAPERX_GROUP("spar", 1100, run_spar);
SCRAPERX_SELECT("spar", run_spar);
