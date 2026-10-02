#include "sim/suspended_ladder.hpp"
#include <vector>
namespace scraperx::sim {
namespace {
using kit::Part;
using kit::Material;
Part span(JPH::Vec3 low, JPH::Vec3 high, Material material) {
    return {(high-low)*0.5F,(high+low)*0.5F,JPH::Quat::sIdentity(),material};
}
}
void build_suspended_ladder(kit::Kit &kit) {
    using namespace JPH;
    std::vector<Part> frame;
    const auto add=[&](Vec3 a,Vec3 b,Material m) {frame.push_back(span(a,b,m));};
    // Catch deck bears into the +110m north ring, with a narrow overlap tongue.
    add({3,109.72F,-184.5F},{12.5F,110,-176.05F},Material::Galvanised);
    add({9,109.72F,-176.1F},{12,110,-175.75F},Material::Galvanised);
    for(float x:{3.2F,12.2F}) {
        add({x-.18F,110,-183.55F},{x+.18F,124.6F,-183.19F},Material::Rust);
        add({x-.18F,109.35F,-184.5F},{x+.18F,109.72F,-175.75F},Material::Steel);
    }
    add({3,124.0F,-183.55F},{12.5F,124.6F,-183.19F},Material::Steel);
    // Exposed bearing cantilever and axle: the hinge reaction enters this frame.
    add({5.8F,123.85F,-183.55F},{6.2F,124.3F,-181.5F},Material::Steel);
    Part axle{{.30F,.65F,.30F},{6,124,-181.2F},Quat::sRotation(Vec3::sAxisX(),JPH_PI*.5F),Material::Yellow};
    axle.shape=Part::Shape::Cylinder;const auto bearing=kit.add_body(1961,{axle},RVec3::sZero(),Quat::sIdentity(),0,.8F);
    // Short fixed climb, then a genuine sideways air gap to the moving ladder.
    for(float x:{9.45F,10.55F}) add({x-.05F,110.55F,-180.30F},{x+.05F,113.8F,-180.20F},Material::Steel);
    for(float y=111.25F;y<=113.75F;y+=.30F) {
        const float z=y>113.6F ? -179.96F : -180.25F;
        add({9.45F,y-.04F,z-.04F},{10.55F,y+.04F,z+.04F},Material::Galvanised);
    }
    add({8.5F,113.72F,-182.0F},{11.5F,114,-180.0F},Material::Galvanised);
    add({9.3F,112.6F,-180.60F},{10.7F,114,-180.30F},Material::Rust);
    add({11.1F,110,-181.65F},{11.45F,113.72F,-181.3F},Material::Steel);
    // Physical travel stops meet the ladder's lower side rails, not a clock.
    for(float x:{3.75F,8.25F}) {
        add({x-.15F,110,-181.5F},{x+.15F,115.5F,-181.0F},Material::Rust);
        add({x-.20F,114.65F,-181.55F},{x+.20F,115.45F,-180.95F},Material::Timber);
    }
    // Offset receiver: release toward the tower with actual swing momentum.
    add({7.8F,118.72F,-179.4F},{11.5F,119,-177.8F},Material::Galvanised);
    add({8.0F,110,-179.1F},{8.3F,118.72F,-178.8F},Material::Steel);
    add({10.5F,110,-179.1F},{10.8F,118.72F,-178.8F},Material::Steel);
    // A final short hand-over-hand rise, faced south, joins the +121m ring.
    for(float x:{8.45F,9.55F}) add({x-.05F,119.5F,-177.85F},{x+.05F,120.8F,-177.75F},Material::Steel);
    for(float y=120.0F;y<=120.75F;y+=.25F)
        add({8.45F,y-.04F,-177.84F},{9.55F,y+.04F,-177.76F},Material::Galvanised);
    add({8.0F,120.72F,-177.8F},{10.2F,121,-175.75F},Material::Galvanised);
    add({8.4F,120.0F,-177.82F},{9.6F,121,-177.78F},Material::Rust);
    (void)kit.add_body(1960,frame,RVec3::sZero(),Quat::sIdentity(),0,.8F);

    std::vector<Part> ladder;
    const auto member=[&](Vec3 lo,Vec3 hi,Material m,float mass) {
        auto part=span(lo,hi,m);part.mass_kg=mass;ladder.push_back(part);
    };
    for(float x:{-.85F,.85F}) member({x-.15F,-9.2F,-.08F},{x+.15F,-.2F,.08F},Material::Yellow,160);
    for(int i=0;i<24;++i) {
        const float y=-8.9F+.36F*float(i);
        member({-.85F,y-.04F,-.04F},{.85F,y+.04F,.04F},Material::Steel,8);
    }
    member({-1.0F,-.22F,-.15F},{1.0F,-.02F,.15F},Material::Yellow,8);
    const auto body=kit.add_body(2960,ladder,{6,124,-181.2F},Quat::sRotation(Vec3::sAxisZ(),.06F),520,.8F);
    kit.set_damping(body,0,0);
    kit.set_continuous_collision(body);
    // Bearing/rotor contact is excluded locally by the real joint; frame stops
    // and receiver remain collidable. No actuator or body pose writes. Loss is
    // finite/passive; contact stops bound travel. Player grip load is native.
    (void)kit.add_hinge(bearing,body,{6,124,-181.2F},Vec3::sAxisZ(),20);
}
}
