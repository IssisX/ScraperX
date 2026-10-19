// The Matron: a sealed locker, a build-up, and a hunt by sound on the 154 m
// deck (see matron.hpp).
#include "sim/matron.hpp"

#include "sim/zipline.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
namespace {
using namespace JPH;

constexpr float kPi = 3.14159265F;
// How far a sound of full loudness carries.
constexpr float kFullHearingM = 55.0F;
constexpr float kTurnRate = 7.0F;

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
               const BodyID player)
    : physics_(physics), moving_layer_(moving_layer), player_(player) {
    shape_ = new CapsuleShape(kHalfHeight, kRadius);
    // Her deck: down the middle of each band, the catwalk across, and in off
    // the north band round the ladder's west rail to its foot; her post in the
    // mouth of Stage A's cage, facing out over the band; the cage's eye on its
    // west face, reached from inside it facing west.
    Level deck{kDeckTop, {}, {}, {}, {}, 0, {-11.35F, kCagesCenterZ, -0.5F * kPi}};
    deck.nodes = {{-kBandX, kBandNorthZ},      {0.0F, kBandNorthZ},          {kBandX, kBandNorthZ},
                  {kBandX, kMidZ},             {kBandX, kBandSouthZ},        {0.0F, kBandSouthZ},
                  {-kBandX, kBandSouthZ},      {-kBandX, kMidZ},             {kLadderSideX, kBandNorthZ},
                  {kLadderSideX, kLadderFootZ}, {kLadderX, kLadderFootZ}};
    const std::vector<Edge> band_loop = {{0, 1, Link::Floor}, {1, 2, Link::Floor}, {2, 3, Link::Floor},
                                         {3, 4, Link::Floor}, {4, 5, Link::Floor}, {5, 6, Link::Floor},
                                         {6, 7, Link::Floor}, {7, 0, Link::Floor}};
    deck.edges = {{0, 1, Link::Floor}, {1, 8, Link::Floor}, {8, 2, Link::Floor}, {2, 3, Link::Floor},
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
    // side and topped out north over its head.
    climbs_ = {{kLadderX, kLadderFootZ, kDeckTop, kRingFloor, 0.0F, 1.0F}};
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
}

void Matron::remove_body() {
    if (!body_in_world_) {
        return;
    }
    physics_.GetBodyInterface().RemoveBody(body_);
    body_in_world_ = false;
}

bool Matron::sees(const RVec3 from, const RVec3 to) const {
    RayCastResult hit;
    const Vec3 run(to - from);
    const float length = run.Length();
    if (length < 0.5F) {
        return true;
    }
    const IgnoreSingleBodyFilter self(body_);
    if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, run), hit, {}, {}, self)) {
        return true;
    }
    // A hit on the player (or just short of them) is the line reaching them.
    return hit.mFraction >= 1.0F - 0.7F / length;
}

int Matron::decks_between(const RVec3 from, const RVec3 to) const {
    const Vec3 run(to - from);
    if (run.LengthSq() < 0.01F) {
        return 0;
    }
    // Neither her body nor her locker: the ear is in one or the other.
    class NotHers final : public BodyFilter {
    public:
        NotHers(const BodyID body, const BodyID locker) : body_(body), locker_(locker) {}
        bool ShouldCollide(const BodyID &id) const override { return id != body_ && id != locker_; }

    private:
        BodyID body_;
        BodyID locker_;
    };
    const NotHers not_hers(body_, locker_body_);
    const RRayCast ray(from, run);
    AllHitCollisionCollector<CastRayCollector> hits;
    physics_.GetNarrowPhaseQuery().CastRay(ray, RayCastSettings(), hits, {}, {}, not_hers);
    // A deck is met through its top or its underside; a column's side, a
    // rail or anything that moves is not one.
    int decks = 0;
    for (const RayCastResult &hit : hits.mHits) {
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded() || !lock.GetBody().IsStatic() || lock.GetBody().IsSensor()) {
            continue;
        }
        const Vec3 normal =
            lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
        decks += std::abs(normal.GetY()) > 0.7F ? 1 : 0;
    }
    return decks;
}

float Matron::carry_m(const RVec3 from, const RVec3 to) const {
    const float straight = Vec3(to - from).Length();
    const int decks = decks_between(from, to);
    if (decks == 0) {
        return straight;
    }
    float through = straight;
    for (int i = 0; i < decks; ++i) {
        through *= kDeckMuffle;
    }
    // Or round by the shaft: out to it level, along it, and in again level.
    const auto in_shaft = [](const RVec3 p) {
        return RVec3(std::clamp(float(p.GetX()), -kShaftHalfM, kShaftHalfM), p.GetY(),
                     std::clamp(float(p.GetZ()), kMidZ - kShaftHalfM, kMidZ + kShaftHalfM));
    };
    const RVec3 out = in_shaft(from);
    const RVec3 in = in_shaft(to);
    const float to_shaft = Vec3(out - from).Length();
    const float from_shaft = Vec3(to - in).Length();
    float round = to_shaft + Vec3(in - out).Length() + from_shaft;
    round *= to_shaft > 0.5F ? kBendMuffle : 1.0F;
    round *= from_shaft > 0.5F ? kBendMuffle : 1.0F;
    return std::min(through, round);
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
        return (e.link != Link::Catwalk || allow_catwalk) && (e.link != Link::Gap || allow_gaps);
    };
    // She stands on a line she may take (the catwalk under her counts, once
    // she is out on it: at its end, on the band, the band's line is hers),
    // and the place she heads for is put on one.
    constexpr float kOnItM = 0.5F;
    const auto insert = [&](const float x, const float z, const bool loop_only) {
        int best = -1;
        float best_d = std::numeric_limits<float>::max();
        float bx = 0.0F;
        float bz = 0.0F;
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if ((loop_only && !allowed(edges[e])) || (edges[e].link == Link::Gap && !allow_gaps)) {
                continue;
            }
            float px = 0.0F;
            float pz = 0.0F;
            float t = 0.0F;
            const Node &a = nodes[static_cast<std::size_t>(edges[e].a)];
            const Node &b = nodes[static_cast<std::size_t>(edges[e].b)];
            project(a.x, a.z, b.x, b.z, x, z, px, pz, t);
            // A line she may not take only wins when she is clearly on it.
            const float d = distance_xz(px, pz, x, z) + (allowed(edges[e]) ? 0.0F : kOnItM);
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
    // Dijkstra over at most a dozen nodes.
    const std::size_t n = nodes.size();
    std::vector<float> dist(n, std::numeric_limits<float>::max());
    std::vector<int> prev(n, -1);
    std::vector<bool> done(n, false);
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
            if (v < 0) {
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
    constexpr float kReachM = 1.2F;
    wx = tx;
    wz = tz;
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        const Node &node = nodes[static_cast<std::size_t>(*it)];
        if (*it == to || distance_xz(fx, fz, node.x, node.z) > kReachM) {
            if (*it != to) {
                wx = node.x;
                wz = node.z;
            }
            return true;
        }
    }
    return true;
}

void Matron::take_lead(const float x, const float z, const float floor, const float loudness, const bool seen) {
    noise_heard_ = loudness;
    heard_x_ = x;
    heard_z_ = z;
    lead_floor_ = floor;
    guess_line_ = -1;
    guess_climb_ = -1;
    since_heard_s_ = 0.0F;
    lead_seen_ = seen;
}

void Matron::stand_down() {
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
    if (level.way < 0 || !ways_out_[static_cast<std::size_t>(level.way)]) {
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
        floor_vy = bodies.GetLinearVelocity(hit.mBodyID).GetY();
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
    for (std::size_t i = 0; i < levels_.size(); ++i) {
        if (static_cast<int>(i) != level_ && std::abs(feet_y_ - levels_[i].floor_y) < 0.6F) {
            level_ = static_cast<int>(i);
            commit_s_ = 0.0F;
            const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
            pick_patrol(float(at.GetX()), float(at.GetZ()));
            return;
        }
    }
}

bool Matron::follow_line(const float dt, const RVec3 at) {
    if (zipline_ == nullptr) {
        return false;
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    // The line from her level down to her lead's: the one she saw taken, or
    // the nearest.
    const std::vector<Zipline::Line> &lines = zipline_->lines();
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Zipline::Line &line = lines[i];
        if (std::abs(float(line.top_floor.GetY()) - floor_y) > 0.6F ||
            std::abs(float(line.bottom_floor.GetY()) - lead_floor_) > 0.6F) {
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
    if (best < 0) {
        return false;
    }
    const Zipline::Line &line = lines[static_cast<std::size_t>(best)];
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
        }
        walk_toward(dt, wx, wz, kHuntMps, turn_ != Turn::None ? kTurnFootAccel : kFootAccel);
        return true;
    }
    // On the stand under the cable, looking down the line. Frail, she cannot
    // follow. In the Turn she jumps for the cable and goes.
    walk_toward(dt, ax, az, 0.0F);
    yaw_ = turn_toward(yaw_, std::atan2(line.direction.GetX(), line.direction.GetZ()), kTurnRate, dt);
    if (turn_ == Turn::None) {
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
    // Her forearms' drag: friction on her weight across the cable, as a
    // squeeze of the line's brake.
    const Zipline::Line &line = zipline_->lines()[static_cast<std::size_t>(ride.line)];
    const float across = std::sqrt(std::max(0.0F, 1.0F - line.direction.GetY() * line.direction.GetY()));
    const float gravity = -physics_.GetGravity().GetY();
    zipline_->set_other_brake(kCableFriction * kMassKg * gravity * across / Zipline::kHandBrakeMaxN);
    // Down at the end (at the stop, or against the trolley there) and
    // still: she lets go, swinging on over the far band as she does.
    if (ride.along_m > line.length - Zipline::kStopMarginM - Zipline::kBufferM && std::abs(ride.speed_mps) < 0.3F) {
        zipline_->unclip_other();
        on_line_ = false;
        BodyInterface &bodies = physics_.GetBodyInterface();
        const Vec3 run = Vec3(line.direction.GetX(), 0.0F, line.direction.GetZ()).NormalizedOr(Vec3::sZero());
        bodies.SetLinearVelocity(body_, bodies.GetLinearVelocity(body_) + run * kLetGoSwingMps);
    }
}

bool Matron::follow_climb(const float dt, const RVec3 at) {
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float level_floor = levels_[static_cast<std::size_t>(level_)].floor_y;
    // The climb from her level up to her lead's: the one she saw taken, or
    // the nearest.
    int best = -1;
    float best_d = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < climbs_.size(); ++i) {
        const Climb &c = climbs_[i];
        if (std::abs(c.foot_y - level_floor) > 0.6F || std::abs(c.head_y - lead_floor_) > 0.6F) {
            continue;
        }
        const float d = static_cast<int>(i) == guess_climb_ ? -1.0F : distance_xz(ax, az, c.x, c.z);
        if (d < best_d) {
            best_d = d;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) {
        return false;
    }
    const Climb &c = climbs_[static_cast<std::size_t>(best)];
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
    // Turn she does, and her hands have her weight.
    walk_toward(dt, c.x, c.z, 0.0F, accel);
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    if (turn_ == Turn::None) {
        return true;
    }
    climb_ = best;
    mantling_ = false;
    ++climbs_done_;
    physics_.GetBodyInterface().SetGravityFactor(body_, 0.0F);
    return true;
}

void Matron::climb(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const Climb &c = climbs_[static_cast<std::size_t>(climb_)];
    const RVec3 at = bodies.GetPosition(body_);
    const float feet = float(at.GetY()) - kStandCentre;
    const Vec3 velocity = bodies.GetLinearVelocity(body_);
    Vec3 want = Vec3::sZero();
    if (!mantling_ && feet < c.head_y + 0.15F) {
        // Up it rung by rung, held to its line: still in a Turn's wind-up, in
        // jerks in its lunges.
        float rate = kClimbMps;
        if (turn_ == Turn::Windup) {
            rate = 0.0F;
        } else if (turn_ == Turn::Lunge &&
                   std::fmod(turn_s_ - kTurnWindupSeconds, kTurnCycleSeconds) < kTurnBurstSeconds) {
            rate = kClimbBurstMps;
        }
        want = Vec3(std::clamp((c.x - float(at.GetX())) * 5.0F, -0.5F, 0.5F), rate,
                    std::clamp((c.z - float(at.GetZ())) * 5.0F, -0.5F, 0.5F));
    } else {
        // Her feet over the floor above: out over its edge, and once her
        // middle is over it, her own weight on her feet again.
        mantling_ = true;
        const float out = (float(at.GetX()) - c.x) * c.out_x + (float(at.GetZ()) - c.z) * c.out_z;
        if (out >= kMantleOutM) {
            bodies.SetGravityFactor(body_, 1.0F);
            climb_ = -1;
            mantling_ = false;
            return;
        }
        want = Vec3(c.out_x * kMantleMps, std::clamp((c.head_y + 0.15F - feet) * 4.0F, 0.0F, 1.0F),
                    c.out_z * kMantleMps);
    }
    Vec3 delta = want - velocity;
    const float limit = kClimbAccel * dt;
    if (delta.Length() > limit) {
        delta = delta.Normalized() * limit;
    }
    const Vec3 moved = velocity + delta;
    bodies.SetLinearVelocity(body_, moved);
    yaw_ = turn_toward(yaw_, std::atan2(c.out_x, c.out_z), kTurnRate, dt);
    speed_ = 0.0F;
    stride_ += moved.Length() * dt;
}

void Matron::walk_toward(const float dt, const float wx, const float wz, const float speed, const float foot_accel,
                         const bool turn_moves) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    target_x_ = wx;
    target_z_ = wz;
    Vec3 velocity = bodies.GetLinearVelocity(body_);
    // Feet drive only on a floor, and not once she has jumped off it (rising
    // off it faster than it rises): a vault or a leap flies until she lands.
    RayCastResult hit;
    const IgnoreSingleBodyFilter self(body_);
    const bool floor = physics_.GetNarrowPhaseQuery().CastRay(
                           RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), hit, {}, {}, self) &&
                       velocity.GetY() < bodies.GetLinearVelocity(hit.mBodyID).GetY() + 1.0F;
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
        const auto floor_at = [&](const Vec3 &dir, const float ahead_m, const float above, const float depth,
                                  float &top) {
            RayCastResult down;
            const RVec3 from = at + dir * ahead_m + Vec3(0.0F, above - kStandCentre, 0.0F);
            if (!physics_.GetNarrowPhaseQuery().CastRay(RRayCast(from, Vec3(0.0F, -depth, 0.0F)), down, {}, {},
                                                        not_us)) {
                return false;
            }
            top = float(from.GetY()) - depth * down.mFraction;
            return true;
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
        bodies.SetLinearVelocity(body_, velocity);
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
        turn_cooldown_s_ = kTurnCooldownSeconds;
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

void Matron::pre_step(const float dt, const RVec3 player, const float loudness) {
    // This tick's sounds, taken now whatever she does with them.
    const std::size_t sound_count = sound_count_;
    sound_count_ = 0;
    step_planks(dt);
    vault_cooldown_s_ = std::max(0.0F, vault_cooldown_s_ - dt);
    look_s_ = std::max(0.0F, look_s_ - dt);
    sees_player_ = false;
    sees_far_ = false;
    noise_heard_ = 0.0F;
    if (dead_) {
        return;
    }
    if (body_in_world_) {
        ride_line();
        feel_floor(dt);
        settle_level();
    }
    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    // On her level (the height of the floor under her, within the tower's
    // footprint) the player can be heard at all: her deck, the ring a cage
    // brought her to, the cage she rides with the player in it.
    const bool on_deck = player.GetY() > feet_y_ - 1.0 && player.GetY() < feet_y_ + 6.0 && std::abs(px) < 27.0F &&
                         pz < -123.0F && pz > -177.0F;
    // Or up a climb from her level (in its column, off its foot's floor):
    // heard and seen up it, and where it goes is her guess.
    int player_climb = -1;
    const float level_floor = levels_[static_cast<std::size_t>(level_)].floor_y;
    for (std::size_t i = 0; i < climbs_.size(); ++i) {
        const Climb &c = climbs_[i];
        if (std::abs(c.foot_y - level_floor) < 0.6F && std::abs(px - c.x) < 1.0F && std::abs(pz - c.z) < 1.0F &&
            player.GetY() > c.foot_y + 1.3F && player.GetY() < c.head_y + 1.2F) {
            player_climb = static_cast<int>(i);
        }
    }
    player_on_deck_ = on_deck || player_climb >= 0;
    // The loudest at her ear of the tick's sounds that carry to it (her
    // locker while she sleeps, her body awake): not her own, carried within
    // `within` and within its own reach. Her lead goes to the level it is on.
    const RVec3 body_at = body_in_world_ ? physics_.GetBodyInterface().GetPosition(body_) : RVec3::sZero();
    const auto pick = [&](const RVec3 ear, const float within) {
        Sound best{};
        float best_share = 0.0F;
        for (std::size_t i = 0; i < sound_count; ++i) {
            const Sound &s = sounds_[i];
            const RVec3 at(s.x, s.y, s.z);
            const float reach = s.loudness * kFullHearingM;
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
            }
        }
        return best;
    };
    // How far the player's noise carries to her ear this tick (none: 0 loud).
    const RVec3 locker_ear(kLockerX, kDeckTop + kLockerHalfY, kLockerZ);
    const RVec3 ear = body_in_world_ ? body_at : locker_ear;
    const float player_carry = loudness > 0.0F && Vec3(player - ear).Length() <= loudness * kFullHearingM
                                   ? carry_m(player, ear)
                                   : std::numeric_limits<float>::max();
    switch (state_) {
    case State::Dormant: {
        const Sound crash = pick(locker_ear, kWakeNoiseM);
        if (crash.heard) {
            noise_heard_ = crash.loudness;
            state_ = State::Straining;
            strain_s_ = 0.0F;
        } else if (player_carry <= kWakeNoiseM && loudness * kFullHearingM >= player_carry) {
            noise_heard_ = loudness;
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
            heard_x_ = px;
            heard_z_ = pz;
        }
        break;
    case State::Emerging:
        emerge_s_ += dt;
        if (emerge_s_ >= kEmergeSeconds) {
            state_ = State::Hunting;
            since_heard_s_ = 0.0F;
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
        const bool heard = loudness * kFullHearingM >= player_carry;
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
        const float player_floor = floor_of(float(player.GetY()));
        if (saw) {
            // In sight: no doubt where the player is.
            take_lead(px, pz, player_floor, heard ? loudness : 0.3F, true);
            wary_ = false;
            state_ = State::Hunting;
        } else if (crash_heard || heard) {
            // The louder sound at her ear: a crash, or the player.
            const bool crash_first =
                crash_heard && (!heard || crash.loudness / std::max(carry_m(RVec3(crash.x, crash.y, crash.z), at), 0.1F) >
                                              loudness / std::max(player_carry, 0.1F));
            const float sx = crash_first ? crash.x : px;
            const float sy = crash_first ? crash.y : float(player.GetY());
            const float sz = crash_first ? crash.z : pz;
            const float sfloor = crash_first ? floor_of(crash.y) : player_floor;
            const float loud = crash_first ? crash.loudness : loudness;
            const bool close = Vec3(RVec3(sx, sy, sz) - at).Length() <= kWaryNoiseM;
            if (state_ == State::Hunting && since_heard_s_ < 1.0F) {
                // The chase goes on by ear: the same lead, moved on.
                take_lead(sx, sz, sfloor, loud, lead_seen_);
                wary_ = wary_ && !close;
            } else if ((state_ == State::Guarding || state_ == State::Unrigging) && fooled_ >= 2 && !close) {
                // Fooled twice: a far noise only turns her head.
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
        } else {
            since_heard_s_ += dt;
        }
        // A guess where the player is going: seen or heard riding a line, to
        // its far end.
        if (zipline_ != nullptr && (saw || heard) && zipline_->riding()) {
            const int line = zipline_->state().line;
            const RVec3 end = zipline_->lines()[static_cast<std::size_t>(line)].bottom_floor;
            heard_x_ = float(end.GetX());
            heard_z_ = float(end.GetZ());
            lead_floor_ = float(end.GetY());
            guess_line_ = line;
        }
        // Or seen or heard on a climb from her level: over its head.
        if (player_climb >= 0 && (saw || heard)) {
            const Climb &c = climbs_[static_cast<std::size_t>(player_climb)];
            heard_x_ = c.x + c.out_x * kMantleOutM;
            heard_z_ = c.z + c.out_z * kMantleOutM;
            lead_floor_ = c.head_y;
            guess_climb_ = player_climb;
        }
        // On a climb she is going up it, whatever else she is about.
        if (climb_ >= 0) {
            climb(dt);
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
        // The catwalk (her deck's) is only ever taken in plain sight of prey
        // ahead, or once she is already on it.
        const bool deck = level_ == 0;
        const bool on_catwalk = deck && on_catwalk_line(ax, az, 0.4F);
        const bool prey_at_catwalk =
            deck && (on_catwalk_line(px, pz, 0.0F) || distance_xz(px, pz, -kBandX, kMidZ) < kEndM ||
                     distance_xz(px, pz, kBandX, kMidZ) < kEndM);
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
            if (!wary_ && since_heard_s_ > kLoseTrackSeconds) {
                if (elsewhere) {
                    // Gone down a line she could not follow: she searches
                    // where she lost them, at its top.
                    if (guess_line_ >= 0 && zipline_ != nullptr) {
                        const RVec3 top = zipline_->lines()[static_cast<std::size_t>(guess_line_)].stand;
                        heard_x_ = float(top.GetX());
                        heard_z_ = float(top.GetZ());
                    }
                    // Gone up a climb she could not take hold of: at its foot.
                    if (guess_climb_ >= 0) {
                        const Climb &c = climbs_[static_cast<std::size_t>(guess_climb_)];
                        heard_x_ = c.x;
                        heard_z_ = c.z;
                    }
                    lead_floor_ = floor_y;
                    guess_line_ = -1;
                    guess_climb_ = -1;
                }
                turn_ = Turn::None;
                turn_s_ = 0.0F;
                state_ = State::Searching;
                search_s_ = 0.0F;
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
            } else if (elsewhere && (follow_line(dt, at) || follow_climb(dt, at))) {
                // After the lead down a line (its stand, its cable) or up a
                // climb (its foot, its rungs).
                step_turn(dt, d, loudness, player_carry);
                break;
            } else {
                // In the Turn a gap is no bar: she leaps it.
                route_toward(ax, az, heard_x_, heard_z_, allow, wx, wz, turn_ != Turn::None);
                step_turn(dt, d, loudness, player_carry);
                if (turn_ == Turn::Windup) {
                    walk_toward(dt, wx, wz, 0.0F, kTurnFootAccel);
                } else if (turn_ == Turn::Lunge) {
                    const bool burst = std::fmod(turn_s_ - kTurnWindupSeconds, kTurnCycleSeconds) < kTurnBurstSeconds;
                    walk_toward(dt, wx, wz, burst ? kTurnBurstMps : kTurnPauseMps, kTurnFootAccel, true);
                } else {
                    walk_toward(dt, wx, wz, kHuntMps);
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
            if (distance_xz(ax, az, heard_x_, heard_z_) > 1.5F) {
                route_toward(ax, az, heard_x_, heard_z_, on_catwalk, wx, wz);
                walk_toward(dt, wx, wz, kSearchMps);
            } else {
                search_s_ += dt;
                walk_toward(dt, ax, az, 0.0F);
                if (search_s_ >= kSearchSeconds) {
                    // Ran to a sound and found nobody: she will not be had so easily again.
                    if (!lead_seen_) {
                        ++fooled_;
                    }
                    stand_down();
                }
            }
            break;
        }
        const Level &here = levels_[static_cast<std::size_t>(level_)];
        const bool has_way = here.way >= 0;
        const std::size_t way = has_way ? static_cast<std::size_t>(here.way) : 0;
        if (state_ == State::Guarding && has_way && rigged_[way] && ways_out_[way] &&
            route_toward(ax, az, here.eye.x, here.eye.z, on_catwalk, wx, wz)) {
            // The way out rigged to ride, and the player not here: she goes
            // to undo it.
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
                    // from it, or toward a far noise she let go.
                    walk_toward(dt, ax, az, 0.0F);
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
    case State::Dead:
        break;
    }
}

void Matron::post_step(const float dt) {
    (void)dt;
    if (body_in_world_ && !dead_) {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        if (at.GetY() < levels_[static_cast<std::size_t>(level_)].floor_y - kFallenBelowM) {
            dead_ = true;
            state_ = State::Dead;
            remove_body();
        }
    }
    refresh_view();
}

bool Matron::caught(const RVec3 player) const noexcept {
    // Clinging to a moving floor, her hands are on the rail.
    if (!body_in_world_ || dead_ || riding_ ||
        (state_ != State::Hunting && state_ != State::Searching && state_ != State::Shuffling &&
         state_ != State::Guarding && state_ != State::Peering && state_ != State::Unrigging)) {
        return false;
    }
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    return distance_xz(float(at.GetX()), float(at.GetZ()), float(player.GetX()), float(player.GetZ())) < kCatchM &&
           std::abs(at.GetY() - player.GetY()) < 1.4;
}

void Matron::restore(const Saved &saved) {
    if (!dead_) {
        remove_body();
        state_ = saved.state;
        strain_s_ = saved.strain_s;
        speed_ = 0.0F;
        since_heard_s_ = 0.0F;
        turn_ = Turn::None;
        turn_s_ = 0.0F;
        turn_cooldown_s_ = kTurnCooldownSeconds * 0.5F;
        sight_far_s_ = 0.0F;
        // A new run: what she learned of the player's tricks is forgotten.
        fooled_ = 0;
        wary_ = false;
        lead_seen_ = false;
        peer_s_ = 0.0F;
        look_s_ = 0.0F;
        vault_cooldown_s_ = 0.0F;
        vaults_ = 0;
        leaps_ = 0;
        unrigs_ = 0;
        guess_line_ = -1;
        lead_floor_ = kDeckTop;
        on_line_ = false;
        cable_slides_ = 0;
        guess_climb_ = -1;
        climb_ = -1;
        mantling_ = false;
        climbs_done_ = 0;
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
        if (state_ == State::Emerging || state_ == State::Hunting || state_ == State::Searching ||
            state_ == State::Shuffling || state_ == State::Guarding || state_ == State::Peering ||
            state_ == State::Unrigging) {
            // Back at her locker, as she burst out of it.
            spawn_body();
            state_ = State::Emerging;
            emerge_s_ = 0.4F;
        }
        restore_planks();
    }
    refresh_view();
}

void Matron::refresh_view() {
    view_.state = state_;
    view_.alive = !dead_;
    view_.released = state_ == State::Emerging || state_ == State::Hunting || state_ == State::Searching ||
                     state_ == State::Shuffling || state_ == State::Guarding || state_ == State::Peering ||
                     state_ == State::Unrigging;
    view_.fooled = fooled_;
    view_.wary = wary_ || state_ == State::Peering;
    view_.vaults = vaults_;
    view_.leaps = leaps_;
    view_.unrigs = unrigs_;
    view_.on_line = on_line_;
    view_.cable_slides = cable_slides_;
    view_.climbing = climb_ >= 0;
    view_.climbs = climbs_done_;
    view_.clinging = riding_;
    view_.floor_y = levels_[static_cast<std::size_t>(level_)].floor_y;
    view_.lead_floor_y = lead_floor_;
    view_.strain = state_ == State::Dormant ? 0.0F : std::min(1.0F, strain_s_ / kStrainSeconds);
    if (view_.released) {
        view_.strain = 1.0F;
    }
    view_.bolts_popped = std::floor(view_.strain * 4.0F);
    view_.speed = speed_;
    view_.stride = stride_;
    view_.yaw = yaw_;
    view_.noise_heard = noise_heard_;
    view_.sees_player = sees_player_;
    view_.target_x = target_x_;
    view_.target_z = target_z_;
    view_.catwalk_allowed = allow_catwalk_;
    view_.turn = turn_;
    view_.turn_s = turn_s_;
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
