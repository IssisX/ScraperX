#pragma once

// C7, the west face: an exposed climb from the 264 ring to the walkway between
// the cascade mast's top and the pitman lift (291.25 m), the alternative to
// riding the mast. Out over the drop on the tower's west side, worked by the
// body's acrobatics, not a machine:
//   1. a kentledge on the 264 ring's west band to mantle;
//   2. from its north end a running leap to a gallery of five pipes,
//      swung along hand over hand, the last swing onto a platform's lip;
//   3. a hang up onto an outrigger out over the drop, and out along it;
//   4. under its far end, a standing leap to a gallery of eight pipes each
//      0.3 m higher than the last, swung up to a second platform's lip;
//   5. three hang-ups, platform to platform, onto the 286 ring's west band;
//   6. a block on the band, a step on it, and a hang up onto the walkway.
// Every piece is placed so it cannot be skipped from below: each platform
// stands out of a hang's reach of what is under it.

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

class ClimbC7 final {
public:
    static constexpr std::uint64_t kFrameEntity = 2977;

    static constexpr float kRing264Top = 264.25F;
    static constexpr float kLaneOneX = -19.2F;        // gallery one's lane, along -z
    static constexpr float kKentledgeTop = 265.85F;
    static constexpr float kKentledgeNorth = -139.0F; // its north end: the take-off
    static constexpr float kGalleryOneY = 268.95F;    // its pipes' axis
    static constexpr float kGalleryOneFirstZ = -139.8F;
    static constexpr int kGalleryOnePipes = 5;
    static constexpr float kPlatformOneTop = 269.05F;
    static constexpr float kPlatformOneSouth = -144.7F;
    static constexpr float kOutriggerTop = 272.3F;
    static constexpr float kOutriggerSouth = -148.0F;
    static constexpr float kOutriggerWestX = -30.5F;
    static constexpr float kLaneTwoX = -29.3F;        // gallery two's lane, along -z
    static constexpr float kGalleryTwoFirstY = 274.9F;
    static constexpr float kGalleryTwoFirstZ = -149.3F;
    static constexpr float kGalleryTwoPitch = 0.95F;  // between pipes along the lane
    static constexpr float kGalleryTwoRise = 0.3F;    // each pipe over the last
    static constexpr int kGalleryTwoPipes = 8;
    static constexpr float kPlatformTwoTop = 277.1F;
    static constexpr float kPlatformThreeTop = 280.35F;
    static constexpr float kPlatformFourTop = 283.6F;
    static constexpr float kRing286Top = 286.25F;
    static constexpr float kRing286Edge = -20.55F;   // its west band's outer edge (probed)
    static constexpr float kBlockTop = 287.85F;      // 1.6 m to mantle
    static constexpr float kBlockStepTop = 288.15F;  // a stair's step on it; 3.1 m below the walkway
    static constexpr float kWalkwayTop = 291.25F;

    static void build(kit::Kit &kit);
};

} // namespace scraperx::sim
