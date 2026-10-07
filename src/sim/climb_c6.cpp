// C6, the west band: the 242 ring to the 264 ring (see climb_c6.hpp).
#include "sim/climb_c6.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

// The frame's parts are laid out in world coordinates about this origin.
const RVec3 kOrigin(-20.0, 250.0, -150.0);

Vec3 local(const float x, const float y, const float z) {
    return Vec3(x - float(kOrigin.GetX()), y - float(kOrigin.GetY()), z - float(kOrigin.GetZ()));
}

// A box between two corners, in world coordinates.
Part span(const Vec3 a, const Vec3 b, const Material material) {
    Part part;
    part.half = (b - a).Abs() * 0.5F;
    part.offset = local((a.GetX() + b.GetX()) * 0.5F, (a.GetY() + b.GetY()) * 0.5F, (a.GetZ() + b.GetZ()) * 0.5F);
    part.material = material;
    return part;
}

// A round member from a to b, in world coordinates.
Part member(const Vec3 a, const Vec3 b, const float radius, const Material material) {
    Part part;
    const Vec3 run = b - a;
    part.shape = Part::Shape::Cylinder;
    part.half = Vec3(radius, run.Length() * 0.5F, radius);
    part.offset = local((a.GetX() + b.GetX()) * 0.5F, (a.GetY() + b.GetY()) * 0.5F, (a.GetZ() + b.GetZ()) * 0.5F);
    part.rotation = Quat::sFromTo(Vec3::sAxisY(), run.Normalized());
    part.material = material;
    return part;
}

void post(std::vector<Part> &parts, const float x, const float z, const float bottom, const float top) {
    parts.push_back(span(Vec3(x - 0.1F, bottom, z - 0.1F), Vec3(x + 0.1F, top, z + 0.1F), Material::Rust));
}

// The 242 ring's top, under the band; the 264 ring's underside over it.
constexpr float kRing242Top = 242.25F;
constexpr float kRing264Bottom = 263.75F;
} // namespace

void ClimbC6::build(kit::Kit &kit) {
    std::vector<Part> c6;
    // 1. Kentledge on the 242 ring's west band: two concrete ballast blocks,
    // the upper set back, 1.6 m to mantle, 1.4 m clear of the band's inner
    // edge. Its north end, striped, is the take-off: a leap from the last
    // 0.7 m before the edge at a run reaches the platform's lip.
    c6.push_back(span(Vec3(-22.0F, kRing242Top, kKentledgeNorth - 0.1F), Vec3(-19.8F, kRing242Top + 0.8F, -141.65F),
                      Material::Concrete));
    c6.push_back(span(Vec3(-21.9F, kRing242Top + 0.8F, kKentledgeNorth + 0.7F), Vec3(-19.9F, kKentledgeTop, -141.75F),
                      Material::Concrete));
    c6.push_back(span(Vec3(-21.9F, kRing242Top + 0.8F, kKentledgeNorth), Vec3(-19.9F, kKentledgeTop,
                                                                              kKentledgeNorth + 0.7F),
                      Material::Hazard));
    // 2. North of it over a 4.75 m gap, the platform, 3.5 m above the
    // kentledge: a fascia 1.8 m deep on its south face, its lip marked,
    // caught by the hands at the end of a running leap.
    c6.push_back(span(Vec3(-22.2F, kPlatformTop - 0.6F, -153.6F), Vec3(-19.0F, kPlatformTop, -149.6F), Material::Rust));
    c6.push_back(span(Vec3(-22.2F, kPlatformTop - 1.8F, -149.6F), Vec3(-19.0F, kPlatformTop, -149.5F), Material::Rust));
    c6.push_back(span(Vec3(-22.2F, kPlatformTop - 0.08F, -149.5F), Vec3(-19.0F, kPlatformTop + 0.02F, -149.44F),
                      Material::Yellow));
    for (const float x : {-22.0F, -19.2F})
        for (const float z : {-153.4F, -149.8F}) post(c6, x, z, kRing242Top, kPlatformTop - 0.6F);
    // 3. A girder 0.3 m wide out west from the platform over the void, 6.8 m
    // to a landing hung from the outrigger's end.
    c6.push_back(span(Vec3(-29.0F, kPlatformTop - 0.3F, -151.75F), Vec3(-22.2F, kPlatformTop, -151.45F),
                      Material::Yellow));
    c6.push_back(span(Vec3(-30.5F, kPlatformTop - 0.3F, -152.6F), Vec3(-28.6F, kPlatformTop, -150.6F), Material::Rust));
    c6.push_back(member(Vec3(-29.1F, kPlatformTop, -150.75F), Vec3(-29.1F, kOutriggerTop - 0.6F, -150.75F), 0.04F,
                        Material::Steel));
    // 4. The outrigger over it all, out from the band: hang 3.3 from the
    // landing onto its west end, its lip marked. On posts from the platform,
    // a strut under its overhang from the band's edge.
    c6.push_back(span(Vec3(-28.9F, kOutriggerTop - 0.6F, -152.6F), Vec3(-20.2F, kOutriggerTop, -150.6F), Material::Rust));
    c6.push_back(span(Vec3(-28.9F, kOutriggerTop - 1.4F, -152.6F), Vec3(-28.5F, kOutriggerTop - 0.6F, -150.6F),
                      Material::Rust));
    c6.push_back(span(Vec3(-28.96F, kOutriggerTop - 0.08F, -152.6F), Vec3(-28.9F, kOutriggerTop + 0.02F, -150.6F),
                      Material::Yellow));
    post(c6, -20.4F, -150.8F, kPlatformTop, kOutriggerTop - 0.6F);
    post(c6, -20.4F, -152.4F, kPlatformTop, kOutriggerTop - 0.6F);
    c6.push_back(member(Vec3(-22.3F, 244.5F, -150.75F), Vec3(-28.0F, kOutriggerTop - 0.65F, -150.75F), 0.1F,
                        Material::Rust));
    // 5. A girder 0.4 m wide rising north at 24 degrees from the outrigger's
    // north edge to a platform 4.2 m higher: walked up on the balance.
    {
        const float z0 = -152.6F;
        const float z1 = -162.0F;
        const float rise = kInclineTop - kOutriggerTop;
        const float run = z0 - z1;
        const float slope = std::atan2(rise, run);
        Part girder;
        girder.half = Vec3(0.2F, 0.15F, 0.5F * std::sqrt(rise * rise + run * run));
        const Vec3 top_mid(-21.2F, 0.5F * (kOutriggerTop + kInclineTop), 0.5F * (z0 + z1));
        const Vec3 normal(0.0F, std::cos(slope), std::sin(slope));
        const Vec3 centre = top_mid - normal * girder.half.GetY();
        girder.offset = local(centre.GetX(), centre.GetY(), centre.GetZ());
        girder.rotation = Quat::sRotation(Vec3::sAxisX(), slope);
        girder.material = Material::Yellow;
        c6.push_back(girder);
    }
    c6.push_back(span(Vec3(-22.0F, kInclineTop - 0.6F, -164.0F), Vec3(-19.4F, kInclineTop, -162.0F), Material::Rust));
    for (const float x : {-21.8F, -19.6F}) post(c6, x, -163.8F, kRing242Top, kInclineTop - 0.6F);
    // 6. A crossbeam east from that platform into the tower: hang 3.15 onto
    // its west end, then along it, 0.8 m wide, over the well.
    c6.push_back(span(Vec3(-19.4F, kCrossbeamTop - 0.5F, -163.4F), Vec3(-14.8F, kCrossbeamTop, -162.6F),
                      Material::Rust));
    // Its end, a wall from 1.15 m over the platform to the beam: the body
    // meets it at the chest rather than passing under, and the hands find
    // its top as the jump rises.
    c6.push_back(span(Vec3(-19.4F, kCrossbeamTop - 2.0F, -163.4F), Vec3(-19.0F, kCrossbeamTop - 0.5F, -162.6F),
                      Material::Rust));
    c6.push_back(span(Vec3(-19.46F, kCrossbeamTop - 0.08F, -163.4F), Vec3(-19.4F, kCrossbeamTop + 0.02F, -162.6F),
                      Material::Yellow));
    // Hung from the 264 ring by rods either side of it, clear of the walk.
    for (const float z : {-163.55F, -162.45F})
        c6.push_back(member(Vec3(-18.2F, kCrossbeamTop - 0.25F, z), Vec3(-18.2F, kRing264Bottom, z), 0.04F,
                            Material::Steel));
    // 7. At its end a platform, hung from the 264 ring's inner edge by struts.
    c6.push_back(span(Vec3(-17.2F, kCrossbeamTop - 0.5F, -166.0F), Vec3(-14.8F, kCrossbeamTop, -160.0F),
                      Material::Rust));
    for (const float z : {-165.6F, -160.4F})
        c6.push_back(member(Vec3(-15.0F, kCrossbeamTop - 0.5F, z), Vec3(-17.6F, kRing264Bottom - 0.05F, z), 0.08F,
                            Material::Rust));
    // 8. A block on it to hang up onto (3.0), and from its top the last hang
    // (3.25) onto the 264 ring's inner edge.
    c6.push_back(span(Vec3(-17.2F, kCrossbeamTop, -165.5F), Vec3(-15.6F, kBlockTop, -163.5F), Material::Galvanised));
    c6.push_back(span(Vec3(-17.2F, kBlockTop - 0.08F, -163.56F), Vec3(-15.6F, kBlockTop + 0.02F, -163.5F),
                      Material::Yellow));
    // 9. An edge plate on the 264 ring's inner edge over the block, its
    // fascia hanging 1.65 m below the ring: a face for the last hang, and a
    // top, both the climb's own, where the hands close and the feet land.
    c6.push_back(span(Vec3(-18.6F, kRing264Top, -166.0F), Vec3(-17.35F, kRing264Top + 0.1F, -161.0F), Material::Steel));
    c6.push_back(span(Vec3(-17.45F, kRing264Top - 1.65F, -166.0F), Vec3(-17.35F, kRing264Top + 0.1F, -161.0F),
                      Material::Yellow));
    (void)kit.add_body(kFrameEntity, c6, kOrigin, Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace scraperx::sim
