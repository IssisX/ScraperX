// The Matron chasing the climber across the tower on her own ways
// (sim/matron.cpp, the chase-time rule): one sound, then silence, or a sound
// every 8 s, and she must still get to the place she heard it, by her own
// hands and feet, alive, under gravity, never carried or snapped, every
// force inside traversal::Limits. Seeds come from her own scanned map
// (matron_affordances): a start region and a spot region her planner can
// reach, so no seed is a place, a route or a deck picked by hand.

#include "sim_tests.hpp"

#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;
using scraperx::sim::traversal::AffordanceMap;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

constexpr double kForceLimitN = law::kForceN + 0.5;  // the owner's law (sim_tests.hpp), with float slack
constexpr double kPowerLimitW = law::kPowerW + 0.5;
// The Turn lunge is its own move with its own cap, checked on its own.
constexpr double kLungeForceLimitN = law::kLungeForceN + 0.5;
constexpr double kLungePowerLimitW = law::kLungePowerW + 0.5;
constexpr double kStepLimitM = 0.20;

struct Seed {
    int start_region = -1;
    int spot_region = -1;
    Vector3 start{};
    Vector3 spot{};
    double start_top = 0.0;
    double spot_top = 0.0;
    int floors = 0;
    int plan_edges = 0;
    bool patch_start = false;  // region centre unstandable: a patch centre used instead
    bool patch_spot = false;
};

// A standable spot in a region: its centre when the centre lies on one of
// the region's own walkable patches, else the centre of the region's largest
// walkable patch (a patch is a walkable top by construction).
bool spot_in_region(const AffordanceMap &map, const int region, Vector3 &out, bool &patch_used) {
    const auto &regions = map.regions();
    if (region < 0 || region >= static_cast<int>(regions.size())) {
        return false;
    }
    const auto &r = regions[static_cast<std::size_t>(region)];
    const double cx = 0.5 * (static_cast<double>(r.min_x) + static_cast<double>(r.max_x));
    const double cz = 0.5 * (static_cast<double>(r.min_z) + static_cast<double>(r.max_z));
    const auto &patches = map.patches();
    for (const int pi : r.patches) {
        if (pi < 0 || pi >= static_cast<int>(patches.size())) {
            continue;
        }
        const auto &p = patches[static_cast<std::size_t>(pi)];
        if (p.narrow) {
            continue;
        }
        if (cx >= p.min_x - 0.05 && cx <= p.max_x + 0.05 && cz >= p.min_z - 0.05 && cz <= p.max_z + 0.05) {
            out = Vector3{cx, static_cast<double>(p.top), cz};
            patch_used = false;
            return true;
        }
    }
    double best_area = -1.0;
    Vector3 best{};
    for (const int pi : r.patches) {
        if (pi < 0 || pi >= static_cast<int>(patches.size())) {
            continue;
        }
        const auto &p = patches[static_cast<std::size_t>(pi)];
        if (p.narrow) {
            continue;
        }
        const double area = (static_cast<double>(p.max_x) - p.min_x) * (static_cast<double>(p.max_z) - p.min_z);
        if (area > best_area) {
            best_area = area;
            best = Vector3{0.5 * (static_cast<double>(p.min_x) + static_cast<double>(p.max_x)),
                           static_cast<double>(p.top),
                           0.5 * (static_cast<double>(p.min_z) + static_cast<double>(p.max_z))};
        }
    }
    if (best_area <= 0.0) {
        return false;
    }
    out = best;
    patch_used = true;
    return true;
}

int floors_away(const double a, const double b) {
    const double diff = std::abs(a - b);
    if (diff <= 1.0) {
        return 0;
    }
    return std::max(1, static_cast<int>(std::lround(diff / 11.0)));
}

// Candidates from the scan, bucketed by floors away and direction so the
// chosen set covers same-floor, one, two-to-three and four-plus floors up
// and down, shortest plan first inside each bucket.
std::vector<Seed> build_seeds(const AffordanceMap &map, const int mode, const std::size_t cap) {
    struct Candidate {
        Seed seed;
        int bucket = 0;
    };
    std::vector<Candidate> candidates;
    const auto &regions = map.regions();
    const auto bucket_of = [&](const Seed &s) {
        if (s.floors == 0) {
            return 0;
        }
        const int dir = s.spot_top > s.start_top ? 0 : 1;  // up, then down
        const int f = s.floors >= 4 ? 4 : s.floors;
        return 1 + (f - 1) * 2 + dir;
    };
    for (int from = 0; from < static_cast<int>(regions.size()); ++from) {
        // The tower's floors start at its lowest stack deck: yard ground
        // and pit tops below it are not floors she chases across.
        if (regions[static_cast<std::size_t>(from)].top < 11.0) {
            continue;
        }
        for (int to = 0; to < static_cast<int>(regions.size()); ++to) {
            if (from == to) {
                continue;
            }
            if (regions[static_cast<std::size_t>(to)].top < 11.0) {
                continue;
            }
            const std::vector<int> plan = map.plan(from, to);
            if (plan.empty()) {
                continue;
            }
            Seed s;
            s.start_region = from;
            s.spot_region = to;
            s.start_top = regions[static_cast<std::size_t>(from)].top;
            s.spot_top = regions[static_cast<std::size_t>(to)].top;
            s.floors = floors_away(s.start_top, s.spot_top);
            s.plan_edges = static_cast<int>(plan.size());
            if (!spot_in_region(map, from, s.start, s.patch_start) ||
                !spot_in_region(map, to, s.spot, s.patch_spot)) {
                continue;
            }
            if (s.floors == 0 &&
                std::hypot(s.start.x - s.spot.x, s.start.z - s.spot.z) < 10.0) {
                continue;  // same floor, but not the far side
            }
            if (mode == 1 && (s.floors < 1 || s.floors > 3)) {
                continue;  // silent mode: one to three floors away
            }
            candidates.push_back(Candidate{s, bucket_of(s)});
        }
    }
    if (mode == 2) {
        // Same region, far side: no plan (a plan needs two regions), but a
        // long walk across one region's own steel.
        for (int r = 0; r < static_cast<int>(regions.size()); ++r) {
            if (regions[static_cast<std::size_t>(r)].top < 11.0) {
                continue;
            }
            Vector3 a{};
            bool pa = false;
            if (!spot_in_region(map, r, a, pa)) {
                continue;
            }
            const auto &patches = map.patches();
            double best_d = 15.0;
            Vector3 far{};
            bool found = false;
            for (const int pi : regions[static_cast<std::size_t>(r)].patches) {
                if (pi < 0 || pi >= static_cast<int>(patches.size()) || patches[static_cast<std::size_t>(pi)].narrow) {
                    continue;
                }
                const auto &p = patches[static_cast<std::size_t>(pi)];
                const Vector3 c{0.5 * (static_cast<double>(p.min_x) + static_cast<double>(p.max_x)),
                                static_cast<double>(p.top),
                                0.5 * (static_cast<double>(p.min_z) + static_cast<double>(p.max_z))};
                const double d = std::hypot(a.x - c.x, a.z - c.z);
                if (d > best_d) {
                    best_d = d;
                    far = c;
                    found = true;
                }
            }
            if (!found) {
                continue;
            }
            Seed s;
            s.start_region = r;
            s.spot_region = r;
            s.start = a;
            s.spot = far;
            s.start_top = regions[static_cast<std::size_t>(r)].top;
            s.spot_top = regions[static_cast<std::size_t>(r)].top;
            s.floors = 0;
            s.plan_edges = 0;
            s.patch_start = pa;
            s.patch_spot = true;
            candidates.push_back(Candidate{s, 0});
        }
    }
    // Shortest plan first inside each bucket, then round-robin the buckets.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.bucket != b.bucket) {
            return a.bucket < b.bucket;
        }
        if (a.seed.plan_edges != b.seed.plan_edges) {
            return a.seed.plan_edges < b.seed.plan_edges;
        }
        if (a.seed.start_region != b.seed.start_region) {
            return a.seed.start_region < b.seed.start_region;
        }
        return a.seed.spot_region < b.seed.spot_region;
    });
    std::vector<Seed> out;
    std::vector<bool> used(candidates.size(), false);
    while (out.size() < cap) {
        bool progressed = false;
        int last_bucket = -1;
        for (std::size_t i = 0; i < candidates.size() && out.size() < cap; ++i) {
            if (used[i] || candidates[i].bucket == last_bucket) {
                continue;
            }
            // One per bucket per round: a bucket already taken this round
            // waits for the next round (buckets are sorted, so a new bucket
            // value opens a new round only after the sweep restarts).
            used[i] = true;
            out.push_back(candidates[i].seed);
            last_bucket = candidates[i].bucket;
            progressed = true;
        }
        if (!progressed) {
            break;
        }
    }
    return out;
}

struct Result {
    bool pass = false;
    bool caught = false;
    bool alive = true;
    bool laws_ok = true;
    double seconds = 0.0;
    double final_dist = 0.0;
    double final_feet_delta = 0.0;
    double worst_step = 0.0;
    double max_climb_force = 0.0;
    double max_lunge_force = 0.0;
    double max_foot_power = 0.0;
    double max_lunge_power = 0.0;
    double max_pipe_peak = 0.0;
    double max_journey_peak = 0.0;
    std::string fail_reason;
};

Result run_seed(const Seed &seed, const int mode) {
    const double limit_s = mode == 1 ? 180.0 : 600.0;
    const std::uint8_t stage = mode == 1 ? 2 : 1;
    Simulation s(InitialSpawn::Deck154);
    require(s.advance_frame(1.0).accepted, "matron chase: settle");
    (void)s.matron_affordances();  // her own scan, as in play
    s.debug_launch_player(Vector3{seed.spot.x, seed.spot.y + 0.3, seed.spot.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(seed.start);
    tick(s);  // her view of herself is refreshed in the step
    auto last = s.matron_state().position;
    const auto claims0 = s.matron_state().claims;
    const auto shoves0 = s.matron_state().shoves;
    const auto catches0 = s.matron_state().catches;
    Result r;
    int on_spot = 0;
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    for (int i = 0; i < static_cast<int>(limit_s) * hz; ++i) {
        s.debug_set_matron_stage(stage);
        if (mode == 1 ? i == 0 : i % (8 * hz) == 0) {
            s.debug_matron_hear(seed.spot);
        }
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const auto m = s.matron_state();
        // Kept diagnostic, off by default: a one-second trace for future
        // chase debugging. It costs nothing when the variable is unset.
        if (std::getenv("SCRAPERX_CHASE_TRACE") != nullptr && i % hz == 0) {
            std::cout << "    chase trace mode " << mode << " t " << i / hz << " at (" << m.position.x << ", "
                      << m.position.y << ", " << m.position.z << ") floor " << m.floor_y << " lead_floor "
                      << m.lead_floor_y << " state " << int(m.state) << " pipe " << m.on_pipe << " climbing "
                      << m.climbing << " lead (" << m.lead_x << ", " << m.lead_z << ") fooled " << m.fooled
                      << " wary " << m.wary << " climbs " << m.climbs << " edges " << m.route_edges << " fails "
                      << m.route_failures << " knockdowns " << m.knockdowns << " blow " << m.blow_mps << " down_s "
                      << m.down_s << " stalls " << m.stalls << " sidesteps " << m.sidesteps << " reroutes "
                      << m.reroutes << " vaults " << m.vaults << " leaps " << m.leaps << " stepups "
                      << m.step_ups << " clambers " << m.clambers << '\n';
        }
        r.worst_step = std::max(r.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                         m.position.z - last.z));
        last = m.position;
        if (s.debug_matron_gravity_factor() != 1.0) {
            r.laws_ok = false;
            r.fail_reason = "gravity off";
        }
        if (m.climb_lunge_drive) {
            r.max_lunge_force = std::max(r.max_lunge_force, m.climb_force_n);
            r.max_lunge_power = std::max(r.max_lunge_power, m.foot_power_w);
            if (m.climb_force_n > kLungeForceLimitN && r.fail_reason.empty()) {
                r.laws_ok = false;
                r.fail_reason = "lunge force over its cap";
            }
            if (m.foot_power_w > kLungePowerLimitW && r.fail_reason.empty()) {
                r.laws_ok = false;
                r.fail_reason = "lunge power over its cap";
            }
        } else {
            r.max_foot_power = std::max(r.max_foot_power, m.foot_power_w);
            if (m.foot_power_w > kPowerLimitW && r.fail_reason.empty()) {
                r.laws_ok = false;
                r.fail_reason = "foot power over Limits";
            }
            r.max_climb_force = std::max(r.max_climb_force, m.climb_force_n);
            if (m.climb_force_n > kForceLimitN && r.fail_reason.empty()) {
                r.laws_ok = false;
                r.fail_reason = "climb force over Limits";
            }
        }
        r.max_pipe_peak = std::max(r.max_pipe_peak, m.pipe_peak_force_n);
        if (r.max_pipe_peak > kForceLimitN && r.fail_reason.empty()) {
            r.laws_ok = false;
            r.fail_reason = "pipe force over Limits";
        }
        const auto journey = s.matron_journey();
        r.max_journey_peak = std::max(r.max_journey_peak, journey.peak_force_n);
        if (r.max_journey_peak > kForceLimitN && r.fail_reason.empty()) {
            r.laws_ok = false;
            r.fail_reason = "travel force over Limits";
        }
        if (r.worst_step >= kStepLimitM && r.fail_reason.empty()) {
            r.laws_ok = false;
            r.fail_reason = "step over 0.20 m";
        }
        if (!m.alive) {
            r.alive = false;
            r.fail_reason = "died";
            break;
        }
        if (m.claims > claims0 || m.shoves > shoves0 || m.catches > catches0) {
            r.pass = true;
            r.caught = true;
            r.seconds = static_cast<double>(i) / hz;
            break;
        }
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        const double dist = std::hypot(m.position.x - seed.spot.x, m.position.z - seed.spot.z);
        if (!m.climbing && !m.on_pipe && dist <= 3.0 && std::abs(feet - seed.spot.y) <= 1.0) {
            if (++on_spot >= hz) {  // a second standing on the spot's floor
                r.pass = true;
                r.seconds = static_cast<double>(i) / hz;
                break;
            }
        } else {
            on_spot = 0;
        }
        r.seconds = static_cast<double>(i) / hz;
        r.final_dist = dist;
        r.final_feet_delta = feet - seed.spot.y;
    }
    const auto m = s.matron_state();
    const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
    r.final_dist = std::hypot(m.position.x - seed.spot.x, m.position.z - seed.spot.z);
    r.final_feet_delta = feet - seed.spot.y;
    if (!r.pass && r.fail_reason.empty()) {
        r.fail_reason = "never reached the spot";
    }
    return r;
}

} // namespace

void run_matron_chase() {
    const bool full = std::getenv("SCRAPERX_CHASE_ALL") != nullptr;
    // Seeds from one probe world; the coordinates are copied out before it
    // goes away, so each seed's own world is built from the same map.
    std::vector<Seed> mode1, mode2;
    {
        Simulation probe(InitialSpawn::Deck154);
        require(probe.advance_frame(1.0).accepted, "matron chase: probe settle");
        const AffordanceMap &map = probe.matron_affordances();
        std::cout << "  matron chase: scan regions " << map.regions().size() << ", edges " << map.edges().size()
                  << ", patches " << map.patches().size() << '\n';
        mode1 = build_seeds(map, 1, full ? 20 : 10);
        mode2 = build_seeds(map, 2, full ? 20 : 10);
    }
    std::cout << "  matron chase: mode 1 seeds " << mode1.size() << ", mode 2 seeds " << mode2.size()
              << (full ? " (full set)" : " (default subset)") << '\n';
    int pass1 = 0, pass2 = 0;
    double peak_force = 0.0, peak_power = 0.0, peak_lunge_force = 0.0, peak_lunge_power = 0.0;
    const auto peaks = [&](const Result &r) {
        peak_force = std::max({peak_force, r.max_climb_force, r.max_pipe_peak, r.max_journey_peak});
        peak_power = std::max(peak_power, r.max_foot_power);
        peak_lunge_force = std::max(peak_lunge_force, r.max_lunge_force);
        peak_lunge_power = std::max(peak_lunge_power, r.max_lunge_power);
    };
    for (const Seed &seed : mode1) {
        const Result r = run_seed(seed, 1);
        peaks(r);
        pass1 += r.pass && r.laws_ok && r.alive ? 1 : 0;
        std::cout << "  matron chase: mode 1 start " << seed.start_top << " spot " << seed.spot_top << " floors "
                  << seed.floors << (r.pass && r.laws_ok && r.alive ? " PASS" : " FAIL") << " in " << r.seconds << " s"
                  << ", final distance " << r.final_dist << " m, feet delta " << r.final_feet_delta << " m"
                  << (r.caught ? ", caught" : "") << (r.fail_reason.empty() ? "" : ", " + r.fail_reason)
                  << ", worst step " << r.worst_step << " m" << (seed.patch_spot ? " (patch spot)" : "") << '\n';
    }
    for (const Seed &seed : mode2) {
        const Result r = run_seed(seed, 2);
        peaks(r);
        pass2 += r.pass && r.laws_ok && r.alive ? 1 : 0;
        std::cout << "  matron chase: mode 2 start " << seed.start_top << " spot " << seed.spot_top << " floors "
                  << seed.floors << (r.pass && r.laws_ok && r.alive ? " PASS" : " FAIL") << " in " << r.seconds << " s"
                  << ", final distance " << r.final_dist << " m, feet delta " << r.final_feet_delta << " m"
                  << (r.caught ? ", caught" : "") << (r.fail_reason.empty() ? "" : ", " + r.fail_reason)
                  << ", worst step " << r.worst_step << " m" << (seed.patch_spot ? " (patch spot)" : "") << '\n';
    }
    std::cout << "  matron chase summary: mode 1 " << pass1 << '/' << mode1.size() << ", mode 2 " << pass2 << '/'
              << mode2.size() << (full ? " (full set)" : " (default subset)") << '\n';
    std::cout << "  matron chase peaks over every seed: force " << peak_force << " N, feet's power " << peak_power
              << " W; lunge " << peak_lunge_force << " N, " << peak_lunge_power << " W\n";
    if (full) {
        require(mode1.size() >= 20, "matron chase: full set needs at least 20 silent seeds");
        require(mode2.size() >= 20, "matron chase: full set needs at least 20 repeated seeds");
    } else {
        // The bar below is a share of the seeds: with none it would pass.
        require(mode1.size() >= 10 && mode2.size() >= 10, "matron chase: the default subset needs 10 seeds a mode");
    }
    // Owner bar, 2026-10-24, superseding the earlier 90% bar: 17/20 passes.
    // The owner is the creator of the documents, so this bar, not the old
    // 90% figure, is the pass bar. 85% in integer form: pass * 20 >= size * 17.
    require(mode1.empty() || pass1 * 20 >= static_cast<int>(mode1.size()) * 17,
            "matron chase: silent mode under 85% (owner bar, 17/20 passes)");
    require(mode2.empty() || pass2 * 20 >= static_cast<int>(mode2.size()) * 17,
            "matron chase: repeated mode under 85% (owner bar, 17/20 passes)");
    std::cout << "PASS scraperx_sim matron chase\n";
}
