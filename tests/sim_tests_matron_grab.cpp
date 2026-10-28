// Her hold (sim/matron_hold.cpp, sim/physics_world_grab.cpp): at every stage,
// her hand on a player is no death. She takes an ankle, carries them upside
// down to the shaft's rail, holds them out over the drop, tears an arm off,
// and lets go aimed at a ledge, or sets them back on the deck. Her grip is
// finite: a still body hangs; a thrashing one tears free (once she has its
// wrist, leaving the arm in her other hand). Nothing teleports. Caught, a
// blow that is not his own frees nobody. These meet her fully changed, as
// written; her stage changes none of it.

#include "sim_tests.hpp"

#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>
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

// The fatality's beats (matron.hpp Fatal), Lay to Black, none skipped.
const std::vector<int> kEveryBeat{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
// Each beat as it comes, once.
void note_beat(const scraperx::sim::Snapshot &p, std::vector<int> &beats) {
    if (p.claimed_by_matron && (beats.empty() || beats.back() != p.fatality_beat)) {
        beats.push_back(p.fatality_beat);
    }
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
}

// By the east band's rail at `z` (x 18), her 1.6 m along the band.
void at_the_rail(Simulation &s, const double z) { caught_at(s, 18.0, z, 18.3, z + 1.6); }
} // namespace

void run_matron_grab() {
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
            const HoldRun r = hold_run(s, 60.0, [&](int, const scraperx::sim::MatronSnapshot &) {
                s.debug_set_matron_stage(1);
                (void)s.set_move_input(0.0, 0.0);
            });
            std::cout << "  matron rail: z " << z << ", grabs " << r.m.grabs << " torn " << r.m.grips_torn
                      << " retakes " << r.m.retakes << " armplays " << r.m.armplays << " tears "
                      << r.p.arm_tear_count << " claims " << r.m.claims << " deaths " << r.deaths_start << "->"
                      << r.deaths_end << " (held " << r.deaths_held << "), ended " << r.ended << "\n";
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
    // ---- the wrist out of her reach: as her arm play begins, the body (held
    //      out over the drop at the east band's rail) caught on the steel by a
    //      snag that holds more than her arm's pull (kHandPullN) and less than
    //      her hard grip in arm play (kGripRipFloorN). She does not give it
    //      up: kArmReachSeconds without the wrist, she re-poses, a step back
    //      with her weight in it, the snag gives, and the arm comes off within
    //      10 s of her arm play's start; no claim, no death ----
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_rail(s, -159.5);
        s.debug_matron_hold(0, 3.0);
        constexpr int kArmPlay = static_cast<int>(Matron::State::ArmPlay);
        double snagged_at = -1.0;
        double torn_at = -1.0;
        bool held_past_reach = false;
        const HoldRun r = hold_run(s, 60.0, [&](const int i, const scraperx::sim::MatronSnapshot &m) {
            s.debug_set_matron_stage(1);
            (void)s.set_move_input(0.0, 0.0);
            if (snagged_at < 0.0 && m.state == kArmPlay) {
                s.debug_snag_player(0.5 * (Matron::kHandPullN + Matron::kGripRipFloorN));
                snagged_at = i / 90.0;
            }
            if (snagged_at >= 0.0 && m.arm_reposes == 0 && s.debug_player_snagged()) {
                // The snag holds while she reaches from where she stands.
                held_past_reach = i / 90.0 - snagged_at > Matron::kArmReachSeconds - 0.1;
            }
            if (torn_at < 0.0 && s.snapshot().arm_tear_count > 0) {
                torn_at = i / 90.0;
            }
        });
        std::cout << "  matron arm snagged: snagged at " << snagged_at << " s, held past her reach " << held_past_reach
                  << ", re-poses " << r.m.arm_reposes << ", snag left " << s.debug_player_snagged() << ", torn at "
                  << torn_at << " s, tears " << r.p.arm_tear_count << " claims " << r.m.claims << " deaths "
                  << r.deaths_start << "->" << r.deaths_end << ", ended " << r.ended << "\n";
        require(snagged_at >= 0.0 && held_past_reach && r.m.arm_reposes >= 1 && !s.debug_player_snagged(),
                "matron arm snagged: the wrist out of her reach, she re-poses and her weight pulls the body off the snag");
        require(r.grabbed && r.ended && r.p.arm_tear_count == 1 && r.p.one_arm && torn_at - snagged_at < 10.0,
                "matron arm snagged: the arm comes off within 10 s of her arm play's start");
        require(r.m.claims == 0 && r.deaths_held == 0 && r.deaths_end == r.deaths_start,
                "matron arm snagged: one arm gone, no claim and no death");
        std::cout << "PASS scraperx_sim matron arm snagged: torn " << torn_at - snagged_at << " s into her arm play\n";
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
        // The idle leg checks the thrown arm's lifetime, not her hands: her
        // grab held off for it (as #11 did for the burst and vault legs), so
        // a second catch, now the fatality, does not end it (Claude, #3).
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
        s.debug_hold_matron_grab(false);
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
        s.place_matron_home(154.0F);
        s.debug_lose_arm(0);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        s.debug_matron_armplay(1);
        bool claimed = false;
        std::uint64_t ended = 0;
        std::vector<int> beats;
        for (int i = 0; i < 90 * 90 && ended == 0; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            claimed = claimed || s.snapshot().claimed_by_matron;
            note_beat(s.snapshot(), beats);
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
        require(beats == kEveryBeat, "matron claimed: every beat of it, in order");
        require(!p.left_arm_lost && !p.right_arm_lost && !p.held_by_matron && s.matron_state().state == 0,
                "matron claimed: the run starts again, the body whole, her in her locker");
        require(p.player_body_length_m > 1.75, "matron claimed: the run starts again with the body's full length");
        std::cout << "PASS scraperx_sim matron claimed\n";
    }
    // ---- two grabs, two tears, the ending: nothing faked between them ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(1, 3.0);   // the first: set back on her deck
        s.debug_matron_armplay(1);
        std::uint64_t most_tears = 0;
        bool claimed = false;
        std::uint64_t ended = 0;
        int second_meet = 0;
        std::uint64_t deaths_seen = 0;
        std::vector<int> beats;
        for (int i = 0; i < 400 * 90 && ended == 0; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            const auto q = s.snapshot();
            note_beat(q, beats);
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
        require(beats == kEveryBeat, "matron two tears: every beat of the end, in order");
        const auto p = s.snapshot();
        require(!p.left_arm_lost && !p.right_arm_lost && p.death_count == 0,
                "matron two tears: the new run is whole");
        std::cout << "PASS scraperx_sim matron two tears\n";
    }
}

// The second arm gone: the fatality, beat by beat, all of it physics. The
// knee pin, both legs torn at the hips, the torso dragged onto the skip
// lift's rope and hauled up the well as the catch's pin comes out, the knot
// parting at the jerk, the fall, her hands on her apron, black, and a whole
// body in a new run. No death is counted: the run was hers.
void run_matron_fatality() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    s.debug_lose_arm(0);
    meet_her(s, -6.0, -129.0);
    s.debug_matron_hold(0, 3.0);
    s.debug_matron_armplay(1);
    std::vector<int> order;
    std::array<double, 10> at_s{};
    double max_step = 0.0;
    double peak_leg = 0.0, peak_knot = 0.0, peak_y = 0.0, landing = 0.0, drop = 0.0;
    std::uint64_t tears = 0, kicks = 0, hauls = 0, parts = 0, lines = 0, deaths = 0, knots = 0, pins = 0;
    bool legs_seen = false;
    std::uint64_t ended = 0;
    scraperx::sim::Vector3 last{};
    bool have_last = false;
    const bool debug = std::getenv("FATAL_DEBUG") != nullptr;
    for (int i = 0; i < 200 * 90 && ended == 0; ++i) {
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const auto p = s.snapshot();
        ended = p.runs_ended_by_matron;
        if (ended != 0) {
            break;
        }
        if (p.claimed_by_matron) {
            if (order.empty() || order.back() != p.fatality_beat) {
                order.push_back(p.fatality_beat);
                at_s[static_cast<std::size_t>(p.fatality_beat)] = i / 90.0;
                if (debug) {
                    std::cout << "    beat " << p.fatality_beat << " at " << i / 90.0 << " torso " << p.player_position.x
                              << "," << p.player_position.y << "," << p.player_position.z << " her "
                              << s.matron_state().position.x << "," << s.matron_state().position.z << "\n";
                }
            }
            if (have_last && p.fatality_beat < 6) {
                const double dx = p.player_position.x - last.x, dy = p.player_position.y - last.y,
                             dz = p.player_position.z - last.z;
                max_step = std::max(max_step, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
            last = p.player_position;
            have_last = true;
            peak_leg = std::max(peak_leg, p.leg_peak_n);
            peak_knot = std::max(peak_knot, p.knot_peak_n);
            peak_y = std::max(peak_y, p.haul_peak_y);
            tears = p.leg_tear_count;
            legs_seen = legs_seen || (p.left_leg_lost && p.right_leg_lost && p.severed_leg_present[0] &&
                                      p.severed_leg_present[1]);
            if (p.left_leg_lost && p.right_leg_lost) {
                require(p.player_body_length_m < 1.0,
                        "matron fatality: both legs gone, the body is its torso, not the whole capsule");
            }
            kicks = p.brake_kick_count;
            hauls = p.haul_count;
            parts = p.knot_part_count;
            knots = p.knot_count;
            pins = p.knee_pin_count;
            landing = std::max(landing, p.fall_landing_speed_mps);
            drop = std::max(drop, p.fall_drop_m);
            lines = p.waste_not_count;
            deaths = p.death_count;
            if (debug && i % 45 == 0) {
                std::cout << "      t=" << i / 90.0 << " beat " << p.fatality_beat << " torso " << p.player_position.x
                          << "," << p.player_position.y << "," << p.player_position.z << " hand "
                          << p.matron_hand.x << "," << p.matron_hand.y << "," << p.matron_hand.z << " knee "
                          << p.matron_knee_down << " leg " << p.leg_load_n << " knot " << p.knot_load_n << " unhooks " << p.shackle_unhook_count << " collar " << p.collar_count << " her "
                          << s.matron_state().position.x << "," << s.matron_state().position.z << "\n";
            }
        }
    }
    std::cout << "  matron fatality: beats";
    for (const int b : order) {
        std::cout << " " << b << "@" << at_s[static_cast<std::size_t>(b)];
    }
    std::cout << " | pins " << pins << " leg tears " << tears << " peak hip " << peak_leg << " N, knots " << knots
              << " kicks " << kicks << " hauls " << hauls << " peak torso y " << peak_y << ", knot peak " << peak_knot
              << " N, parts " << parts << ", fell " << drop << " m landing " << landing << " m/s, lines " << lines
              << ", deaths " << deaths << ", max step " << max_step << " m, ended " << ended << "\n";
    require(order == kEveryBeat, "matron fatality: every beat, in order");
    require(pins == 1, "matron fatality: her knee on the chest");
    require(tears == 2 && legs_seen, "matron fatality: both legs torn off, bodies on the deck");
    require(peak_leg > scraperx::sim::Matron::kLegHoldN && peak_leg < 8000.0, "matron fatality: the hip's load finite");
    require(knots == 1 && kicks == 1 && hauls == 1, "matron fatality: on the rope, the pin out, hauled");
    require(peak_y > 165.0, "matron fatality: the machine takes it up the well");
    require(peak_y < 191.0, "matron fatality: it stops under the head beam, not past the sheave");
    require(parts == 1 && drop > 8.0 && landing > 10.0, "matron fatality: the knot parts and it falls");
    require(lines == 1, "matron fatality: 'Waste not.'");
    require(deaths == 0, "matron fatality: no death counted, the run was hers");
    require(max_step < 0.2, "matron fatality: no teleport (the torso's step, before the haul)");
    require(ended == 1, "matron fatality: the run ends");
    const auto p = s.snapshot();
    require(!p.left_arm_lost && !p.right_arm_lost && !p.left_leg_lost && !p.right_leg_lost && !p.claimed_by_matron &&
                p.fatality_beat == -1 && p.death_count == 0,
            "matron fatality: a new run, a whole body");
    std::cout << "PASS scraperx_sim matron fatality: peak_hip_n=" << peak_leg << " peak_y=" << peak_y
              << " landing_mps=" << landing << "\n";
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
