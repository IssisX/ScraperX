// Body-order stability (03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md): a
// static body built into one band of the tower leaves every simulation
// elsewhere as it was, bit for bit. The world is built with no extra solid,
// with one in the yard's band, one in the 143 m deck's band, one in the crane
// band (484 to 640 m) and one after every body the world builds; each is a
// half-meter box 300 m east and south of the tower that nothing ever touches.
// Then a dynamic scenario at grade (the yard's forklift boarded and driven,
// with the yard's and the decks' loose stock settling) and one in a far band
// (the climber walking the 706 m deck as its stock settles) must end in the
// same state word for word in every world.
#include "sim/forklift.hpp"
#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using scraperx::sim::Forklift;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;

namespace {

struct World final {
    const char *name;
    std::vector<Simulation::TestExtraSolid> extras;
};

struct End final {
    std::vector<std::uint64_t> bits;
    scraperx::sim::Vector3 truck{};
    scraperx::sim::Vector3 player{};
    std::uint32_t solids = 0;
};

scraperx::sim::Vector3 body_at(const Simulation &s, const std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

// At grade: a second of settling, round the beam stack onto the yard's
// forklift's floor to its wheel, DRIVE, and two seconds forward.
End drive_at_grade(const World &world) {
    Simulation s(InitialSpawn::ExteriorGrade, world.extras);
    End end;
    end.solids = s.snapshot().world_solid_bodies;
    require(s.advance_frame(1.0).accepted, "body order: the settle interval must be accepted");
    require(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
                walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1),
            "body order: the climber reaches the yard's forklift");
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    require(s.forklift_state().at_controls && s.request_forklift_action(),
            "body order: at the wheel, DRIVE takes the controls");
    for (int tick = 0; tick < 2 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        (void)s.set_move_input(0.0, 1.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.0);
    end.bits = world_bits(s);
    end.truck = body_at(s, Forklift::kChassisEntity);
    end.player = s.snapshot().player_position;
    return end;
}

// In a far band: on the 706 m deck, three seconds walking east then west
// while the tower's stock settles about the climber.
End walk_high(const World &world) {
    Simulation s(InitialSpawn::Deck706, world.extras);
    End end;
    end.solids = s.snapshot().world_solid_bodies;
    require(s.advance_frame(1.0).accepted, "body order: the high settle interval must be accepted");
    for (int tick = 0; tick < 3 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        const double east = tick < static_cast<int>(1.5 * Simulation::kTickRateHz) ? 1.0 : -1.0;
        (void)s.set_facing(east, 0.0);
        (void)s.set_move_input(east, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.0);
    end.bits = world_bits(s);
    end.player = s.snapshot().player_position;
    return end;
}

// Where the two ends differ: the first differing word, or -1 for none.
long first_difference(const End &a, const End &b) {
    const auto differ = std::mismatch(a.bits.begin(), a.bits.end(), b.bits.begin());
    if (a.bits.size() != b.bits.size()) {
        return static_cast<long>(std::min(a.bits.size(), b.bits.size()));
    }
    return differ.first == a.bits.end() ? -1L : static_cast<long>(differ.first - a.bits.begin());
}

} // namespace

void run_body_order() {
    const std::vector<World> worlds = {
        {"as built", {}},
        {"a box in the yard's band", {{300.0, 0.5, 300.0, 0.5F, 0.5F, 0.5F, false}}},
        {"a box in the 143 m deck's band", {{300.0, 143.5, 300.0, 0.5F, 0.5F, 0.5F, false}}},
        {"a box in the crane band", {{300.0, 500.5, 300.0, 0.5F, 0.5F, 0.5F, false}}},
        {"a box after every body", {{300.0, 0.5, 300.0, 0.5F, 0.5F, 0.5F, true}}},
    };
    std::vector<End> grade;
    std::vector<End> high;
    for (const World &world : worlds) {
        grade.push_back(drive_at_grade(world));
        high.push_back(walk_high(world));
        const End &g = grade.back();
        std::cout << "  body order: " << world.name << ": " << g.solids << " solids; at grade the truck ends at ("
                  << g.truck.x << ", " << g.truck.y << ", " << g.truck.z << "), the climber at (" << g.player.x
                  << ", " << g.player.y << ", " << g.player.z << "); on the 706 m deck the climber ends at ("
                  << high.back().player.x << ", " << high.back().player.y << ", " << high.back().player.z << ")\n";
    }
    require(grade.front().bits.size() > 64 && high.front().bits.size() > 64,
            "body order: the compared state must hold the world's bodies");
    require(std::abs(grade.front().truck.z - Forklift::kParkZ) > 1.0,
            "body order: the truck must have been driven");
    bool same = true;
    for (std::size_t i = 1; i < worlds.size(); ++i) {
        require(grade[i].solids == grade.front().solids + 1 && high[i].solids == high.front().solids + 1,
                "body order: each world must build its one extra solid");
        const long at_grade = first_difference(grade.front(), grade[i]);
        const long at_height = first_difference(high.front(), high[i]);
        if (at_grade >= 0 || at_height >= 0) {
            same = false;
            std::cerr << "body order: with " << worlds[i].name << " the state differs: at grade from word " << at_grade
                      << " of " << grade.front().bits.size() << ", on the 706 m deck from word " << at_height << " of "
                      << high.front().bits.size() << " (-1: the same)\n";
        }
    }
    require(same, "body order: a body built in one band leaves every simulation elsewhere unchanged, bit for bit");
    std::cout << "PASS scraperx_sim body order: worlds=" << worlds.size() << " grade_words=" << grade.front().bits.size()
              << " high_words=" << high.front().bits.size() << " truck_x=" << grade.front().truck.x
              << " truck_z=" << grade.front().truck.z << '\n';
}
