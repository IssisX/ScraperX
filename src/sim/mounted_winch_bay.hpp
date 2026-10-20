#pragma once
#include "sim/rigging_system.hpp"

namespace scraperx::sim {
inline constexpr std::uint64_t kMountedWinchFrameEntity = 1497;
inline constexpr std::uint64_t kMountedWinchBeamEntity = 2499;
inline constexpr std::uint64_t kMountedWinchEntryEntity = 1498;
inline const JPH::RVec3 kMountedWinchBayOrigin{-108, 0, -36};
// Open yard workstation beside the completed counterweight bay. The loose
// beam has all six physical DOFs; the winch supplies only real cable tension.
void build_mounted_winch_bay(kit::Kit &, RiggingSystem &);
} // namespace scraperx::sim
