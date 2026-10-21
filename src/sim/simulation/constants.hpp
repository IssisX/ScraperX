#pragma once

#include "sim/simulation.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace scraperx::sim::detail {

namespace object_layers {
inline constexpr JPH::ObjectLayer kStatic = 0;
inline constexpr JPH::ObjectLayer kMoving = 1;
inline constexpr JPH::ObjectLayer kCount = 2;
} // namespace object_layers

namespace broadphase_layers {
inline constexpr JPH::BroadPhaseLayer kStatic(0);
inline constexpr JPH::BroadPhaseLayer kMoving(1);
inline constexpr JPH::uint kCount = 2;
} // namespace broadphase_layers

inline constexpr double kPi = 3.14159265358979323846;

inline constexpr float kSupportNormalThreshold = 0.55F;
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
inline constexpr float kCheckpointFootingSlack = 0.15F;
// A checkpoint needs a small planar footprint, not one ray touching a
// rounded edge. This still fits the narrow 0.30 m campaign girders.
inline constexpr float kCheckpointFootingMargin = 0.10F;
inline constexpr float kPlayerMaximumRelativeSpeed = 5.5F;
inline constexpr float kGroundAcceleration = 22.0F;
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
inline constexpr float kAirAcceleration = 14.0F;
inline constexpr float kJumpSpeed = 5.5F;
inline constexpr double kTranslatingSupportAmplitudeMeters = 2.0;
inline constexpr double kTranslatingSupportAngularFrequency = 1.0;
inline constexpr double kRotatingSupportAngularSpeed = 0.8;
inline constexpr double kMovingLedgeAmplitudeMeters = 1.5;
inline constexpr double kMovingLedgeAngularFrequency = 0.9;
inline constexpr double kMovingLedgeCenterZ = 12.5;

// Player capsule: cylinder half-height 0.55 plus radius 0.35.
inline constexpr float kPlayerRadius = 0.35F;
inline constexpr float kPlayerHalfHeight = 0.9F;
// Crouched (GDD 7.2): a 1.2 m capsule, same radius. The soles stay where they
// are, so the centre drops by the difference in half-heights.
inline constexpr float kPlayerCrouchHalfHeight = 0.6F;
inline constexpr float kCrouchDrop = kPlayerHalfHeight - kPlayerCrouchHalfHeight;
// Crouched walking tops out at 45 % of the standing speed (2.5 of 5.5 m/s).
inline constexpr double kCrouchSpeedScale = 0.45;
// The standing capsule is tested this far above the soles, so the floor it
// already rests on does not read as an obstruction; a ceiling does.
inline constexpr float kStandClearanceSkin = 0.05F;
// Crouch fixture: a beam across a 4 m lane, underside 1.45 m over the deck --
// under the standing capsule's 1.8 m, over the crouched one's 1.2 m -- on two
// posts. Between the traversal fixtures and the kerb run at z = -16.
inline constexpr float kCrawlLaneCenterX = -2.0F;
inline constexpr float kCrawlLaneHalfX = 2.0F;
inline constexpr float kCrawlBeamZ = -13.0F;
inline constexpr float kCrawlBeamHalfZ = 0.3F;
inline constexpr float kCrawlBeamUndersideY = 1.45F;
inline constexpr float kCrawlBeamHalfY = 0.2F;
inline constexpr float kCrawlPostHalf = 0.15F;

// An athletic climber with gear. Left to Jolt's default density this capsule
// weighs 602.9 kg -- freight, not a person -- which silently made the player
// the heaviest thing in any mechanism they stood on. GDD 17 puts the power in
// the tower, not the body; a machine that needs a human's weight must be built
// around a human's weight.
inline constexpr float kPlayerMassKg = 85.0F;

// Traversal reach / clearance rules. Every one of these is a bound on what the
// native assist may attempt; none of them fabricates geometry.
inline constexpr float kTraversalReach = 0.95F;
inline constexpr float kTopProbeInset = 0.12F;
inline constexpr float kLandingInset = kPlayerRadius + 0.12F;
inline constexpr float kTopProbeMargin = 0.35F;
inline constexpr float kLedgeTopNormalThreshold = 0.7F;
inline constexpr float kLandingSkin = 0.02F;
inline constexpr float kLandingSupportProbeUp = 0.12F;
inline constexpr float kLandingSupportTolerance = 0.10F;

// Walking up a step. An edge lower than this in the path of a grounded,
// walking body is climbed the way legs climb it; anything taller is a vault
// or a mantle (both start at 0.35 m), so no height is left unclaimed.
inline constexpr float kStepMaximumHeight = 0.35F;
inline constexpr float kStepMinimumHeight = 0.02F;
inline constexpr float kStepLookahead = 0.08F;
inline constexpr float kStepMinimumSpeed = 0.2F;

inline constexpr float kMantleMinimumRise = 0.35F;
inline constexpr float kMantleMaximumRise = 1.85F;
inline constexpr float kMantleClearanceLift = 0.12F;
inline constexpr double kMantleDurationSeconds = 0.42;

inline constexpr float kVaultMinimumRise = 0.35F;
inline constexpr float kVaultMaximumRise = 1.15F;
inline constexpr float kVaultCrossDistance = 1.30F;
inline constexpr float kVaultApexClearance = 0.10F;
inline constexpr float kVaultMaximumDrop = 1.40F;
inline constexpr double kVaultDurationSeconds = 0.38;
// Double-tap Jump: a second Jump this soon after a takeoff asks for the vault
// the takeoff could have been. It is granted only on exactly the ground
// vault's terms -- a real obstacle top 0.35-1.15 m over the takeoff floor and
// a real landing beyond it -- so it answers a late press, and never makes a
// vault the ground itself would have refused (Governing Law 4).
inline constexpr std::uint32_t kJumpVaultWindowTicks =
    static_cast<std::uint32_t>(0.30 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));

// Intent lasts eleven native ticks (~122 ms). Contact sampled at a tick's
// end is eligible on the following tick; this window supplies no support.
inline constexpr std::uint32_t kOrdinaryJumpIntentTicks = 11;
// A deliberate hand-supported pull retains the existing0.25m stroke and
// 500W supplied-spring-work limit: 2*1500N*0.25m/500W=1.5s at worst load.
// Retain the ordinary intent window for the solved body's response. This
// lifetime grants no reach, force, footing or guaranteed settling time.
inline constexpr std::uint32_t kHangTopoutPreparationTicks =
    static_cast<std::uint32_t>(1.5 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz)) +
    kOrdinaryJumpIntentTicks;
// Intent ceiling only: real foot reach, traction and lifetime can end sooner.
inline constexpr std::uint32_t kCoyotePlantIntentTicks = 8;

// Hang band expressed against the capsule centre: hands reach a ledge between
// chest height and just above the head.
inline constexpr float kHangMinimumRiseAboveCentre = 0.45F;
inline constexpr float kHangMaximumRiseAboveCentre = 1.35F;
inline constexpr float kHangDropBelowLedge = 1.05F;
inline constexpr float kHangWallGap = 0.06F;
inline constexpr float kHangMaximumClimbSpeed = 0.2F;
inline constexpr double kHangIntentDotThreshold = 0.3;

// A standing mantle steps in before it climbs. The probe offers a ledge from
// 1.30 m out, and a body that rises from there hangs an arm's length off the
// wall with nothing under its hands; a climber closes on the wall, hands on
// the lip, then pulls. The step ends at the hang hold's own standoff, so a
// mantle from the ground passes the wall distance a mantle from a hang starts
// at -- and it is a step: swept clear of real geometry, onto real support, in
// a frame the ground and the ledge share. Where any of that fails the step
// shortens, down to none, and the climb starts where the player stands.
inline constexpr float kMantleApproachStandoff = kPlayerRadius + kHangWallGap;
inline constexpr float kMantleApproachSpeedMps = 3.0F;        // mean speed over the step
inline constexpr float kMantleApproachLift = 0.05F;           // clears the resting contact
inline constexpr float kMantleApproachSweepMargin = 0.02F;
inline constexpr float kMantleApproachSupportSpacing = 0.10F;
inline constexpr float kMantleApproachFrameDriftMps = 0.05F;  // ground vs ledge
inline constexpr float kMantleApproachMaximumEntrySlope = 3.0F;

// After a deliberate release the controller stops offering an automatic re-grab
// for a moment, so letting go is a real decision rather than an instant re-hang.
inline constexpr std::uint32_t kReleaseRegrabLockoutTicks = 27;

// ---- AS-017 traversal adapted from ScraperX-Claude f872c41 ----
// Sprint: 8.0 m/s against the 5.5 walk, with the stick at least 0.7 and
// within 45 degrees of the facing.
inline constexpr double kSprintSpeedScale = 8.0 / 5.5;
inline constexpr double kSprintMinimumInput = 0.7;
inline constexpr double kSprintMaximumAngleCos = 0.7071;
// A hold: a member a hand can close round -- its two thinner dimensions at
// most kGripMaxSection, its length at least kGripMinLength. The real body's
// mass, contacts and joints respond to the finite reciprocal hand forces.
inline constexpr float kGripMaxSection = 0.18F;
inline constexpr float kGripMinLength = 0.25F;
// Where a hand looks for a hold: a box this big (across, up, along the
// facing) round the point it aims at.
const JPH::Vec3 kGripSearchHalf(0.30F, 0.30F, 0.35F);
// Climbing: the body moves at these rates. Its hands aim this high over its
// centre, this far in front of it and this far to either side; one hand
// moves at a time.
inline constexpr float kClimbUpSpeed = 0.9F;
inline constexpr float kClimbDownSpeed = 1.2F;
inline constexpr float kClimbSideSpeed = 0.6F;
inline constexpr double kClimbInputDeadzone = 0.2;
inline constexpr float kClimbHandHigh = 0.80F;
// Climbing on up needs a hold from chest height: a body pushes up on holds at
// its chest, so it rises to the top of a pipe, not an arm's length short.
inline constexpr float kClimbMoveAim = 0.50F;
inline constexpr float kClimbHandMid = 0.45F;
inline constexpr float kTransferShoulderReachM = 1.05F;
inline constexpr float kClimbHandLow = 0.20F;
inline constexpr float kClimbHandReach = 0.40F;
inline constexpr float kClimbHandSpan = 0.20F;
inline constexpr float kClimbSideLead = 0.35F;
inline constexpr std::uint32_t kClimbRegripTicks = 18;
inline constexpr float kClimbJumpBackSpeed = 2.5F;
inline constexpr float kClimbJumpUpSpeed = 4.0F;
// Climbing down, the soles find ground this close under them and step off.
inline constexpr float kClimbFootReach = 0.10F;
// Shimmy along a hung ledge.
inline constexpr float kShimmySpeed = 0.6F;
// Balance: walking a support narrower than kBalanceMaxWidth and at least
// kBalanceMinLength long. Sideways input below kBalanceStepOffInput requests
// fine correction; at/above it, the body may deliberately step off.
inline constexpr float kBalanceMaxWidth = 0.50F;
inline constexpr float kBalanceMinLength = 1.50F;
inline constexpr double kBalanceSpeedScale = 2.0 / 5.5;
inline constexpr double kBalanceStepOffInput = 0.8;
inline constexpr float kBalanceCorrectionMaxMps = 0.4F;
// A beam must stand over a fall: the ground is probed this far out from
// each side, from this far above its top, down to a step below it.
inline constexpr float kBalanceSideProbe = 0.10F;
inline constexpr float kBalanceSideProbeLift = 0.05F;
// Controlled drop: an edge behind the body within kEdgeSearchReach, over a
// drop of at least kEdgeDropMinimum, lowered over in kLoweringSeconds.
inline constexpr float kEdgeSearchStart = 0.25F;
inline constexpr float kEdgeSearchReach = 0.90F;
inline constexpr float kEdgeSearchStep = 0.05F;
inline constexpr float kEdgeDropMinimum = 1.5F;
inline constexpr float kLipProbeDepth = 0.05F;
inline constexpr float kClimbSweepRadius = 0.30F;
// A hang's hands, either side of the body on the lip.
inline constexpr float kHangHandSpan = 0.22F;
inline constexpr double kLoweringSeconds = 0.6;

inline constexpr float kTraversalStallTolerance = 0.22F;
inline constexpr std::uint32_t kTraversalStallAbortTicks = 12;


} // namespace scraperx::sim::detail
