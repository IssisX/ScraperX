#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <array>
#include <cstdint>

namespace scraperx::sim {
// A planted hand names a piece of line, not its nearest world-space point.
// Coordinates are metres of unstretched material from end A. The source owns
// all bodies and outlives attached hands; detach hands before source teardown.
struct LineMaterialHandle {
    std::uint64_t generation = 0;
    float coordinate_m = 0;
};
struct LineMaterialSample {
    struct Endpoint {
        JPH::Body *body = nullptr;
        JPH::Vec3 shape_local_point = JPH::Vec3::sZero();
        float weight = 0;
    };
    JPH::RVec3 world_point = JPH::RVec3::sZero();
    JPH::Vec3 point_velocity = JPH::Vec3::sZero();
    std::array<Endpoint, 2> endpoints{};
};
class LineMaterialSource {
public:
    virtual ~LineMaterialSource() = default;
    // Serialized owner thread or collision OnStep, before island construction.
    // No body locks: the caller owns the synchronization boundary. False means
    // stale material identity, lost body, or an unsupported contacted element;
    // it must never be replaced by a chord through a routed cable.
    virtual bool sample_material(LineMaterialHandle, LineMaterialSample &) const = 0;
};
} // namespace scraperx::sim
