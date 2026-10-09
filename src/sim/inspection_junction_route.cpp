#include "sim/inspection_junction_route.hpp"
#include "sim/deformable_plank.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return {(high - low) * .5F, (high + low) * .5F,
            JPH::Quat::sIdentity(), material};
}

Part beam_between(const JPH::Vec3 first, const JPH::Vec3 last,
                  const float half_section, const Material material) {
    const auto delta = last - first;
    return {{half_section, half_section, delta.Length() * .5F},
            (first + last) * .5F,
            JPH::Quat::sFromTo(JPH::Vec3::sAxisZ(), delta.Normalized()), material};
}

void bolt(std::vector<Part> &parts, const JPH::Vec3 position) {
    Part head{{.055F, .02F, .055F}, position,
              JPH::Quat::sIdentity(), Material::Steel};
    head.shape = Part::Shape::Cylinder;
    parts.push_back(head);
}
} // namespace

void build_inspection_junction_route(kit::Kit &kit) {
    using namespace JPH;
    constexpr float top = kInspectionJunctionTopY;
    constexpr float walk_z = kInspectionJunctionWalkZ;
    constexpr float half_width = kInspectionJunctionWalkWidth * .5F;
    constexpr float mount_z = walk_z + 1.2F;
    const float px = static_cast<float>(kInspectionSwingPivot.GetX());
    const float py = static_cast<float>(kInspectionSwingPivot.GetY());

    std::vector<Part> structure;
    structure.reserve(40);
    const auto frame = [&](Vec3 low, Vec3 high, Material material = Material::Steel) {
        structure.push_back(span(low, high, material));
    };

    frame(kInspectionJunctionArrivalLow, kInspectionJunctionArrivalHigh,
          Material::Galvanised);
    frame({-22.0F, top - .28F, walk_z - half_width},
          {kInspectionJunctionLeftEndX, top, walk_z + half_width}, Material::Galvanised);
    frame({kInspectionJunctionRightStartX, top - .28F, walk_z - half_width},
          {-8.03F, top, walk_z + half_width}, Material::Galvanised);
    frame({-5.57F, top - .28F, walk_z - half_width},
          {1.4F, top, walk_z + half_width}, Material::Galvanised);
    frame(kInspectionJunctionReceiverLow, kInspectionJunctionReceiverHigh,
          Material::Galvanised);
    // The return brace arrives beside the swing plane, leaving the opening
    // and its catch tray clear. This widened receiver preserves X=-11.6.
    frame({-11.6F, top - .28F, -130.05F},
          {-10.2F, top, -128.0F}, Material::Galvanised);
    frame({-11.25F, top - .60F, -130.05F},
          {-10.95F, top - .28F, walk_z + half_width});

    // Each transom has its own underfloor chord. Neither a chord nor an edge
    // marking crosses the 4.4m opening at walking height.
    frame({-22.0F, top - .60F, walk_z - .16F},
          {kInspectionJunctionLeftEndX, top - .28F, walk_z + .16F});
    frame({kInspectionJunctionRightStartX, top - .60F, walk_z - .16F},
          {-8.03F, top - .28F, walk_z + .16F});
    frame({-5.57F, top - .60F, walk_z - .16F},
          {1.4F, top - .28F, walk_z + .16F});
    frame({-24.0F, top - .60F, -128.8F}, {-19.2F, top - .28F, -128.5F});
    frame({-24.0F, top - .60F, -126.8F}, {-19.2F, top - .28F, -126.5F});

    // Arrival shoe and rear brackets physically enter the existing west
    // corner column at (-24.18,401.5,-125.82). The left cantilever returns
    // its load there, below the walking face and outside the swing plane.
    frame({-24.65F, top - .72F, -126.25F}, {-23.55F, top - .28F, -125.4F});
    structure.push_back(beam_between({-23.9F, top - .55F, -125.82F},
                                      {-19.6F, top - .55F, walk_z}, .16F, Material::Steel));
    structure.push_back(beam_between({-23.9F, 403.0F, -125.82F},
                                      {-16.2F, top - .45F, mount_z}, .16F, Material::Rust));
    frame({-16.4F, top - .60F, walk_z - .16F},
          {-16.0F, top - .28F, mount_z + .16F});

    // Right-side suspension brackets attach to the actual rising 407->418
    // north brace. Its centreline is y=407-11*x/23.725, z=-126.275.
    // Thick stems stay behind the swing plane and are structural members,
    // rather than extra thin grips or a fabricated second floor.
    for (const float x : {-11.2F, -5.2F, .7F}) {
        const float brace_x = x > 0 ? 0.0F : x;
        const float brace_y = top - 11.0F * brace_x / 23.725F;
        frame({x - .16F, top - .60F, walk_z - .16F},
              {x + .16F, top - .28F, -126.115F});
        frame({x - .16F, top - .45F, -126.435F},
              {x + .16F, brace_y + .12F, -126.115F});
        if (x > 0)
            structure.push_back(beam_between({x, brace_y, -126.275F},
                                              {brace_x, brace_y, -126.275F}, .16F,
                                              Material::Steel));
    }

    // Open bearing bore: journal radius .09m, opening half-width .15m.
    // The bearing sits 1.2m behind the arm/bar plane; its short upper hanger
    // bracket ends on the existing upper diagonal, not across the walking gap.
    frame({px - .4F, py - .4F, mount_z - .25F},
          {px - .15F, py + .4F, mount_z + .25F});
    frame({px + .15F, py - .4F, mount_z - .25F},
          {px + .4F, py + .4F, mount_z + .25F});
    frame({px - .15F, py - .4F, mount_z - .25F},
          {px + .15F, py - .15F, mount_z + .25F});
    frame({px - .15F, py + .15F, mount_z - .25F},
          {px + .15F, py + .4F, mount_z + .25F});
    const float bearing_brace_y = top - 11.0F * px / 23.725F;
    frame({px - .16F, py + .4F, mount_z - .16F},
          {px + .16F, bearing_brace_y + .12F, mount_z + .16F});
    frame({px - .16F, bearing_brace_y - .16F, mount_z - .16F},
          {px + .16F, bearing_brace_y + .16F, -126.115F});

    // Flush yellow splice faces show the inspection gap without extending
    // support. Bolt heads remain flush with the actual arrival/receiver tops.
    frame({kInspectionJunctionLeftEndX - .16F, top - .08F, walk_z - half_width},
          {kInspectionJunctionLeftEndX, top, walk_z + half_width}, Material::Yellow);
    frame({kInspectionJunctionRightStartX, top - .08F, walk_z - half_width},
          {kInspectionJunctionRightStartX + .16F, top, walk_z + half_width}, Material::Yellow);
    for (const float x : {-23.65F, -19.55F})
        for (const float z : {-128.85F, -126.7F}) bolt(structure, {x, top - .02F, z});
    for (const float z : {-128.55F, -126.05F}) bolt(structure, {.95F, top - .02F, z});
    const auto bearing = kit.add_body(kInspectionFrameEntity, structure,
                                      RVec3::sZero(), Quat::sIdentity(), 0, .85F);

    // A pin and a slotted roller support the actual wood at its endpoints.
    // Forks guide roll/yaw while allowing bending about Z. Clearance around
    // the wood ends permits rotation; no steel lies under the2.4m span.
    std::vector<Part> seats;
    for (const float x : {-8.0F, -5.6F}) {
        for (const float side : {-1.0F, 1.0F}) {
            seats.push_back(span({x - .045F, top - .10F, walk_z + side * .13F - .025F},
                                 {x + .045F, top - .019F, walk_z + side * .13F + .025F},
                                 Material::Galvanised));
        }
        Part pin{{.009F, .155F, .009F}, {x, top - .019F, walk_z},
                 Quat::sRotation(Vec3::sAxisX(), JPH_PI * .5F), Material::Steel};
        pin.shape = Part::Shape::Cylinder;
        seats.push_back(pin);
        // A low outboard shoe connects each fork to real transom steel.
        const float outboard = x == -8.0F ? x - .08F : x + .08F;
        seats.push_back(span({std::min(x, outboard) - .045F, top - .18F, walk_z - .17F},
                             {std::max(x, outboard) + .045F, top - .105F, walk_z + .17F},
                             Material::Steel));
    }
    (void)kit.add_body(kDeformablePlankSeatEntity, seats, RVec3::sZero(), Quat::sIdentity(), 0, .85F);

    std::vector<Part> recovery;
    recovery.reserve(16);
    const auto catch_part = [&](Vec3 low, Vec3 high,
                                Material material = Material::Steel) {
        recovery.push_back(span(low, high, material));
    };
    catch_part(kInspectionJunctionRecoveryLow, kInspectionJunctionRecoveryHigh,
               Material::Galvanised);
    catch_part({-24.55F, 402.75F, -126.27F}, {-22.6F, 403.22F, -125.45F});
    for (const float z : {-129.75F, -126.65F}) {
        catch_part({-23.0F, 402.9F, z - .16F}, {-4.8F, 403.22F, z + .16F});
        recovery.push_back(beam_between({-23.85F, 399.8F, -125.82F},
                                        {-21.5F, 403.06F, z}, .16F, Material::Rust));
    }
    // The far shoe lands on the real lower north diagonal at x=-8.4.
    const float recovery_brace_y = 396.0F + 11.0F * 8.4F / 24.18F;
    catch_part({-8.56F, recovery_brace_y - .12F, -127.1F},
               {-8.24F, recovery_brace_y + .08F, -125.64F});
    catch_part({-8.56F, recovery_brace_y, -127.1F},
               {-8.24F, 403.22F, -126.78F});
    catch_part({-8.56F, 402.9F, -129.9F}, {-8.24F, 403.22F, -126.49F});

    // A real inspection return brace uses the same feasible walking grade
    // as the Tower diagonals. Its top runs continuously from the catch tray
    // to the receiving transom; no automatic recovery or ladder is supplied.
    const Vec3 return_low{-19.2F, 403.5F, -129.65F};
    const Vec3 return_high{-11.6F, top, -129.65F};
    const Vec3 return_delta = return_high - return_low;
    const Quat return_rotation = Quat::sRotation(Vec3::sAxisZ(),
        std::atan2(return_delta.GetY(), return_delta.GetX()));
    recovery.push_back(Part{{return_delta.Length() * .5F, .2F, .2F},
        (return_low + return_high) * .5F - return_rotation * Vec3(0, .2F, 0),
        return_rotation, Material::Steel});
    (void)kit.add_body(kInspectionRecoveryEntity, recovery,
                       RVec3::sZero(), Quat::sIdentity(), 0, .85F);

    // Same passive native primitive as Crown, shortened to 3m. The selected
    // hollow assembly masses are explicit; no source motor or scripted reset.
    Part arm{{.2F, kInspectionJunctionSwingLength * .5F, .2F},
             {0, -kInspectionJunctionSwingLength * .5F, 0},
             Quat::sIdentity(), Material::Yellow};
    arm.mass_kg = 33.0F;
    Part hand_bar{{.06F, .06F, .7F}, {0, -kInspectionJunctionSwingLength, 0},
                  Quat::sIdentity(), Material::Steel};
    hand_bar.mass_kg = 5.0F;
    Part journal{{.09F, .6F, .09F}, {0, 0, .6F},
                 Quat::sRotation(Vec3::sAxisX(), JPH_PI * .5F), Material::Galvanised};
    journal.shape = Part::Shape::Cylinder;
    journal.mass_kg = 2.0F;
    const auto hanger = kit.add_body(kInspectionSwingEntity,
        {arm, hand_bar, journal}, kInspectionSwingPivot, Quat::sIdentity(),
        kInspectionJunctionSwingMassKg, .8F);
    kit.set_damping(hanger, 0, 0);
    kit.set_continuous_collision(hanger);
    (void)kit.add_hinge(bearing, hanger, kInspectionSwingPivot,
                       Vec3::sAxisZ(), 2.0F);
}

} // namespace scraperx::sim
