#include "sim/bands.hpp"
#include "sim/simulation.hpp"

#include <cmath>

// AS-010, the first machine of Midstack Service (Atlas band B06). Contract:
// 03_EXECUTION/ASCENT/AS-010_MIDSTACK.md.
//
// The effect this machine has to produce is a rider, starting on TP-640,
// standing on a cage floor 22 m higher, because an 800 kg skip fell the same
// 22 m. The rope is found made fast on a bollard. Until the rider takes that
// end off the bollard and hooks it to the cage, tripping the catch does not
// lift the cage. The skip survives at the bottom, so the lift can be armed
// again. A landing sits level with the cage at the top. The way onto it
// without the lift is a stack of steel billets, each one turned, each rise
// too tall to step. A ladder on the east side is only the easy backup.

namespace scraperx::sim::bands {

namespace {

using kit::Material;
using kit::Part;
using Sim = Simulation;

// Stage A's proven cage, shifted so its floor sits 0.05 m clear of TP-640
// (plate top 640.25) the way A's floor sits clear of the 154 m deck.
constexpr float kDx = 2.50F;
constexpr float kDy = 486.25F;
constexpr float kDz = -16.60F;

constexpr float kDeckTop = 154.00F + kDy;
constexpr float kCageCenterX = -10.50F + kDx;
constexpr float kCageCenterZ = -131.40F + kDz;
constexpr float kCageHalfX = 1.50F;
constexpr float kCageHalfZ = 1.40F;
constexpr float kCageFloorHalfY = 0.10F;
constexpr float kCageFloorTop = 154.25F + kDy;
constexpr float kCageOriginY = kCageFloorTop - kCageFloorHalfY;
constexpr float kCagePostHeight = 2.70F;
constexpr float kCageMassKg = 350.0F;
constexpr float kCageTravel = 22.0F;
constexpr float kCageGovernorSpeed = 2.50F;
constexpr float kCageGovernorForce = 9000.0F;
constexpr float kCageLevelAccel = 2.0F;
const JPH::Vec3 kCageEyeLocal(-kCageHalfX - 0.12F, 0.80F, 0.0F);

constexpr float kSkipCenterX = -10.50F + kDx;
constexpr float kSkipCenterZ = -135.00F + kDz;
constexpr float kSkipHalfX = 0.80F;
constexpr float kSkipHalfY = 0.90F;
constexpr float kSkipHalfZ = 0.70F;
constexpr float kSkipTopCenterY = 188.00F + kDy;
constexpr float kSkipMassKg = 800.0F;
constexpr float kSheaveY = 191.00F + kDy;

const JPH::RVec3 kLeverPivot(-11.60 + kDx, 189.80 + kDy, -133.90 + kDz);
constexpr float kLeverArm = 1.00F;
constexpr float kLeverMassKg = 60.0F;
constexpr float kLeverReleaseAngle = 0.50F;
constexpr float kLeverTravel = 1.20F;
constexpr float kGantryZ = -129.55F + kDz;
constexpr float kGantryWestX = -12.55F + kDx;
constexpr float kGantryEastX = -8.60F + kDx;
constexpr float kGantryBeamY = 156.35F + kDy;
const JPH::RVec3 kTripSheaveWest(-12.20 + kDx, 156.25 + kDy, kGantryZ);
const JPH::RVec3 kTripSheaveEast(-11.00 + kDx, 156.25 + kDy, kGantryZ);
constexpr float kTripHandleDrop = 0.75F;
constexpr float kTripHandleHalfY = 0.04F;

constexpr float kBollardX = -12.45F + kDx;
constexpr float kBollardZ = -132.30F + kDz;
constexpr float kBollardHalfXZ = 0.12F;
constexpr float kBollardSlack = 0.03F;

[[nodiscard]] std::vector<Part> cage_parts() {
    const float post_y = kCageFloorHalfY + 0.5F * kCagePostHeight;
    std::vector<Part> cage{
        {JPH::Vec3(kCageHalfX, kCageFloorHalfY, kCageHalfZ), JPH::Vec3::sZero(), JPH::Quat::sIdentity(),
         Material::Galvanised},
    };
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            cage.push_back({JPH::Vec3(0.05F, 0.5F * kCagePostHeight, 0.05F),
                            JPH::Vec3(sx * (kCageHalfX - 0.05F), post_y, sz * (kCageHalfZ - 0.05F)),
                            JPH::Quat::sIdentity(), Material::Yellow});
        }
    }
    const float top_y = kCageFloorHalfY + kCagePostHeight;
    cage.push_back({JPH::Vec3(kCageHalfX, 0.06F, 0.05F), JPH::Vec3(0.0F, top_y, kCageHalfZ - 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kCageHalfX, 0.06F, 0.05F), JPH::Vec3(0.0F, top_y, -kCageHalfZ + 0.05F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kCageHalfZ), JPH::Vec3(-kCageHalfX + 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(0.05F, 0.06F, kCageHalfZ), JPH::Vec3(kCageHalfX - 0.05F, top_y, 0.0F),
                    JPH::Quat::sIdentity(), Material::Yellow});
    cage.push_back({JPH::Vec3(kCageHalfX, 0.45F, 0.04F), JPH::Vec3(0.0F, 0.55F, -kCageHalfZ + 0.04F),
                    JPH::Quat::sIdentity(), Material::Galvanised});
    cage.push_back({JPH::Vec3(0.04F, 0.45F, kCageHalfZ), JPH::Vec3(-(kCageHalfX - 0.04F), 0.55F, 0.0F),
                    JPH::Quat::sIdentity(), Material::Galvanised});
    cage.push_back({JPH::Vec3(0.12F, 0.06F, 0.06F),
                    JPH::Vec3(-(kCageHalfX + 0.06F), kCageEyeLocal.GetY(), 0.0F), JPH::Quat::sIdentity(),
                    Material::Hazard});
    return cage;
}

// A ladder and a landing east of the cage, clear of its travel. The landing's
// top matches the cage floor at the top of the rise, so a rider can step off
// or climb the ladder and stand there without tripping the catch.
Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return {0.5F * (high - low), 0.5F * (high + low), JPH::Quat::sIdentity(), material};
}

void ladder(std::vector<Part> &parts, const float x, const float z, const float bottom, const float top) {
    constexpr float kMember = 0.03F;
    for (const float side : {-1.0F, 1.0F}) {
        const float rail_z = z + side * 0.28F;
        parts.push_back(span({x - kMember, bottom, rail_z - kMember}, {x + kMember, top, rail_z + kMember},
                             Material::Yellow));
    }
    for (float y = bottom + 0.30F; y <= top - 0.05F; y += 0.30F) {
        parts.push_back({JPH::Vec3(0.02F, 0.02F, 0.28F), JPH::Vec3(x, y, z), JPH::Quat::sIdentity(), Material::Steel});
    }
}

} // namespace

void build_midstack_service(kit::Kit &kit, MidstackService &service) {
    service.m_cage = kit.add_body(Sim::kServiceMCageEntityId, cage_parts(),
                                  JPH::RVec3(kCageCenterX, kCageOriginY, kCageCenterZ), JPH::Quat::sIdentity(),
                                  kCageMassKg, 0.9F);
    service.m_cage_guide = kit.add_guide(service.m_cage, JPH::Vec3::sAxisY(), 0.0F, kCageTravel,
                                         kCageGovernorSpeed, kCageGovernorForce, kCageLevelAccel);
    service.m_cage_anchor = kit.add_anchor(service.m_cage, kCageEyeLocal, 1.2F);

    service.m_skip = kit.add_body(
        Sim::kServiceMSkipEntityId,
        {{JPH::Vec3(kSkipHalfX, kSkipHalfY, kSkipHalfZ), JPH::Vec3::sZero(), JPH::Quat::sIdentity(), Material::Rust},
         {JPH::Vec3(0.06F, 0.12F, 0.06F), JPH::Vec3(0.0F, kSkipHalfY + 0.12F, 0.0F), JPH::Quat::sIdentity(),
          Material::Hazard}},
        JPH::RVec3(kSkipCenterX, kSkipTopCenterY, kSkipCenterZ), JPH::Quat::sIdentity(), kSkipMassKg, 0.6F);
    service.m_skip_guide = kit.add_guide(service.m_skip, JPH::Vec3::sAxisY(), -kCageTravel, 0.0F, 0.0F, 0.0F, 0.0F);

    const JPH::RVec3 eye_bottom =
        JPH::RVec3(kCageCenterX, kCageOriginY, kCageCenterZ) + JPH::RVec3(kCageEyeLocal);
    const JPH::RVec3 cage_sheave(eye_bottom.GetX(), kSheaveY, eye_bottom.GetZ());
    const JPH::RVec3 skip_sheave(kSkipCenterX, kSheaveY, kSkipCenterZ);
    const float free_length = JPH::Vec3(eye_bottom - cage_sheave).Length();
    const float bollard_offset = std::hypot(kBollardX - cage_sheave.GetX(), kBollardZ - cage_sheave.GetZ());
    const float bollard_eye_y =
        kSheaveY - std::sqrt((free_length - kBollardSlack) * (free_length - kBollardSlack) -
                             bollard_offset * bollard_offset);
    const float rail_mid = 0.5F * (kDeckTop + 179.4F + kDy);
    std::vector<Part> frame{
        {JPH::Vec3(0.20F, 0.5F * (kSheaveY + 0.6F - kDeckTop), 0.20F),
         JPH::Vec3(-13.20F + kDx, 0.5F * (kSheaveY + 0.6F + kDeckTop), -132.20F + kDz), JPH::Quat::sIdentity(),
         Material::Rust},
        {JPH::Vec3(0.15F, 0.15F, 0.40F), JPH::Vec3(-13.20F + kDx, kSheaveY + 0.40F, -131.80F + kDz),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.725F, 0.15F, 0.15F), JPH::Vec3(-12.675F + kDx, kSheaveY + 0.40F, -131.40F + kDz),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(2.15F, 0.15F, 0.15F), JPH::Vec3(-11.85F + kDx, kSheaveY + 0.40F, -133.60F + kDz),
         JPH::Quat::sRotation(JPH::Vec3::sAxisY(), 0.8034F), Material::Rust},
        {JPH::Vec3(1.00F, 0.15F, 0.15F), JPH::Vec3(kSkipCenterX, kSheaveY + 0.40F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(cage_sheave.GetX(), kSheaveY + 0.07F, cage_sheave.GetZ()),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.18F, 0.18F, 0.10F), JPH::Vec3(kSkipCenterX, kSheaveY + 0.07F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.06F, 0.5F * (kSheaveY + 0.25F - kLeverPivot.GetY()), 0.06F),
         JPH::Vec3(kLeverPivot.GetX(), 0.5F * (kSheaveY + 0.25F + kLeverPivot.GetY()), kLeverPivot.GetZ() - 0.25F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.08F, 0.08F, 0.12F),
         JPH::Vec3(kLeverPivot.GetX(), kSheaveY + 0.30F, kLeverPivot.GetZ() - 0.13F), JPH::Quat::sIdentity(),
         Material::Rust},
        {JPH::Vec3(0.04F, 0.5F * (179.4F + kDy - kDeckTop), 0.03F),
         JPH::Vec3(kCageCenterX - 1.20F, rail_mid, -132.895F + kDz), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.04F, 0.5F * (179.4F + kDy - kDeckTop), 0.03F),
         JPH::Vec3(kCageCenterX + 1.20F, rail_mid, -132.895F + kDz), JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.03F, 13.50F, 0.06F),
         JPH::Vec3(kSkipCenterX - kSkipHalfX - 0.08F, 177.60F + kDy, kSkipCenterZ), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.03F, 13.50F, 0.06F),
         JPH::Vec3(kSkipCenterX + kSkipHalfX + 0.08F, 177.60F + kDy, kSkipCenterZ), JPH::Quat::sIdentity(),
         Material::Steel},
        {JPH::Vec3(0.40F, 0.25F, 0.30F),
         JPH::Vec3(kSkipCenterX, kSkipTopCenterY - kCageTravel - kSkipHalfY - 0.25F, kSkipCenterZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(kBollardHalfXZ, 0.5F * (bollard_eye_y - 0.02F - kDeckTop), kBollardHalfXZ),
         JPH::Vec3(kBollardX, 0.5F * (bollard_eye_y - 0.02F + kDeckTop), kBollardZ), JPH::Quat::sIdentity(),
         Material::Hazard},
        {JPH::Vec3(0.05F, 0.5F * (kGantryBeamY + 0.05F - kDeckTop), 0.05F),
         JPH::Vec3(kGantryWestX, 0.5F * (kGantryBeamY + 0.05F + kDeckTop), kGantryZ), JPH::Quat::sIdentity(),
         Material::Yellow},
        {JPH::Vec3(0.05F, 0.5F * (kGantryBeamY + 0.05F - kDeckTop), 0.05F),
         JPH::Vec3(kGantryEastX, 0.5F * (kGantryBeamY + 0.05F + kDeckTop), kGantryZ), JPH::Quat::sIdentity(),
         Material::Yellow},
        {JPH::Vec3(0.5F * (kGantryEastX - kGantryWestX) + 0.05F, 0.05F, 0.05F),
         JPH::Vec3(0.5F * (kGantryEastX + kGantryWestX), kGantryBeamY, kGantryZ), JPH::Quat::sIdentity(),
         Material::Yellow},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kTripSheaveWest.GetX(), kTripSheaveWest.GetY(), kGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.06F, 0.05F, 0.03F), JPH::Vec3(kTripSheaveEast.GetX(), kTripSheaveEast.GetY(), kGantryZ),
         JPH::Quat::sIdentity(), Material::Hazard},
    };
    const kit::BodyIndex frame_body = kit.add_body(Sim::kServiceFrameEntityId, frame, JPH::RVec3::sZero(),
                                                    JPH::Quat::sIdentity(), 0.0F, 0.8F);
    service.m_bollard_anchor = kit.add_anchor(frame_body, JPH::Vec3(kBollardX, bollard_eye_y, kBollardZ), 1.2F);

    const JPH::RVec3 bail(kSkipCenterX, kSkipTopCenterY + kSkipHalfY + 0.24F, kSkipCenterZ);
    const float length = free_length + JPH::Vec3(bail - skip_sheave).Length();
    const JPH::RVec3 shackle_at(kBollardX, bollard_eye_y + 0.11F, kBollardZ);
    service.m_shackle = kit.add_body(
        Sim::kServiceMShackleEntityId,
        {{JPH::Vec3(0.09F, 0.11F, 0.05F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(), Material::Hazard}}, shackle_at,
        JPH::Quat::sIdentity(), 8.0F, 0.6F);
    kit.set_carry(service.m_shackle, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.11F, 0.0F));
    service.m_rope = kit.add_rope(service.m_skip, JPH::Vec3(0.0F, kSkipHalfY + 0.24F, 0.0F), skip_sheave,
                                  service.m_shackle, JPH::Vec3(0.0F, -0.11F, 0.0F), cage_sheave, 1.0F, length, 0.0F);
    (void)kit.hook(Sim::kServiceMShackleEntityId, service.m_bollard_anchor);

    service.m_lever_body = kit.add_body(
        Sim::kServiceMLeverEntityId,
        {{JPH::Vec3(0.5F * kLeverArm, 0.05F, 0.05F), JPH::Vec3(-0.5F * kLeverArm, 0.0F, 0.0F), JPH::Quat::sIdentity(),
          Material::Hazard},
         {JPH::Vec3(0.15F, 0.25F, 0.15F), JPH::Vec3(0.35F, 0.0F, 0.0F), JPH::Quat::sIdentity(), Material::Rust}},
        kLeverPivot, JPH::Quat::sIdentity(), kLeverMassKg, 0.5F);
    service.m_lever = kit.add_lever(service.m_lever_body, kLeverPivot, JPH::Vec3::sAxisZ(), -JPH::Vec3::sAxisX(), 0.0F,
                                    kLeverTravel);
    service.m_catch = kit.add_catch(service.m_skip, service.m_lever, kLeverReleaseAngle, 0.05F, true);
    const JPH::Vec3 handle_top(0.0F, kTripHandleHalfY, 0.0F);
    service.m_handle = kit.add_body(
        Sim::kServiceMHandleEntityId,
        {{JPH::Vec3(0.22F, kTripHandleHalfY, 0.04F), JPH::Vec3::sZero(), JPH::Quat::sIdentity(), Material::Yellow}},
        kTripSheaveEast - JPH::RVec3(0.0, kTripHandleDrop + kTripHandleHalfY, 0.0), JPH::Quat::sIdentity(), 3.0F,
        0.9F);
    kit.set_carry(service.m_handle, kit::CarryKind::Handle, handle_top);
    kit.set_damping(service.m_handle, 1.5F, 1.5F);
    (void)kit.add_trip_line(service.m_lever_body, JPH::Vec3(-kLeverArm, 0.0F, 0.0F), service.m_handle, handle_top,
                            kTripSheaveWest, kTripSheaveEast);

    const float landing_top = kCageFloorTop + kCageTravel - 0.05F;
    std::vector<Part> route;
    route.push_back(span({-6.15F, landing_top - 0.16F, kCageCenterZ - kCageHalfZ},
                         {-4.55F, landing_top, kCageCenterZ + kCageHalfZ}, Material::Timber));
    // The deck's east lip, for the ladder backup. The south lip is the last
    // pull of the billet route.
    route.push_back(span({-4.63F, landing_top - 1.20F, kCageCenterZ - kCageHalfZ},
                         {-4.47F, landing_top, kCageCenterZ + kCageHalfZ}, Material::Timber));
    route.push_back(span({-6.15F, landing_top - 1.25F, -149.62F}, {-4.55F, landing_top, -149.48F}, Material::Steel));
    // The ladder is the easy way, left for someone who does not want to work
    // out the billets. It is not the route.
    ladder(route, -4.28F, kCageCenterZ, kDeckTop, landing_top - 0.40F);

    // Steel billets, turned a quarter each time. Every rise is too tall to
    // step and too tall to vault, and none of them lines up with the one
    // under it, so the way up is a mantle, a walk to the next face, a mantle.
    // The last billet is the wide one under the landing's south lip.
    constexpr float kPlate = 640.25F;
    constexpr float kRises[] = {1.70F, 1.80F, 1.65F, 1.80F, 1.75F, 1.80F, 1.60F, 1.70F, 1.65F, 1.75F, 1.55F};
    constexpr float kSlots[4][4] = {
        {-3.50F, -1.20F, -155.60F, -153.30F},
        {-1.10F, 1.20F, -155.60F, -153.30F},
        {-1.10F, 1.20F, -153.20F, -150.90F},
        {-3.50F, -1.20F, -153.20F, -150.90F},
    };
    float top = kPlate;
    for (int i = 0; i < 11; ++i) {
        top += kRises[i];
        const float *slot = kSlots[i % 4];
        route.push_back(span({slot[0], top - (kRises[i] - 0.40F), slot[2]}, {slot[1], top, slot[3]}, Material::Steel));
    }
    route.push_back(span({-6.20F, 660.70F - 1.30F, -151.30F}, {-1.15F, 660.70F, -149.50F}, Material::Steel));
    (void)kit.add_body(Sim::kServiceRouteEntityId, route, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);
}

} // namespace scraperx::sim::bands
