#pragma once

#include "sim/simulation.hpp"

#include <cmath>
#include <cstdint>

namespace scraperx::test {
// These encounters have exactly one ordinary rigid receiver. Source identity
// alone cannot admit a powered leg: require the passive receipts and an actual
// grounded acquisition, or continuation of that same finite row. The native
// passive plant reserves .3 m for possible later activation but commands none.
// Snapshot does
// not expose purpose/body/subshape; do not generalize this to arbitrary solids.
inline bool passive_receiver_plant(const sim::Snapshot &now, const sim::Snapshot &before,
                                   std::uint64_t receiver) {
    const auto passive = [receiver](const sim::Snapshot &v) {
        return v.foot_push_active && v.foot_push_support_entity_id == receiver &&
            v.foot_push_command_work_bound_j == 0 && v.foot_push_stroke_m == 0 &&
            v.foot_push_stroke_limit_m == static_cast<double>(.3F);
    };
    if (!passive(now)) return false;
    const auto f = now.foot_push_player_force;
    const double force = std::hypot(std::hypot(f.x, f.y), f.z);
    if (!std::isfinite(force) || force > 3500.01 ||
        !std::isfinite(now.foot_push_last_load_n) || now.foot_push_last_load_n < 0 ||
        now.foot_push_last_load_n > 3500.01 ||
        !std::isfinite(now.foot_push_peak_load_n) || now.foot_push_peak_load_n < 0 ||
        now.foot_push_peak_load_n > 3500.01 ||
        !std::isfinite(now.foot_push_elapsed_seconds) || now.foot_push_elapsed_seconds < 0 ||
        now.foot_push_elapsed_seconds > .25 + sim::Simulation::kFixedStepSeconds ||
        !std::isfinite(now.foot_push_last_impulse_ns) || now.foot_push_last_impulse_ns < 0 ||
        now.foot_push_last_impulse_ns > 3500.0 / 360.0 + .0001 ||
        !std::isfinite(now.foot_push_initial_distance_m) || now.foot_push_initial_distance_m <= 0 ||
        !std::isfinite(now.foot_push_actual_distance_m) || now.foot_push_actual_distance_m <= 0)
        return false;
    const bool continued = passive(before) &&
        now.foot_push_start_count == before.foot_push_start_count &&
        now.foot_push_initial_distance_m == before.foot_push_initial_distance_m &&
        now.foot_push_elapsed_seconds >= before.foot_push_elapsed_seconds;
    // Retention may outlive bottom contact; a new/replaced row cannot.
    const bool acquired = now.player_grounded && now.support_entity_id == receiver &&
        now.foot_push_start_count == before.foot_push_start_count;
    return continued || acquired;
}
} // namespace scraperx::test
