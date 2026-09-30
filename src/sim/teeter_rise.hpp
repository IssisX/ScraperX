#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

// AS-020: an exterior, player-loaded rocker with a hand-positioned captive
// ballast. It crosses from the +66 m ring to a lower service shelf and an
// exposed climb to the +77 m ring. Jolt/Kit owns both moving bodies and
// their constraints; no occupancy response drives the beam.
class TeeterRise final {
public:
    explicit TeeterRise(kit::Kit &kit);
};

} // namespace scraperx::sim
