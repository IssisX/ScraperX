#include "sim/rigging_bay.hpp"
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Part;
using kit::Material;
Part box(Vec3 half, Vec3 at, Material material) {
    Part p{half, at, Quat::sIdentity(), material}; p.convex_radius = .003F; return p;
}
}
void build_rigging_bay(kit::Kit &kit, RiggingSystem &rig) {
    using namespace JPH;
    const auto body = [&](std::uint64_t entity, std::vector<Part> parts, Vec3 at, float mass) {
        return kit.add_body(entity, parts, kRiggingBayOrigin + at, Quat::sIdentity(), mass, .7F);
    };
    std::vector<Part> frame;
    for (float x : {-4.F, 4.F}) for (float z : {-1.8F, 1.8F}) {
        frame.push_back(box({.3F, .12F, .3F}, {x,.12F,z}, Material::Concrete));
        frame.push_back(box({.10F,2.15F,.10F}, {x,2.39F,z}, Material::Rust));
    }
    for (float z : {-1.8F,1.8F})
        frame.push_back(box({4.1F,.12F,.12F}, {0,4.55F,z}, Material::Steel));
    for (float x : {-3.5F,2.5F}) {
        frame.push_back(box({.12F,.10F,1.9F}, {x,4.55F,0}, Material::Steel));
        // Visible sheave cheeks flank the admitted fixed cable corridor.
        for (float z : {.78F,1.02F})
            frame.push_back(box({.22F,.22F,.025F}, {x,4.3F,z}, Material::Yellow));
    }
    // The low pilot's overhead travel stop has a real frame reaction.
    for (float x : {-3.6F,-1.4F}) {
        frame.push_back(box({.12F,.1F,.25F}, {x,2.43F,-.95F}, Material::Steel));
        frame.push_back(box({.12F,.035F,.25F}, {x,.035F,-.95F}, Material::Steel));
    }
    frame.push_back(box({.5F,.245F,.5F}, {2.5F,.245F,0}, Material::Concrete));
    const auto fixed = body(kRiggingFrameEntity, frame, Vec3::sZero(), 0);

    const auto platform = body(kRiggingPlatformEntity, {
        box({1.2F,.08F,1.2F}, {0,0,0}, Material::Steel),
        box({.10F,.08F,1.1F}, {-1.1F,-.1F,0}, Material::Rust),
        box({.10F,.08F,1.1F}, {1.1F,-.1F,0}, Material::Rust),
        box({.045F,.2F,.045F}, {-1,.28F,.9F}, Material::Yellow),
    }, {-2.5F,.25F,0}, 85);
    // Existing governor is finite and can only oppose measured motion;
    // gravity/counterweight supply all ascent work. Target creep is existing
    // owner policy, not a commanded body trajectory.
    kit.add_guide(platform, Vec3::sAxisY(), 0, 2.1F, .7F, 3000, .8F);
    kit.set_continuous_collision(platform);

    const auto weight = body(kRiggingWeightEntity, {
        box({.65F,.06F,.65F}, {0,0,0}, Material::Steel),
        box({.06F,.35F,.65F}, {-.59F,.35F,0}, Material::Yellow),
        box({.06F,.35F,.65F}, {.59F,.35F,0}, Material::Yellow),
        box({.53F,.35F,.06F}, {0,.35F,-.59F}, Material::Rust),
        box({.045F,.3F,.045F}, {0,.4F,.9F}, Material::Yellow),
    }, {2.5F,2.65F,0}, 120);
    kit.add_guide(weight, Vec3::sAxisY(), -2.1F, 0, .7F, 3000, .8F);
    kit.set_continuous_collision(weight);

    const auto lever = body(kRiggingLeverEntity, {
        box({.045F,.35F,.045F}, {0,-.35F,0}, Material::Rust),
        box({.23F,.045F,.055F}, {0,-.7F,0}, Material::Yellow),
        // Hollow counterbalance puts the actual COM over centre after the
        // pull. The released lever rests against its stop without a timer.
        box({.16F,.16F,.16F}, {.12F,.6F,0}, Material::Yellow),
    }, {-4.5F,1.5F,-.5F}, 8);
    const auto hinge = kit.add_lever(lever, kRiggingBayOrigin + Vec3(-4.5F,1.5F,-.5F),
        Vec3::sAxisZ(), Vec3::sAxisX(), 0, .9F);
    const Vec3 lever_com(kit.body_center_of_mass_position(lever) - kit.body_position(lever));
    kit.set_carry(lever, kit::CarryKind::Handle, Vec3(0,-.7F,0) - lever_com);
    kit.add_catch(weight, hinge, .4F, .10F, true);

    // Service access is separate from the receiving deck. It puts the real
    // counterweight eye and fixed guide within the assisted tool's reach.
    std::vector<Part> service;
    for (int i = 0; i < 9; ++i) {
        const float top = .2F * float(i + 1);
        service.push_back(box({.85F,top*.5F,.3F}, {2.5F,top*.5F,6.8F-.6F*float(i)}, Material::Concrete));
    }
    service.push_back(box({1.0F,.08F,.7F}, {2.5F,1.72F,1.1F}, Material::Steel));
    (void)body(1496, service, Vec3::sZero(), 0);
    // Three separate reusable25kg blocks. Nothing counts ballast by flag:
    // its actual contact with the tray transfers its full weight.
    for (int i = 0; i < 3; ++i) {
        const auto block = body(std::uint64_t(2496 + i), {
            box({.23F,.2F,.23F}, Vec3::sZero(), Material::Timber)
        }, {4.8F+.6F*float(i),.2F,4.5F}, 25);
        kit.set_carry(block, kit::CarryKind::Load, {0,.2F,0});
        kit.set_continuous_collision(block);
    }
    (void)body(kRiggingReceiverEntity, {
        box({1.0F,.08F,1.2F}, {-.25F,2.35F,0}, Material::Steel),
        box({.1F,1.15F,.1F}, {-.95F,1.15F,-.9F}, Material::Rust),
        box({.1F,1.15F,.1F}, {.45F,1.15F,-.9F}, Material::Rust),
        box({.1F,1.15F,.1F}, {-.95F,1.15F,.9F}, Material::Rust),
        box({.1F,1.15F,.1F}, {.45F,1.15F,.9F}, Material::Rust),
        box({1.0F,.08F,.9F}, {-.25F,2.35F,-2.0F}, Material::Steel),
    }, Vec3::sZero(), 0);
    rig.add_eye(platform, {-1,.48F,.9F});
    rig.add_eye(weight, {0,.7F,.9F});
    rig.add_fixed_guide(fixed, {2.5F,4.3F,.9F}, {-3.5F,4.3F,.9F}, {2.5F,4.3F,.9F});
}
} // namespace scraperx::sim
