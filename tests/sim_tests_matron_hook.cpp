// J-090 (#408): hook her and let the rope decide. His grappling hook, a real
// 1.5 kg body in flight, bites the Matron where it strikes her collision
// body; the line then joins his chest and her body (the kit's two-body rope,
// over the ring's edge as a sheave when it bends there). The reel stays at
// kHookReelForceN and the tines' hold at kHookHoldForceN, so nothing gives
// him more than the line gives: her feet, gravity and the steel answer the
// pull. Once she is staged, nothing here writes her position or velocity.
//
// All on the 220 ring, where he first takes the hook: its north band (top
// 220.25, inner lip z -130.73, the well beyond it) and its west band.
#include "sim_tests.hpp"
#include "sim/matron.hpp"
#include "sim/physics_world_tuning.hpp"

#include <cstdio>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::MatronSnapshot;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Vector3;
namespace tuning = scraperx::sim::detail;

constexpr double kDt = Simulation::kFixedStepSeconds;
constexpr double kRingTop = 220.25;
constexpr double kRackX = -21.6, kRackZ = -127.6;   // where he stands to take the hook
constexpr double kWestX = -21.6, kWestZ = -134.0;   // on the west band, the well between him and her

double dist3(const Vector3 &a, const Vector3 &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// From p to the segment a-b.
double to_segment(const Vector3 &p, const Vector3 &a, const Vector3 &b) {
    const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const double uu = ux * ux + uy * uy + uz * uz;
    double t = uu > 0.0 ? ((p.x - a.x) * ux + (p.y - a.y) * uy + (p.z - a.z) * uz) / uu : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return dist3(p, {a.x + ux * t, a.y + uy * t, a.z + uz * t});
}

double her_surface_gap(const Vector3 &p, const MatronSnapshot &m) { return to_segment(p, m.shape_a, m.shape_b) - m.shape_r; }

void face(Simulation &s, const Vector3 &at) {
    const auto p = s.snapshot().player_position;
    const double dx = at.x - p.x, dz = at.z - p.z, l = std::max(1.0e-6, std::hypot(dx, dz));
    (void)s.set_facing(dx / l, dz / l);
}

struct Stage {
    Vector3 her;          // her feet, staged standing
    double her_face = 0.0;
    double him_x = kRackX, him_z = kRackZ;
    bool watched = true;  // his eyes on her: she holds still where she stands (her stalk)
    double aim_up = 0.3;  // above her body's middle
};

// He takes the hook at the 220 ring's rack, walks to his spot; she is put
// standing at hers (her hands held off him: her grab is J-001's, not this
// job's), and he fires at her. Returns the snapshot of the tick the hook
// came down (bit or not), and hers then.
std::pair<Snapshot, MatronSnapshot> hook_her(Simulation &s, const Stage &st) {
    require(s.advance_frame(0.4).accepted, "matron-hook: settle");
    require(walk_to(s, kRackX, kRackZ, 14.0, 0.3), "matron-hook: he walks the 220 ring's north band to the rack");
    require(s.request_hook_take() && s.advance_frame(kDt).accepted && s.snapshot().has_hook,
            "matron-hook: the hook taken from its rack");
    if (st.him_x != kRackX || st.him_z != kRackZ) {
        require(walk_to(s, kRackX + 0.3, st.him_z, 20.0, 0.3) && walk_to(s, st.him_x, st.him_z, 10.0, 0.2),
                "matron-hook: he walks to where he fires from");
    }
    s.debug_place_matron(st.her);
    s.debug_hold_matron_grab(true);
    s.debug_hold_matron_hands(60.0);
    s.debug_face_matron(st.her_face);
    if (st.watched) face(s, st.her);
    require(s.advance_frame(0.3).accepted, "matron-hook: she stands");
    const MatronSnapshot m = s.matron_state();
    const auto p = s.snapshot().player_position;
    require(s.set_hook_aim(m.position.x - p.x, m.position.y + st.aim_up - (p.y + 0.6), m.position.z - p.z) &&
                s.request_hook_fire(),
            "matron-hook: fired at her");
    for (int i = 0; i < 90 * 3; ++i) {
        if (st.watched) face(s, s.matron_state().position);
        require(s.advance_frame(kDt).accepted, "matron-hook: flight tick");
        if (s.snapshot().hook_state != 1) break;
    }
    return {s.snapshot(), s.matron_state()};
}

// What the pull did, tick by tick, up to `ticks` or his death (a death puts
// the world back at the checkpoint: nothing after it is the tug's).
struct Tug {
    double peak_tension_n = 0.0;
    double peak_feet_n = 0.0;
    double her_step_m = 0.0;    // her largest move in one tick
    double his_jump_m = 0.0;   // his move in a tick past what his speed carries him
    double her_moved_m = 0.0;   // her furthest from where the pull began (horizontal)
    double his_moved_m = 0.0;
    double his_lowest_y = 1.0e9;
    std::uint32_t most_bends = 0;
    bool her_gravity_on = true;
    bool her_supported = true;  // every tick
    bool he_died = false;
    std::uint64_t tears = 0;
    std::uint64_t stalls = 0;
    int first_over_tick = -1;   // the first tick past the tines' hold
    int tear_tick = -1;
    bool torn_free = false;     // on the tear tick: the hook out of her and in flight
    int ticks = 0;
    double fold = 0.0;          // their positions folded, for the determinism check
};

template <typename Each>
Tug pull(Simulation &s, const int ticks, Each each) {
    Tug t;
    Snapshot prev = s.snapshot();
    MatronSnapshot her_prev = s.matron_state();
    const Vector3 her0 = her_prev.position;
    const Vector3 him0 = prev.player_position;
    const std::uint64_t tears0 = prev.hook_tear_count;
    const std::uint64_t stalls0 = prev.hook_stall_ticks;
    for (int i = 0; i < ticks; ++i) {
        each(i);
        require(s.advance_frame(kDt).accepted, "matron-hook: tick");
        const Snapshot n = s.snapshot();
        const MatronSnapshot m = s.matron_state();
        if (n.death_count != prev.death_count) {
            t.he_died = true;
            break;
        }
        t.peak_tension_n = std::max(t.peak_tension_n, n.hook_tension_n);
        if (t.first_over_tick < 0 && n.hook_tension_n > static_cast<double>(tuning::kHookHoldForceN)) t.first_over_tick = i;
        if (t.tear_tick < 0 && n.hook_tear_count > tears0) {
            t.tear_tick = i;
            t.torn_free = n.hook_state == 1 && !n.hook_in_matron;
        }
        t.peak_feet_n = std::max(t.peak_feet_n, m.feet_push_n);
        t.her_step_m = std::max(t.her_step_m, dist3(her_prev.position, m.position));
        const double carried = 0.5 * (std::hypot(prev.player_linear_velocity.x, prev.player_linear_velocity.y,
                                                 prev.player_linear_velocity.z) +
                                      std::hypot(n.player_linear_velocity.x, n.player_linear_velocity.y,
                                                 n.player_linear_velocity.z)) * kDt;
        t.his_jump_m = std::max(t.his_jump_m, dist3(prev.player_position, n.player_position) - carried);
        t.her_moved_m = std::max(t.her_moved_m, horizontal_distance(her0, m.position));
        t.his_moved_m = std::max(t.his_moved_m, horizontal_distance(him0, n.player_position));
        t.his_lowest_y = std::min(t.his_lowest_y, n.player_position.y);
        t.most_bends = std::max(t.most_bends, n.hook_bend_count);
        t.her_gravity_on = t.her_gravity_on && s.debug_matron_gravity_factor() == 1.0;
        t.her_supported = t.her_supported && m.contact_supported;
        t.fold = t.fold * 0.999 + m.position.x * 3.0 + m.position.y * 5.0 + m.position.z * 7.0 + n.player_position.x * 11.0 +
                 n.player_position.y * 13.0 + n.player_position.z * 17.0 + n.hook_tension_n * 1.0e-3;
        ++t.ticks;
        prev = n;
        her_prev = m;
    }
    t.tears = prev.hook_tear_count - tears0;
    t.stalls = prev.hook_stall_ticks - stalls0;
    return t;
}

void print(const char *name, const Tug &t) {
    std::printf("  %s: ticks=%d peak_tension_n=%.0f peak_feet_n=%.0f stalls=%llu tears=%llu her_moved_m=%.2f "
                "his_moved_m=%.2f his_lowest_y=%.2f bends=%u her_step_m=%.3f his_jump_m=%.3f supported=%d died=%d\n",
                name, t.ticks, t.peak_tension_n, t.peak_feet_n, static_cast<unsigned long long>(t.stalls),
                static_cast<unsigned long long>(t.tears), t.her_moved_m, t.his_moved_m, t.his_lowest_y, t.most_bends,
                t.her_step_m, t.his_jump_m, t.her_supported ? 1 : 0, t.he_died ? 1 : 0);
    std::fflush(stdout);
}

// Every tick of every pull: her one budget, her gravity on, no jump in her
// place or his (no position write: a tick at her 6.8 m/s hunt is 0.076 m;
// his moves are what his speed carries him).
void laws(const Tug &t, const std::string &what) {
    require(t.ticks > 0, (what + ": the pull ran").c_str());
    require(t.peak_feet_n <= static_cast<double>(law::kForceN) + 0.5, (what + ": her feet within her 8 kN").c_str());
    require(t.her_gravity_on, (what + ": her gravity stays on").c_str());
    require(t.her_step_m <= 0.2, (what + ": her every tick moves her at most 0.2 m: nothing writes her place").c_str());
    require(t.his_jump_m <= 0.05, (what + ": his every tick moves him no further than his speed carries him").c_str());
}

void require_bitten(const std::pair<Snapshot, MatronSnapshot> &bit, const char *what) {
    const std::string w(what);
    require(bit.first.hook_state == 2 && bit.first.hook_in_matron && bit.first.hook_matron_bite_count == 1,
            (w + ": the hook, fired at her, bites her body").c_str());
    const double gap = her_surface_gap(bit.first.hook_position, bit.second);
    std::printf("  %s: bite %.3f m off her collision surface\n", what, gap);
    require(std::abs(gap) <= 0.06, (w + ": the bite is on her collision surface, where the hook struck").c_str());
}

// Her planted on the flat band, his eyes on her (so she holds still, her
// stalk): he reels. The reel stalls at its kHookReelForceN and her feet
// answer it; she keeps the floor, and the stalled line drags him to her.
void planted() {
    Simulation s(InitialSpawn::Ring220North);
    const auto bit = hook_her(s, {{-12.0, kRingTop, -128.7}});
    require_bitten(bit, "planted");
    require(s.set_hook_reel(true), "planted: reel");
    const Tug t = pull(s, 90 * 4, [&](int) { face(s, s.matron_state().position); });
    print("planted", t);
    laws(t, "planted");
    require(t.stalls >= static_cast<std::uint64_t>(0.9 * t.ticks), "planted: her weight on her feet stalls the reel");
    require(t.peak_tension_n <= static_cast<double>(tuning::kHookReelForceN) * 1.01,
            "planted: the line pulls no harder than the reel turns (kHookReelForceN)");
    require(t.her_moved_m < 0.5, "planted: she keeps her floor");
    require(t.his_moved_m > 0.2, "planted: the stalled line drags him across the deck to her");
    require(t.tears == 0, "planted: a stalled reel tears nothing");
}

// Her within one step of the lip (her side 0.08 m from it), his eyes on her:
// he reels, stalls, then runs off the west band into the well. His weight
// and swing come onto the line, round the ring's edge, and onto her.
Tug at_the_lip(const bool report) {
    Simulation s(InitialSpawn::Ring220North);
    const auto bit = hook_her(s, {{-12.0, kRingTop, -130.35}, 3.1416, kWestX, kWestZ});
    if (report) require_bitten(bit, "lip");
    require(s.set_hook_reel(true), "lip: reel");
    const Tug stall = pull(s, 90, [&](int) { face(s, s.matron_state().position); });
    if (report) {
        print("lip, reeled", stall);
        laws(stall, "lip, reeled");
        require(stall.stalls >= 80 && stall.her_moved_m < 0.3, "lip: reeled from across the well, she stalls him");
    }
    const Tug t = pull(s, 90 * 4, [&](int i) { (void)s.set_move_input(i < 135 ? 1.0 : 0.0, 0.0); });
    if (report) {
        print("lip, his weight on the line", t);
        laws(t, "lip, his weight on the line");
        require(t.his_lowest_y < kRingTop - 2.0, "lip: he goes off the west band into the well, on the line");
        require(t.most_bends >= 1, "lip: the line to her runs over the ring's edge");
        require(t.peak_tension_n > 3000.0, "lip: his weight and swing load the line");
        require(t.her_moved_m > 0.5, "lip: the line drags her (her feet, not a write, decide how far)");
        require(t.her_supported, "lip: her feet keep her on the band: the line drags her along the lip, not off it");
        require(t.tears == 0, "lip: within the tines' hold, the bite holds");
    }
    return t;
}

// Her already moving across the lip line (hunting, his eyes off her): he
// fires as she goes and drops into the well. The snatch passes the tines'
// hold for a tick, short of tearing them out.
void crossing() {
    Simulation s(InitialSpawn::Ring220North);
    const auto bit = hook_her(s, {{-16.0, kRingTop, -130.4}, 1.5708, kWestX, kWestZ, false});
    require_bitten(bit, "crossing");
    std::printf("  crossing: her speed at the bite %.2f m/s\n", bit.second.speed);
    require(bit.second.speed > 1.0, "crossing: she is moving when the hook bites");
    require(s.set_hook_reel(true), "crossing: reel");
    const Tug t = pull(s, 90 * 4, [&](int i) { (void)s.set_move_input(i < 135 ? 1.0 : 0.0, 0.0); });
    print("crossing", t);
    laws(t, "crossing");
    require(t.her_moved_m > 1.0, "crossing: she carries on, the line on her");
    require(t.peak_tension_n > static_cast<double>(tuning::kHookHoldForceN),
            "crossing: the snatch passes the tines' hold");
    require(t.tears == 0, "crossing: a snatch short of kHookTearImpulseNs past the hold does not tear the bite");
    require(t.her_supported, "crossing: she keeps her feet");
}

// Fired over her head: the hook passes her and bites nothing of hers.
void miss() {
    Simulation s(InitialSpawn::Ring220North);
    Stage st{{-12.0, kRingTop, -128.7}};
    st.aim_up = 3.0;
    const auto bit = hook_her(s, st);
    require(bit.first.hook_matron_bite_count == 0 && !bit.first.hook_in_matron, "miss: a hook that passes her bites nothing of hers");
    require(bit.first.hook_fire_count == 1, "miss: one shot");
    std::printf("  miss: hook came down in state %u, bites %llu, none in her\n", static_cast<unsigned>(bit.first.hook_state),
                static_cast<unsigned long long>(bit.first.hook_bite_count));
}

// Overload: hooked into her from the rack, he walks in close to her along
// the lip (the rope stays as long as it was: slack) and steps off. Caught on
// the line, the fall's jolt is past kHookTearImpulseNs over the hold: the
// tines tear out of her, the hook springs back free, as off structure.
void tear() {
    Simulation s(InitialSpawn::Ring220North);
    const auto bit = hook_her(s, {{-12.0, kRingTop, -128.7}});
    require_bitten(bit, "tear");
    const double rope = bit.first.hook_line_length;
    require(walk_to(s, -14.6, -130.45, 6.0, 0.15), "tear: he walks in to the lip near her, the line slack");
    require(s.snapshot().hook_in_matron, "tear: still bitten");
    const Tug t = pull(s, 90 * 4, [&](int i) {
        (void)s.set_move_input(0.0, i < 30 ? -1.0 : 0.0);
        if (i == 0) face(s, {-14.6, kRingTop, -140.0});
    });
    print("tear", t);
    std::printf("  tear: %.2f m of rope, first past the hold at tick %d, torn at tick %d\n", rope, t.first_over_tick, t.tear_tick);
    laws(t, "tear");
    require(t.tears == 1, "tear: the fall on the line tears the hook out of her");
    require(t.first_over_tick >= 0 && t.tear_tick == t.first_over_tick,
            "tear: torn on the jolt past the tines' hold (kHookHoldForceN, kHookTearImpulseNs), as off structure");
    require(t.torn_free, "tear: torn out of her, the hook springs back in flight (free to catch again)");
}
} // namespace

void run_matron_hook() {
    planted();
    const Tug lip = at_the_lip(true);
    crossing();
    miss();
    tear();
    // Determinism: the lip again, from nothing: the same, tick for tick.
    const Tug again = at_the_lip(false);
    std::printf("  determinism: fold %.17g vs %.17g, ticks %d vs %d\n", lip.fold, again.fold, lip.ticks, again.ticks);
    require(lip.ticks == again.ticks && lip.fold == again.fold && lip.peak_tension_n == again.peak_tension_n &&
                lip.her_moved_m == again.her_moved_m,
            "matron-hook: two runs of the same tug are identical");
    std::cout << "PASS scraperx_sim matron-hook: planted, lip, crossing, miss, tear, deterministic\n";
}
SCRAPERX_SLICE("matron-hook", run_matron_hook);
