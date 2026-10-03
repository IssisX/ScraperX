#include "sim/physical_hand_climb.hpp"
namespace scraperx::sim {
PhysicalHandClimb::PhysicalHandClimb(JPH::PhysicsSystem &system, JPH::BodyID player): system_(system), player_(player) {}
PhysicalHandClimb::~PhysicalHandClimb() { clear(); }
bool PhysicalHandClimb::attach(unsigned, JPH::BodyID, JPH::RVec3) { return false; }
void PhysicalHandClimb::detach(unsigned) {}
void PhysicalHandClimb::clear() {}
bool PhysicalHandClimb::active() const noexcept { return false; }
bool PhysicalHandClimb::attached(unsigned) const noexcept { return false; }
void PhysicalHandClimb::advance_targets(JPH::Vec3, float) {}
void PhysicalHandClimb::post_step(float) {}
JPH::Vec3 PhysicalHandClimb::hand_force(unsigned) const noexcept { return JPH::Vec3::sZero(); }
}
