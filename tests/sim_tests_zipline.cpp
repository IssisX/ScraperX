// Ziplines (sim/zipline.hpp): the yard line from deck 2's south band down to
// the yard, and a line across the well on nearly every level. Ridden: up the
// stand behind the top post (up its ramp), a jump with the hands up takes the
// parked trolley's bar; the body rides down under its own weight, fast. With
// the stick held back near the end the hand brake brings it into the buffer
// slowly, the buffer stops it short of the stop and the hands hold; DROP
// lets go onto the yard, the trolley left at the bottom. Ridden flat out,
// the stop's jolt tears the hands off the bar and the body goes on into the
// bottom post. Ridden again, JUMP OFF midway throws the body on off the line.

#include "sim_tests.hpp"

#include <cmath>
#include <iostream>

using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;

namespace {

// Up the stand behind the yard line's top post (its ramp runs up from the
// north, away from the line) at a run, and the jump south at the parked
// trolley from the stand: riding, or not. The run over the ramp's crest
// lifts the feet off the stand for a few ticks (2 m/s up, not a reach), so
// the jump pressed there is a coyote jump (kCoyoteTicks): without it the
// body hops past under the bar and off the deck.
bool catch_the_yard_line(Simulation &s) {
    require(s.advance_frame(1.0).accepted, "zipline: settle");
    require(walk_to(s, -2.0, -129.4, 20.0, 0.12), "zipline: to the ramp's foot");
    // Stopped at the foot, then straight up it: walk_to hands over on
    // arrival, still moving, and a run turned up the 1 m ramp with that
    // sideways drift carries the body into the post beside the stand.
    (void)s.set_move_input(0.0, 0.0);
    require(s.advance_frame(0.4).accepted, "zipline: stopped at the ramp's foot");
    (void)s.set_facing(0.0, 1.0);
    (void)s.set_move_input(0.0, 1.0);
    require(advance_until(s, [](const Snapshot &n) { return n.player_position.z > -126.4; }, 3.0),
            "zipline: up the ramp onto the stand");
    const Snapshot on_stand = s.snapshot();
    std::cout << "  zipline: on the stand at y " << on_stand.player_position.y << ", grounded "
              << on_stand.player_grounded << "\n";
    require(on_stand.player_position.y > 23.9, "zipline: the stand is climbed");
    require(s.request_jump(), "zipline: the jump at the trolley");
    return advance_until(s, [](const Snapshot &n) { return n.zipline_riding; }, 1.2);
}

// Rides the caught line to its end: the stick held back (the hand brake)
// from `brake_from_end` metres out while faster than `brake_to`, never when
// brake_from_end is 0. Ends when the trolley stops near the end or the
// hands leave the bar; the fastest speed down the line.
double ride_to_the_end(Simulation &s, const double length, const double brake_from_end, const double brake_to) {
    double fastest = 0.0;
    int quiet = 0;
    for (int tick = 0; tick < 15 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        const Snapshot n = s.snapshot();
        const bool braking = brake_from_end > 0.0 && n.zipline_along_m > length - brake_from_end &&
                             n.zipline_speed_mps > brake_to;
        (void)s.set_move_input(0.0, braking ? -1.0 : 0.0);
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "zipline: riding");
        const Snapshot m = s.snapshot();
        if (!m.zipline_riding) {
            break;
        }
        fastest = std::max(fastest, m.zipline_speed_mps);
        quiet = m.zipline_along_m > length - 4.0 && std::abs(m.zipline_speed_mps) < 0.3 ? quiet + 1 : 0;
        if (quiet >= 45) {
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    return fastest;
}

} // namespace

void run_zipline() {
    using scraperx::sim::InitialSpawn;
    Simulation s(InitialSpawn::Deck2South);
    std::cout << "  zipline: " << s.zipline_count() << " lines built\n";
    require(s.zipline_count() >= 18, "zipline: the yard line and a line across the well on most levels");
    const auto yard = s.zipline_line(0);
    require(std::abs(yard.top.x + 2.0) < 0.01 && std::abs(yard.bottom.z + 72.0) < 0.01 && yard.length > 50.0,
            "zipline: the yard line runs from deck 2's south band down to the yard");

    // Braked: full brake from 22 m out down to 4 m/s, into the buffer slowly.
    require(catch_the_yard_line(s), "zipline: a jump at the parked trolley takes its bar");
    const Snapshot caught = s.snapshot();
    require(caught.zipline_ride_count == 1 && caught.traversal_state == scraperx::sim::TraversalState::Swinging,
            "zipline: the hands on the bar, the body hung from the trolley");
    const double fastest = ride_to_the_end(s, yard.length, 22.0, 4.0);
    const Snapshot at_stop = s.snapshot();
    std::cout << "  zipline: braked, rode to " << fastest << " m/s, stopped at " << at_stop.zipline_along_m << " m of "
              << yard.length << ", hanging at (" << at_stop.player_position.x << ", " << at_stop.player_position.y << ", "
              << at_stop.player_position.z << ")\n";
    require(fastest > 12.0, "zipline: the slope takes the body to a real speed");
    require(at_stop.zipline_riding && at_stop.zipline_along_m > yard.length - 4.0 && at_stop.hang_slip_count == 0 &&
                at_stop.zipline_torn_count == 0 && at_stop.death_count == 0,
            "zipline: braked, the buffer stops the trolley at the end and the hands hold");
    require(s.request_release(), "zipline: DROP");
    require(advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 2.0), "zipline: down onto the yard");
    const Snapshot down = s.snapshot();
    std::cout << "  zipline: let go, down at (" << down.player_position.x << ", " << down.player_position.y << ", "
              << down.player_position.z << "), deaths " << down.death_count << "\n";
    require(down.death_count == 0 && !down.zipline_riding && down.player_position.y < 1.5 && down.player_position.z > -77.0,
            "zipline: let go at the bottom, the body lands on the yard under the line");
    const auto ridden = s.zipline_line(0);
    require(!ridden.parked && ridden.trolley_m > ridden.length - 4.0 && ridden.block_m >= ridden.trolley_m - 0.01,
            "zipline: the trolley and the buffer stay where the ride ended");

    // Flat out: the stop's jolt tears the hands off the bar; the body goes on
    // into the bottom post and down beside it.
    Simulation f(InitialSpawn::Deck2South);
    require(catch_the_yard_line(f), "zipline: caught again");
    const double flat_out = ride_to_the_end(f, yard.length, 0.0, 0.0);
    const Snapshot torn = f.snapshot();
    require(advance_until(f, [](const Snapshot &n) { return n.player_grounded || n.death_count > 0; }, 3.0),
            "zipline: torn off, it comes down");
    const Snapshot after = f.snapshot();
    std::cout << "  zipline: flat out at " << flat_out << " m/s, torn " << torn.zipline_torn_count << ", down at ("
              << after.player_position.x << ", " << after.player_position.y << ", " << after.player_position.z << "), deaths "
              << after.death_count << "\n";
    require(flat_out > 18.0 && !torn.zipline_riding && torn.zipline_torn_count == 1 && torn.hang_slip_count == 1,
            "zipline: unbraked, the stop's jolt tears the hands off the bar");
    require(after.player_position.z > yard.bottom.z - 0.8 && after.player_position.z < yard.bottom.z,
            "zipline: torn off, the body goes on into the bottom post and down beside it");

    // Ridden again, JUMP OFF midway throws the body on and off the line.
    Simulation j(InitialSpawn::Deck2South);
    require(catch_the_yard_line(j), "zipline: caught a third time");
    require(j.advance_frame(1.5).accepted, "zipline: a second and a half down the line");
    const Snapshot riding = j.snapshot();
    require(j.request_jump() && j.advance_frame(Simulation::kFixedStepSeconds).accepted, "zipline: JUMP OFF");
    const Snapshot thrown = j.snapshot();
    std::cout << "  zipline: jumped off at " << riding.zipline_speed_mps << " m/s down the line, flying at ("
              << thrown.player_linear_velocity.x << ", " << thrown.player_linear_velocity.y << ", "
              << thrown.player_linear_velocity.z << ")\n";
    require(!thrown.zipline_riding && thrown.swing_launch_count == 1 && thrown.player_linear_velocity.z > 3.0,
            "zipline: JUMP OFF throws the body on, the trolley left behind");
    require(advance_until(j, [](const Snapshot &n) { return n.player_grounded || n.death_count > 0; }, 4.0),
            "zipline: it comes down");
    std::cout << "PASS scraperx_sim zipline: lines=" << s.zipline_count() << " braked_fastest_mps=" << fastest
              << " stopped_m=" << at_stop.zipline_along_m << " flat_out_mps=" << flat_out << " torn_z="
              << after.player_position.z << "\n";
}
