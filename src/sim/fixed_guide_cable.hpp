#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>

namespace scraperx::sim {

// Shared fixed-guide law for authored Kit machines and installed rigging.
// Points are world-space; length is the sum of the two moving legs, in metres.
inline JPH::Ref<JPH::PulleyConstraint> make_fixed_guide_cable(
        JPH::Body &first, JPH::RVec3 first_eye, JPH::RVec3 first_guide,
        JPH::Body &second, JPH::RVec3 second_eye, JPH::RVec3 second_guide,
        float ratio, float length_m) {
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = first_eye;
    settings.mBodyPoint2 = second_eye;
    settings.mFixedPoint1 = first_guide;
    settings.mFixedPoint2 = second_guide;
    settings.mRatio = ratio;
    settings.mMinLength = 0;
    settings.mMaxLength = length_m;
    return static_cast<JPH::PulleyConstraint *>(settings.Create(first, second));
}

} // namespace scraperx::sim
