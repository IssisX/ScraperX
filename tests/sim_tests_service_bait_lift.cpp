// J-108 (#427), the bait lift west of the 706 deck's strip, beside C5: two
// platforms on guides, one rope over the head. From the strip: A, a level
// gap of 1.0 m, B. He crosses A and jumps the gap onto B; she, hunting him,
// follows him onto A and stops at the gap (she has no level gap move); her
// weight sinks A to the 684 deck and lifts B, with him on it, to the 728
// deck, and he steps off along the catwalk. Nothing aboard, or him alone on
// A, it stays; her alone on A, she lands under her drop limit. Gravity on
// throughout; nothing here sets a speed: her weight and his are the only
// drive, his moves are his inputs and hers her own hunt.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
namespace traversal = scraperx::sim::traversal;

constexpr double kDeck684 = 684.1;
constexpr double kDeck706 = 706.1;
constexpr double kDeck728 = 728.1;
constexpr double kStroke = 21.9;              // A's underside onto the 684 deck
constexpr double kAX = -6.085, kBX = -9.3, kZ = -145.0;
constexpr double kAWest = -7.0, kBEast = -8.0, kBWest = -10.6;
constexpr double kStripX = -3.5;              // the 706 deck's strip, x -5.1 to -2.0
constexpr double kPlayerAboveFeet = 0.9;      // his body's centre over his soles
// Her drop limit (traversal::Limits::drop_max_m, 4.6 m): the speed a free
// fall of it lands at.
const double kHerDropMps = std::sqrt(2.0 * 9.81 * double(traversal::Limits{}.drop_max_m));

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }
void settle(Simulation &s, const double seconds) {
    for (double t = 0.0; t < seconds; t += Simulation::kFixedStepSeconds) tick(s);
}
double her_feet(const Simulation &s) { return s.matron_state().position.y - double(Matron::kStandCentre); }
bool trace() { return std::getenv("SCRAPERX_BAIT_TRACE") != nullptr; }

void bait_stays() {
    // Nobody aboard: B, the heavier, sits on its stop.
    Simulation s(InitialSpawn::Deck706);
    settle(s, 10.0);
    const auto empty = s.service_state();
    std::cout << "  bait lift, nobody aboard: a=" << empty.bait_a_travel << " b=" << empty.bait_b_travel
              << " chain_kg=" << empty.bait_chain_kg << " rope_n=" << empty.bait_rope_tension << '\n';
    require(std::abs(empty.bait_a_travel) < 0.01 && std::abs(empty.bait_b_travel) < 0.01,
            "bait lift: with nobody aboard it stays");
    // Him alone on A: 30 kg over B, held by the guides' friction.
    s.debug_launch_player(Vector3{kAX, kDeck706 + kPlayerAboveFeet, kZ}, Vector3{0.0, 0.0, 0.0});
    // His landing on it jolts it a few centimetres; then the friction holds.
    settle(s, 5.0);
    const double jolt = s.service_state().bait_a_travel;
    settle(s, 10.0);
    const auto him = s.service_state();
    const auto at = s.snapshot().player_position;
    std::cout << "  bait lift, him alone on A: jolt=" << jolt << " a=" << him.bait_a_travel << " b=" << him.bait_b_travel
              << " him=" << at.x << "," << at.y << "," << at.z << '\n';
    require(std::abs(at.x - kAX) < 0.6 && std::abs(at.y - kDeck706 - kPlayerAboveFeet) < 0.3,
            "bait lift: he stands on A");
    require(him.bait_a_travel < 0.1 && std::abs(him.bait_a_travel - jolt) < 0.002,
            "bait lift: with him alone on A it stays (a jolt as he lands, then held)");
    std::cout << "PASS scraperx_sim bait lift stays: nobody, and him alone on A\n";
}

// The bait, all of it his inputs and her hunt. He walks west off the strip
// across A and jumps the gap, stick held (B's far wall stops him), and waits
// on B. She hunts him from the strip, south of him.
void bait_take() {
    Simulation s(InitialSpawn::Deck706);
    settle(s, 1.0);
    s.debug_launch_player(Vector3{kStripX, kDeck706 + kPlayerAboveFeet, kZ}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(Vector3{kStripX, kDeck706, kZ - 8.0});
    settle(s, 0.5);
    if (trace()) {
        const traversal::AffordanceMap &map = s.matron_affordances();
        const int ra = map.region_at(JPH::Vec3(float(kAX), float(kDeck706), float(kZ)));
        const int rs = map.region_at(JPH::Vec3(float(kStripX), float(kDeck706), float(kZ)));
        std::cout << "    map: A's region " << ra << " strip's region " << rs << " patch_at_A " << map.patch_at(JPH::Vec3(float(kAX), float(kDeck706), float(kZ)), 0.3F) << '\n';
    }
    const auto catches0 = s.matron_state().catches;
    const auto deaths0 = s.snapshot().death_count;
    constexpr int hz = static_cast<int>(Simulation::kTickRateHz);
    bool jumped = false, on_b = false;
    double t = 0.0, t_on_b = -1.0, t_her_on_a = -1.0, t_arrive = -1.0, prev_a = 0.0, a_arrive = 0.0, a_peak = 0.0;
    double her_low = 1.0e9, her_fall = 0.0, prev_her_y = s.matron_state().position.y, his_land_x = 0.0;
    int i = 0;
    for (; t < 120.0; ++i, t += Simulation::kFixedStepSeconds) {
        s.debug_set_matron_stage(1);
        const auto me = s.snapshot();
        // Heard where he stands, every 2 s (the floor under him, as her
        // hearing gives it).
        if (i % (2 * hz) == 0) {
            s.debug_matron_hear(Vector3{me.player_position.x, me.player_position.y - kPlayerAboveFeet, me.player_position.z});
        }
        if (!jumped) {
            (void)s.set_facing(-1.0, 0.0);
            (void)s.set_move_input(-1.0, 0.0);
            if (me.player_position.x <= kAWest + 0.5) {
                (void)s.request_jump();
                jumped = true;
            }
        } else if (!on_b) {
            (void)s.set_move_input(-1.0, 0.0);
        } else {
            (void)s.set_move_input(0.0, 0.0);
        }
        tick(s);
        const auto n = s.snapshot();
        const auto st = s.service_state();
        const auto m = s.matron_state();
        if (jumped && !on_b && n.player_grounded && n.player_position.x < kBEast && n.player_position.x > kBWest) {
            on_b = true;
            t_on_b = t;
            his_land_x = n.player_position.x;
        }
        const double feet = m.position.y - double(Matron::kStandCentre);
        const bool her_on_a = m.position.x < -5.1 && m.position.x > kAWest - 0.35 && std::abs(m.position.z - kZ) < 1.5;
        if (t_her_on_a < 0.0 && her_on_a) t_her_on_a = t;
        const double va = (st.bait_a_travel - prev_a) / Simulation::kFixedStepSeconds;
        prev_a = st.bait_a_travel;
        a_peak = std::max(a_peak, va);
        her_fall = std::max(her_fall, (prev_her_y - m.position.y) / Simulation::kFixedStepSeconds);
        prev_her_y = m.position.y;
        her_low = std::min(her_low, feet);
        if (t_arrive < 0.0 && st.bait_a_travel > kStroke - 0.05) {
            a_arrive = va;
            t_arrive = t;
        }
        if (trace() && i % (hz / 2) == 0) {
            std::cout << "    t=" << t << " him=" << n.player_position.x << "," << n.player_position.y << ","
                      << n.player_position.z << " g=" << n.player_grounded << " her=" << m.position.x << "," << feet
                      << "," << m.position.z << " state=" << int(m.state) << " a=" << st.bait_a_travel
                      << " b=" << st.bait_b_travel << " catches=" << (m.catches - catches0) << " target=" << m.target_x << "," << m.target_z << " sees=" << m.sees_player << '\n';
        }
        if (t_arrive > 0.0 && t > t_arrive + 8.0) break;
    }
    const auto st = s.service_state();
    const auto him = s.snapshot();
    const auto her = s.matron_state();
    const double his_feet = him.player_position.y - kPlayerAboveFeet;
    std::cout << "  bait lift take: on_b_t=" << t_on_b << " land_x=" << his_land_x << " her_on_a_t=" << t_her_on_a
              << " arrive_t=" << t_arrive << " a=" << st.bait_a_travel << " b=" << st.bait_b_travel
              << " a_arrive_mps=" << a_arrive << " a_peak_mps=" << a_peak << " her_fall_mps=" << her_fall
              << " his_feet=" << his_feet << " her_feet=" << her_feet(s) << " her_alive=" << her.alive
              << " her_knockdowns=" << her.knockdowns << " catches=" << (her.catches - catches0)
              << " deaths=" << (him.death_count - deaths0) << " his_ankle_breaks=" << him.ankle_break_count << '\n';
    require(t_on_b > 0.0, "bait lift take: he jumps the gap onto B and stands on it");
    require(t_her_on_a > 0.0, "bait lift take: she follows him onto A by her own hunt");
    require(her.catches == catches0, "bait lift take: across the gap her hands never reach him");
    require(t_arrive > 0.0, "bait lift take: her weight sinks A to the 684 deck");
    require(a_arrive <= 2.5, "bait lift take: A meets the 684 deck no faster than the evaluator's band (2.2 m/s)");
    require(st.bait_b_travel >= kStroke - 0.05 && st.bait_b_travel <= kStroke + 0.5,
            "bait lift take: B settles on its dogs at the 728 deck");
    require(std::abs(his_feet - kDeck728) < 0.35, "bait lift take: he is carried up to the 728 deck");
    require(him.death_count == deaths0 && him.ankle_break_count == 0, "bait lift take: he rides it unhurt");
    require(her.alive && her.knockdowns == 0 && std::abs(her_feet(s) - kDeck684) < 0.4,
            "bait lift take: she rides it down to the 684 deck on her feet");
    require(her_fall < kHerDropMps, "bait lift take: she lands under her 4.6 m drop limit");
    // He steps off B east along the catwalk onto the 728 deck (x -2.0 on).
    (void)s.set_facing(1.0, 0.0);
    for (int k = 0; k < 4 * hz; ++k) {
        (void)s.set_move_input(1.0, 0.0);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    settle(s, 1.0);
    const auto off = s.snapshot();
    std::cout << "  bait lift take, off B: him=" << off.player_position.x << "," << off.player_position.y - kPlayerAboveFeet
              << "," << off.player_position.z << " grounded=" << off.player_grounded << '\n';
    require(off.player_position.x > -1.5 && std::abs(off.player_position.y - kPlayerAboveFeet - kDeck728) < 0.3 &&
                off.player_grounded && off.death_count == deaths0,
            "bait lift take: he walks off B along the catwalk onto the 728 deck");
    std::cout << "PASS scraperx_sim bait lift take: he is lifted " << (st.bait_b_travel) << " m in " << (t_arrive - t_her_on_a)
              << " s after she steps on; A meets the deck at " << a_arrive << " m/s\n";
}

// Her alone on A (him elsewhere): A sinks faster and B runs up empty onto its
// dogs; she lands on the 684 deck under her drop limit, and climbs back to
// the 706 deck by her own plan, past the lift's mesh, on her own moves.
void bait_her_alone() {
    Simulation s(InitialSpawn::Deck706);
    settle(s, 1.0);
    s.debug_place_matron(Vector3{kAX, kDeck706, kZ});
    double prev_y = s.matron_state().position.y, fall = 0.0, t = 0.0, t_arrive = -1.0;
    for (; t < 30.0; t += Simulation::kFixedStepSeconds) {
        tick(s);
        const auto m = s.matron_state();
        fall = std::max(fall, (prev_y - m.position.y) / Simulation::kFixedStepSeconds);
        prev_y = m.position.y;
        if (t_arrive < 0.0 && s.service_state().bait_a_travel > kStroke - 0.05) t_arrive = t;
        if (t_arrive > 0.0 && t > t_arrive + 3.0) break;
    }
    const auto st = s.service_state();
    const auto her = s.matron_state();
    std::cout << "  bait lift, her alone: arrive_t=" << t_arrive << " a=" << st.bait_a_travel << " b=" << st.bait_b_travel
              << " her_fall_mps=" << fall << " limit_mps=" << kHerDropMps << " her_feet=" << her_feet(s)
              << " alive=" << her.alive << " knockdowns=" << her.knockdowns << " damage_ns=" << her.damage_ns << '\n';
    require(t_arrive > 0.0 && std::abs(her_feet(s) - kDeck684) < 0.4,
            "bait lift, her alone: A takes her down to the 684 deck");
    require(fall < kHerDropMps, "bait lift, her alone: she lands under her 4.6 m drop limit");
    require(her.alive && her.knockdowns == 0, "bait lift, her alone: she lands on her feet");
    // Up to its guide's top (kStroke + 0.5, a tooth), a float's step over.
    require(st.bait_b_travel >= kStroke - 1.0 && st.bait_b_travel <= kStroke + 0.51,
            "bait lift, her alone: B runs up empty and holds on its dogs");
    // Her way back up from where A let her off, past the lift's mesh.
    const traversal::AffordanceMap &map = s.matron_affordances();
    const std::uint32_t failed_before = s.matron_journey().moves_failed;
    require(s.debug_matron_travel(kDeck706), "bait lift, her alone: a journey from the 684 deck to the 706 deck plans");
    Vector3 last = her.position;
    double worst_step = 0.0, worst_force = 0.0, worst_power = 0.0, climb_t = 0.0;
    std::string moves;
    std::uint8_t move = 0;
    for (; climb_t < 150.0; climb_t += Simulation::kFixedStepSeconds) {
        tick(s);
        const Simulation::MatronJourney j = s.matron_journey();
        const Vector3 at = s.matron_state().position;
        worst_step = std::max(worst_step, std::hypot(at.x - last.x, at.y - last.y, at.z - last.z));
        last = at;
        worst_force = std::max(worst_force, j.force_n + double(s.matron_hoist().hand_n));
        worst_power = std::max(worst_power, j.power_w + double(s.matron_hoist().hand_power_w));
        if (j.move != move && j.move != 0) {
            moves += std::string(moves.empty() ? "" : ",") + traversal::move_name(static_cast<traversal::MoveKind>(j.move));
        }
        move = j.move;
        if (!j.active) break;
    }
    const Simulation::MatronJourney j = s.matron_journey();
    const double top = j.region >= 0 ? map.regions()[static_cast<std::size_t>(j.region)].top : -1.0;
    std::cout << "  bait lift, her way back up: t=" << climb_t << " region_top=" << top << " feet=" << her_feet(s)
              << " way=" << moves << " failed=" << (j.moves_failed - failed_before) << " worst_step=" << worst_step
              << " worst_force=" << worst_force << " worst_power=" << worst_power << '\n';
    require(!j.active && j.arrivals > 0 && std::abs(top - kDeck706) < 0.3 && std::abs(her_feet(s) - kDeck706) < 0.4,
            "bait lift, her alone: she climbs back to the 706 deck by her own plan");
    require(j.moves_failed == failed_before && worst_step < 0.2,
            "bait lift, her alone: every move ends Done, never snapped");
    require(worst_force <= double(traversal::Limits{}.total_force_n) + 1.0 && worst_power <= 44000.0,
            "bait lift, her alone: her feet and hands stay inside her 8 kN, 44 kW budget");
    std::cout << "PASS scraperx_sim bait lift, her alone: lands at " << fall << " m/s (limit " << kHerDropMps
              << "), back up in " << climb_t << " s by " << moves << '\n';
}
// A save as a world before the lift wrote it (measured against a save that
// trunk's own build wrote: its bytes are this world's with exactly these
// taken out, every list's other entries the same, but solver noise in the
// 1e-8s): without A and B and the hightower toolboxes (the kit's last five
// bodies: their entities and states), the last rope (the lift's) and the last
// two guides' dogs (A's and B's). With `bodies` false, the bodies stay and
// only the dogs go: a save that matches no world. The layout is
// Simulation::save_game's: the format key, the tick, the checkpoint's
// position, crouch, commits, deaths and carried entity, then the lists.
std::vector<std::uint8_t> before_the_lift(const Simulation &s, const std::vector<std::uint8_t> &bytes, const bool bodies) {
    using State = scraperx::sim::kit::Kit::BodyState;
    std::vector<std::uint8_t> out;
    std::size_t at = 8 + 8 + sizeof(JPH::RVec3) + 1 + 8 + 8 + 8;
    out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(at));
    // One list: its count, less `drop`, then its first elements; the
    // dropped ones skipped.
    const auto list = [&](const std::size_t size, const std::uint32_t drop) {
        std::uint32_t count = 0;
        std::memcpy(&count, bytes.data() + at, 4);
        require(count >= drop, "bait lift, old save: the list holds what the lift added");
        const std::uint32_t kept = count - drop;
        const auto *k = reinterpret_cast<const std::uint8_t *>(&kept);
        out.insert(out.end(), k, k + 4);
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at + 4),
                   bytes.begin() + static_cast<std::ptrdiff_t>(at + 4 + size * kept));
        at += 4 + size * count;
    };
    // A and B come right before the hightower toolboxes, the save's last
    // bodies; each toolbox entity is checked.
    const std::uint32_t hightower_begin = s.loose_stock_hightower_begin();
    const std::uint32_t toolboxes = s.loose_stock_count() - hightower_begin;
    std::uint32_t entities = 0;
    std::memcpy(&entities, bytes.data() + at, 4);
    std::uint64_t ab[2] = {};
    std::memcpy(ab, bytes.data() + at + 4 + 8 * (std::size_t{entities} - 2 - toolboxes), 16);
    require(ab[0] == Simulation::kServiceBaitAEntityId && ab[1] == Simulation::kServiceBaitBEntityId,
            "bait lift, old save: A and B come right before the toolboxes");
    for (std::uint32_t i = 0; i < toolboxes; ++i) {
        std::uint64_t e = 0;
        std::memcpy(&e, bytes.data() + at + 4 + 8 * (std::size_t{entities} - toolboxes + i), 8);
        require(e == Simulation::kLooseStockFirstEntity + hightower_begin + i,
                "bait lift, old save: each toolbox entity is checked");
    }
    const std::uint32_t drop = bodies ? 2 + toolboxes : 0;
    list(8, drop);                   // the bodies' entities
    list(sizeof(State), drop);       // their states
    list(4, bodies ? 1 : 0);         // the ropes' anchors: the lift's rope last
    list(4, 2);                      // the guides' dogs: A's and B's last
    list(4, 0);                      // the bins
    list(1, bodies ? 1 : 0);         // the ropes' parted bits
    out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at), bytes.end());
    return out;
}

void bait_old_save() {
    // A checkpoint saved before the lift was built loads into this world:
    // the lift's bodies, rope and dogs start as built, and it stays put.
    Simulation s(InitialSpawn::Deck706);
    settle(s, 1.0);
    const std::vector<std::uint8_t> bytes = s.save_game();
    const std::vector<std::uint8_t> old = before_the_lift(s, bytes, true);
    std::cout << "  bait lift, old save: " << bytes.size() << " bytes now, " << old.size() << " as before the lift\n";
    Simulation loaded(InitialSpawn::Deck706);
    require(loaded.load_game(old.data(), old.size()), "bait lift, old save: a save from before the lift loads");
    settle(loaded, 10.0);
    const auto lift = loaded.service_state();
    std::cout << "  bait lift, old save loaded: a=" << lift.bait_a_travel << " b=" << lift.bait_b_travel << '\n';
    require(std::abs(lift.bait_a_travel) < 0.01 && std::abs(lift.bait_b_travel) < 0.01,
            "bait lift, old save: loaded, the lift stands as built and stays");
    // Fewer dogs with every body saved matches no world: refused.
    const std::vector<std::uint8_t> odd = before_the_lift(s, bytes, false);
    Simulation refused(InitialSpawn::Deck706);
    require(!refused.load_game(odd.data(), odd.size()),
            "bait lift, old save: fewer dogs with no fewer bodies is refused");
    require(refused.load_game(bytes.data(), bytes.size()), "bait lift, old save: this world's own save loads");
    std::cout << "PASS scraperx_sim bait lift: a save from before it loads, the lift as built\n";
}
} // namespace

void run_service_bait_lift() {
    bait_stays();
    bait_take();
    bait_her_alone();
    bait_old_save();
}
SCRAPERX_GROUP("service_bait_lift", 335, run_service_bait_lift);
SCRAPERX_SLICE("service-bait-lift", run_service_bait_lift);
