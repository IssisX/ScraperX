#pragma once

// J-059 injury feed (#307, Claude 11-08): per-region injury from where each
// blow lands, exported to the view. J-044a §D/E is the contract:
//  - every accepted impulse is credited once, to one region or to the
//    `unclassified` bucket, so sum(regions) + unclassified == damage_ns
//    delta in each tick (no double count, no fabricated facial injury);
//  - each region keeps J_region (accumulated) and J_event (the last local
//    impact); both are monotonically nondecreasing in a fight (Cory's
//    no-heal ruling);
//  - eye stages (Cory): ring at 75, lid swells at 150, slit at 250 N·s;
//  - capability (J-044b/J-044l): one explicit conversion from regional
//    injury to limb health. The arms hang from the chest, so chest dose
//    costs them, linearly to nothing at the stage-2 line (2700 N·s, a
//    proposed calibration for Claude to rule on). Legs have no regions in
//    slice 1 and stay whole. The conversion is computed live in the sim
//    (Matron::refresh_view); the motor/budget consumers that spend it are
//    J-044c/J-044l's (their code owns the caps), so this file only converts
//    and publishes — no consumer is faked here.
//
// Geometry (measured, never guessed): the regions are the view's own
// anchors (matron_injury.gd ANCHORS), in her head frame: origin the eye
// midpoint (Matron::eyes, kEyeAboveCentreM 0.64 m over her middle, 0.12 m
// forward), +X her left, +Y up, +Z out of her face. The rig's eye sockets
// sit at head-local (±0.0315, 0.0505, 0.0847) off the head bone (1.6435 m
// over her soles, body middle 1.05 m), so in the eye frame the sockets are
// at (±0.0315, 0, ~0). A punch's contact point is on her capsule surface,
// 0.16 m in front of her skin, so the eye check is 2-D (lateral +
// vertical), the same 0.035 m radius the view uses — depth is the
// capsule's, not the hit's. Upright pose only (slice 1): when she hunts
// low her drawn head drops ~0.13 m and a face-height blow reads one band
// low (neck for face); the pose-aware correction is owed, not faked.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>

namespace scraperx::sim::injury {

// Regions the sim credits, in the view's own names (matron_injury.gd
// ANCHORS). Unclassified is the reconciliation bucket, never drawn.
enum class Region : std::uint8_t { FaceL, FaceR, Jaw, Skull, Neck, Chest, EyeL, EyeR, Unclassified };
inline constexpr std::size_t kRegions = 8;      // drawn regions (not the bucket)
inline constexpr std::size_t kRegionCount = 9;  // drawn regions + unclassified

// Bridge export names, in Region order (must match matron_injury.gd).
inline constexpr const char *kRegionNames[kRegions] = {"face_l", "face_r", "jaw", "skull",
                                                       "neck",   "chest",  "eye_l", "eye_r"};

struct Dose {
    float j_region = 0.0F;  // accumulated dose in this region, never heals
    float j_event = 0.0F;   // the last local impact here
};

struct State {
    std::array<Dose, kRegionCount> doses{};
    void credit(const Region region, const float impulse_ns) noexcept {
        Dose &d = doses[static_cast<std::size_t>(region)];
        d.j_region += impulse_ns;
        d.j_event = impulse_ns;
    }
    [[nodiscard]] float total() const noexcept {
        float sum = 0.0F;
        for (const Dose &d : doses) sum += d.j_region;
        return sum;
    }
};

// Eye stages (Cory, J-059): the first visible eye injury is the black-eye
// ring at 75 N·s; the lid swells at 150, the eye is a slit at 250.
inline constexpr float kBlackEyeNs = 75.0F;
inline constexpr float kLidSwellNs = 150.0F;
inline constexpr float kEyeSlitNs = 250.0F;
// Classification radius round a socket (same 0.035 m as matron_injury.gd).
inline constexpr float kEyeClassifyM = 0.035F;
// Socket lateral offset from the eye midpoint (rig, head-local).
inline constexpr float kEyeLateralM = 0.0315F;

// Bands in the head frame (origin the eye midpoint, metres). Head: chin
// -0.08 (jaw 0.591 m over her soles) to crown +0.10 (drawn head 1.79 m);
// neck -0.20 to -0.08 (neck_hi 0.538 m); chest -0.59 to -0.20 (chest anchor
// 0.380 m, trunk down to the waist 0.195 m). Below the trunk, above the
// crown, or a glancing top hit: unclassified.
inline constexpr float kHeadTopM = 0.10F;
inline constexpr float kHeadBottomM = -0.08F;
inline constexpr float kNeckBottomM = -0.20F;
inline constexpr float kChestBottomM = -0.59F;
// A chin hit is central: the jaw anchor sits on the midline at the chin
// (eye-frame y = kHeadBottomM), the cheeks at ±0.055 m. A midline hit
// counts as jaw only within the classification radius of the chin point,
// the same radius the eyes use: a centred forehead or nose blow is face,
// not jaw.

// Classify a judged contact point (the player's fist: a real contact
// point, measured by the strike) in her head frame: `eye_mid` is the
// sim's eye midpoint (Matron::eyes), `forward` where her head faces.
// The eye needs a front hit within the socket radius in the face plane;
// the face needs the front hemisphere; the back of the head is the skull.
[[nodiscard]] inline Region classify_contact(const JPH::Vec3 point, const JPH::Vec3 eye_mid,
                                             const JPH::Vec3 forward) noexcept {
    const JPH::Vec3 left(forward.GetZ(), 0.0F, -forward.GetX());  // her left
    const JPH::Vec3 d = point - eye_mid;
    const float x = d.Dot(left);
    const float y = d.GetY();
    const float z = d.Dot(forward);
    if (z > 0.0F && y >= kHeadBottomM && y <= kHeadTopM) {
        // Within the socket radius in the face plane: that eye. A hit
        // behind the face plane can never be the eye, however close its
        // lateral and vertical position.
        const float dxl = x - kEyeLateralM;
        const float dxr = x + kEyeLateralM;
        if (dxl * dxl + y * y <= kEyeClassifyM * kEyeClassifyM) return Region::EyeL;
        if (dxr * dxr + y * y <= kEyeClassifyM * kEyeClassifyM) return Region::EyeR;
        // Within the classification radius of the chin point: the jaw.
        // Anywhere else on the midline is face (forehead above, nose below
        // the eyes): the jaw anchor is the chin, not the whole midline.
        const float dy_chin = y - kHeadBottomM;
        if (x * x + dy_chin * dy_chin <= kEyeClassifyM * kEyeClassifyM) return Region::Jaw;
        return x >= 0.0F ? Region::FaceL : Region::FaceR;
    }
    if (y >= kHeadBottomM && y <= kHeadTopM) return Region::Skull;
    if (y >= kNeckBottomM && y < kHeadBottomM) return Region::Neck;
    if (y >= kChestBottomM && y < kNeckBottomM) return Region::Chest;
    return Region::Unclassified;
}

// An environment striker's centre is not a contact point (a long body can
// meet her far from its middle), so only the trunk band is coarse enough
// to name from it. Anything else is unclassified: no facial injury is
// ever fabricated from a body centre (J-044a §D).
[[nodiscard]] inline Region classify_striker_centre(const JPH::Vec3 centre, const JPH::Vec3 body_centre,
                                                    const JPH::Vec3 /*forward*/) noexcept {
    // Chest band in middle-relative height: the trunk, 0.05 m (waist) to
    // 0.45 m (collar) over her middle. Same band as classify_contact's
    // chest, shifted by the eye height (0.64 m over her middle).
    const float y = centre.GetY() - body_centre.GetY();
    if (y >= 0.05F && y < 0.45F) return Region::Chest;
    return Region::Unclassified;
}

// Capability (J-044b/J-044l): the single explicit conversion. Chest dose
// costs the arms linearly to nothing at kArmLossNs; legs have no regions
// in slice 1 and stay whole. Order: arm_l, arm_r, leg_l, leg_r (the
// capacity test's kLimbs order: two arms, two legs).
inline constexpr float kArmLossNs = 2700.0F;
[[nodiscard]] inline std::array<float, 4> limb_health(const State &state) noexcept {
    const float chest = state.doses[static_cast<std::size_t>(Region::Chest)].j_region;
    float arm = 1.0F - chest / kArmLossNs;
    if (arm < 0.0F) arm = 0.0F;
    if (arm > 1.0F) arm = 1.0F;
    return {arm, arm, 1.0F, 1.0F};
}

}  // namespace scraperx::sim::injury
