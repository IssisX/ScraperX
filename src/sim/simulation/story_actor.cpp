#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::PhysicsWorld::story_actor_line_of_sight(Vector3 eye) const noexcept {
    if (!story_actor_ || !std::isfinite(eye.x) || !std::isfinite(eye.y) || !std::isfinite(eye.z)) return false;
    const auto actor = story_actor_->readback();
    if (!actor.present) return false;
    const JPH::RVec3 origin(eye.x,eye.y,eye.z);
    const JPH::Vec3 segment(actor.position+JPH::Vec3(0,.72F,0)-origin);
    if (segment.LengthSq() > 36 || segment.LengthSq() < 1.0e-8F) return false;
    JPH::RayCastResult hit;
    // Visibility is a prospective native geometry query, never footing. The
    // player capsule is the only exclusion; the first hit must be this actor.
    return cast_ray(origin,segment,hit) && hit.mBodyID == story_actor_->body_id();
}

StoryActorSnapshot Simulation::PhysicsWorld::story_actor_state() const noexcept {
    StoryActorSnapshot out;
    if (!story_actor_) return out;
    const auto actor = story_actor_->readback();
    out.present = actor.present;
    out.entity_id = Simulation::kStoryActorEntityId;
    out.supported = story_actor_supported_;
    out.support_entity_id = story_actor_support_entity_;
    out.episode = story_actor_episode_;
    out.position = to_vector3(actor.position);
    out.velocity = to_vector3(JPH::RVec3(actor.velocity));
    out.drive_force_n = to_vector3(JPH::RVec3(actor.applied_force_n));
    out.heading_radians = story_actor_heading_;
    return out;
}

void Simulation::PhysicsWorld::prepare_story_actor(float dt) noexcept {
    if (!story_actor_) return;
    ++story_actor_tick_;
    const auto actor = story_actor_->readback();
    const auto player = physics_system_.GetBodyInterface().GetPosition(player_id_);
    const auto delta = JPH::Vec3(player-actor.position);
    JPH::Vec3 desired = JPH::Vec3::sZero();
    if (delta.LengthSq() <= 36) {
        // Presence changes intent, never pose or player input. Facing uses
        // Godot's conventional -Z forward axis; the capsule has no yaw motor.
        if (delta.GetX()*delta.GetX()+delta.GetZ()*delta.GetZ() > .001F)
            story_actor_heading_ = std::atan2(-delta.GetX(), -delta.GetZ());
    } else {
        const double target = story_actor_leg_ > 0 ? -141.2 : -143.2;
        if (std::abs(actor.position.GetZ()-target) < .12) story_actor_leg_ = -story_actor_leg_;
        desired = JPH::Vec3(float(std::clamp(-26.75-actor.position.GetX(), -.2, .2)),
            0, .45F*story_actor_leg_);
        const auto motion = actor.velocity.GetX()*actor.velocity.GetX()+
            actor.velocity.GetZ()*actor.velocity.GetZ() > .0016F ? actor.velocity : desired;
        story_actor_heading_ = std::atan2(-motion.GetX(), -motion.GetZ());
    }
    story_actor_->pre_step(story_actor_tick_, dt, desired);
    contact_listener_.set_story_actor(story_actor_->body_id(), story_actor_tick_);
}

void Simulation::PhysicsWorld::finish_story_actor(bool solved) noexcept {
    if (!story_actor_) return;
    story_actor_supported_ = false;
    story_actor_support_entity_ = 0;
    PhysicalStoryActor::Contact contact;
    if (!solved || !contact_listener_.story_actor_contact(contact)) {
        story_actor_->clear_contact();
        return;
    }
    // The listener retained only the final collision substep. These bodies
    // and this material point participated in the Update that just succeeded.
    story_actor_->submit_solved_contact(contact);
    const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), contact.support);
    if (lock.Succeeded()) {
        story_actor_supported_ = true;
        story_actor_support_entity_ = lock.GetBody().GetUserData();
    }
}
} // namespace scraperx::sim
