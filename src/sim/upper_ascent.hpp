#pragma once

#include "sim/mechanism_kit.hpp"

namespace scraperx::sim {

// AS-019: gravity-driven, guided +44 to +55 m exterior lift. Kit/Jolt owns
// the bodies, cable, catch and player contact; the timber bed is the one
// additional constitutive state.
class UpperAscent final {
public:
    struct State {
        double bed_front_m = 0.0;
        double plastic_work_j = 0.0;
        double damping_work_j = 0.0;
    };

    UpperAscent(JPH::PhysicsSystem &system, kit::Kit &kit);
    void pre_step(float dt);
    [[nodiscard]] const State &state() const noexcept { return state_; }
    void restore(const State &state) noexcept { state_ = state; }

private:
    JPH::PhysicsSystem &system_;
    kit::Kit &kit_;
    kit::BodyIndex weight_;
    kit::BodyIndex bed_;
    kit::LeverIndex lever_;
    kit::CatchIndex catch_;
    State state_{};
};

} // namespace scraperx::sim
