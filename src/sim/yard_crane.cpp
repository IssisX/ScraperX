// The yard crane (see yard_crane.hpp).
#include "sim/yard_crane.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

Part box(const Vec3 half, const Vec3 offset, const Material material, const float mass_kg) {
    return {half, offset, Quat::sIdentity(), material, Part::Shape::Box, mass_kg};
}

// A bar of square section `half_thick` from a to b, in the body's frame.
Part bar(const Vec3 a, const Vec3 b, const float half_thick, const Material material, const float mass_kg) {
    const Vec3 run = b - a;
    const float length = run.Length();
    return {Vec3(0.5F * length, half_thick, half_thick), 0.5F * (a + b), Quat::sFromTo(Vec3::sAxisX(), run / length),
            material, Part::Shape::Box, mass_kg};
}

Part cylinder(const float radius, const float half_length, const Vec3 offset, const Quat rotation,
              const Material material, const float mass_kg) {
    return {Vec3(radius, half_length, 0.0F), offset, rotation, material, Part::Shape::Cylinder, mass_kg};
}

float total(const std::vector<Part> &parts) {
    float mass = 0.0F;
    for (const Part &part : parts) {
        mass += part.mass_kg;
    }
    return mass;
}

// A cylinder lying across the jib (along z).
const Quat kAcross = Quat::sRotation(Vec3::sAxisX(), 0.5F * JPH_PI);

// ---- the mast, in its own frame: its origin on the ground at its axis -----
constexpr float kPlinthTop = 0.5F;
constexpr float kChord = 1.0F;          // the chords' centres, either side of the axis
constexpr float kMastTop = YardCrane::kSlewY - 0.4F;
constexpr int kMastPanels = 23;

std::vector<Part> mast_parts() {
    std::vector<Part> parts;
    // The foundation, and the base frame's four feet on it.
    parts.push_back(box(Vec3(3.0F, 0.5F * kPlinthTop, 3.0F), Vec3(0.0F, 0.5F * kPlinthTop, 0.0F), Material::Concrete, 0.0F));
    // The four chords, 0.2 m square: too thick for a hand. The lacing between
    // them, a bar each way on every face of every panel, is not.
    const float half_run = 0.5F * (kMastTop - kPlinthTop);
    for (const float x : {-kChord, kChord}) {
        for (const float z : {-kChord, kChord}) {
            parts.push_back(box(Vec3(0.1F, half_run, 0.1F), Vec3(x, kPlinthTop + half_run, z), Material::Yellow, 0.0F));
            parts.push_back(box(Vec3(0.3F, 0.1F, 0.3F), Vec3(x, kPlinthTop + 0.1F, z), Material::Steel, 0.0F));
        }
    }
    const float panel = (kMastTop - kPlinthTop) / static_cast<float>(kMastPanels);
    for (int k = 0; k < kMastPanels; ++k) {
        const float low = kPlinthTop + panel * static_cast<float>(k);
        const float high = low + panel;
        const float flip = k % 2 == 0 ? 1.0F : -1.0F;
        // The faces at x = +-kChord and at z = +-kChord.
        for (const float side : {-kChord, kChord}) {
            parts.push_back(bar(Vec3(side, low, -flip * kChord), Vec3(side, high, flip * kChord), 0.045F,
                                Material::Yellow, 0.0F));
            parts.push_back(bar(Vec3(-flip * kChord, low, side), Vec3(flip * kChord, high, side), 0.045F,
                                Material::Yellow, 0.0F));
            parts.push_back(box(Vec3(0.04F, 0.04F, kChord), Vec3(side, high, 0.0F), Material::Yellow, 0.0F));
            parts.push_back(box(Vec3(kChord, 0.04F, 0.04F), Vec3(0.0F, high, side), Material::Yellow, 0.0F));
        }
    }
    // The bearing's seat on top.
    parts.push_back(box(Vec3(1.25F, 0.2F, 1.25F), Vec3(0.0F, kMastTop + 0.2F, 0.0F), Material::Steel, 0.0F));
    return parts;
}

// ---- the slewing assembly, in its own frame: its origin on the mast's axis
// at the top of the slewing ring, +x along the jib, +y up -------------------
constexpr float kJibRoot = 1.2F;
constexpr float kJibTip = 38.5F;
constexpr float kJibLow = 2.0F;       // the bottom chords (60 m)
constexpr float kJibHigh = 3.7F;      // the top chord
constexpr float kJibHalfWidth = 0.85F;
constexpr int kJibPanels = 15;
constexpr float kCounterEnd = -14.0F;
const Vec3 kApex(0.0F, 13.0F, 0.0F);

std::vector<Part> jib_parts() {
    std::vector<Part> parts;
    // The slewing ring's platform, and the cab off the jib's root with its
    // window toward the jib.
    parts.push_back(box(Vec3(1.6F, 0.4F, 1.6F), Vec3(0.0F, 0.4F, 0.0F), Material::Yellow, 4000.0F));
    parts.push_back(box(Vec3(1.0F, 1.1F, 0.8F), Vec3(1.4F, 1.6F, 1.95F), Material::Yellow, 1200.0F));
    parts.push_back(box(Vec3(0.02F, 0.6F, 0.65F), Vec3(2.42F, 1.85F, 1.95F), Material::Galvanised, 20.0F));
    // The jib: two bottom chords the trolley runs on, a top chord, laced in
    // a W down both sloping faces with a cross bar under every panel.
    const float half_jib = 0.5F * (kJibTip - kJibRoot);
    const float jib_mid = 0.5F * (kJibTip + kJibRoot);
    for (const float z : {-kJibHalfWidth, kJibHalfWidth}) {
        parts.push_back(box(Vec3(half_jib, 0.09F, 0.09F), Vec3(jib_mid, kJibLow, z), Material::Yellow, 1500.0F));
    }
    parts.push_back(box(Vec3(half_jib, 0.08F, 0.08F), Vec3(jib_mid, kJibHigh, 0.0F), Material::Yellow, 1000.0F));
    const float panel = (kJibTip - kJibRoot) / static_cast<float>(kJibPanels);
    for (int k = 0; k < kJibPanels; ++k) {
        const float x0 = kJibRoot + panel * static_cast<float>(k);
        const float x1 = x0 + 0.5F * panel;
        const float x2 = x0 + panel;
        for (const float z : {-kJibHalfWidth, kJibHalfWidth}) {
            parts.push_back(bar(Vec3(x0, kJibLow, z), Vec3(x1, kJibHigh, 0.0F), 0.045F, Material::Yellow, 30.0F));
            parts.push_back(bar(Vec3(x1, kJibHigh, 0.0F), Vec3(x2, kJibLow, z), 0.045F, Material::Yellow, 30.0F));
        }
        parts.push_back(box(Vec3(0.04F, 0.04F, kJibHalfWidth), Vec3(x2, kJibLow, 0.0F), Material::Yellow, 20.0F));
    }
    // The jib's root, where it pins to the tower head.
    parts.push_back(box(Vec3(0.3F, 0.95F, kJibHalfWidth + 0.1F), Vec3(kJibRoot, 2.8F, 0.0F), Material::Yellow, 600.0F));
    // The counter-jib: two box girders and a walkway between them, its
    // rails, the hoist's drum and motor, the panel, and at its end the
    // counterweight's four concrete slabs standing in their rack.
    const float half_counter = 0.5F * (-kJibRoot - kCounterEnd);
    const float counter_mid = 0.5F * (-kJibRoot + kCounterEnd);
    for (const float z : {-0.9F, 0.9F}) {
        parts.push_back(box(Vec3(half_counter, 0.35F, 0.12F), Vec3(counter_mid, 2.35F, z), Material::Yellow, 1200.0F));
        parts.push_back(box(Vec3(half_counter, 0.03F, 0.03F), Vec3(counter_mid, 3.35F, 1.08F * z), Material::Yellow, 40.0F));
    }
    parts.push_back(box(Vec3(half_counter, 0.03F, 0.75F), Vec3(counter_mid, 2.03F, 0.0F), Material::Galvanised, 600.0F));
    parts.push_back(cylinder(0.55F, 0.6F, Vec3(-6.0F, 2.65F, 0.0F), kAcross, Material::Steel, 2500.0F));
    parts.push_back(box(Vec3(0.5F, 0.4F, 0.5F), Vec3(-7.4F, 2.5F, 0.0F), Material::Galvanised, 400.0F));
    parts.push_back(box(Vec3(0.35F, 0.6F, 0.25F), Vec3(-3.6F, 2.7F, -0.45F), Material::Galvanised, 200.0F));
    for (int slab = 0; slab < 4; ++slab) {
        parts.push_back(box(Vec3(0.28F, 1.4F, 0.7F), Vec3(-13.4F + 0.6F * static_cast<float>(slab), 2.0F, 0.0F),
                            Material::Concrete, 4000.0F));
    }
    // The tower head: four legs from the platform's corners to its apex,
    // tied twice on the way, and the pendant bars from the apex out to the
    // jib and back to the counter-jib's end.
    for (const float x : {-1.0F, 1.0F}) {
        for (const float z : {-1.0F, 1.0F}) {
            parts.push_back(bar(Vec3(x, 0.8F, z), kApex, 0.1F, Material::Yellow, 800.0F));
        }
    }
    for (const float y : {4.6F, 8.6F}) {
        const float reach = 1.0F * (kApex.GetY() - y) / (kApex.GetY() - 0.8F);
        parts.push_back(box(Vec3(reach, 0.04F, 0.04F), Vec3(0.0F, y, -reach), Material::Yellow, 30.0F));
        parts.push_back(box(Vec3(reach, 0.04F, 0.04F), Vec3(0.0F, y, reach), Material::Yellow, 30.0F));
        parts.push_back(box(Vec3(0.04F, 0.04F, reach), Vec3(-reach, y, 0.0F), Material::Yellow, 30.0F));
        parts.push_back(box(Vec3(0.04F, 0.04F, reach), Vec3(reach, y, 0.0F), Material::Yellow, 30.0F));
    }
    parts.push_back(box(Vec3(0.3F, 0.3F, 0.3F), kApex, Material::Steel, 200.0F));
    for (const float z : {-0.12F, 0.12F}) {
        parts.push_back(bar(kApex + Vec3(0.0F, 0.0F, z), Vec3(26.0F, kJibHigh, z), 0.04F, Material::Steel, 150.0F));
    }
    for (const float z : {-0.9F, 0.9F}) {
        parts.push_back(bar(kApex, Vec3(kCounterEnd + 0.4F, 2.7F, z), 0.04F, Material::Steel, 150.0F));
    }
    return parts;
}

// ---- the trolley, in its own frame: its origin at its frame's middle, its
// axes the jib's ----------------------------------------------------------
// It hangs 0.5 m under the bottom chords, its wheels on top of them.
constexpr float kTrolleyBelowJib = 0.5F;
const Vec3 kTrolleyRope(0.0F, -0.75F, 0.0F);

std::vector<Part> trolley_parts() {
    std::vector<Part> parts;
    parts.push_back(box(Vec3(0.8F, 0.25F, 0.7F), Vec3::sZero(), Material::Yellow, 700.0F));
    for (const float z : {-1.02F, 1.02F}) {
        parts.push_back(box(Vec3(0.8F, 0.35F, 0.03F), Vec3(0.0F, 0.35F, z), Material::Yellow, 120.0F));
    }
    for (const float x : {-0.55F, 0.55F}) {
        for (const float z : {-kJibHalfWidth, kJibHalfWidth}) {
            parts.push_back(cylinder(0.14F, 0.06F, Vec3(x, 0.73F, z), kAcross, Material::Steel, 25.0F));
        }
        // The hoist's sheaves under the frame.
        parts.push_back(cylinder(0.3F, 0.08F, Vec3(0.6F * x, -0.42F, 0.0F), kAcross, Material::Steel, 60.0F));
    }
    return parts;
}

// ---- the hook block, in its own frame: its origin at the sheave case's
// middle ------------------------------------------------------------------
const Vec3 kHookRope(0.0F, 0.6F, 0.0F);
// The hook's saddle, where the sling's ring lies: under the swivel, as a
// hook's is, so a load hangs plumb under the rope.
const Vec3 kHookThroat(0.0F, -1.06F, 0.0F);

std::vector<Part> hook_parts() {
    std::vector<Part> parts;
    // The sheave case, painted in hazard stripes, its sheaves showing at
    // the top; the hook under it on its swivel: shank, belly, point.
    parts.push_back(box(Vec3(0.38F, 0.5F, 0.22F), Vec3::sZero(), Material::Hazard, 560.0F));
    parts.push_back(cylinder(0.32F, 0.1F, Vec3(0.0F, 0.35F, 0.0F), kAcross, Material::Steel, 80.0F));
    parts.push_back(cylinder(0.12F, 0.08F, Vec3(0.0F, -0.58F, 0.0F), Quat::sIdentity(), Material::Steel, 30.0F));
    parts.push_back(box(Vec3(0.07F, 0.2F, 0.07F), Vec3(-0.12F, -0.84F, 0.0F), Material::Steel, 25.0F));
    parts.push_back(box(Vec3(0.2F, 0.07F, 0.08F), Vec3(0.0F, -1.13F, 0.0F), Material::Steel, 50.0F));
    parts.push_back(box(Vec3(0.06F, 0.13F, 0.08F), Vec3(0.14F, -0.97F, 0.0F), Material::Steel, 25.0F));
    return parts;
}

// The sling's end: a master ring and its shackle, lifted by the ring.
const Vec3 kShackleRope(0.0F, 0.1F, 0.0F);

std::vector<Part> shackle_parts() {
    return {cylinder(0.11F, 0.025F, Vec3(0.0F, 0.1F, 0.0F), kAcross, Material::Steel, 8.0F),
            box(Vec3(0.05F, 0.07F, 0.03F), Vec3(0.0F, -0.04F, 0.0F), Material::Hazard, 4.0F)};
}

// ---- the remote's cabinet, by the foundation's south-east corner, open to
// the south; the remote hangs in it ----------------------------------------
const Vec3 kCabinetAt(4.2F, 0.0F, 4.2F);

std::vector<Part> cabinet_parts() {
    return {box(Vec3(0.06F, 0.6F, 0.06F), Vec3(0.0F, 0.6F, -0.1F), Material::Steel, 0.0F),
            box(Vec3(0.34F, 0.42F, 0.02F), Vec3(0.0F, 1.48F, -0.2F), Material::Yellow, 0.0F),
            box(Vec3(0.02F, 0.42F, 0.2F), Vec3(-0.34F, 1.48F, 0.0F), Material::Yellow, 0.0F),
            box(Vec3(0.02F, 0.42F, 0.2F), Vec3(0.34F, 1.48F, 0.0F), Material::Yellow, 0.0F),
            box(Vec3(0.36F, 0.02F, 0.22F), Vec3(0.0F, 1.92F, 0.0F), Material::Yellow, 0.0F),
            box(Vec3(0.36F, 0.02F, 0.22F), Vec3(0.0F, 1.04F, 0.0F), Material::Yellow, 0.0F)};
}

std::vector<Part> remote_parts() {
    // The belly box: its case, its two joysticks, its stop button, its aerial.
    return {box(Vec3(0.13F, 0.05F, 0.09F), Vec3::sZero(), Material::Yellow, 0.0F),
            cylinder(0.015F, 0.04F, Vec3(-0.07F, 0.08F, 0.0F), Quat::sIdentity(), Material::Steel, 0.0F),
            cylinder(0.015F, 0.04F, Vec3(0.07F, 0.08F, 0.0F), Quat::sIdentity(), Material::Steel, 0.0F),
            cylinder(0.025F, 0.015F, Vec3(0.0F, 0.06F, 0.05F), Quat::sIdentity(), Material::Hazard, 0.0F),
            cylinder(0.006F, 0.09F, Vec3(0.1F, 0.12F, -0.06F), Quat::sIdentity(), Material::Steel, 0.0F)};
}

// ---- a dock, in its own frame: its origin at the face, at its deck's top,
// at its middle along the face ---------------------------------------------
std::vector<Part> dock_parts(const YardCrane::Dock &dock) {
    std::vector<Part> parts;
    const float depth = YardCrane::kFaceX - dock.x_out;          // out from the face
    const float half_width = 0.5F * (dock.z_max - dock.z_min);
    const float mid_x = -0.5F * depth;
    // The deck plate, flush with the deck it serves, and two girders under it.
    parts.push_back(box(Vec3(0.5F * depth, 0.1F, half_width), Vec3(mid_x, -0.1F, 0.0F), Material::Steel, 0.0F));
    for (const float z : {-1.0F, 1.0F}) {
        parts.push_back(box(Vec3(0.5F * depth, 0.3F, 0.15F), Vec3(mid_x, -0.5F, z * (half_width - 0.35F)),
                            Material::Yellow, 0.0F));
    }
    // Rails down both sides: posts, a top and a mid rail, a toe board; and a
    // kerb along the outer edge, low enough for a forklift's wheel, high
    // enough to stop a rolling drum.
    for (const float z : {-1.0F, 1.0F}) {
        const float side = z * (half_width - 0.04F);
        for (float x = -0.3F; x > -depth; x -= 1.5F) {
            parts.push_back(box(Vec3(0.03F, 0.55F, 0.03F), Vec3(x, 0.55F, side), Material::Hazard, 0.0F));
        }
        parts.push_back(box(Vec3(0.5F * depth, 0.025F, 0.025F), Vec3(mid_x, 1.1F, side), Material::Hazard, 0.0F));
        parts.push_back(box(Vec3(0.5F * depth, 0.02F, 0.02F), Vec3(mid_x, 0.55F, side), Material::Hazard, 0.0F));
        parts.push_back(box(Vec3(0.5F * depth, 0.075F, 0.015F), Vec3(mid_x, 0.075F, side), Material::Yellow, 0.0F));
    }
    parts.push_back(box(Vec3(0.05F, 0.06F, half_width), Vec3(-depth + 0.05F, 0.06F, 0.0F), Material::Hazard, 0.0F));
    if (dock.legs) {
        // The pier's two legs under its outer end, down to the ground,
        // braced across.
        const float height = dock.top_y - 0.8F;
        for (const float z : {-1.0F, 1.0F}) {
            parts.push_back(box(Vec3(0.2F, 0.5F * height, 0.2F), Vec3(-depth + 0.6F, -0.8F - 0.5F * height,
                                                                     z * (half_width - 0.35F)),
                                Material::Rust, 0.0F));
        }
        parts.push_back(bar(Vec3(-depth + 0.6F, -1.0F, -(half_width - 0.35F)),
                            Vec3(-depth + 0.6F, -dock.top_y + 0.5F, half_width - 0.35F), 0.08F, Material::Rust, 0.0F));
        parts.push_back(box(Vec3(0.2F, 0.4F, 0.6F), Vec3(-depth + 0.6F, -dock.top_y + 0.4F, 0.0F), Material::Concrete, 0.0F));
    } else {
        // Two struts under each girder back to the face.
        for (const float z : {-1.0F, 1.0F}) {
            parts.push_back(bar(Vec3(-depth + 0.4F, -0.75F, z * (half_width - 0.35F)),
                                Vec3(-0.15F, -4.5F, z * (half_width - 0.35F)), 0.1F, Material::Rust, 0.0F));
        }
    }
    return parts;
}

float signed_unit(const float value) {
    return std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0.0F;
}

// The jib's heading when built: out over the laydown, south of the mast.
constexpr float kBuiltHeading = -0.5F * JPH_PI;
// The trolley and the hook, as built: 20 m out, the hook near the ground,
// its sling's shackle hanging just clear of it.
constexpr float kBuiltTrolleyM = 20.0F;
constexpr float kBuiltHookY = 4.55F;

} // namespace

YardCrane::YardCrane(PhysicsSystem &physics, kit::Kit &kit) : physics_(physics), kit_(kit) {
    const RVec3 axis(kMastX, 0.0, kMastZ);
    mast_ = kit_.add_body(kMastEntity, mast_parts(), axis, Quat::sIdentity(), 0.0F, 0.7F);
    const Quat heading = Quat::sRotation(Vec3::sAxisY(), kBuiltHeading);
    const RVec3 pivot(kMastX, kSlewY, kMastZ);
    const std::vector<Part> jib = jib_parts();
    jib_ = kit_.add_body(kJibEntity, jib, pivot, heading, total(jib), 0.6F);
    // The slewing platform rides on the mast's bearing, not on its seat.
    kit_.disable_collision(mast_, jib_);
    const Vec3 out = heading * Vec3::sAxisX();
    const std::vector<Part> trolley = trolley_parts();
    const RVec3 trolley_at = pivot + out * kBuiltTrolleyM + Vec3(0.0F, kJibLow - kTrolleyBelowJib, 0.0F);
    trolley_ = kit_.add_body(kTrolleyEntity, trolley, trolley_at, heading, total(trolley), 0.4F);
    kit_.disable_collision(jib_, trolley_);
    const std::vector<Part> hook = hook_parts();
    const RVec3 hook_at(trolley_at.GetX(), kBuiltHookY, trolley_at.GetZ());
    hook_ = kit_.add_body(kHookEntity, hook, hook_at, heading, total(hook), 0.5F);
    // The falls of the reeving (four, between the trolley's sheaves and the
    // block's) keep the block from rocking on its rope as a single line would
    // let it: declared, as damping of its turn.
    kit_.set_damping(hook_, 0.05F, 1.0F);
    const std::vector<Part> shackle = shackle_parts();
    const RVec3 throat = hook_at + heading * kHookThroat;
    shackle_ = kit_.add_body(kShackleEntity, shackle, throat - Vec3(0.0F, kSlingM, 0.0F) - kShackleRope, heading,
                             total(shackle), 0.6F);
    kit_.set_carry(shackle_, kit::CarryKind::Shackle, kShackleRope);
    kit_.set_damping(shackle_, 0.5F, 1.0F);
    kit_.disable_collision(hook_, shackle_);
    // The chain sling, 12 t safe working load: it does not part under
    // anything the hoist can lift.
    sling_ = kit_.add_direct_rope(hook_, kHookThroat, shackle_, kShackleRope, kSlingM, 0.0F);
    // The remote's cabinet and the remote in it.
    const RVec3 cabinet_at = axis + kCabinetAt;
    cabinet_ = kit_.add_body(kCabinetEntity, cabinet_parts(), cabinet_at, Quat::sIdentity(), 0.0F, 0.6F);
    remote_ = kit_.add_body(kRemoteEntity, remote_parts(), cabinet_at + Vec3(0.0F, 1.42F, 0.02F), Quat::sIdentity(),
                            0.0F, 0.6F);
    // The docks on the stack's west face.
    for (int index = 0; index < kDocks; ++index) {
        const Dock &dock = kDockSpecs[static_cast<std::size_t>(index)];
        (void)kit_.add_body(kDockFirstEntity + static_cast<std::uint64_t>(index), dock_parts(dock),
                            RVec3(kFaceX, dock.top_y, 0.5F * (dock.z_min + dock.z_max)), Quat::sIdentity(), 0.0F, 0.8F);
    }

    BodyLockInterfaceNoLock const &locks = physics_.GetBodyLockInterfaceNoLock();
    Body &jib_body = *locks.TryGetBody(kit_.body_id(jib_));
    Body &trolley_body = *locks.TryGetBody(kit_.body_id(trolley_));
    Body &hook_body = *locks.TryGetBody(kit_.body_id(hook_));

    // The slew: the assembly on the mast's bearing, a hinge about the axis,
    // turned by its drive (a velocity motor of finite torque), its brake the
    // same motor held at rest.
    HingeConstraintSettings slew;
    slew.mSpace = EConstraintSpace::WorldSpace;
    slew.mPoint1 = slew.mPoint2 = pivot;
    slew.mHingeAxis1 = slew.mHingeAxis2 = Vec3::sAxisY();
    slew.mNormalAxis1 = slew.mNormalAxis2 = out;
    slew.mMotorSettings = MotorSettings(2.0F, 1.0F, 0.0F, kSlewTorqueNm);
    slew_ = static_cast<HingeConstraint *>(slew.Create(Body::sFixedToWorld, jib_body));
    physics_.AddConstraint(slew_);
    slew_->SetMotorState(EMotorState::Velocity);
    slew_->SetTargetAngularVelocity(0.0F);

    // The trolley along the jib, its drive a velocity motor of finite force,
    // its travel between its limit stops.
    SliderConstraintSettings run;
    run.mSpace = EConstraintSpace::WorldSpace;
    run.mAutoDetectPoint = true;
    run.SetSliderAxis(out);
    run.mLimitsMin = kTrolleyMinM - kBuiltTrolleyM;
    run.mLimitsMax = kTrolleyMaxM - kBuiltTrolleyM;
    run.mMotorSettings = MotorSettings(2.0F, 1.0F, kTrolleyForceN, 0.0F);
    run.mMotorSettings.SetForceLimits(-kTrolleyForceN, kTrolleyForceN);
    trolley_drive_ = static_cast<SliderConstraint *>(run.Create(jib_body, trolley_body));
    physics_.AddConstraint(trolley_drive_);
    trolley_drive_->SetMotorState(EMotorState::Velocity);
    trolley_drive_->SetTargetVelocity(0.0F);

    // The hoist rope, straight from the trolley's sheaves to the hook
    // block's, as long as it hangs.
    PulleyConstraintSettings rope;
    rope.mSpace = EConstraintSpace::WorldSpace;
    rope.mBodyPoint1 = trolley_body.GetWorldTransform() * kTrolleyRope;
    rope.mBodyPoint2 = hook_body.GetWorldTransform() * kHookRope;
    rope.mFixedPoint1 = rope.mBodyPoint1;
    rope.mFixedPoint2 = rope.mBodyPoint2;
    rope.mMinLength = 0.0F;
    rope.mMaxLength = static_cast<float>(Vec3(rope.mBodyPoint1 - rope.mBodyPoint2).Length());
    hoist_ = new kit::RopeConstraint(trolley_body, hook_body, rope, true);
    physics_.AddConstraint(hoist_);
    post_step(0.0F);
}

YardCrane::~YardCrane() {
    physics_.RemoveConstraint(hoist_);
    physics_.RemoveConstraint(trolley_drive_);
    physics_.RemoveConstraint(slew_);
}

bool YardCrane::remote_in_reach(const RVec3 player) const {
    return kit_.body_enabled(remote_) &&
           Vec3(kit_.body_position(remote_) - player).Length() <= kRemoteReachM;
}

bool YardCrane::take_remote(const RVec3 player) {
    if (!remote_in_reach(player)) {
        return false;
    }
    kit_.park(remote_);
    state_.has_remote = true;
    return true;
}

bool YardCrane::has_remote() const {
    return !kit_.body_enabled(remote_);
}

RVec3 YardCrane::on(const kit::BodyIndex body, const Vec3 local) const {
    return kit_.body_position(body) + kit_.body_rotation(body) * local;
}

RVec3 YardCrane::rope_top() const {
    return on(trolley_, kTrolleyRope);
}

RVec3 YardCrane::rope_bottom() const {
    return on(hook_, kHookRope);
}

RVec3 YardCrane::hook_position() const {
    return on(hook_, kHookThroat);
}

float YardCrane::rope_length_now() const {
    return static_cast<float>(Vec3(rope_top() - rope_bottom()).Length());
}

void YardCrane::pre_step(const float delta_seconds, const bool toggle, const bool able, const float hook_x,
                         const float hook_z, const float hoist) {
    state_.has_remote = has_remote();
    const bool may = able && state_.has_remote;
    if (toggle) {
        state_.operating = !state_.operating && may;
    } else if (state_.operating && !may) {
        // Hands taken, out of range: the remote's dead man lets go.
        state_.operating = false;
    }

    // Where the stick sends the hook, shared between the slew and the
    // trolley: out along the jib is the trolley's, round it the slew's.
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    Vec3 out = bodies.GetRotation(kit_.body_id(jib_)) * Vec3::sAxisX();
    out.SetY(0.0F);
    out = out.NormalizedOr(Vec3::sAxisX());
    const Vec3 round = Vec3::sAxisY().Cross(out);
    float want_slew = 0.0F;
    float want_trolley = 0.0F;
    float want_hoist = 0.0F;
    if (state_.operating) {
        Vec3 stick(signed_unit(hook_x), 0.0F, signed_unit(hook_z));
        if (stick.LengthSq() > 1.0F) {
            stick = stick.Normalized();
        }
        const float radius = std::max(state_.trolley_m, kTrolleyMinM);
        want_trolley = std::clamp(stick.Dot(out) * kHookMps, -kTrolleyMaxMps, kTrolleyMaxMps);
        want_slew = std::clamp(stick.Dot(round) * kHookMps / radius, -kSlewMaxRadS, kSlewMaxRadS);
        want_hoist = signed_unit(hoist);
    }
    // The slewing limiter: short of the zone, in the way the jib is turning,
    // by less than it needs to brake (and a tick's travel), the drive is cut.
    {
        const float heading = state_.slew_rad;
        const float braking = 0.5F * state_.slew_rate * state_.slew_rate / kSlewAccelRadS2 +
                              std::abs(state_.slew_rate) * delta_seconds + 0.002F;
        const auto ahead = [](const float from, const float to) {
            const float angle = std::fmod(to - from, 2.0F * JPH_PI);
            return angle < 0.0F ? angle + 2.0F * JPH_PI : angle;
        };
        if ((want_slew > 0.0F || state_.slew_rate > 0.0F) && ahead(heading, kZoneLowRad) <= braking) {
            want_slew = std::min(want_slew, 0.0F);
        }
        if ((want_slew < 0.0F || state_.slew_rate < 0.0F) && ahead(kZoneHighRad, heading) <= braking) {
            want_slew = std::max(want_slew, 0.0F);
        }
    }
    // Each drive runs up to the rate asked at its acceleration, and down.
    const float slew_step = kSlewAccelRadS2 * delta_seconds;
    state_.slew_rate += std::clamp(want_slew - state_.slew_rate, -slew_step, slew_step);
    const float trolley_step = kTrolleyAccelMps2 * delta_seconds;
    state_.trolley_rate += std::clamp(want_trolley - state_.trolley_rate, -trolley_step, trolley_step);
    // The trolley's limit switches stop its drive short of each stop.
    if ((state_.trolley_m >= kTrolleyMaxM - 0.05F && state_.trolley_rate > 0.0F) ||
        (state_.trolley_m <= kTrolleyMinM + 0.05F && state_.trolley_rate < 0.0F)) {
        state_.trolley_rate = 0.0F;
    }
    slew_->SetTargetAngularVelocity(state_.slew_rate);
    trolley_drive_->SetTargetVelocity(state_.trolley_rate);

    // The hoist. The drum runs up to the rate asked at kHoistAccelMps2, slows
    // at kHoistDecelMps2, and never runs faster than it can stop in the rope
    // left before the upper limit (raising) or the drum's end (lowering). Raising, the winch takes the
    // rope in with at most its pull (the rope's length follows it in); at the
    // upper limit it stands. Lowering, the drum pays out while the rope is
    // loaded; slack, the switch stops it. At rest, the brake holds the rope.
    const float lies = rope_length_now();
    const float now = hoist_->GetMaxLength();
    float want_rate = std::abs(want_hoist) > 0.05F ? want_hoist * kHoistMps : 0.0F;
    const float room_up = std::max(0.0F, lies - kHoistMinM);
    const float room_down = std::max(0.0F, kHoistMaxM - now);
    want_rate = std::clamp(want_rate, -std::sqrt(2.0F * kHoistDecelMps2 * room_down),
                           std::sqrt(2.0F * kHoistDecelMps2 * room_up));
    // Speeding up (the same way, faster) at the drive's acceleration; slowing
    // or reversing at its deceleration.
    const bool speeding_up = want_rate * state_.hoist_rate >= 0.0F && std::abs(want_rate) > std::abs(state_.hoist_rate);
    const float hoist_step = (speeding_up ? kHoistAccelMps2 : kHoistDecelMps2) * delta_seconds;
    state_.hoist_rate += std::clamp(want_rate - state_.hoist_rate, -hoist_step, hoist_step);
    if (std::abs(state_.hoist_rate) < 1.0e-4F && want_rate == 0.0F) {
        state_.hoist_rate = 0.0F;
    }
    state_.upper_limit = want_hoist > 0.05F && room_up <= 0.01F;
    state_.slack = false;
    if (state_.hoist_rate > 0.0F && room_up > 0.0F) {
        hoist_->SetMaxLength(std::max(kHoistMinM, std::min(now, lies)));
        hoist_->SetMotor(state_.hoist_rate, kHoistForceN);
    } else if (state_.hoist_rate < 0.0F) {
        hoist_->SetMotor(0.0F, 0.0F);
        const float next = std::min({now - state_.hoist_rate * delta_seconds, lies + kSlackPayOutM, kHoistMaxM});
        if (next > now) {
            hoist_->SetMaxLength(next);
        } else {
            // Paying out no further: slack (the switch), or the drum's end.
            state_.slack = lies + kSlackPayOutM <= now;
            state_.hoist_rate = 0.0F;
        }
    } else {
        // At rest, or arrived at the upper limit: the brake holds.
        state_.hoist_rate = 0.0F;
        hoist_->SetMotor(0.0F, 0.0F);
    }
    state_.hoist = want_hoist;
}

void YardCrane::post_step(const float delta_seconds) {
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const Vec3 out = bodies.GetRotation(kit_.body_id(jib_)) * Vec3::sAxisX();
    state_.slew_rad = std::atan2(-out.GetZ(), out.GetX());
    const RVec3 trolley = bodies.GetPosition(kit_.body_id(trolley_));
    state_.trolley_m = static_cast<float>(std::hypot(trolley.GetX() - kMastX, trolley.GetZ() - kMastZ));
    const float was = state_.hoist_m;
    state_.hoist_m = rope_length_now();
    if (delta_seconds > 0.0F) {
        state_.hoist_n = std::abs(hoist_->GetTotalLambdaPosition() + hoist_->GetMotorLambda()) / delta_seconds;
        // Stalled: at its full pull and the rope coming in at under a tenth
        // of the speed asked (a load snatched up off slack is at full pull
        // for a moment, and rising).
        const float coming_in = (was - state_.hoist_m) / delta_seconds;
        state_.hoist_stalled = state_.hoist_rate > 0.05F &&
                               -hoist_->GetMotorLambda() >= kHoistForceN * delta_seconds * 0.999F &&
                               coming_in < 0.1F * state_.hoist_rate;
        state_.rope_mps = coming_in;
        state_.slew_torque_nm = std::abs(slew_->GetTotalLambdaMotor()) / delta_seconds;
        state_.trolley_force_n = std::abs(trolley_drive_->GetTotalLambdaMotor()) / delta_seconds;
    }
    state_.slew_measured = bodies.GetAngularVelocity(kit_.body_id(jib_)).GetY();
    const Vec3 along = Vec3(out.GetX(), 0.0F, out.GetZ()).NormalizedOr(Vec3::sAxisX());
    state_.trolley_mps = (bodies.GetLinearVelocity(kit_.body_id(trolley_)) -
                          bodies.GetPointVelocity(kit_.body_id(jib_), trolley))
                             .Dot(along);
    state_.hook_mps = bodies.GetLinearVelocity(kit_.body_id(hook_)).Length();
    state_.sling_n = kit_.rope_tension(sling_);
    const std::uint64_t end = kit_.rope_end_entity(sling_);
    state_.load_entity = end == kShackleEntity ? 0 : end;
}

std::array<YardCrane::Noise, 3> YardCrane::noises() const {
    // CHOSEN loudness (a sound of 1 carries 55 m): geared electric drives,
    // the hoist's the loudest and louder with what it lifts.
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    std::array<Noise, 3> out{};
    out[0] = {kMastX, kSlewY, kMastZ, 0.4F * std::min(1.0F, std::abs(state_.slew_measured) / kSlewMaxRadS)};
    const RVec3 drum = bodies.GetWorldTransform(kit_.body_id(jib_)) * Vec3(-6.0F, 2.65F, 0.0F);
    float hoist = 0.0F;
    if (state_.hoist_stalled) {
        hoist = 0.7F;
    } else if (std::abs(state_.rope_mps) > 0.02F) {
        hoist = 0.35F + 0.25F * std::min(1.0F, state_.hoist_n / kHoistForceN);
    }
    out[1] = {float(drum.GetX()), float(drum.GetY()), float(drum.GetZ()), hoist};
    const RVec3 top = rope_top();
    out[2] = {float(top.GetX()), float(top.GetY()), float(top.GetZ()),
              0.3F * std::min(1.0F, std::abs(state_.trolley_mps) / kTrolleyMaxMps)};
    return out;
}

void YardCrane::restore() {
    state_ = {};
    state_.has_remote = has_remote();
    slew_->SetTargetAngularVelocity(0.0F);
    trolley_drive_->SetTargetVelocity(0.0F);
    hoist_->SetMotor(0.0F, 0.0F);
    hoist_->SetMaxLength(std::max(kHoistMinM, rope_length_now()));
    slew_->ResetWarmStart();
    trolley_drive_->ResetWarmStart();
    hoist_->ResetWarmStart();
    post_step(0.0F);
}

} // namespace scraperx::sim
