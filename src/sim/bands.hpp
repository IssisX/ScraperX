#pragma once

// The bands of the mechanism ascent, each built from the mechanism kit.
// 03_EXECUTION/ASCENT/AS-006_CW_PIN.md is the Counterweight Well's contract,
// AS-007_WET_ISOLATION.md Wet Isolation's.

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

// AS-007, Atlas band B03, 220 -> 340 m. 03_EXECUTION/ASCENT/AS-007_WET_ISOLATION.md.
struct WetIsolation final {
    // Stage D, the float ram.
    kit::BodyIndex d_platform;       // platform, mast and float: one body
    kit::BodyIndex d_spool;
    kit::BodyIndex d_valve_body;
    kit::BodyIndex d_valve_handle;
    kit::BodyIndex d_drain_body;
    kit::BodyIndex d_drain_handle;
    kit::GuideIndex d_guide;
    kit::LeverIndex d_valve;
    kit::LeverIndex d_drain;
    kit::PoolIndex d_tank;
    kit::PoolIndex d_tube;
    kit::PipeIndex d_fill;
    kit::PipeIndex d_drain_pipe;

    // Stage E, the chiller drop.
    kit::BodyIndex e_cab;
    kit::BodyIndex e_chiller;
    kit::BodyIndex e_door;
    kit::BodyIndex e_latch_body;
    kit::BodyIndex e_lever_body;
    kit::BodyIndex e_handle;
    kit::BodyIndex e_bucket;
    kit::BodyIndex e_striker_body;
    kit::GuideIndex e_cab_guide;
    kit::GuideIndex e_chiller_guide;
    kit::GuideIndex e_bucket_guide;
    kit::LeverIndex e_door_hinge;
    kit::LeverIndex e_latch;
    kit::LeverIndex e_lever;
    kit::LeverIndex e_striker;
    kit::CatchIndex e_door_catch;
    kit::CatchIndex e_catch;
    kit::CellIndex e_duct_cell;
    kit::CellIndex e_cab_cell;
    kit::RopeIndex e_rope;
    kit::BinIndex e_bucket_bin;

    // Stage F, the accumulator.
    kit::BodyIndex f_platform;
    kit::BodyIndex f_accumulator;
    kit::BodyIndex f_hose;
    kit::BodyIndex f_lever_body;
    kit::BodyIndex f_handle;
    kit::GuideIndex f_platform_guide;
    kit::GuideIndex f_accumulator_guide;
    kit::LeverIndex f_lever;
    kit::CatchIndex f_catch;
    kit::RopeIndex f_line;
    kit::AnchorIndex f_inlet;

    // The header on TP-340 and its cascade.
    kit::BodyIndex header_dump_body;
    kit::BodyIndex header_dump_handle;
    kit::LeverIndex header_dump;
    kit::PoolIndex header;
    kit::PipeIndex header_to_tank;
    kit::PipeIndex header_to_bucket;
};

void build_wet_isolation(kit::Kit &kit, WetIsolation &wet);

} // namespace scraperx::sim::bands
