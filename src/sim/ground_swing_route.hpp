#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kGroundSwingFrameEntity = 1480;
inline constexpr std::uint64_t kGroundSwingRecoveryEntity = kGroundSwingFrameEntity;
inline constexpr std::uint64_t kGroundSwingFirstEntity = 2480;
inline constexpr std::uint64_t kGroundSwingSecondEntity = 2481;
inline const JPH::RVec3 kGroundSwingFirstPivot{-49.0, 4.2, -59.0};
inline const JPH::RVec3 kGroundSwingSecondPivot{-47.9, 4.2, -59.0};
inline constexpr float kGroundSwingLength = 1.2F;
inline constexpr float kGroundSwingMassKg = 12.0F;

[[nodiscard]] constexpr bool is_ground_swing_entity(std::uint64_t entity) noexcept {
    return entity == kGroundSwingFirstEntity || entity == kGroundSwingSecondEntity;
}

[[nodiscard]] inline JPH::RVec3 ground_swing_pivot(std::uint64_t entity) noexcept {
    return entity == kGroundSwingSecondEntity ? kGroundSwingSecondPivot
                                             : kGroundSwingFirstPivot;
}

// Ground practice bay: real stairs, entry/rest pads, fixed bearing frame and
// two passive hangers. Kit supplies the same parts to collision and rendering.
void build_ground_swing_route(kit::Kit &kit);

} // namespace scraperx::sim
