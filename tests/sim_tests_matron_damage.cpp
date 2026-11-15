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
                "damage: damaged checkpoint loads");
        require(std::abs(s.matron_state().damage_ns - out.total) < 0.001 &&
                    std::abs(s.matron_state().player_damage_ns - out.player) < 0.001,
                "damage: reload after a damaged checkpoint restores the same damage");
        require(s.matron_state().alive, "damage: reload after a damaged checkpoint restores her alive");
        // Kill her by a fall, past the lethal landing: a death the damage
        // meters do not hold, so only a saved death brings it back.
        s.debug_place_matron({0.0, 154.0 + 16.0, static_cast<double>(Matron::kBandSouthZ)});
        bool dead = false;
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !dead; ++i) {
            tick(s);
            dead = s.matron_state().state == static_cast<int>(Matron::State::Dead);
        }
        require(dead && !s.matron_state().alive, "damage: a 16 m fall is the end of her");
        const std::uint64_t death_commits_before = s.snapshot().checkpoint_commit_count;
        for (int i = 0; i < 180 && s.snapshot().checkpoint_commit_count == death_commits_before; ++i) tick(s);
        require(s.snapshot().checkpoint_commit_count > death_commits_before,
                "damage: a checkpoint actually commits after her death");
        const auto dead_save = s.save_game();
        require(dead_save.size() == earlier.size(),
                "damage: a death checkpoint has the unchanged disk-save layout");
        require(s.load_game(dead_save.data(), dead_save.size()), "damage: death checkpoint loads");
        require(!s.matron_state().alive && s.matron_state().state == static_cast<int>(Matron::State::Dead),
                "damage: reload after a death checkpoint restores her dead");
        require(std::abs(s.matron_state().damage_ns - out.total) < 0.001,
                "damage: a death checkpoint keeps the damage she died with");
        require(s.load_game(earlier.data(), earlier.size()), "damage: saved checkpoint restores");
        require(s.matron_state().alive && s.matron_state().damage_ns == 0.0 &&
                    s.matron_state().player_damage_ns == 0.0,
                "damage: rewind to earlier checkpoint removes later damage");
    }
    return out;
}
void place_after_death() {
    // J-070: a Matron placed by a debug scenario is drawn where the sim has
    // her. A kill left dead_ set while place_for_test moved her to Hunting,
    // so the view (released, and alive or in the Dead state) refused her and
    // she stayed invisible even metres from the player.
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    // Kill her with a fall: 60 m over the yard lands past the 16 m/s lethal
    // landing, on grade or on any deck under her.
    s.debug_place_matron({0.0, 60.0, 0.0});
    int dead_tick = -1;
    for (int i = 0; i < 900; ++i) {
        tick(s);
        if (s.matron_state().state == static_cast<int>(Matron::State::Dead)) {
            dead_tick = i;
            break;
        }
    }
    require(dead_tick >= 0, "place-after-death: the fall kills her");
    require(!s.matron_state().alive, "place-after-death: she is dead after the fall");
    // Debug-place the dead Matron beside the player, as the live look did.
    const Vector3 p = s.snapshot().player_position;
    s.debug_place_matron({p.x + 6.0, p.y, p.z - 6.0});
    for (int i = 0; i < 5; ++i) tick(s);
    const auto m = s.matron_state();
    require(m.alive, "place-after-death: a debug-placed Matron is alive");
    require(m.released, "place-after-death: a debug-placed Matron is released");
    // She may search before she sees him (no lead survives the kill), but
    // the view draws any out state: what the fix owes is alive and out, not
    // the exact hunting state. Dead is the one state that is not drawn.
    require(m.state != static_cast<int>(Matron::State::Dead),
            "place-after-death: a debug-placed Matron is not dead");
    require(m.state >= static_cast<int>(Matron::State::Emerging),
            "place-after-death: a debug-placed Matron is out where the view draws her");
    // And she stays that way: the placement does not re-kill her.
    for (int i = 0; i < 30; ++i) tick(s);
    const auto m2 = s.matron_state();
    require(m2.alive && m2.state != static_cast<int>(Matron::State::Dead),
            "place-after-death: she stays alive and out after placement");
    std::cout << "PASS matron-damage place-after-death (died tick " << dead_tick << ")\n";
}

// J-040 (issue #228): blows while she is Down or Emerging count. The old
// guard in Matron::take_blow returned early in both states, so a 2-ton slab
// on her while Down added 0 N-s. These three trials are Claude's ruling
// tests 1-3; the Down-timer trial is the reworked test_down block in hit(),
// which rides fb-bot/integrate-protected-j040.
std::uint32_t plate_slab(const Simulation &s) {
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::PlateSlab)) {
            return s.kit_body_index(LooseStock::kFirstEntity + i);
        }
    }
    require(false, "down-blows: the world has its 2,000 kg plate slab");
    return 0;
}

void ready_hunting(Simulation &s) {
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    s.launch_matron_for_test({p.x, p.y - 0.20, p.z + 1.05});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                    s.matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) tick(s);
    require(s.matron_state().state == static_cast<int>(Matron::State::Hunting),
            "down-blows: she finishes Emerging before the trial");
}

void slab_on_down() {
    Simulation s(InitialSpawn::Deck154);
    ready_hunting(s);
    s.debug_set_matron_stage(0);  // Pest: the longest Down, 4.0 s
    const std::uint32_t first = s.loose_stock_throwables_begin();
    const std::uint32_t piece = s.kit_body_index(LooseStock::kFirstEntity + first);
    const Vector3 her0 = s.matron_state().position;
    const double feet = her0.y - static_cast<double>(Matron::kStandCentre);
    s.debug_launch_kit_body(piece, {her0.x, feet + 1.3, her0.z + 2.0}, {0.0, 0.0, -5.8});
    for (int i = 0; i < 65 && s.matron_state().state != static_cast<int>(Matron::State::Down); ++i) tick(s);
    require(s.matron_state().state == static_cast<int>(Matron::State::Down),
            "slab-down: the stock blow puts her Down");
    const double down_damage = s.matron_state().damage_ns;
    require(s.matron_state().down_left_s > 2.0,
            "slab-down: she lies there with time for the slab to fall");
    // The 2,000 kg plate slab, let go over her lying body, centre 4 m over
    // the deck: about 0.8 s to her, well inside her Down, and its blow is
    // far past the 6000 N-s death line.
    const Vector3 her = s.matron_state().position;
    const std::uint32_t slab = plate_slab(s);
    s.debug_launch_kit_body(slab, {her.x, 154.0 + 4.0, her.z}, {0.0, 0.0, 0.0});
    bool dead = false;
    for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz) && !dead; ++i) {
        tick(s);
        dead = !s.matron_state().alive;
    }
    const auto m = s.matron_state();
    require(dead && !m.alive, "slab-down: the 2-ton slab on her while Down kills her");
    require(m.damage_ns >= 6000.0, "slab-down: the slab's blow reaches the death line");
    std::cout << "PASS matron-damage slab-down (down damage " << down_damage
              << " -> " << m.damage_ns << " N-s)\n";
}

void blow_while_emerging() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    s.launch_matron_for_test({p.x, p.y - 0.20, p.z + 1.05});
    const std::uint32_t first = s.loose_stock_throwables_begin();
    const std::uint32_t piece = s.kit_body_index(LooseStock::kFirstEntity + first);
    // Emerging lasts 2.2 s; the piece reaches her in about 0.35 s, so the
    // blow lands while she is still coming out. The state before the tick
    // that first reads damage is the state the blow was read in.
    bool struck_emerging = false;
    double struck_damage = 0.0;
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        const auto before = s.matron_state();
        if (i == 5) {
            const Vector3 her = before.position;
            const double feet = her.y - static_cast<double>(Matron::kStandCentre);
            s.debug_launch_kit_body(piece, {her.x, feet + 1.3, her.z + 2.0}, {0.0, 0.0, -5.8});
        }
        tick(s);
        const auto m = s.matron_state();
        if (m.damage_ns > 0.0) {
            struck_emerging = before.state == static_cast<int>(Matron::State::Emerging);
            struck_damage = m.damage_ns;
            break;
        }
        if (m.state == static_cast<int>(Matron::State::Hunting)) break;
    }
    require(struck_emerging, "emerging-blow: the blow landed while she was Emerging");
    require(struck_damage > 0.0, "emerging-blow: a blow while Emerging counts");
    std::cout << "PASS matron-damage emerging-blow (" << struck_damage << " N-s)\n";
}

void own_contacts_add_nothing() {
    // The locker burst is her emergence: she comes out with no damage.
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    s.launch_matron_for_test({p.x, p.y - 0.20, p.z + 1.05});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                    s.matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) tick(s);
    require(s.matron_state().state == static_cast<int>(Matron::State::Hunting),
            "own-contacts: she finishes Emerging");
    require(s.matron_state().damage_ns == 0.0, "own-contacts: the locker burst adds 0");
    // Her landing: set down 2.5 m over the deck and let her fall. She lands
    // at about 7 m/s, under the 8.2 m/s hard-landing bar, and the floor is
    // static, so the striker filter drops it: no damage.
    s.debug_place_matron({p.x + 3.0, 154.0 + 2.5, p.z});
    for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz); ++i) tick(s);
    const auto m = s.matron_state();
    require(m.damage_ns == 0.0, "own-contacts: her landing adds 0");
    require(std::abs(m.position.y - (154.0 + static_cast<double>(Matron::kStandCentre))) < 0.5,
            "own-contacts: she landed back on the deck");
    std::cout << "PASS matron-damage own-contacts (locker burst and landing add 0)\n";
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
    slab_on_down();
    blow_while_emerging();
    own_contacts_add_nothing();
    place_after_death();
}
SCRAPERX_SLICE("matron-damage", run_matron_damage);
