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
    // The drop direction the header claims but the loop above does not check:
    // from the 352 ring she can drop a long way (HangDrop has no rise cap),
    // so a region below she can reach must also have a usable way back.
    // Measured 2026-11-05 on trunk 1c227d70: 51 reachable below, exactly 1
    // with no way back (region 546, top 261.8, the cascade mast's bottom
    // beam: she reaches it by RungDown x4, PoleDown, HangDrop and cannot
    // leave by any usable edge). The beam stood 0.75 m from the entry
    // platform's lip, 0.05 m past her 0.7 m LedgeJump take-off limit; the
    // beam is 1.5 m deep since the fix in cascade_mast.cpp, so the take-off
    // sits 0.45 m from the lip and the check passes.
    std::size_t below_reached = 0;
    std::size_t below_pockets = 0;
    for (const auto &region : map.regions()) {
        if (region.id == start || region.top >= 352.0F) {
            continue;
        }
        if (map.plan(start, region.id).empty()) {
            continue;   // she cannot drop there from the 352 ring
        }
        ++below_reached;
        if (map.plan(region.id, start).empty()) {
            ++below_pockets;
            if (below_pockets <= 12) {
                std::cout << "  below-pocket: region " << region.id << " top " << region.top << " x[" << region.min_x
                          << "," << region.max_x << "] z[" << region.min_z << "," << region.max_z << "] area "
                          << region.area << "\n";
            }
        }
    }
    std::cout << "  census: regions=" << map.regions().size() << " below_reached=" << below_reached
              << " below_pockets=" << below_pockets << "\n";
    require(below_pockets == 0,
            "pockets: every region she can reach below the 352 ring has a usable way back to it");
    std::cout << "PASS scraperx_sim route pockets: reachable=" << reached << "\n";
}
SCRAPERX_GROUP("route_pockets", 280, run_route_pockets);
SCRAPERX_SELECT("route-pockets", run_route_pockets);
