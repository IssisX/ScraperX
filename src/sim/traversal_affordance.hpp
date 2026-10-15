#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <array>
#include <cstdint>

namespace JPH { class PhysicsSystem; class Shape; }

namespace scraperx::sim {

enum class TraversalAnchorKind { RigidSubshape, CargoNetMaterial, DeformableFragment };

struct RigidTraversalAnchor {
    JPH::BodyID body;
    JPH::SubShapeID subshape;
    std::uint64_t entity_id = 0;
    // Explicit geometry revision supplied by the owner, never movement progress.
    std::uint64_t geometry_generation = 0;
    // All local vectors use the body's center-of-mass frame.
    JPH::Vec3 local_point = JPH::Vec3::sZero();
    JPH::Vec3 local_normal = JPH::Vec3::sAxisY();
    JPH::Vec3 local_tangent = JPH::Vec3::sAxisX();
    const JPH::Shape *shape_identity = nullptr;
};

// Prospective identity only. No entry is evidence of physical support.
struct ReceiverSurfacePatch {
    std::array<JPH::SubShapeID, 8> subshapes{};
    unsigned count = 0;
    bool overflow = false;
    [[nodiscard]] bool contains(JPH::SubShapeID id) const noexcept {
        if (overflow || count > subshapes.size()) return false;
        for (unsigned i = 0; i < count; ++i) if (subshapes[i] == id) return true;
        return false;
    }
    bool add(JPH::SubShapeID id) noexcept {
        if (overflow || count > subshapes.size()) { overflow = true; return false; }
        if (contains(id)) return true;
        if (count == subshapes.size()) { overflow = true; return false; }
        subshapes[count++] = id;
        return true;
    }
};

struct TraversalAffordance {
    TraversalAnchorKind anchor_kind = TraversalAnchorKind::RigidSubshape;
    RigidTraversalAnchor rigid;
    JPH::BodyID receiver_body;
    JPH::SubShapeID receiver_subshape;
    ReceiverSurfacePatch receiver_patch;
    std::uint64_t receiver_entity_id = 0;
    const JPH::Shape *receiver_shape_identity = nullptr;
    std::uint64_t observed_tick = 0;
    std::uint64_t generation = 0;
    JPH::RVec3 world_point = JPH::RVec3::sZero();
    JPH::Vec3 world_normal = JPH::Vec3::sAxisY();
    JPH::Vec3 world_tangent = JPH::Vec3::sAxisX();
    JPH::Vec3 point_velocity = JPH::Vec3::sZero();
    bool grip_capable = false;
    bool receiver_footing_capable = false;
};

// Prospective rigid geometry only: no body writes, controller or constraints.
// A successful refresh preserves identity, not clearance under a new pose.
// The caller must still validate its current landing/transfer before committing.
class TraversalAffordanceCache {
public:
    [[nodiscard]] std::uint64_t remember(const TraversalAffordance &sample,
                                         std::uint64_t tick) noexcept;
    [[nodiscard]] const TraversalAffordance *refresh(std::uint64_t generation,
        const JPH::PhysicsSystem &system, std::uint64_t tick,
        std::uint64_t max_age_ticks = 8) noexcept;
    // Readback only; find does not refresh or validate against the world.
    [[nodiscard]] const TraversalAffordance *find(std::uint64_t generation) const noexcept;
    void clear() noexcept;
private:
    std::array<TraversalAffordance, 4> entries_{};
    std::array<std::uint64_t, 4> last_refresh_tick_{};
    std::uint64_t generation_ = 0;
    unsigned next_slot_ = 0;
};

} // namespace scraperx::sim
