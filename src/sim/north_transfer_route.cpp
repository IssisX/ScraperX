#include "sim/north_transfer_route.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(const JPH::Vec3 low, const JPH::Vec3 high,
          const Material material = Material::Steel) {
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

void bolt(std::vector<Part> &parts, const JPH::Vec3 at) {
    Part head{{.05F, .018F, .05F}, at,
              JPH::Quat::sIdentity(), Material::Steel};
    head.shape = Part::Shape::Cylinder;
    parts.push_back(head);
}
} // namespace

void build_north_transfer_route(kit::Kit &kit) {
    using namespace JPH;
    constexpr float top = kNorthTransferTopY;
    constexpr float beam_z = kNorthTransferFootBeamZ;
    constexpr float back_z = -125.95F;
    std::vector<Part> parts;
    parts.reserve(64);
    const auto box = [&](const Vec3 low, const Vec3 high,
                         const Material material = Material::Steel) {
        parts.push_back(span(low, high, material));
    };

    // Three distinct footing islands. Neither structural chords nor paint
    // extend the actual three-metre hand opening or3.2m running jump.
    box(kNorthTransferEntryLow, kNorthTransferEntryHigh, Material::Galvanised);
    box(kNorthTransferHandReceiverLow, kNorthTransferHandReceiverHigh,
        Material::Galvanised);
    box({kNorthTransferFootBeamStartX, top - .44F, beam_z - .2F},
        {kNorthTransferFootBeamEndX, top, beam_z + .2F}, Material::Galvanised);
    // This .9m-wide,1.6m-long rest permits an ordinary earned run-up after
    // the .4m balance beam; it changes geometry, never the speed controller.
    box(kNorthTransferLaunchLow, kNorthTransferLaunchHigh, Material::Galvanised);
    box(kNorthTransferGapReceiverLow, kNorthTransferGapReceiverHigh,
        Material::Galvanised);
    box({-3.6F, top - .44F, -129.35F},
        {-.8F, top, -128.95F}, Material::Galvanised);
    box(kNorthTransferToeLow, kNorthTransferToeHigh, Material::Galvanised);

    // Entry shoe physically overlaps the existing corner column centred at
    // (-23.27,423.5,-126.73), ending below its429m top. The original lower
    // brace/capsule corridor at Z=-126.73 remains clear of the arrival slab.
    box({-23.55F, top - .75F, -128.85F}, {-22.0F, top - .28F, -127.10F});
    box({-23.5F, top - .75F, -127.40F}, {-22.6F, top - .35F, -126.35F});
    box({-22.2F, top - .65F, -128.70F}, {-19.2F, top - .28F, -128.30F});
    parts.push_back(beam_between({-23.0F, 424.2F, -126.9F},
                                 {-19.5F, top - .48F, -128.6F}, .2F,
                                 Material::Rust));

    // Each receiving island is suspended independently from the ACTUAL
    // upper brace y=429-11*x/22.815,z=-127.185. Uprights sit on its far side,
    // outside both the new walking lane and the brace's own capsule sweep.
    // No continuous rear member bridges either required opening.
    const auto upper_mount = [&](const float x, const float lane_z,
                                 const float bottom_y) {
        const float brace_x = x > 0 ? 0.0F : x;
        const float brace_y = top - 11.0F * brace_x / kNorthTransferUpperBraceRun;
        box({x - .2F, bottom_y, lane_z - .2F},
            {x + .2F, bottom_y + .4F, back_z + .2F});
        box({x - .2F, bottom_y + .1F, back_z - .2F},
            {x + .2F, brace_y + .08F, back_z + .2F});
        parts.push_back(beam_between({brace_x, brace_y - .12F,
                                      kNorthTransferUpperBraceZ},
                                     {x, brace_y - .12F, back_z}, .2F,
                                     Material::Steel));
    };
    upper_mount(-15.0F, -128.6F, top - .68F);
    upper_mount(-11.8F, beam_z, top - .68F);
    upper_mount(-8.7F, beam_z, top - .68F);
    upper_mount(-4.4F, -129.15F, top - .68F);
    upper_mount(.6F, -128.6F, top - .68F);

    // Only this intended grip has two .055m minor half-extents. Its local
    // longitudinal axis is Z, rotated into world+X. A level hold uses the
    // existing lateral regrip path without competing upward-hand requests.
    parts.push_back(beam_between(kNorthTransferRailStart,
                                 kNorthTransferRailEnd,
                                 kNorthTransferRailHalfSection, Material::Steel));

    // A real header covers the bar, with underside .55m and top
    // .85m vertically above its centreline at the same X. Hanging heads fit
    // beneath it; upright/crouched footing on the bar must contact it.
    // End overhangs and mounted end plates prevent an uncovered bar tip.
    const Vec3 rail_delta = kNorthTransferRailEnd - kNorthTransferRailStart;
    const float slope = rail_delta.GetY() / rail_delta.GetX();
    const float angle = std::atan(slope);
    const float header_start_x = kNorthTransferRailStart.GetX() - .4F;
    const float header_end_x = kNorthTransferRailEnd.GetX() + .4F;
    const Vec3 header_start{header_start_x,
        kNorthTransferRailStart.GetY() - .4F * slope + .70F, -127.7F};
    const Vec3 header_end{header_end_x,
        kNorthTransferRailEnd.GetY() + .4F * slope + .70F, -127.7F};
    parts.push_back(Part{{(header_end - header_start).Length() * .5F,
                          .15F * std::cos(angle), .65F},
                         (header_start + header_end) * .5F,
                         Quat::sRotation(Vec3::sAxisZ(), angle), Material::Steel});
    // Plates meet the rear of the real grip, clear of the hanging capsule.
    box({-19.20F, 431.05F, -127.78F}, {-18.97F, 431.67F, -127.45F});
    box({-15.73F, 431.05F, -127.78F}, {-15.50F, 431.67F, -127.45F});
    upper_mount(-18.9F, -127.7F, 431.70F);
    upper_mount(-15.5F, -127.7F, 431.70F);

    // Sheltered side pocket is connected physically to the exposed beam.
    // Its1.45m clear height forces ordinary crouch; its roof and back posts
    // stay off the .9m-wide standing launch lane at Z[-128.85,-127.95].
    box(kNorthTransferCrouchRestLow, kNorthTransferCrouchRestHigh,
        Material::Galvanised);
    box({-9.8F, top - .44F, -129.05F}, {-9.2F, top, beam_z + .05F},
        Material::Galvanised);
    box({-10.6F, top + kNorthTransferCrouchHeadroom, -130.25F},
        {-8.4F, top + kNorthTransferCrouchHeadroom + .28F, -128.9F});
    for (const float x : {-10.5F, -8.5F}) {
        box({x - .2F, top - .48F, -130.5F},
            {x + .2F, top + kNorthTransferCrouchHeadroom + .28F, -130.1F});
        box({x - .2F, top - .68F, -130.5F},
            {x + .2F, top - .28F, beam_z + .2F});
    }

    // Flush splice paint marks real edges. Bolt heads belong to the actual
    // rests; no marker, bracket or unseen platform reaches across the gap.
    box({-19.35F, top - .07F, -129.2F}, {-19.2F, top, -127.1F}, Material::Yellow);
    box({-16.2F, top - .07F, -129.2F}, {-16.05F, top, -127.1F}, Material::Yellow);
    box({-8.55F, top - .07F, -128.85F}, {-8.4F, top, -127.95F}, Material::Yellow);
    box({-5.2F, top - .07F, -129.65F}, {-5.05F, top, -128.65F}, Material::Yellow);
    for (const float x : {-21.9F, -19.6F})
        for (const float z : {-128.85F, -127.45F}) bolt(parts, {x, top - .018F, z});
    for (const float x : {-15.8F, -14.6F})
        for (const float z : {-128.85F, -127.45F}) bolt(parts, {x, top - .018F, z});
    for (const float z : {-128.75F, -127.25F}) bolt(parts, {.5F, top - .018F, z});

    (void)kit.add_body(kNorthTransferEntity, parts,
                       RVec3::sZero(), Quat::sIdentity(), 0, .85F);
}

} // namespace scraperx::sim
