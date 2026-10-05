#pragma once
#include "sim/mechanism_kit.hpp"
namespace scraperx::sim {
inline constexpr std::uint64_t kFacadeRouteEntityId = 1600;
// The original landing identity is retained; the other existing structural
// groups have distinct dynamic owners. No geometry or traversal layout changes.
inline constexpr std::uint64_t kFacadeFirstDynamicEntityId = 2560;
inline constexpr std::uint64_t kFacadeLastDynamicEntityId = 2566;
[[nodiscard]] constexpr bool is_facade_route_entity(std::uint64_t entity) noexcept {
    return entity == kFacadeRouteEntityId ||
        (entity >= kFacadeFirstDynamicEntityId && entity <= kFacadeLastDynamicEntityId);
}
// These assemblies are bolted into the static tower at their named mount sites.
// Their embedded attachment faces use the mount, rather than duplicate contact.
[[nodiscard]] constexpr bool is_facade_tower_attachment(std::uint64_t entity) noexcept {
    return entity == kFacadeRouteEntityId || entity == 2561 || entity == 2562 ||
        entity == 2563 || entity == 2565;
}
void build_facade_route(kit::Kit &kit);
}
