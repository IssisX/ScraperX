// J-038 slice 1: dents and breaks for loose stock, by measured hit.
// Decision measure: peak-per-tick impulse, N.s = struck mass times
// the body's velocity change in one 90 Hz tick (the probe's basis:
// Jolt exposes no applied contact impulse, so the change itself is
// measured, gravity-adjusted as the probe did). The 0.2 s hit total
// is NOT the basis: a long grind inflates it. Bands are ChatGPT's
// cross-check (2026-11-04, #169), confirmed against the probe's
// re-run numbers; where the probe disagrees, the probe wins.
// Placement is NEVER guessed: a dent records only when the peak
// tick has a real pre-solve contact hint for that body. Depth is
// linear within each tier (Muse's interpolation, flagged for
// cross-check on #169); the severe tier runs to a cap. Radius:
// r = clamp(2.5*d + 0.12, 0.12, 0.75). Depths quantize to 1 mm.
// A placement's own settle never dents: after a body is placed or
// spawned, its settle is excluded for kSettleTicks ticks, bounded
// and keyed to the placement event. A real vertical hit past that
// dents by the same bands. Supported means a contact normal
// opposing gravity (normal_y > 0.7 in the body's favour) persisting
// at least 2 ticks; a side-scrape is not support.
#pragma once

#include "sim/loose_stock.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace scraperx::sim::stock_damage {

// A pre-solve contact hint for one stock body, one tick (plain
// floats; the listener converts from Jolt types when it records).
struct ContactHint final {
    std::uint64_t struck = 0;
    float point_x = 0.0F;
    float point_y = 0.0F;
    float point_z = 0.0F;
    float normal_x = 0.0F;
    float normal_y = 0.0F;
    float normal_z = 0.0F;
};

enum class Tier : std::uint8_t {
    None = 0,
    Shallow = 1,
    Medium = 2,
    Severe = 3,
    Damage = 4,  // crates only: hurt, not broken
    Break = 5,   // crates only: at or past the break limit
};

struct Decision final {
    Tier tier = Tier::None;
    float depth_m = 0.0F;
    float radius_m = 0.0F;
    bool breaks = false;
};

// The pure decision: kind + peak-per-tick impulse in, tier + depth
// out. No state, no physics: unit-tested at every tier boundary.
[[nodiscard]] Decision decide(LooseStock::Kind kind, float peak_ns) noexcept;
[[nodiscard]] float radius_for(float depth_m) noexcept;

// One dent, in the struck body's frame at the peak tick: the peak
// tick's place and turn are snapshotted with the hint, so the dent
// travels with the body (and survives a death-restore, which resets
// bodies but keeps their identities).
struct Dent final {
    float point_x = 0.0F;
    float point_y = 0.0F;
    float point_z = 0.0F;
    float normal_x = 0.0F;
    float normal_y = 0.0F;
    float normal_z = 0.0F;
    float depth_m = 0.0F;
    float radius_m = 0.0F;
    Tier tier = Tier::None;
};

inline constexpr std::uint8_t kMaxDents = 8;

// The settle exclusion, in ticks: long enough for a 9 cm test
// set-down (~12-tick fall plus impact) and a severe spawn settle
// (~7-tick fall), short enough that a real hit a quarter second
// after placement still counts.
inline constexpr int kSettleTicks = 20;

struct DamageState final {
    std::array<Dent, kMaxDents> dents{};
    std::uint8_t dent_count = 0;
    bool broken = false;
};

// The 8-cap merge (the plan's rule): past 8 dents, a new dent folds
// into the nearest existing one, deepening it a quarter of the new
// depth, capped at the kind's severe depth.
void merge_dent(DamageState &state, const Dent &dent, float cap_m) noexcept;

// One prefractured piece of a broken crate: its offset from the
// intact body's centre, in the body's frame, and its half extents
// (2 mm shaved, so neighbours touch but do not overlap).
struct BreakPiece final {
    float offset_x = 0.0F;
    float offset_y = 0.0F;
    float offset_z = 0.0F;
    float half_x = 0.0F;
    float half_y = 0.0F;
    float half_z = 0.0F;
    float scatter_x = 0.0F;  // symmetric: sums to zero over all
    float scatter_y = 0.0F;  // pieces, so the break conserves momentum
    float scatter_z = 0.0F;
};

struct BreakGeometry final {
    std::array<BreakPiece, 8> pieces{};
    std::uint8_t count = 0;
};

// The prefracture for a kind: 8 pieces for crates and skid crates,
// none for anything else (slice 1 breaks crates only).
[[nodiscard]] BreakGeometry break_geometry(LooseStock::Kind kind) noexcept;

// Break-piece entities: after the fabricated range, 8 per stock
// body, derived from the broken body's own entity, so a restore can
// never collide two breaks or reuse a restored piece's entity.
// Owned by kit (mechanism_kit.hpp): the entity predicates there
// must recognize them, and the include runs the other way.
inline constexpr std::uint64_t kPieceEntityBase = kit::kPieceEntityBase;
inline constexpr std::uint64_t kPieceEntityLimit = kit::kPieceEntityLimit;

[[nodiscard]] constexpr std::uint64_t piece_entity_for(const std::uint64_t stock_entity,
                                                       const std::uint8_t piece) noexcept {
    return kPieceEntityBase + (stock_entity - LooseStock::kFirstEntity) * 8U + piece;
}

// A break decided this tick, for the world to execute (park the
// intact body, spawn the pieces): the decision is pure, the
// execution is the world's, which owns the kit.
struct BreakAction final {
    std::uint64_t entity = 0;
    std::uint32_t stock_index = 0;
    LooseStock::Kind kind = LooseStock::Kind::Crate;
};

// Per-body state: dent records, the open hit window (peak impulse,
// the peak tick's hint and place/turn, quiet ticks since the last
// active tick), the previous tick's velocity, the support run
// (consecutive ticks with a normal opposing gravity), and the
// settle exclusion left. One
// decision per window: it closes after 6 quiet ticks, and a tick
// counts active at 5 N.s or more (below the smallest dent
// threshold, 12).
class StockDamage final {
public:
    // One tick's observation for one in-scope body: velocity, place
    // and turn now, and this tick's contact hints. Dents record at
    // window close, in the body's frame at the peak tick.
    void observe(std::uint64_t entity, LooseStock::Kind kind, float mass_kg, float vel_x, float vel_y,
                 float vel_z, float pos_x, float pos_y, float pos_z, float rot_x, float rot_y, float rot_z,
                 float rot_w, const std::vector<ContactHint> &hints) noexcept;

    [[nodiscard]] std::vector<BreakAction> take_actions() noexcept;

    [[nodiscard]] const DamageState &state(std::uint64_t entity) const noexcept;

    // A placement event for one body: its settle is excluded for
    // kSettleTicks ticks, and any open hit window is dropped (it
    // belongs to the old pose). Called by the world when it puts a
    // body down at rest (a zero-velocity launch), and by observe()
    // itself on first sight (spawn or load). A launch with velocity
    // is not a placement: its landing is a real hit.
    void note_placed(std::uint64_t entity) noexcept;

    // Forgets one stock entirely: its damage, its open hit window, its
    // last velocity, and its support and settle counts, so the next
    // sight is a first sight. Called by the world when a restore
    // rewinds a stock to a checkpoint that holds it whole: the damage
    // the rewind discards must not keep the restored body unobservable.
    void clear(std::uint64_t entity) noexcept;

    // Drops the transient observation history for every body — the open
    // hit windows, the previous velocities, the support and settle
    // counts, and any break not yet taken — while the recorded damage
    // (states_) stands. Called by the world after a death rewind:
    // the bodies are back at their checkpoint velocities, so the next
    // observation must take those as its baseline, not measure the
    // rewind itself as an impact. Idempotent with restore(), which
    // clears the same history when a save is loaded.
    void resync() noexcept;

    // Save form: one record per damaged or broken body (plain data,
    // trivially copyable, so SaveWriter::list holds it).
    struct SavedDent final {
        float point_x = 0.0F;
        float point_y = 0.0F;
        float point_z = 0.0F;
        float normal_x = 0.0F;
        float normal_y = 0.0F;
        float normal_z = 0.0F;
        float depth_m = 0.0F;
        float radius_m = 0.0F;
        std::uint8_t tier = 0;
    };
    struct SavedDamage final {
        std::uint64_t entity = 0;
        std::uint8_t broken = 0;
        std::uint8_t dent_count = 0;
        std::array<SavedDent, kMaxDents> dents{};
    };
    [[nodiscard]] std::vector<SavedDamage> serialize() const;
    // Validates every record first, then applies: false when a
    // record is not this world's stock, is overfull, or holds a
    // non-finite value, and then nothing is applied.
    bool restore(const std::vector<SavedDamage> &saved, std::size_t stock_count) noexcept;

private:
    struct Window final {
        float peak_ns = 0.0F;
        ContactHint hint{};
        bool has_hint = false;
        int quiet_ticks = 0;
        std::array<float, 3> peak_pos{};
        std::array<float, 4> peak_rot{0.0F, 0.0F, 0.0F, 1.0F};
    };
    std::map<std::uint64_t, DamageState> states_;
    std::map<std::uint64_t, Window> windows_;
    std::map<std::uint64_t, std::array<float, 3>> prev_vel_;
    std::map<std::uint64_t, int> support_ticks_;
    std::map<std::uint64_t, int> settle_ticks_;
    std::vector<BreakAction> breaks_;
};

} // namespace scraperx::sim::stock_damage
