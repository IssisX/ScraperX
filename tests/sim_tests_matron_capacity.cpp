// J-044b H.4 (#244): the capacity test. A pure function plus five falsifiers,
// one test each, measured before any motor code lands. The per-limb maxima
// m_i are real capacities with headroom, separate from the healthy shares
// a_i; what an injured limb loses moves to the healthy ones, and the total
// never rises above her one 8 kN / 44 kW budget. Where contacts vanish, the
// delivered force drops honestly: no made-up effort.
#include "sim_tests.hpp"
#include "sim/matron_capacity.hpp"
#include "sim/matron_traversal.hpp"

#include <array>
#include <cstdio>

namespace {
using namespace scraperx::sim;
using namespace scraperx::sim::capacity;

// Today's per-limb maxima m_i, DECLARED not measured: ChatGPT's example and
// Grok's answer both use 3000 N each for four limbs (#244 comments
// 6685709422, 6697145691). The builder posts measured m_i on #244 before any
// numeric gain (H.4/H.1). If these fail the test, report the numbers: do not
// raise them here (a protected change, alone in its own PR).
constexpr std::array<float, kLimbs> kMaximaN = {3000.0F, 3000.0F, 3000.0F, 3000.0F};

bool near(const float a, const float b, const float eps = 0.05F) {
    return a > b - eps && a < b + eps;
}

std::array<Limb, kLimbs> make_limbs(const std::array<float, kLimbs> &health,
                                     const std::array<bool, kLimbs> &contact,
                                     const std::array<float, kLimbs> &speed) {
    std::array<Limb, kLimbs> limbs{};
    for (std::size_t i = 0; i < kLimbs; ++i) {
        limbs[i].max_n = kMaximaN[i];
        limbs[i].health = health[i];
        limbs[i].contact = contact[i];
        limbs[i].speed_mps = speed[i];
    }
    return limbs;
}

void print_sums(const char *what) {
    std::printf("  %s: m_i =", what);
    for (std::size_t i = 0; i < kLimbs; ++i) std::printf(" %.0f", kMaximaN[i]);
    std::printf(" N; sum_{j!=i} m_j =");
    for (std::size_t i = 0; i < kLimbs; ++i) {
        float s = 0.0F;
        for (std::size_t j = 0; j < kLimbs; ++j) {
            if (j != i) s += kMaximaN[j];
        }
        std::printf(" %.0f", s);
    }
    std::printf(" N (want %.0f N)\n", law::kForceN);
}

// F1: all healthy. Every single-limb loss still leaves 8000 N of capacity,
// and the allocation reaches 8000 N with every limb within its cap.
void test_all_healthy() {
    print_sums("all-healthy");
    require(single_loss_ok(kMaximaN, law::kForceN),
            "capacity: all-healthy: some single limb loss leaves under 8000 N of maxima");
    require(!single_loss_ok({2000.0F, 2000.0F, 2000.0F, 2000.0F}, law::kForceN),
            "capacity: all-healthy: the single-loss check passes for 2000 N maxima that cannot cover a loss");
    const auto a = allocate(make_limbs({1.0F, 1.0F, 1.0F, 1.0F}, {true, true, true, true}, {0.0F, 0.0F, 0.0F, 0.0F}),
                            law::kForceN, law::kForceN);
    require(near(a.total_n, law::kForceN), "capacity: all-healthy: the allocation does not reach 8000 N");
    for (std::size_t i = 0; i < kLimbs; ++i) {
        require(a.applied_n[i] <= kMaximaN[i] + 0.05F, "capacity: all-healthy: a limb is over its maximum");
    }
    std::printf("  all-healthy: total %.1f N, applied =", a.total_n);
    for (const float v : a.applied_n) std::printf(" %.1f", v);
    std::printf(" N\n");
}

// F2: one limb off, within caps and contacts. The loss moves to the healthy
// limbs and the total still reaches 8000 N.
void test_one_limb_off() {
    const auto a = allocate(make_limbs({0.0F, 1.0F, 1.0F, 1.0F}, {true, true, true, true}, {0.0F, 0.0F, 0.0F, 0.0F}),
                            law::kForceN, law::kForceN);
    require(near(a.applied_n[0], 0.0F), "capacity: one-limb-off: the dead limb still applies force");
    require(near(a.total_n, law::kForceN), "capacity: one-limb-off: the healthy limbs do not reach 8000 N");
    for (std::size_t i = 1; i < kLimbs; ++i) {
        require(a.applied_n[i] <= kMaximaN[i] + 0.05F, "capacity: one-limb-off: a healthy limb is over its maximum");
    }
    std::printf("  one-limb-off: total %.1f N, applied =", a.total_n);
    for (const float v : a.applied_n) std::printf(" %.1f", v);
    std::printf(" N\n");
}

// F3: two limbs off drops the output. The remaining caps sum to 6000 N, so
// the allocation honestly reaches 6000 N and no more.
void test_two_limbs_off() {
    const auto a = allocate(make_limbs({0.0F, 0.0F, 1.0F, 1.0F}, {true, true, true, true}, {0.0F, 0.0F, 0.0F, 0.0F}),
                            law::kForceN, law::kForceN);
    require(near(a.total_n, 6000.0F), "capacity: two-limbs-off: the total is not the honest 6000 N");
    require(a.total_n < law::kForceN, "capacity: two-limbs-off: the total reaches 8000 N with two limbs gone");
    std::printf("  two-limbs-off: total %.1f N (honestly under 8000 N)\n", a.total_n);
}

// F4: positive power with no cancellation. One limb does +400 W, another
// -200 W (braking, costs nothing): the total is 400 W, never 200 W.
void test_positive_power() {
    const auto a = allocate(make_limbs({1.0F, 1.0F, 1.0F, 1.0F}, {true, true, false, false}, {0.2F, -0.1F, 0.0F, 0.0F}),
                            4000.0F, law::kForceN);
    require(near(a.applied_n[0], 2000.0F) && near(a.applied_n[1], 2000.0F),
            "capacity: positive-power: the two contacting limbs do not share 4000 N");
    require(near(a.positive_power_w, 400.0F), "capacity: positive-power: the power nets the braking limb (want 400 W)");
    std::printf("  positive-power: applied %.1f + %.1f N, power %.1f W (braking limb contributes 0)\n",
                a.applied_n[0], a.applied_n[1], a.positive_power_w);
}

// F5: the grant matches the applied lambda or AddForce. The allocation,
// applied through Budget::take, charges exactly what it grants: no silent
// shortfall, no over-charge.
void test_grant_matches_applied() {
    const auto a = allocate(make_limbs({1.0F, 1.0F, 1.0F, 1.0F}, {true, true, true, true}, {0.0F, 0.0F, 0.0F, 0.0F}),
                            law::kForceN, law::kForceN);
    traversal::Budget budget{law::kForceN, 0.0F, law::kPowerW, 0.0F, JPH::Vec3::sZero()};
    for (std::size_t i = 0; i < kLimbs; ++i) {
        const JPH::Vec3 granted = budget.take(JPH::Vec3(a.applied_n[i], 0.0F, 0.0F));
        require(near(granted.Length(), a.applied_n[i]),
                "capacity: grant-matches-applied: Budget::take granted less than the allocation asked");
    }
    require(near(budget.used, a.total_n), "capacity: grant-matches-applied: the Budget charged less than the allocation");
    require(near(budget.left, 0.0F), "capacity: grant-matches-applied: the Budget has force left after the full allocation");
    std::printf("  grant-matches-applied: charged %.1f N for %.1f N applied, left %.1f N\n", budget.used, a.total_n,
                budget.left);
}
} // namespace

void run_matron_capacity() {
    test_all_healthy();
    test_one_limb_off();
    test_two_limbs_off();
    test_positive_power();
    test_grant_matches_applied();
    std::printf("PASS scraperx_sim matron-capacity: 5 falsifiers, m_i = 3000 N x 4, "
                "single-loss capacity 9000 N >= 8000 N\n");
}
