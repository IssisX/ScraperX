// Her parachute and the safety net (src/sim/matron_chute.cpp; Cory 2026-10-27):
// a long fall is flown down to a deck that joins the player's, never to an
// island, and she is still there and after him a minute later. Every force of
// the canopy is on her one account (8,000 N / 22 kW).

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr int kHunting = static_cast<int>(Matron::State::Hunting);
constexpr int kSearching = static_cast<int>(Matron::State::Searching);
constexpr int kDead = static_cast<int>(Matron::State::Dead);

struct Fall {
    bool alive_throughout = true;
    bool after_him = false;     // hunting or searching at the end
    std::uint32_t chutes = 0, respawns = 0;
    double end_y = 0.0, floor_y = 0.0, landed_floor = -1.0;
    int end_state = 0;
};

Fall drop_her(const Vector3 at, const double seconds) {
    Simulation s(InitialSpawn::Deck154);
    for (int i = 0; i < 30; ++i) (void)s.advance_frame(Simulation::kFixedStepSeconds);
    s.launch_matron_for_test(at);
    Fall f;
    bool was_open = false;
    const int ticks = static_cast<int>(seconds * Simulation::kTickRateHz);
    for (int i = 0; i < ticks; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        f.alive_throughout = f.alive_throughout && m.alive && m.state != kDead;
        if (was_open && !m.chute_open) f.landed_floor = m.position.y;
        was_open = m.chute_open;
        // Pursuit is a lead she keeps: any tick in the last 20 s after him counts.
        if (i > ticks - static_cast<int>(20.0 * Simulation::kTickRateHz) &&
            (m.state == kHunting || m.state == kSearching)) {
            f.after_him = true;
        }
    }
    const auto m = s.matron_state();
    f.chutes = m.chutes;
    f.respawns = m.respawns;
    f.end_y = m.position.y;
    f.floor_y = m.floor_y;
    f.end_state = m.state;
    return f;
}
} // namespace

void run_matron_chute() {
    // 1. Off the tower's west side 33 m over the 154 deck: no chute, that is a
    // death (a ~33 m fall lands at ~25 m/s). She opens it, flies to the deck
    // and is after him a minute on.
    {
        const Fall f = drop_her(Vector3{-30.0, 190.0, -128.0}, 60.0);
        std::cout << "  matron chute: west drop chutes=" << f.chutes << " respawns=" << f.respawns
                  << " landed y=" << f.landed_floor << " end y=" << f.end_y << " state=" << f.end_state << '\n';
        require(f.chutes >= 1, "matron chute: a long fall opens her parachute");
        require(f.respawns == 0, "matron chute: with a deck in reach, the safety net never fires");
        require(f.alive_throughout, "matron chute: she lives through the fall");
        require(std::abs(f.floor_y - 154.0) < 0.5, "matron chute: she lands on the deck that joins the player's");
        require(f.after_him, "matron chute: a minute later she is still after the player");
    }
    // 2. Out over open air where nothing in glide reach joins the player's
    // decks (grade, an island to her): she must not be left there. The net puts
    // her a deck below him, joined to his, and logs it.
    {
        const Fall f = drop_her(Vector3{0.0, 175.0, -100.0}, 60.0);
        std::cout << "  matron chute: open-air drop chutes=" << f.chutes << " respawns=" << f.respawns
                  << " end y=" << f.end_y << " floor=" << f.floor_y << " state=" << f.end_state << '\n';
        require(f.chutes >= 1, "matron chute: open air, the parachute opens");
        require(f.respawns == 1, "matron chute: no joined deck below, the safety net fires once");
        require(f.alive_throughout, "matron chute: present the whole minute");
        require(f.floor_y > 120.0 && f.floor_y < 154.0,
                "matron chute: the net puts her about a deck below the player, not on the island");
    }
}
