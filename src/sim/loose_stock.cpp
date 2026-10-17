// Loose stock about the tower (see loose_stock.hpp).
#include "sim/loose_stock.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>
#include <array>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;
using K = LooseStock::Kind;

struct Spec {
    std::vector<Part> parts;
    float mass_kg = 0.0F;
    float friction = 0.6F;
    float half_height = 0.0F;   // from its origin to its underside
    bool carried = false;
    Vec3 handle = Vec3::sZero();
};

// A cylinder lying along x (a cylinder part runs along its local y).
const Quat kAlongX = Quat::sRotation(Vec3::sAxisZ(), 0.5F * JPH_PI);

Part box(const Vec3 half, const Vec3 offset, const Material material, const float mass_kg) {
    return {half, offset, Quat::sIdentity(), material, Part::Shape::Box, mass_kg};
}

Part cylinder(const float radius, const float half_length, const Vec3 offset, const Quat rotation,
              const Material material, const float mass_kg) {
    return {Vec3(radius, half_length, 0.0F), offset, rotation, material, Part::Shape::Cylinder, mass_kg};
}

Spec make_spec(const K kind) {
    Spec s;
    switch (kind) {
    case K::Crate: {
        // The box, its boards' battens proud of each side, dark steel straps
        // down its corners.
        constexpr float kMass = 160.0F;
        const Vec3 h(0.48F, 0.44F, 0.48F);
        s.parts.push_back(box(h, Vec3::sZero(), Material::Timber, kMass * 0.8F));
        for (const float y : {-0.28F, 0.0F, 0.28F}) {
            s.parts.push_back(box(Vec3(h.GetX() + 0.012F, 0.05F, h.GetZ() + 0.012F), Vec3(0.0F, y, 0.0F),
                                  Material::Timber, kMass * 0.05F));
        }
        for (const float x : {-1.0F, 1.0F}) {
            for (const float z : {-1.0F, 1.0F}) {
                s.parts.push_back(box(Vec3(0.03F, h.GetY() + 0.005F, 0.03F),
                                      Vec3(x * (h.GetX() - 0.015F), 0.0F, z * (h.GetZ() - 0.015F)), Material::Rust,
                                      kMass * 0.0125F));
            }
        }
        s.half_height = h.GetY();
        s.friction = 0.9F;
        break;
    }
    case K::IngotDolly:
        // A steel box of ingots on a low dolly with four small wheels (the
        // dolly slides on low friction, as rolling does).
        s.parts.push_back(box(Vec3(0.66F, 0.05F, 0.66F), Vec3(0.0F, -0.32F, 0.0F), Material::Galvanised, 40.0F));
        for (const float x : {-0.5F, 0.5F}) {
            for (const float z : {-0.5F, 0.5F}) {
                s.parts.push_back(cylinder(0.1F, 0.04F, Vec3(x, -0.4F, z), kAlongX, Material::Steel, 5.0F));
            }
        }
        s.parts.push_back(box(Vec3(0.56F, 0.3F, 0.56F), Vec3(0.0F, 0.03F, 0.0F), Material::Rust, 340.0F));
        s.parts.push_back(box(Vec3(0.58F, 0.03F, 0.58F), Vec3(0.0F, 0.34F, 0.0F), Material::Steel, 20.0F));
        s.half_height = 0.5F;
        s.friction = 0.06F;
        break;
    case K::TimberBeam:
        s.parts = {box(Vec3(2.0F, 0.13F, 0.13F), Vec3::sZero(), Material::Timber, 70.0F)};
        s.half_height = 0.13F;
        s.friction = 0.7F;
        break;
    case K::Drum:
        // Rolled steel, a rolling hoop round its middle; lifted by its rim.
        s.parts = {cylinder(0.27F, 0.44F, Vec3::sZero(), Quat::sIdentity(), Material::Steel, 49.0F),
                   cylinder(0.28F, 0.015F, Vec3(0.0F, 0.15F, 0.0F), Quat::sIdentity(), Material::Steel, 3.0F),
                   cylinder(0.28F, 0.015F, Vec3(0.0F, -0.15F, 0.0F), Quat::sIdentity(), Material::Steel, 3.0F)};
        s.half_height = 0.44F;
        s.friction = 0.6F;
        s.carried = true;
        s.handle = Vec3(0.0F, 0.44F, 0.0F);
        break;
    case K::Pallet:
        // Three stringers under five deck boards.
        for (const float x : {-0.55F, 0.0F, 0.55F}) {
            s.parts.push_back(box(Vec3(0.05F, 0.05F, 0.5F), Vec3(x, -0.02F, 0.0F), Material::Timber, 4.0F));
        }
        for (int k = 0; k < 5; ++k) {
            s.parts.push_back(box(Vec3(0.6F, 0.01F, 0.06F), Vec3(0.0F, 0.06F, -0.44F + 0.22F * static_cast<float>(k)),
                                  Material::Timber, 2.0F));
        }
        s.half_height = 0.07F;
        s.friction = 0.8F;
        s.carried = true;
        s.handle = Vec3(0.0F, 0.07F, 0.0F);
        break;
    case K::Container:
        // Corrugated steel, its doors at +x. A machine's load, not a man's.
        s.parts = {box(Vec3(6.095F, 1.295F, 1.22F), Vec3::sZero(), Material::Rust, 3750.0F)};
        s.half_height = 1.295F;
        s.friction = 0.6F;
        break;
    case K::SteelBeam:
        s.parts = {box(Vec3(5.0F, 0.15F, 0.15F), Vec3::sZero(), Material::Steel, 600.0F)};
        s.half_height = 0.15F;
        s.friction = 0.5F;
        break;
    case K::Sleeper:
        s.parts = {box(Vec3(0.125F, 0.075F, 1.5F), Vec3::sZero(), Material::Timber, 90.0F)};
        s.half_height = 0.075F;
        s.friction = 0.7F;
        break;
    case K::PipeBearer:
        // A steel cradle under a pipe rack's three pipes (0.62 m apart, 0.3 m
        // round): a post at each end, as tall as the pipes' middles, stands
        // 5 mm off each outer pipe, so a pipe nudged sideways does not roll
        // off. (Pipes stacked in a pyramid on it sank into one another in the
        // solver, so each rack holds one tier.)
        s.parts = {box(Vec3(0.1F, 0.1F, 1.2F), Vec3::sZero(), Material::Steel, 200.0F),
                   box(Vec3(0.06F, 0.275F, 0.04F), Vec3(0.0F, 0.375F, -0.965F), Material::Steel, 25.0F),
                   box(Vec3(0.06F, 0.275F, 0.04F), Vec3(0.0F, 0.375F, 0.965F), Material::Steel, 25.0F)};
        s.half_height = 0.1F;
        s.friction = 0.7F;
        break;
    case K::PlateStack:
        s.parts = {box(Vec3(1.5F, 0.4F, 0.75F), Vec3::sZero(), Material::Steel, 8000.0F)};
        s.half_height = 0.4F;
        s.friction = 0.5F;
        break;
    case K::PlateSlab:
        s.parts = {box(Vec3(1.2F, 0.15F, 0.6F), Vec3::sZero(), Material::Rust, 2000.0F)};
        s.half_height = 0.15F;
        s.friction = 0.5F;
        break;
    case K::Pipe:
        s.parts = {cylinder(0.3F, 4.0F, Vec3::sZero(), kAlongX, Material::Steel, 700.0F)};
        s.half_height = 0.3F;
        s.friction = 0.5F;
        break;
    case K::BilletStillage:
    case K::CoilSkid: {
        // On two runners 0.55 m either side of its middle, its deck 12 cm up:
        // a forklift's 5 cm forks slide in between the runners, under the
        // deck, from either end. Its origin on the ground.
        for (const float x : {-0.55F, 0.55F}) {
            s.parts.push_back(box(Vec3(0.06F, 0.06F, 0.62F), Vec3(x, 0.06F, 0.0F), Material::Timber, 20.0F));
        }
        s.parts.push_back(box(Vec3(0.66F, 0.02F, 0.62F), Vec3(0.0F, 0.14F, 0.0F), Material::Steel, 60.0F));
        if (kind == K::BilletStillage) {
            // Steel billets bundled 0.62 m deep in a frame of corner posts:
            // 0.75 m3 of steel at 85 % packed, 4.9 t.
            for (const float x : {-0.62F, 0.62F}) {
                for (const float z : {-0.58F, 0.58F}) {
                    s.parts.push_back(box(Vec3(0.03F, 0.33F, 0.03F), Vec3(x, 0.49F, z), Material::Steel, 5.0F));
                }
            }
            s.parts.push_back(box(Vec3(0.55F, 0.31F, 0.55F), Vec3(0.0F, 0.47F, 0.0F), Material::Rust, 4900.0F));
        } else {
            // The coil on its side, eye along x, in two chocks.
            s.parts.push_back(cylinder(0.55F, 0.35F, Vec3(0.0F, 0.71F, 0.0F), kAlongX, Material::Galvanised, 2700.0F));
            for (const float z : {-0.5F, 0.5F}) {
                s.parts.push_back(box(Vec3(0.4F, 0.07F, 0.08F), Vec3(0.0F, 0.23F, z), Material::Timber, 10.0F));
            }
        }
        s.half_height = 0.0F;
        s.friction = 0.6F;
        break;
    }
    }
    for (const Part &part : s.parts) {
        s.mass_kg += part.mass_kg;
    }
    return s;
}

constexpr float kDeck154 = 154.0F;

void add_stock_yard(std::vector<LooseStock::Placement> &t) {
    // South of the start, clear of the ground's fixtures (x -20..20, z -12..25
    // is kept empty) and of the yard crew's routes (north of z -12).
    // Containers: [x, z, tier, along x, paint].
    struct Box {
        float x, z;
        int tier;
        bool along_x;
        std::uint8_t paint;
    };
    std::vector<Box> containers{{-42.0F, 8.0F, 0, true, 0}, {-42.0F, 10.6F, 0, true, 1}, {-42.0F, 8.0F, 1, true, 2},
                                {-40.0F, 20.0F, 0, true, 3}, {-40.0F, 20.0F, 1, true, 4},
                                {-54.5F, 14.3F, 0, false, 1}};
    for (int i = 0; i < 7; ++i) {
        const float x = -36.0F + 12.5F * static_cast<float>(i);
        containers.push_back({x, 70.0F, 0, true, static_cast<std::uint8_t>((i * 3 + 1) % 5)});
        if (i == 1 || i == 4 || i == 5) {
            containers.push_back({x, 70.0F, 1, true, static_cast<std::uint8_t>((i * 3 + 3) % 5)});
        }
    }
    constexpr float kContainerHeight = 2.59F;
    for (const Box &c : containers) {
        t.push_back({K::Container, c.paint, c.x, kContainerHeight * static_cast<float>(c.tier), c.z,
                     c.along_x ? 0.0F : 0.5F * JPH_PI});
    }
    // Steel beams on sleepers, three layers (paint 0 mill scale, 1 rust).
    for (int layer = 0; layer < 3; ++layer) {
        const float base = 0.45F * static_cast<float>(layer);
        for (const float x : {32.0F, 36.0F, 40.0F}) {
            t.push_back({K::Sleeper, 0, x, base, 11.9F, 0.0F});
        }
        for (int j = 0; j < 6; ++j) {
            t.push_back({K::SteelBeam, static_cast<std::uint8_t>((j + layer) % 3 != 0 ? 0 : 1), 36.0F, base + 0.15F,
                         10.5F + 0.55F * static_cast<float>(j), 0.0F});
        }
    }
    // Plate, stacked.
    t.push_back({K::PlateStack, 0, 47.0F, 0.0F, 12.0F, 0.0F});
    t.push_back({K::PlateSlab, 0, 47.3F, 0.8F, 12.1F, 0.0F});
    // Drums on pallets, four to a pallet, 2 cm apart (paint 0 blue, 1 ochre,
    // 2 oxide red).
    const std::array<std::array<float, 2>, 3> pallets{{{38.0F, 24.0F}, {39.4F, 24.0F}, {38.0F, 25.3F}}};
    for (std::size_t p = 0; p < pallets.size(); ++p) {
        t.push_back({K::Pallet, 0, pallets[p][0], 0.0F, pallets[p][1], 0.0F});
        for (int d = 0; d < 4; ++d) {
            t.push_back({K::Drum, static_cast<std::uint8_t>((static_cast<int>(p) + d) % 3),
                         pallets[p][0] + (d % 2 == 0 ? -0.3F : 0.3F), 0.14F, pallets[p][1] + (d < 2 ? -0.29F : 0.29F),
                         0.0F});
        }
    }
    // Heavy steel for the forklift (forklift.hpp) west of the drums, their
    // runners along z: the stillage of billets, too heavy for it (its lift
    // stalls and its back end rises), and the coil, a hard lift.
    t.push_back({K::BilletStillage, 0, 34.0F, 0.0F, 24.0F, 0.0F});
    t.push_back({K::CoilSkid, 0, 30.0F, 0.0F, 24.0F, 0.0F});
    // Two pipe racks, three pipes side by side on three cradles each.
    for (const float rack_z : {25.0F, 28.0F}) {
        for (const float x : {47.0F, 50.0F, 53.0F}) {
            t.push_back({K::PipeBearer, 0, x, 0.0F, rack_z, 0.0F});
        }
        for (const float dz : {-0.62F, 0.0F, 0.62F}) {
            t.push_back({K::Pipe, 0, 50.0F, 0.2F, rack_z + dz, 0.0F});
        }
    }
}

} // namespace

const std::vector<LooseStock::Placement> &LooseStock::placements() {
    // The Matron's deck first (her crash hearing and the ingot dolly's plate
    // are tested against these by index), then the stock yard.
    static const std::vector<Placement> table = [] {
        std::vector<Placement> t{
            // By the south band's outer rail, east, far from her locker: two
            // crates side by side and one on the eastern one, like a step.
            {K::Crate, 0, 14.4F, kDeck154, -174.6F, 0.0F},
            {K::Crate, 0, 15.4F, kDeck154, -174.6F, 0.03F},
            {K::Crate, 0, 15.45F, kDeck154 + 0.884F, -174.6F, -0.05F},
            // Either side of the catwalk's east end, by the east band's rail.
            {K::Crate, 0, 24.6F, kDeck154, -145.5F, 0.3F},
            {K::Crate, 0, 24.5F, kDeck154, -155.0F, -0.2F},
            // The ingot dolly against the shaft's rail just north of the
            // catwalk's east end: shoved south into line and then west, out
            // onto the plates.
            {K::IngotDolly, 0, 19.4F, kDeck154, -147.4F, 0.0F},
            // A timber beam along the south band's outer rail.
            {K::TimberBeam, 0, 2.0F, kDeck154, -174.4F, 0.0F},
        };
        add_stock_yard(t);
        return t;
    }();
    return table;
}

LooseStock::LooseStock(PhysicsSystem &physics, kit::Kit &kit) : physics_(physics), kit_(kit) {
    std::array<Spec, kKinds> specs;
    for (int k = 0; k < kKinds; ++k) {
        specs[static_cast<std::size_t>(k)] = make_spec(static_cast<Kind>(k));
    }
    const std::vector<Placement> &table = placements();
    pieces_.reserve(table.size());
    last_velocity_.reserve(table.size());
    const BodyInterface &bodies = physics_.GetBodyInterface();
    for (std::size_t i = 0; i < table.size(); ++i) {
        const Placement &p = table[i];
        const Spec &spec = specs[static_cast<std::size_t>(p.kind)];
        // 4 mm clear of what it lies on, so nothing starts in contact.
        const kit::BodyIndex body =
            kit_.add_body(kFirstEntity + i, spec.parts, RVec3(p.x, p.y + spec.half_height + 0.004F, p.z),
                          Quat::sRotation(Vec3::sAxisY(), p.yaw), spec.mass_kg, spec.friction);
        if (spec.carried) {
            kit_.set_carry(body, kit::CarryKind::Load, spec.handle);
        }
        kit_.allow_sleeping(body);
        pieces_.push_back(body);
        last_velocity_.push_back(bodies.GetLinearVelocity(kit_.body_id(body)));
    }
}

void LooseStock::resync() {
    const BodyInterface &bodies = physics_.GetBodyInterface();
    for (std::size_t i = 0; i < pieces_.size(); ++i) {
        last_velocity_[i] = bodies.GetLinearVelocity(kit_.body_id(pieces_[i]));
    }
    crash_ = {};
}

void LooseStock::post_step() {
    const BodyInterface &bodies = physics_.GetBodyInterface();
    for (std::size_t i = 0; i < pieces_.size(); ++i) {
        const BodyID id = kit_.body_id(pieces_[i]);
        if (!bodies.IsActive(id)) {
            // Asleep: it is at rest.
            last_velocity_[i] = Vec3::sZero();
            continue;
        }
        const Vec3 velocity = bodies.GetLinearVelocity(id);
        const float change = (velocity - last_velocity_[i]).Length();
        last_velocity_[i] = velocity;
        if (change < kImpactMps) {
            continue;
        }
        const float loudness = std::clamp((change - kQuietMps) * kNoisePerMps, 0.0F, 1.0F);
        if (!crash_.heard || loudness > crash_.loudness) {
            const RVec3 at = bodies.GetPosition(id);
            crash_ = {true, static_cast<float>(at.GetX()), static_cast<float>(at.GetY()), static_cast<float>(at.GetZ()),
                      loudness};
        }
    }
}

} // namespace scraperx::sim
