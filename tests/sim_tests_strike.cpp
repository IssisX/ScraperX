// The player's strike (sim/physics_world_strike.cpp): the Action button's
// punch. Offered with her body close in front; thrown, it winds up, reaches
// and recoils; landed, it is a force on her held over its contact ticks and
// drawn from his strike limits (player_strike_limits.hpp), never a velocity
// written. On air it lands nothing. The same inputs give the same strike.

#include "sim_tests.hpp"

#include "sim/player_strike_limits.hpp"

#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

struct Thrown {
    bool offered = false;
    std::uint64_t strikes = 0;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double her_moved_m = 0.0;
    double max_player_step_m = 0.0;
    Vector3 point{};
};

Thrown throw_one(const double her_ahead_m) {
    Simulation s(InitialSpawn::Deck154);
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + her_ahead_m});
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    Thrown out;
    out.offered = s.snapshot().strike_offered;
    const Vector3 her_before = s.matron_state().position;
    (void)s.request_strike();
    Vector3 last = s.snapshot().player_position;
    for (int i = 0; i < 40; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const Vector3 now = s.snapshot().player_position;
        out.max_player_step_m = std::max(out.max_player_step_m,
                                         std::hypot(now.x - last.x, now.y - last.y, now.z - last.z));
        last = now;
    }
    const auto &snap = s.snapshot();
    out.strikes = snap.strike_count;
    out.hits = snap.strike_hit_count;
    out.impulse_ns = snap.strike_impulse_ns;
    out.point = snap.strike_hit_point;
    const Vector3 her_after = s.matron_state().position;
    out.her_moved_m = std::hypot(her_after.x - her_before.x, her_after.z - her_before.z);
    return out;
}
} // namespace

void run_strike() {
    using scraperx::sim::strike::kPlayerStrikeForceN;
    const Thrown close = throw_one(1.05);
    std::cout << "  strike close: offered=" << close.offered << " hits=" << close.hits
              << " impulse_ns=" << close.impulse_ns << " her_moved_m=" << close.her_moved_m
              << " max_step_m=" << close.max_player_step_m << "\n";
    require(close.offered, "strike: Action offers the punch with her body close in front");
    require(close.strikes == 1, "strike: one press throws one strike");
    require(close.hits == 1, "strike: a strike at her body within reach lands once");
    // Force times time: at most the limit held over its 3 contact ticks.
    const double most = kPlayerStrikeForceN * 3.0 * Simulation::kFixedStepSeconds + 1.0e-3;
    require(close.impulse_ns > 0.0 && close.impulse_ns <= most,
            "strike: the landing's impulse is his force held over the contact, within his limit");
    require(close.max_player_step_m <= 0.20, "strike: no tick moves the player more than 0.20 m");

    const Thrown far = throw_one(3.0);
    std::cout << "  strike far: offered=" << far.offered << " hits=" << far.hits << "\n";
    require(!far.offered, "strike: Action does not offer the punch with her out of reach");
    require(far.strikes == 1 && far.hits == 0, "strike: a strike on air lands nothing");

    const Thrown again = throw_one(1.05);
    require(again.hits == close.hits && again.impulse_ns == close.impulse_ns &&
                again.point.x == close.point.x && again.point.y == close.point.y && again.point.z == close.point.z,
            "strike: the same inputs give the same strike, to the bit");
    std::cout << "PASS scraperx_sim strike: impulse_ns=" << close.impulse_ns << "\n";
}
