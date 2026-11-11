// J-038 (#169) measurement probe: the impulses the game's own world deals to
// loose things, for the dent and break limits Muse will set. Not a test: it
// changes nothing in the game and asserts nothing; it prints numbers.
//
// Build (off by default):
//   cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DSCRAPERX_BUILD_BRIDGE=ON
//     -DSCRAPERX_WITH_JOLT=ON -DGODOTCPP_TARGET=template_release -DSCRAPERX_BUILD_PROBES=ON
//   cmake --build build/host --target scraperx_j038_impulse_probe
//   build/host/scraperx_j038_impulse_probe
//
// What it reads. Jolt reports no applied contact impulse to a listener, and
// the game exposes none, so the impulse is read the way the ImpactLedger
// (src/sim/impacts.hpp) reads a hit: from the struck body's own velocity,
// tick by tick (90 Hz). Per tick, J = m |v_after - v_before| is the net
// impulse of every contact on the body that tick (the striker's, plus the
// ground's for a drop); its peak is the "peak per-tick impulse", and peak
// J * 90 Hz is the mean force over that tick. The "total" is m |dv| at the
// largest speed change within 1 s of first contact: the momentum the hit put
// into the body. The ledger's own hit (its speed change and energy) is
// printed beside it for a cross-check.
//
// Every case is run twice from a fresh world; the probe says whether the two
// runs gave bit-identical numbers.
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim_tests.hpp"

#include <cstdio>

using scraperx::sim::Forklift;
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

namespace {

constexpr double kDt = Simulation::kFixedStepSeconds;

struct Hit {
    double mass = 0.0;
    double striker_mps = 0.0;    // the striker's speed just before contact
    double peak_tick_ns = 0.0;   // peak per-tick impulse, N s
    double hit_ns = 0.0;         // m * largest |dv| within 0.2 s of contact: the hit itself
    double hit_dv_mps = 0.0;
    double total_ns = 0.0;       // m * largest |dv| within 1 s of contact: with any push or tumble after
    double dv_mps = 0.0;
    double ledger_dv = 0.0;      // the ImpactLedger's hit on the body (largest)
    double ledger_j = 0.0;
    int contact_tick = -1;
    bool operator==(const Hit &o) const {
        return mass == o.mass && striker_mps == o.striker_mps && peak_tick_ns == o.peak_tick_ns &&
               hit_ns == o.hit_ns && hit_dv_mps == o.hit_dv_mps && total_ns == o.total_ns && dv_mps == o.dv_mps && ledger_dv == o.ledger_dv && ledger_j == o.ledger_j &&
               contact_tick == o.contact_tick;
    }
};

double mag(const Vector3 &a, const Vector3 &b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

std::uint32_t piece(const Simulation &s, const std::uint32_t i) {
    return s.kit_body_index(LooseStock::kFirstEntity + i);
}

std::uint32_t first_of_kind(const Simulation &s, const LooseStock::Kind kind) {
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        if (s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(kind)) {
            return i;
        }
    }
    return UINT32_MAX;
}

void put(Simulation &s, const std::uint32_t body, const Vector3 at) {
    s.debug_launch_kit_body(body, at, {0.0, 0.0, 0.0});
}

// Watches `target` for up to `ticks`, calling `drive(tick, contact_tick)`
// before each step. Contact is the first tick its velocity changes by more
// than 0.05 m/s beyond what gravity alone does: for a body at rest
// (`in_flight` false) the ground already cancels gravity, so its raw change;
// for a body falling or thrown (`in_flight` true) its change less g dt, which
// is also then the contacts' impulse.
template <typename Drive, typename Striker>
Hit watch(Simulation &s, const std::uint32_t target, const int ticks, const bool in_flight, Drive drive,
          Striker striker) {
    Hit h;
    h.mass = s.kit_body_mass(target);
    const std::uint64_t entity = s.kit_body_entity(target);
    const std::uint64_t seen = s.last_impact_sequence();
    const double g_dt = in_flight ? 9.81 * kDt : 0.0;
    Vector3 pre{};
    for (int t = 0; t < ticks; ++t) {
        drive(t, h.contact_tick);
        const Vector3 v0 = s.kit_body_velocity(target);
        const double striker_before = striker();
        (void)s.advance_frame(kDt);
        const Vector3 v1 = s.kit_body_velocity(target);
        const double dv_tick = mag(v1, {v0.x, v0.y - g_dt, v0.z});
        if (h.contact_tick < 0 && dv_tick > 0.05) {
            h.contact_tick = t;
            h.striker_mps = striker_before;
            pre = v0;
        }
        if (h.contact_tick >= 0 && t - h.contact_tick < static_cast<int>(Simulation::kTickRateHz)) {
            h.peak_tick_ns = std::max(h.peak_tick_ns, h.mass * dv_tick);
            h.dv_mps = std::max(h.dv_mps, mag(v1, pre));
            if (t - h.contact_tick < static_cast<int>(0.2 * Simulation::kTickRateHz)) {
                h.hit_dv_mps = std::max(h.hit_dv_mps, mag(v1, pre));
            }
        }
    }
    h.total_ns = h.mass * h.dv_mps;
    h.hit_ns = h.mass * h.hit_dv_mps;
    for (const auto &hit : s.impacts()) {
        if (hit.sequence > seen && hit.entity == entity && hit.energy_j > h.ledger_j) {
            h.ledger_j = hit.energy_j;
            h.ledger_dv = hit.speed_mps;
        }
    }
    return h;
}

// ---- the forklift -----------------------------------------------------------

bool board(Simulation &s) {
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
    return s.forklift_state().driving;
}

enum class Target { Container, Drum, Crate };

// The yard's forklift at its wheel, the drums' pallet moved 40 m up its lane
// out of the way, `what` stood in the lane at z = 30 across the truck's path
// (the truck parks at z 16.5 facing +z), then full stick `pace` ahead until
// 0.2 s after contact, then let go.
Hit forklift_into(const Target what, const double pace) {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    if (!board(s)) {
        std::cerr << "probe: could not board the forklift\n";
        return {};
    }
    // The pallet of drums ahead of the truck (pieces 54..68 of the table),
    // moved as a group 40 m up the lane.
    const std::uint32_t pallet = first_of_kind(s, LooseStock::Kind::Pallet);
    for (std::uint32_t i = pallet; i < pallet + 15; ++i) {
        const auto b = piece(s, i);
        auto p = s.kit_body_position(b);
        p.z += 40.0;
        put(s, b, p);
    }
    std::uint32_t target = 0;
    if (what == Target::Container) {
        // The container at the lane's end (39, 70), along x, brought to z 30.
        std::uint32_t c = UINT32_MAX;
        for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
            const auto p = s.kit_body_position(piece(s, i));
            if (s.loose_stock_piece(i).kind == 5 && std::abs(p.x - 39.0) < 0.1 && std::abs(p.z - 70.0) < 0.1) {
                c = i;
            }
        }
        target = piece(s, c);
        put(s, target, {38.0, 1.31, 30.0});
    } else if (what == Target::Drum) {
        target = piece(s, pallet + 1);   // a drum off the moved pallet
        put(s, target, {38.0, 0.47, 30.0});
    } else {
        target = piece(s, LooseStock::kDeckCrateNorth);   // the 160 kg timber crate
        put(s, target, {38.0, 0.46, 30.0});
    }
    (void)s.advance_frame(1.5);   // settled
    (void)s.set_facing(0.0, 1.0);
    const int hold_after = static_cast<int>(0.2 * Simulation::kTickRateHz);
    Hit h = watch(
        s, target, 20 * static_cast<int>(Simulation::kTickRateHz), false,
        [&](const int t, const int contact) {
            const bool held = contact < 0 || t - contact < hold_after;
            (void)s.set_move_input(0.0, held ? pace : 0.0);
        },
        [&]() { return s.forklift_state().speed_mps; });
    (void)s.set_move_input(0.0, 0.0);
    return h;
}

// ---- drops (the crane's load let go 1 m up) ---------------------------------

// `load` raised 1 m above `rest` (where its centre lies at rest on what it
// falls onto) and let go from still.
Hit drop(const std::uint32_t load_piece, const bool onto_container) {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const auto load = piece(s, load_piece);
    auto p = s.kit_body_position(load);
    if (onto_container) {
        // The container at (-80.5, -113), its roof at 2.59 m.
        p = {-80.5, 2.59 + (p.y - 0.30) + 1.0, -113.0};
    } else {
        p.y += 1.0;
    }
    put(s, load, p);
    return watch(
        s, load, 3 * static_cast<int>(Simulation::kTickRateHz), true, [](int, int) {},
        [&]() { return std::abs(s.kit_body_velocity(load).y); });
}

// ---- her thrown stock -------------------------------------------------------

// The toolbox she throws (Matron::kThrowMps 7.5 m/s), sent level from 1 m
// short of `what` stood in the open lane south of the containers at (0, 64).
Hit thrown_into(const Target what, const bool measure_toolbox) {
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    const std::uint32_t n = s.loose_stock_count();
    const auto toolbox = piece(s, n - LooseStock::kMatronThrowables);
    std::uint32_t target = 0;
    double rest_y = 0.0;
    double half = 0.0;
    if (what == Target::Drum) {
        target = piece(s, n - 1);   // her deck's drum
        rest_y = 0.47;
        half = 0.30;
    } else if (what == Target::Crate) {
        target = piece(s, LooseStock::kDeckCrateNorth);
        rest_y = 0.46;
        half = 0.48;
    } else {
        // The container at (1.5, 70) where it lies.
        for (std::uint32_t i = 0; i < n; ++i) {
            const auto p = s.kit_body_position(piece(s, i));
            if (s.loose_stock_piece(i).kind == 5 && std::abs(p.x - 1.5) < 0.1 && std::abs(p.z - 70.0) < 0.1) {
                target = piece(s, i);
            }
        }
        rest_y = 1.295;
        half = 1.22;
    }
    const double tz = what == Target::Container ? 70.0 : 64.0;
    if (what != Target::Container) {
        put(s, target, {1.5, rest_y, tz});
    }
    (void)s.advance_frame(1.5);
    const double y = what == Target::Container ? 1.3 : rest_y;
    s.debug_launch_kit_body(toolbox, {1.5, y, tz - half - 1.0}, {0.0, 0.0, 7.5});
    const auto watched = measure_toolbox ? toolbox : target;
    // The toolbox is in flight from the first tick: its contact is the first
    // tick it loses more than 0.05 m/s, which gravity alone never does.
    return watch(
        s, watched, 2 * static_cast<int>(Simulation::kTickRateHz), measure_toolbox, [](int, int) {},
        [&]() { return mag(s.kit_body_velocity(toolbox), {0.0, 0.0, 0.0}); });
}

template <typename Case>
void report(const char *name, Case run) {
    const Hit a = run();
    const Hit b = run();
    std::printf("%-44s m=%8.1f kg  striker=%5.2f m/s  peak/tick=%9.1f N.s (%8.0f N)  hit(0.2s)=%9.1f N.s dv=%5.2f  "
                "1s=%9.1f N.s dv=%5.2f m/s  "
                "ledger dv=%5.2f E=%8.1f J  tick=%d  rerun=%s\n",
                name, a.mass, a.striker_mps, a.peak_tick_ns, a.peak_tick_ns * Simulation::kTickRateHz, a.hit_ns,
                a.hit_dv_mps, a.total_ns,
                a.dv_mps, a.ledger_dv, a.ledger_j, a.contact_tick, a == b ? "identical" : "DIFFERENT");
    std::fflush(stdout);
}

} // namespace

int main() {
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        std::printf("forklift chassis mass %.1f kg (wheels add 100 kg)\n",
                    s.kit_body_mass(s.kit_body_index(Forklift::kChassisEntity)));
    }
    report("forklift full stick -> container", [] { return forklift_into(Target::Container, 1.0); });
    report("forklift full stick -> drum", [] { return forklift_into(Target::Drum, 1.0); });
    report("forklift full stick -> crate", [] { return forklift_into(Target::Crate, 1.0); });
    report("forklift creep 0.25 -> container", [] { return forklift_into(Target::Container, 0.25); });
    report("forklift creep 0.25 -> drum", [] { return forklift_into(Target::Drum, 0.25); });
    report("forklift creep 0.25 -> crate", [] { return forklift_into(Target::Crate, 0.25); });
    report("drop 1 m: container onto ground", [] { return drop(83, false); });
    report("drop 1 m: 2 t plate slab onto slab", [] { return drop(318, false); });
    report("drop 1 m: 2 t plate slab onto container", [] { return drop(318, true); });
    report("drop 1 m: 600 kg steel beam onto ground", [] { return drop(319, false); });
    report("her throw 7.5 m/s: toolbox -> drum", [] { return thrown_into(Target::Drum, false); });
    report("her throw 7.5 m/s: toolbox -> crate", [] { return thrown_into(Target::Crate, false); });
    // On a 3.75 t container the toolbox's blow moves it less than the 0.05 m/s
    // contact mark, so the impulse is read off the toolbox (equal, opposite).
    report("her throw 7.5 m/s: the toolbox itself (container)", [] { return thrown_into(Target::Container, true); });
    report("her throw 7.5 m/s: the toolbox itself (drum)", [] { return thrown_into(Target::Drum, true); });
    return 0;
}
