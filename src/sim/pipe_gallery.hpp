#pragma once

// Parkour step 2's place in the yard: the pipe gallery, south of the trapeze.
// A ramp up to a 1.5 m start stand; from its east edge, ten pipes a metre
// apart 4.6 m up, hung between two beams on posts; past the last, a platform
// 4.7 m up whose lip the hand takes from the front of a swing; a ramp back
// down to the yard. A toy, like the trapeze beside it: run, jump, swing hand
// over hand, take the lip, climb up.

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

class PipeGallery final {
public:
    static constexpr std::uint64_t kFrameEntity = 1999;

    static constexpr float kLaneZ = -34.0F;
    static constexpr float kStandWestX = 23.0F;
    static constexpr float kStandEastX = 26.0F;
    static constexpr float kStandTop = 1.5F;
    static constexpr float kFirstPipeX = 26.8F;
    static constexpr float kPipeSpacing = 1.0F;
    static constexpr int kPipeCount = 10;
    static constexpr float kPipeY = 4.6F;    // the pipes' axis
    static constexpr float kPlatformWestX = 36.7F;
    static constexpr float kPlatformEastX = 39.7F;
    static constexpr float kPlatformTop = 4.7F;

    static void build(kit::Kit &kit);
};

} // namespace scraperx::sim
