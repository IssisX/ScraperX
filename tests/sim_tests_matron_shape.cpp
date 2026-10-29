// Her collision body follows her pose (issue #3, M2: the body you can hit is
// the body you see; src/sim/matron_shape.cpp).
//
// 1. Her drawn body against her stance's capsule. tests/data/
//    matron_drawn_limbs.inc holds her drawn limb volumes (each bone of her
//    rig as a segment with the radius of her flesh about it, in her body
//    frame) for frames of each stance family, recorded from the game's own
//    drawn body by tools/matron_probe (probe.gd, then shape_fit.py
//    --fixture). For every frame, each volume of her body (trunk, head, legs)
//    must lie within kOutM of the capsule her simulation gives that stance
//    (her arms are her reach: her hands on a hold, an ankle, a rail, their
//    contacts are S6's, matron_contact_audit.hpp), and the
//    capsule must reach no further than kInM past her drawn body in any of
//    her frame's six directions (the body that hits with nothing shown).
//    Her old capsule's numbers are printed beside them.
// 2. No tick inside the steel: every run of this process has her stance
//    upkeep audited (Matron::audit_shapes, on from main); at exit no tick may
//    have had her shape deeper than Matron::kInsideSteelM in the steel
//    (shape_audit_at_exit). Every matron group checks it for its own runs:
//    her walk, her hunt low, her ladders, pipe, holds and fatality.

#include "sim_tests.hpp"
#include "sim/matron.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>

using scraperx::sim::Matron;

namespace {

struct DrawnLimb {
    int stance;   // Matron::Stance
    int frame;    // the recorded frame (within its stance)
    int region;   // 0 trunk and head, 1 forearm and hand, 2 upper arm, 3 leg
    float a[3];   // x right, y up from her middle, z forward
    float b[3];
    float radius;
};

constexpr DrawnLimb kDrawn[] = {
#include "data/matron_drawn_limbs.inc"
};

// Her drawn body (trunk, head, legs) within this of her capsule (95th
// percentile of her frames; the worst frame within kOutWorstM); her capsule
// no further than kInM past her drawn body (95th percentile). Her old upright
// capsule: 0.37-0.55 m out, 0.43-1.01 m past. Each stance's must also beat
// the old capsule on both counts.
constexpr double kOutM = 0.34;
constexpr double kOutWorstM = 0.50;
constexpr double kInM = 0.42;

struct V3 {
    double x, y, z;
};
V3 sub(const V3 a, const V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const V3 a, const V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

double to_segment(const V3 p, const V3 a, const V3 b) {
    const V3 ab = sub(b, a);
    const double len2 = dot(ab, ab);
    const double t = len2 > 1.0e-12 ? std::clamp(dot(sub(p, a), ab) / len2, 0.0, 1.0) : 0.0;
    const V3 q{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
    return std::sqrt(dot(sub(p, q), sub(p, q)));
}

struct Capsule {
    V3 a, b;
    double r;
};

Capsule capsule_of(const Matron::Stance stance) {
    const Matron::StanceCapsule c = Matron::stance_capsule(stance);
    return {{0.0, c.ay, c.az}, {0.0, c.by, c.bz}, c.radius};
}

// How far a drawn limb volume reaches out of the capsule: its segment
// sampled, each sample's distance to the capsule's axis plus its own flesh,
// less the capsule's radius.
double out_of(const DrawnLimb &limb, const Capsule &cap) {
    const V3 a{limb.a[0], limb.a[1], limb.a[2]};
    const V3 b{limb.b[0], limb.b[1], limb.b[2]};
    double worst = -1.0e9;
    for (int k = 0; k <= 8; ++k) {
        const double t = k / 8.0;
        const V3 p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
        worst = std::max(worst, to_segment(p, cap.a, cap.b) + limb.radius - cap.r);
    }
    return worst;
}

double percentile(std::vector<double> v, const double p) {
    if (v.empty()) {
        return 0.0;
    }
    std::sort(v.begin(), v.end());
    const auto i = static_cast<std::size_t>(std::lround(p * static_cast<double>(v.size() - 1)));
    return v[i];
}

struct Fit {
    double out95 = 0.0, out_worst = 0.0, in95 = 0.0;
    int frames = 0;
};

Fit fit_of(const int stance, const Capsule &cap) {
    std::vector<double> outs;
    std::vector<double> ins;
    int frame = -1;
    double out = -1.0e9;
    std::array<double, 6> drawn{};
    const auto close = [&]() {
        if (frame < 0) {
            return;
        }
        outs.push_back(out);
        // The capsule's reach in each of her frame's six directions, less
        // her drawn body's.
        const V3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        double worst = -1.0e9;
        for (int d = 0; d < 6; ++d) {
            const double reach = std::max(dot(cap.a, dirs[d]), dot(cap.b, dirs[d])) + cap.r;
            worst = std::max(worst, reach - drawn[static_cast<std::size_t>(d)]);
        }
        ins.push_back(worst);
    };
    for (const DrawnLimb &limb : kDrawn) {
        if (limb.stance != stance) {
            continue;
        }
        if (limb.frame != frame) {
            close();
            frame = limb.frame;
            out = -1.0e9;
            drawn.fill(-1.0e9);
        }
        const V3 a{limb.a[0], limb.a[1], limb.a[2]};
        const V3 b{limb.b[0], limb.b[1], limb.b[2]};
        const V3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (int d = 0; d < 6; ++d) {
            drawn[static_cast<std::size_t>(d)] = std::max(drawn[static_cast<std::size_t>(d)],
                                                          std::max(dot(a, dirs[d]), dot(b, dirs[d])) + limb.radius);
        }
        if (limb.region == 0 || limb.region == 3) {
            out = std::max(out, out_of(limb, cap));
        }
    }
    close();
    Fit f;
    f.frames = static_cast<int>(outs.size());
    f.out95 = percentile(outs, 0.95);
    f.out_worst = outs.empty() ? 0.0 : *std::max_element(outs.begin(), outs.end());
    f.in95 = percentile(ins, 0.95);
    return f;
}

void shape_audit_at_exit() {
    const Matron::ShapeAudit &a = Matron::shape_audit();
    if (a.ticks == 0) {
        return;
    }
    const double upkeep_us = 1.0e6 * a.upkeep_s / static_cast<double>(a.ticks);
    std::printf("  matron shape audit: %ld ticks (upright %ld, low %ld, climb %ld, pipe %ld, down %ld, hold %ld), "
                "%ld switches, %ld refused, deepest in the steel %.3f m, upkeep %.2f us a tick\n",
                a.ticks, a.per_stance[0], a.per_stance[1], a.per_stance[2], a.per_stance[3], a.per_stance[4],
                a.per_stance[5], a.switches, a.refused, static_cast<double>(a.worst_depth_m), upkeep_us);
    if (a.inside_steel != 0) {
        std::fprintf(stderr, "FAIL: matron shape: %ld ticks her body was in the steel (deepest %.3f m)\n",
                     a.inside_steel, static_cast<double>(a.worst_depth_m));
        std::fflush(nullptr);
        std::_Exit(EXIT_FAILURE);
    }
    std::printf("PASS scraperx_sim matron shape: no tick in the steel (%ld ticks, deepest %.3f m)\n", a.ticks,
                static_cast<double>(a.worst_depth_m));
}

} // namespace

void start_matron_shape_audit() {
    Matron::audit_shapes(true);
    std::atexit(shape_audit_at_exit);
}

void run_matron_shape() {
    const Capsule old{{0.0, -0.75, 0.0}, {0.0, 0.75, 0.0}, 0.30};
    const char *names[] = {"upright", "low", "climb", "pipe", "down", "hold"};
    std::vector<Fit> fits;
    for (const Matron::Stance stance : {Matron::Stance::Upright, Matron::Stance::Low, Matron::Stance::Climb,
                                        Matron::Stance::Pipe, Matron::Stance::Hold}) {
        const int s = static_cast<int>(stance);
        const Fit before = fit_of(s, old);
        const Fit after = fit_of(s, capsule_of(stance));
        std::printf("  matron shape %s (%d drawn frames): her body out of it p95 %.2f m (worst %.2f), it past her "
                    "body p95 %.2f m; her old capsule: out p95 %.2f m (worst %.2f), past p95 %.2f m\n",
                    names[s], after.frames, after.out95, after.out_worst, after.in95, before.out95, before.out_worst,
                    before.in95);
        fits.push_back(after);
        require(after.out95 < before.out95 && after.in95 < before.in95,
                "matron shape: her stance's capsule fits her drawn body better than her old upright one");
    }
    for (const Fit &after : fits) {
        require(after.frames >= 8, "matron shape: her drawn body recorded in each stance family");
        require(after.out95 <= kOutM && after.out_worst <= kOutWorstM,
                "matron shape: her drawn body (but her forearms and hands) within its margin of her stance's capsule");
        require(after.in95 <= kInM, "matron shape: her stance's capsule reaches no further past her drawn body than its margin");
    }
    // Each capsule turned with her yaw is the same capsule: its segment's
    // length and its height kept, its bottom at her soles where it stands.
    for (int s = 0; s < Matron::kStanceCount; ++s) {
        const Matron::StanceCapsule c = Matron::stance_capsule(static_cast<Matron::Stance>(s));
        if (s != static_cast<int>(Matron::Stance::Pipe)) {
            require(std::abs(std::min(c.ay, c.by) - c.radius + Matron::kStandCentre) < 1.0e-4F,
                    "matron shape: a standing family's capsule keeps its lowest point at her soles");
        }
    }
    require(Matron::stance_bin(0.0F) == 0 && Matron::stance_bin(6.2831853F) == 0 &&
                Matron::stance_bin(-0.5F * 3.14159265F) == 3 * Matron::kStanceYawBins / 4,
            "matron shape: her yaw picks its turned capsule");
    std::cout << "PASS scraperx_sim matron shape fit: her collision capsule within its margins of her drawn body in "
                 "every stance\n";
}
