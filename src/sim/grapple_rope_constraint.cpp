#include "sim/grapple_rope_constraint.hpp"

namespace scraperx::sim {
JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Winvalid-offsetof")
JPH_GCC_SUPPRESS_WARNING("-Winvalid-offsetof")
JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(GrappleRopeSettings) {
    JPH_ADD_BASE_CLASS(GrappleRopeSettings, JPH::TwoBodyConstraintSettings)
    JPH_ADD_ATTRIBUTE(GrappleRopeSettings, mLocalEye)
    JPH_ADD_ATTRIBUTE(GrappleRopeSettings, mRestLength)
}
JPH_SUPPRESS_WARNING_POP

JPH::TwoBodyConstraint *GrappleRopeSettings::Create(JPH::Body &player, JPH::Body &hook) const {
    return new GrappleRope(player,hook,*this);
}

} // namespace scraperx::sim
