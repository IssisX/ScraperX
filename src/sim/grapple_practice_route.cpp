#include "sim/grapple_practice_route.hpp"

#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;
using JPH::Vec3;

// CHOSEN steel7850kg/m3; plates are solid, structural tubes have6mm walls.
// Same constituent-mass/COM/inertia convention as split_inspection_route.
void steel_mass(Part &part, const bool plate) {
    const auto size = 2.0F * part.half;
    const float outer = size.GetX()*size.GetY()*size.GetZ();
    const float inner = plate ? 0.0F :
        std::max(0.0F, size.GetX()-.012F)*
        std::max(0.0F, size.GetY()-.012F)*
        std::max(0.0F, size.GetZ()-.012F);
    part.mass_kg = 7850.0F*(outer-inner);
    part.convex_radius = 0.0F;
}

void box(std::vector<Part> &parts, const Vec3 low, const Vec3 high,
         const bool plate = false, const Material material = Material::Steel) {
    Part part{(high-low)*.5F, (high+low)*.5F,
              JPH::Quat::sIdentity(), material};
    steel_mass(part, plate);
    parts.push_back(part);
}

// Actual solid lofted steel panel. The kit cooks these same points for its
// collider and rendered facets; derive material mass from the real hull volume.
void panel(std::vector<Part> &parts, const std::vector<Vec3> &points) {
    JPH::ConvexHullShapeSettings settings(points.data(),int(points.size()),0);
    const auto cooked=settings.Create();
    if (cooked.HasError()) throw std::runtime_error(cooked.GetError().c_str());
    const auto *hull=static_cast<const JPH::ConvexHullShape *>(cooked.Get().GetPtr());
    Part part;
    part.shape=Part::Shape::ConvexHull; part.points=points;
    part.offset=Vec3(14,490.5F,-135.5F); part.material=Material::Yellow;
    part.mass_kg=7850.0F*hull->GetVolume(); part.convex_radius=0;
    parts.push_back(part);
}

void beam(std::vector<Part> &parts, const Vec3 first, const Vec3 last,
          const float half_section, const Material material = Material::Steel) {
    const auto delta = last-first;
    Part part{{half_section, half_section, delta.Length()*.5F},
              (first+last)*.5F,
              JPH::Quat::sFromTo(Vec3::sAxisZ(), delta.Normalized()), material};
    steel_mass(part, false);
    parts.push_back(part);
}

kit::BodyIndex body(kit::Kit &kit, const std::uint64_t entity,
                    std::vector<Part> parts, const Vec3 origin) {
    float mass = 0.0F;
    for (auto &part : parts) {
        mass += part.mass_kg;
        part.offset -= origin;
    }
    const auto result = kit.add_body(entity, parts, JPH::RVec3(origin + kGrapplePracticeTranslation),
                                    JPH::Quat::sIdentity(), mass, .8F);
    kit.set_damping(result, 0.0F, 0.0F);
    return result;
}

void mount(kit::Kit &kit, const kit::BodyIndex structure,
           const JPH::RVec3 point) {
    // CHOSEN existing facade/inspection finite passive anchorage precedent.
    kit.add_elastic_mount({}, structure,
                          point + JPH::RVec3(kGrapplePracticeTranslation), 2.0e6F, 40000.0F,
                          2.0e6F, 40000.0F);
}
} // namespace

GrapplePracticeBodies build_grapple_practice_route(kit::Kit &kit) {
    GrapplePracticeBodies result;
    std::vector<Part> departure;
    // Author coordinates below are translated uniformly+2mX at body/mount
    // creation. Actual departureX12 clears the full inclined inspection truss,
    // rather than only its endpoint shoe. The player approaches on real roof.
    box(departure, {8.8F, 484.234F, -136.8F},
                   {11.2F, 484.25F, -133.46F}, true, Material::Galvanised);
    for (const float x : {9.0F, 11.0F}) {
        // High bearer stops outside original roof; the mounting tongue
        // enters below the slab underside483.75, never through its volume.
        box(departure, {x-.10F, 483.83F, -136.8F},
                       {x+.10F, 484.234F, -133.46F});
        box(departure, {x-.10F, 483.59F, -134.0F},
                       {x+.10F, 483.75F, -132.0F});
        box(departure, {x-.10F, 483.75F, -134.0F},
                       {x+.10F, 483.83F, -133.54F});
        beam(departure, {x, 483.9F, -136.5F},
                        {x, 483.35F, -132.3F}, .10F);
        box(departure, {x-.18F, 483.32F, -132.5F},
                       {x+.18F, 483.75F, -132.1F});
    }
    // Visible overhead yoke, with all uprights behind the launch/swing plane.
    // Posts attach at original north roof z=-132; no floating shelf/anchor.
    for (const float x : {9.0F, 18.2F}) {
        box(departure, {x-.20F, 484.25F, -132.25F},
                       {x+.20F, 484.274F, -131.75F}, true);
        beam(departure, {x, 484.274F, -132.0F},
                        {x, 492.0F, -132.0F}, .12F);
        beam(departure, {x, 492.0F, -132.0F},
                        {x, 492.0F, -134.5F}, .12F);
        beam(departure, {x, 488.6F, -132.0F},
                        {x, 492.0F, -134.5F}, .10F);
    }
    beam(departure, {9.0F, 492.0F, -134.5F},
                    {18.2F, 492.0F, -134.5F}, .16F, Material::Yellow);
    // Rear cheek hangers leave the entire vertical ballistic aperture open;
    // an overhead beam directly over the cup would intercept the high arc.
    for (const float x : {13.4F, 14.6F}) {
        beam(departure, {x, 491.1F, -134.77F},
                        {x, 492.0F, -134.5F}, .06F);
    }
    result.departure = body(kit, kGrapplePracticeDepartureEntity,
                            departure, {10.0F, 484.25F, -135.5F});
    for (const float x : {9.0F, 11.0F, 18.2F})
        mount(kit, result.departure, {x, 483.75, -132.3});

    std::vector<Part> cup;
    // Exposed solid40mm steel crossbar. Four open crook fingers descend around
    // it; collars limit transverse escape. The downward cable window is open.
    const Vec3 bar_center(14,490.44F,-135.5F);
    Part bar{{.020F,.115F,.020F},bar_center,
        JPH::Quat::sFromTo(Vec3::sAxisY(),Vec3::sAxisZ()),Material::Steel};
    bar.shape=Part::Shape::Cylinder; bar.convex_radius=0;
    bar.mass_kg=7850.0F*3.14159265358979323846F*.020F*.020F*.230F;
    cup.push_back(bar);
    // Real annular collars, cooked/rendered from the same convex prisms.
    // Circumscribed inner polygon keeps every collar leaf outside the round bar.
    constexpr float pi=3.14159265358979323846F;
    const float inner=.020F/std::cos(pi/12.0F);
    for (float z:{-.109F,.109F}) for (int i=0;i<12;++i) {
        std::vector<Vec3> points;
        for (float face:{z-.006F,z+.006F}) for (float radius:{inner,.060F})
            for (float angle:{i*pi/6.0F,(i+1)*pi/6.0F})
                points.emplace_back(radius*std::cos(angle),-.060F+radius*std::sin(angle),face);
        panel(cup,points);
    }
    // Outside the collars, finite end supports join the existing rear hangers.
    // Their offset leaves the entire descending crook and eye-to-player ray free.
    for (float side:{-1.0F,1.0F}) {
        beam(cup,bar_center+Vec3(0,0,side*.115F),
                 bar_center+Vec3(0,0,side*.35F),.025F);
        const Vec3 upright(14,491.10F,-135.5F+side*.35F);
        const Vec3 outboard(14.60F,491.10F,-135.5F+side*.35F);
        beam(cup,bar_center+Vec3(0,0,side*.35F),upright,.025F);
        beam(cup,upright,outboard,.025F);
    }
    // Both return legs run on the receiver side of the full high-arc cable
    // cone. Cross back to the left hanger only behind the flight/cable plane.
    beam(cup,{14.60F,491.10F,-135.85F},{14.60F,491.10F,-134.77F},.025F);
    beam(cup,{13.40F,491.10F,-134.77F},{14.60F,491.10F,-134.77F},.025F);
    result.cup = body(kit, kGrapplePracticeCupEntity,
                      cup, kGrapplePracticeCupOrigin - kGrapplePracticeTranslation);
    // This joint joins structural steel only. The hook remains collision-held.
    kit.add_fixed_joint(result.departure, result.cup);

    std::vector<Part> receiver;
    // Existing east roof occupies actual X16.54..20.54,Y483.75..484.25.
    // Lay the receiver plate on its top, with only the outboard overhang
    // carried by face-mounted cantilevers. No steel lies inside concrete.
    box(receiver, {16.8F, 484.25F, -136.8F},
                  {19.2F, 484.266F, -133.46F}, true, Material::Galvanised);
    for (const float z : {-136.55F, -133.75F}) {
        box(receiver, {18.54F, 484.05F, z-.10F},
                      {19.2F, 484.25F, z+.10F});
        box(receiver, {18.54F, 483.70F, z-.18F},
                      {18.564F, 484.25F, z+.18F}, true);
    }
    // Rear rail feet meet the plate; the receiving face and roof exit stay open.
    for (const float x : {17.0F, 18.2F})
        beam(receiver, {x, 484.266F, -136.765F},
                       {x, 485.10F, -136.765F}, .035F);
    beam(receiver, {17.0F, 485.10F, -136.765F},
                   {18.2F, 485.10F, -136.765F}, .035F, Material::Yellow);
    result.receiver = body(kit, kGrapplePracticeReceiverEntity,
                           receiver, kGrapplePracticeReceiver-kGrapplePracticeTranslation);
    for (const float z : {-136.55F, -133.75F})
        mount(kit, result.receiver, {18.54F, 483.97F, z});

    std::vector<Part> recovery;
    // Lower deck catches short misses and reachable loose heads. It does not
    // give upper receiving footing: return stairs exist only at departure.
    // Rear notch clears the entire existing truss, including lower chords
    // and diagonal webs. Continuous playable floor remainsZ<=-134.55.
    box(recovery, {8.8F, 481.726F, -138.3F},
                  {20.2F, 481.75F, -134.55F}, true, Material::Rust);
    box(recovery, {14.8F, 481.726F, -134.55F},
                  {20.2F, 481.75F, -133.50F}, true, Material::Rust);
    for (const float z : {-138.1F, -134.75F})
        beam(recovery, {8.8F, 481.55F, z}, {20.2F, 481.55F, z}, .15F);
    beam(recovery, {14.8F, 481.55F, -133.7F},
                   {20.2F, 481.55F, -133.7F}, .15F);
    for (const float x : {9.0F, 11.0F, 17.0F, 18.2F}) {
        beam(recovery, {x, 481.55F, -138.1F},
                       {x, 481.55F, -134.75F}, .10F);
        // West rods attach to departure bearers; east rods stop at actual
        // underside shoes rather than penetrating the existing concrete roof.
        const bool under_east_roof = x > 16.0F;
        const float rod_top = under_east_roof ? 483.734F : 483.9F;
        beam(recovery, {x, 481.55F, -134.75F},
                       {x, rod_top, -134.75F}, .035F);
        if (under_east_roof)
            box(recovery, {x-.18F, 483.734F, -134.93F},
                          {x+.18F, 483.75F, -134.57F}, true);
    }
    // CHOSEN ten250mm rises/300mm treads,1.2m width, east-to-west return.
    // Stairs run outside the platform footprint, so its underside cannot
    // block the ascending capsule. Top steps sideways onto departure only.
    for (unsigned step = 0; step < 10; ++step) {
        const float x = 12.1F-static_cast<float>(step)*.30F;
        const float top = 482.0F+static_cast<float>(step)*.25F;
        box(recovery, {x-.30F, top-.04F, -138.0F},
                      {x, top, -136.8F}, true, Material::Galvanised);
    }
    for (const float z : {-137.9F, -136.9F})
        beam(recovery, {12.1F, 481.70F, z},
                       {9.1F, 484.20F, z}, .07F);
    // A low outer curb retains floor-rolling tools; no fake inventory refill.
    box(recovery, {8.8F, 481.75F, -138.3F},
                  {20.2F, 481.90F, -138.24F}, true, Material::Yellow);
    box(recovery, {8.8F, 481.75F, -138.24F},
                  {8.86F, 481.90F, -134.55F}, true);
    box(recovery, {20.14F, 481.75F, -138.24F},
                  {20.2F, 481.90F, -133.50F}, true);
    result.recovery = body(kit, kGrapplePracticeRecoveryEntity,
                           recovery, {14.0F, 481.75F, -135.5F});
    // Lower-deck loads enter the departure frame and the real east-roof
    // underside shoes. The overlaid receiver has its own face anchorage.
    kit.add_fixed_joint(result.departure, result.recovery);
    for (const float x : {17.0F, 18.2F})
        mount(kit, result.recovery, {x, 483.75F, -134.75F});
    return result;
}

} // namespace scraperx::sim
