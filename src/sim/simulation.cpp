#include "sim/simulation.hpp"

#include "sim/bands.hpp"
#include "sim/mechanism_kit.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "WO-003 requires the pinned Jolt physics substrate"
#endif

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

namespace {

namespace object_layers {
constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;
constexpr JPH::ObjectLayer kCount = 2;
} // namespace object_layers

namespace broadphase_layers {
constexpr JPH::BroadPhaseLayer kStatic(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr JPH::uint kCount = 2;
} // namespace broadphase_layers

constexpr double kPi = 3.14159265358979323846;

constexpr float kSupportNormalThreshold = 0.55F;
// Footing firm enough to commit a checkpoint on: the body's centre is over
// what it stands on, so a ray straight down from it meets a walkable surface
// within the capsule's half-height plus this slack. Grounded alone (above)
// includes a capsule held up by its rim with its centre past an edge -- a
// stance it cannot keep. Committed there, a lethal fall off that edge
// restored the body into the same slide, every time (observed carrying the
// hook block off MOD-HALL-DECK: committed 0.31 m past the edge, and with a
// 32-degree normal limit instead, 0.16 m past it, still creeping off). The
// slack covers the deployed 30-degree flight, where the surface under the
// centre is 0.95 m down.
constexpr float kCheckpointFootingSlack = 0.15F;
constexpr float kPlayerMaximumRelativeSpeed = 5.5F;
constexpr float kGroundAcceleration = 22.0F;
// 14.0, not the original 8.0. Measured directly, by executing a jump and an
// airborne redirect against the real solver, not calculated: a still-
// airborne redirect from a dead stop to 90% of max relative speed
// took 0.622 s at 8.0 -- more than half of an ordinary jump's own measured
// 1.111 s total airtime -- against GDD 7's own "Player intent should feel
// immediate and athletic... sluggishness is not used as a substitute for
// physical credibility." At 14.0 (63.6% of ground control, so air steering
// still reads as distinctly weaker than ground -- a jump stays a committed
// arc, not free ground-speed control) the same redirect measures 0.356 s,
// under a third of airtime, with real time left to act on it before
// landing. kGroundAcceleration, kJumpSpeed and kPlayerMaximumRelativeSpeed
// were measured against the same probe and left unchanged: ground accel
// already reaches full speed in 0.25 s and jump apex/airtime (1.51 m,
// 1.11 s) both read as athletic, not floaty -- the redirect lag was the one
// number the numbers themselves flagged.
constexpr float kAirAcceleration = 14.0F;
constexpr float kJumpSpeed = 5.5F;
constexpr double kTranslatingSupportAmplitudeMeters = 2.0;
constexpr double kTranslatingSupportAngularFrequency = 1.0;
constexpr double kRotatingSupportAngularSpeed = 0.8;
constexpr double kMovingLedgeAmplitudeMeters = 1.5;
constexpr double kMovingLedgeAngularFrequency = 0.9;
constexpr double kMovingLedgeCenterZ = 12.5;

// Player capsule: cylinder half-height 0.55 plus radius 0.35.
constexpr float kPlayerRadius = 0.35F;
constexpr float kPlayerHalfHeight = 0.9F;
// Crouched (GDD 7.2): a 1.2 m capsule, same radius. The soles stay where they
// are, so the centre drops by the difference in half-heights.
constexpr float kPlayerCrouchHalfHeight = 0.6F;
constexpr float kCrouchDrop = kPlayerHalfHeight - kPlayerCrouchHalfHeight;
// Crouched walking tops out at 45 % of the standing speed (2.5 of 5.5 m/s).
constexpr double kCrouchSpeedScale = 0.45;
// The standing capsule is tested this far above the soles, so the floor it
// already rests on does not read as an obstruction; a ceiling does.
constexpr float kStandClearanceSkin = 0.05F;
// Crouch fixture: a beam across a 4 m lane, underside 1.45 m over the deck --
// under the standing capsule's 1.8 m, over the crouched one's 1.2 m -- on two
// posts. Between the traversal fixtures and the kerb run at z = -16.
constexpr float kCrawlLaneCenterX = -2.0F;
constexpr float kCrawlLaneHalfX = 2.0F;
constexpr float kCrawlBeamZ = -13.0F;
constexpr float kCrawlBeamHalfZ = 0.3F;
constexpr float kCrawlBeamUndersideY = 1.45F;
constexpr float kCrawlBeamHalfY = 0.2F;
constexpr float kCrawlPostHalf = 0.15F;

// An athletic climber with gear. Left to Jolt's default density this capsule
// weighs 602.9 kg -- freight, not a person -- which silently made the player
// the heaviest thing in any mechanism they stood on. GDD 17 puts the power in
// the tower, not the body; a machine that needs a human's weight must be built
// around a human's weight.
constexpr float kPlayerMassKg = 85.0F;

// Traversal reach / clearance rules. Every one of these is a bound on what the
// native assist may attempt; none of them fabricates geometry.
constexpr float kTraversalReach = 0.95F;
constexpr float kTopProbeInset = 0.12F;
constexpr float kLandingInset = kPlayerRadius + 0.12F;
constexpr float kTopProbeMargin = 0.35F;
constexpr float kLedgeTopNormalThreshold = 0.7F;
constexpr float kLandingSkin = 0.02F;
constexpr float kLandingSupportProbeUp = 0.12F;
constexpr float kLandingSupportTolerance = 0.10F;

// Walking up a step. An edge lower than this in the path of a grounded,
// walking body is climbed the way legs climb it; anything taller is a vault
// or a mantle (both start at 0.35 m), so no height is left unclaimed.
constexpr float kStepMaximumHeight = 0.35F;
constexpr float kStepMinimumHeight = 0.02F;
constexpr float kStepLookahead = 0.08F;
constexpr float kStepMinimumSpeed = 0.2F;

constexpr float kMantleMinimumRise = 0.35F;
constexpr float kMantleMaximumRise = 1.85F;
constexpr float kMantleClearanceLift = 0.12F;
constexpr double kMantleDurationSeconds = 0.42;

constexpr float kVaultMinimumRise = 0.35F;
constexpr float kVaultMaximumRise = 1.15F;
constexpr float kVaultCrossDistance = 1.30F;
constexpr float kVaultApexClearance = 0.10F;
constexpr float kVaultMaximumDrop = 1.40F;
constexpr double kVaultDurationSeconds = 0.38;
// Double-tap Jump: a second Jump this soon after a takeoff asks for the vault
// the takeoff could have been. It is granted only on exactly the ground
// vault's terms -- a real obstacle top 0.35-1.15 m over the takeoff floor and
// a real landing beyond it -- so it answers a late press, and never makes a
// vault the ground itself would have refused (Governing Law 4).
constexpr std::uint32_t kJumpVaultWindowTicks =
    static_cast<std::uint32_t>(0.30 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));

// Hang band expressed against the capsule centre: hands reach a ledge between
// chest height and just above the head.
constexpr float kHangMinimumRiseAboveCentre = 0.45F;
constexpr float kHangMaximumRiseAboveCentre = 1.35F;
constexpr float kHangDropBelowLedge = 1.05F;
constexpr float kHangWallGap = 0.06F;
constexpr float kHangMaximumClimbSpeed = 0.2F;
constexpr double kHangIntentDotThreshold = 0.3;

// A standing mantle steps in before it climbs. The probe offers a ledge from
// 1.30 m out, and a body that rises from there hangs an arm's length off the
// wall with nothing under its hands; a climber closes on the wall, hands on
// the lip, then pulls. The step ends at the hang hold's own standoff, so a
// mantle from the ground passes the wall distance a mantle from a hang starts
// at -- and it is a step: swept clear of real geometry, onto real support, in
// a frame the ground and the ledge share. Where any of that fails the step
// shortens, down to none, and the climb starts where the player stands.
constexpr float kMantleApproachStandoff = kPlayerRadius + kHangWallGap;
constexpr float kMantleApproachSpeedMps = 3.0F;        // mean speed over the step
constexpr float kMantleApproachLift = 0.05F;           // clears the resting contact
constexpr float kMantleApproachSweepMargin = 0.02F;
constexpr float kMantleApproachSupportSpacing = 0.10F;
constexpr float kMantleApproachFrameDriftMps = 0.05F;  // ground vs ledge
constexpr float kMantleApproachMaximumEntrySlope = 3.0F;

// After a deliberate release the controller stops offering an automatic re-grab
// for a moment, so letting go is a real decision rather than an instant re-hang.
constexpr std::uint32_t kReleaseRegrabLockoutTicks = 27;

// ---- Step 2 movement (03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md §8) ----
// Sprint: 8.0 m/s against the 5.5 walk, with the stick at least 0.7 and
// within 45 degrees of the facing.
constexpr double kSprintSpeedScale = 8.0 / 5.5;
constexpr double kSprintMinimumInput = 0.7;
constexpr double kSprintMaximumAngleCos = 0.7071;
// A hold: a member a hand can close round -- its two thinner dimensions at
// most kGripMaxSection, its length at least kGripMinLength -- on a static
// body, or on a moving one heavy enough to carry a climber (a 3 kg handle is
// not a hold).
constexpr float kGripMaxSection = 0.18F;
constexpr float kGripMinLength = 0.25F;
constexpr float kGripMinBodyMassKg = 400.0F;
// Where a hand looks for a hold: a box this big (across, up, along the
// facing) round the point it aims at.
const JPH::Vec3 kGripSearchHalf(0.30F, 0.30F, 0.35F);
// Climbing: the body moves at these rates. Its hands aim this high over its
// centre, this far in front of it and this far to either side; one hand
// moves at a time.
constexpr float kClimbUpSpeed = 0.9F;
constexpr float kClimbDownSpeed = 1.2F;
constexpr float kClimbSideSpeed = 0.6F;
constexpr double kClimbInputDeadzone = 0.2;
constexpr float kClimbHandHigh = 0.80F;
// Climbing on up needs a hold from chest height: a body pushes up on holds at
// its chest, so it rises to the top of a pipe, not an arm's length short.
constexpr float kClimbMoveAim = 0.50F;
constexpr float kClimbHandMid = 0.45F;
constexpr float kClimbHandLow = 0.20F;
constexpr float kClimbHandReach = 0.40F;
constexpr float kClimbHandSpan = 0.20F;
constexpr float kClimbSideLead = 0.35F;
constexpr std::uint32_t kClimbRegripTicks = 18;
constexpr float kClimbJumpBackSpeed = 2.5F;
constexpr float kClimbJumpUpSpeed = 4.0F;
// Climbing down, the soles find ground this close under them and step off.
constexpr float kClimbFootReach = 0.10F;
// Shimmy along a hung ledge.
constexpr float kShimmySpeed = 0.6F;
// Balance: walking a support narrower than kBalanceMaxWidth and at least
// kBalanceMinLength long. Sideways input under kBalanceStepOffInput is held on
// the support's line; over it, the body steps off.
constexpr float kBalanceMaxWidth = 0.50F;
constexpr float kBalanceMinLength = 1.50F;
constexpr double kBalanceSpeedScale = 2.0 / 5.5;
constexpr double kBalanceStepOffInput = 0.8;
constexpr float kBalanceCentering = 3.0F;
constexpr float kBalanceCenteringMaxMps = 0.4F;
// Controlled drop: an edge behind the body within kEdgeSearchReach, over a
// drop of at least kEdgeDropMinimum, lowered over in kLoweringSeconds.
constexpr float kEdgeSearchStart = 0.25F;
constexpr float kEdgeSearchReach = 0.90F;
constexpr float kEdgeSearchStep = 0.05F;
constexpr float kEdgeDropMinimum = 1.5F;
constexpr float kLipProbeDepth = 0.05F;
constexpr float kClimbSweepRadius = 0.30F;
// A hang's hands, either side of the body on the lip.
constexpr float kHangHandSpan = 0.22F;
constexpr double kLoweringSeconds = 0.6;

// --- coupled machine geometry, metres / seconds / kilograms ---------------
// The plant sits in the approach yard between grade spawn and the tower, so the
// player meets it on the way in rather than being born inside it.
constexpr double kMachineCyclePeriodSeconds = 26.0;
constexpr float kScoopX = 34.0F;
constexpr float kScoopZ = -96.0F;
constexpr float kScoopBottomY = 0.03F;
constexpr float kScoopTopY = 12.0F;
constexpr float kScoopDischargeTilt = 0.62F;
constexpr float kTipperHingeX = 29.0F;
constexpr float kTipperHingeY = 3.6F;
constexpr float kTipperZ = -96.0F;
constexpr float kValveHingeX = 29.6F;
constexpr float kValveHingeY = 7.2F;
constexpr float kRopeSlackMeters = 0.15F;
constexpr float kLiftMastX = 13.0F;
constexpr float kLiftZ = -100.0F;
constexpr float kLiftPlatformX = 16.4F;
constexpr float kLiftPlatformRestY = 1.2F;
constexpr float kLiftTravelMeters = 7.6F;
constexpr float kCounterweightX = 11.6F;
constexpr float kSheaveY = 10.4F;
constexpr float kBallastMassKg = 380.0F;
constexpr float kLiftPlatformMassKg = 2100.0F;
constexpr float kCounterweightMassKg = 1800.0F;
constexpr float kTipperMassKg = 900.0F;
constexpr float kValveLeverMassKg = 90.0F;
// Valve lever angle band that maps to a fully shut / fully open orifice.
constexpr float kValveShutAngle = -0.05F;
constexpr float kValveOpenAngle = 0.62F;

// --- WO-010 catwalk treadle ------------------------------------------------
// The player masses 85 kg and cannot shift a 900 kg counterweighted tipper, so
// body-in-the-machine has to happen through a control built for a body. The
// treadle is a see-saw on the catwalk deck: standing on the outboard end lifts
// the inboard end, which hauls a cable to the valve lever's counterweight and
// opens the orifice. Arms are equal, so this is reach and placement rather than
// force multiplication -- the valve gear only needs ~190 N.m, which is human
// scale by design, and the treadle is only reachable by someone the lift has
// already carried up (WO-009).
constexpr float kTreadleX = 16.4F;
constexpr float kTreadleZ = -106.0F;
constexpr float kTreadleHingeY = 9.09F;      // 0.40 m above the catwalk deck: a step, not a mantle.
constexpr float kTreadlePlateMeters = 1.5F;  // Plate runs from the hinge out to -x.
constexpr float kTreadleCableArm = 0.70F;    // Cable hangs from here, under a sheave.
constexpr float kTreadleMassKg = 130.0F;
// Rest is level (held against the min stop by the inboard counterweight, valve
// shut); depressed is the throw that hauls the valve gear fully open.
constexpr float kTreadleRestAngle = 0.0F;
constexpr float kTreadleDepressedAngle = 0.24F;
// Sheave heights above each cable anchor. The run is a real two-sheave cable
// span across the yard, which is also what makes the linkage legible from the
// catwalk: you can see what the pedal is wired to.
constexpr float kTreadleSheaveRise = 2.0F;
constexpr float kValveCableArm = 0.25F;      // Short arm: 0.17 m of travel, ~750 N to move.
constexpr float kValveSheaveY = 9.6F;
// Sheave masts stand clear of everything that swings: the treadle mast is set
// off the walkway in z so the plate and the player never foul it, and the valve
// mast is set off in z so it misses the lever's counterweight.
constexpr float kTreadleMastZ = -105.0F;
constexpr float kValveMastZ = -92.0F;
constexpr float kCatwalkDeckY = 8.69F;

// --- WO-011 Ascent Atlas v1.0 kernel: KX-JIB / KX-CRATE --------------------
// Ascent Atlas section 9 places the WO-005..008 kernel at "z=0-24 m, plan cut
// 36 m x 36 m" as its own bounded proof volume, separate from band content --
// it is explicitly not the Kellerworks yard above. Sited well clear of it.
constexpr float kKernelBaseX = 200.0F;
constexpr float kKernelBaseZ = 0.0F;
constexpr float kKernelDeckHalfExtent = 10.0F; // 20 m square kernel apron.

constexpr float kJibMastX = kKernelBaseX;
constexpr float kJibMastZ = kKernelBaseZ;
constexpr float kJibMastHeight = 5.0F;
constexpr float kJibBoomLength = 6.0F;
constexpr float kJibBoomMassKg = 400.0F;
// Slew is bounded, not a full 360 -- a compact pendant swinging a load between
// a pickup point and a drop point, matching WO-005's "a compact pendant is
// enough" scope note. +/-2.0 rad (~115 deg) covers pickup-to-drop with margin.
constexpr float kJibSlewLimitRadians = 2.0F;
// The crate+hook at the boom tip (radius ~4.65 m) carries most of the slew
// moment of inertia: I ~= (1200+40)*4.65^2 + boom's own (1/3)*400*6^2 ~=
// 31,600 kg*m^2. 6000 N*m -- an earlier, unmeasured guess -- produced 0.006
// rad/s after 6 s of full command, not the target 0.5 rad/s. Measured via a
// probe and corrected; this value is a real, falsifiable design target now,
// not a guess (Atlas section 0.3).
constexpr float kJibSlewMaxTorqueNm = 45000.0F;
constexpr float kJibSlewMaxRateRadPerSec = 0.5F;

constexpr float kJibHookMassKg = 40.0F;
constexpr float kJibHoistMaxRateMetersPerSec = 1.0F;
// The rated winch force. A finite, enforced Jolt motor limit (Governing Law 26:
// a real constraint capacity, not a number that only appears in an HUD label).
// Atlas section 0.3: masses/loads below this line are design targets, not
// proof requirements, until a benchmark scene exists -- this WO is that scene.
constexpr float kJibMaxLiftForceN = 20000.0F;

constexpr float kCrateMassKg = 1200.0F; // weight ~11.8 kN, well inside rating.
constexpr float kCrateHalfExtent = 0.75F;

// A fixed, permanently-overweight capacity-proving stand: same rated winch
// force as the jib's hoist, a load past that rating, always commanded to
// raise. Proves "unlimited winch force" is forbidden without staging an
// unsafe lift on the real jib (WO-005 forbidden-shortcuts list).
constexpr float kCapacityStandX = kKernelBaseX + 8.0F;
constexpr float kCapacityStandZ = kKernelBaseZ + 6.0F;
constexpr float kCapacityStandMastHeight = 4.0F;
constexpr float kCapacityStandLoadMassKg = 2500.0F; // weight ~24.5 kN > rating.
constexpr float kCapacityStandLoadHalfExtent = 0.6F;

// Pendant station: a fixed point near the mast base. Commands only take
// effect within this radius (WO-005: "Action to enter station").
constexpr float kJibStationX = kJibMastX - 2.5F;
constexpr float kJibStationZ = kJibMastZ - 2.0F;
constexpr float kJibStationRadius = 2.5F;

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
constexpr float kNeedleGapCenterX = kKernelBaseX;
constexpr float kNeedleGapCenterZ = kKernelBaseZ - 16.0F;
constexpr float kNeedleGapWidthMeters = 3.2F; // clear span the beam must bridge.
constexpr float kNeedlePierHalfExtentX = 2.5F;
constexpr float kNeedlePierHalfExtentZ = 1.6F;
constexpr float kNeedlePierHeight = 4.0F; // "one bay of frame" (Atlas section 9).
constexpr float kNeedlePierTopY = kNeedlePierHeight;

constexpr float kNeedlePierApproachX =
    kNeedleGapCenterX - (kNeedleGapWidthMeters * 0.5F + kNeedlePierHalfExtentX);
constexpr float kNeedlePierFarX =
    kNeedleGapCenterX + (kNeedleGapWidthMeters * 0.5F + kNeedlePierHalfExtentX);

// How far each end must rest onto its pier once seated.
constexpr float kNeedleSeatOverlapMeters = 0.9F;
constexpr float kNeedleBeamHalfLength = kNeedleGapWidthMeters * 0.5F + kNeedleSeatOverlapMeters;
constexpr float kNeedleBeamHalfWidth = 0.5F;
constexpr float kNeedleBeamHalfHeight = 0.18F;
constexpr float kNeedleBeamMassKg = 900.0F; // GDD 17: real machinery, not player strength.

// Rest (seated) height of the beam's centreline -- also the hard bottom of
// the hoist's travel, so "reaches the bottom" and "reaches the seat" are the
// same event, not two independently-tuned numbers that could drift apart.
// The beam's *top* is set flush with the pier tops (seated into a pocket,
// not resting proud on top of one): this locomotion is a raw dynamic
// capsule with no step-up assist at all (confirmed by direct observation --
// a proud-mounted beam, top 0.36 m above the pier, flatly blocked forward
// walking rather than being climbed), so any step here is a wall, and a real
// seated span has to be a level continuation of the pier top, not a curb.
constexpr float kNeedleSeatedY = kNeedlePierTopY - kNeedleBeamHalfHeight;
// Top of the notch block cut into each pier's gap-facing edge (see
// build_kernel_needle): low enough that the seated beam's underside clears
// it and its own top still lands flush with the main pier top.
constexpr float kNeedlePocketNotchTopY = kNeedlePierTopY - 2.0F * kNeedleBeamHalfHeight;
// Clearance between the notch's boundary and the beam's own resting edge,
// well past JPH::BoxShape's default convex radius plus Jolt's default
// speculative contact distance, so the main block's rounded corner can never
// intercept the descending beam (see build_kernel_needle).
constexpr float kNeedlePocketMarginMeters = 0.2F;

// Pocket world points: the beam's two end centrelines when correctly seated.
// The beam has no horizontal or slew freedom at all (see above), so these
// are the only points its ends can ever occupy -- alignment is guaranteed by
// construction, not by a tolerance check.
constexpr float kNeedlePocketApproachX = kNeedleGapCenterX - kNeedleBeamHalfLength;
constexpr float kNeedlePocketFarX = kNeedleGapCenterX + kNeedleBeamHalfLength;

constexpr float kNeedleHoistMastHeight = 7.0F; // clears the beam's stowed pose above the piers.
// Stowed (top of travel): near the mast head, clear of the piers entirely.
constexpr float kNeedleStowedY = kNeedleHoistMastHeight - 0.8F;
constexpr float kNeedleHoistMaxRateMetersPerSec = 0.6F;
constexpr float kNeedleMaxLiftForceN = 16000.0F; // weight ~8.8 kN, well inside rating.

// A rest-pose seat predicate (WO-006: "a model that can say seated => support
// predicate true... optional sag only if the chosen reduced model already
// exists" -- this one has none). Speed, not position, is the operative test:
// position is already guaranteed by construction, so all that remains is
// "has it actually come to rest at the bottom," not "is it approximately
// somewhere near it."
constexpr float kNeedleSeatPositionToleranceMeters = 0.12F;
constexpr float kNeedleSeatSpeedToleranceMetersPerSec = 0.35F;
// A sustained raise command while seated is the legal unseat path: it pulls
// the pockets pins first (the beam cannot otherwise move at all while
// rigidly pinned), then the same motor that lowered it lifts it clear -- a
// real mechanism reversal, not a teleport or a flag flip.
constexpr float kNeedleUnseatCommandThreshold = 0.5F;

// Sited on the approach pier itself, not at grade: the pier top is the only
// place a player standing at grade cannot climb back up to unaided (no
// stair/ramp exists in this kernel slice), so the pendant has to be where
// the operator can actually reach it and then step onto the seated beam.
// At the pier's own centre, clear of the beam's horizontal footprint (which
// starts at kNeedlePocketApproachX = 197.5) by well over kPlayerRadius --
// close enough to that edge and the descending beam clips the standing
// player and wedges them against its face (found by direct observation:
// an earlier siting 0.1 m from that edge froze forward movement dead).
constexpr float kNeedleStationX = kNeedlePierApproachX;
constexpr float kNeedleStationZ = kNeedleGapCenterZ;
constexpr float kNeedleStationRadius = 2.5F;

// --- The stack: the tower's climbable lower section -------------------------
// The machine IS the building. This is a real open steel frame the player
// walks inside and outside of -- perimeter deck rings around a central shaft,
// so you can always see up and down through the structure -- not the solid
// slab that stood here before, which had no interior at all and could only
// ever be looked at from the yard.
constexpr float kStackCenterX = 0.0F;
constexpr float kStackCenterZ = -150.0F;
constexpr float kStackHalfExtent = 26.0F;      // 52 m square footprint: a building, not a mast.
constexpr float kStackLevelHeight = 11.0F;     // generous industrial floor-to-floor.
constexpr int kStackLevelCount = 14;           // decks at 11..154 m; level 0 is grade.
constexpr float kStackDeckHalfThickness = 0.25F;
constexpr float kStackDeckBandDepth = 9.0F;    // walkable perimeter band; leaves a 34 m shaft.
constexpr float kStackColumnHalf = 0.8F;
constexpr float kStackRampHalfWidth = 1.6F;
// Each flight climbs under the deck band of the level it serves, so that deck
// is cut open above the flight's upper run: from where headroom over the
// slab falls under ~2.5 m to the flight's head, and 0.5 m clear of the slab
// each side (the stringers guard the edges). Without the well every flight
// ended at the deck's underside with the player's head on it.
constexpr float kStackStairwellStart = 7.5F;       // along the climb, from the stack centre
constexpr float kStackStairwellHalfWidth = 2.1F;

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
constexpr float kIntakeBayCenterX = 0.0F;
constexpr float kIntakeBayFrontZ = -110.5F;   // wall carrying the dog throat
constexpr float kIntakeBayBackZ = -122.5F;    // 12 m deep bay
constexpr float kIntakeBayHalfX = 10.0F;
// 5 m of wall against a 1.85 m maximum mantle rise: the throat is the only way
// in at grade, and that is a geometric fact, not an invisible wall.
constexpr float kIntakeBayWallHeight = 5.0F;
constexpr float kIntakeBayWallHalfZ = 0.30F;
constexpr float kIntakeThroatHalfWidth = 1.30F;   // 2.60 m opening
constexpr float kIntakeThroatHeight = 2.50F;      // capsule needs 1.80 m

// MOD-DOG-A: a hinged landing dog filling the throat, swinging into the bay.
// Every boundary is held off its neighbour by kIntakeDogClearance rather than
// being exactly coincident: JPH::BoxShape carries a rounded convex radius, so
// a plate built exactly flush with the jambs, the lintel and the ground jams
// against all three and never travels. 0.06 m clears that and is still an
// order of magnitude under the 0.70 m capsule, so the throat stays shut.
constexpr float kIntakeDogClearance = 0.06F;
constexpr float kIntakeDogHalfThickness = 0.20F;
// The hinge sits at the plate's own end face, so the trailing corner sweeps a
// circle of the plate's half-thickness as it opens. Setting the plate back by
// that half-thickness plus the clearance is what keeps that corner out of the
// west jamb -- without it the dog binds at ~0.31 rad and never travels.
constexpr float kIntakeDogHalfWidth =
    kIntakeThroatHalfWidth - kIntakeDogHalfThickness - kIntakeDogClearance;
constexpr float kIntakeDogHalfHeight =
    (kIntakeThroatHeight - 2.0F * kIntakeDogClearance) * 0.5F;
constexpr float kIntakeDogCenterY = kIntakeDogClearance + kIntakeDogHalfHeight;
constexpr float kIntakeDogMassKg = 900.0F;
constexpr float kIntakeDogHingeX = kIntakeBayCenterX - kIntakeDogHalfWidth;
constexpr float kIntakeDogRetractAngle = 1.45F;         // Atlas B00 figure
constexpr float kIntakeDogRetractSpeed = 0.62F;         // rad/s, Atlas B00
// Rated so the dog swings its own 900 kg plate but cannot shove freight:
// breaking 4000 kg loose at mu = 0.6 needs 0.6 * 4000 * 9.81 = 23544 N, which
// at the pack's ~2.0 m contact radius is 47088 N*m. 12000 N*m is a quarter of
// that, and roughly five times what accelerating the plate alone demands
// ((1/3) * 900 * 2.6^2 * 1.24 = 2515 N*m).
constexpr float kIntakeDogMaxTorqueNm = 12000.0F;
// Derived-predicate threshold only (HUD and falsifiers): the dog is "travelled"
// once it has swung clear enough that the throat passes a 0.70 m capsule.
constexpr float kIntakeThroatClearAngle = 1.20F;

// MOD-YARD-JIB: Atlas B00 gives 12 m boom, 11.50 m boom height, 5 t SWL.
constexpr float kIntakeGravity = 9.81F;
constexpr float kIntakeJibSwlKg = 5000.0F;
constexpr float kIntakeBoomLength = 12.0F;
constexpr float kIntakeBoomHeight = 11.50F;
constexpr float kIntakeJibMastX = kIntakeBayCenterX;
// Sited so the boom's full working radius lands the pack just inside the
// throat: close enough to be inside the dog's 2.48 m swing arc, and clear of
// MOD-STAIR-A's southern edge at z = -114.7.
constexpr float kIntakeJibMastZ = -100.2F;
constexpr float kIntakeJibBoomMassKg = 2400.0F;
constexpr float kIntakeJibHookMassKg = 120.0F;
// tau = r * F = 12 * 5000 * 9.81 = 588600 N*m; winch F = 5000 * 9.81 = 49050 N.
constexpr float kIntakeJibSlewTorqueNm =
    kIntakeBoomLength * kIntakeJibSwlKg * kIntakeGravity;
constexpr float kIntakeJibWinchForceN = kIntakeJibSwlKg * kIntakeGravity;
constexpr float kIntakeJibSlewLimitRadians = 0.90F;     // Atlas B00
constexpr float kIntakeJibSlewRateRadPerSec = 0.22F;    // Atlas B00
constexpr float kIntakeJibHoistRateMetersPerSec = 0.85F; // Atlas B00

// The 4 t pack, and the 9 t pack that proves the rating is real.
constexpr float kIntakePackHalfX = 1.10F;
constexpr float kIntakePackHalfY = 0.90F;
constexpr float kIntakePackHalfZ = 1.15F;
constexpr float kIntakePackMassKg = 4000.0F;   // 39240 N, inside 49050 N
constexpr float kIntakeOverweightPackMassKg = 9000.0F; // 88290 N, outside it
// Sited exactly at the boom's full working radius, straight down the -Z line
// from the mast, and therefore squarely inside the dog's swing envelope.
constexpr float kIntakePackZ = kIntakeJibMastZ - kIntakeBoomLength;
constexpr float kIntakeOverweightStandX = 7.0F;
constexpr float kIntakeOverweightStandZ = -98.0F;
constexpr float kIntakeOverweightMastHeight = 9.0F;

// MOD-INTAKE-BELT: 18 m stroke translating slat deck. Riding it is legal
// (WO-002 support-point velocity law), so it is a moving support, not scenery.
constexpr float kIntakeBeltX = 6.0F;
constexpr float kIntakeBeltTopY = 1.38F;
constexpr float kIntakeBeltHalfY = 0.18F;
constexpr float kIntakeBeltCenterZ = -96.0F;
constexpr float kIntakeBeltHalfX = 2.0F;
constexpr float kIntakeBeltHalfZ = 6.0F;
constexpr double kIntakeBeltStrokeMeters = 18.0;
constexpr double kIntakeBeltAngularFrequency = 0.40; // Atlas B00 omega

// CAP-PENDANT: the jib control, on the belt catwalk where Atlas B00 puts it.
constexpr float kIntakePendantX = 3.0F;
constexpr float kIntakePendantZ = -100.0F;
constexpr float kIntakePendantTopY = kIntakeBeltTopY;
constexpr float kIntakeStationRadius = 3.40F;  // Atlas B00

// MOD-STAIR-A: six switchback flights of 4 m inside the bay, grade to +24 m.
// Opposing flights run in two separate Z lanes joined by a landing at each
// turn. Sharing one lane makes consecutive flights meet in a V whose apex
// pinches below the 1.80 m standing capsule -- the player climbs to the pinch
// and stops, which is a stair that cannot be walked.
// Set far enough north that the south lane clears the pack's standing pose at
// z = -113.35: a flight slab through the pack wedges the whole rig and the
// winch creeps at 0.02 m/s instead of its rated 0.85.
constexpr float kIntakeStairZ = -118.0F;
constexpr float kIntakeStairLaneOffset = 2.0F;
constexpr int kIntakeStairFlightCount = 6;
constexpr float kIntakeStairFlightRise = 4.0F;
constexpr float kIntakeStairHalfRun = 7.0F;     // 14 m run, 15.9 degree pitch
constexpr float kIntakeStairHalfWidth = 1.8F;
constexpr float kIntakeStairSlabHalfY = 0.18F;
constexpr float kIntakeStairLandingX = 8.0F;
constexpr float kIntakeStairLandingHalfX = 1.0F;
constexpr float kIntakeStairLandingHalfZ =
    kIntakeStairLaneOffset + kIntakeStairHalfWidth;
constexpr float kIntakeHandoffY =
    static_cast<float>(kIntakeStairFlightCount) * kIntakeStairFlightRise; // 24 m
constexpr float kIntakeHandoffCenterX = -6.0F;
constexpr float kIntakeHandoffHalfX = 4.0F;
constexpr float kIntakeHandoffHalfY = 0.20F;
constexpr float kIntakeHandoffSouthZ = -107.7F;  // meets the SKIN ladder head
// Stops short of the odd-flight lane: run the deck over it and the top of
// flight 5 is buried inside the deck instead of arriving on it.
constexpr float kIntakeHandoffNorthZ = -117.5F;

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
constexpr float kIntakeSkinCenterX = kIntakeHandoffCenterX;
constexpr float kIntakeSkinRungRise = 1.60F;
constexpr float kIntakeSkinRungHalfX = 1.0F;
constexpr float kIntakeSkinRungHalfY = 0.50F;
constexpr float kIntakeSkinRungHalfZ = 1.00F;
constexpr int kIntakeSkinRungCount = 15;       // tops at 1.6 .. 24.0 m
// The head rung sits flush against the handoff deck's south edge, so both
// braids -- MOD-STAIR-A and SKIN -- arrive on the same deck.
constexpr float kIntakeSkinHeadZ = kIntakeHandoffSouthZ + kIntakeSkinRungHalfZ;
constexpr float kIntakeSkinFootZ =
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
constexpr float kLegalFortyFlightLength = 16.0F;             // L: 8.000 m rise / sin(30 deg)
constexpr float kLegalFortyFlightHalfLength = kLegalFortyFlightLength * 0.5F; // r from the hinge
constexpr float kLegalFortyFlightHalfWidth = 0.90F;          // 1.80 m tread, one lane
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
constexpr float kLegalFortyFlightMassKg = 400.0F;
constexpr float kLegalFortyFlightRiseMeters = 8.0F;          // per flight; two flights close 16.000 m

// Swing hinge: world-Z axis, sited at the mid-landing's height above
// MOD-STAIR-A's lower run. theta is measured from the downward vertical;
// built at the stowed pose (8 deg) so the hinge's as-built angle is 0 -- the
// same convention kIntakeDogEntityId already uses.
constexpr float kIntakeSwingHingeX = 7.856F;
constexpr float kIntakeSwingHingeZ = -112.5F;
constexpr float kLegalFortyStowedThetaRadians = 0.139626F;   // 8 deg
constexpr float kLegalFortyDeployedThetaRadians = 1.047198F; // 60 deg
// Jolt's HingeConstraint::GetCurrentAngle() (Jolt/Physics/Constraints/
// HingeConstraint.cpp) reports the physical rotation body2 has undergone
// since construction, signed by the right-hand rule about the world hinge
// axis (+Z). This flight is built with its local +X already pointing along
// (sin(theta_stowed), cos(theta_stowed), 0); as theta increases toward
// deployed, that direction rotates TOWARD world +X, which is a NEGATIVE
// rotation about +Z -- so the reported angle runs stowed=0 to deployed
// -0.907571 rad, and every reader below negates it back to a non-negative
// "how far open" travel.
constexpr float kIntakeSwingHingeTravelRadians =
    kLegalFortyDeployedThetaRadians - kLegalFortyStowedThetaRadians; // 52 deg, 0.907571 rad

// The rope bracket: r_a = 15.000 m from the hinge (Design Values), which on a
// flight whose hinge is at local +8.00 and whose foot is at local -8.00 sits
// at local x = 8.00 - 15.00 = -7.00 -- one metre inboard of the foot, on the
// slab's underside so it clears the tread.
constexpr float kIntakeSwingRopeLeverArmMeters = 15.0F;  // r_a
// h_s, sheave above the hinge. The plan's own 4.0 m -- unlike the flight
// and tare masses above and below, this one needed no retuning once
// kIntakeSwingAnchorEntityId's own fix (excluding the flight from contact
// with its own hinge anchor) was in place.
constexpr float kIntakeSwingSheaveHeightMeters = 4.0F;
constexpr float kIntakeSwingBracketLocalX =
    kLegalFortyFlightHalfLength - kIntakeSwingRopeLeverArmMeters; // -7.00
constexpr float kIntakeSwingBracketLocalY = -kIntakeStairSlabHalfY;

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
constexpr float kIntakeCwCradleTareMassKg = 500.0F;
constexpr float kIntakeCwCradleHalfX = 1.50F;
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
constexpr float kIntakeCwCradleHalfY = 0.90F;
constexpr float kIntakeCwCradleHalfZ = 1.20F;
constexpr float kIntakeCwCradleTopBuildY = 5.20F;
constexpr float kIntakeCwCradleBuildCenterY = kIntakeCwCradleTopBuildY - kIntakeCwCradleHalfY;
constexpr float kIntakeCwCradleGuideHalfX = 0.35F;
constexpr float kIntakeCwCradleGuideHalfY = 4.0F;
constexpr float kIntakeCwCradleGuideHalfZ = 0.35F;
// Jolt's SliderConstraintSettings::mAutoDetectPoint anchors displacement 0 at
// the build pose (kNeedleStowedY's own comment: "travel is signed from the
// spawn pose"), so the plan's centre range y in [1.80, 4.60] -- top face in
// [3.00, 5.80] -- becomes signed limits relative to the build pose. Stated
// on the top face, they are unchanged by the car's height.
constexpr float kIntakeCwCradleLimitMinMeters = 3.00F - kIntakeCwCradleTopBuildY;
constexpr float kIntakeCwCradleLimitMaxMeters = 5.80F - kIntakeCwCradleTopBuildY;
// The hook hangs at the boom tip, so working radius is fixed at the 12 m
// boom; only the slew bearing chooses where the cradle sits on that circle.
constexpr float kIntakeCwCradleBearingRadians = 0.80F; // of the 0.90 slew limit

// The sling release/attach envelopes (AS-002 Design Values 8.4.3).
constexpr float kLegalFortyReleaseXZToleranceMeters = 0.55F;
constexpr float kLegalFortyReleaseYToleranceMeters = 0.35F;
constexpr float kLegalFortyReleaseSpeedToleranceMps = 0.15F;
constexpr float kLegalFortyAttachToleranceMeters = 0.40F;
constexpr float kLegalFortyAttachSpeedToleranceMps = 0.20F;

// MOD-STAIR-A upper flight (static) + mid-landing + MOD-HALL-DECK. Sited by
// the plan's own geometry table; each surface's height is derived at build
// time from AS-001's own tread-surface offset (kIntakeHandoffY plus slab
// half-thickness over cosine pitch) stacked with two more 8.000 m rises, so
// the whole run stays flush to the handoff deck below it by construction
// rather than by matching a second copy of the same literal.
constexpr float kLegalFortyMidLandingCenterX = 9.350F;
constexpr float kLegalFortyMidLandingHalfX = 1.05F;
constexpr float kLegalFortyMidLandingCenterZ = -115.80F;
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
constexpr float kLegalFortyMidLandingHalfZ = 5.20F;

constexpr float kLegalFortyUpperFlightCenterX = 2.422F;
constexpr float kLegalFortyUpperFlightCenterZ = -117.0F;

constexpr float kLegalFortyHallDeckHalfX = 10.0F;
constexpr float kLegalFortyHallDeckCenterZ = -115.1F;
constexpr float kLegalFortyHallDeckHalfZ = 7.40F;
constexpr float kLegalFortyHallDeckHalfThickness = 0.20F;

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
constexpr float kLegalFortyWellCenterX = -1.65F;
constexpr float kLegalFortyWellHalfX = 3.15F;
constexpr float kLegalFortyWellCenterZ = -117.0F;
constexpr float kLegalFortyWellHalfZ = 2.20F;

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
constexpr int kLegalFortySkinRungFirst = 16;
constexpr int kLegalFortySkinRungLast = 20;    // tops at 32.1872 m, the mid-landing's height
constexpr float kLegalFortySkinRungFirstZ = -110.0F;
constexpr float kLegalFortySkinRungStepZMeters = 2.0F;

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
constexpr float kLegalFortySkinRungJogX = -0.8F;
constexpr float kLegalFortySkinWalkwayMinX = -5.8F;  // rung 20's east face
constexpr float kLegalFortySkinWalkwayMaxX = 8.30F;
// z in [-121, -117]: rung 20's own band plus 2 m south of it. Rung 20's band
// alone ([-119, -117]) runs under the upper flight's z in [-117.9, -116.1],
// whose underside falls toward its foot to 0.6 m over the walkway at x = 8:
// a standing capsule (radius 0.35) had one 0.4 m-wide lane, z in
// [-118.65, -118.25], and anyone walking the walkway's middle met the slab
// (reported by a player as an opening too small to fit through). South of
// the flight the air is clear to the hall deck's underside (39.79 m, 7.6 m
// up), so the added 2 m gives a 2.4 m lane with full headroom.
constexpr float kLegalFortySkinWalkwayCenterZ = -119.0F;
constexpr float kLegalFortySkinWalkwayHalfZ = 2.00F;

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
constexpr float kHook5CageTopY = 4.45F;
constexpr float kHook5RoofHalfY = 0.15F;
constexpr float kHook5WallTopY = kHook5CageTopY - 2.0F * kHook5RoofHalfY;  // 4.15, under the roof
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
constexpr float kHook5MinX = 8.0F;      // the belt deck's east edge
constexpr float kHook5MaxX = 12.0F;
constexpr float kHook5MinZ = -88.0F;
constexpr float kHook5MaxZ = kHook5MinZ + 4.0F;
constexpr float kHook5ButtressMaxX = 9.4F;  // west buttress x in [8.0, 9.4], full height
constexpr float kHook5WallThickness = 0.30F;
// The hatch cut in the roof, over open floor west of the rack and north of
// the door's sweep, so the drop lands clear of both. 1.2 x 1.6 m.
constexpr float kHook5HatchMinX = 9.7F;
constexpr float kHook5HatchMaxX = 10.9F;
constexpr float kHook5HatchMinZ = kHook5MinZ + 1.4F;
constexpr float kHook5HatchMaxZ = kHook5MinZ + 3.0F;
// The south wall's doorway, 1.40 m under a 2.20 m header.
constexpr float kHook5DoorwayMinX = 9.85F;
constexpr float kHook5DoorwayMaxX = 11.25F;
constexpr float kHook5DoorwayTopY = 2.20F;
// MOD-HOOK5-DOOR: hinged 0.20 m back from the west jamb (MOD-DOG-A bound at
// 0.31 rad because its hinge sat flush with its jamb), swinging INWARD under a
// permanent opening drive commanded at build time and never again. Shut, the
// leaf leaves 0.20 m at the hinge jamb and 0.06 m at the latch jamb.
constexpr float kHook5DoorHingeX = 10.05F;
constexpr float kHook5DoorZ = kHook5MinZ + 0.15F;          // the wall's mid-plane
constexpr float kHook5DoorHalfLength = 0.57F;             // shut: x in [10.05, 11.19]
constexpr float kHook5DoorHalfHeight = 1.06F;             // y in [0.06, 2.18]
constexpr float kHook5DoorHalfThickness = 0.12F;
constexpr float kHook5DoorCenterY = 1.12F;
constexpr float kHook5DoorMassKg = 80.0F;
constexpr float kHook5DoorOpenAngle = 1.45F;              // inward stop
constexpr float kHook5DoorDriveSpeed = 0.55F;             // rad/s
// 600 N*m, not the plan's 6 000: the rigid bar holds against any torque, and
// a 6 000 N*m leaf pins the bar in its brackets with ~7 kN, so lifting it
// would fight that much friction. 600 N*m swings the 80 kg leaf (34.7 kg*m^2
// about the hinge) at 17 rad/s^2 and pins the bar with ~0.5 kN.
constexpr float kHook5DoorDriveTorqueNm = 600.0F;
// MOD-HOOK5-BAR: 38 kg across the inside of the doorway, 0.04 m north of the
// shut leaf, so the drive stalls within 0.035 rad. Its ends sit in two
// brackets -- a ledge under each end and a stop on its north face -- that are
// OUTSIDE the leaf's sweep (radius 1.146 m about the hinge): the west one
// behind the hinge, the east one 1.31 m from it. (The plan put the keepers
// inside the sweep, where they would have stopped the door with the bar
// gone.) Lift the bar out and nothing is left in the swing.
constexpr float kHook5BarMassKg = 38.0F;
constexpr float kHook5BarHalfLength = 1.025F;             // x in [9.50, 11.55]
constexpr float kHook5BarHalfSection = 0.09F;
constexpr float kHook5BarCenterX = 10.525F;
constexpr float kHook5BarCenterY = 1.00F;    // handle 1.09, under the carry hands
constexpr float kHook5BarCenterZ = kHook5MinZ + 0.40F;
constexpr float kHook5BracketHalfX = 0.10F;
constexpr float kHook5BracketWestX = 9.60F;
constexpr float kHook5BracketEastX = 11.45F;
// CAP-HOOK5: the 36 kg hook block on a pedestal rack in the north-east
// corner. There, not mid-wall: the 2.05 m bar has to be set down somewhere in
// a 2.3 m-wide room clear of the door's swing, and with the rack mid-wall
// every such place put one end of the bar on it (observed).
constexpr float kHook5BlockMassKg = 36.0F;
constexpr float kHook5BlockHalf = 0.30F;
constexpr float kHook5BlockHalfY = 0.25F;
constexpr float kHook5RackTopY = 0.60F;       // block top 1.10, under the carry hands
constexpr float kHook5BlockSeatX = 11.35F;
constexpr float kHook5BlockSeatZ = kHook5MaxZ - kHook5WallThickness - 0.45F;
constexpr float kHook5BlockSeatY = kHook5RackTopY + kHook5BlockHalfY;
constexpr float kHook5InRackTolerance = 0.35F;            // hook_in_rack: within this of the seat

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
constexpr float kCarryReach = 1.20F;
constexpr float kCarryMaxBodySpeed = 0.50F;
constexpr float kCarryHandForward = 0.60F;
constexpr float kCarryHandUp = 0.35F;
// The hands swing round to the facing at this rate, not with it: a 36 kg load
// turns with the body, and a glance round would otherwise throw the hand
// point 1.2 m in one tick and the load out of it (observed with an instant
// half-turn). A half-turn with a load takes about a second.
constexpr float kCarryTurnRadiansPerSecond = 3.0F;
// A held body wedged hard enough that the hands are dragged this far from its
// handle, and still moving apart, slips out of them. Nothing holds a wedged
// steel member, and without this a bar jammed across a doorway would anchor
// the player to it. "Still moving apart" is what spares a pick-up: a block
// taken off the floor at the edge of reach starts over 1 m below the hands
// and closes on them, so it is held.
constexpr float kCarrySlipDistance = 0.90F;
// AS-006: a hand holds a mechanism-kit body -- a shackle, a trip-line handle,
// a load -- with at most this force. Pulled harder for kGripSeconds running,
// the body is torn out of the hands: grip gives to a sustained pull (a lever
// on its stop, a cage rising away), not to the jolt of a hand starting to
// move. The AS-003 bar and block keep the slip rule above alone.
constexpr float kGripNewtons = 900.0F;
constexpr float kGripSeconds = 0.10F;

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
constexpr float kSumpCenterX = kKernelBaseX;         // 200
constexpr float kSumpCenterZ = kKernelBaseZ + 16.0F; // clear of the jib (Z~0) and the needle (Z~-16).
constexpr float kSumpPlatformTopY = 3.0F; // a real, clearly-survivable fall through (~7.7 m/s impact).
constexpr float kSumpDeckHalfThickness = 0.15F;
constexpr float kSumpDeckHalfZ = 1.5F;
constexpr float kSumpApproachDeckHalfX = 1.5F;
constexpr float kSumpGrateHalfX = 1.5F;
constexpr float kSumpFarDeckHalfX = 1.5F;

constexpr float kSumpGrateX = kSumpCenterX;
constexpr float kSumpApproachDeckX = kSumpGrateX - kSumpGrateHalfX - kSumpApproachDeckHalfX;
constexpr float kSumpFarDeckX = kSumpGrateX + kSumpGrateHalfX + kSumpFarDeckHalfX;

// Lumped process state. Starts full (wet, unsafe) -- WO-006's needle and
// WO-005's jib both start in their "nothing done yet" pose; the sump matches
// that convention with "nothing isolated yet, still wet."
constexpr float kSumpCapacityKg = 1000.0F;
constexpr float kSumpInflowKgPerSec = 150.0F; // while the valve is open, inflow keeps it topped up.
constexpr float kSumpDrainKgPerSec = 100.0F;  // always draining; only wins once isolated.

constexpr float kSumpStationX = kSumpApproachDeckX;
constexpr float kSumpStationZ = kSumpCenterZ;
constexpr float kSumpStationRadius = 2.5F;

// WO-008 fall / parachute / checkpoint. An 8.8 m unassisted lift-platform
// fall (~13.1 m/s impact) must stay survivable per GDD 8.2; a genuine
// tower-scale drop must not be. Terminal parachute speed (~9 m/s, derived
// below) sits comfortably under that threshold -- Simulation::
// kLethalImpactSpeedMps, public in simulation.hpp -- with margin either side.

// Quadratic drag a = -k*v*|v|. Solved for a target terminal speed v_t at
// k = g / v_t^2 (net vertical accel is zero at v_t: g - k*v_t^2 = 0).
constexpr float kParachuteDragCoefficient = 0.1211F; // v_t ~= 9 m/s at g=9.81

// The presentation's static dressing as native collision (see
// kWorldSolidEntityId). Boxes are oriented; hulls are world-space point sets.
struct WorldSolidBox final {
    float px, py, pz;
    float qx, qy, qz, qw;
    float hx, hy, hz;
};
struct WorldSolidHull final {
    std::uint32_t first;
    std::uint32_t count;
};
#include "sim/world_solids.inc"

// A drawn box whose world bounds match an owned body this closely is that
// body's mirror, not a second body.
constexpr float kWorldSolidMirrorTolerance = 0.02F;
constexpr float kWorldSolidFriction = 0.8F;

constexpr float kTraversalStallTolerance = 0.22F;
constexpr std::uint32_t kTraversalStallAbortTicks = 12;

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterface() {
        mapping_[object_layers::kStatic] = broadphase_layers::kStatic;
        mapping_[object_layers::kMoving] = broadphase_layers::kMoving;
    }

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
        return broadphase_layers::kCount;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
        const JPH::ObjectLayer layer) const override {
        JPH_ASSERT(layer < object_layers::kCount);
        return mapping_[layer];
    }

    [[nodiscard]] const char *GetBroadPhaseLayerName(
        const JPH::BroadPhaseLayer layer) const override {
        if (layer == broadphase_layers::kStatic) {
            return "STATIC";
        }
        if (layer == broadphase_layers::kMoving) {
            return "MOVING";
        }
        JPH_ASSERT(false);
        return "INVALID";
    }

private:
    JPH::BroadPhaseLayer mapping_[object_layers::kCount];
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer object_layer,
                                     const JPH::BroadPhaseLayer broadphase_layer) const override {
        if (object_layer == object_layers::kStatic) {
            return broadphase_layer == broadphase_layers::kMoving;
        }
        if (object_layer == object_layers::kMoving) {
            return true;
        }
        JPH_ASSERT(false);
        return false;
    }
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer first,
                                     const JPH::ObjectLayer second) const override {
        if (first == object_layers::kStatic) {
            return second == object_layers::kMoving;
        }
        if (first == object_layers::kMoving) {
            return true;
        }
        JPH_ASSERT(false);
        return false;
    }
};

class JoltRuntimeLease final {
public:
    JoltRuntimeLease() {
        const std::scoped_lock lock(mutex_);
        if (lease_count_++ == 0) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    ~JoltRuntimeLease() {
        const std::scoped_lock lock(mutex_);
        if (--lease_count_ == 0) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

    JoltRuntimeLease(const JoltRuntimeLease &) = delete;
    JoltRuntimeLease &operator=(const JoltRuntimeLease &) = delete;

private:
    static std::mutex mutex_;
    static std::uint32_t lease_count_;
};

std::mutex JoltRuntimeLease::mutex_;
std::uint32_t JoltRuntimeLease::lease_count_ = 0;

struct SupportSample final {
    bool grounded = false;
    std::uint64_t entity_id = 0;
    scraperx::sim::Vector3 contact_point{};
    scraperx::sim::Vector3 point_velocity{};
    float normal_y = 0.0F;
};

// Supports that actually move. These outrank static ground when the player is
// in contact with both, so support-relative locomotion picks the machine.
[[nodiscard]] bool entity_is_moving_support(const std::uint64_t entity_id) noexcept {
    using Sim = scraperx::sim::Simulation;
    // Mechanism-kit bodies that move are machines by construction.
    return scraperx::sim::kit::is_dynamic_entity(entity_id) ||
           entity_id == Sim::kTranslatingSupportEntityId ||
           entity_id == Sim::kRotatingSupportEntityId ||
           entity_id == Sim::kMovingLedgeEntityId ||
           entity_id == Sim::kHoistScoopEntityId ||
           entity_id == Sim::kTipperEntityId ||
           entity_id == Sim::kLiftPlatformEntityId ||
           entity_id == Sim::kCounterweightEntityId ||
           entity_id == Sim::kTreadleEntityId ||
           entity_id == Sim::kJibHookEntityId ||
           entity_id == Sim::kCrateEntityId ||
           entity_id == Sim::kNeedleBeamEntityId ||
           entity_id == Sim::kIntakeBeltEntityId ||
           entity_id == Sim::kIntakeJibHookEntityId ||
           entity_id == Sim::kIntakePackEntityId ||
           entity_id == Sim::kIntakeDogEntityId ||
           // AS-002: the deployed swing flight is a dynamic body under real
           // load, not scenery -- a player standing on it while it is still
           // settling against its stop must inherit its point velocity under
           // the WO-002 law, exactly like every other machine member above.
           entity_id == Sim::kIntakeSwingFlightEntityId;
}

class PlayerContactListener final : public JPH::ContactListener {
public:
    void begin_tick() noexcept {
        lock();
        sample_ = {};
        unlock();
    }

    [[nodiscard]] SupportSample sample() const noexcept {
        lock();
        const SupportSample result = sample_;
        unlock();
        return result;
    }

    // AS-003: the entity on the player's carry point, 0 for none. The player
    // and what it holds share the carry point; contact between them is never
    // meaningful, only the constraint relates them.
    void set_carried_entity(const std::uint64_t entity) noexcept {
        carried_entity_.store(entity, std::memory_order_relaxed);
    }

    // AS-002: the deployed swing flight's own foot lands flush on top of the
    // (frozen, AS-001) handoff deck by design -- deployed foot at
    // (-6.000, 24.1872), exactly the deck's own centre and tread-surface
    // height (see kLegalFortyDeployedThetaRadians's comment). But the
    // flight's length and its 8.000 m rise together force theta_deployed to
    // 60 deg (cos 60 = rise / L = 8/16 = 0.5), so the final third of the
    // sweep -- theta in [48.8 deg, 60 deg], confirmed by a rotated-box vs
    // AABB separating-axis check -- drags the flight's own solid body
    // through the deck's near corner: up to 0.36 m of overlap, 0.156 m of
    // it still present at the final hinge-limit rest pose itself. No
    // AS-002-owned knob (hinge position, flight length, deployed angle) can
    // be retuned to clear this without unpicking the rope-crossing,
    // mid-landing-gap, or T_crit statics numbers the plan derives from this
    // exact geometry, and the deck itself is out of scope to touch. The
    // plan's own design note already settles what the deck's role should
    // be: "deployed foot must not bear: the stop is the hinge limit, not
    // the deck" -- it was never meant to be a physical obstacle to the
    // flight, so excluding this one body pair from collision is fidelity to
    // that intent, not a workaround for it. Every other pair keeps ordinary
    // collision, flight vs the player included, so a player caught in the
    // sweep is still struck and shoved exactly as the plan's own "standing
    // in the sweep when it deploys" note requires.
    [[nodiscard]] JPH::ValidateResult OnContactValidate(
        const JPH::Body &first, const JPH::Body &second, JPH::RVec3Arg,
        const JPH::CollideShapeResult &) override {
        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        using Sim = scraperx::sim::Simulation;
        const bool is_swing_flight_vs_handoff_deck =
            (first_entity == Sim::kIntakeSwingFlightEntityId &&
             second_entity == Sim::kIntakeHandoffEntityId) ||
            (first_entity == Sim::kIntakeHandoffEntityId &&
             second_entity == Sim::kIntakeSwingFlightEntityId);
        // Same reasoning, second body pair: the flight's own cross-section
        // is coincident with its hinge anchor at every sweep angle by
        // construction, so contact between them is never meaningful -- see
        // kIntakeSwingAnchorEntityId's own comment at the anchor's build
        // site for the diagnostic trace that found this.
        const bool is_swing_flight_vs_hinge_anchor =
            (first_entity == Sim::kIntakeSwingFlightEntityId &&
             second_entity == Sim::kIntakeSwingAnchorEntityId) ||
            (first_entity == Sim::kIntakeSwingAnchorEntityId &&
             second_entity == Sim::kIntakeSwingFlightEntityId);
        const std::uint64_t carried = carried_entity_.load(std::memory_order_relaxed);
        const bool is_player_vs_carried =
            carried != 0 &&
            ((first_entity == Sim::kPlayerEntityId && second_entity == carried) ||
             (first_entity == carried && second_entity == Sim::kPlayerEntityId));
        return (is_swing_flight_vs_handoff_deck || is_swing_flight_vs_hinge_anchor ||
                is_player_vs_carried)
                   ? JPH::ValidateResult::RejectAllContactsForThisBodyPair
                   : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body &first,
                        const JPH::Body &second,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &) override {
        observe_support(first, second, manifold);
    }

    void OnContactPersisted(const JPH::Body &first,
                            const JPH::Body &second,
                            const JPH::ContactManifold &manifold,
                            JPH::ContactSettings &) override {
        observe_support(first, second, manifold);
    }

private:
    [[nodiscard]] static int support_rank(const std::uint64_t entity_id) noexcept {
        if (entity_id == 0 || entity_id == scraperx::sim::Simulation::kPlayerEntityId) {
            return 0;
        }
        return entity_is_moving_support(entity_id) ? 2 : 1;
    }

    void observe_support(const JPH::Body &first,
                         const JPH::Body &second,
                         const JPH::ContactManifold &manifold) noexcept {
        if (manifold.mRelativeContactPointsOn1.size() == 0 ||
            manifold.mRelativeContactPointsOn2.size() == 0) {
            return;
        }

        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        std::uint64_t support_entity = 0;
        float support_normal_y = 0.0F;
        JPH::RVec3 support_contact_point{};
        const JPH::Body *support_body = nullptr;

        if (first_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = second_entity;
            support_normal_y = -manifold.mWorldSpaceNormal.GetY();
            support_contact_point = manifold.GetWorldSpaceContactPointOn2(0);
            support_body = &second;
        } else if (second_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = first_entity;
            support_normal_y = manifold.mWorldSpaceNormal.GetY();
            support_contact_point = manifold.GetWorldSpaceContactPointOn1(0);
            support_body = &first;
        }

        // WO-013: a sensor body (the wet grate) still produces a full contact
        // manifold -- Jolt's own doc comment is explicit that sensors "will
        // receive collision callbacks, but will not cause any collision
        // responses" -- so without this check a wet grate would read as
        // real support from geometry alone, even though no physical force
        // is actually holding the player up (they are in freefall through
        // it). A sensor is never a valid support.
        if (support_body == nullptr || support_body->IsSensor() ||
            support_normal_y < kSupportNormalThreshold || support_rank(support_entity) == 0) {
            return;
        }

        const JPH::Vec3 point_velocity = support_body->GetPointVelocity(support_contact_point);
        const SupportSample candidate{
            true,
            support_entity,
            {support_contact_point.GetX(), support_contact_point.GetY(), support_contact_point.GetZ()},
            {point_velocity.GetX(), point_velocity.GetY(), point_velocity.GetZ()},
            support_normal_y,
        };

        lock();
        const int current_rank = support_rank(sample_.entity_id);
        const int candidate_rank = support_rank(candidate.entity_id);
        if (!sample_.grounded || candidate_rank > current_rank ||
            (candidate_rank == current_rank && candidate.normal_y > sample_.normal_y)) {
            sample_ = candidate;
        }
        unlock();
    }

    void lock() const noexcept {
        while (lock_.test_and_set(std::memory_order_acquire)) {
        }
    }

    void unlock() const noexcept {
        lock_.clear(std::memory_order_release);
    }

    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    SupportSample sample_{};
    std::atomic<std::uint64_t> carried_entity_{0};
};

[[nodiscard]] JPH::RVec3 spawn_position(const scraperx::sim::InitialSpawn spawn) noexcept {
    switch (spawn) {
    case scraperx::sim::InitialSpawn::StaticDeck:
        return {0.0, 3.0, -8.0};
    case scraperx::sim::InitialSpawn::RotatingSupport:
        return {-6.5, 3.0, 0.0};
    case scraperx::sim::InitialSpawn::VaultApproach:
        return {1.2, 1.2, -6.0};
    case scraperx::sim::InitialSpawn::MantleApproach:
        return {7.9, 1.2, -6.0};
    case scraperx::sim::InitialSpawn::HangApproach:
        return {7.6, 4.2, 4.0};
    case scraperx::sim::InitialSpawn::MovingLedgeApproach:
        return {6.1, 4.2, 12.0};
    case scraperx::sim::InitialSpawn::BlockedLedgeApproach:
        return {-3.6, 1.2, -8.0};
    case scraperx::sim::InitialSpawn::HighDrop:
        // ~61 m above the static deck: unmitigated free fall reaches
        // sqrt(2*g*61) ~= 34.6 m/s, well past kLethalImpactSpeedMps.
        return {0.0, 62.0, -8.0};
    case scraperx::sim::InitialSpawn::SurvivableDrop:
        // ~12 m above the static deck: unmitigated free fall reaches
        // sqrt(2*g*12) ~= 15.3 m/s, under kLethalImpactSpeedMps with margin.
        return {0.0, 13.0, -8.0};
    case scraperx::sim::InitialSpawn::CatwalkTreadle:
        // Above the outboard half of the treadle plate, where a body has real
        // leverage on the hinge.
        return {15.3, 10.3, -106.0};
    case scraperx::sim::InitialSpawn::KernelJibStation:
        return {kJibStationX, 1.2, kJibStationZ};
    case scraperx::sim::InitialSpawn::KernelCrateTop:
        // Offset from the crate's centre so the player doesn't spawn inside
        // the hook, which hangs directly above the centre via the pin link.
        return {static_cast<double>(kJibMastX + kJibBoomLength) + 0.4, 2.4,
                static_cast<double>(kJibMastZ)};
    case scraperx::sim::InitialSpawn::KernelNeedleStation:
        // On the approach pier top, not at grade: see the station-siting
        // note by kNeedleStationX above.
        return {static_cast<double>(kNeedleStationX), static_cast<double>(kNeedlePierTopY) + 1.0,
                static_cast<double>(kNeedleStationZ)};
    case scraperx::sim::InitialSpawn::KernelSumpStation:
        // On the fixed approach decking, not the grate -- see the station-
        // siting note by kSumpStationX above.
        return {static_cast<double>(kSumpStationX), static_cast<double>(kSumpPlatformTopY) + 1.0,
                static_cast<double>(kSumpStationZ)};
    case scraperx::sim::InitialSpawn::IntakePendant:
        // Standing on the belt catwalk at CAP-PENDANT, inside the station
        // radius, so the freight sequence is driven without a climb.
        return {static_cast<double>(kIntakePendantX),
                static_cast<double>(kIntakePendantTopY) + 1.0,
                static_cast<double>(kIntakePendantZ)};
    case scraperx::sim::InitialSpawn::IntakeThroat:
        // On the apron 3 m outside the MOD-DOG-A throat, facing into the bay.
        return {static_cast<double>(kIntakeBayCenterX), 1.2,
                static_cast<double>(kIntakeBayFrontZ) + 3.0};
    case scraperx::sim::InitialSpawn::IntakeSkinFoot:
        // On the apron just south of MOD-SKIN-LADDER-S's lowest rung.
        return {static_cast<double>(kIntakeSkinCenterX), 1.2,
                static_cast<double>(kIntakeSkinFootZ + kIntakeSkinRungHalfZ) + 1.2};
    case scraperx::sim::InitialSpawn::IntakeHandoffDeck:
        // On the +24.1872 m handoff deck itself, near its east edge, facing
        // MOD-STAIR-A-SWING's stowed footprint.
        return {-3.0, static_cast<double>(kIntakeHandoffY) + 1.0872, -112.5};
    case scraperx::sim::InitialSpawn::Hook5Cage:
        // On the cage floor under the roof hatch, where the drop lands.
        return {10.30, 1.0, static_cast<double>(kHook5MinZ) + 2.2};
    case scraperx::sim::InitialSpawn::Hook5Apron:
        return {static_cast<double>(kHook5MaxX) + 1.5, 1.0, static_cast<double>(kHook5MinZ) - 3.0};
    case scraperx::sim::InitialSpawn::StairTop:
        // The 154 m deck's north band, north of Stage A's cage.
        return {-10.5, 155.0, -128.2};
    case scraperx::sim::InitialSpawn::WellBCage:
        // Inside B's cage, west of its middle.
        return {-7.6, 177.2, -131.2};
    case scraperx::sim::InitialSpawn::WellCPlatform:
        // On C's platform, west of its middle.
        return {-4.3, 199.2, -132.2};
    case scraperx::sim::InitialSpawn::Ring176East:
        // On the 176 ring, 1 m in from its inner edge.
        return {9.5, 177.2, -127.9};
    case scraperx::sim::InitialSpawn::Ring198East:
        return {11.0, 199.2, -127.5};
    case scraperx::sim::InitialSpawn::Ring220North:
        // On the 220 ring west of the spool, as a rider off C's platform.
        return {4.0, 221.2, -129.2};
    case scraperx::sim::InitialSpawn::WetECab:
        // In E's cab on its bottom stop, south of its door.
        return {13.0, 257.2, -137.2};
    case scraperx::sim::InitialSpawn::WetFPlatform:
        // On F's platform, east of the hose.
        return {13.4, 299.2, -139.9};
    case scraperx::sim::InitialSpawn::PlateTop:
        // On TP-340 north of F's hole, as a rider off F's platform.
        return {12.0, 341.2, -136.8};
    case scraperx::sim::InitialSpawn::Ring374West:
        // On the 374 ring's west band by H's gangway.
        return {-14.5, 375.2, -145.6};
    case scraperx::sim::InitialSpawn::ShopICage:
        // On I's cage, west of its shackle.
        return {-7.3, 419.2, -145.6};
    case scraperx::sim::InitialSpawn::Ring484North:
        // On the 484 ring's north band west of J's traveler.
        return {-3.0, 485.2, -139.6};
    case scraperx::sim::InitialSpawn::CraneKCage:
        // On K's cage, west of its shackle.
        return {-13.2, 529.2, -155.0};
    case scraperx::sim::InitialSpawn::CraneLCab:
        // On L's cab, west of its middle.
        return {-3.6, 573.2, -160.1};
    case scraperx::sim::InitialSpawn::Plate640:
        // On TP-640, north of the service cage, the way StairTop stands north of A's.
        return {-8.0, 641.25, -144.8};
    case scraperx::sim::InitialSpawn::Plate640Dismount:
        // Where L's cab steps the rider off onto TP-640.
        return {-3.0, 641.25, -156.5};
    case scraperx::sim::InitialSpawn::MachineYard:
        return {31.2, 5.0, -96.0};
    case scraperx::sim::InitialSpawn::LiftPlatform:
        return {16.4, 2.0, -100.0};
    case scraperx::sim::InitialSpawn::ExteriorGrade:
        // At grade, outdoors, 120 m short of the tower face: far enough that the
        // mass reads as something you approach, close enough that its lower
        // third already fills the frame.
        return {6.0, 1.2, -25.0};
    case scraperx::sim::InitialSpawn::TranslatingSupport:
        return {0.0, 3.0, 8.0};
    default:
        return {6.0, 1.2, -25.0};
    }
}

void approach_relative_horizontal_velocity(JPH::Vec3 &world_velocity,
                                           const JPH::Vec3 reference_velocity,
                                           const double move_input_x,
                                           const double move_input_z,
                                           const float acceleration,
                                           const float delta_seconds,
                                           const float full_speed = kPlayerMaximumRelativeSpeed) noexcept {
    float relative_x = world_velocity.GetX() - reference_velocity.GetX();
    float relative_z = world_velocity.GetZ() - reference_velocity.GetZ();
    const float target_x = static_cast<float>(move_input_x) * full_speed;
    const float target_z = static_cast<float>(move_input_z) * full_speed;
    float delta_x = target_x - relative_x;
    float delta_z = target_z - relative_z;
    const float delta_length = std::sqrt(delta_x * delta_x + delta_z * delta_z);
    const float maximum_delta = acceleration * delta_seconds;

    if (delta_length > maximum_delta && delta_length > 0.0F) {
        const float scale = maximum_delta / delta_length;
        delta_x *= scale;
        delta_z *= scale;
    }

    relative_x += delta_x;
    relative_z += delta_z;
    world_velocity.SetX(reference_velocity.GetX() + relative_x);
    world_velocity.SetZ(reference_velocity.GetZ() + relative_z);
}

[[nodiscard]] float smoothstep(const float edge0, const float edge1, const float value) noexcept {
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

// Cubic ease over [0, 1] that leaves at `entry_slope` (in units of the mean
// rate) and arrives at rest: a Hermite segment, monotonic for slopes in
// [0, 3], so a body entering it at speed carries that speed in.
[[nodiscard]] float ease_to_rest(const float value, const float entry_slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float slope = std::clamp(entry_slope, 0.0F, 3.0F);
    return slope * t * (1.0F - t) * (1.0F - t) + t * t * (3.0F - 2.0F * t);
}

// WO-008 checkpoint capture: full rigid-body state for one dynamic machine
// member. Kinematic bodies (supports, hoist scoop) deliberately have no
// equivalent -- they are pure functions of the authoritative tick counter
// and are always correct without restoration.
struct BodyCheckpoint final {
    JPH::RVec3 position{JPH::RVec3::sZero()};
    JPH::Quat rotation = JPH::Quat::sIdentity();
    JPH::Vec3 linear_velocity{JPH::Vec3::sZero()};
    JPH::Vec3 angular_velocity{JPH::Vec3::sZero()};
};

[[nodiscard]] BodyCheckpoint capture_body(const JPH::BodyInterface &bodies,
                                          const JPH::BodyID id) noexcept {
    return {bodies.GetPosition(id), bodies.GetRotation(id), bodies.GetLinearVelocity(id),
            bodies.GetAngularVelocity(id)};
}

void restore_body(JPH::BodyInterface &bodies, const JPH::BodyID id,
                  const BodyCheckpoint &checkpoint) noexcept {
    bodies.SetPositionAndRotation(id, checkpoint.position, checkpoint.rotation,
                                  JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(id, checkpoint.linear_velocity,
                                       checkpoint.angular_velocity);
}

[[nodiscard]] scraperx::sim::Vector3 to_vector3(const JPH::RVec3 value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

[[nodiscard]] scraperx::sim::Quaternion to_quaternion(const JPH::Quat value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}

// Result of one geometry probe against the authoritative Jolt world. Every
// field is derived from an actual cast or clearance query.
struct LedgeProbe final {
    bool valid = false;
    JPH::BodyID ledge_body;
    std::uint64_t ledge_entity_id = 0;
    JPH::RVec3 wall_point{};
    JPH::RVec3 ledge_point{};
    JPH::RVec3 landing_centre{};
    JPH::BodyID landing_body;
    std::uint64_t landing_entity_id = 0;
    float rise = 0.0F;
};

// Step 2 movement. A hold a hand has closed round: the point on the member's
// axis, on the body the member belongs to.
struct Grip final {
    bool valid = false;
    JPH::BodyID body;
    std::uint64_t entity_id = 0;
    JPH::RVec3 point{JPH::RVec3::sZero()};
};

// A ledge's lip, faced along a hang's normal: the top just inside the edge,
// where a hanging body holds below it, and where a climb up it lands.
struct Lip final {
    bool valid = false;
    JPH::BodyID body;
    std::uint64_t entity_id = 0;
    JPH::RVec3 ledge{JPH::RVec3::sZero()};
    JPH::RVec3 hold{JPH::RVec3::sZero()};
    JPH::RVec3 landing{JPH::RVec3::sZero()};
};

// One box of a body's shape, in world space.
struct LeafBox final {
    JPH::RVec3 centre{JPH::RVec3::sZero()};
    JPH::Vec3 axes[3]{};
    JPH::Vec3 half{JPH::Vec3::sZero()};
};

// A member a hand can close round (see kGripMaxSection): its long axis in
// `long_axis`.
[[nodiscard]] bool box_is_hold(const LeafBox &box, std::uint32_t &long_axis) noexcept {
    long_axis = 0;
    for (std::uint32_t axis = 1; axis < 3; ++axis) {
        if (box.half[axis] > box.half[long_axis]) {
            long_axis = axis;
        }
    }
    if (2.0F * box.half[long_axis] < kGripMinLength) {
        return false;
    }
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        if (axis != long_axis && 2.0F * box.half[axis] > kGripMaxSection) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool leaf_box(const JPH::Body &body, const JPH::SubShapeID &sub_shape,
                            LeafBox &out) noexcept {
    JPH::SubShapeID remainder;
    const JPH::TransformedShape leaf = body.GetShape()->GetSubShapeTransformedShape(
        sub_shape, body.GetCenterOfMassPosition(), body.GetRotation(), JPH::Vec3::sReplicate(1.0F),
        remainder);
    if (leaf.mShape == nullptr || leaf.mShape->GetSubType() != JPH::EShapeSubType::Box) {
        return false;
    }
    out.half = static_cast<const JPH::BoxShape *>(leaf.mShape.GetPtr())->GetHalfExtent();
    const JPH::RMat44 frame = leaf.GetCenterOfMassTransform();
    out.centre = frame.GetTranslation();
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        out.axes[axis] = frame.GetColumn3(axis).Normalized();
    }
    return true;
}

} // namespace

namespace scraperx::sim {

class Simulation::PhysicsWorld final {
public:
    explicit PhysicsWorld(const InitialSpawn initial_spawn)
        : temp_allocator_(8U * 1024U * 1024U),
          job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1) {
        // Raised from 1024: the world's solid dressing adds ~1 600 static
        // bodies (world_solids.inc) on top of the ~300 the kernel owns.
        physics_system_.Init(4096,
                             0,
                             2048,
                             1024,
                             broadphase_layer_interface_,
                             object_vs_broadphase_filter_,
                             object_layer_pair_filter_);
        physics_system_.SetContactListener(&contact_listener_);

        auto &bodies = physics_system_.GetBodyInterface();

        // Grade: the yard the player is born on, outdoors, wide enough to walk
        // the tower approach and to carry the whole plant.
        deck_id_ = add_box(bodies,
                           JPH::Vec3(240.0F, 0.5F, 240.0F),
                           JPH::RVec3(0.0, -0.5, -60.0),
                           JPH::EMotionType::Static,
                           object_layers::kStatic,
                           0.6F,
                           Simulation::kStaticDeckEntityId);

        // The neighbouring shaft: 1.6 km of unclimbable mass standing on
        // grade, set well back from the climbable frame. Overhead and close,
        // it read as a black void hanging over the stack; begun 159 m up
        // with nothing under it, it read as a skyscraper floating in the air.
        const float mass_half_height = static_cast<float>(Simulation::kTowerHeightMeters * 0.5);
        tower_id_ = add_box(bodies,
                            JPH::Vec3(46.0F, mass_half_height, 40.0F),
                            JPH::RVec3(-30.0, mass_half_height, -330.0),
                            JPH::EMotionType::Static,
                            object_layers::kStatic,
                            0.8F,
                            Simulation::kTowerEntityId);

        translating_support_id_ = add_box(bodies,
                                          JPH::Vec3(2.75F, 0.25F, 2.75F),
                                          JPH::RVec3(0.0, 0.25, 8.0),
                                          JPH::EMotionType::Kinematic,
                                          object_layers::kMoving,
                                          0.8F,
                                          Simulation::kTranslatingSupportEntityId);

        rotating_support_id_ = add_box(bodies,
                                       JPH::Vec3(3.0F, 0.25F, 3.0F),
                                       JPH::RVec3(-8.0, 0.25, 0.0),
                                       JPH::EMotionType::Kinematic,
                                       object_layers::kMoving,
                                       0.8F,
                                       Simulation::kRotatingSupportEntityId);

        vault_rail_id_ = add_box(bodies,
                                 JPH::Vec3(0.22F, 0.475F, 2.5F),
                                 JPH::RVec3(5.0, 0.475, -6.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kVaultRailEntityId);

        mantle_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 0.775F, 2.0F),
                                   JPH::RVec3(11.0, 0.775, -6.0),
                                   JPH::EMotionType::Static,
                                   object_layers::kStatic,
                                   0.7F,
                                   Simulation::kMantleLedgeEntityId);

        hang_ledge_id_ = add_box(bodies,
                                 JPH::Vec3(2.5F, 1.8F, 2.5F),
                                 JPH::RVec3(11.0, 1.8, 4.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kHangLedgeEntityId);

        moving_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 1.8F, 2.0F),
                                   JPH::RVec3(9.0, 1.8, kMovingLedgeCenterZ),
                                   JPH::EMotionType::Kinematic,
                                   object_layers::kMoving,
                                   0.8F,
                                   Simulation::kMovingLedgeEntityId);

        blocked_ledge_id_ = add_box(bodies,
                                    JPH::Vec3(1.5F, 0.775F, 1.5F),
                                    JPH::RVec3(-6.0, 0.775, -8.0),
                                    JPH::EMotionType::Static,
                                    object_layers::kStatic,
                                    0.7F,
                                    Simulation::kBlockedLedgeEntityId);

        blocked_ledge_canopy_id_ = add_box(bodies,
                                           JPH::Vec3(2.2F, 0.15F, 2.2F),
                                           JPH::RVec3(-6.0, 2.7, -8.0),
                                           JPH::EMotionType::Static,
                                           object_layers::kStatic,
                                           0.7F,
                                           Simulation::kBlockedLedgeCanopyEntityId);

        {
            const auto crawl_part = [this, &bodies](const JPH::Vec3 half, const JPH::RVec3 at) {
                machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                                  object_layers::kStatic, 0.7F,
                                                  Simulation::kCrawlBeamEntityId));
            };
            const float beam_top = kCrawlBeamUndersideY + 2.0F * kCrawlBeamHalfY;
            crawl_part(JPH::Vec3(kCrawlLaneHalfX, kCrawlBeamHalfY, kCrawlBeamHalfZ),
                       JPH::RVec3(kCrawlLaneCenterX, kCrawlBeamUndersideY + kCrawlBeamHalfY,
                                  kCrawlBeamZ));
            for (const float side : {-1.0F, 1.0F}) {
                crawl_part(JPH::Vec3(kCrawlPostHalf, beam_top * 0.5F, kCrawlPostHalf),
                           JPH::RVec3(kCrawlLaneCenterX + side * (kCrawlLaneHalfX + kCrawlPostHalf),
                                      beam_top * 0.5F, kCrawlBeamZ));
            }
        }

        build_stack(bodies);
        build_machine(bodies);
        build_kernel_jib(bodies);
        build_kernel_needle(bodies);
        build_kernel_sump(bodies);
        build_intake_rise(bodies);
        build_legal_forty(bodies);
        build_world_solids(bodies);

        player_shape_ = new JPH::CapsuleShape(0.55F, kPlayerRadius);
        player_crouch_shape_ =
            new JPH::CapsuleShape(kPlayerCrouchHalfHeight - kPlayerRadius, kPlayerRadius);
        grip_region_shape_ = new JPH::BoxShape(kGripSearchHalf);
        // A climber's moves are swept with a body 5 cm slimmer than the
        // capsule: a body on its holds already touches the rungs it climbs.
        climb_sweep_shape_ =
            new JPH::CapsuleShape(0.55F + kPlayerRadius - kClimbSweepRadius, kClimbSweepRadius);
        JPH::BodyCreationSettings player_settings(player_shape_,
                                                  spawn_position(initial_spawn),
                                                  JPH::Quat::sIdentity(),
                                                  JPH::EMotionType::Dynamic,
                                                  object_layers::kMoving);
        player_settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
                                       JPH::EAllowedDOFs::TranslationY |
                                       JPH::EAllowedDOFs::TranslationZ;
        player_settings.mAllowSleeping = false;
        player_settings.mFriction = 0.0F;
        player_settings.mLinearDamping = 0.0F;
        player_settings.mUserData = Simulation::kPlayerEntityId;
        player_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        player_settings.mMassPropertiesOverride.mMass = kPlayerMassKg;
        player_id_ = bodies.CreateAndAddBody(player_settings, JPH::EActivation::Activate);

        // Built last, so every body before it keeps the id it had: the ascent
        // routes proven against them are contact-order sensitive.
        build_hook5_rack(bodies);

        // AS-006: the mechanism ascent's bands, built last for the same
        // reason: every body before them keeps its id.
        kit_ = std::make_unique<scraperx::sim::kit::Kit>(physics_system_, object_layers::kStatic,
                                                        object_layers::kMoving);
        scraperx::sim::bands::build_counterweight_well(*kit_, well_);
        scraperx::sim::bands::build_wet_isolation(*kit_, wet_);
        scraperx::sim::bands::build_plate_shop(*kit_, shop_);
        scraperx::sim::bands::build_facade_crane(*kit_, crane_);
        scraperx::sim::bands::build_midstack_service(*kit_, service_);

        physics_system_.OptimizeBroadPhase();

        checkpoint_position_ = JPH::RVec3(0.0, 0.9, 0.0);
        commit_machine_checkpoint(bodies);

        read_state();
    }

    ~PhysicsWorld() {
        physics_system_.SetContactListener(nullptr);
        // Dynamically-owned pins (track_for_teardown=false) are never in
        // machine_constraints_, so the loop below cannot reach them -- remove
        // whichever of them are currently seated before it runs.
        if (needle_pin_approach_ != nullptr) {
            physics_system_.RemoveConstraint(needle_pin_approach_);
            needle_pin_approach_ = nullptr;
        }
        if (needle_pin_far_ != nullptr) {
            physics_system_.RemoveConstraint(needle_pin_far_);
            needle_pin_far_ = nullptr;
        }
        if (intake_sling_pin_ != nullptr) {
            physics_system_.RemoveConstraint(intake_sling_pin_);
            intake_sling_pin_ = nullptr;
        }
        if (carry_constraint_ != nullptr) {
            physics_system_.RemoveConstraint(carry_constraint_);
            carry_constraint_ = nullptr;
        }
        kit_.reset();
        for (JPH::Ref<JPH::TwoBodyConstraint> &constraint : machine_constraints_) {
            if (constraint != nullptr) {
                physics_system_.RemoveConstraint(constraint);
            }
        }
        machine_constraints_.clear();
        tipper_hinge_ = nullptr;
        valve_hinge_ = nullptr;
        treadle_hinge_ = nullptr;
        jib_slew_hinge_ = nullptr;
        jib_hoist_slider_ = nullptr;
        needle_hoist_slider_ = nullptr;
        intake_swing_hinge_ = nullptr;
        hook5_door_hinge_ = nullptr;
        auto &bodies = physics_system_.GetBodyInterface();
        for (auto it = machine_bodies_.rbegin(); it != machine_bodies_.rend(); ++it) {
            remove_and_destroy(bodies, *it);
        }
        machine_bodies_.clear();
        remove_and_destroy(bodies, tower_id_);
        remove_and_destroy(bodies, player_id_);
        remove_and_destroy(bodies, blocked_ledge_canopy_id_);
        remove_and_destroy(bodies, blocked_ledge_id_);
        remove_and_destroy(bodies, moving_ledge_id_);
        remove_and_destroy(bodies, hang_ledge_id_);
        remove_and_destroy(bodies, mantle_ledge_id_);
        remove_and_destroy(bodies, vault_rail_id_);
        remove_and_destroy(bodies, rotating_support_id_);
        remove_and_destroy(bodies, translating_support_id_);
        remove_and_destroy(bodies, deck_id_);
    }

    struct StepCommands final {
        double move_input_x = 0.0;
        double move_input_z = 0.0;
        double facing_x = 0.0;
        double facing_z = 0.0;
        bool jump_requested = false;
        bool traversal_requested = false;
        bool release_requested = false;
        bool crouch_held = false;
        bool sprint_held = false;
        bool pick_up_requested = false;
        bool set_down_requested = false;
        bool rig_requested = false;
        bool parachute_toggle_requested = false;
        double jib_slew_input = 0.0;
        double jib_hoist_input = 0.0;
        double needle_hoist_input = 0.0;
        bool valve_toggle_requested = false;
        double intake_slew_input = 0.0;
        double intake_hoist_input = 0.0;
        bool intake_sling_release_requested = false;
        bool intake_sling_attach_requested = false;
    };

    void step(const StepCommands &commands,
              const float delta_seconds,
              const double next_time_seconds) noexcept {
        // WO-008: captured before anything this tick can change grounded_, so
        // it means exactly "was the player standing on something one tick ago."
        const bool was_grounded_before_tick = grounded_;

        auto &bodies = physics_system_.GetBodyInterface();
        update_support_motion(bodies, delta_seconds, next_time_seconds);
        update_scoop(bodies, delta_seconds, next_time_seconds);
        update_plant(bodies, delta_seconds);
        update_jib(bodies, commands.jib_slew_input, commands.jib_hoist_input);
        update_needle(bodies, commands.needle_hoist_input);
        update_sump(bodies, delta_seconds, commands.valve_toggle_requested);
        update_intake(bodies, commands.intake_slew_input, commands.intake_hoist_input);
        update_legal_forty(bodies, commands.intake_sling_release_requested,
                           commands.intake_sling_attach_requested);

        // A toggle while airborne only: deploying/retracting on the ground is
        // meaningless and would let a grounded button-mash pre-arm the canopy.
        if (commands.parachute_toggle_requested && !was_grounded_before_tick) {
            parachute_deployed_ = !parachute_deployed_;
        }

        if (regrab_lockout_ticks_ > 0) {
            --regrab_lockout_ticks_;
        }
        facing_ = normalized_horizontal(commands.facing_x, commands.facing_z);

        update_crouch(bodies, commands);
        update_rig(bodies, commands);
        update_carry(bodies, commands, delta_seconds);
        apply_traversal_commands(bodies, commands);
        if (traversal_state_ == TraversalState::Climbing) {
            update_climb(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Hanging) {
            update_shimmy(bodies, commands, delta_seconds);
        }

        bool jump_started = false;
        if (traversal_state_ == TraversalState::None) {
            jump_started = apply_locomotion(bodies, commands, delta_seconds);
            if (!jump_started) {
                try_begin_hang(bodies, commands);
            }
            if (!was_grounded_before_tick) {
                apply_parachute_drag(bodies, delta_seconds);
                // Distinct from fall_peak_speed_mps_ (a running max, telemetry
                // only): this is overwritten every tick, so it always holds
                // exactly the velocity the body carries into this tick's
                // contact resolution -- what "impact speed" has to mean for
                // late deceleration (a well-timed parachute) to matter.
                const float vertical_speed = bodies.GetLinearVelocity(player_id_).GetY();
                pre_contact_fall_speed_mps_ = std::max(0.0F, -vertical_speed);
                fall_peak_speed_mps_ =
                    std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
            }
        }

        if (traversal_state_ != TraversalState::None) {
            drive_traversal(bodies, delta_seconds);
            load_hold(bodies);
        }

        kit_->pre_step(delta_seconds);
        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);
        kit_->post_step(delta_seconds);

        SupportSample support = contact_listener_.sample();
        if (jump_started || traversal_state_ != TraversalState::None) {
            support = {};
        }
        support_sample_ = support;
        grounded_ = support.grounded;
        support_entity_id_ = support.entity_id;

        // WO-008: a genuine (non-traversal, non-jump) landing is the one
        // moment fall consequence is resolved. Traversal-completion landings
        // never reach here with a real fall velocity -- complete_traversal
        // always sets a support-relative landing velocity first -- so no
        // separate traversal exemption is needed.
        bool died_this_tick = false;
        if (grounded_ && !was_grounded_before_tick && traversal_state_ == TraversalState::None &&
            !jump_started) {
            last_impact_speed_mps_ = pre_contact_fall_speed_mps_;
            if (last_impact_speed_mps_ > kLethalImpactSpeedMps) {
                restore_from_checkpoint(bodies);
                died_this_tick = true;
            }
        }
        if (!died_this_tick && grounded_ && traversal_state_ == TraversalState::None &&
            footing_is_firm(bodies)) {
            commit_checkpoint(bodies);
        }
        if (grounded_) {
            fall_peak_speed_mps_ = 0.0F;
            parachute_deployed_ = false;
        }

        if (traversal_state_ != TraversalState::None) {
            resolve_traversal_outcome(bodies);
        }

        update_affordance(bodies);
        read_state();
    }

    [[nodiscard]] const Snapshot &state() const noexcept {
        return state_;
    }

    [[nodiscard]] bool traversal_committed() const noexcept {
        return traversal_state_ != TraversalState::None;
    }

    [[nodiscard]] const scraperx::sim::kit::Kit &kit() const noexcept {
        return *kit_;
    }

    [[nodiscard]] const scraperx::sim::bands::WetIsolation &wet() const noexcept {
        return wet_;
    }

    [[nodiscard]] const scraperx::sim::bands::PlateShop &shop() const noexcept {
        return shop_;
    }

    [[nodiscard]] const scraperx::sim::bands::FacadeCrane &crane() const noexcept {
        return crane_;
    }

    [[nodiscard]] const scraperx::sim::bands::MidstackService &service() const noexcept {
        return service_;
    }

    void set_feed_enabled(const bool enabled) noexcept {
        steam_plant_.set_feed_enabled(enabled);
    }

private:
    static void remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
        bodies.RemoveBody(body_id);
        bodies.DestroyBody(body_id);
    }

    static JPH::Body *add_shape_body(JPH::BodyInterface &bodies,
                                     const JPH::Shape *shape,
                                     const JPH::RVec3 position,
                                     const JPH::Quat rotation,
                                     const JPH::EMotionType motion_type,
                                     const JPH::ObjectLayer layer,
                                     const float friction,
                                     const std::uint64_t entity_id,
                                     const float mass_kg) {
        JPH::BodyCreationSettings settings(shape, position, rotation, motion_type, layer);
        settings.mFriction = friction;
        settings.mUserData = entity_id;
        settings.mAllowSleeping = false;
        if (mass_kg > 0.0F) {
            settings.mOverrideMassProperties =
                JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = mass_kg;
        }
        JPH::Body *body = bodies.CreateBody(settings);
        const JPH::EActivation activation = motion_type == JPH::EMotionType::Static
                                                ? JPH::EActivation::DontActivate
                                                : JPH::EActivation::Activate;
        bodies.AddBody(body->GetID(), activation);
        return body;
    }

    static JPH::BodyID add_box(JPH::BodyInterface &bodies,
                               const JPH::Vec3 half_extent,
                               const JPH::RVec3 position,
                               const JPH::EMotionType motion_type,
                               const JPH::ObjectLayer layer,
                               const float friction,
                               const std::uint64_t entity_id,
                               const JPH::Quat rotation = JPH::Quat::sIdentity(),
                               const float mass_kg = 0.0F) {
        return add_shape_body(bodies,
                              new JPH::BoxShape(half_extent),
                              position,
                              rotation,
                              motion_type,
                              layer,
                              friction,
                              entity_id,
                              mass_kg)
            ->GetID();
    }

    [[nodiscard]] static JPH::Vec3 normalized_horizontal(const double x, const double z) noexcept {
        const double length = std::hypot(x, z);
        if (!(length > 1.0e-6)) {
            return JPH::Vec3::sZero();
        }
        return JPH::Vec3(static_cast<float>(x / length), 0.0F, static_cast<float>(z / length));
    }

    [[nodiscard]] JPH::BodyID body_id_for_entity(const std::uint64_t entity_id) const noexcept {
        switch (entity_id) {
        case Simulation::kStaticDeckEntityId:
            return deck_id_;
        case Simulation::kTranslatingSupportEntityId:
            return translating_support_id_;
        case Simulation::kRotatingSupportEntityId:
            return rotating_support_id_;
        case Simulation::kVaultRailEntityId:
            return vault_rail_id_;
        case Simulation::kMantleLedgeEntityId:
            return mantle_ledge_id_;
        case Simulation::kHangLedgeEntityId:
            return hang_ledge_id_;
        case Simulation::kMovingLedgeEntityId:
            return moving_ledge_id_;
        case Simulation::kBlockedLedgeEntityId:
            return blocked_ledge_id_;
        case Simulation::kBlockedLedgeCanopyEntityId:
            return blocked_ledge_canopy_id_;
        case Simulation::kTowerEntityId:
            return tower_id_;
        case Simulation::kHoistScoopEntityId:
            return scoop_ids_[0];
        case Simulation::kBallastEntityId:
            return ballast_id_;
        case Simulation::kTipperEntityId:
            return tipper_id_;
        case Simulation::kValveLeverEntityId:
            return valve_lever_id_;
        case Simulation::kLiftPlatformEntityId:
            return lift_platform_id_;
        case Simulation::kCounterweightEntityId:
            return counterweight_id_;
        case Simulation::kNeedlePierApproachEntityId:
            return needle_pier_approach_id_;
        case Simulation::kNeedlePierFarEntityId:
            return needle_pier_far_id_;
        case Simulation::kNeedleBeamEntityId:
            return needle_beam_id_;
        case Simulation::kSumpGrateEntityId:
            return sump_grate_id_;
        default:
            return {};
        }
    }

    // ---- coupled machine ------------------------------------------------
    //
    // One causal chain, all of it authoritative:
    //   hoist scoop lifts ballast -> tips it onto a chute -> ballast falls onto a
    //   hinged tipper -> tipper rotation drags a tension-only rope -> rope pulls a
    //   counterweighted valve lever -> lever angle sets a real orifice area ->
    //   orifice vents a finite pressure vessel into an actuator cylinder ->
    //   cylinder pressure pushes a piston -> piston lifts a counterweighted
    //   platform the player can stand on and ride.
    //
    // Nothing in the chain is scripted. Each link reads the previous link's
    // actual body state, so the player can enter it mid-cycle, block it, ride it,
    // or start it early by standing on the tipper.
    void build_machine(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };

        // Static plant structure.
        const JPH::BodyID machine_pylon = track(add_box(
            bodies, JPH::Vec3(0.55F, 1.45F, 1.4F), JPH::RVec3(kTipperHingeX, 1.45, kTipperZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kMachinePylonEntityId));
        const JPH::BodyID valve_pylon = track(add_box(
            bodies, JPH::Vec3(0.3F, 3.6F, 0.3F), JPH::RVec3(kValveHingeX, 3.6, -92.25),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kMachinePylonEntityId));
        track(add_box(bodies, JPH::Vec3(0.4F, 6.2F, 0.4F), JPH::RVec3(35.9, 6.2, kScoopZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kMachinePylonEntityId));
        track(add_box(bodies, JPH::Vec3(1.7F, 2.3F, 1.7F), JPH::RVec3(30.5, 2.3, -101.5),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.75F,
                      Simulation::kVesselShellEntityId));
        track(add_box(bodies, JPH::Vec3(0.9F, 0.625F, 0.8F), JPH::RVec3(27.0, 0.625, -94.05),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kMachinePylonEntityId));
        track(add_box(bodies, JPH::Vec3(0.9F, 1.25F, 0.8F), JPH::RVec3(28.3, 1.25, -94.05),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kMachinePylonEntityId));
        // The landing starts at the second step's east face (x = 29.2). It used
        // to start 0.7 m west of it, over the step: 1.0 m of headroom on the
        // step's top, a slot a standing capsule walked into and wedged.
        track(add_box(bodies, JPH::Vec3(1.0F, 0.15F, 0.8F), JPH::RVec3(30.2, 3.65, -94.05),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kMachinePylonEntityId));

        const JPH::BodyID lift_mast = track(add_box(
            bodies, JPH::Vec3(0.4F, 5.6F, 0.4F), JPH::RVec3(kLiftMastX, 5.6, kLiftZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kLiftMastEntityId));
        track(add_box(bodies, JPH::Vec3(2.6F, 0.14F, 9.0F), JPH::RVec3(kLiftPlatformX, 8.55, -112.0),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.7F,
                      Simulation::kCatwalkEntityId));

        // Return basin: a sloped real surface that walks the ballast back to the
        // hoist mouth under gravity and friction alone, with lane guards so it
        // cannot wander out of the machine.
        track(add_box(bodies, JPH::Vec3(1.2F, 0.14F, 1.5F), JPH::RVec3(31.82, 1.84, kScoopZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.30F,
                      Simulation::kCatchBasinEntityId,
                      JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -0.20F)));
        for (const float guard_z : {kScoopZ - 1.55F, kScoopZ + 1.55F}) {
            track(add_box(bodies, JPH::Vec3(1.4F, 0.45F, 0.12F),
                          JPH::RVec3(31.82, 2.20, guard_z), JPH::EMotionType::Static,
                          object_layers::kStatic, 0.4F, Simulation::kChuteEntityId));
        }

        // Hoist scoop: four kinematic panels driven by one rigid transform, open
        // on its -x face so a forward tilt discharges the ballast.
        scoop_local_[0] = JPH::Vec3(0.0F, -0.03F, 0.0F);
        scoop_local_[1] = JPH::Vec3(1.42F, 0.70F, 0.0F);
        scoop_local_[2] = JPH::Vec3(0.0F, 0.70F, -1.42F);
        scoop_local_[3] = JPH::Vec3(0.0F, 0.70F, 1.42F);
        const JPH::Vec3 scoop_half[4] = {
            JPH::Vec3(1.30F, 0.03F, 1.30F),
            JPH::Vec3(0.12F, 0.70F, 1.30F),
            JPH::Vec3(1.30F, 0.70F, 0.12F),
            JPH::Vec3(1.30F, 0.70F, 0.12F),
        };
        for (int i = 0; i < 4; ++i) {
            scoop_ids_[i] = track(add_box(
                bodies, scoop_half[i],
                JPH::RVec3(kScoopX + scoop_local_[i].GetX(),
                           kScoopBottomY + scoop_local_[i].GetY(),
                           kScoopZ + scoop_local_[i].GetZ()),
                JPH::EMotionType::Kinematic, object_layers::kMoving, 0.25F,
                Simulation::kHoistScoopEntityId));
        }

        // Ballast: a real 380 kg mass. The player can push it, be struck by it, or
        // stand where it lands.
        ballast_id_ = track(add_box(bodies, JPH::Vec3(0.42F, 0.42F, 0.42F),
                                    JPH::RVec3(34.6, 1.0, kScoopZ), JPH::EMotionType::Dynamic,
                                    object_layers::kMoving, 0.45F, Simulation::kBallastEntityId,
                                    JPH::Quat::sIdentity(), kBallastMassKg));

        // Tipper: beam plus an inboard counterweight lump, so it rests with the
        // catch end raised and resets itself once the ballast rolls off.
        JPH::StaticCompoundShapeSettings tipper_settings;
        tipper_settings.AddShape(JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
                                 new JPH::BoxShape(JPH::Vec3(3.6F, 0.20F, 1.1F)));
        tipper_settings.AddShape(JPH::Vec3(-3.95F, -0.30F, 0.0F), JPH::Quat::sIdentity(),
                                 new JPH::BoxShape(JPH::Vec3(0.44F, 0.44F, 0.44F)));
        JPH::Body *tipper = add_shape_body(
            bodies, tipper_settings.Create().Get(),
            JPH::RVec3(kTipperHingeX, kTipperHingeY, kTipperZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.30F,
            Simulation::kTipperEntityId, kTipperMassKg);
        tipper_id_ = track(tipper->GetID());

        // Valve lever: arm plus an outboard counterweight, so gravity shuts the
        // valve and only rope tension opens it.
        JPH::StaticCompoundShapeSettings lever_settings;
        lever_settings.AddShape(JPH::Vec3(-0.80F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
                                new JPH::BoxShape(JPH::Vec3(0.80F, 0.13F, 0.20F)));
        lever_settings.AddShape(JPH::Vec3(0.62F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
                                new JPH::BoxShape(JPH::Vec3(0.34F, 0.34F, 0.34F)));
        JPH::Body *lever = add_shape_body(
            bodies, lever_settings.Create().Get(),
            JPH::RVec3(kValveHingeX, kValveHingeY, -93.0), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kValveLeverEntityId, kValveLeverMassKg);
        valve_lever_id_ = track(lever->GetID());

        // WO-010 treadle: the plant's human-scale control, on the catwalk deck.
        // A plate hinged at its inboard end with a counterweight just past the
        // hinge, so it rests level against its stop with the valve shut, and an
        // 85 kg body standing on it swings it down against that counterweight.
        // Pylon top stops below the plate's underside so the hinge is free.
        const JPH::BodyID treadle_pylon = track(add_box(
            bodies, JPH::Vec3(0.24F, 0.155F, 0.34F),
            JPH::RVec3(kTreadleX, kCatwalkDeckY + 0.155, kTreadleZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kMachinePylonEntityId));
        JPH::StaticCompoundShapeSettings treadle_settings;
        treadle_settings.AddShape(
            JPH::Vec3(-kTreadlePlateMeters * 0.5F, 0.0F, 0.0F), JPH::Quat::sIdentity(),
            new JPH::BoxShape(JPH::Vec3(kTreadlePlateMeters * 0.5F, 0.04F, 0.55F)));
        // Counterweight rides above the hinge line: only its x offset sets the
        // restoring torque, so putting it high keeps it clear of the deck.
        treadle_settings.AddShape(JPH::Vec3(0.55F, 0.42F, 0.0F), JPH::Quat::sIdentity(),
                                  new JPH::BoxShape(JPH::Vec3(0.32F, 0.32F, 0.32F)));
        JPH::Body *treadle = add_shape_body(
            bodies, treadle_settings.Create().Get(),
            JPH::RVec3(kTreadleX, kTreadleHingeY, kTreadleZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.9F,
            Simulation::kTreadleEntityId, kTreadleMassKg);
        treadle_id_ = track(treadle->GetID());
        add_hinge(treadle_pylon, treadle_id_, JPH::RVec3(kTreadleX, kTreadleHingeY, kTreadleZ),
                  kTreadleRestAngle, kTreadleDepressedAngle, &treadle_hinge_,
                  Simulation::kMachinePylonEntityId);

        // Sheave masts carrying the control cable. Static, and load-bearing only
        // as pulley anchor points.
        track(add_box(bodies, JPH::Vec3(0.10F, 1.0F, 0.10F),
                      JPH::RVec3(kTreadleX - kTreadleCableArm, kCatwalkDeckY + 1.61,
                                 kTreadleMastZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kMachinePylonEntityId));
        track(add_box(bodies, JPH::Vec3(0.10F, 1.2F, 0.10F),
                      JPH::RVec3(kValveHingeX + kValveCableArm, kValveSheaveY - 1.2, kValveMastZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kMachinePylonEntityId));

        // Lift platform and its counterweight, each on a real vertical slider and
        // joined by a real pulley rope.
        JPH::Body *platform = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(2.3F, 0.16F, 2.3F)),
            JPH::RVec3(kLiftPlatformX, kLiftPlatformRestY, kLiftZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.9F,
            Simulation::kLiftPlatformEntityId, kLiftPlatformMassKg);
        lift_platform_id_ = track(platform->GetID());

        JPH::Body *counterweight = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(0.5F, 0.9F, 0.5F)),
            JPH::RVec3(kCounterweightX, 7.4, kLiftZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kCounterweightEntityId, kCounterweightMassKg);
        counterweight_id_ = track(counterweight->GetID());

        add_hinge(machine_pylon, tipper_id_, JPH::RVec3(kTipperHingeX, kTipperHingeY, kTipperZ),
                  -0.42F, 0.06F, &tipper_hinge_, Simulation::kMachinePylonEntityId);
        add_hinge(valve_pylon, valve_lever_id_, JPH::RVec3(kValveHingeX, kValveHingeY, -93.0),
                  kValveShutAngle, 0.95F, &valve_hinge_, Simulation::kMachinePylonEntityId);

        add_slider(lift_mast, lift_platform_id_, 0.0F, kLiftTravelMeters);
        add_slider(lift_mast, counterweight_id_, -kLiftTravelMeters, 0.0F);
        // -1 max length is unchanged from before add_pulley took explicit
        // points: Jolt auto-derives it from this exact as-built configuration
        // (PulleyConstraint.cpp), so behaviour here is byte-identical.
        (void)add_pulley(JPH::RVec3(kLiftPlatformX, kLiftPlatformRestY + 0.16, kLiftZ),
                        JPH::RVec3(kLiftPlatformX, kSheaveY, kLiftZ), lift_platform_id_,
                        JPH::RVec3(kCounterweightX, 8.3, kLiftZ),
                        JPH::RVec3(kCounterweightX, kSheaveY, kLiftZ), counterweight_id_, 1.0F, 0.0F,
                        -1.0F);
        settle_machine();
        add_rope();
        add_treadle_cable();
    }

    // WO-011. Ascent Atlas v1.0 kernel (section 9): KX-JIB + KX-CRATE. A
    // pendant-controlled crane, not an autonomous cycle -- everything here
    // moves only in response to a real command, through a real, finite-force
    // Jolt constraint motor, never a scripted animation or a teleport.
    void build_kernel_jib(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };

        // KX-DECK kernel patch: its own bounded apron, not the Kellerworks
        // yard -- Ascent Atlas section 9 places the kernel at a separate,
        // compressed scale.
        track(add_box(bodies, JPH::Vec3(kKernelDeckHalfExtent, 0.3F, kKernelDeckHalfExtent),
                      JPH::RVec3(kKernelBaseX, -0.3, kKernelBaseZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                      Simulation::kStaticDeckEntityId));

        const JPH::BodyID jib_mast = track(add_box(
            bodies, JPH::Vec3(0.35F, kJibMastHeight * 0.5F, 0.35F),
            JPH::RVec3(kJibMastX, kJibMastHeight * 0.5F, kJibMastZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kJibMastEntityId));

        JPH::Body *boom = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(kJibBoomLength * 0.5F, 0.15F, 0.15F)),
            JPH::RVec3(kJibMastX + kJibBoomLength * 0.5F, kJibMastHeight, kJibMastZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
            Simulation::kJibBoomEntityId, kJibBoomMassKg);
        jib_boom_id_ = track(boom->GetID());

        add_vertical_hinge(jib_mast, jib_boom_id_,
                           JPH::RVec3(kJibMastX, kJibMastHeight, kJibMastZ),
                           -kJibSlewLimitRadians, kJibSlewLimitRadians, kJibSlewMaxTorqueNm,
                           &jib_slew_hinge_);

        // Hook and crate both start near the deck: the crate is where a real
        // load actually sits, and the hook is pre-rigged to it (WO-005 allows
        // this -- "CAP-HOOK5 may be pre-placed on the crate for this WO").
        // Raising is what "picks it up"; nothing snaps into a solved pose.
        const float crate_start_y = kCrateHalfExtent;
        const float link_y = crate_start_y + kCrateHalfExtent;
        const float hook_start_y = link_y + 0.15F;
        JPH::Body *crate = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kCrateHalfExtent, kCrateHalfExtent, kCrateHalfExtent)),
            JPH::RVec3(kJibMastX + kJibBoomLength, crate_start_y, kJibMastZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kCrateEntityId, kCrateMassKg);
        crate_id_ = track(crate->GetID());

        JPH::Body *hook = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(0.15F, 0.15F, 0.15F)),
            JPH::RVec3(kJibMastX + kJibBoomLength, hook_start_y, kJibMastZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.4F,
            Simulation::kJibHookEntityId, kJibHookMassKg);
        jib_hook_id_ = track(hook->GetID());

        add_point_link(jib_hook_id_, crate_id_,
                      JPH::RVec3(kJibMastX + kJibBoomLength, link_y, kJibMastZ));

        // Travel is signed from this starting (lowest) pose, matching every
        // other slider in this file (lift platform, counterweight): 0 here,
        // upward-only, so raising is unambiguously the positive direction.
        const float hoist_travel = (kJibMastHeight - 0.35F) - hook_start_y;
        add_motorized_slider(jib_boom_id_, jib_hook_id_, 0.0F, hoist_travel, kJibMaxLiftForceN,
                             &jib_hoist_slider_);

        // Capacity-proving stand: fixed, no slew, permanently overweight,
        // always commanded to raise. Proves the rated force is real without
        // staging that failure as an unsafe lift on the working jib (WO-005
        // forbidden shortcuts: "unlimited winch force").
        const JPH::BodyID stand_mast = track(add_box(
            bodies, JPH::Vec3(0.3F, kCapacityStandMastHeight * 0.5F, 0.3F),
            JPH::RVec3(kCapacityStandX, kCapacityStandMastHeight * 0.5F, kCapacityStandZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kJibMastEntityId));
        JPH::Body *stand_load = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kCapacityStandLoadHalfExtent, kCapacityStandLoadHalfExtent,
                                        kCapacityStandLoadHalfExtent)),
            JPH::RVec3(kCapacityStandX, kCapacityStandLoadHalfExtent, kCapacityStandZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kCapacityStandEntityId, kCapacityStandLoadMassKg);
        capacity_stand_load_id_ = track(stand_load->GetID());
        JPH::Ref<JPH::SliderConstraint> stand_slider;
        const float stand_travel = kCapacityStandMastHeight - kCapacityStandLoadHalfExtent -
                                    kCapacityStandLoadHalfExtent;
        add_motorized_slider(stand_mast, capacity_stand_load_id_, 0.0F, stand_travel,
                             kJibMaxLiftForceN, &stand_slider);
        stand_slider->SetTargetVelocity(kJibHoistMaxRateMetersPerSec);
    }

    // WO-012. Ascent Atlas v1.0 kernel (section 9): KX-NEEDLE + KX-POCKETS. A
    // second, minimal jib-pattern hoist -- mast plus one finite-force
    // vertical motor, no slew -- lowers the beam on a fixed vertical line
    // directly above the gap. Two piers stand in for the atlas's KX-POCKETS;
    // seating pins them to the beam at runtime (update_needle), never here.
    void build_kernel_needle(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };

        // Each pier is two boxes, not one: a full-height main block, and a
        // shorter notch block under the overlap strip where the beam's end
        // actually lands. Seating the beam flush with the pier top (see
        // kNeedleSeatedY) means its underside has to have somewhere to go
        // that isn't solid pier -- a real pocket is a recess, not a shelf.
        //
        // The notch's boundary against the main block is pulled back by
        // kNeedlePocketMarginMeters, well clear of the beam's own edge --
        // found necessary by direct observation: siting that boundary exactly
        // at the beam's edge left the descending beam stopping ~4 cm short of
        // its seat, against JPH::BoxShape's default rounded convex radius on
        // the main block's corner, not the notch it was actually meant to
        // land on.
        const auto box_from_span = [](const float min_x, const float max_x) {
            return std::pair<float, float>{(min_x + max_x) * 0.5F, (max_x - min_x) * 0.5F};
        };
        const float pier_top_y_half = kNeedlePierHeight * 0.5F;
        const float notch_top_y_half = kNeedlePocketNotchTopY * 0.5F;

        const float approach_outer_x = kNeedlePierApproachX + kNeedlePierHalfExtentX;
        const float approach_inner_x = kNeedlePierApproachX - kNeedlePierHalfExtentX;
        const float approach_notch_boundary_x = kNeedlePocketApproachX - kNeedlePocketMarginMeters;
        const auto [approach_main_x, approach_main_half_x] =
            box_from_span(approach_inner_x, approach_notch_boundary_x);
        const auto [approach_notch_x, approach_notch_half_x] =
            box_from_span(approach_notch_boundary_x, approach_outer_x);

        track(add_box(bodies, JPH::Vec3(approach_main_half_x, pier_top_y_half, kNeedlePierHalfExtentZ),
                      JPH::RVec3(approach_main_x, pier_top_y_half, kNeedleGapCenterZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                      Simulation::kNeedlePierApproachEntityId));
        needle_pier_approach_id_ = track(add_box(
            bodies, JPH::Vec3(approach_notch_half_x, notch_top_y_half, kNeedlePierHalfExtentZ),
            JPH::RVec3(approach_notch_x, notch_top_y_half, kNeedleGapCenterZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
            Simulation::kNeedlePierApproachEntityId));

        const float far_outer_x = kNeedlePierFarX + kNeedlePierHalfExtentX;
        const float far_inner_x = kNeedlePierFarX - kNeedlePierHalfExtentX;
        const float far_notch_boundary_x = kNeedlePocketFarX + kNeedlePocketMarginMeters;
        const auto [far_main_x, far_main_half_x] = box_from_span(far_notch_boundary_x, far_outer_x);
        const auto [far_notch_x, far_notch_half_x] = box_from_span(far_inner_x, far_notch_boundary_x);

        track(add_box(bodies, JPH::Vec3(far_main_half_x, pier_top_y_half, kNeedlePierHalfExtentZ),
                      JPH::RVec3(far_main_x, pier_top_y_half, kNeedleGapCenterZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                      Simulation::kNeedlePierFarEntityId));
        needle_pier_far_id_ = track(add_box(
            bodies, JPH::Vec3(far_notch_half_x, notch_top_y_half, kNeedlePierHalfExtentZ),
            JPH::RVec3(far_notch_x, notch_top_y_half, kNeedleGapCenterZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
            Simulation::kNeedlePierFarEntityId));

        // Mast (visual + mass) and a small fixed head at the top, directly
        // above the gap centreline -- the head is the actual slider anchor,
        // the mast beneath it is proof scaffolding like the capacity stand's,
        // not a load-bearing member of the kernel chain.
        track(add_box(bodies, JPH::Vec3(0.35F, kNeedleHoistMastHeight * 0.5F, 0.35F),
                      JPH::RVec3(kNeedleGapCenterX, kNeedleHoistMastHeight * 0.5F,
                                kNeedleGapCenterZ + kNeedlePierHalfExtentZ + 1.0F),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kNeedleHoistMastEntityId));
        const JPH::BodyID needle_head = track(add_box(
            bodies, JPH::Vec3(0.4F, 0.2F, 0.4F),
            JPH::RVec3(kNeedleGapCenterX, kNeedleHoistMastHeight, kNeedleGapCenterZ),
            JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
            Simulation::kNeedleHoistMastEntityId));

        JPH::Body *beam = add_shape_body(
            bodies,
            new JPH::BoxShape(
                JPH::Vec3(kNeedleBeamHalfLength, kNeedleBeamHalfHeight, kNeedleBeamHalfWidth)),
            JPH::RVec3(kNeedleGapCenterX, kNeedleStowedY, kNeedleGapCenterZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.7F,
            Simulation::kNeedleBeamEntityId, kNeedleBeamMassKg);
        needle_beam_id_ = track(beam->GetID());

        // Travel is signed from the spawn (stowed, top) pose: 0 here, and
        // downward-only to the hard-limited seat height -- "reaches the
        // bottom of travel" and "reaches the seat" are the same event.
        const float travel_down = kNeedleStowedY - kNeedleSeatedY;
        add_motorized_slider(needle_head, needle_beam_id_, -travel_down, 0.0F, kNeedleMaxLiftForceN,
                             &needle_hoist_slider_);
    }

    // The stack: the tower's climbable lower section, as real static
    // collision. A perimeter deck ring per level around an open central
    // shaft, corner and mid-span columns carrying each deck, and a stair
    // ramp per level alternating sides so the ascent spirals. The player is
    // inside this, not looking at it.
    // Everything the player can see is something the player can hit. The
    // table is generated from the Godot builders (godot/presentation/
    // solid_export.gd), so the world these tests walk is the world that is
    // drawn. Built last, so a drawn mirror of a body already owned here is
    // recognised by its bounds and skipped rather than doubled -- a second
    // coincident body would split ledge probes and support identity.
    void build_world_solids(JPH::BodyInterface &bodies) {
        for (const WorldSolidBox &box : kWorldSolidBoxes) {
            const JPH::Vec3 half(box.hx, box.hy, box.hz);
            const JPH::RVec3 position(box.px, box.py, box.pz);
            const JPH::Quat rotation = JPH::Quat(box.qx, box.qy, box.qz, box.qw).Normalized();
            const JPH::AABox bounds = JPH::AABox(-half, half).Transformed(
                JPH::Mat44::sRotationTranslation(rotation, JPH::Vec3(position)));
            if (mirrors_owned_body(bounds)) {
                ++world_solid_mirrors_;
                continue;
            }
            const float convex_radius = std::min(JPH::cDefaultConvexRadius, half.ReduceMin());
            add_shape_body(bodies, new JPH::BoxShape(half, convex_radius), position, rotation,
                           JPH::EMotionType::Static, object_layers::kStatic, kWorldSolidFriction,
                           Simulation::kWorldSolidEntityId, 0.0F);
            ++world_solid_bodies_;
        }
        JPH::Array<JPH::Vec3> points;
        for (const WorldSolidHull &hull : kWorldSolidHulls) {
            JPH::DVec3 sum = JPH::DVec3::sZero();
            for (std::uint32_t index = 0; index < hull.count; ++index) {
                const float *point = &kWorldSolidHullPoints[(hull.first + index) * 3U];
                sum += JPH::DVec3(point[0], point[1], point[2]);
            }
            const JPH::DVec3 centre = sum / static_cast<double>(std::max<std::uint32_t>(hull.count, 1U));
            points.clear();
            for (std::uint32_t index = 0; index < hull.count; ++index) {
                const float *point = &kWorldSolidHullPoints[(hull.first + index) * 3U];
                points.push_back(JPH::Vec3(static_cast<float>(point[0] - centre.GetX()),
                                           static_cast<float>(point[1] - centre.GetY()),
                                           static_cast<float>(point[2] - centre.GetZ())));
            }
            const JPH::ConvexHullShapeSettings settings(points.data(), static_cast<int>(points.size()));
            const JPH::ShapeSettings::ShapeResult result = settings.Create();
            if (result.HasError()) {
                ++world_solid_rejected_;
                continue;
            }
            add_shape_body(bodies, result.Get().GetPtr(),
                           JPH::RVec3(centre.GetX(), centre.GetY(), centre.GetZ()),
                           JPH::Quat::sIdentity(), JPH::EMotionType::Static, object_layers::kStatic,
                           kWorldSolidFriction, Simulation::kWorldSolidEntityId, 0.0F);
            ++world_solid_bodies_;
        }
    }

    [[nodiscard]] bool mirrors_owned_body(const JPH::AABox &bounds) const {
        JPH::AllHitCollisionCollector<JPH::CollideShapeBodyCollector> collector;
        physics_system_.GetBroadPhaseQuery().CollideAABox(bounds, collector);
        for (const JPH::BodyID id : collector.mHits) {
            const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), id);
            if (!lock.Succeeded() ||
                lock.GetBody().GetUserData() == Simulation::kWorldSolidEntityId) {
                continue;
            }
            const JPH::AABox owned = lock.GetBody().GetWorldSpaceBounds();
            if ((owned.mMin - bounds.mMin).Abs().ReduceMax() < kWorldSolidMirrorTolerance &&
                (owned.mMax - bounds.mMax).Abs().ReduceMax() < kWorldSolidMirrorTolerance) {
                return true;
            }
        }
        return false;
    }

    void build_stack(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };
        const auto frame = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position,
                               const JPH::Quat rotation = JPH::Quat::sIdentity()) {
            track(add_box(bodies, half_extent, position, JPH::EMotionType::Static,
                          object_layers::kStatic, 0.85F, Simulation::kTowerEntityId, rotation));
        };

        const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.5F;
        const float inner_half = kStackHalfExtent - kStackDeckBandDepth;

        const float flight_head = kStackHalfExtent - kStackDeckBandDepth;
        for (int level = 1; level <= kStackLevelCount; ++level) {
            const float deck_y = static_cast<float>(level) * kStackLevelHeight;
            const float slab_y = deck_y - kStackDeckHalfThickness;
            // The flight arriving at this level: on band `well_side`, climbing
            // toward x = well_side * flight_head.
            const float well_side = ((level - 1) % 2 == 0) ? 1.0F : -1.0F;

            // Deck ring: two full-width bands and two inner bands, leaving a
            // 22 m shaft open through every level.
            for (const float sz : {1.0F, -1.0F}) {
                const float band_z = kStackCenterZ + sz * band_center;
                if (sz != well_side) {
                    frame(JPH::Vec3(kStackHalfExtent, kStackDeckHalfThickness,
                                    kStackDeckBandDepth * 0.5F),
                          JPH::RVec3(kStackCenterX, slab_y, band_z));
                    continue;
                }
                // Band with the stairwell: full depth before and after the
                // well along x, two side strips beside it.
                const auto span = [&](const float from, const float to, const float z0,
                                      const float z1) {
                    const float x0 = well_side * from;
                    const float x1 = well_side * to;
                    frame(JPH::Vec3(std::abs(x1 - x0) * 0.5F, kStackDeckHalfThickness,
                                    std::abs(z1 - z0) * 0.5F),
                          JPH::RVec3(kStackCenterX + (x0 + x1) * 0.5F, slab_y,
                                     kStackCenterZ + sz * (z0 + z1) * 0.5F));
                };
                const float band_in = band_center - kStackDeckBandDepth * 0.5F;
                const float band_out = band_center + kStackDeckBandDepth * 0.5F;
                const float well_in = band_center - kStackStairwellHalfWidth;
                const float well_out = band_center + kStackStairwellHalfWidth;
                span(-kStackHalfExtent, kStackStairwellStart, band_in, band_out);
                span(flight_head, kStackHalfExtent, band_in, band_out);
                span(kStackStairwellStart, flight_head, band_in, well_in);
                span(kStackStairwellStart, flight_head, well_out, band_out);
            }
            for (const float sx : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, inner_half),
                      JPH::RVec3(kStackCenterX + sx * band_center, slab_y, kStackCenterZ));
            }
        }

        // Columns: corners and edge mid-spans, one run per storey.
        for (int level = 0; level < kStackLevelCount; ++level) {
            const float base_y = static_cast<float>(level) * kStackLevelHeight;
            const float column_half = kStackLevelHeight * 0.5F;
            for (const float sx : {1.0F, -1.0F}) {
                for (const float sz : {1.0F, -1.0F}) {
                    frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                          JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                                     kStackCenterZ + sz * kStackHalfExtent));
                }
                frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                      JPH::RVec3(kStackCenterX + sx * kStackHalfExtent, base_y + column_half,
                                 kStackCenterZ));
                frame(JPH::Vec3(kStackColumnHalf, column_half, kStackColumnHalf),
                      JPH::RVec3(kStackCenterX, base_y + column_half,
                                 kStackCenterZ + sx * kStackHalfExtent));
            }
        }

        // Stair runs: one flight per storey, alternating sides so the climb
        // spirals the perimeter rather than stacking in one corner. A single
        // inclined slab per flight -- the visible steps are drawn on top of
        // it, so what you see and what you stand on agree.
        for (int level = 0; level < kStackLevelCount; ++level) {
            const float base_y = static_cast<float>(level) * kStackLevelHeight;
            const float run = kStackHalfExtent * 2.0F - kStackDeckBandDepth * 2.0F;
            const float rise = kStackLevelHeight;
            const float length = std::sqrt(run * run + rise * rise);
            const float pitch = std::atan2(rise, run);
            const float side = (level % 2 == 0) ? 1.0F : -1.0F;
            // Runs along X on alternating Z bands, climbing in +X or -X. The
            // slab is set down by its own half-thickness along its normal so
            // its walking surface meets the floor below and the deck above
            // flush at both ends, not 0.19 m proud of them.
            const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), side * pitch);
            constexpr float kFlightHalfThickness = 0.18F;
            frame(JPH::Vec3(length * 0.5F, kFlightHalfThickness, kStackRampHalfWidth),
                  JPH::RVec3(kStackCenterX + side * kFlightHalfThickness * std::sin(pitch),
                             base_y + rise * 0.5F - kFlightHalfThickness * std::cos(pitch),
                             kStackCenterZ + side * band_center),
                  rotation);
        }
    }

    // WO-013. Ascent Atlas v1.0 kernel (section 9): KX-SUMP + KX-GRATE. Fixed
    // approach/far decking flank one grate panel; only the grate's own
    // collidability changes, driven by update_sump every tick. Starts wet
    // (grate is a sensor -- see create) since the sump starts full.
    // AS-001. Ascent Atlas §6 band B00: MOD-APRON, MOD-INTAKE-BELT,
    // CAP-PENDANT, MOD-YARD-JIB, the 4 t pack, MOD-DOG-A, MOD-STAIR-A,
    // MOD-SKIN-LADDER-S. The ground plane already reaches here, so MOD-APRON
    // is the yard furniture standing on it rather than a second slab.
    void build_intake_rise(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };
        const auto fixed = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position,
                               const std::uint64_t entity_id,
                               const JPH::Quat rotation = JPH::Quat::sIdentity()) {
            return track(add_box(bodies, half_extent, position, JPH::EMotionType::Static,
                                 object_layers::kStatic, 0.85F, entity_id, rotation));
        };

        // ---- the bay: back, sides, and the front wall carrying the throat --
        const float bay_half_z = (kIntakeBayFrontZ - kIntakeBayBackZ) * 0.5F;
        const float bay_center_z = (kIntakeBayFrontZ + kIntakeBayBackZ) * 0.5F;
        const float wall_half_y = kIntakeBayWallHeight * 0.5F;

        fixed(JPH::Vec3(kIntakeBayHalfX, wall_half_y, kIntakeBayWallHalfZ),
              JPH::RVec3(kIntakeBayCenterX, wall_half_y, kIntakeBayBackZ),
              Simulation::kIntakeBayEntityId);
        for (const float sx : {1.0F, -1.0F}) {
            fixed(JPH::Vec3(kIntakeBayWallHalfZ, wall_half_y, bay_half_z),
                  JPH::RVec3(kIntakeBayCenterX + sx * kIntakeBayHalfX, wall_half_y, bay_center_z),
                  Simulation::kIntakeBayEntityId);
        }

        // Front wall: two jambs plus a solid lintel over the throat. The
        // lintel is what stops the throat from being mantled over once the
        // dog is shut -- with it, a closed dog leaves no standable surface
        // anywhere in the opening.
        const float jamb_half_x = (kIntakeBayHalfX - kIntakeThroatHalfWidth) * 0.5F;
        JPH::BodyID west_jamb;
        for (const float sx : {1.0F, -1.0F}) {
            const JPH::BodyID jamb =
                fixed(JPH::Vec3(jamb_half_x, wall_half_y, kIntakeBayWallHalfZ),
                      JPH::RVec3(kIntakeBayCenterX + sx * (kIntakeThroatHalfWidth + jamb_half_x),
                                 wall_half_y, kIntakeBayFrontZ),
                      Simulation::kIntakeBayEntityId);
            if (sx < 0.0F) {
                west_jamb = jamb;
            }
        }
        const float lintel_half_y = (kIntakeBayWallHeight - kIntakeThroatHeight) * 0.5F;
        fixed(JPH::Vec3(kIntakeThroatHalfWidth, lintel_half_y, kIntakeBayWallHalfZ),
              JPH::RVec3(kIntakeBayCenterX, kIntakeThroatHeight + lintel_half_y,
                         kIntakeBayFrontZ),
              Simulation::kIntakeBayEntityId);

        // ---- MOD-DOG-A ----------------------------------------------------
        // Hinged at the west jamb, extending east to fill the throat at angle
        // zero. Positive rotation about +Y carries its far end toward -Z, i.e.
        // into the bay, which is where the pack is standing.
        JPH::Body *dog = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kIntakeDogHalfWidth, kIntakeDogHalfHeight,
                                        kIntakeDogHalfThickness)),
            JPH::RVec3(kIntakeBayCenterX, kIntakeDogCenterY, kIntakeBayFrontZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.7F,
            Simulation::kIntakeDogEntityId, kIntakeDogMassKg);
        intake_dog_id_ = track(dog->GetID());
        add_vertical_hinge(west_jamb, intake_dog_id_,
                           JPH::RVec3(kIntakeDogHingeX, kIntakeDogCenterY,
                                      kIntakeBayFrontZ),
                           0.0F, kIntakeDogRetractAngle, kIntakeDogMaxTorqueNm,
                           &intake_dog_hinge_);
        if (intake_dog_hinge_ != nullptr) {
            // Commanded open from build time and never commanded otherwise.
            // The pack is the only thing holding it shut.
            intake_dog_hinge_->SetTargetAngularVelocity(kIntakeDogRetractSpeed);
        }

        // ---- MOD-INTAKE-BELT and the CAP-PENDANT catwalk -------------------
        intake_belt_id_ = track(add_box(
            bodies, JPH::Vec3(kIntakeBeltHalfX, kIntakeBeltHalfY, kIntakeBeltHalfZ),
            JPH::RVec3(kIntakeBeltX, kIntakeBeltTopY - kIntakeBeltHalfY, kIntakeBeltCenterZ),
            JPH::EMotionType::Kinematic, object_layers::kMoving, 0.9F,
            Simulation::kIntakeBeltEntityId));

        fixed(JPH::Vec3(1.6F, kIntakeBeltHalfY, 3.0F),
              JPH::RVec3(kIntakePendantX, kIntakePendantTopY - kIntakeBeltHalfY,
                         kIntakePendantZ),
              Simulation::kIntakeApronEntityId);
        // Ramp up to the catwalk so the pendant is reachable on foot without
        // spending a mantle, matching how every other stair in this world works.
        {
            const float rise = kIntakePendantTopY;
            const float run = 4.0F;
            const float length = std::sqrt(run * run + rise * rise);
            const float pitch = std::atan2(rise, run);
            fixed(JPH::Vec3(1.6F, 0.15F, length * 0.5F),
                  JPH::RVec3(kIntakePendantX, rise * 0.5F, kIntakePendantZ + 3.0F + run * 0.5F),
                  Simulation::kIntakeApronEntityId,
                  JPH::Quat::sRotation(JPH::Vec3::sAxisX(), pitch));
        }

        // ---- MOD-YARD-JIB --------------------------------------------------
        const JPH::BodyID mast = fixed(
            JPH::Vec3(0.45F, kIntakeBoomHeight * 0.5F, 0.45F),
            JPH::RVec3(kIntakeJibMastX, kIntakeBoomHeight * 0.5F, kIntakeJibMastZ),
            Simulation::kIntakeJibMastEntityId);

        // Built already pointing -Z, down the line to the pack. The hinge takes
        // its world-space normal axes at construction, so "angle zero" is this
        // as-built pose and the +-0.90 rad slew limit sweeps symmetrically
        // around the throat.
        const JPH::Quat boom_rotation =
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(), static_cast<float>(kPi * 0.5));
        JPH::Body *boom = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(kIntakeBoomLength * 0.5F, 0.22F, 0.22F)),
            JPH::RVec3(kIntakeJibMastX, kIntakeBoomHeight,
                       kIntakeJibMastZ - kIntakeBoomLength * 0.5F),
            boom_rotation, JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
            Simulation::kIntakeJibBoomEntityId, kIntakeJibBoomMassKg);
        intake_boom_id_ = track(boom->GetID());
        add_vertical_hinge(mast, intake_boom_id_,
                           JPH::RVec3(kIntakeJibMastX, kIntakeBoomHeight, kIntakeJibMastZ),
                           -kIntakeJibSlewLimitRadians, kIntakeJibSlewLimitRadians,
                           kIntakeJibSlewTorqueNm, &intake_slew_hinge_);

        // The pack sits on the apron under the boom tip, pre-slung (Atlas B00
        // allows the rigging pre-placed; the hook is still a real constraint).
        const float pack_start_y = kIntakePackHalfY;
        const float pack_link_y = pack_start_y + kIntakePackHalfY;
        const float hook_start_y = pack_link_y + 0.20F;
        JPH::Body *pack = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kIntakePackHalfX, kIntakePackHalfY, kIntakePackHalfZ)),
            JPH::RVec3(kIntakeBayCenterX, pack_start_y, kIntakePackZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kIntakePackEntityId, kIntakePackMassKg);
        intake_pack_id_ = track(pack->GetID());

        JPH::Body *hook = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(0.20F, 0.20F, 0.20F)),
            JPH::RVec3(kIntakeBayCenterX, hook_start_y, kIntakePackZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, object_layers::kMoving, 0.4F,
            Simulation::kIntakeJibHookEntityId, kIntakeJibHookMassKg);
        intake_hook_id_ = track(hook->GetID());
        // AS-002 makes this releasable (release_pack_to_cradle() /
        // sling_pack()): create_sling_pin() uses track_for_teardown=false,
        // exactly the needle pins' contract, so it is intake_sling_pin_'s
        // sole owner rather than also sitting in machine_constraints_, which
        // Jolt's ConstraintManager::Remove would then assert on if it had
        // already been removed once at runtime. Unchanged from AS-001: the
        // pin point is pack_link_y, 0.20 m below the hook's own built
        // hook_start_y -- not the hook's position -- so this is byte-
        // identical to the add_point_link() call it replaces.
        create_sling_pin(JPH::RVec3(kIntakeBayCenterX, pack_link_y, kIntakePackZ));

        const float hoist_travel = (kIntakeBoomHeight - 0.45F) - hook_start_y;
        add_motorized_slider(intake_boom_id_, intake_hook_id_, 0.0F, hoist_travel,
                             kIntakeJibWinchForceN, &intake_hoist_slider_);

        // The 9 t pack on its own fixed stand, permanently commanded up at the
        // same rated winch force. It proves the rating is enforced by the
        // solver without staging that failure as an unsafe lift on the working
        // jib (Atlas B00: "9 t overweight must stall").
        const JPH::BodyID overweight_mast = fixed(
            JPH::Vec3(0.35F, kIntakeOverweightMastHeight * 0.5F, 0.35F),
            JPH::RVec3(kIntakeOverweightStandX, kIntakeOverweightMastHeight * 0.5F,
                       kIntakeOverweightStandZ),
            Simulation::kIntakeJibMastEntityId);
        JPH::Body *overweight = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kIntakePackHalfX, kIntakePackHalfY, kIntakePackHalfZ)),
            JPH::RVec3(kIntakeOverweightStandX, kIntakePackHalfY, kIntakeOverweightStandZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kIntakeOverweightPackEntityId, kIntakeOverweightPackMassKg);
        intake_overweight_pack_id_ = track(overweight->GetID());
        JPH::Ref<JPH::SliderConstraint> overweight_slider;
        add_motorized_slider(overweight_mast, intake_overweight_pack_id_, 0.0F,
                             kIntakeOverweightMastHeight - 2.0F * kIntakePackHalfY,
                             kIntakeJibWinchForceN, &overweight_slider);
        overweight_slider->SetTargetVelocity(kIntakeJibHoistRateMetersPerSec);

        // ---- MOD-STAIR-A ---------------------------------------------------
        // One inclined slab per flight, switchbacking along X, with a landing
        // pad at each turn. Same pattern as build_stack(): what you see drawn
        // on top and what you stand on are the same body.
        const float flight_length =
            std::sqrt(4.0F * kIntakeStairHalfRun * kIntakeStairHalfRun +
                      kIntakeStairFlightRise * kIntakeStairFlightRise);
        const float pitch = std::atan2(kIntakeStairFlightRise, 2.0F * kIntakeStairHalfRun);
        // An inclined slab's walking surface stands this far above the line
        // through its ends. Landings are raised to match, so a turn is a flush
        // step and not a lip the no-step-assist capsule has to climb.
        const float tread_surface = kIntakeStairSlabHalfY / std::cos(pitch);
        const auto flight_side = [](const int flight) {
            return (flight % 2 == 0) ? 1.0F : -1.0F;
        };
        for (int flight = 0; flight < kIntakeStairFlightCount; ++flight) {
            const float base_y = static_cast<float>(flight) * kIntakeStairFlightRise;
            // Even flights climb toward +X in the south lane, odd flights back
            // toward -X in the north lane.
            const float side = flight_side(flight);
            fixed(JPH::Vec3(flight_length * 0.5F, kIntakeStairSlabHalfY, kIntakeStairHalfWidth),
                  JPH::RVec3(kIntakeBayCenterX, base_y + kIntakeStairFlightRise * 0.5F,
                             kIntakeStairZ + side * kIntakeStairLaneOffset),
                  Simulation::kIntakeStairEntityId,
                  JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), side * pitch));
        }
        // One landing at the foot of every flight, plus the one flight 5
        // arrives on. Each spans both lanes, so a turn is a walk across it.
        for (int flight = 0; flight <= kIntakeStairFlightCount; ++flight) {
            const float base_y = static_cast<float>(flight) * kIntakeStairFlightRise;
            fixed(JPH::Vec3(kIntakeStairLandingHalfX, kIntakeStairSlabHalfY,
                            kIntakeStairLandingHalfZ),
                  JPH::RVec3(kIntakeBayCenterX - flight_side(flight) * kIntakeStairLandingX,
                             base_y + tread_surface - kIntakeStairSlabHalfY, kIntakeStairZ),
                  Simulation::kIntakeStairEntityId);
        }

        // ---- the +24 m handoff ---------------------------------------------
        // Reaches from the top of flight 5 south past the bay wall to meet the
        // SKIN ladder head, so both braids arrive on the same deck.
        const float handoff_half_z = (kIntakeHandoffSouthZ - kIntakeHandoffNorthZ) * 0.5F;
        const float handoff_center_z = (kIntakeHandoffSouthZ + kIntakeHandoffNorthZ) * 0.5F;
        fixed(JPH::Vec3(kIntakeHandoffHalfX, kIntakeHandoffHalfY, handoff_half_z),
              JPH::RVec3(kIntakeHandoffCenterX,
                         kIntakeHandoffY + tread_surface - kIntakeHandoffHalfY,
                         handoff_center_z),
              Simulation::kIntakeHandoffEntityId);

        // ---- MOD-SKIN-LADDER-S ---------------------------------------------
        for (int rung = 1; rung <= kIntakeSkinRungCount; ++rung) {
            const float top_y = static_cast<float>(rung) * kIntakeSkinRungRise;
            const float rung_z =
                kIntakeSkinHeadZ + 2.0F * kIntakeSkinRungHalfZ *
                                       static_cast<float>(kIntakeSkinRungCount - rung);
            fixed(JPH::Vec3(kIntakeSkinRungHalfX, kIntakeSkinRungHalfY, kIntakeSkinRungHalfZ),
                  JPH::RVec3(kIntakeSkinCenterX, top_y - kIntakeSkinRungHalfY, rung_z),
                  Simulation::kIntakeSkinEntityId);
        }
    }

    // AS-002. Ascent Atlas §6 band B00's 24.1872-40.1872 m leftover, chain K0
    // PLAY. Everything above the handoff deck stacks off AS-001's own
    // tread-surface offset (kIntakeHandoffY + slab-half-thickness / cos(pitch)),
    // recomputed here at this run's own 30 deg pitch, so it stays flush with
    // the deck below to the millimetre rather than by matching a second copy
    // of the same literal.
    void build_legal_forty(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };
        const auto fixed = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position,
                               const std::uint64_t entity_id,
                               const JPH::Quat rotation = JPH::Quat::sIdentity()) {
            track(add_box(bodies, half_extent, position, JPH::EMotionType::Static,
                          object_layers::kStatic, 0.85F, entity_id, rotation));
        };

        const float handoff_pitch = std::atan2(kIntakeStairFlightRise, 2.0F * kIntakeStairHalfRun);
        const float handoff_tread_surface = kIntakeStairSlabHalfY / std::cos(handoff_pitch);
        const float handoff_surface_y = kIntakeHandoffY + handoff_tread_surface;
        const float mid_landing_surface_y = handoff_surface_y + kLegalFortyFlightRiseMeters;
        const float hall_deck_surface_y = mid_landing_surface_y + kLegalFortyFlightRiseMeters;

        // ---- MOD-STAIR-A-SWING: the dynamic bascule flight -------------------
        const JPH::RVec3 hinge_point(kIntakeSwingHingeX, mid_landing_surface_y, kIntakeSwingHingeZ);
        // R(phi), phi = pi/2 - theta, maps local +X to world (sin theta,
        // cos theta, 0) about the world +Z hinge axis -- verified against
        // Jolt::Quat::sRotation's own [axis*sin(angle/2), cos(angle/2)] form
        // (see kIntakeSwingHingeTravelRadians's comment above). Built at
        // theta_stowed, so this is also the body's as-built rotation.
        const float stowed_phi = static_cast<float>(kPi) * 0.5F - kLegalFortyStowedThetaRadians;
        const JPH::Quat stowed_rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), stowed_phi);
        // Body centre = hinge - R(phi) * (half_length, 0, 0): the hinge sits at
        // the body's own local (+half_length, 0, 0), its "top end".
        const JPH::Vec3 hinge_to_centre_world =
            stowed_rotation * JPH::Vec3(kLegalFortyFlightHalfLength, 0.0F, 0.0F);
        const JPH::RVec3 swing_build_centre = hinge_point - hinge_to_centre_world;

        JPH::Body *swing_flight = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kLegalFortyFlightHalfLength, kIntakeStairSlabHalfY,
                                        kLegalFortyFlightHalfWidth)),
            swing_build_centre, stowed_rotation, JPH::EMotionType::Dynamic, object_layers::kMoving,
            0.7F, Simulation::kIntakeSwingFlightEntityId, kLegalFortyFlightMassKg);
        intake_swing_flight_id_ = track(swing_flight->GetID());

        // A small static anchor at the hinge point -- the hinge's first body,
        // exactly like the needle hoist's fixed head (add_motorized_slider's
        // own comment: "the head is the actual... anchor, the mast beneath it
        // is proof scaffolding"). Sized well under the flight's own 0.18 m
        // corner-sweep radius (this file's clearances note above, echoing
        // the plan's own: "the slab's top-end corners lie 0.18 m off the
        // hinge axis and sweep a 0.18 m circle") -- a cube of half-extent h
        // reaches h*sqrt(2) at its corner, so h=0.08 stays inside that
        // circle by 0.067 m at every angle. AS-001 lost a day to the
        // opposite mistake once already (MOD-DOG-A's own hinge sitting
        // flush with its jamb); this is that same trap, self-inflicted
        // against AS-002's own anchor rather than a neighbour's structure --
        // found by direct observation: at the old 0.30 m half-extent (bigger
        // than the sweep radius on every side) the loaded flight never
        // opened past a fraction of a degree, pinned by its own anchor.
        //
        // Shrinking it to 0.08 only thinned that same pinning, never removed
        // it: the flight's own cross-section is coincident with the hinge
        // pivot at every sweep angle by construction, so ANY nonzero-size
        // anchor box keeps a sliver of solid overlap there always -- found
        // by direct diagnostic tracing (OnContactAdded/Persisted instrumented
        // to log the actual world-space contact point, not just entity IDs)
        // showing a real, continuous PERSISTED contact at the hinge itself
        // for the entire length of a 20+ s window in which the flight was
        // otherwise correctly, fully unloaded and should have been relaxing
        // freely under its own weight. kIntakeSwingAnchorEntityId gives this
        // one body its own identity (unlike every other static piece of
        // MOD-STAIR-A, which stays grouped under kIntakeStairEntityId) so
        // OnContactValidate below can exclude exactly this pair, the same
        // way it already excludes the flight from the handoff deck: the
        // hinge CONSTRAINT is what relates the anchor and the flight, and
        // ordinary contact resolution between them was never meaningful.
        const JPH::BodyID swing_anchor = track(add_box(
            bodies, JPH::Vec3(0.08F, 0.08F, 0.08F), hinge_point, JPH::EMotionType::Static,
            object_layers::kStatic, 0.8F, Simulation::kIntakeSwingAnchorEntityId));
        // Unmotored: gravity alone holds this at its stowed limit (jolt angle
        // 0, this build pose) until MOD-CW-CRADLE's rope tension beats it.
        // Limits are negative-only -- see kIntakeSwingHingeTravelRadians.
        add_hinge(swing_anchor, intake_swing_flight_id_, hinge_point,
                  -kIntakeSwingHingeTravelRadians, 0.0F, &intake_swing_hinge_,
                  Simulation::kIntakeStairEntityId);

        // ---- MOD-CW-CRADLE: dynamic car on a free vertical slider ------------
        // The hook hangs at the boom tip, so working radius is fixed at the
        // 12 m boom; only the slew bearing chooses where on that circle the
        // cradle sits. Sited on the SAME (east, +X) side as the hinge/flight
        // above, matching AS-002's own geometry table (hinge x=+7.856,
        // cradle x=+8.608): hook_x = -boomLength*sin(angle) for a POSITIVE
        // set_intake_slew_input, verified against the running mechanism, so
        // reaching this +X cradle is a NEGATIVE slew command
        // (kIntakeCwCradleBearingRadians is the magnitude; update_intake's
        // sign is negated at every call site that targets the cradle).
        const float cradle_x =
            kIntakeJibMastX + kIntakeBoomLength * std::sin(kIntakeCwCradleBearingRadians);
        const float cradle_z =
            kIntakeJibMastZ - kIntakeBoomLength * std::cos(kIntakeCwCradleBearingRadians);
        const JPH::RVec3 cradle_build_centre(cradle_x, kIntakeCwCradleBuildCenterY, cradle_z);

        // Offset south of the car's own centreline, clear of the column the
        // pack is lowered through -- found by direct observation: a mast
        // built AT the car's own xz (matching the plan's literal "same xz")
        // stood directly in that column, and the descending pack snagged its
        // top on every attempt. add_slider only needs a static anchor body
        // for the vertical constraint; nothing requires it to share the
        // car's own xz, and the cradle is open in +-Z for exactly this kind
        // of side approach.
        const float guide_mast_z = cradle_z + 2.0F;
        const JPH::BodyID cradle_guide = track(add_box(
            bodies,
            JPH::Vec3(kIntakeCwCradleGuideHalfX, kIntakeCwCradleGuideHalfY,
                      kIntakeCwCradleGuideHalfZ),
            JPH::RVec3(cradle_x, kIntakeCwCradleGuideHalfY, guide_mast_z), JPH::EMotionType::Static,
            object_layers::kStatic, 0.8F, Simulation::kIntakeCwCradleEntityId));

        // Three-sided open frame -- floor and two side rails, open in +-Z --
        // not a closed hopper: a 2.4 m hopper cannot take the 2.2 x 2.3 m pack
        // without pushing the cradle into the bay's front wall.
        JPH::Body *cradle = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kIntakeCwCradleHalfX, kIntakeCwCradleHalfY,
                                        kIntakeCwCradleHalfZ)),
            cradle_build_centre, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
            object_layers::kMoving, 0.7F, Simulation::kIntakeCwCradleEntityId,
            kIntakeCwCradleTareMassKg);
        intake_cw_cradle_id_ = track(cradle->GetID());
        add_slider(cradle_guide, intake_cw_cradle_id_, kIntakeCwCradleLimitMinMeters,
                  kIntakeCwCradleLimitMaxMeters);

        // ---- The rope: one generalised JPH::PulleyConstraint -----------------
        const JPH::RVec3 sheave_a(kIntakeSwingHingeX,
                                  mid_landing_surface_y + kIntakeSwingSheaveHeightMeters,
                                  kIntakeSwingHingeZ);
        const JPH::RVec3 sheave_b(cradle_x,
                                  mid_landing_surface_y + kIntakeSwingSheaveHeightMeters,
                                  cradle_z);
        const JPH::RVec3 flight_bracket_point =
            bodies.GetCenterOfMassTransform(intake_swing_flight_id_) *
            JPH::RVec3(kIntakeSwingBracketLocalX, kIntakeSwingBracketLocalY, 0.0);
        const JPH::RVec3 cradle_top_point =
            bodies.GetCenterOfMassTransform(intake_cw_cradle_id_) *
            JPH::RVec3(0.0, kIntakeCwCradleHalfY, 0.0);
        const float flight_side_length = JPH::Vec3(sheave_a - flight_bracket_point).Length();
        const float cradle_side_length_at_build =
            JPH::Vec3(sheave_b - cradle_top_point).Length();
        // Tension-only, rated per the T_crit derivation above (empty cradle
        // 0.37x the stowed demand, loaded 1.21x): never pushes, and its total
        // length is fixed at this as-built (empty-cradle, stowed-flight) sum,
        // which is also this mechanism's one physically consistent rest state
        // when the rope is fully taut.
        intake_cw_pulley_ =
            add_pulley(flight_bracket_point, sheave_a, intake_swing_flight_id_, cradle_top_point,
                      sheave_b, intake_cw_cradle_id_, 1.0F, 0.0F,
                      flight_side_length + cradle_side_length_at_build);

        // ---- MOD-STAIR-A upper flight (static) + mid-landing ------------------
        // An inclined slab like every other flight in this file -- AS-001's own
        // pattern (Quat::sRotation(axisZ, side*pitch)), not the flat, unrotated
        // box this was until now. Climbs toward -X (side = -1, "odd flights
        // back toward -X" per flight_side's own comment): the un-rotated half-
        // length (8.00) times cos(pitch) puts its rotated footprint at
        // x in [-4.506, 9.350], which is exactly the well's own centre-X at
        // the top end and the mid-landing's own centre-X at the foot -- this
        // position was already sited for a rotated slab, only the rotation
        // itself was missing.
        const float upper_flight_pitch =
            std::asin(kLegalFortyFlightRiseMeters / kLegalFortyFlightLength);
        fixed(JPH::Vec3(kLegalFortyFlightHalfLength, kIntakeStairSlabHalfY,
                        kLegalFortyFlightHalfWidth),
              JPH::RVec3(kLegalFortyUpperFlightCenterX,
                         mid_landing_surface_y + kLegalFortyFlightRiseMeters * 0.5F,
                         kLegalFortyUpperFlightCenterZ),
              Simulation::kIntakeStairEntityId,
              JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -upper_flight_pitch));
        fixed(JPH::Vec3(kLegalFortyMidLandingHalfX, kIntakeStairSlabHalfY,
                        kLegalFortyMidLandingHalfZ),
              JPH::RVec3(kLegalFortyMidLandingCenterX, mid_landing_surface_y - kIntakeStairSlabHalfY,
                         kLegalFortyMidLandingCenterZ),
              Simulation::kIntakeStairEntityId);

        // ---- MOD-HALL-DECK lower landing, with its stair well -----------------
        // Four strips tiling the deck minus a rectangular well, exactly the
        // jamb+lintel pattern the bay's own throat already uses, rotated from
        // a wall opening to a floor opening.
        const float well_min_x = kLegalFortyWellCenterX - kLegalFortyWellHalfX;
        const float well_max_x = kLegalFortyWellCenterX + kLegalFortyWellHalfX;
        const float well_min_z = kLegalFortyWellCenterZ - kLegalFortyWellHalfZ;
        const float well_max_z = kLegalFortyWellCenterZ + kLegalFortyWellHalfZ;
        const float deck_min_x = -kLegalFortyHallDeckHalfX;
        const float deck_max_x = kLegalFortyHallDeckHalfX;
        const float deck_min_z = kLegalFortyHallDeckCenterZ - kLegalFortyHallDeckHalfZ;
        const float deck_max_z = kLegalFortyHallDeckCenterZ + kLegalFortyHallDeckHalfZ;
        const float deck_center_y = hall_deck_surface_y - kLegalFortyHallDeckHalfThickness;
        const auto span = [](const float lo, const float hi) {
            return std::pair<float, float>{(lo + hi) * 0.5F, (hi - lo) * 0.5F};
        };
        const auto [south_cz, south_hz] = span(well_max_z, deck_max_z);
        fixed(JPH::Vec3(kLegalFortyHallDeckHalfX, kLegalFortyHallDeckHalfThickness, south_hz),
              JPH::RVec3(0.0F, deck_center_y, south_cz), Simulation::kIntakeHallDeckEntityId);
        const auto [north_cz, north_hz] = span(deck_min_z, well_min_z);
        fixed(JPH::Vec3(kLegalFortyHallDeckHalfX, kLegalFortyHallDeckHalfThickness, north_hz),
              JPH::RVec3(0.0F, deck_center_y, north_cz), Simulation::kIntakeHallDeckEntityId);
        const auto [west_cx, west_hx] = span(deck_min_x, well_min_x);
        fixed(JPH::Vec3(west_hx, kLegalFortyHallDeckHalfThickness, kLegalFortyWellHalfZ),
              JPH::RVec3(west_cx, deck_center_y, kLegalFortyWellCenterZ),
              Simulation::kIntakeHallDeckEntityId);
        const auto [east_cx, east_hx] = span(well_max_x, deck_max_x);
        fixed(JPH::Vec3(east_hx, kLegalFortyHallDeckHalfThickness, kLegalFortyWellHalfZ),
              JPH::RVec3(east_cx, deck_center_y, kLegalFortyWellCenterZ),
              Simulation::kIntakeHallDeckEntityId);

        // ---- MOD-SKIN-LADDER-S continuation, rungs 16-20 -----------------------
        // Continues AS-001's own rung law north and up to the mid-landing's
        // own height -- see the constants block above for why it stops there
        // rather than climbing on to the hall deck directly.
        for (int rung = kLegalFortySkinRungFirst; rung <= kLegalFortySkinRungLast; ++rung) {
            const float top_y = handoff_surface_y +
                                kIntakeSkinRungRise *
                                    static_cast<float>(rung - kIntakeSkinRungCount);
            const float rung_z = kLegalFortySkinRungFirstZ -
                                 kLegalFortySkinRungStepZMeters *
                                     static_cast<float>(rung - kLegalFortySkinRungFirst);
            const float rung_x = kIntakeSkinCenterX +
                                 (rung > kLegalFortySkinRungFirst ? kLegalFortySkinRungJogX : 0.0F);
            fixed(JPH::Vec3(kIntakeSkinRungHalfX, kIntakeSkinRungHalfY, kIntakeSkinRungHalfZ),
                  JPH::RVec3(rung_x, top_y - kIntakeSkinRungHalfY, rung_z),
                  Simulation::kIntakeSkinEntityId);
        }

        // ---- the walkway from rung 20 to the mid-landing -----------------------
        {
            const auto [walk_cx, walk_hx] =
                span(kLegalFortySkinWalkwayMinX, kLegalFortySkinWalkwayMaxX);
            fixed(JPH::Vec3(walk_hx, kIntakeStairSlabHalfY, kLegalFortySkinWalkwayHalfZ),
                  JPH::RVec3(walk_cx, mid_landing_surface_y - kIntakeStairSlabHalfY,
                             kLegalFortySkinWalkwayCenterZ),
                  Simulation::kIntakeStairEntityId);
        }
    }

    // ---- AS-003 MOD-HOOK5-RACK ------------------------------------------------
    // A cage locked by its height, a door held shut by a bar lying in its
    // swing, and the hook block on a rack inside. No lock object, no flag:
    // see the kHook5* constants for every clearance.
    void build_hook5_rack(JPH::BodyInterface &bodies) {
        const auto box = [this, &bodies](const float x0, const float x1, const float y0,
                                         const float y1, const float z0, const float z1) {
            const JPH::BodyID id = add_box(
                bodies, JPH::Vec3((x1 - x0) * 0.5F, (y1 - y0) * 0.5F, (z1 - z0) * 0.5F),
                JPH::RVec3((x0 + x1) * 0.5F, (y0 + y1) * 0.5F, (z0 + z1) * 0.5F),
                JPH::EMotionType::Static, object_layers::kStatic, 0.7F,
                Simulation::kHook5CageEntityId);
            machine_bodies_.push_back(id);
            return id;
        };
        const float wall_inner_z = kHook5MinZ + kHook5WallThickness;
        // The west buttress, full height: the one face the belt reaches.
        box(kHook5MinX, kHook5ButtressMaxX, 0.0F, kHook5CageTopY, kHook5MinZ, kHook5MaxZ);
        // North and east walls, up to the roof.
        box(kHook5ButtressMaxX, kHook5MaxX, 0.0F, kHook5WallTopY,
            kHook5MaxZ - kHook5WallThickness, kHook5MaxZ);
        box(kHook5MaxX - kHook5WallThickness, kHook5MaxX, 0.0F, kHook5WallTopY, kHook5MinZ,
            kHook5MaxZ - kHook5WallThickness);
        // The south wall either side of the doorway, and the header over it.
        const JPH::BodyID south_west = box(kHook5ButtressMaxX, kHook5DoorwayMinX, 0.0F,
                                           kHook5WallTopY, kHook5MinZ, wall_inner_z);
        box(kHook5DoorwayMaxX, kHook5MaxX - kHook5WallThickness, 0.0F, kHook5WallTopY,
            kHook5MinZ, wall_inner_z);
        box(kHook5DoorwayMinX, kHook5DoorwayMaxX, kHook5DoorwayTopY, kHook5WallTopY, kHook5MinZ,
            wall_inner_z);
        // The roof: four strips around the hatch.
        box(kHook5ButtressMaxX, kHook5HatchMinX, kHook5WallTopY, kHook5CageTopY, kHook5MinZ,
            kHook5MaxZ);
        box(kHook5HatchMaxX, kHook5MaxX, kHook5WallTopY, kHook5CageTopY, kHook5MinZ, kHook5MaxZ);
        box(kHook5HatchMinX, kHook5HatchMaxX, kHook5WallTopY, kHook5CageTopY, kHook5HatchMaxZ,
            kHook5MaxZ);
        box(kHook5HatchMinX, kHook5HatchMaxX, kHook5WallTopY, kHook5CageTopY, kHook5MinZ,
            kHook5HatchMinZ);
        // The bar's brackets: a ledge under each end, and a stop on its north
        // face that rises only 0.02 m past the bar's centreline -- enough to
        // take the door's push, low enough that a lift of 0.14 m clears it.
        const float bar_bottom = kHook5BarCenterY - kHook5BarHalfSection;
        const float bar_north = kHook5BarCenterZ + kHook5BarHalfSection;
        for (const float bracket_x : {kHook5BracketWestX, kHook5BracketEastX}) {
            box(bracket_x - kHook5BracketHalfX, bracket_x + kHook5BracketHalfX, bar_bottom - 0.08F,
                bar_bottom, wall_inner_z, bar_north + 0.10F);
            box(bracket_x - kHook5BracketHalfX, bracket_x + kHook5BracketHalfX, bar_bottom - 0.06F,
                kHook5BarCenterY + 0.02F, bar_north, bar_north + 0.10F);
        }
        // The rack: a pedestal against the east wall.
        box(10.95F, kHook5MaxX - kHook5WallThickness, 0.0F, kHook5RackTopY,
            kHook5BlockSeatZ - 0.45F, kHook5BlockSeatZ + 0.45F);

        // MOD-HOOK5-DOOR, driven open from build time and never commanded
        // again. About -Y, positive rotation carries the leaf's far end toward
        // +Z: into the cage, where the bar lies.
        JPH::Body *door = add_shape_body(
            bodies,
            new JPH::BoxShape(JPH::Vec3(kHook5DoorHalfLength, kHook5DoorHalfHeight,
                                        kHook5DoorHalfThickness)),
            JPH::RVec3(kHook5DoorHingeX + kHook5DoorHalfLength, kHook5DoorCenterY, kHook5DoorZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
            Simulation::kHook5DoorEntityId, kHook5DoorMassKg);
        hook5_door_id_ = door->GetID();
        machine_bodies_.push_back(hook5_door_id_);
        add_vertical_hinge(south_west, hook5_door_id_,
                           JPH::RVec3(kHook5DoorHingeX, kHook5DoorCenterY, kHook5DoorZ), 0.0F,
                           kHook5DoorOpenAngle, kHook5DoorDriveTorqueNm, &hook5_door_hinge_,
                           -JPH::Vec3::sAxisY());
        if (hook5_door_hinge_ != nullptr) {
            hook5_door_hinge_->SetTargetAngularVelocity(kHook5DoorDriveSpeed);
        }

        // MOD-HOOK5-BAR and the hook block: plain bodies, at rest where they sit.
        JPH::Body *bar = add_shape_body(
            bodies,
            new JPH::BoxShape(
                JPH::Vec3(kHook5BarHalfLength, kHook5BarHalfSection, kHook5BarHalfSection)),
            JPH::RVec3(kHook5BarCenterX, kHook5BarCenterY, kHook5BarCenterZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.5F,
            Simulation::kHook5BarEntityId, kHook5BarMassKg);
        hook5_bar_id_ = bar->GetID();
        machine_bodies_.push_back(hook5_bar_id_);
        JPH::Body *block = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(kHook5BlockHalf, kHook5BlockHalfY, kHook5BlockHalf)),
            JPH::RVec3(kHook5BlockSeatX, kHook5BlockSeatY, kHook5BlockSeatZ),
            JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, object_layers::kMoving, 0.6F,
            Simulation::kHook5BlockEntityId, kHook5BlockMassKg);
        hook5_block_id_ = block->GetID();
        machine_bodies_.push_back(hook5_block_id_);
    }

    void build_kernel_sump(JPH::BodyInterface &bodies) {
        const auto track = [this](const JPH::BodyID id) {
            machine_bodies_.push_back(id);
            return id;
        };

        const float deck_y = kSumpPlatformTopY - kSumpDeckHalfThickness;
        track(add_box(bodies,
                      JPH::Vec3(kSumpApproachDeckHalfX, kSumpDeckHalfThickness, kSumpDeckHalfZ),
                      JPH::RVec3(kSumpApproachDeckX, deck_y, kSumpCenterZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                      Simulation::kStaticDeckEntityId));
        track(add_box(bodies, JPH::Vec3(kSumpFarDeckHalfX, kSumpDeckHalfThickness, kSumpDeckHalfZ),
                      JPH::RVec3(kSumpFarDeckX, deck_y, kSumpCenterZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.9F,
                      Simulation::kStaticDeckEntityId));

        // Support legs, purely structural -- under each fixed deck section,
        // clear of the grate span so nothing but the grate itself is ever the
        // question of whether this walkway holds.
        track(add_box(bodies, JPH::Vec3(0.25F, kSumpPlatformTopY * 0.5F, 0.25F),
                      JPH::RVec3(kSumpApproachDeckX, kSumpPlatformTopY * 0.5F, kSumpCenterZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kStaticDeckEntityId));
        track(add_box(bodies, JPH::Vec3(0.25F, kSumpPlatformTopY * 0.5F, 0.25F),
                      JPH::RVec3(kSumpFarDeckX, kSumpPlatformTopY * 0.5F, kSumpCenterZ),
                      JPH::EMotionType::Static, object_layers::kStatic, 0.8F,
                      Simulation::kStaticDeckEntityId));

        JPH::Body *grate = add_shape_body(
            bodies, new JPH::BoxShape(JPH::Vec3(kSumpGrateHalfX, kSumpDeckHalfThickness, kSumpDeckHalfZ)),
            JPH::RVec3(kSumpGrateX, deck_y, kSumpCenterZ), JPH::Quat::sIdentity(),
            JPH::EMotionType::Static, object_layers::kStatic, 0.9F, Simulation::kSumpGrateEntityId,
            0.0F);
        sump_grate_id_ = track(grate->GetID());
        // The sump starts full (existing truth: wet is the default), so the
        // grate starts as a sensor -- see update_sump for why a sensor alone
        // is not the whole mechanism.
        bodies.SetIsSensor(sump_grate_id_, true);
        sump_volume_kg_ = kSumpCapacityKg;
    }

    // Lets the linkage reach its own resting pose before the rope is measured, so
    // slack is slack against the machine as it actually hangs.
    void settle_machine() noexcept {
        for (int step = 0; step < 90; ++step) {
            physics_system_.Update(static_cast<float>(Simulation::kFixedStepSeconds), 1,
                                   &temp_allocator_, &job_system_);
        }
    }

    void add_hinge(const JPH::BodyID anchor_id,
                   const JPH::BodyID moving_id,
                   const JPH::RVec3 point,
                   const float limit_min,
                   const float limit_max,
                   JPH::Ref<JPH::HingeConstraint> *out,
                   const std::uint64_t) {
        JPH::HingeConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = point;
        settings.mPoint2 = point;
        settings.mHingeAxis1 = JPH::Vec3::sAxisZ();
        settings.mHingeAxis2 = JPH::Vec3::sAxisZ();
        settings.mNormalAxis1 = JPH::Vec3::sAxisX();
        settings.mNormalAxis2 = JPH::Vec3::sAxisX();
        settings.mLimitsMin = limit_min;
        settings.mLimitsMax = limit_max;
        JPH::TwoBodyConstraint *constraint = create_constraint(settings, anchor_id, moving_id);
        if (constraint != nullptr && out != nullptr) {
            *out = static_cast<JPH::HingeConstraint *>(constraint);
        }
    }

    void add_slider(const JPH::BodyID anchor_id,
                    const JPH::BodyID moving_id,
                    const float limit_min,
                    const float limit_max) {
        JPH::SliderConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mAutoDetectPoint = true;
        settings.SetSliderAxis(JPH::Vec3::sAxisY());
        settings.mLimitsMin = limit_min;
        settings.mLimitsMax = limit_max;
        (void)create_constraint(settings, anchor_id, moving_id);
    }

    // WO-011. A vertical-axis hinge with a real, torque-limited Jolt motor --
    // the jib's slew. EMotorState::Velocity drives toward a commanded angular
    // velocity "limited only by max force/torque the motor can apply" (Jolt's
    // own doc comment on EMotorState): exceeding that torque does not snap to
    // the target, the body simply cannot reach it. That is the finite-actuator
    // requirement (WO-005 forbidden shortcuts: "unlimited winch force"),
    // enforced by the engine's own constraint solver, not by application code.
    void add_vertical_hinge(const JPH::BodyID anchor_id,
                            const JPH::BodyID moving_id,
                            const JPH::RVec3 point,
                            const float limit_min,
                            const float limit_max,
                            const float max_motor_torque_nm,
                            JPH::Ref<JPH::HingeConstraint> *out,
                            const JPH::Vec3 axis = JPH::Vec3::sAxisY()) {
        JPH::HingeConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = point;
        settings.mPoint2 = point;
        settings.mHingeAxis1 = axis;
        settings.mHingeAxis2 = axis;
        settings.mNormalAxis1 = JPH::Vec3::sAxisX();
        settings.mNormalAxis2 = JPH::Vec3::sAxisX();
        settings.mLimitsMin = limit_min;
        settings.mLimitsMax = limit_max;
        settings.mMotorSettings.SetTorqueLimit(max_motor_torque_nm);
        JPH::TwoBodyConstraint *constraint = create_constraint(settings, anchor_id, moving_id);
        if (constraint == nullptr || out == nullptr) {
            return;
        }
        auto *hinge = static_cast<JPH::HingeConstraint *>(constraint);
        hinge->SetMotorState(JPH::EMotorState::Velocity);
        hinge->SetTargetAngularVelocity(0.0F);
        *out = hinge;
    }

    // A vertical slider with a real, force-limited motor -- the jib's hoist
    // winch, and the capacity-proving stand that shares its rating. Same
    // honesty property as the slew motor: EMotorState::Velocity can only push
    // as hard as mMaxForceLimit, so an overweight load is not held, it sags or
    // falls at a rate the deficit between weight and rated force actually
    // produces -- not scripted, read back from the solver.
    void add_motorized_slider(const JPH::BodyID anchor_id,
                              const JPH::BodyID moving_id,
                              const float limit_min,
                              const float limit_max,
                              const float max_motor_force_n,
                              JPH::Ref<JPH::SliderConstraint> *out) {
        JPH::SliderConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mAutoDetectPoint = true;
        settings.SetSliderAxis(JPH::Vec3::sAxisY());
        settings.mLimitsMin = limit_min;
        settings.mLimitsMax = limit_max;
        settings.mMotorSettings.SetForceLimit(max_motor_force_n);
        JPH::TwoBodyConstraint *constraint = create_constraint(settings, anchor_id, moving_id);
        if (constraint == nullptr || out == nullptr) {
            return;
        }
        auto *slider = static_cast<JPH::SliderConstraint *>(constraint);
        slider->SetMotorState(JPH::EMotorState::Velocity);
        slider->SetTargetVelocity(0.0F);
        *out = slider;
    }

    // A real pin between two bodies at one shared world point -- the hook-to-
    // crate rigging. WO-005 allows the attachment pre-placed for this WO
    // ("CAP-HOOK5 may be pre-placed on the crate... but the hook must still be
    // a real constraint"); a PointConstraint fixes the pin but leaves rotation
    // free, so the crate genuinely swings under the hook rather than being
    // welded to it.
    void add_point_link(const JPH::BodyID first_id,
                        const JPH::BodyID second_id,
                        const JPH::RVec3 point) {
        JPH::PointConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = point;
        settings.mPoint2 = point;
        (void)create_constraint(settings, first_id, second_id);
    }

    // Tension-only rope: it can pull the valve lever but never push it, and it
    // carries deliberate slack so the valve opens a beat after the strike.
    void add_rope() {
        const auto &bodies = physics_system_.GetBodyInterface();
        const JPH::RVec3 tipper_point =
            bodies.GetCenterOfMassTransform(tipper_id_) * JPH::RVec3(3.0, -0.2, 0.0);
        const JPH::RVec3 lever_point =
            bodies.GetCenterOfMassTransform(valve_lever_id_) * JPH::RVec3(-1.60, 0.0, 0.0);
        rope_rest_length_ = JPH::Vec3(lever_point - tipper_point).Length();

        JPH::DistanceConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = tipper_point;
        settings.mPoint2 = lever_point;
        settings.mMinDistance = 0.0F;
        settings.mMaxDistance = rope_rest_length_ + kRopeSlackMeters;
        (void)create_constraint(settings, tipper_id_, valve_lever_id_);
    }

    // WO-010 control cable. A real two-sheave run: pressing the treadle pays out
    // cable on the catwalk side, which must be taken up on the valve side, so the
    // valve lever's counterweight end is hauled up and the orifice opens. Like
    // every rope here it can only pull -- when the player steps off, the treadle
    // is returned by its own counterweight and the valve by its own, not by the
    // cable pushing anything.
    void add_treadle_cable() {
        JPH::PulleyConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mBodyPoint1 =
            JPH::RVec3(kTreadleX - kTreadleCableArm, kTreadleHingeY, kTreadleZ);
        settings.mFixedPoint1 = JPH::RVec3(kTreadleX - kTreadleCableArm,
                                           kTreadleHingeY + kTreadleSheaveRise, kTreadleMastZ);
        settings.mBodyPoint2 = JPH::RVec3(kValveHingeX + kValveCableArm, kValveHingeY + 0.2, -93.0);
        settings.mFixedPoint2 = JPH::RVec3(kValveHingeX + kValveCableArm, kValveSheaveY, kValveMastZ);
        settings.mRatio = 1.0F;
        settings.mMinLength = 0.0F;
        settings.mMaxLength = -1.0F;
        (void)create_constraint(settings, treadle_id_, valve_lever_id_);
    }

    // A tension-only rope over two sheaves: mMinLength=0 lets it go fully
    // slack; mMaxLength caps it (Jolt auto-derives a negative mMaxLength from
    // the configuration's length AT CONSTRUCTION -- PulleyConstraint.cpp's own
    // "Calculate min/max length if it was not provided" -- so an explicit,
    // computed mMaxLength is what makes this a real inextensible rope rather
    // than one silently locked to whatever pose happened to be built).
    // Generalised (AS-002) to take its own four points, bodies, ratio and
    // length, so every rope in this file -- the Kellerworks lift and AS-002's
    // counterweight cradle alike -- is one real JPH::PulleyConstraint, not a
    // copy of this function.
    JPH::PulleyConstraint *add_pulley(const JPH::RVec3 body_point_1,
                                      const JPH::RVec3 fixed_point_1,
                                      const JPH::BodyID body_1,
                                      const JPH::RVec3 body_point_2,
                                      const JPH::RVec3 fixed_point_2,
                                      const JPH::BodyID body_2,
                                      const float ratio,
                                      const float min_length,
                                      const float max_length) {
        JPH::PulleyConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mBodyPoint1 = body_point_1;
        settings.mFixedPoint1 = fixed_point_1;
        settings.mBodyPoint2 = body_point_2;
        settings.mFixedPoint2 = fixed_point_2;
        settings.mRatio = ratio;
        settings.mMinLength = min_length;
        settings.mMaxLength = max_length;
        return static_cast<JPH::PulleyConstraint *>(create_constraint(settings, body_1, body_2));
    }

    // track_for_teardown=false hands ownership entirely to the caller (WO-012
    // needle pins, created and removed at runtime as the seat predicate
    // changes): Jolt's ConstraintManager::Remove asserts on an already-
    // invalidated constraint index, so a constraint that might be removed
    // before the destructor runs must never also sit in machine_constraints_,
    // which is unconditionally removed there once. Exactly one owner removes
    // it, on every path.
    [[nodiscard]] JPH::TwoBodyConstraint *create_constraint(
        const JPH::TwoBodyConstraintSettings &settings,
        const JPH::BodyID first,
        const JPH::BodyID second,
        const bool track_for_teardown = true) {
        // Jolt stripes body mutexes across a fixed-size array, so two distinct
        // bodies can share one. Taking two separate BodyLockWrite locks then
        // deadlocks on a non-recursive mutex ("Resource deadlock avoided").
        // BodyLockMultiWrite sorts and dedupes the mutexes, which is exactly
        // what it exists for. Every constraint pair built before this simply
        // happened not to collide.
        const JPH::BodyID ids[2] = {first, second};
        JPH::BodyLockMultiWrite lock(physics_system_.GetBodyLockInterface(), ids, 2);
        JPH::Body *first_body = lock.GetBody(0);
        JPH::Body *second_body = lock.GetBody(1);
        if (first_body == nullptr || second_body == nullptr) {
            return nullptr;
        }
        JPH::TwoBodyConstraint *constraint = settings.Create(*first_body, *second_body);
        if (constraint == nullptr) {
            return nullptr;
        }
        physics_system_.AddConstraint(constraint);
        if (track_for_teardown) {
            machine_constraints_.emplace_back(constraint);
        }
        return constraint;
    }

    // Kinematic skip-hoist cycle. Every phase is a function of authoritative
    // simulation time, never wall clock, so the loop is deterministic and can be
    // walked into at any point.
    void update_scoop(JPH::BodyInterface &bodies,
                      const float delta_seconds,
                      const double next_time_seconds) noexcept {
        const double phase = std::fmod(next_time_seconds, kMachineCyclePeriodSeconds);
        machine_cycle_phase_seconds_ = phase;

        float height = kScoopBottomY;
        float tilt = 0.0F;
        if (phase < 7.0) {
            height = kScoopBottomY + (kScoopTopY - kScoopBottomY) *
                                         smoothstep(0.0F, 1.0F, static_cast<float>(phase / 7.0));
        } else if (phase < 9.0) {
            height = kScoopTopY;
        } else if (phase < 12.5) {
            height = kScoopTopY;
            tilt = kScoopDischargeTilt *
                   smoothstep(0.0F, 1.0F, static_cast<float>((phase - 9.0) / 3.5));
        } else if (phase < 14.0) {
            height = kScoopTopY;
            tilt = kScoopDischargeTilt;
        } else if (phase < 16.0) {
            height = kScoopTopY + (kScoopBottomY - kScoopTopY) *
                                      smoothstep(0.0F, 1.0F, static_cast<float>((phase - 14.0) / 2.0));
            tilt = kScoopDischargeTilt *
                   (1.0F - smoothstep(0.0F, 1.0F, static_cast<float>((phase - 14.0) / 1.6)));
        }

        scoop_height_ = height;
        scoop_tilt_ = tilt;
        const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), tilt);
        const JPH::RVec3 origin(kScoopX, height, kScoopZ);
        for (int i = 0; i < 4; ++i) {
            bodies.MoveKinematic(scoop_ids_[i], origin + rotation * scoop_local_[i], rotation,
                                 delta_seconds);
        }
    }

    // Reads the real valve lever angle, advances the plant on the same fixed
    // step, and pushes the piston. Presentation never touches any of this.
    void update_plant(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
        float lever_angle = kValveShutAngle;
        if (valve_hinge_ != nullptr) {
            const float measured = valve_hinge_->GetCurrentAngle();
            if (std::isfinite(measured)) {
                lever_angle = measured;
            }
        }
        valve_lever_angle_ = lever_angle;
        if (treadle_hinge_ != nullptr) {
            const float measured = treadle_hinge_->GetCurrentAngle();
            if (std::isfinite(measured)) {
                treadle_angle_ = measured;
            }
        }
        const float span = kValveOpenAngle - kValveShutAngle;
        const double fraction =
            span > 1.0e-4F ? static_cast<double>((lever_angle - kValveShutAngle) / span) : 0.0;
        steam_plant_.set_valve_open_fraction(fraction);
        steam_plant_.step(static_cast<double>(delta_seconds));

        const float piston_force = static_cast<float>(steam_plant_.state().piston_force_n);
        if (piston_force > 0.0F) {
            bodies.AddForce(lift_platform_id_, JPH::Vec3(0.0F, piston_force, 0.0F));
        }
    }

    // WO-011 KX-JIB. Commands take effect only within the pendant station
    // radius (WO-005: "Action to enter station"); away from it, both motors
    // are forced to hold at zero velocity regardless of queued input, so
    // walking off the station always safely brakes the jib rather than
    // leaving it drifting on a stale command.
    void update_jib(const JPH::BodyInterface &bodies,
                    const double slew_input,
                    const double hoist_input) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const float station_dx = static_cast<float>(player_position.GetX()) - kJibStationX;
        const float station_dz = static_cast<float>(player_position.GetZ()) - kJibStationZ;
        const bool at_station =
            (station_dx * station_dx + station_dz * station_dz) <=
            (kJibStationRadius * kJibStationRadius);
        jib_station_active_ = at_station;

        const float slew =
            at_station ? std::clamp(static_cast<float>(slew_input), -1.0F, 1.0F) : 0.0F;
        const float hoist =
            at_station ? std::clamp(static_cast<float>(hoist_input), -1.0F, 1.0F) : 0.0F;

        if (jib_slew_hinge_ != nullptr) {
            jib_slew_hinge_->SetTargetAngularVelocity(slew * kJibSlewMaxRateRadPerSec);
            const float measured = jib_slew_hinge_->GetCurrentAngle();
            if (std::isfinite(measured)) {
                jib_boom_angle_ = measured;
            }
        }
        if (jib_hoist_slider_ != nullptr) {
            jib_hoist_slider_->SetTargetVelocity(hoist * kJibHoistMaxRateMetersPerSec);
        }
    }

    // WO-012 KX-NEEDLE. Same station-gated, continuous-axis contract as
    // update_jib. Seating and unseating are real topology changes -- two
    // PointConstraint pockets added or removed at runtime -- driven entirely
    // by this tick's measured position/speed or command, never a flag.
    void update_needle(const JPH::BodyInterface &bodies, const double hoist_input) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const float station_dx = static_cast<float>(player_position.GetX()) - kNeedleStationX;
        const float station_dz = static_cast<float>(player_position.GetZ()) - kNeedleStationZ;
        const bool at_station =
            (station_dx * station_dx + station_dz * station_dz) <=
            (kNeedleStationRadius * kNeedleStationRadius);
        needle_station_active_ = at_station;

        const float hoist =
            at_station ? std::clamp(static_cast<float>(hoist_input), -1.0F, 1.0F) : 0.0F;
        if (needle_hoist_slider_ != nullptr) {
            needle_hoist_slider_->SetTargetVelocity(hoist * kNeedleHoistMaxRateMetersPerSec);
        }

        // Gated on "not actively raising": right after unseat_needle() removes
        // the pins, the beam is still sitting exactly at the seat pose with
        // near-zero velocity for at least one tick, since the motor needs real
        // time to accelerate it away. Checking the seat predicate unconditionally
        // would re-seat it that same tick, before a held raise command ever got
        // a chance to move it -- found by direct observation (the unseat proof
        // path never actually left the seated state). A held raise is an
        // unambiguous "not trying to seat" signal, so it suppresses the check
        // entirely rather than racing it.
        if (!needle_seated_ && hoist <= 0.0F) {
            const JPH::RVec3 beam_position = bodies.GetPosition(needle_beam_id_);
            const JPH::Vec3 beam_velocity = bodies.GetLinearVelocity(needle_beam_id_);
            const JPH::Vec3 beam_angular_velocity = bodies.GetAngularVelocity(needle_beam_id_);
            const float height_error =
                std::fabs(static_cast<float>(beam_position.GetY()) - kNeedleSeatedY);
            const bool close_enough = height_error <= kNeedleSeatPositionToleranceMeters;
            const bool settled = beam_velocity.Length() <= kNeedleSeatSpeedToleranceMetersPerSec &&
                                 beam_angular_velocity.Length() <= kNeedleSeatSpeedToleranceMetersPerSec;
            if (close_enough && settled) {
                seat_needle();
            }
        } else if (needle_seated_ && hoist > kNeedleUnseatCommandThreshold) {
            unseat_needle();
        }
    }

    // Pins the beam into both piers at the fixed pocket points. The hoist
    // slider is left connected (Governing Law 26 sidestep: no attach/detach
    // system to invent -- see WO-012's design notes), which over-constrains
    // the beam slightly but consistently, since the slider's own rest point
    // already coincides exactly with these pocket points.
    void seat_needle() noexcept {
        JPH::PointConstraintSettings approach_settings;
        approach_settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        approach_settings.mPoint1 =
            JPH::RVec3(kNeedlePocketApproachX, kNeedleSeatedY, kNeedleGapCenterZ);
        approach_settings.mPoint2 = approach_settings.mPoint1;
        needle_pin_approach_ = static_cast<JPH::PointConstraint *>(create_constraint(
            approach_settings, needle_pier_approach_id_, needle_beam_id_, false));

        JPH::PointConstraintSettings far_settings;
        far_settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        far_settings.mPoint1 = JPH::RVec3(kNeedlePocketFarX, kNeedleSeatedY, kNeedleGapCenterZ);
        far_settings.mPoint2 = far_settings.mPoint1;
        needle_pin_far_ = static_cast<JPH::PointConstraint *>(
            create_constraint(far_settings, needle_pier_far_id_, needle_beam_id_, false));

        needle_seated_ = true;
    }

    // Removes both pocket pins. The beam is still hoist-connected, so it does
    // not fall -- the same motor that lowered it now lifts it clear on the
    // next sustained raise, exactly reversing how it was seated.
    void unseat_needle() noexcept {
        if (needle_pin_approach_ != nullptr) {
            physics_system_.RemoveConstraint(needle_pin_approach_);
            needle_pin_approach_ = nullptr;
        }
        if (needle_pin_far_ != nullptr) {
            physics_system_.RemoveConstraint(needle_pin_far_);
            needle_pin_far_ = nullptr;
        }
        needle_seated_ = false;
    }

    // WO-012 checkpoint topology reconciliation: restore_from_checkpoint
    // already restores the beam's continuous transform via restore_body
    // (called just before this); this reconciles the discrete seated/unseated
    // state to match what was actually committed, rather than leaving
    // whatever pins happened to exist at the moment of death. Forbidden-
    // shortcuts list (WO-006): "resetting seat on play-mode restart without
    // going through checkpoint rules" -- this is that checkpoint rule.
    void restore_needle_topology(const bool checkpoint_seated) noexcept {
        if (checkpoint_seated && !needle_seated_) {
            seat_needle();
        } else if (!checkpoint_seated && needle_seated_) {
            unseat_needle();
        }
    }

    // WO-013 KX-SUMP. One lumped volume, one isolation edge, one drain sink,
    // one derived predicate -- updated every authoritative tick, same as
    // every other machine link in this file. The valve toggle is a one-shot
    // Action (WO-006 text: "Player Action may... close a valve only at the
    // real station"), gated by station radius exactly like the jib/needle
    // pendants, but flips a binary state rather than driving a motor.
    // AS-001 MOD-YARD-JIB / MOD-DOG-A. Same station-gated, continuous-axis
    // contract as update_jib. The dog is deliberately absent from the command
    // path: its motor was commanded open at build time and is never touched
    // here, so the only thing that can change the throat is the pack moving.
    void update_intake(const JPH::BodyInterface &bodies,
                       const double slew_input,
                       const double hoist_input) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const float station_dx = static_cast<float>(player_position.GetX()) - kIntakePendantX;
        const float station_dz = static_cast<float>(player_position.GetZ()) - kIntakePendantZ;
        const bool at_station =
            (station_dx * station_dx + station_dz * station_dz) <=
            (kIntakeStationRadius * kIntakeStationRadius);
        intake_station_active_ = at_station;

        const float slew =
            at_station ? std::clamp(static_cast<float>(slew_input), -1.0F, 1.0F) : 0.0F;
        const float hoist =
            at_station ? std::clamp(static_cast<float>(hoist_input), -1.0F, 1.0F) : 0.0F;

        if (intake_slew_hinge_ != nullptr) {
            intake_slew_hinge_->SetTargetAngularVelocity(slew * kIntakeJibSlewRateRadPerSec);
            const float measured = intake_slew_hinge_->GetCurrentAngle();
            if (std::isfinite(measured)) {
                intake_boom_angle_ = measured;
            }
        }
        if (intake_hoist_slider_ != nullptr) {
            intake_hoist_slider_->SetTargetVelocity(hoist * kIntakeJibHoistRateMetersPerSec);
        }

        // Derived predicates, for the HUD and the falsifiers only. Nothing in
        // this simulation branches on either of them: the dog is held by the
        // pack's mass through the solver, and the throat is open or shut
        // because a real body is or is not standing in it.
        if (intake_dog_hinge_ != nullptr) {
            const float measured = intake_dog_hinge_->GetCurrentAngle();
            if (std::isfinite(measured)) {
                intake_dog_angle_ = measured;
            }
        }
        intake_throat_clear_ = intake_dog_angle_ >= kIntakeThroatClearAngle;

        // "In the dog's way" means overlapping the quarter-disc its plate
        // sweeps: within the swing radius of the hinge, and low enough that
        // the plate would strike it.
        const JPH::RVec3 pack = bodies.GetPosition(intake_pack_id_);
        const float pack_dx = static_cast<float>(pack.GetX()) - kIntakeDogHingeX;
        const float pack_dz = static_cast<float>(pack.GetZ()) - kIntakeBayFrontZ;
        const float swing_radius = 2.0F * kIntakeDogHalfWidth + kIntakePackHalfZ;
        intake_pack_pins_dog_ =
            (pack_dx * pack_dx + pack_dz * pack_dz) <= (swing_radius * swing_radius) &&
            static_cast<float>(pack.GetY()) - kIntakePackHalfY < kIntakeThroatHeight;
    }

    // AS-002 MOD-STAIR-A-SWING / MOD-CW-CRADLE. Reads the hinge every tick
    // regardless of station (the flight keeps swinging under gravity/rope
    // tension whether or not the player is anywhere near the pendant); the
    // sling commands are gated on intake_station_active_, which update_intake
    // -- called immediately before this every tick -- has just refreshed.
    // Release and Attach are one-shot, exactly like the sump's valve toggle,
    // gated additionally on physical tolerance: a command outside tolerance
    // is accepted but produces no state change, rather than a pose-only
    // predicate silently re-firing the instant geometry happens to line up
    // -- WO-012's seat_needle/unseat_needle race, generalised in advance.
    void update_legal_forty(const JPH::BodyInterface &bodies,
                            const bool release_requested,
                            const bool attach_requested) noexcept {
        if (intake_swing_hinge_ != nullptr) {
            const float measured = intake_swing_hinge_->GetCurrentAngle();
            if (std::isfinite(measured)) {
                // Jolt reports this NEGATIVE as the flight opens -- see
                // kIntakeSwingHingeTravelRadians -- so travel is its negation.
                intake_swing_travel_ =
                    std::clamp(-measured, 0.0F, kIntakeSwingHingeTravelRadians);
            }
        }

        if (!intake_station_active_) {
            return;
        }
        if (release_requested && intake_pack_slung_) {
            release_pack_to_cradle(bodies);
        }
        if (attach_requested && !intake_pack_slung_) {
            sling_pack(bodies);
        }
    }

    // Shared by release_pack_to_cradle (may release fire) and
    // update_legal_forty's retract nudge (is the cradle genuinely unloaded)
    // -- the same seated-pose tolerance, asked from two directions.
    bool intake_pack_seated_on_cradle(const JPH::BodyInterface &bodies) const noexcept {
        const JPH::RVec3 pack_position = bodies.GetPosition(intake_pack_id_);
        const JPH::RVec3 cradle_position = bodies.GetPosition(intake_cw_cradle_id_);
        // The pack's CENTRE when resting on the cradle floor sits one pack
        // half-height above the cradle's own top face -- not at the top face
        // itself. Found by direct observation: comparing pack.y against the
        // bare top face read a settled, correctly-resting pack as 0.96 m out
        // of tolerance.
        const JPH::RVec3 cradle_seat(
            cradle_position.GetX(),
            cradle_position.GetY() + kIntakeCwCradleHalfY + kIntakePackHalfY,
            cradle_position.GetZ());
        const auto dx = static_cast<float>(pack_position.GetX() - cradle_seat.GetX());
        const auto dz = static_cast<float>(pack_position.GetZ() - cradle_seat.GetZ());
        const auto dy = static_cast<float>(pack_position.GetY() - cradle_seat.GetY());
        const bool xz_in_tolerance =
            (dx * dx + dz * dz) <=
            (kLegalFortyReleaseXZToleranceMeters * kLegalFortyReleaseXZToleranceMeters);
        const bool y_in_tolerance = std::fabs(dy) <= kLegalFortyReleaseYToleranceMeters;
        return xz_in_tolerance && y_in_tolerance;
    }

    // Removes the hook-pack sling once the pack is physically seated on
    // MOD-CW-CRADLE and settled. The rope-tension jump this causes -- read
    // back from the solver on the next tick, never asserted here -- is the
    // whole mechanism; this function's only job is deciding whether a real
    // release may happen right now.
    void release_pack_to_cradle(const JPH::BodyInterface &bodies) noexcept {
        const bool settled = bodies.GetLinearVelocity(intake_pack_id_).Length() <=
                             kLegalFortyReleaseSpeedToleranceMps;
        if (!intake_pack_seated_on_cradle(bodies) || !settled) {
            return;
        }
        remove_sling_pin();
    }

    // Re-creates the hook-pack sling once the hook is physically back over
    // the pack's padeye and settled. Exact mirror of release_pack_to_cradle.
    void sling_pack(const JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 hook_position = bodies.GetPosition(intake_hook_id_);
        const JPH::RVec3 pack_position = bodies.GetPosition(intake_pack_id_);
        const JPH::RVec3 padeye(pack_position.GetX(), pack_position.GetY() + kIntakePackHalfY,
                                pack_position.GetZ());
        const auto distance = static_cast<float>(JPH::Vec3(hook_position - padeye).Length());
        const bool in_tolerance = distance <= kLegalFortyAttachToleranceMeters;
        const bool settled = bodies.GetLinearVelocity(intake_hook_id_).Length() <=
                             kLegalFortyAttachSpeedToleranceMps;
        if (!in_tolerance || !settled) {
            return;
        }
        create_sling_pin(hook_position);
    }

    // Raw, unconditional creation/removal -- shared by the tolerance-gated
    // sling_pack()/release_pack_to_cradle() command path, the initial build
    // in build_intake_rise, and restore_legal_forty_topology()'s checkpoint
    // reconciliation, which must NOT re-apply the tolerance gate against
    // bodies restore_from_checkpoint does not reposition (the pack/hook/dog
    // are an AS-001 gap this ticket does not reopen -- see MachineCheckpoint).
    void create_sling_pin(const JPH::RVec3 point) noexcept {
        JPH::PointConstraintSettings sling_settings;
        sling_settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        sling_settings.mPoint1 = point;
        sling_settings.mPoint2 = point;
        intake_sling_pin_ = static_cast<JPH::PointConstraint *>(
            create_constraint(sling_settings, intake_hook_id_, intake_pack_id_, false));
        intake_pack_slung_ = true;
    }

    void remove_sling_pin() noexcept {
        if (intake_sling_pin_ != nullptr) {
            physics_system_.RemoveConstraint(intake_sling_pin_);
            intake_sling_pin_ = nullptr;
        }
        intake_pack_slung_ = false;
    }

    // WO-012 checkpoint topology reconciliation, generalised: restore the
    // discrete sling presence to match what was actually committed, rather
    // than leaving whatever pin happened to exist at the moment of death.
    void restore_legal_forty_topology(const bool checkpoint_slung) noexcept {
        if (checkpoint_slung && !intake_pack_slung_) {
            create_sling_pin(physics_system_.GetBodyInterface().GetPosition(intake_hook_id_));
        } else if (!checkpoint_slung && intake_pack_slung_) {
            remove_sling_pin();
        }
    }

    void update_sump(JPH::BodyInterface &bodies, const float delta_seconds,
                     const bool valve_toggle_requested) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const float station_dx = static_cast<float>(player_position.GetX()) - kSumpStationX;
        const float station_dz = static_cast<float>(player_position.GetZ()) - kSumpStationZ;
        const bool at_station = (station_dx * station_dx + station_dz * station_dz) <=
                                (kSumpStationRadius * kSumpStationRadius);
        sump_station_active_ = at_station;

        if (valve_toggle_requested && at_station) {
            sump_isolated_ = !sump_isolated_;
        }

        const float inflow = sump_isolated_ ? 0.0F : kSumpInflowKgPerSec;
        sump_volume_kg_ += (inflow - kSumpDrainKgPerSec) * delta_seconds;
        sump_volume_kg_ = std::clamp(sump_volume_kg_, 0.0F, kSumpCapacityKg);

        const bool grate_safe = sump_volume_kg_ <= 0.0F;
        // Set every tick, not just on transition: a plain bool flag, cheap to
        // reassert, and it removes any chance of a missed-edge desync between
        // grate_safe_ and the body's actual sensor state.
        bodies.SetIsSensor(sump_grate_id_, !grate_safe);
        grate_safe_ = grate_safe;
    }

    [[nodiscard]] JPH::Vec3 current_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept {
        const JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
        if (support_id.IsInvalid()) {
            return JPH::Vec3::sZero();
        }
        return bodies.GetPointVelocity(
            support_id,
            JPH::RVec3(support_sample_.contact_point.x,
                       support_sample_.contact_point.y,
                       support_sample_.contact_point.z));
    }

    void update_support_motion(JPH::BodyInterface &bodies,
                               const float delta_seconds,
                               const double next_time_seconds) noexcept {
        const double translating_x =
            kTranslatingSupportAmplitudeMeters *
            std::sin(kTranslatingSupportAngularFrequency * next_time_seconds);
        bodies.MoveKinematic(translating_support_id_,
                             JPH::RVec3(translating_x, 0.25, 8.0),
                             JPH::Quat::sIdentity(),
                             delta_seconds);

        rotating_support_yaw_radians_ =
            std::fmod(kRotatingSupportAngularSpeed * next_time_seconds, 2.0 * kPi);
        bodies.MoveKinematic(
            rotating_support_id_,
            JPH::RVec3(-8.0, 0.25, 0.0),
            JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F),
                                 static_cast<float>(rotating_support_yaw_radians_)),
            delta_seconds);

        const double moving_ledge_z =
            kMovingLedgeCenterZ +
            kMovingLedgeAmplitudeMeters *
                std::sin(kMovingLedgeAngularFrequency * next_time_seconds);
        bodies.MoveKinematic(moving_ledge_id_,
                             JPH::RVec3(9.0, 1.8, moving_ledge_z),
                             JPH::Quat::sIdentity(),
                             delta_seconds);

        // AS-001 MOD-INTAKE-BELT: an 18 m translating slat deck. Kinematic and
        // in the moving-support set, so riding it inherits its velocity under
        // the WO-002 support-point law rather than being scenery you slide on.
        const double belt_z =
            kIntakeBeltCenterZ +
            0.5 * kIntakeBeltStrokeMeters *
                std::sin(kIntakeBeltAngularFrequency * next_time_seconds);
        bodies.MoveKinematic(
            intake_belt_id_,
            JPH::RVec3(kIntakeBeltX, kIntakeBeltTopY - kIntakeBeltHalfY, belt_z),
            JPH::Quat::sIdentity(),
            delta_seconds);
    }

    // ---- geometry probes -------------------------------------------------

    [[nodiscard]] bool cast_ray(const JPH::RVec3 origin,
                                const JPH::Vec3 direction,
                                JPH::RayCastResult &hit) const noexcept {
        const JPH::RRayCast ray(origin, direction);
        hit.Reset();
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        return physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter);
    }

    // A ray for a wall. With see_past_holds, members a hand could close
    // round (rungs, pipes, bars) are seen through, the way a climber on a
    // ladder sees the wall behind it.
    [[nodiscard]] bool cast_wall(const JPH::RVec3 origin, const JPH::Vec3 direction,
                                 const bool see_past_holds, JPH::RayCastResult &hit,
                                 JPH::RVec3 &point) const noexcept {
        const float length = direction.Length();
        if (length <= 0.0F) {
            return false;
        }
        const JPH::Vec3 unit = direction / length;
        JPH::RVec3 from = origin;
        float left = length;
        for (std::uint32_t pass = 0; pass < 4; ++pass) {
            const JPH::Vec3 ray = unit * left;
            if (!cast_ray(from, ray, hit)) {
                return false;
            }
            point = JPH::RRayCast(from, ray).GetPointOnRay(hit.mFraction);
            if (!see_past_holds || !is_hold(hit.mBodyID, hit.mSubShapeID2)) {
                return true;
            }
            constexpr float kPastHold = 0.20F;
            left -= hit.mFraction * left + kPastHold;
            if (left <= 0.0F) {
                return false;
            }
            from = point + unit * kPastHold;
        }
        return false;
    }

    [[nodiscard]] bool is_hold(const JPH::BodyID body_id,
                               const JPH::SubShapeID &sub_shape_id) const noexcept {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
        LeafBox box;
        std::uint32_t long_axis = 0;
        return lock.Succeeded() && leaf_box(lock.GetBody(), sub_shape_id, box) &&
               box_is_hold(box, long_axis);
    }

    [[nodiscard]] JPH::Vec3 surface_normal(const JPH::BodyID body_id,
                                           const JPH::SubShapeID &sub_shape_id,
                                           const JPH::RVec3 point) const noexcept {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
        if (!lock.Succeeded()) {
            return JPH::Vec3::sZero();
        }
        return lock.GetBody().GetWorldSpaceSurfaceNormal(sub_shape_id, point);
    }

    // Firm footing, for a checkpoint (kCheckpointFootingSlack): a ray straight
    // down from the body's centre meets a walkable surface within reach. The
    // carried body is not footing, whatever it hangs over.
    [[nodiscard]] bool footing_is_firm(const JPH::BodyInterface &bodies) const noexcept {
        const float reach =
            (crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight) + kCheckpointFootingSlack;
        const JPH::RRayCast ray(bodies.GetPosition(player_id_), JPH::Vec3(0.0F, -reach, 0.0F));
        JPH::RayCastResult hit;
        const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
        const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
        if (!physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter)) {
            return false;
        }
        return surface_normal(hit.mBodyID, hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction))
                   .GetY() >= kSupportNormalThreshold;
    }

    // The capsule the body has now: standing, or crouched.
    [[nodiscard]] const JPH::Shape *active_player_shape() const noexcept {
        return crouched_ ? player_crouch_shape_.GetPtr() : player_shape_.GetPtr();
    }

    [[nodiscard]] bool capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept {
        return shape_pose_is_clear(active_player_shape(), centre);
    }

    [[nodiscard]] bool shape_pose_is_clear(const JPH::Shape *shape,
                                           const JPH::RVec3 centre) const noexcept {
        JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
        JPH::CollideShapeSettings settings;
        settings.mMaxSeparationDistance = 0.0F;
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        physics_system_.GetNarrowPhaseQuery().CollideShape(shape,
                                                           JPH::Vec3::sReplicate(1.0F),
                                                           JPH::RMat44::sTranslation(centre),
                                                           settings,
                                                           centre,
                                                           collector,
                                                           {},
                                                           {},
                                                           body_filter);
        return !collector.HadHit();
    }

    // Finds a ledge in front of `origin`. Every returned field comes from a real
    // cast against the authoritative world; a failed reach, a missing top
    // surface, a too-steep top, an out-of-band rise, an unsupported landing, or
    // an obstructed landing pose all return an invalid probe.
    [[nodiscard]] LedgeProbe probe_ledge(const JPH::RVec3 origin,
                                         const JPH::Vec3 facing,
                                         const float minimum_rise,
                                         const float maximum_rise,
                                         const bool require_supported_landing,
                                         const bool see_past_holds = false) const noexcept {
        LedgeProbe probe;
        if (facing.IsNearZero()) {
            return probe;
        }

        JPH::RayCastResult wall_hit;
        JPH::RVec3 wall_point;
        if (!cast_wall(origin, facing * (kTraversalReach + kPlayerRadius), see_past_holds, wall_hit,
                       wall_point)) {
            return probe;
        }

        const float feet_y = origin.GetY() - kPlayerHalfHeight;
        const JPH::RVec3 top_origin(wall_point.GetX() + facing.GetX() * kTopProbeInset,
                                    feet_y + maximum_rise + kTopProbeMargin,
                                    wall_point.GetZ() + facing.GetZ() * kTopProbeInset);
        const float top_ray_length = maximum_rise + kTopProbeMargin - minimum_rise;
        if (top_ray_length <= 0.0F) {
            return probe;
        }
        const JPH::Vec3 top_direction(0.0F, -top_ray_length, 0.0F);

        JPH::RayCastResult top_hit;
        if (!cast_ray(top_origin, top_direction, top_hit)) {
            return probe;
        }
        if (top_hit.mBodyID != wall_hit.mBodyID) {
            return probe;
        }
        const JPH::RVec3 ledge_point =
            JPH::RRayCast(top_origin, top_direction).GetPointOnRay(top_hit.mFraction);
        if (surface_normal(top_hit.mBodyID, top_hit.mSubShapeID2, ledge_point).GetY() <
            kLedgeTopNormalThreshold) {
            return probe;
        }

        const float rise = ledge_point.GetY() - feet_y;
        if (rise < minimum_rise || rise > maximum_rise) {
            return probe;
        }

        const JPH::RVec3 landing_centre(wall_point.GetX() + facing.GetX() * kLandingInset,
                                        ledge_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                                        wall_point.GetZ() + facing.GetZ() * kLandingInset);

        JPH::BodyID landing_body = top_hit.mBodyID;
        if (require_supported_landing) {
            const JPH::RVec3 support_origin(landing_centre.GetX(),
                                            ledge_point.GetY() + kLandingSupportProbeUp,
                                            landing_centre.GetZ());
            const JPH::Vec3 support_direction(
                0.0F, -(kLandingSupportProbeUp + kLandingSupportTolerance), 0.0F);
            JPH::RayCastResult landing_hit;
            if (!cast_ray(support_origin, support_direction, landing_hit)) {
                return probe;
            }
            if (landing_hit.mBodyID != top_hit.mBodyID) {
                return probe;
            }
            landing_body = landing_hit.mBodyID;
        }

        if (!capsule_pose_is_clear(landing_centre)) {
            return probe;
        }

        const auto &bodies = physics_system_.GetBodyInterface();
        probe.valid = true;
        probe.ledge_body = top_hit.mBodyID;
        probe.ledge_entity_id = bodies.GetUserData(top_hit.mBodyID);
        probe.wall_point = wall_point;
        probe.ledge_point = ledge_point;
        probe.landing_centre = landing_centre;
        probe.landing_body = landing_body;
        probe.landing_entity_id = bodies.GetUserData(landing_body);
        probe.rise = rise;
        return probe;
    }

    // Finds the far-side landing that makes an obstacle vaultable rather than
    // mantleable. Without a real, clear landing beyond the obstacle there is no
    // vault.
    [[nodiscard]] bool probe_vault_landing(const LedgeProbe &obstacle,
                                           const JPH::Vec3 facing,
                                           const float feet_y,
                                           JPH::RVec3 &landing_centre,
                                           JPH::BodyID &landing_body) const noexcept {
        const JPH::RVec3 far_origin(
            obstacle.wall_point.GetX() + facing.GetX() * kVaultCrossDistance,
            obstacle.ledge_point.GetY() + 0.40F,
            obstacle.wall_point.GetZ() + facing.GetZ() * kVaultCrossDistance);
        const float drop_length = obstacle.ledge_point.GetY() + 0.40F - (feet_y - kVaultMaximumDrop);
        if (drop_length <= 0.0F) {
            return false;
        }
        const JPH::Vec3 drop_direction(0.0F, -drop_length, 0.0F);

        JPH::RayCastResult landing_hit;
        if (!cast_ray(far_origin, drop_direction, landing_hit)) {
            return false;
        }
        if (landing_hit.mBodyID == obstacle.ledge_body) {
            return false;
        }
        const JPH::RVec3 landing_point =
            JPH::RRayCast(far_origin, drop_direction).GetPointOnRay(landing_hit.mFraction);
        if (surface_normal(landing_hit.mBodyID, landing_hit.mSubShapeID2, landing_point).GetY() <
            kLedgeTopNormalThreshold) {
            return false;
        }

        const JPH::RVec3 candidate(landing_point.GetX(),
                                   landing_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                                   landing_point.GetZ());
        if (!capsule_pose_is_clear(candidate)) {
            return false;
        }

        landing_centre = candidate;
        landing_body = landing_hit.mBodyID;
        return true;
    }

    // ---- support-frame helpers ------------------------------------------

    [[nodiscard]] JPH::Vec3 to_support_local(const JPH::BodyInterface &bodies,
                                             const JPH::BodyID body_id,
                                             const JPH::RVec3 world_point) const noexcept {
        if (body_id.IsInvalid()) {
            return JPH::Vec3(world_point);
        }
        return JPH::Vec3(bodies.GetCenterOfMassTransform(body_id).Inversed() * world_point);
    }

    [[nodiscard]] JPH::RVec3 from_support_local(const JPH::BodyInterface &bodies,
                                                const JPH::BodyID body_id,
                                                const JPH::Vec3 local_point) const noexcept {
        if (body_id.IsInvalid()) {
            return JPH::RVec3(local_point);
        }
        return bodies.GetCenterOfMassTransform(body_id) * JPH::RVec3(local_point);
    }

    // ---- traversal state machine ----------------------------------------

    // ---- AS-003 carry ---------------------------------------------------------
    // Where the carried body's handle is held, relative to the player's
    // centre: forward along a horizontal bearing, a little above the centre.
    [[nodiscard]] static JPH::Vec3 carry_hand_offset(const JPH::Vec3 bearing) noexcept {
        return bearing * kCarryHandForward + JPH::Vec3(0.0F, kCarryHandUp, 0.0F);
    }

    // The bearing the hands hold the load on this tick: the load's present
    // bearing from the body, turned toward the facing by at most
    // kCarryTurnRadiansPerSecond. Derived from poses, so it adds no state.
    [[nodiscard]] JPH::Vec3 carry_hand_bearing(const JPH::BodyInterface &bodies,
                                               const float delta_seconds) const noexcept {
        const JPH::Vec3 to_handle(
            bodies.GetCenterOfMassTransform(carried_id_) * carry_handle(carried_entity_) -
            bodies.GetPosition(player_id_));
        const JPH::Vec3 flat(to_handle.GetX(), 0.0F, to_handle.GetZ());
        if (flat.LengthSq() < 1.0e-4F) {
            return facing_;
        }
        const JPH::Vec3 bearing = flat.Normalized();
        if (facing_.IsNearZero()) {
            return bearing;
        }
        // Signed angle from the bearing to the facing about +Y.
        const float sine = bearing.GetZ() * facing_.GetX() - bearing.GetX() * facing_.GetZ();
        const float angle = std::atan2(sine, bearing.Dot(facing_));
        const float limit = kCarryTurnRadiansPerSecond * delta_seconds;
        return JPH::Quat::sRotation(JPH::Vec3::sAxisY(), std::clamp(angle, -limit, limit)) *
               bearing;
    }

    // The middle of the carried body's top face, in its own frame; a kit
    // body's handle is where its build put it.
    [[nodiscard]] JPH::Vec3 carry_handle(const std::uint64_t entity) const noexcept {
        if (scraperx::sim::kit::is_kit_entity(entity)) {
            return kit_->carry_handle(entity);
        }
        return JPH::Vec3(0.0F,
                         entity == Simulation::kHook5BarEntityId ? kHook5BarHalfSection
                                                                 : kHook5BlockHalfY,
                         0.0F);
    }

    // What a pick-up would take now: the nearest carryable within reach of the
    // body's centre, at rest, not behind it -- and only for a grounded body
    // outside a traversal with its hands free.
    JPH::BodyID carry_candidate(const JPH::BodyInterface &bodies,
                                std::uint64_t &entity) const noexcept {
        entity = 0;
        JPH::BodyID best;
        if (carry_constraint_ != nullptr || !grounded_ ||
            traversal_state_ != TraversalState::None || facing_.IsNearZero()) {
            return best;
        }
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        float best_distance = kCarryReach;
        const std::pair<JPH::BodyID, std::uint64_t> carryables[] = {
            {hook5_bar_id_, Simulation::kHook5BarEntityId},
            {hook5_block_id_, Simulation::kHook5BlockEntityId},
        };
        const auto consider = [&](const JPH::BodyID id, const std::uint64_t candidate_entity) {
            const JPH::Vec3 to_body(bodies.GetCenterOfMassPosition(id) - at);
            const float distance = to_body.Length();
            if (distance > best_distance ||
                bodies.GetLinearVelocity(id).Length() > kCarryMaxBodySpeed) {
                return;
            }
            const JPH::Vec3 flat(to_body.GetX(), 0.0F, to_body.GetZ());
            if (flat.Dot(facing_) < 0.0F) {
                return;
            }
            best = id;
            best_distance = distance;
            entity = candidate_entity;
        };
        for (const auto &[id, candidate_entity] : carryables) {
            consider(id, candidate_entity);
        }
        kit_->carry_candidates(kit_carryables_);
        for (const auto &candidate : kit_carryables_) {
            consider(candidate.id, candidate.entity);
        }
        return best;
    }

    // A point constraint between the hands and the body's handle. Rotation is
    // left free, so a block hangs and swings from it.
    void attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept {
        JPH::PointConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
        settings.mPoint1 = JPH::RVec3(carry_hand_offset(facing_));
        settings.mPoint2 = JPH::RVec3(carry_handle(entity));
        carry_constraint_ = static_cast<JPH::PointConstraint *>(
            create_constraint(settings, player_id_, id, false));
        if (carry_constraint_ == nullptr) {
            return;
        }
        carried_id_ = id;
        carried_entity_ = entity;
        grip_over_seconds_ = 0.0F;
        contact_listener_.set_carried_entity(entity);
    }

    // Removes the constraint and nothing else: the body falls and rests.
    void release_carry() noexcept {
        if (carry_constraint_ != nullptr) {
            physics_system_.RemoveConstraint(carry_constraint_);
            carry_constraint_ = nullptr;
        }
        carried_id_ = JPH::BodyID();
        carried_entity_ = 0;
        contact_listener_.set_carried_entity(0);
    }

    void update_carry(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept {
        if (carry_constraint_ != nullptr) {
            if (commands.set_down_requested) {
                release_carry();
                return;
            }
            // The hands swing round to the facing.
            const JPH::Vec3 offset = carry_hand_offset(carry_hand_bearing(bodies, delta_seconds));
            carry_constraint_->SetPoint1(JPH::EConstraintSpace::LocalToBodyCOM,
                                         JPH::RVec3(offset));
            const JPH::RVec3 hand = bodies.GetPosition(player_id_) + offset;
            const JPH::RVec3 handle =
                bodies.GetCenterOfMassTransform(carried_id_) * carry_handle(carried_entity_);
            const JPH::Vec3 apart(handle - hand);
            const JPH::Vec3 separating_velocity = bodies.GetPointVelocity(carried_id_, handle) -
                                                  bodies.GetLinearVelocity(player_id_);
            if (apart.Length() > kCarrySlipDistance && apart.Dot(separating_velocity) > 0.0F) {
                release_carry();
                return;
            }
            // The grip: the force the hands put through the carry on the
            // last step.
            if (scraperx::sim::kit::is_kit_entity(carried_entity_)) {
                const float pull =
                    carry_constraint_->GetTotalLambdaPosition().Length() / delta_seconds;
                grip_over_seconds_ = pull > kGripNewtons ? grip_over_seconds_ + delta_seconds : 0.0F;
                if (grip_over_seconds_ >= kGripSeconds) {
                    release_carry();
                }
            }
            return;
        }
        if (commands.pick_up_requested) {
            std::uint64_t entity = 0;
            const JPH::BodyID id = carry_candidate(bodies, entity);
            if (!id.IsInvalid()) {
                attach_carry(id, entity);
            }
        }
    }

    // Checkpoint reconciliation, the restore_needle_topology shape: after the
    // bodies' poses are restored, make the carry match what was committed.
    void restore_carry_topology(const std::uint64_t committed_entity) noexcept {
        if (committed_entity == carried_entity_) {
            return;
        }
        release_carry();
        if (committed_entity == Simulation::kHook5BarEntityId) {
            attach_carry(hook5_bar_id_, committed_entity);
        } else if (committed_entity == Simulation::kHook5BlockEntityId) {
            attach_carry(hook5_block_id_, committed_entity);
        } else if (scraperx::sim::kit::is_kit_entity(committed_entity)) {
            const scraperx::sim::kit::BodyIndex body = kit_->body_for_entity(committed_entity);
            if (kit_->body_enabled(body)) {
                attach_carry(kit_->body_id(body), committed_entity);
            }
        }
    }

    // ---- AS-006 rigging ------------------------------------------------------
    // Where the hands are this tick: the carry point, whether or not anything
    // is held.
    [[nodiscard]] JPH::RVec3 hand_position(const JPH::BodyInterface &bodies) const noexcept {
        return bodies.GetPosition(player_id_) + carry_hand_offset(facing_);
    }

    // What the rig command would do now, from poses and the carry alone.
    void find_rig_action(const JPH::BodyInterface &bodies) noexcept {
        rig_action_ = 0;
        rig_target_entity_ = 0;
        rig_anchor_ = scraperx::sim::kit::AnchorIndex{};
        rig_rope_ = scraperx::sim::kit::RopeIndex{};
        if (traversal_state_ != TraversalState::None) {
            return;
        }
        if (carry_constraint_ != nullptr) {
            if (kit_->carry_kind(carried_entity_) != scraperx::sim::kit::CarryKind::Shackle) {
                return;
            }
            rig_anchor_ = kit_->hook_target(carried_entity_);
            if (rig_anchor_.valid()) {
                rig_action_ = 1;
                rig_target_entity_ = kit_->anchor_entity(rig_anchor_);
            }
            return;
        }
        if (!grounded_) {
            return;
        }
        rig_rope_ = kit_->unhook_target(hand_position(bodies));
        if (rig_rope_.valid()) {
            rig_action_ = 2;
            rig_target_entity_ = kit_->rope_shackle_entity(rig_rope_);
        }
    }

    // Hook: the shackle leaves the hands and the rope's end goes onto the
    // anchor. Unhook: the end comes off the anchor onto its shackle, in the
    // hands.
    void update_rig(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
        if (!commands.rig_requested) {
            return;
        }
        find_rig_action(bodies);
        if (rig_action_ == 1) {
            const std::uint64_t shackle = carried_entity_;
            release_carry();
            (void)kit_->hook(shackle, rig_anchor_);
        } else if (rig_action_ == 2) {
            const std::uint64_t shackle = kit_->unhook(rig_rope_);
            const scraperx::sim::kit::BodyIndex body = kit_->body_for_entity(shackle);
            if (kit_->body_enabled(body)) {
                attach_carry(kit_->body_id(body), shackle);
            }
        }
        find_rig_action(bodies);
    }

    // Crouch and stand (GDD 7.2, Governing Law 4). The body swaps capsules
    // with its soles fixed: the centre moves by kCrouchDrop, nothing under
    // the feet does. It crouches only from the ground and never inside a
    // traversal (every traversal pose is a standing pose), and it stands
    // only where the full standing capsule is clear, so a gap entered
    // crouched keeps the body crouched until it is out of it. A Jump or a
    // traversal request asks to stand first; where the body cannot, they
    // are refused further down this tick.
    void update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
        if (traversal_state_ != TraversalState::None) {
            return;
        }
        const bool wants_up =
            !commands.crouch_held || commands.jump_requested || commands.traversal_requested;
        if (crouched_) {
            if (wants_up) {
                (void)try_stand(bodies);
            }
            return;
        }
        if (!wants_up && grounded_) {
            const JPH::RVec3 at = bodies.GetPosition(player_id_);
            bodies.SetShape(player_id_, player_crouch_shape_.GetPtr(), false,
                            JPH::EActivation::Activate);
            bodies.SetPosition(player_id_, at - JPH::Vec3(0.0F, kCrouchDrop, 0.0F),
                               JPH::EActivation::Activate);
            crouched_ = true;
        }
    }

    [[nodiscard]] bool try_stand(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 standing =
            bodies.GetPosition(player_id_) + JPH::Vec3(0.0F, kCrouchDrop, 0.0F);
        if (!shape_pose_is_clear(player_shape_.GetPtr(),
                                 standing + JPH::Vec3(0.0F, kStandClearanceSkin, 0.0F))) {
            return false;
        }
        bodies.SetShape(player_id_, player_shape_.GetPtr(), false, JPH::EActivation::Activate);
        bodies.SetPosition(player_id_, standing, JPH::EActivation::Activate);
        crouched_ = false;
        return true;
    }

    void apply_traversal_commands(JPH::BodyInterface &bodies,
                                  const StepCommands &commands) noexcept {
        if (traversal_state_ == TraversalState::Hanging) {
            if (commands.release_requested) {
                release_hang(bodies);
            } else if (commands.jump_requested || commands.traversal_requested) {
                begin_mantle_from_hang(bodies);
            }
            return;
        }

        if (traversal_state_ == TraversalState::Climbing) {
            if (commands.release_requested) {
                let_go_climb(bodies);
            } else if (commands.jump_requested) {
                jump_off_climb(bodies);
            } else if (commands.traversal_requested && !try_top_out(bodies)) {
                ++rejected_traversal_count_;
            }
            return;
        }

        if (traversal_state_ != TraversalState::None) {
            if (commands.traversal_requested || commands.release_requested) {
                ++rejected_traversal_count_;
            }
            return;
        }

        // Still crouched here means update_crouch could not stand the body:
        // every vault, mantle and hang is a standing pose, so none begins.
        if (crouched_) {
            jump_vault_ticks_left_ = 0;
            if (commands.traversal_requested) {
                ++rejected_traversal_count_;
            }
            return;
        }

        // Both hands on a carried body (AS-003 §8.4.3): vault, mantle and hang
        // are all pull-ups, and none begins until it is set down.
        if (carry_constraint_ != nullptr) {
            jump_vault_ticks_left_ = 0;
            if (commands.traversal_requested) {
                ++rejected_traversal_count_;
            }
            return;
        }

        if (jump_vault_ticks_left_ > 0) {
            --jump_vault_ticks_left_;
            if (commands.jump_requested && try_begin_jump_vault(bodies)) {
                jump_vault_ticks_left_ = 0;
                return;
            }
        }

        // Drop with an edge behind: lower over it into a hang.
        if (commands.release_requested && grounded_) {
            if (!try_begin_lowering(bodies)) {
                ++rejected_traversal_count_;
            }
            return;
        }

        // Action: a vault or mantle where one is offered, else a hold faced
        // at hand height is climbed.
        if (commands.traversal_requested && !try_begin_ground_traversal(bodies)) {
            const Grip grip = grounded_ ? grip_in_front(bodies) : Grip{};
            if (grip.valid) {
                begin_climb(bodies, grip);
            } else {
                ++rejected_traversal_count_;
            }
        }
    }

    // The vault the takeoff could have been: probed from the takeoff floor at
    // the body's current plan position, with the same rise band and the same
    // far-side landing test as a vault started on the ground.
    [[nodiscard]] bool try_begin_jump_vault(JPH::BodyInterface &bodies) noexcept {
        if (facing_.IsNearZero()) {
            return false;
        }
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        const JPH::RVec3 takeoff(at.GetX(), jump_takeoff_feet_y_ + kPlayerHalfHeight, at.GetZ());
        const LedgeProbe probe =
            probe_ledge(takeoff, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
        if (!probe.valid) {
            return false;
        }
        JPH::RVec3 landing_centre;
        JPH::BodyID landing_body;
        if (!probe_vault_landing(probe, facing_, jump_takeoff_feet_y_, landing_centre,
                                 landing_body)) {
            return false;
        }
        begin_vault(bodies, probe, landing_centre, landing_body, at);
        ++jump_vault_count_;
        return true;
    }

    [[nodiscard]] bool apply_locomotion(JPH::BodyInterface &bodies,
                                        const StepCommands &commands,
                                        const float delta_seconds) noexcept {
        JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        JPH::Vec3 reference_velocity = airborne_inherited_velocity_;
        const double speed_scale = crouched_ ? kCrouchSpeedScale : 1.0;
        double move_x = commands.move_input_x * speed_scale;
        double move_z = commands.move_input_z * speed_scale;
        sprinting_ = false;
        balancing_ = false;

        if (grounded_ && support_entity_id_ != 0) {
            reference_velocity = current_support_point_velocity(bodies);
            airborne_inherited_velocity_ = reference_velocity;
            const Beam beam = beam_underfoot(bodies);
            const double input = std::hypot(commands.move_input_x, commands.move_input_z);
            if (beam.valid) {
                // On a beam: along it at a walk, held on its line unless the
                // stick means to step off.
                balancing_ = true;
                const double along = move_x * beam.along.GetX() + move_z * beam.along.GetZ();
                const double across = move_x * beam.across.GetX() + move_z * beam.across.GetZ();
                if (std::abs(across) < kBalanceStepOffInput) {
                    const double walk = std::clamp(along, -kBalanceSpeedScale, kBalanceSpeedScale);
                    const double centre =
                        std::clamp(-static_cast<double>(beam.offset * kBalanceCentering),
                                   -static_cast<double>(kBalanceCenteringMaxMps),
                                   static_cast<double>(kBalanceCenteringMaxMps)) /
                        kPlayerMaximumRelativeSpeed;
                    move_x = beam.along.GetX() * walk + beam.across.GetX() * centre;
                    move_z = beam.along.GetZ() * walk + beam.across.GetZ() * centre;
                }
            } else if (commands.sprint_held && !crouched_ && carry_constraint_ == nullptr &&
                       input >= kSprintMinimumInput &&
                       (commands.move_input_x * facing_.GetX() +
                        commands.move_input_z * facing_.GetZ()) >= kSprintMaximumAngleCos * input) {
                sprinting_ = true;
            }
            const float full_speed = sprinting_
                                         ? static_cast<float>(kPlayerMaximumRelativeSpeed * kSprintSpeedScale)
                                         : kPlayerMaximumRelativeSpeed;
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  move_x,
                                                  move_z,
                                                  kGroundAcceleration,
                                                  delta_seconds,
                                                  full_speed);
            // The air keeps what the ground gave: a running jump, or a run
            // off an edge, carries its speed.
            const JPH::Vec3 relative = player_velocity - reference_velocity;
            air_full_speed_ = std::max(kPlayerMaximumRelativeSpeed,
                                       JPH::Vec3(relative.GetX(), 0.0F, relative.GetZ()).Length());
        } else {
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  move_x,
                                                  move_z,
                                                  kAirAcceleration,
                                                  delta_seconds,
                                                  air_full_speed_);
        }

        // Crouched here means there was no room to stand, so no room to jump.
        const bool jump_started = commands.jump_requested && grounded_ && !crouched_;
        if (jump_started) {
            player_velocity.SetY(reference_velocity.GetY() + kJumpSpeed);
            jump_takeoff_feet_y_ =
                static_cast<float>(bodies.GetPosition(player_id_).GetY()) - kPlayerHalfHeight;
            jump_vault_ticks_left_ = kJumpVaultWindowTicks;
        }
        bodies.SetLinearVelocity(player_id_, player_velocity);
        if (!jump_started && grounded_ && support_entity_id_ != 0) {
            try_step_up(bodies, player_velocity - reference_velocity, delta_seconds);
        }
        return jump_started;
    }

    // The capsule swept from `from` by `displacement`: true on a hit, with
    // the hit fraction and the surface normal (pointing out of what was hit).
    [[nodiscard]] bool cast_capsule(const JPH::RVec3 from, const JPH::Vec3 displacement,
                                    float &fraction, JPH::Vec3 &normal) const {
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const JPH::RShapeCast sweep(active_player_shape(), JPH::Vec3::sReplicate(1.0F),
                                    JPH::RMat44::sTranslation(from), displacement);
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        physics_system_.GetNarrowPhaseQuery().CastShape(
            sweep, JPH::ShapeCastSettings(), from, collector, {}, {}, body_filter);
        if (!collector.HadHit()) {
            return false;
        }
        fraction = collector.mHit.mFraction;
        normal = -collector.mHit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
        return true;
    }

    // Stepping up. Blocked at the feet, clear with the body raised by the
    // maximum step, and walkable support found by sweeping back down: then
    // the body is lifted onto the step, as Jolt's own character controller
    // walks stairs. Every stage is a sweep against real geometry, so nothing
    // is passed through and nothing is stood on that is not there.
    void try_step_up(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                     const float delta_seconds) {
        const JPH::Vec3 horizontal(relative_velocity.GetX(), 0.0F, relative_velocity.GetZ());
        const float speed = horizontal.Length();
        if (speed < kStepMinimumSpeed) {
            return;
        }
        const JPH::Vec3 ahead = horizontal / speed * (speed * delta_seconds + kStepLookahead);
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        float fraction = 1.0F;
        JPH::Vec3 normal = JPH::Vec3::sZero();
        if (!cast_capsule(at, ahead, fraction, normal) ||
            normal.GetY() >= kSupportNormalThreshold) {
            return;  // Nothing in the way, or only a slope the feet can walk.
        }
        const JPH::RVec3 raised = at + JPH::Vec3(0.0F, kStepMaximumHeight, 0.0F);
        if (!capsule_pose_is_clear(raised) || cast_capsule(raised, ahead, fraction, normal)) {
            return;  // No headroom, or the obstacle is taller than a step.
        }
        const JPH::RVec3 over = raised + ahead;
        const JPH::Vec3 down(0.0F, -(kStepMaximumHeight + kStepMinimumHeight), 0.0F);
        if (!cast_capsule(over, down, fraction, normal) ||
            normal.GetY() < kSupportNormalThreshold) {
            return;  // Nothing walkable to put the feet on.
        }
        const float rise = kStepMaximumHeight + down.GetY() * fraction;
        if (rise < kStepMinimumHeight) {
            return;
        }
        bodies.SetPosition(player_id_, at + JPH::Vec3(0.0F, rise + 0.01F, 0.0F),
                           JPH::EActivation::Activate);
        ++step_up_count_;
    }

    // Real quadratic drag opposing the full velocity vector, not a clamp: it
    // can only ever pull speed toward the terminal value, never accelerate
    // the player upward past what deceleration implies (Governing Law 7 --
    // no powered ascent). Applied on top of ordinary air control, so existing
    // horizontal steering doubles as the "redirection" the same law permits.
    void apply_parachute_drag(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
        if (!parachute_deployed_) {
            return;
        }
        JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
        const float speed = velocity.Length();
        if (speed > 1.0e-4F) {
            const JPH::Vec3 drag_acceleration =
                -(velocity / speed) * (kParachuteDragCoefficient * speed * speed);
            velocity += drag_acceleration * delta_seconds;
            bodies.SetLinearVelocity(player_id_, velocity);
        }
    }

    void try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
        if (grounded_ || crouched_ || carry_constraint_ != nullptr || regrab_lockout_ticks_ > 0) {
            return;
        }
        if (bodies.GetLinearVelocity(player_id_).GetY() > kHangMaximumClimbSpeed) {
            return;
        }
        if (facing_.IsNearZero()) {
            return;
        }
        const double intent = commands.move_input_x * static_cast<double>(facing_.GetX()) +
                              commands.move_input_z * static_cast<double>(facing_.GetZ());
        if (intent < kHangIntentDotThreshold) {
            return;
        }

        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        const LedgeProbe probe = probe_ledge(origin,
                                             facing_,
                                             kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                             kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                                             true);
        if (!probe.valid) {
            // No ledge: a hold reached for in the air is caught and climbed.
            const Grip grip = find_grip(
                origin, origin + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach,
                facing_);
            if (grip.valid) {
                begin_climb(bodies, grip);
            }
            return;
        }

        const JPH::RVec3 hold(probe.wall_point.GetX() - facing_.GetX() * (kPlayerRadius + kHangWallGap),
                              probe.ledge_point.GetY() - kHangDropBelowLedge,
                              probe.wall_point.GetZ() - facing_.GetZ() * (kPlayerRadius + kHangWallGap));

        traversal_state_ = TraversalState::Hanging;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = probe.landing_body;
        traversal_normal_ = facing_;
        traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_target_ =
            to_support_local(bodies, traversal_target_body_, probe.landing_centre);
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = hold;
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    [[nodiscard]] bool try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept {
        if (!grounded_ || facing_.IsNearZero()) {
            return false;
        }

        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        const float feet_y = origin.GetY() - kPlayerHalfHeight;

        const LedgeProbe vault_probe =
            probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
        if (vault_probe.valid) {
            JPH::RVec3 landing_centre;
            JPH::BodyID landing_body;
            if (probe_vault_landing(vault_probe, facing_, feet_y, landing_centre, landing_body)) {
                begin_vault(bodies, vault_probe, landing_centre, landing_body, origin);
                return true;
            }
        }

        const LedgeProbe mantle_probe =
            probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
        if (!mantle_probe.valid) {
            return false;
        }
        begin_mantle(bodies, mantle_probe, origin);
        return true;
    }

    // Real support under the capsule's axis at `centre`: walkable ground no
    // deeper than the step's own lift plus the landing tolerance.
    [[nodiscard]] bool step_is_supported(const JPH::RVec3 centre) const noexcept {
        const JPH::RVec3 origin(centre.GetX(),
                                centre.GetY() - kPlayerHalfHeight + kLandingSupportProbeUp,
                                centre.GetZ());
        const JPH::Vec3 direction(
            0.0F, -(kLandingSupportProbeUp + kMantleApproachLift + kLandingSupportTolerance), 0.0F);
        JPH::RayCastResult hit;
        if (!cast_ray(origin, direction, hit)) {
            return false;
        }
        const JPH::RVec3 point = JPH::RRayCast(origin, direction).GetPointOnRay(hit.mFraction);
        return surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() >= kSupportNormalThreshold;
    }

    // Where a standing mantle's step-in ends (see kMantleApproachStandoff).
    // Returns `origin` itself when no step is possible.
    [[nodiscard]] JPH::RVec3 mantle_approach_end(const JPH::BodyInterface &bodies,
                                                 const LedgeProbe &probe,
                                                 const JPH::RVec3 origin) const noexcept {
        // The path lives in the ledge's frame; ground moving against that
        // frame would carry the feet off it mid-step.
        const JPH::Vec3 drift = current_support_point_velocity(bodies) -
                                bodies.GetPointVelocity(probe.ledge_body, origin);
        if (drift.Length() > kMantleApproachFrameDriftMps) {
            return origin;
        }
        const JPH::Vec3 to_wall(static_cast<float>(probe.wall_point.GetX() - origin.GetX()),
                                0.0F,
                                static_cast<float>(probe.wall_point.GetZ() - origin.GetZ()));
        const float wall_distance = to_wall.Length();
        const float travel = wall_distance - kMantleApproachStandoff;
        if (travel <= kMantleApproachSweepMargin) {
            return origin;
        }
        const JPH::Vec3 direction = to_wall / wall_distance;
        const JPH::RVec3 lifted(origin.GetX(), origin.GetY() + kMantleApproachLift, origin.GetZ());
        if (!capsule_pose_is_clear(lifted)) {
            return origin;
        }

        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const JPH::RShapeCast sweep(player_shape_,
                                    JPH::Vec3::sReplicate(1.0F),
                                    JPH::RMat44::sTranslation(lifted),
                                    direction * travel);
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        physics_system_.GetNarrowPhaseQuery().CastShape(
            sweep, JPH::ShapeCastSettings(), lifted, collector, {}, {}, body_filter);
        float allowed = travel;
        if (collector.HadHit()) {
            allowed = collector.mHit.mFraction * travel - kMantleApproachSweepMargin;
        }

        // Back off from the swept limit until the step ends on support: a step
        // over a gap is a float, not a step.
        for (; allowed > kMantleApproachSweepMargin; allowed -= kMantleApproachSupportSpacing) {
            const JPH::RVec3 step_end = lifted + direction * allowed;
            if (step_is_supported(step_end)) {
                return step_end;
            }
        }
        return origin;
    }

    void begin_mantle(JPH::BodyInterface &bodies,
                      const LedgeProbe &probe,
                      const JPH::RVec3 origin) noexcept {
        const JPH::RVec3 approach = mantle_approach_end(bodies, probe, origin);
        const float step_length = static_cast<float>(
            std::hypot(approach.GetX() - origin.GetX(), approach.GetZ() - origin.GetZ()));
        const double approach_seconds = static_cast<double>(step_length / kMantleApproachSpeedMps);
        // A player running in keeps their speed into the step; one standing
        // still starts it from rest.
        float entry_slope = 0.0F;
        if (step_length > 0.0F) {
            const JPH::Vec3 relative = bodies.GetLinearVelocity(player_id_) -
                                       bodies.GetPointVelocity(probe.ledge_body, origin);
            const float toward =
                relative.GetX() * facing_.GetX() + relative.GetZ() * facing_.GetZ();
            entry_slope = std::clamp(
                toward / kMantleApproachSpeedMps, 0.0F, kMantleApproachMaximumEntrySlope);
        }

        traversal_state_ = TraversalState::Mantling;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = probe.landing_body;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_local_approach_ = to_support_local(bodies, traversal_body_, approach);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_target_ =
            to_support_local(bodies, traversal_target_body_, probe.landing_centre);
        traversal_progress_ = 0.0;
        traversal_duration_ = kMantleDurationSeconds + approach_seconds;
        traversal_approach_fraction_ = approach_seconds / traversal_duration_;
        traversal_approach_entry_slope_ = entry_slope;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    void begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        traversal_state_ = TraversalState::Mantling;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_local_approach_ = traversal_local_start_;
        traversal_progress_ = 0.0;
        traversal_duration_ = kMantleDurationSeconds;
        traversal_approach_fraction_ = 0.0;
        traversal_approach_entry_slope_ = 0.0F;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    void begin_vault(JPH::BodyInterface &bodies,
                     const LedgeProbe &probe,
                     const JPH::RVec3 landing_centre,
                     const JPH::BodyID landing_body,
                     const JPH::RVec3 origin) noexcept {
        const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        const JPH::Vec3 support_velocity =
            grounded_ ? current_support_point_velocity(bodies) : airborne_inherited_velocity_;
        JPH::Vec3 relative(player_velocity.GetX() - support_velocity.GetX(),
                           0.0F,
                           player_velocity.GetZ() - support_velocity.GetZ());
        const float relative_speed = relative.Length();
        if (relative_speed > kPlayerMaximumRelativeSpeed) {
            relative = relative * (kPlayerMaximumRelativeSpeed / relative_speed);
        }

        const JPH::RVec3 apex(origin.GetX(),
                              probe.ledge_point.GetY() + kPlayerHalfHeight + kVaultApexClearance,
                              origin.GetZ());

        traversal_state_ = TraversalState::Vaulting;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = landing_body;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_apex_ = to_support_local(bodies, traversal_body_, apex);
        traversal_local_target_ = to_support_local(bodies, traversal_target_body_, landing_centre);
        traversal_progress_ = 0.0;
        traversal_duration_ = kVaultDurationSeconds;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = relative;
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    [[nodiscard]] JPH::RVec3 traversal_point(const JPH::BodyInterface &bodies,
                                             const float progress) const noexcept {
        const JPH::RVec3 start = from_support_local(bodies, traversal_body_, traversal_local_start_);
        const JPH::RVec3 target =
            from_support_local(bodies, traversal_target_body_, traversal_local_target_);

        if (traversal_state_ == TraversalState::Lowering) {
            // Back over the edge at standing height, then down to the hold
            // once the body is clear of the lip.
            const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
            const float over = smoothstep(0.0F, 0.55F, progress);
            const float down = smoothstep(0.55F, 1.0F, progress);
            return JPH::RVec3(start.GetX() + (hold.GetX() - start.GetX()) * over,
                              start.GetY() + (hold.GetY() - start.GetY()) * down,
                              start.GetZ() + (hold.GetZ() - start.GetZ()) * over);
        }

        if (traversal_state_ == TraversalState::Vaulting) {
            const JPH::RVec3 apex = from_support_local(bodies, traversal_body_, traversal_local_apex_);
            const float horizontal = progress;
            float height;
            if (progress < 0.5F) {
                height = start.GetY() +
                         (apex.GetY() - start.GetY()) * smoothstep(0.0F, 1.0F, progress * 2.0F);
            } else {
                height = apex.GetY() + (target.GetY() - apex.GetY()) *
                                           smoothstep(0.0F, 1.0F, (progress - 0.5F) * 2.0F);
            }
            return JPH::RVec3(start.GetX() + (target.GetX() - start.GetX()) * horizontal,
                              height,
                              start.GetZ() + (target.GetZ() - start.GetZ()) * horizontal);
        }

        // Step in (standing mantles only), then the climb from the step's end.
        const JPH::RVec3 approach =
            from_support_local(bodies, traversal_body_, traversal_local_approach_);
        const float approach_fraction = static_cast<float>(traversal_approach_fraction_);
        if (progress < approach_fraction) {
            const float phase = progress / approach_fraction;
            const float along = ease_to_rest(phase, traversal_approach_entry_slope_);
            const float up = smoothstep(0.0F, 1.0F, phase);
            return JPH::RVec3(start.GetX() + (approach.GetX() - start.GetX()) * along,
                              start.GetY() + (approach.GetY() - start.GetY()) * up,
                              start.GetZ() + (approach.GetZ() - start.GetZ()) * along);
        }
        const float climb = approach_fraction > 0.0F
                                ? (progress - approach_fraction) / (1.0F - approach_fraction)
                                : progress;
        const float vertical = smoothstep(0.0F, 0.55F, climb);
        const float horizontal = smoothstep(0.45F, 1.0F, climb);
        const float lift =
            kMantleClearanceLift * std::sin(static_cast<float>(kPi) * std::clamp(climb, 0.0F, 1.0F));
        return JPH::RVec3(approach.GetX() + (target.GetX() - approach.GetX()) * horizontal,
                          approach.GetY() + (target.GetY() - approach.GetY()) * vertical + lift,
                          approach.GetZ() + (target.GetZ() - approach.GetZ()) * horizontal);
    }

    void drive_traversal(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
        const JPH::RVec3 current = bodies.GetPosition(player_id_);

        if (traversal_state_ == TraversalState::Hanging ||
            traversal_state_ == TraversalState::Climbing) {
            traversal_desired_ = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        } else {
            traversal_progress_ =
                std::min(1.0, traversal_progress_ + static_cast<double>(delta_seconds) /
                                                        traversal_duration_);
            traversal_desired_ = traversal_point(bodies, static_cast<float>(traversal_progress_));
        }

        bodies.SetLinearVelocity(player_id_,
                                 JPH::Vec3(traversal_desired_ - current) / delta_seconds);
    }

    void resolve_traversal_outcome(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 actual = bodies.GetPosition(player_id_);
        const float error = JPH::Vec3(actual - traversal_desired_).Length();
        if (error > kTraversalStallTolerance) {
            ++traversal_stall_ticks_;
        } else {
            traversal_stall_ticks_ = 0;
        }

        if (traversal_stall_ticks_ >= kTraversalStallAbortTicks) {
            abort_traversal(bodies);
            return;
        }

        if (traversal_state_ == TraversalState::Lowering && traversal_progress_ >= 1.0) {
            finish_lowering();
        } else if (traversal_state_ != TraversalState::Hanging &&
                   traversal_state_ != TraversalState::Climbing && traversal_progress_ >= 1.0) {
            complete_traversal(bodies);
        }
    }

    [[nodiscard]] JPH::Vec3 traversal_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept {
        if (traversal_target_body_.IsInvalid()) {
            return JPH::Vec3::sZero();
        }
        const JPH::RVec3 target =
            from_support_local(bodies, traversal_target_body_, traversal_local_target_);
        return bodies.GetPointVelocity(traversal_target_body_, target);
    }

    void complete_traversal(JPH::BodyInterface &bodies) noexcept {
        const JPH::Vec3 support_velocity = traversal_support_point_velocity(bodies);
        JPH::Vec3 exit_velocity = support_velocity;
        if (traversal_state_ == TraversalState::Vaulting) {
            exit_velocity.SetX(support_velocity.GetX() + traversal_exit_relative_velocity_.GetX());
            exit_velocity.SetZ(support_velocity.GetZ() + traversal_exit_relative_velocity_.GetZ());
        }
        bodies.SetLinearVelocity(player_id_, exit_velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        ++accepted_traversal_count_;
        clear_traversal();
    }

    void abort_traversal(JPH::BodyInterface &bodies) noexcept {
        JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
        const JPH::Vec3 support_velocity = traversal_support_point_velocity(bodies);
        JPH::Vec3 relative(velocity.GetX() - support_velocity.GetX(),
                           0.0F,
                           velocity.GetZ() - support_velocity.GetZ());
        const float relative_speed = relative.Length();
        if (relative_speed > kPlayerMaximumRelativeSpeed) {
            relative = relative * (kPlayerMaximumRelativeSpeed / relative_speed);
        }
        velocity.SetX(support_velocity.GetX() + relative.GetX());
        velocity.SetZ(support_velocity.GetZ() + relative.GetZ());
        bodies.SetLinearVelocity(player_id_, velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        ++aborted_traversal_count_;
        clear_traversal();
    }

    void release_hang(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 support_velocity =
            traversal_body_.IsInvalid() ? JPH::Vec3::sZero()
                                        : bodies.GetPointVelocity(traversal_body_, ledge);
        bodies.SetLinearVelocity(player_id_, support_velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
        clear_traversal();
    }


    // ---- Step 2 movement (MECHANISM_ASCENT_PLAN.md §8) --------------------

    // A hang or climb on a dynamic body hangs the climber's weight on it at
    // the hold: the structure feels who is on it. Static and kinematic
    // bodies take no force.
    void load_hold(JPH::BodyInterface &bodies) noexcept {
        if ((traversal_state_ != TraversalState::Hanging &&
             traversal_state_ != TraversalState::Climbing) ||
            traversal_body_.IsInvalid() ||
            bodies.GetMotionType(traversal_body_) != JPH::EMotionType::Dynamic) {
            return;
        }
        const JPH::RVec3 at = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        bodies.AddForce(traversal_body_, physics_system_.GetGravity() * kPlayerMassKg, at);
    }

    // Horizontal right of the structure a hang or climb faces.
    [[nodiscard]] JPH::Vec3 traversal_right() const noexcept {
        return JPH::Vec3(-traversal_normal_.GetZ(), 0.0F, traversal_normal_.GetX());
    }

    // The hold nearest `aim` that a hand in front of `centre` can close
    // round: a box whose two thinner dimensions are at most kGripMaxSection
    // and whose length is at least kGripMinLength, on a static body or one of
    // at least kGripMinBodyMassKg, searched in a kGripSearchHalf box round
    // `aim` squared to `facing`. The point is on the member's axis.
    [[nodiscard]] Grip find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                                 const JPH::Vec3 facing) const noexcept {
        Grip best;
        if (facing.IsNearZero()) {
            return best;
        }
        const JPH::Quat rotation =
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(), std::atan2(facing.GetX(), facing.GetZ()));
        JPH::CollideShapeSettings settings;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
        const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
        const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
        physics_system_.GetNarrowPhaseQuery().CollideShape(
            grip_region_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
            JPH::RMat44::sRotationTranslation(rotation, aim), settings, aim, hits, {}, {}, filter);
        float best_distance = std::numeric_limits<float>::max();
        for (const JPH::CollideShapeResult &hit : hits.mHits) {
            const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(),
                                         hit.mBodyID2);
            if (!lock.Succeeded()) {
                continue;
            }
            const JPH::Body &body = lock.GetBody();
            if (body.IsSensor() ||
                (body.IsDynamic() &&
                 body.GetMotionProperties()->GetInverseMass() * kGripMinBodyMassKg > 1.0F)) {
                continue;
            }
            LeafBox box;
            if (!leaf_box(body, hit.mSubShapeID2, box)) {
                continue;
            }
            std::uint32_t long_axis = 0;
            if (!box_is_hold(box, long_axis)) {
                continue;
            }
            const JPH::Vec3 along = box.axes[long_axis];
            const float half_length = box.half[long_axis];
            const float t =
                std::clamp(JPH::Vec3(aim - box.centre).Dot(along), -half_length, half_length);
            const JPH::RVec3 point = box.centre + along * t;
            // In front of the body: not beside it, not behind it.
            if (JPH::Vec3(point - centre).Dot(facing) < 0.5F * kPlayerRadius) {
                continue;
            }
            const float distance = JPH::Vec3(point - aim).Length();
            if (distance < best_distance) {
                best_distance = distance;
                best.valid = true;
                best.body = hit.mBodyID2;
                best.entity_id = body.GetUserData();
                best.point = point;
            }
        }
        return best;
    }

    // Where a hand in front of `centre` aims: `height` over the centre,
    // kClimbHandReach toward the structure, `across` to its right.
    [[nodiscard]] JPH::RVec3 hand_aim(const JPH::RVec3 centre, const float height,
                                      const float across) const noexcept {
        return centre + JPH::Vec3(0.0F, height, 0.0F) + traversal_normal_ * kClimbHandReach +
               traversal_right() * across;
    }

    [[nodiscard]] JPH::RVec3 hand_point(const JPH::BodyInterface &bodies,
                                        const std::uint32_t hand) const noexcept {
        return from_support_local(bodies, hands_[hand].body, hands_[hand].local);
    }

    void set_hand(const JPH::BodyInterface &bodies, const std::uint32_t hand,
                  const Grip &grip) noexcept {
        hands_[hand].body = grip.body;
        hands_[hand].local = to_support_local(bodies, grip.body, grip.point);
        hands_[hand].valid = true;
    }

    // Both hands onto the holds nearest their rest aims, left a little lower
    // than right; a hand that finds none takes the hold the climb began on.
    void take_hand_holds(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                         const Grip &fallback) noexcept {
        for (std::uint32_t hand = 0; hand < 2; ++hand) {
            const float side = hand == 0 ? -1.0F : 1.0F;
            const float height = hand == 0 ? kClimbHandMid : kClimbHandMid + 0.2F;
            const Grip grip =
                find_grip(centre, hand_aim(centre, height, kClimbHandSpan * side), traversal_normal_);
            set_hand(bodies, hand, grip.valid ? grip : fallback);
        }
        regrip_cooldown_ticks_ = 0;
    }

    // The swept move of a climbing or hanging body, slimmed so the holds it
    // already touches do not stop it.
    [[nodiscard]] bool climb_move_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept {
        const JPH::Vec3 displacement(to - from);
        if (displacement.IsNearZero(1.0e-10F)) {
            return true;
        }
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const JPH::RShapeCast sweep(climb_sweep_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
                                    JPH::RMat44::sTranslation(from), displacement);
        const JPH::IgnoreSingleBodyFilter player_filter(player_id_);
        const JPH::IgnoreSingleBodyFilterChained filter(carried_id_, player_filter);
        physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(), from,
                                                        collector, {}, {}, filter);
        return !collector.HadHit();
    }

    // Take hold of `grip` where the body is, facing it.
    void begin_climb(JPH::BodyInterface &bodies, const Grip &grip) noexcept {
        const JPH::RVec3 hold = bodies.GetPosition(player_id_);
        traversal_state_ = TraversalState::Climbing;
        traversal_body_ = grip.body;
        traversal_entity_id_ = grip.entity_id;
        traversal_target_body_ = {};
        traversal_normal_ = facing_;
        traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, grip.point);
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = hold;
        bodies.SetGravityFactor(player_id_, 0.0F);
        take_hand_holds(bodies, hold, grip);
        ++climb_count_;
    }

    // From the ground, a hold faced at hand height.
    [[nodiscard]] Grip grip_in_front(const JPH::BodyInterface &bodies) const noexcept {
        const JPH::RVec3 centre = bodies.GetPosition(player_id_);
        const JPH::RVec3 aim = centre + JPH::Vec3(0.0F, kClimbHandMid, 0.0F) + facing_ * kClimbHandReach;
        return find_grip(centre, aim, facing_);
    }

    // Climbing, the stick toward the structure climbs, away climbs down,
    // sideways moves across -- each only onto holds the hands can reach.
    void update_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept {
        const JPH::Vec3 normal = traversal_normal_;
        const JPH::Vec3 right = traversal_right();
        const double up_input =
            commands.move_input_x * normal.GetX() + commands.move_input_z * normal.GetZ();
        const double side_input =
            commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
        const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        JPH::RVec3 next = hold;
        Grip carries;
        if (up_input > kClimbInputDeadzone) {
            // A ledge in reach is climbed over before any hold above it: grab
            // rails that run on past a deck's edge must not carry the body
            // past the height it can mantle from.
            if (up_input > 0.5 && try_top_out(bodies)) {
                return;
            }
            const JPH::RVec3 moved =
                hold + JPH::Vec3(0.0F, static_cast<float>(up_input) * kClimbUpSpeed * delta_seconds, 0.0F);
            const Grip above = find_grip(moved, hand_aim(moved, kClimbMoveAim, 0.0F), normal);
            if (above.valid) {
                next = moved;
                carries = above;
            }
        } else if (up_input < -kClimbInputDeadzone) {
            const JPH::RVec3 moved =
                hold + JPH::Vec3(0.0F, static_cast<float>(up_input) * kClimbDownSpeed * delta_seconds, 0.0F);
            if (feet_on_ground(moved)) {
                step_off_climb(bodies);
                return;
            }
            const Grip below = find_grip(moved, hand_aim(moved, kClimbHandLow, 0.0F), normal);
            if (below.valid) {
                next = moved;
                carries = below;
            }
        }
        if (std::abs(side_input) > kClimbInputDeadzone) {
            const float lead = side_input > 0.0 ? kClimbSideLead : -kClimbSideLead;
            const JPH::RVec3 moved =
                next + right * (static_cast<float>(side_input) * kClimbSideSpeed * delta_seconds);
            const Grip beside = find_grip(moved, hand_aim(moved, kClimbHandMid, lead), normal);
            if (beside.valid) {
                next = moved;
                carries = beside;
            }
        }
        if (carries.valid && climb_move_clear(hold, next)) {
            traversal_body_ = carries.body;
            traversal_entity_id_ = carries.entity_id;
            traversal_local_hold_ = to_support_local(bodies, traversal_body_, next);
            traversal_local_ledge_ = to_support_local(bodies, traversal_body_, carries.point);
        }
        move_hands(bodies, from_support_local(bodies, traversal_body_, traversal_local_hold_),
                   up_input, side_input);
    }

    // Hand over hand: the hand furthest behind the way the body moves
    // reaches ahead to the next hold, one hand every kClimbRegripTicks.
    void move_hands(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                    const double up_input, const double side_input) noexcept {
        if (regrip_cooldown_ticks_ > 0) {
            --regrip_cooldown_ticks_;
            return;
        }
        const JPH::Vec3 right = traversal_right();
        std::int32_t mover = -1;
        float height = kClimbHandMid;
        float across = 0.0F;
        const auto rise = [&](const std::uint32_t hand) {
            return static_cast<float>(hand_point(bodies, hand).GetY() - centre.GetY());
        };
        const auto offset = [&](const std::uint32_t hand) {
            return JPH::Vec3(hand_point(bodies, hand) - centre).Dot(right);
        };
        if (up_input > kClimbInputDeadzone) {
            const std::uint32_t low = rise(0) <= rise(1) ? 0U : 1U;
            if (rise(low) < kClimbHandMid - 0.15F) {
                mover = static_cast<std::int32_t>(low);
                height = kClimbHandHigh;
            }
        } else if (up_input < -kClimbInputDeadzone) {
            const std::uint32_t high = rise(0) >= rise(1) ? 0U : 1U;
            if (rise(high) > kClimbHandHigh + 0.10F) {
                mover = static_cast<std::int32_t>(high);
                height = kClimbHandLow;
            }
        } else if (std::abs(side_input) > kClimbInputDeadzone) {
            const float sign = side_input > 0.0 ? 1.0F : -1.0F;
            const std::uint32_t behind = offset(0) * sign <= offset(1) * sign ? 0U : 1U;
            if (offset(behind) * sign < -kClimbHandSpan) {
                mover = static_cast<std::int32_t>(behind);
                across = sign * kClimbSideLead;
            }
        }
        // A hand the body has left out of reach takes the nearest hold again.
        for (std::uint32_t hand = 0; hand < 2 && mover < 0; ++hand) {
            const JPH::RVec3 shoulder = centre + JPH::Vec3(0.0F, kClimbHandMid, 0.0F);
            if (!hands_[hand].valid ||
                JPH::Vec3(hand_point(bodies, hand) - shoulder).Length() > 0.95F) {
                mover = static_cast<std::int32_t>(hand);
                across = hand == 0 ? -kClimbHandSpan : kClimbHandSpan;
            }
        }
        if (mover < 0) {
            return;
        }
        const std::uint32_t hand = static_cast<std::uint32_t>(mover);
        if (across == 0.0F) {
            across = hand == 0 ? -kClimbHandSpan : kClimbHandSpan;
        }
        const Grip grip = find_grip(centre, hand_aim(centre, height, across), traversal_normal_);
        if (grip.valid) {
            set_hand(bodies, hand, grip);
            regrip_cooldown_ticks_ = kClimbRegripTicks;
        }
    }

    // Soles within kClimbFootReach of walkable ground under a body at `centre`.
    [[nodiscard]] bool feet_on_ground(const JPH::RVec3 centre) const noexcept {
        JPH::RayCastResult hit;
        const JPH::Vec3 down(0.0F, -(kPlayerHalfHeight + kClimbFootReach), 0.0F);
        if (!cast_ray(centre, down, hit)) {
            return false;
        }
        const JPH::RVec3 point = JPH::RRayCast(centre, down).GetPointOnRay(hit.mFraction);
        return surface_normal(hit.mBodyID, hit.mSubShapeID2, point).GetY() >= kSupportNormalThreshold;
    }

    // Climbing over a ledge in reach: the mantle takes over from where the
    // body is. The ledge is looked for where the player looks, then along
    // the structure's normal: a climber on a pipe faced from an angle looks
    // round at the deck it means to climb onto.
    [[nodiscard]] bool try_top_out(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        LedgeProbe probe;
        if (!facing_.IsNearZero()) {
            probe = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true, true);
        }
        if (!probe.valid) {
            probe = probe_ledge(origin, traversal_normal_, kMantleMinimumRise, kMantleMaximumRise,
                                true, true);
        }
        if (!probe.valid) {
            return false;
        }
        hands_[0].valid = false;
        hands_[1].valid = false;
        begin_mantle(bodies, probe, origin);
        return true;
    }

    void step_off_climb(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        bodies.SetLinearVelocity(player_id_, bodies.GetPointVelocity(traversal_body_, hold));
        bodies.SetGravityFactor(player_id_, 1.0F);
        ++accepted_traversal_count_;
        clear_traversal();
    }

    void let_go_climb(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        const JPH::Vec3 support_velocity = bodies.GetPointVelocity(traversal_body_, hold);
        bodies.SetLinearVelocity(player_id_, support_velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
        clear_traversal();
    }

    // Springing back off the structure, away from it and up.
    void jump_off_climb(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        const JPH::Vec3 support_velocity = bodies.GetPointVelocity(traversal_body_, hold);
        bodies.SetLinearVelocity(player_id_, support_velocity -
                                                 traversal_normal_ * kClimbJumpBackSpeed +
                                                 JPH::Vec3(0.0F, kClimbJumpUpSpeed, 0.0F));
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
        clear_traversal();
    }

    // The lip of a ledge near `near` (a point on or near its top edge),
    // faced along `normal`: a ray across just under the top onto its face,
    // then a ray down onto the top just inside the face.
    [[nodiscard]] Lip probe_lip(const JPH::RVec3 near, const JPH::Vec3 normal) const noexcept {
        Lip lip;
        const JPH::RVec3 outside =
            near - normal * 0.5F - JPH::Vec3(0.0F, kLipProbeDepth, 0.0F);
        const JPH::Vec3 across = normal * 0.9F;
        JPH::RayCastResult face_hit;
        if (!cast_ray(outside, across, face_hit)) {
            return lip;
        }
        const JPH::RVec3 face = JPH::RRayCast(outside, across).GetPointOnRay(face_hit.mFraction);
        const JPH::RVec3 above = face + normal * kTopProbeInset + JPH::Vec3(0.0F, 0.30F, 0.0F);
        const JPH::Vec3 down(0.0F, -0.45F, 0.0F);
        JPH::RayCastResult top_hit;
        if (!cast_ray(above, down, top_hit) || top_hit.mBodyID != face_hit.mBodyID) {
            return lip;
        }
        const JPH::RVec3 top = JPH::RRayCast(above, down).GetPointOnRay(top_hit.mFraction);
        if (surface_normal(top_hit.mBodyID, top_hit.mSubShapeID2, top).GetY() <
            kLedgeTopNormalThreshold) {
            return lip;
        }
        lip.valid = true;
        lip.body = face_hit.mBodyID;
        lip.entity_id = physics_system_.GetBodyInterfaceNoLock().GetUserData(face_hit.mBodyID);
        lip.ledge = top;
        const JPH::RVec3 off_face = face - normal * (kPlayerRadius + kHangWallGap);
        lip.hold = JPH::RVec3(off_face.GetX(), top.GetY() - kHangDropBelowLedge, off_face.GetZ());
        const JPH::RVec3 inside = face + normal * kLandingInset;
        lip.landing =
            JPH::RVec3(inside.GetX(), top.GetY() + kPlayerHalfHeight + kLandingSkin, inside.GetZ());
        return lip;
    }

    // Hanging, the stick sideways moves along the ledge while its lip goes
    // on under the hands. Something standing on the ledge back from its lip
    // does not stop a shimmy; only climbing up needs the landing clear.
    void update_shimmy(JPH::BodyInterface &bodies, const StepCommands &commands,
                       const float delta_seconds) noexcept {
        const JPH::Vec3 right = traversal_right();
        const double side_input =
            commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
        if (std::abs(side_input) <= kClimbInputDeadzone || traversal_normal_.IsNearZero()) {
            return;
        }
        const float lead = side_input > 0.0 ? 0.25F : -0.25F;
        const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 step = right * (static_cast<float>(side_input) * kShimmySpeed * delta_seconds);
        const Lip at = probe_lip(ledge + step, traversal_normal_);
        const Lip ahead = probe_lip(ledge + step + right * lead, traversal_normal_);
        if (!at.valid || !ahead.valid || std::abs(at.ledge.GetY() - ledge.GetY()) > 0.15 ||
            !climb_move_clear(hold, at.hold)) {
            return;
        }
        traversal_body_ = at.body;
        traversal_entity_id_ = at.entity_id;
        traversal_local_hold_ = to_support_local(bodies, at.body, at.hold);
        traversal_local_ledge_ = to_support_local(bodies, at.body, at.ledge);
        traversal_target_body_ = at.body;
        traversal_local_target_ = to_support_local(bodies, at.body, at.landing);
    }

    // An edge behind a standing body with a drop beyond it: the floor ends
    // within kEdgeSearchReach behind the body and nothing is under the space
    // beyond for kEdgeDropMinimum, and a hanging body fits below its lip.
    [[nodiscard]] Lip probe_edge_drop(const JPH::BodyInterface &bodies) const noexcept {
        if (!grounded_ || crouched_ || carry_constraint_ != nullptr || facing_.IsNearZero()) {
            return {};
        }
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        const double feet_y = origin.GetY() - kPlayerHalfHeight;
        const JPH::Vec3 back = -facing_;
        for (float behind = kEdgeSearchStart; behind <= kEdgeSearchReach + 1.0e-4F;
             behind += kEdgeSearchStep) {
            const JPH::RVec3 at = origin + back * behind;
            const JPH::RVec3 from(at.GetX(), feet_y + 0.05, at.GetZ());
            JPH::RayCastResult floor;
            if (cast_ray(from, JPH::Vec3(0.0F, -(0.05F + kEdgeDropMinimum), 0.0F), floor)) {
                continue;
            }
            const JPH::RVec3 last = origin + back * (behind - kEdgeSearchStep);
            const Lip lip = probe_lip(JPH::RVec3(last.GetX(), feet_y, last.GetZ()), facing_);
            if (!lip.valid || std::abs(lip.ledge.GetY() - feet_y) > 0.10 ||
                !capsule_pose_is_clear(lip.hold)) {
                return {};
            }
            return lip;
        }
        return {};
    }

    // Drop, standing with an edge behind: lower over it into a hang.
    [[nodiscard]] bool try_begin_lowering(JPH::BodyInterface &bodies) noexcept {
        const Lip lip = probe_edge_drop(bodies);
        if (!lip.valid) {
            return false;
        }
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        traversal_state_ = TraversalState::Lowering;
        traversal_body_ = lip.body;
        traversal_entity_id_ = lip.entity_id;
        traversal_target_body_ = lip.body;
        traversal_normal_ = facing_;
        traversal_local_start_ = to_support_local(bodies, lip.body, origin);
        traversal_local_hold_ = to_support_local(bodies, lip.body, lip.hold);
        traversal_local_ledge_ = to_support_local(bodies, lip.body, lip.ledge);
        traversal_local_target_ = to_support_local(bodies, lip.body, lip.landing);
        traversal_progress_ = 0.0;
        traversal_duration_ = kLoweringSeconds;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        bodies.SetGravityFactor(player_id_, 0.0F);
        return true;
    }

    // Lowered: hanging from the lip, as if the hang had been caught there.
    void finish_lowering() noexcept {
        traversal_state_ = TraversalState::Hanging;
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        ++accepted_traversal_count_;
    }

    // The support under a walking body, if it is a beam: a box narrower
    // than kBalanceMaxWidth and at least kBalanceMinLength long, lying level.
    struct Beam final {
        bool valid = false;
        JPH::Vec3 along{JPH::Vec3::sZero()};
        JPH::Vec3 across{JPH::Vec3::sZero()};
        float offset = 0.0F;
    };

    [[nodiscard]] Beam beam_underfoot(const JPH::BodyInterface &bodies) const noexcept {
        Beam beam;
        const JPH::RVec3 centre = bodies.GetPosition(player_id_);
        const float half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
        JPH::RayCastResult hit;
        if (!cast_ray(centre, JPH::Vec3(0.0F, -(half_height + kCheckpointFootingSlack), 0.0F), hit)) {
            return beam;
        }
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), hit.mBodyID);
        if (!lock.Succeeded()) {
            return beam;
        }
        LeafBox box;
        if (!leaf_box(lock.GetBody(), hit.mSubShapeID2, box)) {
            return beam;
        }
        std::uint32_t up = 0;
        for (std::uint32_t axis = 1; axis < 3; ++axis) {
            if (std::abs(box.axes[axis].GetY()) > std::abs(box.axes[up].GetY())) {
                up = axis;
            }
        }
        if (std::abs(box.axes[up].GetY()) < 0.9F) {
            return beam;
        }
        const std::uint32_t a = (up + 1) % 3;
        const std::uint32_t b = (up + 2) % 3;
        const std::uint32_t long_axis = box.half[a] >= box.half[b] ? a : b;
        const std::uint32_t short_axis = long_axis == a ? b : a;
        if (2.0F * box.half[short_axis] > kBalanceMaxWidth ||
            2.0F * box.half[long_axis] < kBalanceMinLength) {
            return beam;
        }
        JPH::Vec3 along = box.axes[long_axis];
        along.SetY(0.0F);
        if (along.IsNearZero()) {
            return beam;
        }
        beam.valid = true;
        beam.along = along.Normalized();
        beam.across = JPH::Vec3(-beam.along.GetZ(), 0.0F, beam.along.GetX());
        beam.offset = JPH::Vec3(centre - box.centre).Dot(beam.across);
        return beam;
    }

    void clear_traversal() noexcept {
        traversal_state_ = TraversalState::None;
        traversal_normal_ = JPH::Vec3::sZero();
        hands_[0].valid = false;
        hands_[1].valid = false;
        air_full_speed_ = kPlayerMaximumRelativeSpeed;
        traversal_body_ = {};
        traversal_target_body_ = {};
        traversal_entity_id_ = 0;
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    }

    void update_affordance(const JPH::BodyInterface &bodies) noexcept {
        affordance_ = {};
        grip_affordance_ = {};
        edge_affordance_ = {};
        carry_target_entity_ = 0;
        (void)carry_candidate(bodies, carry_target_entity_);
        find_rig_action(bodies);
        if (traversal_state_ == TraversalState::None && !crouched_ &&
            carry_constraint_ == nullptr && !facing_.IsNearZero() && grounded_) {
            grip_affordance_ = grip_in_front(bodies);
            edge_affordance_ = probe_edge_drop(bodies);
        }
        // The probes measure rises from standing feet and test standing
        // landing poses; a crouched body is offered none (a request stands
        // it first, see update_crouch). Hands full, no ledge is offered at all.
        if (traversal_state_ != TraversalState::None || facing_.IsNearZero() || crouched_ ||
            carry_constraint_ != nullptr) {
            return;
        }

        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        if (grounded_) {
            const LedgeProbe vault_probe =
                probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
            if (vault_probe.valid) {
                JPH::RVec3 landing_centre;
                JPH::BodyID landing_body;
                if (probe_vault_landing(vault_probe,
                                        facing_,
                                        origin.GetY() - kPlayerHalfHeight,
                                        landing_centre,
                                        landing_body)) {
                    affordance_ = vault_probe;
                    return;
                }
            }
            affordance_ = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
            return;
        }

        affordance_ = probe_ledge(origin,
                                  facing_,
                                  kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                  kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                                  true);
    }

    // Machine half of a checkpoint (TDD 14.1: "machine/control state").
    // Kinematic bodies are deliberately excluded -- see BodyCheckpoint comment.
    void commit_machine_checkpoint(const JPH::BodyInterface &bodies) noexcept {
        checkpoint_.ballast = capture_body(bodies, ballast_id_);
        checkpoint_.tipper = capture_body(bodies, tipper_id_);
        checkpoint_.valve_lever = capture_body(bodies, valve_lever_id_);
        checkpoint_.treadle = capture_body(bodies, treadle_id_);
        checkpoint_.lift_platform = capture_body(bodies, lift_platform_id_);
        checkpoint_.counterweight = capture_body(bodies, counterweight_id_);
        checkpoint_.jib_boom = capture_body(bodies, jib_boom_id_);
        checkpoint_.jib_hook = capture_body(bodies, jib_hook_id_);
        checkpoint_.crate = capture_body(bodies, crate_id_);
        checkpoint_.needle_beam = capture_body(bodies, needle_beam_id_);
        checkpoint_.needle_seated = needle_seated_;
        checkpoint_.sump_volume_kg = sump_volume_kg_;
        checkpoint_.sump_isolated = sump_isolated_;
        checkpoint_.vessel_mass_kg = steam_plant_.state().vessel_mass_kg;
        checkpoint_.cylinder_mass_kg = steam_plant_.state().cylinder_mass_kg;
        checkpoint_.intake_swing_flight = capture_body(bodies, intake_swing_flight_id_);
        checkpoint_.intake_cw_cradle = capture_body(bodies, intake_cw_cradle_id_);
        checkpoint_.hook5_door = capture_body(bodies, hook5_door_id_);
        checkpoint_.hook5_bar = capture_body(bodies, hook5_bar_id_);
        checkpoint_.hook5_block = capture_body(bodies, hook5_block_id_);
        checkpoint_.carrying_entity = carried_entity_;
        checkpoint_.intake_pack_slung = intake_pack_slung_;
        kit_->capture(checkpoint_.kit);
    }

    // WO-008 automatic commit (GDD 9.1): every tick the player is grounded on
    // firm footing (kCheckpointFootingNormalY) and not mid-traversal, so the
    // checkpoint is always "wherever the player was last standing." No dwell
    // timer, no player-facing save action.
    void commit_checkpoint(const JPH::BodyInterface &bodies) noexcept {
        checkpoint_position_ = bodies.GetPosition(player_id_);
        checkpoint_crouched_ = crouched_;
        commit_machine_checkpoint(bodies);
        ++checkpoint_commit_count_;
    }

    // WO-008 death restore (Governing Laws 9, 21): the one sanctioned
    // exception to "no hidden teleportation," explicitly named by Law 21
    // itself. Restores player and every captured machine body, then clears
    // this tick's now-stale contact/traversal-adjacent state so the next
    // tick re-establishes ground truth from a fresh contact pass rather than
    // publishing a snapshot that mixes a teleported position with a contact
    // sample that referred to the pre-restore position.
    void restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept {
        // The capsule the checkpoint was committed in: a crouched commit's
        // centre is a crouched centre, and may sit under a low ceiling.
        crouched_ = checkpoint_crouched_;
        bodies.SetShape(player_id_, active_player_shape(), false, JPH::EActivation::Activate);
        bodies.SetPositionAndRotation(player_id_, checkpoint_position_, JPH::Quat::sIdentity(),
                                      JPH::EActivation::Activate);
        bodies.SetLinearAndAngularVelocity(player_id_, JPH::Vec3::sZero(), JPH::Vec3::sZero());

        restore_body(bodies, ballast_id_, checkpoint_.ballast);
        restore_body(bodies, tipper_id_, checkpoint_.tipper);
        restore_body(bodies, valve_lever_id_, checkpoint_.valve_lever);
        restore_body(bodies, treadle_id_, checkpoint_.treadle);
        restore_body(bodies, lift_platform_id_, checkpoint_.lift_platform);
        restore_body(bodies, counterweight_id_, checkpoint_.counterweight);
        restore_body(bodies, jib_boom_id_, checkpoint_.jib_boom);
        restore_body(bodies, jib_hook_id_, checkpoint_.jib_hook);
        restore_body(bodies, crate_id_, checkpoint_.crate);
        restore_body(bodies, needle_beam_id_, checkpoint_.needle_beam);
        restore_needle_topology(checkpoint_.needle_seated);
        restore_body(bodies, intake_swing_flight_id_, checkpoint_.intake_swing_flight);
        restore_body(bodies, intake_cw_cradle_id_, checkpoint_.intake_cw_cradle);
        restore_legal_forty_topology(checkpoint_.intake_pack_slung);
        restore_body(bodies, hook5_door_id_, checkpoint_.hook5_door);
        restore_body(bodies, hook5_bar_id_, checkpoint_.hook5_bar);
        restore_body(bodies, hook5_block_id_, checkpoint_.hook5_block);
        // A kit body in the hands may be one the restore takes out of the
        // world (a shackle hooked at the commit): let go of it first.
        if (carry_constraint_ != nullptr &&
            scraperx::sim::kit::is_kit_entity(carried_entity_)) {
            release_carry();
        }
        kit_->restore(checkpoint_.kit);
        restore_carry_topology(checkpoint_.carrying_entity);
        // The body comes back at rest, so what it holds does too. Restored
        // with the walking speed it was committed at, the load swung out of
        // the still hands and dragged the body back off the edge it had just
        // been restored onto (observed at MOD-HALL-DECK's north edge).
        if (carry_constraint_ != nullptr) {
            bodies.SetLinearAndAngularVelocity(carried_id_, JPH::Vec3::sZero(),
                                               JPH::Vec3::sZero());
        }
        // No topology reconciliation call needed here, unlike the needle:
        // update_sump recomputes grate_safe_ and reasserts the grate's
        // sensor flag from sump_volume_kg_ unconditionally every tick, so
        // restoring the scalar is the whole restore.
        sump_volume_kg_ = checkpoint_.sump_volume_kg;
        sump_isolated_ = checkpoint_.sump_isolated;
        steam_plant_.restore_state(checkpoint_.vessel_mass_kg, checkpoint_.cylinder_mass_kg);

        grounded_ = false;
        jump_vault_ticks_left_ = 0;
        support_entity_id_ = 0;
        support_sample_ = {};
        airborne_inherited_velocity_ = JPH::Vec3::sZero();
        air_full_speed_ = kPlayerMaximumRelativeSpeed;
        sprinting_ = false;
        balancing_ = false;
        fall_peak_speed_mps_ = 0.0F;
        pre_contact_fall_speed_mps_ = 0.0F;
        parachute_deployed_ = false;
        ++death_count_;
    }

    void read_machine_state(const JPH::BodyInterface &bodies) noexcept {
        state_.hoist_scoop_position = {kScoopX, scoop_height_, kScoopZ};
        state_.hoist_scoop_tilt_radians = scoop_tilt_;

        const JPH::RVec3 ballast_position = bodies.GetPosition(ballast_id_);
        const JPH::Vec3 ballast_velocity = bodies.GetLinearVelocity(ballast_id_);
        state_.ballast_position = to_vector3(ballast_position);
        state_.ballast_linear_velocity =
            {ballast_velocity.GetX(), ballast_velocity.GetY(), ballast_velocity.GetZ()};

        state_.tipper_position = to_vector3(bodies.GetPosition(tipper_id_));
        state_.tipper_angle_radians =
            tipper_hinge_ != nullptr ? tipper_hinge_->GetCurrentAngle() : 0.0;
        state_.valve_lever_angle_radians = valve_lever_angle_;
        state_.treadle_angle_radians = treadle_angle_;

        const JPH::RVec3 platform_position = bodies.GetPosition(lift_platform_id_);
        const JPH::Vec3 platform_velocity = bodies.GetLinearVelocity(lift_platform_id_);
        state_.lift_platform_position = to_vector3(platform_position);
        state_.lift_platform_linear_velocity =
            {platform_velocity.GetX(), platform_velocity.GetY(), platform_velocity.GetZ()};
        state_.counterweight_position = to_vector3(bodies.GetPosition(counterweight_id_));

        const auto &plant = steam_plant_.state();
        state_.valve_open_fraction = plant.valve_open_fraction;
        state_.vessel_pressure_pa = plant.vessel_pressure_pa;
        state_.cylinder_pressure_pa = plant.cylinder_pressure_pa;
        state_.orifice_mass_flow_kg_per_s = plant.orifice_mass_flow_kg_per_s;
        state_.vented_mass_kg = plant.vented_mass_kg;
        state_.piston_force_n = plant.piston_force_n;
        state_.vessel_available_energy_j = steam_plant_.vessel_available_energy_j();
        state_.machine_cycle_phase_seconds = machine_cycle_phase_seconds_;

        state_.jib_station_active = jib_station_active_;
        state_.jib_boom_angle_radians = jib_boom_angle_;
        const JPH::Vec3 hook_velocity = bodies.GetLinearVelocity(jib_hook_id_);
        state_.jib_hook_position = to_vector3(bodies.GetPosition(jib_hook_id_));
        state_.jib_hook_linear_velocity =
            {hook_velocity.GetX(), hook_velocity.GetY(), hook_velocity.GetZ()};
        const JPH::Vec3 crate_velocity = bodies.GetLinearVelocity(crate_id_);
        state_.jib_crate_position = to_vector3(bodies.GetPosition(crate_id_));
        state_.jib_crate_linear_velocity =
            {crate_velocity.GetX(), crate_velocity.GetY(), crate_velocity.GetZ()};
        state_.jib_capacity_stand_load_position =
            to_vector3(bodies.GetPosition(capacity_stand_load_id_));

        state_.needle_station_active = needle_station_active_;
        state_.needle_seated = needle_seated_;
        const JPH::Vec3 needle_velocity = bodies.GetLinearVelocity(needle_beam_id_);
        state_.needle_position = to_vector3(bodies.GetPosition(needle_beam_id_));
        state_.needle_linear_velocity =
            {needle_velocity.GetX(), needle_velocity.GetY(), needle_velocity.GetZ()};

        state_.sump_station_active = sump_station_active_;
        state_.sump_isolated = sump_isolated_;
        state_.sump_volume_kg = sump_volume_kg_;
        state_.grate_safe = grate_safe_;

        state_.intake_station_active = intake_station_active_;
        state_.intake_boom_angle_radians = intake_boom_angle_;
        state_.intake_hook_position = to_vector3(bodies.GetPosition(intake_hook_id_));
        state_.intake_pack_position = to_vector3(bodies.GetPosition(intake_pack_id_));
        state_.intake_overweight_pack_position =
            to_vector3(bodies.GetPosition(intake_overweight_pack_id_));
        state_.intake_dog_angle_radians = intake_dog_angle_;
        state_.intake_pack_pins_dog = intake_pack_pins_dog_;
        state_.intake_throat_clear = intake_throat_clear_;

        state_.legal_forty_pack_slung = intake_pack_slung_;
        state_.legal_forty_swing_travel_radians = intake_swing_travel_;
        state_.legal_forty_swing_flight_position =
            to_vector3(bodies.GetPosition(intake_swing_flight_id_));
        state_.legal_forty_cradle_position = to_vector3(bodies.GetPosition(intake_cw_cradle_id_));

        state_.carrying_entity_id = carried_entity_;
        state_.carry_target_entity_id = carry_target_entity_;
        state_.hook5_door_angle_radians =
            hook5_door_hinge_ != nullptr ? hook5_door_hinge_->GetCurrentAngle() : 0.0;
        const JPH::RVec3 block_position = bodies.GetPosition(hook5_block_id_);
        state_.hook5_block_position = to_vector3(block_position);
        state_.hook5_block_rotation = to_quaternion(bodies.GetRotation(hook5_block_id_));
        state_.hook5_bar_position = to_vector3(bodies.GetPosition(hook5_bar_id_));
        state_.hook5_bar_rotation = to_quaternion(bodies.GetRotation(hook5_bar_id_));
        state_.hook_in_rack =
            JPH::Vec3(block_position - JPH::RVec3(kHook5BlockSeatX, kHook5BlockSeatY,
                                                  kHook5BlockSeatZ))
                .Length() <= kHook5InRackTolerance;

        state_.rig_action = rig_action_;
        state_.rig_target_entity_id = rig_target_entity_;
        switch (kit_->carry_kind(carry_target_entity_)) {
        case scraperx::sim::kit::CarryKind::Shackle:
            state_.carry_target_kind = 1;
            break;
        case scraperx::sim::kit::CarryKind::Handle:
            state_.carry_target_kind = 2;
            break;
        default:
            state_.carry_target_kind = 0;
            break;
        }
        const auto &kit = *kit_;
        state_.well_a_cage_travel = kit.guide_travel(well_.a_cage_guide);
        state_.well_a_skip_travel = kit.guide_travel(well_.a_skip_guide);
        state_.well_a_cage_peak_speed = kit.guide_peak_speed(well_.a_cage_guide);
        state_.well_a_catch_latched = kit.catch_latched(well_.a_catch);
        state_.well_a_rope_end_entity_id = kit.rope_end_entity(well_.a_rope);
        state_.well_a_rope_tension_n = kit.rope_tension(well_.a_rope);
        state_.well_a_lever_angle = kit.lever_angle(well_.a_lever);
        state_.well_b_cage_travel = kit.guide_travel(well_.b_cage_guide);
        state_.well_b_cage_peak_speed = kit.guide_peak_speed(well_.b_cage_guide);
        state_.well_b_boom_angle = kit.lever_angle(well_.b_boom_hinge);
        state_.well_b_catch_latched = kit.catch_latched(well_.b_catch);
        state_.well_b_rope_end_entity_id = kit.rope_end_entity(well_.b_rope);
        state_.well_b_rope_let_go = kit.rope_parted(well_.b_rope);
        state_.well_b_rope_tension_n = kit.rope_tension(well_.b_rope);
        state_.well_c_platform_travel = kit.guide_travel(well_.c_platform_guide);
        state_.well_c_platform_peak_speed = kit.guide_peak_speed(well_.c_platform_guide);
        state_.well_c_dumpster_travel = kit.guide_travel(well_.c_dumpster_guide);
        state_.well_c_catch_latched = kit.catch_latched(well_.c_catch);
        state_.well_c_hopper_kg = kit.bin_contents(well_.c_hopper);
        state_.well_c_dumpster_kg = kit.bin_contents(well_.c_dumpster_bin);
        state_.well_a_cage_rubble_kg = kit.bin_contents(well_.a_cage_bin);
        state_.well_rubble_spilled_kg = kit.spilled();
        state_.well_c_rebar_angle = kit.lever_angle(well_.c_rebar);
        state_.well_c_latch_angle = kit.lever_angle(well_.c_latch);

        const JPH::RVec3 rope_tipper =
            bodies.GetCenterOfMassTransform(tipper_id_) * JPH::RVec3(3.0, -0.2, 0.0);
        const JPH::RVec3 rope_lever =
            bodies.GetCenterOfMassTransform(valve_lever_id_) * JPH::RVec3(-1.60, 0.0, 0.0);
        state_.rope_extension_meters =
            JPH::Vec3(rope_lever - rope_tipper).Length() - rope_rest_length_;
    }

    void read_state() noexcept {
        const auto &bodies = physics_system_.GetBodyInterface();

        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        state_.player_position = to_vector3(player_position);
        state_.player_linear_velocity =
            {player_velocity.GetX(), player_velocity.GetY(), player_velocity.GetZ()};
        state_.player_grounded = grounded_;
        state_.player_crouched = crouched_;
        state_.support_entity_id = support_entity_id_;
        state_.support_contact_point = support_sample_.contact_point;
        state_.support_point_linear_velocity = support_sample_.point_velocity;

        const JPH::RVec3 translating_position = bodies.GetPosition(translating_support_id_);
        const JPH::Vec3 translating_velocity = bodies.GetLinearVelocity(translating_support_id_);
        state_.translating_support_position = to_vector3(translating_position);
        state_.translating_support_linear_velocity =
            {translating_velocity.GetX(), translating_velocity.GetY(), translating_velocity.GetZ()};

        const JPH::RVec3 rotating_position = bodies.GetPosition(rotating_support_id_);
        const JPH::Vec3 rotating_angular_velocity = bodies.GetAngularVelocity(rotating_support_id_);
        state_.rotating_support_position = to_vector3(rotating_position);
        state_.rotating_support_yaw_radians = rotating_support_yaw_radians_;
        state_.rotating_support_angular_velocity =
            {rotating_angular_velocity.GetX(),
             rotating_angular_velocity.GetY(),
             rotating_angular_velocity.GetZ()};

        const JPH::RVec3 moving_ledge_position = bodies.GetPosition(moving_ledge_id_);
        const JPH::Vec3 moving_ledge_velocity = bodies.GetLinearVelocity(moving_ledge_id_);
        state_.moving_ledge_position = to_vector3(moving_ledge_position);
        state_.moving_ledge_linear_velocity =
            {moving_ledge_velocity.GetX(), moving_ledge_velocity.GetY(), moving_ledge_velocity.GetZ()};

        state_.traversal_state = traversal_state_;
        state_.traversal_support_entity_id = traversal_entity_id_;
        state_.traversal_progress = traversal_progress_;
        if (traversal_state_ == TraversalState::None) {
            state_.traversal_ledge_point = {};
            state_.traversal_target_point = {};
        } else {
            state_.traversal_ledge_point =
                to_vector3(from_support_local(bodies, traversal_body_, traversal_local_ledge_));
            state_.traversal_target_point = to_vector3(
                from_support_local(bodies, traversal_target_body_, traversal_local_target_));
        }

        // Step 2: the hands' points, the structure's direction, the legs,
        // and what a grounded body is offered.
        state_.traversal_left_hand = {};
        state_.traversal_right_hand = {};
        state_.traversal_normal = {};
        if (traversal_state_ != TraversalState::None) {
            state_.traversal_normal = {traversal_normal_.GetX(), 0.0, traversal_normal_.GetZ()};
        }
        if (traversal_state_ == TraversalState::Climbing && hands_[0].valid && hands_[1].valid) {
            state_.traversal_left_hand = to_vector3(hand_point(bodies, 0));
            state_.traversal_right_hand = to_vector3(hand_point(bodies, 1));
        } else if ((traversal_state_ == TraversalState::Hanging ||
                    traversal_state_ == TraversalState::Lowering) &&
                   !traversal_normal_.IsNearZero()) {
            const JPH::RVec3 lip = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
            const JPH::Vec3 span = traversal_right() * kHangHandSpan;
            state_.traversal_left_hand = to_vector3(lip - span);
            state_.traversal_right_hand = to_vector3(lip + span);
        }
        state_.player_sprinting = sprinting_;
        state_.player_balancing = balancing_;
        state_.grip_available = grip_affordance_.valid;
        state_.grip_entity_id = grip_affordance_.valid ? grip_affordance_.entity_id : 0;
        state_.grip_point = grip_affordance_.valid ? to_vector3(grip_affordance_.point) : Vector3{};
        state_.edge_drop_available = edge_affordance_.valid;
        state_.edge_drop_point =
            edge_affordance_.valid ? to_vector3(edge_affordance_.ledge) : Vector3{};
        state_.climb_count = climb_count_;

        state_.ledge_available = affordance_.valid;
        state_.ledge_entity_id = affordance_.valid ? affordance_.ledge_entity_id : 0;
        state_.ledge_point = affordance_.valid ? to_vector3(affordance_.ledge_point) : Vector3{};
        state_.ledge_rise_meters = affordance_.valid ? affordance_.rise : 0.0;

        read_machine_state(bodies);

        state_.accepted_traversal_count = accepted_traversal_count_;
        state_.world_solid_bodies = world_solid_bodies_;
        state_.step_up_count = step_up_count_;
        state_.jump_vault_count = jump_vault_count_;
        state_.world_solid_mirrors = world_solid_mirrors_;
        state_.world_solid_rejected = world_solid_rejected_;
        state_.rejected_traversal_count = rejected_traversal_count_;
        state_.aborted_traversal_count = aborted_traversal_count_;

        state_.fall_state = !grounded_
            ? (parachute_deployed_ ? FallState::Parachuting : FallState::Airborne)
            : FallState::Grounded;
        state_.fall_peak_speed_mps = fall_peak_speed_mps_;
        state_.last_impact_speed_mps = last_impact_speed_mps_;
        state_.parachute_deployed = parachute_deployed_;
        state_.checkpoint_position = to_vector3(checkpoint_position_);
        state_.checkpoint_commit_count = checkpoint_commit_count_;
        state_.death_count = death_count_;
    }

    JoltRuntimeLease runtime_;
    JPH::TempAllocatorImpl temp_allocator_;
    JPH::JobSystemThreadPool job_system_;
    BroadPhaseLayerInterface broadphase_layer_interface_;
    ObjectVsBroadPhaseFilter object_vs_broadphase_filter_;
    ObjectLayerPairFilter object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    PlayerContactListener contact_listener_;
    JPH::RefConst<JPH::Shape> player_shape_;
    JPH::RefConst<JPH::Shape> player_crouch_shape_;
    JPH::RefConst<JPH::Shape> grip_region_shape_;
    JPH::RefConst<JPH::Shape> climb_sweep_shape_;
    JPH::BodyID deck_id_;
    JPH::BodyID translating_support_id_;
    JPH::BodyID rotating_support_id_;
    JPH::BodyID vault_rail_id_;
    JPH::BodyID mantle_ledge_id_;
    JPH::BodyID hang_ledge_id_;
    JPH::BodyID moving_ledge_id_;
    JPH::BodyID blocked_ledge_id_;
    JPH::BodyID blocked_ledge_canopy_id_;
    JPH::BodyID tower_id_;
    JPH::BodyID player_id_;

    SteamPlant steam_plant_{};
    std::vector<JPH::BodyID> machine_bodies_;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> machine_constraints_;
    JPH::Ref<JPH::HingeConstraint> tipper_hinge_;
    JPH::Ref<JPH::HingeConstraint> valve_hinge_;
    JPH::Ref<JPH::HingeConstraint> treadle_hinge_;
    JPH::Ref<JPH::HingeConstraint> jib_slew_hinge_;
    JPH::Ref<JPH::SliderConstraint> jib_hoist_slider_;
    JPH::Ref<JPH::SliderConstraint> needle_hoist_slider_;
    JPH::Ref<JPH::HingeConstraint> intake_slew_hinge_;
    JPH::Ref<JPH::SliderConstraint> intake_hoist_slider_;
    JPH::Ref<JPH::HingeConstraint> intake_dog_hinge_;
    // AS-002: unmotored -- gravity and MOD-CW-CRADLE's rope tension are the
    // only actuators, so this carries no target-velocity state.
    JPH::Ref<JPH::HingeConstraint> intake_swing_hinge_;
    JPH::Ref<JPH::PulleyConstraint> intake_cw_pulley_;
    // track_for_teardown=false: created/removed at runtime by seat_needle/
    // unseat_needle, never through machine_constraints_. See create_constraint.
    JPH::Ref<JPH::PointConstraint> needle_pin_approach_;
    JPH::Ref<JPH::PointConstraint> needle_pin_far_;
    // AS-002: the hook-pack sling, created/removed at runtime by
    // sling_pack()/release_pack_to_cradle() -- same track_for_teardown=false
    // contract as the needle pins above.
    JPH::Ref<JPH::PointConstraint> intake_sling_pin_;
    // AS-003: the carry, created/removed at runtime (track_for_teardown=false).
    JPH::Ref<JPH::PointConstraint> carry_constraint_;
    JPH::BodyID carried_id_;
    std::uint64_t carried_entity_ = 0;
    std::uint64_t carry_target_entity_ = 0;
    float grip_over_seconds_ = 0.0F;
    // AS-006: the mechanism kit and the bands built from it.
    std::unique_ptr<scraperx::sim::kit::Kit> kit_;
    scraperx::sim::bands::CounterweightWell well_{};
    scraperx::sim::bands::WetIsolation wet_{};
    scraperx::sim::bands::PlateShop shop_{};
    scraperx::sim::bands::FacadeCrane crane_{};
    scraperx::sim::bands::MidstackService service_{};
    mutable std::vector<scraperx::sim::kit::Kit::CarryCandidate> kit_carryables_;
    std::uint8_t rig_action_ = 0;
    std::uint64_t rig_target_entity_ = 0;
    scraperx::sim::kit::AnchorIndex rig_anchor_;
    scraperx::sim::kit::RopeIndex rig_rope_;
    JPH::Ref<JPH::HingeConstraint> hook5_door_hinge_;
    JPH::BodyID hook5_door_id_;
    JPH::BodyID hook5_bar_id_;
    JPH::BodyID hook5_block_id_;
    JPH::BodyID scoop_ids_[4];
    JPH::Vec3 scoop_local_[4]{};
    JPH::BodyID ballast_id_;
    JPH::BodyID tipper_id_;
    JPH::BodyID valve_lever_id_;
    JPH::BodyID treadle_id_;
    JPH::BodyID lift_platform_id_;
    JPH::BodyID counterweight_id_;
    JPH::BodyID jib_boom_id_;
    JPH::BodyID jib_hook_id_;
    JPH::BodyID crate_id_;
    JPH::BodyID capacity_stand_load_id_;
    JPH::BodyID needle_pier_approach_id_;
    JPH::BodyID needle_pier_far_id_;
    JPH::BodyID needle_beam_id_;
    JPH::BodyID sump_grate_id_;
    JPH::BodyID intake_belt_id_;
    JPH::BodyID intake_boom_id_;
    JPH::BodyID intake_hook_id_;
    JPH::BodyID intake_pack_id_;
    JPH::BodyID intake_overweight_pack_id_;
    JPH::BodyID intake_dog_id_;
    JPH::BodyID intake_swing_flight_id_;
    JPH::BodyID intake_cw_cradle_id_;
    // AS-002: the sling's actual presence -- read back for the snapshot and
    // the checkpoint, never asserted. Starts true: AS-001 pre-slings the pack.
    bool intake_pack_slung_ = true;
    float scoop_height_ = kScoopBottomY;
    float scoop_tilt_ = 0.0F;
    float valve_lever_angle_ = kValveShutAngle;
    float treadle_angle_ = kTreadleRestAngle;
    float jib_boom_angle_ = 0.0F;
    bool jib_station_active_ = false;
    bool needle_station_active_ = false;
    bool needle_seated_ = false;
    bool sump_station_active_ = false;
    bool sump_isolated_ = false;
    float sump_volume_kg_ = 0.0F;
    bool grate_safe_ = false;
    bool intake_station_active_ = false;
    float intake_boom_angle_ = 0.0F;
    float intake_dog_angle_ = 0.0F;
    bool intake_pack_pins_dog_ = false;
    bool intake_throat_clear_ = false;
    // AS-002: non-negative "how far open" travel, derived from the hinge's
    // raw (negative-signed) angle -- see kIntakeSwingHingeTravelRadians.
    float intake_swing_travel_ = 0.0F;
    float rope_rest_length_ = 0.0F;
    double machine_cycle_phase_seconds_ = 0.0;
    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    JPH::Vec3 facing_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    std::uint64_t step_up_count_ = 0;
    std::uint64_t jump_vault_count_ = 0;
    std::uint32_t jump_vault_ticks_left_ = 0;
    float jump_takeoff_feet_y_ = 0.0F;
    bool crouched_ = false;
    std::uint32_t world_solid_bodies_ = 0;
    std::uint32_t world_solid_mirrors_ = 0;
    std::uint32_t world_solid_rejected_ = 0;

    TraversalState traversal_state_ = TraversalState::None;
    JPH::BodyID traversal_body_;
    JPH::BodyID traversal_target_body_;
    std::uint64_t traversal_entity_id_ = 0;
    JPH::Vec3 traversal_local_start_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_approach_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_hold_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_ledge_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_apex_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_target_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_exit_relative_velocity_{JPH::Vec3::sZero()};
    JPH::RVec3 traversal_desired_{JPH::RVec3::sZero()};
    double traversal_progress_ = 0.0;
    double traversal_duration_ = kMantleDurationSeconds;
    double traversal_approach_fraction_ = 0.0;
    float traversal_approach_entry_slope_ = 0.0F;
    std::uint32_t traversal_stall_ticks_ = 0;
    std::uint32_t regrab_lockout_ticks_ = 0;
    std::uint64_t accepted_traversal_count_ = 0;
    std::uint64_t rejected_traversal_count_ = 0;
    std::uint64_t aborted_traversal_count_ = 0;
    LedgeProbe affordance_{};
    // Step 2 movement. The direction a hang or a climb faces its structure;
    // each hand's hold, in the frame of the body it is on; the affordances a
    // grounded body is offered; and what the legs are doing.
    struct HandHold final {
        JPH::BodyID body;
        JPH::Vec3 local{JPH::Vec3::sZero()};
        bool valid = false;
    };
    JPH::Vec3 traversal_normal_{JPH::Vec3::sZero()};
    HandHold hands_[2]{};
    std::uint32_t regrip_cooldown_ticks_ = 0;
    std::uint64_t climb_count_ = 0;
    Grip grip_affordance_{};
    Lip edge_affordance_{};
    bool sprinting_ = false;
    bool balancing_ = false;
    // Top speed the air steers toward: the walk, or what the ground gave the
    // body as it left it (a running jump keeps its speed).
    float air_full_speed_ = kPlayerMaximumRelativeSpeed;

    struct MachineCheckpoint final {
        BodyCheckpoint ballast{};
        BodyCheckpoint tipper{};
        BodyCheckpoint valve_lever{};
        BodyCheckpoint treadle{};
        BodyCheckpoint lift_platform{};
        BodyCheckpoint counterweight{};
        BodyCheckpoint jib_boom{};
        BodyCheckpoint jib_hook{};
        BodyCheckpoint crate{};
        BodyCheckpoint needle_beam{};
        bool needle_seated = false;
        float sump_volume_kg = 0.0F;
        bool sump_isolated = false;
        double vessel_mass_kg = 0.0;
        double cylinder_mass_kg = 0.0;
        // AS-002. The pack/hook and the dog are AS-001 gaps this ticket does
        // not reopen (00_START_HERE.md's record-gap convention): they are not
        // captured here either, unchanged from AS-001. MOD-STAIR-A-SWING and
        // MOD-CW-CRADLE are this ticket's own bodies, so they are.
        BodyCheckpoint intake_swing_flight{};
        BodyCheckpoint intake_cw_cradle{};
        bool intake_pack_slung = true;
        // AS-003: the door, the bar and the block, and what was being carried.
        BodyCheckpoint hook5_door{};
        BodyCheckpoint hook5_bar{};
        BodyCheckpoint hook5_block{};
        std::uint64_t carrying_entity = 0;
        // AS-006: every kit body, rope end, parted rope and catch.
        scraperx::sim::kit::Kit::Checkpoint kit{};
    };
    JPH::RVec3 checkpoint_position_{JPH::RVec3::sZero()};
    bool checkpoint_crouched_ = false;
    MachineCheckpoint checkpoint_{};
    std::uint64_t checkpoint_commit_count_ = 0;
    std::uint64_t death_count_ = 0;
    bool parachute_deployed_ = false;
    float fall_peak_speed_mps_ = 0.0F;
    float pre_contact_fall_speed_mps_ = 0.0F;
    float last_impact_speed_mps_ = 0.0F;

    Snapshot state_{};
};

Simulation::Simulation(const InitialSpawn initial_spawn)
    : physics_world_(std::make_unique<PhysicsWorld>(initial_spawn)) {
    snapshot_ = physics_world_->state();
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
}

Simulation::~Simulation() = default;

bool Simulation::set_move_input(double world_x, double world_z) noexcept {
    if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
        return false;
    }

    const double length = std::hypot(world_x, world_z);
    if (length > 1.0) {
        world_x /= length;
        world_z /= length;
    }
    move_input_x_ = world_x;
    move_input_z_ = world_z;
    return true;
}

bool Simulation::set_facing(const double world_x, const double world_z) noexcept {
    if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
        return false;
    }
    const double length = std::hypot(world_x, world_z);
    if (!(length > 1.0e-6)) {
        return false;
    }
    facing_x_ = world_x / length;
    facing_z_ = world_z / length;
    return true;
}

bool Simulation::request_jump() noexcept {
    jump_requested_ = true;
    return true;
}

bool Simulation::request_traversal() noexcept {
    if (snapshot_.traversal_state == TraversalState::Mantling ||
        snapshot_.traversal_state == TraversalState::Vaulting) {
        return false;
    }
    traversal_requested_ = true;
    return true;
}

void Simulation::set_boiler_feed_enabled(const bool enabled) noexcept {
    physics_world_->set_feed_enabled(enabled);
}

bool Simulation::request_release() noexcept {
    const bool holding = snapshot_.traversal_state == TraversalState::Hanging ||
                         snapshot_.traversal_state == TraversalState::Climbing;
    const bool standing =
        snapshot_.traversal_state == TraversalState::None && snapshot_.player_grounded;
    if (!holding && !standing) {
        return false;
    }
    release_requested_ = true;
    return true;
}

bool Simulation::request_parachute() noexcept {
    parachute_toggle_requested_ = true;
    return true;
}

bool Simulation::set_crouch_input(const bool held) noexcept {
    crouch_input_ = held;
    return true;
}

bool Simulation::set_sprint_input(const bool held) noexcept {
    sprint_input_ = held;
    return true;
}

bool Simulation::request_pick_up() noexcept {
    pick_up_requested_ = true;
    return true;
}

bool Simulation::request_set_down() noexcept {
    set_down_requested_ = true;
    return true;
}

bool Simulation::request_rig() noexcept {
    rig_requested_ = true;
    return true;
}

namespace {

[[nodiscard]] Vector3 to_sim_vector(const JPH::Vec3 value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

[[nodiscard]] Quaternion to_sim_quaternion(const JPH::Quat value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}

} // namespace

std::uint32_t Simulation::kit_body_count() const noexcept {
    return physics_world_->kit().body_count();
}

std::uint64_t Simulation::kit_body_entity(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_entity(kit::BodyIndex{body});
}

bool Simulation::kit_body_dynamic(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_dynamic(kit::BodyIndex{body});
}

bool Simulation::kit_body_enabled(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_enabled(kit::BodyIndex{body});
}

std::uint32_t Simulation::kit_body_part_count(const std::uint32_t body) const noexcept {
    return static_cast<std::uint32_t>(
        physics_world_->kit().body_parts(kit::BodyIndex{body}).size());
}

KitPart Simulation::kit_body_part(const std::uint32_t body, const std::uint32_t part) const noexcept {
    const std::vector<kit::Part> &parts = physics_world_->kit().body_parts(kit::BodyIndex{body});
    if (part >= parts.size()) {
        return {};
    }
    const kit::Part &source = parts[part];
    return {to_sim_vector(source.half), to_sim_vector(source.offset),
            to_sim_quaternion(source.rotation), static_cast<std::uint8_t>(source.material)};
}

Vector3 Simulation::kit_body_position(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_position(kit::BodyIndex{body}));
}

Vector3 Simulation::kit_body_center_of_mass(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_center_of_mass(kit::BodyIndex{body}));
}

Quaternion Simulation::kit_body_rotation(const std::uint32_t body) const noexcept {
    return to_sim_quaternion(physics_world_->kit().body_rotation(kit::BodyIndex{body}));
}

Vector3 Simulation::kit_body_velocity(const std::uint32_t body) const noexcept {
    return to_sim_vector(physics_world_->kit().body_velocity(kit::BodyIndex{body}));
}

double Simulation::kit_body_mass(const std::uint32_t body) const noexcept {
    return physics_world_->kit().body_mass(kit::BodyIndex{body});
}

std::uint32_t Simulation::kit_body_index(const std::uint64_t entity) const noexcept {
    return physics_world_->kit().body_for_entity(entity).value;
}

std::uint32_t Simulation::kit_cable_count() const noexcept {
    return physics_world_->kit().cable_count();
}

std::uint32_t Simulation::kit_cable_points(const std::uint32_t cable, Vector3 *out,
                                           const std::uint32_t capacity) const noexcept {
    if (out == nullptr) {
        return 0;
    }
    std::vector<JPH::RVec3> points;
    physics_world_->kit().cable_polyline(cable, points);
    const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(points.size(), capacity));
    for (std::uint32_t index = 0; index < count; ++index) {
        out[index] = to_sim_vector(points[index]);
    }
    return count;
}

std::uint32_t Simulation::kit_bin_count() const noexcept {
    return physics_world_->kit().bin_count();
}

KitBin Simulation::kit_bin(const std::uint32_t bin) const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    KitBin out;
    if (bin >= kit.bin_count()) {
        return out;
    }
    const kit::BinIndex index{bin};
    out.body = kit.bin_body(index).value;
    out.contents_kg = kit.bin_contents(index);
    out.capacity_kg = kit.bin_capacity(index);
    out.water = kit.bin_water(index);
    JPH::RVec3 from;
    JPH::RVec3 to;
    out.flowing = kit.bin_stream(index, from, to);
    if (out.flowing) {
        out.stream_from = to_sim_vector(from);
        out.stream_to = to_sim_vector(to);
    }
    return out;
}

std::uint32_t Simulation::kit_pile_count() const noexcept {
    return static_cast<std::uint32_t>(physics_world_->kit().piles().size());
}

KitPile Simulation::kit_pile(const std::uint32_t pile) const noexcept {
    const auto &piles = physics_world_->kit().piles();
    if (pile >= piles.size()) {
        return {};
    }
    return {to_sim_vector(piles[pile].at), piles[pile].kg};
}

std::uint32_t Simulation::kit_pool_count() const noexcept {
    return physics_world_->kit().pool_count();
}

KitPool Simulation::kit_pool(const std::uint32_t pool) const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    KitPool out;
    if (pool >= kit.pool_count()) {
        return out;
    }
    const kit::PoolIndex index{pool};
    JPH::Vec3 low;
    JPH::Vec3 high;
    kit.pool_box(index, low, high);
    out.min_corner = to_sim_vector(JPH::RVec3(low));
    out.max_corner = to_sim_vector(JPH::RVec3(high));
    out.level_m = kit.pool_level(index);
    out.water_kg = kit.pool_water(index);
    return out;
}

std::uint32_t Simulation::kit_spout_count() const noexcept {
    return physics_world_->kit().pipe_count();
}

KitSpout Simulation::kit_spout(const std::uint32_t spout) const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    KitSpout out;
    JPH::RVec3 from;
    JPH::RVec3 to;
    if (spout < kit.pipe_count() && kit.pipe_stream(kit::PipeIndex{spout}, from, to)) {
        out.pouring = true;
        out.from = to_sim_vector(from);
        out.to = to_sim_vector(to);
    }
    return out;
}

WetState Simulation::wet_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::WetIsolation &wet = physics_world_->wet();
    WetState out;
    out.d_pipe_whole = kit.pipe_whole(wet.d_fill);
    out.d_fill_kg_s = kit.pipe_flow(wet.d_fill);
    out.d_tank_kg = kit.pool_water(wet.d_tank);
    out.d_tube_kg = kit.pool_water(wet.d_tube);
    out.d_tank_level = kit.pool_level(wet.d_tank);
    out.d_tube_level = kit.pool_level(wet.d_tube);
    out.d_platform_travel = kit.guide_travel(wet.d_guide);
    out.d_valve_angle = kit.lever_angle(wet.d_valve);
    out.d_drain_angle = kit.lever_angle(wet.d_drain);
    out.dump_angle = kit.lever_angle(wet.header_dump);
    out.e_door_latched = kit.catch_latched(wet.e_door_catch);
    out.e_door_angle = kit.lever_angle(wet.e_door_hinge);
    out.e_catch_latched = kit.catch_latched(wet.e_catch);
    out.e_duct_pa = kit.cell_pressure(wet.e_duct_cell);
    out.e_cab_pa = kit.cell_pressure(wet.e_cab_cell);
    out.e_cab_travel = kit.guide_travel(wet.e_cab_guide);
    out.e_chiller_travel = kit.guide_travel(wet.e_chiller_guide);
    out.e_bucket_kg = kit.bin_contents(wet.e_bucket_bin);
    out.f_catch_latched = kit.catch_latched(wet.f_catch);
    out.f_hose_coupled = kit.rope_end_entity(wet.f_line) != Simulation::kWetFHoseEntityId;
    out.f_platform_travel = kit.guide_travel(wet.f_platform_guide);
    out.f_accumulator_travel = kit.guide_travel(wet.f_accumulator_guide);
    out.header_kg = kit.pool_water(wet.header);
    out.drained_kg = kit.drained();
    return out;
}

ShopState Simulation::shop_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::PlateShop &shop = physics_world_->shop();
    ShopState out;
    out.g_rope_on_eye = kit.rope_end_entity(shop.g_rope) == Simulation::kShopGPlatformEntityId;
    out.g_tower_latched = kit.catch_latched(shop.g_catch);
    out.g_platform_travel = kit.guide_travel(shop.g_platform_guide);
    out.g_tower_travel = kit.guide_travel(shop.g_tower_guide);
    out.h_girder_latched = kit.catch_latched(shop.h_girder_catch);
    out.h_trolley_latched = kit.catch_latched(shop.h_trolley_catch);
    out.h_girder_angle = kit.lever_angle(shop.h_girder_hinge);
    out.h_platform_travel = kit.guide_travel(shop.h_platform_guide);
    out.i_rope_on_eye = kit.rope_end_entity(shop.i_rope) == Simulation::kShopICageEntityId;
    out.i_domino_latched = kit.catch_latched(shop.i_domino_catch);
    out.i_monolith_latched = kit.catch_latched(shop.i_monolith_catch);
    out.i_domino_angle = kit.lever_angle(shop.i_domino_hinge);
    out.i_trip_angle = kit.lever_angle(shop.i_trip);
    out.i_monolith_angle = kit.lever_angle(shop.i_monolith_hinge);
    out.i_cage_travel = kit.guide_travel(shop.i_cage_guide);
    return out;
}

CraneState Simulation::crane_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::FacadeCrane &crane = physics_world_->crane();
    CraneState out;
    out.j_rail_whole = kit.rail_whole(crane.j_traveler_guide);
    out.j_wagon_latched = kit.catch_latched(crane.j_wagon_catch);
    out.j_traveler_travel = kit.guide_travel(crane.j_traveler_guide);
    out.j_wagon_travel = kit.guide_travel(crane.j_wagon_guide);
    out.k_rope_on_eye = kit.rope_end_entity(crane.k_rope) == Simulation::kCraneKCageEntityId;
    out.k_jib_latched = kit.catch_latched(crane.k_jib_catch);
    out.k_jib_angle = kit.lever_angle(crane.k_jib_hinge);
    out.k_cage_travel = kit.guide_travel(crane.k_cage_guide);
    out.l_clutch_in = kit.clutch_in(crane.l_rope);
    out.l_weight_latched = kit.catch_latched(crane.l_weight_catch);
    out.l_cart_latched = kit.catch_latched(crane.l_cart_catch);
    out.l_cart_travel = kit.guide_travel(crane.l_cart_guide);
    out.l_cab_travel = kit.guide_travel(crane.l_cab_guide);
    return out;
}

ServiceState Simulation::service_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::MidstackService &service = physics_world_->service();
    ServiceState out;
    out.m_catch_latched = kit.catch_latched(service.m_catch);
    out.m_cage_travel = kit.guide_travel(service.m_cage_guide);
    out.m_skip_travel = kit.guide_travel(service.m_skip_guide);
    out.m_rope_end_entity_id = kit.rope_end_entity(service.m_rope);
    return out;
}

bool Simulation::set_jib_slew_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    jib_slew_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_jib_hoist_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    jib_hoist_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_needle_hoist_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    needle_hoist_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::request_valve_toggle() noexcept {
    valve_toggle_requested_ = true;
    return true;
}

bool Simulation::set_intake_slew_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    intake_slew_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::set_intake_hoist_input(const double value) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    intake_hoist_input_ = std::clamp(value, -1.0, 1.0);
    return true;
}

bool Simulation::request_intake_sling_release() noexcept {
    intake_sling_release_requested_ = true;
    return true;
}

bool Simulation::request_intake_sling_attach() noexcept {
    intake_sling_attach_requested_ = true;
    return true;
}

void Simulation::step_fixed() noexcept {
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;

    PhysicsWorld::StepCommands commands;
    commands.move_input_x = move_input_x_;
    commands.move_input_z = move_input_z_;
    commands.facing_x = facing_x_;
    commands.facing_z = facing_z_;
    commands.jump_requested = jump_requested_;
    commands.traversal_requested = traversal_requested_;
    commands.release_requested = release_requested_;
    commands.crouch_held = crouch_input_;
    commands.sprint_held = sprint_input_;
    commands.pick_up_requested = pick_up_requested_;
    commands.set_down_requested = set_down_requested_;
    commands.rig_requested = rig_requested_;
    commands.parachute_toggle_requested = parachute_toggle_requested_;
    commands.jib_slew_input = jib_slew_input_;
    commands.jib_hoist_input = jib_hoist_input_;
    commands.needle_hoist_input = needle_hoist_input_;
    commands.valve_toggle_requested = valve_toggle_requested_;
    commands.intake_slew_input = intake_slew_input_;
    commands.intake_hoist_input = intake_hoist_input_;
    commands.intake_sling_release_requested = intake_sling_release_requested_;
    commands.intake_sling_attach_requested = intake_sling_attach_requested_;

    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
    parachute_toggle_requested_ = false;
    pick_up_requested_ = false;
    set_down_requested_ = false;
    rig_requested_ = false;
    valve_toggle_requested_ = false;
    intake_sling_release_requested_ = false;
    intake_sling_attach_requested_ = false;
    ++tick_index_;

    snapshot_ = physics_world_->state();
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds =
        static_cast<double>(tick_index_) * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
}

AdvanceResult Simulation::advance_frame(const double frame_delta_seconds) noexcept {
    if (!std::isfinite(frame_delta_seconds) || frame_delta_seconds < 0.0 ||
        frame_delta_seconds > kMaximumAcceptedFrameDeltaSeconds) {
        return {};
    }

    const double accumulated = remainder_seconds_ + frame_delta_seconds;
    const double step_epsilon = kFixedStepSeconds * 1.0e-9;
    const double due_as_double = std::floor((accumulated + step_epsilon) / kFixedStepSeconds);

    if (due_as_double < 0.0 ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint64_t>::max() - tick_index_)) {
        return {};
    }

    auto due = static_cast<std::uint32_t>(due_as_double);
    remainder_seconds_ = accumulated - static_cast<double>(due) * kFixedStepSeconds;

    if (remainder_seconds_ < 0.0 && remainder_seconds_ > -step_epsilon) {
        remainder_seconds_ = 0.0;
    }
    if (remainder_seconds_ >= kFixedStepSeconds &&
        remainder_seconds_ - kFixedStepSeconds < step_epsilon) {
        remainder_seconds_ = 0.0;
        ++due;
    }

    for (std::uint32_t step = 0; step < due; ++step) {
        step_fixed();
    }
    return {true, due};
}

Snapshot Simulation::snapshot() const noexcept {
    Snapshot result = snapshot_;
    result.interpolation_alpha = remainder_seconds_ / kFixedStepSeconds;
    return result;
}

} // namespace scraperx::sim
