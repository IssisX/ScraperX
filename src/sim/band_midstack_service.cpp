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

// ---- the climbing route, no lift --------------------------------------------------
// The backup: ladders from TP-640 up through the 662 deck's hatch, and from
// the 662 deck up through the 684 deck's.
void build_climbing_route(kit::Kit &kit) {
    std::vector<Part> route;
    ladder(route, 0.5F * (kHatchX0 + kHatchX1), kHatchZ1 - 0.15F, true, kPlateTop, kDeckTop);
    ladder(route, 0.5F * (kUpperHatchX0 + kUpperHatchX1), kUpperHatchZ1 - 0.15F, true, kDeckTop, kUpperTop);
    (void)kit.add_body(Sim::kServiceRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace

void build_midstack_service(kit::Kit &kit, MidstackService &service) {
    std::vector<Part> frame;
    build_frame(frame);
    build_upper_deck(frame);
    build_stage_m(kit, service, frame);
    (void)kit.add_body(Sim::kServiceFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    build_c4(kit);
    build_climbing_route(kit);
}

} // namespace scraperx::sim::bands
