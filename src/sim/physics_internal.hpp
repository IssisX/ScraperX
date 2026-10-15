#pragma once

// Private to the physics translation units. The public API stays
// simulation.hpp; nothing outside src/sim includes this.

#include "sim/simulation.hpp"

#include "sim/bands.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/slingshot.hpp"

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


namespace scraperx::sim {


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
inline const JPH::Vec3 kGripSearchHalf(0.30F, 0.30F, 0.35F);
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
// A hang can be kicked out from the wall and let go with that speed.
// Angle is radians about the ledge, away from the structure.
constexpr float kSwingMax = 0.85F;
constexpr float kSwingPump = 2.6F;
constexpr float kSwingSpring = 3.2F;
constexpr float kSwingDamp = 1.8F;
constexpr float kReleaseSpeedCap = 7.0F;
// A pull on the pitman's bar, newtons. Full when that pull goes with the
// bar's motion, a fifth of it when the pull is against the motion. The
// rider's weight is applied once, by load_hold, not again here.
constexpr float kPitmanPumpForce = 1500.0F;
constexpr float kPitmanPumpOpposed = 0.20F;
constexpr float kPitmanCoastSpeed = 0.15F;
constexpr std::uint32_t kPitmanRegrabTicks = 80;
// A hard landing under the lethal line keeps the horizontal speed the fall
// arrived with. Contact was eating it.
constexpr float kRollKeepImpact = 6.0F;
constexpr float kLipCatchReach = 1.35F;
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

// WO-008 fall / parachute / checkpoint. A 12 m unassisted fall (~15.3 m/s
// impact) must stay survivable per GDD 8.2; a genuine
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
    inline static std::mutex mutex_;
    inline static std::uint32_t lease_count_ = 0;
};


struct SupportSample final {
    bool grounded = false;
    std::uint64_t entity_id = 0;
    scraperx::sim::Vector3 contact_point{};
    scraperx::sim::Vector3 point_velocity{};
    float normal_y = 0.0F;
};

// Supports that actually move. These outrank static ground when the player is
// in contact with both, so support-relative locomotion picks the machine.
[[nodiscard]] inline bool entity_is_moving_support(const std::uint64_t entity_id) noexcept {
    using Sim = scraperx::sim::Simulation;
    // Mechanism-kit bodies that move are machines by construction.
    return scraperx::sim::kit::is_dynamic_entity(entity_id) ||
           entity_id == Sim::kTranslatingSupportEntityId ||
           entity_id == Sim::kRotatingSupportEntityId ||
           entity_id == Sim::kMovingLedgeEntityId;
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

    void set_slingshot(scraperx::sim::Slingshot *launcher) noexcept {
        slingshot_ = launcher;
    }

    // The player and what it carries share the carry point: contact between
    // them is never meaningful, only the constraint relates them.
    [[nodiscard]] JPH::ValidateResult OnContactValidate(
        const JPH::Body &first, const JPH::Body &second, JPH::RVec3Arg,
        const JPH::CollideShapeResult &) override {
        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        using Sim = scraperx::sim::Simulation;
        const std::uint64_t carried = carried_entity_.load(std::memory_order_relaxed);
        const bool is_player_vs_carried =
            carried != 0 &&
            ((first_entity == Sim::kPlayerEntityId && second_entity == carried) ||
             (first_entity == carried && second_entity == Sim::kPlayerEntityId));
        return is_player_vs_carried ? JPH::ValidateResult::RejectAllContactsForThisBodyPair
                                    : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body &first,
                        const JPH::Body &second,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &) override {
        note_slingshot(first, second);
        observe_support(first, second, manifold);
    }

    void OnContactPersisted(const JPH::Body &first,
                            const JPH::Body &second,
                            const JPH::ContactManifold &manifold,
                            JPH::ContactSettings &) override {
        note_slingshot(first, second);
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

        // A sensor body still produces a full contact manifold -- Jolt's own
        // doc comment is explicit that sensors "will receive collision
        // callbacks, but will not cause any collision responses" -- so without
        // this check a sensor would read as real support from geometry alone,
        // with nothing holding the player up. A sensor is never a support.
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

    void note_slingshot(const JPH::Body &first, const JPH::Body &second) noexcept {
        if (slingshot_ == nullptr) {
            return;
        }
        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        if (first_entity == scraperx::sim::Slingshot::kPouchEntity ||
            second_entity == scraperx::sim::Slingshot::kPouchEntity) {
            slingshot_->note_pouch_contact();
        }
        if (first_entity == scraperx::sim::Slingshot::kLaunchRailEntity ||
            second_entity == scraperx::sim::Slingshot::kLaunchRailEntity) {
            slingshot_->note_carrier_contact();
        }
        if (first_entity == scraperx::sim::Simulation::kPlayerEntityId ||
            second_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            slingshot_->note_external_influence();
        }
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
    scraperx::sim::Slingshot *slingshot_ = nullptr;
};

[[nodiscard]] inline JPH::RVec3 spawn_position(const scraperx::sim::InitialSpawn spawn) noexcept {
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
    case scraperx::sim::InitialSpawn::Deck154:
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
    case scraperx::sim::InitialSpawn::Deck2South:
        // On deck 2's south band, north of S1's corbel.
        return {10.0, 23.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck4South:
        // On deck 4's south band, west of C1's davit.
        return {11.0, 45.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck6South:
        // On deck 6's south band, by S2's upper gangway.
        return {10.0, 67.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck8North:
        // On deck 8's north band, where C2 lands.
        return {10.0, 89.2, -170.0};
    case scraperx::sim::InitialSpawn::Deck12North:
        // On deck 12's north band, where S3 lands.
        return {-8.0, 133.2, -170.0};
    case scraperx::sim::InitialSpawn::ServiceSkinWest:
        // On TP-640, south of the service manifold, in the lane it crosses.
        return {-10.30, 641.15, -147.15};
    case scraperx::sim::InitialSpawn::AfterCrane:
        // On the 640 m floor, where the last crane car lets you off.
        return {-3.0, 641.20, -156.50};
    case scraperx::sim::InitialSpawn::CasingGallery:
        // On the fan-casing gallery, just south of the pitman's bar.
        return {9.40, 760.00, -150.55};
    case scraperx::sim::InitialSpawn::PitmanDeck:
        // On the pitman's landing, where that swing lets you off.
        return {9.40, 759.90, -146.10};
    case scraperx::sim::InitialSpawn::WheelDeck:
        // On the wheel's high deck, where the car lets you off.
        return {9.40, 765.10, -138.70};
    case scraperx::sim::InitialSpawn::BracePerch:
        // On the perch west of that deck, east of the caught brace.
        return {6.50, 779.00, -142.70};
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

inline void approach_relative_horizontal_velocity(JPH::Vec3 &world_velocity,
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

[[nodiscard]] inline float smoothstep(const float edge0, const float edge1, const float value) noexcept {
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

// Cubic ease over [0, 1] that leaves at `entry_slope` (in units of the mean
// rate) and arrives at rest: a Hermite segment, monotonic for slopes in
// [0, 3], so a body entering it at speed carries that speed in.
[[nodiscard]] inline float ease_to_rest(const float value, const float entry_slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float slope = std::clamp(entry_slope, 0.0F, 3.0F);
    return slope * t * (1.0F - t) * (1.0F - t) + t * t * (3.0F - 2.0F * t);
}

// A vault should meet the run on both ends. Equal Hermite end slopes preserve
// the incoming velocity along the landing path, while the middle absorbs the
// remaining distance. Slopes in [0, 3] keep the path monotone.
[[nodiscard]] inline float ease_with_carried_speed(const float value, const float slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float s = std::clamp(slope, 0.0F, 3.0F);
    return t * t * (3.0F - 2.0F * t) + s * t * (1.0F - t) * (1.0F - 2.0F * t);
}

[[nodiscard]] inline scraperx::sim::Vector3 to_vector3(const JPH::RVec3 value) noexcept {
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
[[nodiscard]] inline bool box_is_hold(const LeafBox &box, std::uint32_t &long_axis) noexcept {
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

[[nodiscard]] inline bool leaf_box(const JPH::Body &body, const JPH::SubShapeID &sub_shape,
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


class Simulation::PhysicsWorld final {
public:
    explicit PhysicsWorld(const InitialSpawn initial_spawn);

    ~PhysicsWorld();

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
        double sling_draw = 0.0;
        double sling_yaw = 0.0;
        double sling_elevation = 1.4311699866353502;
        bool sling_action = false;
        bool sling_drop = false;
    };

    void step(const StepCommands &commands,
              const float delta_seconds,
              const double next_time_seconds) noexcept;

    [[nodiscard]] const Snapshot &state() const noexcept {
        return state_;
    }

    [[nodiscard]] bool traversal_committed() const noexcept {
        return traversal_state_ != TraversalState::None;
    }

    [[nodiscard]] const scraperx::sim::kit::Kit &kit() const noexcept {
        return *kit_;
    }

    [[nodiscard]] const scraperx::sim::Slingshot *slingshot() const noexcept {
        return slingshot_.get();
    }

    [[nodiscard]] scraperx::sim::Vector3 rider_specific_acceleration() const noexcept {
        return {rider_specific_acceleration_.GetX(), rider_specific_acceleration_.GetY(),
                rider_specific_acceleration_.GetZ()};
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

    [[nodiscard]] const scraperx::sim::bands::Stack &stack() const noexcept {
        return stack_;
    }

private:
    static void remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id);

    static JPH::Body *add_shape_body(JPH::BodyInterface &bodies,
                                     const JPH::Shape *shape,
                                     const JPH::RVec3 position,
                                     const JPH::Quat rotation,
                                     const JPH::EMotionType motion_type,
                                     const JPH::ObjectLayer layer,
                                     const float friction,
                                     const std::uint64_t entity_id,
                                     const float mass_kg);

    static JPH::BodyID add_box(JPH::BodyInterface &bodies,
                               const JPH::Vec3 half_extent,
                               const JPH::RVec3 position,
                               const JPH::EMotionType motion_type,
                               const JPH::ObjectLayer layer,
                               const float friction,
                               const std::uint64_t entity_id,
                               const JPH::Quat rotation = JPH::Quat::sIdentity(),
                               const float mass_kg = 0.0F);

    [[nodiscard]] static JPH::Vec3 normalized_horizontal(const double x, const double z) noexcept;

    [[nodiscard]] JPH::BodyID body_id_for_entity(const std::uint64_t entity_id) const noexcept;

    // Everything the player can see is something the player can hit. The
    // table is generated from the Godot builders (godot/presentation/
    // solid_export.gd), so the world these tests walk is the world that is
    // drawn. Built last, so a drawn mirror of a body already owned here is
    // recognised by its bounds and skipped rather than doubled -- a second
    // coincident body would split ledge probes and support identity.
    void build_world_solids(JPH::BodyInterface &bodies);

    [[nodiscard]] bool mirrors_owned_body(const JPH::AABox &bounds) const;

    // The stack: the tower's lower section, as real static collision. A
    // perimeter deck ring per level round an open 34 m shaft, and corner and
    // mid-span columns carrying each deck. There is no stair and no ramp: a
    // level is gained by a machine or a climb.
    void build_stack(JPH::BodyInterface &bodies);

    // track_for_teardown=false hands ownership entirely to the caller (the
    // carry, created and removed at runtime): Jolt's ConstraintManager::Remove
    // asserts on an already-
    // invalidated constraint index, so a constraint that might be removed
    // before the destructor runs must never also sit in machine_constraints_,
    // which is unconditionally removed there once. Exactly one owner removes
    // it, on every path.
    [[nodiscard]] JPH::TwoBodyConstraint *create_constraint(
        const JPH::TwoBodyConstraintSettings &settings,
        const JPH::BodyID first,
        const JPH::BodyID second,
        const bool track_for_teardown = true);

    [[nodiscard]] JPH::Vec3 current_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept;

    void update_support_motion(JPH::BodyInterface &bodies,
                               const float delta_seconds,
                               const double next_time_seconds) noexcept;

    // ---- geometry probes -------------------------------------------------

    [[nodiscard]] bool cast_ray(const JPH::RVec3 origin,
                                const JPH::Vec3 direction,
                                JPH::RayCastResult &hit) const noexcept;

    // A ray for a wall. With see_past_holds, members a hand could close
    // round (rungs, pipes, bars) are seen through, the way a climber on a
    // ladder sees the wall behind it.
    [[nodiscard]] bool cast_wall(const JPH::RVec3 origin, const JPH::Vec3 direction,
                                 const bool see_past_holds, JPH::RayCastResult &hit,
                                 JPH::RVec3 &point) const noexcept;

    [[nodiscard]] bool is_hold(const JPH::BodyID body_id,
                               const JPH::SubShapeID &sub_shape_id) const noexcept;

    [[nodiscard]] JPH::Vec3 surface_normal(const JPH::BodyID body_id,
                                           const JPH::SubShapeID &sub_shape_id,
                                           const JPH::RVec3 point) const noexcept;

    // Firm footing, for a checkpoint (kCheckpointFootingSlack): a ray straight
    // down from the body's centre meets a walkable surface within reach. The
    // carried body is not footing, whatever it hangs over.
    [[nodiscard]] bool footing_is_firm(const JPH::BodyInterface &bodies) const noexcept;

    // The capsule the body has now: standing, or crouched.
    [[nodiscard]] const JPH::Shape *active_player_shape() const noexcept;

    [[nodiscard]] bool capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept;

    [[nodiscard]] bool shape_pose_is_clear(const JPH::Shape *shape,
                                           const JPH::RVec3 centre) const noexcept;

    // Finds a ledge in front of `origin`. Every returned field comes from a real
    // cast against the authoritative world; a failed reach, a missing top
    // surface, a too-steep top, an out-of-band rise, an unsupported landing, or
    // an obstructed landing pose all return an invalid probe.
    [[nodiscard]] LedgeProbe probe_ledge(const JPH::RVec3 origin,
                                         const JPH::Vec3 facing,
                                         const float minimum_rise,
                                         const float maximum_rise,
                                         const bool require_supported_landing,
                                         const bool see_past_holds = false) const noexcept;

    // Finds the far-side landing that makes an obstacle vaultable rather than
    // mantleable. Without a real, clear landing beyond the obstacle there is no
    // vault.
    [[nodiscard]] bool probe_vault_landing(const LedgeProbe &obstacle,
                                           const JPH::Vec3 facing,
                                           const float feet_y,
                                           JPH::RVec3 &landing_centre,
                                           JPH::BodyID &landing_body) const noexcept;

    // ---- support-frame helpers ------------------------------------------

    [[nodiscard]] JPH::Vec3 to_support_local(const JPH::BodyInterface &bodies,
                                             const JPH::BodyID body_id,
                                             const JPH::RVec3 world_point) const noexcept;

    [[nodiscard]] JPH::RVec3 from_support_local(const JPH::BodyInterface &bodies,
                                                const JPH::BodyID body_id,
                                                const JPH::Vec3 local_point) const noexcept;

    // ---- traversal state machine ----------------------------------------

    // ---- AS-003 carry ---------------------------------------------------------
    // Where the carried body's handle is held, relative to the player's
    // centre: forward along a horizontal bearing, a little above the centre.
    [[nodiscard]] static JPH::Vec3 carry_hand_offset(const JPH::Vec3 bearing) noexcept;

    // The bearing the hands hold the load on this tick: the load's present
    // bearing from the body, turned toward the facing by at most
    // kCarryTurnRadiansPerSecond. Derived from poses, so it adds no state.
    [[nodiscard]] JPH::Vec3 carry_hand_bearing(const JPH::BodyInterface &bodies,
                                               const float delta_seconds) const noexcept;

    // Where the carried kit body's handle is, in its own frame: where its
    // build put it.
    [[nodiscard]] JPH::Vec3 carry_handle(const std::uint64_t entity) const noexcept;

    // What a pick-up would take now: the nearest carryable within reach of the
    // body's centre, at rest, not behind it -- and only for a grounded body
    // outside a traversal with its hands free.
    JPH::BodyID carry_candidate(const JPH::BodyInterface &bodies,
                                std::uint64_t &entity) const noexcept;

    // A point constraint between the hands and the body's handle. Rotation is
    // left free, so a block hangs and swings from it.
    void attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept;

    // Removes the constraint and nothing else: the body falls and rests.
    void release_carry() noexcept;

    void update_carry(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    // Checkpoint reconciliation: after the bodies' poses are restored, make
    // the carry match what was committed.
    void restore_carry_topology(const std::uint64_t committed_entity) noexcept;

    // ---- AS-006 rigging ------------------------------------------------------
    // Where the hands are this tick: the carry point, whether or not anything
    // is held.
    [[nodiscard]] JPH::RVec3 hand_position(const JPH::BodyInterface &bodies) const noexcept;

    // What the rig command would do now, from poses and the carry alone.
    void find_rig_action(const JPH::BodyInterface &bodies) noexcept;

    // Hook: the shackle leaves the hands and the rope's end goes onto the
    // anchor. Unhook: the end comes off the anchor onto its shackle, in the
    // hands.
    void update_rig(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    // Crouch and stand (GDD 7.2, Governing Law 4). The body swaps capsules
    // with its soles fixed: the centre moves by kCrouchDrop, nothing under
    // the feet does. It crouches only from the ground and never inside a
    // traversal (every traversal pose is a standing pose), and it stands
    // only where the full standing capsule is clear, so a gap entered
    // crouched keeps the body crouched until it is out of it. A Jump or a
    // traversal request asks to stand first; where the body cannot, they
    // are refused further down this tick.
    void update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    [[nodiscard]] bool try_stand(JPH::BodyInterface &bodies) noexcept;

    void apply_traversal_commands(JPH::BodyInterface &bodies,
                                  const StepCommands &commands) noexcept;

    // The vault the takeoff could have been: probed from the takeoff floor at
    // the body's current plan position, with the same rise band and the same
    // far-side landing test as a vault started on the ground.
    [[nodiscard]] bool try_begin_jump_vault(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] bool apply_locomotion(JPH::BodyInterface &bodies,
                                        const StepCommands &commands,
                                        const float delta_seconds) noexcept;

    // The capsule swept from `from` by `displacement`: true on a hit, with
    // the hit fraction and the surface normal (pointing out of what was hit).
    [[nodiscard]] bool cast_capsule(const JPH::RVec3 from, const JPH::Vec3 displacement,
                                    float &fraction, JPH::Vec3 &normal) const;

    // Stepping up. Blocked at the feet, clear with the body raised by the
    // maximum step, and walkable support found by sweeping back down: then
    // the body is lifted onto the step, as Jolt's own character controller
    // walks stairs. Every stage is a sweep against real geometry, so nothing
    // is passed through and nothing is stood on that is not there.
    void try_step_up(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                     const float delta_seconds);

    // Real quadratic drag opposing the full velocity vector, not a clamp: it
    // can only ever pull speed toward the terminal value, never accelerate
    // the player upward past what deceleration implies (Governing Law 7 --
    // no powered ascent). Applied on top of ordinary air control, so existing
    // horizontal steering doubles as the "redirection" the same law permits.
    void apply_parachute_drag(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    void try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    [[nodiscard]] bool try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept;

    // Real support under the capsule's axis at `centre`: walkable ground no
    // deeper than the step's own lift plus the landing tolerance.
    [[nodiscard]] bool step_is_supported(const JPH::RVec3 centre) const noexcept;

    // Where a standing mantle's step-in ends (see kMantleApproachStandoff).
    // Returns `origin` itself when no step is possible.
    [[nodiscard]] JPH::RVec3 mantle_approach_end(const JPH::BodyInterface &bodies,
                                                 const LedgeProbe &probe,
                                                 const JPH::RVec3 origin) const noexcept;

    void begin_mantle(JPH::BodyInterface &bodies,
                      const LedgeProbe &probe,
                      const JPH::RVec3 origin) noexcept;

    void begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept;

    void begin_vault(JPH::BodyInterface &bodies,
                     const LedgeProbe &probe,
                     const JPH::RVec3 landing_centre,
                     const JPH::BodyID landing_body,
                     const JPH::RVec3 origin) noexcept;

    [[nodiscard]] JPH::RVec3 traversal_point(const JPH::BodyInterface &bodies,
                                             const float progress) const noexcept;

    void drive_traversal(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    void resolve_traversal_outcome(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] JPH::Vec3 traversal_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept;

    void complete_traversal(JPH::BodyInterface &bodies) noexcept;

    void abort_traversal(JPH::BodyInterface &bodies) noexcept;

    void release_with_speed(JPH::BodyInterface &bodies, const JPH::Vec3 support_velocity) noexcept;

    void release_hang(JPH::BodyInterface &bodies) noexcept;

    // Let go of the bar and keep the speed the swing already gave the body.
    void release_pitman_hang(JPH::BodyInterface &bodies) noexcept;

    // Hands on the bar, where the body already is. The constraint's two
    // points start on the same spot, so the grab does not yank the body.
    // Gravity is off and load_hold puts the weight on the bar once: the
    // deck is not also asked to carry it. The body's mass stays in the
    // constraint, so the bar feels who is hanging on it.
    void begin_pitman_hang(JPH::BodyInterface &bodies, const Grip &grip) noexcept;

    // A pull along the bar's swing. Full when it goes with the bar, a
    // fraction when it fights the bar, so holding one direction still
    // feeds the swing instead of stalling it.
    void pump_pitman(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    // ---- Step 2 movement (MECHANISM_ASCENT_PLAN.md §8) --------------------

    // A hang or climb on a dynamic body hangs the climber's weight on it at
    // the hold: the structure feels who is on it. Static and kinematic
    // bodies take no force.
    void load_hold(JPH::BodyInterface &bodies) noexcept;

    // Horizontal right of the structure a hang or climb faces.
    [[nodiscard]] JPH::Vec3 traversal_right() const noexcept;

    // The hold nearest `aim` that a hand in front of `centre` can close
    // round: a box whose two thinner dimensions are at most kGripMaxSection
    // and whose length is at least kGripMinLength, on a static body or one of
    // at least kGripMinBodyMassKg, searched in a kGripSearchHalf box round
    // `aim` squared to `facing`. The point is on the member's axis.
    [[nodiscard]] Grip find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                                 const JPH::Vec3 facing) const noexcept;

    // Where a hand in front of `centre` aims: `height` over the centre,
    // kClimbHandReach toward the structure, `across` to its right.
    [[nodiscard]] JPH::RVec3 hand_aim(const JPH::RVec3 centre, const float height,
                                      const float across) const noexcept;

    [[nodiscard]] JPH::RVec3 hand_point(const JPH::BodyInterface &bodies,
                                        const std::uint32_t hand) const noexcept;

    void set_hand(const JPH::BodyInterface &bodies, const std::uint32_t hand,
                  const Grip &grip) noexcept;

    // Both hands onto the holds nearest their rest aims, left a little lower
    // than right; a hand that finds none takes the hold the climb began on.
    void take_hand_holds(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                         const Grip &fallback) noexcept;

    // The swept move of a climbing or hanging body, slimmed so the holds it
    // already touches do not stop it.
    [[nodiscard]] bool climb_move_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept;

    // Take hold of `grip` where the body is, facing it.
    void begin_climb(JPH::BodyInterface &bodies, const Grip &grip) noexcept;

    // From the ground, a hold faced at hand height.
    [[nodiscard]] Grip grip_in_front(const JPH::BodyInterface &bodies) const noexcept;

    // Climbing, the stick toward the structure climbs, away climbs down,
    // sideways moves across -- each only onto holds the hands can reach.
    void update_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    // Hand over hand: the hand furthest behind the way the body moves
    // reaches ahead to the next hold, one hand every kClimbRegripTicks.
    void move_hands(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                    const double up_input, const double side_input) noexcept;

    // Soles within kClimbFootReach of walkable ground under a body at `centre`.
    [[nodiscard]] bool feet_on_ground(const JPH::RVec3 centre) const noexcept;

    // Climbing over a ledge in reach: the mantle takes over from where the
    // body is. The ledge is looked for where the player looks, then along
    // the structure's normal: a climber on a pipe faced from an angle looks
    // round at the deck it means to climb onto.
    [[nodiscard]] bool try_top_out(JPH::BodyInterface &bodies) noexcept;

    void step_off_climb(JPH::BodyInterface &bodies) noexcept;

    void let_go_climb(JPH::BodyInterface &bodies) noexcept;

    // Springing back off the structure, away from it and up.
    void jump_off_climb(JPH::BodyInterface &bodies) noexcept;

    // The lip of a ledge near `near` (a point on or near its top edge),
    // faced along `normal`: a ray across just under the top onto its face,
    // then a ray down onto the top just inside the face.
    [[nodiscard]] Lip probe_lip(const JPH::RVec3 near, const JPH::Vec3 normal) const noexcept;

    // Hanging, pulling back from the wall pumps a swing. Letting go keeps it.
    // On the pitman the bar itself is the swing: a finite pull at the hands.
    void update_swing(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    // An edge in front of the body that a hand can catch: a beam, a flange,
    // a deck lip. It does not have to be a floor you could stand on.
    [[nodiscard]] Lip lip_in_front(const JPH::BodyInterface &bodies) const noexcept;

    void begin_hang_on_lip(JPH::BodyInterface &bodies, const Lip &lip) noexcept;

    // Action in the air: a bar, or an edge, even while still rising.
    [[nodiscard]] bool try_air_catch(JPH::BodyInterface &bodies) noexcept;

    // Hanging, the stick sideways moves along the ledge while its lip goes
    // on under the hands. Something standing on the ledge back from its lip
    // does not stop a shimmy; only climbing up needs the landing clear.
    void update_shimmy(JPH::BodyInterface &bodies, const StepCommands &commands,
                       const float delta_seconds) noexcept;

    // An edge behind a standing body with a drop beyond it: the floor ends
    // within kEdgeSearchReach behind the body and nothing is under the space
    // beyond for kEdgeDropMinimum, and a hanging body fits below its lip.
    [[nodiscard]] Lip probe_edge_drop(const JPH::BodyInterface &bodies) const noexcept;

    // Drop, standing with an edge behind: lower over it into a hang.
    [[nodiscard]] bool try_begin_lowering(JPH::BodyInterface &bodies) noexcept;

    // Lowered: hanging from the lip, as if the hang had been caught there.
    void finish_lowering() noexcept;

    // The support under a walking body, if it is a beam: a box narrower
    // than kBalanceMaxWidth and at least kBalanceMinLength long, lying level.
    struct Beam final {
        bool valid = false;
        JPH::Vec3 along{JPH::Vec3::sZero()};
        JPH::Vec3 across{JPH::Vec3::sZero()};
        float offset = 0.0F;
    };

    [[nodiscard]] Beam beam_underfoot(const JPH::BodyInterface &bodies) const noexcept;

    void clear_traversal() noexcept;

    void update_affordance(const JPH::BodyInterface &bodies) noexcept;

    // Machine half of a checkpoint (TDD 14.1: "machine/control state"): the
    // carry and every kit body. Kinematic bodies are pure functions of the
    // tick counter and need no restoring.
    void commit_machine_checkpoint() noexcept;

    // WO-008 automatic commit (GDD 9.1): every tick the player is grounded on
    // firm footing (kCheckpointFootingNormalY) and not mid-traversal, so the
    // checkpoint is always "wherever the player was last standing." No dwell
    // timer, no player-facing save action.
    void commit_checkpoint(const JPH::BodyInterface &bodies) noexcept;

    // WO-008 death restore (Governing Laws 9, 21): the one sanctioned
    // exception to "no hidden teleportation," explicitly named by Law 21
    // itself. Restores player and every captured machine body, then clears
    // this tick's now-stale contact/traversal-adjacent state so the next
    // tick re-establishes ground truth from a fresh contact pass rather than
    // publishing a snapshot that mixes a teleported position with a contact
    // sample that referred to the pre-restore position.
    void restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept;

    void read_machine_state() noexcept;

    void read_state() noexcept;

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

    std::vector<JPH::BodyID> machine_bodies_;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> machine_constraints_;
    // AS-003: the carry, created/removed at runtime (track_for_teardown=false).
    JPH::Ref<JPH::PointConstraint> carry_constraint_;
    JPH::BodyID carried_id_;
    std::uint64_t carried_entity_ = 0;
    // Hands on the pitman's bar. Not tracked for teardown: the hang removes
    // it, and so does the destructor.
    JPH::Ref<JPH::PointConstraint> pitman_grip_;
    JPH::RVec3 pitman_pivot_{JPH::RVec3::sZero()};
    std::uint64_t carry_target_entity_ = 0;
    float grip_over_seconds_ = 0.0F;
    // AS-006: the mechanism kit and the bands built from it.
    std::unique_ptr<scraperx::sim::kit::Kit> kit_;
    std::unique_ptr<scraperx::sim::Slingshot> slingshot_;
    scraperx::sim::bands::CounterweightWell well_{};
    scraperx::sim::bands::WetIsolation wet_{};
    scraperx::sim::bands::PlateShop shop_{};
    scraperx::sim::bands::FacadeCrane crane_{};
    scraperx::sim::bands::Stack stack_{};
    mutable std::vector<scraperx::sim::kit::Kit::CarryCandidate> kit_carryables_;
    std::uint8_t rig_action_ = 0;
    std::uint64_t rig_target_entity_ = 0;
    scraperx::sim::kit::AnchorIndex rig_anchor_;
    scraperx::sim::kit::RopeIndex rig_rope_;
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
    float traversal_vault_horizontal_slope_ = 0.0F;
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
    float swing_angle_ = 0.0F;
    float swing_rate_ = 0.0F;
    JPH::Vec3 pre_contact_planar_{JPH::Vec3::sZero()};

    struct MachineCheckpoint final {
        // What was being carried, and every kit body, rope end, parted rope
        // and catch.
        std::uint64_t carrying_entity = 0;
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
    JPH::Vec3 rider_specific_acceleration_ = JPH::Vec3::sZero();

    Snapshot state_{};
};

} // namespace scraperx::sim
