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

void run_matron_arm() {
    // ---- ArmPlay: her other hand on a wrist, the pull, the tear; the arm hers,
    //      shown, thrown; the player one-armed till a death ----
    {
        Simulation s(InitialSpawn::Deck154);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        s.debug_matron_armplay(1);
        double peak_wrist = 0.0;
        double strain_at_tear = 0.0;
        double last_strain = 0.0;
        std::set<int> beats;
        bool thrown_seen = false;
        const HoldRun r = hold_run(s, 70.0, [&](int, const scraperx::sim::MatronSnapshot &m) {
            (void)s.set_move_input(0.0, 0.0);
            const auto p = s.snapshot();
            peak_wrist = std::max(peak_wrist, p.wrist_load_n);
            if (p.arm_tear_count == 0) {
                last_strain = p.wrist_strain_ns;
            } else if (strain_at_tear == 0.0) {
                strain_at_tear = last_strain;
            }
            if (m.state == static_cast<int>(Matron::State::ArmPlay)) {
                beats.insert(m.armplay_beat);
            }
            thrown_seen = thrown_seen || p.arm_throw_count > 0;
            if (std::getenv("ARM_DEBUG") != nullptr && m.state >= 14) {
                std::cout << "    st=" << int(m.state) << " beat=" << int(m.armplay_beat) << " tears=" << m.arm_tears
                          << " hold_s=" << m.hold_s << " wrist=" << p.wrist_load_n << " strain=" << p.wrist_strain_ns
                          << " grip=" << p.held_by_matron << "\n";
            }
        });
        report("matron arm", r);
        const auto p = s.snapshot();
        std::cout << "  matron arm: tears " << p.arm_tear_count << " side " << p.last_severed_side << " lost L/R "
                  << p.left_arm_lost << "/" << p.right_arm_lost << " peak wrist " << peak_wrist << " N, strain before tear "
                  << strain_at_tear << " N*s, throws " << p.arm_throw_count << ", arm at " << p.severed_arm_position.x
                  << "," << p.severed_arm_position.y << "," << p.severed_arm_position.z << " beats " << beats.size()
                  << "\n";
        require(r.m.armplays >= 1 && p.arm_tear_count == 1 && (p.left_arm_lost != p.right_arm_lost),
                "matron arm: her pull tears one arm from the shoulder");
        require(peak_wrist < 6000.0, "matron arm: the load on the wrist is finite");
        require(beats.size() >= 4 && thrown_seen && p.arm_throw_count == 1, "matron arm: shown, swung and thrown");
        require(p.severed_arm_present, "matron arm: the arm is a body in the world");
        require(r.deaths_held == 0 && r.deaths_end == r.deaths_start && p.one_arm,
                "matron arm: one-armed and alive; no health, a body");
        {
            std::uint64_t d0 = s.snapshot().death_count;
            int st0 = -1;
            for (int i = 0; i < 95 * 90; ++i) {
                tick(s);
                const auto q = s.snapshot();
                const int st = s.matron_state().state;
                if (std::getenv("ARM_DEBUG") != nullptr && (st != st0 || q.death_count != d0)) {
                    std::cout << "    idle t=" << i / 90.0 << " st " << st << " deaths " << q.death_count << " impact "
                              << q.last_impact_speed_mps << " tears " << q.arm_tear_count << " p " << q.player_position.x
                              << "," << q.player_position.y << "," << q.player_position.z << "\n";
                }
                if (std::getenv("ARM_DEBUG") != nullptr && i > int(25.0 * 90) && i < int(31.7 * 90) && i % 18 == 0) {
                    std::cout << "      t=" << i / 90.0 << " p " << q.player_position.x << "," << q.player_position.y << ","
                              << q.player_position.z << " free " << q.player_free_rotation << " held " << q.held_by_matron
                              << " gnd " << q.player_grounded << " torn " << s.matron_state().grips_torn << " drops "
                              << s.matron_state().carry_drops << " brace " << s.matron_state().brace_losses << " her "
                              << s.matron_state().position.x << "," << s.matron_state().position.z << "\n";
                }
                st0 = st;
                d0 = q.death_count;
            }
        }
        std::cout << "  matron arm: after 95 s present " << s.snapshot().severed_arm_present << " despawns "
                  << s.snapshot().severed_arm_despawn_count << " L/R " << s.snapshot().left_arm_lost
                  << s.snapshot().right_arm_lost << " deaths " << s.snapshot().death_count << " state "
                  << int(s.matron_state().state) << "\n";
        require(!s.snapshot().severed_arm_present && s.snapshot().severed_arm_despawn_count == 1 &&
                    (s.snapshot().left_arm_lost || s.snapshot().right_arm_lost),
                "matron arm: the thrown arm goes in time; the loss stays");
        const auto deaths = s.snapshot().death_count;
        s.debug_launch_player(scraperx::sim::Vector3{0.0, 154.5, -150.0}, scraperx::sim::Vector3{0.0, -5.0, 0.0});
        for (int i = 0; i < 40 * 90 && s.snapshot().death_count == deaths; ++i) {
            tick(s);
        }
        require(s.snapshot().death_count > deaths && !s.snapshot().left_arm_lost && !s.snapshot().right_arm_lost,
                "matron arm: a death, and the body is whole again");
        std::cout << "PASS scraperx_sim matron arm: strain_ns=" << strain_at_tear << " peak_wrist_n=" << peak_wrist
                  << "\n";
    }
    // ---- the second arm: she has won; the run ends and starts again whole ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_lose_arm(0);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        s.debug_matron_armplay(1);
        bool claimed = false;
        std::uint64_t ended = 0;
        for (int i = 0; i < 90 * 90 && ended == 0; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            claimed = claimed || s.snapshot().claimed_by_matron;
            if (std::getenv("ARM_DEBUG") != nullptr && s.matron_state().state == 15 && i % 10 == 0) {
                const auto q = s.snapshot();
                std::cout << "    c beat=" << int(s.matron_state().armplay_beat) << " hold_s=" << s.matron_state().hold_s
                          << " wrist=" << q.wrist_load_n << " strain=" << q.wrist_strain_ns << " h2=" << s.matron_state().hand2.x << "," << s.matron_state().hand2.y
                          << "\n";
            }
            ended = s.snapshot().runs_ended_by_matron;
        }
        const auto p = s.snapshot();
        std::cout << "  matron claimed: claimed " << claimed << " ended " << ended << " arms L/R " << p.left_arm_lost
                  << "/" << p.right_arm_lost << " matron state " << int(s.matron_state().state) << " grabs "
                  << s.matron_state().grabs << " armplays " << s.matron_state().armplays << " tears " << p.arm_tear_count
                  << " one_arm " << p.one_arm << " deaths " << p.death_count << "\n";
        require(claimed && ended == 1, "matron claimed: both arms gone, she has won and the run ends");
        require(!p.left_arm_lost && !p.right_arm_lost && !p.held_by_matron && s.matron_state().state == 0,
                "matron claimed: the run starts again, the body whole, her in her locker");
        std::cout << "PASS scraperx_sim matron claimed\n";
    }
    // ---- two grabs, two tears, the ending: nothing faked between them ----
    {
        Simulation s(InitialSpawn::Deck154);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(1, 3.0);   // the first: set back on her deck
        s.debug_matron_armplay(1);
        std::uint64_t most_tears = 0;
        bool claimed = false;
        std::uint64_t ended = 0;
        int second_meet = 0;
        std::uint64_t deaths_seen = 0;
        for (int i = 0; i < 400 * 90 && ended == 0; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            const auto q = s.snapshot();
            if (q.arm_tear_count != most_tears && std::getenv("ARM_DEBUG") != nullptr) {
                std::cout << "    tear " << q.arm_tear_count << " at " << i / 90.0 << " side " << q.last_severed_side
                          << " L/R " << q.left_arm_lost << q.right_arm_lost << " st " << int(s.matron_state().state) << "\n";
            }
            if (q.death_count != deaths_seen && std::getenv("ARM_DEBUG") != nullptr) {
                std::cout << "    death at " << i / 90.0 << " impact " << q.last_impact_speed_mps << " st "
                          << int(s.matron_state().state) << " second " << second_meet << "\n";
            }
            if (std::getenv("ARM_DEBUG") != nullptr && s.matron_state().state == 15 && i % 30 == 0) {
                std::cout << "    t beat=" << int(s.matron_state().armplay_beat) << " hold_s=" << s.matron_state().hold_s
                          << " wrist=" << q.wrist_load_n << " strain=" << q.wrist_strain_ns << " grip=" << q.grip_load_n << "\n";
            }
            deaths_seen = std::max<std::uint64_t>(deaths_seen, q.death_count);
            most_tears = std::max<std::uint64_t>(most_tears, q.arm_tear_count);
            claimed = claimed || q.claimed_by_matron;
            ended = q.runs_ended_by_matron;
            if (second_meet == 0 && q.arm_tear_count == 1 && !q.held_by_matron &&
                s.matron_state().state < static_cast<int>(Matron::State::Grab) && q.player_grounded) {
                second_meet = 1;
                require(q.one_arm && q.death_count == 0, "matron two tears: one-armed, alive, on her deck");
                // Back on her deck, one-armed; stand there. She comes back for
                // it on her own.
                (void)s.set_move_input(0.0, 0.0);
                s.debug_set_matron_stage(2);
                s.debug_matron_hold(0, 3.0);
                s.debug_matron_armplay(1);
            }
            if (second_meet == 1 && s.matron_state().state == static_cast<int>(Matron::State::Grab)) {
                second_meet = 2;   // her hand on the one-armed body
            }
        }
        std::cout << "  matron two tears: tears " << most_tears << " second meet " << second_meet << " claimed "
                  << claimed << " ended " << ended << " state " << int(s.matron_state().state) << " grabs "
                  << s.matron_state().grabs << " armplays "
                  << s.matron_state().armplays << " player " << s.snapshot().player_position.x << ","
                  << s.snapshot().player_position.y << "," << s.snapshot().player_position.z << " her " << s.matron_state().position.x << "," << s.matron_state().position.y << "," << s.matron_state().position.z << " held "
                  << s.snapshot().held_by_matron << " deaths " << s.snapshot().death_count << "\n";
        require(deaths_seen == 0, "matron two tears: no death on the way");
        require(second_meet == 2 && most_tears == 2 && claimed && ended == 1,
                "matron two tears: two grabs, two arms, and the run is hers");
        const auto p = s.snapshot();
        require(!p.left_arm_lost && !p.right_arm_lost && p.death_count == 0,
                "matron two tears: the new run is whole");
        std::cout << "PASS scraperx_sim matron two tears\n";
    }
}
