#include "bridge/scraperx_screw.hpp"

#include <godot_cpp/core/class_db.hpp>

namespace scraperx::bridge {

void ScraperXScrew::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("advance_frame", "seconds"), &ScraperXScrew::advance_frame);
    ClassDB::bind_method(D_METHOD("request_motor_toggle", "x", "y", "z"), &ScraperXScrew::request_motor_toggle);
    ClassDB::bind_method(D_METHOD("request_reverse", "x", "y", "z"), &ScraperXScrew::request_reverse);
    ClassDB::bind_method(D_METHOD("request_valve_toggle", "x", "y", "z"), &ScraperXScrew::request_valve_toggle);
    ClassDB::bind_method(D_METHOD("request_release", "x", "y", "z"), &ScraperXScrew::request_release);
    ClassDB::bind_method(D_METHOD("request_reset", "x", "y", "z"), &ScraperXScrew::request_reset);
    ClassDB::bind_method(D_METHOD("set_rider_on_cage", "aboard"), &ScraperXScrew::set_rider_on_cage);
    ClassDB::bind_method(D_METHOD("at_pump_station", "x", "y", "z"), &ScraperXScrew::at_pump_station);
    ClassDB::bind_method(D_METHOD("at_cage_control", "x", "y", "z"), &ScraperXScrew::at_cage_control);
    ClassDB::bind_method(D_METHOD("at_upper_control", "x", "y", "z"), &ScraperXScrew::at_upper_control);
    ClassDB::bind_method(D_METHOD("is_motor_enabled"), &ScraperXScrew::is_motor_enabled);
    ClassDB::bind_method(D_METHOD("get_drive_direction"), &ScraperXScrew::get_drive_direction);
    ClassDB::bind_method(D_METHOD("get_shaft_angle"), &ScraperXScrew::get_shaft_angle);
    ClassDB::bind_method(D_METHOD("get_shaft_rpm"), &ScraperXScrew::get_shaft_rpm);
    ClassDB::bind_method(D_METHOD("get_motor_torque"), &ScraperXScrew::get_motor_torque);
    ClassDB::bind_method(D_METHOD("get_screw_flow"), &ScraperXScrew::get_screw_flow);
    ClassDB::bind_method(D_METHOD("get_basin_water"), &ScraperXScrew::get_basin_water);
    ClassDB::bind_method(D_METHOD("get_tank_water"), &ScraperXScrew::get_tank_water);
    ClassDB::bind_method(D_METHOD("get_bucket_water"), &ScraperXScrew::get_bucket_water);
    ClassDB::bind_method(D_METHOD("get_total_water"), &ScraperXScrew::get_total_water);
    ClassDB::bind_method(D_METHOD("is_valve_open"), &ScraperXScrew::is_valve_open);
    ClassDB::bind_method(D_METHOD("get_valve_flow"), &ScraperXScrew::get_valve_flow);
    ClassDB::bind_method(D_METHOD("get_cage_travel"), &ScraperXScrew::get_cage_travel);
    ClassDB::bind_method(D_METHOD("get_cage_speed"), &ScraperXScrew::get_cage_speed);
    ClassDB::bind_method(D_METHOD("get_rope_tension"), &ScraperXScrew::get_rope_tension);
    ClassDB::bind_method(D_METHOD("is_upper_caught"), &ScraperXScrew::is_upper_caught);
    ClassDB::bind_method(D_METHOD("is_bucket_caught"), &ScraperXScrew::is_bucket_caught);
}

bool ScraperXScrew::advance_frame(double seconds) noexcept { return stage_.advance_frame(seconds); }
bool ScraperXScrew::request_motor_toggle(double x, double y, double z) noexcept { return stage_.request_motor_toggle(x, y, z); }
bool ScraperXScrew::request_reverse(double x, double y, double z) noexcept { return stage_.request_reverse(x, y, z); }
bool ScraperXScrew::request_valve_toggle(double x, double y, double z) noexcept { return stage_.request_valve_toggle(x, y, z); }
bool ScraperXScrew::request_release(double x, double y, double z) noexcept { return stage_.request_release(x, y, z); }
bool ScraperXScrew::request_reset(double x, double y, double z) noexcept { return stage_.request_reset(x, y, z); }
void ScraperXScrew::set_rider_on_cage(bool aboard) noexcept { stage_.set_rider_on_cage(aboard); }
bool ScraperXScrew::at_pump_station(double x, double y, double z) const noexcept { return stage_.at_pump_station(x, y, z); }
bool ScraperXScrew::at_cage_control(double x, double y, double z) const noexcept { return stage_.at_cage_control(x, y, z); }
bool ScraperXScrew::at_upper_control(double x, double y, double z) const noexcept { return stage_.at_upper_control(x, y, z); }
bool ScraperXScrew::is_motor_enabled() const noexcept { return stage_.state().motor_enabled; }
int ScraperXScrew::get_drive_direction() const noexcept { return stage_.state().drive_direction; }
double ScraperXScrew::get_shaft_angle() const noexcept { return stage_.state().shaft_angle_radians; }
double ScraperXScrew::get_shaft_rpm() const noexcept { return stage_.state().shaft_rpm; }
double ScraperXScrew::get_motor_torque() const noexcept { return stage_.state().motor_torque_nm; }
double ScraperXScrew::get_screw_flow() const noexcept { return stage_.state().delivered_flow_m3_s; }
double ScraperXScrew::get_basin_water() const noexcept { return stage_.state().basin_volume_m3; }
double ScraperXScrew::get_tank_water() const noexcept { return stage_.state().tank_volume_m3; }
double ScraperXScrew::get_bucket_water() const noexcept { return stage_.lift_state().bucket_water_m3; }
double ScraperXScrew::get_total_water() const noexcept { return stage_.total_water_m3(); }
bool ScraperXScrew::is_valve_open() const noexcept { return stage_.lift_state().valve_open; }
double ScraperXScrew::get_valve_flow() const noexcept { return stage_.lift_state().valve_flow_m3_s; }
double ScraperXScrew::get_cage_travel() const noexcept { return stage_.lift_state().cage_travel_m; }
double ScraperXScrew::get_cage_speed() const noexcept { return stage_.lift_state().cage_speed_m_s; }
double ScraperXScrew::get_rope_tension() const noexcept { return stage_.lift_state().rope_tension_n; }
bool ScraperXScrew::is_upper_caught() const noexcept { return stage_.lift_state().upper_catch_latched; }
bool ScraperXScrew::is_bucket_caught() const noexcept { return stage_.lift_state().bucket_top_catch_latched; }

} // namespace scraperx::bridge
