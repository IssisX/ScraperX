// The first slice of the second shaft. Four holds across the void off the
// 220 ring's north band, a landing bolted to the shaft's near face, and a
// short ladder up off that landing.
//
// The shaft itself stays the solid box at (-30, 800, -330), half
// (46, 800, 40). Its near face is z = -290. Nothing here is inside that
// box. The bracket that carries the landing is fat on purpose: full
// section 0.60 m, over kGripMaxSection (0.18), so it is not a hold and it
// does not reach the ground. The way onto the shaft is the hook.
//
// The drawn copies are Crossing* in main_world.gd, listed as mirrors in
// solid_export.gd. A stay or a cable is paint only.

#include "sim/physics_world.hpp"

namespace scraperx::sim {

namespace {

// A hold: the two thin full widths are 0.16 m, under kGripMaxSection.
constexpr float kHoldHalf = 0.08F;
constexpr float kBarHalfLength = 2.0F;

struct Bar final {
    float x;
    float y;
    float z;
};

// Straight-line from a chest on the north lip (about -16, 221.7, -172.5)
// to bar 1 is 25.6 m. Each later hop is 22.5 to 25.1 m. The line is 30 m
// (kHookLineMaxM). Vertical drop on a hop is at most 2.4 m.
constexpr Bar kBars[] = {
    {-18.0F, 222.0F, -198.0F},
    {-25.0F, 219.6F, -222.0F},
    {-32.0F, 217.2F, -246.0F},
    {-36.0F, 215.0F, -268.0F},
};

// Landing deck. Top at 213.85. Not a hold: the walkable widths are 3.2 m
// and 1.7 m.
constexpr float kLedgeX = -36.0F;
constexpr float kLedgeY = 213.70F;
constexpr float kLedgeZ = -285.80F;
constexpr float kLedgeHalfX = 1.60F;
constexpr float kLedgeHalfY = 0.15F;
constexpr float kLedgeHalfZ = 0.85F;

// Needle from inside the shaft face (z -290) out under the deck. Half
// 0.30 / 0.28, so the full section is 0.60 by 0.56. Not a hold.
constexpr float kBracketX = -36.0F;
constexpr float kBracketY = 213.40F;
constexpr float kBracketZ = -288.30F;
constexpr float kBracketHalfX = 0.30F;
constexpr float kBracketHalfY = 0.28F;
constexpr float kBracketHalfZ = 2.00F;

// Grab just off the landing's south lip, the side that faces the stack.
// The bar's north edge is 0.09 m clear of the slab, so it does not stub a
// foot, and the floating audit's 0.15 m mount gap still reaches the slab.
// 16.8 m from bar 4.
constexpr float kRailX = -36.0F;
constexpr float kRailY = 213.85F;
constexpr float kRailZ = -284.78F;
constexpr float kRailHalfLength = 1.20F;

// Ladder rising out of the deck. Stiles are holds. Rungs join them.
constexpr float kStileZ = -286.20F;
constexpr float kStileHalfY = 3.05F;
constexpr float kStileY = 216.65F;
constexpr float kStileXWest = -37.45F;
constexpr float kStileXEast = -34.55F;
constexpr float kRungHalfLength = 1.45F;
constexpr float kRungY[] = {215.2F, 216.6F, 218.0F};

// Cap you can stand on. Not a hold.
constexpr float kShelfX = -36.0F;
constexpr float kShelfY = 219.55F;
constexpr float kShelfZ = -286.20F;
constexpr float kShelfHalfX = 1.55F;
constexpr float kShelfHalfY = 0.12F;
constexpr float kShelfHalfZ = 0.40F;

} // namespace

void Simulation::PhysicsWorld::build_second_crossing(JPH::BodyInterface &bodies) {
    const auto put = [&](const JPH::Vec3 half, const JPH::RVec3 at) {
        machine_bodies_.push_back(add_box(bodies, half, at, JPH::EMotionType::Static,
                                          object_layers::kStatic, 0.8F,
                                          Simulation::kCrossingEntityId));
    };

    for (const Bar &bar : kBars) {
        put(JPH::Vec3(kBarHalfLength, kHoldHalf, kHoldHalf), JPH::RVec3(bar.x, bar.y, bar.z));
    }
    put(JPH::Vec3(kLedgeHalfX, kLedgeHalfY, kLedgeHalfZ),
        JPH::RVec3(kLedgeX, kLedgeY, kLedgeZ));
    put(JPH::Vec3(kBracketHalfX, kBracketHalfY, kBracketHalfZ),
        JPH::RVec3(kBracketX, kBracketY, kBracketZ));
    put(JPH::Vec3(kRailHalfLength, kHoldHalf, kHoldHalf), JPH::RVec3(kRailX, kRailY, kRailZ));
    for (const float x : {kStileXWest, kStileXEast}) {
        put(JPH::Vec3(kHoldHalf, kStileHalfY, kHoldHalf), JPH::RVec3(x, kStileY, kStileZ));
    }
    for (const float y : kRungY) {
        put(JPH::Vec3(kRungHalfLength, kHoldHalf, kHoldHalf), JPH::RVec3(kShelfX, y, kStileZ));
    }
    put(JPH::Vec3(kShelfHalfX, kShelfHalfY, kShelfHalfZ),
        JPH::RVec3(kShelfX, kShelfY, kShelfZ));
}

} // namespace scraperx::sim
