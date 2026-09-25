#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <vector>

// AS-007, Wet Isolation (Atlas band B03, 220 -> 340 m). Contract:
// 03_EXECUTION/ASCENT/AS-007_WET_ISOLATION.md. Three stages in the well's
// north-east quarter, each on water or air: D, a float ram fed from a head
// tank; E, a cab lifted on the air a falling chiller pushes; F, a
// weight-loaded hydraulic accumulator. A header on TP-340 re-arms them.
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
constexpr float kGapWest = 8.7F;
constexpr float kGapEast = 10.3F;
const JPH::RVec3 kSpoolSeat(9.5, 220.47, -127.5);
// The fill valve: a bar on a pin, thrown over by a line from its lug to a
// handle by the platform's north-west corner. It stays where it falls.
const JPH::RVec3 kDValvePivot(10.2, 221.2, -131.4);
constexpr float kBarLength = 1.4F;
constexpr float kBarLug = 0.5F;
constexpr float kBarFarStop = 2.2F;
const JPH::RVec3 kDValveSheave1(10.7, 223.2, -131.4);
const JPH::RVec3 kDValveSheave2(12.3, 222.2, -131.9);
// The drain: a counterweighted lever held open by its handle.
const JPH::RVec3 kDDrainPivot(15.3, 223.5, -131.0);
const JPH::RVec3 kDDrainSheave(14.3, 222.2, -131.0);
constexpr float kDrainArea = 0.3F;

// ---- Stage E ----------------------------------------------------------------
constexpr float kShaftInnerHalf = 1.35F;
constexpr float kPlenumFloor = 254.0F;
constexpr float kECabTop = 256.25F;
constexpr float kETravel = 42.0F;          // 256.25 -> 298.25
constexpr float kECabMassKg = 900.0F;
constexpr float kShaftRim = 298.2F;
constexpr float kDuctTop = 304.2F;
constexpr float kChillerHalfY = 1.25F;
constexpr float kChillerBottom = 301.5F;
constexpr float kChillerTravel = 45.0F;
constexpr float kChillerMassKg = 5000.0F;
constexpr float kPistonArea = 4.0F * kShaftInnerHalf * kShaftInnerHalf;   // 7.29 m^2
constexpr float kPlenumExtra = 0.5F;       // the throttle's duct, m^3
constexpr float kThrottleArea = 0.2F;
constexpr float kBreakerArea = 0.3F;
constexpr float kDoorWest = 12.4F;
constexpr float kDoorEast = 13.6F;
constexpr float kDoorBottom = 256.3F;
constexpr float kDoorTop = 258.5F;
constexpr float kDoorOpen = 0.9F;          // rad, standing ajar as found
const JPH::RVec3 kELatchPivot(13.8, 257.6, -135.45);   // inside, by the east jamb
const JPH::RVec3 kELeverPivot(10.0, 305.3, -134.8);
const JPH::RVec3 kESheave1(9.0, 304.6, -134.8);
const JPH::RVec3 kESheave2(14.1, 258.2, kEZ);     // inside the cab's east wall
// The bucket, hung from the chiller over two sheaves above the duct.
constexpr float kBucketFloorY = 257.76F;   // its floor's centre, as found
constexpr float kBucketMassKg = 300.0F;
constexpr float kBucketCapacityKg = 6000.0F;
const JPH::RVec3 kESheaveDuct(kWestX, 306.2, kEZ);
const JPH::RVec3 kESheaveBucket(kBucketX, 306.2, kEZ);
const JPH::RVec3 kEStrikerPivot(kBucketX, 257.76, -135.45);

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
    // The pipe along the 220 ring, its gap and cradle, its riser to the
    // tank, and its turn down into the well east of the walkway.
    frame.push_back(span({6.0F, kPipeY - kPipeHalf, kPipeZ - kPipeHalf},
                         {kGapWest, kPipeY + kPipeHalf, kPipeZ + kPipeHalf}, Material::Galvanised));
    frame.push_back(span({kGapEast, kPipeY - kPipeHalf, kPipeZ - kPipeHalf},
                         {15.0F, kPipeY + kPipeHalf, kPipeZ + kPipeHalf}, Material::Galvanised));
    for (const float x : {kGapWest, kGapEast}) {
        frame.push_back(span({x - 0.03F, kDDeckTop, kPipeZ - 0.28F}, {x + 0.03F, kPipeY + 0.3F, kPipeZ + 0.28F},
                             Material::Rust));
    }
    // Two low guides on the deck either side of the gap: a spool set down
    // between them lies across it.
    for (const float side : {-1.0F, 1.0F}) {
        frame.push_back(span({kGapWest + 0.05F, kDDeckTop, kPipeZ + side * 0.5F - 0.03F},
                             {kGapEast - 0.05F, kDDeckTop + 0.06F, kPipeZ + side * 0.5F + 0.03F},
                             Material::Steel));
    }
    frame.push_back(span({6.0F - kPipeHalf, kPipeY, kPipeZ - kPipeHalf},
                         {6.0F + kPipeHalf, 228.2F, kPipeZ + kPipeHalf}, Material::Galvanised));
    frame.push_back(span({6.0F - kPipeHalf, 227.8F, kTankMax.GetZ() + kWall},
                         {6.0F + kPipeHalf, 228.2F, kPipeZ}, Material::Galvanised));
    frame.push_back(span({15.0F - kPipeHalf, kPipeY - kPipeHalf, -131.2F},
                         {15.0F + kPipeHalf, kPipeY + kPipeHalf, kPipeZ}, Material::Galvanised));
    frame.push_back(span({15.0F - kPipeHalf, 214.0F, -131.2F - 2.0F * kPipeHalf},
                         {15.0F + kPipeHalf, kPipeY + kPipeHalf, -131.2F}, Material::Galvanised));
    // Brackets: the fill bar's pin off the ring's edge, the drain lever's.
    frame.push_back(span({kDValvePivot.GetX() - 0.05F, 221.0F, -131.45F},
                         {kDValvePivot.GetX() + 0.05F, 221.1F, -130.73F}, Material::Steel));
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

    wet.d_valve_body = add_throw_bar(kit, Sim::kWetDValveEntityId, kDValvePivot, wet.d_valve);
    wet.d_valve_handle = add_handle(kit, Sim::kWetDValveHandleEntityId, kDValveSheave2, kHandleDrop);
    (void)kit.add_trip_line(wet.d_valve_body, JPH::Vec3(kBarLug, 0.0F, 0.0F), wet.d_valve_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kDValveSheave1, kDValveSheave2);
    wet.d_fill = kit.add_pipe(wet.d_tank, kTankMin.GetY(), wet.d_tube, kTubeFloor, JPH::RVec3::sZero(),
                              kFillArea, wet.d_valve, 0.4F, 1.8F);

    // The spool lies on the ring south of its gap.
    wet.d_spool = kit.add_body(
        Sim::kWetDSpoolEntityId,
        {box(JPH::Vec3(0.6F, kPipeHalf, kPipeHalf), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(0.03F, 0.3F, 0.3F), JPH::Vec3(-0.57F, 0.0F, 0.0F), Material::Rust),
         box(JPH::Vec3(0.03F, 0.3F, 0.3F), JPH::Vec3(0.57F, 0.0F, 0.0F), Material::Rust)},
        JPH::RVec3(9.5, 220.25 + 0.3, -129.8), JPH::Quat::sIdentity(), 50.0F, 0.7F);
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

void build_stage_e(kit::Kit &kit, WetIsolation &wet, std::vector<Part> &frame) {
    // ---- the cab shaft and its door; the duct ------------------------------------
    const float cx0 = kColX - kShaftInnerHalf;
    const float cx1 = kColX + kShaftInnerHalf;
    const float z0 = kEZ - kShaftInnerHalf;
    const float z1 = kEZ + kShaftInnerHalf;
    const float floor = kPlenumFloor - 0.15F;
    // South, west and east walls whole; the north wall round its door.
    frame.push_back(span({cx0 - kWall, floor, z0 - kWall}, {cx1 + kWall, kShaftRim, z0}, Material::Steel));
    frame.push_back(span({cx0 - kWall, floor, z0}, {cx0, kShaftRim, z1}, Material::Steel));
    frame.push_back(span({cx1, floor, z0}, {cx1 + kWall, kShaftRim, z1}, Material::Steel));
    frame.push_back(span({cx0 - kWall, floor, z1}, {kDoorWest, kShaftRim, z1 + kWall}, Material::Steel));
    frame.push_back(span({kDoorEast, floor, z1}, {cx1 + kWall, kShaftRim, z1 + kWall}, Material::Steel));
    frame.push_back(span({kDoorWest, floor, z1}, {kDoorEast, kDoorBottom, z1 + kWall}, Material::Steel));
    frame.push_back(span({kDoorWest, kDoorTop, z1}, {kDoorEast, kShaftRim, z1 + kWall}, Material::Steel));
    frame.push_back(span({cx0, floor, z0}, {cx1, kPlenumFloor, z1}, Material::Steel));
    const float dx0 = kWestX - kShaftInnerHalf;
    const float dx1 = kWestX + kShaftInnerHalf;
    add_walls(frame, dx0, dx1, z0, z1, floor, kDuctTop, Material::Galvanised);
    frame.push_back(span({dx0, floor, z0}, {dx1, kPlenumFloor, z1}, Material::Galvanised));
    // The bucket's basin, and the hoist sheaves' beam over the duct.
    frame.push_back(box(JPH::Vec3(0.9F, 0.15F, 0.9F), JPH::Vec3(kBucketX, 254.5F, kEZ),
                        Material::Concrete));
    frame.push_back(span({kBucketX - 0.2F, 306.5F, kEZ - 0.12F}, {kWestX + 0.2F, 306.74F, kEZ + 0.12F},
                         Material::Steel));

    // ---- the cab and the chiller ------------------------------------------------------
    // Open at both ends: in by the door from D, out over the rim to F.
    std::vector<Part> cab = platform_parts(0.0F);
    cab.front().half = JPH::Vec3(kPlatformHalf, 0.125F, kPlatformHalf);
    cab.front().offset = JPH::Vec3(0.0F, -0.125F, 0.0F);
    wet.e_cab = kit.add_body(Sim::kWetECabEntityId, cab, JPH::RVec3(kColX, kECabTop, kEZ),
                             JPH::Quat::sIdentity(), kECabMassKg, 0.8F);
    wet.e_cab_guide = kit.add_guide(wet.e_cab, JPH::Vec3::sAxisY(), 0.0F, kETravel, 2.0F, 25000.0F,
                                    1.0F);
    wet.e_chiller = kit.add_body(
        Sim::kWetEChillerEntityId,
        {box(JPH::Vec3(1.3F, kChillerHalfY, 1.3F), JPH::Vec3::sZero(), Material::Galvanised),
         box(JPH::Vec3(1.0F, 0.08F, 0.3F), JPH::Vec3(0.0F, kChillerHalfY + 0.08F, 0.0F), Material::Rust)},
        JPH::RVec3(kWestX, kChillerBottom + kChillerHalfY, kEZ), JPH::Quat::sIdentity(),
        kChillerMassKg, 0.5F);
    wet.e_chiller_guide = kit.add_guide(wet.e_chiller, JPH::Vec3::sAxisY(), -kChillerTravel, 0.0F,
                                        0.0F, 0.0F, 0.0F);

    // ---- the air: the duct's cell, the cab's, the throttle, door, breaker ------------
    wet.e_duct_cell = kit.add_cell(kPistonArea * (kChillerBottom - kPlenumFloor) + kPlenumExtra);
    kit.add_piston(wet.e_duct_cell, wet.e_chiller, kPistonArea);
    wet.e_cab_cell = kit.add_cell(kPistonArea * (kECabTop - 0.25F - kPlenumFloor) + kPlenumExtra);
    kit.add_piston(wet.e_cab_cell, wet.e_cab, kPistonArea);
    kit.add_throttle(wet.e_duct_cell, wet.e_cab_cell, kThrottleArea);
    // The breaker is on the cab's side: re-armed, the rising chiller draws
    // the cab's air first, and only a cab on its stop lets outside air in.
    kit.add_breaker(wet.e_cab_cell, kBreakerArea);
    // A bleed round the cab's seal: a lifted cab left alone settles back on
    // its stop in a minute or two.
    kit.add_bleed(wet.e_cab_cell, 0.004F);

    // ---- the door: hinged on its west edge, opening into the cab, so the
    // cab's air presses it onto its frame; a spring latch ------------------
    // The leaf stands 5 cm clear of each jamb, so it swings without cutting
    // into the wall; the gasket closes the gap when it is shut.
    const float door_half_x = 0.5F * (kDoorEast - kDoorWest) - 0.05F;
    const float hinge_x = kDoorWest + 0.05F;
    const float door_mid_y = 0.5F * (kDoorBottom + kDoorTop);
    const float door_z = z1 + 0.5F * kWall;
    wet.e_door = kit.add_body(
        Sim::kWetEDoorEntityId,
        {box(JPH::Vec3(door_half_x, 0.5F * (kDoorTop - kDoorBottom) - 0.01F, 0.03F), JPH::Vec3::sZero(),
             Material::Hazard),
         box(JPH::Vec3(0.03F, 0.08F, 0.04F), JPH::Vec3(door_half_x - 0.15F, 0.0F, -0.07F), Material::Steel)},
        JPH::RVec3(hinge_x + door_half_x, door_mid_y, door_z), JPH::Quat::sIdentity(), 60.0F, 0.6F);
    kit.set_carry(wet.e_door, kit::CarryKind::Handle, JPH::Vec3(door_half_x - 0.15F, 0.0F, -0.07F));
    kit.set_damping(wet.e_door, 0.5F, 1.5F);
    const JPH::RVec3 hinge(hinge_x, door_mid_y, door_z);
    wet.e_door_hinge = kit.add_lever(wet.e_door, hinge, JPH::Vec3::sAxisY(), JPH::Vec3::sAxisX(), 0.0F,
                                     1.6F);
    wet.e_latch_body = kit.add_body(
        Sim::kWetELatchEntityId,
        {box(JPH::Vec3(0.02F, 0.15F, 0.02F), JPH::Vec3(0.0F, -0.15F, 0.0F), Material::Steel),
         box(JPH::Vec3(0.04F, 0.04F, 0.04F), JPH::Vec3(0.0F, -0.3F, 0.0F), Material::Yellow)},
        kELatchPivot, JPH::Quat::sIdentity(), 2.0F, 0.6F);
    kit.set_carry(wet.e_latch_body, kit::CarryKind::Handle, JPH::Vec3(0.0F, -0.3F, 0.0F));
    wet.e_latch = kit.add_lever(wet.e_latch_body, kELatchPivot, JPH::Vec3::sAxisZ(), -JPH::Vec3::sAxisY(),
                                0.0F, 1.2F);
    wet.e_door_catch = kit.add_catch(wet.e_door, wet.e_latch, 0.4F, 0.08F, true);
    kit.add_door(wet.e_cab_cell, wet.e_door_hinge, (kDoorEast - kDoorWest) * (kDoorTop - kDoorBottom),
                 0.3F, JPH::Vec3::sAxisZ());
    // As found: unlatched and standing ajar.
    kit.open_catch(wet.e_door_catch);
    const JPH::Quat open = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), kDoorOpen);
    kit.place_body(wet.e_door, hinge + JPH::RVec3(open * JPH::Vec3(door_half_x, 0.0F, 0.0F)), open);

    // ---- the chiller's catch and the trip line into the cab ---------------------------
    wet.e_lever_body = add_trip_lever(kit, Sim::kWetELeverEntityId, kELeverPivot, wet.e_lever);
    wet.e_catch = kit.add_catch(wet.e_chiller, wet.e_lever, kLeverRelease, 0.05F, true);
    wet.e_handle = add_handle(kit, Sim::kWetEHandleEntityId, kESheave2, kHandleDrop);
    (void)kit.add_trip_line(wet.e_lever_body, JPH::Vec3(-kLeverArm, 0.0F, 0.0F), wet.e_handle,
                            JPH::Vec3(0.0F, kHandleHalfY, 0.0F), kESheave1, kESheave2);

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
    wet.e_bucket_guide = kit.add_guide(wet.e_bucket, JPH::Vec3::sAxisY(), 0.0F, kChillerTravel, 2.0F,
                                       25000.0F, 1.0F);
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
    build_stage_d(kit, wet, frame);
    build_stage_e(kit, wet, frame);
    build_stage_f(kit, wet, frame);
    build_plate_and_header(kit, wet, frame);
    build_climbing_route(kit);
    (void)kit.add_body(Sim::kWetFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F,
                       0.8F);
}

} // namespace scraperx::sim::bands
