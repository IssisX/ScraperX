// J-060 fight contact truth (issue #320): a blow lands only when the
// striking limb really meets a part of her body; a near miss misses; her
// arm in the fist's way takes the blow; her shove is her hand on him; both
// bodies take each blow's force.
//
// The measurement (matron-fight-measure) prints, for each staged blow, the
// limb's point on the tick it scored, its gap to her simulated body and to
// her drawn body (tests/data/matron_drawn_limbs.inc: her drawn limb
// volumes, 30 recorded frames per stance), and for her shove what it gave
// each body on the tick it fired. It asserts only its own setup.
#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_injury.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
namespace injury = scraperx::sim::injury;

constexpr double kPi = 3.141592653589793;

struct DrawnLimb {
    int stance;
    int frame;
    int region; // 0 trunk and head, 1 forearm and hand, 2 upper arm, 3 leg
    float a[3];
    float b[3];
    float radius;
};
constexpr DrawnLimb kDrawn[] = {
#include "data/matron_drawn_limbs.inc"
};

struct V {
    double x, y, z;
};
V sub(const V a, const V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const V a, const V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double len(const V a) { return std::sqrt(dot(a, a)); }
V of(const Vector3 v) { return {v.x, v.y, v.z}; }

double to_segment(const V p, const V a, const V b) {
    const V ab = sub(b, a);
    const double l2 = dot(ab, ab);
    const double t = l2 > 1.0e-12 ? std::clamp(dot(sub(p, a), ab) / l2, 0.0, 1.0) : 0.0;
    return len(sub(p, V{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t}));
}

// Her body frame (x right, y up, z forward about her middle, turned by her
// yaw: shape_fit.py's frame) to the world.
V to_world(const V mid, const double yaw, const float *l) {
    const double c = std::cos(yaw), s = std::sin(yaw);
    return {mid.x + c * l[0] + s * l[2], mid.y + l[1], mid.z - s * l[0] + c * l[2]};
}

// The limb sphere's gap to her drawn body in each recorded frame of `stance`
// (negative: inside it): the least, the median and the most over her
// frames, and the region nearest in the median frame.
struct DrawnGap {
    double least = 1.0e9, median = 1.0e9, most = -1.0e9;
    int region = -1;
    int frames = 0;
};
DrawnGap drawn_gap(const V limb, const double limb_r, const V mid, const double yaw, const int stance) {
    std::vector<std::pair<double, int>> per;
    for (int f = 0; f < 64; ++f) {
        double best = 1.0e9;
        int region = -1;
        bool any = false;
        for (const DrawnLimb &d : kDrawn) {
            if (d.stance != stance || d.frame != f) continue;
            any = true;
            const double g = to_segment(limb, to_world(mid, yaw, d.a), to_world(mid, yaw, d.b)) - d.radius - limb_r;
            if (g < best) {
                best = g;
                region = d.region;
            }
        }
        if (any) per.emplace_back(best, region);
    }
    DrawnGap out;
    if (per.empty()) return out;
    std::sort(per.begin(), per.end());
    out.frames = static_cast<int>(per.size());
    out.least = per.front().first;
    out.most = per.back().first;
    out.median = per[per.size() / 2].first;
    out.region = per[per.size() / 2].second;
    return out;
}

// The limb sphere's gap to her simulated body (her stance capsule as the
// snapshot reports it).
double sim_gap(const V limb, const double limb_r, const scraperx::sim::MatronSnapshot &m) {
    return to_segment(limb, of(m.shape_a), of(m.shape_b)) - m.shape_r - limb_r;
}

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

Vector3 settle(Simulation &s) {
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    return p;
}

void wait_hunting(Simulation &s, const char *what) {
    constexpr int kHunting = static_cast<int>(Matron::State::Hunting);
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && s.matron_state().state != kHunting; ++i) {
        tick(s);
    }
    require(s.matron_state().state == kHunting, what);
}

double chest_dose(const scraperx::sim::MatronSnapshot &m) {
    return m.injury_j_region[static_cast<std::size_t>(injury::Region::Chest)];
}

// One blow at her: she `ahead` m in front of him (his facing +Z), `lateral`
// m to the side (+X), hunting, her hands held, her pace a creep, her facing
// pinned at him. move_kind: 0 punch (the jab), 1 sweep (crouched), 2 kick
// (moving toward her). `stance`: the press waits (at most 1 s) for her body
// to be in that stance (Matron::Stance; -1 whatever it is).
struct Blow {
    bool offered = false;
    std::uint64_t hits = 0;
    double impulse_ns = 0.0;
    double blow_mps = 0.0;
    int scoring_tick = -1;
    int stance = -1;
    int part = 255;
    V limb{0, 0, 0};
    double limb_to_sim_m = 0.0;
    DrawnGap drawn{};
    double her_speed = 0.0;
    double limb_up_m = 0.0;  // the limb over her middle
    double chest_before = 0.0, chest_after = 0.0, unclassified_after = 0.0;
    double her_dv = 0.0;  // her change of velocity along his facing over the blow
    double his_dv = 0.0;  // his
};

Blow blow_at(const double ahead, const double lateral, const int move_kind, const int stance = -1) {
    Simulation s(InitialSpawn::Deck154);
    const Vector3 p = settle(s);
    s.launch_matron_for_test(Vector3{p.x + lateral, p.y - 0.20, p.z + ahead});
    s.debug_hold_matron_hands(30.0);
    wait_hunting(s, "fight: she hunts before the blow (setup)");
    s.debug_fix_matron_pace(0);
    s.debug_place_matron(Vector3{p.x + lateral, p.y - 0.85, p.z + ahead});
    s.debug_face_matron(kPi);
    if (move_kind == 1) {
        (void)s.set_crouch_input(true);
        for (int i = 0; i < 10 && !s.snapshot().player_crouched; ++i) tick(s);
        require(s.snapshot().player_crouched, "fight: crouched before the sweep (setup)");
    }
    if (move_kind == 2) {
        (void)s.set_move_input(0.0, 1.0);
        tick(s);
    }
    tick(s);
    for (int i = 0; i < static_cast<int>(Simulation::kTickRateHz) && stance >= 0 && s.matron_state().stance != stance;
         ++i) {
        s.debug_face_matron(kPi);
        tick(s);
    }
    require(stance < 0 || s.matron_state().stance == stance, "fight: her body in the stance asked (setup)");
    // The offer is read before her stance upkeep in a tick: one more, for an
    // offer of the body she has.
    s.debug_face_matron(kPi);
    tick(s);
    require(stance < 0 || s.matron_state().stance == stance, "fight: her body stays in the stance asked (setup)");
    Blow out;
    out.offered = s.snapshot().strike_offered;
    out.chest_before = chest_dose(s.matron_state());
    out.her_speed = s.matron_state().speed;
    const std::uint64_t hits0 = s.snapshot().strike_hit_count;
    Vector3 her_prev = s.matron_state().position;
    Vector3 his_prev = s.snapshot().player_position;
    double her_v0 = 0.0, his_v0 = 0.0, her_v1 = 0.0, his_v1 = 0.0;
    (void)s.request_strike();
    for (int i = 0; i < 60; ++i) {
        tick(s);
        if (i == 0) (void)s.set_move_input(0.0, 0.0);
        s.debug_face_matron(kPi);
        const auto snap = s.snapshot();
        const auto m = s.matron_state();
        const double her_v = (m.position.z - her_prev.z) * Simulation::kTickRateHz;
        const double his_v = (snap.player_position.z - his_prev.z) * Simulation::kTickRateHz;
        her_prev = m.position;
        his_prev = snap.player_position;
        out.blow_mps = std::max(out.blow_mps, m.blow_mps);
        if (out.scoring_tick < 0 && snap.strike_hit_count > hits0) {
            out.scoring_tick = i + 1;
            out.stance = m.stance;
            out.part = snap.strike_part;
            // The contact as the sim has it: the point on her part.
            out.limb = of(snap.strike_hit_point);
            out.limb_to_sim_m = sim_gap(out.limb, 0.0, m);
            out.limb_up_m = out.limb.y - m.position.y;
            out.drawn = drawn_gap(out.limb, 0.0, of(m.position), m.yaw, m.stance);
        }
        if (out.scoring_tick > 0 && i + 1 == out.scoring_tick + 8) {
            her_v1 = her_v;
            his_v1 = his_v;
        }
    }
    out.her_dv = her_v1 - her_v0;
    out.his_dv = his_v1 - his_v0;
    out.hits = s.snapshot().strike_hit_count - hits0;
    out.impulse_ns = s.snapshot().strike_impulse_ns;
    out.chest_after = chest_dose(s.matron_state());
    out.unclassified_after = s.matron_state().injury_j_region_unclassified;
    return out;
}

void print_blow(const char *label, const Blow &b) {
    std::cout << "  fight-measure " << label << ": offered=" << b.offered << " hits=" << b.hits
              << " impulse_ns=" << b.impulse_ns << " blow_mps=" << b.blow_mps << " scoring_tick=" << b.scoring_tick
              << " part_stance=" << b.stance << " part=" << b.part << " her_speed=" << b.her_speed << " contact_over_her_middle_m=" << b.limb_up_m;
    if (b.scoring_tick > 0) {
        std::cout << " contact_to_sim_body_m=" << b.limb_to_sim_m << " contact_to_drawn_m(least/median/most)="
                  << b.drawn.least << "/" << b.drawn.median << "/" << b.drawn.most << " over " << b.drawn.frames
                  << " frames, nearest_region=" << b.drawn.region;
    }
    std::cout << " chest_dose " << b.chest_before << "->" << b.chest_after << "\n";
}

// Her shove: her placed `ahead` m in front of him (`lateral` to the side),
// facing `yaw`, her grab held so a touch is her shove. What the tick it fired
// gave him and her.
struct Shove {
    std::uint64_t before = 0, after = 0;
    double his_dv = 0.0;   // his change of speed on the shove's tick
    double her_dv = 0.0;   // hers, along the shove
    double his_kg = 85.0;
};
Shove shove_at(const double ahead, const double lateral, const double yaw,
               const std::vector<Simulation::TestExtraSolid> &solids = {}) {
    Simulation s(InitialSpawn::Deck154, solids);
    const Vector3 p = settle(s);
    s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
    s.debug_hold_matron_grab(true);
    wait_hunting(s, "fight: she hunts before the shove window (setup)");
    Shove out;
    out.before = s.matron_state().shoves;
    s.debug_place_matron(Vector3{p.x + lateral, p.y - 0.85, p.z + ahead});
    s.debug_face_matron(yaw);
    // His velocity as Jolt has it; hers from her middle's travel. The
    // shove's change: his on the tick it fired against the tick before; hers
    // over the two ticks after it (the placement's own jump is the tick of it).
    const Vector3 rest = s.snapshot().player_linear_velocity;
    std::vector<Vector3> his_v, her_v;
    int fired = -1;
    Vector3 her_prev = s.matron_state().position;
    for (int i = 0; i < 90; ++i) {
        s.debug_face_matron(yaw);
        tick(s);
        const Vector3 her = s.matron_state().position;
        his_v.push_back(s.snapshot().player_linear_velocity);
        her_v.push_back(Vector3{(her.x - her_prev.x) * Simulation::kTickRateHz, 0.0,
                                (her.z - her_prev.z) * Simulation::kTickRateHz});
        her_prev = her;
        // The tick his speed jumps (her shove; the count is read at the end).
        const Vector3 was = i > 0 ? his_v[his_v.size() - 2] : rest, now = his_v.back();
        if (fired < 0 && std::hypot(now.x - was.x, now.y - was.y, now.z - was.z) > 0.25) fired = i;
    }
    if (fired >= 0 && fired + 2 < static_cast<int>(his_v.size())) {
        const Vector3 a = fired > 0 ? his_v[static_cast<std::size_t>(fired - 1)] : rest;
        const Vector3 b = his_v[static_cast<std::size_t>(fired)];
        out.his_dv = std::hypot(b.x - a.x, b.y - a.y, b.z - a.z);
        const Vector3 d = her_v[static_cast<std::size_t>(fired + 2)];
        out.her_dv = std::hypot(d.x, d.z);
    }
    out.after = s.matron_state().shoves;
    return out;
}

// A 1 cm steel plate, 1.5 m tall, between them at 0.30 m ahead of his spawn.
std::vector<Simulation::TestExtraSolid> post_between() {
    Vector3 start{};
    {
        Simulation s(InitialSpawn::Deck154);
        start = settle(s);
    }
    return {Simulation::TestExtraSolid{start.x, start.y + 0.75, start.z + 0.30, 0.005F, 0.75F, 0.005F, true}};
}

void print_shove(const char *label, const Shove &v) {
    std::cout << "  fight-measure shove " << label << ": shoves=" << (v.after - v.before) << " his_dv_mps=" << v.his_dv
              << " (his impulse " << v.his_dv * v.his_kg << " N-s, " << v.his_dv * v.his_kg * Simulation::kTickRateHz
              << " N in one tick) her_dv_mps=" << v.her_dv << " (her impulse " << v.her_dv * Matron::kMassKg << " N-s)\n";
}
} // namespace

void run_matron_fight_measure() {
    std::cout << "J-060 fight-measure\n";
    print_blow("punch dead on 1.05m", blow_at(1.05, 0.0, 0));
    print_blow("punch near miss 0.75m +0.33m", blow_at(0.75, 0.33, 0));
    print_blow("punch at her arm 1.00m -0.20m", blow_at(1.00, -0.20, 0));
    print_blow("kick 1.05m", blow_at(1.05, 0.0, 2));
    print_blow("sweep 1.05m", blow_at(1.05, 0.0, 1));

    print_shove("face-on 0.60m", shove_at(0.60, 0.0, kPi));
    print_shove("she faces away 0.60m", shove_at(0.60, 0.0, 0.0));
    print_shove("through steel 0.60m", shove_at(0.60, 0.0, kPi, post_between()));
    print_shove("at her side 0.60m", shove_at(0.0, 0.60, 0.0));
    std::cout << "PASS scraperx_sim matron-fight-measure\n";
}
SCRAPERX_SLICE("matron-fight-measure", run_matron_fight_measure);

// J-060's cases, each failing on trunk's capsule test first:
//  - her arm in the fist's way takes the blow: a punch 0.33 m beside her
//    axis, her standing (her forearm hangs there, 0.22 to 0.28 m out, 0.115 m
//    thick), lands on her arm: no dose to her chest (trunk dosed her chest);
//  - a near miss misses: the same punch with her hunting low (her drawn
//    trunk and head 0.25 m wide under it, her arms below it) meets nothing,
//    and the punch is not offered (trunk landed it: 0.33 m is inside her
//    old capsule's 0.30 m and the fist's 0.06 m);
//  - the hits that were true stay: dead on, standing and low, it lands once
//    with the jab's held impulse, on her trunk or head.
void run_matron_fight_contact() {
    constexpr int kUpright = static_cast<int>(Matron::Stance::Upright);
    constexpr int kLow = static_cast<int>(Matron::Stance::Low);
    const Blow hit = blow_at(1.05, 0.0, 0, kUpright);
    print_blow("hit, she stands", hit);
    require(hit.offered && hit.hits == 1 && std::abs(hit.impulse_ns - 66.7) < 0.5 && hit.blow_mps > 0.29 &&
                hit.chest_after - hit.chest_before > 66.0,
            "fight-contact: a punch dead on her standing body lands once, the jab's held impulse, on her chest");
    const Blow low = blow_at(1.05, 0.0, 0, kLow);
    print_blow("hit, she hunts low", low);
    require(low.offered && low.hits == 1 && std::abs(low.impulse_ns - 66.7) < 0.5 && low.blow_mps > 0.29,
            "fight-contact: a punch dead on her body hunting low lands once, the jab's held impulse");
    const Blow arm = blow_at(0.75, 0.33, 0, kUpright);
    print_blow("at her arm, she stands", arm);
    require(arm.offered && arm.hits == 1 && std::abs(arm.impulse_ns - 66.7) < 0.5,
            "fight-contact: a punch 0.33 m beside her standing axis lands, on the arm in its way");
    require(arm.chest_after == arm.chest_before && arm.unclassified_after > 66.0,
            "fight-contact: her arm in the fist's way takes the blow, not her chest behind it");
    const Blow miss = blow_at(0.75, 0.33, 0, kLow);
    print_blow("near miss, she hunts low", miss);
    require(!miss.offered && miss.hits == 0 && miss.impulse_ns == 0.0 && miss.chest_after == miss.chest_before,
            "fight-contact: a fist 0.33 m beside her axis, clear of her low body, misses and is not offered");
    std::cout << "PASS scraperx_sim matron-fight-contact\n";
}
SCRAPERX_SLICE("matron-fight-contact", run_matron_fight_contact);
