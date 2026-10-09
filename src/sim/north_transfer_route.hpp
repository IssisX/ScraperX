#pragma once

#include "sim/mechanism_kit.hpp"

#include <cstdint>

namespace scraperx::sim {

inline constexpr std::uint64_t kNorthTransferEntity = 1938;
inline constexpr float kNorthTransferTopY = 429.0F;
inline constexpr float kNorthTransferLowerBraceZ = -126.73F;
inline constexpr float kNorthTransferUpperBraceZ = -127.185F;
inline constexpr float kNorthTransferUpperBraceRun = 22.815F;

inline const JPH::Vec3 kNorthTransferEntryLow{-22.3F, 428.72F, -129.2F};
inline const JPH::Vec3 kNorthTransferEntryHigh{-19.2F, 429.0F, -127.1F};
inline const JPH::Vec3 kNorthTransferRailStart{-19.0F, 431.1F, -127.7F};
inline const JPH::Vec3 kNorthTransferRailEnd{-15.7F, 431.1F, -127.7F};
inline constexpr float kNorthTransferRailHalfSection = .055F;
inline constexpr float kNorthTransferHeaderUndersideOffset = .55F;
inline constexpr float kNorthTransferHeaderTopOffset = .85F;

inline const JPH::Vec3 kNorthTransferHandReceiverLow{-16.2F, 428.72F, -129.2F};
inline const JPH::Vec3 kNorthTransferHandReceiverHigh{-14.2F, 429.0F, -127.1F};
inline constexpr float kNorthTransferFootBeamStartX = -14.2F;
inline constexpr float kNorthTransferFootBeamEndX = -8.4F;
inline constexpr float kNorthTransferFootBeamZ = -128.4F;
inline constexpr float kNorthTransferFootBeamWidth = .4F;
inline const JPH::Vec3 kNorthTransferLaunchLow{-10.0F, 428.72F, -128.85F};
inline const JPH::Vec3 kNorthTransferLaunchHigh{-8.4F, 429.0F, -127.95F};
inline const JPH::Vec3 kNorthTransferCrouchRestLow{-10.5F, 428.72F, -130.1F};
inline const JPH::Vec3 kNorthTransferCrouchRestHigh{-8.5F, 429.0F, -129.0F};
inline constexpr float kNorthTransferCrouchHeadroom = 1.45F;

inline constexpr float kNorthTransferGapLength = 3.2F;
inline const JPH::Vec3 kNorthTransferGapReceiverLow{-5.2F, 428.72F, -129.65F};
inline const JPH::Vec3 kNorthTransferGapReceiverHigh{-3.6F, 429.0F, -128.65F};
inline const JPH::Vec3 kNorthTransferToeLow{-1.2F, 428.72F, -129.15F};
inline const JPH::Vec3 kNorthTransferToeHigh{.9F, 429.0F, -126.85F};

[[nodiscard]] constexpr bool is_north_transfer_entity(
    const std::uint64_t entity) noexcept {
    return entity == kNorthTransferEntity;
}

// Kit owns one fixed compound body and the identical native/render parts.
// Existing walking, hand coupling, crouch and Jump owners supply all motion.
void build_north_transfer_route(kit::Kit &kit);

} // namespace scraperx::sim
