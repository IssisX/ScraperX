#pragma once

// The bands of the mechanism ascent, each built from the mechanism kit.
// 03_EXECUTION/ASCENT/AS-006_CW_PIN.md is the Counterweight Well's contract,
// AS-007_WET_ISOLATION.md Wet Isolation's, AS-008_PLATE_SHOP.md the Plate
// Shop's, AS-009_FACADE_CRANE_STACK.md the Facade Crane Stack's.

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

// AS-008, Atlas band B04, 340 -> 484 m. 03_EXECUTION/ASCENT/AS-008_PLATE_SHOP.md.
struct PlateShop final {
    // Stage G, the scaffold slump.
    kit::BodyIndex g_platform;
    kit::BodyIndex g_tower;
    kit::BodyIndex g_pin;
    kit::BodyIndex g_handle;
    kit::BodyIndex g_shackle;
    kit::GuideIndex g_platform_guide;
    kit::GuideIndex g_tower_guide;
    kit::CatchIndex g_catch;
    kit::RopeIndex g_rope;
    kit::AnchorIndex g_eye;
    kit::AnchorIndex g_cleat;

    // Stage H, the girder tip.
    kit::BodyIndex h_platform;
    kit::BodyIndex h_girder;
    kit::BodyIndex h_trolley;
    kit::BodyIndex h_girder_pin;
    kit::BodyIndex h_chock;
    kit::BodyIndex h_handle;
    kit::GuideIndex h_platform_guide;
    kit::LeverIndex h_girder_hinge;
    kit::LeverIndex h_chock_lever;
    kit::CatchIndex h_girder_catch;
    kit::CatchIndex h_trolley_catch;
    kit::RopeIndex h_rope;

    // Stage I, the domino and the monolith.
    kit::BodyIndex i_cage;
    kit::BodyIndex i_monolith;
    kit::BodyIndex i_domino;
    kit::BodyIndex i_domino_pin;
    kit::BodyIndex i_trip_body;
    kit::BodyIndex i_handle;
    kit::BodyIndex i_shackle;
    kit::GuideIndex i_cage_guide;
    kit::LeverIndex i_monolith_hinge;
    kit::LeverIndex i_domino_hinge;
    kit::LeverIndex i_trip;
    kit::CatchIndex i_domino_catch;
    kit::CatchIndex i_monolith_catch;
    kit::RopeIndex i_rope;
    kit::AnchorIndex i_eye;
};

void build_plate_shop(kit::Kit &kit, PlateShop &shop);

// AS-009, Atlas band B05, 484 -> 640 m. 03_EXECUTION/ASCENT/AS-009_FACADE_CRANE_STACK.md.
struct FacadeCrane final {
    // Stage J, the traveler and the runaway wagon.
    kit::BodyIndex j_traveler;
    kit::BodyIndex j_wagon;
    kit::BodyIndex j_joint;
    kit::BodyIndex j_chock;
    kit::BodyIndex j_handle;
    kit::GuideIndex j_traveler_guide;
    kit::GuideIndex j_wagon_guide;
    kit::LeverIndex j_chock_lever;
    kit::CatchIndex j_wagon_catch;
    kit::RopeIndex j_rope;

    // Stage K, the crane jib pendulum.
    kit::BodyIndex k_cage;
    kit::BodyIndex k_jib;
    kit::BodyIndex k_pin;
    kit::BodyIndex k_handle;
    kit::BodyIndex k_shackle;
    kit::GuideIndex k_cage_guide;
    kit::LeverIndex k_jib_hinge;
    kit::CatchIndex k_jib_catch;
    kit::RopeIndex k_rope;
    kit::AnchorIndex k_eye;

    // Stage L, the kinetic winch.
    kit::BodyIndex l_cab;
    kit::BodyIndex l_cart;
    kit::BodyIndex l_catch_body;
    kit::BodyIndex l_weight;
    kit::BodyIndex l_pin;
    kit::BodyIndex l_pin_handle;
    kit::BodyIndex l_clutch_body;
    kit::BodyIndex l_clutch_handle;
    kit::GuideIndex l_cab_guide;
    kit::GuideIndex l_cart_guide;
    kit::LeverIndex l_catch_lever;
    kit::LeverIndex l_clutch_lever;
    kit::CatchIndex l_cart_catch;
    kit::CatchIndex l_weight_catch;
    kit::RopeIndex l_rope;
};

void build_facade_crane(kit::Kit &kit, FacadeCrane &crane);

// AS-010, Atlas band B06, the first lift off TP-640.
// 03_EXECUTION/ASCENT/AS-010_MIDSTACK.md.
struct MidstackService final {
    kit::BodyIndex m_cage;
    kit::BodyIndex m_skip;
    kit::BodyIndex m_shackle;
    kit::BodyIndex m_lever_body;
    kit::BodyIndex m_handle;
    kit::GuideIndex m_cage_guide;
    kit::GuideIndex m_skip_guide;
    kit::RopeIndex m_rope;
    kit::LeverIndex m_lever;
    kit::CatchIndex m_catch;
    kit::AnchorIndex m_cage_anchor;
    kit::AnchorIndex m_bollard_anchor;
};

void build_midstack_service(kit::Kit &kit, MidstackService &service);

// The apron chain, on the ground east of the grade spawn.
struct ApronChain final {
    kit::BodyIndex wedge;
    kit::BodyIndex ball;
    kit::BodyIndex pipe[6];
    kit::BodyIndex scale;
    kit::BodyIndex block;
    kit::BodyIndex latch;
    kit::BodyIndex door;
    kit::BodyIndex boulder;
    kit::BodyIndex wreck;
    kit::BodyIndex slab[5];
    kit::BodyIndex seesaw;
    kit::GuideIndex wedge_guide;
    kit::GuideIndex latch_guide;
    kit::LeverIndex scale_hinge;
    kit::LeverIndex door_hinge;
    kit::LeverIndex wreck_hinge;
    kit::LeverIndex seesaw_hinge;
};

void build_apron_chain(kit::Kit &kit, ApronChain &chain);

} // namespace scraperx::sim::bands
