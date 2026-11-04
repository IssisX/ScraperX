// J-017: her scan of the no-lift routes (AffordanceMap, the matron's graph) has no
// pockets. The map records one-way drops (HangDrop has no rise cap, her up edges
// stop at 2.30 m Mantle / 3.64 m LedgeJump), so a stage built on the player's
// moves can leave a region she can drop into and not leave. Every region she can
// reach from the 352 ring has a usable way back to it, checked as a chain
// (plan() over usable edges), not one hop.

#include "sim_tests.hpp"

#include "sim/matron_traversal.hpp"

void run_route_pockets() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation s(InitialSpawn::PlateTop);
    require(s.advance_frame(0.5).accepted, "pockets: settle");
    const scraperx::sim::traversal::AffordanceMap &map = s.matron_affordances();
    const int start = map.region_at(JPH::Vec3(15.5F, 352.25F, -150.0F));
    require(start >= 0, "pockets: the 352 ring's east band is on her scan");
    std::size_t reached = 0;
    std::size_t pockets = 0;
    for (const auto &region : map.regions()) {
        if (region.id == start || region.top < 352.0F || region.top > 641.0F) {
            continue;
        }
        if (map.plan(start, region.id).empty()) {
            continue;   // she cannot get there from the 352 ring
        }
        ++reached;
        if (map.plan(region.id, start).empty()) {
            ++pockets;
            if (pockets <= 12) {
                std::cout << "  pocket: region " << region.id << " top " << region.top << " x[" << region.min_x << ","
                          << region.max_x << "] z[" << region.min_z << "," << region.max_z << "] area " << region.area
                          << "\n";
            }
        }
    }
    std::cout << "  pockets: " << reached << " regions reachable from the 352 ring, " << pockets
              << " with no way back\n";
    require(pockets == 0, "pockets: every region she can reach above the 352 ring has a usable way back to it");
    std::cout << "PASS scraperx_sim route pockets: reachable=" << reached << "\n";
}
