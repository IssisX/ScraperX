#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>
#include <vector>

// AS-009, the Facade Crane Stack (Atlas band B05, 484 -> 640 m). Contract:
// 03_EXECUTION/ASCENT/AS-009_FACADE_CRANE_STACK.md. The frame has narrowed to
// a 17 m well and the machines work on its faces: J, a runaway rail wagon
// dragging a facade traveler up its rails once the rail's missing joint is
// laid in; K, a retired tower crane's jib swinging down like a pendulum
// through a four-part purchase; L, a freight cart whose run down an incline
// drives a two-drum winch, set off by a drop weight -- the band's cascade --
// into TP-640.

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

JPH::Vec3 vec(const JPH::RVec3 v) {
    return JPH::Vec3(static_cast<float>(v.GetX()), static_cast<float>(v.GetY()), static_cast<float>(v.GetZ()));
}

// The frame's rings: the ring at 330 + 22 k has inner half-size
// 14.72 - 0.91 k about (0, -150), and is 4 m wide.
constexpr float kWellZ = -150.0F;
[[nodiscard]] float ring_inner(const float height) {
    return 14.72F - 0.91F * (height - 330.0F) / 22.0F;
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

// Handles, levers, pins and platforms, as AS-008 builds them.
constexpr float kHandleHalfY = 0.04F;
constexpr float kHandleDrop = 0.75F;
constexpr float kHandleMassKg = 0.5F;       // a lanyard's T-handle
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

// A platform about its deck's top centre, railed along two sides: along
// its east and west sides (in from the north or the south) or, with
// `rails_ns`, its north and south sides (in from the east or the west).
std::vector<Part> platform_parts(const bool rails_ns) {
    std::vector<Part> parts{
        box(JPH::Vec3(kPlatformHalf, 0.1F, kPlatformHalf), JPH::Vec3(0.0F, -0.1F, 0.0F), Material::Galvanised)};
    const float edge = kPlatformHalf - 0.03F;
    for (const float side : {-1.0F, 1.0F}) {
        for (const float end : {-1.0F, 1.0F}) {
            parts.push_back(box(JPH::Vec3(0.03F, 0.5F, 0.03F), JPH::Vec3(side * edge, 0.5F, end * edge),
                                Material::Yellow));
        }
        parts.push_back(rails_ns ? box(JPH::Vec3(kPlatformHalf, 0.03F, 0.03F), JPH::Vec3(0.0F, 1.0F, side * edge),
                                       Material::Yellow)
                                 : box(JPH::Vec3(0.03F, 0.03F, kPlatformHalf), JPH::Vec3(side * edge, 1.0F, 0.0F),
                                       Material::Yellow));
    }
    return parts;
}

// A pin in a socket: a short bar resting on a ledge between two cheeks, its
// axis along z, drawn out north by a lanyard on its north end.
kit::BodyIndex add_pin(kit::Kit &kit, std::vector<Part> &frame, const std::uint64_t entity,
                       const JPH::RVec3 at) {
    const JPH::Vec3 c = vec(at);
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

// A lever standing up from its pivot on a post, its weight a little west of
// the pivot holding it on its stop until its lanyard draws the top east
// (positive about -Z).
constexpr float kStandingArm = 0.7F;

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

// An inclined track of two rails on sleepers, in the frame of `turn` (local
// +X or +Z down the slope, as `along_x`), from travel `from` to `to` past
// `head`, `below` under the rolling body's centre.
void incline_track(std::vector<Part> &frame, const JPH::RVec3 head, const JPH::Quat turn, const bool along_x,
                   const float from, const float to, const float below) {
    const JPH::Vec3 down = turn * (along_x ? JPH::Vec3::sAxisX() : JPH::Vec3::sAxisZ());
    const JPH::Vec3 side = turn * (along_x ? JPH::Vec3::sAxisZ() : JPH::Vec3::sAxisX());
    const JPH::Vec3 under = turn * JPH::Vec3::sAxisY();
    const float half = 0.5F * (to - from);
    const JPH::Vec3 middle = vec(head) + down * (0.5F * (from + to)) - under * below;
    const JPH::Vec3 rail_half = along_x ? JPH::Vec3(half, 0.05F, 0.05F) : JPH::Vec3(0.05F, 0.05F, half);
    for (const float s : {-0.5F, 0.5F}) {
        frame.push_back({rail_half, middle + side * s, turn, Material::Rust});
    }
    const JPH::Vec3 sleeper_half = along_x ? JPH::Vec3(0.1F, 0.04F, 0.8F) : JPH::Vec3(0.8F, 0.04F, 0.1F);
    for (float t = from + 0.5F; t <= to - 0.49F; t += 1.0F) {
        frame.push_back({sleeper_half, vec(head) + down * t - under * (below + 0.09F), turn, Material::Timber});
    }
}

// ---- the frame above 484 m, and TP-640 -----------------------------------------
constexpr float kPlateBottom = 638.75F;
constexpr float kPlateTop = 640.25F;

// L's cab arrives flush in a hole in the plate; the route's last ladder
// climbs through a hatch.
constexpr float kLX = -3.0F;
constexpr float kLZ = -160.1F;
constexpr float kHatchX0 = 4.1F;
constexpr float kHatchX1 = 5.7F;
constexpr float kHatchZ0 = -152.4F;
constexpr float kHatchZ1 = -150.25F;

void build_frame(std::vector<Part> &frame) {
    for (float h = 506.0F; h <= 616.01F; h += 22.0F) {
        const float s = ring_inner(h);
        const float outer = s + 4.0F;
        const float y0 = h - 0.25F;
        const float y1 = h + 0.25F;
        frame.push_back(span({-outer, y0, kWellZ + s}, {outer, y1, kWellZ + outer}, Material::Concrete));
        frame.push_back(span({-outer, y0, kWellZ - outer}, {outer, y1, kWellZ - s}, Material::Concrete));
        frame.push_back(span({s, y0, kWellZ - s}, {outer, y1, kWellZ + s}, Material::Concrete));
        frame.push_back(span({-outer, y0, kWellZ - s}, {-s, y1, kWellZ + s}, Material::Concrete));
    }
    // Corner columns in 11 m lifts on the rings' outer corners, the last up
    // to the plate's underside.
    for (float y = 484.0F; y < 637.99F; y += 11.0F) {
        const float top = y + 11.0F > 637.99F ? kPlateBottom : y + 11.0F;
        const float mid = 0.5F * (y + top);
        const float corner = ring_inner(mid) + 4.0F;
        const float half = 0.41F - 0.01F * (y - 484.0F) / 11.0F;
        for (const float sx : {-1.0F, 1.0F}) {
            for (const float sz : {-1.0F, 1.0F}) {
                frame.push_back(box(JPH::Vec3(half, 0.5F * (top - y), half),
                                    JPH::Vec3(sx * corner, mid, kWellZ + sz * corner), Material::Rust));
            }
        }
    }
    // TP-640, 24 m square, in strips round the cab's hole and the hatch.
    const float cx0 = kLX - 1.4F;
    const float cx1 = kLX + 1.4F;
    const float cz0 = kLZ - 1.4F;
    const float cz1 = kLZ + 1.4F;
    const auto plate = [&](const float x0, const float x1, const float z0, const float z1) {
        frame.push_back(span({x0, kPlateBottom, z0}, {x1, kPlateTop, z1}, Material::Concrete));
    };
    plate(-12.0F, cx0, -162.0F, -138.0F);
    plate(cx0, cx1, cz1, -138.0F);
    plate(cx0, cx1, -162.0F, cz0);
    plate(cx1, kHatchX0, -162.0F, -138.0F);
    plate(kHatchX0, kHatchX1, kHatchZ1, -138.0F);
    plate(kHatchX0, kHatchX1, -162.0F, kHatchZ0);
    plate(kHatchX1, 12.0F, -162.0F, -138.0F);
}

// ---- Stage J -----------------------------------------------------------------
constexpr float kJX = 0.0F;
constexpr float kJZ = -136.2F;
constexpr float kJDeck = 484.25F;
constexpr float kJTravel = 44.0F;           // to 528.25
constexpr float kJMassKg = 900.0F;
const JPH::Vec3 kJEyeLocal(0.0F, 1.1F, 1.0F);
constexpr float kJRailX = 1.9F;
constexpr float kJTrayX = 1.6F;            // the joint's tray, beside the rail's gap
const JPH::RVec3 kJJointSeat(kJTrayX, 485.0, kJZ);
const JPH::RVec3 kJJointFound(3.5, 484.3, -139.0);
constexpr float kWagonMassKg = 4000.0F;
constexpr float kWagonTravel = 46.0F;
constexpr float kWagonSlope = 1.2217F;      // 70 degrees
// The wagon's 44 m run ends beside the 528 ring's east band, where the
// traveler it hauls parks: its head stands off the east face's south end.
const JPH::RVec3 kWagonHead(11.6, 571.665, -157.656);
const JPH::RVec3 kJChockPivot = kWagonHead + JPH::RVec3(1.1, 0.4, 0.0);
const JPH::RVec3 kJLanyard1 = kWagonHead + JPH::RVec3(1.9, 1.1, 0.0);
const JPH::RVec3 kJLanyard2(kJX, 486.19, kJZ + 1.6);

// A box on a vehicle that runs along its incline without turning, as it
// stands in the world: level, or plumb, whatever the slope.
Part upright(const JPH::Quat slope, const JPH::Vec3 half, const JPH::Vec3 offset, const Material material) {
    const JPH::Quat back = slope.Conjugated();
    return {half, back * offset, back, material};
}

void build_stage_j(kit::Kit &kit, FacadeCrane &crane, std::vector<Part> &frame) {
    crane.j_traveler = kit.add_body(Sim::kCraneJTravelerEntityId, platform_parts(false), JPH::RVec3(kJX, kJDeck, kJZ),
                                    JPH::Quat::sIdentity(), kJMassKg, 0.8F);
    crane.j_traveler_guide =
        kit.add_guide(crane.j_traveler, JPH::Vec3::sAxisY(), 0.0F, kJTravel, 2.5F, 40000.0F, 1.0F);

    // Its rails, either side, tied back to the rings; the east rail's joint
    // is missing just above the rollers, the tray for its splice block empty.
    const float top = kJDeck + kJTravel + 1.8F;
    frame.push_back(span({-kJRailX - 0.04F, kJDeck, kJZ - 0.04F}, {-kJRailX + 0.04F, top, kJZ + 0.04F}, Material::Steel));
    frame.push_back(span({kJRailX - 0.04F, kJDeck, kJZ - 0.04F}, {kJRailX + 0.04F, 484.84F, kJZ + 0.04F}, Material::Steel));
    frame.push_back(span({kJRailX - 0.04F, 485.6F, kJZ - 0.04F}, {kJRailX + 0.04F, top, kJZ + 0.04F}, Material::Steel));
    frame.push_back(span({kJTrayX - 0.23F, 484.84F, kJZ - 0.3F}, {kJRailX + 0.04F, 484.94F, kJZ + 0.3F}, Material::Steel));
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kJTrayX - 0.25F, 484.84F, kJZ + side * 0.31F - 0.02F},
                             {kJRailX + 0.04F, 485.04F, kJZ + side * 0.31F + 0.02F}, Material::Steel));
    }
    frame.push_back(span({kJTrayX - 0.25F, 484.84F, kJZ - 0.33F}, {kJTrayX - 0.23F, 485.04F, kJZ + 0.33F}, Material::Steel));
    for (const float h : {506.0F, 528.0F}) {
        const float edge = kWellZ + ring_inner(h) + 4.0F;
        for (const float side : {-1.0F, 1.0F}) {
            frame.push_back(span({side * kJRailX - 0.05F, h - 0.15F, edge - 0.05F},
                                 {side * kJRailX + 0.05F, h + 0.15F, kJZ - 0.04F}, Material::Rust));
        }
    }
    frame.push_back(span({-kJRailX - 0.05F, 530.6F, kJZ + 0.9F}, {kJRailX + 0.05F, 530.8F, kJZ + 1.1F},
                         Material::Steel));
    // The landing at the 528 ring's north band.
    frame.push_back(span({kJX - 0.15F, 528.05F, kWellZ + ring_inner(528.0F) + 3.95F},
                         {kJX + 0.15F, 528.25F, kJZ - kPlatformHalf - 0.05F}, Material::Timber));

    crane.j_joint = kit.add_body(Sim::kCraneJJointEntityId,
                                 {box(JPH::Vec3(0.12F, 0.06F, 0.2F), JPH::Vec3::sZero(), Material::Galvanised)},
                                 kJJointFound, JPH::Quat::sIdentity(), 30.0F, 0.8F);
    kit.set_carry(crane.j_joint, kit::CarryKind::Load, JPH::Vec3(0.0F, 0.06F, 0.0F));
    // Any way up, the block in its tray closes the gap.
    kit.set_rail_gap(crane.j_traveler_guide, 0.02F, crane.j_joint, kJJointSeat, JPH::Vec3::sAxisX(), 0.25F, 3.2F);

    // The wagon at the head of its incline on the east face, down-north at
    // 70 degrees, chocked: a funicular car, its deck level and a grab bar
    // plumb down its west side, so where it comes to rest it is a climb.
    const JPH::Quat slope = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), kWagonSlope);
    const JPH::Vec3 down = slope * JPH::Vec3::sAxisZ();
    crane.j_wagon = kit.add_body(
        Sim::kCraneJWagonEntityId,
        {box(JPH::Vec3(0.7F, 0.6F, 1.2F), JPH::Vec3(0.0F, 0.3F, 0.0F), Material::Rust),
         box(JPH::Vec3(0.6F, 0.35F, 1.0F), JPH::Vec3(0.0F, 1.25F, 0.0F), Material::Concrete),
         upright(slope, JPH::Vec3(0.7F, 0.25F, 0.8F), JPH::Vec3(0.0F, 1.45F, 0.8F), Material::Galvanised),
         upright(slope, JPH::Vec3(0.04F, 1.45F, 0.04F), JPH::Vec3(-0.75F, 0.25F, 0.6F), Material::Yellow)},
        kWagonHead, slope, kWagonMassKg, 0.2F);
    crane.j_wagon_guide = kit.add_guide(crane.j_wagon, down, 0.0F, kWagonTravel, 3.0F, 60000.0F, 1.0F);
    incline_track(frame, kWagonHead, slope, false, -3.5F, kWagonTravel + 1.5F, 0.35F);
    crane.j_chock = add_standing_lever(kit, frame, Sim::kCraneJChockEntityId, kJChockPivot,
                                       static_cast<float>(kWagonHead.GetY()) - 0.5F, crane.j_chock_lever);
    crane.j_wagon_catch = kit.add_catch(crane.j_wagon, crane.j_chock_lever, kLeverRelease, 0.05F, false);
    crane.j_handle = add_handle(kit, Sim::kCraneJHandleEntityId, kJLanyard2);
    (void)kit.add_trip_line(crane.j_chock, JPH::Vec3(-0.1F, kStandingArm, 0.0F), crane.j_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kJLanyard1, kJLanyard2);

    // The tow rope: from the wagon's up-slope end over the incline's head
    // sheave, across to the head sheave over the traveler, down to its eye.
    const JPH::Vec3 end1(0.0F, 0.3F, -1.25F);
    const JPH::RVec3 p1 = kWagonHead + JPH::RVec3(slope * end1);
    const JPH::RVec3 f1 = kWagonHead + JPH::RVec3(slope * JPH::Vec3(0.0F, 0.3F, -3.5F));
    const JPH::RVec3 eye = JPH::RVec3(kJX, kJDeck, kJZ) + JPH::RVec3(kJEyeLocal);
    const JPH::RVec3 f2(eye.GetX(), 530.5, eye.GetZ());
    const float length = static_cast<float>(JPH::Vec3(p1 - f1).Length() + JPH::Vec3(eye - f2).Length()) + 0.02F;
    crane.j_rope = kit.add_rope(crane.j_wagon, end1, f1, crane.j_traveler, kJEyeLocal, f2, 1.0F, length, 0.0F);
}

// ---- Stage K -----------------------------------------------------------------
constexpr float kKX = -12.6F;
constexpr float kKZ = -155.0F;
constexpr float kKDeck = 528.25F;
constexpr float kKStop = 44.4F;             // the guide's end, just past the 572 ring's board
constexpr float kKMassKg = 900.0F;
const JPH::Vec3 kKEyeLocal(1.0F, 1.1F, 0.0F);
constexpr float kKPurchase = 0.25F;
// The jib swings in a plane south of the mast, from its heel just off the
// 528 ring's west edge, down to hang plumb beside that edge.
constexpr float kJibZ = -151.0F;
const JPH::RVec3 kJibHeel(-11.3, 552.0, kJibZ);
constexpr float kJibLength = 24.0F;
constexpr float kJibMassKg = 8000.0F;
constexpr float kJibFall = 1.5708F;         // rad, from level to hanging plumb
// Over the heel by as much as lets the jib hang plumb just as the cage,
// hooked on, rises 44.15 m.
const JPH::RVec3 kSnatchBlock(-11.3, 567.56, kJibZ);
constexpr float kMastX0 = -10.5F;
constexpr float kMastX1 = -9.7F;
constexpr float kMastTop = 568.6F;
const JPH::RVec3 kKPinSeat(-10.1, 568.95, kWellZ);
const JPH::RVec3 kKLanyard1(-10.1, 568.95, kWellZ + 1.7);
const JPH::RVec3 kKLanyard2(kKX, 530.19, kKZ + 1.6);

void build_stage_k(kit::Kit &kit, FacadeCrane &crane, std::vector<Part> &frame) {
    crane.k_cage = kit.add_body(Sim::kCraneKCageEntityId, platform_parts(true), JPH::RVec3(kKX, kKDeck, kKZ),
                                JPH::Quat::sIdentity(), kKMassKg, 0.8F);
    crane.k_cage_guide = kit.add_guide(crane.k_cage, JPH::Vec3::sAxisY(), 0.0F, kKStop, 2.5F, 40000.0F, 1.0F);
    kit.set_dogs(crane.k_cage_guide, 0.05F);
    crane.k_eye = kit.add_anchor(crane.k_cage, kKEyeLocal, 1.2F);
    // Boards from the 528 ring onto the cage, and from the cage to the 572
    // ring where it parks.
    const float east = kKX + kPlatformHalf + 0.05F;
    frame.push_back(span({east, 528.05F, kKZ - 0.3F}, {-ring_inner(528.0F) - 4.0F + 0.05F, 528.25F, kKZ + 0.3F},
                         Material::Timber));
    frame.push_back(span({east, 572.05F, kKZ - 0.15F}, {-ring_inner(572.0F) - 4.0F + 0.05F, 572.25F, kKZ + 0.15F},
                         Material::Timber));

    // The crane: a lattice mast on the 528 ring's outer strip, a bracket to
    // the jib's heel, the snatch block's beam at the head.
    for (const float x : {kMastX0 + 0.05F, kMastX1 - 0.05F}) {
        for (const float z : {kWellZ - 0.35F, kWellZ + 0.35F}) {
            frame.push_back(span({x - 0.05F, 528.25F, z - 0.05F}, {x + 0.05F, kMastTop, z + 0.05F}, Material::Yellow));
        }
    }
    for (float y = 530.25F; y < kMastTop - 0.5F; y += 2.0F) {
        frame.push_back(span({kMastX0, y - 0.03F, kWellZ - 0.4F}, {kMastX1, y + 0.03F, kWellZ - 0.3F}, Material::Yellow));
        frame.push_back(span({kMastX0, y - 0.03F, kWellZ + 0.3F}, {kMastX1, y + 0.03F, kWellZ + 0.4F}, Material::Yellow));
    }
    // The heel pin's cheek off the mast's south face, north of the jib; the
    // snatch block's beam at the head.
    const float heel_x = static_cast<float>(kJibHeel.GetX());
    const float block_y = static_cast<float>(kSnatchBlock.GetY());
    frame.push_back(span({heel_x - 0.2F, 551.6F, kJibZ + 0.57F}, {kMastX0, 552.4F, kWellZ - 0.37F}, Material::Steel));
    frame.push_back(span({heel_x - 0.1F, block_y + 0.1F, kJibZ - 0.1F}, {kMastX0, block_y + 0.3F, kWellZ - 0.3F},
                         Material::Steel));

    // The jib, a truss level over the void from its heel, 8 t, a catwalk along
    // its top; a middle chord along its underside is the climb when it hangs.
    // The catwalk's weight keeps it hanging hard on its stop, plumb, rather
    // than swinging about it.
    std::vector<Part> jib;
    const float half = 0.5F * kJibLength;
    for (const float z : {-0.45F, 0.45F}) {
        jib.push_back(box(JPH::Vec3(half, 0.06F, 0.06F), JPH::Vec3(-half, -0.55F, z), Material::Yellow));
    }
    jib.push_back(box(JPH::Vec3(half, 0.06F, 0.06F), JPH::Vec3(-half, 0.55F, 0.0F), Material::Yellow));
    jib.push_back(box(JPH::Vec3(half, 0.04F, 0.35F), JPH::Vec3(-half, 0.65F, 0.0F), Material::Steel));
    jib.push_back(box(JPH::Vec3(half - 0.15F, 0.05F, 0.05F), JPH::Vec3(-half - 0.15F, -0.55F, 0.0F), Material::Yellow));
    for (float x = 1.0F; x < kJibLength - 0.5F; x += 1.0F) {
        for (const float z : {-0.45F, 0.45F}) {
            jib.push_back(box(JPH::Vec3(0.04F, 0.55F, 0.04F), JPH::Vec3(-x, 0.0F, z), Material::Yellow));
        }
    }
    crane.k_jib = kit.add_body(Sim::kCraneKJibEntityId, jib, kJibHeel, JPH::Quat::sIdentity(), kJibMassKg, 0.6F);
    crane.k_jib_hinge =
        kit.add_lever(crane.k_jib, kJibHeel, JPH::Vec3::sAxisZ(), -JPH::Vec3::sAxisX(), 0.0F, kJibFall);

    // Its pendant's pin at the mast head, and the pin's lanyard to a handle
    // over the cage's north edge.
    crane.k_pin = add_pin(kit, frame, Sim::kCraneKPinEntityId, kKPinSeat);
    crane.k_jib_catch = kit.add_pin_catch(crane.k_jib, crane.k_pin, 0.15F, 0.05F);
    crane.k_handle = add_handle(kit, Sim::kCraneKHandleEntityId, kKLanyard2);
    (void)kit.add_trip_line(crane.k_pin, JPH::Vec3(0.0F, 0.0F, 0.25F), crane.k_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kKLanyard1, kKLanyard2);

    // The hoist rope: the jib's tip over the snatch block, across to a head
    // sheave over the cage, down a four-part purchase to its shackle,
    // hanging free over the deck.
    const JPH::Vec3 tip_local(-kJibLength, 0.0F, 0.0F);
    const JPH::RVec3 tip = kJibHeel + JPH::RVec3(tip_local);
    const JPH::RVec3 eye = JPH::RVec3(kKX, kKDeck, kKZ) + JPH::RVec3(kKEyeLocal);
    const JPH::RVec3 f2(kKX, 574.5, kKZ);
    const float hauling = static_cast<float>(JPH::Vec3(eye - f2).Length()) + 0.08F;
    crane.k_shackle = kit.add_body(Sim::kCraneKShackleEntityId,
                                   {box(JPH::Vec3(0.08F, 0.06F, 0.05F), JPH::Vec3::sZero(), Material::Yellow)},
                                   f2 - JPH::RVec3(0.0, hauling, 0.0), JPH::Quat::sIdentity(), 6.0F, 0.8F);
    kit.set_carry(crane.k_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.06F, 0.0F));
    const float length = static_cast<float>(JPH::Vec3(tip - kSnatchBlock).Length()) + kKPurchase * hauling;
    crane.k_rope = kit.add_rope(crane.k_jib, tip_local, kSnatchBlock, crane.k_shackle, JPH::Vec3::sZero(), f2,
                                kKPurchase, length, 0.0F);
    frame.push_back(span({kKX - 0.5F, 574.7F, kKZ - 0.1F}, {kKX + 0.5F, 574.9F, kKZ + 0.1F}, Material::Steel));
}

// ---- Stage L -----------------------------------------------------------------
constexpr float kLDeck = 572.25F;
constexpr float kLTravel = 68.0F;           // to 640.25, flush in TP-640
constexpr float kLMassKg = 900.0F;
const JPH::Vec3 kLEyeLocal(1.0F, 1.1F, 0.0F);
constexpr float kWinchRatio = 0.75F;        // the drums, 3 : 4
constexpr float kCartMassKg = 4000.0F;
constexpr float kCartTravel = 52.0F;
constexpr float kCartSlope = 1.3963F;       // 80 degrees
// The cart's run ends beside the 572 ring's south band, east of the cab: its
// head stands off the south face's east end, 51 m higher.
const JPH::RVec3 kCartHead(9.63, 625.81, -159.75);
const JPH::RVec3 kCatchPivot = kCartHead + JPH::RVec3(-0.8, 0.9, 1.4);
constexpr float kCatchArm = 1.2F;
const JPH::RVec3 kWeightAt = kCartHead + JPH::RVec3(0.1, 3.0, 1.4);
const JPH::RVec3 kWeightPinSeat = kCartHead + JPH::RVec3(0.1, 3.45, 1.4);
const JPH::RVec3 kWeightLanyard1 = kCartHead + JPH::RVec3(0.1, 3.45, 3.1);
const JPH::RVec3 kWeightLanyard2(kLX, 574.19, kLZ - 1.6);
const JPH::RVec3 kClutchPivot(-5.3, 573.6, -157.4);
const JPH::RVec3 kClutchLanyard1(-4.7, 574.4, -157.4);
const JPH::RVec3 kClutchLanyard2(kLX - 0.7, 574.24, kLZ + 1.6);
constexpr float kClutchIn = 0.8F;

void build_stage_l(kit::Kit &kit, FacadeCrane &crane, std::vector<Part> &frame) {
    crane.l_cab = kit.add_body(Sim::kCraneLCabEntityId, platform_parts(false), JPH::RVec3(kLX, kLDeck, kLZ),
                               JPH::Quat::sIdentity(), kLMassKg, 0.8F);
    crane.l_cab_guide = kit.add_guide(crane.l_cab, JPH::Vec3::sAxisY(), 0.0F, kLTravel, 2.5F, 40000.0F, 1.0F);
    kit.set_dogs(crane.l_cab_guide, 0.05F);

    // The cart at the head of its incline on the south face, down-west at
    // 80 degrees, caught: a funicular car, its deck level and a grab bar
    // plumb down its north side.
    const JPH::Quat slope = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::JPH_PI) *
                            JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -kCartSlope);
    const JPH::Vec3 down = slope * JPH::Vec3::sAxisX();
    crane.l_cart = kit.add_body(
        Sim::kCraneLCartEntityId,
        {box(JPH::Vec3(1.1F, 0.5F, 0.7F), JPH::Vec3(0.0F, 0.25F, 0.0F), Material::Rust),
         box(JPH::Vec3(0.9F, 0.3F, 0.6F), JPH::Vec3(0.0F, 1.05F, 0.0F), Material::Steel),
         upright(slope, JPH::Vec3(0.85F, 0.25F, 0.7F), JPH::Vec3(-0.45F, 1.15F, 0.0F), Material::Galvanised),
         upright(slope, JPH::Vec3(0.04F, 1.25F, 0.04F), JPH::Vec3(-0.5F, 0.15F, 0.75F), Material::Yellow)},
        kCartHead, slope, kCartMassKg, 0.2F);
    crane.l_cart_guide = kit.add_guide(crane.l_cart, down, 0.0F, kCartTravel, 2.0F, 60000.0F, 1.0F);
    incline_track(frame, kCartHead, slope, true, -3.5F, kCartTravel + 1.5F, 0.3F);

    // The catch's lever beside the cart, its east arm held up on its stop by
    // a counterweight; over its arm the drop weight hangs on a pin; a buffer
    // shelf under both.
    crane.l_catch_body = kit.add_body(
        Sim::kCraneLCatchEntityId,
        {box(JPH::Vec3(0.5F * kCatchArm, 0.05F, 0.05F), JPH::Vec3(0.5F * kCatchArm, 0.0F, 0.0F), Material::Hazard),
         box(JPH::Vec3(0.15F, 0.2F, 0.15F), JPH::Vec3(-0.3F, 0.0F, 0.0F), Material::Rust)},
        kCatchPivot, JPH::Quat::sIdentity(), 60.0F, 0.5F);
    crane.l_catch_lever =
        kit.add_lever(crane.l_catch_body, kCatchPivot, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), 0.0F, 1.2F);
    crane.l_cart_catch = kit.add_catch(crane.l_cart, crane.l_catch_lever, kLeverRelease, 0.05F, false);
    const JPH::Vec3 h = vec(kCartHead);
    const JPH::Vec3 c = vec(kCatchPivot);
    frame.push_back(span({c.GetX() - 0.1F, h.GetY() - 1.0F, c.GetZ() + 0.18F},
                         {c.GetX() + 0.1F, c.GetY() + 0.05F, c.GetZ() + 0.35F}, Material::Steel));
    frame.push_back(span({h.GetX() - 0.5F, h.GetY() - 0.8F, c.GetZ() - 0.4F}, {h.GetX() + 0.8F, h.GetY() - 0.7F, c.GetZ() + 0.4F},
                         Material::Steel));
    crane.l_weight = kit.add_body(Sim::kCraneLWeightEntityId,
                                  {box(JPH::Vec3(0.25F, 0.25F, 0.25F), JPH::Vec3::sZero(), Material::Concrete)},
                                  kWeightAt, JPH::Quat::sIdentity(), 200.0F, 0.6F);
    crane.l_pin = add_pin(kit, frame, Sim::kCraneLPinEntityId, kWeightPinSeat);
    crane.l_weight_catch = kit.add_pin_catch(crane.l_weight, crane.l_pin, 0.15F, 0.05F);
    crane.l_pin_handle = add_handle(kit, Sim::kCraneLPinHandleEntityId, kWeightLanyard2);
    (void)kit.add_trip_line(crane.l_pin, JPH::Vec3(0.0F, 0.0F, 0.25F), crane.l_pin_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kWeightLanyard1, kWeightLanyard2);

    // The winch, hung from TP-640's underside at the incline's head: the
    // cart's rope on the small drum, the cab's on the big one, their dog
    // clutch out; its lever on the 572 ring by the cab.
    const JPH::Vec3 end1(-1.15F, 0.3F, 0.0F);
    const JPH::RVec3 p1 = kCartHead + JPH::RVec3(slope * end1);
    const JPH::RVec3 f1 = kCartHead + JPH::RVec3(slope * JPH::Vec3(-3.0F, 0.3F, 0.0F));
    const JPH::RVec3 eye = JPH::RVec3(kLX, kLDeck, kLZ) + JPH::RVec3(kLEyeLocal);
    const JPH::RVec3 f2(eye.GetX(), 642.2, eye.GetZ());
    const float length =
        static_cast<float>(JPH::Vec3(p1 - f1).Length() + kWinchRatio * JPH::Vec3(eye - f2).Length()) + 0.02F;
    crane.l_rope = kit.add_rope(crane.l_cart, end1, f1, crane.l_cab, kLEyeLocal, f2, kWinchRatio, length, 0.0F);
    const JPH::Vec3 w = vec(f1);
    for (const float r : {0.45F, 0.8F}) {
        frame.push_back(box(JPH::Vec3(r, r, 0.3F), w + JPH::Vec3(0.0F, 0.0F, r > 0.5F ? 0.7F : -0.1F), Material::Rust));
    }
    frame.push_back(span({w.GetX() - 0.1F, w.GetY() + 0.8F, w.GetZ() - 0.6F}, {w.GetX() + 0.1F, kPlateBottom, w.GetZ() + 0.95F},
                         Material::Steel));
    crane.l_clutch_body =
        add_standing_lever(kit, frame, Sim::kCraneLClutchEntityId, kClutchPivot, 572.25F, crane.l_clutch_lever);
    kit.add_clutch(crane.l_rope, crane.l_clutch_lever, kClutchIn);
    crane.l_clutch_handle = add_handle(kit, Sim::kCraneLClutchHandleEntityId, kClutchLanyard2);
    (void)kit.add_trip_line(crane.l_clutch_body, JPH::Vec3(-0.1F, kStandingArm, 0.0F), crane.l_clutch_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kClutchLanyard1, kClutchLanyard2);
    // The head sheave's gallows on TP-640 over the cab's hole.
    frame.push_back(span({kLX - 1.4F, 642.4F, kLZ - 0.1F}, {kLX + 1.4F, 642.6F, kLZ + 0.1F}, Material::Steel));
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kLX + side * 1.5F - 0.08F, kPlateTop, kLZ - 0.08F},
                             {kLX + side * 1.5F + 0.08F, 642.6F, kLZ + 0.08F}, Material::Steel));
    }
}

// ---- the climbing route, no lift --------------------------------------------------
// Up the east band at z = -150 as AS-008's route: on each ring an L of boards
// out into the well to a ladder on the next ring's inner face; from the 616
// ring a ladder up through TP-640's hatch.
constexpr float kRouteZ = -150.0F;

void build_climbing_route(kit::Kit &kit) {
    std::vector<Part> route;
    for (float h = 484.0F; h < 615.9F; h += 22.0F) {
        const float s = ring_inner(h);
        const float y0 = h + 0.05F;
        const float y1 = h + 0.25F;
        route.push_back(span({s - 1.82F, y0, kRouteZ + 0.85F}, {s + 0.05F, y1, kRouteZ + 1.15F}, Material::Timber));
        route.push_back(span({s - 1.82F, y0, kRouteZ - 0.4F}, {s - 1.52F, y1, kRouteZ + 0.85F}, Material::Timber));
        ladder(route, ring_inner(h + 22.0F) - 0.14F, kRouteZ, false, y1, h + 22.25F);
    }
    ladder(route, 0.5F * (kHatchX0 + kHatchX1), kHatchZ1 - 0.15F, true, 616.25F, kPlateTop);
    (void)kit.add_body(Sim::kCraneRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace

void build_facade_crane(kit::Kit &kit, FacadeCrane &crane) {
    std::vector<Part> frame;
    build_frame(frame);
    build_stage_j(kit, crane, frame);
    build_stage_k(kit, crane, frame);
    build_stage_l(kit, crane, frame);
    (void)kit.add_body(Sim::kCraneFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    build_climbing_route(kit);
}

} // namespace scraperx::sim::bands
