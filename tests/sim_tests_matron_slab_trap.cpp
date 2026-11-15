// J-092 (#410), step 2: the slab trap as built (src/sim/slab_cell.hpp), the
// whole sequence on the production 90 Hz path. He stands at the cord west of
// the cell, its handle in his hands; she comes out east of her locker after
// him along her deck's north band. Her stalk is her own (matron_stalk.cpp):
// under his gaze she freezes; looked away from, she closes to 3 m and holds
// there, facing him. He steps back with the handle: the cord lifts the
// trigger, the pelican hook opens, and from there nothing touches the slab or
// her. Gravity gives it its speed; her balance (Matron::take_blow) judges
// what it gave up. Her death stays at exactly 6000 N s (Matron::kKillDamageNs).
//
// Number first (before the run): the slab's underside falls from 174.45 to
// the top of her body (her hunting crouch, 1.61 m tall), 18.8 m: in free fall
// 19.2 m/s, about 18.5 m/s with Jolt's 0.05/s damping (the probe, #410:
// 18.57 m/s), 2.0 s. The probe's one-step blow on her standing was
// 24,203 N s: expect the same order, 4x her kill line.
//
// Both start alike: he watches her come out, and she freezes far off along
// the band. The miss: he steps back then, and it lands between them. The
// hit: he looks away, she closes and holds under the slab; he looks back at
// her (she freezes) and steps back. The two are the same tick for tick
// until he looks away.
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
#include "sim/slab_cell.hpp"
#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::MatronSnapshot;
using scraperx::sim::SlabCell;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kDeck = 154.0;
constexpr double kSlabKg = 2000.0;
constexpr double kSlabHalfY = 0.15;
constexpr double kSlabHalfX = 0.6;   // laid long side along z
constexpr double kSlabHalfZ = 1.2;
constexpr int kHz = static_cast<int>(Simulation::kTickRateHz);
// She comes out east of her locker, along the band toward him.
constexpr double kHerStartX = 15.0;
constexpr double kHerStartZ = -127.3;

bool trace() { return std::getenv("SCRAPERX_SLAB_TRACE") != nullptr; }

void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "slab-trap: tick accepted"); }

std::uint32_t slab_of(const Simulation &s) {
    // The cell's slab: the loose-stock table's last piece.
    const std::uint32_t last = s.loose_stock_count() - 1;
    require(s.loose_stock_piece(last).kind == static_cast<std::uint8_t>(LooseStock::Kind::PlateSlab),
            "slab-trap: the table's last piece is the cell's plate slab");
    return s.kit_body_index(LooseStock::kFirstEntity + last);
}

double body_top(const MatronSnapshot &m) { return std::max(m.shape_a.y, m.shape_b.y) + m.shape_r; }
double body_bottom(const MatronSnapshot &m) { return std::min(m.shape_a.y, m.shape_b.y) - m.shape_r; }

// One tick of him holding the handle: facing her (east, the cell) or away
// (north), still or stepping back west from the cell.
void hold(Simulation &s, const bool watching, const bool back) {
    (void)s.set_move_input(back ? -0.7 : 0.0, 0.0);
    (void)s.set_facing(watching ? 1.0 : 0.0, watching ? 0.0 : 1.0);
    tick(s);
}

// A fresh world: him on her deck at the cord, the handle in his hands, and
// her out east of her locker, hunting him.
std::unique_ptr<Simulation> ready() {
    auto s = std::make_unique<Simulation>(InitialSpawn::Deck154);
    s->place_matron_home(static_cast<float>(kDeck));
    for (int i = 0; i < 30; ++i) tick(*s);
    s->debug_launch_player({SlabCell::kHandleX - 0.7, kDeck + 0.9, SlabCell::kHandleZ}, {0.0, 0.0, 0.0});
    for (int i = 0; i < kHz / 2; ++i) hold(*s, true, false);
    require(s->snapshot().carry_target_entity_id == SlabCell::kHandleEntity, "slab-trap: the cord's handle is his to take");
    (void)s->request_pick_up();
    for (int i = 0; i < kHz / 3; ++i) hold(*s, true, false);
    require(s->snapshot().carrying_entity_id == SlabCell::kHandleEntity, "slab-trap: the handle is in his hands");
    s->launch_matron_for_test({kHerStartX, kDeck + static_cast<double>(Matron::kStandCentre), kHerStartZ});
    return s;
}

struct Drop {
    bool held_still = true;      // the slab did not move till the hook opened
    bool his_hands = false;      // the handle was in his hands as it opened
    double her_x = 0.0, her_z = 0.0;   // where she stood as he stepped back
    double open_s = 0.0;         // from his first step back to the hook opening
    double fall_s = 0.0;         // from the hook opening to the slab meeting her or the deck
    double free_fall_mps = 0.0;
    double entry_mps = 0.0;      // its speed down at the step it first touched her
    double first_tick_ns = 0.0;  // its change on that step (less gravity's) x its mass, as her balance read it
    double added_ns = 0.0;
    int contact_ticks = 0;
    int judged_ticks = 0;
    int missed_ticks = 0;
    double worst_overlap_m = 0.0;
    bool passed_through = false;
    double off_x = 0.0, off_z = 0.0;   // where it lay, less where it hung
    double slab_end_y = 0.0, slab_end_mps = 0.0;
    double her_lowest_m = 1.0e9;
    double her_max_up_mps = 0.0;
    double clear_m = -1.0;
    bool dead = false;
    int state = 0;
};

// He watches her till she stands frozen. With `lure` he looks away till she
// has closed and holds, then back at her. Once she has stood still half a
// second under his gaze, he steps back with the handle.
Drop drop(const bool lure) {
    const auto sim = ready();
    Simulation &s = *sim;
    const std::uint32_t slab = slab_of(s);
    const Vector3 hung = s.kit_body_position(slab);
    const double damage0 = s.matron_state().damage_ns;
    const int down = static_cast<int>(Matron::State::Down);
    const int dead = static_cast<int>(Matron::State::Dead);
    const int hunting = static_cast<int>(Matron::State::Hunting);
    Drop out;
    // He watches her first (she stalks only one who has seen her, and takes
    // one who has not); with the lure he then looks away, and back.
    bool watching = true;
    int looks = lure ? 2 : 0;
    int still = 0;
    int back_at = -1;
    int open_at = -1;
    int hit_at = -1;
    int landed_at = -1;
    for (int i = 0; i < 40 * kHz; ++i) {
        const bool back = back_at >= 0 && open_at < 0;
        const auto before = s.matron_state();
        const double fall_v = s.kit_body_velocity(slab).y;
        const bool reading = before.state != down && before.state != dead;
        hold(s, watching, back);
        const auto m = s.matron_state();
        const Vector3 p = s.kit_body_position(slab);
        const Vector3 v = s.kit_body_velocity(slab);
        if (trace() && i % 45 == 0) {
            const auto him = s.snapshot().player_position;
            std::cout << "  t" << i << " her (" << m.position.x << "," << m.position.z << ") v " << m.speed << " state "
                      << static_cast<int>(m.state) << " him (" << him.x << "," << him.z << ") watching " << watching
                      << " slab y " << p.y << "\n";
        }
        if (back_at < 0) {
            still = m.state == hunting && m.speed < 0.05 ? still + 1 : 0;
            if (looks > 0 && still >= kHz / 2) {
                watching = !watching;   // she stands: he looks away, or back at her
                --looks;
                still = 0;
            } else if (still >= kHz / 2) {
                back_at = i;
                out.her_x = m.position.x;
                out.her_z = m.position.z;
            }
        }
        if (open_at < 0) {
            if (v.y < -0.05) {
                open_at = i;
                out.open_s = static_cast<double>(i - back_at) / kHz;
                out.his_hands = s.snapshot().carrying_entity_id == SlabCell::kHandleEntity;
                out.free_fall_mps = std::sqrt(2.0 * 9.81 * std::max(0.0, (hung.y - kSlabHalfY) - body_top(m)));
            } else if (std::hypot(p.x - hung.x, p.y - hung.y, p.z - hung.z) > 1.0e-3) {
                out.held_still = false;
            }
        }
        const auto k = s.debug_matron_strikers();
        bool contact = false;
        bool judged = false;
        double given_ns = 0.0;
        for (const auto &striker : k.kept) {
            if (striker.kg > 0.5 * kSlabKg) {
                contact = contact || striker.contact;
                if (striker.judged) {
                    judged = true;
                    given_ns = striker.change_mps * striker.kg;
                }
            }
        }
        if (contact && hit_at < 0 && landed_at < 0) {
            hit_at = i;
            out.fall_s = static_cast<double>(i - open_at) / kHz;
            out.entry_mps = -fall_v;
            out.first_tick_ns = given_ns;
        }
        if (reading && landed_at < 0 && open_at >= 0) {
            out.contact_ticks += contact ? 1 : 0;
            out.judged_ticks += judged ? 1 : 0;
            out.missed_ticks += static_cast<int>(k.touching_missed.size());
        }
        const bool over = std::abs(p.x - m.position.x) < kSlabHalfX + m.shape_r &&
                          std::abs(p.z - m.position.z) < kSlabHalfZ + m.shape_r;
        if (reading && over && fall_v < -1.0) {
            out.worst_overlap_m = std::max(out.worst_overlap_m, body_top(before) - (p.y - kSlabHalfY));
        }
        if (over && fall_v < -1.0 && p.y < std::min(m.shape_a.y, m.shape_b.y)) {
            out.passed_through = true;
        }
        if (hit_at >= 0) {
            out.her_lowest_m = std::min(out.her_lowest_m, body_bottom(m) - kDeck);
            out.her_max_up_mps = std::max(out.her_max_up_mps, (m.position.y - before.position.y) * static_cast<double>(kHz));
        }
        if (open_at >= 0 && landed_at < 0 && hit_at < 0 && p.y - kSlabHalfY < kDeck + 0.05 && std::abs(v.y) < 0.5) {
            landed_at = i;
            out.fall_s = static_cast<double>(i - open_at) / kHz;
            const double dx = std::max(0.0, std::abs(m.position.x - p.x) - kSlabHalfX);
            const double dz = std::max(0.0, std::abs(m.position.z - p.z) - kSlabHalfZ);
            out.clear_m = std::hypot(dx, dz) - m.shape_r;
        }
        // The hit: two seconds on, for what the solver does with her under
        // it. A miss: half a second on the deck.
        if ((hit_at >= 0 && i - hit_at >= 2 * kHz) || (landed_at >= 0 && i - landed_at >= kHz / 2)) {
            break;
        }
    }
    require(back_at >= 0, "slab-trap: she stood still under his gaze, and he stepped back");
    require(open_at >= 0, "slab-trap: the hook opened");
    const auto m = s.matron_state();
    const Vector3 p = s.kit_body_position(slab);
    const Vector3 v = s.kit_body_velocity(slab);
    out.added_ns = m.damage_ns - damage0;
    out.off_x = p.x - hung.x;
    out.off_z = p.z - hung.z;
    out.slab_end_y = p.y;
    out.slab_end_mps = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    out.dead = !m.alive;
    out.state = m.state;
    return out;
}

void print(const char *name, const Drop &d) {
    std::cout << "slab-trap " << name << ": she stood at (" << d.her_x << ", " << d.her_z << "); held still "
              << d.held_still << ", in his hands " << d.his_hands << "; hook open " << d.open_s << " s into his step back, fell "
              << d.fall_s << " s; entry " << d.entry_mps << " m/s (free fall " << d.free_fall_mps << "); damage added "
              << d.added_ns << " N s (first step " << d.first_tick_ns << ", kill at " << Matron::kKillDamageNs
              << "); contact steps " << d.contact_ticks << ", judged " << d.judged_ticks << ", missed "
              << d.missed_ticks << "; overlap " << d.worst_overlap_m << " m, through " << d.passed_through
              << "; lay off its hang by (" << d.off_x << ", " << d.off_z << ") m at y " << d.slab_end_y
              << " moving " << d.slab_end_mps << " m/s; her bottom at least " << d.her_lowest_m
              << " m over the deck, rise at most " << d.her_max_up_mps << " m/s; clear " << d.clear_m << " m; dead "
              << d.dead << " state " << d.state << "\n";
}
}  // namespace

void run_matron_slab_trap() {
    const Drop hit = drop(true);
    print("hit", hit);
    require(std::abs(hit.her_x - static_cast<double>(SlabCell::kSlabX)) < kSlabHalfX &&
                std::abs(hit.her_z - static_cast<double>(SlabCell::kSlabZ)) < kSlabHalfZ,
            "slab-trap: looked away from, she closed and held under the slab");
    require(hit.held_still && hit.his_hands, "slab-trap: it hung still till the hook opened, the handle in his hands");
    require(hit.entry_mps > 0.95 * hit.free_fall_mps && hit.entry_mps <= hit.free_fall_mps,
            "slab-trap: gravity alone gave the slab its speed");
    require(hit.judged_ticks >= 1 && hit.missed_ticks == 0, "slab-trap: her balance judged the hit, and missed nothing");
    require(hit.first_tick_ns >= static_cast<double>(Matron::kKillDamageNs) && std::abs(hit.added_ns - hit.first_tick_ns) < 1.0,
            "slab-trap: one step of contact took her damage past 6000 N s");
    require(hit.dead && hit.state == static_cast<int>(Matron::State::Dead), "slab-trap: the hit killed her");
    require(!hit.passed_through && hit.worst_overlap_m < 0.25 && hit.her_lowest_m > -0.15,
            "slab-trap: no tunnelling, into her or the deck");
    require(hit.slab_end_mps < 0.2 && hit.slab_end_y > kDeck + kSlabHalfY - 0.05 && hit.her_max_up_mps < 3.0,
            "slab-trap: the slab came to rest on her, the solver kicking nothing away");
    std::cout << "PASS matron-slab-trap hit: " << hit.added_ns << " N s from one step at " << hit.entry_mps << " m/s, dead\n";

    const Drop miss = drop(false);
    print("miss", miss);
    require(miss.held_still && miss.his_hands, "slab-trap: it hung still till he opened the hook");
    require(miss.contact_ticks == 0 && miss.judged_ticks == 0 && miss.clear_m > 0.0,
            "slab-trap: watched from the start, she froze short of it and it landed in front of her");
    require(miss.added_ns == 0.0 && !miss.dead, "slab-trap: a miss adds no damage");
    require(std::abs(miss.off_x) < 0.05 && std::abs(miss.off_z) < 0.05, "slab-trap: it came straight down where it hung");
    std::cout << "PASS matron-slab-trap miss: clear by " << miss.clear_m << " m, 0 N s\n";
}
SCRAPERX_SLICE("matron-slab-trap", run_matron_slab_trap);
