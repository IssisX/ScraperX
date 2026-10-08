#include "sim/gravity_wheel_geometry.hpp"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::BodyIndex;
using kit::Material;
using kit::Part;

constexpr float kSteelDensity = 7850.0F;
constexpr float kTimberDensity = 650.0F;
constexpr float kRefractoryDensity = 2450.0F;
constexpr float kRadius = 12.5F;
constexpr unsigned kScoopCount = 8;
constexpr unsigned kFragmentCount = 80;

float density(const Material material) {
    return material == Material::Timber ? kTimberDensity : kSteelDensity;
}

Part plate(const Vec3 half, const Vec3 offset = Vec3::sZero(),
           const Quat rotation = Quat::sIdentity(), const Material material = Material::Steel) {
    Part part;
    part.half = half;
    part.offset = offset;
    part.rotation = rotation;
    part.material = material;
    // Thin steel sheets have their actual square edges, rather than a margin
    // larger than the sheet. The same sheets form native and visible geometry.
    part.convex_radius = 0.0F;
    part.mass_kg = 8.0F * half.GetX() * half.GetY() * half.GetZ() * density(material);
    return part;
}

Part journal(const float radius, const float length, const Vec3 center) {
    Part part = plate(Vec3(radius, length * .5F, radius), center,
                      Quat::sRotation(Vec3::sAxisX(), JPH_PI * .5F));
    part.shape = Part::Shape::Cylinder;
    part.mass_kg = JPH_PI * radius * radius * length * kSteelDensity;
    return part;
}

float mass_of(const std::vector<Part> &parts) {
    float mass = 0.0F;
    for (const auto &part : parts) mass += part.mass_kg;
    return mass;
}

// Open rectangular section along local X: four separate physical sheets.
// No box collider fills the bore and no mass is assigned to its empty space.
void hollow_member(std::vector<Part> &parts, const float length, const float width,
                   const float depth, const float wall, const Vec3 center,
                   const Quat rotation = Quat::sIdentity(),
                   const Material material = Material::Steel) {
    for (const float sign : {-1.0F, 1.0F}) {
        parts.push_back(plate(Vec3(length * .5F, wall * .5F, depth * .5F),
            center + rotation * Vec3(0, sign * (width - wall) * .5F, 0), rotation, material));
        parts.push_back(plate(Vec3(length * .5F, (width - 2.0F * wall) * .5F, wall * .5F),
            center + rotation * Vec3(0, 0, sign * (depth - wall) * .5F), rotation, material));
    }
}

void open_bearing(std::vector<Part> &parts, const Vec3 center, const float bore,
                  const float outside, const float half_depth) {
    for (const float sign : {-1.0F, 1.0F}) {
        parts.push_back(plate(Vec3((outside - bore) * .5F, outside, half_depth),
            center + Vec3(sign * (outside + bore) * .5F, 0, 0)));
        parts.push_back(plate(Vec3(bore, (outside - bore) * .5F, half_depth),
            center + Vec3(0, sign * (outside + bore) * .5F, 0)));
    }
}

void hollow_between(std::vector<Part> &parts, const Vec3 first, const Vec3 second,
                    const float section, const float wall) {
    const Vec3 along = second - first;
    hollow_member(parts, along.Length(), section, section, wall, (first + second) * .5F,
                  Quat::sFromTo(Vec3::sAxisX(), along.Normalized()));
}

Vec3 local(const float x, const float y, const float z) {
    return Vec3(x + 44.0F, y - 321.5F, z + 164.0F);
}

void fixed_span(std::vector<Part> &parts, const Vec3 low, const Vec3 high,
                const Material material = Material::Steel) {
    parts.push_back(plate((high - low) * .5F, (high + low) * .5F,
                          Quat::sIdentity(), material));
}

void control_post(std::vector<Part> &parts, const Vec3 floor) {
    hollow_member(parts, 1.10F, .07F, .07F, .003F, floor + Vec3(0, .55F, 0),
                  Quat::sRotation(Vec3::sAxisZ(), JPH_PI * .5F), Material::Yellow);
    parts.push_back(plate(Vec3(.18F, .035F, .055F), floor + Vec3(0, 1.10F, 0),
                          Quat::sIdentity(), Material::Hazard));
}

std::vector<Part> receiver_parts() {
    std::vector<Part> parts;
    parts.push_back(plate(Vec3(7.25F, .10F, .80F), Vec3(0, -.10F, 0),
                          Quat::sIdentity(), Material::Galvanised));
    for (const float z : {-.60F, .60F}) {
        hollow_member(parts, 14.5F, .20F, .14F, .006F, Vec3(0, -.30F, z));
    }
    // The boarding side is open. A rail on the outer side leaves the full
    // receiving strip and the real 0.45m jump gap unobstructed.
    for (const float x : {-6.9F, 0.0F, 6.9F}) {
        hollow_member(parts, 1.1F, .06F, .06F, .003F, Vec3(x, .55F, .72F),
                      Quat::sRotation(Vec3::sAxisZ(), JPH_PI * .5F));
    }
    hollow_member(parts, 14.1F, .06F, .06F, .003F, Vec3(0, 1.1F, .72F));
    parts.push_back(plate(Vec3(1.80F, .0025F, .025F), Vec3(-5.31F, .0025F, -.775F),
                          Quat::sIdentity(), Material::Hazard));
    control_post(parts, Vec3::sZero());
    return parts;
}

Part fragment(const unsigned variant) {
    // Eight authored fracture families, in metres. Unequal corner cuts and
    // opposing face sizes produce bevelled, visibly uneven refractory chunks.
    constexpr std::array<std::array<float, 4>, 8> forms{{
        {{.130F, .100F, .110F, .20F}}, {{.142F, .094F, .108F, .28F}},
        {{.122F, .111F, .116F, .23F}}, {{.137F, .103F, .098F, .34F}},
        {{.126F, .089F, .123F, .18F}}, {{.139F, .108F, .106F, .30F}},
        {{.118F, .105F, .119F, .25F}}, {{.134F, .097F, .114F, .38F}},
    }};
    const auto &form = forms[variant];
    Part part;
    part.shape = Part::Shape::ConvexHull;
    part.material = Material::Refractory;
    part.convex_radius = 0.0F;
    part.half = Vec3(form[0], form[1], form[2]);
    for (unsigned corner = 0; corner < 8; ++corner) {
        const float sx = (corner & 1U) ? 1.0F : -1.0F;
        const float sy = (corner & 2U) ? 1.0F : -1.0F;
        const float sz = (corner & 4U) ? 1.0F : -1.0F;
        // This corner's three facets differ from the opposite corner. All
        // vertices stay within the declared packing envelope.
        const float cut = form[3] + .025F * static_cast<float>((corner + variant) % 3U);
        const Vec3 extent(form[0] * (sx > 0 ? .94F : 1.0F),
                          form[1] * (sy > 0 ? 1.0F : .91F),
                          form[2] * (sz > 0 ? .93F : 1.0F));
        part.points.emplace_back(sx * extent.GetX() * (1.0F - cut),
                                 sy * extent.GetY(), sz * extent.GetZ());
        part.points.emplace_back(sx * extent.GetX(),
                                 sy * extent.GetY() * (1.0F - cut * .83F), sz * extent.GetZ());
        part.points.emplace_back(sx * extent.GetX(), sy * extent.GetY(),
                                 sz * extent.GetZ() * (1.0F - cut * 1.07F));
    }
    ConvexHullShapeSettings settings(part.points.data(), static_cast<int>(part.points.size()), 0.0F);
    settings.mDensity = kRefractoryDensity;
    const auto cooked = settings.Create();
    if (cooked.HasError()) {
        throw std::invalid_argument("refractory fragment cook: " + std::string(cooked.GetError().c_str()));
    }
    // The native cooked solid supplies both volume and inertia. Kit cooks the
    // same points, with this density-derived mass, for the enabled body.
    part.mass_kg = cooked.Get()->GetMassProperties().mMass;
    if (!std::isfinite(part.mass_kg) || part.mass_kg <= 0.0F) {
        throw std::invalid_argument("refractory fragment has invalid native mass");
    }
    return part;
}
} // namespace

std::array<Part,2> split_refractory_fragment(const Part &whole) {
    if (whole.shape!=Part::Shape::ConvexHull || whole.points.size()<4)
        throw std::invalid_argument("refractory split requires a solid authored hull");
    std::array<Part,2> halves{whole,whole};
    for(auto &half:halves)half.points.clear();
    bool negative=false,positive=false;
    for(const auto p:whole.points){
        negative=negative||p.GetX()<0;positive=positive||p.GetX()>0;
        if(p.GetX()<=0)halves[0].points.push_back(p);
        if(p.GetX()>=0)halves[1].points.push_back(p);
    }
    if(!negative||!positive)throw std::invalid_argument("refractory cut must cross the solid");
    // All crossing vertex-pair intersections stay inside the original convex
    // solid and include every true edge intersection. Cooking their hull
    // therefore gives the exact half-space cut without a mesh-fracture owner.
    for(unsigned a=0;a<whole.points.size();++a)for(unsigned b=a+1;b<whole.points.size();++b){
        const auto p=whole.points[a],q=whole.points[b];
        if(!((p.GetX()<0&&q.GetX()>0)||(p.GetX()>0&&q.GetX()<0)))continue;
        auto intersection=p+(q-p)*(p.GetX()/(p.GetX()-q.GetX()));
        intersection.SetX(0);
        for(auto &half:halves)half.points.push_back(intersection);
    }
    for(auto &half:halves){
        ConvexHullShapeSettings settings(half.points.data(),static_cast<int>(half.points.size()),0);
        settings.mDensity=kRefractoryDensity;
        const auto cooked=settings.Create();
        if(cooked.HasError())throw std::invalid_argument("refractory half cook: "+std::string(cooked.GetError().c_str()));
        half.mass_kg=cooked.Get()->GetMassProperties().mMass;
        if(!std::isfinite(half.mass_kg)||half.mass_kg<=0)throw std::invalid_argument("refractory half must have positive finite mass");
    }
    if(whole.mass_kg>0&&std::abs(halves[0].mass_kg+halves[1].mass_kg-whole.mass_kg)>whole.mass_kg*.0001F)
        throw std::invalid_argument("refractory cut must conserve native material mass");
    return halves;
}

GravityWheelGeometry build_gravity_reclaim_wheel(PhysicsSystem &world, kit::Kit &kit) {
    vertical::BuildContext context{kit, world, {RVec3(-44.0, 321.5, -164.0), 0.0F},
                                   {1940, 6}, {2200, 104}};
    GravityWheelGeometry result;
    result.machine = std::make_unique<vertical::Machine>(context, "gravity_reclaim_wheel", 6, 103);
    auto &machine = *result.machine;
    const Quat upright = Quat::sRotation(Vec3::sAxisZ(), JPH_PI * .5F);
    const Quat along_z = Quat::sRotation(Vec3::sAxisY(), -JPH_PI * .5F);
    const float passenger_angle = -std::asin(11.0F / kRadius);
    const float wheel_travel = -2.0F * passenger_angle;
    const Vec3 passenger_pin(kRadius * std::cos(passenger_angle), -11.0F, 3.65F);

    std::vector<Part> foundation;
    // Bearing and reaction beam are behind the scoop rear faces. A front beam
    // would be struck by the passenger cantilever halfway through the ascent.
    open_bearing(foundation, Vec3(0, 0, -2.7F), .135F, .42F, .18F);
    hollow_member(foundation, 18.08F, .34F, .34F, .008F,
                  Vec3(9.46F, 0, -2.7F));
    hollow_member(foundation, 38.0F, .34F, .34F, .008F,
                  local(-25.75F, 321.6F, -166.7F), upright);
    // Tray and hopper load paths join this fixed Tower-side post. The feeder's
    // rear mast stays behind the complete moving bucket/door depth envelope.
    hollow_member(foundation, 33.25F, .30F, .30F, .007F,
                  local(-42.375F, 303.5F, -168.0F));
    hollow_member(foundation, 37.0F, .26F, .26F, .006F,
                  local(-52.0F, 321.9F, -168.0F), upright);
    hollow_member(foundation, 26.25F, .26F, .26F, .006F,
                  local(-38.875F, 340.4F, -168.0F));
    hollow_member(foundation, 1.3F, .24F, .24F, .006F,
                  local(-25.75F, 303.4F, -167.35F), along_z);
    for (const float y : {308.0F, 319.0F, 330.0F}) {
        hollow_member(foundation, 8.5F, .20F, .20F, .006F,
                      local(-25.75F, y - .32F, -162.6F), along_z);
    }
    // A real replacement gusset at the feed-control landing explains the
    // maintained industrial structure. Its thin triangular native plate joins
    // the underside and outer face of the receiving beam.
    Part repair;
    repair.shape = Part::Shape::ConvexHull;
    repair.material = Material::Galvanised;
    repair.convex_radius = 0.0F;
    repair.offset = local(-32.75F, 307.80F, -158.825F);
    for (const float z : {-.006F, .006F}) {
        repair.points.emplace_back(-.5F, 0, z);
        repair.points.emplace_back(.5F, 0, z);
        repair.points.emplace_back(-.5F, -.35F, z);
    }
    repair.mass_kg = .5F * 1.0F * .35F * .012F * kSteelDensity;
    foundation.push_back(std::move(repair));
    const BodyIndex frame = machine.body("frame", std::move(foundation), Vec3::sZero(), 0.0F);
    const BodyIndex lower = machine.body("lower", receiver_parts(),
        local(-32.75F, 308.0F, -158.15F), 0.0F);
    const BodyIndex upper = machine.body("upper", receiver_parts(),
        local(-32.75F, 330.0F, -158.15F), 0.0F);

    std::vector<Part> wheel_parts;
    for (unsigned segment = 0; segment < 24; ++segment) {
        const float angle = 2.0F * JPH_PI * (static_cast<float>(segment) + .5F) / 24.0F;
        const float half_sector = JPH_PI / 24.0F;
        const float chord_radius = kRadius * std::cos(half_sector);
        const Vec3 center(chord_radius * std::cos(angle), chord_radius * std::sin(angle), 0);
        hollow_member(wheel_parts, 2.0F * kRadius * std::sin(half_sector) + .018F,
                      .22F, .32F, .004F, center,
                      Quat::sRotation(Vec3::sAxisZ(), angle + JPH_PI * .5F), Material::Rust);
    }
    for (unsigned spoke = 0; spoke < 8; ++spoke) {
        const float angle = 2.0F * JPH_PI * static_cast<float>(spoke) / 8.0F;
        const Quat rotation = Quat::sRotation(Vec3::sAxisZ(), angle);
        hollow_member(wheel_parts, 12.08F, .14F, .22F, .004F,
                      rotation * Vec3(6.24F, 0, 0), rotation, Material::Steel);
        wheel_parts.push_back(plate(Vec3(.11F, .04F, .13F), rotation * Vec3(.15F, 0, 0), rotation));
    }
    wheel_parts.push_back(journal(.10F, 3.10F, Vec3(0, 0, -1.35F)));
    // A visible hollow cantilever carries the passenger pin ahead of the wheel
    // and fixed bearing structure. Its real sheet mass is part of wheel inertia.
    // The square cantilever ends behind the cabin hanger. Only the round
    // journal enters its rotating clearance: a square tube reaching the
    // hanger wedged the real cabin at a28-degree tilt during empty return.
    hollow_member(wheel_parts, 2.65F, .18F, .18F, .004F,
                  Vec3(passenger_pin.GetX(), passenger_pin.GetY(), 1.325F), along_z);
    wheel_parts.push_back(journal(.075F, 1.24F,
        Vec3(passenger_pin.GetX(), passenger_pin.GetY(), 3.27F)));

    std::array<Vec3, kScoopCount> scoop_centers;
    for (unsigned scoop = 0; scoop < kScoopCount; ++scoop) {
        const float beta = (130.0F + 45.0F * static_cast<float>(scoop)) * JPH_PI / 180.0F;
        const Vec3 center(kRadius * std::cos(beta), kRadius * std::sin(beta), -1.35F);
        scoop_centers[scoop] = center;
        // Real pin journals connect the rim to independent hanging buckets.
        // The short shaft is narrow at the opening; rear-side hanger members
        // below keep the usable feed mouth free of a transverse crossbar.
        wheel_parts.push_back(journal(.06F, 1.60F,
            Vec3(center.GetX(), center.GetY(), -.75F)));
    }
    const float wheel_mass = mass_of(wheel_parts);
    result.wheel = machine.body("wheel", std::move(wheel_parts), Vec3::sZero(), wheel_mass);
    result.bearing = machine.hinge(frame, result.wheel, Vec3::sZero(), 0.0F, wheel_travel);
    result.bearing->SetMotorState(EMotorState::Off);
    result.bearing->SetMaxFrictionTorque(250000.0F);

    std::vector<Part> cabin_parts;
    for (unsigned plank = 0; plank < 8; ++plank) {
        cabin_parts.push_back(plate(Vec3(.221F, .0275F, .95F),
            Vec3(-1.575F + .45F * static_cast<float>(plank), -2.5275F, 0),
            Quat::sIdentity(), Material::Timber));
    }
    for (const float z : {-.65F, .65F}) {
        hollow_member(cabin_parts, 3.6F, .09F, .10F, .003F, Vec3(0, -2.62F, z));
    }
    open_bearing(cabin_parts, Vec3::sZero(), .11F, .15F, .05F);
    hollow_member(cabin_parts, .8F, .08F, .08F, .003F, Vec3(0, -.155F, -.4F), along_z);
    hollow_member(cabin_parts, 3.20F, .10F, .10F, .004F, Vec3(0, -.20F, -.80F));
    for (const float x : {-1.55F, 1.55F}) {
        hollow_member(cabin_parts, 2.30F, .10F, .10F, .004F, Vec3(x, -1.40F, -.80F), upright);
    }
    for (const float x : {-1.71F, 1.71F}) {
        for (const float z : {-.82F, .82F}) {
            hollow_member(cabin_parts, 1.10F, .05F, .05F, .002F, Vec3(x, -1.95F, z), upright);
        }
        hollow_member(cabin_parts, 1.70F, .05F, .05F, .002F, Vec3(x, -1.40F, 0), along_z);
    }
    hollow_member(cabin_parts, 3.42F, .05F, .05F, .002F, Vec3(0, -1.40F, -.82F));
    control_post(cabin_parts, Vec3(0, -2.5F, .56F));
    const float cabin_mass = mass_of(cabin_parts);
    result.cabin = machine.body("cabin", std::move(cabin_parts), passenger_pin, cabin_mass);
    result.suspension = machine.hinge(result.wheel, result.cabin, passenger_pin);
    result.suspension->SetMaxFrictionTorque(100.0F);
    result.suspension->SetMotorState(EMotorState::Off);

    const float feed_x = -44.0F + scoop_centers[0].GetX();
    const float floor_angle = 35.0F * JPH_PI / 180.0F;
    const Quat floor_rotation = Quat::sRotation(Vec3::sAxisX(), floor_angle);
    const Vec3 mouth = local(feed_x, 336.9F, -165.08F);
    const Vec3 hopper_floor = mouth + floor_rotation * Vec3(0, 0, -1.166F);
    std::vector<Part> hopper_parts;
    hopper_parts.push_back(plate(Vec3(1.30F, .03F, 1.166F),
        hopper_floor + floor_rotation * Vec3(0, -.03F, 0), floor_rotation, Material::Rust));
    for (const float sign : {-1.0F, 1.0F}) {
        fixed_span(hopper_parts, local(feed_x + sign * 1.34F - .025F, 336.6F, -167.03F),
                   local(feed_x + sign * 1.34F + .025F, 339.6F, -165.07F), Material::Rust);
        // Real guide channels have clearance around the guillotine plate.
        fixed_span(hopper_parts, local(feed_x + sign * 1.34F - .02F, 336.88F, -165.11F),
                   local(feed_x + sign * 1.34F + .02F, 339.70F, -164.99F));
        fixed_span(hopper_parts, local(feed_x + sign * 1.34F - .025F, 335.25F, -165.42F),
                   local(feed_x + sign * 1.34F + .025F, 336.88F, -164.45F), Material::Rust);
    }
    fixed_span(hopper_parts, local(feed_x - 1.365F, 337.72F, -167.04F),
               local(feed_x + 1.365F, 339.6F, -166.99F), Material::Rust);
    // The real off-ramp opening is0.58m, wider than two chunk envelopes.
    // A0.31m entry allowed the confined irregular charge to bridge.
    // Its front lip stays below the floor mouth instead of supporting a pile.
    // A short contact-driven chute redirects the inclined feed vertically.
    // Its open exit is at335.25m; the hanging wall tops follow the12.5m pins.
    fixed_span(hopper_parts, local(feed_x - 1.315F, 335.25F, -164.50F),
               local(feed_x + 1.315F, 336.88F, -164.45F), Material::Rust);
    fixed_span(hopper_parts, local(feed_x - 1.315F, 335.25F, -165.42F),
               local(feed_x + 1.315F, 336.60F, -165.37F), Material::Rust);
    for (const float x : {feed_x - 1.35F, feed_x + 1.35F}) {
        hollow_member(hopper_parts, 1.02F, .16F, .16F, .005F,
                      local(x, 337.70F, -167.49F), along_z);
    }
    hollow_member(hopper_parts, 2.7F, .18F, .18F, .005F,
                  local(feed_x, 337.70F, -168.0F));
    const BodyIndex hopper = machine.body("hopper_chute", std::move(hopper_parts), Vec3::sZero(), 0.0F);
    // The35-degree feed slope is worn steel. Its steeper native geometry
    // lets the confined irregular charge feed instead of resting as a pile.
    // Material flow comes from actual traction and gravity, with no feed force.
    machine.native(hopper).SetFriction(.20F);

    std::vector<Part> tray_parts;
    fixed_span(tray_parts, local(-59.0F, 303.80F, -168.2F),
               local(-46.0F, 304.0F, -162.3F), Material::Galvanised);
    for (const float x : {-59.0F, -46.0F}) {
        fixed_span(tray_parts, local(x - .04F, 304.0F, -168.2F),
                   local(x + .04F, 304.60F, -162.3F), Material::Rust);
    }
    fixed_span(tray_parts, local(-59.0F, 304.0F, -168.24F),
               local(-46.0F, 304.60F, -168.16F), Material::Rust);
    fixed_span(tray_parts, local(-59.0F, 304.0F, -162.34F),
               local(-46.0F, 304.30F, -162.26F), Material::Rust);
    hollow_member(tray_parts, 13.0F, .22F, .18F, .006F,
                  local(-52.5F, 303.70F, -168.0F));
    (void)machine.body("spent_material_tray", std::move(tray_parts), Vec3::sZero(), 0.0F);

    std::vector<Part> recovery_parts;
    fixed_span(recovery_parts, local(-43.0F, 318.80F, -159.1F),
               local(-25.5F, 319.0F, -157.4F), Material::Galvanised);
    for (const float z : {-158.86F, -157.60F}) {
        hollow_member(recovery_parts, 17.5F, .20F, .14F, .006F,
                      local(-34.25F, 318.70F, z));
    }
    // The ladder sits outside the319m floor edge. A +X-facing climber has
    // open head/hand clearance and tops out onto real footing to its east.
    fixed_span(recovery_parts, local(-44.5F, 307.80F, -159.1F),
               local(-39.7F, 308.0F, -157.4F), Material::Galvanised);
    for (const float z : {-158.86F, -157.60F}) {
        hollow_member(recovery_parts, 4.8F, .20F, .14F, .006F,
                      local(-42.1F, 307.70F, z));
    }
    for (const float z : {-158.69F, -157.81F}) {
        hollow_member(recovery_parts, 11.05F, .07F, .07F, .003F,
                      local(-43.10F, 313.525F, z), upright, Material::Yellow);
    }
    for (unsigned rung = 0; rung < 37; ++rung) {
        const float y = 308.30F + .30F * static_cast<float>(rung);
        recovery_parts.push_back(plate(Vec3(.04F, .04F, .44F),
            local(-43.10F, y, -158.25F), Quat::sIdentity(), Material::Steel));
    }
    for (const float x : {-42.5F, -34.25F, -26.0F}) {
        hollow_member(recovery_parts, 1.1F, .06F, .06F, .003F,
                      local(x, 319.55F, -157.48F), upright);
    }
    hollow_member(recovery_parts, 17.0F, .06F, .06F, .003F,
                  local(-34.25F, 320.1F, -157.48F));
    // This panel is actually on the recovery catwalk, not an invisible remote
    // copy of the upper control's body-origin point.
    control_post(recovery_parts, local(-40.5F, 319.0F, -158.25F));
    const BodyIndex recovery = machine.body("recovery_catwalk", std::move(recovery_parts),
                                            Vec3::sZero(), 0.0F);

    std::vector<Part> gate_parts{plate(Vec3(1.30F, 1.0F, .00294F), Vec3::sZero(),
                                      Quat::sIdentity(), Material::Hazard)};
    const float gate_mass = mass_of(gate_parts);
    result.gate = machine.body("feed_gate", std::move(gate_parts),
                              local(feed_x, 337.88F, -165.05F), gate_mass);
    const auto gate_guide = machine.slider(hopper, result.gate, Vec3::sAxisY(), 0.0F, .75F);
    gate_guide->SetMaxFrictionForce(0.0F);
    gate_guide->GetMotorSettings().SetForceLimits(-20000.0F, 20000.0F);
    gate_guide->SetMotorState(EMotorState::Off);
    vertical::Drive gate_drive;
    gate_drive.slider = gate_guide;
    gate_drive.extent = .75F;
    gate_drive.speed = .25F;
    gate_drive.acceleration = 1.0F;
    machine.drives.push_back(gate_drive);

    for (unsigned scoop = 0; scoop < kScoopCount; ++scoop) {
        std::vector<Part> scoop_parts;
        for (const float sign : {-1.0F, 1.0F}) {
            scoop_parts.push_back(plate(Vec3(.002F, .60F, 1.0F),
                Vec3(sign * 1.402F, -.60F, 0), Quat::sIdentity(), Material::Rust));
            scoop_parts.push_back(plate(Vec3(1.4F, .60F, .002F),
                Vec3(0, -.60F, sign * 1.002F), Quat::sIdentity(), Material::Rust));
        }
        open_bearing(scoop_parts, Vec3::sZero(), .09F, .14F, .04F);
        for (const float sign : {-1.0F, 1.0F}) {
            hollow_between(scoop_parts, Vec3(sign * .11F, -.10F, -.04F),
                           Vec3(sign * 1.402F, -.14F, -.90F), .05F, .002F);
        }
        const float scoop_mass = mass_of(scoop_parts);
        const BodyIndex bucket = machine.body("scoop_" + std::to_string(scoop),
            std::move(scoop_parts), scoop_centers[scoop], scoop_mass);
        const auto hanger = machine.hinge(result.wheel, bucket, scoop_centers[scoop]);
        hanger->SetMaxFrictionTorque(100.0F);
        hanger->SetMotorState(EMotorState::Off);
        // Only this actual journal/bearing pair shares a collision exception.
        // Bucket fronts are rearward of the wheel rim, not intersecting it.
        machine.no_collision(result.wheel, bucket);

        const Vec3 pivot = scoop_centers[scoop] + Vec3(0, -1.2F, -1.0F);
        std::vector<Part> door_parts{plate(Vec3(1.382F, .003F, .982F), Vec3(0, -.003F, 1.0F),
                                          Quat::sIdentity(), Material::Steel)};
        const float door_mass = mass_of(door_parts);
        const BodyIndex door = machine.body("scoop_floor_" + std::to_string(scoop),
            std::move(door_parts), pivot, door_mass);
        const auto hinge = machine.axis_hinge(bucket, door, pivot,
            Vec3::sAxisX(), 0.0F, JPH_PI * .5F);
        hinge->SetMaxFrictionTorque(40000.0F);
        hinge->GetMotorSettings().SetTorqueLimits(-40000.0F, 40000.0F);
        hinge->SetMotorState(EMotorState::Off);
        // Only adjacent wall/floor self-contact is filtered. Every refractory
        // body keeps contacts with doors, wheel, chute, tray and other chunks.
        machine.no_collision(bucket, door);
        vertical::Drive drive;
        drive.hinge = hinge;
        drive.extent = JPH_PI * .5F;
        drive.speed = .5F;
        drive.acceleration = 1.0F;
        machine.drives.push_back(drive);
    }

    std::array<Part, 8> fragments;
    for (unsigned variant = 0; variant < fragments.size(); ++variant) fragments[variant] = fragment(variant);
    result.material.reserve(kFragmentCount+4);
    for (unsigned chunk = 0; chunk < kFragmentCount; ++chunk) {
        const unsigned column = chunk % 5U;
        const unsigned row = (chunk / 5U) % 4U;
        const unsigned layer = chunk / 20U;
        Part part = fragments[(chunk * 3U + layer) % fragments.size()];
        const float yaw = .08F * static_cast<float>(static_cast<int>(chunk % 3U) - 1);
        part.rotation = floor_rotation * Quat::sRotation(Vec3::sAxisY(), yaw);
        const Vec3 packed((static_cast<float>(column) - 2.0F) * .43F,
                          .135F + .26F * static_cast<float>(layer),
                          -.86F + .41F * static_cast<float>(row));
        const auto position=hopper_floor+floor_rotation*packed;
        const auto add_material=[&](const Part &piece,const std::string &suffix){
            const auto body=machine.body("refractory_"+std::to_string(chunk)+suffix,{piece},position,piece.mass_kg);
            machine.native(body).SetFriction(.55F);machine.native(body).SetRestitution(.08F);
            result.material.push_back(body);return body;
        };
        if(chunk==15||chunk==18||chunk==35||chunk==58){
            const auto halves=split_refractory_fragment(part);
            const auto first=add_material(halves[0],"_a"),second=add_material(halves[1],"_b");
            // Chosen brittle joint-load tuning, not certified material data.
            // Chosen brittle load rating: the bending ceiling uses a brick
            // scale lever arm, rather than failing under the resting stack.
            // These are gameplay tuning values, not calibrated fracture data.
            (void)kit.add_breakable_weld(first,second,6000.0F,750.0F);
        }else add_material(part,"");
    }

    machine.walkable("deck", result.cabin, Vec3(0, -2.5F, 0));
    machine.walkable("lower", lower, Vec3::sZero());
    machine.walkable("upper", upper, Vec3::sZero());
    machine.walkable("upper_recall", recovery, local(-40.5F, 319.0F, -158.25F));
    return result;
}

} // namespace scraperx::sim
