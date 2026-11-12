// J-100 #418: she reads a load hung over her and gets out from under it.
// Her sight learns a heavy load hung overhead (the scan, matron.cpp); standing
// under it costs in her route choice while she sees it and him near its
// release; a falling load she sees with time to clear, she steps clear of;
// with no warning she is hit. Four scenarios against a test hanging slab
// (a 2000 kg plate, debug_hang_slab), on deck 154's north band, east of the
// bay's cut: a straight 22 m run at z -128, past her ladder's foot (5, -129.55)
// and south of her locker (11, -126.2). Not the z -150 catwalk: its plates
// shear under her 2.2 kN (kBoltShearN 2100 N, matron.hpp) — it carries the
// climber, never her.
#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <cmath>
#include <cstdint>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

// The slab hangs over (10, -128), on her walk to the climber.
constexpr double kSlabX = 10.0;
constexpr double kSlabZ = -128.0;
// High hang: the slab's bottom 5.75 m over her head, 1.08 s to fall on her.
constexpr double kHangHigh = 162.0;
// Low hang: the slab's bottom 1.0 m over her head, 0.45 s to fall on her.
constexpr double kHangLow = 157.25;
// The slab's half-diagonal + her radius + margin: the rim she must not cross.
constexpr double kRimM = 1.94;

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

struct Hung {
    double min_dist_m = 1.0e9; // closest she ever came to the slab's centre
    double end_dist_m = 0.0;   // where she ended (2 s after a release)
    bool released = false;
    bool alive = true;
    std::uint32_t knockdowns = 0;
};

// One scenario: her at `from`, the climber standing at `prey`, the slab hung
// at `hang_y` over (10, -128). `release_at_m`: the slab's pin is pulled once
// she is this close under it (0: never). `needs_still`: pull it only while she
// stands (the Turn's windup), so the low hang falls on a still target.
Hung run_hung(const Vector3 from, const Vector3 prey, const double hang_y, const double release_at_m,
              const double seconds, const bool needs_still) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron hung load: settle");
    s.debug_hang_slab(Vector3{kSlabX, hang_y, kSlabZ});
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.debug_place_matron(from);
    tick(s);
    Hung out;
    const std::uint32_t knockdowns0 = s.matron_state().knockdowns;
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    int release_tick = -1;
    for (int i = 0; i < static_cast<int>(seconds) * hz; ++i) {
        if (release_tick >= 0 && i >= release_tick + 2 * hz) {
            break;
        }
        s.debug_set_matron_stage(1);
        if (i % (8 * hz) == 0) {
            s.debug_matron_hear(prey);
        }
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const auto m = s.matron_state();
        const double d = std::hypot(m.position.x - kSlabX, m.position.z - kSlabZ);
        out.min_dist_m = std::min(out.min_dist_m, d);
        out.end_dist_m = d;
        if (!m.alive) {
            out.alive = false;
            break;
        }
        if (release_tick < 0 && release_at_m > 0.0 && d < release_at_m &&
            (!needs_still || m.speed < 0.5)) {
            s.debug_release_hung_slab();
            release_tick = i;
            out.released = true;
        }
    }
    out.knockdowns = s.matron_state().knockdowns - knockdowns0;
    return out;
}

} // namespace

SCRAPERX_GROUP("matron_hung_load", 895, run_matron_hung_load);
SCRAPERX_SELECT("matron-hung-load", run_matron_hung_load);
void run_matron_hung_load() {
    // 1. Sees the hang and him at its release: she stops at its rim, never
    // under it.
    {
        const Hung h = run_hung(Vector3{1.0, 154.0, -128.0}, Vector3{10.0, 154.0, -128.0}, kHangHigh, 0.0, 15.0,
                                false);
        require(h.alive, "matron hung load: seen hang, him at the release: she lives");
        require(h.min_dist_m > 1.5, "matron hung load: seen hang, him at the release: she never walks under it");
        std::cout << "  matron hung load: seen hang, him at release: min " << h.min_dist_m << " m from the slab\n";
    }
    // 2. Does not see him at the release: she walks under it.
    {
        const Hung h = run_hung(Vector3{1.0, 154.0, -128.0}, Vector3{20.0, 154.0, -128.0}, kHangHigh, 0.0, 10.0,
                                false);
        require(h.alive, "matron hung load: him away from the release: she lives");
        require(h.min_dist_m < 1.5, "matron hung load: him away from the release: she walks under the hang");
        std::cout << "  matron hung load: him away from release: min " << h.min_dist_m << " m from the slab\n";
    }
    // 3. It falls in her view with a second to spare: she steps clear, unhit.
    {
        const Hung h = run_hung(Vector3{1.0, 154.0, -128.0}, Vector3{20.0, 154.0, -128.0}, kHangHigh, 1.5, 10.0,
                                false);
        require(h.released, "matron hung load: fall in view: the pin was pulled under her");
        require(h.alive && h.knockdowns == 0, "matron hung load: fall in view: she steps clear, unhit");
        require(h.end_dist_m > kRimM, "matron hung load: fall in view: she ends clear of its footprint");
        std::cout << "  matron hung load: fall in view: ends " << h.end_dist_m << " m from the slab\n";
    }
    // 4. No warning, no time: the low hang falls on her as she stands under
    // it (the Turn's windup, 13 m from him), and hits her.
    {
        const Hung h = run_hung(Vector3{1.0, 154.0, -128.0}, Vector3{23.0, 154.0, -128.0}, kHangLow, 1.5, 12.0,
                                true);
        require(h.released, "matron hung load: no warning: the pin was pulled under her");
        require(h.knockdowns > 0 || !h.alive, "matron hung load: no warning: the low hang hits her");
        std::cout << "  matron hung load: no warning: knockdowns " << h.knockdowns << ", alive " << h.alive
                  << "\n";
    }
    std::cout << "PASS matron hung load: refusal, walk-under, dodge, hit\n";
}
