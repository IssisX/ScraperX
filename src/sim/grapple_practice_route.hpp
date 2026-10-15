#pragma once

#include "sim/mechanism_kit.hpp"

#include <cstdint>

namespace scraperx::sim {

inline constexpr std::uint64_t kGrapplePracticeDepartureEntity = 1965;
inline constexpr std::uint64_t kGrapplePracticeCupEntity = 1966;
inline constexpr std::uint64_t kGrapplePracticeReceiverEntity = 1968;
inline constexpr std::uint64_t kGrapplePracticeRecoveryEntity = 1969;
[[nodiscard]] inline constexpr bool is_grapple_practice_route_entity(std::uint64_t entity) noexcept {
    return entity==kGrapplePracticeDepartureEntity || entity==kGrapplePracticeCupEntity ||
           entity==kGrapplePracticeReceiverEntity || entity==kGrapplePracticeRecoveryEntity;
}
inline const JPH::Vec3 kGrapplePracticeTranslation{2.0F, 0.0F, 0.0F};
inline constexpr float kGrapplePracticeDepartureTopY = 484.25F;
inline constexpr float kGrapplePracticeReceiverTopY = 484.266F;
inline constexpr float kGrapplePracticeRecoveryTopY = 481.75F;
// Code-linked Cup identifiers retain the same finite receiver body; geometry
// is now an exposed bar/collars, not a slotted pocket or capture trigger.
inline constexpr float kGrapplePracticeBarRadius = .020F;
inline const JPH::Vec3 kGrapplePracticeBarCenterLocal{0,-.060F,0};
inline const JPH::Vec3 kGrapplePracticeDeparture{12.0F, 484.25F, -135.5F};
inline const JPH::Vec3 kGrapplePracticeReceiver{20.0F, 484.266F, -135.5F};
inline const JPH::Vec3 kGrapplePracticeExit{20.0F, 484.25F, -132.8F};
inline const JPH::Vec3 kGrapplePracticeCupOrigin{16.0F, 490.5F, -135.5F};
inline const JPH::Vec3 kGrapplePracticeBoundsLow{10.8F, 481.35F, -138.3F};
inline const JPH::Vec3 kGrapplePracticeBoundsHigh{22.2F, 492.4F, -131.5F};
// DERIVED high ballistic arc for a muzzle at (12,485.65,-135.5),
// 2kg head and180J:82.478 degrees above horizontal toward +X.
// Aim is advice only; ordinary native launch/contact remains authoritative.
inline const JPH::Vec3 kGrapplePracticeRecommendedAim{.13090F, .99140F, 0.0F};
inline const JPH::Vec3 kGrapplePracticeReferenceMuzzle{12.0F, 485.65F, -135.5F};

struct GrapplePracticeBodies final {
    kit::BodyIndex departure;
    kit::BodyIndex cup;
    kit::BodyIndex receiver;
    kit::BodyIndex recovery;
};

// Finite native steel bay beside the original484.25m north/east roof.
// Crook retention is compound collision only; no hook/receiver joint is created.
[[nodiscard]] GrapplePracticeBodies build_grapple_practice_route(kit::Kit &kit);

} // namespace scraperx::sim
