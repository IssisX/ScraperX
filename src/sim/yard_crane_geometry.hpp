#pragma once

#include "sim/mechanism_kit.hpp"
#include <array>

namespace scraperx::sim {

// Geometry only. Kit owns solids; YardCrane owns all joints and actuators.
// All attachment Vec3 values are shape-local, never COM-local.
struct YardCraneGeometry final {
    kit::BodyIndex foundation, mast, boom, trolley, spreader;
    std::array<kit::BodyIndex, 2> docks;
    JPH::RVec3 pivot{-60, 36, -150};
    JPH::RVec3 ground_station{-66.5, 1, -150};
    JPH::RVec3 pickup_chassis{-80, .604, -150};
    JPH::Quat pickup_rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), JPH::JPH_PI * .5F);
    JPH::Vec3 hoist_support_eye{0, -.15F, 0};
    JPH::Vec3 hoist_load_eye = JPH::Vec3::sZero();
    std::array<JPH::Vec3, 4> spreader_eyes;
    std::array<float, 4> sling_lengths;
    float initial_trolley_m = 20;
    float min_trolley_m = 8;
    float max_trolley_m = 35;
    float boom_yaw_initial = -JPH::JPH_PI * .5F;
    float spreader_mass_kg = 0;
    JPH::Vec3 spreader_com_local = JPH::Vec3::sZero();
};

YardCraneGeometry build_yard_crane_geometry(kit::Kit &kit);

} // namespace scraperx::sim
