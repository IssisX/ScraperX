#include "sim/cargo_net_route.hpp"
#include <vector>
namespace scraperx::sim {
void build_cargo_net_route(kit::Kit &kit) {
    using kit::Part;
    using kit::Material;
    std::vector<Part> parts;
    const auto span = [&parts](JPH::Vec3 low, JPH::Vec3 high, Material material) {
        parts.push_back({(high-low)*0.5F, (high+low)*0.5F, JPH::Quat::sIdentity(), material});
    };
    // The separate Jolt soft body owns the woven mesh; this body is only its rigid frame/receiver.
    // Broad, flat receiving deck joins the existing AS-017 loading landing
    // at Z=-120.8. No ramp, stair or progression teleport is added.
    span({17.5F,10.76F,-120.8F},{22.5F,11.0F,-118.0F},Material::Galvanised);
    // Fascia gives the native ledge probe a coherent, reachable top-out face.
    span({18.0F,9.75F,-118.14F},{22.0F,11.0F,-118.0F},Material::Hazard);
    for (float x : {17.4F,22.6F}) {
        span({x-0.18F,0.0F,-118.18F},{x+0.18F,13.55F,-117.82F},Material::Rust);
        span({x-0.42F,0.0F,-118.42F},{x+0.42F,0.30F,-117.58F},Material::Concrete);
        // Exposed overhead beams brace the free-standing gantry toward the tower.
        span({x-0.18F,13.15F,-124.05F},{x+0.18F,13.55F,-117.82F},Material::Steel);
        // Receiver guardrails stay outside the four-metre-wide top-out.
        span({x-0.08F,11.0F,-120.8F},{x+0.08F,12.0F,-120.64F},Material::Yellow);
        span({x-0.08F,11.84F,-120.8F},{x+0.08F,12.0F,-118.3F},Material::Yellow);
    }
    span({17.2F,13.15F,-118.18F},{22.8F,13.55F,-117.82F},Material::Yellow);
    // Broad entry marker is a 0.08 m sole step, not a staircase.
    span({17.6F,0.0F,-117.6F},{22.4F,0.08F,-115.8F},Material::Hazard);
    kit.add_body(1952,parts,JPH::RVec3::sZero(),JPH::Quat::sIdentity(),0.0F,0.8F);
}
}
