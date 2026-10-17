// Loose stock on the 154 m deck (see deck_stock.hpp).
#include "sim/deck_stock.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

constexpr float kCrateHalfX = 0.48F;
constexpr float kCrateHalfY = 0.44F;
constexpr float kCrateHalfZ = 0.48F;

// A timber crate: the box, its boards' battens proud of each side, dark
// steel straps round its corners.
std::vector<Part> crate(const float mass_kg) {
    std::vector<Part> parts;
    parts.push_back({Vec3(kCrateHalfX, kCrateHalfY, kCrateHalfZ), Vec3::sZero(), Quat::sIdentity(), Material::Timber,
                     Part::Shape::Box, mass_kg * 0.8F});
    for (const float y : {-0.28F, 0.0F, 0.28F}) {
        parts.push_back({Vec3(kCrateHalfX + 0.012F, 0.05F, kCrateHalfZ + 0.012F), Vec3(0.0F, y, 0.0F),
                         Quat::sIdentity(), Material::Timber, Part::Shape::Box, mass_kg * 0.05F});
    }
    for (const float x : {-1.0F, 1.0F}) {
        for (const float z : {-1.0F, 1.0F}) {
            parts.push_back({Vec3(0.03F, kCrateHalfY + 0.005F, 0.03F),
                             Vec3(x * (kCrateHalfX - 0.015F), 0.0F, z * (kCrateHalfZ - 0.015F)), Quat::sIdentity(),
                             Material::Rust, Part::Shape::Box, mass_kg * 0.0125F});
        }
    }
    return parts;
}

// A steel box of ingots on a low dolly with four small wheels (the wheels
// are drawn; the dolly slides on low friction, as rolling does).
std::vector<Part> ingot_dolly() {
    std::vector<Part> parts;
    parts.push_back({Vec3(0.66F, 0.05F, 0.66F), Vec3(0.0F, -0.32F, 0.0F), Quat::sIdentity(), Material::Galvanised,
                     Part::Shape::Box, 40.0F});
    const Quat axle = Quat::sRotation(Vec3::sAxisZ(), 0.5F * JPH_PI);
    for (const float x : {-0.5F, 0.5F}) {
        for (const float z : {-0.5F, 0.5F}) {
            parts.push_back({Vec3(0.1F, 0.04F, 0.0F), Vec3(x, -0.4F, z), axle, Material::Steel, Part::Shape::Cylinder,
                             5.0F});
        }
    }
    parts.push_back({Vec3(0.56F, 0.3F, 0.56F), Vec3(0.0F, 0.03F, 0.0F), Quat::sIdentity(), Material::Rust,
                     Part::Shape::Box, 340.0F});
    parts.push_back({Vec3(0.58F, 0.03F, 0.58F), Vec3(0.0F, 0.34F, 0.0F), Quat::sIdentity(), Material::Steel,
                     Part::Shape::Box, 20.0F});
    return parts;
}

std::vector<Part> beam() {
    return {{Vec3(2.0F, 0.13F, 0.13F), Vec3::sZero(), Quat::sIdentity(), Material::Timber, Part::Shape::Box, 70.0F}};
}

float total(const std::vector<Part> &parts) {
    float mass = 0.0F;
    for (const Part &part : parts) {
        mass += part.mass_kg;
    }
    return mass;
}
} // namespace

DeckStock::DeckStock(PhysicsSystem &physics, kit::Kit &kit) : physics_(physics), kit_(kit) {
    const std::vector<Part> one = crate(kCrateMassKg);
    const float crate_mass = total(one);
    const float gap = 0.004F;
    // By the south band's outer rail, east, clear of its walking line and
    // far from her locker: two side by side, and one on the eastern one,
    // like a step.
    const float floor_y = kDeckTop + kCrateHalfY + gap;
    pieces_[0] = kit_.add_body(kFirstEntity + 0, one, RVec3(14.4F, floor_y, -174.6F), Quat::sIdentity(), crate_mass,
                               0.9F);
    pieces_[1] = kit_.add_body(kFirstEntity + 1, one, RVec3(15.4F, floor_y, -174.6F),
                               Quat::sRotation(Vec3::sAxisY(), 0.03F), crate_mass, 0.9F);
    pieces_[2] = kit_.add_body(kFirstEntity + 2, one, RVec3(15.45F, floor_y + 2.0F * kCrateHalfY + gap, -174.6F),
                               Quat::sRotation(Vec3::sAxisY(), -0.05F), crate_mass, 0.9F);
    // Two loose crates on the east band, by its outer rail, either side of
    // the catwalk's end.
    pieces_[3] = kit_.add_body(kFirstEntity + 3, one, RVec3(24.6F, kDeckTop + kCrateHalfY + gap, -145.5F),
                               Quat::sRotation(Vec3::sAxisY(), 0.3F), crate_mass, 0.9F);
    pieces_[4] = kit_.add_body(kFirstEntity + 4, one, RVec3(24.5F, kDeckTop + kCrateHalfY + gap, -155.0F),
                               Quat::sRotation(Vec3::sAxisY(), -0.2F), crate_mass, 0.9F);
    // The ingot box on its dolly, against the shaft's rail just north of the
    // catwalk's east end: shoved south a few metres and then west, it goes
    // out onto the plates.
    const std::vector<Part> dolly = ingot_dolly();
    pieces_[5] = kit_.add_body(kFirstEntity + 5, dolly, RVec3(19.4F, kDeckTop + 0.44F + gap, -147.4F),
                               Quat::sIdentity(), total(dolly), 0.06F);
    // The beam on the south band, along its outer rail.
    const std::vector<Part> timber = beam();
    pieces_[6] = kit_.add_body(kFirstEntity + 6, timber, RVec3(2.0F, kDeckTop + 0.13F + gap, -174.4F),
                               Quat::sIdentity(), total(timber), 0.7F);
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (int i = 0; i < kPieces; ++i) {
        last_velocity_[static_cast<std::size_t>(i)] =
            bodies.GetLinearVelocity(kit_.body_id(pieces_[static_cast<std::size_t>(i)]));
    }
}

void DeckStock::resync() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (int i = 0; i < kPieces; ++i) {
        last_velocity_[static_cast<std::size_t>(i)] =
            bodies.GetLinearVelocity(kit_.body_id(pieces_[static_cast<std::size_t>(i)]));
    }
    noise_ = {};
}

void DeckStock::post_step() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (int i = 0; i < kPieces; ++i) {
        const BodyID id = kit_.body_id(pieces_[static_cast<std::size_t>(i)]);
        const Vec3 velocity = bodies.GetLinearVelocity(id);
        const float change = (velocity - last_velocity_[static_cast<std::size_t>(i)]).Length();
        last_velocity_[static_cast<std::size_t>(i)] = velocity;
        if (change < kImpactMps) {
            continue;
        }
        const float loudness = std::clamp((change - kQuietMps) * kNoisePerMps, 0.0F, 1.0F);
        if (!noise_.heard || loudness > noise_.loudness) {
            const RVec3 at = bodies.GetPosition(id);
            noise_ = {true, static_cast<float>(at.GetX()), static_cast<float>(at.GetY()), static_cast<float>(at.GetZ()),
                      loudness};
        }
    }
}

} // namespace scraperx::sim
