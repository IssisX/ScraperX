// The Matron: a sealed locker, a build-up, and a hunt by sound on the 154 m
// deck (see matron.hpp).
#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"
#include "sim/mechanism_kit.hpp"
#include <cstdio>
#include <cstdlib>

#include "sim/zipline.hpp"
#include "sim/shared_survival.hpp"
#include "sim/bay.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace scraperx::sim {
namespace {
using namespace JPH;
using grip::ClimbBodyFilter;
using grip::hand_rests_on;

constexpr float kPi = 3.14159265F;
// How far a sound of full loudness carries.
constexpr float kFullHearingM = 55.0F;
constexpr float kTurnRate = 7.0F;
// way_clear sweeps her width this much short of her body's: a wall, a rail or
// a post her side only brushes she slides past; one nearer her middle line
// stops her body.
constexpr float kWaySkinM = 0.1F;
// Held to a ramp she is on only until the stall watchdog has had her stalled this long on it (its clamber,
// sidestep and reroute all tried): a blocked ramp is given up to her other ways.
// Value lives as Matron::kRampStallS in matron.hpp (see there).

float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, 2.0F * kPi);
}

float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}

// The closest point to (x, z) on segment a-b, and how far along it (0..1).
void project(const float ax, const float az, const float bx, const float bz, const float x, const float z,
             float &px, float &pz, float &t) {
    const float dx = bx - ax;
    const float dz = bz - az;
    const float length2 = dx * dx + dz * dz;
    t = length2 > 0.0F ? std::clamp(((x - ax) * dx + (z - az) * dz) / length2, 0.0F, 1.0F) : 0.0F;
    px = ax + dx * t;
    pz = az + dz * t;
}
} // namespace

Matron::Matron(PhysicsSystem &physics, const std::uint32_t static_layer, const std::uint32_t moving_layer,
               const std::uint32_t steel_layer, const BodyID player)
    : physics_(physics), moving_layer_(moving_layer), steel_layer_(steel_layer), player_(player) {
    shape_ = new CapsuleShape(kHalfHeight, kRadius);
    hips_ = new SphereShape(kRadius - kWaySkinM);
    // Her deck: down the middle of each band, the catwalk across, and in off
    // the north band round the ladder's west rail to its foot; her post in the
    // mouth of Stage A's cage, facing out over the band; the cage's eye on its
    // west face, reached from inside it facing west.
    Level deck{kDeckTop, {}, {}, {}, {}, 0, {-11.35F, kCagesCenterZ, -0.5F * kPi}};
    deck.nodes = {{-kBandX, kBandNorthZ},      {0.0F, kBandNorthZ},          {kBandX, kBandNorthZ},
                  {kBandX, kMidZ},             {kBandX, kBandSouthZ},        {0.0F, kBandSouthZ},
                  {-kBandX, kBandSouthZ},      {-kBandX, kMidZ},             {kLadderSideX, kBandNorthZ},
                  {kLadderSideX, kLadderFootZ}, {kLadderX, kLadderFootZ}, {Bay::kWestX - 0.5F, kBandNorthZ}};
    const std::vector<Edge> band_loop = {{0, 1, Link::Floor}, {1, 2, Link::Floor}, {2, 3, Link::Floor},
                                         {3, 4, Link::Floor}, {4, 5, Link::Floor}, {5, 6, Link::Floor},
                                         {6, 7, Link::Floor}, {7, 0, Link::Floor}};
    // The north band's line crosses the bay (bay.hpp) on its leaf.
    deck.edges = {{0, 11, Link::Floor}, {11, 1, Link::Leaf}, {1, 8, Link::Floor}, {8, 2, Link::Floor}, {2, 3, Link::Floor},
                  {3, 4, Link::Floor}, {4, 5, Link::Floor}, {5, 6, Link::Floor}, {6, 7, Link::Floor},
                  {7, 0, Link::Floor}, {7, 3, Link::Catwalk}, {8, 9, Link::Floor}, {9, 10, Link::Floor}};
    deck.patrols = {{0, 1, 2, 3, 4, 5, 6, 7}};
    deck.posts = {{kPostX, kPostZ, 0.0F}};
    // The 176 ring: down the middle of its band, and the two cages at its
    // north edge, a step apart, a gap (1.09 m) off the ring. The gap is open
    // air from the west half of A's mouth (the cascade striker's post stands
    // on the ring's edge before its middle; B's gantry and its gear hang over
    // B's). Its post is by B's cage, its way up: on the ring facing the cage,
    // or in the cage's mouth; B's eye is on its east face.
    Level ring{kRingFloor, {}, {}, {}, {}, 1, {-6.35F, kCagesCenterZ, 0.5F * kPi}};
    ring.nodes = {{-kRingX, kRingNorthZ},       {kLeapX, kRingNorthZ},        {kCageBCenterX, kRingNorthZ},
                  {kRingX, kRingNorthZ},        {kRingX, kMidZ},              {kRingX, kRingSouthZ},
                  {-kRingX, kRingSouthZ},       {-kRingX, kMidZ},             {kCageACenterX, kCagesCenterZ},
                  {kCageBCenterX, kCagesCenterZ}, {kLeapX + 0.5F, kCagesCenterZ + 0.9F}};
    ring.edges = {{0, 1, Link::Floor}, {1, 2, Link::Floor}, {2, 3, Link::Floor},  {3, 4, Link::Floor},
                  {4, 5, Link::Floor}, {5, 6, Link::Floor}, {6, 7, Link::Floor},  {7, 0, Link::Floor},
                  {8, 9, Link::Floor}, {8, 10, Link::Floor}, {10, 1, Link::Gap}};
    ring.patrols = {{0, 1, 2, 3, 4, 5, 6, 7}, {8, 9}};
    ring.posts = {{kCageBCenterX, kRingNorthZ - 1.2F, kPi}, {kCageBCenterX, kCagesCenterZ + 0.9F, 0.0F}};
    levels_ = {deck, ring};
    // The rings above: each down the middle of its band (the bands' inner
    // edges close in 0.91 m a ring going up), with no way up of hers known.
    for (int k = 1; k <= kUpperRings; ++k) {
        const float half = kRingX - kRingStepIn * static_cast<float>(k);
        Level upper{kRingFloor + kRingPitch * static_cast<float>(k), {}, {}, {}, {}, -1, {0.0F, 0.0F, 0.0F}};
        upper.nodes = {{-half, kMidZ + half}, {0.0F, kMidZ + half}, {half, kMidZ + half}, {half, kMidZ},
                       {half, kMidZ - half},  {0.0F, kMidZ - half}, {-half, kMidZ - half}, {-half, kMidZ}};
        upper.edges = {{0, 1, Link::Floor}, {1, 2, Link::Floor}, {2, 3, Link::Floor}, {3, 4, Link::Floor},
                       {4, 5, Link::Floor}, {5, 6, Link::Floor}, {6, 7, Link::Floor}, {7, 0, Link::Floor}};
        upper.patrols = {{0, 1, 2, 3, 4, 5, 6, 7}};
        levels_.push_back(upper);
    }
    // The stack's decks under hers, laid out as hers without the catwalk or
    // the ladder: her deck's band loop.
    for (int k = 1; k <= kStackDecks; ++k) {
        Level stack{kStackPitch * static_cast<float>(k), {}, band_loop, deck.patrols, {}, -1, {0.0F, 0.0F, 0.0F}};
        stack.nodes.assign(deck.nodes.begin(), deck.nodes.begin() + 8);
        levels_.push_back(stack);
    }
    // The ladder from her deck up to the 176 ring, climbed from its south
    // side and topped out north over its head; and the riser from 143 up to
    // her deck, climbed from its plate over the shaft and left north down the
    // steps off its bridge, which she knows but does not take hold of.
    // Her ways up are not listed here: each is a rung edge her own scan of
    // the world found (climb_of), read when she needs one.
    // The bay's bracket (bay.hpp): where she holds the leaf and the foot of
    // its rungs, which she takes hold of whatever her stage (her own way out
    // of her own trap), topping out west onto her deck.
    Level bay{Bay::kBracketTop, {{Bay::kBraceX, Bay::kBraceZ}, {Bay::kRungsX, Bay::kRungsZ}},
              {{0, 1, Link::Floor}}, {}, {}, -1, {0.0F, 0.0F, 0.0F}};
    levels_.push_back(bay);
    bay_level_ = static_cast<int>(levels_.size()) - 1;
    patrol_node_ = 2;

    BodyInterface &bodies = physics_.GetBodyInterface();
    // Her body exists from the start but is only in the world once she is out.
    BodyCreationSettings settings(shape_, RVec3(kLockerX, kDeckTop + kStandCentre, kLockerZ), Quat::sIdentity(),
                                  EMotionType::Dynamic, static_cast<ObjectLayer>(moving_layer_));
    settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
    settings.mAllowSleeping = false;
    settings.mFriction = 0.0F;
    settings.mLinearDamping = 0.0F;
    settings.mUserData = kEntity;
    settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = kMassKg;
    Body *body = bodies.CreateBody(settings);
    body_ = body->GetID();

    // The locker: solid from the start, and for good.
    BodyCreationSettings cabinet(new BoxShape(Vec3(kLockerHalfX, kLockerHalfY, kLockerHalfZ)),
                                 RVec3(kLockerX, kDeckTop + kLockerHalfY, kLockerZ), Quat::sIdentity(),
                                 EMotionType::Static, static_cast<ObjectLayer>(static_layer));
    cabinet.mUserData = kLockerEntity;
    locker_body_ = bodies.CreateAndAddBody(cabinet, EActivation::DontActivate);

    // The catwalk: plates across the shaft, each on a bolt to the world.
    const float start = -0.5F * kPlanks * kPlankLength;
    for (int i = 0; i < kPlanks; ++i) {
        Plank &plank = planks_[static_cast<std::size_t>(i)];
        plank.home_x = start + kPlankLength * 0.5F + kPlankLength * static_cast<float>(i);
        BodyCreationSettings plate(new BoxShape(Vec3(0.5F * (kPlankLength - 0.02F), 0.5F * kPlankThick, 0.5F * kPlankWidth)),
                                   RVec3(plank.home_x, kDeckTop - 0.5F * kPlankThick, kMidZ), Quat::sIdentity(),
                                   EMotionType::Dynamic, static_cast<ObjectLayer>(moving_layer_));
        plate.mAllowSleeping = false;
        plate.mFriction = 0.9F;
        plate.mUserData = kPlankEntityBase + static_cast<std::uint64_t>(i);
        plate.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        plate.mMassPropertiesOverride.mMass = kPlankMassKg;
        plank.body = bodies.CreateAndAddBody(plate, EActivation::Activate);
        FixedConstraintSettings bolt;
        bolt.mSpace = EConstraintSpace::WorldSpace;
        bolt.mAutoDetectPoint = true;
        BodyLockWrite lock(physics_.GetBodyLockInterface(), plank.body);
        plank.bolt = static_cast<FixedConstraint *>(bolt.Create(Body::sFixedToWorld, lock.GetBody()));
        physics_.AddConstraint(plank.bolt);
    }
    refresh_view();
}

Matron::~Matron() {
    if (hold_ != nullptr) {
        physics_.RemoveConstraint(hold_);
        hold_ = nullptr;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (Plank &plank : planks_) {
        if (plank.bolt != nullptr) {
            physics_.RemoveConstraint(plank.bolt);
        }
        if (!plank.gone) {
            bodies.RemoveBody(plank.body);
        }
        bodies.DestroyBody(plank.body);
    }
    if (body_in_world_) {
        bodies.RemoveBody(body_);
    }
    bodies.DestroyBody(body_);
    bodies.RemoveBody(locker_body_);
    bodies.DestroyBody(locker_body_);
}

void Matron::spawn_body() {
    if (body_in_world_) {
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    bodies.SetPositionAndRotation(body_, RVec3(kLockerX, kDeckTop + kStandCentre + 0.02F, kSpawnZ), Quat::sIdentity(),
                                  EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(body_, Vec3::sZero(), Vec3::sZero());
    bodies.AddBody(body_, EActivation::Activate);
    body_in_world_ = true;
    fall_vy_ = 0.0F;
    foot_driving_ = false;
    foot_prep_ = false;
    foot_body_ = BodyID();
    foot_force_n_ = 0.0F;
    foot_power_w_ = 0.0F;
    foot_lunge_ = false;
    foot_drive_dv_ = Vec3::sZero();
}

void Matron::place_for_test(const RVec3 feet) {
    spawn_body();
    BodyInterface &bodies = physics_.GetBodyInterface();
    bodies.SetPositionAndRotation(body_, feet + Vec3(0.0F, kStandCentre + 0.02F, 0.0F), Quat::sIdentity(),
                                  EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(body_, Vec3::sZero(), Vec3::sZero());
    fall_vy_ = 0.0F;
    foot_driving_ = false;
    foot_prep_ = false;
    foot_body_ = BodyID();
    foot_force_n_ = 0.0F;
    foot_power_w_ = 0.0F;
    foot_lunge_ = false;
    foot_drive_dv_ = Vec3::sZero();
    state_ = State::Hunting;
    turn_ = Turn::None;
    feet_y_ = float(feet.GetY());
    // Her level: the deck she stands on, or (on a ledge between two) the
    // nearest under her feet.
    float best = -std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        const float floor = levels_[i].floor_y;
        if (floor <= feet_y_ + 0.6F && floor > best) {
            best = floor;
            level_ = static_cast<int>(i);
        }
    }
}

void Matron::hear_for_test(const RVec3 at) {
    take_lead(float(at.GetX()), float(at.GetZ()), float(at.GetY()), 1.0F, false);
    if (state_ == State::Shuffling || state_ == State::Guarding || state_ == State::Searching) {
        state_ = State::Hunting;
    }
}

void Matron::only_way_for_test(const std::size_t index) {
    for (std::size_t i = 0; i < routes_.edge_count(); ++i) {
        if (i != index) {
            routes_.set_status(i, routes::Status::Blocked, routes::Reason::Untested);
        }
    }
    found_climbs_off_ = index < routes_.edge_count();   // one way alone: none of her own either (no way named: her own only)
}

void Matron::let_go_for_test() {
    // Her hands open on the way that holds her, as a hold giving way opens them.
    (void)(climb_way_.let_go() || pipe_way_.let_go() || routecraft_way_.let_go() || journey_way_.let_go());
}

bool Matron::climb_for_test(const routes::Connector which, const bool down) {
    // The scanned way nearest her off her floor (the connector only says
    // pipe or not: every climb is one her scan found).
    int index = -1;
    if (body_in_world_ && which != routes::Connector::CornerPipe) {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        index = nearest_climb(float(at.GetX()), float(at.GetZ()), float(at.GetY()) - kStandCentre, down);
    }
    climb_retry_s_ = 0.0F;
    if (which == routes::Connector::CornerPipe) {
        const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
        const float to = floor_y + (down ? -kStackPitch : kStackPitch);
        return pipe_reaches(floor_y) && pipe_reaches(to) && get_on_pipe(floor_y, to, !route_on_edge_);
    }
    return index >= 0 && begin_climb(index, down);
}

void Matron::remove_body() {
    end_journey(false);
    routecraft_.cancel();
    routecraft_recovering_ = false;
    routecraft_mine_ = false;
    found_ = FoundClimb{};
    if (!body_in_world_) {
        return;
    }
    release_climb(false);
    physics_.GetBodyInterface().RemoveBody(body_);
    body_in_world_ = false;
}

bool Matron::sees(const RVec3 from, const RVec3 to) const {
    const Vec3 run(to - from);
    const float length = run.Length();
    if (length < 0.5F) {
        return true;
    }
    const IgnoreSingleBodyFilter self(body_);
    // Two lines a head's width apart: a post narrower than that (a lever's,
    // a gantry's) hides nobody from her.
    const Vec3 flat(run.GetX(), 0.0F, run.GetZ());
    const Vec3 across = flat.LengthSq() > 1.0e-6F ? Vec3(-flat.GetZ(), 0.0F, flat.GetX()).Normalized() * kEyesHalfM
                                                  : Vec3::sZero();
    for (const float side : {-1.0F, 1.0F}) {
        RayCastResult hit;
        if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from + across * side, run), hit, {}, {}, self)) {
            return true;
        }
        // A hit on the player (or just short of them) is the line reaching them.
        if (hit.mFraction >= 1.0F - 0.7F / length) {
            return true;
        }
    }
    return false;
}

float Matron::muffled_distance(const RVec3 from, const RVec3 to) const {
    const Vec3 run(to - from);
    const float straight = run.Length();
    if (straight < 0.1F) return straight;
    if (acoustic_queries_ >= kMaxAcousticQueries) return std::numeric_limits<float>::max();
    // Bounded storage and work per path: no allocating all-hit collector.
    // Saturation is conservatively inaudible, never an untested clear path.
    class BoundedHits final : public CastRayCollector {
    public:
        std::array<RayCastResult, kMaxAcousticHits> hits{};
        std::size_t count = 0;
        void AddHit(const RayCastResult &hit) override {
            if (count < hits.size()) hits[count++] = hit;
            if (count == hits.size()) ForceEarlyOut();
        }
    } hits;
    class BuiltOnly final : public BodyFilter {
    public:
        explicit BuiltOnly(const BodyID locker) : locker_(locker) {}
        bool ShouldCollide(const BodyID &id) const override { return id != locker_; }
        bool ShouldCollideLocked(const Body &body) const override {
            return body.IsStatic() && !body.IsSensor();
        }
    private:
        BodyID locker_;
    } built(locker_body_);
    const RRayCast ray(from, run);
    ++acoustic_queries_;
    physics_.GetNarrowPhaseQuery().CastRay(ray, RayCastSettings(), hits, {}, {}, built);
    if (hits.count == hits.hits.size()) return std::numeric_limits<float>::max();
    float carried = straight;
    std::array<BodyID, kMaxAcousticHits> barriers{};
    std::size_t barrier_count = 0;
    for (std::size_t i = 0; i < hits.count; ++i) {
        const RayCastResult &hit = hits.hits[i];
        if (hit.mFraction * straight < 0.15F || (1.0F - hit.mFraction) * straight < 0.15F) continue;
        bool duplicate = false;
        for (std::size_t j = 0; j < barrier_count; ++j) duplicate |= barriers[j] == hit.mBodyID;
        if (duplicate) continue;
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded()) return std::numeric_limits<float>::max();
        const RVec3 point = ray.GetPointOnRay(hit.mFraction);
        bool deck = false;
        if (std::abs(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point).GetY()) > 0.7F) {
            const float y = float(point.GetY());
            for (const Level &level : levels_) {
                deck |= y > level.floor_y - kDeckDepthM && y < level.floor_y + 0.05F;
            }
        }
        if (!deck) {
            // Low thuds diffract around a narrow rail or rung. Test the hit
            // leaf, not a whole ladder's/compound's envelope, against a
            // 30 cm acoustic aperture perpendicular to the sound path.
            TransformedShape leaf = lock.GetBody().GetTransformedShape();
            SubShapeID remainder = hit.mSubShapeID2;
            for (int depth = 0; depth < 8; ++depth) {
                SubShapeID next;
                TransformedShape child = leaf.GetSubShapeTransformedShape(remainder, next);
                if (child.mShape == leaf.mShape || child.mShape == nullptr) break;
                leaf = child;
                remainder = next;
            }
            const Vec3 direction = run / straight;
            const Vec3 horizontal(-direction.GetZ(), 0, direction.GetX());
            const Vec3 across = horizontal.LengthSq() > 1.0e-6F ? horizontal.Normalized() : Vec3::sAxisX();
            const Vec3 up = direction.Cross(across);
            const AABox bounds = leaf.mShape->GetLocalBounds();
            const Vec3 size = (bounds.mMax - bounds.mMin) * leaf.GetShapeScale().Abs();
            const auto span = [&](const Vec3 axis) {
                return (leaf.mShapeRotation.Conjugated() * axis).Abs().Dot(size);
            };
            if (span(across) < kAcousticApertureM || span(up) < kAcousticApertureM) continue;
        }
        // Only accepted barriers count for deduplication. A compound's rail
        // must not mask a wall hit belonging to that same compound body.
        barriers[barrier_count++] = hit.mBodyID;
        carried *= deck ? kDeckMuffle : kObstructionMuffle;
    }
    return carried;
}

float Matron::carry_m(const RVec3 from, const RVec3 to) const {
    const float straight = Vec3(to - from).Length();
    const float through = muffled_distance(from, to);
    if (through <= straight || std::abs(from.GetY() - to.GetY()) < 1.0) return through;
    // Or round by the shaft: out to it level, along it, and in again level.
    const auto in_shaft = [](const RVec3 p, const float inset) {
        const float half = kShaftHalfM - inset;
        return RVec3(std::clamp(float(p.GetX()), -half, half), p.GetY(),
                     std::clamp(float(p.GetZ()), kMidZ - half, kMidZ + half));
    };
    // The floor edge is not an open ray: its supporting beam projects 25 cm
    // into the shaft. Half a metre clears that beam plus the 15 cm aperture
    // radius. Every displaced leg still gets its ordinary geometry query.
    const RVec3 out = in_shaft(from, 0.5F);
    const RVec3 in = in_shaft(to, 0.5F);
    // Bends describe the actual room/shaft boundary; clearance displacement
    // must not introduce a second bend for a source already at that opening.
    const float to_shaft = Vec3(in_shaft(from, 0.0F) - from).Length();
    const float from_shaft = Vec3(to - in_shaft(to, 0.0F)).Length();
    float round = muffled_distance(from, out) + muffled_distance(out, in) + muffled_distance(in, to);
    round *= to_shaft > 0.5F ? kBendMuffle : 1.0F;
    round *= from_shaft > 0.5F ? kBendMuffle : 1.0F;
    return std::min(through, round);
}

void Matron::onto_floor_lines(float &x, float &z) const {
    const Level &level = levels_[static_cast<std::size_t>(level_)];
    float best = std::numeric_limits<float>::max();
    float bx = x;
    float bz = z;
    for (const Edge &e : level.edges) {
        if (e.link != Link::Floor) {
            continue;
        }
        const Node &a = level.nodes[static_cast<std::size_t>(e.a)];
        const Node &b = level.nodes[static_cast<std::size_t>(e.b)];
        float px = 0.0F;
        float pz = 0.0F;
        float t = 0.0F;
        project(a.x, a.z, b.x, b.z, x, z, px, pz, t);
        const float d = distance_xz(px, pz, x, z);
        if (d < best) {
            best = d;
            bx = px;
            bz = pz;
        }
    }
    x = bx;
    z = bz;
}

float Matron::floor_of(const float y) const {
    float best = levels_[static_cast<std::size_t>(level_)].floor_y;
    float best_above = std::numeric_limits<float>::max();
    for (const Level &level : levels_) {
        const float above = y - level.floor_y;
        if (above > -0.5F && above < 8.0F && above < best_above) {
            best_above = above;
            best = level.floor_y;
        }
    }
    return best;
}

bool Matron::on_catwalk_line(const float x, const float z, const float margin) {
    return std::abs(z - kMidZ) < 1.4F && std::abs(x) < 17.6F + margin;
}

bool Matron::route_toward(const float fx, const float fz, const float tx, const float tz, const bool allow_catwalk,
                          float &wx, float &wz, const bool allow_gaps) const {
    const Level &level = levels_[static_cast<std::size_t>(level_)];
    std::vector<Node> nodes = level.nodes;
    std::vector<Edge> edges = level.edges;
    const auto allowed = [&](const Edge &e) {
        return (e.link != Link::Catwalk || allow_catwalk) && (e.link != Link::Gap || allow_gaps) &&
               (e.link != Link::Leaf || bay_open_);
    };
    // She stands on a line she may take (the catwalk under her counts, but
    // only once she is out on it: beside it on the band, the band's line is
    // hers), and the place she heads for is put on one.
    const auto insert = [&](const float x, const float z, const bool loop_only) {
        int best = -1;
        float best_d = std::numeric_limits<float>::max();
        float bx = 0.0F;
        float bz = 0.0F;
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if ((loop_only && !allowed(edges[e])) || (edges[e].link == Link::Gap && !allow_gaps) ||
                (edges[e].link == Link::Leaf && !bay_open_) ||
                (!allowed(edges[e]) && !on_catwalk_line(x, z, 0.0F))) {
                continue;
            }
            float px = 0.0F;
            float pz = 0.0F;
            float t = 0.0F;
            const Node &a = nodes[static_cast<std::size_t>(edges[e].a)];
            const Node &b = nodes[static_cast<std::size_t>(edges[e].b)];
            project(a.x, a.z, b.x, b.z, x, z, px, pz, t);
            const float d = distance_xz(px, pz, x, z);
            if (d < best_d) {
                best_d = d;
                best = static_cast<int>(e);
                bx = px;
                bz = pz;
            }
        }
        const Edge old = edges[static_cast<std::size_t>(best)];
        nodes.push_back({bx, bz});
        const int fresh = static_cast<int>(nodes.size()) - 1;
        edges[static_cast<std::size_t>(best)] = {old.a, fresh, old.link};
        edges.push_back({fresh, old.b, old.link});
        return fresh;
    };
    const int from = insert(fx, fz, false);
    const int to = insert(tx, tz, true);
    // Dijkstra over at most a dozen nodes. Nodes she found no way to just
    // now (matron_moves.cpp, the watchdog's reroute) are left out first; if
    // that leaves no way at all, they are tried again.
    const std::size_t n = nodes.size();
    std::vector<float> dist(n, std::numeric_limits<float>::max());
    std::vector<int> prev(n, -1);
    std::vector<bool> done(n, false);
    const auto blocked_node = [&](const int i) {
        return i != from && i != to && node_blocked(nodes[static_cast<std::size_t>(i)].x,
                                                    nodes[static_cast<std::size_t>(i)].z);
    };
    for (int attempt = blocked_.empty() ? 1 : 0; attempt < 2; ++attempt) {
    std::fill(dist.begin(), dist.end(), std::numeric_limits<float>::max());
    std::fill(prev.begin(), prev.end(), -1);
    std::fill(done.begin(), done.end(), false);
    dist[static_cast<std::size_t>(from)] = 0.0F;
    for (std::size_t round = 0; round < n; ++round) {
        int u = -1;
        for (std::size_t i = 0; i < n; ++i) {
            if (!done[i] && (u < 0 || dist[i] < dist[static_cast<std::size_t>(u)])) {
                u = static_cast<int>(i);
            }
        }
        if (u < 0 || dist[static_cast<std::size_t>(u)] == std::numeric_limits<float>::max()) {
            break;
        }
        done[static_cast<std::size_t>(u)] = true;
        for (const Edge &e : edges) {
            if (!allowed(e)) {
                continue;
            }
            int v = -1;
            if (e.a == u) {
                v = e.b;
            } else if (e.b == u) {
                v = e.a;
            }
            if (v < 0 || (attempt == 0 && blocked_node(v))) {
                continue;
            }
            const Node &a = nodes[static_cast<std::size_t>(u)];
            const Node &b = nodes[static_cast<std::size_t>(v)];
            const float candidate = dist[static_cast<std::size_t>(u)] + distance_xz(a.x, a.z, b.x, b.z);
            if (candidate < dist[static_cast<std::size_t>(v)]) {
                dist[static_cast<std::size_t>(v)] = candidate;
                prev[static_cast<std::size_t>(v)] = u;
            }
        }
    }
    if (prev[static_cast<std::size_t>(to)] >= 0 || to == from) {
        break;
    }
    }
    // The path from `from` to `to`, then its first point she is not already
    // standing on (the start is inserted on whichever edge is nearest, often
    // beside a real node: heading for that node would be heading nowhere).
    if (prev[static_cast<std::size_t>(to)] < 0 && to != from) {
        wx = tx;
        wz = tz;
        return false;
    }
    std::vector<int> path;
    for (int at = to; at >= 0 && at != from; at = prev[static_cast<std::size_t>(at)]) {
        path.push_back(at);
    }
    std::reverse(path.begin(), path.end());
    constexpr float kReachM = 1.2F;
    const auto point = [&](const int i, float &x, float &z) {
        if (path[static_cast<std::size_t>(i)] == to) {
            x = tx;
            z = tz;
        } else {
            const Node &node = nodes[static_cast<std::size_t>(path[static_cast<std::size_t>(i)])];
            x = node.x;
            z = node.z;
        }
    };
    wx = tx;
    wz = tz;
    int pick = static_cast<int>(path.size()) - 1;
    for (int i = 0; i < static_cast<int>(path.size()); ++i) {
        const Node &node = nodes[static_cast<std::size_t>(path[static_cast<std::size_t>(i)])];
        if (path[static_cast<std::size_t>(i)] == to || distance_xz(fx, fz, node.x, node.z) > kReachM) {
            pick = i;
            break;
        }
    }
    if (pick >= 0) {
        point(pick, wx, wz);
        last_way_node_ = path[static_cast<std::size_t>(pick)] != to;
        last_way_x_ = wx;
        last_way_z_ = wz;
    }
    // Something built in the straight way (a post, a ladder's rail, a cage's
    // corner by her post): when it is the place she is going to, first its
    // own point on her line (from there the place is a step off it); else the
    // nearest point before it on her path she can walk to straight; failing
    // that, back onto her line, which runs clear. A point she already stands
    // on is no way round.
    if (!way_clear(fx, fz, wx, wz)) {
        const auto way_round = [&](const float x, const float z) {
            return distance_xz(fx, fz, x, z) > 0.15F && way_clear(fx, fz, x, z);
        };
        bool found = false;
        if (pick >= 0 && path[static_cast<std::size_t>(pick)] == to) {
            const Node &place = nodes[static_cast<std::size_t>(to)];
            if (way_round(place.x, place.z)) {
                wx = place.x;
                wz = place.z;
                found = true;
            }
        }
        for (int i = pick - 1; i >= 0 && !found; --i) {
            float x = 0.0F;
            float z = 0.0F;
            point(i, x, z);
            if (way_round(x, z)) {
                wx = x;
                wz = z;
                found = true;
            }
        }
        const Node &on_line = nodes[static_cast<std::size_t>(from)];
        if (!found && distance_xz(fx, fz, on_line.x, on_line.z) > 0.15F) {
            wx = on_line.x;
            wz = on_line.z;
        }
    }
    return true;
}

bool Matron::way_clear(const float fx, const float fz, const float tx, const float tz) const {
    const float dx = tx - fx;
    const float dz = tz - fz;
    const float length = std::hypot(dx, dz);
    if (length < 0.05F) {
        return true;
    }
    // Only what is built in: a hanging handle or a loose crate swings or
    // slides aside; a post does not.
    class BuiltOnly final : public BodyFilter {
    public:
        bool ShouldCollideLocked(const Body &body) const override { return body.IsStatic(); }
    };
    const BuiltOnly built;
    // Her width at her hips, swept the whole way.
    const RVec3 start(fx, feet_y_ + 0.9F, fz);
    const RShapeCast sweep(hips_, Vec3::sReplicate(1.0F), RMat44::sTranslation(start), Vec3(dx, 0.0F, dz));
    AnyHitCollisionCollector<CastShapeCollector> hit;
    physics_.GetNarrowPhaseQuery().CastShape(sweep, ShapeCastSettings(), start, hit, {}, {}, built);
    if (hit.HadHit()) {
        return false;
    }
    // And at her knees (kKneeSweepM, over what a step-up takes): a low box or
    // a curb at the hips' sweep passes under it, and stopped her dead.
    const RVec3 knee(fx, feet_y_ + kKneeSweepM, fz);
    const RShapeCast low(hips_, Vec3::sReplicate(1.0F), RMat44::sTranslation(knee), Vec3(dx, 0.0F, dz));
    AnyHitCollisionCollector<CastShapeCollector> knee_hit;
    physics_.GetNarrowPhaseQuery().CastShape(low, ShapeCastSettings(), knee, knee_hit, {}, {}, built);
    return !knee_hit.HadHit();
}

void Matron::take_lead(const float x, const float z, const float floor, const float loudness, const bool seen) {
    noise_heard_ = loudness;
    heard_x_ = x;
    heard_z_ = z;
    lead_floor_ = floor;
    guess_line_ = -1;
    guess_climb_ = -1;
    since_heard_s_ = 0.0F;
    pursuit_s_ = 0.0F;
    lead_seen_ = seen;
    lead_valid_ = true;
    chase_way_held_ = false;  // a fresh lead earns its own chase window
}

void Matron::stand_down() {
    lead_valid_ = false;
    chase_way_held_ = false;
    turn_ = Turn::None;
    turn_s_ = 0.0F;
    wary_ = false;
    peer_s_ = 0.0F;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    if (reachable_post(float(at.GetX()), float(at.GetZ())) != nullptr) {
        state_ = State::Guarding;
    } else {
        state_ = State::Shuffling;
        pick_patrol(float(at.GetX()), float(at.GetZ()));
    }
}

const Matron::Post *Matron::reachable_post(const float ax, const float az) const {
    const Level &level = levels_[static_cast<std::size_t>(level_)];
    if (patrol_only_ || level.way < 0 || !ways_out_[static_cast<std::size_t>(level.way)]) {
        return nullptr;
    }
    for (const Post &post : level.posts) {
        float wx = 0.0F;
        float wz = 0.0F;
        if (route_toward(ax, az, post.x, post.z, false, wx, wz)) {
            return &post;
        }
    }
    return nullptr;
}

void Matron::pick_patrol(const float ax, const float az) {
    const Level &level = levels_[static_cast<std::size_t>(level_)];
    for (std::size_t loop = 0; loop < level.patrols.size(); ++loop) {
        const std::vector<int> &nodes = level.patrols[loop];
        const Node &first = level.nodes[static_cast<std::size_t>(nodes.front())];
        float wx = 0.0F;
        float wz = 0.0F;
        if (!route_toward(ax, az, first.x, first.z, false, wx, wz)) {
            continue;
        }
        patrol_ = static_cast<int>(loop);
        float best = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const Node &node = level.nodes[static_cast<std::size_t>(nodes[i])];
            const float d = distance_xz(ax, az, node.x, node.z);
            if (d < best) {
                best = d;
                patrol_node_ = static_cast<int>(i);
            }
        }
        return;
    }
}

void Matron::feel_floor(const float dt) {
    if (const Mover *way = holder(); way != nullptr && way->view().tick == MoverTick::Rides) {
        // Hanging from a cable: held until she lets go of it.
        riding_ = true;
        let_go_s_ = kLetGoSeconds;
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    RayCastResult hit;
    const IgnoreSingleBodyFilter self(body_);
    const float reach = kStandCentre + 0.3F;
    float floor_vy = 0.0F;
    // Her floor's height and motion, from the support under her: her middle
    // first, then around it in a cross, for the same narrow-support reason
    // as walk_toward's drive gate (a step, a beam, a ladder foot with her
    // middle past its edge). Where her middle is over the floor this reads
    // exactly as before.
    const auto floor_hit = [&](const RVec3 &from) {
        return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -reach, 0.0F)), hit, {}, {}, self);
    };
    const Vec3 facing(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 across(std::cos(yaw_), 0.0F, -std::sin(yaw_));
    if (floor_hit(at) || floor_hit(at + across * kFootSpanM) || floor_hit(at - across * kFootSpanM) ||
        floor_hit(at + facing * kFootSpanM) || floor_hit(at - facing * kFootSpanM)) {
        feet_y_ = float(at.GetY()) - reach * hit.mFraction;
        // The bay's leaf on its stop under its motor is a walkway: what it
        // gives under her stride is not a floor going.
        floor_vy = hit.mBodyID == bay_leaf_ && bay_open_ ? 0.0F : bodies.GetLinearVelocity(hit.mBodyID).GetY();
    }
    if (std::abs(floor_vy) > kRideMps) {
        if (!riding_) {
            // Whatever she was about, the floor going takes it from her.
            turn_ = Turn::None;
            turn_s_ = 0.0F;

        }
        riding_ = true;
        let_go_s_ = kLetGoSeconds;
    } else if (riding_) {
        let_go_s_ -= dt;
        if (let_go_s_ <= 0.0F) {
            riding_ = false;
            let_go_s_ = 0.0F;
        }
    }
}

void Matron::settle_level() {
    if (riding_) {
        return;
    }
    // On a level is standing on it: not passing its height falling, and (where
    // her map knows the deck's steel) inside its footprint.
    BodyInterface &bodies = physics_.GetBodyInterface();
    if (std::abs(bodies.GetLinearVelocity(body_).GetY()) > 0.6F) {
        return;
    }
    const RVec3 here = bodies.GetPosition(body_);
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        if (static_cast<int>(i) != level_ && std::abs(feet_y_ - levels_[i].floor_y) < 0.6F &&
            !off_its_steel(static_cast<int>(i), float(here.GetX()), feet_y_, float(here.GetZ()))) {
            level_ = static_cast<int>(i);
            commit_s_ = 0.0F;
            const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
            pick_patrol(float(at.GetX()), float(at.GetZ()));
            return;
        }
    }
}

bool Matron::off_its_steel(const int level, const float x, const float feet, const float z) const {
    for (std::size_t r = 0; r < routes::kRegionCount; ++r) {
        const routes::RegionInfo &info = routes_.region(static_cast<routes::Region>(r));
        if (info.level == level) {
            return routes_.region_at(x, feet, z) != info.id;
        }
    }
    return false;   // a level her map does not know: its height is all she has
}

void Matron::walk_toward(const float dt, float wx, float wz, const float speed, const float foot_accel,
                         const bool turn_moves) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    // A sidestep the stall watchdog set (matron_moves.cpp): along the face
    // in her way a moment, whatever she was making for.
    if (sidestep_s_ > 0.0F && speed > 0.0F) {
        wx = sidestep_x_;
        wz = sidestep_z_;
    }
    target_x_ = wx;
    target_z_ = wz;
    Vec3 velocity = bodies.GetLinearVelocity(body_);
    const Vec3 start_v = velocity;
    foot_drive_dv_ = Vec3::sZero();
    foot_force_n_ = 0.0F;
    foot_power_w_ = 0.0F;
    foot_lunge_ = false;
    bool floor_boarding = false;
    // Feet drive only on a floor, and not once she has jumped off it (rising
    // off it faster than it rises): a vault or a leap flies until she lands.
    // A single ray down her middle misses a narrow support she still rests
    // on (a step, a beam, a ladder foot: her capsule overhangs it with her
    // middle past its edge), and then she can neither drive nor trip the
    // stall watchdog, so she stands forever where the hunt's ways left
    // her. Her feet span either side of her middle (kFootSpanM): rays
    // there count too, in a cross (sideways and fore-aft), centre first so
    // nothing changes where her middle is over the floor.
    const IgnoreSingleBodyFilter self(body_);
    const auto supported = [&](const RVec3 &from) {
        RayCastResult hit;
        return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), hit, {},
                                                      {}, self) &&
               velocity.GetY() < bodies.GetLinearVelocity(hit.mBodyID).GetY() + 1.0F;
    };
    const Vec3 facing(std::sin(yaw_), 0.0F, std::cos(yaw_));
    const Vec3 across(std::cos(yaw_), 0.0F, -std::sin(yaw_));
    const bool floor = supported(at) || supported(at + across * kFootSpanM) || supported(at - across * kFootSpanM) ||
                       supported(at + facing * kFootSpanM) || supported(at - facing * kFootSpanM);
    // Validated sole contact for the force-law foot drive (patch Item 4, adapted to dadee's
    // supported() cross-ray): first hit, centre first, that is a real floor under her.
    BodyID support_body;
    SubShapeID support_sub;
    RVec3 support_point = at;
    Vec3 support_local = Vec3::sZero();
    Vec3 support_normal = Vec3::sAxisY();
    Vec3 support_v = Vec3::sZero();
    {
        IgnoreMultipleBodiesFilter support_filter;
        support_filter.Reserve(2);
        support_filter.IgnoreBody(body_);
        support_filter.IgnoreBody(player_);
        for (const Vec3 off : {Vec3::sZero(), across * kFootSpanM, -across * kFootSpanM, facing * kFootSpanM,
                               -facing * kFootSpanM}) {
            RayCastResult hit;
            const RRayCast ray(at + off, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F));
            if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, support_filter)) {
                continue;
            }
            const RVec3 contact = ray.GetPointOnRay(hit.mFraction);
            const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
            if (!lock.Succeeded() || lock.GetBody().IsSensor()) {
                continue;
            }
            const Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, contact);
            if (normal.GetY() < 0.5F) {
                continue;
            }
            const Vec3 sv = bodies.GetPointVelocity(hit.mBodyID, contact);
            if (velocity.GetY() < sv.GetY() + 1.0F) {
                support_body = hit.mBodyID;
                support_sub = hit.mSubShapeID2;
                support_point = contact;
                support_local = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * contact);
                support_normal = normal;
                support_v = sv;
                break;
            }
        }
    }
    const traversal::Limits limits{};
    const bool lunge_push = turn_ == Turn::Lunge && !foot_driving_;
    const float power_cap = lunge_push ? kTurnLungePowerW : limits.power_w;
    const float force_cap = lunge_push ? kTurnLungeForceN : limits.total_force_n;
    traversal::Budget budget{force_cap, 0.0F, power_cap, 0.0F, velocity - support_v};
    const auto apply_foot_force = [&](const Vec3 want_force, const BodyID on_body, const RVec3 on_point,
                                      const Vec3 slope_slide = Vec3::sZero()) {
        if (on_body.IsInvalid() || !bodies.IsAdded(on_body)) {
            return Vec3::sZero();
        }
        (void)slope_slide;
        budget.velocity = velocity - bodies.GetPointVelocity(on_body, on_point);
        const Vec3 granted = budget.take(want_force);
        if (granted.LengthSq() > 0.0F) {
            bodies.AddForce(body_, granted);
            if (bodies.GetMotionType(on_body) == EMotionType::Dynamic) {
                bodies.AddForce(on_body, -granted, on_point);
            }
            foot_drive_dv_ += (granted / kMassKg) * dt;
            foot_force_n_ = budget.used;
            foot_power_w_ = budget.power_used;
            foot_lunge_ = lunge_push;
        }
        return granted;
    };
    float dx = wx - float(at.GetX());
    float dz = wz - float(at.GetZ());
    // Where she is headed is on a machine's floor (a cage, a lift), and
    // that floor is still a step or a clamber. She walks onto it. The
    // push up is the shared force budget, held over the ticks it takes,
    // so a floor that has started to rise does not leave her at the lip.
    // A loose crate is not a machine.
    bool boarding = false;
    float board_top = 0.0F;
    BodyID board_body;
    if (floor) {
        const float reach = std::hypot(dx, dz);
        if (reach > 0.35F) {
            const float feet_now = float(at.GetY()) - kStandCentre;
            const Vec3 dir(dx / reach, 0.0F, dz / reach);
            const auto machine = [&](const BodyID id) {
                if (bodies.GetMotionType(id) != EMotionType::Dynamic) {
                    return false;
                }
                const std::uint64_t entity = bodies.GetUserData(id);
                return entity >= kit::kDynamicEntityBase && entity < kit::kEntityLimit &&
                       !kit::is_stock_entity(entity) && !kit::is_fabricated_entity(entity);
            };
            IgnoreMultipleBodiesFilter not_us;
            not_us.Reserve(2);
            not_us.IgnoreBody(body_);
            not_us.IgnoreBody(player_);
            const auto top_at = [&](const float x, const float z, float &top, BodyID &id) {
                const RVec3 from(x, feet_now + kClamberMaxM + 0.3F, z);
                RayCastResult down;
                const float depth = kClamberMaxM + 0.8F;
                if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), down, {}, {},
                                                            not_us)) {
                    return false;
                }
                if (!machine(down.mBodyID)) {
                    return false;
                }
                const BodyLockRead lock(physics_.GetBodyLockInterface(), down.mBodyID);
                if (!lock.Succeeded()) {
                    return false;
                }
                const RVec3 point = from + Vec3(0.0F, -depth * down.mFraction, 0.0F);
                if (lock.GetBody().GetWorldSpaceSurfaceNormal(down.mSubShapeID2, point).GetY() < 0.7F) {
                    return false;
                }
                top = feet_now + kClamberMaxM + 0.3F - depth * down.mFraction;
                id = down.mBodyID;
                return top >= feet_now - 0.05F && top <= feet_now + kClamberMaxM;
            };
            float goal_top = 0.0F;
            BodyID goal_id;
            if (top_at(wx, wz, goal_top, goal_id) && goal_top > feet_now + 0.08F) {
                float lip = reach;
                float lip_top = goal_top;
                BodyID lip_id = goal_id;
                for (float d = 0.3F; d < reach; d += 0.3F) {
                    float sample = 0.0F;
                    BodyID sample_id;
                    if (top_at(float(at.GetX()) + dir.GetX() * d, float(at.GetZ()) + dir.GetZ() * d, sample,
                               sample_id)) {
                        lip = d;
                        lip_top = sample;
                        lip_id = sample_id;
                        break;
                    }
                }
                const float onto = std::min(reach, lip + 0.55F);
                wx = float(at.GetX()) + dir.GetX() * onto;
                wz = float(at.GetZ()) + dir.GetZ() * onto;
                dx = wx - float(at.GetX());
                dz = wz - float(at.GetZ());
                // The push starts when the lip is a step away, not from
                // across the deck: a force from six metres off throws her.
                if (lip < 1.8F) {
                    boarding = true;
                    board_top = lip_top;
                    board_body = lip_id;
                }
            }
        }
    }
    const float length = std::hypot(dx, dz);
    Vec3 want = Vec3::sZero();
    if (length > 0.15F) {
        want = Vec3(dx / length * speed, 0.0F, dz / length * speed);
        yaw_ = turn_toward(yaw_, std::atan2(dx, dz), kTurnRate, dt);
    }
    if (floor) {
        // Speeding up along her heading is slow (foot_accel); planting a foot to
        // Speeding up along her heading is slow (foot_accel); planting a foot to
        // brake or cut sideways is quick (kCutAccel): without that she cannot
        // turn inside a five-metre circle at a run and orbits a corner she is
        // heading for.
        const Vec3 flat(velocity.GetX(), 0.0F, velocity.GetZ());
        Vec3 delta = want - flat;
        const float cut_limit = std::max(foot_accel, kCutAccel) * dt;
        const float speed_now = flat.Length();
        if (speed_now > 0.2F) {
            const Vec3 heading = flat / speed_now;
            float along = delta.Dot(heading);
            Vec3 lateral = delta - heading * along;
            along = along > 0.0F ? std::min(along, foot_accel * dt) : std::max(along, -cut_limit);
            if (lateral.Length() > cut_limit) {
                lateral = lateral.Normalized() * cut_limit;
            }
            delta = heading * along + lateral;
        } else if (delta.Length() > foot_accel * dt) {
            delta = delta.Normalized() * (foot_accel * dt);
        }
        velocity = Vec3(velocity.GetX() + delta.GetX(), velocity.GetY(), velocity.GetZ() + delta.GetZ());
        IgnoreMultipleBodiesFilter not_us;
        not_us.Reserve(2);
        not_us.IgnoreBody(body_);
        not_us.IgnoreBody(player_);
        // Floor `ahead_m` along `dir`, looked for from `above` her feet down
        // `depth`: its top, if there is one.
        const float feet = float(at.GetY()) - kStandCentre;
        const auto floor_under = [&](const RVec3 &from, const float depth, float &top) {
            RayCastResult down;
            if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), down, {}, {},
                                                        not_us)) {
                return false;
            }
            top = float(from.GetY()) - depth * down.mFraction;
            return true;
        };
        // Floor she can stand on: under her middle and across her width (a
        // board or a beam narrower than she is is no floor to her).
        const auto floor_at = [&](const Vec3 &dir, const float ahead_m, const float above, const float depth,
                                  float &top) {
            const RVec3 from = at + dir * ahead_m + Vec3(0.0F, above - kStandCentre, 0.0F);
            const Vec3 across(-dir.GetZ(), 0.0F, dir.GetX());
            float side = 0.0F;
            return floor_under(from, depth, top) && floor_under(from + across * kFootSpanM, depth, side) &&
                   floor_under(from - across * kFootSpanM, depth, side);
        };
        // An edge `ahead_m` along `dir`: no floor within kEdgeDropM below her
        // feet there, nor for kStepOverM on (a crack between plates, or the
        // step between two cages, her foot bridges: not an edge).
        // Centre only, not the full footprint: walking parallel to an edge
        // (or diagonally past its corner) puts a side ray past the edge
        // while her centre path stays on deck, and the full-width test
        // then cancels the whole velocity and traps her at the edge.
        // Falling off is about her centre leaving floor, so that is what
        // this test checks; floor_at (full width) still judges landings
        // and steps elsewhere.
        float top = 0.0F;
        const auto edge_at = [&](const Vec3 &dir, const float ahead_m) {
            for (float on = 0.0F; on <= kStepOverM + 0.01F; on += 0.15F) {
                if (floor_under(at + dir * (ahead_m + on) + Vec3(0.0F, 0.5F - kStandCentre, 0.0F),
                                0.5F + kEdgeDropM, top)) {
                    return false;
                }
                // A top she can still clamber onto is not a drop. The low
                // ray starts at her knees and misses a floor that has
                // risen above them. A rail fails the full-width check.
                float high_top = 0.0F;
                if (floor_at(dir, ahead_m + on, kClamberMaxM + 0.3F, kClamberMaxM + 0.5F, high_top) &&
                    high_top >= feet - 0.05F && high_top <= feet + kClamberMaxM) {
                    return false;
                }
            }
            return true;
        };
        const auto step_planted_foot = [&]() -> bool {
        if (!foot_driving_) {
            return false;
        }
        if (dt <= 0.0F || ++foot_ticks_ > 65) {
            foot_driving_ = false;
            foot_prep_ = false;
            return false;
        }
        if (foot_prep_) {
            if (!floor) {
                foot_driving_ = false;
                foot_prep_ = false;
                return false;
            }
            const float dist_now = Vec3(foot_obstacle_ - at).Dot(foot_heading_);
            const float along_now = Vec3(velocity.GetX(), 0.0F, velocity.GetZ()).Dot(foot_heading_);
            if ((dist_now >= foot_prep_dist_ && along_now >= -0.08F) || edge_at(-foot_heading_, 0.30F) ||
                foot_ticks_ > 28) {
                foot_prep_ = false;
                foot_body_ = support_body;
                foot_sub_ = support_sub;
                foot_local_ = support_local;
                foot_start_y_ = float(at.GetY() - support_point.GetY());
                foot_ticks_ = 1;
            } else {
                const float err = dist_now - (foot_prep_dist_ + 0.06F);
                const float want_along = std::clamp(err * 6.0F, -2.5F, 1.5F);
                const Vec3 flat_v(velocity.GetX(), 0.0F, velocity.GetZ());
                Vec3 want_f = (foot_heading_ * want_along - flat_v) * (kMassKg / dt);
                const float cap = 0.70F * limits.total_force_n;
                if (want_f.Length() > cap) {
                    want_f = want_f.Normalized() * cap;
                }
                (void)apply_foot_force(want_f, support_body, support_point);
                return true;
            }
        }
        if (foot_body_.IsInvalid() || !bodies.IsAdded(foot_body_)) {
            foot_driving_ = false;
            return false;
        }
        RVec3 anchor = RVec3::sZero();
        {
            const BodyLockRead lock(physics_.GetBodyLockInterface(), foot_body_);
            if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor()) {
                foot_driving_ = false;
                return false;
            }
            anchor = lock.GetBody().GetCenterOfMassTransform() * foot_local_;
        }
        const float reach = kStandCentre + foot_stroke_m_ + 0.12F;
        const RRayCast ray(RVec3(anchor.GetX(), at.GetY(), anchor.GetZ()), Vec3(0.0F, -reach, 0.0F));
        RayCastResult hit;
        if (!physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, not_us) || hit.mBodyID != foot_body_ ||
            hit.mSubShapeID2 != foot_sub_) {
            foot_driving_ = false;
            return false;
        }
        const RVec3 contact = ray.GetPointOnRay(hit.mFraction);
        if (Vec3(contact - anchor).Length() > 0.06F) {
            foot_driving_ = false;
            return false;
        }
        {
            const BodyLockRead lock(physics_.GetBodyLockInterface(), foot_body_);
            if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(foot_sub_, contact).GetY() < 0.5F) {
                foot_driving_ = false;
                return false;
            }
        }
        const float ext_y = float(at.GetY() - contact.GetY()) - foot_start_y_;
        const float ext_xz = std::hypot(float(at.GetX() - contact.GetX()), float(at.GetZ() - contact.GetZ()));
        if (ext_y < -0.35F || ext_y >= foot_stroke_m_ || ext_xz > 0.85F) {
            foot_driving_ = false;
            return false;
        }
        const Vec3 sv = bodies.GetPointVelocity(foot_body_, contact);
        const Vec3 rel_v = velocity - sv;
        const float along_v = Vec3(rel_v.GetX(), 0.0F, rel_v.GetZ()).Dot(foot_heading_);
        if (foot_ticks_ > 1 && rel_v.GetY() >= foot_want_vy_ - 0.02F && std::abs(along_v - foot_want_vx_) <= 0.15F) {
            foot_driving_ = false;
            return false;
        }
        const Vec3 flat_rel(rel_v.GetX(), 0.0F, rel_v.GetZ());
        Vec3 f_horiz = (foot_heading_ * foot_want_vx_ - flat_rel) * (kMassKg / dt);
        const float horiz_cap = 0.65F * limits.total_force_n;
        if (f_horiz.Length() > horiz_cap) {
            f_horiz = f_horiz.Normalized() * horiz_cap;
        }
        const float horiz_power = f_horiz.Dot(flat_rel);
        if (horiz_power > limits.power_w && horiz_power > 1.0e-4F) {
            f_horiz *= limits.power_w / horiz_power;
        }
        const float g = -physics_.GetGravity().GetY();
        const float next_vy_no_push = rel_v.GetY() - g * dt;
        const float dvy = std::max(0.0F, foot_want_vy_ - next_vy_no_push);
        const float fy_avail =
            std::sqrt(std::max(0.0F, limits.total_force_n * limits.total_force_n - f_horiz.LengthSq()));
        const float p_left = std::max(0.0F, limits.power_w - std::max(0.0F, f_horiz.Dot(flat_rel)));
        float fy = std::min(fy_avail, dvy * (kMassKg / dt));
        if (rel_v.GetY() > 0.01F) {
            fy = std::min(fy, p_left / rel_v.GetY());
        }
        (void)apply_foot_force(Vec3(f_horiz.GetX(), fy, f_horiz.GetZ()), foot_body_, contact);
        return true;
    };
    const auto start_foot_jump = [&](const Vec3 &heading, const float stroke_m, const float want_vx,
                                     const float want_vy, const bool prep, const float prep_dist,
                                     const RVec3 &obstacle) {
        foot_driving_ = true;
        foot_prep_ = prep;
        foot_body_ = support_body;
        foot_sub_ = support_sub;
        foot_local_ = support_local;
        foot_start_y_ = float(at.GetY() - support_point.GetY());
        foot_stroke_m_ = stroke_m;
        foot_prep_dist_ = prep_dist;
        foot_obstacle_ = obstacle;
        foot_heading_ = heading;
        foot_want_vx_ = want_vx;
        foot_want_vy_ = want_vy;
        foot_ticks_ = 0;
        (void)step_planted_foot();
    };
    if (step_planted_foot()) {
        clamber_now_ = false;
        watch_stall(dt, at, 0.0F, 0.0F, false);
        const Vec3 after_v = start_v + foot_drive_dv_;
        speed_ = std::hypot(after_v.GetX(), after_v.GetZ());
        stride_ += speed_ * dt;
        return;
    }
        bool leapt = false;
        if (turn_moves && vault_cooldown_s_ <= 0.0F && length > 0.15F && speed > 0.0F) {
            // The Turn's leap: at an edge in her way, with floor again within
            // reach beyond at about her height, she jumps the gap.
            const Vec3 heading(dx / length, 0.0F, dz / length);
            // Nothing in the way across her width, at her middle and her head.
            const Vec3 across(-heading.GetZ(), 0.0F, heading.GetX());
            bool headroom = true;
            for (const float side : {-kRadius, 0.0F, kRadius}) {
                for (const float rise : {0.0F, 0.9F}) {
                    RayCastResult clear;
                    headroom = headroom && !physics_.GetNarrowPhaseQuery().CastRay(
                                               RRayCast(at + across * side + Vec3(0.0F, rise, 0.0F), heading * kLeapReachM),
                                               clear, {}, {}, not_us);
                }
            }
            if (headroom && edge_at(heading, 0.45F)) {
                bool landing = false;
                for (float reach = 0.9F; reach <= kLeapReachM && !landing; reach += 0.5F) {
                    landing = floor_at(heading, reach, 0.8F, 1.6F, top) && std::abs(top - feet) < 0.8F;
                }
                if (landing) {
                    start_foot_jump(heading, 0.40F, kLeapForwardMps, kLeapUpMps, false, 0.0F, at + heading * 0.45F);
                    vault_cooldown_s_ = kVaultCooldownS;
                    ++leaps_;
                    leapt = true;
                }
            }
        }
        bool cleared = leapt;
        const Vec3 flat_new(velocity.GetX(), 0.0F, velocity.GetZ());
        const float going = flat_new.Length();
        if (!leapt && going > 0.05F) {
            // Never off an edge: where her feet are taking her (just ahead, and
            // as far as she needs to stop from this speed), she plants them.
            const Vec3 way = flat_new / going;
            const float check = 0.3F + going * going / 40.0F + going * dt;
            if (edge_at(way, 0.2F) || edge_at(way, check)) {
                velocity -= way * std::min(going, cut_limit);
            }
            // And no step that brings her within kEdgeMarginM of an edge to
            // either side (the tower's edges run along x and z): sliding along
            // one at a slant she keeps her distance.
            for (const Vec3 &side : {Vec3::sAxisX(), -Vec3::sAxisX(), Vec3::sAxisZ(), -Vec3::sAxisZ()}) {
                const float toward = velocity.Dot(side);
                if (toward > 0.0F && edge_at(side, toward * dt + kEdgeMarginM)) {
                    velocity -= side * toward;
                }
            }
        }
        // The Turn's vault: something lower than her chest close ahead in
        // her way (not the player), and nothing at her head: she jumps it.
        if (turn_moves && !leapt && !boarding && vault_cooldown_s_ <= 0.0F && length > 0.15F &&
            !on_catwalk_line(float(at.GetX()), float(at.GetZ()), 0.5F)) {
            const Vec3 heading(dx / length, 0.0F, dz / length);
            const float look_m = std::clamp(1.55F + std::max(0.0F, speed_now - 3.5F) * 0.11F, 1.55F, 2.15F);
            const Vec3 ahead = heading * look_m;
            RayCastResult knee;
            RayCastResult head;
            const bool low = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(at + Vec3(0.0F, 0.45F - kStandCentre, 0.0F), ahead), knee, {}, {}, not_us);
            const bool high = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(at + Vec3(0.0F, 1.6F - kStandCentre, 0.0F), ahead), head, {}, {}, not_us);
            const float hit_dist = low ? look_m * knee.mFraction : look_m;
            // A loose crate is not a vault (dadee shove rule, kept): she drives it off her line instead.
            bool loose_turn = false;
            if (low) {
                const std::uint64_t entity = bodies.GetUserData(knee.mBodyID);
                loose_turn = bodies.GetMotionType(knee.mBodyID) == EMotionType::Dynamic &&
                             (kit::is_stock_entity(entity) || kit::is_fabricated_entity(entity));
            }
            // And floor where she would come down (about 2 m past the obstacle):
            // she does not vault a crate stood against the rail into the drop beyond it.
            RayCastResult landing;
            const bool lands = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(at + heading * (hit_dist + 1.9F) + Vec3(0.0F, 0.5F, 0.0F),
                         Vec3(0.0F, -(kStandCentre + 1.2F), 0.0F)),
                landing, {}, {}, not_us);
            if (low && !high && lands && !loose_turn) {
                const bool need_prep = hit_dist < 1.48F && !edge_at(-heading, (1.48F - hit_dist) + 0.35F);
                start_foot_jump(heading, 0.40F, 3.6F, 4.2F, need_prep, 1.48F, at + heading * hit_dist);
                vault_cooldown_s_ = kVaultCooldownS;
                ++vaults_;
                cleared = true;
            }
        }
        // Outside the Turn (matron.hpp, getting unstuck): her feet pressed
        // into something in her way, she steps up onto it (under kStepUpM) or
        // clambers onto it (to kClamberMaxM), slower than the Turn's vault;
        // and on the watchdog's first rung she does whichever her feet find,
        // or vaults. A finite push from her feet: the speed up that just
        // carries her feet over its top.
        bool climbed = false;
        if (!boarding && !turn_moves && !leapt && vault_cooldown_s_ <= 0.0F && length > 0.15F && speed > 0.3F &&
            !edge_at(Vec3(dx / length, 0.0F, dz / length), 0.3F)) {
            // (Only with floor under the step: across a gap only the Turn's leap.)
            const Vec3 heading(dx / length, 0.0F, dz / length);
            const float along_now = Vec3(velocity.GetX(), 0.0F, velocity.GetZ()).Dot(heading);
            const bool pressed = speed_now < 0.35F * std::min(speed, 2.0F) || clamber_now_;
            const auto ray_ahead = [&](const float rise, const float reach, float &fraction) {
                RayCastResult r;
                const bool any = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + Vec3(0.0F, rise - kStandCentre, 0.0F), heading * reach), r, {}, {}, not_us);
                fraction = any ? r.mFraction : 1.0F;
                return any;
            };
            // The top of what is in her way, `ahead_m` on, looked for from
            // her head height down.
            const auto top_ahead = [&](const float ahead_m, float &top_y) {
                return floor_under(at + heading * ahead_m + Vec3(0.0F, 2.2F - kStandCentre, 0.0F), 2.25F, top_y);
            };
            float f = 1.0F;
            float top_y = 0.0F;
            if (ray_ahead(0.12F, 0.7F, f) && !ray_ahead(kStepUpM + 0.12F, 0.9F, f) && pressed &&
                top_ahead(0.7F * f + 0.25F, top_y) && top_y - feet > 0.05F && top_y - feet <= kStepUpM) {
                const float hit_at = 0.7F * f;
                const float rise = top_y - feet + 0.12F;
                const bool need_prep = hit_at < 0.52F && !edge_at(-heading, 0.45F);
                start_foot_jump(heading, 0.38F, std::max(along_now, 1.4F), std::sqrt(2.0F * 9.81F * rise), need_prep,
                                0.52F, at + heading * hit_at);
                vault_cooldown_s_ = 0.4F;
                ++step_ups_;
                climbed = true;
            } else if (pressed || clamber_now_) {
                // Same checks as before, named so a failure can be seen:
                // the ray first (a face at knee height), then its top,
                // then room over it, then a top she can stand on. The
                // face ray sweeps her whole height, not just her knee: a
                // slab on posts (a board, a bridge deck) has open air
                // under its rim, so a knee ray passes under it while her
                // capsule still presses its edge at chest height. The
                // sweep steps a hand's width, so a thin rail or slab
                // cannot fall between two rays.
                bool ray = false;
                f = 1.0F;
                for (float rise = 0.15F; rise <= 1.65F; rise += 0.05F) {
                    float fr = 1.0F;
                    if (ray_ahead(rise, 1.0F, fr) && fr < f) {
                        f = fr;
                        ray = true;
                    }
                }
                const float hit_at = 1.0F * f;
                const bool top_ok = ray && top_ahead(hit_at + 0.3F, top_y) && top_y - feet > kStepUpM - 0.05F &&
                                    top_y - feet <= kClamberMaxM;
                if (top_ok) {
                    const float h = top_y - feet;
                    // Room over its top for her: nothing at her height above it.
                    RayCastResult over;
                    const bool room = !physics_.GetNarrowPhaseQuery().CastRay(
                        RRayCast(at + Vec3(0.0F, h + 0.3F - kStandCentre + 1.0F, 0.0F), heading * (hit_at + 0.8F)),
                        over, {}, {}, not_us);
                    if (room) {
                        // And a top she can stand on, not just a rail's
                        // strip: a rail across her path is long sideways
                        // but thin along it, so sideways rays alone would
                        // pass it and she would clamber, fall off it, and
                        // clamber again forever, starving the watchdog
                        // that would walk her round it. Floor under her
                        // whole footprint there, each ray landing on the
                        // top itself.
                        const float land_x = float(at.GetX()) + heading.GetX() * (hit_at + 0.3F);
                        const float land_z = float(at.GetZ()) + heading.GetZ() * (hit_at + 0.3F);
                        const RVec3 land_base(land_x, top_y + 0.5F, land_z);
                        const Vec3 land_across(-heading.GetZ(), 0.0F, heading.GetX());
                        const auto land_hit = [&](const RVec3 &p) {
                            float t = 0.0F;
                            return floor_under(p, 1.0F, t) && std::abs(t - top_y) < 0.3F;
                        };
                        const bool land_c = land_hit(land_base);
                        const bool land_ap = land_hit(land_base + land_across * kFootSpanM);
                        const bool land_am = land_hit(land_base - land_across * kFootSpanM);
                        const bool land_hp = land_hit(land_base + heading * kFootSpanM);
                        const bool land_hm = land_hit(land_base - heading * kFootSpanM);
                        const bool standable = land_c && land_ap && land_am && land_hp && land_hm;
                        // All five landing rays are evaluated, not
                        // short-circuited: they are read-only raycasts,
                        // so the result is the same either way.
                        if (standable) {
                            const float ax = float(at.GetX());
                            const float az = float(at.GetZ());
                            const float rdx = ax - clamber_refused_x_;
                            const float rdz = az - clamber_refused_z_;
                            bool refused = clock_s_ < clamber_refused_until_ && rdx * rdx + rdz * rdz < 1.0F;
                            if (!refused && clamber_hist_n_ >= 3) {
                                bool loop = true;
                                for (int k = 1; k <= 3 && loop; ++k) {
                                    const int idx = (clamber_hist_n_ - k) % 3;
                                    const float dx = clamber_hist_x_[idx] - ax;
                                    const float dz = clamber_hist_z_[idx] - az;
                                    if (dx * dx + dz * dz > 0.25F || clock_s_ - clamber_hist_t_[idx] > 30.0F) {
                                        loop = false;
                                    }
                                }
                                if (loop) {
                                    clamber_refused_x_ = ax;
                                    clamber_refused_z_ = az;
                                    clamber_refused_until_ = clock_s_ + 30.0F;
                                    refused = true;
                                }
                            }
                            if (!refused) {
                                const int slot = clamber_hist_n_ % 3;
                                clamber_hist_x_[slot] = ax;
                                clamber_hist_z_[slot] = az;
                                clamber_hist_t_[slot] = clock_s_;
                                ++clamber_hist_n_;
                                const float prep_d = std::clamp(0.40F + std::max(0.0F, h - 0.15F) * 0.65F, 0.55F, 1.05F);
                                const bool need_prep = hit_at < prep_d && !edge_at(-heading, (prep_d - hit_at) + 0.35F);
                                const float want_vy =
                                    std::min(4.25F, std::sqrt(2.0F * 9.81F * std::max(0.25F, h + 0.35F - 0.35F)));
                                start_foot_jump(heading, 0.40F, std::max(kClamberForwardMps, 1.8F), want_vy, need_prep, prep_d,
                                                at + heading * hit_at);
                                vault_cooldown_s_ = kClamberCooldownS;
                                ++clambers_;
                                climbed = true;
                            }
                        }
                    }
                }
            }
            if (!climbed && clamber_now_) {
                // Nothing to clamber onto: the Turn's vault, if what is in her
                // way is lower than her chest and there is floor beyond.
                const Vec3 ahead = heading * 1.55F;
                RayCastResult knee;
                RayCastResult head;
                const bool low = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + Vec3(0.0F, 0.45F - kStandCentre, 0.0F), ahead), knee, {}, {}, not_us);
                const bool high = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + Vec3(0.0F, 1.6F - kStandCentre, 0.0F), ahead), head, {}, {}, not_us);
                const float hit_dist = low ? 1.55F * knee.mFraction : 1.55F;
                RayCastResult landing;
                const bool lands = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + heading * (hit_dist + 1.9F) + Vec3(0.0F, 0.5F, 0.0F),
                             Vec3(0.0F, -(kStandCentre + 1.2F), 0.0F)),
                    landing, {}, {}, not_us);
                // A loose crate is not a vault. She drives that off her line.
                bool loose = false;
                if (low) {
                    const BodyID id = knee.mBodyID;
                    const std::uint64_t entity = bodies.GetUserData(id);
                    loose = bodies.GetMotionType(id) == EMotionType::Dynamic &&
                            (kit::is_stock_entity(entity) || kit::is_fabricated_entity(entity));
                }
                // Outside the Turn never over a gap: floor runs on under it.
                if (low && !high && lands && !loose && !edge_at(heading, 0.45F) && !edge_at(heading, 1.5F)) {
                    const bool need_prep = hit_dist < 1.48F && !edge_at(-heading, (1.48F - hit_dist) + 0.35F);
                    start_foot_jump(heading, 0.40F, 3.6F, 4.2F, need_prep, 1.48F, at + heading * hit_dist);
                    vault_cooldown_s_ = kVaultCooldownS;
                    ++vaults_;
                    climbed = true;
                }
            }
        }
        clamber_now_ = false;
        // A loose crate that stopped her, off the side that has floor
        // (matron.hpp). Same force and power as the rest of her. Both
        // sides a drop, she leaves it. She does not vault it.
        if (!cleared && !climbed && length > 0.15F && speed > 0.3F && speed_now < 1.2F) {
            const Vec3 heading(dx / length, 0.0F, dz / length);
            const Vec3 side(-heading.GetZ(), 0.0F, heading.GetX());
            RayCastResult block;
            bool found = false;
            for (const float lateral : {-0.8F, 0.0F, 0.8F}) {
                RayCastResult hit;
                if (!physics_.GetNarrowPhaseQuery().CastRay(
                        RRayCast(at + side * lateral + Vec3(0.0F, 0.45F - kStandCentre, 0.0F), heading * 4.0F), hit,
                        {}, {}, not_us)) {
                    continue;
                }
                const std::uint64_t entity = bodies.GetUserData(hit.mBodyID);
                if (bodies.GetMotionType(hit.mBodyID) == EMotionType::Dynamic &&
                    (kit::is_stock_entity(entity) || kit::is_fabricated_entity(entity))) {
                    block = hit;
                    found = true;
                    break;
                }
            }
            if (found) {
                const BodyID id = block.mBodyID;
                const bool left_void = edge_at(side, 5.0F);
                const bool right_void = edge_at(-side, 5.0F);
                if (!left_void || !right_void) {
                    Vec3 dir = heading;
                    if (!left_void && right_void) {
                        dir = side;
                    } else if (left_void && !right_void) {
                        dir = -side;
                    } else {
                        dir = side;
                    }
                    const float along = std::max(0.4F, bodies.GetLinearVelocity(id).Dot(dir));
                    const traversal::Limits budget;
                    const float shove_force = turn_ == Turn::Lunge ? kTurnLungeForceN : budget.total_force_n;
                    const float shove_power = turn_ == Turn::Lunge ? kTurnLungePowerW : budget.power_w;
                    const float force_n = std::min(shove_force, shove_power / along);
                    Vec3 push = dir + Vec3(0.0F, 0.08F, 0.0F);
                    push /= push.Length();
                    bodies.AddForce(id, push * force_n, EActivation::Activate);
                }
            }
        }
        if (boarding && !board_body.IsInvalid()) {
            const float feet_now = float(at.GetY()) - kStandCentre;
            const float floor_vy = bodies.GetLinearVelocity(board_body).GetY();
            if (feet_now < board_top - 0.02F && velocity.GetY() < floor_vy + 2.0F) {
                const float gap = std::max(0.05F, board_top - feet_now);
                const float need = std::max(floor_vy + 1.2F, gap / 0.25F);
                if (velocity.GetY() < need) {
                    const traversal::Limits budget;
                    const float speed_up = std::max(0.5F, std::abs(velocity.GetY()));
                    const float cap = std::min(budget.total_force_n, budget.power_w / speed_up);
                    const float want = (need - velocity.GetY()) * kMassKg / std::max(dt, 1.0e-3F);
                    bodies.AddForce(body_, Vec3(0.0F, std::min(cap, std::max(0.0F, want)), 0.0F), EActivation::Activate);
                }
            }
        }
        if (boarding) {
            bodies.SetLinearVelocity(body_, velocity);
        } else if (!leapt && !cleared && !climbed) {
            const Vec3 dv_horiz(velocity.GetX() - bodies.GetLinearVelocity(body_).GetX(), 0.0F,
                                velocity.GetZ() - bodies.GetLinearVelocity(body_).GetZ());
            // Planned velocity is `velocity`; the force that earns it through her sole contact.
            const Vec3 cur = bodies.GetLinearVelocity(body_);
            const Vec3 want_horiz(velocity.GetX() - cur.GetX(), 0.0F, velocity.GetZ() - cur.GetZ());
            Vec3 want_force = want_horiz * (kMassKg / dt);
            Vec3 slope_slide = Vec3::sZero();
            if (!support_body.IsInvalid() && support_normal.GetY() < 0.98F &&
                bodies.GetMotionType(support_body) != EMotionType::Dynamic) {
                const Vec3 weight = kMassKg * physics_.GetGravity();
                slope_slide = weight - support_normal * weight.Dot(support_normal);
                const float want_vy_slope =
                    -(velocity.GetX() * support_normal.GetX() + velocity.GetZ() * support_normal.GetZ()) /
                    support_normal.GetY();
                const float dvy_slope = want_vy_slope - cur.GetY();
                want_force = Vec3(want_horiz.GetX(), dvy_slope, want_horiz.GetZ()) * (kMassKg / dt) - slope_slide;
            }
            if (support_body.IsInvalid()) {
                bodies.SetLinearVelocity(body_, velocity);
            } else {
                (void)apply_foot_force(want_force, support_body, support_point, slope_slide);
            }
            (void)dv_horiz;
        }
        floor_boarding = boarding;
        // The watchdog: a way ahead of her (more than half a stride off, at a pace).
        // Held on a tick a remedy fires (climbed): firing it makes wants
        // false, and clearing the episode for that would trap the
        // watchdog on rung one (see watch_stall).
        watch_stall(dt, at, length > 0.15F ? dx / length : 0.0F, length > 0.15F ? dz / length : 0.0F,
                    speed > 0.3F && length > 0.5F && !climbed, wx, wz, climbed);
    } else {
        // No floor: she is airborne (a vault's or a clamber's flight, a
        // fall). Hold the stall episode, do not clear it: clearing it
        // here would put every stall after a remedy back on rung one.
        watch_stall(dt, at, 0.0F, 0.0F, false, 0.0F, 0.0F, true);
    }
    if (floor && !floor_boarding) {
        const Vec3 after_v = start_v + foot_drive_dv_;
        speed_ = std::hypot(after_v.GetX(), after_v.GetZ());
    } else {
        speed_ = std::hypot(velocity.GetX(), velocity.GetZ());
    }
    stride_ += speed_ * dt;
}

// The Turn's clock and its two triggers (see matron.hpp). Only simulation
// state decides: her distance, the line, the player's landing, the cooldown.
void Matron::step_turn(const float dt, const float distance_to_player, const float loudness, const float carried_m) {
    sight_far_s_ = sees_far_ ? sight_far_s_ + dt : 0.0F;
    if (turn_ == Turn::None) {
        turn_cooldown_s_ = std::max(0.0F, turn_cooldown_s_ - dt);
        const bool close_and_clear = sight_far_s_ >= kTurnSightSeconds && distance_to_player <= kTurnTriggerM;
        const bool hard_landing = loudness >= kTurnNoiseLoudness && carried_m <= kTurnNoiseM;
        if (turn_cooldown_s_ <= 0.0F && (close_and_clear || hard_landing)) {
            turn_ = Turn::Windup;
            turn_s_ = 0.0F;
        }
        return;
    }
    turn_s_ += dt;
    if (turn_ == Turn::Windup && turn_s_ >= kTurnWindupSeconds) {
        turn_ = Turn::Lunge;
    }
    if (turn_s_ >= kTurnWindupSeconds + kTurnLungeSeconds) {
        turn_ = Turn::None;
        turn_s_ = 0.0F;
        turn_cooldown_s_ = turn_cooldown_seconds();
    }
}

// Her rage (matron.hpp, the stages): fed by this tick's level at her ear,
// and by how much that level jumped (a landing, a crash: one tick's sound
// would otherwise count for nothing), lowered by every quiet second. The
// stage follows it with a gap between its way up and its way down. Her
// exposure: her time out of the locker, alive.
void Matron::step_rage(const float dt) {
    if (state_ == State::Dormant || dead_) {
        heard_level_last_ = 0.0F;
        return;
    }
    const bool out = state_ != State::Straining;
    if (out) {
        exposure_s_ += dt;
    }
    const float spike = std::max(0.0F, heard_level_ - heard_level_last_);
    heard_level_last_ = heard_level_;
    rage_ = std::clamp(rage_ + heard_level_ * kRageGainPerSecond * dt + spike * kRagePerSpike -
                           kRageDecayPerSecond * dt,
                       0.0F, 1.0F);
    switch (stage_) {
    case Stage::Pest:
        if (rage_ >= kRageDangerousUp) {
            stage_ = Stage::Dangerous;
        }
        break;
    case Stage::Dangerous:
        if (rage_ >= kRageChangedUp) {
            stage_ = Stage::Changed;
        } else if (rage_ < kRageDangerousDown) {
            stage_ = Stage::Pest;
        }
        break;
    case Stage::Changed:
        if (rage_ < kRageChangedDown) {
            stage_ = Stage::Dangerous;
        }
        break;
    }
}

float Matron::turn_cooldown_seconds() const noexcept {
    switch (stage_) {
    case Stage::Pest:
        return kTurnCooldownPestSeconds;
    case Stage::Changed:
        return kTurnCooldownChangedSeconds;
    case Stage::Dangerous:
    default:
        return kTurnCooldownSeconds;
    }
}

void Matron::step_planks(const float dt) {
    planks_clock_ += dt;
    BodyInterface &bodies = physics_.GetBodyInterface();
    float peak = 0.0F;
    for (Plank &plank : planks_) {
        if (plank.gone) {
            continue;
        }
        if (plank.bolted && plank.bolt != nullptr) {
            // Only the weight on it counts (vertical): a sprint pushes sideways much harder.
            const float force = std::abs(plank.bolt->GetTotalLambdaPosition().GetY()) / dt;
            plank.load_avg_n += (force - plank.load_avg_n) * std::min(1.0F, dt / kBoltWindowS);
            peak = std::max(peak, plank.load_avg_n);
            if (plank.load_avg_n > kBoltShearN) {
                physics_.RemoveConstraint(plank.bolt);
                plank.bolt = nullptr;
                plank.bolted = false;
            }
        } else if (bodies.GetPosition(plank.body).GetY() < kDeckTop - 70.0F) {
            bodies.RemoveBody(plank.body);
            plank.gone = true;
        }
    }
    plank_peak_n_ = std::max(plank_peak_n_, peak);
}

void Matron::restore_planks() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (std::size_t i = 0; i < planks_.size(); ++i) {
        Plank &plank = planks_[i];
        if (plank.bolted && !plank.gone) {
            plank.load_avg_n = 0.0F;
            continue;
        }
        if (plank.bolt != nullptr) {
            physics_.RemoveConstraint(plank.bolt);
            plank.bolt = nullptr;
        }
        if (plank.gone) {
            bodies.AddBody(plank.body, EActivation::Activate);
            plank.gone = false;
        }
        bodies.SetPositionAndRotation(plank.body, RVec3(plank.home_x, kDeckTop - 0.5F * kPlankThick, kMidZ),
                                      Quat::sIdentity(), EActivation::Activate);
        bodies.SetLinearAndAngularVelocity(plank.body, Vec3::sZero(), Vec3::sZero());
        FixedConstraintSettings bolt;
        bolt.mSpace = EConstraintSpace::WorldSpace;
        bolt.mAutoDetectPoint = true;
        BodyLockWrite lock(physics_.GetBodyLockInterface(), plank.body);
        plank.bolt = static_cast<FixedConstraint *>(bolt.Create(Body::sFixedToWorld, lock.GetBody()));
        physics_.AddConstraint(plank.bolt);
        plank.bolted = true;
        plank.load_avg_n = 0.0F;
    }
}

int Matron::planks_fallen() const noexcept {
    int count = 0;
    for (const Plank &plank : planks_) {
        count += plank.bolted ? 0 : 1;
    }
    return count;
}

void Matron::set_shared_survival(SharedSurvival *encounter) noexcept {
    if (shared_survival_ && shared_survival_ != encounter) shared_survival_->release_hands();
    shared_survival_ = encounter;
}

bool Matron::bracing_structure() const noexcept {
    return shared_survival_ && shared_survival_->snapshot().hands_busy;
}

namespace {
bool in_north_band(const float z) noexcept {
    return z > Bay::kSouthZ + 0.3F && z < Bay::kNorthZ - 0.3F;
}
} // namespace

// Her trap at the bay (matron.hpp, bay.hpp): set when she is on her deck
// west of it and the player across it to the east; then to the edge over
// the bracket, down onto it, to her spot under the leaf's west end, hands
// up (Holding: the shared block before the states holds the leaf); and once
// she lets go, up the rungs onto her deck. True when it moved her.
bool Matron::trap_step(const float dt, const RVec3 at, const RVec3 player, const bool saw, const bool heard,
                       const bool on_deck) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    BodyInterface &bodies = physics_.GetBodyInterface();
    const bool player_past = (saw || heard) && px < Bay::kWestX - 0.2F && in_north_band(pz) && player.GetY() > kDeckTop;
    trap_s_ += dt;
    if (trap_ == Trap::Off) {
        const bool can = level_ == 0 && turn_ == Turn::None && held_ < 0 && traps_ < kTrapsPerRun && bay_open_ &&
                         !bay_hooked_ &&
                         (state_ == State::Guarding || state_ == State::Hunting || state_ == State::Searching);
        const bool she_west = ax < Bay::kWestX - 0.3F && ax > Bay::kWestX - kTrapFromM && in_north_band(az);
        const bool player_east = on_deck && (saw || heard) && px > Bay::kEastX + 0.3F &&
                                 px < Bay::kEastX + kTrapFromM && in_north_band(pz) &&
                                 std::abs(float(player.GetY()) - (kDeckTop + 1.0F)) < 1.5F;
        if (!(can && she_west && player_east)) {
            return false;
        }
        trap_ = Trap::ToEdge;
        trap_s_ = 0.0F;
        ++traps_;
        allow_catwalk_ = false;
    }
    if (trap_ == Trap::ToEdge) {
        // To the deck's edge over the bracket, south of the leaf, where the
        // rail is open; then facing the bay.
        if (level_ != 0 || player_past || trap_s_ > 20.0F) {
            trap_ = Trap::Off;
            return false;
        }
        const float off = distance_xz(ax, az, Bay::kDropX, Bay::kDropZ);
        if (off > 0.2F) {
            float wx = Bay::kDropX;
            float wz = Bay::kDropZ;
            route_toward(ax, az, Bay::kDropX, Bay::kDropZ, false, wx, wz);
            walk_toward(dt, wx, wz, std::min(kSearchMps, 0.6F + 2.0F * off));
            return true;
        }
        walk_toward(dt, ax, az, 0.0F);
        yaw_ = turn_toward(yaw_, 0.5F * kPi, kTurnRate, dt);
        if (std::abs(yaw_ - 0.5F * kPi) < 0.1F) {
            trap_ = Trap::Down;
            trap_s_ = 0.0F;
        }
        return true;
    }
    if (trap_ == Trap::Down) {
        // A step off the edge (her feet past it, not her lines), then straight
        // down onto the bracket 1.78 m below.
        const float feet = float(at.GetY()) - kStandCentre;
        Vec3 velocity = bodies.GetLinearVelocity(body_);
        // Off the edge at 0.8 m/s, gained at her walking accel. Writing 0.8
        // in one tick is a snap the pace exam rejects, and it skips the force law.
        const float want_vx = feet > kDeckTop - 0.6F ? 0.8F : 0.0F;
        const float step = kFootAccel * dt;
        const float vx = velocity.GetX() + std::clamp(want_vx - velocity.GetX(), -step, step);
        velocity = Vec3(vx, velocity.GetY(), 0.0F);
        bodies.SetLinearVelocity(body_, velocity);
        yaw_ = turn_toward(yaw_, 0.5F * kPi, kTurnRate, dt);
        speed_ = std::abs(velocity.GetX());
        stride_ += speed_ * dt;
        if (level_ == bay_level_ && std::abs(velocity.GetY()) < 0.3F && feet < Bay::kBracketTop + 0.2F) {
            trap_ = Trap::ToSpot;
            trap_s_ = 0.0F;
        } else if (trap_s_ > 4.0F || feet < Bay::kBracketTop - 1.0F) {
            // Missed it: whatever she is on now is hers to get off.
            trap_ = Trap::Off;
            return false;
        }
        return true;
    }
    if (trap_ == Trap::ToSpot) {
        // On the bracket, to her spot under the leaf's west end, facing it.
        if (level_ != bay_level_) {
            trap_ = Trap::Off;
            return false;
        }
        if (player_past || trap_s_ > 8.0F) {
            trap_ = Trap::ClimbOut;
            trap_s_ = 0.0F;
            return true;
        }
        const float off = distance_xz(ax, az, Bay::kBraceX, Bay::kBraceZ);
        if (off > 0.3F) {
            walk_toward(dt, Bay::kBraceX, Bay::kBraceZ, std::min(0.9F, 0.3F + 2.0F * off));
            return true;
        }
        // The last of it placed foot by foot (her walk stops steering inside
        // 0.15 m): her hands must come under the leaf, not beside it.
        Vec3 velocity = bodies.GetLinearVelocity(body_);
        const float want_x = std::clamp((Bay::kBraceX - ax) * 4.0F, -0.6F, 0.6F);
        const float want_z = std::clamp((Bay::kBraceZ - az) * 4.0F, -0.6F, 0.6F);
        const float step = kFootAccel * dt;
        const float gx = velocity.GetX() + std::clamp(want_x - velocity.GetX(), -step, step);
        const float gz = velocity.GetZ() + std::clamp(want_z - velocity.GetZ(), -step, step);
        bodies.SetLinearVelocity(body_, Vec3(gx, velocity.GetY(), gz));
        target_x_ = Bay::kBraceX;
        target_z_ = Bay::kBraceZ;
        yaw_ = turn_toward(yaw_, Bay::kBraceYaw, kTurnRate, dt);
        speed_ = std::hypot(gx, gz);
        stride_ += speed_ * dt;
        if (off < 0.05F && std::abs(yaw_ - Bay::kBraceYaw) < 0.03F) {
            trap_ = Trap::Holding;
            trap_s_ = 0.0F;
            trap_lost_s_ = 0.0F;
        }
        return true;
    }
    if (trap_ == Trap::Holding) {
        // Hands up at her spot (the shared block holds the leaf once they
        // meet it). They meet nothing within 2 s: no leaf there to hold.
        walk_toward(dt, ax, az, 0.0F);
        yaw_ = turn_toward(yaw_, Bay::kBraceYaw, kTurnRate, dt);
        if (player_past || (trap_s_ > 2.0F && !bracing_structure())) {
            trap_ = Trap::ClimbOut;
            trap_s_ = 0.0F;
        }
        return true;
    }
    if (trap_ == Trap::ClimbOut) {
        if (climb_way_.status() == MoverStatus::Idle && level_ != bay_level_) {
            // Up and out (or off it some other way): done with it.
            trap_ = Trap::Off;
            return false;
        }
        bay_climb_ = bay_way_out();
        if (bay_climb_ < 0) {
            trap_ = Trap::Off;
            return false;
        }
        const Climb c = climb_spec(bay_climb_);
        const float off = distance_xz(ax, az, c.x, c.z);
        if (off > 0.2F) {
            walk_toward(dt, c.x, c.z, std::min(1.0F, 0.3F + 2.0F * off));
            return true;
        }
        walk_toward(dt, c.x, c.z, 0.0F);
        yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
        climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
        begin_climb(bay_climb_);
        return true;
    }
    return false;
}

// Whether, holding the bay's leaf, she lets it go (matron.hpp).
bool Matron::trap_lets_go(const RVec3 player, const float dt, const bool perceived) {
    // The chain has it: her trap is beaten.
    if (bay_hooked_) {
        return true;
    }
    if (!perceived) {
        trap_lost_s_ += dt;
        return trap_lost_s_ >= kTrapLostSeconds;
    }
    const float px = float(player.GetX());
    const float py = float(player.GetY());
    const float pz = float(player.GetZ());
    // The player down in the bay or under the deck, or past it on hers.
    if (py < kDeckTop - 0.5F || (px < Bay::kWestX - 0.2F && in_north_band(pz))) {
        return true;
    }
    // Lost: far from her, or off her deck, for long enough.
    const bool far = distance_xz(px, pz, Bay::kBraceX, Bay::kBraceZ) > kTrapLostM || py > kDeckTop + 6.0F;
    trap_lost_s_ = far ? trap_lost_s_ + dt : 0.0F;
    return trap_lost_s_ >= kTrapLostSeconds;
}

Mover *Matron::holder() noexcept {
    for (Mover *way : ways_) {
        if (way->status() == MoverStatus::Running) {
            return way;
        }
    }
    return nullptr;
}

const Mover *Matron::holder() const noexcept {
    for (const Mover *way : ways_) {
        if (way->status() == MoverStatus::Running) {
            return way;
        }
    }
    return nullptr;
}

bool Matron::held_in_hunt() const noexcept {
    const Mover *way = holder();
    if (way == nullptr) {
        return false;
    }
    const MoverView v = way->view();
    return v.held && (v.tick == MoverTick::Hunt || v.tick == MoverTick::Rides);
}

bool Matron::take_way(const float dt, const RVec3 at, traversal::Budget &budget) {
    MoverGoal goal{at, heard_x_, heard_z_, lead_floor_, true};
    const auto ask = [&](Mover &way, const bool wait_ok) {
        goal.wait_ok = wait_ok;
        return way.begin(goal) && way.step(dt, budget) != MoverStatus::Idle;
    };
    // After the climber the way he went, where she saw or heard it: down the
    // shaft's pipe, or a line he took.
    if (ask(pipe_way_, true) || (guess_line_ >= 0 && ask(line_way_, false))) {
        return true;
    }
    // Started up a line's ramp, she finishes it: a way found from the ramp
    // would walk her off its side. Off the ramp, or stalled on it, she is not held.
    const bool on_ramp = on_line_ramp(at) && stall_s_ < kRampStallS;
    if (on_ramp && ask(line_way_, true)) {
        return true;
    }
    // Every other way is her own: bars she finds herself, else a way she
    // plans over the steel her scan read (matron_found_climb.cpp), climbed by
    // her own hands. Held to a line's ramp, or in the test of one map way
    // alone, they stand aside.
    if (on_ramp || found_climbs_off_) {
        routecraft_way_.stand_aside();
    } else if (ask(routecraft_way_, true)) {
        return true;
    }
    // The authored map's way alone, for the test that drives one of its
    // ways; in play she has none of it.
    if (found_climbs_off_ && ask(route_way_, true)) {
        return true;
    }
    // A rung way her scan found off her floor (her clock to try a start again
    // runs down here); a line she would only wait at, last.
    climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
    return (!found_climbs_off_ && ask(climb_way_, true)) || ask(line_way_, true);
}

bool Matron::request_climb_goal(const RVec3 goal, const Vec3 facing) {
    if (!body_in_world_ || dead_ || holding_state(state_) || state_ == State::Down ||
        state_ == State::Staggering || held_in_hunt() || riding_ || held_ >= 0 || bracing_structure()) return false;
    MatronRoutecraft candidate = routecraft_.replacement();
    const bool accepted = candidate.request(physics_, body_, goal, facing);
    if (accepted || (!routecraft_.planning() && !routecraft_.owns_motion())) routecraft_ = std::move(candidate);
    if (accepted) {
        routecraft_recovering_ = true;
        routecraft_release_floor_ = float(physics_.GetBodyInterface().GetPosition(body_).GetY()) - kStandCentre;
        chase_way_held_ = true;  // a climb taken holds the chase window past its own ticks
    }
    return accepted;
}

void Matron::pre_step(const float dt, const RVec3 player, const float loudness) {
    clock_s_ += dt;
    acoustic_queries_ = 0;
    foot_force_n_ = 0.0F;
    foot_power_w_ = 0.0F;
    foot_lunge_ = false;
    foot_drive_dv_ = Vec3::sZero();
    // Her tick's one account, handed to the way that has her (matron_mover.hpp).
    const traversal::Limits law{};
    traversal::Budget account{law.total_force_n, 0.0F, law.power_w, 0.0F, Vec3::sZero()};
    if (const Mover *way = holder(); dead_ || !body_in_world_ || holding_state(state_) || state_ == State::Down ||
                                     state_ == State::Staggering || (way != nullptr && way->view().drives)) {
        foot_driving_ = false;
        foot_prep_ = false;
    }
    since_heard_s_ = std::min(since_heard_s_ + dt, 3600.0F);
    // Waking evidence keeps aging while sealed, but she needs an opportunity
    // to investigate it once she can move. Neither clock refreshes a position.
    if (state_ != State::Dormant && state_ != State::Straining && state_ != State::Emerging) {
        pursuit_s_ = std::min(pursuit_s_ + dt, 3600.0F);
    }
    // On a way of her own, the chase window holds whether or not the
    // Hunting block runs this tick: a journey, a travel, and a routecraft
    // move all step past it (early returns below), so a way begun by one
    // of them would otherwise leave the latch unset and the hunt would
    // end the tick the way ends, on arrival itself. The way-start sites
    // set it too; this covers any way whose start they miss.
    if (const Mover *way = holder(); stage_ != Stage::Pest && way != nullptr && way->view().holds_chase) {
        chase_way_held_ = true;
    }
    // On her own journey (travel_to) nothing else has her body: a found
    // climb or a routecraft climb begun beside it gives way to it.
    if (travel_way_.status() == MoverStatus::Running) {
        if (journey_way_.status() == MoverStatus::Running) {
            journey_way_.cancel();
        }
        if (routecraft_way_.status() == MoverStatus::Running) {
            routecraft_way_.cancel();
        }
    }
    // A way she took herself (her journey, her bars) has her body until it
    // ends, first in her tick; knocked about, held or dead, it ends.
    for (Mover *way = holder(); way != nullptr && way->view().tick == MoverTick::Owns; way = holder()) {
        if (dead_ || holding_state(state_) || state_ == State::Down || state_ == State::Staggering) {
            way->cancel();
            continue;
        }
        sound_count_ = 0;
        step_planks(dt);
        feel_floor(dt);
        settle_level();
        (void)way->step(dt, account);
        notice_ridden_line(player);
        sense_near();
        return;
    }
    // This tick's sounds, taken now whatever she does with them.
    const std::size_t sound_count = sound_count_;
    sound_count_ = 0;
    step_planks(dt);
    vault_cooldown_s_ = std::max(0.0F, vault_cooldown_s_ - dt);
    set_s_ = std::max(0.0F, set_s_ - dt);
    shove_cooldown_s_ = std::max(0.0F, shove_cooldown_s_ - dt);
    grab_cooldown_s_ = std::max(0.0F, grab_cooldown_s_ - dt);
    calm_s_ = std::max(0.0F, calm_s_ - dt);
    throw_cooldown_s_ = std::max(0.0F, throw_cooldown_s_ - dt);
    fetch_refused_s_ = std::max(0.0F, fetch_refused_s_ - dt);
    if (fetch_refused_s_ <= 0.0F) {
        fetch_refused_ = 0;
    }
    thrown_s_ = std::max(0.0F, thrown_s_ - dt);
    // What the player handles is theirs to her.
    for (std::size_t i = 0; i < throwables_.size(); ++i) {
        if (player_holding_ != 0 && throwables_[i].entity == player_holding_) {
            handled_ |= 1U << i;
        }
    }
    look_s_ = std::max(0.0F, look_s_ - dt);
    sees_player_ = false;
    sees_far_ = false;
    noise_heard_ = 0.0F;
    heard_level_ = 0.0F;
    if (dead_) {
        if (shared_survival_) shared_survival_->pre_step(BodyID(), false);
        return;
    }
    if (body_in_world_) {
        // On a cable she rides it first.
        if (Mover *way = holder(); way != nullptr && way->view().tick == MoverTick::Rides) {
            (void)way->step(dt, account);
        }
        // On the corner pipe her floor is the steel in her hands.
        if (const Mover *way = holder(); way == nullptr || !way->view().floor_in_hands) {
            feel_floor(dt);
            settle_level();
        }
    }
    // Out in a world no one has read for her, she reads it herself, once:
    // every way she climbs is one her own scan found.
    if (wants_routes()) {
        own_scan_ = std::make_unique<traversal::AffordanceMap>();
        own_scan_->scan(physics_, physics_.GetBounds(), {body_, BodyID(), moving_layer()});
        scanned_ = own_scan_.get();
    }
    // On a journey over the tower's steel (travel_to) she is about nothing else.
    if (Mover *way = holder(); body_in_world_ && way != nullptr && way->view().tick == MoverTick::Travel) {
        notice_ridden_line(player);
        // Knocked off her feet on the way: she gets them back first, then
        // goes on (her journey plans again from where she gets up).
        if (state_ == State::Down || state_ == State::Staggering) {
            if (climber_ && climber_->status() == traversal::Climber::Status::Running) climber_->cancel();
            step_knocked(dt, player);
        } else {
            (void)way->step(dt, account);
        }
        if (shared_survival_) shared_survival_->pre_step(BodyID(), false);
        return;
    }
    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    // On her level (the height of the floor under her, within the tower's
    // footprint) the player can be heard at all: her deck, the ring a cage
    // brought her to, the cage she rides with the player in it.
    const bool in_footprint = std::abs(px) < 27.0F && pz < -123.0F && pz > -177.0F;
    const bool on_deck = player.GetY() > feet_y_ - 1.0 && player.GetY() < feet_y_ + 6.0 && in_footprint;
    // Or up a climb from her level (in its column, off its foot's floor):
    // heard and seen up it, and where it goes is her guess.
    int player_climb = -1;
    const float level_floor = levels_[static_cast<std::size_t>(level_)].floor_y;
    for (int i = 0; i < climb_count(); ++i) {
        Climb c{};
        // Only a ladder has a middle he can be on: by a short lip's window he is standing on its top.
        if (!climb_of(i, c, Ways::Ladders)) {
            continue;
        }
        if (std::abs(c.foot_y - level_floor) < 0.6F && std::abs(px - c.x) < 1.0F && std::abs(pz - c.z) < 1.0F &&
            player.GetY() > c.foot_y + 1.3F && player.GetY() < c.head_y + 1.2F) {
            player_climb = i;
        }
    }
    // Or on the deck her corner pipe is taking her to, or (no longer frail) a
    // deck it joins to hers: a storey of it is a few seconds to her.
    const float player_floor_y = floor_of(float(player.GetY()));
    const Mover *taking = holder();
    const bool by_her_pipe =
        body_in_world_ && in_footprint &&
        ((taking != nullptr && taking->view().has_goal &&
          std::abs(player_floor_y - taking->view().goal_floor_y) < 0.1F) ||
         (stage_ != Stage::Pest && pipe_reaches(level_floor) && pipe_reaches(player_floor_y) &&
          std::abs(std::abs(player_floor_y - level_floor) - kStackPitch) < 0.1F));
    player_on_deck_ = on_deck || player_climb >= 0 || by_her_pipe;
    // The player's way on her level, remembered (matron_moves.cpp).
    if (on_deck && body_in_world_) {
        drop_crumb(px, pz, player_floor_y);
    }
    if (state_ != State::Hunting) {
        hunt_s_ = 0.0F;
        hush_s_ = 0.0F;
    }
    // The loudest at her ear of the tick's sounds that carry to it (her
    // locker while she sleeps, her body awake): not her own, carried within
    // `within` and within its own reach. Her lead goes to the level it is on.
    const RVec3 body_at = body_in_world_ ? physics_.GetBodyInterface().GetPosition(body_) : RVec3::sZero();
    // Just off the corner pipe she stands listening (matron.hpp): every sound
    // carries farther to her.
    const float hearing =
        body_in_world_ && pipe_way_.status() == MoverStatus::Idle && pipe_listen_s_ > 0.0F ? kListenReach : 1.0F;
    float picked_carry = std::numeric_limits<float>::max();
    float picked_level = 0.0F;   // the picked sound's level at her ear, 0..1
    const auto pick = [&](const RVec3 ear, const float within) {
        Sound best{};
        float best_share = 0.0F;
        picked_level = 0.0F;
        for (std::size_t i = 0; i < sound_count; ++i) {
            const Sound &s = sounds_[i];
            const RVec3 at(s.x, s.y, s.z);
            const float reach = s.loudness * kFullHearingM * hearing;
            const float straight = Vec3(at - ear).Length();
            if (straight > within || straight > reach) {
                continue;
            }
            if (body_in_world_ && distance_xz(float(body_at.GetX()), float(body_at.GetZ()), s.x, s.z) < kOwnSoundM &&
                std::abs(s.y - float(body_at.GetY())) < 2.0F) {
                continue;
            }
            const float carried = carry_m(at, ear);
            if (carried > within || carried > reach) {
                continue;
            }
            // Louder at her ear: its reach over the way it carried.
            const float share = reach / std::max(carried, 0.1F);
            if (!best.heard || share > best_share) {
                best = s;
                best_share = share;
                picked_carry = carried;
                picked_level = s.loudness * std::max(0.0F, 1.0F - carried / std::max(reach, 0.1F));
            }
        }
        return best;
    };
    // How far the player's noise carries to her ear this tick (none: 0 loud),
    // and how far it reaches.
    const RVec3 locker_ear(kLockerX, kDeckTop + kLockerHalfY, kLockerZ);
    const RVec3 ear = body_in_world_ ? body_at : locker_ear;
    const float player_reach = loudness * kFullHearingM * hearing;
    const float player_carry = loudness > 0.0F && Vec3(player - ear).Length() <= player_reach
                                   ? carry_m(player, ear)
                                   : std::numeric_limits<float>::max();
    if (shared_survival_) {
        // Her hands go up under the bay's leaf only in her trap, at her spot
        // (an intent; the contact and her footing decide what they hold), and
        // come away when she lets go.
        const Sound support_sound = trap_ == Trap::Holding ? pick(ear, kFullHearingM) : Sound{};
        const bool support_heard = player_reach >= player_carry &&
            (!support_sound.heard || loudness / std::max(player_carry, 0.1F) >=
                                     support_sound.loudness / std::max(picked_carry, 0.1F));
        const bool support_saw = trap_ == Trap::Holding && Vec3(player - ear).Length() <= kSightM &&
            sees(ear + RVec3(0, -0.5, 0), player - RVec3(0, 0.3, 0));
        if (trap_ == Trap::Holding && trap_lets_go(player, dt, support_saw || support_heard)) {
            trap_ = Trap::ClimbOut;
            trap_s_ = 0.0F;
            // The load released; pursue the last evidence, not hidden live coordinates.
            state_ = State::Hunting;
        }
        const bool eligible = trap_ == Trap::Holding && body_in_world_ && out(state_) && state_ != State::Emerging &&
            state_ != State::Down && state_ != State::Staggering && !riding_ && !held_in_hunt() && held_ < 0;
        shared_survival_->pre_step(body_, eligible, Quat::sRotation(Vec3::sAxisY(), yaw_));
        if (bracing_structure()) {
            // She keeps listening, but neither her feet nor hands pursue prey
            // while those hands carry this load. Her hostile state is retained.
            const Sound crash = support_sound;
            noise_heard_ = std::max(crash.loudness, player_reach >= player_carry ? loudness : 0.0F);
            heard_level_ = std::max(crash.heard ? picked_level : 0.0F,
                player_reach >= player_carry ? loudness * std::max(0.0F,
                    1.0F - player_carry / std::max(player_reach, 0.1F)) : 0.0F);
            turn_ = Turn::None;
            step_rage(dt);
            return;
        }
    }
    switch (state_) {
    case State::Dormant: {
        const Sound crash = pick(locker_ear, kWakeNoiseM);
        if (crash.heard) {
            take_lead(crash.x, crash.z, floor_of(crash.y), crash.loudness, false);
            state_ = State::Straining;
            strain_s_ = 0.0F;
        } else if (player_carry <= kWakeNoiseM && loudness * kFullHearingM >= player_carry) {
            take_lead(px, pz, floor_of(float(player.GetY())), loudness, false);
            state_ = State::Straining;
            strain_s_ = 0.0F;
        }
        break;
    }
    case State::Straining:
        strain_s_ += dt;
        if (strain_s_ >= kStrainSeconds) {
            spawn_body();
            state_ = State::Emerging;
            emerge_s_ = 0.0F;
            released_edge_ = true;
            // Keep the sound that woke her, even if its source has moved.
        }
        break;
    case State::Emerging:
        emerge_s_ += dt;
        if (emerge_s_ >= kEmergeSeconds) {
            state_ = State::Hunting;
        }
        break;
    case State::Hunting:
    case State::Searching:
    case State::Shuffling:
    case State::Guarding:
    case State::Peering:
    case State::Unrigging: {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        const float ax = float(at.GetX());
        const float az = float(at.GetZ());
        const float d = distance_xz(ax, az, px, pz);
        // Seen on her level, or up a climb from it (the straight line to the
        // player mostly up it); heard wherever the noise carries to her.
        const bool perceivable = on_deck || player_climb >= 0;
        const float reach = on_deck ? d : std::hypot(d, float(player.GetY() - at.GetY()));
        const bool heard = player_reach >= player_carry;
        bool saw = false;
        if (perceivable && reach <= kSightM) {
            saw = sees(at + RVec3(0.0, -0.5, 0.0), player - RVec3(0.0, 0.3, 0.0));
        }
        sees_player_ = saw;
        // The straight line across the shaft: plain sight at long range.
        // (Low rays: a guard rail is bars, not a wall.)
        sees_far_ = perceivable && reach <= kCommitSightM &&
                    (saw || sees(at + RVec3(0.0, -0.5, 0.0), player - RVec3(0.0, 0.3, 0.0)));
        // A sound louder at her ear than the player draws her to it instead.
        const Sound crash = pick(at, std::numeric_limits<float>::max());
        const bool crash_heard = crash.heard;
        // The level at her ear this tick, for her rage: the louder there of
        // the player and the picked sound.
        heard_level_ = std::max(crash_heard ? picked_level : 0.0F,
                                heard ? loudness * std::max(0.0F, 1.0F - player_carry / std::max(player_reach, 0.1F)) : 0.0F);
        const float player_floor = floor_of(float(player.GetY()));
        bool selected_player_sound = false;
        if (saw) {
            // In sight: no doubt where the player is. His feet are the
            // floor he stands on, not the nearest entry in her hand
            // list: that list does not know scan surfaces (a board, a
            // high deck between its floors), and floor_of falls back
            // to her own floor for those, which snapped a seen player
            // on one onto her floor and reset the chase to a 9 s
            // clock with no way. The player's capsule is 1.8 m, its
            // middle 0.9 up (physics_world.cpp); a crouch drops the
            // middle 0.3 with the soles where they are, so this reads
            // at most 0.3 low for a crouched player, inside every
            // floor tolerance here. A sound's lead keeps floor_of: a
            // noise's height is not a floor she can see.
            take_lead(px, pz, float(player.GetY()) - 0.9F, heard ? loudness : 0.3F, true);
            wary_ = false;
            state_ = State::Hunting;
        } else if (crash_heard || heard) {
            // The louder sound at her ear: a crash, or the player.
            const bool crash_first =
                crash_heard && (!heard || crash.loudness / std::max(picked_carry, 0.1F) >
                                              loudness / std::max(player_carry, 0.1F));
            selected_player_sound = !crash_first;
            const float sx = crash_first ? crash.x : px;
            const float sy = crash_first ? crash.y : float(player.GetY());
            const float sz = crash_first ? crash.z : pz;
            const float sfloor = crash_first ? floor_of(crash.y) : player_floor;
            const float loud = crash_first ? crash.loudness : loudness;
            const bool close = Vec3(RVec3(sx, sy, sz) - at).Length() <= kWaryNoiseM;
            if (state_ == State::Hunting && since_heard_s_ < 1.0F) {
                // The chase goes on by ear: the same lead, moved on.
                take_lead(sx, sz, sfloor, loud, false);
                wary_ = wary_ && !close;
            } else if ((state_ == State::Guarding || state_ == State::Unrigging) && fooled_ >= 2 && !close) {
                // Fooled twice: a far noise only turns her head.
                selected_player_sound = false;
                noise_heard_ = loud;
                look_x_ = sx;
                look_z_ = sz;
                look_s_ = 3.0F;
            } else {
                // A fresh lead: once fooled, checked from a vantage first (on
                // her level: another's she cannot see for its deck, she goes
                // by the way there).
                take_lead(sx, sz, sfloor, loud, false);
                wary_ = fooled_ >= 1 && !close && std::abs(sfloor - levels_[static_cast<std::size_t>(level_)].floor_y) <= 1.0F;
                peer_s_ = 0.0F;
                state_ = State::Hunting;
            }
        }
        // A guess where the player is going: seen or heard riding a line, to
        // its far end.
        if (zipline_ != nullptr && (saw || selected_player_sound) && zipline_->riding()) {
            const int line = zipline_->state().line;
            const RVec3 end = zipline_->lines()[static_cast<std::size_t>(line)].bottom_floor;
            heard_x_ = float(end.GetX());
            heard_z_ = float(end.GetZ());
            lead_floor_ = float(end.GetY());
            guess_line_ = line;
        }
        // Or seen or heard on a climb from her level: where it lets them off
        // over its head. That lead is on another level: not a noise she
        // checks from a vantage (wary), but one she goes after by her way there.
        if (player_climb >= 0 && (saw || selected_player_sound)) {
            const Climb c = climb_spec(player_climb);
            heard_x_ = c.top_x;
            heard_z_ = c.top_z;
            lead_floor_ = c.head_y;
            guess_climb_ = player_climb;
            wary_ = false;
        }
        // What she holds she lets go when it no longer fits what she is doing
        // (matron.hpp, her mischief); else her hands keep it.
        if (held_ >= 0) {
            const bool at_player = saw && Vec3(player - at).Length() < kCatchM + 0.6F;
            if (stage_ != Stage::Pest || turn_ != Turn::None || held_in_hunt() || riding_ ||
                state_ == State::Unrigging || state_ == State::Peering || at_player) {
                drop_stock();
            } else {
                hold_stock(dt);
            }
        }
        if (throw_windup_s_ >= 0.0F && (!saw || held_ < 0 || state_ != State::Hunting)) {
            throw_windup_s_ = -1.0F;
        }
        // On a way the hunt steps (the corner pipe, a climb, her map's way
        // across the tower) she goes on along it, whatever else she is about;
        // started along a way across the tower, she finishes it.
        if (Mover *way = holder(); way != nullptr && way->view().tick == MoverTick::Hunt) {
            const bool turns = way->view().turns;
            (void)way->step(dt, account);
            if (turns) {
                step_turn(dt, d, loudness, player_carry);
            }
            break;
        }
        // Holding on to a floor that moves (or has only just stopped): she
        // keeps her eyes on the player and does nothing else.
        if (riding_) {
            commit_s_ = 0.0F;
            allow_catwalk_ = false;
            walk_toward(dt, ax, az, 0.0F);
            if (saw) {
                yaw_ = turn_toward(yaw_, std::atan2(px - ax, pz - az), kTurnRate, dt);
            }
            break;
        }
        // Off the corner pipe she stands a moment and listens, toward where
        // she last had the player, before she goes on.
        if (pipe_listen_s_ > 0.0F) {
            pipe_listen_s_ -= dt;
            walk_toward(dt, ax, az, 0.0F);
            yaw_ = turn_toward(yaw_, std::atan2(heard_x_ - ax, heard_z_ - az), kTurnRate, dt);
            break;
        }
        // Her trap at the bay (bay.hpp): set, and her way to it and out of it.
        if (trap_step(dt, at, player, saw, selected_player_sound, on_deck)) {
            break;
        }
        // The catwalk (her deck's) is only ever taken in plain sight of prey
        // ahead, or once she is already on it.
        const bool deck = level_ == 0;
        const bool on_catwalk = deck && on_catwalk_line(ax, az, 0.4F);
        const bool prey_at_catwalk =
            deck && std::abs(lead_floor_ - kDeckTop) < 1.0F &&
            (on_catwalk_line(heard_x_, heard_z_, 0.0F) || distance_xz(heard_x_, heard_z_, -kBandX, kMidZ) < kEndM ||
             distance_xz(heard_x_, heard_z_, kBandX, kMidZ) < kEndM);
        // Once she has decided for the catwalk she keeps to it a moment.
        // At the catwalk's end she is off its axis as often as on it, and a rail
        // post can hide the prey on the plates: a sound a moment ago counts as sight there.
        const bool at_end = deck && (distance_xz(ax, az, -kBandX, kMidZ) < kEndM + 3.0F ||
                                     distance_xz(ax, az, kBandX, kMidZ) < kEndM + 3.0F);
        if ((sees_far_ || (at_end && since_heard_s_ < 1.0F)) && prey_at_catwalk) {
            commit_s_ = kCommitHoldSeconds;
        } else {
            commit_s_ = std::max(0.0F, commit_s_ - dt);
        }
        const bool allow = on_catwalk || commit_s_ > 0.0F;
        allow_catwalk_ = allow;
        float wx = ax;
        float wz = az;
        if (state_ == State::Hunting) {
            // Going to look from a vantage is not losing track: she goes on to it.
            const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
            const bool elsewhere = std::abs(lead_floor_ - floor_y) > 1.0F;
            // A lead the corner pipe goes toward, no longer frail: chased longer.
            const float toward = lead_floor_ < floor_y ? floor_y - kStackPitch : floor_y + kStackPitch;
            // So is one her map has a way to, that she is on her way along.
            const bool by_pipe = (elsewhere && stage_ != Stage::Pest && pipe_reaches(floor_y) && pipe_reaches(toward)) ||
                                 route_on_edge_ || (elsewhere && route_.count > 0 && route_goal_ == lead_region());
            // Her own way toward her lead's floor, general (not her map's
            // hand-drawn route, which is test-only now): a found climb she is
            // walking to, a journey or a travel she is on, a climb she is
            // mid-way up, or — between ways, when none of those is yet set —
            // a plan over her scanned map from where she stands to her
            // lead's floor. The scan's own floor, not the hand-listed
            // levels_ table, says where she stands: a bridge, a board or a
            // duct top is a floor to her scan though it is in no levels_
            // entry, and a lead on it is not elsewhere to her. Frail, a pest
            // gets none of this: otherwise the stage rule below would hand
            // her 40 s she has not earned.
            const float feet_y = float(at.GetY()) - kStandCentre;
            float own_floor_y = floor_y;
            int own_region = -1;
            if (elsewhere && scanned_ != nullptr) {
                own_region = scanned_->region_at(Vec3(ax, feet_y, az));
                if (own_region < 0) {
                    own_region = region_under();
                }
                if (own_region >= 0) {
                    own_floor_y = scanned_->regions()[static_cast<std::size_t>(own_region)].top;
                }
            }
            const bool elsewhere_own = std::abs(lead_floor_ - own_floor_y) > 1.0F;
            const auto toward_lead = [&](const float goal_floor) {
                return std::abs(goal_floor - lead_floor_) < std::abs(own_floor_y - lead_floor_) ||
                       std::abs(goal_floor - lead_floor_) <= 1.0F;
            };
            bool by_own_way = false;
            if (stage_ != Stage::Pest && elsewhere_own) {
                // A way of hers that knows the floor it takes her to (a found
                // climb she is walking to, a journey, a travel), toward her
                // lead from the floor it sets out from; or one that has her.
                for (const Mover *way : ways_) {
                    const MoverView v = way->view();
                    if (v.has_goal && (!v.has_from || std::abs(feet_y - v.from_floor_y) <= 1.0F) &&
                        toward_lead(v.goal_floor_y)) {
                        by_own_way = true;
                    }
                }
                if (holder() != nullptr) {
                    by_own_way = true;
                }
                // Between ways, when the chase has already outrun the short
                // window and no way is yet set: a plan over her scan is the
                // way she has, cached by its two ends so the search does not
                // run again while she stands in the same region for the same
                // lead. (At or under the short window the value cannot
                // change the decision, so no search is spent there.)
                if (!by_own_way && !wary_ && pursuit_s_ > kLoseTrackSeconds && scanned_ != nullptr) {
                    int lead_region_scan = scanned_->region_at(Vec3(heard_x_, lead_floor_, heard_z_));
                    if (lead_region_scan < 0) {
                        lead_region_scan = scanned_->region_near_top(lead_floor_, 1.0F);
                    }
                    if (own_region >= 0 && lead_region_scan >= 0 && own_region != lead_region_scan) {
                        if (chase_plan_map_ != scanned_ || chase_plan_from_ != own_region ||
                            chase_plan_to_ != lead_region_scan) {
                            chase_plan_map_ = scanned_;
                            chase_plan_from_ = own_region;
                            chase_plan_to_ = lead_region_scan;
                            chase_plan_ok_ = false;
                            const std::vector<int> plan = scanned_->plan(own_region, lead_region_scan);
                            if (!plan.empty()) {
                                const traversal::Edge &last =
                                    scanned_->edges()[static_cast<std::size_t>(plan.back())];
                                if (last.to >= 0 && last.to < static_cast<int>(scanned_->regions().size())) {
                                    chase_plan_ok_ =
                                        toward_lead(scanned_->regions()[static_cast<std::size_t>(last.to)].top);
                                }
                            }
                        }
                        by_own_way = chase_plan_ok_;
                    }
                }
            }
            const bool by_way = by_pipe || by_own_way;
            // Once a chase has had a way, the window it earned survives the
            // arrival tick: on her lead's floor there is no way left to be
            // on, and without this she would give up at the spot itself.
            // A fresh lead clears it (take_lead), so a new chase earns its
            // own window.
            if (by_way) {
                chase_way_held_ = true;
            }
            const bool by_way_held = by_way || chase_way_held_;
            const float lose_track = !by_way_held                ? kLoseTrackSeconds
                                     : stage_ == Stage::Changed ? kPipeChaseChangedSeconds
                                                                : kPipeChaseDangerousSeconds;
            // A cautious approach walks at search speed and may need longer
            // than the chase timer to reach a vantage, but never forever.
            // By a way of her own, the chase window is the stage window
            // itself, for the pursuit clock and for the age of the sound:
            // capping either at the acoustic memory would cut a changed
            // chase short at 40 s, the very failure this rule fixes. With
            // no way, both caps stay as they were.
            const float evidence_timeout = wary_ ? kAcousticMemorySeconds
                                           : by_way_held ? lose_track
                                                         : (lead_seen_ ? lose_track
                                                                        : std::min(lose_track, kAcousticMemorySeconds));
            const float heard_limit = (by_way_held && !wary_) ? lose_track : kAcousticMemorySeconds;
            if (pursuit_s_ > evidence_timeout || (!lead_seen_ && since_heard_s_ > heard_limit)) {
                if (elsewhere) {
                    // Gone down a line she could not follow: she searches
                    // where she lost them, at its top.
                    if (guess_line_ >= 0 && zipline_ != nullptr) {
                        const RVec3 top = zipline_->lines()[static_cast<std::size_t>(guess_line_)].stand;
                        heard_x_ = float(top.GetX());
                        heard_z_ = float(top.GetZ());
                    }
                    // Gone up a climb she could not take hold of: at its foot
                    // (one she never takes hold of, the riser from its plate
                    // over the shaft: at the place on her own lines by it).
                    if (guess_climb_ >= 0) {
                        const Climb c = climb_spec(guess_climb_);
                        heard_x_ = c.x;
                        heard_z_ = c.z;
                        if (!c.hers) {
                            onto_floor_lines(heard_x_, heard_z_);
                        }
                    }
                    // Heard on another level she has no way to: at the place
                    // on her own lines over or under it (not out over the
                    // shaft, nor on the catwalk).
                    if (guess_line_ < 0 && guess_climb_ < 0) {
                        onto_floor_lines(heard_x_, heard_z_);
                    }
                    lead_floor_ = floor_y;
                    guess_line_ = -1;
                    guess_climb_ = -1;
                }
                turn_ = Turn::None;
                turn_s_ = 0.0F;
                state_ = State::Searching;
                search_s_ = 0.0F;
                search_budget_s_ = 1.0e9F;
                search_trip_s_ = 0.0F;
            } else if (wary_) {
                // Wary: up to where she can see the place, then she looks.
                if (distance_xz(ax, az, heard_x_, heard_z_) <= kVantageM &&
                    sees(at + RVec3(0.0, -0.5, 0.0), RVec3(heard_x_, at.GetY() - 0.5, heard_z_))) {
                    state_ = State::Peering;
                    peer_s_ = 0.0F;
                } else {
                    route_toward(ax, az, heard_x_, heard_z_, allow, wx, wz);
                    walk_toward(dt, wx, wz, kSearchMps);
                    break;
                }
            } else if (elsewhere && take_way(dt, at, account)) {
                // After the lead along the corner pipe (no longer frail), down
                // a line (its stand, its cable), or by her map's ways across
                // the tower (its ladders up and down), or up a climb.
                step_turn(dt, d, loudness, player_carry);
                break;
            } else {
                // A pest's mischief first: what she holds thrown at the player
                // in sight and in reach; else a piece of theirs within her
                // reach gone for, with them still a throw off.
                if (held_ >= 0 && saw && throw_stock(dt, at, player)) {
                    break;
                }
                if ((!saw || Vec3(player - at).Length() > kThrowMinM) && fetch_stock(dt, ax, az, allow)) {
                    break;
                }
                // In the Turn a gap is no bar: she leaps it. A lead on
                // another level she has no way to: the place on her own lines
                // over or under it.
                float tx = heard_x_;
                float tz = heard_z_;
                // A lead standing on a floor she can still step or clamber
                // onto is not another deck. Projecting that spot onto her
                // band strands her under a machine that is leaving (a cage
                // she has not stepped into). Another level, or a floor
                // already above a clamber, stays on the band.
                const float lead_up = lead_floor_ - feet_y;
                const bool step_to_lead = lead_up > -0.3F && lead_up <= kClamberMaxM &&
                                          distance_xz(ax, az, heard_x_, heard_z_) < 8.0F;
                if (elsewhere && !step_to_lead) {
                    onto_floor_lines(tx, tz);
                }
                // The scan's patch path, when the hand-drawn levels graph
                // cannot represent where she heard them: a lead on a
                // surface the levels do not know (a bridge, a board, a
                // duct top), or on a patch off the graph's lines (a cage
                // top, a step). The graph would walk her to the
                // projection of the spot and strand her there; the scan's
                // own walkable patches lead her to the spot itself. When
                // the scan has no patch route, the graph runs as before.
                bool scan_walk = false;
                if (scanned_ != nullptr) {
                    float nearest_level = std::numeric_limits<float>::max();
                    int nearest_idx = -1;
                    for (std::size_t i = 0; i < levels_.size(); ++i) {
                        const float d = std::abs(lead_floor_ - levels_[i].floor_y);
                        if (d < nearest_level) {
                            nearest_level = d;
                            nearest_idx = static_cast<int>(i);
                        }
                    }
                    float nearest_line = std::numeric_limits<float>::max();
                    if (nearest_idx >= 0 && nearest_level <= 1.0F) {
                        const Level &lv = levels_[static_cast<std::size_t>(nearest_idx)];
                        for (const Edge &e : lv.edges) {
                            if (e.a < 0 || e.b < 0 || e.a >= static_cast<int>(lv.nodes.size()) ||
                                e.b >= static_cast<int>(lv.nodes.size())) {
                                continue;
                            }
                            float px = 0.0F, pz = 0.0F, t = 0.0F;
                            project(lv.nodes[static_cast<std::size_t>(e.a)].x, lv.nodes[static_cast<std::size_t>(e.a)].z,
                                    lv.nodes[static_cast<std::size_t>(e.b)].x, lv.nodes[static_cast<std::size_t>(e.b)].z,
                                    heard_x_, heard_z_, px, pz, t);
                            nearest_line = std::min(nearest_line, distance_xz(heard_x_, heard_z_, px, pz));
                        }
                    }
                    const bool off_graph = nearest_level > 1.0F || nearest_line > 1.0F;
                    if (off_graph) {
                        const Vec3 feet(at.GetX(), float(at.GetY()) - kStandCentre, at.GetZ());
                        const Vec3 goal(heard_x_, lead_floor_, heard_z_);
                        const int foot_patch = scanned_->patch_at(feet, 0.4F);
                        const int goal_patch = scanned_->patch_at(goal, 0.4F);
                        if (foot_patch >= 0 && foot_patch == goal_patch) {
                            // The spot is on the very surface under her feet.
                            wx = heard_x_;
                            wz = heard_z_;
                            scan_walk = true;
                        } else {
                            const std::vector<Vec3> path = scanned_->walk_path(feet, goal);
                            if (path.size() > 1) {
                                // First point she is not already standing
                                // on: 1.2 m, as route_toward uses, not
                                // 0.35 m. At 0.35 m a path recomputed
                                // every tick flips between two turn
                                // points straddling her (a corner's
                                // two patch points 0.9 m apart), and
                                // she walks toward one, then the other,
                                // netting zero for the whole run.
                                for (const Vec3 &p : path) {
                                    if (distance_xz(ax, az, p.GetX(), p.GetZ()) > 1.2F) {
                                        wx = p.GetX();
                                        wz = p.GetZ();
                                        scan_walk = true;
                                        break;
                                    }
                                }
                                if (!scan_walk) {
                                    wx = heard_x_;
                                    wz = heard_z_;
                                    scan_walk = true;
                                }
                            }
                        }
                    }
                }
                if (!scan_walk) {
                    route_toward(ax, az, tx, tz, allow, wx, wz, turn_ != Turn::None);
                }
                step_turn(dt, d, loudness, player_carry);
                // Her hands on the player do what her stage has them do (a
                // shove, once in its cooldown; fully changed, a death): her
                // feet stand meanwhile, and do not drive her body on into the
                // player, carrying them off along a rail between shoves.
                const bool hands_on = d < kCatchM && std::abs(float(player.GetY() - at.GetY())) < 1.4F;
                if (turn_ == Turn::Windup || hands_on) {
                    walk_toward(dt, wx, wz, 0.0F, turn_ != Turn::None ? kTurnFootAccel : kFootAccel);
                } else if (turn_ == Turn::Lunge) {
                    const bool burst = std::fmod(turn_s_ - kTurnWindupSeconds, kTurnCycleSeconds) < kTurnBurstSeconds;
                    walk_toward(dt, wx, wz, burst ? kTurnBurstMps : kTurnPauseMps, kTurnFootAccel, true);
                } else if (hush_step(dt, saw, d, loudness)) {
                    // The player gone quiet: so does she. Still, listening,
                    // turned to where she had them (matron.hpp kHushAfterS).
                    walk_toward(dt, ax, az, 0.0F);
                    yaw_ = turn_toward(yaw_, std::atan2(heard_x_ - ax, heard_z_ - az), kTurnRate, dt);
                } else {
                    // Her pace: creep, stalk, stride or run, as she chooses.
                    walk_toward(dt, wx, wz, pace_mps(choose_pace(dt, d, saw, heard, loudness)));
                }
                break;
            }
        }
        if (state_ == State::Peering) {
            // Still, looking at the place the noise came from.
            peer_s_ += dt;
            walk_toward(dt, ax, az, 0.0F);
            yaw_ = turn_toward(yaw_, std::atan2(heard_x_ - ax, heard_z_ - az), kTurnRate, dt);
            if (peer_s_ >= kPeerSeconds) {
                ++fooled_;   // nobody there: another noise that lied
                stand_down();
            }
            break;
        }
        if (state_ == State::Searching) {
            // Her look about the place takes kSearchSeconds once she is there;
            // her way there what it takes, within its budget (matron.hpp).
            const float to_place = distance_xz(ax, az, heard_x_, heard_z_);
            if (search_budget_s_ > 1.0e8F) {
                search_budget_s_ = kSearchTripSeconds + 2.0F * to_place / kSearchMps;
            }
            if (to_place <= 1.5F) {
                search_s_ += dt;
            } else {
                search_trip_s_ += dt;
            }
            if (search_s_ >= kSearchSeconds || search_trip_s_ >= search_budget_s_ ||
                (!lead_seen_ && since_heard_s_ > kAcousticMemorySeconds)) {
                if (!lead_seen_) ++fooled_;
                stand_down();
                break;
            }
            if (to_place > 1.5F) {
                // Where she meets the player's trail she walks it, the way
                // they went (matron_moves.cpp crumb_ahead); else to the place.
                float cx = 0.0F;
                float cz = 0.0F;
                if (crumb_ahead(ax, az, cx, cz)) {
                    route_toward(ax, az, cx, cz, on_catwalk, wx, wz);
                } else {
                    route_toward(ax, az, heard_x_, heard_z_, on_catwalk, wx, wz);
                }
                walk_toward(dt, wx, wz, kSearchMps);
            } else {
                walk_toward(dt, ax, az, 0.0F);
            }
            break;
        }
        const Level &here = levels_[static_cast<std::size_t>(level_)];
        const bool has_way = here.way >= 0;
        const std::size_t way = has_way ? static_cast<std::size_t>(here.way) : 0;
        const Post *guarded = state_ == State::Guarding ? reachable_post(ax, az) : nullptr;
        const bool at_post = guarded != nullptr && distance_xz(ax, az, guarded->x, guarded->z) <= 0.6F;
        if (at_post && has_way && rigged_[way] && ways_out_[way] &&
            route_toward(ax, az, here.eye.x, here.eye.z, on_catwalk, wx, wz)) {
            // The way out rigged to ride, and the player not here: from her
            // post in its mouth (the eye is inside; from anywhere else the
            // straight way to it can run into the cage's back) she goes to
            // undo it.
            state_ = State::Unrigging;
            unrig_s_ = 0.0F;
        }
        if (state_ == State::Unrigging) {
            if (!has_way || !rigged_[way] || !ways_out_[way] ||
                !route_toward(ax, az, here.eye.x, here.eye.z, on_catwalk, wx, wz)) {
                state_ = State::Guarding;
            } else if (distance_xz(ax, az, here.eye.x, here.eye.z) > 0.35F) {
                walk_toward(dt, wx, wz, kSearchMps);
                break;
            } else {
                // Her hands at the eye, at the shackle, till it is off.
                walk_toward(dt, ax, az, 0.0F);
                yaw_ = turn_toward(yaw_, here.eye.yaw, kTurnRate, dt);
                unrig_s_ += dt;
                if (unrig_s_ >= kUnrigSeconds) {
                    unrig_way_ = here.way;
                    ++unrigs_;
                    state_ = State::Guarding;
                }
                break;
            }
        }
        if (state_ == State::Guarding) {
            const Post *post = reachable_post(ax, az);
            if (post == nullptr) {
                state_ = State::Shuffling;
                pick_patrol(ax, az);
            } else {
                if (distance_xz(ax, az, post->x, post->z) > 0.5F) {
                    route_toward(ax, az, post->x, post->z, on_catwalk, wx, wz);
                    walk_toward(dt, wx, wz, kSearchMps);
                } else {
                    // In the mouth of the way out, still and silent: facing out
                    // from it, or toward a far noise she let go. What she
                    // carried off she drops here.
                    walk_toward(dt, ax, az, 0.0F);
                    if (held_ >= 0) {
                        drop_stock();
                    }
                    const float want = look_s_ > 0.0F ? std::atan2(look_x_ - ax, look_z_ - az) : post->yaw;
                    yaw_ = turn_toward(yaw_, want, 2.0F, dt);
                }
                break;
            }
        }
        // Shuffling: the stooped patrol round her level's loop.
        const std::vector<int> &loop = here.patrols[static_cast<std::size_t>(patrol_)];
        const Node &next = here.nodes[static_cast<std::size_t>(loop[static_cast<std::size_t>(patrol_node_)])];
        if (distance_xz(ax, az, next.x, next.z) < 1.5F) {
            patrol_node_ = (patrol_node_ + 1) % static_cast<int>(loop.size());
        }
        const Node &goal = here.nodes[static_cast<std::size_t>(loop[static_cast<std::size_t>(patrol_node_)])];
        route_toward(ax, az, goal.x, goal.z, on_catwalk, wx, wz);
        walk_toward(dt, wx, wz, kShuffleMps);
        break;
    }
    case State::Grab:
    case State::Carry:
    case State::Dangle:
    case State::ArmPlay:
    case State::Release:
    case State::Spare:
    case State::Claimed:
        // Her hold (matron_hold.cpp): her eyes and ears on what is in her hand,
        // and her hands on it: a lead she has, seen, where it is.
        take_lead(float(player.GetX()), float(player.GetZ()), floor_of(float(player.GetY())), noise_heard_, true);
        step_hold(dt, player);
        break;
    case State::Staggering:
    case State::Down:
        // Knocked about: no eyes on the player, and every sound runs on
        // unheeded while she gets her feet (and her head) back.
        step_knocked(dt, player);
        break;
    case State::Dead:
        break;
    }
    // Spared: she walks off and does not hunt for a while.
    if (calm_s_ > 0.0F && (state_ == State::Hunting || state_ == State::Searching || state_ == State::Peering)) {
        state_ = State::Shuffling;
        turn_ = Turn::None;
        if (body_in_world_) {
            const RVec3 here = physics_.GetBodyInterface().GetPosition(body_);
            pick_patrol(float(here.GetX()), float(here.GetZ()));
        }
    }
    step_rage(dt);
    sense_near();
}

void Matron::post_step(const float dt) {
    take_blow(dt);
    // Her bars let go of her at once (her journey ends in her next tick).
    if (dead_ || state_ == State::Down || state_ == State::Staggering || holding_state(state_)) routecraft_way_.cancel();
    const Mover *held_by = holder();
    if (body_in_world_ && !dead_ && !(held_by != nullptr && held_by->view().held) && !riding_ &&
        routecraft_recovering_) {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        {
            RayCastResult floor;
            const RRayCast down(at, Vec3(0, -(kStandCentre + 0.08F), 0));
            if (physics_.GetNarrowPhaseQuery().CastRay(down, floor, {}, {}, IgnoreSingleBodyFilter(body_))) {
                bool walkable = false;
                {
                    const BodyLockRead lock(physics_.GetBodyLockInterface(), floor.mBodyID);
                    walkable = lock.Succeeded() && lock.GetBody().GetWorldSpaceSurfaceNormal(
                        floor.mSubShapeID2, down.GetPointOnRay(floor.mFraction)).GetY() > 0.7F;
                }
                if (walkable) {
                    feet_y_ = float(down.GetPointOnRay(floor.mFraction).GetY());
                    if (std::abs(float(at.GetY()) - kStandCentre - feet_y_) < 0.025F &&
                        std::abs(physics_.GetBodyInterface().GetLinearVelocity(body_).GetY()) < 0.3F) {
                        routecraft_recovering_ = false;
                    }
                }
            }
            settle_level();
        }
    }
    // A fall ends where she lands, and the landing decides it: a stop from
    // kLethalLandingMps or more kills her, on a deck, on the ground or on a
    // plate that came down with her. No deck height counts against her on the
    // way (one she passes is no floor under her).
    if (const Mover *way = holder(); body_in_world_ && !dead_ && !(way != nullptr && way->view().lands)) {
        BodyInterface &bodies = physics_.GetBodyInterface();
        const float vy = bodies.GetLinearVelocity(body_).GetY();
        const float falling = -fall_vy_;
        const bool landed_hard = falling >= kLethalLandingMps && vy - fall_vy_ > 0.5F * falling;
        if (landed_hard || bodies.GetPosition(body_).GetY() < kOffTheWorldY) {
            drop_stock();
            dead_ = true;
            state_ = State::Dead;
            remove_body();
        } else if (falling >= kHardLandingMps && vy - fall_vy_ > 0.5F * falling && state_ != State::Down &&
                   state_ != State::Staggering) {
            // Down hard, alive: down a while, the way she was going.
            const Vec3 v = bodies.GetLinearVelocity(body_);
            const Vec3 way(v.GetX(), 0.0F, v.GetZ());
            fall_x_ = way.LengthSq() > 1.0e-4F ? way.Normalized().GetX() : std::sin(yaw_);
            fall_z_ = way.LengthSq() > 1.0e-4F ? way.Normalized().GetZ() : std::cos(yaw_);
            knock_down();
        }
    }
    fall_vy_ = body_in_world_ && !dead_ ? physics_.GetBodyInterface().GetLinearVelocity(body_).GetY() : 0.0F;
    refresh_view();
}

bool Matron::caught(const RVec3 player) const noexcept {
    // Clinging to a moving floor, her hands are on the rail; on the corner
    // pipe, on the steel; on a ladder, on its rungs.
    const Mover *way = holder();
    if (bracing_structure() || !body_in_world_ || dead_ || riding_ || (way != nullptr && way->view().held) ||
        (state_ != State::Hunting && state_ != State::Searching && state_ != State::Shuffling &&
         state_ != State::Guarding && state_ != State::Peering && state_ != State::Unrigging)) {
        return false;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    return distance_xz(float(at.GetX()), float(at.GetZ()), float(player.GetX()), float(player.GetZ())) < kCatchM &&
           std::abs(at.GetY() - player.GetY()) < 1.4;
}

Vec3 Matron::shove_direction(const RVec3 player) const noexcept {
    const RVec3 at = body_in_world_ ? physics_.GetBodyInterface().GetPosition(body_) : player;
    Vec3 away(float(player.GetX() - at.GetX()), 0.0F, float(player.GetZ() - at.GetZ()));
    if (away.LengthSq() < 1.0e-4F) {
        away = Vec3(std::sin(yaw_), 0.0F, std::cos(yaw_));
    }
    away = away.Normalized();
    if (stage_ != Stage::Dangerous) {
        return away;
    }
    // Dangerous: at the shaft's rim nearest the player (the point of its box
    // nearest them), when it is within reach; over the shaft already (the
    // catwalk), or far from its rim, away from her as a pest would.
    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    const Vec3 to_rim(std::clamp(px, -kShaftHalfM, kShaftHalfM) - px, 0.0F,
                      std::clamp(pz, kMidZ - kShaftHalfM, kMidZ + kShaftHalfM) - pz);
    const float reach = to_rim.Length();
    if (reach < 0.05F || reach > kShoveEdgeReachM) {
        return away;
    }
    return to_rim / reach;
}

void Matron::set_throwables(std::vector<Throwable> throwables) {
    throwables_ = std::move(throwables);
    if (throwables_.size() > 32) {
        throwables_.resize(32);
    }
    for (Throwable &t : throwables_) {
        const BodyLockRead lock(physics_.GetBodyLockInterface(), t.body);
        const float inverse_mass =
            lock.Succeeded() && lock.GetBody().IsDynamic() ? lock.GetBody().GetMotionProperties()->GetInverseMass() : 0.0F;
        t.mass_kg = inverse_mass > 0.0F ? 1.0F / inverse_mass : std::numeric_limits<float>::infinity();
    }
}

Vec3 Matron::hands_offset() const noexcept {
    return Vec3(std::sin(yaw_) * kHandsOutM, kHandsUpM, std::cos(yaw_) * kHandsOutM);
}

int Matron::stock_to_fetch(const float ax, const float az) const {
    const BodyInterface &bodies = physics_.GetBodyInterface();
    int best = -1;
    float best_m = kSeekStockM;
    for (std::size_t i = 0; i < throwables_.size(); ++i) {
        const Throwable &t = throwables_[i];
        if ((handled_ & (1U << i)) == 0U || (fetch_refused_ & (1U << i)) != 0U || t.entity == player_holding_ ||
            t.mass_kg > kTakeMaxKg) {
            continue;
        }
        const RVec3 p = bodies.GetPosition(t.body);
        // At rest on her floor (a piece's centre stands well under a metre over it).
        if (std::abs(float(p.GetY()) - feet_y_) > 1.0F || bodies.GetLinearVelocity(t.body).Length() > 0.3F) {
            continue;
        }
        const float m = distance_xz(ax, az, float(p.GetX()), float(p.GetZ()));
        if (m < best_m) {
            best = static_cast<int>(i);
            best_m = m;
        }
    }
    return best;
}

bool Matron::fetch_stock(const float dt, const float ax, const float az, const bool allow_catwalk) {
    if (held_ >= 0 || stage_ != Stage::Pest || turn_ != Turn::None || wary_ || throw_cooldown_s_ > 0.0F) {
        fetch_ = -1;
        return false;
    }
    if (fetch_ < 0) {
        fetch_ = stock_to_fetch(ax, az);
        fetch_best_m_ = std::numeric_limits<float>::max();
        fetch_stall_s_ = 0.0F;
    }
    if (fetch_ < 0) {
        return false;
    }
    const BodyInterface &bodies = physics_.GetBodyInterface();
    const Throwable &t = throwables_[static_cast<std::size_t>(fetch_)];
    const RVec3 p = bodies.GetPosition(t.body);
    const float px = float(p.GetX());
    const float pz = float(p.GetZ());
    if (t.entity == player_holding_ || distance_xz(ax, az, px, pz) > kSeekStockM + 1.0F ||
        bodies.GetLinearVelocity(t.body).Length() > 0.5F) {
        fetch_ = -1;
        return false;
    }
    // Getting no nearer to it, she gives it up for a while.
    const float m = distance_xz(ax, az, px, pz);
    if (m < fetch_best_m_ - 0.1F) {
        fetch_best_m_ = m;
        fetch_stall_s_ = 0.0F;
    } else {
        fetch_stall_s_ += dt;
    }
    if (fetch_stall_s_ >= kFetchStallSeconds) {
        fetch_refused_ |= 1U << static_cast<unsigned>(fetch_);
        fetch_refused_s_ = kFetchRetrySeconds;
        fetch_ = -1;
        return false;
    }
    if (distance_xz(ax, az, px, pz) <= kTakeM) {
        walk_toward(dt, ax, az, 0.0F);
        yaw_ = turn_toward(yaw_, std::atan2(px - ax, pz - az), kTurnRate, dt);
        take_stock(fetch_);
        fetch_ = -1;
        return true;
    }
    float wx = ax;
    float wz = az;
    if (!route_toward(ax, az, px, pz, allow_catwalk, wx, wz)) {
        fetch_ = -1;
        return false;
    }
    walk_toward(dt, wx, wz, kHuntMps);
    return true;
}

void Matron::take_stock(const int index) {
    if (hold_ != nullptr || !body_in_world_ || index < 0) {
        return;
    }
    const Throwable &t = throwables_[static_cast<std::size_t>(index)];
    {
        const BodyID ids[2] = {body_, t.body};
        BodyLockMultiWrite lock(physics_.GetBodyLockInterfaceNoLock(), ids, 2);
        Body *her = lock.GetBody(0);
        Body *it = lock.GetBody(1);
        if (her == nullptr || it == nullptr || !it->IsDynamic()) {
            return;
        }
        // The hands close on the handle where it is; hold_stock draws it in.
        const RVec3 handle = it->GetWorldTransform() * t.handle;
        hold_offset_ = Vec3(handle - her->GetCenterOfMassPosition());
        PointConstraintSettings settings;
        settings.mSpace = EConstraintSpace::LocalToBodyCOM;
        settings.mPoint1 = RVec3(her->GetRotation().Conjugated() * hold_offset_);
        settings.mPoint2 = it->GetCenterOfMassTransform().Inversed() * handle;
        hold_ = static_cast<PointConstraint *>(settings.Create(*her, *it));
    }
    physics_.AddConstraint(hold_);
    physics_.GetBodyInterface().ActivateBody(t.body);
    held_ = index;
    ++takes_;
}

void Matron::hold_stock(const float dt) {
    if (hold_ == nullptr || held_ < 0) {
        return;
    }
    // The hands draw what they took in to a point ahead of her chest.
    const Vec3 want = hands_offset();
    const Vec3 left = want - hold_offset_;
    const float step = kHandsPullMps * dt;
    hold_offset_ = left.Length() > step ? hold_offset_ + left.Normalized() * step : want;
    BodyInterface &bodies = physics_.GetBodyInterface();
    hold_->SetPoint1(EConstraintSpace::LocalToBodyCOM, RVec3(bodies.GetRotation(body_).Conjugated() * hold_offset_));
    // Wedged and dragged off the hands: it slips out of them.
    const Throwable &t = throwables_[static_cast<std::size_t>(held_)];
    const RVec3 handle = bodies.GetWorldTransform(t.body) * t.handle;
    const RVec3 hands = bodies.GetCenterOfMassPosition(body_) + hold_offset_;
    if (Vec3(handle - hands).Length() > kHandsSlipM) {
        drop_stock();
    }
}

bool Matron::throw_stock(const float dt, const RVec3 at, const RVec3 player) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    // Thrown at the player's chest as they are when it leaves her hands, at
    // kThrowMps, on the lower of the two arcs that reach them.
    const auto velocity = [&](Vec3 &v) {
        const BodyInterface &bodies = physics_.GetBodyInterface();
        const RVec3 from = bodies.GetCenterOfMassPosition(body_) + hands_offset();
        const Vec3 run(player + RVec3(0.0, 0.1, 0.0) - from);
        const float across = std::hypot(run.GetX(), run.GetZ());
        if (across < 0.1F) {
            return false;
        }
        const float g = physics_.GetGravity().Length();
        const float v2 = kThrowMps * kThrowMps;
        const float disc = v2 * v2 - g * (g * across * across + 2.0F * run.GetY() * v2);
        if (disc < 0.0F) {
            return false;
        }
        const float angle = std::atan((v2 - std::sqrt(disc)) / (g * across));
        const Vec3 flat(run.GetX() / across, 0.0F, run.GetZ() / across);
        v = flat * (kThrowMps * std::cos(angle)) + Vec3(0.0F, kThrowMps * std::sin(angle), 0.0F);
        return true;
    };
    const float across = distance_xz(ax, az, px, pz);
    if (throw_windup_s_ < 0.0F) {
        Vec3 v;
        if (throw_cooldown_s_ > 0.0F || across < kThrowMinM || across > kThrowMaxM || !velocity(v)) {
            return false;
        }
        throw_windup_s_ = 0.0F;
    }
    // Rearing back, still, turning to them.
    throw_windup_s_ += dt;
    walk_toward(dt, ax, az, 0.0F);
    yaw_ = turn_toward(yaw_, std::atan2(px - ax, pz - az), kTurnRate, dt);
    if (throw_windup_s_ < kThrowWindupSeconds) {
        return true;
    }
    throw_windup_s_ = -1.0F;
    Vec3 v;
    if (velocity(v)) {
        // Turning over end on end as it goes, about her right-hand side.
        const Vec3 spin(std::cos(yaw_) * 4.0F, 0.0F, -std::sin(yaw_) * 4.0F);
        release_stock(v, spin, true);
    }
    return true;
}

void Matron::release_stock(const Vec3 velocity, const Vec3 spin, const bool thrown) {
    const int index = held_;
    held_ = -1;
    if (hold_ != nullptr) {
        physics_.RemoveConstraint(hold_);
        hold_ = nullptr;
    }
    if (index < 0) {
        return;
    }
    const Throwable &t = throwables_[static_cast<std::size_t>(index)];
    BodyInterface &bodies = physics_.GetBodyInterface();
    bodies.ActivateBody(t.body);
    if (thrown) {
        bodies.SetLinearAndAngularVelocity(t.body, velocity, spin);
        thrown_ = index;
        thrown_s_ = kThrowQuietSeconds;
        throw_cooldown_s_ = kThrowCooldownSeconds;
        ++throws_;
    }
}

void Matron::drop_stock() {
    fetch_ = -1;
    throw_windup_s_ = -1.0F;
    release_stock(Vec3::sZero(), Vec3::sZero(), false);
}

void Matron::set_stage_for_test(const Stage stage) noexcept {
    stage_ = stage;
    switch (stage) {
    case Stage::Pest:
        rage_ = 0.0F;
        break;
    case Stage::Dangerous:
        rage_ = kRageDangerousUp + 0.05F;
        break;
    case Stage::Changed:
        rage_ = kRageChangedUp + 0.05F;
        break;
    }
    refresh_view();
}

void Matron::restore(const Saved &saved) {
    routecraft_.cancel();
    routecraft_recovering_ = false;
    routecraft_mine_ = false;
    found_ = FoundClimb{};
    climb_blocks_ = {};
    find_wait_ticks_ = 0;
    end_journey(false);
    journey_wait_ticks_ = 0;
    if (shared_survival_) shared_survival_->reset();
    if (!dead_) {
        remove_body();
        state_ = saved.state;
        strain_s_ = saved.strain_s;
        rage_ = saved.rage;
        stage_ = saved.stage;
        exposure_s_ = saved.exposure_s;
        heard_level_ = 0.0F;
        heard_level_last_ = 0.0F;
        shove_cooldown_s_ = 0.0F;
        shoves_ = 0;
        drop_stock();
        handled_ = saved.handled;
        throw_cooldown_s_ = 0.0F;
        fetch_ = -1;
        fetch_refused_ = 0;
        fetch_refused_s_ = 0.0F;
        thrown_ = -1;
        thrown_s_ = 0.0F;
        takes_ = 0;
        throws_ = 0;
        speed_ = 0.0F;
        since_heard_s_ = 0.0F;
        pursuit_s_ = 0.0F;
        turn_ = Turn::None;
        turn_s_ = 0.0F;
        turn_cooldown_s_ = kTurnCooldownSeconds * 0.5F;
        sight_far_s_ = 0.0F;
        // A new run: what she learned of the player's tricks is forgotten.
        fooled_ = 0;
        trap_ = Trap::Off;
        trap_s_ = 0.0F;
        trap_lost_s_ = 0.0F;
        traps_ = 0;
        wary_ = false;
        lead_seen_ = false;
        lead_valid_ = false;
        peer_s_ = 0.0F;
        look_s_ = 0.0F;
        vault_cooldown_s_ = 0.0F;
        vaults_ = 0;
        // matron_moves.cpp: the same rolls again from a restore.
        rng_ = 0x9E3779B9U;
        pace_ = Pace::Stalk;
        pace_hold_s_ = 0.0F;
        pace_saw_ = false;
        hunt_s_ = 0.0F;
        pace_changes_ = 0;
        stall_window_s_ = 0.0F;
        stall_rung_ = 0;
        stall_s_ = 0.0F;
        longest_stall_s_ = 0.0F;
        stalls_ = step_ups_ = clambers_ = sidesteps_ = reroutes_ = 0;
        clamber_now_ = false;
        sidestep_s_ = 0.0F;
        blocked_.clear();
        crumb_count_ = 0;
        patrol_only_ = false;
        hold_s_ = 0.0F;
        grab_cooldown_s_ = 0.0F;
        calm_s_ = 0.0F;
        grip_on_ = false;
        grip_was_on_ = false;
        player_braced_ = false;
        thrash_ = 0.0F;
        thrash_failed_ = false;
        let_go_ = false;
        hold_phase_ = 0;
        pull_n_ = kHandReachPullN;
        arm_reach_m_ = kArmReachM;
        wrist_on_ = false;
        arm_throw_ = false;
        pull2_n_ = kHandReachPullN;
        crumb_head_ = 0;
        crumb_on_ = -1;
        crumbs_walked_ = 0;
        quiet_s_ = 0.0F;
        hush_s_ = 0.0F;
        hush_spent_ = false;
        creaks_ = 0;
        leaps_ = 0;
        unrigs_ = 0;
        guess_line_ = -1;
        lead_floor_ = kDeckTop;
        restore_line();
        guess_climb_ = -1;
        release_climb(false);
        climb_retry_s_ = 0.0F;
        climb_grips_ = 0;
        climb_releases_ = 0;
        climbs_done_ = 0;
        // A restore is a world changed: every way of her map is worth trying again.
        route_clear();
        routes_.unblock_all();
        route_edges_done_ = 0;
        route_failures_ = 0;
        physics_.GetBodyInterface().SetGravityFactor(body_, 1.0F);
        unrig_s_ = 0.0F;
        unrig_way_ = -1;
        // Back on her deck, off whatever she rode.
        level_ = 0;
        patrol_ = 0;
        patrol_node_ = 2;
        feet_y_ = kDeckTop;
        riding_ = false;
        let_go_s_ = 0.0F;
        restore_pipe();
        foot_driving_ = false;
        foot_prep_ = false;
        foot_body_ = BodyID();
        foot_force_n_ = 0.0F;
        foot_power_w_ = 0.0F;
        foot_lunge_ = false;
        foot_drive_dv_ = Vec3::sZero();
        // On her feet, whatever had her down.
        if (lying_) {
            BodyInterface &bodies = physics_.GetBodyInterface();
            bodies.SetShape(body_, shape_, false, EActivation::DontActivate);
            bodies.SetFriction(body_, 0.0F);
        }
        lying_ = false;
        kneeling_ = false;
        down_shape_ = nullptr;
        stagger_s_ = 0.0F;
        down_s_ = 0.0F;
        down_for_s_ = 0.0F;
        set_s_ = 0.0F;
        staggers_ = 0;
        knockdowns_ = 0;
        blow_mps_ = 0.0F;
        blow_change_ = Vec3::sZero();
        blow_impulse_ = Vec3::sZero();
        blow_above_ = 0.0F;
        blow_quiet_ = kBlowQuietTicks;
        near_count_ = 0;
        sensed_ = false;
        if (out(state_)) {
            // Back at her locker, as she burst out of it.
            spawn_body();
            state_ = State::Emerging;
            emerge_s_ = 0.4F;
        }
        restore_planks();
    }
    refresh_view();
}

float Matron::step_reach() const noexcept {
    switch (stage_) {
    case Stage::Dangerous:
        return kDangerousStepM;
    case Stage::Changed:
        return kChangedStepM;
    case Stage::Pest:
    default:
        return kPestStepM;
    }
}

float Matron::recover_accel() const noexcept {
    switch (stage_) {
    case Stage::Dangerous:
        return kDangerousRecoverAccel;
    case Stage::Changed:
        return kChangedRecoverAccel;
    case Stage::Pest:
    default:
        return kPestRecoverAccel;
    }
}

float Matron::down_seconds() const noexcept {
    switch (stage_) {
    case Stage::Dangerous:
        return kDangerousDownSeconds;
    case Stage::Changed:
        return kChangedDownSeconds;
    case Stage::Pest:
    default:
        return kPestDownSeconds;
    }
}

// After she has moved (pre_step's end): what her feet set, and the moving
// bodies near her with their motion, so the step's blow can be read off both.
void Matron::sense_near() {
    sensed_ = false;
    near_count_ = 0;
    if (!body_in_world_ || dead_) {
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    // Her own drive (feet or pipe) is a force the step applies, not a velocity
    // set: what it will change is hers, not a blow's.
    const Mover *way = holder();
    set_velocity_ = bodies.GetLinearVelocity(body_) + foot_drive_dv_ +
                    (way != nullptr ? way->view().drive_dv : Vec3::sZero());
    sensed_ = true;
    const Vec3 centre(at);
    AllHitCollisionCollector<CollideShapeBodyCollector> found;
    physics_.GetBroadPhaseQuery().CollideAABox(
        AABox(centre - Vec3::sReplicate(kNearM), centre + Vec3::sReplicate(kNearM)), found);
    const BodyID held = held_body();
    const ObjectLayer hers = bodies.GetObjectLayer(body_);
    struct Candidate {
        Near near;
        float distance;
        std::uint32_t id;
    };
    std::array<Candidate, 32> candidates{};
    std::size_t count = 0;
    for (const BodyID id : found.mHits) {
        if (id == body_ || id == player_ || id == held || count == candidates.size()) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), id);
        if (!lock.Succeeded()) {
            continue;
        }
        const Body &body = lock.GetBody();
        const std::uint64_t entity = body.GetUserData();
        // Never what cannot meet her (a corpse's parts), nor the load in the
        // player's hands: carried into her, it is the player's own push.
        if (!body.IsDynamic() || body.IsSensor() ||
            (entity >= kPlankEntityBase && entity < kPlankEntityBase + static_cast<std::uint64_t>(kPlanks)) ||
            (player_holding_ != 0 && entity == player_holding_) ||
            !physics_.GetObjectLayerPairFilter().ShouldCollide(hers, body.GetObjectLayer())) {
            continue;
        }
        const float inverse_mass = body.GetMotionProperties()->GetInverseMass();
        if (inverse_mass <= 0.0F) {
            continue;
        }
        Candidate &c = candidates[count++];
        c.near.body = id;
        c.near.velocity = body.GetLinearVelocity();
        c.near.at = body.GetCenterOfMassPosition();
        c.near.mass_kg = 1.0F / inverse_mass;
        c.near.radius = body.GetWorldSpaceBounds().GetExtent().Length();
        c.distance = Vec3(c.near.at - at).Length();
        c.id = id.GetIndexAndSequenceNumber();
    }
    // The nearest; among equals, by id (the broad phase's order is its own).
    std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(count),
              [](const Candidate &a, const Candidate &b) {
                  return a.distance != b.distance ? a.distance < b.distance : a.id < b.id;
              });
    near_count_ = std::min(count, kMaxNear);
    for (std::size_t i = 0; i < near_count_; ++i) {
        near_[i] = candidates[i].near;
    }
}

// After the step (matron.hpp, her balance): the change in her speed her feet
// did not make, and the impulse given up by what was closing on her and met
// her, added up over the contact.
void Matron::take_blow(const float dt) {
    if (!sensed_) {
        return;
    }
    sensed_ = false;
    // On the pipe her hands hold her against her weight: a blow is read the
    // same way, but in all three directions, and only on the pipe itself.
    const bool on_pipe = pipe_way_.status() == MoverStatus::Running;
    const Mover *held_by = holder();
    if (!body_in_world_ || dead_ || riding_ || (held_by != nullptr && held_by->view().blow_proof) ||
        state_ == State::Down || state_ == State::Emerging) {
        blow_quiet_ = kBlowQuietTicks;
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const Vec3 velocity = bodies.GetLinearVelocity(body_);
    const Vec3 change = on_pipe ? velocity - set_velocity_
                                : Vec3(velocity.GetX() - set_velocity_.GetX(), 0.0F, velocity.GetZ() - set_velocity_.GetZ());
    const float feet = float(at.GetY()) - kStandCentre;
    const Vec3 facing(std::sin(yaw_), 0.0F, std::cos(yaw_));
    bool struck = false;
    Vec3 impulse = Vec3::sZero();
    float above_share = 0.0F;
    for (std::size_t i = 0; i < near_count_; ++i) {
        const Near &n = near_[i];
        if (!bodies.IsAdded(n.body)) {
            continue;
        }
        const Vec3 after = bodies.GetLinearVelocity(n.body);
        // What met it this step: its change less gravity's own.
        const Vec3 its_change = after - n.velocity - physics_.GetGravity() * (bodies.GetGravityFactor(n.body) * dt);
        if (its_change.Length() < 0.1F) {
            continue;   // nothing met it this step
        }
        Vec3 toward(at - n.at);
        const float apart = toward.Length();
        if (apart < 1.0e-3F) {
            continue;
        }
        toward /= apart;
        // Closing on her of its own motion (not her walking into it at rest).
        if (n.velocity.Dot(toward) < kStrikerClosingMps ||
            (n.velocity - set_velocity_).Dot(toward) < kStrikerClosingMps) {
            continue;
        }
        // And it met her: in contact with her body this step (passing close
        // by is no blow).
        if (!physics_.WereBodiesInContact(body_, n.body)) {
            continue;
        }
        struck = true;
        // What it gave up is hers: along the floor (on the pipe, every way),
        // and a weight's from above.
        impulse -= (on_pipe ? its_change : Vec3(its_change.GetX(), 0.0F, its_change.GetZ())) * (n.mass_kg / kMassKg);
        const bool its_low = float(n.at.GetY()) < feet + kLowBlowM;
        const bool its_above = n.mass_kg >= kAboveMassKg && n.velocity.GetY() < -kAboveMps && n.at.GetY() > at.GetY();
        if (its_above) {
            above_share += std::max(0.0F, its_change.GetY()) * n.mass_kg / kMassKg;
        }
        if (blow_quiet_ >= kBlowQuietTicks) {
            // A fresh blow: what kind, from where, and whether she faces it.
            blow_low_ = false;
            blow_from_above_ = false;
            blow_faced_ = false;
            blow_staggered_ = false;
            blow_from_ = Vec3::sZero();
        }
        blow_low_ = blow_low_ || its_low;
        blow_from_above_ = blow_from_above_ || its_above;
        Vec3 flat(toward.GetX(), 0.0F, toward.GetZ());
        if (flat.LengthSq() > 1.0e-6F) {
            flat = flat.Normalized();
            if (blow_from_.IsNearZero()) {
                blow_from_ = flat;
            }
            blow_faced_ = blow_faced_ || (!its_low && !its_above && -flat.Dot(facing) >= kBraceCos);
        }
    }
    if (!struck) {
        if (++blow_quiet_ >= kBlowQuietTicks) {
            blow_change_ = Vec3::sZero();
            blow_impulse_ = Vec3::sZero();
            blow_above_ = 0.0F;
        }
        return;
    }
    if (blow_quiet_ >= kBlowQuietTicks) {
        blow_change_ = Vec3::sZero();
        blow_impulse_ = Vec3::sZero();
        blow_above_ = 0.0F;
    }
    blow_quiet_ = 0;
    // Her own change is the blow's only while the player's body is not
    // against her too (their push is a shove, not a blow).
    if (!physics_.WereBodiesInContact(body_, player_)) {
        blow_change_ += change;
    }
    blow_impulse_ += impulse;
    blow_above_ += above_share;
    const float blow = std::max({blow_change_.Length(), blow_impulse_.Length(), blow_from_above_ ? blow_above_ : 0.0F});
    blow_mps_ = blow;
    if (on_pipe) {
        if (blow >= kPipeTearMps) {
            blow_quiet_ = kBlowQuietTicks;
            pipe_struck(at);
        }
        return;
    }
    // Her balance: her stance, her base, and the step she can take.
    const bool lunge_burst = turn_ == Turn::Lunge &&
                             std::fmod(turn_s_ - kTurnWindupSeconds, kTurnCycleSeconds) < kTurnBurstSeconds;
    const bool crouched =
        (state_ == State::Hunting && speed_ > 1.0F && stage_ != Stage::Pest) || turn_ == Turn::Lunge;
    const float height = crouched ? kCrouchHeightM : kUprightHeightM;
    const float base = crouched ? kCrouchBaseM : kUprightBaseM;
    const float w = std::sqrt(-physics_.GetGravity().GetY() / height);
    float reach = step_reach();
    if (turn_ == Turn::Windup || blow_low_ || blow_from_above_) {
        reach = 0.0F;
    } else {
        reach *= lunge_burst ? 2.0F : 1.0F;
        reach *= set_s_ > 0.0F ? 2.0F : 1.0F;
        reach *= held_ >= 0 ? 0.5F : 1.0F;
        reach *= blow_faced_ && stage_ == Stage::Dangerous ? 2.0F : 1.0F;
    }
    const float takes = w * base;
    const float fells = w * (base + reach);
    // Fully changed, a blow she faces only staggers her (outside the wind-up).
    const bool braced = blow_faced_ && stage_ == Stage::Changed && turn_ != Turn::Windup;
    if (blow < takes || (state_ == State::Staggering && (blow < fells || braced))) {
        return;   // taken; or staggering already, and no worse than that
    }
    // The way it sends her: along the impulse, else her change, else away
    // from the striker.
    Vec3 way = blow_impulse_.LengthSq() > 1.0e-4F ? blow_impulse_.Normalized()
               : blow_change_.LengthSq() > 1.0e-4F ? blow_change_.Normalized()
                                                    : blow_from_;
    if (way.IsNearZero()) {
        way = facing;
    }
    fall_x_ = way.GetX();
    fall_z_ = way.GetZ();
    if (blow < fells || braced) {
        resume_state_ = state_;
        state_ = State::Staggering;
        stagger_s_ = kStaggerMinSeconds + (kStaggerMaxSeconds - kStaggerMinSeconds) *
                                              std::clamp((blow - takes) / std::max(fells - takes, 0.01F), 0.0F, 1.0F);
        ++staggers_;
        rage_ = std::min(1.0F, rage_ + kStaggerRage);
        blow_staggered_ = true;
        throw_windup_s_ = -1.0F;
        if (turn_ != Turn::None) {
            // Knocked out of the Turn: not straight back into it.
            turn_cooldown_s_ = turn_cooldown_seconds();
            sight_far_s_ = 0.0F;
        }
        turn_ = Turn::None;
        turn_s_ = 0.0F;
        return;
    }
    knock_down();
}

void Matron::knock_down() {
    state_ = State::Down;
    resume_state_ = State::Searching;
    down_s_ = down_seconds();
    down_for_s_ = 0.0F;
    drop_stock();
    turn_ = Turn::None;
    turn_s_ = 0.0F;
    turn_cooldown_s_ = turn_cooldown_seconds();
    sight_far_s_ = 0.0F;
    if (blow_staggered_) {
        // The stagger it began as is this knockdown: counted once.
        --staggers_;
        rage_ = std::max(0.0F, rage_ - kStaggerRage);
        blow_staggered_ = false;
    }
    ++knockdowns_;
    rage_ = std::min(1.0F, rage_ + kKnockdownRage);
    blow_quiet_ = kBlowQuietTicks;
    lie_down(fall_x_, fall_z_);
}

bool Matron::shape_fits(const Shape *shape, const RVec3 at) const {
    AllHitCollisionCollector<CollideShapeCollector> hits;
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    IgnoreMultipleBodiesFilter not_us;
    not_us.Reserve(2);
    not_us.IgnoreBody(body_);
    not_us.IgnoreBody(player_);
    // `at` is the body's origin; Jolt places a shape about its centre of mass
    // (a lying capsule's sits under the origin).
    physics_.GetNarrowPhaseQuery().CollideShape(shape, Vec3::sReplicate(1.0F),
                                                RMat44::sTranslation(at + RVec3(shape->GetCenterOfMass())), settings,
                                                at, hits, {}, {}, not_us);
    for (const CollideShapeResult &hit : hits.mHits) {
        // Resting on a floor is touching, not in it.
        if (hit.mPenetrationDepth <= 0.03F) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
        if (lock.Succeeded() && lock.GetBody().IsSensor()) {
            continue;
        }
        return false;
    }
    return true;
}

bool Matron::room_to_stand(const RVec3 at) const {
    return shape_fits(shape_, at + RVec3(0.0, 0.02, 0.0));
}

// Down on the steel along (fall_x, fall_z): a capsule lying that way under
// her middle (her middle stays at her standing height over the floor, so her
// floor, level and fall checks hold), or kneeling where lying does not fit.
void Matron::lie_down(const float fall_x, const float fall_z) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    Vec3 fall(fall_x, 0.0F, fall_z);
    fall = fall.LengthSq() > 1.0e-6F ? fall.Normalized() : Vec3(std::sin(yaw_), 0.0F, std::cos(yaw_));
    // Her long axis (the capsule's y) laid along the fall: turned a quarter
    // about the horizontal across it.
    const Vec3 across = Vec3::sAxisY().Cross(fall).Normalized();
    const Ref<Shape> lying =
        new RotatedTranslatedShape(Vec3(0.0F, -(kStandCentre - kRadius), 0.0F), Quat::sRotation(across, 0.5F * kPi),
                                   new CapsuleShape(kHalfHeight, kRadius));
    kneeling_ = false;
    if (shape_fits(lying, at)) {
        down_shape_ = lying;
    } else {
        constexpr float kKneelHalf = 0.15F;   // a 0.9 m heap on her knees
        const Ref<Shape> kneel = new RotatedTranslatedShape(
            Vec3(0.0F, -(kStandCentre - kRadius - kKneelHalf), 0.0F), Quat::sIdentity(), new CapsuleShape(kKneelHalf, kRadius));
        kneeling_ = true;
        down_shape_ = kneel;
    }
    bodies.SetShape(body_, down_shape_, false, EActivation::Activate);
    bodies.SetFriction(body_, kDownFriction);
    lying_ = true;
}

void Matron::stand_up() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    bodies.SetShape(body_, shape_, false, EActivation::Activate);
    bodies.SetFriction(body_, 0.0F);
    lying_ = false;
    kneeling_ = false;
    down_shape_ = nullptr;
    set_s_ = kSetSeconds;
    // Up, she looks about her where she fell (not back at an old noise: what
    // put her down was no lie, and nothing she is fooled by).
    const RVec3 at = bodies.GetPosition(body_);
    heard_x_ = float(at.GetX());
    heard_z_ = float(at.GetZ());
    lead_seen_ = true;
    state_ = State::Searching;
    search_s_ = 0.0F;
    search_budget_s_ = 1.0e9F;
    search_trip_s_ = 0.0F;
}

void Matron::brace_at_edges(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    Vec3 velocity = bodies.GetLinearVelocity(body_);
    IgnoreMultipleBodiesFilter not_us;
    not_us.Reserve(2);
    not_us.IgnoreBody(body_);
    not_us.IgnoreBody(player_);
    // Floor within a step's drop below her feet at (x, z) (a crack her foot or
    // hand bridges is not an edge).
    const auto floor_near = [&](const Vec3 &dir, const float ahead_m) {
        for (float on = 0.0F; on <= kStepOverM + 0.01F; on += 0.15F) {
            const RVec3 from = at + dir * (ahead_m + on) + Vec3(0.0F, 0.5F - kStandCentre, 0.0F);
            RayCastResult down;
            if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -(0.5F + kEdgeDropM), 0.0F)), down, {},
                                                       {}, not_us)) {
                return true;
            }
        }
        return false;
    };
    bool braced = false;
    for (const Vec3 &side : {Vec3::sAxisX(), -Vec3::sAxisX(), Vec3::sAxisZ(), -Vec3::sAxisZ()}) {
        const float toward = velocity.Dot(side);
        if (toward > 0.0F && !floor_near(side, toward * dt + kEdgeMarginM)) {
            velocity -= side * toward;
            braced = true;
        }
    }
    if (braced) {
        bodies.SetLinearVelocity(body_, velocity);
    }
}

// Staggering (her feet catching her) and down (on the steel, then up).
void Matron::step_knocked(const float dt, const RVec3 player) {
    (void)player;
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    const Vec3 velocity = bodies.GetLinearVelocity(body_);
    Vec3 flat(velocity.GetX(), 0.0F, velocity.GetZ());
    if (state_ == State::Staggering) {
        // Her feet catch her: stopping her at her stage's recovery, the way
        // she was sent.
        const float going = flat.Length();
        if (going > 1.0e-4F) {
            flat -= flat / going * std::min(going, recover_accel() * dt);
        }
        bodies.SetLinearVelocity(body_, Vec3(flat.GetX(), velocity.GetY(), flat.GetZ()));
        brace_at_edges(dt);
        speed_ = flat.Length();
        stride_ += speed_ * dt;
        stagger_s_ -= dt;
        if (stagger_s_ <= 0.0F) {
            stagger_s_ = 0.0F;
            state_ = resume_state_;
        }
        return;
    }
    // Down: sliding to a stop on the steel (friction), never off an edge.
    brace_at_edges(dt);
    speed_ = flat.Length();
    down_for_s_ += dt;
    down_s_ -= dt;
    if (down_s_ > 0.0F) {
        return;
    }
    if (room_to_stand(at)) {
        stand_up();
        return;
    }
    // Pinned where she would stand (under a crate, a beam, against a post):
    // she crawls out to room first, the way she fell first.
    const float fall = std::atan2(fall_x_, fall_z_);
    for (int k = 0; k < 8; ++k) {
        const float a = fall + kPi * 0.25F * static_cast<float>((k + 1) / 2) * (k % 2 == 0 ? 1.0F : -1.0F);
        const Vec3 dir(std::sin(a), 0.0F, std::cos(a));
        if (room_to_stand(at + dir * 0.6F)) {
            bodies.SetLinearVelocity(body_, Vec3(dir.GetX() * kCrawlMps, velocity.GetY(), dir.GetZ() * kCrawlMps));
            brace_at_edges(dt);
            stride_ += kCrawlMps * dt;
            return;
        }
    }
}

void Matron::refresh_view() {
    view_.state = state_;
    view_.alive = !dead_;
    view_.released = out(state_);
    view_.fooled = fooled_;
    view_.wary = wary_ || state_ == State::Peering;
    view_.vaults = vaults_;
    view_.leaps = leaps_;
    view_.unrigs = unrigs_;
    report_line();
    // What the way that has her shows (matron_mover.hpp): a planned move
    // counts as climbing once a hand of hers holds (walking to its start,
    // her hands are free).
    const Mover *way = holder();
    const MoverView shown = way != nullptr ? way->view() : MoverView{};
    view_.climbing = shown.hands;
    view_.climb_move_active = shown.move_active;
    view_.climb_bar_traverse = shown.bar_traverse;
    view_.climb_move = shown.move_active ? shown.move : traversal::MoveKind::Walk;
    view_.climb_phase = shown.phase;
    view_.climb_airborne = shown.airborne;
    view_.climb_sliding = shown.sliding;
    view_.climb_leg_driving = shown.leg_driving || (foot_driving_ && !foot_prep_);
    view_.climbs = climbs_done_;
    // The hand and force of whichever of her climbs or foot drives has her
    // (no way showing a hand: the last rung she held).
    view_.climb_hand = shown.hand_shown ? shown.hand : climb_way_.view().hand;
    view_.climb_force_n = shown.force_shown ? shown.force_n : foot_force_n_;
    // The Turn lunge is its own move with its own cap (kTurnLungeForceN,
    // kTurnLungePowerW); the report says when the force shown is that drive.
    view_.climb_lunge_drive = !shown.force_shown && foot_lunge_;
    view_.foot_power_w = foot_power_w_;
    view_.climb_grips = climb_grips_;
    view_.climb_releases = climb_releases_;
    view_.mantling = mantling_;
    view_.clinging = riding_;
    view_.floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    view_.lead_floor_y = lead_floor_;
    view_.rage = rage_;
    view_.stage = stage_;
    view_.exposure = std::min(1.0F, exposure_s_ / kExposureFullSeconds);
    view_.shoves = shoves_;
    view_.bracing_structure = bracing_structure();
    view_.support_n = shared_survival_ ? shared_survival_->snapshot().applied_n : 0.0F;
    view_.trap = static_cast<std::uint8_t>(trap_);
    view_.traps = traps_;
    view_.carrying = held_ >= 0 ? throwables_[static_cast<std::size_t>(held_)].entity : 0;
    view_.takes = takes_;
    view_.throws = throws_;
    view_.staggers = staggers_;
    view_.knockdowns = knockdowns_;
    view_.blow_mps = blow_mps_;
    view_.fall_x = fall_x_;
    view_.fall_z = fall_z_;
    view_.kneeling = kneeling_;
    view_.down_s = state_ == State::Down ? down_for_s_ : 0.0F;
    view_.down_left_s = state_ == State::Down ? std::max(0.0F, down_s_) : 0.0F;
    view_.route_edges = route_edges_done_;
    view_.route_failures = route_failures_;
    report_pipe();
    view_.strain = state_ == State::Dormant ? 0.0F : std::min(1.0F, strain_s_ / kStrainSeconds);
    if (view_.released) {
        view_.strain = 1.0F;
    }
    view_.bolts_popped = std::floor(view_.strain * 4.0F);
    view_.speed = speed_;
    view_.stride = stride_;
    view_.yaw = yaw_;
    view_.noise_heard = noise_heard_;
    view_.lead_x = heard_x_;
    view_.lead_z = heard_z_;
    view_.lead_age_s = since_heard_s_;
    view_.lead_seen = lead_seen_;
    view_.lead_valid = lead_valid_ && since_heard_s_ <= (lead_seen_ ? kPipeChaseChangedSeconds : kAcousticMemorySeconds);
    view_.acoustic_queries = acoustic_queries_;
    view_.sees_player = sees_player_;
    view_.target_x = target_x_;
    view_.target_z = target_z_;
    view_.catwalk_allowed = allow_catwalk_;
    view_.turn = turn_;
    view_.turn_s = turn_s_;
    view_.pace = static_cast<std::uint8_t>(pace_);
    view_.pace_mps = pace_mps(pace_);
    view_.pace_changes = pace_changes_;
    view_.stalls = stalls_;
    view_.step_ups = step_ups_;
    view_.clambers = clambers_;
    view_.sidesteps = sidesteps_;
    view_.reroutes = reroutes_;
    view_.longest_stall_s = longest_stall_s_;
    view_.crumbs = static_cast<std::uint32_t>(crumb_count_);
    view_.crumbs_walked = crumbs_walked_;
    view_.hushed = hush_s_ > 0.0F;
    view_.creaks = creaks_;
    view_.grabs = grabs_;
    view_.grab_misses = grab_misses_;
    view_.carries = carries_;
    view_.dangles = dangles_;
    view_.armplays = armplays_;
    view_.releases = releases_;
    view_.spares = spares_;
    view_.grips_torn = grips_torn_;
    view_.brace_losses = brace_losses_;
    view_.carry_drops = carry_drops_;
    view_.arm_tears = arm_tears_;
    view_.arm_throws = arm_throws_;
    view_.claims = claims_;
    view_.fatal_beat = state_ == State::Claimed ? static_cast<std::uint8_t>(fatal_beat_) : 0;
    view_.waste_not = waste_not_;
    view_.armplay_beat = state_ == State::ArmPlay ? static_cast<std::uint8_t>(hold_phase_) : 0;
    view_.hand2_x = float(hand2_target_.GetX());
    view_.hand2_y = float(hand2_target_.GetY());
    view_.hand2_z = float(hand2_target_.GetZ());
    view_.grip_on = grip_on_;
    view_.hold_s = hold_s_;
    view_.hold_braced = holding() && braced_at_rim(view_.x, view_.z);
    view_.hand_x = float(hand_target_.GetX());
    view_.hand_y = float(hand_target_.GetY());
    view_.hand_z = float(hand_target_.GetZ());
    if (shown.bar_traverse) {
        view_.hand_x = float(shown.hand.GetX()); view_.hand_y = float(shown.hand.GetY()); view_.hand_z = float(shown.hand.GetZ());
        view_.support_n = shown.force_n;
    }
    view_.aim_ledge = aim_ledge_;
    view_.aim_x = float(aim_.GetX());
    view_.aim_y = float(aim_.GetY());
    view_.aim_z = float(aim_.GetZ());
    if (body_in_world_) {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        view_.x = float(at.GetX());
        view_.y = float(at.GetY());
        view_.z = float(at.GetZ());
    } else {
        view_.x = kLockerX;
        view_.y = kDeckTop + kStandCentre;
        view_.z = kLockerZ;
    }
    // Second climb hold + feet support: VIEW PLUMBING ONLY. Nothing here
    // moves her, grips, or changes a climb: it only reports two points
    // for the presentation to pose to, computed from the climb geometry
    // and probed against the world. Movement code never reads these
    // fields, and no climb state (climb_, climb_support_, climb_hand_)
    // is written here.
    view_.climb_hand2 = JPH::RVec3::sZero();
    view_.climb_hand2_known = false;
    view_.climb_feet_support = JPH::RVec3::sZero();
    view_.climb_feet_support_known = false;
    if (shown.hand_held) {
        // Her left, one shoulder-width (0.45 m) along the rung or lip
        // from the first hold, at the same height. On rungs across her
        // facing (a legacy climb) her left runs along them; otherwise it
        // is her yaw's left, (-cos yaw, 0, sin yaw), matching the
        // figure's local -x.
        const Vec3 left = shown.rungs_across ? shown.rung_left : Vec3(-std::cos(yaw_), 0.0F, std::sin(yaw_));
        if (left.LengthSq() > 1.0e-6F) {
            const RVec3 second = view_.climb_hand + left.Normalized() * 0.45F;
            view_.climb_hand2 = second;
            // A legacy climb can prove the second point: it must rest
            // on the very support and member the first hand holds.
            // Journey and routecraft climbs carry no support handle
            // here, so their candidate stays reported-but-unknown and
            // the rig keeps the single-hold pose rather than posing a
            // wrist to a point no surface was ever shown under.
            if (shown.rungs_across) {
                const ClimbBodyFilter filter(body_, player_);
                BodyID rests_body;
                SubShapeID rests_member;
                view_.climb_hand2_known =
                    hand_rests_on(physics_.GetNarrowPhaseQuery(), second, filter, rests_body, rests_member) &&
                    rests_body == shown.hold_body && rests_member == shown.hold_member;
            }
        }
    }
    if (body_in_world_) {
        // Feet: the surface straight under her body centre, probed like
        // climb()'s floor check (centre down past her soles).
        const RVec3 at(view_.x, view_.y, view_.z);
        const ClimbBodyFilter filter(body_, player_);
        RayCastResult floor_hit;
        if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor_hit,
                                                    {}, {}, filter)) {
            view_.climb_feet_support = RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)).GetPointOnRay(floor_hit.mFraction);
            view_.climb_feet_support_known = true;
        } else if (shown.rungs_across) {
            // No surface under her (mid-mantle, feet in the air): the
            // climb's top deck is where her feet land.
            view_.climb_feet_support = shown.top;
            view_.climb_feet_support_known = true;
        } else {
            view_.climb_feet_support = RVec3(view_.x, view_.floor_y, view_.z);
            view_.climb_feet_support_known = false;
        }
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (std::size_t i = 0; i < planks_.size(); ++i) {
        PlankView &view = plank_views_[i];
        view.bolted = planks_[i].bolted;
        view.gone = planks_[i].gone;
        if (planks_[i].gone) {
            continue;
        }
        const RVec3 at = bodies.GetPosition(planks_[i].body);
        const Quat rotation = bodies.GetRotation(planks_[i].body);
        view.x = float(at.GetX());
        view.y = float(at.GetY());
        view.z = float(at.GetZ());
        view.qx = rotation.GetX();
        view.qy = rotation.GetY();
        view.qz = rotation.GetZ();
        view.qw = rotation.GetW();
    }
}

} // namespace scraperx::sim
