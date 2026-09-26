// AS-017. Adapted from ScraperX-Claude f872c41, C1 façade architecture.
#include "sim/facade_route.hpp"
#include <algorithm>
#include <vector>
namespace scraperx::sim {
namespace {
using kit::Part;
using kit::Material;
constexpr float kDeck2Top = 22.0F;
[[nodiscard]] Part box(const JPH::Vec3 half, const JPH::Vec3 centre, const Material material) {
    return {half, centre, JPH::Quat::sIdentity(), material};
}

// A box from its low corner to its high corner.
[[nodiscard]] Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (low + high), material);
}

// A member of half-section `half` from a to b.
[[nodiscard]] Part strut(const JPH::Vec3 a, const JPH::Vec3 b, const float half,
                         const Material material) {
    const JPH::Vec3 along = b - a;
    const float length = along.Length();
    return {JPH::Vec3(half, half, 0.5F * length), 0.5F * (a + b),
            JPH::Quat::sFromTo(JPH::Vec3::sAxisZ(), along / length), material};
}

constexpr float kDeck3Top = 33.00F;
constexpr float kDeck4Top = 44.00F;
constexpr float kFaceOuterZ = -123.70F;   // the edge beams' outer face
constexpr float kEdgeBeamDrop = 1.25F;    // an edge beam's bottom under its deck

// The loading landing at deck 2, on knee braces to deck 2's edge beam, with
// its switchgear cabinet standing clear of the duct above.
constexpr float kLandingX0 = 17.80F;
constexpr float kLandingX1 = 22.20F;
constexpr float kLandingZ1 = -120.80F;
constexpr float kCabinetX0 = 19.40F;
constexpr float kCabinetX1 = 20.60F;
constexpr float kCabinetZ0 = -122.50F;
constexpr float kCabinetZ1 = -121.80F;
constexpr float kCabinetTop = kDeck2Top + 1.70F;
// The duct along the face, hung on straps from deck 3's edge beam: its lip
// is 3.6 m over the cabinet's top, in reach at the top of a jump, and its
// south face stands 0.4 m clear of the cabinet so the jump does not meet
// its underside.
constexpr float kDuctX0 = 17.40F;
constexpr float kDuctX1 = 24.40F;
constexpr float kDuctBottom = 25.80F;
constexpr float kDuctTop = 27.30F;
constexpr float kDuctZ1 = -122.90F;
// The vent stack from the duct up to deck 3's edge. Where it meets the edge
// a steel plate lies on the deck with its fascia down the edge beam's face:
// one lip, flush from the beam's foot to the plate's top, that a climber on
// the stack tops out over as soon as it is in reach. The mantle carries the
// body straight up past the stack's head and over the plate, so the stack
// tees off under the deck's top and its two outlets rise 1 m on either side
// of that path, wider apart than a body.
constexpr float kVentX = 24.00F;
constexpr float kVentZ = -123.45F;
constexpr float kVentHalf = 0.07F;
constexpr float kVentTeeY = kDeck3Top - 0.30F;
constexpr float kVentOutletHalfSpan = 0.50F;
constexpr float kVentTop = kDeck3Top + 1.00F;
constexpr float kVentPlateHalfX = 0.60F;
constexpr float kVentPlateDepth = 0.95F;     // from the fascia in over the deck
constexpr float kVentPlateThick = 0.03F;
constexpr float kVentFascia = 0.04F;
// Deck 3's monorail and deck 4's davit, one over the other, and the ladder
// hung from the davit's arm with its stiles 1 m above the arm; its bottom
// rung is a jump from the monorail, its stiles clear of a walker's head.
constexpr float kDavitX = 12.50F;
constexpr float kMonorailZ0 = -127.50F;
constexpr float kMonorailZ1 = -119.30F;
constexpr float kMonorailTop = kDeck3Top + 0.30F;
constexpr float kArmZ0 = -126.50F;
constexpr float kArmZ1 = -120.30F;
constexpr float kArmTop = kDeck4Top + 0.30F;
constexpr float kArmBottom = kDeck4Top - 0.30F;
constexpr float kLadderZ = kArmZ1 + 0.15F;
constexpr float kLadderBottomRung = kMonorailTop + 2.30F;
constexpr float kLadderTop = kArmTop + 1.00F;
constexpr float kLadderHalfWidth = 0.28F;
constexpr float kGrabHalfWidth = 0.45F;

void build_c1(std::vector<Part> &route) {
    // ---- the loading landing and its cabinet ---------------------------------
    route.push_back(span({kLandingX0, kDeck2Top - 0.10F, -124.05F}, {kLandingX1, kDeck2Top, kLandingZ1},
                         Material::Galvanised));
    for (const float x : {kLandingX0 + 0.40F, 0.5F * (kLandingX0 + kLandingX1), kLandingX1 - 0.40F}) {
        route.push_back(strut(JPH::Vec3(x, kDeck2Top - 0.12F, kLandingZ1 + 0.10F),
                              JPH::Vec3(x, kDeck2Top - kEdgeBeamDrop + 0.20F, kFaceOuterZ + 0.06F), 0.05F,
                              Material::Rust));
    }
    for (const float x : {kLandingX0 + 0.04F, kLandingX1 - 0.04F}) {
        for (const float z : {kFaceOuterZ + 0.10F, kLandingZ1 - 0.04F}) {
            route.push_back(box(JPH::Vec3(0.04F, 0.525F, 0.04F), JPH::Vec3(x, kDeck2Top + 0.525F, z),
                                Material::Yellow));
        }
        route.push_back(span({x - 0.04F, kDeck2Top + 1.01F, kFaceOuterZ + 0.10F},
                             {x + 0.04F, kDeck2Top + 1.09F, kLandingZ1 - 0.04F}, Material::Yellow));
    }
    route.push_back(span({kLandingX0 + 0.08F, kDeck2Top, kLandingZ1 - 0.06F},
                         {kLandingX1 - 0.08F, kDeck2Top + 0.10F, kLandingZ1}, Material::Hazard));
    route.push_back(span({kCabinetX0, kDeck2Top, kCabinetZ0}, {kCabinetX1, kCabinetTop, kCabinetZ1},
                         Material::Steel));
    route.push_back(span({kCabinetX0 - 0.02F, kCabinetTop - 0.08F, kCabinetZ0 - 0.02F},
                         {kCabinetX1 + 0.02F, kCabinetTop, kCabinetZ1 + 0.02F}, Material::Hazard));

    // ---- the duct, its intake and its straps ---------------------------------
    route.push_back(span({kDuctX0, kDuctBottom, kFaceOuterZ}, {kDuctX1, kDuctTop, kDuctZ1},
                         Material::Galvanised));
    route.push_back(span({kDuctX0 - 0.40F, kDuctBottom - 0.20F, kFaceOuterZ},
                         {kDuctX0, kDuctTop + 0.30F, kDuctZ1 + 0.10F}, Material::Rust));
    const float deck3_beam_bottom = kDeck3Top - kEdgeBeamDrop;
    for (const float x : {18.20F, 23.40F}) {
        route.push_back(span({x - 0.10F, kDuctTop, kFaceOuterZ}, {x + 0.10F, deck3_beam_bottom + 0.30F,
                              kFaceOuterZ + 0.03F},
                             Material::Steel));
    }

    // ---- the vent stack, on the duct and clamped to deck 3's edge beam -------
    route.push_back(span({kVentX - kVentHalf, kDuctTop, kVentZ - kVentHalf},
                         {kVentX + kVentHalf, kVentTeeY + kVentHalf, kVentZ + kVentHalf}, Material::Galvanised));
    route.push_back(span({kVentX - kVentOutletHalfSpan - kVentHalf, kVentTeeY - kVentHalf, kVentZ - kVentHalf},
                         {kVentX + kVentOutletHalfSpan + kVentHalf, kVentTeeY + kVentHalf, kVentZ + kVentHalf},
                         Material::Galvanised));
    for (const float side : {-1.0F, 1.0F}) {
        const float x = kVentX + side * kVentOutletHalfSpan;
        route.push_back(span({x - kVentHalf, kVentTeeY - kVentHalf, kVentZ - kVentHalf},
                             {x + kVentHalf, kVentTop, kVentZ + kVentHalf}, Material::Galvanised));
    }
    route.push_back(span({kVentX - 0.03F, deck3_beam_bottom + 0.35F, kFaceOuterZ + kVentFascia},
                         {kVentX + 0.03F, deck3_beam_bottom + 0.45F, kVentZ - kVentHalf}, Material::Steel));
    route.push_back(span({kVentX - kVentPlateHalfX, kDeck3Top, kFaceOuterZ - kVentPlateDepth},
                         {kVentX + kVentPlateHalfX, kDeck3Top + kVentPlateThick, kFaceOuterZ + kVentFascia},
                         Material::Steel));
    route.push_back(span({kVentX - kVentPlateHalfX, deck3_beam_bottom, kFaceOuterZ},
                         {kVentX + kVentPlateHalfX, kDeck3Top, kFaceOuterZ + kVentFascia}, Material::Steel));

    // ---- deck 3's monorail -------------------------------------------------------
    route.push_back(span({kDavitX - 0.15F, kDeck3Top, kMonorailZ0}, {kDavitX + 0.15F, kMonorailTop, kMonorailZ1},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.20F, kMonorailTop, kMonorailZ1 - 0.12F},
                         {kDavitX + 0.20F, kMonorailTop + 0.15F, kMonorailZ1 - 0.02F}, Material::Hazard));
    // Held down at its back end by a post to deck 4's underside.
    route.push_back(span({kDavitX - 0.125F, kMonorailTop, kMonorailZ0 + 0.10F},
                         {kDavitX + 0.125F, kDeck4Top - 0.50F, kMonorailZ0 + 0.35F}, Material::Rust));
    // Its trolley and hook block, parked under the beam near its end.
    route.push_back(span({kDavitX - 0.25F, kDeck3Top - 0.30F, -120.90F}, {kDavitX + 0.25F, kDeck3Top, -120.50F},
                         Material::Rust));
    route.push_back(span({kDavitX - 0.10F, kDeck3Top - 1.10F, -120.80F},
                         {kDavitX + 0.10F, kDeck3Top - 0.30F, -120.60F}, Material::Hazard));

    // ---- deck 4's davit: a box girder, deep over the drop -----------------------
    route.push_back(span({kDavitX - 0.175F, kArmBottom, kFaceOuterZ}, {kDavitX + 0.175F, kArmTop, kArmZ1},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.175F, kDeck4Top, kArmZ0}, {kDavitX + 0.175F, kArmTop, kFaceOuterZ},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.125F, kArmTop, kArmZ0 + 0.10F},
                         {kDavitX + 0.125F, kDeck4Top + 10.50F, kArmZ0 + 0.35F}, Material::Rust));

    // ---- the ladder hung from the arm's end ---------------------------------------
    // Its stiles and rungs stop at the arm's top, hooked over it by low plates,
    // so a climber topping out passes over them; the grab handles above stand
    // wider than a body.
    for (const float side : {-1.0F, 1.0F}) {
        const float x = kDavitX + side * kLadderHalfWidth;
        route.push_back(span({x - 0.03F, kLadderBottomRung - 0.10F, kLadderZ - 0.03F},
                             {x + 0.03F, kArmTop, kLadderZ + 0.03F}, Material::Yellow));
        route.push_back(span({x - 0.03F, kArmTop, kArmZ1 - 0.03F}, {x + 0.03F, kArmTop + 0.06F, kLadderZ + 0.03F},
                             Material::Steel));
        const float grab_x = kDavitX + side * kGrabHalfWidth;
        route.push_back(span({grab_x - 0.03F, kArmTop, kLadderZ - 0.03F}, {grab_x + 0.03F, kLadderTop, kLadderZ + 0.03F},
                             Material::Yellow));
        route.push_back(span({std::min(x, grab_x) - 0.03F, kArmTop - 0.30F, kLadderZ - 0.03F},
                             {std::max(x, grab_x) + 0.03F, kArmTop - 0.24F, kLadderZ + 0.03F}, Material::Yellow));
    }
    for (float y = kLadderBottomRung; y <= kArmTop - 0.05F; y += 0.30F) {
        route.push_back(box(JPH::Vec3(kLadderHalfWidth, 0.02F, 0.02F), JPH::Vec3(kDavitX, y, kLadderZ),
                            Material::Steel));
    }
}

} // namespace
void build_facade_route(kit::Kit &kit) {
    std::vector<Part> route;
    build_c1(route);
    kit.add_body(kFacadeRouteEntityId, route, JPH::RVec3(0, -11, 0),
                 JPH::Quat::sIdentity(), 0.0F, 0.8F);
}
} // namespace scraperx::sim
