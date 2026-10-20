#include "sim/mounted_winch_bay.hpp"
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Part;
using kit::Material;
Part box(Vec3 half, Vec3 at, Material material) {
    Part p{half, at, Quat::sIdentity(), material}; p.convex_radius = .005F; return p;
}
Part brace(Vec3 a, Vec3 b) {
    const Vec3 delta = b - a;
    auto p = box({.045F, delta.Length() * .5F, .045F}, (a + b) * .5F, Material::Rust);
    p.rotation = Quat::sFromTo(Vec3::sAxisY(), delta.Normalized()); return p;
}
}
void build_mounted_winch_bay(kit::Kit &kit, RiggingSystem &rig) {
    using namespace JPH;
    std::vector<Part> frame;
    for (float x : {-3.1F, 3.1F}) for (float z : {-1.2F, 1.2F}) {
        frame.push_back(box({.32F, .12F, .32F}, {x, .12F, z}, Material::Concrete));
        frame.push_back(box({.10F, 2.0F, .10F}, {x, 2.24F, z}, Material::Rust));
    }
    frame.push_back(box({3.3F, .12F, .12F}, {0, 4.25F, 0}, Material::Steel));
    for (float x : {-3.1F, 3.1F}) {
        frame.push_back(box({.12F, .12F, 1.32F}, {x, 4.25F, 0}, Material::Steel));
        for (float z : {-1.2F, 1.2F}) frame.push_back(brace({x, 3.45F, z}, {x, 4.25F, 0}));
    }
    // A real frame-mounted motor/drum enclosure. Its downward cable exit
    // remains clear of the walking/jumping volume. Separate physical control
    // panels keep the high drum usable while the operator watches the load.
    for (float x : {-1.8F, 1.8F}) {
        frame.push_back(box({.30F, .08F, .22F}, {x, 4.09F, 0}, Material::Yellow));
        frame.push_back(box({.22F, .16F, .18F}, {x, 3.89F, 0}, Material::Steel));
        frame.push_back(box({.10F, .12F, .20F}, {x + .34F, 3.89F, 0}, Material::Galvanised));
        frame.push_back(box({.32F, .10F, .13F}, {x, 1.08F, -1.2F}, Material::Yellow));
    }
    // Opposed passive side stays provide transverse support. All droppers
    // and guide cheeks stay outside the clear centre walking lane.
    frame.push_back(box({.10F, .10F, 1.32F}, {0, 4.25F, 0}, Material::Steel));
    for (float z : {-1.2F, 1.2F}) {
        frame.push_back(box({.055F, .925F, .055F}, {0, 3.325F, z}, Material::Rust));
        for (float x : {-.13F, .13F})
            frame.push_back(box({.025F, .18F, .16F}, {x, 2.4F, z}, Material::Yellow));
    }
    const auto fixed = kit.add_body(kMountedWinchFrameEntity, frame, kMountedWinchBayOrigin,
        Quat::sIdentity(), 0, .8F);

    // Chosen 240kg hollow girder/deck assembly, not solid steel density. No
    // guide, frozen rotation or scripted lift: the off-centre eye first tips
    // the beam on its actual floor contact before the load can leave grade.
    const auto beam = kit.add_body(kMountedWinchBeamEntity, {
        box({2.5F, .10F, .45F}, Vec3::sZero(), Material::Steel),
        box({2.45F, .04F, .055F}, {0, -.14F, -.32F}, Material::Rust),
        box({2.45F, .04F, .055F}, {0, -.14F, .32F}, Material::Rust),
        box({.09F, .025F, .09F}, {-1.8F, .125F, 0}, Material::Yellow),
        box({.09F, .025F, .09F}, {1.8F, .125F, 0}, Material::Yellow),
        box({.09F, .035F, .09F}, {0, 0, -.50F}, Material::Yellow),
        box({.09F, .035F, .09F}, {0, 0, .50F}, Material::Yellow),
    }, kMountedWinchBayOrigin + Vec3(0, .18F, 0), Quat::sIdentity(), 240, .7F);
    kit.set_continuous_collision(beam);
    rig.add_eye(beam, {-1.8F, .17F, 0}, 8000);
    rig.add_eye(beam, {1.8F, .17F, 0}, 8000);

    PoweredCable::Config profile;
    profile.min_length_m = .1F; profile.max_length_m = 12;
    profile.max_speed_mps = .25F;
    profile.pull_force_n = profile.brake_force_n = 6000;
    profile.electrical_power_w = 2000; profile.efficiency = .8;
    profile.capacity_j = 250000; // Finite source tuning hypothesis, joules.
    rig.add_powered_mount(fixed, {-1.8F, 3.70F, 0}, {-1.8F, 1.08F, -1.33F}, profile, 8000);
    rig.add_powered_mount(fixed, {1.8F, 3.70F, 0}, {1.8F, 1.08F, -1.33F}, profile, 8000);
    rig.add_eye(beam, {0, 0, -.50F}, 8000);
    rig.add_eye(beam, {0, 0, .50F}, 8000);
    for (int side : {-1, 1}) {
        const float z = float(side) * 1.2F;
        rig.add_fixed_guide(fixed, {0, 2.4F, z}, {float(side)*.15F, 2.4F, z},
            {0, 2.4F, z-float(side)*.10F}, 8000);
        rig.add_eye(fixed, {float(side)*3.1F, 1.30F, z}, 8000);
    }

    // The west stair landing approaches the suspended beam. The receiving
    // surface is the existing east YardStock stack (WorldSolid51): its top
    // is y1.58, x[-104.7,-103.3], z[-38.55,-35.45]. Do not build a deck
    // through those solids. The ordinary jump already reaches this stack.
    {
        constexpr float x = -4.5F;
        std::vector<Part> landing{
            box({1.7F, .08F, .65F}, {x, 1.12F, 0}, Material::Steel),
            box({.10F, .52F, .10F}, {x-.9F, .52F, -.45F}, Material::Rust),
            box({.10F, .52F, .10F}, {x+.9F, .52F, .45F}, Material::Rust),
        };
        for (unsigned i = 0; i < 6; ++i) {
            const float top = .2F * float(i+1);
            landing.push_back(box({.6F, top*.5F, .35F},
                {x, top*.5F, -4.F + .6F*float(i)}, Material::Concrete));
        }
        (void)kit.add_body(kMountedWinchEntryEntity, landing,
            kMountedWinchBayOrigin, Quat::sIdentity(), 0, .8F);
    }
}
} // namespace scraperx::sim
