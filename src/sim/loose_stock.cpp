// Loose stock about the tower (see loose_stock.hpp).
#include "sim/loose_stock.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <algorithm>
#include <array>
#include <cmath>

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
    case K::GasBottle:
        // A welding gas bottle on end: its shoulder, its valve under a cap,
        // a collar round its neck. Lifted by the cap.
        s.parts = {cylinder(0.115F, 0.55F, Vec3::sZero(), Quat::sIdentity(), Material::Galvanised, 56.0F),
                   cylinder(0.085F, 0.04F, Vec3(0.0F, 0.59F, 0.0F), Quat::sIdentity(), Material::Galvanised, 2.0F),
                   cylinder(0.05F, 0.06F, Vec3(0.0F, 0.69F, 0.0F), Quat::sIdentity(), Material::Steel, 2.0F)};
        s.half_height = 0.55F;
        s.friction = 0.5F;
        s.carried = true;
        s.handle = Vec3(0.0F, 0.75F, 0.0F);
        break;
    case K::CableReel:
        // A wooden reel on its rims, its axle along x: two 1.1 m flanges and
        // the black cable wound between them. It rolls.
        for (const float x : {-0.28F, 0.28F}) {
            s.parts.push_back(cylinder(0.55F, 0.03F, Vec3(x, 0.0F, 0.0F), kAlongX, Material::Timber, 35.0F));
        }
        s.parts.push_back(cylinder(0.36F, 0.25F, Vec3::sZero(), kAlongX, Material::Rubble, 110.0F));
        s.half_height = 0.55F;
        s.friction = 0.7F;
        break;
    case K::Toolbox:
        // A steel box with a lid and a handle over it.
        s.parts = {box(Vec3(0.3F, 0.12F, 0.13F), Vec3::sZero(), Material::Hazard, 16.0F),
                   box(Vec3(0.18F, 0.015F, 0.015F), Vec3(0.0F, 0.17F, 0.0F), Material::Steel, 1.0F),
                   box(Vec3(0.015F, 0.025F, 0.015F), Vec3(-0.17F, 0.145F, 0.0F), Material::Steel, 0.5F),
                   box(Vec3(0.015F, 0.025F, 0.015F), Vec3(0.17F, 0.145F, 0.0F), Material::Steel, 0.5F)};
        s.half_height = 0.12F;
        s.friction = 0.6F;
        s.carried = true;
        s.handle = Vec3(0.0F, 0.17F, 0.0F);
        break;
    case K::PlankBundle:
        // Planks 1.8 m long stacked in a bundle under two steel straps.
        s.parts = {box(Vec3(0.9F, 0.12F, 0.25F), Vec3::sZero(), Material::Timber, 58.0F)};
        for (const float x : {-0.55F, 0.55F}) {
            s.parts.push_back(box(Vec3(0.02F, 0.125F, 0.255F), Vec3(x, 0.0F, 0.0F), Material::Rust, 1.0F));
        }
        s.half_height = 0.12F;
        s.friction = 0.8F;
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
    for (const Placement &placement : placements()) {
        add_piece(placement);
    }
}

void LooseStock::add_piece(const Placement &p) {
    static const std::array<Spec, kKinds> specs = [] {
        std::array<Spec, kKinds> all;
        for (int k = 0; k < kKinds; ++k) {
            all[static_cast<std::size_t>(k)] = make_spec(static_cast<Kind>(k));
        }
        return all;
    }();
    const Spec &spec = specs[static_cast<std::size_t>(p.kind)];
    // 4 mm clear of what it lies on, so nothing starts in contact.
    const kit::BodyIndex body =
        kit_.add_body(kFirstEntity + pieces_.size(), spec.parts, RVec3(p.x, p.y + spec.half_height + 0.004F, p.z),
                      Quat::sRotation(Vec3::sAxisY(), p.yaw), spec.mass_kg, spec.friction);
    if (spec.carried) {
        kit_.set_carry(body, kit::CarryKind::Load, spec.handle);
    }
    kit_.allow_sleeping(body);
    pieces_.push_back(body);
    placed_.push_back(p);
    last_velocity_.push_back(physics_.GetBodyInterface().GetLinearVelocity(kit_.body_id(body)));
}

namespace {

// A spot's pieces in its own frame: `along` the edge it is set against,
// `out` from that edge across the deck, `up` from the floor to the piece's
// underside, and turned `yaw` from lying along the edge.
struct SpotPiece {
    K kind;
    std::uint8_t variant;
    float along, out, up, yaw;
};
struct SpotTemplate {
    float width;   // along the edge
    float depth;   // out from it
    std::vector<SpotPiece> pieces;
};

const std::vector<SpotTemplate> &spot_templates() {
    static const std::vector<SpotTemplate> templates = {
        // A step of crates: two side by side, the right-hand one with a
        // third square on it (turned on it, its battens never settled).
        {2.3F, 1.2F,
         {{K::Crate, 0, -0.52F, 0.6F, 0.0F, 0.02F},
          {K::Crate, 0, 0.5F, 0.6F, 0.0F, -0.03F},
          {K::Crate, 0, 0.5F, 0.6F, 0.884F, -0.03F}}},
        // Welding gas bottles stood against the rail, a toolbox by them.
        {2.0F, 0.9F,
         {{K::GasBottle, 0, -0.75F, 0.25F, 0.0F, 0.0F},
          {K::GasBottle, 1, -0.45F, 0.3F, 0.0F, 0.0F},
          {K::GasBottle, 0, -0.15F, 0.25F, 0.0F, 0.0F},
          {K::Toolbox, 0, 0.55F, 0.45F, 0.0F, 0.3F}}},
        // A reel of cable on its rims, and a bundle of planks.
        {3.4F, 1.3F,
         {{K::CableReel, 0, -1.0F, 0.65F, 0.0F, 0.0F},
          {K::PlankBundle, 0, 0.65F, 0.45F, 0.0F, 0.0F}}},
        // A pallet of three drums, a fourth stood beside it.
        {2.4F, 1.3F,
         {{K::Pallet, 0, -0.3F, 0.65F, 0.0F, 0.0F},
          {K::Drum, 0, -0.6F, 0.4F, 0.14F, 0.0F},
          {K::Drum, 1, 0.0F, 0.4F, 0.14F, 0.0F},
          {K::Drum, 2, -0.3F, 0.9F, 0.14F, 0.0F},
          {K::Drum, 0, 0.8F, 0.5F, 0.0F, 0.0F}}},
    };
    return templates;
}

// Only the bodies a spot must keep clear of: the machines (every kit body)
// and whatever moves. The tower's own frame and dressing are not felt, which
// is also what keeps the test cheap.
class MachineFilter final : public BodyFilter {
public:
    [[nodiscard]] bool ShouldCollideLocked(const Body &body) const override {
        return kit::is_kit_entity(body.GetUserData()) || !body.IsStatic();
    }
};

// Where the spots may be: the stack's deck rings (11 m to 143 m: the 154 m
// deck has its own seven, set for the Matron) and the upper rings (176 m to
// 396 m), each a square ring round the shaft centred on (0, -150).
constexpr float kRingCentreX = 0.0F;
constexpr float kRingCentreZ = -150.0F;
// Every spot this far along its ring from the last, at most this many a ring.
constexpr float kSpotSpacing = 9.0F;
constexpr int kSpotsPerStackRing = 3;
constexpr int kSpotsPerUpperRing = 2;
// Clear of every machine and every other spot by this much, and of every place
// the player is put down by kKeepClear; the walkway left past a spot.
constexpr float kMachineClear = 3.0F;
constexpr float kWalkway = 1.5F;

} // namespace

void LooseStock::dress_tower(const std::vector<JPH::RVec3> &keep_clear, const float keep_clear_radius) {
    const NarrowPhaseQuery &query = physics_.GetNarrowPhaseQuery();
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const BodyLockInterfaceNoLock &locks = physics_.GetBodyLockInterfaceNoLock();
    const MachineFilter machines;
    // The floor at (x, z) near height `top`: a level, static top within 5 cm
    // of it (its height in `at`).
    const auto floor_at = [&](const float x, const float z, const float top, float &at) {
        const RRayCast ray(RVec3(x, top + 0.6F, z), Vec3(0.0F, -1.2F, 0.0F));
        RayCastResult hit;
        if (!query.CastRay(ray, hit) || bodies.GetMotionType(hit.mBodyID) != EMotionType::Static) {
            return false;
        }
        const RVec3 point = ray.GetPointOnRay(hit.mFraction);
        at = static_cast<float>(point.GetY());
        if (std::abs(at - top) > 0.05F) {
            return false;
        }
        const BodyLockRead lock(locks, hit.mBodyID);
        return lock.Succeeded() && lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point).GetY() > 0.98F;
    };
    std::vector<float> tops;
    for (int level = 1; level <= 13; ++level) {
        tops.push_back(11.0F * static_cast<float>(level));
    }
    const std::size_t stack_rings = tops.size();
    for (int k = 0; k <= 10; ++k) {
        tops.push_back(176.25F + 22.0F * static_cast<float>(k));
    }
    // The four sides, each by the direction out from the shaft across it.
    const std::array<Vec3, 4> sides = {Vec3(0.0F, 0.0F, 1.0F), Vec3(1.0F, 0.0F, 0.0F), Vec3(0.0F, 0.0F, -1.0F),
                                       Vec3(-1.0F, 0.0F, 0.0F)};
    int taken = 0;
    for (std::size_t ring = 0; ring < tops.size(); ++ring) {
        const float top = tops[ring];
        const int most = ring < stack_rings ? kSpotsPerStackRing : kSpotsPerUpperRing;
        std::vector<RVec3> spots;
        // Each ring starts on another side and at another place along it, so
        // no two rings are dressed alike.
        for (std::size_t s = 0; s < sides.size(); ++s) {
            const Vec3 out = sides[(s + ring) % sides.size()];
            if (static_cast<int>(spots.size()) >= most) {
                break;
            }
            const Vec3 along(out.GetZ(), 0.0F, -out.GetX());
            // The shaft's edge on this side: the first floor out from the
            // middle of the ring, felt for every 0.5 m and then to 5 cm.
            float edge = -1.0F;
            float ignored = 0.0F;
            const auto floor_out = [&](const float d) {
                return floor_at(kRingCentreX + out.GetX() * d, kRingCentreZ + out.GetZ() * d, top, ignored);
            };
            for (float d = 10.0F; d < 30.0F; d += 0.5F) {
                if (floor_out(d)) {
                    edge = d;
                    for (float back = d - 0.05F; back > d - 0.5F && floor_out(back); back -= 0.05F) {
                        edge = back;
                    }
                    break;
                }
            }
            if (edge < 0.0F) {
                continue;
            }
            const float start = -edge + 2.0F + std::fmod(3.7F * static_cast<float>(ring + s), 7.0F);
            for (float a = start; a <= edge - 2.0F && static_cast<int>(spots.size()) < most; a += 1.5F) {
                const SpotTemplate &spot =
                    spot_templates()[static_cast<std::size_t>(taken + static_cast<int>(ring)) % spot_templates().size()];
                const RVec3 origin(kRingCentreX + out.GetX() * edge + along.GetX() * a, top,
                                   kRingCentreZ + out.GetZ() * edge + along.GetZ() * a);
                const auto at = [&](const float u, const float v) {
                    return origin + along * u + out * v;
                };
                // Spaced along the ring, and clear of where the player is put down.
                bool apart = true;
                for (const RVec3 &other : spots) {
                    apart = apart && Vec3(other - origin).Length() >= kSpotSpacing;
                }
                for (const RVec3 &point : keep_clear) {
                    apart = apart && Vec3(point - origin).Length() >= keep_clear_radius;
                }
                if (!apart) {
                    continue;
                }
                // Set against the rail where there is one (felt for at hand
                // height over the edge); where the edge is open, back from it.
                RayCastResult rail;
                const bool railed =
                    query.CastRay(RRayCast(at(0.0F, 0.4F) + Vec3(0.0F, 1.05F, 0.0F), out * -0.6F), rail);
                const float near = railed ? 0.15F : 0.6F;
                const float far = near + spot.depth + kWalkway;
                const RVec3 middle = at(0.0F, near + 0.5F * spot.depth) + Vec3(0.0F, 1.3F, 0.0F);
                // No machine, nor any body that moves, within kMachineClear of
                // it.
                const RefConst<Shape> around =
                    new BoxShape(Vec3(0.5F * spot.width + kMachineClear, 2.5F, 0.5F * spot.width + kMachineClear));
                AnyHitCollisionCollector<CollideShapeCollector> near_it;
                query.CollideShape(around, Vec3::sReplicate(1.0F), RMat44::sRotationTranslation(Quat::sIdentity(), middle),
                                   CollideShapeSettings(), middle, near_it, {}, {}, machines);
                if (near_it.HadHit()) {
                    continue;
                }
                // A level floor under all of it, and a walkway past it, felt
                // for every metre.
                bool level = true;
                float floor = top;
                for (float u = -0.5F * spot.width; u <= 0.5F * spot.width + 0.5F && level; u += 1.0F) {
                    for (float v = near; v <= far + 0.5F && level; v += 1.0F) {
                        const RVec3 p = at(std::min(u, 0.5F * spot.width), std::min(v, far));
                        level = floor_at(static_cast<float>(p.GetX()), static_cast<float>(p.GetZ()), top, floor);
                    }
                }
                if (!level) {
                    continue;
                }
                // Nothing at all in the way above it, from 2 cm over the floor
                // (a toe plate or a bolted plate lower than a hand's width is
                // in the way too).
                const RefConst<Shape> room = new BoxShape(Vec3(0.5F * spot.width, 1.24F, 0.5F * spot.depth));
                const RVec3 room_middle = middle - Vec3(0.0F, 1.3F - 1.26F, 0.0F);
                AnyHitCollisionCollector<CollideShapeCollector> in_the_way;
                query.CollideShape(room, Vec3::sReplicate(1.0F),
                                   RMat44::sRotationTranslation(Quat::sIdentity(), room_middle), CollideShapeSettings(),
                                   room_middle, in_the_way);
                if (in_the_way.HadHit()) {
                    continue;
                }
                // And under the pieces themselves, felt for every 0.4 m: a
                // seam or a gap the metre's grid stepped over would leave a
                // piece on an edge, never at rest.
                for (float u = -0.5F * spot.width; u <= 0.5F * spot.width + 0.2F && level; u += 0.4F) {
                    for (float v = near; v <= near + spot.depth + 0.2F && level; v += 0.4F) {
                        const RVec3 p = at(std::min(u, 0.5F * spot.width), std::min(v, near + spot.depth));
                        level = floor_at(static_cast<float>(p.GetX()), static_cast<float>(p.GetZ()), top, floor);
                    }
                }
                if (!level) {
                    continue;
                }
                const float yaw = std::atan2(-along.GetZ(), along.GetX());
                for (std::size_t n = 0; n < spot.pieces.size(); ++n) {
                    const SpotPiece &piece = spot.pieces[n];
                    const RVec3 p = at(piece.along, near + piece.out);
                    const auto variant =
                        static_cast<std::uint8_t>((piece.variant + static_cast<std::size_t>(taken) + n) % 3U);
                    add_piece({piece.kind, variant, static_cast<float>(p.GetX()), floor + piece.up,
                               static_cast<float>(p.GetZ()), yaw + piece.yaw});
                }
                spots.push_back(origin);
                ++taken;
            }
        }
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
