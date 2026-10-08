#pragma once

#include "sim/supplied/vertical_machine.hpp"
#include <array>

namespace scraperx::sim {

// Kit owns every body; Machine owns the bearing, suspension and feed/discharge
// constraints. The host commands the finite gate/door drives and passive brake.
struct GravityWheelGeometry final {
    std::unique_ptr<vertical::Machine> machine;
    JPH::Ref<JPH::HingeConstraint> bearing;
    JPH::Ref<JPH::HingeConstraint> suspension;
    kit::BodyIndex wheel;
    kit::BodyIndex cabin;
    kit::BodyIndex gate;
    std::vector<kit::BodyIndex> material;
};

GravityWheelGeometry build_gravity_reclaim_wheel(JPH::PhysicsSystem &world, kit::Kit &kit);
// Complementary native hull halves retain the whole authored frame and density.
std::array<kit::Part,2> split_refractory_fragment(const kit::Part &whole);

} // namespace scraperx::sim
