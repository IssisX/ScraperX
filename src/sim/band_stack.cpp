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
#include <iostream>

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
// (150 kg) is lighter than the cage (300 kg). In the middle of the cage's
// floor a scale plate hangs on a line from the valve's lever, 0.1 m proud of
// the floor: nothing to pull (plan rule 11). A rider who stands on it weighs
// it down onto the floor, and the line turns the lever: it lets the catch go,
// opens the tank's valve, and a pawl drops behind it and holds it open. Water
// runs into the bucket until it outweighs the cage with the rider in it. Then
// the bucket falls 21.8 m, knocking the pawl out as it leaves the head, and
// the cage rides 21.8 m to a gangway onto deck 2 under a brake-only governor.
// At the foot of its guide the bucket lands on a striker that opens its
// drain: emptied, it is lighter than the cage again, and the cage comes back
// down to the yard by itself.
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
// The pawl drops behind the lever just past full open, and the bucket knocks
// it out as it leaves the head, 0.15 m down.
constexpr float kValveLatch = 0.25F;
constexpr float kPawlKnockTravel = -0.15F;
// The scale plate in the cage's floor, 1.2 m square, hangs 0.1 m proud of
// the floor on a line that turns the lever by a crank 0.38 m out along its
// arm: weighed down onto the floor, the plate draws 0.1 m of line, which
// turns the lever 0.26 rad, past the pawl. Its 15 kg alone does not turn it
// (the lever's counterweight holds 150 N·m); a rider's weight does.
constexpr float kPlateHalf = 0.60F;
constexpr float kPlateHalfY = 0.03F;
constexpr float kPlateProud = 0.10F;
constexpr float kPlateMassKg = 15.0F;
constexpr float kPlateCrank = 0.38F;
// The line leaves the crank downward over a sheave under the lever, runs
// east under the head to a sheave over the plate's north-east corner, and
// drops through the cage's open roof to an eye on the plate there.
constexpr float kLineSheaveY = 25.40F;

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
// z, both hands close on it; hazard-striped. S2's and S3's trips still hang
// from these (plan rule 11: each to be replaced in its turn).
constexpr float kHandleHalfY = 0.04F;

kit::BodyIndex add_chain_handle(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 top) {
    const kit::BodyIndex handle = kit.add_body(
        entity, {box(JPH::Vec3(0.04F, kHandleHalfY, 0.22F), JPH::Vec3::sZero(), Material::Hazard)},
        top - JPH::RVec3(0.0, kHandleHalfY, 0.0), JPH::Quat::sIdentity(), 3.0F, 0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kHandleHalfY, 0.0F));
    kit.set_damping(handle, 1.5F, 1.5F);
    return handle;
}

// The scale plate about its centre: a yellow tread with an eye at its
// north-east corner where the line is made fast.
[[nodiscard]] std::vector<Part> plate_parts() {
    return {
        box(JPH::Vec3(kPlateHalf, kPlateHalfY, kPlateHalf), JPH::Vec3::sZero(), Material::Yellow),
        box(JPH::Vec3(0.05F, 0.05F, 0.05F), JPH::Vec3(kPlateHalf - 0.08F, kPlateHalfY + 0.05F, -(kPlateHalf - 0.08F)),
            Material::Hazard),
    };
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

void build_yard_traversal(std::vector<Part> &frame) {
    // 1. Transformer & Compressor Station (West Yard, x in [-14, -2], z in [-118, -111])
    // Base concrete containment curb (height 0.30 m: step-up curb):
    frame.push_back(span({-14.00F, 0.00F, -118.00F}, {-2.00F, 0.30F, -111.00F}, Material::Rust));

    // Main step-up transformer 1 (rise 1.10 m from curb -> y = 1.40 m, vaultable):
    frame.push_back(span({-12.50F, 0.30F, -117.00F}, {-8.50F, 1.40F, -113.00F}, Material::Steel));
    frame.push_back(span({-12.55F, 1.32F, -117.05F}, {-8.45F, 1.40F, -117.00F}, Material::Hazard));
    // Cooling radiator fins:
    frame.push_back(span({-8.50F, 0.40F, -116.50F}, {-8.10F, 1.30F, -113.50F}, Material::Hazard));

    // Compressor unit 2 (rise 1.10 m above transformer 1 -> y = 2.50 m, vault/mantle):
    frame.push_back(span({-7.50F, 0.30F, -117.00F}, {-3.50F, 2.50F, -113.00F}, Material::Steel));
    frame.push_back(span({-7.55F, 2.42F, -117.05F}, {-3.45F, 2.50F, -117.00F}, Material::Hazard));

    // Elevated cable tray gantry (0.8 m wide balance beam at y = 3.95 m bridging east):
    frame.push_back(span({-3.50F, 3.80F, -115.50F}, {1.80F, 3.95F, -114.70F}, Material::Yellow));
    frame.push_back(box(JPH::Vec3(0.08F, 1.90F, 0.08F), JPH::Vec3(-0.85F, 1.90F, -115.10F), Material::Steel));
    frame.push_back(box(JPH::Vec3(0.08F, 1.90F, 0.08F), JPH::Vec3(0.95F, 1.90F, -115.10F), Material::Steel));

    // Pipe header & valve manifold (x in [1.8, 4.5], z in [-121.5, -119.5]):
    frame.push_back(span({1.80F, 0.00F, -121.50F}, {4.50F, 1.20F, -119.50F}, Material::Rust));
    frame.push_back(span({1.80F, 1.20F, -121.50F}, {4.50F, 4.20F, -120.00F}, Material::Steel));
    frame.push_back(span({1.75F, 4.15F, -121.55F}, {4.55F, 4.25F, -119.95F}, Material::Hazard));

    // 2. S1 Headframe West Service Tower (x in [1.8, 5.0], z in [-122.6, -119.1])
    // Vertical structural corner columns:
    for (const float x : {1.90F, 4.90F}) {
        for (const float z : {-122.50F, -119.20F}) {
            frame.push_back(box(JPH::Vec3(0.08F, 11.00F, 0.08F), JPH::Vec3(x, 11.00F, z), Material::Rust));
        }
    }

    // Tier 1 Maintenance Catwalk (y = 7.50 m):
    frame.push_back(span({1.80F, 7.38F, -122.60F}, {5.00F, 7.50F, -119.10F}, Material::Galvanised));
    frame.push_back(span({1.75F, 7.50F, -119.20F}, {5.05F, 7.62F, -119.10F}, Material::Hazard));
    frame.push_back(span({1.80F, 8.45F, -119.15F}, {5.00F, 8.55F, -119.10F}, Material::Yellow));
    // Lower service ladder (y = 4.20 m to 7.50 m):
    for (float y = 4.40F; y <= 7.40F; y += 0.30F) {
        frame.push_back(box(JPH::Vec3(0.28F, 0.02F, 0.02F), JPH::Vec3(2.40F, y, -120.10F), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({2.40F + side * 0.28F - 0.02F, 4.20F, -120.13F},
                             {2.40F + side * 0.28F + 0.02F, 7.50F, -120.07F}, Material::Yellow));
    }

    // Tier 2 Compressor Platform & Outrigger Beam (y = 14.80 m):
    frame.push_back(span({1.80F, 14.68F, -122.60F}, {4.80F, 14.80F, -119.10F}, Material::Steel));
    // Mid service ladder (y = 7.50 m to 14.80 m):
    for (float y = 7.70F; y <= 14.60F; y += 0.30F) {
        frame.push_back(box(JPH::Vec3(0.28F, 0.02F, 0.02F), JPH::Vec3(4.50F, y, -119.25F), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({4.50F + side * 0.28F - 0.02F, 7.50F, -119.28F},
                             {4.50F + side * 0.28F + 0.02F, 14.80F, -119.22F}, Material::Yellow));
    }
    // Outrigger cantilever I-beam (balance beam extending toward the face):
    frame.push_back(span({4.80F, 14.68F, -122.60F}, {7.80F, 14.80F, -122.10F}, Material::Yellow));
    frame.push_back(strut(JPH::Vec3(4.80F, 11.20F, -122.35F), JPH::Vec3(7.50F, 14.68F, -122.35F), 0.08F, Material::Rust));

    // Tier 3 Upper Service Gantry (y = 22.00 m, exact Deck 2 elevation):
    frame.push_back(span({1.80F, 21.88F, -124.00F}, {5.00F, 22.00F, -119.10F}, Material::Galvanised));
    // Upper service ladder (y = 14.80 m to 22.00 m):
    for (float y = 15.00F; y <= 21.80F; y += 0.30F) {
        frame.push_back(box(JPH::Vec3(0.28F, 0.02F, 0.02F), JPH::Vec3(2.40F, y, -122.45F), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({2.40F + side * 0.28F - 0.02F, 14.80F, -122.48F},
                             {2.40F + side * 0.28F + 0.02F, 22.00F, -122.42F}, Material::Yellow));
    }
    // Transfer gangway bridge connecting Tier 3 directly onto Deck 2 south-west band:
    frame.push_back(span({1.80F, 21.90F, -125.00F}, {4.80F, 22.00F, -124.00F}, Material::Galvanised));
    frame.push_back(span({1.80F, 22.00F, -124.05F}, {4.80F, 22.05F, -124.00F}, Material::Hazard));
    frame.push_back(span({1.80F, 22.95F, -125.00F}, {1.88F, 23.05F, -124.00F}, Material::Yellow));
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
    build_yard_traversal(frame);

    // ---- the valve's lever, the scale plate, the valve, the catch, the pawl -----
    // About -z with the arm east, so the line pulling its crank down is
    // positive.
    stack.s1_lever_body = kit.add_body(
        Sim::kStackS1LeverEntityId,
        {box(kLeverArmHalf, JPH::Vec3(0.5F * kLeverArm, 0.0F, 0.0F), Material::Hazard),
         box(kCounterweightHalf, JPH::Vec3(-kCounterweightAt, 0.0F, 0.0F), Material::Rust)},
        kLeverPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    stack.s1_lever = kit.add_lever(stack.s1_lever_body, kLeverPivot, -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(),
                                   0.0F, kLeverTravel);
    // The plate, proud of the cage's floor on its own vertical guide; it rides
    // up and down with the cage, on the floor or under a rider's feet.
    const JPH::RVec3 plate_at(kCageX, kCageFloorTop + kPlateProud + kPlateHalfY, kCageZ);
    stack.s1_plate = kit.add_body(Sim::kStackS1PlateEntityId, plate_parts(), plate_at, JPH::Quat::sIdentity(),
                                  kPlateMassKg, 0.9F);
    stack.s1_plate_guide = kit.add_guide(stack.s1_plate, JPH::Vec3::sAxisY(), -kPlateProud, kCageTravel + 0.2F,
                                         0.0F, 0.0F, 0.0F);
    const JPH::Vec3 plate_eye(kPlateHalf - 0.08F, kPlateHalfY + 0.10F, -(kPlateHalf - 0.08F));
    const JPH::RVec3 crank = kLeverPivot + JPH::RVec3(kPlateCrank, 0.0, 0.0);
    const JPH::RVec3 crank_sheave(crank.GetX(), kLineSheaveY, crank.GetZ());
    const JPH::RVec3 plate_sheave = plate_at + JPH::RVec3(plate_eye.GetX(), 0.0, plate_eye.GetZ());
    (void)kit.add_trip_line(stack.s1_lever_body, JPH::Vec3(kPlateCrank, 0.0F, 0.0F), stack.s1_plate, plate_eye,
                            crank_sheave, JPH::RVec3(plate_sheave.GetX(), kLineSheaveY, plate_sheave.GetZ()));
    stack.s1_catch = kit.add_catch(stack.s1_bucket, stack.s1_lever, kCatchRelease, 0.05F, true);
    stack.s1_pawl = kit.add_latch(stack.s1_lever, kValveLatch, stack.s1_bucket_guide, kPawlKnockTravel);
    // The pawl, on a strap from the head beside the lever's arm, clear of its
    // swing, and its trip rod down to a foot just over the bucket's bail at
    // the head: the bucket falling away from under it knocks the pawl out.
    {
        const JPH::Vec3 pivot(kLeverPivot);
        frame.push_back(span({pivot.GetX() + 0.50F, pivot.GetY() - 0.30F, pivot.GetZ() + 0.09F},
                             {pivot.GetX() + 0.62F, pivot.GetY() + 0.02F, pivot.GetZ() + 0.15F}, Material::Hazard));
        frame.push_back(span({pivot.GetX() + 0.53F, pivot.GetY() + 0.02F, pivot.GetZ() + 0.11F},
                             {pivot.GetX() + 0.59F, kHeadY - 0.20F, pivot.GetZ() + 0.13F}, Material::Steel));
        frame.push_back(strut(JPH::Vec3(pivot.GetX() + 0.56F, pivot.GetY() - 0.30F, pivot.GetZ() + 0.12F),
                              JPH::Vec3(kBucketX + 0.05F, kBucketFloorY + kBailY + 0.10F, kBucketZ + 0.30F), 0.02F,
                              Material::Steel));
    }
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

void build_deck2_traversal(std::vector<Part> &route) {
    // 1. High-Voltage Transformer Blast Barrier & Capacitor Bank
    // Located in south-east bay (x in [13.5, 17.5], z in [-131.0, -126.8])
    // Clear of nominal path x in [10.0, 21.2], z in [-126.0, -124.4]
    route.push_back(span({13.50F, kDeck2Top, -131.00F}, {13.80F, kDeck2Top + 2.20F, -126.80F}, Material::Rust));
    route.push_back(span({14.20F, kDeck2Top, -130.50F}, {17.20F, kDeck2Top + 0.45F, -127.20F}, Material::Rust));
    route.push_back(span({14.50F, kDeck2Top + 0.45F, -130.20F}, {16.80F, kDeck2Top + 1.55F, -128.80F}, Material::Steel));
    route.push_back(span({14.45F, kDeck2Top + 1.47F, -130.25F}, {16.85F, kDeck2Top + 1.55F, -128.75F}, Material::Hazard));
    route.push_back(span({15.45F, kDeck2Top + 1.40F, -128.80F}, {15.85F, kDeck2Top + 1.55F, -126.80F}, Material::Yellow));

    // 2. Overhead Crane Runway I-Beam (Balance Beam High Route)
    // West access stair up to crane runway I-beam (x in [6.5, 8.5], z from -126.0 to -129.7):
    route.push_back(span({6.50F, kDeck2Top, -126.50F}, {8.50F, kDeck2Top + 0.27F, -126.00F}, Material::Steel));
    route.push_back(span({6.50F, kDeck2Top, -127.00F}, {8.50F, kDeck2Top + 0.54F, -126.50F}, Material::Steel));
    route.push_back(span({6.50F, kDeck2Top, -127.50F}, {8.50F, kDeck2Top + 0.81F, -127.00F}, Material::Steel));
    route.push_back(span({6.50F, kDeck2Top, -128.00F}, {8.50F, kDeck2Top + 1.08F, -127.50F}, Material::Steel));
    route.push_back(span({6.50F, kDeck2Top, -129.70F}, {8.20F, kDeck2Top + 1.35F, -128.00F}, Material::Steel));

    // Narrow 0.40 m wide runway girder spanning east across open machinery bay:
    route.push_back(span({8.20F, kDeck2Top + 1.20F, -129.70F}, {13.50F, kDeck2Top + 1.35F, -129.30F}, Material::Yellow));
    route.push_back(span({8.15F, kDeck2Top + 1.27F, -129.75F}, {13.55F, kDeck2Top + 1.35F, -129.25F}, Material::Hazard));
    for (const float col_x : {8.30F, 10.50F, 13.20F}) {
        route.push_back(box(JPH::Vec3(0.08F, 0.60F, 0.08F), JPH::Vec3(col_x, kDeck2Top + 0.60F, -129.50F), Material::Steel));
    }

    // 3. Broken Catwalk with Angle-Iron Catch Lip
    route.push_back(span({11.00F, kDeck2Top + 0.68F, -132.30F}, {14.20F, kDeck2Top + 0.80F, -131.30F}, Material::Galvanised));
    route.push_back(span({11.00F, kDeck2Top + 0.80F, -131.35F}, {14.20F, kDeck2Top + 1.70F, -131.30F}, Material::Yellow));
    for (float ly = kDeck2Top + 0.20F; ly <= kDeck2Top + 0.70F; ly += 0.25F) {
        route.push_back(box(JPH::Vec3(0.25F, 0.02F, 0.02F), JPH::Vec3(11.30F, ly, -131.30F), Material::Steel));
    }

    // 2.20 m Gap from x = 14.20F to x = 16.40F
    route.push_back(span({16.40F, kDeck2Top + 0.68F, -132.30F}, {18.50F, kDeck2Top + 0.80F, -131.30F}, Material::Galvanised));
    route.push_back(span({16.38F, kDeck2Top + 0.72F, -132.30F}, {16.45F, kDeck2Top + 0.80F, -131.30F}, Material::Hazard));
    route.push_back(span({16.60F, kDeck2Top + 0.80F, -131.35F}, {18.50F, kDeck2Top + 1.70F, -131.30F}, Material::Yellow));
    route.push_back(span({17.80F, kDeck2Top - 0.05F, -131.30F}, {18.50F, kDeck2Top + 0.80F, -125.50F}, Material::Galvanised));
}

void build_c1_traversal_enhancements(std::vector<Part> &route) {
    // 1. Suspended Maintenance Recovery Cradle under Davit Leap
    constexpr float kCradleX0 = 10.00F;
    constexpr float kCradleX1 = 15.00F;
    constexpr float kCradleZ0 = -123.70F;
    constexpr float kCradleZ1 = -118.00F;
    constexpr float kCradleFloorY = 30.50F;

    route.push_back(span({kCradleX0, kCradleFloorY - 0.10F, kCradleZ0},
                         {kCradleX1, kCradleFloorY, kCradleZ1}, Material::Galvanised));
    // Toe boards along perimeter
    route.push_back(span({kCradleX0 - 0.02F, kCradleFloorY, kCradleZ1 - 0.02F},
                         {kCradleX1 + 0.02F, kCradleFloorY + 0.15F, kCradleZ1 + 0.02F}, Material::Hazard));
    route.push_back(span({kCradleX0 - 0.02F, kCradleFloorY, kCradleZ0},
                         {kCradleX0 + 0.02F, kCradleFloorY + 0.15F, kCradleZ1}, Material::Hazard));
    route.push_back(span({kCradleX1 - 0.02F, kCradleFloorY, kCradleZ0},
                         {kCradleX1 + 0.02F, kCradleFloorY + 0.15F, kCradleZ1}, Material::Hazard));

    // Guardrails on west, east, south edges
    route.push_back(span({kCradleX0, kCradleFloorY + 0.95F, kCradleZ1 - 0.06F},
                         {kCradleX1, kCradleFloorY + 1.05F, kCradleZ1}, Material::Yellow));
    route.push_back(span({kCradleX0, kCradleFloorY + 0.95F, kCradleZ0},
                         {kCradleX0 + 0.06F, kCradleFloorY + 1.05F, kCradleZ1}, Material::Yellow));
    route.push_back(span({kCradleX1 - 0.06F, kCradleFloorY + 0.95F, kCradleZ0},
                         {kCradleX1, kCradleFloorY + 1.05F, kCradleZ1}, Material::Yellow));

    // Corner suspension hangers
    for (const float sx : {kCradleX0 + 0.10F, kCradleX1 - 0.10F}) {
        for (const float sz : {kCradleZ0 + 0.20F, kCradleZ1 - 0.10F}) {
            route.push_back(box(JPH::Vec3(0.03F, 1.00F, 0.03F), JPH::Vec3(sx, kCradleFloorY + 1.00F, sz), Material::Steel));
        }
    }

    // Vertical recovery ladder mounted to Deck 3 South fascia at X = 11.20F
    constexpr float kRecLadderX = 11.20F;
    constexpr float kRecLadderZ = -123.58F;
    for (float y = kCradleFloorY + 0.30F; y <= kDeck3Top - 0.10F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(0.25F, 0.02F, 0.02F), JPH::Vec3(kRecLadderX, y, kRecLadderZ), Material::Steel));
    }
    // Stiles below deck top
    for (const float side : {-1.0F, 1.0F}) {
        const float x = kRecLadderX + side * 0.25F;
        route.push_back(span({x - 0.02F, kCradleFloorY, kRecLadderZ - 0.02F},
                             {x + 0.02F, kDeck3Top, kRecLadderZ + 0.02F}, Material::Yellow));
        // Flared walk-through grab handles above deck top allowing body passage
        const float grab_x = kRecLadderX + side * kGrabHalfWidth;
        route.push_back(span({grab_x - 0.03F, kDeck3Top, kRecLadderZ - 0.02F},
                             {grab_x + 0.03F, kDeck3Top + 0.90F, kRecLadderZ + 0.02F}, Material::Yellow));
        route.push_back(span({std::min(x, grab_x) - 0.02F, kDeck3Top - 0.05F, kRecLadderZ - 0.02F},
                             {std::max(x, grab_x) + 0.02F, kDeck3Top, kRecLadderZ + 0.02F}, Material::Yellow));
    }

    // Fascia wall plate and top landing floor plate on Deck 3
    route.push_back(span({kRecLadderX - 0.60F, kDeck3Top - 1.20F, kFaceOuterZ},
                         {kRecLadderX + 0.60F, kDeck3Top, kFaceOuterZ + 0.04F}, Material::Steel));
    route.push_back(span({kRecLadderX - 0.60F, kDeck3Top, kFaceOuterZ - 1.20F},
                         {kRecLadderX + 0.60F, kDeck3Top + 0.03F, kFaceOuterZ + 0.04F}, Material::Steel));

    // 2. Alternative Exterior Diagrid Route for C1
    route.push_back(span({24.40F, 27.38F, -123.50F}, {26.20F, 27.50F, -122.90F}, Material::Galvanised));
    route.push_back(span({24.50F, 29.68F, -126.00F}, {26.20F, 29.80F, -124.00F}, Material::Galvanised));
    route.push_back(span({24.50F, 29.80F, -124.06F}, {26.20F, 30.70F, -124.00F}, Material::Yellow));
    for (float y = 27.70F; y <= 29.60F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(0.25F, 0.02F, 0.02F), JPH::Vec3(25.80F, y, -123.80F), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        route.push_back(span({25.80F + side * 0.25F - 0.02F, 27.50F, -123.83F},
                             {25.80F + side * 0.25F + 0.02F, 29.80F, -123.77F}, Material::Yellow));
    }
    route.push_back(span({25.00F, 29.80F, -126.00F}, {25.80F, 30.80F, -127.20F}, Material::Steel));
    route.push_back(span({25.00F, 30.80F, -127.20F}, {25.80F, 31.90F, -128.40F}, Material::Steel));
    route.push_back(span({25.00F, 31.90F, -128.40F}, {25.80F, 33.00F, -129.60F}, Material::Yellow));
}

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

    build_deck2_traversal(route);
    build_c1_traversal_enhancements(route);
}

// ---- S2, the walking beam hoist with fixed ballast cart (archetype 18 mutated)
//
// Bridges Deck 4 (44.0 m) to Deck 6 (66.0 m) across the southern edge of the
// 34 m central shaft. A 24 m steel walking beam is balanced on a central
// trunnion fulcrum at +55 m (kS2Pivot).
// At rest, the walking beam is held tilted by a safety chock catch: its west
// arm (carrying a 1500 kg pig-iron ballast cart, 2400 kg total beam assembly)
// sits high at deck 6 elevation (Y = 66.05), while its east arm sits low at
// deck 4 (Y = 44.05).
// A tow cable runs from the west arm over headframe sheaves to a passenger cage
// at the east arm.
// Inside the cage, a trip handle hangs on a lanyard to the chock lever.
// When the rider boards at deck 4 and pulls the lanyard, the catch releases:
// the 1500 kg ballast cart drives the walking beam down, tilting the massive
// girder across the atrium. As the west arm sinks 22 m, the tow rope hoists
// the cage 22 m up to deck 6 under its 2.5 m/s brake governor. At deck 6,
// safety dogs engage, and the rider walks off across the upper gangway onto
// deck 6's south band.

constexpr float kDeck5Top = 55.00F;
constexpr float kDeck6Top = 66.00F;
constexpr float kS2Travel = 22.00F;

constexpr float kS2CageX = 10.00F;
constexpr float kS2CageZ = -138.00F;
constexpr float kS2CageHalfX = 1.50F;
constexpr float kS2CageHalfZ = 1.40F;
constexpr float kS2CageFloorHalfY = 0.10F;
constexpr float kS2CageFloorTop = kDeck4Top + 0.05F; // 44.05 m
constexpr float kS2CageOriginY = kS2CageFloorTop - kS2CageFloorHalfY;
constexpr float kS2CageMassKg = 300.0F;
constexpr float kS2CageGovernorSpeed = 2.50F;
constexpr float kS2CageGovernorForce = 40000.0F;
constexpr float kS2CageLevelAccel = 2.00F;
const JPH::Vec3 kS2CageEyeLocal(-kS2CageHalfX - 0.12F, 0.80F, 0.0F);

// The walking beam pivot (fulcrum):
const JPH::RVec3 kS2Pivot(0.0, kDeck5Top, -141.0);
constexpr float kS2BeamMassKg = 2400.0F; // 1500 kg ballast cart + 900 kg steel frame
const JPH::Vec3 kS2BeamWestEye(-10.0F, 11.05F, 0.0F); // local to beam at angle 0

// Chock lever at the trunnion stand:
const JPH::RVec3 kS2ChockPivot(0.0, kDeck5Top + 1.20, -140.20);
constexpr float kS2ChockArm = 1.80F;
constexpr float kS2ChockMassKg = 35.0F;
constexpr float kS2ChockTravel = 0.60F;
constexpr float kS2ChockRelease = 0.15F;

// Head sheaves:
const JPH::RVec3 kS2SheaveWest(-10.0F, kDeck6Top + 3.00, -141.0);
const JPH::RVec3 kS2SheaveEast(kS2CageX + kS2CageEyeLocal.GetX(), kDeck6Top + 3.00, -138.0);

// Gangways:
constexpr float kS2GangwayNorthZ = -138.0F + kS2CageHalfZ; // -136.60
constexpr float kS2GangwayHalfX = 1.40F;

[[nodiscard]] std::vector<Part> s2_cage_parts() {
    const float post_half = 0.5F * kCagePostHeight;
    const float post_y = kS2CageFloorHalfY + post_half;
    const float top_y = kS2CageFloorHalfY + kCagePostHeight;
    std::vector<Part> cage{
        box(JPH::Vec3(kS2CageHalfX, kS2CageFloorHalfY, kS2CageHalfZ), JPH::Vec3::sZero(),
            Material::Galvanised),
    };
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            cage.push_back(box(JPH::Vec3(0.05F, post_half, 0.05F),
                               JPH::Vec3(sx * (kS2CageHalfX - 0.05F), post_y, sz * (kS2CageHalfZ - 0.05F)),
                               Material::Yellow));
        }
        cage.push_back(box(JPH::Vec3(kS2CageHalfX, 0.06F, 0.05F),
                           JPH::Vec3(0.0F, top_y, sx * (kS2CageHalfZ - 0.05F)), Material::Yellow));
        cage.push_back(box(JPH::Vec3(0.05F, 0.06F, kS2CageHalfZ),
                           JPH::Vec3(sx * (kS2CageHalfX - 0.05F), top_y, 0.0F), Material::Yellow));
        cage.push_back(box(JPH::Vec3(0.04F, 0.45F, kS2CageHalfZ - 0.10F),
                           JPH::Vec3(sx * (kS2CageHalfX - 0.04F), 0.55F, 0.0F), Material::Galvanised));
    }
    cage.push_back(box(JPH::Vec3(0.12F, 0.06F, 0.06F),
                       JPH::Vec3(-(kS2CageHalfX + 0.06F), kS2CageEyeLocal.GetY(), 0.0F),
                       Material::Hazard));
    return cage;
}

[[nodiscard]] std::vector<Part> s2_beam_parts() {
    std::vector<Part> beam;
    // Central trunnion bearing axle along Z
    beam.push_back(box(JPH::Vec3(0.20F, 0.20F, 0.35F), JPH::Vec3::sZero(), Material::Steel));

    // The walking beam main girder: top and bottom chords from west (-10, 11) to east (+10, -11)
    const JPH::Vec3 west_pt(-10.0F, 11.05F, 0.0F);
    const JPH::Vec3 east_pt(10.0F, -11.05F, 0.0F);
    const JPH::Vec3 along = (east_pt - west_pt).Normalized();
    const JPH::Vec3 normal(-along.GetY(), along.GetX(), 0.0F);

    // Chords offset by normal (depth 0.35 m, leaves full trunnion clearance)
    beam.push_back(strut(west_pt + normal * 0.35F, east_pt + normal * 0.35F, 0.09F, Material::Rust));
    beam.push_back(strut(west_pt - normal * 0.35F, east_pt - normal * 0.35F, 0.09F, Material::Rust));

    // Web struts along the beam
    for (float t = 0.10F; t <= 0.90F; t += 0.10F) {
        const JPH::Vec3 pt = west_pt * (1.0F - t) + east_pt * t;
        beam.push_back(strut(pt - normal * 0.35F, pt + normal * 0.35F, 0.05F, Material::Steel));
    }

    // West Arm Ballast Cart (pig-iron trolley, 1500 kg):
    // Mounted directly on the outer west arm (centered at t = 0.15, R ~ 12 m from fulcrum)
    const JPH::Vec3 cart_center = west_pt * 0.85F + east_pt * 0.15F;
    beam.push_back(box(JPH::Vec3(1.10F, 0.60F, 0.22F), cart_center + normal * 0.50F, Material::Concrete));
    beam.push_back(box(JPH::Vec3(1.20F, 0.12F, 0.24F), cart_center + normal * 0.10F, Material::Rust));

    // Eye plate on west tip
    beam.push_back(box(JPH::Vec3(0.15F, 0.15F, 0.15F), west_pt, Material::Hazard));

    // East Arm tip & eye plate
    beam.push_back(box(JPH::Vec3(0.15F, 0.15F, 0.15F), east_pt, Material::Hazard));
    return beam;
}

[[maybe_unused]] void build_crossover_gangway(std::vector<Part> &frame, const float deck_top) {
    // 7-step industrial crossover bridge over the perimeter ShaftRail at Z = -133.00:
    // Deck floor is at deck_top. ShaftRail top is at deck_top + 1.09 m.
    // The crossover steps up by 0.28 m per step onto a bridge platform at deck_top + 1.18 m,
    // embedding the rail inside the bridge platform, then steps back down onto the gangway.
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -131.50F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 0.28F, -131.00F}, Material::Galvanised));
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -132.00F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 0.56F, -131.50F}, Material::Galvanised));
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -132.50F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 0.84F, -132.00F}, Material::Galvanised));
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -133.50F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 1.18F, -132.50F}, Material::Galvanised));
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -134.00F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 0.84F, -133.50F}, Material::Galvanised));
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -134.50F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 0.56F, -134.00F}, Material::Galvanised));
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, -135.00F},
                         {kS2CageX + kS2GangwayHalfX, deck_top + 0.28F, -134.50F}, Material::Galvanised));
    const float runway_north = kS2GangwayNorthZ + 0.05F; // -136.55F (5 cm sill clearance)
    frame.push_back(span({kS2CageX - kS2GangwayHalfX, deck_top - 0.10F, runway_north},
                         {kS2CageX + kS2GangwayHalfX, deck_top, -135.00F}, Material::Galvanised));

    for (const float side : {-1.0F, 1.0F}) {
        const float rail_x = kS2CageX + side * (kS2GangwayHalfX - 0.04F);
        frame.push_back(span({rail_x - 0.04F, deck_top + 1.05F, runway_north},
                             {rail_x + 0.04F, deck_top + 1.13F, -134.50F}, Material::Yellow));
        frame.push_back(span({rail_x - 0.04F, deck_top + 2.15F, -134.50F},
                             {rail_x + 0.04F, deck_top + 2.23F, -131.50F}, Material::Yellow));
        frame.push_back(span({rail_x - 0.04F, deck_top, runway_north},
                             {rail_x + 0.04F, deck_top + 1.05F, runway_north + 0.08F}, Material::Yellow));
    }
}

void build_deck4_traversal(std::vector<Part> &frame) {
    // 1. High-Pressure Steam Separator & Receiver Skid (Deck 4 East Bay)
    frame.push_back(span({15.00F, kDeck4Top, -131.00F}, {21.00F, kDeck4Top + 0.45F, -126.00F}, Material::Rust));
    frame.push_back(span({15.50F, kDeck4Top + 0.45F, -130.50F}, {18.50F, kDeck4Top + 1.55F, -128.00F}, Material::Steel));
    frame.push_back(span({15.45F, kDeck4Top + 1.47F, -130.55F}, {18.55F, kDeck4Top + 1.55F, -127.95F}, Material::Hazard));
    frame.push_back(span({19.00F, kDeck4Top + 0.45F, -130.50F}, {20.80F, kDeck4Top + 2.65F, -127.50F}, Material::Steel));
    frame.push_back(span({18.95F, kDeck4Top + 2.57F, -130.55F}, {20.85F, kDeck4Top + 2.65F, -127.45F}, Material::Hazard));

    // 2. East Bay Runway Girder & Balance Beam
    // South access stair from deck floor (44.0m) to runway girder (45.35m):
    for (int step = 0; step < 5; ++step) {
        const float z0 = -127.50F - static_cast<float>(step + 1) * 0.50F;
        const float z1 = -127.50F - static_cast<float>(step) * 0.50F;
        const float y = kDeck4Top + static_cast<float>(step + 1) * 0.27F;
        frame.push_back(span({11.50F, kDeck4Top, z0}, {13.50F, y, z1}, Material::Steel));
    }

    // Narrow 0.40m wide runway balance girder spanning north across open machinery pit:
    frame.push_back(span({12.30F, kDeck4Top + 1.20F, -133.50F}, {12.70F, kDeck4Top + 1.35F, -130.00F}, Material::Yellow));
    frame.push_back(span({12.25F, kDeck4Top + 1.27F, -133.55F}, {12.75F, kDeck4Top + 1.35F, -129.95F}, Material::Hazard));
    // Transverse bracing beam
    frame.push_back(span({12.50F, kDeck4Top + 1.20F, -130.70F}, {15.50F, kDeck4Top + 1.35F, -130.30F}, Material::Yellow));
    frame.push_back(span({12.45F, kDeck4Top + 1.27F, -130.75F}, {15.55F, kDeck4Top + 1.35F, -130.25F}, Material::Hazard));

    // Support stanchions
    frame.push_back(box(JPH::Vec3(0.08F, 0.60F, 0.08F), JPH::Vec3(12.50F, kDeck4Top + 0.60F, -133.50F), Material::Steel));
    frame.push_back(box(JPH::Vec3(0.08F, 0.60F, 0.08F), JPH::Vec3(15.50F, kDeck4Top + 0.60F, -130.50F), Material::Steel));
}

void build_deck6_traversal(std::vector<Part> &frame) {
    // 1. West Service Catwalk & Buffer Casing (Deck 6 South Band Machinery Bay, Z in [-133.0, -128.5])
    frame.push_back(span({-11.50F, kDeck6Top, -133.00F}, {-8.00F, kDeck6Top + 0.45F, -128.50F}, Material::Galvanised));
    frame.push_back(span({-10.80F, kDeck6Top + 0.45F, -132.50F}, {-9.20F, kDeck6Top + 1.55F, -129.50F}, Material::Rust));
    frame.push_back(span({-10.85F, kDeck6Top + 1.47F, -132.55F}, {-9.15F, kDeck6Top + 1.55F, -129.45F}, Material::Hazard));
    frame.push_back(span({-8.00F, kDeck6Top, -133.00F}, {-5.00F, kDeck6Top + 0.45F, -128.50F}, Material::Galvanised));

    // 2. East Bay Machinery Housing & Balance Trough (Deck 6 South Band Machinery Bay, Z in [-133.0, -128.5])
    frame.push_back(span({14.00F, kDeck6Top, -133.00F}, {18.00F, kDeck6Top + 1.10F, -128.50F}, Material::Steel));
    frame.push_back(span({13.95F, kDeck6Top + 1.02F, -133.05F}, {18.05F, kDeck6Top + 1.10F, -128.45F}, Material::Hazard));
    frame.push_back(span({15.80F, kDeck6Top + 0.95F, -133.00F}, {16.20F, kDeck6Top + 1.10F, -128.50F}, Material::Yellow));
}

void build_s2_frame(std::vector<Part> &frame) {
    build_crossover_gangway(frame, kDeck4Top);
    build_crossover_gangway(frame, kDeck6Top);
    build_deck4_traversal(frame);
    build_deck6_traversal(frame);

    // Landing buffer beams under S2 cage sill at Deck 4 (5 cm clearance under cage floor at 43.85 m)
    const float s2_sill_top = kS2CageOriginY - kS2CageFloorHalfY - 0.05F;
    frame.push_back(span(JPH::Vec3(kS2CageX - kS2CageHalfX - 0.05F, s2_sill_top - 0.40F, kS2CageZ - kS2CageHalfZ - 0.05F),
                         JPH::Vec3(kS2CageX + kS2CageHalfX + 0.05F, s2_sill_top, kS2CageZ + kS2CageHalfZ + 0.05F),
                         Material::Steel));

    // Vertical guide rails for S2 cage on its east side (with 12 cm running clearance)
    const float guide_mid_y = 0.5F * (43.0F + 69.0F);
    const float guide_half_y = 0.5F * (69.0F - 43.0F);
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(box(JPH::Vec3(0.04F, guide_half_y, 0.10F),
                            JPH::Vec3(kS2CageX + kS2CageHalfX + 0.16F, guide_mid_y,
                                      kS2CageZ + side * 0.60F),
                            Material::Steel));
    }

    // Central Trunnion A-frame stand (Y: 44.0 to 55.0, Z: -141.0)
    for (const float sx : {-1.8F, 1.8F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            frame.push_back(strut(JPH::Vec3(sx, kDeck4Top, -141.0F + sz * 0.85F),
                                  JPH::Vec3(0.0F, kDeck5Top - 0.40F, -141.0F + sz * 0.55F), 0.10F,
                                  Material::Steel));
        }
    }
    // Trunnion bearing pillow blocks on north and south sides of fulcrum
    for (const float sz : {-1.0F, 1.0F}) {
        frame.push_back(box(JPH::Vec3(0.35F, 0.40F, 0.10F),
                            JPH::Vec3(0.0F, kDeck5Top - 0.40F, -141.0F + sz * 0.55F),
                            Material::Rust));
    }

    // Head sheaves support beam at Y = 69.0 spanning from west (Z = -141) to east (Z = -138)
    frame.push_back(span(JPH::Vec3(-11.0F, kDeck6Top + 2.80F, -141.5F),
                         JPH::Vec3(kS2CageX + 1.0F, kDeck6Top + 3.20F, -137.5F), Material::Yellow));
    frame.push_back(box(JPH::Vec3(0.2F, 0.2F, 0.1F),
                        JPH::Vec3(-10.0F, kDeck6Top + 3.00F, -141.0F), Material::Hazard));
    frame.push_back(box(JPH::Vec3(0.2F, 0.2F, 0.1F),
                        JPH::Vec3(kS2CageX + kS2CageEyeLocal.GetX(), kDeck6Top + 3.00F, -138.0F),
                        Material::Hazard));
}

void build_s2(kit::Kit &kit, Stack &stack, std::vector<Part> &frame) {
    using Sim = Simulation;

    // ---- The cage, on its vertical guide with governor and safety dogs ---------
    stack.s2_cage = kit.add_body(Sim::kStackS2CageEntityId, s2_cage_parts(),
                                 JPH::RVec3(kS2CageX, kS2CageOriginY, kS2CageZ), JPH::Quat::sIdentity(),
                                 kS2CageMassKg, 0.9F);
    stack.s2_cage_guide = kit.add_guide(stack.s2_cage, JPH::Vec3::sAxisY(), 0.0F, kS2Travel,
                                        kS2CageGovernorSpeed, kS2CageGovernorForce, kS2CageLevelAccel);

    // ---- The walking beam (teeter-totter), on central fulcrum trunnion ---------
    stack.s2_beam = kit.add_body(Sim::kStackS2BeamEntityId, s2_beam_parts(),
                                 kS2Pivot, JPH::Quat::sIdentity(), kS2BeamMassKg, 0.5F);
    // Hinge around +Z axis: at rest angle = 0, can rotate up to 1.75 rad
    stack.s2_beam_hinge = kit.add_lever(stack.s2_beam, kS2Pivot, JPH::Vec3::sAxisZ(),
                                        JPH::Vec3::sAxisX(), 0.0F, 1.75F);

    // ---- The tow rope: connects west arm to east cage -------------------------
    const JPH::RVec3 eye_cage =
        JPH::RVec3(kS2CageX, kS2CageOriginY, kS2CageZ) + JPH::RVec3(kS2CageEyeLocal);
    const JPH::RVec3 eye_beam = kS2Pivot + JPH::RVec3(kS2BeamWestEye);
    const float leg1 = JPH::Vec3(eye_beam - kS2SheaveWest).Length();
    const float leg2 = JPH::Vec3(eye_cage - kS2SheaveEast).Length();
    const float rope_len = leg1 + leg2;
    stack.s2_rope = kit.add_rope(stack.s2_beam, kS2BeamWestEye, kS2SheaveWest,
                                 stack.s2_cage, kS2CageEyeLocal, kS2SheaveEast, 1.0F, rope_len, 0.0F);

    // ---- The chock lever, catch, trip line, and lanyard handle ---------------
    stack.s2_chock_body = kit.add_body(
        Sim::kStackS2ChockEntityId,
        {box(JPH::Vec3(0.5F * kS2ChockArm, 0.06F, 0.05F), JPH::Vec3(0.5F * kS2ChockArm, 0.0F, 0.0F),
             Material::Hazard),
         box(JPH::Vec3(0.20F, 0.15F, 0.15F), JPH::Vec3(1.00F, 0.0F, 0.0F), Material::Rust)},
        kS2ChockPivot, JPH::Quat::sIdentity(), kS2ChockMassKg, 0.5F);
    stack.s2_chock_lever = kit.add_lever(stack.s2_chock_body, kS2ChockPivot, JPH::Vec3::sAxisZ(),
                                         JPH::Vec3::sAxisX(), 0.0F, kS2ChockTravel);
    stack.s2_catch = kit.add_catch(stack.s2_beam, stack.s2_chock_lever, kS2ChockRelease, 0.05F, true);
    stack.s2_cage_catch = kit.add_catch(stack.s2_cage, stack.s2_chock_lever, kS2ChockRelease, 0.05F, true);

    const JPH::RVec3 handle_top(kS2CageX, kS2CageFloorTop + 1.95F, kS2CageZ);
    stack.s2_handle = add_chain_handle(kit, Sim::kStackS2HandleEntityId, handle_top);
    kit.set_damping(stack.s2_handle, 8.0F, 8.0F);

    const JPH::RVec3 trip_sheave1(kS2ChockPivot.GetX() + kS2ChockArm, kS2ChockPivot.GetY() + 0.60,
                                  kS2ChockPivot.GetZ());
    const JPH::RVec3 trip_sheave2(kS2CageX, kDeck6Top + 3.00, kS2CageZ);
    (void)kit.add_trip_line(stack.s2_chock_body, JPH::Vec3(kS2ChockArm, 0.0F, 0.0F), stack.s2_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), trip_sheave1, trip_sheave2);

    build_s2_frame(frame);
}

// ---- C2, the East Machinery Hall & Pipe Rack (66 -> 88 m) -------------------
//
// Climbs from Deck 6's South band along the East machinery hall to Deck 8's
// North band (22 m vertical climb). A switchgear enclosure (1.7 m mantle),
// an exhaust manifold duct (jump-and-hang at 70.8 m, mantle onto duct top),
// a wall rung ladder to Deck 7 (+77 m), a mantle onto a pipe rack girder,
// a 0.35 m monorail balance beam, and a leap to grab a davit hanging ladder
// that tops out onto Deck 8's North band.

constexpr float kDeck7Top = 77.00F;
constexpr float kDeck8Top = 88.00F;

void build_c2_traversal_enhancements(std::vector<Part> &route) {
    // Suspended Maintenance Recovery Cradle under C2 Davit Leap
    constexpr float kC2CradleX0 = 8.50F;
    constexpr float kC2CradleX1 = 11.50F;
    constexpr float kC2CradleZ0 = -163.50F;
    constexpr float kC2CradleZ1 = -160.50F;
    constexpr float kC2CradleFloorY = 78.50F;

    route.push_back(span({kC2CradleX0, kC2CradleFloorY - 0.10F, kC2CradleZ0},
                         {kC2CradleX1, kC2CradleFloorY, kC2CradleZ1}, Material::Galvanised));
    route.push_back(span({kC2CradleX0 - 0.02F, kC2CradleFloorY, kC2CradleZ0 - 0.02F},
                         {kC2CradleX1 + 0.02F, kC2CradleFloorY + 0.15F, kC2CradleZ0 + 0.02F}, Material::Hazard));
    route.push_back(span({kC2CradleX0 - 0.02F, kC2CradleFloorY, kC2CradleZ1 - 0.02F},
                         {kC2CradleX1 + 0.02F, kC2CradleFloorY + 0.15F, kC2CradleZ1 + 0.02F}, Material::Hazard));
    route.push_back(span({kC2CradleX0, kC2CradleFloorY + 0.95F, kC2CradleZ0},
                         {kC2CradleX1, kC2CradleFloorY + 1.05F, kC2CradleZ0 + 0.06F}, Material::Yellow));
    route.push_back(span({kC2CradleX0, kC2CradleFloorY + 0.95F, kC2CradleZ1 - 0.06F},
                         {kC2CradleX1, kC2CradleFloorY + 1.05F, kC2CradleZ1}, Material::Yellow));
    route.push_back(span({kC2CradleX0, kC2CradleFloorY + 0.95F, kC2CradleZ0},
                         {kC2CradleX0 + 0.06F, kC2CradleFloorY + 1.05F, kC2CradleZ1}, Material::Yellow));

    for (const float sx : {kC2CradleX0 + 0.10F, kC2CradleX1 - 0.10F}) {
        for (const float sz : {kC2CradleZ0 + 0.10F, kC2CradleZ1 - 0.10F}) {
            route.push_back(box(JPH::Vec3(0.03F, 1.20F, 0.03F), JPH::Vec3(sx, kC2CradleFloorY + 1.20F, sz), Material::Steel));
        }
    }

    for (float y = kC2CradleFloorY + 0.30F; y <= 82.00F - 0.10F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(0.25F, 0.02F, 0.02F), JPH::Vec3(8.70F, y, -161.80F), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        route.push_back(span({8.70F + side * 0.25F - 0.02F, kC2CradleFloorY, -161.83F},
                             {8.70F + side * 0.25F + 0.02F, 82.90F, -161.77F}, Material::Yellow));
    }
}

void build_c2(std::vector<Part> &route) {
    // 1. Switchgear enclosure on Deck 6 East band (floor 66.00F, top 67.70F)
    route.push_back(span({20.50F, kDeck6Top, -139.00F}, {23.50F, kDeck6Top + 1.70F, -136.50F},
                         Material::Steel));
    route.push_back(span({20.45F, kDeck6Top + 1.62F, -136.55F},
                         {23.55F, kDeck6Top + 1.70F, -136.50F}, Material::Hazard));

    // 2. High exhaust manifold duct along the East band (bottom 69.80F, top 71.30F)
    // South face stands 0.4 m clear of the cabinet (Z = -139.40F vs -139.00F)
    route.push_back(span({20.80F, 69.80F, -148.50F}, {23.20F, 71.30F, -139.40F},
                         Material::Galvanised));
    for (const float z : {-141.0F, -145.0F}) {
        for (const float x : {20.90F, 23.10F}) {
            route.push_back(span({x - 0.05F, 71.30F, z - 0.05F},
                                 {x + 0.05F, kDeck7Top - 0.25F, z + 0.05F}, Material::Rust));
        }
    }

    // 3. Wall rung ladder to Deck 7 East band (at X = 22.00F, Z = -148.50F)
    constexpr float kC2LadderX = 22.00F;
    constexpr float kC2LadderZ = -148.50F;
    constexpr float kC2LadderHalfW = 0.28F;
    for (float y = 71.60F; y <= 76.80F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(kC2LadderHalfW, 0.02F, 0.02F),
                            JPH::Vec3(kC2LadderX, y, kC2LadderZ), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        const float sx = kC2LadderX + side * kC2LadderHalfW;
        route.push_back(span({sx - 0.03F, 71.30F, kC2LadderZ - 0.03F},
                             {sx + 0.03F, kDeck7Top, kC2LadderZ + 0.03F}, Material::Yellow));
    }
    // Vertical fascia plate under Deck 7 edge (north rim of equipment hatch):
    route.push_back(span({kC2LadderX - 0.60F, kDeck7Top - 1.20F, -148.70F},
                         {kC2LadderX + 0.60F, kDeck7Top, -148.65F}, Material::Steel));
    // Deck 7 top-out plate on deck floor (from fascia into deck towards -Z):
    route.push_back(span({kC2LadderX - 0.60F, kDeck7Top, -150.50F},
                         {kC2LadderX + 0.60F, kDeck7Top + 0.03F, -148.65F}, Material::Steel));

    // 4. Deck 7 pipe rack & monorail beam
    // Pipe rack mantle platform at Y = 78.75F covering X in [18.00F, 22.50F], Z in [-162.50F, -160.50F]:
    route.push_back(span({18.00F, kDeck7Top, -162.50F}, {22.50F, 78.75F, -160.50F},
                         Material::Steel));
    route.push_back(span({17.95F, 78.67F, -160.55F}, {22.55F, 78.75F, -160.50F},
                         Material::Hazard));

    // Stepped monorail beam rising from Y = 78.75F at X = 18.00F up to Y = 82.00F at X = 13.00F:
    // 10 steps of <= 0.30 m each, 0.90 m wide in Z (Z in [-162.45F, -161.55F]):
    constexpr float kBeamZ0 = -162.45F;
    constexpr float kBeamZ1 = -161.55F;
    const struct Step { float x0, x1, y; } kSteps[] = {
        {17.50F, 18.00F, 79.05F},
        {17.00F, 17.50F, 79.35F},
        {16.50F, 17.00F, 79.65F},
        {16.00F, 16.50F, 79.95F},
        {15.50F, 16.00F, 80.25F},
        {15.00F, 15.50F, 80.55F},
        {14.50F, 15.00F, 80.85F},
        {14.00F, 14.50F, 81.15F},
        {13.50F, 14.00F, 81.45F},
        {13.00F, 13.50F, 81.75F},
    };
    for (const auto &s : kSteps) {
        route.push_back(span({s.x0, 77.00F, kBeamZ0}, {s.x1, s.y, kBeamZ1}, Material::Yellow));
    }
    // Level balance beam at Y = 82.00F from X = 13.00F out into the atrium to X = 9.80F:
    route.push_back(span({9.80F, 81.50F, kBeamZ0}, {13.00F, 82.00F, kBeamZ1}, Material::Yellow));
    route.push_back(span({9.75F, 81.50F, kBeamZ0 - 0.05F}, {10.00F, 82.05F, kBeamZ1 + 0.05F},
                         Material::Hazard));

    // 5. Davit hanging ladder from Deck 8 (arm at 88.00F to 89.20F)
    constexpr float kC2DavitX = 10.00F;
    constexpr float kC2DavitLadderZ = -162.40F;
    constexpr float kC2DavitLadderHalfW = 0.28F;
    constexpr float kC2DavitArmTop = 89.20F; // 1.20 m above Deck 8 floor (88.00F), embedding shaft rail at 89.05F
    constexpr float kC2DavitArmBottom = 87.70F;

    // Davit arm extending north from Z = -162.55F over the shaft rail at Z = -167.00F to Z = -167.50F:
    route.push_back(span({kC2DavitX - 0.45F, kC2DavitArmBottom, -167.50F},
                         {kC2DavitX + 0.45F, kC2DavitArmTop, -162.55F}, Material::Yellow));

    // Steps down from davit arm (89.20F) to Deck 8 North band floor (88.00F):
    // Step 1: 88.85F (-0.35)
    route.push_back(span({kC2DavitX - 0.50F, kDeck8Top - 0.05F, -168.25F},
                         {kC2DavitX + 0.50F, 88.85F, -167.50F}, Material::Yellow));
    // Step 2: 88.50F (-0.35)
    route.push_back(span({kC2DavitX - 0.50F, kDeck8Top - 0.05F, -169.00F},
                         {kC2DavitX + 0.50F, 88.50F, -168.25F}, Material::Yellow));
    // Step 3: 88.15F (-0.35)
    route.push_back(span({kC2DavitX - 0.50F, kDeck8Top - 0.05F, -169.75F},
                         {kC2DavitX + 0.50F, 88.15F, -169.00F}, Material::Yellow));
    // Runway landing plate on Deck 8 floor:
    route.push_back(span({kC2DavitX - 0.60F, kDeck8Top - 0.05F, -170.50F},
                         {kC2DavitX + 0.60F, kDeck8Top + 0.03F, -169.75F}, Material::Yellow));

    // Davit hanging ladder rungs from 84.30F up to kC2DavitArmTop - 0.05F (89.15F):
    for (float y = 84.30F; y <= kC2DavitArmTop - 0.05F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(kC2DavitLadderHalfW, 0.02F, 0.02F),
                            JPH::Vec3(kC2DavitX, y, kC2DavitLadderZ), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        const float sx = kC2DavitX + side * kC2DavitLadderHalfW;
        route.push_back(span({sx - 0.03F, 84.20F, kC2DavitLadderZ - 0.03F},
                             {sx + 0.03F, kC2DavitArmTop, kC2DavitLadderZ + 0.03F}, Material::Yellow));
        const float grab_x = kC2DavitX + side * 0.45F;
        route.push_back(span({grab_x - 0.03F, kC2DavitArmTop, kC2DavitLadderZ - 0.03F},
                             {grab_x + 0.03F, kC2DavitArmTop + 0.90F, kC2DavitLadderZ + 0.03F}, Material::Yellow));
    }

    build_c2_traversal_enhancements(route);
}

// ---- S3, the brake-override counterweight hoist (archetype 17) --------------
//
// Bridges Deck 8 (88.0 m) to Deck 12 (132.0 m) in the North central shaft.
// An overloaded 3,500 kg freight car at Deck 12 is held by a brake caliper /
// crowbar catch; the rider boards a 400 kg counterweight carriage at Deck 8,
// pulls the trip handle, the brake trips, and the car falls 44 m under governor
// (<= 2.6 m/s), hoisting the player carriage 44 m up to Deck 12!

constexpr float kDeck12Top = 132.00F;
constexpr float kS3Travel = 44.00F;

constexpr float kS3CageX = -8.00F;
constexpr float kS3CageZ = -158.00F;
constexpr float kS3CageHalfX = 1.50F;
constexpr float kS3CageHalfZ = 1.40F;
constexpr float kS3CageFloorHalfY = 0.10F;
constexpr float kS3CageFloorTop = kDeck8Top + 0.05F; // 88.05 m
constexpr float kS3CageOriginY = kS3CageFloorTop - kS3CageFloorHalfY;
constexpr float kS3CageMassKg = 400.0F;
constexpr float kS3CageGovernorSpeed = 2.50F;
constexpr float kS3CageGovernorForce = 45000.0F;
constexpr float kS3CageLevelAccel = 2.00F;
const JPH::Vec3 kS3CageEyeLocal(-kS3CageHalfX - 0.12F, 0.80F, 0.0F);

constexpr float kS3CarX = -8.00F;
constexpr float kS3CarZ = -146.00F;
constexpr float kS3CarHalfX = 1.60F;
constexpr float kS3CarHalfZ = 1.50F;
constexpr float kS3CarFloorHalfY = 0.10F;
constexpr float kS3CarFloorTop = kDeck12Top + 0.05F; // 132.05 m
constexpr float kS3CarOriginY = kS3CarFloorTop - kS3CarFloorHalfY;
constexpr float kS3CarMassKg = 3500.0F;
const JPH::Vec3 kS3CarEyeLocal(-kS3CarHalfX - 0.12F, 0.80F, 0.0F);

constexpr float kS3HeadSheaveY = kDeck12Top + 3.00F; // 135.00 m
const JPH::RVec3 kS3BrakePivot(kS3CarX, kDeck12Top + 2.50, kS3CarZ + 0.80);
constexpr float kS3BrakeArm = 1.80F;
constexpr float kS3BrakeMassKg = 40.0F;
constexpr float kS3BrakeTravel = 0.60F;
constexpr float kS3BrakeRelease = 0.15F;

void build_north_crossover_gangway(std::vector<Part> &frame, const float deck_top, const float center_x) {
    const float half_x = 1.40F;
    // ShaftRail at North perimeter is at Z = -167.00.
    // Stepping south (increasing Z) from -170.00 to -167.00 over the rail:
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -170.00F},
                         {center_x + half_x, deck_top + 0.28F, -169.25F}, Material::Galvanised));
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -169.25F},
                         {center_x + half_x, deck_top + 0.56F, -168.50F}, Material::Galvanised));
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -168.50F},
                         {center_x + half_x, deck_top + 0.84F, -167.75F}, Material::Galvanised));
    // Bridge platform over rail at -167.00:
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -167.75F},
                         {center_x + half_x, deck_top + 1.18F, -166.25F}, Material::Galvanised));
    // Stepping down south:
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -166.25F},
                         {center_x + half_x, deck_top + 0.84F, -165.50F}, Material::Galvanised));
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -165.50F},
                         {center_x + half_x, deck_top + 0.56F, -164.75F}, Material::Galvanised));
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -164.75F},
                         {center_x + half_x, deck_top + 0.28F, -164.00F}, Material::Galvanised));
    // Runway to cage sill at -159.45F (cage center is -158.00F, half Z is 1.40F -> north edge -159.40F):
    frame.push_back(span({center_x - half_x, deck_top - 0.10F, -164.00F},
                         {center_x + half_x, deck_top, -159.45F}, Material::Galvanised));

    for (const float side : {-1.0F, 1.0F}) {
        const float rail_x = center_x + side * (half_x - 0.04F);
        frame.push_back(span({rail_x - 0.04F, deck_top + 1.05F, -164.00F},
                             {rail_x + 0.04F, deck_top + 1.13F, -159.45F}, Material::Yellow));
        frame.push_back(span({rail_x - 0.04F, deck_top + 2.15F, -170.00F},
                             {rail_x + 0.04F, deck_top + 2.23F, -164.00F}, Material::Yellow));
    }
}

[[nodiscard]] std::vector<Part> s3_cage_parts() {
    const float post_half = 0.5F * kCagePostHeight;
    const float post_y = kS3CageFloorHalfY + post_half;
    const float top_y = kS3CageFloorHalfY + kCagePostHeight;
    std::vector<Part> cage{
        box(JPH::Vec3(kS3CageHalfX, kS3CageFloorHalfY, kS3CageHalfZ), JPH::Vec3::sZero(),
            Material::Galvanised),
    };
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            cage.push_back(box(JPH::Vec3(0.05F, post_half, 0.05F),
                               JPH::Vec3(sx * (kS3CageHalfX - 0.05F), post_y, sz * (kS3CageHalfZ - 0.05F)),
                               Material::Yellow));
        }
        cage.push_back(box(JPH::Vec3(kS3CageHalfX, 0.06F, 0.05F),
                           JPH::Vec3(0.0F, top_y, sx * (kS3CageHalfZ - 0.05F)), Material::Yellow));
        cage.push_back(box(JPH::Vec3(0.05F, 0.06F, kS3CageHalfZ),
                           JPH::Vec3(sx * (kS3CageHalfX - 0.05F), top_y, 0.0F), Material::Yellow));
        cage.push_back(box(JPH::Vec3(0.04F, 0.45F, kS3CageHalfZ - 0.10F),
                           JPH::Vec3(sx * (kS3CageHalfX - 0.04F), 0.55F, 0.0F), Material::Galvanised));
    }
    cage.push_back(box(JPH::Vec3(0.12F, 0.06F, 0.06F),
                       JPH::Vec3(-(kS3CageHalfX + 0.06F), kS3CageEyeLocal.GetY(), 0.0F),
                       Material::Hazard));
    return cage;
}

[[nodiscard]] std::vector<Part> s3_car_parts() {
    const float wall_y = kS3CarFloorHalfY + 0.60F;
    std::vector<Part> car{
        box(JPH::Vec3(kS3CarHalfX, kS3CarFloorHalfY, kS3CarHalfZ), JPH::Vec3::sZero(), Material::Steel),
        // Heavy pig-iron & concrete ballast blocks:
        box(JPH::Vec3(kS3CarHalfX - 0.15F, 0.50F, kS3CarHalfZ - 0.15F),
            JPH::Vec3(0.0F, kS3CarFloorHalfY + 0.50F, 0.0F), Material::Concrete),
        box(JPH::Vec3(kS3CarHalfX - 0.20F, 0.15F, kS3CarHalfZ - 0.20F),
            JPH::Vec3(0.0F, kS3CarFloorHalfY + 1.15F, 0.0F), Material::Rust),
    };
    for (const float sx : {-1.0F, 1.0F}) {
        car.push_back(box(JPH::Vec3(kS3CarHalfX, 0.60F, 0.05F),
                          JPH::Vec3(0.0F, wall_y, sx * (kS3CarHalfZ - 0.05F)), Material::Rust));
        car.push_back(box(JPH::Vec3(0.05F, 0.60F, kS3CarHalfZ - 0.10F),
                          JPH::Vec3(sx * (kS3CarHalfX - 0.05F), wall_y, 0.0F), Material::Rust));
    }
    car.push_back(box(JPH::Vec3(0.12F, 0.06F, 0.06F),
                      JPH::Vec3(-(kS3CarHalfX + 0.06F), kS3CarEyeLocal.GetY(), 0.0F),
                      Material::Hazard));
    return car;
}

void build_s3_frame(std::vector<Part> &frame) {
    build_north_crossover_gangway(frame, kDeck8Top, kS3CageX);
    build_north_crossover_gangway(frame, kDeck12Top, kS3CageX);

    // Landing buffer beams under cage sill at Deck 8:
    const float s3_cage_sill = kS3CageOriginY - kS3CageFloorHalfY - 0.05F;
    frame.push_back(span(JPH::Vec3(kS3CageX - kS3CageHalfX - 0.05F, s3_cage_sill - 0.40F, kS3CageZ - kS3CageHalfZ - 0.05F),
                         JPH::Vec3(kS3CageX + kS3CageHalfX + 0.05F, s3_cage_sill, kS3CageZ + kS3CageHalfZ + 0.05F),
                         Material::Steel));

    // Landing buffer beams under car sill at Deck 8 (car lands at 88.05 m after 44 m fall):
    const float s3_car_land_y = (kS3CarOriginY - kS3Travel) - kS3CarFloorHalfY - 0.05F;
    frame.push_back(span(JPH::Vec3(kS3CarX - kS3CarHalfX - 0.05F, s3_car_land_y - 0.40F, kS3CarZ - kS3CarHalfZ - 0.05F),
                         JPH::Vec3(kS3CarX + kS3CarHalfX + 0.05F, s3_car_land_y, kS3CarZ + kS3CarHalfZ + 0.05F),
                         Material::Steel));

    // Vertical guide rails for cage and car:
    const float mid_y = 0.5F * (kDeck8Top + kDeck12Top + 3.0F);
    const float half_y = 0.5F * (kDeck12Top + 3.0F - kDeck8Top);
    for (const float sz : {-1.0F, 1.0F}) {
        frame.push_back(box(JPH::Vec3(0.04F, half_y, 0.10F),
                            JPH::Vec3(kS3CageX + kS3CageHalfX + 0.16F, mid_y, kS3CageZ + sz * 0.60F),
                            Material::Steel));
        frame.push_back(box(JPH::Vec3(0.04F, half_y, 0.10F),
                            JPH::Vec3(kS3CarX + kS3CarHalfX + 0.16F, mid_y, kS3CarZ + sz * 0.60F),
                            Material::Steel));
    }

    // Headframe at Deck 12 crown (Y = 135.0 to 136.0):
    frame.push_back(span(JPH::Vec3(kS3CageX - 2.0F, kS3HeadSheaveY + 0.20F, kS3CageZ - 1.0F),
                         JPH::Vec3(kS3CageX + 1.0F, kS3HeadSheaveY + 0.60F, kS3CarZ + 1.0F), Material::Yellow));
    frame.push_back(box(JPH::Vec3(0.20F, 0.20F, 0.10F),
                        JPH::Vec3(kS3CageX + kS3CageEyeLocal.GetX(), kS3HeadSheaveY, kS3CageZ), Material::Hazard));
    frame.push_back(box(JPH::Vec3(0.20F, 0.20F, 0.10F),
                        JPH::Vec3(kS3CarX + kS3CarEyeLocal.GetX(), kS3HeadSheaveY, kS3CarZ), Material::Hazard));
}

void build_s3(kit::Kit &kit, Stack &stack, std::vector<Part> &frame) {
    using Sim = Simulation;

    // ---- The counterweight cage (player rides) ------------------------------
    stack.s3_cage = kit.add_body(Sim::kStackS3CageEntityId, s3_cage_parts(),
                                 JPH::RVec3(kS3CageX, kS3CageOriginY, kS3CageZ), JPH::Quat::sIdentity(),
                                 kS3CageMassKg, 0.9F);
    stack.s3_cage_guide = kit.add_guide(stack.s3_cage, JPH::Vec3::sAxisY(), 0.0F, kS3Travel,
                                        kS3CageGovernorSpeed, kS3CageGovernorForce, kS3CageLevelAccel);

    // ---- The freight car (counter-mass) -------------------------------------
    stack.s3_car = kit.add_body(Sim::kStackS3CarEntityId, s3_car_parts(),
                                JPH::RVec3(kS3CarX, kS3CarOriginY, kS3CarZ), JPH::Quat::sIdentity(),
                                kS3CarMassKg, 0.5F);
    stack.s3_car_guide = kit.add_guide(stack.s3_car, JPH::Vec3::sAxisY(), -kS3Travel, 0.0F,
                                       0.0F, 0.0F, 0.0F);

    // ---- The 1:1 suspension rope -------------------------------------------
    const JPH::RVec3 eye_cage =
        JPH::RVec3(kS3CageX, kS3CageOriginY, kS3CageZ) + JPH::RVec3(kS3CageEyeLocal);
    const JPH::RVec3 eye_car =
        JPH::RVec3(kS3CarX, kS3CarOriginY, kS3CarZ) + JPH::RVec3(kS3CarEyeLocal);
    const JPH::RVec3 cage_sheave(eye_cage.GetX(), kS3HeadSheaveY, eye_cage.GetZ());
    const JPH::RVec3 car_sheave(eye_car.GetX(), kS3HeadSheaveY, eye_car.GetZ());
    const float rope_length =
        JPH::Vec3(eye_car - car_sheave).Length() + JPH::Vec3(eye_cage - cage_sheave).Length();

    stack.s3_rope = kit.add_rope(stack.s3_car, kS3CarEyeLocal, car_sheave,
                                 stack.s3_cage, kS3CageEyeLocal, cage_sheave, 1.0F, rope_length, 0.0F);

    // ---- The brake lever, catch, trip line, and lanyard handle -------------
    stack.s3_brake_body = kit.add_body(
        Sim::kStackS3BrakeEntityId,
        {box(JPH::Vec3(0.5F * kS3BrakeArm, 0.06F, 0.05F), JPH::Vec3(0.5F * kS3BrakeArm, 0.0F, 0.0F),
             Material::Hazard),
         box(JPH::Vec3(0.20F, 0.15F, 0.15F), JPH::Vec3(1.00F, 0.0F, 0.0F), Material::Rust)},
        kS3BrakePivot, JPH::Quat::sIdentity(), kS3BrakeMassKg, 0.5F);
    stack.s3_brake_lever = kit.add_lever(stack.s3_brake_body, kS3BrakePivot, JPH::Vec3::sAxisZ(),
                                         JPH::Vec3::sAxisX(), 0.0F, kS3BrakeTravel);
    stack.s3_catch = kit.add_catch(stack.s3_car, stack.s3_brake_lever, kS3BrakeRelease, 0.05F, true);
    stack.s3_cage_catch = kit.add_catch(stack.s3_cage, stack.s3_brake_lever, kS3BrakeRelease, 0.05F, true);

    const JPH::RVec3 handle_top(kS3CageX, kS3CageFloorTop + 1.95F, kS3CageZ);
    stack.s3_handle = add_chain_handle(kit, Sim::kStackS3HandleEntityId, handle_top);
    kit.set_damping(stack.s3_handle, 8.0F, 8.0F);

    const JPH::RVec3 trip_sheave1(kS3BrakePivot.GetX() + kS3BrakeArm, kS3BrakePivot.GetY() + 0.60,
                                  kS3BrakePivot.GetZ());
    const JPH::RVec3 trip_sheave2(kS3CageX, kDeck12Top + 3.00, kS3CageZ);
    (void)kit.add_trip_line(stack.s3_brake_body, JPH::Vec3(kS3BrakeArm, 0.0F, 0.0F), stack.s3_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), trip_sheave1, trip_sheave2);

    build_s3_frame(frame);
}

// ---- C3, the Crown Trusses & High Riser Ladder (132 -> 154 m) ---------------
//
// Climbs from Deck 12's North band to Deck 14's South band (Deck154 at +154 m).
// An inclined knee-brace box girder (19 deg slope to 135.5 m catwalk),
// an atrium duct (mantle at 137.2 m), wall rungs to Deck 13 (+143 m),
// and a high vertical riser ladder (11 m of rungs, 143.4 to 154.0 m)
// topping out directly at InitialSpawn::Deck154 (-10.5, 155.0, -128.2)!

constexpr float kDeck13Top = 143.00F;
constexpr float kDeck14Top = 154.00F;

void build_c3(std::vector<Part> &route) {
    constexpr float kC3X = -10.50F;
    constexpr float kWalkHalfW = 0.60F;

    // 1. Stepped incline girder from Deck 12 North band (Z = -170.0 to -145.0, Y = 132.0 to 135.5)
    // Step 0: gentle step up from Deck 12 floor (132.00F)
    route.push_back(span({kC3X - kWalkHalfW, kDeck12Top - 0.10F, -170.00F},
                         {kC3X + kWalkHalfW, 132.25F, -169.00F}, Material::Steel));
    // Step 1:
    route.push_back(span({kC3X - kWalkHalfW, kDeck12Top - 0.10F, -169.00F},
                         {kC3X + kWalkHalfW, 132.60F, -168.00F}, Material::Steel));
    // Step 2:
    route.push_back(span({kC3X - kWalkHalfW, kDeck12Top - 0.10F, -168.00F},
                         {kC3X + kWalkHalfW, 132.95F, -167.20F}, Material::Steel));
    // Step 3: Bridge over ShaftRail at Z = -167.00F (rail at Y = 133.05F, top 133.09F)
    route.push_back(span({kC3X - kWalkHalfW, kDeck12Top - 0.10F, -167.20F},
                         {kC3X + kWalkHalfW, 133.30F, -165.50F}, Material::Steel));

    // Steps 4 through 11 (8 steps): rising from 133.30F to 135.50F over Z in [-165.50F, -145.00F]
    for (int k = 0; k < 8; ++k) {
        const float z0 = -165.50F + static_cast<float>(k) * (20.50F / 8.00F);
        const float z1 = -165.50F + static_cast<float>(k + 1) * (20.50F / 8.00F);
        const float y = 133.30F + static_cast<float>(k + 1) * (2.20F / 8.00F);
        route.push_back(span({kC3X - kWalkHalfW, kDeck12Top - 0.10F, z0},
                             {kC3X + kWalkHalfW, y, z1}, Material::Steel));
    }
    // Landing platform at Y = 135.50F (Z in [-145.50F, -144.00F]):
    route.push_back(span({kC3X - kWalkHalfW - 0.20F, 135.40F, -145.50F},
                         {kC3X + kWalkHalfW + 0.20F, 135.50F, -144.00F}, Material::Galvanised));

    // Continuous handrails along stepped walkway:
    for (const float side : {-1.0F, 1.0F}) {
        const float rx = kC3X + side * (kWalkHalfW - 0.04F);
        route.push_back(span({rx - 0.04F, 133.40F, -169.00F},
                             {rx + 0.04F, 134.30F, -165.50F}, Material::Yellow));
        route.push_back(span({rx - 0.04F, 134.30F, -165.50F},
                             {rx + 0.04F, 136.50F, -144.00F}, Material::Yellow));
    }

    // 2. Atrium ventilation duct (mantle rise 1.70F from catwalk)
    route.push_back(span({-11.80F, 135.80F, -144.00F}, {-9.20F, 137.20F, -135.55F},
                         Material::Galvanised));
    route.push_back(span({-11.85F, 137.12F, -144.05F}, {-9.15F, 137.20F, -144.00F},
                         Material::Hazard));
    // Guard edges on duct sides
    route.push_back(span({-11.84F, 137.20F, -144.00F}, {-11.76F, 137.60F, -135.55F}, Material::Yellow));
    route.push_back(span({-9.24F, 137.20F, -144.00F}, {-9.16F, 137.60F, -135.55F}, Material::Yellow));

    // 3. Wall ladder to Deck 13 (at X = -10.50F, Z = -135.50F)
    constexpr float kC3LadderX = -10.50F;
    constexpr float kC3LadderZ = -135.50F;
    constexpr float kC3LadderHalfW = 0.28F;
    for (float y = 137.80F; y <= 142.80F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(kC3LadderHalfW, 0.02F, 0.02F),
                            JPH::Vec3(kC3LadderX, y, kC3LadderZ), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        const float sx = kC3LadderX + side * kC3LadderHalfW;
        route.push_back(span({sx - 0.03F, 137.50F, kC3LadderZ - 0.03F},
                             {sx + 0.03F, kDeck13Top, kC3LadderZ + 0.03F}, Material::Yellow));
    }
    // Vertical fascia plate under Deck 13 edge:
    route.push_back(span({-11.50F, kDeck13Top - 1.20F, -135.35F},
                         {-5.00F, kDeck13Top, -135.30F}, Material::Steel));
    // Deck 13 walkway plate:
    route.push_back(span({-11.50F, kDeck13Top, -135.35F},
                         {-5.00F, kDeck13Top + 0.03F, -133.00F}, Material::Steel));

    // 4. High riser ladder and Deck 14 crossover bridge (143.4 to 155.18 m)
    // Placed at X = -6.00F to clear Stage A's cage and guide rails (X in [-12.0, -9.0]).
    // ShaftRail at Deck 14 South perimeter is at Z = -133.00F, Y = 155.05F.
    // Crossover bridge tops at 155.18F to clear/embed the rail, then steps down to 154.00F.
    constexpr float kC3HighLadderX = -6.00F;
    constexpr float kC3HighLadderZ = -133.50F;
    constexpr float kC3BridgeTop = 155.18F;

    for (float y = 143.40F; y <= kC3BridgeTop - 0.03F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(kC3LadderHalfW, 0.02F, 0.02F),
                            JPH::Vec3(kC3HighLadderX, y, kC3HighLadderZ), Material::Steel));
    }
    for (const float side : {-1.0F, 1.0F}) {
        const float sx = kC3HighLadderX + side * kC3LadderHalfW;
        route.push_back(span({sx - 0.03F, 143.00F, kC3HighLadderZ - 0.03F},
                             {sx + 0.03F, kC3BridgeTop, kC3HighLadderZ + 0.03F}, Material::Yellow));
        const float grab_x = kC3HighLadderX + side * 0.45F;
        route.push_back(span({grab_x - 0.03F, kC3BridgeTop, kC3HighLadderZ - 0.03F},
                             {grab_x + 0.03F, kC3BridgeTop + 0.90F, kC3HighLadderZ + 0.03F}, Material::Yellow));
    }
    // Vertical fascia plate under bridge north lip:
    route.push_back(span({kC3HighLadderX - 0.70F, kDeck14Top, -133.55F},
                         {kC3HighLadderX + 0.70F, kC3BridgeTop, -133.50F}, Material::Steel));
    // Bridge platform over ShaftRail (Z = -133.00F):
    route.push_back(span({kC3HighLadderX - 0.70F, kDeck14Top - 0.05F, -133.50F},
                         {kC3HighLadderX + 0.70F, kC3BridgeTop, -132.25F}, Material::Galvanised));

    // Steps down south into Deck 14 South perimeter band:
    // Step 1: 154.85F (-0.33)
    route.push_back(span({kC3HighLadderX - 0.70F, kDeck14Top - 0.05F, -132.25F},
                         {kC3HighLadderX + 0.70F, 154.85F, -131.50F}, Material::Galvanised));
    // Step 2: 154.50F (-0.35)
    route.push_back(span({kC3HighLadderX - 0.70F, kDeck14Top - 0.05F, -131.50F},
                         {kC3HighLadderX + 0.70F, 154.50F, -130.75F}, Material::Galvanised));
    // Step 3: 154.15F (-0.35)
    route.push_back(span({kC3HighLadderX - 0.70F, kDeck14Top - 0.05F, -130.75F},
                         {kC3HighLadderX + 0.70F, 154.15F, -130.00F}, Material::Galvanised));
    // Runway landing plate on Deck 14 floor:
    route.push_back(span({kC3HighLadderX - 0.80F, kDeck14Top - 0.05F, -130.00F},
                         {kC3HighLadderX + 0.80F, kDeck14Top + 0.03F, -127.50F}, Material::Yellow));
}

} // namespace

void build_stack(kit::Kit &kit, Stack &stack) {
    using Sim = Simulation;
    std::vector<Part> frame;
    build_s1(kit, stack, frame);
    build_s2(kit, stack, frame);
    build_s3(kit, stack, frame);
    (void)kit.add_body(Sim::kStackFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);

    std::vector<Part> route;
    build_c1(route);
    (void)kit.add_body(Sim::kStackRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);

    std::vector<Part> c2_route;
    build_c2(c2_route);
    (void)kit.add_body(Sim::kStackC2RouteEntityId, c2_route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);

    std::vector<Part> c3_route;
    build_c3(c3_route);
    (void)kit.add_body(Sim::kStackC3RouteEntityId, c3_route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);
}

} // namespace scraperx::sim::bands
