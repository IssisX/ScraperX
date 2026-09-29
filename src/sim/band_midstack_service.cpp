#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <vector>

// AS-010, Midstack Service (Atlas band B06, 640 -> 780 m). Contract:
// 03_EXECUTION/ASCENT/AS-010_MIDSTACK_SERVICE.md. Its first stage, M, is the
// band's service lift: a 1.5 t cage on TP-640 hauled 22 m to the 662 deck by
// a reel of lift cable falling beside it. The reel's cable is made fast
// above it, so it pays out as the reel falls and its weight leaves the reel:
// the drive fades through the stroke and turns, the cage rises to its apex
// just past the deck and settles back onto its safety dogs. No governor: the
// arrival is the machine's own.

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;
using Sim = Simulation;

Part box(const JPH::Vec3 half, const JPH::Vec3 center, const Material material) {
    return {half, center, JPH::Quat::sIdentity(), material};
}

Part turned(const JPH::Vec3 half, const JPH::Vec3 center, const JPH::Quat rotation, const Material material) {
    return {half, center, rotation, material};
}

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (high + low), material);
}

JPH::Vec3 vec(const JPH::RVec3 v) {
    return JPH::Vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
}

// A ladder at (x, z) from `bottom` to `top`: rails and rungs 0.3 m apart,
// the rungs across x (`across_x`) or across z, as AS-009 builds them.
constexpr float kMember = 0.03F;
constexpr float kRung = 0.30F;

void ladder(std::vector<Part> &parts, const float x, const float z, const bool across_x, const float bottom,
            const float top) {
    const JPH::Vec3 across = across_x ? JPH::Vec3::sAxisX() : JPH::Vec3::sAxisZ();
    for (const float side : {-1.0F, 1.0F}) {
        const JPH::Vec3 c = JPH::Vec3(x, 0.0F, z) + across * (side * 0.28F);
        parts.push_back(span({c.GetX() - kMember, bottom, c.GetZ() - kMember},
                             {c.GetX() + kMember, top, c.GetZ() + kMember}, Material::Yellow));
    }
    const JPH::Vec3 rung = across_x ? JPH::Vec3(0.28F, 0.02F, 0.02F) : JPH::Vec3(0.02F, 0.02F, 0.28F);
    for (float y = bottom + kRung; y <= top - 0.05F; y += kRung) {
        parts.push_back(box(rung, JPH::Vec3(x, y, z), Material::Steel));
    }
}

// Handles, levers and platforms, as AS-008 and AS-009 build them.
constexpr float kHandleHalfY = 0.04F;
constexpr float kHandleDrop = 0.75F;
constexpr float kHandleMassKg = 0.5F;       // a lanyard's T-handle
constexpr float kLeverRelease = 0.5F;
constexpr float kPlatformHalf = 1.3F;
constexpr float kStandingArm = 0.7F;

kit::BodyIndex add_handle(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 sheave) {
    const kit::BodyIndex handle = kit.add_body(
        entity, {box(JPH::Vec3(0.22F, kHandleHalfY, 0.04F), JPH::Vec3::sZero(), Material::Yellow)},
        sheave - JPH::RVec3(0.0, kHandleDrop + kHandleHalfY, 0.0), JPH::Quat::sIdentity(), kHandleMassKg, 0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kHandleHalfY, 0.0F));
    kit.set_damping(handle, 1.5F, 1.5F);
    return handle;
}

// A platform about its deck's top centre, railed along its north and south
// sides: in from the east or the west.
std::vector<Part> cage_parts() {
    std::vector<Part> parts{
        box(JPH::Vec3(kPlatformHalf, 0.1F, kPlatformHalf), JPH::Vec3(0.0F, -0.1F, 0.0F), Material::Galvanised)};
    const float edge = kPlatformHalf - 0.03F;
    for (const float side : {-1.0F, 1.0F}) {
        for (const float end : {-1.0F, 1.0F}) {
            parts.push_back(box(JPH::Vec3(0.03F, 0.5F, 0.03F), JPH::Vec3(side * edge, 0.5F, end * edge),
                                Material::Yellow));
        }
        parts.push_back(box(JPH::Vec3(kPlatformHalf, 0.03F, 0.03F), JPH::Vec3(0.0F, 1.0F, side * edge),
                            Material::Yellow));
    }
    return parts;
}

// A lever standing up from its pivot on a post, its weight a little west of
// the pivot holding it on its stop until its lanyard draws the top east
// (positive about -Z), as AS-009's chocks.
kit::BodyIndex add_standing_lever(kit::Kit &kit, std::vector<Part> &frame, const std::uint64_t entity,
                                  const JPH::RVec3 pivot, const float post_foot, kit::LeverIndex &lever) {
    const kit::BodyIndex body = kit.add_body(
        entity,
        {box(JPH::Vec3(0.04F, 0.5F * kStandingArm, 0.04F), JPH::Vec3(-0.1F, 0.5F * kStandingArm, 0.0F),
             Material::Hazard)},
        pivot, JPH::Quat::sIdentity(), 12.0F, 0.6F);
    lever = kit.add_lever(body, pivot, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisY(), 0.0F, 1.2F);
    const JPH::Vec3 p = vec(pivot);
    frame.push_back(span({p.GetX() - 0.06F, post_foot, p.GetZ() - 0.12F},
                         {p.GetX() + 0.06F, p.GetY() - 0.06F, p.GetZ() + 0.12F}, Material::Steel));
    return body;
}

// ---- the frame over TP-640 and the 662 deck ---------------------------------------
constexpr float kPlateTop = 640.25F;
// Where the cage stands on its dogs: a hand's breadth either way of the
// deck, over the whole friction band, so its rider steps off either way.
constexpr float kDeckBottom = 661.6F;
constexpr float kDeckTop = 662.1F;
constexpr float kHalf = 12.0F;              // TP-640's footprint, about (0, -150)
constexpr float kWellZ = -150.0F;

// Stage M's two shafts through the deck: the cage's and the reel's; and the
// climbing route's hatch.
constexpr float kMX = -8.0F;
constexpr float kMZ = -143.5F;
constexpr float kRX = -10.8F;
constexpr float kRZ = -143.5F;
constexpr float kCageHoleX0 = kMX - 1.4F;
constexpr float kCageHoleX1 = kMX + 1.4F;
constexpr float kCageHoleZ0 = kMZ - 1.5F;
constexpr float kCageHoleZ1 = kMZ + 1.9F;   // and the lanyard's line past its north edge
constexpr float kReelHoleX0 = kRX - 1.1F;
constexpr float kReelHoleX1 = kCageHoleX0;
constexpr float kReelHoleZ0 = kRZ - 0.8F;
constexpr float kReelHoleZ1 = kRZ + 0.8F;
constexpr float kHatchX0 = 8.1F;
constexpr float kHatchX1 = 9.7F;
constexpr float kHatchZ0 = -147.4F;
constexpr float kHatchZ1 = -145.25F;

void build_frame(std::vector<Part> &frame) {
    // Corner columns from TP-640 to the deck's underside.
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            frame.push_back(box(JPH::Vec3(0.35F, 0.5F * (kDeckBottom - kPlateTop), 0.35F),
                                JPH::Vec3(sx * (kHalf - 0.4F), 0.5F * (kPlateTop + kDeckBottom),
                                          kWellZ + sz * (kHalf - 0.4F)),
                                Material::Rust));
        }
    }
    // The deck, in strips round the reel's and the cage's shafts and the
    // hatch.
    const auto deck = [&](const float x0, const float x1, const float z0, const float z1) {
        frame.push_back(span({x0, kDeckBottom, z0}, {x1, kDeckTop, z1}, Material::Concrete));
    };
    const float z0 = kWellZ - kHalf;
    const float z1 = kWellZ + kHalf;
    deck(-kHalf, kReelHoleX0, z0, z1);
    deck(kReelHoleX0, kReelHoleX1, z0, kReelHoleZ0);
    deck(kReelHoleX0, kReelHoleX1, kReelHoleZ1, z1);
    deck(kCageHoleX0, kCageHoleX1, z0, kCageHoleZ0);
    deck(kCageHoleX0, kCageHoleX1, kCageHoleZ1, z1);
    deck(kCageHoleX1, kHatchX0, z0, z1);
    deck(kHatchX0, kHatchX1, z0, kHatchZ0);
    deck(kHatchX0, kHatchX1, kHatchZ1, z1);
    deck(kHatchX1, kHalf, z0, z1);
}

// ---- Stage M -----------------------------------------------------------------
// Numbers from the evaluator (stage1dof.py, the contract's spec): INTEGRATED
// over guide friction 50 to 150 N, every case caught on its fall-back.
constexpr float kMDeck = 640.45F;           // the cage's deck, standing on TP-640
constexpr float kMStop = 22.9F;             // the guide's top, a dog's tooth
constexpr float kMDogPitch = 0.1F;
constexpr float kMFriction = 100.0F;        // N, the cage's guide shoes
constexpr float kMMassKg = 1500.0F;
const JPH::Vec3 kMEyeLocal(1.0F, 1.1F, 0.0F);
constexpr float kReelMassKg = 1339.5F;      // drum and yoke
constexpr float kCableKgPerM = 22.0F;
constexpr float kCableCoil = 23.0F;         // m wound, a metre past the stroke
constexpr float kReelRadius = 0.9F;
constexpr float kReelTop = 664.4F;          // the drum's hub, held at the top
constexpr float kSheaveY = 668.0F;
const JPH::RVec3 kReelHub(kRX, kReelTop, kRZ);
const JPH::RVec3 kChockPivot(kRX + 1.2, 662.95, kRZ + 1.1);
const JPH::RVec3 kLanyard1(kMX, 663.6, kMZ + 1.6);
const JPH::RVec3 kLanyard2(kMX, kMDeck + 1.94, kMZ + 1.6);

void build_stage_m(kit::Kit &kit, MidstackService &service, std::vector<Part> &frame) {
    service.m_cage = kit.add_body(Sim::kServiceMCageEntityId, cage_parts(), JPH::RVec3(kMX, kMDeck, kMZ),
                                  JPH::Quat::sIdentity(), kMMassKg, 0.8F);
    service.m_cage_guide = kit.add_guide(service.m_cage, JPH::Vec3::sAxisY(), 0.0F, kMStop, 0.0F, 0.0F, 0.0F);
    // No air drag worth the name at 3 m/s (some 15 N on the cage): the guide
    // shoes are its only resistance, as the evaluator has it.
    kit.set_damping(service.m_cage, 0.0F, 0.05F);
    kit.set_dogs(service.m_cage_guide, kMDogPitch);
    kit.set_guide_friction(service.m_cage_guide, kMFriction);
    service.m_eye = kit.add_anchor(service.m_cage, kMEyeLocal, 1.2F);
    // The cage's guide posts, either side, TP-640 to the head.
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kMX - 0.05F, kPlateTop, kMZ + side * 1.45F - 0.05F},
                             {kMX + 0.05F, kSheaveY - 0.3F, kMZ + side * 1.45F + 0.05F}, Material::Steel));
    }

    // The reel: a drum of lift cable in its yoke, its axle along z, held at
    // the top of its shaft by a chock; its guide rails down to a timber crib
    // on TP-640.
    std::vector<Part> reel;
    const float flange = kReelRadius;
    for (const float z : {-0.5F, 0.5F}) {
        for (const float turn : {0.0F, 0.7854F}) {
            reel.push_back(turned(JPH::Vec3(flange * 0.707F, flange * 0.707F, 0.04F), JPH::Vec3(0.0F, 0.0F, z),
                                  JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), turn), Material::Rust));
        }
    }
    for (const float turn : {0.0F, 0.7854F}) {
        reel.push_back(turned(JPH::Vec3(0.55F, 0.55F, 0.46F), JPH::Vec3::sZero(),
                              JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), turn), Material::Steel));
    }
    reel.push_back(box(JPH::Vec3(0.06F, 0.95F, 0.06F), JPH::Vec3(0.0F, 0.05F, -0.62F), Material::Yellow));
    reel.push_back(box(JPH::Vec3(0.06F, 0.95F, 0.06F), JPH::Vec3(0.0F, 0.05F, 0.62F), Material::Yellow));
    reel.push_back(box(JPH::Vec3(0.08F, 0.06F, 0.68F), JPH::Vec3(0.0F, 1.0F, 0.0F), Material::Yellow));
    service.m_reel = kit.add_body(Sim::kServiceMReelEntityId, reel, kReelHub, JPH::Quat::sIdentity(), kReelMassKg,
                                  0.6F);
    service.m_reel_guide = kit.add_guide(service.m_reel, -JPH::Vec3::sAxisY(), 0.0F, kMStop, 0.0F, 0.0F, 0.0F);
    kit.set_damping(service.m_reel, 0.0F, 0.05F);
    // Its cable made fast to the head beam over the drum's east side.
    const JPH::Vec3 rim(kReelRadius * 0.6F, 0.0F, 0.0F);
    service.m_reel_cable = kit.add_reel(service.m_reel, kReelHub + JPH::RVec3(rim) + JPH::RVec3(0.0, 3.4, 0.0), rim,
                                        kCableKgPerM, kCableCoil);
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kRX - 0.05F, kPlateTop, kRZ + side * 0.72F - 0.05F},
                             {kRX + 0.05F, kSheaveY - 0.3F, kRZ + side * 0.72F + 0.05F}, Material::Steel));
    }
    frame.push_back(span({kRX - 1.0F, kPlateTop, kRZ - 0.7F}, {kRX + 1.0F, kPlateTop + 0.3F, kRZ + 0.7F},
                         Material::Timber));
    service.m_chock = add_standing_lever(kit, frame, Sim::kServiceMChockEntityId, kChockPivot, kDeckTop,
                                         service.m_chock_lever);
    service.m_reel_catch = kit.add_catch(service.m_reel, service.m_chock_lever, kLeverRelease, 0.05F, false);
    service.m_handle = add_handle(kit, Sim::kServiceMHandleEntityId, kLanyard2);
    (void)kit.add_trip_line(service.m_chock, JPH::Vec3(-0.1F, kStandingArm, 0.0F), service.m_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kLanyard1, kLanyard2);

    // The head: a beam over both shafts carrying the reel's sheave and the
    // cage's, on posts from the deck either side.
    frame.push_back(span({kRX - 0.6F, kSheaveY + 0.2F, kMZ - 0.25F}, {kMX + 0.6F, kSheaveY + 0.5F, kMZ + 0.25F},
                         Material::Yellow));
    for (const float z : {kCageHoleZ0 - 0.3F, kCageHoleZ1 + 0.3F}) {
        frame.push_back(span({kCageHoleX0 - 0.15F, kDeckTop, z - 0.15F}, {kCageHoleX0 + 0.15F, kSheaveY + 0.5F, z + 0.15F},
                             Material::Yellow));
    }
    frame.push_back(span({kCageHoleX0 - 0.1F, kSheaveY + 0.2F, kCageHoleZ0 - 0.45F},
                         {kCageHoleX0 + 0.1F, kSheaveY + 0.5F, kCageHoleZ1 + 0.45F}, Material::Yellow));

    // The hoist rope: the reel's hub over its sheave, across the head to the
    // sheave over the cage, down to its shackle hanging free over the deck.
    const JPH::RVec3 f1(kRX, kSheaveY, kRZ);
    const JPH::RVec3 f2(kMX, kSheaveY, kMZ);
    const JPH::RVec3 eye = JPH::RVec3(kMX, kMDeck, kMZ) + JPH::RVec3(kMEyeLocal);
    const float hauling = static_cast<float>(JPH::Vec3(eye - f2).Length()) + 0.08F;
    service.m_shackle = kit.add_body(Sim::kServiceMShackleEntityId,
                                     {box(JPH::Vec3(0.08F, 0.06F, 0.05F), JPH::Vec3::sZero(), Material::Yellow)},
                                     f2 - JPH::RVec3(0.0, hauling, 0.0), JPH::Quat::sIdentity(), 6.0F, 0.8F);
    kit.set_carry(service.m_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.06F, 0.0F));
    const float length = static_cast<float>(JPH::Vec3(kReelHub - f1).Length()) + hauling;
    service.m_rope = kit.add_rope(service.m_reel, JPH::Vec3::sZero(), f1, service.m_shackle, JPH::Vec3::sZero(), f2,
                                  1.0F, length, 0.0F);
}

// ---- the climbing route, no lift --------------------------------------------------
// A ladder from TP-640 up through the deck's hatch.
void build_climbing_route(kit::Kit &kit) {
    std::vector<Part> route;
    ladder(route, 0.5F * (kHatchX0 + kHatchX1), kHatchZ1 - 0.15F, true, kPlateTop, kDeckTop);
    (void)kit.add_body(Sim::kServiceRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace

void build_midstack_service(kit::Kit &kit, MidstackService &service) {
    std::vector<Part> frame;
    build_frame(frame);
    build_stage_m(kit, service, frame);
    (void)kit.add_body(Sim::kServiceFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    build_climbing_route(kit);
}

} // namespace scraperx::sim::bands
