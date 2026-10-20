#pragma once
#include <Jolt/Jolt.h>
#include <array>
#include <cstdint>

namespace scraperx::sim {
// A local geodesic on the straight extrusion of an actual BoxShape, including
// its real sharp faces/corner normal cones when convex radius is exactly zero.
// Axis and traversal direction are persistent contact topology, not a wall pin.
struct RoundedBoxLineTopology {
    int axis = -1;
    int direction = 0; // +1/-1 around local cross-section; 0 selects initially.
};
struct RoundedBoxLineQuery {
    JPH::RMat44 leaf_to_world = JPH::RMat44::sIdentity(); // rigid, actual leaf COM
    JPH::Vec3 half_extent = JPH::Vec3::sZero(); // BoxShape::GetHalfExtent()
    double convex_radius = 0; // BoxShape::GetConvexRadius(), not a chosen offset
    JPH::RVec3 endpoint_a = JPH::RVec3::sZero();
    JPH::RVec3 endpoint_b = JPH::RVec3::sZero();
    RoundedBoxLineTopology retained;
};
enum class RoundedBoxLineStatus : std::uint8_t { Invalid, Clear, Wrapped, Unsupported };
struct RoundedBoxLineResult {
    RoundedBoxLineStatus status = RoundedBoxLineStatus::Invalid;
    RoundedBoxLineTopology topology;
    double length_m = 0;
    JPH::Vec3 gradient_a = JPH::Vec3::sZero(); // d length / d world endpoint
    JPH::Vec3 gradient_b = JPH::Vec3::sZero();
    // Surface samples only. Presentation approximation never defines length or
    // gradients. First/last samples are the exact tangent contacts.
    std::array<JPH::RVec3, 33> surface_points{};
    std::uint32_t surface_point_count = 0;
    double cross_section_length_m = 0;
    double boundary_length_m = 0;
};
// Allocation-free. Clear means the actual 3D shortcut misses rounded solid.
// Wrapped means an exact straight-extrusion geodesic with every contact inside
// +/- (half_extent[axis]-radius). Unsupported includes endcaps, endpoints inside
// the projected obstacle, and blocked positive radii <= 1e-8m. At an endpoint
// coincident with a sharp vertex, gradients select the retained boundary's
// one-sided tangent; a two-sided derivative does not exist at that kink.
// Never use samples as cable segments for collision/force; chordal rendering
// can lie inside arcs or across polygon corners.
RoundedBoxLineResult rounded_box_line_path(const RoundedBoxLineQuery &query);
} // namespace scraperx::sim
