#include "sim/parkour_route.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(JPH::Vec3 low, JPH::Vec3 high, Material material) {
    return {(high - low) * 0.5F, (high + low) * 0.5F,
            JPH::Quat::sIdentity(), material};
}

// As in braced_bay.cpp, the north-to-south endpoints are the walking top.
// The rotated 0.46m wide, 0.50m deep box has the same collision/render face.
Part girder(const JPH::Vec3 north, const JPH::Vec3 south) {
    const JPH::Vec3 along = south - north;
    const JPH::Quat rotation = JPH::Quat::sRotation(
        JPH::Vec3::sAxisX(), -std::atan2(along.GetY(), along.GetZ()));
    return {{.23F, .25F, .5F * along.Length()},
            .5F * (north + south) - rotation * JPH::Vec3(0, .25F, 0),
            rotation, Material::Steel};
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

void build_taper_inspection_route(kit::Kit &kit) {
    using namespace JPH;
    std::vector<Part> structure;
    const auto frame_span = [&](Vec3 low, Vec3 high, Material material) {
        structure.push_back(span(low, high, material));
    };
    const auto post = [&](float x, float z, float top) {
        frame_span({x - .18F, 352.0F, z - .18F},
                   {x + .18F, top, z + .18F}, Material::Steel);
    };

    // Brace-connection inspection lanes in the first taper. The +363m
    // landings are authored here; there is no Tower floor at that height.
    structure.push_back(girder({-24.0F, 363.0F, -163.0F},
                               {-24.0F, 352.0F, -139.5F}));
    frame_span({-26.9F, 362.72F, -165.25F},
               {-23.4F, 363.0F, -162.75F}, Material::Galvanised);
    frame_span({-21.4F, 362.72F, -165.25F},
               {-19.0F, 363.0F, -162.75F}, Material::Galvanised);
    // The exact 2m A-to-B gap remains open at landing height. The lower
    // service apron is a separate choice, not a standable gap crossmember.
    structure.push_back(girder({-20.35F, 363.0F, -163.0F},
                               {-20.35F, 374.0F, -139.5F}));
    structure.push_back(girder({-19.55F, 363.0F, -163.0F},
                               {-19.55F, 360.5F, -157.5F}));
    frame_span({-21.0F, 373.5F, -139.75F},
               {-19.6F, 374.0F, -138.6F}, Material::Steel);
    // One 0.25m step joins the real +374.25m WorldSolid51 edge at X=-21.09.
    // The existing Tower floor remains the exit's sole authority.
    frame_span({-21.09F, 373.5F, -139.73F},
               {-19.6F, 374.0F, -139.27F}, Material::Rust);

    // A real bearing header obstructs standing on the service girder;
    // its minimum chosen headroom is 1.477m for the native crouched body.
    frame_span({-20.525F, 362.75F, -159.2F},
               {-18.575F, 363.10F, -158.4F}, Material::Rust);
    for (const float x : {-20.35F, -18.75F}) {
        frame_span({x - .175F, 352.0F, -158.975F},
                   {x + .175F, 362.75F, -158.625F}, Material::Steel);
    }
    // Left bearing closes the header-to-main-girder reaction path.
    frame_span({-20.50F, 363.10F, -158.95F},
               {-20.20F, 364.45F, -158.65F}, Material::Rust);

    // Separate A/B crossheads and ring-seated posts carry each landing;
    // none crosses the jump gap. Their posts also carry the apron below.
    for (const float x : {-25.35F, -23.65F, -21.15F, -19.25F}) {
        for (const float z : {-164.2F, -163.8F}) post(x, z, 362.72F);
    }
    frame_span({-26.9F, 362.36F, -164.38F},
               {-23.4F, 362.72F, -163.62F}, Material::Steel);
    frame_span({-21.4F, 362.36F, -164.38F},
               {-19.0F, 362.72F, -163.62F}, Material::Steel);
    post(-24.0F, -151.25F, 357.05F);
    post(-20.35F, -151.25F, 368.0F);
    post(-19.45F, -138.9F, 373.5F);
    frame_span({-20.75F, 373.14F, -139.08F},
               {-19.27F, 373.5F, -138.72F}, Material::Steel);

    // Flush yellow edge splices identify the two jump faces. They are solid
    // pieces within the slabs, so markings add no invisible walking reach.
    frame_span({-23.58F, 362.84F, -165.25F},
               {-23.4F, 363.0F, -162.75F}, Material::Yellow);
    frame_span({-21.4F, 362.84F, -165.25F},
               {-21.22F, 363.0F, -162.75F}, Material::Yellow);
    (void)kit.add_body(kTaperInspectionEntity, structure,
        RVec3::sZero(), Quat::sIdentity(), 0.0F, .85F);

    std::vector<Part> recovery;
    const auto recovery_span = [&](Vec3 low, Vec3 high, Material material) {
        recovery.push_back(span(low, high, material));
    };
    const auto recovery_post = [&](float x, float z, float top) {
        recovery_span({x - .18F, 352.0F, z - .18F},
                      {x + .18F, top, z + .18F}, Material::Steel);
    };
    // Missed gap: the +360.5m apron meets the lower girder at Z=-157.6591
    // or the service girder's toe at (-19.55,-157.5). Both use real contact.
    recovery_span({-27.2F, 360.22F, -165.7F},
                  {-18.8F, 360.5F, -157.4F}, Material::Galvanised);
    recovery_span({-27.2F, 359.86F, -164.38F},
                  {-18.8F, 360.22F, -163.62F}, Material::Steel);
    recovery_span({-24.5F, 366.22F, -150.0F},
                  {-17.4F, 366.5F, -137.9F}, Material::Galvanised);
    // The catch deck stops at Z=-150: minimum chosen girder headroom is
    // 2.033m. The narrower tongue leads back to the same upper girder.
    recovery_span({-23.8F, 366.22F, -155.9F},
                  {-22.5F, 366.5F, -150.0F}, Material::Galvanised);
    recovery_span({-23.8F, 366.22F, -155.63F},
                  {-20.12F, 366.5F, -155.17F}, Material::Steel);
    // Keep the catch columns clear of the lower girder's loaded capsule.
    for (const float x : {-23.2F, -18.0F}) {
        for (const float z : {-149.6F, -138.8F}) {
            recovery_post(x, z, 366.22F);
        }
    }
    recovery_post(-23.15F, -155.6F, 366.22F);
    for (const float z : {-149.6F, -138.8F}) {
        recovery_span({-24.5F, 365.86F, z - .18F},
                      {-17.4F, 366.22F, z + .18F}, Material::Steel);
    }
    recovery_span({-23.33F, 365.86F, -155.9F},
                  {-22.97F, 366.22F, -149.6F}, Material::Steel);
    // The static Tower ring is the declared fixed foundation. These bodies
    // add no motion, route state, trigger, checkpoint or traversal override.
    (void)kit.add_body(kTaperRecoveryEntity, recovery,
        RVec3::sZero(), Quat::sIdentity(), 0.0F, .85F);
}

} // namespace scraperx::sim
