#pragma once
#ifdef SCRAPERX_HAS_JOLT
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <array>
#include <memory>
namespace scraperx::sim {
// Passive, frictionless, massless cable with one shared paid length. Owner thread
// creates/destroys outside Update, calls pre_step/post_step there, and calls
// collision_step in the serialized PhysicsStepListener before island building.
// Bodies must be static or dynamic (no prescribed-motion energy source), and
// must outlive this owner. Restore bodies only after destroying
// the owner, then reconstruct cold. No warm start, position repair, or motor.
class RoutedCable final {
public:
 static constexpr unsigned max_nodes = 8, max_bodies = 9, max_line_nodes = 72;
 struct Node { JPH::BodyID body; JPH::Vec3 point = JPH::Vec3::sZero(); }; // shape-origin local
 struct SphereObstacle {
  JPH::BodyID body;
  JPH::Vec3 center = JPH::Vec3::sZero(); // shape-origin local
  float radius_m = 0;
 };
 struct Config {
  std::array<Node,max_nodes> nodes{};
  unsigned node_count = 0;
  float paid_length_m = 1, rating_n = 50000;
  SphereObstacle sphere{}; // invalid BodyID disables; actual SphereShape required
 };
 struct Reaction {
  JPH::BodyID body;
  JPH::Vec3 force_n = JPH::Vec3::sZero(), torque_nm = JPH::Vec3::sZero();
 };
 struct State {
  float path_length_m = 0, paid_length_m = 0, length_error_m = 0;
  float tension_n = 0, peak_tension_n = 0, impulse_ns = 0, collision_dt = 0;
  double kinetic_work_j = 0, positive_event_work_j = 0, dissipated_j = 0;
  std::array<JPH::RVec3,max_line_nodes> line_nodes{};
  unsigned line_node_count = 0, contact_count = 0, reaction_count = 0;
  std::array<Reaction,max_bodies> reactions{};
  // Unsupported geometry is rejected, never silently replaced by a world pin.
  bool invalid_geometry = false;
 };
 RoutedCable(JPH::PhysicsSystem &, const Config &);
 ~RoutedCable();
 RoutedCable(const RoutedCable &) = delete;
 RoutedCable &operator=(const RoutedCable &) = delete;
 void pre_step();
 void collision_step(float dt);
 void post_step();
 [[nodiscard]] State state() const;
private:
 struct Impl;
 std::unique_ptr<Impl> impl_;
};
}
#endif
