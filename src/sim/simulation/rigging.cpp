#include "sim/simulation/world.hpp"

namespace scraperx::sim {
namespace {
constexpr float kCutHoldSeconds = .75F; // Deliberate native hold, not a UI timer.
}

bool Simulation::set_rigging_aim(double x, double y, double z) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
    const double length = std::hypot(x, y, z);
    if (length < 1.e-6) return false;
    rigging_aim_ = {x / length, y / length, z / length};
    return true;
}
bool Simulation::request_rigging(RiggingCommandKind kind, std::uint64_t target) noexcept {
    if (kind == RiggingCommandKind::EndCut) {
        rigging_cut_held_ = false; // Cancellation cannot be lost to a full queue.
        return true;
    }
    if (unsigned(kind) > unsigned(RiggingCommandKind::EndCut) ||
        rigging_command_count_ == rigging_commands_.size()) return false;
    rigging_commands_[rigging_command_count_++] = {kind, target};
    if (kind == RiggingCommandKind::BeginCut) rigging_cut_held_ = true;
    if (kind == RiggingCommandKind::Unequip) {
        rigging_cut_held_ = false;
        rigging_power_input_ = 0;
    }
    return true;
}
bool Simulation::set_rigging_power_input(double effort) noexcept {
    if (!std::isfinite(effort)) return false;
    rigging_power_input_ = std::clamp(effort, -1.0, 1.0);
    return true;
}
RiggingSnapshot Simulation::rigging_state() const noexcept { return physics_world_->rigging_state(); }

void Simulation::PhysicsWorld::clear_rigging_input() noexcept {
    rigging_cut_target_ = 0;
    rigging_cut_seconds_ = 0;
    if (rigging_) rigging_->equip(false);
}
void Simulation::PhysicsWorld::apply_rigging_commands(const StepCommands &commands, float dt) {
    if (!rigging_) return;
    bool machine_command = commands.yard_crane_attach || commands.yard_crane_detach ||
        commands.forklift_tool_request != ForkliftToolRequest::None;
    for (unsigned i = 0; i < 4; ++i) machine_command |= commands.forklift_input[i] != 0;
    for (double value : commands.yard_crane_input) machine_command |= value != 0;
    const auto allowed = [&] {
        return traversal_state_ == TraversalState::None && !physical_hands_->active() &&
            carry_constraint_ == nullptr && !grapple_equipped_ &&
            !(slingshot_ && slingshot_->controls_player()) &&
            !machine_command && !commands.pick_up_requested && !commands.traversal_requested;
    };
    const auto refresh = [&] {
        const auto origin = physics_system_.GetBodyInterface().GetPosition(player_id_) +
            JPH::Vec3(0, crouched_ ? .35F : .62F, 0);
        rigging_->aim(origin, JPH::Vec3(float(commands.rigging_aim.x),
            float(commands.rigging_aim.y), float(commands.rigging_aim.z)), player_id_, allowed());
    };
    refresh();
    for (std::size_t i = 0; i < commands.rigging_command_count; ++i) {
        const auto command = commands.rigging_commands[i];
        if (command.kind != RiggingCommandKind::BeginCut && command.kind != RiggingCommandKind::EndCut) {
            rigging_cut_target_ = 0;
            rigging_cut_seconds_ = 0;
        }
        switch (command.kind) {
        case RiggingCommandKind::Equip:
            if (traversal_line_) {
                traversal_line_->equip(false);
                if (traversal_state_ == TraversalState::LineSwing) traversal_state_ = TraversalState::None;
            }
            if (allowed()) rigging_->equip(true);
            break;
        case RiggingCommandKind::Unequip: clear_rigging_input(); break;
        case RiggingCommandKind::Action: (void)rigging_->action(command.target); break;
        case RiggingCommandKind::Undo: rigging_->undo(); break;
        case RiggingCommandKind::Select: (void)rigging_->select(command.target); break;
        case RiggingCommandKind::Unclip: (void)rigging_->unclip(command.target); break;
        case RiggingCommandKind::BeginCut:
            if (allowed() && rigging_->readback().equipped && rigging_->select(command.target)) {
                rigging_cut_target_ = command.target;
                rigging_cut_seconds_ = 0;
            }
            break;
        case RiggingCommandKind::EndCut:
            rigging_cut_target_ = 0;
            rigging_cut_seconds_ = 0;
            break;
        }
        refresh();
    }
    if (!commands.rigging_cut_held || !allowed() || !rigging_->readback().equipped) {
        rigging_cut_target_ = 0;
        rigging_cut_seconds_ = 0;
    } else if (rigging_cut_target_) {
        rigging_cut_seconds_ += dt;
        if (rigging_cut_seconds_ >= kCutHoldSeconds) {
            (void)rigging_->cut(rigging_cut_target_);
            rigging_cut_target_ = 0;
            rigging_cut_seconds_ = 0;
        }
    }
    rigging_->set_power_input(static_cast<float>(commands.rigging_power));
}
RiggingSnapshot Simulation::PhysicsWorld::rigging_state() const noexcept {
    RiggingSnapshot out;
    if (!rigging_) return out;
    const auto state = rigging_->readback();
    out.available = true;
    out.equipped = state.equipped; out.suspended = state.suspended;
    out.draft = state.draft; out.guide_threaded = state.guide_threaded;
    out.target = state.target; out.selected = state.selected; out.event_serial = state.event_serial;
    out.target_line = state.target_line;
    out.target_kind = unsigned(state.target_kind); out.failure = unsigned(state.failure);
    out.installed_count = state.installed_count;
    out.target_point = detail::to_vector3(state.target_point);
    out.tension_n = state.tension_n; out.slack_m = state.slack_m;
    out.power_selected = state.power_selected; out.power_operable = state.power_operable;
    out.braking = state.braking; out.energy_cutoff = state.energy_cutoff;
    out.rest_length_m = state.rest_length_m; out.actual_length_m = state.actual_length_m;
    out.target_speed_mps = state.target_speed_mps;
    out.energy_j = state.energy_j; out.capacity_j = state.capacity_j;
    out.source_energy_used_j = state.source_energy_used_j;
    out.motor_work_j = state.motor_work_j; out.heat_j = state.heat_j;
    std::array<JPH::RVec3, 4> preview;
    out.preview_count = rigging_->preview(preview);
    for (std::size_t i = 0; i < out.preview_count; ++i)
        out.preview[i] = detail::to_vector3(preview[i]);
    out.cut_progress = std::clamp(double(rigging_cut_seconds_ / kCutHoldSeconds), 0.0, 1.0);
    for (std::size_t i = 0; i < out.lines.size(); ++i) out.lines[i] = rigging_->line_handle(i);
    return out;
}
} // namespace scraperx::sim
