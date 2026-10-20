#include "sim/impacts.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {

void ImpactLedger::track(const JPH::BodyID body, const std::uint64_t entity, const ImpactSound sound,
                         const float mass_kg) {
    Tracked t;
    t.body = body;
    t.entity = entity;
    t.sound = sound;
    t.mass_kg = mass_kg;
    t.last_velocity = physics_.GetBodyInterface().GetLinearVelocity(body);
    tracked_.push_back(t);
    tick_.reserve(tracked_.size());
}

float ImpactLedger::loudness(const float energy_j) noexcept {
    if (energy_j <= kSilentJ) {
        return 0.0F;
    }
    return std::clamp(std::log10(energy_j / kSilentJ) / std::log10(kFullJ / kSilentJ), 0.0F, 1.0F);
}

void ImpactLedger::post_step(const std::uint64_t carried) {
    const JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    tick_.clear();
    for (Tracked &t : tracked_) {
        if (t.quiet_ticks > 0) {
            --t.quiet_ticks;
        }
        if (!bodies.IsActive(t.body)) {
            // Asleep: it is at rest.
            t.last_velocity = JPH::Vec3::sZero();
            continue;
        }
        const JPH::Vec3 velocity = bodies.GetLinearVelocity(t.body);
        const float change = (velocity - t.last_velocity).Length();
        t.last_velocity = velocity;
        if (t.entity == carried || change < kMinChangeMps) {
            continue;
        }
        const float energy = 0.5F * t.mass_kg * change * change;
        if (energy < kMinEnergyJ || (t.quiet_ticks > 0 && energy < 2.0F * t.last_energy_j)) {
            continue;
        }
        t.quiet_ticks = kQuietTicks;
        t.last_energy_j = energy;
        const JPH::RVec3 at = bodies.GetCenterOfMassPosition(t.body);
        ImpactSnapshot hit;
        hit.entity = t.entity;
        hit.at = {static_cast<double>(at.GetX()), static_cast<double>(at.GetY()), static_cast<double>(at.GetZ())};
        hit.energy_j = energy;
        hit.speed_mps = change;
        hit.mass_kg = t.mass_kg;
        hit.sound = t.sound;
        tick_.push_back(hit);
    }
    if (tick_.empty()) {
        return;
    }
    // The hardest first; among equals, the order they were tracked in.
    std::stable_sort(tick_.begin(), tick_.end(), [](const ImpactSnapshot &a, const ImpactSnapshot &b) {
        return a.energy_j > b.energy_j;
    });
    if (tick_.size() > kMaxPerTick) {
        tick_.resize(kMaxPerTick);
    }
    for (ImpactSnapshot &hit : tick_) {
        hit.sequence = ++sequence_;
        ring_[ring_next_] = hit;
        ring_next_ = (ring_next_ + 1) % kKept;
        ring_count_ = std::min(ring_count_ + 1, kKept);
    }
}

void ImpactLedger::resync() {
    const JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    for (Tracked &t : tracked_) {
        t.last_velocity = bodies.GetLinearVelocity(t.body);
        t.quiet_ticks = 0;
        t.last_energy_j = 0.0F;
    }
    tick_.clear();
}

void ImpactLedger::resync_body(const std::uint64_t entity) {
    const JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    for (Tracked &t : tracked_) {
        if (t.entity == entity) {
            t.last_velocity = bodies.GetLinearVelocity(t.body);
            t.quiet_ticks = 0;
            t.last_energy_j = 0.0F;
        }
    }
}

std::vector<ImpactSnapshot> ImpactLedger::kept() const {
    std::vector<ImpactSnapshot> result;
    result.reserve(ring_count_);
    const std::size_t first = (ring_next_ + kKept - ring_count_) % kKept;
    for (std::size_t i = 0; i < ring_count_; ++i) {
        result.push_back(ring_[(first + i) % kKept]);
    }
    return result;
}

} // namespace scraperx::sim
