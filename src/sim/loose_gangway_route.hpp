#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kLooseGangwayDockEntity = 1490;
inline constexpr std::uint64_t kLooseGangwayEntity = 2490;
inline constexpr std::uint64_t kLooseGangwayCribEntity = 2491;
inline constexpr std::uint64_t kLooseGangwaySpareCribEntity = 2492;
inline const JPH::RVec3 kLooseGangwayBay{-65.0, 0.0, -28.0};
inline constexpr float kLooseGangwayDockTop = .36F;
inline constexpr float kLooseGangwayStorageX = -6.0F;
inline constexpr float kLooseGangwayHalfLength = 2.4F;

// A staging board starts balanced on loose dunnage. All three movable
// objects use ordinary contact and the existing pickup/drop owner. Neither
// placement nor support is inferred from a target region or puzzle flag.
void build_loose_gangway_route(kit::Kit &kit);

} // namespace scraperx::sim
