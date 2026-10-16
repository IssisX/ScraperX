#pragma once

#include "sim/powered_cable.hpp"
#include "sim/warehouse_forklift.hpp"
#include "sim/yard_crane_geometry.hpp"

namespace scraperx::sim {

// Kit owns every solid. This owner holds the real bearing/rail, finite drives
// and removable four-leg rig. The host authorizes controls from actual footing.
class YardCrane final {
public:
    struct Command { float slew = 0, trolley = 0, hoist = 0; };
    enum class Failure : std::uint8_t { None, Obstructed, CableInventory, SlingFailure };
    struct State {
        PoweredCable::State hoist;
        std::array<PoweredCable::State, 4> slings{};
        std::array<JPH::RVec3, 4> loose_ends{};
        double transport_energy_j = 2000000, transport_positive_work_j = 0;
        double transport_heat_j = 0, reservation_excess_j = 0;
        float slew_angle_rad = 0, trolley_position_m = 20;
        float slew_torque_nm = 0, trolley_force_n = 0;
        float slew_target_rad_s = 0, trolley_target_mps = 0;
        std::uint8_t attached_mask = 0;
        Failure failure = Failure::None;
        bool transport_cutoff = false;
        bool hoist_connected = true;
    };
    YardCrane(JPH::PhysicsSystem &, kit::Kit &, const WarehouseForklift::Config &);
    ~YardCrane();
    YardCrane(const YardCrane &) = delete;
    YardCrane &operator=(const YardCrane &) = delete;
    void pre_step(Command);
    void post_step();
    // Actual aligned steel, a stationary truck and real wheel support are
    // required. Pickup additionally requires the real coupled box forks at
    // least80mm above the upward three-wheel contact plane, no actual external
    // fork contact, and mast within .02rad of its existing forward tilt stop.
    // These transactions never assign a body pose or velocity.
    [[nodiscard]] bool try_attach(const WarehouseForklift &);
    [[nodiscard]] bool try_detach(const WarehouseForklift &);
    // Host separately validates the grounded operator's hand reach to this
    // actual eye. Owner checks the same physical stow/leg gates per transaction.
    // Remaining legs may be assembled without replacing already attached legs.
    [[nodiscard]] bool try_attach_leg(unsigned, const WarehouseForklift &);
    [[nodiscard]] bool try_detach_leg(unsigned, const WarehouseForklift &);
    [[nodiscard]] State state() const;
    [[nodiscard]] const YardCraneGeometry &geometry() const;
    // Restore Kit bodies first, then this inventory and joint lifecycle.
    void restore(const State &);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace scraperx::sim
