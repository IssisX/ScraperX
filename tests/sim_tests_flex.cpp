// J-096 slice 1 (#414): a plank bends under real weight, springs back, and
// snaps past its strength (sim/flex_plank.hpp). One C24 scaffold board,
// 225 x 38 mm, clamped flush with a block's top and out 1.2 m over open
// ground by the spawn, in a test-only world (the plank and its block are
// built after every body of the world; the world as built holds neither).
//   him: walks out to its end; the end drops within 10% of the beam formula
//        for his weight where he stands; he walks back and it returns within
//        1 cm of rest.
//   her: put on its end; her moment against its rupture moment (snap or hold).
//   a drum: a loose 55 kg drum stood on its end sags it and it holds.
//   determinism: his walk twice, bit for bit.
#include "sim_tests.hpp"
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
#include "sim/physics_world_tuning.hpp"

#include <cstdio>
#include <cstring>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr double kG = 9.81;
// Open ground east of the spawn (6, 0, -25): nothing within 4 m, the grade
// 2 m below the plank.
constexpr Vector3 kRoot{0.0, 2.0, -32.0};
constexpr double kLength = 1.2;
constexpr double kWidth = 0.225;
constexpr double kThick = 0.038;
// The board's clamp: a block 2 x 2 x 2 m whose top is the plank's top face,
// its east face 1 cm short of the plank's root.
constexpr double kClampGap = 0.01;
// EN 338 C24 (flex_plank.hpp), restated so the sim's numbers are checked.
constexpr double kE = 11.0e9;
constexpr double kFm = 24.0e6;

double second_moment() { return kWidth * kThick * kThick * kThick / 12.0; }

// A cantilever's free end under P at a from the clamp (a <= L).
double tip_formula(const double load_n, const double a) {
    return load_n * a * a * (3.0 * kLength - a) / (6.0 * kE * second_moment());
}

void tick(Simulation &s) {
    require(s.advance_frame(Simulation::kFixedStepSeconds).steps_advanced == 1, "flex plank: one tick, one step");
    if (std::getenv("FLEX_TRACE") != nullptr && s.flex_plank_count() > 0) {
        const auto v = s.flex_plank(0);
        const auto me = s.snapshot();
        std::printf("  t up=%llu dn=%llu bal=%d vy=%.3f p=(%.3f %.3f) sup=%llu g=%d tip=(%.3f %.4f) m=[%.0f %.0f %.0f %.0f] snap=%d\n",
                    static_cast<unsigned long long>(me.step_up_count), static_cast<unsigned long long>(me.step_down_count),
                    int(me.player_balancing), me.player_linear_velocity.y, me.player_position.x, me.player_position.y,
                    static_cast<unsigned long long>(me.support_entity_id), int(me.player_grounded), v.tip.x, v.tip.y,
                    v.moment_nm[0], v.moment_nm[1], v.moment_nm[2], v.moment_nm[3], v.snapped_joint);
    }
}

void ticks(Simulation &s, const double seconds) {
    const int n = static_cast<int>(seconds * Simulation::kTickRateHz + 0.5);
    for (int i = 0; i < n; ++i) tick(s);
}

// walk_to (sim_tests.hpp), a tick at a time through tick().
bool walk(Simulation &s, const double x, const double z, const double budget_seconds, const double tolerance) {
    const int n = static_cast<int>(budget_seconds * Simulation::kTickRateHz);
    for (int i = 0; i < n; ++i) {
        const auto me = s.snapshot();
        if (std::hypot(x - me.player_position.x, z - me.player_position.z) <= tolerance) {
            (void)s.set_move_input(0.0, 0.0);
            return true;
        }
        steer_toward(s, x, z, 0.35);   // a careful walk out, not a run
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

std::vector<Simulation::TestExtraSolid> clamp_block() {
    return {Simulation::TestExtraSolid{kRoot.x - kClampGap - 1.0, kRoot.y - 1.0, kRoot.z, 1.0F, 1.0F, 1.0F, true}};
}

// The world with the block and the plank, settled under its own weight.
struct Yard final {
    Simulation s{InitialSpawn::ExteriorGrade, clamp_block()};
    double rest_tip_y = 0.0;
    Yard() {
        require(s.debug_add_flex_plank(kRoot, 1.0, 0.0, kLength, kWidth, kThick) == 0, "flex plank: the plank is built");
        ticks(s, 1.0);
        rest_tip_y = s.flex_plank(0).tip.y;
    }
    double drop() const { return rest_tip_y - s.flex_plank(0).tip.y; }
};

// The settle after a load lands or leaves: the tip's drop each tick for
// `seconds`, and how long it took to come within 5% (or 2 mm) of where it
// ends, and how far it went past that.
struct Settle final {
    double final_drop = 0.0;
    double settle_s = 0.0;
    double overshoot_m = 0.0;
};

Settle settle(Yard &y, const double seconds) {
    std::vector<double> drops;
    const int n = static_cast<int>(seconds * Simulation::kTickRateHz + 0.5);
    for (int i = 0; i < n; ++i) {
        tick(y.s);
        drops.push_back(y.drop());
    }
    Settle out;
    out.final_drop = drops.back();
    const double band = std::max(0.002, 0.05 * std::abs(out.final_drop));
    int last_out = -1;
    for (int i = 0; i < n; ++i) {
        if (std::abs(drops[static_cast<std::size_t>(i)] - out.final_drop) > band) last_out = i;
        out.overshoot_m = std::max(out.overshoot_m, (drops[static_cast<std::size_t>(i)] - out.final_drop) *
                                                        (out.final_drop >= 0.0 ? 1.0 : -1.0));
    }
    out.settle_s = static_cast<double>(last_out + 1) / static_cast<double>(Simulation::kTickRateHz);
    return out;
}

void check_beam_numbers(const Simulation::FlexPlankView &v) {
    const double I = second_moment();
    const double K = 3.0 * kE * I / (kLength * kLength * kLength);
    const double k = kE * I * 5.0 * 9.0 / (2.0 * 4.0 * kLength);   // (n+1)(2n+1)/(2nL), n = 4
    const double Mr = kFm * kWidth * kThick * kThick / 6.0;
    require(std::abs(v.tip_stiffness_n_per_m - K) <= 1.0e-4 * K, "flex plank: the tip stiffness is 3EI/L^3");
    require(std::abs(v.joint_stiffness_nm_per_rad - k) <= 1.0e-4 * k,
            "flex plank: each hinge's spring is EI(n+1)(2n+1)/(2nL)");
    require(std::abs(v.rupture_moment_nm - Mr) <= 1.0e-4 * Mr, "flex plank: the rupture moment is f_m b h^2 / 6");
    require(std::abs(v.segment_mass_kg - 420.0 * kWidth * kThick * kLength / 4.0) <= 1.0e-4,
            "flex plank: the segments weigh C24's density");
}

struct HimRun final {
    double a = 0.0;            // his distance out from the clamp, standing at the end
    double loaded_drop = 0.0;
    double formula = 0.0;
    double root_moment = 0.0;
    double rupture = 0.0;
    double peak_moment = 0.0;
    Settle on, off;
    double back_drop = 0.0;
    bool snapped = false;
    std::vector<double> bits;
};

HimRun run_him() {
    Yard y;
    check_beam_numbers(y.s.flex_plank(0));
    // On the block a metre back from the plank, facing out along it.
    y.s.debug_launch_player({kRoot.x - 1.0, kRoot.y + scraperx::sim::detail::kPlayerHalfHeight + 0.02, kRoot.z}, {0.0, 0.0, 0.0});
    (void)y.s.set_facing(1.0, 0.0);
    ticks(y.s, 1.0);
    require(std::abs(y.drop()) < 0.002, "flex plank: him on the block leaves the plank at rest");
    require(walk(y.s, kRoot.x + kLength - 0.08, kRoot.z, 8.0, 0.05), "flex plank: he walks out to the plank's end");
    HimRun out;
    out.on = settle(y, 1.5);
    const auto me = y.s.snapshot();
    require(me.player_grounded, "flex plank: he stands on the end");
    const auto v = y.s.flex_plank(0);
    std::printf("FLEX him at end: p=(%.3f %.3f %.3f) support=%llu seg3=%llu drop=%.4f m0=%.0f snapped=%d\n",
                me.player_position.x, me.player_position.y, me.player_position.z,
                static_cast<unsigned long long>(me.support_entity_id),
                static_cast<unsigned long long>(v.segment_entity[3]), y.drop(), v.moment_nm[0], v.snapped_joint);
    require(v.segment_entity[3] == me.support_entity_id || v.segment_entity[2] == me.support_entity_id,
            "flex plank: he stands on its outer segments");
    out.a = me.player_position.x - kRoot.x;
    out.loaded_drop = y.drop();
    out.formula = tip_formula(scraperx::sim::detail::kPlayerMassKg * kG, out.a);
    out.root_moment = v.moment_nm[0];
    out.rupture = v.rupture_moment_nm;
    out.peak_moment = v.peak_moment_nm;
    out.snapped = v.snapped_joint >= 0;
    // Back onto the block.
    require(walk(y.s, kRoot.x - 0.8, kRoot.z, 8.0, 0.05), "flex plank: he walks back onto the block");
    out.off = settle(y, 1.5);
    out.back_drop = y.drop();
    const auto w = y.s.flex_plank(0);
    for (int i = 0; i < 4; ++i) {
        const auto n = static_cast<std::size_t>(i);
        for (const double c : {w.segment_position[n].x, w.segment_position[n].y, w.segment_position[n].z,
                               w.segment_rotation[n].x, w.segment_rotation[n].y, w.segment_rotation[n].z,
                               w.segment_rotation[n].w}) {
            out.bits.push_back(c);
        }
    }
    const auto end = y.s.snapshot().player_position;
    out.bits.push_back(end.x);
    out.bits.push_back(end.y);
    out.bits.push_back(end.z);
    return out;
}

void test_him() {
    const HimRun r = run_him();
    std::printf("FLEX him: a=%.3f m drop=%.4f m formula=%.4f m (err %+.1f%%) root_moment=%.0f N m "
                "rupture=%.0f N m peak=%.0f N m settle_on=%.2f s overshoot=%.4f m | off: back_drop=%.4f m "
                "settle_off=%.2f s\n",
                r.a, r.loaded_drop, r.formula, 100.0 * (r.loaded_drop / r.formula - 1.0), r.root_moment, r.rupture,
                r.peak_moment, r.on.settle_s, r.on.overshoot_m, r.back_drop, r.off.settle_s);
    require(!r.snapped, "flex plank: his weight at the end does not snap it");
    require(std::abs(r.loaded_drop - r.formula) <= 0.10 * r.formula,
            "flex plank: his weight drops the end within 10% of P a^2 (3L - a) / (6EI)");
    require(r.on.settle_s < 1.0, "flex plank: under him it settles within 1 s");
    require(std::abs(r.back_drop) <= 0.01, "flex plank: he steps off and it returns within 1 cm of rest");
    require(r.off.settle_s < 1.0, "flex plank: unloaded it settles within 1 s");
    const HimRun again = run_him();
    require(r.bits.size() == again.bits.size() &&
                std::memcmp(r.bits.data(), again.bits.data(), r.bits.size() * sizeof(double)) == 0,
            "flex plank: his walk out and back twice ends bit for bit the same");
    std::cout << "PASS scraperx_sim flex-plank him: drop " << r.loaded_drop << " m vs formula " << r.formula
              << " m; back within " << std::abs(r.back_drop) << " m; deterministic\n";
}

void test_her() {
    Yard y;
    const double a = kLength - 0.15;
    y.s.debug_place_matron({kRoot.x + a, kRoot.y, kRoot.z});
    double peak_drop = 0.0;
    int snap_tick = -1;
    for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        tick(y.s);
        if (y.s.flex_plank(0).snapped_joint < 0) {
            peak_drop = std::max(peak_drop, y.drop());
        } else if (snap_tick < 0) {
            snap_tick = i;
        }
    }
    const auto v = y.s.flex_plank(0);
    const double her_moment = static_cast<double>(Matron::kMassKg) * kG * a;
    const double her_formula = tip_formula(static_cast<double>(Matron::kMassKg) * kG, a);
    const double his_formula = tip_formula(scraperx::sim::detail::kPlayerMassKg * kG, a);
    std::printf("FLEX her: a=%.3f m static moment=%.0f N m rupture=%.0f N m -> %s; her formula drop=%.4f m "
                "(%.3f x his %.4f m) peak drop before the break=%.4f m snapped_joint=%d at %.3f s, moment %.0f N m\n",
                a, her_moment, v.rupture_moment_nm, her_moment > v.rupture_moment_nm ? "snaps" : "holds", her_formula,
                her_formula / his_formula, his_formula, peak_drop, v.snapped_joint,
                snap_tick < 0 ? -1.0 : static_cast<double>(snap_tick + 1) / Simulation::kTickRateHz, v.snap_moment_nm);
    require(std::abs(her_formula / his_formula - static_cast<double>(Matron::kMassKg) / scraperx::sim::detail::kPlayerMassKg) <
                1.0e-9,
            "flex plank: her drop is his by her mass ratio");
    if (her_moment > v.rupture_moment_nm) {
        require(v.snapped_joint == 0, "flex plank: her weight past its rupture moment snaps it at the clamp");
        require(v.snap_moment_nm > v.rupture_moment_nm, "flex plank: it broke past the rupture moment");
        // The pieces are loose: in a second they are well below where they hung.
        ticks(y.s, 1.0);
        const auto w = y.s.flex_plank(0);
        for (int i = 0; i < 4; ++i) {
            require(w.segment_position[static_cast<std::size_t>(i)].y < kRoot.y - 0.5,
                    "flex plank: the snapped pieces fall loose");
        }
    } else {
        require(v.snapped_joint < 0, "flex plank: under its rupture moment it holds her");
    }
    std::cout << "PASS scraperx_sim flex-plank her: " << (v.snapped_joint >= 0 ? "snapped" : "held") << '\n';
}

void test_drum() {
    Yard y;
    std::uint32_t drum = Simulation::kKitNone;
    for (std::uint32_t i = 0; i < y.s.loose_stock_count() && drum == Simulation::kKitNone; ++i) {
        if (y.s.loose_stock_piece(i).kind == static_cast<std::uint8_t>(LooseStock::Kind::Drum)) {
            const std::uint32_t body = y.s.kit_body_index(Simulation::kLooseStockFirstEntity + i);
            if (body != Simulation::kKitNone && y.s.kit_body_enabled(body) && y.s.kit_body_dynamic(body)) {
                drum = body;
            }
        }
    }
    require(drum != Simulation::kKitNone, "flex plank: a loose drum is in the world");
    const double mass = y.s.kit_body_mass(drum);
    require(std::abs(mass - 55.0) < 0.01, "flex plank: the drum weighs 55 kg");
    const double a = kLength - 0.30;
    y.s.debug_launch_kit_body(drum, {kRoot.x + a, kRoot.y + 0.44 + 0.005, kRoot.z}, {0.0, 0.0, 0.0});
    const Settle st = settle(y, 2.0);
    const auto v = y.s.flex_plank(0);
    const auto at = y.s.kit_body_position(drum);
    std::printf("FLEX drum body %u at (%.3f %.3f %.3f)\n", drum, at.x, at.y, at.z);
    const double on_a = at.x - kRoot.x;
    const double formula = tip_formula(mass * kG, on_a);
    std::printf("FLEX drum: a=%.3f m drop=%.4f m formula=%.4f m (err %+.1f%%) root_moment=%.0f N m rupture=%.0f N m "
                "settle=%.2f s overshoot=%.4f m snapped_joint=%d\n",
                on_a, y.drop(), formula, 100.0 * (y.drop() / formula - 1.0), v.moment_nm[0], v.rupture_moment_nm,
                st.settle_s, st.overshoot_m, v.snapped_joint);
    require(v.snapped_joint < 0, "flex plank: the drum does not snap it");
    require(at.y > kRoot.y && std::abs(at.z - kRoot.z) < 0.1, "flex plank: the drum stays stood on it");
    require(std::abs(y.drop() - formula) <= 0.10 * formula, "flex plank: the drum sags it by the beam formula");
    std::cout << "PASS scraperx_sim flex-plank drum: sags " << y.drop() << " m and holds\n";
}

// The plank's cost: the same world (block and spawn) stepped 4 s with him
// standing still, once on the block with no plank built and once on the
// plank's end. Logged, not asserted (a host's timer; the phone's is J-044's).
struct Cost final {
    double step_us = 0.0;
    double physics_us = 0.0;
};

Cost measure_cost(const bool with_plank) {
    Simulation s(InitialSpawn::ExteriorGrade, clamp_block());
    if (with_plank) {
        require(s.debug_add_flex_plank(kRoot, 1.0, 0.0, kLength, kWidth, kThick) == 0, "flex plank: cost: built");
    }
    const double x = with_plank ? kRoot.x + kLength - 0.12 : kRoot.x - 0.8;
    s.debug_launch_player({x, kRoot.y + scraperx::sim::detail::kPlayerHalfHeight + 0.02, kRoot.z}, {0.0, 0.0, 0.0});
    ticks(s, 2.0);
    std::vector<double> step, physics;
    const int n = 4 * static_cast<int>(Simulation::kTickRateHz);
    for (int i = 0; i < n; ++i) {
        tick(s);
        step.push_back(s.step_timing().step.last_us);
        physics.push_back(s.step_timing().physics.last_us);
    }
    std::sort(step.begin(), step.end());
    std::sort(physics.begin(), physics.end());
    // The tenth percentile: the step's own cost, a busy host's scheduler kept out.
    return {step[step.size() / 10], physics[physics.size() / 10]};
}

void test_cost() {
    Cost without{}, with{};
    constexpr int kRuns = 5;
    for (int run = 0; run < kRuns; ++run) {
        const Cost a = measure_cost(false);
        const Cost b = measure_cost(true);
        without.step_us += a.step_us / kRuns;
        without.physics_us += a.physics_us / kRuns;
        with.step_us += b.step_us / kRuns;
        with.physics_us += b.physics_us / kRuns;
    }
    std::printf("PERF flex-plank step_p10_us without=%.1f with=%.1f (+%.1f us = +%.0f ns/tick); physics_p10_us "
                "without=%.1f with=%.1f (+%.1f us)\n",
                without.step_us, with.step_us, with.step_us - without.step_us,
                1000.0 * (with.step_us - without.step_us), without.physics_us, with.physics_us,
                with.physics_us - without.physics_us);
}

void run_flex_plank() {
    test_him();
    test_her();
    test_drum();
    test_cost();
}
} // namespace

SCRAPERX_SLICE("flex-plank", run_flex_plank);
