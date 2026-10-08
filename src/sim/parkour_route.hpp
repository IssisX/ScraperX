#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kParkourFrameEntity = 1930;
inline constexpr std::uint64_t kParkourSwingEntity = 2930;
inline constexpr std::uint64_t kParkourRecoveryEntity = 1931;
inline const JPH::RVec3 kParkourSwingPivot{-36.95, 207.5, -159.8};
inline constexpr float kParkourSwingLength = 7.0F;
inline constexpr float kParkourSwingMassKg = 40.0F;

[[nodiscard]] constexpr bool is_parkour_swing_entity(std::uint64_t entity) noexcept {
    return entity == kParkourSwingEntity;
}

[[nodiscard]] constexpr bool is_parkour_route_entity(std::uint64_t entity) noexcept {
    return entity == kParkourFrameEntity || is_parkour_swing_entity(entity) ||
           entity == kParkourRecoveryEntity;
}

// Normal-world Crown exit: passive hanging arm and fixed recovery footing.
// Kit owns the bodies, hinge, shared collision/render geometry and checkpoint.
void build_parkour_route(kit::Kit &kit);

} // namespace scraperx::sim
