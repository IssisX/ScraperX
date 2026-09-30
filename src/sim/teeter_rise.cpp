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
        // The carriage rail is visible on one side; the other side remains
        // wide enough for the player to walk beside and push the carriage.
        // Per-part masses make the compound COM and inertia match the model.
        box({3.40F, 0.14F, 0.90F}, {1.60F, 0, 0}, Material::Yellow, 150.0F),
        box({2.23F, 0.07F, 0.05F}, {2.40F, 0.24F, -0.75F}, Material::Steel, 5.0F),
        box({2.23F, 0.07F, 0.05F}, {2.40F, 0.24F, -0.15F}, Material::Steel, 5.0F),
        box({0.40F, 0.46F, 0.35F}, {-1.40F, -0.60F, 0}, Material::Rust, 540.0F),
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
    const Quat initial_rotation = Quat::sRotation(Vec3::sAxisZ(), 0.10F);
    const auto beam = kit.add_body(2800, beam_parts(), kPivot,
        initial_rotation, 700.0F, 0.9F);
    // Losses belong to the visible bearing and stop contacts. Jolt's default
    // velocity damping would add an undeclared sink to the compiled assembly.
    kit.set_damping(beam, 0.0F, 0.0F);
    kit.set_continuous_collision(beam);
    (void)kit.add_hinge(frame, beam, kPivot, Vec3::sAxisZ(), 90.0F);

    // A 65 kg visible steel carriage starts near the bearing. The player
    // moves it along the beam; its gravity then reaches the hinge through a
    // real two-body rail constraint. Dry rail friction holds the weight on
    // the raised, unloaded beam; gravity drives it toward the outboard stop
    // once the beam falls. End limits retain it on failure.
    const RVec3 carriage_position = kPivot + RVec3(initial_rotation * Vec3(0.50F, 0.69F, -0.45F));
    const auto carriage = kit.add_body(2801,
        {box({0.33F, 0.55F, 0.22F}, Vec3::sZero(), Material::Rust, 65.0F)},
        carriage_position, initial_rotation, 65.0F, 0.75F);
    kit.set_damping(carriage, 0.0F, 0.0F);
    kit.set_continuous_collision(carriage);
    kit.add_sliding_track(beam, carriage, initial_rotation * Vec3::sAxisX(),
                          0.0F, 3.80F, 100.0F);
}

} // namespace scraperx::sim
