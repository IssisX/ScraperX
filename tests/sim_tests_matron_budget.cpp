// J-046 (#257): her one budget, audited. Every tick of the scenario groups
// that exercise her pushes and holds, the effort ledger
// (src/sim/matron_effort.hpp) adds up the force and power of each path of
// hers: every Budget::take, the pole's friction, the crate shove, the
// routecraft drive, the stock hold's constraint (lambda / dt), the grab
// hand's spring and arm, her second hand's, her knee's, the shove and the
// let-go (velocity writes: mass * dv / dt). Power is force . velocity, the
// positive part, as in her Budget.
//
// The scenarios are the groups' own stagings, run as they are (each in its
// own child process, so no group's state reaches another): the ledger
// watches; it changes nothing.
//
// Asserts, per scenario and path:
//  - a path over her 8 kN or 44 kW on any tick, or applying more than it
//    charges to a Budget (ChatGPT, #244/#257), is a leak: it must be on
//    kExpected below, with its measured peaks; an unlisted one FAILs;
//  - a listed leak that comes back honest FAILs as XPASS: take it off the
//    list (it only ever shrinks); one that grows past its listed peak by
//    more than kGrowth FAILs;
//  - the per-tick sum of the honest paths stays within her budget;
//  - every force on her body as Jolt steps is one a path recorded
//    (the residual), so no new path goes unseen.
// Measurement only: no budget is raised, no behaviour changes.
//
// The matron-ammo group (J-042, PR #256) joins kScenarios once it lands.

#include "sim_tests.hpp"
#include "sim/matron_effort.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
namespace effort = scraperx::sim::effort;
using effort::Path;

struct Scenario {
    const char *name;
    void (*run)();
};

// The groups that exercise her paths (#257): the grab group's three, her
// load, the forklift on her deck, her mischief, her damage, and his blows.
const Scenario kScenarios[] = {
    {"matron-grab", run_matron_grab},     {"matron-arm", run_matron_arm},       {"matron-fatality", run_matron_fatality},
    {"matron-load", run_matron_load},     {"forklift-matron", run_forklift_matron},
    {"matron-mischief", run_matron_mischief}, {"matron-damage", run_matron_damage}, {"strike", run_strike},
    {"matron-sweep", run_matron_sweep},   {"matron-kick", run_matron_kick},     {"matron-jab", run_matron_jab},
};

// A known leak: the scenario, the path, what is wrong with it (over her
// budget on some tick; applying force no Budget is charged for), and its
// measured peaks (N, W).
struct Expected {
    const char *scenario;
    Path path;
    bool over;
    bool uncharged;
    double peak_n;
    double peak_w;
};

const Expected kExpected[] = {
    // Measured 11-05 on trunk 1ebebdb9 (host g++ 13.3, Release). over: some tick past 8 kN or 44 kW;
    // uncharged: force no Budget is charged for. The budget path's overs are the Turn lunge's own
    // caps (law::kLungeForceN, kLungePowerW), the one designed exception to her one budget.
    // scenario, path, over, uncharged, peak N, peak W
    {"matron-grab", Path::Budget, true, false, 9800.0, 65000.0},
    {"matron-grab", Path::CrateShove, false, true, 8000.0, 1429.0},
    {"matron-grab", Path::StockHold, false, true, 1084.0, 3.0},
    {"matron-grab", Path::GrabSpring, false, true, 2600.0, 11424.0},
    {"matron-grab", Path::GrabArm, false, true, 1452.0, 545.0},
    {"matron-grab", Path::Hand2Spring, false, true, 2600.0, 5675.0},
    {"matron-grab", Path::Hand2Arm, false, true, 2498.0, 935.0},
    {"matron-grab", Path::Shove, true, true, 44771.0, 131006.0},
    {"matron-arm", Path::Budget, true, false, 9800.0, 61324.0},
    {"matron-arm", Path::GrabSpring, false, true, 2600.0, 5141.0},
    {"matron-arm", Path::GrabArm, false, true, 974.0, 401.0},
    {"matron-arm", Path::Hand2Spring, false, true, 2600.0, 2025.0},
    {"matron-arm", Path::Hand2Arm, false, true, 1477.0, 400.0},
    {"matron-arm", Path::Knee, false, true, 5005.0, 9263.0},
    {"matron-arm", Path::Shove, true, true, 44770.0, 131006.0},
    {"matron-fatality", Path::CrateShove, false, true, 8000.0, 18913.0},
    {"matron-fatality", Path::GrabSpring, false, true, 2600.0, 6963.0},
    {"matron-fatality", Path::GrabArm, false, true, 1144.0, 336.0},
    {"matron-fatality", Path::Knee, false, true, 4864.0, 8029.0},
    {"matron-fatality", Path::Shove, true, true, 52285.0, 131006.0},
    {"matron-fatality", Path::LetGo, false, true, 6885.0, 6355.0},
    {"matron-load", Path::GrabSpring, false, true, 2600.0, 5141.0},
    {"matron-load", Path::GrabArm, false, true, 2824.0, 2064.0},
    {"matron-load", Path::Hand2Spring, false, true, 2600.0, 2025.0},
    {"matron-load", Path::Hand2Arm, false, true, 1477.0, 400.0},
    {"matron-load", Path::Knee, false, true, 4989.0, 7222.0},
    {"forklift-matron", Path::Budget, true, false, 9800.0, 65000.0},
    {"forklift-matron", Path::GrabSpring, false, true, 1340.0, 1517.0},
    {"forklift-matron", Path::GrabArm, false, true, 380.0, 368.0},
    {"matron-mischief", Path::StockHold, true, true, 17219.0, 932.0},
    {"matron-mischief", Path::GrabSpring, false, true, 1617.0, 4469.0},
    {"matron-mischief", Path::GrabArm, false, true, 252.0, 292.0},
    {"matron-damage", Path::GrabSpring, false, true, 1339.0, 2012.0},
    {"matron-damage", Path::GrabArm, false, true, 406.0, 389.0},
    {"strike", Path::GrabSpring, false, true, 2245.0, 3963.0},
    {"strike", Path::GrabArm, false, true, 974.0, 373.0},
    {"strike", Path::Hand2Spring, false, true, 900.0, 0.0},
    {"matron-jab", Path::GrabSpring, false, true, 900.0, 2005.0},
    {"matron-jab", Path::GrabArm, false, true, 406.0, 389.0},
    {"matron-jab", Path::Shove, true, true, 44770.0, 131006.0},
};

constexpr double kGrowth = 1.10;          // a listed leak may not grow past its measured peak by more than this
constexpr double kChargeSlackN = 1.0;     // applied less charged, per tick, still honest
constexpr double kOverSlackN = 0.5;       // her 8 kN, as the other force checks read it (8,000.5 N)
constexpr double kOverSlackW = 1.0;
constexpr double kResidualN = 1.0;        // a force on her that no path recorded

struct Result {
    int ok = 0;
    std::array<effort::Stats, effort::kPaths> run{};
    effort::Stats total{};
    effort::Stats total_all{};
    double residual_n = 0.0;
    long ticks = 0;
};

const Expected *listed(const char *scenario, const Path p) {
    for (const Expected &e : kExpected) {
        if (e.path == p && std::string(e.scenario) == scenario) return &e;
    }
    return nullptr;
}

bool over(const effort::Stats &s) {
    return s.peak_force_n > static_cast<double>(law::kForceN) + kOverSlackN ||
           s.peak_power_w > static_cast<double>(law::kPowerW) + kOverSlackW;
}

pid_t start(const Scenario &sc, int &read_fd) {
    int fds[2];
    require(pipe(fds) == 0, "matron-budget: a pipe for the scenario's ledger");
    std::cout.flush();
    std::cerr.flush();
    const pid_t pid = fork();
    require(pid >= 0, "matron-budget: fork a scenario");
    if (pid == 0) {
        close(fds[0]);
        // The scenario's own lines stay out of this group's log; its FAILs (stderr) do not.
        const int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) dup2(devnull, STDOUT_FILENO);
        effort::Ledger &l = effort::ledger();
        l.enabled = true;
        l.force_cap_n = law::kForceN + static_cast<float>(kOverSlackN);
        l.power_cap_w = law::kPowerW + static_cast<float>(kOverSlackW);
        for (std::size_t i = 0; i < effort::kPaths; ++i) {
            l.listed[i] = listed(sc.name, static_cast<Path>(i)) != nullptr;
        }
        effort::reset_run();
        sc.run();
        Result r;
        r.ok = 1;
        r.run = l.run;
        r.total = l.total;
        r.total_all = l.total_all;
        r.residual_n = l.peak_residual_n;
        r.ticks = l.ticks;
        const char *at = reinterpret_cast<const char *>(&r);
        std::size_t left = sizeof(r);
        while (left > 0) {
            const ssize_t n = write(fds[1], at, left);
            if (n <= 0) _exit(3);
            at += n;
            left -= static_cast<std::size_t>(n);
        }
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    read_fd = fds[0];
    return pid;
}

Result finish(const pid_t pid, const int fd) {
    Result r;
    char *at = reinterpret_cast<char *>(&r);
    std::size_t got = 0;
    while (got < sizeof(r)) {
        const ssize_t n = read(fd, at + got, sizeof(r) - got);
        if (n <= 0) break;
        got += static_cast<std::size_t>(n);
    }
    close(fd);
    int status = 0;
    (void)waitpid(pid, &status, 0);
    if (got != sizeof(r) || !WIFEXITED(status) || WEXITSTATUS(status) != 0) r.ok = 0;
    return r;
}

void row(const char *scenario, const char *path, const effort::Stats &s, const char *mark) {
    std::cout << "  " << std::left << std::setw(16) << scenario << std::setw(16) << path << std::right << std::fixed
              << std::setprecision(0) << std::setw(9) << s.peak_force_n << " N" << std::setw(9) << s.peak_power_w
              << " W" << std::setw(10) << s.work_j << " J" << std::setw(7) << s.ticks_over << " over"
              << std::setw(7) << s.ticks_active << " ticks" << std::setw(9) << s.peak_uncharged_n << " N uncharged"
              << (mark[0] != '\0' ? "  " : "") << mark << "\n";
}
} // namespace

void run_matron_budget() {
    constexpr std::size_t kCount = sizeof(kScenarios) / sizeof(kScenarios[0]);
    std::array<Result, kCount> results{};
    // Three at a time: the grab group alone is about two minutes.
    constexpr std::size_t kAtOnce = 3;
    for (std::size_t from = 0; from < kCount; from += kAtOnce) {
        std::array<pid_t, kAtOnce> pids{};
        std::array<int, kAtOnce> fds{};
        const std::size_t to = std::min(kCount, from + kAtOnce);
        for (std::size_t i = from; i < to; ++i) pids[i - from] = start(kScenarios[i], fds[i - from]);
        for (std::size_t i = from; i < to; ++i) results[i] = finish(pids[i - from], fds[i - from]);
    }

    int failures = 0;
    const auto fail = [&failures](const std::string &what) {
        std::cerr << "FAIL: matron-budget: " << what << "\n";
        ++failures;
    };
    std::cout << "  her budget: " << law::kForceN << " N, " << law::kPowerW << " W, every path together\n";
    for (std::size_t k = 0; k < kCount; ++k) {
        const Scenario &sc = kScenarios[k];
        const Result &r = results[k];
        if (r.ok == 0) {
            fail(std::string(sc.name) + ": the scenario did not run to its end (its own FAIL is above)");
            continue;
        }
        std::cout << "  " << sc.name << ": " << r.ticks << " ticks\n";
        for (std::size_t i = 0; i < effort::kPaths; ++i) {
            const Path p = static_cast<Path>(i);
            const effort::Stats &s = r.run[i];
            const Expected *e = listed(sc.name, p);
            const bool counts = effort::summed(p);   // his blows, and what reaches him through her hands, are reported only
            const bool is_over = counts && s.ticks_active > 0 && over(s);
            const bool is_uncharged = counts && s.ticks_active > 0 && s.peak_uncharged_n > kChargeSlackN;
            std::string mark;
            if (e != nullptr) {
                mark = "EXPECTED";
                if ((e->over && !is_over) || (e->uncharged && !is_uncharged)) {
                    fail(std::string(sc.name) + " " + effort::path_name(p) +
                         ": XPASS, listed as a leak and honest now: take it off kExpected");
                    mark = "XPASS";
                }
                if ((is_over && !e->over) || (is_uncharged && !e->uncharged)) {
                    fail(std::string(sc.name) + " " + effort::path_name(p) + ": a leak its kExpected entry does not name");
                    mark = "NEW LEAK";
                }
                if (s.peak_force_n > e->peak_n * kGrowth || s.peak_power_w > e->peak_w * kGrowth + kOverSlackW) {
                    fail(std::string(sc.name) + " " + effort::path_name(p) + ": the listed leak grew past its measured peak");
                    mark = "GREW";
                }
            } else if (is_over || is_uncharged) {
                fail(std::string(sc.name) + " " + effort::path_name(p) + (is_over ? ": over her budget" : "") +
                     (is_uncharged ? ": applies force no Budget is charged for" : "") + " (not on kExpected)");
                mark = "LEAK";
            }
            if (s.ticks_active > 0) row(sc.name, effort::path_name(p), s, mark.c_str());
        }
        row(sc.name, "TOTAL honest", r.total, over(r.total) ? "OVER" : "");
        row(sc.name, "total, all", r.total_all, "");
        std::cout << "  " << sc.name << ": peak residual " << std::setprecision(2) << r.residual_n
                  << " N (her body's force as Jolt steps, less every path's)\n";
        if (over(r.total)) fail(std::string(sc.name) + ": the honest paths' sum went over her budget");
        if (r.residual_n > kResidualN) fail(std::string(sc.name) + ": a force on her no path records (residual)");
    }
    for (const Expected &e : kExpected) {
        if (e.path == Path::Count) continue;
        bool found = false;
        for (std::size_t k = 0; k < kCount; ++k) {
            if (std::string(kScenarios[k].name) == e.scenario) {
                found = results[k].ok == 0 || results[k].run[static_cast<std::size_t>(e.path)].ticks_active > 0;
            }
        }
        if (!found) fail(std::string(e.scenario) + " " + effort::path_name(e.path) + ": XPASS, listed and never active");
    }
    require(failures == 0, "matron-budget: her one budget (8 kN, 44 kW) audited, see the lines above");
    std::cout << "PASS scraperx_sim matron-budget: " << kCount << " scenarios, every path within her budget or listed\n";
}
