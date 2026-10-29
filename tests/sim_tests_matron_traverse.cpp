// The Matron on the tower's own ladders, up AND down (sim/matron.cpp
// climb(): her hand on a real hold, a capped force, gravity on): each climb of
// the first multi-deck territory taken both ways from a standing start, and
// each must leave her standing on the steel at its other end, on the deck
// its steel is (sim_tests_decks.hpp), having never been carried, snapped or
// let fall.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim_tests_decks.hpp"

namespace {
using scraperx_test_decks::Deck;
using scraperx_test_decks::deck_at;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

struct Leg {
    const char *name;
    bool pipe;          // the corner pipe, else the ladder nearest her
    bool down;
    Vector3 start;      // her feet, standing
    double end_y;       // the top of the steel she ends on
    Deck end_deck;      // Deck::None: a ledge of C3's that is no deck
};

struct Result {
    double x = 0.0, y = 0.0, z = 0.0, peak_force = 0.0, worst_step = 0.0, seconds = 0.0;
    std::uint64_t releases = 0;
};

Result take(const Leg &leg) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron traverse: settle");
    s.debug_place_matron(leg.start);
    for (int i = 0; i < 30; ++i) {
        tick(s);
    }
    const auto before = s.matron_state();
    require(s.debug_matron_climb_way(leg.pipe, leg.down), leg.name);
    Result r;
    auto last = s.matron_state().position;
    int ticks = 0;
    // (Her view of herself, climbing or not, is refreshed in the step.)
    for (bool climbing = true; ticks < 90 * static_cast<int>(Simulation::kTickRateHz) && climbing; ++ticks) {
        tick(s);
        climbing = s.matron_state().climbing || s.matron_state().on_pipe;
        const auto m = s.matron_state();
        r.peak_force = std::max(r.peak_force, m.climb_force_n);
        r.worst_step = std::max(r.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                         m.position.z - last.z));
        last = m.position;
    }
    // Then a second on her feet, to show she stands there.
    for (int i = 0; i < 90; ++i) {
        tick(s);
    }
    const auto m = s.matron_state();
    r.x = m.position.x;
    r.y = m.position.y - Matron::kStandCentre;
    r.z = m.position.z;
    r.seconds = ticks / Simulation::kTickRateHz;
    r.releases = m.climb_releases - before.climb_releases;
    return r;
}

} // namespace

void run_matron_traverse() {
    const std::array<Leg, 12> legs{{
        {"home ladder up: 154 to the 176 ring", false, false, {5.0, 154.0, -129.55}, 176.25,
         Deck::Ring176},
        {"home ladder down: the 176 ring to 154", false, true, {5.0, 176.25, -128.55}, 154.0,
         Deck::Deck154},
        {"C3 riser up: 143 to its bridge over 154's rail", false, false, {-6.0, 143.03, -133.85},
         Matron::kRiserBridgeTop, Deck::None},
        {"C3 riser down: its bridge to the 143 plate", false, true, {-6.0, 155.18, -132.85}, 143.03,
         Deck::Deck143},
        {"C3 wall ladder up: the duct to 143", false, false, {-10.5, 137.2, -135.85}, 143.03,
         Deck::Deck143},
        {"C3 wall ladder down: 143 to the duct", false, true, {-10.5, 143.03, -134.85}, 137.2,
         Deck::None},
        {"C3 duct up: the landing onto the duct", false, false, {-10.5, 135.5, -144.4}, 137.2,
         Deck::None},
        {"C3 duct down: the duct to the landing", false, true, {-10.5, 137.2, -143.4}, 135.5,
         Deck::None},
        // Test fixture placement only (the world's deck stand, not runtime input).
        {"corner pipe down: 154 to 143", true, true,
         {14.85, 154.0, -132.55}, 143.0, Deck::Deck143},
        {"corner pipe up: 143 to 154", true, false,
         {14.85, 143.0, -132.55}, 154.0, Deck::Deck154},
        {"corner pipe down: 143 to 132", true, true,
         {14.85, 143.0, -132.55}, 132.0, Deck::Deck132},
        {"corner pipe up: 132 to 143", true, false,
         {14.85, 132.0, -132.55}, 143.0, Deck::Deck143},
    }};
    for (const Leg &leg : legs) {
        const Result r = take(leg);
        std::cout << "  matron traverse: " << leg.name << ": " << r.seconds << " s, ends (" << r.x << ", " << r.y
                  << ", " << r.z << "), peak " << r.peak_force << " N, worst step " << r.worst_step
                  << " m, releases " << r.releases << '\n';
        require(r.releases == 0, "matron traverse: no hold lost, no failed climb");
        require(std::abs(r.y - leg.end_y) < 0.08, "matron traverse: standing on the steel at its other end");
        require(r.peak_force <= law::kForceN + 0.1, "matron traverse: her hands never past their cap");
        require(r.worst_step < 0.15, "matron traverse: never carried or snapped (a step a tick)");
        if (leg.end_deck != Deck::None) {
            require(deck_at(float(r.x), float(r.y), float(r.z)) == leg.end_deck,
                    "matron traverse: on the deck its steel is");
        }
    }
    std::cout << "PASS scraperx_sim matron traverse: legs=" << legs.size() << '\n';
}
