// PE 3, gap 18 (rule 51): a wrecked machine leaves a way on. Each machine is
// spent the way a player can spend it (sent off with nobody aboard), and the
// way onward is then climbed with the player's own limits, as
// run_plate_wreckage (G/H/I) and run_crane_wreckage (J/K/L) do.
#include "sim_tests.hpp"

#include "sim/vertical/vertical_route.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;

namespace {
constexpr double k662Top = 662.1;
constexpr double kN706Top = 706.1;
constexpr double kO750Top = 750.1;
} // namespace

// S1 stepped on and straight off: the water sends the empty cage up, and
// drained at the foot the bucket brings it back down by itself. The way on
// is S1 itself: boarded again and ridden to deck 2.
void run_s1_wreckage() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(0.5);
    require(board_s1(s) && walk_to(s, 10.0, -116.5, 6.0), "S1 wreck: on its plate and straight off again");
    bool sent_up = false;
    bool back = false;
    for (std::uint32_t tick = 0; tick < 90 * 120 && !back; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.stack_state();
        sent_up = sent_up || state.s1_cage_travel >= kS1Travel - 0.01;
        back = sent_up && std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched && s1_plate_at_rest(s);
    }
    require(sent_up && back, "S1 wreck: the cage sent up empty and back down by itself");
    require(board_s1(s), "S1 wreck: boarded again from the yard");
    const S1Ride ride = ride_s1(s, 90.0);
    require(ride.reached_top && ride.rode_on_cage && s.snapshot().death_count == 0,
            "S1 wreck: ridden to deck 2 after the empty trip, alive");
    std::cout << "PASS scraperx_sim stack wreckage S1: ride_s=" << ride.seconds
              << " rider_y=" << s.snapshot().player_position.y << '\n';
}

// S2 ridden to deck 6 and the rider off: the empty cage must come back down
// to deck 4 by itself, as S1's does, and stay there at rest. The way on is
// S2 itself: the cage back at deck 4, ready to be boarded again.
void run_s2_wreckage() {
    Simulation s(InitialSpawn::Deck4South);
    (void)s.advance_frame(0.5);
    // Slice 1 (J-015): the cart is a tank bolted to the beam, 1500 kg wet.
    const std::uint32_t cart = s.kit_body_index(Simulation::kStackS2CartEntityId);
    const std::uint32_t beam = s.kit_body_index(Simulation::kStackS2BeamEntityId);
    std::cout << "S2 wreck: beam_mass=" << s.kit_body_mass(beam) << " cart_mass=" << s.kit_body_mass(cart)
              << " cart_water=" << s.stack_state().s2_cart_water_kg << '\n';
    require(cart < s.kit_body_count(), "S2 wreck: the cart is built");
    require(std::abs(s.kit_body_mass(beam) - 900.0) < 1.0, "S2 wreck: the beam frame weighs 900 kg");
    require(std::abs(s.kit_body_mass(cart) - 1500.0) < 1.0, "S2 wreck: the cart weighs 1500 kg wet");
    require(std::abs(s.stack_state().s2_cart_water_kg - 1250.0) < 1.0,
            "S2 wreck: the tank holds 1250 kg of water");
    require(shove_s2_bolt(s), "S2 wreck: the bolt shoved out");
    const S2Ride ride = ride_s2(s, 20.0);
    require(ride.reached_top && ride.rode_on_cage, "S2 wreck: ridden to deck 6");
    // The rider is off the cage on deck 6 (ride_s2 walks off). The empty
    // cage must come back down to deck 4 by itself and settle there.
    bool sent_up = false;
    bool back = false;
    for (std::uint32_t tick = 0; tick < 90 * 150 && !back; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.stack_state();
        sent_up = sent_up || state.s2_cage_travel >= 22.0 - 0.01;
        if (tick % (90 * 15) == 0) {
            std::cout << "S2 wreck t=" << tick / 90 << "s cage_travel=" << state.s2_cage_travel
                      << " beam_angle=" << state.s2_beam_angle
                      << " cart_water=" << state.s2_cart_water_kg
                      << " striker=" << state.s2_drain_striker_angle
                      << " pawled=" << (state.s2_drain_pawled ? 1 : 0)
                      << " drained=" << state.s2_drained_kg
                      << " rope_tension=" << state.s2_rope_tension << '\n';
        }
        if (sent_up && std::abs(state.s2_cage_travel) <= 0.02) {
            back = true;
        }
    }
    require(sent_up && back, "S2 wreck: the empty cage came back down to deck 4 by itself");
    // It must stay down at rest, not pass through on its way back up.
    for (std::uint32_t tick = 0; tick < 90 * 10; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        require(std::abs(s.stack_state().s2_cage_travel) <= 0.02,
                "S2 wreck: the returned cage stays down at deck 4");
    }
    std::cout << "PASS scraperx_sim stack wreckage S2: ride_s=" << ride.seconds << '\n';
}

// S3 ridden to deck 12 and the rider off: the car falls, the cage rises,
// and the spent machine stays put. S3 has no empty-return mechanism by
// design — the 3500 kg car holds the 400 kg empty cage up — so the test
// asserts the spent state, not a return: the car down, the cage up, the
// brake tripped and holding, nothing moving for its settle time. The way on
// is the C3 crown trusses and high riser ladder from deck 12.
void run_s3_wreckage() {
    Simulation s(InitialSpawn::Deck8North);
    (void)s.advance_frame(0.5);
    require(load_s3_pan(s), "S3 wreck: the pig loaded, the brake tripped");
    const S3Ride ride = ride_s3(s, 25.0);
    require(ride.reached_top && ride.rode_on_cage, "S3 wreck: ridden to deck 12");
    // The rider is off the cage on deck 12 (ride_s3 walks off). The car has
    // fallen and the cage has risen; the spent machine must sit still.
    bool spent = false;
    for (std::uint32_t tick = 0; tick < 90 * 150 && !spent; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.stack_state();
        spent = state.s3_cage_travel >= 44.0 - 0.01 && state.s3_car_travel <= -44.0 + 0.01;
        if (tick % (90 * 15) == 0) {
            std::cout << "S3 wreck t=" << tick / 90 << "s cage_travel=" << state.s3_cage_travel
                      << " car_travel=" << state.s3_car_travel
                      << " brake_latched=" << (state.s3_brake_latched ? 1 : 0)
                      << " rope_tension=" << state.s3_rope_tension << '\n';
        }
    }
    require(spent, "S3 wreck: the car fell to the foot, the cage rose to the head");
    // The rope is still carrying the fall's transient when the car first
    // reaches the foot — let it die before the spent state is read, or the
    // baseline is the spike, not the hold.
    for (std::uint32_t tick = 0; tick < 90 * 5; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    // The spent state: car down, cage up, brake tripped and holding, nothing
    // moving for its settle time.
    const auto settled = s.stack_state();
    require(std::abs(settled.s3_cage_travel - 44.0) <= 0.1, "S3 wreck: the cage is up at the head");
    require(std::abs(settled.s3_car_travel + 44.0) <= 0.1, "S3 wreck: the car is down at the foot");
    require(!settled.s3_brake_latched, "S3 wreck: the brake stays tripped");
    for (std::uint32_t tick = 0; tick < 90 * 10; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.stack_state();
        require(std::abs(state.s3_cage_travel - settled.s3_cage_travel) <= 0.05,
                "S3 wreck: the spent cage stays up at the head");
        require(std::abs(state.s3_car_travel - settled.s3_car_travel) <= 0.05,
                "S3 wreck: the spent car stays down at the foot");
        require(!state.s3_brake_latched, "S3 wreck: the brake stays tripped");
        require(std::abs(state.s3_rope_tension - settled.s3_rope_tension) <= 500.0,
                "S3 wreck: the spent rope stays in tension");
    }
    std::cout << "PASS scraperx_sim stack wreckage S3: ride_s=" << ride.seconds
              << " cage_travel=" << settled.s3_cage_travel << " car_travel=" << settled.s3_car_travel
              << " rope_tension=" << settled.s3_rope_tension << '\n';
}

// F ridden to TP-340 and the rider off: the header's dump thrown over
// brings the spent platform back down to where the way on exists, and
// re-arms it.
void run_f_dump_return() {
    Simulation f(InitialSpawn::WetFPlatform);
    (void)f.advance_frame(1.0);
    require(couple_wet_hose(f), "F wreck: the hose coupled");
    require(pull_wet_stop_valve(f), "F wreck: the stop valve thrown, coupled");
    require(wait_for(f, 90.0, [&](const scraperx::sim::Snapshot &) {
                return f.wet_state().f_platform_travel >= 41.95;
            }),
            "F wreck: the platform carried to TP-340");
    require(walk_to(f, 13.0, -137.2, 6.0), "F wreck: off the platform onto TP-340");
    require(pull_facing(f, 11.6, -137.3, -1.0, 0.0, Simulation::kWetHeaderHandleEntityId, 0.5, 4.0,
                        [&](const scraperx::sim::Snapshot &) {
                            return f.wet_state().dump_angle > 1.8;
                        }),
            "F wreck: the header's dump thrown");
    require(wait_for(f, 150.0, [&](const scraperx::sim::Snapshot &) {
                const auto w = f.wet_state();
                return w.f_catch_latched && w.f_platform_travel <= 0.05;
            }),
            "F wreck: the dump brought the platform back down and re-armed it");
    std::cout << "PASS scraperx_sim AS-007 wreckage F: platform_travel=" << f.wet_state().f_platform_travel
              << '\n';
}

// M's chock pulled with its shackle free: the reel falls its shaft for
// nothing and the cage stays on TP-640. The way on is the backup ladder from
// TP-640 up through the 662 deck's hatch.
void run_m_wreckage() {
    Simulation m(InitialSpawn::PlateTop640);
    (void)m.advance_frame(1.0);
    require(walk_to(m, -3.0, -144.3, 12.0) && pull_service_chock(m), "M wreck: the chock pulled, the shackle free");
    (void)m.advance_frame(15.0);
    const auto spent = m.service_state();
    require(spent.m_reel_travel > 22.0 && spent.m_cage_travel < 0.05, "M wreck: the reel fallen, the cage left on TP-640");
    const bool walked = walk_to(m, 3.0, -144.3, 10.0) && walk_to(m, 8.9, -144.3, 10.0, 0.1) &&
                        walk_to(m, 10.3, -144.3, 6.0, 0.1) && walk_to(m, 10.3, -146.3, 6.0, 0.1) &&
                        walk_to(m, 8.9, -146.02, 6.0, 0.08);
    const auto at = m.snapshot();
    std::cout << "  M wreck: at the ladder's foot " << walked << " (" << at.player_position.x << ", "
              << at.player_position.y << ", " << at.player_position.z << ") grip " << at.grip_available << '\n';
    require(walked && climb_wet_hold(m, 8.9, -146.02, 0.0, 1.0, false, k662Top + 0.4),
            "M wreck: up the backup ladder through the 662 deck's hatch");
    require(standing_above(m.snapshot(), k662Top - 0.1) && m.snapshot().death_count == 0,
            "M wreck: on the 662 deck past the spent lift, alive");
    std::cout << "PASS scraperx_sim AS-010 wreckage M: reel_travel=" << spent.m_reel_travel
              << " deck_y=" << m.snapshot().player_position.y << '\n';
}

// N's flap opened and the rider out: the cage goes up alone and sits on its
// dogs at the head. The way on is the backup ladder from the 684 deck up
// through the 706 deck's hatch.
void run_n_wreckage() {
    Simulation n(InitialSpawn::Deck684);
    (void)n.advance_frame(1.0);
    require(open_n_flap(n) && walk_to(n, -3.2, -155.0, 3.0) && n.service_state().n_cage_travel < 0.01,
            "N wreck: the flap opened and the rider out onto the 684 deck before the cage moves");
    const NRide alone = ride_service_n(n, false);
    require(alone.settled && n.service_state().n_cage_travel >= 21.1 && n.snapshot().player_position.y < 685.5,
            "N wreck: the cage gone up alone to its head, the rider left on the 684 deck");
    const double spent = n.service_state().n_cage_travel;
    const bool walked = walk_to(n, -3.5, -148.0, 8.0) && walk_to(n, -3.5, -142.4, 8.0, 0.1) && walk_to(n, -2.5, -142.4, 6.0, 0.1) && walk_to(n, -2.5, -139.6, 8.0, 0.1) && walk_to(n, -3.5, -139.6, 6.0, 0.1) && walk_to(n, -3.5, -140.73, 6.0, 0.08);
    const auto at = n.snapshot();
    std::cout << "  N wreck: at the ladder's foot " << walked << " (" << at.player_position.x << ", "
              << at.player_position.y << ", " << at.player_position.z << ") grip " << at.grip_available << '\n';
    const bool climbed = walked && climb_wet_hold(n, -3.5, -140.73, 0.0, -1.0, false, kN706Top + 0.4);
    const auto up = n.snapshot();
    std::cout << "  N wreck: climbed " << climbed << " (" << up.player_position.x << ", " << up.player_position.y << ", "
              << up.player_position.z << ") traversal " << static_cast<int>(up.traversal_state) << " grounded "
              << up.player_grounded << " ledge " << up.ledge_available << '\n';
    require(climbed,
            "N wreck: up the backup ladder through the 706 deck's hatch");
    require(standing_above(n.snapshot(), kN706Top - 0.1) && n.snapshot().death_count == 0,
            "N wreck: on the 706 deck past the spent cage, alive");
    require(std::abs(n.service_state().n_cage_travel - spent) < 0.05, "N wreck: the spent cage stays on its dogs");
    std::cout << "PASS scraperx_sim AS-010 wreckage N: cage_travel=" << spent
              << " deck_y=" << n.snapshot().player_position.y << '\n';
}

// O's ram freed with nobody aboard: the cab goes up alone and stays on its
// dogs at the head. The way on is the ladder from the 728 deck up through the
// 750 deck's hatch.
void run_o_wreckage() {
    Simulation o(InitialSpawn::Deck728);
    (void)o.advance_frame(1.0);
    double freed = -1.0;
    require(pull_o_handle(o, &freed) && walk_to(o, 5.2, -150.0, 3.0, 0.15) && o.service_state().o_cab_travel < 0.01,
            "O wreck: the ram freed and the rider off onto the 728 deck before the cab moves");
    const ORide alone = ride_service_o(o, false, freed);
    require(alone.settled && o.service_state().o_cab_travel >= 21.1 && o.snapshot().player_position.y < 729.5,
            "O wreck: the cab gone up alone to its head, the rider left on the 728 deck");
    const double spent = o.service_state().o_cab_travel;
    require(walk_to(o, 10.7, -155.0, 10.0) && climb_wet_hold(o, 10.7, -157.43, 0.0, -1.0, false, kO750Top + 0.4),
            "O wreck: up the ladder through the 750 deck's hatch");
    require(standing_above(o.snapshot(), kO750Top - 0.1) && o.snapshot().death_count == 0,
            "O wreck: on the 750 deck past the spent cab, alive");
    require(std::abs(o.service_state().o_cab_travel - spent) < 0.05, "O wreck: the spent cab stays on its dogs");
    std::cout << "PASS scraperx_sim AS-010 wreckage O: cab_travel=" << spent
              << " deck_y=" << o.snapshot().player_position.y << '\n';
}

// ---- D-F, the wet band (220 -> 340 m) ---------------------------------------------
// Defined in sim_tests_wet.cpp.
bool seat_wet_spool(Simulation &simulation);

namespace {
// The band's climbing route, 220 -> TP-340, as run_wet_route climbs it
// (copied: that test's own steps, the player's limits).
bool climb_wet_route(Simulation &route) {
    using scraperx::sim::TraversalState;
    if (!(walk_to(route, 3.5, -130.5, 6.0, 0.1) && walk_to(route, 3.5, -132.4, 6.0, 0.1) &&
          climb_wet_hold(route, 2.5, -132.4, 0.0, 1.0, false, 242.5))) {
        std::cout << "  wet route: short of the 242 ring at (" << route.snapshot().player_position.x << ", " << route.snapshot().player_position.y << ", " << route.snapshot().player_position.z << ")\n";
        return false;
    }
    if (!(walk_to(route, -1.0, -131.2, 8.0, 0.1) && climb_wet_hold(route, -1.0, -133.2, 0.0, 1.0, true, 264.5))) {
        std::cout << "  wet route: short of the 264 ring\n";
        return false;
    }
    if (!walk_to(route, 1.2, -131.4, 6.0, 0.08)) {
        return false;
    }
    (void)route.set_facing(0.0, 1.0);
    (void)hold_stick(route, 0.0, -0.4, 0.0, 1.0, 2.0,
                     [](const scraperx::sim::Snapshot &state) { return state.player_position.z <= -132.09; });
    (void)route.advance_frame(0.3);
    if (!route.snapshot().edge_drop_available) {
        std::cout << "  wet route: no drop at the bulkhead\n";
        return false;
    }
    (void)route.request_release();
    if (!wait_for(route, 1.5, [](const scraperx::sim::Snapshot &state) {
            return state.traversal_state == TraversalState::Hanging;
        }) ||
        !hold_stick(route, 1.0, 0.0, 0.0, 1.0, 6.0,
                    [](const scraperx::sim::Snapshot &state) { return state.player_position.x >= 3.45; })) {
        std::cout << "  wet route: the hang under the bulkhead\n";
        return false;
    }
    (void)route.request_jump();
    if (!wait_for(route, 2.5, [](const scraperx::sim::Snapshot &state) { return standing_above(state, 264.5); })) {
        return false;
    }
    if (!(walk_to(route, 5.35, -132.2, 6.0, 0.1) && walk_to(route, 5.35, -134.4, 6.0, 0.1) &&
          walk_to(route, 5.0, -134.4, 6.0, 0.1) && climb_wet_hold(route, 5.0, -134.2, 0.0, 1.0, false, 286.5))) {
        std::cout << "  wet route: short of the 286 ring\n";
        return false;
    }
    if (!(walk_to(route, 4.0, -133.0, 6.0, 0.1) && walk_to(route, 4.0, -135.1, 6.0, 0.1) &&
          climb_wet_hold(route, 3.0, -135.1, 0.0, 1.0, false, 308.5))) {
        std::cout << "  wet route: short of the 308 ring\n";
        return false;
    }
    if (!(walk_to(route, 1.5, -134.0, 6.0, 0.1) && climb_wet_hold(route, 1.5, -135.95, 0.0, 1.0, true, 330.5))) {
        std::cout << "  wet route: short of the 330 ring\n";
        return false;
    }
    if (!climb_wet_hold(route, 3.0, -134.9, 0.0, -1.0, false, 340.5)) {
        std::cout << "  wet route: short of TP-340\n";
        return false;
    }
    return true;
}
} // namespace

// D's spool seated, the rider off its platform before the water lifts it: D
// rises empty to E's doors. E set off with nobody in it (a 55 kg drum on its
// deck, as "E loads" sets it off): the cab blows up the shaft empty. The way
// on past both is the band's climbing route, climbed to TP-340.
void run_de_wreckage() {
    Simulation w(InitialSpawn::Ring220North);
    (void)w.advance_frame(1.0);
    require(seat_wet_spool(w) && walk_to(w, 13.0, -130.2, 4.0, 0.15),
            "D wreck: the spool seated and the rider off the platform");
    const bool d_up = wait_for(w, 120.0, [&](const scraperx::sim::Snapshot &) {
        return w.wet_state().d_platform_travel >= 35.95;
    });
    require(d_up && w.snapshot().player_position.y < 222.0, "D wreck: the platform gone up empty, the rider on the 220 ring");
    std::uint32_t drum = Simulation::kKitNone;
    for (std::uint32_t i = 0; i < w.loose_stock_count() && drum == Simulation::kKitNone; ++i) {
        if (w.loose_stock_piece(i).kind == 3) {
            drum = w.kit_body_index(Simulation::kLooseStockFirstEntity + i);
        }
    }
    require(drum != Simulation::kKitNone, "E wreck: a drum is found");
    w.debug_launch_kit_body(drum, {13.0, 256.4, -136.65}, {0.0, 0.0, 0.0});
    require(wait_for(w, 40.0, [&](const scraperx::sim::Snapshot &) { return w.wet_state().e_cab_travel > 40.0; }),
            "E wreck: the cab blown up the shaft empty");
    require(walk_to(w, 9.0, -129.4, 8.0, 0.1) && walk_to(w, 4.0, -129.2, 8.0, 0.1), "D/E wreck: back along the 220 ring to the route's foot");
    require(climb_wet_route(w) && standing_above(w.snapshot(), 340.4) && w.snapshot().death_count == 0,
            "D/E wreck: the band's route climbed past both spent lifts to TP-340");
    std::cout << "PASS scraperx_sim AS-007 wreckage D+E: d_travel=" << w.wet_state().d_platform_travel
              << " e_travel=" << w.wet_state().e_cab_travel << " top_y=" << w.snapshot().player_position.y << '\n';
}


// ---- The vertical machines (src/sim/vertical) --------------------------------------
// Defined in sim_tests_rides.cpp.
bool ride_to_top(Simulation &s, std::size_t index, double *ride_s);

bool pitman_ride(Simulation &s, double *ride_s);

namespace {
// A machine worked by Action, sent off with nobody aboard and called back:
// from its lower receiver (entry) onto its deck, Action, straight back off
// onto the receiver; the deck goes to its top empty. CALL from the receiver
// brings it back, and it is boarded and ridden to the top, then stepped off
// onto the upper receiver (at `entry` again: these stand one over the other).
bool wreck_and_call(Simulation &s, const std::size_t index, const double ex, const double ez, const double dx,
                    const double dz, const char *name) {
    const auto lift = [&s] { return s.lift_state(); };
    const bool on_entry = walk_to(s, ex, ez, 8.0) && s.advance_frame(0.5).accepted &&
                          lift().machine == static_cast<int>(index) && lift().role == 2;
    const bool on_deck = on_entry && walk_to(s, dx, dz, 6.0) && s.advance_frame(0.5).accepted && lift().role == 1;
    if (!on_deck) {
        std::cout << "  " << name << " wreck: entry " << on_entry << " deck " << on_deck << '\n';
        return false;
    }
    (void)s.request_lift_action();
    const bool off = walk_to(s, ex, ez, 4.0, 0.15) && s.advance_frame(0.3).accepted && lift().role == 2;
    const bool gone = off && wait_for(s, 150.0, [&](const scraperx::sim::Snapshot &) {
                          return std::abs(lift().travels[index] - 1.0) < 0.01 && !lift().moving;
                      });
    if (!gone) {
        std::cout << "  " << name << " wreck: off " << off << " travel " << lift().travels[index] << '\n';
        return false;
    }
    (void)s.request_lift_action();
    const bool back = wait_for(s, 150.0, [&](const scraperx::sim::Snapshot &) {
        return lift().travels[index] < 0.01 && !lift().moving;
    });
    double ride_s = 0.0;
    const bool rode = back && walk_to(s, dx, dz, 6.0) && ride_to_top(s, index, &ride_s) && walk_to(s, ex, ez, 6.0) &&
                      s.advance_frame(0.5).accepted && lift().role == 3;
    std::cout << "  " << name << " wreck: called back " << back << ", ridden " << rode << " in " << ride_s << " s\n";
    return rode && s.snapshot().death_count == 0;
}
} // namespace

// The cascade mast, then the barrel helix: each sent up empty, called back
// from its lower receiver, ridden on (the pitman lift between them as built).
void run_mast_helix_wreckage() {
    Simulation s(InitialSpawn::Ring264West);
    (void)s.advance_frame(1.0);
    require(walk_to(s, -19.5, -150.0, 12.0) && wreck_and_call(s, 0, -16.0, -150.0, -10.65, -150.0, "mast"),
            "mast wreck: sent up empty, called back, ridden to its upper receiver");
    double pitman_s = 0.0;
    require(pitman_ride(s, &pitman_s), "mast/helix wreck: the pitman to 314.89 m");
    require(wreck_and_call(s, 2, -27.0, -149.0, -30.8, -149.0, "helix"),
            "helix wreck: sent up empty, called back, ridden to its upper receiver");
    std::cout << "PASS scraperx_sim vertical wreckage mast+helix: top_y=" << s.snapshot().player_position.y << '\n';
}

namespace {
// The incline's helpers, as sim_tests_ground.cpp has them (copied).
constexpr std::uint64_t kInclineDeck = scraperx::sim::vertical::Route::kInclineDynamicFirst;
constexpr std::uint64_t kInclineTrip = kInclineDeck + 1;
constexpr std::uint64_t kInclineBrake = kInclineDeck + 2;
constexpr double kOffBrakeM = 1.0;
constexpr double kNearHeadM = 4.0;
constexpr double kOffBrakeNearM = 0.5;

void incline_hold_on(Simulation &s, std::uint64_t entity, int ticks, double offset_x = 0.0) {
    for (int i = 0; i < ticks; ++i) {
        auto at = s.kit_body_position(s.kit_body_index(entity));
        at.x -= offset_x;
        const auto me = s.snapshot().player_position;
        const double dx = at.x - me.x, dz = at.z - me.z, d = std::hypot(dx, dz);
        const double gain = std::min(1.0, d / 0.3);
        (void)s.set_move_input(d > 0.05 ? gain * dx / d : 0.0, d > 0.05 ? gain * dz / d : 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
}

bool board_incline_trolley(Simulation &s) {
    return walk_to(s, 6.0, -70.0, 20.0) && walk_to(s, -13.4, -86.0, 20.0) && walk_to(s, -13.4, -103.0, 12.0) &&
           s.advance_frame(0.5).accepted && s.lift_state().machine == 5 && s.lift_state().role == 2 &&
           walk_to(s, -10.0, -103.08, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 1;
}
} // namespace

// The slab incline tripped with nobody aboard: on the trip plate, the pin
// pulled, and straight back off onto the boarding platform; the slab sends
// the empty trolley up. The way on: CALL from the boarding platform winds it
// back down and re-arms it, and it is ridden to deck 3 on its brake.
void run_incline_wreckage() {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    require(board_incline_trolley(s), "incline wreck: onto the trolley");
    incline_hold_on(s, kInclineTrip, 60);
    require(s.lift_state().incline_state == 1, "incline wreck: the trip plate pulls the pin");
    require(walk_to(s, -13.4, -103.0, 4.0, 0.15) && s.advance_frame(0.3).accepted && s.lift_state().role == 2,
            "incline wreck: straight back off onto the boarding platform");
    require(wait_for(s, 60.0, [&s](const scraperx::sim::Snapshot &) {
                return s.lift_state().travels[5] > 0.99 && s.lift_state().incline_speed_mps < 0.05;
            }),
            "incline wreck: the trolley gone up empty");
    (void)s.request_lift_action();
    require(wait_for(s, 180.0, [&s](const scraperx::sim::Snapshot &) {
                return s.lift_state().travels[5] < 0.005 && s.lift_state().incline_state == 0;
            }),
            "incline wreck: CALL from the boarding platform winds it down and re-arms it");
    require(walk_to(s, -10.0, -103.08, 6.0) && s.advance_frame(0.5).accepted && s.lift_state().role == 1,
            "incline wreck: back onto the trolley");
    incline_hold_on(s, kInclineTrip, 60);
    const double length = scraperx::sim::vertical::Route::kInclineSlabTravel;
    for (int i = 0; i < 90 * 60; ++i) {
        const auto lift = s.lift_state();
        const double remaining = (1.0 - lift.travels[5]) * length;
        const double allowed = std::min(2.5, std::sqrt(2.0 * 0.8 * std::max(0.0, remaining - 0.5)) + 0.2);
        if (lift.travels[5] > 0.995 && lift.incline_speed_mps < 0.05) break;
        incline_hold_on(s, kInclineBrake, 1,
                        lift.incline_speed_mps > allowed ? 0.0 : remaining < kNearHeadM ? kOffBrakeNearM : kOffBrakeM);
    }
    require(s.lift_state().travels[5] > 0.995 && walk_to(s, -13.4, -120.4, 6.0) && walk_to(s, -13.4, -126.0, 8.0) &&
                s.advance_frame(1.0).accepted && s.snapshot().death_count == 0 &&
                s.snapshot().player_position.y > 33.7,
            "incline wreck: ridden on its brake to the head and onto deck 3, alive");
    std::cout << "PASS scraperx_sim vertical wreckage incline: deck3_y=" << s.snapshot().player_position.y << '\n';
}

// The crown gondola sent off with nobody aboard: Action from its cabin, the
// rider straight back off onto the lower receiver; the cabin carries itself
// over the crown empty. The way on: CALL from the receiver brings it back,
// boarded and ridden to the top.
void run_crown_wreckage() {
    constexpr std::size_t kCrown = 3;
    Simulation s(InitialSpawn::Deck750);
    (void)s.advance_frame(1.0);
    require(walk_to(s, 10.8, -152.0, 8.0) && walk_to(s, 11.3, -152.0, 3.0, 0.06), "crown wreck: to the east parapet");
    (void)s.set_facing(1.0, 0.0);
    (void)s.advance_frame(0.3);
    (void)s.request_traversal();
    require(hold_stick(s, 0.0, 0.0, 1.0, 0.0, 2.0,
                       [](const scraperx::sim::Snapshot &state) {
                           return state.traversal_state == scraperx::sim::TraversalState::None &&
                                  state.player_grounded && state.player_position.x > 12.4;
                       }) &&
                walk_to(s, 13.5, -152.0, 4.0) && walk_to(s, 17.3, -152.0, 6.0) && s.advance_frame(0.5).accepted &&
                s.lift_state().machine == static_cast<int>(kCrown) && s.lift_state().role == 1,
            "crown wreck: over the parapet onto the cabin");
    (void)s.request_lift_action();
    const bool off = walk_to(s, 13.5, -152.0, 4.0, 0.15) && s.advance_frame(0.3).accepted;
    const auto at = s.lift_state();
    std::cout << "  crown wreck: off " << off << " machine " << at.machine << " role " << at.role << " travel "
              << at.travels[kCrown] << '\n';
    require(off && at.role == 2, "crown wreck: sent, and straight back off onto the lower receiver");
    require(wait_for(s, 120.0, [&s](const scraperx::sim::Snapshot &) {
                const auto lift = s.lift_state();
                return std::abs(lift.travels[kCrown] - 1.0) < 0.01 && !lift.moving;
            }),
            "crown wreck: the cabin gone over the crown empty");
    (void)s.request_lift_action();
    require(wait_for(s, 120.0, [&s](const scraperx::sim::Snapshot &) {
                const auto lift = s.lift_state();
                return lift.travels[kCrown] < 0.01 && !lift.moving;
            }),
            "crown wreck: CALL from the lower receiver brings the cabin back");
    double ride_s = 0.0;
    require(walk_to(s, 17.3, -152.0, 6.0) && ride_to_top(s, kCrown, &ride_s) && s.snapshot().death_count == 0,
            "crown wreck: boarded and ridden over the crown");
    std::cout << "PASS scraperx_sim vertical wreckage crown: ride_s=" << ride_s
              << " top_y=" << s.snapshot().player_position.y << '\n';
}

// S2's save from before the drain fix, taken mid-ride: the beam past 0.6 rad,
// the cage past 8 m, the tank still full. Loaded into a fresh world, the
// checkpoint restores where it was — the beam and the cage where they were,
// the tank full on the arm — and the ride finishes: the cage to deck 6, the
// empty cage back to deck 4 by itself, and stays there at rest.
void run_s2_upgrade() {
    Simulation s(InitialSpawn::Deck4South);
    (void)s.advance_frame(0.5);
    require(shove_s2_bolt(s), "S2 upgrade: the bolt shoved out");
    bool halfway = false;
    double beam0 = 0.0;
    double cage0 = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 30 && !halfway; ++tick) {
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_facing(0.0, -1.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.stack_state();
        beam0 = state.s2_beam_angle;
        cage0 = state.s2_cage_travel;
        halfway = beam0 >= 0.6 && cage0 >= 8.0;
    }
    require(halfway, "S2 upgrade: halfway up, the beam past 0.6 rad, the cage past 8 m");
    const auto mid = s.stack_state();
    require(s.snapshot().support_entity_id == Simulation::kStackS2CageEntityId,
            "S2 upgrade: the rider is aboard the cage at the save");
    require(std::abs(mid.s2_cart_water_kg - 1250.0) < 1.0 && !mid.s2_drain_pawled,
            "S2 upgrade: halfway, the tank still full, its gate shut");
    const std::vector<std::uint8_t> bytes = s.save_game();
    const std::vector<std::uint8_t> parent = parent_save(bytes, s);
    Simulation loaded(InitialSpawn::Deck4South);
    require(loaded.load_game(parent.data(), parent.size()),
            "S2 upgrade: a save from before the drain fix loads mid-ride");
    const auto at = loaded.stack_state();
    std::cout << "S2 upgrade: halfway at beam " << beam0 << " cage " << cage0 << "; loaded at beam "
              << at.s2_beam_angle << " cage " << at.s2_cage_travel << " water " << at.s2_cart_water_kg << '\n';
    require(std::abs(at.s2_beam_angle - beam0) < 0.05 && std::abs(at.s2_cage_travel - cage0) < 0.2 &&
                std::abs(at.s2_cart_water_kg - 1250.0) < 1.0 && !at.s2_drain_pawled,
            "S2 upgrade: loaded mid-ride, the beam and the cage where they were, the tank full, its gate shut");
    bool top = false;
    for (std::uint32_t tick = 0; tick < 90 * 30 && !top; ++tick) {
        (void)loaded.set_move_input(0.0, 0.0);
        (void)loaded.set_facing(0.0, -1.0);
        (void)loaded.advance_frame(Simulation::kFixedStepSeconds);
        top = loaded.stack_state().s2_cage_travel >= 22.0 - 0.10;
    }
    require(top && loaded.snapshot().death_count == 0, "S2 upgrade: the loaded ride reaches deck 6, alive");
    bool back = false;
    for (std::uint32_t tick = 0; tick < 90 * 150 && !back; ++tick) {
        (void)loaded.advance_frame(Simulation::kFixedStepSeconds);
        if (std::abs(loaded.stack_state().s2_cage_travel) <= 0.02) {
            back = true;
        }
    }
    require(back, "S2 upgrade: the loaded cage came back down to deck 4 by itself");
    for (std::uint32_t tick = 0; tick < 90 * 10; ++tick) {
        (void)loaded.advance_frame(Simulation::kFixedStepSeconds);
        require(std::abs(loaded.stack_state().s2_cage_travel) <= 0.02,
                "S2 upgrade: the returned cage stays down at deck 4");
    }
    std::cout << "PASS scraperx_sim stack wreckage S2 upgrade: beam0=" << beam0 << " cage0=" << cage0 << '\n';
}

// The same save from before the drain, loaded into a world that has ridden
// S2 since and drained its tank (J-015 #466 Codex P1): the save has no tank,
// so the tank is full as built, not as dry as the running world's, its gate
// shut as built. The beam and the cage are where the save has them, and the
// loaded ride finishes: the cage to deck 6, then home to deck 4 by itself.
// (Filled from the running world, the tank came back dry with the cage
// halfway up.)
void run_s2_upgrade_after_drain() {
    Simulation s(InitialSpawn::Deck4South);
    (void)s.advance_frame(0.5);
    require(shove_s2_bolt(s), "S2 upgrade after the drain: the bolt shoved out");
    bool halfway = false;
    double beam0 = 0.0;
    double cage0 = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 30 && !halfway; ++tick) {
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_facing(0.0, -1.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = s.stack_state();
        beam0 = state.s2_beam_angle;
        cage0 = state.s2_cage_travel;
        halfway = beam0 >= 0.6 && cage0 >= 8.0;
    }
    require(halfway, "S2 upgrade after the drain: halfway up, the beam past 0.6 rad, the cage past 8 m");
    require(s.snapshot().support_entity_id == Simulation::kStackS2CageEntityId,
            "S2 upgrade after the drain: the rider is aboard the cage at the save");
    require(std::abs(s.stack_state().s2_cart_water_kg - 1250.0) < 1.0 && !s.stack_state().s2_drain_pawled,
            "S2 upgrade after the drain: halfway, the tank still full, its gate shut");
    const std::vector<std::uint8_t> parent = parent_save(s.save_game(), s);
    // The cage to deck 6 with the rider on it, then home to deck 4 by itself.
    const auto finish = [&](const char *what) {
        bool top = false;
        for (std::uint32_t tick = 0; tick < 90 * 30 && !top; ++tick) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_facing(0.0, -1.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            top = s.stack_state().s2_cage_travel >= 22.0 - 0.10;
        }
        bool back = false;
        for (std::uint32_t tick = 0; top && tick < 90 * 150 && !back; ++tick) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            back = std::abs(s.stack_state().s2_cage_travel) <= 0.02;
        }
        const auto state = s.stack_state();
        std::cout << "S2 upgrade after the drain: " << what << " top " << top << " back " << back << " cage "
                  << state.s2_cage_travel << " beam " << state.s2_beam_angle << " water "
                  << state.s2_cart_water_kg << " pawled " << (state.s2_drain_pawled ? 1 : 0) << '\n';
        return top && back && s.snapshot().death_count == 0;
    };
    // The same world rides on and drains its tank.
    require(finish("the first ride"), "S2 upgrade after the drain: the first ride reaches deck 6 and comes home");
    require(s.stack_state().s2_cart_water_kg < 1.0,
            "S2 upgrade after the drain: the running world's tank is drained before the load");
    require(s.load_game(parent.data(), parent.size()),
            "S2 upgrade after the drain: a save from before the drain loads into the drained world");
    const auto at = s.stack_state();
    std::cout << "S2 upgrade after the drain: halfway at beam " << beam0 << " cage " << cage0 << "; loaded at beam "
              << at.s2_beam_angle << " cage " << at.s2_cage_travel << " water " << at.s2_cart_water_kg
              << " pawled " << (at.s2_drain_pawled ? 1 : 0) << '\n';
    require(std::abs(at.s2_cart_water_kg - 1250.0) < 1.0,
            "S2 upgrade after the drain: loaded, the tank holds 1250 kg as built, not the drained world's water");
    require(std::abs(at.s2_beam_angle - beam0) < 0.05 && std::abs(at.s2_cage_travel - cage0) < 0.2 &&
                !at.s2_drain_pawled,
            "S2 upgrade after the drain: loaded mid-ride, the beam and the cage where they were, the gate shut");
    require(finish("the loaded ride"),
            "S2 upgrade after the drain: the loaded ride reaches deck 6 and comes home to deck 4, alive");
    for (std::uint32_t tick = 0; tick < 90 * 10; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        require(std::abs(s.stack_state().s2_cage_travel) <= 0.02,
                "S2 upgrade after the drain: the returned cage stays down at deck 4");
    }
    std::cout << "PASS scraperx_sim stack wreckage S2 upgrade after the drain: beam0=" << beam0
              << " cage0=" << cage0 << '\n';
}

// A real save from the build before the drain (trunk 74d84b25, J-015 #389
// Codex P1): S2 ridden to deck 6, the rider still on the cage, saved 10 s
// after it arrived (tests/data/s2_top_before_drain.save). That build has no
// tank and no striker, so the tank here is full at the lower station from
// the first tick, where the striker stands. Loaded, the cage must not stay
// held up: the tank drains and the cage comes home to deck 4, as after a
// ride in this build. (With the striker started as built it was caught in
// the tank and the cage stayed at 21.98 m for 150 s, the tank dry.)
void run_s2_old_save_top() {
#ifndef SCRAPERX_SOURCE_DIR
#error "SCRAPERX_SOURCE_DIR must name the repository (CMakeLists.txt sets it)"
#endif
    std::ifstream in(std::string(SCRAPERX_SOURCE_DIR) + "/tests/data/s2_top_before_drain.save", std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    require(bytes.size() > 1000, "S2 old save at the top: the save from the build before the drain is read");
    Simulation s(InitialSpawn::ExteriorGrade);
    require(s.load_game(bytes.data(), bytes.size()), "S2 old save at the top: it loads");
    const auto loaded = s.stack_state();
    std::cout << "S2 old save at the top: loaded cage " << loaded.s2_cage_travel << " beam " << loaded.s2_beam_angle
              << " water " << loaded.s2_cart_water_kg << " rider_y " << s.snapshot().player_position.y << '\n';
    require(loaded.s2_cage_travel > 21.9 && loaded.s2_beam_angle > 1.6 &&
                std::abs(loaded.s2_cart_water_kg - 1250.0) < 1.0 &&
                s.snapshot().player_position.y > 66.5,
            "S2 old save at the top: the cage at deck 6 with the rider on it, the tank full as built");
    bool home = false;
    double home_s = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !home; ++tick) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        home = std::abs(s.stack_state().s2_cage_travel) <= 0.02;
        home_s = static_cast<double>(tick + 1) / 90.0;
    }
    const auto end = s.stack_state();
    std::cout << "S2 old save at the top: home " << home << " in " << home_s << " s, water " << end.s2_cart_water_kg
              << " cage " << end.s2_cage_travel << " beam " << end.s2_beam_angle << '\n';
    require(home && end.s2_cart_water_kg < 1.0 && s.snapshot().death_count == 0,
            "S2 old save at the top: the tank drained and the cage home at deck 4, the rider alive");
    std::cout << "PASS scraperx_sim stack wreckage S2 old save at the top: home_s=" << home_s << '\n';
}

void run_wreck_way_on() {
    run_incline_wreckage();
    run_mast_helix_wreckage();
    run_crown_wreckage();
    run_de_wreckage();
    run_s1_wreckage();
    run_s2_wreckage();
    run_s2_upgrade();
    run_s2_upgrade_after_drain();
    run_s2_old_save_top();
    run_s3_wreckage();
    run_m_wreckage();
    run_n_wreckage();
    run_o_wreckage();
    run_f_dump_return();
}
SCRAPERX_GROUP("wreck_way_on", 350, run_wreck_way_on);
SCRAPERX_SELECT("wreck-way-on", run_wreck_way_on);
SCRAPERX_SELECT("s2-upgrade-after-drain", run_s2_upgrade_after_drain);
