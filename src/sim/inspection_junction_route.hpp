#pragma once

#include "sim/mechanism_kit.hpp"

#include <cstdint>

namespace scraperx::sim {

inline constexpr std::uint64_t kInspectionFrameEntity = 1935;
inline constexpr std::uint64_t kInspectionRecoveryEntity = 1936;
inline constexpr std::uint64_t kInspectionSwingEntity = 2935;

inline constexpr float kInspectionJunctionTopY = 407.0F;
inline constexpr float kInspectionJunctionRecoveryTopY = 403.5F;
inline constexpr float kInspectionJunctionWalkZ = -128.2F;
inline constexpr float kInspectionJunctionWalkWidth = .65F;
inline constexpr float kInspectionJunctionLeftEndX = -16.0F;
inline constexpr float kInspectionJunctionRightStartX = -11.6F;
inline const JPH::RVec3 kInspectionSwingPivot{-14.0, 411.8, -128.2};
inline constexpr float kInspectionJunctionSwingLength = 3.0F;
inline constexpr float kInspectionJunctionSwingMassKg = 40.0F;

inline const JPH::Vec3 kInspectionJunctionArrivalLow{-24.0F, 406.72F, -129.2F};
inline const JPH::Vec3 kInspectionJunctionArrivalHigh{-19.2F, 407.0F, -126.35F};
inline const JPH::Vec3 kInspectionJunctionReceiverLow{-.8F, 406.72F, -128.9F};
inline const JPH::Vec3 kInspectionJunctionReceiverHigh{1.4F, 407.0F, -125.7F};
inline const JPH::Vec3 kInspectionJunctionRecoveryLow{-23.0F, 403.22F, -130.3F};
inline const JPH::Vec3 kInspectionJunctionRecoveryHigh{-4.8F, 403.5F, -126.35F};
inline const JPH::Vec3 kInspectionJunctionRecoveryBraceLow{-19.2F, 403.5F, -129.65F};
inline const JPH::Vec3 kInspectionJunctionRecoveryBraceHigh{-11.6F, 407.0F, -129.65F};

[[nodiscard]] constexpr bool is_inspection_junction_swing_entity(
    const std::uint64_t entity) noexcept {
    return entity == kInspectionSwingEntity;
}

[[nodiscard]] constexpr bool is_inspection_junction_entity(
    const std::uint64_t entity) noexcept {
    return entity == kInspectionFrameEntity ||
           entity == kInspectionRecoveryEntity ||
           is_inspection_junction_swing_entity(entity);
}

// Shared native/render inspection footing and a passive mounted hanger.
// Kit owns bodies, hinge and checkpoint state. The traversal owner supplies
// actual hand coupling, voluntary pumping, departure and receiving contact.
void build_inspection_junction_route(kit::Kit &kit);

} // namespace scraperx::sim
