#pragma once

#include "sim/simulation/constants.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <cstdint>

namespace scraperx::sim::detail {

[[nodiscard]] JPH::RVec3 spawn_position(const scraperx::sim::InitialSpawn spawn) noexcept;

void approach_relative_horizontal_velocity(JPH::Vec3 &world_velocity,
                                           const JPH::Vec3 reference_velocity,
                                           const double move_input_x,
                                           const double move_input_z,
                                           const float acceleration,
                                           const float delta_seconds,
                                           const float full_speed = kPlayerMaximumRelativeSpeed) noexcept;

[[nodiscard]] float smoothstep(const float edge0, const float edge1, const float value) noexcept;

// Cubic ease over [0, 1] that leaves at `entry_slope` (in units of the mean
// rate) and arrives at rest: a Hermite segment, monotonic for slopes in
// [0, 3], so a body entering it at speed carries that speed in.
[[nodiscard]] float ease_to_rest(const float value, const float entry_slope) noexcept;

// A vault should meet the run on both ends. Equal Hermite end slopes preserve
// the incoming velocity along the landing path, while the middle absorbs the
// remaining distance. Slopes in [0, 3] keep the path monotone.
[[nodiscard]] float ease_with_carried_speed(const float value, const float slope) noexcept;

// WO-008 checkpoint capture: full rigid-body state for one dynamic machine
// member. Kinematic bodies (supports, hoist scoop) deliberately have no
// equivalent -- they are pure functions of the authoritative tick counter
// and are always correct without restoration.
struct BodyCheckpoint final {
    JPH::RVec3 position{JPH::RVec3::sZero()};
    JPH::Quat rotation = JPH::Quat::sIdentity();
    JPH::Vec3 linear_velocity{JPH::Vec3::sZero()};
    JPH::Vec3 angular_velocity{JPH::Vec3::sZero()};
};

[[nodiscard]] BodyCheckpoint capture_body(const JPH::BodyInterface &bodies,
                                          const JPH::BodyID id) noexcept;

void restore_body(JPH::BodyInterface &bodies, const JPH::BodyID id,
                  const BodyCheckpoint &checkpoint) noexcept;

[[nodiscard]] scraperx::sim::Vector3 to_vector3(const JPH::RVec3 value) noexcept;

[[nodiscard]] scraperx::sim::Quaternion to_quaternion(const JPH::Quat value) noexcept;

// Result of one geometry probe against the authoritative Jolt world. Every
// field is derived from an actual cast or clearance query.
struct LedgeProbe final {
    bool valid = false;
    JPH::BodyID ledge_body;
    JPH::SubShapeID ledge_sub_shape;
    std::uint64_t ledge_entity_id = 0;
    JPH::RVec3 wall_point{};
    JPH::Vec3 inward{JPH::Vec3::sZero()};
    JPH::RVec3 ledge_point{};
    JPH::RVec3 landing_centre{};
    JPH::BodyID landing_body;
    JPH::SubShapeID landing_subshape;
    std::uint64_t landing_entity_id = 0;
    float rise = 0.0F;
};

// Step 2 movement. A hold a hand has closed round: the point on the member's
// axis, on the body the member belongs to.
struct Grip final {
    bool valid = false;
    JPH::BodyID body;
    JPH::SubShapeID sub_shape;
    std::uint64_t entity_id = 0;
    JPH::RVec3 point{JPH::RVec3::sZero()};
};

// A ledge's lip, faced along a hang's normal: the top just inside the edge,
// where a hanging body holds below it, and where a climb up it lands.
struct Lip final {
    bool valid = false;
    JPH::BodyID body;
    JPH::SubShapeID sub_shape;
    std::uint64_t entity_id = 0;
    JPH::RVec3 ledge{JPH::RVec3::sZero()};
    JPH::RVec3 hold{JPH::RVec3::sZero()};
    JPH::RVec3 landing{JPH::RVec3::sZero()};
};

// One box of a body's shape, in world space.
struct LeafBox final {
    JPH::RVec3 centre{JPH::RVec3::sZero()};
    JPH::Vec3 axes[3]{};
    JPH::Vec3 half{JPH::Vec3::sZero()};
};

// A member a hand can close round (see kGripMaxSection): its long axis in
// `long_axis`.
[[nodiscard]] bool box_is_hold(const LeafBox &box, std::uint32_t &long_axis) noexcept;

[[nodiscard]] bool leaf_box(const JPH::Body &body, const JPH::SubShapeID &sub_shape,
                            LeafBox &out) noexcept;


} // namespace scraperx::sim::detail
