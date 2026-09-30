#include "sim/braced_bay.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return {0.5F * (high - low), 0.5F * (high + low),
            JPH::Quat::sIdentity(), material};
}

// The endpoints specify the walking top, not the beam's centreline.
// Both endpoints run north-to-south so local +Y stays the upward face.
Part girder(const JPH::Vec3 north, const JPH::Vec3 south) {
    const JPH::Vec3 along = south - north;
    const JPH::Quat rotation = JPH::Quat::sRotation(
        JPH::Vec3::sAxisX(), -std::atan2(along.GetY(), along.GetZ()));
    return {{0.23F, 0.25F, 0.5F * along.Length()},
            0.5F * (north + south) - rotation * JPH::Vec3(0, 0.25F, 0),
            rotation, Material::Rust};
}
} // namespace

void build_braced_bay(kit::Kit &kit) {
    std::vector<Part> parts;
    // Real recovery deck. The gap between these slabs is already occupied
    // by AS-020's catwalk, whose collider and entity remain authoritative.
    parts.push_back(span({26.05F, 76.72F, -145.0F},
                         {35.20F, 77.0F, -141.67F}, Material::Galvanised));
    parts.push_back(span({26.05F, 76.72F, -140.13F},
                         {34.10F, 77.0F, -130.0F}, Material::Galvanised));
    // Keep the existing teeter grip's complete capsule top-out sweep open.
    parts.push_back(span({34.10F, 76.72F, -139.0F},
                         {35.20F, 77.0F, -130.0F}, Material::Galvanised));
    // Floor members overlap the tower edge below the walking surface.
    for (const float z : {-144.6F, -132.0F}) {
        parts.push_back(span({25.30F, 76.15F, z - 0.25F},
                             {35.20F, 76.72F, z + 0.25F}, Material::Steel));
    }
    parts.push_back(span({26.92F, 76.2F, -144.6F},
                         {27.28F, 76.72F, -130.2F}, Material::Steel));
    parts.push_back(span({34.72F, 76.2F, -144.6F},
                         {35.08F, 76.72F, -140.6F}, Material::Steel));
    parts.push_back(span({34.72F, 76.2F, -138.8F},
                         {35.08F, 76.72F, -130.2F}, Material::Steel));

    parts.push_back(girder({28.0F, 82.0F, -143.0F}, {28.0F, 77.0F, -132.0F}));
    // Keep the front edge within a 0.35 m step of the inclined capsule's
    // soles; extending the landing down-slope would create a tall wall.
    parts.push_back(span({27.1F, 81.72F, -145.25F},
                         {30.6F, 82.0F, -142.75F}, Material::Steel));
    parts.push_back(span({32.8F, 81.72F, -145.25F},
                         {34.9F, 82.0F, -142.75F}, Material::Steel));
    for (const float x : {27.35F, 30.35F, 33.05F, 34.65F}) {
        parts.push_back(span({x - 0.18F, 77.0F, -144.2F},
                             {x + 0.18F, 81.72F, -143.84F}, Material::Rust));
    }

    parts.push_back(girder({34.0F, 82.0F, -143.0F}, {34.0F, 87.0F, -132.0F}));
    // A low, load-bearing crossmember. Its 1.386 m minimum clearance
    // admits the 1.2 m crouched capsule and obstructs a standing body.
    for (const float x : {33.2F, 34.8F}) {
        parts.push_back(span({x - 0.175F, 77.0F, -138.6F},
                             {x + 0.175F, 85.75F, -137.8F}, Material::Steel));
    }
    parts.push_back(span({33.025F, 85.75F, -138.6F},
                         {34.975F, 86.10F, -137.8F}, Material::Steel));

    // The top girder has a broad outer junction to turn on; its narrow
    // return ends at the +88 m slab's real edge, one mantle higher.
    parts.push_back(span({33.2F, 86.50F, -132.20F},
                         {34.9F, 87.0F, -130.60F}, Material::Steel));
    parts.push_back(span({25.75F, 86.50F, -132.23F},
                         {34.9F, 87.0F, -131.77F}, Material::Rust));
    for (const float x : {27.1F, 34.65F}) {
        const float z = x < 30.0F ? -132.0F : -131.5F;
        parts.push_back(span({x - 0.18F, 77.0F, z - 0.18F},
                             {x + 0.18F, 86.50F, z + 0.18F}, Material::Steel));
    }
    (void)kit.add_body(1900, parts, JPH::RVec3::sZero(),
                       JPH::Quat::sIdentity(), 0.0F, 0.85F);
}
} // namespace scraperx::sim
