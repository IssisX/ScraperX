// PE 3, gap 18 (rule 51): a wrecked machine leaves a way on. Each machine is
// spent the way a player can spend it (sent off with nobody aboard), and the
// way onward is then climbed with the player's own limits, as
// run_plate_wreckage (G/H/I) and run_crane_wreckage (J/K/L) do.
#include "sim_tests.hpp"

#include <cmath>
#include <iostream>

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

void run_wreck_way_on() {
    run_mast_helix_wreckage();
    run_crown_wreckage();
    run_de_wreckage();
    run_s1_wreckage();
    run_m_wreckage();
    run_n_wreckage();
    run_o_wreckage();
}
