#pragma once

#include "sim/mechanism_kit.hpp"

#include <cstdint>

namespace scraperx::sim {

inline constexpr std::uint64_t kWestBraceBayEntity = 1934;

[[nodiscard]] constexpr bool is_west_brace_bay_entity(
    const std::uint64_t entity) noexcept {
    return entity == kWestBraceBayEntity;
}

// A fixed, collision-honest climb structure at the west brace bay. Kit owns
// the compound's shared render and collision geometry; traversal stays native.
void build_west_brace_bay_route(kit::Kit &kit);

} // namespace scraperx::sim
