#pragma once
#include "sim/cargo_net.hpp"
namespace scraperx::sim {
inline constexpr std::uint64_t kCatchNetEntity = 1963;
inline constexpr std::uint64_t kCatchNetFoundationEntity = 1964;
inline constexpr std::uint64_t kCatchNetFrameEntity = 2963;
[[nodiscard]] CargoNetConfig catch_net_config();
[[nodiscard]] kit::BodyIndex build_catch_net_route(kit::Kit &kit);
}
