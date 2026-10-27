// The player's strike (sim/physics_world_strike.cpp): the Action button's
// punch. Offered with her body close in front; thrown, it winds up, reaches
// and recoils; landed, it is a force on her held over its contact ticks and
// drawn from his strike limits (player_strike_limits.hpp), never a velocity
// written. On air it lands nothing. The same inputs give the same strike.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/player_strike_limits.hpp"

#include <cmath>
#include <iostream>
#include <vector>

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
    double her_blow_mps = 0.0;
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
        out.her_blow_mps = std::max(out.her_blow_mps, s.matron_state().blow_mps);
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

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

bool go(Simulation &s, const double x, const double z, const bool sprint, const double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        const auto p = s.snapshot().player_position;
        const double dx = x - p.x, dz = z - p.z, l = std::hypot(dx, dz);
        if (l < 0.5) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
            return true;
        }
        (void)s.set_move_input(dx / l, dz / l);
        (void)s.set_facing(dx / l, dz / l);
        (void)s.set_sprint_input(sprint);
        tick(s);
    }
    return false;
}

// As matron grab's: woken, called to a spot north-west of her locker, changed.
void meet_her(Simulation &s, const double x, const double z) {
    s.debug_fix_matron_pace(3);
    require(s.advance_frame(1.0).accepted, "strike: settle");
    require(go(s, 10.0, -128.5, true, 4.0), "strike: a sprint to wake her");
    require(go(s, x, z, false, 30.0), "strike: to the spot");
    (void)s.set_move_input(0.0, 0.0);
    constexpr int kStraining = static_cast<int>(scraperx::sim::Matron::State::Straining);
    constexpr int kEmerging = static_cast<int>(scraperx::sim::Matron::State::Emerging);
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) &&
                    (s.matron_state().state == kStraining || s.matron_state().state == kEmerging);
         ++i) {
        tick(s);
    }
    require(s.set_room_loudness(1.0), "strike: a noise from the spot");
    tick(s);
    require(s.set_room_loudness(0.0), "strike: quiet again");
    s.debug_set_matron_stage(2);
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
    // It moves her: hunting (out of her locker's emergence, when blows are
    // read), her balance reads the blow as the impulse over her mass.
    double blow_mps = 0.0, dv = 0.0;
    std::uint64_t blow_hits = 0;
    {
        Simulation s(InitialSpawn::Deck154);
        for (int i = 0; i < 30; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        constexpr int kHunting = static_cast<int>(scraperx::sim::Matron::State::Hunting);
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && s.matron_state().state != kHunting; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)s.request_strike();
        for (int i = 0; i < 40; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            blow_mps = std::max(blow_mps, s.matron_state().blow_mps);
        }
        blow_hits = s.snapshot().strike_hit_count;
        dv = s.snapshot().strike_impulse_ns / scraperx::sim::Matron::kMassKg;
    }
    std::cout << "  strike blow: hits=" << blow_hits << " her_blow_mps=" << blow_mps << " impulse/mass=" << dv << "\n";
    require(blow_hits == 1 && dv > 0.0 && blow_mps >= 0.8 * dv && blow_mps <= 1.5 * dv + 0.05,
            "strike: her velocity change is about the impulse over her 220 kg");

    // Her hands on him: no punch offered, and a press throws nothing.
    {
        Simulation s(InitialSpawn::Deck154);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        bool held = false;
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !held; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            held = s.snapshot().held_by_matron;
        }
        require(held, "strike: she took hold of him (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        require(!s.snapshot().strike_offered, "strike: Action does not offer the punch while she holds him");
        (void)s.request_strike();
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        require(s.snapshot().strike_count == before, "strike: no strike starts in her hands");
    }

    // A checkpoint load mid-strike: the fist back at guard, nothing lands.
    {
        Simulation s(InitialSpawn::Deck154);
        for (int i = 0; i < 30; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const std::vector<std::uint8_t> saved = s.save_game();
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        (void)s.request_strike();
        for (int i = 0; i < 14; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        require(s.snapshot().strike_phase != 0, "strike: mid-strike before the load (setup)");
        require(s.load_game(saved.data(), saved.size()), "strike: the checkpoint loads");
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto &snap = s.snapshot();
        require(snap.strike_phase == 0 && snap.strike_impulse_ns == 0.0,
                "strike: a checkpoint load mid-strike leaves the fist at guard with no blow");
        // No blow pending in her from before the load: a few ticks on, her
        // balance has taken none.
        for (int i = 0; i < 8; ++i) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const auto m = s.matron_state();
        require(m.staggers == 0 && m.knockdowns == 0 && m.blow_mps == 0.0,
                "strike: a checkpoint load mid-strike carries no pending blow into her balance");
    }
    std::cout << "PASS scraperx_sim strike: impulse_ns=" << close.impulse_ns << "\n";
}
