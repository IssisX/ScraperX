// The native tests: one executable. The groups register themselves, each
// beside its function in its own sim_tests_*.cpp (sim_registry.hpp); this
// file only runs the registry. SCRAPERX_ONLY=<name> runs one selection,
// otherwise the default sequence runs, in order of each group's key, then
// name (a key may repeat: two lanes in flight may pick the same one).

#include "sim_tests.hpp"

#include <chrono>
#include <cstdio>
#include <map>

namespace scraperx_tests {
namespace {

struct Group {
    std::string name;
    long order;
    TestFn fn;
    std::string fn_name;
    std::string where;
};

struct Selection {
    std::string name;
    bool ctest_slice;
    std::vector<TestFn> fns;
    std::string fn_names;
    std::string where;
};

struct Registry {
    std::vector<Group> groups;
    std::vector<Selection> selections;
};

// Filled during static initialization, from every file, in no set order.
Registry &registry() {
    static Registry r;
    return r;
}

std::string where_of(const char *file, const int line) {
    return std::string(file) + ":" + std::to_string(line);
}

bool plain_name(const std::string &name) {
    return !name.empty() && name.find_first_of(" \t\n;") == std::string::npos;
}

// Every repeated name and empty registration, as one line each. A repeated
// key is no error: groups on one key run in name order (main's sort).
std::vector<std::string> registry_errors(const Registry &r) {
    std::vector<std::string> errors;
    std::map<std::string, std::string> group_names;
    for (const Group &g : r.groups) {
        if (!plain_name(g.name) || g.fn == nullptr) {
            errors.push_back("group '" + g.name + "' has no plain name or no function (" + g.where + ")");
        }
        if (const auto [it, added] = group_names.emplace(g.name, g.where); !added) {
            errors.push_back("group " + g.name + " registered twice (" + it->second + ", " + g.where + ")");
        }
    }
    std::map<std::string, std::string> selection_names;
    for (const Selection &s : r.selections) {
        bool null_fn = s.fns.empty();
        for (const TestFn fn : s.fns) {
            null_fn = null_fn || fn == nullptr;
        }
        if (!plain_name(s.name) || null_fn) {
            errors.push_back("SCRAPERX_ONLY name '" + s.name + "' has no plain name or no function (" + s.where + ")");
        }
        if (const auto [it, added] = selection_names.emplace(s.name, s.where); !added) {
            errors.push_back("SCRAPERX_ONLY name " + s.name + " registered twice (" + it->second + ", " + s.where +
                             ")");
        }
    }
    return errors;
}

}  // namespace

bool register_group(const char *name, const long order, const TestFn fn, const char *fn_name, const char *file,
                    const int line) {
    registry().groups.push_back({name, order, fn, fn_name, where_of(file, line)});
    return true;
}

bool register_only(const char *name, const bool ctest_slice, const std::initializer_list<TestFn> fns,
                   const char *fn_names, const char *file, const int line) {
    registry().selections.push_back({name, ctest_slice, std::vector<TestFn>(fns), fn_names, where_of(file, line)});
    return true;
}

}  // namespace scraperx_tests

int main() {
    using namespace scraperx_tests;
    // A line at a time, piped or not: a harness that stops the run early
    // still shows every result up to that moment (fully buffered, a group
    // stopped at 90 s showed only its first 14 of 24 passes).
    std::setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);

    Registry &r = registry();
    if (const std::vector<std::string> errors = registry_errors(r); !errors.empty()) {
        for (const std::string &e : errors) {
            std::cerr << "FAIL: test registry: " << e << '\n';
        }
        return EXIT_FAILURE;
    }
    // The order is (key, name): a key may repeat, a name may not (checked
    // above), so the order is total and never the registration order.
    std::sort(r.groups.begin(), r.groups.end(), [](const Group &a, const Group &b) {
        return a.order != b.order ? a.order < b.order : a.name < b.name;
    });
    std::sort(r.selections.begin(), r.selections.end(),
              [](const Selection &a, const Selection &b) { return a.name < b.name; });

    // SCRAPERX_LIST=1: the default sequence, in order ("<key> <name> <function>").
    // SCRAPERX_LIST=only: every SCRAPERX_ONLY name ("<name> <slice|-> <functions>").
    // SCRAPERX_LIST=slices: the names that are their own CTest tests, one a line.
    if (const char *list = std::getenv("SCRAPERX_LIST"); list != nullptr && *list != '\0') {
        const std::string mode(list);
        if (mode == "only") {
            for (const Selection &s : r.selections) {
                std::cout << s.name << '\t' << (s.ctest_slice ? "slice" : "-") << '\t' << s.fn_names << '\n';
            }
        } else if (mode == "slices") {
            for (const Selection &s : r.selections) {
                if (s.ctest_slice) {
                    std::cout << s.name << '\n';
                }
            }
        } else if (mode == "1" || mode == "groups") {
            for (const Group &g : r.groups) {
                std::cout << g.order << '\t' << g.name << '\t' << g.fn_name << '\n';
            }
        } else {
            std::cerr << "FAIL: SCRAPERX_LIST=" << mode << " is not 1, groups, only or slices\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    // An unknown SCRAPERX_ONLY name stops here, before any test: it never
    // falls through to the whole sequence. Unset or empty runs the sequence.
    const Selection *only = nullptr;
    if (const char *name = std::getenv("SCRAPERX_ONLY"); name != nullptr && *name != '\0') {
        for (const Selection &s : r.selections) {
            only = s.name == name ? &s : only;
        }
        if (only == nullptr) {
            std::cerr << "FAIL: SCRAPERX_ONLY=" << name
                      << " names no registered group (SCRAPERX_LIST=only lists every name)\n";
            return EXIT_FAILURE;
        }
    }

    // Her collision body audited every tick of every run (matron shape: no
    // tick in the steel, checked at exit; sim_tests_matron_shape.cpp).
    start_matron_shape_audit();
    // SCRAPERX_EXTRA_SOLID_Y=<meters>: every world this run builds gets one
    // half-meter static box 300 m east and south of the tower, built with the
    // solids of the 22 m band at that height, touching nothing. The run's
    // every line must then read as the run without it
    // (03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md).
    if (const char *extra_y = std::getenv("SCRAPERX_EXTRA_SOLID_Y"); extra_y != nullptr) {
        const double y = std::strtod(extra_y, nullptr);
        scraperx::sim::Simulation::set_test_extra_solids_for_every_world(
            {{300.0, y, 300.0, 0.5F, 0.5F, 0.5F, false}});
        std::cout << "  every world of this run: one extra solid in the band at " << y << " m\n";
    }

    if (only != nullptr) {
        for (const TestFn fn : only->fns) {
            fn();
        }
        return EXIT_SUCCESS;
    }

    // Wall seconds per group, printed as TIME lines, so the next shard split
    // is measured, not guessed. Print-only. A SCRAPERX_ONLY run returns above,
    // so TIME lines appear only in the bare-binary run, never in a slice log.
    for (const Group &g : r.groups) {
        const auto start = std::chrono::steady_clock::now();
        g.fn();
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "TIME scraperx_sim " << g.name << ' ' << seconds << " s\n";
    }

    return EXIT_SUCCESS;
}
