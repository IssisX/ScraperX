#include "sim/catch_net_route.hpp"
#include <algorithm>
namespace scraperx::sim {
CargoNetConfig catch_net_config() {
    CargoNetConfig c;
    c.entity=kCatchNetEntity;c.origin=JPH::RVec3(-40,4,-156);
    // Rotation sends local U to +X, V to +Z, face normal to -Y.
    c.rotation=JPH::Quat::sRotation(JPH::Vec3::sAxisX(),JPH::JPH_PI*.5F);
    // Jolt discrete soft collision uses vertices, not filled triangle interiors.
    // At .5m pitch the loaded mesh opened around the .35m capsule. A .3m
    // grid gives the bottom-cap support normal geometric room to remain valid:
    // worst rest offset .3/sqrt(2), versus .3215m at normal_y=.55.
    c.columns=41;c.rows=41;c.dx=.3F;c.dy=.3F;
    c.surface=CargoNetSurface::ContinuousMembrane;
    c.vertex_mass=150.0F/(c.columns*c.rows);
    // Keep the declared 150kg and aggregate boundary law across resolution.
    const float clips=2.0F*(c.columns+c.rows)-4.0F;
    c.clip_stiffness=1920000.0F/clips;c.clip_damping=9600.0F/clips;
    c.clips=CargoNetClips::Perimeter;c.damping=1.2F;c.iterations=12;
    // CHOSEN: 5% critical damping of each actual native edge pair.
    c.axial_damping_ratio=.05F;
    c.paired_sleep=true;
    c.internally_integrated_clips=true;
    // CHOSEN industrial shock cord: finite preloaded tensile restraint.
    // Deep impact can go slack; upward recovery stretches and dissipates.
    c.tether_enabled=true;c.tether_material=JPH::Vec3(20,20,0);
    c.tether_anchor_body_local=JPH::Vec3(-34,.4F,-150);
    c.tether_rest_length_m=2.5F;c.tether_stiffness=600.0F;c.tether_damping=300.0F;
    return c;
}
kit::BodyIndex build_catch_net_route(kit::Kit &kit) {
    using kit::Part;using kit::Material;
    std::vector<Part> parts,foundations;
    // All dimensions CHOSEN. Hollow steel beams use 8mm wall, density7850kg/m³.
    const auto span=[&](JPH::Vec3 low,JPH::Vec3 high,Material material,float plate=0) {
        const auto d=high-low;Part p{d*.5F,(high+low)*.5F,JPH::Quat::sIdentity(),material};
        float a=d.GetX(),b=d.GetY(),c=d.GetZ();
        if(a>b)std::swap(a,b);
        if(b>c)std::swap(b,c);
        if(a>b)std::swap(a,b);
        const float wall=.008F;
        p.mass_kg=7850*(plate>0?b*c*plate:(a*b-(a-2*wall)*(b-2*wall))*c);
        parts.push_back(p);
    };
    // The rim surrounds the membrane and does not span the catch interior.
    for(float x:{-40.3F,-27.7F})span({x-.18F,3.65F,-156.48F},{x+.18F,4.0F,-143.52F},Material::Steel);
    for(float z:{-156.3F,-143.7F})span({-40.48F,3.65F,z-.18F},{-27.52F,4.0F,z+.18F},Material::Steel);
    for(float x:{-40.3F,-27.7F})for(float z:{-156.3F,-143.7F}) {
        span({x-.18F,.30F,z-.18F},{x+.18F,3.65F,z+.18F},Material::Rust);
        foundations.push_back({JPH::Vec3(.55F,.15F,.55F),JPH::Vec3(x,.15F,z),JPH::Quat::sIdentity(),Material::Concrete});
    }
    // Ground-level bearing/cross ties carry the tether reaction into the same
    // finite mounted frame. All steel is visible and collides natively.
    for(float z:{-156.3F,-143.7F})span({-40.3F,.30F,z-.08F},{-27.7F,.40F,z+.08F},Material::Steel);
    for(float x:{-40.3F,-27.7F})span({x-.08F,.30F,-156.3F},{x+.08F,.40F,-143.7F},Material::Steel);
    span({-40.3F,.30F,-150.08F},{-27.7F,.40F,-149.92F},Material::Steel);
    span({-34.08F,.30F,-156.3F},{-33.92F,.40F,-143.7F},Material::Steel);
    span({-34.16F,.40F,-150.16F},{-33.84F,.48F,-149.84F},Material::Steel);
    // Every physical perimeter attachment has a visible steel clip back.
    const auto cloth=catch_net_config();
    for(int i=0;i<cloth.columns;++i) {
        const float x=float(cloth.origin.GetX())+i*cloth.dx;
        for(float edge:{-156.0F,-144.0F})span({x-.08F,3.87F,edge-.16F},{x+.08F,3.94F,edge+.16F},Material::Steel);
    }
    for(int i=1;i<cloth.rows-1;++i) {
        const float z=float(cloth.origin.GetZ())+i*cloth.dy;
        for(float edge:{-40.0F,-28.0F})span({edge-.16F,3.87F,z-.08F},{edge+.16F,3.94F,z+.08F},Material::Steel);
    }
    // Ordinary solid west-side treads descend outside the catch footprint.
    // 0.25m rise / 0.36m run: a physical stair, not a support proxy or launch.
    for(int i=0;i<16;++i) {
        const float top=4-i*.25F,x=-40.84F-i*.36F;
        span({x,top-.08F,-151.0F},{x+.36F,top,-149.0F},Material::Galvanised,.008F);
    }
    // Stringers carry treads into the same mounted structure.
    for(float z:{-151.0F,-149.0F}) {
        Part p{JPH::Vec3(3.32F,.12F,.10F),JPH::Vec3(-43.38F,2.02F,z),
               JPH::Quat::sRotation(JPH::Vec3::sAxisZ(),.6078F),Material::Steel};
        p.mass_kg=7850*((.24F*.20F)-(.224F*.184F))*6.64F;parts.push_back(p);
    }
    float mass=0;for(const auto &p:parts)mass+=p.mass_kg;
    const auto foundation=kit.add_body(kCatchNetFoundationEntity,foundations,JPH::RVec3::sZero(),JPH::Quat::sIdentity(),0,.8F);
    const auto frame=kit.add_body(kCatchNetFrameEntity,parts,JPH::RVec3::sZero(),JPH::Quat::sIdentity(),mass,.8F);
    kit.set_damping(frame,0,0);
    for(float x:{-40.3F,-27.7F})for(float z:{-156.3F,-143.7F})
        kit.add_elastic_mount(foundation,frame,JPH::RVec3(x,.30F,z),3e6F,180000,5e6F,300000);
    return frame;
}
}
