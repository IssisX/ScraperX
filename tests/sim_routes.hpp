#pragma once

// Functions one test file calls out of another.

#include "sim_checks.hpp"

bool seat_wet_spool(scraperx::sim::Simulation &simulation);
bool throw_wet_fill(scraperx::sim::Simulation &simulation);
bool shut_wet_door(scraperx::sim::Simulation &simulation);
bool pull_wet_trip(scraperx::sim::Simulation &simulation);
bool couple_wet_hose(scraperx::sim::Simulation &simulation);
bool pull_wet_stop_valve(scraperx::sim::Simulation &simulation);
bool on_support(const scraperx::sim::Snapshot &state, const std::uint64_t entity);
void run_wet_isolation();
void run_wet_band();
bool climb_wet_hold(scraperx::sim::Simulation &simulation, const double x, const double z,
                    const double fx, const double fz, const bool jump, const double top);
void run_wet_route();
bool walk_to_shop_cleat(scraperx::sim::Simulation &simulation);
bool rig_shop_g(scraperx::sim::Simulation &simulation);
bool pull_shop_g(scraperx::sim::Simulation &simulation);
bool pull_shop_girder_pin(scraperx::sim::Simulation &simulation);
bool board_shop_h(scraperx::sim::Simulation &simulation);
bool pull_shop_chock(scraperx::sim::Simulation &simulation);
bool rig_shop_i(scraperx::sim::Simulation &simulation);
bool pull_shop_domino(scraperx::sim::Simulation &simulation);
void run_plate_shop();
void run_plate_band();
void run_plate_route();
bool climb_wreck(scraperx::sim::Simulation &simulation, const double x, const double z, const double fx,
                 const double fz, const std::uint64_t entity, const double top);
void run_plate_wreckage();
void run_facade_crane();
bool climb_crane_band(scraperx::sim::Simulation &band);
void run_crane_band();
void run_crane_route();
void run_crane_wreckage();
void run_ascent();
void run_stack();
void run_service_skin();
bool climb_girder(scraperx::sim::Simulation &floor);
bool climb_shutter(scraperx::sim::Simulation &floor);
bool climb_ramp(scraperx::sim::Simulation &floor);
bool climb_gate(scraperx::sim::Simulation &floor);
bool climb_cart(scraperx::sim::Simulation &floor);
bool climb_slats(scraperx::sim::Simulation &floor);
bool climb_duct(scraperx::sim::Simulation &floor);
bool climb_casing(scraperx::sim::Simulation &floor);
bool climb_pitman(scraperx::sim::Simulation &floor);
bool climb_wheel(scraperx::sim::Simulation &floor);
bool climb_wheel_ladder(scraperx::sim::Simulation &floor);
bool miss_wheel(scraperx::sim::Simulation &floor);
bool climb_above_ring220(scraperx::sim::Simulation &run, const double start_seconds,
                         const std::uint64_t deaths_at_start, const char *pass_name);
bool climb_brace(scraperx::sim::Simulation &floor);
bool miss_brace(scraperx::sim::Simulation &floor);
void run_girder();

void run_proving_ground();
void run_campaign();
