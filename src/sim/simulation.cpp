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
        // On deck 2's south band, north of S1's gangway.
        return {10.0, 23.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck4South:
        // On deck 4's south band, west of C1's davit.
        return {11.0, 45.2, -125.5};
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
        // bodies (world_solids.inc) on top of the tower's and the bands'.
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
        // the tower approach.
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

        // The proving ground's movement fixtures (WO-002, WO-003): only in a
        // world started at one of its spawns, never in the game's.
        if (scraperx::sim::is_proving_spawn(initial_spawn)) {
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
        }

        build_stack(bodies);
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

        // AS-006: the mechanism ascent's bands, built after the player.
        kit_ = std::make_unique<scraperx::sim::kit::Kit>(physics_system_, object_layers::kStatic,
                                                        object_layers::kMoving);
        scraperx::sim::bands::build_counterweight_well(*kit_, well_);
        scraperx::sim::bands::build_wet_isolation(*kit_, wet_);
        scraperx::sim::bands::build_plate_shop(*kit_, shop_);
        scraperx::sim::bands::build_facade_crane(*kit_, crane_);
        // Band 0, the Stack: the ascent from grade.
        scraperx::sim::bands::build_stack(*kit_, stack_);

        physics_system_.OptimizeBroadPhase();

        checkpoint_position_ = JPH::RVec3(0.0, 0.9, 0.0);
        commit_machine_checkpoint();

        read_state();
    }

    ~PhysicsWorld() {
        physics_system_.SetContactListener(nullptr);
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
    };

    void step(const StepCommands &commands,
              const float delta_seconds,
              const double next_time_seconds) noexcept {
        // WO-008: captured before anything this tick can change grounded_, so
        // it means exactly "was the player standing on something one tick ago."
        const bool was_grounded_before_tick = grounded_;

        auto &bodies = physics_system_.GetBodyInterface();
        update_support_motion(bodies, delta_seconds, next_time_seconds);

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

    [[nodiscard]] const scraperx::sim::bands::Stack &stack() const noexcept {
        return stack_;
    }

private:
    static void remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
        // A proving-ground fixture is never built in the game's world.
        if (body_id.IsInvalid()) {
            return;
        }
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
        default:
            return {};
        }
    }

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

    // The stack: the tower's lower section, as real static collision. A
    // perimeter deck ring per level round an open 34 m shaft, and corner and
    // mid-span columns carrying each deck. There is no stair and no ramp: a
    // level is gained by a machine or a climb.
    void build_stack(JPH::BodyInterface &bodies) {
        const auto frame = [&](const JPH::Vec3 half_extent, const JPH::RVec3 position) {
            machine_bodies_.push_back(add_box(bodies, half_extent, position,
                                              JPH::EMotionType::Static, object_layers::kStatic,
                                              0.85F, Simulation::kTowerEntityId));
        };

        const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.5F;
        const float inner_half = kStackHalfExtent - kStackDeckBandDepth;
        for (int level = 1; level <= kStackLevelCount; ++level) {
            const float slab_y =
                static_cast<float>(level) * kStackLevelHeight - kStackDeckHalfThickness;
            // Deck ring: two full-width bands and two inner bands.
            for (const float side : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackHalfExtent, kStackDeckHalfThickness,
                                kStackDeckBandDepth * 0.5F),
                      JPH::RVec3(kStackCenterX, slab_y, kStackCenterZ + side * band_center));
            }
            for (const float side : {1.0F, -1.0F}) {
                frame(JPH::Vec3(kStackDeckBandDepth * 0.5F, kStackDeckHalfThickness, inner_half),
                      JPH::RVec3(kStackCenterX + side * band_center, slab_y, kStackCenterZ));
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
    }

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

    // Where the carried kit body's handle is, in its own frame: where its
    // build put it.
    [[nodiscard]] JPH::Vec3 carry_handle(const std::uint64_t entity) const noexcept {
        return kit_->carry_handle(entity);
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

    // Checkpoint reconciliation: after the bodies' poses are restored, make
    // the carry match what was committed.
    void restore_carry_topology(const std::uint64_t committed_entity) noexcept {
        if (committed_entity == carried_entity_) {
            return;
        }
        release_carry();
        if (scraperx::sim::kit::is_kit_entity(committed_entity)) {
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
        const JPH::Vec3 unit_along = along.Normalized();
        const JPH::Vec3 across(-unit_along.GetZ(), 0.0F, unit_along.GetX());
        const float offset = JPH::Vec3(centre - box.centre).Dot(across);
        // A beam is a beam over a fall. Ground within a step beside it (the
        // yard's kerb, a sill) is walked on and off like any floor: held on
        // its line, a body could not step down off a kerb.
        const JPH::RVec3 underfoot =
            centre + JPH::Vec3(0.0F, -(half_height + kCheckpointFootingSlack) * hit.mFraction, 0.0F) -
            across * offset;
        for (const float side : {-1.0F, 1.0F}) {
            const JPH::RVec3 beside = underfoot +
                                      across * (side * (box.half[short_axis] + kBalanceSideProbe)) +
                                      JPH::Vec3(0.0F, kBalanceSideProbeLift, 0.0F);
            JPH::RayCastResult below;
            const JPH::Vec3 down(0.0F, -(kStepMaximumHeight + kBalanceSideProbeLift), 0.0F);
            if (cast_ray(beside, down, below) &&
                surface_normal(below.mBodyID, below.mSubShapeID2, beside + down * below.mFraction)
                        .GetY() >= kSupportNormalThreshold) {
                return beam;
            }
        }
        beam.valid = true;
        beam.along = unit_along;
        beam.across = across;
        beam.offset = offset;
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

    // Machine half of a checkpoint (TDD 14.1: "machine/control state"): the
    // carry and every kit body. Kinematic bodies are pure functions of the
    // tick counter and need no restoring.
    void commit_machine_checkpoint() noexcept {
        checkpoint_.carrying_entity = carried_entity_;
        kit_->capture(checkpoint_.kit);
    }

    // WO-008 automatic commit (GDD 9.1): every tick the player is grounded on
    // firm footing (kCheckpointFootingNormalY) and not mid-traversal, so the
    // checkpoint is always "wherever the player was last standing." No dwell
    // timer, no player-facing save action.
    void commit_checkpoint(const JPH::BodyInterface &bodies) noexcept {
        checkpoint_position_ = bodies.GetPosition(player_id_);
        checkpoint_crouched_ = crouched_;
        commit_machine_checkpoint();
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
        // been restored onto (observed at a deck's edge).
        if (carry_constraint_ != nullptr) {
            bodies.SetLinearAndAngularVelocity(carried_id_, JPH::Vec3::sZero(),
                                               JPH::Vec3::sZero());
        }

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

    void read_machine_state() noexcept {
        state_.carrying_entity_id = carried_entity_;
        state_.carry_target_entity_id = carry_target_entity_;

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

        read_machine_state();

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

    std::vector<JPH::BodyID> machine_bodies_;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> machine_constraints_;
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

StackState Simulation::stack_state() const noexcept {
    const kit::Kit &kit = physics_world_->kit();
    const scraperx::sim::bands::Stack &stack = physics_world_->stack();
    StackState out;
    out.s1_cage_travel = kit.guide_travel(stack.s1_cage_guide);
    out.s1_cage_peak_speed = kit.guide_peak_speed(stack.s1_cage_guide);
    out.s1_bucket_travel = kit.guide_travel(stack.s1_bucket_guide);
    out.s1_bucket_water_kg = kit.bin_contents(stack.s1_bin);
    out.s1_tank_water_kg = kit.pool_water(stack.s1_tank);
    out.s1_valve_angle = kit.lever_angle(stack.s1_lever);
    out.s1_catch_latched = kit.catch_latched(stack.s1_catch);
    out.s2_stair_angle = kit.lever_angle(stack.s2_hinge);
    out.s2_stair_rate = kit.lever_rate(stack.s2_hinge);
    out.s2_catch_lever_angle = kit.lever_angle(stack.s2_catch_lever);
    out.s2_catch_latched = kit.catch_latched(stack.s2_catch);
    out.s2_on_pad = kit.lever_on_pad(stack.s2_hinge);
    return out;
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

    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
    parachute_toggle_requested_ = false;
    pick_up_requested_ = false;
    set_down_requested_ = false;
    rig_requested_ = false;
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
