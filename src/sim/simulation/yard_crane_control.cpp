#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::set_yard_crane_input(double slew, double trolley, double hoist) noexcept {
    const std::array<double, 3> values{slew, trolley, hoist};
    for (const double value : values) if (!std::isfinite(value) || std::abs(value) > 1) {
        yard_crane_input_ = {};
        return false;
    }
    yard_crane_input_ = values;
    return true;
}
bool Simulation::request_yard_crane_attach() noexcept {
    const auto state = yard_crane_state();
    if (!state.rigging_reachable || state.rigging_leg < 0 ||
        (state.attached_mask & (1U << unsigned(state.rigging_leg)))) return false;
    yard_crane_attach_requested_ = true;
    return true;
}
bool Simulation::request_yard_crane_detach() noexcept {
    const auto state = yard_crane_state();
    if (!state.rigging_reachable || state.rigging_leg < 0 ||
        !(state.attached_mask & (1U << unsigned(state.rigging_leg)))) return false;
    yard_crane_detach_requested_ = true;
    return true;
}
YardCraneSnapshot Simulation::yard_crane_state() const noexcept {
    return physics_world_->yard_crane_state();
}
YardCraneSnapshot Simulation::PhysicsWorld::yard_crane_state() const noexcept {
    return yard_crane_snapshot_;
}

bool Simulation::PhysicsWorld::yard_crane_controls_reachable(
        const JPH::BodyInterface &bodies) const noexcept {
    if (!yard_crane_ || !grounded_ || crouched_ || traversal_state_ != TraversalState::None ||
        physical_hands_->active() || carried_entity_ || !foot_contact_.valid ||
        foot_contact_.solved_tick != contact_solve_serial_ ||
        foot_contact_.entity_id != 1450 || support_entity_id_ != 1450 ||
        foot_contact_.body != kit_->body_id(yard_crane_->geometry().foundation) ||
        !footing_is_firm(bodies)) return false;
    const auto floor = from_support_local(bodies, foot_contact_.body, foot_contact_.local_point);
    // Actual raised booth floor, not the whole foundation or nearby grade.
    if (floor.GetX() < -67.4 || floor.GetX() > -65.6 ||
        floor.GetZ() < -151.2 || floor.GetZ() > -148.8 ||
        std::abs(floor.GetY() - .18) > .04) return false;
    const auto station = yard_crane_->geometry().ground_station;
    return (bodies.GetPosition(player_id_) - station).LengthSq() <= 1.2 * 1.2;
}

int Simulation::PhysicsWorld::nearby_yard_crane_leg(const JPH::BodyInterface &bodies) const noexcept {
    if (!yard_crane_ || !grounded_ || crouched_ || traversal_state_ != TraversalState::None ||
        physical_hands_->active() || carried_entity_ || !foot_contact_.valid ||
        foot_contact_.solved_tick != contact_solve_serial_ || !footing_is_firm(bodies)) return -1;
    const auto chassis = kit_->body_id(forklift_yard_.truck.chassis);
    const auto shoulder = bodies.GetPosition(player_id_) + JPH::Vec3(0, .30F, 0);
    const auto eyes = WarehouseForkliftYard::lifting_points();
    int nearest = -1;
    double distance = kTraversalReach * kTraversalReach;
    for (unsigned i = 0; i < eyes.size(); ++i) {
        const auto point = bodies.GetWorldTransform(chassis) * eyes[i];
        const auto square = (point - shoulder).LengthSq();
        if (square > distance) continue;
        JPH::IgnoreSingleBodyFilter ignored(player_id_);
        JPH::RayCastResult hit;
        const bool blocked = physics_system_.GetNarrowPhaseQuery().CastRay(
            JPH::RRayCast(shoulder, JPH::Vec3(point - shoulder)), hit, {}, {}, ignored);
        // The target is inside the actual eye's top bar. Accept that exact
        // box as the first hit, rather than a fraction window that mistakes
        // its60mm half-width for an obstruction on short hand approaches.
        // Any other chassis steel or another body still blocks the hand.
        if (blocked) {
            if (hit.mBodyID != chassis) continue;
            const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), chassis);
            LeafBox bar;
            if (!lock.Succeeded() || !leaf_box(lock.GetBody(), hit.mSubShapeID2, bar)) continue;
            const float roundoff = 8 * std::numeric_limits<float>::epsilon() *
                std::max(1.0F, JPH::Vec3(point).Length());
            if ((bar.centre - point).LengthSq() > roundoff * roundoff) continue;
        }
        nearest = int(i); distance = square;
    }
    return nearest;
}

void Simulation::PhysicsWorld::prepare_yard_crane(
        const StepCommands &commands, const JPH::BodyInterface &bodies) {
    if (!yard_crane_) return;
    const int leg = nearby_yard_crane_leg(bodies);
    // Walking to a real eye is required; cab controls cannot remotely rig.
    if (leg >= 0 && warehouse_forklift_) {
        if (commands.yard_crane_detach)
            (void)yard_crane_->try_detach_leg(unsigned(leg), *warehouse_forklift_);
        else if (commands.yard_crane_attach)
            (void)yard_crane_->try_attach_leg(unsigned(leg), *warehouse_forklift_);
    }
    YardCrane::Command demand;
    if (!commands.jump_requested && !commands.release_requested &&
        yard_crane_controls_reachable(bodies)) {
        demand.slew = float(commands.yard_crane_input[0]);
        demand.trolley = float(commands.yard_crane_input[1]);
        demand.hoist = float(commands.yard_crane_input[2]);
        // Physical control interlock: an incomplete four-leg lift cannot
        // be powered upward. Empty rig alignment and lowering stay usable.
        const auto mask = yard_crane_->state().attached_mask;
        if (mask != 0 && mask != 15 && demand.hoist > 0) demand.hoist = 0;
    }
    yard_crane_->pre_step(demand);
}

void Simulation::PhysicsWorld::refresh_yard_crane_snapshot(const JPH::BodyInterface &bodies) {
    yard_crane_snapshot_ = {};
    if (!yard_crane_) return;
    auto &out = yard_crane_snapshot_;
    const auto &geometry = yard_crane_->geometry();
    const auto s = yard_crane_->state();
    out.available = true;
    out.controls_reachable = yard_crane_controls_reachable(bodies);
    out.rigging_leg = nearby_yard_crane_leg(bodies);
    out.rigging_reachable = out.rigging_leg >= 0;
    out.station_position = to_vector3(geometry.ground_station);
    out.operator_position = {-66.5, 0, -150};
    out.operator_left_hand = {-65.95, 1.02, -150.30};
    out.operator_right_hand = {-65.95, 1.02, -149.70};
    out.attached_mask = s.attached_mask;
    out.failure = std::uint8_t(s.failure);
    out.hoist_connected = s.hoist_connected;
    out.hoist_length_m = s.hoist.rest_length_m;
    out.hoist_actual_length_m = s.hoist.actual_length_m;
    out.hoist_force_n = s.hoist.tensile_force_n;
    out.hoist_target_mps = s.hoist.target_speed_mps;
    out.hoist_work_j = s.hoist.positive_motor_work_j;
    out.hoist_source_used_j = s.hoist.source_energy_used_j;
    out.hoist_source_capacity_j = s.hoist.energy_j + s.hoist.source_energy_used_j;
    out.slew_angle_rad = s.slew_angle_rad;
    out.slew_target_rad_s = s.slew_target_rad_s;
    out.slew_torque_nm = s.slew_torque_nm;
    out.trolley_position_m = s.trolley_position_m;
    out.trolley_target_mps = s.trolley_target_mps;
    out.trolley_force_n = s.trolley_force_n;
    out.transport_energy_j = s.transport_energy_j;
    out.transport_work_j = s.transport_positive_work_j;
    out.lines[0] = {to_vector3(s.hoist.support_endpoint), to_vector3(s.hoist.load_endpoint),
        s.hoist.tensile_force_n, s.hoist.rest_length_m, s.hoist_connected};
    const auto spreader_frame = bodies.GetWorldTransform(kit_->body_id(geometry.spreader));
    for (unsigned i = 0; i < 4; ++i) {
        const bool attached = bool(s.attached_mask & (1U << i));
        out.lines[i + 1] = {to_vector3(spreader_frame * geometry.spreader_eyes[i]),
            to_vector3(attached ? s.slings[i].load_endpoint : s.loose_ends[i]),
            s.slings[i].tensile_force_n, geometry.sling_lengths[i], attached};
    }
}
} // namespace scraperx::sim
