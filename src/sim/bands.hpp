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

    // Stage B, the derrick boom.
    kit::BodyIndex b_cage;
    kit::BodyIndex b_boom;
    kit::BodyIndex b_shackle;
    kit::BodyIndex b_lever_body;
    kit::BodyIndex b_handle;
    kit::BodyIndex b_striker_body;
    kit::GuideIndex b_cage_guide;
    kit::RopeIndex b_rope;
    kit::LeverIndex b_boom_hinge;
    kit::LeverIndex b_lever;
    kit::LeverIndex b_striker;
    kit::CatchIndex b_catch;
    kit::SlipIndex b_slip;
    kit::AnchorIndex b_cage_anchor;
    kit::AnchorIndex b_cleat_anchor;

    // Stage C, the debris chute, and its cascade into A.
    kit::BodyIndex c_platform;
    kit::BodyIndex c_dumpster;
    kit::BodyIndex c_latch_body;
    kit::BodyIndex c_rebar_body;
    kit::BodyIndex c_handle;
    kit::BodyIndex c_striker_body;
    kit::BodyIndex a_tip_body;
    kit::BodyIndex a_tip_handle;
    kit::BodyIndex c_latch_handle;
    kit::GuideIndex c_platform_guide;
    kit::GuideIndex c_dumpster_guide;
    kit::RopeIndex c_rope;
    kit::LeverIndex c_latch;
    kit::LeverIndex c_rebar;
    kit::LeverIndex c_striker;
    kit::LeverIndex a_tip;
    kit::CatchIndex c_catch;
    kit::BinIndex c_hopper;
    kit::BinIndex c_dumpster_bin;
    kit::BinIndex a_cage_bin;
};

void build_counterweight_well(kit::Kit &kit, CounterweightWell &well);

} // namespace scraperx::sim::bands
