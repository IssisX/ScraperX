// C7, the west face: the 264 ring to the mast-pitman walkway (see climb_c7.hpp).
#include "sim/climb_c7.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

// The frame's parts are laid out in world coordinates about this origin.
const RVec3 kOrigin(-24.0, 276.0, -152.0);

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

// A hanger rod from (x, y, z) up to the cantilever over the face.
constexpr float kCantileverY = 287.5F;
void rod(std::vector<Part> &parts, const float x, const float y, const float z) {
    parts.push_back(member(Vec3(x, y, z), Vec3(x, kCantileverY, z), 0.04F, Material::Steel));
}

// A platform: a deck x0..x1, z0..z1 (z0 > z1: its south edge first), 0.5 m
// deep, its top at `top`.
void platform(std::vector<Part> &parts, const float x0, const float x1, const float z0, const float z1,
              const float top) {
    parts.push_back(span(Vec3(x0, top - 0.5F, z1), Vec3(x1, top, z0), Material::Rust));
}

// A fascia hung from an edge to `bottom`, its lip marked: a face for the
// hands and feet. Along x at z (facing +z when `faces_south`), or along z at x.
void fascia_z(std::vector<Part> &parts, const float x0, const float x1, const float z, const float bottom,
              const float top, const bool faces_south) {
    const float d = faces_south ? 0.1F : -0.1F;
    parts.push_back(span(Vec3(x0, bottom, z), Vec3(x1, top, z + d), Material::Rust));
    parts.push_back(span(Vec3(x0, top - 0.08F, z + d), Vec3(x1, top + 0.02F, z + d * 1.6F), Material::Yellow));
}
void fascia_x(std::vector<Part> &parts, const float x, const float z0, const float z1, const float bottom,
              const float top, const bool faces_west) {
    const float d = faces_west ? -0.1F : 0.1F;
    parts.push_back(span(Vec3(x, bottom, z1), Vec3(x + d, top, z0), Material::Rust));
    parts.push_back(span(Vec3(x + d, top - 0.08F, z1), Vec3(x + d * 1.6F, top + 0.02F, z0), Material::Yellow));
}

// A swung-onto lip (as the yard's gallery): a painted face set just inside
// the platform's south edge, from 1.6 m to 0.2 m under its top, so nothing
// stands proud of the lip into the swing.
void swing_lip(std::vector<Part> &parts, const float x0, const float x1, const float z, const float top) {
    parts.push_back(span(Vec3(x0, top - 1.6F, z - 0.1F), Vec3(x1, top - 0.2F, z), Material::Yellow));
}

// A pipe across a lane (along x), a hold: its section under the hands' grip.
void pipe(std::vector<Part> &parts, const float lane_x, const float y, const float z) {
    constexpr float kHalf = 0.035F;
    parts.push_back(span(Vec3(lane_x - 0.62F, y - kHalf, z - kHalf), Vec3(lane_x + 0.62F, y + kHalf, z + kHalf),
                         Material::Galvanised));
}

constexpr float kBeamX = 0.74F;   // a gallery's beams either side of its lane
} // namespace

void ClimbC7::build(kit::Kit &kit) {
    std::vector<Part> c7;
    const float lane1 = kLaneOneX;
    // 1. The kentledge on the 264 ring's west band: 1.6 m to mantle from the
    // south; its north end, striped, is gallery one's take-off.
    c7.push_back(span(Vec3(lane1 - 1.2F, kRing264Top, kKentledgeNorth - 0.1F),
                      Vec3(lane1 + 1.2F, kRing264Top + 0.8F, kKentledgeNorth + 4.6F), Material::Concrete));
    c7.push_back(span(Vec3(lane1 - 1.1F, kRing264Top + 0.8F, kKentledgeNorth + 0.7F),
                      Vec3(lane1 + 1.1F, kKentledgeTop, kKentledgeNorth + 4.5F), Material::Concrete));
    c7.push_back(span(Vec3(lane1 - 1.1F, kRing264Top + 0.8F, kKentledgeNorth),
                      Vec3(lane1 + 1.1F, kKentledgeTop, kKentledgeNorth + 0.7F), Material::Hazard));
    // 2. Gallery one: five pipes north from 0.8 m past the take-off, 3.1 m
    // over it, 1 m apart, between two beams on posts from the band.
    for (int i = 0; i < kGalleryOnePipes; ++i) pipe(c7, lane1, kGalleryOneY, kGalleryOneFirstZ - float(i));
    const float last1 = kGalleryOneFirstZ - float(kGalleryOnePipes - 1);
    for (const float side : {-1.0F, 1.0F}) {
        const float bx = lane1 + side * kBeamX;
        c7.push_back(span(Vec3(bx - 0.12F, kGalleryOneY - 0.035F, last1 - 0.4F),
                          Vec3(bx + 0.12F, kGalleryOneY + 0.265F, kGalleryOneFirstZ + 0.5F), Material::Rust));
        for (const float z : {kGalleryOneFirstZ + 0.38F, last1 - 0.28F})
            post(c7, bx, z, kRing264Top, kGalleryOneY - 0.035F);
    }
    // Platform one, its lip 0.9 m past the last pipe, 0.1 m over the pipes.
    const float p1_north = kPlatformOneSouth - 3.0F;
    platform(c7, lane1 - 1.2F, lane1 + 1.2F, kPlatformOneSouth, p1_north, kPlatformOneTop);
    swing_lip(c7, lane1 - 1.2F, lane1 + 1.2F, kPlatformOneSouth, kPlatformOneTop);
    for (const float x : {lane1 - 1.05F, lane1 + 1.05F})
        for (const float z : {kPlatformOneSouth - 0.15F, p1_north + 0.15F})
            post(c7, x, z, kRing264Top, kPlatformOneTop - 0.5F);
    // 3. The outrigger, out west over the drop from over the band: a hang of
    // 3.25 up from platform one's north edge onto its south lip, its fascia
    // from 1.15 m over the platform so the body meets it at the chest.
    const float o_north = kOutriggerSouth - 2.2F;
    platform(c7, kOutriggerWestX, lane1 + 1.2F, kOutriggerSouth, o_north, kOutriggerTop);
    fascia_z(c7, kOutriggerWestX, lane1 + 1.2F, kOutriggerSouth, kPlatformOneTop + 1.15F, kOutriggerTop, true);
    for (const float x : {kOutriggerWestX + 0.2F, -24.0F, lane1 + 1.0F}) rod(c7, x, kOutriggerTop, o_north + 0.2F);
    // 4. Gallery two, up and out over the drop: eight pipes north from over
    // the outrigger's west end, each 0.95 m on and 0.3 m higher, the first
    // 2.6 m over the outrigger (a standing leap's catch). Its beams rise with
    // it, hung from the cantilever.
    const float lane2 = kLaneTwoX;
    for (int i = 0; i < kGalleryTwoPipes; ++i)
        pipe(c7, lane2, kGalleryTwoFirstY + kGalleryTwoRise * float(i), kGalleryTwoFirstZ - kGalleryTwoPitch * float(i));
    const float last2_z = kGalleryTwoFirstZ - kGalleryTwoPitch * float(kGalleryTwoPipes - 1);
    const float last2_y = kGalleryTwoFirstY + kGalleryTwoRise * float(kGalleryTwoPipes - 1);
    for (const float side : {-1.0F, 1.0F}) {
        const float bx = lane2 + side * kBeamX;
        const Vec3 from(bx, kGalleryTwoFirstY + 0.115F, kGalleryTwoFirstZ + 0.4F);
        const Vec3 to(bx, last2_y + 0.115F, last2_z - 0.4F);
        c7.push_back(member(from, to, 0.12F, Material::Rust));
        rod(c7, bx, kGalleryTwoFirstY + 0.2F, kGalleryTwoFirstZ + 0.3F);
        rod(c7, bx, last2_y + 0.2F, last2_z - 0.3F);
    }
    // Platform two, its lip 0.9 m past the last pipe and 0.1 m over it.
    const float p2_south = last2_z - 0.9F;
    const float p2_north = p2_south - 3.15F;
    platform(c7, lane2 - 1.2F, lane2 + 1.2F, p2_south, p2_north, kPlatformTwoTop);
    swing_lip(c7, lane2 - 1.2F, lane2 + 1.2F, p2_south, kPlatformTwoTop);
    rod(c7, lane2 - 1.0F, kPlatformTwoTop, p2_north + 0.2F);
    rod(c7, lane2 + 1.0F, kPlatformTwoTop, p2_north + 0.2F);
    // 5. Platform three east of it, 3.25 m up: its west fascia faces platform
    // two's east edge; platform four north of three, 3.25 m higher again.
    const float p3_west = lane2 + 1.5F;
    platform(c7, p3_west, -22.0F, p2_south - 0.15F, p2_north, kPlatformThreeTop);
    fascia_x(c7, p3_west, p2_south - 0.15F, p2_north, kPlatformTwoTop + 1.15F, kPlatformThreeTop, true);
    rod(c7, -22.2F, kPlatformThreeTop, p2_south - 0.3F);
    rod(c7, p3_west + 0.2F, kPlatformThreeTop, p2_south - 0.3F);
    const float p4_south = p2_north - 0.3F;
    const float p4_north = p4_south - 3.2F;
    platform(c7, -25.0F, kRing286Edge - 0.3F, p4_south, p4_north, kPlatformFourTop);
    fascia_z(c7, -25.0F, kRing286Edge - 0.3F, p4_south, kPlatformThreeTop + 1.15F, kPlatformFourTop, true);
    rod(c7, -24.8F, kPlatformFourTop, p4_north + 0.2F);
    // 6. The 286 ring's west band over platform four's east edge: a fascia on
    // its outer edge, the last hang (2.65 m) onto the band.
    fascia_x(c7, kRing286Edge, p4_south - 0.2F, p4_north + 0.2F, kPlatformFourTop + 1.15F, kRing286Top, true);
    // The cantilever the outer pieces hang from, out west from the band's
    // edge over the drop, above head height over the band's edge.
    for (const float z : {-149.0F, -156.0F, -161.5F})
        c7.push_back(span(Vec3(-31.0F, kCantileverY, z - 0.15F), Vec3(kRing286Edge, kCantileverY + 0.35F, z + 0.15F),
                          Material::Rust));
    c7.push_back(span(Vec3(-31.0F, kCantileverY, -162.0F), Vec3(-30.6F, kCantileverY + 0.35F, -148.6F),
                      Material::Rust));
    // 7. On the band, south of the walkway: a block (1.6 m to mantle from the
    // south), and on the walkway's south edge a fascia, the hang from the
    // block's north edge onto the walkway.
    const float b_north = -147.7F;
    c7.push_back(span(Vec3(-20.0F, kRing286Top, b_north - 0.1F), Vec3(-18.0F, kRing286Top + 0.8F, b_north + 2.9F),
                      Material::Concrete));
    c7.push_back(span(Vec3(-19.9F, kRing286Top + 0.8F, b_north), Vec3(-18.1F, kBlockTop, b_north + 2.8F),
                      Material::Concrete));
    // A 0.3 m step on its north end, walked up like a stair: from its edge
    // the walkway's lip is 3.1 m up, a hang's reach.
    c7.push_back(span(Vec3(-19.9F, kBlockTop, b_north), Vec3(-18.1F, kBlockStepTop, b_north + 1.1F),
                      Material::Steel));
    fascia_z(c7, -20.0F, -18.0F, -148.0F, kBlockStepTop + 1.15F, kWalkwayTop, true);
    (void)kit.add_body(kFrameEntity, c7, kOrigin, Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace scraperx::sim
