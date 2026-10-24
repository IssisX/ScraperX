// The Matron: a sealed locker, a build-up, and a hunt by sound on the 154 m
// deck (see matron.hpp).
#include "sim/matron.hpp"
#include "sim/matron_grip.hpp"

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

constexpr float kPi = 3.14159265F;
// How far a sound of full loudness carries.
constexpr float kFullHearingM = 55.0F;
constexpr float kTurnRate = 7.0F;
// way_clear sweeps her width this much short of her body's: a wall, a rail or
// a post her side only brushes she slides past; one nearer her middle line
// stops her body.
constexpr float kWaySkinM = 0.1F;
// Up a ramp (follow_line) she aims this far ahead along its middle.
constexpr float kRampAheadM = 0.6F;
// On a ramp's strip: this near its middle line (the ramp is 1 m across).
constexpr float kRampStripM = 0.6F;
// Held to a ramp she is on only until the stall watchdog has had her stalled this long on it (its clamber,
// sidestep and reroute all tried): a blocked ramp is given up to her other ways.
constexpr float kRampStallS = 3.0F;

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
}

void Matron::place_for_test(const RVec3 feet) {
    spawn_body();
    BodyInterface &bodies = physics_.GetBodyInterface();
    bodies.SetPositionAndRotation(body_, feet + Vec3(0.0F, kStandCentre + 0.02F, 0.0F), Quat::sIdentity(),
                                  EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(body_, Vec3::sZero(), Vec3::sZero());
    fall_vy_ = 0.0F;
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
    if (climb_ >= 0) {
        release_climb(true);
    } else if (pipe_phase_ != PipePhase::Off && pipe_phase_ != PipePhase::Slide) {
        tear_off_pipe();
    } else if (routecraft_.owns_motion()) {
        // Her hands open on a climb of her own, as a hold giving way opens
        // them: the column is left alone its block time, and no way back.
        routecraft_.cancel();
        if (routecraft_mine_) {
            block_climb(climbing_key_);
            routecraft_mine_ = false;
        }
        climb_back_tried_ = true;
        ++climb_releases_;
    } else if (journey_moving_) {
        end_journey(true);
        ++climb_releases_;
    }
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
        return pipe_reaches(floor_y) && pipe_reaches(to) && get_on_pipe(floor_y, to);
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
}

void Matron::stand_down() {
    lead_valid_ = false;
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
    if (on_line_) {
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
    if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(at, Vec3(0.0F, -reach, 0.0F)), hit, {}, {}, self)) {
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
    // On a level is standing on it: not passing its height falling. Where her
    // map knows the deck, the footprint still counts while she is in the air.
    // Steel the ray actually finds is the floor even beside that footprint
    // (a board or a lip the map does not contain).
    BodyInterface &bodies = physics_.GetBodyInterface();
    if (std::abs(bodies.GetLinearVelocity(body_).GetY()) > 0.6F) {
        return;
    }
    const RVec3 here = bodies.GetPosition(body_);
    // Her middle can sit past a lip while her soles still bear. The floor is
    // the highest walkable hit within a step of her feet, not only the ray
    // through her centre (that one misses a board she is standing on).
    const float reach = kStandCentre + 0.3F;
    const float kinematic = float(here.GetY()) - kStandCentre;
    const IgnoreSingleBodyFilter self(body_);
    float support_y = feet_y_;
    bool supported = false;
    const Vec3 soles[] = {Vec3::sZero(), Vec3(kFootSpanM, 0.0F, 0.0F), Vec3(-kFootSpanM, 0.0F, 0.0F),
                          Vec3(0.0F, 0.0F, kFootSpanM), Vec3(0.0F, 0.0F, -kFootSpanM)};
    for (const Vec3 &sole : soles) {
        RayCastResult one;
        const RVec3 from = here + sole;
        if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -reach, 0.0F)), one, {}, {}, self) ||
            one.mFraction <= 1.0e-4F) {
            continue;
        }
        const RVec3 point = from + Vec3(0.0F, -reach * one.mFraction, 0.0F);
        const float y = float(point.GetY());
        if (std::abs(y - kinematic) > 0.40F) {
            continue;
        }
        {
            const BodyLockRead lock(physics_.GetBodyLockInterface(), one.mBodyID);
            if (!lock.Succeeded() ||
                lock.GetBody().GetWorldSpaceSurfaceNormal(one.mSubShapeID2, point).GetY() < 0.7F) {
                continue;
            }
        }
        if (!supported || y > support_y) {
            supported = true;
            support_y = y;
        }
    }
    if (supported) {
        feet_y_ = support_y;
    }
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        if (static_cast<int>(i) == level_ || std::abs(support_y - levels_[i].floor_y) >= 0.6F) {
            continue;
        }
        if (!supported &&
            off_its_steel(static_cast<int>(i), float(here.GetX()), support_y, float(here.GetZ()))) {
            continue;
        }
        level_ = static_cast<int>(i);
        commit_s_ = 0.0F;
        pick_patrol(float(here.GetX()), float(here.GetZ()));
        return;
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

// A line from her level down toward her lead's (to it, or to a level on the
// way): the one she saw taken, or the nearest. -1 when none does.
int Matron::line_to_take(const float ax, const float az) const {
    if (zipline_ == nullptr) {
        return -1;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const std::vector<Zipline::Line> &lines = zipline_->lines();
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Zipline::Line &line = lines[i];
        const float bottom = float(line.bottom_floor.GetY());
        if (std::abs(float(line.top_floor.GetY()) - floor_y) > 0.6F || bottom < lead_floor_ - 0.6F ||
            bottom > floor_y - 0.6F) {
            continue;
        }
        const float d = static_cast<int>(i) == guess_line_
                            ? -1.0F
                            : distance_xz(ax, az, float(line.stand.GetX()), float(line.stand.GetZ()));
        if (d < best_d) {
            best_d = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// On the ramp up to the line's stand: past its foot, on its strip, below the stand.
bool Matron::on_line_ramp(const RVec3 at) const {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const int best = line_to_take(ax, az);
    if (best < 0) {
        return false;
    }
    const Zipline::Line &line = zipline_->lines()[static_cast<std::size_t>(best)];
    const float fx = float(line.ramp_foot.GetX());
    const float fz = float(line.ramp_foot.GetZ());
    const float rx = float(line.stand.GetX()) - fx;
    const float rz = float(line.stand.GetZ()) - fz;
    const float rlen = std::hypot(rx, rz);
    if (rlen < 0.1F) {
        return false;
    }
    const float along = ((ax - fx) * rx + (az - fz) * rz) / rlen;
    const float across = std::abs((ax - fx) * rz - (az - fz) * rx) / rlen;
    const float feet = float(at.GetY()) - kStandCentre;
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    return feet >= floor_y + 0.15F && feet < float(line.stand.GetY()) - 0.3F && along > 0.0F && along < rlen &&
           across < kRampStripM;
}

bool Matron::follow_line(const float dt, const RVec3 at, const bool wait_ok) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const int best = line_to_take(ax, az);
    if (best < 0) {
        return false;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    const std::vector<Zipline::Line> &lines = zipline_->lines();
    const Zipline::Line &line = lines[static_cast<std::size_t>(best)];
    // A line she would only wait at (outside her Turn, or one nobody has
    // ridden) is not a way yet: her own climbing goes first, and she comes
    // to its stand only when she finds none.
    if (!wait_ok && (turn_ == Turn::None || line.parked)) {
        return false;
    }
    const float sx = float(line.stand.GetX());
    const float sz = float(line.stand.GetZ());
    const float stand_y = float(line.stand.GetY());
    const float feet = float(at.GetY()) - kStandCentre;
    if (feet < stand_y - 0.3F || distance_xz(ax, az, sx, sz) > 0.6F) {
        // To the ramp's foot along her lines, then up the ramp to the stand.
        float wx = sx;
        float wz = sz;
        const float fx = float(line.ramp_foot.GetX());
        const float fz = float(line.ramp_foot.GetZ());
        if (feet < floor_y + 0.15F && distance_xz(ax, az, fx, fz) > 0.5F) {
            route_toward(ax, az, fx, fz, false, wx, wz);
        } else {
            // Up the ramp she keeps to its middle: she aims a little ahead along the line from its foot to
            // the stand, not at the stand from wherever she is (a ramp is narrow, and a diagonal run at the
            // stand walks her off its side).
            const float rx = sx - fx;
            const float rz = sz - fz;
            const float rlen = std::hypot(rx, rz);
            if (rlen > 0.1F) {
                const float along = std::clamp(((ax - fx) * rx + (az - fz) * rz) / rlen, 0.0F, rlen);
                const float ahead = std::min(along + kRampAheadM, rlen);
                wx = fx + rx / rlen * ahead;
                wz = fz + rz / rlen * ahead;
            }
        }
        walk_toward(dt, wx, wz, kHuntMps, turn_ != Turn::None ? kTurnFootAccel : kFootAccel);
        return true;
    }
    // On the stand under the cable, looking down the line. Frail, she cannot
    // follow. In the Turn she jumps for the cable and goes; but not down a
    // line nobody has ridden (its trolley still parked at the top): its
    // buffer's block at rest far short of the end, she would stop on it and
    // let go over the shaft.
    walk_toward(dt, ax, az, 0.0F);
    yaw_ = turn_toward(yaw_, std::atan2(line.direction.GetX(), line.direction.GetZ()), kTurnRate, dt);
    if (turn_ == Turn::None || line.parked) {
        return true;
    }
    const RVec3 cable = line.top + line.direction * Zipline::kParkM;
    const RVec3 hands = at + Vec3(0.0F, kHandsAboveCentre, 0.0F);
    if (Vec3(cable - hands).Length() < kCableReachM) {
        zipline_->clip_other(best, body_, Vec3(0.0F, kHandsAboveCentre, 0.0F), true);
        on_line_ = true;
        riding_ = true;
        let_go_s_ = kLetGoSeconds;
        ++cable_slides_;
        return true;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Vec3 velocity = bodies.GetLinearVelocity(body_);
    if (feet < stand_y + 0.08F && std::abs(velocity.GetY()) < 0.2F) {
        // Standing on it: the jump for the cable.
        bodies.SetLinearVelocity(body_, Vec3(velocity.GetX(), kCableJumpMps, velocity.GetZ()));
    }
    return true;
}

void Matron::ride_line() {
    if (!on_line_ || zipline_ == nullptr) {
        return;
    }
    const Zipline::State &ride = zipline_->other_state();
    if (!ride.riding) {
        on_line_ = false;   // taken off it (a restore)
        return;
    }
    // Down at the end (at the stop, or against the trolley or its rider
    // there) and still: she lets go, swinging on over the far band as she
    // does. Never into the shaft: held still where her swing would come down
    // with any of her over it (the buffer's clamp, a trolley left short in
    // it, a rider still hanging at the stop ahead of her), she eases her
    // forearms' drag and comes on as far as the cable lets her.
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Zipline::Line &line = zipline_->lines()[static_cast<std::size_t>(ride.line)];
    const RVec3 at = bodies.GetPosition(body_);
    const bool at_end = ride.along_m > line.length - Zipline::kStopMarginM - Zipline::kBufferM;
    const auto over_shaft = [](const RVec3 p) {
        return std::abs(float(p.GetX())) < kShaftHalfM + kRadius && std::abs(float(p.GetZ()) - kMidZ) < kShaftHalfM + kRadius;
    };
    // Her forearms' drag: friction on her weight across the cable, as a
    // squeeze of the line's brake.
    const float across = std::sqrt(std::max(0.0F, 1.0F - line.direction.GetY() * line.direction.GetY()));
    const float gravity = -physics_.GetGravity().GetY();
    zipline_->set_other_brake(at_end && over_shaft(at)
                                  ? 0.0F
                                  : kCableFriction * kMassKg * gravity * across / Zipline::kHandBrakeMaxN);
    // Where letting go puts her: on along the line as far as her swing
    // carries her while she drops to the far floor.
    const Vec3 run = Vec3(line.direction.GetX(), 0.0F, line.direction.GetZ()).NormalizedOr(Vec3::sZero());
    const float drop = std::max(0.0F, float(at.GetY() - line.bottom_floor.GetY()) - kStandCentre);
    const RVec3 lands = at + run * (kLetGoSwingMps * std::sqrt(2.0F * drop / gravity));
    if (at_end && !over_shaft(lands) && std::abs(ride.speed_mps) < 0.3F) {
        zipline_->unclip_other();
        on_line_ = false;
        bodies.SetLinearVelocity(body_, bodies.GetLinearVelocity(body_) + run * kLetGoSwingMps);
    }
}

bool Matron::follow_climb(const float dt, const RVec3 at) {
    climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
    if (found_climbs_off_) {
        return false;   // one way alone (a test): none of her own
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float level_floor = levels_[static_cast<std::size_t>(level_)].floor_y;
    // A climb from her level up toward her lead's (to it, or to a level on
    // the way): the one she saw taken, or the nearest.
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (int i = 0; i < climb_count(); ++i) {
        Climb c{};
        // Her hand climb in the hunt is for ladders; a lip she takes by her own plan.
        if (!climb_of(i, c, Ways::Ladders)) {
            continue;
        }
        if (!c.hers || std::abs(c.foot_y - level_floor) > 0.6F || c.head_y > lead_floor_ + 0.6F ||
            c.head_y < level_floor + 0.6F) {
            continue;
        }
        const float d = i == guess_climb_ ? -1.0F : distance_xz(ax, az, c.x, c.z);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    if (best < 0) {
        return false;
    }
    const Climb c = climb_spec(best);
    const float off = distance_xz(ax, az, c.x, c.z);
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (off > 0.2F) {
        // To its foot along her lines, slowing into it.
        float wx = c.x;
        float wz = c.z;
        route_toward(ax, az, c.x, c.z, false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // At its foot, looking up it. Frail, she cannot take hold of it; in the
    // Turn she does, and her hands have her weight. The bay's rungs she
    // always does: they are her way out of her own trap.
    walk_toward(dt, c.x, c.z, 0.0F, accel);
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    if (turn_ == Turn::None && best != bay_climb_) {
        return true;
    }
    // One that failed under her a moment ago she leaves alone a while (her map).
    const std::size_t way = routes_.find_way(connector_of(best), false);
    if (way < routes::kMaxEdges && !routes_.usable(way, clock_s_)) {
        return true;
    }
    begin_climb(best);
    return true;
}

namespace {
using grip::ClimbBodyFilter;
using grip::hand_rests_on;
using grip::PointHit;
using grip::RayHits;
} // namespace

// A real upward-facing contact on the climb's line of holds, in hand reach,
// visible from the chest, with room above it for her fingers (a rung buried
// in a fascia plate is no hold). As in the player's physical hang, anchors
// live in the support's frame and the support receives the reaction. The
// mechanism kit's welded load grips cannot attach to static steel and
// rigidly weld the bodies rather than allowing a climber to move relative to
// a hand contact. Going up she takes the highest such hold she can reach
// above the hand she has; going down, the highest below it, never the floor
// at the foot; starting down from the top, the first under its lip.
bool Matron::find_climb_hold(const RVec3 at, const bool down) {
    const Climb c = climb_spec(climb_);
    const bool holding = !climb_support_.IsInvalid();
    float top = 0.0F;
    float bottom = 0.0F;
    if (!down && holding) {
        top = std::min(c.lip_y + 0.04F, float(at.GetY()) + kClimbReachM);
        bottom = float(climb_hand_.GetY()) + 0.1F;
    } else if (!down) {
        // Her first hold, from her feet: overhead, as far as a standing reach.
        top = std::min(c.lip_y + 0.04F, float(at.GetY()) + kStandingReachM);
        bottom = top - 0.6F;
    } else if (holding) {
        top = float(climb_hand_.GetY()) - 0.1F;
        bottom = std::max(c.foot_y + 0.15F, float(at.GetY()) - 0.6F);
    } else {
        top = c.lip_y + 0.04F;
        bottom = c.lip_y - 0.6F;
    }
    if (top <= bottom) {
        return false;
    }
    const RVec3 origin(c.x + c.out_x * c.standoff, top, c.z + c.out_z * c.standoff);
    const ClimbBodyFilter filter(body_, player_);
    const RRayCast ray(origin, Vec3(0.0F, bottom - top, 0.0F));
    RayCastSettings settings;
    settings.mTreatConvexAsSolid = false; // a probe starting inside a rung must not invent a top normal
    RayHits hits;
    physics_.GetNarrowPhaseQuery().CastRay(ray, settings, hits, {}, {}, filter);
    hits.sort();
    for (int i = 0; i < hits.count; ++i) {
        const RayCastResult &hit = hits.hits[static_cast<std::size_t>(i)];
        const RVec3 point = ray.GetPointOnRay(hit.mFraction);
        if (Vec3(point - at).Length() > kClimbReachM) {
            continue;
        }
        PointHit fingers;
        physics_.GetNarrowPhaseQuery().CollidePoint(point + Vec3(0.0F, 0.04F, 0.0F), fingers, {}, {}, filter);
        if (fingers.hit) {
            continue;
        }
        // Her arm's way to it: up in front of the face to just over the hold
        // (a hand goes over a lip's edge, not through its corner); the last
        // few centimeters down onto it are the room above it, checked above.
        const RVec3 chest = at + Vec3(0.0F, 0.6F, 0.0F);
        const RVec3 over = point + Vec3(0.0F, 0.08F, 0.0F) - Vec3(c.out_x, 0.0F, c.out_z) * 0.12F;
        const Vec3 reach(over - chest);
        RayCastResult obstruction;
        if (physics_.GetNarrowPhaseQuery().CastRay(RRayCast(chest, reach), obstruction, {}, {}, filter) &&
            (1.0F - obstruction.mFraction) * reach.Length() > 0.06F) {
            continue;
        }
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point).GetY() < 0.7F) {
            continue;
        }
        BodyID rests_body;
        SubShapeID rests_member;
        if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), point, filter, rests_body, rests_member) ||
            rests_body != hit.mBodyID) {
            continue;
        }
        climb_support_ = hit.mBodyID;
        climb_member_ = rests_member;
        climb_local_hand_ = Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point);
        climb_hand_ = point;
        climb_hand_known_ = true;
        ++climb_grips_;
        return true;
    }
    return false;
}

bool Matron::begin_climb(const int index, const bool down) {
    if (climb_retry_s_ > 0.0F) {
        return false;
    }
    climb_ = index;
    climb_down_ = down;
    lowering_ = down;
    mantling_ = false;
    climb_support_ = BodyID();
    climb_member_ = SubShapeID();
    climb_hand_known_ = false;
    climb_force_n_ = 0.0F;
    climb_stall_s_ = 0.0F;
    climb_overload_s_ = 0.0F;
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    climb_best_y_ = -std::numeric_limits<float>::max();
    // Up: a hold overhead. Down: from its top, the first hold under its lip.
    reaching_up_ = false;
    if (!find_climb_hold(at, down)) {
        release_climb(true);
        return false;
    }
    ++climbs_done_;
    return true;
}

void Matron::release_climb(const bool failed) {
    // A climb she set out on by herself (not along her map's route, which
    // records its own): done, her map knows it carried her; failed, her map
    // leaves that way alone a while.
    if (!route_on_edge_ && climb_ >= 0) {
        const std::size_t way = routes_.find_way(connector_of(climb_), climb_down_);
        if (way < routes::kMaxEdges && failed) {
            routes_.fail(way, routes::Reason::Failed, clock_s_);
            ++route_failures_;
        } else if (way < routes::kMaxEdges) {
            routes_.succeed(way);
            ++route_edges_done_;
        }
    }
    climb_end_ = failed ? ClimbEnd::Failed : ClimbEnd::Done;
    climb_ = -1;
    reaching_up_ = false;
    climb_down_ = false;
    lowering_ = false;
    mantling_ = false;
    climb_support_ = BodyID();
    climb_member_ = SubShapeID();
    climb_force_n_ = 0.0F;
    if (failed) {
        ++climb_releases_;
        climb_retry_s_ = 2.0F;
        // The column it dropped her from, her found ways leave alone as long
        // (none when no hold of it was ever found).
        if (climb_hand_known_) {
            block_climb_at(float(climb_hand_.GetX()), float(climb_hand_.GetZ()));
        }
    }
    // No velocity write: a missed hand or broken hold leaves her momentum
    // and gravity to the same collision solver as every other falling body.
}

void Matron::climb(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Climb c = climb_spec(climb_);
    const RVec3 at = bodies.GetPosition(body_);
    const float feet = float(at.GetY()) - kStandCentre;
    if (climb_support_.IsInvalid() || !bodies.IsAdded(climb_support_)) {
        release_climb(true);
        return;
    }
    climb_hand_ = bodies.GetCenterOfMassTransform(climb_support_) * climb_local_hand_;
    climb_hand_known_ = true;
    // The body can survive while a compound's rung disappears or moves.
    // Revalidate the contact, the very part of the support she holds and not
    // merely its body (a compound is many rungs), before applying load.
    const ClimbBodyFilter support_filter(body_, player_);
    BodyID rests_body;
    SubShapeID rests_member;
    if (!hand_rests_on(physics_.GetNarrowPhaseQuery(), climb_hand_, support_filter, rests_body, rests_member) ||
        rests_body != climb_support_ || rests_member != climb_member_) {
        release_climb(true);
        return;
    }

    if (Vec3(climb_hand_ - at).Length() > kClimbReachM) {
        release_climb(true);
        return;
    }
    const auto floor_under = [&] {
        RayCastResult floor;
        return physics_.GetNarrowPhaseQuery().CastRay(RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), floor,
                                                      {}, {}, support_filter);
    };
    Vec3 target(c.x, float(climb_hand_.GetY()) - (reaching_up_ ? 0.30F : 0.65F), c.z);
    float progress = 0.0F;
    if (!climb_down_) {
        // The hand can only advance to another reachable surface; empty space
        // above the last rung is not a hold. Retain the old hand during a miss.
        // With none higher in reach yet, she pulls up higher on the one she
        // has (the hand at her chest), to reach further.
        if (!mantling_ && climb_hand_.GetY() - at.GetY() < 1.05F) {
            reaching_up_ = !find_climb_hold(at, false);
        }
        // Over the top from a hand near the lip, or from the last hold under
        // a lip she cannot hold on its line (a plate's edge stands out from
        // the rungs): up and over it, if her body fits all the way.
        if (!mantling_ && (climb_hand_.GetY() > c.lip_y - 0.4F || (reaching_up_ && climb_hand_.GetY() > c.lip_y - 0.7F))) {
            // Reuse the same full-body clearance query as standing after a fall,
            // up over the lip, half way out and landed: collisions still
            // arbitrate every step in between.
            const RVec3 lifted(c.x, c.lip_y + kStandCentre + 0.12F, c.z);
            const RVec3 half = lifted + Vec3(c.out_x, 0.0F, c.out_z) * (0.5F * kMantleOutM);
            const RVec3 landed = lifted + Vec3(c.out_x, 0.0F, c.out_z) * kMantleOutM;
            if (!shape_fits(shape_, lifted) || !shape_fits(shape_, half) || !shape_fits(shape_, landed)) {
                release_climb(true);
                return;
            }
            mantling_ = true;
            climb_stall_s_ = 0.0F;
        }
        if (mantling_) {
            target.SetY(c.lip_y + kStandCentre + 0.12F);
            if (feet > c.lip_y + 0.03F) {
                target.SetX(c.x + c.out_x * kMantleOutM);
                target.SetZ(c.z + c.out_z * kMantleOutM);
            }
            const float out = (float(at.GetX()) - c.x) * c.out_x + (float(at.GetZ()) - c.z) * c.out_z;
            // Verify actual landing steel, not just the authored destination.
            if (out > 0.8F && feet >= c.lip_y - 0.08F && floor_under()) {
                release_climb(false);
                return;
            }
        }
        progress = float(at.GetY()) +
                   (mantling_ ? (float(at.GetX()) - c.x) * c.out_x + (float(at.GetZ()) - c.z) * c.out_z : 0.0F);
    } else if (lowering_) {
        // Off the top backward onto its face, her hands on the hold under the
        // lip: at the height she has, to its line, then down it.
        target = Vec3(c.x, float(at.GetY()), c.z);
        const float to_line = distance_xz(float(at.GetX()), float(at.GetZ()), c.x, c.z);
        progress = -to_line;
        if (to_line < 0.12F) {
            // On its face: from here her progress is measured going down.
            lowering_ = false;
            climb_stall_s_ = 0.0F;
            progress = -float(at.GetY());
            climb_best_y_ = progress - 0.05F;
        }
    } else {
        // Hand under hand: hanging off the hold she has, and with her hands
        // high over her, the next one down at her chest. Her feet down onto
        // its foot's support is the end of it; air under them is not.
        if (climb_hand_.GetY() - at.GetY() > 0.95F) {
            find_climb_hold(at, true);
        }
        target.SetY(std::max(float(climb_hand_.GetY()) - 1.15F, c.foot_y + kStandCentre));
        if (feet <= c.foot_y + 0.06F && floor_under()) {
            release_climb(false);
            return;
        }
        progress = -float(at.GetY());
    }
    const Vec3 relative = bodies.GetLinearVelocity(body_) - bodies.GetPointVelocity(climb_support_, climb_hand_);
    Vec3 wanted = (target - Vec3(at)) * 3.0F;
    const float rate = mantling_ ? kMantleMps : kClimbMps;
    if (wanted.Length() > rate) {
        wanted *= rate / wanted.Length();
    }
    // Clearance is enforced by the real capsule and solver. A jam consumes
    // finite force and times out; it never teleports through the lip/ceiling.
    Vec3 force = kMassKg * ((wanted - relative) * 7.0F - physics_.GetGravity());
    const float requested = force.Length();
    climb_overload_s_ = requested > kClimbGripMaxN ? climb_overload_s_ + dt : 0.0F;
    if (climb_overload_s_ > 0.18F) {
        release_climb(true);
        return;
    }
    if (requested > kClimbGripMaxN) {
        force *= kClimbGripMaxN / requested;
    }
    if (progress > climb_best_y_ + 0.04F) {
        climb_best_y_ = progress;
        climb_stall_s_ = 0.0F;
    } else {
        climb_stall_s_ += dt;
    }
    if (climb_stall_s_ > 2.5F) {
        release_climb(true);
        return;
    }
    climb_force_n_ = force.Length();
    bodies.AddForce(body_, force);
    if (bodies.GetMotionType(climb_support_) == EMotionType::Dynamic) {
        bodies.AddForce(climb_support_, -force, climb_hand_);
    }
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    speed_ = 0.0F;
    stride_ += bodies.GetLinearVelocity(body_).Length() * dt;
}

bool Matron::pipe_reaches(const float floor_y) noexcept {
    if (floor_y < kPipeLowestFloor - 0.1F || floor_y > kDeckTop + 0.1F) {
        return false;
    }
    const float storeys = floor_y / kStackPitch;
    return std::abs(storeys - std::round(storeys)) < 0.02F;
}

bool Matron::follow_pipe(const float dt, const RVec3 at) {
    // Frail, she cannot take hold of it.
    if (stage_ == Stage::Pest) {
        return false;
    }
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    float next = floor_y;
    if (lead_floor_ < floor_y - 0.6F) {
        next = floor_y - kStackPitch;
    } else if (lead_floor_ > floor_y + 0.6F) {
        next = floor_y + kStackPitch;
    }
    if (next == floor_y || !pipe_reaches(floor_y) || !pipe_reaches(next)) {
        return false;
    }
    // A way that failed under her a moment ago she leaves alone a while.
    const std::size_t way = pipe_way(floor_y, next);
    if (way < routes::kMaxEdges && !routes_.usable(way, clock_s_)) {
        return false;
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float off = distance_xz(ax, az, kPipeDeckX, kPipeDeckZ);
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (off > 0.25F) {
        // To the rail's corner along her lines, slowing into it.
        float wx = kPipeDeckX;
        float wz = kPipeDeckZ;
        route_toward(ax, az, kPipeDeckX, kPipeDeckZ, false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    // Her hands on the rail: over it and onto the pipe (matron_pipe.cpp).
    climb_retry_s_ = std::max(0.0F, climb_retry_s_ - dt);
    walk_toward(dt, ax, az, 0.0F, accel);
    yaw_ = turn_toward(yaw_, std::atan2(0.0F, kPipeRailZ - kPipeDeckZ), kTurnRate, dt);
    (void)get_on_pipe(floor_y, next);
    return true;
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
    // Feet drive on a floor under her soles, not only under her middle: she
    // can be standing with her centre past a lip. Not once she has jumped
    // off it (rising off it faster than it rises).
    const IgnoreSingleBodyFilter self(body_);
    const float kinematic = float(at.GetY()) - kStandCentre;
    const float reach = kStandCentre + 0.3F;
    bool floor = false;
    const Vec3 soles[] = {Vec3::sZero(), Vec3(kFootSpanM, 0.0F, 0.0F), Vec3(-kFootSpanM, 0.0F, 0.0F),
                          Vec3(0.0F, 0.0F, kFootSpanM), Vec3(0.0F, 0.0F, -kFootSpanM)};
    for (const Vec3 &sole : soles) {
        RayCastResult one;
        const RVec3 from = at + sole;
        if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -reach, 0.0F)), one, {}, {}, self) ||
            one.mFraction <= 1.0e-4F) {
            continue;
        }
        const float y = float(from.GetY()) - reach * one.mFraction;
        if (std::abs(y - kinematic) > 0.40F) {
            continue;
        }
        const RVec3 point = from + Vec3(0.0F, -reach * one.mFraction, 0.0F);
        const BodyLockRead lock(physics_.GetBodyLockInterface(), one.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().GetWorldSpaceSurfaceNormal(one.mSubShapeID2, point).GetY() < 0.7F) {
            continue;
        }
        if (velocity.GetY() < bodies.GetLinearVelocity(one.mBodyID).GetY() + 1.0F) {
            floor = true;
            break;
        }
    }
    float dx = wx - float(at.GetX());
    float dz = wz - float(at.GetZ());
    const float length = std::hypot(dx, dz);
    Vec3 want = Vec3::sZero();
    if (length > 0.15F) {
        want = Vec3(dx / length * speed, 0.0F, dz / length * speed);
        yaw_ = turn_toward(yaw_, std::atan2(dx, dz), kTurnRate, dt);
    }
    if (floor) {
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
        float top = 0.0F;
        const auto edge_at = [&](const Vec3 &dir, const float ahead_m) {
            for (float on = 0.0F; on <= kStepOverM + 0.01F; on += 0.15F) {
                if (floor_at(dir, ahead_m + on, 0.5F, 0.5F + kEdgeDropM, top)) {
                    return false;
                }
            }
            return true;
        };
        // A board already under her feet, continuing ahead, is floor even when
        // it is narrower than kFootSpanM. On her map's own deck the span check
        // still refuses a step off that deck onto a post. Off the map (the
        // landing a climb just put her on), the steel under her middle is the
        // floor, including the step down onto the deck within kStepUpM.
        const auto narrow_continues = [&](const Vec3 &dir, const float ahead_m) {
            float here_top = 0.0F;
            const float depth = 0.5F + kEdgeDropM;
            const RVec3 mid(float(at.GetX()), feet + 0.5F, float(at.GetZ()));
            if (!floor_under(mid, depth, here_top) || std::abs(here_top - feet) > 0.35F) {
                return false;
            }
            const bool off_map = off_its_steel(level_, float(at.GetX()), feet, float(at.GetZ()));
            if (!off_map) {
                const Vec3 across(-dir.GetZ(), 0.0F, dir.GetX());
                float side = 0.0F;
                const bool left =
                    floor_under(mid + across * kFootSpanM, depth, side) && std::abs(side - here_top) <= kStepUpM;
                const bool right =
                    floor_under(mid - across * kFootSpanM, depth, side) && std::abs(side - here_top) <= kStepUpM;
                if (left && right) {
                    return false;
                }
            }
            float ahead_top = 0.0F;
            const RVec3 from = at + dir * ahead_m + Vec3(0.0F, 0.5F - kStandCentre, 0.0F);
            if (!floor_under(from, depth, ahead_top)) {
                return false;
            }
            return std::abs(ahead_top - here_top) <= kStepUpM;
        };
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
                    velocity = Vec3(heading.GetX() * kLeapForwardMps, kLeapUpMps, heading.GetZ() * kLeapForwardMps);
                    vault_cooldown_s_ = kVaultCooldownS;
                    ++leaps_;
                    leapt = true;
                }
            }
        }
        const Vec3 flat_new(velocity.GetX(), 0.0F, velocity.GetZ());
        const float going = flat_new.Length();
        if (!leapt && going > 0.05F) {
            // Never off an edge: where her feet are taking her (just ahead, and
            // as far as she needs to stop from this speed), she plants them.
            const Vec3 way = flat_new / going;
            const float check = 0.3F + going * going / 40.0F + going * dt;
            if ((edge_at(way, 0.2F) && !narrow_continues(way, 0.2F)) ||
                (edge_at(way, check) && !narrow_continues(way, check))) {
                velocity -= way * std::min(going, cut_limit);
            }
            // And no step that brings her within kEdgeMarginM of an edge to
            // either side (the tower's edges run along x and z): sliding along
            // one at a slant she keeps her distance. The board she is already
            // on is not that edge.
            for (const Vec3 &side : {Vec3::sAxisX(), -Vec3::sAxisX(), Vec3::sAxisZ(), -Vec3::sAxisZ()}) {
                const float toward = velocity.Dot(side);
                const float reach = toward * dt + kEdgeMarginM;
                if (toward > 0.0F && edge_at(side, reach) && !narrow_continues(side, reach)) {
                    velocity -= side * toward;
                }
            }
        }
        // The Turn's vault: something lower than her chest close ahead in
        // her way (not the player), and nothing at her head: she jumps it.
        if (turn_moves && !leapt && vault_cooldown_s_ <= 0.0F && length > 0.15F) {
            const Vec3 ahead(dx / length * 1.1F, 0.0F, dz / length * 1.1F);
            RayCastResult knee;
            RayCastResult head;
            const bool low = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(at + Vec3(0.0F, 0.45F - kStandCentre, 0.0F), ahead), knee, {}, {}, not_us);
            const bool high = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(at + Vec3(0.0F, 1.6F - kStandCentre, 0.0F), ahead), head, {}, {}, not_us);
            // And floor where she would come down (about 3 m on): she does not
            // vault a crate stood against the rail into the drop beyond it.
            RayCastResult landing;
            const bool lands = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(at + ahead * 2.7F + Vec3(0.0F, 0.5F, 0.0F), Vec3(0.0F, -(kStandCentre + 1.2F), 0.0F)), landing,
                {}, {}, not_us);
            if (low && !high && lands) {
                velocity = Vec3(dx / length * kVaultForwardMps, kVaultUpMps, dz / length * kVaultForwardMps);
                vault_cooldown_s_ = kVaultCooldownS;
                ++vaults_;
            }
        }
        // Outside the Turn (matron.hpp, getting unstuck): her feet pressed
        // into something in her way, she steps up onto it (under kStepUpM) or
        // clambers onto it (to kClamberMaxM), slower than the Turn's vault;
        // and on the watchdog's first rung she does whichever her feet find,
        // or vaults. A finite push from her feet: the speed up that just
        // carries her feet over its top.
        bool climbed = false;
        if (!turn_moves && !leapt && vault_cooldown_s_ <= 0.0F && length > 0.15F && speed > 0.3F &&
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
                const float rise = top_y - feet + 0.12F;
                velocity = Vec3(heading.GetX() * std::max(along_now, 1.2F), std::sqrt(2.0F * 9.81F * rise),
                                heading.GetZ() * std::max(along_now, 1.2F));
                vault_cooldown_s_ = 0.4F;
                ++step_ups_;
                climbed = true;
            } else if ((pressed || clamber_now_) && ray_ahead(0.45F, 1.0F, f)) {
                const float hit_at = 1.0F * f;
                if (top_ahead(hit_at + 0.3F, top_y) && top_y - feet > kStepUpM - 0.05F &&
                    top_y - feet <= kClamberMaxM) {
                    const float h = top_y - feet;
                    // Room over its top for her: nothing at her height above it.
                    RayCastResult over;
                    const bool room = !physics_.GetNarrowPhaseQuery().CastRay(
                        RRayCast(at + Vec3(0.0F, h + 0.3F - kStandCentre + 1.0F, 0.0F), heading * (hit_at + 0.8F)),
                        over, {}, {}, not_us);
                    if (room) {
                        velocity = Vec3(heading.GetX() * kClamberForwardMps, std::sqrt(2.0F * 9.81F * (h + 0.35F)),
                                        heading.GetZ() * kClamberForwardMps);
                        vault_cooldown_s_ = kClamberCooldownS;
                        ++clambers_;
                        climbed = true;
                    }
                }
            }
            if (!climbed && clamber_now_) {
                // Nothing to clamber onto: the Turn's vault, if what is in her
                // way is lower than her chest and there is floor beyond.
                const Vec3 ahead = heading * 1.1F;
                RayCastResult knee;
                RayCastResult head;
                const bool low = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + Vec3(0.0F, 0.45F - kStandCentre, 0.0F), ahead), knee, {}, {}, not_us);
                const bool high = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + Vec3(0.0F, 1.6F - kStandCentre, 0.0F), ahead), head, {}, {}, not_us);
                RayCastResult landing;
                const bool lands = physics_.GetNarrowPhaseQuery().CastRay(
                    RRayCast(at + ahead * 2.7F + Vec3(0.0F, 0.5F, 0.0F), Vec3(0.0F, -(kStandCentre + 1.2F), 0.0F)),
                    landing, {}, {}, not_us);
                // Outside the Turn never over a gap: floor runs on under it.
                if (low && !high && lands && !edge_at(heading, 0.45F) && !edge_at(heading, 1.5F)) {
                    velocity = Vec3(heading.GetX() * kVaultForwardMps, kVaultUpMps, heading.GetZ() * kVaultForwardMps);
                    vault_cooldown_s_ = kVaultCooldownS;
                    ++vaults_;
                    climbed = true;
                }
            }
        }
        clamber_now_ = false;
        bodies.SetLinearVelocity(body_, velocity);
        // The watchdog: a way ahead of her (more than half a stride off, at a pace).
        watch_stall(dt, at, length > 0.15F ? dx / length : 0.0F, length > 0.15F ? dz / length : 0.0F,
                    speed > 0.3F && length > 0.5F && !climbed, wx, wz);
    } else {
        watch_stall(dt, at, 0.0F, 0.0F, false);
    }
    speed_ = std::hypot(velocity.GetX(), velocity.GetZ());
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
        velocity = Vec3(feet > kDeckTop - 0.6F ? 0.8F : 0.0F, velocity.GetY(), 0.0F);
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
        const float gx = std::clamp((Bay::kBraceX - ax) * 4.0F, -0.6F, 0.6F);
        const float gz = std::clamp((Bay::kBraceZ - az) * 4.0F, -0.6F, 0.6F);
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
        if (climb_ < 0 && level_ != bay_level_) {
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

bool Matron::request_climb_goal(const RVec3 goal, const Vec3 facing) {
    if (!body_in_world_ || dead_ || holding_state(state_) || state_ == State::Down ||
        state_ == State::Staggering || climb_ >= 0 || pipe_phase_ != PipePhase::Off || on_line_ || riding_ ||
        held_ >= 0 || bracing_structure()) return false;
    MatronRoutecraft candidate = routecraft_.replacement();
    const bool accepted = candidate.request(physics_, body_, goal, facing);
    if (accepted || (!routecraft_.planning() && !routecraft_.owns_motion())) routecraft_ = std::move(candidate);
    if (accepted) {
        routecraft_recovering_ = true;
        routecraft_release_floor_ = float(physics_.GetBodyInterface().GetPosition(body_).GetY()) - kStandCentre;
    }
    return accepted;
}

void Matron::pre_step(const float dt, const RVec3 player, const float loudness) {
    clock_s_ += dt;
    acoustic_queries_ = 0;
    since_heard_s_ = std::min(since_heard_s_ + dt, 3600.0F);
    // Waking evidence keeps aging while sealed, but she needs an opportunity
    // to investigate it once she can move. Neither clock refreshes a position.
    if (state_ != State::Dormant && state_ != State::Straining && state_ != State::Emerging) {
        pursuit_s_ = std::min(pursuit_s_ + dt, 3600.0F);
    }
    // On her own journey (travel_to) nothing else has her body: a found
    // climb or a routecraft climb begun beside it gives way to it.
    if (travel_.active && journey_active_) end_journey(false);
    if (travel_.active && (routecraft_.planning() || routecraft_.owns_motion())) {
        routecraft_.cancel();
        routecraft_mine_ = false;
    }
    if (journey_active_) {
        // A way she planned herself: its Climber has her body until it ends.
        if (dead_ || holding_state(state_) || state_ == State::Down || state_ == State::Staggering) {
            end_journey(false);
        } else {
            sound_count_ = 0;
            step_planks(dt);
            step_journey(dt);
            // Her body's travel counts her strides and rungs (each rung rings).
            stride_ += physics_.GetBodyInterface().GetLinearVelocity(body_).Length() * dt;
            sense_near();
            return;
        }
    }
    if (routecraft_.planning() || routecraft_.owns_motion()) {
        if (dead_ || holding_state(state_) || state_ == State::Down || state_ == State::Staggering) routecraft_.cancel();
        else {
            sound_count_ = 0;
            step_planks(dt);
            feel_floor(dt);
            settle_level();
            routecraft_.step(physics_, body_, dt);
            stride_ += physics_.GetBodyInterface().GetLinearVelocity(body_).Length() * dt;   // each rung rings
            if (routecraft_.report().force > 0) routecraft_release_floor_ = float(physics_.GetBodyInterface().GetPosition(body_).GetY()) - kStandCentre;
            // A climb of her own: at its goal (standing over its head or at
            // its foot) her hands come off; ended short of it, the column is
            // left alone a while.
            if (routecraft_mine_ && routecraft_.report().status == MatronRoutecraft::Status::Holding) {
                routecraft_.cancel();
                routecraft_mine_ = false;
            } else if (routecraft_mine_ && !routecraft_.planning() && !routecraft_.owns_motion()) {
                // Acceptance is provisional while the initial search runs.
                // A refused route never took her off the foot of the column.
                const bool refused = !climb_back_tried_ && routecraft_.report().steps == 0;
                block_climb(climbing_key_, !refused);
                routecraft_mine_ = false;
                // Ended short of its goal (blocked at its end, a plan given
                // out) with a hold still in her hands: back down the way she
                // came, once, rather than let go. That failing too, she falls.
                if (refused) {
                    if (climbs_done_ > 0) --climbs_done_;
                } else if (!climb_back_tried_) {
                    climb_back_tried_ = true;
                    routecraft_mine_ = request_climb_goal(climb_back_, climb_back_facing_);
                }
            }
            sense_near();
            return;
        }
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
        ride_line();
        // On the corner pipe her floor is the steel in her hands.
        if (pipe_phase_ == PipePhase::Off) {
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
    if (body_in_world_ && travel_.active) {
        // Knocked off her feet on the way: she gets them back first, then
        // goes on (her journey plans again from where she gets up).
        if (state_ == State::Down || state_ == State::Staggering) {
            if (climber_ && climber_->status() == traversal::Climber::Status::Running) climber_->cancel();
            step_knocked(dt, player);
        } else {
            step_travel(dt);
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
    const bool by_her_pipe =
        body_in_world_ && in_footprint &&
        ((pipe_phase_ != PipePhase::Off && std::abs(player_floor_y - pipe_to_) < 0.1F) ||
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
    const float hearing = body_in_world_ && pipe_phase_ == PipePhase::Off && pipe_listen_s_ > 0.0F ? kListenReach : 1.0F;
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
            state_ != State::Down && state_ != State::Staggering && !riding_ && !on_line_ &&
            climb_ < 0 && pipe_phase_ == PipePhase::Off && held_ < 0;
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
            // In sight: no doubt where the player is.
            take_lead(px, pz, player_floor, heard ? loudness : 0.3F, true);
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
            if (stage_ != Stage::Pest || turn_ != Turn::None || climb_ >= 0 || on_line_ || riding_ ||
                pipe_phase_ != PipePhase::Off || state_ == State::Unrigging || state_ == State::Peering || at_player) {
                drop_stock();
            } else {
                hold_stock(dt);
            }
        }
        if (throw_windup_s_ >= 0.0F && (!saw || held_ < 0 || state_ != State::Hunting)) {
            throw_windup_s_ = -1.0F;
        }
        // On the corner pipe she goes on along it, whatever else she is about.
        if (pipe_phase_ != PipePhase::Off) {
            ride_pipe(dt);
            break;
        }
        // On a climb she is going up (or down) it, whatever else she is about;
        // started along a way across the tower, she finishes it.
        if (climb_ >= 0) {
            climb(dt);
            step_turn(dt, d, loudness, player_carry);
            break;
        }
        if (route_on_edge_) {
            (void)follow_route(dt, at);
            step_turn(dt, d, loudness, player_carry);
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
            const float lose_track = !by_pipe                     ? kLoseTrackSeconds
                                     : stage_ == Stage::Changed ? kPipeChaseChangedSeconds
                                                                : kPipeChaseDangerousSeconds;
            // A cautious approach walks at search speed and may need longer
            // than the chase timer to reach a vantage, but never forever.
            const float evidence_timeout = wary_ ? kAcousticMemorySeconds :
                (lead_seen_ ? lose_track : std::min(lose_track, kAcousticMemorySeconds));
            if (pursuit_s_ > evidence_timeout || (!lead_seen_ && since_heard_s_ > kAcousticMemorySeconds)) {
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
            } else if (elsewhere &&
                       (// After the climber the way he went, where she saw or heard
                        // it: down the shaft's pipe, or a line he took.
                        follow_pipe(dt, at) ||
                        (guess_line_ >= 0 && follow_line(dt, at, false)) ||
                        // Started up a line's ramp, she finishes it: a way found from the ramp
                        // would walk her off its side. Off the ramp, or stalled on it, she is not held.
                        (on_line_ramp(at) && stall_s_ < kRampStallS && follow_line(dt, at, true)) ||
                        // Every other way is her own: bars she finds herself, else
                        // a way she plans over the steel her scan read
                        // (matron_found_climb.cpp), climbed by her own hands.
                        follow_found_climb(dt, at) ||
                        // The authored map's way alone, for the test that drives one of its
                        // ways; in play she has none of it.
                        (found_climbs_off_ && follow_route(dt, at)) ||
                        // A rung way her scan found off her floor; a line she
                        // would only wait at, last.
                        follow_climb(dt, at) ||
                        follow_line(dt, at, true))) {
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
                if (elsewhere) {
                    onto_floor_lines(tx, tz);
                }
                // Steel her map does not contain (the board under a climb she
                // just finished): a step along the real floor, not a straight
                // line off it onto the authored ring. The edge guard still
                // keeps her on what continues under her feet.
                if (off_its_steel(level_, ax, feet_y_, az) && sidestep_s_ <= 0.0F) {
                    const traversal::Ignore ignore{body_, player_, moving_layer()};
                    const std::vector<Vec3> steps =
                        traversal::grid_walk(physics_, ignore, traversal::Limits{}, Vec3(ax, feet_y_, az),
                                             Vec3(tx, feet_y_, tz), 3.0F, 0.35F, 6000);
                    float along = 0.0F;
                    Vec3 prev(ax, feet_y_, az);
                    const IgnoreSingleBodyFilter self(body_);
                    const auto on_steel = [&](const float x, const float z) {
                        RayCastResult down;
                        const RVec3 from(x, feet_y_ + 0.45F, z);
                        if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -0.9F, 0.0F)), down, {}, {},
                                                                    self) ||
                            down.mFraction <= 1.0e-4F) {
                            return false;
                        }
                        const float y = float(from.GetY()) - 0.9F * down.mFraction;
                        return std::abs(y - feet_y_) <= kStepUpM;
                    };
                    // A step counts only when the straight line to it stays on
                    // steel. Her middle may already sit a hand's width past
                    // the lip she is standing on, so the first span is not
                    // judged. A later turn that is nearer across a gap is not
                    // the next step.
                    const auto segment_on = [&](const float x, const float z) {
                        const float span = distance_xz(x, z, ax, az);
                        const int n = std::max(1, static_cast<int>(span / 0.15F));
                        for (int i = 1; i <= n; ++i) {
                            const float t = float(i) / float(n);
                            if (t * span < 0.12F) {
                                continue;
                            }
                            if (!on_steel(ax + (x - ax) * t, az + (z - az) * t)) {
                                return false;
                            }
                        }
                        return on_steel(x, z);
                    };
                    for (const Vec3 &step : steps) {
                        along += distance_xz(step.GetX(), step.GetZ(), prev.GetX(), prev.GetZ());
                        prev = step;
                        if (along < 0.30F || !segment_on(step.GetX(), step.GetZ())) {
                            continue;
                        }
                        float px = step.GetX();
                        float pz = step.GetZ();
                        const float d = distance_xz(px, pz, ax, az);
                        if (d > 1.2F) {
                            const float cx = ax + (px - ax) * (1.2F / d);
                            const float cz = az + (pz - az) * (1.2F / d);
                            if (on_steel(cx, cz)) {
                                px = cx;
                                pz = cz;
                            }
                        }
                        if (!on_steel(px, pz)) {
                            continue;
                        }
                        sidestep_x_ = px;
                        sidestep_z_ = pz;
                        sidestep_s_ = 0.45F;
                        break;
                    }
                }
                route_toward(ax, az, tx, tz, allow, wx, wz, turn_ != Turn::None);
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
    if (dead_ || state_ == State::Down || state_ == State::Staggering || holding_state(state_)) routecraft_.cancel();
    if (body_in_world_ && !dead_ && !routecraft_.owns_motion() && !journey_moving_ && !riding_ && !on_line_ && climb_ < 0 &&
        pipe_phase_ == PipePhase::Off && routecraft_recovering_) {
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
    if (body_in_world_ && !dead_ && !routecraft_.owns_motion() && !on_line_ && climb_ < 0 && pipe_phase_ == PipePhase::Off) {
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
    if (bracing_structure() || !body_in_world_ || dead_ || riding_ || pipe_phase_ != PipePhase::Off || climb_ >= 0 ||
        routecraft_.owns_motion() || journey_moving_ ||
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
        on_line_ = false;
        cable_slides_ = 0;
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
        // Off the pipe, whatever had her on it.
        pipe_support_ = BodyID();
        pipe_drive_dv_ = Vec3::sZero();
        pipe_phase_ = PipePhase::Off;
        pipe_links_ = 0;
        pipe_holds_ = 0;
        pipe_flanges_ = 0;
        pipe_slams_ = 0;
        pipe_slam_mps_ = 0.0F;
        pipe_stun_s_ = 0.0F;
        pipe_listen_s_ = 0.0F;
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
    // On the pipe her own drive is a force the step applies, not a velocity
    // set: what it will change is hers, not a blow's.
    set_velocity_ = bodies.GetLinearVelocity(body_) + (pipe_phase_ != PipePhase::Off ? pipe_drive_dv_ : Vec3::sZero());
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
    const bool on_pipe = pipe_phase_ != PipePhase::Off;
    if (!body_in_world_ || dead_ || riding_ || on_line_ || climb_ >= 0 || travel_.active || state_ == State::Down ||
        state_ == State::Emerging || pipe_phase_ == PipePhase::Slide || pipe_phase_ == PipePhase::Stunned) {
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
            if (distance_xz(float(at.GetX()), float(at.GetZ()), kPipeLineX, kPipeLineZ) < 0.5F) {
                tear_off_pipe();
            } else {
                // Still at a rail, over the band: her hands off it, down
                // onto whatever steel her feet find.
                get_off_pipe(true);
                const Vec3 way = blow_impulse_.LengthSq() > 1.0e-4F ? Vec3(blow_impulse_.GetX(), 0.0F, blow_impulse_.GetZ())
                                                                   : Vec3::sZero();
                fall_x_ = way.LengthSq() > 1.0e-6F ? way.Normalized().GetX() : std::sin(yaw_);
                fall_z_ = way.LengthSq() > 1.0e-6F ? way.Normalized().GetZ() : std::cos(yaw_);
                knock_down();
            }
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
    view_.on_line = on_line_;
    view_.cable_slides = cable_slides_;
    // A planned move counts as climbing once a hand of hers holds (walking
    // to its start, her hands are free).
    const bool journey_move = journey_moving_ && climber_ != nullptr && climber_->view().active;
    const bool journey_hands = journey_move && climber_->view().holding;
    const bool routecraft_hands = routecraft_.owns_motion();
    view_.climbing = climb_ >= 0 || routecraft_hands || journey_hands;
    view_.climb_move_active = journey_move || routecraft_hands || climb_ >= 0 || mantling_;
    view_.climb_bar_traverse = routecraft_hands;
    view_.climb_move = journey_move ? climber_->view().kind
                       : routecraft_hands ? traversal::MoveKind::RungUp
                       : mantling_ ? traversal::MoveKind::Mantle
                       : climb_ >= 0 ? (climb_down_ ? traversal::MoveKind::RungDown : traversal::MoveKind::RungUp)
                       : traversal::MoveKind::Walk;
    view_.climb_phase = journey_move ? climber_->view().phase : (mantling_ ? 3 : 0);
    view_.climb_airborne = journey_move && climber_->view().airborne;
    view_.climb_sliding = journey_move && climber_->view().sliding;
    view_.climb_leg_driving = journey_move && climber_->view().leg_driving;
    view_.climbs = climbs_done_;
    // The hand and force of whichever of her climbs has her.
    view_.climb_hand = routecraft_.owns_motion() ? routecraft_.report().hand
                       : journey_hands           ? climber_->view().hand
                                                 : climb_hand_;
    view_.climb_force_n = pipe_phase_ != PipePhase::Off ? pipe_force_n_
                          : routecraft_.owns_motion()   ? routecraft_.report().force
                          : journey_hands               ? climber_->view().force_n
                                                        : climb_force_n_;
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
    view_.on_pipe = pipe_phase_ != PipePhase::Off;
    view_.route_edges = route_edges_done_;
    view_.route_failures = route_failures_;
    view_.pipe_head_down = pipe_phase_ == PipePhase::Climb && stage_ == Stage::Changed && pipe_to_ < pipe_from_;
    view_.pipe_sliding = pipe_phase_ == PipePhase::Slide;
    view_.pipe_peak_force_n = pipe_peak_force_n_;
    view_.pipe_peak_power_w = pipe_peak_power_w_;
    view_.pipe_stunned = pipe_phase_ == PipePhase::Stunned;
    view_.pipe_listening = pipe_phase_ == PipePhase::Off && pipe_listen_s_ > 0.0F;
    view_.pipe_links = pipe_links_;
    view_.pipe_holds = pipe_holds_;
    view_.pipe_flanges = pipe_flanges_;
    view_.pipe_slams = pipe_slams_;
    view_.pipe_slam_mps = pipe_slam_mps_;
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
    if (routecraft_.owns_motion()) {
        const auto &contact = routecraft_.report();
        view_.hand_x = float(contact.hand.GetX()); view_.hand_y = float(contact.hand.GetY()); view_.hand_z = float(contact.hand.GetZ());
        view_.support_n = contact.force;
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
