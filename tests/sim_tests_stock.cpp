// Loose stock: the yard's containers, beams, plate, drums and pipes, and the
// Matron's deck's crates, are real bodies that lie where they were put,
// sleep there, and move when moved.
#include "sim/loose_stock.hpp"
#include "sim_tests.hpp"

#include <cmath>
#include <iostream>
#include <vector>

using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;

namespace {

scraperx::sim::Vector3 piece_at(const Simulation &s, const int piece) {
    return s.kit_body_position(s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(piece)));
}

bool asleep(const Simulation &s, const int piece) {
    // A sleeping body's velocity is set to exactly nothing; one awake on the
    // ground keeps the solver's last trace of gravity.
    const auto v =
        s.kit_body_velocity(s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(piece)));
    return v.x == 0.0 && v.y == 0.0 && v.z == 0.0;
}

int first_of(const LooseStock::Kind kind) {
    const auto &table = LooseStock::placements();
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].kind == kind) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

void run_loose_stock() {
    // Left alone, every piece settles where it was put (nothing stacked
    // slides off or rolls away, nothing starts inside anything) and sleeps:
    // the table's, and the tower's spots (LooseStock::dress_tower), wherever
    // the world let them be put.
    Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    const int pieces = static_cast<int>(s.loose_stock_count());
    const int tabled = static_cast<int>(LooseStock::placements().size());
    std::cout << "  stock: " << tabled << " pieces in the table, " << pieces - tabled << " in the tower's spots\n";
    require(pieces > tabled + 60, "stock: the tower's rings are dressed");
    std::vector<scraperx::sim::Vector3> placed;
    for (int i = 0; i < pieces; ++i) {
        require(s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(i)) != Simulation::kKitNone,
                "stock: every piece is a kit body");
        placed.push_back(piece_at(s, i));
    }
    require(s.advance_frame(12.0).accepted, "stock: the settling interval is accepted");
    double worst = 0.0;
    int worst_piece = -1;
    int awake = 0;
    for (int i = 0; i < pieces; ++i) {
        const auto now = piece_at(s, i);
        const double moved = std::hypot(now.x - placed[i].x, now.y - placed[i].y, now.z - placed[i].z);
        if (moved > worst) {
            worst = moved;
            worst_piece = i;
        }
        awake += asleep(s, i) ? 0 : 1;
    }
    std::cout << "  stock: " << pieces << " pieces settled within " << worst << " m (piece " << worst_piece
              << "), " << awake << " awake after 12 s\n";
    require(worst < 0.03, "stock: left alone, every piece lies where it was put");
    require(awake == 0, "stock: left alone, every piece sleeps");

    // A drum is carried: taken off its pallet, walked 3 m and set down, it
    // lies where it was set and sleeps again.
    const int drum = first_of(LooseStock::Kind::Drum);
    const auto drum0 = piece_at(s, drum);
    // Round the east end of the beam stack, between it and the plate.
    require(walk_to(s, 30.0, -10.0, 30.0) && walk_to(s, 43.5, 6.0, 20.0) && walk_to(s, 43.5, 21.0, 20.0) &&
                walk_to(s, drum0.x, drum0.z - 1.6, 20.0) &&
                walk_to(s, drum0.x, drum0.z - 0.75, 6.0, 0.08),
            "stock: the walk to the drums on their pallet");
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.3);
    require(s.snapshot().carry_target_entity_id == LooseStock::kFirstEntity + static_cast<std::uint64_t>(drum),
            "stock: the nearest drum is in reach");
    require(s.request_pick_up(), "stock: the drum's pick-up is accepted");
    (void)s.advance_frame(0.5);
    require(s.snapshot().carrying_entity_id == LooseStock::kFirstEntity + static_cast<std::uint64_t>(drum),
            "stock: the drum is in the hands");
    require(walk_to(s, drum0.x, drum0.z - 3.75, 8.0), "stock: walked back 3 m with the drum");
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.5);
    require(s.request_set_down(), "stock: the drum's set-down is accepted");
    require(walk_to(s, drum0.x + 2.0, drum0.z - 3.5, 6.0), "stock: stepped away from the drum");
    (void)s.advance_frame(6.0);
    const auto drum1 = piece_at(s, drum);
    std::cout << "  stock: the drum carried " << std::hypot(drum1.x - drum0.x, drum1.z - drum0.z) << " m, at y "
              << drum1.y << "\n";
    require(std::hypot(drum1.x - drum0.x, drum1.z - drum0.z) > 2.0 && drum1.y < drum0.y,
            "stock: the drum is set down off its pallet, on the ground");
    require(asleep(s, drum), "stock: set down and left, the drum sleeps again");

    // A container is a machine's load: walked into for 4 s, it does not move.
    int container = -1;
    const auto &table = LooseStock::placements();
    for (std::size_t i = 0; i < table.size(); ++i) {
        // The lone one standing across the row at x -54.5.
        if (table[i].kind == LooseStock::Kind::Container && table[i].x < -54.0F) {
            container = static_cast<int>(i);
        }
    }
    require(container >= 0, "stock: the yard's lone container");
    const auto box0 = piece_at(s, container);
    require(walk_to(s, box0.x + 4.0, box0.z, 60.0) && walk_to(s, box0.x + 1.6, box0.z, 6.0, 0.1),
            "stock: the walk to the lone container's side");
    for (int i = 0; i < 4 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        (void)s.set_move_input(-1.0, 0.0);
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    const auto box1 = piece_at(s, container);
    const double shoved = std::hypot(box1.x - box0.x, box1.z - box0.z);
    std::cout << "  stock: a container walked into for 4 s moved " << shoved << " m\n";
    require(shoved < 0.01, "stock: a 3.75 t container is not shoved by hand");
    std::cout << "PASS scraperx_sim stock: pieces=" << pieces << " settle_m=" << worst << " shoved_m=" << shoved << "\n";
}
