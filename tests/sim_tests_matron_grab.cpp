// Her hold (sim/matron_hold.cpp, sim/physics_world_grab.cpp): at every stage,
// her hand on a player is no death. She takes an ankle, carries them upside
// down to the shaft's rail, holds them out over the drop, tears an arm off,
// and sets them back on the deck, never letting go over open air. Her grip
// is finite: a still body hangs; a thrashing one tears free (once she has its
// wrist, leaving the arm in her other hand). Nothing teleports. Caught, a
// blow that is not his own frees nobody; his own, before the rip, does. Her
// second catch is the fatality (run_matron_fatality). These meet her fully
// changed, as written; her stage changes none of it.

#include "sim_tests.hpp"

#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <set>
#include <string>

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

// Her arm's length, every tick her hands are out (the owner, 2026-10-29: her
// arm was drawn 12 feet long): each hand no further from its shoulder than
// her arm (law::kArmLengthM), and the ankle in her fingers where her hand is
// (a real grip, not one held at a distance).
struct ArmReach {
    double max_m = 0.0;     // shoulder to hand (or to the held ankle), the most
    double max_grip_m = 0.0;   // her hand to the ankle it holds, the most
    int ticks = 0;
    int held_ticks = 0;
    int state_at_max = -1;
};
ArmReach g_reach;

void track_arm_reach(const Simulation &s) {
    const auto p = s.snapshot();
    const auto dist = [](const scraperx::sim::Vector3 &a, const scraperx::sim::Vector3 &b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    };
    const auto note = [&](const double d) {
        if (d > g_reach.max_m) {
            g_reach.max_m = d;
            g_reach.state_at_max = int(s.matron_state().state);
        }
    };
    if (p.matron_hand_present) {
        ++g_reach.ticks;
        note(dist(p.matron_hand, p.matron_shoulder));
        if (p.held_by_matron) {
            ++g_reach.held_ticks;
            note(dist(p.held_ankle, p.matron_shoulder));
            g_reach.max_grip_m = std::max(g_reach.max_grip_m, dist(p.held_ankle, p.matron_hand));
        }
    }
    if (p.matron_hand2_present) {
        note(dist(p.matron_hand2, p.matron_shoulder2));
    }
}

// The group's verdict on her arm: within its length every tick, the grip on him real.
void require_arm_reach(const char *group) {
    std::cout << "  " << group << " arm reach: max shoulder to hand " << g_reach.max_m << " m (state "
              << g_reach.state_at_max << "), hand to held ankle " << g_reach.max_grip_m << " m, over "
              << g_reach.ticks << " ticks, " << g_reach.held_ticks << " held\n";
    require(g_reach.held_ticks > 0, "matron arm reach: she held him");
    require(g_reach.max_m <= law::kArmLengthM,
            "matron arm reach: no hand of hers further from her shoulder than her arm, any tick");
    require(g_reach.max_grip_m <= 0.10, "matron arm reach: the ankle in her fingers is where her hand is");
    std::cout << "PASS scraperx_sim matron arm reach " << group << ": max_m=" << g_reach.max_m
              << " grip_m=" << g_reach.max_grip_m << "\n";
    g_reach = ArmReach{};
}

void tick(Simulation &s) {
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    track_arm_reach(s);
}

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
// waits there still for her hand. She cannot know where a quiet walk went while
// she was sealed: once she is out, one real noise from the spot calls her.
void meet_her(Simulation &s, const double x, const double z) {
    s.debug_fix_matron_pace(3);
    require(s.advance_frame(1.0).accepted, "matron grab: settle");
    require(go(s, 10.0, -128.5, true, 4.0), "matron grab: a sprint to wake her");
    require(go(s, x, z, false, 30.0), "matron grab: to the spot");
    (void)s.set_move_input(0.0, 0.0);
    constexpr int kStraining = static_cast<int>(scraperx::sim::Matron::State::Straining);
    constexpr int kEmerging = static_cast<int>(scraperx::sim::Matron::State::Emerging);
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) &&
                    (s.matron_state().state == kStraining || s.matron_state().state == kEmerging);
         ++i) {
        tick(s);
    }
    require(s.set_room_loudness(1.0), "matron grab: a noise from the spot");
    tick(s);
    require(s.matron_state().noise_heard > 0.0, "matron grab: she hears it");
    require(s.set_room_loudness(0.0), "matron grab: quiet again");
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

// A still player on her deck at (x, z), her out at (her_x, her_z) beside
// them, her own scan made as in play.
void caught_at(Simulation &s, const double x, const double z, const double her_x, const double her_z) {
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron caught: settle");
    (void)s.matron_affordances();
    s.debug_launch_player(scraperx::sim::Vector3{x, 154.95, z}, scraperx::sim::Vector3{0.0, 0.0, 0.0});
    for (int i = 0; i < 90; ++i) {
        tick(s);
    }
    s.debug_place_matron(scraperx::sim::Vector3{her_x, 154.0, her_z});
    // Facing him: her sight is her eyes (matron.hpp kSightHalfFovRad), and
    // these catches start with him in it, as her old all-round sight had.
    s.debug_face_matron(std::atan2(x - her_x, z - her_z));
}

// By the east band's rail at `z` (x 18), her 1.6 m along the band.
void at_the_rail(Simulation &s, const double z) { caught_at(s, 18.0, z, 18.3, z + 1.6); }
} // namespace

void run_matron_grab() {
    if (const char *e = std::getenv("SNAG_EXP")) {
        const int mode = std::atoi(e);
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        s.debug_matron_hold(0, 3.0);
        constexpr int kArmPlayE = static_cast<int>(Matron::State::ArmPlay);
        double snagged_at = -1.0, torn_at = -1.0, armplay_at = -1.0;
        bool held_past_reach = false;
        double best_far = 0.0;
        const HoldRun r = hold_run(s, 60.0, [&](const int i, const scraperx::sim::MatronSnapshot &m) {
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
            const auto q = s.snapshot();
            const double x = q.player_rot_x, y = q.player_rot_y, z = q.player_rot_z, w = q.player_rot_w;
            const double ux = 2 * (x * y - w * z), uy = 1 - 2 * (x * x + z * z), uz = 2 * (y * z + w * x);
            const double rx = 1 - 2 * (y * y + z * z), ry = 2 * (x * y + w * z), rz = 2 * (x * z - w * y);
            double nearest = 1e9, ny = 0, nax = 0, nay = 0, naz = 0;
            for (int k = -1; k <= 1; k += 2) {
                const double ax = q.player_position.x + 0.45 * ux + k * 0.25 * rx;
                const double ay = q.player_position.y + 0.45 * uy + k * 0.25 * ry;
                const double az = q.player_position.z + 0.45 * uz + k * 0.25 * rz;
                const double d = std::hypot(ax - m.position.x, az - m.position.z);
                if (d < nearest) { nearest = d; ny = ay - (m.position.y - 1.05); nax = ax; nay = ay; naz = az; }
            }
            if (armplay_at < 0.0 && m.state == kArmPlayE) armplay_at = i / 90.0;
            bool snag_now = false;
            if (snagged_at < 0.0) {
                if (mode == 1 && m.state == kArmPlayE) snag_now = true;
                if (mode == 2 && m.state == kDangle && nearest > best_far) { best_far = nearest; }
                static bool was_up = false;
                if (mode == 3 && m.state == kDangle && uy >= 0.0) was_up = true;
                if ((mode >= 3) && m.state == kDangle && uy >= 0.0) was_up = true;
                if ((mode >= 3) && m.state == kDangle && was_up && uy < 0.0) snag_now = true;
            }
            if (snag_now) { s.debug_snag_player(0.5 * (Matron::kHandPullN + Matron::kGripRipFloorN), mode >= 4 ? std::atof(std::getenv("GIVE") ? std::getenv("GIVE") : "5") : 0.0, std::getenv("UPM") ? std::atof(std::getenv("UPM")) : 0.0); snagged_at = i / 90.0; }
            if (snagged_at >= 0.0 && m.arm_reposes == 0 && s.debug_player_snagged())
                held_past_reach = i / 90.0 - snagged_at > Matron::kArmReachSeconds - 0.1;
            if (torn_at < 0.0 && q.arm_tear_count > 0) torn_at = i / 90.0;
            static double px = 0, py = 0, pz = 0;
            const double sp = std::sqrt((q.player_position.x - px) * (q.player_position.x - px) + (q.player_position.y - py) * (q.player_position.y - py) + (q.player_position.z - pz) * (q.player_position.z - pz)) * 90.0;
            px = q.player_position.x; py = q.player_position.y; pz = q.player_position.z;
            if (std::getenv("EVERY") ? (i > std::atoi(std::getenv("EVERY")) && i < std::atoi(std::getenv("EVERY")) + 120) : i % 9 == 0)
                std::cout << "E sp " << sp << " t " << i / 90.0 << " st " << int(m.state) << " arm_out " << nearest << " arm_up " << ny << " c_out " << std::hypot(q.player_position.x - m.position.x, q.player_position.z - m.position.z) << " uy " << uy << " lean " << m.hold_lean << " crouch " << m.hold_crouch << " snag " << s.debug_player_snagged() << " rep " << m.arm_reposes << " h2 " << m.hand2.x << "," << m.hand2.y << "," << m.hand2.z << " nearpt " << nax << "," << nay << "," << naz << " load " << q.grip_load_n << " hand " << m.hand.x << "," << m.hand.y << " her " << m.position.x << "," << m.position.z << "\n";
        });
        std::cout << "EXP snagged " << snagged_at << " armplay " << armplay_at << " held_past " << held_past_reach << " reposes " << r.m.arm_reposes << " snag_left " << s.debug_player_snagged() << " torn " << torn_at << " tears " << r.p.arm_tear_count << " claims " << r.m.claims << " ended " << r.ended << " best_far " << best_far << "\n";
        return;
    }
    // ---- a still body: taken, carried, dangled, let go aimed; no kill on touch ----
    std::uint64_t hash_a = 0;
    for (int pass = 0; pass < 2; ++pass) {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
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
        s.place_matron_home(154.0F);
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
        s.place_matron_home(154.0F);
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
    // ---- a still body caught by the east band's rail, at nine spots 0.5 m
    //      apart (the act's rail): her heave over it tears her grip at some
    //      (the body hooked on the rail); there she takes the body again where
    //      it lands and the arm comes off there. Every catch, one arm for sure
    //      (Cory, 2026-10-27); no claim, no death ----
    {
        std::uint32_t retaken = 0;
        for (int k = 0; k < 9; ++k) {
            const double z = -160.0 + 0.5 * k;
            Simulation s(InitialSpawn::Deck154);
            s.place_matron_home(154.0F);
            require(s.advance_frame(1.0).accepted, "matron rail: settle");
            (void)s.matron_affordances();   // her own scan, as in play
            s.debug_launch_player(scraperx::sim::Vector3{18.0, 154.95, z}, scraperx::sim::Vector3{0.0, 0.0, 0.0});
            for (int i = 0; i < 90; ++i) {
                tick(s);
            }
            s.debug_place_matron(scraperx::sim::Vector3{18.3, 154.0, z + 1.6});
            // Where the body is as her fingers open (a let-go, not a tear).
            bool was_held = false;
            std::uint64_t lets_go = s.snapshot().grip_release_count;
            double let_go_x = 1.0e9;
            const HoldRun r = hold_run(s, 60.0, [&](int, const scraperx::sim::MatronSnapshot &) {
                s.debug_set_matron_stage(1);
                (void)s.set_move_input(0.0, 0.0);
                const auto q = s.snapshot();
                if (was_held && !q.held_by_matron && q.grip_release_count > lets_go) {
                    let_go_x = std::min(let_go_x, q.player_position.x);
                }
                lets_go = q.grip_release_count;
                was_held = q.held_by_matron;
            });
            std::cout << "  matron rail: z " << z << ", grabs " << r.m.grabs << " torn " << r.m.grips_torn
                      << " retakes " << r.m.retakes << " armplays " << r.m.armplays << " tears "
                      << r.p.arm_tear_count << " claims " << r.m.claims << " deaths " << r.deaths_start << "->"
                      << r.deaths_end << " (held " << r.deaths_held << "), ended " << r.ended << ", let go at x "
                      << let_go_x << " (releases " << r.m.releases << " spares " << r.m.spares << ")\n";
            // The east band's side over the shaft (its rail) is at x 17: her deck is past it, east.
            require(r.m.releases == 0 && let_go_x > 17.0 && let_go_x < 1.0e8,
                    "matron rail: an ordinary hold sets him down over her deck, never lets go over open air");
            require(r.grabbed && r.ended && r.p.arm_tear_count == 1 && r.p.one_arm,
                    "matron rail: a still body caught by the rail loses one arm, wherever her heave goes");
            require(r.m.claims == 0 && r.deaths_held == 0 && r.deaths_end == r.deaths_start,
                    "matron rail: one arm gone, no claim and no death");
            require(r.m.retakes <= static_cast<std::uint32_t>(Matron::kMaxRetakes),
                    "matron rail: at most two re-takes in a hold, no loop on the same steel");
            retaken += r.m.retakes > 0 ? 1U : 0U;
        }
        std::cout << "PASS scraperx_sim matron rail: 9 spots, one arm each, retaken at " << retaken << "\n";
    }
    // ---- the wrist out of her reach: as he goes over the east band's rail
    //      head first (his body level across the drop, his arms out past
    //      it), his body is caught on the steel out there by a snag that
    //      holds more than her arm's pull (kHandPullN) and less than her hard
    //      grip in arm play (kGripRipFloorN), with a cloth's give (a body
    //      still moving is caught, not torn off it by one tick's jolt), a
    //      little toward his shoulders: hung there from her fist and the
    //      snag, his arms are out past her real reach. She does not give it
    //      up: kArmReachSeconds of her arm play without the wrist, she
    //      re-poses, a step back with her weight in it, the snag gives, and
    //      the arm comes off within 10 s of her arm play's start; no claim,
    //      no death. (With her old 1.2 m arm the snag was made as her arm
    //      play began, the body then hung past her reach; at her real arm's
    //      length it hangs within it there, so it is caught as it goes over.)
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        s.debug_matron_hold(0, 3.0);
        constexpr int kArmPlay = static_cast<int>(Matron::State::ArmPlay);
        constexpr double kSnagGiveHz = 3.0;   // cloth on a hook: a few cm under her pull
        constexpr double kSnagUpM = 0.3;      // from his middle toward his shoulders
        double snagged_at = -1.0;
        double armplay_at = -1.0;
        double torn_at = -1.0;
        bool held_past_reach = false;
        bool tipped_up = false;
        const HoldRun r = hold_run(s, 60.0, [&](const int i, const scraperx::sim::MatronSnapshot &m) {
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
            const auto q = s.snapshot();
            // Over the rail he turns head up across it, then over head first:
            // the snag as his body passes level.
            const double his_up_y = up_y(q);
            tipped_up = tipped_up || (m.state == kDangle && his_up_y >= 0.0);
            if (snagged_at < 0.0 && m.state == kDangle && tipped_up && his_up_y < 0.0) {
                s.debug_snag_player(0.5 * (Matron::kHandPullN + Matron::kGripRipFloorN), kSnagGiveHz, kSnagUpM);
                snagged_at = i / 90.0;
            }
            if (armplay_at < 0.0 && m.state == kArmPlay) {
                armplay_at = i / 90.0;
            }
            if (armplay_at >= 0.0 && m.arm_reposes == 0 && s.debug_player_snagged()) {
                // The snag holds while she reaches from where she stands.
                held_past_reach = i / 90.0 - armplay_at > Matron::kArmReachSeconds - 0.1;
            }
            if (torn_at < 0.0 && q.arm_tear_count > 0) {
                torn_at = i / 90.0;
            }
        });
        std::cout << "  matron arm snagged: snagged at " << snagged_at << " s, arm play at " << armplay_at
                  << " s, held past her reach " << held_past_reach << ", re-poses " << r.m.arm_reposes
                  << ", snag left " << s.debug_player_snagged() << ", torn at " << torn_at << " s, tears "
                  << r.p.arm_tear_count << " claims " << r.m.claims << " deaths " << r.deaths_start << "->"
                  << r.deaths_end << ", ended " << r.ended << "\n";
        require(snagged_at >= 0.0 && held_past_reach && r.m.arm_reposes >= 1 && !s.debug_player_snagged(),
                "matron arm snagged: the wrist out of her reach, she re-poses and her weight pulls the body off the snag");
        require(r.grabbed && r.ended && r.p.arm_tear_count == 1 && r.p.one_arm && torn_at - armplay_at < 10.0,
                "matron arm snagged: the arm comes off within 10 s of her arm play's start");
        require(r.m.claims == 0 && r.deaths_held == 0 && r.deaths_end == r.deaths_start,
                "matron arm snagged: one arm gone, no claim and no death");
        std::cout << "PASS scraperx_sim matron arm snagged: torn " << torn_at - armplay_at << " s into her arm play\n";
    }
    // ---- the arm never hers: her pull kept under the shoulder's hold (a
    //      test's arm that will not come). She re-poses at most
    //      Matron::kMaxArmReposes times, then (Cory, 2026-10-28) keeps him
    //      and sets him down on her deck (Spare): never let go over open
    //      air or the shaft, the arm on, no loop, no tear, no claim, no death ----
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        s.debug_matron_hold(0, 3.0);
        s.debug_matron_armplay(2);
        constexpr int kArmPlay = static_cast<int>(Matron::State::ArmPlay);
        bool in_armplay = false;
        bool capped = false;
        bool released = false;
        bool spared = false;
        bool dropped_in_release = false;
        std::uint32_t reposes_at_cap = 0;
        double low_after_cap = 1.0e9;
        double y_at_let_go = 0.0;
        int after = 0;
        for (int i = 0; i < 90 * 120 && after < 3 * 90; ++i) {
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            const auto m = s.matron_state();
            const auto snap = s.snapshot();
            in_armplay = in_armplay || m.state == kArmPlay;
            if (in_armplay && !capped && m.state != kArmPlay) {
                capped = true;
                reposes_at_cap = m.arm_reposes;
                spared = m.state == kSpare;
                dropped_in_release = m.state == kRelease;
            }
            if (capped) {
                low_after_cap = std::min(low_after_cap, snap.player_position.y);
                if (!released && !snap.held_by_matron) {
                    released = true;
                    y_at_let_go = snap.player_position.y;
                }
            }
            if (released) {
                ++after;
            }
        }
        const auto end = s.matron_state();
        std::cout << "  matron arm never hers: arm play " << in_armplay << ", capped " << capped << " (spare "
                  << spared << "), re-poses " << reposes_at_cap << " (then " << end.arm_reposes << "), let go "
                  << released << " at y " << y_at_let_go << ", lowest after " << low_after_cap << ", tears "
                  << s.snapshot().arm_tear_count << ", claims " << end.claims << "\n";
        require(in_armplay && capped && spared && !dropped_in_release,
                "matron arm never hers: past her re-poses she keeps him to set him down (Spare), no drop");
        require(reposes_at_cap == static_cast<std::uint32_t>(Matron::kMaxArmReposes) &&
                    end.arm_reposes == reposes_at_cap,
                "matron arm never hers: exactly kMaxArmReposes re-poses, then no more");
        require(released && low_after_cap > 153.5 && s.snapshot().player_position.y > 153.5 &&
                    s.snapshot().player_position.y < 156.5,
                "matron arm never hers: the cap release never leaves him over the shaft (set down on her deck)");
        require(s.snapshot().arm_tear_count == 0 && !s.snapshot().held_by_matron && end.claims == 0,
                "matron arm never hers: let go with the arm on, out of her hand, no claim");
        std::cout << "PASS scraperx_sim matron arm never hers: set down on her deck after " << reposes_at_cap
                  << " re-poses, player_y=" << s.snapshot().player_position.y << "\n";
    }
    // ---- the arm never hers, his body snagged past the rail: as her arm
    //      play begins the body, held out over the shaft, is caught on the
    //      steel there (more than her arm's pull), and her re-pose cap comes
    //      at once (a test's arm play): her deck-only set-down begins with
    //      her hand over her deck and his body over the drop. She never lets
    //      go of him without her deck close under his body ----
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        s.debug_matron_hold(0, 3.0);
        s.debug_matron_armplay(3);
        constexpr int kArmPlay = static_cast<int>(Matron::State::ArmPlay);
        const std::uint64_t deaths = s.snapshot().death_count;
        bool snagged = false;
        bool spare_past_rail = false;
        int spare_ticks = 0;
        int open_air_lets_go = 0;
        std::uint64_t releases = s.snapshot().grip_release_count;
        for (int i = 0; i < 40 * 90; ++i) {
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
            if (!snagged && s.matron_state().state == kArmPlay) {
                s.debug_snag_player(0.5 * (Matron::kHandPullN + Matron::kGripRipFloorN));
                snagged = true;
            }
            tick(s);
            const auto m = s.matron_state();
            const auto p = s.snapshot();
            const double under = s.debug_support_y(p.player_position, 2.4);
            const bool deck_under = !std::isnan(under) && under > 153.4 && under < 154.15;
            if (m.state == kSpare && spare_ticks++ == 0) {
                spare_past_rail = s.debug_player_snagged() && p.player_position.x < Matron::kShaftHalfM && !deck_under;
            }
            if (p.grip_release_count != releases) {
                releases = p.grip_release_count;
                open_air_lets_go += deck_under ? 0 : 1;
            }
        }
        const auto p = s.snapshot();
        std::cout << "  matron arm snagged past the rail: spare past the rail " << spare_past_rail << ", her lets go "
                  << p.grip_release_count << " (over open air " << open_air_lets_go << "), torn " << p.grip_tear_count
                  << ", player at " << p.player_position.x << "," << p.player_position.y << ", tears "
                  << p.arm_tear_count << ", deaths " << deaths << "->" << p.death_count << "\n";
        require(snagged && spare_past_rail,
                "matron arm snagged past the rail: her deck-only set-down begins with his body over the drop (setup)");
        require(open_air_lets_go == 0,
                "matron arm snagged past the rail: she never lets go of him without her deck close under his body");
        require(p.arm_tear_count == 0 && s.matron_state().claims == 0 && p.death_count == deaths,
                "matron arm snagged past the rail: the arm on, no claim, no death");
        std::cout << "PASS scraperx_sim matron arm snagged past the rail: lets go over open air " << open_air_lets_go
                  << "\n";
    }
    // ---- kicked free once her hand has his wrist (Cory, through PE: once
    //      the rip starts nothing breaks it, his struggling neither): his
    //      kicks tear her ankle grip, the body pulls away and the arm stays
    //      in her other hand, torn as it goes; no re-take, no claim, no
    //      death, alive with one arm ----
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        s.debug_matron_hold(0, 3.0);
        constexpr int kArmPlay = static_cast<int>(Matron::State::ArmPlay);
        int kicks_from = -1;
        double torn_at = -1.0;
        double arm_at = -1.0;
        const HoldRun r = hold_run(s, 60.0, [&](const int i, const scraperx::sim::MatronSnapshot &m) {
            s.debug_set_matron_stage(1);
            if (kicks_from < 0 && m.state == kArmPlay && m.armplay_beat == 1) {
                kicks_from = i;   // her other hand has the wrist: he kicks
            }
            const bool kick = kicks_from >= 0 && m.grip_on;
            (void)s.set_move_input(kick ? (((i - kicks_from) / 11) % 2 == 0 ? 1.0 : -1.0) : 0.0, 0.0);
            const auto p = s.snapshot();
            if (torn_at < 0.0 && p.grip_tear_count > 0) {
                torn_at = i / 90.0;
            }
            if (arm_at < 0.0 && p.arm_tear_count > 0) {
                arm_at = i / 90.0;
            }
        });
        std::cout << "  matron arm kicked free: kicks from " << kicks_from / 90.0 << " s, " << r.p.thrash_kick_count
                  << " kicks, grip torn at " << torn_at << " s, arm at " << arm_at << " s, tears "
                  << r.p.arm_tear_count << " one_arm " << r.p.one_arm << " retakes " << r.m.retakes << " claims "
                  << r.m.claims << " deaths " << r.deaths_start << "->" << r.deaths_end << "\n";
        require(kicks_from >= 0 && r.m.grips_torn >= 1 && r.p.grip_tear_count >= 1,
                "matron arm kicked free: his kicks tear her ankle grip once she has his wrist");
        require(r.p.arm_tear_count == 1 && r.p.one_arm && arm_at == torn_at,
                "matron arm kicked free: the arm stays in her hand, torn as the body pulls away");
        require(r.m.retakes == 0 && r.m.claims == 0 && r.deaths_held == 0 && r.deaths_end == r.deaths_start,
                "matron arm kicked free: no re-take, no claim, no death: alive with one arm");
        std::cout << "PASS scraperx_sim matron arm kicked free: torn " << torn_at - kicks_from / 90.0
                  << " s into his kicking\n";
    }
    // ---- set down by the north band's rail: a still body caught there, her
    //      grip torn on the rail and taken again, the arm off where she
    //      stands; she sets it down on the first spot round her with her deck
    //      under the body's breadth (to her right the rail, to her left a
    //      post), and the hold is over ----
    {
        Simulation s(InitialSpawn::Deck154);
        caught_at(s, -12.0, -132.0, -10.4, -131.7);
        const HoldRun r = hold_run(s, 60.0, [&](int, const scraperx::sim::MatronSnapshot &) {
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
        });
        report("matron set down", r);
        require(r.grabbed && r.ended && r.m.spares >= 1 && r.p.arm_tear_count == 1 && r.p.one_arm,
                "matron set down: the arm off, set down on her deck beside her, the hold over");
        require(r.m.claims == 0 && r.deaths_held == 0 && r.deaths_end == r.deaths_start,
                "matron set down: no claim and no death");
        std::cout << "PASS scraperx_sim matron set down: spares=" << r.m.spares << " player_y=" << r.p.player_position.y
                  << "\n";
    }
    // ---- a blow mid-hold (Cory, 2026-10-27, 6:51 PM): a toolbox thrown at her
    //      back as she holds him out over the drop (one that staggers her at
    //      her post, run_matron_balance) is not his: she takes it on her feet,
    //      him in her hand, and the arm comes off. The same blow tagged as his
    //      own (his punch's tag) before the rip staggers her and frees him,
    //      whole; once the rip has begun, it frees nothing ----
    {
        constexpr int kStaggering = static_cast<int>(Matron::State::Staggering);
        constexpr int kArmPlay = static_cast<int>(Matron::State::ArmPlay);
        struct Blown {
            HoldRun r;
            bool thrown = false;
            bool staggered = false;
            bool freed_whole = false;
        };
        // `his`: the blow tagged as his; thrown at her once she is in `when`.
        const auto blow_mid_hold = [&](const bool his, const int when) {
            Blown b;
            Simulation s(InitialSpawn::Deck154);
            at_the_rail(s, -159.5);
            s.debug_matron_hold(0, 3.0);
            const std::uint32_t first =
                s.loose_stock_count() - static_cast<std::uint32_t>(scraperx::sim::LooseStock::kMatronThrowables);
            const std::uint32_t toolbox = s.kit_body_index(scraperx::sim::LooseStock::kFirstEntity + first);
            const auto staggers = s.matron_state().staggers;
            b.r = hold_run(s, 60.0, [&](int, const scraperx::sim::MatronSnapshot &m) {
                s.debug_set_matron_stage(1);
                (void)s.set_move_input(0.0, 0.0);
                if (!b.thrown && m.state == when && s.snapshot().held_by_matron) {
                    b.thrown = true;
                    if (his) {
                        s.debug_matron_player_blow();
                    }
                    const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
                    const double fx = std::sin(m.yaw);
                    const double fz = std::cos(m.yaw);
                    s.debug_launch_kit_body(toolbox, {m.position.x - 2.0 * fx, feet + 1.3, m.position.z - 2.0 * fz},
                                            {5.8 * fx, 0.0, 5.8 * fz});
                }
                const auto p = s.snapshot();
                b.staggered = b.staggered || m.state == kStaggering;
                b.freed_whole = b.freed_whole || (b.thrown && !p.held_by_matron && p.arm_tear_count == 0 &&
                                                  m.state == kStaggering);
            });
            b.staggered = b.staggered || b.r.m.staggers != staggers;
            std::cout << "  matron hold blow: " << (his ? "his" : "not his") << " in state " << when << ", thrown "
                      << b.thrown << ", blow " << b.r.m.blow_mps << " m/s, staggered " << b.staggered
                      << ", freed whole " << b.freed_whole << ", tears " << b.r.p.arm_tear_count << " claims "
                      << b.r.m.claims << " deaths " << b.r.deaths_start << "->" << b.r.deaths_end << "\n";
            return b;
        };
        const Blown world = blow_mid_hold(false, kDangle);
        require(world.thrown && world.r.m.blow_mps > 0.0 && !world.staggered && !world.freed_whole,
                "matron hold blow: a blow not his, mid-hold, takes neither her feet nor him");
        require(world.r.ended && world.r.p.arm_tear_count == 1 && world.r.p.one_arm && world.r.m.claims == 0 &&
                    world.r.deaths_end == world.r.deaths_start,
                "matron hold blow: him still in her hand, the arm comes off; no claim, no death");
        const Blown punch = blow_mid_hold(true, kDangle);
        require(punch.thrown && punch.staggered && punch.freed_whole && punch.r.p.arm_tear_count == 0 &&
                    punch.r.deaths_end == punch.r.deaths_start,
                "matron hold blow: his own blow before the rip staggers her and frees him, whole");
        const Blown late = blow_mid_hold(true, kArmPlay);
        require(late.thrown && late.r.m.blow_mps > 0.0 && !late.staggered && late.r.p.arm_tear_count == 1 &&
                    late.r.p.one_arm && late.r.deaths_end == late.r.deaths_start,
                "matron hold blow: once the rip has begun, his blow breaks nothing; the arm comes off");
        std::cout << "PASS scraperx_sim matron hold blow: not his, held; his before the rip, freed; after, held\n";
    }
    // ---- his punch's tag alone, a blow too small to shift her (the punch's
    //      50 N*s is 0.23 m/s on her 220 kg, under what staggers her): in an
    //      ordinary hold before the rip, her hand opens on it. Carried at her
    //      side over her deck, he drops onto it at once; out over the drop,
    //      she reels back and sets him down on her deck. Whole, alive, never
    //      let go over open air; her hands off him kRegrabAfterTearS ----
    {
        constexpr int kStaggering = static_cast<int>(Matron::State::Staggering);
        struct Tagged {
            bool tagged = false, freed = false, staggered = false;
            double tagged_s = -1.0, freed_s = -1.0, let_go_x = 1.0e9, end_s = 0.0;
            HoldRun r;
        };
        const auto tag_in = [&](Simulation &s, const int when, const bool mid_band) {
            Tagged t;
            if (mid_band) {
                caught_at(s, 21.5, -145.0, 21.8, -143.4);   // the middle of her east band, 4.5 m from either side
            } else {
                at_the_rail(s, -159.5);
            }
            bool was_held = false;
            int last_i = 0;
            t.r = hold_run(s, 60.0, [&](const int i, const scraperx::sim::MatronSnapshot &m) {
                last_i = i;
                // Dangerous in her hold; fully changed once he is free (no shove at the rim).
                s.debug_set_matron_stage(t.freed_s >= 0.0 ? 2 : 1);
                (void)s.set_move_input(0.0, 0.0);
                const auto q = s.snapshot();
                if (t.tagged && was_held && !q.held_by_matron && t.freed_s < 0.0) {
                    t.freed_s = i / 90.0;
                    t.let_go_x = q.player_position.x;
                }
                t.staggered = t.staggered || (t.tagged && m.state == kStaggering);
                was_held = q.held_by_matron;
                if (!t.tagged && m.state == when && q.held_by_matron) {
                    t.tagged = true;
                    t.tagged_s = i / 90.0;
                    s.debug_matron_player_blow();
                }
            });
            t.freed = t.freed_s >= 0.0 && t.r.p.arm_tear_count == 0;
            std::cout << "  matron blow tag: in state " << when << ", tagged at " << t.tagged_s << " s, freed at "
                      << t.freed_s << " s at x " << t.let_go_x << ", staggered " << t.staggered << ", tears "
                      << t.r.p.arm_tear_count << " deaths " << t.r.deaths_start << "->" << t.r.deaths_end
                      << ", player " << t.r.p.player_position.x << "," << t.r.p.player_position.y << ","
                      << t.r.p.player_position.z << ", reels " << t.r.m.reels << "\n";
            t.end_s = (last_i + 1) / 90.0 + 3.0;   // hold_run settles 3 s after its last tick
            return t;
        };
        Simulation carried(InitialSpawn::Deck154);
        const Tagged at_side = tag_in(carried, kCarry, true);
        require(at_side.tagged && at_side.freed && at_side.freed_s - at_side.tagged_s < 0.1 && at_side.staggered,
                "matron blow tag: carried over her deck, his punch's tag alone opens her hand at once, before the rip");
        Simulation dangled(InitialSpawn::Deck154);
        const Tagged out_over = tag_in(dangled, kDangle, false);
        require(out_over.tagged && out_over.freed && out_over.r.m.reels >= 1 && out_over.staggered,
                "matron blow tag: out over the drop, his punch's tag alone frees him, before the rip");
        for (const Tagged *t : {&at_side, &out_over}) {
            // The east band's side over the shaft (its rail) is at x 17: her deck is past it, east.
            require(t->let_go_x > 17.0 && t->r.ended && t->r.deaths_end == t->r.deaths_start &&
                        !t->r.p.left_arm_lost && !t->r.p.right_arm_lost && t->r.p.player_position.y > 153.5 &&
                        t->r.p.player_position.y < 156.5 && t->r.p.player_position.x > 17.0,
                    "matron blow tag: set down on her deck, whole and alive, never let go over open air");
        }
        // Her hands off him kRegrabAfterTearS after his blow freed him: put
        // on him, fully changed (no shove), they do not take him till then.
        Simulation &s = carried;
        double t = at_side.end_s - at_side.freed_s;   // since his blow freed him
        s.debug_launch_player(scraperx::sim::Vector3{21.5, 154.95, -150.0}, scraperx::sim::Vector3{0.0, 0.0, 0.0});
        for (int i = 0; i < 45; ++i, t += 1.0 / 90.0) {
            tick(s);
        }
        s.debug_place_matron(scraperx::sim::Vector3{21.5, 154.0, -149.3});
        const auto grabs = s.matron_state().grabs;
        bool blocked = true;
        double closest = 1.0e9;
        for (; t < static_cast<double>(Matron::kRegrabAfterTearS) - 1.0; t += 1.0 / 90.0) {
            s.debug_set_matron_stage(2);
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            const auto m = s.matron_state();
            const auto p = s.snapshot();
            closest = std::min(closest, std::hypot(m.position.x - p.player_position.x, m.position.z - p.player_position.z));
            blocked = blocked && m.grabs == grabs && !p.held_by_matron;
        }
        std::cout << "  matron blow tag: freed, then on him: closest " << closest << " m, blocked " << blocked
                  << " till " << t << " s after the free\n";
        require(closest < static_cast<double>(Matron::kCatchM) && blocked,
                "matron blow tag: freed by his blow, her hands do not take him again for kRegrabAfterTearS");
        std::cout << "PASS scraperx_sim matron blow tag: carried, freed at once; out over the drop, reeled in; both on "
                     "her deck\n";
    }
    // ---- an arm torn off and set down alive: her hands do not take him again
    //      for kRegrabAfterTearS (the owner, 2026-10-28), though she stands on
    //      him; then they do ----
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        // The hold, till she walks off from setting him down (the moment she
        // sets his hands-off time).
        bool walked_off = false;
        for (int i = 0; i < 60 * 90 && !walked_off; ++i) {
            const int was = s.matron_state().state;
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            walked_off = was == kSpare && s.matron_state().state != kSpare;
        }
        require(walked_off && s.snapshot().arm_tear_count == 1 && !s.snapshot().held_by_matron,
                "matron regrab: an arm off, set down alive, and she walks off");
        // Out of her calm (she walks off first), she is put on him, fully
        // changed (no shove), him in the middle of her east band.
        const double calm_out = static_cast<double>(Matron::kSpareCalmSeconds) + 1.0;
        double t = 0.0;   // since she walked off
        for (; t < calm_out; t += 1.0 / 90.0) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
        }
        s.debug_launch_player(scraperx::sim::Vector3{21.5, 154.95, -150.0}, scraperx::sim::Vector3{0.0, 0.0, 0.0});
        for (int i = 0; i < 45; ++i, t += 1.0 / 90.0) {
            tick(s);
        }
        s.debug_place_matron(scraperx::sim::Vector3{21.5, 154.0, -149.3});
        const auto grabs = s.matron_state().grabs;
        const auto catches = s.matron_state().catches;
        double closest = 1.0e9;
        bool blocked = true;
        for (; t < static_cast<double>(Matron::kRegrabAfterTearS) - 0.3; t += 1.0 / 90.0) {
            s.debug_set_matron_stage(2);
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            const auto m = s.matron_state();
            const auto p = s.snapshot();
            closest = std::min(closest, std::hypot(m.position.x - p.player_position.x, m.position.z - p.player_position.z));
            blocked = blocked && m.grabs == grabs && m.catches == catches && !p.held_by_matron;
        }
        double grabbed_at = -1.0;
        for (; t < static_cast<double>(Matron::kRegrabAfterTearS) + 8.0 && grabbed_at < 0.0; t += 1.0 / 90.0) {
            s.debug_set_matron_stage(2);
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            if (s.matron_state().grabs != grabs) {
                grabbed_at = t;
            }
        }
        std::cout << "  matron regrab: closest " << closest << " m while blocked " << blocked << ", grabbed again at "
                  << grabbed_at << " s after she walked off\n";
        require(closest < static_cast<double>(Matron::kCatchM) && blocked,
                "matron regrab: on him, her hands do not take him for kRegrabAfterTearS after the arm");
        require(grabbed_at >= static_cast<double>(Matron::kRegrabAfterTearS) - 0.3,
                "matron regrab: then they take him again");
        std::cout << "PASS scraperx_sim matron regrab: blocked " << Matron::kRegrabAfterTearS << " s, grabbed at "
                  << grabbed_at << " s\n";
    }
    require_arm_reach("matron-grab");
}

void run_matron_arm() {
    // ---- ArmPlay: her other hand on a wrist, the pull, the tear; the arm hers,
    //      shown, thrown; the player one-armed till a death ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
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
        // Set down on her deck now (an ordinary hold no longer lets go over
        // the ledge below), she would come for him again once her hands may,
        // and that catch is the fatality, whose death makes him whole: her
        // hands kept off him while the arm's own time runs.
        s.debug_hold_matron_grab(true);
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
    // ---- the second catch, his one arm already gone (the owner, 2026-10-28):
    //      the fatality, not a second arm; it ends in a death, and the run
    //      restarts from the last save with the body whole ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        s.debug_lose_arm(0);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        s.debug_matron_armplay(1);
        const auto deaths = s.snapshot().death_count;
        const auto releases = s.matron_state().fatal_release_beat;
        int path = 0;
        bool claimed = false;
        for (int i = 0; i < 90 * 90 && s.snapshot().death_count == deaths; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            path = std::max<int>(path, s.matron_state().fatal_path);
            claimed = claimed || s.snapshot().claimed_by_matron;
        }
        const auto p = s.snapshot();
        const auto m = s.matron_state();
        std::cout << "  matron second catch: path " << path << " claimed " << claimed << " releases " << releases
                  << "->" << m.fatal_release_beat << " deaths " << deaths << "->" << p.death_count << " ended "
                  << p.runs_ended_by_matron << " tears " << p.arm_tear_count << " arms L/R " << p.left_arm_lost << "/"
                  << p.right_arm_lost << " matron state " << int(m.state) << " grabs " << m.grabs << "\n";
        require(path != 0 && p.death_count == deaths + 1 && p.runs_ended_by_matron == 0,
                "matron second catch: one arm gone, her next catch is the fatality, and it ends in a death");
        require(p.arm_tear_count == 0 &&
                    (path == static_cast<int>(Matron::FatalPath::Floor) ? claimed : m.fatal_release_beat == releases + 1),
                "matron second catch: no second arm torn: let go over the drop, or her floor version");
        require(!p.left_arm_lost && !p.right_arm_lost && !p.held_by_matron &&
                    !Matron::holding_state(static_cast<Matron::State>(m.state)),
                "matron second catch: the run restarts from the last save, the body whole, out of her hands");
        require(p.player_body_length_m > 1.75, "matron claimed: the run starts again with the body's full length");
        std::cout << "PASS scraperx_sim matron second catch: path=" << path << "\n";
    }
    // ---- two grabs: the first tears an arm off and sets him down, her hands
    //      off him kRegrabAfterTearS; the second is the fatality, its death
    //      restarts the run from the last save: nothing faked between them ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(1, 3.0);   // the first: set back on her deck
        s.debug_matron_armplay(1);
        std::uint64_t most_tears = 0;
        int path = 0;
        int second_meet = 0;
        std::uint64_t deaths_before_second = 0;
        double set_down_s = -1.0;
        double second_grab_s = -1.0;
        bool was_held = false;
        const auto releases = s.matron_state().fatal_release_beat;
        for (int i = 0; i < 400 * 90 && s.snapshot().death_count == 0; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            const auto q = s.snapshot();
            if (q.arm_tear_count != most_tears && std::getenv("ARM_DEBUG") != nullptr) {
                std::cout << "    tear " << q.arm_tear_count << " at " << i / 90.0 << " side " << q.last_severed_side
                          << " L/R " << q.left_arm_lost << q.right_arm_lost << " st " << int(s.matron_state().state) << "\n";
            }
            most_tears = std::max<std::uint64_t>(most_tears, q.arm_tear_count);
            path = std::max<int>(path, s.matron_state().fatal_path);
            if (was_held && !q.held_by_matron && q.arm_tear_count == 1 && set_down_s < 0.0) {
                set_down_s = i / 90.0;   // her fingers open on the one-armed body
            }
            was_held = q.held_by_matron;
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
                second_grab_s = i / 90.0;
                deaths_before_second = q.death_count;
            }
        }
        const auto p = s.snapshot();
        std::cout << "  matron two tears: tears " << most_tears << " second meet " << second_meet << " path " << path
                  << " set down " << set_down_s << " s, second grab " << second_grab_s << " s, releases " << releases
                  << "->" << s.matron_state().fatal_release_beat << " deaths " << p.death_count << " (before the second "
                  << deaths_before_second << ") ended " << p.runs_ended_by_matron << " state "
                  << int(s.matron_state().state) << " grabs " << s.matron_state().grabs << "\n";
        require(second_meet == 2 && deaths_before_second == 0, "matron two tears: no death on the way");
        require(second_grab_s - set_down_s >= static_cast<double>(Matron::kRegrabAfterTearS),
                "matron two tears: her hands off him kRegrabAfterTearS after the arm");
        require(most_tears == 1 && path != 0 && p.death_count == 1 && p.runs_ended_by_matron == 0,
                "matron two tears: two grabs, one arm, and the second is the fatality: a death");
        require(!p.left_arm_lost && !p.right_arm_lost && p.death_count == 1,
                "matron two tears: the run restarts from the last save, whole");
        std::cout << "PASS scraperx_sim matron two tears: regrab_after_s=" << second_grab_s - set_down_s << "\n";
    }
    require_arm_reach("matron-arm");
}

// The fatality (the owner, 2026-10-28; sim/matron.hpp kFatalTargetS): her
// second catch, his one arm left. She carries him to the nearest edge with a
// killing fall under it and holds him out over it upside down by the ankle,
// her arm out; his own blow in the window frees him onto her deck, alive;
// else she lets go, gravity takes him down, the landing kills him, and the
// run restarts from the checkpoint. No edge in reach: the floor version
// (Lay, Pin, Wipe, Black), ending the same way. Nothing teleports but the
// death's restore.
namespace {
constexpr int kStaggering = static_cast<int>(Matron::State::Staggering);

double dist3(const scraperx::sim::Vector3 &a, const scraperx::sim::Vector3 &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// His one arm already gone (her first catch's), a still body caught by the
// east band's rail (the act's rail) at `z`, her beside him: her second catch.
void one_armed_at_the_rail(Simulation &s, const double z) {
    s.place_matron_home(154.0F);
    s.debug_lose_arm(0);
    at_the_rail(s, z);
}

// Her stage held dangerous and the player still, as the rail's tests have it.
void still_dangerous(Simulation &s) {
    s.debug_set_matron_stage(1);
    (void)s.set_move_input(0.0, 0.0);
}

struct FatalRun {
    double catch_s = -1.0, grip_s = -1.0, release_s = -1.0, death_s = -1.0, claimed_s = -1.0, let_go_s = -1.0;
    int hang_ticks = 0;
    bool hand_out = true;         // her hand out and on him every tick of the hang
    double hand_to_ankle = 0.0;   // most: her hand to the held ankle point, in the hang
    double ankle_to_feet = 0.0;   // most: the held point to the thigh her hand takes (kHeldBelowMiddleM), in the hang
    double least_down = 1.0;      // least: how far his body's up points down (1 straight down), in the hang
    double least_past = 1.0e9;    // least: how far out past the edge his body hangs, in the hang
    // His window (the hang's first kFatalHangWindowS): its ticks, the most
    // his shoulder is from her body (his punch's reach), and the least his
    // middle is out past the edge.
    int window_ticks = 0;
    double window_reach = -1.0, window_least_past = 1.0e9;
    double let_go_past = 0.0;     // how far past the edge he was as her fingers opened (< 0: over her deck)
    // Set down (not her fatality's let-go): in the 1.5 s after her fingers
    // opened, his fastest fall and his lowest middle.
    double set_down_fall_mps = 0.0, set_down_low_y = 1.0e9;
    double fall_mps_1s = -1.0;    // his speed down one second after she let go of him over the drop
    double fall_m_1s = 0.0;       // and how far he had fallen by then
    double release_down_mps = 0.0;   // his speed down as her fingers opened (her arm on its way out)
    double max_step = 0.0;        // his biggest move in a tick while held
    double claimed_step = 0.0;    // and while hers (the floor version's beats)
    // From her let-go to his death: landings he lived and hits he felt (none:
    // the first meeting kills), and the death's speed and kind.
    std::uint64_t fall_landings = 0, fall_hits = 0, pins = 0;
    double death_speed = 0.0;
    bool death_side_on = false;
    int path = 0;
    bool staggered = false;
    std::vector<int> beats;
    std::uint64_t deaths_start = 0, deaths_end = 0, deaths_held = 0;
    std::uint32_t releases_start = 0, escapes_start = 0;
    scraperx::sim::Vector3 checkpoint{}, edge{};
    double out_x = 0.0, out_z = 0.0;
    scraperx::sim::MatronSnapshot m;
    scraperx::sim::Snapshot p;
};

double past_edge(const FatalRun &r, const scraperx::sim::Vector3 &at) {
    return (at.x - r.edge.x) * r.out_x + (at.z - r.edge.z) * r.out_z;
}

// His punch's reach, shoulder to fist (the player's punch), and how far his
// shoulder (0.25 m along his body's up from his middle: the punch's start)
// is from her body (her capsule: kHalfHeight either side of her middle,
// kRadius round it).
constexpr double kStrikeReachM = 0.75;
double fist_to_her(const scraperx::sim::Snapshot &p, const scraperx::sim::MatronSnapshot &m) {
    const double qx = p.player_rot_x, qy = p.player_rot_y, qz = p.player_rot_z, qw = p.player_rot_w;
    const double ux = 2.0 * (qx * qy - qw * qz), uy = 1.0 - 2.0 * (qx * qx + qz * qz), uz = 2.0 * (qy * qz + qw * qx);
    const scraperx::sim::Vector3 shoulder{p.player_position.x + 0.25 * ux, p.player_position.y + 0.25 * uy,
                                          p.player_position.z + 0.25 * uz};
    const double half = static_cast<double>(Matron::kHalfHeight);
    const scraperx::sim::Vector3 axis{m.position.x, std::clamp(shoulder.y, m.position.y - half, m.position.y + half),
                                      m.position.z};
    return dist3(shoulder, axis) - static_cast<double>(Matron::kRadius);
}

// Her let-go was into a fall that kills: from her fingers opening to his
// death, no landing he lived and no hit he felt, and the death's speed past
// the world's lethal one (a landing's, or side-on a hit's).
bool lethal_first_meeting(const FatalRun &r) {
    constexpr double kLethalHitMps = 10.0;   // kHitLethalSpeed
    const double lethal = r.death_side_on ? kLethalHitMps : Simulation::kLethalImpactSpeedMps;
    return r.release_s >= 0.0 && r.death_s > r.release_s && r.fall_landings == 0 && r.fall_hits == 0 &&
           r.death_speed >= lethal;
}

// Her fatality tick by tick, till his death (and its restore, that same
// tick) or the hold's end with him alive and on his feet. `input(i, m)` runs
// before each tick.
template <typename Input>
FatalRun fatal_run(Simulation &s, const double seconds, Input input) {
    FatalRun r;
    r.deaths_start = s.snapshot().death_count;
    r.checkpoint = s.snapshot().checkpoint_position;
    r.releases_start = s.matron_state().fatal_release_beat;
    r.escapes_start = s.matron_state().fatal_escapes;
    const auto catches = s.matron_state().catches;
    const std::uint64_t pins_start = s.snapshot().knee_pin_count;
    bool held_before = false;
    bool claimed_before = false;
    scraperx::sim::Vector3 last{};
    double release_y = 0.0;
    std::uint64_t landings_at_release = 0, hits_at_release = 0;
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        input(i, s.matron_state());
        tick(s);
        const double t = i / 90.0;
        const auto m = s.matron_state();
        const auto p = s.snapshot();
        if (r.catch_s < 0.0 && m.catches != catches) {
            r.catch_s = t;
        }
        if (p.death_count != r.deaths_start) {
            r.death_s = t;
            r.deaths_held += held_before ? 1U : 0U;
            r.death_speed = p.death_speed_mps;
            r.death_side_on = p.death_side_on;
            break;
        }
        if (r.release_s >= 0.0) {
            r.fall_landings = p.landing_count - landings_at_release;
            r.fall_hits = p.hit_count - hits_at_release;
        }
        if (p.claimed_by_matron) {
            r.pins = std::max(r.pins, p.knee_pin_count - pins_start);
            if (claimed_before) {
                r.claimed_step = std::max(r.claimed_step, dist3(p.player_position, last));
            }
        }
        claimed_before = p.claimed_by_matron;
        if (m.fatal_path != 0) {
            r.path = m.fatal_path;
        }
        if (m.fatal_path == static_cast<int>(Matron::FatalPath::Hang)) {
            // The edge of the hang (kept once she has hauled him back in).
            r.edge = m.fatal_edge;
            r.out_x = m.fatal_out_x;
            r.out_z = m.fatal_out_z;
        }
        if (p.held_by_matron) {
            if (r.grip_s < 0.0) {
                r.grip_s = t;
            }
            if (held_before) {
                r.max_step = std::max(r.max_step, dist3(p.player_position, last));
            }
        }
        if (m.fatal_hang) {
            ++r.hang_ticks;
            r.hand_out = r.hand_out && p.matron_hand_present && p.held_by_matron;
            r.hand_to_ankle = std::max(r.hand_to_ankle, dist3(p.matron_hand, p.held_ankle));
            // His body's up in the world, from its rotation; the thigh her
            // hand takes, Matron::kHeldBelowMiddleM down it from its middle
            // (a real arm's hold: the owner, 2026-10-29, her arm was "like 12
            // feet long"; by the ankle a 0.6 m arm drags his head on her deck).
            const double qx = p.player_rot_x, qy = p.player_rot_y, qz = p.player_rot_z, qw = p.player_rot_w;
            const double ux = 2.0 * (qx * qy - qw * qz);
            const double uy = up_y(p);
            const double uz = 2.0 * (qy * qz + qw * qx);
            const double held = static_cast<double>(Matron::kHeldBelowMiddleM);
            const scraperx::sim::Vector3 feet{p.player_position.x - held * ux, p.player_position.y - held * uy,
                                              p.player_position.z - held * uz};
            r.ankle_to_feet = std::max(r.ankle_to_feet, dist3(p.held_ankle, feet));
            r.least_down = std::min(r.least_down, -uy);
            r.least_past = std::min(r.least_past, past_edge(r, p.player_position));
            if (m.fatal_window_s < static_cast<double>(Matron::kFatalHangWindowS)) {
                ++r.window_ticks;
                r.window_reach = std::max(r.window_reach, fist_to_her(p, m));
                r.window_least_past = std::min(r.window_least_past, past_edge(r, p.player_position));
            }
        }
        if (held_before && !p.held_by_matron && r.let_go_s < 0.0) {
            r.let_go_s = t;
            r.let_go_past = past_edge(r, p.player_position);
        }
        if (r.release_s < 0.0 && m.fatal_release_beat != r.releases_start) {
            r.release_s = t;
            release_y = p.player_position.y;
            r.release_down_mps = -p.player_linear_velocity.y;
            landings_at_release = p.landing_count;
            hits_at_release = p.hit_count;
        }
        if (r.release_s >= 0.0 && r.fall_mps_1s < 0.0 && t >= r.release_s + 1.0) {
            r.fall_mps_1s = -p.player_linear_velocity.y;
            r.fall_m_1s = release_y - p.player_position.y;
        }
        if (r.let_go_s >= 0.0 && r.release_s < 0.0 && t <= r.let_go_s + 1.5) {
            r.set_down_fall_mps = std::max(r.set_down_fall_mps, -p.player_linear_velocity.y);
            r.set_down_low_y = std::min(r.set_down_low_y, p.player_position.y);
        }
        if (p.claimed_by_matron) {
            if (r.claimed_s < 0.0) {
                r.claimed_s = t;
            }
            if (r.beats.empty() || r.beats.back() != p.fatality_beat) {
                r.beats.push_back(p.fatality_beat);
            }
        }
        r.staggered = r.staggered || m.state == kStaggering;
        held_before = p.held_by_matron;
        last = p.player_position;
        if (r.grip_s >= 0.0 && !p.held_by_matron && !p.player_free_rotation &&
            !Matron::holding_state(static_cast<Matron::State>(m.state)) && m.state != kRelease) {
            // Over with him alive and on his feet: let the landing settle.
            (void)s.advance_frame(2.0);
            break;
        }
    }
    r.m = s.matron_state();
    r.p = s.snapshot();
    r.deaths_end = r.p.death_count;
    return r;
}

void report_fatal(const char *name, const FatalRun &r) {
    std::cout << "  " << name << ": path " << r.path << ", catch " << r.catch_s << " s, grip " << r.grip_s
              << " s, release " << r.release_s << " s, death " << r.death_s << " s, claimed " << r.claimed_s
              << " s; hang " << r.hang_ticks << " ticks, hand to ankle " << r.hand_to_ankle << " m, ankle to feet "
              << r.ankle_to_feet << " m, least down " << r.least_down << ", least past " << r.least_past
              << " m; window " << r.window_ticks << " ticks, his punch's reach to her at most " << r.window_reach
              << " m, least past " << r.window_least_past << " m; let go at " << r.let_go_s << " s " << r.let_go_past << " m past the edge (" << r.edge.x << ","
              << r.edge.z << " out " << r.out_x << "," << r.out_z << "); fell " << r.fall_m_1s << " m at "
              << r.fall_mps_1s << " m/s 1 s on; beats";
    for (const int b : r.beats) {
        std::cout << " " << b;
    }
    std::cout << "; deaths " << r.deaths_start << "->" << r.deaths_end << " (held " << r.deaths_held
              << "), releases " << r.releases_start << "->" << r.m.fatal_release_beat << ", escapes "
              << r.escapes_start << "->" << r.m.fatal_escapes << ", claims " << r.m.claims << ", player "
              << r.p.player_position.x << "," << r.p.player_position.y << "," << r.p.player_position.z
              << " (checkpoint " << r.checkpoint.x << "," << r.checkpoint.y << "," << r.checkpoint.z << "), arms L/R "
              << r.p.left_arm_lost << "/" << r.p.right_arm_lost << ", max step " << r.max_step << " m (claimed "
              << r.claimed_step << " m), pins " << r.pins << ", falling: landings " << r.fall_landings << " hits "
              << r.fall_hits << ", death " << r.death_speed << " m/s" << (r.death_side_on ? " side-on" : "")
              << ", state " << int(r.m.state) << "\n";
}
} // namespace

void run_matron_fatality() {
    // ---- the hang at the act's rail, no blow: upside down out over the drop
    //      by the ankle, her hand on it every tick; she lets go, gravity takes
    //      him down, the landing kills him, and the run restarts from the
    //      checkpoint, whole. Its length, from her hands on him to her
    //      letting go, about kFatalTargetS ----
    {
        Simulation s(InitialSpawn::Deck154);
        one_armed_at_the_rail(s, -159.5);
        const FatalRun r = fatal_run(s, 40.0, [&](int, const scraperx::sim::MatronSnapshot &) { still_dangerous(s); });
        report_fatal("matron fatality hang", r);
        const int window_ticks = static_cast<int>(Matron::kFatalHangWindowS * 90.0) - 2;
        require(r.path == static_cast<int>(Matron::FatalPath::Hang) && r.hang_ticks >= window_ticks,
                "matron fatality hang: his second catch at the act's rail, held out over the drop the whole window");
        require(r.hand_out && r.hand_to_ankle <= 0.10,
                "matron fatality hang: her hand within 0.10 m of the held ankle every tick of the hang");
        require(r.ankle_to_feet <= 0.20, "matron fatality hang: the held point is the thigh her hand takes, a point of his body");
        require(r.least_down >= 0.8, "matron fatality hang: upside down every tick of the hang");
        // His window (the owner, 2026-10-28: a punch of his landed in it frees
        // him): held close, out past the edge, within his punch's reach; over
        // open air (0.5 m out) as her fingers open, below.
        require(r.window_ticks >= window_ticks && r.window_reach <= kStrikeReachM,
                "matron fatality hang: every tick of his window his punch reaches her (shoulder within kStrikeReachM)");
        require(r.window_least_past >= 0.1, "matron fatality hang: every tick of his window, out past the edge");
        require(r.max_step < 0.2, "matron fatality hang: no teleport -- his every step in her hand a real move");
        require(r.m.fatal_release_beat == r.releases_start + 1 && r.release_s > 0.0 && r.let_go_past >= 0.5,
                "matron fatality hang: no blow, she lets go of him over the drop (the release beat rises once)");
        // Let go on her arm's way out (moving down with it): gravity's second
        // on top of what he had.
        const double gained_mps = r.fall_mps_1s - r.release_down_mps;
        const double gained_m = r.fall_m_1s - r.release_down_mps;
        require(gained_mps > 8.8 && gained_mps < 10.8 && gained_m > 4.4 && gained_m < 5.4,
                "matron fatality hang: gravity takes him down: a second on, 9.8 m/s and 4.9 m more than let go at");
        require(r.death_s > r.release_s && r.deaths_end == r.deaths_start + 1 && r.deaths_held == 0,
                "matron fatality hang: the landing kills him, out of her hand");
        require(lethal_first_meeting(r), "matron fatality hang: the first thing he meets kills him");
        require(dist3(r.p.player_position, r.checkpoint) < 0.05 && !r.p.held_by_matron && !r.p.left_arm_lost &&
                    !r.p.right_arm_lost && r.p.runs_ended_by_matron == 0 && r.m.claims == 0,
                "matron fatality hang: the run restarts from the checkpoint, the body whole");
        require(r.m.fatal_path == 0 && !r.m.fatal_hang && !Matron::holding_state(static_cast<Matron::State>(r.m.state)),
                "matron fatality hang: the restore forgets her fatality");
        // The owner's bar (2026-10-28): her fingers closed to her letting go,
        // 5 to 7 s (the view fades a second after: 6 to 8 s from the catch).
        const double length = r.release_s - r.grip_s;
        require(length >= 5.0 && length <= 7.0,
                "matron fatality length: from her fingers closing on him to her letting go, 5 to 7 s");
        std::cout << "PASS scraperx_sim matron fatality hang: catch_to_release_s=" << r.release_s - r.catch_s
                  << " grip_to_release_s=" << length << " release_to_death_s="
                  << r.death_s - r.release_s << " catch_to_death_s=" << r.death_s - r.catch_s
                  << " hand_to_ankle_m=" << r.hand_to_ankle << " window_reach_m=" << r.window_reach << "\n";
    }
    // ---- across her band, off the rail: caught where he stands (the review's
    //      spots, east band, and one on the north band), her 1.6 m along from
    //      him. At each, the same hang and the same bar: she lets go 5 to 7 s
    //      after her fingers close, into a fall whose first meeting kills;
    //      every step in her hand a real move; the release beat once; the
    //      checkpoint's restore, the body whole ----
    {
        const std::array<std::array<double, 2>, 7> spots{{{19.5, -140.0},
                                                          {20.0, -150.0},
                                                          {21.0, -155.0},
                                                          {21.5, -145.0},
                                                          {21.5, -150.0},
                                                          {22.5, -145.0},
                                                          {-6.0, -129.0}}};
        for (const auto &spot : spots) {
            Simulation s(InitialSpawn::Deck154);
            s.place_matron_home(154.0F);
            s.debug_lose_arm(0);
            caught_at(s, spot[0], spot[1], spot[0] + 0.3, spot[1] + 1.6);
            const FatalRun r = fatal_run(s, 40.0, [&](int, const scraperx::sim::MatronSnapshot &) { still_dangerous(s); });
            std::array<char, 32> name{};
            (void)std::snprintf(name.data(), name.size(), "(%.1f, %.1f)", spot[0], spot[1]);
            const std::string at(name.data());
            report_fatal(("matron fatality band " + at).c_str(), r);
            const double length = r.release_s - r.grip_s;
            require(r.path == static_cast<int>(Matron::FatalPath::Hang) && r.release_s > 0.0,
                    ("matron fatality band " + at + ": held out over a drop that kills, and let go").c_str());
            require(length >= 5.0 && length <= 7.0,
                    ("matron fatality band " + at + ": her fingers closing to her letting go, 5 to 7 s").c_str());
            require(lethal_first_meeting(r) && r.deaths_end == r.deaths_start + 1 && r.deaths_held == 0,
                    ("matron fatality band " + at + ": the first thing he meets kills him").c_str());
            require(r.max_step < 0.2, ("matron fatality band " + at + ": no teleport while held").c_str());
            require(r.window_ticks >= static_cast<int>(Matron::kFatalHangWindowS * 90.0) - 2 &&
                        r.window_reach <= kStrikeReachM && r.window_least_past >= 0.1,
                    ("matron fatality band " + at + ": every tick of his window, out past the edge, his punch reaches her")
                        .c_str());
            require(r.m.fatal_release_beat == r.releases_start + 1,
                    ("matron fatality band " + at + ": the release beat, once").c_str());
            require(dist3(r.p.player_position, r.checkpoint) < 0.05 && !r.p.held_by_matron && !r.p.left_arm_lost &&
                        !r.p.right_arm_lost && r.p.runs_ended_by_matron == 0 && r.m.claims == 0 && r.m.fatal_path == 0,
                    ("matron fatality band " + at + ": the run restarts from the checkpoint, whole").c_str());
            std::cout << "PASS scraperx_sim matron fatality band " << at << ": grip_to_release_s=" << length
                      << " catch_to_release_s=" << r.release_s - r.catch_s << " window_reach_m=" << r.window_reach
                      << " release_to_death_s=" << r.death_s - r.release_s << " death_mps=" << r.death_speed
                      << " max_step_m=" << r.max_step << "\n";
        }
    }
    // ---- his blow in the window (his punch's tag, a blow too small to shift
    //      her): she reels back and sets him down on her deck, not into the
    //      shaft; alive, his one arm left. At the act's rail, and at the
    //      tower's outside off the east band (its deck's edge beam out past
    //      the lip). Her fingers open only with her deck close under his body:
    //      he drops onto it, slow, and lies on it ----
    for (int where = 0; where < 2; ++where) {
        Simulation s(InitialSpawn::Deck154);
        if (where == 0) {
            one_armed_at_the_rail(s, -159.5);
        } else {
            s.place_matron_home(154.0F);
            s.debug_lose_arm(0);
            caught_at(s, 22.5, -145.0, 22.8, -143.4);
        }
        const std::string name = where == 0 ? "matron fatality blow" : "matron fatality blow (22.5, -145.0)";
        bool thrown = false;
        double blow_reach = -1.0;   // his shoulder to her body as his blow lands
        const FatalRun r = fatal_run(s, 40.0, [&](int, const scraperx::sim::MatronSnapshot &m) {
            still_dangerous(s);
            if (!thrown && m.fatal_hang && m.fatal_window_s >= 1.0) {
                thrown = true;
                blow_reach = fist_to_her(s.snapshot(), m);
                s.debug_matron_player_blow();
            }
        });
        report_fatal(name.c_str(), r);
        std::cout << "  " << name << ": his punch's reach to her as it lands " << blow_reach << " m; set down, fastest fall "
                  << r.set_down_fall_mps << " m/s, lowest " << r.set_down_low_y << "\n";
        require(thrown && blow_reach >= 0.0 && blow_reach <= kStrikeReachM,
                (name + ": in his window his punch reaches her (shoulder within kStrikeReachM of her body)").c_str());
        require(thrown && r.path == static_cast<int>(Matron::FatalPath::Hang) && r.m.fatal_escapes == r.escapes_start + 1,
                (name + ": his blow lands in the window, and frees him").c_str());
        require(r.m.fatal_release_beat == r.releases_start && r.let_go_past < 0.0,
                (name + ": she sets him down on her deck, never lets go of him over the drop").c_str());
        require(r.set_down_fall_mps < 4.0 && r.set_down_low_y > 153.5,
                (name + ": her fingers open with her deck close under him: a short drop onto it").c_str());
        require(r.staggered, (name + ": she staggers").c_str());
        require(r.deaths_end == r.deaths_start && r.p.one_arm && r.p.left_arm_lost && !r.p.right_arm_lost &&
                    r.p.player_position.y > 153.5 && r.p.player_position.y < 156.5 && past_edge(r, r.p.player_position) < -0.2,
                (name + ": alive on her deck, his one arm left").c_str());
        std::cout << "PASS scraperx_sim " << name << ": blow_reach_m=" << blow_reach << " let_go_past_m=" << r.let_go_past
                  << " set_down_fall_mps=" << r.set_down_fall_mps << "\n";
    }
    // ---- no edge in her reach: the floor version, her beats cut short (Lay,
    //      Pin, Wipe, Black), about kFatalFloorS, and the same restart ----
    {
        Simulation s(InitialSpawn::Deck154);
        one_armed_at_the_rail(s, -159.5);
        s.debug_matron_rule_out_hang();
        const FatalRun r = fatal_run(s, 40.0, [&](int, const scraperx::sim::MatronSnapshot &) { still_dangerous(s); });
        report_fatal("matron fatality floor", r);
        const std::vector<int> floor_beats{0, 1, 8, 9};
        require(r.path == static_cast<int>(Matron::FatalPath::Floor) && r.beats == floor_beats && r.claimed_s > 0.0,
                "matron fatality floor: no edge, her floor version: laid down, her knee, her apron, black");
        require(r.p.leg_tear_count == 0 && r.m.fatal_release_beat == r.releases_start,
                "matron fatality floor: no leg torn, nobody let go over a drop");
        require(r.pins == 1, "matron fatality floor: her knee on the chest");
        require(r.max_step < 0.2 && r.claimed_step < 0.2,
                "matron fatality floor: no teleport -- his every step in her hand and under her knee a real move");
        require(r.death_s > r.claimed_s && r.deaths_end == r.deaths_start + 1 &&
                    dist3(r.p.player_position, r.checkpoint) < 0.05 && !r.p.left_arm_lost && !r.p.right_arm_lost &&
                    r.p.runs_ended_by_matron == 0,
                "matron fatality floor: a death, and the run restarts from the checkpoint, whole");
        const double beats_s = r.death_s - r.claimed_s;
        require(std::abs(beats_s - static_cast<double>(Matron::kFatalFloorS)) <= 1.0,
                "matron fatality length: the floor version's beats, about kFatalFloorS");
        std::cout << "PASS scraperx_sim matron fatality floor: beats_s=" << beats_s
                  << " grip_to_death_s=" << r.death_s - r.grip_s << " catch_to_death_s=" << r.death_s - r.catch_s
                  << "\n";
    }
    // ---- no let-go foreseen to kill him (as if the fall under him were one
    //      he would live): she never lets go over the drop; by kFatalLetGoByS
    //      she hauls him back in, sets him down on her deck, and her floor
    //      version ends it, the same restart ----
    {
        Simulation s(InitialSpawn::Deck154);
        one_armed_at_the_rail(s, -159.5);
        s.debug_matron_foresee_no_kill();
        const FatalRun r = fatal_run(s, 40.0, [&](int, const scraperx::sim::MatronSnapshot &) { still_dangerous(s); });
        report_fatal("matron fatality held on", r);
        std::cout << "  matron fatality held on: set down, fastest fall " << r.set_down_fall_mps << " m/s, lowest "
                  << r.set_down_low_y << "\n";
        require(r.hang_ticks > 0 && r.m.fatal_release_beat == r.releases_start,
                "matron fatality held on: out over the drop, and never let go there");
        require(r.let_go_s - r.grip_s >= static_cast<double>(Matron::kFatalLetGoByS) && r.let_go_past < 0.0 &&
                    r.set_down_fall_mps < 4.0 && r.set_down_low_y > 153.5,
                "matron fatality held on: by kFatalLetGoByS hauled back in, set down on her deck");
        const std::vector<int> pin_beats{1, 8, 9};
        require(r.path == static_cast<int>(Matron::FatalPath::Floor) && r.claimed_s >= r.let_go_s &&
                    r.beats == pin_beats && r.pins == 1,
                "matron fatality held on: then her floor version where he lies: her knee, her apron, black");
        require(r.max_step < 0.2 && r.claimed_step < 0.2, "matron fatality held on: no teleport");
        require(r.death_s > r.claimed_s && r.deaths_end == r.deaths_start + 1 && r.deaths_held == 0 &&
                    dist3(r.p.player_position, r.checkpoint) < 0.05 && !r.p.left_arm_lost && !r.p.right_arm_lost &&
                    r.p.runs_ended_by_matron == 0,
                "matron fatality held on: a death, and the run restarts from the checkpoint, whole");
        std::cout << "PASS scraperx_sim matron fatality held on: grip_to_set_down_s=" << r.let_go_s - r.grip_s
                  << " grip_to_death_s=" << r.death_s - r.grip_s << "\n";
    }
    // ---- a save and a load mid-hang: what the death would have been. Two
    //      runs alike to the window's middle save the same bytes; one plays
    //      the fatality out to its death, the other loads its save there; the
    //      restore forgets her fatality, and the two go on alike ----
    {
        const auto to_window = [](Simulation &s) {
            one_armed_at_the_rail(s, -159.5);
            for (int i = 0; i < 30 * 90; ++i) {
                still_dangerous(s);
                tick(s);
                const auto m = s.matron_state();
                if (m.fatal_hang && m.fatal_window_s >= 1.0) {
                    return true;
                }
            }
            return false;
        };
        Simulation a(InitialSpawn::Deck154);
        Simulation b(InitialSpawn::Deck154);
        require(to_window(a) && to_window(b), "matron fatality save: two runs alike, both in the hang's window");
        require(a.snapshot().tick_index == b.snapshot().tick_index &&
                    dist3(a.snapshot().player_position, b.snapshot().player_position) < 1.0e-6,
                "matron fatality save: two runs alike, at the same tick of the hang");
        const std::vector<std::uint8_t> saved_b = b.save_game();
        require(!saved_b.empty(), "matron fatality save: saved mid-hang");
        const FatalRun ra = fatal_run(a, 30.0, [&](int, const scraperx::sim::MatronSnapshot &) { still_dangerous(a); });
        require(ra.deaths_end == ra.deaths_start + 1, "matron fatality save: played out, the fatality ends in a death");
        const auto releases_b = b.matron_state().fatal_release_beat;
        require(b.load_game(saved_b.data(), saved_b.size()), "matron fatality save: the save loads mid-hang");
        for (int i = 0; i < 3 * 90; ++i) {
            still_dangerous(a);
            still_dangerous(b);
            tick(a);
            tick(b);
        }
        const auto pa = a.snapshot();
        const auto pb = b.snapshot();
        const auto ma = a.matron_state();
        const auto mb = b.matron_state();
        std::cout << "  matron fatality save: after, a at " << pa.player_position.x << "," << pa.player_position.y << ","
                  << pa.player_position.z << " her " << int(ma.state) << " at " << ma.position.x << "," << ma.position.y
                  << "," << ma.position.z << "; b at " << pb.player_position.x << "," << pb.player_position.y << ","
                  << pb.player_position.z << " her " << int(mb.state) << " at " << mb.position.x << ","
                  << mb.position.y << "," << mb.position.z << "; paths " << int(ma.fatal_path) << "/"
                  << int(mb.fatal_path) << " releases b " << releases_b << "->" << mb.fatal_release_beat << "\n";
        require(!pb.held_by_matron && !pb.player_free_rotation && mb.fatal_path == 0 && !mb.fatal_hang &&
                    mb.fatal_release_beat == releases_b && !pb.left_arm_lost && !pb.right_arm_lost,
                "matron fatality save: loaded mid-hang, her fatality is forgotten and the body whole");
        require(dist3(pa.player_position, pb.player_position) < 0.02 && ma.state == mb.state &&
                    dist3(ma.position, mb.position) < 0.05 && ma.fatal_path == mb.fatal_path,
                "matron fatality save: a load mid-hang gives what the death gave (deterministic)");
        std::cout << "PASS scraperx_sim matron fatality save: load mid-hang == the death's restore\n";
    }
    require_arm_reach("matron-fatality");
}

// One good arm, on a built route (a stub out from a wall, the wall's lip
// higher, far from the tower): caught one-handed on the lower lip, the hang under the
// good hand; a lunge up to the higher lip caught at its dead point; a miss
// when the hand is early; the heel hook up over the top.
namespace {
std::vector<Simulation::TestExtraSolid> one_arm_route() {
    return {
        {300.0, 5.5, 302.0, 4.0F, 0.5F, 6.0F, true},      // the floor, top 6
        {300.0, 8.0, 303.55, 0.4F, 2.0F, 0.45F, true},     // a stub out from the wall, its lip at 10 (face z 303.1)
        {300.0, 8.425, 305.0, 3.0F, 2.425F, 1.0F, true},   // the wall behind, its lip at 10.85 (face z 304)
    };
}

bool hang_on_lower(Simulation &s) {
    (void)s.set_facing(0.0, 1.0);
    s.debug_launch_player(scraperx::sim::Vector3{300.0, 9.1, 302.6}, scraperx::sim::Vector3{0.0, 0.0, 0.3});
    for (int i = 0; i < 90; ++i) {
        (void)s.set_facing(0.0, 1.0);
        (void)s.set_move_input(0.0, 1.0);
        tick(s);
        if (s.snapshot().traversal_state == scraperx::sim::TraversalState::Hanging) {
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    for (int i = 0; i < 135; ++i) {   // the swing stilled
        (void)s.set_facing(0.0, 1.0);
        tick(s);
    }
    return s.snapshot().traversal_state == scraperx::sim::TraversalState::Hanging;
}

// Launch the lunge; press Action `offset_s` from the dead point (vy to 0).
std::uint64_t lunge(Simulation &s, const double offset_s, double &error_s, const double aside = 0.0) {
    (void)s.set_move_input(aside, 0.0);
    (void)s.request_jump();
    tick(s);
    bool apex = false;
    int after = 0;
    const int want = static_cast<int>(std::lround(offset_s * 90.0));
    (void)s.set_move_input(0.0, 0.0);
    double last_vy = s.snapshot().player_linear_velocity.y;
    for (int i = 0; i < 180; ++i) {
        (void)s.set_facing(0.0, 1.0);
        const double vy = s.snapshot().player_linear_velocity.y;
        if (!apex && last_vy > 0.0 && vy <= 0.0) {
            apex = true;
        }
        last_vy = vy;
        if (want < 0 && !apex && vy > 0.0 && vy <= -want * 9.81 / 90.0 + 1.0e-6) {
            (void)s.request_traversal();
            tick(s);
            break;
        }
        if (apex && after++ == std::max(0, want)) {
            (void)s.request_traversal();
            tick(s);
            break;
        }
        tick(s);
    }
    for (int i = 0; i < 30; ++i) {
        tick(s);
    }
    error_s = s.snapshot().dyno_timing_error_s;
    return s.snapshot().dyno_catch_count;
}
} // namespace

void run_one_arm_route() {
    // At the dead point: caught.
    {
        Simulation s(InitialSpawn::Deck154, one_arm_route());
        s.place_matron_home(154.0F);
        s.debug_lose_arm(0);
        require(hang_on_lower(s), "one arm route: caught one-handed on the lower lip");
        const auto h = s.snapshot();
        std::cout << "  one arm route: hanging on hand " << h.hang_hand_side << " at " << h.player_position.x << ","
                  << h.player_position.y << "," << h.player_position.z << "\n";
        require(h.hang_hand_side == 1 && h.one_arm, "one arm route: the hang is on the good (right) hand");
        double error = 0.0;
        const auto caught = lunge(s, 0.0, error, 1.0);   // up and over to the right
        const auto p = s.snapshot();
        std::cout << "  one arm route: lunge refused " << p.dyno_refused_count << " heel " << p.heel_hook_count << " launches " << p.dyno_launch_count << " caught " << caught << " misses "
                  << p.dyno_miss_count << " error " << error << " s, at y " << p.player_position.y << " state "
                  << int(p.traversal_state) << "\n";
        require(p.dyno_launch_count == 1 && caught == 1 && std::abs(error) <= 0.12 &&
                    p.traversal_state == scraperx::sim::TraversalState::Hanging && p.player_position.y > 9.15,
                "one arm route: the lunge caught at its dead point, hanging from the upper lip");
        // The heel hook: still first, then up over.
        for (int i = 0; i < 135; ++i) {
            (void)s.set_facing(0.0, 1.0);
            tick(s);
        }
        (void)s.request_traversal();
        for (int i = 0; i < 270; ++i) {
            (void)s.set_facing(0.0, 1.0);
            tick(s);
        }
        const auto top = s.snapshot();
        std::cout << "  one arm route: heel hooks " << top.heel_hook_count << " refused " << top.heel_hook_refused_count
                  << ", on top at " << top.player_position.y << " grounded " << top.player_grounded << " deaths "
                  << top.death_count << "\n";
        require(top.heel_hook_count == 1 && top.player_grounded && top.player_position.y > 11.5 &&
                    top.player_position.z > 304.2 && top.death_count == 0,
                "one arm route: the heel hook puts the body on top");
    }
    // Early: the hand finds nothing, and the body falls.
    {
        Simulation s(InitialSpawn::Deck154, one_arm_route());
        s.place_matron_home(154.0F);
        s.debug_lose_arm(1);
        require(hang_on_lower(s), "one arm route: caught (left hand)");
        require(s.snapshot().hang_hand_side == 0, "one arm route: the hang is on the left hand");
        double error = 0.0;
        const auto caught = lunge(s, -0.25, error);
        for (int i = 0; i < 180; ++i) {
            tick(s);
        }
        const auto p = s.snapshot();
        std::cout << "  one arm route: early press error " << error << " s, caught " << caught << " misses "
                  << p.dyno_miss_count << ", down at " << p.player_position.y << " grounded " << p.player_grounded << "\n";
        require(caught == 0 && p.dyno_miss_count == 1 && error < -0.12 && p.player_grounded &&
                    p.player_position.y < 7.5 && p.death_count == 0,
                "one arm route: early, the lunge misses and the body falls to the floor");
    }
    // Two arms: no lunge or heel hook; the plain pull-up still.
    {
        Simulation s(InitialSpawn::Deck154, one_arm_route());
        s.place_matron_home(154.0F);
        require(hang_on_lower(s) && s.snapshot().hang_hand_side == -1, "one arm route: two hands, centred");
        (void)s.request_traversal();
        for (int i = 0; i < 180; ++i) {
            tick(s);
        }
        const auto p = s.snapshot();
        require(p.dyno_launch_count == 0 && p.heel_hook_count == 0 && p.player_position.y > 10.5,
                "one arm route: two arms pull up as before");
    }
    std::cout << "PASS scraperx_sim one arm route\n";
}
