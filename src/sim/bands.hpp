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

    // Stage B, the guided lattice counterweight. The 22 m lattice is stored
    // between the 198 and 220 rings; once released it falls to span 176..198
    // while raising the adjacent cage to the 198 ring.
    kit::BodyIndex b_cage;
    kit::BodyIndex b_counterweight;
    kit::BodyIndex b_shackle;
    kit::BodyIndex b_lever_body;
    kit::BodyIndex b_handle;
    kit::GuideIndex b_cage_guide;
    kit::GuideIndex b_counterweight_guide;
    kit::RopeIndex b_rope;
    kit::LeverIndex b_lever;
    kit::CatchIndex b_catch;
    kit::AnchorIndex b_cage_anchor;
    kit::AnchorIndex b_bollard_anchor;
};

void build_counterweight_well(kit::Kit &kit, CounterweightWell &well);

} // namespace scraperx::sim::bands
