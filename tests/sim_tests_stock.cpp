// Loose stock: the yard's containers, beams, plate, drums and pipes, and the
// Matron's deck's crates, are real bodies that lie where they were put,
// sleep there, and move when moved.
#include "sim/forklift.hpp"
#include "sim/impacts.hpp"
#include "sim/loose_stock.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

using scraperx::sim::Forklift;
using scraperx::sim::ImpactLedger;
using scraperx::sim::ImpactSound;
using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

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

// The container at (x, z) in the table, or -1. By position, not by index,
// so a table reorder does not silently retarget the test.
int container_at(const float x, const float z) {
    const auto &table = LooseStock::placements();
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].kind == LooseStock::Kind::Container && std::abs(table[i].x - x) < 0.1F &&
            std::abs(table[i].z - z) < 0.1F) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// A forklift ram, the probe's own pattern
// (tools/probes/j038_impulse_probe.cpp): aboard at its park, the drums'
// pallet moved 40 m up its lane out of the way, the target stood in the
// lane at z 30, full stick until 0.2 s after contact, then let go.
struct RamHit {
    double hit_dv_mps = 0.0;    // the target's speed change over 0.2 s from contact
    double peak_tick_ns = 0.0;  // mass x its largest per-tick speed change
    double struck_mass_kg = 0.0;
};

bool clear_lane_and_board(Simulation &s) {
    if (!(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
          walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    if (!s.forklift_state().at_controls || !s.request_forklift_action()) {
        return false;
    }
    (void)s.advance_frame(0.2);
    if (!s.forklift_state().driving) {
        return false;
    }
    // The pallet of drums ahead of the truck (3 pallets + 12 drums), moved
    // as a group 40 m up the lane.
    const int pallet = first_of(LooseStock::Kind::Pallet);
    for (int i = pallet; i < pallet + 15; ++i) {
        const auto b = s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(i));
        auto p = s.kit_body_position(b);
        p.z += 40.0;
        s.debug_launch_kit_body(b, p, {0.0, 0.0, 0.0});
    }
    return true;
}

// `pace` stick into `target` (already staged in the lane): contact is the
// first tick its speed changes by more than 0.02 m/s; the hit is its speed
// change over 0.2 s from contact, the peak mass x its largest per-tick
// change — the probe's own watch, with a lower contact mark. The probe's
// 0.05 m/s mark is calibrated for the empty container: a creep knock on a
// loaded one moves it ~0.04 m/s in its hardest tick, under 0.05, so the
// knock reads 0 N.s and is missed outright (measured). A body at rest has
// exactly zero velocity, so 0.02 sits above no noise; the target is
// required asleep before the watch starts.
RamHit ram_target(Simulation &s, const std::uint32_t target, const double pace) {
    RamHit out;
    out.struck_mass_kg = s.kit_body_mass(target);
    const double dt = Simulation::kFixedStepSeconds;
    bool settled = false;
    for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        const Vector3 v = s.kit_body_velocity(target);
        if (v.x == 0.0 && v.y == 0.0 && v.z == 0.0) {
            settled = true;
            break;
        }
        (void)s.advance_frame(dt);
    }
    require(settled, "stock: the staged container settles before the ram");
    const int ticks = 20 * static_cast<int>(Simulation::kTickRateHz);
    const int hold_after = static_cast<int>(0.2 * Simulation::kTickRateHz);
    int contact = -1;
    Vector3 pre{};
    for (int t = 0; t < ticks; ++t) {
        const bool held = contact < 0 || t - contact < hold_after;
        (void)s.set_move_input(0.0, held ? pace : 0.0);
        const Vector3 v0 = s.kit_body_velocity(target);
        (void)s.advance_frame(dt);
        const Vector3 v1 = s.kit_body_velocity(target);
        const double dv_tick =
            std::hypot(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        if (contact < 0 && dv_tick > 0.02) {
            contact = t;
            pre = v0;
        }
        if (contact >= 0 && t - contact < static_cast<int>(Simulation::kTickRateHz)) {
            out.peak_tick_ns = std::max(out.peak_tick_ns, out.struck_mass_kg * dv_tick);
            if (t - contact < hold_after) {
                out.hit_dv_mps = std::max(
                    out.hit_dv_mps,
                    std::hypot(v1.x - pre.x, v1.y - pre.y, v1.z - pre.z));
            }
        }
        if (contact >= 0 && t - contact >= static_cast<int>(Simulation::kTickRateHz)) {
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    return out;
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
    // The tower's spots are two ranges (loose_stock.hpp): the pre-world-b
    // dress, which ends where the yard crane's steel begins, and the
    // east/west stock, which takes the end of the table after the Matron's
    // throwables. The crane steel, the machines' weights and the
    // throwables sit between them and are not tower spots; as one run the
    // tower range would end at dress_end plus the east/west count.
    const int dress_end = static_cast<int>(s.loose_stock_crane_steel_begin());
    const int east_west_begin = static_cast<int>(s.loose_stock_east_west_begin());
    // The east/west stock ends where J-042's two kinds by her band begin
    // (laid last of all, on her deck; not tower spots).
    const int east_west_end = static_cast<int>(s.loose_stock_matron_kinds_begin());
    // The tower's spots end where the yard crane's steel begins (and the
    // machines' weights after it, and the Matron's throwables after them).
    const int towered = dress_end + (east_west_end - east_west_begin);
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
        for (int i = tabled; i < east_west_end; ++i) {
            if (i >= dress_end && i < east_west_begin) {
                continue;
            }
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
        for (int i = tabled; i < east_west_end; ++i) {
            if (i >= dress_end && i < east_west_begin) {
                continue;
            }
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
    // Dress_tower side coverage after settling, where the pieces actually
    // lie in play. Rings as in LooseStock::dress_tower (stack 11..143 m,
    // upper 176.25..396.25 m); side by dominant axis from the ring centre
    // (0, -150), matching dress_tower's four out directions. The gate is
    // per band, not per ring: a ring holds at most 3 stack spots, so one
    // ring cannot cover four sides, but each band must dress every side.
    // Measured before the room-box fix: stack east/west were zero on all
    // 13 rings. The per-ring count gate is a separate, pending package
    // (fb-bot/expand-world-20261031, unlanded); this gate does not redo it.
    {
        std::vector<double> rings;
        for (int level = 1; level <= 13; ++level) rings.push_back(11.0 * static_cast<double>(level));
        for (int k = 0; k <= 10; ++k) rings.push_back(176.25 + 22.0 * static_cast<double>(k));
        std::vector<std::array<int, 4>> per_side(rings.size(), {0, 0, 0, 0});
        int unassigned = 0;
        for (int i = tabled; i < east_west_end; ++i) {
            if (i >= dress_end && i < east_west_begin) {
                continue;
            }
            const auto p = piece_at(s, i);
            int ring = -1;
            for (std::size_t r = 0; r < rings.size(); ++r) {
                if (p.y >= rings[r] - 0.1 && p.y < rings[r] + 3.0) { ring = static_cast<int>(r); break; }
            }
            if (ring < 0) { ++unassigned; continue; }
            const double dx = p.x, dz = p.z + 150.0;
            int side = 0;
            if (std::abs(dx) >= std::abs(dz)) side = dx > 0 ? 1 : 3;
            else side = dz > 0 ? 0 : 2;
            ++per_side[static_cast<std::size_t>(ring)][static_cast<std::size_t>(side)];
        }
        std::cout << "  stock probe: settled tower pieces per ring sides N/E/S/W (unassigned " << unassigned
                  << "):\n";
        for (std::size_t r = 0; r < rings.size(); ++r) {
            std::cout << "    " << rings[r] << ": " << per_side[r][0] << "/" << per_side[r][1] << "/"
                      << per_side[r][2] << "/" << per_side[r][3] << "\n";
        }
        for (const char *band : {"stack", "upper"}) {
            const bool is_stack = band[0] == 's';
            const std::size_t lo = is_stack ? 0 : 13, hi = is_stack ? 13 : rings.size();
            std::array<int, 4> tot{0, 0, 0, 0}, hit{0, 0, 0, 0};
            for (std::size_t r = lo; r < hi; ++r)
                for (int s = 0; s < 4; ++s) {
                    tot[static_cast<std::size_t>(s)] += per_side[r][static_cast<std::size_t>(s)];
                    if (per_side[r][static_cast<std::size_t>(s)] > 0) ++hit[static_cast<std::size_t>(s)];
                }
            std::cout << "  stock probe: " << band << " side totals N/E/S/W " << tot[0] << "/" << tot[1] << "/"
                      << tot[2] << "/" << tot[3] << ", rings hit " << hit[0] << "/" << hit[1] << "/" << hit[2]
                      << "/" << hit[3] << " of " << (hi - lo) << "\n";
            for (int s = 0; s < 4; ++s) {
                require(tot[static_cast<std::size_t>(s)] > 0,
                        is_stack ? "stock: every side of the stack band is dressed"
                                 : "stock: every side of the upper band is dressed");
            }
        }
    }
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

    // ---- J-038 slice 0: the stated-load column ------------------------------
    // Every container in the table carries its stated load on top of the
    // 3,750 kg empty 40 ft spec. Three ground containers, set down by road
    // where a truck could have left them, carry 10,000 kg each (13,750 kg
    // in all); the stacked tier-1s and the crane's laydown stay empty,
    // because the yard crane's 75 kN hoist could not have put a loaded box
    // there. The probe's own containers ((39, 70), (1.5, 70) and the
    // (-76, -113) laydown) stay empty, so its numbers reproduce.
    {
        int containers = 0, loaded = 0;
        for (std::size_t i = 0; i < table.size(); ++i) {
            if (table[i].kind != LooseStock::Kind::Container) {
                continue;
            }
            ++containers;
            const auto body = s.kit_body_index(LooseStock::kFirstEntity + i);
            const double mass = s.kit_body_mass(body);
            const double expect = 3750.0 + static_cast<double>(table[i].load_kg);
            std::cout << "  stock: container " << i << " at (" << table[i].x << ", " << table[i].z << ") load "
                      << table[i].load_kg << " kg, body " << mass << " kg\n";
            require(std::abs(mass - expect) < 1.0, "stock: a container's body carries its stated load");
            if (table[i].load_kg > 0.0F) {
                ++loaded;
            }
        }
        require(containers == 18, "stock: eighteen containers in the table");
        require(loaded == 3, "stock: three containers carry a stated load");
    }

    // ---- J-038 slice 0: a full ram barely moves a loaded container ---------
    // The loaded container at (14, 70), stood in the forklift's lane, rammed
    // at creep (0.25) and at full stick, each in a fresh world, the probe's
    // own cases. The probe's empty container takes a full ram at 1.63 m/s
    // over 0.2 s and a creep knock at 587 N.s peak; a loaded one must take
    // measurably less motion, while still moving — the ram is not a no-op —
    // and both peaks are printed for the slice-1 band re-check.
    {
        const auto ram_loaded = [](const double pace) {
            Simulation r(scraperx::sim::InitialSpawn::ExteriorGrade);
            (void)r.advance_frame(1.0);
            require(clear_lane_and_board(r), "stock: aboard the forklift for the loaded ram");
            const int target_piece = container_at(14.0F, 70.0F);
            require(target_piece >= 0, "stock: the loaded container at (14, 70)");
            const auto target =
                r.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(target_piece));
            r.debug_launch_kit_body(target, {38.0, 1.31, 30.0}, {0.0, 0.0, 0.0});
            (void)r.advance_frame(1.5);
            return ram_target(r, target, pace);
        };
        const RamHit creep = ram_loaded(0.25);
        const RamHit full = ram_loaded(1.0);
        std::cout << "  stock: creep knock on the loaded container: mass " << creep.struck_mass_kg << " kg, peak "
                  << creep.peak_tick_ns << " N.s, hit dv " << creep.hit_dv_mps << " m/s (empty peak: 587 N.s)\n";
        std::cout << "  stock: full ram into the loaded container: mass " << full.struck_mass_kg << " kg, peak "
                  << full.peak_tick_ns << " N.s, hit dv " << full.hit_dv_mps << " m/s (empty: 1.63 m/s)\n";
        require(full.hit_dv_mps > 0.2, "stock: a full ram still moves a loaded container");
        require(full.hit_dv_mps < 1.0, "stock: a full ram barely moves a loaded container");
        // Slice-1 band pins (measured 2026-11-05: creep 308.6, full 3147.4):
        // the creep stays in the shallow band (250-700) and the full ram in
        // the severe band (>= 2500, below the 5000 depth cap), so the slice-1
        // thresholds hold for a loaded container unchanged.
        require(creep.peak_tick_ns > 250.0, "stock: a creep knock on a loaded container is no mere touch");
        require(creep.peak_tick_ns < 700.0, "stock: a creep knock on a loaded container stays shallow");
        require(full.peak_tick_ns >= 2500.0, "stock: a full ram into a loaded container is severe");
        require(full.peak_tick_ns < 5000.0, "stock: a full ram into a loaded container stays under the depth cap");
    }

    // ---- J-038 slice 0: a stacked container rests and sleeps after a shove -
    // The (-42, 8) stack, base loaded, top empty, staged in the forklift's
    // lane and rammed at full stick in its base. After the shove and a
    // re-settle, the top still rides its base, both sleep, and nothing
    // else in the world moved: no drift, no settle explosion.
    {
        Simulation r(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)r.advance_frame(1.0);
        require(clear_lane_and_board(r), "stock: aboard the forklift for the stack shove");
        const int base_piece = container_at(-42.0F, 8.0F);
        require(base_piece >= 0, "stock: the (-42, 8) stack's base");
        const auto base = r.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(base_piece));
        // Its top: the tier-1 container over the same (x, z).
        int top_piece = -1;
        for (std::size_t i = 0; i < table.size(); ++i) {
            if (table[i].kind == LooseStock::Kind::Container && std::abs(table[i].x + 42.0F) < 0.1F &&
                std::abs(table[i].z - 8.0F) < 0.1F && table[i].y > 1.0F) {
                top_piece = static_cast<int>(i);
            }
        }
        require(top_piece >= 0, "stock: the (-42, 8) stack's top");
        const auto top = r.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(top_piece));
        r.debug_launch_kit_body(base, {38.0, 1.31, 30.0}, {0.0, 0.0, 0.0});
        r.debug_launch_kit_body(top, {38.0, 3.90, 30.0}, {0.0, 0.0, 0.0});
        (void)r.advance_frame(1.5);
        std::vector<scraperx::sim::Vector3> before;
        for (int i = 0; i < static_cast<int>(r.loose_stock_count()); ++i) {
            before.push_back(r.kit_body_position(
                r.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(i))));
        }
        const auto top_before = r.kit_body_position(top);
        const RamHit hit = ram_target(r, base, 1.0);
        std::cout << "  stock: full ram into the stack's loaded base: peak " << hit.peak_tick_ns
                  << " N.s, hit dv " << hit.hit_dv_mps << " m/s\n";
        // Slice-1 band note (measured 5998.3): a full ram into a loaded base
        // carrying its top reads severe and past the 5000 depth cap, so its
        // dent depth saturates at the cap; the band edge itself still holds.
        require(hit.peak_tick_ns >= 2500.0, "stock: a full ram into a loaded stack base is severe");
        // The machine backs away, as in play, so the stack is left to
        // settle on its own and not held awake against the forks.
        for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            (void)r.set_move_input(0.0, -0.4);
            (void)r.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)r.set_move_input(0.0, 0.0);
        (void)r.advance_frame(12.0);
        const auto top_after = r.kit_body_position(top);
        const double top_shift = std::hypot(top_after.x - top_before.x, top_after.z - top_before.z);
        const auto base_v = r.kit_body_velocity(base);
        const auto top_v = r.kit_body_velocity(top);
        std::cout << "  stock: the stacked top shifted " << top_shift << " m, y " << top_before.y << " -> "
                  << top_after.y << "; base v (" << base_v.x << ", " << base_v.y << ", " << base_v.z
                  << "), top v (" << top_v.x << ", " << top_v.y << ", " << top_v.z << ")\n";
        require(top_shift < 1.0, "stock: the stacked top rides its base, it does not slide off");
        require(std::abs(top_after.y - top_before.y) < 0.15, "stock: the stacked top stays stacked");
        require(asleep(r, base_piece) && asleep(r, top_piece), "stock: the shoved stack sleeps again");
        double worst_other = 0.0;
        for (int i = 0; i < static_cast<int>(r.loose_stock_count()); ++i) {
            if (i == base_piece || i == top_piece) {
                continue;
            }
            const auto now = r.kit_body_position(
                r.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(i)));
            worst_other = std::max(worst_other, std::hypot(now.x - before[static_cast<std::size_t>(i)].x,
                                                           now.y - before[static_cast<std::size_t>(i)].y,
                                                           now.z - before[static_cast<std::size_t>(i)].z));
        }
        std::cout << "  stock: nothing else moved more than " << worst_other << " m in the shove\n";
        require(worst_other < 0.05, "stock: the stack shove moves nothing else");
    }

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
    // The Matron's throwables are laid before the east/west stock, the
    // drum the last of them.
    const std::uint64_t drum =
        LooseStock::kFirstEntity + static_cast<std::uint64_t>(s.loose_stock_throwables_begin()) + 2;
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
