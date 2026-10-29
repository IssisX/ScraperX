// Her corner pipe's ways remember a failure (matron_pipe.cpp get_off_pipe,
// matron.hpp PipeWayMemory): one failed go leaves that way alone
// kPipeWayBlockS; past kPipeWayRetries it stays shut, with its reason, until
// the world changes (a restore), which frees it. The other ways are untouched.

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;

void run_for(Simulation &s, const double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * Simulation::kTickRateHz); ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
}
} // namespace

void run_matron_pipe_ways() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    run_for(s, 1.0);
    const double up_from = 132.0, up_to = 143.0;   // way 0: one story up from 132
    const double other_from = 143.0, other_to = 154.0;
    require(s.matron_pipe_way(up_from, up_to).open && s.matron_pipe_way(up_from, up_to).failures == 0,
            "matron pipe ways: every way open at the start");
    // One failure: shut a while, then open again.
    s.debug_matron_fail_pipe_way(up_from, up_to);
    auto w = s.matron_pipe_way(up_from, up_to);
    require(!w.open && w.failures == 1 && std::abs(w.blocked_for_s - double(Matron::kPipeWayBlockS)) < 0.1 && !w.ruled_out,
            "matron pipe ways: a failed way is left alone kPipeWayBlockS");
    require(s.matron_pipe_way(other_from, other_to).open, "matron pipe ways: the other ways stay open");
    run_for(s, double(Matron::kPipeWayBlockS) + 1.0);
    require(s.matron_pipe_way(up_from, up_to).open, "matron pipe ways: after its block the way is open again");
    // Past its retries: shut until the world changes.
    // (Failures 2..kPipeWayRetries: each still a while; one more is past them.)
    for (int k = 2; k <= int(Matron::kPipeWayRetries); ++k) {
        s.debug_matron_fail_pipe_way(up_from, up_to);
        require(std::isfinite(s.matron_pipe_way(up_from, up_to).blocked_for_s),
                "matron pipe ways: within its retries the block runs out");
        run_for(s, double(Matron::kPipeWayBlockS) + 1.0);
    }
    s.debug_matron_fail_pipe_way(up_from, up_to);
    w = s.matron_pipe_way(up_from, up_to);
    require(w.failures == int(Matron::kPipeWayRetries) + 1 && !w.open && std::isinf(w.blocked_for_s) && !w.ruled_out,
            "matron pipe ways: failed past kPipeWayRetries, the way stays shut, its reason the failures");
    run_for(s, 2.0 * double(Matron::kPipeWayBlockS));
    require(!s.matron_pipe_way(up_from, up_to).open, "matron pipe ways: no time frees a way past its retries");
    require(s.matron_pipe_way(other_from, other_to).open && s.matron_pipe_way(other_from, other_to).failures == 0,
            "matron pipe ways: the other ways untouched");
    // A restore: the world changed, every way worth trying again.
    s.launch_matron_for_test(scraperx::sim::Vector3{10.0, 155.05, -126.2});
    w = s.matron_pipe_way(up_from, up_to);
    std::cout << "  matron pipe ways: after the restore failures=" << w.failures << " open=" << w.open << '\n';
    require(w.open && w.failures == 0, "matron pipe ways: a restore frees a way shut past its retries");
    std::cout << "PASS scraperx_sim matron pipe ways: retries=" << int(Matron::kPipeWayRetries)
              << " block_s=" << Matron::kPipeWayBlockS << '\n';
}
