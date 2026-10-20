#include "sim/rigging_system.hpp"
#include "sim/fixed_guide_cable.hpp"
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace scraperx::sim {
namespace {
struct SpanFilter final : JPH::BodyFilter {
    JPH::BodyID first, second, player;
    bool ShouldCollide(const JPH::BodyID &id) const override {
        return id != first && id != second && id != player;
    }
};
constexpr float kUnloadedN = 60; // Existing Kit unhook policy, in newtons.
constexpr float kInitialSlackM = .025F; // Chosen installation allowance, never preload.
template <class V> bool finite_vector(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
}

RiggingSystem::RiggingSystem(JPH::PhysicsSystem &world, kit::Kit &kit) : world_(world), kit_(kit) {}
RiggingSystem::~RiggingSystem() { for (auto &slot : slots_) disconnect(slot); }
const RiggingSystem::Eye *RiggingSystem::available_eye(std::size_t index) const {
    return index < eye_count_ && body(eyes_[index]) ? &eyes_[index] : nullptr;
}

JPH::Body *RiggingSystem::body(const Eye &eye) const {
    if (!eye.body.valid() || kit_.body_id(eye.body) != eye.identity || !kit_.body_enabled(eye.body)) return nullptr;
    return world_.GetBodyLockInterfaceNoLock().TryGetBody(eye.identity);
}
JPH::RVec3 RiggingSystem::point(std::uint16_t eye, JPH::Vec3 local) const {
    auto *b = body(eyes_[eye - 1]);
    return b ? b->GetWorldTransform() * local : JPH::RVec3::sZero();
}
std::uint16_t RiggingSystem::add_eye(kit::BodyIndex index, JPH::Vec3 local, float rating) {
    if (eye_count_ == kEyeCapacity || !index.valid() || !kit_.body_enabled(index) ||
        !std::isfinite(rating) || rating <= 0 || !finite_vector(local))
        throw std::invalid_argument("Rigging add_eye requires live Kit body, finite local point/rating and free capacity");
    auto &eye = eyes_[eye_count_++];
    eye.body = index; eye.identity = kit_.body_id(index); eye.local = local; eye.rating_n = rating;
    return static_cast<std::uint16_t>(eye_count_);
}
std::uint16_t RiggingSystem::add_fixed_guide(kit::BodyIndex index, JPH::Vec3 local,
        JPH::Vec3 incoming, JPH::Vec3 outgoing, float rating) {
    if (!index.valid() || kit_.body_dynamic(index) || !finite_vector(incoming) || !finite_vector(outgoing))
        throw std::invalid_argument("Rigging fixed guide requires a fixed support and finite shape-local routing points");
    const auto id = add_eye(index, local, rating);
    auto &eye = eyes_[id - 1]; eye.kind = Kind::FixedGuide;
    eye.guide_in = incoming; eye.guide_out = outgoing;
    return id;
}
std::uint16_t RiggingSystem::add_powered_mount(kit::BodyIndex index, JPH::Vec3 local,
        const PoweredCable::Config &profile, float rating) {
    return add_powered_mount(index, local, local, profile, rating);
}
std::uint16_t RiggingSystem::add_powered_mount(kit::BodyIndex index, JPH::Vec3 local,
        JPH::Vec3 control, const PoweredCable::Config &profile, float rating) {
    if (!finite_vector(control))
        throw std::invalid_argument("Rigging powered mount requires a finite shape-local control point");
    // Endpoints and initial rest length belong to the accepted installation.
    if (!(std::isfinite(profile.min_length_m) && profile.min_length_m > 0 &&
          std::isfinite(profile.max_length_m) && profile.max_length_m >= profile.min_length_m &&
          std::isfinite(profile.max_speed_mps) && profile.max_speed_mps > 0 &&
          std::isfinite(profile.pull_force_n) && profile.pull_force_n > 0 &&
          std::isfinite(profile.brake_force_n) && profile.brake_force_n > 0 &&
          std::isfinite(profile.electrical_power_w) && profile.electrical_power_w > 0 &&
          std::isfinite(profile.efficiency) && profile.efficiency > 0 && profile.efficiency <= 1 &&
          std::isfinite(profile.capacity_j) && profile.capacity_j >= 0))
        throw std::invalid_argument("Rigging powered mount requires finite actuator limits and source capacity");
    const auto id = add_eye(index, local, rating);
    eyes_[id - 1].kind = Kind::PoweredMount;
    auto &source = power_sources_[id - 1]; source.profile = profile; source.control_local = control;
    source.state.energy_j = profile.capacity_j;
    return id;
}
bool RiggingSystem::power_operable(const Slot &slot) const {
    if (!equipped_ || !allowed_ || !slot.powered || !valid_record(slot.record)) return false;
    const auto &mount = eyes_[slot.record.first - 1];
    const auto at = interaction_point(slot.record.first);
    return (at - hand_).Length() <= kInstallReachM && clear_span(hand_, at, player_, mount.identity);
}
void RiggingSystem::stop_power() {
    for (auto &slot : slots_) if (slot.powered) slot.powered->pre_step(0);
}
void RiggingSystem::set_power_input(float effort) {
    stop_power();
    if (!std::isfinite(effort) || selected_ < 0) return;
    auto &slot = slots_[std::size_t(selected_)];
    if (power_operable(slot)) slot.powered->pre_step(std::clamp(effort, -1.F, 1.F));
}
void RiggingSystem::collision_step(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto &slot : slots_) if (slot.powered) {
        slot.powered->collision_step(dt); slot.collision_pending = true;
    }
}
std::uint64_t RiggingSystem::eye_token(std::size_t i) const { return (std::uint64_t(epoch_) << 32) | (i + 1); }
std::uint64_t RiggingSystem::line_token(std::size_t i) const {
    return (std::uint64_t(epoch_) << 32) | (std::uint64_t(slots_[i].record.generation) << 8) | (i + 1);
}
int RiggingSystem::line_index(std::uint64_t token) const {
    const auto index = unsigned(token & 255);
    if (!index || index > kCapacity || token != line_token(index - 1) || !slots_[index - 1].record.occupied) return -1;
    return int(index - 1);
}
bool RiggingSystem::reject(Failure reason) { failure_ = reason; ++serial_; return false; }
void RiggingSystem::equip(bool value) { stop_power(); equipped_ = value; target_ = 0; allowed_ = false; }
JPH::RVec3 RiggingSystem::interaction_point(std::uint16_t id) const {
    const auto &eye = eyes_[id - 1];
    return point(id, eye.kind == Kind::PoweredMount ? power_sources_[id - 1].control_local : eye.local);
}
bool RiggingSystem::clear_span(JPH::RVec3 from, JPH::RVec3 to, JPH::BodyID first, JPH::BodyID second) const {
    const JPH::Vec3 delta(to - from);
    if (delta.LengthSq() < 1.e-8F) return true;
    SpanFilter filter; filter.first = first; filter.second = second; filter.player = player_;
    JPH::RayCastResult hit;
    return !world_.GetNarrowPhaseQuery().CastRay(JPH::RRayCast(from, delta), hit, {}, {}, filter);
}
void RiggingSystem::aim(JPH::RVec3 hand, JPH::Vec3 direction, JPH::BodyID player, bool allowed) {
    hand_ = hand; player_ = player;
    allowed_ = allowed && equipped_ && finite_vector(hand) && finite_vector(direction);
    target_ = 0;
    aim_ = direction.NormalizedOr(-JPH::Vec3::sAxisZ());
    if (selected_ >= 0 && !power_operable(slots_[std::size_t(selected_)])) stop_power();
    if (!allowed_) return;
    float best = -1;
    for (std::size_t i = 0; i < eye_count_; ++i) {
        if (!body(eyes_[i])) continue;
        const auto at = interaction_point(std::uint16_t(i + 1));
        const JPH::Vec3 delta(at - hand_);
        const float distance = delta.Length();
        if (distance > kInstallReachM || distance < .02F) continue;
        const float alignment = delta.Dot(aim_) / distance;
        if (alignment < .8F || !clear_span(hand_, at, player_, eyes_[i].identity)) continue;
        const float score = alignment - .08F * distance;
        if (score > best) { best = score; target_ = eye_token(i); }
    }
}
bool RiggingSystem::valid_record(const Record &r) const {
    if (!r.first || !r.second || r.first > eye_count_ || r.second > eye_count_ ||
        r.first == r.second || !std::isfinite(r.paid_length_m) || r.paid_length_m <= 0 ||
        !std::isfinite(r.rating_n) || r.rating_n <= 0) return false;
    const auto &a = eyes_[r.first - 1], &b = eyes_[r.second - 1];
    if (!body(a) || !body(b) || b.kind != Kind::End || a.identity == b.identity) return false;
    if (a.kind == Kind::PoweredMount) {
        const auto &p = power_sources_[r.first - 1].profile;
        return !r.guide && body(b)->IsDynamic() && r.paid_length_m >= p.min_length_m &&
               r.paid_length_m <= p.max_length_m;
    }
    if (!r.guide || r.guide > eye_count_ || a.kind != Kind::End) return false;
    const auto &g = eyes_[r.guide - 1];
    return body(g) && g.kind == Kind::FixedGuide && !body(g)->IsDynamic() &&
        (body(a)->IsDynamic() || body(b)->IsDynamic());
}
bool RiggingSystem::reachable(const Record &r) const {
    for (auto id : {r.first, r.second}) {
        const auto &eye = eyes_[id - 1];
        const auto at = interaction_point(id);
        if ((at - hand_).Length() <= kInstallReachM && clear_span(hand_, at, player_, eye.identity)) return true;
    }
    return false;
}
std::size_t RiggingSystem::polyline(std::size_t index, std::array<JPH::RVec3, 4> &out) const {
    if (index >= kCapacity) return 0;
    const auto &r = slots_[index].record;
    if (!r.occupied || r.broken || !valid_record(r)) return 0;
    if (eyes_[r.first - 1].kind == Kind::PoweredMount) {
        out[0] = point(r.first, eyes_[r.first - 1].local);
        out[1] = point(r.second, eyes_[r.second - 1].local);
        return 2;
    }
    const auto &g = eyes_[r.guide - 1];
    out = {point(r.first, eyes_[r.first - 1].local),
           point(r.guide, r.reversed ? g.guide_out : g.guide_in),
           point(r.guide, r.reversed ? g.guide_in : g.guide_out),
           point(r.second, eyes_[r.second - 1].local)};
    return out.size();
}
void RiggingSystem::connect(Slot &slot) {
    if (!slot.record.occupied || slot.record.broken || !valid_record(slot.record)) return;
    const auto index = std::size_t(&slot - slots_.data());
    std::array<JPH::RVec3, 4> p;
    if (!polyline(index, p)) return;
    if (eyes_[slot.record.first - 1].kind == Kind::PoweredMount) {
        auto &source = power_sources_[slot.record.first - 1];
        auto config = source.profile;
        config.support = eyes_[slot.record.first - 1].identity;
        config.load = eyes_[slot.record.second - 1].identity;
        config.support_eye = eyes_[slot.record.first - 1].local;
        config.load_eye = eyes_[slot.record.second - 1].local;
        config.initial_length_m = slot.record.paid_length_m;
        slot.powered = std::make_unique<PoweredCable>(world_, config);
        auto state = source.state; state.rest_length_m = config.initial_length_m;
        slot.powered->restore(state);
        for (auto id : {config.support, config.load})
            if (world_.GetBodyLockInterfaceNoLock().TryGetBody(id)->IsDynamic())
                world_.GetBodyInterface().ActivateBody(id);
        return;
    }
    const float legs = slot.record.paid_length_m - float((p[2] - p[1]).Length());
    if (legs <= .01F) { slot.record.broken = true; return; }
    slot.constraint = make_fixed_guide_cable(*body(eyes_[slot.record.first - 1]), p[0], p[1],
        *body(eyes_[slot.record.second - 1]), p[3], p[2], 1, legs);
    world_.AddConstraint(slot.constraint);
    auto &bodies = world_.GetBodyInterface();
    bodies.ActivateBody(eyes_[slot.record.first - 1].identity);
    bodies.ActivateBody(eyes_[slot.record.second - 1].identity);
}
void RiggingSystem::disconnect(Slot &slot) {
    if (slot.powered) {
        const auto state = slot.powered->state();
        power_sources_[slot.record.first - 1].state = state;
        slot.record.paid_length_m = state.rest_length_m;
        slot.powered.reset();
        for (const auto id : {slot.record.first, slot.record.second}) {
            auto *end = body(eyes_[id - 1]);
            if (end && end->IsDynamic()) world_.GetBodyInterface().ActivateBody(end->GetID());
        }
    }
    if (slot.constraint) {
        world_.RemoveConstraint(slot.constraint);
        slot.constraint = nullptr;
        for (const auto id : {slot.record.first, slot.record.second}) {
            if (!id || id > eye_count_) continue;
            auto *end = body(eyes_[id - 1]);
            if (end && end->IsDynamic()) world_.GetBodyInterface().ActivateBody(end->GetID());
        }
    }
    slot.tension_n = 0; slot.overload_ticks = 0; slot.solved = false; slot.collision_pending = false;
}
bool RiggingSystem::action(std::uint64_t token) {
    if (!allowed_) return reject(Failure::UnsafeContext);
    if (!token || token != target_ || std::uint32_t(token >> 32) != epoch_) return reject(Failure::StaleTarget);
    const auto id = std::uint16_t(token & 0xffffffffU);
    if (!id || id > eye_count_) return reject(Failure::StaleTarget);
    const auto &eye = eyes_[id - 1];
    if (!body(eye)) return reject(Failure::StaleTarget);
    if ((interaction_point(id) - hand_).Length() > kInstallReachM) return reject(Failure::OutOfReach);
    if (!first_) {
        if (eye.kind != Kind::End && eye.kind != Kind::PoweredMount) return reject(Failure::WrongTarget);
        if (eye.kind == Kind::PoweredMount)
            for (const auto &slot : slots_)
                if (slot.record.occupied && slot.record.first == id) return reject(Failure::Capacity);
        first_ = id; failure_ = Failure::None; ++serial_; return true;
    }
    const bool powered = eyes_[first_ - 1].kind == Kind::PoweredMount;
    if (powered && eye.kind != Kind::End) return reject(Failure::WrongTarget);
    if (eye.kind == Kind::FixedGuide) { guide_ = id; failure_ = Failure::None; ++serial_; return true; }
    if (!powered && !guide_) return reject(Failure::MissingGuide);
    auto free = std::find_if(slots_.begin(), slots_.end(), [](const Slot &s) { return !s.record.occupied; });
    if (free == slots_.end()) return reject(Failure::Capacity);
    Record r; r.first = first_; r.second = id; r.guide = guide_; r.occupied = true;
    const auto start = point(first_, eyes_[first_ - 1].local), end = point(id, eye.local);
    r.paid_length_m = powered ? float((end - start).Length()) + kInitialSlackM : 1;
    if (powered) r.paid_length_m = std::max(r.paid_length_m, power_sources_[first_ - 1].profile.min_length_m);
    r.rating_n = std::min(eyes_[first_ - 1].rating_n, eye.rating_n);
    if (!powered) r.rating_n = std::min(r.rating_n, eyes_[guide_ - 1].rating_n);
    if (!valid_record(r)) return reject(Failure::Degenerate);
    if (powered) {
        if ((end - start).Length() < .05F) return reject(Failure::Degenerate);
        if (!clear_span(start, end, eyes_[first_ - 1].identity, eye.identity)) return reject(Failure::Obstructed);
    } else {
        const auto &g = eyes_[guide_ - 1];
        auto a = point(guide_, g.guide_in), b = point(guide_, g.guide_out);
        r.reversed = (start - b).LengthSq() + (end - a).LengthSq() <
                     (start - a).LengthSq() + (end - b).LengthSq();
        if (r.reversed) std::swap(a, b);
        if ((start - a).Length() < .05F || (end - b).Length() < .05F) return reject(Failure::Degenerate);
        if (!clear_span(start, a, eyes_[first_ - 1].identity, g.identity) ||
            !clear_span(a, b, g.identity, g.identity) || !clear_span(b, end, g.identity, eye.identity))
            return reject(Failure::Obstructed);
        r.paid_length_m = float((start - a).Length() + (a - b).Length() + (b - end).Length()) + kInitialSlackM;
    }
    stop_power();
    r.generation = free->record.generation + 1;
    free->record = r; connect(*free);
    if (!free->constraint && !free->powered) {
        free->record.occupied = false; return reject(Failure::Degenerate);
    }
    selected_ = int(free - slots_.begin()); first_ = guide_ = 0; failure_ = Failure::None; ++serial_;
    return true;
}
void RiggingSystem::undo() {
    if (guide_) guide_ = 0; else first_ = 0;
    failure_ = Failure::None; ++serial_;
}
std::uint64_t RiggingSystem::line_handle(std::size_t i) const {
    return i < kCapacity && slots_[i].record.occupied ? line_token(i) : 0;
}
bool RiggingSystem::select(std::uint64_t token) {
    if (!allowed_) return reject(Failure::UnsafeContext);
    const int i = line_index(token);
    if (i < 0) return reject(Failure::StaleTarget);
    if (!reachable(slots_[std::size_t(i)].record)) return reject(Failure::OutOfReach);
    if (selected_ != i) stop_power();
    selected_ = i; failure_ = Failure::None; ++serial_; return true;
}
bool RiggingSystem::unclip(std::uint64_t token) {
    if (!allowed_) return reject(Failure::UnsafeContext);
    const int i = line_index(token);
    if (i < 0) return reject(Failure::StaleTarget);
    auto &slot = slots_[std::size_t(i)];
    if (((slot.constraint || slot.powered) && !slot.solved) || slot.tension_n > kUnloadedN) return reject(Failure::Loaded);
    const auto &r = slot.record;
    if (!reachable(r))
        return reject(Failure::OutOfReach);
    disconnect(slot); slot.record.occupied = false; ++slot.record.generation;
    if (selected_ == i) selected_ = -1;
    failure_ = Failure::None; ++serial_; return true;
}
bool RiggingSystem::cut(std::uint64_t token) {
    if (!allowed_) return reject(Failure::UnsafeContext);
    const int i = line_index(token);
    if (i < 0) return reject(Failure::StaleTarget);
    auto &slot = slots_[std::size_t(i)];
    const auto &r = slot.record;
    if (!reachable(r))
        return reject(Failure::OutOfReach);
    disconnect(slot); slot.record.broken = true; failure_ = Failure::None; ++serial_; return true;
}
void RiggingSystem::post_step(float dt) {
    if (!std::isfinite(dt) || !(dt > 0)) return;
    for (auto &slot : slots_) {
        if (!slot.constraint && !slot.powered) continue;
        if (!valid_record(slot.record)) {
            disconnect(slot); slot.record.broken = true; reject(Failure::StaleTarget); continue;
        }
        if (slot.powered) {
            slot.powered->post_step();
            const auto state = slot.powered->state();
            power_sources_[slot.record.first - 1].state = state;
            slot.record.paid_length_m = state.rest_length_m;
            slot.tension_n = state.tensile_force_n;
            if (!power_operable(slot) || selected_ != int(&slot - slots_.data())) slot.powered->pre_step(0);
            if (!slot.collision_pending || !(state.last_collision_dt > 0)) continue;
            slot.collision_pending = false;
        } else slot.tension_n = std::abs(slot.constraint->GetTotalLambdaPosition()) / dt;
        slot.solved = true;
        // The powered row publishes float impulse / float interval. At its
        // exact rated force this can round one ULP above the rating.
        const float limit = slot.powered ? std::nextafter(slot.record.rating_n,
            std::numeric_limits<float>::infinity()) : slot.record.rating_n;
        slot.overload_ticks = slot.tension_n > limit ? slot.overload_ticks + 1 : 0;
        if (slot.overload_ticks >= 3) {
            disconnect(slot); slot.record.broken = true; reject(Failure::Overload);
        }
    }
}
RiggingSystem::Readback RiggingSystem::readback() const {
    Readback out; out.equipped = equipped_; out.suspended = equipped_ && !allowed_;
    out.draft = first_ != 0; out.guide_threaded = guide_ != 0;
    out.target = target_; out.event_serial = serial_; out.failure = failure_;
    if (target_) {
        const auto id = std::uint16_t(target_ & 0xffffffffU);
        out.target_kind = eyes_[id - 1].kind; out.target_point = interaction_point(id);
        if (out.target_kind != Kind::FixedGuide) {
            for (std::size_t i = 0; i < kCapacity; ++i) {
                const auto &slot = slots_[i];
                if (slot.record.occupied && !slot.record.broken && (slot.powered || slot.constraint) &&
                    (slot.record.first == id || slot.record.second == id) && valid_record(slot.record)) {
                    out.target_line = line_token(i);
                    break;
                }
            }
        }
    }
    for (const auto &s : slots_) if (s.record.occupied) ++out.installed_count;
    if (selected_ >= 0 && slots_[std::size_t(selected_)].record.occupied) {
        const auto i = std::size_t(selected_); out.selected = line_token(i);
        out.tension_n = slots_[i].tension_n;
        std::array<JPH::RVec3, 4> p;
        if (slots_[i].powered) {
            const auto state = slots_[i].powered->state();
            out.power_selected = true; out.power_operable = power_operable(slots_[i]);
            out.braking = state.braking; out.energy_cutoff = state.energy_cutoff;
            out.rest_length_m = state.rest_length_m; out.actual_length_m = state.actual_length_m;
            out.target_speed_mps = state.target_speed_mps; out.energy_j = state.energy_j;
            out.capacity_j = power_sources_[slots_[i].record.first - 1].profile.capacity_j;
            out.source_energy_used_j = state.source_energy_used_j; out.motor_work_j = state.positive_motor_work_j;
            out.heat_j = state.heat_j; out.slack_m = std::max(0.F, state.rest_length_m - state.actual_length_m);
        } else if (polyline(i, p)) out.slack_m = std::max(0.F, slots_[i].record.paid_length_m -
            float((p[1] - p[0]).Length() + (p[2] - p[1]).Length() + (p[3] - p[2]).Length()));
    }
    return out;
}
std::size_t RiggingSystem::preview(std::array<JPH::RVec3, 4> &out) const {
    if (!equipped_ || !first_ || !body(eyes_[first_ - 1])) return 0;
    out[0] = point(first_, eyes_[first_ - 1].local);
    const auto end = target_ ? point(std::uint16_t(target_ & 0xffffffffU),
        eyes_[std::uint16_t(target_ & 0xffffffffU) - 1].local) : hand_ + aim_ * 1.5F;
    if (!guide_) { out[1] = end; return 2; }
    const auto &g = eyes_[guide_ - 1];
    out[1] = point(guide_, g.guide_in); out[2] = point(guide_, g.guide_out); out[3] = end;
    return 4;
}
RiggingSystem::Checkpoint RiggingSystem::capture() const {
    Checkpoint out;
    for (std::size_t i = 0; i < kEyeCapacity; ++i) out.power_sources[i] = power_sources_[i].state;
    for (std::size_t i = 0; i < kCapacity; ++i) {
        out.lines[i] = slots_[i].record;
        if (slots_[i].powered) {
            const auto state = slots_[i].powered->state();
            out.powered_lines[i] = state; out.lines[i].paid_length_m = state.rest_length_m;
            out.power_sources[slots_[i].record.first - 1] = state;
        }
    }
    return out;
}
void RiggingSystem::detach_for_restore() {
    for (auto &slot : slots_) disconnect(slot);
    ++epoch_; first_ = guide_ = 0; target_ = 0; selected_ = -1; allowed_ = equipped_ = false;
    failure_ = Failure::None; ++serial_;
}
void RiggingSystem::restore(const Checkpoint &state) {
    detach_for_restore();
    for (std::size_t i = 0; i < eye_count_; ++i)
        if (eyes_[i].kind == Kind::PoweredMount) power_sources_[i].state = state.power_sources[i];
    std::array<bool, kEyeCapacity> mounted{};
    for (std::size_t i = 0; i < kCapacity; ++i) {
        auto &slot = slots_[i]; slot.record = state.lines[i];
        if (slot.record.occupied && !valid_record(slot.record)) {
            slot.record.broken = true; reject(Failure::StaleTarget);
        }
        if (slot.record.occupied && !slot.record.broken && eyes_[slot.record.first - 1].kind == Kind::PoweredMount) {
            if (mounted[slot.record.first - 1]) { slot.record.broken = true; reject(Failure::Capacity); }
            mounted[slot.record.first - 1] = true;
        }
        connect(slot);
        if (slot.powered) {
            auto saved = state.powered_lines[i];
            // Device inventory is authoritative; cable owns only its paid rest length.
            const auto &source = power_sources_[slot.record.first - 1].state;
            saved.energy_j = source.energy_j;
            saved.source_energy_used_j = source.source_energy_used_j;
            saved.positive_motor_work_j = source.positive_motor_work_j;
            saved.absorbed_work_j = source.absorbed_work_j;
            saved.command_work_bound_j = source.command_work_bound_j;
            saved.heat_j = source.heat_j;
            saved.rest_length_m = slot.record.paid_length_m;
            slot.powered->restore(saved);
        }
        if (slot.record.occupied && selected_ < 0) selected_ = int(i);
    }
}

} // namespace scraperx::sim
