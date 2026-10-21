#include "sim/twin_tower.hpp"

#include <cmath>
#include <vector>

namespace scraperx::sim {
TwinTower::Built TwinTower::build(kit::Kit &kit) {
    using namespace JPH;
    using kit::Material;
    using kit::Part;
    const RVec3 origin(-5.0, kDeckTop, -218.0);
    std::vector<Part> parts;
    const auto box = [&](Vec3 a, Vec3 b, Material material) {
        Part p;
        p.half = (b - a).Abs() * 0.5F;
        p.offset = (a + b) * 0.5F - Vec3(origin);
        p.material = material;
        parts.push_back(p);
    };
    const auto beam = [&](Vec3 a, Vec3 b, float width, Material material) {
        Part p;
        const Vec3 delta = b - a;
        p.half = Vec3(width * 0.5F, delta.Length() * 0.5F, width * 0.5F);
        p.offset = (a + b) * 0.5F - Vec3(origin);
        p.rotation = Quat::sFromTo(Vec3::sAxisY(), delta.Normalized());
        p.material = material;
        parts.push_back(p);
    };
    const float rail_top = kDeckTop + 1.1F;
    // A half of the crew's bridge: deck, two 3 m trussed stringers, and hand
    // rails along its sides from rail_from to rail_to (the tips stay open).
    const auto half = [&](float west, float east, float from_z, float to_z, float rail_from, float rail_to, bool deck) {
        if (deck)
            box(Vec3(west, kDeckTop - 0.3F, to_z), Vec3(east, kDeckTop, from_z), Material::Steel);
        for (const float x : {west - 0.1F, east + 0.1F}) {
            beam(Vec3(x, 217.0F, from_z), Vec3(x, 217.0F, to_z), 0.3F, Material::Rust);
            beam(Vec3(x, kDeckTop - 0.2F, from_z), Vec3(x, kDeckTop - 0.2F, to_z), 0.3F, Material::Rust);
            const int bays = static_cast<int>(std::ceil((from_z - to_z) / 6.0F));
            const float pitch = (from_z - to_z) / static_cast<float>(bays);
            for (int i = 0; i <= bays; ++i) {
                const float z = from_z - static_cast<float>(i) * pitch;
                beam(Vec3(x, 217.0F, z), Vec3(x, kDeckTop - 0.2F, z), 0.2F, Material::Rust);
                if (i < bays) {
                    beam(Vec3(x, 217.0F, z), Vec3(x, kDeckTop - 0.2F, z - pitch), 0.2F, Material::Rust);
                }
            }
            const int posts = static_cast<int>(std::ceil((rail_from - rail_to) / 5.0F));
            for (int i = 0; i <= posts; ++i) {
                const float z = rail_from - (rail_from - rail_to) * static_cast<float>(i) / static_cast<float>(posts);
                beam(Vec3(x, kDeckTop, z), Vec3(x, rail_top, z), 0.12F, Material::Galvanised);
            }
            beam(Vec3(x, rail_top, rail_from), Vec3(x, rail_top, rail_to), 0.12F, Material::Galvanised);
        }
    };

    // ---- the near half, out from the 220 ring -----------------------------
    // Seated on the ring's north band (to z -172) and hung from the 242 ring
    // above by two stays, from its tip back up to the ring's edge.
    half(kDeckWestX, kDeckEastX, -172.0F, kNearTipZ, -173.0F, kNearTipZ + 1.5F, false);
    // Its deck, with the seat the leaf's tip comes down into: the leaf lane's
    // last 1.3 m set down by the leaf's depth, so its roadway lies level.
    const float seat_back = kNearTipZ + 1.3F;
    box(Vec3(kLeafEastX + 0.1F, kDeckTop - 0.3F, kNearTipZ), Vec3(kDeckEastX, kDeckTop, -172.0F), Material::Steel);
    box(Vec3(kDeckWestX, kDeckTop - 0.3F, seat_back), Vec3(kLeafEastX + 0.1F, kDeckTop, -172.0F), Material::Steel);
    box(Vec3(kDeckWestX, kDeckTop - kLeafDepth - 0.3F, kNearTipZ), Vec3(kLeafEastX + 0.1F, kDeckTop - kLeafDepth, seat_back),
        Material::Rust);
    for (const float x : {kDeckWestX - 0.1F, kDeckEastX + 0.1F}) {
        beam(Vec3(x, rail_top, kNearTipZ + 0.6F), Vec3(x, 242.0F, -172.2F), 0.1F, Material::Galvanised);
    }

    // ---- the far half, out from the shaft ----------------------------------
    // Carried on the shaft's brackets (below) and hung from two eyes set in
    // the shaft's face 18 m up.
    half(kDeckWestX, kDeckEastX, kFarTipZ, -215.5F, kFarTipZ - 1.0F, -215.0F, false);
    // Its deck, with the leaf's heel pit cut in the west lane from the tip
    // back past the hinge.
    const float pit_back = kHingeZ - kLeafDepth - 0.2F;
    box(Vec3(kLeafEastX + 0.1F, kDeckTop - 0.3F, -215.5F), Vec3(kDeckEastX, kDeckTop, kFarTipZ), Material::Steel);
    box(Vec3(kDeckWestX, kDeckTop - 0.3F, -215.5F), Vec3(kLeafEastX + 0.1F, kDeckTop, pit_back), Material::Steel);
    for (const float x : {kDeckWestX - 0.1F, kDeckEastX + 0.1F}) {
        beam(Vec3(x, rail_top, kFarTipZ - 1.6F), Vec3(x, 238.0F, static_cast<float>(kFaceZ) - 0.3F), 0.1F,
             Material::Galvanised);
    }
    // The leaf's two bearings, either side of its heel in the pit, each on
    // a cross girder spanning under the pit's sides.
    for (const float x : {kLeafWestX - 0.2F, kLeafEastX + 0.1F}) {
        box(Vec3(x, kDeckTop - kLeafDepth - 0.3F, kHingeZ - 0.3F), Vec3(x + 0.1F, kDeckTop - 0.3F, kHingeZ + 0.3F),
            Material::Rust);
    }
    box(Vec3(kDeckWestX, kDeckTop - kLeafDepth - 0.6F, kHingeZ - 0.3F),
        Vec3(kLeafEastX + 0.2F, kDeckTop - kLeafDepth - 0.3F, kHingeZ + 0.3F), Material::Rust);
    // The erection arm: a box girder out from the shaft's face over the
    // gap's middle, on a raking strut from the face below, the yellow bar
    // across its tip (a hold: the girder itself is too deep to bite).
    const float face = static_cast<float>(kFaceZ);
    box(Vec3(kSwingX - 0.2F, kArmY - 0.6F, kArmTipZ - 0.06F), Vec3(kSwingX + 0.2F, kArmY - 0.1F, face - 0.3F),
        Material::Rust);
    beam(Vec3(kSwingX, kArmY - 12.0F, face - 0.3F), Vec3(kSwingX, kArmY - 0.6F, kArmTipZ - 12.0F), 0.3F, Material::Rust);
    box(Vec3(kSwingX - kBarHalfLength, kArmY - 0.12F, kArmTipZ - 0.12F), Vec3(kSwingX + kBarHalfLength, kArmY, kArmTipZ),
        Material::Yellow);
    // The two steps to swing from, each at its tip's edge.
    box(Vec3(kNearStepWestX, kDeckTop, kNearTipZ + 1.0F), Vec3(kNearStepWestX + 1.0F, kDeckTop + kStepHeight, kNearTipZ),
        Material::Hazard);
    box(Vec3(kFarStepWestX, kDeckTop, kFarTipZ), Vec3(kFarStepWestX + 1.0F, kDeckTop + kStepHeight, kFarTipZ - 1.0F),
        Material::Hazard);
    // The trip's hood on its post behind the leaf: back to the gap, mouth to
    // the shaft.
    const float hood_low = kFlagPivotY - 0.8F;
    const float hood_high = kFlagPivotY + 0.15F;
    const float mouth = kFlagZ - 0.1F;
    const float back = kFlagZ + 0.9F;
    box(Vec3(kFlagX - 0.15F, kDeckTop, kFlagZ + 0.25F), Vec3(kFlagX + 0.15F, hood_low, kFlagZ + 0.55F), Material::Rust);
    box(Vec3(kFlagX - 0.5F, hood_low - 0.1F, mouth), Vec3(kFlagX + 0.5F, hood_low, back + 0.1F), Material::Steel);
    box(Vec3(kFlagX - 0.5F, hood_high, mouth), Vec3(kFlagX + 0.5F, hood_high + 0.1F, back + 0.1F), Material::Steel);
    box(Vec3(kFlagX - 0.5F, hood_low, back), Vec3(kFlagX + 0.5F, hood_high, back + 0.1F), Material::Steel);
    for (const float x : {kFlagX - 0.5F, kFlagX + 0.4F})
        box(Vec3(x, hood_low, mouth), Vec3(x + 0.1F, hood_high, back), Material::Steel);

    // ---- the shaft's side: receiving platform, incline, plinth ------------
    // B's receiving platform is forward of all facade projections (the
    // ducts reach z=-220.9). No opening through the solid shaft is implied.
    box(Vec3(kDeckWestX, 219.95F, -220.5F), Vec3(-2.0F, kDeckTop, -215.5F), Material::Steel);
    // 21.8 degree service incline, then a 1.4 m mantle onto its end plinth.
    // Its 2 m tread and both landings permit an ordinary retreat as well.
    const float slope = std::atan2(4.0F, 10.0F);
    Part incline;
    incline.half = Vec3(0.5F * std::sqrt(116.0F), 0.15F, 1.0F);
    incline.rotation = Quat::sRotation(Vec3::sAxisZ(), -slope);
    incline.offset = Vec3(-12.0F, 222.25F, -218.0F) - Vec3(origin) -
                     incline.rotation * Vec3(0.0F, 0.15F, 0.0F);
    incline.material = Material::Galvanised;
    parts.push_back(incline);
    box(Vec3(-19.0F, 223.95F, -220.0F), Vec3(-17.0F, kInclineTop, -216.0F), Material::Steel);
    box(Vec3(-23.0F, 223.95F, -220.0F), Vec3(-19.0F, kLandingTop, -216.0F), Material::Rust);
    // Wall brackets reach INTO the original solid mass; their knees and
    // diagonal struts carry each deck. They stay below the walking surface.
    for (float x : {-7.0F, -3.0F, -12.0F, -18.0F, -22.0F}) {
        const float top = x < -16.0F ? kInclineTop : (x < -9.0F ? 222.25F : kDeckTop);
        beam(Vec3(x, top - 0.35F, -224.5F), Vec3(x, top - 0.35F, -216.2F), 0.3F, Material::Rust);
        beam(Vec3(x, top - 5.0F, -224.5F), Vec3(x, top - 0.35F, -216.2F), 0.3F, Material::Rust);
    }
    // A real thin square member, within existing hook eligibility, carried
    // by posts off the upper landing. The broad facade remains unhookable.
    for (float z : {-219.7F, -216.3F})
        beam(Vec3(-20.5F, kLandingTop, z), Vec3(-20.5F, 228.5F, z), 0.16F, Material::Galvanised);
    beam(Vec3(-20.5F, 228.5F, -219.7F), Vec3(-20.5F, 228.5F, -216.3F), 0.12F, Material::Yellow);

    Built built;
    built.frame = kit.add_body(kFrameEntity, parts, origin, Quat::sIdentity(), 0.0F, 0.8F);

    // ---- the leaf ----------------------------------------------------------
    // Built raised: its own y runs up the leaf from the hinge line, its
    // roadway (its -z face, kLeafDepth out from the hinge line) toward the
    // shaft, the girders and cross members between the roadway and the
    // hinge line. Turned about +x it comes down toward the near half, the
    // roadway up and level with the deck, the girders under it.
    const float centre_x = 0.5F * (kLeafWestX + kLeafEastX);
    const float half_width = 0.5F * (kLeafEastX - kLeafWestX);
    std::vector<Part> leaf;
    const auto leaf_box = [&](Vec3 half_extent, Vec3 offset, Material material) {
        Part p;
        p.half = half_extent;
        p.offset = offset;
        p.material = material;
        leaf.push_back(p);
    };
    leaf_box(Vec3(half_width, 0.5F * kLeafLength, 0.06F), Vec3(0.0F, 0.5F * kLeafLength, -kLeafDepth + 0.06F),
             Material::Steel);
    for (const float x : {-half_width + 0.1F, half_width - 0.1F})
        leaf_box(Vec3(0.1F, 0.5F * kLeafLength, 0.25F), Vec3(x, 0.5F * kLeafLength, -kLeafDepth + 0.37F), Material::Rust);
    for (float y = 1.5F; y < kLeafLength - 0.5F; y += 3.0F)
        leaf_box(Vec3(half_width, 0.08F, 0.08F), Vec3(0.0F, y, -0.08F), Material::Rust);
    leaf_box(Vec3(half_width, 0.3F, 0.02F), Vec3(0.0F, kLeafLength - 0.3F, -kLeafDepth - 0.02F), Material::Hazard);
    const RVec3 hinge_at(centre_x, kDeckTop - kLeafDepth, kHingeZ);
    built.leaf = kit.add_body(kLeafEntity, leaf, hinge_at, Quat::sRotation(Vec3::sAxisX(), kLeafLeanRad), kLeafMassKg,
                              0.8F);
    kit.set_damping(built.leaf, 0.0F, kLeafBufferPerS);
    built.hinge = kit.add_lever(built.leaf, hinge_at, Vec3::sAxisX(), Vec3::sAxisY(), 0.0F,
                                0.5F * JPH_PI - kLeafLeanRad);

    // ---- the trip flag ------------------------------------------------------
    // Hung by its top edge inside the hood's mouth. About -x, a blow from the
    // shaft's side swings it in (positive); its own weight brings it back.
    const RVec3 flag_pivot(kFlagX, kFlagPivotY, kFlagZ);
    built.flag = kit.add_body(kFlagEntity,
                              {{Vec3(kFlagHalfWidth, kFlagHalfHeight, 0.03F), Vec3(0.0F, -kFlagHalfHeight, 0.0F),
                                Quat::sIdentity(), Material::Hazard}},
                              flag_pivot, Quat::sIdentity(), kFlagMassKg, 0.5F);
    kit.set_damping(built.flag, 0.0F, 1.0F);
    built.flag_hinge = kit.add_lever(built.flag, flag_pivot, -Vec3::sAxisX(), -Vec3::sAxisY(), 0.0F, 1.2F);
    built.catch_index = kit.add_catch(built.leaf, built.flag_hinge, kFlagReleaseRad, 0.05F, false);
    return built;
}
} // namespace scraperx::sim
