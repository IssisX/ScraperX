// Life L2: the yard crew (see crew.hpp).
#include "sim/crew.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;

constexpr float kPi = 3.14159265F;

// Turn toward `want` at no more than `rate` rad/s.
float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * kPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, 2.0F * kPi);
}

float yaw_of(const Vec3 direction) { return std::atan2(direction.GetX(), direction.GetZ()); }

Vec3 flat(const Vec3 v) { return Vec3(v.GetX(), 0.0F, v.GetZ()); }

}  // namespace

Crew::Crew(PhysicsSystem &physics, const std::uint32_t moving_layer)
    : physics_(physics), moving_layer_(moving_layer) {
    shape_ = new CapsuleShape(kHalfHeight, kRadius);
    // The yard crew, on grade between the game's start and the tower's base.
    // Routes keep to lanes clear of every machine and stand (mapped with the
    // native's own surface probe): the slingshot's posts stand about x -7..1,
    // z -52..-54; the pipe gallery and trapeze stands x 16..30, z -28..-42; a
    // shed at x -14..-2, z -110..-114. The yard's two kerbs (0.28 m, at z -16
    // and z -60) are stepped over. Yaw 0 faces +z (the start), pi the tower.
    // The foreman: from the start's edge down the west lane to the base, and back.
    add({{-4.0F, -12.0F, 4.0F, kPi},
         {-20.0F, -22.0F, 0.0F, 0.0F},
         {-20.0F, -90.0F, 0.0F, 0.0F},
         {-8.0F, -96.0F, 6.0F, kPi},
         {-20.0F, -90.0F, 0.0F, 0.0F},
         {-20.0F, -22.0F, 0.0F, 0.0F}},
        1.35F);
    // Two at the slingshot, one either side, checking its posts and bands.
    add({{6.0F, -50.5F, 14.0F, -kPi * 0.5F}, {6.5F, -56.5F, 9.0F, -kPi * 0.5F}}, 1.2F);
    add({{-12.0F, -50.0F, 11.0F, kPi * 0.5F}, {-12.0F, -57.0F, 7.0F, kPi * 0.5F}}, 1.25F);
    // A rigger at the base, east of the first lift.
    add({{14.0F, -104.0F, 10.0F, kPi}, {20.0F, -100.0F, 6.0F, kPi * 0.5F}, {14.0F, -98.0F, 4.0F, kPi}}, 1.3F);
    // A runner between the start and the base, down the far west lane.
    add({{-30.0F, -20.0F, 3.0F, kPi}, {-30.0F, -100.0F, 4.0F, 0.0F}}, 1.45F);
    // Someone watching the trapeze from beside its plane, now and then pacing.
    add({{36.0F, -20.0F, 16.0F, kPi}, {37.5F, -36.0F, 8.0F, 0.0F}}, 1.15F);
}

Crew::~Crew() {
    BodyInterface &bodies = physics_.GetBodyInterface();
    for (const Worker &worker : workers_) {
        bodies.RemoveBody(worker.body);
        bodies.DestroyBody(worker.body);
    }
}

void Crew::add(std::vector<Station> route, const float walk_mps) {
    if (workers_.size() >= kMaxWorkers || route.empty()) {
        return;
    }
    Worker worker;
    worker.route = std::move(route);
    worker.walk_mps = walk_mps;
    const Station &home = worker.route.front();
    BodyCreationSettings settings(shape_, RVec3(home.x, kStandCentre, home.z), Quat::sIdentity(),
                                  EMotionType::Dynamic, static_cast<ObjectLayer>(moving_layer_));
    settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
    settings.mAllowSleeping = false;
    settings.mFriction = 0.0F;
    settings.mLinearDamping = 0.0F;
    settings.mUserData = kWorkerEntityBase + workers_.size();
    settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = kMassKg;
    worker.body = physics_.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
    worker.next = worker.route.size() > 1 ? 1 : 0;
    worker.view.x = home.x;
    worker.view.y = kStandCentre;
    worker.view.z = home.z;
    worker.view.yaw = home.face_yaw;
    // Start at the first station's work, so the yard is busy from the start.
    worker.view.state = home.dwell_s > 0.0F ? State::Working : State::Walking;
    worker.timer = home.dwell_s * 0.5F;
    worker.next = 0;
    if (worker.view.state == State::Walking) {
        worker.next = worker.route.size() > 1 ? 1 : 0;
    }
    workers_.push_back(std::move(worker));
}

bool Crew::is_worker(const BodyID body) const noexcept {
    for (const Worker &worker : workers_) {
        if (worker.body == body) {
            return true;
        }
    }
    return false;
}

bool Crew::grounded(const Worker &worker, const BodyInterface &bodies) const {
    const RVec3 centre = bodies.GetPosition(worker.body);
    const RRayCast ray(centre, Vec3(0.0F, -(kStandCentre + 0.12F), 0.0F));
    RayCastResult hit;
    const IgnoreSingleBodyFilter filter(worker.body);
    return physics_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, filter);
}

void Crew::pre_step(const float dt, const RVec3 player, const Vec3 player_velocity, const bool player_grounded) {
    if (!(dt > 0.0F)) {
        return;
    }
    BodyInterface &bodies = physics_.GetBodyInterface();
    // A hard landing: the player was falling at kThudMps or more and is down.
    const bool thud = player_grounded && last_player_vy_ <= -kThudMps;
    last_player_vy_ = player_grounded ? 0.0F : player_velocity.GetY();
    const float player_speed = flat(player_velocity).Length();

    for (Worker &worker : workers_) {
        const RVec3 at = bodies.GetPosition(worker.body);
        const Vec3 velocity = bodies.GetLinearVelocity(worker.body);
        const Vec3 to_player = flat(Vec3(player - at));
        const float distance = to_player.Length();
        const bool on_ground = grounded(worker, bodies);
        View &view = worker.view;
        view.grounded = on_ground;

        // ---- what the player is doing near it --------------------------------
        const bool calm = view.state == State::Walking || view.state == State::Working;
        // How fast the player is coming at it.
        const float closing = distance > 0.01F ? flat(player_velocity).Dot(-to_player) / distance : 0.0F;
        if (view.state != State::Stumbling) {
            const bool running_at = player_speed >= kRunMps && distance < kRunStartleM &&
                                    flat(player_velocity).Dot(-to_player) > 0.0F;
            const bool landed_near = thud && distance < kThudM;
            if ((running_at || landed_near) && view.state != State::Startled) {
                if (calm) {
                    worker.resume = view.state;
                }
                view.state = State::Startled;
                worker.timer = 1.6F;
                ++startles_;
            } else if (calm && player_speed > 0.4F && player_speed < kRunMps &&
                       closing > 0.0F && distance < std::max(kYieldM, closing * kYieldLeadS)) {
                // A walking player coming at it: step off their line, to the
                // side the player is not already passing.
                const Vec3 across(-player_velocity.GetZ(), 0.0F, player_velocity.GetX());
                const Vec3 side = across.Normalized() * (across.Dot(-to_player) >= 0.0F ? 1.0F : -1.0F);
                worker.aside = flat(Vec3(at)) + side * kYieldStepM;
                worker.resume = view.state;
                view.state = State::Yielding;
                worker.timer = 0.6F;
                ++yields_;
            }
        }

        // ---- what the feet ask for -------------------------------------------
        Vec3 want = Vec3::sZero();
        float face = view.yaw;
        switch (view.state) {
        case State::Walking: {
            const Station &target = worker.route[worker.next];
            const Vec3 to = Vec3(target.x - float(at.GetX()), 0.0F, target.z - float(at.GetZ()));
            const float left = to.Length();
            if (left < 0.35F) {
                if (target.dwell_s > 0.0F) {
                    view.state = State::Working;
                    worker.timer = target.dwell_s;
                } else {
                    worker.next = (worker.next + 1) % worker.route.size();
                }
            } else {
                // Slowing into a station over the last metre.
                want = to / left * worker.walk_mps * std::min(1.0F, 0.4F + left);
                face = yaw_of(to);
            }
            break;
        }
        case State::Working: {
            face = worker.route[worker.next].face_yaw;
            worker.timer -= dt;
            if (worker.timer <= 0.0F) {
                worker.next = (worker.next + 1) % worker.route.size();
                view.state = State::Walking;
            }
            break;
        }
        case State::Yielding: {
            const Vec3 to = worker.aside - flat(Vec3(at));
            if (to.Length() > 0.15F) {
                want = to.Normalized() * std::min(kDodgeMps, to.Length() * 3.0F);
            }
            face = distance > 0.1F ? yaw_of(to_player) : view.yaw;
            // Watching them by until they are well past.
            if (distance > kYieldM + 0.6F) {
                worker.timer -= dt;
                if (worker.timer <= 0.0F) {
                    view.state = worker.resume;
                }
            }
            break;
        }
        case State::Startled: {
            // A step back from the player, staring at them.
            if (worker.timer > 1.2F && distance > 0.1F) {
                want = -to_player / distance * 1.0F;
            }
            face = distance > 0.1F ? yaw_of(to_player) : view.yaw;
            worker.timer -= dt;
            if (worker.timer <= 0.0F) {
                view.state = worker.resume;
            }
            break;
        }
        case State::Stumbling: {
            worker.timer -= dt;
            if (worker.timer <= 0.0F) {
                view.state = worker.resume;
            }
            break;
        }
        }
        view.yaw = turn_toward(view.yaw, face, view.state == State::Startled ? 9.0F : 4.0F, dt);

        // A kerb or step ahead, knee height clear: step up onto it -- the
        // feet lift the body (kStepLiftMps up, once), the walk carries on.
        worker.step_cooldown = std::max(0.0F, worker.step_cooldown - dt);
        float vertical = velocity.GetY();
        if (on_ground && worker.step_cooldown <= 0.0F && want.LengthSq() > 0.04F &&
            std::abs(velocity.GetY()) < 0.3F) {
            const Vec3 ahead = want.Normalized() * (kRadius + 0.45F);
            const RVec3 feet = at - Vec3(0.0F, kStandCentre, 0.0F);
            RayCastResult low;
            RayCastResult knee;
            const IgnoreSingleBodyFilter self(worker.body);
            const bool blocked = physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(feet + Vec3(0.0F, 0.12F, 0.0F), ahead), low, {}, {}, self);
            const bool clear_above = !physics_.GetNarrowPhaseQuery().CastRay(
                RRayCast(feet + Vec3(0.0F, kStepMaxM + 0.05F, 0.0F), ahead), knee, {}, {}, self);
            if (blocked && clear_above) {
                vertical = kStepLiftMps;
                worker.step_cooldown = 0.6F;
                ++steps_;
            }
        }

        // Feet drive only on the ground, and never past kFootAccel; a
        // stumbling worker's feet only try to stop it.
        const Vec3 horizontal = flat(velocity);
        Vec3 next = horizontal;
        if (on_ground) {
            const float accel = view.state == State::Stumbling ? kFootAccel * 0.4F
                                : view.state == State::Yielding ? kDodgeAccel
                                                                : kFootAccel;
            const Vec3 target = view.state == State::Stumbling ? Vec3::sZero() : want;
            const Vec3 change = target - horizontal;
            const float most = accel * dt;
            next = horizontal + (change.Length() > most ? change.Normalized() * most : change);
            bodies.SetLinearVelocity(worker.body, Vec3(next.GetX(), vertical, next.GetZ()));
        }
        worker.drive = next;
    }
}

void Crew::post_step(const float dt) {
    const BodyInterface &bodies = physics_.GetBodyInterface();
    for (Worker &worker : workers_) {
        const RVec3 at = bodies.GetPosition(worker.body);
        const Vec3 velocity = bodies.GetLinearVelocity(worker.body);
        View &view = worker.view;
        const Vec3 horizontal = flat(velocity);
        // Shoved: moving otherwise than its feet drove it.
        if (view.state != State::Stumbling && (horizontal - worker.drive).Length() > kShoveMps) {
            if (view.state == State::Walking || view.state == State::Working) {
                worker.resume = view.state;
            }
            view.state = State::Stumbling;
            worker.timer = 1.2F;
            ++stumbles_;
        }
        view.speed = horizontal.Length();
        view.stride += view.speed * dt;
        view.x = float(at.GetX());
        view.y = float(at.GetY());
        view.z = float(at.GetZ());
    }
}

}  // namespace scraperx::sim
