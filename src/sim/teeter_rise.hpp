#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

// AS-020: an exterior, player-loaded rocker from the +66 m ring to a
// lower service shelf and exposed climb to the +77 m ring. All moving
// state belongs to Jolt/Kit; there is no per-player or occupancy update.
class TeeterRise final {
public:
    explicit TeeterRise(kit::Kit &kit);
};

} // namespace scraperx::sim
