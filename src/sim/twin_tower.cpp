#include "sim/twin_tower.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
void TwinTower::build(kit::Kit &kit) {
    using namespace JPH;
    using kit::Material;
    using kit::Part;
    const RVec3 origin(-5.0, kDeckTop, -218.0);
    std::vector<Part> parts;
    const auto box = [&](Vec3 a, Vec3 b, Material material) {
        Part p;
        p.half = (b - a).Abs() * 0.5F;
        p.offset = (a + b) * 0.5F - Vec3(origin);
        p.material = material;
        parts.push_back(p);
    };
    const auto beam = [&](Vec3 a, Vec3 b, float width, Material material) {
        Part p;
        const Vec3 delta = b - a;
        p.half = Vec3(width * 0.5F, delta.Length() * 0.5F, width * 0.5F);
        p.offset = (a + b) * 0.5F - Vec3(origin);
        p.rotation = Quat::sFromTo(Vec3::sAxisY(), delta.Normalized());
        p.material = material;
        parts.push_back(p);
    };
    // A narrow trussed maintenance bridge across the void, seated on A's
    // 220 ring and B's wall brackets. 2.4 m deck, 3 m deep triangulated
    // stringers: no isolated hook stepping stones or unsupported shelves.
    box(Vec3(-8.2F, 219.95F, -220.0F), Vec3(-5.8F, kDeckTop, -172.0F), Material::Steel);
    for (float x : {-8.3F, -5.7F}) {
        beam(Vec3(x, 217.0F, -223.8F), Vec3(x, 217.0F, -173.0F), 0.3F, Material::Rust);
        beam(Vec3(x, 220.05F, -223.8F), Vec3(x, 220.05F, -173.0F), 0.3F, Material::Rust);
        for (int i = 0; i < 8; ++i) {
            const float z = -173.0F - float(i) * 6.0F;
            beam(Vec3(x, 217.0F, z), Vec3(x, 220.05F, z - 6.0F), 0.2F, Material::Rust);
            beam(Vec3(x, 217.0F, z), Vec3(x, 220.05F, z), 0.2F, Material::Rust);
            if (i < 7) beam(Vec3(x, kDeckTop, z - 1.0F), Vec3(x, 221.35F, z - 1.0F), 0.12F, Material::Galvanised);
        }
        beam(Vec3(x, 221.35F, -214.0F), Vec3(x, 221.35F, -173.0F), 0.12F, Material::Galvanised);
    }
    // B's receiving platform is forward of all facade projections (the
    // ducts reach z=-220.9). No opening through the solid shaft is implied.
    box(Vec3(-9.0F, 219.95F, -220.5F), Vec3(-2.0F, kDeckTop, -215.5F), Material::Steel);
    // 21.8 degree service incline, then a 1.4 m mantle onto its end plinth.
    // Its 2 m tread and both landings permit an ordinary retreat as well.
    const float slope = std::atan2(4.0F, 10.0F);
    Part incline;
    incline.half = Vec3(0.5F * std::sqrt(116.0F), 0.15F, 1.0F);
    incline.rotation = Quat::sRotation(Vec3::sAxisZ(), -slope);
    incline.offset = Vec3(-12.0F, 222.25F, -218.0F) - Vec3(origin) -
                     incline.rotation * Vec3(0.0F, 0.15F, 0.0F);
    incline.material = Material::Galvanised;
    parts.push_back(incline);
    box(Vec3(-19.0F, 223.95F, -220.0F), Vec3(-17.0F, kInclineTop, -216.0F), Material::Steel);
    box(Vec3(-23.0F, 223.95F, -220.0F), Vec3(-19.0F, kLandingTop, -216.0F), Material::Rust);
    // Wall brackets reach INTO the original solid mass; their knees and
    // diagonal struts carry each deck. They stay below the walking surface.
    for (float x : {-7.0F, -3.0F, -12.0F, -18.0F, -22.0F}) {
        const float top = x < -16.0F ? kInclineTop : (x < -9.0F ? 222.25F : kDeckTop);
        beam(Vec3(x, top - 0.35F, -224.5F), Vec3(x, top - 0.35F, -216.2F), 0.3F, Material::Rust);
        beam(Vec3(x, top - 5.0F, -224.5F), Vec3(x, top - 0.35F, -216.2F), 0.3F, Material::Rust);
    }
    // A real thin square member, within existing hook eligibility, carried
    // by posts off the upper landing. The broad facade remains unhookable.
    for (float z : {-219.7F, -216.3F})
        beam(Vec3(-20.5F, kLandingTop, z), Vec3(-20.5F, 228.5F, z), 0.16F, Material::Galvanised);
    beam(Vec3(-20.5F, 228.5F, -219.7F), Vec3(-20.5F, 228.5F, -216.3F), 0.12F, Material::Yellow);
    (void)kit.add_body(kFrameEntity, parts, origin, Quat::sIdentity(), 0.0F, 0.8F);
}
} // namespace scraperx::sim
