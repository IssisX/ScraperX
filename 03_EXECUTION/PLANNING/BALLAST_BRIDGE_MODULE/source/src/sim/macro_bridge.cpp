#include "sim/macro_bridge.hpp"

#include <algorithm>
#include <cmath>

namespace scraperx::sim::macro {
namespace {
using kit::Material;
using kit::Part;
using JPH::Vec3;
using JPH::RVec3;
using JPH::Quat;

void box(std::vector<Part> &parts, Vec3 half, Vec3 at, Material material,
         Quat rotation = Quat::sIdentity()) {
    parts.push_back({half, at, rotation, material});
}

// Polygonal sheave housing: the cable law's fixed bearing is inside a visible
// supported housing. The housing is static, not a pretend driven wheel.
void sheave(std::vector<Part> &parts, Vec3 centre) {
    for (int i = 0; i < 16; ++i) {
        const float a = static_cast<float>(i) * JPH::JPH_PI / 8.0F;
        box(parts, Vec3(0.14F, 0.14F, 0.22F),
            centre + Vec3(0.0F, 0.72F * std::cos(a), 0.72F * std::sin(a)),
            Material::Yellow, Quat::sRotation(Vec3::sAxisX(), -a));
    }
    box(parts, Vec3(0.5F, 0.12F, 0.12F), centre, Material::Galvanised);
}
} // namespace

void Bridge::build(kit::Kit &kit) {
    std::vector<Part> fixed;
    // Bearing piers leave the entire four-metre walking lane and hinge sweep
    // clear. No collision filtering hides an overlap with this structure.
    for (float x : {11.55F, 16.45F}) {
        box(fixed, Vec3(0.35F, 0.22F, 0.8F), Vec3(x, 0.22F, -92), Material::Concrete);
        box(fixed, Vec3(0.12F, 9.6F, 0.25F), Vec3(x, 9.6F, -118), Material::Rust);
        box(fixed, Vec3(0.30F, 0.16F, 1.2F), Vec3(x, 0.16F, -118), Material::Concrete);
    }
    box(fixed, Vec3(6.5F, 0.3F, 0.3F), Vec3(17.5F, 19, -118), Material::Rust);
    for (float x : {20.3F, 23.7F}) {
        box(fixed, Vec3(0.15F, 9.6F, 0.25F), Vec3(x, 9.6F, -118), Material::Rust);
        box(fixed, Vec3(0.6F, 0.15F, 1.5F), Vec3(x, 0.15F, -118), Material::Concrete);
        box(fixed, Vec3(0.09F, 7.5F, 0.12F), Vec3(x, 8.5F, -118), Material::Galvanised);
    }
    // Full-scale cross ties and back braces show the reaction path to grade.
    for (float y : {3.0F, 7.0F, 11.0F, 15.0F}) {
        box(fixed, Vec3(1.65F, 0.14F, 0.14F), Vec3(22, y, -119.5F), Material::Rust);
    }
    sheave(fixed, Vec3(14, 19, -118));
    sheave(fixed, Vec3(22, 19, -118));

    // The receiver joins the tower's real first-deck front strip. Its thin
    // approach lip remains above the entire rising nose sweep (>= 70 mm).
    box(fixed, Vec3(6.5F, 0.08F, 1.45F), Vec3(18, 10.92F, -123.65F), Material::Galvanised);
    for (float x : {10.5F, 24.7F}) {
        box(fixed, Vec3(0.22F, 5.45F, 0.3F), Vec3(x, 5.45F, -123.3F), Material::Rust);
        box(fixed, Vec3(0.8F, 0.2F, 0.8F), Vec3(x, 0.2F, -123.3F), Material::Concrete);
    }
    // Nose lugs meet these caps from below. Remaining source weight tensions
    // the cable against the caps; no pose snap, success latch or invisible seat.
    const Quat final_rotation = Quat::sRotation(Vec3::sAxisX(), kAngle);
    for (float side : {-1.0F, 1.0F}) {
        const Vec3 lug = Vec3(14, 0.16F, -92) +
                         final_rotation * Vec3(side * 2.20F, 0.0F, -31.6F);
        box(fixed, Vec3(0.20F, 0.12F, 0.32F),
            lug + Vec3(0, 0.33F, 0), Material::Hazard);
        box(fixed, Vec3(0.12F, 0.55F, 0.12F),
            Vec3(lug.GetX(), 10.60F, -123.2F), Material::Rust);
    }
    // Outer receiver rails leave its approach and tower continuation open.
    box(fixed, Vec3(4.0F, 0.05F, 0.05F), Vec3(20.5F, 12.05F, -122.25F), Material::Yellow);
    for (float x : {16.6F, 20.5F, 24.4F})
        box(fixed, Vec3(0.05F, 0.55F, 0.05F), Vec3(x, 11.55F, -122.25F), Material::Yellow);

    // Brake housing and the supported return sheave at the ballast head.
    // The long visible control cable ends here, on the actual moving arm.
    box(fixed, Vec3(0.3F, 0.27F, 0.30F), Vec3(22, 19.70F, -118), Material::Rust);
    box(fixed, Vec3(0.12F, 0.12F, 2.0F), Vec3(22, 19.86F, -120), Material::Rust);
    box(fixed, Vec3(0.22F, 0.15F, 0.22F), Vec3(22, 20.15F, -122), Material::Yellow);
    box(fixed, Vec3(0.35F, 0.45F, 0.35F), Vec3(7, 0.45F, -84), Material::Concrete);
    for (const Vec3 top : {Vec3(7, 2.5F, -86)}) {
        box(fixed, Vec3(0.07F, top.GetY() * 0.5F - 0.12F, 0.07F),
            Vec3(top.GetX(), top.GetY() * 0.5F - 0.12F, top.GetZ()), Material::Rust);
        box(fixed, Vec3(0.22F, 0.12F, 0.22F), top, Material::Yellow);
    }
    box(fixed, Vec3(0.06F, 0.95F, 0.06F), Vec3(4.5F, 0.95F, -84), Material::Rust);
    box(fixed, Vec3(1.05F, 0.45F, 0.06F), Vec3(4.5F, 2, -84), Material::Steel);
    frame = kit.add_body(kFrame, fixed, RVec3::sZero(), Quat::sIdentity(), 0, 0.8F);

    std::vector<Part> bridge;
    box(bridge, Vec3(2, 0.06F, 16), Vec3(0, 0, -16), Material::Timber);
    for (float side : {-1.0F, 1.0F}) {
        box(bridge, Vec3(0.10F, 0.32F, 15), Vec3(side * 1.9F, 0.28F, -15), Material::Rust);
        box(bridge, Vec3(0.055F, 0.055F, 14.6F), Vec3(side * 1.9F, 1.20F, -15), Material::Yellow);
        for (int i = 1; i <= 14; ++i) {
            const float z = -2.0F * static_cast<float>(i);
            box(bridge, Vec3(0.06F, 0.56F, 0.06F), Vec3(side * 1.9F, 0.64F, z), Material::Rust);
        }
        box(bridge, Vec3(0.35F, 0.12F, 0.26F), Vec3(side * 2.05F, 0.0F, -31.6F), Material::Hazard);
        box(bridge, Vec3(0.12F, 1.4F, 0.12F), Vec3(side * 1.8F, 1.4F, -28), Material::Yellow);
    }
    box(bridge, Vec3(1.95F, 0.12F, 0.12F), Vec3(0, 2.8F, -28), Material::Yellow);
    deck = kit.add_body(kDeck, bridge, RVec3(14, 0.16, -92), Quat::sIdentity(), 12000, 0.9F);
    initial_deck_com_y = kit.body_center_of_mass_position(deck).GetY();
    kit.set_damping(deck, 0.01F, 0.01F);
    // Limit is an emergency bearing limit beyond the actual visible cap seat.
    deck_hinge = kit.add_lever(deck, RVec3(14, 0.16, -92), Vec3::sAxisX(),
                               Vec3::sAxisY(), 0, kAngle + 0.035F);

    std::vector<Part> weight;
    box(weight, Vec3(1.0F, 1.05F, 1.0F), Vec3::sZero(), Material::Concrete);
    for (float x : {-1.08F, 1.08F}) {
        box(weight, Vec3(0.10F, 1.16F, 1.08F), Vec3(x, 0, 0), Material::Yellow);
        // Guide shoes travel beside the continuous rails.
        box(weight, Vec3(0.22F, 0.55F, 0.30F), Vec3(x * 1.2F, 0, 0), Material::Rust);
    }
    box(weight, Vec3(1.20F, 0.12F, 0.16F), Vec3(0, 1.20F, 0), Material::Steel);
    ballast = kit.add_body(kBallast, weight, RVec3(22, 13.8, -118), Quat::sIdentity(), 10000, 0.0F);
    kit.set_damping(ballast, 0, 0);
    ballast_guide = kit.add_guide(ballast, Vec3::sAxisY(), -kDrop - 0.15F, 0, 1.3F, 150000, 0.6F);
    // 1:1 tension-only cable; its geometry, not an angle table, couples the
    // changing bridge-eye radius to vertical ballast travel. The 1 MN breaking
    // limit covers the measured ~345 kN cap-contact impulse, not just the
    // 98.1 kN static hanging load (which undersizes a cable for this stop).
    rope = kit.add_rope(deck, Vec3(0, 2.8F, -28), RVec3(14, 19, -118),
                        ballast, Vec3(0, 1.32F, 0), RVec3(22, 19, -118), 1, -1, 1000000);

    std::vector<Part> control;
    box(control, Vec3(1.5F, 0.075F, 0.075F), Vec3(1.5F, 0, 0), Material::Yellow);
    box(control, Vec3(0.08F, 0.08F, 0.40F), Vec3(3, 0, 0), Material::Galvanised);
    brake_arm = kit.add_body(kBrakeArm, control, RVec3(22, 20.15, -118), Quat::sIdentity(), 35, 0.7F);
    kit.set_damping(brake_arm, 0.1F, 1.0F);
    handle_hinge = kit.add_lever(brake_arm, RVec3(22, 20.15, -118), Vec3::sAxisY(),
                                 Vec3::sAxisX(), 0, JPH::JPH_PI / 2.0F);
    kit.set_lever_friction(handle_hinge, 45); // 15 N tangential at the three-metre arm tip
    // A free T-grip on a real trip cable lets a thumb user pull the long arm
    // without matching a mathematically exact circular hand trajectory.
    std::vector<Part> grip;
    box(grip, Vec3(0.46F, 0.055F, 0.055F), Vec3::sZero(), Material::Yellow);
    box(grip, Vec3(0.055F, 0.055F, 0.20F), Vec3(0, 0, -0.15F), Material::Steel);
    handle = kit.add_body(kHandle, grip, RVec3(7, 1.10, -84), Quat::sIdentity(), 3.0F, 0.7F);
    kit.set_damping(handle, 2.0F, 2.0F);
    const Vec3 com = Vec3(kit.body_center_of_mass_position(handle) - kit.body_position(handle));
    kit.set_carry(handle, kit::CarryKind::Handle, -com);
    kit.add_trip_line(brake_arm, Vec3(3, 0, 0), handle, Vec3(0, 0, -0.3F),
                       RVec3(22, 20.15, -122), RVec3(7, 2.5, -86));
    pre_step(kit);
}

void Bridge::pre_step(kit::Kit &kit) const {
    const float angle = kit.lever_angle(handle_hinge);
    const float released = std::clamp((angle - 0.15F) / 0.85F, 0.0F, 1.0F);
    // Brake resistance reflected to the source's guide coordinate. The actual
    // head arm is pulled by the visible trip cable; no remote angle signal or
    // event starts the bridge. This is a finite passive constitutive brake,
    // not a detailed pad/cam or rotating-sheave inertia simulation.
    kit.set_guide_brake(ballast_guide, 150000.0F * (1.0F - released));
}

} // namespace scraperx::sim::macro
