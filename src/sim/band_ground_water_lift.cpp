#include "sim/bands.hpp"
#include "sim/simulation.hpp"

namespace scraperx::sim::bands {
namespace {

using kit::Material;
using kit::Part;

constexpr float kCageX = -12.50F;
constexpr float kCageZ = -108.20F;
constexpr float kCageFloorHalfX = 1.35F;
constexpr float kCageFloorHalfZ = 1.25F;
constexpr float kCageFloorHalfY = 0.10F;
constexpr float kCageFloorTop = 0.25F;
constexpr float kCageOriginY = kCageFloorTop - kCageFloorHalfY;
constexpr float kCagePostHeight = 2.40F;
constexpr float kCageMassKg = 500.0F;
constexpr float kCageTravel = 8.0F;
constexpr float kCageGovernorSpeed = 2.20F;
constexpr float kCageGovernorForce = 30000.0F;
constexpr float kCageLevelAccel = 2.0F;
const JPH::Vec3 kCageEyeLocal(0.0F, 2.20F, 0.0F);

constexpr float kBucketX = -18.0F;
// The 4 m descent must clear both the upper tank and the inclined screw.
// At z=-104.90 the bucket overlapped the screw's swept assembly below it:
// release gave zero rope tension and no cage rise. North of the tank, its
// entire vertical swept volume has free clearance from the screw assembly.
constexpr float kBucketZ = -111.50F;
constexpr float kBucketCenterTopY = 4.50F;
constexpr float kBucketHalfX = 0.85F;
constexpr float kBucketHalfY = 0.45F;
constexpr float kBucketHalfZ = 0.85F;
constexpr float kBucketDryMassKg = 200.0F;
constexpr float kBucketTravel = 4.0F;
const JPH::Vec3 kBucketEyeLocal(0.0F, 0.62F, 0.0F);

constexpr float kSheaveY = 10.50F;
constexpr float kPulleyRatio = 0.50F; // 4 m bucket drop -> 8 m cage rise
constexpr float kRopeRatingN = 50000.0F;
constexpr float kDockTopY = 8.25F;

std::vector<Part> cage_parts() {
    std::vector<Part> out{{
        JPH::Vec3(kCageFloorHalfX, kCageFloorHalfY, kCageFloorHalfZ),
        JPH::Vec3::sZero(), JPH::Quat::sIdentity(), Material::Galvanised}};
    const float post_y = kCageFloorHalfY + 0.5F * kCagePostHeight;
    for (float sx : {-1.0F, 1.0F}) {
        for (float sz : {-1.0F, 1.0F}) {
            out.push_back({JPH::Vec3(0.05F, 0.5F * kCagePostHeight, 0.05F),
                           JPH::Vec3(sx * (kCageFloorHalfX - 0.05F), post_y,
                                     sz * (kCageFloorHalfZ - 0.05F)),
                           JPH::Quat::sIdentity(), Material::Yellow});
        }
    }
    const float top = kCageFloorHalfY + kCagePostHeight;
    out.push_back({JPH::Vec3(kCageFloorHalfX, 0.05F, 0.05F),
                   JPH::Vec3(0.0F, top, kCageFloorHalfZ - 0.05F),
                   JPH::Quat::sIdentity(), Material::Yellow});
    out.push_back({JPH::Vec3(kCageFloorHalfX, 0.05F, 0.05F),
                   JPH::Vec3(0.0F, top, -kCageFloorHalfZ + 0.05F),
                   JPH::Quat::sIdentity(), Material::Yellow});
    out.push_back({JPH::Vec3(0.05F, 0.45F, kCageFloorHalfZ),
                   JPH::Vec3(-kCageFloorHalfX + 0.05F, 0.55F, 0.0F),
                   JPH::Quat::sIdentity(), Material::Galvanised});
    // Leave a real central exit through the east guard at the fixed dock.
    // Two short guard sections protect the ends without walling off the
    // player's capsule when the cage is caught at +8 m.
    for (float sz : {-1.0F, 1.0F}) {
        out.push_back({JPH::Vec3(0.05F, 0.45F, 0.30F),
                       JPH::Vec3(kCageFloorHalfX - 0.05F, 0.55F, sz * 0.95F),
                       JPH::Quat::sIdentity(), Material::Galvanised});
    }
    return out;
}

std::vector<Part> bucket_parts() {
    return {
        {JPH::Vec3(kBucketHalfX, 0.08F, kBucketHalfZ),
         JPH::Vec3(0.0F, -kBucketHalfY + 0.08F, 0.0F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        {JPH::Vec3(0.08F, kBucketHalfY, kBucketHalfZ),
         JPH::Vec3(-kBucketHalfX + 0.08F, 0.0F, 0.0F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        {JPH::Vec3(0.08F, kBucketHalfY, kBucketHalfZ),
         JPH::Vec3(kBucketHalfX - 0.08F, 0.0F, 0.0F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        {JPH::Vec3(kBucketHalfX, kBucketHalfY, 0.08F),
         JPH::Vec3(0.0F, 0.0F, -kBucketHalfZ + 0.08F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        {JPH::Vec3(kBucketHalfX, kBucketHalfY, 0.08F),
         JPH::Vec3(0.0F, 0.0F, kBucketHalfZ - 0.08F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        {JPH::Vec3(0.08F, 0.10F, 0.08F), kBucketEyeLocal,
         JPH::Quat::sIdentity(), Material::Hazard},
    };
}

} // namespace

void build_ground_water_lift(kit::Kit &kit, GroundWaterLift &lift) {
    using Sim = Simulation;
    lift.cage = kit.add_body(Sim::kGroundWaterLiftCageEntityId, cage_parts(),
                             JPH::RVec3(kCageX, kCageOriginY, kCageZ),
                             JPH::Quat::sIdentity(), kCageMassKg, 0.9F);
    lift.cage_guide = kit.add_guide(lift.cage, JPH::Vec3::sAxisY(), 0.0F, kCageTravel,
                                    kCageGovernorSpeed, kCageGovernorForce, kCageLevelAccel);

    lift.bucket = kit.add_body(Sim::kGroundWaterLiftBucketEntityId, bucket_parts(),
                               JPH::RVec3(kBucketX, kBucketCenterTopY, kBucketZ),
                               JPH::Quat::sIdentity(), kBucketDryMassKg, 0.7F);
    lift.bucket_guide = kit.add_guide(lift.bucket, JPH::Vec3::sAxisY(), -kBucketTravel, 0.0F,
                                      0.0F, 0.0F, 0.0F);

    const JPH::RVec3 bucket_eye =
        JPH::RVec3(kBucketX, kBucketCenterTopY, kBucketZ) + JPH::RVec3(kBucketEyeLocal);
    const JPH::RVec3 cage_eye =
        JPH::RVec3(kCageX, kCageOriginY, kCageZ) + JPH::RVec3(kCageEyeLocal);
    const JPH::RVec3 bucket_sheave(kBucketX, kSheaveY, kBucketZ);
    const JPH::RVec3 cage_sheave(kCageX, kSheaveY, kCageZ);
    const float rope_length =
        JPH::Vec3(bucket_eye - bucket_sheave).Length() +
        kPulleyRatio * JPH::Vec3(cage_eye - cage_sheave).Length();
    lift.rope = kit.add_rope(lift.bucket, kBucketEyeLocal, bucket_sheave,
                             lift.cage, kCageEyeLocal, cage_sheave,
                             kPulleyRatio, rope_length, kRopeRatingN);

    lift.bucket_top_catch = kit.add_catch(lift.bucket, {}, 0.0F, 0.05F, true);
    // Catch seats are center-of-mass coordinates. Compound cages can have a
    // non-zero shape COM offset, so derive the +8 m seat from the actual Jolt
    // COM instead of the authored shape origin.
    const JPH::RVec3 cage_upper_seat =
        kit.body_center_of_mass_position(lift.cage) + JPH::RVec3(0.0F, kCageTravel, 0.0F);
    lift.cage_upper_catch =
        kit.add_catch_at(lift.cage, {}, cage_upper_seat, 0.0F, 0.08F, true, false);

    std::vector<Part> frame{
        // The cage needs a collision-clear swept volume for its full 8 m rise.
        // The previous two uprights sat at x +/-1.15 m inside the 2.70 m-wide
        // cage floor, so the dynamic floor had to travel through static steel.
        // Four corner posts now sit outside both floor extents and leave the
        // east-side cage-to-dock handoff open through the centre.
        {JPH::Vec3(0.10F, 5.45F, 0.10F),
         JPH::Vec3(kCageX - 1.55F, 5.45F, kCageZ - 1.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.10F, 5.45F, 0.10F),
         JPH::Vec3(kCageX - 1.55F, 5.45F, kCageZ + 1.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.10F, 5.45F, 0.10F),
         JPH::Vec3(kCageX + 1.55F, 5.45F, kCageZ - 1.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.10F, 5.45F, 0.10F),
         JPH::Vec3(kCageX + 1.55F, 5.45F, kCageZ + 1.45F),
         JPH::Quat::sIdentity(), Material::Rust},
        // At +8 m the cage roof reaches y=10.70 m. The old sheave support
        // started at y=10.59 m and physically blocked the final 0.11 m before
        // the upper catch seat. Keep the support above the complete swept roof.
        {JPH::Vec3(1.75F, 0.16F, 1.65F), JPH::Vec3(kCageX, 11.05F, kCageZ),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.08F, 5.15F, 0.08F), JPH::Vec3(kBucketX - 1.00F, 5.15F, kBucketZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(0.08F, 5.15F, 0.08F), JPH::Vec3(kBucketX + 1.00F, 5.15F, kBucketZ),
         JPH::Quat::sIdentity(), Material::Steel},
        {JPH::Vec3(1.40F, 0.16F, 1.40F), JPH::Vec3(kBucketX, kSheaveY + 0.25F, kBucketZ),
         JPH::Quat::sIdentity(), Material::Rust},
        // A rigid outlet from the raised tank ends above the caught bucket.
        // As a kit part it is both rendered and collided from the same body.
        {JPH::Vec3(0.325F, 0.175F, 1.50F), JPH::Vec3(-18.0F, 5.55F, -110.25F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        // The dock begins 0.20 m beyond the cage floor's swept east edge.
        // A flush edge met the moving cage post at y=8.03 and parted the
        // loaded rope before it could reach its upper catch.
        {JPH::Vec3(1.50F, 0.12F, 1.65F), JPH::Vec3(-9.45F, kDockTopY - 0.12F, kCageZ),
         JPH::Quat::sIdentity(), Material::Galvanised},
        // A bolted transfer span rests on the dock at its south end and on
        // MOD-STAIR-A's existing +8 m landing at its north end. The old
        // 4.35 m opening made a timed leap the only way across; a missed
        // launch dropped the player eight metres onto the lower flight.
        // The narrow grating and its two longitudinal girders are one real
        // static compound, so the visible route and Jolt contact agree.
        {JPH::Vec3(0.85F, 0.10F, 2.55F), JPH::Vec3(-8.55F, kDockTopY - 0.10F, -112.15F),
         JPH::Quat::sIdentity(), Material::Galvanised},
        {JPH::Vec3(0.09F, 0.24F, 2.55F), JPH::Vec3(-9.43F, 7.91F, -112.15F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.09F, 0.24F, 2.55F), JPH::Vec3(-7.67F, 7.91F, -112.15F),
         JPH::Quat::sIdentity(), Material::Rust},
        // A transverse stiffener joins the side girders above the grating.
        // It is a solid 0.70 m hurdle, cleared by the existing jump,
        // with a full landing length on the far side of the same span.
        {JPH::Vec3(0.82F, 0.35F, 0.14F), JPH::Vec3(-8.55F, 8.60F, -112.05F),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.14F, 4.06F, 0.14F), JPH::Vec3(-8.10F, 4.06F, kCageZ + 1.30F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.14F, 4.06F, 0.14F), JPH::Vec3(-8.10F, 4.06F, kCageZ - 1.30F),
         JPH::Quat::sIdentity(), Material::Rust},
        {JPH::Vec3(0.22F, 0.80F, 0.22F), JPH::Vec3(-14.45F, 0.80F, -107.00F),
         JPH::Quat::sIdentity(), Material::Hazard},
        {JPH::Vec3(0.22F, 0.80F, 0.22F), JPH::Vec3(-14.45F, 0.80F, -108.00F),
         JPH::Quat::sIdentity(), Material::Yellow},
        {JPH::Vec3(0.22F, 0.80F, 0.22F), JPH::Vec3(-9.45F, 9.05F, -106.85F),
         JPH::Quat::sIdentity(), Material::Yellow},
    };
    lift.frame = kit.add_body(Sim::kGroundWaterLiftFrameEntityId, frame,
                              JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.85F);
}

} // namespace scraperx::sim::bands
