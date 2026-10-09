#pragma once

#include "sim/mechanism_kit.hpp"
#include "sim/west_brace_bay_route.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kParkourFrameEntity = 1930;
inline constexpr std::uint64_t kParkourSwingEntity = 2930;
inline constexpr std::uint64_t kParkourRecoveryEntity = 1931;
inline constexpr std::uint64_t kTaperInspectionEntity = 1932;
inline constexpr std::uint64_t kTaperRecoveryEntity = 1933;
inline const JPH::RVec3 kParkourSwingPivot{-36.95, 207.5, -159.8};
inline constexpr float kParkourSwingLength = 7.0F;
inline constexpr float kParkourSwingMassKg = 40.0F;

[[nodiscard]] constexpr bool is_parkour_swing_entity(std::uint64_t entity) noexcept {
    return entity == kParkourSwingEntity;
}

[[nodiscard]] constexpr bool is_parkour_route_entity(std::uint64_t entity) noexcept {
    return entity == kParkourFrameEntity || is_parkour_swing_entity(entity) ||
           entity == kParkourRecoveryEntity || entity == kTaperInspectionEntity ||
           entity == kTaperRecoveryEntity || is_west_brace_bay_entity(entity);
}

// Normal-world Crown exit: passive hanging arm and fixed recovery footing.
// Kit owns the bodies, hinge, shared collision/render geometry and checkpoint.
void build_parkour_route(kit::Kit &kit);

// First-taper brace inspection: fixed girders, gap and crouched service path
// from the +352m ring to the existing +374.25m Tower floor. Kit owns the
// shared collision/render parts; ordinary native traversal owns all motion.
void build_taper_inspection_route(kit::Kit &kit);

} // namespace scraperx::sim
