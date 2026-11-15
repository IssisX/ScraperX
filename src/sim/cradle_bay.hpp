#pragma once

// J-054, the Counterweight Crossing (#293): the bay's frame, off the 220
// ring's south face. A gangway out from the ring's edge to a boarding deck,
// a gap the cradle spans, and an exit deck high above the cradle's pivot,
// held by posts and a head beam. The frame is static; the cradle itself
// (cradle.hpp) is the moving body, a lever on a hinge fixed to the world.
//
// The crossing, in one line: the exit deck stands 2.05 m over the ring's
// deck, so the cradle's far lip is within a jump of it only when the cradle
// is tipped toward it (its far lip up, 0.88 m under the exit deck); level it
// is 2.05 m under and tipped away 3.22 m under, both past a jump's reach.
// The drum on the boarding deck is the ballast that tips it toward.
//
// This header is the bay's single source of geometry: the frame, the cradle
// and the tests all read these numbers, so a moved deck cannot strand the
// mechanism on a stale constant.

#include "sim/mechanism_kit.hpp"

#include <Jolt/Jolt.h>

#include <cstdint>

namespace scraperx::sim {

struct CradleBay final {
    static constexpr std::uint64_t kFrameEntity = 1660;

    // The bay's centreline, and the 220 ring's south face (its outer edge).
    static constexpr float kBayCenterX = 5.0F;
    static constexpr float kRingFaceZ = -126.73F;
    static constexpr float kRingDeckTop = 220.25F;

    // The gangway from the ring's edge to the boarding deck.
    static constexpr float kGangwayHalfX = 1.0F;
    static constexpr float kGangwayEndZ = -118.0F;
    // The boarding deck, where the drum is staged.
    static constexpr float kBoardingWestX = 2.0F;
    static constexpr float kBoardingEastX = 8.0F;
    static constexpr float kBoardingSouthZ = -114.0F;
    static constexpr float kBoardingTop = 220.25F;
    // The gap the cradle spans, boarding deck to exit deck.
    static constexpr float kGapNorthZ = -114.0F;
    static constexpr float kGapSouthZ = -106.0F;
    // The exit deck, high over the pivot: the far lip reaches it only when
    // the cradle is tipped toward it.
    static constexpr float kExitWestX = 2.0F;
    static constexpr float kExitEastX = 8.0F;
    static constexpr float kExitNorthZ = -106.0F;
    static constexpr float kExitSouthZ = -102.0F;
    static constexpr float kExitTop = 222.30F;

    // The cradle's pivot: the hinge fixed to the world, at the gap's middle,
    // level with the boarding deck.
    static constexpr float kPivotY = 220.25F;
    static constexpr float kPivotZ = -110.0F;
    // The cradle's deck: 7.6 m long, so its lips stand 0.2 m off the decks
    // either side when level; 2.4 m wide; 0.15 m thick.
    static constexpr float kCradleHalfLength = 3.8F;
    static constexpr float kCradleHalfWidth = 1.2F;
    static constexpr float kCradleThick = 0.15F;
    // The hinge's stops, either way. At 18 deg a lip stands 1.17 m off
    // level: tipped toward, the far lip is 0.88 m under the exit deck; level
    // 2.05 m under; tipped away 3.22 m under.
    static constexpr float kTiltMaxRad = 0.314F;

    // The head beam over the pivot, and the posts that carry it. The hanger
    // down to the pivot ends 0.15 m over the cradle deck's top at the pivot:
    // the deck's top rises 0.062 m at the hanger's edge when the cradle
    // swings to a stop, and a hanger sitting on the deck wedges it level.
    static constexpr float kBeamY = 224.5F;
    static constexpr float kHangerBottomY = kPivotY + 0.15F;

    static void build(kit::Kit &kit);
};

} // namespace scraperx::sim
