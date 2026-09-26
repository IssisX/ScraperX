#pragma once

#include "sim/ground_stage.hpp"

#include <godot_cpp/classes/ref_counted.hpp>

namespace scraperx::bridge {

class ScraperXScrew final : public godot::RefCounted {
    GDCLASS(ScraperXScrew, godot::RefCounted)

public:
    static void _bind_methods();

    [[nodiscard]] bool advance_frame(double seconds) noexcept;
    [[nodiscard]] bool request_motor_toggle(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_reverse(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_valve_toggle(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_release(double x, double y, double z) noexcept;
    [[nodiscard]] bool request_reset(double x, double y, double z) noexcept;
    void set_rider_on_cage(bool aboard) noexcept;
    [[nodiscard]] bool at_pump_station(double x, double y, double z) const noexcept;
    [[nodiscard]] bool at_cage_control(double x, double y, double z) const noexcept;
    [[nodiscard]] bool at_upper_control(double x, double y, double z) const noexcept;

    [[nodiscard]] bool is_motor_enabled() const noexcept;
    [[nodiscard]] int get_drive_direction() const noexcept;
    [[nodiscard]] double get_shaft_angle() const noexcept;
    [[nodiscard]] double get_shaft_rpm() const noexcept;
    [[nodiscard]] double get_motor_torque() const noexcept;
    [[nodiscard]] double get_screw_flow() const noexcept;
    [[nodiscard]] double get_basin_water() const noexcept;
    [[nodiscard]] double get_tank_water() const noexcept;
    [[nodiscard]] double get_bucket_water() const noexcept;
    [[nodiscard]] double get_total_water() const noexcept;
    [[nodiscard]] bool is_valve_open() const noexcept;
    [[nodiscard]] double get_valve_flow() const noexcept;
    [[nodiscard]] double get_cage_travel() const noexcept;
    [[nodiscard]] double get_cage_speed() const noexcept;
    [[nodiscard]] double get_rope_tension() const noexcept;
    [[nodiscard]] bool is_upper_caught() const noexcept;
    [[nodiscard]] bool is_bucket_caught() const noexcept;

private:
    sim::GroundStage stage_{};
};

} // namespace scraperx::bridge
