// J-054, the Counterweight Crossing (#293): the bay's frame, off the 220
// ring's south face. One static body: a gangway out from the ring's edge to
// a boarding deck, an exit deck high over the gap, and the posts and head
// beam that carry the cradle's pivot. The cradle itself (cradle.hpp) hangs
// on a hinge fixed to the world at the pivot, under the head beam.
#include "sim/cradle_bay.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;

kit::Part box(Vec3 half, Vec3 offset, kit::Material material) {
    kit::Part result;
    result.half = half;
    result.offset = offset;
    result.material = material;
    return result;
}

// The frame's origin, so the parts read in world coordinates.
const RVec3 kFrameOrigin(CradleBay::kBayCenterX, 220.0, -112.0);

Vec3 local(float x, float y, float z) {
    return Vec3(x - float(kFrameOrigin.GetX()), y - float(kFrameOrigin.GetY()),
                z - float(kFrameOrigin.GetZ()));
}

std::vector<kit::Part> frame_parts() {
    std::vector<kit::Part> parts;
    constexpr float kBeamY = CradleBay::kBeamY;
    constexpr float kBoardingEastX = CradleBay::kBoardingEastX;
    constexpr float kBoardingSouthZ = CradleBay::kBoardingSouthZ;
    constexpr float kBoardingTop = CradleBay::kBoardingTop;
    constexpr float kBoardingWestX = CradleBay::kBoardingWestX;
    constexpr float kExitEastX = CradleBay::kExitEastX;
    constexpr float kExitNorthZ = CradleBay::kExitNorthZ;
    constexpr float kExitSouthZ = CradleBay::kExitSouthZ;
    constexpr float kExitTop = CradleBay::kExitTop;
    constexpr float kExitWestX = CradleBay::kExitWestX;
    constexpr float kGangwayEndZ = CradleBay::kGangwayEndZ;
    constexpr float kGangwayHalfX = CradleBay::kGangwayHalfX;
    constexpr float kPivotZ = CradleBay::kPivotZ;
    constexpr float kRingDeckTop = CradleBay::kRingDeckTop;
    constexpr float kRingFaceZ = CradleBay::kRingFaceZ;

    const float bay_x = CradleBay::kBayCenterX;
    // The gangway from the ring's edge out to the boarding deck: a
    // galvanised deck on two stringers, rails either side.
    const float gangway_mid_z = (kRingFaceZ + kGangwayEndZ) * 0.5F;
    const float gangway_half_z = (kGangwayEndZ - kRingFaceZ) * 0.5F;
    parts.push_back(box(Vec3(kGangwayHalfX, 0.06F, gangway_half_z),
                        local(bay_x, kRingDeckTop - 0.06F, gangway_mid_z), kit::Material::Galvanised));
    for (const float side : {-1.0F, 1.0F}) {
        parts.push_back(box(Vec3(0.06F, 0.18F, gangway_half_z),
                            local(bay_x + side * (kGangwayHalfX - 0.06F), kRingDeckTop - 0.30F, gangway_mid_z),
                            kit::Material::Steel));
        const float rail_x = bay_x + side * (kGangwayHalfX - 0.03F);
        parts.push_back(box(Vec3(0.03F, 0.03F, gangway_half_z),
                            local(rail_x, kRingDeckTop + 1.05F, gangway_mid_z), kit::Material::Yellow));
        for (float z = kRingFaceZ + 2.7F; z < kGangwayEndZ - 0.4F; z += 3.2F)
            parts.push_back(box(Vec3(0.03F, 0.52F, 0.03F), local(rail_x, kRingDeckTop + 0.52F, z),
                                kit::Material::Yellow));
    }
    // The boarding deck, and the exit deck high over the gap.
    const float boarding_half_x = (kBoardingEastX - kBoardingWestX) * 0.5F;
    const float boarding_mid_z = (kGangwayEndZ + kBoardingSouthZ) * 0.5F;
    const float boarding_half_z = (kBoardingSouthZ - kGangwayEndZ) * 0.5F;
    parts.push_back(box(Vec3(boarding_half_x, 0.06F, boarding_half_z),
                        local(bay_x, kBoardingTop - 0.06F, boarding_mid_z), kit::Material::Galvanised));
    for (const float side : {-1.0F, 1.0F})
        parts.push_back(box(Vec3(0.06F, 0.18F, boarding_half_z),
                            local(bay_x + side * (boarding_half_x - 0.06F), kBoardingTop - 0.30F, boarding_mid_z),
                            kit::Material::Steel));
    const float exit_half_x = (kExitEastX - kExitWestX) * 0.5F;
    const float exit_mid_z = (kExitNorthZ + kExitSouthZ) * 0.5F;
    const float exit_half_z = (kExitSouthZ - kExitNorthZ) * 0.5F;
    parts.push_back(box(Vec3(exit_half_x, 0.06F, exit_half_z), local(bay_x, kExitTop - 0.06F, exit_mid_z),
                        kit::Material::Galvanised));
    for (const float side : {-1.0F, 1.0F})
        parts.push_back(box(Vec3(0.06F, 0.18F, exit_half_z),
                            local(bay_x + side * (exit_half_x - 0.06F), kExitTop - 0.30F, exit_mid_z),
                            kit::Material::Steel));
    // The posts that carry the head beam: two off the boarding deck, two off
    // the exit deck.
    for (const float x : {kBoardingWestX, kBoardingEastX}) {
        const float half_y = (kBeamY - kBoardingTop) * 0.5F;
        parts.push_back(box(Vec3(0.12F, half_y, 0.12F),
                            local(x, kBoardingTop + half_y, boarding_mid_z), kit::Material::Steel));
    }
    for (const float x : {kExitWestX, kExitEastX}) {
        const float half_y = (kBeamY - kExitTop) * 0.5F;
        parts.push_back(box(Vec3(0.12F, half_y, 0.12F), local(x, kExitTop + half_y, exit_mid_z),
                            kit::Material::Steel));
    }
    // The head beam: two side beams along the gap, a cross beam over the
    // pivot, and a hanger down to it.
    const float beam_half_z = (kExitSouthZ - kGangwayEndZ) * 0.5F;
    const float beam_mid_z = (kGangwayEndZ + kExitSouthZ) * 0.5F;
    for (const float x : {kBoardingWestX, kBoardingEastX})
        parts.push_back(box(Vec3(0.15F, 0.25F, beam_half_z), local(x, kBeamY, beam_mid_z),
                            kit::Material::Steel));
    parts.push_back(box(Vec3(boarding_half_x + 1.0F, 0.25F, 0.15F), local(bay_x, kBeamY, kPivotZ),
                        kit::Material::Steel));
    const float hanger_half_y = (kBeamY - CradleBay::kHangerBottomY) * 0.5F;
    parts.push_back(box(Vec3(0.08F, hanger_half_y, 0.08F),
                        local(bay_x, CradleBay::kHangerBottomY + hanger_half_y, kPivotZ),
                        kit::Material::Steel));
    parts.push_back(box(Vec3(0.20F, 0.15F, 0.20F),
                        local(bay_x, CradleBay::kHangerBottomY + 0.15F, kPivotZ), kit::Material::Steel));
    return parts;
}
} // namespace

void CradleBay::build(kit::Kit &kit) {
    (void)kit.add_body(kFrameEntity, frame_parts(), kFrameOrigin, Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace scraperx::sim
