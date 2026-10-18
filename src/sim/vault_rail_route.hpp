#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

inline constexpr std::uint64_t kVaultRailEntity = 1461;
inline constexpr float kVaultRailTopM = 1.65F;

// Grade-bearing steel service rail. Kit owns the same three physical and
// rendered parts; ordinary contact and traversal own the player's movement.
void build_vault_rail_route(kit::Kit &kit);

} // namespace scraperx::sim
