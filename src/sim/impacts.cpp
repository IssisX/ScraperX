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

bool ImpactLedger::track_machine(const JPH::BodyID body, const std::uint64_t entity, const ImpactSound sound,
                                 const float mass_kg, const bool runs) {
    if (body.IsInvalid() || mass_kg <= 0.0F ||
        physics_.GetBodyInterface().GetMotionType(body) != JPH::EMotionType::Dynamic) {
        return false;
    }
    track(body, entity, sound, mass_kg);
    tracked_.back().runs = runs;
    tracked_.back().machine = true;
    machines_.push_back({entity, runs});
    if (runs) {
        running_.reserve(tracked_.size());
    }
    return true;
}

JPH::BodyID ImpactLedger::machine_body(const std::uint64_t entity) const noexcept {
    if (std::none_of(machines_.begin(), machines_.end(), [entity](const Machine &m) { return m.entity == entity; })) {
        return JPH::BodyID();
    }
    for (const Tracked &t : tracked_) {
        if (t.entity == entity) {
            return t.body;
        }
    }
    return JPH::BodyID();
}

float ImpactLedger::run_loudness(const float mass_kg, const float speed_mps) noexcept {
    if (speed_mps < kRunMinMps) {
        return 0.0F;
    }
    return std::min(kRunMaxLoudness, kRunShare * loudness(0.5F * mass_kg * speed_mps * speed_mps));
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
    running_.clear();
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
        if (t.runs) {
            const float speed = velocity.Length();
            const float run = run_loudness(t.mass_kg, speed);
            if (run > 0.0F) {
                running_.push_back({t.entity, bodies.GetCenterOfMassPosition(t.body), speed, run});
            }
        }
        if (t.machine && t.entity != carried) {
            // The stop: a machine part that comes to rest after carrying
            // kinetic energy hit its stop with that energy. The peak is
            // kept while it moves and counted once the rest is sustained,
            // so a leveled stop is one hit with the energy it arrived with
            // and a trajectory apex is never a stop.
            const float speed = velocity.Length();
            if (speed >= kRunMinMps) {
                t.rest_ticks = 0;
                t.peak_ke_j = std::max(t.peak_ke_j, 0.5F * t.mass_kg * speed * speed);
            } else if (++t.rest_ticks == kStopRestTicks) {
                if (t.peak_ke_j > kSilentJ &&
                    (t.quiet_ticks <= 0 || t.peak_ke_j >= 2.0F * t.last_energy_j)) {
                    t.quiet_ticks = kQuietTicks;
                    t.last_energy_j = t.peak_ke_j;
                    const JPH::RVec3 at = bodies.GetCenterOfMassPosition(t.body);
                    ImpactSnapshot hit;
                    hit.entity = t.entity;
                    hit.at = {static_cast<double>(at.GetX()), static_cast<double>(at.GetY()),
                              static_cast<double>(at.GetZ())};
                    hit.energy_j = t.peak_ke_j;
                    hit.speed_mps = std::sqrt(2.0F * t.peak_ke_j / t.mass_kg);
                    hit.mass_kg = t.mass_kg;
                    hit.sound = t.sound;
                    tick_.push_back(hit);
                }
                t.peak_ke_j = 0.0F;
            }
        }
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
        t.peak_ke_j = 0.0F;
        t.rest_ticks = 0;
    }
    tick_.clear();
    running_.clear();
}

void ImpactLedger::resync_body(const std::uint64_t entity) {
    const JPH::BodyInterface &bodies = physics_.GetBodyInterface();
    for (Tracked &t : tracked_) {
        if (t.entity == entity) {
            t.last_velocity = bodies.GetLinearVelocity(t.body);
            t.quiet_ticks = 0;
            t.last_energy_j = 0.0F;
            t.peak_ke_j = 0.0F;
            t.rest_ticks = 0;
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
