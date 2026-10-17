#pragma once

// The tuning of the player's body and of the physics world: every constant
// the native's movement, traversal and machines are built on, with the
// reasoning beside it. Private to the native (physics_world*.cpp).

#include "sim/simulation.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "WO-003 requires the pinned Jolt physics substrate"
#endif

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <cstdint>

namespace scraperx::sim::detail {

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
// restored the body into the same slide, every time (observed carrying a
// load off a deck: committed 0.31 m past the edge, and with a 32-degree
// normal limit instead, 0.16 m past it, still creeping off). The slack
// covers a 30-degree slope, where the surface under the centre is 0.95 m
// down.
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

// Phase 4: landing recovery, a reduced-order body. The capsule is rigid, so
// Jolt stops its fall in one contact; what the legs and trunk did with that
// energy is modelled here, explicitly, as a balance debt the feet pay back
// through reduced traction (finite ground force), never as a lockout.
// - Absorbed for free: the legs take a landing up to kLandingFreeSpeed (a
//   1.8 m drop) with no lasting cost. Crouch held at contact (prepared) bends
//   them deeper: kLandingPreparedFreeSpeed, a 3.3 m drop.
// - Above that the debt grows with the normal energy past the free part, to
//   kLandingMaxLoss at the lethal speed, plus a quarter of the tangential
//   energy when the slide is across the facing (a stumble). The lethal rule
//   (kLethalImpactSpeedMps) is untouched and decided first.
// - Balance b runs from 1 - loss back to 1 over the recovery time; ground
//   acceleration (traction) scales by .25 + .75 b and the reachable speed by
//   .45 + .55 b. Input still steers and brakes at every b.
// - A roll needs crouch held at contact, a forward relative speed of at least
//   kRollMinimumSpeed within 45 degrees of the facing, and a clear, supported
//   lane for the crouched capsule as far as the roll will carry. It keeps
//   the forward momentum (braked by kRollDeceleration, rolling friction
//   rather than a planted stop), holds the 1.2 m capsule for kRollSeconds,
//   and leaves a quarter of the debt. Something in the lane ends it as a
//   heavy landing at the full debt. CHOSEN values; limitations: no
//   multi-contact body, and the normal impulse is the solver's own.
constexpr float kLandingFreeSpeed = 6.0F;
constexpr float kLandingPreparedFreeSpeed = 8.0F;
constexpr double kLandingMaxLoss = 0.9;
constexpr double kLandingPreparedLossScale = 0.6;
constexpr double kRollLossScale = 0.25;
constexpr float kRollMinimumSpeed = 2.5F;
constexpr float kRollMinimumNormalSpeed = 5.0F;
constexpr float kRollDeceleration = 3.0F;
constexpr float kRollSteerAcceleration = 4.0F;
constexpr double kRollSeconds = 0.6;
constexpr float kStumbleMinimumSpeed = 3.0F;
// Parkour step 3: the slide. Crouching at a run -- kSlideMinimumSpeed or more
// over the ground, within 45 degrees of the facing -- drops the body onto its
// hip in the 1.2 m capsule and lets it run on: the ground takes the speed off
// only as sliding friction, kSlideFriction (CHOSEN: boots and canvas on steel
// plate, about 0.35) times g, 3.4 m/s/s, never the planted stop the crouch
// walk's traction would make. The stick steers it with at most
// kSlideSteerAcceleration. It ends when the speed falls to the crouch walk's
// (kPlayerMaximumRelativeSpeed * kCrouchSpeedScale), the body stands or
// leaves the ground. Limitation: on a slope it is braked as on the flat.
// Parkour step 3: the speed vault. At kSpeedVaultMinimumSpeed or more along
// the vault, its crossing takes the run's own time (travel / speed, never
// under kSpeedVaultMinimumSeconds) instead of kVaultDurationSeconds, so the
// body goes over at the run's pace and leaves at it -- the fixed duration
// carried a sprint's 8 m/s down to about 1 m/s mid-crossing. The vault keeps
// speeds up to a sprint's (it capped them at a walk's). At a run it is offered
// and taken from kSpeedVaultLookaheadSeconds of travel ahead (up to
// kSpeedVaultMaximumReach): from arm's length a 0.95 m rail at 8 m/s would need
// the body lifted a metre in a tenth of a second.
constexpr double kSpeedVaultMinimumSpeed = 6.0;
constexpr double kSpeedVaultMinimumSeconds = 0.22;
constexpr float kSpeedVaultLookaheadSeconds = 0.25F;
constexpr float kSpeedVaultMaximumReach = 2.0F;
// Parkour step 4: the wall-run, as steps. In flight from a jump, running along
// a wall beside the body (at kWallRunMinimumSpeed or more within
// kWallRunMaximumAngle of it, the stick not turned away from it), a foot plants
// on the wall every kWallStepTicks while the body is coming down (rising, a
// foot there takes nothing), at most kWallStepsMax times before the
// feet touch ground again. Each plant pushes the body off the wall by
// kWallStepPushMps (the legs' push, CHOSEN) and the wall's friction on the
// sole, at most kWallFriction (CHOSEN: rubber on rough steel) of that push,
// resists only its sliding down: a step can stop the fall, never lift the
// body. The stick, held into the wall, brings it back for the next step by air
// control. A jump while running kicks off the wall: kWallKickMps out, and the
// friction's share up.
constexpr float kWallRunMinimumSpeed = 4.5F;
constexpr float kWallRunMaximumAngle = 0.7F;   // radians, about 40 degrees
constexpr float kWallRunProbe = 0.3F;          // past the capsule's side
constexpr std::uint32_t kWallStepTicks = 20;   // 0.22 s
constexpr std::uint32_t kWallStepsMax = 3;
constexpr float kWallStepPushMps = 1.2F;
constexpr float kWallFriction = 0.8F;
constexpr float kWallKickMps = 3.0F;
constexpr double kWallRunHoldSeconds = 0.3;
// PARKOUR_MOVE_SET.md M7, the tic-tac: the kick goes where the stick points
// as well as out from the wall -- up to kWallKickAlongMps (CHOSEN, the legs'
// push turned along the wall) in the stick's direction across the wall's
// normal -- so the body can be sent at a ledge or the facing wall, and the
// steps start again there. At most kWallKicksMax kicks in one flight: the
// legs' push is finite.
constexpr float kWallKickAlongMps = 2.0F;
// Kicks in one flight: enough for a chain across a passage, wall to facing
// wall and back (Mirror's Edge's wall-run chain), each kick needing a real
// step on the wall it leaves; not enough to pogo up one wall for ever.
constexpr std::uint32_t kWallKicksMax = 4;
// Mirror's Edge's wall climb (the owner, 2026-10-17: "running a few steps up
// a wall"). Jumped at a wall at kWallClimbMinimumSpeed or more into it, within
// kWallClimbMaximumAngle of square, the stick held at it, a foot plants on it:
// the leg takes the speed into the wall and the sole's friction on the wall,
// at most kWallFriction of that, turns it upward -- capped at
// kWallClimbMaxRiseMps up, a fast jump's (CHOSEN: about a metre of reach over
// a standing jump, 2.5 m of rise). Only a wall standing above the highest
// the jump's own arc brings the hands (probed there and at the chest) is run
// up; a lower one is caught, vaulted or mantled. Once a flight.
// Off it, a jump kicks off the wall (see kWallKickMps): the wall turn.
constexpr float kWallClimbMinimumSpeed = 3.0F;
constexpr float kWallClimbMaximumAngle = 0.5F;   // radians, about 30 degrees
constexpr float kWallClimbProbe = 0.3F;          // ahead of the capsule
constexpr float kWallClimbChestAboveFeet = 1.3F;
constexpr float kWallClimbMaxRiseMps = 7.0F;
// Parkour step 5: the grappling hook, a toy found up the tower. A hook of
// kHookMassKg is fired along the aim at kHookLaunchMps on a line of at most
// kHookLineMaxM (tension only), and flies under gravity as a body like any
// other -- what it hits, it hits. Its tines bite only round a hold (a bar,
// pipe, rung or rail: see find_grip) they strike within kHookBiteReach; on
// anything else it glances off, and with no bite in kHookMissSeconds it is
// reeled back in. Bitten, the line holds the body at its length: it hangs and
// swings on it. The reel is a winch on the line (rope_constraint.hpp): it
// takes the line in at up to kHookReelSpeedMps pulling with at most
// kHookReelForceN (CHOSEN, 1.5 body weights); past that it stalls and its
// brake holds, so it lifts the body but cannot tear it off a hold or out of
// a machine, and it never moves the body faster than the line comes in. On the taut line the stick pumps the swing as on a bar
// (kSwingKipForceN), not as air control. Let go, the body keeps its velocity.
constexpr float kHookMassKg = 1.5F;
constexpr float kHookRadius = 0.08F;
constexpr float kHookLaunchMps = 40.0F;
constexpr float kHookLineMaxM = 30.0F;
constexpr float kHookBiteReach = 0.16F;
constexpr double kHookMissSeconds = 2.5;
constexpr float kHookReelSpeedMps = 3.0F;
constexpr float kHookReelForceN = 1200.0F;
constexpr float kHookLineShortestM = 1.0F;
constexpr float kHookTautTensionN = 30.0F;
constexpr float kHookRackReach = 1.6F;
// A real rope (task #102): the line bends round what it meets. Where the
// straight line from the chest to its anchor (the bite, or the last bend)
// would pass through a body the hook could bite on (static, or of at least
// kGripMinBodyMassKg), it catches on that body's edge instead: a bend there,
// kHookWrapStandoffM off the face, takes the line, and the rope from it back
// to the anchor lies still against the steel; the reel and the swing work the
// free length from the chest to the bend. The edge is where the line, swept
// from where the chest was the tick before, first met the body (a bisection,
// kHookWrapSearchSteps halvings: ~1 mm over a swing's 30 cm tick). Swung back
// round the bend (its turn reverses) with the anchor before it in sight, the
// line comes off. Hits within kHookWrapEndM of the line's far end are the
// member it is fast to, not an edge. At most kHookBendsMax bends: past that a
// line runs straight (bounded state; counted in hook_bends_refused).
constexpr std::uint32_t kHookBendsMax = 8;
// The bite is not a weld (task #102: the hook "can slip under load"). The
// tines hold round a member up to kHookHoldForceN (CHOSEN, about 7.6 body
// weights: a swing, a hang, a hard reel never come near it). Past it for
// kHookSlipTicks on end the tines walk off the member; and a jolt whose pull
// past it adds up to kHookTearImpulseNs in one run (CHOSEN: a free fall of
// about 2.5 m caught on a taut line) tears them out. Either way the hook comes
// loose, springs back along the line toward the climber and is a hook in
// flight again: it can catch another member, or it is reeled in.
constexpr float kHookHoldForceN = 6000.0F;
constexpr float kHookTearImpulseNs = 500.0F;
constexpr int kHookSlipTicks = 14;
constexpr float kHookSpringBackMps = 6.0F;
constexpr float kHookSpringBackClearM = 0.35F;
constexpr float kHookWrapStandoffM = 0.03F;
constexpr float kHookWrapEndM = 0.15F;
constexpr int kHookWrapSearchSteps = 12;
constexpr float kSlideMinimumSpeed = 5.0F;
constexpr float kSlideFriction = 0.35F;
constexpr float kSlideSteerAcceleration = 2.0F;
// Phase 4: a support jerked under the feet. The body keeps its velocity and
// the soles re-match the support only at the ground's traction, so a jolt
// dv leaves the COM moving at dv over the footing: an XCoM shift dv / omega
// (omega = sqrt(g / kTeeterComHeight)). Past kSupportJoltMargin (CHOSEN,
// about half a foot's length) no ankle can hold it and a step is taken: a
// stumble, whose debt grows with the shift past the margin over
// kSupportJoltSpan (CHOSEN; at margin + span the debt is the most a landing
// can leave). Smooth motion (a turning platform, a lift's start) is re-matched
// tick by tick and never adds up to a shift.
constexpr float kSupportJoltMargin = 0.15F;
constexpr float kSupportJoltSpan = 0.5F;
// A blow from something moving: counted only when a body closed on the player
// at kShoveClosingMps or more across a side contact (CHOSEN: faster than a
// careful brush), and only what it gave the body past kShoveMinimumMps
// (CHOSEN: the contact solver's own settling).
// Task 91, pressure from the world: the wind off the tower's west face, where
// C7 goes out over the drop (kWindZone*), outward (-x). Its speed swells in
// gusts with the simulation's own clock (deterministic: the same inputs give
// the same gusts), from kWindBaseMps to kWindGustMps, a full gust every few
// seconds; it pushes a body that
// is not standing as drag does, 0.5 rho CdA v^2 (rho 1.2 kg/m^3, CdA 0.7 m^2
// for a person, CHOSEN): about 107 N at the gust's peak, an eighth of the
// body's weight. Standing, the soles' traction takes it.
constexpr float kWindZoneX = -26.6F;       // the west face; the wind is west of it
constexpr float kWindZoneRampM = 2.0F;     // it rises over the first 2 m out
constexpr float kWindZoneLowY = 262.0F;
constexpr float kWindZoneHighY = 300.0F;
constexpr float kWindZoneNorthZ = -166.0F;
constexpr float kWindZoneSouthZ = -128.0F;
// The summit (task 92): the same wind over the runway arm, west of the deck's
// end (Summit::kDeckWestX), from the arm's root out and 20 m above and below it.
constexpr float kSummitWindX = -12.0F;
constexpr float kSummitWindRampM = 3.0F;
constexpr float kSummitWindLowY = 788.0F;
constexpr float kSummitWindHighY = 830.0F;
constexpr float kSummitWindNorthZ = -197.0F;
constexpr float kSummitWindSouthZ = -178.0F;
constexpr float kWindBaseMps = 4.0F;
constexpr float kWindGustMps = 16.0F;
constexpr float kWindDragHalfRhoCdA = 0.5F * 1.2F * 0.7F;
constexpr float kShoveClosingMps = 0.5F;
constexpr float kShoveMinimumMps = 0.1F;

// Phase 4: balance at an edge, by the reduced-order extrapolated centre of
// mass, XCoM = COM + v / sqrt(g / h), with v the horizontal velocity relative
// to the support and h the COM's height over the soles (CHOSEN 1.0 m
// standing, 0.7 m crouched). While the XCoM is past the edge of what the feet
// stand on (no support under it) and the stick is let go or pulled back (any
// push along the motion is the player stepping it in, and a run-off stays a
// run-off), the feet can brake that motion only as hard
// as a planted human can: kTeeterBrakeAcceleration (0.85 g, the traction the
// soles have) when the stick pulls back, 60 % of it with the stick let go.
// The ground's normal 22 m/s/s does not apply there. The XCoM past the edge
// means no planted brake saves the body: the stop needs v^2 / 2a, more than
// the v / omega left whenever v > 2a / omega (5.3 m/s pulling back), and the
// two shrink at the same rate. Pulling back buys time at the edge, not a
// stop; the save is not to sprint at it. Over it, an ordinary fall takes
// over. "The edge" is where the ground stops running on toward the XCoM (see
// ground_runs_to): a slope or a stair run is ground. Below kTeeterMinimumSpeed the ordinary brake
// applies: CHOSEN above the 5.5 m/s run, so a stick let go at a run stops
// where players expect it to (every route relies on it), and teetering is
// the cost of sprinting at an edge.
constexpr float kTeeterComHeight = 1.0F;
constexpr float kTeeterCrouchedComHeight = 0.7F;
constexpr float kTeeterBrakeAcceleration = 8.3F;
constexpr float kTeeterPassiveScale = 0.6F;
constexpr double kTeeterDeliberateInput = 0.3;
constexpr float kTeeterMinimumSpeed = 6.0F;
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
// Mirror's Edge's tuck: Crouch while rising in a jump (or no faster than
// kTuckMaximumFallMps falling) draws the legs up under the body -- the short
// capsule, the head where it was, the feet kCrouchDrop higher -- to clear what
// the standing legs would strike. Let go of, the legs reach down again where
// there is room. Pressed while falling faster, Crouch readies a roll instead.
constexpr float kTuckMaximumFallMps = 1.0F;
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
// How far above a probe's lowest rise the low wall ray runs.
constexpr double kLowWallProbeAbove = 0.10;
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
// How near the obstacle's face a vault's path may bring the body before it
// has risen clear of the top.
constexpr float kVaultFaceGap = 0.02F;
// Double-tap Jump: a second Jump this soon after a takeoff asks for the vault
// the takeoff could have been. It is granted only on exactly the ground
// vault's terms -- a real obstacle top 0.35-1.15 m over the takeoff floor and
// a real landing beyond it -- so it answers a late press, and never makes a
// vault the ground itself would have refused (Governing Law 4).
constexpr std::uint32_t kJumpVaultWindowTicks =
    static_cast<std::uint32_t>(0.30 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
// Jump pressed in the air this soon before the feet come down is kept for the
// legs to use on the first tick they stand, so a press a frame early is not
// lost. It is a press held over, never a jump in the air: it fires only from
// real support (and not crouched under a ceiling), and anything else that
// takes the press -- a wall kick, the double-tap vault, a hand, a harness --
// spends it.
constexpr std::uint32_t kJumpBufferTicks =
    static_cast<std::uint32_t>(0.10 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
// Its twin the other way: a jump pressed within kCoyoteTicks of the feet
// leaving the ground without a jump (a ramp's crest run over at speed, an
// edge stepped off) still jumps, as a runner's does, the legs having loaded
// as the ground ran out. Only from a walk's leaving -- rising under
// kCoyoteMaxRiseMps (a sprint over a 27 degree crest: 8 m/s at a half
// rise), falling under kCoyoteMaxFallMps -- never off a jump (5.5 m/s), a
// kick, a launch or a let-go, which the speed alone rules out.
constexpr std::uint32_t kCoyoteTicks =
    static_cast<std::uint32_t>(0.10 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
constexpr float kCoyoteMaxRiseMps = 4.5F;
// A step up lifts the body onto the step and lets it come down on top; for
// this long after one the step-down leaves it be (it found the body a hair
// off the ground beside the step's edge and pulled it onto the edge, which
// shoved it sideways off a 0.3 m board).
constexpr std::uint32_t kStepUpGraceTicks =
    static_cast<std::uint32_t>(0.10 * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
// The grace is for a body settling onto its step; one still rising faster
// than this is a slope's launch carried over the step (a ramp's head onto
// a flat top) and is put down at once.
constexpr float kStepUpLaunchMps = 1.0F;
constexpr float kCoyoteMaxFallMps = 1.5F;
// PARKOUR_MOVE_SET.md M3, the precision jump: the sighted, committed jump
// onto a lip or ledge the eyes have picked. Grounded, the look (facing and
// pitch) is cast ahead; a surface it meets between kPrecisionMinM and
// kPrecisionMaxM out, within kPrecisionDropMaxM below or kPrecisionRiseMaxM
// above the feet, with a clear stand on it, is the sighted landing. JUMP
// then takes off at the legs' 5.5 m/s up and at the one horizontal speed
// that lands the feet there (DERIVED each jump from the arc), changed from
// the run's by at most kPrecisionPushMps (CHOSEN: the legs' push, so a
// standing precision reaches ~1.5 m and a sprint one the far side of a 4 m
// gap) and never past the sprint's speed: a landing further than the legs
// can send the body is jumped at short, no magic. The feet set down with the
// point under the sole (the capsule's radius plus kPrecisionLandingSlack)
// land as a prepared landing: the legs were ready for it. No air steering is
// added: a committed arc.
constexpr float kPrecisionMinM = 0.8F;
constexpr float kPrecisionMaxM = 4.5F;
constexpr float kPrecisionRiseMaxM = 1.2F;
constexpr float kPrecisionDropMaxM = 2.0F;
constexpr float kPrecisionPushMps = 3.0F;
constexpr float kPrecisionLandingSlack = 0.15F;
constexpr float kPrecisionLookMaxM = 9.0F;   // the look's cast, to the ground

// Hang band expressed against the capsule centre: hands reach a ledge between
// chest height and just above the head.
constexpr float kHangMinimumRiseAboveCentre = 0.45F;
constexpr float kHangMaximumRiseAboveCentre = 1.35F;
constexpr float kHangDropBelowLedge = 1.05F;
constexpr float kHangWallGap = 0.06F;
// From the hold, kHangWallGap off the wall, to where the climb-up stands the
// body, kLandingInset past it: the ground a climb-up covers from the hold.
constexpr double kHangHoldToLandingMeters = kPlayerRadius + kHangWallGap + kLandingInset;
// PARKOUR_MOVE_SET.md M6: the hands close on a lip or a hold near the apex
// of a jump, still rising (an expert's cat leap catches on the way up, and
// lands soft in the arms); past kHangMaximumClimbSpeed it is going by too
// fast for them. Up to kHangApexSpeed the body is at its apex or falling and
// the hands close as they always did; rising faster they close only in
// flight from a jump (a step's lift pops the body up as fast, reaching for
// nothing), and on a hold only where the body can pull on from it (a rising
// body pinned under the rung it has just struck is no catch: it keeps its
// flight and catches on the way down, clear). The hands reach only where
// the stick is held (kHangIntentDotThreshold): where the eyes are and the
// thumb points, never at whatever the body happens to be passing.
constexpr float kHangMaximumClimbSpeed = 2.5F;
constexpr float kHangApexSpeed = 0.2F;
constexpr double kHangIntentDotThreshold = 0.3;
// A caught ledge pulls the body in to its hold at this speed, never in one
// tick: the probe takes a wall up to kTraversalReach past the capsule, and
// a one-tick pull moved the body that whole gap in 11 ms (seen: 0.31 m in
// one frame catching the C2 duct from a keyboard leap).
constexpr float kHangPullInSpeed = 3.0F;
// Phase 3: the hang is held by arms, not placed. The hands close on the lip
// and the shoulders hang from them on a tension-only line (a distance
// constraint from the hands to a point kHangShoulderAboveCentre over the
// body's centre, at the wall's face): taut at the reach the catch was made
// at, drawn in at kHangPullInSpeed to kHangArmLength, a spring at that stop
// (the trapeze's arms: Trapeze::kArmSpringHz, kArmDamping) that takes a
// falling catch over a tenth of a second or so. Gravity stays on: the body
// hangs, and swings under the hands with the momentum it caught them with.
// The hands slip when the load passes kLedgeGripMaxForceN (CHOSEN, about 5.7
// body weights; a ledge is held harder than a bar, the fingers hooked over its
// edge and the feet on the wall) for Trapeze::kSlipTicks, or when the impulse
// over it in one run passes kLedgeGripTearImpulseNs (CHOSEN): a catch is a
// spike, not a hold. MEASURED on the proving ledge: a catch from a fall of a
// metre peaks at ~10.5 kN with ~170 N*s over the grip and holds; one from
// six metres (10.9 m/s) peaks at ~18 kN with ~700 N*s over and tears.
constexpr float kHangShoulderAboveCentre = 0.45F;
constexpr float kLedgeGripMaxForceN = 4500.0F;
constexpr float kLedgeGripTearImpulseNs = 350.0F;
// The body hanging is not a dead weight: the shoulders and core still its
// swing under the hands with up to kHangSteadyForceN (CHOSEN, about 0.36 body
// weights), against its motion relative to the hands across the arms, at
// kHangSteadyRate per second of that motion.
constexpr float kHangSteadyForceN = 300.0F;
constexpr float kHangSteadyRate = 8.0F;
constexpr float kHangArmLength = kHangDropBelowLedge - kHangShoulderAboveCentre;
// The arms' stop sits short of kHangArmLength by the spring's sag under the
// body's own weight, g / (2 pi kArmSpringHz)^2 (DERIVED, ~0.062 m, the same for
// any mass): hanging still, the body rests at its hold, not below it.
// Parkour step 2, brachiation. A bar or pipe (a hold, see find_grip) caught in
// the air over the head -- at least kSwingMinHandRise over the body's centre,
// above any climb hold -- is hung from by the same arms as a ledge, from the
// shoulders straight up. Pressing along the swing kips: at most
// kSwingKipForceN (CHOSEN, 0.2 body weights: hips and legs thrown forward),
// only while the body moves that way or hangs near still (kSwingKipFromRestMps),
// so it pumps the swing in phase and never brakes it. With the stick let go
// the body stills its swing as on a ledge (kHangSteadyForceN). Held forward, the free hand
// reaches for the next hold ahead: one within kSwingReachM of the shoulders
// (CHOSEN: an arm and the shoulder girdle at full stretch), at least
// kSwingMinAdvanceM further on, no sooner than kSwingRegripTicks after the
// last. Monkey bars a metre apart are out of reach from a dead hang (1.14 m)
// and in reach at the front of a swing: the swing is what carries the body.
constexpr float kSwingMinHandRise = 0.75F;
constexpr float kSwingCatchAimRise = 1.10F;
constexpr float kSwingCatchAimAhead = 0.15F;
constexpr float kSwingKipForceN = 160.0F;
constexpr float kSwingKipFromRestMps = 0.05F;
constexpr float kSwingReachM = 1.0F;
constexpr float kSwingReachAimAhead = 0.60F;
constexpr float kSwingReachAimRise = 0.50F;
constexpr float kSwingMinAdvanceM = 0.30F;
constexpr std::uint32_t kSwingRegripTicks = 27;
// The swing's launch (Mirror's Edge's swing pole): Jump in a swing's forward
// half -- the body moving the way it faces, faster than kSwingLaunchMinMps --
// throws it on as the hands open: the arms' pull, kSwingLaunchMps forward
// and up at 45 degrees on top of the swing's own speed (CHOSEN: well under
// the legs' 5.5 m/s jump). The bar takes the push. In the back half, or
// near still, Jump only lets go.
constexpr float kSwingLaunchMps = 2.0F;
constexpr float kSwingLaunchMinMps = 0.5F;
constexpr float kHangArmStop =
    kHangArmLength - 9.81F / ((2.0F * static_cast<float>(kPi) * scraperx::sim::Trapeze::kArmSpringHz) *
                              (2.0F * static_cast<float>(kPi) * scraperx::sim::Trapeze::kArmSpringHz));

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
// PARKOUR_MOVE_SET.md M5: a mantle made at a run leaves at a run. The speed
// the body carried in along the facing, less what the hands' and the knee's
// push onto the top costs (CHOSEN, about a walking step's worth), is the
// speed it steps off the top with; a standing mantle still stands. A
// climb-up from a hang carried nothing in and leaves with nothing.
constexpr float kMantleExitCostMps = 1.5F;

// After a deliberate release the controller stops offering an automatic re-grab
// for a moment, so letting go is a real decision rather than an instant re-hang.
constexpr std::uint32_t kReleaseRegrabLockoutTicks = 27;

// ---- Step 2 movement (03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md §8) ----
// Sprint: 8.0 m/s against the 5.5 walk, with the stick at least 0.7 and
// within 45 degrees of the facing.
constexpr double kSprintSpeedScale = 8.0 / 5.5;
// Flow (Mirror's Edge's speed, gathered and kept): past the sprint's 8 m/s a
// run kept up gathers on toward kFlowSpeedMps at kFlowGatherMps2. The legs
// run at what the body carries, so whatever a move brought in -- a vault, a
// roll, a wall-run, a slide, a jump -- is kept, and whatever a collision, a
// heavy landing or a stop took has to be gathered again from the sprint's
// speed. CHOSEN: 9 m/s (a fast runner's flat-out; a sprinter's is 10-12),
// 8 to 9 in about 1.7 s of clean running.
constexpr float kFlowSpeedMps = 9.0F;
constexpr float kFlowGatherMps2 = 0.6F;
// The springboard (Mirror's Edge's): Jump at a run (at least
// kSpringboardMinimumSpeed) with something knee- to waist-high (a vault's
// rise) close ahead, and room to stand on its top: a foot is planted on the
// top and the jump goes from there -- the same jump, from that much higher.
// The plant takes a vault's path onto the top; the step up costs the run
// some of its speed (it keeps kSpringboardSpeedKept, CHOSEN), and the foot's
// push goes into what it stood on. Close ahead: its face within
// kSpringboardStrideSeconds of the run (CHOSEN: about one stride). A foothold,
// not a rail: its top runs on kSpringboardTopDepth past the face along the
// run (CHOSEN: so the proving ground's 0.44 m rail is still vaulted).
constexpr float kSpringboardMinimumSpeed = 4.5F;
constexpr float kSpringboardTopDepth = 0.5F;
constexpr float kSpringboardStrideSeconds = 0.18F;
constexpr float kSpringboardSpeedKept = 0.92F;
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
// The jump off a climb goes where the stick points along the structure
// (Mirror's Edge's side jump off a pipe): kClimbJumpSideSpeed along it at
// full stick, and that much less of the push back off it. Neutral, or
// pulled back, it is the spring back off.
constexpr float kClimbJumpSideSpeed = 3.5F;
// The fireman's slide (its ladder slide): Crouch held on a climb loosens the
// hands, and the body drops down the holds, braked by their grip to at most
// kClimbSlideMps (CHOSEN: a fireman's pole). Out of holds, it falls; at the
// ground, the legs take the landing.
constexpr float kClimbSlideMps = 4.5F;
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
// Phase 4: across a beam the body is the inverted pendulum of the teeter
// (omega = sqrt(g / h)): x'' = omega^2 (x - p), x the COM's offset from the
// beam's line and p the centre of pressure, which the soles can put only on
// the beam: |p| <= half its width plus kBalanceHipMargin (CHOSEN: what the
// hips and arms add by swinging). p is set to bring the XCoM (x + v/omega)
// back to the line, or to the lean the stick asks for (up to
// kBalanceLeanFraction of that reach); gain kBalanceCaptureGain (CHOSEN,
// critically damped near the line). An XCoM past the reach cannot be brought
// back: the body goes off the beam. No force but the clamped centre of
// pressure acts across it.
constexpr float kBalanceHipMargin = 0.12F;
constexpr float kBalanceCaptureGain = 1.0F;
constexpr float kBalanceLeanFraction = 0.5F;
// A beam must stand over a fall: the ground is probed this far out from
// each side, from this far above its top, down to a step below it.
constexpr float kBalanceSideProbe = 0.10F;
constexpr float kBalanceSideProbeLift = 0.05F;
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
// The least a load in the hands leaves the carrier to start and stop with.
constexpr float kCarryMinimumAcceleration = 2.0F;
// A carrier keeps a fifth of the grip in hand: carried at the grip's full
// limit, D's spool swung past it on a keyboard's stop (933 N).
constexpr float kCarryGripShare = 0.80F;
// Both hands on a free load's flanks hold its turn about the vertical: this
// far out from its middle, with the grip, and settling at this rate.
constexpr float kCarryFlankArm = 0.30F;
constexpr float kCarryYawRate = 8.0F;
// A taken handle or load is drawn in to the hands at this speed, never in
// one tick: taken where it is, E's door handle was pulled to the hands and
// the 60 kg leaf swung open at 6 m/s.
constexpr float kCarryPullInSpeed = 3.0F;

// WO-008 fall / parachute / checkpoint. A 12 m unassisted fall (~15.3 m/s
// impact) must stay survivable per GDD 8.2; a genuine
// tower-scale drop must not be. Terminal parachute speed (~9 m/s, derived
// below) sits comfortably under that threshold -- Simulation::
// kLethalImpactSpeedMps, public in simulation.hpp -- with margin either side.

// Quadratic drag a = -k*v*|v|. Solved for a target terminal speed v_t at
// k = g / v_t^2 (net vertical accel is zero at v_t: g - k*v_t^2 = 0).
constexpr float kParachuteDragCoefficient = 0.1211F; // v_t ~= 9 m/s at g=9.81


// A drawn box whose world bounds match an owned body this closely is that
// body's mirror, not a second body.
constexpr float kWorldSolidMirrorTolerance = 0.02F;
constexpr float kWorldSolidFriction = 0.8F;

constexpr float kTraversalStallTolerance = 0.22F;
constexpr std::uint32_t kTraversalStallAbortTicks = 12;

} // namespace scraperx::sim::detail
