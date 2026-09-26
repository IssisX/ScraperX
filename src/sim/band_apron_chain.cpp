#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>

// The apron chain. One machine on the ground, east of where you are born.
// A wedge holds a stack of pipes on a stair of concrete. Knock the wedge
// out and the pipes run into a scale. The scale's light side rises and
// kicks a beam out from under a trapdoor. The boulder on that door drops
// onto a hanging iron ball, the ball knocks down a line of slabs, and the
// last slab knocks a seesaw off its post. The long end of the seesaw is
// the deck you stand on. It rises.

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;
using Sim = Simulation;

constexpr float kOx = 78.0F;
constexpr float kOz = -8.0F;

Part box(const JPH::Vec3 half, const JPH::Vec3 at, const Material material) {
    return Part{half, at, JPH::Quat::sIdentity(), material};
}

} // namespace

void build_apron_chain(kit::Kit &kit, ApronChain &chain) {
    std::vector<Part> frame;
    // One concrete ramp, high in the west. The pipes sit on it and want to
    // run east. The wedge is the only thing in the way.
    constexpr float kRampX = kOx + 9.2F;
    constexpr float kRampY = 6.20F;
    constexpr float kRampAngle = -0.40F;
    const JPH::Quat ramp_turn = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), kRampAngle);
    Part ramp;
    ramp.half = JPH::Vec3(8.0F, 0.28F, 1.15F);
    ramp.offset = JPH::Vec3(kRampX, kRampY, kOz);
    ramp.rotation = ramp_turn;
    ramp.material = Material::Concrete;
    frame.push_back(ramp);
    // The post the seesaw's short end sits on, and the pier the hinge stands on.
    frame.push_back(box({0.30F, 1.40F, 0.30F}, {kOx + 68.70F, 1.40F, kOz}, Material::Concrete));
    frame.push_back(box({0.50F, 1.70F, 0.50F}, {kOx + 72.00F, 1.70F, kOz}, Material::Concrete));
    frame.push_back(box({1.50F, 0.24F, 1.30F}, {kOx + 40.80F, 1.70F, kOz}, Material::Steel));
    // A lane, so the ball can only go north, into the wedge. Too narrow to
    // walk past the ball.
    const JPH::Vec3 ramp_east = ramp_turn * JPH::Vec3(8.0F, 0.28F, 0.0F) + JPH::Vec3(kRampX, kRampY, kOz);
    const float lane_x = ramp_east.GetX() + 1.05F;
    frame.push_back(box({0.20F, 0.70F, 2.40F}, {lane_x - 1.25F, ramp_east.GetY() - 0.05F, kOz - 4.60F}, Material::Concrete));
    frame.push_back(box({0.20F, 0.70F, 2.40F}, {lane_x + 1.25F, ramp_east.GetY() - 0.05F, kOz - 4.60F}, Material::Concrete));
    frame.push_back(box({3.40F, 0.25F, 3.20F}, {lane_x, ramp_east.GetY() - 0.70F, kOz - 3.20F}, Material::Concrete));
    (void)kit.add_body(Sim::kApronFrameEntityId, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);

    // The wedge. It can only slide north, out of the pipes' way. The pipes
    // push east; the slide does not give in that direction.
    const float wedge_x = lane_x;
    const JPH::RVec3 wedge_at(wedge_x, ramp_east.GetY() + 0.55F, kOz);
    chain.wedge = kit.add_body(Sim::kApronWedgeEntityId,
                               {box({0.72F, 0.95F, 2.10F}, JPH::Vec3::sZero(), Material::Hazard)},
                               wedge_at, JPH::Quat::sIdentity(), 280.0F, 0.05F);
    chain.wedge_guide = kit.add_guide(chain.wedge, JPH::Vec3::sAxisZ(), 0.0F, 5.50F, 0.0F, 0.0F, 0.0F);

    chain.ball = kit.add_body(Sim::kApronBallEntityId,
                              {box({0.48F, 0.48F, 0.48F}, JPH::Vec3::sZero(), Material::Yellow)},
                              JPH::RVec3(wedge_x, ramp_east.GetY() - 0.15F, kOz - 3.40F), JPH::Quat::sIdentity(), 70.0F, 0.3F);
    kit.set_carry(chain.ball, kit::CarryKind::Load, JPH::Vec3(0.0F, 0.48F, 0.0F));

    for (int pipe = 0; pipe < 6; ++pipe) {
        const JPH::Vec3 on_ramp = ramp_turn * JPH::Vec3(-5.5F + static_cast<float>(pipe) * 1.70F, 0.70F, 0.0F);
        chain.pipe[pipe] = kit.add_body(
            Sim::kApronPipeEntityId + static_cast<std::uint64_t>(pipe),
            {box({0.38F, 0.38F, 1.05F}, JPH::Vec3::sZero(), Material::Rust)},
            JPH::RVec3(kRampX + on_ramp.GetX(), kRampY + on_ramp.GetY(), kOz), ramp_turn, 560.0F, 0.02F);
        kit.set_damping(chain.pipe[pipe], 0.05F, 0.15F);
    }

    // The scale. Built level. The block in the east basket pulls that side
    // down. Pipes in the west basket bring it back up, and the horn on the
    // east side rises into the latch.
    std::vector<Part> scale;
    scale.push_back(box({4.80F, 0.16F, 0.18F}, JPH::Vec3(0.40F, 0.0F, 0.0F), Material::Steel));
    scale.push_back(box({1.35F, 0.12F, 1.35F}, JPH::Vec3(-4.30F, -0.20F, 0.0F), Material::Steel));
    scale.push_back(box({0.12F, 0.55F, 1.35F}, JPH::Vec3(-3.05F, 0.30F, 0.0F), Material::Steel));
    scale.push_back(box({1.35F, 0.12F, 0.12F}, JPH::Vec3(-4.30F, 0.40F, 1.25F), Material::Steel));
    scale.push_back(box({1.35F, 0.12F, 0.12F}, JPH::Vec3(-4.30F, 0.40F, -1.25F), Material::Steel));
    scale.push_back(box({1.35F, 0.12F, 1.35F}, JPH::Vec3(4.30F, -0.20F, 0.0F), Material::Steel));
    scale.push_back(box({0.12F, 0.70F, 1.35F}, JPH::Vec3(5.55F, 0.40F, 0.0F), Material::Steel));
    scale.push_back(box({0.12F, 0.70F, 1.35F}, JPH::Vec3(3.05F, 0.40F, 0.0F), Material::Steel));
    scale.push_back(box({1.35F, 0.12F, 0.12F}, JPH::Vec3(4.30F, 0.40F, 1.25F), Material::Steel));
    scale.push_back(box({1.35F, 0.12F, 0.12F}, JPH::Vec3(4.30F, 0.40F, -1.25F), Material::Steel));
    Part cam;
    cam.half = JPH::Vec3(0.30F, 0.16F, 0.85F);
    cam.offset = JPH::Vec3(5.80F, 1.55F, 0.20F);
    cam.rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), -0.65F);
    cam.material = Material::Yellow;
    scale.push_back(cam);
    const JPH::Quat scale_turn = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -0.42F);
    const JPH::RVec3 scale_pivot(ramp_east.GetX() + 6.50F, ramp_east.GetY() + 1.80F, kOz);
    chain.scale = kit.add_body(Sim::kApronScaleEntityId, scale, scale_pivot, scale_turn, 700.0F, 0.7F);
    chain.scale_hinge = kit.add_lever(chain.scale, scale_pivot, JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), -0.08F, 0.85F);
    kit.set_damping(chain.scale, 0.15F, 0.40F);

    const JPH::Vec3 block_at = scale_turn * JPH::Vec3(4.30F, 0.50F, 0.0F);
    chain.block = kit.add_body(Sim::kApronBlockEntityId,
                               {box({0.70F, 0.55F, 0.70F}, JPH::Vec3::sZero(), Material::Concrete)},
                               scale_pivot + JPH::RVec3(block_at.GetX(), block_at.GetY(), block_at.GetZ()),
                               scale_turn, 2400.0F, 0.8F);

    // The latch. West end sits in the horn's path. East end holds the door.
    // Built already tipped, east end up. The horn drives the west end up,
    // and the east end drops out from under the door.
    // The beam under the door. It can only slide north. A fin on its west end
    // hangs in front of the scale's horn, and the horn is fat on its north
    // side, so as the horn rises it shoves the beam off the door.
    const JPH::RVec3 pin_at(kOx + 31.50F, 5.40F, kOz);
    chain.latch = kit.add_body(Sim::kApronLatchEntityId,
                               {box({3.20F, 0.20F, 0.40F}, JPH::Vec3::sZero(), Material::Hazard),
                                box({0.22F, 0.70F, 0.28F}, JPH::Vec3(-3.00F, -0.85F, 0.0F), Material::Hazard)},
                               pin_at, JPH::Quat::sIdentity(), 160.0F, 0.35F);
    chain.latch_guide = kit.add_guide(chain.latch, JPH::Vec3::sAxisZ(), 0.0F, 3.60F, 0.0F, 0.0F, 0.0F);

    const JPH::RVec3 door_hinge(kOx + 30.40F, 6.05F, kOz);
    chain.door = kit.add_body(Sim::kApronDoorEntityId,
                              {box({2.60F, 0.16F, 1.70F}, JPH::Vec3(2.60F, 0.0F, 0.0F), Material::Steel)},
                              door_hinge, JPH::Quat::sIdentity(), 900.0F, 0.55F);
    chain.door_hinge = kit.add_lever(chain.door, door_hinge, JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), -1.40F, 0.08F);

    chain.boulder = kit.add_body(Sim::kApronBoulderEntityId,
                                 {box({0.80F, 0.80F, 0.80F}, JPH::Vec3::sZero(), Material::Concrete)},
                                 door_hinge + JPH::RVec3(3.10, 1.10, 0.0), JPH::Quat::sIdentity(), 4200.0F, 0.7F);

    const JPH::RVec3 ball_pivot(kOx + 43.40F, 7.20F, kOz);
    chain.wreck = kit.add_body(Sim::kApronWreckEntityId,
                               {box({0.18F, 2.50F, 0.18F}, JPH::Vec3(0.0F, -2.50F, 0.0F), Material::Steel),
                                box({0.90F, 0.90F, 0.90F}, JPH::Vec3(0.0F, -5.10F, 0.0F), Material::Steel)},
                               ball_pivot, JPH::Quat::sIdentity(), 2800.0F, 0.45F);
    chain.wreck_hinge = kit.add_lever(chain.wreck, ball_pivot, JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisY(), -0.25F, 1.40F);
    kit.set_damping(chain.wreck, 0.02F, 0.05F);

    for (int slab = 0; slab < 5; ++slab) {
        const float x = kOx + 49.80F + static_cast<float>(slab) * 4.20F;
        chain.slab[slab] = kit.add_body(
            Sim::kApronSlabEntityId + static_cast<std::uint64_t>(slab),
            {box({0.62F, 3.00F, 1.40F}, JPH::Vec3::sZero(), Material::Concrete)},
            JPH::RVec3(x, 3.08F, kOz), JPH::Quat::sIdentity(), 6500.0F, 0.75F);
        kit.set_damping(chain.slab[slab], 0.02F, 0.04F);
    }

    const float rest = -0.23F;
    const JPH::RVec3 seesaw_pivot(kOx + 72.00F, 3.40F, kOz);
    chain.seesaw = kit.add_body(
        Sim::kApronSeesawEntityId,
        {box({1.50F, 1.10F, 1.50F}, JPH::Vec3(-2.40F, 0.15F, 0.0F), Material::Concrete),
         box({5.60F, 0.12F, 1.60F}, JPH::Vec3(7.40F, -0.05F, 0.0F), Material::Timber)},
        seesaw_pivot, JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), rest), 900.0F, 0.95F);
    chain.seesaw_hinge =
        kit.add_lever(chain.seesaw, seesaw_pivot, JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), -0.05F, 0.95F);
    kit.set_damping(chain.seesaw, 0.05F, 0.55F);
}

} // namespace scraperx::sim::bands
