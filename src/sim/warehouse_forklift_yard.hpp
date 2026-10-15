#pragma once

#include "sim/warehouse_forklift.hpp"

namespace scraperx::sim {

// Appended normal-world Kit content. These ids must never renumber retained
// routes. Cargo remains independent dynamic solids, never vehicle children.
struct WarehouseForkliftYard final {
    static constexpr std::uint64_t kChassisEntity = 2440;
    static constexpr std::uint64_t kMastEntity = 2441;
    static constexpr std::uint64_t kCarriageEntity = 2442;
    static constexpr std::uint64_t kForkEntity = 2443;
    static constexpr std::uint64_t kFirstCrateEntity = 2444;
    static constexpr std::uint64_t kSecondCrateEntity = 2445;
    static constexpr std::uint64_t kBeamEntity = 2446;
    static constexpr std::uint64_t kFrameEntity = 1440;

    WarehouseForklift::Config truck;
    std::array<kit::BodyIndex, 2> crates;
    kit::BodyIndex beam, receiving_frame;
    // Shape-origin local points on the actual chassis. Control authorization
    // additionally needs current platform support and reach, not this point alone.
    JPH::Vec3 operator_floor{0, -.24F, -1.65F};
    JPH::Vec3 control_station{0, .58F, -.88F};
    JPH::RVec3 stack_center = JPH::RVec3::sZero();
    JPH::RVec3 receiver_top = JPH::RVec3::sZero();
};

[[nodiscard]] WarehouseForkliftYard build_warehouse_forklift_yard(
    kit::Kit &kit, JPH::RVec3 origin = JPH::RVec3(-100, 0, -70));

} // namespace scraperx::sim
