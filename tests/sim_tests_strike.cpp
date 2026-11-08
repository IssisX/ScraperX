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
    int arm = 1;
    std::uint64_t strikes = 0;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double her_moved_m = 0.0;
    double max_player_step_m = 0.0;
    double her_blow_mps = 0.0;
    Vector3 point{};
};

// Her `her_ahead_m` in front of him on the Deck154 start, one press: what it
// threw. `solids`: test steel in the world; `lost`, `broken`: an arm torn off,
// an arm broken (0 the left, 1 the right, -1 none) before she comes.
Thrown throw_one(const double her_ahead_m, const std::vector<Simulation::TestExtraSolid> &solids = {},
                 const int lost = -1, const int broken = -1) {
    Simulation s(InitialSpawn::Deck154, solids);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    if (lost >= 0) {
        s.debug_lose_arm(lost);
    }
    if (broken >= 0) {
        s.debug_break_arm(broken);
    }
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + her_ahead_m});
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    Thrown out;
    out.offered = s.snapshot().strike_offered;
    out.arm = s.snapshot().strike_arm;
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
        s.place_matron_home(154.0F);
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

    // A second punch after quiet must read again: the blow export returns
    // to zero once the judged window closes (6 quiet ticks), so the next
    // landing is a fresh rising edge for the grunt/haptic readers; a blow
    // that never returns to zero leaves the second punch silent.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        constexpr int kHunting = static_cast<int>(scraperx::sim::Matron::State::Hunting);
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && s.matron_state().state != kHunting; ++i) {
            tick(s);
        }
        for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            tick(s);
        }
        require(s.snapshot().strike_offered, "strike: second read, first punch on offer (setup)");
        (void)s.request_strike();
        int waited = 0;
        while (s.snapshot().strike_hit_count == 0 && waited < 60) {
            tick(s);
            ++waited;
        }
        require(s.snapshot().strike_hit_count == 1, "strike: second read, first punch lands (setup)");
        double peak1 = 0.0;
        while (s.snapshot().strike_phase != 0 && waited < 120) {
            tick(s);
            ++waited;
            peak1 = std::max(peak1, s.matron_state().blow_mps);
        }
        peak1 = std::max(peak1, s.matron_state().blow_mps);
        for (int i = 0; i < 20; ++i) {
            tick(s);
        }
        const double quiet = s.matron_state().blow_mps;
        for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            tick(s);
        }
        require(s.snapshot().strike_offered, "strike: second read, second punch on offer (setup)");
        (void)s.request_strike();
        waited = 0;
        while (s.snapshot().strike_hit_count < 2 && waited < 60) {
            tick(s);
            ++waited;
        }
        require(s.snapshot().strike_hit_count == 2, "strike: second read, second punch lands (setup)");
        double peak2 = 0.0;
        for (int i = 0; i < 10; ++i) {
            tick(s);
            peak2 = std::max(peak2, s.matron_state().blow_mps);
        }
        std::cout << "  strike second read: peak1=" << peak1 << " quiet=" << quiet << " peak2=" << peak2 << "\n";
        require(peak1 > 0.0, "strike: second read, first punch reads a blow");
        require(quiet == 0.0, "strike: second read, the blow export returns to zero after quiet");
        require(peak2 > 0.0, "strike: second read, second punch after quiet reads a blow again");
    }

    // Her hands on him (the owner's design): during her reach, before her
    // fingers close, the brace keeps Action, no punch; in her hand before
    // the rip the punch is offered and thrown, his escape, and lands on her
    // within his limit; once the rip has begun, none.
    constexpr int kGrab = static_cast<int>(scraperx::sim::Matron::State::Grab);
    constexpr int kCarry = static_cast<int>(scraperx::sim::Matron::State::Carry);
    constexpr int kArmPlay = static_cast<int>(scraperx::sim::Matron::State::ArmPlay);
    const auto until = [](Simulation &s, const auto &done) {
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !done(); ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
        }
        return done();
    };
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        require(until(s, [&] { return s.matron_state().state == kGrab && !s.snapshot().held_by_matron; }),
                "strike: her reach for him (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        require(!s.snapshot().strike_offered, "strike: during her reach Action does not offer the punch (the brace)");
        (void)s.request_strike();
        tick(s);
        require(s.snapshot().strike_count == before, "strike: no strike starts during her reach");
        require(until(s, [&] { return s.matron_state().state == kCarry && s.snapshot().held_by_matron; }),
                "strike: she carries him in her hand (setup)");
        require(s.snapshot().strike_offered, "strike: in her hand before the rip Action offers the punch (his escape)");
        const std::uint64_t hits = s.snapshot().strike_hit_count;
        const std::uint64_t tears = s.snapshot().grip_tear_count;
        double grip_peak_n = 0.0;
        (void)s.request_strike();
        for (int i = 0; i < 40; ++i) {
            tick(s);
            grip_peak_n = std::max(grip_peak_n, s.snapshot().grip_load_n);
        }
        const auto &snap = s.snapshot();
        const double most = kPlayerStrikeForceN * 3.0 * Simulation::kFixedStepSeconds + 1.0e-3;
        std::cout << "  strike in her hand: strikes=" << snap.strike_count - before << " hits=" << snap.strike_hit_count - hits
                  << " impulse_ns=" << snap.strike_impulse_ns << " grip_peak_n=" << grip_peak_n
                  << " grip_tears=" << snap.grip_tear_count - tears << "\n";
        require(snap.strike_count == before + 1 && snap.strike_hit_count == hits + 1,
                "strike: thrown in her hand, the punch lands on her");
        require(snap.strike_impulse_ns > 0.0 && snap.strike_impulse_ns <= most,
                "strike: the punch in her hand is his force held over the contact, within his limit");
        require(snap.grip_tear_count == tears && grip_peak_n < scraperx::sim::Matron::kGripMaxForceN,
                "strike: his punch in her hand tears nothing: her grip's load stays under its limit");
    }
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        meet_her(s, -6.0, -129.0);
        s.debug_matron_hold(0, 3.0);
        require(until(s, [&] { return s.matron_state().state == kArmPlay && s.snapshot().held_by_matron; }),
                "strike: her rip begun, him in her hand (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        require(!s.snapshot().strike_offered, "strike: once the rip has begun Action does not offer the punch");
        (void)s.request_strike();
        tick(s);
        require(s.snapshot().strike_count == before, "strike: no strike starts once the rip has begun");
    }

    // A checkpoint load mid-strike: the fist back at guard, nothing lands.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
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
    // At the toolbox by the north band's rail (PICK UP on offer), her body
    // dead ahead in reach: the punch (it would land) and the pick-up are both
    // there, each on its own button.
    const auto at_the_box = [](Simulation &s) {
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "strike: settle");
        s.debug_launch_player(Vector3{-4.4, 154.95, -125.85}, Vector3{0.0, 0.0, 0.0});
        (void)s.set_facing(0.0, 1.0);
        for (int i = 0; i < 90; ++i) {
            tick(s);
        }
    };
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_box(s);
        const Vector3 p = s.snapshot().player_position;
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        tick(s);
        require(s.snapshot().carry_target_entity_id != 0 && s.snapshot().strike_offered && s.snapshot().strike_ready,
                "strike: at the toolbox with her body in reach ahead, the punch and the pick-up are both on offer");
    }
    // A punch under way owns his hands: thrown on air at the toolbox, Action
    // pressed for the pick-up and the climb every tick of its wind-up, reach
    // and recoil starts neither; at guard again, the pick-up takes it.
    {
        Simulation s(InitialSpawn::Deck154);
        at_the_box(s);
        require(s.snapshot().carry_target_entity_id != 0, "strike: the toolbox in reach (setup)");
        const std::uint64_t before = s.snapshot().strike_count;
        (void)s.request_strike();
        tick(s);
        require(s.snapshot().strike_count == before + 1 && s.snapshot().strike_phase != 0, "strike: thrown on air (setup)");
        bool other = false;
        for (int i = 0; i < 60 && s.snapshot().strike_phase != 0; ++i) {
            (void)s.request_pick_up();
            (void)s.request_traversal();
            tick(s);
            other = other || s.snapshot().carrying_entity_id != 0 ||
                    s.snapshot().traversal_state != scraperx::sim::TraversalState::None;
        }
        require(!other && s.snapshot().strike_phase == 0,
                "strike: while his punch is under way no pick-up or climb starts (his hands are the punch's)");
        for (int i = 0; i < 30 && s.snapshot().carrying_entity_id == 0; ++i) {
            (void)s.request_pick_up();
            tick(s);
        }
        require(s.snapshot().carrying_entity_id != 0, "strike: at guard again, the pick-up takes the toolbox");
    }
    // The world takes his hands from a punch under way: his left torn off and
    // his right broken in its wind-up, no fist left; it ends there and lands
    // nothing, her body in reach in front all the while.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        s.debug_lose_arm(0);
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        tick(s);
        const std::uint64_t hits = s.snapshot().strike_hit_count;
        (void)s.request_strike();
        for (int i = 0; i < 4; ++i) {
            tick(s);
        }
        require(s.snapshot().strike_phase == 1, "strike: in its wind-up (setup)");
        s.debug_break_arm(1);
        tick(s);
        const bool ended = s.snapshot().strike_phase == 0;
        for (int i = 0; i < 40; ++i) {
            tick(s);
        }
        require(ended && s.snapshot().strike_hit_count == hits,
                "strike: his last fist broken in its wind-up, the punch ends there and lands nothing");
    }
    // The fist is a fist, not a line: a post 1 cm thick 4.5 cm off its way
    // (inside its 6 cm) takes it, no landing; 10 cm off, it lands.
    Vector3 start{};
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        start = s.snapshot().player_position;
    }
    const auto post = [&start](const double off_x) {
        return std::vector<Simulation::TestExtraSolid>{
            Simulation::TestExtraSolid{start.x + off_x, start.y + 0.25, start.z + 0.40, 0.005F, 0.5F, 0.005F, true}};
    };
    const Thrown grazed = throw_one(1.05, post(0.045));
    const Thrown clear = throw_one(1.05, post(0.10));
    std::cout << "  strike by a post: 4.5 cm off offered=" << grazed.offered << " hits=" << grazed.hits
              << ", 10 cm off offered=" << clear.offered << " hits=" << clear.hits << "\n";
    require(grazed.strikes == 1 && grazed.hits == 0 && !grazed.offered,
            "strike: a thin post within the fist's breadth of its way takes the fist (none offered, none landed)");
    require(clear.offered && clear.hits == 1, "strike: the same post clear of the fist's breadth, it lands");
    // No fist to throw, no punch: his right torn off and his left broken,
    // none offered and a press throws none; the right off alone, the left
    // throws it and it lands.
    const Thrown none = throw_one(1.05, {}, 1, 0);
    const Thrown left = throw_one(1.05, {}, 1, -1);
    std::cout << "  strike arms: torn and broken arm=" << none.arm << " offered=" << none.offered
              << " strikes=" << none.strikes << ", right torn arm=" << left.arm << " hits=" << left.hits << "\n";
    require(none.arm == -1 && !none.offered && none.strikes == 0,
            "strike: one arm torn off and the other broken, no fist: no punch offered or thrown");
    require(left.arm == 0 && left.offered && left.hits == 1, "strike: the right torn off, the left throws it and it lands");
    // In the air no punch: the PUNCH button greyed and a press ignored, so a
    // lunge's catch and a wall run's kick are never eaten by one.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        s.debug_launch_player(Vector3{p.x, p.y + 0.05, p.z}, Vector3{0.0, 4.0, 0.0});
        for (int i = 0; i < 6; ++i) {
            tick(s);
        }
        const std::uint64_t before = s.snapshot().strike_count;
        const bool airborne = !s.snapshot().player_grounded;
        const bool ready = s.snapshot().strike_ready;
        (void)s.request_strike();
        tick(s);
        require(airborne && !ready && s.snapshot().strike_count == before,
                "strike: in the air the punch is greyed and a press throws none");
    }
    std::cout << "PASS scraperx_sim strike: impulse_ns=" << close.impulse_ns << "\n";
}

void run_matron_upper_body() {
    // J-044c: the jointed upper body — head, neck, chest, shoulders on
    // motorized joints, 220 kg total, one 8 kN / 44 kW budget. The reference
    // punch is J_ref = 1500 N x 3/90 s = 50 N-s (J044A_SPEC.md).
    // Her upper bodies exist once she is in the world: home puts her in the
    // locker, the launch puts her on the deck (as the strike tests do).
    auto place = [](Simulation &s) {
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
    };
    {
        // Rest pose: place her on the deck (spawns the upper bodies), hold
        // her Dormant (sealed, listening, feet still), and let the motors
        // settle. A hunting Matron's gait would lean the joints; the rest
        // bound is about the motors holding still, so she must stand still.
        Simulation s(InitialSpawn::Deck154);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.debug_place_matron(Vector3{p.x, p.y - 0.20, p.z + 3.0});
        s.debug_set_matron_state_for_test(0);  // Dormant
        const auto at_launch = s.matron_state();
        std::cout << "  upper body at launch: angles_deg head=" << at_launch.upper_angle_deg[0]
                  << " neck=" << at_launch.upper_angle_deg[1] << " chest=" << at_launch.upper_angle_deg[2]
                  << " arm_l=" << at_launch.upper_angle_deg[3] << " arm_r=" << at_launch.upper_angle_deg[4]
                  << " yaw=" << at_launch.yaw << "\n";
        // Settle to the motors' steady hold, then measure twice: the rest
        // pose is a steady hold within the spec joint limits, not exactly
        // 0 deg. The head's centre sits forward of the neck joint, so its
        // weight leans the neck ~3.4 deg at equilibrium (the 20 Hz spring
        // balancing ~4 N-m); the proper gains (Grok #244) will tighten it.
        // What the motors must prove is a steady hold inside the limits.
        for (int i = 0; i < 150; ++i) {
            tick(s);
        }
        const auto m1 = s.matron_state();
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const auto m = s.matron_state();
        double total = m.upper_root_mass_kg;
        double max_drift = 0.0;
        for (int i = 0; i < 5; ++i) {
            total += m.upper_mass_kg[i];
            max_drift = std::max(max_drift, std::abs(m.upper_angle_deg[i] - m1.upper_angle_deg[i]));
        }
        float neck_normal = -1.0F, neck_plane = -1.0F, neck_twist_min = -1.0F, neck_twist_max = -1.0F;
        s.upper_neck_limits_for_test(neck_normal, neck_plane, neck_twist_min, neck_twist_max);
        std::cout << "  upper body masses: root=" << m.upper_root_mass_kg << " head=" << m.upper_mass_kg[0]
                  << " neck=" << m.upper_mass_kg[1] << " chest=" << m.upper_mass_kg[2]
                  << " arm_l=" << m.upper_mass_kg[3] << " arm_r=" << m.upper_mass_kg[4] << " total=" << total
                  << " angles_deg head=" << m.upper_angle_deg[0] << " neck=" << m.upper_angle_deg[1]
                  << " chest=" << m.upper_angle_deg[2] << " arm_l=" << m.upper_angle_deg[3]
                  << " arm_r=" << m.upper_angle_deg[4] << " drift_deg=" << max_drift
                  << " constraints_in_system=" << s.upper_constraints_in_system_for_test()
                  << " neck_limits_rad normal=" << neck_normal << " plane=" << neck_plane
                  << " twist_min=" << neck_twist_min << " twist_max=" << neck_twist_max
                  << " motor_w=" << m.upper_motor_power_w << "\n";
        require(std::abs(total - 220.0) < 1.0e-3, "upper body: the masses add to 220 kg");
        require(std::abs(m.upper_mass_kg[0] - 12.188) < 1.0e-3 && std::abs(m.upper_mass_kg[1] - 3.080) < 1.0e-3 &&
                    std::abs(m.upper_mass_kg[2] - 35.112) < 1.0e-3 &&
                    std::abs(m.upper_mass_kg[3] - 10.868) < 1.0e-3 &&
                    std::abs(m.upper_mass_kg[4] - 10.868) < 1.0e-3,
                "upper body: head 12.188, neck 3.080, chest 35.112, arms 10.868 each");
        require(std::abs(m.upper_root_mass_kg - 147.884) < 1.0e-3,
                "upper body: the root carries the 147.884 kg rest");
        // Steady hold inside the spec joint limits (head 35/25, neck
        // 15/10/15, chest 14/18/22.5, arms 150/130/90 deg): the neck's
        // ~3.4 deg is the head-weight equilibrium, well inside its 10 deg
        // plane cone. Drift < 0.5 deg over 30 ticks proves it is a hold,
        // not a transient passing through.
        require(m.upper_angle_deg[0] < 10.0 && m.upper_angle_deg[1] < 10.0 && m.upper_angle_deg[2] < 10.0 &&
                    m.upper_angle_deg[3] < 10.0 && m.upper_angle_deg[4] < 10.0,
                "upper body: the motors hold the rest pose standing, inside the joint limits");
        require(max_drift < 0.5, "upper body: the rest pose is steady, not drifting");
        require(m.upper_motor_power_w < 50.0, "upper body: the motors draw ~nothing at rest");
    }
    {
        Simulation s(InitialSpawn::Deck154);
        place(s);
        // J_ref, 50 N-s, forward at 8 cm over the head's centre: it snaps
        // the head back on its neck, never the root.
        const auto m0 = s.matron_state();
        s.debug_upper_impulse(0, Vector3{0.0, 0.0, 50.0},
                              Vector3{m0.position.x, m0.position.y + 0.69, m0.position.z});
        double peak_head = 0.0, peak_power = 0.0;
        for (int i = 0; i < 10; ++i) {
            tick(s);
            peak_head = std::max(peak_head, s.matron_state().upper_angle_deg[0]);
            peak_power = std::max(peak_power, s.matron_state().upper_motor_power_w);
        }
        for (int i = 0; i < 90; ++i) {
            tick(s);
        }
        const auto m1 = s.matron_state();
        std::cout << "  upper body head impulse: peak_head_deg=" << peak_head << " peak_motor_w=" << peak_power
                  << " head_deg_after_1s=" << m1.upper_angle_deg[0] << "\n";
        require(peak_head > 5.0, "upper body: a 50 N-s impulse to the head snaps it off rest");
        require(m1.upper_angle_deg[0] < 2.0, "upper body: the head returns to rest after the impulse");
        require(peak_power > 0.0 && peak_power < 44000.0,
                "upper body: the motors worked, inside the 44 kW budget");
    }
    {
        // The same impulse twice gives the same head angle, to the bit.
        auto head_after = [&]() {
            Simulation s(InitialSpawn::Deck154);
            place(s);
            const auto m0 = s.matron_state();
            s.debug_upper_impulse(0, Vector3{0.0, 0.0, 50.0},
                                  Vector3{m0.position.x, m0.position.y + 0.69, m0.position.z});
            for (int i = 0; i < 100; ++i) {
                tick(s);
            }
            return s.matron_state().upper_angle_deg[0];
        };
        const double a = head_after();
        const double b = head_after();
        std::cout << "  upper body determinism: head_deg_a=" << a << " head_deg_b=" << b << "\n";
        require(a == b, "upper body: the same impulse gives the same head angle, to the bit");
    }
    {
        Simulation s(InitialSpawn::Deck154);
        place(s);
        const auto m = s.matron_state();
        // Her frame: forward from her yaw, right across it; heights above her feet.
        const double yaw = m.yaw;
        const double fx = std::sin(yaw), fz = std::cos(yaw);
        const double rx = std::cos(yaw), rz = -std::sin(yaw);
        const double feet = m.position.y - 1.05;
        const auto pt = [&](const double h, const double across, const double forward) {
            return Vector3{m.position.x + across * rx + forward * fx, feet + h,
                           m.position.z + across * rz + forward * fz};
        };
        const int head = s.strike_segment_for_test(pt(1.66, 0.0, 0.075));
        const int neck = s.strike_segment_for_test(pt(1.56, 0.0, 0.05));
        const int chest = s.strike_segment_for_test(pt(1.40, 0.0, 0.02));
        const int arm_l = s.strike_segment_for_test(pt(1.20, -0.30, 0.15));
        const int arm_r = s.strike_segment_for_test(pt(1.20, 0.30, 0.15));
        const int leg = s.strike_segment_for_test(pt(0.50, 0.0, 0.0));
        std::cout << "  upper body routing: head=" << head << " neck=" << neck << " chest=" << chest
                  << " arm_l=" << arm_l << " arm_r=" << arm_r << " leg=" << leg << "\n";
        require(head == 0 && neck == 1 && chest == 2 && arm_l == 3 && arm_r == 4 && leg == -1,
                "upper body: a strike lands on the segment it falls on, the root below the chest");
    }
    std::cout << "PASS scraperx_sim matron-upper-body\n";
}

namespace {
// The low sweep (sim/physics_world_strike.cpp): Punch while crouched. Her
// `her_ahead_m` in front of him on the Deck154 start, crouched, one sweep:
// what it did. `stage`: her stage held for the blow. `fast`: wait until she
// hunts toward him faster than 1 m/s before throwing (her crouch).
// `immediate`: throw at once, no hunt wait (the whiff: she must not close
// the distance first).
struct Swept {
    bool offered = false;
    std::uint64_t strikes = 0;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double max_blow_mps = 0.0;
    int state = -1;
    std::uint64_t knockdowns = 0;
    std::uint64_t staggers = 0;
    std::uint8_t phase = 0;
    std::uint8_t move = 0;
    Vector3 point{};
};

Swept sweep_one(const double her_ahead_m, const int stage, const bool fast, const bool immediate,
                const double lift_m = 0.0) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) {
        tick(s);
    }
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    // Her soles sit `lift_m` above his launch height: the level launch
    // (0.20 m under his standing centre) sits her capsule 0.35 m into
    // the deck, which the physics resolves back to the deck, so the
    // soles' difference is the lift less 0.35 (lift 0 is the level cases
    // below; the raised cases below state their true differences).
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20 + lift_m, p.z + her_ahead_m});
    s.debug_hold_matron_hands(30.0);
    constexpr int kHunting = static_cast<int>(scraperx::sim::Matron::State::Hunting);
    if (!immediate) {
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && s.matron_state().state != kHunting;
             ++i) {
            tick(s);
        }
        require(s.matron_state().state == kHunting, "sweep: she hunts before the sweep (setup)");
        s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
    }
    (void)s.set_crouch_input(true);
    for (int i = 0; i < 10 && !s.snapshot().player_crouched; ++i) {
        tick(s);
    }
    require(s.snapshot().player_crouched, "sweep: crouched before the sweep (setup)");
    if (fast) {
        // Pressed at 2.05 m, she crosses the gap band during the sweep's reach.
        bool entered = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !entered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            tick(s);
            const double dx = s.matron_state().position.x - s.snapshot().player_position.x;
            const double dz = s.matron_state().position.z - s.snapshot().player_position.z;
            entered = s.matron_state().state == kHunting && s.matron_state().speed > 1.0 &&
                      std::hypot(dx, dz) <= 2.05;
        }
        require(entered, "sweep: she hunts fast toward sweep reach (setup)");
    } else {
        const Vector3 me = s.snapshot().player_position;
        const Vector3 her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
        tick(s);
    }
    Swept out;
    out.offered = s.snapshot().strike_offered;
    const std::uint64_t strikes0 = s.snapshot().strike_count;
    const std::uint64_t hits0 = s.snapshot().strike_hit_count;
    const std::uint64_t downs0 = s.matron_state().knockdowns;
    const std::uint64_t stags0 = s.matron_state().staggers;
    (void)s.request_strike();
    for (int i = 0; i < 60; ++i) {
        tick(s);
        out.max_blow_mps = std::max(out.max_blow_mps, s.matron_state().blow_mps);
    }
    const auto &snap = s.snapshot();
    out.strikes = snap.strike_count - strikes0;
    out.hits = snap.strike_hit_count - hits0;
    out.impulse_ns = snap.strike_impulse_ns;
    out.state = s.matron_state().state;
    out.knockdowns = s.matron_state().knockdowns - downs0;
    out.staggers = s.matron_state().staggers - stags0;
    out.phase = snap.strike_phase;
    out.move = snap.strike_move;
    out.point = snap.strike_hit_point;
    return out;
}
} // namespace

void run_matron_sweep() {
    constexpr int kDown = static_cast<int>(scraperx::sim::Matron::State::Down);
    constexpr int kStaggering = static_cast<int>(scraperx::sim::Matron::State::Staggering);
    const Swept pest = sweep_one(1.05, 0, false, false);
    std::cout << "  sweep pest: offered=" << pest.offered << " hits=" << pest.hits << " impulse_ns=" << pest.impulse_ns
              << " max_blow=" << pest.max_blow_mps << " state=" << pest.state << "\n";
    require(pest.strikes == 1, "sweep: one press throws one sweep");
    require(pest.offered && pest.hits == 1 && pest.move == 1, "sweep: a sweep in reach lands once, as move 1");
    require(pest.impulse_ns > 59.0 && pest.impulse_ns <= 60.01, "sweep: 1350 N over 4 ticks is 60 N-s");
    require(pest.max_blow_mps > 0.26 && pest.max_blow_mps < 0.29, "sweep: 60 N-s over her 220 kg is 0.273 m/s");
    require(pest.state == kDown && pest.knockdowns == 1 && pest.staggers == 0, "sweep: an upright pest goes down");
    const Swept changed = sweep_one(1.05, 2, false, false);
    std::cout << "  sweep changed faced: hits=" << changed.hits << " state=" << changed.state << "\n";
    require(changed.hits == 1 && changed.state == kDown && changed.knockdowns == 1,
            "sweep: a faced changed Matron goes down, her brace bypassed low");
    const Swept crouched = sweep_one(2.5, 1, true, false);
    std::cout << "  sweep crouched: hits=" << crouched.hits << " state=" << crouched.state << "\n";
    require(crouched.hits == 1 && crouched.impulse_ns > 59.0,
            "sweep: the crouched blow lands (no whiff hiding behind no effect)");
    require(crouched.state != kDown && crouched.state != kStaggering && crouched.knockdowns == 0 &&
                crouched.staggers == 0,
            "sweep: a crouched Matron takes nothing from it");
    const Swept whiff = sweep_one(3.0, 0, false, true);
    std::cout << "  sweep whiff: offered=" << whiff.offered << " hits=" << whiff.hits << " phase=" << int(whiff.phase)
              << "\n";
    require(!whiff.offered && whiff.strikes == 1 && whiff.hits == 0 && whiff.impulse_ns == 0.0 &&
                whiff.move == 1,
            "sweep: out of reach, none offered and none lands, but the press still threw a sweep");
    require(whiff.phase == 0 && whiff.knockdowns == 0 && whiff.staggers == 0,
            "sweep: after a whiff the fist is back at guard, full recovery");
    // The sweep is a leg from his foot: an ankle out of its vertical reach
    // is no landing, whatever the X/Z gap says. Crouched beside a platform
    // with her soles 1.15 m above his, and under her while she climbs
    // with her soles 1.65 m above his (the launch heights 1.50 / 2.00
    // less the 0.35 the level launch sits into the deck), the sweep is
    // not offered and lands nothing; both are thrown at once, so she is
    // still high through the whole reach window.
    const Swept platform = sweep_one(1.05, 0, false, true, 1.50);
    std::cout << "  sweep platform: offered=" << platform.offered << " hits=" << platform.hits
              << " state=" << platform.state << "\n";
    require(!platform.offered && platform.hits == 0 && platform.knockdowns == 0,
            "sweep: a crouched sweep at a platform ankle 1.15 m up is not offered and lands nothing");
    const Swept climbing = sweep_one(0.50, 0, false, true, 2.00);
    std::cout << "  sweep climbing: offered=" << climbing.offered << " hits=" << climbing.hits
              << " state=" << climbing.state << "\n";
    require(!climbing.offered && climbing.hits == 0 && climbing.knockdowns == 0,
            "sweep: a sweep under a climbing Matron 1.65 m up is not offered and lands nothing");
    const Swept again = sweep_one(1.05, 0, false, false);
    require(again.hits == pest.hits && again.impulse_ns == pest.impulse_ns && again.point.x == pest.point.x &&
                again.point.y == pest.point.y && again.point.z == pest.point.z,
            "sweep: the same inputs give the same sweep, to the bit");
    std::cout << "PASS scraperx_sim matron sweep: impulse_ns=" << pest.impulse_ns << "\n";
}

namespace {
// The heavy kick: Punch while moving toward her, one kick on the
// Deck154 start. `stage`: her stage held. `fast`: wait until she hunts
// faster than 1 m/s (her crouch). `immediate`: throw at once (whiff).
// `move_toward`: hold move toward her (false: a punch). `crouch`:
// hold Crouch too (a sweep). `slow`: creep pace (upright Dangerous).
struct Kicked {
    bool offered = false;
    std::uint64_t strikes = 0;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double max_blow_mps = 0.0;
    int state = -1;
    std::uint64_t knockdowns = 0;
    std::uint64_t staggers = 0;
    std::uint8_t phase = 0;
    std::uint8_t move = 0;
    bool staggered = false;
    Vector3 point{};
};

Kicked kick_one(const double her_ahead_m, const int stage, const bool fast, const bool immediate,
                const bool move_toward = true, const bool crouch = false, const bool slow = false) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) {
        tick(s);
    }
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + her_ahead_m});
    s.debug_hold_matron_hands(30.0);
    constexpr int kHunting = static_cast<int>(scraperx::sim::Matron::State::Hunting);
    if (slow) s.debug_fix_matron_pace(0);  // creep: upright Dangerous.
    if (!immediate) {
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && s.matron_state().state != kHunting;
             ++i)
            tick(s);
        require(s.matron_state().state == kHunting, "kick: she hunts before the kick (setup)");
        s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
    }
    if (crouch) {
        (void)s.set_crouch_input(true);
        for (int i = 0; i < 10 && !s.snapshot().player_crouched; ++i) tick(s);
        require(s.snapshot().player_crouched, "kick: crouched before the press (setup)");
    }
    if (fast) {
        // Pressed at 2.05 m, she crosses the kick's reach band during
        // windup and reach, still running fast at contact.
        bool entered = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !entered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            tick(s);
            const double dx = s.matron_state().position.x - s.snapshot().player_position.x;
            const double dz = s.matron_state().position.z - s.snapshot().player_position.z;
            entered = s.matron_state().state == kHunting && s.matron_state().speed > 1.0 &&
                      std::hypot(dx, dz) <= 2.05;
        }
        require(entered, "kick: she hunts fast toward kick reach (setup)");
    } else {
        const Vector3 me = s.snapshot().player_position;
        const Vector3 her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
        tick(s);
    }
    if (move_toward) {
        const Vector3 me = s.snapshot().player_position;
        const Vector3 her = s.matron_state().position;
        const double dx = her.x - me.x, dz = her.z - me.z, l = std::hypot(dx, dz);
        (void)s.set_facing(dx / l, dz / l);
        (void)s.set_move_input(dx / l, dz / l);
        tick(s);
    }
    Kicked out;
    out.offered = s.snapshot().strike_offered;
    const std::uint64_t strikes0 = s.snapshot().strike_count, hits0 = s.snapshot().strike_hit_count,
                      downs0 = s.matron_state().knockdowns, stags0 = s.matron_state().staggers;
    (void)s.request_strike();
    for (int i = 0; i < 60; ++i) {
        tick(s);
        // The strike started, so its move is frozen: release move now.
        if (i == 0) (void)s.set_move_input(0.0, 0.0);
        out.max_blow_mps = std::max(out.max_blow_mps, s.matron_state().blow_mps);
        // A stagger is short here (0.45 s): assert she staggered at all,
        // never that she is still staggering at one fixed tick.
        if (s.matron_state().state == static_cast<int>(scraperx::sim::Matron::State::Staggering)) out.staggered = true;
        // The move is frozen while the strike is under way, but Guard
        // recomputes it from the CURRENT input each tick: after the kick
        // he has walked past her, so reading it after the strike ends
        // would report the punch. Sample it while a strike is running.
        if (s.snapshot().strike_phase != 0) {
            out.move = s.snapshot().strike_move;
        }
    }
    const auto &snap = s.snapshot();
    out.strikes = snap.strike_count - strikes0;
    out.hits = snap.strike_hit_count - hits0;
    out.impulse_ns = snap.strike_impulse_ns;
    out.state = s.matron_state().state;
    out.knockdowns = s.matron_state().knockdowns - downs0;
    out.staggers = s.matron_state().staggers - stags0;
    out.phase = snap.strike_phase;
    out.point = snap.strike_hit_point;
    return out;
}
} // namespace

void run_matron_kick() {
    constexpr int kDown = static_cast<int>(scraperx::sim::Matron::State::Down);
    constexpr int kStaggering = static_cast<int>(scraperx::sim::Matron::State::Staggering);
    const Kicked pest = kick_one(1.05, 0, false, false);
    std::cout << "  kick pest: offered=" << pest.offered << " hits=" << pest.hits << " impulse_ns=" << pest.impulse_ns << " max_blow=" << pest.max_blow_mps << " state=" << pest.state << "\n";
    require(pest.strikes == 1, "kick: one press throws one kick");
    require(pest.offered && pest.hits == 1 && pest.move == 2, "kick: a kick in reach lands once, as move 2");
    require(pest.impulse_ns > 166.6 && pest.impulse_ns <= 166.7, "kick: 3000 N over 5 ticks is 166.7 N-s");
    // Down on the 3rd contact tick (100 N-s over Pest's 0.306 bar);
    // take_blow ignores blows during Down, so the peak stops at 0.4545.
    require(pest.max_blow_mps > 0.45 && pest.max_blow_mps < 0.46, "kick: judged peak stops at Down, 100 N-s over 220 kg");
    require(pest.state == kDown && pest.knockdowns == 1 && pest.staggers == 0, "kick: an upright pest goes down");
    const Kicked dangerous = kick_one(1.05, 1, false, false, true, false, true);
    std::cout << "  kick dangerous: hits=" << dangerous.hits << " state=" << dangerous.state << "\n";
    require(dangerous.hits == 1 && dangerous.staggered && dangerous.staggers == 1 && dangerous.knockdowns == 0,
            "kick: an upright dangerous Matron staggers only, never down");
    const Kicked changed = kick_one(1.05, 2, false, false);
    std::cout << "  kick changed faced: hits=" << changed.hits << " state=" << changed.state << "\n";
    require(changed.hits == 1 && changed.staggered && changed.staggers == 1 && changed.knockdowns == 0,
            "kick: a faced changed Matron staggers only, her brace holds");
    const Kicked crouched = kick_one(2.5, 1, true, false);
    std::cout << "  kick crouched: hits=" << crouched.hits << " state=" << crouched.state << "\n";
    require(crouched.hits == 1 && crouched.impulse_ns > 166.6 && crouched.state != kDown &&
                crouched.state != kStaggering && crouched.knockdowns == 0 && crouched.staggers == 0,
            "kick: the crouched blow lands but takes nothing from it");
    const Kicked whiff = kick_one(3.0, 0, false, true);
    std::cout << "  kick whiff: offered=" << whiff.offered << " hits=" << whiff.hits << " phase=" << int(whiff.phase) << "\n";
    require(!whiff.offered && whiff.strikes == 1 && whiff.hits == 0 && whiff.impulse_ns == 0.0 && whiff.move == 2,
            "kick: out of reach, none offered and none lands, but the press still threw a kick");
    require(whiff.phase == 0 && whiff.knockdowns == 0 && whiff.staggers == 0,
            "kick: after a whiff the foot is back at guard, full recovery");
    const Kicked plain = kick_one(1.05, 0, false, false, false);
    std::cout << "  kick no-move: move=" << int(plain.move) << " hits=" << plain.hits << " impulse_ns=" << plain.impulse_ns << "\n";
    require(plain.move == 0 && plain.hits == 1 && plain.impulse_ns > 49.0 && plain.impulse_ns <= 50.01,
            "kick: standing still, the same press is the plain punch, not a kick");
    const Kicked low = kick_one(1.05, 0, false, false, true, true);
    std::cout << "  kick crouch-move: move=" << int(low.move) << " hits=" << low.hits << "\n";
    require(low.move == 1 && low.hits == 1,
            "kick: crouched and moving, the same press is the low sweep, not a kick");
    const Kicked again = kick_one(1.05, 0, false, false);
    require(again.hits == pest.hits && again.impulse_ns == pest.impulse_ns && again.point.x == pest.point.x && again.point.y == pest.point.y && again.point.z == pest.point.z,
            "kick: the same inputs give the same kick, to the bit");
    std::cout << "PASS scraperx_sim matron kick: impulse_ns=" << pest.impulse_ns << "\n";
}

void run_matron_fist_contact() {
    // J-044d: the fist lands on the body it actually meets, in the region
    // it falls on, with the glancing share of the dose. The reference punch
    // is J_ref = 1500 N x 3/90 s = 50 N-s (J044A_SPEC.md). A level punch
    // reaches her chest only (his fist 1.50 m, her head centre 1.66 m), so
    // the cheek and the graze are proved by the same cast at head height.
    // Her centre is 1.05 m over her feet; her head centre 0.61 and her
    // chest centre 0.33 over that. A chest-edge graze is shielded by her
    // arm, so the graze is on her head, where nothing stands in the way.
    using Region = scraperx::sim::Matron::HitRegion;
    // A fist cast at her: from 1.5 m out along her facing, through the point
    // `lateral_m` across her right at `height_m` over her centre, to 0.5 m
    // past. Her live yaw, so the path is exact for this tick.
    auto probe = [](Simulation &s, const double lateral_m, const double height_m) {
        const auto m = s.matron_state();
        const double fx = std::sin(m.yaw), fz = std::cos(m.yaw);
        const double rx = std::cos(m.yaw), rz = -std::sin(m.yaw);
        const double cx = m.position.x + rx * lateral_m;
        const double cz = m.position.z + rz * lateral_m;
        const double cy = m.position.y + height_m;
        return s.fist_contact_for_test(Vector3{cx + fx * 1.5, cy, cz + fz * 1.5},
                                       Vector3{cx - fx * 0.5, cy, cz - fz * 0.5});
    };
    // Her on the deck, standing still (Dormant), motors settled.
    auto place_still = [](Simulation &s) {
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.debug_place_matron(Vector3{p.x, p.y - 0.20, p.z + 3.0});
        s.debug_set_matron_state_for_test(0);  // Dormant
        for (int i = 0; i < 150; ++i) {
            tick(s);
        }
    };
    {
        // The cheek, left and right: the same cast at head height, mirrored.
        // A punch on her left cheek turns her head one way, on her right
        // cheek the other way, just as far. The impulse is J_ref, at the
        // cast's contact point, on the head (segment 0).
        double turn_l = 0.0, turn_r = 0.0;
        for (int side = 0; side < 2; ++side) {
            Simulation s(InitialSpawn::Deck154);
            place_still(s);
            const double lateral = side == 0 ? -0.10 : 0.10;
            const auto hit = probe(s, lateral, 0.61);
            std::cout << "  fist contact cheek side=" << side << " hit=" << hit.hit
                      << " region=" << hit.region << " solid=" << hit.solid_share << "\n";
            require(hit.hit, "fist contact: the cheek cast meets her head");
            require(hit.region == static_cast<int>(side == 0 ? Region::FaceL : Region::FaceR),
                    "fist contact: the cast lands on the cheek it is aimed at");
            const auto m = s.matron_state();
            s.debug_upper_impulse(0, Vector3{-std::sin(m.yaw) * 50.0, 0.0, -std::cos(m.yaw) * 50.0}, hit.point);
            tick(s);
            const double signed_turn = s.matron_state().upper_head_angvel.y;
            double peak = std::abs(signed_turn);
            for (int i = 0; i < 4; ++i) {
                tick(s);
                peak = std::max(peak, std::abs(s.matron_state().upper_head_angvel.y));
            }
            std::cout << "  fist contact cheek side=" << side << " turn=" << signed_turn << " peak=" << peak
                      << "\n";
            require(peak > 0.1, "fist contact: the cheek hit turns her head");
            if (side == 0) {
                turn_l = signed_turn;
            } else {
                turn_r = signed_turn;
            }
        }
        require((turn_l > 0.0) != (turn_r > 0.0),
                "fist contact: the left and right cheeks turn her head opposite ways");
        require(std::abs(std::abs(turn_l) - std::abs(turn_r)) <
                    0.2 * std::max(std::abs(turn_l), std::abs(turn_r)),
                "fist contact: the mirrored cheeks turn her head equally far");
    }
    {
        // Glancing vs solid, on her head: dead-on the share is 1, a graze
        // at the edge is near 0. The dead-on hit is her face; the graze
        // catches the side of her skull.
        Simulation s(InitialSpawn::Deck154);
        place_still(s);
        const auto solid = probe(s, 0.0, 0.61);
        const auto graze = probe(s, 0.18, 0.61);
        std::cout << "  fist contact glancing: solid=" << solid.solid_share << " region="
                  << static_cast<int>(solid.region) << " graze=" << graze.solid_share << " region="
                  << static_cast<int>(graze.region) << "\n";
        // Dead-on there is no side, so which cheek is floating-point noise;
        // the contract is that the hit is on her face, left or right.
        require(solid.hit &&
                    (solid.region == static_cast<int>(Region::FaceL) ||
                     solid.region == static_cast<int>(Region::FaceR)),
                "fist contact: the dead-on head cast is her face");
        require(solid.solid_share > 0.9, "fist contact: the dead-on head cast is solid");
        require(graze.hit && graze.region == static_cast<int>(Region::Skull),
                "fist contact: the head graze catches her skull");
        require(graze.solid_share < 0.5 * solid.solid_share,
                "fist contact: the graze delivers less than half the solid share");
    }
    {
        // The arm intercepts: a punch line across her right arm meets the
        // arm first, not her chest; the mirror meets her left arm.
        Simulation s(InitialSpawn::Deck154);
        place_still(s);
        const auto right = probe(s, 0.20, 0.33);
        const auto left = probe(s, -0.20, 0.33);
        std::cout << "  fist contact arm: right region=" << static_cast<int>(right.region)
                  << " solid=" << right.solid_share << " left region=" << static_cast<int>(left.region)
                  << " solid=" << left.solid_share << "\n";
        require(right.hit && right.region == static_cast<int>(Region::ArmR),
                "fist contact: the right line meets her right arm");
        require(left.hit && left.region == static_cast<int>(Region::ArmL),
                "fist contact: the left line meets her left arm");
        require(right.solid_share > 0.5 && left.solid_share > 0.5,
                "fist contact: the arm hits are solid, not grazes");
    }
    {
        // A miss: a cast a metre to her side, and one over her head, meet
        // nothing; a punch pressed with her out of reach is not offered and
        // credits nothing.
        Simulation s(InitialSpawn::Deck154);
        place_still(s);
        require(!probe(s, 1.0, 0.33).hit, "fist contact: a cast a metre to her side meets nothing");
        require(!probe(s, 0.0, 2.5).hit, "fist contact: a cast over her head meets nothing");
        Simulation t(InitialSpawn::Deck154);
        t.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(t);
        }
        const Vector3 p = t.snapshot().player_position;
        (void)t.set_facing(0.0, 1.0);
        t.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 3.0});
        tick(t);
        const double damage_before = t.matron_state().damage_ns;
        require(!t.snapshot().strike_offered, "fist contact: the punch is not offered out of reach");
        require(t.matron_state().damage_ns == damage_before, "fist contact: the unoffered punch credits nothing");
        require(t.hit_contact_count_for_test() == 0, "fist contact: the unoffered punch records nothing");
    }
    {
        // A level punch, dead-on, end to end: the fist meets her chest, the
        // dose is J_ref, and the region credit plus the unclassified bucket
        // add up to the damage. The contact is recorded, with its region
        // and share.
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        // Her on the deck 0.80 m in front, Dormant. The punch's reach is
        // 0.75 m from his chest and her chest's front is 0.10 m behind her
        // centre, so at 1.05 m the fist can only meet her root (belly); at
        // 0.80 m it meets her chest's front face dead-on.
        s.debug_place_matron(Vector3{p.x, p.y - 1.25, p.z + 0.80});
        // Dormant: she stands still, so the punch meets her chest dead-on
        // instead of catching her mid-stride (take_blow still credits a
        // blow while Dormant; only Down and Emerging discard one).
        s.debug_set_matron_state_for_test(0);  // Dormant
        // debug_place_matron leaves her facing +z, away from him; turn her
        // to face him (yaw pi, toward -z), so the punch meets her chest,
        // not her back.
        s.debug_face_matron(3.141592653589793);
        s.debug_hold_matron_hands(30.0);
        const double damage_before = s.matron_state().damage_ns;
        const double chest_before = s.region_impulse_for_test(static_cast<int>(Region::Chest));
        const double unclass_before = s.unclassified_impulse_for_test();
        const std::uint64_t contacts_before = s.hit_contact_count_for_test();
        tick(s);
        require(s.snapshot().strike_offered, "fist contact: the level punch is offered");
        (void)s.request_strike();
        // Windup 11 + reach 6 + the 3 contact ticks: 25 ticks clears the
        // landing, while the strike is still in recoil and its impulse
        // stands in the snapshot.
        for (int i = 0; i < 25; ++i) {
            tick(s);
        }
        require(s.snapshot().strike_hit_count == 1, "fist contact: the level punch lands");
        const double impulse = s.snapshot().strike_impulse_ns;
        const double damage = s.matron_state().damage_ns - damage_before;
        const double chest = s.region_impulse_for_test(static_cast<int>(Region::Chest)) - chest_before;
        const double unclass = s.unclassified_impulse_for_test() - unclass_before;
        const int contact_region = s.last_hit_region_for_test();
        const float contact_solid = s.last_hit_solid_for_test();
        std::cout << "  fist contact punch: impulse_ns=" << impulse << " damage_ns=" << damage
                  << " chest_ns=" << chest << " unclassified_ns=" << unclass << " region=" << contact_region
                  << " solid=" << contact_solid
                  << " contacts=" << (s.hit_contact_count_for_test() - contacts_before) << "\n";
        require(contact_region == static_cast<int>(Region::Chest),
                "fist contact: the level punch lands on her chest");
        // The dose is J_ref scaled by the glancing share: her chest sits
        // high, so the fist catches its lower edge (measured solid ~0.59,
        // a real hit, not a graze); the scaling, not a fixed 50, is the
        // mechanism under test.
        require(contact_solid > 0.5, "fist contact: the level punch is a real hit, not a graze");
        require(std::abs(impulse - 50.0 * static_cast<double>(contact_solid)) < 5.0,
                "fist contact: the level punch delivers J_ref times its share");
        require(std::abs(chest + unclass - damage) < 1.0e-3,
                "fist contact: the region credit plus the unclassified bucket is the damage");
        require(std::abs(unclass) < 1.0e-3, "fist contact: a clean punch leaves nothing unclassified");
        // One contact per tick the force is held (kStrikeContactTicks = 3),
        // each a third of J_ref; the region credit is the sum.
        require(s.hit_contact_count_for_test() - contacts_before == 3,
                "fist contact: one contact is recorded per tick the punch is held");
    }
    std::cout << "PASS scraperx_sim matron fist contact\n";
}
