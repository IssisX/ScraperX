#include "sim/cargo_net_route.hpp"
#include <vector>
#include <algorithm>
namespace scraperx::sim {
kit::BodyIndex build_cargo_net_route(kit::Kit &kit) {
    using kit::Part;
    using kit::Material;
    std::vector<Part> parts,foundations;
    const auto span = [&parts](JPH::Vec3 low,JPH::Vec3 high,Material material,float plate_thickness=0.0F) {
        const auto dimensions=high-low;
        Part part{dimensions*0.5F,(high+low)*0.5F,JPH::Quat::sIdentity(),material};
        float a=dimensions.GetX(),b=dimensions.GetY(),c=dimensions.GetZ();
        if(a>b) std::swap(a,b);
        if(b>c) std::swap(b,c);
        if(a>b) std::swap(a,b);
        // CHOSEN hollow steel wall8mm (rails4mm), density7850kg/m³.
        // Deck/fascia equivalent plate thickness is explicit at the call site.
        const float wall=material==Material::Yellow && a<0.20F?0.004F:0.008F;
        part.mass_kg=7850.0F*(plate_thickness>0.0F ? b*c*plate_thickness :
                      (a*b-(a-2*wall)*(b-2*wall))*c);
        parts.push_back(part);
    };
    // The separate Jolt soft body owns the woven mesh; this dynamic body owns its reacting frame/receiver.
    // Broad, flat receiving deck joins the existing AS-017 loading landing
    // at Z=-120.8. No ramp, stair or progression teleport is added.
    span({17.5F,10.76F,-120.8F},{22.5F,11.0F,-118.0F},Material::Galvanised,0.016F);
    // Fascia gives the native ledge probe a coherent, reachable top-out face.
    span({18.0F,9.75F,-118.14F},{22.0F,11.0F,-118.0F},Material::Hazard,0.008F);
    for (float x : {17.4F,22.6F}) {
        span({x-0.18F,0.30F,-118.18F},{x+0.18F,13.55F,-117.82F},Material::Rust);
        foundations.push_back({JPH::Vec3(0.42F,0.15F,0.42F),JPH::Vec3(x,0.15F,-118.0F),JPH::Quat::sIdentity(),Material::Concrete});
        // Exposed overhead beams brace the free-standing gantry toward the tower.
        span({x-0.18F,13.15F,-124.05F},{x+0.18F,13.55F,-117.82F},Material::Steel);
        // Receiver guardrails stay outside the four-metre-wide top-out.
        span({x-0.08F,11.0F,-120.8F},{x+0.08F,12.0F,-120.64F},Material::Yellow);
        span({x-0.08F,11.84F,-120.8F},{x+0.08F,12.0F,-118.3F},Material::Yellow);
    }
    span({17.2F,13.15F,-118.18F},{22.8F,13.55F,-117.82F},Material::Yellow);
    // Broad entry marker is a 0.08 m sole step, not a staircase.
    Part entry{JPH::Vec3(2.4F,0.04F,0.9F),JPH::Vec3::sZero(),JPH::Quat::sIdentity(),Material::Hazard};
    entry.mass_kg=4.8F*1.8F*0.010F*7850.0F;
    // An actual loose stiffened tread plate, not a static step painted as steel.
    kit.add_body(2954,{entry},JPH::RVec3(20.0,0.04,-116.7),JPH::Quat::sIdentity(),entry.mass_kg,0.8F);
    // Visible lower tie rail and bolted clip backs connect the bottom/top
    // woven knots to this same frame; attachment springs carry their load.
    span({17.4F,0.34F,-118.20F},{22.6F,0.50F,-117.98F},Material::Steel);
    for(int column=0;column<9;++column) for(float y:{0.45F,10.75F}) {
        const float x=18.0F+0.5F*column;
        span({x-0.08F,y-0.075F,-118.04F},{x+0.08F,y+0.075F,-117.94F},Material::Steel);
    }
    float mass=0.0F;
    for(const auto &part:parts) mass+=part.mass_kg;
    const auto frame=kit.add_body(kCargoGantryEntity,parts,JPH::RVec3::sZero(),JPH::Quat::sIdentity(),mass,0.8F);
    const auto foundation=kit.add_body(1952,foundations,JPH::RVec3::sZero(),JPH::Quat::sIdentity(),0.0F,0.8F);
    kit.set_damping(frame,0.0F,0.0F);
    // CHOSEN per foot:3MN/m,180kNs/m,5MNm/rad,300kNms/rad.
    // Two footings take gravity, net reaction, rider momentum and deck load.
    for(float x:{17.4F,22.6F})
        kit.add_elastic_mount(foundation,frame,JPH::RVec3(x,0.30,-118.0),3.0e6F,180000.0F,5.0e6F,300000.0F);
    return frame;
}
}
