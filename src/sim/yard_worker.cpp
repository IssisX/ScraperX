#include "sim/yard_worker.hpp"

#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Physics/Body/BodyLock.h>
#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
JPH::Vec3 flat(JPH::Vec3 value) { value.SetY(0); return value; }
double yaw(JPH::Vec3 direction) { return std::atan2(-direction.GetX(), -direction.GetZ()); }
double turn(double current, double target, double dt) {
    constexpr double pi = 3.14159265358979323846;
    return std::remainder(current + std::clamp(std::remainder(target-current, 2*pi),
        -2.0*dt, 2.0*dt), 2*pi);
}
}

YardWorker::YardWorker(JPH::PhysicsSystem &physics, JPH::ObjectLayer layer, std::uint64_t entity)
    : physics_(physics), actor_(physics, layer, entity) {}
bool YardWorker::spawn() { return actor_.spawn(home()); }
void YardWorker::enter(Activity activity) noexcept {
    if (state_.activity == activity) return;
    state_.activity = activity;
    state_.seconds = 0;
}

void YardWorker::pre_step(float dt, JPH::RVec3 player, JPH::Vec3 player_velocity, bool visible) {
    if (!std::isfinite(dt) || dt <= 0 || dt > .05F) return;
    ++tick_;
    state_.seconds += dt;
    const auto body = actor_.readback();
    const auto to_player = flat(JPH::Vec3(player-body.position));
    const auto to_home = flat(JPH::Vec3(home()-body.position));
    const float distance = to_player.Length();
    const bool near = visible && distance < 2.2F && std::abs(player.GetY()-body.position.GetY()) < 1.5;
    const auto velocity = flat(body.velocity);
    if (distance > 5.0F) state_.greeted = false;
    JPH::Vec3 desired = JPH::Vec3::sZero();
    double heading = state_.heading;
    if (!supported_) {
        enter(Activity::Recovering);
    } else if (velocity.Length() > PhysicalStoryActor::speed_bound_mps()+.5F &&
               state_.activity != Activity::Recovering) {
        // A real shove interrupts the work. Only supported finite braking
        // acts; there is no reset, artificial damping or velocity assignment.
        enter(Activity::Recovering);
    } else if (state_.activity == Activity::Recovering && state_.seconds > .45) {
        enter(to_home.Length() > .15F ? Activity::Returning : Activity::Working);
    }
    const float closing = distance > .01F ? flat(player_velocity).Dot(-to_player)/distance : 0;
    if (supported_ && state_.activity != Activity::Recovering &&
        state_.activity != Activity::Yielding && near && distance < 1.05F && closing > .3F) {
        auto side = JPH::Vec3(-player_velocity.GetZ(), 0, player_velocity.GetX()).NormalizedOr(JPH::Vec3::sAxisX());
        if (side.Dot(-to_player) < 0) side = -side;
        state_.aside = JPH::Vec3(body.position) + side*.7F;
        enter(Activity::Yielding);
    }
    switch (state_.activity) {
    case Activity::Working:
        look_target_ = work_point();
        heading = 0;
        if (to_home.Length() > .2F) enter(Activity::Returning);
        else if (near && !state_.greeted) enter(Activity::Attending);
        break;
    case Activity::Attending:
        look_target_ = player + JPH::Vec3(0,.62F,0);
        if (to_player.LengthSq() > .001F) heading = yaw(to_player);
        if (!near && distance > 3.0F) enter(Activity::Returning);
        else if (!state_.greeted && state_.seconds > .8) {
            state_.greeted = true;
            enter(Activity::Pointing);
            look_target_ = guide_point();
        } else if (state_.greeted && state_.seconds > 3.0) enter(Activity::Returning);
        break;
    case Activity::Pointing:
        look_target_ = guide_point();
        if (to_player.LengthSq() > .001F) heading = yaw(to_player);
        if (state_.seconds > 2.0 || distance > 4.5F) enter(Activity::Attending);
        break;
    case Activity::Returning:
        look_target_ = work_point();
        if (to_home.Length() > .12F) {
            desired = to_home.Normalized()*std::min(.9F,to_home.Length()*2.0F);
            heading = yaw(to_home);
        } else if (velocity.Length() < .15F) enter(Activity::Working);
        break;
    case Activity::Yielding: {
        const auto to = flat(state_.aside-JPH::Vec3(body.position));
        if (to.Length() > .1F) desired = to.Normalized()*std::min(1.2F,to.Length()*2.5F);
        look_target_ = player+JPH::Vec3(0,.62F,0);
        if (to_player.LengthSq() > .001F) heading = yaw(to_player);
        if (state_.seconds > .9) enter(near ? Activity::Attending : Activity::Returning);
        break;
    }
    case Activity::Recovering:
        look_target_ = player+JPH::Vec3(0,.62F,0);
        break;
    }
    state_.heading = turn(state_.heading,heading,dt);
    actor_.pre_step(tick_,dt,desired);
}

void YardWorker::finish_step(const PhysicalStoryActor::Contact *contact) {
    supported_ = false;
    support_entity_ = 0;
    if (!contact || contact->actor != actor_.body_id() || contact->solved_tick != tick_) {
        actor_.clear_contact();
        return;
    }
    actor_.submit_solved_contact(*contact);
    const JPH::BodyLockRead lock(physics_.GetBodyLockInterface(),contact->support);
    if (lock.Succeeded()) {
        supported_ = true;
        support_entity_ = lock.GetBody().GetUserData();
    }
}
YardWorker::View YardWorker::view() const {
    return {actor_.readback(), state_.activity, state_.seconds, state_.heading,
        tick_, episode_, supported_, support_entity_, work_point(), guide_point(), look_target_};
}
void YardWorker::restore(const State &state) {
    state_ = state;
    actor_.clear_contact();
    supported_ = false;
    support_entity_ = 0;
    ++episode_;
}
} // namespace scraperx::sim
#endif
