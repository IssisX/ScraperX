#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kWorkshopRoofAccessEntity = 1460;
inline constexpr float kWorkshopLaunchTopM = 1.8F;
inline constexpr float kWorkshopRoofTopM = 5.14F;

// Fixed ground-bearing service plinth and access treads. Kit owns their
// identical collision/render parts; ordinary feet and hands own all movement.
void build_workshop_roof_route(kit::Kit &kit);

} // namespace scraperx::sim
