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
// again. Nothing above this landing is built yet.

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
}

} // namespace scraperx::sim::bands
