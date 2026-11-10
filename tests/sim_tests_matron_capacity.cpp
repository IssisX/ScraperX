// J-044b H.4 (#244): the capacity test. A pure function plus five falsifiers,
// one test each, measured before any motor code lands. The per-limb maxima
// m_i are real capacities with headroom, separate from the healthy shares
// a_i; what an injured limb loses moves to the healthy ones, and the total
// never rises above her one 8 kN / 44 kW budget. Where contacts vanish, the
// delivered force drops honestly: no made-up effort.
//
// J-044b follow-up (#244 comment 6706810879, ChatGPT): F5 was a FALSE GREEN.
// It claimed "grant matches applied lambda or AddForce" but had no actuator,
// no AddForce, no solved lambda, and its speed was 0 — so a 160 kW draw at
// 20 m/s passed. The fix: F5 is renamed to say it is zero-speed accounting
// only (no actuation claimed); F6 drives the same allocation at a real
// 20 m/s and asserts the capped grant (2200 N, 44 kW); F7 applies the
// granted forces to a real 220 kg Jolt body at 20 m/s and checks the
// solver's delivered force against the Budget's charge. (J-046 separately
// does a real joint/hold with a motor and a lambda.)
#include "sim_tests.hpp"
#include "sim/matron_capacity.hpp"
#include "sim/matron_traversal.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/RegisterTypes.h>

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

// F5: the zero-speed grant is accounting only. The allocation, applied
// through Budget::take at zero relative speed, charges exactly what it
// grants — but no actuator, AddForce or solved lambda is behind it, and the
// power cap cannot bite at speed 0. It claims no actuation.
void test_zero_speed_grant_is_accounting_only() {
    const auto a = allocate(make_limbs({1.0F, 1.0F, 1.0F, 1.0F}, {true, true, true, true}, {0.0F, 0.0F, 0.0F, 0.0F}),
                            law::kForceN, law::kForceN);
    traversal::Budget budget{law::kForceN, 0.0F, law::kPowerW, 0.0F, JPH::Vec3::sZero()};
    for (std::size_t i = 0; i < kLimbs; ++i) {
        const JPH::Vec3 granted = budget.take(JPH::Vec3(a.applied_n[i], 0.0F, 0.0F));
        require(near(granted.Length(), a.applied_n[i]),
                "capacity: zero-speed-grant: Budget::take granted less than the allocation asked");
    }
    require(near(budget.used, a.total_n), "capacity: zero-speed-grant: the Budget charged less than the allocation");
    require(near(budget.left, 0.0F), "capacity: zero-speed-grant: the Budget has force left after the full allocation");
    std::printf("  zero-speed-grant: charged %.1f N for %.1f N applied, left %.1f N\n", budget.used, a.total_n,
                budget.left);
}

// F6: the grant is CAPPED at a real speed. Four limbs at 20 m/s, 8000 N
// desired, draw 160 kW: the 44 kW account delivers at most 2200 N of
// positive aligned force (44000 / 20), not 8000 N. The Budget charges
// exactly what it grants, and the force account stays honest. (The RED
// version of this test asserted the old F5 claim at 20 m/s and failed:
// the grant was 2200 N, not 8000 N — the false green, exposed.)
void test_grant_capped_at_speed() {
    const auto a = allocate(make_limbs({1.0F, 1.0F, 1.0F, 1.0F}, {true, true, true, true}, {20.0F, 20.0F, 20.0F, 20.0F}),
                            law::kForceN, law::kForceN);
    require(near(a.total_n, law::kForceN), "capacity: capped-at-speed: the allocation does not reach 8000 N");
    require(near(a.positive_power_w, 160000.0F, 1.0F),
            "capacity: capped-at-speed: the requested draw is not 160 kW");
    traversal::Budget budget{law::kForceN, 0.0F, law::kPowerW, 0.0F, JPH::Vec3(20.0F, 0.0F, 0.0F)};
    float granted_total = 0.0F;
    for (std::size_t i = 0; i < kLimbs; ++i) {
        const JPH::Vec3 granted = budget.take(JPH::Vec3(a.applied_n[i], 0.0F, 0.0F));
        granted_total += granted.Length();
    }
    // 44 kW at 20 m/s delivers at most 2200 N of aligned force.
    require(near(granted_total, law::kPowerW / 20.0F, 1.0F),
            "capacity: capped-at-speed: the delivered force is not the 44 kW cap at 20 m/s");
    require(near(budget.power_used, law::kPowerW, 1.0F),
            "capacity: capped-at-speed: the Budget did not spend the full 44 kW");
    require(budget.power_used <= law::kPowerW + 1.0F,
            "capacity: capped-at-speed: the Budget spent more than the 44 kW account");
    require(near(budget.used, granted_total),
            "capacity: capped-at-speed: the Budget charged something other than what it granted");
    require(near(budget.left, law::kForceN - granted_total),
            "capacity: capped-at-speed: the Budget's force left does not match the shortfall");
    // No negative work credits positive: a braking limb does not refill power.
    traversal::Budget brake{law::kForceN, 0.0F, law::kPowerW, 0.0F, JPH::Vec3(20.0F, 0.0F, 0.0F)};
    const JPH::Vec3 back = brake.take(JPH::Vec3(-2000.0F, 0.0F, 0.0F));
    require(near(back.Length(), 2000.0F), "capacity: capped-at-speed: a braking limb is not granted its force");
    require(near(brake.power_left, law::kPowerW),
            "capacity: capped-at-speed: braking refilled the power account");
    const JPH::Vec3 fwd = brake.take(JPH::Vec3(2000.0F, 0.0F, 0.0F));
    require(near(fwd.Length(), 2000.0F), "capacity: capped-at-speed: the driving limb is not granted after braking");
    require(near(brake.power_used, 40000.0F, 1.0F),
            "capacity: capped-at-speed: the driving limb did not pay the full 40 kW after braking");
    std::printf("  capped-at-speed: granted %.1f N of %.1f N at 20 m/s, power %.1f W\n", granted_total, a.total_n,
                budget.power_used);
}

// A minimal Jolt world for F7 (the exam test's pattern): one layer, one
// body, one step. The runtime is held by the lease, so it shares cleanly
// with any other group in the same process.
constexpr float kDt = 1.0F / 90.0F;
constexpr JPH::ObjectLayer kLayer = 0;

class Layers final : public JPH::BroadPhaseLayerInterface {
public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override {
        return JPH::BroadPhaseLayer(0);
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "ALL"; }
#endif
};

class AllVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
};

class AllPairs final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
};

struct World {
    Layers layers;
    AllVsBroadPhase broad_phase_filter;
    AllPairs pair_filter;
    JPH::PhysicsSystem physics;
    JPH::TempAllocatorImpl temp{8 << 20};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    World() { physics.Init(256, 0, 1024, 1024, layers, broad_phase_filter, pair_filter); }
    void step() { (void)physics.Update(kDt, 1, &temp, &jobs); }
};

// F7: the grant is REALLY applied. The granted forces go through AddForce
// onto a real 220 kg body moving at 20 m/s, and one solver step later the
// body's velocity change is exactly the force the Budget charged: the grant
// matches the applied force in the solver, not just in the accounting.
void test_grant_applied_to_body() {
    detail::JoltRuntimeLease runtime;
    const auto a = allocate(make_limbs({1.0F, 1.0F, 1.0F, 1.0F}, {true, true, true, true}, {20.0F, 20.0F, 20.0F, 20.0F}),
                            law::kForceN, law::kForceN);
    traversal::Budget budget{law::kForceN, 0.0F, law::kPowerW, 0.0F, JPH::Vec3(20.0F, 0.0F, 0.0F)};
    World world;
    JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F)), JPH::RVec3(0.0, 10.0, 0.0),
                                       JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, kLayer);
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY |
                            JPH::EAllowedDOFs::TranslationZ;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = law::kMassKg;
    settings.mGravityFactor = 0.0F;
    settings.mLinearDamping = 0.0F;
    settings.mAngularDamping = 0.0F;
    const JPH::BodyID body = world.physics.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    world.physics.GetBodyInterface().SetLinearVelocity(body, JPH::Vec3(20.0F, 0.0F, 0.0F));
    for (std::size_t i = 0; i < kLimbs; ++i) {
        const JPH::Vec3 granted = budget.take(JPH::Vec3(a.applied_n[i], 0.0F, 0.0F));
        world.physics.GetBodyInterface().AddForce(body, granted);
    }
    const JPH::Vec3 v0 = world.physics.GetBodyInterface().GetLinearVelocity(body);
    world.step();
    const JPH::Vec3 v1 = world.physics.GetBodyInterface().GetLinearVelocity(body);
    const float delivered_n = law::kMassKg * (v1.GetX() - v0.GetX()) / kDt;
    require(near(delivered_n, budget.used, 5.0F),
            "capacity: grant-applied: the solver delivered a force the Budget did not charge");
    require(near(budget.used, law::kPowerW / 20.0F, 1.0F),
            "capacity: grant-applied: the Budget did not charge the 44 kW cap at 20 m/s");
    std::printf("  grant-applied: solver delivered %.1f N, Budget charged %.1f N\n", delivered_n, budget.used);
}
} // namespace

void run_matron_capacity() {
    test_all_healthy();
    test_one_limb_off();
    test_two_limbs_off();
    test_positive_power();
    test_zero_speed_grant_is_accounting_only();
    test_grant_capped_at_speed();
    test_grant_applied_to_body();
    std::printf("PASS scraperx_sim matron-capacity: 7 falsifiers, m_i = 3000 N x 4, "
                "single-loss capacity 9000 N >= 8000 N\n");
}
SCRAPERX_SLICE("matron-capacity", run_matron_capacity);
