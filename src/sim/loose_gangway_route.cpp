#include "sim/loose_gangway_route.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

Part box(Vec3 half, Vec3 at, Material material) {
    Part part{half, at, Quat::sIdentity(), material};
    part.convex_radius = .003F;
    return part;
}

Part timber(Vec3 half, Vec3 at) {
    auto part = box(half, at, Material::Timber);
    // A light timber staging assembly, treated as rigid. These chosen
    // dimensions/density determine mass and compound inertia together.
    part.mass_kg = 450.0F * 8.0F * half.GetX() * half.GetY() * half.GetZ();
    return part;
}

void portable(kit::Kit &kit, std::uint64_t entity, const std::vector<Part> &parts,
              RVec3 at, Vec3 grip) {
    float mass = 0;
    for (const auto &part : parts) mass += part.mass_kg;
    const auto body = kit.add_body(entity, parts, at, Quat::sIdentity(), mass, .65F);
    // Carry handles use COM coordinates; Part coordinates use the authored
    // body origin. Keep the actual grip on the central solid crossmember.
    const Vec3 com_offset(kit.body_center_of_mass_position(body) - kit.body_position(body));
    kit.set_carry(body, kit::CarryKind::Handle, grip - com_offset);
    kit.set_continuous_collision(body);
}
} // namespace

void build_loose_gangway_route(kit::Kit &kit) {
    using namespace JPH;
    std::vector<Part> docks;
    // Two receiving plinths leave a 3.8m open span. The low yard is a
    // forgiving recovery route; the board is a player-built upper crossing.
    // Every fixed piece reaches grade, including the ordinary .18m treads.
    docks.push_back(box({1.2F, .18F, .65F}, {-3.1F, .18F, 0}, Material::Concrete));
    docks.push_back(box({.45F, .09F, .65F}, {-4.75F, .09F, 0}, Material::Concrete));
    docks.push_back(box({1.2F, .18F, .65F}, {3.1F, .18F, 0}, Material::Concrete));
    docks.push_back(box({.45F, .09F, .65F}, {4.75F, .09F, 0}, Material::Concrete));
    // A broad onward perch gives the crossing a destination and a clear
    // step-down back to the yard, without locking any of the loose cargo.
    docks.push_back(box({1.2F, .18F, 1.0F}, {3.1F, .18F, -1.65F}, Material::Concrete));
    docks.push_back(box({1.2F, .09F, .45F}, {3.1F, .09F, -3.1F}, Material::Concrete));
    (void)kit.add_body(kLooseGangwayDockEntity, docks, kLooseGangwayBay,
                       Quat::sIdentity(), 0.0F, .8F);

    std::vector<Part> board;
    board.push_back(timber({kLooseGangwayHalfLength, .009F, .4F}, {0, .069F, 0}));
    for (float z : {-.375F, .375F}) {
        board.push_back(timber({kLooseGangwayHalfLength, .06F, .025F}, {0, 0, z}));
    }
    for (float x : {-2.0F, -1.0F, 0.0F, 1.0F, 2.0F}) {
        board.push_back(timber({.025F, .05F, .35F}, {x, 0, 0}));
    }
    portable(kit, kLooseGangwayEntity, board,
             kLooseGangwayBay + Vec3(kLooseGangwayStorageX, .46F, 0), {0, .045F, 0});
    // No bearing joint: moving the crib or loading one end changes the
    // real support polygon and can spill the board onto the yard.
    const std::vector<Part> crib{timber({.2F, .2F, .4F}, Vec3::sZero())};
    portable(kit, kLooseGangwayCribEntity, crib,
             kLooseGangwayBay + Vec3(kLooseGangwayStorageX, .2F, 0), {0, .19F, 0});
    portable(kit, kLooseGangwaySpareCribEntity, crib,
             kLooseGangwayBay + Vec3(kLooseGangwayStorageX - 2.9F, .2F, 1.8F), {0, .19F, 0});
}

} // namespace scraperx::sim
