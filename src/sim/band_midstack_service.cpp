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
// arrival is the machine's own. Then C4, a climb: the service gantry, from
// the 662 deck to the 684 deck on nothing but the player's movement.

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

// ---- the 684 deck ------------------------------------------------------------------
// Over the footprint's west, above M and its head frame. Its east edge is a
// deep yellow girder, the face C4's last mantle takes and the line its
// climber sees from the 662 deck; the backup ladder comes up through a hatch.
constexpr float kUpperBottom = 683.6F;
constexpr float kUpperTop = 684.1F;
constexpr float kUpperEast = -2.0F;
constexpr float kUpperGirderBottom = 682.9F;
constexpr float kUpperHatchX0 = -4.8F;
constexpr float kUpperHatchX1 = -3.2F;
constexpr float kUpperHatchZ0 = -147.4F;
constexpr float kUpperHatchZ1 = -145.25F;

void build_upper_deck(std::vector<Part> &frame) {
    // Columns from the 662 deck: the west corners over the 662 deck's own,
    // and three along the east edge, thicker than any hold.
    for (const float z : {kWellZ - kHalf + 0.4F, kWellZ + kHalf - 0.4F}) {
        frame.push_back(span({-kHalf + 0.05F, kDeckTop, z - 0.35F}, {-kHalf + 0.75F, kUpperBottom, z + 0.35F},
                             Material::Rust));
    }
    for (const float z : {kWellZ - kHalf + 0.4F, kWellZ, kWellZ + kHalf - 0.4F}) {
        frame.push_back(span({kUpperEast - 1.0F, kDeckTop, z - 0.35F}, {kUpperEast - 0.3F, kUpperBottom, z + 0.35F},
                             Material::Rust));
    }
    const auto deck = [&](const float x0, const float x1, const float z0, const float z1) {
        frame.push_back(span({x0, kUpperBottom, z0}, {x1, kUpperTop, z1}, Material::Concrete));
    };
    const float z0 = kWellZ - kHalf;
    const float z1 = kWellZ + kHalf;
    deck(-kHalf, kUpperHatchX0, z0, z1);
    deck(kUpperHatchX0, kUpperHatchX1, z0, kUpperHatchZ0);
    deck(kUpperHatchX0, kUpperHatchX1, kUpperHatchZ1, z1);
    deck(kUpperHatchX1, kUpperEast, z0, z1);
    frame.push_back(span({kUpperEast - 0.3F, kUpperGirderBottom, z0}, {kUpperEast, kUpperBottom, z1}, Material::Yellow));
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

// ---- C4, the service gantry: a climb from the 662 deck to the 684 deck --------------
// One static body of cabinet, duct, beam, girder and plate, climbed on the
// movement the native allows and nothing else, each move well inside its
// envelope (simulation.cpp): mantles of 1.55 m (0.9 to 1.85 m), hangs caught
// 3.2 m up (3.76 m reach) on faces 1.2 m or more deep, a standpipe, a level
// beam, a drop over an edge into a hang and a shimmy past a winch house, a
// 3 m gap. Each platform's face and top are this one body, as the ledge
// probe needs; every column is thicker than a hold (0.18 m), so nothing but
// the standpipe is climbed hand over hand. Where the duct, the pump deck and
// the runway come within 1 m of the tower's faces, a 0.8 m parapet (below the
// 0.9 m the ledge probe starts at, above a step, thicker than a hold) keeps a
// body walking into it from going off past the faces; a jump clears it. A
// miss elsewhere lands on a lower platform or the 662 deck, under the 20.4 m
// a body survives, except from the hoist platform.
constexpr float kC4Cabinet = 663.65F;       // the switchgear cabinet: mantle 1.55
constexpr float kC4Duct = 666.85F;          // the duct and the pump deck: hang 3.2
constexpr float kC4Runway = 674.6F;         // the hoist runway: the standpipe, 7.75
constexpr float kC4Landing = 674.6F;        // across a 3 m gap from the runway
constexpr float kC4Gallery = 677.8F;        // hang 3.2
constexpr float kC4Riser = 679.35F;         // mantle 1.55
constexpr float kC4Hoist = 682.55F;         // hang 3.2; mantle 1.55 onto the 684 deck
constexpr float kC4Column = 0.2F;           // a column's half width
constexpr float kC4Parapet = 0.8F;          // under the ledge probe's reach, over a step
constexpr float kC4ParapetWidth = 0.25F;    // thicker than a hold

void c4_column(std::vector<Part> &parts, const float x, const float z, const float top) {
    parts.push_back(span({x - kC4Column, kDeckTop, z - kC4Column}, {x + kC4Column, top, z + kC4Column}, Material::Rust));
}

// A parapet on a platform whose top is `top`, from (x0, z0) to (x1, z1).
void c4_parapet(std::vector<Part> &parts, const float x0, const float z0, const float x1, const float z1,
                const float top) {
    parts.push_back(span({x0, top, z0}, {x1, top + kC4Parapet, z1}, Material::Concrete));
}

void build_c4(kit::Kit &kit) {
    std::vector<Part> c4;
    // 1. The switchgear cabinet, mantled from the 662 deck facing -z.
    c4.push_back(span({5.0F, kDeckTop, -151.2F}, {7.4F, kC4Cabinet, -149.8F}, Material::Hazard));
    // 2. The duct along x, its +z face 0.3 m past the cabinet: from the
    // cabinet's back edge a jump catches its lip.
    c4.push_back(span({1.0F, kC4Duct - 1.35F, -153.1F}, {11.0F, kC4Duct, -151.5F}, Material::Galvanised));
    for (const float x : {1.4F, 10.6F}) {
        c4_column(c4, x, -152.3F, kC4Duct - 1.35F);
    }
    c4_parapet(c4, 11.0F - kC4ParapetWidth, -153.1F, 11.0F, -151.5F, kC4Duct);
    // 3. A level beam from the duct's back edge to the pump deck: 0.4 m wide,
    // 4.5 m long, open on both sides.
    c4.push_back(span({9.8F, kC4Duct - 0.4F, -157.6F}, {10.2F, kC4Duct, -153.1F}, Material::Yellow));
    c4.push_back(span({8.0F, kC4Duct - 0.5F, -161.5F}, {11.5F, kC4Duct, -157.6F}, Material::Steel));
    for (const JPH::Vec3 at : {JPH::Vec3(8.3F, 0.0F, -161.2F), JPH::Vec3(8.3F, 0.0F, -157.9F),
                               JPH::Vec3(11.2F, 0.0F, -157.9F)}) {
        c4_column(c4, at.GetX(), at.GetZ(), kC4Duct - 0.5F);
    }
    c4.push_back(span({10.2F, kC4Duct, -161.3F}, {11.3F, kC4Duct + 1.0F, -160.3F}, Material::Rust));   // the pump
    c4_parapet(c4, 8.0F, -161.5F, 11.5F, -161.5F + kC4ParapetWidth, kC4Duct);
    c4_parapet(c4, 11.5F - kC4ParapetWidth, -161.5F, 11.5F, -157.6F, kC4Duct);
    // 4. The standpipe up the runway's +z face, 0.14 m off it, a hold from the
    // pump deck to 0.2 m under the runway's top.
    c4.push_back(span({9.42F, kC4Duct, -159.34F}, {9.58F, kC4Runway - 0.2F, -159.18F}, Material::Yellow));
    // 5. The hoist runway, a 1.2 m girder 10.5 m long, and its winch house:
    // 4.2 m high, out of any jump's reach, filling the runway. From 0.35 m up
    // its face stands flush with the runway's +z lip, over a plinth set 0.2 m
    // back: to pass on foot a body would have to stand 0.35 m out beyond the
    // lip, where the edge no longer holds it up, while a body hanging from
    // the lip passes under the overhang, and the lip probe (0.25 m up, 0.12 m
    // in) still finds the runway's top. The way west along the runway is
    // over that lip, hanging; a jump off its east part across to the landing
    // is the other way past the house.
    c4.push_back(span({1.0F, kC4Runway - 1.2F, -161.8F}, {11.5F, kC4Runway, -159.4F}, Material::Rust));
    c4_parapet(c4, 1.0F, -161.8F, 11.5F, -161.8F + kC4ParapetWidth, kC4Runway);
    c4_parapet(c4, 11.5F - kC4ParapetWidth, -161.8F, 11.5F, -159.4F, kC4Runway);
    for (const float x : {1.3F, 6.0F, 11.2F}) {
        for (const float z : {-161.5F, -159.7F}) {
            c4_column(c4, x, z, kC4Runway - 1.2F);
        }
    }
    c4.push_back(span({5.0F, kC4Runway, -161.8F}, {7.0F, kC4Runway + 0.35F, -159.6F}, Material::Concrete));
    c4.push_back(span({5.0F, kC4Runway + 0.35F, -161.8F}, {7.0F, kC4Runway + 4.2F, -159.4F}, Material::Concrete));
    // 6. Across a 3 m gap in +z from the runway's west end, a landing.
    c4.push_back(span({1.0F, kC4Landing - 0.3F, -156.4F}, {4.0F, kC4Landing, -152.9F}, Material::Galvanised));
    for (const float x : {1.3F, 3.7F}) {
        for (const float z : {-156.1F, -153.6F}) {
            c4_column(c4, x, z, kC4Landing - 0.3F);
        }
    }
    // 7. The gallery, its -z face 0.4 m past the landing's edge: a hang.
    c4.push_back(span({0.5F, kC4Gallery - 1.35F, -152.5F}, {4.5F, kC4Gallery, -150.9F}, Material::Yellow));
    for (const float x : {0.8F, 4.2F}) {
        c4_column(c4, x, -151.1F, kC4Gallery - 1.35F);
    }
    // 8. The riser, a concrete shaft beside the gallery's west end: a mantle.
    c4.push_back(span({-2.0F, kDeckTop, -152.5F}, {0.5F, kC4Riser, -150.9F}, Material::Concrete));
    // 9. The hoist platform, its +z face 0.4 m past the riser's: a hang; from
    // its west end a mantle onto the 684 deck.
    c4.push_back(span({kUpperEast, kC4Hoist - 1.35F, -154.5F}, {0.8F, kC4Hoist, -152.9F}, Material::Yellow));
    for (const float x : {-1.7F, 0.5F}) {
        c4_column(c4, x, -154.2F, kC4Hoist - 1.35F);
    }
    (void)kit.add_body(Sim::kServiceC4EntityId, c4, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

// ---- Stage N, the granular discharge hoist: the 684 deck to the 706 deck ------------
// The owner's archetype 09. A 2 t cage stands on the 684 deck; west of it a
// steel hopper hangs empty at the top of its own guide from the rope over the
// head, a chain hanging from its floor to the deck. Over the hopper, an
// aggregate bin stands on the headframe, its chute shut by a counterweighted
// flap whose arm reaches over the cage, a chain hanging from its end into the
// cage. Taken hold of there, the chain draws the arm past its dead point and
// the flap falls open: the bin pours into the hopper, which outweighs the
// cage and its rider two-thirds of the way through the pour and sinks, the
// last of the gravel falling in with it. Every metre it sinks sets a metre of
// its chain down on the deck, so its drive fades through the stroke and
// turns; the cage rises past the 706 deck and settles back onto its safety
// dogs. No catch and no governor: the gravel starts it, the chain stops it.
// Numbers from the evaluator (stage1dof.py, the contract's spec): INTEGRATED
// over the guide friction band, every case caught on its fall-back.
constexpr float kNX = -6.5F;                    // the cage, about its deck's top centre
constexpr float kNZ = -155.0F;
constexpr float kNDeck = kUpperTop + 0.2F;      // on the 684 deck
constexpr float kNStop = 22.9F;                 // the guide's top, a dog's tooth
constexpr float kNDogPitch = 0.1F;
constexpr float kNFriction = 100.0F;            // N, the cage's guide shoes
constexpr float kNCageMassKg = 2000.0F;
const JPH::Vec3 kNEyeLocal(-1.0F, 1.1F, 0.25F);
// The hopper, about its floor's centre: 1.2 m square inside, 1.4 m deep, a
// bail across its top north of the pour.
constexpr float kNHopperX = -10.0F;
constexpr float kNHopperHalf = 0.6F;
constexpr float kNHopperDepth = 1.4F;
constexpr float kNHopperFloorHalf = 0.06F;
constexpr float kNHopperWall = 0.03F;
constexpr float kNHopperMassKg = 500.0F;
constexpr float kNHopperCapacityKg = 1600.0F;   // more than the bin holds
const JPH::Vec3 kNBailLocal(0.0F, kNHopperDepth + 0.2F, 0.25F);
// Its chain, from the middle of its underside to the 684 deck: each metre the
// hopper sinks sets 33 kg of it down.
constexpr float kNChainKgPerM = 33.0F;
constexpr float kNChainLength = 23.0F;          // a metre past the stroke
constexpr float kNHopperY = kUpperTop + kNChainLength + kNHopperFloorHalf;
// The bin: the charge, through a chute 0.5 m square (Beverloo: some 400 kg/s
// of 10 mm gravel).
constexpr float kNGravelKg = 1213.0F;
constexpr float kNPourKgPerS = 400.0F;
const JPH::RVec3 kNMouth(kNHopperX, 710.2, kNZ - 0.35);
constexpr float kNBinBottom = 712.5F;
constexpr float kNBinTop = 714.3F;
// The flap turns about -z on a pin west of the chute, its arm reaching east
// under the chute's mouth and on over the cage. Its weight rides up and west
// of the pin, so it rests shut on its stop; pulled some 0.43 m, the arm's end
// passes the dead point and the flap falls open onto its other stop, and
// stays there.
const JPH::RVec3 kNFlapPin(kNHopperX - 0.4, 710.05, kNZ - 0.35);
constexpr float kNFlapArm = 3.3F;
constexpr float kNFlapMassKg = 100.0F;
constexpr float kNFlapOpen = 0.3F;              // past the dead point
constexpr float kNFlapStop = 0.4F;
constexpr float kNFlapReach = 0.6F;
const JPH::Vec3 kNFlapWeight(-0.55F, 0.87F, -0.8F);   // clear of the hopper's posts
constexpr float kNChainGuideDrop = 1.5F;        // the chain's guide under the arm's end
constexpr float kNHandleTop = kNDeck + 1.95F;   // at rest, in the cage
constexpr float kNSheaveY = 711.4F;
// The headframe's posts: round the hopper's shaft, and east of the cage.
constexpr float kNHeadY = 712.2F;
constexpr float kNPostXs[3] = {-11.4F, -8.6F, -4.9F};
constexpr float kNPostZs[2] = {-157.9F, -152.1F};
// The 706 deck: a strip east of the cage, from the cage's side to over the
// 684 deck's east girder, railed all round but where the cage comes to it;
// the backup ladder up through its hatch.
constexpr float kN706Bottom = 705.6F;
constexpr float kN706Top = 706.1F;
constexpr float kN706West = kNX + kPlatformHalf + 0.1F;
constexpr float kN706East = kUpperEast;
constexpr float kN706HatchX0 = -4.3F;
constexpr float kN706HatchX1 = -2.7F;
constexpr float kN706HatchZ0 = -141.5F;
constexpr float kN706HatchZ1 = -139.3F;
// Where C5's plant floor opens off the 706 deck's east edge.
constexpr float kC5FloorZ0 = -149.0F;
constexpr float kC5FloorZ1 = -141.0F;

void build_stage_n_frame(std::vector<Part> &frame) {
    const float z0 = kWellZ - kHalf;
    const float z1 = kWellZ + kHalf;
    // The headframe: posts from the 684 deck, beams along the head carrying
    // the sheaves, the bin's bearers and the chain's guide.
    for (const float x : kNPostXs) {
        for (const float z : kNPostZs) {
            frame.push_back(span({x - 0.15F, kUpperTop, z - 0.15F}, {x + 0.15F, kNHeadY, z + 0.15F},
                                 Material::Yellow));
        }
    }
    for (const float z : {kNPostZs[0], kNZ + 0.25F, kNPostZs[1]}) {
        frame.push_back(span({kNPostXs[0] - 0.15F, kNHeadY - 0.3F, z - 0.15F},
                             {kNPostXs[2] + 0.15F, kNHeadY, z + 0.15F}, Material::Yellow));
    }
    for (const float x : kNPostXs) {
        frame.push_back(span({x - 0.15F, kNHeadY - 0.3F, kNPostZs[0] - 0.15F},
                             {x + 0.15F, kNHeadY, kNPostZs[1] + 0.15F}, Material::Yellow));
    }
    for (const float z : {kNZ - 1.9F, kNZ - 0.2F}) {
        frame.push_back(span({kNPostXs[0], kNHeadY, z - 0.15F}, {kNPostXs[1], kNHeadY + 0.3F, z + 0.15F},
                             Material::Rust));
    }
    const float guide_x = kNHopperX + kNFlapArm - 0.4F;
    const float guide_y = static_cast<float>(kNFlapPin.GetY()) - kNChainGuideDrop;
    frame.push_back(span({guide_x - 0.04F, guide_y - 0.05F, kNZ - 0.2F}, {guide_x + 0.04F, kNHeadY - 0.3F, kNZ - 0.12F},
                         Material::Steel));
    // The flap's pin in a hanger from the bin, north of the arm.
    frame.push_back(span({static_cast<float>(kNFlapPin.GetX()) - 0.05F, static_cast<float>(kNFlapPin.GetY()) - 0.06F,
                          kNZ - 0.25F},
                         {static_cast<float>(kNFlapPin.GetX()) + 0.05F, kNBinBottom, kNZ - 0.15F}, Material::Steel));
    // The hopper's guide posts at its corners, and the cage's either side.
    for (const float dx : {-0.78F, 0.78F}) {
        for (const float dz : {-0.78F, 0.78F}) {
            frame.push_back(span({kNHopperX + dx - 0.05F, kUpperTop, kNZ + dz - 0.05F},
                                 {kNHopperX + dx + 0.05F, kNHeadY - 0.3F, kNZ + dz + 0.05F}, Material::Steel));
        }
    }
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kNX - 0.05F, kUpperTop, kNZ + side * 1.45F - 0.05F},
                             {kNX + 0.05F, kNHeadY - 0.3F, kNZ + side * 1.45F + 0.05F}, Material::Steel));
    }

    // The 706 deck, on columns from the 684 deck, round its hatch.
    for (const float z : {z0 + 0.4F, kWellZ, z1 - 0.4F}) {
        frame.push_back(span({kN706East - 0.9F, kUpperTop, z - 0.25F}, {kN706East - 0.4F, kN706Bottom, z + 0.25F},
                             Material::Rust));
    }
    for (const float z : {z0 + 0.4F, z1 - 0.4F}) {
        frame.push_back(span({kN706West + 0.1F, kUpperTop, z - 0.25F}, {kN706West + 0.6F, kN706Bottom, z + 0.25F},
                             Material::Rust));
    }
    const auto deck = [&](const float x0, const float x1, const float za, const float zb) {
        frame.push_back(span({x0, kN706Bottom, za}, {x1, kN706Top, zb}, Material::Concrete));
    };
    deck(kN706West, kN706HatchX0, z0, z1);
    deck(kN706HatchX0, kN706HatchX1, z0, kN706HatchZ0);
    deck(kN706HatchX0, kN706HatchX1, kN706HatchZ1, z1);
    deck(kN706HatchX1, kN706East, z0, z1);
    // Parapets as C4's: along the east edge, the tower's faces, and the west
    // edge either side of where the cage comes up.
    const auto parapet = [&](const float x0, const float za, const float x1, const float zb) {
        frame.push_back(span({x0, kN706Top, za}, {x1, kN706Top + kC4Parapet, zb}, Material::Concrete));
    };
    parapet(kN706East - kC4ParapetWidth, z0, kN706East, kC5FloorZ0);
    parapet(kN706East - kC4ParapetWidth, kC5FloorZ1, kN706East, z1);
    parapet(kN706West, z0, kN706East, z0 + kC4ParapetWidth);
    parapet(kN706West, z1 - kC4ParapetWidth, kN706East, z1);
    parapet(kN706West, z0, kN706West + kC4ParapetWidth, kNZ - kPlatformHalf - 0.1F);
    parapet(kN706West, kNZ + kPlatformHalf + 0.1F, kN706West + kC4ParapetWidth, z1);
}

void build_stage_n(kit::Kit &kit, MidstackService &service) {
    // The aggregate bin on its bearers, its chute down to the flap.
    const std::vector<Part> bin{
        span({kNHopperX - 0.7F, kNBinBottom, kNZ - 2.0F}, {kNHopperX + 0.7F, kNBinTop, kNZ - 0.1F},
             Material::Galvanised),
        span({kNHopperX - 0.25F, static_cast<float>(kNMouth.GetY()), kNZ - 0.6F},
             {kNHopperX + 0.25F, kNBinBottom, kNZ - 0.1F}, Material::Hazard)};
    service.n_silo = kit.add_body(Sim::kServiceNSiloEntityId, bin, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F,
                                  0.8F);

    // The flap: its arm under the chute's mouth and on east over the cage, a
    // plate under the mouth, its weight on struts up and west of the pin.
    const JPH::Vec3 weight_strut = kNFlapWeight - JPH::Vec3(0.0F, 0.0F, kNFlapWeight.GetZ());
    service.n_gate = kit.add_body(
        Sim::kServiceNGateEntityId,
        {box(JPH::Vec3(0.5F * kNFlapArm, 0.05F, 0.05F), JPH::Vec3(0.5F * kNFlapArm, 0.0F, 0.0F), Material::Hazard),
         box(JPH::Vec3(0.3F, 0.02F, 0.3F), JPH::Vec3(0.4F, 0.07F, 0.0F), Material::Steel),
         box(JPH::Vec3(0.04F, 0.04F, 0.5F * -kNFlapWeight.GetZ()), JPH::Vec3(0.0F, 0.0F, 0.5F * kNFlapWeight.GetZ()),
             Material::Steel),
         turned(JPH::Vec3(0.5F * weight_strut.Length(), 0.04F, 0.04F),
                0.5F * weight_strut + JPH::Vec3(0.0F, 0.0F, kNFlapWeight.GetZ()),
                JPH::Quat::sFromTo(JPH::Vec3::sAxisX(), weight_strut.Normalized()), Material::Steel),
         box(JPH::Vec3::sReplicate(0.25F), kNFlapWeight, Material::Rust)},
        kNFlapPin, JPH::Quat::sIdentity(), kNFlapMassKg, 0.5F);
    service.n_gate_lever = kit.add_lever(service.n_gate, kNFlapPin, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), 0.0F,
                                         kNFlapStop);
    service.n_silo_bin = kit.add_bin(service.n_silo, kNGravelKg, kNGravelKg, vec(kNMouth), service.n_gate_lever,
                                     kNFlapOpen, kNFlapReach, kNPourKgPerS);

    // The chain from the arm's end, down past its guide into the cage.
    const JPH::RVec3 arm_end = kNFlapPin + JPH::RVec3(kNFlapArm, 0.0, 0.0);
    const JPH::RVec3 chain_guide = arm_end - JPH::RVec3(0.0, kNChainGuideDrop, 0.0);
    service.n_handle = kit.add_body(Sim::kServiceNHandleEntityId,
                                    {box(JPH::Vec3(0.04F, kHandleHalfY, 0.22F), JPH::Vec3::sZero(), Material::Hazard)},
                                    JPH::RVec3(arm_end.GetX(), kNHandleTop - kHandleHalfY, arm_end.GetZ()),
                                    JPH::Quat::sIdentity(), kHandleMassKg, 0.9F);
    kit.set_carry(service.n_handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kHandleHalfY, 0.0F));
    kit.set_damping(service.n_handle, 1.5F, 1.5F);
    (void)kit.add_trip_line(service.n_gate, JPH::Vec3(kNFlapArm, 0.0F, 0.0F), service.n_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), chain_guide, chain_guide);

    // The cage, on its guide with safety dogs and no governor.
    service.n_cage = kit.add_body(Sim::kServiceNCageEntityId, cage_parts(), JPH::RVec3(kNX, kNDeck, kNZ),
                                  JPH::Quat::sIdentity(), kNCageMassKg, 0.8F);
    service.n_cage_guide = kit.add_guide(service.n_cage, JPH::Vec3::sAxisY(), 0.0F, kNStop, 0.0F, 0.0F, 0.0F);
    kit.set_damping(service.n_cage, 0.0F, 0.05F);
    kit.set_dogs(service.n_cage_guide, kNDogPitch);
    kit.set_guide_friction(service.n_cage_guide, kNFriction);

    // The hopper, empty at the top of its guide, its chain to the deck.
    const float wall_y = 0.5F * (kNHopperDepth + kNHopperFloorHalf);
    const float wall_half_y = 0.5F * (kNHopperDepth - kNHopperFloorHalf);
    const float out = kNHopperHalf + kNHopperWall;
    service.n_hopper = kit.add_body(
        Sim::kServiceNHopperEntityId,
        {box(JPH::Vec3(kNHopperHalf, kNHopperFloorHalf, kNHopperHalf), JPH::Vec3::sZero(), Material::Rust),
         box(JPH::Vec3(kNHopperWall, wall_half_y, out), JPH::Vec3(-out, wall_y, 0.0F), Material::Rust),
         box(JPH::Vec3(kNHopperWall, wall_half_y, out), JPH::Vec3(out, wall_y, 0.0F), Material::Rust),
         box(JPH::Vec3(kNHopperHalf, wall_half_y, kNHopperWall), JPH::Vec3(0.0F, wall_y, -out), Material::Rust),
         box(JPH::Vec3(kNHopperHalf, wall_half_y, kNHopperWall), JPH::Vec3(0.0F, wall_y, out), Material::Rust),
         box(JPH::Vec3(out, 0.05F, kNHopperWall), JPH::Vec3(0.0F, kNHopperDepth, -out), Material::Hazard),
         box(JPH::Vec3(0.03F, 0.12F, 0.04F), JPH::Vec3(-out, kNHopperDepth + 0.1F, kNBailLocal.GetZ()),
             Material::Yellow),
         box(JPH::Vec3(0.03F, 0.12F, 0.04F), JPH::Vec3(out, kNHopperDepth + 0.1F, kNBailLocal.GetZ()), Material::Yellow),
         box(JPH::Vec3(out, 0.04F, 0.04F), kNBailLocal, Material::Yellow)},
        JPH::RVec3(kNHopperX, kNHopperY, kNZ), JPH::Quat::sIdentity(), kNHopperMassKg, 0.6F);
    service.n_hopper_guide = kit.add_guide(service.n_hopper, -JPH::Vec3::sAxisY(), 0.0F, kNStop, 0.0F, 0.0F, 0.0F);
    kit.set_damping(service.n_hopper, 0.0F, 0.05F);
    service.n_hopper_bin = kit.add_bin(service.n_hopper, 0.0F, kNHopperCapacityKg, JPH::Vec3::sZero(),
                                       kit::LeverIndex{}, 0.0F, 0.0F, 0.0F);
    service.n_chain = kit.add_chain(service.n_hopper, JPH::Vec3(0.0F, -kNHopperFloorHalf, 0.0F), kUpperTop,
                                    kNChainKgPerM, kNChainLength);

    // The hoist rope: the hopper's bail over its sheave, across the head to
    // the sheave over the cage, down to the cage's eye, made fast.
    const JPH::RVec3 bail = JPH::RVec3(kNHopperX, kNHopperY, kNZ) + JPH::RVec3(kNBailLocal);
    const JPH::RVec3 f1(kNHopperX, kNSheaveY, bail.GetZ());
    const JPH::RVec3 eye = JPH::RVec3(kNX, kNDeck, kNZ) + JPH::RVec3(kNEyeLocal);
    const JPH::RVec3 f2(eye.GetX(), kNSheaveY, eye.GetZ());
    const float length =
        static_cast<float>(JPH::Vec3(bail - f1).Length()) + static_cast<float>(JPH::Vec3(eye - f2).Length());
    service.n_rope = kit.add_rope(service.n_hopper, kNBailLocal, f1, service.n_cage, kNEyeLocal, f2, 1.0F, length,
                                  0.0F);
}

// ---- C5, the cooling plant: a climb from the 706 deck to the 728 deck --------------
// After stage N, a climb again, as the owner laid the route out. Over the
// footprint's north-east, on columns from the 662 deck: a plant floor off the
// 706 deck with a pipe manifold across it to vault and a duct bank over it to
// crawl under; a tank to mantle; its standpipe up to a platform; a sprint and
// a jump over a 6.5 m gap, past any walking jump's 6.17 m, to a second; a hang
// up, and another; a second standpipe; a mantle over the 728 deck's north
// girder. A miss from the gap or the hangs lands on a platform or the plant
// floor, under the 20.4 m a body survives. One static body, as C4's, but the
// manifold: a vault lands on another body than the one it goes over.
constexpr float kC5Floor = kN706Top;            // level with the 706 deck
constexpr float kC5FloorX0 = kUpperEast;
constexpr float kC5FloorX1 = 9.0F;
constexpr float kC5ManifoldX0 = 0.0F;           // vault 1.0 over a 0.5 m manifold
constexpr float kC5ManifoldX1 = 0.5F;
constexpr float kC5DuctX0 = 3.0F;               // crawl 2 m, 1.45 m under the ducts
constexpr float kC5DuctX1 = 5.0F;
constexpr float kC5Tank = 707.7F;               // mantle 1.6
constexpr float kC5TankX1 = 11.5F;
constexpr float kC5PlatformZ0 = -144.2F;        // the south face of the platforms over the tank
constexpr float kC5Upper = 714.0F;              // the standpipe 6.3; the gap 6.5
constexpr float kC5GapX1 = 8.0F;                // the gap, from P1's west edge
constexpr float kC5GapX0 = 1.5F;                // to P2's east edge
constexpr float kC5Hang1 = 717.2F;              // hang 3.2
constexpr float kC5Hang2 = 720.4F;              // hang 3.2
constexpr float kC5Top = 726.5F;                // the second standpipe 6.1; mantle 1.6
constexpr float kC5North = -138.25F;            // the platforms' north edges, by the face
constexpr float kC5StopWall = 1.2F;             // where the jump's landing runs out
constexpr float kC5LandingSlab = 0.4F;          // under the 0.45 m of face a hang needs
// The 728 deck: over the footprint's east half south of the plant, its north
// edge a girder for the last mantle; the backup ladder up through its hatch.
constexpr float kD728Bottom = 727.6F;
constexpr float kD728Top = 728.1F;
constexpr float kD728North = -142.2F;
constexpr float kD728GirderBottom = 727.3F;
constexpr float kD728East = kHalf;
constexpr float kD728HatchX0 = -1.9F;
constexpr float kD728HatchX1 = -0.5F;
constexpr float kD728HatchZ0 = -148.6F;
constexpr float kD728HatchZ1 = -146.4F;
// The top platform's south edge, 0.2 m short of the 728 deck's girder.
constexpr float kC5TopSouth = kD728North + 0.2F;

void c5_column(std::vector<Part> &parts, const float x, const float z, const float bottom, const float top) {
    parts.push_back(span({x - kC4Column, bottom, z - kC4Column}, {x + kC4Column, top, z + kC4Column}, Material::Rust));
}

void build_c5_frame(std::vector<Part> &frame) {
    const float z0 = kWellZ - kHalf;
    // Columns: at the east face from the 662 deck; at the west edge from the
    // 706 deck and the plant floor.
    for (const float z : {z0 + 0.4F, kWellZ - 2.0F, kC5FloorZ1 - 2.0F}) {
        c5_column(frame, kD728East - 0.2F, z, kDeckTop, kD728Bottom);
    }
    for (const float z : {z0 + 0.4F, kWellZ - 2.5F}) {
        c5_column(frame, kUpperEast - 0.65F, z, kN706Top, kD728Bottom);
    }
    c5_column(frame, kUpperEast + 0.4F, kC5FloorZ1 - 2.0F, kC5Floor, kD728Bottom);
    const auto deck = [&](const float x0, const float x1, const float za, const float zb) {
        frame.push_back(span({x0, kD728Bottom, za}, {x1, kD728Top, zb}, Material::Concrete));
    };
    deck(kUpperEast, kD728HatchX0, z0, kD728North);
    deck(kD728HatchX0, kD728HatchX1, z0, kD728HatchZ0);
    deck(kD728HatchX0, kD728HatchX1, kD728HatchZ1, kD728North);
    deck(kD728HatchX1, kD728East, z0, kD728North);
    frame.push_back(span({kUpperEast, kD728GirderBottom, kD728North - 0.3F}, {kD728East, kD728Bottom, kD728North},
                         Material::Yellow));
    // Parapets but where the last mantle comes over the girder.
    const auto parapet = [&](const float x0, const float za, const float x1, const float zb) {
        frame.push_back(span({x0, kD728Top, za}, {x1, kD728Top + kC4Parapet, zb}, Material::Concrete));
    };
    parapet(kD728East - kC4ParapetWidth, z0, kD728East, kD728North);
    parapet(kUpperEast, z0, kD728East, z0 + kC4ParapetWidth);
    parapet(kUpperEast, z0, kUpperEast + kC4ParapetWidth, kD728North);
    parapet(kUpperEast, kD728North - kC4ParapetWidth, 7.0F, kD728North);
    parapet(kC5TankX1, kD728North - kC4ParapetWidth, kD728East, kD728North);
}

void build_c5(kit::Kit &kit) {
    std::vector<Part> c5;
    // 1. The plant floor off the 706 deck, railed where it meets the air, on
    // columns from the 662 deck.
    c5.push_back(span({kC5FloorX0, kC5Floor - 0.5F, kC5FloorZ0}, {kC5FloorX1, kC5Floor, kC5FloorZ1}, Material::Concrete));
    c4_parapet(c5, kC5FloorX0, kC5FloorZ0, kC5FloorX1, kC5FloorZ0 + kC4ParapetWidth, kC5Floor);
    c4_parapet(c5, kC5FloorX0, kC5FloorZ1 - kC4ParapetWidth, kC5FloorX1, kC5FloorZ1, kC5Floor);
    for (const float x : {-1.6F, 3.8F, 8.4F}) {
        for (const float z : {kC5FloorZ0 + 0.4F, kC5FloorZ1 - 0.4F}) {
            c5_column(c5, x, z, kDeckTop, kC5Floor - 0.5F);
        }
    }
    // 2. Past the manifold (its own body), a duct bank 1.45 m over the floor,
    // spanning it over its parapets: under it on hands and knees.
    c5.push_back(span({kC5DuctX0, kC5Floor + 1.45F, kC5FloorZ0}, {kC5DuctX1, kC5Floor + 2.5F, kC5FloorZ1},
                      Material::Galvanised));
    // Railed at its ends, which meet the air: dropped onto from the platform
    // over it, a body walks off its sides onto the floor, not off its ends.
    c4_parapet(c5, kC5DuctX0, kC5FloorZ0, kC5DuctX1, kC5FloorZ0 + kC4ParapetWidth, kC5Floor + 2.5F);
    c4_parapet(c5, kC5DuctX0, kC5FloorZ1 - kC4ParapetWidth, kC5DuctX1, kC5FloorZ1, kC5Floor + 2.5F);
    // 3. The tank across the floor's east end, on legs, railed round its top
    // but on the floor's side; it runs on under the platform over it.
    c5.push_back(span({kC5FloorX1, 700.0F, kC5FloorZ0}, {kC5TankX1, kC5Tank, kC5FloorZ1}, Material::Galvanised));
    c4_parapet(c5, kC5FloorX1, kC5FloorZ0, kC5TankX1, kC5FloorZ0 + kC4ParapetWidth, kC5Tank);
    c4_parapet(c5, kC5FloorX1, kC5FloorZ1 - kC4ParapetWidth, kC5TankX1, kC5FloorZ1, kC5Tank);
    c4_parapet(c5, kC5TankX1 - kC4ParapetWidth, kC5FloorZ0, kC5TankX1, kC5FloorZ1, kC5Tank);
    for (const float x : {9.4F, 11.1F}) {
        for (const float z : {kC5FloorZ0 + 0.4F, kC5FloorZ1 - 0.4F}) {
            c5_column(c5, x, z, kDeckTop, 700.0F);
        }
    }
    // 4. Its standpipe, 0.06 m off the face of the platform over it: a hold
    // from the tank's top to 0.2 m under the platform's.
    c5.push_back(span({10.175F, kC5Tank, kC5PlatformZ0 - 0.2F}, {10.325F, kC5Upper - 0.2F, kC5PlatformZ0 - 0.06F},
                      Material::Yellow));
    // 5. Either side of the gap, platforms level at 714.0, railed to the
    // faces. The far one is a slab too thin to hang from, so a jump short of
    // it falls to the plant floor; where the jump lands, a wall to run out
    // against.
    c5.push_back(span({kC5GapX1, kC5Upper - 1.2F, kC5PlatformZ0}, {kC5TankX1, kC5Upper, kC5FloorZ1}, Material::Rust));
    c4_parapet(c5, kC5TankX1 - kC4ParapetWidth, kC5PlatformZ0, kC5TankX1, kC5FloorZ1, kC5Upper);
    c4_parapet(c5, kC5GapX1, kC5FloorZ1 - kC4ParapetWidth, kC5TankX1, kC5FloorZ1, kC5Upper);
    c5.push_back(span({kC5FloorX0, kC5Upper - kC5LandingSlab, kC5PlatformZ0}, {kC5GapX0, kC5Upper, kC5FloorZ1},
                      Material::Rust));
    c5.push_back(span({kC5FloorX0, kC5Upper, kC5PlatformZ0}, {kC5FloorX0 + kC4ParapetWidth, kC5Upper + kC5StopWall,
                                                             kC5FloorZ1},
                      Material::Concrete));
    c4_parapet(c5, kC5FloorX0, kC5FloorZ1 - kC4ParapetWidth, kC5GapX0, kC5FloorZ1, kC5Upper);
    c5_column(c5, 8.4F, kC5FloorZ1 - 0.4F, kC5Floor, kC5Upper - 1.2F);
    c5_column(c5, 11.1F, kC5FloorZ1 - 0.4F, kC5Tank, kC5Upper - 1.2F);
    c5_column(c5, -1.6F, kC5FloorZ1 - 0.4F, kC5Floor, kC5Upper - kC5LandingSlab);
    c5_column(c5, 1.1F, kC5PlatformZ0 + 0.4F, kC5Floor, kC5Upper - kC5LandingSlab);
    // 6. North of the landing, over nothing but the 662 deck 55 m down, a
    // platform 3.2 m up to hang from, railed on its west edge, where a step
    // off would land beside the 706 deck's hatch and slide into it. East of
    // that, a block 3.2 m higher standing on the platform's level, so its face
    // closes the platform's east edge: nothing walks out under it. Its slab
    // reaches as far south as the top platform on its east edge: a body
    // stepping off the top platform's west edge anywhere lands on it, 6.1 m
    // down, never 20.4 m onto the plant floor.
    c5.push_back(span({kC5FloorX0, kC5Hang1 - 1.35F, kC5FloorZ1}, {3.0F, kC5Hang1, kC5North}, Material::Rust));
    c4_parapet(c5, kC5FloorX0, kC5North - kC4ParapetWidth, 3.0F, kC5North, kC5Hang1);
    c4_parapet(c5, kC5FloorX0, kC5FloorZ1, kC5FloorX0 + kC4ParapetWidth, kC5North - kC4ParapetWidth, kC5Hang1);
    c5.push_back(span({3.0F, kC5Hang1, kC5FloorZ1}, {7.0F, kC5Hang2, kC5North}, Material::Rust));
    c5.push_back(span({3.0F, kC5Hang2 - 1.35F, kC5TopSouth}, {7.0F, kC5Hang2, kC5FloorZ1}, Material::Rust));
    c4_parapet(c5, 3.0F, kC5North - kC4ParapetWidth, 7.0F, kC5North, kC5Hang2);
    // 7. The second standpipe up the face of the top platform, from the one
    // below it; the top platform a block down to that one's level, its face
    // closing that one's east edge as the one below's is closed.
    c5.push_back(span({6.80F, kC5Hang2, -139.70F}, {6.94F, kC5Top - 0.2F, -139.56F}, Material::Yellow));
    c5.push_back(span({7.0F, kC5Hang2, kC5TopSouth}, {kC5TankX1, kC5Top, kC5North}, Material::Rust));
    c4_parapet(c5, 7.0F, kC5North - kC4ParapetWidth, kC5TankX1, kC5North, kC5Top);
    c4_parapet(c5, kC5TankX1 - kC4ParapetWidth, kD728North + 0.2F, kC5TankX1, kC5North, kC5Top);
    for (const float x : {-1.6F, 2.6F, 3.4F, 6.6F, 7.4F, 11.1F}) {
        const float top = x < 3.0F ? kC5Hang1 - 1.35F : (x < 7.0F ? kC5Hang1 : kC5Hang2);
        c5_column(c5, x, kC5North - 0.35F, kDeckTop, top);
    }
    (void)kit.add_body(Sim::kServiceC5EntityId, c5, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);

    // The manifold: a bundle of pipes across the plant floor, 1.0 m high and
    // 0.5 m deep, parapet to parapet.
    (void)kit.add_body(Sim::kServiceC5ManifoldEntityId,
                       {span({kC5ManifoldX0, kC5Floor, kC5FloorZ0 + kC4ParapetWidth},
                             {kC5ManifoldX1, kC5Floor + 1.0F, kC5FloorZ1 - kC4ParapetWidth}, Material::Steel)},
                       JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

// ---- the climbing route, no lift --------------------------------------------------
// The backup: ladders from TP-640 up through the 662 deck's hatch, and from
// the 662 deck up through the 684 deck's.
void build_climbing_route(kit::Kit &kit) {
    std::vector<Part> route;
    ladder(route, 0.5F * (kHatchX0 + kHatchX1), kHatchZ1 - 0.15F, true, kPlateTop, kDeckTop);
    ladder(route, 0.5F * (kUpperHatchX0 + kUpperHatchX1), kUpperHatchZ1 - 0.15F, true, kDeckTop, kUpperTop);
    // Up the 706 deck's hatch facing south, onto the open deck: its hatch fills
    // the strip's width between the parapets.
    ladder(route, 0.5F * (kN706HatchX0 + kN706HatchX1), kN706HatchZ0 + 0.15F, true, kUpperTop, kN706Top);
    ladder(route, 0.5F * (kD728HatchX0 + kD728HatchX1), kD728HatchZ0 + 0.15F, true, kC5Floor, kD728Top);
    (void)kit.add_body(Sim::kServiceRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace

void build_midstack_service(kit::Kit &kit, MidstackService &service) {
    std::vector<Part> frame;
    build_frame(frame);
    build_upper_deck(frame);
    build_stage_m(kit, service, frame);
    build_stage_n_frame(frame);
    build_c5_frame(frame);
    (void)kit.add_body(Sim::kServiceFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    build_c4(kit);
    build_climbing_route(kit);
    build_stage_n(kit, service);
    build_c5(kit);
}

} // namespace scraperx::sim::bands
