// AS-006, the Counterweight Well (Atlas band B02, 154 -> 220 m). Every lift in
// this band runs on a counterweight; see 03_EXECUTION/ASCENT/AS-006_CW_PIN.md.
//
// Positions are world metres against the frame measured at 77a4364: the
// stair's top deck at 154.00 (north band z in [-133, -124]), the well's guard
// rail on z = -133 (top rail 155.01-155.09), and the rings above at 176.25,
// 198.25 and 220.25.

#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;

constexpr float kDeckTop = 154.00F;

// ---- Stage A, the skip lift (archetype 01, counter-mass) --------------------
//
// The cage stands on the north deck, inside the 176 ring's inner edge
// (z = -128.91), clear of the guard rail on z = -133. Its floor top starts at
// 154.25 and is governed to 176.25, flush with the 176 ring.
constexpr float kCageCenterX = -10.50F;
constexpr float kCageCenterZ = -131.40F;
constexpr float kCageHalfX = 1.50F;
constexpr float kCageHalfZ = 1.40F;
constexpr float kCageFloorHalfY = 0.10F;
constexpr float kCageFloorTop = 154.25F;
constexpr float kCageOriginY = kCageFloorTop - kCageFloorHalfY;
constexpr float kCagePostHeight = 2.70F;
constexpr float kCageMassKg = 350.0F;
constexpr float kCageTravel = 22.0F;          // 154.25 -> 176.25
constexpr float kCageGovernorSpeed = 2.50F;   // m/s
constexpr float kCageGovernorForce = 9000.0F; // N, brake only
constexpr float kCageLevelAccel = 2.0F;       // m/s^2 into each stop
// The lifting eye, on a bracket 0.12 m proud of the cage's west face at
// knee height, directly under its head sheave. It is below the hands on
// purpose: the rope is exactly long enough to reach the eye with the skip
// in its catch, so any carried height above the eye leaves the rope slack
// and a shackle can be walked to it from anywhere within 2 m of the sheave.
const JPH::Vec3 kCageEyeLocal(-kCageHalfX - 0.12F, 0.80F, 0.0F);

// The skip: 800 kg of billets on its own guide in the well, south of the
// guard rail, caught at the top. It falls 22 m as the cage rises 22 m.
constexpr float kSkipCenterX = -10.50F;
constexpr float kSkipCenterZ = -135.00F;
constexpr float kSkipHalfX = 0.80F;
constexpr float kSkipHalfY = 0.90F;
constexpr float kSkipHalfZ = 0.70F;
constexpr float kSkipTopCenterY = 188.00F;
constexpr float kSkipMassKg = 800.0F;

// Head sheaves under the head beam at 191.0: one over the cage's eye, one
// over the skip's bail.
constexpr float kSheaveY = 191.00F;

// The catch lever hangs from the head beam beside the skip's catch. Its trip
// line runs from the lever's free end down, west of the cage all the way, to
// a sheave under a gantry that spans the cage's open north side 2.35 m over
// the deck, along the gantry to a second sheave in front of the opening, and
// down to a handle at a rider's hand height. The rider takes the handle from
// inside the cage and steps back: the line lengthens and turns the lever.
// Let go, the handle is reeled back through the opening, clear of the posts.
//
// The lever is counterweighted: 60 kg of uniform steel, a 1 m arm to the
// west and a block to the east, so its weight holds the arm up against its
// stop (about 115 N m) and the handle's pull turns it down.
const JPH::RVec3 kLeverPivot(-11.60, 189.80, -133.90);
constexpr float kLeverArm = 1.00F;            // pivot to the trip line's eye, pointing west
constexpr float kLeverMassKg = 60.0F;
constexpr float kLeverReleaseAngle = 0.50F;   // rad, the pin has left its hole
constexpr float kLeverTravel = 1.20F;         // rad, to its lower stop
constexpr float kGantryZ = -129.55F;
constexpr float kGantryWestX = -12.55F;
constexpr float kGantryEastX = -8.60F;
constexpr float kGantryBeamY = 156.35F;
const JPH::RVec3 kTripSheaveWest(-12.20, 156.25, kGantryZ);
const JPH::RVec3 kTripSheaveEast(-11.00, 156.25, kGantryZ);
constexpr float kTripHandleDrop = 0.75F;      // east sheave to the handle's top
constexpr float kTripHandleHalfY = 0.04F;

// The bollard beside the cage's west side, south of its eye, where the rope
// is made fast as found. Its eye is kBollardSlack nearer the head sheave
// than the cage's eye: made fast here the rope holds the skip if the catch
// is tripped, and the skip drops only that far, inside the catch's seat.
// From inside the cage a rider reaches it facing south-west and the cage's
// eye facing west.
constexpr float kBollardX = -12.45F;
constexpr float kBollardZ = -132.30F;
constexpr float kBollardHalfXZ = 0.12F;
constexpr float kBollardSlack = 0.03F;

void build_stage_a(kit::Kit &kit, CounterweightWell &well) {
    using Sim = Simulation;
    // ---- the cage -----------------------------------------------------------
    const float post_y = kCageFloorHalfY + 0.5F * kCagePostHeight;
    std::vector<Part> cage{
        {JPH::Vec3(kCageHalfX, kCageFloorHalfY, kCageHalfZ), JPH::Vec3::sZero(),
         JPH::Quat::sIdentity(), Material::Galvanised},
    };
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            cage.push_back({JPH::Vec3(0.05F, 0.5F * kCagePostHeight, 0.05F),
                            JPH::Vec3(sx * (kCageHalfX - 0.05F), post_y, sz * (kCageHalfZ - 0.05F)),
                            JPH::Quat::sIdentity(), Material::Yellow});
        }
    }
    // The top frame, a ring of four rails at post height: the cage is open on
    // top, so a load can be dropped into it, and open on its north and east
    // sides, where the deck, the 176 ring and the next cage are.
    const float top_y = kCageFloorHalfY + kCagePostHeight;
    cage.push_back({JPH::Vec3(kCageHalfX, 0.06F, 0.05F), JPH::Vec3(0.0F, top_y, kCageHalfZ - 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kCageHalfX, 0.06F, 0.05F), JPH::Vec3(0.0F, top_y, -kCageHalfZ + 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kCageHalfZ), JPH::Vec3(-kCageHalfX + 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kCageHalfZ), JPH::Vec3(kCageHalfX - 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    // Waist rails on the well side (south) and the sheave side (west).
    cage.push_back({JPH::Vec3(kCageHalfX, 0.45F, 0.04F), JPH::Vec3(0.0F, 0.55F, -kCageHalfZ + 0.04F),
                    JPH::Quat::sIdentity(), Material::Galvanised});
    cage.push_back({JPH::Vec3(0.04F, 0.45F, kCageHalfZ), JPH::Vec3(-kCageHalfX + 0.04F, 0.55F, 0.0F),
                    JPH::Quat::sIdentity(), Material::Galvanised});
    // The eye bracket.
    cage.push_back({JPH::Vec3(0.12F, 0.06F, 0.06F), kCageEyeLocal + JPH::Vec3(0.06F, 0.0F, 0.0F),
                    JPH::Quat::sIdentity(), Material::Hazard});
    well.a_cage = kit.add_body(Sim::kWellACageEntityId, cage,
                               JPH::RVec3(kCageCenterX, kCageOriginY, kCageCenterZ),
                               JPH::Quat::sIdentity(), kCageMassKg, 0.9F);
    well.a_cage_guide = kit.add_guide(well.a_cage, JPH::Vec3::sAxisY(), 0.0F, kCageTravel,
                                      kCageGovernorSpeed, kCageGovernorForce, kCageLevelAccel);
    well.a_cage_anchor = kit.add_anchor(well.a_cage, kCageEyeLocal, 1.2F);

    // ---- the skip -----------------------------------------------------------
    well.a_skip = kit.add_body(
        Sim::kWellASkipEntityId,
        {{JPH::Vec3(kSkipHalfX, kSkipHalfY, kSkipHalfZ), JPH::Vec3::sZero(),
          JPH::Quat::sIdentity(), Material::Rust},
         {JPH::Vec3(0.06F, 0.12F, 0.06F), JPH::Vec3(0.0F, kSkipHalfY + 0.12F, 0.0F),
          JPH::Quat::sIdentity(), Material::Hazard}},
        JPH::RVec3(kSkipCenterX, kSkipTopCenterY, kSkipCenterZ), JPH::Quat::sIdentity(),
        kSkipMassKg, 0.6F);
    well.a_skip_guide =
        kit.add_guide(well.a_skip, JPH::Vec3::sAxisY(), -kCageTravel, 0.0F, 0.0F, 0.0F, 0.0F);

    // ---- the frame: head post and beam, guide rails, bollard, buffer --------
    const JPH::RVec3 eye_bottom = JPH::RVec3(kCageCenterX, kCageOriginY, kCageCenterZ) +
                                  JPH::RVec3(kCageEyeLocal);
    const JPH::RVec3 cage_sheave(eye_bottom.GetX(), kSheaveY, eye_bottom.GetZ());
    const JPH::RVec3 skip_sheave(kSkipCenterX, kSheaveY, kSkipCenterZ);
    // The rope's free length below the cage's sheave, with the skip in its
    // catch, and the height at which the bollard's eye takes kBollardSlack
    // off it.
    const float free_length = JPH::Vec3(eye_bottom - cage_sheave).Length();
    const float bollard_offset = std::hypot(kBollardX - cage_sheave.GetX(),
                                            kBollardZ - cage_sheave.GetZ());
    const float bollard_eye_y =
        kSheaveY - std::sqrt((free_length - kBollardSlack) * (free_length - kBollardSlack) -
                             bollard_offset * bollard_offset);
    const float rail_mid = 0.5F * (kDeckTop + 179.4F);
    std::vector<Part> frame{
        // Head post on the deck, west of the cage.
        {JPH::Vec3(0.20F, 0.5F * (kSheaveY + 0.6F - kDeckTop), 0.20F),
         JPH::Vec3(-13.20F, 0.5F * (kSheaveY + 0.6F + kDeckTop), -132.20F),
         JPH::Quat::sIdentity(), Material::Rust},
        // Head beam over the cage's eye and the skip's bail.
        {JPH::Vec3(1.80F, 0.15F, 2.25F), JPH::Vec3(-11.60F, kSheaveY + 0.40F, -133.25F),
         JPH::Quat::sIdentity(), Material::Rust},
        // Sheave housings.
        {JPH::Vec3(0.18F, 0.18F, 0.10F),
         JPH::Vec3(cage_sheave.GetX(), kSheaveY + 0.07F, cage_sheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(kSkipCenterX, kSheaveY + 0.07F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The lever's hanger, from the head beam's underside to the pivot,
        // on the lever's south side and clear of it at every angle.
        {JPH::Vec3(0.06F, 0.5F * (kSheaveY + 0.25F - kLeverPivot.GetY()), 0.06F),
         JPH::Vec3(kLeverPivot.GetX(), 0.5F * (kSheaveY + 0.25F + kLeverPivot.GetY()),
                   kLeverPivot.GetZ() - 0.25F),
         JPH::Quat::sIdentity(), Material::Rust},
        // Cage guide rails, south of the cage and north of the guard rail.
        {JPH::Vec3(0.04F, 0.5F * (179.4F - kDeckTop), 0.03F),
         JPH::Vec3(kCageCenterX - 1.20F, rail_mid, -132.895F), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.04F, 0.5F * (179.4F - kDeckTop), 0.03F),
         JPH::Vec3(kCageCenterX + 1.20F, rail_mid, -132.895F), JPH::Quat::sIdentity(),
         Material::Steel},
        // Skip guide rails, hung from the head beam.
        {JPH::Vec3(0.03F, 13.50F, 0.06F), JPH::Vec3(kSkipCenterX - kSkipHalfX - 0.08F, 177.60F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.03F, 13.50F, 0.06F), JPH::Vec3(kSkipCenterX + kSkipHalfX + 0.08F, 177.60F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Steel},
        // The skip's buffer at the foot of its guide, under the skip at the
        // end of its travel.
        {JPH::Vec3(0.40F, 0.25F, 0.30F),
         JPH::Vec3(kSkipCenterX, kSkipTopCenterY - kCageTravel - kSkipHalfY - 0.25F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The bollard, up to its eye.
        {JPH::Vec3(kBollardHalfXZ, 0.5F * (bollard_eye_y - 0.02F - kDeckTop), kBollardHalfXZ),
         JPH::Vec3(kBollardX, 0.5F * (bollard_eye_y - 0.02F + kDeckTop), kBollardZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The trip line's gantry: two posts, a beam, and its two sheaves.
        {JPH::Vec3(0.05F, 0.5F * (kGantryBeamY + 0.05F - kDeckTop), 0.05F),
         JPH::Vec3(kGantryWestX, 0.5F * (kGantryBeamY + 0.05F + kDeckTop), kGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.5F * (kGantryBeamY + 0.05F - kDeckTop), 0.05F),
         JPH::Vec3(kGantryEastX, 0.5F * (kGantryBeamY + 0.05F + kDeckTop), kGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.5F * (kGantryEastX - kGantryWestX) + 0.05F, 0.05F, 0.05F),
         JPH::Vec3(0.5F * (kGantryEastX + kGantryWestX), kGantryBeamY, kGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kTripSheaveWest.GetX(), kTripSheaveWest.GetY(), kGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kTripSheaveEast.GetX(), kTripSheaveEast.GetY(), kGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
    };
    const kit::BodyIndex frame_body =
        kit.add_body(Sim::kWellAFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                     0.0F, 0.8F);
    well.a_bollard_anchor =
        kit.add_anchor(frame_body, JPH::Vec3(kBollardX, bollard_eye_y, kBollardZ), 1.2F);

    // ---- the rope and its shackle ------------------------------------------
    // Its length is exact for the cage's eye at the bottom and the skip at the
    // top: hooked to the cage, it is taut, and nothing drops before it lifts.
    const JPH::RVec3 bail(kSkipCenterX, kSkipTopCenterY + kSkipHalfY + 0.24F, kSkipCenterZ);
    const float length = free_length + JPH::Vec3(bail - skip_sheave).Length();
    const JPH::RVec3 shackle_at(kBollardX, bollard_eye_y + 0.11F, kBollardZ);
    well.a_shackle = kit.add_body(
        Sim::kWellAShackleEntityId,
        {{JPH::Vec3(0.09F, 0.11F, 0.05F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
          Material::Hazard}},
        shackle_at, JPH::Quat::sIdentity(), 8.0F, 0.6F);
    kit.set_carry(well.a_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.11F, 0.0F));
    well.a_rope = kit.add_rope(well.a_skip, JPH::Vec3(0.0F, kSkipHalfY + 0.24F, 0.0F), skip_sheave,
                               well.a_shackle, JPH::Vec3(0.0F, -0.11F, 0.0F), cage_sheave, 1.0F,
                               length, 0.0F);
    // As found: made fast on the bollard.
    (void)kit.hook(Sim::kWellAShackleEntityId, well.a_bollard_anchor);

    // ---- the catch, its lever and the trip line ----------------------------
    // Built about its pivot: the arm west, the counterweight east. Uniform
    // density puts the centre of mass 0.19 m east of the pivot.
    well.a_lever_body = kit.add_body(
        Sim::kWellALeverEntityId,
        {{JPH::Vec3(0.5F * kLeverArm, 0.05F, 0.05F), JPH::Vec3(-0.5F * kLeverArm, 0.0F, 0.0F),
          JPH::Quat::sIdentity(), Material::Hazard},
         {JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(0.35F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
          Material::Rust}},
        kLeverPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    // About +z with the arm pointing west, so turning its free end down is a
    // positive angle.
    well.a_lever = kit.add_lever(well.a_lever_body, kLeverPivot, JPH::Vec3::sAxisZ(),
                                 -JPH::Vec3::sAxisX(), 0.0F, kLeverTravel);
    well.a_catch = kit.add_catch(well.a_skip, well.a_lever, kLeverReleaseAngle, 0.05F, true);
    // The handle hangs from the gantry's east sheave by its top.
    const JPH::Vec3 handle_top(0.0F, kTripHandleHalfY, 0.0F);
    well.a_handle = kit.add_body(
        Sim::kWellAHandleEntityId,
        {{JPH::Vec3(0.22F, kTripHandleHalfY, 0.04F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
          Material::Yellow}},
        kTripSheaveEast - JPH::RVec3(0.0, kTripHandleDrop + kTripHandleHalfY, 0.0),
        JPH::Quat::sIdentity(), 3.0F, 0.9F);
    kit.set_carry(well.a_handle, kit::CarryKind::Handle, handle_top);
    kit.set_damping(well.a_handle, 1.5F, 1.5F);
    (void)kit.add_trip_line(well.a_lever_body, JPH::Vec3(-kLeverArm, 0.0F, 0.0F), well.a_handle,
                            handle_top, kTripSheaveWest, kTripSheaveEast);
}

// ---- Stage B, the guided lattice counterweight ------------------------------
//
// A 2.5 t, 22 m lattice is stored vertically between the 198 and 220 rings.
// The player moves its rope end from a bollard to the adjacent cage and trips
// its catch. Gravity lowers the lattice 22 m while the cage rises 22 m. At
// rest the lattice spans 176.25..198.25 m as permanent physical structure.
constexpr float kBCageCenterX = -7.35F;
constexpr float kBCageCenterZ = -131.40F;
constexpr float kBCageHalfX = 1.45F;
constexpr float kBCageHalfZ = 1.40F;
constexpr float kBCageFloorHalfY = 0.10F;
constexpr float kBCageFloorTop = 176.25F;
constexpr float kBCageOriginY = kBCageFloorTop - kBCageFloorHalfY;
constexpr float kBCagePostHeight = 2.70F;
constexpr float kBCageMassKg = 350.0F;
constexpr float kBTravel = 22.0F;
constexpr float kBCageGovernorSpeed = 3.00F;
constexpr float kBCageGovernorForce = 30000.0F;
constexpr float kBCageLevelAccel = 2.5F;
const JPH::Vec3 kBCageEyeLocal(kBCageHalfX + 0.12F, 0.80F, 0.0F);

constexpr float kBWeightCenterX = -6.50F;
constexpr float kBWeightCenterZ = -135.60F;
constexpr float kBWeightHalfX = 0.62F;
constexpr float kBWeightHalfY = 11.00F;
constexpr float kBWeightHalfZ = 0.16F;
constexpr float kBWeightTopCenterY = 209.25F;
constexpr float kBWeightMassKg = 2500.0F;
constexpr float kBSheaveY = 223.00F;

const JPH::RVec3 kBLeverPivot(-8.75, 221.20, -135.60);
constexpr float kBLeverArm = 1.00F;
constexpr float kBLeverMassKg = 60.0F;
constexpr float kBLeverReleaseAngle = 0.50F;
constexpr float kBLeverTravel = 1.20F;
constexpr float kBGantryZ = -129.55F;
constexpr float kBGantryWestX = -9.70F;
constexpr float kBGantryEastX = -6.10F;
constexpr float kBGantryBeamY = 178.60F;
const JPH::RVec3 kBTripSheaveWest(-9.35, 178.50, kBGantryZ);
const JPH::RVec3 kBTripSheaveEast(-7.35, 178.50, kBGantryZ);
constexpr float kBTripHandleDrop = 0.75F;
constexpr float kBTripHandleHalfY = 0.04F;
constexpr float kBBollardX = -8.70F;
constexpr float kBBollardZ = -132.25F;
constexpr float kBBollardHalfXZ = 0.12F;
constexpr float kBBollardSlack = 0.03F;

void build_stage_b(kit::Kit &kit, CounterweightWell &well) {
    using Sim = Simulation;

    const float post_y = kBCageFloorHalfY + 0.5F * kBCagePostHeight;
    std::vector<Part> cage{
        {JPH::Vec3(kBCageHalfX, kBCageFloorHalfY, kBCageHalfZ), JPH::Vec3::sZero(),
         JPH::Quat::sIdentity(), Material::Galvanised},
    };
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            cage.push_back({JPH::Vec3(0.05F, 0.5F * kBCagePostHeight, 0.05F),
                            JPH::Vec3(sx * (kBCageHalfX - 0.05F), post_y,
                                      sz * (kBCageHalfZ - 0.05F)),
                            JPH::Quat::sIdentity(), Material::Yellow});
        }
    }
    const float top_y = kBCageFloorHalfY + kBCagePostHeight;
    cage.push_back({JPH::Vec3(kBCageHalfX, 0.06F, 0.05F),
                    JPH::Vec3(0.0F, top_y, kBCageHalfZ - 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kBCageHalfX, 0.06F, 0.05F),
                    JPH::Vec3(0.0F, top_y, -kBCageHalfZ + 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kBCageHalfZ),
                    JPH::Vec3(-kBCageHalfX + 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kBCageHalfZ),
                    JPH::Vec3(kBCageHalfX - 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kBCageHalfX, 0.45F, 0.04F),
                    JPH::Vec3(0.0F, 0.55F, -kBCageHalfZ + 0.04F),
                    JPH::Quat::sIdentity(), Material::Galvanised});
    cage.push_back({JPH::Vec3(0.12F, 0.06F, 0.06F),
                    kBCageEyeLocal - JPH::Vec3(0.06F, 0.0F, 0.0F),
                    JPH::Quat::sIdentity(), Material::Hazard});
    well.b_cage = kit.add_body(
        Sim::kWellBCageEntityId, cage,
        JPH::RVec3(kBCageCenterX, kBCageOriginY, kBCageCenterZ),
        JPH::Quat::sIdentity(), kBCageMassKg, 0.9F);
    well.b_cage_guide = kit.add_guide(well.b_cage, JPH::Vec3::sAxisY(), 0.0F, kBTravel,
                                      kBCageGovernorSpeed, kBCageGovernorForce,
                                      kBCageLevelAccel);
    well.b_cage_anchor = kit.add_anchor(well.b_cage, kBCageEyeLocal, 1.2F);

    std::vector<Part> lattice;
    lattice.reserve(15);
    lattice.push_back({JPH::Vec3(0.06F, kBWeightHalfY, 0.06F),
                       JPH::Vec3(-0.50F, 0.0F, 0.0F),
                       JPH::Quat::sIdentity(), Material::Rust});
    lattice.push_back({JPH::Vec3(0.06F, kBWeightHalfY, 0.06F),
                       JPH::Vec3(0.50F, 0.0F, 0.0F),
                       JPH::Quat::sIdentity(), Material::Rust});
    for (int rung = -5; rung <= 5; ++rung) {
        lattice.push_back({JPH::Vec3(kBWeightHalfX, 0.045F, kBWeightHalfZ),
                           JPH::Vec3(0.0F, static_cast<float>(rung) * 2.0F, 0.0F),
                           JPH::Quat::sIdentity(),
                           (rung % 2 == 0) ? Material::Hazard : Material::Galvanised});
    }
    lattice.push_back({JPH::Vec3(kBWeightHalfX, 0.12F, kBWeightHalfZ),
                       JPH::Vec3(0.0F, kBWeightHalfY - 0.12F, 0.0F),
                       JPH::Quat::sIdentity(), Material::Yellow});
    lattice.push_back({JPH::Vec3(kBWeightHalfX, 0.12F, kBWeightHalfZ),
                       JPH::Vec3(0.0F, -kBWeightHalfY + 0.12F, 0.0F),
                       JPH::Quat::sIdentity(), Material::Yellow});
    well.b_counterweight = kit.add_body(
        Sim::kWellBCounterweightEntityId, lattice,
        JPH::RVec3(kBWeightCenterX, kBWeightTopCenterY, kBWeightCenterZ),
        JPH::Quat::sIdentity(), kBWeightMassKg, 0.8F);
    well.b_counterweight_guide =
        kit.add_guide(well.b_counterweight, JPH::Vec3::sAxisY(), -kBTravel, 0.0F,
                      0.0F, 0.0F, 0.0F);

    const JPH::RVec3 cage_eye =
        JPH::RVec3(kBCageCenterX, kBCageOriginY, kBCageCenterZ) + JPH::RVec3(kBCageEyeLocal);
    const JPH::RVec3 cage_sheave(cage_eye.GetX(), kBSheaveY, cage_eye.GetZ());
    const JPH::RVec3 weight_bail(kBWeightCenterX,
                                 kBWeightTopCenterY + kBWeightHalfY + 0.20F,
                                 kBWeightCenterZ);
    const JPH::RVec3 weight_sheave(kBWeightCenterX, kBSheaveY, kBWeightCenterZ);
    const float free_length = JPH::Vec3(cage_eye - cage_sheave).Length();
    const float bollard_offset =
        std::hypot(kBBollardX - cage_sheave.GetX(), kBBollardZ - cage_sheave.GetZ());
    const float bollard_eye_y =
        kBSheaveY - std::sqrt((free_length - kBBollardSlack) *
                                  (free_length - kBBollardSlack) -
                              bollard_offset * bollard_offset);

    const float cage_rail_mid = 0.5F * (176.00F + 201.4F);
    std::vector<Part> frame{
        {JPH::Vec3(0.20F, 23.70F, 0.20F),
         JPH::Vec3(-4.90F, 199.70F, -133.80F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(2.20F, 0.16F, 2.45F),
         JPH::Vec3(-6.85F, kBSheaveY + 0.40F, -133.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.18F, 0.18F, 0.10F),
         JPH::Vec3(cage_sheave.GetX(), kBSheaveY + 0.07F, cage_sheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.18F, 0.18F, 0.10F),
         JPH::Vec3(weight_sheave.GetX(), kBSheaveY + 0.07F, weight_sheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.04F, 12.70F, 0.03F),
         JPH::Vec3(kBCageCenterX - 1.15F, cage_rail_mid, -132.895F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.04F, 12.70F, 0.03F),
         JPH::Vec3(kBCageCenterX + 1.15F, cage_rail_mid, -132.895F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.03F, 23.4F, 0.06F),
         JPH::Vec3(kBWeightCenterX - 0.82F, 198.25F, kBWeightCenterZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.03F, 23.4F, 0.06F),
         JPH::Vec3(kBWeightCenterX + 0.82F, 198.25F, kBWeightCenterZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(kBBollardHalfXZ,
                   0.5F * (bollard_eye_y - 0.02F - 176.00F),
                   kBBollardHalfXZ),
         JPH::Vec3(kBBollardX,
                   0.5F * (bollard_eye_y - 0.02F + 176.00F),
                   kBBollardZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.05F, 0.5F * (kBGantryBeamY + 0.05F - 176.00F), 0.05F),
         JPH::Vec3(kBGantryWestX, 0.5F * (kBGantryBeamY + 0.05F + 176.00F), kBGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.5F * (kBGantryBeamY + 0.05F - 176.00F), 0.05F),
         JPH::Vec3(kBGantryEastX, 0.5F * (kBGantryBeamY + 0.05F + 176.00F), kBGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.5F * (kBGantryEastX - kBGantryWestX) + 0.05F, 0.05F, 0.05F),
         JPH::Vec3(0.5F * (kBGantryEastX + kBGantryWestX), kBGantryBeamY, kBGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
    };
    const kit::BodyIndex frame_body =
        kit.add_body(Sim::kWellBFrameEntityId, frame, JPH::RVec3::sZero(),
                     JPH::Quat::sIdentity(), 0.0F, 0.8F);
    well.b_bollard_anchor =
        kit.add_anchor(frame_body, JPH::Vec3(kBBollardX, bollard_eye_y, kBBollardZ), 1.2F);

    const float rope_length =
        JPH::Vec3(weight_bail - weight_sheave).Length() + free_length;
    well.b_shackle = kit.add_body(
        Sim::kWellBShackleEntityId,
        {{JPH::Vec3(0.09F, 0.11F, 0.05F), JPH::Vec3::sZero(),
          JPH::Quat::sIdentity(), Material::Hazard}},
        JPH::RVec3(kBBollardX, bollard_eye_y + 0.11F, kBBollardZ),
        JPH::Quat::sIdentity(), 8.0F, 0.6F);
    kit.set_carry(well.b_shackle, kit::CarryKind::Shackle,
                  JPH::Vec3(0.0F, 0.11F, 0.0F));
    well.b_rope =
        kit.add_rope(well.b_counterweight,
                     JPH::Vec3(0.0F, kBWeightHalfY + 0.20F, 0.0F),
                     weight_sheave, well.b_shackle,
                     JPH::Vec3(0.0F, -0.11F, 0.0F), cage_sheave,
                     1.0F, rope_length, 0.0F);
    (void)kit.hook(Sim::kWellBShackleEntityId, well.b_bollard_anchor);

    well.b_lever_body = kit.add_body(
        Sim::kWellBLeverEntityId,
        {{JPH::Vec3(0.5F * kBLeverArm, 0.05F, 0.05F),
          JPH::Vec3(-0.5F * kBLeverArm, 0.0F, 0.0F),
          JPH::Quat::sIdentity(), Material::Hazard},
         {JPH::Vec3(0.15F, 0.25F, 0.15F),
          JPH::Vec3(0.35F, 0.0F, 0.0F),
          JPH::Quat::sIdentity(), Material::Rust}},
        kBLeverPivot, JPH::Quat::sIdentity(), kBLeverMassKg, 0.5F);
    well.b_lever =
        kit.add_lever(well.b_lever_body, kBLeverPivot, JPH::Vec3::sAxisZ(),
                      -JPH::Vec3::sAxisX(), 0.0F, kBLeverTravel);
    well.b_catch =
        kit.add_catch(well.b_counterweight, well.b_lever,
                      kBLeverReleaseAngle, 0.05F, true);

    const JPH::Vec3 handle_top(0.0F, kBTripHandleHalfY, 0.0F);
    well.b_handle = kit.add_body(
        Sim::kWellBHandleEntityId,
        {{JPH::Vec3(0.22F, kBTripHandleHalfY, 0.04F), JPH::Vec3::sZero(),
          JPH::Quat::sIdentity(), Material::Yellow}},
        kBTripSheaveEast -
            JPH::RVec3(0.0, kBTripHandleDrop + kBTripHandleHalfY, 0.0),
        JPH::Quat::sIdentity(), 3.0F, 0.9F);
    kit.set_carry(well.b_handle, kit::CarryKind::Handle, handle_top);
    kit.set_damping(well.b_handle, 1.5F, 1.5F);
    (void)kit.add_trip_line(well.b_lever_body,
                            JPH::Vec3(-kBLeverArm, 0.0F, 0.0F),
                            well.b_handle, handle_top,
                            kBTripSheaveWest, kBTripSheaveEast);
}

} // namespace

void build_counterweight_well(kit::Kit &kit, CounterweightWell &well) {
    build_stage_a(kit, well);
    build_stage_b(kit, well);
}

} // namespace scraperx::sim::bands
