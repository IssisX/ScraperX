#include "bridge/scraperx_simulation.hpp"
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace scraperx::bridge {
namespace {
godot::Vector3 vector(const sim::Vector3 &v) {
    return {godot::real_t(v.x), godot::real_t(v.y), godot::real_t(v.z)};
}
}
bool ScraperXSimulation::set_yard_crane_input(double slew, double trolley, double hoist) {
    return simulation_->set_yard_crane_input(slew, trolley, hoist);
}
bool ScraperXSimulation::request_yard_crane_attach() {
    return simulation_->request_yard_crane_attach();
}
bool ScraperXSimulation::request_yard_crane_detach() {
    return simulation_->request_yard_crane_detach();
}
godot::Dictionary ScraperXSimulation::get_yard_crane_state() const {
    const auto s = simulation_->yard_crane_state();
    godot::Dictionary out;
#define RECEIPT(name) out[#name] = s.name
    RECEIPT(available); RECEIPT(controls_reachable); RECEIPT(rigging_reachable);
    RECEIPT(rigging_leg); RECEIPT(hoist_connected);
    RECEIPT(hoist_length_m); RECEIPT(hoist_actual_length_m); RECEIPT(hoist_force_n);
    RECEIPT(hoist_target_mps); RECEIPT(hoist_work_j); RECEIPT(hoist_source_used_j);
    RECEIPT(hoist_source_capacity_j); RECEIPT(slew_angle_rad); RECEIPT(slew_target_rad_s);
    RECEIPT(slew_torque_nm); RECEIPT(trolley_position_m); RECEIPT(trolley_target_mps);
    RECEIPT(trolley_force_n); RECEIPT(transport_energy_j); RECEIPT(transport_work_j);
#undef RECEIPT
    out["foundation_entity"] = std::int64_t(s.foundation_entity);
    out["boom_entity"] = std::int64_t(s.boom_entity);
    out["trolley_entity"] = std::int64_t(s.trolley_entity);
    out["spreader_entity"] = std::int64_t(s.spreader_entity);
    out["attached_mask"] = int(s.attached_mask);
    out["failure"] = int(s.failure);
    out["station_position"] = vector(s.station_position);
    out["operator_forward"] = vector(s.operator_forward);
    out["operator_left_hand"] = vector(s.operator_left_hand);
    out["operator_right_hand"] = vector(s.operator_right_hand);
    const auto &q = s.operator_rotation;
    out["operator_frame"] = godot::Transform3D(godot::Basis(godot::Quaternion(
        godot::real_t(q.x), godot::real_t(q.y), godot::real_t(q.z), godot::real_t(q.w))),
        vector(s.operator_position));
    godot::Array lines;
    for (const auto &line : s.lines) {
        godot::Dictionary value;
        value["from"] = vector(line.from); value["to"] = vector(line.to);
        value["attached"] = line.attached;
        value["force_n"] = line.force_n; value["rest_length_m"] = line.rest_length_m;
        lines.push_back(value);
    }
    out["lines"] = lines;
    return out;
}
} // namespace scraperx::bridge
