// Loose stock: the yard's containers, beams, plate, drums and pipes, and the
// Matron's deck's crates, are real bodies that lie where they were put,
// sleep there, and move when moved.
#include "sim/impacts.hpp"
#include "sim/loose_stock.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

using scraperx::sim::ImpactLedger;
using scraperx::sim::ImpactSound;
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

// The hits on `entity` after sequence `after`.
std::vector<scraperx::sim::ImpactSnapshot> hits_on(const Simulation &s, const std::uint64_t entity,
                                                   const std::uint64_t after) {
    std::vector<scraperx::sim::ImpactSnapshot> found;
    for (const auto &hit : s.impacts()) {
        if (hit.entity == entity && hit.sequence > after) {
            found.push_back(hit);
        }
    }
    return found;
}

std::uint64_t last_hit(const Simulation &s) {
    const auto hits = s.impacts();
    return hits.empty() ? 0 : hits.back().sequence;
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
    // The tower's spots end where the yard crane's steel begins (and the
    // machines' weights after it, and the Matron's throwables after them).
    const int towered =
        pieces - LooseStock::kCraneSteelPieces - LooseStock::kMachineWeights - LooseStock::kMatronThrowables;
    std::cout << "  stock: " << tabled << " pieces in the table, " << towered - tabled << " in the tower's spots\n";
    require(towered > tabled + 60, "stock: the tower's rings are dressed");
    // The owner's rules for what is up there (2026-10-17): every heavy piece
    // stands on a pallet or a skid, so a forklift's forks fit under it; and
    // what only the yard crane could have brought up stands only on the
    // decks it lands loads on (11, 22 and 44 m), each of which has some.
    {
        const std::array<double, 3> crane_decks{11.0, 22.0, 44.0};
        std::array<int, 3> crane_pieces{};
        int heavy = 0;
        for (int i = tabled; i < towered; ++i) {
            const auto kind = static_cast<LooseStock::Kind>(s.loose_stock_piece(static_cast<std::uint32_t>(i)).kind);
            if (!LooseStock::heavy(kind)) {
                continue;
            }
            ++heavy;
            require(LooseStock::forkable(kind), "stock: every heavy piece up the tower stands on a pallet or a skid");
            const bool crane_only = kind == LooseStock::Kind::CoilSkid || kind == LooseStock::Kind::PlateSkid ||
                                    kind == LooseStock::Kind::BottleCage;
            if (!crane_only) {
                continue;
            }
            const double floor = piece_at(s, i).y;
            bool on_crane_deck = false;
            for (std::size_t d = 0; d < crane_decks.size(); ++d) {
                if (std::abs(floor - crane_decks[d]) < 0.3) {
                    on_crane_deck = true;
                    ++crane_pieces[d];
                }
            }
            require(on_crane_deck, "stock: the crane's loads stand only on the decks the crane serves");
        }
        std::cout << "  stock: " << heavy << " heavy pieces up the tower, all forkable; the crane's loads on the 11, "
                  << "22 and 44 m decks: " << crane_pieces[0] << ", " << crane_pieces[1] << ", " << crane_pieces[2]
                  << "\n";
        require(crane_pieces[0] > 0 && crane_pieces[1] > 0 && crane_pieces[2] > 0,
                "stock: each deck the crane serves has the crane's loads");
    }
    std::vector<scraperx::sim::Vector3> placed;
    for (int i = 0; i < pieces; ++i) {
        require(s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(i)) != Simulation::kKitNone,
                "stock: every piece is a kit body");
        placed.push_back(piece_at(s, i));
    }
    // Tower pieces per dress_tower ring, before settling. Rings are the stack
    // rings 11..143 m and the upper rings 176.25..396.25 m, as in
    // LooseStock::dress_tower. The old total-only check (tower pieces > 60)
    // passes even if one ring is left bare, so each ring is gated here: a
    // piece's center sits under 3 m above its ring's floor (spot pieces stack
    // at most about 1.5 m up), and rings are at least 11 m apart, so the
    // band cannot cross-assign. Measured on trunk 403fb9ab: every ring
    // dressed, unassigned 0.
    {
        std::vector<double> rings;
        for (int level = 1; level <= 13; ++level) {
            rings.push_back(11.0 * static_cast<double>(level));
        }
        for (int k = 0; k <= 10; ++k) {
            rings.push_back(176.25 + 22.0 * static_cast<double>(k));
        }
        std::vector<int> per_ring(rings.size(), 0);
        int unassigned = 0;
        for (int i = tabled; i < towered; ++i) {
            const double y = placed[static_cast<std::size_t>(i)].y;
            bool hit = false;
            for (std::size_t r = 0; r < rings.size(); ++r) {
                if (y >= rings[r] && y < rings[r] + 3.0) {
                    ++per_ring[r];
                    hit = true;
                    break;
                }
            }
            if (!hit) {
                ++unassigned;
            }
        }
        std::cout << "  stock: tower pieces per ring:";
        for (std::size_t r = 0; r < rings.size(); ++r) {
            std::cout << " " << rings[r] << ":" << per_ring[r];
        }
        std::cout << " unassigned:" << unassigned << "\n";
        require(unassigned == 0, "stock: every tower piece sits within 3 m above a dress_tower ring");
        for (std::size_t r = 0; r < rings.size(); ++r) {
            require(per_ring[r] > 0, "stock: every dress_tower ring is dressed");
        }
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
    // Settling is not a hit: nothing is heard from stock left alone.
    std::cout << "  stock: " << s.impacts().size() << " hits heard while settling\n";
    require(s.impacts().empty(), "stock: left alone, nothing is struck");
    // How loud a hit is to the Matron, by its energy.
    require(ImpactLedger::loudness(ImpactLedger::kSilentJ) == 0.0F &&
                ImpactLedger::loudness(ImpactLedger::kFullJ) == 1.0F &&
                std::abs(ImpactLedger::loudness(500.0F) - 2.0F / 3.0F) < 1.0e-4F,
            "stock: a hit's loudness runs by the log of its energy, silent at 5 J, full at 5 kJ");

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
    const std::uint64_t drum_entity = LooseStock::kFirstEntity + static_cast<std::uint64_t>(drum);
    const std::uint64_t before_carry = last_hit(s);
    require(s.request_pick_up(), "stock: the drum's pick-up is accepted");
    (void)s.advance_frame(0.5);
    require(s.snapshot().carrying_entity_id == LooseStock::kFirstEntity + static_cast<std::uint64_t>(drum),
            "stock: the drum is in the hands");
    require(walk_to(s, drum0.x, drum0.z - 3.75, 8.0), "stock: walked back 3 m with the drum");
    (void)s.set_facing(0.0, -1.0);
    (void)s.advance_frame(0.5);
    require(hits_on(s, drum_entity, before_carry).empty(), "stock: a drum in the hands is not struck by the carry");
    const std::uint64_t before_set_down = last_hit(s);
    require(s.request_set_down(), "stock: the drum's set-down is accepted");
    require(walk_to(s, drum0.x + 2.0, drum0.z - 3.5, 6.0), "stock: stepped away from the drum");
    (void)s.advance_frame(6.0);
    const auto drum1 = piece_at(s, drum);
    std::cout << "  stock: the drum carried " << std::hypot(drum1.x - drum0.x, drum1.z - drum0.z) << " m, at y "
              << drum1.y << "\n";
    require(std::hypot(drum1.x - drum0.x, drum1.z - drum0.z) > 2.0 && drum1.y < drum0.y,
            "stock: the drum is set down off its pallet, on the ground");
    require(asleep(s, drum), "stock: set down and left, the drum sleeps again");
    // Let go from the hands, the drum drops to the ground: a hit, heard hollow.
    const auto drop = hits_on(s, drum_entity, before_set_down);
    require(!drop.empty(), "stock: the drum let down from the hands is heard landing");
    std::cout << "  stock: the drum landed with " << drop.front().energy_j << " J at " << drop.front().speed_mps
              << " m/s, " << drop.size() << " hits, loudness "
              << ImpactLedger::loudness(static_cast<float>(drop.front().energy_j)) << "\n";
    require(drop.front().sound == ImpactSound::Hollow, "stock: a drum sounds hollow");
    require(drop.front().energy_j > 5.0 && drop.front().energy_j < 1000.0,
            "stock: the drum's landing carries the energy of a drop from the hands");

    // A container is a machine's load: walked into for 4 s, it does not move.
    int container = -1;
    const auto &table = LooseStock::placements();
    for (std::size_t i = 0; i < table.size(); ++i) {
        // The lone one standing across the row at x -54.5 (in the yard, not
        // the crane's laydown west of the stack).
        if (table[i].kind == LooseStock::Kind::Container && table[i].x < -54.0F && table[i].z > 0.0F) {
            container = static_cast<int>(i);
        }
    }
    require(container >= 0, "stock: the yard's lone container");
    const auto box0 = piece_at(s, container);
    const std::uint64_t before_shove = last_hit(s);
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
    require(hits_on(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(container), before_shove).empty(),
            "stock: a container leaned on is not struck");
    std::cout << "PASS scraperx_sim stock: pieces=" << pieces << " settle_m=" << worst << " shoved_m=" << shoved << "\n";
}

// The eyes' sight line (Simulation::look_target, read by the presentation
// for what the player's aim rests on): from the 154 m deck a look at the
// Matron's oil drum meets the drum, by its entity; a look straight down meets
// the deck under the feet, not the player's own body; nonsense is refused.
void run_look_target() {
    Simulation s(scraperx::sim::InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto me = s.snapshot().player_position;
    const double eye_x = me.x;
    const double eye_y = me.y + 0.62;
    const double eye_z = me.z;
    // The Matron's throwables are the last pieces laid, the drum the last.
    const std::uint64_t drum = LooseStock::kFirstEntity + static_cast<std::uint64_t>(s.loose_stock_count()) - 1;
    const auto at = s.kit_body_position(s.kit_body_index(drum));
    Simulation::LookHit hit;
    require(s.look_target(eye_x, eye_y, eye_z, at.x - eye_x, at.y - eye_y, at.z - eye_z, 30.0, hit),
            "look: a look at the drum meets something");
    std::cout << "  look: at the drum meets entity " << hit.entity << " at " << hit.distance << " m\n";
    require(hit.entity == drum, "look: a look at the drum meets the drum");
    const double to_drum = std::sqrt((at.x - eye_x) * (at.x - eye_x) + (at.y - eye_y) * (at.y - eye_y) +
                                     (at.z - eye_z) * (at.z - eye_z));
    require(hit.distance > 0.0 && hit.distance < to_drum, "look: the drum is met on its near side");
    Simulation::LookHit down;
    require(s.look_target(eye_x, eye_y, eye_z, 0.0, -1.0, 0.0, 5.0, down), "look: a look down meets the deck");
    require(down.entity != Simulation::kPlayerEntityId, "look: the player's own body is seen through");
    require(std::abs(down.point.y - kDeck14Top) < 0.2 && down.normal.y > 0.9,
            "look: a look down meets the deck's top, facing up");
    Simulation::LookHit refused;
    require(!s.look_target(eye_x, eye_y, eye_z, 0.0, 0.0, 0.0, 5.0, refused), "look: no direction is refused");
    require(!s.look_target(eye_x, std::nan(""), eye_z, 0.0, -1.0, 0.0, 5.0, refused), "look: NaN is refused");
    require(!s.look_target(eye_x, eye_y, eye_z, 0.0, -1.0, 0.0, 2500.0, refused), "look: past 2 km is refused");
    std::cout << "PASS scraperx_sim look target: drum=" << drum << " drum_m=" << hit.distance
              << " deck_y=" << down.point.y << "\n";
}
