#include "sim/west_brace_bay_route.hpp"

#include <vector>

namespace scraperx::sim {
namespace {

using kit::Material;
using kit::Part;

Part span(const JPH::Vec3 low, const JPH::Vec3 high,
          const Material material) {
    return {(high - low) * 0.5F, (high + low) * 0.5F,
            JPH::Quat::sIdentity(), material};
}

Part beam_between(const JPH::Vec3 first, const JPH::Vec3 second,
                  const float half_section, const Material material) {
    const JPH::Vec3 delta = second - first;
    const float length = delta.Length();
    return {{half_section, half_section, 0.5F * length},
            0.5F * (first + second),
            JPH::Quat::sFromTo(JPH::Vec3::sAxisZ(), delta / length), material};
}

} // namespace

void build_west_brace_bay_route(kit::Kit &kit) {
    using namespace JPH;

    std::vector<Part> structure;
    structure.reserve(48);

    // The two full-height outboard columns and their crossheads disappear into
    // the real 374.25m and 396.25m Tower ring slabs. Both crossings sit beyond
    // the hand-route's z envelope, leaving the climbing lane open.
    constexpr float post_x = -25.60F;
    constexpr float post_half_x = 0.20F;
    constexpr float post_half_z = 0.20F;
    constexpr float south_post_z = -142.35F;
    constexpr float north_post_z = -135.65F;
    for (const float post_z : {south_post_z, north_post_z}) {
        structure.push_back(span(
            {post_x - post_half_x, 373.75F, post_z - post_half_z},
            {post_x + post_half_x, 396.25F, post_z + post_half_z},
            Material::Galvanised));

        // These crossheads are embedded in the existing ring slabs. They join
        // the outboard post to the Tower without adding a step to either deck.
        structure.push_back(span(
            {post_x - post_half_x, 373.75F, post_z - post_half_z},
            {-23.95F, 374.25F, post_z + post_half_z}, Material::Steel));
        structure.push_back(span(
            {post_x - post_half_x, 395.75F, post_z - post_half_z},
            {-23.95F, 396.25F, post_z + post_half_z}, Material::Steel));
    }

    // Deep diagonal bracing closes the tall frame behind the climber. Every
    // member has a 0.28m square section, so these are structural braces, not
    // accidental extra handholds or a rung ladder.
    structure.push_back(beam_between(
        {post_x, 374.15F, south_post_z}, {post_x, 385.0F, north_post_z},
        0.14F, Material::Rust));
    structure.push_back(beam_between(
        {post_x, 385.0F, south_post_z}, {post_x, 396.0F, north_post_z},
        0.14F, Material::Rust));
    structure.push_back(beam_between(
        {post_x, 374.15F, north_post_z}, {post_x, 385.0F, south_post_z},
        0.14F, Material::Rust));
    structure.push_back(beam_between(
        {post_x, 385.0F, north_post_z}, {post_x, 396.0F, south_post_z},
        0.14F, Material::Rust));

    // Two small exposed recovery shelves sit on alternating sides of the
    // climb plane. The first is inboard (+X), so the climber can pull above
    // its edge without colliding with its underside; the second is outboard
    // (-X), forcing a deliberate reversal before the final wall climb.
    // Their upper surface heights are 381.90m and 389.40m.
    structure.push_back(span({-24.18F, 381.70F, -138.34F},
                             {-22.95F, 381.90F, -137.06F},
                             Material::Galvanised));
    structure.push_back(span({-25.45F, 389.20F, -138.64F},
                             {-24.50F, 389.40F, -137.36F},
                             Material::Galvanised));

    // Rear crossmembers join the outboard shelf to the north Tower-side post.
    structure.push_back(span({post_x - 0.18F, 381.38F, -138.55F},
                             {post_x + 0.18F, 381.72F, north_post_z + 0.20F},
                             Material::Steel));
    structure.push_back(span({post_x - 0.18F, 389.08F, -138.85F},
                             {post_x + 0.18F, 389.42F, north_post_z + 0.20F},
                             Material::Steel));

    for (const float edge_z : {-138.34F, -137.06F}) {
        structure.push_back(span({post_x - 0.18F, 381.38F, edge_z - 0.14F},
                                 {-22.95F, 381.72F, edge_z + 0.14F},
                                 Material::Steel));
        // Fan the lower/upper knees away from the diagonal hand line, leaving
        // a clear capsule corridor for the physical regrips.
        const bool south_edge = edge_z < -138.0F;
        const JPH::Vec3 frame_anchor = south_edge
            ? JPH::Vec3(post_x, 378.0F, -139.98F)
            : JPH::Vec3(post_x, 378.0F, north_post_z);
        structure.push_back(beam_between(
            frame_anchor, {-23.05F, 381.56F, edge_z}, 0.14F, Material::Rust));
    }
    for (const float edge_z : {-138.64F, -137.36F}) {
        structure.push_back(span({-25.66F, 389.08F, edge_z - 0.14F},
                                 {-24.46F, 389.42F, edge_z + 0.14F},
                                 Material::Steel));
        structure.push_back(beam_between(
            {post_x, 386.90F, edge_z}, {-24.65F, 389.26F, edge_z},
            0.14F, Material::Rust));
    }

    // The first hand line rises diagonally from the lower staging floor to the
    // first shelf. It is one long, thin BoxShape, not a sequence of rungs.
    structure.push_back(beam_between(
        {-24.30F, 375.60F, -139.50F},
        {-24.30F, 382.20F, -137.50F}, 0.055F, Material::Yellow));

    // The deliberate jump-catch traverse is a real transverse hold at 384.60m.
    structure.push_back(span({-24.36F, 384.54F, -141.20F},
                             {-24.24F, 384.66F, -136.80F},
                             Material::Galvanised));

    // A low cap makes the traverse a controlled, exposed movement. Its
    // underside is 0.31m above the standing-jump capsule's nominal apex top
    // (384.34m centre + 0.90m half-height); clearance still needs route-level
    // runtime confirmation. The rear crosshead and short end brackets carry it
    // to the outboard posts without entering the player's torso corridor.
    structure.push_back(span({-25.18F, 385.55F, -141.60F},
                             {-24.55F, 385.83F, -136.40F},
                             Material::Hazard));
    structure.push_back(span({post_x - 0.18F, 385.27F, south_post_z - 0.20F},
                             {post_x + 0.18F, 385.55F, north_post_z + 0.20F},
                             Material::Steel));
    for (const float cap_edge_z : {-141.60F, -136.40F}) {
        structure.push_back(span({post_x - 0.15F, 385.27F, cap_edge_z - 0.12F},
                                 {-25.10F, 385.63F, cap_edge_z + 0.12F},
                                 Material::Steel));
    }

    // After traversing toward the opposite end, the second hold reverses the
    // z direction while rising to the upper recovery shelf.
    structure.push_back(beam_between(
        {-24.30F, 384.60F, -136.80F},
        {-24.30F, 389.50F, -138.40F}, 0.055F, Material::Hazard));

    // The final reversed diagonal reaches a genuine hand position just below
    // the existing 396.25m deck edge. It is set off along the shelf so its
    // lower bar does not cross the causal hand-to-foot transfer path. The
    // climber must move across the narrow rest before reaching it.
    structure.push_back(beam_between(
        {-24.30F, 390.50F, -137.70F},
        {-24.30F, 396.00F, -139.00F}, 0.055F, Material::Yellow));

    // Real collision-backed attachment shoes close the hand-line load paths.
    // Keep them below the receiving footing, away from the pull-up corridor.
    structure.push_back(span({-24.36F, 381.45F, -137.90F},
                             {-24.08F, 381.70F, -137.45F}, Material::Steel));
    structure.push_back(span({-24.55F, 389.05F, -138.58F},
                             {-24.25F, 389.20F, -138.14F}, Material::Steel));
    structure.push_back(span({-24.36F, 395.78F, -139.18F},
                             {-24.08F, 396.02F, -138.82F}, Material::Steel));

    // Mount the transverse bar through folded end brackets at the frame posts,
    // outside its active traverse span rather than through the climber's torso.
    structure.push_back(span({-24.44F, 384.46F, south_post_z - 0.14F},
                             {-24.16F, 384.74F, -141.15F}, Material::Steel));
    structure.push_back(span({-24.44F, 384.46F, -136.85F},
                             {-24.16F, 384.74F, north_post_z + 0.14F}, Material::Steel));
    for (const float post_z : {south_post_z, north_post_z}) {
        structure.push_back(span({post_x - 0.18F, 384.46F, post_z - 0.14F},
                                 {-24.16F, 384.74F, post_z + 0.14F}, Material::Steel));
    }
    // Carry both upper shelf knee toes into the north post behind the climb.
    structure.push_back(span({-25.78F, 386.72F, -138.85F},
                             {-25.42F, 387.08F, -135.45F}, Material::Steel));

    (void)kit.add_body(kWestBraceBayEntity, structure, RVec3::sZero(),
                       Quat::sIdentity(), 0.0F, 0.85F);
}

} // namespace scraperx::sim
