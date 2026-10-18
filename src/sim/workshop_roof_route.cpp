#include "sim/workshop_roof_route.hpp"

#include <vector>

namespace scraperx::sim {

void build_workshop_roof_route(kit::Kit &kit) {
    using JPH::Vec3;
    using kit::Material;
    std::vector<kit::Part> parts;
    const auto block = [&](Vec3 low, Vec3 high) {
        kit::Part part{(high-low)*.5F, (high+low)*.5F,
                       JPH::Quat::sIdentity(), Material::Concrete};
        // Sharp concrete tread faces retain the real receiving edge. No
        // cosmetic rounding changes the native foot-contact geometry.
        part.convex_radius = 0;
        parts.push_back(part);
    };
    // The workshop roof is5.14m; its south wall stops at3.8m. This external
    // maintenance plinth supplies earned launch height, leaving the full
    //3m air gap to the roof edge and the existing wall-step lane clear.
    block({-37.6F,0,-44.2F}, {-34.4F,kWorkshopLaunchTopM,-41.4F});
    // Eight ordinary225mm risers give an accessible approach and a return
    // path. Every solid bears on grade; this is fixed site infrastructure,
    // not a movable cargo stack or a hidden progression constraint.
    constexpr int count = 8;
    constexpr float tread = .4F;
    for (int i=0; i<count; ++i) {
        const float near_z = -47.4F+tread*float(i);
        const float top = kWorkshopLaunchTopM*float(i+1)/float(count);
        block({-36.8F,0,near_z}, {-35.2F,top,near_z+tread});
    }
    (void)kit.add_body(kWorkshopRoofAccessEntity, parts,
        JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, .8F);
}

} // namespace scraperx::sim
