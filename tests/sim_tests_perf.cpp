// J-044m (#245), the fight perf probes: what one fixed step costs while the
// Matron fights the player up close on her 154 deck. Measures and logs only:
// it asserts that the step timer runs (one sample per step, every span) and
// that the timer never reaches the state (the same fight twice ends in the
// same world_bits, bit for bit). No performance threshold yet; the numbers
// are for J-044's Fold 6 budget (03_EXECUTION/FIGHT/PERF_PROBES.md).
#include "sim_tests.hpp"
#include "sim/matron.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::StepTimer;
using scraperx::sim::StepTiming;
using scraperx::sim::Vector3;

constexpr int kFightTicks = 6 * static_cast<int>(Simulation::kTickRateHz);   // 6 s of fighting
constexpr int kPunchEveryTicks = 45;                                         // a punch each 0.5 s

struct Samples final {
    std::vector<double> step, physics, matron_pre, matron_post;
};

struct Fight final {
    Samples samples;
    StepTiming timing;
    std::vector<std::uint64_t> bits;
    std::uint64_t strikes = 0;
    std::uint64_t hits = 0;
    std::uint64_t runs_ended = 0;
};

void tick(Simulation &s) {
    require(s.advance_frame(Simulation::kFixedStepSeconds).steps_advanced == 1, "perf-step: one tick, one step");
}

// The matron-damage group's staging: the player on the 154 deck, her woken
// 1.05 m in front of him, Hunting, changed (stage 2) so a punch does not
// end the fight. Then punches thrown at her, facing her every tick.
Fight fight() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    for (int i = 0; i < 30; ++i) tick(s);
    const Vector3 p = s.snapshot().player_position;
    (void)s.set_facing(0.0, 1.0);
    s.launch_matron_for_test({p.x, p.y - 0.20, p.z + 1.05});
    for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                    s.matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) tick(s);
    require(s.matron_state().state == static_cast<int>(Matron::State::Hunting),
            "perf-step: she must be Hunting before the fight is measured");
    s.debug_set_matron_stage(2);
    Fight out;
    const std::uint64_t strikes_before = s.snapshot().strike_count;
    const std::uint64_t hits_before = s.snapshot().strike_hit_count;
    const std::uint64_t samples_before = s.step_timing().step.samples;
    for (int i = 0; i < kFightTicks; ++i) {
        const Vector3 me = s.snapshot().player_position, her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
        if (i % kPunchEveryTicks == 0) (void)s.request_strike();
        tick(s);
        const StepTiming t = s.step_timing();
        out.samples.step.push_back(t.step.last_us);
        out.samples.physics.push_back(t.physics.last_us);
        out.samples.matron_pre.push_back(t.matron_pre.last_us);
        out.samples.matron_post.push_back(t.matron_post.last_us);
    }
    out.timing = s.step_timing();
    require(out.timing.step.samples - samples_before == static_cast<std::uint64_t>(kFightTicks) &&
                out.timing.physics.samples == out.timing.step.samples &&
                out.timing.matron_pre.samples == out.timing.step.samples &&
                out.timing.matron_post.samples == out.timing.step.samples,
            "perf-step: the timer must take one sample of every span per step");
    out.strikes = s.snapshot().strike_count - strikes_before;
    out.hits = s.snapshot().strike_hit_count - hits_before;
    out.runs_ended = s.snapshot().runs_ended_by_matron;
    out.bits = world_bits(s);
    return out;
}

struct Summary final {
    double min = 0.0, mean = 0.0, p95 = 0.0, max = 0.0;
};

Summary summarize(std::vector<double> v) {
    Summary out;
    if (v.empty()) return out;
    std::sort(v.begin(), v.end());
    double sum = 0.0;
    for (const double x : v) sum += x;
    out.min = v.front();
    out.max = v.back();
    out.mean = sum / static_cast<double>(v.size());
    const std::size_t rank = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(v.size())));
    out.p95 = v[std::max<std::size_t>(rank, 1) - 1];
    return out;
}

void print(const char *name, const std::vector<double> &v, const StepTimer &running) {
    const Summary s = summarize(v);
    char line[256];
    std::snprintf(line, sizeof line,
                  "PERF perf-step %-11s n=%zu min_us=%.1f mean_us=%.1f p95_us=%.1f max_us=%.1f "
                  "running_mean_us=%.1f running_max_us=%.1f",
                  name, v.size(), s.min, s.mean, s.p95, s.max, running.mean_us, running.max_us);
    std::cout << line << '\n';
}
} // namespace

void run_perf_step() {
    const Fight first = fight();
    const Fight second = fight();
    double total = 0.0;
    for (const double x : first.samples.step) total += x;
    require(total > 0.0, "perf-step: the step timer must measure time");
    require(first.bits == second.bits,
            "perf-step: the same fight twice must end in the same state, bit for bit (the timer never feeds back)");
    print("step", first.samples.step, first.timing.step);
    print("physics", first.samples.physics, first.timing.physics);
    print("matron_pre", first.samples.matron_pre, first.timing.matron_pre);
    print("matron_post", first.samples.matron_post, first.timing.matron_post);
    // Per second of the fight, the whole step: where in the fight it costs.
    for (std::size_t at = 0; at + Simulation::kTickRateHz <= first.samples.step.size(); at += Simulation::kTickRateHz) {
        const std::vector<double> second_of(first.samples.step.begin() + static_cast<std::ptrdiff_t>(at),
                                            first.samples.step.begin() +
                                                static_cast<std::ptrdiff_t>(at + Simulation::kTickRateHz));
        const Summary s = summarize(second_of);
        char line[160];
        std::snprintf(line, sizeof line, "PERF perf-step second=%zu step mean_us=%.1f p95_us=%.1f max_us=%.1f",
                      at / Simulation::kTickRateHz, s.mean, s.p95, s.max);
        std::cout << line << '\n';
    }
    const double budget_us = 1.0e6 / static_cast<double>(Simulation::kTickRateHz);
    std::cout << "PASS scraperx_sim perf-step: ticks=" << first.samples.step.size()
              << " strikes=" << first.strikes << " hits=" << first.hits << " runs_ended=" << first.runs_ended
              << " words=" << first.bits.size() << " step_budget_us=" << budget_us << " (logged, not asserted)\n";
}
SCRAPERX_SLICE("perf-step", run_perf_step);
