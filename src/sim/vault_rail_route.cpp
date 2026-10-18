#include "sim/vault_rail_route.hpp"

#include <vector>

namespace scraperx::sim {

void build_vault_rail_route(kit::Kit &kit) {
    using JPH::Vec3;
    using kit::Material;
    std::vector<kit::Part> parts;
    const auto steel = [&](Vec3 low, Vec3 high) {
        kit::Part part{(high - low) * .5F, (high + low) * .5F,
                       JPH::Quat::sIdentity(), Material::Steel};
        part.convex_radius = 0;
        parts.push_back(part);
    };
    constexpr float cap_bottom = kVaultRailTopM - .12F;
    // The narrow exposed cap is a real contact surface, supported directly
    // by two end posts. The open centre is neither a wall nor a hidden ramp.
    steel({-43.5F, cap_bottom, -72.09F}, {-40.5F, kVaultRailTopM, -71.91F});
    steel({-43.5F, 0, -72.06F}, {-43.38F, cap_bottom, -71.94F});
    steel({-40.62F, 0, -72.06F}, {-40.5F, cap_bottom, -71.94F});
    (void)kit.add_body(kVaultRailEntity, parts,
        JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, .8F);
}

} // namespace scraperx::sim
