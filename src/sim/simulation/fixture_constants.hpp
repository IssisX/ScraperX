#pragma once

#include "sim/simulation.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace scraperx::sim::detail {

inline constexpr float kWaterScrewX = -18.0F;
inline constexpr float kWaterScrewLowY = 0.0F;
inline constexpr float kWaterScrewLowZ = -98.0F;
inline constexpr float kWaterScrewLength = 11.0F;
inline constexpr float kWaterScrewIncline = 0.5235987756F;
inline constexpr float kWaterScrewUpperY = 5.5F;
inline constexpr float kWaterScrewUpperZ = -107.526279F;
inline constexpr float kWaterScrewStationX = -14.0F;
inline constexpr float kWaterScrewStationZ = -96.5F;
inline constexpr float kWaterScrewStationRadius = 2.6F;

// Ground water-weight lift. The valve only transfers water between owners;
// gravity, the pulley and Jolt constraints decide whether anything moves.
inline constexpr float kWaterLiftValveStationX = -14.45F;
inline constexpr float kWaterLiftValveStationY = 0.90F;
inline constexpr float kWaterLiftValveStationZ = -107.00F;
inline constexpr float kWaterLiftReleaseStationX = -14.45F;
inline constexpr float kWaterLiftReleaseStationY = 0.90F;
inline constexpr float kWaterLiftReleaseStationZ = -108.00F;
inline constexpr float kWaterLiftResetStationX = -9.45F;
inline constexpr float kWaterLiftResetStationY = 9.05F;
inline constexpr float kWaterLiftResetStationZ = -106.85F;
inline constexpr float kWaterLiftStationRadius = 1.60F;
inline constexpr double kWaterLiftBucketCapacityM3 = 2.0;
inline constexpr double kWaterLiftFillRateM3S = 0.10;
inline constexpr double kWaterLiftDrainRateM3S = 0.12;
inline constexpr double kWaterLiftBucketDryMassKg = 200.0;
inline constexpr double kWaterDensityKgM3 = 1000.0;

// --- coupled machine geometry, metres / seconds / kilograms ---------------
// The plant sits in the approach yard between grade spawn and the tower, so the
// player meets it on the way in rather than being born inside it.
inline constexpr double kMachineCyclePeriodSeconds = 26.0;
inline constexpr float kScoopX = 34.0F;
inline constexpr float kScoopZ = -96.0F;
inline constexpr float kScoopBottomY = 0.03F;
inline constexpr float kScoopTopY = 12.0F;
inline constexpr float kScoopDischargeTilt = 0.62F;
inline constexpr float kTipperHingeX = 29.0F;
inline constexpr float kTipperHingeY = 3.6F;
inline constexpr float kTipperZ = -96.0F;
inline constexpr float kValveHingeX = 29.6F;
inline constexpr float kValveHingeY = 7.2F;
inline constexpr float kRopeSlackMeters = 0.15F;
inline constexpr float kLiftMastX = 13.0F;
inline constexpr float kLiftZ = -100.0F;
inline constexpr float kLiftPlatformX = 16.4F;
inline constexpr float kLiftPlatformRestY = 1.2F;
inline constexpr float kLiftTravelMeters = 7.6F;
inline constexpr float kCounterweightX = 11.6F;
inline constexpr float kSheaveY = 10.4F;
inline constexpr float kBallastMassKg = 380.0F;
inline constexpr float kLiftPlatformMassKg = 2100.0F;
inline constexpr float kCounterweightMassKg = 1800.0F;
inline constexpr float kTipperMassKg = 900.0F;
inline constexpr float kValveLeverMassKg = 90.0F;
// Valve lever angle band that maps to a fully shut / fully open orifice.
inline constexpr float kValveShutAngle = -0.05F;
inline constexpr float kValveOpenAngle = 0.62F;

// --- WO-010 catwalk treadle ------------------------------------------------
// The player masses 85 kg and cannot shift a 900 kg counterweighted tipper, so
// body-in-the-machine has to happen through a control built for a body. The
// treadle is a see-saw on the catwalk deck: standing on the outboard end lifts
// the inboard end, which hauls a cable to the valve lever's counterweight and
// opens the orifice. Arms are equal, so this is reach and placement rather than
// force multiplication -- the valve gear only needs ~190 N.m, which is human
// scale by design, and the treadle is only reachable by someone the lift has
// already carried up (WO-009).
inline constexpr float kTreadleX = 16.4F;
inline constexpr float kTreadleZ = -106.0F;
inline constexpr float kTreadleHingeY = 9.09F;      // 0.40 m above the catwalk deck: a step, not a mantle.
inline constexpr float kTreadlePlateMeters = 1.5F;  // Plate runs from the hinge out to -x.
inline constexpr float kTreadleCableArm = 0.70F;    // Cable hangs from here, under a sheave.
inline constexpr float kTreadleMassKg = 130.0F;
// Rest is level (held against the min stop by the inboard counterweight, valve
// shut); depressed is the throw that hauls the valve gear fully open.
inline constexpr float kTreadleRestAngle = 0.0F;
inline constexpr float kTreadleDepressedAngle = 0.24F;
// Sheave heights above each cable anchor. The run is a real two-sheave cable
// span across the yard, which is also what makes the linkage legible from the
// catwalk: you can see what the pedal is wired to.
inline constexpr float kTreadleSheaveRise = 2.0F;
inline constexpr float kValveCableArm = 0.25F;      // Short arm: 0.17 m of travel, ~750 N to move.
inline constexpr float kValveSheaveY = 9.6F;
// Sheave masts stand clear of everything that swings: the treadle mast is set
// off the walkway in z so the plate and the player never foul it, and the valve
// mast is set off in z so it misses the lever's counterweight.
inline constexpr float kTreadleMastZ = -105.0F;
inline constexpr float kValveMastZ = -92.0F;
inline constexpr float kCatwalkDeckY = 8.69F;

// --- WO-011 Ascent Atlas v1.0 kernel: KX-JIB / KX-CRATE --------------------
// Ascent Atlas section 9 places the WO-005..008 kernel at "z=0-24 m, plan cut
// 36 m x 36 m" as its own bounded proof volume, separate from band content --
// it is explicitly not the Kellerworks yard above. Sited well clear of it.
inline constexpr float kKernelBaseX = 200.0F;
inline constexpr float kKernelBaseZ = 0.0F;
inline constexpr float kKernelDeckHalfExtent = 10.0F; // 20 m square kernel apron.

inline constexpr float kJibMastX = kKernelBaseX;
inline constexpr float kJibMastZ = kKernelBaseZ;
inline constexpr float kJibMastHeight = 5.0F;
inline constexpr float kJibBoomLength = 6.0F;
inline constexpr float kJibBoomMassKg = 400.0F;
// Slew is bounded, not a full 360 -- a compact pendant swinging a load between
// a pickup point and a drop point, matching WO-005's "a compact pendant is
// enough" scope note. +/-2.0 rad (~115 deg) covers pickup-to-drop with margin.
inline constexpr float kJibSlewLimitRadians = 2.0F;
// The crate+hook at the boom tip (radius ~4.65 m) carries most of the slew
// moment of inertia: I ~= (1200+40)*4.65^2 + boom's own (1/3)*400*6^2 ~=
// 31,600 kg*m^2. 6000 N*m -- an earlier, unmeasured guess -- produced 0.006
// rad/s after 6 s of full command, not the target 0.5 rad/s. Measured via a
// probe and corrected; this value is a real, falsifiable design target now,
// not a guess (Atlas section 0.3).
inline constexpr float kJibSlewMaxTorqueNm = 45000.0F;
inline constexpr float kJibSlewMaxRateRadPerSec = 0.5F;

inline constexpr float kJibHookMassKg = 40.0F;
inline constexpr float kJibHoistMaxRateMetersPerSec = 1.0F;
// The rated winch force. A finite, enforced Jolt motor limit (Governing Law 26:
// a real constraint capacity, not a number that only appears in an HUD label).
// Atlas section 0.3: masses/loads below this line are design targets, not
// proof requirements, until a benchmark scene exists -- this WO is that scene.
inline constexpr float kJibMaxLiftForceN = 20000.0F;
// Powered hold compliance: the 1240 kg hook/load settles about 24 mm below
// its captured target. Damping is approximately critical for that load.
inline constexpr float kJibHoldStiffnessNPerMeter = 500000.0F;
inline constexpr float kJibHoldDampingNsPerMeter = 50000.0F;

inline constexpr float kCrateMassKg = 1200.0F; // weight ~11.8 kN, well inside rating.
inline constexpr float kCrateHalfExtent = 0.75F;

// A fixed, permanently-overweight capacity-proving stand: same rated winch
// force as the jib's hoist, a load past that rating, always commanded to
// raise. Proves "unlimited winch force" is forbidden without staging an
// unsafe lift on the real jib (WO-005 forbidden-shortcuts list).
inline constexpr float kCapacityStandX = kKernelBaseX + 8.0F;
inline constexpr float kCapacityStandZ = kKernelBaseZ + 6.0F;
inline constexpr float kCapacityStandMastHeight = 4.0F;
inline constexpr float kCapacityStandLoadMassKg = 2500.0F; // weight ~24.5 kN > rating.
inline constexpr float kCapacityStandLoadHalfExtent = 0.6F;

// Pendant station: a fixed point near the mast base. Commands only take
// effect within this radius (WO-005: "Action to enter station").
inline constexpr float kJibStationX = kJibMastX - 2.5F;
inline constexpr float kJibStationZ = kJibMastZ - 2.0F;
inline constexpr float kJibStationRadius = 2.5F;

// --- WO-012 Ascent Atlas v1.0 kernel: KX-NEEDLE / KX-POCKETS ---------------
// Atlas section 9 kernel chain: "player seats KX-NEEDLE with the jib." A
// second, minimal jib-pattern mechanism -- mast + one finite-force vertical
// motor, no slew -- carries the beam. WO-006's own text sanctions this
// reduction ("reduced-order connection... exact beam formulation remains
// TDD-gated... do not invent FEM to finish this WO"): siting the mast
// directly above the pocket centreline removes any need for slew, since the
// beam only ever travels straight down into the seat. Sited well clear of
// both the WO-011 jib's ~6.15 m swept reach and the capacity stand, inside
// the same 36x36 m kernel envelope (Atlas section 9).
inline constexpr float kNeedleGapCenterX = kKernelBaseX;
inline constexpr float kNeedleGapCenterZ = kKernelBaseZ - 16.0F;
inline constexpr float kNeedleGapWidthMeters = 3.2F; // clear span the beam must bridge.
inline constexpr float kNeedlePierHalfExtentX = 2.5F;
inline constexpr float kNeedlePierHalfExtentZ = 1.6F;
inline constexpr float kNeedlePierHeight = 4.0F; // "one bay of frame" (Atlas section 9).
inline constexpr float kNeedlePierTopY = kNeedlePierHeight;

inline constexpr float kNeedlePierApproachX =
    kNeedleGapCenterX - (kNeedleGapWidthMeters * 0.5F + kNeedlePierHalfExtentX);
inline constexpr float kNeedlePierFarX =
    kNeedleGapCenterX + (kNeedleGapWidthMeters * 0.5F + kNeedlePierHalfExtentX);

// How far each end must rest onto its pier once seated.
inline constexpr float kNeedleSeatOverlapMeters = 0.9F;
inline constexpr float kNeedleBeamHalfLength = kNeedleGapWidthMeters * 0.5F + kNeedleSeatOverlapMeters;
inline constexpr float kNeedleBeamHalfWidth = 0.5F;
inline constexpr float kNeedleBeamHalfHeight = 0.18F;
inline constexpr float kNeedleBeamMassKg = 900.0F; // GDD 17: real machinery, not player strength.

// Rest (seated) height of the beam's centreline -- also the hard bottom of
// the hoist's travel, so "reaches the bottom" and "reaches the seat" are the
// same event, not two independently-tuned numbers that could drift apart.
// The beam's *top* is set flush with the pier tops (seated into a pocket,
// not resting proud on top of one): this locomotion is a raw dynamic
// capsule with no step-up assist at all (confirmed by direct observation --
// a proud-mounted beam, top 0.36 m above the pier, flatly blocked forward
// walking rather than being climbed), so any step here is a wall, and a real
// seated span has to be a level continuation of the pier top, not a curb.
inline constexpr float kNeedleSeatedY = kNeedlePierTopY - kNeedleBeamHalfHeight;
// Top of the notch block cut into each pier's gap-facing edge (see
// build_kernel_needle): low enough that the seated beam's underside clears
// it and its own top still lands flush with the main pier top.
inline constexpr float kNeedlePocketNotchTopY = kNeedlePierTopY - 2.0F * kNeedleBeamHalfHeight;
// Clearance between the notch's boundary and the beam's own resting edge,
// well past JPH::BoxShape's default convex radius plus Jolt's default
// speculative contact distance, so the main block's rounded corner can never
// intercept the descending beam (see build_kernel_needle).
inline constexpr float kNeedlePocketMarginMeters = 0.2F;

// Pocket world points: the beam's two end centrelines when correctly seated.
// The beam has no horizontal or slew freedom at all (see above), so these
// are the only points its ends can ever occupy -- alignment is guaranteed by
// construction, not by a tolerance check.
inline constexpr float kNeedlePocketApproachX = kNeedleGapCenterX - kNeedleBeamHalfLength;
inline constexpr float kNeedlePocketFarX = kNeedleGapCenterX + kNeedleBeamHalfLength;

inline constexpr float kNeedleHoistMastHeight = 7.0F; // clears the beam's stowed pose above the piers.
// Stowed (top of travel): near the mast head, clear of the piers entirely.
inline constexpr float kNeedleStowedY = kNeedleHoistMastHeight - 0.8F;
inline constexpr float kNeedleHoistMaxRateMetersPerSec = 0.6F;
inline constexpr float kNeedleMaxLiftForceN = 16000.0F; // weight ~8.8 kN, well inside rating.

// A rest-pose seat predicate (WO-006: "a model that can say seated => support
// predicate true... optional sag only if the chosen reduced model already
// exists" -- this one has none). Speed, not position, is the operative test:
// position is already guaranteed by construction, so all that remains is
// "has it actually come to rest at the bottom," not "is it approximately
// somewhere near it."
inline constexpr float kNeedleSeatPositionToleranceMeters = 0.12F;
inline constexpr float kNeedleSeatSpeedToleranceMetersPerSec = 0.35F;
// A sustained raise command while seated is the legal unseat path: it pulls
// the pockets pins first (the beam cannot otherwise move at all while
// rigidly pinned), then the same motor that lowered it lifts it clear -- a
// real mechanism reversal, not a teleport or a flag flip.
inline constexpr float kNeedleUnseatCommandThreshold = 0.5F;

// Sited on the approach pier itself, not at grade: the pier top is the only
// place a player standing at grade cannot climb back up to unaided (no
// stair/ramp exists in this kernel slice), so the pendant has to be where
// the operator can actually reach it and then step onto the seated beam.
// At the pier's own centre, clear of the beam's horizontal footprint (which
// starts at kNeedlePocketApproachX = 197.5) by well over kPlayerRadius --
// close enough to that edge and the descending beam clips the standing
// player and wedges them against its face (found by direct observation:
// an earlier siting 0.1 m from that edge froze forward movement dead).
inline constexpr float kNeedleStationX = kNeedlePierApproachX;
inline constexpr float kNeedleStationZ = kNeedleGapCenterZ;
inline constexpr float kNeedleStationRadius = 2.5F;

// --- The stack: the tower's climbable lower section -------------------------
// The machine IS the building. This is a real open steel frame the player
// walks inside and outside of -- perimeter deck rings around a central shaft,
// so you can always see up and down through the structure -- not the solid
// slab that stood here before, which had no interior at all and could only
// ever be looked at from the yard.
inline constexpr float kStackCenterX = 0.0F;
inline constexpr float kStackCenterZ = -150.0F;
inline constexpr float kStackHalfExtent = 26.0F;      // 52 m square footprint: a building, not a mast.
inline constexpr float kStackLevelHeight = 11.0F;     // generous industrial floor-to-floor.
inline constexpr int kStackLevelCount = 32;           // real launch receivers at 11..352 m.
inline constexpr int kRegressionStackLevelCount = 14;
inline constexpr float kStackDeckHalfThickness = 0.25F;
inline constexpr float kStackDeckBandDepth = 9.0F;    // walkable perimeter band; leaves a 34 m shaft.
inline constexpr float kStackColumnHalf = 0.8F;

// --- AS-001: Ascent Atlas §6 band B00, "Apron and Intake" (0 -> 24 m) -------
// The first real campaign slice. Everything here is MOD-* content in the tower
// yard, sited against the stack's south face (z = kStackCenterZ +
// kStackHalfExtent = -124) rather than at a bare origin.
//
// The causal chain this geometry exists to make true:
//
//   ACT  [work CAP-PENDANT: raise MOD-YARD-JIB's winch]
//   STATE[finite winch force and slew torque; pack pose read back from Jolt]
//   WORLD[the 4 t pack leaves MOD-DOG-A's swing envelope, so the dog's
//         permanent opening torque is no longer resisted and the dog travels]
//   PLAY [the MOD-STAIR-A throat is physically open; walk to +24 m]
//
// There is deliberately no "dog" command and no pinned/unpinned flag in the
// simulation: the dog motor is commanded open on every tick from build time,
// and is held shut only because 4000 kg of freight is physically in its way.
//
// The southern column row of build_stack() reaches to z = -123.2, so the bay's
// back wall stands at -122.5 to clear it.
inline constexpr float kIntakeBayCenterX = 0.0F;
inline constexpr float kIntakeBayFrontZ = -110.5F;   // wall carrying the dog throat
inline constexpr float kIntakeBayBackZ = -122.5F;    // 12 m deep bay
inline constexpr float kIntakeBayHalfX = 10.0F;
// 5 m of wall against a 1.85 m maximum mantle rise: the throat is the only way
// in at grade, and that is a geometric fact, not an invisible wall.
inline constexpr float kIntakeBayWallHeight = 5.0F;
inline constexpr float kIntakeBayWallHalfZ = 0.30F;
inline constexpr float kIntakeThroatHalfWidth = 1.30F;   // 2.60 m opening
inline constexpr float kIntakeThroatHeight = 2.50F;      // capsule needs 1.80 m

// MOD-DOG-A: a hinged landing dog filling the throat, swinging into the bay.
// Every boundary is held off its neighbour by kIntakeDogClearance rather than
// being exactly coincident: JPH::BoxShape carries a rounded convex radius, so
// a plate built exactly flush with the jambs, the lintel and the ground jams
// against all three and never travels. 0.06 m clears that and is still an
// order of magnitude under the 0.70 m capsule, so the throat stays shut.
inline constexpr float kIntakeDogClearance = 0.06F;
inline constexpr float kIntakeDogHalfThickness = 0.20F;
// The hinge sits at the plate's own end face, so the trailing corner sweeps a
// circle of the plate's half-thickness as it opens. Setting the plate back by
// that half-thickness plus the clearance is what keeps that corner out of the
// west jamb -- without it the dog binds at ~0.31 rad and never travels.
inline constexpr float kIntakeDogHalfWidth =
    kIntakeThroatHalfWidth - kIntakeDogHalfThickness - kIntakeDogClearance;
inline constexpr float kIntakeDogHalfHeight =
    (kIntakeThroatHeight - 2.0F * kIntakeDogClearance) * 0.5F;
inline constexpr float kIntakeDogCenterY = kIntakeDogClearance + kIntakeDogHalfHeight;
inline constexpr float kIntakeDogMassKg = 900.0F;
inline constexpr float kIntakeDogHingeX = kIntakeBayCenterX - kIntakeDogHalfWidth;
inline constexpr float kIntakeDogRetractAngle = 1.45F;         // Atlas B00 figure
inline constexpr float kIntakeDogRetractSpeed = 0.62F;         // rad/s, Atlas B00
// Rated so the dog swings its own 900 kg plate but cannot shove freight:
// breaking 4000 kg loose at mu = 0.6 needs 0.6 * 4000 * 9.81 = 23544 N, which
// at the pack's ~2.0 m contact radius is 47088 N*m. 12000 N*m is a quarter of
// that, and roughly five times what accelerating the plate alone demands
// ((1/3) * 900 * 2.6^2 * 1.24 = 2515 N*m).
inline constexpr float kIntakeDogMaxTorqueNm = 12000.0F;
// Derived-predicate threshold only (HUD and falsifiers): the dog is "travelled"
// once it has swung clear enough that the throat passes a 0.70 m capsule.
inline constexpr float kIntakeThroatClearAngle = 1.20F;

// MOD-YARD-JIB: Atlas B00 gives 12 m boom, 11.50 m boom height, 5 t SWL.
inline constexpr float kIntakeGravity = 9.81F;
inline constexpr float kIntakeJibSwlKg = 5000.0F;
inline constexpr float kIntakeBoomLength = 12.0F;
inline constexpr float kIntakeBoomHeight = 11.50F;
inline constexpr float kIntakeJibMastX = kIntakeBayCenterX;
// Sited so the boom's full working radius lands the pack just inside the
// throat: close enough to be inside the dog's 2.48 m swing arc, and clear of
// MOD-STAIR-A's southern edge at z = -114.7.
inline constexpr float kIntakeJibMastZ = -100.2F;
inline constexpr float kIntakeJibBoomMassKg = 2400.0F;
inline constexpr float kIntakeJibHookMassKg = 120.0F;
// tau = r * F = 12 * 5000 * 9.81 = 588600 N*m; winch F = 5000 * 9.81 = 49050 N.
inline constexpr float kIntakeJibSlewTorqueNm =
    kIntakeBoomLength * kIntakeJibSwlKg * kIntakeGravity;
inline constexpr float kIntakeJibWinchForceN = kIntakeJibSwlKg * kIntakeGravity;
inline constexpr float kIntakeJibSlewLimitRadians = 0.90F;     // Atlas B00
inline constexpr float kIntakeJibSlewRateRadPerSec = 0.22F;    // Atlas B00
inline constexpr float kIntakeJibHoistRateMetersPerSec = 0.85F; // Atlas B00

// The 4 t pack, and the 9 t pack that proves the rating is real.
inline constexpr float kIntakePackHalfX = 1.10F;
inline constexpr float kIntakePackHalfY = 0.90F;
inline constexpr float kIntakePackHalfZ = 1.15F;
inline constexpr float kIntakePackMassKg = 4000.0F;   // 39240 N, inside 49050 N
inline constexpr float kIntakeOverweightPackMassKg = 9000.0F; // 88290 N, outside it
// Sited exactly at the boom's full working radius, straight down the -Z line
// from the mast, and therefore squarely inside the dog's swing envelope.
inline constexpr float kIntakePackZ = kIntakeJibMastZ - kIntakeBoomLength;
inline constexpr float kIntakeOverweightStandX = 7.0F;
inline constexpr float kIntakeOverweightStandZ = -98.0F;
inline constexpr float kIntakeOverweightMastHeight = 9.0F;

// MOD-INTAKE-BELT: 18 m stroke translating slat deck. Riding it is legal
// (WO-002 support-point velocity law), so it is a moving support, not scenery.
inline constexpr float kIntakeBeltX = 6.0F;
inline constexpr float kIntakeBeltTopY = 1.38F;
inline constexpr float kIntakeBeltHalfY = 0.18F;
inline constexpr float kIntakeBeltCenterZ = -96.0F;
inline constexpr float kIntakeBeltHalfX = 2.0F;
inline constexpr float kIntakeBeltHalfZ = 6.0F;
inline constexpr double kIntakeBeltStrokeMeters = 18.0;
inline constexpr double kIntakeBeltAngularFrequency = 0.40; // Atlas B00 omega

// CAP-PENDANT: the jib control, on the belt catwalk where Atlas B00 puts it.
inline constexpr float kIntakePendantX = 3.0F;
inline constexpr float kIntakePendantZ = -100.0F;
inline constexpr float kIntakePendantTopY = kIntakeBeltTopY;
inline constexpr float kIntakeStationRadius = 3.40F;  // Atlas B00

// MOD-STAIR-A: six switchback flights of 4 m inside the bay, grade to +24 m.
// Opposing flights run in two separate Z lanes joined by a landing at each
// turn. Sharing one lane makes consecutive flights meet in a V whose apex
// pinches below the 1.80 m standing capsule -- the player climbs to the pinch
// and stops, which is a stair that cannot be walked.
// Set far enough north that the south lane clears the pack's standing pose at
// z = -113.35: a flight slab through the pack wedges the whole rig and the
// winch creeps at 0.02 m/s instead of its rated 0.85.
inline constexpr float kIntakeStairZ = -118.0F;
inline constexpr float kIntakeStairLaneOffset = 2.0F;
inline constexpr int kIntakeStairFlightCount = 6;
inline constexpr float kIntakeStairFlightRise = 4.0F;
inline constexpr float kIntakeStairHalfRun = 7.0F;     // 14 m run, 15.9 degree pitch
inline constexpr float kIntakeStairHalfWidth = 1.8F;
inline constexpr float kIntakeStairSlabHalfY = 0.18F;
inline constexpr float kIntakeStairLandingX = 8.0F;
inline constexpr float kIntakeStairLandingHalfX = 1.0F;
inline constexpr float kIntakeStairLandingHalfZ =
    kIntakeStairLaneOffset + kIntakeStairHalfWidth;
inline constexpr float kIntakeHandoffY =
    static_cast<float>(kIntakeStairFlightCount) * kIntakeStairFlightRise; // 24 m
inline constexpr float kIntakeHandoffCenterX = -6.0F;
inline constexpr float kIntakeHandoffHalfX = 4.0F;
inline constexpr float kIntakeHandoffHalfY = 0.20F;
inline constexpr float kIntakeHandoffSouthZ = -107.7F;  // meets the SKIN ladder head
// Stops short of the odd-flight lane: run the deck over it and the top of
// flight 5 is buried inside the deck instead of arriving on it.
inline constexpr float kIntakeHandoffNorthZ = -117.5F;

// MOD-SKIN-LADDER-S: the always-legal bypass. A stepped ledge line climbing
// north up the apron toward the tower, because this game climbs by mantling
// real bodies -- each rung is one box whose front face is the wall probe's
// target and whose top face is the landing, which is what probe_ledge()
// requires (wall hit and top hit must be the same body).
//
// Rung geometry is constrained, not chosen. With rise R, half-height H and
// half-depth D:
//   R - 2H <= 0.90   so the next rung is struck by the chest-height wall ray
//   R <= kMantleMaximumRise
//   2D - kLandingInset > kTraversalReach + kPlayerRadius
// The third is what makes this climbable at all. A mantle drops the player
// kLandingInset in from the rung's near edge; if the next rung's face is still
// within probe reach from there, the player auto-grabs a hang the instant they
// land, never becomes grounded, and the climb degenerates into a mantle-hang-
// fall cycle that makes no height. A 2.00 m rung leaves 1.53 m of stand, which
// is outside the 1.30 m reach, so every rung is a real footing the player
// walks across before taking the next one.
//
// The rungs step straight north rather than staggering left and right for the
// same reason: staggered rungs put the next target exactly where the mantle
// lands you.
inline constexpr float kIntakeSkinCenterX = kIntakeHandoffCenterX;
inline constexpr float kIntakeSkinRungRise = 1.60F;
inline constexpr float kIntakeSkinRungHalfX = 1.0F;
inline constexpr float kIntakeSkinRungHalfY = 0.50F;
inline constexpr float kIntakeSkinRungHalfZ = 1.00F;
inline constexpr int kIntakeSkinRungCount = 15;       // tops at 1.6 .. 24.0 m
// The head rung sits flush against the handoff deck's south edge, so both
// braids -- MOD-STAIR-A and SKIN -- arrive on the same deck.
inline constexpr float kIntakeSkinHeadZ = kIntakeHandoffSouthZ + kIntakeSkinRungHalfZ;
inline constexpr float kIntakeSkinFootZ =
    kIntakeSkinHeadZ + 2.0F * kIntakeSkinRungHalfZ *
                           static_cast<float>(kIntakeSkinRungCount - 1);

// --- AS-002 Legal Forty: MOD-STAIR-A-SWING, MOD-CW-CRADLE, MOD-HALL-DECK ---
// Atlas band B00's 24-40 m leftover, chain K0 PLAY. A counterweighted
// bascule: gravity alone holds the flight at its stowed hinge limit; only
// real rope tension read back from the solver can beat that and swing it to
// its deployed limit. With the sheave directly above the hinge, the sin(theta)
// term cancels out of the torque balance (T_crit falls as theta rises), so
// there is no stable intermediate pose -- both stops are hinge limits, not
// soft targets.
inline constexpr float kLegalFortyFlightLength = 16.0F;             // L: 8.000 m rise / sin(30 deg)
inline constexpr float kLegalFortyFlightHalfLength = kLegalFortyFlightLength * 0.5F; // r from the hinge
inline constexpr float kLegalFortyFlightHalfWidth = 0.90F;          // 1.80 m tread, one lane
// 400 kg, not the plan's own 1900 kg (66 kg/m^2 "DESIGN TARGET" grating
// density). T_crit(theta) = m_f*g*(L/2)*l(theta)/(r_a*h_s) is sound --
// re-derived independently here, and the plan's own worked numbers (loaded
// cradle 1.21x margin, empty cradle 0.37x) check out at 1900 kg -- but a
// bare majority does not survive contact with the actual solver: built at
// 1900 kg, the flight sat inert at its stowed limit under the real 4000 kg
// pack for 80+ s of simulated time (verified). A hinge starting exactly at
// a hard limit needs real headroom to depart it, not just >1x. 400 kg
// (kIntakeCwCradleTareMassKg's own comment has the loaded-cradle side of
// this same margin) is the value actually run end to end -- deploy under
// the real pack, the full ascent walk to +40 m, checkpoint, and retract
// once unloaded -- not a clean margin ratio picked in advance, the value
// verified.
inline constexpr float kLegalFortyFlightMassKg = 400.0F;
inline constexpr float kLegalFortyFlightRiseMeters = 8.0F;          // per flight; two flights close 16.000 m

// Swing hinge: world-Z axis, sited at the mid-landing's height above
// MOD-STAIR-A's lower run. theta is measured from the downward vertical;
// built at the stowed pose (8 deg) so the hinge's as-built angle is 0 -- the
// same convention kIntakeDogEntityId already uses.
inline constexpr float kIntakeSwingHingeX = 7.856F;
inline constexpr float kIntakeSwingHingeZ = -112.5F;
inline constexpr float kLegalFortyStowedThetaRadians = 0.139626F;   // 8 deg
inline constexpr float kLegalFortyDeployedThetaRadians = 1.047198F; // 60 deg
// Jolt's HingeConstraint::GetCurrentAngle() (Jolt/Physics/Constraints/
// HingeConstraint.cpp) reports the physical rotation body2 has undergone
// since construction, signed by the right-hand rule about the world hinge
// axis (+Z). This flight is built with its local +X already pointing along
// (sin(theta_stowed), cos(theta_stowed), 0); as theta increases toward
// deployed, that direction rotates TOWARD world +X, which is a NEGATIVE
// rotation about +Z -- so the reported angle runs stowed=0 to deployed
// -0.907571 rad, and every reader below negates it back to a non-negative
// "how far open" travel.
inline constexpr float kIntakeSwingHingeTravelRadians =
    kLegalFortyDeployedThetaRadians - kLegalFortyStowedThetaRadians; // 52 deg, 0.907571 rad

// The rope bracket: r_a = 15.000 m from the hinge (Design Values), which on a
// flight whose hinge is at local +8.00 and whose foot is at local -8.00 sits
// at local x = 8.00 - 15.00 = -7.00 -- one metre inboard of the foot, on the
// slab's underside so it clears the tread.
inline constexpr float kIntakeSwingRopeLeverArmMeters = 15.0F;  // r_a
// h_s, sheave above the hinge. The plan's own 4.0 m -- unlike the flight
// and tare masses above and below, this one needed no retuning once
// kIntakeSwingAnchorEntityId's own fix (excluding the flight from contact
// with its own hinge anchor) was in place.
inline constexpr float kIntakeSwingSheaveHeightMeters = 4.0F;
inline constexpr float kIntakeSwingBracketLocalX =
    kLegalFortyFlightHalfLength - kIntakeSwingRopeLeverArmMeters; // -7.00
inline constexpr float kIntakeSwingBracketLocalY = -kIntakeStairSlabHalfY;

// MOD-CW-CRADLE: a three-sided open frame (not a closed hopper -- a 2.4 m
// hopper cannot take a 2.2 x 2.3 m pack), riding a free (unmotored) vertical
// slider on a fixed guide mast. Built at the empty-cradle rest pose (the rope
// taut, tension losing to the flight's own stowed-stop demand); loading the
// pack lets the rope pull the cradle down and the flight up.
// 500 kg, not the plan's own 1800 kg. The 4000 kg pack is AS-001's own,
// frozen, so the ratio between "loaded" (tare+4000) and "tare-only" demand
// on the flight's T_crit is fixed by tare mass alone -- see
// kLegalFortyFlightMassKg's own comment on how thin a static-torque margin
// the real solver tolerates.
//
// Retracting once looked like it needed a lighter tare still: at 500 kg
// the unloaded, re-slung flight sat frozen at the exact deployed limit for
// 20+ s, and dropping tare to 50 kg (an 80x ratio against the 4000 kg
// pack) did make it depart -- but traded that for a worse fault, the
// LOADED rest pose itself drifting closed and back over tens of degrees
// while the pack was still seated. Neither number was the real problem:
// direct diagnostic tracing of the stuck state (GetWorldSpaceContactPointOn1
// on every contact touching the flight) found a real, continuous contact
// between the flight and its OWN hinge anchor at the hinge point itself,
// on top of and separate from the hinge constraint that is supposed to be
// the only thing relating them -- the flight's cross-section is coincident
// with that anchor at every sweep angle by construction, so any nonzero-
// size anchor box (see kIntakeSwingAnchorEntityId's own comment at the
// anchor's build site) keeps a sliver of it. Excluding that one body pair
// from contact resolution, the same way OnContactValidate already excludes
// the flight from the handoff deck, fixed retracting outright at the
// original 500 kg -- no mass or geometry retuning needed once the real
// fault was gone.
inline constexpr float kIntakeCwCradleTareMassKg = 500.0F;
inline constexpr float kIntakeCwCradleHalfX = 1.50F;
// 0.90, not the plan's 1.20. The plan's deployed car (centre 2.380) put its
// underside at 1.18 m, under the B00 belt's 1.38 m top -- and the belt's
// 18 m stroke carries its south end to z = -105 - 6 = -111, across the
// car's x in [7.108, 8.0] and z in [-109.76, -107.36]. Once a stroke
// (15.7 s) the belt rammed the loaded car, jerked the flight 0.045 rad off
// its stop, and walked the seated pack 0.3-0.6 m per blow until it fell
// (observed: off the car within ~40 s, nobody near it). The top face --
// the seat and the rope's body point -- stays at the plan's 5.200 m, so
// the rope, stroke and statics are unchanged; only the underside rises,
// to 1.78 m deployed, 0.40 m over the belt.
inline constexpr float kIntakeCwCradleHalfY = 0.90F;
inline constexpr float kIntakeCwCradleHalfZ = 1.20F;
inline constexpr float kIntakeCwCradleTopBuildY = 5.20F;
inline constexpr float kIntakeCwCradleBuildCenterY = kIntakeCwCradleTopBuildY - kIntakeCwCradleHalfY;
inline constexpr float kIntakeCwCradleGuideHalfX = 0.35F;
inline constexpr float kIntakeCwCradleGuideHalfY = 4.0F;
inline constexpr float kIntakeCwCradleGuideHalfZ = 0.35F;
// Jolt's SliderConstraintSettings::mAutoDetectPoint anchors displacement 0 at
// the build pose (kNeedleStowedY's own comment: "travel is signed from the
// spawn pose"), so the plan's centre range y in [1.80, 4.60] -- top face in
// [3.00, 5.80] -- becomes signed limits relative to the build pose. Stated
// on the top face, they are unchanged by the car's height.
inline constexpr float kIntakeCwCradleLimitMinMeters = 3.00F - kIntakeCwCradleTopBuildY;
inline constexpr float kIntakeCwCradleLimitMaxMeters = 5.80F - kIntakeCwCradleTopBuildY;
// The hook hangs at the boom tip, so working radius is fixed at the 12 m
// boom; only the slew bearing chooses where the cradle sits on that circle.
inline constexpr float kIntakeCwCradleBearingRadians = 0.80F; // of the 0.90 slew limit

// The sling release/attach envelopes (AS-002 Design Values 8.4.3).
inline constexpr float kLegalFortyReleaseXZToleranceMeters = 0.55F;
inline constexpr float kLegalFortyReleaseYToleranceMeters = 0.35F;
inline constexpr float kLegalFortyReleaseSpeedToleranceMps = 0.15F;
inline constexpr float kLegalFortyAttachToleranceMeters = 0.40F;
inline constexpr float kLegalFortyAttachSpeedToleranceMps = 0.20F;

// MOD-STAIR-A upper flight (static) + mid-landing + MOD-HALL-DECK. Sited by
// the plan's own geometry table; each surface's height is derived at build
// time from AS-001's own tread-surface offset (kIntakeHandoffY plus slab
// half-thickness over cosine pitch) stacked with two more 8.000 m rises, so
// the whole run stays flush to the handoff deck below it by construction
// rather than by matching a second copy of the same literal.
inline constexpr float kLegalFortyMidLandingCenterX = 9.350F;
inline constexpr float kLegalFortyMidLandingHalfX = 1.05F;
inline constexpr float kLegalFortyMidLandingCenterZ = -115.80F;
// 5.20, not the plan's own 3.20: south edge -121.00, matching the walkway's
// own south edge (kLegalFortySkinWalkwayCenterZ +/- HalfZ) exactly rather
// than falling 3.0 m short of it at -118.00. The walkway and the landing
// touch at x = 8.30 with zero x-overlap, so the ONLY way across is through
// whatever z-band both cover at once, and the upper flight above (its own
// z in [-117.9, -116.1], underside descending toward its foot at this same
// x = 9.350) leaves a capsule under 2.1 m of headroom -- the same figure
// the well's own east edge comment above derives -- for x greater than
// about 5.7, ruling out the flight's own z-band as that crossing corridor
// entirely. -118.00 (south of the flight, clear the whole way to its foot)
// is the only band left, and at the plan's own 3.20 it was not covered by
// the landing at all: found by direct observation, a capsule walked there
// falling clean through the gap between the two footprints. Widening this
// one edge to meet the walkway's own is the fix, not moving the walkway
// (AS-002-owned on both sides, and the walkway's own z is independently
// anchored to rung 20 -- see its own comment). The walkway has since grown
// 2 m south (its own comment has why); this edge follows it.
inline constexpr float kLegalFortyMidLandingHalfZ = 5.20F;

inline constexpr float kLegalFortyUpperFlightCenterX = 2.422F;
inline constexpr float kLegalFortyUpperFlightCenterZ = -117.0F;

inline constexpr float kLegalFortyHallDeckHalfX = 10.0F;
inline constexpr float kLegalFortyHallDeckCenterZ = -115.1F;
inline constexpr float kLegalFortyHallDeckHalfZ = 7.40F;
inline constexpr float kLegalFortyHallDeckHalfThickness = 0.20F;

// The stair well MOD-HALL-DECK leaves open for MOD-STAIR-A to arrive
// through. Both edges moved from the plan's own literal (center -4.51,
// half 3.00, i.e. x in [-7.51, -1.51]) -- SKIN's own rung 20 never actually
// depended on that literal (it stops at the mid-landing's own height, see
// its own comment), so nothing else anchors it.
//
// East edge, -1.51 -> +1.50: a standing capsule needs roughly 2.1 m of
// headroom over an inclined surface, not the ~0.9 m a flat floor needs, so
// the upper flight's own climb -- rising 8.000 m over its 13.856 m
// horizontal run from the mid-landing -- does not clear the east strip's
// underside (39.787 m) until past where the original edge sat, and a
// capsule walking the incline stops there, wedged under the deck a full
// 2 m short of the opening it is trying to reach. Found by direct
// observation, not derived: the incline height a stuck capsule reports
// (37.726 m) versus the strip's underside (39.787 m) is short by exactly a
// second capsule-height's worth of headroom, not the first.
//
// West edge, -7.51 -> -4.80: the flight's own top -- local -X end of its
// rotated footprint, at x = -4.506 (see the flight's own comment) -- is
// where it reaches +40.1872 m, but the original west edge left 3.0 m of
// open well between that point and the nearest deck strip: real floor at
// the right height with nothing beside it to step onto, so a capsule
// walking off the flight's own edge falls through the well instead of
// reaching MOD-HALL-DECK itself. -4.80 overlaps the west strip with the
// flight's own last 0.3 m -- harmless, since Jolt does not solve contact
// response between two static bodies, and flush joints elsewhere in this
// file already rely on exactly that. Found the same way as the east edge:
// direct observation of where the fall began.
inline constexpr float kLegalFortyWellCenterX = -1.65F;
inline constexpr float kLegalFortyWellHalfX = 3.15F;
inline constexpr float kLegalFortyWellCenterZ = -117.0F;
inline constexpr float kLegalFortyWellHalfZ = 2.20F;

// MOD-SKIN-LADDER-S continuation, rungs 16-20: same column (x = -6.0) as
// AS-001's rungs 1-15, resuming north of rung 15's own footprint and of the
// handoff deck's south-edge transition (rung 15 spans z in [-107.7,-105.7];
// that whole band, plus headroom above it, must stay clear or it becomes a
// low ceiling over the exact spot AS-001's climb steps onto the deck --
// found by direct observation: siting rung 16 at kIntakeSkinHeadZ (-106.7,
// the natural next step) or even one metre north of it broke that proven
// transition).
//
// Only 5 rungs, not the ten a straight climb all the way to +40 m would
// need. That climb does not fit: MOD-HALL-DECK's own stair well is only
// 4.40 m deep, and the mantle law below is a floor on the GAP between
// rungs, not (as an earlier draft of this comment had it) on each rung's
// own depth alone -- derived properly, flush-adjacent rungs land a mantle
// kLandingInset short of the next rung's face, so step - kLandingInset >
// kTraversalReach + kPlayerRadius, i.e. step > 1.77 m regardless of rung
// depth. Nine rungs at that spacing need >= 14.2 m of clear run; the well
// gives 4.4 m and the run north of rung 16 is hard-capped at 12.5 m by
// build_stack()'s own southern columns (kIntakeBayBackZ's comment: they
// reach z = -123.2). No single straight column threads both needles.
//
// So SKIN only climbs to the +32.1872 m mid-landing height here -- clear
// of the well problem entirely, since none of these five rungs' tops come
// anywhere near MOD-HALL-DECK's own y-band -- and a short static walkway
// (below) carries the remaining, purely horizontal distance to the
// mid-landing itself, from which MOD-STAIR-A's own (now-inclined) upper
// flight is the rest of the route, exactly as the freight braid uses it.
// Reuses AS-001's rung law verbatim for rise and depth (a derived
// clearance, not a style choice -- see kIntakeSkinRungRise's comment):
// rise 1.60 <= kMantleMaximumRise, 2 * half-depth - kLandingInset >
// kTraversalReach + kPlayerRadius. Step is flush (2 * half-depth = 2.00 m),
// matching AS-001's own rungs 1-15 and clearing the 1.77 m floor above with
// margin.
inline constexpr int kLegalFortySkinRungFirst = 16;
inline constexpr int kLegalFortySkinRungLast = 20;    // tops at 32.1872 m, the mid-landing's height
inline constexpr float kLegalFortySkinRungFirstZ = -110.0F;
inline constexpr float kLegalFortySkinRungStepZMeters = 2.0F;

// The walkway from rung 20's top to the mid-landing: flush with both (top
// at 32.1872 m), touching rung 20's own east face at x = -5.8 and the
// mid-landing's own west face at x = 8.30 so neither joint is a step. It
// covers rung 20's own z band (centre kLegalFortySkinRungFirstZ - 4 * step
// = -118.0) and 2 m south of it, exactly the mid-landing's own south part,
// and its north edge (-117.0) stays clear (>= 3.6 m) of the swing flight's
// own z in [-113.4, -111.6] on every path, deployed or not.
// Rungs 17-20 step 0.8 m west of the column. Directly above the bascule
// flight's foot (x from -6.0), rung 17 on the column left 1.4 m of headroom
// over the flight's first metre: the SHAFT braid walked into its underside.
// West by 0.8 m, its east edge (-5.8) clears a capsule on the slope, and a
// climber on the column line (-6.0) still strikes every face 0.2 m inside it.
inline constexpr float kLegalFortySkinRungJogX = -0.8F;
inline constexpr float kLegalFortySkinWalkwayMinX = -5.8F;  // rung 20's east face
inline constexpr float kLegalFortySkinWalkwayMaxX = 8.30F;
// z in [-121, -117]: rung 20's own band plus 2 m south of it. Rung 20's band
// alone ([-119, -117]) runs under the upper flight's z in [-117.9, -116.1],
// whose underside falls toward its foot to 0.6 m over the walkway at x = 8:
// a standing capsule (radius 0.35) had one 0.4 m-wide lane, z in
// [-118.65, -118.25], and anyone walking the walkway's middle met the slab
// (reported by a player as an opening too small to fit through). South of
// the flight the air is clear to the hall deck's underside (39.79 m, 7.6 m
// up), so the added 2 m gives a 2.4 m lane with full headroom.
inline constexpr float kLegalFortySkinWalkwayCenterZ = -119.0F;
inline constexpr float kLegalFortySkinWalkwayHalfZ = 2.00F;

// --- AS-003 MOD-HOOK5-RACK: CAP-HOOK5 in a locked cage beside the belt -------
// Atlas B00: "Hook block + slings in a locked cage opened by moving the crate
// or circling the belt." There is no lock object: the cage is locked by
// HEIGHT. Its top is out of reach from grade by every move and in reach from
// MOD-INTAKE-BELT's deck.
//
// 4.45 m, not the plan's 2.90. The plan measured the lock against the mantle
// ceiling (1.85 m) alone; a jump into a ledge grab reaches much higher.
// Measured here against walls of every height, jumped at from a run: a grab
// reaches 3.75 m above the floor it leaves -- 3.75 m from grade, 5.13 m from
// the belt's 1.38 m deck. 2.90 m was grabbable from grade. 4.45 m is 0.70 m
// out of reach from grade and 0.68 m inside reach from the deck, so the belt
// is the key: jumped from and grabbed, not mantled.
inline constexpr float kHook5CageTopY = 4.45F;
inline constexpr float kHook5RoofHalfY = 0.15F;
inline constexpr float kHook5WallTopY = kHook5CageTopY - 2.0F * kHook5RoofHalfY;  // 4.15, under the roof
//
// Sited at z in [-88, -84], 18 m north of the plan's [-106, -102]. A height
// lock only holds if nothing but the key comes within a jump, and a running
// jump carries far: it still grabs a 4.45 m ledge across 6.3 m of air from a
// 1.38 m floor and 6.8 m from 1.8 m, and drops onto a 4.45 m roof across
// 10.8 m from 8 m up (all measured). At the plan's site the pendant catwalk,
// the 9 t pack's top, the intake bay's wall top (off MOD-STAIR-A's first
// landing) and the WO-006 lift all reached the roof, so the belt was not a
// key but one of five. Here, 8.1 m beyond the 9 t pack's farthest jostle, a
// brute-force audit of 1 006 running jumps -- from grade round every face,
// the 9 t pack, the pendant catwalk, its ramp, the WO-006 catwalk and the
// WO-006 lift at its 9 m top -- reached a cage body only from the lift (3 of
// its 162), by an 11 m leap: a machine-made route, and legal (Governing Law
// 17). The deck still lies alongside for 43 % of its stroke, lingering near
// its north end, where the jump is made.
inline constexpr float kHook5MinX = 8.0F;      // the belt deck's east edge
inline constexpr float kHook5MaxX = 12.0F;
inline constexpr float kHook5MinZ = -88.0F;
inline constexpr float kHook5MaxZ = kHook5MinZ + 4.0F;
inline constexpr float kHook5ButtressMaxX = 9.4F;  // west buttress x in [8.0, 9.4], full height
inline constexpr float kHook5WallThickness = 0.30F;
// The hatch cut in the roof, over open floor west of the rack and north of
// the door's sweep, so the drop lands clear of both. 1.2 x 1.6 m.
inline constexpr float kHook5HatchMinX = 9.7F;
inline constexpr float kHook5HatchMaxX = 10.9F;
inline constexpr float kHook5HatchMinZ = kHook5MinZ + 1.4F;
inline constexpr float kHook5HatchMaxZ = kHook5MinZ + 3.0F;
// The south wall's doorway, 1.40 m under a 2.20 m header.
inline constexpr float kHook5DoorwayMinX = 9.85F;
inline constexpr float kHook5DoorwayMaxX = 11.25F;
inline constexpr float kHook5DoorwayTopY = 2.20F;
// MOD-HOOK5-DOOR: hinged 0.20 m back from the west jamb (MOD-DOG-A bound at
// 0.31 rad because its hinge sat flush with its jamb), swinging INWARD under a
// permanent opening drive commanded at build time and never again. Shut, the
// leaf leaves 0.20 m at the hinge jamb and 0.06 m at the latch jamb.
inline constexpr float kHook5DoorHingeX = 10.05F;
inline constexpr float kHook5DoorZ = kHook5MinZ + 0.15F;          // the wall's mid-plane
inline constexpr float kHook5DoorHalfLength = 0.57F;             // shut: x in [10.05, 11.19]
inline constexpr float kHook5DoorHalfHeight = 1.06F;             // y in [0.06, 2.18]
inline constexpr float kHook5DoorHalfThickness = 0.12F;
inline constexpr float kHook5DoorCenterY = 1.12F;
inline constexpr float kHook5DoorMassKg = 80.0F;
inline constexpr float kHook5DoorOpenAngle = 1.45F;              // inward stop
inline constexpr float kHook5DoorDriveSpeed = 0.55F;             // rad/s
// 600 N*m, not the plan's 6 000: the rigid bar holds against any torque, and
// a 6 000 N*m leaf pins the bar in its brackets with ~7 kN, so lifting it
// would fight that much friction. 600 N*m swings the 80 kg leaf (34.7 kg*m^2
// about the hinge) at 17 rad/s^2 and pins the bar with ~0.5 kN.
inline constexpr float kHook5DoorDriveTorqueNm = 600.0F;
// MOD-HOOK5-BAR: 38 kg across the inside of the doorway, 0.04 m north of the
// shut leaf, so the drive stalls within 0.035 rad. Its ends sit in two
// brackets -- a ledge under each end and a stop on its north face -- that are
// OUTSIDE the leaf's sweep (radius 1.146 m about the hinge): the west one
// behind the hinge, the east one 1.31 m from it. (The plan put the keepers
// inside the sweep, where they would have stopped the door with the bar
// gone.) Lift the bar out and nothing is left in the swing.
inline constexpr float kHook5BarMassKg = 38.0F;
inline constexpr float kHook5BarHalfLength = 1.025F;             // x in [9.50, 11.55]
inline constexpr float kHook5BarHalfSection = 0.09F;
inline constexpr float kHook5BarCenterX = 10.525F;
inline constexpr float kHook5BarCenterY = 1.00F;    // handle 1.09, under the carry hands
inline constexpr float kHook5BarCenterZ = kHook5MinZ + 0.40F;
inline constexpr float kHook5BracketHalfX = 0.10F;
inline constexpr float kHook5BracketWestX = 9.60F;
inline constexpr float kHook5BracketEastX = 11.45F;
// CAP-HOOK5: the 36 kg hook block on a pedestal rack in the north-east
// corner. There, not mid-wall: the 2.05 m bar has to be set down somewhere in
// a 2.3 m-wide room clear of the door's swing, and with the rack mid-wall
// every such place put one end of the bar on it (observed).
inline constexpr float kHook5BlockMassKg = 36.0F;
inline constexpr float kHook5BlockHalf = 0.30F;
inline constexpr float kHook5BlockHalfY = 0.25F;
inline constexpr float kHook5RackTopY = 0.60F;       // block top 1.10, under the carry hands
inline constexpr float kHook5BlockSeatX = 11.35F;
inline constexpr float kHook5BlockSeatZ = kHook5MaxZ - kHook5WallThickness - 0.45F;
inline constexpr float kHook5BlockSeatY = kHook5RackTopY + kHook5BlockHalfY;
inline constexpr float kHook5InRackTolerance = 0.35F;            // hook_in_rack: within this of the seat

// The carry (AS-003 §8.4.3). A carryable is picked up within kCarryReach of
// the body's centre, at rest, in front of it; it hangs by the middle of its
// top face from a point at the hands that follows the facing, so it swings.
// The hands are at belly height, 1.25 m over the soles, and must be above the
// handle of anything picked up -- off a rack, a bracket or the floor -- so the
// pick-up lifts it and the ground carries the player. (Below a seated bar's
// handle, the constraint pulled the bar down into its brackets, could not,
// and hoisted the player off the floor instead -- observed.) So the bar and
// the rack are seated low, not the hands raised: at chest height (1.55 m) the
// block's top was at the eye and the carried load filled the view.
inline constexpr float kCarryReach = 1.20F;
inline constexpr float kCarryMaxBodySpeed = 0.50F;
inline constexpr float kCarryHandForward = 0.60F;
inline constexpr float kCarryHandUp = 0.35F;
// The hands swing round to the facing at this rate, not with it: a 36 kg load
// turns with the body, and a glance round would otherwise throw the hand
// point 1.2 m in one tick and the load out of it (observed with an instant
// half-turn). A half-turn with a load takes about a second.
inline constexpr float kCarryTurnRadiansPerSecond = 3.0F;
// A held body wedged hard enough that the hands are dragged this far from its
// handle, and still moving apart, slips out of them. Nothing holds a wedged
// steel member, and without this a bar jammed across a doorway would anchor
// the player to it. "Still moving apart" is what spares a pick-up: a block
// taken off the floor at the edge of reach starts over 1 m below the hands
// and closes on them, so it is held.
inline constexpr float kCarrySlipDistance = 0.90F;
// AS-006: a hand holds a mechanism-kit body -- a shackle, a trip-line handle,
// a load -- with at most this force. Pulled harder for kGripSteps steps
// running, the body is torn out of the hands. The AS-003 bar and block keep
// the slip rule above alone.
inline constexpr float kGripNewtons = 900.0F;
inline constexpr std::uint32_t kGripSteps = 2;

// --- WO-013 Ascent Atlas v1.0 kernel: KX-SUMP / KX-GRATE -------------------
// Atlas section 9: "wet sump makes KX-GRATE a hazard... isolated + drained
// grate is ordinary walkable support." A lumped process graph (WO-007's own
// "Allowed seam": one volume, one isolation edge, one drain sink, one derived
// safe predicate) -- no particle fluid, no second process engine.
//
// The walkway is elevated, like the needle's piers, rather than a hole cut
// into the existing world deck: that deck is one solid box spanning nearly
// the whole map, so a below-grade pit would need the deck itself carved
// open, which nothing in this codebase does. A raised grate with real open
// air beneath it, landing back on that same deck, reuses proven geometry.
inline constexpr float kSumpCenterX = kKernelBaseX;         // 200
inline constexpr float kSumpCenterZ = kKernelBaseZ + 16.0F; // clear of the jib (Z~0) and the needle (Z~-16).
inline constexpr float kSumpPlatformTopY = 3.0F; // a real, clearly-survivable fall through (~7.7 m/s impact).
inline constexpr float kSumpDeckHalfThickness = 0.15F;
inline constexpr float kSumpDeckHalfZ = 1.5F;
inline constexpr float kSumpApproachDeckHalfX = 1.5F;
inline constexpr float kSumpGrateHalfX = 1.5F;
inline constexpr float kSumpFarDeckHalfX = 1.5F;

inline constexpr float kSumpGrateX = kSumpCenterX;
inline constexpr float kSumpApproachDeckX = kSumpGrateX - kSumpGrateHalfX - kSumpApproachDeckHalfX;
inline constexpr float kSumpFarDeckX = kSumpGrateX + kSumpGrateHalfX + kSumpFarDeckHalfX;

// Lumped process state. Starts full (wet, unsafe) -- WO-006's needle and
// WO-005's jib both start in their "nothing done yet" pose; the sump matches
// that convention with "nothing isolated yet, still wet."
inline constexpr float kSumpCapacityKg = 1000.0F;
inline constexpr float kSumpInflowKgPerSec = 150.0F; // while the valve is open, inflow keeps it topped up.
inline constexpr float kSumpDrainKgPerSec = 100.0F;  // always draining; only wins once isolated.

inline constexpr float kSumpStationX = kSumpApproachDeckX;
inline constexpr float kSumpStationZ = kSumpCenterZ;
inline constexpr float kSumpStationRadius = 2.5F;

// WO-008 fall / parachute / checkpoint. An 8.8 m unassisted lift-platform
// fall (~13.1 m/s impact) must stay survivable per GDD 8.2; a genuine
// tower-scale drop must not be. Terminal parachute speed (~9 m/s, derived
// below) sits comfortably under that threshold -- Simulation::
// kLethalImpactSpeedMps, public in simulation.hpp -- with margin either side.

// Quadratic drag a = -k*v*|v|. Solved for a target terminal speed v_t at
// k = g / v_t^2 (net vertical accel is zero at v_t: g - k*v_t^2 = 0).
inline constexpr float kParachuteDragCoefficient = 0.1211F; // v_t ~= 9 m/s at g=9.81

inline constexpr float kWorldSolidMirrorTolerance = 0.02F;
inline constexpr float kWorldSolidFriction = 0.8F;


} // namespace scraperx::sim::detail
