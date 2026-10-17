// The Matron: a sealed locker, a build-up, and a hunt by sound on the 154 m
// deck (see matron.hpp).
#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
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
    // The walking lines: down the middle of each band, the catwalk across.
    nodes_ = {{-kBandX, kBandNorthZ}, {0.0F, kBandNorthZ}, {kBandX, kBandNorthZ}, {kBandX, kMidZ},
              {kBandX, kBandSouthZ},  {0.0F, kBandSouthZ}, {-kBandX, kBandSouthZ}, {-kBandX, kMidZ}};
    edges_ = {{0, 1, false}, {1, 2, false}, {2, 3, false}, {3, 4, false}, {4, 5, false},
              {5, 6, false}, {6, 7, false}, {7, 0, false}, {7, 3, true}};
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

bool Matron::on_catwalk_line(const float x, const float z, const float margin) {
    return std::abs(z - kMidZ) < 1.4F && std::abs(x) < 17.6F + margin;
}

void Matron::route_toward(const float fx, const float fz, const float tx, const float tz, const bool allow_catwalk,
                          float &wx, float &wz) const {
    std::vector<Node> nodes = nodes_;
    std::vector<Edge> edges = edges_;
    const auto insert = [&](const float x, const float z, const bool loop_only) {
        int best = -1;
        float best_d = std::numeric_limits<float>::max();
        float bx = 0.0F;
        float bz = 0.0F;
        for (std::size_t e = 0; e < edges.size(); ++e) {
            if (loop_only && edges[e].catwalk) {
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
        edges[static_cast<std::size_t>(best)] = {old.a, fresh, old.catwalk};
        edges.push_back({fresh, old.b, old.catwalk});
        return fresh;
    };
    const int from = insert(fx, fz, false);
    const int to = insert(tx, tz, !allow_catwalk);
    // Dijkstra over at most ten nodes.
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
            if (e.catwalk && !allow_catwalk) {
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
        return;
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
            return;
        }
    }
}

void Matron::walk_toward(const float dt, const float wx, const float wz, const float speed, const float foot_accel) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    target_x_ = wx;
    target_z_ = wz;
    Vec3 velocity = bodies.GetLinearVelocity(body_);
    // Feet drive only on a floor.
    RayCastResult hit;
    const IgnoreSingleBodyFilter self(body_);
    const bool floor = physics_.GetNarrowPhaseQuery().CastRay(
        RRayCast(at, Vec3(0.0F, -(kStandCentre + 0.3F), 0.0F)), hit, {}, {}, self);
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
        bodies.SetLinearVelocity(body_, velocity);
    }
    speed_ = std::hypot(velocity.GetX(), velocity.GetZ());
    stride_ += speed_ * dt;
}

// The Turn's clock and its two triggers (see matron.hpp). Only simulation
// state decides: her distance, the line, the player's landing, the cooldown.
void Matron::step_turn(const float dt, const float distance_to_player, const float loudness) {
    sight_far_s_ = sees_far_ ? sight_far_s_ + dt : 0.0F;
    if (turn_ == Turn::None) {
        turn_cooldown_s_ = std::max(0.0F, turn_cooldown_s_ - dt);
        const bool close_and_clear = sight_far_s_ >= kTurnSightSeconds && distance_to_player <= kTurnTriggerM;
        const bool hard_landing = loudness >= kTurnNoiseLoudness && distance_to_player <= kTurnNoiseM;
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
    step_planks(dt);
    sees_player_ = false;
    sees_far_ = false;
    noise_heard_ = 0.0F;
    if (dead_) {
        return;
    }
    const float px = float(player.GetX());
    const float pz = float(player.GetZ());
    // On the deck (its height, its footprint) the player can be heard at all.
    const bool on_deck = player.GetY() > kDeckTop - 1.0 && player.GetY() < kDeckTop + 6.0 && std::abs(px) < 27.0F &&
                         pz < -123.0F && pz > -177.0F;
    player_on_deck_ = on_deck;
    const Crash crash = crash_;
    crash_ = {};
    const bool crash_on_deck = crash.heard && crash.y > kDeckTop - 1.0F && crash.y < kDeckTop + 6.0F;
    switch (state_) {
    case State::Dormant: {
        const float d = distance_xz(px, pz, kLockerX, kLockerZ);
        const float crash_d = distance_xz(crash.x, crash.z, kLockerX, kLockerZ);
        if (crash_on_deck && crash_d <= kWakeNoiseM && crash.loudness * kFullHearingM >= crash_d) {
            noise_heard_ = crash.loudness;
            state_ = State::Straining;
            strain_s_ = 0.0F;
        } else if (on_deck && loudness > 0.0F && d <= kWakeNoiseM && loudness * kFullHearingM >= d) {
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
    case State::Shuffling: {
        const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
        const float ax = float(at.GetX());
        const float az = float(at.GetZ());
        const float d = distance_xz(ax, az, px, pz);
        bool heard = on_deck && loudness > 0.0F && loudness * kFullHearingM >= d;
        bool saw = false;
        if (on_deck && d <= kSightM) {
            saw = sees(at + RVec3(0.0, -0.5, 0.0), player - RVec3(0.0, 0.3, 0.0));
        }
        sees_player_ = saw;
        // The straight line across the shaft: plain sight at long range.
        // (Low rays: a guard rail is bars, not a wall.)
        sees_far_ = on_deck && d <= kCommitSightM &&
                    (saw || sees(at + RVec3(0.0, -0.5, 0.0), player - RVec3(0.0, 0.3, 0.0)));
        // A crash on the deck louder than the player draws her to it instead.
        const bool crash_heard = crash_on_deck && crash.loudness * kFullHearingM >= distance_xz(ax, az, crash.x, crash.z);
        if (crash_heard && !saw && (!heard || crash.loudness > loudness)) {
            noise_heard_ = crash.loudness;
            heard_x_ = crash.x;
            heard_z_ = crash.z;
            since_heard_s_ = 0.0F;
            state_ = State::Hunting;
        } else if (heard || saw) {
            noise_heard_ = heard ? loudness : 0.3F;
            heard_x_ = px;
            heard_z_ = pz;
            since_heard_s_ = 0.0F;
            if (state_ != State::Hunting) {
                state_ = State::Hunting;
            }
        } else {
            since_heard_s_ += dt;
        }
        // The catwalk is only ever taken in plain sight of prey ahead, or once
        // she is already on it.
        const bool on_catwalk = on_catwalk_line(ax, az, 0.4F);
        const bool prey_at_catwalk =
            on_catwalk_line(px, pz, 0.0F) ||
            distance_xz(px, pz, -kBandX, kMidZ) < kEndM || distance_xz(px, pz, kBandX, kMidZ) < kEndM;
        // Once she has decided for the catwalk she keeps to it a moment.
        // At the catwalk's end she is off its axis as often as on it, and a rail
        // post can hide the prey on the plates: a sound a moment ago counts as sight there.
        const bool at_end = distance_xz(ax, az, -kBandX, kMidZ) < kEndM + 3.0F ||
                            distance_xz(ax, az, kBandX, kMidZ) < kEndM + 3.0F;
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
            if (since_heard_s_ > kLoseTrackSeconds) {
                turn_ = Turn::None;
                turn_s_ = 0.0F;
                state_ = State::Searching;
                search_s_ = 0.0F;
            } else {
                route_toward(ax, az, heard_x_, heard_z_, allow, wx, wz);
                step_turn(dt, d, loudness);
                if (turn_ == Turn::Windup) {
                    walk_toward(dt, wx, wz, 0.0F, kTurnFootAccel);
                } else if (turn_ == Turn::Lunge) {
                    const bool burst = std::fmod(turn_s_ - kTurnWindupSeconds, kTurnCycleSeconds) < kTurnBurstSeconds;
                    walk_toward(dt, wx, wz, burst ? kTurnBurstMps : kTurnPauseMps, kTurnFootAccel);
                } else {
                    walk_toward(dt, wx, wz, kHuntMps);
                }
                break;
            }
        }
        if (state_ == State::Searching) {
            if (distance_xz(ax, az, heard_x_, heard_z_) > 1.5F) {
                route_toward(ax, az, heard_x_, heard_z_, on_catwalk, wx, wz);
                walk_toward(dt, wx, wz, kSearchMps);
            } else {
                search_s_ += dt;
                walk_toward(dt, ax, az, 0.0F);
                if (search_s_ >= kSearchSeconds) {
                    state_ = State::Shuffling;
                }
            }
            break;
        }
        // Shuffling: the stooped patrol round the loop.
        const Node &next = nodes_[static_cast<std::size_t>(patrol_node_)];
        if (distance_xz(ax, az, next.x, next.z) < 1.5F) {
            patrol_node_ = (patrol_node_ + 1) % 8;
        }
        const Node &goal = nodes_[static_cast<std::size_t>(patrol_node_)];
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
        if (at.GetY() < kDeckTop - kFallenBelowM) {
            dead_ = true;
            state_ = State::Dead;
            remove_body();
        }
    }
    refresh_view();
}

bool Matron::caught(const RVec3 player) const noexcept {
    if (!body_in_world_ || dead_ || (state_ != State::Hunting && state_ != State::Searching && state_ != State::Shuffling)) {
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
        if (state_ == State::Emerging || state_ == State::Hunting || state_ == State::Searching ||
            state_ == State::Shuffling) {
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
                     state_ == State::Shuffling;
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
