#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::PhysicsWorld::nearby_grapple_hook(const GrapplingGun::Readback &state) {
    for (const auto &hook:state.hooks) if (hook.present &&
        (hook.position+hook.rotation*GrapplingGun::hook_eye_local()-state.player_endpoint).LengthSq()<=2.25)
        return true;
    return false;
}

[[nodiscard]] GrappleGunSnapshot Simulation::PhysicsWorld::grapple_gun_state() const noexcept {
    GrappleGunSnapshot out;
    if (!grappling_gun_) return out;
    out.available = true;
    out.acquired = grapple_acquired_;
    out.equipped = grapple_equipped_;
    const auto player = physics_system_.GetBodyInterface().GetPosition(player_id_);
    const auto platform=kit_->body_for_entity(kGrapplePracticeDepartureEntity);
    const JPH::RVec3 rack=platform.valid() ? kit_->body_position(platform)+
        kit_->body_rotation(platform)*JPH::Vec3(-.7F,.9F,0) : JPH::RVec3::sZero();
    out.rack_position = to_vector3(rack);
    out.station_available = platform.valid() && grounded_ && traversal_state_ == TraversalState::None &&
        carried_entity_ == 0 && (player - rack).LengthSq() <= 2.56;
    const auto state = grappling_gun_->readback();
    out.state = unsigned(state.state);
    out.failure = unsigned(state.failure);
    out.cartridges = state.cartridges;
    out.available_hooks = state.available_hooks;
    out.shot_profile = unsigned(state.shot_profile); out.shot_energy_j = state.shot_energy_j;
    out.line_attached = grappling_gun_->line_active();
    // A locked distant line is a travel tool, not a remote pickup command.
    // Retrieval still validates the actual grasp/clearance in its owner.
    out.action_recovers_hook = grappling_gun_->recovery_in_reach();
    out.recovery_nearby = nearby_grapple_hook(state);
    out.active_hook = state.hook.GetIndexAndSequenceNumber();
    out.player_endpoint = to_vector3(state.player_endpoint);
    out.hook_endpoint = to_vector3(state.hook_endpoint);
    out.line_length_m = state.line_length_m;
    out.tension_n = state.tension_n;
    out.extension_m = state.extension_m;
    out.launch_work_j = state.launch_work_j;
    out.last_launch_work_j = state.last_launch_work_j;
    out.reel_work_j = state.reel_work_j;
    out.auto_reel = state.auto_reel;
    out.stored_elastic_energy_j = state.stored_elastic_energy_j;
    out.spool_absorbed_energy_j = state.spool_absorbed_energy_j;
    out.release_discarded_elastic_energy_j = state.release_discarded_elastic_energy_j;
    for (unsigned i = 0; i < out.hooks.size(); ++i) {
        const auto &hook = state.hooks[i];
        out.hooks[i] = {hook.present, hook.body.GetIndexAndSequenceNumber(),
            to_vector3(hook.position), {hook.rotation.GetX(), hook.rotation.GetY(),
            hook.rotation.GetZ(), hook.rotation.GetW()}};
    }
    return out;
}

void Simulation::PhysicsWorld::apply_grapple_commands(const StepCommands &commands) {
    if (!grappling_gun_) return;
    if (commands.grapple_equip_requested) {
        if (!commands.grapple_equipped) {
            grappling_gun_->release();
            grapple_equipped_ = false;
        } else if (traversal_state_ == TraversalState::None && carried_entity_ == 0 &&
                   !(slingshot_ && slingshot_->controls_player()) &&
                   (grapple_acquired_ || grapple_gun_state().station_available)) {
            grapple_acquired_ = grapple_equipped_ = true;
        }
    }
    if (commands.grapple_shot_profile_requested && grapple_equipped_)
        (void)grappling_gun_->set_shot_profile(GrapplingGun::ShotProfile(commands.grapple_shot_profile));
    if (commands.grapple_auto_reel_requested && grapple_equipped_)
        (void)grappling_gun_->set_auto_reel(commands.grapple_auto_reel);
    if (commands.grapple_release) grappling_gun_->release();
    grappling_gun_->set_reel_input(grapple_equipped_ ? float(commands.grapple_reel) : 0.0F);
    if (!commands.grapple_action || !grapple_equipped_ ||
        traversal_state_ != TraversalState::None || carried_entity_ != 0) return;
    const auto before = grappling_gun_->readback();
    if (grappling_gun_->recovery_in_reach()) {
        // A reachable hook is recovered before spool mode can consume Action
        // as Lock Line. The owner still validates actual hand/steel clearance.
        (void)grappling_gun_->retrieve();
    } else if (before.state == GrapplingGun::LineState::PayingOut) {
        (void)grappling_gun_->lock_line();
    } else if (before.state == GrapplingGun::LineState::Locked) {
        (void)grappling_gun_->retrieve();
    } else {
        // Physical retrieval is tried only at actual reach. A distant spent
        // hook remains a body; firing consumes another finite cartridge.
        if (nearby_grapple_hook(before)) {
            (void)grappling_gun_->retrieve();
            // A failed recovery retains its real cause and inventory.
            // Action must not silently fall through into another shot.
            return;
        }
        (void)grappling_gun_->fire(JPH::Vec3(float(commands.grapple_aim.x),
            float(commands.grapple_aim.y), float(commands.grapple_aim.z)));
    }
}

} // namespace scraperx::sim
