// Her hold (sim/matron_hold.cpp, sim/physics_world_grab.cpp): fully changed,
// her hand on a player is no death. She takes an ankle, carries them upside
// down to the shaft's rail, holds them out over the drop and lets go aimed
// at a ledge, or sets them back on the deck. Her grip is finite: a still
// body hangs; a thrashing one tears free. Nothing teleports.

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <cstdlib>
#include <set>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;

constexpr int kGrab = static_cast<int>(Matron::State::Grab);
constexpr int kCarry = static_cast<int>(Matron::State::Carry);
constexpr int kDangle = static_cast<int>(Matron::State::Dangle);
constexpr int kRelease = static_cast<int>(Matron::State::Release);
constexpr int kSpare = static_cast<int>(Matron::State::Spare);
constexpr int kHunting = static_cast<int>(Matron::State::Hunting);

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

bool steer(Simulation &s, const double x, const double z, const bool sprint) {
    const auto p = s.snapshot().player_position;
    const double dx = x - p.x;
    const double dz = z - p.z;
    const double l = std::hypot(dx, dz);
    if (l < 0.5) {
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        return true;
    }
    (void)s.set_move_input(dx / l, dz / l);
    (void)s.set_facing(dx / l, dz / l);
    (void)s.set_sprint_input(sprint);
    return false;
}

bool go(Simulation &s, const double x, const double z, const bool sprint, const double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        if (steer(s, x, z, sprint)) {
            return true;
        }
        tick(s);
    }
    return false;
}

// Wakes her, walks to the north band west of her locker, fully changed, and
// waits there still for her hand.
void meet_her(Simulation &s, const double x, const double z) {
    s.debug_fix_matron_pace(3);
    require(s.advance_frame(1.0).accepted, "matron grab: settle");
    require(go(s, 10.0, -128.5, true, 4.0), "matron grab: a sprint to wake her");
    require(go(s, x, z, false, 30.0), "matron grab: to the spot");
    (void)s.set_move_input(0.0, 0.0);
    s.debug_set_matron_stage(2);
}

double up_y(const scraperx::sim::Snapshot &p) {
    // The body's up (its local Y) in the world, from its rotation.
    const double x = p.player_rot_x, y = p.player_rot_y, z = p.player_rot_z, w = p.player_rot_w;
    return 1.0 - 2.0 * (x * x + z * z);
    (void)y;
    (void)w;
}

struct HoldRun {
    std::set<int> states;
    bool grabbed = false;
    bool inverted = false;
    double peak_n = 0.0;
    double max_step = 0.0;
    std::uint64_t deaths_held = 0;
    std::uint64_t deaths_start = 0;
    std::uint64_t deaths_end = 0;
    bool ended = false;
    scraperx::sim::MatronSnapshot m;
    scraperx::sim::Snapshot p;
    double first_grab_s = -1.0;
    double torn_s = -1.0;
};

// Runs until her hold is over and the body is back upright (or `seconds`).
template <typename Input>
HoldRun hold_run(Simulation &s, const double seconds, Input input) {
    HoldRun r;
    r.deaths_start = s.snapshot().death_count;
    bool was_held = false;
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        const auto before = s.matron_state();
        input(i, before);
        tick(s);
        const auto m = s.matron_state();
        const auto p = s.snapshot();
        r.states.insert(int(m.state));
        if (p.held_by_matron) {
            if (!r.grabbed) {
                r.first_grab_s = i / 90.0;
            }
            r.grabbed = true;
            r.peak_n = std::max(r.peak_n, p.grip_load_n);
            r.inverted = r.inverted || up_y(p) < -0.8;
            if (p.death_count != r.deaths_start) {
                ++r.deaths_held;
            }
        }
        if (was_held && !p.held_by_matron && p.grip_tear_count > 0 && r.torn_s < 0.0) {
            r.torn_s = i / 90.0;
        }
        was_held = p.held_by_matron;
        if (r.grabbed && !p.held_by_matron && !p.player_free_rotation && m.state != kGrab && m.state != kCarry &&
            m.state != kDangle && m.state != kSpare && m.state != kRelease) {
            // Over: let the landing settle a moment.
            (void)s.advance_frame(3.0);
            r.ended = true;
            break;
        }
    }
    r.m = s.matron_state();
    r.p = s.snapshot();
    r.max_step = r.p.max_hold_step_m;
    r.deaths_end = r.p.death_count;
    return r;
}

void report(const char *name, const HoldRun &r) {
    std::cout << "  " << name << ": states";
    for (const int st : r.states) {
        std::cout << " " << st;
    }
    std::cout << ", grabbed " << r.grabbed << " at " << r.first_grab_s << " s, inverted " << r.inverted << ", peak "
              << r.peak_n << " N, max step " << r.max_step << " m, deaths " << r.deaths_start << "->" << r.deaths_end
              << ", grabs " << r.m.grabs << " misses " << r.m.grab_misses << " carries " << r.m.carries << " dangles "
              << r.m.dangles << " releases " << r.m.releases << " spares " << r.m.spares << " torn " << r.m.grips_torn
              << " drops " << r.m.carry_drops << " brace losses " << r.m.brace_losses << ", aim " << r.m.aim_ledge
              << " (" << r.m.aim.x << "," << r.m.aim.y << "," << r.m.aim.z << "), player at " << r.p.player_position.x
              << "," << r.p.player_position.y << "," << r.p.player_position.z << " upright " << r.p.upright_count
              << " free " << r.p.player_free_rotation << " kicks " << r.p.thrash_kick_count << " state "
              << int(r.m.state) << "\n";
}
} // namespace

void run_matron_grab() {
    // ---- a still body: taken, carried, dangled, let go aimed; no kill on touch ----
    std::uint64_t hash_a = 0;
    for (int pass = 0; pass < 2; ++pass) {
        Simulation s(InitialSpawn::Deck154);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        const HoldRun r = hold_run(s, 60.0, [&](int, const scraperx::sim::MatronSnapshot &) {
            (void)s.set_move_input(0.0, 0.0);
        });
        report("matron grab still", r);
        require(r.grabbed && r.m.grabs >= 1, "matron grab: fully changed, her hand closes on a still player's ankle");
        require(r.deaths_held == 0, "matron grab: her hand on them is no death");
        require(r.states.count(kCarry) && r.states.count(kDangle), "matron grab: carried to the rail and dangled");
        require(r.inverted, "matron grab: held by the ankle, the body hangs head down");
        require(r.peak_n < Matron::kGripMaxForceN && r.m.grips_torn == 0,
                "matron grab: a still body's load stays under her grip's limit; it is not torn");
        require(r.max_step < 0.25, "matron grab: no teleport -- the body's every step while held is a real move");
        require(r.ended && !r.p.player_free_rotation && r.p.upright_count >= 1,
                "matron grab: let go, the body rights itself and the DOFs lock again");
        require(r.m.releases + r.m.spares >= 1, "matron grab: she lets go or spares");
        if (r.m.releases >= 1 && r.m.aim_ledge) {
            require(r.deaths_end == r.deaths_start, "matron grab: let go over her chosen ledge, the fall is survivable");
        }
        const std::uint64_t h = static_cast<std::uint64_t>(r.p.player_position.x * 1000.0) * 31U +
                                static_cast<std::uint64_t>(r.p.player_position.z * -1000.0) * 7U + r.m.grabs;
        if (pass == 0) {
            hash_a = h;
        } else {
            require(h == hash_a, "matron grab: the same hold twice, the same outcome (deterministic)");
        }
        std::cout << "PASS scraperx_sim matron grab still: peak_n=" << r.peak_n << " max_step=" << r.max_step
                  << " releases=" << r.m.releases << " spares=" << r.m.spares << "\n";
    }
    // ---- thrashing: the kicks strain her grip till it tears ----
    {
        Simulation s(InitialSpawn::Deck154);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 6.0);
        const HoldRun r = hold_run(s, 60.0, [&](const int i, const scraperx::sim::MatronSnapshot &m) {
            if (m.grip_on) {
                (void)s.set_move_input((i / 11) % 2 == 0 ? 1.0 : -1.0, 0.0);
            } else {
                (void)s.set_move_input(0.0, 0.0);
            }
        });
        report("matron grab thrash", r);
        require(r.grabbed && r.m.grips_torn >= 1, "matron grab: a thrashing body tears out of her grip");
        require(r.p.grip_tear_count >= 1 && r.deaths_held == 0, "matron grab: torn free, not killed in her hand");
        std::cout << "PASS scraperx_sim matron grab thrash: torn_s=" << r.torn_s << " kicks=" << r.p.thrash_kick_count
                  << "\n";
    }
    // ---- spared: set back on the deck, and she walks off ----
    {
        Simulation s(InitialSpawn::Deck154);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(1, 3.0);
        const HoldRun r = hold_run(s, 60.0, [&](int, const scraperx::sim::MatronSnapshot &) {
            (void)s.set_move_input(0.0, 0.0);
        });
        report("matron grab spare", r);
        require(r.ended && r.m.spares >= 1 && r.deaths_end == r.deaths_start, "matron grab: spared, set down alive");
        require(r.p.player_position.y > 153.5 && r.p.player_position.y < 156.5, "matron grab: spared onto her deck");
        const auto grabs = r.m.grabs;
        (void)s.advance_frame(10.0);
        std::cout << "  after spare: grabs " << s.matron_state().grabs << " state " << int(s.matron_state().state) << "\n";
        require(s.matron_state().grabs == grabs && s.matron_state().state != kHunting,
                "matron grab: spared, she walks off and leaves them be a while");
        std::cout << "PASS scraperx_sim matron grab spare: player_y=" << r.p.player_position.y << "\n";
    }
}
