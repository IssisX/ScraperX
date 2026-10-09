// +33 to +44 m swinging stair, adapted from Claude f872c41 and bound to
// ChatGPT's one Kit/Jolt simulation owner.
#include "swing_stair.hpp"
#include <algorithm>
#include <cmath>
namespace scraperx::sim {
namespace {
using kit::Part;
using kit::Material;
Part box(JPH::Vec3 half,JPH::Vec3 center,Material material) {
    return {half,center,JPH::Quat::sIdentity(),material};
}
Part span(JPH::Vec3 low,JPH::Vec3 high,Material material) {
    return box(.5F*(high-low),.5F*(high+low),material);
}
#include "swing_stair_geometry.inc"
#include "swing_stair_frame.inc"
JPH::Vec3 striker(float z=0) {
    return {.25F+43*.25F/std::tan(kS2Pitch)+.55F,11.18F,z};
}
}
SwingStair::SwingStair(JPH::PhysicsSystem &system,kit::Kit &kit):system_(system),kit_(kit) {
    using namespace JPH;
    std::vector<Part> frame;
    build_s2_statics(frame);
    (void)kit_.add_body(1601,frame,RVec3::sZero(),Quat::sIdentity(),0,.8F);
    stair_=kit_.add_body(kStairEntity,s2_stair_parts(),kS2Hinge,
        Quat::sRotation(Vec3::sAxisZ(),kS2Lift),17500,.8F);
    kit_.set_damping(stair_,0,0);
    (void)kit_.add_hinge({},stair_,kS2Hinge,Vec3::sAxisZ(),1500);
    // Mass belongs to every part: the 50 kg over-center weight and 10 kg
    // of lever/post metal must survive Kit's compound-mass normalization.
    std::vector<Part> trip={
        box({1.575F,.05F,.04F},{.025F,0,0},Material::Hazard),
        box({.05F,.20F,.04F},{1.55F,-.15F,0},Material::Hazard),
        box({.04F,.325F,.04F},{0,.375F,0},Material::Rust),
        box(kS2CatchWeightHalf,{.12F,.75F,0},Material::Rust),
        box({.04F,.04F,.025F},{0,0,.065F},Material::Steel)};
    float volume=0;
    for(std::size_t i=0;i<trip.size();++i) if(i!=3) {
        const auto h=trip[i].half; volume+=8*h.GetX()*h.GetY()*h.GetZ();
    }
    for(std::size_t i=0;i<trip.size();++i) {
        const auto h=trip[i].half;
        trip[i].mass_kg=i==3?50:10*8*h.GetX()*h.GetY()*h.GetZ()/volume;
    }
    const auto lever=kit_.add_body(2601,trip,kS2CatchPivot,Quat::sIdentity(),60,.5F);
    const auto pivot=kit_.add_lever(lever,kS2CatchPivot,Vec3::sAxisZ(),Vec3::sAxisX(),0,kS2CatchTravel);
    constexpr float half_y=.04F;
    const auto handle=kit_.add_body(2602,{box({.22F,half_y,.04F},Vec3::sZero(),Material::Hazard)},
        RVec3(kS2ChainGuide.GetX(),kS2ChainTop-half_y,kS2ChainGuide.GetZ()),Quat::sIdentity(),3,.9F);
    kit_.set_carry(handle,kit::CarryKind::Handle,{0,half_y,0});
    kit_.set_damping(handle,1.5F,1.5F);
    (void)kit_.add_trip_line(lever,{-kS2ChainArm,-.05F,0},handle,{0,half_y,0},kS2ChainGuide,kS2ChainGuide);
    (void)kit_.add_catch(stair_,pivot,kS2CatchRelease,.05F,false);
    // The old blade occupies z=+-0.03. Bed inner edges are +-0.55;
    // stringers start at +-0.89, outside the outer edges at +-0.85. The 1.1 m gap also clears the player on the last treads.
    for(int i=0;i<2;++i) {
        const double z=-122.1+(i==0?-.7:.7);
        pads_[i]=kit_.add_body(2603+i,{box({1.2F,float(kStroke/2),.15F},Vec3::sZero(),Material::Timber)},
            RVec3(-2.1,kBedTop-kStroke/2,z),Quat::sIdentity(),1000,.8F);
        system_.GetBodyInterface().SetMotionType(kit_.body_id(pads_[i]),EMotionType::Kinematic,EActivation::Activate);
        kit_.disable_collision(pads_[i],stair_);
    }
}
void SwingStair::pre_step(float dt) {
    using namespace JPH;
    auto &bodies=system_.GetBodyInterface();
    const auto id=kit_.body_id(stair_);
    const auto transform=bodies.GetWorldTransform(id);
    const auto p=transform*striker();
    // Both visible pads share compression under the rigid landing.
    // Reactions are applied at their two actual contact patches.
    if(p.GetX()>=-3.1 && p.GetX()<=-1.1 && std::abs(p.GetZ()+122.1)<=.1) {
        constexpr double yield=9800,stiffness=125000,damping=400;
        const double penetration=std::max(0.,kBedTop-p.GetY());
        const double next=std::min(kStroke,std::max(state_.front_m,penetration-yield/stiffness));
        state_.plastic_work_j+=yield*(next-state_.front_m);
        state_.front_m=next;
        const double elastic=stiffness*std::max(0.,penetration-next);
        const double closing=-bodies.GetPointVelocity(id,p).GetY();
        const double normal=penetration>0?std::max(0.,elastic+damping*closing):0;
        state_.damping_work_j+=std::max(0.,(normal-elastic)*closing)*dt;
        if(normal>0) for(float z:{-.7F,.7F})
            bodies.AddForce(id,Vec3(0,float(.5*normal),0),transform*striker(z));
    }
    for(int i=0;i<2;++i) bodies.MoveKinematic(kit_.body_id(pads_[i]),
        RVec3(-2.1,kBedTop-kStroke/2-state_.front_m,-122.1+(i==0?-.7:.7)),Quat::sIdentity(),dt);
}
double SwingStair::floor_height() const {
    const auto point=system_.GetBodyInterface().GetWorldTransform(kit_.body_id(stair_))*(striker()+JPH::Vec3(0,.12F,0));
    return point.GetY();
}
}
