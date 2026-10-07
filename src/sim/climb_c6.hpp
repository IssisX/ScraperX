#pragma once

// C6, the west band: a climb from the 242 ring to the 264 ring, after the
// swing (AS-012) lands its rider on the 242 ring. No ladder, no standpipe:
// kentledge to mantle, a running leap over a 4.75 m gap caught by the hands,
// a girder out over the void to balance along, an outrigger to hang up onto,
// a girder rising at 24 degrees to walk up, a crossbeam, a block, and the 264
// ring's inner edge.

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

class ClimbC6 final {
public:
    static constexpr std::uint64_t kFrameEntity = 1961;

    // Where each move lands, for tests and the presentation.
    static constexpr float kKentledgeTop = 243.85F;  // the take-off
    static constexpr float kKentledgeNorth = -144.75F;
    static constexpr float kPlatformTop = 247.35F;   // the leap's far side, the girder out, its landing
    static constexpr float kOutriggerTop = 250.65F;
    static constexpr float kInclineTop = 254.85F;    // the inclined girder's head, a platform
    static constexpr float kCrossbeamTop = 258.0F;
    static constexpr float kBlockTop = 261.0F;
    static constexpr float kRing264Top = 264.25F;

    static void build(kit::Kit &kit);
};

} // namespace scraperx::sim
