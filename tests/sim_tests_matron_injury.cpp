// J-059 injury feed (#307): per-region injury from where each blow lands,
// exported by the bridge, drawn by matron_injury.gd. J-044a §D/E is the
// contract: every accepted impulse is credited once (sum regions +
// unclassified == damage delta each tick), eye stages 75/150/250, and the
// chest dose costs her arms through the one explicit capability
// conversion. The view needs no change (it already passes data["injury"]);
// these tests pin the sim side and the bridge export.
#include "sim_tests.hpp"
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
#include "sim/matron_capacity.hpp"
#include "sim/matron_injury.hpp"
#include <array>
#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
namespace injury = scraperx::sim::injury;
namespace capacity = scraperx::sim::capacity;

void tick(Simulation &s) { require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "injury: tick accepted"); }

Vector3 add(const Vector3 a, const Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 scaled(const Vector3 a, const double k) { return {a.x * k, a.y * k, a.z * k}; }

// Her hunting, stage Dangerous, the player in front of her: the damage
// test's setup, so a real strike can land. `gap_m` is how far north of the
// player she emerges: the injected-blow tests use 3 m, so she cannot close
// to grab range (0.85 m) while the test aims at her head — in a grab her
// head tracks the held player and swings too fast for a one-tick-old aim.
void setup(Simulation &s, const double gap_m = 1.05) {
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test({p.x, p.y - 0.20, p.z + gap_m});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                    s.matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) tick(s);
    require(s.matron_state().state == static_cast<int>(Matron::State::Hunting),
            "injury: Matron must finish Emerging before a judged blow");
    s.debug_set_matron_stage(2);
}

double region(const Simulation &s, const injury::Region r) {
    return s.matron_state().injury_j_region[static_cast<std::size_t>(r)];
}

double regions_total(const Simulation &s) {
    double sum = 0.0;
    for (const double d : s.matron_state().injury_j_region) sum += d;
    return sum + s.matron_state().injury_j_region_unclassified;
}

// A player blow at a chosen point (the strike's own contact point in real
// play; here placed by the test so the classifier is what is measured).
// The impulse pushes her away from the player in front of her.
void blow_at(Simulation &s, const Vector3 at, const float ns) {
    const auto m = s.matron_state();
    const float yaw = static_cast<float>(m.yaw);
    const Vector3 dir{-std::sin(yaw), 0.0, -std::cos(yaw)};
    s.debug_matron_player_blow_at(scaled(dir, ns), at, false);
    tick(s);
    for (int i = 0; i < 10; ++i) tick(s);  // let the blow window close
}

// Tick until her head stops turning (she faces the player, head settled),
// so a point computed from the snapshot is the frame take_blow will use
// on the next tick. Bounded: if she never settles, the fresh point still
// bounds the error to one tick of head motion.
// Put her back 3 m north of the player, feet on the deck: she hunts him
// and walks the gap closed while the test aims, so without this she ends
// up on top of him with her head whipping after him at close range. The
// placement keeps state Hunting and touches no damage or injury.
void replace_gap(Simulation &s) {
    const Vector3 p = s.snapshot().player_position;
    const Vector3 m = s.matron_state().position;
    s.debug_place_matron({p.x, m.y - static_cast<double>(Matron::kStandCentre), p.z + 3.0});
}

void settle_head(Simulation &s) {
    // First, her eyes back open: a blow staggers her (0.4-1.2 s), and while
    // she staggers eyes() returns the zero frame — a point aimed from that
    // snapshot is garbage. Wait for Hunting again, bounded.
    for (int i = 0; i < 200 &&
                    s.matron_state().state != static_cast<int>(Matron::State::Hunting);
         ++i) {
        tick(s);
    }
    double prev = s.matron_state().eye_yaw;
    int stable = 0;
    for (int i = 0; i < 360 && stable < 5; ++i) {
        tick(s);
        const double now = s.matron_state().eye_yaw;
        stable = std::abs(std::remainder(now - prev, 2.0 * 3.141592653589793)) < 0.001 ? stable + 1 : 0;
        prev = now;
    }
}

// Her eye midpoint and head-left from the snapshot's own eye frame, so the
// test and the classifier read the same pose.
Vector3 eye_mid(const Simulation &s) { return s.matron_state().eye; }
Vector3 head_left(const Simulation &s) {
    const double yaw = s.matron_state().eye_yaw;
    return {std::cos(yaw), 0.0, -std::sin(yaw)};
}
Vector3 head_fwd(const Simulation &s) {
    const double yaw = s.matron_state().eye_yaw;
    return {std::sin(yaw), 0.0, std::cos(yaw)};
}
// A fist's contact point on her capsule surface, in the eye frame: the
// capsule radius (0.30 m) less the eye forward offset (0.12 m). A point at
// the socket itself sits inside her head and is no contact at all.
constexpr double kCapsuleSurfaceM = 0.18;

void run_classify() {
    using JPH::Vec3;
    const Vec3 eye(0.0F, 1.69F, 0.0F);  // eye midpoint, sim frame
    const Vec3 fwd(0.0F, 0.0F, 1.0F);   // head faces +Z
    const auto at = [&](const float x, const float y, const float z) {
        return Vec3(x, 1.69F + y, z);
    };
    require(injury::classify_contact(at(0.0315F, 0.0F, 0.10F), eye, fwd) == injury::Region::EyeL,
            "injury: a fist on her left socket is eye_l");
    require(injury::classify_contact(at(-0.0315F, 0.0F, 0.10F), eye, fwd) == injury::Region::EyeR,
            "injury: a fist on her right socket is eye_r");
    require(injury::classify_contact(at(0.10F, -0.05F, 0.10F), eye, fwd) == injury::Region::FaceL,
            "injury: a fist on her left cheek is face_l");
    require(injury::classify_contact(at(-0.10F, -0.05F, 0.10F), eye, fwd) == injury::Region::FaceR,
            "injury: a fist on her right cheek is face_r");
    require(injury::classify_contact(at(0.0F, -0.05F, 0.10F), eye, fwd) == injury::Region::Jaw,
            "injury: a fist dead on her chin is jaw");
    require(injury::classify_contact(at(0.0F, 0.0F, -0.10F), eye, fwd) == injury::Region::Skull,
            "injury: a fist on the back of her head is skull, never an eye");
    require(injury::classify_contact(at(0.0315F, 0.0F, -0.10F), eye, fwd) == injury::Region::Skull,
            "injury: a fist at her eye's height from behind is skull, not the eye");
    require(injury::classify_contact(at(0.0F, -0.15F, 0.10F), eye, fwd) == injury::Region::Neck,
            "injury: a fist on her throat is neck");
    require(injury::classify_contact(at(0.0F, -0.30F, 0.10F), eye, fwd) == injury::Region::Chest,
            "injury: a fist on her trunk is chest");
    require(injury::classify_contact(at(0.0F, 0.20F, 0.10F), eye, fwd) == injury::Region::Unclassified,
            "injury: a fist over the crown is unclassified");
    require(injury::classify_contact(at(0.0F, -0.80F, 0.10F), eye, fwd) == injury::Region::Unclassified,
            "injury: a fist at her ankle is unclassified");
    const Vec3 middle(0.0F, 1.05F, 0.0F);
    require(injury::classify_striker_centre(Vec3(0.0F, 1.30F, 0.0F), middle, fwd) == injury::Region::Chest,
            "injury: a weight's middle at her trunk is chest");
    require(injury::classify_striker_centre(Vec3(0.0F, 1.75F, 0.0F), middle, fwd) == injury::Region::Unclassified,
            "injury: a weight's middle at her head height is unclassified, never a face");
    std::cout << "PASS matron-injury classify\n";
}

void run_eye_stages() {
    Simulation s(InitialSpawn::Deck154);
    setup(s, 3.0);
    // The socket is recomputed fresh before each blow: she turns to face
    // the player, so a point from an earlier frame reads in the wrong one.
    const auto socket_l = [&]() {
        replace_gap(s);
        settle_head(s);
        return add(add(eye_mid(s), scaled(head_left(s), injury::kEyeLateralM)),
                   scaled(head_fwd(s), kCapsuleSurfaceM));
    };
    blow_at(s, socket_l(), 50.0F);
    require(std::abs(region(s, injury::Region::EyeL) - 50.0) < 0.01,
            "injury: one 50 N·s blow to her left eye is 50 in eye_l");
    require(region(s, injury::Region::EyeR) == 0.0 && region(s, injury::Region::FaceL) == 0.0,
            "injury: her left eye takes it alone");
    blow_at(s, socket_l(), 50.0F);
    require(std::abs(region(s, injury::Region::EyeL) - 100.0) < 0.01,
            "injury: a second 50 N·s blow is 100 in eye_l (ring at 75)");
    blow_at(s, socket_l(), 50.0F);
    require(std::abs(region(s, injury::Region::EyeL) - 150.0) < 0.01,
            "injury: 150 in eye_l (lid swells at 150)");
    blow_at(s, socket_l(), 100.0F);
    require(std::abs(region(s, injury::Region::EyeL) - 250.0) < 0.01,
            "injury: 250 in eye_l (slit at 250)");
    require(std::abs(s.matron_state().damage_ns - 250.0) < 0.01 &&
                std::abs(s.matron_state().player_damage_ns - 250.0) < 0.01,
            "injury: the eye blows are the whole damage, player-attributed, once");
    require(std::abs(regions_total(s) - s.matron_state().damage_ns) < 0.01,
            "injury: regions + unclassified reconcile with damage_ns");
    std::cout << "PASS matron-injury eye stages\n";
}

void run_face_sides() {
    Simulation s(InitialSpawn::Deck154);
    setup(s, 3.0);
    // Each cheek is recomputed fresh: she turns to face the player, so a
    // point from an earlier frame reads in the wrong one.
    const auto cheek = [&](const double side) {
        replace_gap(s);
        settle_head(s);
        return add(add(add(eye_mid(s), scaled(head_left(s), side)), {0.0, -0.05, 0.0}),
                   scaled(head_fwd(s), kCapsuleSurfaceM));
    };
    blow_at(s, cheek(0.10), 50.0F);
    require(std::abs(region(s, injury::Region::FaceL) - 50.0) < 0.01,
            "injury: a left-cheek blow is face_l");
    require(region(s, injury::Region::FaceR) == 0.0 && region(s, injury::Region::EyeL) == 0.0,
            "injury: her left cheek takes it alone, not the eye");
    blow_at(s, cheek(-0.10), 50.0F);
    require(std::abs(region(s, injury::Region::FaceR) - 50.0) < 0.01,
            "injury: a right-cheek blow is face_r");
    require(std::abs(region(s, injury::Region::FaceL) - 50.0) < 0.01,
            "injury: the left cheek keeps its dose (no heal)");
    std::cout << "PASS matron-injury face sides\n";
}

void run_real_punch() {
    Simulation s(InitialSpawn::Deck154);
    setup(s);
    for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
        const Vector3 me = s.snapshot().player_position, her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
        tick(s);
    }
    require(s.request_strike(), "injury: physical punch starts");
    for (int i = 0; i < 65; ++i) tick(s);
    const auto m = s.matron_state();
    std::cout << "injury real-punch trace: total=" << m.damage_ns << " player=" << m.player_damage_ns
              << " chest=" << region(s, injury::Region::Chest)
              << " unclassified=" << s.matron_state().injury_j_region_unclassified
              << " strike_impulse=" << s.snapshot().strike_impulse_ns << "\n";
    require(s.snapshot().strike_hit_count == 1 && s.snapshot().strike_impulse_ns > 0.0,
            "injury: one actual punch lands");
    require(m.player_damage_ns > 0.0 && std::abs(m.damage_ns - m.player_damage_ns) < 0.02,
            "injury: the real punch is the whole damage");
    require(region(s, injury::Region::Chest) > 0.0,
            "injury: a real standing punch lands on her trunk");
    require(std::abs(regions_total(s) - m.damage_ns) < 0.01,
            "injury: the real punch reconciles: regions + unclassified == damage_ns");
    std::cout << "PASS matron-injury real punch\n";
}

void run_mixed_reconciliation() {
    Simulation s(InitialSpawn::Deck154);
    setup(s);
    for (int i = 0; i < 90 && !s.snapshot().strike_offered; ++i) {
        const Vector3 me = s.snapshot().player_position, her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
        tick(s);
    }
    require(s.request_strike(), "injury: physical punch starts");
    const std::uint32_t first = s.loose_stock_throwables_begin();
    const std::uint32_t piece = s.kit_body_index(LooseStock::kFirstEntity + first);
    const double before = s.matron_state().damage_ns;
    for (int i = 0; i < 65; ++i) {
        if (i == 11) {
            const Vector3 her = s.matron_state().position;
            const double feet = her.y - static_cast<double>(Matron::kStandCentre);
            s.debug_launch_kit_body(piece, {her.x, feet + 1.3, her.z + 1.15}, {0.0, 0.0, -8.0});
        }
        tick(s);
    }
    const auto m = s.matron_state();
    require(m.damage_ns > before && m.player_damage_ns > 0.0 && m.damage_ns > m.player_damage_ns,
            "injury: the mixed window has player and stock shares");
    require(std::abs(regions_total(s) - (m.damage_ns - before)) < 0.02,
            "injury: mixed window reconciles: regions + unclassified == damage delta");
    require(region(s, injury::Region::FaceL) == 0.0 && region(s, injury::Region::FaceR) == 0.0 &&
                region(s, injury::Region::EyeL) == 0.0 && region(s, injury::Region::EyeR) == 0.0,
            "injury: no facial injury is fabricated from a stock body");
    std::cout << "PASS matron-injury mixed reconciliation\n";
}

void run_capability() {
    Simulation s(InitialSpawn::Deck154);
    setup(s, 3.0);
    const Vector3 e = eye_mid(s);
    const Vector3 chest_at = add(add(e, {0.0, -0.26, 0.0}), scaled(head_fwd(s), kCapsuleSurfaceM));
    for (int i = 0; i < 18; ++i) blow_at(s, chest_at, 150.0F);  // 2700 N·s to her chest
    const auto m = s.matron_state();
    require(std::abs(region(s, injury::Region::Chest) - 2700.0) < 1.0,
            "injury: 18 x 150 N·s to her chest is 2700 in chest");
    require(m.limb_health[0] == 0.0 && m.limb_health[1] == 0.0,
            "injury: at 2700 chest dose her arms are spent");
    require(m.limb_health[2] == 1.0 && m.limb_health[3] == 1.0,
            "injury: her legs are whole (no leg regions in slice 1)");
    // Healthy-side compensation through the real capacity function: the
    // arms spent, the legs take the load up to their own caps, no force
    // created. The demand is the legs' combined cap (6000), not the 8 kN
    // budget: the property under test is the honest shortfall at the caps,
    // and the budget number itself is not this test's to restate.
    const std::array<capacity::Limb, 4> limbs{{
        {3000.0F, static_cast<float>(m.limb_health[0]), true, 1.0F},
        {3000.0F, static_cast<float>(m.limb_health[1]), true, 1.0F},
        {3000.0F, static_cast<float>(m.limb_health[2]), true, 1.0F},
        {3000.0F, static_cast<float>(m.limb_health[3]), true, 1.0F},
    }};
    const capacity::Allocation a = capacity::allocate(limbs, 6000.0F, 6000.0F);
    require(a.applied_n[0] == 0.0F && a.applied_n[1] == 0.0F,
            "injury: spent arms apply nothing");
    require(std::abs(a.total_n - 6000.0F) < 0.01,
            "injury: the healthy legs cover 6000 honestly, no backfill");
    std::cout << "PASS matron-injury capability\n";
}
}  // namespace

void run_matron_injury() {
    run_classify();
    run_eye_stages();
    run_face_sides();
    run_real_punch();
    run_mixed_reconciliation();
    run_capability();
}
