#pragma once

#include "sim/simulation.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace scraperx::bridge {

class ScraperXSimulation final : public godot::RefCounted {
    GDCLASS(ScraperXSimulation, godot::RefCounted)

public:
    [[nodiscard]] bool set_move_input(double world_x, double world_z);
    [[nodiscard]] std::int64_t advance_frame(double frame_delta_seconds);
    [[nodiscard]] std::int64_t get_tick_index() const;
    [[nodiscard]] double get_simulation_time_seconds() const;
    [[nodiscard]] double get_fixed_step_seconds() const;
    [[nodiscard]] double get_interpolation_alpha() const;
    [[nodiscard]] godot::Vector3 get_player_position() const;
    [[nodiscard]] godot::Vector3 get_player_linear_velocity() const;
    [[nodiscard]] bool is_player_grounded() const;
    [[nodiscard]] std::int64_t get_support_entity_id() const;

protected:
    static void _bind_methods();

private:
    sim::Simulation simulation_;
};

} // namespace scraperx::bridge
