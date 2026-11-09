#pragma once

// The native test groups register themselves, each beside its function in its
// own sim_tests_*.cpp, so adding a group or a slice file edits no shared list
// (J-074, #352). simulation_tests.cpp's main() runs the registry.
//
//   SCRAPERX_GROUP("twin", 70, run_twin);
//       A group of the default sequence (the bare-binary run, the CTest test
//       bare scraperx_sim_tests run), printed as its TIME line. The
//       sequence runs in order of the key, never in registration order:
//       static initialization across files has no set order. Keys are spaced
//       by 10, so a new group takes a free key between two others.
//   SCRAPERX_SELECT("twin", run_twin, run_twin_balconies);
//       SCRAPERX_ONLY=twin runs those functions, in that order, and nothing else.
//   SCRAPERX_SLICE("matron-riser", test_matron_riser);
//       As SCRAPERX_SELECT, and also its own CTest test scraperx_sim.matron-riser
//       (CMakeLists.txt reads these lines; the post-build check holds the two to
//       the same list).
//
// A repeated name, a repeated key or an unknown SCRAPERX_ONLY name stops the
// binary at start-up (and so the build, through the post-build check).
// SCRAPERX_LIST=1 prints the default sequence in order, SCRAPERX_LIST=only every
// SCRAPERX_ONLY name with what it runs, SCRAPERX_LIST=slices the CTest slices.

#include <initializer_list>

namespace scraperx_tests {

using TestFn = void (*)();

bool register_group(const char *name, long order, TestFn fn, const char *fn_name,
                    const char *file, int line);
bool register_only(const char *name, bool ctest_slice, std::initializer_list<TestFn> fns,
                   const char *fn_names, const char *file, int line);

}  // namespace scraperx_tests

#define SCRAPERX_REGISTRY_CAT2(a, b) a##b
#define SCRAPERX_REGISTRY_CAT(a, b) SCRAPERX_REGISTRY_CAT2(a, b)
#define SCRAPERX_REGISTRY_VAR SCRAPERX_REGISTRY_CAT(scraperx_registered_at_line_, __LINE__)

#define SCRAPERX_GROUP(name, order, fn)                                                    \
    [[maybe_unused]] static const bool SCRAPERX_REGISTRY_VAR =                             \
        ::scraperx_tests::register_group(name, order, fn, #fn, __FILE__, __LINE__)
#define SCRAPERX_SELECT(name, ...)                                                         \
    [[maybe_unused]] static const bool SCRAPERX_REGISTRY_VAR =                             \
        ::scraperx_tests::register_only(name, false, {__VA_ARGS__}, #__VA_ARGS__, __FILE__, __LINE__)
#define SCRAPERX_SLICE(name, ...)                                                          \
    [[maybe_unused]] static const bool SCRAPERX_REGISTRY_VAR =                             \
        ::scraperx_tests::register_only(name, true, {__VA_ARGS__}, #__VA_ARGS__, __FILE__, __LINE__)
