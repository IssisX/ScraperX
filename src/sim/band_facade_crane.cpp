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

// On the 640 m floor, north of where the crane car lets you off. A girder
// stands on a hinge, held by a pin. Carry the pin clear and the girder falls
// north, still on this floor, and its top end rests on the next bit of floor.
// That floor is held up by posts standing on this one. Nothing of it leaves
// the building.
constexpr float kGirderHingeX = 0.0F;
constexpr float kGirderHingeY = 640.50F;
constexpr float kGirderHingeZ = -155.00F;
constexpr float kGirderHalf = 3.50F;
constexpr float kGirderLean = 0.15F;
constexpr float kGirderFall = 0.86F;

void build_girder(kit::Kit &kit, std::vector<Part> &frame) {
    const JPH::RVec3 hinge(kGirderHingeX, kGirderHingeY, kGirderHingeZ);
    const JPH::Quat lean = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), kGirderLean);
    const JPH::Vec3 up = lean * JPH::Vec3::sAxisY();
    // A clevis on the west face, a short way up from the hinge, so the pin
    // sits just above this floor. The hands lift it out. The cheeks, the
    // shelf and the end stops are part of the girder: the pin can slide a
    // little in the slot, and it cannot fall out or be knocked out.
    constexpr float kClevisUp = 0.36F;
    const JPH::Vec3 clevis(-0.90F, kClevisUp - kGirderHalf, 0.05F);
    const kit::BodyIndex girder = kit.add_body(
        Sim::kGirderEntityId,
        {box(JPH::Vec3(0.60F, kGirderHalf, 0.08F), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(0.08F, kGirderHalf, 0.02F), JPH::Vec3(0.52F, 0.0F, 0.08F), Material::Hazard),
         box(JPH::Vec3(0.085F, 0.10F, 0.06F), clevis + JPH::Vec3(0.195F, 0.0F, 0.0F), Material::Steel),
         box(JPH::Vec3(0.03F, 0.08F, 0.05F), clevis + JPH::Vec3(0.12F, 0.0F, 0.0F), Material::Steel),
         box(JPH::Vec3(0.03F, 0.08F, 0.05F), clevis + JPH::Vec3(-0.14F, 0.0F, 0.0F), Material::Steel),
         box(JPH::Vec3(0.07F, 0.015F, 0.05F), clevis + JPH::Vec3(0.0F, -0.049F, 0.0F), Material::Steel),
         box(JPH::Vec3(0.06F, 0.05F, 0.018F), clevis + JPH::Vec3(0.0F, 0.0F, -0.09F), Material::Steel),
         box(JPH::Vec3(0.06F, 0.05F, 0.018F), clevis + JPH::Vec3(0.0F, 0.0F, 0.09F), Material::Steel)},
        hinge + JPH::RVec3(lean * JPH::Vec3(0.0F, kGirderHalf, 0.0F)), lean, 2800.0F, 0.95F);
    (void)kit.add_lever(girder, hinge, JPH::Vec3::sAxisX(), up, 0.0F, kGirderFall);

    frame.push_back(span({-3.50F, 643.60F, -149.50F}, {3.60F, 643.82F, -147.20F}, Material::Concrete));
    frame.push_back(span({-1.20F, 640.25F, -147.70F}, {-0.90F, 643.60F, -147.40F}, Material::Rust));
    frame.push_back(span({0.90F, 640.25F, -147.70F}, {1.20F, 643.60F, -147.40F}, Material::Rust));
    frame.push_back(span({2.90F, 640.25F, -147.70F}, {3.20F, 643.60F, -147.40F}, Material::Rust));
    frame.push_back(span({-3.20F, 640.25F, -147.70F}, {-2.90F, 643.60F, -147.40F}, Material::Rust));
    frame.push_back(span({-0.85F, 640.25F, -155.20F}, {-0.65F, 640.38F, -154.80F}, Material::Steel));
    frame.push_back(span({0.65F, 640.25F, -155.20F}, {0.85F, 640.38F, -154.80F}, Material::Steel));

    const JPH::RVec3 pin_at = hinge + JPH::RVec3(lean * JPH::Vec3(-0.90F, kClevisUp, 0.05F));
    const kit::BodyIndex pin = kit.add_body(
        Sim::kGirderPinEntityId,
        {box(JPH::Vec3(0.055F, 0.022F, 0.022F), JPH::Vec3::sZero(), Material::Hazard)}, pin_at, lean, 8.0F,
        1.0F);
    kit.set_carry(pin, kit::CarryKind::Load, JPH::Vec3(0.0F, 0.03F, 0.0F));
    (void)kit.add_pin_catch(girder, pin, 0.12F, 0.08F);
}

// East of where the girder lets you off, still on that floor. A ladder goes
// up to the next floor. A sheet of steel hangs in front of the ladder. A
// weight beside the floor, and the sheet, are both held by one pin. Lift the
// pin and the weight drops, hauling the sheet east, off the ladder. A shove
// does not move the sheet. The player climbs. Nothing of it leaves the
// building, and the sheet does not carry the player.
constexpr float kLadderX = 2.15F;
constexpr float kLadderZ = -147.55F;
constexpr float kUpperTop = 652.00F;
constexpr float kUpperSouth = -147.10F;
constexpr float kShutterSlide = 1.60F;

void build_shutter(kit::Kit &kit, std::vector<Part> &frame) {
    const float ladder_top = kUpperTop - 0.55F;
    ladder(frame, kLadderX, kLadderZ, true, 643.82F, ladder_top);
    frame.push_back(span({kLadderX - 0.70F, ladder_top - 0.85F, kUpperSouth - 0.05F},
                         {kLadderX + 0.70F, kUpperTop, kUpperSouth + 0.06F}, Material::Steel));
    frame.push_back(span({1.20F, kUpperTop - 0.24F, kUpperSouth}, {3.30F, kUpperTop, -145.40F}, Material::Concrete));
    frame.push_back(span({1.35F, 640.25F, -146.35F}, {1.65F, kUpperTop - 0.24F, -146.05F}, Material::Rust));
    frame.push_back(span({2.70F, 640.25F, -146.35F}, {3.00F, kUpperTop - 0.24F, -146.05F}, Material::Rust));

    const float shutter_z = kLadderZ - 0.42F;
    const float shutter_y = 645.20F;
    const JPH::RVec3 shutter_at(kLadderX, shutter_y, shutter_z);
    const kit::BodyIndex shutter = kit.add_body(
        Sim::kShutterEntityId,
        {box(JPH::Vec3(0.55F, 1.15F, 0.04F), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(0.08F, 1.15F, 0.015F), JPH::Vec3(0.42F, 0.0F, 0.045F), Material::Hazard)},
        shutter_at, JPH::Quat::sIdentity(), 220.0F, 0.6F);
    const kit::GuideIndex shutter_guide =
        kit.add_guide(shutter, JPH::Vec3::sAxisX(), 0.0F, kShutterSlide, 0.0F, 0.0F, 1.0F);
    kit.set_guide_friction(shutter_guide, 250.0F);

    const JPH::RVec3 weight_at(4.30F, 642.90F, -148.60F);
    const kit::BodyIndex weight = kit.add_body(
        Sim::kShutterWeightEntityId,
        {box(JPH::Vec3(0.22F, 0.28F, 0.22F), JPH::Vec3::sZero(), Material::Concrete)}, weight_at,
        JPH::Quat::sIdentity(), 450.0F, 0.6F);
    (void)kit.add_guide(weight, -JPH::Vec3::sAxisY(), 0.0F, 2.20F, 1.2F, 25000.0F, 2.0F);

    const JPH::Vec3 pull_local(0.55F, 0.0F, 0.0F);
    const JPH::RVec3 sheave_plate(4.55F, shutter_y, shutter_z);
    const JPH::Vec3 eye_local(0.0F, 0.28F, 0.0F);
    const JPH::RVec3 sheave_weight(weight_at.GetX(), 645.40F, weight_at.GetZ());
    const float span_plate = JPH::Vec3(sheave_plate - (shutter_at + JPH::RVec3(pull_local))).Length();
    const float span_weight = JPH::Vec3(sheave_weight - (weight_at + JPH::RVec3(eye_local))).Length();
    (void)kit.add_rope(shutter, pull_local, sheave_plate, weight, eye_local, sheave_weight, 1.0F,
                       span_plate + span_weight + 0.02F, 0.0F);
    frame.push_back(box(JPH::Vec3(0.08F, 0.08F, 0.08F), vec(sheave_plate), Material::Rust));
    frame.push_back(box(JPH::Vec3(0.08F, 0.08F, 0.08F), vec(sheave_weight), Material::Rust));

    // The pin on this floor, west of the girder. Lift it out and the weight
    // is free to drop.
    const JPH::RVec3 pin_at(-2.20F, 644.45F, -148.10F);
    const kit::BodyIndex release = add_pin(kit, frame, Sim::kShutterPinEntityId, pin_at);
    frame.push_back(span({-2.32F, 643.82F, -148.22F}, {-2.08F, 644.36F, -147.98F}, Material::Steel));
    // The same pin holds the weight up and holds the sheet where it is. A
    // shove does not move the sheet. Lifting the pin frees both, and the
    // falling weight is what hauls the sheet aside.
    (void)kit.add_pin_catch(weight, release, 0.15F, 0.05F);
    (void)kit.add_pin_catch(shutter, release, 0.15F, 0.05F);
}

// Above the ladder floor, still over the 640 m plate. A ramp sits too high
// and too far east to step onto. It is on a rail that runs down toward this
// floor. A pin on this floor holds it. Lift the pin and the ramp slides down
// onto this floor, and up onto the next one. Nothing of it leaves the building.
constexpr float kRampCos = 0.8660254F;
constexpr float kRampSin = 0.5F;
constexpr float kRampAngle = 0.5235988F;
constexpr float kRampHalf = 3.40F;
constexpr float kRampTravel = 2.10F;

kit::BodyIndex build_ramp(kit::Kit &kit, std::vector<Part> &frame) {
    const JPH::Vec3 uphill(kRampCos, kRampSin, 0.0F);
    const JPH::Vec3 low(3.42F, 652.14F, -146.20F);
    const JPH::Vec3 connected = low + uphill * kRampHalf;
    const JPH::Vec3 stowed = connected + uphill * kRampTravel;
    const JPH::Quat tilt = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kRampAngle);
    const kit::BodyIndex ramp = kit.add_body(
        Sim::kRampEntityId,
        {box(JPH::Vec3(kRampHalf, 0.05F, 0.70F), JPH::Vec3::sZero(), Material::Concrete),
         box(JPH::Vec3(kRampHalf, 0.035F, 0.03F), JPH::Vec3(0.0F, 0.22F, 0.66F), Material::Yellow),
         box(JPH::Vec3(kRampHalf, 0.035F, 0.03F), JPH::Vec3(0.0F, 0.22F, -0.66F), Material::Yellow)},
        JPH::RVec3(stowed.GetX(), stowed.GetY(), stowed.GetZ()), tilt, 700.0F, 0.9F);
    const kit::GuideIndex rail =
        kit.add_guide(ramp, JPH::Vec3(-kRampCos, -kRampSin, 0.0F), 0.0F, kRampTravel, 1.1F, 12000.0F, 1.4F);
    kit.set_guide_friction(rail, 350.0F);
    // The pin is on this floor, clear of the ladder. It holds the ramp up
    // the rail. Lift it out and the ramp slides down.
    const JPH::RVec3 pin_at(1.50, 652.55, -146.40);
    const kit::BodyIndex pin = add_pin(kit, frame, Sim::kRampPinEntityId, pin_at);
    (void)kit.add_pin_catch(ramp, pin, 0.15F, 0.08F);
    (void)rail;

    frame.push_back(span({9.20F, 655.28F, -150.20F}, {11.50F, 655.52F, -145.05F}, Material::Concrete));
    frame.push_back(span({9.70F, 640.25F, -147.15F}, {10.00F, 655.28F, -146.85F}, Material::Rust));
    frame.push_back(span({10.80F, 640.25F, -145.55F}, {11.10F, 655.28F, -145.25F}, Material::Rust));
    frame.push_back(span({10.80F, 640.25F, -147.15F}, {11.10F, 655.28F, -146.85F}, Material::Rust));
    frame.push_back(span({10.15F, 640.25F, -149.85F}, {10.45F, 655.28F, -149.55F}, Material::Rust));
    return ramp;
}

// Above the ramp's floor. A gate stands in front of a ladder. A pin holds it
// shut. Lift the pin, pull the gate off the ladder, and climb. The gate does
// not carry you.
kit::BodyIndex build_gate(kit::Kit &kit, std::vector<Part> &frame) {
    const float hinge_x = 9.55F;
    const float door_half_x = 0.62F;
    const float door_mid_y = 656.90F;
    const float door_z = -146.05F;
    const kit::BodyIndex gate = kit.add_body(
        Sim::kGateEntityId,
        {box(JPH::Vec3(door_half_x, 1.15F, 0.035F), JPH::Vec3::sZero(), Material::Hazard),
         box(JPH::Vec3(0.04F, 0.08F, 0.04F), JPH::Vec3(door_half_x - 0.18F, 0.0F, -0.07F), Material::Yellow)},
        JPH::RVec3(hinge_x + door_half_x, door_mid_y, door_z), JPH::Quat::sIdentity(), 80.0F, 0.6F);
    kit.set_carry(gate, kit::CarryKind::Handle, JPH::Vec3(door_half_x - 0.18F, 0.0F, -0.07F));
    kit.set_damping(gate, 0.4F, 1.2F);
    const JPH::RVec3 hinge(hinge_x, door_mid_y, door_z);
    (void)kit.add_lever(gate, hinge, JPH::Vec3::sAxisY(), JPH::Vec3::sAxisX(), 0.0F, 1.45F);
    // A pin on this floor holds the gate shut. Lift it, then pull the gate.
    // Walking into the gate does nothing while the pin is in.
    const JPH::RVec3 pin_at(11.00, 656.10, -146.90);
    const kit::BodyIndex pin = add_pin(kit, frame, Sim::kGateLatchEntityId, pin_at);
    (void)kit.add_pin_catch(gate, pin, 0.16F, 0.08F);

    // Same arrangement as the shutter ladder. The floor starts 0.45 m past
    // the ladder, and a plate hangs down from that edge so a climber can
    // find the lip. The floor does not cover the person on the ladder.
    constexpr float kGateLadderX = 10.20F;
    constexpr float kGateLadderZ = -145.50F;
    constexpr float kGateFloorTop = 672.00F;
    constexpr float kGateFloorSouth = kGateLadderZ + 0.45F;
    const float ladder_top = kGateFloorTop - 0.55F;
    ladder(frame, kGateLadderX, kGateLadderZ, true, 655.52F, ladder_top);
    frame.push_back(span({kGateLadderX - 0.70F, ladder_top - 0.85F, kGateFloorSouth - 0.05F},
                         {kGateLadderX + 0.70F, kGateFloorTop, kGateFloorSouth + 0.06F}, Material::Steel));
    frame.push_back(span({kGateLadderX - 0.95F, kGateFloorTop - 0.24F, kGateFloorSouth},
                         {kGateLadderX + 1.15F, kGateFloorTop, kGateFloorSouth + 1.70F}, Material::Concrete));
    frame.push_back(span({kGateLadderX - 0.80F, 640.25F, kGateFloorSouth + 0.75F},
                         {kGateLadderX - 0.50F, kGateFloorTop - 0.24F, kGateFloorSouth + 1.05F},
                         Material::Rust));
    frame.push_back(span({kGateLadderX + 0.55F, 640.25F, kGateFloorSouth + 0.75F},
                         {kGateLadderX + 0.85F, kGateFloorTop - 0.24F, kGateFloorSouth + 1.05F},
                         Material::Rust));
    return gate;
}

// Above the ladder floor. A cart with wheels sits on rails that climb west
// at 45°. A concrete weight hangs at the head of the rails. A pin on this
// floor holds the weight. Lift the pin and the weight drops, and the rope
// drags the cart up the rails. The cart is not a cage.
void build_slat_tower(std::vector<Part> &frame);
void build_duct(kit::Kit &kit, std::vector<Part> &frame);
void build_casing(std::vector<Part> &frame);
void build_pitman(kit::Kit &kit, std::vector<Part> &frame);
kit::BodyIndex build_cart_haul(kit::Kit &kit, std::vector<Part> &frame) {
    constexpr float kS = 0.70710678F;
    constexpr float kSlope = 2.3561945F; // local +X points up the 45° rail, west and up
    constexpr float kRun = 16.0F;
    const JPH::Vec3 uphill(-kS, kS, 0.0F);
    const JPH::Vec3 origin(7.20F, 672.22F, -150.0F);
    const JPH::Quat slope = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kSlope);
    const JPH::Vec3 into = slope * JPH::Vec3::sAxisY();
    const auto wheel = [](const float x, const float z) {
        Part part;
        part.half = JPH::Vec3(0.16F, 0.05F, 0.16F);
        part.offset = JPH::Vec3(x, 1.35F, z);
        part.rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 1.5707963F);
        part.material = Material::Rust;
        part.shape = Part::Shape::Cylinder;
        return part;
    };
    const kit::BodyIndex cart = kit.add_body(
        Sim::kCartHaulEntityId,
        {box(JPH::Vec3(1.15F, 0.05F, 0.42F), JPH::Vec3(0.0F, 1.55F, 0.0F), Material::Steel),
         upright(slope, JPH::Vec3(0.95F, 0.05F, 0.62F), JPH::Vec3(0.0F, -0.05F, 0.0F), Material::Timber),
         upright(slope, JPH::Vec3(0.95F, 0.10F, 0.035F), JPH::Vec3(0.0F, 0.10F, 0.58F), Material::Yellow),
         upright(slope, JPH::Vec3(0.95F, 0.10F, 0.035F), JPH::Vec3(0.0F, 0.10F, -0.58F), Material::Yellow),
         upright(slope, JPH::Vec3(0.05F, 0.42F, 0.58F), JPH::Vec3(0.88F, 0.38F, 0.0F), Material::Steel),
         wheel(-0.72F, -0.36F), wheel(-0.72F, 0.36F), wheel(0.72F, -0.36F), wheel(0.72F, 0.36F)},
        JPH::RVec3(origin.GetX(), origin.GetY(), origin.GetZ()), slope, 800.0F, 0.95F);
    const kit::GuideIndex rail =
        kit.add_guide(cart, uphill, 0.0F, kRun, 1.5F, 40000.0F, 1.2F);
    kit.set_guide_friction(rail, 180.0F);
    (void)rail;

    // The hitch sits on the line through the cart's centre, along the rail.
    // A hitch off to one side twists the cart against the rail and the rope
    // spends its pull fighting that twist instead of hauling.
    const JPH::Vec3 com_world = vec(kit.body_center_of_mass(cart));
    const JPH::Vec3 com_local = slope.Conjugated() * (com_world - origin);
    const JPH::Vec3 hitch_local = com_local + JPH::Vec3(1.15F, 0.0F, 0.0F);

    const JPH::Vec3 rail_mid = origin + uphill * (0.5F * kRun) + into * 1.15F;
    for (const float side : {-0.42F, 0.42F}) {
        Part steel;
        steel.half = JPH::Vec3(0.5F * kRun + 1.2F, 0.04F, 0.04F);
        steel.offset = rail_mid + JPH::Vec3(0.0F, 0.0F, side);
        steel.rotation = slope;
        steel.material = Material::Rust;
        frame.push_back(steel);
    }
    for (float t = 0.4F; t < kRun + 0.8F; t += 1.4F) {
        Part sleeper;
        sleeper.half = JPH::Vec3(0.08F, 0.035F, 0.62F);
        sleeper.offset = origin + uphill * t + into * 1.22F;
        sleeper.rotation = slope;
        sleeper.material = Material::Timber;
        frame.push_back(sleeper);
    }

    // From the ladder's floor, west and south, onto the rail. Kept off the
    // ladder itself so it does not sit on the climber's head.
    frame.push_back(span({8.2F, 671.76F, -145.35F}, {9.70F, 672.00F, -144.15F}, Material::Concrete));
    frame.push_back(span({4.2F, 671.76F, -151.30F}, {8.60F, 672.00F, -145.05F}, Material::Concrete));

    const JPH::Vec3 hitch0 = origin + slope * hitch_local;
    // Past the landing, so the head beam is not a wall across the walk off.
    const JPH::Vec3 sheave = hitch0 + uphill * (kRun + 6.5F);
    const JPH::Vec3 sheave_w(sheave.GetX(), sheave.GetY(), -153.20F);
    frame.push_back(box(JPH::Vec3(0.12F, 0.12F, 0.12F), sheave, Material::Yellow));
    frame.push_back(box(JPH::Vec3(0.12F, 0.12F, 0.12F), sheave_w, Material::Yellow));
    frame.push_back(span({sheave.GetX() - 0.08F, sheave.GetY() - 0.08F, -153.35F},
                         {sheave.GetX() + 0.08F, sheave.GetY() + 0.35F, -149.85F}, Material::Steel));

    const JPH::Vec3 weight_at(sheave_w.GetX(), sheave_w.GetY() - 8.0F, sheave_w.GetZ());
    const kit::BodyIndex weight = kit.add_body(
        Sim::kCartHaulWeightEntityId,
        {box(JPH::Vec3(0.55F, 0.50F, 0.42F), JPH::Vec3::sZero(), Material::Concrete),
         box(JPH::Vec3(0.10F, 0.10F, 0.10F), JPH::Vec3(0.0F, 0.50F, 0.0F), Material::Hazard)},
        // 2200 kg of concrete. A person standing against the seat loads the
        // cart harder than their weight alone, and 740 kg only creeps. This
        // block is about 0.92 m³, which is that mass, and it reaches the
        // brake at the top of the rail with the rider still aboard.
        JPH::RVec3(weight_at.GetX(), weight_at.GetY(), weight_at.GetZ()), JPH::Quat::sIdentity(), 2200.0F, 0.6F);
    (void)kit.add_guide(weight, -JPH::Vec3::sAxisY(), 0.0F, kRun + 0.4F, 1.6F, 30000.0F, 1.3F);
    for (const float sx : {-0.85F, 0.85F}) {
        for (const float sz : {-0.85F, 0.85F}) {
            frame.push_back(span({weight_at.GetX() + sx - 0.06F, weight_at.GetY() - kRun - 1.5F,
                                  weight_at.GetZ() + sz - 0.06F},
                                 {weight_at.GetX() + sx + 0.06F, sheave_w.GetY() + 0.4F,
                                  weight_at.GetZ() + sz + 0.06F},
                            Material::Rust));
        }
    }

    const JPH::Vec3 weight_com = vec(kit.body_center_of_mass(weight));
    const JPH::Vec3 eye(0.0F, weight_com.GetY() - weight_at.GetY() + 0.55F, 0.0F);
    const float span_cart = (hitch0 - sheave).Length();
    const float span_weight = (weight_at + eye - sheave_w).Length();
    (void)kit.add_rope(cart, hitch_local, JPH::RVec3(sheave.GetX(), sheave.GetY(), sheave.GetZ()), weight, eye,
                       JPH::RVec3(sheave_w.GetX(), sheave_w.GetY(), sheave_w.GetZ()), 1.0F,
                       span_cart + span_weight + 0.04F, 0.0F);

    // The pin is on this floor, beside the cart, not on the weight. Lifting
    // it is what lets the weight drop.
    const JPH::RVec3 pin_at(7.90, 673.05, -149.80);
    const kit::BodyIndex pin = add_pin(kit, frame, Sim::kCartHaulPinEntityId, pin_at);
    (void)kit.add_pin_catch(weight, pin, 0.16F, 0.08F);

    const JPH::Vec3 landed = origin + uphill * kRun;
    frame.push_back(span({landed.GetX() - 4.4F, landed.GetY() - 0.28F, -151.40F},
                         {landed.GetX() - 0.55F, landed.GetY() - 0.04F, -148.50F}, Material::Concrete));
    frame.push_back(span({landed.GetX() - 3.6F, 640.25F, -151.15F},
                         {landed.GetX() - 3.3F, landed.GetY() - 0.28F, -150.85F}, Material::Rust));
    frame.push_back(span({landed.GetX() - 3.6F, 640.25F, -149.15F},
                         {landed.GetX() - 3.3F, landed.GetY() - 0.28F, -148.85F}, Material::Rust));
    (void)landed;
    build_slat_tower(frame);
    build_duct(kit, frame);
    return cart;
}

// North of the cart's landing. A cooling-tower shell, and on its south face
// a column of louvers. The louvers are the climb. Nothing here is a cage,
// and nothing here is held by a pin.
void build_slat_tower(std::vector<Part> &frame) {
    constexpr float kX = -6.20F;
    constexpr float kZ = -147.55F;
    constexpr float kFloorTop = 708.00F;
    constexpr float kFloorSouth = kZ + 0.45F;
    const float ladder_top = kFloorTop - 0.55F;
    ladder(frame, kX, kZ, true, 683.70F, ladder_top);
    frame.push_back(span({kX - 0.70F, ladder_top - 0.85F, kFloorSouth - 0.05F},
                         {kX + 0.70F, kFloorTop, kFloorSouth + 0.06F}, Material::Steel));
    frame.push_back(span({kX - 1.40F, kFloorTop - 0.24F, kFloorSouth},
                         {kX + 1.60F, kFloorTop, kFloorSouth + 2.40F}, Material::Galvanised));
    frame.push_back(span({kX - 1.55F, 683.50F, kZ - 0.15F},
                         {kX - 1.15F, kFloorTop, kFloorSouth + 2.30F}, Material::Galvanised));
    frame.push_back(span({kX + 1.15F, 683.50F, kZ - 0.15F},
                         {kX + 1.55F, kFloorTop, kFloorSouth + 2.30F}, Material::Galvanised));
    frame.push_back(span({-8.40F, 683.25F, -148.50F}, {-4.60F, 683.494F, -147.90F}, Material::Concrete));
    frame.push_back(span({kX - 1.35F, 640.25F, kFloorSouth + 1.00F},
                         {kX - 1.05F, kFloorTop - 0.24F, kFloorSouth + 1.30F}, Material::Rust));
    frame.push_back(span({kX + 1.20F, 640.25F, kFloorSouth + 1.00F},
                         {kX + 1.50F, kFloorTop - 0.24F, kFloorSouth + 1.30F}, Material::Rust));
}

// East of the cooling-tower floor. A duct stands folded up on a hinge at
// its high end. A lever on the floor holds it there. The lever is light:
// the catch takes the duct's weight, and hauling the bar over is all it
// takes. The duct then swings down until its low end sits on this floor,
// thirty degrees, and the walk up it reaches the next deck. Nothing about
// it is a pin, and nothing about it is a cage.
void build_duct(kit::Kit &kit, std::vector<Part> &frame) {
    constexpr float kHingeX = 6.212F;
    constexpr float kHingeY = 714.052F;
    constexpr float kZ = -146.00F;
    constexpr float kHalf = 6.00F;
    constexpr float kStow = -1.3962634F; // -80°, free end up, clear of the floor
    // Hinge angle from that pose down to about 37°. The floor stops it
    // nearer 30°. The extra is so the hinge is not what the walk stands on.
    constexpr float kDrop = 2.05F;
    const JPH::Quat stow = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kStow);
    const JPH::Vec3 hinge(kHingeX, kHingeY, kZ);
    const JPH::Vec3 origin = hinge + stow * JPH::Vec3(-kHalf, 0.0F, 0.0F);
    const kit::BodyIndex duct = kit.add_body(
        Sim::kDuctEntityId,
        {box(JPH::Vec3(kHalf, 0.06F, 0.48F), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(kHalf, 0.035F, 0.03F), JPH::Vec3(0.0F, 0.09F, 0.50F), Material::Yellow),
         box(JPH::Vec3(kHalf, 0.035F, 0.03F), JPH::Vec3(0.0F, 0.09F, -0.50F), Material::Yellow)},
        JPH::RVec3(origin.GetX(), origin.GetY(), origin.GetZ()), stow, 900.0F, 0.9F);
    kit.set_damping(duct, 0.35F, 2.2F);
    kit.set_continuous_collision(duct);
    const JPH::Vec3 stowed_up = stow * JPH::Vec3::sAxisY();
    (void)kit.add_lever(duct, JPH::RVec3(hinge.GetX(), hinge.GetY(), hinge.GetZ()), JPH::Vec3::sAxisZ(),
                        stowed_up, 0.0F, kDrop);

    // The lever stands on the north-west of the cooling-tower floor, off the
    // ladder's line. Its own weight holds it on the stop. The catch takes
    // the duct's weight, so the bar only has to be hauled over.
    const JPH::RVec3 pivot(-7.35, 708.40, -145.35);
    kit::LeverIndex lever = {};
    const kit::BodyIndex lever_body =
        add_standing_lever(kit, frame, Sim::kDuctLeverEntityId, pivot, 708.00F, lever);
    // The bar's weight sits west of the pivot, so its centre is not the pivot.
    // The grab is the top of the bar, measured from that centre.
    kit.set_carry(lever_body, kit::CarryKind::Handle, JPH::Vec3(0.0F, 0.35F, 0.0F));
    (void)kit.add_catch(duct, lever, kLeverRelease, 0.05F, false);

    // Apron east of the cooling-tower floor, where the low end sits.
    frame.push_back(span({-4.70F, 707.76F, -147.20F}, {-3.30F, 708.00F, -144.80F}, Material::Concrete));
    frame.push_back(span({-4.15F, 640.25F, -146.20F}, {-3.85F, 707.76F, -145.90F}, Material::Rust));

    // The deck the high end meets. A short gap, same height as the duct's
    // walk, so the step off is a step and not a drop.
    frame.push_back(span({6.40F, 713.86F, -147.20F}, {8.90F, 714.10F, -144.80F}, Material::Concrete));
    frame.push_back(span({7.20F, 640.25F, -146.85F}, {7.50F, 713.86F, -146.55F}, Material::Rust));
    frame.push_back(span({8.15F, 640.25F, -145.45F}, {8.45F, 713.86F, -145.15F}, Material::Rust));
    // Cheeks at the hinge, either side of the duct, so the pivot is a thing
    // you can see and not a point in the air.
    frame.push_back(span({6.02F, 713.30F, -146.72F}, {6.48F, 714.55F, -146.58F}, Material::Steel));
    frame.push_back(span({6.02F, 713.30F, -145.42F}, {6.48F, 714.55F, -145.28F}, Material::Steel));
    build_casing(frame);
    build_pitman(kit, frame);
}

// South of the duct's deck. A fan casing stands against the east side of
// the well. The climb is the outside of it: up the north ladder, south
// along a gallery, up the south ladder, back north, up again. No pin,
// no cage, and no stair that skips the housing.
void build_casing(std::vector<Part> &frame) {
    constexpr float kWallEast = 7.55F;
    constexpr float kLadderX = 8.70F;
    constexpr float kL1Z = -150.20F;
    constexpr float kL2Z = -155.50F;
    constexpr float kL3Z = -152.40F;

    // The housing itself, founded on the cooling-tower level so it is not
    // hanging off the duct deck.
    frame.push_back(span({4.80F, 708.00F, -158.20F}, {kWallEast, 768.00F, -148.60F}, Material::Galvanised));
    frame.push_back(span({5.10F, 640.25F, -157.90F}, {5.40F, 708.00F, -157.60F}, Material::Rust));
    frame.push_back(span({7.00F, 640.25F, -157.50F}, {7.30F, 708.00F, -157.20F}, Material::Rust));

    // Off the duct deck, south, onto the foot of the first ladder.
    frame.push_back(span({7.00F, 713.86F, -150.50F}, {11.00F, 714.10F, -147.20F}, Material::Concrete));
    frame.push_back(span({8.15F, 713.86F, -152.10F}, {11.00F, 714.10F, -150.50F}, Material::Concrete));
    ladder(frame, kLadderX, kL1Z, true, 714.35F, 728.55F);
    frame.push_back(span({kLadderX - 0.70F, 727.70F, -149.98F}, {kLadderX + 0.70F, 729.10F, -149.82F},
                         Material::Steel));
    frame.push_back(span({8.15F, 728.86F, -149.90F}, {11.20F, 729.10F, -148.20F}, Material::Concrete));
    frame.push_back(span({7.50F, 728.70F, -149.90F}, {8.20F, 729.10F, -148.20F}, Material::Steel));

    // East of the ladders, so the walk south does not stand on a rung.
    frame.push_back(span({9.20F, 728.86F, -155.15F}, {11.20F, 729.10F, -148.20F}, Material::Concrete));
    frame.push_back(span({8.15F, 728.86F, -155.15F}, {11.20F, 729.10F, -154.20F}, Material::Concrete));
    ladder(frame, kLadderX, kL2Z, true, 729.35F, 743.55F);
    frame.push_back(span({kLadderX - 0.70F, 742.70F, -155.98F}, {kLadderX + 0.70F, 744.10F, -155.82F},
                         Material::Steel));
    frame.push_back(span({8.15F, 743.86F, -157.80F}, {11.20F, 744.10F, -155.85F}, Material::Concrete));
    frame.push_back(span({7.50F, 743.70F, -157.80F}, {8.20F, 744.10F, -155.85F}, Material::Steel));

    frame.push_back(span({9.20F, 743.86F, -157.80F}, {11.20F, 744.10F, -152.70F}, Material::Concrete));
    frame.push_back(span({8.15F, 743.86F, -153.50F}, {11.20F, 744.10F, -152.70F}, Material::Concrete));
    ladder(frame, kLadderX, kL3Z, true, 744.35F, 758.55F);
    frame.push_back(span({kLadderX - 0.70F, 757.70F, -152.13F}, {kLadderX + 0.70F, 759.10F, -151.97F},
                         Material::Steel));
    frame.push_back(span({8.15F, 758.86F, -152.05F}, {11.20F, 759.10F, -150.30F}, Material::Concrete));
    frame.push_back(span({7.50F, 758.70F, -152.05F}, {8.20F, 759.10F, -150.30F}, Material::Steel));
}

// North of that gallery. A pitman hangs from a bearing between two posts.
// The rod is too thick to grab. The bar at the bottom is the hold. Pull
// with the swing and the bar carries you over the gap onto the next deck.
// Let go early and you drop onto the shelf under the gap, then climb the
// ladder on the west side back onto the gallery. The bearing is heavy and
// close to the pivot, so a person's pull can move the bar.
void build_pitman(kit::Kit &kit, std::vector<Part> &frame) {
    constexpr float kPivotX = 9.40F;
    constexpr float kPivotY = 763.55F;
    constexpr float kPivotZ = -150.10F;
    constexpr float kRodHalf = 1.50F;
    const JPH::RVec3 pivot(kPivotX, kPivotY, kPivotZ);
    const kit::BodyIndex pitman = kit.add_body(
        Sim::kPitmanEntityId,
        {box(JPH::Vec3(0.50F, 0.32F, 0.32F), JPH::Vec3::sZero(), Material::Steel),
         box(JPH::Vec3(0.12F, 1.00F, 0.08F), JPH::Vec3(0.0F, -1.20F, 0.0F), Material::Rust),
         box(JPH::Vec3(0.55F, 0.045F, 0.045F), JPH::Vec3(0.0F, -2.0F * kRodHalf, 0.0F), Material::Yellow)},
        pivot, JPH::Quat::sIdentity(), 420.0F, 0.15F);
    kit.set_damping(pitman, 0.08F, 0.22F);
    kit.set_continuous_collision(pitman);
    (void)kit.add_lever(pitman, pivot, JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY(), -1.05F, 1.05F);

    // Posts and a cap, clear of the bearing, so the pivot is part of the
    // casing and not a point in the air.
    frame.push_back(span({8.55F, 759.10F, -150.22F}, {8.75F, 764.40F, -149.98F}, Material::Steel));
    frame.push_back(span({10.55F, 759.10F, -150.22F}, {10.75F, 764.40F, -149.98F}, Material::Steel));
    frame.push_back(span({8.05F, 764.12F, -150.24F}, {10.75F, 764.42F, -149.96F}, Material::Steel));
    frame.push_back(span({7.55F, 763.90F, -150.28F}, {8.10F, 764.30F, -149.92F}, Material::Rust));

    // The deck the swing reaches. Its south edge is past the low part of
    // the arc, so letting go too soon falls through the gap.
    frame.push_back(span({8.05F, 758.76F, -148.55F}, {10.85F, 759.00F, -145.50F}, Material::Concrete));
    frame.push_back(span({8.20F, 714.10F, -146.35F}, {8.50F, 758.76F, -146.05F}, Material::Rust));
    frame.push_back(span({10.40F, 714.10F, -146.35F}, {10.70F, 758.76F, -146.05F}, Material::Rust));
    frame.push_back(span({10.40F, 714.10F, -147.70F}, {10.70F, 758.76F, -147.40F}, Material::Rust));
    frame.push_back(span({7.55F, 758.70F, -148.70F}, {8.15F, 759.00F, -148.40F}, Material::Steel));

    // The shelf under the gap. A miss lands here, not twelve metres down.
    frame.push_back(span({7.70F, 756.46F, -150.20F}, {11.00F, 756.70F, -145.40F}, Material::Concrete));
    frame.push_back(span({8.35F, 714.10F, -147.05F}, {8.65F, 756.46F, -146.75F}, Material::Rust));
    frame.push_back(span({10.40F, 714.10F, -147.05F}, {10.70F, 756.46F, -146.75F}, Material::Rust));

    // West of the swing, on the gallery's north edge. The plate is the lip
    // the climb mantles onto, the same shape as the casing ladders, and it
    // stays on that edge so it does not stand in the walk along the shelf.
    ladder(frame, 8.10F, -149.95F, true, 756.95F, 758.55F);
    frame.push_back(span({7.40F, 757.70F, -150.38F}, {8.80F, 759.10F, -150.22F}, Material::Steel));
}

} // namespace

void build_facade_crane(kit::Kit &kit, FacadeCrane &crane) {
    std::vector<Part> frame;
    build_frame(frame);
    build_stage_j(kit, crane, frame);
    build_stage_k(kit, crane, frame);
    build_stage_l(kit, crane, frame);
    build_girder(kit, frame);
    build_shutter(kit, frame);
    const kit::BodyIndex ramp = build_ramp(kit, frame);
    const kit::BodyIndex gate = build_gate(kit, frame);
    const kit::BodyIndex cart = build_cart_haul(kit, frame);
    const kit::BodyIndex frame_body =
        kit.add_body(Sim::kCraneFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    kit.disable_collision(ramp, frame_body);
    kit.disable_collision(gate, frame_body);
    kit.disable_collision(cart, frame_body);
    kit.disable_collision(kit.body_for_entity(Sim::kCartHaulWeightEntityId), frame_body);
    build_climbing_route(kit);
}

} // namespace scraperx::sim::bands
