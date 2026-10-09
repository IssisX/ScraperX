#pragma once

#include "sim/supplied/vertical_machine.hpp"
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <array>

namespace scraperx::sim {

// Kit owns bodies and the tension-only tow. Machine owns native constraints.
// The host owns finite reset/brake work and retainer release; this factory
// supplies no ascent motor, proximity catch, or second physics state owner.
struct GravityCartGeometry final {
    std::unique_ptr<vertical::Machine> machine;
    JPH::Ref<JPH::SliderConstraint> slab_axis;
    JPH::Ref<JPH::SixDOFConstraint> lateral_guide;
    JPH::Ref<JPH::HingeConstraint> upper_latch_hinge;
    kit::BodyIndex deck, slab, upper_latch, optional_load;
    std::array<kit::BodyIndex, 4> rollers;
    kit::RopeIndex tow;
    JPH::Vec3 tow_local;
    JPH::RVec3 lower_deck, upper_deck, latch_pivot;
    double stroke_m = 25.403412;
    double capacity_j = 200000;
    double power_w = 10000;
    double reset_force_n = 9000;
    double brake_force_n = 9000;
    double reset_speed_mps = .6;
    // Chosen finite release authority, initially Off. Host reads native
    // upper_latch_hinge->GetCurrentAngle(); no stored latch/open flag.
    double retainer_release_torque_nm = 120;
    double retainer_open_angle_rad = 1.4;
};

GravityCartGeometry build_gravity_cart(JPH::PhysicsSystem &world, kit::Kit &kit);

} // namespace scraperx::sim
