// The Matron after the climber across decks, up and down (sim/matron_pipe.cpp
// and her climbs): she knows where he is only from what she hears (a sound
// every few seconds where he stands, as her ears would report it), and gets
// there by her own ways, by her own hands and feet: the corner pipe, the home
// ladder. Each run ends with her standing on his deck's steel, alive, never
// carried or snapped there.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim_tests_decks.hpp"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
namespace traversal = scraperx::sim::traversal;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
using scraperx_test_decks::Deck;
using scraperx_test_decks::deck_at;

// A way that failed under her is left alone this long before she takes it
// again (her found ways' block, matron.hpp kFoundClimbBlockTicks).
constexpr double kWayBlockS = 30.0;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

struct Chase {
    bool let_go = false;          // her hands opened on the climb (let_go_at)
    double fell_mps = 0.0;        // the fastest she fell after it
    double retook_after_s = -1.0; // from her hands opening to her next climb or pipe
    std::uint32_t knockdowns = 0;
    bool arrived = false;
    double seconds = 0.0;
    double worst_step = 0.0;
    std::uint32_t pipe_links = 0, climbs = 0;
    bool alive = true;
};

// Her at `from` (feet), dangerous; the climber standing at `prey` (feet),
// heard there every kHearEveryS. Until she stands on his deck, or `seconds`.
Chase chase(const Vector3 from, const Vector3 prey, const Deck goal, const double seconds,
            const double let_go_at = 1.0e9) {
    constexpr double kHearEveryS = 8.0;
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron pursuit: settle");
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(from);
    tick(s);   // her view of herself is refreshed in the step: from here, where she was put
    Chase c;
    auto last = s.matron_state().position;
    const auto climbs0 = s.matron_state().climbs;
    const auto knockdowns0 = s.matron_state().knockdowns;
    int let_go_tick = -1;
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    int on_goal = 0;
    for (int i = 0; i < static_cast<int>(seconds) * hz; ++i) {
        s.debug_set_matron_stage(1);
        if (i % (static_cast<int>(kHearEveryS) * hz) == 0) {
            s.debug_matron_hear(prey);
        }
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const auto m = s.matron_state();
        c.worst_step = std::max(c.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                         m.position.z - last.z));
        const double vy = (m.position.y - last.y) * hz;
        last = m.position;
        if (!m.alive) {
            c.alive = false;
            break;
        }
        if (std::getenv("SCRAPERX_PURSUIT_TRACE") != nullptr && i % (5 * hz) == 0) {
            std::cout << "    t " << i / hz << " at (" << m.position.x << ", " << m.position.y << ", " << m.position.z
                      << ") floor " << m.floor_y << " state " << int(m.state) << " pipe " << m.on_pipe
                      << " climbing " << m.climbing << '\n';
        }
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        if (!c.let_go && m.climbing && feet >= let_go_at) {
            s.debug_matron_let_go();
            c.let_go = true;
            let_go_tick = i;
        } else if (c.let_go) {
            c.fell_mps = std::max(c.fell_mps, -vy);
            if (c.retook_after_s < 0.0 && i > let_go_tick + hz && (m.climbing || m.on_pipe)) {
                c.retook_after_s = static_cast<double>(i - let_go_tick) / hz;
            }
        }
        const bool there = !m.on_pipe && !m.climbing &&
                           deck_at(float(m.position.x), float(feet), float(m.position.z)) == goal;
        on_goal = there ? on_goal + 1 : 0;
        if (on_goal >= hz) {   // a second on its steel
            c.arrived = true;
            c.seconds = static_cast<double>(i) / hz;
            break;
        }
    }
    const auto m = s.matron_state();
    c.knockdowns = m.knockdowns - knockdowns0;
    c.pipe_links = m.pipe_links;
    c.climbs = m.climbs - climbs0;
    return c;
}

void report(const char *name, const Chase &c) {
    std::cout << "  matron pursuit: " << name << ": arrived " << c.arrived << " in " << c.seconds << " s, pipe "
              << c.pipe_links << " ways, climbs " << c.climbs << ", worst step " << c.worst_step << " m, alive "
              << c.alive << '\n';
}

} // namespace

// Down the well's route onto the 176 ring, then across that ring to the
// evidence. The standpipe used to leave her in the gap beside its foot, and
// the ring's map does not contain the boards, so she stood there. She must
// arrive on the evidence floor, alive, without a step over 0.20 m. A shove
// after she is already there can kill the player and restore the checkpoint;
// that restore is not this climb, so the run stops at the arrival.
void run_matron_ring_descent() {
    constexpr double kHearEveryS = 8.0;
    constexpr double kArriveM = 3.0;
    Simulation settle(InitialSpawn::Deck154);
    require(settle.advance_frame(1.0).accepted, "matron ring descent: settle");
    const auto &map = settle.matron_affordances();
    struct Spot {
        int id = -1;
        float top = 0.0F;
        float x = 0.0F;
        float z = 0.0F;
        float area = 0.0F;
    };
    const auto spot_for = [&](const float want) {
        Spot best;
        for (const auto &region : map.regions()) {
            if (std::abs(region.top - want) > 0.08F || region.area <= best.area) {
                continue;
            }
            float area = 0.0F;
            float x = 0.0F;
            float z = 0.0F;
            for (const int patch_index : region.patches) {
                const auto &patch = map.patches()[static_cast<std::size_t>(patch_index)];
                const float patch_area = (patch.max_x - patch.min_x) * (patch.max_z - patch.min_z);
                if (patch_area > area) {
                    area = patch_area;
                    x = 0.5F * (patch.min_x + patch.max_x);
                    z = 0.5F * (patch.min_z + patch.max_z);
                }
            }
            if (area > 0.0F) {
                best = Spot{region.id, region.top, x, z, area};
            }
        }
        return best;
    };
    const Spot goal = spot_for(176.25F);
    require(goal.id >= 0, "matron ring descent: the 176 ring");
    for (const float from_top : {198.25F, 220.25F}) {
        const Spot from = spot_for(from_top);
        require(from.id >= 0, "matron ring descent: the start ring");
        Simulation s(InitialSpawn::Deck154);
        require(s.advance_frame(1.0).accepted, "matron ring descent: world");
        const Vector3 prey{goal.x, goal.top, goal.z};
        s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
        s.debug_place_matron(Vector3{from.x, from.top, from.z});
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        auto last = s.matron_state().position;
        double worst = 0.0;
        bool alive = true;
        bool arrived = false;
        double seconds = 0.0;
        const int hz = static_cast<int>(Simulation::kTickRateHz);
        const int budget = (from_top > 210.0F ? 120 : 80) * hz;
        for (int i = 0; i < budget; ++i) {
            s.debug_set_matron_stage(1);
            if (i % (static_cast<int>(kHearEveryS) * hz) == 0) {
                s.debug_matron_hear(prey);
            }
            (void)s.set_move_input(0.0, 0.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            const auto m = s.matron_state();
            const double step = std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z);
            worst = std::max(worst, step);
            last = m.position;
            if (!m.alive) {
                alive = false;
                seconds = static_cast<double>(i) / hz;
                break;
            }
            const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
            const double horizontal = std::hypot(m.position.x - prey.x, m.position.z - prey.z);
            if (!m.climbing && !m.on_pipe && horizontal < kArriveM && std::abs(feet - prey.y) < 0.6) {
                arrived = true;
                seconds = static_cast<double>(i) / hz;
                break;
            }
        }
        std::cout << "  matron ring descent: " << from.top << " -> " << goal.top << " arrived " << arrived
                  << " in " << seconds << " s, worst step " << worst << " m, alive " << alive << '\n';
        require(alive, "matron ring descent: she stays alive on the way down");
        require(arrived, "matron ring descent: she reaches the evidence on its floor");
        require(worst < 0.2, "matron ring descent: never carried or snapped");
    }
    std::cout << "PASS scraperx_sim matron ring descent\n";
}

void run_matron_pursuit() {
    // Up: from 132 by the corner pipe to 143 and 154, then the home ladder.
    {
        const Chase c = chase(Vector3{14.85, 132.0, -131.5}, Vector3{5.0, 176.25, -127.5}, Deck::Ring176, 300.0);
        report("132 up to the 176 ring", c);
        require(c.alive && c.arrived, "matron pursuit: up three decks to the climber, on her own hands and feet");
        require(c.pipe_links + c.climbs >= 3, "matron pursuit: by three ways, one a storey");
        require(c.worst_step < 0.2, "matron pursuit: never carried or snapped (a step a tick)");
    }
    // Down: from the ring by the home ladder to 154, then the pipe.
    {
        const Chase c = chase(Vector3{5.0, 176.25, -127.5}, Vector3{14.85, 132.0, -131.5}, Deck::Deck132, 300.0);
        report("the 176 ring down to 132", c);
        require(c.alive && c.arrived, "matron pursuit: down three decks to the climber");
        require(c.pipe_links + c.climbs >= 3, "matron pursuit: by three ways down, one a storey");
        require(c.worst_step < 0.2, "matron pursuit: never carried or snapped (a step a tick)");
    }
    // A way that fails under her: up the home ladder after him, her hands
    // open 4.5 m up (as a hold that gave way would open them). She falls for
    // real and lands hard on 154, alive, down a while; she leaves the ladder
    // alone its block time; and on what she still hears she takes it again
    // once it may be tried, and reaches him.
    {
        const Chase c =
            chase(Vector3{3.9, 154.0, -129.0}, Vector3{5.0, 176.25, -127.5}, Deck::Ring176, 300.0, 158.5);
        report("154 up the home ladder, her hands opening on it", c);
        std::cout << "    fell at " << c.fell_mps << " m/s, knockdowns " << c.knockdowns << ", took it again after "
                  << c.retook_after_s << " s\n";
        require(c.let_go && c.fell_mps >= Matron::kHardLandingMps && c.alive,
                "matron pursuit: her hands open, she falls for real and lives");
        require(c.knockdowns >= 1, "matron pursuit: the landing puts her down");
        require(c.retook_after_s >= kWayBlockS - 1.0, "matron pursuit: she leaves the failed way alone its block time");
        require(c.arrived, "matron pursuit: then takes it again and reaches him");
        require(c.worst_step < 0.2, "matron pursuit: never carried or snapped (a step a tick)");
    }
    std::cout << "PASS scraperx_sim matron pursuit\n";
}

// Every way between two of her decks that her scan of the tower finds (a
// ladder's rungs, a pole, a lip, a hang and drop), end to end and alone: from
// where the way starts, her travel to the region it ends in (her travel has
// her body: no other way of hers takes it), until she stands there with
// every move of it done and none failed. Its walks, its climbs and its
// top-outs are all her own body in the world, gravity on.
// The tests' answer key against the world: every deck's outline is the steel
// under it (the check the authored map's own test made of its regions).
void run_decks_are_steel() {
    Simulation s(InitialSpawn::Deck154);
    int checked = 0;
    for (const scraperx_test_decks::DeckInfo &d : scraperx_test_decks::decks()) {
        for (std::uint8_t p = 0; p < d.count; ++p) {
            const scraperx_test_decks::Patch &patch = d.patches[p];
            if (patch.moving) {
                continue;
            }
            const double top = double(d.top_y) + double(patch.rise);
            const double cx = 0.5 * double(patch.min_x + patch.max_x);
            const double cz = 0.5 * double(patch.min_z + patch.max_z);
            const bool along_x = patch.max_x - patch.min_x >= patch.max_z - patch.min_z;
            // Machines and posts stand on the bands; a sample that lands on one
            // reads its top, so try points along the patch for the bare deck.
            bool found = false;
            double nearest = 1.0e9;
            for (int k = 0; k < 9 && !found; ++k) {
                const double t = 0.1 + 0.1 * k;
                const double x = along_x ? double(patch.min_x) + double(patch.max_x - patch.min_x) * t : cx;
                const double z = along_x ? cz : double(patch.min_z) + double(patch.max_z - patch.min_z) * t;
                const double y = s.debug_support_y(Vector3{x, top + 0.05, z}, 0.5);
                if (!std::isnan(y)) {
                    nearest = std::min(nearest, std::abs(y - top));
                    found = std::abs(y - top) < 0.03;
                }
            }
            if (!found) {
                std::cerr << "  " << d.name << " patch " << int(p) << ": nearest top off by " << nearest << '\n';
            }
            require(found, "matron ways: a deck's outline is the steel under it");
            ++checked;
        }
    }
    std::cout << "PASS scraperx_sim matron decks are steel: patches=" << checked << '\n';
}

void run_matron_ways() {
    run_decks_are_steel();
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron ways: settle");
    const traversal::AffordanceMap &map = s.matron_affordances();
    const auto deck_of = [&](const int region) {
        const float top = map.regions()[static_cast<std::size_t>(region)].top;
        for (const scraperx_test_decks::DeckInfo &d : scraperx_test_decks::decks()) {
            if (std::abs(top - d.top_y) <= scraperx_test_decks::kOnTopM) {
                return d.id;
            }
        }
        return Deck::None;
    };
    const auto rung = [](const traversal::MoveKind kind) {
        return kind == traversal::MoveKind::RungUp || kind == traversal::MoveKind::RungDown;
    };
    std::vector<std::size_t> ways;
    for (std::size_t k = 0; k < map.edges().size(); ++k) {
        const traversal::Edge &e = map.edges()[k];
        if (!e.usable || e.kind == traversal::MoveKind::Walk) {
            continue;
        }
        const Deck from = deck_of(e.from);
        const Deck to = deck_of(e.to);
        if (from == Deck::None || to == Deck::None || from == to) {
            continue;
        }
        // Her planner never takes another way where a ladder's rungs reach
        // the same region: driving this one would climb the rungs instead.
        bool by_rungs = false;
        for (const traversal::Edge &o : map.edges()) {
            by_rungs = by_rungs || (o.usable && rung(o.kind) && !rung(e.kind) && o.from == e.from && o.to == e.to);
        }
        if (by_rungs) {
            std::cout << "  matron way " << scraperx_test_decks::deck(from).name << " to "
                      << scraperx_test_decks::deck(to).name << " (" << traversal::move_name(e.kind)
                      << "): not hers, a ladder's rungs reach the same region\n";
            continue;
        }
        ways.push_back(k);
    }
    int taken = 0;
    for (std::size_t w = 0; w < ways.size(); ++w) {
        if (const char *one = std::getenv("SCRAPERX_WAY"); one != nullptr && std::atoi(one) != int(w)) {
            continue;
        }
        const traversal::Edge e = map.edges()[ways[w]];
        const auto st = e.spec.start;
        s.debug_place_matron(Vector3{double(st.GetX()), double(st.GetY()), double(st.GetZ())});
        for (int i = 0; i < 30; ++i) {
            (void)s.advance_frame(1.0 / 60.0);
        }
        auto last = s.matron_state().position;
        double worst = 0.0, gravity = 1.0, secs = 0.0;
        bool alive = true;
        const auto j0 = s.matron_journey();
        const bool planned = s.debug_matron_travel_region(e.to);
        Simulation::MatronJourney j = s.matron_journey();
        while (planned && j.active && secs < 120.0) {
            (void)s.advance_frame(1.0 / 60.0);
            secs += 1.0 / 60.0;
            const auto m = s.matron_state();
            worst = std::max(worst, std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z));
            last = m.position;
            alive = alive && m.alive;
            j = s.matron_journey();
            gravity = std::min(gravity, j.gravity_factor);
        }
        // Standing where it ends: an arrival her journey counts, then a second
        // on the far deck's steel, off any rung or pipe.
        int on_goal = 0;
        if (planned && !j.active && j.region == e.to && j.arrivals > j0.arrivals) {
            for (int i = 0; i < 120 && on_goal < 60; ++i) {
                (void)s.advance_frame(1.0 / 60.0);
                secs += 1.0 / 60.0;
                const auto m = s.matron_state();
                worst = std::max(worst, std::hypot(m.position.x - last.x, m.position.y - last.y, m.position.z - last.z));
                last = m.position;
                alive = alive && m.alive;
                const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
                const bool there = !m.on_pipe && !m.climbing &&
                                   deck_at(float(m.position.x), float(feet), float(m.position.z)) == deck_of(e.to);
                on_goal = there ? on_goal + 1 : 0;
            }
        }
        const bool arrived = on_goal >= 60;
        std::cout << "  matron way " << w << ": " << scraperx_test_decks::deck(deck_of(e.from)).name << " to "
                  << scraperx_test_decks::deck(deck_of(e.to)).name << " (" << traversal::move_name(e.kind)
                  << "): arrived " << arrived << " in " << secs << " s, moves " << (j.moves_done - j0.moves_done)
                  << " (failed " << (j.moves_failed - j0.moves_failed) << "), worst step " << worst << " m, alive "
                  << alive << '\n';
        require(planned, "matron ways: her scan plans it");
        require(alive && arrived, "matron ways: along it, standing where it ends");
        require(j.moves_failed == j0.moves_failed && j.moves_done > j0.moves_done,
                "matron ways: every move of it, without a failure");
        require(worst < 0.2, "matron ways: never carried or snapped (a step a tick)");
        require(gravity == 1.0, "matron ways: gravity on her the whole way");
        ++taken;
    }
    if (std::getenv("SCRAPERX_WAY") == nullptr) {
        require(taken >= 10, "matron ways: every way between her decks that her scan finds");
    }
    std::cout << "PASS scraperx_sim matron ways: " << taken << " ways\n";
}
