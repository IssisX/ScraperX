#pragma once

#include "sim/mechanism_kit.hpp"

#include <cstdint>

namespace scraperx::sim {

inline constexpr std::uint64_t kPipeServiceEntity = 1939;
inline constexpr float kPipeServiceTopY = 451.0F;
inline constexpr float kPipeServiceLowerBraceZ = -127.64F;
inline constexpr float kPipeServiceUpperBraceZ = -128.095F;
inline constexpr float kPipeServiceLowerBraceRun = 22.36F;
inline constexpr float kPipeServiceUpperBraceRun = 21.905F;

inline const JPH::Vec3 kPipeServiceEntryLow{-22.3F, 450.72F, -131.25F};
inline const JPH::Vec3 kPipeServiceEntryHigh{-19.2F, 451.0F, -128.85F};
inline const JPH::Vec3 kPipeServiceFloorLow{-19.2F, 450.72F, -131.25F};
inline const JPH::Vec3 kPipeServiceFloorHigh{-1.0F, 451.0F, -128.85F};
inline constexpr float kPipeServiceQuickLaneZ = -129.375F;
inline constexpr float kPipeServiceQuickLaneWidth = 1.05F;
inline const JPH::Vec3 kPipeServicePipeCentre{-15.0F, 451.5F, -129.375F};
inline constexpr float kPipeServicePipeRadius = .5F;
inline constexpr float kPipeServicePipeHalfLength = .525F;
// Maximum includes the real 0.53m-radius blind end flanges.
inline constexpr float kPipeServicePipeTopY = 452.03F;

inline constexpr float kPipeServiceShelterLaneZ = -130.65F;
inline constexpr float kPipeServiceShelterStartX = -17.2F;
inline constexpr float kPipeServiceShelterEndX = -7.2F;
inline constexpr float kPipeServiceShelterClearWidth = 1.05F;
inline constexpr float kPipeServiceShelterHeadroom = 1.45F;
inline constexpr float kPipeServiceShelterRoofThickness = .3F;
inline const JPH::Vec3 kPipeServiceToeLow{-1.3F, 450.72F, -131.25F};
inline const JPH::Vec3 kPipeServiceToeHigh{.9F, 451.0F, -127.75F};

[[nodiscard]] constexpr bool is_pipe_service_entity(std::uint64_t entity) noexcept {
    return entity == kPipeServiceEntity;
}

// Static Kit collision and presentation parts. Native walking, ballistic
// Jump, crouch, falling and the original Tower roofs supply every motion.
void build_pipe_service_route(kit::Kit &kit);

} // namespace scraperx::sim
