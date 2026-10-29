// The summit: a deck, a dead searchlight lit by a striker plate, and a runway
// arm (see summit.hpp). World coordinates throughout; the frame is laid out
// about kOrigin.
#include "sim/summit.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

const RVec3 kOrigin(-20.0, 800.0, -187.5);

constexpr float kDeckTop = Summit::kDeckTop;

Vec3 local(const float x, const float y, const float z) {
    return Vec3(x - float(kOrigin.GetX()), y - float(kOrigin.GetY()), z - float(kOrigin.GetZ()));
}

// A box between two corners, in world coordinates.
Part span(const Vec3 a, const Vec3 b, const Material material) {
    Part part;
    part.half = (b - a).Abs() * 0.5F;
    part.offset = local((a.GetX() + b.GetX()) * 0.5F, (a.GetY() + b.GetY()) * 0.5F, (a.GetZ() + b.GetZ()) * 0.5F);
    part.material = material;
    return part;
}

// A round member from a to b, in world coordinates.
Part member(const Vec3 a, const Vec3 b, const float radius, const Material material) {
    Part part;
    const Vec3 run = b - a;
    part.shape = Part::Shape::Cylinder;
    part.half = Vec3(radius, run.Length() * 0.5F, radius);
    part.offset = local((a.GetX() + b.GetX()) * 0.5F, (a.GetY() + b.GetY()) * 0.5F, (a.GetZ() + b.GetZ()) * 0.5F);
    part.rotation = Quat::sFromTo(Vec3::sAxisY(), run.Normalized());
    part.material = material;
    return part;
}

// A box in a moving body's own frame.
Part box(const Vec3 half, const Vec3 offset, const Material material, const float mass) {
    Part part;
    part.half = half;
    part.offset = offset;
    part.material = material;
    part.mass_kg = mass;
    return part;
}

float total_mass(const std::vector<Part> &parts) {
    float sum = 0.0F;
    for (const Part &part : parts) sum += part.mass_kg;
    return sum;
}

// A rail along x at z: posts every 3 m and two rails, `from` to `to`.
void rail_x(std::vector<Part> &parts, const float from, const float to, const float z, const float top) {
    for (float x = from; x < to + 0.01F; x += 3.0F) {
        parts.push_back(span(Vec3(x - 0.05F, kDeckTop, z - 0.05F), Vec3(x + 0.05F, top, z + 0.05F), Material::Rust));
    }
    parts.push_back(span(Vec3(from, top - 0.06F, z - 0.04F), Vec3(to, top, z + 0.04F), Material::Rust));
    parts.push_back(span(Vec3(from, kDeckTop + 0.5F, z - 0.03F), Vec3(to, kDeckTop + 0.55F, z + 0.03F), Material::Rust));
}

// A rail along z at x, from z0 to z1 (z0 < z1).
void rail_z(std::vector<Part> &parts, const float x, const float z0, const float z1, const float top) {
    for (float z = z0; z < z1 + 0.01F; z += 3.0F) {
        parts.push_back(span(Vec3(x - 0.05F, kDeckTop, z - 0.05F), Vec3(x + 0.05F, top, z + 0.05F), Material::Rust));
    }
    parts.push_back(span(Vec3(x - 0.04F, top - 0.06F, z0), Vec3(x + 0.04F, top, z1), Material::Rust));
    parts.push_back(span(Vec3(x - 0.03F, kDeckTop + 0.5F, z0), Vec3(x + 0.03F, kDeckTop + 0.55F, z1),
                         Material::Rust));
}

std::vector<Part> frame_parts() {
    constexpr float kD = kDeckTop;
    constexpr float kWest = Summit::kDeckWestX;
    constexpr float kEast = Summit::kDeckEastX;
    constexpr float kNorth = Summit::kDeckNorthZ;
    constexpr float kSouth = Summit::kDeckSouthZ;
    std::vector<Part> parts;

    // ---- the deck, over two long girders and cross ribs ---------------------
    parts.push_back(span(Vec3(kWest, kD - 0.5F, kNorth), Vec3(kEast, kD, kSouth), Material::Rust));
    parts.push_back(span(Vec3(kWest, kD - 1.8F, kNorth + 0.4F), Vec3(kEast, kD - 0.5F, kNorth + 0.8F), Material::Steel));
    parts.push_back(span(Vec3(kWest, kD - 1.8F, kSouth - 0.8F), Vec3(kEast, kD - 0.5F, kSouth - 0.4F), Material::Steel));
    for (float x = kWest + 1.5F; x < kEast; x += 4.0F) {
        parts.push_back(span(Vec3(x - 0.15F, kD - 1.3F, kNorth + 0.4F), Vec3(x + 0.15F, kD - 0.5F, kSouth - 0.4F),
                             Material::Steel));
    }

    // ---- the south rail, clear of the derrick's cradle, and the north rail ---
    rail_x(parts, kWest, kEast - 0.1F, kSouth - 0.05F, kD + 1.1F);
    rail_x(parts, -3.0F, kEast - 0.1F, kNorth + 0.05F, kD + 1.1F);
    // The west end closes either side of the arm's root.
    rail_z(parts, kWest + 0.05F, kNorth, Summit::kArmCentreZ - Summit::kArmHalfWidth - 0.2F, kD + 1.1F);
    rail_z(parts, kWest + 0.05F, Summit::kArmCentreZ + Summit::kArmHalfWidth + 0.2F, kSouth, kD + 1.1F);

    // ---- the arm: a deck, kerbs, and a girder deep at the root ------------------
    const float az0 = Summit::kArmCentreZ - Summit::kArmHalfWidth;
    const float az1 = Summit::kArmCentreZ + Summit::kArmHalfWidth;
    parts.push_back(span(Vec3(Summit::kArmEndX, kD - 0.5F, az0), Vec3(kWest, kD, az1), Material::Rust));
    parts.push_back(span(Vec3(Summit::kArmEndX, kD, az0), Vec3(kWest, kD + 0.12F, az0 + 0.25F), Material::Yellow));
    parts.push_back(span(Vec3(Summit::kArmEndX, kD, az1 - 0.25F), Vec3(kWest, kD + 0.12F, az1), Material::Yellow));
    const float girder_z0 = Summit::kArmCentreZ - 0.5F;
    const float girder_z1 = Summit::kArmCentreZ + 0.5F;
    parts.push_back(span(Vec3(-24.0F, kD - 3.2F, girder_z0), Vec3(kWest, kD - 0.5F, girder_z1), Material::Steel));
    parts.push_back(span(Vec3(-36.0F, kD - 2.0F, girder_z0), Vec3(-24.0F, kD - 0.5F, girder_z1), Material::Steel));
    parts.push_back(span(Vec3(Summit::kArmEndX, kD - 1.1F, girder_z0), Vec3(-36.0F, kD - 0.5F, girder_z1), Material::Steel));
    // The end of the arm: a yellow-and-black lip across the deck's face.
    parts.push_back(span(Vec3(Summit::kArmEndX, kD - 0.5F, az0), Vec3(Summit::kArmEndX + 0.5F, kD, az1), Material::Hazard));
    // The windsock's pole, at the arm's north corner at its end.
    parts.push_back(member(Vec3(Summit::kArmEndX + 0.9F, kD, az0 + 0.12F),
                           Vec3(Summit::kArmEndX + 0.9F, kD + 7.0F, az0 + 0.12F), 0.09F, Material::Steel));
    parts.push_back(span(Vec3(Summit::kArmEndX + 0.6F, kD, az0), Vec3(Summit::kArmEndX + 1.2F, kD + 0.3F, az0 + 0.3F),
                         Material::Steel));

    // ---- the lamp's pylon: a lattice mast, four legs, rings and cross braces ------
    const float px = Summit::kPylonX;
    const float pz = Summit::kPylonZ;
    const float top = Summit::kTurntableY;
    const float h = 1.6F;
    constexpr float kBay = 3.0F;
    const Vec3 corner[4] = {Vec3(px - h, kD, pz - h), Vec3(px + h, kD, pz - h), Vec3(px + h, kD, pz + h),
                            Vec3(px - h, kD, pz + h)};
    for (int i = 0; i < 4; ++i) {
        parts.push_back(member(corner[i], Vec3(corner[i].GetX(), top - 0.6F, corner[i].GetZ()), 0.22F, Material::Steel));
    }
    const int bays = static_cast<int>((top - kD - 0.6F) / kBay);
    for (int level = 0; level < bays; ++level) {
        const float y0 = kD + kBay * static_cast<float>(level);
        const float y1 = y0 + kBay;
        for (int i = 0; i < 4; ++i) {
            const Vec3 a = corner[i];
            const Vec3 b = corner[(i + 1) % 4];
            // One diagonal in each face, alternating the way it leans.
            const bool lean = ((level + i) % 2) == 0;
            parts.push_back(member(Vec3(lean ? a.GetX() : b.GetX(), y0, lean ? a.GetZ() : b.GetZ()),
                                   Vec3(lean ? b.GetX() : a.GetX(), y1, lean ? b.GetZ() : a.GetZ()), 0.08F,
                                   Material::Galvanised));
        }
        parts.push_back(span(Vec3(px - h - 0.1F, y1 - 0.08F, pz - h - 0.1F), Vec3(px + h + 0.1F, y1 + 0.08F, pz + h + 0.1F),
                             Material::Rust));
    }
    // The turntable's seat, and a service platform round it.
    parts.push_back(span(Vec3(px - 2.2F, top - 0.6F, pz - 2.2F), Vec3(px + 2.2F, top, pz + 2.2F), Material::Steel));

    // ---- the gantry the strike is made from, and the blocks up to it -----------
    const float gz0 = Summit::kStrikerNorthZ - 0.9F;
    const float gz1 = Summit::kStrikerSouthZ + 0.9F;
    parts.push_back(span(Vec3(Summit::kGantryWestX, Summit::kGantryTop - 0.5F, gz0),
                         Vec3(Summit::kGantryEastX, Summit::kGantryTop, gz1), Material::Rust));
    parts.push_back(span(Vec3(Summit::kGantryEastX - 0.3F, Summit::kGantryTop, gz0),
                         Vec3(Summit::kGantryEastX, Summit::kGantryTop + 0.1F, gz1), Material::Yellow));
    // The gantry's west end is a solid pier (block two's wall to mantle), its
    // east end stands on two legs.
    parts.push_back(span(Vec3(Summit::kGantryWestX, kD, gz0), Vec3(Summit::kGantryWestX + 1.2F, Summit::kGantryTop - 0.5F, gz1),
                         Material::Concrete));
    for (const float z : {gz0 + 0.2F, gz1 - 0.2F}) {
        parts.push_back(member(Vec3(Summit::kGantryEastX - 0.2F, kD, z),
                               Vec3(Summit::kGantryEastX - 0.2F, Summit::kGantryTop - 0.5F, z), 0.12F, Material::Steel));
    }
    parts.push_back(span(Vec3(Summit::kGantryWestX + 1.2F, Summit::kGantryTop - 2.3F, gz0 + 0.1F),
                         Vec3(Summit::kGantryEastX, Summit::kGantryTop - 0.5F, gz0 + 0.3F), Material::Steel));
    parts.push_back(span(Vec3(Summit::kGantryWestX + 1.2F, Summit::kGantryTop - 2.3F, gz1 - 0.3F),
                         Vec3(Summit::kGantryEastX, Summit::kGantryTop - 0.5F, gz1 - 0.1F), Material::Steel));
    // Block one, block two: each 1.6 m over what is before it.
    parts.push_back(span(Vec3(-4.0F, kD, Summit::kStrikerNorthZ), Vec3(-2.4F, Summit::kBlockOneTop, Summit::kStrikerSouthZ),
                         Material::Concrete));
    parts.push_back(span(Vec3(-2.4F, kD, Summit::kStrikerNorthZ), Vec3(Summit::kGantryWestX, Summit::kBlockTwoTop,
                                                                      Summit::kStrikerSouthZ),
                         Material::Concrete));
    // Their yellow lips.
    parts.push_back(span(Vec3(-4.0F, Summit::kBlockOneTop - 0.06F, Summit::kStrikerNorthZ),
                         Vec3(-2.4F, Summit::kBlockOneTop + 0.02F, Summit::kStrikerSouthZ), Material::Yellow));
    parts.push_back(span(Vec3(-2.4F, Summit::kBlockTwoTop - 0.06F, Summit::kStrikerNorthZ),
                         Vec3(Summit::kGantryWestX, Summit::kBlockTwoTop + 0.02F, Summit::kStrikerSouthZ), Material::Yellow));

    // ---- the striker's four guides, and the spring cans under the plate ------------
    const float cx = 0.5F * (Summit::kStrikerWestX + Summit::kStrikerEastX);
    const float cz = 0.5F * (Summit::kStrikerNorthZ + Summit::kStrikerSouthZ);
    const float hx = 0.5F * (Summit::kStrikerEastX - Summit::kStrikerWestX);
    const float hz = 0.5F * (Summit::kStrikerSouthZ - Summit::kStrikerNorthZ);
    for (const float sx : {-1.0F, 1.0F}) {
        for (const float sz : {-1.0F, 1.0F}) {
            parts.push_back(member(Vec3(cx + sx * (hx + 0.2F), kD, cz + sz * (hz + 0.2F)),
                                   Vec3(cx + sx * (hx + 0.2F), kD + 1.7F, cz + sz * (hz + 0.2F)), 0.12F, Material::Steel));
            parts.push_back(member(Vec3(cx + sx * (hx - 0.6F), kD, cz + sz * (hz - 0.6F)),
                                   Vec3(cx + sx * (hx - 0.6F), kD + 0.08F, cz + sz * (hz - 0.6F)), 0.28F, Material::Galvanised));
        }
    }
    return parts;
}

// The plate about its centre at rest: a cast pad with a ribbed underside.
std::vector<Part> striker_parts() {
    const float half_x = 0.5F * (Summit::kStrikerEastX - Summit::kStrikerWestX);
    const float half_z = 0.5F * (Summit::kStrikerSouthZ - Summit::kStrikerNorthZ);
    std::vector<Part> parts;
    parts.push_back(box(Vec3(half_x, 0.1F, half_z), Vec3(0.0F, -0.1F, 0.0F), Material::Hazard,
                        Summit::kStrikerMassKg - 60.0F));
    parts.push_back(box(Vec3(half_x - 0.1F, 0.1F, 0.12F), Vec3(0.0F, -0.3F, 0.0F), Material::Steel, 30.0F));
    parts.push_back(box(Vec3(0.12F, 0.1F, half_z - 0.1F), Vec3(0.0F, -0.3F, 0.0F), Material::Steel, 30.0F));
    return parts;
}

// The lamp: a turntable, its yoke, the drum on it pointing out along local +x
// and tilted up, a lens, and a weight behind. A drum 6.4 m across.
std::vector<Part> lamp_parts() {
    constexpr float kTilt = 0.6F;   // the beam points 34 degrees up: it climbs over the tower
    constexpr float kRadius = 3.2F;
    constexpr float kHalfLength = 4.6F;
    const Vec3 centre(1.0F, 7.0F, 0.0F);
    std::vector<Part> parts;
    Part table;
    table.shape = Part::Shape::Cylinder;
    table.half = Vec3(3.0F, 0.3F, 3.0F);
    table.offset = Vec3(0.0F, 0.3F, 0.0F);
    table.material = Material::Steel;
    table.mass_kg = 900.0F;
    parts.push_back(table);
    for (const float z : {-3.6F, 3.6F}) {
        parts.push_back(box(Vec3(0.35F, 3.4F, 0.25F), Vec3(0.6F, 3.8F, z), Material::Steel, 220.0F));
    }
    Part drum;
    drum.shape = Part::Shape::Cylinder;
    drum.half = Vec3(kRadius, kHalfLength, kRadius);
    drum.offset = centre;
    drum.rotation = Quat::sRotation(Vec3::sAxisZ(), -0.5F * JPH_PI + kTilt);
    drum.material = Material::Galvanised;
    drum.mass_kg = 1800.0F;
    parts.push_back(drum);
    Part lens = drum;
    lens.half = Vec3(kRadius - 0.15F, 0.12F, kRadius - 0.15F);
    lens.offset = centre + Vec3(std::cos(kTilt), std::sin(kTilt), 0.0F) * (kHalfLength + 0.1F);
    lens.material = Material::Yellow;
    lens.mass_kg = 100.0F;
    parts.push_back(lens);
    parts.push_back(box(Vec3(1.6F, 1.6F, 1.6F), Vec3(-5.4F, 4.0F, 0.0F), Material::Rust, 1800.0F));
    parts.push_back(box(Vec3(2.6F, 0.15F, 0.15F), Vec3(-2.6F, 3.4F, 0.0F), Material::Steel, 80.0F));
    return parts;
}
} // namespace

Summit::Summit(PhysicsSystem &system, kit::Kit &kit) : system_(system), kit_(kit) {
    frame_ = kit_.add_body(kFrameEntity, frame_parts(), kOrigin, Quat::sIdentity(), 0.0F, 0.8F);

    // The plate on its four guides: one slider, down along -y from its upper stop.
    const RVec3 plate_at(0.5 * (kStrikerWestX + kStrikerEastX), kStrikerRestTop, 0.5 * (kStrikerNorthZ + kStrikerSouthZ));
    const std::vector<Part> striker = striker_parts();
    striker_ = kit_.add_body(kStrikerEntity, striker, plate_at, Quat::sIdentity(), total_mass(striker), 0.9F);
    kit_.disable_collision(striker_, frame_);
    kit_.set_continuous_collision(striker_);
    {
        SliderConstraintSettings settings;
        settings.mSpace = EConstraintSpace::WorldSpace;
        settings.mPoint1 = settings.mPoint2 = plate_at;
        settings.SetSliderAxis(-Vec3::sAxisY());
        settings.mLimitsMin = 0.0F;
        settings.mLimitsMax = kStopTravel;
        // The gas struts under the plate, each pushing up with a constant
        // force (a velocity motor aimed up, limited to the struts' force): the
        // plate's weight and the preload. A rider standing on the plate (785 N)
        // is well under the preload, so it holds against its upper stop.
        settings.mMotorSettings.SetForceLimit(
            total_mass(striker) * 9.81F + kStrutPreloadN);
        BodyLockWrite lock(system_.GetBodyLockInterface(), kit_.body_id(striker_));
        striker_slide_ = static_cast<SliderConstraint *>(settings.Create(Body::sFixedToWorld, lock.GetBody()));
        striker_slide_->SetMotorState(EMotorState::Velocity);
        striker_slide_->SetTargetVelocity(-kPlateRiseMps);
        system_.AddConstraint(striker_slide_);
    }

    const RVec3 turntable(kPylonX, kTurntableY, kPylonZ);
    const std::vector<Part> lamp = lamp_parts();
    lamp_ = kit_.add_body(kLampEntity, lamp, turntable, Quat::sIdentity(), total_mass(lamp), 0.5F);
    kit_.disable_collision(lamp_, frame_);
    kit_.disable_collision(lamp_, striker_);
    {
        HingeConstraintSettings settings;
        settings.mSpace = EConstraintSpace::WorldSpace;
        settings.mPoint1 = settings.mPoint2 = turntable;
        settings.mHingeAxis1 = settings.mHingeAxis2 = Vec3::sAxisY();
        settings.mNormalAxis1 = settings.mNormalAxis2 = Vec3::sAxisX();
        settings.mMaxFrictionTorque = 12000.0F;
        BodyLockWrite lock(system_.GetBodyLockInterface(), kit_.body_id(lamp_));
        lamp_hinge_ = static_cast<HingeConstraint *>(settings.Create(Body::sFixedToWorld, lock.GetBody()));
        system_.AddConstraint(lamp_hinge_);
    }
}

Summit::~Summit() {
    for (auto &chain : chains_) {
        if (chain != nullptr) system_.RemoveConstraint(chain);
    }
    if (lamp_hinge_ != nullptr) system_.RemoveConstraint(lamp_hinge_);
    if (striker_slide_ != nullptr) system_.RemoveConstraint(striker_slide_);
}

void Summit::build_ending() {
    const RVec3 origin(kBeamX, kDeckTop, kBeamZ);
    std::vector<Part> steel;
    // The two stools stand on the existing deck. Cheeks guide the flanges;
    // they do not hold the beam up. The strongback carries the chain blocks.
    for (const float x : {-3.0F, 3.0F}) {
        const std::size_t i = x < 0 ? 0 : 1;
        const std::vector<Part> stool{
            box(Vec3(0.35F, 1.2F, 0.35F), Vec3(0, 1.2F, 0), Material::Steel, 0),
            box(Vec3(0.5F, 0.08F, 0.45F), Vec3(0, 2.32F, 0), Material::Rust, 0)};
        bearings_[i] = kit_.add_body(i == 0 ? kLeftBearingEntity : kRightBearingEntity, stool,
                                     origin + Vec3(x, 0, 0), Quat::sIdentity(), 0, 0.8F);
        for (const float z : {-0.46F, 0.46F}) {
            steel.push_back(box(Vec3(0.35F, 0.85F, 0.06F), Vec3(x, 3.25F, z), Material::Yellow, 0));
        }
        steel.push_back(box(Vec3(0.08F, 2.5F, 0.08F), Vec3(x, 2.5F, 0.8F), Material::Steel, 0));
    }
    steel.push_back(box(Vec3(3.15F, 0.12F, 0.16F), Vec3(0, 5.0F, 0.8F), Material::Yellow, 0));
    for (const float x : {-2.0F, 2.0F}) {
        steel.push_back(box(Vec3(0.12F, 0.12F, 0.5F), Vec3(x, 5.0F, 0.4F), Material::Steel, 0));
        steel.push_back(box(Vec3(0.2F, 0.2F, 0.15F), Vec3(x, 4.8F, 0), Material::Rust, 0));
    }
    // A drive shaft brings the brake outboard of the load. Working beside
    // the beam lets the climber see both bearings while the steel settles.
    // Keep the wheel and its supports clear of the deck's south-side walk
    // (z=-185) to the parachute arm. The handle is reached from that walk.
    constexpr float brake_dx = kWorkX - kBeamX;
    constexpr float brake_z = -0.25F;
    steel.push_back(box(Vec3(-brake_dx * 0.5F, 0.08F, 0.08F),
                        Vec3(brake_dx * 0.5F, 1.2F, brake_z), Material::Steel, 0));
    // The shaft bears on a pedestal at either end, rather than floating.
    steel.push_back(box(Vec3(0.12F, 0.6F, 0.12F), Vec3(0, 0.6F, brake_z), Material::Steel, 0));
    steel.push_back(box(Vec3(0.12F, 0.6F, 0.12F), Vec3(brake_dx, 0.6F, brake_z), Material::Steel, 0));
    steel.push_back(box(Vec3(0.28F, 0.1F, 0.18F), Vec3(brake_dx, 1.2F, brake_z), Material::Yellow, 0));
    ending_frame_ = kit_.add_body(kEndingFrameEntity, steel, origin, Quat::sIdentity(), 0, 0.8F);
    const std::vector<Part> beam{
        box(Vec3(3.35F, 0.06F, 0.3F), Vec3(0, -0.24F, 0), Material::Steel, 300),
        box(Vec3(3.35F, 0.06F, 0.3F), Vec3(0, 0.24F, 0), Material::Steel, 300),
        box(Vec3(3.35F, 0.18F, 0.045F), Vec3::sZero(), Material::Rust, 300)};
    last_beam_ = kit_.add_body(kLastBeamEntity, beam, RVec3(kBeamX, kBeamSeatY + kBeamLift, kBeamZ),
                               Quat::sIdentity(), 900.0F, 0.8F);
    kit_.set_continuous_collision(last_beam_);
    kit_.set_damping(last_beam_, 0.15F, 0.2F);
    // Same tension-only hoist used by the yard crane. Paying out its length
    // releases the brake; gravity lowers the load. No body pose or velocity
    // is prescribed, and releasing Action holds the chain where it is.
    rig_chains(0.0F);
}

void Summit::rig_chains(const float slack) {
    const std::array<BodyID, 2> ids{kit_.body_id(ending_frame_), kit_.body_id(last_beam_)};
    BodyLockMultiWrite lock(system_.GetBodyLockInterface(), ids.data(), 2);
    Body &frame = *lock.GetBody(0);
    Body &load = *lock.GetBody(1);
    for (std::size_t i = 0; i < chains_.size(); ++i) {
        const float x = i == 0 ? -2.0F : 2.0F;
        PulleyConstraintSettings rope;
        rope.mSpace = EConstraintSpace::WorldSpace;
        rope.mBodyPoint1 = RVec3(kBeamX + x, kChainTop, kBeamZ);
        rope.mBodyPoint2 = load.GetWorldTransform() * Vec3(x, 0.3F, 0);
        rope.mMinLength = 0.0F;
        rope.mMaxLength = static_cast<float>((rope.mBodyPoint1 - rope.mBodyPoint2).Length()) + slack;
        if (chains_[i] == nullptr) {
            chains_[i] = new kit::RopeConstraint(frame, load, rope, true);
            system_.AddConstraint(chains_[i]);
        } else {
            chains_[i]->SetMaxLength(rope.mMaxLength);
            chains_[i]->ResetWarmStart();
        }
    }
}

bool Summit::work_in_reach(const RVec3 player) const noexcept {
    const Vec3 to = Vec3(player - RVec3(kWorkX, kDeckTop + 0.9F, kWorkZ));
    return to.LengthSq() <= 1.3F * 1.3F;
}

bool Summit::can_unrig() const noexcept {
    return state_.beam_seated && !state_.sling_free && state_.chain_tension_n < 100.0F &&
           state_.chain_paid_m > kBeamLift + 0.1F;
}

void Summit::work(const float dt, const bool lower, const bool unrig, const bool able, const RVec3 player) {
    if (!last_beam_.valid() || !able || !work_in_reach(player) || state_.sling_free) return;
    if (unrig && can_unrig()) {
        for (auto &chain : chains_) {
            system_.RemoveConstraint(chain);
            chain = nullptr;
        }
        state_.sling_free = true;
        state_.ending = Ending::Unrigged;
        return;
    }
    if (lower) {
        const float paid = std::min(kBeamLift + kChainSlack, state_.chain_paid_m + kChainRate * dt);
        for (auto &chain : chains_) chain->SetMaxLength(chain->GetMaxLength() + paid - state_.chain_paid_m);
        state_.chain_paid_m = paid;
    }
}

bool Summit::bearing(const int side) const {
    // A short ray from the actual underside at each end must meet the top
    // of that stool. Neither a proximity zone nor a chain can stand in for
    // steel beneath the flange. This also rejects a suspended or tipped beam.
    class StoolOnly final : public BodyFilter {
    public:
        explicit StoolOnly(const BodyID id) : id_(id) {}
        bool ShouldCollide(const BodyID &id) const override { return id == id_; }
    private:
        BodyID id_;
    };
    const auto &bodies = system_.GetBodyInterface();
    const BodyID stool = kit_.body_id(bearings_[side < 0 ? 0 : 1]);
    if (!system_.WereBodiesInContact(kit_.body_id(last_beam_), stool)) return false;
    const RVec3 foot = bodies.GetWorldTransform(kit_.body_id(last_beam_)) * Vec3(side * 3.0F, -0.3F, 0);
    const RRayCast ray(foot + Vec3(0, 0.01F, 0), Vec3(0, -0.025F, 0));
    RayCastResult hit;
    if (!system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, StoolOnly(stool))) return false;
    const RVec3 at = ray.GetPointOnRay(hit.mFraction);
    const BodyLockRead lock(system_.GetBodyLockInterface(), hit.mBodyID);
    return lock.Succeeded() && lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, at).GetY() > 0.95F &&
           std::abs(static_cast<float>(at.GetY()) - kBearingTop) < 0.01F;
}

void Summit::read_ending(const float dt, const bool grounded, const bool chute, const RVec3 player,
                         const std::uint64_t deaths) {
    if (!last_beam_.valid()) return;
    const auto &bodies = system_.GetBodyInterface();
    state_.left_bearing = bearing(-1);
    state_.right_bearing = bearing(1);
    bool slack = true;
    state_.chain_tension_n = 0.0F;
    for (std::size_t i = 0; i < chains_.size(); ++i) {
        const auto &chain = chains_[i];
        if (chain == nullptr) continue;
        const float x = i == 0 ? -2.0F : 2.0F;
        const RVec3 at = bodies.GetWorldTransform(kit_.body_id(last_beam_)) * Vec3(x, 0.3F, 0);
        const float lies = static_cast<float>((RVec3(kBeamX + x, kChainTop, kBeamZ) - at).Length());
        slack = slack && chain->GetMaxLength() - lies > 0.05F;
        state_.chain_tension_n += std::abs(chain->GetTotalLambdaPosition()) / dt;
    }
    const bool settled = slack && state_.chain_tension_n < 100.0F &&
                         state_.left_bearing && state_.right_bearing &&
                         bodies.GetLinearVelocity(kit_.body_id(last_beam_)).Length() < 0.03F &&
                         bodies.GetAngularVelocity(kit_.body_id(last_beam_)).Length() < 0.02F;
    bearing_seconds_ = settled ? bearing_seconds_ + dt : 0.0F;
    state_.beam_seated = bearing_seconds_ >= 0.4F;
    if (state_.ending == Ending::Unfinished && state_.beam_seated) state_.ending = Ending::Seated;
    if (!state_.sling_free || !state_.beam_seated) return;
    if (state_.ending == Ending::Canopy &&
        (deaths != flight_deaths_ || (!chute && !grounded) || (grounded && player.GetY() >= 5.0F)))
        state_.ending = Ending::Unrigged;
    if (state_.ending == Ending::Unrigged && chute && !grounded && player.GetY() < kDeckTop - 5.0F) {
        flight_deaths_ = deaths;
        state_.ending = Ending::Canopy;
    }
    if (state_.ending == Ending::Canopy && chute && grounded && player.GetY() < 5.0F && deaths == flight_deaths_)
        state_.ending = Ending::Home;
}

void Summit::restore_ending() {
    if (!last_beam_.valid()) return;
    // The save format already stores the kit bodies. Rebuild the safe hoist
    // topology from those bodies, never snap a saved beam back to its build
    // position. Radio and the final descent are per launch, like the rest of
    // the foreman's captions; a resumed seated load can be unrigged again.
    const bool seated = bearing(-1) && bearing(1);
    const float slack = seated ? kChainSlack : 0.0F;
    rig_chains(slack);
    state_.sling_free = false;
    state_.beam_seated = false;
    state_.ending = Ending::Unfinished;
    state_.chain_paid_m = std::clamp(static_cast<float>(kBeamSeatY + kBeamLift -
        system_.GetBodyInterface().GetPosition(kit_.body_id(last_beam_)).GetY()) + slack,
        0.0F, kBeamLift + kChainSlack);
    bearing_seconds_ = 0;
}

void Summit::pre_step() {
    if (state_.lit) {
        lamp_hinge_->GetMotorSettings().SetTorqueLimit(kLampTorqueNm);
        lamp_hinge_->SetTargetAngularVelocity(kLampSpeedRadPerS);
        lamp_hinge_->SetMotorState(EMotorState::Velocity);
    } else {
        lamp_hinge_->SetMotorState(EMotorState::Off);
    }
}

void Summit::post_step() {
    const float travel = striker_slide_->GetCurrentPosition();
    state_.striker_travel_m = travel;
    state_.striker_peak_m = std::max(state_.striker_peak_m, travel);
    if (travel >= kTripTravel) state_.lit = true;
    state_.lamp_yaw_rad = lamp_hinge_->GetCurrentAngle();
    state_.lamp_speed_rad_s = system_.GetBodyInterface().GetAngularVelocity(kit_.body_id(lamp_)).GetY();
}

} // namespace scraperx::sim
