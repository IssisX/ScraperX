#pragma once
#include "sim/rigging_system.hpp"

namespace scraperx::sim {
inline constexpr std::uint64_t kRiggingFrameEntity = 1494, kRiggingReceiverEntity = 1495;
inline constexpr std::uint64_t kRiggingPlatformEntity = 2493, kRiggingWeightEntity = 2494;
inline constexpr std::uint64_t kRiggingLeverEntity = 2495;
inline const JPH::RVec3 kRiggingBayOrigin{-120, 0, -36};
// Chosen pilot: 85kg carriage, 120kg counterweight tray, three25kg ballast
// blocks. With the actual85kg rider aboard, two blocks balance; three lift.
void build_rigging_bay(kit::Kit &, RiggingSystem &);
} // namespace scraperx::sim
