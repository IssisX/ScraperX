#pragma once

#include <cstdint>

namespace scraperx::tests {

// Immutable scalar diagnostics only: no live world or shared mutable test state.
// Their printing stays at the original point after the first plant checks.
struct SupportReport final {
    std::uint64_t translating_support;
    double translating_vx;
    double jump_vx;
    std::uint64_t rotating_support;
    double rotating_point_vx;
    double rotating_point_vz;
    double omega_y;
};

struct TraversalReport final {
    double vault_x;
    double double_tap_vault_x;
    double vault_speed;
    std::uint64_t mantle_support;
    double mantle_rise_setback;
    double mantle_y;
    std::uint64_t hang_support;
    std::uint64_t moving_hang_support;
    double moving_hang_vz;
    double moving_ledge_vz;
    double release_vz;
    std::uint64_t rejected;
    std::uint64_t accepted;
};

SupportReport run_foundation_support_tests();
TraversalReport run_athletic_traversal_tests();
void print_support_report(const SupportReport &report);
void print_traversal_report(const TraversalReport &report);
void run_plant_recovery_tests(const SupportReport &support, const TraversalReport &traversal);
void run_freight_jib_tests();
void run_structural_needle_tests();
void run_process_sump_tests();
void run_intake_rise_tests();
void run_legal_forty_tests();
void run_world_crouch_tests();
void run_hook5_rack_tests();
void run_ground_water_tests();
void run_counterweight_well_tests();

} // namespace scraperx::tests
