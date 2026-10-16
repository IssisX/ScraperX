#include "sim/yard_crane_geometry.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;
constexpr float kSteel = 7800;

Part solid(Vec3 half, Vec3 offset, Material material = Material::Steel,
           float density = kSteel, Quat rotation = Quat::sIdentity()) {
    Part p;
    p.half = half;
    p.offset = offset;
    p.rotation = rotation;
    p.material = material;
    p.mass_kg = 8 * half.GetX() * half.GetY() * half.GetZ() * density;
    p.convex_radius = .001F;
    return p;
}

// Four nonoverlapping wall plates: the cavity is real collision geometry.
// Local X is the tube axis; width is local Z and depth local Y.
void rhs(std::vector<Part> &parts, Vec3 first, Vec3 last,
         float width, float depth, float wall, Material material = Material::Steel) {
    const Vec3 delta = last - first;
    const float length = delta.Length();
    const Quat rotation = Quat::sFromTo(Vec3::sAxisX(), delta / length);
    const Vec3 center = (first + last) * .5F;
    for (float sign : {-1.F, 1.F}) {
        parts.push_back(solid({length * .5F, wall * .5F, width * .5F},
            center + rotation * Vec3(0, sign * (depth-wall) * .5F, 0),
            material, kSteel, rotation));
        parts.push_back(solid({length * .5F, (depth-2*wall) * .5F, wall * .5F},
            center + rotation * Vec3(0, 0, sign * (width-wall) * .5F),
            material, kSteel, rotation));
    }
}

float mass(const std::vector<Part> &parts) {
    float total = 0;
    for (const auto &p : parts) total += p.mass_kg;
    return total;
}

Vec3 centroid(const std::vector<Part> &parts) {
    Vec3 weighted = Vec3::sZero();
    for (const auto &p : parts) weighted += p.offset * p.mass_kg;
    return weighted / mass(parts);
}

// Open eye in the local XY plane. Its point is the aperture center.
// All four bars meet without double-counted volume.
void eye(std::vector<Part> &parts, Vec3 point, float half_width, float half_height,
         float wall, float thickness) {
    for (float sign : {-1.F, 1.F}) {
        parts.push_back(solid({wall*.5F, half_height-wall, thickness*.5F},
            point + Vec3(sign*(half_width-wall*.5F), 0, 0), Material::Yellow));
        parts.push_back(solid({half_width, wall*.5F, thickness*.5F},
            point + Vec3(0, sign*(half_height-wall*.5F), 0), Material::Yellow));
    }
}

std::vector<Part> dock_parts(float top, bool catch_on_negative_z) {
    // Dock origin is its western edge and centerline. East end overlaps the
    // existing ring by .5m. Every support/brace remains below the deck or in
    // the two side planes, outside the accepted center driving corridor.
    std::vector<Part> parts;
    parts.push_back(solid({5.25F,.06F,4}, {5.25F,top-.06F,0}, Material::Galvanised));
    // The catch assembly occupies z[-156.48,-143.52] below4.1m.
    // Set back only its neighbouring support plane by1m: the footings and
    // diagonals must not intersect its moving rim or strain the membrane.
    // Full-width crossmembers carry the deck overhang; deck, guardrails and
    // both driving mouths retain their original geometry and ring overlap.
    const float negative_support = catch_on_negative_z ? -2.75F : -3.75F;
    const float positive_support = catch_on_negative_z ? 3.75F : 2.75F;
    for (float z : {negative_support,positive_support}) {
        rhs(parts, {0,top-.30F,z}, {10.5F,top-.30F,z}, .30F,.45F,.012F);
        for (float x : {.35F,9.75F}) {
            rhs(parts, {x,.10F,z}, {x,top-.525F,z}, .35F,.35F,.012F);
            parts.push_back(solid({.65F,.30F,.65F}, {x,-.20F,z}, Material::Concrete,2400));
        }
        rhs(parts, {.35F,.4F,z}, {9.75F,top-.525F,z}, .20F,.20F,.008F);
        rhs(parts, {9.75F,.4F,z}, {.35F,top-.525F,z}, .20F,.20F,.008F);
    }
    for (float z : {-3.75F,3.75F}) {
        // Side guardrails; neither west crane receiving face nor east
        // driving mouth gets a rail or post across its open span.
        rhs(parts, {.25F,top+1.10F,z}, {10.25F,top+1.10F,z}, .06F,.06F,.004F,Material::Yellow);
        for (float x : {.25F,3.5F,7.F,10.25F})
            rhs(parts, {x,top,z}, {x,top+1.10F,z}, .06F,.06F,.004F,Material::Yellow);
    }
    for (float x : {.35F,3.5F,7.F,9.75F})
        rhs(parts, {x,top-.30F,-3.6F}, {x,top-.30F,3.6F}, .20F,.30F,.008F);
    // East rails stop 1.4m either side of the mouth, wider than truck/player.
    for (float sign : {-1.F,1.F}) {
        rhs(parts, {10.35F,top+1.10F,sign*1.4F}, {10.35F,top+1.10F,sign*3.75F},
            .06F,.06F,.004F,Material::Yellow);
        rhs(parts, {10.35F,top,sign*1.4F}, {10.35F,top+1.10F,sign*1.4F},
            .06F,.06F,.004F,Material::Yellow);
    }
    return parts;
}
} // namespace

YardCraneGeometry build_yard_crane_geometry(kit::Kit &kit) {
    for (const auto id : {1450,1451,1452,2450,2451,2452,2453})
        if (kit.body_for_entity(id).valid())
            throw std::invalid_argument("YardCrane geometry entity already owned");
    YardCraneGeometry g;
    const Quat identity = Quat::sIdentity();
    const RVec3 base(-60,0,-150);

    std::vector<Part> foundation;
    foundation.push_back(solid({6,.60F,6}, {0,-.60F,0}, Material::Concrete,2400));
    foundation.push_back(solid({1.0F,.10F,1.4F}, {-6.5F,-.10F,0}, Material::Concrete,2400));
    // Cabinet face/lever are native solids, reached from the western pad.
    foundation.push_back(solid({.30F,.65F,.45F}, {-5.5F,.65F,0}, Material::Yellow));
    foundation.push_back(solid({.18F,.045F,.30F}, {-5.95F,1.0F,0}, Material::Hazard));
    for (float x : {-1.35F,1.35F}) for (float z : {-1.35F,1.35F})
        foundation.push_back(solid({.40F,.08F,.40F}, {x,.08F,z}));
    // Anchored standing operator booth. Raised real steel floor establishes
    // its own contact identity above grade; the western side remains a door.
    foundation.push_back(solid({.9F,.003F,1.2F},{-6.5F,.177F,0},Material::Galvanised));
    for (float x : {-7.35F,-5.65F}) for (float z : {-1.1F,1.1F}) {
        rhs(foundation,{x,0,z},{x,.174F,z},.08F,.08F,.006F);
        rhs(foundation,{x,.18F,z},{x,2.45F,z},.06F,.06F,.004F,Material::Yellow);
    }
    // Open overhead frame keeps the high jib and suspended load in the
    // operator's actual sightline. These four rails are collision geometry.
    for (float x : {-7.4F,-5.6F})
        rhs(foundation,{x,2.45F,-1.25F},{x,2.45F,1.25F},.06F,.06F,.004F,Material::Yellow);
    for (float z : {-1.25F,1.25F})
        rhs(foundation,{-7.4F,2.45F,z},{-5.6F,2.45F,z},.06F,.06F,.004F,Material::Yellow);
    // Console crossbar/joystick mounting faces agree with native hand receipts.
    rhs(foundation,{-5.95F,1.0F,-.45F},{-5.95F,1.0F,.45F},.06F,.06F,.004F);
    g.foundation = kit.add_body(1450,foundation,base,identity,0,.85F);
    for (std::size_t i=0;i<g.docks.size();++i)
        g.docks[i] = kit.add_body(1451+i,dock_parts(i==0?11.F:22.F,i==0),
            RVec3(-36,0,i==0?-140:-160),identity,0,.9F);

    std::vector<Part> mast;
    for (float x : {-1.35F,1.35F}) for (float z : {-1.35F,1.35F})
        rhs(mast,{x,.16F,z},{x,35.7F,z},.30F,.30F,.012F);
    for (int bay=0;bay<9;++bay) {
        const float low=.30F+bay*3.9F, high=low+3.9F;
        for (float side : {-1.35F,1.35F}) {
            rhs(mast,{-1.35F,low,side},{1.35F,low,side},.16F,.16F,.008F);
            rhs(mast,{side,low,-1.35F},{side,low,1.35F},.16F,.16F,.008F);
            rhs(mast,{-1.35F,low,side},{1.35F,high,side},.12F,.12F,.006F);
            rhs(mast,{side,low,-1.35F},{side,high,1.35F},.12F,.12F,.006F);
        }
    }
    mast.push_back(solid({1.5F,.15F,1.5F},{0,35.85F,0}));
    g.mast=kit.add_body(2450,mast,base,identity,mass(mast),.7F);

    std::vector<Part> boom;
    // Lower chord underside is y=-.10. Top chord y=2 gives a 2m truss.
    for (float x : {-.8F,.8F}) for (float y : {0.F,2.F})
        rhs(boom,{x,y,-10},{x,y,35},.20F,.20F,.008F,Material::Yellow);
    for (int bay=0;bay<15;++bay) {
        const float z=-10.F+3.F*bay;
        for (float x : {-.8F,.8F}) {
            rhs(boom,{x,0,z},{x,2,z+3},.12F,.12F,.006F);
            rhs(boom,{x,0,z},{x,2,z},.12F,.12F,.006F);
        }
        // Keep crossmembers above the under-running trolley/hoist corridor.
        rhs(boom,{-.8F,2,z},{.8F,2,z},.12F,.12F,.006F);
        rhs(boom,{-.8F,2,z},{.8F,2,z+3},.10F,.10F,.005F);
    }
    for (float x : {-.8F,.8F}) rhs(boom,{x,0,35},{x,2,35},.12F,.12F,.006F);
    rhs(boom,{-.8F,2,35},{.8F,2,35},.12F,.12F,.006F);
    // Concrete ballast: 3.2 x 2 x .9114583m at 2400kg/m3 = 14000kg.
    // It sits on two real steel cross-bearers behind the pivot.
    for (float z : {-7.50F,-6.50F})
        rhs(boom,{-1.6F,2.15F,z},{1.6F,2.15F,z},.25F,.30F,.012F);
    boom.push_back(solid({1.6F,1,.455729167F},{0,3.30F,-7},Material::Concrete,2400));
    const Quat boom_rotation=Quat::sRotation(Vec3::sAxisY(),g.boom_yaw_initial);
    g.boom=kit.add_body(2451,boom,g.pivot,boom_rotation,mass(boom),.7F);

    std::vector<Part> trolley;
    for (float z : {-.45F,.45F})
        rhs(trolley,{-.95F,0,z},{.95F,0,z},.12F,.12F,.008F,Material::Yellow);
    for (float x : {-.65F,.65F})
        rhs(trolley,{x,0,-.45F},{x,0,.45F},.12F,.12F,.008F);
    rhs(trolley,{-.65F,0,-.25F},{.65F,0,-.25F},.12F,.12F,.008F);
    const Quat axle_rotation=Quat::sRotation(Vec3::sAxisZ(),JPH_PI*.5F);
    for (float x : {-.8F,.8F}) for (float z : {-.45F,.45F}) {
        Part wheel=solid({.11F,.06F,.11F},{x,.14F,z},Material::Steel,kSteel,axle_rotation);
        wheel.shape=Part::Shape::Cylinder;
        wheel.mass_kg=JPH_PI*.11F*.11F*.12F*kSteel;
        trolley.push_back(wheel);
    }
    for (float x : {-.095F,.095F})
        trolley.push_back(solid({.015F,.12F,.225F},{x,-.06F,-.085F},Material::Yellow));
    Part axle=solid({.025F,.11F,.025F},{0,-.04F,0},Material::Steel,kSteel,axle_rotation);
    axle.shape=Part::Shape::Cylinder;
    axle.mass_kg=JPH_PI*.025F*.025F*.22F*kSteel;
    trolley.push_back(axle);
    Part sheave=solid({.11F,.075F,.11F},{0,-.04F,0},Material::Steel,kSteel,axle_rotation);
    sheave.shape=Part::Shape::Cylinder;
    sheave.mass_kg=JPH_PI*.11F*.11F*.15F*kSteel;
    trolley.push_back(sheave);

    std::vector<Part> spreader;
    for (float x : {-.55F,.55F})
        rhs(spreader,{x,0,-1.25F},{x,0,.9F},.10F,.15F,.005F,Material::Yellow);
    for (float z : {-1.30F,.95F})
        rhs(spreader,{-.60F,0,z},{.60F,0,z},.10F,.15F,.005F,Material::Yellow);
    // Eye apertures are below the rails, connected at their upper bars.
    std::size_t n=0;
    for (float z : {.75F,-.75F}) for (float x : {-.52F,.52F}) {
        g.spreader_eyes[n]=Vec3(x,-.125F,z);
        eye(spreader,g.spreader_eyes[n],.045F,.065F,.015F,.025F);
        const float truck_eye_y=z>0?.315F:.565F;
        // Fixed stock for a tire-supported pickup. Forward tilt requires
        // lifting the real blades clear first; a tine-supported chassis is
        // not a valid reference for manufacturing different chain lengths.
        g.sling_lengths[n]=4.4F+g.spreader_eyes[n].GetY()-.604F-truck_eye_y;
        ++n;
    }
    // The real stowed empty truck's3372.956kg load center was measured at
    // chassis-local z=-.419814m. Its lower sling resultant must lie below
    // this actual upper hook; the old empty-only balance at-.275599m let
    // the shallow spreader pitch19degrees and drive a cable into the mast.
    // Extend rear steel rails and solve the physical ballast position from
    // every fixed part's mass/moment. No COM or inertia is overridden.
    constexpr float suspension_z = -.419814F;
    auto ballast_plate = solid({.50F,.006F,.16F},{0,-.069F,0});
    auto ballast_block = solid({.40F,.028044872F,.08F},{0,-.034955128F,0});
    const float ballast_mass = ballast_plate.mass_kg + ballast_block.mass_kg;
    const float fixed_mass = mass(spreader);
    const float fixed_moment = centroid(spreader).GetZ() * fixed_mass;
    const float ballast_z = (suspension_z * (fixed_mass + ballast_mass) - fixed_moment) / ballast_mass;
    if (ballast_z - .16F < -1.25F || ballast_z + .16F > .90F)
        throw std::invalid_argument("Spreader ballast requires real supporting rail length");
    ballast_plate.offset.SetZ(ballast_z);
    ballast_block.offset.SetZ(ballast_z);
    spreader.push_back(ballast_plate);
    spreader.push_back(ballast_block);
    // Later connected crosshead/eye parts have the same target z centroid,
    // preserving the honestly derived empty balance under the actual hook.
    rhs(spreader,{-.50F,0,suspension_z},{.50F,0,suspension_z},.10F,.15F,.005F);
    // Two cheek plates join crosshead to an open 120x160mm lifting eye.
    for (float x : {-.075F,.075F})
        spreader.push_back(solid({.015F,.0525F,.04F},{x,.1275F,suspension_z},Material::Yellow));
    g.hoist_load_eye=Vec3(0,.24F,suspension_z);
    eye(spreader,g.hoist_load_eye,.09F,.08F,.02F,.035F);
    g.spreader_mass_kg=mass(spreader);
    g.spreader_com_local=centroid(spreader);
    g.spreader=kit.add_body(2453,spreader,RVec3(-80,4.4,-150),g.pickup_rotation,
        g.spreader_mass_kg,.7F);
    // The pickup chassis origin is not its load eye. Place the actual
    // trolley/sheave above the balanced empty spreader's actual attachment,
    // so an idle rig does not swing away from the authored pickup points.
    const RVec3 suspension = RVec3(-80,4.4,-150) + g.pickup_rotation * g.hoist_load_eye;
    g.initial_trolley_m = Vec3(suspension - g.pivot).Dot(boom_rotation * Vec3::sAxisZ());
    g.trolley=kit.add_body(2452,trolley,g.pivot+boom_rotation*Vec3(0,-.35F,g.initial_trolley_m),
        boom_rotation,mass(trolley),.6F);
    return g;
}

} // namespace scraperx::sim
