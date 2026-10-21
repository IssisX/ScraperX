#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>
#include <vector>

// AS-007, Wet Isolation (Atlas band B03, 220 -> 340 m). Contract:
// 03_EXECUTION/ASCENT/AS-007_WET_ISOLATION.md. Three stages in the well's
// north-east quarter, each on water or air: D, a float ram fed from a head
// tank; E, a cab blown up its shaft on the air a falling chiller pushes,
// set off by the rider dropping onto its deck; F, a weight-loaded hydraulic
// accumulator. A header on TP-340 re-arms them.
//
// Every body keeps south of each ring's north inner edge and west of its
// east inner edge at the heights it reaches, so nothing rising meets a ring.

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;
using Sim = Simulation;

constexpr float kWall = 0.15F;

Part box(const JPH::Vec3 half, const JPH::Vec3 center, const Material material) {
    return {half, center, JPH::Quat::sIdentity(), material};
}

// A box between two corners.
Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (high + low), material);
}

// Four walls round an inner box, from y0 to y1.
void add_walls(std::vector<Part> &parts, const float x0, const float x1, const float z0,
               const float z1, const float y0, const float y1, const Material material) {
    parts.push_back(span({x0 - kWall, y0, z0 - kWall}, {x1 + kWall, y1, z0}, material));
    parts.push_back(span({x0 - kWall, y0, z1}, {x1 + kWall, y1, z1 + kWall}, material));
    parts.push_back(span({x0 - kWall, y0, z0}, {x0, y1, z1}, material));
    parts.push_back(span({x1, y0, z0}, {x1 + kWall, y1, z1}, material));
}

// ---- the columns ------------------------------------------------------------
constexpr float kColX = 13.0F;     // D, E's cab, F
constexpr float kWestX = 10.0F;    // E's duct, F's accumulator
constexpr float kBucketX = 7.1F;   // E's hoist bucket
constexpr float kDZ = -133.6F;
constexpr float kEZ = -136.65F;
constexpr float kFZ = -139.85F;
constexpr float kPlatformHalf = 1.3F;

// ---- Stage D ----------------------------------------------------------------
constexpr float kTubeHalf = 1.2F;          // inner
constexpr float kTubeFloor = 181.0F;
constexpr float kTubeOverflow = 219.0F;
constexpr float kTubeLidBottom = 219.8F;
constexpr float kTubeLidTop = 220.0F;
constexpr float kDDeckTop = 220.25F;
constexpr float kDTravel = 36.0F;          // 220.25 -> 256.25
constexpr float kDMassKg = 2500.0F;
constexpr float kFloatHalf = 1.1F;
constexpr float kFloatHeight = 2.0F;
constexpr float kFloatBottom = -39.0F;     // below the deck's top
constexpr float kMastHalf = 0.15F;
// The head tank, hung from the 264 ring, and the pipe along the 220 ring.
const JPH::Vec3 kTankMin(5.65F, 227.0F, -135.0F);
const JPH::Vec3 kTankMax(11.3F, 243.0F, -132.2F);
constexpr float kTankWaterKg = 250000.0F;
constexpr float kFillArea = 0.5F;
constexpr float kPipeY = 220.47F;         // lying on the ring's deck
constexpr float kPipeZ = -127.5F;
constexpr float kPipeHalf = 0.22F;
// Where the pipe crosses the walkway to the platform, its gap in the middle
// and its drop into the well west of the walkway.
constexpr float kCrossZ = -131.8F;
constexpr float kGapWest = 12.2F;
constexpr float kGapEast = 13.8F;
constexpr float kDownX = 11.2F;
// The spool's square end flanges, which it lies on: 0.25 m, so that hung 0.6 m
// ahead of the body (kCarryHandForward) they clear its 0.3 m capsule and the
// spool hangs level instead of leaning back on the chest (at 0.3 m it did,
// rolled 40 degrees, and dropped from there it landed a flange on whatever
// lay under it). Square, it never rolls off where it is set down.
constexpr float kSpoolFlange = 0.25F;
const JPH::RVec3 kSpoolSeat(13.0, 220.47, kCrossZ);
// Where the fill valve's bar and its handle were (D was started by a pull
// until 2026-10-19). Their bodies are kept, parked, for the save's order.
const JPH::RVec3 kDValvePivot(10.2, 221.2, -131.4);
const JPH::RVec3 kDValveSheave2(12.3, 222.2, -131.9);
// A throw bar (the header's dump).
constexpr float kBarLength = 1.4F;
constexpr float kBarLug = 0.5F;
constexpr float kBarFarStop = 2.2F;
// The drain: a counterweighted lever held open by its handle.
const JPH::RVec3 kDDrainPivot(15.3, 223.5, -131.0);
const JPH::RVec3 kDDrainSheave(14.3, 222.2, -131.0);
constexpr float kDrainArea = 0.3F;

// ---- Stage E ----------------------------------------------------------------
// 03_EXECUTION/CLIMB_REDESIGN/chiller_drop_spec.md. The rider walks through
// the doors off D's platform onto no floor and drops 2.85 m onto the deck,
// which hangs on the chiller's trip line 0.15 m proud of the cab's plate. The
// blow carries the deck down onto the plate and turns the trip lever past its
// release; the chiller falls in its duct and its air blows the cab up the
// shaft and out of its mouth, where the vent collar lets the air out and the
// cab settles on what it has left, level with F's platform.
constexpr float kShaftInnerHalf = 1.35F;
constexpr float kCabPlenumFloor = 249.5F;
constexpr float kDuctFloor = 247.75F;
constexpr float kECabTop = 253.25F;        // the cab's plate, as found
constexpr float kECabPlate = 0.10F;
constexpr float kSkirt = 1.75F;            // the sealing edge, under the plate's top
constexpr float kSkirtWall = 0.04F;
constexpr float kETravel = 46.5F;          // the hard top: the skirt's edge at the rim
constexpr float kECabMassKg = 900.0F;
constexpr float kShaftRim = 298.0F;
constexpr float kDuctTop = 304.2F;
constexpr float kChillerHalfY = 1.25F;
constexpr float kChillerBottom = 301.5F;
constexpr float kChillerTravel = 50.0F;
constexpr float kChillerMassKg = 5000.0F;
constexpr float kPistonArea = 4.0F * kShaftInnerHalf * kShaftInnerHalf;   // 7.29 m^2
constexpr float kPlenumExtra = 0.5F;       // the throttle's duct, m^3
constexpr float kThrottleArea = 0.42F;
constexpr float kBreakerArea = 0.3F;
constexpr float kBleedArea = 0.0005F;
// Declared (seal friction): the skirt's seal drags on the shaft's walls.
constexpr float kSealFrictionN = 2500.0F;
// Declared (vent collar): slots round the shaft's mouth, open to the skirt's
// edge once it rises past kVentFrom. Its lower edge sets where the cab comes
// to rest; its area, how far it overshoots.
constexpr float kVentFrom = 296.85F;
constexpr float kVentPerM = 3.0F;
// The glass in the shaft's west and east walls and the duct's east wall.
constexpr float kGlassFrom = 258.0F;
constexpr float kGlassTo = 296.0F;
constexpr float kDoorWest = 12.4F;
constexpr float kDoorEast = 13.6F;
constexpr float kDoorBottom = 256.3F;
constexpr float kDoorTop = 258.5F;
// The doors: two light leaves on hinges leaning top outward, so each swings
// shut on its own weight.
constexpr float kLeafMassKg = 35.0F;
constexpr float kHingeLean = 8.0F * JPH::JPH_PI / 180.0F;
constexpr float kLeafOpen = 1.35F;         // rad, the stop
constexpr float kLeafGap = 0.05F;          // off each jamb
constexpr float kLeafShutRad = 0.05F;      // a leaf turned less than this is shut
// The deck the rider drops onto, hung on the trip line over the cab's plate.
// 2.46 m square: the cab's corner posts stand 1.24 m out from its middle.
constexpr float kDeckProud = 0.15F;
constexpr float kDeckHalf = 1.23F;
constexpr float kDeckThick = 0.05F;
constexpr float kDeckMassKg = 60.0F;
const JPH::Vec3 kDeckEye(1.15F, 0.03F, 1.15F);
// The trip lever: the old lever's two parts, its eye 0.25 m out on the arm.
constexpr float kELeverEye = 0.25F;
constexpr float kELeverArmKg = 4.0F;
constexpr float kELeverBlockKg = 151.0F;
const JPH::RVec3 kELeverPivot(10.0, 305.3, -134.8);
const JPH::RVec3 kESheave1(9.75, 304.6, -134.8);
const JPH::RVec3 kESheave2(14.15, 304.6, -135.5);
// The bucket, hung from the chiller over two sheaves above the duct.
constexpr float kBucketFloorY = 252.76F;   // its floor's center, as found
constexpr float kBucketMassKg = 300.0F;
constexpr float kBucketCapacityKg = 6000.0F;
constexpr float kBucketBasinY = 249.5F;
const JPH::RVec3 kESheaveDuct(kWestX, 306.2, kEZ);
const JPH::RVec3 kESheaveBucket(kBucketX, 306.2, kEZ);
// The striker's arm lies in the bucket's floor at the bottom of its travel, as
// a full bucket come down onto it leaves it: 3 cm under the floor's middle,
// so the floor pushes the arm down (its gate open) and never stands on it.
// (Level with the floor's middle, the push-out was a tie: 5 m lower than it
// was built first, it pushed the bucket down onto the arm's stop instead, and
// the bucket, and the chiller hung on it, could not move.)
const JPH::RVec3 kEStrikerPivot(kBucketX, kBucketFloorY - 0.03, -135.45);

// ---- Stage F ----------------------------------------------------------------
constexpr float kFDeckTop = 298.25F;
constexpr float kFTravel = 42.0F;          // 298.25 -> 340.25
constexpr float kFMassKg = 1000.0F;
constexpr float kAccumulatorMassKg = 20000.0F;
constexpr float kAccumulatorHalfY = 1.5F;
constexpr float kAccumulatorBottom = 302.0F;
constexpr float kAccumulatorStroke = 3.0F;
constexpr float kRamBase = 297.0F;
const JPH::Vec3 kFInletLocal(-1.1F, 0.85F, 0.9F);   // on the west rail, waist high
const JPH::RVec3 kFLeverPivot(8.6, 306.0, -141.35);
const JPH::RVec3 kFSheave1(7.6, 300.3, -141.35);
const JPH::RVec3 kFSheave2(12.25, 300.3, -141.35);

// ---- TP-340 and the header --------------------------------------------------
constexpr float kPlateBottom = 338.75F;
constexpr float kPlateTop = 340.25F;
const JPH::Vec3 kHeaderMin(2.0F, 340.45F, -152.0F);
const JPH::Vec3 kHeaderMax(10.0F, 345.0F, -143.0F);
constexpr float kHeaderWaterKg = 300000.0F;
// The dump's bar lies north of F's hole, where the rider steps off; its
// handle hangs west of the hole's north-west corner.
const JPH::RVec3 kDumpPivot(9.2, 341.2, -136.5);
const JPH::RVec3 kDumpSheave1(9.7, 343.2, -136.5);
const JPH::RVec3 kDumpSheave2(10.9, 342.2, -137.3);
const JPH::RVec3 kBucketSpout(kBucketX, 305.9, kEZ);

// Handles and levers, as AS-006 builds them.
constexpr float kHandleHalfY = 0.04F;
constexpr float kHandleDrop = 0.75F;
constexpr float kLeverArm = 1.0F;
constexpr float kLeverMassKg = 60.0F;
constexpr float kLeverRelease = 0.5F;
constexpr float kLeverTravel = 1.2F;

kit::BodyIndex add_handle(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 sheave,
                          const float drop) {
    const kit::BodyIndex handle = kit.add_body(
        entity,
        {box(JPH::Vec3(0.22F, kHandleHalfY, 0.04F), JPH::Vec3::sZero(), Material::Yellow)},
        sheave - JPH::RVec3(0.0, drop + kHandleHalfY, 0.0), JPH::Quat::sIdentity(), 3.0F, 0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kHandleHalfY, 0.0F));
    kit.set_damping(handle, 1.5F, 1.5F);
    return handle;
}

// A body a machine no longer has, kept so a save's bodies line up: static
// and parked out of the world, so a saved state for it is not taken back.
void add_retired(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 at) {
    kit.park(kit.add_body(entity, {box(JPH::Vec3::sReplicate(0.05F), JPH::Vec3::sZero(), Material::Steel)},
                          at, JPH::Quat::sIdentity(), 0.0F, 0.5F));
}

// A counterweighted trip lever about +z: its arm west, a block east, so its
// weight holds the arm up on its stop and a pull on the arm's eye turns it.
kit::BodyIndex add_trip_lever(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 pivot,
                              kit::LeverIndex &lever) {
    const kit::BodyIndex body = kit.add_body(
        entity,
        {box(JPH::Vec3(0.5F * kLeverArm, 0.05F, 0.05F), JPH::Vec3(-0.5F * kLeverArm, 0.0F, 0.0F),
             Material::Hazard),
         box(JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(0.35F, 0.0F, 0.0F), Material::Rust)},
        pivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    lever = kit.add_lever(body, pivot, JPH::Vec3::sAxisZ(), -JPH::Vec3::sAxisX(), 0.0F,
                          kLeverTravel);
    return body;
}

// A bar lying east of its pin about +z: a line from its lug lifts it over
// upright, and it falls on over to its far stop and stays.
kit::BodyIndex add_throw_bar(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 pivot,
                             kit::LeverIndex &lever) {
    const kit::BodyIndex body = kit.add_body(
        entity,
        {box(JPH::Vec3(0.5F * kBarLength, 0.035F, 0.035F), JPH::Vec3(0.5F * kBarLength, 0.0F, 0.0F),
             Material::Hazard)},
        pivot, JPH::Quat::sIdentity(), 10.0F, 0.6F);
    lever = kit.add_lever(body, pivot, JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), 0.0F,
                          kBarFarStop);
    return body;
}

// A platform about its deck's top centre: deck, side rails, and the rail
// on `closed_side` (+1 south, -1 north, 0 none) across one end.
std::vector<Part> platform_parts(const float closed_side) {
    std::vector<Part> parts{
        box(JPH::Vec3(kPlatformHalf, 0.1F, kPlatformHalf), JPH::Vec3(0.0F, -0.1F, 0.0F),
            Material::Galvanised)};
    for (const float side : {-1.0F, 1.0F}) {
        const float x = side * (kPlatformHalf - 0.03F);
        parts.push_back(box(JPH::Vec3(0.03F, 0.5F, 0.03F), JPH::Vec3(x, 0.5F, kPlatformHalf - 0.03F),
                            Material::Yellow));
        parts.push_back(box(JPH::Vec3(0.03F, 0.5F, 0.03F),
                            JPH::Vec3(x, 0.5F, -(kPlatformHalf - 0.03F)), Material::Yellow));
        parts.push_back(box(JPH::Vec3(0.03F, 0.03F, kPlatformHalf), JPH::Vec3(x, 1.0F, 0.0F),
                            Material::Yellow));
    }
    if (closed_side != 0.0F) {
        const float z = -closed_side * (kPlatformHalf - 0.03F);
        parts.push_back(box(JPH::Vec3(kPlatformHalf, 0.03F, 0.03F), JPH::Vec3(0.0F, 1.0F, z),
                            Material::Yellow));
    }
    return parts;
}

void build_stage_d(kit::Kit &kit, WetIsolation &wet, std::vector<Part> &frame) {
    // ---- the tube, its lid and gland, the walkway, the head tank ------------
    const float x0 = kColX - kTubeHalf;
    const float x1 = kColX + kTubeHalf;
    const float z0 = kDZ - kTubeHalf;
    const float z1 = kDZ + kTubeHalf;
    add_walls(frame, x0, x1, z0, z1, kTubeFloor - 0.2F, kTubeLidTop, Material::Rust);
    frame.push_back(span({x0, kTubeFloor - 0.2F, z0}, {x1, kTubeFloor, z1}, Material::Rust));
    const float gland = 0.2F;
    frame.push_back(span({x0, kTubeLidBottom, z0}, {kColX - gland, kTubeLidTop, z1}, Material::Steel));
    frame.push_back(span({kColX + gland, kTubeLidBottom, z0}, {x1, kTubeLidTop, z1}, Material::Steel));
    frame.push_back(span({kColX - gland, kTubeLidBottom, z0}, {kColX + gland, kTubeLidTop, kDZ - gland},
                         Material::Steel));
    frame.push_back(span({kColX - gland, kTubeLidBottom, kDZ + gland}, {kColX + gland, kTubeLidTop, z1},
                         Material::Steel));
    // The walkway from the 220 ring's edge to the platform's north edge.
    frame.push_back(span({kColX - kPlatformHalf, 220.05F, kDZ + kPlatformHalf + 0.05F},
                         {kColX + kPlatformHalf, kDDeckTop, -130.73F}, Material::Galvanised));
    add_walls(frame, kTankMin.GetX(), kTankMax.GetX(), kTankMin.GetZ(), kTankMax.GetZ(),
              kTankMin.GetY() - 0.2F, kTankMax.GetY() + 0.4F, Material::Rust);
    frame.push_back(span({kTankMin.GetX(), kTankMin.GetY() - 0.2F, kTankMin.GetZ()},
                         {kTankMax.GetX(), kTankMin.GetY(), kTankMax.GetZ()}, Material::Rust));
    frame.push_back(span({kTankMin.GetX(), kTankMax.GetY() + 0.2F, kTankMin.GetZ()},
                         {kTankMax.GetX(), kTankMax.GetY() + 0.4F, kTankMax.GetZ()}, Material::Rust));
    for (const float x : {6.0F, 11.0F}) {
        frame.push_back(span({x - 0.06F, kTankMax.GetY() + 0.4F, -132.42F},
                             {x + 0.06F, 263.75F, -132.30F}, Material::Steel));
    }
    // The pipe along the 220 ring and its riser to the tank; its turn south
    // off the ring east of the walkway and west across the walkway's foot,
    // where its gap is, and down into the well west of it.
    frame.push_back(span({6.0F, kPipeY - kPipeHalf, kPipeZ - kPipeHalf},
                         {15.0F + kPipeHalf, kPipeY + kPipeHalf, kPipeZ + kPipeHalf}, Material::Galvanised));
    frame.push_back(span({6.0F - kPipeHalf, kPipeY, kPipeZ - kPipeHalf},
                         {6.0F + kPipeHalf, 228.2F, kPipeZ + kPipeHalf}, Material::Galvanised));
    frame.push_back(span({6.0F - kPipeHalf, 227.8F, kTankMax.GetZ() + kWall},
                         {6.0F + kPipeHalf, 228.2F, kPipeZ}, Material::Galvanised));
    frame.push_back(span({15.0F - kPipeHalf, kPipeY - kPipeHalf, kCrossZ - kPipeHalf},
                         {15.0F + kPipeHalf, kPipeY + kPipeHalf, kPipeZ - kPipeHalf}, Material::Galvanised));
    frame.push_back(span({kGapEast, kPipeY - kPipeHalf, kCrossZ - kPipeHalf},
                         {15.0F - kPipeHalf, kPipeY + kPipeHalf, kCrossZ + kPipeHalf}, Material::Galvanised));
    frame.push_back(span({kDownX - kPipeHalf, kPipeY - kPipeHalf, kCrossZ - kPipeHalf},
                         {kGapWest, kPipeY + kPipeHalf, kCrossZ + kPipeHalf}, Material::Galvanised));
    frame.push_back(span({kDownX - kPipeHalf, 214.0F, kCrossZ - kPipeHalf},
                         {kDownX + kPipeHalf, kPipeY - kPipeHalf, kCrossZ + kPipeHalf}, Material::Galvanised));
    // The couplings' flanges either side of the gap. Nothing lies on the
    // walkway between them: a spool set down flat across the gap, within
    // 0.28 m of its line, is seated, and guides there only caught a flange
    // dropped a hand's width off and tipped it. Open, the gap is the way
    // onto the platform.
    for (const float x : {kGapWest, kGapEast}) {
        frame.push_back(span({x - 0.03F, kDDeckTop, kCrossZ - 0.28F}, {x + 0.03F, kPipeY + 0.3F, kCrossZ + 0.28F},
                             Material::Rust));
    }
    // The drain lever's bracket.
    frame.push_back(span({kDDrainPivot.GetX() - 0.05F, 220.25F, -131.05F},
                         {kDDrainPivot.GetX() + 0.05F, 223.3F, -130.95F}, Material::Steel));
    // The drain's basin under the tube's foot.
    frame.push_back(box(JPH::Vec3(0.5F, 0.2F, 0.5F), JPH::Vec3(kColX, 176.0F, z0 - 0.5F),
                        Material::Concrete));

    // ---- the platform, its mast and float: one body --------------------------
    std::vector<Part> ram = platform_parts(0.0F);
    ram.push_back(box(JPH::Vec3(kMastHalf, 18.4F, kMastHalf), JPH::Vec3(0.0F, -18.6F, 0.0F),
                      Material::Steel));
    ram.push_back(box(JPH::Vec3(kFloatHalf, 0.5F * kFloatHeight, kFloatHalf),
                      JPH::Vec3(0.0F, kFloatBottom + 0.5F * kFloatHeight, 0.0F), Material::Rust));
    wet.d_platform = kit.add_body(Sim::kWetDPlatformEntityId, ram, JPH::RVec3(kColX, kDDeckTop, kDZ),
                                  JPH::Quat::sIdentity(), kDMassKg, 0.8F);
    wet.d_guide = kit.add_guide(wet.d_platform, JPH::Vec3::sAxisY(), 0.0F, kDTravel, 2.5F, 40000.0F,
                                1.0F);

    // ---- water: the tank, the tube, the fill and the drain --------------------
    wet.d_tank = kit.add_pool(kTankMin, kTankMax, kTankWaterKg);
    wet.d_tube = kit.add_pool(JPH::Vec3(x0, kTubeFloor, z0), JPH::Vec3(x1, kTubeOverflow, z1), 0.0F);
    kit.add_float(wet.d_tube, wet.d_platform, kFloatHalf, kFloatHalf, kFloatBottom, kFloatHeight);

    add_retired(kit, Sim::kWetDValveEntityId, kDValvePivot);
    add_retired(kit, Sim::kWetDValveHandleEntityId, kDValveSheave2);
    // The fill has no valve to throw (rule 11): the pipe's two ends at the
    // gap are self-sealing couplings, each shut until a spool's flange seats
    // against it, as a dry-break hose coupling is. The rider carries the
    // spool through the gap onto the platform, turns and sets it down in the
    // gap behind: the tank's water runs, and the float lifts the platform
    // within a second as the tube fills.
    wet.d_fill = kit.add_pipe(wet.d_tank, kTankMin.GetY(), wet.d_tube, kTubeFloor, JPH::RVec3::sZero(),
                              kFillArea, kit::LeverIndex{}, 0.0F, 0.0F);

    // The spool lies on the ring west of the walkway, on its flanges.
    wet.d_spool = kit.add_body(
        Sim::kWetDSpoolEntityId,
        {box(JPH::Vec3(0.6F, kPipeHalf, kPipeHalf), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(0.03F, kSpoolFlange, kSpoolFlange), JPH::Vec3(-0.57F, 0.0F, 0.0F), Material::Rust),
         box(JPH::Vec3(0.03F, kSpoolFlange, kSpoolFlange), JPH::Vec3(0.57F, 0.0F, 0.0F), Material::Rust)},
        JPH::RVec3(9.5, 220.25 + kSpoolFlange, -129.8), JPH::Quat::sIdentity(), 50.0F, 0.7F);
    kit.set_carry(wet.d_spool, kit::CarryKind::Load, JPH::Vec3(0.0F, kPipeHalf, 0.0F));
    kit.set_pipe_spool(wet.d_fill, wet.d_spool, kSpoolSeat, JPH::Vec3::sAxisX(), 0.28F, 0.35F);

    wet.d_drain_body = add_trip_lever(kit, Sim::kWetDDrainEntityId, kDDrainPivot, wet.d_drain);
    wet.d_drain_handle = add_handle(kit, Sim::kWetDDrainHandleEntityId, kDDrainSheave, kHandleDrop);
    (void)kit.add_trip_line(wet.d_drain_body, JPH::Vec3(-kLeverArm, 0.0F, 0.0F), wet.d_drain_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kDDrainSheave, kDDrainSheave);
    wet.d_drain_pipe = kit.add_pipe(wet.d_tube, kTubeFloor, kit::PoolIndex{},
                                    0.0F, JPH::RVec3(kColX, kTubeFloor - 0.3F, z0 - 0.5F),
                                    kDrainArea, wet.d_drain, 0.15F, 0.5F);
}

// E's trip lever: the counterweighted lever's two parts as add_trip_lever
// builds them, each with its own mass: a 151 kg block 0.35 m east of the pin
// and a 4 kg arm west, so the line at the arm's eye, kELeverEye out, is held
// up to about 2 kN before the lever turns.
kit::BodyIndex add_chiller_trip_lever(kit::Kit &kit, const std::uint64_t entity, const JPH::RVec3 pivot,
                                      kit::LeverIndex &lever) {
    Part arm = box(JPH::Vec3(0.5F * kLeverArm, 0.05F, 0.05F), JPH::Vec3(-0.5F * kLeverArm, 0.0F, 0.0F),
                   Material::Hazard);
    arm.mass_kg = kELeverArmKg;
    Part block = box(JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(0.35F, 0.0F, 0.0F), Material::Rust);
    block.mass_kg = kELeverBlockKg;
    const kit::BodyIndex body = kit.add_body(entity, {arm, block}, pivot, JPH::Quat::sIdentity(),
                                             kELeverArmKg + kELeverBlockKg, 0.5F);
    lever = kit.add_lever(body, pivot, JPH::Vec3::sAxisZ(), -JPH::Vec3::sAxisX(), 0.0F, kLeverTravel);
    return body;
}

// `late` takes the frame's parts E gained in its rebuild (the glass, the
// steel over it, the line's post and arm): appended after every other frame
// part, so each older part keeps its place in the frame.
void build_stage_e(kit::Kit &kit, WetIsolation &wet, std::vector<Part> &frame, std::vector<Part> &late) {
    // ---- the cab shaft and its doorway; the duct ----------------------------------
    const float cx0 = kColX - kShaftInnerHalf;
    const float cx1 = kColX + kShaftInnerHalf;
    const float z0 = kEZ - kShaftInnerHalf;
    const float z1 = kEZ + kShaftInnerHalf;
    const float floor = kCabPlenumFloor - 0.15F;
    // South wall whole; the west and east walls steel up to their glass; the
    // north wall round its doorway.
    frame.push_back(span({cx0 - kWall, floor, z0 - kWall}, {cx1 + kWall, kShaftRim, z0}, Material::Steel));
    frame.push_back(span({cx0 - kWall, floor, z0}, {cx0, kGlassFrom, z1}, Material::Steel));
    frame.push_back(span({cx1, floor, z0}, {cx1 + kWall, kGlassFrom, z1}, Material::Steel));
    frame.push_back(span({cx0 - kWall, floor, z1}, {kDoorWest, kShaftRim, z1 + kWall}, Material::Steel));
    frame.push_back(span({kDoorEast, floor, z1}, {cx1 + kWall, kShaftRim, z1 + kWall}, Material::Steel));
    frame.push_back(span({kDoorWest, floor, z1}, {kDoorEast, kDoorBottom, z1 + kWall}, Material::Steel));
    frame.push_back(span({kDoorWest, kDoorTop, z1}, {kDoorEast, kShaftRim, z1 + kWall}, Material::Steel));
    frame.push_back(span({cx0, floor, z0}, {cx1, kCabPlenumFloor, z1}, Material::Steel));
    const float dx0 = kWestX - kShaftInnerHalf;
    const float dx1 = kWestX + kShaftInnerHalf;
    const float duct_floor = kDuctFloor - 0.15F;
    add_walls(frame, dx0, dx1, z0, z1, duct_floor, kDuctTop, Material::Galvanised);
    // The duct's east wall, beside the shaft's west wall: steel up to its glass.
    frame.back() = span({dx1, duct_floor, z0}, {dx1 + kWall, kGlassFrom, z1}, Material::Galvanised);
    frame.push_back(span({dx0, duct_floor, z0}, {dx1, kDuctFloor, z1}, Material::Galvanised));
    // The bucket's basin, and the hoist sheaves' beam over the duct.
    frame.push_back(box(JPH::Vec3(0.9F, 0.15F, 0.9F), JPH::Vec3(kBucketX, kBucketBasinY, kEZ),
                        Material::Concrete));
    frame.push_back(span({kBucketX - 0.2F, 306.5F, kEZ - 0.12F}, {kWestX + 0.2F, 306.74F, kEZ + 0.12F},
                         Material::Steel));
    // Glass from 258 to 296 m in the shaft's west and east walls and the
    // duct's east wall: the chiller seen falling past, the tower outside.
    // Steel over it to the rim (the vent collar's band) and the duct's top.
    late.push_back(span({cx0 - kWall, kGlassFrom, z0}, {cx0, kGlassTo, z1}, Material::Glass));
    late.push_back(span({cx0 - kWall, kGlassTo, z0}, {cx0, kShaftRim, z1}, Material::Steel));
    late.push_back(span({cx1, kGlassFrom, z0}, {cx1 + kWall, kGlassTo, z1}, Material::Glass));
    late.push_back(span({cx1, kGlassTo, z0}, {cx1 + kWall, kShaftRim, z1}, Material::Steel));
    late.push_back(span({dx1, kGlassFrom, z0}, {dx1 + kWall, kGlassTo, z1}, Material::Glass));
    late.push_back(span({dx1, kGlassTo, z0}, {dx1 + kWall, kDuctTop, z1}, Material::Galvanised));
    // The deck's line: up from the deck's corner eye to a sheave on an arm off
    // a post on the shaft's north-east corner, then west to the lever's.
    late.push_back(span({cx1, kShaftRim, z1}, {cx1 + kWall, 305.0F, z1 + kWall}, Material::Steel));
    late.push_back(span({14.05F, 304.68F, -135.60F}, {cx1 + kWall, 304.80F, z1 + kWall}, Material::Steel));

    // ---- the cab and the chiller ------------------------------------------------------
    // A skirted box: its plate, four skirt walls down to the sealing edge, and
    // rails east and west; open north (the doorway) and south (to F).
    std::vector<Part> cab = platform_parts(0.0F);
    cab.front().half = JPH::Vec3(kPlatformHalf, 0.5F * kECabPlate, kPlatformHalf);
    cab.front().offset = JPH::Vec3(0.0F, -0.5F * kECabPlate, 0.0F);
    const float skirt_half_y = 0.5F * (kSkirt - kECabPlate);
    const float skirt_mid = -0.5F * (kSkirt + kECabPlate);
    for (const float side : {-1.0F, 1.0F}) {
        cab.push_back(box(JPH::Vec3(kPlatformHalf, skirt_half_y, 0.5F * kSkirtWall),
                          JPH::Vec3(0.0F, skirt_mid, side * (kPlatformHalf - 0.5F * kSkirtWall)), Material::Steel));
        cab.push_back(box(JPH::Vec3(0.5F * kSkirtWall, skirt_half_y, kPlatformHalf - kSkirtWall),
                          JPH::Vec3(side * (kPlatformHalf - 0.5F * kSkirtWall), skirt_mid, 0.0F), Material::Steel));
    }
    wet.e_cab = kit.add_body(Sim::kWetECabEntityId, cab, JPH::RVec3(kColX, kECabTop, kEZ),
                             JPH::Quat::sIdentity(), kECabMassKg, 0.8F);
    // No governor: the throttle sets its speed, and the seal drags on the walls.
    wet.e_cab_guide = kit.add_guide(wet.e_cab, JPH::Vec3::sAxisY(), 0.0F, kETravel, 0.0F, 0.0F, 0.0F);
    kit.set_guide_friction(wet.e_cab_guide, kSealFrictionN);
    wet.e_chiller = kit.add_body(
        Sim::kWetEChillerEntityId,
        {box(JPH::Vec3(1.3F, kChillerHalfY, 1.3F), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(1.0F, 0.08F, 0.3F), JPH::Vec3(0.0F, kChillerHalfY + 0.08F, 0.0F), Material::Rust)},
        JPH::RVec3(kWestX, kChillerBottom + kChillerHalfY, kEZ), JPH::Quat::sIdentity(),
        kChillerMassKg, 0.5F);
    wet.e_chiller_guide = kit.add_guide(wet.e_chiller, JPH::Vec3::sAxisY(), -kChillerTravel, 0.0F,
                                        0.0F, 0.0F, 0.0F);

    // ---- the air: the duct's cell, the cab's, the throttle, breaker, bleed --------------
    wet.e_duct_cell = kit.add_cell(kPistonArea * (kChillerBottom - kDuctFloor) + kPlenumExtra);
    kit.add_piston(wet.e_duct_cell, wet.e_chiller, kPistonArea);
    wet.e_cab_cell = kit.add_cell(kPistonArea * (kECabTop - kSkirt - kCabPlenumFloor) + kPlenumExtra);
    kit.add_piston(wet.e_cab_cell, wet.e_cab, kPistonArea);
    kit.add_throttle(wet.e_duct_cell, wet.e_cab_cell, kThrottleArea);
    // The breaker is on the cab's side: re-armed, the rising chiller draws
    // the cab's air first, and only a cab on its stop lets outside air in.
    kit.add_breaker(wet.e_cab_cell, kBreakerArea);
    // A bleed round the cab's seal: a lifted cab left alone sinks a few
    // centimeters in a minute and a half.
    kit.add_bleed(wet.e_cab_cell, kBleedArea);

    // ---- the doors: two leaves on hinges leaning top outward ---------------------------
    // Each opens into the shaft and lifts as it opens, so its own weight swings
    // it shut (about 13 N m shut; some 30 N at its free edge opens it); shut,
    // the cell's air presses it onto its frame. A body walking in pushes them
    // open, and they close behind it. Nothing in them is taken in the hands.
    const float leaf_half_x = 0.5F * (0.5F * (kDoorEast - kDoorWest) - kLeafGap - 0.005F);
    const float leaf_half_y = 1.07F;
    const float door_mid_y = 0.5F * (kDoorBottom + kDoorTop);
    const float door_z = z1 + 0.5F * kWall;
    const float lean_c = std::cos(kHingeLean);
    const float lean_s = std::sin(kHingeLean);
    const JPH::RVec3 west_hinge(kDoorWest + kLeafGap, door_mid_y, door_z);
    const JPH::RVec3 east_hinge(kDoorEast - kLeafGap, door_mid_y, door_z);
    wet.e_door = kit.add_body(
        Sim::kWetEDoorEntityId,
        {box(JPH::Vec3(leaf_half_x, leaf_half_y, 0.03F), JPH::Vec3::sZero(), Material::Hazard)},
        west_hinge + JPH::RVec3(leaf_half_x, 0.0, 0.0), JPH::Quat::sIdentity(), kLeafMassKg, 0.6F);
    kit.set_damping(wet.e_door, 0.5F, 1.0F);
    wet.e_door_hinge = kit.add_lever(wet.e_door, west_hinge, JPH::Vec3(0.0F, lean_c, lean_s),
                                     JPH::Vec3::sAxisX(), 0.0F, kLeafOpen);
    wet.e_leaf_east = kit.add_body(
        Sim::kWetEEastLeafEntityId,
        {box(JPH::Vec3(leaf_half_x, leaf_half_y, 0.03F), JPH::Vec3::sZero(), Material::Hazard)},
        east_hinge - JPH::RVec3(leaf_half_x, 0.0, 0.0), JPH::Quat::sIdentity(), kLeafMassKg, 0.6F);
    kit.set_damping(wet.e_leaf_east, 0.5F, 1.0F);
    wet.e_latch = kit.add_lever(wet.e_leaf_east, east_hinge, JPH::Vec3(0.0F, -lean_c, -lean_s),
                                -JPH::Vec3::sAxisX(), 0.0F, kLeafOpen);
    // The old door's spring catch keeps its slot, open and never relatching:
    // with nothing to hold, the leaves close on their own weight.
    wet.e_door_catch = kit.add_catch(wet.e_door, wet.e_latch, 0.4F, 0.08F, false);
    const float leaf_area = 0.5F * (kDoorEast - kDoorWest) * (kDoorTop - kDoorBottom);
    kit.add_door(wet.e_cab_cell, wet.e_door_hinge, leaf_area, 0.3F, JPH::Vec3::sAxisZ(), wet.e_cab, -kSkirt,
                 kDoorBottom, kDoorTop);
    kit.add_door(wet.e_cab_cell, wet.e_latch, leaf_area, 0.3F, JPH::Vec3::sAxisZ(), wet.e_cab, -kSkirt,
                 kDoorBottom, kDoorTop);
    kit.open_catch(wet.e_door_catch);
    // The door interlock (declared): with a leaf standing open, the cab stops
    // with its deck 0.1 m under the sill and stays there until it is back on
    // its bottom stop, so the ride never meets an open leaf.
    kit.set_interlock(wet.e_cab_guide, wet.e_door_hinge, wet.e_latch, kLeafShutRad,
                      kDoorBottom - 0.1F - (kECabTop + kDeckThick));

    // ---- the chiller's catch, its lever, and the deck on its trip line -----------------
    wet.e_lever_body = add_chiller_trip_lever(kit, Sim::kWetELeverEntityId, kELeverPivot, wet.e_lever);
    wet.e_catch = kit.add_catch(wet.e_chiller, wet.e_lever, kLeverRelease, 0.05F, true);
    // The deck: grating with hazard edges, its line's eye on its north-east
    // corner, hung kDeckProud over the cab's plate, on its own guide. The
    // guide is built here, in E's own place in the build, not after every
    // other band: the solver orders an island's constraints by their place in
    // its list, and one built in the middle moves every later constraint along
    // by one, keeping their order, while one built last would be the one
    // swapped into the first hole any constraint taken away leaves.
    const float strip = 0.12F;
    std::vector<Part> deck{
        box(JPH::Vec3(kDeckHalf - strip, 0.5F * kDeckThick, kDeckHalf - strip),
            JPH::Vec3(0.0F, -0.5F * kDeckThick, 0.0F), Material::Galvanised)};
    for (const float side : {-1.0F, 1.0F}) {
        deck.push_back(box(JPH::Vec3(kDeckHalf, 0.5F * kDeckThick, 0.5F * strip),
                           JPH::Vec3(0.0F, -0.5F * kDeckThick, side * (kDeckHalf - 0.5F * strip)), Material::Hazard));
        deck.push_back(box(JPH::Vec3(0.5F * strip, 0.5F * kDeckThick, kDeckHalf - strip),
                           JPH::Vec3(side * (kDeckHalf - 0.5F * strip), -0.5F * kDeckThick, 0.0F), Material::Hazard));
    }
    deck.push_back(box(JPH::Vec3(0.04F, 0.03F, 0.04F), kDeckEye, Material::Yellow));
    wet.e_deck = kit.add_body(Sim::kWetEDeckEntityId, deck,
                              JPH::RVec3(kColX, kECabTop + kDeckProud + kDeckThick, kEZ), JPH::Quat::sIdentity(),
                              kDeckMassKg, 0.8F);
    // It rides on the cab's plate, from kDeckProud below where it hangs to the
    // cab's hard top. Its travel lies inside the cab's.
    wet.e_deck_guide = kit.add_guide(wet.e_deck, JPH::Vec3::sAxisY(), -kDeckProud, kETravel + kDeckProud, 0.0F,
                                     0.0F, 0.0F);
    (void)kit.add_trip_line(wet.e_lever_body, JPH::Vec3(-kELeverEye, 0.0F, 0.0F), wet.e_deck, kDeckEye,
                            kESheave1, kESheave2);
    // The vent collar at the shaft's mouth.
    wet.e_vent = kit.add_vent(wet.e_cab_cell, wet.e_cab, -kSkirt, JPH::Vec3(cx0, kVentFrom, z0),
                              JPH::Vec3(cx1, kShaftRim, z1), kVentPerM);

    // ---- the hoist bucket, its rope, its striker ---------------------------------------
    std::vector<Part> bucket{
        box(JPH::Vec3(0.9F, 0.06F, 0.9F), JPH::Vec3::sZero(), Material::Rust)};
    for (const float side : {-1.0F, 1.0F}) {
        bucket.push_back(box(JPH::Vec3(0.9F, 1.2F, 0.04F), JPH::Vec3(0.0F, 1.26F, side * 0.86F), Material::Rust));
        bucket.push_back(box(JPH::Vec3(0.04F, 1.2F, 0.82F), JPH::Vec3(side * 0.86F, 1.26F, 0.0F), Material::Rust));
    }
    bucket.push_back(box(JPH::Vec3(0.9F, 0.04F, 0.04F), JPH::Vec3(0.0F, 2.5F, 0.0F), Material::Steel));
    wet.e_bucket = kit.add_body(Sim::kWetEBucketEntityId, bucket, JPH::RVec3(kBucketX, kBucketFloorY, kEZ),
                                JPH::Quat::sIdentity(), kBucketMassKg, 0.6F);
    // Its governor eases the falling chiller onto its stop.
    wet.e_bucket_guide = kit.add_guide(wet.e_bucket, JPH::Vec3::sAxisY(), 0.0F, kChillerTravel, 4.0F,
                                       50000.0F, 3.0F);
    const JPH::Vec3 bail(0.0F, 2.5F, 0.0F);
    const JPH::Vec3 lug(0.0F, kChillerHalfY + 0.16F, 0.0F);
    const float rope_length =
        static_cast<float>(kESheaveDuct.GetY() - (kChillerBottom + 2.0F * kChillerHalfY + 0.16F)) +
        static_cast<float>(kESheaveBucket.GetY() - (kBucketFloorY + 2.5F));
    wet.e_rope = kit.add_rope(wet.e_chiller, lug, kESheaveDuct, wet.e_bucket, bail, kESheaveBucket, 1.0F,
                              rope_length, 0.0F);
    wet.e_striker_body = kit.add_body(
        Sim::kWetEStrikerEntityId,
        {box(JPH::Vec3(0.05F, 0.03F, 0.5F), JPH::Vec3(0.0F, 0.0F, -0.5F), Material::Hazard),
         box(JPH::Vec3(0.12F, 0.12F, 0.12F), JPH::Vec3(0.0F, 0.0F, 0.25F), Material::Rust)},
        kEStrikerPivot, JPH::Quat::sIdentity(), 12.0F, 0.5F);
    wet.e_striker = kit.add_lever(wet.e_striker_body, kEStrikerPivot, -JPH::Vec3::sAxisX(),
                                  -JPH::Vec3::sAxisZ(), 0.0F, 0.6F);
    wet.e_bucket_bin = kit.add_bin(wet.e_bucket, 0.0F, kBucketCapacityKg, JPH::Vec3(0.0F, -0.08F, 0.0F),
                                   wet.e_striker, 0.12F, 1.5F, 2500.0F);
    kit.set_bin_water(wet.e_bucket_bin);
}

void build_stage_f(kit::Kit &kit, WetIsolation &wet, std::vector<Part> &frame) {
    // ---- the platform and its ram --------------------------------------------------
    std::vector<Part> platform = platform_parts(1.0F);
    platform.push_back(box(JPH::Vec3(0.2F, 0.5F, 0.2F), JPH::Vec3(0.0F, -0.7F, 0.0F), Material::Galvanised));
    wet.f_platform = kit.add_body(Sim::kWetFPlatformEntityId, platform, JPH::RVec3(kColX, kFDeckTop, kFZ),
                                  JPH::Quat::sIdentity(), kFMassKg, 0.8F);
    wet.f_platform_guide = kit.add_guide(wet.f_platform, JPH::Vec3::sAxisY(), 0.0F, kFTravel, 2.0F,
                                         20000.0F, 1.0F);
    wet.f_inlet = kit.add_anchor(wet.f_platform, kFInletLocal, 1.2F);
    frame.push_back(span({kColX - 0.3F, kRamBase - 1.5F, kFZ - 0.3F}, {kColX + 0.3F, kRamBase, kFZ + 0.3F},
                         Material::Steel));

    // ---- the accumulator on its cylinder --------------------------------------------
    wet.f_accumulator = kit.add_body(
        Sim::kWetFAccumulatorEntityId,
        {box(JPH::Vec3(1.3F, kAccumulatorHalfY, 1.3F), JPH::Vec3::sZero(), Material::Rust),
         box(JPH::Vec3(1.35F, 0.08F, 1.35F), JPH::Vec3(0.0F, kAccumulatorHalfY - 0.3F, 0.0F), Material::Steel)},
        JPH::RVec3(kWestX, kAccumulatorBottom + kAccumulatorHalfY, kFZ), JPH::Quat::sIdentity(),
        kAccumulatorMassKg, 0.5F);
    wet.f_accumulator_guide = kit.add_guide(wet.f_accumulator, JPH::Vec3::sAxisY(), -kAccumulatorStroke,
                                            0.0F, 0.12F, 150000.0F, 0.05F);
    frame.push_back(span({kWestX - 0.4F, kRamBase, kFZ - 0.4F},
                         {kWestX + 0.4F, kAccumulatorBottom - kAccumulatorStroke, kFZ + 0.4F},
                         Material::Steel));

    // ---- the stop valve: a catch on the accumulator, its line to the platform --------
    wet.f_lever_body = add_trip_lever(kit, Sim::kWetFLeverEntityId, kFLeverPivot, wet.f_lever);
    wet.f_catch = kit.add_catch(wet.f_accumulator, wet.f_lever, kLeverRelease, 0.05F, true);
    wet.f_handle = add_handle(kit, Sim::kWetFHandleEntityId, kFSheave2, kHandleDrop);
    (void)kit.add_trip_line(wet.f_lever_body, JPH::Vec3(-kLeverArm, 0.0F, 0.0F), wet.f_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kFSheave1, kFSheave2);

    // ---- the hose: a push-only line from under the accumulator to the inlet ----------
    wet.f_hose = kit.add_body(
        Sim::kWetFHoseEntityId,
        {box(JPH::Vec3(0.12F, 0.06F, 0.08F), JPH::Vec3::sZero(), Material::Yellow)},
        JPH::RVec3(12.4, kFDeckTop + 0.06, -140.6), JPH::Quat::sIdentity(), 8.0F, 0.8F);
    kit.set_carry(wet.f_hose, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.06F, 0.0F));
    const JPH::RVec3 inlet_foot(kColX + kFInletLocal.GetX(), kRamBase, kFZ + kFInletLocal.GetZ());
    wet.f_line = kit.add_rope(wet.f_accumulator, JPH::Vec3(0.0F, -kAccumulatorHalfY, 0.0F),
                              JPH::RVec3(kWestX, kRamBase, kFZ), wet.f_hose, JPH::Vec3::sZero(), inlet_foot,
                              kAccumulatorStroke / kFTravel, 200.0F, 0.0F);
    kit.set_strut(wet.f_line);
}

void build_plate_and_header(kit::Kit &kit, WetIsolation &wet, std::vector<Part> &frame) {
    // ---- TP-340, with a hole over F -----------------------------------------------------
    const float hx0 = kColX - kPlatformHalf - 0.1F;
    const float hx1 = kColX + kPlatformHalf + 0.1F;
    const float hz0 = kFZ - kPlatformHalf - 0.2F;
    const float hz1 = kFZ + kPlatformHalf + 0.1F;
    frame.push_back(span({-14.6F, kPlateBottom, -164.5F}, {hx0, kPlateTop, -135.6F}, Material::Concrete));
    frame.push_back(span({hx0, kPlateBottom, -164.5F}, {14.6F, kPlateTop, hz0}, Material::Concrete));
    frame.push_back(span({hx0, kPlateBottom, hz1}, {14.6F, kPlateTop, -135.6F}, Material::Concrete));
    frame.push_back(span({hx1, kPlateBottom, hz0}, {14.6F, kPlateTop, hz1}, Material::Concrete));

    // ---- the header tank on the plate -------------------------------------------------
    add_walls(frame, kHeaderMin.GetX(), kHeaderMax.GetX(), kHeaderMin.GetZ(), kHeaderMax.GetZ(), kPlateTop,
              kHeaderMax.GetY() + 0.3F, Material::Rust);
    frame.push_back(span({kHeaderMin.GetX(), kPlateTop, kHeaderMin.GetZ()},
                         {kHeaderMax.GetX(), kHeaderMin.GetY(), kHeaderMax.GetZ()}, Material::Rust));
    wet.header = kit.add_pool(kHeaderMin, kHeaderMax, kHeaderWaterKg);

    // ---- the dump: a bar thrown over by its handle beside F's hole --------------------
    frame.push_back(span({kDumpPivot.GetX() - 0.08F, kPlateTop, kDumpPivot.GetZ() - 0.08F},
                         {kDumpPivot.GetX() + 0.08F, static_cast<float>(kDumpPivot.GetY()) - 0.04F,
                          kDumpPivot.GetZ() + 0.08F},
                         Material::Steel));
    wet.header_dump_body = add_throw_bar(kit, Sim::kWetHeaderDumpEntityId, kDumpPivot, wet.header_dump);
    wet.header_dump_handle = add_handle(kit, Sim::kWetHeaderHandleEntityId, kDumpSheave2, kHandleDrop);
    (void)kit.add_trip_line(wet.header_dump_body, JPH::Vec3(kBarLug, 0.0F, 0.0F), wet.header_dump_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kDumpSheave1, kDumpSheave2);

    // ---- the riser's three branches: D's tank, E's bucket, F's charge -----------------
    wet.header_to_tank = kit.add_pipe(wet.header, kHeaderMin.GetY(), wet.d_tank, kTankMax.GetY(),
                                      JPH::RVec3::sZero(), 0.3F, wet.header_dump, 0.4F, 1.8F);
    wet.header_to_bucket = kit.add_pipe(wet.header, kHeaderMin.GetY(), kit::PoolIndex{}, 0.0F, kBucketSpout,
                                        0.08F, wet.header_dump, 0.4F, 1.8F);
    kit.add_charge(wet.header, kHeaderMin.GetY(), wet.f_accumulator, 0.6F, kRamBase, wet.header_dump, 1.0F);
    frame.push_back(span({kBucketX - 0.12F, static_cast<float>(kBucketSpout.GetY()) + 0.1F, kEZ - 0.12F},
                         {kBucketX + 0.12F, kPlateBottom, kEZ + 0.12F}, Material::Galvanised));
    frame.push_back(span({5.68F, kTankMax.GetY() + 0.4F, -135.72F}, {5.92F, kPlateBottom, -135.48F},
                         Material::Galvanised));
    frame.push_back(span({5.68F, kTankMax.GetY() + 0.4F, -135.72F}, {5.92F, kTankMax.GetY() + 0.64F, -134.0F},
                         Material::Galvanised));
    frame.push_back(span({8.3F, kRamBase, -141.7F}, {8.5F, kPlateBottom, -141.5F}, Material::Galvanised));
}

// ---- The climbing route: 220 -> 340 with no lift ----------------------------
//
// West of the machines, on the well side of the rings, each climb facing the
// ring it tops out onto (AS-006's rule):
//
//  220 -> 242  an L of boards out from the 220 ring to a ladder on the 242
//              ring's face (balance, climb, mantle);
//  242 -> 264  a beam out from the 242 ring under a scaffold panel on the
//              264 ring's face, caught with a jump;
//  264 -> 286  a bulkhead across the 264 ring leaves only its lip: drop over
//              the edge, shimmy under it, climb back up; then an L of boards
//              to a standpipe to the 286 ring;
//  286 -> 308  an L of boards to a ladder on the 308 ring's face;
//  308 -> 330  a catwalk under a panel on the 330 ring's face, jumped for;
//  330 -> 340  a ladder on TP-340's north face, climbed facing south.
constexpr float kMember = 0.03F;
constexpr float kRung = 0.30F;

void route_ladder(std::vector<Part> &route, const float x, const float z, const float bottom,
                  const float top) {
    for (const float side : {-1.0F, 1.0F}) {
        route.push_back(span({x + side * 0.28F - kMember, bottom, z - kMember},
                             {x + side * 0.28F + kMember, top, z + kMember}, Material::Yellow));
    }
    for (float y = bottom + kRung; y <= top - 0.05F; y += kRung) {
        route.push_back(box(JPH::Vec3(0.28F, 0.02F, 0.02F), JPH::Vec3(x, y, z), Material::Steel));
    }
}

void route_panel(std::vector<Part> &route, const float x0, const float x1, const float z,
                 const float bottom, const float top) {
    for (float x = x0; x <= x1 + 0.01F; x += 0.5F) {
        route.push_back(span({x - kMember, bottom, z - kMember}, {x + kMember, top, z + kMember},
                             Material::Yellow));
    }
    for (float y = bottom; y <= top + 0.01F; y += 0.4F) {
        route.push_back(span({x0 - kMember, y - kMember, z - kMember}, {x1 + kMember, y + kMember, z + kMember},
                             Material::Steel));
    }
}

void route_board(std::vector<Part> &route, const JPH::Vec3 low, const JPH::Vec3 high) {
    route.push_back(span(low, high, Material::Timber));
}

void build_climbing_route(kit::Kit &kit) {
    std::vector<Part> route;
    // 220 -> 242.
    route_board(route, {3.35F, 220.05F, -132.55F}, {3.65F, 220.25F, -130.73F});
    route_board(route, {2.10F, 220.05F, -132.55F}, {3.35F, 220.25F, -132.25F});
    route_ladder(route, 2.5F, -131.78F, 220.25F, 242.25F);
    // 242 -> 264.
    route_board(route, {-1.15F, 242.05F, -133.60F}, {-0.85F, 242.25F, -131.64F});
    route_panel(route, -2.0F, 0.0F, -132.68F, 244.5F, 264.2F);
    // 264 -> 286: the bulkhead, clear of the lip by 0.15 m, to the 286 ring.
    route.push_back(span({2.4F, 264.25F, -132.40F}, {2.8F, 285.75F, -128.55F}, Material::Concrete));
    route_board(route, {5.05F, 264.05F, -134.75F}, {5.65F, 264.25F, -132.55F});
    route_board(route, {4.40F, 264.05F, -134.75F}, {5.05F, 264.25F, -134.15F});
    route.push_back(span({5.0F - 0.05F, 264.25F, -133.60F - 0.05F}, {5.0F + 0.05F, 286.2F, -133.60F + 0.05F},
                         Material::Galvanised));
    // 286 -> 308.
    route_board(route, {3.70F, 286.05F, -135.40F}, {4.30F, 286.25F, -133.46F});
    route_board(route, {2.40F, 286.05F, -135.40F}, {3.70F, 286.25F, -134.80F});
    route_ladder(route, 3.0F, -134.51F, 286.25F, 308.25F);
    // 308 -> 330.
    route.push_back(span({1.0F, 308.15F, -136.30F}, {2.0F, 308.25F, -134.37F}, Material::Galvanised));
    route_panel(route, 0.5F, 2.5F, -135.41F, 310.5F, 330.2F);
    // 330 -> TP-340, on the plate's north face.
    route_ladder(route, 3.0F, -135.46F, 330.25F, kPlateTop);
    (void)kit.add_body(Sim::kWetRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F,
                       0.8F);
}

} // namespace

void build_wet_isolation(kit::Kit &kit, WetIsolation &wet) {
    std::vector<Part> frame;
    std::vector<Part> late;
    build_stage_d(kit, wet, frame);
    build_stage_e(kit, wet, frame, late);
    build_stage_f(kit, wet, frame);
    build_plate_and_header(kit, wet, frame);
    build_climbing_route(kit);
    frame.insert(frame.end(), late.begin(), late.end());
    const kit::BodyIndex frame_body =
        kit.add_body(Sim::kWetFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
    // E's leaves swing on hinges leaning 8 degrees, so the corners at their
    // hinged edges sweep a few centimeters into the jambs as they open, and
    // their lower free corners a centimeter or two past each other as they
    // close: the hinges hold them, and the frame and each other let them by.
    kit.disable_collision(wet.e_door, frame_body);
    kit.disable_collision(wet.e_leaf_east, frame_body);
    kit.disable_collision(wet.e_door, wet.e_leaf_east);
}

} // namespace scraperx::sim::bands
