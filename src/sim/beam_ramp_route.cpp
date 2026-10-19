#include "sim/beam_ramp_route.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

Part member(Vec3 half, Vec3 center, Material material) {
    Part part{half, center, Quat::sIdentity(), material};
    part.convex_radius = .005F;
    return part;
}

Part brace(Vec3 from, Vec3 to) {
    const auto delta = to - from;
    auto part = member({.04F, delta.Length() * .5F, .04F},
                       (from + to) * .5F, Material::Rust);
    part.rotation = Quat::sFromTo(Vec3::sAxisY(), delta.Normalized());
    return part;
}
} // namespace

void build_beam_ramp_route(kit::Kit &kit) {
    using namespace JPH;
    std::vector<Part> frame;
    for (float x : {-1.25F, 1.25F}) {
        for (float z : {-.95F, .95F}) {
            frame.push_back(member({.3F, .1F, .3F}, {x, .1F, z}, Material::Concrete));
            frame.push_back(member({.08F, 1.025F, .08F}, {x, 1.225F, z}, Material::Rust));
        }
        frame.push_back(member({.10F, .08F, 1.1F}, {x, 2.3F, 0}, Material::Steel));
        frame.push_back(brace({x, .25F, -.95F}, {x, 2.25F, .95F}));
    }
    for (float z : {-.95F, .95F}) {
        frame.push_back(member({1.4F, .08F, .10F}, {0, 2.3F, z}, Material::Steel));
        frame.push_back(brace({-1.25F, .25F, z}, {1.25F, 2.25F, z}));
    }
    frame.push_back(member({1.5F, .025F, 1.2F}, {0, kBeamRampLandingTop - .025F, 0}, Material::Steel));
    // Fixed foundations belong to the industrial backbone. The reused
    // stock beam remains an independent free body with its original mass,
    // friction and six DOFs, whether resting here or being lifted away.
    (void)kit.add_body(kBeamRampLandingEntity, frame, kBeamRampLandingOrigin,
                       Quat::sIdentity(), 0.0F, .8F);
}

} // namespace scraperx::sim
