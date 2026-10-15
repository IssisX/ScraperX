#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::set_forklift_input(double drive, double steer, double lift,
                                   double tilt, double brake) noexcept {
    const std::array<double, 5> requested{drive, steer, lift, tilt, brake};
    for (unsigned i = 0; i < requested.size(); ++i) {
        if (!std::isfinite(requested[i]) || requested[i] > 1 ||
            requested[i] < (i == 4 ? 0 : -1)) {
            forklift_input_ = {0, 0, 0, 0, 1};
            return false;
        }
    }
    forklift_input_ = requested;
    return true;
}

ForkliftSnapshot Simulation::forklift_state() const noexcept {
    return physics_world_->forklift_state();
}

ForkliftSnapshot Simulation::render_forklift_state() const noexcept {
    auto result = forklift_state();
    if (!result.available || !previous_forklift_state_.available ||
        result.chassis_entity != previous_forklift_state_.chassis_entity) return result;
    const double alpha = std::clamp(remainder_seconds_ / kFixedStepSeconds, 0.0, 1.0);
    for (unsigned i = 0; i < result.wheels.size(); ++i) {
        auto &wheel = result.wheels[i];
        const auto &before = previous_forklift_state_.wheels[i];
        wheel.position = {
            before.position.x + (wheel.position.x - before.position.x) * alpha,
            before.position.y + (wheel.position.y - before.position.y) * alpha,
            before.position.z + (wheel.position.z - before.position.z) * alpha};
        const JPH::Quat a(float(before.rotation.x), float(before.rotation.y),
                          float(before.rotation.z), float(before.rotation.w));
        const JPH::Quat b(float(wheel.rotation.x), float(wheel.rotation.y),
                          float(wheel.rotation.z), float(wheel.rotation.w));
        wheel.rotation = to_quaternion(a.SLERP(b, float(alpha)));
    }
    return result;
}

ForkliftSnapshot Simulation::PhysicsWorld::forklift_state() const noexcept {
    return forklift_snapshot_;
}

bool Simulation::PhysicsWorld::forklift_controls_reachable(
        const JPH::BodyInterface &bodies) const noexcept {
    if (!warehouse_forklift_ || !grounded_ || crouched_ ||
        traversal_state_ != TraversalState::None || physical_hands_->active() ||
        carried_entity_ != 0 || !foot_contact_.valid ||
        foot_contact_.solved_tick != contact_solve_serial_ ||
        foot_contact_.entity_id != WarehouseForkliftYard::kChassisEntity ||
        support_entity_id_ != WarehouseForkliftYard::kChassisEntity ||
        foot_contact_.body != kit_->body_id(forklift_yard_.truck.chassis) ||
        !footing_is_firm(bodies)) return false;
    const auto chassis = foot_contact_.body;
    const auto contact = from_support_local(bodies, chassis, foot_contact_.local_point);
    const auto local = bodies.GetRotation(chassis).Conjugated() *
        JPH::Vec3(contact - bodies.GetPosition(chassis));
    // The chassis also contains the battery hood. Only actual contact on
    // the rear operator floor authorizes controls, not standing on the hood.
    if (std::abs(local.GetX()) > .64F || std::abs(local.GetY() + .24F) > .08F ||
        local.GetZ() < -2.03F || local.GetZ() > -1.20F) return false;
    const auto station = bodies.GetWorldTransform(chassis) * forklift_yard_.control_station;
    if ((bodies.GetPosition(player_id_) - station).LengthSq() > 1.4 * 1.4) return false;
    const auto eye = bodies.GetPosition(player_id_) + JPH::Vec3(0, .6F, 0);
    JPH::IgnoreMultipleBodiesFilter ignored;
    ignored.IgnoreBody(player_id_);
    ignored.IgnoreBody(chassis);
    JPH::RayCastResult hit;
    return !physics_system_.GetNarrowPhaseQuery().CastRay(
        JPH::RRayCast(eye, JPH::Vec3(station - eye)), hit, {}, {}, ignored);
}

void Simulation::PhysicsWorld::prepare_forklift(
        const StepCommands &commands, const JPH::BodyInterface &bodies) {
    if (!warehouse_forklift_) return;
    WarehouseForklift::Command demand;
    if (!commands.jump_requested && !commands.release_requested &&
        forklift_controls_reachable(bodies)) {
        demand.drive = float(commands.forklift_input[0]);
        demand.steer = float(commands.forklift_input[1]);
        demand.lift = float(commands.forklift_input[2]);
        demand.tilt = float(commands.forklift_input[3]);
        demand.brake = float(commands.forklift_input[4]);
    }
    warehouse_forklift_->pre_step(demand);
}

void Simulation::PhysicsWorld::refresh_forklift_snapshot(const JPH::BodyInterface &bodies) {
    forklift_snapshot_ = {};
    if (!warehouse_forklift_) return;
    auto &result = forklift_snapshot_;
    result.available = true;
    result.controls_reachable = forklift_controls_reachable(bodies);
    result.chassis_entity = WarehouseForkliftYard::kChassisEntity;
    result.station_position = to_vector3(bodies.GetWorldTransform(
        kit_->body_id(forklift_yard_.truck.chassis)) * forklift_yard_.control_station);
    const auto state = warehouse_forklift_->state();
    result.battery_j = state.battery_j;
    result.capacity_j = warehouse_forklift_->config().battery_capacity_j;
    result.coupled = state.coupled;
    result.energy_cutoff = state.energy_cutoff;
    result.lift_position_m = state.lift_position_m;
    result.tilt_angle_rad = state.tilt_angle_rad;
    result.lift_force_n = state.lift_force_n;
    result.tilt_torque_nm = state.tilt_torque_nm;
    result.applied_drive = state.applied_drive;
    result.applied_brake = state.applied_brake;
    result.positive_work_j = state.hydraulic_positive_work_j;
    result.reservation_excess_j = state.reservation_excess_j;
    const auto wheels = warehouse_forklift_->wheels();
    for (unsigned i = 0; i < wheels.size(); ++i) {
        auto &wheel = result.wheels[i];
        const auto &source = wheels[i];
        wheel.position = to_vector3(source.position);
        wheel.rotation = to_quaternion(source.rotation);
        wheel.contact_position = to_vector3(source.contact_position);
        wheel.contact_normal = {source.contact_normal.GetX(), source.contact_normal.GetY(), source.contact_normal.GetZ()};
        wheel.contact_body = source.contact_body.GetIndexAndSequenceNumber();
        wheel.radius_m = source.radius_m;
        wheel.width_m = source.width_m;
        wheel.normal_force_n = source.normal_force_n;
        wheel.angular_velocity_rad_s = source.angular_velocity_rad_s;
        wheel.steer_rad = source.steer_rad;
        wheel.contact = source.contact;
    }
}
} // namespace scraperx::sim
