#include "sim/teeter_rise.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

const JPH::RVec3 kPivot(28.5, 65.8, -139.0);

Part box(const JPH::Vec3 half, const JPH::Vec3 offset, const Material material,
         const float mass = 0.0F) {
    Part part{half, offset, JPH::Quat::sIdentity(), material};
    part.mass_kg = mass;
    return part;
}

Part span(const JPH::Vec3 lo, const JPH::Vec3 hi, const Material material) {
    return box(0.5F * (hi - lo), 0.5F * (hi + lo), material);
}

std::vector<Part> frame_parts() {
    std::vector<Part> parts;
    // The bearing frame grows out of the +55 m east ring. Its two tall
    // cheeks leave the player-facing side and the beam sweep exposed.
    parts.push_back(span({26.25F, 55.02F, -140.35F},
                         {29.15F, 55.32F, -137.65F}, Material::Steel));
    for (const float z : {-140.20F, -137.80F}) {
        parts.push_back(span({28.32F, 55.32F, z - 0.16F},
                             {28.68F, 65.80F, z + 0.16F}, Material::Rust));
    }
    parts.push_back(span({28.15F, 65.53F, -140.30F},
                         {28.85F, 66.07F, -137.70F}, Material::Steel));
    Part axle = box({0.23F, 1.30F, 0.23F}, {28.50F, 65.80F, -139.0F},
                    Material::Yellow);
    axle.shape = Part::Shape::Cylinder;
    axle.rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5F);
    parts.push_back(axle);

    // Small fixed tongue closes the ring-to-rocker walking gap.
    parts.push_back(span({26.25F, 65.83F, -139.82F},
                         {26.35F, 66.05F, -138.18F}, Material::Galvanised));

    // The far shelf is level with the outboard tip at the lower stop.
    // These beams carry its load into the existing +55 m ring.
    parts.push_back(span({26.20F, 54.70F, -145.00F},
                         {35.60F, 55.00F, -133.00F}, Material::Galvanised));
    for (const float z : {-144.75F, -133.25F}) {
        parts.push_back(span({26.20F, 54.35F, z - 0.18F},
                             {35.60F, 54.70F, z + 0.18F}, Material::Steel));
    }
    parts.push_back(span({33.00F, 63.27F, -140.35F},
                         {35.55F, 63.55F, -137.65F}, Material::Yellow));
    for (const float x : {33.20F, 35.30F}) {
        parts.push_back(span({x - 0.15F, 55.00F, -140.20F},
                             {x + 0.15F, 63.27F, -139.90F}, Material::Rust));
    }

    // A bare vertical service grip, its top lip, and a real receiving
    // catwalk. The catwalk overlaps the pre-existing +77 m ring.
    parts.push_back(span({34.65F, 63.55F, -140.12F},
                         {34.79F, 76.90F, -139.98F}, Material::Galvanised));
    parts.push_back(span({34.20F, 76.86F, -140.12F},
                         {35.20F, 77.00F, -139.98F}, Material::Galvanised));
    parts.push_back(span({25.75F, 76.72F, -141.65F},
                         {35.55F, 77.00F, -140.15F}, Material::Galvanised));
    return parts;
}

std::vector<Part> upper_stop_parts() {
    return {
        span({26.92F, 55.32F, -139.88F},
             {27.28F, 65.30F, -139.54F}, Material::Rust),
        span({26.86F, 65.30F, -139.92F},
             {27.34F, 65.51F, -139.50F}, Material::Timber),
    };
}

std::vector<Part> lower_stop_parts() {
    return {
        span({32.02F, 55.00F, -139.88F},
             {32.38F, 63.44F, -139.54F}, Material::Rust),
        span({31.97F, 63.44F, -139.92F},
             {32.43F, 63.66F, -139.50F}, Material::Timber),
    };
}

std::vector<Part> beam_parts() {
    return {
        // Per-part densities give the compound its real centre of mass
        // and inertia. The visible yellow tread is exactly its collider.
        box({3.40F, 0.14F, 0.90F}, {1.60F, 0, 0}, Material::Yellow, 160.0F),
        box({0.40F, 0.46F, 0.35F}, {-1.40F, -0.60F, 0}, Material::Rust, 369.0F),
    };
}
} // namespace

TeeterRise::TeeterRise(kit::Kit &kit) {
    using namespace JPH;
    const auto frame = kit.add_body(1800, frame_parts(), RVec3::sZero(),
                                    Quat::sIdentity(), 0.0F, 0.8F);
    (void)kit.add_body(1801, upper_stop_parts(), RVec3::sZero(),
                       Quat::sIdentity(), 0.0F, 0.8F);
    (void)kit.add_body(1802, lower_stop_parts(), RVec3::sZero(),
                       Quat::sIdentity(), 0.0F, 0.8F);
    const auto beam = kit.add_body(2800, beam_parts(), kPivot,
        Quat::sRotation(Vec3::sAxisZ(), 0.10F), 529.0F, 0.9F);
    // Losses belong to the visible bearing and stop contacts. Jolt's default
    // velocity damping would add an undeclared sink to the compiled assembly.
    kit.set_damping(beam, 0.0F, 0.0F);
    kit.set_continuous_collision(beam);
    (void)kit.add_hinge(frame, beam, kPivot, Vec3::sAxisZ(), 90.0F);
}

} // namespace scraperx::sim
