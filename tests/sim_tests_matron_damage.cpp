// Fighting S7: exact damage boundaries and the existing physical blow path.
#include "sim_tests.hpp"
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
#include <array>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "damage: tick accepted"); }

struct Hit {
    double total = 0.0;
    double player = 0.0;
    double strike = 0.0;
    bool down = false;
    double down_damage = 0.0;
};

Hit hit(const bool punch, const bool stock, const bool test_down) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test({p.x, p.y - 0.20, p.z + 1.05});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                    s.matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) tick(s);
    require(s.matron_state().state == static_cast<int>(Matron::State::Hunting),
            "damage: Matron must finish Emerging before a judged blow");
    s.debug_set_matron_stage(test_down ? 0 : 2);
    for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
        const Vector3 me = s.snapshot().player_position, her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
        tick(s);
    }
    const auto earlier = s.save_game();
    if (punch) require(s.request_strike(), "damage: physical punch starts");
    const std::uint32_t first = s.loose_stock_throwables_begin();
    const std::uint32_t piece = s.kit_body_index(LooseStock::kFirstEntity + first);
    for (int i = 0; i < 65; ++i) {
        if (stock && i == (test_down ? 0 : 11)) {
            const Vector3 her = s.matron_state().position;
            const double feet = her.y - static_cast<double>(Matron::kStandCentre);
            s.debug_launch_kit_body(piece, {her.x, feet + 1.3, her.z + (test_down ? 2.0 : 1.15)},
                                    {0.0, 0.0, test_down ? -5.8 : -8.0});
        }
        tick(s);
    }
    Hit out;
    const auto m = s.matron_state();
    out.total = m.damage_ns;
    out.player = m.player_damage_ns;
    out.strike = s.snapshot().strike_impulse_ns;
    out.down = m.state == static_cast<int>(Matron::State::Down);
    if (test_down) {
        require(out.down, "damage: environmental blow puts her Down");
        out.down_damage = out.total;
        const Vector3 her = m.position;
        s.debug_launch_kit_body(piece, {her.x, her.y + 0.9, her.z + 0.5}, {0.0, 0.0, -7.0});
        for (int i = 0; i < 15; ++i) tick(s);
        require(std::abs(s.matron_state().damage_ns - out.down_damage) < 0.001,
                "damage: impacts while Down accrue no damage");
    }
    if (punch && !test_down) {
        std::cout << "damage attribution trace: total=" << out.total
                  << " player=" << out.player << " physical_strike=" << out.strike
                  << " matron_state=" << static_cast<int>(m.state)
                  << " strike_hits=" << s.snapshot().strike_hit_count << "\n";
        require(s.snapshot().strike_hit_count == 1 && out.strike > 0.0,
                "damage: one actual punch lands");
        require(std::abs(out.player - out.strike) < 0.02,
                "damage: player attribution is only the actual judged fist impulse");
    }
    if (punch && !stock) {
        // Move her away without resetting the judged live damage. The player's
        // grounded checkpoint can then commit after the hit, not just before it.
        require(out.total > 0.0 && out.player > 0.0,
                "damage: real punch has accumulated before checkpoint");
        const std::uint64_t commits_before = s.snapshot().checkpoint_commit_count;
        s.debug_place_matron({0.0, 11.0, 0.0});
        for (int i = 0; i < 180 && s.snapshot().checkpoint_commit_count == commits_before; ++i) tick(s);
        require(s.snapshot().checkpoint_commit_count > commits_before,
                "damage: a checkpoint actually commits after judged damage");
        const auto damaged_save = s.save_game();
        require(damaged_save.size() == earlier.size(),
                "damage: a damaged checkpoint has the unchanged disk-save layout");
        require(s.load_game(damaged_save.data(), damaged_save.size()),
                "damage: damaged checkpoint loads without a damage extension");
        require(s.matron_state().alive &&
                    s.matron_state().damage_ns == 0.0 &&
                    s.matron_state().player_damage_ns == 0.0,
                "damage: reload after a damaged checkpoint restores her alive and undamaged");
        require(s.load_game(earlier.data(), earlier.size()), "damage: saved checkpoint restores");
        require(s.matron_state().damage_ns == 0.0 && s.matron_state().player_damage_ns == 0.0,
                "damage: rewind to earlier checkpoint removes later damage");
    }
    return out;
}
}

void run_matron_damage() {
    constexpr std::array<std::pair<float, int>, 11> edges = {{
        {0.0F, 0}, {1199.0F, 0}, {1200.0F, 1}, {2699.0F, 1},
        {2700.0F, 2}, {4199.0F, 2}, {4200.0F, 3}, {5399.0F, 3},
        {5400.0F, 4}, {5999.0F, 4}, {6000.0F, 4}
    }};
    for (const auto &edge : edges) {
        require(Matron::damage_stage_for_ns(edge.first) == edge.second,
                "damage: exact stage boundary");
        require(Matron::damage_lethal_at_ns(edge.first) == (edge.first >= 6000.0F),
                "damage: 6000 is DEAD, not an additional stage");
    }
    require(Matron::damage_recovery_scale_for_ns(5400.0F) == 1.6F,
            "damage: stage-four recovery is 1.6x");
    std::cout << "PASS matron-damage boundaries\n";
    const Hit fist = hit(true, false, false);
    require(fist.total >= fist.player && std::abs(fist.total - fist.player) < 0.02,
            "damage: fist alone judged once");
    std::cout << "PASS matron-damage punch checkpoint\n";
    const Hit environment = hit(false, true, false);
    require(environment.total > 0.0 && environment.player == 0.0,
            "damage: physical stock counts without player attribution");
    const Hit mixed = hit(true, true, false);
    require(mixed.total > mixed.player && mixed.player > 0.0,
            "damage: same judged window includes player and stock shares without attributing stock to player");
    std::cout << "PASS matron-damage environment mixed\n";
    const Hit down = hit(false, true, true);
    require(down.down, "damage: Down trial reached the real Down state");
    std::cout << "PASS matron-damage Down pause\n";
}
SCRAPERX_SLICE("matron-damage", run_matron_damage);
