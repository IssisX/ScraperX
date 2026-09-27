#pragma once
#include "sim/mechanism_kit.hpp"
#include <array>
namespace scraperx::sim {
// The normal +33 to +44 m machine. Kit/Jolt owns motion; the only extra
// evolving state is the irreversible receiver material.
class SwingStair final {
public:
    struct State { double front_m=0, plastic_work_j=0, damping_work_j=0; };
    SwingStair(JPH::PhysicsSystem &system, kit::Kit &kit);
    void pre_step(float dt);
    const State &state() const noexcept { return state_; }
    void restore(const State &state) noexcept { state_=state; }
    double floor_height() const;
    static constexpr double kBedTop=44.9, kStroke=1.75;
private:
    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    kit::BodyIndex stair_;
    std::array<kit::BodyIndex,2> pads_;
    State state_{};
};
}
