#include "sim/simulation.hpp"

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
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
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
constexpr float kPlayerMaximumRelativeSpeed = 5.5F;
constexpr float kGroundAcceleration = 22.0F;
constexpr float kAirAcceleration = 8.0F;
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

// Hang band expressed against the capsule centre: hands reach a ledge between
// chest height and just above the head.
constexpr float kHangMinimumRiseAboveCentre = 0.45F;
constexpr float kHangMaximumRiseAboveCentre = 1.35F;
constexpr float kHangDropBelowLedge = 1.05F;
constexpr float kHangWallGap = 0.06F;
constexpr float kHangMaximumClimbSpeed = 0.2F;
constexpr double kHangIntentDotThreshold = 0.3;

// After a deliberate release the controller stops offering an automatic re-grab
// for a moment, so letting go is a real decision rather than an instant re-hang.
constexpr std::uint32_t kReleaseRegrabLockoutTicks = 27;

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
// Where the tower's unclimbable mass resumes above the playable slice.
constexpr float kTowerMassBaseY =
    static_cast<float>(kStackLevelCount) * kStackLevelHeight + 5.0F;

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
// below) sits comfortably under this threshold with margin either side.
constexpr float kLethalImpactSpeedMps = 20.0F;

// Quadratic drag a = -k*v*|v|. Solved for a target terminal speed v_t at
// k = g / v_t^2 (net vertical accel is zero at v_t: g - k*v_t^2 = 0).
constexpr float kParachuteDragCoefficient = 0.1211F; // v_t ~= 9 m/s at g=9.81

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
    return entity_id == Sim::kTranslatingSupportEntityId ||
           entity_id == Sim::kRotatingSupportEntityId ||
           entity_id == Sim::kMovingLedgeEntityId ||
           entity_id == Sim::kHoistScoopEntityId ||
           entity_id == Sim::kTipperEntityId ||
           entity_id == Sim::kLiftPlatformEntityId ||
           entity_id == Sim::kCounterweightEntityId ||
           entity_id == Sim::kTreadleEntityId ||
           entity_id == Sim::kJibHookEntityId ||
           entity_id == Sim::kCrateEntityId ||
           entity_id == Sim::kNeedleBeamEntityId;
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
                                           const float delta_seconds) noexcept {
    float relative_x = world_velocity.GetX() - reference_velocity.GetX();
    float relative_z = world_velocity.GetZ() - reference_velocity.GetZ();
    const float target_x = static_cast<float>(move_input_x) * kPlayerMaximumRelativeSpeed;
    const float target_z = static_cast<float>(move_input_z) * kPlayerMaximumRelativeSpeed;
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

} // namespace

namespace scraperx::sim {

class Simulation::PhysicsWorld final {
public:
    explicit PhysicsWorld(const InitialSpawn initial_spawn)
        : temp_allocator_(8U * 1024U * 1024U),
          job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1) {
        // Raised from 256: the climbable stack adds ~90 static frame bodies.
        physics_system_.Init(1024,
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

        // The tower's upper mass: still 1.6 km of it, but it now begins above
        // the playable slice instead of at grade. Everything below this line
        // is the real, climbable frame built by build_stack().
        const float mass_half_height =
            static_cast<float>(Simulation::kTowerHeightMeters * 0.5) - kTowerMassBaseY * 0.5F;
        // Set well back from the climbable frame: overhead and close, it read
        // as a black void hanging over the stack rather than as a neighbouring
        // structure receding into the weather.
        tower_id_ = add_box(bodies,
                            JPH::Vec3(46.0F, mass_half_height, 40.0F),
                            JPH::RVec3(-30.0, kTowerMassBaseY + mass_half_height, -330.0),
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

        build_stack(bodies);
        build_machine(bodies);
        build_kernel_jib(bodies);
        build_kernel_needle(bodies);
        build_kernel_sump(bodies);

        player_shape_ = new JPH::CapsuleShape(0.55F, kPlayerRadius);
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
        bool parachute_toggle_requested = false;
        double jib_slew_input = 0.0;
        double jib_hoist_input = 0.0;
        double needle_hoist_input = 0.0;
        bool valve_toggle_requested = false;
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

        // A toggle while airborne only: deploying/retracting on the ground is
        // meaningless and would let a grounded button-mash pre-arm the canopy.
        if (commands.parachute_toggle_requested && !was_grounded_before_tick) {
            parachute_deployed_ = !parachute_deployed_;
        }

        if (regrab_lockout_ticks_ > 0) {
            --regrab_lockout_ticks_;
        }
        facing_ = normalized_horizontal(commands.facing_x, commands.facing_z);

        apply_traversal_commands(bodies, commands);

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
        }

        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);

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
        if (!died_this_tick && grounded_ && traversal_state_ == TraversalState::None) {
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
        track(add_box(bodies, JPH::Vec3(1.0F, 0.15F, 0.8F), JPH::RVec3(29.5, 3.65, -94.05),
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
        add_pulley();
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

        for (int level = 1; level <= kStackLevelCount; ++level) {
            const float deck_y = static_cast<float>(level) * kStackLevelHeight;
            const float slab_y = deck_y - kStackDeckHalfThickness;

            // Deck ring: two full-width bands and two inner bands, leaving a
            // 22 m shaft open through every level.
            for (const float sz : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackHalfExtent, kStackDeckHalfThickness,
                                kStackDeckBandDepth * 0.5F),
                      JPH::RVec3(kStackCenterX, slab_y, kStackCenterZ + sz * band_center));
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
            // Runs along X on alternating Z bands, climbing in +X or -X.
            const JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), side * pitch);
            frame(JPH::Vec3(length * 0.5F, 0.18F, kStackRampHalfWidth),
                  JPH::RVec3(kStackCenterX, base_y + rise * 0.5F,
                             kStackCenterZ + side * band_center),
                  rotation);
        }
    }

    // WO-013. Ascent Atlas v1.0 kernel (section 9): KX-SUMP + KX-GRATE. Fixed
    // approach/far decking flank one grate panel; only the grate's own
    // collidability changes, driven by update_sump every tick. Starts wet
    // (grate is a sensor -- see create) since the sump starts full.
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
                            JPH::Ref<JPH::HingeConstraint> *out) {
        JPH::HingeConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = point;
        settings.mPoint2 = point;
        settings.mHingeAxis1 = JPH::Vec3::sAxisY();
        settings.mHingeAxis2 = JPH::Vec3::sAxisY();
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

    void add_pulley() {
        JPH::PulleyConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mBodyPoint1 = JPH::RVec3(kLiftPlatformX, kLiftPlatformRestY + 0.16, kLiftZ);
        settings.mFixedPoint1 = JPH::RVec3(kLiftPlatformX, kSheaveY, kLiftZ);
        settings.mBodyPoint2 = JPH::RVec3(kCounterweightX, 8.3, kLiftZ);
        settings.mFixedPoint2 = JPH::RVec3(kCounterweightX, kSheaveY, kLiftZ);
        settings.mRatio = 1.0F;
        settings.mMinLength = 0.0F;
        settings.mMaxLength = -1.0F;
        (void)create_constraint(settings, lift_platform_id_, counterweight_id_);
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

    [[nodiscard]] JPH::Vec3 surface_normal(const JPH::BodyID body_id,
                                           const JPH::SubShapeID &sub_shape_id,
                                           const JPH::RVec3 point) const noexcept {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
        if (!lock.Succeeded()) {
            return JPH::Vec3::sZero();
        }
        return lock.GetBody().GetWorldSpaceSurfaceNormal(sub_shape_id, point);
    }

    [[nodiscard]] bool capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept {
        JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
        JPH::CollideShapeSettings settings;
        settings.mMaxSeparationDistance = 0.0F;
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        physics_system_.GetNarrowPhaseQuery().CollideShape(player_shape_,
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
                                         const bool require_supported_landing) const noexcept {
        LedgeProbe probe;
        if (facing.IsNearZero()) {
            return probe;
        }

        JPH::RayCastResult wall_hit;
        if (!cast_ray(origin, facing * (kTraversalReach + kPlayerRadius), wall_hit)) {
            return probe;
        }
        const JPH::RVec3 wall_point =
            JPH::RRayCast(origin, facing * (kTraversalReach + kPlayerRadius))
                .GetPointOnRay(wall_hit.mFraction);

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

        if (traversal_state_ != TraversalState::None) {
            if (commands.traversal_requested || commands.release_requested) {
                ++rejected_traversal_count_;
            }
            return;
        }

        if (commands.traversal_requested && !try_begin_ground_traversal(bodies)) {
            ++rejected_traversal_count_;
        }
    }

    [[nodiscard]] bool apply_locomotion(JPH::BodyInterface &bodies,
                                        const StepCommands &commands,
                                        const float delta_seconds) noexcept {
        JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        JPH::Vec3 reference_velocity = airborne_inherited_velocity_;

        if (grounded_ && support_entity_id_ != 0) {
            reference_velocity = current_support_point_velocity(bodies);
            airborne_inherited_velocity_ = reference_velocity;
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  commands.move_input_x,
                                                  commands.move_input_z,
                                                  kGroundAcceleration,
                                                  delta_seconds);
        } else {
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  commands.move_input_x,
                                                  commands.move_input_z,
                                                  kAirAcceleration,
                                                  delta_seconds);
        }

        const bool jump_started = commands.jump_requested && grounded_;
        if (jump_started) {
            player_velocity.SetY(reference_velocity.GetY() + kJumpSpeed);
        }
        bodies.SetLinearVelocity(player_id_, player_velocity);
        return jump_started;
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
        if (grounded_ || regrab_lockout_ticks_ > 0) {
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
            return;
        }

        const JPH::RVec3 hold(probe.wall_point.GetX() - facing_.GetX() * (kPlayerRadius + kHangWallGap),
                              probe.ledge_point.GetY() - kHangDropBelowLedge,
                              probe.wall_point.GetZ() - facing_.GetZ() * (kPlayerRadius + kHangWallGap));

        traversal_state_ = TraversalState::Hanging;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = probe.landing_body;
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

    void begin_mantle(JPH::BodyInterface &bodies,
                      const LedgeProbe &probe,
                      const JPH::RVec3 origin) noexcept {
        traversal_state_ = TraversalState::Mantling;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = probe.landing_body;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_target_ =
            to_support_local(bodies, traversal_target_body_, probe.landing_centre);
        traversal_progress_ = 0.0;
        traversal_duration_ = kMantleDurationSeconds;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    void begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        traversal_state_ = TraversalState::Mantling;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_progress_ = 0.0;
        traversal_duration_ = kMantleDurationSeconds;
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

        const float vertical = smoothstep(0.0F, 0.55F, progress);
        const float horizontal = smoothstep(0.45F, 1.0F, progress);
        const float lift =
            kMantleClearanceLift * std::sin(static_cast<float>(kPi) * std::clamp(progress, 0.0F, 1.0F));
        return JPH::RVec3(start.GetX() + (target.GetX() - start.GetX()) * horizontal,
                          start.GetY() + (target.GetY() - start.GetY()) * vertical + lift,
                          start.GetZ() + (target.GetZ() - start.GetZ()) * horizontal);
    }

    void drive_traversal(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
        const JPH::RVec3 current = bodies.GetPosition(player_id_);

        if (traversal_state_ == TraversalState::Hanging) {
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

        if (traversal_state_ != TraversalState::Hanging && traversal_progress_ >= 1.0) {
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

    void clear_traversal() noexcept {
        traversal_state_ = TraversalState::None;
        traversal_body_ = {};
        traversal_target_body_ = {};
        traversal_entity_id_ = 0;
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    }

    void update_affordance(const JPH::BodyInterface &bodies) noexcept {
        affordance_ = {};
        if (traversal_state_ != TraversalState::None || facing_.IsNearZero()) {
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
    }

    // WO-008 automatic commit (GDD 9.1): every tick the player is grounded and
    // not mid-traversal, so the checkpoint is always "wherever the player was
    // last standing." No dwell timer, no player-facing save action.
    void commit_checkpoint(const JPH::BodyInterface &bodies) noexcept {
        checkpoint_position_ = bodies.GetPosition(player_id_);
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
        // No topology reconciliation call needed here, unlike the needle:
        // update_sump recomputes grate_safe_ and reasserts the grate's
        // sensor flag from sump_volume_kg_ unconditionally every tick, so
        // restoring the scalar is the whole restore.
        sump_volume_kg_ = checkpoint_.sump_volume_kg;
        sump_isolated_ = checkpoint_.sump_isolated;
        steam_plant_.restore_state(checkpoint_.vessel_mass_kg, checkpoint_.cylinder_mass_kg);

        grounded_ = false;
        support_entity_id_ = 0;
        support_sample_ = {};
        airborne_inherited_velocity_ = JPH::Vec3::sZero();
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

        state_.ledge_available = affordance_.valid;
        state_.ledge_entity_id = affordance_.valid ? affordance_.ledge_entity_id : 0;
        state_.ledge_point = affordance_.valid ? to_vector3(affordance_.ledge_point) : Vector3{};
        state_.ledge_rise_meters = affordance_.valid ? affordance_.rise : 0.0;

        read_machine_state(bodies);

        state_.accepted_traversal_count = accepted_traversal_count_;
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
    // track_for_teardown=false: created/removed at runtime by seat_needle/
    // unseat_needle, never through machine_constraints_. See create_constraint.
    JPH::Ref<JPH::PointConstraint> needle_pin_approach_;
    JPH::Ref<JPH::PointConstraint> needle_pin_far_;
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
    float rope_rest_length_ = 0.0F;
    double machine_cycle_phase_seconds_ = 0.0;
    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    JPH::Vec3 facing_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    TraversalState traversal_state_ = TraversalState::None;
    JPH::BodyID traversal_body_;
    JPH::BodyID traversal_target_body_;
    std::uint64_t traversal_entity_id_ = 0;
    JPH::Vec3 traversal_local_start_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_hold_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_ledge_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_apex_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_target_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_exit_relative_velocity_{JPH::Vec3::sZero()};
    JPH::RVec3 traversal_desired_{JPH::RVec3::sZero()};
    double traversal_progress_ = 0.0;
    double traversal_duration_ = kMantleDurationSeconds;
    std::uint32_t traversal_stall_ticks_ = 0;
    std::uint32_t regrab_lockout_ticks_ = 0;
    std::uint64_t accepted_traversal_count_ = 0;
    std::uint64_t rejected_traversal_count_ = 0;
    std::uint64_t aborted_traversal_count_ = 0;
    LedgeProbe affordance_{};

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
    };
    JPH::RVec3 checkpoint_position_{JPH::RVec3::sZero()};
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
    if (snapshot_.traversal_state != TraversalState::Hanging) {
        return false;
    }
    release_requested_ = true;
    return true;
}

bool Simulation::request_parachute() noexcept {
    parachute_toggle_requested_ = true;
    return true;
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
    commands.parachute_toggle_requested = parachute_toggle_requested_;
    commands.jib_slew_input = jib_slew_input_;
    commands.jib_hoist_input = jib_hoist_input_;
    commands.needle_hoist_input = needle_hoist_input_;
    commands.valve_toggle_requested = valve_toggle_requested_;

    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
    parachute_toggle_requested_ = false;
    valve_toggle_requested_ = false;
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
