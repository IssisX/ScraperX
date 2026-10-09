#include "sim/pipe_service_route.hpp"

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
} // namespace

void build_pipe_service_route(kit::Kit &kit) {
    using namespace JPH;
    constexpr float top = kPipeServiceTopY;
    constexpr float back_z = -126.75F;
    std::vector<Part> parts;
    parts.reserve(40);
    const auto box = [&](const Vec3 low, const Vec3 high,
                         const Material material = Material::Steel) {
        parts.push_back(span(low, high, material));
    };

    // This service crossover occupies the missing 451m connection. Its
    // entire ordinary walking footprint sits above the real 440.25m north
    // roof, rather than over a new catch tray or a scripted recovery state.
    box(kPipeServiceEntryLow, kPipeServiceEntryHigh, Material::Galvanised);
    box(kPipeServiceFloorLow, kPipeServiceFloorHigh, Material::Galvanised);
    box(kPipeServiceToeLow, kPipeServiceToeHigh, Material::Galvanised);

    // Closed pipe barrel, axis local Y rotated into world Z. Its actual
    // one-metre crown obstructs the quick lane. The player supplies an
    // ordinary ballistic Jump; this module owns no vault/grip/motion grant.
    Part barrel{{kPipeServicePipeRadius, kPipeServicePipeHalfLength,
                 kPipeServicePipeRadius}, kPipeServicePipeCentre,
                Quat::sRotation(Vec3::sAxisX(), JPH_PI * .5F), Material::Rust};
    barrel.shape = Part::Shape::Cylinder;
    parts.push_back(barrel);
    // Two real blind flanges identify a stored header spool. Their faces
    // stay within its authored Z span; the small raised rim is collision
    // geometry too, lifting the highest crown by 0.03m.
    for (const float side : {-1.0F, 1.0F}) {
        Part flange{{kPipeServicePipeRadius + .03F, .035F,
                     kPipeServicePipeRadius + .03F},
                    kPipeServicePipeCentre + Vec3(0, 0,
                        side * (kPipeServicePipeHalfLength - .035F)),
                    barrel.rotation, Material::Steel};
        flange.shape = Part::Shape::Cylinder;
        parts.push_back(flange);
    }
    // Real saddles intersect the barrel's lower arc and bear on the floor.
    for (const float z : {-129.82F, -128.93F}) {
        box({-15.35F, top, z - .06F}, {-14.65F, top + .15F, z + .06F});
    }

    // The ten-metre maintenance passage is a deliberate slower choice:
    // 1.45m clear height fits the existing 1.2m crouched capsule, while the
    // existing 1.8m standing body meets the real roof. Sidewalls retain the
    // clearance choice through its length; both ends remain entirely open.
    constexpr float outer_clear = kPipeServiceShelterLaneZ -
                                  kPipeServiceShelterClearWidth * .5F;
    constexpr float inner_clear = kPipeServiceShelterLaneZ +
                                  kPipeServiceShelterClearWidth * .5F;
    constexpr float roof_y = top + kPipeServiceShelterHeadroom;
    constexpr float roof_top = roof_y + kPipeServiceShelterRoofThickness;
    box({kPipeServiceShelterStartX, roof_y, outer_clear - .15F},
        {kPipeServiceShelterEndX, roof_top, inner_clear + .15F}, Material::Yellow);
    // Real observation openings in the outer side, rather than a shader
    // hiding an opaque collision wall. A continuous 0.65m-high sill and
    // three solid wall piers support the roof around two 2m-wide openings.
    // The 0.8m vertical openings show the drop from crouched eye height;
    // they do not fit the existing 1.2m crouched capsule as a shortcut.
    box({kPipeServiceShelterStartX, top - .28F, outer_clear - .15F},
        {kPipeServiceShelterEndX, top + .65F, outer_clear});
    constexpr float wall_piers[][2] = {
        {kPipeServiceShelterStartX, -15.8F},
        {-13.8F, -10.8F},
        {-8.8F, kPipeServiceShelterEndX},
    };
    for (const auto &ends : wall_piers) {
        box({ends[0], top + .65F, outer_clear - .15F},
            {ends[1], roof_top, outer_clear});
    }
    box({kPipeServiceShelterStartX, top - .28F, inner_clear},
        {kPipeServiceShelterEndX, roof_top, inner_clear + .15F});

    // Entry shoe physically overlaps the actual lower corner column at
    // (-22.36,445.5,-127.64). Its chord is behind the lower diagonal's
    // capsule corridor; an inward Jump before the upper column earns entry.
    box({-22.55F, top - .68F, -130.4F}, {-21.95F, top - .28F, -127.6F});
    box({-22.3F, top - .68F, -130.4F}, {-19.2F, top - .28F, -129.9F});
    parts.push_back(beam_between({-22.36F, 446.0F, -127.64F},
                                {-19.6F, top - .48F, -130.2F}, .2F,
                                Material::Rust));

    // Independent suspended bearers close the load path into the real
    // upper diagonal y=451-11*x/21.905, Z=-128.095. The first bearer is at
    // X=-15: a bearer near the arrival would obstruct the LOWER diagonal's
    // standing capsule before its inward Jump. Uprights stay on the far
    // side of the upper diagonal and connectors touch below its walking top.
    const auto upper_mount = [&](const float x) {
        const float brace_x = x > 0 ? 0.0F : x;
        const float brace_y = top - 11.0F * brace_x / kPipeServiceUpperBraceRun;
        box({x - .2F, top - .68F, -130.9F},
            {x + .2F, top - .28F, back_z + .2F});
        box({x - .2F, top - .58F, back_z - .2F},
            {x + .2F, brace_y + .08F, back_z + .2F});
        parts.push_back(beam_between({brace_x, brace_y - .12F,
                                     kPipeServiceUpperBraceZ},
                                    {x, brace_y - .12F, back_z}, .2F,
                                    Material::Steel));
    };
    for (const float x : {-15.0F, -9.5F, -4.0F, .6F}) upper_mount(x);

    // Flush native splice steel marks the start/landing around the pipe
    // and the service entry. Every marking remains within real footing.
    box({-19.32F, top - .07F, -131.25F},
        {-19.2F, top, -128.85F}, Material::Yellow);
    box({-16.95F, top - .07F, -129.9F},
        {-16.83F, top, -128.85F}, Material::Yellow);
    box({-13.2F, top - .07F, -129.9F},
        {-13.08F, top, -128.85F}, Material::Yellow);
    box({-.9F, top - .07F, -131.25F},
        {-.78F, top, -127.75F}, Material::Yellow);

    (void)kit.add_body(kPipeServiceEntity, parts, RVec3::sZero(),
                       Quat::sIdentity(), 0.0F, .85F);
}

} // namespace scraperx::sim
