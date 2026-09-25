#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>
#include <vector>

// AS-008, the Plate Shop (Atlas band B04, 340 -> 484 m). Contract:
// 03_EXECUTION/ASCENT/AS-008_PLATE_SHOP.md. Three stages up the well's west
// half, each on collapse or a lever: G, a netted scaffold tower slumping
// down its shaft; H, a transfer girder tipping as a plate trolley rolls past
// its fulcrum, hauling a four-part purchase; I, a domino beam tripping a
// 20 t monolith whose fall hauls another. Above 418 m the frame is built
// here, on the rings' taper.

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;
using Sim = Simulation;

Part box(const JPH::Vec3 half, const JPH::Vec3 center, const Material material) {
    return {half, center, JPH::Quat::sIdentity(), material};
}

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (high + low), material);
}

// The frame's rings: the ring at 330 + 22 k has inner half-size
// 14.72 - 0.91 k about (0, -150), and is 4 m wide.
constexpr float kWellZ = -150.0F;
[[nodiscard]] float ring_inner(const float height) {
    return 14.72F - 0.91F * (height - 330.0F) / 22.0F;
}

// ---- the frame above 418 m ----------------------------------------------------
void build_upper_frame(std::vector<Part> &frame, const float top) {
    for (float h = 418.0F; h <= top + 0.01F; h += 22.0F) {
        const float s = ring_inner(h);
        const float outer = s + 4.0F;
        const float y0 = h - 0.25F;
        const float y1 = h + 0.25F;
        frame.push_back(span({-outer, y0, kWellZ + s}, {outer, y1, kWellZ + outer}, Material::Concrete));
        frame.push_back(span({-outer, y0, kWellZ - outer}, {outer, y1, kWellZ - s}, Material::Concrete));
        frame.push_back(span({s, y0, kWellZ - s}, {outer, y1, kWellZ + s}, Material::Concrete));
        frame.push_back(span({-outer, y0, kWellZ - s}, {-s, y1, kWellZ + s}, Material::Concrete));
    }
    // Corner columns in 11 m lifts on the rings' outer corners.
    for (float y = 418.0F; y < top - 0.01F; y += 11.0F) {
        const float mid = y + 5.5F;
        const float corner = ring_inner(mid) + 4.0F;
        const float half = 0.47F - 0.01F * (y - 418.0F) / 11.0F;
        for (const float sx : {-1.0F, 1.0F}) {
            for (const float sz : {-1.0F, 1.0F}) {
                frame.push_back(box(JPH::Vec3(half, 5.5F, half),
                                    JPH::Vec3(sx * corner, mid, kWellZ + sz * corner), Material::Rust));
            }
        }
    }
}

// A ladder at (x, z) from `bottom` to `top`: rails and rungs 0.3 m apart,
// the rungs across x (`across_x`) or across z.
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

// Handles, levers and platforms, as the other bands build them.
constexpr float kHandleHalfY = 0.04F;
constexpr float kHandleDrop = 0.75F;
constexpr float kHandleMassKg = 0.5F;       // a lanyard's T-handle
constexpr float kLeverArm = 1.0F;
constexpr float kLeverMassKg = 60.0F;
constexpr float kLeverRelease = 0.5F;
constexpr float kPlatformHalf = 1.3F;

kit::BodyIndex add_handle(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 sheave) {
    const kit::BodyIndex handle = kit.add_body(
        entity, {box(JPH::Vec3(0.22F, kHandleHalfY, 0.04F), JPH::Vec3::sZero(), Material::Yellow)},
        sheave - JPH::RVec3(0.0, kHandleDrop + kHandleHalfY, 0.0), JPH::Quat::sIdentity(), kHandleMassKg, 0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kHandleHalfY, 0.0F));
    kit.set_damping(handle, 1.5F, 1.5F);
    return handle;
}

// A platform about its deck's top centre, railed along its north and south
// sides: in from the east or west, out the same way.
std::vector<Part> platform_parts() {
    std::vector<Part> parts{
        box(JPH::Vec3(kPlatformHalf, 0.1F, kPlatformHalf), JPH::Vec3(0.0F, -0.1F, 0.0F), Material::Galvanised)};
    for (const float side : {-1.0F, 1.0F}) {
        const float z = side * (kPlatformHalf - 0.03F);
        parts.push_back(box(JPH::Vec3(0.03F, 0.5F, 0.03F), JPH::Vec3(kPlatformHalf - 0.03F, 0.5F, z),
                            Material::Yellow));
        parts.push_back(box(JPH::Vec3(0.03F, 0.5F, 0.03F), JPH::Vec3(-(kPlatformHalf - 0.03F), 0.5F, z),
                            Material::Yellow));
        parts.push_back(box(JPH::Vec3(kPlatformHalf, 0.03F, 0.03F), JPH::Vec3(0.0F, 1.0F, z), Material::Yellow));
    }
    return parts;
}

// A pin in a socket: a short bar resting on a ledge between two cheeks,
// its axis along z, carried or pulled out by its north end.
kit::BodyIndex add_pin(kit::Kit &kit, std::vector<Part> &frame, const std::uint64_t entity,
                       const JPH::RVec3 at) {
    const JPH::Vec3 c(static_cast<float>(at.GetX()), static_cast<float>(at.GetY()),
                      static_cast<float>(at.GetZ()));
    frame.push_back(box(JPH::Vec3(0.05F, 0.02F, 0.3F), c - JPH::Vec3(0.0F, 0.055F, 0.0F), Material::Steel));
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(box(JPH::Vec3(0.01F, 0.05F, 0.3F), c + JPH::Vec3(side * 0.045F, -0.01F, 0.0F),
                            Material::Steel));
    }
    const kit::BodyIndex pin = kit.add_body(
        entity, {box(JPH::Vec3(0.03F, 0.03F, 0.25F), JPH::Vec3::sZero(), Material::Hazard)}, at,
        JPH::Quat::sIdentity(), 6.0F, 1.0F);
    kit.set_carry(pin, kit::CarryKind::Load, JPH::Vec3(0.0F, 0.03F, 0.0F));
    return pin;
}

// ---- Stage G ---------------------------------------------------------------
constexpr float kGX = -11.3F;
constexpr float kGZ = kWellZ;
constexpr float kGDeckTop = 340.45F;        // on TP-340
constexpr float kGTravel = 34.0F;           // to 374.45, by the 374 ring
constexpr float kGMassKg = 800.0F;
constexpr float kTowerX = -8.0F;
constexpr float kTowerHalf = 1.5F;
constexpr float kTowerHalfY = 17.0F;
constexpr float kTowerFoot = 374.25F;
constexpr float kTowerMassKg = 3000.0F;
constexpr float kHeadY = 409.5F;
const JPH::Vec3 kGEyeLocal(1.0F, 1.1F, 0.0F);
const JPH::RVec3 kGCleat(-9.7, 341.4, -152.0);
const JPH::RVec3 kGPinSeat(-6.25, 374.55, kGZ);
const JPH::RVec3 kGLanyard1(-6.25, 374.55, -148.2);
const JPH::RVec3 kGLanyard2(kGX, 342.8, -148.4);

void build_stage_g(kit::Kit &kit, PlateShop &shop, std::vector<Part> &frame) {
    shop.g_platform = kit.add_body(Sim::kShopGPlatformEntityId, platform_parts(),
                                   JPH::RVec3(kGX, kGDeckTop, kGZ), JPH::Quat::sIdentity(), kGMassKg, 0.8F);
    shop.g_platform_guide = kit.add_guide(shop.g_platform, JPH::Vec3::sAxisY(), 0.0F, kGTravel, 2.5F,
                                          40000.0F, 1.0F);
    shop.g_eye = kit.add_anchor(shop.g_platform, kGEyeLocal, 1.2F);

    // The tower: four corner posts and a ledger round every 2 m, 3 t, on a
    // guide in its shaft of four rails.
    std::vector<Part> tower;
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            tower.push_back(box(JPH::Vec3(0.06F, kTowerHalfY, 0.06F),
                                JPH::Vec3(sx * (kTowerHalf - 0.06F), 0.0F, sz * (kTowerHalf - 0.06F)),
                                Material::Galvanised));
        }
    }
    for (float y = -kTowerHalfY + 1.0F; y <= kTowerHalfY - 0.99F; y += 2.0F) {
        for (const float side : {-1.0F, 1.0F}) {
            tower.push_back(box(JPH::Vec3(kTowerHalf, 0.03F, 0.03F),
                                JPH::Vec3(0.0F, y, side * (kTowerHalf - 0.06F)), Material::Yellow));
            tower.push_back(box(JPH::Vec3(0.03F, 0.03F, kTowerHalf),
                                JPH::Vec3(side * (kTowerHalf - 0.06F), y, 0.0F), Material::Yellow));
        }
    }
    shop.g_tower = kit.add_body(Sim::kShopGTowerEntityId, tower,
                                JPH::RVec3(kTowerX, kTowerFoot + kTowerHalfY, kGZ), JPH::Quat::sIdentity(),
                                kTowerMassKg, 0.6F);
    shop.g_tower_guide = kit.add_guide(shop.g_tower, JPH::Vec3::sAxisY(), -kGTravel, 0.0F, 0.0F, 0.0F, 0.0F);
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            const float x = kTowerX + sx * (kTowerHalf + 0.1F);
            const float z = kGZ + sz * (kTowerHalf + 0.1F);
            frame.push_back(span({x - 0.05F, 340.25F, z - 0.05F}, {x + 0.05F, kHeadY - 0.3F, z + 0.05F},
                                 Material::Steel));
        }
    }
    frame.push_back(span({kGX + 0.6F, kHeadY + 0.25F, kGZ - 0.15F}, {kTowerX + 1.7F, kHeadY + 0.55F, kGZ + 0.15F},
                         Material::Steel));

    // The prop's pin, and its lanyard down to a handle over the platform's
    // north rail.
    shop.g_pin = add_pin(kit, frame, Sim::kShopGPinEntityId, kGPinSeat);
    frame.push_back(span({kTowerX + kTowerHalf + 0.05F, 374.34F, kGZ - kTowerHalf - 0.15F},
                         {kTowerX + kTowerHalf + 0.3F, 374.44F, kGZ + kTowerHalf + 0.15F}, Material::Steel));
    shop.g_catch = kit.add_pin_catch(shop.g_tower, shop.g_pin, 0.15F, 0.05F);
    shop.g_handle = add_handle(kit, Sim::kShopGHandleEntityId, kGLanyard2);
    (void)kit.add_trip_line(shop.g_pin, JPH::Vec3(0.0F, 0.0F, 0.25F), shop.g_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kGLanyard1, kGLanyard2);

    // The rope, from the tower's head over two sheaves to its shackle, made
    // fast on a cleat by the platform as found.
    frame.push_back(span({static_cast<float>(kGCleat.GetX()) - 0.06F, 340.25F, static_cast<float>(kGCleat.GetZ()) - 0.06F},
                         {static_cast<float>(kGCleat.GetX()) + 0.06F, static_cast<float>(kGCleat.GetY()) + 0.1F,
                          static_cast<float>(kGCleat.GetZ()) + 0.06F},
                         Material::Rust));
    shop.g_shackle = kit.add_body(Sim::kShopGShackleEntityId,
                                  {box(JPH::Vec3(0.08F, 0.06F, 0.05F), JPH::Vec3::sZero(), Material::Yellow)},
                                  kGCleat + JPH::RVec3(0.0, 0.3, 0.0), JPH::Quat::sIdentity(), 6.0F, 0.8F);
    kit.set_carry(shop.g_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.06F, 0.0F));
    const JPH::RVec3 f1(kTowerX, kHeadY, kGZ);
    const JPH::RVec3 f2 = JPH::RVec3(kGX, 0.0, kGZ) + JPH::RVec3(kGEyeLocal.GetX(), kHeadY, kGEyeLocal.GetZ());
    const float first = kHeadY - (kTowerFoot + 2.0F * kTowerHalfY);
    const float to_cleat = static_cast<float>(JPH::Vec3(kGCleat - f2).Length());
    shop.g_rope = kit.add_rope(shop.g_tower, JPH::Vec3(0.0F, kTowerHalfY, 0.0F), f1, shop.g_shackle,
                               JPH::Vec3::sZero(), f2, 1.0F, first + to_cleat + 0.03F, 0.0F);
}

// ---- Stage H ---------------------------------------------------------------
constexpr float kHX = -9.4F;
constexpr float kHZ = -145.6F;
constexpr float kHDeckTop = 374.25F;
constexpr float kHTravel = 44.0F;           // to 418.25
constexpr float kHMassKg = 800.0F;
const JPH::Vec3 kHEyeLocal(1.0F, 1.1F, -1.0F);
constexpr float kGirderX = -5.0F;
const JPH::RVec3 kGirderPivot(kGirderX, 379.5, -135.5);  // its underside clears the ring's edge to its stop
constexpr float kGirderInboard = 2.0F;    // clear of the north face's braces
constexpr float kGirderOutboard = 13.0F;
constexpr float kGirderHalfW = 0.3F;
constexpr float kGirderHalfH = 0.6F;
constexpr float kGirderMassKg = 4000.0F;
constexpr float kGirderTilt = 1.15F;        // rad, outboard down, to its stop
constexpr float kRampRise = 0.8F;           // the trolley's track falls this far outboard
constexpr float kTrolleyMassKg = 14000.0F;
constexpr float kHPurchase = 0.25F;         // a four-part purchase
// The girder's tail is tied down to the ring by a drop pin under its inboard
// arm.
const JPH::RVec3 kGirderPinSeat(kGirderX + 0.6, 374.45, -133.9);
// The chock's lever stands on a post east of the track, beside the trolley's
// front wheels; its pawl holds the trolley until the lever is thrown east.
const JPH::RVec3 kChockPivot(kGirderX + 1.15, 381.05, -135.8);
constexpr float kChockArm = 0.7F;
const JPH::RVec3 kChockLanyard1(kGirderX + 1.8, 381.8, -135.8);
const JPH::RVec3 kChockLanyard2(kHX, 376.6, -144.0);

void build_stage_h(kit::Kit &kit, PlateShop &shop, std::vector<Part> &frame) {
    shop.h_platform = kit.add_body(Sim::kShopHPlatformEntityId, platform_parts(),
                                   JPH::RVec3(kHX, kHDeckTop, kHZ), JPH::Quat::sIdentity(), kHMassKg, 0.8F);
    shop.h_platform_guide = kit.add_guide(shop.h_platform, JPH::Vec3::sAxisY(), 0.0F, kHTravel, 2.5F,
                                          40000.0F, 1.0F);
    // The gangway from the 374 ring's west band.
    frame.push_back(span({-ring_inner(374.0F) - 0.05F, 374.05F, kHZ - 0.15F},
                         {kHX - kPlatformHalf - 0.05F, kHDeckTop, kHZ + 0.15F}, Material::Timber));

    // The girder, built level about its pivot: inboard north, outboard
    // south, a track on top falling outboard, an end stop at the tip.
    const float length = kGirderInboard + kGirderOutboard;
    const float mid = 0.5F * (kGirderInboard - kGirderOutboard);   // centre, from the pivot, along +z
    std::vector<Part> girder{
        box(JPH::Vec3(kGirderHalfW, kGirderHalfH, 0.5F * length), JPH::Vec3(0.0F, 0.0F, mid), Material::Rust)};
    const float slope = std::atan2(kRampRise, length);
    const JPH::Quat ramp_turn = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), -slope);
    girder.push_back({JPH::Vec3(0.9F, 0.05F, 0.5F * length), JPH::Vec3(0.0F, kGirderHalfH + 0.05F + 0.5F * kRampRise, mid),
                      ramp_turn, Material::Steel});
    girder.push_back(box(JPH::Vec3(0.9F, 0.6F, 0.1F),
                         JPH::Vec3(0.0F, kGirderHalfH + 0.6F, -kGirderOutboard + 0.1F), Material::Hazard));
    for (const float side : {-1.0F, 1.0F}) {
        girder.push_back(box(JPH::Vec3(0.04F, 0.04F, 0.5F * length),
                             JPH::Vec3(side * (kGirderHalfW + 0.2F), 0.0F, mid), Material::Yellow));
    }
    shop.h_girder = kit.add_body(Sim::kShopHGirderEntityId, girder, kGirderPivot, JPH::Quat::sIdentity(),
                                 kGirderMassKg, 0.05F);
    shop.h_girder_hinge = kit.add_lever(shop.h_girder, kGirderPivot, -JPH::Vec3::sAxisX(),
                                        -JPH::Vec3::sAxisZ(), 0.0F, kGirderTilt);
    // The trestle under the pivot, on the 374 ring's north band.
    // The pivot's cheeks stand either side of the track, and a post under the
    // axle stops short of the girder's underside through its whole tip.
    const float pz = static_cast<float>(kGirderPivot.GetZ());
    frame.push_back(span({kGirderX - 0.25F, 374.25F, pz - 0.15F}, {kGirderX + 0.25F, 377.5F, pz + 0.15F}, Material::Steel));
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kGirderX + side * 1.1F - 0.05F, 374.25F, pz - 0.15F},
                             {kGirderX + side * 1.1F + 0.05F, 379.8F, pz + 0.15F}, Material::Steel));
    }

    // The trolley on the track's inboard end, held by its chock.
    const float track_z = static_cast<float>(kGirderPivot.GetZ()) + 0.9F;
    const float track_y = static_cast<float>(kGirderPivot.GetY()) + kGirderHalfH + 0.1F +
                          0.5F * kRampRise + (track_z - static_cast<float>(kGirderPivot.GetZ()) - mid) * std::tan(slope);
    shop.h_trolley = kit.add_body(
        Sim::kShopHTrolleyEntityId,
        {box(JPH::Vec3(0.8F, 0.5F, 1.1F), JPH::Vec3::sZero(), Material::Steel),
         box(JPH::Vec3(0.75F, 0.25F, 1.0F), JPH::Vec3(0.0F, 0.75F, 0.0F), Material::Rust)},
        JPH::RVec3(kGirderX, track_y + 0.5F, track_z), ramp_turn, kTrolleyMassKg, 0.01F);
    shop.h_chock = kit.add_body(
        Sim::kShopHChockEntityId,
        {box(JPH::Vec3(0.04F, 0.5F * kChockArm, 0.04F), JPH::Vec3(-0.1F, 0.5F * kChockArm, 0.0F), Material::Hazard)},
        kChockPivot, JPH::Quat::sIdentity(), 12.0F, 0.6F);
    shop.h_chock_lever = kit.add_lever(shop.h_chock, kChockPivot, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisY(), 0.0F,
                                       1.2F);
    frame.push_back(span({static_cast<float>(kChockPivot.GetX()) - 0.06F, 374.25F,
                          static_cast<float>(kChockPivot.GetZ()) - 0.12F},
                         {static_cast<float>(kChockPivot.GetX()) + 0.06F, static_cast<float>(kChockPivot.GetY()) - 0.06F,
                          static_cast<float>(kChockPivot.GetZ()) + 0.12F},
                         Material::Steel));
    shop.h_trolley_catch = kit.add_catch(shop.h_trolley, shop.h_chock_lever, kLeverRelease, 0.05F, false);
    shop.h_handle = add_handle(kit, Sim::kShopHHandleEntityId, kChockLanyard2);
    (void)kit.add_trip_line(shop.h_chock, JPH::Vec3(-0.1F, kChockArm, 0.0F), shop.h_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kChockLanyard1, kChockLanyard2);

    shop.h_girder_pin = add_pin(kit, frame, Sim::kShopHGirderPinEntityId, kGirderPinSeat);
    shop.h_girder_catch = kit.add_pin_catch(shop.h_girder, shop.h_girder_pin, 0.15F, 0.05F);

    // The purchase: the girder's tip over a head sheave, across to one over
    // the platform's eye; made fast to the eye.
    const JPH::RVec3 tip = kGirderPivot + JPH::RVec3(0.0, 0.0, -kGirderOutboard + 0.3);
    const JPH::RVec3 f1(tip.GetX(), 421.5, tip.GetZ());
    const JPH::RVec3 eye = JPH::RVec3(kHX, kHDeckTop, kHZ) + JPH::RVec3(kHEyeLocal);
    const JPH::RVec3 f2(eye.GetX(), 421.5, eye.GetZ());
    const float length_rope = static_cast<float>(f1.GetY() - tip.GetY()) +
                              kHPurchase * static_cast<float>(f2.GetY() - eye.GetY());
    shop.h_rope = kit.add_rope(shop.h_girder, JPH::Vec3(0.0F, 0.0F, -kGirderOutboard + 0.3F), f1, shop.h_platform,
                               kHEyeLocal, f2, kHPurchase, length_rope, 0.0F);
    frame.push_back(span({static_cast<float>(f2.GetX()) - 0.1F, 421.7F, static_cast<float>(f1.GetZ()) - 0.1F},
                         {static_cast<float>(f1.GetX()) + 0.1F, 421.9F, static_cast<float>(f1.GetZ()) + 0.1F},
                         Material::Steel));
    frame.push_back(span({static_cast<float>(f2.GetX()) - 0.1F, 421.7F, static_cast<float>(f1.GetZ()) + 0.1F},
                         {static_cast<float>(f2.GetX()) + 0.1F, 421.9F, static_cast<float>(f2.GetZ()) + 0.1F},
                         Material::Steel));
}

// ---- Stage I ---------------------------------------------------------------
constexpr float kIX = -6.5F;
constexpr float kIZ = -145.6F;
constexpr float kIDeckTop = 418.25F;
constexpr float kITravel = 44.0F;           // to 462.25
constexpr float kIMassKg = 900.0F;
const JPH::Vec3 kIEyeLocal(1.0F, 1.1F, 0.0F);
const JPH::RVec3 kMonolithEdge(-2.0, 418.25, -137.5);   // the foot's south edge, its hinge
constexpr float kMonolithHalfY = 6.0F;
constexpr float kMonolithLean = 0.5236F;    // 30 deg south, as found
constexpr float kMonolithMassKg = 20000.0F;
constexpr float kMonolithFall = 1.0F;       // rad, to its stop: the rope pays out 11.1 m
const JPH::RVec3 kMonolithSheave(-2.0, 441.5, -140.2);
constexpr float kIPurchase = 0.25F;
const JPH::RVec3 kTripPivot(0.0, 419.5, -135.5);
const JPH::RVec3 kDominoFoot(2.5, 418.25, -135.5);    // its foot's west edge, its hinge
constexpr float kDominoHalfY = 2.5F;
constexpr float kDominoLean = 0.21F;        // west, toward the trip lever
const JPH::RVec3 kDominoPinSeat(2.9, 418.45, -135.1);
const JPH::RVec3 kDominoLanyard1(2.9, 418.45, -133.4);
const JPH::RVec3 kDominoLanyard2(kIX, 420.2, -144.0);

void build_stage_i(kit::Kit &kit, PlateShop &shop, std::vector<Part> &frame) {
    shop.i_cage = kit.add_body(Sim::kShopICageEntityId, platform_parts(), JPH::RVec3(kIX, kIDeckTop, kIZ),
                               JPH::Quat::sIdentity(), kIMassKg, 0.8F);
    shop.i_cage_guide = kit.add_guide(shop.i_cage, JPH::Vec3::sAxisY(), 0.0F, kITravel, 2.5F, 40000.0F, 1.0F);
    kit.set_dogs(shop.i_cage_guide, 0.05F);
    shop.i_eye = kit.add_anchor(shop.i_cage, kIEyeLocal, 1.2F);
    // The landing at the 462 ring's west band, and the band's way out: a
    // ladder up the 484 ring's inner face, reached from the parked cage.
    frame.push_back(span({-ring_inner(462.0F) - 0.05F, 462.05F, kIZ - 0.15F},
                         {kIX - kPlatformHalf - 0.05F, 462.25F, kIZ + 0.15F}, Material::Timber));
    ladder(frame, -ring_inner(484.0F) + 0.14F, kIZ, false, 462.25F, 484.25F);

    // The trip lever over the monolith's foot, its arm east toward the
    // domino; its counterweight, north of the domino's fall, holds the arm up
    // on its stop.
    shop.i_trip_body = kit.add_body(
        Sim::kShopITripEntityId,
        {box(JPH::Vec3(0.5F * kLeverArm, 0.05F, 0.05F), JPH::Vec3(0.5F * kLeverArm, 0.0F, 0.0F), Material::Hazard),
         box(JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(-0.35F, 0.0F, 0.35F), Material::Rust)},
        kTripPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    shop.i_trip = kit.add_lever(shop.i_trip_body, kTripPivot, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), 0.0F, 1.2F);
    frame.push_back(span({static_cast<float>(kTripPivot.GetX()) - 0.1F, 418.25F, -135.33F},
                         {static_cast<float>(kTripPivot.GetX()) + 0.1F, static_cast<float>(kTripPivot.GetY()) + 0.05F, -135.15F},
                         Material::Steel));

    // The monolith, leaning south on its foot hinge as found, caught.
    const JPH::Quat lean = JPH::Quat::sRotation(-JPH::Vec3::sAxisX(), kMonolithLean);
    const JPH::Vec3 up = lean * JPH::Vec3::sAxisY();
    shop.i_monolith = kit.add_body(
        Sim::kShopIMonolithEntityId,
        {box(JPH::Vec3(0.5F, kMonolithHalfY, 0.5F), JPH::Vec3::sZero(), Material::Concrete)},
        kMonolithEdge + JPH::RVec3(lean * JPH::Vec3(0.0F, kMonolithHalfY, 0.5F)), lean, kMonolithMassKg, 0.6F);
    shop.i_monolith_hinge =
        kit.add_lever(shop.i_monolith, kMonolithEdge, -JPH::Vec3::sAxisX(), up, 0.0F, kMonolithFall);
    shop.i_monolith_catch = kit.add_catch(shop.i_monolith, shop.i_trip, kLeverRelease, 0.05F, false);

    // The domino, leaning west toward the trip lever, pinned at its foot.
    const JPH::Quat domino_lean = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kDominoLean);
    const JPH::Vec3 domino_up = domino_lean * JPH::Vec3::sAxisY();
    shop.i_domino = kit.add_body(
        Sim::kShopIDominoEntityId,
        {box(JPH::Vec3(0.2F, kDominoHalfY, 0.15F), JPH::Vec3::sZero(), Material::Rust)},
        kDominoFoot + JPH::RVec3(domino_lean * JPH::Vec3(0.2F, kDominoHalfY, 0.0F)), domino_lean, 800.0F, 0.6F);
    shop.i_domino_hinge = kit.add_lever(shop.i_domino, kDominoFoot, JPH::Vec3::sAxisZ(), domino_up, 0.0F, 1.36F);
    shop.i_domino_pin = add_pin(kit, frame, Sim::kShopIDominoPinEntityId, kDominoPinSeat);
    shop.i_domino_catch = kit.add_pin_catch(shop.i_domino, shop.i_domino_pin, 0.15F, 0.05F);
    shop.i_handle = add_handle(kit, Sim::kShopIHandleEntityId, kDominoLanyard2);
    (void)kit.add_trip_line(shop.i_domino_pin, JPH::Vec3(0.0F, 0.0F, 0.25F), shop.i_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kDominoLanyard1, kDominoLanyard2);

    // The purchase: the monolith's top over a sheave on a gallows at the
    // 440 ring, across to one over the cage's eye; its shackle on the
    // cage's deck, loose.
    const JPH::RVec3 top = kMonolithEdge + JPH::RVec3(lean * JPH::Vec3(0.0F, 2.0F * kMonolithHalfY, 0.5F));
    const JPH::RVec3 eye = JPH::RVec3(kIX, kIDeckTop, kIZ) + JPH::RVec3(kIEyeLocal);
    const JPH::RVec3 f2(kIX, 464.5, kIZ);
    const float hauling = static_cast<float>(JPH::Vec3(eye - f2).Length()) + 0.08F;
    shop.i_shackle = kit.add_body(Sim::kShopIShackleEntityId,
                                  {box(JPH::Vec3(0.08F, 0.06F, 0.05F), JPH::Vec3::sZero(), Material::Yellow)},
                                  f2 - JPH::RVec3(0.0, hauling, 0.0), JPH::Quat::sIdentity(), 6.0F, 0.8F);
    kit.set_carry(shop.i_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.06F, 0.0F));
    const float rope_length =
        static_cast<float>(JPH::Vec3(top - kMonolithSheave).Length()) + kIPurchase * hauling;
    shop.i_rope = kit.add_rope(shop.i_monolith, JPH::Vec3(0.0F, kMonolithHalfY, 0.0F), kMonolithSheave,
                               shop.i_shackle, JPH::Vec3::sZero(), f2, kIPurchase, rope_length, 0.0F);
    // The gallows on the 440 ring for the monolith's sheave, and the head
    // beam over the cage.
    frame.push_back(span({-2.1F, 440.25F, -139.75F}, {-1.9F, 442.1F, -139.55F}, Material::Steel));
    frame.push_back(span({-2.1F, 441.9F, -140.35F}, {-1.9F, 442.1F, -139.55F}, Material::Steel));
    frame.push_back(span({static_cast<float>(f2.GetX()) - 0.4F, 464.7F, kIZ - 0.15F},
                         {static_cast<float>(f2.GetX()) + 0.4F, 464.9F, kIZ + 0.15F}, Material::Steel));
}

// ---- the climbing route, no lift ----------------------------------------------
// Up the well's east band at z = -158, one climb per ring gap, each facing the
// ring it tops out onto: a ladder on the 352 ring's face from TP-340; then on
// each ring an L of boards out into the well to a ladder on the next ring's
// inner face, which stands 0.91 m further in (the rings' taper).
constexpr float kRouteZ = -158.0F;

void build_climbing_route(kit::Kit &kit) {
    std::vector<Part> route;
    ladder(route, ring_inner(352.0F) - 0.14F, kRouteZ, false, 340.25F, 352.25F);
    for (float h = 352.0F; h < 483.9F; h += 22.0F) {
        const float s = ring_inner(h);
        const float y0 = h + 0.05F;
        const float y1 = h + 0.25F;
        route.push_back(span({s - 1.82F, y0, kRouteZ + 0.85F}, {s + 0.05F, y1, kRouteZ + 1.15F}, Material::Timber));
        route.push_back(span({s - 1.82F, y0, kRouteZ - 0.4F}, {s - 1.52F, y1, kRouteZ + 0.85F}, Material::Timber));
        ladder(route, ring_inner(h + 22.0F) - 0.14F, kRouteZ, false, y1, h + 22.25F);
    }
    (void)kit.add_body(Sim::kShopRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace

void build_plate_shop(kit::Kit &kit, PlateShop &shop) {
    std::vector<Part> upper;
    build_upper_frame(upper, 484.0F);
    (void)kit.add_body(Sim::kUpperFrameEntityId, upper, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    std::vector<Part> frame;
    build_stage_g(kit, shop, frame);
    build_stage_h(kit, shop, frame);
    build_stage_i(kit, shop, frame);
    build_climbing_route(kit);
    const kit::BodyIndex frame_body =
        kit.add_body(Sim::kShopFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    // G's rope is found made fast on its cleat.
    const kit::AnchorIndex cleat = kit.add_anchor(frame_body, JPH::Vec3(kGCleat), 1.2F);
    shop.g_cleat = cleat;
    (void)kit.hook(Sim::kShopGShackleEntityId, cleat);
}

} // namespace scraperx::sim::bands
