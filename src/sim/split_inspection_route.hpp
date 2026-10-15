#pragma once

#include "sim/mechanism_kit.hpp"

#include <cstdint>

namespace scraperx::sim {

inline constexpr std::uint64_t kSplitInspectionCrossheadEntity = 1973;
inline constexpr std::uint64_t kSplitInspectionReceiverEntity = 1974;
inline constexpr std::uint64_t kSplitInspectionGirderEntity = 1978;
inline constexpr std::uint64_t kSplitInspectionHatchEntity = 1979;
inline constexpr std::uint64_t kSplitInspectionStopsEntity = 1997;
// 1980..1996 are reserved by supplied ascent machines, including unused slots.
inline constexpr std::uint64_t kSplitInspectionBalanceBlockEntity = 1902;
inline constexpr float kSplitInspectionBalanceBlockMassKg = 25.0F;
inline constexpr std::uint64_t kSplitInspectionBearingEntity = 1903;
inline constexpr std::uint64_t kSplitInspectionSpanSeatEntity = 1904;
inline constexpr std::uint64_t kSplitInspectionCableFrameEntity = 1905;
inline const JPH::RVec3 kSplitInspectionSpanPivot{10,484.10,-134.15};
inline constexpr float kSplitInspectionSpanRaisedAngle = -.075F;
inline const JPH::Vec3 kSplitInspectionSpanCablePoint{7.9F,483.225F,-135.2F};
inline const JPH::Vec3 kSplitInspectionHatchCableLocal{0,-.3F,-1.0F};
inline const JPH::RVec3 kSplitInspectionLowerSheave{-19.35,471.8,-135.2};
inline const JPH::RVec3 kSplitInspectionUpperSheave{7.9,481.5,-135.2};
inline const JPH::RVec3 kSplitInspectionHatchPivot{-19.35, 473.2, -134.3};

inline constexpr float kSplitInspectionTopY = 473.25F;
inline constexpr float kSplitInspectionExitTopY = 484.25F;
inline constexpr float kSplitInspectionGapNorthZ = -130.2F;
inline constexpr float kSplitInspectionGapSouthZ = -132.3F;
inline constexpr float kSplitInspectionGirderZ = -134.15F;
inline constexpr float kSplitInspectionGirderWidth = .55F;
inline constexpr float kSplitInspectionDuckX = -3.0F;
inline constexpr float kSplitInspectionDuckClearance = 1.65F;
inline const JPH::Vec3 kSplitInspectionCrossheadLow{-21.5F, 473.234F, -130.2F};
inline const JPH::Vec3 kSplitInspectionCrossheadHigh{-18.0F, 473.25F, -129.3F};
inline const JPH::Vec3 kSplitInspectionReceiverLow{-20.7F, 473.234F, -134.6F};
inline const JPH::Vec3 kSplitInspectionReceiverHigh{-18.0F, 473.25F, -132.3F};
inline const JPH::Vec3 kSplitInspectionGirderToe{-18.0F, 473.25F, -134.15F};
inline const JPH::Vec3 kSplitInspectionGirderExit{10.0F, 484.25F, -134.15F};

[[nodiscard]] constexpr bool is_split_inspection_entity(
    const std::uint64_t entity) noexcept {
    return entity == kSplitInspectionCrossheadEntity ||
           entity == kSplitInspectionReceiverEntity ||
           entity == kSplitInspectionGirderEntity ||
           entity == kSplitInspectionHatchEntity ||
           entity == kSplitInspectionStopsEntity ||
           entity == kSplitInspectionBalanceBlockEntity ||
           entity == kSplitInspectionBearingEntity ||
           entity == kSplitInspectionSpanSeatEntity ||
           entity == kSplitInspectionCableFrameEntity;
}

// Finite steel inspection access, mounted to the existing tower backbone.
// The original 462m roof/brace and 484m roof retain their world-solid authority.
void build_split_inspection_route(kit::Kit &kit);

} // namespace scraperx::sim
