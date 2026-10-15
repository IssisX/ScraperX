#include "simulation/test_suites.hpp"

#include <cstdlib>

int main() {
    using namespace scraperx::tests;

    const auto support = run_foundation_support_tests();
    const auto traversal = run_athletic_traversal_tests();
    run_plant_recovery_tests(support, traversal);
    run_freight_jib_tests();
    run_structural_needle_tests();
    run_process_sump_tests();
    run_intake_rise_tests();
    run_legal_forty_tests();
    run_world_crouch_tests();
    run_hook5_rack_tests();
    run_ground_water_tests();
    run_counterweight_well_tests();

    return EXIT_SUCCESS;
}
