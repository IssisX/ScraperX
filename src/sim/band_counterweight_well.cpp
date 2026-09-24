// AS-006, the Counterweight Well (Atlas band B02, 154 -> 220 m). Every lift in
// this band runs on a counterweight; see 03_EXECUTION/ASCENT/AS-006_CW_PIN.md.
//
// Positions are world metres against the frame measured at 77a4364: the
// stair's top deck at 154.00 (north band z in [-133, -124]), the well's guard
// rail on z = -133 (top rail 155.01-155.09), and the rings above at 176.25,
// 198.25 and 220.25.

#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <algorithm>
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

// A lift cage about its floor's centre: a galvanised floor, four yellow
// posts, a top frame of four rails, and waist rails on the well side (south)
// and, with side_rail, on the eye's side; open on top, so a load can be
// dropped into it, and open on the north. eye_side is -1 for an eye on the
// west face, +1 for the east.
[[nodiscard]] std::vector<Part> cage_parts(const float eye_side, const bool side_rail) {
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
    const float top_y = kCageFloorHalfY + kCagePostHeight;
    cage.push_back({JPH::Vec3(kCageHalfX, 0.06F, 0.05F), JPH::Vec3(0.0F, top_y, kCageHalfZ - 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kCageHalfX, 0.06F, 0.05F), JPH::Vec3(0.0F, top_y, -kCageHalfZ + 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kCageHalfZ), JPH::Vec3(-kCageHalfX + 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kCageHalfZ), JPH::Vec3(kCageHalfX - 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kCageHalfX, 0.45F, 0.04F), JPH::Vec3(0.0F, 0.55F, -kCageHalfZ + 0.04F),
                    JPH::Quat::sIdentity(), Material::Galvanised});
    if (side_rail) {
        cage.push_back({JPH::Vec3(0.04F, 0.45F, kCageHalfZ),
                        JPH::Vec3(eye_side * (kCageHalfX - 0.04F), 0.55F, 0.0F),
                        JPH::Quat::sIdentity(), Material::Galvanised});
    }
    // The eye bracket, 0.12 m proud of the eye's face.
    cage.push_back({JPH::Vec3(0.12F, 0.06F, 0.06F),
                    JPH::Vec3(eye_side * (kCageHalfX + 0.06F), kCageEyeLocal.GetY(), 0.0F),
                    JPH::Quat::sIdentity(), Material::Hazard});
    return cage;
}

void build_stage_a(kit::Kit &kit, CounterweightWell &well) {
    using Sim = Simulation;
    // ---- the cage -----------------------------------------------------------
    const std::vector<Part> cage = cage_parts(-1.0F, true);
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
        // The head frame: an arm from the post out over the cage's eye, and
        // a diagonal out to a crossbeam over the skip's bail. It leaves the
        // column over the cage's east half open: Stage C's dumpster comes
        // down it to the parked cage.
        {JPH::Vec3(0.15F, 0.15F, 0.40F), JPH::Vec3(-13.20F, kSheaveY + 0.40F, -131.80F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.725F, 0.15F, 0.15F), JPH::Vec3(-12.675F, kSheaveY + 0.40F, -131.40F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(2.15F, 0.15F, 0.15F), JPH::Vec3(-11.85F, kSheaveY + 0.40F, -133.60F),
         JPH::Quat::sRotation(JPH::Vec3::sAxisY(), 0.8034F), Material::Rust},
        {JPH::Vec3(1.00F, 0.15F, 0.15F), JPH::Vec3(kSkipCenterX, kSheaveY + 0.40F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Rust},
        // Sheave housings.
        {JPH::Vec3(0.18F, 0.18F, 0.10F),
         JPH::Vec3(cage_sheave.GetX(), kSheaveY + 0.07F, cage_sheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(kSkipCenterX, kSheaveY + 0.07F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The lever's hanger, from the diagonal down to the pivot, on the
        // lever's south side and clear of it at every angle.
        {JPH::Vec3(0.06F, 0.5F * (kSheaveY + 0.25F - kLeverPivot.GetY()), 0.06F),
         JPH::Vec3(kLeverPivot.GetX(), 0.5F * (kSheaveY + 0.25F + kLeverPivot.GetY()),
                   kLeverPivot.GetZ() - 0.25F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.08F, 0.08F, 0.12F),
         JPH::Vec3(kLeverPivot.GetX(), kSheaveY + 0.30F, kLeverPivot.GetZ() - 0.13F),
         JPH::Quat::sIdentity(), Material::Rust},
        // Cage guide rails, south of the cage and north of the guard rail.
        {JPH::Vec3(0.04F, 0.5F * (179.4F - kDeckTop), 0.03F),
         JPH::Vec3(kCageCenterX - 1.20F, rail_mid, -132.895F), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.04F, 0.5F * (179.4F - kDeckTop), 0.03F),
         JPH::Vec3(kCageCenterX + 1.20F, rail_mid, -132.895F), JPH::Quat::sIdentity(),
         Material::Steel},
        // Skip guide rails, hung from the crossbeam.
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

// ---- Stage B, the derrick boom (archetype 13, the boom as a counterweight) --
//
// B's cage rests on its bottom stop in the well with its floor at 176.25,
// 0.3 m east of where A's cage parks: A's parked cage is the step into it.
// It rides 22 m to 198.25, flush with the 198 ring. What lifts it is a 22 m,
// 2 500 kg lattice boom pivoted off the 220 ring and held level over the
// well by a catch at its rest. Its hoist line runs from a two-part block at
// the boom's tip up to a block under the 242 ring's inner edge (S2), across
// to a block under the 220 ring's edge (S), and down to B's cage's eye: the
// boom swinging down pulls the tip away from S2, and the cage rises twice as
// far as the tip draws away. A statics sweep over S2 (scratch, recorded in
// AS-006) gives the full 22 m at a boom angle of about 61 degrees with the
// boom's weight out-pulling the line by at least 76 kN m all the way.
//
// Found, the line's end is made fast on a cleat hanging from the gantry in
// front of B's cage. At the top the cage's west top rail lifts a striker that
// opens the slip hook on the line's dead end at S2: the line runs out, the
// boom swings on to hang from its pivot, and B's cage stays up on its
// safety dogs. Nothing puts the boom back: one-shot.
constexpr float kBCageCenterX = -7.20F;
constexpr float kBCageFloorTop = 176.25F;
constexpr float kBCageOriginY = kBCageFloorTop - kCageFloorHalfY;
constexpr float kBGovernorSpeed = 3.0F;
constexpr float kBGovernorForce = 15000.0F;  // N, brake only: the boom out-pulls 9 kN near the top
constexpr float kBDogPitch = 0.05F;
const JPH::Vec3 kBCageEyeLocal(kCageHalfX + 0.12F, kCageEyeLocal.GetY(), 0.0F);

constexpr float kBoomX = 0.00F;
constexpr float kBoomLength = 22.0F;
constexpr float kBoomHalfSection = 0.34F;   // chord centres from the boom's axis
constexpr float kBoomMassKg = 2500.0F;
const JPH::RVec3 kBoomPivot(kBoomX, 222.00, -131.30);
// The two-part block, just under the tip.
const JPH::Vec3 kBoomBlockLocal(0.0F, -0.45F, -21.70F);
// S, over B's eye under the 220 ring's inner edge; S2, under the 242 ring's.
constexpr float kBSheaveY = 219.40F;
const JPH::RVec3 kBSheave2(kBoomX, 241.40, -131.90);

// The boom's catch lever, on a stand on the 220 ring west of the boom, and
// its trip line down to a gantry spanning B's opening at 176.
const JPH::RVec3 kBLeverPivot(-1.20, 222.60, -131.00);
const JPH::RVec3 kBTripSheaveEast(-5.00, 178.60, -129.45);
const JPH::RVec3 kBTripSheaveWest(-8.00, 178.60, -129.45);
constexpr float kBGantryZ = -129.45F;
constexpr float kBGantryWestX = -8.60F;
constexpr float kBGantryEastX = -4.60F;
constexpr float kBGantryBeamY = 178.70F;
constexpr float kBGantryPostZ = -128.70F;   // on the 176 ring, inside its edge
// The cleat the line is found on, hanging from the gantry beam in front of
// the middle of B's opening: taken from inside the cage it comes in through
// the opening, clear of the corner posts. The handle hangs 1.7 m west of it.
constexpr float kBCleatX = -6.30F;

// The striker: an arm pivoted west of B's travel at the top, lying across
// the cage's west top rail's path 6 cm below where the rail stops: the rail
// turns it about 0.2 rad, past its release.
const JPH::RVec3 kBStrikerPivot(-8.95, 200.98, -131.40);
constexpr float kBStrikerArm = 0.90F;
constexpr float kBStrikerRelease = 0.12F;

// The boom's lattice about its pivot, pointing south (-z) when level: four
// chords, a batten on each face every 2 m, a diagonal in each side bay, and
// the two-part block at the tip.
[[nodiscard]] std::vector<Part> boom_parts() {
    const float half = 0.5F * kBoomLength;
    std::vector<Part> boom;
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sy : {-1.0F, 1.0F}) {
            boom.push_back({JPH::Vec3(0.06F, 0.06F, half),
                            JPH::Vec3(sx * kBoomHalfSection, sy * kBoomHalfSection, -half),
                            JPH::Quat::sIdentity(), Material::Yellow});
        }
    }
    const float bay = 2.0F;
    for (int station = 0; station <= 11; ++station) {
        const float z = -bay * static_cast<float>(station);
        const float zc = std::max(-kBoomLength + 0.04F, std::min(-0.04F, z));
        for (const float sy : {-1.0F, 1.0F}) {
            boom.push_back({JPH::Vec3(kBoomHalfSection, 0.03F, 0.03F),
                            JPH::Vec3(0.0F, sy * kBoomHalfSection, zc), JPH::Quat::sIdentity(),
                            Material::Yellow});
        }
        for (const float sx : {-1.0F, 1.0F}) {
            boom.push_back({JPH::Vec3(0.03F, kBoomHalfSection, 0.03F),
                            JPH::Vec3(sx * kBoomHalfSection, 0.0F, zc), JPH::Quat::sIdentity(),
                            Material::Yellow});
        }
        if (station == 11) {
            break;
        }
        const float lean = std::atan2(2.0F * kBoomHalfSection, bay);
        const float along = 0.5F * std::sqrt(bay * bay + 4.0F * kBoomHalfSection * kBoomHalfSection);
        for (const float sx : {-1.0F, 1.0F}) {
            const float sign = (station % 2 == 0) ? 1.0F : -1.0F;
            boom.push_back({JPH::Vec3(0.025F, 0.025F, along),
                            JPH::Vec3(sx * kBoomHalfSection, 0.0F, z - 0.5F * bay),
                            JPH::Quat::sRotation(JPH::Vec3::sAxisX(), sign * lean), Material::Rust});
        }
    }
    boom.push_back({JPH::Vec3(0.15F, 0.15F, 0.15F), kBoomBlockLocal, JPH::Quat::sIdentity(),
                    Material::Hazard});
    return boom;
}

void build_stage_b(kit::Kit &kit, CounterweightWell &well) {
    using Sim = Simulation;
    // ---- the cage, its dogs and its eye -------------------------------------
    well.b_cage = kit.add_body(Sim::kWellBCageEntityId, cage_parts(1.0F, false),
                               JPH::RVec3(kBCageCenterX, kBCageOriginY, kCageCenterZ),
                               JPH::Quat::sIdentity(), kCageMassKg, 0.9F);
    well.b_cage_guide = kit.add_guide(well.b_cage, JPH::Vec3::sAxisY(), 0.0F, kCageTravel,
                                      kBGovernorSpeed, kBGovernorForce, kCageLevelAccel);
    kit.set_dogs(well.b_cage_guide, kBDogPitch);
    well.b_cage_anchor = kit.add_anchor(well.b_cage, kBCageEyeLocal, 1.2F);

    // ---- the boom, on its hinge, caught level at its rest -------------------
    well.b_boom = kit.add_body(Sim::kWellBBoomEntityId, boom_parts(), kBoomPivot,
                               JPH::Quat::sIdentity(), kBoomMassKg, 0.7F);
    kit.set_damping(well.b_boom, 0.05F, 0.4F);
    // About -x with the boom pointing south, so swinging down is positive; it
    // hangs straight down at pi/2.
    well.b_boom_hinge = kit.add_lever(well.b_boom, kBoomPivot, -JPH::Vec3::sAxisX(),
                                      -JPH::Vec3::sAxisZ(), 0.0F, 0.5F * JPH::JPH_PI);

    // ---- the catch lever and its trip line -----------------------------------
    well.b_lever_body = kit.add_body(
        Sim::kWellBLeverEntityId,
        {{JPH::Vec3(0.5F * kLeverArm, 0.05F, 0.05F), JPH::Vec3(-0.5F * kLeverArm, 0.0F, 0.0F),
          JPH::Quat::sIdentity(), Material::Hazard},
         {JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(0.35F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
          Material::Rust}},
        kBLeverPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    well.b_lever = kit.add_lever(well.b_lever_body, kBLeverPivot, JPH::Vec3::sAxisZ(),
                                 -JPH::Vec3::sAxisX(), 0.0F, kLeverTravel);
    well.b_catch = kit.add_catch(well.b_boom, well.b_lever, kLeverReleaseAngle, 0.05F, true);
    const JPH::Vec3 handle_top(0.0F, kTripHandleHalfY, 0.0F);
    well.b_handle = kit.add_body(
        Sim::kWellBHandleEntityId,
        {{JPH::Vec3(0.22F, kTripHandleHalfY, 0.04F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
          Material::Yellow}},
        kBTripSheaveWest - JPH::RVec3(0.0, kTripHandleDrop + kTripHandleHalfY, 0.0),
        JPH::Quat::sIdentity(), 3.0F, 0.9F);
    kit.set_carry(well.b_handle, kit::CarryKind::Handle, handle_top);
    kit.set_damping(well.b_handle, 1.5F, 1.5F);
    (void)kit.add_trip_line(well.b_lever_body, JPH::Vec3(-kLeverArm, 0.0F, 0.0F), well.b_handle,
                            handle_top, kBTripSheaveEast, kBTripSheaveWest);

    // ---- the striker ----------------------------------------------------------
    // Rests on its stop by its own weight, level; lifted, it turns about +z.
    well.b_striker_body = kit.add_body(
        Sim::kWellBStrikerEntityId,
        {{JPH::Vec3(0.5F * kBStrikerArm, 0.03F, 0.04F), JPH::Vec3(0.5F * kBStrikerArm, 0.0F, 0.0F),
          JPH::Quat::sIdentity(), Material::Hazard}},
        kBStrikerPivot, JPH::Quat::sIdentity(), 5.0F, 0.5F);
    well.b_striker = kit.add_lever(well.b_striker_body, kBStrikerPivot, JPH::Vec3::sAxisZ(),
                                   JPH::Vec3::sAxisX(), 0.0F, 1.0F);

    // ---- the frame: stands, blocks, gantry, cleat, rails ---------------------
    const JPH::RVec3 eye_bottom =
        JPH::RVec3(kBCageCenterX, kBCageOriginY, kCageCenterZ) + JPH::RVec3(kBCageEyeLocal);
    const JPH::RVec3 sheave(eye_bottom.GetX(), kBSheaveY, eye_bottom.GetZ());
    const float free_length = JPH::Vec3(eye_bottom - sheave).Length();
    const float cleat_offset =
        std::hypot(kBCleatX - sheave.GetX(), kBGantryZ - sheave.GetZ());
    const float cleat_eye_y =
        kBSheaveY - std::sqrt((free_length - kBollardSlack) * (free_length - kBollardSlack) -
                              cleat_offset * cleat_offset);
    const float rail_bottom = kBCageFloorTop - 1.0F;
    const float rail_top = kBCageFloorTop + kCageTravel + 3.2F;
    std::vector<Part> frame{
        // Cage guide rails south of the cage, hung from brackets off the rings.
        {JPH::Vec3(0.04F, 0.5F * (rail_top - rail_bottom), 0.03F),
         JPH::Vec3(kBCageCenterX - 1.20F, 0.5F * (rail_top + rail_bottom), -132.895F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.04F, 0.5F * (rail_top - rail_bottom), 0.03F),
         JPH::Vec3(kBCageCenterX + 1.20F, 0.5F * (rail_top + rail_bottom), -132.895F),
         JPH::Quat::sIdentity(), Material::Steel},
        // The boom's side plates on the 220 ring, a pin between them.
        {JPH::Vec3(0.05F, 1.10F, 0.45F), JPH::Vec3(kBoomX - 0.55F, 221.35F, -130.95F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.05F, 1.10F, 0.45F), JPH::Vec3(kBoomX + 0.55F, 221.35F, -130.95F),
         JPH::Quat::sIdentity(), Material::Rust},
        // The lever's stand and pin.
        {JPH::Vec3(0.06F, 0.5F * (kBLeverPivot.GetY() + 0.35F - 220.25F), 0.06F),
         JPH::Vec3(kBLeverPivot.GetX(), 0.5F * (kBLeverPivot.GetY() + 0.35F + 220.25F), -130.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.04F, 0.04F, 0.16F),
         JPH::Vec3(kBLeverPivot.GetX(), kBLeverPivot.GetY(), -130.61F), JPH::Quat::sIdentity(),
         Material::Steel},
        // S, under the 220 ring's inner edge, and its bracket.
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(sheave.GetX(), kBSheaveY + 0.07F, sheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.08F, 0.08F, 0.45F),
         JPH::Vec3(sheave.GetX(), kBSheaveY + 0.30F, 0.5F * (sheave.GetZ() - 130.40F)),
         JPH::Quat::sIdentity(), Material::Rust},
        // S2 and the slip hook, under the 242 ring's inner edge.
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(kBoomX, kBSheave2.GetY() + 0.07F, kBSheave2.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.08F, 0.08F, 0.25F),
         JPH::Vec3(kBoomX, kBSheave2.GetY() + 0.30F, kBSheave2.GetZ() + 0.20F),
         JPH::Quat::sIdentity(), Material::Rust},
        // The gantry over B's opening: posts on the 176 ring, arms, beam.
        {JPH::Vec3(0.05F, 0.5F * (kBGantryBeamY + 0.05F - kBCageFloorTop), 0.05F),
         JPH::Vec3(kBGantryWestX, 0.5F * (kBGantryBeamY + 0.05F + kBCageFloorTop), kBGantryPostZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.5F * (kBGantryBeamY + 0.05F - kBCageFloorTop), 0.05F),
         JPH::Vec3(kBGantryEastX, 0.5F * (kBGantryBeamY + 0.05F + kBCageFloorTop), kBGantryPostZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.05F, 0.5F * (kBGantryPostZ - kBGantryZ) + 0.05F),
         JPH::Vec3(kBGantryWestX, kBGantryBeamY, 0.5F * (kBGantryPostZ + kBGantryZ)),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.05F, 0.5F * (kBGantryPostZ - kBGantryZ) + 0.05F),
         JPH::Vec3(kBGantryEastX, kBGantryBeamY, 0.5F * (kBGantryPostZ + kBGantryZ)),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.5F * (kBGantryEastX - kBGantryWestX) + 0.05F, 0.05F, 0.05F),
         JPH::Vec3(0.5F * (kBGantryEastX + kBGantryWestX), kBGantryBeamY, kBGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.06F, 0.05F, 0.03F),
         JPH::Vec3(kBTripSheaveEast.GetX(), kBTripSheaveEast.GetY(), kBGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.06F, 0.05F, 0.03F),
         JPH::Vec3(kBTripSheaveWest.GetX(), kBTripSheaveWest.GetY(), kBGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The cleat: a drop bar from the beam down to the line's eye.
        {JPH::Vec3(0.03F, 0.5F * (kBGantryBeamY - 0.05F - cleat_eye_y), 0.03F),
         JPH::Vec3(kBCleatX, 0.5F * (kBGantryBeamY - 0.05F + cleat_eye_y), kBGantryZ),
         JPH::Quat::sIdentity(), Material::Steel},
        // The striker's bracket: a post on the 198 ring, an arm out to the
        // pivot, and its stop under the arm's heel.
        {JPH::Vec3(0.05F, 0.5F * (kBStrikerPivot.GetY() + 0.35F - 198.25F), 0.05F),
         JPH::Vec3(kBStrikerPivot.GetX(), 0.5F * (kBStrikerPivot.GetY() + 0.35F + 198.25F), -129.60F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.05F, 0.05F, 0.5F * (kBStrikerPivot.GetZ() + 129.60F) * -1.0F + 0.05F),
         JPH::Vec3(kBStrikerPivot.GetX() - 0.10F, kBStrikerPivot.GetY() + 0.30F,
                   0.5F * (kBStrikerPivot.GetZ() - 129.60F)),
         JPH::Quat::sIdentity(), Material::Rust},
    };
    const kit::BodyIndex frame_body = kit.add_body(Sim::kWellBFrameEntityId, frame,
                                                   JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                                                   0.0F, 0.8F);
    well.b_cleat_anchor =
        kit.add_anchor(frame_body, JPH::Vec3(kBCleatX, cleat_eye_y, kBGantryZ), 1.2F);

    // ---- the hoist line --------------------------------------------------------
    // Exactly taut at the eye with the boom at its rest: the boom's block's
    // distance to S2, plus half the eye's distance to S (the two parts at the
    // tip are the first end's side, ratio 0.5 on the end's).
    const JPH::RVec3 block = kBoomPivot + JPH::RVec3(kBoomBlockLocal);
    const float length = JPH::Vec3(block - kBSheave2).Length() + 0.5F * free_length;
    well.b_shackle = kit.add_body(
        Sim::kWellBShackleEntityId,
        {{JPH::Vec3(0.09F, 0.11F, 0.05F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
          Material::Hazard}},
        JPH::RVec3(kBCleatX, cleat_eye_y + 0.11F, kBGantryZ), JPH::Quat::sIdentity(), 8.0F, 0.6F);
    kit.set_carry(well.b_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.11F, 0.0F));
    well.b_rope = kit.add_rope(well.b_boom, kBoomBlockLocal, kBSheave2, well.b_shackle,
                               JPH::Vec3(0.0F, -0.11F, 0.0F), sheave, 0.5F, length, 0.0F);
    (void)kit.hook(Sim::kWellBShackleEntityId, well.b_cleat_anchor);
    well.b_slip = kit.add_slip(well.b_rope, well.b_striker, kBStrikerRelease);
}

// ---- Stage C, the debris chute (archetype 12), the band's finale -----------
//
// C's platform stands in the well 0.5 m east of B's parked cage, its floor at
// 198.25, and rides 22 m to 220.25 on a 1:1 rope over two blocks on gallows
// on the 220 ring, down to a steel dumpster hanging over A's parked cage.
// Empty, the dumpster (250 kg) is lighter than the platform (700 kg): the
// platform rests on its stop and the rope holds the dumpster up at the top
// of its guide, under the mouth of a rubble hopper on the 198 ring. The
// hopper holds 900 kg; a length of rebar jams its mouth shut.
//
// Found: the platform held down by its keeper latch, the chute jammed. The
// rider pulls the latch lever back (the missing link: the platform is free)
// and pulls the rebar's trip line from a handle in front of the platform
// (the trigger): rubble pours into the dumpster, and once it outweighs
// platform and rider it sinks, lifting them. Latched, the full dumpster just
// hangs. The platform's dogs hold it at the top.
//
// The cascade into A: the dumpster comes down its guide to just above A's
// parked cage, where it presses a striker that opens its bottom gate, and
// the rubble drops into A's cage. A's cage then outweighs A's skip, sinks to
// 154 and hauls the skip back up into its catch: C's spent source re-arms
// A. At 154 a tip-out lever beside the cage opens the cage's floor gate and
// the rubble spills onto the deck. Rubble is the declared granular model of
// the kit's bins (mechanism_kit.hpp), not bodies.
constexpr float kCPlatformX = -3.70F;
constexpr float kCPlatformZ = -132.45F;
constexpr float kCPlatformHalfX = 1.50F;
constexpr float kCPlatformHalfZ = 1.25F;
constexpr float kCPlatformFloorTop = 198.25F;
constexpr float kCPlatformOriginY = kCPlatformFloorTop - kCageFloorHalfY;
constexpr float kCPlatformMassKg = 700.0F;
constexpr float kCGovernorSpeed = 3.0F;
constexpr float kCGovernorForce = 12000.0F;
const JPH::Vec3 kCPlatformEyeLocal(0.0F, kCageEyeLocal.GetY(), -kCPlatformHalfZ - 0.12F);
constexpr float kCBlockY = 224.20F;
constexpr float kCGallowsZ = -130.40F;   // posts on the 220 ring, inside its edge

constexpr float kDumpsterX = -10.15F;
constexpr float kDumpsterZ = -131.75F;
constexpr float kDumpsterHalfX = 0.95F;
constexpr float kDumpsterHalfY = 0.65F;
constexpr float kDumpsterHalfZ = 0.85F;
constexpr float kDumpsterBottomTop = 201.30F;   // its floor's underside, found
constexpr float kDumpsterMassKg = 250.0F;
constexpr float kDumpsterCapacityKg = 1000.0F;
const JPH::Vec3 kDumpsterBailLocal(0.0F, kDumpsterHalfY + 0.50F, 0.0F);

constexpr float kHopperRubbleKg = 900.0F;
constexpr float kRubbleFlow = 150.0F;           // kg/s through an open mouth
constexpr float kTipOutFlow = 450.0F;           // kg/s through A's floor gate
const JPH::RVec3 kHopperMouth(kDumpsterX, 203.40, kDumpsterZ);

// The rebar: a 32 mm bar jammed across the chute's mouth, pinned at its west
// end. Its trip line runs from a lug 0.5 m along it up over a sheave on a
// bracket west of the hopper, then across and down to a gantry in front of
// C's platform. The sheave stands 0.8 m west of the pin, so a full pull (0.7 m
// of line) draws the bar up past upright (the line can turn it to about
// 1.88 rad): over the top, its own weight throws it back onto its far stop
// and the mouth stays clear. Let go short of upright, it drops back into the
// mouth.
const JPH::RVec3 kRebarPivot(kDumpsterX - 0.85, 203.70, kDumpsterZ);   // clear of the chute's lip
constexpr float kRebarLength = 1.70F;
constexpr float kRebarMassKg = 12.0F;
constexpr float kRebarLug = 0.50F;
constexpr float kRebarOpen = 0.45F;
constexpr float kRebarFarStop = 2.20F;   // rad, lying back west of its pin
const JPH::RVec3 kRebarSheave(kRebarPivot.GetX() - 0.80, kRebarPivot.GetY() + 2.50, kDumpsterZ);
// The gantry in front of C's platform, on the 198 ring: a beam at 201.2 with
// the rebar's and the latch's lower sheaves hung a metre under it on drop
// bars, half a metre over a rider's hands. A rider stepping back from a
// handle draws its line out nearly level, not up a line hanging straight
// down, where the first metre of a step pays out almost none.
constexpr float kCGantryBeamY = 201.20F;
const JPH::RVec3 kCTripSheaveWest(-4.40, 200.10, -130.75);
constexpr float kCTripHandleDrop = 0.55F;   // the handle's top at a rider's hands
constexpr float kCGantryZ = -130.75F;
constexpr float kCGantryPostZ = -129.60F;   // on the 198 ring

// The keeper latch: a counterweighted lever over the gantry's east end, its
// arm's tip straight above the gantry sheave its own handle hangs from, 1.4 m
// east of the rebar's. Pull the handle down and away: the tip comes down and
// the latch's pin leaves the platform, at 0.25 rad, a quarter metre of pull.
// The lever's stop (0.55 rad) is well short of where the line would come
// square to the arm (about 1.07 rad).
const JPH::RVec3 kCLatchPivot(-2.10, 201.75, -130.75);
constexpr float kCLatchArm = 0.90F;
const JPH::RVec3 kCLatchSheave(-3.00, 200.10, -130.75);
constexpr float kCLatchRelease = 0.25F;
constexpr float kCLatchTravel = 0.55F;

// The dumpster's gate striker, over A's parked cage, and A's tip-out: a
// counterweighted lever over A's gantry whose handle hangs in front of A's
// opening east of A's trip handle. Its linkage reaches the cage's floor gate
// only while the cage stands at the foot of its travel.
//
// The striker lies north-south in A's column, above A's parked cage's top
// frame and north of the dumpster's column, its arm reaching south under the
// dumpster's floor: nothing of it stands in B's column or the dumpster's.
const JPH::RVec3 kGateStrikerPivot(kDumpsterX, 179.45, -130.55);
constexpr float kGateStrikerArm = 1.00F;
constexpr float kGateOpen = 0.12F;
const JPH::RVec3 kTipOutPivot(-8.70, 156.90, -129.55);
const JPH::RVec3 kTipOutSheave(-9.60, 156.25, -129.55);
// The keeper latch's release and stop. The tip-out's sheave is 0.65 m under
// the arm's tip, so its line comes square to the arm at about 0.62 rad, past
// the stop; a held handle keeps the gate open across 0.2 m of the stroke.
constexpr float kTipOutOpen = kCLatchRelease;
constexpr float kTipOutTravel = kCLatchTravel;

// A counterweighted lever about +z with its arm pointing west: 60 kg of
// uniform steel, like A's catch lever, resting on its stop at 0.
[[nodiscard]] std::vector<Part> trip_lever_parts(const float arm) {
    return {
        {JPH::Vec3(0.5F * arm, 0.05F, 0.05F), JPH::Vec3(-0.5F * arm, 0.0F, 0.0F),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(0.35F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
         Material::Rust},
    };
}

// A trip handle hanging kDrop below a sheave, damped like the others.
kit::BodyIndex add_hanging_handle(kit::Kit &kit, const std::uint64_t entity,
                                  const JPH::RVec3 sheave, const float drop) {
    const kit::BodyIndex handle = kit.add_body(
        entity,
        {{JPH::Vec3(0.22F, kTripHandleHalfY, 0.04F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
          Material::Yellow}},
        sheave - JPH::RVec3(0.0, drop + kTripHandleHalfY, 0.0), JPH::Quat::sIdentity(), 3.0F,
        0.9F);
    kit.set_carry(handle, kit::CarryKind::Handle, JPH::Vec3(0.0F, kTripHandleHalfY, 0.0F));
    kit.set_damping(handle, 1.5F, 1.5F);
    return handle;
}

void build_stage_c(kit::Kit &kit, CounterweightWell &well) {
    using Sim = Simulation;
    // ---- the platform ------------------------------------------------------------
    const float rail_y = kCageFloorHalfY + 0.55F;
    std::vector<Part> platform{
        {JPH::Vec3(kCPlatformHalfX, kCageFloorHalfY, kCPlatformHalfZ), JPH::Vec3::sZero(),
         JPH::Quat::sIdentity(), Material::Timber},
        // Waist rails on the south (the rope's side) and east sides.
        {JPH::Vec3(kCPlatformHalfX, 0.45F, 0.04F),
         JPH::Vec3(0.0F, rail_y, -kCPlatformHalfZ + 0.04F), JPH::Quat::sIdentity(),
         Material::Yellow},
        {JPH::Vec3(0.04F, 0.45F, kCPlatformHalfZ),
         JPH::Vec3(kCPlatformHalfX - 0.04F, rail_y, 0.0F), JPH::Quat::sIdentity(),
         Material::Yellow},
        // The eye bracket, 0.12 m proud of the south rail.
        {JPH::Vec3(0.06F, 0.06F, 0.12F),
         JPH::Vec3(0.0F, kCPlatformEyeLocal.GetY(), -kCPlatformHalfZ - 0.06F),
         JPH::Quat::sIdentity(), Material::Hazard},
    };
    well.c_platform = kit.add_body(Sim::kWellCPlatformEntityId, platform,
                                   JPH::RVec3(kCPlatformX, kCPlatformOriginY, kCPlatformZ),
                                   JPH::Quat::sIdentity(), kCPlatformMassKg, 0.9F);
    well.c_platform_guide = kit.add_guide(well.c_platform, JPH::Vec3::sAxisY(), 0.0F,
                                          kCageTravel, kCGovernorSpeed, kCGovernorForce,
                                          kCageLevelAccel);
    kit.set_dogs(well.c_platform_guide, kBDogPitch);

    // ---- the keeper latch ----------------------------------------------------------
    well.c_latch_body = kit.add_body(Sim::kWellCLatchEntityId, trip_lever_parts(kCLatchArm),
                                     kCLatchPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    well.c_latch = kit.add_lever(well.c_latch_body, kCLatchPivot, JPH::Vec3::sAxisZ(),
                                 -JPH::Vec3::sAxisX(), 0.0F, kCLatchTravel);
    well.c_catch = kit.add_catch(well.c_platform, well.c_latch, kCLatchRelease, 0.05F, true);
    well.c_latch_handle =
        add_hanging_handle(kit, Sim::kWellCLatchHandleEntityId, kCLatchSheave, kCTripHandleDrop);
    (void)kit.add_trip_line(well.c_latch_body, JPH::Vec3(-kCLatchArm, 0.0F, 0.0F),
                            well.c_latch_handle, JPH::Vec3(0.0F, kTripHandleHalfY, 0.0F),
                            kCLatchSheave, kCLatchSheave);

    // ---- the dumpster on its guide -------------------------------------------------
    const float wall = 0.05F;
    std::vector<Part> dumpster{
        {JPH::Vec3(kDumpsterHalfX, wall, kDumpsterHalfZ),
         JPH::Vec3(0.0F, -kDumpsterHalfY + wall, 0.0F), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(wall, kDumpsterHalfY, kDumpsterHalfZ),
         JPH::Vec3(-kDumpsterHalfX + wall, 0.0F, 0.0F), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(wall, kDumpsterHalfY, kDumpsterHalfZ),
         JPH::Vec3(kDumpsterHalfX - wall, 0.0F, 0.0F), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(kDumpsterHalfX, kDumpsterHalfY, wall),
         JPH::Vec3(0.0F, 0.0F, -kDumpsterHalfZ + wall), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(kDumpsterHalfX, kDumpsterHalfY, wall),
         JPH::Vec3(0.0F, 0.0F, kDumpsterHalfZ - wall), JPH::Quat::sIdentity(), Material::Steel},
        // The bail: two uprights and a crossbar over the open top.
        {JPH::Vec3(0.04F, 0.30F, 0.04F), JPH::Vec3(-kDumpsterHalfX + wall, kDumpsterHalfY + 0.25F, 0.0F),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.04F, 0.30F, 0.04F), JPH::Vec3(kDumpsterHalfX - wall, kDumpsterHalfY + 0.25F, 0.0F),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(kDumpsterHalfX, 0.04F, 0.04F), JPH::Vec3(0.0F, kDumpsterHalfY + 0.51F, 0.0F),
         JPH::Quat::sIdentity(), Material::Hazard},
    };
    const float dumpster_center_y = kDumpsterBottomTop + kDumpsterHalfY;
    well.c_dumpster = kit.add_body(Sim::kWellCDumpsterEntityId, dumpster,
                                   JPH::RVec3(kDumpsterX, dumpster_center_y, kDumpsterZ),
                                   JPH::Quat::sIdentity(), kDumpsterMassKg, 0.6F);
    well.c_dumpster_guide = kit.add_guide(well.c_dumpster, JPH::Vec3::sAxisY(), -kCageTravel,
                                          0.0F, 0.0F, 0.0F, 0.0F);

    // ---- the rope: platform's eye over C1 and C2 to the dumpster's bail --------------
    const JPH::RVec3 eye = JPH::RVec3(kCPlatformX, kCPlatformOriginY, kCPlatformZ) +
                           JPH::RVec3(kCPlatformEyeLocal);
    const JPH::RVec3 block1(eye.GetX(), kCBlockY, eye.GetZ());
    const JPH::RVec3 bail = JPH::RVec3(kDumpsterX, dumpster_center_y, kDumpsterZ) +
                            JPH::RVec3(kDumpsterBailLocal);
    const JPH::RVec3 block2(kDumpsterX, kCBlockY, kDumpsterZ);
    const float length = JPH::Vec3(eye - block1).Length() + JPH::Vec3(bail - block2).Length();
    well.c_rope = kit.add_rope(well.c_platform, kCPlatformEyeLocal, block1, well.c_dumpster,
                               kDumpsterBailLocal, block2, 1.0F, length, 0.0F);

    // ---- the rebar and its trip line -------------------------------------------------
    well.c_rebar_body = kit.add_body(
        Sim::kWellCRebarEntityId,
        {{JPH::Vec3(0.5F * kRebarLength, 0.025F, 0.025F), JPH::Vec3(0.5F * kRebarLength, 0.0F, 0.0F),
          JPH::Quat::sIdentity(), Material::Rust}},
        kRebarPivot, JPH::Quat::sIdentity(), kRebarMassKg, 0.6F);
    // About +z from its west pin, so lifting the east end is positive; its
    // own weight holds it down on the chute's lip, its stop at 0.
    well.c_rebar = kit.add_lever(well.c_rebar_body, kRebarPivot, JPH::Vec3::sAxisZ(),
                                 JPH::Vec3::sAxisX(), 0.0F, kRebarFarStop);
    const JPH::Vec3 handle_top(0.0F, kTripHandleHalfY, 0.0F);
    well.c_handle =
        add_hanging_handle(kit, Sim::kWellCHandleEntityId, kCTripSheaveWest, kCTripHandleDrop);
    (void)kit.add_trip_line(well.c_rebar_body, JPH::Vec3(kRebarLug, 0.0F, 0.0F), well.c_handle,
                            handle_top, kRebarSheave, kCTripSheaveWest);

    // ---- the gate striker over A's cage, and A's tip-out lever ------------------------
    // The striker rests level on its stop, its arm south under the dumpster's
    // floor and its weight north of its pin (uniform density puts the centre
    // of mass 0.09 m north); the dumpster's last 0.18 m of travel presses it
    // down, about 0.47 rad at the floor's north edge.
    well.c_striker_body = kit.add_body(
        Sim::kWellCStrikerEntityId,
        {{JPH::Vec3(0.05F, 0.03F, 0.5F * kGateStrikerArm), JPH::Vec3(0.0F, 0.0F, -0.5F * kGateStrikerArm),
          JPH::Quat::sIdentity(), Material::Hazard},
         {JPH::Vec3(0.14F, 0.14F, 0.14F), JPH::Vec3(0.0F, 0.0F, 0.25F), JPH::Quat::sIdentity(),
          Material::Rust}},
        kGateStrikerPivot, JPH::Quat::sIdentity(), 12.0F, 0.5F);
    // About -x with the arm pointing south, so pressing it down is positive.
    well.c_striker = kit.add_lever(well.c_striker_body, kGateStrikerPivot, -JPH::Vec3::sAxisX(),
                                   -JPH::Vec3::sAxisZ(), 0.0F, 0.6F);
    well.a_tip_body = kit.add_body(Sim::kWellATipOutEntityId, trip_lever_parts(kCLatchArm),
                                   kTipOutPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    well.a_tip = kit.add_lever(well.a_tip_body, kTipOutPivot, JPH::Vec3::sAxisZ(),
                               -JPH::Vec3::sAxisX(), 0.0F, kTipOutTravel);
    well.a_tip_handle = add_hanging_handle(kit, Sim::kWellATipHandleEntityId, kTipOutSheave,
                                           kTripHandleDrop);
    (void)kit.add_trip_line(well.a_tip_body, JPH::Vec3(-kCLatchArm, 0.0F, 0.0F), well.a_tip_handle,
                            JPH::Vec3(0.0F, kTripHandleHalfY, 0.0F), kTipOutSheave, kTipOutSheave);

    // ---- the bins: hopper, dumpster, A's cage -----------------------------------------
    // The hopper is static: rubble in it has no weight on anything moving.
    const std::vector<Part> hopper{
        {JPH::Vec3(1.00F, 1.25F, 1.20F), JPH::Vec3(kDumpsterX, 205.75F, -128.00F),
         JPH::Quat::sIdentity(), Material::Rust},
        // Legs to the 198 ring.
        {JPH::Vec3(0.08F, 0.5F * (204.5F - 198.25F), 0.08F),
         JPH::Vec3(kDumpsterX - 0.9F, 0.5F * (204.5F + 198.25F), -127.0F), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.08F, 0.5F * (204.5F - 198.25F), 0.08F),
         JPH::Vec3(kDumpsterX + 0.9F, 0.5F * (204.5F + 198.25F), -127.0F), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.08F, 0.5F * (204.5F - 198.25F), 0.08F),
         JPH::Vec3(kDumpsterX - 0.9F, 0.5F * (204.5F + 198.25F), -129.0F), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.08F, 0.5F * (204.5F - 198.25F), 0.08F),
         JPH::Vec3(kDumpsterX + 0.9F, 0.5F * (204.5F + 198.25F), -129.0F), JPH::Quat::sIdentity(),
         Material::Steel},
        // The chute: from under the bin's south face down to the mouth.
        {JPH::Vec3(0.45F, 0.06F, 1.55F), JPH::Vec3(kDumpsterX, 203.95F, -130.40F),
         JPH::Quat::sRotation(JPH::Vec3::sAxisX(), -0.33F), Material::Galvanised},
    };
    const kit::BodyIndex hopper_body = kit.add_body(Sim::kWellCHopperEntityId, hopper,
                                                    JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                                                    0.0F, 0.8F);
    well.c_hopper = kit.add_bin(hopper_body, kHopperRubbleKg, kHopperRubbleKg,
                                JPH::Vec3(kHopperMouth), well.c_rebar, kRebarOpen, 1.5F,
                                kRubbleFlow);
    well.c_dumpster_bin = kit.add_bin(well.c_dumpster, 0.0F, kDumpsterCapacityKg,
                                      JPH::Vec3(0.0F, -kDumpsterHalfY - 0.02F, 0.0F),
                                      well.c_striker, kGateOpen, 1.5F, kRubbleFlow);
    // A's floor gate is the whole floor's width: it dumps a load in two
    // seconds, as long as the tip-out handle is held down.
    well.a_cage_bin = kit.add_bin(well.a_cage, 0.0F, kDumpsterCapacityKg,
                                  JPH::Vec3(0.0F, -kCageFloorHalfY - 0.02F, 0.0F), well.a_tip,
                                  kTipOutOpen, 4.0F, kTipOutFlow);

    // ---- the frame: rails, gallows, blocks, gantry, pins ------------------------------
    const float c_rail_bottom = kCPlatformFloorTop - 1.0F;
    const float c_rail_top = kCPlatformFloorTop + kCageTravel + 1.5F;
    // The dumpster's rails stop just under its floor at the end of its
    // travel: lower, the east one stands in the top of A's cage's column.
    const float d_rail_bottom = kDumpsterBottomTop - kCageTravel - 0.10F;
    const float d_rail_top = kDumpsterBottomTop + 2.0F;
    std::vector<Part> frame{
        // Platform guide rails south of it.
        {JPH::Vec3(0.04F, 0.5F * (c_rail_top - c_rail_bottom), 0.03F),
         JPH::Vec3(kCPlatformX - 1.2F, 0.5F * (c_rail_top + c_rail_bottom), kCPlatformZ - kCPlatformHalfZ - 0.34F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.04F, 0.5F * (c_rail_top - c_rail_bottom), 0.03F),
         JPH::Vec3(kCPlatformX + 1.2F, 0.5F * (c_rail_top + c_rail_bottom), kCPlatformZ - kCPlatformHalfZ - 0.34F),
         JPH::Quat::sIdentity(), Material::Steel},
        // Dumpster guide rails, west and east of it.
        {JPH::Vec3(0.03F, 0.5F * (d_rail_top - d_rail_bottom), 0.06F),
         JPH::Vec3(kDumpsterX - kDumpsterHalfX - 0.08F, 0.5F * (d_rail_top + d_rail_bottom), kDumpsterZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.03F, 0.5F * (d_rail_top - d_rail_bottom), 0.06F),
         JPH::Vec3(kDumpsterX + kDumpsterHalfX + 0.08F, 0.5F * (d_rail_top + d_rail_bottom), kDumpsterZ),
         JPH::Quat::sIdentity(), Material::Steel},
        // Gallows on the 220 ring for the two blocks.
        {JPH::Vec3(0.10F, 0.5F * (kCBlockY + 0.4F - 220.25F), 0.10F),
         JPH::Vec3(block1.GetX(), 0.5F * (kCBlockY + 0.4F + 220.25F), kCGallowsZ),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.08F, 0.08F, 0.5F * (kCGallowsZ - block1.GetZ())),
         JPH::Vec3(block1.GetX(), kCBlockY + 0.3F, 0.5F * (kCGallowsZ + block1.GetZ())),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.10F, 0.5F * (kCBlockY + 0.4F - 220.25F), 0.10F),
         JPH::Vec3(block2.GetX(), 0.5F * (kCBlockY + 0.4F + 220.25F), kCGallowsZ),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.08F, 0.08F, 0.5F * (kCGallowsZ - block2.GetZ())),
         JPH::Vec3(block2.GetX(), kCBlockY + 0.3F, 0.5F * (kCGallowsZ + block2.GetZ())),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(block1.GetX(), kCBlockY + 0.07F, block1.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(block2.GetX(), kCBlockY + 0.07F, block2.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The trip lines' gantry in front of the platform, on the 198 ring:
        // two posts, their arms, and a beam running on east over the latch's
        // sheave; drop bars down to the two lower sheaves.
        {JPH::Vec3(0.05F, 0.5F * (kCGantryBeamY + 0.05F - kCPlatformFloorTop), 0.05F),
         JPH::Vec3(kCTripSheaveWest.GetX() - 1.0F, 0.5F * (kCGantryBeamY + 0.05F + kCPlatformFloorTop), kCGantryPostZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.5F * (kCGantryBeamY + 0.05F - kCPlatformFloorTop), 0.05F),
         JPH::Vec3(kCTripSheaveWest.GetX() + 1.0F, 0.5F * (kCGantryBeamY + 0.05F + kCPlatformFloorTop), kCGantryPostZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.5F * (kCLatchSheave.GetX() + 0.10F - kCTripSheaveWest.GetX() + 1.05F), 0.05F, 0.05F),
         JPH::Vec3(0.5F * (kCLatchSheave.GetX() + 0.10F + kCTripSheaveWest.GetX() - 1.05F), kCGantryBeamY, kCGantryZ),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.05F, 0.5F * (kCGantryPostZ - kCGantryZ) + 0.05F),
         JPH::Vec3(kCTripSheaveWest.GetX() - 1.0F, kCGantryBeamY, 0.5F * (kCGantryPostZ + kCGantryZ)),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.05F, 0.05F, 0.5F * (kCGantryPostZ - kCGantryZ) + 0.05F),
         JPH::Vec3(kCTripSheaveWest.GetX() + 1.0F, kCGantryBeamY, 0.5F * (kCGantryPostZ + kCGantryZ)),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.02F, 0.5F * (kCGantryBeamY - kCTripSheaveWest.GetY() - 0.10F), 0.02F),
         JPH::Vec3(kCTripSheaveWest.GetX(), 0.5F * (kCGantryBeamY + kCTripSheaveWest.GetY()), kCGantryZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kCTripSheaveWest.GetX(), kCTripSheaveWest.GetY(), kCGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.02F, 0.5F * (kCGantryBeamY - kCLatchSheave.GetY() - 0.10F), 0.02F),
         JPH::Vec3(kCLatchSheave.GetX(), 0.5F * (kCGantryBeamY + kCLatchSheave.GetY()), kCGantryZ),
         JPH::Quat::sIdentity(), Material::Steel},
        // The rebar's pin block on the chute's mouth, and the bracket off the
        // hopper's south face, west of it, that carries the trip line's first
        // sheave.
        {JPH::Vec3(0.06F, 0.06F, 0.12F), JPH::Vec3(kRebarPivot.GetX(), kRebarPivot.GetY(), kRebarPivot.GetZ() - 0.20F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.5F * (kDumpsterX - 0.75F - kRebarSheave.GetX()), 0.06F, 0.06F),
         JPH::Vec3(0.5F * (kDumpsterX - 0.75F + kRebarSheave.GetX()), kRebarSheave.GetY() + 0.12F, -129.20F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.06F, 0.5F * (-129.20F - kRebarSheave.GetZ()) + 0.06F),
         JPH::Vec3(kRebarSheave.GetX(), kRebarSheave.GetY() + 0.12F, 0.5F * (kRebarSheave.GetZ() - 129.20F)),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.06F, 0.03F), JPH::Vec3(kRebarSheave.GetX(), kRebarSheave.GetY(), kRebarSheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The latch lever's post on the 198 ring and its arm out to the pin,
        // and the latch handle's sheave on its drop bar.
        {JPH::Vec3(0.06F, 0.5F * (kCLatchPivot.GetY() + 0.35F - kCPlatformFloorTop), 0.06F),
         JPH::Vec3(kCLatchPivot.GetX(), 0.5F * (kCLatchPivot.GetY() + 0.35F + kCPlatformFloorTop), kCGantryPostZ),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.04F, 0.04F, 0.5F * (kCGantryPostZ - kCLatchPivot.GetZ()) - 0.16F),
         JPH::Vec3(kCLatchPivot.GetX(), kCLatchPivot.GetY(), 0.5F * (kCGantryPostZ + kCLatchPivot.GetZ()) + 0.08F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kCLatchSheave.GetX(), kCLatchSheave.GetY(), kCLatchSheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        // The striker's bracket: a post on the 176 ring and an arm out along
        // the striker's west side to its pin.
        {JPH::Vec3(0.06F, 0.5F * (kGateStrikerPivot.GetY() + 0.16F - 176.25F), 0.06F),
         JPH::Vec3(kGateStrikerPivot.GetX() - 0.25F, 0.5F * (kGateStrikerPivot.GetY() + 0.16F + 176.25F), -128.75F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.06F, 0.5F * (-128.75F - kGateStrikerPivot.GetZ()) + 0.06F),
         JPH::Vec3(kGateStrikerPivot.GetX() - 0.25F, kGateStrikerPivot.GetY() + 0.10F, 0.5F * (kGateStrikerPivot.GetZ() - 128.75F)),
         JPH::Quat::sIdentity(), Material::Steel},
        // The tip-out lever's post on the 154 deck north of A's gantry, its
        // pin, and its handle's sheave under the gantry's beam.
        {JPH::Vec3(0.06F, 0.5F * (kTipOutPivot.GetY() + 0.35F - kDeckTop), 0.06F),
         JPH::Vec3(kTipOutPivot.GetX(), 0.5F * (kTipOutPivot.GetY() + 0.35F + kDeckTop), kTipOutPivot.GetZ() + 0.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.04F, 0.04F, 0.12F),
         JPH::Vec3(kTipOutPivot.GetX(), kTipOutPivot.GetY(), kTipOutPivot.GetZ() + 0.30F),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kTipOutSheave.GetX(), kTipOutSheave.GetY(), kTipOutSheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
    };
    (void)kit.add_body(Sim::kWellCFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);
}

// ---- The climbing route: 154 -> 220 with no lift -----------------------------
//
// East of the machines, on the well side of the rings. Each climb faces the
// ring it tops out onto, from the well side: the rings step 0.91 m further
// into the well each level up, so a climber on the other side of a
// structure would have the next ring over their head.
//
//  154 -> 176  a rung ladder standing on the 154 m deck against the 176
//              ring's inner face (climb, mantle over the top);
//  176 -> 198  an L of scaffold boards out from the 176 ring (balance) to a
//              standpipe that rises to the 198 ring's edge (climb, mantle);
//  198 -> 220  a catwalk out from the 198 ring under a scaffold panel hung
//              off the 220 ring's face, caught with a jump (climb, mantle).
//
// Holds end at or under the deck they lead to, so the mantle over the top
// passes above them. Rails, rungs and lattice members are 60 mm or less.
constexpr float kMemberHalf = 0.03F;
constexpr float kLadderX = 5.00F;
constexpr float kLadderZ = -129.05F;     // 0.14 m off the 176 ring's face
constexpr float kLadderHalfWidth = 0.28F;
constexpr float kRungPitch = 0.30F;
constexpr float kBoardHalfWidth = 0.15F;
constexpr float kBoardTop = 176.45F;
constexpr float kPipeX = 7.60F;
constexpr float kPipeZ = -130.30F;
constexpr float kPipeTop = 198.20F;
constexpr float kCatwalkX = 11.00F;
constexpr float kPanelZ = -130.86F;      // 0.13 m off the 220 ring's face
constexpr float kPanelBottom = 200.30F;
constexpr float kPanelTop = 220.20F;

void build_climbing_route(kit::Kit &kit) {
    using Sim = Simulation;
    std::vector<Part> route;
    // The ladder: two rails and a rung every 0.3 m up to the 176 ring. The
    // rails stop at the deck's top: a body mantling over the ladder's head
    // is wider than the gap between them.
    const float rail_top = 176.25F;
    for (const float side : {-1.0F, 1.0F}) {
        route.push_back({JPH::Vec3(kMemberHalf, 0.5F * (rail_top - kDeckTop), kMemberHalf),
                         JPH::Vec3(kLadderX + side * kLadderHalfWidth, 0.5F * (rail_top + kDeckTop), kLadderZ),
                         JPH::Quat::sIdentity(), Material::Yellow});
    }
    for (float rung = kDeckTop + kRungPitch; rung <= 176.20F; rung += kRungPitch) {
        route.push_back({JPH::Vec3(kLadderHalfWidth, 0.02F, 0.02F), JPH::Vec3(kLadderX, rung, kLadderZ),
                         JPH::Quat::sIdentity(), Material::Steel});
    }
    // The boards: out south from the 176 ring, then east past the pipe.
    const float board_mid = kBoardTop - 0.10F;
    route.push_back({JPH::Vec3(kBoardHalfWidth, 0.10F, 1.225F),
                     JPH::Vec3(7.05F, board_mid, -129.825F), JPH::Quat::sIdentity(), Material::Timber});
    route.push_back({JPH::Vec3(0.80F, 0.10F, kBoardHalfWidth),
                     JPH::Vec3(7.70F, board_mid, -130.90F), JPH::Quat::sIdentity(), Material::Timber});
    // The standpipe on its foot bracket, to just under the 198 deck's top.
    route.push_back({JPH::Vec3(0.05F, 0.5F * (kPipeTop - kBoardTop), 0.05F),
                     JPH::Vec3(kPipeX, 0.5F * (kPipeTop + kBoardTop), kPipeZ), JPH::Quat::sIdentity(),
                     Material::Galvanised});
    route.push_back({JPH::Vec3(0.175F, 0.075F, 0.04F), JPH::Vec3(7.375F, 176.325F, kPipeZ),
                     JPH::Quat::sIdentity(), Material::Rust});
    // The catwalk, flush with the 198 ring, and the scaffold panel above it:
    // verticals every 0.5 m, a horizontal every 0.4 m.
    route.push_back({JPH::Vec3(0.50F, 0.05F, 1.14F), JPH::Vec3(kCatwalkX, 198.20F, -130.86F),
                     JPH::Quat::sIdentity(), Material::Galvanised});
    for (float x = 10.0F; x <= 12.01F; x += 0.5F) {
        route.push_back({JPH::Vec3(kMemberHalf, 0.5F * (kPanelTop - kPanelBottom), kMemberHalf),
                         JPH::Vec3(x, 0.5F * (kPanelTop + kPanelBottom), kPanelZ),
                         JPH::Quat::sIdentity(), Material::Yellow});
    }
    for (float y = kPanelBottom; y <= kPanelTop + 0.01F; y += 0.4F) {
        route.push_back({JPH::Vec3(1.03F, kMemberHalf, kMemberHalf), JPH::Vec3(11.0F, y, kPanelZ),
                         JPH::Quat::sIdentity(), Material::Steel});
    }
    (void)kit.add_body(Sim::kWellRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(),
                       0.0F, 0.8F);
}

} // namespace

void build_counterweight_well(kit::Kit &kit, CounterweightWell &well) {
    build_stage_a(kit, well);
    build_stage_b(kit, well);
    build_stage_c(kit, well);
    build_climbing_route(kit);
}

} // namespace scraperx::sim::bands
