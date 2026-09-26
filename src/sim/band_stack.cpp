// Band 0, the Stack (grade -> 154 m): the ascent from the ground, one working
// machine at a time with designed climbs between them. The chain and each
// stage's contract are in 03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md.
//
// Positions are world metres against the stack as built: decks every 11 m
// (deck 2's top at 22.00), the south face's outer edge on z = -124 with its
// edge beams 0.3 m proud of it (their tops 0.15 m under each deck's), a
// column at x = 0 and x = +-26, a two-storey diagrid of braces in the face's
// plane (22 m up over 13 m along it), and the yard's kerb along z = -118.

#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <algorithm>

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;

// ---- S1, the water-balance hoist (archetype 01, counter-mass) ---------------
//
// A headframe stands against the south face east of the centre column, a
// header tank on its head. In it a cage rests at grade, open south to the
// yard and north to the tower, and west of the cage an empty bucket hangs at
// the top of its own guide, held by a catch. One rope runs from the bucket's
// bail over two head sheaves and down to the cage's eye. Empty, the bucket
// (150 kg) is lighter than the cage (300 kg). A chain hangs inside the cage
// from the valve's lever over it: a rider who takes hold of it pulls it down,
// the lever opens the tank's valve and lets the catch go, and water runs into
// the bucket until it outweighs the cage with the rider in it. Then the bucket
// falls 21.8 m and the cage rides 21.8 m to a gangway onto deck 2 under a
// brake-only governor. At the foot of its guide the bucket lands on a striker
// that opens its drain: emptied, it is lighter than the cage again, and the
// cage comes back down to the yard by itself.
constexpr float kDeck2Top = 22.00F;

constexpr float kCageX = 10.00F;
constexpr float kCageZ = -120.80F;
constexpr float kCageHalfX = 1.50F;
constexpr float kCageHalfZ = 1.40F;
constexpr float kCageFloorHalfY = 0.10F;
constexpr float kCageFloorTop = 0.25F;
constexpr float kCageOriginY = kCageFloorTop - kCageFloorHalfY;
constexpr float kCagePostHeight = 2.70F;
constexpr float kCageMassKg = 300.0F;
constexpr float kCageTravel = 21.80F;          // floor 0.25 -> 22.05
constexpr float kCageGovernorSpeed = 2.50F;    // m/s
constexpr float kCageGovernorForce = 12000.0F; // N, brake only
constexpr float kCageLevelAccel = 2.00F;       // m/s^2 into each stop
// The rope's end is shackled to an eye on a bracket 0.12 m proud of the
// cage's west face at knee height, under its head sheave.
const JPH::Vec3 kCageEyeLocal(-kCageHalfX - 0.12F, 0.80F, 0.0F);

// The bucket, about its floor's centre: open-topped, a bail across its top.
constexpr float kBucketX = 6.70F;
constexpr float kBucketZ = kCageZ;
constexpr float kBucketHalfX = 0.80F;
constexpr float kBucketHalfZ = 0.70F;
constexpr float kBucketFloorHalfY = 0.06F;
constexpr float kBucketWall = 0.90F;
constexpr float kBucketFloorY = 23.10F;        // at the top of its guide
constexpr float kBucketMassKg = 150.0F;
constexpr float kBucketCapacityKg = 1000.0F;
constexpr float kBailY = kBucketFloorHalfY + kBucketWall + 0.10F;
// The striker opens the drain while the bucket sits on it: 25 kg/s, slow
// enough that a rider has several seconds at the top to step off before the
// cage goes back down.
constexpr float kDrainRate = 25.0F;
const JPH::RVec3 kStrikerPivot(kBucketX, 1.29, kBucketZ + 0.90);

constexpr float kSheaveY = 27.00F;
constexpr float kHeadY = 27.80F;               // top of the headframe
// Legs: west of the bucket, between bucket and cage, east of the cage; north
// and south of both.
constexpr float kLegHalf = 0.12F;
constexpr float kBraceHalf = 0.10F;
constexpr float kLegXs[3] = {5.20F, 8.00F, 11.90F};
constexpr float kLegNorthZ = -122.55F;
constexpr float kLegSouthZ = -119.05F;

// The header tank on the head over the bucket's bay, fed by a riser from the
// yard's main; its downpipe ends in a spout over the bucket's south half.
const JPH::Vec3 kTankMin(5.30F, 28.00F, -122.45F);
const JPH::Vec3 kTankMax(7.90F, 30.40F, -119.15F);
constexpr float kTankWaterKg = 12000.0F;
const JPH::RVec3 kSpout(7.10, 25.10, kBucketZ + 0.45);
constexpr float kFillArea = 0.03F;             // m^2, the valve full open
// The valve's lever turns on the valve's own spindle, on the downpipe over
// the bucket's bay: its 2.4 m arm reaches east over the headframe's middle
// legs and the cage, and a counterweight west of the spindle holds it up on
// its stop. Its chain hangs from the arm's end into the cage: taking hold of
// it pulls it down to hand height, which turns the lever past the catch's
// release and opens the valve. Let go, the lever returns, the valve shuts and
// the catch seats the bucket again. Inside the cage, the chain is in reach
// only of someone standing in it.
const JPH::RVec3 kLeverPivot(kSpout.GetX(), 26.20, kSpout.GetZ() + 0.25);
constexpr float kLeverArm = 2.40F;
const JPH::Vec3 kLeverArmHalf(0.5F * kLeverArm, 0.08F, 0.05F);
constexpr float kCounterweightAt = 0.60F;
const JPH::Vec3 kCounterweightHalf(0.25F, 0.30F, 0.27F);
constexpr float kLeverMassKg = 60.0F;
constexpr float kLeverTravel = 0.80F;
constexpr float kCatchRelease = 0.15F;
constexpr float kValveShut = 0.06F;
constexpr float kValveOpen = 0.24F;
constexpr float kChainTop = kCageFloorTop + 1.95F;   // the handle's top, at rest
// The chain runs straight down from the arm's end past a guide point 1.3 m
// under it, so every pull turns the lever, up to its dead point (0.50 rad,
// 0.97 m of pull); above a rider's head at the top of the ride, and inside
// the cage's open roof.
constexpr float kChainGuideY = 24.90F;
constexpr float kHandleHalfY = 0.04F;

// The gangway from the cage's north side at the top onto deck 2.
constexpr float kGangwayHalfX = 1.40F;
constexpr float kGangwayNorthZ = -124.05F;
constexpr float kGangwaySouthZ = -122.30F;

[[nodiscard]] Part box(const JPH::Vec3 half, const JPH::Vec3 centre, const Material material) {
    return {half, centre, JPH::Quat::sIdentity(), material};
}

// A box from its low corner to its high corner.
[[nodiscard]] Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (low + high), material);
}

// A member of half-section `half` from a to b.
[[nodiscard]] Part strut(const JPH::Vec3 a, const JPH::Vec3 b, const float half,
                         const Material material) {
    const JPH::Vec3 along = b - a;
    const float length = along.Length();
    return {JPH::Vec3(half, half, 0.5F * length), 0.5F * (a + b),
            JPH::Quat::sFromTo(JPH::Vec3::sAxisZ(), along / length), material};
}

// The cage about its floor's centre: a galvanised floor, four yellow posts, a
// top frame, waist rails east and west, open north and south. The eye's
// bracket is on the west face.
[[nodiscard]] std::vector<Part> cage_parts() {
    const float post_half = 0.5F * kCagePostHeight;
    const float post_y = kCageFloorHalfY + post_half;
    const float top_y = kCageFloorHalfY + kCagePostHeight;
    std::vector<Part> cage{
        box(JPH::Vec3(kCageHalfX, kCageFloorHalfY, kCageHalfZ), JPH::Vec3::sZero(),
            Material::Galvanised),
    };
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            cage.push_back(box(JPH::Vec3(0.05F, post_half, 0.05F),
                               JPH::Vec3(sx * (kCageHalfX - 0.05F), post_y, sz * (kCageHalfZ - 0.05F)),
                               Material::Yellow));
        }
        cage.push_back(box(JPH::Vec3(kCageHalfX, 0.06F, 0.05F),
                           JPH::Vec3(0.0F, top_y, sx * (kCageHalfZ - 0.05F)), Material::Yellow));
        cage.push_back(box(JPH::Vec3(0.05F, 0.06F, kCageHalfZ),
                           JPH::Vec3(sx * (kCageHalfX - 0.05F), top_y, 0.0F), Material::Yellow));
        cage.push_back(box(JPH::Vec3(0.04F, 0.45F, kCageHalfZ - 0.10F),
                           JPH::Vec3(sx * (kCageHalfX - 0.04F), 0.55F, 0.0F), Material::Galvanised));
    }
    cage.push_back(box(JPH::Vec3(0.12F, 0.06F, 0.06F),
                       JPH::Vec3(-(kCageHalfX + 0.06F), kCageEyeLocal.GetY(), 0.0F),
                       Material::Hazard));
    return cage;
}

// The bucket about its floor's centre: the floor first (a bin's water lies
// on its first part), four walls, and the bail along z across the top.
[[nodiscard]] std::vector<Part> bucket_parts() {
    const float wall_y = kBucketFloorHalfY + 0.5F * kBucketWall;
    std::vector<Part> bucket{
        box(JPH::Vec3(kBucketHalfX, kBucketFloorHalfY, kBucketHalfZ), JPH::Vec3::sZero(),
            Material::Rust),
    };
    for (const float side : {-1.0F, 1.0F}) {
        bucket.push_back(box(JPH::Vec3(kBucketHalfX, 0.5F * kBucketWall, 0.04F),
                             JPH::Vec3(0.0F, wall_y, side * (kBucketHalfZ - 0.04F)),
                             Material::Rust));
        bucket.push_back(box(JPH::Vec3(0.04F, 0.5F * kBucketWall, kBucketHalfZ - 0.08F),
                             JPH::Vec3(side * (kBucketHalfX - 0.04F), wall_y, 0.0F),
                             Material::Rust));
    }
    bucket.push_back(box(JPH::Vec3(0.04F, 0.04F, kBucketHalfZ), JPH::Vec3(0.0F, kBailY, 0.0F),
                         Material::Hazard));
    return bucket;
}


// A handle hanging with its top at `top` on a chain from above: a bar along
// z, across the view of a rider facing the lever's arm, so both hands close on
// it; hazard-striped, apart from the cage's yellow frame.
kit::BodyIndex add_chain_handle(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 top) {
    const kit::BodyIndex handle = kit.add_body(
        entity, {box(JPH::Vec3(0.04F, kHandleHalfY, 0.22F), JPH::Vec3::sZero(), Material::Hazard)},
        top - JPH::RVec3(0.0, kHandleHalfY, 0.0), JPH::Quat::sIdentity(), 3.0F, 0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kHandleHalfY, 0.0F));
    kit.set_damping(handle, 1.5F, 1.5F);
    return handle;
}

void build_s1_headframe(std::vector<Part> &frame, const JPH::RVec3 cage_sheave,
                        const JPH::RVec3 bucket_sheave) {
    for (const float x : kLegXs) {
        for (const float z : {kLegNorthZ, kLegSouthZ}) {
            frame.push_back(box(JPH::Vec3(kLegHalf, 0.5F * kHeadY, kLegHalf),
                                JPH::Vec3(x, 0.5F * kHeadY, z), Material::Rust));
        }
    }
    const float frame_west = kLegXs[0];
    const float frame_east = kLegXs[2];
    const float span_x = 0.5F * (frame_east - frame_west);
    const float mid_x = 0.5F * (frame_east + frame_west);
    const float span_z = 0.5F * (kLegSouthZ - kLegNorthZ);
    const float mid_z = 0.5F * (kLegSouthZ + kLegNorthZ);
    // Girts round the frame at 9 and 18 m, and the head at the top: beams
    // along x on each face, along z at each leg.
    for (const float y : {9.00F, 18.00F, kHeadY - 0.10F}) {
        const float h = (y > 20.0F) ? 0.10F : 0.08F;
        for (const float z : {kLegNorthZ, kLegSouthZ}) {
            frame.push_back(box(JPH::Vec3(span_x + kLegHalf, h, h), JPH::Vec3(mid_x, y, z),
                                Material::Rust));
        }
        for (const float x : kLegXs) {
            frame.push_back(box(JPH::Vec3(h, h, span_z), JPH::Vec3(x, y, mid_z), Material::Rust));
        }
    }
    frame.push_back(box(JPH::Vec3(span_x, 0.12F, 0.12F), JPH::Vec3(mid_x, kHeadY - 0.30F, kCageZ),
                        Material::Rust));
    // Diagonal braces on the west and north faces, one per bay. Every member a
    // hand could reach from the yard is thicker than a grip (kBraceHalf, the
    // rails' webs, the riser): the headframe carries the machine, it is not a
    // ladder round it.
    for (const float y0 : {0.0F, 9.0F, 18.0F}) {
        const float y1 = (y0 < 17.0F) ? y0 + 9.0F : kHeadY - 0.2F;
        frame.push_back(strut(JPH::Vec3(frame_west, y0 + 0.2F, kLegNorthZ + 0.12F),
                              JPH::Vec3(frame_west, y1, kLegSouthZ - 0.12F), kBraceHalf,
                              Material::Rust));
        frame.push_back(strut(JPH::Vec3(frame_west + 0.12F, y0 + 0.2F, kLegNorthZ),
                              JPH::Vec3(kLegXs[1] - 0.12F, y1, kLegNorthZ), kBraceHalf,
                              Material::Rust));
        if (y0 < 17.0F) {
            frame.push_back(strut(JPH::Vec3(kLegXs[1] + 0.12F, y1, kLegNorthZ),
                                  JPH::Vec3(frame_east - 0.12F, y0 + 0.2F, kLegNorthZ), kBraceHalf,
                                  Material::Rust));
        }
    }
    // The cage's bay is a portal in its top storey, where the cage lets out
    // north onto the gangway: knee braces in the head's corners, above the
    // doorway.
    constexpr float kKneeFoot = 25.00F;
    constexpr float kKneeRun = 1.20F;
    frame.push_back(strut(JPH::Vec3(kLegXs[1] + 0.12F, kKneeFoot, kLegNorthZ),
                          JPH::Vec3(kLegXs[1] + 0.12F + kKneeRun, kHeadY - 0.20F, kLegNorthZ),
                          kBraceHalf, Material::Rust));
    frame.push_back(strut(JPH::Vec3(frame_east - 0.12F, kKneeFoot, kLegNorthZ),
                          JPH::Vec3(frame_east - 0.12F - kKneeRun, kHeadY - 0.20F, kLegNorthZ),
                          kBraceHalf, Material::Rust));
    // The two head sheaves in their housings, hung from the sheave beam.
    for (const JPH::RVec3 sheave : {cage_sheave, bucket_sheave}) {
        const JPH::Vec3 at(sheave);
        frame.push_back(box(JPH::Vec3(0.30F, 0.30F, 0.08F),
                            JPH::Vec3(at.GetX(), kSheaveY + 0.10F, at.GetZ()), Material::Hazard));
        frame.push_back(box(JPH::Vec3(0.05F, 0.5F * (kHeadY - 0.42F - kSheaveY - 0.40F), 0.05F),
                            JPH::Vec3(at.GetX(), 0.5F * (kHeadY - 0.42F + kSheaveY + 0.40F),
                                      at.GetZ() - 0.14F),
                            Material::Rust));
    }
    // The bucket's guide rails either side of it; the cage's on its east side.
    const float rail_bottom = 1.00F;
    const float rail_top = kHeadY - 0.20F;
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(box(JPH::Vec3(0.03F, 0.5F * (rail_top - rail_bottom), 0.11F),
                            JPH::Vec3(kBucketX + side * (kBucketHalfX + 0.08F),
                                      0.5F * (rail_top + rail_bottom), kBucketZ),
                            Material::Steel));
        frame.push_back(box(JPH::Vec3(0.04F, 0.5F * rail_top, 0.11F),
                            JPH::Vec3(kCageX + kCageHalfX + 0.16F, 0.5F * rail_top,
                                      kCageZ + side * 0.60F),
                            Material::Steel));
    }
    // The bucket's buffer at the foot of its guide, and a fence round the foot
    // of its column.
    frame.push_back(box(JPH::Vec3(0.40F, 0.45F, 0.30F), JPH::Vec3(kBucketX, 0.45F, kBucketZ - 0.20F),
                        Material::Hazard));
    const float fence_half_y = 1.20F;
    const float fence_x_half = 0.5F * (kLegXs[1] - kLegXs[0]) - kLegHalf;
    for (const float z : {kLegNorthZ, kLegSouthZ}) {
        frame.push_back(box(JPH::Vec3(fence_x_half, fence_half_y, 0.015F),
                            JPH::Vec3(0.5F * (kLegXs[0] + kLegXs[1]), fence_half_y, z),
                            Material::Galvanised));
    }
    for (const float x : {kLegXs[0], kLegXs[1]}) {
        frame.push_back(box(JPH::Vec3(0.015F, fence_half_y, span_z - kLegHalf),
                            JPH::Vec3(x, fence_half_y, mid_z), Material::Galvanised));
    }
    // The striker's post, beside its pivot.
    const JPH::Vec3 striker(kStrikerPivot);
    frame.push_back(box(JPH::Vec3(0.05F, 0.5F * striker.GetY(), 0.05F),
                        JPH::Vec3(striker.GetX() + 0.22F, 0.5F * striker.GetY(), striker.GetZ()),
                        Material::Rust));
    frame.push_back(box(JPH::Vec3(0.12F, 0.04F, 0.04F),
                        JPH::Vec3(striker.GetX() + 0.10F, striker.GetY(), striker.GetZ()),
                        Material::Rust));

    // The header tank: a floor on the head girts, four walls round the water.
    const JPH::Vec3 tank_mid = 0.5F * (kTankMin + kTankMax);
    const JPH::Vec3 tank_half = 0.5F * (kTankMax - kTankMin);
    frame.push_back(box(JPH::Vec3(tank_half.GetX() + 0.06F, 0.10F, tank_half.GetZ() + 0.06F),
                        JPH::Vec3(tank_mid.GetX(), kTankMin.GetY() - 0.10F, tank_mid.GetZ()),
                        Material::Rust));
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(box(JPH::Vec3(tank_half.GetX() + 0.06F, tank_half.GetY(), 0.03F),
                            JPH::Vec3(tank_mid.GetX(), tank_mid.GetY(),
                                      tank_mid.GetZ() + side * (tank_half.GetZ() + 0.03F)),
                            Material::Rust));
        frame.push_back(box(JPH::Vec3(0.03F, tank_half.GetY(), tank_half.GetZ()),
                            JPH::Vec3(tank_mid.GetX() + side * (tank_half.GetX() + 0.03F),
                                      tank_mid.GetY(), tank_mid.GetZ()),
                            Material::Rust));
    }
    // The riser from the yard's main up the headframe's north-west leg and
    // into the tank's west wall.
    constexpr float kRiserX = 4.90F;
    constexpr float kRiserZ = -122.20F;
    constexpr float kRiserTop = 30.00F;
    frame.push_back(box(JPH::Vec3(0.10F, 0.5F * kRiserTop + 0.10F, 0.10F),
                        JPH::Vec3(kRiserX, 0.5F * kRiserTop, kRiserZ), Material::Galvanised));
    frame.push_back(box(JPH::Vec3(0.5F * (kTankMin.GetX() - 0.03F - kRiserX + 0.10F), 0.10F, 0.10F),
                        JPH::Vec3(0.5F * (kTankMin.GetX() - 0.03F + kRiserX - 0.10F), kRiserTop, kRiserZ),
                        Material::Galvanised));
    // The downpipe from the tank's floor to the spout, and its valve.
    const JPH::Vec3 spout(kSpout);
    const float pipe_top = kTankMin.GetY() - 0.20F;
    frame.push_back(box(JPH::Vec3(0.08F, 0.5F * (pipe_top - spout.GetY()), 0.08F),
                        JPH::Vec3(spout.GetX(), 0.5F * (pipe_top + spout.GetY()), spout.GetZ()),
                        Material::Galvanised));
    // The valve's body, and its spindle out to the lever's hub.
    const JPH::Vec3 pivot(kLeverPivot);
    constexpr float kValveHalf = 0.13F;
    frame.push_back(box(JPH::Vec3(kValveHalf, kValveHalf, kValveHalf),
                        JPH::Vec3(spout.GetX(), pivot.GetY(), spout.GetZ()), Material::Hazard));
    frame.push_back(span({pivot.GetX() - 0.04F, pivot.GetY() - 0.04F, spout.GetZ() + kValveHalf},
                         {pivot.GetX() + 0.04F, pivot.GetY() + 0.04F, pivot.GetZ() - kLeverArmHalf.GetZ() - 0.01F},
                         Material::Steel));
    // The gangway onto deck 2, on knee braces back to the face, with rails
    // along its sides.
    const float gangway_mid_z = 0.5F * (kGangwayNorthZ + kGangwaySouthZ);
    const float gangway_half_z = 0.5F * (kGangwaySouthZ - kGangwayNorthZ);
    frame.push_back(box(JPH::Vec3(kGangwayHalfX, 0.05F, gangway_half_z),
                        JPH::Vec3(kCageX, kDeck2Top - 0.05F, gangway_mid_z), Material::Galvanised));
    for (const float side : {-1.0F, 1.0F}) {
        const float x = kCageX + side * (kGangwayHalfX - 0.10F);
        frame.push_back(strut(JPH::Vec3(x, kDeck2Top - 0.12F, kGangwaySouthZ - 0.10F),
                              JPH::Vec3(x, kDeck2Top - 1.00F, -123.62F), 0.05F, Material::Rust));
        const float rail_x = kCageX + side * (kGangwayHalfX - 0.04F);
        frame.push_back(box(JPH::Vec3(0.04F, 0.04F, gangway_half_z - 0.10F),
                            JPH::Vec3(rail_x, kDeck2Top + 1.05F, gangway_mid_z), Material::Yellow));
        frame.push_back(box(JPH::Vec3(0.04F, 0.525F, 0.04F),
                            JPH::Vec3(rail_x, kDeck2Top + 0.525F, kGangwaySouthZ - 0.12F),
                            Material::Yellow));
    }
}

void build_s1(kit::Kit &kit, Stack &stack, std::vector<Part> &frame) {
    using Sim = Simulation;
    // ---- the cage, on its guide under its governor ---------------------------
    stack.s1_cage = kit.add_body(Sim::kStackS1CageEntityId, cage_parts(),
                                 JPH::RVec3(kCageX, kCageOriginY, kCageZ), JPH::Quat::sIdentity(),
                                 kCageMassKg, 0.9F);
    stack.s1_cage_guide = kit.add_guide(stack.s1_cage, JPH::Vec3::sAxisY(), 0.0F, kCageTravel,
                                        kCageGovernorSpeed, kCageGovernorForce, kCageLevelAccel);

    // ---- the bucket, empty at the top of its guide ---------------------------
    stack.s1_bucket = kit.add_body(Sim::kStackS1BucketEntityId, bucket_parts(),
                                   JPH::RVec3(kBucketX, kBucketFloorY, kBucketZ),
                                   JPH::Quat::sIdentity(), kBucketMassKg, 0.6F);
    stack.s1_bucket_guide =
        kit.add_guide(stack.s1_bucket, JPH::Vec3::sAxisY(), -kCageTravel, 0.0F, 0.0F, 0.0F, 0.0F);

    // ---- the rope: exact for the cage down and the bucket up ------------------
    const JPH::RVec3 eye = JPH::RVec3(kCageX, kCageOriginY, kCageZ) + JPH::RVec3(kCageEyeLocal);
    const JPH::RVec3 cage_sheave(eye.GetX(), kSheaveY, eye.GetZ());
    const JPH::RVec3 bucket_sheave(kBucketX, kSheaveY, kBucketZ);
    const JPH::RVec3 bail(kBucketX, kBucketFloorY + kBailY, kBucketZ);
    const float length =
        JPH::Vec3(eye - cage_sheave).Length() + JPH::Vec3(bail - bucket_sheave).Length();
    stack.s1_rope = kit.add_rope(stack.s1_bucket, JPH::Vec3(0.0F, kBailY, 0.0F), bucket_sheave,
                                 stack.s1_cage, kCageEyeLocal, cage_sheave, 1.0F, length, 0.0F);

    build_s1_headframe(frame, cage_sheave, bucket_sheave);

    // ---- the valve's lever, its chain, the valve and the catch -----------------
    // About -z with the arm east, so the chain pulling its end down is positive.
    stack.s1_lever_body = kit.add_body(
        Sim::kStackS1LeverEntityId,
        {box(kLeverArmHalf, JPH::Vec3(0.5F * kLeverArm, 0.0F, 0.0F), Material::Hazard),
         box(kCounterweightHalf, JPH::Vec3(-kCounterweightAt, 0.0F, 0.0F), Material::Rust)},
        kLeverPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    stack.s1_lever = kit.add_lever(stack.s1_lever_body, kLeverPivot, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(),
                                   0.0F, kLeverTravel);
    const JPH::RVec3 lever_end = kLeverPivot + JPH::RVec3(kLeverArm, 0.0, 0.0);
    const JPH::RVec3 chain_top(lever_end.GetX(), kChainTop, lever_end.GetZ());
    stack.s1_chain = add_chain_handle(kit, Sim::kStackS1ChainEntityId, chain_top);
    const JPH::RVec3 chain_sheave(lever_end.GetX(), kChainGuideY, lever_end.GetZ());
    (void)kit.add_trip_line(stack.s1_lever_body, JPH::Vec3(kLeverArm, 0.0F, 0.0F), stack.s1_chain,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), chain_sheave, chain_sheave);
    stack.s1_catch = kit.add_catch(stack.s1_bucket, stack.s1_lever, kCatchRelease, 0.05F, true);
    stack.s1_tank = kit.add_pool(kTankMin, kTankMax, kTankWaterKg);
    stack.s1_fill = kit.add_pipe(stack.s1_tank, kTankMin.GetY(), kit::PoolIndex{}, 0.0F, kSpout,
                                 kFillArea, stack.s1_lever, kValveShut, kValveOpen);

    // ---- the striker and the bucket's drain ------------------------------------
    // Its arm reaches north under the bucket's landing; a block south holds
    // it up on its stop until the bucket sits on it. About -x, so the arm's
    // end going down is positive.
    stack.s1_striker_body = kit.add_body(
        Sim::kStackS1StrikerEntityId,
        {box(JPH::Vec3(0.05F, 0.03F, 0.35F), JPH::Vec3(0.0F, 0.0F, -0.45F), Material::Hazard),
         box(JPH::Vec3(0.12F, 0.12F, 0.12F), JPH::Vec3(0.0F, 0.0F, 0.25F), Material::Rust)},
        kStrikerPivot, JPH::Quat::sIdentity(), 12.0F, 0.5F);
    stack.s1_striker = kit.add_lever(stack.s1_striker_body, kStrikerPivot, -JPH::Vec3::sAxisX(),
                                     -JPH::Vec3::sAxisZ(), 0.0F, 0.6F);
    stack.s1_bin = kit.add_bin(stack.s1_bucket, 0.0F, kBucketCapacityKg,
                               JPH::Vec3(0.0F, -kBucketFloorHalfY - 0.02F, 0.0F), stack.s1_striker,
                               0.12F, 1.5F, kDrainRate);
    kit.set_bin_water(stack.s1_bin);
}

// ---- C1, the facade (deck 2 -> deck 4) -----------------------------------------
//
// A climb on the south face east of S1, every move one the body has. From
// deck 2: out onto a loading landing, up onto its switchgear cabinet, a jump
// to hang from the lip of the duct that runs along the face above, up onto
// the duct and along it to a vent stack, and up the stack over deck 3's
// edge. On deck 3: out along a monorail beam under the ladder that hangs
// from the davit on deck 4 above, a turn and a leap for the ladder, up it
// onto the davit's arm, and back along the arm onto deck 4. Nothing of it
// reaches below deck 2, so the only way to its foot is S1.
constexpr float kDeck3Top = 33.00F;
constexpr float kDeck4Top = 44.00F;
constexpr float kFaceOuterZ = -123.70F;   // the edge beams' outer face
constexpr float kEdgeBeamDrop = 1.25F;    // an edge beam's bottom under its deck

// The loading landing at deck 2, on knee braces to deck 2's edge beam, with
// its switchgear cabinet standing clear of the duct above.
constexpr float kLandingX0 = 17.80F;
constexpr float kLandingX1 = 22.20F;
constexpr float kLandingZ1 = -120.80F;
constexpr float kCabinetX0 = 19.40F;
constexpr float kCabinetX1 = 20.60F;
constexpr float kCabinetZ0 = -122.50F;
constexpr float kCabinetZ1 = -121.80F;
constexpr float kCabinetTop = kDeck2Top + 1.70F;
// The duct along the face, hung on straps from deck 3's edge beam: its lip
// is 3.6 m over the cabinet's top, in reach at the top of a jump, and its
// south face stands 0.4 m clear of the cabinet so the jump does not meet
// its underside.
constexpr float kDuctX0 = 17.40F;
constexpr float kDuctX1 = 24.40F;
constexpr float kDuctBottom = 25.80F;
constexpr float kDuctTop = 27.30F;
constexpr float kDuctZ1 = -122.90F;
// The vent stack from the duct up to deck 3's edge. Where it meets the edge
// a steel plate lies on the deck with its fascia down the edge beam's face:
// one lip, flush from the beam's foot to the plate's top, that a climber on
// the stack tops out over as soon as it is in reach. The mantle carries the
// body straight up past the stack's head and over the plate, so the stack
// tees off under the deck's top and its two outlets rise 1 m on either side
// of that path, wider apart than a body.
constexpr float kVentX = 24.00F;
constexpr float kVentZ = -123.45F;
constexpr float kVentHalf = 0.07F;
constexpr float kVentTeeY = kDeck3Top - 0.30F;
constexpr float kVentOutletHalfSpan = 0.50F;
constexpr float kVentTop = kDeck3Top + 1.00F;
constexpr float kVentPlateHalfX = 0.60F;
constexpr float kVentPlateDepth = 0.95F;     // from the fascia in over the deck
constexpr float kVentPlateThick = 0.03F;
constexpr float kVentFascia = 0.04F;
// Deck 3's monorail and deck 4's davit, one over the other, and the ladder
// hung from the davit's arm with its stiles 1 m above the arm; its bottom
// rung is a jump from the monorail, its stiles clear of a walker's head.
constexpr float kDavitX = 12.50F;
constexpr float kMonorailZ0 = -127.50F;
constexpr float kMonorailZ1 = -119.30F;
constexpr float kMonorailTop = kDeck3Top + 0.30F;
constexpr float kArmZ0 = -126.50F;
constexpr float kArmZ1 = -120.30F;
constexpr float kArmTop = kDeck4Top + 0.30F;
constexpr float kArmBottom = kDeck4Top - 0.30F;
constexpr float kLadderZ = kArmZ1 + 0.15F;
constexpr float kLadderBottomRung = kMonorailTop + 2.30F;
constexpr float kLadderTop = kArmTop + 1.00F;
constexpr float kLadderHalfWidth = 0.28F;
constexpr float kGrabHalfWidth = 0.45F;

void build_c1(std::vector<Part> &route) {
    // ---- the loading landing and its cabinet ---------------------------------
    route.push_back(span({kLandingX0, kDeck2Top - 0.10F, -124.05F}, {kLandingX1, kDeck2Top, kLandingZ1},
                         Material::Galvanised));
    for (const float x : {kLandingX0 + 0.40F, 0.5F * (kLandingX0 + kLandingX1), kLandingX1 - 0.40F}) {
        route.push_back(strut(JPH::Vec3(x, kDeck2Top - 0.12F, kLandingZ1 + 0.10F),
                              JPH::Vec3(x, kDeck2Top - kEdgeBeamDrop + 0.20F, kFaceOuterZ + 0.06F), 0.05F,
                              Material::Rust));
    }
    for (const float x : {kLandingX0 + 0.04F, kLandingX1 - 0.04F}) {
        for (const float z : {kFaceOuterZ + 0.10F, kLandingZ1 - 0.04F}) {
            route.push_back(box(JPH::Vec3(0.04F, 0.525F, 0.04F), JPH::Vec3(x, kDeck2Top + 0.525F, z),
                                Material::Yellow));
        }
        route.push_back(span({x - 0.04F, kDeck2Top + 1.01F, kFaceOuterZ + 0.10F},
                             {x + 0.04F, kDeck2Top + 1.09F, kLandingZ1 - 0.04F}, Material::Yellow));
    }
    route.push_back(span({kLandingX0 + 0.08F, kDeck2Top, kLandingZ1 - 0.06F},
                         {kLandingX1 - 0.08F, kDeck2Top + 0.10F, kLandingZ1}, Material::Hazard));
    route.push_back(span({kCabinetX0, kDeck2Top, kCabinetZ0}, {kCabinetX1, kCabinetTop, kCabinetZ1},
                         Material::Steel));
    route.push_back(span({kCabinetX0 - 0.02F, kCabinetTop - 0.08F, kCabinetZ0 - 0.02F},
                         {kCabinetX1 + 0.02F, kCabinetTop, kCabinetZ1 + 0.02F}, Material::Hazard));

    // ---- the duct, its intake and its straps ---------------------------------
    route.push_back(span({kDuctX0, kDuctBottom, kFaceOuterZ}, {kDuctX1, kDuctTop, kDuctZ1},
                         Material::Galvanised));
    route.push_back(span({kDuctX0 - 0.40F, kDuctBottom - 0.20F, kFaceOuterZ},
                         {kDuctX0, kDuctTop + 0.30F, kDuctZ1 + 0.10F}, Material::Rust));
    const float deck3_beam_bottom = kDeck3Top - kEdgeBeamDrop;
    for (const float x : {18.20F, 23.40F}) {
        route.push_back(span({x - 0.10F, kDuctTop, kFaceOuterZ}, {x + 0.10F, deck3_beam_bottom + 0.30F,
                              kFaceOuterZ + 0.03F},
                             Material::Steel));
    }

    // ---- the vent stack, on the duct and clamped to deck 3's edge beam -------
    route.push_back(span({kVentX - kVentHalf, kDuctTop, kVentZ - kVentHalf},
                         {kVentX + kVentHalf, kVentTeeY + kVentHalf, kVentZ + kVentHalf}, Material::Galvanised));
    route.push_back(span({kVentX - kVentOutletHalfSpan - kVentHalf, kVentTeeY - kVentHalf, kVentZ - kVentHalf},
                         {kVentX + kVentOutletHalfSpan + kVentHalf, kVentTeeY + kVentHalf, kVentZ + kVentHalf},
                         Material::Galvanised));
    for (const float side : {-1.0F, 1.0F}) {
        const float x = kVentX + side * kVentOutletHalfSpan;
        route.push_back(span({x - kVentHalf, kVentTeeY - kVentHalf, kVentZ - kVentHalf},
                             {x + kVentHalf, kVentTop, kVentZ + kVentHalf}, Material::Galvanised));
    }
    route.push_back(span({kVentX - 0.03F, deck3_beam_bottom + 0.35F, kFaceOuterZ + kVentFascia},
                         {kVentX + 0.03F, deck3_beam_bottom + 0.45F, kVentZ - kVentHalf}, Material::Steel));
    route.push_back(span({kVentX - kVentPlateHalfX, kDeck3Top, kFaceOuterZ - kVentPlateDepth},
                         {kVentX + kVentPlateHalfX, kDeck3Top + kVentPlateThick, kFaceOuterZ + kVentFascia},
                         Material::Steel));
    route.push_back(span({kVentX - kVentPlateHalfX, deck3_beam_bottom, kFaceOuterZ},
                         {kVentX + kVentPlateHalfX, kDeck3Top, kFaceOuterZ + kVentFascia}, Material::Steel));

    // ---- deck 3's monorail -------------------------------------------------------
    route.push_back(span({kDavitX - 0.15F, kDeck3Top, kMonorailZ0}, {kDavitX + 0.15F, kMonorailTop, kMonorailZ1},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.20F, kMonorailTop, kMonorailZ1 - 0.12F},
                         {kDavitX + 0.20F, kMonorailTop + 0.15F, kMonorailZ1 - 0.02F}, Material::Hazard));
    // Held down at its back end by a post to deck 4's underside.
    route.push_back(span({kDavitX - 0.125F, kMonorailTop, kMonorailZ0 + 0.10F},
                         {kDavitX + 0.125F, kDeck4Top - 0.50F, kMonorailZ0 + 0.35F}, Material::Rust));
    // Its trolley and hook block, parked under the beam near its end.
    route.push_back(span({kDavitX - 0.25F, kDeck3Top - 0.30F, -120.90F}, {kDavitX + 0.25F, kDeck3Top, -120.50F},
                         Material::Rust));
    route.push_back(span({kDavitX - 0.10F, kDeck3Top - 1.10F, -120.80F},
                         {kDavitX + 0.10F, kDeck3Top - 0.30F, -120.60F}, Material::Hazard));

    // ---- deck 4's davit: a box girder, deep over the drop -----------------------
    route.push_back(span({kDavitX - 0.175F, kArmBottom, kFaceOuterZ}, {kDavitX + 0.175F, kArmTop, kArmZ1},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.175F, kDeck4Top, kArmZ0}, {kDavitX + 0.175F, kArmTop, kFaceOuterZ},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.125F, kArmTop, kArmZ0 + 0.10F},
                         {kDavitX + 0.125F, kDeck4Top + 10.50F, kArmZ0 + 0.35F}, Material::Rust));

    // ---- the ladder hung from the arm's end ---------------------------------------
    // Its stiles and rungs stop at the arm's top, hooked over it by low plates,
    // so a climber topping out passes over them; the grab handles above stand
    // wider than a body.
    for (const float side : {-1.0F, 1.0F}) {
        const float x = kDavitX + side * kLadderHalfWidth;
        route.push_back(span({x - 0.03F, kLadderBottomRung - 0.10F, kLadderZ - 0.03F},
                             {x + 0.03F, kArmTop, kLadderZ + 0.03F}, Material::Yellow));
        route.push_back(span({x - 0.03F, kArmTop, kArmZ1 - 0.03F}, {x + 0.03F, kArmTop + 0.06F, kLadderZ + 0.03F},
                             Material::Steel));
        const float grab_x = kDavitX + side * kGrabHalfWidth;
        route.push_back(span({grab_x - 0.03F, kArmTop, kLadderZ - 0.03F}, {grab_x + 0.03F, kLadderTop, kLadderZ + 0.03F},
                             Material::Yellow));
        route.push_back(span({std::min(x, grab_x) - 0.03F, kArmTop - 0.30F, kLadderZ - 0.03F},
                             {std::max(x, grab_x) + 0.03F, kArmTop - 0.24F, kLadderZ + 0.03F}, Material::Yellow));
    }
    for (float y = kLadderBottomRung; y <= kArmTop - 0.05F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(kLadderHalfWidth, 0.02F, 0.02F), JPH::Vec3(kDavitX, y, kLadderZ),
                            Material::Steel));
    }
}

} // namespace

void build_stack(kit::Kit &kit, Stack &stack) {
    using Sim = Simulation;
    std::vector<Part> frame;
    build_s1(kit, stack, frame);
    (void)kit.add_body(Sim::kStackFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);
    std::vector<Part> route;
    build_c1(route);
    (void)kit.add_body(Sim::kStackRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);
}

} // namespace scraperx::sim::bands
