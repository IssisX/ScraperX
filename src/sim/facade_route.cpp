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

void build_c1(std::vector<Part> &route, std::vector<std::size_t> &starts) {
    starts.push_back(route.size());
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
    starts.push_back(route.size()); // cabinet, bolted to the loading landing
    route.push_back(span({kCabinetX0, kDeck2Top, kCabinetZ0}, {kCabinetX1, kCabinetTop, kCabinetZ1},
                         Material::Steel));
    route.push_back(span({kCabinetX0 - 0.02F, kCabinetTop - 0.08F, kCabinetZ0 - 0.02F},
                         {kCabinetX1 + 0.02F, kCabinetTop, kCabinetZ1 + 0.02F}, Material::Hazard));

    starts.push_back(route.size());
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

    starts.push_back(route.size());
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

    starts.push_back(route.size());
    // ---- deck 3's monorail -------------------------------------------------------
    route.push_back(span({kDavitX - 0.15F, kDeck3Top, kMonorailZ0}, {kDavitX + 0.15F, kMonorailTop, kMonorailZ1},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.20F, kMonorailTop, kMonorailZ1 - 0.12F},
                         {kDavitX + 0.20F, kMonorailTop + 0.15F, kMonorailZ1 - 0.02F}, Material::Hazard));
    // Held down at its back end by a post to deck 4's underside.
    route.push_back(span({kDavitX - 0.125F, kMonorailTop, kMonorailZ0 + 0.10F},
                         {kDavitX + 0.125F, kDeck4Top - 0.50F, kMonorailZ0 + 0.35F}, Material::Rust));
    starts.push_back(route.size());
    // Its trolley and hook block, parked under the beam near its end.
    route.push_back(span({kDavitX - 0.25F, kDeck3Top - 0.30F, -120.90F}, {kDavitX + 0.25F, kDeck3Top, -120.50F},
                         Material::Rust));
    route.push_back(span({kDavitX - 0.10F, kDeck3Top - 1.10F, -120.80F},
                         {kDavitX + 0.10F, kDeck3Top - 0.30F, -120.60F}, Material::Hazard));

    starts.push_back(route.size());
    // ---- deck 4's davit: a box girder, deep over the drop -----------------------
    route.push_back(span({kDavitX - 0.175F, kArmBottom, kFaceOuterZ}, {kDavitX + 0.175F, kArmTop, kArmZ1},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.175F, kDeck4Top, kArmZ0}, {kDavitX + 0.175F, kArmTop, kFaceOuterZ},
                         Material::Yellow));
    route.push_back(span({kDavitX - 0.125F, kArmTop, kArmZ0 + 0.10F},
                         {kDavitX + 0.125F, kDeck4Top + 10.50F, kArmZ0 + 0.35F}, Material::Rust));

    starts.push_back(route.size());
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
    std::vector<std::size_t> starts;
    build_c1(route, starts);
    starts.push_back(route.size());
    std::vector<kit::BodyIndex> groups;
    for (unsigned group=0; group+1<starts.size(); ++group) {
        std::vector<Part> parts(route.begin()+starts[group], route.begin()+starts[group+1]);
        float mass=0;
        for (auto &part:parts) {
            const auto d=2.0F*part.half;
            // CHOSEN steel rho7850kg/m³; cabinet/duct sheet3mm,
            // other closed steel members6mm. The collision envelope stays
            // exact; constituent masses determine native COM and inertia.
            const float wall=(group==1 || group==2)?0.003F:0.006F;
            const float inner_x=std::max(0.0F,d.GetX()-2*wall);
            const float inner_y=std::max(0.0F,d.GetY()-2*wall);
            const float inner_z=std::max(0.0F,d.GetZ()-2*wall);
            part.mass_kg=7850.0F*(d.GetX()*d.GetY()*d.GetZ()-inner_x*inner_y*inner_z);
            // CHOSEN80kg switchgear payload in the original cabinet shell.
            if(group==1 && &part==&parts.front()) part.mass_kg+=80.0F;
            mass+=part.mass_kg;
        }
        groups.push_back(kit.add_body(group==0?kFacadeRouteEntityId:2559+group,
            parts,JPH::RVec3(0,-11,0),JPH::Quat::sIdentity(),mass,0.8F));
        kit.set_damping(groups.back(),0,0);
    }
    const auto world_mount=[&](unsigned group,JPH::RVec3 point) {
        // Passive elastic attachment at the existing visible beam/strap.
        // CHOSEN2MN/m,40kNs/m,2MNm/rad,40kNms/rad; fixed neutral frames.
        kit.add_elastic_mount({},groups[group],point,2.0e6F,40000,2.0e6F,40000);
    };
    // All positions below are DERIVED original authored positions minus11m.
    for(float x:{18.20F,21.80F}) world_mount(0,{x,10.0,-123.64});
    kit.add_fixed_joint(groups[0],groups[1]); // actual cabinet mounting
    for(float x:{18.20F,23.40F}) world_mount(2,{x,21.0,-123.685});
    world_mount(3,{24.0,21.15,-123.64}); // stack clamp and receiver plate
    kit.add_elastic_mount(groups[2],groups[3],{24.0,16.3,-123.45},
        2.0e6F,40000,2.0e6F,40000);
    world_mount(4,{12.5,22.0,-123.85});
    world_mount(4,{12.5,32.5,-127.275});
    // Existing parked trolley is a finite passive carriage on its actual
    // monorail. No autonomous motion or machine redesign is introduced.
    kit.add_sliding_track(groups[4],groups[5],JPH::Vec3::sAxisZ(),-6.45F,1.0F,120.0F);
    world_mount(6,{12.5,33.0,-123.7});
    world_mount(6,{12.5,43.5,-126.275});
    // Fixed maintenance ladder remains fixed in purpose, with finite mounting
    // compliance; it is not the separate AS-026 pendulum-ladder encounter.
    for(float x:{12.22F,12.78F})
        kit.add_elastic_mount(groups[6],groups[7],{x,33.3,-120.25},
            2.0e6F,40000,200000.0F,10000.0F);

}
} // namespace scraperx::sim
