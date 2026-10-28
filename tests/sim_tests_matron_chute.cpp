// Her parachute and the safety net (src/sim/matron_chute.cpp; Cory 2026-10-27):
// a long fall is flown down to a deck that joins the player's, never to an
// island, and she is still there and after him a minute later. Every force of
// the canopy is on her one account (8,000 N / 22 kW), through Budget::take.
// The net is the last resort: it fires only where her scan cannot plan to his
// last standing region, puts her on a checked spot out of his sight, and a
// restore packs the canopy away.

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr int kHunting = static_cast<int>(Matron::State::Hunting);
constexpr int kSearching = static_cast<int>(Matron::State::Searching);
constexpr int kDead = static_cast<int>(Matron::State::Dead);
constexpr double kForceLawN = 8000.0, kPowerLawW = 22000.0;
constexpr double kLethalMps = 15.34;        // Matron::kLethalLandingMps
constexpr double kAutoDeployByMps = 16.0;   // Cory 16:01 CT

struct Fall {
    bool alive_throughout = true;
    bool after_him = false;
    std::uint32_t chutes = 0, respawns = 0;
    double end_y = 0.0, floor_y = 0.0;
    double landed_floor = -1.0;      // her floor the tick the canopy closed on a landing
    double open_mps = 0.0, peak_force_n = 0.0, peak_power_w = 0.0;
    double respawn_dist = -1.0;      // her distance from him the tick the net put her back
    double settled_drift = -1.0;     // vertical movement in the 2 s after the net
    int end_state = 0;
};

Fall drop_her(const Vector3 at, const double seconds, const bool keep_him_airborne = false) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    // This measures the chute, not a catch: since #11 every catch grabs and
    // spares him, so hold her grab off (as #11 did for its burst and vault legs).
    s.debug_hold_matron_grab(true);
    for (int i = 0; i < 30; ++i) (void)s.advance_frame(Simulation::kFixedStepSeconds);
    s.launch_matron_for_test(at);
    Fall f;
    bool was_open = false;
    std::uint32_t respawns = 0;
    int since_respawn = -1;
    double respawn_y = 0.0;
    const int ticks = static_cast<int>(seconds * Simulation::kTickRateHz);
    for (int i = 0; i < ticks; ++i) {
        // Him in the air the whole flight (a jump, a ladder's hop): he has no
        // standing region, and that alone must not fire the net (finding 3).
        if (keep_him_airborne && (was_open || i < 30) && s.snapshot().player_grounded) {
            s.debug_launch_player(s.snapshot().player_position, Vector3{0.0, 4.0, 0.0});
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        f.alive_throughout = f.alive_throughout && m.alive && m.state != kDead;
        if (was_open && !m.chute_open && m.respawns == respawns && f.landed_floor < 0.0) f.landed_floor = m.floor_y;
        was_open = m.chute_open;
        if (m.respawns != respawns) {
            respawns = m.respawns;
            const auto p = s.snapshot().player_position;
            f.respawn_dist = std::hypot(m.position.x - p.x, m.position.z - p.z);
            since_respawn = 0;
            respawn_y = m.position.y;
        } else if (since_respawn >= 0 && ++since_respawn == static_cast<int>(2.0 * Simulation::kTickRateHz)) {
            f.settled_drift = std::abs(m.position.y - respawn_y);
        }
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
    f.open_mps = m.chute_open_mps;
    f.peak_force_n = m.chute_peak_force_n;
    f.peak_power_w = m.chute_peak_power_w;
    return f;
}

void require_on_account(const Fall &f, const char *what) {
    std::cout << "  matron chute: " << what << " opened at " << f.open_mps << " m/s, peak " << f.peak_force_n
              << " N / " << f.peak_power_w << " W\n";
    require(f.open_mps > 0.0 && f.open_mps < kLethalMps && f.open_mps <= kAutoDeployByMps,
            "matron chute: it opens before the fall is fast enough to kill her (by 16 m/s)");
    require(f.peak_force_n > 0.0 && f.peak_force_n <= kForceLawN + 1.0,
            "matron chute: the canopy's force stays on her 8,000 N account");
    require(f.peak_power_w <= kPowerLawW + 1.0, "matron chute: the canopy's power stays on her 22 kW account");
}
} // namespace

void run_matron_chute() {
    // 1. Off the tower's west side 33 m over the 154 deck: without a chute a
    // ~33 m fall lands at ~25 m/s, a death. She opens it, flies to the deck
    // and is after him a minute on.
    {
        const Fall f = drop_her(Vector3{-30.0, 190.0, -128.0}, 60.0);
        std::cout << "  matron chute: west drop chutes=" << f.chutes << " respawns=" << f.respawns
                  << " landed y=" << f.landed_floor << " end y=" << f.end_y << " state=" << f.end_state << '\n';
        require(f.chutes >= 1, "matron chute: a long fall opens her parachute");
        require_on_account(f, "west drop");
        require(f.respawns == 0, "matron chute: with a deck in reach, the safety net never fires");
        require(f.alive_throughout, "matron chute: she lives through the fall");
        require(std::abs(f.landed_floor - 154.0) < 0.5, "matron chute: she lands on the deck that joins the player's");
        require(f.after_him, "matron chute: a minute later she is still after the player");
    }
    // 2. The same drop with him in the air throughout (finding 3).
    {
        const Fall f = drop_her(Vector3{-30.0, 190.0, -128.0}, 30.0, true);
        std::cout << "  matron chute: west drop, him airborne: respawns=" << f.respawns << " landed y=" << f.landed_floor
                  << '\n';
        require(f.chutes >= 1 && f.respawns == 0,
                "matron chute: him mid-jump or on a ladder does not make her landing the last resort");
        require(std::abs(f.landed_floor - 154.0) < 0.5, "matron chute: him airborne, she still lands on his deck");
    }
    // 3. Out over open air where nothing in glide reach joins the player's
    // decks: the net puts her on a checked spot a deck below him, out of sight.
    {
        const Fall f = drop_her(Vector3{0.0, 175.0, -100.0}, 60.0);
        std::cout << "  matron chute: open-air drop chutes=" << f.chutes << " respawns=" << f.respawns
                  << " end y=" << f.end_y << " floor=" << f.floor_y << " respawn dist=" << f.respawn_dist
                  << " settled drift=" << f.settled_drift << " state=" << f.end_state << '\n';
        require(f.chutes >= 1, "matron chute: open air, the parachute opens");
        require_on_account(f, "open-air drop");
        require(f.respawns == 1, "matron chute: no joined deck below, the safety net fires once");
        require(f.alive_throughout, "matron chute: present the whole minute");
        require(f.floor_y > 120.0 && f.floor_y < 154.0,
                "matron chute: the net puts her about a deck below the player, not on the island");
        require(f.respawn_dist >= 8.0, "matron chute: the net does not put her at his feet");
        require(f.settled_drift >= 0.0 && f.settled_drift < 0.1,
                "matron chute: the net's spot is a real floor she stands on (no mid-air, no fall)");
    }
    // 4. A restore mid-flight (finding 5): the checkpoint packs her canopy.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const std::vector<std::uint8_t> save = s.save_game();
        s.launch_matron_for_test(Vector3{-30.0, 190.0, -128.0});
        bool opened = false;
        for (int i = 0; i < static_cast<int>(3.0 * Simulation::kTickRateHz) && !opened; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            opened = s.matron_state().chute_open;
        }
        require(opened, "matron chute: open before the restore");
        const std::uint32_t chutes = s.matron_state().chutes;
        require(s.load_game(save.data(), save.size()), "matron chute: the checkpoint loads");
        bool flew = false;
        for (int i = 0; i < static_cast<int>(5.0 * Simulation::kTickRateHz); ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            const auto m = s.matron_state();
            flew = flew || m.chute_open || m.respawn_pending;
        }
        const auto m = s.matron_state();
        std::cout << "  matron chute: restored mid-flight: open=" << m.chute_open << " chutes " << chutes << "->"
                  << m.chutes << " y=" << m.position.y << '\n';
        require(!flew && m.chutes == chutes, "matron chute: a restore packs her canopy; nothing flies after it");
        require(m.alive && m.state != kDead, "matron chute: restored, she is there");
    }
}
