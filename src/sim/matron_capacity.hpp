#pragma once

// J-044b H.4 (#244): the capacity test. A pure function: given per-limb
// maxima m_i, health h_i and contact, allocate her one budget across her
// limbs and say honestly what it reaches. No motor code, no sim state, no
// Jolt: the test drives it directly. The healthy shares a_i are separate
// from the maxima m_i; what an injured limb loses moves to the healthy
// ones, and the total never rises above the global force ceiling. Where
// contacts vanish, the delivered force drops honestly: no made-up effort.
// Power is the sum of positive parts only (H.2): a limb doing negative
// work never credits another. The power ceiling is enforced where the
// allocation is applied (Budget::take, H.3); this function reports the
// power honestly and never nets it.

#include <array>
#include <cstddef>

namespace scraperx::sim::capacity {

inline constexpr std::size_t kLimbs = 4;  // two arms, two legs

struct Limb {
    float max_n = 0.0F;     // m_i: the limb's physical maximum (N)
    float health = 1.0F;    // h_i in [0,1]; only falling within a fight
    bool contact = true;    // a load point this tick; without one, no effort is made up
    float speed_mps = 0.0F; // relative speed along the limb's line of action (power)
};

struct Allocation {
    std::array<float, kLimbs> applied_n{};  // a_i, each <= m_i * h_i (0 without contact)
    float total_n = 0.0F;                   // sum a_i, <= force_ceiling_n
    float positive_power_w = 0.0F;                   // sum of max(0, a_i * speed_i), never netted
};

// Allocate want_n across the limbs, capped by the global force ceiling and
// each limb's cap (m_i * h_i, zero without contact). The loss of a limb is
// spread equally over the limbs that still have headroom, so a healthy
// allocation redistributes after any single limb at zero.
[[nodiscard]] inline Allocation allocate(const std::array<Limb, kLimbs> &limbs, const float want_n,
                                         const float force_ceiling_n) noexcept {
    Allocation out{};
    // Per-limb caps: m_i * h_i (h_i clamped to [0, 1]), zero without a load
    // point. No contact means no effort is made up on that limb.
    std::array<float, kLimbs> caps{};
    float total_cap = 0.0F;
    for (std::size_t i = 0; i < kLimbs; ++i) {
        const float h = limbs[i].health < 0.0F ? 0.0F : (limbs[i].health > 1.0F ? 1.0F : limbs[i].health);
        const float m = limbs[i].max_n < 0.0F ? 0.0F : limbs[i].max_n;
        caps[i] = limbs[i].contact ? m * h : 0.0F;
        total_cap += caps[i];
    }
    float target = want_n;
    if (target < 0.0F) target = 0.0F;
    if (force_ceiling_n < 0.0F) {
        target = 0.0F;
    } else if (target > force_ceiling_n) {
        target = force_ceiling_n;
    }
    if (target > total_cap) target = total_cap;  // honest shortfall, never backfilled
    // Water-filling: each round gives every limb that still has headroom an
    // equal share of what is left; limbs that hit their cap drop out and the
    // rest redistributes. Fixed order, bounded rounds (one per limb + one).
    float remaining = target;
    std::array<bool, kLimbs> active{};
    std::size_t active_count = 0;
    for (std::size_t i = 0; i < kLimbs; ++i) {
        active[i] = caps[i] > 0.0F;
        if (active[i]) ++active_count;
    }
    while (remaining > 0.0F && active_count > 0) {
        const float share = remaining / static_cast<float>(active_count);
        bool progressed = false;
        for (std::size_t i = 0; i < kLimbs; ++i) {
            if (!active[i]) continue;
            const float headroom = caps[i] - out.applied_n[i];
            const float add = share < headroom ? share : headroom;
            if (add > 0.0F) {
                out.applied_n[i] += add;
                remaining -= add;
                progressed = true;
            }
            if (out.applied_n[i] >= caps[i]) {
                active[i] = false;
                --active_count;
            }
        }
        if (!progressed) break;
    }
    for (std::size_t i = 0; i < kLimbs; ++i) {
        out.total_n += out.applied_n[i];
        // H.2: power is the sum of positive parts only; a limb doing negative
        // work (braking) never credits another.
        const float work = out.applied_n[i] * limbs[i].speed_mps;
        if (work > 0.0F) out.positive_power_w += work;
    }
    return out;
}

// Necessary for full force after any single limb is lost: for every i, the
// other limbs' maxima still cover want_n. Necessary, not sufficient: where
// geometry removes the load points, no healthy-side effort is made up.
[[nodiscard]] inline bool single_loss_ok(const std::array<float, kLimbs> &maxima, const float want_n) noexcept {
    for (std::size_t i = 0; i < kLimbs; ++i) {
        float s = 0.0F;
        for (std::size_t j = 0; j < kLimbs; ++j) {
            if (j != i) s += maxima[j] < 0.0F ? 0.0F : maxima[j];
        }
        if (s < want_n) return false;
    }
    return true;
}

} // namespace scraperx::sim::capacity
