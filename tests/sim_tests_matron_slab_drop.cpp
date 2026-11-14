// J-092 (#410), step 1: the probe (Cory, 11-11: "yes, test first"). The yard's
// 2,000 kg plate slab (LooseStock::PlateSlab) let go from rest at the highest
// drop a Deck 154 service cell could have, onto the Matron standing still, and
// again with her walking out from under it, on the production 90 Hz path.
// Where the slab starts and where she stands are set; once it is let go
// nothing touches either of them: gravity gives the slab its speed, Jolt meets
// it with her, and her balance (Matron::take_blow) judges what it gave up.
// Her death stays at exactly 6000 N s (Matron::kKillDamageNs).
//
// The highest drop: the cell hangs the slab from the 176 ring's north band
// over her lane (world_solids.inc, UpperDeck: its underside at 175.75), a hook
// block and sling taking kHoistM under that steel. The slab's underside is
// then 18.66 m over the top of her body (her hunting crouch, 1.61 m tall).
// Measured (11-11): it meets her at 18.57 m/s, one step of contact adds
// 24,203 N s to her damage, and she is dead; walking, she is 1.6 m clear.
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
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
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kDeck = 154.0;
constexpr double kRingUnderside = 176.0 - 0.25;   // the 176 ring's north band, its underside
constexpr double kHoistM = 1.0;                    // hook block and sling, the slab's top under the ring
constexpr double kSlabHalfY = 0.15;                // LooseStock::PlateSlab, 2.4 x 0.3 x 1.2 m
constexpr double kSlabHalfX = 1.2;
constexpr double kSlabHalfZ = 0.6;
constexpr double kSlabKg = 2000.0;
constexpr double kReleaseY = kRingUnderside - kHoistM - kSlabHalfY;   // its centre, let go
// Her lane under the ring's north band (z -124.91 .. -128.91), east of the
// bay's cut (x -3.9 .. -0.4) and west of her locker (x 11), on her lane's
// line (Matron::kBandNorthZ). North of the slab cell's own hung slab
// (slab_cell.hpp: z -127.6 .. -125.2 at that height) and its cord
// (z -125.5): the yard's slab is let go clear of both, not inside the cell's.
constexpr double kSpotX = 6.0;
constexpr double kSpotZ = -128.5;
constexpr int kHz = static_cast<int>(Simulation::kTickRateHz);

bool trace() { return std::getenv("SCRAPERX_SLAB_TRACE") != nullptr; }

void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "slab-drop: tick accepted"); }

// The yard's plate slab: the first PlateSlab in the loose-stock table.
std::uint32_t plate_slab(const Simulation &s) {
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::PlateSlab)) {
            return s.kit_body_index(LooseStock::kFirstEntity + i);
        }
    }
    require(false, "slab-drop: the world has its plate slab");
    return 0;
}

struct Drop {
    double fell_m = 0.0;          // its underside's fall, release to the top of her body
    double free_fall_mps = 0.0;   // sqrt(2 g h) over that fall, no damping
    double entry_mps = 0.0;       // the slab's speed down at the step it first touched her
    double her_speed_mps = -1.0;  // her speed then
    double off_spot_m = 0.0;      // and how far she then stood off where it was let go over
    double first_tick_ns = 0.0;   // its change on that step (less gravity's) x its mass, as her balance read it
    double added_ns = 0.0;        // damage_ns after, less before
    int contact_ticks = 0;        // steps in contact with her while her balance read blows (on her feet)
    int judged_ticks = 0;         // of them, the ones it judged a blow
    int missed_ticks = 0;         // a body touching her her balance did not keep, while it read
    double worst_overlap_m = 0.0; // its underside into the top of her body, on her feet, as it came down
    bool passed_through = false;  // its middle under the lowest of her body's axis, still falling: tunnelled
    double slab_end_y = 0.0;      // its centre at the end
    double slab_end_mps = 0.0;    // its speed at the end
    double her_lowest_m = 1.0e9;  // the bottom of her body over the deck, at its lowest
    double her_max_up_mps = 0.0;  // her fastest rise (a solver's kick)
    double clear_m = -1.0;        // a miss: her body's nearest to the slab's footprint as it landed
    bool dead = false;
    int state = 0;
};

double body_top(const MatronSnapshot &m) {
    return std::max(m.shape_a.y, m.shape_b.y) + m.shape_r;
}
double body_bottom(const MatronSnapshot &m) {
    return std::min(m.shape_a.y, m.shape_b.y) - m.shape_r;
}

// A fresh world: she out at the spot, hunting, the player where he spawned
// on the band's west, 16.5 m off. `walk`: she stalks to him (Pace::Stalk,
// 2 m/s) and is up to that pace. Otherwise the player is put down in the
// yard, out of her reach, and she is left to it.
std::unique_ptr<Simulation> ready(const bool walk) {
    auto s = std::make_unique<Simulation>(InitialSpawn::Deck154);
    s->place_matron_home(static_cast<float>(kDeck));
    for (int i = 0; i < 30; ++i) tick(*s);
    s->launch_matron_for_test({kSpotX, kDeck + static_cast<double>(Matron::kStandCentre), kSpotZ});
    for (int i = 0; i < 5 * kHz && s->matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) tick(*s);
    require(s->matron_state().state == static_cast<int>(Matron::State::Hunting), "slab-drop: she is out and hunting");
    if (walk) {
        s->debug_fix_matron_pace(static_cast<int>(Matron::Pace::Stalk));
        for (int i = 0; i < kHz && s->matron_state().speed < 0.9 * static_cast<double>(Matron::kStalkMps); ++i) tick(*s);
        require(s->matron_state().speed >= 0.9 * static_cast<double>(Matron::kStalkMps), "slab-drop: she is walking");
    } else {
        s->debug_launch_player({30.0, 1.0, 24.0}, {0.0, 0.0, 0.0});
    }
    return s;
}

// `walk` false: the slab let go over where she freezes under his gaze.
// `walk` true: let go over her middle as she walks, at her stalking pace.
Drop drop(const bool walk) {
    const auto sim = ready(walk);
    Simulation &s = *sim;
    // Face him at her, from wherever he stands: his gaze is what freezes her.
    const auto face_her = [&] {
        const auto m = s.matron_state();
        const auto p = s.snapshot().player_position;
        const double dx = m.position.x - p.x;
        const double dz = m.position.z - p.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            (void)s.set_facing(dx / len, dz / len);
        }
    };
    Vector3 her0;
    double top = 0.0;
    if (!walk) {
        // Frozen under his gaze: the player on deck 154 a few metres off,
        // facing her; she notices him, stalks, and freezes (the stalk's
        // freeze, walk_toward speed 0), which the dodge does not break
        // (J-092's trap: frozen under the slab, she takes it). The slab is
        // let go over where she froze, and lands while she is still frozen
        // (patience 20 s, fall ~1.95 s).
        s.debug_launch_player({9.0, 154.3, -125.5}, {0.0, 0.0, 0.0});
        int frozen_ticks = 0;
        bool frozen = false;
        for (int i = 0; i < 15 * kHz && !frozen; ++i) {
            face_her();
            tick(s);
            const auto m = s.matron_state();
            if (m.speed < 0.01 && m.state == static_cast<int>(Matron::State::Hunting)) {
                if (++frozen_ticks >= kHz) {
                    her0 = m.position;
                    top = body_top(m);
                    frozen = true;
                }
            } else {
                frozen_ticks = 0;
            }
        }
        require(frozen, "slab-drop: she freezes under his gaze");
    } else {
        her0 = s.matron_state().position;
        top = body_top(s.matron_state());
    }
    const double damage0 = s.matron_state().damage_ns;
    Drop out;
    out.fell_m = (kReleaseY - kSlabHalfY) - top;
    out.free_fall_mps = std::sqrt(2.0 * 9.81 * out.fell_m);
    const std::uint32_t slab = plate_slab(s);
    // The slab drifts as it falls (~+0.16 m x, +0.30 m z over the drop, measured):
    // let it go that far short of her, so it lands centred on her instead of
    // shoving her sideways off-centre (which read 0.1 m/s on her at contact).
    const double release_x = walk ? her0.x : her0.x - 0.16;
    const double release_z = walk ? her0.z : her0.z - 0.30;
    s.debug_launch_kit_body(slab, {release_x, kReleaseY, release_z}, {0.0, 0.0, 0.0});
    const int down = static_cast<int>(Matron::State::Down);
    const int dead = static_cast<int>(Matron::State::Dead);
    int hit_at = -1;
    int landed_at = -1;
    for (int i = 0; i < 6 * kHz; ++i) {
        // Hold his gaze on her, so the freeze holds while the slab falls.
        if (!walk) {
            face_her();
        }
        const auto before = s.matron_state();
        const double fall_v = s.kit_body_velocity(slab).y;
        const bool reading = before.state != down && before.state != dead;   // take_blow reads her
        tick(s);
        const auto m = s.matron_state();
        const Vector3 p = s.kit_body_position(slab);
        const Vector3 v = s.kit_body_velocity(slab);
        const auto k = s.debug_matron_strikers();
        bool contact = false;
        bool judged = false;
        for (const auto &striker : k.kept) {
            if (striker.kg > 0.5 * kSlabKg) {
                contact = contact || striker.contact;
                if (striker.judged) {
                    judged = true;
                }
            }
        }
        if (contact && hit_at < 0 && landed_at < 0) {
            hit_at = i;
            out.entry_mps = -fall_v;
            out.her_speed_mps = before.speed;
            out.off_spot_m = std::hypot(before.position.x - her0.x, before.position.z - her0.z);
            // The first step's damage, as her balance read it: the change in
            // her damage over this step. The striker's 3D impulse is not it -
            // take_blow reads the larger of the horizontal and the overhead
            // share, so the two diverge whenever the hit is not purely one
            // way, and the require is about her damage, not the impulse.
            out.first_tick_ns = m.damage_ns - before.damage_ns;
        }
        if (reading && landed_at < 0) {
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
        out.her_lowest_m = std::min(out.her_lowest_m, body_bottom(m) - kDeck);
        out.her_max_up_mps = std::max(out.her_max_up_mps, (m.position.y - before.position.y) * static_cast<double>(kHz));
        if (landed_at < 0 && hit_at < 0 && p.y - kSlabHalfY < kDeck + 0.05 && std::abs(v.y) < 0.5) {
            landed_at = i;
            const double dx = std::max(0.0, std::abs(m.position.x - p.x) - kSlabHalfX);
            const double dz = std::max(0.0, std::abs(m.position.z - p.z) - kSlabHalfZ);
            out.clear_m = std::hypot(dx, dz) - m.shape_r;
        }
        if (trace() && (contact || i % 20 == 0 || m.damage_ns != before.damage_ns)) {
            std::cout << "  t" << i << " slab (" << p.x << "," << p.y << "," << p.z << ") vy=" << v.y << " her ("
                      << m.position.x << "," << m.position.y << "," << m.position.z << ") top=" << body_top(m)
                      << " bottom=" << body_bottom(m) << " state=" << static_cast<int>(m.state) << " dmg=" << m.damage_ns
                      << " contact=" << contact << " judged=" << judged << " box=" << k.kept.size()
                      << " missed=" << k.touching_missed.size() << "\n";
        }
        // The hit: two seconds on, for what the solver does with her under it.
        // A miss: half a second on the deck, before she can walk into it.
        if ((hit_at >= 0 && i - hit_at >= 2 * kHz) || (landed_at >= 0 && i - landed_at >= kHz / 2)) {
            break;
        }
    }
    const auto m = s.matron_state();
    out.added_ns = m.damage_ns - damage0;
    out.slab_end_y = s.kit_body_position(slab).y;
    const Vector3 v = s.kit_body_velocity(slab);
    out.slab_end_mps = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    out.dead = !m.alive;
    out.state = m.state;
    return out;
}

void print(const char *name, const Drop &d) {
    std::cout << "slab-drop " << name << ": fell " << d.fell_m << " m, entry " << d.entry_mps << " m/s (free fall "
              << d.free_fall_mps << "), her speed " << d.her_speed_mps << " m/s " << d.off_spot_m
              << " m off the spot; damage added " << d.added_ns << " N s (first step " << d.first_tick_ns
              << ", kill at " << Matron::kKillDamageNs << "); contact steps " << d.contact_ticks << ", judged "
              << d.judged_ticks << ", missed " << d.missed_ticks << "; overlap " << d.worst_overlap_m
              << " m, through " << d.passed_through << "; slab ends at y " << d.slab_end_y << " moving "
              << d.slab_end_mps << " m/s; her bottom at least " << d.her_lowest_m << " m over the deck, rise at most "
              << d.her_max_up_mps << " m/s; clear " << d.clear_m << " m; dead " << d.dead << " state " << d.state
              << "\n";
}
}  // namespace

// The verdict #410 asks for: one clean hit, from gravity alone, lethal by her
// own damage meter, with the contact stable; and a miss adds nothing.
void run_matron_slab_drop() {
    const Drop hit = drop(false);
    print("standing", hit);
    require(hit.entry_mps > 0.95 * hit.free_fall_mps && hit.entry_mps <= hit.free_fall_mps,
            "slab-drop: gravity alone gave the slab its speed");
    require(hit.her_speed_mps >= 0.0 && hit.her_speed_mps < 0.05 && hit.off_spot_m < 0.05,
            "slab-drop: it met her standing still where it was let go over");
    require(hit.judged_ticks >= 1 && hit.missed_ticks == 0, "slab-drop: her balance judged the hit, and missed nothing");
    require(hit.first_tick_ns >= static_cast<double>(Matron::kKillDamageNs) &&
                std::abs(hit.added_ns - hit.first_tick_ns) < 1.0,
            "slab-drop: one step of contact took her damage past 6000 N s");
    require(hit.dead && hit.state == static_cast<int>(Matron::State::Dead), "slab-drop: the hit killed her");
    require(!hit.passed_through && hit.worst_overlap_m < 0.25 && hit.her_lowest_m > -0.15,
            "slab-drop: no tunnelling, into her or the deck");
    require(hit.slab_end_mps < 0.2 && hit.slab_end_y > kDeck + kSlabHalfY - 0.05 && hit.her_max_up_mps < 3.0,
            "slab-drop: the slab came to rest, the solver kicking nothing away");
    std::cout << "PASS matron-slab-drop standing: " << hit.added_ns << " N s from one step at " << hit.entry_mps
              << " m/s, dead\n";
    const Drop miss = drop(true);
    print("stepping clear", miss);
    require(miss.contact_ticks == 0 && miss.judged_ticks == 0 && miss.clear_m > 0.0,
            "slab-drop: she walked out from under it before it landed");
    require(miss.added_ns == 0.0 && !miss.dead, "slab-drop: a miss adds no damage");
    std::cout << "PASS matron-slab-drop stepping clear: clear by " << miss.clear_m << " m, 0 N s\n";
}
SCRAPERX_SLICE("matron-slab-drop", run_matron_slab_drop);
