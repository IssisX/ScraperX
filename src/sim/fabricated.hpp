#pragma once

// Live fabrication (03_EXECUTION/LIVE_FABRICATION/DESIGN.md): an object made
// outside the simulation and handed in at the boundary as data, like a touch.
// This is the hull file's reader and the body's shape, nothing about the
// world: the physics world builds the body (physics_world_fabricated.cpp).
//
// The hull file is written by tools/fab (hullfile.py documents the same
// layout): a 104-byte header, then each hull's point count and points.
//
//     offset  size  field
//          0     8  magic "SXFABHUL"
//          8     4  u32 version (1)
//         12     4  u32 material (Material)
//         16     4  u32 hull count, 1 .. kMaxHulls
//         20     4  f32 mass, kg (the material's density times the hulls' volume)
//         24     4  f32 the hulls' volume, m^3
//         28    12  f32[3] half extents, m: the object's box about its origin
//         40    64  SHA-512 of the GLB the presentation draws it with
//        104     .  each hull: u32 point count (kMinHullPoints ..
//                   kMaxHullPoints), then that many f32[3] points, m
//
// It is untrusted input. Its signature is checked by the game layer that
// receives it (src/bridge/fabricated_delivery.hpp); here every field is
// bounded before it is used, and a file that fails any bound is refused,
// never repaired: its size, the counts, every number finite, the box within
// the dispatch carrier and every point within the box, each hull a real
// solid, and the mass within what the hands carry and equal (within
// kMassTolerance) to the material's density times the hulls' volume as the
// physics measures it.

#include "sim/simulation.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace scraperx::sim::fabricated {

// Entity ids: a range of their own, above every other in the world (the
// corpses' wide window in physics_world_support.hpp, 3300 to 3300 + 0x10000,
// included), one per object in the order they were made.
constexpr std::uint64_t kEntityBase = 0x20000;
constexpr std::uint32_t kMaxObjects = 32;
constexpr std::uint64_t kEntityLimit = kEntityBase + kMaxObjects;

constexpr std::size_t kHeaderBytes = 104;
constexpr std::uint32_t kMaxHulls = 8;
constexpr std::uint32_t kMinHullPoints = 4;
constexpr std::uint32_t kMaxHullPoints = 32;
constexpr std::size_t kMaxFileBytes = kHeaderBytes + kMaxHulls * (4 + kMaxHullPoints * 12);
constexpr std::size_t kMinFileBytes = kHeaderBytes + 4 + kMinHullPoints * 12;
constexpr std::uint32_t kVersion = 1;

// The dispatch carrier (DESIGN.md: about 1.2 m): no side of the object's box
// longer; and the least an object may be on any side.
constexpr float kCarrierBoundM = 1.2F;
constexpr float kMinExtentM = 0.02F;
// What the hands carry: the carry's grip holds about 70 kg (loose_stock.hpp).
constexpr float kMinMassKg = 0.2F;
constexpr float kMaxMassKg = 70.0F;
// The file's mass and volume against the physics' own measure of its hulls.
constexpr float kMassTolerance = 0.03F;
// A hull smaller than this (0.1 cm^3) is no solid.
constexpr float kMinHullVolumeM3 = 1.0e-7F;
// Points may lie this far outside the declared box (the f32 rounding of both).
constexpr float kBoxSlackM = 1.0e-4F;

enum class Material : std::uint8_t {
    Steel = 0,
    Timber = 1,
    Rubber = 2,
    Concrete = 3,
};
constexpr std::uint32_t kMaterials = 4;

// What each material is, physically. Density as tools/fab/hullfile.py has it.
struct MaterialSpec final {
    float density_kg_m3;
    float friction;
    float restitution;
    ImpactSound sound;
};
[[nodiscard]] const MaterialSpec &material_spec(Material material) noexcept;

// The file as read: every field already bounded.
struct Spec final {
    Material material = Material::Steel;
    float mass_kg = 0.0F;
    float volume_m3 = 0.0F;
    std::array<float, 3> half_extent{};
    std::array<std::uint8_t, 64> glb_sha512{};
    std::vector<std::vector<JPH::Vec3>> hulls;
};

// Reads and bounds a hull file: None and `out` filled, or why it is refused.
[[nodiscard]] FabricatedRefusal parse(const std::uint8_t *data, std::size_t size, Spec &out);

// The body's shape from a parsed file: each hull a convex hull of its
// points at the material's density (one hull alone, or a compound of them),
// and the mass the physics gives it. None, or why it is refused (a hull no
// solid, the mass out of bounds or not the file's).
[[nodiscard]] FabricatedRefusal build_shape(const Spec &spec, JPH::RefConst<JPH::Shape> &shape, float &mass_kg);

// A short name for a refusal, for logs and the bridge.
[[nodiscard]] const char *refusal_name(FabricatedRefusal refusal) noexcept;

} // namespace scraperx::sim::fabricated
