#include "sim/ground_swing_route.hpp"

#include <vector>

namespace scraperx::sim {
namespace {
using kit::Material;
using kit::Part;

Part span(JPH::Vec3 low, JPH::Vec3 high, Material material) {
    Part part{(high - low) * .5F, (high + low) * .5F,
              JPH::Quat::sIdentity(), material};
    part.convex_radius = .005F;
    return part;
}
} // namespace

void build_ground_swing_route(kit::Kit &kit) {
    using namespace JPH;
    std::vector<Part> frame;
    const auto fixed = [&](Vec3 low, Vec3 high, Material material) {
        frame.push_back(span(low, high, material));
    };
    // Both rear posts bear on concrete foundations at yard grade. The
    // central Z=-59 player lane and the transverse hand bars stay clear.
    for (const auto pivot : {kGroundSwingFirstPivot, kGroundSwingSecondPivot}) {
        const float x = static_cast<float>(pivot.GetX());
        const float y = static_cast<float>(pivot.GetY());
        const float z = static_cast<float>(pivot.GetZ());
        fixed({x - .40F, -.25F, z + 1.20F},
              {x + .40F, .10F, z + 2.00F}, Material::Concrete);
        fixed({x - .15F, .10F, z + 1.45F},
              {x + .15F, y + .50F, z + 1.75F}, Material::Rust);
        // Four blocks form a real 0.22m square journal opening. The
        // 0.15m diameter journal extends along Z into this bearing.
        fixed({x - .25F, y - .25F, z + .95F},
              {x - .11F, y + .25F, z + 1.25F}, Material::Steel);
        fixed({x + .11F, y - .25F, z + .95F},
              {x + .25F, y + .25F, z + 1.25F}, Material::Steel);
        fixed({x - .11F, y - .25F, z + .95F},
              {x + .11F, y - .11F, z + 1.25F}, Material::Steel);
        fixed({x - .11F, y + .11F, z + .95F},
              {x + .11F, y + .25F, z + 1.25F}, Material::Steel);
        // Upper cap/cantilever joins each bearing to its rear post, above
        // the journal and beyond the arm's Z envelope.
        fixed({x - .25F, y + .25F, z + .95F},
              {x + .25F, y + .50F, z + 1.75F}, Material::Yellow);
    }
    fixed({-49.25F, 4.45F, -57.55F},
          {-47.65F, 4.70F, -57.25F}, Material::Steel);

    // Three ordinary 0.15m rises lead east onto the entry plinth. All
    // footing is solid concrete down to grade, with no underfloor shortcut.
    fixed({-52.2F, 0, -59.8F}, {-51.75F, .15F, -58.2F}, Material::Concrete);
    fixed({-51.75F, 0, -59.8F}, {-51.3F, .30F, -58.2F}, Material::Concrete);
    fixed({-51.3F, 0, -59.8F}, {-49.65F, .45F, -58.2F}, Material::Concrete);
    // East rest pad is reached after the second bar; a ground-level retry
    // remains open below both hangers and around either end of the bay.
    fixed({-47.15F, 0, -59.8F}, {-45.55F, .30F, -58.2F}, Material::Concrete);
    fixed({-45.55F, 0, -59.8F}, {-45.10F, .15F, -58.2F}, Material::Concrete);
    const auto bearing = kit.add_body(kGroundSwingFrameEntity, frame,
        RVec3::sZero(), Quat::sIdentity(), 0.0F, .8F);

    for (const auto entity : {kGroundSwingFirstEntity, kGroundSwingSecondEntity}) {
        // Engineering hypothesis: a short hollow steel assembly of 12kg:
        // 5kg arm, 4kg transverse hand tube and 3kg journal. The envelope
        // is not solid steel density; these constituent masses determine
        // the real COM/inertia at Kit's existing compound-shape owner.
        Part arm{{.06F, kGroundSwingLength * .5F, .06F},
                 {0, -kGroundSwingLength * .5F, 0}, Quat::sIdentity(), Material::Yellow};
        arm.mass_kg = 5.0F;
        Part hand_bar{{.06F, .06F, .7F}, {0, -kGroundSwingLength, 0},
                      Quat::sIdentity(), Material::Steel};
        hand_bar.mass_kg = 4.0F;
        Part journal{{.075F, .60F, .075F}, {0, 0, .60F},
                     Quat::sRotation(Vec3::sAxisX(), JPH_PI * .5F), Material::Galvanised};
        journal.shape = Part::Shape::Cylinder;
        journal.mass_kg = 3.0F;
        const auto pivot = ground_swing_pivot(entity);
        const auto hanger = kit.add_body(entity, {arm, hand_bar, journal},
            pivot, Quat::sIdentity(), kGroundSwingMassKg, .8F);
        kit.set_damping(hanger, 0.0F, 0.0F);
        kit.set_continuous_collision(hanger);
        (void)kit.add_hinge(bearing, hanger, pivot, Vec3::sAxisZ(), 2.0F);
        // add_hinge normally excludes its mating bodies. Restore actual
        // collision here: the open bearing and clear rear frame need no
        // invisible exception, even under a large player-driven swing.
        kit.set_pair_collision(bearing, hanger, true);
    }
}

} // namespace scraperx::sim
