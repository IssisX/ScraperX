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

void run_wreck_way_on() {
    run_m_wreckage();
    run_n_wreckage();
    run_o_wreckage();
}
