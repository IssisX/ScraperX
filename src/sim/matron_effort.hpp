#pragma once

// J-046 (#257): a read-only ledger of every force and power the Matron puts
// out, by path, every tick. Measurement only: nothing here is read back by
// the simulation, nothing changes a force, a velocity or a budget. Off unless
// a test turns it on (`ledger().enabled`); off, every call returns at once.
//
//  - a constraint's force is its lambda / dt;
//  - a velocity write counts as the impulse it implies, mass * dv / dt;
//  - power is force . velocity (the other body's relative to hers; a
//    Budget's own velocity for a Budget::take); only positive work counts,
//    as in her Budget;
//  - `charged` is what the path took from a traversal::Budget;
//  - `on_her` is what the path adds to her own body by AddForce, summed and
//    held against the force Jolt has on her body as it steps: a force on her
//    that no path records shows as a residual.

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace scraperx::sim::effort {

enum class Path : std::uint8_t {
    Budget = 0,     // every traversal::Budget::take: her feet, the climber's legs and arms, the pipe
    PoleGrip,       // the pole's friction, bought by a squeeze the Budget is charged for
    CrateShove,     // walk_toward's push on a loose crate that stopped her
    Routecraft,     // MatronRoutecraft's own drive
    StockHold,      // her hands' PointConstraint on a held piece (hold_stock: carry, windup)
    GrabSpring,     // the grab hand's spring (hand_pull_n)
    GrabArm,        // the grab hand's arm, a distance limit at her reach
    Hand2Spring,    // her second hand's spring (the wrist, the arm she took)
    Hand2Arm,       // its arm
    Knee,           // her knee's spring, pinning him (the fatality)
    Shove,          // her shove of him: his velocity written
    LetGo,          // the push as she lets him go: a force on him from her account (J-001)
    HandsPush,      // her hands on a piece she shoves or throws: the same newtons her feet's take charged
    PlayerGrip,     // her grips on him (ankle, wrist, fist, knee pin): what reaches him through her hands
    Blow,           // his blows on her (strike, kick, jab, sweep): received, not hers
    Count
};

inline constexpr std::size_t kPaths = static_cast<std::size_t>(Path::Count);

inline const char *path_name(const Path p) noexcept {
    switch (p) {
    case Path::Budget: return "budget";
    case Path::PoleGrip: return "pole-grip";
    case Path::CrateShove: return "crate-shove";
    case Path::Routecraft: return "routecraft";
    case Path::StockHold: return "stock-hold";
    case Path::GrabSpring: return "grab-spring";
    case Path::GrabArm: return "grab-arm";
    case Path::Hand2Spring: return "hand2-spring";
    case Path::Hand2Arm: return "hand2-arm";
    case Path::Knee: return "knee";
    case Path::Shove: return "shove";
    case Path::LetGo: return "let-go";
    case Path::HandsPush: return "hands-push";
    case Path::PlayerGrip: return "player-grip";
    case Path::Blow: return "blow (received)";
    case Path::Count: break;
    }
    return "?";
}

// Her own actuators, summed into the tick's total. Not summed: HandsPush (the
// feet's charged force, carried on through her hands), StockHold (the same
// newtons as the Budget take above, carried on through her hands), PlayerGrip
// (the load that reaches him through the grab spring and arm), Blow (his, not hers).
inline bool summed(const Path p) noexcept {
    return p != Path::HandsPush && p != Path::StockHold && p != Path::PlayerGrip && p != Path::Blow;
}

struct Sample {
    float force_n = 0.0F;
    float watts = 0.0F;
    float charged_n = 0.0F;
    bool active = false;
};

struct Stats {
    double peak_force_n = 0.0;
    double peak_power_w = 0.0;
    double work_j = 0.0;
    double peak_uncharged_n = 0.0;   // applied less charged, the worst tick
    long ticks_active = 0;
    long ticks_over = 0;             // force over her 8 kN or power over her 44 kW
};

struct Ledger {
    bool enabled = false;
    float force_cap_n = 0.0F;        // set by the test from traversal::Limits{}
    float power_cap_w = 0.0F;
    // Paths a test has listed as known leaks: kept out of `total` (the
    // honest paths' sum), still in `total_all`.
    std::array<bool, kPaths> listed{};
    std::array<Sample, kPaths> tick{};
    JPH::Vec3 on_her = JPH::Vec3::sZero();      // what the paths have added to her body this tick
    JPH::Vec3 on_her_at_step = JPH::Vec3::sZero();
    JPH::Vec3 jolt_on_her = JPH::Vec3::sZero(); // her body's accumulated force as Jolt steps
    bool her_read = false;
    // The run, since reset_run().
    std::array<Stats, kPaths> run{};
    Stats total{};
    Stats total_all{};
    double peak_residual_n = 0.0;
    long ticks = 0;
};

inline Ledger &ledger() noexcept {
    static Ledger l;
    return l;
}

inline void add(const Path p, const float force_n, const float power_w, const float charged_n,
                const JPH::Vec3 on_her) noexcept {
    Ledger &l = ledger();
    if (!l.enabled) return;
    Sample &s = l.tick[static_cast<std::size_t>(p)];
    s.force_n += force_n;
    s.watts += std::max(power_w, 0.0F);
    s.charged_n += charged_n;
    s.active = true;
    l.on_her += on_her;
}

// A force `force` from her onto another body moving at `relative` to her.
inline void add_force(const Path p, const JPH::Vec3 force, const JPH::Vec3 relative, const float charged_n,
                      const JPH::Vec3 on_her) noexcept {
    if (!ledger().enabled) return;
    add(p, force.Length(), force.Dot(relative), charged_n, on_her);
}

// A velocity written (or an impulse added) on a body of `mass_kg`: from `before` to `after` in one tick.
inline void add_velocity_write(const Path p, const float mass_kg, const JPH::Vec3 before, const JPH::Vec3 after,
                               const float dt) noexcept {
    if (!ledger().enabled || dt <= 0.0F) return;
    const JPH::Vec3 force = (after - before) * (mass_kg / dt);
    // Its work over the tick is the kinetic energy it gives: force . the mean velocity.
    add(p, force.Length(), force.Dot((before + after) * 0.5F), 0.0F, JPH::Vec3::sZero());
}

inline void begin_tick() noexcept {
    Ledger &l = ledger();
    l.tick = {};
    l.on_her = JPH::Vec3::sZero();
    l.on_her_at_step = JPH::Vec3::sZero();
    l.jolt_on_her = JPH::Vec3::sZero();
    l.her_read = false;
}

inline void her_force_at_step(const JPH::Vec3 jolt) noexcept {
    Ledger &l = ledger();
    if (!l.enabled) return;
    l.jolt_on_her = jolt;
    l.on_her_at_step = l.on_her;
    l.her_read = true;
}

inline void fold(Stats &st, const float force_n, const float power_w, const float uncharged_n, const float dt,
                 const float force_cap, const float power_cap) noexcept {
    ++st.ticks_active;
    st.peak_force_n = std::max(st.peak_force_n, static_cast<double>(force_n));
    st.peak_power_w = std::max(st.peak_power_w, static_cast<double>(power_w));
    st.peak_uncharged_n = std::max(st.peak_uncharged_n, static_cast<double>(uncharged_n));
    st.work_j += static_cast<double>(power_w) * static_cast<double>(dt);
    if (force_n > force_cap || power_w > power_cap) ++st.ticks_over;
}

inline void end_tick(const float dt) noexcept {
    Ledger &l = ledger();
    if (!l.enabled) return;
    ++l.ticks;
    float force = 0.0F;
    float power = 0.0F;
    float uncharged = 0.0F;
    float force_all = 0.0F;
    float power_all = 0.0F;
    float uncharged_all = 0.0F;
    bool any = false;
    bool any_all = false;
    for (std::size_t i = 0; i < kPaths; ++i) {
        const Sample &s = l.tick[i];
        if (!s.active) continue;
        const float u = std::max(0.0F, s.force_n - s.charged_n);
        fold(l.run[i], s.force_n, s.watts, u, dt, l.force_cap_n, l.power_cap_w);
        if (!summed(static_cast<Path>(i))) continue;
        force_all += s.force_n;
        power_all += s.watts;
        uncharged_all += u;
        any_all = true;
        if (!l.listed[i]) {
            force += s.force_n;
            power += s.watts;
            uncharged += u;
            any = true;
        }
    }
    if (any) fold(l.total, force, power, uncharged, dt, l.force_cap_n, l.power_cap_w);
    if (any_all) fold(l.total_all, force_all, power_all, uncharged_all, dt, l.force_cap_n, l.power_cap_w);
    if (l.her_read) {
        l.peak_residual_n = std::max(l.peak_residual_n,
                                     static_cast<double>((l.jolt_on_her - l.on_her_at_step).Length()));
    }
}

inline void reset_run() noexcept {
    Ledger &l = ledger();
    l.run = {};
    l.total = {};
    l.total_all = {};
    l.peak_residual_n = 0.0;
    l.ticks = 0;
    begin_tick();
}

} // namespace scraperx::sim::effort
