#include "sim/parkour_route.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(JPH::Vec3 low, JPH::Vec3 high, Material material) {
    return {(high - low) * 0.5F, (high + low) * 0.5F,
            JPH::Quat::sIdentity(), material};
}
} // namespace

void build_parkour_route(kit::Kit &kit) {
    using namespace JPH;
    const float px = static_cast<float>(kParkourSwingPivot.GetX());
    const float py = static_cast<float>(kParkourSwingPivot.GetY());
    const float pz = static_cast<float>(kParkourSwingPivot.GetZ());
    const float mount_z = -158.6F;

    std::vector<Part> frame;
    const auto frame_span = [&](Vec3 low, Vec3 high, Material material) {
        frame.push_back(span(low, high, material));
    };
    // Four solid bearing blocks leave an actual 0.15m half-width opening.
    // The 0.09m journal lies along Z; no static collider fills its bore.
    frame_span({px - .4F, py - .4F, mount_z - .25F},
               {px - .15F, py + .4F, mount_z + .25F}, Material::Steel);
    frame_span({px + .15F, py - .4F, mount_z - .25F},
               {px + .4F, py + .4F, mount_z + .25F}, Material::Steel);
    frame_span({px - .15F, py - .4F, mount_z - .25F},
               {px + .15F, py - .15F, mount_z + .25F}, Material::Steel);
    frame_span({px - .15F, py + .15F, mount_z - .25F},
               {px + .15F, py + .4F, mount_z + .25F}, Material::Steel);
    // The cantilever begins at the bearing's outer face, clear of the hole.
    // It and its post sit behind the swing plane and the +/-70 degree arm
    // envelope. The post bears on the existing +198m fixed Tower backbone.
    frame_span({px + .4F, py - .2F, mount_z - .2F},
               {-25.9F, py + .2F, mount_z + .2F}, Material::Steel);
    frame_span({-26.1F, 198.0F, mount_z - .2F},
               {-25.7F, py, mount_z + .2F}, Material::Rust);
    const auto bearing = kit.add_body(kParkourFrameEntity, frame,
        RVec3::sZero(), Quat::sIdentity(), 0.0F, .8F);

    // Chosen 40kg hollow assembly: 33kg arm, 5kg transverse hand bar and
    // 2kg journal. Constituent masses give Kit the actual COM and inertia;
    // the collision/render envelope does not imply a solid-density beam.
    Part arm{{.2F, kParkourSwingLength * .5F, .2F},
             {0, -kParkourSwingLength * .5F, 0}, Quat::sIdentity(), Material::Yellow};
    arm.mass_kg = 33.0F;
    Part hand_bar{{.06F, .06F, .7F}, {0, -kParkourSwingLength, 0},
                  Quat::sIdentity(), Material::Steel};
    hand_bar.mass_kg = 5.0F;
    Part journal{{.09F, .6F, .09F}, {0, 0, .6F},
                 Quat::sRotation(Vec3::sAxisX(), JPH_PI * .5F), Material::Galvanised};
    journal.shape = Part::Shape::Cylinder;
    journal.mass_kg = 2.0F;
    const auto hanger = kit.add_body(kParkourSwingEntity, {arm, hand_bar, journal},
        kParkourSwingPivot, Quat::sIdentity(), kParkourSwingMassKg, .8F);
    kit.set_damping(hanger, 0.0F, 0.0F);
    kit.set_continuous_collision(hanger);
    // Gravity and real player contact supply motion. The unrestricted Kit
    // hinge has only 2Nm passive bearing friction; there is no drive or latch.
    (void)kit.add_hinge(bearing, hanger, kParkourSwingPivot, Vec3::sAxisZ(), 2.0F);

    std::vector<Part> recovery;
    const auto recovery_span = [&](Vec3 low, Vec3 high, Material material) {
        recovery.push_back(span(low, high, material));
    };
    // +193.5m recovery footing stops at X=-29.5: it cannot bridge the
    // 4.5m rise to the +198m receiver. Its western posts join the underside
    // of the start platform, outside the central ladder/hand corridor.
    recovery_span({-38.8F, 193.2F, -161.8F},
                  {-29.5F, 193.5F, -157.8F}, Material::Galvanised);
    for (const float z : {-161.1F, -158.5F}) {
        recovery_span({-38.8F, 193.5F, z - .15F},
                      {-38.6F, 197.7F, z + .15F}, Material::Steel);
    }
    // Visible underfloor brackets close the recovery load path into Tower.
    // They remain below the tray, with no inclined walking connection to198.
    for (const float z : {-161.4F, -158.2F}) {
        recovery_span({-29.85F, 191.6F, z - .15F},
                      {-29.55F, 193.2F, z + .15F}, Material::Steel);
        recovery_span({-29.7F, 191.6F, z - .15F},
                      {-25.9F, 192.0F, z + .15F}, Material::Rust);
        recovery_span({-26.1F, 191.6F, z - .15F},
                      {-25.7F, 198.0F, z + .15F}, Material::Steel);
    }
    // The X=-37.4 ladder is approached facing -X from X=-37.0. Its
    // Z-transverse rungs are 0.8m wide with 0.04m half-section; the top
    // rung allows the native climb owner to top out left onto the platform.
    for (const float z : {pz - .4F, pz + .4F}) {
        recovery_span({-37.46F, 193.5F, z - .06F},
                      {-37.34F, 198.06F, z + .06F}, Material::Yellow);
    }
    for (int rung = 0; rung <= 14; ++rung) {
        const float y = 193.8F + .30F * static_cast<float>(rung);
        recovery_span({-37.44F, y - .04F, pz - .4F},
                      {-37.36F, y + .04F, pz + .4F}, Material::Steel);
    }
    // Static bolts/platform/Tower are the declared fixed foundation boundary;
    // only the hanging arm is dynamic. No reset or progress state lives here.
    (void)kit.add_body(kParkourRecoveryEntity, recovery,
        RVec3::sZero(), Quat::sIdentity(), 0.0F, .8F);
}

} // namespace scraperx::sim
