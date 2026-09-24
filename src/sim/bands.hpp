#pragma once

// The bands of the mechanism ascent, each built from the mechanism kit.
// 03_EXECUTION/ASCENT/AS-006_CW_PIN.md is the Counterweight Well's contract.

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim::bands {

// AS-006, Atlas band B02, 154 -> 220 m.
struct CounterweightWell final {
    // Stage A, the skip lift.
    kit::BodyIndex a_cage;
    kit::BodyIndex a_skip;
    kit::BodyIndex a_shackle;
    kit::BodyIndex a_lever_body;
    kit::BodyIndex a_handle;
    kit::GuideIndex a_cage_guide;
    kit::GuideIndex a_skip_guide;
    kit::RopeIndex a_rope;
    kit::LeverIndex a_lever;
    kit::CatchIndex a_catch;
    kit::AnchorIndex a_cage_anchor;
    kit::AnchorIndex a_bollard_anchor;
};

void build_counterweight_well(kit::Kit &kit, CounterweightWell &well);

} // namespace scraperx::sim::bands
