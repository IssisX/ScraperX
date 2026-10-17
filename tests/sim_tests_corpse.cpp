// Corpses (sim/corpse.hpp): a lethal fall leaves the body where it hit, a
// ragdoll that comes to rest there, while the climber is back at the
// checkpoint; each death leaves another; at most Corpses::kMaxCorpses lie in
// the world; a loaded game has none.

#include "sim_tests.hpp"

#include "sim/corpse.hpp"

#include <cmath>
#include <iostream>

using scraperx::sim::Corpses;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;

namespace {

// Off deck 4's south edge at a walk: down until the fall kills.
bool walk_off_deck_4(Simulation &s) {
    const std::uint64_t deaths = s.snapshot().death_count;
    (void)s.set_facing(0.0, 1.0);
    (void)s.set_move_input(0.0, 1.0);
    const bool died = advance_until(s, [deaths](const Snapshot &n) { return n.death_count > deaths; }, 20.0);
    (void)s.set_move_input(0.0, 0.0);
    return died;
}

} // namespace

void run_corpse() {
    using scraperx::sim::InitialSpawn;
    Simulation s(InitialSpawn::Deck4South);
    require(s.advance_frame(0.5).accepted && s.snapshot().corpse_count == 0, "corpse: none before a death");
    require(walk_off_deck_4(s), "corpse: walking off deck 4 must kill");
    const Snapshot dead = s.snapshot();
    std::cout << "  corpse: died at " << dead.death_speed_mps << " m/s, hit at (" << dead.death_point.x << ", "
              << dead.death_point.y << ", " << dead.death_point.z << "), the climber back at (" << dead.player_position.x
              << ", " << dead.player_position.y << ", " << dead.player_position.z << ")\n";
    require(dead.corpse_count == 1 && dead.corpses_spawned == 1 && dead.death_speed_mps > 15.0,
            "corpse: the death leaves one body");
    require(dead.player_position.y > dead.death_point.y + 20.0, "corpse: the climber is back at the checkpoint, up on the deck");
    // It comes to rest where it hit, lying, not standing.
    require(advance_until(s, [&s](const Snapshot &) { return s.corpse_resting(0); }, 6.0),
            "corpse: the body comes to rest");
    const auto head = s.corpse_part(0, Corpses::kHead);
    const auto pelvis = s.corpse_part(0, Corpses::kPelvis);
    const double spread = std::hypot(head.position.x - dead.death_point.x, head.position.z - dead.death_point.z);
    std::cout << "  corpse: at rest, head at (" << head.position.x << ", " << head.position.y << ", " << head.position.z
              << "), pelvis y " << pelvis.position.y << ", " << spread << " m from the hit\n";
    require(head.position.y < dead.death_point.y + 0.6 && pelvis.position.y < dead.death_point.y + 0.6 && spread < 3.0,
            "corpse: it lies where the body hit, not standing");
    // Another death, another body; past the cap the oldest goes.
    for (int death = 2; death <= Corpses::kMaxCorpses + 1; ++death) {
        require(walk_off_deck_4(s), "corpse: walking off again must kill again");
        const Snapshot n = s.snapshot();
        require(n.corpses_spawned == static_cast<std::uint64_t>(death) &&
                    n.corpse_count == static_cast<std::uint32_t>(std::min(death, Corpses::kMaxCorpses)),
                "corpse: each death leaves a body, up to the cap");
    }
    // Saved and loaded: the world comes back without them.
    const auto bytes = s.save_game();
    require(s.load_game(bytes.data(), bytes.size()) && s.snapshot().corpse_count == 0,
            "corpse: a loaded game has no corpses");
    std::cout << "PASS scraperx_sim corpse: death_mps=" << dead.death_speed_mps << " rest_head_y=" << head.position.y
              << " hit_y=" << dead.death_point.y << " capped=" << Corpses::kMaxCorpses << "\n";
}
