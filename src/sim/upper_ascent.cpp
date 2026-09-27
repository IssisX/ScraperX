#include "sim/upper_ascent.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part box(const JPH::Vec3 half, const JPH::Vec3 centre, const Material material) {
    return {half, centre, JPH::Quat::sIdentity(), material};
}
Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (high + low), material);
}

constexpr float kBedTop = 45.30F;
constexpr float kBedStroke = 1.60F;
const JPH::RVec3 kPlatformAt(4.0, 43.80, -118.0);
const JPH::RVec3 kWeightAt(10.0, 55.70, -118.0);
const JPH::RVec3 kPlatformSheave(5.5, 67.80, -118.0);
const JPH::RVec3 kWeightSheave(10.0, 67.80, -118.0);
const JPH::Vec3 kPlatformEye(1.5F, 0.55F, 0.0F);
const JPH::Vec3 kWeightEye(0.0F, 1.15F, 0.0F);

std::vector<Part> fixed_frame() {
    std::vector<Part> parts;
    // Lower and upper exits touch the platform only along its tower-side
    // edge. The upper plate tilts down half a metre toward the lift, allowing
    // the bed's finite resting range without a precision jump.
    parts.push_back(span({2.0F, 43.82F, -124.0F}, {6.0F, 44.0F, -120.0F},
                         Material::Galvanised));
    parts.push_back({{2.0F, 0.10F, 2.02F}, {4.0F, 54.65F, -122.0F},
                     JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.12435F),
                     Material::Galvanised});
    parts.push_back(span({2.20F, 54.85F, -122.50F},
                         {3.00F, 55.05F, -121.50F}, Material::Yellow));
    // Open gantry and two exposed vertical guide channels. Posts sit beyond
    // the 4 x 4 m platform; the platform's tower-side face remains open.
    for (const float x : {1.65F, 6.35F}) {
        for (const float z : {-115.65F, -120.35F}) {
            parts.push_back(span({x - 0.14F, 43.30F, z - 0.14F},
                                 {x + 0.14F, 68.1F, z + 0.14F}, Material::Rust));
        }
    }
    for (const float x : {9.05F, 10.95F}) {
        parts.push_back(span({x - 0.13F, 43.15F, -118.13F},
                             {x + 0.13F, 68.1F, -117.87F}, Material::Rust));
    }
    parts.push_back(span({1.5F, 67.7F, -118.2F}, {11.1F, 68.1F, -117.8F}, Material::Yellow));
    // Two broad exposed receiver bars support the final catch's load path.
    for (const float x : {1.75F, 6.25F}) {
        parts.push_back(span({x - 0.20F, 54.75F, -118.45F},
                             {x + 0.20F, 55.15F, -117.55F}, Material::Hazard));
    }
    for (const float x : {5.5F, 10.0F}) {
        // Cylinders are drawn and collided as sheaves; their axle is Z.
        parts.push_back({{0.58F, 0.13F, 0.58F}, {x, 67.8F, -118.0F},
                         JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 1.57079633F),
                         Material::Steel, Part::Shape::Cylinder});
    }
    // A broad bed and a visible trip-line guide at the boarding height.
    parts.push_back(span({9.20F, 43.05F, -118.80F}, {10.80F, 43.35F, -117.20F},
                         Material::Rust));
    parts.push_back(span({2.38F, 45.55F, -114.12F}, {2.62F, 45.75F, -113.88F},
                         Material::Steel));
    return parts;
}

std::vector<Part> platform_parts() {
    std::vector<Part> parts{
        box({2.0F, 0.20F, 2.0F}, JPH::Vec3::sZero(), Material::Galvanised),
        box({0.12F, 0.22F, 0.12F}, kPlatformEye, Material::Hazard),
    };
    for (const float x : {-1.90F, 1.90F}) {
        parts.push_back(box({0.10F, 0.32F, 1.80F}, {x, -0.12F, 0}, Material::Yellow));
    }
    // No barrier across the tower-side boarding/exit opening.
    parts.push_back(box({1.80F, 0.45F, 0.08F}, {0, 0.25F, 1.91F}, Material::Yellow));
    return parts;
}

std::vector<Part> upper_parkour_parts() {
    // Compact exterior service equipment. The actual native climb/hang
    // probes see the same geometry Godot draws from these Kit parts.
    std::vector<Part> parts;
    constexpr float face = -123.70F;
    parts.push_back(span({0.80F, 55.0F, -122.50F}, {2.00F, 56.70F, -121.80F}, Material::Steel));
    parts.push_back(span({0.78F, 56.62F, -122.52F}, {2.02F, 56.72F, -121.78F}, Material::Hazard));
    parts.push_back(span({-1.20F, 58.80F, face}, {5.80F, 60.30F, -122.90F}, Material::Galvanised));
    for (const float x : {-0.80F, 5.20F}) {
        parts.push_back(span({x - 0.10F, 60.30F, face},
                             {x + 0.10F, 64.05F, face + 0.03F}, Material::Rust));
    }
    constexpr float vent_x = 5.40F;
    constexpr float vent_z = -123.45F;
    parts.push_back(span({vent_x - 0.07F, 60.30F, vent_z - 0.07F},
                         {vent_x + 0.07F, 65.77F, vent_z + 0.07F}, Material::Galvanised));
    parts.push_back(span({vent_x - 0.57F, 65.63F, vent_z - 0.07F},
                         {vent_x + 0.57F, 65.77F, vent_z + 0.07F}, Material::Galvanised));
    for (const float side : {-1.0F, 1.0F}) {
        const float x = vent_x + side * 0.5F;
        parts.push_back(span({x - 0.07F, 65.63F, vent_z - 0.07F},
                             {x + 0.07F, 67.0F, vent_z + 0.07F}, Material::Galvanised));
    }
    parts.push_back(span({vent_x - 0.03F, 65.10F, face + 0.04F},
                         {vent_x + 0.03F, 65.20F, vent_z - 0.07F}, Material::Steel));
    parts.push_back(span({vent_x - 0.60F, 66.0F, face - 0.95F},
                         {vent_x + 0.60F, 66.03F, face + 0.04F}, Material::Steel));
    parts.push_back(span({vent_x - 0.60F, 64.75F, face},
                         {vent_x + 0.60F, 66.0F, face + 0.04F}, Material::Steel));
    return parts;
}
} // namespace

UpperAscent::UpperAscent(JPH::PhysicsSystem &system, kit::Kit &kit)
    : system_(system), kit_(kit) {
    using namespace JPH;
    const auto frame = kit_.add_body(1700, fixed_frame(), RVec3::sZero(), Quat::sIdentity(), 0, 0.8F);
    (void)kit_.add_body(1701, upper_parkour_parts(), RVec3::sZero(), Quat::sIdentity(), 0, 0.8F);
    const auto platform = kit_.add_body(2700, platform_parts(), kPlatformAt,
                                        Quat::sIdentity(), 2800.0F, 0.9F);
    weight_ = kit_.add_body(2701,
        {box({0.70F, 1.0F, 0.70F}, Vec3::sZero(), Material::Rust),
         box({0.10F, 0.18F, 0.10F}, kWeightEye, Material::Hazard)},
        kWeightAt, Quat::sIdentity(), 3200.0F, 0.8F);
    kit_.disable_collision(platform, frame);
    kit_.disable_collision(weight_, frame);
    kit_.set_continuous_collision(platform);
    kit_.set_continuous_collision(weight_);
    (void)kit_.add_guide(platform, Vec3::sAxisY(), 0.0F, 11.0F, 0, 0, 0);
    (void)kit_.add_guide(weight_, Vec3::sAxisY(), -11.0F, 0.0F, 0, 0, 0);
    const float rope_length = Vec3(kPlatformAt + RVec3(kPlatformEye) - kPlatformSheave).Length() +
                              Vec3(kWeightAt + RVec3(kWeightEye) - kWeightSheave).Length();
    (void)kit_.add_rope(platform, kPlatformEye, kPlatformSheave,
                        weight_, kWeightEye, kWeightSheave, 1.0F, rope_length, 0.0F);
    const RVec3 upper_seat = kit_.body_center_of_mass_position(platform) + RVec3(0, 10.96, 0);
    (void)kit_.add_catch_at(platform, {}, upper_seat, 0.0F, 0.20F, true, false, 0.04F);

    const RVec3 lever_pivot(4.0, 46.5, -114.0);
    const auto lever_body = kit_.add_body(2702,
        {box({0.75F, 0.05F, 0.05F}, {-0.75F, 0, 0}, Material::Hazard),
         box({0.17F, 0.30F, 0.17F}, {0.35F, 0, 0}, Material::Rust)},
        lever_pivot, Quat::sIdentity(), 40.0F, 0.5F);
    lever_ = kit_.add_lever(lever_body, lever_pivot,
                            Vec3::sAxisZ(), Vec3::sAxisX(), 0, 0.45F);
    catch_ = kit_.add_catch(weight_, lever_, 0.18F, 0.05F, false);
    // Keep the release chain outside the carriage sweep after the rider lets
    // go. Otherwise the rising floor carries the handle into its own taut
    // trip line, imposing a second, unintended brake halfway up the shaft.
    const RVec3 handle_sheave(4.0, 47.0, -115.8);
    const auto handle = kit_.add_body(2703,
        {box({0.22F, 0.04F, 0.04F}, Vec3::sZero(), Material::Yellow)},
        RVec3(4.0, 45.28, -115.8), Quat::sIdentity(), 3.0F, 0.9F);
    kit_.set_carry(handle, kit::CarryKind::Handle, {0, 0.04F, 0});
    kit_.set_damping(handle, 1.5F, 1.5F);
    (void)kit_.add_trip_line(lever_body, {-1.50F, 0, 0}, handle, {0, 0.04F, 0},
                             RVec3(2.5, 45.65, -114.0), handle_sheave);

    bed_ = kit_.add_body(2704,
        {box({0.80F, kBedStroke * 0.5F, 0.80F}, Vec3::sZero(), Material::Timber)},
        RVec3(10.0, kBedTop - kBedStroke * 0.5, -118.0), Quat::sIdentity(), 1000.0F, 0.8F);
    system_.GetBodyInterface().SetMotionType(kit_.body_id(bed_), EMotionType::Kinematic,
                                               EActivation::Activate);
    kit_.disable_collision(bed_, weight_);
}

void UpperAscent::pre_step(const float dt) {
    using namespace JPH;
    auto &bodies = system_.GetBodyInterface();
    const BodyID id = kit_.body_id(weight_);
    const RVec3 bottom = bodies.GetWorldTransform(id) * Vec3(0, -1.0F, 0);
    const double penetration = std::max(0.0, double(kBedTop) - bottom.GetY());
    if (penetration > 0.0) {
        constexpr double yield = 15000.0;
        constexpr double stiffness = 200000.0;
        constexpr double damping = 1500.0;
        const double front = std::min(double(kBedStroke),
                                      std::max(state_.bed_front_m, penetration - yield / stiffness));
        state_.plastic_work_j += yield * (front - state_.bed_front_m);
        state_.bed_front_m = front;
        const double elastic = stiffness * std::max(0.0, penetration - front);
        const double closing = -bodies.GetPointVelocity(id, bottom).GetY();
        const double force = std::max(0.0, elastic + damping * closing);
        state_.damping_work_j += std::max(0.0, (force - elastic) * closing) * dt;
        if (force > 0.0) bodies.AddForce(id, Vec3(0, float(force), 0), bottom);
    }
    bodies.MoveKinematic(kit_.body_id(bed_),
        RVec3(10.0, kBedTop - kBedStroke * 0.5 - state_.bed_front_m, -118.0),
        Quat::sIdentity(), dt);
}

} // namespace scraperx::sim
