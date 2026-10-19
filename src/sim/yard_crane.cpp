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

// ---- the lifting magnet, in its own frame: its origin at its casing's
// middle, its face down ---------------------------------------------------
// Its casing, the steel face under it, the cable's terminal box, and the
// bail it hangs by, the eye at the bail's middle.
constexpr float kMagnetHalfHeight = 0.24F;   // from the origin to the face
const Vec3 kMagnetFace(0.0F, -kMagnetHalfHeight, 0.0F);
const Vec3 kMagnetEye(0.0F, 0.54F, 0.0F);
// Where it lies in the yard, on its face, before it is rigged: north of the
// laydown, 15.5 m out from the mast, 2 m clear of the way from the remote's
// cabinet to the containers.
constexpr float kMagnetParkX = -70.0F;
constexpr float kMagnetParkZ = -122.0F;

std::vector<Part> magnet_parts() {
    const float r = YardCrane::kMagnetRadiusM;
    return {cylinder(r, 0.2F, Vec3::sZero(), Quat::sIdentity(), Material::Yellow, 900.0F),
            cylinder(r, 0.02F, Vec3(0.0F, -0.22F, 0.0F), Quat::sIdentity(), Material::Steel, 120.0F),
            box(Vec3(0.12F, 0.08F, 0.1F), Vec3(0.32F, 0.28F, 0.0F), Material::Galvanised, 20.0F),
            box(Vec3(0.04F, 0.16F, 0.04F), Vec3(-0.22F, 0.36F, 0.0F), Material::Steel, 15.0F),
            box(Vec3(0.04F, 0.16F, 0.04F), Vec3(0.22F, 0.36F, 0.0F), Material::Steel, 15.0F),
            box(Vec3(0.26F, 0.04F, 0.05F), Vec3(0.0F, 0.54F, 0.0F), Material::Steel, 30.0F)};
}

// What a magnet holds by: steel bare, rusted or galvanized (not the paint the
// machines wear).
bool magnetic(const Material material) {
    return material == Material::Steel || material == Material::Rust || material == Material::Galvanised;
}

// ---- the container spreader, in its own frame: +x along it, its origin at
// its middle on the level of its pockets' undersides, which sit on a box's
// corner castings ----------------------------------------------------------
// Its two side beams over the twistlocks' lines and its two end beams, all
// clear of a box's roof; a pocket at each corner with its twistlock in it;
// the head block across its middle and the lug on it, the eye at the lug's top.
const Vec3 kSpreaderEye(0.0F, 0.9F, 0.0F);
// Where it lies in the yard, on its pockets, before it is rigged: west of the
// laydown's containers and lying the way they do (along z), 24.5 m out from
// the mast.
constexpr float kSpreaderParkX = -84.5F;
constexpr float kSpreaderParkZ = -121.0F;

std::vector<Part> spreader_parts() {
    using C = YardCrane;
    std::vector<Part> parts;
    for (const float z : {-C::kTwistlockZ, C::kTwistlockZ}) {
        parts.push_back(box(Vec3(6.1F, 0.15F, 0.1F), Vec3(0.0F, 0.27F, z), Material::Yellow, 450.0F));
    }
    for (const float x : {-6.0F, 6.0F}) {
        parts.push_back(box(Vec3(0.12F, 0.15F, 1.23F), Vec3(x, 0.27F, 0.0F), Material::Yellow, 180.0F));
    }
    for (const float x : {-C::kTwistlockX, C::kTwistlockX}) {
        for (const float z : {-C::kTwistlockZ, C::kTwistlockZ}) {
            parts.push_back(box(Vec3(0.15F, 0.12F, 0.15F), Vec3(x, 0.12F, z), Material::Steel, 60.0F));
        }
    }
    parts.push_back(box(Vec3(0.4F, 0.22F, 1.13F), Vec3(0.0F, 0.42F, 0.0F), Material::Yellow, 450.0F));
    parts.push_back(box(Vec3(0.06F, 0.14F, 0.04F), Vec3(0.0F, 0.78F, 0.0F), Material::Steel, 30.0F));
    return parts;
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

void YardCrane::build_devices() {
    const std::vector<Part> magnet = magnet_parts();
    magnet_ = kit_.add_body(kMagnetEntity, magnet, RVec3(kMagnetParkX, kMagnetHalfHeight + 0.004F, kMagnetParkZ),
                            Quat::sIdentity(), total(magnet), 0.6F);
    (void)kit_.add_anchor(magnet_, kMagnetEye, 1.2F);
    // Its chain sling's links grind as it swings, so a hung magnet settles
    // under the hook in a few swings: declared, as damping of its motion.
    kit_.set_damping(magnet_, 0.2F, 0.5F);
    magnet_grip_ = kit_.add_grip(magnet_, kMagnetRatedN);
    // The spreader after it, so a save made with the magnet lines up too.
    const std::vector<Part> spreader = spreader_parts();
    spreader_ = kit_.add_body(kSpreaderEntity, spreader, RVec3(kSpreaderParkX, 0.004F, kSpreaderParkZ),
                              Quat::sRotation(Vec3::sAxisY(), 0.5F * JPH_PI), total(spreader), 0.6F);
    (void)kit_.add_anchor(spreader_, kSpreaderEye, 1.5F);
    // Its four-leg sling, spread to the frame's corners, keeps it from
    // turning or swinging far under the hook: declared, as damping.
    kit_.set_damping(spreader_, 0.2F, 2.0F);
    spreader_grip_ = kit_.add_grip(spreader_, 4.0F * kTwistlockRatedN);
}

bool YardCrane::own(const kit::BodyIndex body) const noexcept {
    return body == jib_ || body == trolley_ || body == hook_ || body == shackle_ || body == magnet_ ||
           body == spreader_;
}

kit::BodyIndex YardCrane::magnet_contact(const std::uint64_t carried, int &points) const {
    points = 0;
    // The face's nine points: its middle, and eight round it at 0.6 of its
    // radius. A point is in contact over a steel box part's face turned to
    // the magnet within kMagnetTiltRad, from kMagnetGapM off it to 2 cm in.
    const RVec3 at = kit_.body_position(magnet_);
    const Quat turn = kit_.body_rotation(magnet_);
    const Vec3 down = turn * Vec3(0.0F, -1.0F, 0.0F);
    std::array<RVec3, 9> face{};
    face[0] = at + turn * kMagnetFace;
    for (int k = 0; k < 8; ++k) {
        const float a = 0.25F * JPH_PI * static_cast<float>(k);
        const float ring = 0.6F * kMagnetRadiusM;
        face[static_cast<std::size_t>(k) + 1] = at + turn * (kMagnetFace + Vec3(ring * std::cos(a), 0.0F, ring * std::sin(a)));
    }
    const float square = std::cos(kMagnetTiltRad);
    kit::BodyIndex best{};
    for (std::uint32_t index = 0; index < kit_.body_count(); ++index) {
        const kit::BodyIndex body{index};
        const std::uint64_t entity = kit_.body_entity(body);
        if (own(body) || !kit_.body_dynamic(body) || !kit_.body_enabled(body) || entity == carried) {
            continue;
        }
        const RVec3 where = kit_.body_position(body);
        const Quat turned = kit_.body_rotation(body);
        // Nothing of it near the face (the longest stock is 12.2 m).
        if (Vec3(where - face[0]).LengthSq() > 7.0F * 7.0F) {
            continue;
        }
        std::uint32_t touching = 0;
        for (const Part &part : kit_.body_parts(body)) {
            if (part.shape != Part::Shape::Box || !magnetic(part.material)) {
                continue;
            }
            const Quat part_turn = turned * part.rotation;
            const RVec3 middle = where + turned * part.offset;
            const Quat back = part_turn.Conjugated();
            // The part's face turned to the magnet: along its axis most in
            // line with the magnet's, on the side toward it.
            const Vec3 d = back * down;
            int axis = 0;
            for (int i = 1; i < 3; ++i) {
                if (std::abs(d[i]) > std::abs(d[axis])) {
                    axis = i;
                }
            }
            if (std::abs(d[axis]) < square) {
                continue;
            }
            const float side = d[axis] > 0.0F ? -1.0F : 1.0F;
            for (std::size_t j = 0; j < face.size(); ++j) {
                const Vec3 p = back * Vec3(face[j] - middle);
                const float off = side * p[axis] - part.half[axis];
                bool over = off >= -0.02F && off <= (j == 0 ? kMagnetTouchM : kMagnetGapM);
                for (int i = 0; i < 3 && over; ++i) {
                    over = i == axis || std::abs(p[i]) <= part.half[i];
                }
                if (over) {
                    touching |= 1U << j;
                }
            }
        }
        int count = 0;
        for (std::uint32_t bits = touching; bits != 0U; bits &= bits - 1U) {
            ++count;
        }
        if ((touching & 1U) != 0U && count >= kMagnetMinContact && count > points) {
            best = body;
            points = count;
        }
    }
    return best;
}

void YardCrane::work_magnet(const bool rigged, const bool flip, const std::uint64_t carried) {
    if (flip) {
        state_.magnet_on = !state_.magnet_on;
    }
    if (!rigged) {
        state_.magnet_on = false;   // off the sling, its cable is dead
    }
    if (!state_.magnet_on) {
        kit_.release(magnet_grip_);
        return;
    }
    if (kit_.grip_load(magnet_grip_).valid()) {
        return;
    }
    // On and holding nothing: it takes hold of what it lies flat against, as
    // hard as the steel under its face lets it; not the load that just tore
    // off it, until it has been clear of the steel.
    int points = 0;
    const kit::BodyIndex body = magnet_contact(carried, points);
    if (!body.valid()) {
        state_.magnet_torn = false;
    } else if (!state_.magnet_torn) {
        const float sheet = kit_.body_hollow(body) ? kMagnetSheetShare : 1.0F;
        kit_.set_grip_rating(magnet_grip_, sheet * kMagnetRatedN * static_cast<float>(points) / 9.0F);
        (void)kit_.grip(magnet_grip_, body);
    }
}

kit::BodyIndex YardCrane::spreader_seat() const {
    // A box's corner castings: their apertures 89 mm in from its top corners.
    constexpr float kCastingInsetM = 0.089F;
    // The twistlocks, where they come out of the pockets' undersides.
    const RVec3 at = kit_.body_position(spreader_);
    const Quat turn = kit_.body_rotation(spreader_);
    std::array<RVec3, 4> locks{};
    std::size_t n = 0;
    for (const float x : {-kTwistlockX, kTwistlockX}) {
        for (const float z : {-kTwistlockZ, kTwistlockZ}) {
            locks[n++] = at + turn * Vec3(x, 0.0F, z);
        }
    }
    const Vec3 along = turn * Vec3::sAxisX();
    const Vec3 up = turn * Vec3::sAxisY();
    const float square = std::cos(kSpreaderSquareRad);
    for (std::uint32_t index = 0; index < kit_.body_count(); ++index) {
        const kit::BodyIndex body{index};
        if (own(body) || !kit_.body_dynamic(body) || !kit_.body_enabled(body)) {
            continue;
        }
        const RVec3 where = kit_.body_position(body);
        if (Vec3(where - at).LengthSq() > 9.0F * 9.0F) {
            continue;
        }
        const Quat turned = kit_.body_rotation(body);
        for (const Part &part : kit_.body_parts(body)) {
            // A box whose castings its twistlocks fit (a 40 ft box's).
            const float cx = part.half.GetX() - kCastingInsetM;
            const float cz = part.half.GetZ() - kCastingInsetM;
            if (part.shape != Part::Shape::Box || std::abs(cx - kTwistlockX) > 0.02F ||
                std::abs(cz - kTwistlockZ) > 0.02F) {
                continue;
            }
            const Quat back = (turned * part.rotation).Conjugated();
            const RVec3 middle = where + turned * part.offset;
            // Square to it, along it either way, its roof up under the frame.
            if (std::abs((back * along).GetX()) < square || (back * up).GetY() < square) {
                continue;
            }
            bool sits = true;
            for (const RVec3 &lock : locks) {
                const Vec3 p = back * Vec3(lock - middle);
                const float above = p.GetY() - part.half.GetY();
                sits = sits && std::abs(std::abs(p.GetX()) - cx) <= kTwistlockReachM &&
                       std::abs(std::abs(p.GetZ()) - cz) <= kTwistlockReachM && above >= -0.02F &&
                       above <= kSpreaderSitM;
            }
            if (sits) {
                return body;
            }
        }
    }
    return {};
}

void YardCrane::work_spreader(const bool rigged, const bool flip) {
    const kit::BodyIndex box = spreader_seat();
    const kit::BodyIndex locked = kit_.grip_load(spreader_grip_);
    const kit::BodyIndex under = locked.valid() ? locked : box;
    // Landed: what it sits on or holds at rest, and the sling carrying little
    // of the frame (unhooked, none of it).
    const bool slack = !rigged || state_.sling_n < kSpreaderSlackShare * kSpreaderKg * 9.81F;
    state_.spreader_seated = box.valid();
    state_.spreader_landed = under.valid() && slack && kit_.body_velocity(under).Length() < 0.05F;
    if (!flip || !state_.spreader_landed) {
        return;
    }
    if (locked.valid()) {
        kit_.release(spreader_grip_);
    } else {
        (void)kit_.grip(spreader_grip_, box);
    }
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
                         const float hook_z, const float hoist, const bool device, const std::uint64_t carried) {
    state_.has_remote = has_remote();
    const bool may = able && state_.has_remote;
    if (toggle) {
        state_.operating = !state_.operating && may;
    } else if (state_.operating && !may) {
        // Hands taken, out of range: the remote's dead man lets go.
        state_.operating = false;
    }
    // The device on the sling, and its switch on the remote, worked with it.
    const bool magnet_rigged = state_.load_entity == kMagnetEntity;
    const bool spreader_rigged = state_.load_entity == kSpreaderEntity;
    if (magnet_.valid()) {
        work_magnet(magnet_rigged, device && state_.operating && magnet_rigged, carried);
    }
    if (spreader_.valid()) {
        work_spreader(spreader_rigged, device && state_.operating && spreader_rigged);
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
    // What the magnet holds, after the step: a load torn off it is let go.
    const kit::BodyIndex magnet_held = kit_.grip_load(magnet_grip_);
    const std::uint64_t magnet_now = magnet_held.valid() ? kit_.body_entity(magnet_held) : 0;
    if (state_.magnet_holding != 0 && magnet_now == 0 && state_.magnet_on) {
        state_.magnet_torn = true;
    }
    state_.magnet_holding = magnet_now;
    // What the device on the sling holds, how hard and to what.
    const kit::GripIndex grip = state_.load_entity == kMagnetEntity     ? magnet_grip_
                                : state_.load_entity == kSpreaderEntity ? spreader_grip_
                                                                        : kit::GripIndex{};
    const kit::BodyIndex held = kit_.grip_load(grip);
    state_.holding_entity = held.valid() ? kit_.body_entity(held) : 0;
    state_.hold_n = kit_.grip_force(grip);
    state_.hold_rated_n = held.valid() ? kit_.grip_rating(grip) : 0.0F;
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
    } else if (std::abs(state_.hoist_rate) > 0.02F) {
        // The drum turning, not the rope's length changing as the block
        // swings under a stopped drum.
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
    // The controls let go, but a magnet holding a load keeps it: on while it
    // holds (the kit's restore welded it again).
    state_.magnet_on = kit_.grip_load(magnet_grip_).valid();
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
