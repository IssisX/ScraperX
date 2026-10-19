#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kBeamRampLandingEntity = 1491;
inline const JPH::RVec3 kBeamRampLandingOrigin{-89.5, 0.0, -44.0};
inline constexpr float kBeamRampLandingTop = 2.4F;

// Receiving trestle for the existing independent 600kg yard beam (2446).
// Placement uses ordinary fork contact; the trestle creates no attachment.
void build_beam_ramp_route(kit::Kit &kit);

} // namespace scraperx::sim
