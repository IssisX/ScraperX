// J-038 slice 1: see stock_damage.hpp. The decision, the per-tick
// observation windows and the save form, all real here.
#include "sim/stock_damage.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim::stock_damage {
namespace {

float clamp01(const float t) noexcept {
    return t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
}

float quantize_mm(const float depth) noexcept {
    return std::round(depth * 1000.0F) / 1000.0F;
}

// Linear within [lo, hi): depth runs dlo -> dhi, clamped at dhi.
float within(const float peak, const float lo, const float hi, const float dlo, const float dhi) noexcept {
    return quantize_mm(dlo + (dhi - dlo) * clamp01((peak - lo) / (hi - lo)));
}

} // namespace

float radius_for(const float depth_m) noexcept {
    const float r = 2.5F * depth_m + 0.12F;
    return r < 0.12F ? 0.12F : (r > 0.75F ? 0.75F : r);
}

Decision decide(const LooseStock::Kind kind, const float peak_ns) noexcept {
    Decision d;
    switch (kind) {
    case LooseStock::Kind::Container:
        if (peak_ns < 250.0F) {
            break;
        }
        if (peak_ns < 700.0F) {
            d.tier = Tier::Shallow;
            d.depth_m = within(peak_ns, 250.0F, 700.0F, 0.0F, 0.05F);
        } else if (peak_ns < 2500.0F) {
            d.tier = Tier::Medium;
            d.depth_m = within(peak_ns, 700.0F, 2500.0F, 0.05F, 0.15F);
        } else {
            d.tier = Tier::Severe;
            d.depth_m = within(peak_ns, 2500.0F, 5000.0F, 0.15F, 0.22F);
        }
        break;
    case LooseStock::Kind::Drum:
        if (peak_ns < 12.0F) {
            break;
        }
        if (peak_ns < 30.0F) {
            d.tier = Tier::Shallow;
            d.depth_m = within(peak_ns, 12.0F, 30.0F, 0.0F, 0.02F);
        } else if (peak_ns < 50.0F) {
            d.tier = Tier::Medium;
            d.depth_m = within(peak_ns, 30.0F, 50.0F, 0.02F, 0.05F);
        } else {
            d.tier = Tier::Severe;
            d.depth_m = within(peak_ns, 50.0F, 100.0F, 0.05F, 0.10F);
        }
        break;
    case LooseStock::Kind::Crate:
    case LooseStock::Kind::SkidCrate:
        // The break line is 300 N.s, not 180: measured 2026-11-06, the
        // walking shove that must not break a crate peaks at 198 N.s
        // (the player, first contact) and 267 N.s (Matron's contact on
        // the south crate), while the full forklift ram that must break
        // it peaks at 320 N.s. 180 sat below an ordinary shove, so the
        // shove broke the crate 0.2 s in and the traversal test read
        // the parked body (0.388 m, not the 1 m a shove gives). 300
        // clears the highest measured walking contact and stays below
        // the measured ram. The throw (80 N.s) stays Damage.
        if (peak_ns < 65.0F) {
            break;
        }
        if (peak_ns < 300.0F) {
            d.tier = Tier::Damage;
            d.depth_m = within(peak_ns, 65.0F, 300.0F, 0.0F, 0.06F);
        } else {
            d.tier = Tier::Break;
            d.depth_m = 0.06F;
            d.breaks = true;
        }
        break;
    default:
        break;
    }
    d.radius_m = radius_for(d.depth_m);
    return d;
}

void merge_dent(DamageState &state, const Dent &dent, const float cap_m) noexcept {
    Dent merged = dent;
    merged.depth_m = quantize_mm(dent.depth_m);
    merged.radius_m = radius_for(merged.depth_m);
    if (state.dent_count < kMaxDents) {
        state.dents[state.dent_count++] = merged;
        return;
    }
    // Past 8: fold into the nearest existing dent, deepening it a
    // quarter of the new depth, capped at the kind's severe depth.
    std::uint8_t nearest = 0;
    float nearest_sq = 0.0F;
    for (std::uint8_t i = 0; i < state.dent_count; ++i) {
        const Dent &old = state.dents[i];
        const float dx = old.point_x - merged.point_x;
        const float dy = old.point_y - merged.point_y;
        const float dz = old.point_z - merged.point_z;
        const float sq = dx * dx + dy * dy + dz * dz;
        if (i == 0 || sq < nearest_sq) {
            nearest = i;
            nearest_sq = sq;
        }
    }
    Dent &old = state.dents[nearest];
    old.depth_m = quantize_mm(std::min(cap_m, old.depth_m + 0.25F * merged.depth_m));
    old.radius_m = radius_for(old.depth_m);
    if (static_cast<std::uint8_t>(merged.tier) > static_cast<std::uint8_t>(old.tier)) {
        old.tier = merged.tier;
    }
}

BreakGeometry break_geometry(const LooseStock::Kind kind) noexcept {
    BreakGeometry g;
    if (kind != LooseStock::Kind::Crate && kind != LooseStock::Kind::SkidCrate) {
        return g;
    }
    // The crate's box, 0.96 x 0.88 x 0.96 m, in eight: halves shaved
    // 2 mm so neighbours touch but do not overlap. A skid crate's
    // box rides 0.56 m up its body frame (0.12 m of skids + 0.44 m
    // half height); a plain crate's box is centred on it.
    const float centre_y = kind == LooseStock::Kind::SkidCrate ? 0.56F : 0.0F;
    std::uint8_t k = 0;
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sy : {-1.0F, 1.0F}) {
            for (const float sz : {-1.0F, 1.0F}) {
                g.pieces[k++] = BreakPiece{sx * 0.24F, centre_y + sy * 0.22F, sz * 0.24F, 0.238F, 0.218F,
                                           0.238F,       sx * 0.4F,          sy * 0.2F,  sz * 0.4F};
            }
        }
    }
    g.count = 8;
    return g;
}

namespace {

constexpr float kGravityMps2 = 9.81F;
constexpr float kTickSeconds = 1.0F / 90.0F;
constexpr float kActiveNs = 5.0F;
// A downward change smaller than this is solver noise, not falling:
// a resting container's raw vertical change measures ~4e-05 m/s,
// while one gravity quantum is 0.109 m/s per tick. The bare `< 0`
// gate fired on that noise and fabricated the full 409 N.s quantum
// on a 3,750 kg body (measured on the rebase: her throw at a
// container peaked 547 N.s, of which the vertical 409 was phantom).
// 0.01 m/s sits 250x above the noise and 11x below the quantum.
constexpr float kFallGateMps = 0.01F;
constexpr int kQuietTicks = 6;

// Rotate v by the inverse of the unit quaternion (x, y, z, w).
void rotate_inverse(const std::array<float, 4> &q, const float vx, const float vy, const float vz, float &ox,
                    float &oy, float &oz) noexcept {
    // v' = v + 2 * cross(-q.xyz, cross(-q.xyz, v) + q.w * v), the
    // conjugate rotation: the same formula with the vector part negated.
    const float cx = -q[1] * vz + q[2] * vy;
    const float cy = -q[2] * vx + q[0] * vz;
    const float cz = -q[0] * vy + q[1] * vx;
    const float tx = cx + q[3] * vx;
    const float ty = cy + q[3] * vy;
    const float tz = cz + q[3] * vz;
    ox = vx + 2.0F * (-q[1] * tz + q[2] * ty);
    oy = vy + 2.0F * (-q[2] * tx + q[0] * tz);
    oz = vz + 2.0F * (-q[0] * ty + q[1] * tx);
}

float severe_cap(const LooseStock::Kind kind) noexcept {
    switch (kind) {
    case LooseStock::Kind::Container: return 0.22F;
    case LooseStock::Kind::Drum: return 0.10F;
    default: return 0.06F;  // crates: the damage tier's top
    }
}

bool finite3(const float x, const float y, const float z) noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

// The pick among equally aligned hints: Jolt calls the contact
// callbacks from its worker threads in no fixed order, so "first
// seen wins" on a tie dents a different spot from run to run. The
// contact point, then the normal, orders the hints totally: the same
// impact picks the same hint whatever order the callbacks arrived in.
// A strict weak ordering (no <=, NaN compares equal to everything,
// so a bad value keeps the first hint rather than breaking the sort).
bool hint_before(const ContactHint &a, const ContactHint &b) noexcept {
    if (a.point_x != b.point_x) {
        return a.point_x < b.point_x;
    }
    if (a.point_y != b.point_y) {
        return a.point_y < b.point_y;
    }
    if (a.point_z != b.point_z) {
        return a.point_z < b.point_z;
    }
    if (a.normal_x != b.normal_x) {
        return a.normal_x < b.normal_x;
    }
    if (a.normal_y != b.normal_y) {
        return a.normal_y < b.normal_y;
    }
    return a.normal_z < b.normal_z;
}

} // namespace

void StockDamage::observe(const std::uint64_t entity, const LooseStock::Kind kind, const float mass_kg,
                          const float vel_x, const float vel_y, const float vel_z, const float pos_x,
                          const float pos_y, const float pos_z, const float rot_x, const float rot_y,
                          const float rot_z, const float rot_w, const std::vector<ContactHint> &hints) noexcept {
    // A body breaks once: a broken body is parked and never observed
    // again, but if one ever is — a restore that left it enabled, a
    // save that says broken — measuring it again would decide a
    // second break and spawn a second set of pieces with the same
    // entities. Find, not operator[]: looking must not insert.
    const auto state_it = states_.find(entity);
    if (state_it != states_.end() && state_it->second.broken) {
        return;
    }
    // No previous velocity (first tick, or just after a load): take
    // this tick as the baseline, so a body restored mid-flight does
    // not read its own motion as a hit. First sight is a placement:
    // the body's own settle is excluded for kSettleTicks ticks.
    const auto prev_it = prev_vel_.find(entity);
    if (prev_it == prev_vel_.end()) {
        prev_vel_[entity] = {vel_x, vel_y, vel_z};
        settle_ticks_[entity] = kSettleTicks;
        return;
    }
    const std::array<float, 3> prev = prev_it->second;
    prev_vel_[entity] = {vel_x, vel_y, vel_z};
    // Supported means a contact normal opposing gravity (normal_y >
    // 0.7; the hint normal points into the struck body, so up on a
    // body resting on the ground) persisting at least 2 ticks. A
    // hint by itself is not support: an airborne side-scrape has a
    // lateral normal and leaves the body in flight, and the first
    // landing tick is still in flight too. The adjustment uses this
    // tick's support run: a body in sustained opposing contact
    // through this tick had gravity cancelled by the ground, so its
    // change is raw; a body whose run is 0 or 1 was in flight for
    // the interval, so gravity's share is added back. Judging by
    // last tick instead would fire the adjustment on the tick after
    // a landing, when the body is already at rest, and fabricate a
    // hit out of still air. A speed test cannot tell the two apart:
    // the creep ram's target carries 0.08 m/s of settle wobble, and
    // adjusting it inflates the probe's 587 N.s to 765.
    bool opposing = false;
    for (const ContactHint &h : hints) {
        if (h.struck == entity && h.normal_y > 0.7F) {
            opposing = true;
            break;
        }
    }
    int &run = support_ticks_[entity];
    run = opposing ? run + 1 : 0;
    const bool supported = run >= 2;
    const float dvx = vel_x - prev[0];
    float dvy = vel_y - prev[1];
    const float dvz = vel_z - prev[2];
    // Gravity's share is added back only when gravity was actually
    // acting on the body this tick: it accelerated downward (dvy <
    // 0) or it was already moving down (prev vel_y < 0, the landing
    // and stopping case). A body at rest with no hints — a sleeping
    // body reports no contact manifolds, so its support run can
    // never reach 2 — has neither: its raw change is zero, and
    // adding gravity fabricates a 409 N.s peak on every tick,
    // holding a window open until any hint closes it with a dent.
    // (Measured: her throw at a container dented it shallow,
    // 0.018 m, on a 413 N.s peak whose raw vertical change was
    // 4e-06 m/s.) The ruled support classifier is untouched: it
    // still decides supported; this gate only decides whether the
    // correction is physically applicable on an unsupported tick.
    // The downward test is gated on kFallGateMps, not bare `< 0`:
    // solver noise at 4e-05 m/s is not falling, and a bare `< 0`
    // fires the full quantum on it.
    // One known under-measurement: a body lifted upward from rest
    // (prev vel_y 0, dvy > 0) reads its lift raw, one gravity
    // quantum short; lifts are gradual, and the alternative —
    // adjusting a resting body — fabricates a phantom every tick.
    if (!supported && (dvy < -kFallGateMps || prev[1] < -kFallGateMps)) {
        dvy += kGravityMps2 * kTickSeconds;
    }
    // A placement's own settle never dents: while the exclusion
    // runs, track the velocity and the support run, but open no
    // window and keep none. The bound is the placement event's, not
    // the hit's: a real hit after it counts by the same bands.
    int &settle = settle_ticks_[entity];
    if (settle > 0) {
        --settle;
        windows_[entity] = Window{};
        return;
    }
    const float dv_len = std::sqrt(dvx * dvx + dvy * dvy + dvz * dvz);
    const float peak_ns = mass_kg * dv_len;
    Window &w = windows_[entity];
    if (peak_ns >= kActiveNs) {
        w.quiet_ticks = 0;
        if (peak_ns > w.peak_ns) {
            w.peak_ns = peak_ns;
            // The peak tick's hint: the one whose normal best aligns
            // with the change direction, so a ground contact is not
            // mistaken for a lateral hit. No hint means no dent,
            // whatever the impulse: placement is never guessed.
            w.has_hint = false;
            if (dv_len > 1.0e-6F) {
                const float ix = dvx / dv_len;
                const float iy = dvy / dv_len;
                const float iz = dvz / dv_len;
                float best_dot = 0.0F;
                for (const ContactHint &h : hints) {
                    if (h.struck != entity) {
                        continue;
                    }
                    const float dot = h.normal_x * ix + h.normal_y * iy + h.normal_z * iz;
                    if (!w.has_hint || dot > best_dot || (dot == best_dot && hint_before(h, w.hint))) {
                        w.has_hint = true;
                        best_dot = dot;
                        w.hint = h;
                        w.peak_pos = {pos_x, pos_y, pos_z};
                        w.peak_rot = {rot_x, rot_y, rot_z, rot_w};
                    }
                }
            }
        }
        return;
    }
    if (++w.quiet_ticks < kQuietTicks || w.peak_ns <= 0.0F) {
        return;
    }
    // The window closes: one decision, from the peak.
    const Decision decision = decide(kind, w.peak_ns);
    const bool hinted = w.has_hint;
    const ContactHint hint = w.hint;
    const std::array<float, 3> peak_pos = w.peak_pos;
    const std::array<float, 4> peak_rot = w.peak_rot;
    w = Window{};
    if (decision.tier == Tier::None || !hinted) {
        return;
    }
    if (decision.breaks) {
        DamageState broken;
        broken.broken = true;
        states_[entity] = broken;
        breaks_.push_back(
            BreakAction{entity, static_cast<std::uint32_t>(entity - LooseStock::kFirstEntity), kind});
        return;
    }
    // Into the body's frame at the peak tick, so the dent travels
    // with the body.
    float lx = 0.0F, ly = 0.0F, lz = 0.0F, lnx = 0.0F, lny = 0.0F, lnz = 0.0F;
    rotate_inverse(peak_rot, hint.point_x - peak_pos[0], hint.point_y - peak_pos[1], hint.point_z - peak_pos[2],
                   lx, ly, lz);
    rotate_inverse(peak_rot, hint.normal_x, hint.normal_y, hint.normal_z, lnx, lny, lnz);
    DamageState &state = states_[entity];
    merge_dent(state, Dent{lx, ly, lz, lnx, lny, lnz, decision.depth_m, decision.radius_m, decision.tier},
               severe_cap(kind));
}

void StockDamage::note_placed(const std::uint64_t entity) noexcept {
    settle_ticks_[entity] = kSettleTicks;
    windows_[entity] = Window{};
}

void StockDamage::clear(const std::uint64_t entity) noexcept {
    states_.erase(entity);
    windows_.erase(entity);
    prev_vel_.erase(entity);
    support_ticks_.erase(entity);
    settle_ticks_.erase(entity);
}

std::vector<BreakAction> StockDamage::take_actions() noexcept {
    std::vector<BreakAction> actions = std::move(breaks_);
    breaks_.clear();
    return actions;
}

const DamageState &StockDamage::state(const std::uint64_t entity) const noexcept {
    static const DamageState empty{};
    const auto it = states_.find(entity);
    return it == states_.end() ? empty : it->second;
}

std::vector<StockDamage::SavedDamage> StockDamage::serialize() const {
    std::vector<SavedDamage> out;
    for (const auto &[entity, state] : states_) {
        if (state.dent_count == 0 && !state.broken) {
            continue;
        }
        SavedDamage saved{};
        saved.entity = entity;
        saved.broken = state.broken ? 1 : 0;
        saved.dent_count = state.dent_count;
        for (std::uint8_t i = 0; i < state.dent_count; ++i) {
            const Dent &dent = state.dents[i];
            saved.dents[i] = SavedDent{dent.point_x, dent.point_y, dent.point_z, dent.normal_x,
                                       dent.normal_y, dent.normal_z, dent.depth_m, dent.radius_m,
                                       static_cast<std::uint8_t>(dent.tier)};
        }
        out.push_back(saved);
    }
    return out;
}

void StockDamage::resync() noexcept {
    windows_.clear();
    prev_vel_.clear();
    support_ticks_.clear();
    settle_ticks_.clear();
    breaks_.clear();
}

bool StockDamage::restore(const std::vector<SavedDamage> &saved, const std::size_t stock_count) noexcept {
    std::map<std::uint64_t, DamageState> rebuilt;
    for (const SavedDamage &record : saved) {
        if (record.entity < LooseStock::kFirstEntity ||
            record.entity >= LooseStock::kFirstEntity + stock_count) {
            return false;
        }
        if (record.broken > 1 || record.dent_count > kMaxDents || (record.broken != 0 && record.dent_count != 0)) {
            return false;
        }
        DamageState state;
        state.broken = record.broken != 0;
        state.dent_count = record.dent_count;
        for (std::uint8_t i = 0; i < record.dent_count; ++i) {
            const SavedDent &dent = record.dents[i];
            if (!finite3(dent.point_x, dent.point_y, dent.point_z) ||
                !finite3(dent.normal_x, dent.normal_y, dent.normal_z) || !std::isfinite(dent.depth_m) ||
                !std::isfinite(dent.radius_m) || dent.depth_m < 0.0F || dent.radius_m < 0.0F ||
                dent.tier < 1 || dent.tier > 5) {
                return false;
            }
            state.dents[i] = Dent{dent.point_x, dent.point_y, dent.point_z, dent.normal_x,
                                  dent.normal_y, dent.normal_z, dent.depth_m, dent.radius_m,
                                  static_cast<Tier>(dent.tier)};
        }
        if (!rebuilt.emplace(record.entity, state).second) {
            return false;
        }
    }
    states_ = std::move(rebuilt);
    resync();
    return true;
}

} // namespace scraperx::sim::stock_damage
