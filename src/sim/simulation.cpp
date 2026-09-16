#include "sim/simulation.hpp"

#include <cmath>
#include <limits>

namespace scraperx::sim {

AdvanceResult Simulation::advance_frame(const double frame_delta_seconds) noexcept {
    if (!std::isfinite(frame_delta_seconds) || frame_delta_seconds < 0.0 ||
        frame_delta_seconds > kMaximumAcceptedFrameDeltaSeconds) {
        return {};
    }

    const double accumulated = remainder_seconds_ + frame_delta_seconds;
    const double step_epsilon = kFixedStepSeconds * 1.0e-9;
    const double due_as_double = std::floor((accumulated + step_epsilon) / kFixedStepSeconds);

    if (due_as_double < 0.0 ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint64_t>::max() - tick_index_)) {
        return {};
    }

    const auto due = static_cast<std::uint32_t>(due_as_double);
    tick_index_ += due;
    remainder_seconds_ = accumulated - static_cast<double>(due) * kFixedStepSeconds;

    if (remainder_seconds_ < 0.0 && remainder_seconds_ > -step_epsilon) {
        remainder_seconds_ = 0.0;
    }
    if (remainder_seconds_ >= kFixedStepSeconds &&
        remainder_seconds_ - kFixedStepSeconds < step_epsilon) {
        remainder_seconds_ = 0.0;
        ++tick_index_;
        return {true, static_cast<std::uint32_t>(due + 1U)};
    }

    return {true, due};
}

Snapshot Simulation::snapshot() const noexcept {
    return Snapshot{
        tick_index_,
        static_cast<double>(tick_index_) * kFixedStepSeconds,
        kFixedStepSeconds,
        remainder_seconds_ / kFixedStepSeconds,
    };
}

} // namespace scraperx::sim

