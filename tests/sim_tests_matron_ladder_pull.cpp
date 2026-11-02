#include "sim_tests.hpp"
#include "sim/matron.hpp"
#include "sim/physics_world_tuning.hpp"

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;

void rung(Simulation &s, const double rise_seconds) {
    s.place_matron_home(154.0F);
    s.debug_hold_matron_grab(true);
    require(s.advance_frame(1.0).accepted, "ladder pull: settle");
    require(walk_to(s, 3.5, -128.2, 16.0) && walk_to(s, 3.5, -129.8, 4.0) &&
                walk_to(s, 5.0, -129.6, 4.0, 0.08), "ladder pull: reach the real ladder");
    require(s.set_facing(0.0, 1.0) && s.request_traversal() && s.advance_frame(0.2).accepted &&
                is_climbing(s.snapshot()), "ladder pull: hands take scanned rungs");
    if (rise_seconds > 0.0) {
        require(s.set_move_input(0.0, 1.0) && s.advance_frame(rise_seconds).accepted,
                "ladder pull: climb to another rung");
    }
    const auto before = s.snapshot();
    require(s.set_move_input(0.0, 0.0) && s.advance_frame(0.4).accepted,
            "ladder pull: settle the rung");
    const auto after = s.snapshot();
    std::cout << "ladder setup: rise=" << rise_seconds << " y=" << before.player_position.y
              << "->" << after.player_position.y << " force=" << before.climb_grip_force_n
              << "->" << after.climb_grip_force_n << " slips=" << before.climb_grip_slips
              << "->" << after.climb_grip_slips << " v=" << before.player_linear_velocity.y
              << "->" << after.player_linear_velocity.y << '\n';
    require(is_climbing(s.snapshot()),
            "ladder pull: finite grip holds without her pull");
}

double step(const scraperx::sim::Vector3 a, const scraperx::sim::Vector3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
}

void run_matron_ladder_pull() {
    for (int scenario = 0; scenario < 4; ++scenario) {
        const double rise = scenario == 0 ? 0.0 : scenario == 2 ? 6.0 : 0.6;
        Simulation s(InitialSpawn::Deck154);
        if (scenario == 3) s.debug_lose_arm(0);
        rung(s, rise);
        const auto start = s.snapshot();
        s.debug_place_matron({start.player_position.x + 0.72, 154.0, start.player_position.z});
        s.debug_fix_matron_pace(3);
        s.debug_set_matron_stage(2);
        s.debug_hold_matron_hands(true);
        // Mere proximity changes none of his grip rules.
        require(s.advance_frame(0.5).accepted && is_climbing(s.snapshot()) &&
                    s.snapshot().climb_grip_slips == start.climb_grip_slips,
                "ladder pull: her nearby body alone cannot open his hands");
        s.debug_hold_matron_grab(false);
        s.debug_hold_matron_hands(false);
        bool both_grips = false, taken_off = false;
        double peak_n = 0.0, peak_w = 0.0, max_step = 0.0;
        auto previous = s.snapshot();
        auto previous_her = s.matron_state().position;
        const auto grabs = s.matron_state().grabs;
        for (int i = 0; i < 5 * 90; ++i) {
            require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "ladder pull: real physics tick");
            const auto p = s.snapshot();
            const auto m = s.matron_state();
            both_grips = both_grips || (is_climbing(p) && m.grip_on);
            peak_n = std::max(peak_n, p.ladder_pull_force_n);
            peak_w = std::max(peak_w, p.ladder_pull_power_w);
            max_step = std::max(max_step, step(previous.player_position, p.player_position));
            require(p.ladder_pull_force_n <= 8000.1 && p.ladder_pull_power_w <= 22000.1,
                    "ladder pull: every tick inside her one force and power budget");
            require(p.player_gravity_factor == 1.0, "ladder pull: gravity stays on");
            require(step(previous.player_position, p.player_position) <= 0.2,
                    "ladder pull: no teleport or step over 0.2 m");
            require(step(previous_her, m.position) <= 0.2,
                    "ladder pull: her every step stays within 0.2 m");
            require(p.death_count == start.death_count, "ladder pull: first catch is not death");
            if (p.climb_grip_slips > start.climb_grip_slips && m.grip_on && !is_climbing(p)) {
                taken_off = true;
                break;
            }
            previous = p;
            previous_her = m.position;
        }
        std::cout << "ladder pull: rise_s=" << rise << " both=" << both_grips << " off=" << taken_off
                  << " force=" << peak_n << " power=" << peak_w << " step=" << max_step << '\n';
        if (rise < 1.0) {
            require(both_grips && taken_off && peak_n > 0.0,
                    "ladder pull: she must overcome his real grip before taking him off");
            require(s.snapshot().climb_grip_slips == start.climb_grip_slips + 1,
                    "ladder pull: exactly one physical grip failure precedes the hold");
            require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "ladder pull: advance into the existing hold");
            if (scenario == 3) {
                require(s.matron_state().fatal_path != 0,
                        "ladder pull: one arm left hands off to the existing fatality");
            }
        } else {
            require(!taken_off && s.matron_state().grabs == grabs && is_climbing(s.snapshot()),
                    "ladder pull: high out-of-reach rung stays safe");
        }
    }
    std::cout << "PASS scraperx_sim ladder pull: reachable rungs release only under load; high rung safe\n";
}
