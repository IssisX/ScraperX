// J-060 fight contact truth (issue #320): a blow lands only when the
// striking limb really contacts a body part; a near miss misses; a guard
// limb in the path intercepts it; her blows follow the same rule; she
// attacks from her real reach. Five tests, each failing on trunk first,
// plus the measurement table Claude ordered (07:46).
//
// Trunk limits (read, not guessed): her body is ONE root capsule
// (kRadius 0.30, kHalfHeight 0.75); no upper bodies, no arms, no head as
// a separate shape. So the near miss and the guard cannot be FIXED on
// trunk: the fist's line through her arm's or head's place still meets
// the fat planning capsule. Those two tests are RED here; their fix waits
// on J-044c (upper bodies) landing, then J-044d (fist_contact). The shove
// (her hit, her reach) IS fixable on trunk: caught() has no facing or
// path check, so she shoves a player behind her and through steel, and
// shove_player writes his velocity directly (Claude 07:44, finding 5).
#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

// Settle a fresh Deck154 world and return his spawn; he faces +Z.
Vector3 settle(Simulation &s) {
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) {
        tick(s);
    }
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    return p;
}

void wait_hunting(Simulation &s, const char *what) {
    constexpr int kHunting = static_cast<int>(Matron::State::Hunting);
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                    s.matron_state().state != kHunting;
         ++i) {
        tick(s);
    }
    require(s.matron_state().state == kHunting, what);
}

// One punch at her, `ahead` m in front of him and `lateral` m to his left.
// Mirrors the measurement's setup (wait for Hunting, hands held, creep,
// re-placed to the exact distance): she is a live target, so the blow is
// judged (blow_mps), and she does not walk during the punch. Her facing
// is pinned at him (yaw pi) so her arms stay at her sides, lateral to the
// fist's line.
struct Thrown {
    bool offered = false;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double blow_mps = 0.0;
};

Thrown throw_at(const double ahead, const double lateral) {
    Simulation s(InitialSpawn::Deck154);
    const Vector3 p = settle(s);
    s.launch_matron_for_test(Vector3{p.x + lateral, p.y - 0.20, p.z + ahead});
    s.debug_hold_matron_hands(30.0);
    wait_hunting(s, "fight-contact: she hunts before the punch");
    s.debug_fix_matron_pace(0);
    s.debug_place_matron(Vector3{p.x + lateral, p.y - 0.85, p.z + ahead});
    s.debug_face_matron(3.141592653589793);
    tick(s);
    Thrown out;
    out.offered = s.snapshot().strike_offered;
    (void)s.request_strike();
    for (int i = 0; i < 40; ++i) {
        s.debug_face_matron(3.141592653589793);
        tick(s);
        out.blow_mps = std::max(out.blow_mps, s.matron_state().blow_mps);
    }
    out.hits = s.snapshot().strike_hit_count;
    out.impulse_ns = s.snapshot().strike_impulse_ns;
    return out;
}

// Her shove count before and after a 1 s window with her placed `ahead` m in
// front of him (lateral m to his left), facing `yaw`, grab held so the
// shove branch is the one that can fire. `solids`: test steel in the world.
// Launched just outside catch range, so no shove can fire during the wait;
// her facing and her spot are pinned every tick so her hunt cannot turn
// her or walk her into (or out of) range during the window.
struct ShoveWindow {
    std::uint64_t before = 0;
    std::uint64_t after = 0;
};

ShoveWindow shove_at(const double ahead, const double lateral, const double yaw,
                     const std::vector<Simulation::TestExtraSolid> &solids = {}) {
    Simulation s(InitialSpawn::Deck154, solids);
    const Vector3 p = settle(s);
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
    s.debug_hold_matron_grab(true);
    wait_hunting(s, "fight-contact: she hunts before the shove window");
    ShoveWindow out;
    out.before = s.snapshot().shove_count;
    // Her feet where the launch put them: centre at p.y - 0.20, 1.05 m over her feet.
    s.debug_place_matron(Vector3{p.x + lateral, p.y - 0.85, p.z + ahead});
    s.debug_face_matron(yaw);
    for (int i = 0; i < 90; ++i) {
        s.debug_face_matron(yaw);
        tick(s);
    }
    out.after = s.snapshot().shove_count;
    return out;
}

// The Deck154 spawn, learned from a throwaway world, so a test post can be
// placed between them before the real world is built (the strike test's
// pattern). A 1 cm steel plate, 1.5 m tall, on the line between them at the
// midpoint of a 0.6 m gap: it stands in her hand's path (her shoulder, 1.80
// m over her feet, to his chest) and takes the shove's cast.
std::vector<Simulation::TestExtraSolid> post_between() {
    Vector3 start{};
    {
        Simulation s(InitialSpawn::Deck154);
        start = settle(s);
    }
    return std::vector<Simulation::TestExtraSolid>{
        Simulation::TestExtraSolid{start.x, start.y + 0.75, start.z + 0.30, 0.005F, 0.75F, 0.005F, true}};
}

// ---- Measurement (Claude's 07:46 order): the BEFORE/AFTER table ----
// Per blow: the scoring tick (ticks from the press until the hit first
// scores), the limb point on that tick, and its signed distance to her
// capsule surface (negative = the limb is inside her body). The drawn
// limb-to-drawn-skin distance needs the live Godot runtime + the probe
// (Claude's repro); this is the scoring truth it is checked against.
struct Measured {
    int scoring_tick = -1; // -1: never scored
    double gap_m = 0.0;
    double centre_m = 0.0; // his chest to her centre at the scoring tick
    double impulse_ns = 0.0;
    std::uint64_t hits = 0;
    int move = -1;
    std::uint64_t strikes = 0;
    int phase = -1;
    bool offered = false;
};

Measured measure_blow(const double ahead, const double lateral, const int move_kind) {
    // move_kind: 0 punch (jab), 1 sweep (crouch), 2 kick (move toward her).
    // The kick test's setup (wait for Hunting, hands held): the press
    // during Emerging selects the wrong move. She is re-placed to the
    // exact distance right before the press and set to a creep, so the
    // distance at the scoring tick is the launch distance.
    Simulation s(InitialSpawn::Deck154);
    const Vector3 p = settle(s);
    s.launch_matron_for_test(Vector3{p.x + lateral, p.y - 0.20, p.z + ahead});
    s.debug_hold_matron_hands(30.0);
    wait_hunting(s, "fight-measure: she hunts before the blow");
    s.debug_fix_matron_pace(0);
    s.debug_place_matron(Vector3{p.x + lateral, p.y - 0.85, p.z + ahead});
    s.debug_face_matron(3.141592653589793);
    if (move_kind == 1) {
        (void)s.set_crouch_input(true);
        for (int i = 0; i < 10 && !s.snapshot().player_crouched; ++i) {
            tick(s);
        }
        require(s.snapshot().player_crouched, "fight-measure: crouched before the sweep");
    }
    if (move_kind == 2) {
        const Vector3 me = s.snapshot().player_position, her = s.matron_state().position;
        const double dx = her.x - me.x, dz = her.z - me.z, l = std::hypot(dx, dz);
        (void)s.set_facing(dx / l, dz / l);
        (void)s.set_move_input(dx / l, dz / l);
        tick(s);
    }
    Measured out;
    const std::uint64_t hits0 = s.snapshot().strike_hit_count;
    const std::uint64_t strikes0 = s.snapshot().strike_count;
    out.offered = s.snapshot().strike_offered;
    (void)s.request_strike();
    for (int i = 0; i < 60; ++i) {
        tick(s);
        if (i == 0) {
            (void)s.set_move_input(0.0, 0.0); // move frozen at the press; he holds still
        }
        s.debug_face_matron(3.141592653589793);
        if (s.snapshot().strike_phase != 0) {
            out.move = s.snapshot().strike_move;
        }
        if (s.snapshot().strike_hit_count > hits0) {
            out.scoring_tick = i + 1;
            const auto &snap = s.snapshot();
            const Vector3 limb = snap.strike_limb_point;
            const Vector3 her = s.matron_state().position;
            const double qy = std::max(her.y - 0.75, std::min(her.y + 0.75, limb.y));
            const double d = std::hypot(limb.x - her.x, limb.y - qy, limb.z - her.z);
            out.gap_m = d - 0.30;
            const Vector3 me = snap.player_position;
            out.centre_m = std::hypot(her.x - me.x, her.z - me.z);
            out.impulse_ns = snap.strike_impulse_ns;
            out.hits = snap.strike_hit_count - hits0;
            break;
        }
    }
    const auto &end = s.snapshot();
    out.strikes = end.strike_count - strikes0;
    out.phase = end.strike_phase;
    return out;
}

void print_blow(const char *label, const Measured &m) {
    std::cout << "  fight-measure " << label << ": scoring_tick=" << m.scoring_tick
              << " hits=" << m.hits << " impulse_ns=" << m.impulse_ns << " centre_m=" << m.centre_m
              << " limb_to_surface_m=" << m.gap_m << (m.gap_m < 0.0 ? " PENETRATED" : "")
              << " move=" << m.move << " strikes=" << m.strikes << " phase=" << m.phase
              << " offered=" << m.offered << "\n";
}
} // namespace

void run_matron_fight_measure() {
    std::cout << "J-060 fight-measure (sim side; drawn limb to drawn skin needs the live runtime)\n";
    print_blow("punch 1.05m", measure_blow(1.05, 0.0, 0));
    print_blow("kick 1.05m", measure_blow(1.05, 0.0, 2));
    print_blow("sweep 1.05m", measure_blow(1.05, 0.0, 1));
    print_blow("side-offset punch 0.75m + 0.33m", measure_blow(0.75, 0.33, 0));
    print_blow("kick 1.10m (max reach)", measure_blow(1.10, 0.0, 2));
    // Her approach: she walks in from 2 m and catches him. Closest centre
    // distance and surface gap (centre - her 0.30 - his 0.35) before the catch.
    // Setup mirrors the chase test's run_seed (place_matron_home + her
    // scan + stage + a lead): the launch alone never made her walk.
    {
        Simulation s(InitialSpawn::Deck154);
        const Vector3 p = settle(s);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "fight-measure: settle after home");
        (void)s.matron_affordances(); // her own scan, as in play
        s.debug_place_matron(Vector3{p.x, p.y - 0.85, p.z + 2.0});
        tick(s);
        s.debug_set_matron_stage(2); // Dangerous: she hunts him down
        s.debug_matron_hear(p);
        double min_centre = 1.0e9, min_gap = 1.0e9;
        int catch_tick = -1;
        const std::uint64_t catches0 = s.matron_state().catches;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            const Vector3 me = s.snapshot().player_position, her = s.matron_state().position;
            const double d = std::hypot(her.x - me.x, her.z - me.z);
            min_centre = std::min(min_centre, d);
            min_gap = std::min(min_gap, d - 0.30 - 0.35);
            if (s.matron_state().catches > catches0) {
                catch_tick = i + 1;
                break;
            }
        }
        std::cout << "  fight-measure approach: closest_centre_m=" << min_centre
                  << " closest_surface_gap_m=" << min_gap << " catch_tick=" << catch_tick
                  << (min_gap < 0.0 ? " OVERLAP" : "") << "\n";
        require(catch_tick > 0, "fight-measure: she catches him on the approach");
    }
    std::cout << "PASS scraperx_sim matron-fight-measure\n";
}
SCRAPERX_SLICE("matron-fight-measure", run_matron_fight_measure);

void run_matron_fight_contact() {
    // Order: the trunk-fixable shove/reach run before the near miss
    // and guard, which wait on J-044c (upper bodies) landing. Each
    // block still fails on trunk first; the order only decides
    // which red the run stops at.
// 1. HIT, the control: a punch dead on her chest lands once, with the
    // jab's held impulse. The correct behaviour the fix must keep.
    const Thrown hit = throw_at(1.05, 0.0);
    std::cout << "  fight-contact hit: offered=" << hit.offered << " hits=" << hit.hits
              << " impulse_ns=" << hit.impulse_ns << " blow_mps=" << hit.blow_mps << "\n";
    require(hit.offered && hit.hits == 1 && std::abs(hit.impulse_ns - 66.7) < 0.5 &&
                hit.blow_mps > 0.29,
            "fight-contact: a punch dead on her chest lands once, the jab's held impulse");

    // 2. HER HIT: her shove is a hand push: she must face him, and nothing
    // may stand between her hand and him. Grab held, so the shove branch
    // is the one that can fire.
    const ShoveWindow face_on = shove_at(0.60, 0.0, 3.141592653589793);
    std::cout << "  fight-contact shove face-on: before=" << face_on.before
              << " after=" << face_on.after << "\n";
    require(face_on.after > face_on.before,
            "fight-contact: she shoves him when she faces him, clear path, in reach");
    const ShoveWindow behind = shove_at(0.60, 0.0, 0.0); // she faces away: no shove
    std::cout << "  fight-contact shove behind her: before=" << behind.before
              << " after=" << behind.after << "\n";
    require(behind.after == behind.before,
            "fight-contact: she does not shove a player behind her, no hand reaches there");
    const ShoveWindow steel = shove_at(0.60, 0.0, 3.141592653589793, post_between());
    std::cout << "  fight-contact shove through steel: before=" << steel.before
              << " after=" << steel.after << "\n";
    require(steel.after == steel.before,
            "fight-contact: she does not shove through steel between her hand and him");

    // 3. REACH: she attacks from her real reach (her arm, 1.4 m from her
    // shoulder, kArmReachM). 0.85 m is the catch range, inside her hand's
    // reach; but her hand only reaches forward, in her facing arc.
    const ShoveWindow side = shove_at(0.0, 0.60, 0.0); // at her side, 90 degrees off: no shove
    std::cout << "  fight-contact shove at her side: before=" << side.before
              << " after=" << side.after << "\n";
    require(side.after == side.before,
            "fight-contact: she does not shove a player at her side, outside her hand's arc");
    const ShoveWindow in_reach = shove_at(0.80, 0.0, 3.141592653589793);
    std::cout << "  fight-contact shove in reach: before=" << in_reach.before
              << " after=" << in_reach.after << "\n";
    require(in_reach.after > in_reach.before,
            "fight-contact: she shoves him 0.80 m in front, inside her hand's reach");
    const ShoveWindow out_of_reach = shove_at(2.50, 0.0, 3.141592653589793);
    std::cout << "  fight-contact shove out of reach: before=" << out_of_reach.before
              << " after=" << out_of_reach.after << "\n";
    require(out_of_reach.after == out_of_reach.before,
            "fight-contact: she does not shove him 2.50 m in front, outside her hand's reach");

// 4. NEAR MISS: the fist passes 0.33 m from her axis, 0.75 m ahead. Her
    // torso is inside 0.20 m (her arms hang at 0.20 m from her axis, the
    // J-044d probe met them there) and her arm's outer surface inside
    // 0.26 m: the fist clears her body and must miss. On trunk it lands:
    // the fat capsule (0.30 + 0.06 fist) reaches 0.36 m. RED until J-044c.
    const Thrown miss = throw_at(0.75, 0.33);
    std::cout << "  fight-contact near miss: offered=" << miss.offered << " hits=" << miss.hits
              << " impulse_ns=" << miss.impulse_ns << "\n";
    require(miss.hits == 0 && miss.impulse_ns == 0.0,
            "fight-contact: a fist 0.33 m from her axis, clear of her torso and her arm, misses");

    // 5. GUARD: her arm hangs at her side, 0.20 m from her axis; she stands
    // 0.20 m to his left, 0.70 m ahead, so one arm is on the fist's line.
    // The fist should meet her arm, not her chest. On trunk her arm is
    // inside the planning capsule, so the punch credits a chest hit first.
    // RED until J-044c gives her arm a body and J-044d meets it first.
    const Thrown guard = throw_at(0.70, -0.20);
    std::cout << "  fight-contact guard: offered=" << guard.offered << " hits=" << guard.hits
              << " impulse_ns=" << guard.impulse_ns << "\n";
    require(guard.hits == 0 && guard.impulse_ns == 0.0,
            "fight-contact: a fist through her arm's place is intercepted by her arm, not her chest");

        std::cout << "PASS scraperx_sim matron-fight-contact\n";
}
SCRAPERX_SLICE("matron-fight-contact", run_matron_fight_contact);
