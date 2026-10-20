#include "bridge/scraperx_simulation.hpp"

namespace scraperx::bridge {
namespace {
godot::Vector3 vector(const sim::Vector3 &value) {
    return {godot::real_t(value.x), godot::real_t(value.y), godot::real_t(value.z)};
}
}
godot::Dictionary ScraperXSimulation::get_story_actor_state() const {
    const auto actor = simulation_->story_actor_state();
    godot::Dictionary result;
    result["present"] = actor.present;
    result["supported"] = actor.supported;
    result["entity_id"] = std::int64_t(actor.entity_id);
    result["support_entity_id"] = std::int64_t(actor.support_entity_id);
    result["episode"] = std::int64_t(actor.episode);
    result["position"] = vector(actor.position);
    result["velocity"] = vector(actor.velocity);
    result["drive_force_n"] = vector(actor.drive_force_n);
    result["heading_radians"] = actor.heading_radians;
    return result;
}
bool ScraperXSimulation::is_story_actor_visible(godot::Vector3 eye) const {
    return simulation_->story_actor_line_of_sight({eye.x, eye.y, eye.z});
}
} // namespace scraperx::bridge
